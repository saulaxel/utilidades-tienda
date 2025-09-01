#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <cstdlib>
#include <string>
#include <clocale>

using std::wstring;

#include "resource.h"

#define ARR_LENGTH(arr) (sizeof(arr) / sizeof(*(arr)))

HINSTANCE hInst;
static long long int times_calculated = 0;
static const wchar_t indicator[][128] =
{
    L"\\\\\\\\\\ \\\\\\\\\\ \\\\\\\\\\ \\\\\\\\\\ \\\\\\\\\\ \\\\\\\\\\ \\\\\\\\\\ \\\\\\\\\\ \\\\\\\\\\ \\\\\\\\\\ \\\\\\\\\\ \\\\\\\\\\ \\\\\\\\\\ \\\\\\\\\\ \\\\\\\\\\ \\\\\\\\\\ \\\\\\\\\\ \\\\\\\\\\ \\\\\\\\\\ \\\\\\\\\\",
    L"||||| ||||| ||||| ||||| ||||| ||||| ||||| ||||| ||||| ||||| ||||| ||||| ||||| ||||| ||||| ||||| ||||| ||||| ||||| |||||",
    L"///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////",
    L"----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- -----",
};

BOOL IsEven(int n)
{
    return n % 2 == 0;
}

BOOL ParseInt(wchar_t buffer[], int* target)
{
    wchar_t* end = NULL;
    long value = wcstol(buffer, &end, 10);

    if (end == buffer)
    {
        // Conversion failed
        return FALSE;
    }

    *target = value;
    return TRUE;
}

void AppendFormattedInt(const wchar_t *format, wstring &buffer, int num)
{
    wchar_t aux[128];
    snwprintf(aux, 128, format, num);
    buffer += aux;
}

void DescendingSequence(wstring &buffer, int start, int end)
{
    buffer = L"";

    // First number doesn't have a comma
    AppendFormattedInt(L"%d", buffer, start);

    for (int i = start - 2; i >= end; i -= 2)
    {
        // Rest of the numbers have comma
        AppendFormattedInt(L",%d", buffer, i);
    }
}

void AscendingSequence(wstring &buffer, int start, int end)
{
    buffer = L"";

    // First number doesn't have a comma
    AppendFormattedInt(L"%d", buffer, start);

    for (int i = start + 2; i <= end; i += 2)
    {
        // Rest of the numbers have comma
        AppendFormattedInt(L",%d", buffer,i);
    }
}

int NumberOfPagesInInterval(int start_page, int end_page)
{
    return std::abs(end_page - start_page) + 1;
}

BOOL CALLBACK DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg)
    {
        case WM_INITDIALOG:
            /*
             * TODO: Add code to initialize the dialog.
             */
            return TRUE;

        case WM_CLOSE:
            EndDialog(hwndDlg, 0);
            return TRUE;

        case WM_COMMAND:
            switch(LOWORD(wParam))
            {

                case IDC_BTN_CALC:
                    wchar_t buffer[256];
                    int start_page, end_page;
                    wstring front_pages, back_pages, lone_page = L"";

                    times_calculated++;

                    GetDlgItemTextW(hwndDlg, IDC_EDIT_START_PAGE, buffer, ARR_LENGTH(buffer));
                    if (!ParseInt(buffer, &start_page) || start_page <= 0)
                    {
                        MessageBoxW(hwndDlg, L"Invalid Start Number", L"Error", MB_ICONERROR);
                        return TRUE;
                    }

                    GetDlgItemTextW(hwndDlg, IDC_EDIT_END_PAGE, buffer, sizeof(buffer)/sizeof(*buffer));
                    if (!ParseInt(buffer, &end_page) || end_page <= 0)
                    {
                        MessageBoxW(hwndDlg, L"Invalid End Number", L"Error", MB_ICONERROR);
                        return TRUE;
                    }

                    if (start_page >= end_page)
                    {
                        MessageBoxW(hwndDlg, L"Start Page must be less than End Page", L"Error", MB_ICONERROR);
                        return TRUE;
                    }

                    // Generating the sequences of comma separated numbers
                    if (IsEven(NumberOfPagesInInterval(start_page, end_page)))
                    {
                        AscendingSequence(front_pages, start_page, end_page - 1);
                        DescendingSequence(back_pages, end_page, start_page);
                    }
                    else
                    {
                        AscendingSequence(front_pages, start_page, end_page - 2);
                        DescendingSequence(back_pages, end_page - 1, start_page);
                        MessageBoxW(hwndDlg, L"LA ULTIMA PAGINA NO TENDRIA REVERSO. IMPRIMIRLA POR SEPARADO MAS TARDE.", L"Aviso", MB_ICONINFORMATION);
                        AppendFormattedInt(L"%d", lone_page, end_page);
                    }

                    HWND hFrontPages = GetDlgItem(hwndDlg, IDC_EDIT_FRONT_PAGES);
                    HWND hBackPages = GetDlgItem(hwndDlg, IDC_EDIT_BACK_PAGES);
                    HWND hLonePage = GetDlgItem(hwndDlg, IDC_EDIT_LONE_PAGE);

                    SetWindowTextW(hFrontPages, front_pages.c_str());
                    SetWindowTextW(hBackPages, back_pages.c_str());
                    SetWindowTextW(hLonePage, lone_page.c_str());
                    SetDlgItemTextW(hwndDlg, IDC_LTEXT_NOTICE, indicator[times_calculated % ARR_LENGTH(indicator)]);
                    return TRUE;
            }
    }

    return FALSE;
}


int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    hInst = hInstance;
    setlocale(LC_ALL, "");

    // The user interface is a modal dialog box
    return DialogBox(hInstance, MAKEINTRESOURCE(DLG_MAIN), NULL, (DLGPROC)DialogProc);
}
