#define UNICODE
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

#define _WIN32_WINNT 0x0501 // To use INPUT and simulate typing
#include <windows.h>
#include <shellapi.h>
#include <tchar.h>
#include <commctrl.h>
#include <vector>
using std::vector;
#include <string>
using std::wstring;
using std::string;
#include <iostream>
using std::wcout;
using std::endl;
#include <algorithm>

#include "EvictingQueue.h"
#include "ClipItem.h"
#define ForEachClipItem(container)                                           \
    for (deque<ClipItem>::const_iterator it = container.begin(); \
         it != container.end();                                  \
         ++it)


#ifndef WM_CLIPBOARDUPDATE
#define WM_CLIPBOARDUPDATE 0x031D
#endif
#ifndef CF_DIBV5
#define CF_DIBV5 17
#endif

#define ARRAY_LENGTH(arr) (sizeof(arr) / sizeof((arr)[0]))

static const UINT DEFAULT_CLIPBOARD_HISTORY_SIZE = 50;
static const UINT WM_TRAY_ICON_CLICK = (WM_USER + 1);
static const UINT HOTKEY_ID = 1;
static const UINT APP_WIDTH = 150;
static const UINT APP_HEIGHT = 300;
static const ClipItem CLIP_ITEM_EMPTY;

struct AppState {
    HWND hwndMain;
    HWND hNextViewer;
    HWND hwndHistDialog;
    NOTIFYICONDATAW nid;
    EvictingQueue<ClipItem> ClipHistory;
    HWND hwndPrevWindow;
};

/***************************************************************/
/******************** Function Declarations ********************/
/***************************************************************/

// Pointers to functions that will be set at runtime
typedef BOOL (WINAPI *PFN_FormatListenerManager)(HWND);
PFN_FormatListenerManager pAddClipboardFormatListener = NULL,
                          pRemoveClipboardFormatListener = NULL;

// ##### WinApi Window Procedures #####
LRESULT CALLBACK HistoryDialogProc(HWND hwndHistDialog, UINT msg, WPARAM wparam, LPARAM lparam,  UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
LRESULT CALLBACK WndProc(HWND hwndMain, UINT msg, WPARAM wParam, LPARAM lParam);

// ##### System clipboard #####
ClipItem GetSystemClipItem();
bool SetSystemClipboard(const ClipItem &item);
void PasteSystemClipboardText();

// ##### GUI Dialog #####
HWND CreateHistoryDialog(HWND hwndMain, HINSTANCE hInst, AppState *appState);
void ShowHistoryDialog(HWND hwndMain, HWND hwndHistDialog);
void SelectHistoryDialogIndex(HWND hwndHistDialog, int index);
void HideHistoryDialog(HWND hwndMain);
void UpdateHistoryDialog(
    HWND hwndHistDialog,
    const EvictingQueue<ClipItem> &ClipHistory
);
void ManageHistoryDialogItemSelected(
    HWND hwndMain,
    HWND hwndHistDialog,
    EvictingQueue<ClipItem> &ClipHistory,
    HWND hwndPrevWindow
);
void PositionHistoryDialogOnMouse(HWND hwndMain, HWND hwndHistoryDialog);


// ##### In memory History #####
void AddToHistory(EvictingQueue<ClipItem> &ClipHistory, const ClipItem &item);
void SetIndexAsFirstHistoryItem(EvictingQueue<ClipItem> &ClipHistory, size_t idx);

// ##### Clipboard Listener #####
void InitClipboardAPI();

// ##### TrayIcon (Little Taskbar Icon) #####
NOTIFYICONDATAW InitTrayIcon(HWND hwndMain);
void RemoveTrayIcon(NOTIFYICONDATAW &nid);

/**************************************************************/
/******************** Function Definitions ********************/
/**************************************************************/


// ##### WinApi Window Procedures #####

LRESULT CALLBACK HistoryDialogProc(
    HWND hwndHistDialog,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam,
    UINT_PTR uIdSubclass,
    DWORD_PTR dwRefData
)
{
    AppState *appState = reinterpret_cast<AppState *>(dwRefData);
    wcout << "HistoryDialogProc " << msg << endl;

    switch (msg)
    {
    case WM_KEYDOWN:
        wcout << "WM_KEYDOWN" << endl;
        if (wParam == VK_RETURN)
        {
            ManageHistoryDialogItemSelected(
                appState->hwndMain,
                hwndHistDialog,
                appState->ClipHistory,
                appState->hwndPrevWindow
            );
            return 0; // swallow message if you don’t want default beep
        }
        else if (wParam == VK_ESCAPE)
        {
            HideHistoryDialog(appState->hwndMain);
            SetForegroundWindow(appState->hwndPrevWindow);
        }
        break;

    case WM_LBUTTONDBLCLK:
        wcout << "WM_LBUTTONDBLCLK" << endl;
        ManageHistoryDialogItemSelected(
            appState->hwndMain,
            hwndHistDialog,
            appState->ClipHistory,
            appState->hwndPrevWindow
        );
        break;

    case WM_LBUTTONDOWN:
    {
        ShowHistoryDialog(appState->hwndMain, hwndHistDialog);
        break;
    }

    case WM_DESTROY:
        // Clean subclass (avoids leaks)
        RemoveWindowSubclass(hwndHistDialog, HistoryDialogProc, uIdSubclass);
        break;
    }

    // Call original procedure ensures WM_COMMAND in main still receives double click
    return DefSubclassProc(hwndHistDialog, msg, wParam, lParam);
}

LRESULT CALLBACK WndProc(HWND hwndMain, UINT msg, WPARAM wParam, LPARAM lParam)
{
    AppState *appState = reinterpret_cast<AppState *>(
         GetWindowLongPtr(hwndMain, GWLP_USERDATA)
    );

    wcout << "WndProc " << msg << endl;
    wcout << "AppState address: " << appState << endl;

    switch(msg)
    {
    case WM_NCCREATE:
    {
        // The creation param is only received on creation
        CREATESTRUCT *cs = reinterpret_cast<CREATESTRUCT *>(lParam);
        appState = reinterpret_cast<AppState *>(cs->lpCreateParams);
        wcout << "WM_NCCREATE" << endl;

        // Store the param for future calls
        SetWindowLongPtr(
            hwndMain,
            GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(appState)
        );
        return TRUE;
    }


    case WM_CREATE:
        wcout << "WM_CREATE" << endl;
        InitClipboardAPI();
        if (pAddClipboardFormatListener)
        {
            wcout << "Windows Vista+ listener" << endl;
            pAddClipboardFormatListener(hwndMain);
        }
        else
        {
            wcout << "Windows XP- listener" << endl;
            appState->hNextViewer = SetClipboardViewer(hwndMain);
        }

        RegisterHotKey(hwndMain, HOTKEY_ID, MOD_WIN, 'V');
        appState->nid = InitTrayIcon(hwndMain);
        AddToHistory(appState->ClipHistory, GetSystemClipItem());
        appState->hwndHistDialog = CreateHistoryDialog(
            hwndMain,
            GetModuleHandle(NULL),
            appState
        );
        break;

    case WM_CLIPBOARDUPDATE:
    case WM_DRAWCLIPBOARD:
        {
            wcout << "WM_CLIPBOARDUPDATE/WM_DRAWCLIPBOARD" << endl;
            ClipItem item = GetSystemClipItem();
            //wcout << "Glipboard Text is: " << item << endl;
            AddToHistory(appState->ClipHistory, item);

            wcout << "Full history:" << endl;
            ForEachClipItem(appState->ClipHistory)
            {
                wcout << "- " << *it << endl;
            }
            wcout << endl;

            UpdateHistoryDialog(appState->hwndHistDialog, appState->ClipHistory);
            if (msg == WM_DRAWCLIPBOARD && appState->hNextViewer)
                SendMessageW(appState->hNextViewer, msg, wParam, lParam);
        }
        break;

    case WM_CHANGECBCHAIN:
        wcout << "WM_CHANGECBCHAIN" << endl;
        if (reinterpret_cast<HWND>(wParam) == appState->hNextViewer)
            appState->hNextViewer = (HWND)lParam;
        else if (appState->hNextViewer)
            SendMessageW(appState->hNextViewer, msg, wParam, lParam);
        break;

    case WM_HOTKEY:
        wcout << "WM_HOTKEY" << endl;
        if (wParam == HOTKEY_ID)
        {
            appState->hwndPrevWindow = GetForegroundWindow();
            PositionHistoryDialogOnMouse(hwndMain, appState->hwndHistDialog);
            ShowHistoryDialog(hwndMain, appState->hwndHistDialog);
        }
        break;

    case WM_KEYDOWN:
        wcout << "WM_KEYDOWN" << endl;
        break;

    case WM_TRAY_ICON_CLICK:
        wcout << "WM_TRAY_ICON_CLIC" << endl;
        if (LOWORD(lParam) == WM_LBUTTONDBLCLK)
        {
            appState->hwndPrevWindow = GetForegroundWindow();
            ShowHistoryDialog(hwndMain, appState->hwndHistDialog);
        }
        break;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwndMain, &ps);

        // Fill background with system window color
        RECT rect;
        GetClientRect(hwndMain, &rect);
        // Apparently, when highword is 0, Windows interprets the next
        // value as a system color + 1
        FillRect(hdc, &rect, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));

        EndPaint(hwndMain, &ps);
    }
    break;

    case WM_DESTROY:
        wcout << "WM_DESTROY" << endl;
        if (pRemoveClipboardFormatListener)
            pRemoveClipboardFormatListener(hwndMain);
        else
            ChangeClipboardChain(hwndMain, appState->hNextViewer);

        RemoveTrayIcon(appState->nid);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwndMain, msg, wParam, lParam);
    }
    wcout << "Finished WndProc" << endl;
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

    AppState appState = {
        NULL,
        NULL,
        NULL,
        {},
        EvictingQueue<ClipItem>(DEFAULT_CLIPBOARD_HISTORY_SIZE),
        0
    };

    wcout << "Before CreateWindow" << endl;
    HWND hwndMain = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        wc.lpszClassName, _T("Historial de Portapapeles"),
        WS_POPUP | WS_VISIBLE ,
        CW_USEDEFAULT, CW_USEDEFAULT, APP_WIDTH, APP_HEIGHT,
        NULL, NULL, hInstance, &appState
    );
    wcout << "After CreateWindow" << endl;

    appState.hwndMain = hwndMain;
    ShowWindow(hwndMain, SW_HIDE);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return static_cast<int>(msg.wParam);
}

END_SUPPRESS_WARNING


// ##### System clipboard #####

ClipItem GetSystemClipItem()
{
    if (!OpenClipboard(NULL))
        return CLIP_ITEM_EMPTY;

    const UINT cfRTF = RegisterClipboardFormat(L"Rich Text Format");

    ClipItem result;
    ClipItem::Type itemType;


    HANDLE hData;

    if (IsClipboardFormatAvailable(CF_TEXT))
    {
        std::cout << "CF_TEXT Available" << endl;
    }
    if (IsClipboardFormatAvailable(CF_UNICODETEXT))
    {
        itemType = ClipItem::TYPE_TEXT;
        hData = GetClipboardData(CF_UNICODETEXT);
        wcout << "CF_UNICODETEXT Available" << endl;
    }
    if (IsClipboardFormatAvailable(cfRTF))
    {
        std::cout << "cfRTF (Rich Text Format Available) Available" << endl;
    }
    if (IsClipboardFormatAvailable(CF_BITMAP))
    {
        std::cout << "CF_BITMAP Available" << endl;
    }
    if (IsClipboardFormatAvailable(CF_DIB))
    {
        itemType = ClipItem::TYPE_IMAGE;
        hData = GetClipboardData(CF_DIB);
        wcout << "CF_DIB Available" << endl;
    }
    if (IsClipboardFormatAvailable(CF_DIBV5))
    {
        std::cout << "CF_DIBV5 Available" << endl;
    }
    if (IsClipboardFormatAvailable(CF_METAFILEPICT))
    {
        std::cout << "CF_METAFILEPICT Available" << endl;
    }
    if (IsClipboardFormatAvailable(CF_ENHMETAFILE))
    {
        std::cout << "CF_ENHMETAFILE Available" << endl;
    }
    if (IsClipboardFormatAvailable(CF_HDROP))
    {
        std::cout << "CF_HDROP (File Drop List) Available" << endl;
    }


    if (!hData)
    {
        CloseClipboard();
        return CLIP_ITEM_EMPTY;
    }

    void *pData = GlobalLock(hData);
    if (!pData)
    {
        CloseClipboard();
        return CLIP_ITEM_EMPTY;
    }


    if (itemType == ClipItem::TYPE_TEXT)
    {
        wchar_t *pszText = static_cast<wchar_t*>(pData);
        wstring text = pszText ? pszText : L"";
        result = ClipItem(text);
    }
    else if (itemType == ClipItem::TYPE_IMAGE)
    {
        SIZE_T size = GlobalSize(hData);

        // Copy data to vector
        result.type = ClipItem::TYPE_IMAGE;
        result.image.assign(
            reinterpret_cast<unsigned char *>(pData),
            reinterpret_cast<unsigned char *>(pData) + size
        );
    }


    GlobalUnlock(hData);
    CloseClipboard();

    return result;
}

bool SetSystemClipboard(const ClipItem &item)
{
    if (item.type != ClipItem::TYPE_TEXT)
        return false;
    const wstring &text = item.text;

    if (!OpenClipboard(NULL))
        return false;

    // Tries to empty the clipboard
    if (!EmptyClipboard())
    {
        CloseClipboard();
        return false;
    }

    // Allocate global memory for the text including null terminator
    size_t sizeInBytes = (text.length() + 1) * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, sizeInBytes);
    if (!hMem)
    {
        CloseClipboard();
        return false;
    }

    // Lock the memory and copy the string
    void* pMem = GlobalLock(hMem);
    if (!pMem)
    {
        GlobalFree(hMem);
        CloseClipboard();
        return false;
    }

    memcpy(pMem, text.c_str(), sizeInBytes);
    GlobalUnlock(hMem);

    // Set clipboard data
    if (SetClipboardData(CF_UNICODETEXT, hMem) == NULL)
    {
        GlobalFree(hMem);
        CloseClipboard();
        return false;
    }

    // Clipboard now owns the memory; we do NOT free hMem
    CloseClipboard();
    return true;
}

void PasteSystemClipboardText()
{
    // Press Ctrl down
    INPUT inputs[4] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_CONTROL;

    // Press V
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = 'V';

    // Release V
    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = 'V';
    inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;

    // Release Ctrl
    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = VK_CONTROL;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;

    SendInput(4, inputs, sizeof(INPUT));
}


// ##### GUI Dialog #####

HWND CreateHistoryDialog(HWND hwndMain, HINSTANCE hInst, AppState *appState)
{
    HWND hwndHistDialog = CreateWindowExW(
        0, L"LISTBOX", NULL,
        WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL
        | LBS_NOTIFY | WS_VSCROLL | WS_TABSTOP,
        CW_USEDEFAULT, CW_USEDEFAULT, APP_WIDTH, APP_HEIGHT,
        hwndMain, NULL, hInst, NULL
    );

    // Subclass procedure a will allow to manage specific keyboard input
    // like RETURN on the child LISTBOX
    SetWindowSubclass(
        hwndHistDialog,
        HistoryDialogProc,
        1,
        reinterpret_cast<DWORD_PTR>(appState)
    );

    // ClipHistory at creation might be from a saved file or simply current clipboard text
    ForEachClipItem(appState->ClipHistory)
    {
        SendMessageW(
            hwndHistDialog,
            LB_ADDSTRING,
            0,
            reinterpret_cast<LPARAM>(it->ToString().c_str())
        );
    }

    return hwndHistDialog;
}

void ShowHistoryDialog(HWND hwndMain, HWND hwndHistDialog)
{
    ShowWindow(hwndMain, SW_SHOW);
    SetForegroundWindow(hwndHistDialog);
    SelectHistoryDialogIndex(hwndHistDialog, 0);
}

void HideHistoryDialog(HWND hwndMain)
{
    ShowWindow(hwndMain, SW_HIDE);
}

void SelectHistoryDialogIndex(HWND hwndHistDialog, int index)
{
    int count = static_cast<int>(SendMessage(hwndHistDialog, LB_GETCOUNT, 0, 0));
    if (count > index)
    {
        SendMessage(hwndHistDialog, LB_SETCURSEL, index, 0);
        SendMessage(hwndHistDialog, LB_SETTOPINDEX, index, 0);
    }
}

void UpdateHistoryDialog(HWND hwndHistDialog, const EvictingQueue<ClipItem> &ClipHistory)
{
    if (!hwndHistDialog)
        return;

    SendMessageW(hwndHistDialog, LB_RESETCONTENT, 0, 0);

    ForEachClipItem(ClipHistory)
    {
        SendMessageW(
            hwndHistDialog,
            LB_ADDSTRING,
            0,
            reinterpret_cast<LPARAM>(it->ToString().c_str())
        );
    }

}

void ManageHistoryDialogItemSelected(
    HWND hwndMain,
    HWND hwndHistDialog,
    EvictingQueue<ClipItem> &ClipHistory,
    HWND hwndPrevWindow
)
{
    int idx = SendMessageW(hwndHistDialog, LB_GETCURSEL, 0, 0);

    if (idx >= 0 && static_cast<size_t>(idx) < ClipHistory.Size())
    {
        if (!SetSystemClipboard(ClipHistory[idx]))
        {
            MessageBoxW(hwndMain, L"No se pudo establecer el Porta Papeles", L"Error", MB_OK | MB_ICONERROR);
        }
        else
        {
            SetIndexAsFirstHistoryItem(ClipHistory, idx);
            UpdateHistoryDialog(hwndHistDialog, ClipHistory);
        }
    }
    HideHistoryDialog(hwndMain);

    if (hwndPrevWindow)
    {
        // If there was a Previous Window, return focus and paste clipboard
        SetForegroundWindow(hwndPrevWindow);
        PasteSystemClipboardText();
    }
}

void PositionHistoryDialogOnMouse(HWND hwndMain, HWND hwndHistoryDialog)
{
    // Offset to position mouse over the first text element
    const LONG OFFSET_X = -16; // Offset X is more or less random
    const LONG OFFSET_Y = -static_cast<LONG>(
        SendMessage(hwndHistoryDialog, LB_GETITEMHEIGHT, 0, 0)
    ) / 2; // Half the height to try to center mouse in item
    POINT pt;
    GetCursorPos(&pt);
    SetWindowPos(hwndMain, NULL, pt.x + OFFSET_X, pt.y + OFFSET_Y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
}


// ##### In memory History #####

/**
 * Adds item if not contained.
 * Moves as first element if item is already contained.
 */
void AddToHistory(EvictingQueue<ClipItem> &ClipHistory, const ClipItem &item)
{
    size_t idx;
    // Validation
    if (item.IsEmpty())
        return;

    idx = ClipHistory.IndexOf(item);
    if (idx == ClipHistory.NotFound)
    {
        // Saving new item
        ClipHistory.PushAndEvictExcess(item);
    }
    else
    {
        // When item is already saved, put it as the first option in the
        // history
        SetIndexAsFirstHistoryItem(ClipHistory, idx);
    }
}

void SetIndexAsFirstHistoryItem(EvictingQueue<ClipItem> &ClipHistory, size_t idx)
{
    std::swap(ClipHistory[0], ClipHistory[idx]);
}


// ##### Clipboard Listener #####

void InitClipboardAPI()
{
    HMODULE hUser32 = GetModuleHandle(TEXT("user32.dll"));

    if (!hUser32)
        return;

    pAddClipboardFormatListener = reinterpret_cast<PFN_FormatListenerManager>(
        GetProcAddress(hUser32, "AddClipboardFormatListener")
    );
    pRemoveClipboardFormatListener = reinterpret_cast<PFN_FormatListenerManager>(
        GetProcAddress(hUser32, "RemoveClipboardFormatListener")
    );

    if (!pAddClipboardFormatListener || !pRemoveClipboardFormatListener)
    {
        MessageBoxW(NULL, L"No se encontró el API del Portapapeles.", L"Error", MB_ICONERROR);
    }
}


// ##### TrayIcon (Little Taskbar Icon) #####

NOTIFYICONDATAW InitTrayIcon(HWND hwndMain)
{
    NOTIFYICONDATAW nid;
    wcout << "Init Tray Icon START" << endl;
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwndMain;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nid.uCallbackMessage = WM_TRAY_ICON_CLICK;
    nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcsncpy(nid.szTip, L"Historial de Portapapeles", ARRAY_LENGTH(nid.szTip));
    Shell_NotifyIconW(NIM_ADD, &nid);
    wcout << "Init Tray Icon END" << endl;

    return nid;
}

void RemoveTrayIcon(NOTIFYICONDATAW &nid)
{
    Shell_NotifyIconW(NIM_DELETE, &nid);
}
