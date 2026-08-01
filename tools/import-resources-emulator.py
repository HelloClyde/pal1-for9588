from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys


REQUIRED = {
    "abc.mkf", "ball.mkf", "data.mkf", "f.mkf", "fbp.mkf",
    "fire.mkf", "gop.mkf", "map.mkf", "mgo.mkf", "mus.mkf",
    "pat.mkf", "rgm.mkf", "rng.mkf", "sss.mkf", "word.dat",
}
OPTIONAL = {
    "m.msg", "voc.mkf", "sounds.mkf", "midi.mkf", "desc.dat",
    "wor16.asc", "wor16.fon",
}
VIDEO = {f"{number}.avi" for number in range(1, 7)}
TARGET = "/应用/数据/PAL"


def digest(path: Path) -> str:
    result = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(block)
    return result.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Import legally owned PAL data into the isolated emulator NAND."
    )
    parser.add_argument("source", type=Path)
    parser.add_argument("--emulator-root", type=Path, required=True)
    parser.add_argument("--nand", type=Path, required=True)
    parser.add_argument("--target", default=TARGET)
    parser.add_argument(
        "--video-source", type=Path,
        help="optional directory containing compact 1.avi through 6.avi",
    )
    args = parser.parse_args()

    source = args.source.resolve()
    emulator_root = args.emulator_root.resolve()
    nand = args.nand.resolve()
    if not source.is_dir():
        raise SystemExit(f"Resource directory does not exist: {source}")
    if not nand.is_file():
        raise SystemExit(f"Test NAND does not exist: {nand}")

    files = {
        item.name.lower(): item
        for item in source.iterdir()
        if item.is_file()
    }
    missing = sorted(REQUIRED - files.keys())
    if missing:
        raise SystemExit("Missing required files: " + ", ".join(missing))
    if "voc.mkf" not in files and "sounds.mkf" not in files:
        raise SystemExit("Missing sound effects: voc.mkf or sounds.mkf is required")

    selected = {
        name: files[name]
        for name in sorted((REQUIRED | OPTIONAL | VIDEO) & files.keys())
    }
    if args.video_source is not None:
        video_source = args.video_source.resolve()
        if not video_source.is_dir():
            raise SystemExit(f"Video directory does not exist: {video_source}")
        videos = {
            item.name.lower(): item
            for item in video_source.iterdir()
            if item.is_file() and item.name.lower() in VIDEO
        }
        missing_video = sorted(VIDEO - videos.keys())
        if missing_video:
            raise SystemExit(
                "Missing transcoded videos: " + ", ".join(missing_video)
            )
        selected.update(videos)
    expected = {
        name: {"size": path.stat().st_size, "sha256": digest(path)}
        for name, path in selected.items()
    }

    sys.path.insert(0, str(emulator_root))
    from emu.qemu.nand_fs import mutate_nand_files, replace_fat_file

    target = "/" + args.target.strip("/")

    def operation(fs):
        fs.makedirs(target, recreate=True)
        for name, path in selected.items():
            with path.open("rb") as stream:
                replace_fat_file(fs, f"{target}/{name}", stream)

    def validator(fs):
        for name, metadata in expected.items():
            destination = f"{target}/{name}"
            if not fs.isfile(destination):
                raise ValueError(f"Imported file is missing: {destination}")
            if fs.getsize(destination) != metadata["size"]:
                raise ValueError(f"Imported file size mismatch: {destination}")
            imported = hashlib.sha256()
            with fs.openbin(destination, "r") as stream:
                for block in iter(lambda: stream.read(1024 * 1024), b""):
                    imported.update(block)
            if imported.hexdigest() != metadata["sha256"]:
                raise ValueError(f"Imported file checksum mismatch: {destination}")

    mutate_nand_files(nand, operation, validator=validator)
    print(json.dumps({
        "ok": True,
        "source": str(source),
        "nand": str(nand),
        "target": target,
        "files": expected,
    }, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
