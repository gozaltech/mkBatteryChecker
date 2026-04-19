#pragma once

#include <windows.h>

#define MAX_DEVICES 16

typedef struct {
    wchar_t displayName[128];
    USHORT  productId;
} AppleDevice;

typedef struct {
    AppleDevice devices[MAX_DEVICES];
    int         count;
} DeviceList;

void    DeviceList_Scan(DeviceList* out);
void    DeviceList_GetName(USHORT pid, wchar_t* buf, int bufLen);
