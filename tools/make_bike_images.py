#!/usr/bin/env python3
"""Generates the bike illustrations used on booked slot cards.

Run from the project root:  python3 tools/make_bike_images.py
Output: resources/images/bike1.jpg .. bike6.jpg (960 x 608, 1.58:1, matching the card's photo area).
Replace any file with a real photo of the same name and a similar aspect ratio to use photography.
Requires Pillow.
"""
from pathlib import Path

from PIL import Image, ImageDraw

OUT_W, OUT_H = 960, 608
SCALE = 3                      # draw at 3x, then downsample for smooth anti-aliased edges
GROUND_Y = 560
WHEEL_R = 118
REAR_HUB = (275, 440)
FRONT_HUB = (685, 440)
BOTTOM_BRACKET = (420, 428)
SEAT_TOP = (370, 250)
HEAD_TOP = (640, 262)
HEAD_BOTTOM = (660, 305)

# (frame colour, sky top, sky bottom, ground colour)
VARIANTS = [
    ((13, 148, 136), (224, 242, 241), (178, 223, 219), (203, 213, 225)),  # teal
    ((234, 88, 12), (255, 247, 237), (254, 215, 170), (214, 211, 209)),   # orange
    ((220, 38, 38), (254, 242, 242), (254, 202, 202), (212, 212, 216)),   # red
    ((124, 58, 237), (245, 243, 255), (221, 214, 254), (209, 213, 219)),  # purple
    ((22, 163, 74), (240, 253, 244), (187, 247, 208), (210, 214, 210)),   # green
    ((30, 58, 138), (239, 246, 255), (191, 219, 254), (203, 213, 225)),   # navy
]

TYRE = (31, 41, 55)
RIM = (203, 213, 225)
SPOKE = (156, 163, 175)
CHROME = (71, 85, 105)
SADDLE = (17, 24, 39)


def s(point):
    """Scale a base-canvas point to the supersampled canvas."""
    return (point[0] * SCALE, point[1] * SCALE)


def tube(draw, a, b, colour, width):
    draw.line([s(a), s(b)], fill=colour, width=width * SCALE, joint="curve")
    for p in (a, b):  # round caps so joints look clean
        r = width * SCALE / 2
        x, y = s(p)
        draw.ellipse([x - r, y - r, x + r, y + r], fill=colour)


def wheel(draw, hub):
    x, y = s(hub)
    r = WHEEL_R * SCALE
    draw.ellipse([x - r, y - r, x + r, y + r], outline=TYRE, width=11 * SCALE)
    rr = (WHEEL_R - 9) * SCALE
    draw.ellipse([x - rr, y - rr, x + rr, y + rr], outline=RIM, width=3 * SCALE)
    for i in range(16):
        angle = i * 3.14159 * 2 / 16
        ex = x + (WHEEL_R - 10) * SCALE * _cos(angle)
        ey = y + (WHEEL_R - 10) * SCALE * _sin(angle)
        draw.line([(x, y), (ex, ey)], fill=SPOKE, width=max(1, int(1.6 * SCALE)))
    h = 9 * SCALE
    draw.ellipse([x - h, y - h, x + h, y + h], fill=CHROME)


def _cos(a):
    import math
    return math.cos(a)


def _sin(a):
    import math
    return math.sin(a)


def draw_bike(image, frame):
    draw = ImageDraw.Draw(image)

    # Soft shadow on the ground.
    shadow = Image.new("RGBA", image.size, (0, 0, 0, 0))
    sd = ImageDraw.Draw(shadow)
    sd.ellipse([s((170, GROUND_Y - 10))[0], s((170, GROUND_Y - 10))[1],
                s((790, GROUND_Y + 14))[0], s((790, GROUND_Y + 14))[1]], fill=(0, 0, 0, 70))
    image.alpha_composite(shadow)
    draw = ImageDraw.Draw(image)

    # Chain between chainring and rear sprocket.
    tube(draw, (BOTTOM_BRACKET[0] - 2, BOTTOM_BRACKET[1] - 30), (REAR_HUB[0], REAR_HUB[1] - 18), CHROME, 4)
    tube(draw, (BOTTOM_BRACKET[0] - 2, BOTTOM_BRACKET[1] + 30), (REAR_HUB[0], REAR_HUB[1] + 18), CHROME, 4)

    wheel(draw, REAR_HUB)
    wheel(draw, FRONT_HUB)

    # Frame.
    tube(draw, REAR_HUB, BOTTOM_BRACKET, frame, 15)          # chainstay
    tube(draw, REAR_HUB, (SEAT_TOP[0] + 4, SEAT_TOP[1] + 12), frame, 11)  # seatstay
    tube(draw, BOTTOM_BRACKET, SEAT_TOP, frame, 18)          # seat tube
    tube(draw, SEAT_TOP, HEAD_TOP, frame, 17)                # top tube
    tube(draw, BOTTOM_BRACKET, HEAD_BOTTOM, frame, 20)       # down tube
    tube(draw, HEAD_TOP, HEAD_BOTTOM, frame, 26)             # head tube
    tube(draw, HEAD_BOTTOM, FRONT_HUB, CHROME, 11)           # fork (left)
    tube(draw, (HEAD_BOTTOM[0] + 14, HEAD_BOTTOM[1]), (FRONT_HUB[0] + 12, FRONT_HUB[1]), CHROME, 11)

    # Chainring and crank.
    cx, cy = s(BOTTOM_BRACKET)
    r = 30 * SCALE
    draw.ellipse([cx - r, cy - r, cx + r, cy + r], outline=CHROME, width=5 * SCALE)
    tube(draw, BOTTOM_BRACKET, (BOTTOM_BRACKET[0] + 44, BOTTOM_BRACKET[1] + 40), CHROME, 9)
    tube(draw, BOTTOM_BRACKET, (BOTTOM_BRACKET[0] - 36, BOTTOM_BRACKET[1] - 40), CHROME, 9)
    tube(draw, (BOTTOM_BRACKET[0] + 30, BOTTOM_BRACKET[1] + 40), (BOTTOM_BRACKET[0] + 58, BOTTOM_BRACKET[1] + 40), SADDLE, 8)
    tube(draw, (BOTTOM_BRACKET[0] - 48, BOTTOM_BRACKET[1] - 40), (BOTTOM_BRACKET[0] - 24, BOTTOM_BRACKET[1] - 40), SADDLE, 8)
    draw.ellipse([cx - 10 * SCALE, cy - 10 * SCALE, cx + 10 * SCALE, cy + 10 * SCALE], fill=CHROME)

    # Saddle, seatpost, stem and handlebars.
    tube(draw, SEAT_TOP, (SEAT_TOP[0] - 8, SEAT_TOP[1] - 30), CHROME, 9)
    tube(draw, (SEAT_TOP[0] - 44, SEAT_TOP[1] - 36), (SEAT_TOP[0] + 20, SEAT_TOP[1] - 36), SADDLE, 16)
    tube(draw, HEAD_TOP, (HEAD_TOP[0] - 10, HEAD_TOP[1] - 36), CHROME, 10)
    tube(draw, (HEAD_TOP[0] - 10, HEAD_TOP[1] - 36), (HEAD_TOP[0] + 28, HEAD_TOP[1] - 40), SADDLE, 10)
    tube(draw, (HEAD_TOP[0] + 28, HEAD_TOP[1] - 40), (HEAD_TOP[0] + 40, HEAD_TOP[1] - 22), SADDLE, 10)


def make_background(top, bottom, ground):
    img = Image.new("RGBA", (OUT_W * SCALE, OUT_H * SCALE), top + (255,))
    draw = ImageDraw.Draw(img)
    for y in range(OUT_H * SCALE):
        t = y / (OUT_H * SCALE)
        colour = tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3))
        draw.line([(0, y), (OUT_W * SCALE, y)], fill=colour + (255,))
    draw.rectangle([0, GROUND_Y * SCALE, OUT_W * SCALE, OUT_H * SCALE], fill=ground + (255,))
    draw.line([(0, GROUND_Y * SCALE), (OUT_W * SCALE, GROUND_Y * SCALE)], fill=(148, 163, 184, 255), width=3 * SCALE)
    return img


def render(index, frame, top, bottom, ground, out_dir):
    canvas = make_background(top, bottom, ground)
    draw_bike(canvas, frame)
    final = canvas.convert("RGB").resize((OUT_W, OUT_H), Image.LANCZOS)
    final.save(out_dir / f"bike{index}.jpg", quality=92, optimize=True)


def main():
    out_dir = Path(__file__).resolve().parent.parent / "resources" / "images"
    out_dir.mkdir(parents=True, exist_ok=True)
    for index, (frame, top, bottom, ground) in enumerate(VARIANTS, start=1):
        render(index, frame, top, bottom, ground, out_dir)
        print(f"wrote bike{index}.jpg")


if __name__ == "__main__":
    main()
