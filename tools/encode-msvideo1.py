from __future__ import annotations

import argparse
from array import array
from dataclasses import dataclass
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
from typing import BinaryIO, Iterable


RGB555_RED = bytes((value >> 10) & 0x1F for value in range(0x8000))
RGB555_GREEN = bytes((value >> 5) & 0x1F for value in range(0x8000))
RGB555_BLUE = bytes(value & 0x1F for value in range(0x8000))


@dataclass
class EncodeStats:
    frames: int = 0
    source_bytes: int = 0
    video_bytes: int = 0
    skip_blocks: int = 0
    solid_blocks: int = 0
    two_color_blocks: int = 0
    eight_color_blocks: int = 0
    largest_frame: int = 0

    def report(self) -> dict[str, int | float]:
        total_blocks = (
            self.skip_blocks
            + self.solid_blocks
            + self.two_color_blocks
            + self.eight_color_blocks
        )
        return {
            "frames": self.frames,
            "source_bytes": self.source_bytes,
            "video_bytes": self.video_bytes,
            "largest_frame": self.largest_frame,
            "total_blocks": total_blocks,
            "skip_blocks": self.skip_blocks,
            "solid_blocks": self.solid_blocks,
            "two_color_blocks": self.two_color_blocks,
            "eight_color_blocks": self.eight_color_blocks,
            "skip_percent": round(
                self.skip_blocks * 100.0 / max(1, total_blocks), 3
            ),
        }


def rgb555(red: int, green: int, blue: int) -> int:
    return ((red & 0x1F) << 10) | ((green & 0x1F) << 5) | (blue & 0x1F)


def color_distance(left: int, right: int) -> int:
    red = RGB555_RED[left] - RGB555_RED[right]
    green = RGB555_GREEN[left] - RGB555_GREEN[right]
    blue = RGB555_BLUE[left] - RGB555_BLUE[right]
    return red * red * 2 + green * green * 4 + blue * blue


def block_error(source: list[int], decoded: list[int]) -> int:
    return sum(color_distance(a, b) for a, b in zip(source, decoded)) // 16


def average_color(colors: Iterable[int]) -> int:
    values = list(colors)
    count = len(values)
    return rgb555(
        (sum(RGB555_RED[value] for value in values) + count // 2) // count,
        (sum(RGB555_GREEN[value] for value in values) + count // 2) // count,
        (sum(RGB555_BLUE[value] for value in values) + count // 2) // count,
    )


def nearest_color(value: int, first: int, second: int) -> int:
    return int(color_distance(value, second) < color_distance(value, first))


def fit_two_colors(colors: list[int], iterations: int = 2) -> tuple[list[int], list[int]]:
    darkest = min(colors, key=lambda value: (
        RGB555_RED[value] * 2 + RGB555_GREEN[value] * 4 + RGB555_BLUE[value]
    ))
    lightest = max(colors, key=lambda value: (
        RGB555_RED[value] * 2 + RGB555_GREEN[value] * 4 + RGB555_BLUE[value]
    ))
    if darkest == lightest:
        return [darkest, darkest], [0] * len(colors)

    palette = [darkest, lightest]
    assignments = [0] * len(colors)
    for _ in range(iterations):
        assignments = [
            nearest_color(value, palette[0], palette[1]) for value in colors
        ]
        if all(index == 0 for index in assignments) or all(
            index == 1 for index in assignments
        ):
            break
        palette = [
            average_color(
                value for value, index in zip(colors, assignments) if index == 0
            ),
            average_color(
                value for value, index in zip(colors, assignments) if index == 1
            ),
        ]
    assignments = [
        nearest_color(value, palette[0], palette[1]) for value in colors
    ]
    return palette, assignments


def encode_skip_run(output: bytearray, count: int) -> None:
    while count:
        current = min(count, 0x3FF)
        output.extend((current & 0xFF, 0x84 + (current >> 8)))
        count -= current


def encode_solid(output: bytearray, color: int) -> None:
    encoded = (color & 0x7FFF) | 0x8000
    if ((encoded >> 8) & 0xFC) == 0x84:
        raise ValueError("solid color collides with an MS Video 1 skip code")
    output.extend(struct.pack("<H", encoded))


def solid_uses_skip_code(color: int) -> bool:
    encoded = (color & 0x7FFF) | 0x8000
    return ((encoded >> 8) & 0xFC) == 0x84


def flags_for_assignments(assignments: list[int]) -> int:
    flags = 0
    for bit, assignment in enumerate(assignments):
        flags |= (assignment ^ 1) << bit
    return flags


def encode_two_color(
    output: bytearray,
    palette: list[int],
    assignments: list[int],
) -> list[int]:
    flags = flags_for_assignments(assignments)
    if flags & 0x8000:
        palette = [palette[1], palette[0]]
        assignments = [index ^ 1 for index in assignments]
        flags ^= 0xFFFF
    output.extend(struct.pack(
        "<HHH", flags, palette[0] & 0x7FFF, palette[1] & 0x7FFF
    ))
    return [palette[index] & 0x7FFF for index in assignments]


def encode_eight_color(output: bytearray, source: list[int]) -> list[int]:
    palette = [0] * 8
    assignments = [0] * 16
    for row_base in (0, 2):
        for column_base in (0, 2):
            positions = [
                (row_base + row) * 4 + column_base + column
                for row in range(2)
                for column in range(2)
            ]
            colors = [source[position] for position in positions]
            pair, pair_assignments = fit_two_colors(colors, iterations=1)
            base = (row_base << 1) + column_base
            palette[base] = pair[0] & 0x7FFF
            palette[base + 1] = pair[1] & 0x7FFF
            for position, assignment in zip(positions, pair_assignments):
                assignments[position] = assignment

    flags = flags_for_assignments(assignments)
    if flags & 0x8000:
        palette[6], palette[7] = palette[7], palette[6]
        for row in range(2, 4):
            for column in range(2, 4):
                position = row * 4 + column
                assignments[position] ^= 1
        flags = flags_for_assignments(assignments)
    if flags & 0x8000:
        raise RuntimeError("MS Video 1 flag normalization failed")

    output.extend(struct.pack("<H", flags))
    palette[0] |= 0x8000
    output.extend(struct.pack("<8H", *palette))
    return [
        palette[((row & 2) << 1) + (column & 2) + assignments[row * 4 + column]]
        & 0x7FFF
        for row in range(4)
        for column in range(4)
    ]


def read_block(frame: array[int], width: int, block_x: int, block_y: int) -> list[int]:
    result: list[int] = []
    left = block_x * 4
    bottom = block_y * 4 + 3
    for row in range(4):
        offset = (bottom - row) * width + left
        result.extend((frame[offset + column] & 0x7FFF) for column in range(4))
    return result


def write_block(
    frame: array[int],
    width: int,
    block_x: int,
    block_y: int,
    pixels: list[int],
) -> None:
    left = block_x * 4
    bottom = block_y * 4 + 3
    position = 0
    for row in range(4):
        offset = (bottom - row) * width + left
        for column in range(4):
            frame[offset + column] = pixels[position] & 0x7FFF
            position += 1


def encode_frame(
    source: array[int],
    reconstructed: array[int] | None,
    width: int,
    height: int,
    quality_error: int,
    skip_error: int,
    stats: EncodeStats,
) -> tuple[bytes, array[int]]:
    current = array("H", reconstructed) if reconstructed is not None else array(
        "H", [0] * (width * height)
    )
    output = bytearray()
    pending_skip = 0

    for block_y in range(height // 4 - 1, -1, -1):
        for block_x in range(width // 4):
            target = read_block(source, width, block_x, block_y)
            if reconstructed is not None:
                previous = read_block(current, width, block_x, block_y)
                if block_error(target, previous) <= skip_error:
                    pending_skip += 1
                    stats.skip_blocks += 1
                    continue

            if pending_skip:
                encode_skip_run(output, pending_skip)
                pending_skip = 0

            solid = average_color(target)
            solid_pixels = [solid] * 16
            if (
                block_error(target, solid_pixels) <= quality_error
                and not solid_uses_skip_code(solid)
            ):
                encode_solid(output, solid)
                write_block(current, width, block_x, block_y, solid_pixels)
                stats.solid_blocks += 1
                continue

            palette, assignments = fit_two_colors(target)
            pair_pixels = [palette[index] for index in assignments]
            if block_error(target, pair_pixels) <= quality_error:
                decoded = encode_two_color(output, palette, assignments)
                write_block(current, width, block_x, block_y, decoded)
                stats.two_color_blocks += 1
                continue

            decoded = encode_eight_color(output, target)
            write_block(current, width, block_x, block_y, decoded)
            stats.eight_color_blocks += 1

    if pending_skip:
        encode_skip_run(output, pending_skip)
    return bytes(output), current


def read_exact(stream: BinaryIO, size: int) -> bytes:
    result = bytearray()
    while len(result) < size:
        block = stream.read(size - len(result))
        if not block:
            break
        result.extend(block)
    return bytes(result)


def decode_video_frames(
    ffmpeg: str,
    source: Path,
    width: int,
    height: int,
    fps: int,
    quality_error: int,
    skip_error: int,
    stats: EncodeStats,
) -> list[bytes]:
    command = [
        ffmpeg,
        "-hide_banner",
        "-loglevel",
        "error",
        "-i",
        str(source),
        "-map",
        "0:v:0",
        "-vf",
        f"scale={width}:{height}:flags=bilinear,fps={fps}",
        "-f",
        "rawvideo",
        "-pix_fmt",
        "rgb555le",
        "pipe:1",
    ]
    process = subprocess.Popen(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if process.stdout is None or process.stderr is None:
        raise RuntimeError("failed to open FFmpeg pipes")

    frame_size = width * height * 2
    packets: list[bytes] = []
    reconstructed: array[int] | None = None
    while True:
        raw = read_exact(process.stdout, frame_size)
        if not raw:
            break
        if len(raw) != frame_size:
            process.kill()
            raise RuntimeError("FFmpeg produced a truncated RGB555 frame")
        frame = array("H")
        frame.frombytes(raw)
        if sys.byteorder != "little":
            frame.byteswap()
        packet, reconstructed = encode_frame(
            frame,
            reconstructed,
            width,
            height,
            quality_error,
            skip_error,
            stats,
        )
        packets.append(packet)
        stats.frames += 1
        stats.source_bytes += frame_size
        stats.video_bytes += len(packet)
        stats.largest_frame = max(stats.largest_frame, len(packet))
        if stats.frames % 150 == 0:
            print(
                f"{source.name}: encoded {stats.frames} frames "
                f"({stats.video_bytes / 1048576:.2f} MiB video)",
                file=sys.stderr,
                flush=True,
            )

    stderr = process.stderr.read().decode("utf-8", errors="replace")
    return_code = process.wait()
    if return_code != 0:
        raise RuntimeError(f"FFmpeg video decode failed ({return_code}): {stderr}")
    if not packets:
        raise RuntimeError("source AVI contains no video frames")
    return packets


def decode_audio(
    ffmpeg: str,
    source: Path,
    audio_rate: int,
) -> bytes:
    command = [
        ffmpeg,
        "-hide_banner",
        "-loglevel",
        "error",
        "-i",
        str(source),
        "-map",
        "0:a:0",
        "-f",
        "u8",
        "-acodec",
        "pcm_u8",
        "-ar",
        str(audio_rate),
        "-ac",
        "1",
        "pipe:1",
    ]
    result = subprocess.run(command, capture_output=True, check=False)
    if result.returncode != 0:
        error = result.stderr.decode("utf-8", errors="replace")
        raise RuntimeError(f"FFmpeg audio decode failed ({result.returncode}): {error}")
    if not result.stdout:
        raise RuntimeError("source AVI contains no audio samples")
    return result.stdout


def riff_chunk(tag: bytes, payload: bytes) -> bytes:
    if len(tag) != 4:
        raise ValueError("RIFF chunk tag must contain four bytes")
    if len(payload) & 1:
        raise ValueError(f"RIFF chunk {tag!r} has an odd payload")
    return tag + struct.pack("<I", len(payload)) + payload


def riff_list(tag: bytes, payload: bytes) -> bytes:
    return b"LIST" + struct.pack("<I", len(payload) + 4) + tag + payload


def build_headers(
    width: int,
    height: int,
    fps: int,
    frame_count: int,
    audio_rate: int,
    audio_length: int,
    suggested_buffer: int,
    max_bytes_per_second: int,
) -> bytes:
    microseconds = (1_000_000 + fps // 2) // fps
    avih = struct.pack(
        "<14I",
        microseconds,
        max_bytes_per_second,
        2,
        0x100,
        frame_count,
        0,
        2,
        suggested_buffer,
        width,
        height,
        0,
        0,
        0,
        0,
    )
    video_stream = struct.pack(
        "<4s4sIHH8I4H",
        b"vids",
        b"MSVC",
        0,
        0,
        0,
        0,
        1,
        fps,
        0,
        frame_count,
        suggested_buffer,
        0xFFFFFFFF,
        0,
        0,
        0,
        width,
        height,
    )
    bitmap_info = struct.pack(
        "<IIIHHIIIIII",
        40,
        width,
        height,
        1,
        16,
        struct.unpack("<I", b"MSVC")[0],
        width * height * 2,
        0,
        0,
        0,
        0,
    )
    audio_stream = struct.pack(
        "<4s4sIHH8I4H",
        b"auds",
        b"\0\0\0\0",
        0,
        0,
        0,
        0,
        1,
        audio_rate,
        0,
        audio_length,
        4096,
        0xFFFFFFFF,
        1,
        0,
        0,
        0,
        0,
    )
    wave_format = struct.pack(
        "<HHIIHHH", 1, 1, audio_rate, audio_rate, 1, 8, 0
    )
    video_list = riff_list(
        b"strl",
        riff_chunk(b"strh", video_stream) + riff_chunk(b"strf", bitmap_info),
    )
    audio_list = riff_list(
        b"strl",
        riff_chunk(b"strh", audio_stream) + riff_chunk(b"strf", wave_format),
    )
    return riff_list(b"hdrl", riff_chunk(b"avih", avih) + video_list + audio_list)


def write_even_stream_chunk(stream: BinaryIO, tag: bytes, payload: bytes) -> int:
    if len(payload) & 1:
        payload += b"\x80" if tag == b"01wb" else b"\0"
    stream.write(tag)
    stream.write(struct.pack("<I", len(payload)))
    stream.write(payload)
    return 8 + len(payload)


def write_avi(
    destination: Path,
    packets: list[bytes],
    audio: bytes,
    width: int,
    height: int,
    fps: int,
    audio_rate: int,
) -> None:
    suggested_buffer = max(max(map(len, packets)), 4096)
    video_bytes = sum(map(len, packets))
    duration_seconds = len(packets) / fps
    max_bytes_per_second = int(
        (video_bytes + len(audio)) / max(duration_seconds, 0.001)
    )
    headers = build_headers(
        width,
        height,
        fps,
        len(packets),
        audio_rate,
        len(audio),
        suggested_buffer,
        max_bytes_per_second,
    )

    destination.parent.mkdir(parents=True, exist_ok=True)
    with destination.open("wb") as stream:
        stream.write(b"RIFF\0\0\0\0AVI ")
        stream.write(headers)
        movi_start = stream.tell()
        stream.write(b"LIST\0\0\0\0movi")
        audio_position = 0
        for frame_index, packet in enumerate(packets):
            if frame_index % 2 == 0:
                covered_frames = min(frame_index + 2, len(packets))
                audio_end = min(
                    len(audio),
                    (covered_frames * audio_rate + fps // 2) // fps,
                )
                # Carry one odd sample into the next interleave group instead
                # of padding every group.  This keeps long movies in sync;
                # at most the final stream chunk needs one neutral pad byte.
                if (audio_end - audio_position) & 1:
                    audio_end -= 1
                if audio_end > audio_position:
                    write_even_stream_chunk(
                        stream, b"01wb", audio[audio_position:audio_end]
                    )
                    audio_position = audio_end
            write_even_stream_chunk(stream, b"00dc", packet)
        if audio_position < len(audio):
            write_even_stream_chunk(stream, b"01wb", audio[audio_position:])

        file_end = stream.tell()
        stream.seek(movi_start + 4)
        stream.write(struct.pack("<I", file_end - (movi_start + 8)))
        stream.seek(4)
        stream.write(struct.pack("<I", file_end - 8))


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Encode PAL98 AVI with a quality-controlled RGB555 MS Video 1 "
            "encoder while retaining SDLPAL's very small decoder."
        )
    )
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    parser.add_argument("--width", type=int, default=288)
    parser.add_argument("--height", type=int, default=180)
    parser.add_argument("--fps", type=int, default=15)
    parser.add_argument("--audio-rate", type=int, default=11025)
    parser.add_argument("--quality-error", type=int, default=14)
    parser.add_argument("--skip-error", type=int, default=7)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    source = args.source.resolve()
    destination = args.destination.resolve()
    if not source.is_file():
        raise SystemExit(f"source AVI does not exist: {source}")
    if source == destination:
        raise SystemExit("source and destination must be different")
    if args.width <= 0 or args.height <= 0 or args.width % 4 or args.height % 4:
        raise SystemExit("width and height must be positive multiples of four")
    if args.fps <= 0 or args.audio_rate <= 0:
        raise SystemExit("fps and audio rate must be positive")
    if args.quality_error < 0 or args.skip_error < 0:
        raise SystemExit("quality and skip errors cannot be negative")

    ffmpeg = shutil.which("ffmpeg")
    if not ffmpeg:
        raise SystemExit("ffmpeg is required")
    stats = EncodeStats()
    try:
        packets = decode_video_frames(
            ffmpeg,
            source,
            args.width,
            args.height,
            args.fps,
            args.quality_error,
            args.skip_error,
            stats,
        )
        audio = decode_audio(ffmpeg, source, args.audio_rate)
        write_avi(
            destination,
            packets,
            audio,
            args.width,
            args.height,
            args.fps,
            args.audio_rate,
        )
    except (OSError, RuntimeError, ValueError) as error:
        raise SystemExit(f"MS Video 1 encoding failed: {error}") from error

    report = stats.report()
    report.update({
        "source": str(source),
        "destination": str(destination),
        "output_bytes": destination.stat().st_size,
        "audio_bytes": len(audio),
        "width": args.width,
        "height": args.height,
        "fps": args.fps,
        "audio_rate": args.audio_rate,
        "quality_error": args.quality_error,
        "skip_error": args.skip_error,
    })
    print(json.dumps(report, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
