/* user/evdevtest.c -- the keyboard and mouse as Linux evdev devices, and the
 * VT ioctls an X server uses
 *
 *   evdevtest        /dev/input/event0 and event1: version, ids, names, the
 *                    event types and codes, key state, the grab (a second
 *                    file gets EBUSY); /dev/tty0 and /dev/tty1: VT_GETSTATE,
 *                    VT_OPENQRY, VT_GETMODE/SETMODE, KDSETMODE, KDSKBMODE
 *   evdevtest -w N   ... then wait up to N seconds for a key press and a
 *                    release on event0 (the host sends one: QEMU sendkey)
 * Exit status 0 = every check passed; 77 = no /dev/input (skipped). */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <sys/ioctl.h>

struct input_event { int64_t sec, usec; uint16_t type, code; int32_t value; };
struct input_id { uint16_t bustype, vendor, product, version; };
struct vt_stat { uint16_t v_active, v_signal, v_state; };
struct vt_mode { char mode, waitv; int16_t relsig, acqsig, frsig; };

#define EVIOCGVERSION      _IOR('E', 0x01, int)
#define EVIOCGID           _IOR('E', 0x02, struct input_id)
#define EVIOCGNAME(len)    _IOC(_IOC_READ, 'E', 0x06, len)
#define EVIOCGKEY(len)     _IOC(_IOC_READ, 'E', 0x18, len)
#define EVIOCGBIT(ev, len) _IOC(_IOC_READ, 'E', 0x20 + (ev), len)
#define EVIOCGRAB          _IOW('E', 0x90, int)
#define VT_OPENQRY  0x5600
#define VT_GETMODE  0x5601
#define VT_SETMODE  0x5602
#define VT_GETSTATE 0x5603
#define KDSETMODE   0x4B3A
#define KDGETMODE   0x4B3B
#define KDGKBMODE   0x4B44
#define KDSKBMODE   0x4B45

static int failures;
#define CHECK(c, what) do { int ok_ = (c); printf("evdevtest:  %s  %s\n", ok_ ? "ok  " : "FAIL", what); \
                            if (!ok_) { failures++; printf("evdevtest:        (errno %d: %s)\n", errno, strerror(errno)); } } while (0)

static int bit(const uint8_t *m, int b) { return m[b / 8] >> (b % 8) & 1; }

int main(int argc, char **argv)
{
    int wait_s = argc > 2 && !strcmp(argv[1], "-w") ? atoi(argv[2]) : 0;
    setvbuf(stdout, NULL, _IOLBF, 0);

    int kb = open("/dev/input/event0", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (kb < 0 && errno == ENOENT) { printf("evdevtest: no /dev/input/event0: skipped\n"); return 77; }
    CHECK(kb >= 0, "/dev/input/event0 opens");
    int ms = open("/dev/input/event1", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    CHECK(ms >= 0, "/dev/input/event1 opens");
    if (kb < 0 || ms < 0) return 1;

    int ver = 0;
    CHECK(ioctl(kb, EVIOCGVERSION, &ver) == 0 && ver == 0x010001, "EVIOCGVERSION is 1.0.1");
    struct input_id id;
    CHECK(ioctl(kb, EVIOCGID, &id) == 0 && id.bustype, "EVIOCGID");
    char name[64] = "";
    int n = ioctl(kb, EVIOCGNAME(sizeof name), name);
    CHECK(n > 1 && (size_t)n == strlen(name) + 1, "EVIOCGNAME gives the name and its length");
    printf("evdevtest:        event0: %s (bus %#x)\n", name, id.bustype);
    n = ioctl(ms, EVIOCGNAME(sizeof name), name);
    printf("evdevtest:        event1: %s\n", n > 0 ? name : "?");

    uint8_t ev[4] = { 0 }, keys[96] = { 0 }, rel[2] = { 0 };
    CHECK(ioctl(kb, EVIOCGBIT(0, sizeof ev), ev) > 0 && bit(ev, 1) && !bit(ev, 2), "event0 sends keys, no motion");
    CHECK(ioctl(kb, EVIOCGBIT(1, sizeof keys), keys) == sizeof keys && bit(keys, 30) && bit(keys, 28), "event0 has KEY_A and KEY_ENTER");
    memset(ev, 0, sizeof ev);
    CHECK(ioctl(ms, EVIOCGBIT(0, sizeof ev), ev) > 0 && bit(ev, 1) && bit(ev, 2), "event1 sends buttons and motion");
    memset(keys, 0, sizeof keys);
    CHECK(ioctl(ms, EVIOCGBIT(1, sizeof keys), keys) > 0 && bit(keys, 0x110) && !bit(keys, 30), "event1 has BTN_LEFT, no letters");
    CHECK(ioctl(ms, EVIOCGBIT(2, sizeof rel), rel) > 0 && bit(rel, 0) && bit(rel, 1) && bit(rel, 8), "event1 has REL_X, REL_Y, REL_WHEEL");
    CHECK(ioctl(kb, EVIOCGKEY(sizeof keys), keys) == sizeof keys, "EVIOCGKEY");

    struct input_event e;
    CHECK(read(kb, &e, sizeof e) < 0 && errno == EAGAIN, "nothing to read yet (O_NONBLOCK)");
    CHECK(ioctl(kb, EVIOCGRAB, (void *)1) == 0, "EVIOCGRAB");
    int kb2 = open("/dev/input/event0", O_RDONLY | O_CLOEXEC);
    CHECK(kb2 >= 0 && ioctl(kb2, EVIOCGRAB, (void *)1) < 0 && errno == EBUSY, "a second grab gets EBUSY");
    CHECK(ioctl(kb, EVIOCGRAB, (void *)0) == 0, "the grab is let go");
    CHECK(ioctl(kb2, EVIOCGRAB, (void *)1) == 0 && ioctl(kb2, EVIOCGRAB, (void *)0) == 0, "then the other file can grab");
    if (kb2 >= 0) close(kb2);

    int t0 = open("/dev/tty0", O_WRONLY | O_CLOEXEC);
    CHECK(t0 >= 0, "/dev/tty0 opens");
    struct vt_stat st = { 0 };
    CHECK(ioctl(t0, VT_GETSTATE, &st) == 0 && st.v_active >= 1, "VT_GETSTATE names the active terminal");
    int vt = 0;
    CHECK(ioctl(t0, VT_OPENQRY, &vt) == 0 && vt >= 1, "VT_OPENQRY");
    printf("evdevtest:        active tty%d, VT_OPENQRY tty%d\n", st.v_active, vt);
    if (t0 >= 0) close(t0);

    char path[24];
    snprintf(path, sizeof path, "/dev/tty%d", vt > 0 ? vt : 1);
    int tty = open(path, O_RDWR | O_CLOEXEC);
    CHECK(tty >= 0, "the terminal opens");
    struct vt_mode vm = { 0 };
    CHECK(ioctl(tty, VT_GETMODE, &vm) == 0, "VT_GETMODE");
    vm.mode = 1; vm.relsig = vm.acqsig = 10;
    CHECK(ioctl(tty, VT_SETMODE, &vm) == 0, "VT_SETMODE VT_PROCESS");
    int mode = -1, kbmode = -1;
    CHECK(ioctl(tty, KDGKBMODE, &kbmode) == 0, "KDGKBMODE");
    CHECK(ioctl(tty, KDSETMODE, 1) == 0 && ioctl(tty, KDGETMODE, &mode) == 0 && mode == 1, "KDSETMODE KD_GRAPHICS");
    CHECK(ioctl(tty, KDSKBMODE, 4) == 0, "KDSKBMODE K_OFF");

    if (wait_s) {                       /* the host types a key now */
        printf("evdevtest: waiting for a key\n");
        int press = 0, release = 0;
        for (int i = 0; i < wait_s * 10 && !(press && release); i++) {
            struct pollfd p = { kb, POLLIN, 0 };
            if (poll(&p, 1, 100) <= 0) continue;
            while (read(kb, &e, sizeof e) == sizeof e)
                if (e.type == 1) { if (e.value == 1) press = e.code; else if (e.value == 0) release = e.code; }
        }
        printf("evdevtest:        pressed %d, released %d\n", press, release);
        CHECK(press && release == press, "a key press and its release arrive on event0");
    }

    CHECK(ioctl(tty, KDSETMODE, 0) == 0 && ioctl(tty, KDSKBMODE, kbmode) == 0, "the terminal is given back");
    if (tty >= 0) close(tty);
    printf("evdevtest: %s (%d failure%s)\n", failures ? "FAILED" : "passed", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
