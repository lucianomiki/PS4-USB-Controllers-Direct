#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "c4f_bt_callback.h"

int main(void)
{
    uint8_t raw[104] = {0};
    C4fBtCallbackEvent event = {0};

    for (int i = 0; i < 8; i++)
        raw[i] = (uint8_t)(i + 1);

    raw[8] = 0x34;
    raw[9] = 0x12;
    raw[10] = 0x78;
    raw[11] = 0x56;
    raw[12] = 4;
    raw[13] = 0x80;
    raw[14] = 3;

    raw[24] = 0xaa;
    raw[25] = 0xbb;
    raw[26] = 0xcc;

    assert(c4fBtDecodeCallback(raw, sizeof(raw), &event));

    assert(event.identity == UINT64_C(0x0807060504030201));
    assert(event.selector_a == 0x1234);
    assert(event.selector_b == 0x5678);
    assert(event.event_type == 4);
    assert(event.event_status == 0x80);
    assert(event.data_length == 3);
    assert(event.data[0] == 0xaa);
    assert(event.data[2] == 0xcc);

    assert(!c4fBtDecodeCallback(raw, 16, &event));
    assert(!c4fBtDecodeCallback(NULL, 104, &event));

    raw[14] = 81;
    assert(!c4fBtDecodeCallback(raw, 104, &event));

    puts("PASO 59: DECODIFICADOR CALLBACK CORRECTO");
    return 0;
}
