#include "battery_monitor.h"
#include "hid_util.h"

#include <hidsdi.h>

#define POLL_INTERVAL  5000
#define MAX_FAILURES   3
#define READ_RETRIES   3
#define RETRY_DELAY_MS 50

typedef struct {
    USHORT targetPid;
    int    result;
} ReadCtx;

static int ReadCallback(HANDLE hDevice, USHORT productId, void* ctx) {
    ReadCtx* rc = (ReadCtx*)ctx;

    if (productId != rc->targetPid)
        return 0;

    for (int attempt = 0; attempt < READ_RETRIES && rc->result == BATTERY_DISCONNECTED; ++attempt) {
        if (attempt > 0)
            Sleep(RETRY_DELAY_MS);
        BYTE report[3] = { 0x90, 0, 0 };
        if (HidD_GetInputReport(hDevice, report, sizeof(report)) && report[0] == 0x90)
            rc->result = report[2];
    }

    return 1;
}

static int ReadBattery(USHORT pid) {
    ReadCtx rc = { pid, BATTERY_DISCONNECTED };
    EnumerateAppleDevices(ReadCallback, &rc);
    return rc.result;
}

static DWORD WINAPI MonitorThread(LPVOID param) {
    BatteryMonitor* mon = (BatteryMonitor*)param;
    int failures = 0;

    while (WaitForSingleObject(mon->stopEvent, 0) == WAIT_TIMEOUT) {
        int battery = ReadBattery(mon->pid);

        if (battery == BATTERY_DISCONNECTED) {
            if (++failures >= MAX_FAILURES) {
                PostMessage(mon->hwnd, mon->msg, (WPARAM)(INT_PTR)BATTERY_DISCONNECTED, 0);
                failures = 0;
            }
        } else {
            failures = 0;
            PostMessage(mon->hwnd, mon->msg, (WPARAM)(INT_PTR)battery, 0);
        }

        WaitForSingleObject(mon->stopEvent, POLL_INTERVAL);
    }

    return 0;
}

void BatteryMonitor_Init(BatteryMonitor* mon, HWND hwnd, UINT msg) {
    mon->hwnd      = hwnd;
    mon->msg       = msg;
    mon->pid       = 0;
    mon->stopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    mon->thread    = NULL;
}

void BatteryMonitor_Start(BatteryMonitor* mon, USHORT pid) {
    BatteryMonitor_Stop(mon);
    ResetEvent(mon->stopEvent);
    mon->pid    = pid;
    mon->thread = CreateThread(NULL, 0, MonitorThread, mon, 0, NULL);
}

void BatteryMonitor_Stop(BatteryMonitor* mon) {
    if (!mon->thread)
        return;
    SetEvent(mon->stopEvent);
    WaitForSingleObject(mon->thread, INFINITE);
    CloseHandle(mon->thread);
    mon->thread = NULL;
}

void BatteryMonitor_Destroy(BatteryMonitor* mon) {
    BatteryMonitor_Stop(mon);
    if (mon->stopEvent) {
        CloseHandle(mon->stopEvent);
        mon->stopEvent = NULL;
    }
}
