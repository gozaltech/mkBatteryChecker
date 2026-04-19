#include "tray_app.h"

#include <stdio.h>
#include <stdlib.h>

#define WM_TRAYNOTIFY   (WM_APP + 1)
#define WM_BATT_UPDATE  (WM_APP + 2)
#define IDM_AUTOSTART   100
#define IDM_EXIT        101
#define IDM_DEVICE_BASE 200
#define TRAY_UID        1
#define WCHARS(arr)     (sizeof(arr) / sizeof(wchar_t))

static const wchar_t kWndClassName[] = L"mkBatteryCheckerWnd";
static const wchar_t kAppName[]      = L"mkBatteryChecker";
static const wchar_t kRegKeyRun[]    = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run";
static const wchar_t kRegKeyApp[]    = L"Software\\mkBatteryChecker";

static UINT kWmTaskbarCreated;

static USHORT LoadPid(void) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRegKeyApp, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return 0;
    DWORD val = 0, size = sizeof(val);
    RegQueryValueExW(hKey, L"TargetPid", NULL, NULL, (LPBYTE)&val, &size);
    RegCloseKey(hKey);
    return (USHORT)val;
}

static void SavePid(USHORT pid) {
    HKEY hKey;
    RegCreateKeyExW(HKEY_CURRENT_USER, kRegKeyApp,
                    0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL);
    DWORD val = pid;
    RegSetValueExW(hKey, L"TargetPid", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
    RegCloseKey(hKey);
}

static int GetAutoStart(void) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRegKeyRun, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return 0;
    int exists = RegQueryValueExW(hKey, kAppName, NULL, NULL, NULL, NULL) == ERROR_SUCCESS;
    RegCloseKey(hKey);
    return exists;
}

static void SetAutoStart(int enable) {
    HKEY hKey;
    RegOpenKeyExW(HKEY_CURRENT_USER, kRegKeyRun, 0, KEY_WRITE, &hKey);
    if (enable) {
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(NULL, path, MAX_PATH);
        RegSetValueExW(hKey, kAppName, 0, REG_SZ,
                       (const BYTE*)path,
                       (DWORD)((wcslen(path) + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(hKey, kAppName);
    }
    RegCloseKey(hKey);
}

static void UpdateTrayTip(TrayApp* app, const wchar_t* text) {
    wcscpy_s(app->nid.szTip, WCHARS(app->nid.szTip), text);
    app->nid.uFlags = NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &app->nid);
    app->nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
}

static void ShowNotification(TrayApp* app, int percent) {
    wchar_t msg[128];
    swprintf_s(msg, 128, L"%s battery is at %d%%. Please charge.",
               app->deviceName, percent);
    app->nid.uFlags     |= NIF_INFO;
    app->nid.dwInfoFlags = NIIF_WARNING;
    wcscpy_s(app->nid.szInfoTitle, WCHARS(app->nid.szInfoTitle), L"Low Battery");
    wcscpy_s(app->nid.szInfo, WCHARS(app->nid.szInfo), msg);
    Shell_NotifyIconW(NIM_MODIFY, &app->nid);
    app->nid.uFlags &= ~NIF_INFO;
}

static void CheckThresholds(TrayApp* app, int newLevel, int oldLevel) {
    static const int kThresholds[] = { 25, 20, 15, 10, 5 };
    static const int kCount = sizeof(kThresholds) / sizeof(kThresholds[0]);

    if (newLevel < 0)
        return;

    if (newLevel > 25) {
        app->lastNotified = INT_MAX;
        return;
    }

    for (int i = 0; i < kCount; ++i) {
        int t = kThresholds[i];
        if (newLevel <= t && (oldLevel < 0 || oldLevel > t) && app->lastNotified > t) {
            app->lastNotified = t;
            ShowNotification(app, newLevel);
            break;
        }
    }
}

static void OnBatteryUpdate(TrayApp* app, int level) {
    int prev    = app->battery;
    app->battery = level;

    wchar_t tip[128];
    if (level < 0)
        swprintf_s(tip, 128, L"%s \u2014 Disconnected", app->deviceName);
    else
        swprintf_s(tip, 128, L"%s: %d%%", app->deviceName, level);
    UpdateTrayTip(app, tip);

    CheckThresholds(app, level, prev);
}

static void UpdateDeviceName(TrayApp* app, USHORT pid) {
    for (int i = 0; i < app->cachedDevices.count; ++i) {
        if (app->cachedDevices.devices[i].productId == pid) {
            wcscpy_s(app->deviceName, 128, app->cachedDevices.devices[i].displayName);
            break;
        }
    }
}

static void SelectDevice(TrayApp* app, USHORT pid) {
    if (pid == app->targetPid)
        return;
    BatteryMonitor_Stop(&app->monitor);
    app->targetPid    = pid;
    app->lastNotified = INT_MAX;
    SavePid(pid);
    UpdateDeviceName(app, pid);
    BatteryMonitor_Start(&app->monitor, pid);
}

static void ShowContextMenu(TrayApp* app) {
    DeviceList_Scan(&app->cachedDevices);

    HMENU hDevices = CreatePopupMenu();
    if (app->cachedDevices.count == 0) {
        AppendMenuW(hDevices, MF_STRING | MF_GRAYED, 0, L"No devices found");
    } else {
        for (int i = 0; i < app->cachedDevices.count; ++i) {
            UINT flags = MF_STRING;
            if (app->cachedDevices.devices[i].productId == app->targetPid)
                flags |= MF_CHECKED;
            AppendMenuW(hDevices, flags, IDM_DEVICE_BASE + i,
                        app->cachedDevices.devices[i].displayName);
        }
    }

    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hDevices, L"Devices");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING | (GetAutoStart() ? MF_CHECKED : 0),
                IDM_AUTOSTART, L"Start with Windows");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, IDM_EXIT, L"Exit");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(app->hwnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, app->hwnd, NULL);
    DestroyMenu(hMenu);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    TrayApp* app = (TrayApp*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    if (msg == WM_NCCREATE) {
        app = (TrayApp*)((CREATESTRUCTW*)lp)->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)app);
    }

    if (!app)
        return DefWindowProcW(hwnd, msg, wp, lp);

    switch (msg) {
    case WM_TRAYNOTIFY:
        if (LOWORD(lp) == WM_RBUTTONUP)
            ShowContextMenu(app);
        return 0;

    case WM_BATT_UPDATE:
        OnBatteryUpdate(app, (int)(INT_PTR)wp);
        return 0;

    case WM_COMMAND: {
        UINT id = LOWORD(wp);
        if (id >= IDM_DEVICE_BASE && id < (UINT)(IDM_DEVICE_BASE + app->cachedDevices.count))
            SelectDevice(app, app->cachedDevices.devices[id - IDM_DEVICE_BASE].productId);
        else if (id == IDM_AUTOSTART)
            SetAutoStart(!GetAutoStart());
        else if (id == IDM_EXIT)
            PostQuitMessage(0);
        return 0;
    }

    case WM_DESTROY:
        Shell_NotifyIconW(NIM_DELETE, &app->nid);
        PostQuitMessage(0);
        return 0;
    }

    if (msg == kWmTaskbarCreated) {
        app->nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        Shell_NotifyIconW(NIM_ADD, &app->nid);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wp, lp);
}

int TrayApp_Init(TrayApp* app, HINSTANCE hInst) {
    app->battery      = -1;
    app->lastNotified = INT_MAX;
    wcscpy_s(app->deviceName, 128, L"Magic Keyboard");

    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = kWndClassName;
    if (!RegisterClassExW(&wc))
        return 0;

    kWmTaskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");

    app->hwnd = CreateWindowExW(
        0, kWndClassName, kAppName, 0,
        0, 0, 0, 0,
        HWND_MESSAGE, NULL, hInst, app);
    if (!app->hwnd)
        return 0;

    app->hIcon = (HICON)LoadImageW(NULL, IDI_APPLICATION, IMAGE_ICON, 0, 0, LR_SHARED);

    memset(&app->nid, 0, sizeof(app->nid));
    app->nid.cbSize           = sizeof(app->nid);
    app->nid.hWnd             = app->hwnd;
    app->nid.uID              = TRAY_UID;
    app->nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    app->nid.uCallbackMessage = WM_TRAYNOTIFY;
    app->nid.hIcon            = app->hIcon;
    wcscpy_s(app->nid.szTip, WCHARS(app->nid.szTip), kAppName);
    Shell_NotifyIconW(NIM_ADD, &app->nid);

    BatteryMonitor_Init(&app->monitor, app->hwnd, WM_BATT_UPDATE);

    app->targetPid = LoadPid();
    DeviceList_Scan(&app->cachedDevices);

    if (app->targetPid == 0 && app->cachedDevices.count > 0)
        app->targetPid = app->cachedDevices.devices[0].productId;

    if (app->targetPid) {
        UpdateDeviceName(app, app->targetPid);
        SavePid(app->targetPid);
        BatteryMonitor_Start(&app->monitor, app->targetPid);
    } else {
        UpdateTrayTip(app, L"WinMagicBattery \u2014 no device found");
    }

    return 1;
}

int TrayApp_Run(TrayApp* app) {
    (void)app;
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}

void TrayApp_Destroy(TrayApp* app) {
    BatteryMonitor_Stop(&app->monitor);
    BatteryMonitor_Destroy(&app->monitor);
    if (app->hIcon)
        DestroyIcon(app->hIcon);
}
