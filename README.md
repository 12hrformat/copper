<img width="1242" height="788" alt="IMG_20260926_213859" src="https://github.com/user-attachments/assets/ba0b244f-ec8e-403d-b595-1a68c8b81463" />


<h1 align="center">Copper Linux</h1>

<p align="center">
  A daily-driving Linux distro, built from source, by a small team.
</p>

<p align="center">
  <a href="https://copper-linux.github.io/Copper-linux-website/">copper-linux.github.io/Copper-linux-website</a>
</p>

---

Copper is our own distro, not a rebrand of Debian or Arch. We build the
kernel, the libc, and the userland from source, and we write the parts that
make it *Copper* ourselves — the shell, the init system, the first-boot
setup. Arch and Debian are reference material, nothing more.

Copper has a twin: **Vortex**, a cybersecurity-focused distro built by the
same team, sharing the Copper base but aimed at security work instead of
general daily use.

---

## Where things stand

| Part | Status |
|---|---|
| `copper-sh` (shell) | Works. Arrow-key line editing, history, pipes, redirects. |
| Networking | Works — wired only. DHCP on boot, `ping`/`nslookup`/`wget` present. |
| `copper charge` / `copper rollback` | Works. Every charge takes a restore point before it changes anything, so rollback has something to put back even when nothing needed patching. |
| First-boot wizard | Boot animation, then a centred table of questions, then the account. Boots to a `you@copper` prompt. |
| Shell starts in your home | Yes. Lands in `/home/<user>`, not `/`. |
| Text editor | busybox `vi`. `nano` is not built — it needs ncurses. |
| GUI | Not started — planned for later. |
| Base system (kernel, musl, userland) | Building from source, CI green end to end. |
| Bootable ISO | Builds successfully. Boots in a VM. |
| WiFi | **Not supported.** Wired drivers only, no `wpa_supplicant`, and a VM has no wireless NIC anyway. |

---

## What's in this repo

```
Copper Linux
├── kernel          — Linux from kernel.org, our own .config
├── libc            — musl, built from source
├── userland        — coreutils, busybox, grep, sed, tar, ...
├── copper-sh       — our shell
├── copper-init     — our init, lives at /sbin/init
├── copper-firstboot — first-boot setup wizard
├── copper          — copper charge / rollback front end
├── hotfixes.json   — the hotfix database `copper charge` reads
└── copper.iso      — bootable live ISO (VMware / VirtualBox / QEMU)
```

The build pipeline (`iso/build.sh`) is staged — `kernel`, `base`, `tools`,
`copper`, `rootfs`, `initramfs`, `iso`, or `all` — and skips a stage if it's
already built, so re-runs on CI are cheap.

**Kernel:** Linux 6.12.10 LTS, with a trimmed config: ISO9660, overlayfs,
tmpfs, devtmpfs, virtio/e1000/vmxnet3 NICs, ATA/SATA, ext4, ptys. Drivers
are built in, no loadable modules.

**Base:** musl 1.2.5, busybox 1.36.1 (static, with adduser, chpasswd,
mount, hostname), coreutils 9.5, grep 3.11, sed 4.9, findutils 4.9.0,
diffutils 3.10, tar 1.35, gzip 1.13, xz 5.4.6 — all compiled from source
and linked statically against musl.

Run it locally with:

```sh
sudo bash iso/build.sh
```

or check the Actions logs for a CI run. A finished build uploads
`copper.iso` (~57 MB) as an Actions artifact.

---

## copper-init

Our own PID 1, not systemd. It mounts the basics, sets the hostname, runs
the first-boot wizard once, then keeps a `copper-sh` login shell alive on
tty1. Lives at `/sbin/init`.

The live initramfs (`iso/live/init`) finds the boot medium and builds a
writable overlay — read-only ISO root, tmpfs on top — before handing off
to `copper-init`. That overlay is also the mechanism persistence will use
once it's built out.

---

## copper-firstboot

What you see the first time the ISO boots:

1. **The shield**, drawn a line at a time at about 100ms per line. Press any
   key to skip to the end of it.
2. **The wordmark**, `COPPER LINUX`, centred. Held for a moment.
3. **A table of questions**, centred, with a live status line underneath.
   Every question is asked up front, then the answers are applied.

```
+----------------------------------------------------------------+
|   Copper Linux  -  first boot setup                            |
+----------------------------------------------------------------+
|   Your name           Zaphod                                    |
|   Username            zaphod                                    |
|   Hostname            attic-box                                 |
|   Root password      ********                                   |
|   Your password      ********                                   |
|   Timezone            UTC                                       |
+----------------------------------------------------------------+
|What should Copper call you?                                     |
+----------------------------------------------------------------+
```

Each row shows the answer once there is one, and a hint until then — the
hint is a default you get by pressing Enter, not text you have to clear.
Typing overwrites it. Arrow keys, Backspace and Ctrl-U all work; passwords
are echoed as `*` and asked for twice.

The table is only used when the terminal is big enough for it and is a real
terminal at all: at least 46 columns by 14 rows, and both stdin and stdout
have to be a tty. Otherwise it falls back to one plain question at a time,
with no animation and no escape sequences. That is deliberate — the VGA
console is 80x25 but a serial console is whatever someone chose, and
clearing someone's scrollback to show them a progress animation is rude.

Box drawing is `+`, `-` and `|` only. The VGA console font has no box-drawing
characters or em dashes, so anything prettier arrives as a row of blanks.

### The art

The shield and the wordmark are generated, not hand-typed into the source:

```sh
python3 tools/gen-boot-art.py     # writes iso/firstboot/boot-art.h
```

That matters more than it sounds. A single dropped `@` in a block of ASCII is
invisible in a diff and turns the logo into a smudge, so the art has one
source and one command that regenerates it.

The wordmark is a compact 7-row 5x7 block font rather than the tall
display one. The display cut is 22 rows by 194 columns, which cannot fit an
80-column console — it wraps into nonsense — so the block font is what
actually renders on a VGA terminal.

### Answers are validated

Everything that ends up in a shell command is checked before it is used.
The hostname and the timezone both go into `system()` calls:

```sh
echo <hostname> > /etc/hostname
ln -sf /usr/share/zoneinfo/<timezone> /etc/localtime
```

so an unvalidated answer would be shell, not a hostname. Every field has a
validator and a rejection message, and the same rules apply on both the
table and the plain path.

---

## copper-sh

A small POSIX-style shell, written in C. Mostly builtins for now, so it can
do useful things before the rest of the userland is on the system.
Anything not built in falls through to `execvp` and runs from `$PATH`.

| Command | Does |
|---|---|
| `cd`, `pwd` | navigate, print working directory |
| `ls`, `cat`, `head`, `tail`, `wc`, `grep` | inspect files |
| `echo`, `tee` | output |
| `cp`, `mv`, `rm`, `ln`, `mkdir`, `rmdir`, `touch`, `chmod` | file operations |
| `whoami`, `id`, `hostname`, `uname`, `date` | system info |
| `history`, `type`, `which`, `env` | shell/session info |
| `sleep`, `true`, `false`, `clear`, `exit` / `quit` | misc |

Also handles `#` comments, single and double quotes, backslash escapes,
pipes (`|`), and redirects (`<`, `>`, `>>`). Ctrl-C kills the running
command, not the shell.

Build it:

```sh
make
./copper-sh
```

Install system-wide:

```sh
make install    # /usr/local/bin/copper-sh
```

---

## copper charge — hotfixes without a reinstall

The idea: a bug is found, someone writes the fix down in `hotfixes.json`, and
anyone running Copper can pull that fix onto their live system in place. No
ISO rebuild, no reinstall, and if a fix turns out to be wrong there is a way
back.

```sh
copper charge                  # take a restore point, then apply what applies
copper charge --status         # say what would change, change nothing
copper charge --backup         # take the restore point and stop
copper charge --dump-entries   # print every entry the parser can see
copper rollback                # list restore points and what is in them
copper rollback --latest       # put the most recent restore point back
copper rollback <snapshot>     # put a named one back
```

Applying anything needs root. `--help` and `--dump-entries` deliberately do not:
they change nothing, and they are the first thing to reach for when working out
why a charge did nothing.

Every action is appended to `/var/log/copper-charge.log`.

### Writing a hotfix

`hotfixes.json` at the repo root. One entry per bug:

```json
{
  "hotfixes": [
    {
      "id": "demo-banner",
      "file": "etc/copper/demo.txt",
      "fail_code": "THIS LINE IS BROKEN",
      "new_code": "THIS LINE HAS BEEN FIXED BY COPPER CHARGE",
      "description": "Demo: proves charge can find, back up and patch a file"
    }
  ]
}
```

| Field | Meaning |
|---|---|
| `id` | Short name for this fix, used in the log |
| `file` | Path on the **live system**, relative to `/` |
| `fail_code` | The exact text to look for. Its absence means "already fixed" |
| `new_code` | What to put in its place |
| `description` | One line, shown when the fix is applied |

### How it decides

Every run, in this order:

1. **Take a restore point.** Copy every file the database refers to that is
   actually present on this system into a new snapshot directory, *before
   anything is modified*. Whether or not anything then turns out to need
   fixing.
2. **For each entry**, look for `fail_code` as a literal string in the file.
3. **Replace** every occurrence of it with `new_code`, and say so.

| Situation | What happens |
|---|---|
| `fail_code` present | Backed up in step 1, then replaced |
| `fail_code` absent | `skipping <id> - <file> does not need it (already fixed?)` |
| File not installed | `skipping <id> - <file> is not on this system` |
| Entry has no `fail_code` | Skipped, and the reason is said |

Skips are reported out loud rather than passed over in silence. A hotfix
pointing at a file this system does not have is the one case you cannot work
out for yourself from the output, and it looks identical on screen to a hotfix
that applied.

That is what makes `copper charge` safe to run twice, or on a machine that
already has the fix.

There is no automatic failure detection. A human maintainer writes the entry,
and `copper charge` applies the text edits. Anything subtler than a literal
string swap does not belong in this format.

### Where it gets the database

`/etc/copper/config`:

```sh
HOTFIX_URL="https://raw.githubusercontent.com/Copper-linux/copper/main/hotfixes.json"
BACKUP_DIR="/var/backups/copper"
LOG_FILE="/var/log/copper-charge.log"
```

Point `HOTFIX_URL` at a fork or branch to test someone else's fixes.

**If `/etc/copper/hotfixes.json` exists it is used and the network is never
touched.** The ISO ships one so `copper charge` is testable with no network
at all. Delete that file to go back to always fetching.

### Rollback

A snapshot is a directory under `/var/backups/copper/`, named for when it was
taken, with a copy of each file and a `MANIFEST` naming what those copies came
from:

```
/var/backups/copper/2026-10-02_16-30-12/
    MANIFEST              <- one absolute path per line
    etc_copper_demo.txt
```

`copper rollback` with no arguments lists them and what is in each.
`copper rollback --latest` restores the newest; `copper rollback <name>`
restores a specific one. Restoring copies the current file aside into a
`pre-rollback-*` directory first, so a rollback can itself be undone.

**The snapshot is taken before anything is modified, not at the moment a fix
lands.** This is the part that matters. The first version of this copied a file
only when it patched it, which meant a machine where nothing needed patching
ended up with no backups at all — so `copper rollback` had nothing to restore
and could only say so, which is the exact question it exists to answer, asked
too late. A snapshot taken up front exists even when the charge did nothing,
because "put these files back the way they were at 16:30" is the thing you
actually want after a bad charge.

Snapshots are kept indefinitely. There is no pruning; on a live system the
volume is a few files per charge.

### Notes and limits

- The live system ships busybox `wget`, not `curl`. `copper charge` prefers
  `curl` when it is present and falls back to `wget` otherwise.
- The rootfs carries no CA bundle, so `wget` runs with
  `--no-check-certificate`. That is fine for fetching a JSON file from a
  known repo over a link you already trust; it is **not** fine for anything
  security-sensitive.
- All three tools are plain busybox `sh`. No python on the live system. The
  build checks that they parse as POSIX `sh` and not just as bash, since
  busybox `ash` is what will run them.
- The JSON parser is hand-written `awk`. It is checked at build time by
  comparing the entries it produces against the entries the file declares —
  see below.
- `fail_code` must not contain `|`. It is used as the `sed` delimiter.

### Two failure modes worth knowing about

**The parser only reading the last entry.** The original JSON splitter
buffered from the outermost `{`, which is the object *containing* `"hotfixes":
[ ... ]`, so it concatenated the whole file into one line. Every field was
then pulled out of that line with a greedy `sed`, which returns the **last**
match — so every field came back as the last entry's value and every entry but
the last was invisible, with no error and no change. The shipped database has
exactly one entry, and one entry always works, so it survived. It is now split
per object, keying off each object's own keys.

`iso/assert-hotfix-db.sh` exists to stop that coming back. It runs the real
parser and requires one output line per `fail_code` key in the file, no empty
field, and no value containing a tab. It runs in CI.

**An apostrophe inside a single-quoted `awk` program.** The parser is written
as `awk '...'`, so an apostrophe in a comment inside that body closes the shell
string early and the rest of the awk is handed to the shell to execute. It
fails as `buf[depth]: not found`, which points at nothing useful. That is why
the `awk` block in `copper-charge.sh` has a comment about it, and why the
parse gate exists.

---

## Repo layout

```
src/main.c            shell loop, prompt, tokenizer, process launching
src/builtins.c        builtin commands + command table
src/builtins.h        interface between the two
hotfixes.json         hotfix database read by `copper charge`
iso/build.sh          from-source distro build
iso/copper.sh         installs as /usr/bin/copper
iso/copper-charge.sh  installs as /usr/bin/copper-charge
iso/copper-rollback.sh installs as /usr/bin/copper-rollback
iso/assert-hotfix-db.sh  build gate: the database must survive the parser
iso/live/init         live initramfs
iso/boot/             GRUB config
iso/src-init/         copper-init source
iso/firstboot/        copper-firstboot source
iso/firstboot/boot-art.h   generated shield + wordmark
tools/gen-boot-art.py regenerates the art above
iso/rootfs-overlay/   default /etc for the rootfs
tests/smoke.sh        copper-sh sanity checks
tests/charge.sh       charge/rollback cycle, end to end, in a sandbox
Makefile
HANDOFF.md            current state, next-person notes
PR.md                 PR notes/template
```

### Tests

```sh
make -s && ./tests/smoke.sh     # the shell
sudo ./tests/charge.sh          # the hotfix tools, end to end
```

`tests/charge.sh` runs the three real scripts against a sandbox in `/tmp`,
pointed there by `COPPER_CONFIG`. It covers the whole cycle including the case
that started the rewrite: a charge that applies nothing must still leave a
rollback something to restore. Exits 77 without root.

---

## What's next

- Persistence — answers that survive a reboot rather than applying for the
  live session only
- `copper charge --update` to pull a newer hotfix database, and a hotfix
  type that can create a file instead of editing an existing one
- Skipping the boot animation from the kernel command line
- WiFi, if we decide a VM-testable target is possible at all
- GUI — no timeline yet, comes after the base system is solid
- `~/.copperrc` init file for the shell
- Shell history persisted to disk
- Tab completion in `copper-sh`

---

## Contributing

Small team, early stage, things will be rough. Read `HANDOFF.md` first.
Issues and pull requests welcome.

## License

Copper's own code — `copper-sh`, `copper-init`, `copper-firstboot`, and the
build scripts — is MIT licensed. See `LICENSE`.

The rest of the system is source-built from other projects, each under its
own license:

| Component | License |
|---|---|
| Linux kernel | GPLv2 |
| busybox | GPLv2 |
| coreutils, findutils, tar, gzip, sed, grep | GPLv3 |
| musl | MIT |
| Copper source files (`copper-sh`, `copper-init`, `copper-firstboot`, `copper` tools, build scripts) | MIT |

Building or distributing the full ISO means complying with all of the
above, not just Copper's own MIT terms.

## Contact

- Email: [12hrformat@proton.me](mailto:12hrformat@proton.me)
- Instagram: [@mommy_said_im_special](https://instagram.com/mommy_said_im_special)
- Or simply tag us in [Discussions](https://github.com/Copper-linux/copper/discussions)
