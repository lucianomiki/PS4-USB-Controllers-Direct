#ifndef C4F_BT_EVENT_H
#define C4F_BT_EVENT_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t type;
    uint8_t flags;
    uint32_t selector;
    uint64_t identity;
} C4fBtEvent;

int c4fBtDecodeEvent(const uint8_t *data, size_t length,
                     C4fBtEvent *event);

#endif
