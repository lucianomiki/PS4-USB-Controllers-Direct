/* v0.6: diagnóstico de disponibilidad de API Bluetooth HID.
 * No inicializa Bluetooth, registra callbacks ni abre dispositivos.
 */
#include <dlfcn.h>
#include "c4f_log.h"

#ifndef RTLD_NOW
#define RTLD_NOW 2
#endif

#ifndef RTLD_LOCAL
#define RTLD_LOCAL 0
#endif

void c4fBtApiProbe(void)
{
    static const char *nombres[] = {
        "sceBluetoothHidInit",
        "sceBluetoothHidRegisterCallback",
        "sceBluetoothHidUnregisterCallback",
        "sceBluetoothHidRegisterDevice",
        "sceBluetoothHidUnregisterDevice",
        "sceBluetoothHidDisconnectDevice",
        "sceBluetoothHidGetDeviceInfo",
        "sceBluetoothHidDebugGetVersion"
    };

    c4fLog("BT v0.6: starting symbol availability probe\n");

    void *lib = dlopen(
        "/system/common/lib/libSceBluetoothHid.sprx",
        RTLD_NOW | RTLD_LOCAL
    );

    if (!lib) {
        c4fLog("BT v0.6: library unavailable\n");
        return;
    }

    c4fLog("BT v0.6: library loaded\n");

    for (unsigned i = 0; i < sizeof(nombres)/sizeof(nombres[0]); i++) {
        void *simbolo = dlsym(lib, nombres[i]);

        c4fLog("BT v0.6: %s = %s\n",
               nombres[i],
               simbolo ? "AVAILABLE" : "UNAVAILABLE");
    }

    dlclose(lib);

    c4fLog("BT v0.6: symbol probe finished\n");
}
