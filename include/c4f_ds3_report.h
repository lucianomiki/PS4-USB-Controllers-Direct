/* DualShock 3 / SIXAXIS USB HID report 0x01 parser.
 * Original adaptation for Control4Free USB Direct, GPL-3.0-or-later.
 * The structure and function are host-testable without the PS4 SDK.
 */
#ifndef C4F_DS3_REPORT_H
#define C4F_DS3_REPORT_H
#include <stddef.h>
#include <stdint.h>
typedef struct {
    uint32_t buttons;
    uint8_t lx, ly, rx, ry, l2, r2;
} C4fDs3Report;
/* Returns 1 only for a complete, valid 49-byte HID input report. */
int c4fDs3Parse(const uint8_t *report, size_t length, C4fDs3Report *out);
#endif
