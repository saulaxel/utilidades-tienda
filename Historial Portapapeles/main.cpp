#if defined(UNICODE) && !defined(_UNICODE)
    #define _UNICODE
#elif defined(_UNICODE) && !defined(UNICODE)
    #define UNICODE
#endif

#include <windows.h>
#include <shellapi.h>
#include <tchar.h>
#include <vector>
#include <string>

#include "EvictingQueue.h"

using std::wstring;

#ifndef WM_CLIPBOARDUPDATE
#define WM_CLIPBOARDUPDATE 0x031D
#endif

#define ARRAY_LENGTH(arr) (sizeof(arr) / sizeof((arr)[0]))

#define DEFAULT_CLIPBOARD_HISTORY_SIZE 50
#define WM_TRAY_ICON_CLICK (WM_USER + 1)

typedef BOOL (WINAPI *PFN_FormatListenerManager)(HWND);
PFN_FormatListenerManager pAddClipboardFormatListener = nullptr,
                          pRemoveClipboardFormatListener = nullptr;

HWND hNextViewer = NULL;
HWND hListDialog = NULL;
NOTIFYICONDATAW nid = {0};
EvictingQueue ClipHistory(DEFAULT_CLIPBOARD_HISTORY_SIZE);
const UINT HOTKEY_ID = 1;


void AddToHistory(const wstring &text)
{
    // Validation
    if (text.empty())
        return;

    if (ClipHistory.Contains(text))
        return;

    // Saving new text
    ClipHistory.PushAndEvictExcess(text);
}

wstring GetSystemClipboardText()
{
    if (!OpenClipboard(NULL))
        return L"";

    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (!hData)
    {
        CloseClipboard();
        return L"";
    }

    wchar_t *pszText = static_cast<wchar_t*>(GlobalLock(hData));
    wstring text = pszText ? pszText : L"";
    GlobalUnlock(hData);
    CloseClipboard();
    return text;
}

void ShowHistoryDialog(HINSTANCE hInst)
{
    if (hListDialog)
    {
        ShowWindow(hListDialog, SW_SHOW);
        SetForegroundWindow(hListDialog);
        return;
    }

    hListDialog = CreateWindowEx(
        0, _T("LISTBOX"), NULL,
        WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_VSCROLL | WS_BORDER,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 300,
        NULL, NULL, hInst, NULL);

    for (wstring &s : ClipHistory)
        SendMessage(hListDialog, LB_ADDSTRING, 0, (LPARAM)s.c_str());
}

void UpdateListBox()
{
    if (!hListDialog)
        return;

    SendMessage(hListDialog, LB_RESETCONTENT, 0, 0);

    for (wstring &s : ClipHistory)
        SendMessage(hListDialog, LB_ADDSTRING, 0, (LPARAM)s.c_str());
}

void InitClipboardAPI()
{
    HMODULE hUser32 = GetModuleHandle(TEXT("user32.dll"));
    if (!hUser32) return;
    pAddClipboardFormatListener = (PFN_FormatListenerManager)GetProcAddress(hUser32, "AddClipboardFormatListener");
    pRemoveClipboardFormatListener = (PFN_FormatListenerManager)GetProcAddress(hUser32, "RemoveClipboardFormatListener");
}

void InitTrayIcon(HWND hwnd)
{
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nid.uCallbackMessage = WM_TRAY_ICON_CLICK;
    nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wcsncpy(nid.szTip, L"Historial de Portapapeles", ARRAY_LENGTH(nid.szTip));
    Shell_NotifyIconW(NIM_ADD, &nid);
}

void RemoveTrayIcon()
{
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch(msg)
    {
    case WM_CREATE:
        InitClipboardAPI();
        if (pAddClipboardFormatListener)
            pAddClipboardFormatListener(hwnd);
        else
            hNextViewer = SetClipboardViewer(hwnd);

        RegisterHotKey(hwnd, HOTKEY_ID, MOD_WIN, 'V');
        InitTrayIcon(hwnd);
        break;

    case WM_CLIPBOARDUPDATE:
    case WM_DRAWCLIPBOARD:
        {
            std::wstring text = GetSystemClipboardText();
            AddToHistory(text);
            UpdateListBox();
            if (msg == WM_DRAWCLIPBOARD && hNextViewer)
                SendMessage(hNextViewer, msg, wParam, lParam);
        }
        break;

    case WM_CHANGECBCHAIN:
        if ((HWND)wParam == hNextViewer)
            hNextViewer = (HWND)lParam;
        else if (hNextViewer)
            SendMessage(hNextViewer, msg, wParam, lParam);
        break;

    case WM_HOTKEY:
        if (wParam == HOTKEY_ID)
            ShowHistoryDialog(GetModuleHandle(nullptr));
        break;

    case WM_COMMAND:
        if (HIWORD(wParam) == LBN_DBLCLK && (HWND)lParam == hListDialog)
        {
            int idx = SendMessage(hListDialog, LB_GETCURSEL, 0, 0);
            if (idx >= 0 && idx < (int)ClipHistory.Size())
            {
                if (OpenClipboard(NULL))
                {
                    EmptyClipboard();
                    // TODO cambiar sizeof(wchar_t) por sizeof(ClipHistory
                    size_t len = (ClipHistory[idx].length()+1)*sizeof(ClipHistory[0]);
                    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
                    memcpy(GlobalLock(hMem), ClipHistory[idx].c_str(), len);
                    GlobalUnlock(hMem);
                    SetClipboardData(CF_UNICODETEXT, hMem);
                    CloseClipboard();
                }
            }
            ShowWindow(hListDialog, SW_HIDE);
        }
        break;

    case WM_TRAY_ICON_CLICK:
        if (LOWORD(lParam) == WM_LBUTTONDBLCLK)
            ShowHistoryDialog(GetModuleHandle(nullptr));
        break;

    case WM_DESTROY:
        if (pRemoveClipboardFormatListener)
            pRemoveClipboardFormatListener(hwnd);
        else
            ChangeClipboardChain(hwnd, hNextViewer);

        RemoveTrayIcon();
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = _T("ClipboardHistoryHiddenWnd");
    RegisterClass(&wc);

    HWND hwndMain = CreateWindow(
        wc.lpszClassName, _T("Historial de Portapapeles"),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 0, 0,
        nullptr, nullptr, hInstance, nullptr);

    ShowWindow(hwndMain, SW_HIDE);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
