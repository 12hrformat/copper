#!/bin/sh
# Prove tools/gen-boot-art.py fails when the boot art is broken, instead of
# assuming it would.
#
#   ./tests/art-gate.sh
#
# Every gate in this repository printed a reassuring line while checking nothing
# at least once: the POSIX check in iso/build.sh inspected zero of the three
# files it named, and HANDOFF.md claimed a pty test covered the account files
# that did not exist. A green run means nothing until a deliberate fault makes
# it red, so this puts each fault back and checks the checker notices.
#
# Works on copies. The real iso/firstboot/boot-art.h is never touched.

set -u

REPO=$(cd "$(dirname "$0")/.." && pwd)
ART="$REPO/iso/firstboot/boot-art.h"
GEN="$REPO/tools/gen-boot-art.py"
WORK=${ART_GATE_ROOT:-/tmp/copper-art-gate}

command -v gcc >/dev/null 2>&1 || { echo "art-gate: no gcc, cannot test" >&2; exit 1; }

pass=0
fail=0

run_case() {
    name=$1; expect=$2; tamper=$3
    sandbox="$WORK/$name"

    rm -rf "$sandbox"
    mkdir -p "$sandbox/tools" "$sandbox/iso/firstboot"
    cp "$GEN" "$sandbox/tools/gen-boot-art.py"
    cp "$ART" "$sandbox/iso/firstboot/boot-art.h"

    python3 - "$sandbox/iso/firstboot/boot-art.h" "$tamper" <<'PY'
import sys, pathlib
p = pathlib.Path(sys.argv[1]); mode = sys.argv[2]
s = p.read_text(encoding='utf-8'); before = s

if mode == 'unicode':
    # One CP437 block character, stored as UTF-8. In an 8x16 byte-indexed font
    # that is three garbage glyphs, not one.
    s = s.replace('@@@@', '\u2588\u2588\u2588\u2588', 1)
elif mode == 'syntax':
    # Unbalanced parens in a macro. Legal C as long as nothing ever expands it,
    # which is why compiling the header standalone does not catch this.
    s = s.replace('#define COPPER_SHIELD_ROWS',
                  '#define COPPER_SHIELD_ROWS (((', 1)
elif mode == 'widen':
    # One art row pushed past the 80-column console, declared width untouched,
    # so the horizontal overflow check itself is what has to fire.
    d = s.index('COPPER_SHIELD[] = {')
    a = s.index('\n    "', d) + 6
    b = s.index('"', a)
    s = s[:a] + s[a:b].ljust(85) + s[b:]
elif mode == 'stale_width':
    s = s.replace('COPPER_WORDMARK_WIDTH = 65',
                  'COPPER_WORDMARK_WIDTH = 68', 1)
elif mode == 'stale_comment':
    s = s.replace('/* 18 rows, widest 65 columns. */',
                  '/* 7 rows, widest 68 columns. */', 1)
elif mode == 'credit':
    s = s.replace('COPPER_SHIELD[] = {',
                  'COPPER_SHIELD[] = {\n'
                  '    "                    MADE BY 12HRFORMAT                 ",', 1)
elif mode == 'clean':
    pass
else:
    raise SystemExit(f'unknown tamper: {mode}')

if s == before and mode != 'clean':
    raise SystemExit(f'TAMPER DID NOT APPLY: {mode} -- the art has changed, so '
                     'this gate no longer tests anything')

p.write_text(s, encoding='utf-8')
PY
    if [ $? -ne 0 ]; then
        printf 'FAIL : %-14s tamper did not apply\n' "$name"
        fail=$((fail + 1))
        return
    fi

    out=$(python3 "$sandbox/tools/gen-boot-art.py" 2>&1)
    rc=$?

    got=clean
    [ "$rc" -ne 0 ] && got=failed

    if [ "$got" = "$expect" ]; then
        printf 'ok   : %-14s exits %s as expected\n' "$name" "$rc"
        pass=$((pass + 1))
    else
        printf 'FAIL : %-14s expected %s, got %s\n' "$name" "$expect" "$got"
        fail=$((fail + 1))
    fi

    # Show the verdict, so a gate that goes red for the wrong reason is visible
    # rather than just a different number.
    printf '%s\n' "$out" | grep '^  FAIL' | head -1 | sed 's/^/       /'
}

echo "boot art: proving the checker fires"
echo

run_case clean          clean   clean
run_case unicode        failed  unicode
run_case syntax         failed  syntax
run_case widen          failed  widen
run_case stale_width    failed  stale_width
run_case stale_comment  failed  stale_comment
run_case credit         failed  credit

echo
if [ $fail -gt 0 ]; then
    echo "$pass passed, $fail failed"
    exit 1
fi
echo "all $pass checks fire as they should"