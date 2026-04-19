#pragma once

#include <windows.h>

#define BATTERY_DISCONNECTED (-1)

typedef struct {
    HWND   hwnd;
    UINT   msg;
    USHORT pid;
    HANDLE stopEvent;
    HANDLE thread;
} BatteryMonitor;

void BatteryMonitor_Init(BatteryMonitor* mon, HWND hwnd, UINT msg);
void BatteryMonitor_Start(BatteryMonitor* mon, USHORT pid);
void BatteryMonitor_Stop(BatteryMonitor* mon);
void BatteryMonitor_Destroy(BatteryMonitor* mon);
