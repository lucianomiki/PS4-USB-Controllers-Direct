/* Xbox 360 / DualShock 3 USB-native input for Control4Free.
 * This is an experimental PS4 payload component, NOT a GoldHEN game plugin.
 * GPL-3.0-or-later: https://github.com/MoHadiShibli/Control4Free
 *
 * Only Sony's already-tested virtual device driver publishes input to games.
 * No scePadOpen/Read/ReadState hooks. No browser, phone or PC is needed at runtime.
 * USB access is dynamically resolved: ps4-payload-sdk does not ship libSceUsbd.
 */
#include "c4f_xbox_direct.h"
#include "c4f_ds3_report.h"
#include "c4f_log.h"

#include <errno.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifndef RTLD_NOW
#define RTLD_NOW 2
#endif
#ifndef RTLD_GLOBAL
#define RTLD_GLOBAL 0x100
#endif

/* libusb's standard USB device descriptor is 18 bytes, packed. */
typedef struct __attribute__((packed)) {
    uint8_t length, descriptorType;
    uint16_t bcdUSB;
    uint8_t deviceClass, deviceSubClass, deviceProtocol, maxPacketSize0;
    uint16_t vendor, product, bcdDevice;
    uint8_t manufacturer, productString, serialNumber, configurations;
} UsbDeviceDesc;
_Static_assert(sizeof(UsbDeviceDesc) == 18, "USB descriptor layout must be 18 bytes");

typedef void UsbDev;
typedef void UsbHandle;

typedef struct {
    int32_t (*init)(void);
    int32_t (*exit)(void);
    int32_t (*list)(UsbDev ***);
    int32_t (*freeList)(UsbDev **);
    int32_t (*descriptor)(UsbDev *, UsbDeviceDesc *);
    int32_t (*open)(UsbDev *, UsbHandle **);
    void (*close)(UsbHandle *);
    int32_t (*claim)(UsbHandle *, int32_t);
    int32_t (*release)(UsbHandle *, int32_t);
    int32_t (*detach)(UsbHandle *, int32_t);
    int32_t (*checkConnected)(UsbHandle *); /* optional, queried only after inactivity */
    int32_t (*interrupt)(UsbHandle *, unsigned char, unsigned char *, int32_t, int32_t *, uint32_t);
    int32_t (*control)(UsbHandle *, uint8_t, uint8_t, uint16_t, uint16_t, unsigned char *, uint16_t, uint32_t);
} UsbFns;

static UsbFns f;
static void *lib;
static UsbHandle *handle;
static pthread_t worker;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static int started;
static _Atomic int running;
static int usbConnected; /* guarded by mutex */
typedef enum { USB_KIND_NONE=0, USB_KIND_XBOX360=1, USB_KIND_DS3=2 } UsbKind;
static UsbKind deviceKind; /* protected by mutex; worker owns active handle */
static uint8_t lastReport[64];
static int lastReportLength;
static uint64_t lastInputMs;
static uint32_t lastSequence;

static uint64_t nowMs(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

/* The USB worker exclusively owns the active USB handle while running.
 * This is deliberately separate from Sony's VDA thread/game processes. */
/* Original Sony DualShock 3 needs a USB HID GET_REPORT (feature F2) to
 * enter operational mode. Bluetooth uses a DIFFERENT command (F4); do not
 * send Bluetooth pairing or master-address commands over USB. */

/* Identidad Bluetooth del DS3: solo memoria, sin exponer la MAC.
 * Los seis bytes se conservan en el orden original del reporte F2.
 */
static pthread_mutex_t g_ds3_identity_lock = PTHREAD_MUTEX_INITIALIZER;
static unsigned char g_ds3_identity_raw[6];
static int g_ds3_identity_valid = 0;

static void c4fDs3StoreBtIdentityRaw(const unsigned char *raw, int valid)
{
    pthread_mutex_lock(&g_ds3_identity_lock);
    g_ds3_identity_valid = valid && raw != NULL;

    for (unsigned i = 0; i < 6; ++i)
        g_ds3_identity_raw[i] =
            g_ds3_identity_valid ? raw[i] : 0;

    pthread_mutex_unlock(&g_ds3_identity_lock);
}

int c4fDs3CopyBtIdentityRaw(unsigned char out[6])
{
    if (!out) return 0;

    pthread_mutex_lock(&g_ds3_identity_lock);

    int available = g_ds3_identity_valid;

    for (unsigned i = 0; i < 6; ++i)
        out[i] = available ? g_ds3_identity_raw[i] : 0;

    pthread_mutex_unlock(&g_ds3_identity_lock);
    return available;
}

static int activateDs3(UsbHandle *candidate)
{
    c4fDs3StoreBtIdentityRaw(NULL, 0);
    if (!f.control) {
        c4fLog("USB direct v0.3: sceUsbdControlTransfer unavailable; PS3 USB unsupported\n");
        return 0;
    }
    unsigned char feature[17] = {0};
    const int32_t ret = f.control(candidate, 0xA1, 0x01, 0x03F2, 0,
                                  feature, (uint16_t)sizeof(feature), 1000);
    if (ret < 0) {
        c4fLog("USB direct v0.3: DS3 feature F2 request failed: 0x%08x\n", (uint32_t)ret);
        return 0;
    }
    c4fLog("USB direct v0.3: DS3 operational-mode F2 request accepted (result=%d)\n", ret);

    /* v0.6: conservar MAC propia del DS3 sin imprimirla. */
    {
        int valid = (ret == (int32_t)sizeof(feature));

        if (valid) {
            int all_zero = 1;
            int all_ff = 1;

            for (unsigned i = 4; i < 10; ++i) {
                if (feature[i] != 0x00) all_zero = 0;
                if (feature[i] != 0xff) all_ff = 0;
            }

            if (all_zero || all_ff) valid = 0;
        }

        c4fDs3StoreBtIdentityRaw(
            valid ? feature + 4 : NULL, valid
        );

        c4fLog("DS3 BT v0.6: device identity %s (MAC hidden)\n",
               valid ? "READY" : "UNAVAILABLE");

          if (valid) {
              c4fLog("DS3 BT v0.7: registration DRY_RUN VID=0x054c PID=0x0268 (no Bluetooth API called)\n");
          }

    }

    /* v0.5: consultar F5 por USB, sin modificar el emparejamiento. */
    unsigned char f5[8] = {0};
    int32_t f5ret = f.control(candidate, 0xA1, 0x01, 0x03F5, 0,
                              f5, (uint16_t)sizeof(f5), 1000);
    c4fLog("USB direct v0.5: DS3 F5 GET_REPORT status=%d (contents hidden)\n", f5ret);

    /* v0.5: guardar F5 original una sola vez; nunca modificar el mando. */
    if (f5ret == 8) {
        const char *path = C4F_LOG_DIR "/ds3_master_original_f5.bin";
        int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (fd >= 0) {
            ssize_t n = write(fd, f5, sizeof(f5));
            close(fd);
            if (n == (ssize_t)sizeof(f5))
                c4fLog("DS3 BT: original F5 backup saved (8 bytes)\n");
            else {
                unlink(path);
                c4fLog("DS3 BT: backup write failed\n");
            }
        } else if (errno == EEXIST) {
            c4fLog("DS3 BT: original F5 backup already exists\n");
        } else {
            c4fLog("DS3 BT: original F5 backup creation failed\n");
        }
    }
    return 1;
}

/* Xbox 360 is prioritized when both supported devices are connected, to
 * avoid changing previously tested behaviour. This first version supports
 * ONE USB physical controller/virtual pad at a time. */
static UsbHandle *openMatchingUsb(UsbKind *found)
{
    UsbDev **devices = NULL;
    int32_t n = f.list(&devices);
    UsbHandle *result = NULL;
    *found = USB_KIND_NONE;
    if (n <= 0 || !devices) {
        if (devices) f.freeList(devices);
        return NULL;
    }
    for (int preferred = USB_KIND_XBOX360; preferred <= USB_KIND_DS3 && !result; preferred++) {
        for (int32_t i = 0; i < n; i++) {
            UsbDeviceDesc d = {0};
            if (f.descriptor(devices[i], &d) != 0) continue;
            UsbKind kind = USB_KIND_NONE;
            if (d.vendor == 0x045e && d.product == 0x028e) kind = USB_KIND_XBOX360;
            else if (d.vendor == 0x054c && d.product == 0x0268) kind = USB_KIND_DS3;
            if ((int)kind != preferred) continue;
            UsbHandle *candidate = NULL;
            if (f.open(devices[i], &candidate) != 0 || !candidate) continue;
            (void)f.detach(candidate, 0);
            if (f.claim(candidate, 0) != 0) { f.close(candidate); continue; }
            if (kind == USB_KIND_DS3 && !activateDs3(candidate)) {
                f.release(candidate, 0);
                f.close(candidate);
                continue;
            }
            *found = kind;
            result = candidate;
            break;
        }
    }
    f.freeList(devices);
    return result;
}

/* Going offline must neutralize an old pressed button immediately. The
 * PS4 virtual pad itself is NEVER deleted/recreated on a cable unplug. */
static void setUsbConnected(int online)
{
    pthread_mutex_lock(&mutex);
    usbConnected = online;
    memset(lastReport, 0, sizeof(lastReport));
    lastReportLength = 0;
    lastInputMs = 0;
    lastSequence++;
    pthread_mutex_unlock(&mutex);
}

static void *readWorker(void *unused)
{
    (void)unused;
    uint64_t lastValid = nowMs();
    uint64_t lastCheck = 0;
    int failedChecks = 0;
    int otherErrors = 0;
    UsbKind kind;
    pthread_mutex_lock(&mutex);
    kind = deviceKind;
    pthread_mutex_unlock(&mutex);

    while (atomic_load(&running)) {
        if (!handle) {
            /* No new PS4 player on reconnect: retain the same virtual pad. */
            UsbKind newKind = USB_KIND_NONE;
            UsbHandle *fresh = openMatchingUsb(&newKind);
            if (fresh) {
                handle = fresh;
                kind = newKind;
                pthread_mutex_lock(&mutex);
                deviceKind = kind;
                pthread_mutex_unlock(&mutex);
                setUsbConnected(1);
                lastValid = nowMs();
                lastCheck = 0;
                failedChecks = otherErrors = 0;
                c4fLog("USB direct v0.3: %s reconnected; original virtual pad retained\n",
                       kind == USB_KIND_DS3 ? "DualShock 3" : "Xbox 360");
            } else {
                usleep(750000);
            }
            continue;
        }

        unsigned char bytes[64] = {0};
        int32_t length = 0;
        /* Xbox: proprietary 20-byte report. DS3: HID 49-byte report with
         * ID 0x01, though USB transfer may be padded to 64 bytes. */
        const int32_t requested = kind == USB_KIND_DS3 ? 64 : 20;
        int32_t ret = f.interrupt(handle, 0x81, bytes, requested, &length, 12);
        uint64_t now = nowMs();
        const int validXbox = kind == USB_KIND_XBOX360 && length == 20 &&
                              bytes[0] == 0 && bytes[1] == 0x14;
        const int validDs3 = kind == USB_KIND_DS3 && length >= 49 &&
                            length <= 64 && bytes[0] == 0x01;
        if (ret == 0 && (validXbox || validDs3)) {
            pthread_mutex_lock(&mutex);
            memcpy(lastReport, bytes, (size_t)length);
            lastReportLength = length;
            lastInputMs = now;
            lastSequence++;
            pthread_mutex_unlock(&mutex);
            lastValid = now;
            failedChecks = otherErrors = 0;
        } else {
            /* Firmware 13.02 returns 0x80240007 for USB timeout.
             * Do not act on transient timeouts; fall back to neutral after
             * 350 ms through c4fXboxDirectSample(), as in working v0.1. */
            if (ret != 0 && (uint32_t)ret != 0x80240007u && otherErrors < 1000)
                otherErrors++;
            if (now >= lastValid && now - lastValid >= 650 &&
                now >= lastCheck && now - lastCheck >= 400) {
                lastCheck = now;
                if (f.checkConnected) {
                    if (f.checkConnected(handle) != 0) failedChecks++;
                    else failedChecks = 0;
                } else if (otherErrors >= 16) {
                    /* Degraded detection when CheckConnected isn't exported. */
                    failedChecks = 2;
                }
            }
        }

        if (failedChecks >= 2) {
            setUsbConnected(0);
            c4fLog("USB direct v0.3: %s disconnected; inputs neutral; scanning\n",
                   kind == USB_KIND_DS3 ? "DualShock 3" : "Xbox 360");
            f.release(handle, 0);
            f.close(handle);
            handle = NULL;
            lastValid = nowMs();
            lastCheck = 0;
            failedChecks = otherErrors = 0;
            usleep(500000);
            continue;
        }
        usleep(4000); /* Same polling cap as functional v0.1. */
    }
    return NULL;
}

#define LOAD_FUNCTION(member, symbol) do { \
    *(void **)(&f.member) = dlsym(lib, symbol); \
    if (!f.member) { c4fLog("USB direct: missing %s\n", symbol); goto fail; } \
} while (0)

int c4fXboxDirectStart(void)
{
    if (started) return 1;
    static const char *names[] = {
        "/system/common/lib/libSceUsbd.sprx",
        "libSceUsbd.sprx",
        "libSceUsbd"
    };
    for (unsigned i = 0; i < sizeof(names)/sizeof(names[0]) && !lib; i++)
        lib = dlopen(names[i], RTLD_NOW | RTLD_GLOBAL);
    if (!lib) { c4fLog("USB direct: libSceUsbd unavailable\n"); return 0; }

    LOAD_FUNCTION(init, "sceUsbdInit");
    LOAD_FUNCTION(exit, "sceUsbdExit");
    LOAD_FUNCTION(list, "sceUsbdGetDeviceList");
    LOAD_FUNCTION(freeList, "sceUsbdFreeDeviceList");
    LOAD_FUNCTION(descriptor, "sceUsbdGetDeviceDescriptor");
    LOAD_FUNCTION(open, "sceUsbdOpen");
    LOAD_FUNCTION(close, "sceUsbdClose");
    LOAD_FUNCTION(claim, "sceUsbdClaimInterface");
    LOAD_FUNCTION(release, "sceUsbdReleaseInterface");
    LOAD_FUNCTION(detach, "sceUsbdDetachKernelDriver");
    LOAD_FUNCTION(interrupt, "sceUsbdInterruptTransfer");
    /* Optional so the working Xbox 360 path never depends on HID control transfers. */
    *(void **)(&f.control) = dlsym(lib, "sceUsbdControlTransfer");
    /* Optional on some builds; don't break the working USB initialization
     * if this export is absent. Reconnection then relies on fatal I/O errors. */
    *(void **)(&f.checkConnected) = dlsym(lib, "sceUsbdCheckConnected");
    {
        int32_t ret = f.init();
        if (ret != 0) { c4fLog("USB direct: sceUsbdInit=0x%08x\n", (uint32_t)ret); goto fail; }
    }
    UsbKind openedKind = USB_KIND_NONE;
    handle = openMatchingUsb(&openedKind);
    if (!handle) {
        c4fLog("USB direct v0.3: no supported Xbox 360 or original PS3 controller available\n");
        goto fail_initialized;
    }

    pthread_mutex_lock(&mutex);
    memset(lastReport, 0, sizeof(lastReport));
    lastSequence = 0; lastInputMs = 0; lastReportLength = 0;
    deviceKind = openedKind; usbConnected = 1;
    pthread_mutex_unlock(&mutex);
    atomic_store(&running, 1);
    if (pthread_create(&worker, NULL, readWorker, NULL) != 0) {
        c4fLog("USB direct: pthread_create failed\n");
        atomic_store(&running, 0);
        f.release(handle, 0); f.close(handle); handle = NULL;
        goto fail_initialized;
    }
    started = 1;
    c4fLog("USB direct v0.3: %s claimed; worker started\n",
           openedKind == USB_KIND_DS3 ? "DualShock 3" : "Xbox 360");
    return 1;

fail_initialized:
    f.exit();
fail:
    if (lib) dlclose(lib);
    lib = NULL;
    memset(&f, 0, sizeof(f));
    return 0;
}

void c4fXboxDirectStop(void)
{
    if (!started) return;
    atomic_store(&running, 0);
    pthread_join(worker, NULL);
    if (handle) { f.release(handle, 0); f.close(handle); handle = NULL; }
    setUsbConnected(0);
    f.exit();
    if (lib) dlclose(lib);
    lib = NULL;
    started = 0;
    memset(&f, 0, sizeof(f));
}

static uint8_t axis(uint8_t low, uint8_t high)
{
    /* Avoid implementation-defined signed shifts: signed 16-bit -> DS4 8-bit. */
    return (uint8_t)((((int16_t)((uint16_t)low | ((uint16_t)high << 8))) + 32768) >> 8);
}

int c4fXboxDirectSample(ScePadData *out, uint32_t *sequence)
{
    uint8_t b[64];
    uint64_t then;
    uint32_t seq;
    int length;
    UsbKind kind;
    memset(out, 0, sizeof(*out));
    out->lx = out->ly = out->rx = out->ry = 128;
    out->quat[3] = 1.0f;
    out->connected = 1;
    out->count = 1;
    if (!started) { if (sequence) *sequence = 0; return 0; }
    pthread_mutex_lock(&mutex);
    memcpy(b, lastReport, sizeof(b));
    then = lastInputMs;
    seq = lastSequence;
    length = lastReportLength;
    kind = deviceKind;
    int online = usbConnected;
    pthread_mutex_unlock(&mutex);
    if (sequence) *sequence = seq;
    if (!online || !then || nowMs() - then > 350) return 0; /* release stale buttons */
    if (kind == USB_KIND_DS3) {
        C4fDs3Report ds3;
        if (!c4fDs3Parse(b, (size_t)length, &ds3)) return 0;
        out->buttons = ds3.buttons;
        out->lx = ds3.lx; out->ly = ds3.ly;
        out->rx = ds3.rx; out->ry = ds3.ry;
        out->l2 = ds3.l2; out->r2 = ds3.r2;
        return 1;
    }
    if (kind != USB_KIND_XBOX360 || length != 20) return 0;

    const uint8_t lo=b[2], hi=b[3];
    if (lo & 0x01) out->buttons |= 0x0010; /* Up */
    if (lo & 0x02) out->buttons |= 0x0040; /* Down */
    if (lo & 0x04) out->buttons |= 0x0080; /* Left */
    if (lo & 0x08) out->buttons |= 0x0020; /* Right */
    if (lo & 0x10) out->buttons |= 0x0008; /* Start -> Options */
    if (lo & 0x20) out->buttons |= 0x00100000; /* Back -> Touchpad click */
    if (lo & 0x40) out->buttons |= 0x0002; /* L3 */
    if (lo & 0x80) out->buttons |= 0x0004; /* R3 */
    if (hi & 0x01) out->buttons |= 0x0400; /* LB */
    if (hi & 0x02) out->buttons |= 0x0800; /* RB */
    if (hi & 0x04) out->buttons |= 0x10000;/* Xbox -> PS */
    if (hi & 0x10) out->buttons |= 0x4000; /* A -> Cross */
    if (hi & 0x20) out->buttons |= 0x2000; /* B -> Circle */
    if (hi & 0x40) out->buttons |= 0x8000; /* X -> Square */
    if (hi & 0x80) out->buttons |= 0x1000; /* Y -> Triangle */
    out->l2 = b[4]; out->r2 = b[5];
    if (b[4] >= 30) out->buttons |= 0x0100;
    if (b[5] >= 30) out->buttons |= 0x0200;
    out->lx = axis(b[6], b[7]);
    out->ly = (uint8_t)(255 - axis(b[8], b[9]));
    out->rx = axis(b[10], b[11]);
    out->ry = (uint8_t)(255 - axis(b[12], b[13]));
    return 1;
}

/* Is the USB cable/controller currently considered connected? This does not
 * inspect USB from the game's thread: only a mutex-protected cached flag. */
int c4fXboxDirectConnected(void)
{
    int online;
    pthread_mutex_lock(&mutex);
    online = usbConnected;
    pthread_mutex_unlock(&mutex);
    return online;
}
