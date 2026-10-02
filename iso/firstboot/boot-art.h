/*
 * boot-art.h -- the Copper Linux boot art. Hand-drawn. Edit THIS FILE.
 *
 * tools/gen-boot-art.py used to generate this, and the header here used to say
 * "edit that script, not this file, or the next run will overwrite your
 * change". That instruction is why edits made here were lost: the generator
 * did not contain the drawing, so running it replaced the drawing with
 * something else. It now only checks this file and never writes, but the rule
 * stands on its own -- there is no script that reproduces this art, so this
 * file is the only place it exists.
 *
 * Run the checker after every edit:
 *
 *     python3 tools/gen-boot-art.py
 *
 * It will not modify anything. It fails on ASCII violations, on anything that
 * will not compile, on a row wider than 80 columns, and on a declared width
 * that has drifted away from the drawing.
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
 *
 * ASCII only. The 8x16 VGA font is indexed by byte, so one block character
 * stored as UTF-8 is three garbage glyphs, not one.
 */
#ifndef COPPER_BOOT_ART_H
#define COPPER_BOOT_ART_H

/* The shield, as drawn: 35 rows by 77 columns, so it does not wrap on
* the 80x25 VGA console. It is taller than 25 rows, so the bottom ten --
* the point -- are cropped off. Widen a row past 80 and it wraps;
* tools/gen-boot-art.py fails on that. */
static const char *const COPPER_SHIELD[] = {
    "                                      @                                      ",
    "      @                              @@@                               @     ",
    "      @@@                           @@@@@                            @@@     ",
    "       @@@@                         @@@@@                          @@@@      ",
    "       @@@@@@                      @@@@@@@                       @@@@@       ",
    "        @@@@@@@                    @@@@@@@                     @@@@@@        ",
    "         @@@@@@@@                 @@@@@@@@@                  @@@@@@@         ",
    "          @@@@@@@@@              @@@@@@@@@@                @@@@@@@@          ",
    "           @@@@@@@@@@            @@@@@@@@@@@             @@@@@@@@@           ",
    "            @@@@@@@@@@@         @@@@@@@@@@@@@          @@@@@@@@@@            ",
    "             @@@@@@@@@@@@       @@@@@@@@@@@@@        @@@@@@@@@@@@            ",
    "             @@@@@@@@@@@@@@    @@@@@@@@@@@@@@@     @@@@@@@@@@@@@             ",
    "              @@@@@@@@@@@@@@@   @@@@@@@@@@@@@   @@@@@@@@@@@@@@@              ",
    "               @@@@@@@@@@@@@@@@   @@@@@@@@@    @@@@@@@@@@@@@@@               ",
    "                @@@@@@@@@@@@@@@@@   @@@@@    @@@@@@@@@@@@@@@@                ",
    "                 @@@@@@@@@@@@@@@@@    @    @@@@@@@@@@@@@@@@@                 ",
    "                  @@@@@  @@@@@@   @@@@@@@@@   @@@@@@  @@@@@                  ",
    "                  @@@@@    @    @@@@@@@@@@@@@    @    @@@@@                  ",
    "                   @@@@@      @@@@@@@@@@@@@@@@@@      @@@@@@                 ",
    "                   @@@@@@    @@@@@@@@@@ @@@@@@@@@    @@@@@@                  ",
    "                  @@@@@@@    @@@@@@@       @@@@@@    @@@@@@@                 ",
    "                 @@@@@@@@@   @@@@@@                 @@@@@@@@@                ",
    "                 @@@@@@@@@@  @@@@@@                @@@@@@@@@@                ",
    "                  @@@@@@@@@   @@@@@                @@@@@@@@@@                ",
    "                    @@@@@@@@  @@@@@@@      @@@@@  @@@@@@@@@                  ",
    "                     @@@@@@@@  @@@@@@@@@@@@@@@@  @@@@@@@@@                   ",
    "                       @@@@@@   @@@@@@@@@@@@@@   @@@@@@@@                    ",
    "                        @@@@@@     @@@@@@@@     @@@@@@                       ",
    "                          @@@@@                @@@@@                         ",
    "                           @@@@@  @@@@  @@@@  @@@@                           ",
    "                              @@@  @@@@@@@@  @@@@                            ",
    "                               @@@  @@@@@@  @@@                              ",
    "                                 @@  @@@@  @@                                ",
    "                                   @  @@  @                                  ",
    "                                                                             ",
};
/* 35 rows, widest 77 columns. */

/* COPPER LINUX, 18 rows by 65 columns: COPPER on top, a blank row, LINUX
* underneath. Both lines fit 80 columns, which the single 65-column-wide
* earlier attempt did not. */
static const char *const COPPER_WORDMARK[] = {
    "    @@@@@@@@                                                     ",
    "   @@@     @@@                                                   ",
    "  @@@            @@@@    @@@@@@@@  @@@@@@@@   @@@@    @@@@@@@@   ",
    " @@@           @@@  @@@   @@  @@@   @@  @@@ @@@  @@@  @@@  @@@   ",
    " @@@           @@@  @@@   @@  @@@   @@  @@@ @@@@@@@   @@@        ",
    "  @@@      @@@ @@@  @@@   @@  @@@   @@  @@@ @@@       @@@        ",
    "    @@@@@@@@     @@@@     @@@@@@    @@@@@@   @@@@@@  @@@@@       ",
    "                          @@@       @@@                          ",
    "                          @@@       @@@                          ",
    "                         @@@@@     @@@@@                         ",
    "                                                                 ",
    "  @@@@@        @@@                                               ",
    "   @@@                                                           ",
    "   @@@        @@@@  @@@@@@@@   @@@@  @@@@ @@@@@ @@@@@            ",
    "   @@@         @@@   @@@  @@@   @@@  @@@   @@@   @@@             ",
    "   @@@         @@@   @@@  @@@   @@@  @@@     @@@@@               ",
    "   @@@      @  @@@   @@@  @@@   @@@  @@@   @@@   @@@             ",
    "  @@@@@@@@@@@ @@@@@ @@@@ @@@@@  @@@@@@@@  @@@@@ @@@@@            ",
};
/* 18 rows, widest 65 columns. */

#define COPPER_SHIELD_ROWS    (int)(sizeof COPPER_SHIELD / sizeof COPPER_SHIELD[0])
#define COPPER_WORDMARK_ROWS  (int)(sizeof COPPER_WORDMARK / sizeof COPPER_WORDMARK[0])

/* Widest row of each block, used to decide whether it fits before drawing it. */
static const int COPPER_SHIELD_WIDTH   = 77;
static const int COPPER_WORDMARK_WIDTH = 65;

#endif /* COPPER_BOOT_ART_H */
