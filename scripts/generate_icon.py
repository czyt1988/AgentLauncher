#!/usr/bin/env python3
"""Generate the AgentLauncher application icon (PNG + ICO).

Uses Pillow to draw a modern, flat icon:
- Rounded-square gradient background (Catppuccin Blue -> Lavender)
- White stylized rocket (launch motif)

Outputs:
  icons/app-icon.png  (256x256, for Qt resource / runtime window icon)
  icons/app-icon.ico  (multi-resolution, for Windows executable icon)

Run from project root:
  python scripts/generate_icon.py
"""

import os
from PIL import Image, ImageDraw


def lerp_color(c1, c2, t):
    """Linear interpolation between two RGB tuples."""
    return tuple(int(c1[i] + (c2[i] - c1[i]) * t) for i in range(3))


def draw_icon(size=256):
    """Draw the icon at *size* x *size* and return an RGBA Image."""
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))

    # --- Gradient background (rounded square) ---
    # Catppuccin Mocha: Blue (#89b4fa) top  ->  Lavender (#7287fd) bottom
    top_color = (0x89, 0xB4, 0xFA)
    bot_color = (0x72, 0x87, 0xFD)

    # Build a 1px-wide vertical gradient strip, then stretch it.
    strip = Image.new("RGB", (1, size))
    for y in range(size):
        t = y / max(size - 1, 1)
        strip.putpixel((0, y), lerp_color(top_color, bot_color, t))
    gradient = strip.resize((size, size)).convert("RGBA")

    # Mask: rounded rectangle (~22% corner radius, macOS-squircle-ish)
    radius = int(size * 0.22)
    mask = Image.new("L", (size, size), 0)
    ImageDraw.Draw(mask).rounded_rectangle(
        [0, 0, size - 1, size - 1], radius=radius, fill=255
    )
    img.paste(gradient, (0, 0), mask)

    # --- White rocket (launch motif) ---
    # Coordinates defined on a 256x256 canvas, scaled to actual size.
    s = size / 256.0

    def sc(pt):
        return tuple(int(v * s) for v in pt)

    rocket = [
        sc((128, 50)),   # nose tip
        sc((148, 110)),  # right nose-body junction
        sc((148, 140)),  # right body, fin starts
        sc((181, 205)),  # right fin outer tip
        sc((128, 205)),  # bottom center (exhaust)
        sc((75, 205)),   # left fin outer tip
        sc((108, 140)),  # left body, fin starts
        sc((108, 110)),  # left nose-body junction
    ]
    ImageDraw.Draw(img).polygon(rocket, fill=(255, 255, 255, 255))

    # Subtle circular "window" on the rocket body (only visible at larger sizes)
    if size >= 64:
        cx, cy = int(128 * s), int(125 * s)
        r = max(int(8 * s), 2)
        ImageDraw.Draw(img).ellipse(
            [cx - r, cy - r, cx + r, cy + r],
            fill=(0x89, 0xB4, 0xFA, 255),
        )

    return img


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)
    icons_dir = os.path.join(project_root, "icons")
    os.makedirs(icons_dir, exist_ok=True)

    # --- PNG (256x256, for Qt resource) ---
    icon_256 = draw_icon(256)
    png_path = os.path.join(icons_dir, "app-icon.png")
    icon_256.save(png_path, "PNG")
    print(f"Saved {png_path}")

    # --- ICO (multi-resolution for Windows executable) ---
    ico_sizes = [16, 24, 32, 48, 64, 128, 256]
    ico_path = os.path.join(icons_dir, "app-icon.ico")
    icon_256.save(
        ico_path,
        format="ICO",
        sizes=[(s, s) for s in ico_sizes],
    )
    print(f"Saved {ico_path}")


if __name__ == "__main__":
    main()
