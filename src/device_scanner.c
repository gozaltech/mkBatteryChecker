#include "device_scanner.h"
#include "hid_util.h"

#include <stdio.h>
#include <stdlib.h>

static const struct { USHORT pid; const wchar_t* name; } kKnownDevices[] = {
    { 0x0257, L"Magic Keyboard" },
    { 0x0267, L"Magic Keyboard" },
    { 0x026C, L"Magic Keyboard with Numeric Keypad" },
    { 0x020E, L"Magic Keyboard (Wireless)" },
    { 0x020F, L"Magic Keyboard (Wireless, Numeric)" },
    { 0x0273, L"Magic Keyboard (A2450)" },
    { 0x029A, L"Magic Keyboard with Touch ID and Numeric Keypad" },
    { 0x029C, L"Magic Keyboard with Touch ID" },
    { 0x0322, L"Magic Keyboard (2021)" },
};
static const int kKnownDeviceCount = sizeof(kKnownDevices) / sizeof(kKnownDevices[0]);

void DeviceList_GetName(USHORT pid, wchar_t* buf, int bufLen) {
    for (int i = 0; i < kKnownDeviceCount; ++i) {
        if (kKnownDevices[i].pid == pid) {
            wcscpy_s(buf, bufLen, kKnownDevices[i].name);
            return;
        }
    }
    swprintf_s(buf, bufLen, L"Apple Device (PID: 0x%04X)", pid);
}

static int ScanCallback(HANDLE hDevice, USHORT productId, void* ctx) {
    (void)hDevice;
    DeviceList* list = (DeviceList*)ctx;

    if (list->count >= MAX_DEVICES)
        return 0;

    for (int i = 0; i < list->count; ++i) {
        if (list->devices[i].productId == productId)
            return 0;
    }

    AppleDevice* dev = &list->devices[list->count++];
    dev->productId = productId;
    DeviceList_GetName(productId, dev->displayName,
                       sizeof(dev->displayName) / sizeof(wchar_t));
    return 0;
}

void DeviceList_Scan(DeviceList* out) {
    out->count = 0;
    EnumerateAppleDevices(ScanCallback, out);
}
