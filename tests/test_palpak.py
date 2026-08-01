from __future__ import annotations

from pathlib import Path
import sys
import tempfile
import unittest


TOOLS = Path(__file__).resolve().parents[1] / "tools"
sys.path.insert(0, str(TOOLS))

import palpak


class PalPakTests(unittest.TestCase):
    def test_round_trip_and_case_normalization(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "source"
            output = root / "PAL9588.PAK"
            extracted = root / "extracted"
            source.mkdir()
            (source / "Abc.MKF").write_bytes(bytes(range(256)) * 17)
            (source / "WORD.DAT").write_bytes(b"Li Xiaoyao")

            packed = palpak.pack_directory(source, output)
            self.assertEqual([item.name for item in packed], ["abc.mkf", "word.dat"])
            verified = palpak.verify_archive(output)
            self.assertEqual(packed, verified)
            palpak.extract_archive(output, extracted)
            self.assertEqual(
                (extracted / "abc.mkf").read_bytes(),
                (source / "Abc.MKF").read_bytes(),
            )
            self.assertEqual(
                (extracted / "word.dat").read_bytes(),
                (source / "WORD.DAT").read_bytes(),
            )

    def test_corruption_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "source"
            output = root / "PAL9588.PAK"
            source.mkdir()
            (source / "data.mkf").write_bytes(b"resource payload")
            entry = palpak.pack_directory(source, output)[0]
            with output.open("r+b") as stream:
                stream.seek(entry.offset)
                stream.write(b"X")
            with self.assertRaises(palpak.PalPakError):
                palpak.verify_archive(output)


if __name__ == "__main__":
    unittest.main()
