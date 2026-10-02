#!/usr/bin/env python3
"""
Emit iso/firstboot/boot-art.h.

The art is generated into a C header rather than hand-pasted into one, because
the failure mode of hand-pasting 40 lines of ASCII art is that a single dropped
character is invisible in a diff and ruins the art on the next boot.

The shield is used exactly as supplied: 40 rows, widest 77, which fits the 80x25
VGA text console the kernel gives us.

The COPPER LINUX wordmark is NOT used as supplied. As supplied it is 22 rows by
194 columns; there is no terminal Copper boots on that is 194 wide, and art that
does not fit wraps into an unreadable smear rather than shrinking. So it is drawn
from a compact block font instead: 7 rows, 61 columns, every letter the same
height. That is a deliberate substitution, not a copy.
"""
import pathlib, sys

# Derived from this file's own location rather than hardcoded, so the generator
# works on anybody's checkout without being edited first.
REPO = pathlib.Path(__file__).resolve().parent.parent
OUT = REPO / "iso" / "firstboot" / "boot-art.h"

# --------------------------------------------------------------- the shield
LOGO = r"""
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@-@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@@@@@*%@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@*-*@@@@@@@@@@@@@@@@@@@@@@@@@@@@@#@@@@@
@@@@@@==%@@@@@@@@@@@@@@@@@@@@@@@@@@@%---%@@@@@@@@@@@@@@@@@@@@@@@@@@@@+=%@@@@@
@@@@@@@=-=%@@@@@@@@@@@@@@@@@@@@@@@@@+-==+@@@@@@@@@@@@@@@@@@@@@@@@@@+--%@@@@@@
@@@@@@@%---=%@@@@@@@@@@@@@@@@@@@@@@#-===-#@@@@@@@@@@@@@@@@@@@@@@@+---%@@@@@@@
@@@@@@@@%--=-=%@@@@@@@@@@@@@@@@@@@@-======@@@@@@@@@@@@@@@@@@@@@+----%@@@@@@@@
@@@@@@@@@%-===--%@@@@@@@@@@@@@@@@@+-=====-*@@@@@@@@@@@@@@@@@@+--==-#@@@@@@@@@
@@@@@@@@@@#-======%@@@@@@@@@@@@@@%-========@@@@@@@@@@@@@@@@+-====-+@@@@@@@@@@
@@@@@@@@@@@*-=======%@@@@@@@@@@@@=-===++==-+@@@@@@@@@@@@@+-=====-+@@@@@@@@@@@
@@@@@@@@@@@@+-=======-%@@@@@@@@@%-===+++===-%@@@@@@@@@@+-======-=@@@@@@@@@@@@
@@@@@@@@@@@@@=-=========#@@@@@@@-====+++=====@@@@@@@@+-=========%@@@@@@@@@@@@
@@@@@@@@@@@@@%--==========%@@@@#-===========-%@@@@@+-=========-%@@@@@@@@@@@@@
@@@@@@@@@@@@@@%--==========-%@@@=--========-=@@@%+-==========-%@@@@@@@@@@@@@@
@@@@@@@@@@@@@@@#--============#@@@*--====-*@@@@+-===========-#@@@@@@@@@@@@@@@
@@@@@@@@@@@@@@@@*-===#-========-%@@@*---*@@@@=-=======-+===-+@@@@@@@@@@@@@@@@
@@@@@@@@@@@@@@@@@+-==*%*-======--%@@@@%@@@@%---=====-*%#-=-+@@@@@@@@@@@@@@@@@
@@@@@@@@@@@@@@@@@@=-=-#@@#----*@@@%+-----=%@@@*----*@@%-=-=@@@@@@@@@@@@@@@@@@
@@@@@@@@@@@@@@@@@@%--==@@@@%@@@@*--========-#@@@%%@@@@----%@@@@@@@@@@@@@@@@@@
@@@@@@@@@@@@@@@@@@@#-==*@@@@@#=-======--=====-+%@@@@@*-=-#@@@@@@@@@@@@@@@@@@@
@@@@@@@@@@@@@@@@@@@====-#@@#--=====-=#@%+--====--*@@#-==-*@@@@@@@@@@@@@@@@@@@
@@@@@@@@@@@@@@@@@@*-=====@@@-====-#@@@@@@@%*****#@@@=====-#@@@@@@@@@@@@@@@@@@
@@@@@@@@@@@@@@@@@#-======+@@+-===+@@@@@@@@@@@@@@@@@+-=====-%@@@@@@@@@@@@@@@@@
@@@@@@@@@@@@@@@@@=-=======%@%-==-+@@@@@@@@@@@@@@@@%-======-*@@@@@@@@@@@@@@@@@
@@@@@@@@@@@@@@@@@@#-======-@@+-=-+@@@@@@@@@@@@@@@%-======-%@@@@@@@@@@@@@@@@@@
@@@@@@@@@@@@@@@@@@@@+-=====*@#-===+%@@@@@@*+++++%+-====-*@@@@@@@@@@@@@@@@@@@@
@@@@@@@@@@@@@@@@@@@@@%-====-#@--=====+%*=-==--=%#-=====%@@@@@@@@@@@@@@@@@@@@@
@@@@@@@@@@@@@@@@@@@@@@+-====@@#--=========-+@@%-===-*@@@@@@@@@@@@@@@@@@@@@@@
@@@@@@@@@@@@@@@@@@@@@@@@%=-==*@@@@*--===-=#@@@@*-===%@@@@@@@@@@@@@@@@@@@@@@@@
@@@@@@@@@@@@@@@@@@@@@@@@@@*---%@%=%@%---%@@=%@#---#@@@@@@@@@@@@@@@@@@@@@@@@@@
@@@@@@@@@@@@@@@@@@@@@@@@@@@%=-=@@+--*%@@#--+@@=-=@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@-*@@=-=======@@+-#@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@-@@%-=====-%@%+@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@*-====*@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@+====@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@%---%@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@#-#@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@=@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
""".strip("\n").split("\n")

# --------------------------------------------------------------- the font
# 5 wide, 7 tall. '#' rather than the supplied ':' and 'o' -- one glyph, high
# contrast, and it renders on every console, including a raw VGA text mode.
FONT = {
    "C": [" ####", "#    ", "#    ", "#    ", "#    ", "#    ", " ####"],
    "O": [" ####", "#   #", "#   #", "#   #", "#   #", "#   #", " ####"],
    "P": [" ####", "#   #", "#   #", " ####", "#    ", "#    ", "#    "],
    "E": [" ####", "#    ", "#    ", " ### ", "#    ", "#    ", " ####"],
    "R": [" ####", "#   #", "#   #", " ####", "#  # ", "#   #", "#   #"],
    "L": ["#    ", "#    ", "#    ", "#    ", "#    ", "#    ", " ####"],
    "I": [" #####", "  #  ", "  #  ", "  #  ", "  #  ", "  #  ", " #####"],
    "N": ["#   #", "##  #", "# # #", "#  ##", "#   #", "#   #", "#   #"],
    "U": ["#   #", "#   #", "#   #", "#   #", "#   #", "#   #", " ####"],
    "X": ["#   #", " # # ", "  #  ", "  #  ", "  #  ", " # # ", "#   #"],
}


def wordmark(text="COPPER LINUX"):
    rows = ["" for _ in range(7)]
    for word_i, word in enumerate(text.split(" ")):
        if word_i:
            for i in range(7):
                rows[i] += "   "          # three spaces between words
        for ch_i, ch in enumerate(word):
            glyph = FONT[ch]
            for i in range(7):
                rows[i] += glyph[i] + (" " if ch_i < len(word) - 1 else "")
    return rows


WORDMARK = wordmark()


def cesc(s):
    out = []
    for c in s:
        if c == '"':  out.append('\\"')
        elif c == "\\": out.append("\\\\")
        else: out.append(c)
    return "".join(out)


def emit(name, rows, doc):
    widest = max(len(r) for r in rows)
    lines = [f"/* {doc} */",
             f"static const char *const {name}[] = {{"]
    for r in rows:
        lines.append(f'    "{cesc(r)}",')
    lines.append("};")
    lines.append(f"/* {len(rows)} rows, widest {widest} columns. */")
    return "\n".join(lines), widest


def main():
    a, aw = emit("COPPER_SHIELD", LOGO,
                 "The shield, exactly as supplied: 40 rows by 77 columns, so it\n"
                 " * fits the 80x25 VGA console without wrapping.")
    b, bw = emit("COPPER_WORDMARK", WORDMARK,
                 "COPPER LINUX in a 5x7 block font. NOT the supplied 194-column\n"
                 " * version -- see the note at the top of this file.")

    body = f"""/*
 * boot-art.h -- the Copper Linux boot art. Generated by tools/gen-boot-art.py;
 * edit that script, not this file, or the next run will overwrite your change.
 *
 * Two rules shaped this, both learned the hard way:
 *
 *  1. The VGA text console is 80x25 and that is what the kernel gives us no
 *     matter how large the emulator window is. Anything wider does not "fit
 *     small", it WRAPS, and wrapped ASCII art is unreadable. copper-firstboot
 *     measures the terminal and refuses to draw a block that will not fit.
 *
 *  2. On a serial console, or any tty that is not a real terminal, none of the
 *     art runs at all and the wizard falls back to plain prompts. Somebody
 *     reading a boot over serial should not have their scrollback cleared by a
 *     progress animation.
 */
#ifndef COPPER_BOOT_ART_H
#define COPPER_BOOT_ART_H

{a}

{b}

#define COPPER_SHIELD_ROWS    (int)(sizeof COPPER_SHIELD / sizeof COPPER_SHIELD[0])
#define COPPER_WORDMARK_ROWS  (int)(sizeof COPPER_WORDMARK / sizeof COPPER_WORDMARK[0])

/* Widest row of each block, used to decide whether it fits before drawing it. */
static const int COPPER_SHIELD_WIDTH   = {aw};
static const int COPPER_WORDMARK_WIDTH = {bw};

#endif /* COPPER_BOOT_ART_H */
"""
    OUT.write_text(body)
    print(f"wrote {OUT}")
    print(f"  shield   {len(LOGO)} rows x {aw} cols")
    print(f"  wordmark {len(WORDMARK)} rows x {bw} cols")
    print()
    print("the wordmark as it will appear:")
    for r in WORDMARK:
        print("  " + r)


if __name__ == "__main__":
    main()