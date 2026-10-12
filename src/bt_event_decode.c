#include "c4f_bt_event.h"

int c4fBtDecodeEvent(const uint8_t *data, size_t length,
                     C4fBtEvent *event)
{
    if (!data || !event || length != 16)
        return 0;

    if (data[0] < 0x18 || data[0] > 0x20)
        return 0;

    event->type = data[0];
    event->flags = data[1];

    event->selector =
        (uint32_t)data[4] |
        ((uint32_t)data[5] << 8) |
        ((uint32_t)data[6] << 16) |
        ((uint32_t)data[7] << 24);

    event->identity = 0;

    for (unsigned i = 0; i < 8; ++i)
        event->identity |= (uint64_t)data[8+i] << (8*i);

    return 1;
}
