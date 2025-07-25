#include <Windows.h>
#include <tchar.h>
#include <stdio.h>
#include <commctrl.h>
#include <tlhelp32.h>

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

typedef struct {
    HINSTANCE hInstance;
    HWND hWnd;
    FILE *console;
} InjectorCtx;

int Init(InjectorCtx*);
int MainLoop(void);
void Cleanup(InjectorCtx*);

#ifdef CONSOLE
int InitConsole(InjectorCtx*);
#endif
int InitWindow(InjectorCtx*);
int InitUI(HWND);

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

int WINAPI _WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPTSTR lpCmdLine, _In_ int nShowCmd) {
    InjectorCtx ctx = { 0 };
    
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

int InitUI(HWND hWnd) {
    HWND hListView = CreateWindow(
        WC_LISTVIEW,
        NULL,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS,
        10, 10, WINDOW_WIDTH - 10, 340,
        hWnd,
        NULL,
        NULL,
        NULL
    );

    HIMAGELIST hImageList = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 1, 1);

    ListView_SetImageList(hListView, hImageList, LVSIL_SMALL);

    LVCOLUMN lvc;
    int iCol = 0;

    lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;

    lvc.iSubItem = iCol;
    lvc.pszText = TEXT("Process Name");
    lvc.cx = 100;
    lvc.fmt = LVCFMT_LEFT;

    ListView_InsertColumn(hListView, iCol++, &lvc);

    lvc.iSubItem = iCol;
    lvc.pszText = TEXT("Architechture");
    lvc.cx = 100;
    lvc.fmt = LVCFMT_CENTER;

    ListView_InsertColumn(hListView, iCol++, &lvc);

    lvc.iSubItem = iCol;
    lvc.pszText = TEXT("PID");
    lvc.cx = 100;
    lvc.fmt = LVCFMT_LEFT;

    ListView_InsertColumn(hListView, iCol++, &lvc);

    LVITEM lvi;
    lvi.mask = LVIF_TEXT | LVIF_IMAGE;
    lvi.iImage = 0;
    
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        MessageBox(hWnd, TEXT("Failed to create process snapshot!"), TEXT("Error"), MB_OK | MB_ICONERROR);
        return 0;
    }

    PROCESSENTRY32 pe32 = { sizeof(PROCESSENTRY32) };
    if (Process32First(hSnapshot, &pe32)) {
        int index = 0;
        do {
            lvi.iItem = index;
            lvi.iSubItem = 0;
            lvi.pszText = pe32.szExeFile;
            ListView_InsertItem(hListView, &lvi);
            
            //Add Arch
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe32.th32ProcessID);
            if (hProcess) {
                USHORT machine;
                if (IsWow64Process2(hProcess, &machine, NULL))
                   ListView_SetItemText(hListView, index, 1, machine == IMAGE_FILE_MACHINE_I386 ? TEXT("x86") : TEXT("x64"));
                
                TCHAR exePath[MAX_PATH] = { 0 };
                DWORD size = MAX_PATH;
                int iconIndex = 0;

                if (QueryFullProcessImageName(hProcess, 0, exePath, &size)) {
                    SHFILEINFO sfi = { 0 };
                    if (SHGetFileInfo(exePath, 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_SMALLICON) && sfi.hIcon) {
                        iconIndex = ImageList_AddIcon(hImageList, sfi.hIcon);
                        DestroyIcon(sfi.hIcon);
                    }
                }

                lvi.iImage = iconIndex;

                CloseHandle(hProcess);
            }

            // Add PID
            TCHAR pidStr[16];
            _stprintf_s(pidStr, sizeof(pidStr) / sizeof(*pidStr), TEXT("%lu"), pe32.th32ProcessID);
            ListView_SetItemText(hListView, index, 2, pidStr);

            index++;
        } while (Process32Next(hSnapshot, &pe32));
    }
    CloseHandle(hSnapshot);

    return 0;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
    {
        CREATESTRUCT *cs = (CREATESTRUCT*)lParam;
        if (InitUI(hWnd) != 0)
            PostQuitMessage(1);
    }
        break;
    case WM_CLOSE:
        PostQuitMessage(0);
        break;
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}