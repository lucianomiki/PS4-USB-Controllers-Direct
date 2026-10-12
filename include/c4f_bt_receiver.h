#ifndef C4F_BT_RECEIVER_H
#define C4F_BT_RECEIVER_H

#include <stdint.h>

/* ABI deducida del despachador de libSceBluetoothHid.sprx:
 * rdi = puntero a registros de 104 bytes;
 * esi = cantidad, como maximo 16;
 * rdx = argumento del usuario.
 *
 * ATENCION: esta funcion NO se registra ante Sony en v0.6.
 */
typedef struct {
    unsigned batches;
    unsigned valid_events;
    unsigned invalid_events;
    unsigned rejected_batches;
    unsigned type_counts[10];
} C4fBtReceiverStats;

void c4fBtReceiverOnBatch(const void *records, unsigned count, void *context);
void c4fBtReceiverReadStats(C4fBtReceiverStats *out);
void c4fBtReceiverReset(void);

#endif
