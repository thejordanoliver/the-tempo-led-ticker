"""Unit tests for offline logo packing. Run: python3 -m unittest discover -s tests/firmware -v"""
from __future__ import annotations

import importlib.util
import io
import tempfile
import unittest
from pathlib import Path

from PIL import Image

SCRIPT = Path(__file__).resolve().parents[2] / "tools" / "generate_matrix_logos.py"
spec = importlib.util.spec_from_file_location("matrix_logo_converter", SCRIPT)
assert spec and spec.loader
generator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generator)


class LogoPackTests(unittest.TestCase):
    def test_rgb565_and_alpha_mask_keep_real_white(self):
        image = Image.new("RGBA", (4, 4), (0, 0, 0, 0))
        image.putpixel((1, 1), (255, 255, 255, 255))
        pixels, alpha = generator.pixel_sprite(image)
        self.assertEqual(len(pixels), 1024)
        self.assertEqual(len(alpha), 128)
        self.assertIn(65535, pixels)
        self.assertEqual(sum(b.bit_count() for b in alpha), 1)  # transparent surround stays transparent

    def test_read_local_source_and_multi_league_bundle(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            img = Image.new("RGBA", (32, 24), (2, 200, 123, 255))
            img.save(root / "logo.png")
            games = [{"away": {"code": "BOS", "logo": "logo.png"}}]
            data = {"sources": [
                {"league": "nba", "games": games},
                {"league": "nfl", "games": games}
            ]}
            header, count, failures = generator.convert(data, None, 64, root, set())
            self.assertEqual(count, 2)
            self.assertEqual(failures, [])
            self.assertIn('{"nba", "BOS"', header)
            self.assertIn('{"nfl", "BOS"', header)
            self.assertIn("kLogoMask1[128]", header)

    def test_bad_payload_and_paths_rejected(self):
        with self.assertRaises(ValueError):
            generator.collect_games({"games": "not an array"})
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            with self.assertRaises(ValueError):
                generator.read_image("../outside.png", assets_root=root, allowed_hosts=set())
            with self.assertRaises(ValueError):
                generator.read_image("https://127.0.0.1/logo.png",
                                     assets_root=None, allowed_hosts={"a.espncdn.com"})
        self.assertEqual(generator.normalize_code("TEAM!!LONG"), "TEAML")
        self.assertIsNone(generator.normalize_code("!!!"))

    def test_failures_do_not_generate_empty_header(self):
        with self.assertRaises(ValueError):
            generator.convert({"games": [{"away": {"code": "BOS",
                "logo": "https://localhost/logo.png"}}]}, "nba", 64, None, set())


if __name__ == "__main__":
    unittest.main()
