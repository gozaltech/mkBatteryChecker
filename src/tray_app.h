#pragma once

#include <windows.h>
#include <shellapi.h>

#include "battery_monitor.h"
#include "device_scanner.h"

typedef struct {
    HWND            hwnd;
    HICON           hIcon;
    NOTIFYICONDATAW nid;
    BatteryMonitor  monitor;
    USHORT          targetPid;
    int             battery;
    int             lastNotified;
    wchar_t         deviceName[128];
    DeviceList      cachedDevices;
} TrayApp;

int  TrayApp_Init(TrayApp* app, HINSTANCE hInst);
int  TrayApp_Run(TrayApp* app);
void TrayApp_Destroy(TrayApp* app);
