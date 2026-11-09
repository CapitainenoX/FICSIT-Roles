"""FICSIT Roles asset pipeline.

Splits the transparent reference atlases into separate, trimmed images and generates the
procedural UI pieces (chamfered 9-slice fill/frame, safety stripes, shade, logo).
No text is baked into functional UI textures; the logo/banner are the only images with lettering.

Usage: python -I tools/make_assets.py   (run from F:\\FICSIT-Roles-Rebuild)
"""
import json
import math
import os
import sys

from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REF = os.path.join(ROOT, "references")
OUT = os.path.join(ROOT, "Mod", "FICSITRoles", "Resources", "UI")
RES = os.path.join(ROOT, "Mod", "FICSITRoles", "Resources")
SS = 4  # supersampling for anti-aliased procedural shapes
ORANGE = (244, 154, 42, 255)
DARK = (22, 24, 27, 255)
DIV_COLORS = {"exploration": (79, 180, 119), "energy": (242, 182, 48), "industry": (90, 160, 216), "logistics": (180, 140, 224)}
manifest = []


def save(img, name, note, folder=OUT):
    path = os.path.join(folder, name)
    img.save(path, optimize=True)
    a = img.getchannel("A").getextrema() if img.mode == "RGBA" else None
    manifest.append({"file": os.path.relpath(path, RES).replace("\\", "/"), "size": img.size, "alpha": a, "source": note})


def trim(img, pad=0):
    box = img.getchannel("A").point(lambda v: 255 if v > 8 else 0).getbbox()
    img = img.crop(box)
    if pad:
        w, h = img.size
        canvas = Image.new("RGBA", (w + 2 * pad, h + 2 * pad), (0, 0, 0, 0))
        canvas.paste(img, (pad, pad))
        img = canvas
    return img


def square(img, size):
    w, h = img.size
    s = max(w, h)
    canvas = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    canvas.paste(img, ((s - w) // 2, (s - h) // 2))
    return canvas.resize((size, size), Image.LANCZOS)


def components(alpha, min_area):
    """Bounding boxes of connected alpha regions (4-neighbour flood fill on a downscaled mask)."""
    small = alpha.resize((alpha.width // 4, alpha.height // 4), Image.BILINEAR).point(lambda v: 1 if v > 20 else 0)
    w, h = small.size
    px = small.load()
    seen = [[False] * w for _ in range(h)]
    boxes = []
    for y in range(h):
        for x in range(w):
            if px[x, y] and not seen[y][x]:
                stack = [(x, y)]
                seen[y][x] = True
                x0 = x1 = x
                y0 = y1 = y
                n = 0
                while stack:
                    cx, cy = stack.pop()
                    n += 1
                    x0, x1, y0, y1 = min(x0, cx), max(x1, cx), min(y0, cy), max(y1, cy)
                    for nx, ny in ((cx + 1, cy), (cx - 1, cy), (cx, cy + 1), (cx, cy - 1)):
                        if 0 <= nx < w and 0 <= ny < h and px[nx, ny] and not seen[ny][nx]:
                            seen[ny][nx] = True
                            stack.append((nx, ny))
                if n >= min_area:
                    boxes.append((x0 * 4, y0 * 4, (x1 + 1) * 4, (y1 + 1) * 4))
    return boxes


def merge_boxes(boxes, gap):
    merged = True
    while merged:
        merged = False
        out = []
        for b in boxes:
            for i, o in enumerate(out):
                if b[0] - gap < o[2] and o[0] - gap < b[2] and b[1] - gap < o[3] and o[1] - gap < b[3]:
                    out[i] = (min(b[0], o[0]), min(b[1], o[1]), max(b[2], o[2]), max(b[3], o[3]))
                    merged = True
                    break
            else:
                out.append(b)
        boxes = out
    return boxes


# --- 1. Division icons from the transparent 2x2 icon atlas -------------------------------------
def division_icons():
    atlas = Image.open(os.path.join(REF, "Icons.png")).convert("RGBA")
    boxes = merge_boxes(components(atlas.getchannel("A"), 40), 60)
    boxes = sorted(boxes, key=lambda b: (b[1] // 300, b[0]))  # row-major
    if len(boxes) != 4:
        sys.exit(f"Icons.png: expected 4 icons, found {len(boxes)}")
    names = ["industry", "logistics", "energy", "exploration"]  # atlas order verified visually
    for name, box in zip(names, boxes):
        icon = trim(atlas.crop(box))
        save(square(icon, 256), f"div_{name}.png", f"references/Icons.png crop {box}")


# --- 2. Player-count illustrations from the transparent card atlas -----------------------------
def player_cards():
    atlas = Image.open(os.path.join(REF, "PlayersAtlas-v2.png")).convert("RGBA")
    boxes = merge_boxes(components(atlas.getchannel("A"), 400), 8)
    boxes = sorted([b for b in boxes if (b[2] - b[0]) > 200], key=lambda b: b[0])
    if len(boxes) != 4:
        sys.exit(f"PlayersAtlas-v2.png: expected 4 cards, found {len(boxes)}")
    for i, (x0, y0, x1, y1) in enumerate(boxes, start=1):
        w, h = x1 - x0, y1 - y0
        # Inside the baked metal frame, and above the baked corner stripes (drawn natively instead).
        inner = atlas.crop((x0 + int(w * 0.05), y0 + int(h * 0.035), x1 - int(w * 0.05), y1 - int(h * 0.11))).convert("RGB")
        target_w = 380
        inner = inner.resize((target_w, int(inner.height * target_w / inner.width)), Image.LANCZOS)
        save(inner.convert("RGBA"), f"players_{i}.png", f"references/PlayersAtlas-v2.png card {i} inner crop")


# --- 3. Procedural UI pieces --------------------------------------------------------------------
def chamfer_poly(w, h, c):
    # Bevel on top-left and bottom-right corners: the FICSIT-like asymmetric cut.
    return [(c, 0), (w, 0), (w, h - c), (w - c, h), (0, h), (0, c)]


def fill_and_frame():
    size, cut, line = 64, 12, 2
    big = Image.new("RGBA", (size * SS, size * SS), (0, 0, 0, 0))
    ImageDraw.Draw(big).polygon([(x * SS, y * SS) for x, y in chamfer_poly(size, size, cut)], fill=(255, 255, 255, 255))
    save(big.resize((size, size), Image.LANCZOS), "fill.png", "procedural chamfer fill (9-slice 16px)")

    frame = Image.new("RGBA", (size * SS, size * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(frame)
    d.polygon([(x * SS, y * SS) for x, y in chamfer_poly(size, size, cut)], fill=(255, 255, 255, 255))
    inset = line * SS
    inner = [(x * SS, y * SS) for x, y in chamfer_poly(size - 2 * line, size - 2 * line, cut - 1)]
    d.polygon([(x + inset, y + inset) for x, y in inner], fill=(0, 0, 0, 0))
    save(frame.resize((size, size), Image.LANCZOS), "frame.png", "procedural chamfer frame 2px (9-slice 16px)")


def stripes():
    w, h = 128, 40
    big = Image.new("RGBA", (w * SS, h * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(big)
    step, thick = 22, 11
    for k in range(-2, 8):
        x = k * step * SS
        d.polygon([(x, h * SS), (x + thick * SS, h * SS), (x + (thick + h) * SS, 0), (x + h * SS, 0)], fill=(255, 255, 255, 255))
    save(big.resize((w, h), Image.LANCZOS), "stripes.png", "procedural safety stripes (tinted at runtime)")


def shade():
    w, h = 8, 256
    img = Image.new("RGBA", (w, h))
    px = img.load()
    for y in range(h):
        t = y / (h - 1)
        a = 0.88 * max(0.0, 1 - t / 0.42) ** 1.6 + 0.55 * max(0.0, (t - 0.72) / 0.28) ** 1.4
        for x in range(w):
            px[x, y] = (14, 15, 17, int(255 * min(1.0, a)))
    save(img, "shade.png", "procedural legibility gradient")


def logo_mark(size):
    s = size * SS
    img = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    c = s * 0.2
    d.polygon([(c, 0), (s, 0), (s, s - c), (s - c, s), (0, s), (0, c)], fill=ORANGE)
    m = s * 0.075
    d.polygon([(c + m * 0.4, m), (s - m, m), (s - m, s - c - m * 0.4), (s - c - m * 0.4, s - m), (m, s - m), (m, c + m * 0.4)], fill=DARK)
    # Four division segments around a hub: one shared factory, four specialities.
    cx = cy = s / 2
    r_out, r_in = s * 0.30, s * 0.17
    order = ["exploration", "energy", "industry", "logistics"]
    for i, name in enumerate(order):
        a0 = math.radians(-90 + i * 90 + 7)
        a1 = math.radians(-90 + (i + 1) * 90 - 7)
        pts = []
        for k in range(24):
            a = a0 + (a1 - a0) * k / 23
            pts.append((cx + r_out * math.cos(a), cy + r_out * math.sin(a)))
        for k in range(24):
            a = a1 - (a1 - a0) * k / 23
            pts.append((cx + r_in * math.cos(a), cy + r_in * math.sin(a)))
        d.polygon(pts, fill=DIV_COLORS[name] + (255,))
    hub = s * 0.095
    d.polygon([(cx, cy - hub), (cx + hub, cy), (cx, cy + hub), (cx - hub, cy)], fill=ORANGE)
    return img.resize((size, size), Image.LANCZOS)


def logos():
    mark = logo_mark(256)
    save(mark, "logo_mark.png", "procedural logo mark")
    big = logo_mark(512)
    save(big, "logo.png", "procedural logo 512")
    save(logo_mark(128).convert("RGBA"), "Icon128.png", "plugin icon", folder=RES)

    font_path = r"C:\Windows\Fonts\bahnschrift.ttf"
    banner = Image.new("RGBA", (1200, 400), DARK)
    d = ImageDraw.Draw(banner)
    d.rectangle([0, 0, 1200, 6], fill=ORANGE)
    banner.alpha_composite(logo_mark(260), (70, 70))
    try:
        title = ImageFont.truetype(font_path, 112)
        title.set_variation_by_name("Bold")
        sub = ImageFont.truetype(font_path, 34)
    except Exception:
        title = ImageFont.truetype(r"C:\Windows\Fonts\arialbd.ttf", 104)
        sub = ImageFont.truetype(r"C:\Windows\Fonts\arialbd.ttf", 32)
    d.text((380, 112), "FICSIT ROLES", font=title, fill=(236, 234, 230, 255))
    d.text((384, 250), "DIVISIONS  \u2022  PERMISSIONS  \u2022  COOPERATION", font=sub, fill=(155, 161, 168, 255))
    for i in range(5):
        x = 1040 + i * 26
        d.polygon([(x, 360), (x + 14, 360), (x + 34, 330), (x + 20, 330)], fill=ORANGE)
    save(banner, "banner.png", "procedural ficsit.app banner (only image with lettering besides logo)")


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    division_icons()
    player_cards()
    fill_and_frame()
    stripes()
    shade()
    logos()
    with open(os.path.join(ROOT, "docs", "ASSET-MANIFEST.json"), "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=1)
    for m in manifest:
        print(m["file"], m["size"], m["alpha"])
