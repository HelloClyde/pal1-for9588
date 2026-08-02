from __future__ import annotations

import argparse
import sys
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser(description="Pack prelinked PAL1 binary")
    parser.add_argument("raw", type=Path)
    parser.add_argument("--sdk", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--title", required=True)
    parser.add_argument("--icon", required=True, type=Path)
    args = parser.parse_args()

    sys.path.insert(0, str(args.sdk.resolve()))
    from bda_packer.build import (  # pylint: disable=import-outside-toplevel
        ENTRY_OFFSET,
        ENTRY_VA,
        ICON_SIZES,
        ICON_START,
        RUNTIME_FILE_BASE,
        build_icons,
    )
    from bda_packer.header import (  # pylint: disable=import-outside-toplevel
        BdaHeaderFields,
        write_header,
    )
    from bda_packer.validate import validate_bda  # pylint: disable=import-outside-toplevel

    code = args.raw.read_bytes()
    data = bytearray(b"\0" * ENTRY_OFFSET)
    icons = build_icons(args.icon, (0, 0, 0))
    data[ICON_START:ENTRY_OFFSET] = icons
    data.extend(code)
    data.extend(b"\0" * (-len(data) & 3))

    fields = BdaHeaderFields(
        category=4,
        file_size_minus_4=len(data) - 4,
        entry_offset=ENTRY_OFFSET,
        icon_start=ICON_START,
        icon0_size=ICON_SIZES[0],
        icon1_size=ICON_SIZES[1],
        icon2_size=ICON_SIZES[2],
        icon3_size=ICON_SIZES[3],
    )
    write_header(data, fields, args.title)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(data)

    report = validate_bda(args.output)
    if not report["ok"]:
        raise SystemExit("BDA validation failed: " + "; ".join(report["errors"]))
    print(f"output={args.output}")
    print(f"size=0x{len(data):x}")
    print(f"entry_va=0x{ENTRY_VA:x}")
    print(f"runtime_file_base=0x{RUNTIME_FILE_BASE:x}")
    print(f"checksum_ok={report['checksum_ok']}")


if __name__ == "__main__":
    main()
