#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "c4f_bt_receiver.h"

int main(void)
{
    unsigned char records[2 * 104] = {0};
    C4fBtReceiverStats stats = {0};

    /* Un evento sintetico valido, tipo 4 y 3 bytes. */
    records[12] = 4;
    records[14] = 3;
    records[24] = 0xaa;
    records[25] = 0xbb;
    records[26] = 0xcc;
    /* Segundo registro con longitud superior al maximo real 78. */
    records[104 + 12] = 5;
    records[104 + 14] = 79;

    c4fBtReceiverReset();
    c4fBtReceiverOnBatch(records, 2, NULL);
    c4fBtReceiverOnBatch(NULL, 1, NULL);
    c4fBtReceiverOnBatch(records, 17, NULL);
    c4fBtReceiverReadStats(&stats);

    assert(stats.batches == 1);
    assert(stats.valid_events == 1);
    assert(stats.invalid_events == 1);
    assert(stats.rejected_batches == 2);
    assert(stats.type_counts[4] == 1);
    assert(stats.type_counts[5] == 0);

    puts("PASO 61: RECEPTOR DESARMADO - PRUEBAS CORRECTAS");
    puts("Sin init, registro de callback, lectura Bluetooth ni datos MAC.");
    return 0;
}
