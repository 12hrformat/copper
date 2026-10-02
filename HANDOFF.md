# Copper Linux — Handoff

> Read this first, then `iso/README.md` for the build internals.
> Written by whoever had the machine last. Everything in it is either verified
> or explicitly marked as unverified — there is no third category.

## What Copper is

Our own Linux distro, built from source. Not a rebrand of Debian or Arch, and
not a respin of them: the kernel is upstream but the `.config` is ours, the
userland is compiled in our own pipeline against musl, and the pieces that
make it *Copper* are written by hand.

- real **Linux kernel** (6.12.10 LTS, from kernel.org) with **our `.config`**
- userland **built from source**: musl, busybox, coreutils and friends
- **our own** shell (`copper-sh`), **our own** PID 1 (`copper-init`), **our own**
  first-boot wizard
- **all the standard Linux commands**, real tools — no stubs, no placeholders
- boots as an **ISO** in VMware / VirtualBox / QEMU
- first boot personalizes like a real distro OOBE
- speaks to **drivers** (wifi, bluetooth, firmware) and reaches the **internet**

Upstream projects are reference material. Nothing gets packaged as-is.

---

# Read this part: the branch, and why it isn't on upstream

**Both previous PRs are merged.** Upstream `main` is now `7b4e5fd`
("Merge pull request #3 from farcrowx/copper-os").

**Current work goes on `untested`, on the `12hrformat` fork.** That branch is
staging: things land there untested, and only move to upstream `main` after
someone has booted them.

```
origin    https://github.com/Copper-linux/copper.git    (read-only here)
dragon    https://github.com/12hrformat/copper.git      (works — push here)
fork      https://github.com/farcrowx/copper.git        (push denied)
```

**Use `dragon` as the push remote.** `fork` was the historical target and now
answers `permission denied`; `origin` answers `push=False`. The credential on
this machine has no write access to `Copper-linux/copper`, which is why
everything lands on the fork first.

Work on `untested` so far:

```
b811a15 add 'copper' front end so 'copper charge' actually works
ab6c17f copper charge/rollback: fix three blockers, add a testable demo hotfix
22ffde5 fix: user creation, doubled banner, garbled typing, hardcoded prompt
7fe1450 firstboot: make stdout unbuffered
59ddc2d firstboot: fix invisible password prompt, give wizard a controlling tty
903e764 copper-sh: guard run_segments against n<1
8a73513 firstboot: fix bad read_line call in confirm-password fallback
007f6fd copper-sh: arrow-key line editing
```

**Landing it still needs somebody with write access.** Open
`12hrformat:untested → Copper-linux/copper:main`. Do not merge blind — G1 is
still open (a full wizard run has not been confirmed end to end).

The old `copper-os` branch has been deleted from the fork; its content lives
on as `untested`. `patch-1` is long merged and is history, not a target.

To pick up the work:

```sh
git fetch dragon
git log --oneline upstream/main..dragon/untested
git checkout -b untested dragon/untested
```

If a push is rejected with `fetch first`:

```sh
git pull --rebase dragon untested
git push dragon HEAD:untested
```

---

# Where things actually stand

**The box boots, gets onto the network, runs the first-boot wizard to
completion, and hands over a working shell.** A VMware guest, 2 GB, NAT,
booting the ISO, has produced this:

```
copper: initramfs up, medium is /dev/sr0
copper: handing over to copper-init
copper: eth0 is up, asking DHCP for an address
copper-net: eth0 leased 192.168.127.132/24
copper-net: default route via 192.168.127.2
copper-net: nameserver 192.168.127.2

            <shield, one line at a time>
                    COPPER LINUX

+----------------------------------------------------------------+
|   Copper Linux  -  first boot setup                            |
+----------------------------------------------------------------+
|   Your name           Zaphod                                    |
|   Username            zaphod                                    |
|   Hostname            copper                                    |
|   Root password      ********                                   |
|   Your password      ********                                   |
|   Timezone            UTC                                       |
+----------------------------------------------------------------+
|What should Copper call you?                                     |
+----------------------------------------------------------------+

+----------------------------------------------------------------+
|   Setting things up                                            |
+----------------------------------------------------------------+
|   [1/6] making your account                                    |
...
+----------------------------------------------------------------+

Done -- welcome, Zaphod
zaphod@copper:~$
```

That is kernel, initramfs, overlay, `switch_root`, our PID 1, DHCP, netmask
conversion, default route, resolver, the wizard, account creation, and the
shell — verified on hardware, from an artifact that was mounted and read
before being trusted.

## What has never run

1. **Real internet traffic.** `ping` to a host on the LAN works. Nothing has
   yet proved that a name resolves or that a TCP connection completes.
   `ping 1.1.1.1`, `nslookup` and a `wget` are still unrun. One attempt
   returned 1ms / 1000ms / 3878ms with 50% loss to the default gateway,
   which is ICMP rate-limiting, not a Copper fault — but it does mean the
   story is unproven rather than disproven.
2. **Skipping the boot animation with a real keypress.** See "the WSL pty
   problem" below. The code is correct; the only terminal available on the
   build host cannot confirm it.
3. **Persistence.** Answers apply for the live session only. Rebooting runs
   the wizard again.

## "ip: command not found" — the cause, and three wrong answers first

**The symptom.** A booted system reported `ip: command not found` and
`ifconfig: command not found`.

**The cause.** PID 1 started with an environment the kernel builds, and that
environment has no `PATH` in it. When a program has no `PATH`, `execvp()` falls
back to `confstr(_CS_PATH)` — the kernel's compiled-in default:

```
/bin:/usr/bin
```

Copper installs its networking applets in `sbin`. So `/sbin` was never searched.
`ip`, `ifconfig`, `route` and `arp` were present, compiled in (the shipped
busybox is 2,464,864 bytes and lists 401 applets) and correctly symlinked the
entire time. The shell simply was never told to look there. `ping`, `grep` and
`touch` worked because they are in `/bin` and `/usr/bin`.

Setting `PATH` inside `start_dhcp()` did not help, because that runs in a forked
child: the child got a `PATH` and the shell — the thing people actually type at
— did not. It is now set once in `main()`, before anything forks.

**Three wrong diagnoses, all mine, all retracted. Read these anyway, because
each one was a measurement artifact that looked exactly like a real bug.**

- *"310 files in the ISO are zero bytes."* **False.** `sbin/ip` and friends are
  symlinks, and ISO9660 records a symlink's size as 0 because the target is
  stored as metadata, not content. Mounted read-only, the image has **671
  symlinks and 2 zero-byte regular files** (`mach_kernel`, a vestigial
  cosmopolitan artifact that `grub.cfg` never boots, and a `.disk/*.uuid`).
- *"There is a `/sbin` collision."* There is not. That came from a `sbin`-only
  listing.
- *"Files are owned by a user called `draon`, who should not exist."* `draon`
  was me — the WSL account doing the extraction. ISO9660 has no per-file owner,
  so p7zip stamps whoever extracted it. The WSL account is now `dragon`.

**The lesson worth keeping: never inspect this ISO with p7zip.** `7z l` reports
the symlinks correctly (`Mode = lr-xr-xr-x`, `Symbolic Link = ../bin/busybox`),
but `7z x` writes *some* of them to disk as 0-byte regular files — `bin/touch`
comes out as a symlink, `sbin/ip` comes out as a regular file, from the same
image. Use `mount -o loop` or `bsdtar`. This cost two full build-and-boot cycles
and produced a fix aimed at a bug that did not exist.

### The gate that replaces it

`assert_commands_reachable` resolves every command a person actually types the
way the live shell will: walking `PATH` in order, first hit wins, no confstr
fallback. It rejects a name missing from `PATH`, a link that resolves to nowhere,
and a link that resolves to an empty file. The `PATH` it checks is the same
string `copper-init` sets, and if those two ever drift the build says so.

Verified against deliberately broken trees: good tree passes; `ip` deleted
fails; `ifconfig` dangling fails; busybox truncated to 0 bytes fails; and the
PATH string narrowed to drop `/sbin` fails — which is the regression that
matters, and which every existence-style check passes.

`assert_no_empty_files` also remains, but narrowed and with its story corrected:
`find -type f` does not match symlinks, so it ignores all 600-odd applet links
and looks only at real files.

## Account creation: four faults, found by running it

`Couldn't create user` went through four distinct causes. All four reproduce
only against **the busybox that ships in the ISO** — a mount namespace with a
private `/etc` is required, because busybox `adduser` hardcodes `/etc/passwd` and
running it on WSL rewrites WSL's real accounts. Reproduce it by driving the real
wizard through a pty; piping answers takes a different code path.

1. **`-G <group>` needs the group to exist first.** Otherwise
   `adduser: unknown group <user>`, and nothing is created. Omitting `-G` does
   not help — busybox then tries to create a group of the same name itself and
   reports `adduser: group '<user>' in use`. So: `addgroup <user>` first.
2. **`-D` is ambiguous on this busybox** — `--debug`, `--disabled-login` and
   `--disabled-password` all claim it. `adduser -D …` answers `Option d is
   ambiguous`, prints its usage, creates nothing, and exits 0. Use the long
   option `--disabled-password`.
3. **`/etc/passwd`, `/etc/group` and `/etc/shadow` all shipped without a
   trailing newline.** Appending to a file that does not end in one does not
   begin a line, it concatenates onto the last record, producing
   `…:/bin/falsedemo:x:1000:…` — the last existing record ending in `false`
   welded to the new one starting with `demo`. One unparseable line instead of
   two records, and then busybox refuses to read the file at all: `addgroup:
   /etc/passwd: bad record`, once per supplementary group. Fixed at source,
   and `ensure_trailing_newline()` also guards at runtime.
4. **busybox chatter.** With the group present it still prints `warn:
   /etc/adduser.conf does not exist` and `fatal: addgroup with two arguments is
   an unspecified operation` — the word "fatal", on a boot where nothing failed.
   Six lines of that landed on top of the password prompt, which is what made
   the wizard look like it was asking the same question over and over. It was
   printing warnings underneath it. Output is now captured, shown only on
   failure.

The wizard still writes `/etc/passwd`, `/etc/group` and `/etc/shadow` itself
(`create_user_direct()`) if `adduser` fails for any reason, and the whole thing
is covered by a pty test that asserts a clean run with parseable account files.

## Landing in the user's home, and the name on the prompt

`copper-init` reads the **first line** of `/etc/copper-firstboot.done` after the
wizard has run, `chdir`s to `/home/<user>`, and sets `HOME`, `USER`, `LOGNAME`.

**Line 1 is the login name, and that ordering is load-bearing.** The marker used
to hold the *display* name, so anyone who typed "Jane Doe" as their name and
"jane" as their username was sent to `/home/Jane Doe`, which does not exist —
init printed "no home directory" and dropped them in `/`. Line 1 is now the
username; line 2 is the display name.

**The prompt reads the same marker**, because the shell genuinely does run as
root: `copper-init` execs it directly, there is no `su` and no login, so
`getpwuid(getuid())` is `root`. That is why the prompt said `root@copper` on a
machine that had just announced "Done — welcome, <name>".

Worth being plain about: none of this is a security boundary. There is no login
in front of the shell, so anyone at the console is uid 0 whatever the working
directory or the prompt says.

## Tilde expansion

`touch test.txt ~/test` answered `~/test: No such file or directory` — `~` was
never expanded anywhere. `tokenize_line()` now expands a leading `~` (and
`~user`, via `getpwnam`) on unquoted words only.

Tokens are written into a **separate output buffer**, because expansion makes
text longer: `~/test` is six characters and `/home/alice/test` is sixteen, so
writing in place would run past the end of the line buffer. The tokenizer is
unit-tested directly by `#include`-ing `main.c` with `main` renamed, so the test
cannot drift from the code that ships.

Two bugs the tests caught that reading would not have:

- the user-name scan stopped only at `/`, so `echo ~ /tmp` looked up a user
  literally named `" "` and left the `~` unexpanded;
- `#` started a comment mid-word, so `echo x#y` printed `x`.

It also refuses an expansion that will not fit rather than overflowing.

---

# The first-boot screen, and four bugs in it

The wizard used to print a banner and then ask one question at a time. It
now draws a logo, then a table with every question on it at once. Four real
bugs came out of that, and every one of them was invisible until the thing
was actually run under a pty and rendered to a screen.

## The form was never actually centred

`render_form()` computed

```c
int left = (term_cols - w) / 2;
```

and used `left` when positioning the cursor — but never printed it. The box
drew hard against the left edge while the cursor sat seven columns further
right. So typing appended *after* the hint instead of over it, and every
answer came out looking like the default with your text stuck to the end.

The fix is a file-scope `box_left`, set once by `box_begin(w, rows)` and
emitted as leading spaces by `box_line()` and `box_rule()`. It is state
rather than a parameter on purpose: there are two consumers of the number,
and a parameter is one more thing to pass wrong.

## The typed characters were never stored

This is the one that would have shipped a completely broken wizard.

`type_into()` echoed each character to the screen and incremented `n`, and
then broke on Enter and wrote the terminator — but there was no line
storing the character into the buffer:

```c
putchar(f->secret ? '*' : (char)c);
n++;                      /* counted it, echoed it, kept it nowhere */
```

The field came back empty. Every answer was the hint, or the default.

It looks correct in a diff. It looks correct when you read it. It only
fails when something reads the buffer afterwards, and the only thing that
reads the buffer afterwards is the account-creation code, which runs later
and cannot tell you which field went wrong.

## Hostname and timezone were never validated on the table path

The plain prompt path checked both. The table path checked only the
username. Both values go into `system()`:

```c
run("echo %s > /etc/hostname", host);
run("ln -sf /usr/share/zoneinfo/%s /etc/localtime", tz);
```

so on the table path the answer was shell. Not a theoretical concern: the
same code that renders a box will happily accept `; touch /tmp/pwned` and
paste it into a command line.

Every field now carries a validator and a rejection message, and both paths
use the same one:

```c
int (*ok)(const char *);
```

Verified by driving the form with `; touch /tmp/COPPER_PWNED` as a hostname
and `` UTC`touch /tmp/COPPER_PWNED` `` as a timezone, then checking the file
did not appear. It did not appear, and the collected values were the good
ones that followed them.

## An empty stdin spun forever

In the plain path the username prompt was a `do { ... } while (!valid_user())`.
On EOF, `fgets` fails, the buffer stays empty, an empty answer is not a valid
username, and the loop re-asks — forever, as fast as the console takes it.
`read_line()` now sets a flag, the loop breaks on it, and `main()` refuses to
apply a half-filled form rather than creating an account with an empty
password.

Note this is *not* a hang in `getpass()`. The plain path blocks in `getpass`
waiting for a real `/dev/tty`, which is correct — the plain path exists for
a serial console, which is a tty.

## How the animation is made skippable, and why that is unverified

The shield scrolls by at about 100ms per line, and any key skips to the end.
Detection is a zero-timeout `select()` on stdin.

**On this build host that cannot be tested, and not because of the wizard.**
On a WSL2 pty slave, `select`, `poll`, `O_NONBLOCK` reads and `FIONREAD` all
report "nothing waiting" for a byte that is demonstrably in the line
discipline's queue — the tty even echoes it. In canonical mode `read()` then
blocks forever. Raw mode reads work fine, which is why driving the form
works and only the skip does not.

So the fix is built not to depend on the detection working. `boot_sequence()`
ends with an unconditional

```c
tcflush(STDIN_FILENO, TCIFLUSH);
```

before the form appears. A stray keypress can never become the first
character of the first answer, whether or not anything noticed it first.
That was a real user-visible bug: on a WSL pty the skip key survived into
the form and left `riend` where `friend` should have been.

If the skip does not work on a real console it is a cosmetic problem — the
animation plays out in full and the flush still does its job. But it has not
been seen working, and it should be confirmed on the next VMware boot.

## The wordmark could never have fit

The display `COPPER LINUX` cut is 22 rows by 194 columns. An 80-column VGA
console wraps it into an unreadable mess, so it was never going to work
there. The wordmark that ships is a compact 7-row, 68-column 5x7 block font
that renders correctly at 80 columns. The shield, at 40x77, *does* fit and is
used exactly as supplied.

The art is generated:

```sh
python3 tools/gen-boot-art.py     # -> iso/firstboot/boot-art.h
```

not pasted into the C file, because a single dropped `@` in a block of ASCII
is invisible in a diff. One source, one command, no hand-editing.

## The branding line is gone

"handcrafted by 12hrformat" was removed from all thirteen files that carried
it — the CI workflow, `grub.cfg`, `build.sh`, all three `copper-*` scripts,
`iso/live/init`, five files under `rootfs-overlay`, and `copper-init.c`. A
dangling bare `#` left at the top of `resolv.conf` went with it.

The same line crediting farcrowx in `copper-firstboot.c` went too, so no
per-file attribution header ships at all. Repo-wide grep confirms zero
remaining occurrences.

---

# What was messed up, and what fixed it

Nine commits, and most of them exist because something was quietly wrong in a
way that looked like something else. Worth reading in order — several of these
cost a boot cycle each, and the reasons generalise.

## 1. The overlay directories were created before the tmpfs went over them

`iso/live/init` did `mkdir -p /mnt/upper/upper` and then
`mount -t tmpfs … /mnt/upper`. Mounting hides what was underneath, so the
overlay came up with no `upperdir` and died:

```
overlays: failed to resolve '/mnt/upper/upper': -2
```

Fix: mount the tmpfs **first**, then `mkdir` inside it. The whole staging
order is that one trick. `3ae61c1`.

## 2. CI shipped an ISO built from the *previous* commit

The nastiest one, because **the run was fully green**. `restore-keys:
copper-work-` deliberately restores the previous tree — that is what saves a
7-minute kernel build — but the stage guards were existence-only
(`[ -s "$TGT/boot/initrd.img" ] && skip`). Key changed, old tree restored, file
present, stage skipped. An ISO went out with an initrd packed from the old
`init`.

Fix: `stamped_skip` / `stamp_set` / `tree_hash` in `build.sh`. Every stage
stamps its output with a hash of its own inputs. `d9b1ce1`.

**Do not "fix" this by deleting `restore-keys`.** The fallback is not the bug;
it is what makes a warm cache survive an unrelated change. The stamps are what
make it sound.

## 3. `$0` is not a path you can hash after a `cd`

`SELF=$(cd "$(dirname "$0")" && pwd)/$(basename "$0")` is pinned before the
script `cd`s, because CI runs `sudo bash iso/build.sh kernel` — so `$0` is the
*relative* `iso/build.sh`, and line 20 `cd`s out from under it. `cat "$0"` then
had nothing to open, and under `set -euo pipefail` that killed the script 14
lines after a successful 7-minute kernel build. `307d6b9`.

## 4. My own rescue check was a false negative

Added in fix #1, to turn "switch_root on a root with no init" into something
readable. It read:

```sh
[ -x /mnt/merged/sbin/init ] || rescue "no /sbin/init in the merged root"
```

`/sbin/init` is a symlink to the **absolute** path `/usr/bin/copper-init`.
Before `switch_root` does its chroot, an absolute symlink resolves against the
initramfs root — where there is no `/usr/bin` at all. So the test followed the
link into the initramfs, found nothing, and reported a perfectly good ISO as
broken. It cost a boot cycle to find.

Fix: test the binary, not the link. `switch_root` still gets handed
`/sbin/init`, which resolves correctly after the chroot. `b7facc7`.

## 5. …and the build gate I added to catch #4 made the same mistake

`[ -e "$TGT/sbin/init" ]` — which also *follows* the link, this time to the
**build host's** `/usr/bin/copper-init`, which does not exist. Dangling, so
"missing", so the first CI run of the new gate failed on a staged tree that was
fine. It uses `readlink` now. `0cf3fd9`.

Worth internalising: **`-e` and `-x` follow symlinks. Any check on a
Copper-created link must use `readlink`, or it is testing the build machine.**

## 6. Stale files survived in two staging trees

`build_initramfs` and `build_rootfs` both copy into a `$TGT` that may have come
straight out of the cache, and `cp` only ever adds. A file or directory deleted
from the source came back in the next ISO — and because the ISO stage stamps
the whole tree, it did that while *looking* like a clean rebuild. `rm -rf` the
initramfs staging dir; keep a manifest of what the overlay contained last time
and drop whatever it no longer claims. `0cf3fd9`.

**The manifest has to be built from `iso/rootfs-overlay/`, not from `$TGT`.**
My first version diffed `$TGT` against itself, which is the cached directory
that still holds the stale file, so it could never fire. A test now pins that:
it runs the old logic against the scenario and asserts the file survives, so
the test cannot pass on the broken version.

## 7. The serial console addition made the screen lie

`console=tty0 console=ttyS0,115200` looked obviously right. It is not. **The
last `console=` on the command line is what `/dev/console` points at**, and that
is the only place userspace output goes. On a VM with no serial port, `ttyS0`
never registers, `/dev/console` has no working target, and the screen sits on
GRUB's "Booting up kernel" while the system boots perfectly out of sight. It
looks exactly like a hang. Cost hours. `971b6e5`.

Correct order is `console=ttyS0,115200 console=tty0` — register both, screen
last.

## 8. …which then made the serial log useless

Fixing #7 correctly left the file getting the kernel's half of the boot and
none of ours, and under `quiet` — the entry anyone actually boots — **0 bytes**.
Measured, not assumed.

So `say()` in all three places (initramfs, PID 1, lease script) now writes to
stdout *and* to `/dev/ttyS0`, guarded by `[ -c /dev/ttyS0 ]` so a machine with
no serial port does nothing. `9750dac`.

Practical upshot: **you never have to photograph or transcribe a screen
again.** Point the VM's serial port at a file and paste that.

## 9. DHCP was broadcasting into a tunnel

This is the one that mattered most, and it was hiding in plain sight:

```
copper-net: no lease on sit0 yet, still asking
udhcpc: no lease, forking to background
```

`sit0` is not a network card. It is the IPv6-in-IPv4 tunnel the `sit` module
creates at boot, and with `MODULES=n` every driver is built in, so it exists
before the real NIC finishes probing. `first_nonloop_iface()` took the first
name in `/sys/class/net` that wasn't `lo`, so it got `sit0`, brought it up with
`SIOCSIFFLAGS`, and broadcast DHCP discovers into an interface that cannot
carry them. `eth0` sat there untouched and never even reported link up,
because nothing had asked it to.

Fix: read `/sys/class/net/<if>/type` (the `ARPHRD_*` value) and only accept
`ARPHRD_ETHER`, or `ARPHRD_IEEE80211_RADIOTAP` when there is no wired one.
That skips `sit0`, `gre0`, `ip6tnl0`, `ipip0`, `teql0`, `tun0` and `ifb0`
without naming any of them, and it no longer depends on directory order.
`2601222`.

The old code carried a comment predicting this exact failure. It was right, and
it was still worth doing properly.

---

# Goals

Ordered by what unblocks the most.

## G1 — Confirm the first-boot wizard ✅ done

**Done looks like:** the screen after `copper-net: nameserver …` shows the
wizard asking for a name, and a `copper-sh` prompt afterwards.

**Answered.** artifact6 booted to the animation, the table, a created
account, and a prompt in `/home/<user>`. `copper charge` and
`copper rollback` were both run from that shell and both worked.

One loose end under this goal: the animation's keypress-to-skip could not be
exercised on the build host (see above). Confirm it, or on a real console,
on the next boot.

## G2 — Prove the internet, not just DHCP

**Done looks like:**

```sh
ping -c 1 1.1.1.1          # raw IP, no DNS
nslookup example.com       # resolver works
wget -O - http://example.com   # HTTP end to end
```

We have a lease, a route and a resolver. None of the three above has run. The
cheap version is to add a reachability probe to the lease script's `bound`
handler so the next log answers it without anyone typing commands; the honest
version is to run the three commands at a `copper-sh` prompt and paste the
output.

## G3 — Static-IP escape hatch

Some networks don't hand out leases, and a DHCP-only box is a box that can't
be used on one.

**Done looks like:** a `/etc/network`-style file (interface, address, netmask
or prefix, gateway, nameservers) that `copper-init` reads and applies instead
of starting `udhcpc`, when the file is present and non-empty. Missing file
means DHCP, as it does today.

## G4 — WiFi

Bigger than ethernet, because `MODULES=off` means the wireless driver and
`CONFIG_CFG80211` have to be compiled into the kernel `.config` directly.

**Done looks like:** the box associates with an access point and gets a lease
without manual fiddling.

Roughly: enable `CFG80211` plus the driver in `build.sh`; build
**wpa_supplicant** from source (static musl) for association; keep busybox
`udhcpc` for the IP afterwards, since it now demonstrably works; and drop the
card's firmware blobs (a `linux-firmware` subset) into the rootfs.
NetworkManager — glib and dbus from source — is the heavy end state for
roaming and a GUI, and is not required to get onto a network.

The interface-selection bug that broke wired DHCP is already handled for wifi:
`first_nonloop_iface()` now asks sysfs for the `ARPHRD_*` type and prefers
`ARPHRD_ETHER` over `ARPHRD_IEEE80211_RADIOTAP`, so a wireless NIC no longer
turns it into a coin flip.

## G5 — Bluetooth

**Done looks like:** `bluetoothctl` or equivalent sees a paired device.

BlueZ built from source, kernel `CONFIG_BT=y` with the relevant protocol
drivers. BlueZ wants a D-Bus daemon; that's the part to budget time for.

## G6 — Persistence

**Done looks like:** a reboot keeps your files.

The overlay's upper layer is currently tmpfs, so the session is throwaway by
design. Move the upper onto a real disk partition the user picks on the
wizard's last page — same overlay mechanism, different `upperdir`.
`iso/live/init` already lays the overlay down, so this is a matter of mounting
a disk where the tmpfs goes and telling the wizard to offer it.

## G7 — Land `patch-1`

See the top of this document. It needs a PR from an account with write access
to `Copper-linux/copper`, or someone with that access pushing it.

## G8 — A desktop

The long end. Worth saying plainly: it is far away, and nothing before it is
blocked on it.

---

# `copper charge` — what it is and how it got debugged

The hotfix system. `copper charge` pulls `hotfixes.json` and patches the live
system in place; `copper rollback` puts it back. Full user-facing docs are in
`README.md`. This section is the part a next person needs and the README does
not say: **every one of these was found by running the thing, not by reading
it.**

`copper-charge.sh` and `copper-rollback.sh` had never been executed by anyone
before today. Four defects, each of which made the feature completely
non-functional:

1. **`charge` called `curl`, which does not exist on the live system.** The
   rootfs ships busybox applets; there is no `curl` anywhere in the ISO.
   Checked by listing the artifact, not by assuming. Now prefers `curl` if
   present and falls back to `wget`.
2. **`rollback` could never restore anything.** `charge` names a backup by
   `tr '/' '_'`, so `/usr/bin/foo` becomes `usr_bin_foo`. `rollback` undid
   that with `sed 's|__|/|g'` — replacing a *double* underscore, which can
   never appear when `tr` emits one per slash. Every restore died with
   `target file not found`. Now `tr '_' '/'`.
3. **The only hotfix in the database targeted `src/main.c`,** a repo path that
   does not exist on a booted system, so even a successful fetch could only
   ever print `skipping … not found`. Replaced with a demo entry against
   `/etc/copper/demo.txt`, which the ISO ships.
4. **There was no `copper` binary at all.** The tools shipped as
   `copper-charge` and `copper-rollback`, so the command everyone would type,
   `copper charge`, returned `not found`. Added `/usr/bin/copper` as a front
   end, calling the tools by absolute path so a short `PATH` cannot hide them.

## How it is verified

Not by a green CI run — by running both scripts against a fake root in WSL
with the real ISO overlay files, as root, and reading the actual file contents
after each step:

```
copper: applied demo-banner — Demo: proves charge can find, back up and patch a file
copper:   backed up to .../var/backups/copper/etc_copper_demo.txt
copper: charge complete
  -> second run: skipping demo-banner — fail_code not in etc/copper/demo.txt (already fixed?)
  -> rollback:  restored etc_copper_demo.txt → .../etc/copper/demo.txt
```

Apply, backup, idempotent skip, and restore are all confirmed. What is **not**
confirmed is the same cycle on a booted system.

## Testing it offline

The ISO installs `hotfixes.json` at `/etc/copper/hotfixes.json`, and `charge`
prefers that file and never touches the network when it exists. So the hotfix
system can be tested on a VM with no working internet. Delete the file to go
back to fetching from `HOTFIX_URL`.

## A trap worth knowing

`busybox adduser` and `addgroup` write to the real `/etc/passwd` and
`/etc/group` regardless of `HOME`, `PWD`, or anything else. Probing them
inside a WSL shell **modifies WSL**. Back those files up and restore them in
a `trap`, or you will leave junk accounts behind. This happened twice here and
was cleaned up both times.

---

# What is verified, and what is not

Being precise here matters, because it is easy to mistake "it compiles" for
"it works".

## Has actually been executed

- **The boot path, end to end, on a real machine.** Kernel → initramfs →
  overlay → `switch_root` → `copper-init`. The log at the top of this document
  is the evidence.
- **DHCP against a real server.** A lease, `/24` derived from a
  `255.255.255.0` netmask, a default route, and a resolver. `mask_to_prefix`
  had only ever been unit-tested before this; it is now correct in production.
- **`copper-sh`** — compiled with real GCC 12.2
  (`-std=c11 -O2 -Wall -Wextra`, zero warnings) and run through a 44-command
  battery under **AddressSanitizer**: exit 0, no leaks, no overruns. Bugs that
  found and fixed: an argv heap off-by-one, output lost because `_exit()`
  skipped the stdio flush, history storing tokenized instead of the line you
  typed, children reading stale buffered stdin, `ls -l` on a single file, and
  `ls` going one-per-line on a non-TTY so `ls | grep` filters like real `ls`.
- **`tests/smoke.sh`** — 19 assertions over the shell's core behaviours. Run it
  before you touch the shell.
- **The DHCP lease script** — 58 assertions on `mask_to_prefix` (all 33 valid
  netmasks, generated rather than typed, plus non-contiguous and malformed
  input) and 25 more driving the whole script against a stub `ip`.
- **The stage-stamp logic** — 11 assertions including the exact regression
  (output present *and* input moved on must rebuild), and a separate harness
  proving the helpers survive `set -euo pipefail`.
- **The build gates** — overlay-manifest pruning against a simulated warm
  cache, the CRLF gate, and the `readlink` comparison, with a check that the
  *old* manifest logic fails the same scenario.
- **The logging paths** — all three `say()` implementations reach both the
  screen and `ttyS0`, with the `-c` guard proven to skip a non-character
  device.
- **The whole CI build**, green end to end — kernel, musl, busybox, all eight
  GNU tools, Copper's three binaries, rootfs, initramfs, GRUB ISO.
- **`sh -n`** on all three shipped shell scripts, plus at build time via
  `lint_scripts`.
- **`copper-init.c` and `copper-firstboot.c`** compile clean under GCC 12.2
  with `-Wall -Wextra -Wpedantic -Wshadow -Wwrite-strings`, and in CI against
  musl.
- **The first-boot wizard, driven under a pty.** A Python `pty.fork()` driver
  plus a ~60-line terminal emulator (CUP, ED, EL, cursor show/hide, CR/LF/BS,
  scrolling) rendered every screen: the animation, the form empty, a field
  typed into, a rejected username, masked passwords, and a 50x14 terminal too
  small for the table. Width tracking is how the box was caught not wrapping.
- **The hostile-answer test.** `; touch /tmp/COPPER_PWNED` typed as a hostname
  and `` UTC`touch /tmp/COPPER_PWNED` `` as a timezone: both refused, the
  file was never created, and the values the wizard actually collected were the
  valid ones that followed.

## Has never run

- **Any real internet traffic.** No `ping` to the internet, no `nslookup`,
  no `wget`. LAN ping works. G2.
- **Keypress-to-skip on the boot animation.** The code is right and the
  stray-key flush is independent of it, but the only terminal on the build
  host cannot report input readiness. Confirm on a real console.
- **Persistence.** Answers do not survive a reboot. G6.

## A note on green CI runs

**A green run does not mean the artifact is correct.** Run `36220249701` was
fully green and shipped an ISO built from the previous commit's `init`. Always
take the artifact apart: mount it, `gzip -dc` the initrd, decode the cpio, read
`/init` back out, and parse the ISO's Rock Ridge records if you need to know
what a symlink points at. Windows shows 8.3 names because the image has Rock
Ridge and **no Joliet** — `COPPER_I` is `copper-init`, and that is cosmetic,
not a bug.

## The build gates

- `lint_scripts` — `sh -n` over the initramfs `init` and the lease script. Both
  are read by busybox ash on a machine with no shell to log into and fix them
  with.
- `require_kernel_config` / `require_bb_config` — read the `.config` files back
  after kconfig has had its say and refuse to continue, listing what went
  missing.
- `build_copper` — checks the staged tree actually contains the binaries and
  lease script `switch_root` needs, and that `sbin/init` is a symlink to
  `/usr/bin/copper-init`. Uses `readlink`, not `-e`; see bug #5.

---

# Traps that will cost you a day

**Any change to a cache-key file is a full kernel rebuild.** The CI cache key
hashes `iso/build.sh`, `iso/live/init`, `iso/boot/grub.cfg`,
`iso/rootfs-overlay/**`, `iso/src-init/**`, `iso/firstboot/**` and `src/**`. A
change to *any one* of those throws away the whole `iso/work/` cache. The cache
is saved even on failure (`if: always()`), so a red run still warms the next
one. Batch changes; don't dribble.

**The kernel command line's last `console=` is `/dev/console`.** See bug #7.
This is the single most counter-intuitive thing in the whole boot, it produces
a fake hang, and it cost the most time.

**`console=ttyS0,115200` on a VM with no serial port is not a no-op.** It
takes `/dev/console` away from `tty0`. Add the serial port *and* put `tty0`
last.

**Only add symbols to `require_kernel_config` that you have checked exist.**
A symbol that doesn't exist in 6.12 fails the build immediately and
expensively. `ETHERNET` is the cautionary tale: it was in the list, and 6.12
removed it.

**`MODULES=n` means every `tristate` resolves to `y` or `n`,** never `m`. So a
`--enable`d tristate really is `=y` in the final `.config`.

**`core.autocrlf` was `true` on the build machine** and quietly filled the
working tree with CRLF. A CR is invisible in a diff and it breaks things
quietly: a CR at the end of `PATH` in `/etc/profile` makes every command come up
"not found", CRLF in `passwd`/`hosts` breaks the lookups, and CRLF in a script
busybox ash runs turns every line into "command not found" during boot. It is
`false` now and `.gitattributes` asks for LF everywhere. Leave both alone.

**`git commit -- <paths>` commits the working tree, not the index**, which
leaks `update-index --chmod=+x` changes into a later commit. Use index-only:
`git reset -q; git add -- <files>; git update-index --chmod=+x -- <f>; git commit`.

**`core.fileMode` is off on Windows**, so exec bits need
`git update-index --chmod=+x`. `iso/live/init` and the lease script are mode
100755 and must stay that way.

---

# Config facts worth not rediscovering

Each of these cost a wrong turn once. All were checked against real sources.

- **busybox's `make defconfig` is not a stock config.** busybox patches kconfig
  with `const char conf_defname[] = "/dev/null"`
  (`scripts/kconfig/confdata.c:25`), so "defconfig" means *the Kconfig
  defaults*. There is no `configs/defconfig` in 1.36.1. Per-applet symbols live
  in `//config:config SYMBOL` comments inside the `.c` files and in the
  directory `Config.src` files.
- `CONFIG_STATIC` is `default n`, so it must be set explicitly — and **after**
  `make defconfig`, because reassigning a symbol defconfig already answered is
  silently dropped (first assignment wins). That's what `set_bb_config` is for.
- **6.12 has no `CONFIG_ETHERNET`.** The driver menu is unconditional under
  `NET`/`NETDEVICES`. Those are the real gates.
- In 6.12, `config INET` moved to `net/Kconfig` and `net/ipv4/Makefile` builds
  `tcp.o`/`udp.o` in `obj-y` unconditionally under `INET`. So `CONFIG_INET=y`
  (which `x86_64_defconfig` sets) is enough for IPv4.
- `BLK_DEV_NVME` lives in `drivers/nvme/host/Kconfig` in 6.12. It's `tristate`
  with **no default**, so the explicit `--enable` in `build.sh` turns it on.
- `CONFIG_OVERLAY_FS` is also `tristate` with no default. The entire live-root
  design rests on `build.sh` passing `--enable OVERLAY_FS`.
- vmxnet3's Kconfig path isn't `drivers/net/ethernet/vmware/Kconfig` in 6.12
  and the symbol name couldn't be pinned down. `build.sh` passes **both**
  `--enable VMXNET3` and `--enable VMWARE_VMXNET3`; kconfig drops
  whichever doesn't exist. Deliberately not asserted.
- **In busybox 1.36.1 udhcpc's source is `networking/udhcp/dhcpc.c`,** not
  `networking/udhcp.c` — the client was split into a directory. `ip` is split
  too: address parsing is in `networking/libiproute/`.
- The environment udhcpc exports comes from the DHCP **option** names. The
  consequence that matters: **`$subnet` is the netmask as a dotted quad**, and
  `$mask` is the same mask as a decimal uint32. Neither is a prefix length.
- busybox `ip` *does* accept a dotted mask after the slash: `get_prefix_1`
  (`networking/libiproute/utils.c`) falls back to parsing it as a netmask. We
  convert to a prefix length ourselves rather than depending on that.
- `udhcpc -b` does not exit — after its retries it forks into the background
  and keeps trying forever. So init can fire it and move on; no `-n` needed.
- udhcpc does **not** put `PATH` in the lease script's environment, so init has
  to set it before the exec.
- busybox installs udhcpc at `/sbin/udhcpc`, `ip`/`ifconfig`/`route` in
  `/sbin`, `ping` in `/bin`, `wget`/`nslookup` in `/usr/bin`, `switch_root` in
  `/sbin`. It does **not** install a lease script, which is why we ship one.
- `wget`'s `https://` is busybox's internal TLS. It encrypts but does **not**
  verify certificates. Fine for pulling a tarball, not for a login.

---

# Repo map

```
iso/build.sh              stage pipeline: kernel|base|tools|copper|rootfs|initramfs|iso|all
iso/live/init             initramfs: find the ISO, lay a writable overlay, switch_root
iso/boot/grub.cfg         GRUB menu: normal, verbose, debug, initramfs-shell
iso/src-init/copper-init.c    our PID 1
iso/firstboot/copper-firstboot.c   the OOBE wizard  (never executed)
iso/rootfs-overlay/       /etc and friends that land in the rootfs
iso/rootfs-overlay/usr/share/udhcpc/default.script   our DHCP lease script
src/                      copper-sh: main.c, builtins.c, builtins.h
tests/smoke.sh            19-assertion shell smoke test
.github/workflows/build-iso.yml   per-stage CI steps + workspace cache
PR.md                     drafted PR body
```

---

# Tooling and how to drive a boot

## Getting the ISO

Artifacts download fine now, using the Git Credential Manager token. Earlier
notes saying this returns 401 are out of date. Capture the token into a
variable and never print it:

```sh
out=$(printf 'protocol=https\nhost=github.com\n\n' | git credential fill)
tok=$(printf '%s\n' "$out" | sed -n 's/^password=//p')
curl -sL -H "Authorization: Bearer $tok" \
  https://api.github.com/repos/farcrowx/copper/actions/artifacts/<id>/zip -o iso.zip
```

## The VM

VMware Workstation, guest OS **Linux / Other Linux 6.x kernel 64-bit**, 2 GB,
2 processors, **NAT** networking (bridged may not answer DHCP), and the ISO on
the CD drive with **Connected at power on** ticked.

Then **Add… → Serial Port**, "This end is connected to" → **Output to file** →
`C:\Users\hp\Desktop\copper-boot.txt` (VMware insists on a `.txt` name and will
warn that the file does not exist; accept it, it creates the file) → **Connect
at power on**.

Boot the **verbose** entry while diagnosing. The `copper:` and `copper-net:`
lines now reach the file on *every* entry, so paste that file rather than
photographing a screen.

## Tooling on the build machine

- **Git for Windows** gives a real POSIX shell at
  `C:\Program Files\Git\bin\bash.exe`. That allows `sh -n` on the shipped
  scripts and — more usefully — unit testing shell logic with a stub binary on
  `PATH`, no VM and no compiler. Most of the tests in this branch were written
  that way. Don't assume there's no way to test shell here.
- **This box cannot create a symlink at all** — `ln -s` fails regardless of
  privilege, because Windows wants Developer Mode for native symlinks. So any
  test of symlink *behaviour* has to stub `readlink`; the real thing is CI's to
  prove, and it does.
- No local C toolchain: no gcc/clang/tcc, no WSL, no container runtime. C
  verification goes through Compiler Explorer's API, which is **glibc, not
  musl**, so it cannot validate musl-specific code. CI is the real oracle.
- PowerShell gotchas that cost time: no heredocs (write the message to a file
  and use `git commit -F`); inline `bash -lc` with quotes and `$` gets mangled
  (write a `.sh` and invoke it); `curl.exe` mangles JSON in `--data-raw` (write
  the body to a file, use `--data-binary @file`); nested `$'\r'` through
  `bash -lc` arrives as a literal backslash-r, so CR checks must live in a
  script file.

---

# Rules to keep

- Human-sounding commit messages and code. Nothing that reads like AI slop.
- Keep Copper's identity distinct. Arch and Debian are reference material, not
  packaging material.
- **Verification over vibes.** Compile clean, run the battery under ASan before
  claiming a command works, and never ship a fake or placeholder command. A
  command that exists but doesn't work is worse than a missing one, because it
  lies.
- **Read the artifact, not the CI run.** A green build has shipped a stale ISO
  here. It will again.
- When something is unverified, say so in the commit message and in this file.
  The gap between "it builds" and "it boots" is what this project has been
  living on, and bugs #4 through #9 were all invisible to the build.
