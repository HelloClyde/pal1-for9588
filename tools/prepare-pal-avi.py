from __future__ import annotations

import argparse
import json
from pathlib import Path
import struct


AVIF_MUSTUSEINDEX = 0x20
MAX_SUGGESTED_BUFFER = 64 * 1024
STREAM_CHUNKS = {b"00db", b"00dc", b"01wb"}


def find_chunk(data: bytearray, tag: bytes, limit: int) -> int:
    offset = 0
    while True:
        offset = data.find(tag, offset, limit)
        if offset < 0:
            return -1
        if offset + 8 <= limit:
            length = struct.unpack_from("<I", data, offset + 4)[0]
            if offset + 8 + length <= len(data):
                return offset
        offset += 1


def normalize_stream_chunks(data: bytearray, start: int, end: int) -> int:
    normalized = 0
    offset = start
    while offset + 8 <= end:
        tag = bytes(data[offset:offset + 4])
        length = struct.unpack_from("<I", data, offset + 4)[0]
        payload = offset + 8
        physical_end = payload + length + (length & 1)
        if physical_end > end:
            raise ValueError("movi contains a truncated RIFF chunk")
        if tag == b"LIST":
            if length < 4:
                raise ValueError("movi contains an invalid LIST chunk")
            normalized += normalize_stream_chunks(
                data, payload + 4, payload + length
            )
        elif tag in STREAM_CHUNKS and length & 1:
            # SDLPAL's PAL95 sequential reader does not skip the standard
            # one-byte RIFF padding.  Count that existing byte as payload so
            # the next chunk header remains aligned without growing the file.
            data[payload + length] = 0x80 if tag == b"01wb" else 0
            struct.pack_into("<I", data, offset + 4, length + 1)
            normalized += 1
        offset = physical_end
    if offset != end:
        raise ValueError("movi RIFF chunks do not end on the LIST boundary")
    return normalized


def normalize_index_entries(data: bytearray, start: int) -> int:
    if start < 0:
        return 0
    length = struct.unpack_from("<I", data, start + 4)[0]
    payload = start + 8
    end = payload + length
    if end > len(data) or length % 16:
        raise ValueError("AVI idx1 chunk is malformed")
    normalized = 0
    for offset in range(payload, end, 16):
        if bytes(data[offset:offset + 4]) in STREAM_CHUNKS:
            size = struct.unpack_from("<I", data, offset + 12)[0]
            if size & 1:
                struct.pack_into("<I", data, offset + 12, size + 1)
                normalized += 1
    return normalized


def prepare(path: Path) -> dict[str, object]:
    data = bytearray(path.read_bytes())
    if len(data) < 256 or data[:4] != b"RIFF" or data[8:12] != b"AVI ":
        raise ValueError("not a RIFF AVI file")

    movi = data.find(b"movi", 12)
    if movi < 0:
        raise ValueError("AVI has no movi list")
    movi_list = movi - 8
    movi_end = movi_list + 8 + struct.unpack_from("<I", data, movi - 4)[0]
    if movi_list < 0 or movi_end > len(data):
        raise ValueError("AVI has an invalid movi list")
    avih = find_chunk(data, b"avih", movi)
    if avih < 0 or struct.unpack_from("<I", data, avih + 4)[0] < 40:
        raise ValueError("AVI has no usable main header")

    flags = struct.unpack_from("<I", data, avih + 8 + 12)[0]
    if flags & AVIF_MUSTUSEINDEX:
        raise ValueError("AVI requires index-based playback")
    width, height = struct.unpack_from("<II", data, avih + 8 + 32)
    if width == 0 or height == 0 or width % 4 or height % 4:
        raise ValueError("video dimensions must be non-zero multiples of four")

    suggested_offset = avih + 8 + 28
    original_suggested = struct.unpack_from("<I", data, suggested_offset)[0]
    if original_suggested > MAX_SUGGESTED_BUFFER:
        struct.pack_into("<I", data, suggested_offset, MAX_SUGGESTED_BUFFER)

    video_handlers: list[str] = []
    audio_handlers: list[int] = []
    offset = 0
    while True:
        offset = data.find(b"strh", offset, movi)
        if offset < 0:
            break
        length = struct.unpack_from("<I", data, offset + 4)[0]
        if length >= 8 and offset + 16 <= movi:
            stream_type = bytes(data[offset + 8:offset + 12])
            handler_offset = offset + 12
            handler = bytes(data[handler_offset:handler_offset + 4])
            if stream_type == b"vids":
                video_handlers.append(handler.decode("ascii", errors="replace"))
            elif stream_type == b"auds":
                audio_handlers.append(struct.unpack_from("<I", handler)[0])
                # SDLPAL's PAL95 AVI reader expects the original files' zero
                # audio stream handler; PCM format is still carried by strf.
                data[handler_offset:handler_offset + 4] = b"\0\0\0\0"
        offset += 4

    if len(video_handlers) != 1 or video_handlers[0] not in {"MSVC", "msvc"}:
        raise ValueError(f"expected one MS Video 1 stream, got {video_handlers}")
    if len(audio_handlers) != 1:
        raise ValueError(f"expected one PCM audio stream, got {len(audio_handlers)}")

    normalized_stream_chunks = normalize_stream_chunks(
        data, movi + 4, movi_end
    )
    idx1 = data.find(b"idx1", movi_end)
    normalized_index_entries = normalize_index_entries(data, idx1)

    path.write_bytes(data)
    return {
        "path": str(path),
        "size": len(data),
        "width": width,
        "height": height,
        "video_handler": video_handlers[0],
        "audio_handler_before": audio_handlers[0],
        "audio_handler_after": 0,
        "suggested_buffer_before": original_suggested,
        "suggested_buffer_after": min(original_suggested, MAX_SUGGESTED_BUFFER),
        "odd_stream_chunks_normalized": normalized_stream_chunks,
        "odd_index_entries_normalized": normalized_index_entries,
    }


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Normalize a transcoded AVI for SDLPAL's small PAL95 reader."
    )
    parser.add_argument("path", type=Path)
    args = parser.parse_args()
    path = args.path.resolve()
    if not path.is_file():
        raise SystemExit(f"AVI does not exist: {path}")
    try:
        result = prepare(path)
    except ValueError as error:
        raise SystemExit(f"AVI validation failed: {error}") from error
    print(json.dumps(result, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
