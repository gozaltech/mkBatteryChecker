#include "hid_util.h"

#include <hidsdi.h>
#include <setupapi.h>
#include <stdlib.h>

void EnumerateAppleDevices(AppleHidCallback cb, void* ctx) {
    GUID hidGuid;
    HidD_GetHidGuid(&hidGuid);

    HDEVINFO di = SetupDiGetClassDevs(
        &hidGuid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (di == INVALID_HANDLE_VALUE)
        return;

    SP_DEVICE_INTERFACE_DATA iface;
    iface.cbSize = sizeof(iface);

    for (DWORD idx = 0;
         SetupDiEnumDeviceInterfaces(di, NULL, &hidGuid, idx, &iface);
         ++idx) {
        DWORD needed = 0;
        SetupDiGetDeviceInterfaceDetail(di, &iface, NULL, 0, &needed, NULL);

        SP_DEVICE_INTERFACE_DETAIL_DATA_W* det =
            (SP_DEVICE_INTERFACE_DETAIL_DATA_W*)malloc(needed);
        if (!det)
            continue;
        det->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

        if (!SetupDiGetDeviceInterfaceDetail(di, &iface, det, needed, NULL, NULL)) {
            free(det);
            continue;
        }

        HANDLE h = CreateFileW(
            det->DevicePath,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL, OPEN_EXISTING, 0, NULL);

        free(det);

        if (h == INVALID_HANDLE_VALUE)
            continue;

        HIDD_ATTRIBUTES attrs;
        attrs.Size = sizeof(attrs);

        int stop = 0;
        if (HidD_GetAttributes(h, &attrs)
            && (attrs.VendorID == APPLE_VID_BT || attrs.VendorID == APPLE_VID_USB)) {
            stop = cb(h, attrs.ProductID, ctx);
        }

        CloseHandle(h);

        if (stop)
            break;
    }

    SetupDiDestroyDeviceInfoList(di);
}
