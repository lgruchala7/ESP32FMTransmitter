from PIL import Image, ImageDraw, ImageFont

FONT_SIZE = 16
WIDTH = 10
HEIGHT = 16
OUTPUT_FILE = f"font{FONT_SIZE}.h"

font = ImageFont.truetype("RobotoMono-Medium.ttf", FONT_SIZE)

def char_to_bitmap(c):
    img = Image.new("1", (WIDTH, HEIGHT), 0)
    draw = ImageDraw.Draw(img)

    bbox = draw.textbbox((0, 0), c, font=font)
    w = bbox[2] - bbox[0]

    ascent, descent = font.getmetrics()
    total_height = ascent + descent

    x = (WIDTH - w) // 2

    # Align baseline properly instead of naive centering
    y = (HEIGHT - total_height) // 2

    draw.text((x, y), c, 1, font=font)

    data = []

    # HEIGHT/8 pages (8 px each)
    for page in range(HEIGHT//8):
        for col in range(WIDTH):
            byte = 0
            for bit in range(8):
                ypix = page * 8 + bit
                if img.getpixel((col, ypix)):
                    byte |= (1 << bit)  # LSB = top pixel
            data.append(byte)

    return data


with open(OUTPUT_FILE, "w") as f:
    # Header guard
    f.write(f"#ifndef {OUTPUT_FILE.replace('.', '_').capitalize()}\n")
    f.write(f"#define {OUTPUT_FILE.replace('.', '_').capitalize()}\n\n")

    f.write("#include <stdint.h>\n\n")

    f.write(f"// {WIDTH}x{HEIGHT} font for SH1106 (ASCII 0x20 - 0x7E)\n")
    f.write(f"// Each character: {WIDTH} columns × {HEIGHT//8} pages = {WIDTH*HEIGHT//8} bytes\n")
    f.write("// Format: [char][column bytes...]\n\n")

    f.write(f"const uint8_t {OUTPUT_FILE.split('.')[0]}[95][{WIDTH*HEIGHT//8}] = {{\n")

    for i in range(0x20, 0x7F):
        bitmap = char_to_bitmap(chr(i))

        f.write("    { ")
        f.write(", ".join(f"0x{b:02X}" for b in bitmap))
        f.write(" }},  // '{}' (0x{:02X})\n".format(chr(i), i))

    f.write("};\n\n")
    f.write(f"#endif // {OUTPUT_FILE.replace('.', '_').capitalize()}\n")

print(f"Font header file generated: {OUTPUT_FILE}")