#include "SearchAndOpenTabDialog.h"
#include "resource.h"
#include "PluginDefinition.h"
#include <vector>
#include <string>
#include <algorithm>

#ifndef NPPM_SWITCHTOBUFFER
#define NPPM_SWITCHTOBUFFER (WM_USER + 1088)
#endif

#ifndef NPPM_GETFILENAME
#define NPPM_GETFILENAME (WM_USER + 2013)
#endif

#ifndef NPPM_ACTIVATEDOC
#define NPPM_ACTIVATEDOC (WM_USER + 1018)
#endif

extern NppData nppData;

struct TabInfo {
    std::wstring filePath;
    std::wstring displayName;
    LPARAM bufferID;
    int docIndex;
    double score;
};

std::vector<TabInfo> openTabs;
std::vector<TabInfo> filteredTabs;
static WNDPROC g_originalEditProc = nullptr;

LRESULT CALLBACK EditSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    HWND hDlg = GetParent(hWnd);
    if (uMsg == WM_KEYDOWN)
    {
        if (wParam == VK_UP || wParam == VK_DOWN)
        {
            HWND hList = GetDlgItem(hDlg, IDC_TAB_LIST);
            int count = (int)SendMessage(hList, LB_GETCOUNT, 0, 0);
            if (count > 0)
            {
                int sel = (int)SendMessage(hList, LB_GETCURSEL, 0, 0);
                if (sel == LB_ERR) sel = 0;

                int nextSel = sel;
                if (wParam == VK_DOWN)
                    nextSel++;
                else
                    nextSel--;

                if (nextSel >= 0 && nextSel < count)
                {
                    SendMessage(hList, LB_SETCURSEL, nextSel, 0);
                }
            }
            return 0; // We handled it
        }
    }
    return CallWindowProc(g_originalEditProc, hWnd, uMsg, wParam, lParam);
}

void update_list_box(HWND hDlg)
{
    HWND hList = GetDlgItem(hDlg, IDC_TAB_LIST);
    SendMessage(hList, LB_RESETCONTENT, 0, 0);

    if (filteredTabs.empty())
    {
        // Check if the search box is empty or not
        int textLen = GetWindowTextLength(GetDlgItem(hDlg, IDC_SEARCH_EDIT));
        if (textLen > 0)
        {
            SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)L"No matching tabs found.");
        }
        else
        {
            SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)L"No files are open.");
        }
        return;
    }

    for (size_t i = 0; i < filteredTabs.size(); ++i)
    {
        SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)filteredTabs[i].displayName.c_str());
        SendMessage(hList, LB_SETITEMDATA, i, (LPARAM)i);
    }
}

void filter_and_sort_tabs(const std::wstring& filter)
{
    std::wstring lower_filter = filter;
    std::transform(lower_filter.begin(), lower_filter.end(), lower_filter.begin(), ::towlower);

    filteredTabs.clear();
    // Simple filter for now, will add fuzzy later
    for (const auto& tab : openTabs)
    {
        std::wstring lower_path = tab.filePath;
        std::transform(lower_path.begin(), lower_path.end(), lower_path.begin(), ::towlower);
        if (filter.empty() || lower_path.find(lower_filter) != std::wstring::npos)
        {
            filteredTabs.push_back(tab);
        }
    }
}

INT_PTR CALLBACK SearchAndOpenTabDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
    {
        // Center the dialog.
        HWND hOwner = GetParent(hDlg);
        if (hOwner == NULL) hOwner = nppData._nppHandle;
        RECT rcOwner, rcDlg;
        GetWindowRect(hOwner, &rcOwner);
        GetWindowRect(hDlg, &rcDlg);
        SetWindowPos(hDlg, NULL, rcOwner.left + (rcOwner.right - rcOwner.left - (rcDlg.right - rcDlg.left)) / 2,
            rcOwner.top + (rcOwner.bottom - rcOwner.top - (rcDlg.bottom - rcDlg.top)) / 2, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

        HWND hEdit = GetDlgItem(hDlg, IDC_SEARCH_EDIT);
        g_originalEditProc = (WNDPROC)SetWindowLongPtr(hEdit, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
        
        // Get all open tabs
        int numOpenFiles = (int)::SendMessage(nppData._nppHandle, NPPM_GETNBOPENFILES, 0, 0);

        openTabs.clear();

        if (numOpenFiles > 0)
        {
            std::vector<TCHAR> pathBuffer(MAX_PATH);
            for (int i = 0; i < numOpenFiles; ++i)
            {
                LPARAM bufferID = ::SendMessage(nppData._nppHandle, NPPM_GETBUFFERIDFROMPOS, i, 0);

                pathBuffer[0] = 0;
                ::SendMessage(nppData._nppHandle, NPPM_GETFULLPATHFROMBUFFERID, bufferID, (LPARAM)pathBuffer.data());
                
                std::wstring fullPath(pathBuffer.data());
                std::wstring searchPath;
                std::wstring displayName;

                if (!fullPath.empty())
                {
                    searchPath = fullPath;
                    size_t lastSlash = fullPath.find_last_of(L"\\/");
                    std::wstring filename = (lastSlash == std::wstring::npos) ? fullPath : fullPath.substr(lastSlash + 1);
                    displayName = filename + L" - " + fullPath;
                }
                else
                {
                    std::vector<TCHAR> nameBuffer(MAX_PATH);
                    ::SendMessage(nppData._nppHandle, NPPM_GETFILENAME, bufferID, (LPARAM)nameBuffer.data());
                    searchPath = nameBuffer.data();
                    displayName = searchPath;
                }
                
                if (!searchPath.empty())
                {
                    openTabs.push_back({searchPath, displayName, bufferID, i, 0.0});
                }
            }
        }

        filter_and_sort_tabs(L"");
        update_list_box(hDlg);

        HWND hList = GetDlgItem(hDlg, IDC_TAB_LIST);
        if (SendMessage(hList, LB_GETCOUNT, 0, 0) > 0)
        {
            SendMessage(hList, LB_SETCURSEL, 0, 0);
        }

        return (INT_PTR)TRUE;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
        {
            HWND hList = GetDlgItem(hDlg, IDC_TAB_LIST);
            int sel = (int)SendMessage(hList, LB_GETCURSEL, 0, 0);
            if (sel != LB_ERR)
            {
                int itemIndex = (int)SendMessage(hList, LB_GETITEMDATA, sel, 0);
                const TabInfo& tab = filteredTabs[itemIndex];
                ::SendMessage(nppData._nppHandle, NPPM_ACTIVATEDOC, 0, tab.docIndex);
                EndDialog(hDlg, LOWORD(wParam));
            }
            return (INT_PTR)TRUE;
        }
        case IDCANCEL:
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        case IDC_SEARCH_EDIT:
            if (HIWORD(wParam) == EN_CHANGE)
            {
                int len = GetWindowTextLength(GetDlgItem(hDlg, IDC_SEARCH_EDIT));
                std::vector<TCHAR> filterText(len + 1);
                GetDlgItemText(hDlg, IDC_SEARCH_EDIT, filterText.data(), len + 1);
                filter_and_sort_tabs(filterText.data());
                update_list_box(hDlg);

                HWND hList = GetDlgItem(hDlg, IDC_TAB_LIST);
                if (SendMessage(hList, LB_GETCOUNT, 0, 0) > 0)
                {
                    SendMessage(hList, LB_SETCURSEL, 0, 0);
                }
            }
            return (INT_PTR)TRUE;
        case IDC_TAB_LIST:
            if (HIWORD(wParam) == LBN_SELCHANGE)
            {
                // Check if the left mouse button is down, indicating a click caused the change.
                if (GetKeyState(VK_LBUTTON) & 0x8000)
                {
                    PostMessage(hDlg, WM_COMMAND, IDOK, 0);
                }
            }
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
} 