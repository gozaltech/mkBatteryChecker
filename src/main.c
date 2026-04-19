#include <windows.h>

#include "tray_app.h"

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    (void)hPrev; (void)lpCmd; (void)nShow;

    HANDLE mutex = CreateMutexW(NULL, TRUE, L"mkBatteryCheckerMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(mutex);
        return 0;
    }

    TrayApp app;
    memset(&app, 0, sizeof(app));
    int ret = TrayApp_Init(&app, hInst) ? TrayApp_Run(&app) : 1;
    TrayApp_Destroy(&app);

    CloseHandle(mutex);
    return ret;
}
