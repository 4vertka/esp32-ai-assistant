import sys
from PIL import Image, ImageSequence

# usage: python img2oled.py input.gif 32 32 name [max_frames]
src, W, H, name = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4]
max_frames = int(sys.argv[5]) if len(sys.argv) > 5 else 16

def to_page_bytes(img):
    px = img.load()
    out = []
    for page in range(H // 8):
        for x in range(W):
            b = 0
            for bit in range(8):
                if px[x, page * 8 + bit]:
                    b |= 1 << bit
            out.append(b)
    return out

im = Image.open(src)
frames = []
for i, fr in enumerate(ImageSequence.Iterator(im)):
    if i >= max_frames:
        break
    g = fr.convert("RGBA")
    bg = Image.new("RGBA", g.size, (0, 0, 0, 255))   # transparent -> black
    g = Image.alpha_composite(bg, g).convert("L")
    g = g.resize((W, H), Image.LANCZOS)
    g = g.point(lambda p: 255 if p > 128 else 0).convert("1")  # threshold
    frames.append(to_page_bytes(g))

with open("main/bitmaps.h", "w") as f:
    f.write("#pragma once\n#include <stdint.h>\n\n")
    for i, data in enumerate(frames):
        rows = [", ".join(f"0x{b:02X}" for b in data[j:j+16]) for j in range(0, len(data), 16)]
        f.write(f"static const uint8_t {name}_{i}[] = {{\n    " + ",\n    ".join(rows) + "\n};\n\n")
    f.write(f"#define {name.upper()}_FRAMES {len(frames)}\n")
    f.write(f"static const uint8_t *{name}_frames[] = {{ " +
            ", ".join(f"{name}_{i}" for i in range(len(frames))) + " };\n")

print(f"{len(frames)} frames, {len(frames[0])} bytes each")
