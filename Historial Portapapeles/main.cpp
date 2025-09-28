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
#include <shlobj.h>

#include <vector>
using std::vector;
#include <string>
using std::wstring;
using std::string;
#include <iostream>
using std::wcout;
using std::endl;
#include <algorithm>
#include <sstream>
using std::wostringstream;

#include "Logger.h"
#include "EvictingQueue.h"
#include "ClipItem.h"


#define ForEachClipItem(container)                               \
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
static const UINT APP_WIDTH = 200;
static const UINT APP_HEIGHT = 300;
static const UINT LINE_HEIGHT = 18;
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

// Helper functions
string VectorToString(const vector<unsigned char> &v);
wstring VectorToWstring(const vector<unsigned char> &v);
size_t StringSizeInBytes(const string &str);
size_t WstringSizeInBytes(const wstring &str);
wstring IntToString(long a);

// Pointers to functions that will be set at runtime
typedef BOOL (WINAPI *PFN_FormatListenerManager)(HWND);
PFN_FormatListenerManager pAddClipboardFormatListener = NULL,
                          pRemoveClipboardFormatListener = NULL;

// ##### WinApi Window Procedures #####
LRESULT CALLBACK HistoryDialogProc(HWND hwndHistDialog, UINT msg, WPARAM wparam, LPARAM lparam,  UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
LRESULT CALLBACK WndProc(HWND hwndMain, UINT msg, WPARAM wParam, LPARAM lParam);

// ##### System clipboard #####
vector <unsigned char> GetSystemClipboardData(UINT format);
ClipItem GetSystemClipboardAsClipItem();
bool SetSystemClipboardData(const void *pData, size_t size, UINT format);
bool SetSystemClipboardToClipItem(const ClipItem &item);
void PasteSystemClipboard();

// ##### FDROP management #####
const wchar_t *PathsFromHDROPBlob(const std::vector<unsigned char> &blob);
vector<unsigned char> BuildShellIDList(const wchar_t *pFileList);

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

// ##### Clipboard Listener #####
void InitClipboardAPI();

// ##### TrayIcon (Little Taskbar Icon) #####
NOTIFYICONDATAW InitTrayIcon(HWND hwndMain);
void RemoveTrayIcon(NOTIFYICONDATAW &nid);

/**************************************************************/
/******************** Function Definitions ********************/
/**************************************************************/

// ##### Helper functions #####

void PrintHistory(wstring msg, EvictingQueue<ClipItem> &ClipHistory)
{
    LOG_INFO(msg);
    ForEachClipItem(ClipHistory)
    {
        wstring p = L"- ";
        switch (it->type)
        {
        case ClipItem::TYPE_TEXT:  p += L"Text "; break;
        case ClipItem::TYPE_HTML:  p += L"HTML "; break;
        case ClipItem::TYPE_IMAGE: p += L"Image "; break;
        case ClipItem::TYPE_FILE:  p += L"File "; break;
        default: throw std::logic_error("Printing empty");
        }
        p += L"<";
        p += it->ToString();
        p += L">";
        LOG_INFO(p);
    }
    LOG_INFO(L"");
}

string VectorToString(const vector<unsigned char> &v)
{
    if (v.empty()) return string();
    return string(
        reinterpret_cast<const char *>(&v[0]),
        // Since the vector contained a string with NUL, we substract it to not repeat it
        v.size() - 1
    );
}

wstring VectorToWstring(const vector<unsigned char> &v)
{
    if (v.empty()) return wstring();
    return wstring(
        reinterpret_cast<const wchar_t *>(&v[0]),
        // Since the vector contained a string with NUL, we substract it to not repeat it
        v.size() / sizeof(wchar_t) - 1
    );
}

size_t StringSizeInBytes(const string &str)
{
    return (str.length() + 1) * sizeof(char);
}

size_t WstringSizeInBytes(const wstring &str)
{
    return (str.length() + 1) * sizeof(wchar_t);
}

wstring IntToString(long n)
{
    wostringstream oss;
    oss << n;
    wstring str = oss.str();
    return str;
}

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
    LOG_INFO(wstring(L"HistoryDialogProc ") + IntToString(msg));

    switch (msg)
    {
    case WM_KEYDOWN:
        LOG_INFO(L"WM_KEYDOWN");
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
        LOG_INFO(L"WM_LBUTTONDBLCLK");
        ManageHistoryDialogItemSelected(
            appState->hwndMain,
            hwndHistDialog,
            appState->ClipHistory,
            appState->hwndPrevWindow
        );
        break;

    case WM_LBUTTONDOWN:
    {
        LOG_INFO(L"WM_LBUTTONDOWN");
        ShowHistoryDialog(appState->hwndMain, hwndHistDialog);
        break;
    }

    case WM_DESTROY:
        LOG_INFO(L"WM_DESTROY");
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

    LOG_INFO(L"WndProc " + IntToString(msg));
    LOG_INFO(L"AppState address: " + IntToString(reinterpret_cast<long>(appState)));

    switch(msg)
    {
    case WM_NCCREATE:
    {
        // The creation param is only received on creation
        CREATESTRUCT *cs = reinterpret_cast<CREATESTRUCT *>(lParam);
        appState = reinterpret_cast<AppState *>(cs->lpCreateParams);
        LOG_INFO(L"WM_NCCREATE");

        // Store the param for future calls
        SetWindowLongPtr(
            hwndMain,
            GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(appState)
        );
        return TRUE;
    }


    case WM_CREATE:
        LOG_INFO(L"WM_CREATE");
        InitClipboardAPI();
        if (pAddClipboardFormatListener)
        {
            LOG_INFO(L"Windows Vista+ listener");
            pAddClipboardFormatListener(hwndMain);
        }
        else
        {
            LOG_INFO(L"Windows XP- listener");
            appState->hNextViewer = SetClipboardViewer(hwndMain);
        }

        RegisterHotKey(hwndMain, HOTKEY_ID, MOD_WIN, 'V');
        appState->nid = InitTrayIcon(hwndMain);
        AddToHistory(appState->ClipHistory, GetSystemClipboardAsClipItem());
        appState->hwndHistDialog = CreateHistoryDialog(
            hwndMain,
            GetModuleHandle(NULL),
            appState
        );
        break;

    case WM_CLIPBOARDUPDATE:
    case WM_DRAWCLIPBOARD:
    {
        LOG_INFO(L"WM_CLIPBOARDUPDATE/WM_DRAWCLIPBOARD");
        PrintHistory(L"Clipboard Update before adding", appState->ClipHistory);
        ClipItem item = GetSystemClipboardAsClipItem();
        LOG_DEBUG(L"Glipboard Text is: <" + item.ToString() + L">");
        AddToHistory(appState->ClipHistory, item);

        PrintHistory(L"Clipboard Update ater adding", appState->ClipHistory);

        UpdateHistoryDialog(appState->hwndHistDialog, appState->ClipHistory);
        if (msg == WM_DRAWCLIPBOARD && appState->hNextViewer)
            SendMessageW(appState->hNextViewer, msg, wParam, lParam);

        break;
    }

    case WM_CHANGECBCHAIN:
        LOG_INFO(L"WM_CHANGECBCHAIN");
        if (reinterpret_cast<HWND>(wParam) == appState->hNextViewer)
            appState->hNextViewer = (HWND)lParam;
        else if (appState->hNextViewer)
            SendMessageW(appState->hNextViewer, msg, wParam, lParam);
        break;

    case WM_HOTKEY:
        LOG_INFO(L"WM_HOTKEY");
        if (wParam == HOTKEY_ID)
        {
            appState->hwndPrevWindow = GetForegroundWindow();
            PositionHistoryDialogOnMouse(hwndMain, appState->hwndHistDialog);
            ShowHistoryDialog(hwndMain, appState->hwndHistDialog);
        }
        break;

    case WM_KEYDOWN:
        LOG_INFO(L"WM_KEYDOWN");
        break;

    case WM_TRAY_ICON_CLICK:
        LOG_INFO(L"WM_TRAY_ICON_CLIC");
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
        LOG_INFO(L"WM_PAINT");

        // Fill background with system window color
        RECT rect;
        GetClientRect(hwndMain, &rect);
        // Apparently, when highword is 0, Windows interprets the next
        // value as a system color + 1
        FillRect(hdc, &rect, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));

        EndPaint(hwndMain, &ps);
    }
    break;

    case WM_DRAWITEM:
    {
        LOG_INFO(L"WM_DRAWITEM");
        DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lParam;
        if ((HWND)dis->hwndItem == appState->hwndHistDialog && dis->itemID != (UINT)-1) {
            // Background
            FillRect(dis->hDC, &dis->rcItem,
                     (HBRUSH)(dis->itemState & ODS_SELECTED ?
                              GetSysColorBrush(COLOR_HIGHLIGHT) :
                              GetSysColorBrush(COLOR_WINDOW)));

            // Get type
            ClipItem::Type type = static_cast<ClipItem::Type>(
                SendMessageW(appState->hwndHistDialog, LB_GETITEMDATA, dis->itemID, 0)
            );

            // Draw "icon" (just a colored square)
            HBRUSH hBrush = CreateSolidBrush(type == ClipItem::TYPE_TEXT ? RGB(0, 128, 255) : RGB(0, 200, 0));
            RECT rcIcon = dis->rcItem;
            rcIcon.right = rcIcon.left + 12;
            rcIcon.top += 2;
            rcIcon.bottom -= 2;
            FillRect(dis->hDC, &rcIcon, hBrush);
            DeleteObject(hBrush);

            // Get text
            wchar_t buffer[256];
            SendMessageW(appState->hwndHistDialog, LB_GETTEXT, dis->itemID, (LPARAM)buffer);

            // Draw text after icon
            SetBkMode(dis->hDC, TRANSPARENT);
            SetTextColor(dis->hDC,
                         (dis->itemState & ODS_SELECTED)
                         ? GetSysColor(COLOR_HIGHLIGHTTEXT)
                         : GetSysColor(COLOR_WINDOWTEXT));
            RECT rcText = dis->rcItem;
            rcText.left += 16; // space after icon
            DrawTextW(dis->hDC, buffer, -1, &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }
        return TRUE;
    }

    case WM_MEASUREITEM:
    {
        LOG_INFO(L"WM_MEASUREITEM");
        MEASUREITEMSTRUCT* mis = (MEASUREITEMSTRUCT*)lParam;
        mis->itemHeight = LINE_HEIGHT;
        return TRUE;
    }

    case WM_DESTROY:
        LOG_INFO(L"WM_DESTROY");
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
    LOG_INFO(L"Finished WndProc");
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

    Logger::instance().enableFileLogging(true);
    Logger::instance().setConsoleLogLevel(Logger::INFO);

    AppState appState = {
        NULL,
        NULL,
        NULL,
        {},
        EvictingQueue<ClipItem>(DEFAULT_CLIPBOARD_HISTORY_SIZE),
        0
    };

    LOG_INFO(L"Before CreateWindow");
    HWND hwndMain = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        wc.lpszClassName, _T("Historial de Portapapeles"),
        WS_POPUP | WS_VISIBLE | WS_BORDER,
        CW_USEDEFAULT, CW_USEDEFAULT, APP_WIDTH, APP_HEIGHT,
        NULL, NULL, hInstance, &appState
    );
    LOG_INFO(L"After CreateWindow");

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
vector<unsigned char> GetSystemClipboardData(UINT format)
{
    vector<unsigned char> raw_data;
    HANDLE hData;

    hData = GetClipboardData(format);

    if (!hData)
    {
        return raw_data;
    }

    void *pData = GlobalLock(hData);

    if (!pData)
    {
        return raw_data;
    }

    SIZE_T size = GlobalSize(hData);

    // Copy data to vector
    raw_data.assign(
        reinterpret_cast<unsigned char *>(pData),
        reinterpret_cast<unsigned char *>(pData) + size
    );

    GlobalUnlock(hData);

    return raw_data;
}

ClipItem GetSystemClipboardAsClipItem()
{
    if (!OpenClipboard(NULL))
        return CLIP_ITEM_EMPTY;

    const UINT cfRTF = RegisterClipboardFormat(L"Rich Text Format");
    const UINT cfHTML = RegisterClipboardFormat(L"HTML Format");


    if (IsClipboardFormatAvailable(CF_TEXT))
        LOG_INFO(L"CF_TEXT Available");
    if (IsClipboardFormatAvailable(CF_UNICODETEXT))
        LOG_INFO(L"CF_UNICODETEXT Available");
    if (IsClipboardFormatAvailable(cfRTF))
        LOG_INFO(L"cfRTF (Rich Text Format Available) Available");
    if (IsClipboardFormatAvailable(cfHTML))
        LOG_INFO(L"cfHTML Available");
    if (IsClipboardFormatAvailable(CF_BITMAP))
        LOG_INFO(L"CF_BITMAP Available");
    if (IsClipboardFormatAvailable(CF_DIB))
        LOG_INFO(L"CF_DIB Available");
    if (IsClipboardFormatAvailable(CF_DIBV5))
        LOG_INFO(L"CF_DIBV5 Available");
    if (IsClipboardFormatAvailable(CF_METAFILEPICT))
        LOG_INFO(L"CF_METAFILEPICT Available");
    if (IsClipboardFormatAvailable(CF_ENHMETAFILE))
        LOG_INFO(L"CF_ENHMETAFILE Available");
    if (IsClipboardFormatAvailable(CF_HDROP))
        LOG_INFO(L"CF_HDROP (File Drop List) Available");


    UINT format = 0;
    vector<unsigned char> raw_data;

    if (IsClipboardFormatAvailable(cfHTML))
    {
        format = cfHTML;
    }
    else if (IsClipboardFormatAvailable(CF_UNICODETEXT))
    {
        format = CF_UNICODETEXT;
    }
    else if (IsClipboardFormatAvailable(CF_DIBV5))
    {
        format = CF_DIBV5;
    }
    else if (IsClipboardFormatAvailable(CF_HDROP))
    {
        format = CF_HDROP;
    }
    else
    {
        return CLIP_ITEM_EMPTY;
    }

    raw_data = GetSystemClipboardData(format);

    if (raw_data.empty())
    {
        CloseClipboard();
        return CLIP_ITEM_EMPTY;
    }

    ClipItem result;
    if (format == cfHTML)
    {
        string html(VectorToString(raw_data));
        // In case of HTML, we'll be saving two formats. The formated text to paste
        // and the unicode text to display in the HistoryDialog. If we were to display
        // formated text, there would be a bunch of html <tags>
        vector<unsigned char> unicode_raw_data = GetSystemClipboardData(CF_UNICODETEXT);
        wstring text(VectorToWstring(unicode_raw_data));
        result = ClipItem(html, text);
    }
    else if (format == CF_UNICODETEXT)
    {
        wstring text(VectorToWstring(raw_data));
        result = ClipItem(text);
    }
    else if (format == CF_DIBV5)
    {
        result = ClipItem(raw_data);
    }
    else if (format == CF_HDROP)
    {
        result = ClipItem(raw_data, 0);
    }

    CloseClipboard();

    return result;
}

bool SetSystemClipboardData(const void *pData, size_t size, UINT format)
{
    HGLOBAL hClipboardMem = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!hClipboardMem)
    {
        return false;
    }

    // Lock the memory and copy data
    void *pClipboardMem = GlobalLock(hClipboardMem);
    if (!pClipboardMem)
    {
        GlobalFree(hClipboardMem);
        return false;
    }

    memcpy(pClipboardMem, pData, size);
    GlobalUnlock(hClipboardMem);

    if (SetClipboardData(format, hClipboardMem) == NULL)
    {
        GlobalFree(hClipboardMem);
        return false;
    }

    // Clipboard now owns the memory; we do NOT free hClipboardMem
    return true;
}

bool SetSystemClipboardToClipItem(const ClipItem &item)
{
    if (item.type == ClipItem::TYPE_NONE)
        return false;

    if (!OpenClipboard(NULL))
        return false;

    // Tries to empty the clipboard
    if (!EmptyClipboard())
    {
        CloseClipboard();
        return false;
    }

    const UINT cfHTML = RegisterClipboardFormat(L"HTML Format");

    switch (item.type)
    {
    // Allocate global memory for the text including null terminator
    case ClipItem::TYPE_TEXT:
    {
        const void *pData = item.PtrTextData();
        size_t size = WstringSizeInBytes(item.text);
        if (!SetSystemClipboardData(pData, size, CF_UNICODETEXT))
        {
            LOG_ERROR(L"Error Set Text");
        }
        break;
    }
    case ClipItem::TYPE_HTML:
    {
        // Make available both html and text to paste
        const void *pHtmlData = item.PtrHtmlData();
        size_t htmlSize = StringSizeInBytes(item.html);

        const void *pTextData = item.PtrTextData();
        size_t textSize = WstringSizeInBytes(item.text);

        if (!SetSystemClipboardData(pHtmlData, htmlSize, cfHTML))
        {
            LOG_ERROR(L"Error Set HTML 1");
        }
        if (!SetSystemClipboardData(pTextData, textSize, CF_UNICODETEXT))
        {
            LOG_ERROR(L"Error Set HTML 2");
        }
        break;
    }
    case ClipItem::TYPE_IMAGE:
    {
        const void *pData = item.PtrImageData();
        size_t size = item.image.size();
        if (!SetSystemClipboardData(pData, size, CF_DIBV5))
        {
            LOG_ERROR(L"Error Set Image");
        }
        break;
    }
    case ClipItem::TYPE_FILE:
    {
        // HDROP data (list of files)
        const void *pData = item.PtrFileData();
        size_t size = item.file.size();
        if (!SetSystemClipboardData(pData, size, CF_HDROP))
        {
            LOG_ERROR(L"Error Set File 1");
        }

        // Extra settings for it to work on Windows File Explorer
        DWORD effect = DROPEFFECT_COPY;
        UINT cfEffect = RegisterClipboardFormat(L"Preferred DropEffect");
        if (!SetSystemClipboardData(&effect, sizeof(effect), cfEffect))
        {
            LOG_ERROR(L"Error set File 2");
        }

        UINT cfShellIDList = RegisterClipboardFormat(L"Shell IDList Array");
        const wchar_t *pFileList = PathsFromHDROPBlob(item.file);
        vector<unsigned char> shellIDList = BuildShellIDList(pFileList);
        if (!SetSystemClipboardData(&shellIDList[0], shellIDList.size(), cfShellIDList))
        {
            LOG_ERROR(L"Error set File 3");
        }

        break;
    }
    default:
        throw std::logic_error("Invalid Clip Type");
    }

    // Clipboard now owns the memory; we do NOT free hMem
    CloseClipboard();
    return true;
}

void PasteSystemClipboard()
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

// ##### FDROP management #####

const wchar_t *PathsFromHDROPBlob(const std::vector<unsigned char> &blob)
{
    if (blob.size() < sizeof(DROPFILES))
        return NULL;

    const DROPFILES *df = reinterpret_cast<const DROPFILES *>(&blob[0]);
    if (!df->pFiles || !df->fWide) // only handle Unicode
        return NULL;

    const wchar_t *pFileList = reinterpret_cast<const wchar_t *>(&blob[df->pFiles]);

    return pFileList;
}

vector<unsigned char> BuildShellIDList(const wchar_t *pFileList)
{
    if (!pFileList)
        return vector<unsigned char>();

    std::vector<LPITEMIDLIST> pidls;
    for (const wchar_t* p = pFileList; *p != L'\0'; p += wcslen(p) + 1)
    {
        LPITEMIDLIST pidl = NULL;
        ULONG eaten = 0;
        // Supposedly the old headers (XP or previous) are wrong and
        // the next call is right. Change the .h directly if necessary
        if (SUCCEEDED(SHParseDisplayName(p, NULL, &pidl, 0, &eaten)) && pidl)
            pidls.push_back(pidl);
    }

    if (pidls.empty())
        return vector<unsigned char>();

    // Compute total size for CIDA
    size_t cidaSize = sizeof(CIDA) + (pidls.size() + 1) * sizeof(UINT);
    for (size_t i = 0; i < pidls.size(); ++i)
    {
        cidaSize += ILGetSize(pidls[i]);
    }
    cidaSize += sizeof(ITEMIDLIST);

    vector<unsigned char> result(cidaSize);

    // Fill CIDA
    CIDA *cida = reinterpret_cast<CIDA *>(&result[0]);
    cida->cidl = static_cast<UINT>(pidls.size());
    UINT *offsets = reinterpret_cast<UINT *>(cida + 1);

    size_t dataOffset = sizeof(CIDA) + (pidls.size() + 1) * sizeof(UINT);
    for (size_t i = 0; i < pidls.size(); ++i)
    {
        offsets[i] = static_cast<UINT>(dataOffset);
        size_t sz = ILGetSize(pidls[i]);
        memcpy(&result[dataOffset], pidls[i], sz);
        dataOffset += sz;
    }
    offsets[pidls.size()] = static_cast<UINT>(dataOffset); // null PIDL
    ITEMIDLIST nullID = { };
    memcpy(&result[dataOffset], &nullID, sizeof(nullID));

    // clean up PIDLs
    for (size_t i = 0; i < pidls.size(); ++i)
        ILFree(pidls[i]);

    return result;
}

// ##### GUI Dialog #####

HWND CreateHistoryDialog(HWND hwndMain, HINSTANCE hInst, AppState *appState)
{
    HWND hwndHistDialog = CreateWindowExW(
        0, L"LISTBOX", NULL,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL
        | LBS_NOTIFY | WS_TABSTOP | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS,
        0, 0, APP_WIDTH, APP_HEIGHT,
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
    UpdateHistoryDialog(hwndHistDialog, appState->ClipHistory);

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
        int idx = SendMessageW(
            hwndHistDialog,
            LB_ADDSTRING,
            0,
            reinterpret_cast<LPARAM>(it->ToString().c_str())
        );
        SendMessageW(hwndHistDialog, LB_SETITEMDATA, idx, static_cast<LPARAM>(it->type));
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
    PrintHistory(L"Manage History start", ClipHistory);

    if (idx >= 0 && static_cast<size_t>(idx) < ClipHistory.Size())
    {
        if (!SetSystemClipboardToClipItem(ClipHistory[idx]))
        {
            MessageBoxW(hwndMain, L"No se pudo establecer el Porta Papeles", L"Error", MB_OK | MB_ICONERROR);
        }
        else
        {
            PrintHistory(L"Manage History before updating", ClipHistory);
            ClipHistory.MoveIndexAsFirst(idx);
            UpdateHistoryDialog(hwndHistDialog, ClipHistory);
            PrintHistory(L"Manage History after updating", ClipHistory);
        }
    }
    HideHistoryDialog(hwndMain);

    if (hwndPrevWindow)
    {
        // If there was a Previous Window, return focus and paste clipboard
        SetForegroundWindow(hwndPrevWindow);
        PrintHistory(L"Manage History before pasting", ClipHistory);
        PasteSystemClipboard();
        PrintHistory(L"Manage History after pasting", ClipHistory);
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
        ClipHistory.MoveIndexAsFirst(idx);
    }
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
    LOG_INFO(L"Init Tray Icon START");
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwndMain;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nid.uCallbackMessage = WM_TRAY_ICON_CLICK;
    nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcsncpy(nid.szTip, L"Historial de Portapapeles", ARRAY_LENGTH(nid.szTip));
    Shell_NotifyIconW(NIM_ADD, &nid);
    LOG_INFO(L"Init Tray Icon END");

    return nid;
}

void RemoveTrayIcon(NOTIFYICONDATAW &nid)
{
    Shell_NotifyIconW(NIM_DELETE, &nid);
}
