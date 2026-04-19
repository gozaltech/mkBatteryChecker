#pragma once

#include <windows.h>

#define APPLE_VID_BT  0x004C
#define APPLE_VID_USB 0x05AC

typedef int (*AppleHidCallback)(HANDLE hDevice, USHORT productId, void* ctx);

void EnumerateAppleDevices(AppleHidCallback cb, void* ctx);
