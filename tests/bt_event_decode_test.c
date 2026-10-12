#include <assert.h>
#include <stdio.h>
#include "c4f_bt_event.h"

int main(void)
{
    uint8_t raw[16] = {
        0x18, 0x80, 0, 0,
        0x34, 0x12, 0x78, 0x56,
        1, 2, 3, 4, 5, 6, 7, 8
    };

    C4fBtEvent e = {0};

    assert(c4fBtDecodeEvent(raw, 16, &e) == 1);
    assert(e.type == 0x18);
    assert(e.flags == 0x80);
    assert(e.selector == 0x56781234);

    raw[0] = 0x20;
    assert(c4fBtDecodeEvent(raw, 16, &e) == 1);

    raw[0] = 0x17;
    assert(c4fBtDecodeEvent(raw, 16, &e) == 0);

    assert(c4fBtDecodeEvent(raw, 15, &e) == 0);
    assert(c4fBtDecodeEvent(NULL, 16, &e) == 0);

    puts("DECODIFICADOR BT: PRUEBAS CORRECTAS");
    puts("Eventos 0x18-0x20 y formato de 16 bytes verificados.");
    return 0;
}
