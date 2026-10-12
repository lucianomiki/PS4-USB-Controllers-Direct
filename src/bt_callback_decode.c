#include "c4f_bt_callback.h"
#include <string.h>

static uint16_t read16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint64_t read64(const uint8_t *p)
{
    uint64_t v = 0;
    for (unsigned i = 0; i < 8; i++)
        v |= (uint64_t)p[i] << (8 * i);
    return v;
}

int c4fBtDecodeCallback(
    const uint8_t *raw,
    size_t length,
    C4fBtCallbackEvent *event
)
{
    if (!raw || !event ||
        length != C4F_BT_CALLBACK_RECORD_SIZE)
        return 0;

    uint16_t data_length = read16(raw + 14);

    if (data_length > C4F_BT_CALLBACK_DATA_SIZE)
        return 0;

    memset(event, 0, sizeof(*event));

    event->identity = read64(raw);
    event->selector_a = read16(raw + 8);
    event->selector_b = read16(raw + 10);
    event->event_type = raw[12];
    event->event_status = raw[13];
    event->data_length = data_length;

    memcpy(event->data, raw + 24, data_length);

    return 1;
}
