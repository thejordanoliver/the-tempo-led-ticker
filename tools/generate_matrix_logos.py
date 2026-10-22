#!/usr/bin/env python3
"""Convert Tempo game-logo URLs (or local images) to compile-time RGB565 sprites.

Usage:
  python3 -m pip install Pillow
  python3 tools/generate_matrix_logos.py --league nba --input response.json \
    --output firmware/matrixportal-s3/TempoMatrixLive/logos.generated.h

No API credentials are needed. Only allowlisted HTTPS domains can be downloaded.
Generated assets are gitignored because their source images may have license terms.
A JSON bundle of {"sources": [{"league": "nba", "games": [...]}, ...]} supports multiple leagues.
"""
from __future__ import annotations

import argparse
import io
import json
from pathlib import Path
from urllib.parse import urlparse
from urllib.request import HTTPRedirectHandler, Request, build_opener

try:
    from PIL import Image
except ImportError as exc:
    raise SystemExit("Pillow is required: python3 -m pip install Pillow") from exc

SIZE = 32
MAX_IMAGE_BYTES = 2_000_000
DEFAULT_ALLOWED_HOSTS = {"res.cloudinary.com", "a.espncdn.com", "cdn.espn.com"}


class NoRedirect(HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


def read_image(source: str, *, assets_root: Path | None, allowed_hosts: set[str]) -> Image.Image:
    parsed = urlparse(source)
    if parsed.scheme == "https":
        host = (parsed.hostname or "").lower()
        if host not in allowed_hosts or parsed.username or parsed.password or parsed.port not in (None, 443):
            raise ValueError(f"Image host is not allowed: {host}")
        opener = build_opener(NoRedirect())
        with opener.open(Request(source, headers={"User-Agent": "TempoMatrixLogoPrep/1.0"}),
                         timeout=8) as response:
            if response.headers.get("Content-Length") and int(response.headers["Content-Length"]) > MAX_IMAGE_BYTES:
                raise ValueError("Image exceeds maximum download size")
            payload = response.read(MAX_IMAGE_BYTES + 1)
            if len(payload) > MAX_IMAGE_BYTES:
                raise ValueError("Image exceeds maximum download size")
        file_obj = io.BytesIO(payload)
    elif parsed.scheme == "":
        if assets_root is None:
            raise ValueError("Local asset paths require --assets-root")
        path = (assets_root / source).resolve()
        if not path.is_relative_to(assets_root.resolve()) or not path.is_file():
            raise ValueError("Local image escapes assets directory")
        if path.stat().st_size > MAX_IMAGE_BYTES:
            raise ValueError("Local image exceeds size limit")
        file_obj = io.BytesIO(path.read_bytes())
    else:
        raise ValueError("Only HTTPS URLs or local assets under --assets-root are supported")
    with Image.open(file_obj) as im:
        if im.width > 4096 or im.height > 4096:
            raise ValueError("Source image dimensions too large")
        return im.convert("RGBA")


def normalize_code(code: object) -> str | None:
    if not isinstance(code, str):
        return None
    safe = "".join(ch for ch in code.upper() if ("A" <= ch <= "Z" or "0" <= ch <= "9"))[:5]
    return safe or None


def pixel_sprite(image: Image.Image) -> tuple[list[int], list[int]]:
    image = image.convert("RGBA")
    alpha = image.getchannel("A").point(lambda v: 255 if v > 16 else 0)
    bounds = alpha.getbbox()
    if not bounds:
        raise ValueError("Image contains no visible pixels")
    cropped = image.crop(bounds)
    cropped.thumbnail((SIZE, SIZE), Image.Resampling.LANCZOS)
    canvas = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    canvas.alpha_composite(cropped, ((SIZE - cropped.width) // 2, (SIZE - cropped.height) // 2))
    pixels, mask = [], [0] * (SIZE * SIZE // 8)
    for idx, (r, g, b, a) in enumerate(canvas.getdata()):
        pixels.append(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3))
        if a > 32:
            mask[idx // 8] |= 0x80 >> (idx % 8)
    return pixels, mask


def collect_games(data: object) -> list[dict]:
    if isinstance(data, dict):
        data = data.get("games")
    if not isinstance(data, list):
        raise ValueError("Input JSON must be a games array or an object with a games array")
    return [game for game in data if isinstance(game, dict)]


def convert(data: object, league: str | None, max_logos: int, assets_root: Path | None,
            allowed_hosts: set[str]) -> tuple[str, int, list[str]]:
    # A source bundle can include NBA/NFL/CFB, keeping league keys distinct.
    if isinstance(data, dict) and isinstance(data.get("sources"), list):
        sources = []
        for source in data["sources"]:
            if not isinstance(source, dict) or source.get("league") not in ("nba", "nfl", "cfb"):
                raise ValueError("Each source needs an nba/nfl/cfb league")
            sources.append((source["league"], collect_games(source)))
    elif league in ("nba", "nfl", "cfb"):
        sources = [(league, collect_games(data))]
    else:
        raise ValueError("Use --league for a single response or JSON sources for multiple leagues")

    codes: dict[tuple[str, str], str] = {}
    for selected, all_games in sources:
        for game in all_games:
            for side in ("away", "home"):
                team = game.get(side)
                if not isinstance(team, dict):
                    continue
                code = normalize_code(team.get("code"))
                uri = team.get("logoDark") or team.get("logo")
                if code and isinstance(uri, str) and uri:
                    codes.setdefault((selected, code), uri)
    entries = []
    failures: list[str] = []
    for (selected, code), uri in sorted(codes.items()):
        if len(entries) >= max_logos:
            break
        try:
            image = read_image(uri, assets_root=assets_root, allowed_hosts=allowed_hosts)
            pixels, mask = pixel_sprite(image)
            entries.append((selected, code, pixels, mask))
        except Exception as exc:
            failures.append(f"{selected}/{code}: {exc}")
    if not entries:
        raise ValueError("No usable logo assets. Check image URLs, allowed hosts, and local paths.")

    lines = [
        "#pragma once",
        "#include <stdint.h>",
        "#include <pgmspace.h>",
        "// Generated by tools/generate_matrix_logos.py (32x32 RGB565 + alpha mask).",
        "// Include only via TempoLogos.h, after TempoLogoEntry is declared.",
    ]
    for i, (selected, code, pixels, mask) in enumerate(entries):
        lines.append(f"// {selected.upper()} {code}")
        lines.append(f"static const uint16_t kLogoRgb{i}[1024] PROGMEM = {{")
        for start in range(0, len(pixels), 16):
            lines.append("  " + ", ".join(f"0x{v:04X}" for v in pixels[start:start + 16]) + ",")
        lines.append("};")
        lines.append(f"static const uint8_t kLogoMask{i}[128] PROGMEM = {{")
        for start in range(0, len(mask), 16):
            lines.append("  " + ", ".join(f"0x{v:02X}" for v in mask[start:start + 16]) + ",")
        lines.append("};")
    lines.append("static const TempoLogoEntry kTempoLogos[] = {")
    for i, (selected, code, _, _) in enumerate(entries):
        lines.append(f'  {{"{selected}", "{code}", kLogoRgb{i}, kLogoMask{i}}},')
    lines.append("};")
    lines.append("static constexpr size_t kTempoLogoCount = sizeof(kTempoLogos) / sizeof(kTempoLogos[0]);")
    return "\n".join(lines) + "\n", len(entries), failures


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path, help="Tempo API response JSON")
    parser.add_argument("--output", required=True, type=Path, help="generated .h path")
    parser.add_argument("--league", choices=("nba", "nfl", "cfb"), help="for single-league input")
    parser.add_argument("--max-logos", type=int, default=64)
    parser.add_argument("--assets-root", type=Path, help="allow local image paths inside this directory")
    parser.add_argument("--allow-host", action="append", default=[],
                        help="additional exact HTTPS hostname (only when trusted)")
    args = parser.parse_args()
    if not 1 <= args.max_logos <= 128:
        parser.error("--max-logos must be between 1 and 128")
    content, count, failures = convert(
        json.loads(args.input.read_text(encoding="utf-8")),
        args.league, args.max_logos, args.assets_root,
        DEFAULT_ALLOWED_HOSTS | {h.lower() for h in args.allow_host},
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(content, encoding="utf-8")
    print(f"Created {count} 32x32 sprites: {args.output}")
    print(f"Approximate bitmap flash storage: {count * (SIZE * SIZE * 2 + SIZE * SIZE // 8):,} bytes")
    for failure in failures:
        print(f"Skipped {failure}")


if __name__ == "__main__":
    main()
