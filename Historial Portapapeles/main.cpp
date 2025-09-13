#if defined(UNICODE) && !defined(_UNICODE)
    #define _UNICODE
#elif defined(_UNICODE) && !defined(UNICODE)
    #define UNICODE
#endif

#if defined(__GNUC__) || defined(__clang__)
  #define BEGIN_SUPPRESS_WARNING _Pragma("GCC diagnostic push") \
                                  _Pragma("GCC diagnostic ignored \"-Wmissing-field-initializers\"")
  #define END_SUPPRESS_WARNING   _Pragma("GCC diagnostic pop")
#else
  #define BEGIN_SUPPRESS_WARNING
  #define END_SUPPRESS_WARNING
#endif

#include <windows.h>
#include <shellapi.h>
#include <tchar.h>
#include <vector>
#include <string>
#include <iostream>

#include "EvictingQueue.h"

using std::wstring;

#ifndef WM_CLIPBOARDUPDATE
#define WM_CLIPBOARDUPDATE 0x031D
#endif

#define ARRAY_LENGTH(arr) (sizeof(arr) / sizeof((arr)[0]))

static const UINT DEFAULT_CLIPBOARD_HISTORY_SIZE = 50;
static const UINT WM_TRAY_ICON_CLICK = (WM_USER + 1);
static const UINT HOTKEY_ID = 1;

typedef BOOL (WINAPI *PFN_FormatListenerManager)(HWND);
PFN_FormatListenerManager pAddClipboardFormatListener = NULL,
                          pRemoveClipboardFormatListener = NULL;

struct AppState {
    HWND hNextViewer;
    HWND hListDialog;
    NOTIFYICONDATAW nid;
    EvictingQueue ClipHistory;
};


void AddToHistory(const wstring &text, EvictingQueue &ClipHistory)
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

void ShowHistoryDialog(HINSTANCE hInst, HWND &hListDialog, const EvictingQueue &ClipHistory)
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

    for (deque<wstring>::const_iterator it = ClipHistory.begin(); it != ClipHistory.end(); ++it)
        SendMessage(hListDialog, LB_ADDSTRING, 0, (LPARAM)it->c_str());
}

void UpdateListBox(const HWND hListDialog, const EvictingQueue &ClipHistory)
{
    if (!hListDialog)
        return;

    SendMessage(hListDialog, LB_RESETCONTENT, 0, 0);

    for (deque<wstring>::const_iterator it = ClipHistory.begin(); it != ClipHistory.end(); ++it)
        SendMessage(hListDialog, LB_ADDSTRING, 0, (LPARAM)it->c_str());
}

void InitClipboardAPI()
{
    HMODULE hUser32 = GetModuleHandle(TEXT("user32.dll"));
    if (!hUser32) return;
    pAddClipboardFormatListener = (PFN_FormatListenerManager)GetProcAddress(hUser32, "AddClipboardFormatListener");
    pRemoveClipboardFormatListener = (PFN_FormatListenerManager)GetProcAddress(hUser32, "RemoveClipboardFormatListener");
}

void InitTrayIcon(HWND hwnd, NOTIFYICONDATAW &nid)
{
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nid.uCallbackMessage = WM_TRAY_ICON_CLICK;
    nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcsncpy(nid.szTip, L"Historial de Portapapeles", ARRAY_LENGTH(nid.szTip));
    Shell_NotifyIconW(NIM_ADD, &nid);
}

void RemoveTrayIcon(NOTIFYICONDATAW &nid)
{
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    AppState *appState = (AppState *)lParam;
    switch(msg)
    {
    case WM_CREATE:
        InitClipboardAPI();
        if (pAddClipboardFormatListener)
            pAddClipboardFormatListener(hwnd);
        else
            appState->hNextViewer = SetClipboardViewer(hwnd);

        RegisterHotKey(hwnd, HOTKEY_ID, MOD_WIN, 'V');
        InitTrayIcon(hwnd, appState->nid);
        break;

    case WM_CLIPBOARDUPDATE:
    case WM_DRAWCLIPBOARD:
        {
            wstring text = GetSystemClipboardText();
            AddToHistory(text, appState->ClipHistory);
            UpdateListBox(appState->hListDialog, appState->ClipHistory);
            if (msg == WM_DRAWCLIPBOARD && appState->hNextViewer)
                SendMessage(appState->hNextViewer, msg, wParam, lParam);
        }
        break;

    case WM_CHANGECBCHAIN:
        if ((HWND)wParam == appState->hNextViewer)
            appState->hNextViewer = (HWND)lParam;
        else if (appState->hNextViewer)
            SendMessage(appState->hNextViewer, msg, wParam, lParam);
        break;

    case WM_HOTKEY:
        if (wParam == HOTKEY_ID)
            ShowHistoryDialog(GetModuleHandle(NULL), appState->hListDialog, appState->ClipHistory);
        break;

    case WM_COMMAND:
        if (HIWORD(wParam) == LBN_DBLCLK && (HWND)lParam == appState->hListDialog)
        {
            int idx = SendMessage(appState->hListDialog, LB_GETCURSEL, 0, 0);
            if (idx >= 0 && idx < (int)appState->ClipHistory.Size())
            {
                if (OpenClipboard(NULL))
                {
                    EmptyClipboard();
                    // TODO cambiar sizeof(wchar_t) por sizeof(ClipHistory
                    size_t len = (appState->ClipHistory[idx].length()+1)*sizeof(appState->ClipHistory[0]);
                    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
                    memcpy(GlobalLock(hMem), appState->ClipHistory[idx].c_str(), len);
                    GlobalUnlock(hMem);
                    SetClipboardData(CF_UNICODETEXT, hMem);
                    CloseClipboard();
                }
            }
            ShowWindow(appState->hListDialog, SW_HIDE);
        }
        break;

    case WM_TRAY_ICON_CLICK:
        if (LOWORD(lParam) == WM_LBUTTONDBLCLK)
            ShowHistoryDialog(GetModuleHandle(NULL), appState->hListDialog, appState->ClipHistory);
        break;

    case WM_DESTROY:
        if (pRemoveClipboardFormatListener)
            pRemoveClipboardFormatListener(hwnd);
        else
            ChangeClipboardChain(hwnd, appState->hNextViewer);

        RemoveTrayIcon(appState->nid);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// Pragma intended to ignore NOTIFYICONDATAW initialization for now. It is zeroed instead
// of giving specific values to each field
BEGIN_SUPPRESS_WARNING

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = _T("ClipboardHistoryHiddenWnd");
    RegisterClass(&wc);

    AppState appState = {NULL, NULL, {}, EvictingQueue(DEFAULT_CLIPBOARD_HISTORY_SIZE)};

    std::cout << "test before CreateWindow" << std::endl;
    HWND hwndMain = CreateWindow(
        wc.lpszClassName, _T("Historial de Portapapeles"),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL, NULL, hInstance, &appState);
    std::cerr << "CreateWindow failed, error: " << GetLastError() << std::endl;
    std::cout << "test after CreateWindow" << std::endl;

    ShowWindow(hwndMain, SW_HIDE);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

END_SUPPRESS_WARNING
