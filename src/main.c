#include <Windows.h>
#include <windowsx.h>
#include <tchar.h>
#include <stdio.h>
#include <commctrl.h>
#include <tlhelp32.h>
#include <shobjidl_core.h>

#ifdef UNICODE
#define _WinMain wWinMain
#else
#define _WinMain WinMain
#endif

#define CONSOLE

#ifdef CONSOLE
#define log_msg(msg, ...) _tprintf(TEXT(msg) TEXT("\n"), __VA_ARGS__)
#else
#define log_msg(msg, ...)
#endif

#define WNDCLASSNAME TEXT("SimpleInjector_WndClass")
#define WINDOW_WIDTH 400
#define WINDOW_HEIGHT 600

#define IMAGERES_REFRESH_ICON_ID 229

#define BUTTON_REFRESH_PROCESSES 1
#define BUTTON_SELECT_DLL        2
#define BUTTON_INJECT            3


typedef struct {
    HINSTANCE hInstance;
    HWND hWnd;
    HWND hProcessListView;
    HWND hSelectedDllLabel;
    HWND hSelectedDllIcon;
    HWND hSelectedProcessLabel;
    HWND hInjectButton;
    FILE *console;
    int exitCode;
    BOOL hasDllSelected;
    TCHAR selectedDllPath[MAX_PATH];
    DWORD selectedPID;
} InjectorCtx;

typedef struct {
    TCHAR path[MAX_PATH];
    int imageIndex;
} IconCacheEntry;

typedef struct {
    IconCacheEntry *items;
    int count;
    int capacity;
} IconCache;

int Init(InjectorCtx*);
int MainLoop(void);
void Cleanup(InjectorCtx*);

#ifdef CONSOLE
int InitConsole(InjectorCtx*);
#endif
int InitWindow(InjectorCtx*);
int InitUI(HWND, InjectorCtx*);
int InitProcessListView(HWND, DWORD);
int UpdateListViewProcesses(HWND, DWORD);
int GetProcessIcon(HANDLE hProcess, HIMAGELIST hImageList, IconCache *ic);
void CleanupProcessListView(HWND);

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

int GetDLLPath(InjectorCtx*);

IconCache *CreateIconCache(void);
void DestroyIconCache(IconCache *ic);
int InsertIconToCache(IconCache *ic, const TCHAR *path, int imageIndex);
IconCacheEntry *FindIconInCache(IconCache *ic, const TCHAR *path);



int WINAPI _WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPTSTR lpCmdLine, _In_ int nShowCmd) {
    InjectorCtx ctx = { 0 };
    ctx.selectedPID = -1;

    ctx.hInstance = hInstance;

    if (Init(&ctx) != 0) {
        Cleanup(&ctx);
        return 1;
    }

    int ret = MainLoop();

    Cleanup(&ctx);

	return ret;
}

int Init(InjectorCtx* ctx) {

    INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX), ICC_LISTVIEW_CLASSES };
    InitCommonControlsEx(&icex);

#ifdef CONSOLE
    if (InitConsole(ctx) != 0) return 1;
#endif
    if (InitWindow(ctx) != 0) return 1;

    return 0;
}

int MainLoop(void) {
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (msg.message == WM_QUIT)
            return (int)msg.wParam;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}

void Cleanup(InjectorCtx* ctx) {
    if (ctx->hWnd)
        DestroyWindow(ctx->hWnd);
    UnregisterClass(WNDCLASSNAME, ctx->hInstance);

#ifdef CONSOLE
    if (ctx->console) {
        fclose(ctx->console);
        FreeConsole();
    }
#endif
}

#ifdef CONSOLE
int InitConsole(InjectorCtx* ctx) {
    if (!AllocConsole()) return 1;
    freopen_s(&ctx->console, "CONOUT$", "w", stdout);
    if (!ctx->console) return 1;
    if (ferror(ctx->console)) return 1;

    return 0;
}
#endif

int InitWindow(InjectorCtx *ctx) {
    WNDCLASSEX wc = { 0 };
    wc.cbSize = sizeof(wc);
    wc.style = 0;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = ctx->hInstance;
    wc.hIcon = NULL;
    wc.hCursor = NULL;
    wc.hbrBackground = NULL;
    wc.lpszMenuName = NULL;
    wc.lpszClassName = WNDCLASSNAME;
    wc.hIconSm = NULL;

    RegisterClassEx(&wc);

    RECT wr;
    wr.left = 100;
    wr.top = 100;
    wr.right = WINDOW_WIDTH + wr.left;
    wr.bottom = WINDOW_HEIGHT + wr.top;

    if (!AdjustWindowRect(&wr, WS_CAPTION | WS_MINIMIZEBOX | WS_SYSMENU, FALSE)) {
        log_msg("Failed to AdjustWindowRect");
        return 1;
    }

    ctx->hWnd = CreateWindow(
        WNDCLASSNAME,
        TEXT("SimpleInjector"),
        WS_CAPTION | WS_MINIMIZEBOX | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left, wr.bottom - wr.top,
        NULL,
        NULL,
        ctx->hInstance,
        ctx);


    if (!ctx->hWnd) {
        log_msg("Failed to create window");
        return 1;
    }

    return 0;
}

int InitUI(HWND hWnd, InjectorCtx *ctx) {
    ctx->hProcessListView = CreateWindow(
        WC_LISTVIEW,
        NULL,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL,
        10, 45, WINDOW_WIDTH - 20, 340,
        hWnd,
        NULL,
        NULL,
        NULL
    );

    if (ctx->hProcessListView == NULL) {
        log_msg("Failed to Create ListView");
        return 1;
    }

    if (InitProcessListView(ctx->hProcessListView, -1) != 0) return 1;

    CreateWindow(WC_STATIC, TEXT("Simple Injector"), WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE, 150, 5, WINDOW_WIDTH - 50, 35, hWnd, NULL, NULL, NULL);

    HWND hButton = CreateWindow(
        WC_BUTTON,
        NULL,
        WS_CHILD | WS_VISIBLE | BS_ICON | BS_TEXT,
        10, 5, 35, 35,
        hWnd,
        (HMENU)BUTTON_REFRESH_PROCESSES,
        NULL,
        NULL
    );

    HICON iconLarge = NULL;
    ExtractIconEx(TEXT("C:\\Windows\\System32\\imageres.dll"), IMAGERES_REFRESH_ICON_ID, &iconLarge, NULL, 1);
    if (iconLarge) SendMessage(hButton, BM_SETIMAGE, IMAGE_ICON, (LPARAM)iconLarge);
    else {
        SetWindowPos(hButton, HWND_TOP, 0, 0, 100, 35, SWP_NOMOVE);
        SendMessage(hButton, WM_SETTEXT, NULL, TEXT("Refresh"));
    }

    ctx->hSelectedProcessLabel = CreateWindow(
        WC_STATIC,
        TEXT("Selected Process: -"),
        WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE,
        10, 400, 350, 35,
        hWnd, NULL, NULL, NULL
    );

    hButton = CreateWindow(
        WC_BUTTON,
        TEXT("Select DLL"),
        WS_CHILD | WS_VISIBLE,
        10, 450, 100, 35,
        hWnd,
        (HMENU)BUTTON_SELECT_DLL,
        NULL,
        NULL
    );

    ctx->hSelectedDllLabel = CreateWindow(
        WC_STATIC,
        NULL,
        WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE,
        165, 450, 100, 35,
        hWnd, NULL, NULL, NULL
    );

    ctx->hSelectedDllIcon = CreateWindow(
        WC_STATIC,
        NULL,
        WS_CHILD | SS_ICON,
        130, 450, 35, 35,
        hWnd, NULL, NULL, NULL
    );

    SHFILEINFO sfi = { 0 };
    SHGetFileInfo(TEXT(".dll"), FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi),
        SHGFI_USEFILEATTRIBUTES | SHGFI_ICON);
    
    SendMessage(ctx->hSelectedDllIcon, STM_SETICON, (WPARAM)sfi.hIcon, 0);

    ctx->hInjectButton = CreateWindow(
        WC_BUTTON,
        TEXT("Inject"),
        WS_CHILD | WS_VISIBLE  | WS_DISABLED,
        10, 500, 100, 35,
        hWnd,
        (HMENU)BUTTON_INJECT,
        NULL,
        NULL
    );

    return 0;
}

int InitProcessListView(HWND hListView, DWORD selectPID) {
    ListView_SetExtendedListViewStyle(hListView, LVS_EX_FULLROWSELECT | LVS_EX_AUTOSIZECOLUMNS | LVS_EX_INFOTIP);

    IconCache *ic = CreateIconCache();

    SetWindowLongPtr(hListView, GWLP_USERDATA, (LONG_PTR)ic);

    HIMAGELIST hImageList = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 1, 1);

    ListView_SetImageList(hListView, hImageList, LVSIL_SMALL);
 
    TCHAR *lvCols[] = {
        TEXT("Process Name"),
        TEXT("Arch"),
        TEXT("PID"),
    };

    LVCOLUMN lvc = {.mask = LVCF_FMT  | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM};

    for (int i = 0; i < (sizeof(lvCols) / sizeof(*lvCols)); i++) {
        lvc.iSubItem = i;
        lvc.pszText = lvCols[i];
        lvc.cx = i == 0 ? WINDOW_WIDTH - 19 * 2 - 100 * 2 : 100;
        lvc.fmt = i == 1 ? LVCFMT_CENTER : LVCFMT_LEFT;

        ListView_InsertColumn(hListView, i, &lvc);
    }

    if (UpdateListViewProcesses(hListView, selectPID) != 0) return 1;

    return 0;
}

int UpdateListViewProcesses(HWND hListView, DWORD selectPID) {
    ListView_DeleteAllItems(hListView);
    HIMAGELIST hImageList = ListView_GetImageList(hListView, LVSIL_SMALL);
    IconCache *ic = (IconCache*)GetWindowLongPtr(hListView, GWLP_USERDATA);

    LVITEM lvi = {.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM};

    TCHAR pidStr[16];
   
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        MessageBox(NULL, TEXT("Failed to create process snapshot!"), TEXT("Error"), MB_OK | MB_ICONERROR);
        return 1;
    }

    PROCESSENTRY32 pe32 = { sizeof(PROCESSENTRY32) };
    if (Process32First(hSnapshot, &pe32)) {
        int index = 0;
        do {
            lvi.iItem = index;
            lvi.iSubItem = 0;
            lvi.pszText = pe32.szExeFile;
            lvi.iImage = 0;
            lvi.lParam = pe32.th32ProcessID;

            HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe32.th32ProcessID);

            if (hProcess) {
                // Get process icon
                lvi.iImage = GetProcessIcon(hProcess, hImageList, ic);
            }

            ListView_InsertItem(hListView, &lvi);

            if (hProcess) {
                //Add Arch
                USHORT machine;
                if (IsWow64Process2(hProcess, &machine, NULL))
                    ListView_SetItemText(hListView, index, 1, machine == IMAGE_FILE_MACHINE_I386 ? TEXT("x86") : TEXT("x64"));

                CloseHandle(hProcess);
            }
            
            // Add PID
            _stprintf_s(pidStr, sizeof(pidStr) / sizeof(*pidStr), TEXT("%lu"), pe32.th32ProcessID);
            ListView_SetItemText(hListView, index, 2, pidStr);
            if (pe32.th32ProcessID == selectPID) {
                ListView_SetItemState(hListView, index, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            }

            index++;
        } while (Process32Next(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);

    return 0;
}

int GetProcessIcon(HANDLE hProcess, HIMAGELIST hImageList, IconCache *ic) {
    TCHAR exePath[MAX_PATH];
    DWORD size = MAX_PATH;
    int iconIndex = 0;

    if (!QueryFullProcessImageName(hProcess, 0, exePath, &size))
        return 0; // default icon

    IconCacheEntry *ice = FindIconInCache(ic, exePath);

    if (ice != NULL) {
        return ice->imageIndex;
    }

    SHFILEINFO sfi = { 0 };
    if (SHGetFileInfo(exePath, 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_SMALLICON) && sfi.hIcon) {
        iconIndex = ImageList_AddIcon(hImageList, sfi.hIcon);
        DestroyIcon(sfi.hIcon);
        InsertIconToCache(ic, exePath, iconIndex);
    }

    return iconIndex;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static HBRUSH hBrush = NULL;
    InjectorCtx *ctx = GetWindowLongPtr(hWnd, GWLP_USERDATA);
    switch (uMsg) {
    case WM_CREATE:
    {
        CREATESTRUCT *cs = (CREATESTRUCT*)lParam;
        ctx = cs->lpCreateParams;
        SetWindowLongPtr(hWnd, GWLP_USERDATA, ctx);
        if (InitUI(hWnd, ctx) != 0) {
            ctx->exitCode = EXIT_FAILURE;
            return -1;
        }
    }
        break;
    case WM_COMMAND:
        switch (wParam) {
        case BUTTON_REFRESH_PROCESSES:
            UpdateListViewProcesses(ctx->hProcessListView, ctx->selectedPID);
            break;
        case BUTTON_SELECT_DLL: {
            if (GetDLLPath(ctx)) {
                ctx->hasDllSelected = TRUE;
                ShowWindow(ctx->hSelectedDllIcon, SW_SHOW);
                Static_SetText(ctx->hSelectedDllLabel, _tcsrchr(ctx->selectedDllPath, TEXT('\\')) + 1);
                
                if (ctx->selectedPID != -1)
                    EnableWindow(ctx->hInjectButton, TRUE);
            }
        }
        break;
        case BUTTON_INJECT: {
            int iPos = ListView_GetNextItem(ctx->hProcessListView, -1, LVNI_SELECTED);
            log_msg("iPos: %d", iPos);
            if (iPos != -1) {
                LVITEM lvi = { 0 };
                lvi.mask = LVIF_PARAM;
                lvi.iItem = iPos;
                ListView_GetItem(ctx->hProcessListView, &lvi);
                log_msg("Selected Process PID: %d", (int)lvi.lParam);
            }
        }
        break;
        }
        break;
    case WM_NOTIFY: {
        LPNMHDR hdr = (LPNMHDR)lParam;

        if (hdr->hwndFrom == ctx->hProcessListView && hdr->code == LVN_ITEMCHANGED) {
            NMLISTVIEW *pnmv = (NMLISTVIEW *)lParam;

            // Check if the selection state changed
            if ((pnmv->uChanged & LVIF_STATE) &&
                (pnmv->uNewState & LVIS_SELECTED) &&
                !(pnmv->uOldState & LVIS_SELECTED)) {

                int selectedIndex = pnmv->iItem;
                TCHAR buf[256];
                TCHAR processName[128];
                ListView_GetItemText(ctx->hProcessListView, pnmv->iItem, 0, processName, sizeof(processName) / sizeof(*processName));
                _stprintf_s(buf, sizeof(buf) / sizeof(*buf), TEXT("Selected Process: %s"), processName);
                Static_SetText(ctx->hSelectedProcessLabel, buf);
                
                LVITEM lvi = { 0 };
                lvi.mask = LVIF_PARAM;
                lvi.iItem = pnmv->iItem;
                ListView_GetItem(ctx->hProcessListView, &lvi);

                ctx->selectedPID = lvi.lParam;
                
                if (ctx->hasDllSelected)
                    EnableWindow(ctx->hInjectButton, TRUE);
            }
        }
        break;
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdcStatic = (HDC)wParam;

        // Set the background mode to transparent
        SetBkMode(hdcStatic, TRANSPARENT);

        // Return the parent window's background brush
        if (!hBrush) {
            hBrush = CreateSolidBrush(GetSysColor(COLOR_WINDOW)); // Matches default background
        }
        return (LRESULT)hBrush;
    }
    case WM_DESTROY:
        CleanupProcessListView(ctx->hProcessListView);
        PostQuitMessage(ctx->exitCode);
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

int GetDLLPath(InjectorCtx *ctx) {
    OPENFILENAME ofn = { 0 };

    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = ctx->hWnd;
    ofn.lpstrFile = ctx->selectedDllPath;
    ofn.nMaxFile = sizeof(ctx->selectedDllPath) / sizeof(*ctx->selectedDllPath);
    ofn.lpstrFilter = TEXT("DLL Files\0*.dll\0"); // File filters
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    return GetOpenFileName(&ofn);
}

IconCache *CreateIconCache(void) {
    IconCache *ic = malloc(sizeof(IconCache));
    if (ic == NULL) return NULL;

    ic->capacity = 512;
    ic->count = 0;
    ic->items = malloc(ic->capacity * sizeof(IconCacheEntry));

    return ic;
}

void DestroyIconCache(IconCache *ic) {
    free(ic->items);
    ic->capacity = 0;
    ic->count = 0;
}

int InsertIconToCache(IconCache *ic, const TCHAR *path, int imageIndex) {
    if (ic->count >= ic->capacity) {
        ic->capacity *= 2;
        void *tmp = realloc(ic->items, ic->capacity * sizeof(IconCacheEntry));
        if (tmp == NULL) return 1;
        ic->items = tmp;
    }

    int left = 0, right = ic->count - 1, mid, cmp, insertPos = 0;
    while (left <= right) {
        mid = (left + right) / 2;
        cmp = _tcscmp(path, ic->items[mid].path);
        if (cmp == 0) return 0; // already cached;
        else if (cmp < 0) right = mid - 1;
        else left = mid + 1;
    }
    insertPos = left;

    memmove(&ic->items[insertPos + 1], &ic->items[insertPos], (ic->count - insertPos) * sizeof(IconCacheEntry));

    _tcscpy_s(ic->items[insertPos].path, MAX_PATH, path);
    ic->items[insertPos].imageIndex = imageIndex;
    ic->count++;

    return 0;
}

IconCacheEntry *FindIconInCache(IconCache *ic, const TCHAR *path) {
    int left = 0, right = ic->count - 1, mid, cmp;

    while (left <= right) {
        mid = (left + right) / 2;
        cmp = _tcscmp(path, ic->items[mid].path);

        if (cmp == 0)
            return &ic->items[mid]; // found

        if (cmp < 0)
            right = mid - 1;
        else
            left = mid + 1;
    }

    return NULL;
}

void CleanupProcessListView(HWND hListView) {
    if (hListView) {
        IconCache *ic = GetWindowLongPtr(hListView, GWLP_USERDATA);
        if (ic) DestroyIconCache(ic);
    }
}
