/* DualShock 3 / SIXAXIS USB HID input report parser (report ID 0x01).
 * Sources for the wire layout: Linux hid-sony.c and libusb xusb.c.
 * GPL-3.0-or-later. No Sony proprietary SDK code included.
 */
#include "c4f_ds3_report.h"
#include <string.h>

int c4fDs3Parse(const uint8_t *b, size_t length, C4fDs3Report *out)
{
    if (!b || !out || length < 49 || b[0] != 0x01) return 0;
    C4fDs3Report v;
    memset(&v, 0, sizeof(v));
    const uint8_t d = b[2], p = b[3];
    if (d & 0x01) v.buttons |= 0x00100000; /* Select -> Touchpad click */
    if (d & 0x02) v.buttons |= 0x0002;  /* L3 */
    if (d & 0x04) v.buttons |= 0x0004;  /* R3 */
    if (d & 0x08) v.buttons |= 0x0008;  /* Start -> Options */
    if (d & 0x10) v.buttons |= 0x0010;  /* Up */
    if (d & 0x20) v.buttons |= 0x0020;  /* Right */
    if (d & 0x40) v.buttons |= 0x0040;  /* Down */
    if (d & 0x80) v.buttons |= 0x0080;  /* Left */
    if (p & 0x01) v.buttons |= 0x0100;  /* L2 */
    if (p & 0x02) v.buttons |= 0x0200;  /* R2 */
    if (p & 0x04) v.buttons |= 0x0400;  /* L1 */
    if (p & 0x08) v.buttons |= 0x0800;  /* R1 */
    if (p & 0x10) v.buttons |= 0x1000;  /* Triangle */
    if (p & 0x20) v.buttons |= 0x2000;  /* Circle */
    if (p & 0x40) v.buttons |= 0x4000;  /* Cross */
    if (p & 0x80) v.buttons |= 0x8000;  /* Square */
    if (b[4] & 0x01) v.buttons |= 0x10000; /* PS -> PS */
    v.lx = b[6]; v.ly = b[7];
    v.rx = b[8]; v.ry = b[9];
    /* Authentic DS3 provides analog pressures at bytes 18/19. Some
     * compatible pads provide only the digital bit; retain full press. */
    v.l2 = b[18] ? b[18] : ((p & 0x01) ? 255 : 0);
    v.r2 = b[19] ? b[19] : ((p & 0x02) ? 255 : 0);
    if (v.l2 >= 30) v.buttons |= 0x0100;
    if (v.r2 >= 30) v.buttons |= 0x0200;
    *out = v;
    return 1;
}
