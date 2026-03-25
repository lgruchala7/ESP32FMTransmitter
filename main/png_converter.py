from PIL import Image

INPUT_FILE = "input.png"
OUTPUT_FILE = "output.h"

WIDTH = 16     # expected width
HEIGHT = 24    # expected height (must be multiple of 8)

ARRAY_NAME = "icon_data"


def convert_image_to_sh1106(img):
    img = img.convert("1")  # 1-bit (black & white)

    pixels = img.load()
    data = []

    pages = HEIGHT // 8

    for page in range(pages):
        for x in range(WIDTH):
            byte = 0
            for bit in range(8):
                y = page * 8 + bit
                if pixels[x, y] == 255:  # white pixel = ON
                    byte |= (1 << bit)   # LSB = top pixel
            data.append(byte)

    return data


# Load image
img = Image.open(INPUT_FILE)

# Resize if needed (keeps aspect but forces size)
img = img.resize((WIDTH, HEIGHT), Image.NEAREST)

# Convert
bitmap = convert_image_to_sh1106(img)

# Save to header file
with open(OUTPUT_FILE, "w") as f:
    f.write("#ifndef ICON_H\n#define ICON_H\n\n")
    f.write("#include <stdint.h>\n\n")

    f.write(f"// Generated from {INPUT_FILE}\n")
    f.write(f"const uint8_t {ARRAY_NAME}[{len(bitmap)}] = {{\n    ")

    for i, b in enumerate(bitmap):
        f.write(f"0x{b:02X}")
        if i != len(bitmap) - 1:
            f.write(", ")

        if (i + 1) % WIDTH == 0:
            f.write("\n    ")

    f.write("\n};\n\n#endif\n")

print(f"Converted {INPUT_FILE} -> {OUTPUT_FILE}")