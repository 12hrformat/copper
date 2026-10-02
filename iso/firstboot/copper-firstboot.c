/*
 * copper-firstboot — first-boot personalization, in the spirit of the OOBE
 * handcrafted by farcrowx
 * on real distros / Windows. copper-init runs this once (until the marker
 * /etc/copper-firstboot.done exists).
 *
 * Asks for: name, username, hostname, timezone, and passwords (root + the
 * named user). Creates the account via busybox adduser, sets passwords via
 * busybox chpasswd, wires up /etc/localtime.
 *
 * Live-session only for now (the overlay is tmpfs, so it re-runs next
 * boot) — real persistence is a later phase.
 */

#define _GNU_SOURCE

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static void banner(void) {
    printf("\n===================================================\n");
    printf("         Welcome to Copper Linux\n");
    printf("===================================================\n");
    printf("Made by farcrowx and 12hrformat\n");
    printf("A couple of questions and you're in. (This is a live\n");
    printf("session, so answers apply for now — persistence is\n");
    printf("coming in a later build.)\n\n");
}

static int read_line(char *buf, size_t cap) {
    if (!fgets(buf, cap, stdin)) return 0;
    buf[strcspn(buf, "\r\n")] = '\0';
    return 1;
}

static int valid_user(const char *u) {
    if (!u[0]) return 0;
    size_t n = strlen(u);
    if (n > 32) return 0;
    if (!(isalpha((unsigned char)u[0]) || u[0] == '_')) return 0;
    for (const char *p = u + 1; *p; p++)
        if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '-'))
            return 0;
    return 1;
}

static int valid_host(const char *h) {
    if (!h[0]) return 0;
    size_t n = strlen(h);
    if (n > 63) return 0;
    for (const char *p = h; *p; p++)
        if (!(isalnum((unsigned char)*p) || *p == '-' || *p == '.'))
            return 0;
    return 1;
}

static int valid_tz(const char *z) {
    if (!z[0] || strlen(z) > 100) return 0;
    for (const char *p = z; *p; p++)
        if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '-' ||
              *p == '+' || *p == '/'))
            return 0;
    return 1;   /* bare zones (UTC) and paths (America/New_York) both OK */
}

/* /etc/passwd, /etc/group and /etc/shadow all ship WITHOUT a trailing newline.

   Appending to a file that does not end in one does not begin a new line, it
   concatenates onto the last record. So writing the new account produced

       nobody:x:65534:65534:nobody:/nonexistent:/bin/falsedragon:x:1000:...

   which is one unparseable line instead of two records. busybox then refused
   to read the file at all ("addgroup: /etc/passwd: bad record", once per
   supplementary group), and the account that had just been created was not
   readable by anything.

   The cost of fixing it is one byte per file. The alternative is an account
   that exists on disk and does not work, which is exactly what happened. */
static void ensure_trailing_newline(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return;
    if (fseek(f, -1, SEEK_END) == 0 && fgetc(f) != '\n') {
        fclose(f);
        FILE *w = fopen(path, "a");
        if (w) { fputc('\n', w); fclose(w); }
        return;
    }
    fclose(f);
}

/* Create the account by editing /etc/passwd, /etc/group and /etc/shadow
   directly.

   This exists because busybox adduser has now broken account creation on a
   booted system twice, in two different ways that only reproduce against this
   exact build's busybox (it calls PAM and groupadd, neither of which is in the
   image). /etc/passwd is a colon-separated file and we already know its exact
   format -- it is ours. Writing three lines is less clever than calling a
   helper, and it is a helper that has repeatedly not worked.

   Returns 0 on success. */
static int create_user_direct(const char *user) {
    /* Before anything is appended. See ensure_trailing_newline() above: an
       append to a file with no trailing newline silently corrupts the last
       record instead of adding a new one. */
    ensure_trailing_newline("/etc/passwd");
    ensure_trailing_newline("/etc/group");
    ensure_trailing_newline("/etc/shadow");

    /* Already there? Then this is a re-run and the account is fine. */
    FILE *chk = fopen("/etc/passwd", "r");
    if (chk) {
        char line[512];
        while (fgets(line, sizeof line, chk)) {
            if (strncmp(line, user, strlen(user)) == 0 && line[strlen(user)] == ':') {
                fclose(chk);
                return 0;      /* present already, not a failure */
            }
        }
        fclose(chk);
    }

    /* Find an unused uid by scanning the passwd file for the numeric ids. */
    int uid = 1000;
    FILE *p = fopen("/etc/passwd", "r");
    if (p) {
        char line[512];
        int used[65536];
        memset(used, 0, sizeof used);
        while (fgets(line, sizeof line, p)) {
            char *c1 = strchr(line, ':');
            if (!c1) continue;
            int v = atoi(c1 + 1);
            if (v > 0 && v < 65536) used[v] = 1;
        }
        fclose(p);
        while (uid < 65535 && used[uid]) uid++;
    }

    char home[128], gecos[256];
    snprintf(home, sizeof home, "/home/%s", user);
    snprintf(gecos, sizeof gecos, "%s", user);

    /* group: same name and id as the user, which is the convention adduser
       follows when no primary group is named. */
    int have_group = 0;
    FILE *g = fopen("/etc/group", "r");
    if (g) {
        char line[512];
        while (fgets(line, sizeof line, g))
            if (strncmp(line, user, strlen(user)) == 0 && line[strlen(user)] == ':') {
                have_group = 1; break;
            }
        fclose(g);
    }
    if (!have_group) {
        g = fopen("/etc/group", "a");
        if (g) { fprintf(g, "%s:x:%d:\n", user, uid); fclose(g); }
    }

    p = fopen("/etc/passwd", "a");
    if (!p) return 1;
    fprintf(p, "%s:x:%d:%d:%s:%s:/usr/bin/copper-sh\n",
            user, uid, uid, gecos, home);
    fclose(p);

    /* shadow: locked, no password. set_password() fills it in moments later;
       starting locked means there is no window where the account has an empty
       password and is reachable from a console. */
    int have_shadow = 0;
    FILE *s = fopen("/etc/shadow", "r");
    if (s) {
        char line[512];
        while (fgets(line, sizeof line, s))
            if (strncmp(line, user, strlen(user)) == 0 && line[strlen(user)] == ':') {
                have_shadow = 1; break;
            }
        fclose(s);
    }
    if (!have_shadow) {
        s = fopen("/etc/shadow", "a");
        if (s) { fprintf(s, "%s:!::0:0:99999:7:::\n", user); fclose(s); }
    }

    /* home directory, owned by the new user */
    if (mkdir(home, 0755) != 0 && errno != EEXIST) {
        printf("(note: couldn't create %s)\n", home);
    }
    chown(home, uid, uid);

    /* skel, so a new home is not an empty directory */
    const char *skel = "/etc/skel";
    if (access(skel, R_OK) == 0) {
        /* copy regular files out of skel; no subdirs are shipped there */
        DIR *d = opendir(skel);
        if (d) {
            struct dirent *de;
            while ((de = readdir(d)) != NULL) {
                if (de->d_name[0] == '.') continue;
                char from[512], to[512];
                if (snprintf(from, sizeof from, "%s/%s", skel, de->d_name)
                        >= (int)sizeof from) continue;
                if (snprintf(to, sizeof to, "%s/%s", home, de->d_name)
                        >= (int)sizeof to) continue;
                struct stat st;
                if (stat(from, &st) == 0 && S_ISREG(st.st_mode)) {
                    /* ignore failures: a missing dotfile is not worth a boot */
                    (void)remove(to);
                    if (link(from, to) != 0) { /* hardlink, else copy */
                        FILE *in = fopen(from, "r"), *out = fopen(to, "w");
                        if (in && out) {
                            char buf[4096]; size_t n;
                            while ((n = fread(buf, 1, sizeof buf, in)) > 0)
                                fwrite(buf, 1, n, out);
                        }
                        if (in) fclose(in);
                        if (out) fclose(out);
                    }
                    chown(to, uid, uid);
                }
            }
            closedir(d);
        }
    }

    return 0;
}

/* Ask for one password, twice, and insist the two match.

   getpass() reads from /dev/tty rather than stdin, so it needs the process to
   own a controlling terminal. The wizard is forked straight out of copper-init
   and inherits no session of its own, so getpass() usually cannot open /dev/tty
   and hands back NULL. That fallback used to print only its warning, never the
   question itself, which left the user staring at a bare "(no silent input
   available)" line with no idea what was being asked — and whatever they typed
   next went in blind. Print the prompt ourselves in that case. */
static void ask_password(const char *prompt, char *buf, size_t cap) {
    char *p = getpass(prompt);
    if (p) {
        if (strlen(p) < cap)
            snprintf(buf, cap, "%s", p);
        else
            buf[0] = '\0';
        return;
    }
    printf("%s", prompt);
    fflush(stdout);
    if (!read_line(buf, cap)) buf[0] = '\0';
}

static void read_password(const char *prompt, char *buf, size_t cap,
                          const char *confirm_prompt) {
    char again[256];
    for (;;) {
        ask_password(prompt, buf, cap);

        if (confirm_prompt) {
            ask_password(confirm_prompt, again, sizeof again);
            if (buf[0] && strcmp(buf, again) == 0) return;
            printf("Those didn't match — try again.\n");
            fflush(stdout);
            continue;
        }
        if (buf[0]) return;
        printf("Password can't be empty.\n");
        fflush(stdout);
    }
}

/* run a shell command with absolute busybox; input is pre-validated */
static int run(const char *fmt, ...) {
    char cmd[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(cmd, sizeof cmd, fmt, ap);
    va_end(ap);
    return system(cmd);
}

/* Run a command and throw away what it says.

   The wizard is not a build log. busybox prints a warning for things that are
   entirely normal here -- no /etc/adduser.conf, a group that already exists --
   and six lines of that landing in the middle of a password prompt is what
   made the first boot look like it was asking the same question over and
   over. It was not; it was printing warnings underneath it. */
static int run_quiet(const char *fmt, ...) {
    char cmd[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(cmd, sizeof cmd, fmt, ap);
    va_end(ap);
    char quiet[1200];
    snprintf(quiet, sizeof quiet, "%s >/dev/null 2>&1", cmd);
    return system(quiet);
}

/* Run a command but KEEP what it said, so that if it fails the reason can be
   shown instead of being swallowed. Silence on success, diagnostics on
   failure -- the opposite trade-off to run_quiet().

   `fmt` has to be the LAST named parameter: va_start's second argument must be
   the last named parameter of the variadic function, or the va_list is set up
   from the wrong place and every argument after it is garbage. */
static int run_capture(char *out, size_t cap, const char *fmt, ...) {
    char cmd[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(cmd, sizeof cmd, fmt, ap);
    va_end(ap);

    char wrapped[1200];
    snprintf(wrapped, sizeof wrapped, "%s 2>&1", cmd);

    out[0] = '\0';
    FILE *p = popen(wrapped, "r");
    if (!p) return -1;

    size_t n = fread(out, 1, cap - 1, p);
    out[n] = '\0';
    int rc = pclose(p);
    while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == '\r')) out[--n] = '\0';
    return rc;
}

static void set_password(const char *user, const char *pw) {
    char line[1024];
    snprintf(line, sizeof line, "%s:%s\n", user, pw);
    FILE *p = popen("/bin/busybox chpasswd 2>/dev/null", "w");
    if (p) {
        fputs(line, p);
        pclose(p);
    }
}

int main(void) {
    char name[128]  = "";
    char user[64]   = "";
    char host[64]   = "copper";
    char tz[128]    = "UTC";
    char rootpw[256];
    char userpw[256];

    /* Unbuffered, once, so no prompt can ever be left sitting in a buffer
       waiting for a newline to push it out. /dev/console is a character
       device, not a terminal, so stdio picks full buffering and a prompt
       without a trailing newline stays invisible until something else
       happens to flush it -- which looked like the wizard hanging. */
    setvbuf(stdout, NULL, _IONBF, 0);

    banner();
    fflush(stdout);

    printf("Your name: ");
    fflush(stdout);
    read_line(name, sizeof name);
    if (!name[0]) snprintf(name, sizeof name, "friend");

    do {
        printf("Username [letters, digits, - _]: ");
        fflush(stdout);
        read_line(user, sizeof user);
    } while (!valid_user(user));

    printf("Hostname [copper]: ");
    fflush(stdout);
    {
        char h[64] = "";
        if (read_line(h, sizeof h) && h[0] && valid_host(h))
            snprintf(host, sizeof host, "%s", h);
    }

    read_password("Password (root): ", rootpw, sizeof rootpw,
                  "Confirm root password: ");

    printf("Timezone [UTC]: ");
    fflush(stdout);
    {
        char z[128] = "";
        if (read_line(z, sizeof z) && z[0] && valid_tz(z))
            snprintf(tz, sizeof tz, "%s", z);
    }

    /* --- apply ------------------------------------------------------- */

    printf("\nSetting things up...\n");

    /* hostname + hosts */
    run("echo %s > /etc/hostname", host);
    run("echo '127.0.0.1 localhost %s' > /etc/hosts", host);
    run("echo '::1 localhost ip6-localhost ip6-loopback' >> /etc/hosts");
    run("/bin/busybox hostname %s", host);

    /* The named user, with copper-sh as their login shell.

       Three things about this command, all of them found by running it against
       the busybox that actually ships in the ISO rather than by reading docs:

       -G <own group>   The group must already exist. Without it adduser fails
            with "adduser: unknown group dragon" and creates nothing. So the
            group goes in first, one line above. Omitting -G entirely does not
            help: busybox then tries to create a group of the same name itself
            and reports "adduser: group 'dragon' in use".

       --disabled-password  Not -D. On this busybox -D is AMBIGUOUS between
            --debug, --disabled-login and --disabled-password, so the short
            form is rejected with "Option d is ambiguous" and adduser prints
            its usage and exits without creating the account. The long option
            is unambiguous and does what the wizard wants: the wizard sets both
            passwords itself with chpasswd a few lines further down.

       -s /usr/bin/copper-sh   The login shell. Nothing logs in yet -- init
            goes straight to a shell -- but it is what makes the account a
            Copper account rather than a generic one.

       If it still fails, fall through to writing /etc/passwd by hand rather
       than aborting the boot. A machine with a root account and no named user
       is a broken machine; one with slightly hand-written account files is
       merely unusual. */
    run_quiet("/bin/busybox addgroup %s", user);

    char why[1024] = "";
    if (run_capture(why, sizeof why,
                    "/bin/busybox adduser --disabled-password -G %s "
                    "-h /home/%s -s /usr/bin/copper-sh %s",
                    user, user, user) != 0) {
        printf("(adduser failed, writing the account files directly)\n");
        if (why[0]) printf("  busybox said: %s\n", why);
        if (create_user_direct(user) != 0) {
            printf("Couldn't create user %s.\n", user);
            return 1;
        }
    }
    const char *supp[] = {"users", "audio", "video", "dialout", "cdrom", NULL};
    for (const char **g = supp; *g; g++) {
        /* A missing supplementary group is not worth aborting the boot for --
           the account itself already exists and works. And the output is
           suppressed: on a no-newline /etc/group every one of these printed a
           "bad record" complaint, which is what filled the screen with noise
           in the middle of the password question. */
        run_quiet("/bin/busybox addgroup %s %s", user, *g);
    }
    read_password("Password (for you): ", userpw, sizeof userpw,
                  "Confirm your password: ");
    set_password("root", rootpw);
    set_password(user, userpw);

    /* timezone */
    {
        char zfile[160];
        snprintf(zfile, sizeof zfile, "/usr/share/zoneinfo/%s", tz);
        if (access(zfile, R_OK) == 0) {
            run("ln -sf /usr/share/zoneinfo/%s /etc/localtime", tz);
            run("echo %s > /etc/timezone", tz);
        } else {
            printf("(timezone %s not found — staying on UTC)\n", tz);
        }
    }

    /* done marker

       Line 1 is the LOGIN NAME, not the display name, and that ordering is
       load-bearing: copper-init reads line 1 and does

           chdir("/home/<line 1>")

       to decide where the shell starts. While this file held the display name,
       a person who typed "Jane Doe" as their name and "jane" as their username
       was sent to /home/Jane Doe, which does not exist -- init printed "no
       home directory" and dropped them in / instead.

       Line 2 is the display name, for anything that wants to greet them. */
    {
        FILE *m = fopen("/etc/copper-firstboot.done", "w");
        if (m) {
            fprintf(m, "%s\n%s\n", user, name);
            fclose(m);
        }
    }

    printf("\n===================================================\n");
    printf("  Done — welcome, %s.\n", name);
    printf("  Copper is yours. Type 'help' to see builtins.\n");
    printf("===================================================\n\n");
    return 0;
}
