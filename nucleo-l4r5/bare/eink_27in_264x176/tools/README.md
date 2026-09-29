# tools

`bmp_to_eink.py` converts 1-bpp BMP images into the Pervasive Displays 2.7"
frame format and writes the `src/resource.c` / `resource.h` pair:

```bash
cd bare/eink_27in_264x176
python tools/bmp_to_eink.py ../../eink_assets/board_0.bmp \
                            ../../eink_assets/board_1.bmp \
                            --out src/resource.c
```

Output symbols are named after the input files (`board_0.bmp` -> `board_0`).
Add `--prefix` to namespace them.

The panel format is 264x176, 1 bpp, 33 bytes per row with no padding, MSB =
leftmost pixel, 1 = white, 0 = black, top-down (5808 bytes). A 1-bpp BMP
stores the same pixels but pads rows to 4 bytes (33 -> 36) and writes them
bottom-up for a positive `biHeight`; the tool strips the padding and reverses
the rows.

Options:

| flag | use when |
| ---- | -------- |
| `--flip-y` | the converted image renders upside down on the panel |
| `--invert` | the converted image renders as a negative (BMP palette was not index 0 = black) |
| `--prefix p` | prefix the generated symbol names |

`src/resource.c` is generated output - regenerate it rather than editing it by
hand. Pure standard library (no Pillow).
