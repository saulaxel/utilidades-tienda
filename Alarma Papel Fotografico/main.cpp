#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <mmsystem.h>
#include <winspool.h>
#include <stdio.h>
#include <set>

#include "resource.h"

#define SECOND_IN_MILIS 1000
#ifdef NDEBUG
#define ALARM_TIME (3 * 60 * SECOND_IN_MILIS)
#else
#define ALARM_TIME (2 * SECOND_IN_MILIS)
#endif


#define IDT_ALARM_TIMER 1
#define WM_CUSTOM_PRINT_JOB (WM_USER + 1)

HINSTANCE hInst;

static DWORD g_lastPrinted = 0;

void HandleNewAddedJobs(HWND hwndDlg, HANDLE hChange)
{
    puts("Find next.");
    if (!FindNextPrinterChangeNotification(hChange, NULL, NULL, NULL))
        return;

    puts("Found it");

    if (GetTickCount() - g_lastPrinted > 1 * 60 * 1000)
    {
        g_lastPrinted = GetTickCount();
        PostMessage(hwndDlg, WM_CUSTOM_PRINT_JOB, 0, 0);
    }

}

DWORD WINAPI MonitorPrintQueueThread(LPVOID lParam)
{
    HWND hwndDlg = (HWND)lParam;
    HANDLE hPrinter = NULL;
    PRINTER_DEFAULTS pd = { 0 };
    pd.DesiredAccess = PRINTER_ACCESS_USE;

    puts("Trying to open printer . . .");
    if (!OpenPrinter(TEXT("Brother DCP-T520W Printer"), &hPrinter, &pd))
    {
        DWORD err = GetLastError();
        puts("Failed to open printer.");
        printf("OpenPrinter failed with error: %lu\n", err);

        return 0;
    }
    puts("Printer open.");



    puts("Creating hChange.");
    HANDLE hChange = FindFirstPrinterChangeNotification(
        hPrinter,
        PRINTER_CHANGE_ADD_JOB,
        0,
        NULL
    );

    if (hChange == INVALID_HANDLE_VALUE)
    {
        ClosePrinter((hPrinter));
        puts("hChange not created.");
        return 0;
    }
    puts("hChange created.");


    puts("Start while . . .");
    while (1)
    {
        DWORD wait = WaitForSingleObject(hChange, INFINITE);

        if (wait == WAIT_OBJECT_0)
        {
            HandleNewAddedJobs(hwndDlg, hChange);
        }
        else
            break;
    }

    puts("End while . . .");
    FindClosePrinterChangeNotification(hChange);
    ClosePrinter(hPrinter);

    return 0;
}

BOOL CALLBACK DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg)
    {
        case WM_INITDIALOG:
        {
            // Icon
            HICON hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MAIN_ICON));
            SendMessage(hwndDlg, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
            SendMessage(hwndDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);

            // Thread Monitors printer activity
            CreateThread(NULL, 0, MonitorPrintQueueThread, (LPVOID)hwndDlg, 0, NULL);
            return TRUE;
        }

        case WM_CLOSE:
            EndDialog(hwndDlg, 0);
            return TRUE;

        case WM_CUSTOM_PRINT_JOB:
            ShowWindow(hwndDlg, SW_SHOWNORMAL);
            SetForegroundWindow(hwndDlg);
            break;

        case WM_COMMAND:
            HWND hButton;
            switch(LOWORD(wParam))
            {
                case IDC_BTN_STOP:
                    hButton = GetDlgItem(hwndDlg, IDC_BTN_SET_ALARM);
                    SetWindowTextW(hButton, L"&Alarma  PAPEL FOTOGRÁFICO  en 3 Minutos");
                    mciSendString("close alarma", NULL, 0, NULL);
                    KillTimer(hwndDlg, IDT_ALARM_TIMER);
                    return TRUE;

                case IDC_BTN_SET_ALARM:
                    hButton = GetDlgItem(hwndDlg, IDC_BTN_SET_ALARM);
                    SetWindowTextW(hButton, L"Resetear &alarma a 3 minutos");
                    mciSendString("close alarma", NULL, 0, NULL);
                    SetTimer(hwndDlg, IDT_ALARM_TIMER, ALARM_TIME, NULL);
                    return TRUE;
            }

        case WM_TIMER:
            if (wParam == IDT_ALARM_TIMER)
            {
                mciSendString("open \"alarma.mp3\" type mpegvideo alias alarma", NULL, 0, NULL);
                mciSendString("play alarma repeat", NULL, 0, NULL);

                KillTimer(hwndDlg, IDT_ALARM_TIMER);
                ShowWindow(hwndDlg, SW_SHOWNORMAL);
                SetForegroundWindow(hwndDlg);
            }
            break;
    }

    return FALSE;
}


int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    hInst = hInstance;

    // The user interface is a modal dialog box
    return DialogBox(hInstance, MAKEINTRESOURCE(DLG_MAIN), NULL, (DLGPROC)DialogProc);
}
