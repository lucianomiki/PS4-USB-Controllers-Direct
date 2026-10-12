#ifndef C4F_BT_CALLBACK_H
#define C4F_BT_CALLBACK_H

#include <stddef.h>
#include <stdint.h>

#define C4F_BT_CALLBACK_RECORD_SIZE 104
#define C4F_BT_CALLBACK_DATA_SIZE 80

typedef struct {
    uint64_t identity;
    uint16_t selector_a;
    uint16_t selector_b;
    uint8_t event_type;
    uint8_t event_status;
    uint16_t data_length;
    uint8_t data[C4F_BT_CALLBACK_DATA_SIZE];
} C4fBtCallbackEvent;

int c4fBtDecodeCallback(
    const uint8_t *raw,
    size_t length,
    C4fBtCallbackEvent *event
);

#endif
