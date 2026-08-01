from __future__ import annotations

import argparse
from dataclasses import dataclass
import json
from pathlib import Path
import shutil
import struct
import tempfile
import zlib


MAGIC = b"PAL9588\0"
VERSION = 1
HEADER = struct.Struct("<8s6I")
ENTRY = struct.Struct("<48s4I")
ALIGNMENT = 16
MAX_ENTRIES = 256
MAX_ARCHIVE_SIZE = 0x7FFFFFFF


class PalPakError(RuntimeError):
    pass


@dataclass(frozen=True)
class EntryInfo:
    name: str
    offset: int
    size: int
    crc32: int
    flags: int = 0


def _align(value: int) -> int:
    return (value + ALIGNMENT - 1) & ~(ALIGNMENT - 1)


def _normalized_name(path: Path) -> tuple[str, bytes]:
    name = path.name.lower()
    if not name or name in {".", ".."} or "/" in name or "\\" in name:
        raise PalPakError(f"Invalid archive member name: {path.name!r}")
    try:
        encoded = name.encode("ascii")
    except UnicodeEncodeError as exc:
        raise PalPakError(
            f"Archive member names must be ASCII: {path.name!r}"
        ) from exc
    if len(encoded) > 47:
        raise PalPakError(f"Archive member name is too long: {path.name!r}")
    return name, encoded


def _crc32(path: Path) -> int:
    value = 0
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            value = zlib.crc32(block, value)
    return value & 0xFFFFFFFF


def pack_directory(source: Path, output: Path) -> list[EntryInfo]:
    source = source.resolve()
    output = output.resolve()
    if not source.is_dir():
        raise PalPakError(f"Source directory does not exist: {source}")

    members: list[tuple[str, bytes, Path, int, int]] = []
    seen: set[str] = set()
    for path in sorted(source.iterdir(), key=lambda item: item.name.lower()):
        if not path.is_file():
            continue
        name, encoded = _normalized_name(path)
        if name in seen:
            raise PalPakError(f"Duplicate case-insensitive member name: {name}")
        seen.add(name)
        size = path.stat().st_size
        if size > MAX_ARCHIVE_SIZE:
            raise PalPakError(f"Archive member is too large: {path}")
        members.append((name, encoded, path, size, _crc32(path)))

    if not members:
        raise PalPakError(f"Source directory contains no files: {source}")
    if len(members) > MAX_ENTRIES:
        raise PalPakError(
            f"Too many archive members: {len(members)} > {MAX_ENTRIES}"
        )

    directory_offset = HEADER.size
    data_offset = _align(directory_offset + len(members) * ENTRY.size)
    cursor = data_offset
    entries: list[EntryInfo] = []
    for name, _encoded, _path, size, crc32 in members:
        entries.append(EntryInfo(name, cursor, size, crc32))
        cursor = _align(cursor + size)
    archive_size = cursor
    if archive_size > MAX_ARCHIVE_SIZE:
        raise PalPakError(
            f"Archive exceeds the 9588 signed-seek limit: {archive_size} bytes"
        )

    output.parent.mkdir(parents=True, exist_ok=True)
    temporary: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="w+b", prefix=output.name + ".", suffix=".tmp",
            dir=output.parent, delete=False
        ) as stream:
            temporary = Path(stream.name)
            stream.write(HEADER.pack(
                MAGIC, VERSION, len(entries), directory_offset, ENTRY.size,
                data_offset, archive_size
            ))
            for entry in entries:
                encoded = entry.name.encode("ascii")
                stream.write(ENTRY.pack(
                    encoded.ljust(48, b"\0"), entry.offset, entry.size,
                    entry.crc32, entry.flags
                ))
            stream.write(b"\0" * (data_offset - stream.tell()))

            for entry, member in zip(entries, members):
                path = member[2]
                if stream.tell() != entry.offset:
                    raise PalPakError("Internal archive alignment error")
                with path.open("rb") as source_stream:
                    shutil.copyfileobj(source_stream, stream, 1024 * 1024)
                aligned = _align(stream.tell())
                stream.write(b"\0" * (aligned - stream.tell()))

            if stream.tell() != archive_size:
                raise PalPakError("Internal archive size error")
        temporary.replace(output)
    finally:
        if temporary is not None and temporary.exists():
            temporary.unlink()
    return entries


def read_directory(archive: Path) -> list[EntryInfo]:
    archive = archive.resolve()
    try:
        physical_size = archive.stat().st_size
    except FileNotFoundError as exc:
        raise PalPakError(f"Archive does not exist: {archive}") from exc
    if physical_size > MAX_ARCHIVE_SIZE:
        raise PalPakError(f"Archive exceeds the 9588 size limit: {archive}")

    with archive.open("rb") as stream:
        raw_header = stream.read(HEADER.size)
        if len(raw_header) != HEADER.size:
            raise PalPakError("Archive header is truncated")
        magic, version, count, directory_offset, entry_size, data_offset, archive_size = (
            HEADER.unpack(raw_header)
        )
        if magic != MAGIC:
            raise PalPakError("Archive magic is invalid")
        if version != VERSION:
            raise PalPakError(f"Unsupported archive version: {version}")
        if count == 0 or count > MAX_ENTRIES:
            raise PalPakError(f"Invalid archive member count: {count}")
        if directory_offset != HEADER.size or entry_size != ENTRY.size:
            raise PalPakError("Archive directory layout is invalid")
        minimum_data_offset = _align(directory_offset + count * entry_size)
        if data_offset != minimum_data_offset:
            raise PalPakError("Archive data offset is invalid")
        if archive_size != physical_size:
            raise PalPakError(
                f"Archive size mismatch: header={archive_size}, file={physical_size}"
            )

        stream.seek(directory_offset)
        entries: list[EntryInfo] = []
        seen: set[str] = set()
        for _ in range(count):
            raw_entry = stream.read(ENTRY.size)
            if len(raw_entry) != ENTRY.size:
                raise PalPakError("Archive directory is truncated")
            raw_name, offset, size, crc32, flags = ENTRY.unpack(raw_entry)
            name_bytes = raw_name.split(b"\0", 1)[0]
            try:
                name = name_bytes.decode("ascii")
            except UnicodeDecodeError as exc:
                raise PalPakError("Archive member name is not ASCII") from exc
            if not name or name != name.lower() or name in seen:
                raise PalPakError(f"Invalid or duplicate archive member: {name!r}")
            if "/" in name or "\\" in name or flags != 0:
                raise PalPakError(f"Unsupported archive member: {name!r}")
            if offset < data_offset or offset % ALIGNMENT != 0:
                raise PalPakError(f"Invalid member offset: {name}")
            if size > archive_size or offset > archive_size - size:
                raise PalPakError(f"Member exceeds archive bounds: {name}")
            seen.add(name)
            entries.append(EntryInfo(name, offset, size, crc32, flags))
        return entries


def verify_archive(archive: Path) -> list[EntryInfo]:
    entries = read_directory(archive)
    with archive.open("rb") as stream:
        for entry in entries:
            stream.seek(entry.offset)
            remaining = entry.size
            crc32 = 0
            while remaining:
                block = stream.read(min(1024 * 1024, remaining))
                if not block:
                    raise PalPakError(f"Archive member is truncated: {entry.name}")
                crc32 = zlib.crc32(block, crc32)
                remaining -= len(block)
            if (crc32 & 0xFFFFFFFF) != entry.crc32:
                raise PalPakError(f"CRC32 mismatch: {entry.name}")
    return entries


def extract_archive(archive: Path, destination: Path) -> list[EntryInfo]:
    entries = verify_archive(archive)
    destination = destination.resolve()
    destination.mkdir(parents=True, exist_ok=True)
    with archive.open("rb") as stream:
        for entry in entries:
            stream.seek(entry.offset)
            output = destination / entry.name
            with output.open("wb") as target:
                remaining = entry.size
                while remaining:
                    block = stream.read(min(1024 * 1024, remaining))
                    if not block:
                        raise PalPakError(f"Archive member is truncated: {entry.name}")
                    target.write(block)
                    remaining -= len(block)
    return entries


def _report(archive: Path, entries: list[EntryInfo]) -> None:
    print(json.dumps({
        "ok": True,
        "archive": str(archive.resolve()),
        "size": archive.resolve().stat().st_size,
        "entries": [
            {
                "name": item.name,
                "offset": item.offset,
                "size": item.size,
                "crc32": f"{item.crc32:08x}",
            }
            for item in entries
        ],
    }, ensure_ascii=False, indent=2))


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Create and inspect the random-access PAL9588.PAK format."
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    pack_parser = subparsers.add_parser("pack")
    pack_parser.add_argument("source", type=Path)
    pack_parser.add_argument("archive", type=Path)

    extract_parser = subparsers.add_parser("extract")
    extract_parser.add_argument("archive", type=Path)
    extract_parser.add_argument("destination", type=Path)

    for command in ("list", "verify"):
        command_parser = subparsers.add_parser(command)
        command_parser.add_argument("archive", type=Path)

    args = parser.parse_args()
    try:
        if args.command == "pack":
            entries = pack_directory(args.source, args.archive)
            _report(args.archive, entries)
        elif args.command == "extract":
            entries = extract_archive(args.archive, args.destination)
            _report(args.archive, entries)
        elif args.command == "verify":
            entries = verify_archive(args.archive)
            _report(args.archive, entries)
        else:
            entries = read_directory(args.archive)
            _report(args.archive, entries)
    except (OSError, PalPakError) as exc:
        parser.error(str(exc))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
