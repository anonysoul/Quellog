#!/usr/bin/env python3

from __future__ import annotations

import argparse
import math
import struct
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


MAGIC = b"QGF1"
VERSION = 1
ASCII_RANGE = range(0x20, 0x7F)
CJK_RANGE = range(0x4E00, 0xA000)
CJK_PUNCTUATION = range(0x3000, 0x3040)
FULLWIDTH_RANGE = range(0xFF00, 0xFFF0)
EXTRA_CODEPOINTS = {
    ord("…"),
    ord("□"),
}
FONT_CANDIDATES = (
    (str((Path(__file__).resolve().parent.parent / "assets" / "LXGWWenKai-Regular.ttf")), 0),
    ("/usr/share/fonts/opentype/noto/NotoSansCJK-Medium.ttc", 2),
    ("/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf", 0),
)


def load_font(size: int, font_path: str | None = None, font_index: int = 0) -> ImageFont.FreeTypeFont:
    if font_path is not None:
        path = Path(font_path)
        if not path.is_file():
            raise RuntimeError(f"Font file not found: {font_path}")
        return ImageFont.truetype(str(path), size=size, index=font_index)

    for candidate, index in FONT_CANDIDATES:
        path = Path(candidate)
        if path.is_file():
            return ImageFont.truetype(str(path), size=size, index=index)
    raise RuntimeError("No usable CJK font found on this system, and LXGW WenKai asset is missing")


def codepoints() -> list[int]:
    values = set(ASCII_RANGE)
    values.update(CJK_RANGE)
    values.update(CJK_PUNCTUATION)
    values.update(FULLWIDTH_RANGE)
    values.update(EXTRA_CODEPOINTS)
    return sorted(values)


def render_glyph(font: ImageFont.FreeTypeFont, char: str, line_height: int) -> tuple[bytes, tuple[int, int, int, int, int]]:
    probe = Image.new("1", (line_height * 2, line_height * 2), 0)
    draw = ImageDraw.Draw(probe)
    bbox = draw.textbbox((1, 0), char, font=font)
    advance = max(1, int(math.ceil(font.getlength(char))))
    if bbox is None:
        width = max(1, advance)
        height = line_height
        return b"\x00" * ((width * height + 7) // 8), (width, height, 0, 0, advance)

    width = max(1, bbox[2] - bbox[0])
    height = max(1, bbox[3] - bbox[1])
    image = Image.new("1", (width, height), 0)
    image_draw = ImageDraw.Draw(image)
    image_draw.text((-bbox[0], -bbox[1]), char, fill=1, font=font)

    packed = bytearray((width * height + 7) // 8)
    pixels = image.load()
    bit_index = 0
    for y in range(height):
        for x in range(width):
            if pixels[x, y]:
                packed[bit_index // 8] |= 1 << (bit_index % 8)
            bit_index += 1

    return bytes(packed), (width, height, bbox[0] - 1, bbox[1], advance)


def build_partition(output: Path, font_size: int, font_path: str | None = None, font_index: int = 0) -> None:
    font = load_font(font_size, font_path=font_path, font_index=font_index)
    ascent, descent = font.getmetrics()
    line_height = ascent + descent
    glyph_records: list[bytes] = []
    bitmap_blob = bytearray()

    all_codepoints = codepoints()
    fallback_index = all_codepoints.index(ord("?"))

    for codepoint in all_codepoints:
        bitmap, metrics = render_glyph(font, chr(codepoint), line_height)
        width, height, x_offset, y_offset, advance = metrics
        glyph_records.append(
            struct.pack(
                "<IIHHhhH",
                codepoint,
                len(bitmap_blob),
                width,
                height,
                x_offset,
                y_offset,
                advance,
            )
        )
        glyph_records.append(struct.pack("<H", len(bitmap)))
        bitmap_blob.extend(bitmap)

    glyph_table = b"".join(glyph_records)
    header_size = 4 + 2 + 2 + 4 + 2 + 2 + 2 + 2 + 4 + 4 + 4
    glyph_table_offset = header_size
    bitmap_offset = glyph_table_offset + len(glyph_table)
    header = struct.pack(
        "<4sHHIhhHHIII",
        MAGIC,
        VERSION,
        0,
        len(all_codepoints),
        line_height,
        ascent,
        font_size,
        fallback_index,
        glyph_table_offset,
        bitmap_offset,
        len(bitmap_blob),
    )
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(header + glyph_table + bitmap_blob)


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate Quellog font partition image")
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--font-size", default=16, type=int)
    parser.add_argument("--font-path", type=str, default=None)
    parser.add_argument("--font-index", type=int, default=0)
    args = parser.parse_args()
    build_partition(args.output, args.font_size, font_path=args.font_path, font_index=args.font_index)


if __name__ == "__main__":
    main()
