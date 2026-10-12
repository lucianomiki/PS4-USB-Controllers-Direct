/* Receptor Bluetooth HID DESARMADO para Control4Free v0.6.
 * No llama a Init, RegisterCallback, RegisterDevice, read o ioctl.
 * Se prueba solo con eventos simulados, nunca con el flujo real de Sony.
 *
 * No se almacena ni se imprime identity, MAC ni el contenido HID.
 */
#include "c4f_bt_receiver.h"
#include "c4f_bt_callback.h"
#include <stdatomic.h>
#include <string.h>

#define BT_MAX_BATCH 16u
#define BT_MAX_DATA 78u  /* limite observado en la biblioteca de Sony */

static atomic_uint g_batches;
static atomic_uint g_valid;
static atomic_uint g_invalid;
static atomic_uint g_rejected;
static atomic_uint g_types[10];

void c4fBtReceiverOnBatch(const void *records, unsigned count, void *context)
{
    (void)context;

    if (!records || count == 0 || count > BT_MAX_BATCH) {
        atomic_fetch_add_explicit(&g_rejected, 1, memory_order_relaxed);
        return;
    }

    atomic_fetch_add_explicit(&g_batches, 1, memory_order_relaxed);

    const unsigned char *base = (const unsigned char *)records;
    for (unsigned i = 0; i < count; i++) {
        C4fBtCallbackEvent e;
        if (!c4fBtDecodeCallback(base + i * C4F_BT_CALLBACK_RECORD_SIZE,
                                 C4F_BT_CALLBACK_RECORD_SIZE, &e) ||
            e.data_length > BT_MAX_DATA || e.event_type >= 10) {
            atomic_fetch_add_explicit(&g_invalid, 1, memory_order_relaxed);
            continue;
        }
        atomic_fetch_add_explicit(&g_valid, 1, memory_order_relaxed);
        atomic_fetch_add_explicit(&g_types[e.event_type], 1,
                                  memory_order_relaxed);
        /* El evento e se descarta: no se acumulan IDs ni datos privados. */
        memset(&e, 0, sizeof(e));
    }
}

void c4fBtReceiverReadStats(C4fBtReceiverStats *out)
{
    if (!out) return;
    out->batches = atomic_load_explicit(&g_batches, memory_order_relaxed);
    out->valid_events = atomic_load_explicit(&g_valid, memory_order_relaxed);
    out->invalid_events = atomic_load_explicit(&g_invalid, memory_order_relaxed);
    out->rejected_batches = atomic_load_explicit(&g_rejected, memory_order_relaxed);
    for (unsigned i = 0; i < 10; i++)
        out->type_counts[i] = atomic_load_explicit(&g_types[i], memory_order_relaxed);
}

void c4fBtReceiverReset(void)
{
    atomic_store_explicit(&g_batches, 0, memory_order_relaxed);
    atomic_store_explicit(&g_valid, 0, memory_order_relaxed);
    atomic_store_explicit(&g_invalid, 0, memory_order_relaxed);
    atomic_store_explicit(&g_rejected, 0, memory_order_relaxed);
    for (unsigned i = 0; i < 10; i++)
        atomic_store_explicit(&g_types[i], 0, memory_order_relaxed);
}
