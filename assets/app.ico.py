import ctypes
import struct
import zlib
from pathlib import Path


SIZES = (16, 20, 24, 32, 64)
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
BLACK = (0, 0, 0, 255)
WHITE = (255, 255, 255, 255)


SMALL_GLYPHS = {
    16: {
        "T": ("1111", "0100", "0100", "0100", "0100", "0100", "0100"),
        "S": ("0111", "1000", "1000", "0110", "0001", "0001", "1110"),
        "gap": 1,
        "x": 1,
        "y": 4,
    },
    20: {
        "T": (
            "11111",
            "00100",
            "00100",
            "00100",
            "00100",
            "00100",
            "00100",
            "00100",
            "00100",
        ),
        "S": (
            "01110",
            "10001",
            "10000",
            "10000",
            "01110",
            "00001",
            "00001",
            "10001",
            "01110",
        ),
        "gap": 1,
        "x": 1,
        "y": 5,
    },
    24: {
        "T": (
            "11111",
            "00100",
            "00100",
            "00100",
            "00100",
            "00100",
            "00100",
            "00100",
            "00100",
            "00100",
            "00100",
        ),
        "S": (
            "01110",
            "10001",
            "10000",
            "10000",
            "10000",
            "01110",
            "00001",
            "00001",
            "00001",
            "10001",
            "01110",
        ),
        "gap": 2,
        "x": 2,
        "y": 6,
    },
}


def png_chunk(kind, payload):
    body = kind + payload
    return struct.pack(">I", len(payload)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)


def write_png(path, width, height, rows):
    raw = bytearray()
    for row in rows:
        raw.append(0)
        for red, green, blue, alpha in row:
            raw.extend((red, green, blue, alpha))

    data = bytearray(PNG_SIGNATURE)
    data.extend(png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)))
    data.extend(png_chunk(b"IDAT", zlib.compress(bytes(raw), 9)))
    data.extend(png_chunk(b"IEND", b""))
    path.write_bytes(data)


def draw_pixel_icon(size):
    config = SMALL_GLYPHS[size]
    rows = [[BLACK for _ in range(size)] for _ in range(size)]
    cursor_x = config["x"]

    for index, char in enumerate("TTS"):
        glyph = config[char]
        for glyph_y, pattern in enumerate(glyph):
            for glyph_x, bit in enumerate(pattern):
                if bit != "1":
                    continue
                x = cursor_x + glyph_x
                y = config["y"] + glyph_y
                if 0 <= x < size and 0 <= y < size:
                    rows[y][x] = WHITE

        cursor_x += len(glyph[0])
        if index != 2:
            cursor_x += config["gap"]

    return rows


class BITMAPINFOHEADER(ctypes.Structure):
    _fields_ = [
        ("biSize", ctypes.c_uint32),
        ("biWidth", ctypes.c_int32),
        ("biHeight", ctypes.c_int32),
        ("biPlanes", ctypes.c_uint16),
        ("biBitCount", ctypes.c_uint16),
        ("biCompression", ctypes.c_uint32),
        ("biSizeImage", ctypes.c_uint32),
        ("biXPelsPerMeter", ctypes.c_int32),
        ("biYPelsPerMeter", ctypes.c_int32),
        ("biClrUsed", ctypes.c_uint32),
        ("biClrImportant", ctypes.c_uint32),
    ]


class BITMAPINFO(ctypes.Structure):
    _fields_ = [("bmiHeader", BITMAPINFOHEADER), ("bmiColors", ctypes.c_uint32 * 3)]


class RECT(ctypes.Structure):
    _fields_ = [("left", ctypes.c_long), ("top", ctypes.c_long), ("right", ctypes.c_long), ("bottom", ctypes.c_long)]


class SIZE(ctypes.Structure):
    _fields_ = [("cx", ctypes.c_long), ("cy", ctypes.c_long)]


class TEXTMETRICW(ctypes.Structure):
    _fields_ = [
        ("tmHeight", ctypes.c_long),
        ("tmAscent", ctypes.c_long),
        ("tmDescent", ctypes.c_long),
        ("tmInternalLeading", ctypes.c_long),
        ("tmExternalLeading", ctypes.c_long),
        ("tmAveCharWidth", ctypes.c_long),
        ("tmMaxCharWidth", ctypes.c_long),
        ("tmWeight", ctypes.c_long),
        ("tmOverhang", ctypes.c_long),
        ("tmDigitizedAspectX", ctypes.c_long),
        ("tmDigitizedAspectY", ctypes.c_long),
        ("tmFirstChar", ctypes.c_wchar),
        ("tmLastChar", ctypes.c_wchar),
        ("tmDefaultChar", ctypes.c_wchar),
        ("tmBreakChar", ctypes.c_wchar),
        ("tmItalic", ctypes.c_byte),
        ("tmUnderlined", ctypes.c_byte),
        ("tmStruckOut", ctypes.c_byte),
        ("tmPitchAndFamily", ctypes.c_byte),
        ("tmCharSet", ctypes.c_byte),
    ]


def draw_font_icon(size):
    gdi32 = ctypes.windll.gdi32
    user32 = ctypes.windll.user32

    hdc = gdi32.CreateCompatibleDC(0)
    if not hdc:
        raise ctypes.WinError()

    bits = ctypes.c_void_p()
    bitmap_info = BITMAPINFO()
    bitmap_info.bmiHeader.biSize = ctypes.sizeof(BITMAPINFOHEADER)
    bitmap_info.bmiHeader.biWidth = size
    bitmap_info.bmiHeader.biHeight = -size
    bitmap_info.bmiHeader.biPlanes = 1
    bitmap_info.bmiHeader.biBitCount = 32
    bitmap_info.bmiHeader.biCompression = 0
    bitmap_info.bmiHeader.biSizeImage = size * size * 4

    bitmap = gdi32.CreateDIBSection(hdc, ctypes.byref(bitmap_info), 0, ctypes.byref(bits), None, 0)
    if not bitmap:
        gdi32.DeleteDC(hdc)
        raise ctypes.WinError()

    old_bitmap = gdi32.SelectObject(hdc, bitmap)
    gdi32.PatBlt(hdc, 0, 0, size, size, 0x00000042)
    gdi32.SetBkMode(hdc, 1)
    gdi32.SetTextColor(hdc, 0x00FFFFFF)

    chosen_font = None
    old_font = None
    text = "TTS"
    for pixels in range(int(size * 0.9), 5, -1):
        font = gdi32.CreateFontW(
            -pixels,
            0,
            0,
            0,
            700,
            0,
            0,
            0,
            1,
            4,
            0,
            4,
            0x31,
            "Consolas",
        )
        previous_font = gdi32.SelectObject(hdc, font)
        measured = SIZE()
        metrics = TEXTMETRICW()
        gdi32.GetTextExtentPoint32W(hdc, text, len(text), ctypes.byref(measured))
        gdi32.GetTextMetricsW(hdc, ctypes.byref(metrics))
        if measured.cx <= size - 2 and metrics.tmHeight <= size - 4:
            chosen_font = font
            old_font = previous_font
            break
        gdi32.SelectObject(hdc, previous_font)
        gdi32.DeleteObject(font)

    if chosen_font is None:
        chosen_font = gdi32.CreateFontW(-10, 0, 0, 0, 700, 0, 0, 0, 1, 4, 0, 4, 0x31, "Consolas")
        old_font = gdi32.SelectObject(hdc, chosen_font)

    rect = RECT(0, 0, size, size)
    user32.DrawTextW(hdc, text, len(text), ctypes.byref(rect), 0x00000001 | 0x00000004 | 0x00000020 | 0x00000100)

    buffer_type = ctypes.c_ubyte * (size * size * 4)
    buffer = buffer_type.from_address(bits.value)
    rows = []
    for y in range(size):
        row = []
        for x in range(size):
            offset = (y * size + x) * 4
            blue, green, red = buffer[offset], buffer[offset + 1], buffer[offset + 2]
            row.append((red, green, blue, 255))
        rows.append(row)

    gdi32.SelectObject(hdc, old_font)
    gdi32.DeleteObject(chosen_font)
    gdi32.SelectObject(hdc, old_bitmap)
    gdi32.DeleteObject(bitmap)
    gdi32.DeleteDC(hdc)
    return rows


def generate_pngs(base_dir):
    for size in (16, 20, 24):
        path = base_dir / f"app-{size}x{size}.png"
        write_png(path, size, size, draw_pixel_icon(size))
        print(f"wrote {path} ({path.stat().st_size} bytes)")

    for size in (32, 64):
        path = base_dir / f"app-{size}x{size}.png"
        write_png(path, size, size, draw_font_icon(size))
        print(f"wrote {path} ({path.stat().st_size} bytes)")


def read_png(path):
    data = path.read_bytes()
    if not data.startswith(PNG_SIGNATURE):
        raise ValueError(f"{path} is not a PNG file")

    width, height = struct.unpack(">II", data[16:24])
    return width, height, data


def build_ico(base_dir):
    images = []
    for size in SIZES:
        path = base_dir / f"app-{size}x{size}.png"
        width, height, data = read_png(path)
        if (width, height) != (size, size):
            raise ValueError(f"{path} is {width}x{height}, expected {size}x{size}")
        images.append((size, data))

    header = struct.pack("<HHH", 0, 1, len(images))
    directory = bytearray()
    offset = len(header) + 16 * len(images)

    for size, data in images:
        directory.extend(struct.pack("<BBBBHHII", size, size, 0, 0, 1, 32, len(data), offset))
        offset += len(data)

    return header + directory + b"".join(data for _, data in images)


def main():
    base_dir = Path(__file__).resolve().parent
    generate_pngs(base_dir)

    ico_path = base_dir / "app.ico"
    ico_path.write_bytes(build_ico(base_dir))
    print(f"wrote {ico_path} ({ico_path.stat().st_size} bytes)")


if __name__ == "__main__":
    main()