/*
 * BLUETTI Device Studio V1.0 - Official Release
 * Native Windows x86 application for Serial, CANalyst-II and BLE connection.
 * Editors: zrj / ChatGPT (OpenAI)
 * Release baseline: V1.3.9 final verified test build.
 * Apple-inspired light/dark UI with real serial communication, IMAGE_HEADER_T parsing,
 * Modbus OTA start handshake and XMODEM-1K firmware transfer.
 *
 * Serial protocol implemented:
 *  1. Modbus 0x10 writes registers 700..705, retries every 5 s, max 5 attempts.
 *  2. Waits up to 15 s for receiver character 'C' (0x43).
 *  3. Sends 1029-byte XMODEM-1K packets, waits ACK/NAK, 5 s timeout, max 5 attempts.
 *  4. Sends EOT (0x04), waits ACK, 5 s timeout, max 5 attempts.
 *  5. Supports repeat-upgrade testing with a user-configurable interval.
 */

typedef void* HANDLE;
typedef void* LPVOID;
typedef HANDLE HWND;
typedef HANDLE HINSTANCE;
typedef HANDLE HICON;
typedef HANDLE HCURSOR;
typedef HANDLE HBRUSH;
typedef HANDLE HMODULE;
typedef void* FARPROC;
typedef HANDLE HFONT;
typedef HANDLE HGDIOBJ;
typedef HANDLE HDC;
typedef unsigned char BYTE;
#ifdef _WIN64
typedef unsigned long long SIZE_T;
#else
typedef unsigned int SIZE_T;
#endif

/*
 * Local CRT-free memory helpers. Optimized MSVC/clang-cl builds may lower
 * simple byte-copy loops to memcpy/memset calls even when the application
 * deliberately does not link the C runtime. Volatile accesses keep these
 * implementations self-contained.
 */
__declspec(noinline) void* memset(void* destination, int value, SIZE_T count)
{
    volatile BYTE* output = (volatile BYTE*)destination;
    while (count > 0)
    {
        *output = (BYTE)value;
        output++;
        count--;
    }
    return destination;
}

__declspec(noinline) void* memcpy(void* destination, const void* source, SIZE_T count)
{
    volatile BYTE* output = (volatile BYTE*)destination;
    const volatile BYTE* input = (const volatile BYTE*)source;
    while (count > 0)
    {
        *output = *input;
        output++;
        input++;
        count--;
    }
    return destination;
}
typedef char CHAR;
typedef unsigned short WORD;
typedef unsigned long DWORD;
typedef unsigned int UINT;
#ifdef _WIN64
typedef unsigned long long ULONG_PTR;
typedef unsigned long long UINT_PTR;
typedef unsigned long long WPARAM;
typedef long long LPARAM;
typedef long long LRESULT;
#else
typedef unsigned long ULONG_PTR;
typedef unsigned long UINT_PTR;
typedef unsigned long WPARAM;
typedef long LPARAM;
typedef long LRESULT;
#endif
typedef long LONG;
typedef int BOOL;
typedef unsigned long COLORREF;
typedef unsigned short wchar_t;
typedef const wchar_t* LPCWSTR;
typedef wchar_t* LPWSTR;
typedef unsigned short ATOM;
typedef long long LONGLONG;
typedef unsigned short USHORT;

#define WINAPI __stdcall
#define CALLBACK __stdcall
#define NULL ((void*)0)
#define TRUE 1
#define FALSE 0
#define INVALID_HANDLE_VALUE ((HANDLE)(long long)-1)
#define RGB(r,g,b) ((COLORREF)(((BYTE)(r)) | ((WORD)((BYTE)(g)) << 8) | (((DWORD)(BYTE)(b)) << 16)))

#define CS_HREDRAW 0x0002
#define CS_VREDRAW 0x0001
#define WS_OVERLAPPEDWINDOW 0x00CF0000L
#define WS_VISIBLE 0x10000000L
#define WS_CHILD 0x40000000L
#define WS_TABSTOP 0x00010000L
#define WS_VSCROLL 0x00200000L
#define WS_BORDER 0x00800000L
#define WS_CLIPSIBLINGS 0x04000000L
#define CBS_DROPDOWNLIST 0x0003L
#define CBS_OWNERDRAWFIXED 0x0010L
#define CBS_HASSTRINGS 0x0200L
#define ES_NUMBER 0x2000L
#define ES_MULTILINE 0x0004L
#define ES_AUTOVSCROLL 0x0040L
#define ES_READONLY 0x0800L
#define CW_USEDEFAULT ((int)0x80000000)
#define SW_HIDE 0
#define SW_SHOW 5
#define SW_SHOWNORMAL 1
#define IDC_ARROW ((LPCWSTR)32512)
#define IDC_HAND ((LPCWSTR)32649)

#define WM_CREATE 0x0001
#define WM_DESTROY 0x0002
#define WM_SIZE 0x0005
#define WM_COMMAND 0x0111
#define WM_TIMER 0x0113
#define WM_DRAWITEM 0x002B
#define WM_MEASUREITEM 0x002C
#define WM_PAINT 0x000F
#define WM_ERASEBKGND 0x0014
#define WM_MOUSEMOVE 0x0200
#define WM_MOUSELEAVE 0x02A3
#define WM_LBUTTONUP 0x0202
#define WM_SETCURSOR 0x0020
#define WM_SETFONT 0x0030
#define WM_CTLCOLORSTATIC 0x0138
#define WM_CTLCOLOREDIT 0x0133
#define WM_CTLCOLORLISTBOX 0x0134
#define WM_CTLCOLORBTN 0x0135
#define WM_APP 0x8000
#define WM_APP_UPGRADE_REFRESH (WM_APP + 1)
#define WM_APP_UPGRADE_DONE    (WM_APP + 2)

#define DT_LEFT 0x0000
#define DT_CENTER 0x0001
#define DT_RIGHT 0x0002
#define DT_VCENTER 0x0004
#define DT_SINGLELINE 0x0020
#define DT_WORDBREAK 0x0010
#define DT_NOPREFIX 0x0800
#define DT_END_ELLIPSIS 0x8000
#define TRANSPARENT 1
#define FW_NORMAL 400
#define FW_MEDIUM 500
#define FW_SEMIBOLD 600
#define FW_BOLD 700
#define CLEARTYPE_QUALITY 5
#define DEFAULT_CHARSET 1
#define OUT_DEFAULT_PRECIS 0
#define CLIP_DEFAULT_PRECIS 0
#define DEFAULT_PITCH 0
#define FF_DONTCARE 0
#define DC_BRUSH 18
#define DC_PEN 19
#define TME_LEAVE 0x00000002
#define MB_OK 0x00000000L
#define MB_ICONINFORMATION 0x00000040L
#define MB_ICONWARNING 0x00000030L
#define MB_ICONERROR 0x00000010L
#define COLOR_WINDOW 5

#define CB_ADDSTRING 0x0143
#define CB_GETCOUNT 0x0146
#define CB_GETCURSEL 0x0147
#define CB_GETLBTEXT 0x0148
#define CB_SETCURSEL 0x014E
#define CB_RESETCONTENT 0x014B
#define CBN_SELCHANGE 1
#define EN_CHANGE 0x0300
#define EM_SETLIMITTEXT 0x00C5
#define ODT_COMBOBOX 3
#define ODS_SELECTED 0x0001
#define ODS_FOCUS 0x0010
#define ODS_COMBOBOXEDIT 0x1000

#define GENERIC_READ 0x80000000UL
#define GENERIC_WRITE 0x40000000UL
#define OPEN_EXISTING 3
#define CREATE_ALWAYS 2
#define FILE_SHARE_READ 0x00000001UL
#define FILE_SHARE_WRITE 0x00000002UL
#define CREATE_NO_WINDOW 0x08000000UL
#define FILE_ATTRIBUTE_NORMAL 0x00000080UL
#define FILE_BEGIN 0
#define WAIT_OBJECT_0 0
#define WAIT_TIMEOUT 258
#define INFINITE 0xFFFFFFFFUL
#define MEM_COMMIT 0x00001000UL
#define MEM_RESERVE 0x00002000UL
#define MEM_RELEASE 0x00008000UL
#define PAGE_READWRITE 0x04UL
#define SRCCOPY 0x00CC0020UL
#define PURGE_TXABORT 0x0001
#define PURGE_RXABORT 0x0002
#define PURGE_TXCLEAR 0x0004
#define PURGE_RXCLEAR 0x0008
#define NOPARITY 0
#define ONESTOPBIT 0

#define OFN_FILEMUSTEXIST 0x00001000
#define OFN_PATHMUSTEXIST 0x00000800
#define OFN_EXPLORER 0x00080000
#define OFN_NOCHANGEDIR 0x00000008
#define OFN_ALLOWMULTISELECT 0x00000200

#define PS_SOLID 0
#define GET_X_LPARAM(lp) ((int)(short)((lp) & 0xFFFF))
#define GET_Y_LPARAM(lp) ((int)(short)(((lp) >> 16) & 0xFFFF))
#define LOWORD(l) ((WORD)((UINT_PTR)(l) & 0xFFFF))
#define HIWORD(l) ((WORD)((UINT_PTR)(l) >> 16))

#define ID_COM_COMBO 1001
#define ID_BAUD_COMBO 1002
#define ID_REPEAT_EDIT 1003
#define ID_WAIT_EDIT 1004
#define ID_CHIP_COMBO 1005
#define ID_CAN_DEVICE_COMBO 1101
#define ID_CAN_CHANNEL_COMBO 1102
#define ID_CAN_BAUD_COMBO 1103
#define ID_CAN_LOCAL_EDIT 1104
#define ID_CAN_TARGET_EDIT 1105
#define ID_CAN_REPEAT_EDIT 1106
#define ID_CAN_WAIT_EDIT 1107
#define ID_BLE_FILTER_EDIT 1201
#define ID_BLE_INTERVAL_COMBO 1202
#define ID_BLE_SLAVE_EDIT 1203
#define ID_BLE_MODBUS_SLAVE_EDIT 1204
#define ID_BLE_MODBUS_FUNCTION_EDIT 1205
#define ID_BLE_MODBUS_REGISTER_EDIT 1206
#define ID_BLE_MODBUS_VALUE_EDIT 1207
#define ID_BLE_MODBUS_TIMEOUT_EDIT 1208
#define ID_BLE_MODBUS_RESULT_EDIT 1209
#define ID_BLE_TRAFFIC_EDIT 1210
#define ID_BLE_VERSION_LIST_EDIT 1211
#define ID_BLE_OTA_GAP_EDIT 1220
#define ID_BLE_OTA_TIMEOUT_EDIT 1221
#define ID_BLE_OTA_CHANNEL_COMBO 1222
#define ID_BLE_OTA_CHIP_COMBO 1223
#define ID_BLE_OTA_ROW_CHIP_BASE 1240
#define BLE_MAX_DEVICES 64
#define BLE_VISIBLE_ROWS 8
#define BLE_OTA_MAX_FILES 16
#define BLE_OTA_VISIBLE_ROWS 8

typedef struct tagPOINT { LONG x; LONG y; } POINT;
typedef struct tagRECT { LONG left; LONG top; LONG right; LONG bottom; } RECT;
typedef struct tagDRAWITEMSTRUCT {
    UINT CtlType;
    UINT CtlID;
    UINT itemID;
    UINT itemAction;
    UINT itemState;
    HWND hwndItem;
    HDC hDC;
    RECT rcItem;
    ULONG_PTR itemData;
} DRAWITEMSTRUCT;
typedef struct tagMEASUREITEMSTRUCT {
    UINT CtlType;
    UINT CtlID;
    UINT itemID;
    UINT itemWidth;
    UINT itemHeight;
    ULONG_PTR itemData;
} MEASUREITEMSTRUCT;
typedef struct tagPAINTSTRUCT {
    HDC hdc;
    BOOL fErase;
    RECT rcPaint;
    BOOL fRestore;
    BOOL fIncUpdate;
    BYTE rgbReserved[32];
} PAINTSTRUCT;
typedef struct tagMSG {
    HWND hwnd;
    UINT message;
    WPARAM wParam;
    LPARAM lParam;
    DWORD time;
    POINT pt;
    DWORD lPrivate;
} MSG;
typedef LRESULT (CALLBACK *WNDPROC)(HWND, UINT, WPARAM, LPARAM);
typedef DWORD (WINAPI *LPTHREAD_START_ROUTINE)(LPVOID);
typedef struct tagSYSTEMTIME {
    WORD wYear;
    WORD wMonth;
    WORD wDayOfWeek;
    WORD wDay;
    WORD wHour;
    WORD wMinute;
    WORD wSecond;
    WORD wMilliseconds;
} SYSTEMTIME;
typedef struct tagSRWLOCK { void* Ptr; } SRWLOCK;
typedef struct tagWNDCLASSEXW {
    UINT cbSize;
    UINT style;
    WNDPROC lpfnWndProc;
    int cbClsExtra;
    int cbWndExtra;
    HINSTANCE hInstance;
    HICON hIcon;
    HCURSOR hCursor;
    HBRUSH hbrBackground;
    LPCWSTR lpszMenuName;
    LPCWSTR lpszClassName;
    HICON hIconSm;
} WNDCLASSEXW;
typedef struct tagTRACKMOUSEEVENT {
    DWORD cbSize;
    DWORD dwFlags;
    HWND hwndTrack;
    DWORD dwHoverTime;
} TRACKMOUSEEVENT;
typedef struct tagDCB {
    DWORD DCBlength;
    DWORD BaudRate;
    DWORD Flags;
    WORD wReserved;
    WORD XonLim;
    WORD XoffLim;
    BYTE ByteSize;
    BYTE Parity;
    BYTE StopBits;
    CHAR XonChar;
    CHAR XoffChar;
    CHAR ErrorChar;
    CHAR EofChar;
    CHAR EvtChar;
    WORD wReserved1;
} DCB;
typedef struct tagCOMMTIMEOUTS {
    DWORD ReadIntervalTimeout;
    DWORD ReadTotalTimeoutMultiplier;
    DWORD ReadTotalTimeoutConstant;
    DWORD WriteTotalTimeoutMultiplier;
    DWORD WriteTotalTimeoutConstant;
} COMMTIMEOUTS;
typedef union tagLARGE_INTEGER {
    struct { DWORD LowPart; LONG HighPart; } Parts;
    LONGLONG QuadPart;
} LARGE_INTEGER;
typedef struct tagOPENFILENAMEW {
    DWORD lStructSize;
    HWND hwndOwner;
    HINSTANCE hInstance;
    LPCWSTR lpstrFilter;
    LPWSTR lpstrCustomFilter;
    DWORD nMaxCustFilter;
    DWORD nFilterIndex;
    LPWSTR lpstrFile;
    DWORD nMaxFile;
    LPWSTR lpstrFileTitle;
    DWORD nMaxFileTitle;
    LPCWSTR lpstrInitialDir;
    LPCWSTR lpstrTitle;
    DWORD Flags;
    WORD nFileOffset;
    WORD nFileExtension;
    LPCWSTR lpstrDefExt;
    LPARAM lCustData;
    void* lpfnHook;
    LPCWSTR lpTemplateName;
    void* pvReserved;
    DWORD dwReserved;
    DWORD FlagsEx;
} OPENFILENAMEW;

typedef struct tagSTARTUPINFOW {
    DWORD cb;
    LPWSTR lpReserved;
    LPWSTR lpDesktop;
    LPWSTR lpTitle;
    DWORD dwX;
    DWORD dwY;
    DWORD dwXSize;
    DWORD dwYSize;
    DWORD dwXCountChars;
    DWORD dwYCountChars;
    DWORD dwFillAttribute;
    DWORD dwFlags;
    WORD wShowWindow;
    WORD cbReserved2;
    BYTE* lpReserved2;
    HANDLE hStdInput;
    HANDLE hStdOutput;
    HANDLE hStdError;
} STARTUPINFOW;

typedef struct tagPROCESS_INFORMATION {
    HANDLE hProcess;
    HANDLE hThread;
    DWORD dwProcessId;
    DWORD dwThreadId;
} PROCESS_INFORMATION;

__declspec(dllimport) ATOM WINAPI RegisterClassExW(const WNDCLASSEXW*);
__declspec(dllimport) HWND WINAPI CreateWindowExW(DWORD, LPCWSTR, LPCWSTR, DWORD, int, int, int, int, HWND, HANDLE, HINSTANCE, void*);
__declspec(dllimport) LRESULT WINAPI DefWindowProcW(HWND, UINT, WPARAM, LPARAM);
__declspec(dllimport) BOOL WINAPI ShowWindow(HWND, int);
__declspec(dllimport) BOOL WINAPI UpdateWindow(HWND);
__declspec(dllimport) BOOL WINAPI GetMessageW(MSG*, HWND, UINT, UINT);
__declspec(dllimport) BOOL WINAPI TranslateMessage(const MSG*);
__declspec(dllimport) LRESULT WINAPI DispatchMessageW(const MSG*);
__declspec(dllimport) void WINAPI PostQuitMessage(int);
__declspec(dllimport) HCURSOR WINAPI LoadCursorW(HINSTANCE, LPCWSTR);
__declspec(dllimport) HDC WINAPI BeginPaint(HWND, PAINTSTRUCT*);
__declspec(dllimport) BOOL WINAPI EndPaint(HWND, const PAINTSTRUCT*);
__declspec(dllimport) BOOL WINAPI GetClientRect(HWND, RECT*);
__declspec(dllimport) int WINAPI FillRect(HDC, const RECT*, HBRUSH);
__declspec(dllimport) int WINAPI DrawTextW(HDC, LPCWSTR, int, RECT*, UINT);
__declspec(dllimport) BOOL WINAPI InvalidateRect(HWND, const RECT*, BOOL);
__declspec(dllimport) BOOL WINAPI TrackMouseEvent(TRACKMOUSEEVENT*);
__declspec(dllimport) HCURSOR WINAPI SetCursor(HCURSOR);
__declspec(dllimport) int WINAPI MessageBoxW(HWND, LPCWSTR, LPCWSTR, UINT);
__declspec(dllimport) BOOL WINAPI SetProcessDPIAware(void);
__declspec(dllimport) UINT_PTR WINAPI SetTimer(HWND, UINT_PTR, UINT, void*);
__declspec(dllimport) BOOL WINAPI KillTimer(HWND, UINT_PTR);
__declspec(dllimport) LRESULT WINAPI SendMessageW(HWND, UINT, WPARAM, LPARAM);
__declspec(dllimport) BOOL WINAPI MoveWindow(HWND, int, int, int, int, BOOL);
__declspec(dllimport) int WINAPI GetWindowTextW(HWND, LPWSTR, int);
__declspec(dllimport) BOOL WINAPI SetWindowTextW(HWND, LPCWSTR);
__declspec(dllimport) BOOL WINAPI EnableWindow(HWND, BOOL);
__declspec(dllimport) BOOL WINAPI DestroyWindow(HWND);
__declspec(dllimport) int WINAPI wsprintfW(LPWSTR, LPCWSTR, ...);
__declspec(dllimport) HDC WINAPI GetDC(HWND);
__declspec(dllimport) int WINAPI ReleaseDC(HWND, HDC);
__declspec(dllimport) BOOL WINAPI PostMessageW(HWND, UINT, WPARAM, LPARAM);

__declspec(dllimport) HINSTANCE WINAPI GetModuleHandleW(LPCWSTR);
__declspec(dllimport) DWORD WINAPI GetModuleFileNameW(HMODULE, LPWSTR, DWORD);
__declspec(dllimport) BOOL WINAPI CreateProcessW(LPCWSTR, LPWSTR, void*, void*, BOOL, DWORD, void*, LPCWSTR, STARTUPINFOW*, PROCESS_INFORMATION*);
__declspec(dllimport) HMODULE WINAPI LoadLibraryW(LPCWSTR);
__declspec(dllimport) FARPROC WINAPI GetProcAddress(HMODULE, const CHAR*);
__declspec(dllimport) BOOL WINAPI FreeLibrary(HMODULE);
__declspec(dllimport) void WINAPI ExitProcess(UINT);
__declspec(dllimport) HANDLE WINAPI CreateFileW(LPCWSTR, DWORD, DWORD, void*, DWORD, DWORD, HANDLE);
__declspec(dllimport) BOOL WINAPI CloseHandle(HANDLE);
__declspec(dllimport) BOOL WINAPI DeleteFileW(LPCWSTR);
__declspec(dllimport) BOOL WINAPI ReadFile(HANDLE, void*, DWORD, DWORD*, void*);
__declspec(dllimport) BOOL WINAPI GetFileSizeEx(HANDLE, LARGE_INTEGER*);
__declspec(dllimport) BOOL WINAPI SetFilePointerEx(HANDLE, LARGE_INTEGER, LARGE_INTEGER*, DWORD);
__declspec(dllimport) DWORD WINAPI QueryDosDeviceW(LPCWSTR, LPWSTR, DWORD);
__declspec(dllimport) BOOL WINAPI GetCommState(HANDLE, DCB*);
__declspec(dllimport) BOOL WINAPI SetCommState(HANDLE, DCB*);
__declspec(dllimport) BOOL WINAPI SetCommTimeouts(HANDLE, const COMMTIMEOUTS*);
__declspec(dllimport) BOOL WINAPI SetupComm(HANDLE, DWORD, DWORD);
__declspec(dllimport) BOOL WINAPI PurgeComm(HANDLE, DWORD);
__declspec(dllimport) BOOL WINAPI WriteFile(HANDLE, const void*, DWORD, DWORD*, void*);
__declspec(dllimport) BOOL WINAPI FlushFileBuffers(HANDLE);
__declspec(dllimport) HANDLE WINAPI CreateThread(void*, SIZE_T, LPTHREAD_START_ROUTINE, LPVOID, DWORD, DWORD*);
__declspec(dllimport) HANDLE WINAPI CreateEventW(void*, BOOL, BOOL, LPCWSTR);
__declspec(dllimport) BOOL WINAPI SetEvent(HANDLE);
__declspec(dllimport) BOOL WINAPI ResetEvent(HANDLE);
__declspec(dllimport) DWORD WINAPI WaitForSingleObject(HANDLE, DWORD);
__declspec(dllimport) void WINAPI Sleep(DWORD);
__declspec(dllimport) unsigned long long WINAPI GetTickCount64(void);
__declspec(dllimport) void* WINAPI VirtualAlloc(void*, SIZE_T, DWORD, DWORD);
__declspec(dllimport) BOOL WINAPI VirtualFree(void*, SIZE_T, DWORD);
__declspec(dllimport) void WINAPI GetLocalTime(SYSTEMTIME*);
__declspec(dllimport) void WINAPI AcquireSRWLockExclusive(SRWLOCK*);
__declspec(dllimport) void WINAPI ReleaseSRWLockExclusive(SRWLOCK*);
__declspec(dllimport) void WINAPI AcquireSRWLockShared(SRWLOCK*);
__declspec(dllimport) void WINAPI ReleaseSRWLockShared(SRWLOCK*);

__declspec(dllimport) HBRUSH WINAPI CreateSolidBrush(COLORREF);
__declspec(dllimport) BOOL WINAPI DeleteObject(HGDIOBJ);
__declspec(dllimport) HGDIOBJ WINAPI SelectObject(HDC, HGDIOBJ);
__declspec(dllimport) BOOL WINAPI RoundRect(HDC, int, int, int, int, int, int);
__declspec(dllimport) BOOL WINAPI Ellipse(HDC, int, int, int, int);
__declspec(dllimport) BOOL WINAPI MoveToEx(HDC, int, int, POINT*);
__declspec(dllimport) BOOL WINAPI LineTo(HDC, int, int);
__declspec(dllimport) int WINAPI SetBkMode(HDC, int);
__declspec(dllimport) COLORREF WINAPI SetBkColor(HDC, COLORREF);
__declspec(dllimport) COLORREF WINAPI SetTextColor(HDC, COLORREF);
__declspec(dllimport) HFONT WINAPI CreateFontW(int, int, int, int, int, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, LPCWSTR);
__declspec(dllimport) COLORREF WINAPI SetDCBrushColor(HDC, COLORREF);
__declspec(dllimport) COLORREF WINAPI SetDCPenColor(HDC, COLORREF);
__declspec(dllimport) HGDIOBJ WINAPI GetStockObject(int);
__declspec(dllimport) HDC WINAPI CreateCompatibleDC(HDC);
__declspec(dllimport) HGDIOBJ WINAPI CreateCompatibleBitmap(HDC, int, int);
__declspec(dllimport) BOOL WINAPI DeleteDC(HDC);
__declspec(dllimport) BOOL WINAPI BitBlt(HDC, int, int, int, int, HDC, int, int, DWORD);
__declspec(dllimport) BOOL WINAPI Polygon(HDC, const POINT*, int);

__declspec(dllimport) BOOL WINAPI GetOpenFileNameW(OPENFILENAMEW*);


typedef struct tagBLE_DEVICE_ITEM
{
    unsigned long long Address;
    int AddressType;
    int Rssi;
    wchar_t Name[96];
    wchar_t Mac[24];
} BLE_DEVICE_ITEM;

typedef struct tagBLE_OTA_ITEM
{
    wchar_t Path[1024];
    wchar_t Name[260];
    wchar_t DevModel[64];
    wchar_t Message[128];
    int Chip;
    BYTE FirmwareType;
    DWORD Version;
    DWORD ImageSize;
    DWORD ImageCrc32;
    unsigned long long FileSize;
    BOOL Ready;
    int State;      /* 0 pending, 1 running, 2 success, 3 failed, 4 waiting */
    int Progress;
} BLE_OTA_ITEM;

/* CANalyst-II / ControlCAN compatible interface definitions. */
#define VCI_USBCAN2 4u
#define CAN_STATUS_OK 1u
#define CAN_STATUS_ERR 0xFFFFFFFFu
#define CAN_MAX_DEVICES 16
#define CAN_MAX_BROADCAST_NODES 12
#define CAN_FRAME_TOTAL_PER_BLOCK 171u
#define CAN_RX_BATCH_SIZE 48u
#define CAN_TX_BATCH_SIZE 48u

typedef struct tagVCI_CAN_OBJ
{
    DWORD ID;
    DWORD TimeStamp;
    BYTE TimeFlag;
    BYTE SendType;
    BYTE RemoteFlag;
    BYTE ExternFlag;
    BYTE DataLen;
    BYTE Data[8];
    BYTE Reserved[3];
} VCI_CAN_OBJ;

typedef struct tagVCI_INIT_CONFIG
{
    DWORD AccCode;
    DWORD AccMask;
    DWORD Reserved;
    BYTE Filter;
    BYTE Timing0;
    BYTE Timing1;
    BYTE Mode;
} VCI_INIT_CONFIG;

typedef struct tagVCI_BOARD_INFO
{
    USHORT hw_Version;
    USHORT fw_Version;
    USHORT dr_Version;
    USHORT in_Version;
    USHORT irq_Num;
    BYTE can_Num;
    CHAR str_Serial_Num[20];
    CHAR str_hw_Type[40];
    USHORT Reserved[4];
} VCI_BOARD_INFO;

typedef DWORD (WINAPI *PFN_VCI_OpenDevice)(DWORD, DWORD, DWORD);
typedef DWORD (WINAPI *PFN_VCI_CloseDevice)(DWORD, DWORD);
typedef DWORD (WINAPI *PFN_VCI_InitCAN)(DWORD, DWORD, DWORD, VCI_INIT_CONFIG*);
typedef DWORD (WINAPI *PFN_VCI_StartCAN)(DWORD, DWORD, DWORD);
typedef DWORD (WINAPI *PFN_VCI_ResetCAN)(DWORD, DWORD, DWORD);
typedef DWORD (WINAPI *PFN_VCI_ClearBuffer)(DWORD, DWORD, DWORD);
typedef DWORD (WINAPI *PFN_VCI_Transmit)(DWORD, DWORD, DWORD, VCI_CAN_OBJ*, DWORD);
typedef DWORD (WINAPI *PFN_VCI_Receive)(DWORD, DWORD, DWORD, VCI_CAN_OBJ*, DWORD, int);
typedef DWORD (WINAPI *PFN_VCI_FindUsbDevice)(VCI_BOARD_INFO*);

typedef struct tagCAN_NODE_STATE
{
    BYTE Address;
    BYTE Active;
    BYTE Failed;
    BYTE Completed;
    BYTE LastStatus;
    BYTE LastError;
    BYTE Progress;
    BYTE Reserved;
} CAN_NODE_STATE;

static const wchar_t APP_CLASS[] = L"BluettiFirmwareStudioClass";
static const wchar_t APP_TITLE[] = L"BLUETTI Device Studio";
static const wchar_t CHIP_NAMES[5][24] = {
    L"AT32 (Artery)", L"N32 (Nationstech)", L"TI C2000", L"STM32", L"GigaDevice"
};

static HWND gWindow = NULL;
static HWND gComCombo = NULL;
static HWND gBaudCombo = NULL;
static HWND gRepeatEdit = NULL;
static HWND gWaitEdit = NULL;
static HWND gChipCombo = NULL;
static HWND gCanDeviceCombo = NULL;
static HWND gCanChannelCombo = NULL;
static HWND gCanBaudCombo = NULL;
static HWND gCanLocalEdit = NULL;
static HWND gCanTargetEdit = NULL;
static HWND gCanRepeatEdit = NULL;
static HWND gCanWaitEdit = NULL;
static HWND gBleFilterEdit = NULL;
static HWND gBleIntervalCombo = NULL;
static HWND gBleSlaveEdit = NULL;
static HWND gBleModbusSlaveEdit = NULL;
static HWND gBleModbusFunctionEdit = NULL;
static HWND gBleModbusRegisterEdit = NULL;
static HWND gBleModbusValueEdit = NULL;
static HWND gBleModbusTimeoutEdit = NULL;
static HWND gBleModbusResultEdit = NULL;
static HWND gBleTrafficEdit = NULL;
static HWND gBleVersionListEdit = NULL;
static HWND gBleOtaGapEdit = NULL;
static HWND gBleOtaTimeoutEdit = NULL;
static HWND gBleOtaChannelCombo = NULL;
static HWND gBleOtaChipCombo = NULL;
static HWND gBleOtaRowChipCombos[BLE_OTA_VISIBLE_ROWS];
static HINSTANCE gInstance = NULL;
static HANDLE gSerialHandle = INVALID_HANDLE_VALUE;
static HMODULE gControlCanModule = NULL;
static PFN_VCI_OpenDevice gVciOpenDevice = NULL;
static PFN_VCI_CloseDevice gVciCloseDevice = NULL;
static PFN_VCI_InitCAN gVciInitCan = NULL;
static PFN_VCI_StartCAN gVciStartCan = NULL;
static PFN_VCI_ResetCAN gVciResetCan = NULL;
static PFN_VCI_ClearBuffer gVciClearBuffer = NULL;
static PFN_VCI_Transmit gVciTransmit = NULL;
static PFN_VCI_Receive gVciReceive = NULL;
static PFN_VCI_FindUsbDevice gVciFindUsbDevice = NULL;
static HANDLE gWorkerThread = NULL;
static HANDLE gCancelEvent = NULL;
static SRWLOCK gStateLock = { NULL };
static HANDLE gBleBackendProcess = NULL;
static BOOL gBleBackendStarted = FALSE;
static BOOL gBleScanning = FALSE;
static BOOL gBleConnecting = FALSE;
static BOOL gBleConnected = FALSE;
static DWORD gBleCommandSequence = 0;
static BLE_DEVICE_ITEM gBleDevices[BLE_MAX_DEVICES];
static int gBleDeviceCount = 0;
static int gBleFilteredIndices[BLE_MAX_DEVICES];
static int gBleFilteredCount = 0;
/* 用户选择以 BLE Address/MAC 为唯一身份，数组下标只作为当前快照缓存。 */
static BOOL gBleHasSelectedDevice = FALSE;
static BOOL gBleSelectedPresent = FALSE;
static unsigned long long gBleSelectedAddress = 0;
static int gBleSelectedAddressType = 0;
static int gBleSelectedRssi = 0;
static int gBleSelectedDevice = -1;
static wchar_t gBleSelectedName[96];
static wchar_t gBleSelectedMac[24];
static int gBleServiceCount = 0;
static wchar_t gBleCommandPath[1024];
static wchar_t gBleDevicePath[1024];
static wchar_t gBleStatusPath[1024];
static wchar_t gBleDataPath[1024];
static wchar_t gBleModbusPath[1024];
static wchar_t gBleTrafficPath[1024];
static wchar_t gBleOtaStatusPath[1024];
static wchar_t gBleOtaManifestPath[1024];
static BLE_OTA_ITEM gBleOtaItems[BLE_OTA_MAX_FILES];
static int gBleOtaCount = 0;
static int gBleOtaSelected = -1;
static int gBleOtaListOffset = 0;
static BOOL gBleOtaRunning = FALSE;
static DWORD gBleOtaStatusSequence = 0;
static int gBleOtaCurrentIndex = 0;
static int gBleOtaCurrentPercent = 0;
static int gBleOtaPcPercent = 0;
static int gBleOtaDevicePercent = 0;
static int gBleOtaDistributionDepth = 0;
static int gBleOtaDistributionError = 0;
static int gBleOtaDistributionSlot = -1;
static int gBleOtaGroupValue = 0;
static int gBleOtaProcessPercent = 0;
static int gBleOtaSuccessPercent = 0;
static int gBleOtaSuccessCount = 0;
static int gBleOtaFailureCount = 0;
static int gBleOtaWaitRemaining = 0;
static wchar_t gBleOtaState[48] = L"IDLE";
static wchar_t gBleOtaMessage[192] = L"添加多个固件后，为每个固件选择芯片平台";
static wchar_t gBleStatusText[192] = L"蓝牙状态：未连接";
static wchar_t gBleDataStatus[192] = L"等待首次 Modbus 轮询";
static wchar_t gBleConnectedName[96];
static wchar_t gBleConnectedMac[24];
static wchar_t gBleLastUpdate[32] = L"--";
static wchar_t gBleAddressMode[32] = L"自动识别";
static int gBleSlaveId = 1;
static int gBleConfiguredSlaveId = 1;
static wchar_t gBleDeviceType[96] = L"--";
static wchar_t gBleDeviceSn[128] = L"--";
static wchar_t gBleVersions[512] = L"--";
static BOOL gBleDataValid = FALSE;
static DWORD gBleDataSequence = 0;
static int gBlePollInterval = 2;
static int gBleSoc = -1;
static int gBleAcOutputPower = 0;
static int gBleDcOutputPower = 0;
static int gBlePvInputPower = 0;
static int gBleAcInputPower = 0;
static int gBleAcState = -1;
static int gBleDcState = -1;
static DWORD gBleModbusSequence = 0;
static DWORD gBleTrafficSequence = 0;
static wchar_t gBleModbusStatus[96] = L"等待发送自定义指令";

static HFONT gFontLogo = NULL;
static HFONT gFontMission = NULL;
static HFONT gFontTitle = NULL;
static HFONT gFontSubtitle = NULL;
static HFONT gFontCardTitle = NULL;
static HFONT gFontBody = NULL;
static HFONT gFontSmall = NULL;
static HFONT gFontTiny = NULL;
static HFONT gFontPercent = NULL;

static int gCurrentPage = 0; /* 0 home, 1 bluetooth, 2 serial, 3 CAN mode, 4 CAN single, 5 CAN broadcast, 6 BLE OTA */
static int gCanMode = 0; /* 1 single, 2 broadcast */
static BOOL gCanConnected = FALSE;
static DWORD gCanDeviceIndex = 0;
static DWORD gCanChannelIndex = 0;
static DWORD gCanBaudRate = 250000;
static BYTE gCanLocalAddress = 0x10;
static BYTE gCanTargetAddress = 0x16;
static CAN_NODE_STATE gCanNodes[CAN_MAX_BROADCAST_NODES];
static int gCanNodeCount = 0;
static DWORD gCanWholeCrc32 = 0;
static DWORD gCanTransmitSize = 0;
static int gCanDiscoveredDeviceCount = 0;
static DWORD gCanDeviceMap[8];
static int gCanDeviceMapCount = 0;
static VCI_CAN_OBJ gCanTxFrames[CAN_FRAME_TOTAL_PER_BLOCK];
static VCI_CAN_OBJ gCanRxBatch[CAN_RX_BATCH_SIZE];
static DWORD gCanRxBatchCount = 0;
static DWORD gCanRxBatchIndex = 0;
static int gHoverItem = 0;
static BOOL gTrackingMouse = FALSE;
static BOOL gDarkMode = TRUE;
static BOOL gSerialConnected = FALSE;
static BOOL gFirmwareReady = FALSE;
static BOOL gChipDialogOpen = FALSE;
static BOOL gUpgradeRunning = FALSE;
static BOOL gWaitingNext = FALSE;
static BOOL gUserStopRequested = FALSE;
static int gUpgradeProgress = 0;
static int gCurrentPacket = 0;
static int gTotalPackets = 0;
static int gCurrentAttempt = 0;
static int gWorkerResult = 0; /* 1 success, 0 stopped, -1 failed */
static int gCurrentRepeat = 0;
static int gRepeatTotal = 1;
static int gWaitSeconds = 3;
static int gWaitRemaining = 0;
static int gCompletedCount = 0;
static int gSuccessCount = 0;
static int gFailureCount = 0;
static int gProgressVisualState = 0; /* 0 active/idle, 1 success, -1 failed, 2 stopped */
static BYTE gRoundResults[10000];   /* 0 pending, 1 success, 2 failed */
static int gSelectedChip = 0;
static unsigned long long gFirmwareFileSize = 0;
static DWORD gHeaderReadOffset = 0x000;
static DWORD gHeaderAreaSize = 0x200;
static BOOL gHeaderSignatureValid = FALSE;
static BYTE gFirmwareType = 0;
static DWORD gImageVersion = 0;
static DWORD gImageSize = 0;
static DWORD gImageCrc32 = 0;

static wchar_t gFirmwarePath[1024];
static wchar_t gFirmwareName[260];
static wchar_t gMagicText[32];
static wchar_t gMagicBytesText[64];
static wchar_t gDevModelText[64];
static wchar_t gTimeText[64];
static wchar_t gConnectionText[128] = L"未连接";
static wchar_t gCanConnectionText[160] = L"CANalyst-II 未连接";
static wchar_t gUpgradeStatus[192] = L"等待选择串口和升级固件";
static wchar_t gProtocolStage[96] = L"协议空闲";
static wchar_t gStartFrameText[160] = L"—";
static wchar_t gLastError[192];
static wchar_t gLogs[8][192];

static RECT gCardRects[3];
static RECT gBackRect;
static RECT gConnectRect;
static RECT gFirmwareRect;
static RECT gStartRect;
static RECT gStopRect;
static RECT gChipConfirmRect;
static RECT gChipCancelRect;
static RECT gThemeRect;
static RECT gCanModeRects[2];
static RECT gBleScanRect;
static RECT gBleConnectRect;
static RECT gBleDisconnectRect;
static RECT gBleAcControlRect;
static RECT gBleDcControlRect;
static RECT gBleRefreshRect;
static RECT gBleSlaveApplyRect;
static RECT gBleModbusSendRect;
static RECT gBleOtaEntryRect;
static RECT gBleDeviceRows[BLE_VISIBLE_ROWS];
static RECT gBleOtaAddRect;
static RECT gBleOtaClearRect;
static RECT gBleOtaApplyChipRect;
static RECT gBleOtaRemoveRect;
static RECT gBleOtaStartRect;
static RECT gBleOtaStopRect;
static RECT gBleOtaUpRect;
static RECT gBleOtaDownRect;
static RECT gBleOtaRows[BLE_OTA_VISIBLE_ROWS];
static RECT gBleOtaVerifyRects[BLE_OTA_VISIBLE_ROWS];
/* V1.3.9: 芯片平台统一使用自绘 Apple 风选择器，不再显示原生 Windows ComboBox。 */
static RECT gChipSelectorRect;
static RECT gChipSelectorOptionRects[5];
static RECT gBleOtaChipSelectorRects[BLE_OTA_VISIBLE_ROWS];
static RECT gBleOtaChipOptionRects[5];
static BOOL gChipSelectorOpen = FALSE;
static BOOL gBleOtaChipSelectorOpen = FALSE;
static int gBleOtaChipSelectorRow = -1;

/* V1.3.9: all visible Windows ComboBox controls are replaced by one native-drawn selector system.
 * The old ComboBox HWNDs remain hidden as backing stores only so communication/business logic
 * continues reading exactly the same selections through CB_GETCURSEL/CB_GETLBTEXT. */
#define UI_SELECTOR_COM          1
#define UI_SELECTOR_BAUD         2
#define UI_SELECTOR_CAN_DEVICE   3
#define UI_SELECTOR_CAN_CHANNEL  4
#define UI_SELECTOR_CAN_BAUD     5
#define UI_SELECTOR_BLE_INTERVAL 6
#define UI_SELECTOR_MAX_VISIBLE  12
static RECT gComSelectorRect;
static RECT gBaudSelectorRect;
static RECT gCanDeviceSelectorRect;
static RECT gCanChannelSelectorRect;
static RECT gCanBaudSelectorRect;
static RECT gBleIntervalSelectorRect;
static RECT gUiSelectorOptionRects[UI_SELECTOR_MAX_VISIBLE];
static RECT gUiSelectorPopupUpRect;
static RECT gUiSelectorPopupDownRect;
static int gUiSelectorOpen = 0;
static int gUiSelectorFirstOption = 0;
static int gUiSelectorVisibleCount = 0;

/* Native-drawn, double-buffered BLE version list. This replaces the flashing multiline EDIT. */
static RECT gBleVersionViewportRect;
static RECT gBleVersionUpRect;
static RECT gBleVersionDownRect;
static int gBleVersionScroll = 0;

static int WLen(LPCWSTR text)
{
    int length = 0;
    if (text != NULL) { while (text[length] != 0) { length++; } }
    return length;
}

static void WCopy(LPWSTR destination, LPCWSTR source, int maxCount)
{
    int index = 0;
    if (maxCount <= 0) { return; }
    while (source != NULL && source[index] != 0 && index < maxCount - 1)
    {
        destination[index] = source[index];
        index++;
    }
    destination[index] = 0;
}

static void WAppend(LPWSTR destination, LPCWSTR source, int maxCount)
{
    int length = WLen(destination);
    int index = 0;
    while (source != NULL && source[index] != 0 && length < maxCount - 1)
    {
        destination[length++] = source[index++];
    }
    destination[length] = 0;
}

static int HexDigitValue(wchar_t value);
static void DrawButton(HDC dc, RECT rect, LPCWSTR text, COLORREF fill, COLORREF border, COLORREF textColor, BOOL hover);
static void DrawInputShell(HDC dc, RECT rect, BOOL emphasized);
static void DrawUiSelectorControl(HDC dc, const RECT* rect, int kind, BOOL open, BOOL hover);
static void DrawUiSelectorPopup(HDC dc, const RECT* client);
static void DrawBleVersionViewport(HDC dc, const RECT* rect);

static void BuildSiblingPath(LPWSTR output, LPCWSTR fileName, int maxCount)
{
    wchar_t modulePath[1024];
    int index;
    modulePath[0] = 0;
    output[0] = 0;
    if (GetModuleFileNameW(NULL, modulePath, 1024) == 0) { return; }
    WCopy(output, modulePath, maxCount);
    for (index = WLen(output) - 1; index >= 0; index--)
    {
        if (output[index] == L'\\' || output[index] == L'/') { output[index + 1] = 0; break; }
    }
    WAppend(output, fileName, maxCount);
}

static BOOL WriteUtf16File(LPCWSTR path, LPCWSTR text)
{
    HANDLE file;
    DWORD written = 0;
    WORD bom = 0xFEFF;
    DWORD bytes = (DWORD)(WLen(text) * 2);
    BOOL result = FALSE;
    file = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file != INVALID_HANDLE_VALUE)
    {
        if (WriteFile(file, &bom, 2, &written, NULL) && written == 2 && WriteFile(file, text, bytes, &written, NULL) && written == bytes)
        {
            FlushFileBuffers(file);
            result = TRUE;
        }
        CloseHandle(file);
    }
    return result;
}

static int ReadUtf16File(LPCWSTR path, LPWSTR output, int maxCount)
{
    HANDLE file;
    LARGE_INTEGER size;
    static BYTE buffer[32768];
    DWORD readCount = 0;
    DWORD byteCount;
    DWORD offset = 0;
    int charCount = 0;
    int index;
    if (maxCount <= 0) { return 0; }
    output[0] = 0;
    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) { return 0; }
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0) { CloseHandle(file); return 0; }
    byteCount = (DWORD)(size.QuadPart > (LONGLONG)sizeof(buffer) ? sizeof(buffer) : size.QuadPart);
    if (!ReadFile(file, buffer, byteCount, &readCount, NULL)) { CloseHandle(file); return 0; }
    CloseHandle(file);
    if (readCount >= 2 && buffer[0] == 0xFF && buffer[1] == 0xFE) { offset = 2; }
    for (index = (int)offset; index + 1 < (int)readCount && charCount < maxCount - 1; index += 2)
    {
        output[charCount++] = (wchar_t)((WORD)buffer[index] | ((WORD)buffer[index + 1] << 8));
    }
    output[charCount] = 0;
    return charCount;
}

static void FormatBleAddress(unsigned long long address, LPWSTR mac, int maxCount)
{
    static const wchar_t hex[] = L"0123456789ABCDEF";
    int byteIndex;
    int pos = 0;
    if (maxCount < 18) { if (maxCount > 0) { mac[0] = 0; } return; }
    for (byteIndex = 5; byteIndex >= 0; byteIndex--)
    {
        BYTE value = (BYTE)((address >> (byteIndex * 8)) & 0xFFu);
        mac[pos++] = hex[(value >> 4) & 0x0F];
        mac[pos++] = hex[value & 0x0F];
        if (byteIndex > 0) { mac[pos++] = L':'; }
    }
    mac[pos] = 0;
}

static unsigned long long ParseHex64(LPCWSTR text)
{
    unsigned long long value = 0;
    int index = 0;
    int digit;
    while (text[index] != 0)
    {
        digit = HexDigitValue(text[index]);
        if (digit >= 0) { value = (value << 4) | (unsigned long long)digit; }
        index++;
    }
    return value;
}

static int ParseSignedInt(LPCWSTR text)
{
    int sign = 1;
    int value = 0;
    int index = 0;
    if (text[0] == L'-') { sign = -1; index = 1; }
    while (text[index] >= L'0' && text[index] <= L'9') { value = value * 10 + (text[index] - L'0'); index++; }
    return value * sign;
}

static BOOL ParseUserNumber(LPCWSTR text, BOOL forceHex, int* output)
{
    unsigned int value = 0;
    int index = 0;
    int base = forceHex ? 16 : 10;
    int digit;
    BOOL hasDigit = FALSE;
    if (text == NULL || output == NULL) { return FALSE; }
    while (text[index] == L' ' || text[index] == L'\t') { index++; }
    if (text[index] == L'0' && (text[index + 1] == L'x' || text[index + 1] == L'X')) { base = 16; index += 2; }
    while (text[index] != 0 && text[index] != L' ' && text[index] != L'\t')
    {
        if (text[index] >= L'0' && text[index] <= L'9') { digit = text[index] - L'0'; }
        else if (text[index] >= L'a' && text[index] <= L'f') { digit = text[index] - L'a' + 10; }
        else if (text[index] >= L'A' && text[index] <= L'F') { digit = text[index] - L'A' + 10; }
        else { return FALSE; }
        if (digit >= base) { return FALSE; }
        value = value * (unsigned int)base + (unsigned int)digit;
        if (value > 0x7FFFFFFFu) { return FALSE; }
        hasDigit = TRUE;
        index++;
    }
    if (!hasDigit) { return FALSE; }
    while (text[index] == L' ' || text[index] == L'\t') { index++; }
    if (text[index] != 0) { return FALSE; }
    *output = (int)value;
    return TRUE;
}

static BOOL WStartsWithNoCase(LPCWSTR text, LPCWSTR prefix)
{
    int index = 0;
    wchar_t a;
    wchar_t b;
    if (prefix == NULL || prefix[0] == 0) { return TRUE; }
    while (prefix[index] != 0)
    {
        if (text == NULL || text[index] == 0) { return FALSE; }
        a = text[index]; b = prefix[index];
        if (a >= L'a' && a <= L'z') { a = (wchar_t)(a - L'a' + L'A'); }
        if (b >= L'a' && b <= L'z') { b = (wchar_t)(b - L'a' + L'A'); }
        if (a != b) { return FALSE; }
        index++;
    }
    return TRUE;
}

static BOOL WContainsNoCase(LPCWSTR text, LPCWSTR part)
{
    int index = 0;
    if (part == NULL || part[0] == 0) { return TRUE; }
    while (text != NULL && text[index] != 0)
    {
        if (WStartsWithNoCase(text + index, part)) { return TRUE; }
        index++;
    }
    return FALSE;
}

static void ClearBleSelection(void)
{
    gBleHasSelectedDevice = FALSE;
    gBleSelectedPresent = FALSE;
    gBleSelectedAddress = 0;
    gBleSelectedAddressType = 0;
    gBleSelectedRssi = 0;
    gBleSelectedDevice = -1;
    gBleSelectedName[0] = 0;
    gBleSelectedMac[0] = 0;
}

static void SelectBleDeviceByIndex(int deviceIndex)
{
    BLE_DEVICE_ITEM* device;
    if (deviceIndex >= 0 && deviceIndex < gBleDeviceCount)
    {
        device = &gBleDevices[deviceIndex];
        gBleHasSelectedDevice = TRUE;
        gBleSelectedPresent = TRUE;
        gBleSelectedAddress = device->Address;
        gBleSelectedAddressType = device->AddressType;
        gBleSelectedRssi = device->Rssi;
        gBleSelectedDevice = deviceIndex;
        WCopy(gBleSelectedName, device->Name, 96);
        WCopy(gBleSelectedMac, device->Mac, 24);
    }
}

static void ResolveBleSelectedDevice(void)
{
    int index;
    gBleSelectedDevice = -1;
    gBleSelectedPresent = FALSE;
    if (gBleHasSelectedDevice)
    {
        for (index = 0; index < gBleDeviceCount; index++)
        {
            if (gBleDevices[index].Address == gBleSelectedAddress)
            {
                gBleSelectedDevice = index;
                gBleSelectedPresent = TRUE;
                gBleSelectedAddressType = gBleDevices[index].AddressType;
                gBleSelectedRssi = gBleDevices[index].Rssi;
                WCopy(gBleSelectedName, gBleDevices[index].Name, 96);
                WCopy(gBleSelectedMac, gBleDevices[index].Mac, 24);
                break;
            }
        }
    }
}

static void UpdateBleFilter(void)
{
    wchar_t filter[96];
    wchar_t compact[96];
    int i;
    int j = 0;
    GetWindowTextW(gBleFilterEdit, filter, 96);
    for (i = 0; filter[i] != 0 && j < 95; i++)
    {
        if (filter[i] != L':' && filter[i] != L'-' && filter[i] != L' ') { compact[j++] = filter[i]; }
    }
    compact[j] = 0;
    gBleFilteredCount = 0;
    for (i = 0; i < gBleDeviceCount && gBleFilteredCount < BLE_MAX_DEVICES; i++)
    {
        wchar_t compactMac[24];
        int a = 0;
        int b = 0;
        while (gBleDevices[i].Mac[a] != 0 && b < 23)
        {
            if (gBleDevices[i].Mac[a] != L':') { compactMac[b++] = gBleDevices[i].Mac[a]; }
            a++;
        }
        compactMac[b] = 0;
        if (filter[0] == 0 || WStartsWithNoCase(gBleDevices[i].Name, filter) || WContainsNoCase(compactMac, compact))
        {
            gBleFilteredIndices[gBleFilteredCount++] = i;
        }
    }
}


static BOOL EnsureBleBackend(void)
{
    static wchar_t scriptPath[1024];
    static wchar_t commandLine[6144];
    STARTUPINFOW startup;
    PROCESS_INFORMATION process;
    int index;
    if (gBleBackendStarted && gBleBackendProcess != NULL) { return TRUE; }
    BuildSiblingPath(scriptPath, L"BLUETTI_BLE_Backend.ps1", 1024);
    BuildSiblingPath(gBleCommandPath, L"BLUETTI_BLE_Command.tmp", 1024);
    BuildSiblingPath(gBleDevicePath, L"BLUETTI_BLE_Devices.tmp", 1024);
    BuildSiblingPath(gBleStatusPath, L"BLUETTI_BLE_Status.tmp", 1024);
    BuildSiblingPath(gBleDataPath, L"BLUETTI_BLE_Data.tmp", 1024);
    BuildSiblingPath(gBleModbusPath, L"BLUETTI_BLE_Modbus.tmp", 1024);
    BuildSiblingPath(gBleTrafficPath, L"BLUETTI_BLE_Traffic.tmp", 1024);
    BuildSiblingPath(gBleOtaStatusPath, L"BLUETTI_BLE_OTA_Status.tmp", 1024);
    BuildSiblingPath(gBleOtaManifestPath, L"BLUETTI_BLE_OTA_Manifest.tmp", 1024);
    DeleteFileW(gBleCommandPath);
    DeleteFileW(gBleDevicePath);
    DeleteFileW(gBleStatusPath);
    DeleteFileW(gBleDataPath);
    DeleteFileW(gBleModbusPath);
    DeleteFileW(gBleTrafficPath);
    DeleteFileW(gBleOtaStatusPath);
    DeleteFileW(gBleOtaManifestPath);
    for (index = 0; index < (int)(sizeof(startup) / sizeof(BYTE)); index++) { ((BYTE*)&startup)[index] = 0; }
    for (index = 0; index < (int)(sizeof(process) / sizeof(BYTE)); index++) { ((BYTE*)&process)[index] = 0; }
    startup.cb = sizeof(startup);
    commandLine[0] = 0;
    WCopy(commandLine, L"powershell.exe -NoLogo -NoProfile -STA -ExecutionPolicy Bypass -WindowStyle Hidden -File \"", 6144);
    WAppend(commandLine, scriptPath, 6144);
    WAppend(commandLine, L"\" -CommandPath \"", 6144);
    WAppend(commandLine, gBleCommandPath, 6144);
    WAppend(commandLine, L"\" -DevicePath \"", 6144);
    WAppend(commandLine, gBleDevicePath, 6144);
    WAppend(commandLine, L"\" -StatusPath \"", 6144);
    WAppend(commandLine, gBleStatusPath, 6144);
    WAppend(commandLine, L"\" -DataPath \"", 6144);
    WAppend(commandLine, gBleDataPath, 6144);
    WAppend(commandLine, L"\" -ModbusPath \"", 6144);
    WAppend(commandLine, gBleModbusPath, 6144);
    WAppend(commandLine, L"\" -TrafficPath \"", 6144);
    WAppend(commandLine, gBleTrafficPath, 6144);
    WAppend(commandLine, L"\" -OtaStatusPath \"", 6144);
    WAppend(commandLine, gBleOtaStatusPath, 6144);
    WAppend(commandLine, L"\"", 6144);
    if (!CreateProcessW(NULL, commandLine, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &startup, &process))
    {
        WCopy(gBleStatusText, L"蓝牙后台启动失败：请确认脚本与 EXE 位于同一目录", 192);
        MessageBoxW(gWindow, L"无法启动内置蓝牙后台。请确认 BLUETTI_BLE_Backend.ps1 与 EXE 位于同一目录。", L"蓝牙连接", MB_OK | MB_ICONERROR);
        return FALSE;
    }
    CloseHandle(process.hThread);
    gBleBackendProcess = process.hProcess;
    gBleBackendStarted = TRUE;
    WCopy(gBleStatusText, L"蓝牙状态：正在初始化 Windows BLE 后台", 192);
    return TRUE;
}

static BOOL SendBleCommand(LPCWSTR command)
{
    wchar_t text[2048];
    gBleCommandSequence++;
    wsprintfW(text, L"%u\t%s", (UINT)gBleCommandSequence, command);
    return WriteUtf16File(gBleCommandPath, text);
}

static BOOL ReadBleSlaveAddress(BOOL showError, int* output)
{
    wchar_t valueText[32];
    int slaveId;
    GetWindowTextW(gBleSlaveEdit, valueText, 32);
    if (!ParseUserNumber(valueText, FALSE, &slaveId) || slaveId < 0 || slaveId > 247)
    {
        if (showError) { MessageBoxW(gWindow, L"从机地址必须是 0～247 的十进制数。", L"Modbus 从机地址", MB_OK | MB_ICONWARNING); }
        return FALSE;
    }
    gBleConfiguredSlaveId = slaveId;
    gBleSlaveId = slaveId;
    if (output != NULL) { *output = slaveId; }
    return TRUE;
}

static BOOL ApplyBleSlaveAddress(BOOL showError)
{
    wchar_t command[64];
    int slaveId;
    if (!ReadBleSlaveAddress(showError, &slaveId)) { return FALSE; }
    wsprintfW(command, L"SLAVE\t%d", slaveId);
    SendBleCommand(command);
    wsprintfW(gBleStatusText, L"Modbus 从机地址已设置为 %d", slaveId);
    return TRUE;
}

static void SendBleManualModbusRequest(void)
{
    wchar_t slaveText[32];
    wchar_t functionText[32];
    wchar_t registerText[32];
    wchar_t valueText[32];
    wchar_t timeoutText[32];
    wchar_t command[160];
    int slaveId;
    int functionCode;
    int registerAddress;
    int quantityOrValue;
    int timeoutMs;
    GetWindowTextW(gBleModbusSlaveEdit, slaveText, 32);
    GetWindowTextW(gBleModbusFunctionEdit, functionText, 32);
    GetWindowTextW(gBleModbusRegisterEdit, registerText, 32);
    GetWindowTextW(gBleModbusValueEdit, valueText, 32);
    GetWindowTextW(gBleModbusTimeoutEdit, timeoutText, 32);
    if (!ParseUserNumber(slaveText, FALSE, &slaveId) || slaveId < 0 || slaveId > 247)
    {
        MessageBoxW(gWindow, L"自定义指令的从机地址必须是 0～247。", L"Modbus 指令", MB_OK | MB_ICONWARNING); return;
    }
    if (!ParseUserNumber(functionText, TRUE, &functionCode) || functionCode < 1 || functionCode > 6)
    {
        MessageBoxW(gWindow, L"当前构建器支持功能码 01～06；0F/10 需要额外数据区，暂不发送。", L"Modbus 指令", MB_OK | MB_ICONWARNING); return;
    }
    if (!ParseUserNumber(registerText, FALSE, &registerAddress) || registerAddress < 0 || registerAddress > 65535)
    {
        MessageBoxW(gWindow, L"寄存器地址必须是 0～65535，支持十进制或 0x 前缀。", L"Modbus 指令", MB_OK | MB_ICONWARNING); return;
    }
    if (!ParseUserNumber(valueText, FALSE, &quantityOrValue) || quantityOrValue < 0 || quantityOrValue > 65535)
    {
        MessageBoxW(gWindow, L"长度/写入值必须是 0～65535，支持十进制或 0x 前缀。", L"Modbus 指令", MB_OK | MB_ICONWARNING); return;
    }
    if (!ParseUserNumber(timeoutText, FALSE, &timeoutMs) || timeoutMs < 100 || timeoutMs > 10000)
    {
        MessageBoxW(gWindow, L"超时时间必须是 100～10000 ms。", L"Modbus 指令", MB_OK | MB_ICONWARNING); return;
    }
    wsprintfW(command, L"MODBUS\t%d\t%d\t%d\t%d\t%d", slaveId, functionCode, registerAddress, quantityOrValue, timeoutMs);
    if (SendBleCommand(command))
    {
        WCopy(gBleModbusStatus, L"指令已发送，正在等待响应……", 96);
        SetWindowTextW(gBleModbusResultEdit, L"正在等待设备返回 Modbus 数据……");
    }
}

static void RefreshBleDevices(void)
{
    static wchar_t text[16384];
    wchar_t* cursor;
    int count = 0;
    if (ReadUtf16File(gBleDevicePath, text, 16384) <= 0) { return; }
    cursor = text;
    while (*cursor != 0 && count < BLE_MAX_DEVICES)
    {
        wchar_t* fields[4];
        int field = 0;
        wchar_t* line = cursor;
        fields[field++] = line;
        while (*cursor != 0 && *cursor != L'\r' && *cursor != L'\n')
        {
            if (*cursor == L'\t' && field < 4) { *cursor = 0; fields[field++] = cursor + 1; }
            cursor++;
        }
        while (*cursor == L'\r' || *cursor == L'\n') { *cursor = 0; cursor++; }
        if (field >= 4 && fields[0][0] != 0)
        {
            gBleDevices[count].Address = ParseHex64(fields[0]);
            gBleDevices[count].AddressType = ParseSignedInt(fields[1]);
            gBleDevices[count].Rssi = ParseSignedInt(fields[2]);
            WCopy(gBleDevices[count].Name, fields[3][0] ? fields[3] : L"未命名 BLE 设备", 96);
            FormatBleAddress(gBleDevices[count].Address, gBleDevices[count].Mac, 24);
            count++;
        }
        (void)line;
    }
    gBleDeviceCount = count;
    ResolveBleSelectedDevice();
    UpdateBleFilter();
}

static void UpdateBleVersionListControl(void)
{
    wchar_t output[1024];
    int sourceIndex = 0;
    int outputIndex = 0;
    BOOL lineStart = TRUE;

    if (gBleVersionListEdit == NULL) { return; }
    if (gBleVersions[0] == 0 || (gBleVersions[0] == L'-' && gBleVersions[1] == L'-'))
    {
        SetWindowTextW(gBleVersionListEdit, L"等待设备版本信息……");
        return;
    }

    while (gBleVersions[sourceIndex] != 0 && outputIndex < 1018)
    {
        wchar_t ch = gBleVersions[sourceIndex];
        if (ch == L'|')
        {
            while (outputIndex > 0 && output[outputIndex - 1] == L' ') { outputIndex--; }
            if (outputIndex < 1018) { output[outputIndex++] = L'\r'; output[outputIndex++] = L'\n'; }
            sourceIndex++;
            while (gBleVersions[sourceIndex] == L' ') { sourceIndex++; }
            lineStart = TRUE;
            continue;
        }
        if (lineStart)
        {
            while (ch == L' ') { sourceIndex++; ch = gBleVersions[sourceIndex]; }
            lineStart = FALSE;
        }
        output[outputIndex++] = ch;
        sourceIndex++;
    }
    output[outputIndex] = 0;
    SetWindowTextW(gBleVersionListEdit, output);
}

static void ResetBleRealtimeData(void)
{
    gBleDataValid = FALSE;
    gBleDataSequence = 0;
    gBleSoc = -1;
    gBleAcOutputPower = 0;
    gBleDcOutputPower = 0;
    gBlePvInputPower = 0;
    gBleAcInputPower = 0;
    gBleAcState = -1;
    gBleDcState = -1;
    WCopy(gBleLastUpdate, L"--", 32);
    WCopy(gBleAddressMode, L"手动指定", 32);
    gBleSlaveId = gBleConfiguredSlaveId;
    WCopy(gBleDeviceType, L"--", 96);
    WCopy(gBleDeviceSn, L"--", 128);
    WCopy(gBleVersions, L"--", 512);
    UpdateBleVersionListControl();
    WCopy(gBleDataStatus, L"等待首次 Modbus 轮询", 192);
}

static void ConfirmBleConnectedFromModbusData(void)
{
    /*
     * 双保险：若主界面漏掉后台瞬时CONNECTED状态，但已经收到有效Modbus DATA，
     * 则BLE/GATT/业务通信事实上已经成功。此时立即结束“正在连接”UI状态。
     * 仅在用户当前确实处于连接流程时生效，避免历史数据造成误判。
     */
    if (!gBleConnected && gBleConnecting)
    {
        gBleConnected = TRUE;
        gBleConnecting = FALSE;
        gBleScanning = FALSE;
        if (gBleHasSelectedDevice)
        {
            WCopy(gBleConnectedName, gBleSelectedName, 96);
            WCopy(gBleConnectedMac, gBleSelectedMac, 24);
        }
        WCopy(gBleStatusText, L"蓝牙已连接（Modbus通信已确认）", 192);
    }
}

static void RefreshBleData(void)
{
    static wchar_t text[4096];
    wchar_t* fields[32];
    int field = 0;
    wchar_t* cursor;
    if (ReadUtf16File(gBleDataPath, text, 4096) <= 0) { return; }
    fields[field++] = text;
    cursor = text;
    while (*cursor != 0 && field < 32)
    {
        if (*cursor == L'\t' || *cursor == L'\r' || *cursor == L'\n')
        {
            *cursor = 0;
            if (*(cursor + 1) != 0) { fields[field++] = cursor + 1; }
            if (*(cursor + 1) == L'\n') { cursor++; }
        }
        cursor++;
    }
    if (WStartsWithNoCase(fields[0], L"DATA") && !WStartsWithNoCase(fields[0], L"DATAERROR") && field >= 11)
    {
        gBleDataSequence = (DWORD)ParseSignedInt(fields[1]);
        gBleSoc = ParseSignedInt(fields[2]);
        gBleAcOutputPower = ParseSignedInt(fields[3]);
        gBleDcOutputPower = ParseSignedInt(fields[4]);
        gBlePvInputPower = ParseSignedInt(fields[5]);
        gBleAcInputPower = ParseSignedInt(fields[6]);
        gBleAcState = ParseSignedInt(fields[7]);
        gBleDcState = ParseSignedInt(fields[8]);
        WCopy(gBleLastUpdate, fields[9], 32);
        WCopy(gBleAddressMode, L"标准地址", 32);
        if (field > 11) { gBleSlaveId = ParseSignedInt(fields[11]); }
        if (field > 12 && fields[12][0] != 0) { WCopy(gBleDeviceType, fields[12], 96); }
        if (field > 13 && fields[13][0] != 0) { WCopy(gBleDeviceSn, fields[13], 128); }
        if (field > 14 && fields[14][0] != 0) { WCopy(gBleVersions, fields[14], 512); UpdateBleVersionListControl(); }
        WCopy(gBleDataStatus, L"Modbus实时数据已更新", 192);
        gBleDataValid = TRUE;
        ConfirmBleConnectedFromModbusData();
    }
    else if (WStartsWithNoCase(fields[0], L"DATAERROR"))
    {
        if (field > 2) { WCopy(gBleDataStatus, fields[2], 192); }
        else { WCopy(gBleDataStatus, L"Modbus轮询未收到有效响应，正在自动重试", 192); }
    }
    else if (field > 1)
    {
        WCopy(gBleDataStatus, fields[1], 192);
    }
}

static void RefreshBleModbusResult(void)
{
    static wchar_t text[8192];
    wchar_t* fields[12];
    wchar_t output[8192];
    wchar_t* cursor;
    int field = 0;
    DWORD sequence;
    if (ReadUtf16File(gBleModbusPath, text, 8192) <= 0) { return; }
    fields[field++] = text;
    cursor = text;
    while (*cursor != 0 && field < 12)
    {
        if (*cursor == L'\t' || *cursor == L'\r' || *cursor == L'\n')
        {
            *cursor = 0;
            if (*(cursor + 1) != 0) { fields[field++] = cursor + 1; }
            if (*(cursor + 1) == L'\n') { cursor++; }
        }
        cursor++;
    }
    if (field < 4 || !WStartsWithNoCase(fields[0], L"MANUAL")) { return; }
    sequence = (DWORD)ParseSignedInt(fields[1]);
    if (sequence == gBleModbusSequence) { return; }
    gBleModbusSequence = sequence;
    output[0] = 0;
    WAppend(output, L"状态：", 8192);
    WAppend(output, fields[2], 8192);
    WAppend(output, L"  ·  ", 8192);
    WAppend(output, fields[3], 8192);
    if (field > 9) { WAppend(output, L"\r\n连接/加密诊断：", 8192); WAppend(output, fields[9], 8192); }
    if (field > 4) { WAppend(output, L"\r\nTX加密前RTU：", 8192); WAppend(output, fields[4], 8192); }
    if (field > 5) { WAppend(output, L"\r\nTX加密后BLE载荷（本次实际写入FF02）：", 8192); WAppend(output, fields[5], 8192); }
    if (field > 6) { WAppend(output, L"\r\nRX原始BLE载荷（FF01/FF03通知）：", 8192); WAppend(output, fields[6], 8192); }
    if (field > 7) { WAppend(output, L"\r\nRX解密后RTU：", 8192); WAppend(output, fields[7], 8192); }
    if (field > 8) { WAppend(output, L"\r\n解析：", 8192); WAppend(output, fields[8], 8192); }
    SetWindowTextW(gBleModbusResultEdit, output);
    WCopy(gBleModbusStatus, fields[2][0] == L'O' ? L"已收到自定义 Modbus 响应" : L"自定义 Modbus 指令执行失败", 96);
}

static void RefreshBleTraffic(void)
{
    static wchar_t text[16384];
    wchar_t* content;
    wchar_t* lineEnd;
    DWORD sequence;
    if (ReadUtf16File(gBleTrafficPath, text, 16384) <= 0) { return; }
    if (!WStartsWithNoCase(text, L"FLOWSEQ\t")) { return; }
    content = text + 8;
    sequence = (DWORD)ParseSignedInt(content);
    lineEnd = content;
    while (*lineEnd != 0 && *lineEnd != L'\r' && *lineEnd != L'\n') { lineEnd++; }
    while (*lineEnd == L'\r' || *lineEnd == L'\n') { lineEnd++; }
    if (sequence == gBleTrafficSequence) { return; }
    gBleTrafficSequence = sequence;
    SetWindowTextW(gBleTrafficEdit, *lineEnd != 0 ? lineEnd : L"等待实际数据流……");
    SendMessageW(gBleTrafficEdit, 0x00B1, (WPARAM)-1, (LPARAM)-1);
    SendMessageW(gBleTrafficEdit, 0x00B7, 0, 0);
}

static void RefreshBleStatus(void)
{
    wchar_t text[1024];
    wchar_t* fields[6];
    int field = 0;
    wchar_t* cursor;
    if (ReadUtf16File(gBleStatusPath, text, 1024) <= 0) { return; }
    fields[field++] = text;
    cursor = text;
    while (*cursor != 0 && field < 6)
    {
        if (*cursor == L'\t' || *cursor == L'\r' || *cursor == L'\n')
        {
            *cursor = 0;
            fields[field++] = cursor + 1;
            if (*(cursor + 1) == L'\n') { cursor++; }
        }
        cursor++;
    }
    if (WStartsWithNoCase(fields[0], L"SCANNING"))
    {
        gBleScanning = TRUE; gBleConnecting = FALSE; gBleConnected = FALSE;
        WCopy(gBleStatusText, L"蓝牙状态：正在扫描附近 BLE 设备", 192);
    }
    else if (WStartsWithNoCase(fields[0], L"CONNECTING"))
    {
        gBleScanning = FALSE; gBleConnecting = TRUE;
        WCopy(gBleStatusText, field > 1 ? fields[1] : L"蓝牙状态：正在连接", 192);
    }
    else if (WStartsWithNoCase(fields[0], L"CONNECTED"))
    {
        gBleScanning = FALSE; gBleConnecting = FALSE; gBleConnected = TRUE;
        if (field > 1) { WCopy(gBleConnectedName, fields[1], 96); }
        if (field > 2) { WCopy(gBleConnectedMac, fields[2], 24); }
        if (field > 3) { gBleServiceCount = ParseSignedInt(fields[3]); }
        WCopy(gBleStatusText, L"蓝牙已连接", 192);
    }
    else if (WStartsWithNoCase(fields[0], L"ERROR"))
    {
        gBleScanning = FALSE; gBleConnecting = FALSE; gBleConnected = FALSE;
        WCopy(gBleStatusText, field > 1 ? fields[1] : L"蓝牙操作失败", 192);
    }
    else if (WStartsWithNoCase(fields[0], L"INFO"))
    {
        if (field > 1) { WCopy(gBleStatusText, fields[1], 192); }
    }
    else if (WStartsWithNoCase(fields[0], L"IDLE") || WStartsWithNoCase(fields[0], L"READY"))
    {
        gBleScanning = FALSE; gBleConnecting = FALSE;
        if (!gBleConnected) { WCopy(gBleStatusText, L"蓝牙状态：未连接", 192); }
    }
    else if (field > 1 && fields[1][0] != 0)
    {
        WCopy(gBleStatusText, fields[1], 192);
    }
}

static void StartBleScan(void)
{
    if (EnsureBleBackend())
    {
        gBleConnected = FALSE;
        gBleConnecting = FALSE;
        gBleSelectedDevice = -1;
        gBleSelectedPresent = FALSE;
        ResetBleRealtimeData();
        SendBleCommand(L"START");
        WCopy(gBleStatusText, L"蓝牙状态：正在启动扫描", 192);
    }
}

static void StopBleScan(void)
{
    if (gBleBackendStarted) { SendBleCommand(L"STOP"); }
    gBleScanning = FALSE;
}

static void ConnectSelectedBleDevice(void)
{
    wchar_t addressText[16];
    wchar_t command[128];
    static const wchar_t hex[] = L"0123456789ABCDEF";
    int index;
    if (!gBleHasSelectedDevice || !gBleSelectedPresent || gBleConnecting) { return; }
    for (index = 0; index < 12; index++)
    {
        int shift = (11 - index) * 4;
        addressText[index] = hex[(int)((gBleSelectedAddress >> shift) & 0x0F)];
    }
    addressText[12] = 0;
    wsprintfW(command, L"CONNECT\t%s\t%d", addressText, gBleSelectedAddressType);
    if (EnsureBleBackend() && SendBleCommand(command))
    {
        gBleConnecting = TRUE;
        gBleScanning = FALSE;
        WCopy(gBleStatusText, L"蓝牙状态：正在提交蓝牙连接", 192);
    }
}

static void DisconnectBleDevice(void)
{
    if (gBleBackendStarted) { SendBleCommand(L"DISCONNECT"); }
    gBleConnected = FALSE;
    gBleConnecting = FALSE;
    gBleServiceCount = 0;
    gBleConnectedName[0] = 0;
    gBleConnectedMac[0] = 0;
    ResetBleRealtimeData();
    WCopy(gBleStatusText, L"蓝牙状态：已断开", 192);
    StartBleScan();
}

static BOOL PointInRectSimple(const RECT* rect, int x, int y)
{
    return (x >= rect->left && x < rect->right && y >= rect->top && y < rect->bottom) ? TRUE : FALSE;
}

static void SetRectCoords(RECT* rect, int left, int top, int right, int bottom)
{
    rect->left = left;
    rect->top = top;
    rect->right = right;
    rect->bottom = bottom;
}

static int ParsePositiveInt(LPCWSTR text, int defaultValue, int minimum, int maximum)
{
    int index = 0;
    int value = 0;
    BOOL hasDigit = FALSE;
    while (text[index] >= L'0' && text[index] <= L'9')
    {
        hasDigit = TRUE;
        if (value < 1000000) { value = value * 10 + (text[index] - L'0'); }
        index++;
    }
    if (!hasDigit) { value = defaultValue; }
    if (value < minimum) { value = minimum; }
    if (value > maximum) { value = maximum; }
    return value;
}


static int HexDigitValue(wchar_t value)
{
    if (value >= L'0' && value <= L'9') { return value - L'0'; }
    if (value >= L'a' && value <= L'f') { return value - L'a' + 10; }
    if (value >= L'A' && value <= L'F') { return value - L'A' + 10; }
    return -1;
}

static BYTE ParseHexByte(LPCWSTR text, BYTE defaultValue)
{
    int index = 0;
    int digit;
    unsigned int value = 0;
    BOOL hasDigit = FALSE;
    if (text == NULL) { return defaultValue; }
    if (text[0] == L'0' && (text[1] == L'x' || text[1] == L'X')) { index = 2; }
    while ((digit = HexDigitValue(text[index])) >= 0)
    {
        hasDigit = TRUE;
        value = (value << 4) | (unsigned int)digit;
        if (value > 0xFFu) { value = 0xFFu; }
        index++;
    }
    return hasDigit ? (BYTE)value : defaultValue;
}

static DWORD BuildCanId(BYTE functionCode, BYTE targetAddress, BYTE sourceAddress)
{
    return 0x08000000u | ((DWORD)functionCode << 16) | ((DWORD)targetAddress << 8) | (DWORD)sourceAddress;
}

static BYTE CanIdFunction(DWORD canId) { return (BYTE)((canId >> 16) & 0xFFu); }
static BYTE CanIdTarget(DWORD canId) { return (BYTE)((canId >> 8) & 0xFFu); }
static BYTE CanIdSource(DWORD canId) { return (BYTE)(canId & 0xFFu); }

static LPCWSTR CanErrorText(BYTE errorCode)
{
    switch (errorCode)
    {
        case 0: return L"正常";
        case 1: return L"文件超过范围";
        case 2: return L"FLASH 擦除失败";
        case 3: return L"FLASH 写入失败";
        case 4: return L"FLASH 读取失败";
        case 5: return L"文件错误";
        case 6: return L"CRC16 错误";
        case 7: return L"CRC32 错误";
        case 8: return L"包序号错误";
        case 9: return L"超时重发";
        case 10: return L"设备无响应";
        case 11: return L"内存不足";
        case 12: return L"设备终止";
        case 13: return L"非法文件";
        case 14: return L"总线被占用";
        case 15: return L"总线错误";
        case 16: return L"文件无法打开";
        case 17: return L"镜像拷贝失败";
        default: return L"未知错误";
    }
}

/* 0=success, 1=retry, 2=fatal. */
static int EvaluateCanOtaResponse(BYTE status, BYTE errorCode)
{
    if (status == 0 && errorCode == 0) { return 0; }
    if (status == 1 || errorCode == 6 || errorCode == 8 || errorCode == 9) { return 1; }
    return 2;
}

static BOOL LoadControlCanLibrary(void)
{
    if (gControlCanModule != NULL) { return TRUE; }
    gControlCanModule = LoadLibraryW(L"ControlCAN.dll");
    if (gControlCanModule == NULL)
    {
        WCopy(gCanConnectionText, L"未找到 ControlCAN.dll，请放到 EXE 同目录", 160);
        return FALSE;
    }
    gVciOpenDevice = (PFN_VCI_OpenDevice)GetProcAddress(gControlCanModule, "VCI_OpenDevice");
    gVciCloseDevice = (PFN_VCI_CloseDevice)GetProcAddress(gControlCanModule, "VCI_CloseDevice");
    gVciInitCan = (PFN_VCI_InitCAN)GetProcAddress(gControlCanModule, "VCI_InitCAN");
    gVciStartCan = (PFN_VCI_StartCAN)GetProcAddress(gControlCanModule, "VCI_StartCAN");
    gVciResetCan = (PFN_VCI_ResetCAN)GetProcAddress(gControlCanModule, "VCI_ResetCAN");
    gVciClearBuffer = (PFN_VCI_ClearBuffer)GetProcAddress(gControlCanModule, "VCI_ClearBuffer");
    gVciTransmit = (PFN_VCI_Transmit)GetProcAddress(gControlCanModule, "VCI_Transmit");
    gVciReceive = (PFN_VCI_Receive)GetProcAddress(gControlCanModule, "VCI_Receive");
    gVciFindUsbDevice = (PFN_VCI_FindUsbDevice)GetProcAddress(gControlCanModule, "VCI_FindUsbDevice");
    if (gVciOpenDevice == NULL || gVciCloseDevice == NULL || gVciInitCan == NULL || gVciStartCan == NULL || gVciTransmit == NULL || gVciReceive == NULL)
    {
        FreeLibrary(gControlCanModule);
        gControlCanModule = NULL;
        gVciOpenDevice = NULL;
        gVciCloseDevice = NULL;
        gVciInitCan = NULL;
        gVciStartCan = NULL;
        gVciResetCan = NULL;
        gVciClearBuffer = NULL;
        gVciTransmit = NULL;
        gVciReceive = NULL;
        gVciFindUsbDevice = NULL;
        WCopy(gCanConnectionText, L"ControlCAN.dll 接口不完整或位数不匹配", 160);
        return FALSE;
    }
    return TRUE;
}

static void UnloadControlCanLibrary(void)
{
    if (gControlCanModule != NULL)
    {
        FreeLibrary(gControlCanModule);
        gControlCanModule = NULL;
    }
    gVciOpenDevice = NULL;
    gVciCloseDevice = NULL;
    gVciInitCan = NULL;
    gVciStartCan = NULL;
    gVciResetCan = NULL;
    gVciClearBuffer = NULL;
    gVciTransmit = NULL;
    gVciReceive = NULL;
    gVciFindUsbDevice = NULL;
}

static void GetCanTiming(DWORD baudRate, BYTE* timing0, BYTE* timing1)
{
    *timing0 = 0x00;
    *timing1 = 0x1C;
    if (baudRate == 125000u) { *timing0 = 0x03; *timing1 = 0x1C; }
    else if (baudRate == 250000u) { *timing0 = 0x01; *timing1 = 0x1C; }
    else if (baudRate == 500000u) { *timing0 = 0x00; *timing1 = 0x1C; }
    else if (baudRate == 800000u) { *timing0 = 0x00; *timing1 = 0x16; }
    else if (baudRate == 1000000u) { *timing0 = 0x00; *timing1 = 0x14; }
}

static void SetUpgradeStatus(LPCWSTR status, LPCWSTR stage)
{
    AcquireSRWLockExclusive(&gStateLock);
    if (status != NULL) { WCopy(gUpgradeStatus, status, 192); }
    if (stage != NULL) { WCopy(gProtocolStage, stage, 96); }
    ReleaseSRWLockExclusive(&gStateLock);
    if (gWindow != NULL) { PostMessageW(gWindow, WM_APP_UPGRADE_REFRESH, 0, 0); }
}

static void AddLog(LPCWSTR text)
{
    int index;
    wchar_t line[192];
    SYSTEMTIME time;
    GetLocalTime(&time);
    wsprintfW(line, L"%02u:%02u:%02u  %s", (UINT)time.wHour, (UINT)time.wMinute, (UINT)time.wSecond, text);
    AcquireSRWLockExclusive(&gStateLock);
    for (index = 0; index < 7; index++) { WCopy(gLogs[index], gLogs[index + 1], 192); }
    WCopy(gLogs[7], line, 192);
    ReleaseSRWLockExclusive(&gStateLock);
    if (gWindow != NULL) { PostMessageW(gWindow, WM_APP_UPGRADE_REFRESH, 0, 0); }
}


static COLORREF ThemeBackground(void) { return gDarkMode ? RGB(0,0,0) : RGB(245,245,247); }
static COLORREF ThemeTopBar(void) { return gDarkMode ? RGB(12,12,14) : RGB(255,255,255); }
static COLORREF ThemeSurface(void) { return gDarkMode ? RGB(28,28,30) : RGB(255,255,255); }
static COLORREF ThemeSurfaceAlt(void) { return gDarkMode ? RGB(36,36,38) : RGB(250,250,252); }
static COLORREF ThemeBorder(void) { return gDarkMode ? RGB(58,58,60) : RGB(220,220,225); }
static COLORREF ThemeGrid(void) { return gDarkMode ? RGB(15,15,18) : RGB(239,239,243); }
static COLORREF ThemeText(void) { return gDarkMode ? RGB(245,245,247) : RGB(29,29,31); }
static COLORREF ThemeMuted(void) { return gDarkMode ? RGB(174,174,178) : RGB(99,99,102); }
static COLORREF ThemeMuted2(void) { return gDarkMode ? RGB(99,99,102) : RGB(142,142,147); }
static COLORREF ThemeAccent(void) { return gDarkMode ? RGB(10,132,255) : RGB(0,113,227); }
static COLORREF ThemeAccentSoft(void) { return gDarkMode ? RGB(20,43,70) : RGB(235,245,255); }
static COLORREF ThemeInputFill(void) { return gDarkMode ? RGB(28,28,30) : RGB(250,250,252); }
static COLORREF ThemeInputBorder(void) { return gDarkMode ? RGB(72,72,74) : RGB(209,209,214); }
static COLORREF ThemeShadow(void) { return gDarkMode ? RGB(0,0,0) : RGB(232,232,236); }
static COLORREF ThemeSuccess(void) { return RGB(48,209,88); }
static COLORREF ThemeDanger(void) { return RGB(255,69,58); }
static COLORREF ThemeWarning(void) { return RGB(255,159,10); }

static void UseFillAndPen(HDC dc, COLORREF fillColor, COLORREF penColor)
{
    SelectObject(dc, GetStockObject(DC_BRUSH));
    SelectObject(dc, GetStockObject(DC_PEN));
    SetDCBrushColor(dc, fillColor);
    SetDCPenColor(dc, penColor);
}

static void DrawRoundBox(HDC dc, const RECT* rect, int radius, COLORREF fillColor, COLORREF borderColor)
{
    UseFillAndPen(dc, fillColor, borderColor);
    RoundRect(dc, rect->left, rect->top, rect->right, rect->bottom, radius, radius);
}

static void DrawTextBlock(HDC dc, LPCWSTR text, RECT rect, HFONT font, COLORREF color, UINT format)
{
    SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, text, -1, &rect, format | DT_NOPREFIX);
}

static void DrawLine(HDC dc, int x1, int y1, int x2, int y2, COLORREF color)
{
    SelectObject(dc, GetStockObject(DC_PEN));
    SetDCPenColor(dc, color);
    MoveToEx(dc, x1, y1, NULL);
    LineTo(dc, x2, y2);
}

static void DrawPill(HDC dc, RECT rect, LPCWSTR text, COLORREF fill, COLORREF border, COLORREF textColor, BOOL highlighted)
{
    if (highlighted) { fill = gDarkMode ? RGB(22,43,66) : RGB(235,244,255); }
    DrawRoundBox(dc, &rect, 20, fill, border);
    DrawTextBlock(dc, text, rect, gFontSmall, textColor, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void DrawBluetoothIcon(HDC dc, int cx, int cy, COLORREF color)
{
    RECT circle = { cx - 50, cy - 50, cx + 50, cy + 50 };
    UseFillAndPen(dc, color, color);
    Ellipse(dc, circle.left, circle.top, circle.right, circle.bottom);
    SelectObject(dc, GetStockObject(DC_PEN));
    SetDCPenColor(dc, RGB(255, 255, 255));
    MoveToEx(dc, cx, cy - 28, NULL); LineTo(dc, cx, cy + 28);
    MoveToEx(dc, cx, cy - 28, NULL); LineTo(dc, cx + 22, cy - 8);
    MoveToEx(dc, cx + 22, cy - 8, NULL); LineTo(dc, cx - 18, cy + 24);
    MoveToEx(dc, cx, cy + 28, NULL); LineTo(dc, cx + 22, cy + 8);
    MoveToEx(dc, cx + 22, cy + 8, NULL); LineTo(dc, cx - 18, cy - 24);
}

static void DrawSerialIcon(HDC dc, int cx, int cy, COLORREF color)
{
    RECT circle = { cx - 50, cy - 50, cx + 50, cy + 50 };
    RECT body = { cx - 28, cy - 19, cx + 28, cy + 17 };
    int x;
    UseFillAndPen(dc, color, color);
    Ellipse(dc, circle.left, circle.top, circle.right, circle.bottom);
    UseFillAndPen(dc, color, RGB(255,255,255));
    RoundRect(dc, body.left, body.top, body.right, body.bottom, 8, 8);
    for (x = -16; x <= 16; x += 8)
    {
        UseFillAndPen(dc, RGB(255,255,255), RGB(255,255,255));
        Ellipse(dc, cx + x - 2, cy - 6, cx + x + 2, cy - 2);
        Ellipse(dc, cx + x - 2, cy + 4, cx + x + 2, cy + 8);
    }
    DrawLine(dc, cx - 18, cy + 18, cx - 18, cy + 26, RGB(255,255,255));
    DrawLine(dc, cx + 18, cy + 18, cx + 18, cy + 26, RGB(255,255,255));
}

static void DrawCanIcon(HDC dc, int cx, int cy, COLORREF color)
{
    RECT circle = { cx - 50, cy - 50, cx + 50, cy + 50 };
    UseFillAndPen(dc, color, color);
    Ellipse(dc, circle.left, circle.top, circle.right, circle.bottom);
    DrawLine(dc, cx - 22, cy - 13, cx + 22, cy - 13, RGB(255,255,255));
    DrawLine(dc, cx - 22, cy + 13, cx + 22, cy + 13, RGB(255,255,255));
    UseFillAndPen(dc, color, RGB(255,255,255));
    Ellipse(dc, cx - 29, cy - 20, cx - 15, cy - 6);
    Ellipse(dc, cx + 15, cy - 20, cx + 29, cy - 6);
    Ellipse(dc, cx - 29, cy + 6, cx - 15, cy + 20);
    Ellipse(dc, cx + 15, cy + 6, cx + 29, cy + 20);
}


static void CalculateHomeLayout(const RECT* client)
{
    int width = client->right;
    int margin = 72;
    int gap = 24;
    int cardTop = 286;
    int cardBottom = client->bottom - 74;
    int cardWidth;
    int index;

    if (cardBottom < cardTop + 380) { cardBottom = cardTop + 380; }
    cardWidth = (width - margin * 2 - gap * 2) / 3;
    for (index = 0; index < 3; index++)
    {
        gCardRects[index].left = margin + index * (cardWidth + gap);
        gCardRects[index].top = cardTop;
        gCardRects[index].right = gCardRects[index].left + cardWidth;
        gCardRects[index].bottom = cardBottom;
    }
}


static void DrawBackground(HDC dc, const RECT* client)
{
    HBRUSH brush;
    int x;
    int y;
    RECT top = { 0, 0, client->right, 66 };
    RECT footer = { 0, client->bottom - 46, client->right, client->bottom };

    brush = CreateSolidBrush(ThemeBackground());
    FillRect(dc, client, brush);
    DeleteObject(brush);

    brush = CreateSolidBrush(ThemeTopBar());
    FillRect(dc, &top, brush);
    FillRect(dc, &footer, brush);
    DeleteObject(brush);

    for (x = 0; x < client->right; x += 64) { DrawLine(dc, x, 66, x, client->bottom - 46, ThemeGrid()); }
    for (y = 66; y < client->bottom - 46; y += 64) { DrawLine(dc, 0, y, client->right, y, ThemeGrid()); }

    /* Low-contrast technological halo in the upper-right background. */
    UseFillAndPen(dc, gDarkMode ? RGB(5,18,34) : RGB(239,246,255), gDarkMode ? RGB(16,44,72) : RGB(225,238,253));
    Ellipse(dc, client->right - 420, 84, client->right + 150, 654);
    UseFillAndPen(dc, ThemeBackground(), gDarkMode ? RGB(20,46,75) : RGB(231,241,253));
    Ellipse(dc, client->right - 300, 204, client->right + 30, 534);

    DrawLine(dc, 0, 65, client->right, 65, ThemeBorder());
    DrawLine(dc, 0, client->bottom - 46, client->right, client->bottom - 46, ThemeBorder());
}


static void DrawTopBar(HDC dc, const RECT* client)
{
    RECT brand = { 28, 8, 520, 56 };
    RECT version = { client->right - 420, 8, client->right - 190, 56 };
    RECT themeText;

    DrawTextBlock(dc, L"BLUETTI  /  DEVICE STUDIO", brand, gFontSmall, ThemeText(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawTextBlock(dc, L"SERIAL · CAN · BLE · OTA   |   V1.0", version, gFontTiny, ThemeMuted(), DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    gThemeRect.left = client->right - 174;
    gThemeRect.top = 14;
    gThemeRect.right = client->right - 28;
    gThemeRect.bottom = 52;
    DrawRoundBox(dc, &gThemeRect, 20, ThemeSurfaceAlt(), gHoverItem == 30 ? ThemeAccent() : ThemeBorder());
    themeText = gThemeRect;
    DrawTextBlock(dc, gDarkMode ? L"☀  浅色模式" : L"☾  深色模式", themeText, gFontSmall, gHoverItem == 30 ? ThemeAccent() : ThemeText(), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}


static void DrawFooter(HDC dc, const RECT* client)
{
    RECT left = { 56, client->bottom - 42, 500, client->bottom - 4 };
    RECT center = { 470, client->bottom - 42, client->right - 470, client->bottom - 4 };
    RECT right = { client->right - 500, client->bottom - 42, client->right - 28, client->bottom - 4 };
    BOOL connected = FALSE;
    LPCWSTR statusText = L"连接状态：未连接";
    LPCWSTR protocolText = L"SERIAL  /  CAN  /  BLE CONNECTION";
    if (gCurrentPage == 1)
    {
        connected = gBleConnected;
        statusText = gBleConnected ? L"蓝牙已连接" : gBleStatusText;
        protocolText = L"BLE SCAN  /  PREFIX FILTER  /  GATT CONNECT";
    }
    else if (gCurrentPage == 6)
    {
        connected = gBleConnected;
        statusText = gBleOtaRunning ? gBleOtaMessage : (gBleConnected ? L"BLE OTA 已就绪" : L"BLE OTA 等待设备重连");
        protocolText = L"BLE OTA  /  MODBUS START  /  XMODEM-1K  /  AUTO RECONNECT";
    }
    else if (gCurrentPage == 2)
    {
        connected = gSerialConnected;
        statusText = gSerialConnected ? L"连接状态：串口已连接" : L"连接状态：串口未连接";
        protocolText = L"MODBUS RTU  /  XMODEM-1K";
    }
    else if (gCurrentPage == 3 || gCurrentPage == 4 || gCurrentPage == 5)
    {
        connected = gCanConnected;
        statusText = gCanConnected ? gCanConnectionText : L"连接状态：CANalyst-II 未连接";
        protocolText = L"CAN 29-BIT EXT  /  1K BLOCK OTA";
    }
    UseFillAndPen(dc, connected ? ThemeSuccess() : ThemeMuted2(), connected ? ThemeSuccess() : ThemeMuted2());
    Ellipse(dc, 32, client->bottom - 28, 42, client->bottom - 18);
    DrawTextBlock(dc, statusText, left, gFontTiny, ThemeMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    DrawTextBlock(dc, L"BLUETTI Device Studio  ·  CLEAN ENERGY SOFTWARE", center, gFontTiny, ThemeMuted2(), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    DrawTextBlock(dc, protocolText, right, gFontTiny, ThemeMuted(), DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
}


static void DrawHomePage(HDC dc, const RECT* client)
{
    static const wchar_t* titles[3] = { L"蓝牙设备", L"串口升级", L"CAN 升级" };
    static const wchar_t* english[3] = { L"BLUETOOTH", L"SERIAL OTA", L"CAN OTA" };
    static const wchar_t* descriptions[3] = {
        L"扫描并连接设备，进入实时数据、控制与诊断工作台",
        L"通过 COM 端口完成固件识别、批量升级与可靠性测试",
        L"连接 CANalyst-II，执行单节点或广播固件升级"
    };
    static const wchar_t* numbers[3] = { L"01", L"02", L"03" };
    static const wchar_t* tags[3] = { L"CONNECT", L"OTA", L"OTA" };
    COLORREF accents[3] = { RGB(10,132,255), RGB(48,209,88), RGB(255,159,10) };
    int index;
    RECT eyebrow = { 70, 92, client->right - 70, 124 };
    RECT title = { 70, 119, client->right - 70, 174 };
    RECT subtitle = { 70, 174, client->right - 70, 208 };
    RECT ready = { client->right / 2 - 142, 219, client->right / 2 + 142, 256 };

    CalculateHomeLayout(client);

    DrawTextBlock(dc, L"BLUETTI  ·  DEVICE ENGINEERING", eyebrow, gFontTiny, ThemeAccent(), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    DrawTextBlock(dc, L"BLUETTI Device Studio", title, gFontLogo, ThemeText(), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    DrawTextBlock(dc, L"设备连接 · 通信诊断 · 固件管理", subtitle, gFontBody, ThemeMuted(), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    DrawRoundBox(dc, &ready, 20, ThemeAccentSoft(), gDarkMode ? RGB(27,86,115) : RGB(193,220,255));
    DrawTextBlock(dc, L"●  SYSTEM ONLINE   ·   SERIAL + CAN + BLE READY", ready, gFontTiny, ThemeSuccess(), DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    for (index = 0; index < 3; index++)
    {
        RECT card = gCardRects[index];
        RECT shadow = { card.left + 3, card.top + 7, card.right + 3, card.bottom + 7 };
        RECT number = { card.left + 26, card.top + 22, card.left + 86, card.top + 54 };
        RECT state = { card.right - 154, card.top + 22, card.right - 26, card.top + 54 };
        RECT icon = { card.left + 34, card.top + 78, card.left + 126, card.top + 170 };
        RECT heading = { card.left + 34, card.top + 188, card.right - 34, card.top + 230 };
        RECT sub = { card.left + 34, card.top + 226, card.right - 34, card.top + 258 };
        RECT description = { card.left + 34, card.top + 276, card.right - 34, card.top + 334 };
        RECT enter = { card.left + 34, card.bottom - 66, card.right - 34, card.bottom - 24 };
        RECT rail = { card.left + 34, card.bottom - 88, card.right - 34, card.bottom - 84 };
        BOOL hover = (gHoverItem == index + 1);
        COLORREF border = hover ? accents[index] : ThemeBorder();
        COLORREF cardFill = hover ? (gDarkMode ? RGB(32,32,36) : RGB(255,255,255)) : ThemeSurface();

        DrawRoundBox(dc, &shadow, 28, ThemeShadow(), ThemeShadow());
        DrawRoundBox(dc, &card, 28, cardFill, border);
        DrawTextBlock(dc, numbers[index], number, gFontSubtitle, accents[index], DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        DrawPill(dc, state, tags[index], ThemeSurfaceAlt(), border, accents[index], FALSE);
        DrawRoundBox(dc, &icon, 28, gDarkMode ? RGB(36,36,40) : RGB(247,247,250), border);
        if (index == 0) { DrawBluetoothIcon(dc, (icon.left + icon.right) / 2, (icon.top + icon.bottom) / 2, accents[index]); }
        else if (index == 1) { DrawSerialIcon(dc, (icon.left + icon.right) / 2, (icon.top + icon.bottom) / 2, accents[index]); }
        else { DrawCanIcon(dc, (icon.left + icon.right) / 2, (icon.top + icon.bottom) / 2, accents[index]); }

        DrawTextBlock(dc, titles[index], heading, gFontCardTitle, ThemeText(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        DrawTextBlock(dc, english[index], sub, gFontSmall, accents[index], DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        DrawTextBlock(dc, descriptions[index], description, gFontSmall, ThemeMuted(), DT_LEFT | DT_WORDBREAK);
        DrawRoundBox(dc, &rail, 4, gDarkMode ? RGB(33,47,68) : RGB(230,236,245), gDarkMode ? RGB(33,47,68) : RGB(230,236,245));
        if (hover)
        {
            RECT activeRail = rail;
            activeRail.right = activeRail.left + (rail.right - rail.left) * 2 / 3;
            DrawRoundBox(dc, &activeRail, 4, accents[index], accents[index]);
        }
        DrawTextBlock(dc, hover ? (index == 0 ? L"打开连接工作台   →" : L"打开升级工作台   →") : L"进入模块   →", enter, gFontBody, hover ? accents[index] : ThemeText(), DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }
}

static void SetBleControlsVisible(BOOL visible)
{
    BOOL pageVisible = visible && !gChipDialogOpen;
    BOOL dashboardVisible = pageVisible && gBleConnected;
    ShowWindow(gBleFilterEdit, pageVisible && !gBleConnected ? SW_SHOW : SW_HIDE);
    /* V1.3.9: selection is custom-drawn; legacy ComboBox is backing data only. */
    ShowWindow(gBleIntervalCombo, SW_HIDE);
    ShowWindow(gBleSlaveEdit, dashboardVisible ? SW_SHOW : SW_HIDE);
    ShowWindow(gBleModbusSlaveEdit, dashboardVisible ? SW_SHOW : SW_HIDE);
    ShowWindow(gBleModbusFunctionEdit, dashboardVisible ? SW_SHOW : SW_HIDE);
    ShowWindow(gBleModbusRegisterEdit, dashboardVisible ? SW_SHOW : SW_HIDE);
    ShowWindow(gBleModbusValueEdit, dashboardVisible ? SW_SHOW : SW_HIDE);
    ShowWindow(gBleModbusTimeoutEdit, dashboardVisible ? SW_SHOW : SW_HIDE);
    /* Native multiline EDITs caused the white scrollbar/flicker and stale content.
     * Keep them hidden as data backends; the parent window paints their content. */
    ShowWindow(gBleModbusResultEdit, SW_HIDE);
    ShowWindow(gBleVersionListEdit, SW_HIDE);
}

static void RefreshParentAfterChildVisibility(void)
{
    InvalidateRect(gWindow, NULL, TRUE);
    UpdateWindow(gWindow);
}

static void LayoutBleControls(const RECT* client)
{
    int identityRight = client->right - 78;
    int lowerLeft = 78;
    int lowerRight = client->right - 78;
    int totalWidth = lowerRight - lowerLeft;
    int statusRight = lowerLeft + totalWidth * 32 / 100;
    int modbusLeft = statusRight + 12;
    int modbusRight = lowerRight;
    int statusTop = 682;
    int resultTop = statusTop + 91;
    int resultHeight = client->bottom - 84 - resultTop - 12;
    int modbusWidth = modbusRight - modbusLeft;
    if (resultHeight < 42) { resultHeight = 42; }
    MoveWindow(gBleFilterEdit, 86, 203, 470, 34, TRUE);
    MoveWindow(gBleSlaveEdit, identityRight - 680, 215, 84, 34, TRUE);
    MoveWindow(gBleModbusSlaveEdit, modbusLeft + 18, statusTop + 48, 48, 32, TRUE);
    MoveWindow(gBleModbusFunctionEdit, modbusLeft + 76, statusTop + 48, 50, 32, TRUE);
    MoveWindow(gBleModbusRegisterEdit, modbusLeft + 136, statusTop + 48, 78, 32, TRUE);
    MoveWindow(gBleModbusValueEdit, modbusLeft + 224, statusTop + 48, 78, 32, TRUE);
    MoveWindow(gBleModbusTimeoutEdit, modbusLeft + 312, statusTop + 48, 72, 32, TRUE);
    MoveWindow(gBleModbusResultEdit, modbusLeft + 18, resultTop, modbusWidth - 36, resultHeight, TRUE);
    {
        int versionTop = statusTop + 109;
        int versionBottom = client->bottom - 94;
        int versionHeight = versionBottom - versionTop;
        if (versionHeight < 34) { versionHeight = 34; }
        (void)versionTop; (void)versionHeight;
    }
}

static void DrawBlePage(HDC dc, const RECT* client)
{
    int left = 54;
    int right = client->right - 54;
    int bottom = client->bottom - 60;
    RECT title = { 194, 78, right, 118 };
    RECT subtitle = { 194, 114, right, 144 };
    RECT toolbar = { left, 150, right, 264 };
    RECT listPanel = { left, 282, right - 380, bottom };
    RECT sidePanel = { right - 360, 282, right, bottom };
    RECT label;
    int rowHeight = 58;
    int index;
    wchar_t text[256];

    gBackRect.left = left; gBackRect.top = 82; gBackRect.right = left + 118; gBackRect.bottom = 122;
    DrawPill(dc, gBackRect, L"←  返回首页", ThemeSurface(), ThemeBorder(), ThemeText(), gHoverItem == 10);

    if (gBleConnected)
    {
        RECT product = { left, 150, right, bottom };
        RECT identity = { product.left + 24, product.top + 20, product.right - 24, product.top + 126 };
        RECT socCard = { product.left + 24, product.top + 146, product.left + 342, product.top + 378 };
        int metricLeft = socCard.right + 18;
        int metricRight = product.right - 24;
        int metricGap = 16;
        int metricWidth = (metricRight - metricLeft - metricGap) / 2;
        RECT acOutCard = { metricLeft, product.top + 146, metricLeft + metricWidth, product.top + 252 };
        RECT dcOutCard = { acOutCard.right + metricGap, acOutCard.top, metricRight, acOutCard.bottom };
        RECT pvInCard = { metricLeft, product.top + 270, metricLeft + metricWidth, product.top + 378 };
        RECT acInCard = { pvInCard.right + metricGap, pvInCard.top, metricRight, pvInCard.bottom };
        RECT controlPanel = { product.left + 24, product.top + 398, product.right - 24, product.top + 512 };
        RECT acControlCard = { controlPanel.left + 18, controlPanel.top + 40, (controlPanel.left + controlPanel.right) / 2 - 9, controlPanel.bottom - 14 };
        RECT dcControlCard = { (controlPanel.left + controlPanel.right) / 2 + 9, controlPanel.top + 40, controlPanel.right - 18, controlPanel.bottom - 14 };
        RECT lowerPanel = { product.left + 24, product.top + 532, product.right - 24, product.bottom - 24 };
        int lowerWidth = lowerPanel.right - lowerPanel.left;
        RECT statusPanel = { lowerPanel.left, lowerPanel.top, lowerPanel.left + lowerWidth * 32 / 100, lowerPanel.bottom };
        RECT modbusPanel = { statusPanel.right + 12, lowerPanel.top, lowerPanel.right, lowerPanel.bottom };
        RECT valueRect;
        RECT bar;
        RECT fill;
        COLORREF acStateColor = gBleAcState > 0 ? ThemeSuccess() : ThemeMuted2();
        COLORREF dcStateColor = gBleDcState > 0 ? ThemeSuccess() : ThemeMuted2();

        DrawTextBlock(dc, L"设备能量中心", title, gFontTitle, ThemeText(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        DrawTextBlock(dc, L"蓝牙加密 Modbus 实时监控与输出控制", subtitle, gFontSmall, ThemeSuccess(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        DrawRoundBox(dc, &product, 28, ThemeSurface(), ThemeBorder());
        DrawRoundBox(dc, &identity, 20, ThemeSurfaceAlt(), ThemeBorder());

        label.left = identity.left + 22; label.top = identity.top + 15; label.right = identity.right - 500; label.bottom = identity.top + 50;
        DrawTextBlock(dc, gBleConnectedName[0] ? gBleConnectedName : L"BLE Device", label, gFontCardTitle, ThemeText(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        label.top = identity.top + 50; label.bottom = identity.top + 80;
        DrawTextBlock(dc, gBleConnectedMac[0] ? gBleConnectedMac : L"--:--:--:--:--:--", label, gFontSmall, ThemeMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        label.left = identity.left + 300; label.right = identity.left + 520; label.top = identity.top + 50; label.bottom = identity.top + 80;
        DrawTextBlock(dc, gBleDataValid ? L"●  MODBUS ONLINE" : L"●  BLE CONNECTED", label, gFontSmall, ThemeSuccess(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        label.left = identity.right - 680; label.top = identity.top + 14; label.right = identity.right - 590; label.bottom = identity.top + 40;
        DrawTextBlock(dc, L"从机地址", label, gFontTiny, ThemeMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        { RECT shell = { identity.right - 684, identity.top + 47, identity.right - 590, identity.top + 87 }; DrawInputShell(dc, shell, TRUE); }
        gBleSlaveApplyRect.left = identity.right - 578; gBleSlaveApplyRect.top = identity.top + 47; gBleSlaveApplyRect.right = identity.right - 458; gBleSlaveApplyRect.bottom = identity.top + 87;
        DrawButton(dc, gBleSlaveApplyRect, L"应用地址", ThemeSurface(), ThemeAccent(), ThemeAccent(), gHoverItem == 56);
        label.left = identity.right - 438; label.top = identity.top + 14; label.right = identity.right - 342; label.bottom = identity.top + 40;
        DrawTextBlock(dc, L"刷新周期", label, gFontTiny, ThemeMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        gBleIntervalSelectorRect.left = identity.right - 442; gBleIntervalSelectorRect.top = identity.top + 47;
        gBleIntervalSelectorRect.right = identity.right - 338; gBleIntervalSelectorRect.bottom = identity.top + 87;
        DrawUiSelectorControl(dc, &gBleIntervalSelectorRect, UI_SELECTOR_BLE_INTERVAL, gUiSelectorOpen == UI_SELECTOR_BLE_INTERVAL, gHoverItem == 185);
        gBleOtaEntryRect.left = identity.right - 322; gBleOtaEntryRect.top = identity.top + 8; gBleOtaEntryRect.right = identity.right - 202; gBleOtaEntryRect.bottom = identity.top + 39;
        DrawButton(dc, gBleOtaEntryRect, L"OTA 一键升级", ThemeAccentSoft(), ThemeAccent(), ThemeAccent(), gHoverItem == 58);
        gBleRefreshRect.left = identity.right - 322; gBleRefreshRect.top = identity.top + 47; gBleRefreshRect.right = identity.right - 202; gBleRefreshRect.bottom = identity.top + 87;
        DrawButton(dc, gBleRefreshRect, L"立即刷新", ThemeSurface(), ThemeAccent(), ThemeAccent(), gHoverItem == 55);
        gBleDisconnectRect.left = identity.right - 184; gBleDisconnectRect.top = identity.top + 47; gBleDisconnectRect.right = identity.right - 20; gBleDisconnectRect.bottom = identity.top + 87;
        DrawButton(dc, gBleDisconnectRect, L"断开蓝牙", ThemeSurface(), ThemeDanger(), ThemeDanger(), gHoverItem == 52);

        DrawRoundBox(dc, &socCard, 22, ThemeAccentSoft(), ThemeAccent());
        label.left = socCard.left + 22; label.top = socCard.top + 18; label.right = socCard.right - 22; label.bottom = socCard.top + 46;
        DrawTextBlock(dc, L"电池电量  /  SOC", label, gFontSmall, ThemeMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        valueRect.left = socCard.left + 22; valueRect.top = socCard.top + 54; valueRect.right = socCard.right - 22; valueRect.bottom = socCard.top + 140;
        if (gBleDataValid) { wsprintfW(text, L"%d%%", gBleSoc); } else { WCopy(text, L"--%", 256); }
        DrawTextBlock(dc, text, valueRect, gFontPercent, ThemeAccent(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        bar.left = socCard.left + 22; bar.top = socCard.bottom - 52; bar.right = socCard.right - 22; bar.bottom = bar.top + 12;
        DrawRoundBox(dc, &bar, 6, gDarkMode ? RGB(37,52,72) : RGB(226,234,244), gDarkMode ? RGB(37,52,72) : RGB(226,234,244));
        fill = bar;
        if (gBleDataValid) { int soc = gBleSoc < 0 ? 0 : (gBleSoc > 100 ? 100 : gBleSoc); fill.right = fill.left + (bar.right - bar.left) * soc / 100; }
        else { fill.right = fill.left; }
        if (fill.right > fill.left) { DrawRoundBox(dc, &fill, 6, ThemeAccent(), ThemeAccent()); }
        label.left = socCard.left + 22; label.top = socCard.bottom - 34; label.right = socCard.right - 22; label.bottom = socCard.bottom - 12;
        DrawTextBlock(dc, gBleDataValid ? L"设备实时上报" : L"等待首次轮询", label, gFontTiny, ThemeMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);

#define DRAW_BLE_POWER_CARD(cardRect, cardTitle, powerValue, valueColor) \
        DrawRoundBox(dc, &(cardRect), 18, ThemeSurfaceAlt(), ThemeBorder()); \
        label.left = (cardRect).left + 18; label.top = (cardRect).top + 12; label.right = (cardRect).right - 18; label.bottom = (cardRect).top + 36; \
        DrawTextBlock(dc, (cardTitle), label, gFontTiny, ThemeMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE); \
        if (gBleDataValid) { wsprintfW(text, L"%d W", (powerValue)); } else { WCopy(text, L"-- W", 256); } \
        label.top = (cardRect).top + 40; label.bottom = (cardRect).bottom - 12; \
        DrawTextBlock(dc, text, label, gFontCardTitle, (valueColor), DT_LEFT | DT_VCENTER | DT_SINGLELINE)
        DRAW_BLE_POWER_CARD(acOutCard, L"AC 输出功率", gBleAcOutputPower, ThemeAccent());
        DRAW_BLE_POWER_CARD(dcOutCard, L"DC 输出功率", gBleDcOutputPower, ThemeWarning());
        DRAW_BLE_POWER_CARD(pvInCard, L"PV 输入功率", gBlePvInputPower, ThemeSuccess());
        DRAW_BLE_POWER_CARD(acInCard, L"AC 输入功率", gBleAcInputPower, ThemeText());
#undef DRAW_BLE_POWER_CARD

        DrawRoundBox(dc, &controlPanel, 20, ThemeSurfaceAlt(), ThemeBorder());
        label.left = controlPanel.left + 18; label.top = controlPanel.top + 10; label.right = controlPanel.right - 18; label.bottom = controlPanel.top + 38;
        DrawTextBlock(dc, L"输出控制", label, gFontSmall, ThemeText(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        DrawRoundBox(dc, &acControlCard, 16, ThemeSurface(), ThemeBorder());
        label.left = acControlCard.left + 18; label.top = acControlCard.top + 12; label.right = acControlCard.left + 170; label.bottom = acControlCard.bottom - 12;
        DrawTextBlock(dc, L"AC 输出", label, gFontBody, ThemeText(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        label.left = acControlCard.left + 130; label.right = acControlCard.left + 240;
        DrawTextBlock(dc, gBleAcState > 0 ? L"已开启" : (gBleAcState == 0 ? L"已关闭" : L"未知"), label, gFontSmall, acStateColor, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        gBleAcControlRect.left = acControlCard.right - 170; gBleAcControlRect.top = acControlCard.top + 10; gBleAcControlRect.right = acControlCard.right - 16; gBleAcControlRect.bottom = acControlCard.bottom - 10;
        DrawButton(dc, gBleAcControlRect, gBleAcState > 0 ? L"关闭 AC" : L"开启 AC", gBleDataValid ? (gBleAcState > 0 ? ThemeSurfaceAlt() : ThemeSuccess()) : ThemeSurfaceAlt(), gBleDataValid ? (gBleAcState > 0 ? ThemeDanger() : ThemeSuccess()) : ThemeBorder(), gBleDataValid ? (gBleAcState > 0 ? ThemeDanger() : RGB(255,255,255)) : ThemeMuted(), gHoverItem == 53);

        DrawRoundBox(dc, &dcControlCard, 16, ThemeSurface(), ThemeBorder());
        label.left = dcControlCard.left + 18; label.top = dcControlCard.top + 12; label.right = dcControlCard.left + 170; label.bottom = dcControlCard.bottom - 12;
        DrawTextBlock(dc, L"DC 输出", label, gFontBody, ThemeText(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        label.left = dcControlCard.left + 130; label.right = dcControlCard.left + 240;
        DrawTextBlock(dc, gBleDcState > 0 ? L"已开启" : (gBleDcState == 0 ? L"已关闭" : L"未知"), label, gFontSmall, dcStateColor, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        gBleDcControlRect.left = dcControlCard.right - 170; gBleDcControlRect.top = dcControlCard.top + 10; gBleDcControlRect.right = dcControlCard.right - 16; gBleDcControlRect.bottom = dcControlCard.bottom - 10;
        DrawButton(dc, gBleDcControlRect, gBleDcState > 0 ? L"关闭 DC" : L"开启 DC", gBleDataValid ? (gBleDcState > 0 ? ThemeSurfaceAlt() : ThemeSuccess()) : ThemeSurfaceAlt(), gBleDataValid ? (gBleDcState > 0 ? ThemeDanger() : ThemeSuccess()) : ThemeBorder(), gBleDataValid ? (gBleDcState > 0 ? ThemeDanger() : RGB(255,255,255)) : ThemeMuted(), gHoverItem == 54);

        DrawRoundBox(dc, &statusPanel, 18, ThemeSurfaceAlt(), ThemeBorder());
        label.left = statusPanel.left + 18; label.top = statusPanel.top + 8; label.right = statusPanel.right - 18; label.bottom = statusPanel.top + 30;
        DrawTextBlock(dc, gBleDataStatus, label, gFontSmall, gBleDataValid ? ThemeSuccess() : ThemeWarning(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        wsprintfW(text, L"%s  ·  %d秒  ·  从机%d", gBleLastUpdate, gBlePollInterval, gBleConfiguredSlaveId);
        label.top = statusPanel.top + 29; label.bottom = statusPanel.top + 48;
        DrawTextBlock(dc, text, label, gFontTiny, ThemeMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        wsprintfW(text, L"设备类型：%s", gBleDeviceType);
        label.top = statusPanel.top + 48; label.bottom = statusPanel.top + 67;
        DrawTextBlock(dc, text, label, gFontTiny, ThemeText(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        wsprintfW(text, L"SN：%s", gBleDeviceSn);
        label.top = statusPanel.top + 67; label.bottom = statusPanel.top + 86;
        DrawTextBlock(dc, text, label, gFontTiny, ThemeText(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        label.top = statusPanel.top + 86; label.bottom = statusPanel.top + 106;
        DrawTextBlock(dc, L"版本列表（APP / BOOT）", label, gFontTiny, ThemeMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        gBleVersionViewportRect.left = statusPanel.left + 12; gBleVersionViewportRect.top = statusPanel.top + 105;
        gBleVersionViewportRect.right = statusPanel.right - 12; gBleVersionViewportRect.bottom = statusPanel.bottom - 8;
        DrawBleVersionViewport(dc, &gBleVersionViewportRect);

        DrawRoundBox(dc, &modbusPanel, 18, ThemeSurfaceAlt(), ThemeBorder());
        label.left = modbusPanel.left + 18; label.top = modbusPanel.top + 8; label.right = modbusPanel.right - 18; label.bottom = modbusPanel.top + 31;
        DrawTextBlock(dc, L"Modbus 加密指令构建器", label, gFontSmall, ThemeText(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        label.top = modbusPanel.top + 28; label.bottom = modbusPanel.top + 48;
        DrawTextBlock(dc, gBleModbusStatus, label, gFontTiny, ThemeMuted(), DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        label.left = modbusPanel.left + 18; label.right = modbusPanel.left + 66; label.top = modbusPanel.top + 28; label.bottom = modbusPanel.top + 47;
        DrawTextBlock(dc, L"从机", label, gFontTiny, ThemeMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        label.left = modbusPanel.left + 76; label.right = modbusPanel.left + 126;
        DrawTextBlock(dc, L"功能", label, gFontTiny, ThemeMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        label.left = modbusPanel.left + 136; label.right = modbusPanel.left + 214;
        DrawTextBlock(dc, L"寄存器", label, gFontTiny, ThemeMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        label.left = modbusPanel.left + 224; label.right = modbusPanel.left + 302;
        DrawTextBlock(dc, L"长度/值", label, gFontTiny, ThemeMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        label.left = modbusPanel.left + 312; label.right = modbusPanel.left + 384;
        DrawTextBlock(dc, L"超时", label, gFontTiny, ThemeMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        { RECT shell = { modbusPanel.left + 14, modbusPanel.top + 44, modbusPanel.left + 70, modbusPanel.top + 84 }; DrawInputShell(dc, shell, TRUE); }
        { RECT shell = { modbusPanel.left + 72, modbusPanel.top + 44, modbusPanel.left + 130, modbusPanel.top + 84 }; DrawInputShell(dc, shell, TRUE); }
        { RECT shell = { modbusPanel.left + 132, modbusPanel.top + 44, modbusPanel.left + 218, modbusPanel.top + 84 }; DrawInputShell(dc, shell, TRUE); }
        { RECT shell = { modbusPanel.left + 220, modbusPanel.top + 44, modbusPanel.left + 306, modbusPanel.top + 84 }; DrawInputShell(dc, shell, TRUE); }
        { RECT shell = { modbusPanel.left + 308, modbusPanel.top + 44, modbusPanel.left + 388, modbusPanel.top + 84 }; DrawInputShell(dc, shell, TRUE); }
        gBleModbusSendRect.left = modbusPanel.right - 116; gBleModbusSendRect.top = modbusPanel.top + 44; gBleModbusSendRect.right = modbusPanel.right - 14; gBleModbusSendRect.bottom = modbusPanel.top + 84;
        DrawButton(dc, gBleModbusSendRect, L"发送", ThemeAccent(), ThemeAccent(), RGB(255,255,255), gHoverItem == 57);
        {
            RECT shell = { modbusPanel.left + 14, modbusPanel.top + 87, modbusPanel.right - 14, modbusPanel.bottom - 10 };
            RECT resultText = shell;
            DrawInputShell(dc, shell, TRUE);
            resultText.left += 12; resultText.top += 8; resultText.right -= 12; resultText.bottom -= 8;
            static wchar_t modbusDrawText[8192];
            GetWindowTextW(gBleModbusResultEdit, modbusDrawText, 8192);
            DrawTextBlock(dc, modbusDrawText, resultText, gFontTiny, ThemeText(), DT_LEFT | DT_WORDBREAK);
        }


        SetRectCoords(&gBleScanRect, 0, 0, 0, 0);
        SetRectCoords(&gBleConnectRect, 0, 0, 0, 0);
        for (index = 0; index < BLE_VISIBLE_ROWS; index++) { SetRectCoords(&gBleDeviceRows[index], 0, 0, 0, 0); }
        return;
    }

    SetRectCoords(&gBleAcControlRect, 0, 0, 0, 0);
    SetRectCoords(&gBleDcControlRect, 0, 0, 0, 0);
    SetRectCoords(&gBleRefreshRect, 0, 0, 0, 0);
    SetRectCoords(&gBleSlaveApplyRect, 0, 0, 0, 0);
    SetRectCoords(&gBleModbusSendRect, 0, 0, 0, 0);
    SetRectCoords(&gBleOtaEntryRect, 0, 0, 0, 0);
    DrawTextBlock(dc, L"蓝牙连接", title, gFontTitle, ThemeText(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawTextBlock(dc, L"在当前窗口扫描并筛选 BLE 设备，连接成功后直接进入产品界面", subtitle, gFontSmall, ThemeMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawRoundBox(dc, &toolbar, 22, ThemeSurface(), ThemeBorder());
    label.left = toolbar.left + 24; label.top = toolbar.top + 14; label.right = toolbar.left + 520; label.bottom = toolbar.top + 40;
    DrawTextBlock(dc, L"设备名称或 MAC 筛选", label, gFontTiny, ThemeMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    { RECT input = { toolbar.left + 24, toolbar.top + 51, toolbar.left + 520, toolbar.top + 95 }; DrawInputShell(dc, input, TRUE); }
    gBleScanRect.left = toolbar.right - 204; gBleScanRect.top = toolbar.top + 50; gBleScanRect.right = toolbar.right - 24; gBleScanRect.bottom = toolbar.top + 94;
    DrawButton(dc, gBleScanRect, gBleScanning ? L"停止扫描" : L"开始扫描", gBleScanning ? ThemeSurfaceAlt() : ThemeAccent(), gBleScanning ? ThemeBorder() : ThemeAccent(), gBleScanning ? ThemeText() : RGB(255,255,255), gHoverItem == 50);

    DrawRoundBox(dc, &listPanel, 24, ThemeSurface(), ThemeBorder());
    label.left = listPanel.left + 24; label.top = listPanel.top + 14; label.right = listPanel.right - 24; label.bottom = listPanel.top + 46;
    wsprintfW(text, L"附近设备  ·  %d 个匹配", gBleFilteredCount);
    DrawTextBlock(dc, text, label, gFontCardTitle, ThemeText(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    for (index = 0; index < BLE_VISIBLE_ROWS; index++)
    {
        RECT row = { listPanel.left + 20, listPanel.top + 58 + index * rowHeight, listPanel.right - 20, listPanel.top + 108 + index * rowHeight };
        gBleDeviceRows[index] = row;
        if (index < gBleFilteredCount)
        {
            int deviceIndex = gBleFilteredIndices[index];
            BLE_DEVICE_ITEM* item = &gBleDevices[deviceIndex];
            BOOL selected = gBleHasSelectedDevice && item->Address == gBleSelectedAddress;
            RECT nameRect = { row.left + 16, row.top + 5, row.right - 220, row.top + 28 };
            RECT macRect = { row.left + 16, row.top + 27, row.right - 180, row.bottom - 3 };
            RECT rssiRect = { row.right - 150, row.top, row.right - 18, row.bottom };
            DrawRoundBox(dc, &row, 15, selected ? ThemeAccentSoft() : ThemeSurfaceAlt(), selected ? ThemeAccent() : ThemeBorder());
            DrawTextBlock(dc, item->Name, nameRect, gFontSmall, selected ? ThemeAccent() : ThemeText(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            DrawTextBlock(dc, item->Mac, macRect, gFontTiny, ThemeMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            wsprintfW(text, L"%d dBm", item->Rssi);
            DrawTextBlock(dc, text, rssiRect, gFontSmall, item->Rssi >= -65 ? ThemeSuccess() : ThemeAccent(), DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        }
        else
        {
            SetRectCoords(&gBleDeviceRows[index], 0, 0, 0, 0);
        }
    }
    if (gBleFilteredCount == 0)
    {
        label.left = listPanel.left + 24; label.top = listPanel.top + 150; label.right = listPanel.right - 24; label.bottom = listPanel.top + 210;
        DrawTextBlock(dc, gBleScanning ? L"正在等待附近设备广播……" : L"点击“开始扫描”查找附近 BLE 设备", label, gFontBody, ThemeMuted(), DT_CENTER | DT_VCENTER | DT_WORDBREAK);
    }

    DrawRoundBox(dc, &sidePanel, 24, ThemeSurface(), ThemeBorder());
    label.left = sidePanel.left + 24; label.top = sidePanel.top + 16; label.right = sidePanel.right - 24; label.bottom = sidePanel.top + 46;
    DrawTextBlock(dc, L"连接目标", label, gFontCardTitle, ThemeText(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    label.top = sidePanel.top + 62; label.bottom = sidePanel.top + 92;
    if (gBleHasSelectedDevice)
    {
        RECT macRect = { sidePanel.left + 24, sidePanel.top + 98, sidePanel.right - 24, sidePanel.top + 120 };
        RECT onlineRect = { sidePanel.left + 44, sidePanel.top + 126, sidePanel.right - 24, sidePanel.top + 148 };
        RECT statusDot = { sidePanel.left + 24, sidePanel.top + 132, sidePanel.left + 34, sidePanel.top + 142 };
        DrawTextBlock(dc, gBleSelectedName, label, gFontBody, ThemeText(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        DrawTextBlock(dc, gBleSelectedMac, macRect, gFontSmall, ThemeMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        if (gBleSelectedPresent)
        {
            DrawRoundBox(dc, &statusDot, 10, ThemeSuccess(), ThemeSuccess());
            wsprintfW(text, L"设备在线  ·  %d dBm", gBleSelectedRssi);
            DrawTextBlock(dc, text, onlineRect, gFontTiny, ThemeSuccess(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }
        else
        {
            DrawRoundBox(dc, &statusDot, 10, ThemeMuted2(), ThemeMuted2());
            DrawTextBlock(dc, L"当前广播暂未发现 · 已保留所选设备", onlineRect, gFontTiny, ThemeMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }
    }
    else
    {
        DrawTextBlock(dc, L"请从左侧选择设备", label, gFontBody, ThemeMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
    gBleConnectRect.left = sidePanel.left + 24; gBleConnectRect.top = sidePanel.top + 166; gBleConnectRect.right = sidePanel.right - 24; gBleConnectRect.bottom = sidePanel.top + 214;
    DrawButton(dc, gBleConnectRect, gBleConnecting ? L"正在连接……" : L"连接设备", (gBleHasSelectedDevice && gBleSelectedPresent && !gBleConnecting) ? ThemeSuccess() : ThemeSurfaceAlt(), (gBleHasSelectedDevice && gBleSelectedPresent && !gBleConnecting) ? ThemeSuccess() : ThemeBorder(), (gBleHasSelectedDevice && gBleSelectedPresent && !gBleConnecting) ? RGB(255,255,255) : ThemeMuted(), gHoverItem == 51);
    label.left = sidePanel.left + 24; label.top = sidePanel.top + 246; label.right = sidePanel.right - 24; label.bottom = sidePanel.top + 346;
    DrawTextBlock(dc, L"蓝牙链路：Windows WinRT 直接扫描/连接 → FF00/FF02/FF01 → 2A2A鉴权 → 0086 ECDH → AES-CBC加密 Modbus。", label, gFontSmall, ThemeMuted(), DT_LEFT | DT_WORDBREAK);
    label.top = sidePanel.bottom - 100; label.bottom = sidePanel.bottom - 24;
    DrawTextBlock(dc, gBleStatusText, label, gFontSmall, gBleConnecting ? ThemeAccent() : ThemeMuted(), DT_LEFT | DT_WORDBREAK);
}


static void DrawSelectorChevron(HDC dc, const RECT* rect, BOOL open, COLORREF color)
{
    int cx = rect->right - 22;
    int cy = (rect->top + rect->bottom) / 2;
    SelectObject(dc, GetStockObject(DC_PEN));
    SetDCPenColor(dc, color);
    if (open)
    {
        MoveToEx(dc, cx - 5, cy + 2, NULL); LineTo(dc, cx, cy - 3); LineTo(dc, cx + 5, cy + 2);
    }
    else
    {
        MoveToEx(dc, cx - 5, cy - 2, NULL); LineTo(dc, cx, cy + 3); LineTo(dc, cx + 5, cy - 2);
    }
}

static void DrawChipSelectorControl(HDC dc, const RECT* rect, int chip, BOOL open, BOOL hover)
{
    RECT textRect = *rect;
    RECT dotRect;
    COLORREF fill = (open || hover) ? ThemeAccentSoft() : ThemeInputFill();
    COLORREF border = open ? ThemeAccent() : (hover ? ThemeAccent() : ThemeBorder());
    COLORREF textColor = (chip >= 0 && chip < 5) ? ThemeText() : ThemeMuted();
    DrawRoundBox(dc, rect, 14, fill, border);
    if (chip >= 0 && chip < 5)
    {
        dotRect.left = rect->left + 13; dotRect.top = (rect->top + rect->bottom) / 2 - 4;
        dotRect.right = dotRect.left + 8; dotRect.bottom = dotRect.top + 8;
        DrawRoundBox(dc, &dotRect, 8, ThemeAccent(), ThemeAccent());
    }
    textRect.left += (chip >= 0 && chip < 5) ? 30 : 14;
    textRect.right -= 42;
    DrawTextBlock(dc, (chip >= 0 && chip < 5) ? CHIP_NAMES[chip] : L"选择芯片平台", textRect, gFontSmall, textColor, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    DrawLine(dc, rect->right - 38, rect->top + 8, rect->right - 38, rect->bottom - 8, ThemeBorder());
    DrawSelectorChevron(dc, rect, open, open ? ThemeAccent() : ThemeMuted());
}

static void LayoutChipOptionRects(const RECT* anchor, RECT optionRects[5], const RECT* client)
{
    int itemHeight = 36;
    int gap = 5;
    int totalHeight = itemHeight * 5 + gap * 2;
    int top = anchor->bottom + 6;
    int index;
    if (top + totalHeight > client->bottom - 54) { top = anchor->top - 6 - totalHeight; }
    for (index = 0; index < 5; index++)
    {
        optionRects[index].left = anchor->left;
        optionRects[index].right = anchor->right;
        optionRects[index].top = top + gap + index * itemHeight;
        optionRects[index].bottom = optionRects[index].top + itemHeight;
    }
}

static void DrawChipOptionsPopup(HDC dc, const RECT* anchor, int selectedChip, RECT optionRects[5], const RECT* client, int hoverBase)
{
    RECT popup;
    int index;
    LayoutChipOptionRects(anchor, optionRects, client);
    popup.left = anchor->left - 2;
    popup.right = anchor->right + 2;
    popup.top = optionRects[0].top - 5;
    popup.bottom = optionRects[4].bottom + 5;
    DrawRoundBox(dc, &popup, 16, ThemeSurface(), ThemeBorder());
    for (index = 0; index < 5; index++)
    {
        RECT textRect = optionRects[index];
        RECT dotRect;
        BOOL selected = index == selectedChip;
        BOOL hovered = gHoverItem == hoverBase + index;
        if (selected || hovered)
        {
            RECT rowFill = optionRects[index];
            rowFill.left += 4; rowFill.right -= 4; rowFill.top += 2; rowFill.bottom -= 2;
            DrawRoundBox(dc, &rowFill, 11, selected ? ThemeAccentSoft() : ThemeSurfaceAlt(), selected ? ThemeAccentSoft() : ThemeSurfaceAlt());
        }
        dotRect.left = optionRects[index].left + 13; dotRect.top = (optionRects[index].top + optionRects[index].bottom) / 2 - 4;
        dotRect.right = dotRect.left + 8; dotRect.bottom = dotRect.top + 8;
        if (selected) { DrawRoundBox(dc, &dotRect, 8, ThemeAccent(), ThemeAccent()); }
        else { DrawRoundBox(dc, &dotRect, 8, ThemeSurface(), ThemeBorder()); }
        textRect.left += 31;
        textRect.right -= 12;
        DrawTextBlock(dc, CHIP_NAMES[index], textRect, gFontSmall, selected ? ThemeAccent() : ThemeText(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
}



/* ---------------- V1.3.9 unified native selector / stable text viewport ---------------- */
static HWND UiSelectorBackingCombo(int kind)
{
    if (kind == UI_SELECTOR_COM) { return gComCombo; }
    if (kind == UI_SELECTOR_BAUD) { return gBaudCombo; }
    if (kind == UI_SELECTOR_CAN_DEVICE) { return gCanDeviceCombo; }
    if (kind == UI_SELECTOR_CAN_CHANNEL) { return gCanChannelCombo; }
    if (kind == UI_SELECTOR_CAN_BAUD) { return gCanBaudCombo; }
    if (kind == UI_SELECTOR_BLE_INTERVAL) { return gBleIntervalCombo; }
    return NULL;
}

static RECT* UiSelectorRect(int kind)
{
    if (kind == UI_SELECTOR_COM) { return &gComSelectorRect; }
    if (kind == UI_SELECTOR_BAUD) { return &gBaudSelectorRect; }
    if (kind == UI_SELECTOR_CAN_DEVICE) { return &gCanDeviceSelectorRect; }
    if (kind == UI_SELECTOR_CAN_CHANNEL) { return &gCanChannelSelectorRect; }
    if (kind == UI_SELECTOR_CAN_BAUD) { return &gCanBaudSelectorRect; }
    if (kind == UI_SELECTOR_BLE_INTERVAL) { return &gBleIntervalSelectorRect; }
    return NULL;
}

static BOOL UiSelectorEnabled(int kind)
{
    if (kind == UI_SELECTOR_COM || kind == UI_SELECTOR_BAUD) { return !gUpgradeRunning; }
    if (kind == UI_SELECTOR_CAN_DEVICE || kind == UI_SELECTOR_CAN_CHANNEL || kind == UI_SELECTOR_CAN_BAUD) { return !gUpgradeRunning; }
    if (kind == UI_SELECTOR_BLE_INTERVAL) { return gBleConnected && !gBleOtaRunning; }
    return FALSE;
}

static int UiSelectorHitCode(int kind)
{
    return 179 + kind; /* 180..185 */
}

static void UiSelectorGetText(int kind, int itemIndex, LPWSTR output, int maxCount)
{
    HWND combo = UiSelectorBackingCombo(kind);
    int index = itemIndex;
    output[0] = 0;
    if (combo == NULL) { WCopy(output, L"—", maxCount); return; }
    if (index < 0) { index = (int)SendMessageW(combo, CB_GETCURSEL, 0, 0); }
    if (index >= 0) { SendMessageW(combo, CB_GETLBTEXT, (WPARAM)index, (LPARAM)output); }
    if (output[0] == 0) { WCopy(output, L"请选择", maxCount); }
}

static void DrawUiSelectorControl(HDC dc, const RECT* rect, int kind, BOOL open, BOOL hover)
{
    wchar_t value[128];
    RECT textRect = *rect;
    RECT dotRect;
    BOOL enabled = UiSelectorEnabled(kind);
    COLORREF fill = (open || hover) && enabled ? ThemeAccentSoft() : ThemeInputFill();
    COLORREF border = (open || hover) && enabled ? ThemeAccent() : ThemeInputBorder();
    COLORREF textColor = enabled ? ThemeText() : ThemeMuted2();
    UiSelectorGetText(kind, -1, value, 128);
    DrawRoundBox(dc, rect, 14, fill, border);
    dotRect.left = rect->left + 13; dotRect.top = (rect->top + rect->bottom) / 2 - 4;
    dotRect.right = dotRect.left + 8; dotRect.bottom = dotRect.top + 8;
    DrawRoundBox(dc, &dotRect, 8, enabled ? ThemeAccent() : ThemeMuted2(), enabled ? ThemeAccent() : ThemeMuted2());
    textRect.left += 30;
    textRect.right -= 42;
    DrawTextBlock(dc, value, textRect, gFontSmall, textColor, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    DrawLine(dc, rect->right - 38, rect->top + 8, rect->right - 38, rect->bottom - 8, ThemeBorder());
    DrawSelectorChevron(dc, rect, open, open ? ThemeAccent() : ThemeMuted());
}

static void OpenUiSelector(int kind)
{
    HWND combo = UiSelectorBackingCombo(kind);
    int count;
    int selected;
    if (combo == NULL || !UiSelectorEnabled(kind)) { return; }
    count = (int)SendMessageW(combo, CB_GETCOUNT, 0, 0);
    selected = (int)SendMessageW(combo, CB_GETCURSEL, 0, 0);
    gUiSelectorOpen = kind;
    gUiSelectorFirstOption = 0;
    if (count > UI_SELECTOR_MAX_VISIBLE && selected >= 0)
    {
        gUiSelectorFirstOption = selected - UI_SELECTOR_MAX_VISIBLE / 2;
        if (gUiSelectorFirstOption < 0) { gUiSelectorFirstOption = 0; }
        if (gUiSelectorFirstOption + UI_SELECTOR_MAX_VISIBLE > count) { gUiSelectorFirstOption = count - UI_SELECTOR_MAX_VISIBLE; }
    }
}

static void LayoutUiSelectorPopup(const RECT* client)
{
    HWND combo = UiSelectorBackingCombo(gUiSelectorOpen);
    RECT* anchor = UiSelectorRect(gUiSelectorOpen);
    int count;
    int visible;
    int itemHeight = 34;
    int navHeight = 24;
    int top;
    int totalHeight;
    int index;
    BOOL hasUp;
    BOOL hasDown;
    if (combo == NULL || anchor == NULL) { gUiSelectorVisibleCount = 0; return; }
    count = (int)SendMessageW(combo, CB_GETCOUNT, 0, 0);
    if (count < 0) { count = 0; }
    if (gUiSelectorFirstOption < 0) { gUiSelectorFirstOption = 0; }
    if (gUiSelectorFirstOption >= count && count > 0) { gUiSelectorFirstOption = count - 1; }
    visible = count - gUiSelectorFirstOption;
    if (visible > UI_SELECTOR_MAX_VISIBLE) { visible = UI_SELECTOR_MAX_VISIBLE; }
    gUiSelectorVisibleCount = visible;
    hasUp = gUiSelectorFirstOption > 0;
    hasDown = gUiSelectorFirstOption + visible < count;
    totalHeight = visible * itemHeight + 10 + (hasUp ? navHeight : 0) + (hasDown ? navHeight : 0);
    top = anchor->bottom + 6;
    if (top + totalHeight > client->bottom - 50) { top = anchor->top - 6 - totalHeight; }
    gUiSelectorPopupUpRect.left = anchor->left; gUiSelectorPopupUpRect.right = anchor->right;
    gUiSelectorPopupUpRect.top = top + 5; gUiSelectorPopupUpRect.bottom = gUiSelectorPopupUpRect.top + (hasUp ? navHeight : 0);
    top += 5 + (hasUp ? navHeight : 0);
    for (index = 0; index < UI_SELECTOR_MAX_VISIBLE; index++)
    {
        if (index < visible)
        {
            gUiSelectorOptionRects[index].left = anchor->left;
            gUiSelectorOptionRects[index].right = anchor->right;
            gUiSelectorOptionRects[index].top = top + index * itemHeight;
            gUiSelectorOptionRects[index].bottom = gUiSelectorOptionRects[index].top + itemHeight;
        }
        else { SetRectCoords(&gUiSelectorOptionRects[index], 0, 0, 0, 0); }
    }
    top += visible * itemHeight;
    gUiSelectorPopupDownRect.left = anchor->left; gUiSelectorPopupDownRect.right = anchor->right;
    gUiSelectorPopupDownRect.top = top; gUiSelectorPopupDownRect.bottom = top + (hasDown ? navHeight : 0);
}

static void DrawUiSelectorPopup(HDC dc, const RECT* client)
{
    HWND combo;
    RECT* anchor;
    RECT popup;
    int count;
    int selected;
    int index;
    BOOL hasUp;
    BOOL hasDown;
    wchar_t value[128];
    if (gUiSelectorOpen <= 0) { return; }
    combo = UiSelectorBackingCombo(gUiSelectorOpen);
    anchor = UiSelectorRect(gUiSelectorOpen);
    if (combo == NULL || anchor == NULL) { gUiSelectorOpen = 0; return; }
    LayoutUiSelectorPopup(client);
    count = (int)SendMessageW(combo, CB_GETCOUNT, 0, 0);
    selected = (int)SendMessageW(combo, CB_GETCURSEL, 0, 0);
    hasUp = gUiSelectorFirstOption > 0;
    hasDown = gUiSelectorFirstOption + gUiSelectorVisibleCount < count;
    popup.left = anchor->left - 2;
    popup.right = anchor->right + 2;
    if (hasUp) { popup.top = gUiSelectorPopupUpRect.top - 5; }
    else if (gUiSelectorVisibleCount > 0) { popup.top = gUiSelectorOptionRects[0].top - 5; }
    else { popup.top = anchor->bottom + 6; }
    if (hasDown) { popup.bottom = gUiSelectorPopupDownRect.bottom + 5; }
    else if (gUiSelectorVisibleCount > 0) { popup.bottom = gUiSelectorOptionRects[gUiSelectorVisibleCount - 1].bottom + 5; }
    else { popup.bottom = popup.top + 42; }
    DrawRoundBox(dc, &popup, 16, ThemeSurface(), ThemeBorder());
    if (hasUp)
    {
        DrawTextBlock(dc, L"⌃  更多", gUiSelectorPopupUpRect, gFontTiny, gHoverItem == 208 ? ThemeAccent() : ThemeMuted(), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    for (index = 0; index < gUiSelectorVisibleCount; index++)
    {
        int actualIndex = gUiSelectorFirstOption + index;
        RECT row = gUiSelectorOptionRects[index];
        RECT textRect = row;
        RECT dotRect;
        BOOL isSelected = actualIndex == selected;
        BOOL hovered = gHoverItem == 190 + index;
        if (isSelected || hovered) { DrawRoundBox(dc, &row, 10, isSelected ? ThemeAccentSoft() : ThemeSurfaceAlt(), isSelected ? ThemeAccentSoft() : ThemeBorder()); }
        dotRect.left = row.left + 12; dotRect.top = (row.top + row.bottom) / 2 - 3;
        dotRect.right = dotRect.left + 6; dotRect.bottom = dotRect.top + 6;
        DrawRoundBox(dc, &dotRect, 6, isSelected ? ThemeAccent() : ThemeMuted2(), isSelected ? ThemeAccent() : ThemeMuted2());
        UiSelectorGetText(gUiSelectorOpen, actualIndex, value, 128);
        textRect.left += 28; textRect.right -= 10;
        DrawTextBlock(dc, value, textRect, gFontSmall, isSelected ? ThemeAccent() : ThemeText(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
    if (hasDown)
    {
        DrawTextBlock(dc, L"⌄  更多", gUiSelectorPopupDownRect, gFontTiny, gHoverItem == 209 ? ThemeAccent() : ThemeMuted(), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

static void ApplyUiSelectorSelection(int visibleRow)
{
    HWND combo = UiSelectorBackingCombo(gUiSelectorOpen);
    int actualIndex = gUiSelectorFirstOption + visibleRow;
    int count;
    int kind = gUiSelectorOpen;
    if (combo == NULL) { return; }
    count = (int)SendMessageW(combo, CB_GETCOUNT, 0, 0);
    if (actualIndex < 0 || actualIndex >= count) { return; }
    SendMessageW(combo, CB_SETCURSEL, (WPARAM)actualIndex, 0);
    if (kind == UI_SELECTOR_BLE_INTERVAL)
    {
        wchar_t command[64];
        gBlePollInterval = actualIndex + 1;
        wsprintfW(command, L"INTERVAL\t%d", gBlePollInterval);
        SendBleCommand(command);
        wsprintfW(gBleStatusText, L"Modbus轮询周期已设置为 %d 秒", gBlePollInterval);
    }
}

static int BleVersionLineCount(void)
{
    int index = 0;
    int count = 0;
    BOOL hasText = FALSE;
    if (gBleVersions[0] == 0 || (gBleVersions[0] == L'-' && gBleVersions[1] == L'-')) { return 0; }
    while (gBleVersions[index] != 0)
    {
        if (gBleVersions[index] == L'|') { if (hasText) { count++; hasText = FALSE; } }
        else if (gBleVersions[index] != L' ' && gBleVersions[index] != L'\t') { hasText = TRUE; }
        index++;
    }
    if (hasText) { count++; }
    return count;
}

static BOOL BleVersionGetLine(int wanted, LPWSTR output, int maxCount)
{
    int source = 0;
    int line = 0;
    int out = 0;
    BOOL started = FALSE;
    output[0] = 0;
    while (gBleVersions[source] != 0)
    {
        wchar_t ch = gBleVersions[source++];
        if (ch == L'|')
        {
            if (started)
            {
                while (out > 0 && output[out - 1] == L' ') { out--; }
                output[out] = 0;
                if (line == wanted) { return TRUE; }
                line++; out = 0; started = FALSE;
            }
            continue;
        }
        if (!started && (ch == L' ' || ch == L'\t')) { continue; }
        started = TRUE;
        if (line == wanted && out < maxCount - 1) { output[out++] = ch; }
    }
    if (started && line == wanted)
    {
        while (out > 0 && output[out - 1] == L' ') { out--; }
        output[out] = 0;
        return TRUE;
    }
    return FALSE;
}

static void DrawBleVersionViewport(HDC dc, const RECT* rect)
{
    RECT inner = *rect;
    RECT row;
    RECT track;
    RECT thumb;
    wchar_t line[160];
    int lineCount = BleVersionLineCount();
    int rowHeight = 20;
    int visibleRows;
    int index;
    int maxScroll;
    DrawRoundBox(dc, rect, 12, ThemeInputFill(), ThemeInputBorder());
    inner.left += 12; inner.top += 8; inner.right -= 26; inner.bottom -= 8;
    visibleRows = (inner.bottom - inner.top) / rowHeight;
    if (visibleRows < 1) { visibleRows = 1; }
    maxScroll = lineCount - visibleRows;
    if (maxScroll < 0) { maxScroll = 0; }
    if (gBleVersionScroll > maxScroll) { gBleVersionScroll = maxScroll; }
    if (gBleVersionScroll < 0) { gBleVersionScroll = 0; }
    if (lineCount <= 0)
    {
        DrawTextBlock(dc, L"等待设备版本信息……", inner, gFontTiny, ThemeMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
    else
    {
        for (index = 0; index < visibleRows && gBleVersionScroll + index < lineCount; index++)
        {
            if (BleVersionGetLine(gBleVersionScroll + index, line, 160))
            {
                row.left = inner.left; row.right = inner.right;
                row.top = inner.top + index * rowHeight; row.bottom = row.top + rowHeight;
                DrawTextBlock(dc, line, row, gFontTiny, ThemeText(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            }
        }
    }
    /* Slim custom scrollbar + arrow affordances: no native white scrollbar and no child repaint flicker. */
    gBleVersionUpRect.left = rect->right - 22; gBleVersionUpRect.right = rect->right - 6;
    gBleVersionUpRect.top = rect->top + 5; gBleVersionUpRect.bottom = gBleVersionUpRect.top + 18;
    gBleVersionDownRect.left = gBleVersionUpRect.left; gBleVersionDownRect.right = gBleVersionUpRect.right;
    gBleVersionDownRect.bottom = rect->bottom - 5; gBleVersionDownRect.top = gBleVersionDownRect.bottom - 18;
    DrawTextBlock(dc, L"⌃", gBleVersionUpRect, gFontTiny, gBleVersionScroll > 0 ? ThemeAccent() : ThemeMuted2(), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    DrawTextBlock(dc, L"⌄", gBleVersionDownRect, gFontTiny, gBleVersionScroll < maxScroll ? ThemeAccent() : ThemeMuted2(), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    track.left = rect->right - 12; track.right = rect->right - 9; track.top = gBleVersionUpRect.bottom + 3; track.bottom = gBleVersionDownRect.top - 3;
    if (track.bottom > track.top)
    {
        DrawRoundBox(dc, &track, 3, ThemeBorder(), ThemeBorder());
        thumb = track;
        if (lineCount > visibleRows)
        {
            int trackHeight = track.bottom - track.top;
            int thumbHeight = trackHeight * visibleRows / lineCount;
            int travel;
            if (thumbHeight < 14) { thumbHeight = 14; }
            if (thumbHeight > trackHeight) { thumbHeight = trackHeight; }
            travel = trackHeight - thumbHeight;
            thumb.top = track.top + (maxScroll > 0 ? travel * gBleVersionScroll / maxScroll : 0);
            thumb.bottom = thumb.top + thumbHeight;
            DrawRoundBox(dc, &thumb, 3, ThemeAccent(), ThemeAccent());
        }
    }
}

static void SetBleOtaControlsVisible(BOOL visible)
{
    int command = (visible && !gChipDialogOpen) ? SW_SHOW : SW_HIDE;
    int index;
    ShowWindow(gBleOtaGapEdit, command);
    ShowWindow(gBleOtaTimeoutEdit, command);
    ShowWindow(gBleOtaChannelCombo, SW_HIDE); /* AES自动加密已实机验证，固定使用，不再提供通道选择。 */
    ShowWindow(gBleOtaChipCombo, SW_HIDE);    /* V1.3.6 芯片选择移动到每个固件队列行。 */
    for (index = 0; index < BLE_OTA_VISIBLE_ROWS; index++)
    {
        if (gBleOtaRowChipCombos[index]) { ShowWindow(gBleOtaRowChipCombos[index], SW_HIDE); }
    }
    if (!visible) { gBleOtaChipSelectorOpen = FALSE; gBleOtaChipSelectorRow = -1; }
}

static void LayoutBleOtaControls(const RECT* client)
{
    int left = 54;
    int right = client->right - 54;
    int settingsTop = 150;
    int actionTop = client->bottom - 210;
    int queueBottom;
    int rowHeight;
    int index;
    if (actionTop < 650) { actionTop = 650; }
    queueBottom = actionTop - 14;
    rowHeight = (queueBottom - (282 + 48) - 10) / BLE_OTA_VISIBLE_ROWS;
    if (rowHeight < 40) { rowHeight = 40; }
    if (rowHeight > 51) { rowHeight = 51; }
    MoveWindow(gBleOtaGapEdit, left + 840, settingsTop + 55, 74, 34, TRUE);
    MoveWindow(gBleOtaTimeoutEdit, left + 965, settingsTop + 55, 74, 34, TRUE);
    ShowWindow(gBleOtaChipCombo, SW_HIDE);
    for (index = 0; index < BLE_OTA_VISIBLE_ROWS; index++)
    {
        if (gBleOtaRowChipCombos[index] != NULL) { ShowWindow(gBleOtaRowChipCombos[index], SW_HIDE); }
        SetRectCoords(&gBleOtaChipSelectorRects[index], 0, 0, 0, 0);
    }
}

static LPCWSTR BleOtaStateText(int state)
{
    if (state == 1) { return L"升级中"; }
    if (state == 2) { return L"成功"; }
    if (state == 3) { return L"失败"; }
    if (state == 4) { return L"等待重启"; }
    return L"等待";
}

static COLORREF BleOtaStateColor(int state)
{
    if (state == 1) { return ThemeAccent(); }
    if (state == 2) { return ThemeSuccess(); }
    if (state == 3) { return ThemeDanger(); }
    return ThemeMuted();
}

static void DrawBleOtaPage(HDC dc, const RECT* client)
{
    int left = 54;
    int right = client->right - 54;
    int bottom = client->bottom - 60;
    int actionTop = client->bottom - 210;
    int queueBottom;
    RECT title = { 194, 78, right, 118 };
    RECT subtitle = { 194, 114, right, 144 };
    RECT settings = { left, 150, right, 264 };
    RECT queuePanel;
    RECT actionPanel;
    RECT label;
    RECT progressBar;
    RECT fill;
    wchar_t text[512];
    int index;
    int rowHeight;
    int currentPercent;
    if (actionTop < 650) { actionTop = 650; }
    queueBottom = actionTop - 14;
    rowHeight = (queueBottom - (282 + 48) - 10) / BLE_OTA_VISIBLE_ROWS;
    if (rowHeight < 40) { rowHeight = 40; }
    if (rowHeight > 51) { rowHeight = 51; }
    queuePanel.left = left; queuePanel.top = 282; queuePanel.right = right; queuePanel.bottom = queueBottom;
    actionPanel.left = left; actionPanel.top = actionTop; actionPanel.right = right; actionPanel.bottom = bottom;

    gBackRect.left = left; gBackRect.top = 82; gBackRect.right = left + 118; gBackRect.bottom = 122;
    DrawPill(dc, gBackRect, L"←  返回设备", ThemeSurface(), ThemeBorder(), ThemeText(), gHoverItem == 10);
    DrawTextBlock(dc, L"OTA 一键升级", title, gFontTitle, ThemeText(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawTextBlock(dc, L"两阶段升级 · PC→IOT占前50% · IOT分发占后50% · AES自动加密 · 自动重连", subtitle, gFontSmall, ThemeSuccess(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    DrawRoundBox(dc, &settings, 22, ThemeSurface(), ThemeBorder());
    label.left = settings.left + 24; label.top = settings.top + 14; label.right = settings.left + 330; label.bottom = settings.top + 39;
    DrawTextBlock(dc, L"升级目标", label, gFontTiny, ThemeMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    label.top = settings.top + 39; label.bottom = settings.top + 66;
    DrawTextBlock(dc, gBleConnectedName[0] ? gBleConnectedName : gBleSelectedName, label, gFontBody, ThemeText(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    label.top = settings.top + 66; label.bottom = settings.top + 92;
    DrawTextBlock(dc, gBleConnectedMac[0] ? gBleConnectedMac : gBleSelectedMac, label, gFontTiny, ThemeMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    label.left = settings.left + 610; label.top = settings.top + 14; label.right = settings.left + 800; label.bottom = settings.top + 39;
    DrawTextBlock(dc, L"BLE OTA 通道", label, gFontTiny, ThemeMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    {
        RECT encBadge = { settings.left + 606, settings.top + 49, settings.left + 806, settings.top + 93 };
        DrawRoundBox(dc, &encBadge, 14, ThemeAccentSoft(), ThemeAccent());
        label.left = encBadge.left + 14; label.top = encBadge.top; label.right = encBadge.right - 10; label.bottom = encBadge.bottom;
        DrawTextBlock(dc, L"●  AES 自动加密 · 固定", label, gFontSmall, ThemeAccent(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
    label.left = settings.left + 840; label.right = settings.left + 922; label.top = settings.top + 14; label.bottom = settings.top + 39;
    DrawTextBlock(dc, L"重启间隔", label, gFontTiny, ThemeMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    { RECT shell = { settings.left + 836, settings.top + 48, settings.left + 920, settings.top + 94 }; DrawInputShell(dc, shell, TRUE); }
    label.left = settings.left + 925; label.right = settings.left + 952; label.top = settings.top + 54; label.bottom = settings.top + 88;
    DrawTextBlock(dc, L"秒", label, gFontTiny, ThemeMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    label.left = settings.left + 965; label.right = settings.left + 1064; label.top = settings.top + 14; label.bottom = settings.top + 39;
    DrawTextBlock(dc, L"失败/重连超时", label, gFontTiny, ThemeMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    { RECT shell = { settings.left + 961, settings.top + 48, settings.left + 1047, settings.top + 94 }; DrawInputShell(dc, shell, TRUE); }
    label.left = settings.left + 1052; label.right = settings.left + 1078; label.top = settings.top + 54; label.bottom = settings.top + 88;
    DrawTextBlock(dc, L"秒", label, gFontTiny, ThemeMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    gBleOtaAddRect.left = settings.right - 270; gBleOtaAddRect.top = settings.top + 48; gBleOtaAddRect.right = settings.right - 150; gBleOtaAddRect.bottom = settings.top + 94;
    gBleOtaClearRect.left = settings.right - 138; gBleOtaClearRect.top = settings.top + 48; gBleOtaClearRect.right = settings.right - 24; gBleOtaClearRect.bottom = settings.top + 94;
    DrawButton(dc, gBleOtaAddRect, L"＋ 添加固件", ThemeAccent(), ThemeAccent(), RGB(255,255,255), gHoverItem == 70);
    DrawButton(dc, gBleOtaClearRect, L"清空队列", ThemeSurfaceAlt(), ThemeBorder(), ThemeText(), gHoverItem == 71);

    DrawRoundBox(dc, &queuePanel, 22, ThemeSurface(), ThemeBorder());
    label.left = queuePanel.left + 22; label.top = queuePanel.top + 10; label.right = queuePanel.right - 22; label.bottom = queuePanel.top + 42;
    wsprintfW(text, L"固件队列  ·  %d 个文件", gBleOtaCount);
    DrawTextBlock(dc, text, label, gFontCardTitle, ThemeText(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    label.left = queuePanel.right - 690; label.right = queuePanel.right - 365;
    DrawTextBlock(dc, L"每个固件：选择芯片 → 读取验证 → 才允许升级", label, gFontTiny, ThemeMuted(), DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    gBleOtaRemoveRect.left = queuePanel.right - 350; gBleOtaRemoveRect.top = queuePanel.top + 9; gBleOtaRemoveRect.right = queuePanel.right - 222; gBleOtaRemoveRect.bottom = queuePanel.top + 41;
    DrawButton(dc, gBleOtaRemoveRect, L"移除所选", ThemeSurfaceAlt(), ThemeBorder(), ThemeText(), gHoverItem == 73);
    gBleOtaUpRect.left = queuePanel.right - 98; gBleOtaUpRect.top = queuePanel.top + 11; gBleOtaUpRect.right = queuePanel.right - 62; gBleOtaUpRect.bottom = queuePanel.top + 39;
    gBleOtaDownRect.left = queuePanel.right - 56; gBleOtaDownRect.top = queuePanel.top + 11; gBleOtaDownRect.right = queuePanel.right - 20; gBleOtaDownRect.bottom = queuePanel.top + 39;
    DrawButton(dc, gBleOtaUpRect, L"↑", ThemeSurfaceAlt(), ThemeBorder(), ThemeText(), gHoverItem == 76);
    DrawButton(dc, gBleOtaDownRect, L"↓", ThemeSurfaceAlt(), ThemeBorder(), ThemeText(), gHoverItem == 77);
    SetRectCoords(&gBleOtaApplyChipRect, 0, 0, 0, 0);

    for (index = 0; index < BLE_OTA_VISIBLE_ROWS; index++)
    {
        RECT row = { queuePanel.left + 18, queuePanel.top + 48 + index * rowHeight, queuePanel.right - 18, queuePanel.top + 48 + index * rowHeight + rowHeight - 5 };
        gBleOtaRows[index] = row;
        if (gBleOtaListOffset + index < gBleOtaCount)
        {
            int itemIndex = gBleOtaListOffset + index;
            BLE_OTA_ITEM* item = &gBleOtaItems[itemIndex];
            BOOL selected = itemIndex == gBleOtaSelected;
            RECT numRect = { row.left + 12, row.top, row.left + 50, row.bottom };
            RECT nameRect = { row.left + 52, row.top + 3, row.right - 505, row.top + 23 };
            RECT metaRect = { row.left + 52, row.top + 22, row.right - 505, row.bottom - 2 };
            RECT stateRect = { row.right - 150, row.top + 2, row.right - 12, row.bottom - 2 };
            gBleOtaVerifyRects[index].left = row.right - 270; gBleOtaVerifyRects[index].top = row.top + 6; gBleOtaVerifyRects[index].right = row.right - 160; gBleOtaVerifyRects[index].bottom = row.bottom - 6;
            DrawRoundBox(dc, &row, 14, selected ? ThemeAccentSoft() : ThemeSurfaceAlt(), selected ? ThemeAccent() : ThemeBorder());
            wsprintfW(text, L"%02d", itemIndex + 1);
            DrawTextBlock(dc, text, numRect, gFontSubtitle, selected ? ThemeAccent() : ThemeMuted2(), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            DrawTextBlock(dc, item->Name, nameRect, gFontSmall, ThemeText(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            if (item->Ready)
            {
                wsprintfW(text, L"%s  ·  %s  ·  V%u  ·  %u KB", CHIP_NAMES[item->Chip], item->DevModel, (UINT)item->Version, (UINT)((item->FileSize + 1023ULL) / 1024ULL));
            }
            else if (item->Chip >= 0 && item->Chip < 5) { wsprintfW(text, L"%s  ·  %s", CHIP_NAMES[item->Chip], item->Message); }
            else { WCopy(text, L"请选择芯片平台，然后点击“读取验证”", 512); }
            DrawTextBlock(dc, text, metaRect, gFontTiny, ThemeMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            gBleOtaChipSelectorRects[index].left = row.right - 485; gBleOtaChipSelectorRects[index].top = row.top + 5;
            gBleOtaChipSelectorRects[index].right = row.right - 283; gBleOtaChipSelectorRects[index].bottom = row.bottom - 5;
            DrawChipSelectorControl(dc, &gBleOtaChipSelectorRects[index], item->Chip, gBleOtaChipSelectorOpen && gBleOtaChipSelectorRow == index, gHoverItem == 150 + index);
            DrawButton(dc, gBleOtaVerifyRects[index], item->Ready ? L"重新验证" : L"读取验证", item->Ready ? ThemeSurfaceAlt() : ThemeSurface(), item->Ready ? ThemeBorder() : ThemeAccent(), item->Ready ? ThemeMuted() : ThemeAccent(), gHoverItem == 90 + index);
            if (!item->Ready)
            {
                DrawTextBlock(dc, item->Chip >= 0 ? L"待验证" : L"选择芯片", stateRect, gFontTiny, item->Chip >= 0 ? ThemeAccent() : ThemeMuted(), DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
            }
            else if (item->State == 0)
            {
                DrawTextBlock(dc, L"已验证", stateRect, gFontTiny, ThemeSuccess(), DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
            }
            else
            {
                if (item->State == 1) { wsprintfW(text, L"%s · %d%%", BleOtaStateText(item->State), item->Progress); }
                else { wsprintfW(text, L"%s", BleOtaStateText(item->State)); }
                DrawTextBlock(dc, text, stateRect, gFontTiny, BleOtaStateColor(item->State), DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
            }
        }
        else
        {
            SetRectCoords(&gBleOtaRows[index], 0, 0, 0, 0);
            SetRectCoords(&gBleOtaVerifyRects[index], 0, 0, 0, 0);
            SetRectCoords(&gBleOtaChipSelectorRects[index], 0, 0, 0, 0);
        }
    }
    if (gBleOtaCount == 0)
    {
        label.left = queuePanel.left + 24; label.top = queuePanel.top + 130; label.right = queuePanel.right - 24; label.bottom = queuePanel.top + 210;
        DrawTextBlock(dc, L"点击“添加固件”选择文件\n固件加入队列后，在对应行选择芯片平台并点击“读取验证”", label, gFontBody, ThemeMuted(), DT_CENTER | DT_VCENTER | DT_WORDBREAK);
    }

    DrawRoundBox(dc, &actionPanel, 22, ThemeSurface(), ThemeBorder());
    label.left = actionPanel.left + 22; label.top = actionPanel.top + 12; label.right = actionPanel.left + 190; label.bottom = actionPanel.top + 36;
    DrawTextBlock(dc, L"当前固件总进度", label, gFontTiny, ThemeMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    if (gBleOtaCount > 0)
    {
        LPCWSTR stageText;
        if (gBleOtaWaitRemaining > 0) { stageText = L"等待设备重启/重连"; }
        else if (gBleOtaPcPercent < 100) { stageText = L"PC → IOT"; }
        else if (gBleOtaDistributionDepth == 2) { stageText = L"设备 → 子设备"; }
        else if (gBleOtaDevicePercent < 100) { stageText = L"IOT → MCU"; }
        else { stageText = L"完成"; }
        label.left = actionPanel.left + 190; label.top = actionPanel.top + 12; label.right = actionPanel.right - 235; label.bottom = actionPanel.top + 36;
        wsprintfW(text, L"文件 %d/%d  ·  当前阶段：%s  ·  PC→IOT %d%%  ·  IOT→MCU %d%%  ·  成功 %d/%d", gBleOtaCurrentIndex + 1, gBleOtaCount, stageText, gBleOtaPcPercent, gBleOtaDevicePercent, gBleOtaSuccessCount, gBleOtaCount);
        DrawTextBlock(dc, text, label, gFontTiny, ThemeMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
    gBleOtaStartRect.left = actionPanel.right - 206; gBleOtaStartRect.top = actionPanel.top + 18; gBleOtaStartRect.right = actionPanel.right - 24; gBleOtaStartRect.bottom = actionPanel.top + 64;
    gBleOtaStopRect = gBleOtaStartRect;
    if (gBleOtaRunning) { DrawButton(dc, gBleOtaStopRect, L"停止 OTA", ThemeSurface(), ThemeDanger(), ThemeDanger(), gHoverItem == 75); }
    else { DrawButton(dc, gBleOtaStartRect, L"开始一键升级", ThemeAccent(), ThemeAccent(), RGB(255,255,255), gHoverItem == 74); }

    currentPercent = gBleOtaCurrentPercent;
    if (currentPercent < 0) { currentPercent = 0; }
    if (currentPercent > 100) { currentPercent = 100; }
    progressBar.left = actionPanel.left + 22; progressBar.top = actionPanel.top + 54; progressBar.right = actionPanel.right - 270; progressBar.bottom = progressBar.top + 16;
    DrawRoundBox(dc, &progressBar, 8, gDarkMode ? RGB(37,52,72) : RGB(226,234,244), gDarkMode ? RGB(37,52,72) : RGB(226,234,244));
    if (currentPercent > 0)
    {
        int width = progressBar.right - progressBar.left;
        int half = width / 2;
        fill = progressBar;
        if (currentPercent <= 50)
        {
            fill.right = fill.left + half * currentPercent / 50;
            if (fill.right > fill.left) { DrawRoundBox(dc, &fill, 8, ThemeAccent(), ThemeAccent()); }
        }
        else
        {
            fill.right = fill.left + half;
            DrawRoundBox(dc, &fill, 8, ThemeAccent(), ThemeAccent());
            fill.left = progressBar.left + half;
            fill.right = fill.left + (width - half) * (currentPercent - 50) / 50;
            if (fill.right > fill.left) { DrawRoundBox(dc, &fill, 8, ThemeSuccess(), ThemeSuccess()); }
        }
    }
    label.left = progressBar.right + 12; label.top = actionPanel.top + 44; label.right = actionPanel.right - 218; label.bottom = actionPanel.top + 80;
    wsprintfW(text, L"%d%%", currentPercent);
    DrawTextBlock(dc, text, label, gFontSubtitle, currentPercent >= 50 ? ThemeSuccess() : ThemeAccent(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    label.left = progressBar.left; label.top = actionPanel.top + 75; label.right = progressBar.right; label.bottom = actionPanel.top + 96;
    DrawTextBlock(dc, L"0%      PC → IOT      50%      IOT → MCU      100%", label, gFontTiny, ThemeMuted2(), DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    label.left = actionPanel.left + 22; label.top = actionPanel.top + 101; label.right = actionPanel.right - 24; label.bottom = actionPanel.bottom - 10;
    if (gBleOtaWaitRemaining > 0) { wsprintfW(text, L"%s  ·  等待设备重启/重连：%d 秒", gBleOtaMessage, gBleOtaWaitRemaining); }
    else { WCopy(text, gBleOtaMessage, 512); }
    DrawTextBlock(dc, text, label, gFontTiny, ThemeMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    if (gBleOtaChipSelectorOpen && gBleOtaChipSelectorRow >= 0 && gBleOtaChipSelectorRow < BLE_OTA_VISIBLE_ROWS)
    {
        int itemIndex = gBleOtaListOffset + gBleOtaChipSelectorRow;
        if (itemIndex >= 0 && itemIndex < gBleOtaCount)
        {
            DrawChipOptionsPopup(dc, &gBleOtaChipSelectorRects[gBleOtaChipSelectorRow], gBleOtaItems[itemIndex].Chip, gBleOtaChipOptionRects, client, 160);
        }
    }
}


static void SetSerialControlsVisible(BOOL visible)
{
    int command = (visible && !gChipDialogOpen) ? SW_SHOW : SW_HIDE;
    /* V1.3.9: hidden ComboBoxes keep the existing selections/business API; UI is parent-drawn. */
    ShowWindow(gComCombo, SW_HIDE);
    ShowWindow(gBaudCombo, SW_HIDE);
    ShowWindow(gRepeatEdit, command);
    ShowWindow(gWaitEdit, command);
    ShowWindow(gChipCombo, SW_HIDE);
}


static void LayoutSerialControls(const RECT* client)
{
    int left = 54;
    int controlTop = 207;
    int controlHeight = 30;
    MoveWindow(gComCombo, left + 34, controlTop + 2, 164, 280, TRUE);
    MoveWindow(gBaudCombo, left + 234, controlTop + 2, 142, 280, TRUE);
    MoveWindow(gRepeatEdit, left + 412, controlTop + 3, 92, controlHeight, TRUE);
    MoveWindow(gWaitEdit, left + 540, controlTop + 3, 92, controlHeight, TRUE);
}

static void DrawButton(HDC dc, RECT rect, LPCWSTR text, COLORREF fill, COLORREF border, COLORREF textColor, BOOL hover)
{
    if (hover)
    {
        fill = RGB((BYTE)((fill & 0xFF) > 245 ? 255 : (fill & 0xFF) + 8),
                   (BYTE)((((fill >> 8) & 0xFF) > 245) ? 255 : ((fill >> 8) & 0xFF) + 8),
                   (BYTE)((((fill >> 16) & 0xFF) > 245) ? 255 : ((fill >> 16) & 0xFF) + 8));
    }
    DrawRoundBox(dc, &rect, 18, fill, border);
    DrawTextBlock(dc, text, rect, gFontSmall, textColor, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}


static void DrawInputShell(HDC dc, RECT rect, BOOL emphasized)
{
    COLORREF border = emphasized ? ThemeAccent() : ThemeInputBorder();
    DrawRoundBox(dc, &rect, 16, ThemeInputFill(), border);
}

static COLORREF CurrentMainProgressColor(void)
{
    if (gProgressVisualState == 1) { return ThemeSuccess(); }
    if (gProgressVisualState == -1) { return ThemeDanger(); }
    if (gProgressVisualState == 2) { return ThemeWarning(); }
    return ThemeAccent();
}

static COLORREF CurrentBatchProgressColor(void)
{
    if (gCompletedCount >= gRepeatTotal && gRepeatTotal > 0)
    {
        return (gFailureCount == 0) ? ThemeSuccess() : ThemeDanger();
    }
    if (gFailureCount > 0) { return ThemeWarning(); }
    return ThemeAccent();
}

static void RefreshChildControlTheme(void)
{
    if (gComCombo) { InvalidateRect(gComCombo, NULL, TRUE); }
    if (gBaudCombo) { InvalidateRect(gBaudCombo, NULL, TRUE); }
    if (gRepeatEdit) { InvalidateRect(gRepeatEdit, NULL, TRUE); }
    if (gWaitEdit) { InvalidateRect(gWaitEdit, NULL, TRUE); }
    if (gChipCombo) { InvalidateRect(gChipCombo, NULL, TRUE); }
    if (gCanDeviceCombo) { InvalidateRect(gCanDeviceCombo, NULL, TRUE); }
    if (gCanChannelCombo) { InvalidateRect(gCanChannelCombo, NULL, TRUE); }
    if (gCanBaudCombo) { InvalidateRect(gCanBaudCombo, NULL, TRUE); }
    if (gCanLocalEdit) { InvalidateRect(gCanLocalEdit, NULL, TRUE); }
    if (gCanTargetEdit) { InvalidateRect(gCanTargetEdit, NULL, TRUE); }
    if (gCanRepeatEdit) { InvalidateRect(gCanRepeatEdit, NULL, TRUE); }
    if (gCanWaitEdit) { InvalidateRect(gCanWaitEdit, NULL, TRUE); }
    if (gBleFilterEdit) { InvalidateRect(gBleFilterEdit, NULL, TRUE); }
    if (gBleIntervalCombo) { InvalidateRect(gBleIntervalCombo, NULL, TRUE); }
    if (gBleVersionListEdit) { InvalidateRect(gBleVersionListEdit, NULL, TRUE); }
    if (gBleOtaGapEdit) { InvalidateRect(gBleOtaGapEdit, NULL, TRUE); }
    if (gBleOtaTimeoutEdit) { InvalidateRect(gBleOtaTimeoutEdit, NULL, TRUE); }
    if (gBleOtaChannelCombo) { InvalidateRect(gBleOtaChannelCombo, NULL, TRUE); }
    if (gBleOtaChipCombo) { InvalidateRect(gBleOtaChipCombo, NULL, TRUE); }
    { int index; for (index = 0; index < BLE_OTA_VISIBLE_ROWS; index++) { if (gBleOtaRowChipCombos[index]) { InvalidateRect(gBleOtaRowChipCombos[index], NULL, TRUE); } } }
}

static void DrawFirmwareInfoRow(HDC dc, const RECT* panel, int y, LPCWSTR field, LPCWSTR description, LPCWSTR value, BOOL ellipsis)
{
    RECT fieldRect = { panel->left + 24, y, panel->left + 174, y + 18 };
    RECT descRect = { panel->left + 24, y + 16, panel->left + 174, y + 31 };
    RECT valueRect = { panel->left + 180, y, panel->right - 22, y + 31 };
    UINT format = DT_LEFT | DT_VCENTER | DT_SINGLELINE;
    DrawTextBlock(dc, field, fieldRect, gFontSmall, ThemeText(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawTextBlock(dc, description, descRect, gFontTiny, ThemeMuted2(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    if (ellipsis) { format |= DT_END_ELLIPSIS; }
    DrawTextBlock(dc, value, valueRect, gFontSmall, ThemeMuted(), format);
}


static void DrawSerialPage(HDC dc, const RECT* client)
{
    int rightPanelWidth = 500;
    int gap = 20;
    int leftMargin = 54;
    int rightMargin = 54;
    int mainTop = 282;
    int bottom = client->bottom - 60;
    int leftPanelRight = client->right - rightMargin - rightPanelWidth - gap;
    int infoBottom = mainTop + 410;
    RECT pageTitle = { leftMargin + 140, 78, client->right - rightMargin, 116 };
    RECT pageSubtitle = { leftMargin + 140, 113, client->right - rightMargin, 142 };
    RECT config = { leftMargin, 150, client->right - rightMargin, 264 };
    RECT progressPanel = { leftMargin, mainTop, leftPanelRight, bottom };
    RECT infoPanel = { leftPanelRight + gap, mainTop, client->right - rightMargin, infoBottom };
    RECT rulePanel = { leftPanelRight + gap, infoBottom + 16, client->right - rightMargin, bottom };
    RECT label;
    RECT progressBg;
    RECT progressFill;
    RECT percent;
    RECT status;
    RECT section;
    RECT batchPanel;
    RECT batchBg;
    RECT batchFill;
    RECT logBox;
    RECT ruleItem;
    int progressWidth;
    int batchWidth;
    int y;
    int index;
    wchar_t text[320];
    LPCWSTR typeName = L"Unknown";

    if (infoBottom > bottom - 112) { infoBottom = bottom - 112; infoPanel.bottom = infoBottom; rulePanel.top = infoBottom + 14; }
    if (gFirmwareType == 1) { typeName = L"ARM"; }
    else if (gFirmwareType == 2) { typeName = L"DSP"; }
    else if (gFirmwareType == 3) { typeName = L"BMS"; }
    else if (gFirmwareType == 4) { typeName = L"IOT"; }
    else if (gFirmwareType == 5) { typeName = L"M1"; }
    else if (gFirmwareType == 6) { typeName = L"PACK"; }

    AcquireSRWLockShared(&gStateLock);

    gBackRect.left = leftMargin; gBackRect.top = 82; gBackRect.right = leftMargin + 118; gBackRect.bottom = 122;
    DrawPill(dc, gBackRect, L"←  返回首页", ThemeSurface(), ThemeBorder(), ThemeText(), gHoverItem == 10);
    DrawTextBlock(dc, L"串口固件升级", pageTitle, gFontTitle, ThemeText(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawTextBlock(dc, L"MODBUS RTU 启动握手  /  XMODEM-1K 数据传输  /  连续升级可靠性测试", pageSubtitle, gFontSmall, ThemeMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    {
        RECT shadow = { config.left + 2, config.top + 5, config.right + 2, config.bottom + 5 };
        DrawRoundBox(dc, &shadow, 22, ThemeShadow(), ThemeShadow());
        DrawRoundBox(dc, &config, 22, ThemeSurface(), ThemeBorder());
    }

    label.left = leftMargin + 24; label.top = 166; label.right = leftMargin + 208; label.bottom = 200;
    DrawTextBlock(dc, L"串口设备", label, gFontTiny, ThemeMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    label.left = leftMargin + 224; label.right = leftMargin + 386;
    DrawTextBlock(dc, L"通信波特率", label, gFontTiny, ThemeMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    label.left = leftMargin + 402; label.right = leftMargin + 514;
    DrawTextBlock(dc, L"连续升级次数", label, gFontTiny, ThemeMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    label.left = leftMargin + 530; label.right = leftMargin + 642;
    DrawTextBlock(dc, L"循环等待（秒）", label, gFontTiny, ThemeMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    {
        RECT inputRect;
        gComSelectorRect.left = leftMargin + 24; gComSelectorRect.top = 202; gComSelectorRect.right = leftMargin + 208; gComSelectorRect.bottom = 246;
        gBaudSelectorRect.left = leftMargin + 224; gBaudSelectorRect.top = 202; gBaudSelectorRect.right = leftMargin + 386; gBaudSelectorRect.bottom = 246;
        DrawUiSelectorControl(dc, &gComSelectorRect, UI_SELECTOR_COM, gUiSelectorOpen == UI_SELECTOR_COM, gHoverItem == 180);
        DrawUiSelectorControl(dc, &gBaudSelectorRect, UI_SELECTOR_BAUD, gUiSelectorOpen == UI_SELECTOR_BAUD, gHoverItem == 181);
        inputRect.left = leftMargin + 402; inputRect.top = 202; inputRect.right = leftMargin + 514; inputRect.bottom = 246;
        DrawInputShell(dc, inputRect, FALSE);
        inputRect.left = leftMargin + 530; inputRect.right = leftMargin + 642;
        DrawInputShell(dc, inputRect, FALSE);
    }

    gConnectRect.left = leftMargin + 674; gConnectRect.top = 204; gConnectRect.right = leftMargin + 838; gConnectRect.bottom = 244;
    gFirmwareRect.left = leftMargin + 856; gFirmwareRect.top = 204; gFirmwareRect.right = config.right - 24; gFirmwareRect.bottom = 244;
    DrawButton(dc, gConnectRect, gSerialConnected ? L"断开连接" : L"检测并连接", gSerialConnected ? ThemeSurfaceAlt() : ThemeAccent(), gSerialConnected ? ThemeBorder() : ThemeAccent(), gSerialConnected ? ThemeText() : RGB(255,255,255), gHoverItem == 11);
    DrawButton(dc, gFirmwareRect, L"选择升级固件", ThemeSurfaceAlt(), ThemeAccent(), ThemeAccent(), gHoverItem == 12);

    {
        RECT shadow = { progressPanel.left + 2, progressPanel.top + 5, progressPanel.right + 2, progressPanel.bottom + 5 };
        DrawRoundBox(dc, &shadow, 24, ThemeShadow(), ThemeShadow());
        DrawRoundBox(dc, &progressPanel, 24, ThemeSurface(), ThemeBorder());
    }
    section.left = progressPanel.left + 28; section.top = progressPanel.top + 18; section.right = progressPanel.right - 28; section.bottom = progressPanel.top + 50;
    DrawTextBlock(dc, L"升级任务状态", section, gFontCardTitle, ThemeText(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    percent.left = progressPanel.left + 28; percent.top = progressPanel.top + 52; percent.right = progressPanel.right - 28; percent.bottom = progressPanel.top + 116;
    wsprintfW(text, L"%d%%", gUpgradeProgress);
    DrawTextBlock(dc, text, percent, gFontPercent, ThemeAccent(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    progressBg.left = progressPanel.left + 30; progressBg.top = progressPanel.top + 126; progressBg.right = progressPanel.right - 30; progressBg.bottom = progressPanel.top + 143;
    DrawRoundBox(dc, &progressBg, 12, gDarkMode ? RGB(31,45,65) : RGB(230,236,245), gDarkMode ? RGB(31,45,65) : RGB(230,236,245));
    progressWidth = (progressBg.right - progressBg.left) * gUpgradeProgress / 100;
    progressFill = progressBg;
    progressFill.right = progressFill.left + progressWidth;
    if (progressWidth > 4) { COLORREF progressColor = CurrentMainProgressColor(); DrawRoundBox(dc, &progressFill, 12, progressColor, progressColor); }

    status.left = progressPanel.left + 30; status.top = progressPanel.top + 153; status.right = progressPanel.right - 30; status.bottom = progressPanel.top + 186;
    DrawTextBlock(dc, gUpgradeStatus, status, gFontBody, ThemeText(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    status.top = progressPanel.top + 186; status.bottom = progressPanel.top + 216;
    if (gUpgradeRunning)
    {
        if (gWaitingNext) { wsprintfW(text, L"第 %d/%d 次完成  ·  等待 %d 秒  ·  %s", gCurrentRepeat, gRepeatTotal, gWaitRemaining, gProtocolStage); }
        else if (gTotalPackets > 0) { wsprintfW(text, L"第 %d/%d 次  ·  数据包 %d/%d  ·  尝试 %d/5  ·  %s", gCurrentRepeat, gRepeatTotal, gCurrentPacket, gTotalPackets, gCurrentAttempt, gProtocolStage); }
        else { wsprintfW(text, L"第 %d/%d 次  ·  %s", gCurrentRepeat, gRepeatTotal, gProtocolStage); }
    }
    else { wsprintfW(text, L"重复测试 %d 次  ·  间隔 %d 秒  ·  %s", gRepeatTotal, gWaitSeconds, gProtocolStage); }
    DrawTextBlock(dc, text, status, gFontSmall, ThemeMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    batchPanel.left = progressPanel.left + 28; batchPanel.top = progressPanel.top + 224; batchPanel.right = progressPanel.right - 28; batchPanel.bottom = progressPanel.top + 330;
    DrawRoundBox(dc, &batchPanel, 18, ThemeSurfaceAlt(), ThemeBorder());
    section.left = batchPanel.left + 18; section.top = batchPanel.top + 8; section.right = batchPanel.right - 18; section.bottom = batchPanel.top + 34;
    DrawTextBlock(dc, L"连续升级总览  /  BATCH RELIABILITY", section, gFontSmall, ThemeText(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    wsprintfW(text, L"已完成 %d / %d", gCompletedCount, gRepeatTotal);
    DrawTextBlock(dc, text, section, gFontTiny, ThemeMuted(), DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    batchBg.left = batchPanel.left + 18; batchBg.top = batchPanel.top + 42; batchBg.right = batchPanel.right - 18; batchBg.bottom = batchPanel.top + 54;
    DrawRoundBox(dc, &batchBg, 10, gDarkMode ? RGB(31,45,65) : RGB(230,236,245), gDarkMode ? RGB(31,45,65) : RGB(230,236,245));
    batchWidth = (gRepeatTotal > 0) ? ((batchBg.right - batchBg.left) * gCompletedCount / gRepeatTotal) : 0;
    batchFill = batchBg;
    batchFill.right = batchFill.left + batchWidth;
    if (batchWidth > 3) { COLORREF batchColor = CurrentBatchProgressColor(); DrawRoundBox(dc, &batchFill, 10, batchColor, batchColor); }
    {
        RECT stat1 = { batchPanel.left + 18, batchPanel.top + 62, batchPanel.left + 190, batchPanel.top + 90 };
        RECT stat2 = { batchPanel.left + 200, batchPanel.top + 62, batchPanel.left + 372, batchPanel.top + 90 };
        RECT stat3 = { batchPanel.left + 382, batchPanel.top + 62, batchPanel.right - 18, batchPanel.top + 90 };
        wsprintfW(text, L"成功  %d", gSuccessCount);
        DrawTextBlock(dc, text, stat1, gFontSmall, ThemeSuccess(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        wsprintfW(text, L"失败  %d", gFailureCount);
        DrawTextBlock(dc, text, stat2, gFontSmall, gFailureCount > 0 ? ThemeDanger() : ThemeMuted2(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        wsprintfW(text, L"待执行  %d", (gRepeatTotal > gCompletedCount) ? (gRepeatTotal - gCompletedCount) : 0);
        DrawTextBlock(dc, text, stat3, gFontSmall, ThemeMuted(), DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }

    logBox.left = progressPanel.left + 28; logBox.top = progressPanel.top + 342; logBox.right = progressPanel.right - 28; logBox.bottom = progressPanel.bottom - 80;
    DrawRoundBox(dc, &logBox, 18, ThemeSurfaceAlt(), ThemeBorder());
    section.left = logBox.left + 18; section.top = logBox.top + 8; section.right = logBox.right - 18; section.bottom = logBox.top + 36;
    DrawTextBlock(dc, L"运行日志  /  REAL-TIME TRACE", section, gFontSmall, ThemeText(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    y = logBox.top + 38;
    {
        int maxLines = (logBox.bottom - y - 8) / 22;
        int firstLog = 8 - maxLines;
        if (firstLog < 0) { firstLog = 0; }
        for (index = firstLog; index < 8; index++)
        {
            RECT logRect = { logBox.left + 18, y, logBox.right - 18, y + 24 };
            if (gLogs[index][0] != 0) { DrawTextBlock(dc, gLogs[index], logRect, gFontTiny, ThemeMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS); }
            y += 22;
        }
    }

    gStartRect.left = progressPanel.left + 28; gStartRect.bottom = progressPanel.bottom - 20; gStartRect.top = gStartRect.bottom - 44; gStartRect.right = progressPanel.left + 206;
    gStopRect.left = gStartRect.right + 16; gStopRect.top = gStartRect.top; gStopRect.right = gStopRect.left + 178; gStopRect.bottom = gStartRect.bottom;
    DrawButton(dc, gStartRect, gUpgradeRunning ? L"升级进行中" : L"开始升级", gUpgradeRunning ? (gDarkMode ? RGB(54,69,91) : RGB(190,204,224)) : ThemeAccent(), gUpgradeRunning ? (gDarkMode ? RGB(54,69,91) : RGB(190,204,224)) : ThemeAccent(), RGB(255,255,255), !gUpgradeRunning && gHoverItem == 13);
    DrawButton(dc, gStopRect, L"终止升级", ThemeSurface(), gUpgradeRunning ? RGB(235,88,94) : ThemeBorder(), gUpgradeRunning ? RGB(235,88,94) : ThemeMuted2(), gUpgradeRunning && gHoverItem == 14);

    {
        RECT shadow = { infoPanel.left + 2, infoPanel.top + 5, infoPanel.right + 2, infoPanel.bottom + 5 };
        DrawRoundBox(dc, &shadow, 24, ThemeShadow(), ThemeShadow());
        DrawRoundBox(dc, &infoPanel, 24, ThemeSurface(), ThemeBorder());
    }
    section.left = infoPanel.left + 24; section.top = infoPanel.top + 16; section.right = infoPanel.right - 24; section.bottom = infoPanel.top + 52;
    DrawTextBlock(dc, L"升级固件信息", section, gFontCardTitle, ThemeText(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawLine(dc, infoPanel.left + 24, infoPanel.top + 58, infoPanel.right - 24, infoPanel.top + 58, ThemeBorder());

    y = infoPanel.top + 66;
    DrawFirmwareInfoRow(dc, &infoPanel, y, L"文件名称", L"Firmware File", gFirmwareReady ? gFirmwareName : L"尚未选择", TRUE); y += 32;
    DrawFirmwareInfoRow(dc, &infoPanel, y, L"芯片平台", L"Target Chip", gFirmwareReady ? CHIP_NAMES[gSelectedChip] : L"—", FALSE); y += 32;
    wsprintfW(text, L"0x%03X  /  %u Bytes", gHeaderAreaSize, gHeaderAreaSize);
    DrawFirmwareInfoRow(dc, &infoPanel, y, L"头部大小", L"Header Area", gFirmwareReady ? text : L"—", FALSE); y += 32;
    DrawFirmwareInfoRow(dc, &infoPanel, y, L"MagicNumber", L"固件识别符", gFirmwareReady ? gMagicText : L"—", FALSE); y += 32;
    wsprintfW(text, L"0x%02X  /  %s", (UINT)gFirmwareType, typeName);
    DrawFirmwareInfoRow(dc, &infoPanel, y, L"FirmwareType", L"固件类型", gFirmwareReady ? text : L"—", FALSE); y += 32;
    DrawFirmwareInfoRow(dc, &infoPanel, y, L"DevModel", L"项目代号", gFirmwareReady ? gDevModelText : L"—", FALSE); y += 32;
    wsprintfW(text, L"%u  /  0x%08X", gImageVersion, gImageVersion);
    DrawFirmwareInfoRow(dc, &infoPanel, y, L"Version", L"固件版本", gFirmwareReady ? text : L"—", FALSE); y += 32;
    wsprintfW(text, L"%u Bytes  /  %u KB", gImageSize, (gImageSize + 1023) / 1024);
    DrawFirmwareInfoRow(dc, &infoPanel, y, L"SizeOfBytes", L"固件大小", gFirmwareReady ? text : L"—", FALSE); y += 32;
    wsprintfW(text, L"0x%08X", gImageCrc32);
    DrawFirmwareInfoRow(dc, &infoPanel, y, L"Crc32", L"固件 CRC32（不含头部）", gFirmwareReady ? text : L"—", FALSE); y += 32;
    DrawFirmwareInfoRow(dc, &infoPanel, y, L"Time", L"编译日期", gFirmwareReady ? gTimeText : L"—", FALSE);

    {
        RECT shadow = { rulePanel.left + 2, rulePanel.top + 5, rulePanel.right + 2, rulePanel.bottom + 5 };
        DrawRoundBox(dc, &shadow, 22, ThemeShadow(), ThemeShadow());
        DrawRoundBox(dc, &rulePanel, 22, ThemeSurface(), ThemeBorder());
    }
    section.left = rulePanel.left + 22; section.top = rulePanel.top + 10; section.right = rulePanel.right - 22; section.bottom = rulePanel.top + 40;
    DrawTextBlock(dc, L"IMAGE_HEADER_T 解析规则", section, gFontSmall, ThemeText(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    ruleItem.left = rulePanel.left + 22; ruleItem.top = rulePanel.top + 44; ruleItem.right = (rulePanel.left + rulePanel.right) / 2 - 6; ruleItem.bottom = rulePanel.top + 70;
    DrawPill(dc, ruleItem, L"结构体偏移固定为 0x000", ThemeSurfaceAlt(), ThemeBorder(), ThemeMuted(), FALSE);
    ruleItem.left = (rulePanel.left + rulePanel.right) / 2 + 6; ruleItem.right = rulePanel.right - 22;
    DrawPill(dc, ruleItem, L"u4byte 按小端序解析", ThemeSurfaceAlt(), ThemeBorder(), ThemeMuted(), FALSE);
    ruleItem.left = rulePanel.left + 22; ruleItem.top = rulePanel.top + 76; ruleItem.right = (rulePanel.left + rulePanel.right) / 2 - 6; ruleItem.bottom = rulePanel.top + 102;
    DrawPill(dc, ruleItem, L"TI：16位Word布局 / 头区0x400；其他：8位布局 / 头区0x200", ThemeSurfaceAlt(), ThemeBorder(), ThemeMuted(), FALSE);
    ruleItem.left = (rulePanel.left + rulePanel.right) / 2 + 6; ruleItem.right = rulePanel.right - 22;
    DrawPill(dc, ruleItem, L"Crc32 不包含头部数据", ThemeSurfaceAlt(), ThemeBorder(), ThemeMuted(), FALSE);

    ReleaseSRWLockShared(&gStateLock);
}



static void SetCanControlsVisible(BOOL visible)
{
    int command = (visible && !gChipDialogOpen) ? SW_SHOW : SW_HIDE;
    /* V1.3.9: visible device/channel/baud selectors are custom-drawn. */
    ShowWindow(gCanDeviceCombo, SW_HIDE);
    ShowWindow(gCanChannelCombo, SW_HIDE);
    ShowWindow(gCanBaudCombo, SW_HIDE);
    ShowWindow(gCanLocalEdit, command);
    ShowWindow(gCanTargetEdit, command);
    ShowWindow(gCanRepeatEdit, command);
    ShowWindow(gCanWaitEdit, command);
    ShowWindow(gChipCombo, SW_HIDE);
}

static void HideAllUpgradeControls(void)
{
    gUiSelectorOpen = 0;
    SetBleControlsVisible(FALSE);
    SetBleOtaControlsVisible(FALSE);
    SetSerialControlsVisible(FALSE);
    SetCanControlsVisible(FALSE);
}

static void LayoutCanControls(const RECT* client)
{
    int left = 54;
    int top = 207;
    MoveWindow(gCanDeviceCombo, left + 32, top + 2, 150, 280, TRUE);
    MoveWindow(gCanChannelCombo, left + 212, top + 2, 74, 200, TRUE);
    MoveWindow(gCanBaudCombo, left + 316, top + 2, 106, 240, TRUE);
    MoveWindow(gCanLocalEdit, left + 450, top + 3, 68, 30, TRUE);
    MoveWindow(gCanTargetEdit, left + 548, top + 3, 68, 30, TRUE);
    MoveWindow(gCanRepeatEdit, left + 646, top + 3, 68, 30, TRUE);
    MoveWindow(gCanWaitEdit, left + 744, top + 3, 68, 30, TRUE);
}

static void DrawCanModePage(HDC dc, const RECT* client)
{
    RECT title = { 54, 88, client->right - 54, 132 };
    RECT subtitle = { 54, 128, client->right - 54, 164 };
    RECT back;
    int width = 460;
    int gap = 34;
    int total = width * 2 + gap;
    int left = (client->right - total) / 2;
    int top = 220;
    int bottom = client->bottom - 110;
    int index;
    static const wchar_t* names[2] = { L"单节点升级", L"广播升级" };
    static const wchar_t* english[2] = { L"SINGLE NODE", L"BROADCAST OTA" };
    static const wchar_t* desc[2] = {
        L"指定本地地址和目标地址，完成单台设备的 CAN 固件升级、超时重试与连续可靠性测试。",
        L"目标地址固定为 0xFF，自动发现应答节点，并为每台设备独立显示升级进度和结果。"
    };
    static const wchar_t* tags[2] = { L"PRECISION LINK", L"MULTI-NODE" };

    back.left = 54; back.top = 82; back.right = 172; back.bottom = 122;
    gBackRect = back;
    DrawPill(dc, back, L"←  返回首页", ThemeSurface(), ThemeBorder(), ThemeText(), gHoverItem == 10);
    DrawTextBlock(dc, L"CAN 固件升级", title, gFontTitle, ThemeText(), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    DrawTextBlock(dc, L"CANalyst-II  /  29-bit Extended Frame  /  1K Block Protocol", subtitle, gFontSmall, ThemeMuted(), DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    for (index = 0; index < 2; index++)
    {
        RECT card = { left + index * (width + gap), top, left + index * (width + gap) + width, bottom };
        RECT shadow = { card.left + 3, card.top + 7, card.right + 3, card.bottom + 7 };
        RECT tag = { card.left + 30, card.top + 26, card.left + 178, card.top + 58 };
        RECT icon = { card.left + 34, card.top + 86, card.left + 142, card.top + 194 };
        RECT heading = { card.left + 34, card.top + 222, card.right - 34, card.top + 266 };
        RECT englishRect = { card.left + 34, card.top + 262, card.right - 34, card.top + 294 };
        RECT description = { card.left + 34, card.top + 318, card.right - 34, card.top + 400 };
        RECT enter = { card.left + 34, card.bottom - 72, card.right - 34, card.bottom - 26 };
        BOOL hover = (gHoverItem == 40 + index);
        COLORREF accent = index == 0 ? RGB(35,132,255) : RGB(155,91,255);
        gCanModeRects[index] = card;
        DrawRoundBox(dc, &shadow, 30, ThemeShadow(), ThemeShadow());
        DrawRoundBox(dc, &card, 30, hover ? (gDarkMode ? RGB(19,34,57) : RGB(251,253,255)) : ThemeSurface(), hover ? accent : ThemeBorder());
        DrawPill(dc, tag, tags[index], ThemeSurfaceAlt(), hover ? accent : ThemeBorder(), accent, FALSE);
        DrawRoundBox(dc, &icon, 30, gDarkMode ? RGB(19,32,53) : RGB(240,247,255), hover ? accent : ThemeBorder());
        DrawCanIcon(dc, (icon.left + icon.right) / 2, (icon.top + icon.bottom) / 2, accent);
        DrawTextBlock(dc, names[index], heading, gFontCardTitle, ThemeText(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        DrawTextBlock(dc, english[index], englishRect, gFontSmall, accent, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        DrawTextBlock(dc, desc[index], description, gFontBody, ThemeMuted(), DT_LEFT | DT_WORDBREAK);
        DrawTextBlock(dc, hover ? L"进入升级工作台   →" : L"选择模式   →", enter, gFontBody, hover ? accent : ThemeText(), DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }
}

static LPCWSTR FirmwareTypeName(void)
{
    if (gFirmwareType == 1) { return L"ARM"; }
    if (gFirmwareType == 2) { return L"DSP"; }
    if (gFirmwareType == 3) { return L"BMS"; }
    if (gFirmwareType == 4) { return L"IOT"; }
    if (gFirmwareType == 5) { return L"M1"; }
    if (gFirmwareType == 6) { return L"PACK"; }
    return L"Unknown";
}

static void DrawCanNodeMatrix(HDC dc, const RECT* panel)
{
    RECT title = { panel->left + 22, panel->top + 10, panel->right - 22, panel->top + 40 };
    int rowTop = panel->top + 46;
    int index;
    wchar_t text[128];
    DrawTextBlock(dc, gCanMode == 2 ? L"广播设备进度  /  NODE MATRIX" : L"目标设备状态  /  TARGET NODE", title, gFontSmall, ThemeText(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    if (gCanMode == 2 && gCanNodeCount == 0)
    {
        RECT empty = { panel->left + 22, rowTop + 12, panel->right - 22, rowTop + 64 };
        DrawTextBlock(dc, L"等待 0x72 应答后自动添加设备进度条", empty, gFontSmall, ThemeMuted(), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }
    if (gCanMode == 1)
    {
        RECT row = { panel->left + 18, rowTop, panel->right - 18, rowTop + 54 };
        RECT bg = { row.left + 128, row.top + 28, row.right - 16, row.top + 40 };
        RECT fill = bg;
        int progress = gUpgradeProgress;
        wsprintfW(text, L"设备  ·  0x%02X", (UINT)gCanTargetAddress);
        DrawTextBlock(dc, text, row, gFontSmall, ThemeText(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        DrawRoundBox(dc, &bg, 8, gDarkMode ? RGB(31,45,65) : RGB(230,236,245), gDarkMode ? RGB(31,45,65) : RGB(230,236,245));
        fill.right = fill.left + (bg.right - bg.left) * progress / 100;
        if (fill.right > fill.left + 2) { DrawRoundBox(dc, &fill, 8, CurrentMainProgressColor(), CurrentMainProgressColor()); }
        return;
    }
    {
        int maxRows = (panel->bottom - rowTop - 8) / 38;
        if (maxRows < 1) { maxRows = 1; }
        for (index = 0; index < gCanNodeCount && index < CAN_MAX_BROADCAST_NODES && index < maxRows; index++)
        {
        CAN_NODE_STATE node = gCanNodes[index];
        RECT row = { panel->left + 18, rowTop + index * 38, panel->right - 18, rowTop + index * 38 + 34 };
        RECT bg = { row.left + 146, row.top + 16, row.right - 68, row.top + 27 };
        RECT fill = bg;
        RECT percent = { row.right - 62, row.top, row.right, row.bottom };
        COLORREF color = node.Failed ? ThemeDanger() : (node.Completed ? ThemeSuccess() : ThemeAccent());
        wsprintfW(text, L"设备 %d  ·  地址 0x%02X", index + 1, (UINT)node.Address);
        DrawTextBlock(dc, text, row, gFontTiny, node.Failed ? ThemeDanger() : ThemeText(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        DrawRoundBox(dc, &bg, 7, gDarkMode ? RGB(31,45,65) : RGB(230,236,245), gDarkMode ? RGB(31,45,65) : RGB(230,236,245));
        fill.right = fill.left + (bg.right - bg.left) * node.Progress / 100;
        if (fill.right > fill.left + 2) { DrawRoundBox(dc, &fill, 7, color, color); }
        if (node.Failed) { wsprintfW(text, L"失败"); }
        else { wsprintfW(text, L"%u%%", (UINT)node.Progress); }
        DrawTextBlock(dc, text, percent, gFontTiny, color, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        }
        if (gCanNodeCount > maxRows)
        {
            RECT more = { panel->left + 22, panel->bottom - 28, panel->right - 22, panel->bottom - 6 };
            wsprintfW(text, L"另有 %d 台设备，详细状态见运行日志", gCanNodeCount - maxRows);
            DrawTextBlock(dc, text, more, gFontTiny, ThemeMuted(), DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        }
    }
}

static void DrawCanUpgradePage(HDC dc, const RECT* client)
{
    int rightPanelWidth = 500;
    int gap = 20;
    int leftMargin = 54;
    int rightMargin = 54;
    int mainTop = 282;
    int bottom = client->bottom - 60;
    int leftPanelRight = client->right - rightMargin - rightPanelWidth - gap;
    int infoBottom = mainTop + 410;
    RECT pageTitle = { leftMargin + 140, 78, client->right - rightMargin, 116 };
    RECT pageSubtitle = { leftMargin + 140, 113, client->right - rightMargin, 142 };
    RECT config = { leftMargin, 150, client->right - rightMargin, 264 };
    RECT progressPanel = { leftMargin, mainTop, leftPanelRight, bottom };
    RECT infoPanel = { leftPanelRight + gap, mainTop, client->right - rightMargin, infoBottom };
    RECT nodePanel = { leftPanelRight + gap, infoBottom + 16, client->right - rightMargin, bottom };
    RECT label;
    RECT inputRect;
    RECT section;
    RECT percent;
    RECT progressBg;
    RECT progressFill;
    RECT status;
    RECT batchPanel;
    RECT batchBg;
    RECT batchFill;
    RECT logBox;
    int y;
    int index;
    int progressWidth;
    int batchWidth;
    wchar_t text[320];

    if (infoBottom > bottom - 148) { infoBottom = bottom - 148; infoPanel.bottom = infoBottom; nodePanel.top = infoBottom + 14; }
    AcquireSRWLockShared(&gStateLock);

    gBackRect.left = leftMargin; gBackRect.top = 82; gBackRect.right = leftMargin + 118; gBackRect.bottom = 122;
    DrawPill(dc, gBackRect, L"←  返回模式", ThemeSurface(), ThemeBorder(), ThemeText(), gHoverItem == 10);
    DrawTextBlock(dc, gCanMode == 2 ? L"CAN 广播固件升级" : L"CAN 单节点固件升级", pageTitle, gFontTitle, ThemeText(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawTextBlock(dc, L"CANalyst-II  /  29位扩展帧  /  0x70~0x78 OTA 协议  /  1K数据块", pageSubtitle, gFontSmall, ThemeMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    { RECT shadow = { config.left + 2, config.top + 5, config.right + 2, config.bottom + 5 }; DrawRoundBox(dc, &shadow, 22, ThemeShadow(), ThemeShadow()); DrawRoundBox(dc, &config, 22, ThemeSurface(), ThemeBorder()); }
    SetRectCoords(&label, leftMargin + 24, 166, leftMargin + 198, 200); DrawTextBlock(dc, L"CAN 设备", label, gFontTiny, ThemeMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SetRectCoords(&label, leftMargin + 204, 166, leftMargin + 294, 200); DrawTextBlock(dc, L"CAN 通道", label, gFontTiny, ThemeMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SetRectCoords(&label, leftMargin + 308, 166, leftMargin + 430, 200); DrawTextBlock(dc, L"CAN 波特率", label, gFontTiny, ThemeMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SetRectCoords(&label, leftMargin + 442, 166, leftMargin + 530, 200); DrawTextBlock(dc, L"本地地址 HEX", label, gFontTiny, ThemeMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SetRectCoords(&label, leftMargin + 540, 166, leftMargin + 628, 200); DrawTextBlock(dc, gCanMode == 2 ? L"目标地址（广播）" : L"目标地址 HEX", label, gFontTiny, ThemeMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SetRectCoords(&label, leftMargin + 638, 166, leftMargin + 726, 200); DrawTextBlock(dc, L"连续次数", label, gFontTiny, ThemeMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SetRectCoords(&label, leftMargin + 736, 166, leftMargin + 824, 200); DrawTextBlock(dc, L"循环等待（秒）", label, gFontTiny, ThemeMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    SetRectCoords(&gCanDeviceSelectorRect, leftMargin + 24, 202, leftMargin + 198, 246);
    SetRectCoords(&gCanChannelSelectorRect, leftMargin + 204, 202, leftMargin + 294, 246);
    SetRectCoords(&gCanBaudSelectorRect, leftMargin + 308, 202, leftMargin + 430, 246);
    DrawUiSelectorControl(dc, &gCanDeviceSelectorRect, UI_SELECTOR_CAN_DEVICE, gUiSelectorOpen == UI_SELECTOR_CAN_DEVICE, gHoverItem == 182);
    DrawUiSelectorControl(dc, &gCanChannelSelectorRect, UI_SELECTOR_CAN_CHANNEL, gUiSelectorOpen == UI_SELECTOR_CAN_CHANNEL, gHoverItem == 183);
    DrawUiSelectorControl(dc, &gCanBaudSelectorRect, UI_SELECTOR_CAN_BAUD, gUiSelectorOpen == UI_SELECTOR_CAN_BAUD, gHoverItem == 184);
    SetRectCoords(&inputRect, leftMargin + 442, 202, leftMargin + 530, 246); DrawInputShell(dc, inputRect, FALSE);
    SetRectCoords(&inputRect, leftMargin + 540, 202, leftMargin + 628, 246); DrawInputShell(dc, inputRect, gCanMode == 2);
    SetRectCoords(&inputRect, leftMargin + 638, 202, leftMargin + 726, 246); DrawInputShell(dc, inputRect, FALSE);
    SetRectCoords(&inputRect, leftMargin + 736, 202, leftMargin + 824, 246); DrawInputShell(dc, inputRect, FALSE);

    SetRectCoords(&gConnectRect, leftMargin + 842, 204, leftMargin + 1002, 244);
    SetRectCoords(&gFirmwareRect, leftMargin + 1020, 204, config.right - 24, 244);
    DrawButton(dc, gConnectRect, gCanConnected ? L"断开 CAN" : L"连接 CANalyst-II", gCanConnected ? ThemeSurfaceAlt() : ThemeAccent(), gCanConnected ? ThemeBorder() : ThemeAccent(), gCanConnected ? ThemeText() : RGB(255,255,255), gHoverItem == 11);
    DrawButton(dc, gFirmwareRect, L"选择升级固件", ThemeSurfaceAlt(), ThemeAccent(), ThemeAccent(), gHoverItem == 12);

    { RECT shadow = { progressPanel.left + 2, progressPanel.top + 5, progressPanel.right + 2, progressPanel.bottom + 5 }; DrawRoundBox(dc, &shadow, 24, ThemeShadow(), ThemeShadow()); DrawRoundBox(dc, &progressPanel, 24, ThemeSurface(), ThemeBorder()); }
    SetRectCoords(&section, progressPanel.left + 28, progressPanel.top + 18, progressPanel.right - 28, progressPanel.top + 50);
    DrawTextBlock(dc, L"CAN OTA 任务状态", section, gFontCardTitle, ThemeText(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SetRectCoords(&percent, progressPanel.left + 28, progressPanel.top + 52, progressPanel.right - 28, progressPanel.top + 116);
    wsprintfW(text, L"%d%%", gUpgradeProgress); DrawTextBlock(dc, text, percent, gFontPercent, CurrentMainProgressColor(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SetRectCoords(&progressBg, progressPanel.left + 30, progressPanel.top + 126, progressPanel.right - 30, progressPanel.top + 143);
    DrawRoundBox(dc, &progressBg, 12, gDarkMode ? RGB(31,45,65) : RGB(230,236,245), gDarkMode ? RGB(31,45,65) : RGB(230,236,245));
    progressWidth = (progressBg.right - progressBg.left) * gUpgradeProgress / 100; progressFill = progressBg; progressFill.right = progressFill.left + progressWidth;
    if (progressWidth > 4) { COLORREF c = CurrentMainProgressColor(); DrawRoundBox(dc, &progressFill, 12, c, c); }
    SetRectCoords(&status, progressPanel.left + 30, progressPanel.top + 153, progressPanel.right - 30, progressPanel.top + 186);
    DrawTextBlock(dc, gUpgradeStatus, status, gFontBody, ThemeText(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    status.top = progressPanel.top + 186; status.bottom = progressPanel.top + 216;
    if (gUpgradeRunning)
    {
        if (gWaitingNext) { wsprintfW(text, L"第 %d/%d 次完成  ·  等待 %d 秒  ·  %s", gCurrentRepeat, gRepeatTotal, gWaitRemaining, gProtocolStage); }
        else { wsprintfW(text, L"第 %d/%d 次  ·  1K块 %d/%d  ·  尝试 %d/5  ·  %s", gCurrentRepeat, gRepeatTotal, gCurrentPacket, gTotalPackets, gCurrentAttempt, gProtocolStage); }
    }
    else { wsprintfW(text, L"本地 0x%02X  ·  目标 0x%02X  ·  %s", (UINT)gCanLocalAddress, (UINT)(gCanMode == 2 ? 0xFF : gCanTargetAddress), gProtocolStage); }
    DrawTextBlock(dc, text, status, gFontSmall, ThemeMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    SetRectCoords(&batchPanel, progressPanel.left + 28, progressPanel.top + 224, progressPanel.right - 28, progressPanel.top + 330);
    DrawRoundBox(dc, &batchPanel, 18, ThemeSurfaceAlt(), ThemeBorder());
    SetRectCoords(&section, batchPanel.left + 18, batchPanel.top + 8, batchPanel.right - 18, batchPanel.top + 34);
    DrawTextBlock(dc, L"连续升级总览  /  BATCH RELIABILITY", section, gFontSmall, ThemeText(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    wsprintfW(text, L"已完成 %d / %d", gCompletedCount, gRepeatTotal); DrawTextBlock(dc, text, section, gFontTiny, ThemeMuted(), DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    SetRectCoords(&batchBg, batchPanel.left + 18, batchPanel.top + 42, batchPanel.right - 18, batchPanel.top + 54);
    DrawRoundBox(dc, &batchBg, 10, gDarkMode ? RGB(31,45,65) : RGB(230,236,245), gDarkMode ? RGB(31,45,65) : RGB(230,236,245));
    batchWidth = gRepeatTotal > 0 ? (batchBg.right - batchBg.left) * gCompletedCount / gRepeatTotal : 0; batchFill = batchBg; batchFill.right = batchFill.left + batchWidth;
    if (batchWidth > 3) { COLORREF c = CurrentBatchProgressColor(); DrawRoundBox(dc, &batchFill, 10, c, c); }
    { RECT a = { batchPanel.left + 18, batchPanel.top + 62, batchPanel.left + 190, batchPanel.top + 90 }; RECT b = { batchPanel.left + 200, batchPanel.top + 62, batchPanel.left + 372, batchPanel.top + 90 }; RECT c = { batchPanel.left + 382, batchPanel.top + 62, batchPanel.right - 18, batchPanel.top + 90 }; wsprintfW(text, L"成功  %d", gSuccessCount); DrawTextBlock(dc, text, a, gFontSmall, ThemeSuccess(), DT_LEFT | DT_VCENTER | DT_SINGLELINE); wsprintfW(text, L"失败  %d", gFailureCount); DrawTextBlock(dc, text, b, gFontSmall, gFailureCount ? ThemeDanger() : ThemeMuted2(), DT_LEFT | DT_VCENTER | DT_SINGLELINE); wsprintfW(text, L"待执行  %d", gRepeatTotal > gCompletedCount ? gRepeatTotal - gCompletedCount : 0); DrawTextBlock(dc, text, c, gFontSmall, ThemeMuted(), DT_RIGHT | DT_VCENTER | DT_SINGLELINE); }

    SetRectCoords(&logBox, progressPanel.left + 28, progressPanel.top + 342, progressPanel.right - 28, progressPanel.bottom - 80);
    DrawRoundBox(dc, &logBox, 18, ThemeSurfaceAlt(), ThemeBorder());
    SetRectCoords(&section, logBox.left + 18, logBox.top + 8, logBox.right - 18, logBox.top + 36); DrawTextBlock(dc, L"运行日志  /  CAN TRACE", section, gFontSmall, ThemeText(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    y = logBox.top + 38; { int maxLines = (logBox.bottom - y - 8) / 22; int firstLog = 8 - maxLines; if (firstLog < 0) firstLog = 0; for (index = firstLog; index < 8; index++) { RECT r = { logBox.left + 18, y, logBox.right - 18, y + 24 }; if (gLogs[index][0]) DrawTextBlock(dc, gLogs[index], r, gFontTiny, ThemeMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS); y += 22; } }
    SetRectCoords(&gStartRect, progressPanel.left + 28, progressPanel.bottom - 64, progressPanel.left + 206, progressPanel.bottom - 20);
    SetRectCoords(&gStopRect, gStartRect.right + 16, gStartRect.top, gStartRect.right + 194, gStartRect.bottom);
    DrawButton(dc, gStartRect, gUpgradeRunning ? L"升级进行中" : L"开始 CAN 升级", gUpgradeRunning ? (gDarkMode ? RGB(54,69,91) : RGB(190,204,224)) : ThemeAccent(), gUpgradeRunning ? ThemeBorder() : ThemeAccent(), RGB(255,255,255), !gUpgradeRunning && gHoverItem == 13);
    DrawButton(dc, gStopRect, L"终止升级", ThemeSurface(), gUpgradeRunning ? ThemeDanger() : ThemeBorder(), gUpgradeRunning ? ThemeDanger() : ThemeMuted2(), gUpgradeRunning && gHoverItem == 14);

    { RECT shadow = { infoPanel.left + 2, infoPanel.top + 5, infoPanel.right + 2, infoPanel.bottom + 5 }; DrawRoundBox(dc, &shadow, 24, ThemeShadow(), ThemeShadow()); DrawRoundBox(dc, &infoPanel, 24, ThemeSurface(), ThemeBorder()); }
    SetRectCoords(&section, infoPanel.left + 24, infoPanel.top + 16, infoPanel.right - 24, infoPanel.top + 52); DrawTextBlock(dc, L"升级固件信息", section, gFontCardTitle, ThemeText(), DT_LEFT | DT_VCENTER | DT_SINGLELINE); DrawLine(dc, infoPanel.left + 24, infoPanel.top + 58, infoPanel.right - 24, infoPanel.top + 58, ThemeBorder());
    y = infoPanel.top + 66;
    DrawFirmwareInfoRow(dc, &infoPanel, y, L"文件名称", L"Firmware File", gFirmwareReady ? gFirmwareName : L"尚未选择", TRUE); y += 32;
    DrawFirmwareInfoRow(dc, &infoPanel, y, L"芯片平台", L"Target Chip", gFirmwareReady ? CHIP_NAMES[gSelectedChip] : L"—", FALSE); y += 32;
    wsprintfW(text, L"0x%03X  /  %u Bytes", gHeaderAreaSize, gHeaderAreaSize); DrawFirmwareInfoRow(dc, &infoPanel, y, L"头部大小", L"Header Area", gFirmwareReady ? text : L"—", FALSE); y += 32;
    DrawFirmwareInfoRow(dc, &infoPanel, y, L"MagicNumber", L"固件识别符", gFirmwareReady ? gMagicText : L"—", FALSE); y += 32;
    wsprintfW(text, L"0x%02X  /  %s", (UINT)gFirmwareType, FirmwareTypeName()); DrawFirmwareInfoRow(dc, &infoPanel, y, L"FirmwareType", L"固件类型", gFirmwareReady ? text : L"—", FALSE); y += 32;
    DrawFirmwareInfoRow(dc, &infoPanel, y, L"DevModel", L"项目代号", gFirmwareReady ? gDevModelText : L"—", FALSE); y += 32;
    wsprintfW(text, L"%u  /  0x%08X", gImageVersion, gImageVersion); DrawFirmwareInfoRow(dc, &infoPanel, y, L"Version", L"固件版本", gFirmwareReady ? text : L"—", FALSE); y += 32;
    wsprintfW(text, L"%u Bytes  /  %u KB", gImageSize, (gImageSize + 1023u) / 1024u); DrawFirmwareInfoRow(dc, &infoPanel, y, L"SizeOfBytes", L"固件大小", gFirmwareReady ? text : L"—", FALSE); y += 32;
    wsprintfW(text, L"0x%08X", gImageCrc32); DrawFirmwareInfoRow(dc, &infoPanel, y, L"Crc32", L"固件 CRC32（不含头部）", gFirmwareReady ? text : L"—", FALSE); y += 32;
    DrawFirmwareInfoRow(dc, &infoPanel, y, L"Time", L"编译日期", gFirmwareReady ? gTimeText : L"—", FALSE);

    { RECT shadow = { nodePanel.left + 2, nodePanel.top + 5, nodePanel.right + 2, nodePanel.bottom + 5 }; DrawRoundBox(dc, &shadow, 22, ThemeShadow(), ThemeShadow()); DrawRoundBox(dc, &nodePanel, 22, ThemeSurface(), ThemeBorder()); }
    DrawCanNodeMatrix(dc, &nodePanel);
    ReleaseSRWLockShared(&gStateLock);
}

static void DrawPlaceholderPage(HDC dc, const RECT* client, LPCWSTR title, LPCWSTR description, int kind)
{
    RECT panel = { client->right / 2 - 350, 198, client->right / 2 + 350, client->bottom - 118 };
    RECT eyebrow = { panel.left + 30, panel.top + 40, panel.right - 30, panel.top + 70 };
    RECT titleRect = { panel.left + 30, panel.top + 184, panel.right - 30, panel.top + 236 };
    RECT descRect = { panel.left + 70, panel.top + 240, panel.right - 70, panel.top + 310 };
    int cx = client->right / 2;
    gBackRect.left = 54; gBackRect.top = 82; gBackRect.right = 172; gBackRect.bottom = 122;
    DrawPill(dc, gBackRect, L"←  返回首页", ThemeSurface(), ThemeBorder(), ThemeText(), gHoverItem == 10);
    DrawRoundBox(dc, &panel, 28, ThemeSurface(), ThemeBorder());
    DrawTextBlock(dc, L"MODULE PREVIEW  /  COMING NEXT", eyebrow, gFontTiny, ThemeAccent(), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    if (kind == 1) { DrawBluetoothIcon(dc, cx, panel.top + 130, RGB(78,125,255)); }
    else { DrawCanIcon(dc, cx, panel.top + 130, RGB(246,166,55)); }
    DrawTextBlock(dc, title, titleRect, gFontTitle, ThemeText(), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    DrawTextBlock(dc, description, descRect, gFontBody, ThemeMuted(), DT_CENTER | DT_WORDBREAK);
}


static void DrawChipDialog(HDC dc, const RECT* client)
{
    RECT overlay = *client;
    RECT dialog = { client->right / 2 - 270, client->bottom / 2 - 148, client->right / 2 + 270, client->bottom / 2 + 158 };
    RECT title = { dialog.left + 30, dialog.top + 22, dialog.right - 30, dialog.top + 62 };
    RECT hint = { dialog.left + 34, dialog.top + 64, dialog.right - 34, dialog.top + 112 };
    HBRUSH brush = CreateSolidBrush(gDarkMode ? RGB(7,12,22) : RGB(238,243,250));
    FillRect(dc, &overlay, brush);
    DeleteObject(brush);
    DrawRoundBox(dc, &dialog, 26, ThemeSurface(), ThemeAccent());
    DrawTextBlock(dc, L"选择目标芯片平台", title, gFontCardTitle, ThemeText(), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    DrawTextBlock(dc, L"结构体固定从 0x000 读取。TI C2000 按16位Word物理布局解析，头区0x400；其他芯片按8位布局解析，头区0x200。", hint, gFontSmall, ThemeMuted(), DT_CENTER | DT_WORDBREAK);
    gChipSelectorRect.left = dialog.left + 120; gChipSelectorRect.top = dialog.top + 122; gChipSelectorRect.right = dialog.right - 120; gChipSelectorRect.bottom = dialog.top + 168;
    DrawChipSelectorControl(dc, &gChipSelectorRect, gSelectedChip, gChipSelectorOpen, gHoverItem == 170);
    gChipConfirmRect.left = dialog.left + 64; gChipConfirmRect.top = dialog.bottom - 66; gChipConfirmRect.right = dialog.left + 242; gChipConfirmRect.bottom = dialog.bottom - 22;
    gChipCancelRect.left = dialog.right - 242; gChipCancelRect.top = dialog.bottom - 66; gChipCancelRect.right = dialog.right - 64; gChipCancelRect.bottom = dialog.bottom - 22;
    DrawButton(dc, gChipConfirmRect, L"确认并解析", ThemeAccent(), ThemeAccent(), RGB(255,255,255), gHoverItem == 20);
    DrawButton(dc, gChipCancelRect, L"取消", ThemeSurfaceAlt(), ThemeBorder(), ThemeText(), gHoverItem == 21);
    if (gChipSelectorOpen) { DrawChipOptionsPopup(dc, &gChipSelectorRect, gSelectedChip, gChipSelectorOptionRects, client, 171); }
}

static DWORD ReadU32Le(const BYTE* data)
{
    return ((DWORD)data[0]) | ((DWORD)data[1] << 8) | ((DWORD)data[2] << 16) | ((DWORD)data[3] << 24);
}

static void AsciiToWideTrim(const BYTE* source, int sourceLength, wchar_t* destination, int destinationCount)
{
    int index;
    int out = 0;
    for (index = 0; index < sourceLength && out < destinationCount - 1; index++)
    {
        BYTE value = source[index];
        if (value == 0 || value == 0xFF) { break; }
        destination[out++] = (value >= 32 && value <= 126) ? (wchar_t)value : L'.';
    }
    while (out > 0 && destination[out - 1] == L' ') { out--; }
    destination[out] = 0;
    if (out == 0) { WCopy(destination, L"(empty)", destinationCount); }
}

static USHORT ReadU16Le(const BYTE* data)
{
    return (USHORT)((USHORT)data[0] | ((USHORT)data[1] << 8));
}

static void TiWordCharsToWideTrim(const BYTE* source, int logicalLength, wchar_t* destination, int destinationCount)
{
    BYTE logicalData[32];
    int index;
    if (logicalLength > 32) { logicalLength = 32; }
    for (index = 0; index < logicalLength; index++)
    {
        logicalData[index] = (BYTE)(ReadU16Le(source + (index * 2)) & 0x00FFu);
    }
    AsciiToWideTrim(logicalData, logicalLength, destination, destinationCount);
}

static BOOL ParseFirmwareHeader(void)
{
    HANDLE file;
    LARGE_INTEGER offset;
    LARGE_INTEGER fileSize;
    DWORD bytesRead = 0;
    BYTE data[96];
    BYTE logicalMagic[8];
    BOOL success = FALSE;
    int index;
    DWORD requiredSize;
    static const BYTE expectedMagic[8] = { 'P','O','W','E','R','O','A','K' };

    file = CreateFileW(gFirmwarePath, GENERIC_READ, 1, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file != INVALID_HANDLE_VALUE)
    {
        if (GetFileSizeEx(file, &fileSize))
        {
            gFirmwareFileSize = (unsigned long long)fileSize.QuadPart;
            gHeaderReadOffset = 0x000;
            gHeaderAreaSize = (gSelectedChip == 2) ? 0x400 : 0x200;
            requiredSize = (gSelectedChip == 2) ? 96u : 53u;
            if (fileSize.QuadPart >= requiredSize)
            {
                offset.QuadPart = 0;
                if (SetFilePointerEx(file, offset, NULL, FILE_BEGIN) && ReadFile(file, data, requiredSize, &bytesRead, NULL) && bytesRead == requiredSize)
                {
                    gHeaderSignatureValid = TRUE;
                    if (gSelectedChip == 2)
                    {
                        /* TI C2000: one logical u1byte occupies one physical 16-bit word in the BIN file. */
                        for (index = 0; index < 8; index++)
                        {
                            logicalMagic[index] = (BYTE)(ReadU16Le(data + (index * 2)) & 0x00FFu);
                            if (logicalMagic[index] != expectedMagic[index]) { gHeaderSignatureValid = FALSE; }
                        }
                        AsciiToWideTrim(logicalMagic, 8, gMagicText, 32);
                        wsprintfW(gMagicBytesText, L"%02X %02X %02X %02X %02X %02X %02X %02X", logicalMagic[0], logicalMagic[1], logicalMagic[2], logicalMagic[3], logicalMagic[4], logicalMagic[5], logicalMagic[6], logicalMagic[7]);
                        gFirmwareType = (BYTE)(ReadU16Le(data + 0x10) & 0x00FFu);
                        TiWordCharsToWideTrim(data + 0x12, 12, gDevModelText, 64);
                        /* DevModel[12] is followed by one 16-bit alignment word; Version begins at 0x2C. */
                        gImageVersion = (DWORD)ReadU16Le(data + 0x2C) | ((DWORD)ReadU16Le(data + 0x2E) << 16);
                        gImageSize = (DWORD)ReadU16Le(data + 0x30) | ((DWORD)ReadU16Le(data + 0x32) << 16);
                        gImageCrc32 = (DWORD)ReadU16Le(data + 0x34) | ((DWORD)ReadU16Le(data + 0x36) << 16);
                        TiWordCharsToWideTrim(data + 0x38, 20, gTimeText, 64);
                    }
                    else
                    {
                        AsciiToWideTrim(data, 8, gMagicText, 32);
                        wsprintfW(gMagicBytesText, L"%02X %02X %02X %02X %02X %02X %02X %02X", data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
                        for (index = 0; index < 8; index++)
                        {
                            if (data[index] != expectedMagic[index]) { gHeaderSignatureValid = FALSE; }
                        }
                        gFirmwareType = data[8];
                        AsciiToWideTrim(data + 9, 12, gDevModelText, 64);
                        gImageVersion = ReadU32Le(data + 21);
                        gImageSize = ReadU32Le(data + 25);
                        gImageCrc32 = ReadU32Le(data + 29);
                        AsciiToWideTrim(data + 33, 20, gTimeText, 64);
                    }
                    success = gHeaderSignatureValid;
                }
            }
        }
        CloseHandle(file);
    }
    return success;
}

static void ExtractFileName(void)
{
    int length = WLen(gFirmwarePath);
    int index = length - 1;
    while (index >= 0 && gFirmwarePath[index] != L'\\' && gFirmwarePath[index] != L'/') { index--; }
    WCopy(gFirmwareName, gFirmwarePath + index + 1, 260);
}

static void ShowChipDialog(void)
{
    RECT client;
    gChipDialogOpen = TRUE;
    gChipSelectorOpen = FALSE;
    gUiSelectorOpen = 0;
    ShowWindow(gChipCombo, SW_HIDE);
    /* Native child EDIT controls are separate HWNDs and otherwise stay above a parent-drawn
     * modal. Hide the current page inputs before painting the dialog. */
    if (gCurrentPage == 2) { SetSerialControlsVisible(TRUE); }
    else if (gCurrentPage == 4 || gCurrentPage == 5) { SetCanControlsVisible(TRUE); }
    else if (gCurrentPage == 1) { SetBleControlsVisible(TRUE); }
    else if (gCurrentPage == 6) { SetBleOtaControlsVisible(TRUE); }
    GetClientRect(gWindow, &client);
    if (gCurrentPage == 2) { LayoutSerialControls(&client); }
    else if (gCurrentPage == 4 || gCurrentPage == 5) { LayoutCanControls(&client); }
    RefreshParentAfterChildVisibility();
}

static void CloseChipDialog(void)
{
    RECT client;
    gChipDialogOpen = FALSE;
    gChipSelectorOpen = FALSE;
    ShowWindow(gChipCombo, SW_HIDE);
    GetClientRect(gWindow, &client);
    if (gCurrentPage == 2) { SetSerialControlsVisible(TRUE); LayoutSerialControls(&client); }
    else if (gCurrentPage == 4 || gCurrentPage == 5) { SetCanControlsVisible(TRUE); LayoutCanControls(&client); }
    else if (gCurrentPage == 1) { SetBleControlsVisible(TRUE); LayoutBleControls(&client); }
    else if (gCurrentPage == 6) { SetBleOtaControlsVisible(TRUE); LayoutBleOtaControls(&client); }
    RefreshParentAfterChildVisibility();
}

static void SelectFirmware(void)
{
    OPENFILENAMEW dialog;
    int index;
    for (index = 0; index < (int)(sizeof(dialog) / sizeof(BYTE)); index++) { ((BYTE*)&dialog)[index] = 0; }
    gFirmwarePath[0] = 0;
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = gWindow;
    dialog.lpstrFilter = L"Firmware Files (*.bin;*.img;*.fw)\0*.bin;*.img;*.fw\0All Files (*.*)\0*.*\0\0";
    dialog.lpstrFile = gFirmwarePath;
    dialog.nMaxFile = 1024;
    dialog.lpstrTitle = L"选择升级固件";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_NOCHANGEDIR;
    dialog.lpstrDefExt = L"bin";
    if (GetOpenFileNameW(&dialog))
    {
        ExtractFileName();
        AddLog(L"已选择升级固件，等待选择芯片平台");
        ShowChipDialog();
    }
}


static void ExtractNameFromPath(LPCWSTR path, LPWSTR output, int maxCount)
{
    int length = WLen(path);
    int index = length - 1;
    while (index >= 0 && path[index] != L'\\' && path[index] != L'/') { index--; }
    WCopy(output, path + index + 1, maxCount);
}

static BOOL ParseBleOtaFirmwareItem(BLE_OTA_ITEM* item, int chip)
{
    HANDLE file;
    LARGE_INTEGER fileSize;
    DWORD bytesRead = 0;
    DWORD requiredSize;
    BYTE data[96];
    BYTE logicalMagic[8];
    static const BYTE expectedMagic[8] = { 'P','O','W','E','R','O','A','K' };
    BOOL valid = TRUE;
    int index;
    if (item == NULL || item->Path[0] == 0) { return FALSE; }
    item->Ready = FALSE;
    item->Chip = chip;
    item->FirmwareType = 0;
    item->Version = 0;
    item->ImageSize = 0;
    item->ImageCrc32 = 0;
    item->FileSize = 0;
    item->DevModel[0] = 0;
    WCopy(item->Message, L"正在解析固件头", 128);
    file = CreateFileW(item->Path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) { WCopy(item->Message, L"无法打开固件文件", 128); return FALSE; }
    requiredSize = chip == 2 ? 96u : 53u;
    if (!GetFileSizeEx(file, &fileSize) || fileSize.QuadPart < requiredSize)
    {
        CloseHandle(file);
        WCopy(item->Message, L"固件文件过小", 128);
        return FALSE;
    }
    item->FileSize = (unsigned long long)fileSize.QuadPart;
    if (!ReadFile(file, data, requiredSize, &bytesRead, NULL) || bytesRead != requiredSize)
    {
        CloseHandle(file);
        WCopy(item->Message, L"读取固件头失败", 128);
        return FALSE;
    }
    CloseHandle(file);
    if (chip == 2)
    {
        for (index = 0; index < 8; index++)
        {
            logicalMagic[index] = (BYTE)(ReadU16Le(data + index * 2) & 0x00FFu);
            if (logicalMagic[index] != expectedMagic[index]) { valid = FALSE; }
        }
        item->FirmwareType = (BYTE)(ReadU16Le(data + 0x10) & 0x00FFu);
        TiWordCharsToWideTrim(data + 0x12, 12, item->DevModel, 64);
        item->Version = (DWORD)ReadU16Le(data + 0x2C) | ((DWORD)ReadU16Le(data + 0x2E) << 16);
        item->ImageSize = (DWORD)ReadU16Le(data + 0x30) | ((DWORD)ReadU16Le(data + 0x32) << 16);
        item->ImageCrc32 = (DWORD)ReadU16Le(data + 0x34) | ((DWORD)ReadU16Le(data + 0x36) << 16);
    }
    else
    {
        for (index = 0; index < 8; index++) { if (data[index] != expectedMagic[index]) { valid = FALSE; } }
        item->FirmwareType = data[8];
        AsciiToWideTrim(data + 9, 12, item->DevModel, 64);
        item->Version = ReadU32Le(data + 21);
        item->ImageSize = ReadU32Le(data + 25);
        item->ImageCrc32 = ReadU32Le(data + 29);
    }
    if (!valid)
    {
        WCopy(item->Message, L"MagicNumber不是POWEROAK", 128);
        return FALSE;
    }
    if (item->ImageSize == 0) { WCopy(item->Message, L"SizeOfBytes为0", 128); return FALSE; }
    item->Ready = TRUE;
    item->State = 0;
    item->Progress = 0;
    WCopy(item->Message, L"已解析，等待升级", 128);
    return TRUE;
}

static void ResetBleOtaQueue(void)
{
    int index;
    if (gBleOtaRunning) { return; }
    for (index = 0; index < BLE_OTA_MAX_FILES; index++) { memset(&gBleOtaItems[index], 0, sizeof(BLE_OTA_ITEM)); gBleOtaItems[index].Chip = -1; }
    gBleOtaCount = 0;
    gBleOtaSelected = -1;
    gBleOtaListOffset = 0;
    gBleOtaCurrentIndex = 0;
    gBleOtaCurrentPercent = 0;
    gBleOtaPcPercent = 0;
    gBleOtaDevicePercent = 0;
    gBleOtaDistributionDepth = 0;
    gBleOtaDistributionError = 0;
    gBleOtaDistributionSlot = -1;
    gBleOtaGroupValue = 0;
    gBleOtaProcessPercent = 0;
    gBleOtaSuccessPercent = 0;
    gBleOtaSuccessCount = 0;
    gBleOtaFailureCount = 0;
    gBleOtaWaitRemaining = 0;
    WCopy(gBleOtaState, L"IDLE", 48);
    WCopy(gBleOtaMessage, L"添加固件后，请在对应队列行选择芯片平台并读取验证", 192);
    if (gWindow != NULL) { RECT client; GetClientRect(gWindow, &client); LayoutBleOtaControls(&client); }
    if (gBleOtaStatusPath[0]) { DeleteFileW(gBleOtaStatusPath); }
}

static BOOL AddBleOtaPath(LPCWSTR path)
{
    BLE_OTA_ITEM* item;
    if (path == NULL || path[0] == 0 || gBleOtaCount >= BLE_OTA_MAX_FILES) { return FALSE; }
    item = &gBleOtaItems[gBleOtaCount];
    memset(item, 0, sizeof(BLE_OTA_ITEM));
    item->Chip = -1;
    WCopy(item->Path, path, 1024);
    ExtractNameFromPath(path, item->Name, 260);
    WCopy(item->DevModel, L"待验证", 64);
    WCopy(item->Message, L"请选择芯片平台，然后读取验证", 128);
    gBleOtaSelected = gBleOtaCount;
    gBleOtaCount++;
    if (gBleOtaSelected >= gBleOtaListOffset + BLE_OTA_VISIBLE_ROWS) { gBleOtaListOffset = gBleOtaSelected - BLE_OTA_VISIBLE_ROWS + 1; }
    return TRUE;
}

static int FindNextBleOtaUnconfirmed(int startIndex);
static void SelectBleOtaConfirmItem(int itemIndex);

static void AddBleOtaFirmwareFiles(void)
{
    OPENFILENAMEW dialog;
    static wchar_t fileBuffer[32768];
    wchar_t directory[1024];
    wchar_t fullPath[2048];
    wchar_t* cursor;
    int index;
    int added = 0;
    int firstAddedIndex;
    if (gBleOtaRunning) { return; }
    firstAddedIndex = gBleOtaCount;
    for (index = 0; index < (int)(sizeof(dialog) / sizeof(BYTE)); index++) { ((BYTE*)&dialog)[index] = 0; }
    fileBuffer[0] = 0;
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = gWindow;
    dialog.lpstrFilter = L"Firmware Files (*.bin;*.img;*.fw)\0*.bin;*.img;*.fw\0All Files (*.*)\0*.*\0\0";
    dialog.lpstrFile = fileBuffer;
    dialog.nMaxFile = 32768;
    dialog.lpstrTitle = L"添加一个或多个 BLE OTA 固件";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_ALLOWMULTISELECT;
    dialog.lpstrDefExt = L"bin";
    if (!GetOpenFileNameW(&dialog)) { return; }
    cursor = fileBuffer + WLen(fileBuffer) + 1;
    if (*cursor == 0)
    {
        if (AddBleOtaPath(fileBuffer)) { added = 1; }
    }
    else
    {
        WCopy(directory, fileBuffer, 1024);
        while (*cursor != 0 && gBleOtaCount < BLE_OTA_MAX_FILES)
        {
            WCopy(fullPath, directory, 2048);
            if (WLen(fullPath) > 0 && fullPath[WLen(fullPath) - 1] != L'\\') { WAppend(fullPath, L"\\", 2048); }
            WAppend(fullPath, cursor, 2048);
            if (AddBleOtaPath(fullPath)) { added++; }
            cursor += WLen(cursor) + 1;
        }
    }
    if (added > 0)
    {
        SelectBleOtaConfirmItem(firstAddedIndex);
        wsprintfW(gBleOtaMessage, L"已添加 %d 个固件 · 请在对应队列行选择芯片平台并点击读取验证", added);
    }
    else if (gBleOtaCount >= BLE_OTA_MAX_FILES)
    {
        WCopy(gBleOtaMessage, L"固件队列已达到16个上限", 192);
    }
    { RECT client; GetClientRect(gWindow, &client); LayoutBleOtaControls(&client); }
    InvalidateRect(gWindow, NULL, FALSE);
}

static int FindNextBleOtaUnconfirmed(int startIndex)
{
    int offset;
    if (gBleOtaCount <= 0) { return -1; }
    if (startIndex < 0 || startIndex >= gBleOtaCount) { startIndex = 0; }
    for (offset = 0; offset < gBleOtaCount; offset++)
    {
        int index = (startIndex + offset) % gBleOtaCount;
        if (!gBleOtaItems[index].Ready) { return index; }
    }
    return -1;
}

static void SelectBleOtaConfirmItem(int itemIndex)
{
    if (itemIndex < 0 || itemIndex >= gBleOtaCount) { return; }
    gBleOtaSelected = itemIndex;
    if (gBleOtaSelected < gBleOtaListOffset) { gBleOtaListOffset = gBleOtaSelected; }
    else if (gBleOtaSelected >= gBleOtaListOffset + BLE_OTA_VISIBLE_ROWS) { gBleOtaListOffset = gBleOtaSelected - BLE_OTA_VISIBLE_ROWS + 1; }

}

static void SelectBleOtaRowChip(int visibleRow, int chip)
{
    int itemIndex;
    BLE_OTA_ITEM* item;
    if (gBleOtaRunning || visibleRow < 0 || visibleRow >= BLE_OTA_VISIBLE_ROWS || chip < 0 || chip > 4) { return; }
    itemIndex = gBleOtaListOffset + visibleRow;
    if (itemIndex < 0 || itemIndex >= gBleOtaCount) { return; }
    item = &gBleOtaItems[itemIndex];
    gBleOtaSelected = itemIndex;
    if (item->Chip != chip || item->Ready)
    {
        item->Chip = chip;
        item->Ready = FALSE;
        item->State = 0;
        item->Progress = 0;
        WCopy(item->DevModel, L"待验证", 64);
        WCopy(item->Message, L"芯片已选择，点击读取验证", 128);
    }
    wsprintfW(gBleOtaMessage, L"第%d个固件已选择 %s · 请点击该行“读取验证”", itemIndex + 1, CHIP_NAMES[chip]);
}

static void ApplyBleOtaRowChip(int visibleRow)
{
    int itemIndex;
    int chip;
    BLE_OTA_ITEM* item;
    if (gBleOtaRunning || visibleRow < 0 || visibleRow >= BLE_OTA_VISIBLE_ROWS) { return; }
    itemIndex = gBleOtaListOffset + visibleRow;
    if (itemIndex < 0 || itemIndex >= gBleOtaCount) { return; }
    chip = gBleOtaItems[itemIndex].Chip;
    gBleOtaSelected = itemIndex;
    if (chip < 0 || chip > 4)
    {
        wsprintfW(gBleOtaMessage, L"请先为第%d个固件选择芯片平台：%s", itemIndex + 1, gBleOtaItems[itemIndex].Name);
        InvalidateRect(gWindow, NULL, FALSE);
        return;
    }
    item = &gBleOtaItems[itemIndex];
    item->Chip = chip;
    if (ParseBleOtaFirmwareItem(item, chip))
    {
        wsprintfW(gBleOtaMessage, L"第%d个固件读取验证通过 · %s · %s · V%u", itemIndex + 1, CHIP_NAMES[item->Chip], item->DevModel, (UINT)item->Version);
    }
    else
    {
        wsprintfW(gBleOtaMessage, L"第%d个固件验证失败 · %s · %s", itemIndex + 1, item->Name, item->Message);
    }
    InvalidateRect(gWindow, NULL, FALSE);
}


static void RemoveBleOtaSelected(void)
{
    int index;
    if (gBleOtaRunning || gBleOtaSelected < 0 || gBleOtaSelected >= gBleOtaCount) { return; }
    for (index = gBleOtaSelected; index < gBleOtaCount - 1; index++) { gBleOtaItems[index] = gBleOtaItems[index + 1]; }
    if (gBleOtaCount > 0) { memset(&gBleOtaItems[gBleOtaCount - 1], 0, sizeof(BLE_OTA_ITEM)); }
    gBleOtaCount--;
    if (gBleOtaCount == 0) { gBleOtaSelected = -1; }
    else if (gBleOtaSelected >= gBleOtaCount) { gBleOtaSelected = gBleOtaCount - 1; }
    if (gBleOtaListOffset > 0 && gBleOtaListOffset + BLE_OTA_VISIBLE_ROWS > gBleOtaCount) { gBleOtaListOffset = gBleOtaCount > BLE_OTA_VISIBLE_ROWS ? gBleOtaCount - BLE_OTA_VISIBLE_ROWS : 0; }
    WCopy(gBleOtaMessage, L"已从升级队列移除所选固件", 192);
    { RECT client; GetClientRect(gWindow, &client); LayoutBleOtaControls(&client); }
    InvalidateRect(gWindow, NULL, FALSE);
}

static void BleAddressToHex(unsigned long long address, LPWSTR output, int maxCount)
{
    static const wchar_t hex[] = L"0123456789ABCDEF";
    int index;
    if (maxCount < 13) { if (maxCount > 0) { output[0] = 0; } return; }
    for (index = 0; index < 12; index++) { int shift = (11 - index) * 4; output[index] = hex[(int)((address >> shift) & 0x0F)]; }
    output[12] = 0;
}

static BOOL BuildBleOtaManifest(void)
{
    static wchar_t manifest[32768];
    wchar_t line[4096];
    wchar_t addressHex[16];
    wchar_t valueText[32];
    int gapSeconds;
    int timeoutSeconds;
    int channelMode;
    int index;
    int slaveId;
    if (!gBleConnected || !gBleHasSelectedDevice || gBleSelectedAddress == 0) { WCopy(gBleOtaMessage, L"BLE设备尚未连接，无法启动OTA", 192); return FALSE; }
    if (gBleOtaCount <= 0) { WCopy(gBleOtaMessage, L"请先添加至少一个固件", 192); return FALSE; }
    for (index = 0; index < gBleOtaCount; index++)
    {
        if (!gBleOtaItems[index].Ready)
        {
            SelectBleOtaConfirmItem(index);
            wsprintfW(gBleOtaMessage, L"第%d个固件尚未完成芯片选择与读取验证：%s", index + 1, gBleOtaItems[index].Name);
            return FALSE;
        }
    }
    GetWindowTextW(gBleOtaGapEdit, valueText, 32);
    gapSeconds = ParsePositiveInt(valueText, 18, 0, 300);
    GetWindowTextW(gBleOtaTimeoutEdit, valueText, 32);
    timeoutSeconds = ParsePositiveInt(valueText, 30, 5, 300);
    channelMode = 1; /* V1.3.6：实机验证AES自动加密OTA成功，UI移除通道选择并固定加密 */
    slaveId = gBleConfiguredSlaveId;
    BleAddressToHex(gBleSelectedAddress, addressHex, 16);
    manifest[0] = 0;
    wsprintfW(line, L"CONFIG\t%d\t%d\t%d\t%d\t%s\t%d\t0\r\n", gapSeconds, timeoutSeconds, channelMode, slaveId, addressHex, gBleSelectedAddressType);
    WAppend(manifest, line, 32768);
    for (index = 0; index < gBleOtaCount; index++)
    {
        BLE_OTA_ITEM* item = &gBleOtaItems[index];
        wsprintfW(line, L"ITEM\t%d\t%d\t%u\t%u\t%u\t%u\t%s\t", index, item->Chip, (UINT)item->FirmwareType, (UINT)item->Version, (UINT)item->ImageSize, (UINT)item->FileSize, item->Name);
        WAppend(manifest, line, 32768);
        WAppend(manifest, item->Path, 32768);
        WAppend(manifest, L"\r\n", 32768);
    }
    if (!WriteUtf16File(gBleOtaManifestPath, manifest)) { WCopy(gBleOtaMessage, L"无法写入BLE OTA任务清单", 192); return FALSE; }
    return TRUE;
}

static void StartBleOtaQueue(void)
{
    wchar_t command[1400];
    int index;
    if (gBleOtaRunning) { return; }
    if (!EnsureBleBackend() || !BuildBleOtaManifest()) { InvalidateRect(gWindow, NULL, FALSE); return; }
    for (index = 0; index < gBleOtaCount; index++)
    {
        gBleOtaItems[index].State = 0;
        gBleOtaItems[index].Progress = 0;
        WCopy(gBleOtaItems[index].Message, L"等待升级", 128);
    }
    gBleOtaCurrentIndex = 0;
    gBleOtaCurrentPercent = 0;
    gBleOtaPcPercent = 0;
    gBleOtaDevicePercent = 0;
    gBleOtaDistributionDepth = 0;
    gBleOtaDistributionError = 0;
    gBleOtaDistributionSlot = -1;
    gBleOtaGroupValue = 0;
    gBleOtaProcessPercent = 0;
    gBleOtaSuccessPercent = 0;
    gBleOtaSuccessCount = 0;
    gBleOtaFailureCount = 0;
    gBleOtaWaitRemaining = 0;
    WCopy(gBleOtaState, L"STARTING", 48);
    WCopy(gBleOtaMessage, L"正在提交 BLE OTA 队列 · 升级开始后暂停普通查询", 192);
    DeleteFileW(gBleOtaStatusPath);
    WCopy(command, L"OTA_START\t", 1400);
    WAppend(command, gBleOtaManifestPath, 1400);
    if (SendBleCommand(command)) { int rowIndex; gBleOtaRunning = TRUE; for (rowIndex = 0; rowIndex < BLE_OTA_VISIBLE_ROWS; rowIndex++) { if (gBleOtaRowChipCombos[rowIndex]) { EnableWindow(gBleOtaRowChipCombos[rowIndex], FALSE); } } }
    else { WCopy(gBleOtaMessage, L"OTA任务提交失败", 192); }
    InvalidateRect(gWindow, NULL, FALSE);
}

static void StopBleOtaQueue(void)
{
    if (!gBleOtaRunning) { return; }
    SendBleCommand(L"OTA_STOP");
    WCopy(gBleOtaState, L"STOPPING", 48);
    WCopy(gBleOtaMessage, L"正在终止 BLE OTA……", 192);
    InvalidateRect(gWindow, NULL, FALSE);
}

static int SplitWideFields(LPWSTR line, LPWSTR* fields, int maxFields)
{
    int count = 0;
    wchar_t* cursor = line;
    if (line == NULL || maxFields <= 0) { return 0; }
    fields[count++] = cursor;
    while (*cursor != 0 && count < maxFields)
    {
        if (*cursor == L'\t') { *cursor = 0; fields[count++] = cursor + 1; }
        cursor++;
    }
    return count;
}

static void RefreshBleOtaStatus(void)
{
    static wchar_t text[16384];
    wchar_t* cursor;
    wchar_t* line;
    BOOL firstLine = TRUE;
    if (ReadUtf16File(gBleOtaStatusPath, text, 16384) <= 0) { return; }
    cursor = text;
    while (*cursor != 0)
    {
        wchar_t* fields[24];
        int fieldCount;
        line = cursor;
        while (*cursor != 0 && *cursor != L'\r' && *cursor != L'\n') { cursor++; }
        if (*cursor != 0) { *cursor++ = 0; if (*cursor == L'\n') { cursor++; } }
        if (line[0] == 0) { continue; }
        fieldCount = SplitWideFields(line, fields, 24);
        if (firstLine && fieldCount >= 12 && WStartsWithNoCase(fields[0], L"OTA"))
        {
            DWORD sequence = (DWORD)ParseSignedInt(fields[1]);
            if (sequence != gBleOtaStatusSequence)
            {
                gBleOtaStatusSequence = sequence;
                WCopy(gBleOtaState, fields[2], 48);
                gBleOtaCurrentIndex = ParseSignedInt(fields[3]);
                gBleOtaCurrentPercent = ParseSignedInt(fields[5]);
                gBleOtaProcessPercent = ParseSignedInt(fields[6]);
                gBleOtaSuccessPercent = ParseSignedInt(fields[7]);
                gBleOtaSuccessCount = ParseSignedInt(fields[8]);
                gBleOtaFailureCount = ParseSignedInt(fields[9]);
                gBleOtaWaitRemaining = ParseSignedInt(fields[10]);
                WCopy(gBleOtaMessage, fields[11], 192);
                if (fieldCount >= 18)
                {
                    gBleOtaPcPercent = ParseSignedInt(fields[12]);
                    gBleOtaDevicePercent = ParseSignedInt(fields[13]);
                    gBleOtaDistributionDepth = ParseSignedInt(fields[14]);
                    gBleOtaDistributionError = ParseSignedInt(fields[15]);
                    gBleOtaDistributionSlot = ParseSignedInt(fields[16]);
                    gBleOtaGroupValue = ParseSignedInt(fields[17]);
                }
                gBleOtaRunning = WStartsWithNoCase(fields[2], L"RUNNING") || WStartsWithNoCase(fields[2], L"DISTRIBUTING") || WStartsWithNoCase(fields[2], L"WAITING") || WStartsWithNoCase(fields[2], L"STOPPING");
                { int rowIndex; for (rowIndex = 0; rowIndex < BLE_OTA_VISIBLE_ROWS; rowIndex++) { if (gBleOtaRowChipCombos[rowIndex]) { EnableWindow(gBleOtaRowChipCombos[rowIndex], !gBleOtaRunning); } } }
            }
            firstLine = FALSE;
        }
        else if (fieldCount >= 6 && WStartsWithNoCase(fields[0], L"ITEM"))
        {
            int index = ParseSignedInt(fields[1]);
            if (index >= 0 && index < gBleOtaCount)
            {
                gBleOtaItems[index].State = ParseSignedInt(fields[2]);
                gBleOtaItems[index].Progress = ParseSignedInt(fields[3]);
                WCopy(gBleOtaItems[index].Message, fields[4], 128);
            }
        }
        firstLine = FALSE;
    }
}


static void PopulatePorts(void)
{
    wchar_t port[16];
    wchar_t target[512];
    int index;
    int count = 0;
    SendMessageW(gComCombo, CB_RESETCONTENT, 0, 0);
    for (index = 1; index <= 64; index++)
    {
        wsprintfW(port, L"COM%d", index);
        if (QueryDosDeviceW(port, target, 512) != 0)
        {
            SendMessageW(gComCombo, CB_ADDSTRING, 0, (LPARAM)port);
            count++;
        }
    }
    if (count == 0)
    {
        for (index = 1; index <= 16; index++)
        {
            wsprintfW(port, L"COM%d", index);
            SendMessageW(gComCombo, CB_ADDSTRING, 0, (LPARAM)port);
        }
        WCopy(gConnectionText, L"未检测到串口，已显示 COM1-COM16 候选项", 128);
    }
    SendMessageW(gComCombo, CB_SETCURSEL, 0, 0);
}

static void PopulateBauds(void)
{
    static const wchar_t* bauds[] = { L"9600", L"19200", L"38400", L"57600", L"115200", L"230400", L"460800", L"921600", L"1000000", L"1500000", L"2000000" };
    int index;
    for (index = 0; index < 11; index++) { SendMessageW(gBaudCombo, CB_ADDSTRING, 0, (LPARAM)bauds[index]); }
    SendMessageW(gBaudCombo, CB_SETCURSEL, 4, 0);
}


static void PopulateCanChannels(void)
{
    SendMessageW(gCanChannelCombo, CB_RESETCONTENT, 0, 0);
    SendMessageW(gCanChannelCombo, CB_ADDSTRING, 0, (LPARAM)L"CAN0");
    SendMessageW(gCanChannelCombo, CB_ADDSTRING, 0, (LPARAM)L"CAN1");
    SendMessageW(gCanChannelCombo, CB_SETCURSEL, 0, 0);
}

static void PopulateCanBauds(void)
{
    static const wchar_t* values[] = { L"125 Kbps", L"250 Kbps", L"500 Kbps", L"800 Kbps", L"1 Mbps" };
    int index;
    SendMessageW(gCanBaudCombo, CB_RESETCONTENT, 0, 0);
    for (index = 0; index < 5; index++) { SendMessageW(gCanBaudCombo, CB_ADDSTRING, 0, (LPARAM)values[index]); }
    SendMessageW(gCanBaudCombo, CB_SETCURSEL, 1, 0);
}

static void ResetCanReceiveCache(void)
{
    gCanRxBatchCount = 0;
    gCanRxBatchIndex = 0;
    memset(gCanRxBatch, 0, sizeof(gCanRxBatch));
}

/*
 * The legacy 32-bit ControlCAN.dll supplied with the user's CANalyst-II can be
 * unstable when VCI_FindUsbDevice or repeated open/close probing is invoked.
 * Device selection is therefore deterministic: list device indices and let
 * VCI_OpenDevice perform the real connection validation.
 */
static void ScanCanDevices(void)
{
    DWORD index;
    wchar_t text[128];
    gCanDeviceMapCount = 0;
    SendMessageW(gCanDeviceCombo, CB_RESETCONTENT, 0, 0);
    for (index = 0; index < 4u; index++)
    {
        gCanDeviceMap[gCanDeviceMapCount] = index;
        wsprintfW(text, L"CANalyst-II #%u", (UINT)index);
        SendMessageW(gCanDeviceCombo, CB_ADDSTRING, 0, (LPARAM)text);
        gCanDeviceMapCount++;
    }
    SendMessageW(gCanDeviceCombo, CB_SETCURSEL, 0, 0);
    WCopy(gCanConnectionText, LoadControlCanLibrary() ? L"ControlCAN.dll 已加载，等待连接设备" : L"ControlCAN.dll 加载失败", 160);
}

static void DisconnectCan(void)
{
    if (gCanConnected)
    {
        if (gVciResetCan != NULL) { gVciResetCan(VCI_USBCAN2, gCanDeviceIndex, gCanChannelIndex); }
        if (gVciCloseDevice != NULL) { gVciCloseDevice(VCI_USBCAN2, gCanDeviceIndex); }
    }
    gCanConnected = FALSE;
    ResetCanReceiveCache();
    WCopy(gCanConnectionText, L"CANalyst-II 已断开", 160);
    AddLog(L"CANalyst-II 连接已断开");
    InvalidateRect(gWindow, NULL, TRUE);
}

static void ConnectCan(void)
{
    int deviceSelection;
    int channelSelection;
    int baudSelection;
    wchar_t localText[32];
    wchar_t targetText[32];
    BYTE timing0;
    BYTE timing1;
    VCI_INIT_CONFIG config;
    int index;

    if (gCanConnected)
    {
        DisconnectCan();
        return;
    }
    if (!LoadControlCanLibrary())
    {
        AddLog(L"无法加载 ControlCAN.dll");
        MessageBoxW(gWindow, L"未能加载 ControlCAN.dll。本版本已按你提供的 32 位 CANalyst-II 驱动编译，请确认 ControlCAN.dll 与 EXE 在同一目录。", L"CAN 驱动库缺失", MB_OK | MB_ICONERROR);
        InvalidateRect(gWindow, NULL, TRUE);
        return;
    }

    deviceSelection = (int)SendMessageW(gCanDeviceCombo, CB_GETCURSEL, 0, 0);
    if (deviceSelection < 0 || deviceSelection >= gCanDeviceMapCount) { deviceSelection = 0; }
    gCanDeviceIndex = gCanDeviceMap[deviceSelection];
    channelSelection = (int)SendMessageW(gCanChannelCombo, CB_GETCURSEL, 0, 0);
    gCanChannelIndex = channelSelection <= 0 ? 0u : 1u;
    baudSelection = (int)SendMessageW(gCanBaudCombo, CB_GETCURSEL, 0, 0);
    if (baudSelection == 0) { gCanBaudRate = 125000u; }
    else if (baudSelection == 1) { gCanBaudRate = 250000u; }
    else if (baudSelection == 3) { gCanBaudRate = 800000u; }
    else if (baudSelection == 4) { gCanBaudRate = 1000000u; }
    else if (baudSelection == 2) { gCanBaudRate = 500000u; }
    else { gCanBaudRate = 250000u; }
    GetWindowTextW(gCanLocalEdit, localText, 32);
    GetWindowTextW(gCanTargetEdit, targetText, 32);
    gCanLocalAddress = ParseHexByte(localText, 0x10);
    gCanTargetAddress = (gCanMode == 2) ? 0xFF : ParseHexByte(targetText, 0x16);
    if (gCanMode == 1 && gCanLocalAddress == gCanTargetAddress)
    {
        MessageBoxW(gWindow, L"本地地址和目标地址不能相同。", L"地址配置错误", MB_OK | MB_ICONWARNING);
        return;
    }

    if (gVciOpenDevice(VCI_USBCAN2, gCanDeviceIndex, 0) != CAN_STATUS_OK)
    {
        WCopy(gCanConnectionText, L"CANalyst-II 打开失败", 160);
        AddLog(L"VCI_OpenDevice 失败，请检查设备和驱动");
        MessageBoxW(gWindow, L"CANalyst-II 打开失败。请确认设备已连接、驱动正常且未被其他软件占用。", L"CAN 连接失败", MB_OK | MB_ICONERROR);
        InvalidateRect(gWindow, NULL, TRUE);
        return;
    }

    for (index = 0; index < (int)(sizeof(config) / sizeof(BYTE)); index++) { ((BYTE*)&config)[index] = 0; }
    GetCanTiming(gCanBaudRate, &timing0, &timing1);
    config.AccCode = 0x00000000u;
    config.AccMask = 0xFFFFFFFFu;
    config.Filter = 1;
    config.Timing0 = timing0;
    config.Timing1 = timing1;
    config.Mode = 0;
    if (gVciInitCan(VCI_USBCAN2, gCanDeviceIndex, gCanChannelIndex, &config) != CAN_STATUS_OK ||
        gVciStartCan(VCI_USBCAN2, gCanDeviceIndex, gCanChannelIndex) != CAN_STATUS_OK)
    {
        gVciCloseDevice(VCI_USBCAN2, gCanDeviceIndex);
        WCopy(gCanConnectionText, L"CAN 通道初始化失败", 160);
        AddLog(L"VCI_InitCAN 或 VCI_StartCAN 失败");
        MessageBoxW(gWindow, L"CAN 通道初始化失败，请确认通道和波特率。", L"CAN 连接失败", MB_OK | MB_ICONERROR);
        InvalidateRect(gWindow, NULL, TRUE);
        return;
    }
    if (gVciClearBuffer != NULL) { gVciClearBuffer(VCI_USBCAN2, gCanDeviceIndex, gCanChannelIndex); }
    ResetCanReceiveCache();
    gCanConnected = TRUE;
    wsprintfW(gCanConnectionText, L"CANalyst-II #%u · CAN%u · %u Kbps", (UINT)gCanDeviceIndex, (UINT)gCanChannelIndex, (UINT)(gCanBaudRate / 1000u));
    wsprintfW(gUpgradeStatus, L"%s · 等待选择固件", gCanConnectionText);
    AddLog(L"CANalyst-II 连接成功，ControlCAN x86 · 经典 CAN · 29位扩展帧");
    InvalidateRect(gWindow, NULL, TRUE);
}

static void DisconnectSerial(void)
{
    if (gSerialHandle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(gSerialHandle);
        gSerialHandle = INVALID_HANDLE_VALUE;
    }
    gSerialConnected = FALSE;
    WCopy(gConnectionText, L"串口已断开", 128);
    AddLog(L"串口连接已断开");
    InvalidateRect(gWindow, NULL, TRUE);
}

static void ConnectSerial(void)
{
    wchar_t port[32];
    wchar_t devicePath[64];
    wchar_t baudText[32];
    int selection;
    int baud;
    DCB dcb;
    COMMTIMEOUTS timeouts;
    int index;

    if (gSerialConnected)
    {
        DisconnectSerial();
        return;
    }

    selection = (int)SendMessageW(gComCombo, CB_GETCURSEL, 0, 0);
    if (selection < 0)
    {
        MessageBoxW(gWindow, L"请先选择串口。", L"串口连接", MB_OK | MB_ICONWARNING);
        return;
    }
    SendMessageW(gComCombo, CB_GETLBTEXT, (WPARAM)selection, (LPARAM)port);
    selection = (int)SendMessageW(gBaudCombo, CB_GETCURSEL, 0, 0);
    SendMessageW(gBaudCombo, CB_GETLBTEXT, (WPARAM)selection, (LPARAM)baudText);
    baud = ParsePositiveInt(baudText, 115200, 1200, 4000000);
    wsprintfW(devicePath, L"\\\\.\\%s", port);

    gSerialHandle = CreateFileW(devicePath, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (gSerialHandle == INVALID_HANDLE_VALUE)
    {
        wsprintfW(gConnectionText, L"%s 连接失败", port);
        AddLog(L"串口打开失败，请检查端口占用或设备连接");
        MessageBoxW(gWindow, L"串口打开失败。请检查设备是否连接、端口是否被其他软件占用。", L"连接失败", MB_OK | MB_ICONERROR);
    }
    else
    {
        for (index = 0; index < (int)(sizeof(dcb) / sizeof(BYTE)); index++) { ((BYTE*)&dcb)[index] = 0; }
        dcb.DCBlength = sizeof(dcb);
        if (GetCommState(gSerialHandle, &dcb))
        {
            dcb.BaudRate = (DWORD)baud;
            dcb.Flags = 1; /* fBinary */
            dcb.ByteSize = 8;
            dcb.Parity = NOPARITY;
            dcb.StopBits = ONESTOPBIT;
            if (!SetCommState(gSerialHandle, &dcb))
            {
                CloseHandle(gSerialHandle);
                gSerialHandle = INVALID_HANDLE_VALUE;
                MessageBoxW(gWindow, L"串口参数配置失败。", L"连接失败", MB_OK | MB_ICONERROR);
            }
        }
        if (gSerialHandle != INVALID_HANDLE_VALUE)
        {
            SetupComm(gSerialHandle, 65536, 65536);
            timeouts.ReadIntervalTimeout = 20;
            timeouts.ReadTotalTimeoutMultiplier = 0;
            timeouts.ReadTotalTimeoutConstant = 50;
            timeouts.WriteTotalTimeoutMultiplier = 0;
            timeouts.WriteTotalTimeoutConstant = 10000;
            SetCommTimeouts(gSerialHandle, &timeouts);
            PurgeComm(gSerialHandle, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR);
            gSerialConnected = TRUE;
            wsprintfW(gConnectionText, L"%s · %d bps · 连接成功", port, baud);
            wsprintfW(gUpgradeStatus, L"%s，等待选择固件", gConnectionText);
            AddLog(L"串口连接成功，通信参数 8-N-1");
            MessageBoxW(gWindow, L"串口连接成功。", L"连接检测", MB_OK | MB_ICONINFORMATION);
        }
    }
    InvalidateRect(gWindow, NULL, TRUE);
}

static void ConfirmChipSelection(void)
{
    wchar_t summary[640];
    LPCWSTR typeName = L"Unknown";
    if (gSelectedChip < 0 || gSelectedChip > 4) { gSelectedChip = 0; }
    CloseChipDialog();
    if (ParseFirmwareHeader())
    {
        if (gFirmwareType == 1) { typeName = L"ARM"; }
        else if (gFirmwareType == 2) { typeName = L"DSP"; }
        else if (gFirmwareType == 3) { typeName = L"BMS"; }
        else if (gFirmwareType == 4) { typeName = L"IOT"; }
        else if (gFirmwareType == 5) { typeName = L"M1"; }
        else if (gFirmwareType == 6) { typeName = L"PACK"; }
        gFirmwareReady = TRUE;
        wsprintfW(gUpgradeStatus, L"固件头解析成功 · %s · Version %u", gDevModelText, gImageVersion);
        if (gSelectedChip == 2) { AddLog(L"TI C2000 IMAGE_HEADER_T 已按16位Word布局从0x000正确解析"); }
        else { AddLog(L"IMAGE_HEADER_T 已按8位布局从文件0x000正确解析"); }
        wsprintfW(summary,
            L"IMAGE_HEADER_T 解析结果：\r\n\r\n"
            L"MagicNumber：%s\r\n"
            L"FirmwareType：0x%02X（%s）\r\n"
            L"DevModel：%s\r\n"
            L"Version：%u（0x%08X）\r\n"
            L"SizeOfBytes：%u\r\n"
            L"Crc32：0x%08X\r\n"
            L"Time：%s\r\n\r\n"
            L"结构体读取位置：0x000；头区长度：0x%03X；布局：%s。",
            gMagicText, (UINT)gFirmwareType, typeName, gDevModelText, gImageVersion, gImageVersion, gImageSize, gImageCrc32, gTimeText, gHeaderAreaSize, (gSelectedChip == 2) ? L"TI 16位Word" : L"普通8位");
        MessageBoxW(gWindow, summary, L"固件头解析完成", MB_OK | MB_ICONINFORMATION);
    }
    else
    {
        gFirmwareReady = FALSE;
        WCopy(gUpgradeStatus, L"固件头解析失败 · 文件开头未识别到 POWEROAK", 192);
        AddLog(L"固件头解析失败：0x000 处 MagicNumber 不是 POWEROAK");
        MessageBoxW(gWindow, L"无法识别固件头。程序已从文件 0x000 读取 IMAGE_HEADER_T，但 MagicNumber 不是 POWEROAK。", L"解析失败", MB_OK | MB_ICONERROR);
    }
    InvalidateRect(gWindow, NULL, TRUE);
}

static BOOL IsCancelRequested(void)
{
    BOOL cancelled = FALSE;
    if (gCancelEvent != NULL && WaitForSingleObject(gCancelEvent, 0) == WAIT_OBJECT_0) { cancelled = TRUE; }
    return cancelled;
}

static void SetProtocolProgress(int progress, int packet, int totalPackets, int attempt)
{
    AcquireSRWLockExclusive(&gStateLock);
    gUpgradeProgress = progress;
    gCurrentPacket = packet;
    gTotalPackets = totalPackets;
    gCurrentAttempt = attempt;
    ReleaseSRWLockExclusive(&gStateLock);
    PostMessageW(gWindow, WM_APP_UPGRADE_REFRESH, 0, 0);
}

static USHORT Crc16Modbus(const BYTE* data, DWORD length)
{
    DWORD index;
    int bit;
    USHORT crc = 0xFFFF;
    for (index = 0; index < length; index++)
    {
        crc = (USHORT)(crc ^ data[index]);
        for (bit = 0; bit < 8; bit++)
        {
            if ((crc & 0x0001) != 0) { crc = (USHORT)((crc >> 1) ^ 0xA001); }
            else { crc = (USHORT)(crc >> 1); }
        }
    }
    return crc;
}

static USHORT Crc16Xmodem(const BYTE* data, DWORD length)
{
    DWORD index;
    int bit;
    USHORT crc = 0x0000;
    for (index = 0; index < length; index++)
    {
        crc = (USHORT)(crc ^ ((USHORT)data[index] << 8));
        for (bit = 0; bit < 8; bit++)
        {
            if ((crc & 0x8000) != 0) { crc = (USHORT)((crc << 1) ^ 0x1021); }
            else { crc = (USHORT)(crc << 1); }
        }
    }
    return crc;
}


static DWORD Crc32Mpeg2(const BYTE* data, DWORD length)
{
    DWORD crc = 0xFFFFFFFFu;
    DWORD index;
    int bit;
    for (index = 0; index < length; index++)
    {
        crc ^= (DWORD)data[index] << 24;
        for (bit = 0; bit < 8; bit++)
        {
            if ((crc & 0x80000000u) != 0u) { crc = (crc << 1) ^ 0x04C11DB7u; }
            else { crc <<= 1; }
        }
    }
    return crc;
}

static void FormatHexFrame(const BYTE* data, DWORD length, wchar_t* output, int outputCount)
{
    static const wchar_t hex[] = L"0123456789ABCDEF";
    DWORD index;
    int position = 0;
    if (outputCount <= 0) { return; }
    for (index = 0; index < length && position + 3 < outputCount; index++)
    {
        output[position++] = hex[(data[index] >> 4) & 0x0F];
        output[position++] = hex[data[index] & 0x0F];
        if (index + 1 < length) { output[position++] = L' '; }
    }
    output[position] = 0;
}

static BOOL SerialWriteAll(const BYTE* data, DWORD length)
{
    DWORD total = 0;
    DWORD written = 0;
    BOOL success = TRUE;
    while (total < length && success)
    {
        if (IsCancelRequested()) { success = FALSE; }
        else if (!WriteFile(gSerialHandle, data + total, length - total, &written, NULL) || written == 0) { success = FALSE; }
        else { total += written; }
    }
    if (success) { FlushFileBuffers(gSerialHandle); }
    return success;
}

/* Return: byte 0..255, -1 no byte yet, -2 serial error, -3 cancelled. */
static int SerialReadOneByte(void)
{
    BYTE value = 0;
    DWORD bytesRead = 0;
    if (IsCancelRequested()) { return -3; }
    if (!ReadFile(gSerialHandle, &value, 1, &bytesRead, NULL)) { return -2; }
    if (bytesRead == 1) { return (int)value; }
    return -1;
}

/* Return: 1 expected byte, 2 NAK, 0 timeout, -1 cancelled/error. */
static int WaitControlByte(BYTE expected, DWORD timeoutMs, BOOL detectNak)
{
    unsigned long long startTick = GetTickCount64();
    while ((GetTickCount64() - startTick) < timeoutMs)
    {
        int value = SerialReadOneByte();
        if (value == -3 || value == -2) { return -1; }
        if (value >= 0)
        {
            if ((BYTE)value == expected) { return 1; }
            if (detectNak && (BYTE)value == 0x15) { return 2; }
        }
        else { Sleep(2); }
    }
    return 0;
}

/* Validates the standard 8-byte response to Modbus function 0x10. */
static int WaitModbusStartResponse(DWORD timeoutMs)
{
    BYTE window[8];
    int count = 0;
    int index;
    unsigned long long startTick = GetTickCount64();
    while ((GetTickCount64() - startTick) < timeoutMs)
    {
        int value = SerialReadOneByte();
        if (value == -3 || value == -2) { return -1; }
        if (value >= 0)
        {
            if (count < 8) { window[count++] = (BYTE)value; }
            else
            {
                for (index = 0; index < 7; index++) { window[index] = window[index + 1]; }
                window[7] = (BYTE)value;
            }
            if (count == 8)
            {
                USHORT receivedCrc = (USHORT)(window[6] | ((USHORT)window[7] << 8));
                if (window[0] == 0x01 && window[1] == 0x10 && window[2] == 0x02 && window[3] == 0xBC && window[4] == 0x00 && window[5] == 0x06 && Crc16Modbus(window, 6) == receivedCrc) { return 1; }
            }
        }
        else { Sleep(2); }
    }
    return 0;
}

static BOOL LoadFirmwareData(BYTE** dataOut, DWORD* sizeOut)
{
    HANDLE file;
    LARGE_INTEGER size;
    DWORD bytesRead = 0;
    BYTE* data = NULL;
    BOOL success = FALSE;
    *dataOut = NULL;
    *sizeOut = 0;

    file = CreateFileW(gFirmwarePath, GENERIC_READ, 1, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file != INVALID_HANDLE_VALUE)
    {
        if (GetFileSizeEx(file, &size) && size.QuadPart > 0 && size.QuadPart <= 0xFFFFFFFFLL)
        {
            data = (BYTE*)VirtualAlloc(NULL, (unsigned long long)size.QuadPart, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            if (data != NULL && ReadFile(file, data, (DWORD)size.QuadPart, &bytesRead, NULL) && bytesRead == (DWORD)size.QuadPart)
            {
                *dataOut = data;
                *sizeOut = (DWORD)size.QuadPart;
                success = TRUE;
            }
        }
        CloseHandle(file);
    }
    if (!success && data != NULL) { VirtualFree(data, 0, MEM_RELEASE); }
    return success;
}


static BOOL LoadCanTransmitData(BYTE** dataOut, DWORD* transmitSizeOut, DWORD* blockTotalOut)
{
    BYTE* fileData = NULL;
    BYTE* transmitData = NULL;
    DWORD fileSize = 0;
    DWORD blockTotal;
    DWORD transmitSize;
    if (!LoadFirmwareData(&fileData, &fileSize)) { return FALSE; }
    if (gImageSize == 0)
    {
        VirtualFree(fileData, 0, MEM_RELEASE);
        WCopy(gLastError, L"IMAGE_HEADER_T.SizeOfBytes 为 0", 192);
        return FALSE;
    }
    blockTotal = (gImageSize + 1023u) / 1024u;
    if (blockTotal == 0 || blockTotal > 65535u)
    {
        VirtualFree(fileData, 0, MEM_RELEASE);
        WCopy(gLastError, L"固件 1K 数据块数量超过协议范围", 192);
        return FALSE;
    }
    transmitSize = blockTotal * 1024u;
    if (fileSize > transmitSize)
    {
        VirtualFree(fileData, 0, MEM_RELEASE);
        WCopy(gLastError, L"实际 BIN 大小超过 SizeOfBytes 对应的 1K 对齐范围", 192);
        return FALSE;
    }
    transmitData = (BYTE*)VirtualAlloc(NULL, transmitSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (transmitData == NULL)
    {
        VirtualFree(fileData, 0, MEM_RELEASE);
        WCopy(gLastError, L"无法分配 CAN 固件发送缓冲区", 192);
        return FALSE;
    }
    memset(transmitData, 0x1A, transmitSize);
    memcpy(transmitData, fileData, fileSize);
    VirtualFree(fileData, 0, MEM_RELEASE);
    gCanWholeCrc32 = Crc32Mpeg2(transmitData, transmitSize);
    gCanTransmitSize = transmitSize;
    *dataOut = transmitData;
    *transmitSizeOut = transmitSize;
    *blockTotalOut = blockTotal;
    return TRUE;
}

static void InitCanObject(VCI_CAN_OBJ* object, DWORD id, BYTE dataLength)
{
    int index;
    for (index = 0; index < (int)(sizeof(VCI_CAN_OBJ) / sizeof(BYTE)); index++) { ((BYTE*)object)[index] = 0; }
    object->ID = id;
    object->SendType = 1;
    object->RemoteFlag = 0;
    object->ExternFlag = 1;
    object->DataLen = dataLength;
}

static BOOL CanTransmitObjects(VCI_CAN_OBJ* objects, DWORD count)
{
    DWORD total = 0;
    if (!gCanConnected || gVciTransmit == NULL || IsCancelRequested() || objects == NULL || count == 0u) { return FALSE; }

    /*
     * The supplied legacy ControlCAN.dll internally handles CAN objects in native
     * groups of 48. Submit the 171 XMODEM payload frames as 48/48/48/27 with no
     * software delay. This keeps continuous bus transmission while avoiding a
     * large single driver call and improves stability on older CANalyst-II DLLs.
     */
    while (total < count)
    {
        DWORD remain = count - total;
        DWORD batch = remain > CAN_TX_BATCH_SIZE ? CAN_TX_BATCH_SIZE : remain;
        DWORD sent = gVciTransmit(VCI_USBCAN2, gCanDeviceIndex, gCanChannelIndex, objects + total, batch);
        if (sent != batch)
        {
            wchar_t text[192];
            wsprintfW(text, L"CAN TX 失败：请求%u帧，驱动返回%u帧，累计%u/%u", (UINT)batch, (UINT)sent, (UINT)total, (UINT)count);
            AddLog(text);
            return FALSE;
        }
        total += sent;
    }
    return TRUE;
}

static BOOL CanSendFrame(BYTE functionCode, BYTE targetAddress, const BYTE* data, BYTE dataLength)
{
    VCI_CAN_OBJ object;
    BOOL success;
    int index;
    wchar_t text[224];
    wchar_t dataText[64];
    int offset = 0;
    InitCanObject(&object, BuildCanId(functionCode, targetAddress, gCanLocalAddress), dataLength);
    for (index = 0; index < dataLength && index < 8; index++) { object.Data[index] = data[index]; }
    success = CanTransmitObjects(&object, 1);

    dataText[0] = 0;
    for (index = 0; index < dataLength && index < 8; index++) { offset += wsprintfW(dataText + offset, L"%02X ", (UINT)object.Data[index]); }
    wsprintfW(text, L"CAN TX  ID=0x%08X  EXT=1  DLC=%u  TYPE=%u  DATA=%s  %s", (UINT)object.ID, (UINT)object.DataLen, (UINT)object.SendType, dataText, success ? L"OK" : L"FAIL");
    AddLog(text);
    return success;
}

/* Returns 1 when a frame is received, 0 when no frame is available, -1 on cancellation/error. */
static void LogCanReceiveFrame(const VCI_CAN_OBJ* object)
{
    wchar_t text[192];
    wchar_t dataText[64];
    int index;
    int offset = 0;
    dataText[0] = 0;
    for (index = 0; index < object->DataLen && index < 8; index++)
    {
        offset += wsprintfW(dataText + offset, L"%02X ", (UINT)object->Data[index]);
    }
    wsprintfW(text, L"CAN RX  ID=0x%08X  EXT=%u  DLC=%u  DATA=%s", (UINT)object->ID, (UINT)object->ExternFlag, (UINT)object->DataLen, dataText);
    AddLog(text);
}

/*
 * Important ControlCAN compatibility detail:
 * the supplied legacy VCI_Receive implementation returns zero when Len < 3.
 * An earlier receive path passed Len=1, therefore every valid 0x72/0x75/0x77 was left unread.
 * Receive in driver-native batches of 48 and keep unconsumed frames in a software cache.
 */
static int CanReceiveOne(VCI_CAN_OBJ* object, int waitTimeMs)
{
    DWORD received;
    if (object == NULL) { return -1; }
    if (IsCancelRequested()) { return -1; }
    if (!gCanConnected || gVciReceive == NULL) { return -1; }

    if (gCanRxBatchIndex < gCanRxBatchCount)
    {
        memcpy(object, &gCanRxBatch[gCanRxBatchIndex], sizeof(VCI_CAN_OBJ));
        gCanRxBatchIndex++;
        LogCanReceiveFrame(object);
        return 1;
    }

    ResetCanReceiveCache();
    received = gVciReceive(VCI_USBCAN2, gCanDeviceIndex, gCanChannelIndex, gCanRxBatch, CAN_RX_BATCH_SIZE, waitTimeMs);
    if (received == CAN_STATUS_ERR) { return -1; }
    if (received == 0u) { return 0; }
    if (received > CAN_RX_BATCH_SIZE) { received = CAN_RX_BATCH_SIZE; }

    gCanRxBatchCount = received;
    gCanRxBatchIndex = 1u;
    memcpy(object, &gCanRxBatch[0], sizeof(VCI_CAN_OBJ));
    LogCanReceiveFrame(object);
    return 1;
}

static BOOL IsExpectedCanResponse(const VCI_CAN_OBJ* object, BYTE functionCode, BYTE sourceAddress)
{
    if (object->ExternFlag == 0 || object->RemoteFlag != 0 || object->DataLen < 2) { return FALSE; }
    if ((object->ID & 0xFF000000u) != 0x08000000u) { return FALSE; }
    if (CanIdFunction(object->ID) != functionCode) { return FALSE; }
    if (CanIdTarget(object->ID) != gCanLocalAddress) { return FALSE; }
    if (CanIdSource(object->ID) != sourceAddress) { return FALSE; }
    return TRUE;
}

/* Returns 0 success, 1 retry, 2 fatal, 3 timeout, -1 cancelled/driver error. */
static int WaitSingleCanResponse(BYTE functionCode, BYTE sourceAddress, DWORD timeoutMs, BYTE* statusOut, BYTE* errorOut)
{
    unsigned long long start = GetTickCount64();
    VCI_CAN_OBJ object;
    while ((GetTickCount64() - start) < timeoutMs)
    {
        int result = CanReceiveOne(&object, 50);
        if (result < 0) { return -1; }
        if (result > 0)
        {
            if (IsExpectedCanResponse(&object, functionCode, sourceAddress))
            {
                BYTE status = object.Data[0];
                BYTE errorCode = object.Data[1];
                if (statusOut != NULL) { *statusOut = status; }
                if (errorOut != NULL) { *errorOut = errorCode; }
                return EvaluateCanOtaResponse(status, errorCode);
            }
            else if ((object.ID & 0xFF000000u) == 0x08000000u && CanIdFunction(object.ID) == functionCode)
            {
                wchar_t rejectText[192];
                wsprintfW(rejectText, L"忽略 0x%02X：期望目标0x%02X/源0x%02X，实际目标0x%02X/源0x%02X，EXT=%u，DLC=%u", (UINT)functionCode, (UINT)gCanLocalAddress, (UINT)sourceAddress, (UINT)CanIdTarget(object.ID), (UINT)CanIdSource(object.ID), (UINT)object.ExternFlag, (UINT)object.DataLen);
                AddLog(rejectText);
            }
        }
    }
    return 3;
}

static BOOL SleepCancelable(DWORD milliseconds)
{
    DWORD elapsed = 0;
    while (elapsed < milliseconds)
    {
        DWORD step = (milliseconds - elapsed > 100u) ? 100u : (milliseconds - elapsed);
        if (WaitForSingleObject(gCancelEvent, step) == WAIT_OBJECT_0) { return FALSE; }
        elapsed += step;
    }
    return TRUE;
}

static void BuildCanStartData(BYTE data[8], DWORD blockTotal)
{
    data[0] = gFirmwareType;
    data[1] = (BYTE)(gImageVersion & 0xFFu);
    data[2] = (BYTE)((gImageVersion >> 8) & 0xFFu);
    data[3] = (BYTE)((gImageVersion >> 16) & 0xFFu);
    data[4] = (BYTE)((gImageVersion >> 24) & 0xFFu);
    data[5] = (BYTE)(blockTotal & 0xFFu);
    data[6] = (BYTE)((blockTotal >> 8) & 0xFFu);
    data[7] = 0;
}

static BOOL SendCanProgress(BYTE targetAddress, BYTE progress)
{
    BYTE data[8] = { 0,0,0,0,0,0,0,0 };
    data[0] = gFirmwareType;
    data[1] = progress;
    return CanSendFrame(0x78, targetAddress, data, 8);
}

static BOOL SendCanStartAndWaitReady(DWORD blockTotal, BYTE targetAddress)
{
    BYTE data[8];
    int attempt;
    wchar_t text[192];
    BuildCanStartData(data, blockTotal);
    SetProtocolProgress(0, 0, (int)blockTotal, 1);
    SetUpgradeStatus(L"发送 0x70，通知目标设备进入 Boot", L"OTA Ready");
    if (!CanSendFrame(0x70, targetAddress, data, 8)) { return FALSE; }
    AddLog(L"0x70 已发送，等待 3 秒进入 Boot");
    if (!SleepCancelable(3000)) { return FALSE; }

    for (attempt = 1; attempt <= 5; attempt++)
    {
        BYTE status = 0;
        BYTE errorCode = 0;
        int response;
        SetProtocolProgress(0, 0, (int)blockTotal, attempt);
        wsprintfW(text, L"发送 0x71 开始升级，第 %d/5 次", attempt);
        SetUpgradeStatus(text, L"等待 0x72 擦除应答");
        if (!CanSendFrame(0x71, targetAddress, data, 8)) { return FALSE; }
        response = WaitSingleCanResponse(0x72, targetAddress, 8000, &status, &errorCode);
        if (response == 0)
        {
            AddLog(L"收到 0x72 正常应答，FLASH 擦除完成");
            return TRUE;
        }
        if (response == -1) { return FALSE; }
        if (response == 2)
        {
            wsprintfW(text, L"0x72 返回终止：状态 %u，错误 %u（%s）", (UINT)status, (UINT)errorCode, CanErrorText(errorCode));
            SetUpgradeStatus(text, L"启动阶段失败");
            AddLog(text);
            return FALSE;
        }
        if (response == 1) { wsprintfW(text, L"0x72 请求重试：错误 %u（%s）", (UINT)errorCode, CanErrorText(errorCode)); }
        else { wsprintfW(text, L"等待 0x72 超时，第 %d/5 次", attempt); }
        AddLog(text);
    }
    SetUpgradeStatus(L"0x71 连续 5 次未获得有效 0x72 应答", L"启动阶段超时");
    return FALSE;
}

static BOOL BuildAndSendCanDataBlock(const BYTE* block, DWORD sequence, BYTE targetAddress, BYTE tryCount)
{
    VCI_CAN_OBJ startObject;
    VCI_CAN_OBJ* frames = gCanTxFrames;
    USHORT crc = Crc16Xmodem(block, 1024);
    DWORD frameIndex;
    DWORD dataOffset = 0;
    InitCanObject(&startObject, BuildCanId(0x73, targetAddress, gCanLocalAddress), 8);
    startObject.Data[0] = (BYTE)(sequence & 0xFFu);
    startObject.Data[1] = (BYTE)((sequence >> 8) & 0xFFu);
    startObject.Data[2] = (BYTE)(CAN_FRAME_TOTAL_PER_BLOCK & 0xFFu);
    startObject.Data[3] = (BYTE)((CAN_FRAME_TOTAL_PER_BLOCK >> 8) & 0xFFu);
    startObject.Data[4] = (BYTE)(crc & 0xFFu);
    startObject.Data[5] = (BYTE)((crc >> 8) & 0xFFu);
    startObject.Data[6] = tryCount;
    startObject.Data[7] = 0;
    if (!CanTransmitObjects(&startObject, 1)) { return FALSE; }

    for (frameIndex = 0; frameIndex < CAN_FRAME_TOTAL_PER_BLOCK; frameIndex++)
    {
        BYTE payloadLength = frameIndex == 170u ? 4u : 6u;
        BYTE dataLength = frameIndex == 170u ? 6u : 8u;
        DWORD byteIndex;
        InitCanObject(&frames[frameIndex], BuildCanId(0x74, targetAddress, gCanLocalAddress), dataLength);
        frames[frameIndex].Data[0] = (BYTE)(frameIndex & 0xFFu);
        frames[frameIndex].Data[1] = (BYTE)((frameIndex >> 8) & 0xFFu);
        for (byteIndex = 0; byteIndex < payloadLength; byteIndex++)
        {
            frames[frameIndex].Data[2 + byteIndex] = block[dataOffset++];
        }
    }
    return dataOffset == 1024u && CanTransmitObjects(frames, CAN_FRAME_TOTAL_PER_BLOCK);
}

static BOOL SendCanBlockSingle(const BYTE* block, DWORD sequence, DWORD blockTotal)
{
    int attempt;
    wchar_t text[192];
    for (attempt = 1; attempt <= 5; attempt++)
    {
        BYTE status = 0;
        BYTE errorCode = 0;
        BYTE progress = (BYTE)((sequence * 100u) / blockTotal);
        int response;
        SetProtocolProgress((int)progress, (int)(sequence + 1u), (int)blockTotal, attempt);
        if (!SendCanProgress(gCanTargetAddress, progress)) { return FALSE; }
        wsprintfW(text, L"发送 1K 数据块 %u/%u，TryCnt=%d", (UINT)(sequence + 1u), (UINT)blockTotal, attempt);
        SetUpgradeStatus(text, L"0x73 + 0x74 × 171");
        if (!BuildAndSendCanDataBlock(block, sequence, gCanTargetAddress, (BYTE)attempt)) { AddLog(L"CAN 连续帧发送失败，准备重试当前 1K 块"); continue; }
        response = WaitSingleCanResponse(0x75, gCanTargetAddress, 5000, &status, &errorCode);
        if (response == 0)
        {
            SetProtocolProgress((int)(((sequence + 1u) * 100u) / blockTotal), (int)(sequence + 1u), (int)blockTotal, attempt);
            return TRUE;
        }
        if (response == -1) { return FALSE; }
        if (response == 2)
        {
            wsprintfW(text, L"0x75 终止当前升级：错误 %u（%s）", (UINT)errorCode, CanErrorText(errorCode));
            SetUpgradeStatus(text, L"数据块失败");
            AddLog(text);
            return FALSE;
        }
        if (response == 1) { wsprintfW(text, L"0x75 请求重发数据块 %u：错误 %u（%s）", (UINT)(sequence + 1u), (UINT)errorCode, CanErrorText(errorCode)); }
        else { wsprintfW(text, L"数据块 %u 等待 0x75 超时", (UINT)(sequence + 1u)); }
        AddLog(text);
    }
    wsprintfW(text, L"数据块 %u 连续 5 次发送失败", (UINT)(sequence + 1u));
    SetUpgradeStatus(text, L"当前数据块超时");
    AddLog(text);
    return FALSE;
}

static BOOL SendCanEndSingle(void)
{
    BYTE data[8] = { 0,0,0,0,0,0,0,0 };
    int attempt;
    wchar_t text[192];
    data[0] = gFirmwareType;
    data[1] = (BYTE)(gCanWholeCrc32 & 0xFFu);
    data[2] = (BYTE)((gCanWholeCrc32 >> 8) & 0xFFu);
    data[3] = (BYTE)((gCanWholeCrc32 >> 16) & 0xFFu);
    data[4] = (BYTE)((gCanWholeCrc32 >> 24) & 0xFFu);
    for (attempt = 1; attempt <= 5; attempt++)
    {
        BYTE status = 0;
        BYTE errorCode = 0;
        int response;
        SetProtocolProgress(99, gTotalPackets, gTotalPackets, attempt);
        wsprintfW(text, L"发送 0x76 整包 CRC32 0x%08X，第 %d/5 次", gCanWholeCrc32, attempt);
        SetUpgradeStatus(text, L"等待 0x77 CRC32 应答");
        if (!CanSendFrame(0x76, gCanTargetAddress, data, 8)) { return FALSE; }
        response = WaitSingleCanResponse(0x77, gCanTargetAddress, 5000, &status, &errorCode);
        if (response == 0)
        {
            int repeat;
            for (repeat = 0; repeat < 3; repeat++) { if (!SendCanProgress(gCanTargetAddress, 100)) { return FALSE; } }
            SetProtocolProgress(100, gTotalPackets, gTotalPackets, attempt);
            AddLog(L"0x77 CRC32 校验成功，已连续发送 3 次 100% 进度");
            return TRUE;
        }
        if (response == -1) { return FALSE; }
        if (response == 2)
        {
            wsprintfW(text, L"0x77 返回终止：错误 %u（%s）", (UINT)errorCode, CanErrorText(errorCode));
            SetUpgradeStatus(text, L"整包校验失败");
            AddLog(text);
            return FALSE;
        }
        if (response == 1) { wsprintfW(text, L"0x77 请求重发：错误 %u（%s）", (UINT)errorCode, CanErrorText(errorCode)); }
        else { wsprintfW(text, L"等待 0x77 超时，第 %d/5 次", attempt); }
        AddLog(text);
    }
    SetUpgradeStatus(L"0x76 连续 5 次未获得有效 0x77 应答", L"整包校验超时");
    return FALSE;
}

static BOOL ExecuteCanSingleUpgrade(const BYTE* firmware, DWORD transmitSize, DWORD blockTotal)
{
    DWORD sequence;
    wchar_t text[192];
    (void)transmitSize;
    gTotalPackets = (int)blockTotal;
    if (gVciClearBuffer != NULL) { gVciClearBuffer(VCI_USBCAN2, gCanDeviceIndex, gCanChannelIndex); }
    if (!SendCanStartAndWaitReady(blockTotal, gCanTargetAddress)) { return FALSE; }
    for (sequence = 0; sequence < blockTotal; sequence++)
    {
        if (!SendCanBlockSingle(firmware + sequence * 1024u, sequence, blockTotal)) { return FALSE; }
        if (sequence == 0 || sequence + 1u == blockTotal || ((sequence + 1u) % 10u) == 0u)
        {
            wsprintfW(text, L"目标 0x%02X 已确认 1K 数据块 %u/%u", (UINT)gCanTargetAddress, (UINT)(sequence + 1u), (UINT)blockTotal);
            AddLog(text);
        }
    }
    return SendCanEndSingle();
}

static void ResetCanNodeList(void)
{
    memset(gCanNodes, 0, sizeof(gCanNodes));
    gCanNodeCount = 0;
    gCanDiscoveredDeviceCount = 0;
    PostMessageW(gWindow, WM_APP_UPGRADE_REFRESH, 0, 0);
}

static int FindCanNode(BYTE address)
{
    int index;
    for (index = 0; index < gCanNodeCount; index++) { if (gCanNodes[index].Address == address) { return index; } }
    return -1;
}

static int AddCanNode(BYTE address)
{
    int index = FindCanNode(address);
    if (index >= 0) { return index; }
    if (gCanNodeCount >= CAN_MAX_BROADCAST_NODES) { return -1; }
    index = gCanNodeCount;
    gCanNodes[index].Address = address;
    gCanNodes[index].Active = 0;
    gCanNodes[index].Progress = 0;
    gCanNodeCount++;
    gCanDiscoveredDeviceCount = gCanNodeCount;
    PostMessageW(gWindow, WM_APP_UPGRADE_REFRESH, 0, 0);
    return index;
}

static int CountActiveCanNodes(void)
{
    int index;
    int count = 0;
    for (index = 0; index < gCanNodeCount; index++) { if (gCanNodes[index].Active && !gCanNodes[index].Failed) { count++; } }
    return count;
}

static BOOL CollectBroadcastReady(DWORD timeoutMs)
{
    unsigned long long start = GetTickCount64();
    VCI_CAN_OBJ object;
    wchar_t text[192];
    while ((GetTickCount64() - start) < timeoutMs)
    {
        int result = CanReceiveOne(&object, 50);
        if (result < 0) { return FALSE; }
        if (result > 0 && object.ExternFlag && !object.RemoteFlag && object.DataLen >= 2 &&
            (object.ID & 0xFF000000u) == 0x08000000u && CanIdFunction(object.ID) == 0x72 && CanIdTarget(object.ID) == gCanLocalAddress)
        {
            BYTE source = CanIdSource(object.ID);
            int nodeIndex = AddCanNode(source);
            if (nodeIndex >= 0)
            {
                int action = EvaluateCanOtaResponse(object.Data[0], object.Data[1]);
                gCanNodes[nodeIndex].LastStatus = object.Data[0];
                gCanNodes[nodeIndex].LastError = object.Data[1];
                if (action == 0)
                {
                    gCanNodes[nodeIndex].Active = 1;
                    gCanNodes[nodeIndex].Failed = 0;
                    wsprintfW(text, L"发现广播节点 0x%02X，0x72 应答正常", (UINT)source);
                    AddLog(text);
                }
                else
                {
                    gCanNodes[nodeIndex].Active = 0;
                    gCanNodes[nodeIndex].Failed = 1;
                    wsprintfW(text, L"节点 0x%02X 启动失败：状态 %u，错误 %u（%s）", (UINT)source, (UINT)object.Data[0], (UINT)object.Data[1], CanErrorText(object.Data[1]));
                    AddLog(text);
                }
            }
        }
    }
    return CountActiveCanNodes() > 0;
}

static BOOL SendBroadcastStartAndDiscover(DWORD blockTotal)
{
    BYTE data[8];
    int attempt;
    wchar_t text[192];
    BuildCanStartData(data, blockTotal);
    ResetCanNodeList();
    SetProtocolProgress(0, 0, (int)blockTotal, 1);
    SetUpgradeStatus(L"广播发送 0x70，通知设备进入 Boot", L"广播 OTA Ready");
    if (!CanSendFrame(0x70, 0xFF, data, 8)) { return FALSE; }
    if (!SleepCancelable(3000)) { return FALSE; }
    for (attempt = 1; attempt <= 5; attempt++)
    {
        SetProtocolProgress(0, 0, (int)blockTotal, attempt);
        wsprintfW(text, L"广播发送 0x71，第 %d/5 次，等待节点 0x72", attempt);
        SetUpgradeStatus(text, L"广播节点发现");
        if (!CanSendFrame(0x71, 0xFF, data, 8)) { return FALSE; }
        if (CollectBroadcastReady(8000))
        {
            wsprintfW(text, L"广播发现 %d 个可升级节点", CountActiveCanNodes());
            AddLog(text);
            return TRUE;
        }
        AddLog(L"本轮 8 秒内未发现可升级节点，准备重发 0x71");
    }
    SetUpgradeStatus(L"广播模式未发现有效 0x72 应答节点", L"节点发现失败");
    return FALSE;
}

static void FailBroadcastNode(int nodeIndex, BYTE errorCode, LPCWSTR reason)
{
    wchar_t text[192];
    if (nodeIndex < 0 || nodeIndex >= gCanNodeCount) { return; }
    gCanNodes[nodeIndex].Failed = 1;
    gCanNodes[nodeIndex].Active = 0;
    gCanNodes[nodeIndex].LastStatus = 2;
    gCanNodes[nodeIndex].LastError = errorCode;
    wsprintfW(text, L"节点 0x%02X %s：错误 %u（%s）", (UINT)gCanNodes[nodeIndex].Address, reason, (UINT)errorCode, CanErrorText(errorCode));
    AddLog(text);
    PostMessageW(gWindow, WM_APP_UPGRADE_REFRESH, 0, 0);
}

static BOOL RetryBroadcastBlockForNode(const BYTE* block, DWORD sequence, DWORD blockTotal, int nodeIndex, int firstAttempt)
{
    int attempt;
    wchar_t text[192];
    BYTE address;
    if (nodeIndex < 0 || nodeIndex >= gCanNodeCount) { return FALSE; }
    address = gCanNodes[nodeIndex].Address;
    for (attempt = firstAttempt; attempt <= 5; attempt++)
    {
        BYTE status = 0;
        BYTE errorCode = 0;
        BYTE progress = (BYTE)((sequence * 100u) / blockTotal);
        int response;
        if (!SendCanProgress(address, progress)) { return FALSE; }
        wsprintfW(text, L"单播补发节点 0x%02X 数据块 %u/%u，TryCnt=%d", (UINT)address, (UINT)(sequence + 1u), (UINT)blockTotal, attempt);
        SetUpgradeStatus(text, L"广播节点单播补发");
        if (!BuildAndSendCanDataBlock(block, sequence, address, (BYTE)attempt)) { continue; }
        response = WaitSingleCanResponse(0x75, address, 5000, &status, &errorCode);
        gCanNodes[nodeIndex].LastStatus = status;
        gCanNodes[nodeIndex].LastError = errorCode;
        if (response == 0)
        {
            gCanNodes[nodeIndex].Progress = (BYTE)(((sequence + 1u) * 100u) / blockTotal);
            PostMessageW(gWindow, WM_APP_UPGRADE_REFRESH, 0, 0);
            return TRUE;
        }
        if (response == -1) { return FALSE; }
        if (response == 2)
        {
            FailBroadcastNode(nodeIndex, errorCode, L"返回不可恢复错误");
            return FALSE;
        }
        wsprintfW(text, L"节点 0x%02X 第 %d/5 次补发未成功，准备继续重试", (UINT)address, attempt);
        AddLog(text);
    }
    FailBroadcastNode(nodeIndex, 10, L"当前数据块连续重试失败");
    return FALSE;
}

static BOOL CollectBroadcastBlockResponses(const BYTE* block, DWORD sequence, DWORD blockTotal)
{
    BYTE responded[CAN_MAX_BROADCAST_NODES];
    BYTE retryNeeded[CAN_MAX_BROADCAST_NODES];
    unsigned long long start = GetTickCount64();
    VCI_CAN_OBJ object;
    int index;
    wchar_t text[192];
    memset(responded, 0, sizeof(responded));
    memset(retryNeeded, 0, sizeof(retryNeeded));
    while ((GetTickCount64() - start) < 5000u)
    {
        int result = CanReceiveOne(&object, 50);
        if (result < 0) { return FALSE; }
        if (result > 0 && object.ExternFlag && !object.RemoteFlag && object.DataLen >= 2 &&
            (object.ID & 0xFF000000u) == 0x08000000u && CanIdFunction(object.ID) == 0x75 && CanIdTarget(object.ID) == gCanLocalAddress)
        {
            int nodeIndex = FindCanNode(CanIdSource(object.ID));
            if (nodeIndex >= 0 && gCanNodes[nodeIndex].Active && !gCanNodes[nodeIndex].Failed)
            {
                int action = EvaluateCanOtaResponse(object.Data[0], object.Data[1]);
                responded[nodeIndex] = 1;
                gCanNodes[nodeIndex].LastStatus = object.Data[0];
                gCanNodes[nodeIndex].LastError = object.Data[1];
                if (action == 0)
                {
                    gCanNodes[nodeIndex].Progress = (BYTE)(((sequence + 1u) * 100u) / blockTotal);
                }
                else if (action == 1)
                {
                    retryNeeded[nodeIndex] = 1;
                    wsprintfW(text, L"节点 0x%02X 请求重发当前数据块：%s", (UINT)gCanNodes[nodeIndex].Address, CanErrorText(object.Data[1]));
                    AddLog(text);
                }
                else
                {
                    FailBroadcastNode(nodeIndex, object.Data[1], L"数据块应答终止");
                }
                PostMessageW(gWindow, WM_APP_UPGRADE_REFRESH, 0, 0);
            }
        }
        {
            BOOL allResponded = TRUE;
            for (index = 0; index < gCanNodeCount; index++)
            {
                if (gCanNodes[index].Active && !gCanNodes[index].Failed && !responded[index]) { allResponded = FALSE; break; }
            }
            if (allResponded) { break; }
        }
    }
    for (index = 0; index < gCanNodeCount; index++)
    {
        if (gCanNodes[index].Active && !gCanNodes[index].Failed && !responded[index])
        {
            retryNeeded[index] = 1;
            wsprintfW(text, L"节点 0x%02X 等待广播 0x75 超时，切换单播补发", (UINT)gCanNodes[index].Address);
            AddLog(text);
        }
    }
    for (index = 0; index < gCanNodeCount; index++)
    {
        if (gCanNodes[index].Active && !gCanNodes[index].Failed && retryNeeded[index])
        {
            RetryBroadcastBlockForNode(block, sequence, blockTotal, index, 2);
        }
    }
    PostMessageW(gWindow, WM_APP_UPGRADE_REFRESH, 0, 0);
    return CountActiveCanNodes() > 0;
}

static BOOL SendCanBlockBroadcast(const BYTE* block, DWORD sequence, DWORD blockTotal)
{
    BYTE progress = (BYTE)((sequence * 100u) / blockTotal);
    wchar_t text[192];
    SetProtocolProgress((int)progress, (int)(sequence + 1u), (int)blockTotal, 1);
    if (!SendCanProgress(0xFF, progress)) { return FALSE; }
    wsprintfW(text, L"广播发送 1K 数据块 %u/%u，活动节点 %d", (UINT)(sequence + 1u), (UINT)blockTotal, CountActiveCanNodes());
    SetUpgradeStatus(text, L"广播 0x73 + 0x74 × 171");
    if (!BuildAndSendCanDataBlock(block, sequence, 0xFF, 1)) { return FALSE; }
    if (!CollectBroadcastBlockResponses(block, sequence, blockTotal))
    {
        SetUpgradeStatus(L"所有广播节点均已失败或离线", L"广播升级终止");
        return FALSE;
    }
    SetProtocolProgress((int)(((sequence + 1u) * 100u) / blockTotal), (int)(sequence + 1u), (int)blockTotal, 1);
    return TRUE;
}

static BOOL SendCanEndBroadcast(void)
{
    BYTE data[8] = { 0,0,0,0,0,0,0,0 };
    BYTE acked[CAN_MAX_BROADCAST_NODES];
    BYTE retryNeeded[CAN_MAX_BROADCAST_NODES];
    unsigned long long start;
    VCI_CAN_OBJ object;
    int index;
    wchar_t text[192];
    data[0] = gFirmwareType;
    data[1] = (BYTE)(gCanWholeCrc32 & 0xFFu);
    data[2] = (BYTE)((gCanWholeCrc32 >> 8) & 0xFFu);
    data[3] = (BYTE)((gCanWholeCrc32 >> 16) & 0xFFu);
    data[4] = (BYTE)((gCanWholeCrc32 >> 24) & 0xFFu);
    memset(acked, 0, sizeof(acked));
    memset(retryNeeded, 0, sizeof(retryNeeded));
    SetProtocolProgress(99, gTotalPackets, gTotalPackets, 1);
    wsprintfW(text, L"广播发送 0x76 CRC32 0x%08X", gCanWholeCrc32);
    SetUpgradeStatus(text, L"等待各节点 0x77");
    if (!CanSendFrame(0x76, 0xFF, data, 8)) { return FALSE; }
    start = GetTickCount64();
    while ((GetTickCount64() - start) < 5000u)
    {
        int result = CanReceiveOne(&object, 50);
        if (result < 0) { return FALSE; }
        if (result > 0 && object.ExternFlag && !object.RemoteFlag && object.DataLen >= 2 &&
            (object.ID & 0xFF000000u) == 0x08000000u && CanIdFunction(object.ID) == 0x77 && CanIdTarget(object.ID) == gCanLocalAddress)
        {
            int nodeIndex = FindCanNode(CanIdSource(object.ID));
            if (nodeIndex >= 0 && gCanNodes[nodeIndex].Active && !gCanNodes[nodeIndex].Failed)
            {
                int action = EvaluateCanOtaResponse(object.Data[0], object.Data[1]);
                gCanNodes[nodeIndex].LastStatus = object.Data[0];
                gCanNodes[nodeIndex].LastError = object.Data[1];
                if (action == 0)
                {
                    acked[nodeIndex] = 1;
                    gCanNodes[nodeIndex].Completed = 1;
                    gCanNodes[nodeIndex].Progress = 100;
                }
                else if (action == 1)
                {
                    retryNeeded[nodeIndex] = 1;
                }
                else
                {
                    FailBroadcastNode(nodeIndex, object.Data[1], L"整包 CRC32 校验失败");
                }
                PostMessageW(gWindow, WM_APP_UPGRADE_REFRESH, 0, 0);
            }
        }
        {
            BOOL allResponded = TRUE;
            for (index = 0; index < gCanNodeCount; index++)
            {
                if (gCanNodes[index].Active && !gCanNodes[index].Failed && !acked[index] && !retryNeeded[index]) { allResponded = FALSE; break; }
            }
            if (allResponded) { break; }
        }
    }
    for (index = 0; index < gCanNodeCount; index++)
    {
        if (gCanNodes[index].Active && !gCanNodes[index].Failed && !acked[index]) { retryNeeded[index] = 1; }
    }
    for (index = 0; index < gCanNodeCount; index++)
    {
        if (gCanNodes[index].Active && !gCanNodes[index].Failed && retryNeeded[index])
        {
            int attempt;
            BYTE address = gCanNodes[index].Address;
            for (attempt = 2; attempt <= 5; attempt++)
            {
                BYTE status = 0;
                BYTE errorCode = 0;
                int response;
                wsprintfW(text, L"单播补发节点 0x%02X 的 0x76，第 %d/5 次", (UINT)address, attempt);
                SetUpgradeStatus(text, L"广播节点 CRC32 补发");
                if (!CanSendFrame(0x76, address, data, 8)) { continue; }
                response = WaitSingleCanResponse(0x77, address, 5000, &status, &errorCode);
                gCanNodes[index].LastStatus = status;
                gCanNodes[index].LastError = errorCode;
                if (response == 0)
                {
                    acked[index] = 1;
                    gCanNodes[index].Completed = 1;
                    gCanNodes[index].Progress = 100;
                    break;
                }
                if (response == -1) { return FALSE; }
                if (response == 2)
                {
                    FailBroadcastNode(index, errorCode, L"整包 CRC32 返回终止");
                    break;
                }
            }
            if (!acked[index] && gCanNodes[index].Active && !gCanNodes[index].Failed)
            {
                FailBroadcastNode(index, 10, L"连续 5 次未返回 0x77");
            }
        }
    }
    for (index = 0; index < 3; index++) { if (!SendCanProgress(0xFF, 100)) { return FALSE; } }
    SetProtocolProgress(100, gTotalPackets, gTotalPackets, 1);
    PostMessageW(gWindow, WM_APP_UPGRADE_REFRESH, 0, 0);
    {
        int completedCount = 0;
        int failedCount = 0;
        for (index = 0; index < gCanNodeCount; index++)
        {
            if (gCanNodes[index].Completed) { completedCount++; }
            if (gCanNodes[index].Failed) { failedCount++; }
        }
        wsprintfW(text, L"广播升级结束：成功 %d 台，失败 %d 台", completedCount, failedCount);
        AddLog(text);
        return completedCount > 0 && failedCount == 0;
    }
}

static BOOL ExecuteCanBroadcastUpgrade(const BYTE* firmware, DWORD transmitSize, DWORD blockTotal)
{
    DWORD sequence;
    wchar_t text[192];
    (void)transmitSize;
    gTotalPackets = (int)blockTotal;
    if (gVciClearBuffer != NULL) { gVciClearBuffer(VCI_USBCAN2, gCanDeviceIndex, gCanChannelIndex); }
    if (!SendBroadcastStartAndDiscover(blockTotal)) { return FALSE; }
    for (sequence = 0; sequence < blockTotal; sequence++)
    {
        if (!SendCanBlockBroadcast(firmware + sequence * 1024u, sequence, blockTotal)) { return FALSE; }
        if (sequence == 0 || sequence + 1u == blockTotal || ((sequence + 1u) % 10u) == 0u)
        {
            wsprintfW(text, L"广播数据块 %u/%u 完成，剩余活动节点 %d", (UINT)(sequence + 1u), (UINT)blockTotal, CountActiveCanNodes());
            AddLog(text);
        }
    }
    return SendCanEndBroadcast();
}

static DWORD WINAPI CanUpgradeWorker(LPVOID parameter)
{
    BYTE* firmware = NULL;
    DWORD transmitSize = 0;
    DWORD blockTotal = 0;
    int repeat;
    BOOL success = FALSE;
    BOOL cancelled = FALSE;
    wchar_t text[192];
    (void)parameter;

    if (!LoadCanTransmitData(&firmware, &transmitSize, &blockTotal))
    {
        if (gLastError[0] == 0) { WCopy(gLastError, L"无法准备 CAN 固件发送数据", 192); }
        SetUpgradeStatus(gLastError, L"固件准备失败");
        AddLog(gLastError);
        AcquireSRWLockExclusive(&gStateLock); gWorkerResult = -1; gProgressVisualState = -1; ReleaseSRWLockExclusive(&gStateLock);
        PostMessageW(gWindow, WM_APP_UPGRADE_DONE, 0, 0);
        return 0;
    }
    wsprintfW(text, L"发送数据已按 %u 个 1K 块补齐，CRC32-MPEG2=0x%08X", (UINT)blockTotal, gCanWholeCrc32);
    AddLog(text);

    for (repeat = 1; repeat <= gRepeatTotal; repeat++)
    {
        AcquireSRWLockExclusive(&gStateLock);
        gCurrentRepeat = repeat; gWaitingNext = FALSE; gUpgradeProgress = 0; gCurrentPacket = 0; gCurrentAttempt = 0; gProgressVisualState = 0;
        ReleaseSRWLockExclusive(&gStateLock);
        PostMessageW(gWindow, WM_APP_UPGRADE_REFRESH, 0, 0);
        wsprintfW(text, L"开始第 %d/%d 次 %s", repeat, gRepeatTotal, gCanMode == 2 ? L"CAN 广播升级" : L"CAN 单节点升级");
        AddLog(text);
        success = (gCanMode == 2) ? ExecuteCanBroadcastUpgrade(firmware, transmitSize, blockTotal) : ExecuteCanSingleUpgrade(firmware, transmitSize, blockTotal);
        if (IsCancelRequested())
        {
            cancelled = TRUE;
            AcquireSRWLockExclusive(&gStateLock); gWorkerResult = 0; gProgressVisualState = 2; ReleaseSRWLockExclusive(&gStateLock);
            SetUpgradeStatus(L"CAN 升级任务已由用户终止", L"已终止");
            AddLog(L"CAN 升级任务已终止");
            break;
        }
        AcquireSRWLockExclusive(&gStateLock);
        gCompletedCount++;
        if (success) { gSuccessCount++; gRoundResults[repeat - 1] = 1; gUpgradeProgress = 100; gProgressVisualState = 1; }
        else { gFailureCount++; gRoundResults[repeat - 1] = 2; gProgressVisualState = -1; }
        ReleaseSRWLockExclusive(&gStateLock);
        PostMessageW(gWindow, WM_APP_UPGRADE_REFRESH, 0, 0);
        wsprintfW(text, L"第 %d/%d 次 CAN 升级%s", repeat, gRepeatTotal, success ? L"成功" : L"失败，已记录结果");
        AddLog(text);
        SetUpgradeStatus(text, success ? L"单轮 CAN 升级成功" : L"单轮 CAN 升级失败");

        if (repeat < gRepeatTotal)
        {
            int remaining;
            AcquireSRWLockExclusive(&gStateLock); gWaitingNext = TRUE; gWaitRemaining = gWaitSeconds; ReleaseSRWLockExclusive(&gStateLock);
            for (remaining = gWaitSeconds; remaining > 0; remaining--)
            {
                wsprintfW(text, L"第 %d 次已记录，%d 秒后执行下一次", repeat, remaining);
                SetUpgradeStatus(text, L"连续 CAN 升级等待");
                AcquireSRWLockExclusive(&gStateLock); gWaitRemaining = remaining; ReleaseSRWLockExclusive(&gStateLock);
                if (WaitForSingleObject(gCancelEvent, 1000) == WAIT_OBJECT_0) { cancelled = TRUE; break; }
            }
            if (cancelled) { break; }
        }
    }
    if (!cancelled)
    {
        AcquireSRWLockExclusive(&gStateLock); gWorkerResult = gFailureCount == 0 ? 1 : -1; gProgressVisualState = gFailureCount == 0 ? 1 : -1; ReleaseSRWLockExclusive(&gStateLock);
        wsprintfW(text, L"连续 CAN 升级完成：总计 %d，成功 %d，失败 %d", gRepeatTotal, gSuccessCount, gFailureCount);
        SetUpgradeStatus(text, gFailureCount == 0 ? L"全部 CAN 升级成功" : L"测试完成（存在失败）");
        AddLog(text);
    }
    VirtualFree(firmware, 0, MEM_RELEASE);
    PostMessageW(gWindow, WM_APP_UPGRADE_DONE, 0, 0);
    return 0;
}


/*
 * OtaFileSize is a single 16-bit Modbus register. This implementation sends
 * the number of XMODEM 1K blocks, which preserves the full range for normal
 * firmware images and matches the following 1K packet transfer.
 */
static BOOL BuildOtaStartFrame(BYTE frame[21], DWORD firmwareSize)
{
    DWORD otaImageSize = (gImageSize > 0) ? gImageSize : firmwareSize;
    DWORD packetCount = (otaImageSize + 1023UL) / 1024UL;
    USHORT crc;
    BYTE versionByte0 = (BYTE)(gImageVersion & 0xFF);
    BYTE versionByte1 = (BYTE)((gImageVersion >> 8) & 0xFF);
    BYTE versionByte2 = (BYTE)((gImageVersion >> 16) & 0xFF);
    BYTE versionByte3 = (BYTE)((gImageVersion >> 24) & 0xFF);

    if (packetCount == 0 || packetCount > 65535UL) { return FALSE; }

    frame[0] = 0x01;
    frame[1] = 0x10;
    frame[2] = 0x02;
    frame[3] = 0xBC;
    frame[4] = 0x00;
    frame[5] = 0x06;
    frame[6] = 0x0C;
    frame[7] = 0x00;
    frame[8] = 0x01; /* OtaStart = 1 */
    frame[9] = 0x00;
    frame[10] = gFirmwareType;
    frame[11] = versionByte1;
    frame[12] = versionByte0;
    frame[13] = versionByte3;
    frame[14] = versionByte2;
    frame[15] = (BYTE)((packetCount >> 8) & 0xFF);
    frame[16] = (BYTE)(packetCount & 0xFF);
    frame[17] = 0x00;
    frame[18] = 0x00; /* OtaGroup = 0 */

    crc = Crc16Modbus(frame, 19);
    frame[19] = (BYTE)(crc & 0xFF);
    frame[20] = (BYTE)((crc >> 8) & 0xFF);

    AcquireSRWLockExclusive(&gStateLock);
    FormatHexFrame(frame, 21, gStartFrameText, 160);
    ReleaseSRWLockExclusive(&gStateLock);
    return TRUE;
}

static BOOL SendOtaStartAndWaitC(DWORD firmwareSize)
{
    BYTE startFrame[21];
    int attempt;
    int result;
    wchar_t text[192];

    if (!BuildOtaStartFrame(startFrame, firmwareSize))
    {
        SetUpgradeStatus(L"固件超过起始协议可表达范围", L"起始帧失败");
        AddLog(L"OtaFileSize 超过 16 位范围");
        return FALSE;
    }

    PurgeComm(gSerialHandle, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR);
    AddLog(L"开始发送 21 字节 Modbus OTA 起始帧");
    wsprintfW(text, L"TX 起始帧：%s", gStartFrameText);
    AddLog(text);

    for (attempt = 1; attempt <= 5; attempt++)
    {
        if (IsCancelRequested()) { return FALSE; }
        SetProtocolProgress(0, 0, gTotalPackets, attempt);
        wsprintfW(text, L"发送 OTA 起始命令，第 %d/5 次", attempt);
        SetUpgradeStatus(text, L"Modbus 起始握手");
        if (!SerialWriteAll(startFrame, 21))
        {
            SetUpgradeStatus(L"串口发送 OTA 起始帧失败", L"串口写入错误");
            AddLog(L"OTA 起始帧发送失败");
            return FALSE;
        }

        result = WaitModbusStartResponse(5000);
        if (result == 1)
        {
            AddLog(L"收到 Modbus 0x10 正常应答");
            SetUpgradeStatus(L"Modbus 应答成功，等待接收字符 C", L"等待 XMODEM 接收端");
            result = WaitControlByte(0x43, 15000, FALSE);
            if (result == 1)
            {
                AddLog(L"收到 0x43 ('C')，开始 XMODEM-1K 传输");
                return TRUE;
            }
            if (result < 0) { return FALSE; }
            SetUpgradeStatus(L"等待字符 C 超时（15 秒）", L"握手超时");
            AddLog(L"15 秒内未收到字符 C");
            return FALSE;
        }
        if (result < 0) { return FALSE; }
        wsprintfW(text, L"第 %d 次 Modbus 应答超时，准备重发", attempt);
        AddLog(text);
    }

    SetUpgradeStatus(L"Modbus OTA 起始命令连续 5 次无应答", L"起始握手失败");
    return FALSE;
}

static BOOL SendOneXmodemPacket(const BYTE* firmware, DWORD firmwareSize, DWORD packetIndex)
{
    BYTE packet[1029];
    DWORD dataOffset = packetIndex * 1024UL;
    DWORD available = (dataOffset < firmwareSize) ? (firmwareSize - dataOffset) : 0;
    DWORD copyLength = available > 1024UL ? 1024UL : available;
    DWORD index;
    BYTE sequence = (BYTE)((packetIndex + 1UL) & 0xFF);
    USHORT crc;
    int attempt;
    int result;
    wchar_t text[192];

    packet[0] = 0x02;
    packet[1] = sequence;
    packet[2] = (BYTE)(0xFF - sequence);
    for (index = 0; index < 1024UL; index++) { packet[3 + index] = 0x1A; }
    for (index = 0; index < copyLength; index++) { packet[3 + index] = firmware[dataOffset + index]; }
    crc = Crc16Xmodem(packet + 3, 1024);
    packet[1027] = (BYTE)((crc >> 8) & 0xFF);
    packet[1028] = (BYTE)(crc & 0xFF);

    for (attempt = 1; attempt <= 5; attempt++)
    {
        if (IsCancelRequested()) { return FALSE; }
        SetProtocolProgress((int)(packetIndex * 100UL / gTotalPackets), (int)(packetIndex + 1), gTotalPackets, attempt);
        wsprintfW(text, L"发送数据包 %u/%u，第 %d/5 次", (UINT)(packetIndex + 1), (UINT)gTotalPackets, attempt);
        SetUpgradeStatus(text, L"XMODEM-1K 数据传输");

        if (!SerialWriteAll(packet, 1029))
        {
            SetUpgradeStatus(L"串口写入 XMODEM 数据包失败", L"串口写入错误");
            return FALSE;
        }

        result = WaitControlByte(0x06, 5000, TRUE);
        if (result == 1)
        {
            SetProtocolProgress((int)((packetIndex + 1UL) * 100UL / gTotalPackets), (int)(packetIndex + 1), gTotalPackets, 0);
            return TRUE;
        }
        if (result == 2)
        {
            wsprintfW(text, L"数据包 %u 收到 NAK，立即重发", (UINT)(packetIndex + 1));
            AddLog(text);
        }
        else if (result == 0)
        {
            wsprintfW(text, L"数据包 %u 等待 ACK 超时，准备重发", (UINT)(packetIndex + 1));
            AddLog(text);
        }
        else { return FALSE; }
    }

    wsprintfW(text, L"数据包 %u 连续 5 次未获 ACK", (UINT)(packetIndex + 1));
    SetUpgradeStatus(text, L"数据传输失败");
    AddLog(text);
    return FALSE;
}

static BOOL SendEotAndWaitAck(void)
{
    BYTE eot = 0x04;
    int attempt;
    int result;
    wchar_t text[192];

    for (attempt = 1; attempt <= 5; attempt++)
    {
        if (IsCancelRequested()) { return FALSE; }
        wsprintfW(text, L"发送 EOT 结束符，第 %d/5 次", attempt);
        SetUpgradeStatus(text, L"结束传输");
        SetProtocolProgress(100, gTotalPackets, gTotalPackets, attempt);
        if (!SerialWriteAll(&eot, 1)) { return FALSE; }
        result = WaitControlByte(0x06, 5000, TRUE);
        if (result == 1)
        {
            AddLog(L"收到 EOT ACK，本次升级发送完成");
            return TRUE;
        }
        if (result == 2) { AddLog(L"EOT 收到 NAK，立即重发"); }
        else if (result == 0) { AddLog(L"EOT 等待 ACK 超时，准备重发"); }
        else { return FALSE; }
    }
    SetUpgradeStatus(L"EOT 连续 5 次未获 ACK", L"结束传输失败");
    return FALSE;
}

static BOOL ExecuteOneUpgrade(const BYTE* firmware, DWORD firmwareSize)
{
    DWORD packetIndex;
    DWORD logStep;
    wchar_t text[192];

    gTotalPackets = (int)((firmwareSize + 1023UL) / 1024UL);
    SetProtocolProgress(0, 0, gTotalPackets, 0);
    if (!SendOtaStartAndWaitC(firmwareSize)) { return FALSE; }

    logStep = (DWORD)gTotalPackets / 10UL;
    if (logStep == 0) { logStep = 1; }
    for (packetIndex = 0; packetIndex < (DWORD)gTotalPackets; packetIndex++)
    {
        if (!SendOneXmodemPacket(firmware, firmwareSize, packetIndex)) { return FALSE; }
        if (packetIndex == 0 || packetIndex + 1 == (DWORD)gTotalPackets || ((packetIndex + 1) % logStep) == 0)
        {
            wsprintfW(text, L"已确认数据包 %u/%u", (UINT)(packetIndex + 1), (UINT)gTotalPackets);
            AddLog(text);
        }
    }

    return SendEotAndWaitAck();
}

static DWORD WINAPI UpgradeWorker(LPVOID parameter)
{
    BYTE* firmware = NULL;
    DWORD firmwareSize = 0;
    int repeat;
    BOOL success = FALSE;
    BOOL cancelled = FALSE;
    wchar_t text[192];
    (void)parameter;

    if (!LoadFirmwareData(&firmware, &firmwareSize))
    {
        WCopy(gLastError, L"无法完整读取升级固件文件", 192);
        SetUpgradeStatus(gLastError, L"文件读取失败");
        AddLog(gLastError);
        AcquireSRWLockExclusive(&gStateLock);
        gWorkerResult = -1;
        gProgressVisualState = -1;
        ReleaseSRWLockExclusive(&gStateLock);
        PostMessageW(gWindow, WM_APP_UPGRADE_DONE, 0, 0);
        return 0;
    }

    if (((firmwareSize + 1023UL) / 1024UL) > 65535UL)
    {
        WCopy(gLastError, L"固件的 1K 数据包数量超过 65535", 192);
        SetUpgradeStatus(gLastError, L"固件过大");
        AddLog(gLastError);
        VirtualFree(firmware, 0, MEM_RELEASE);
        AcquireSRWLockExclusive(&gStateLock);
        gWorkerResult = -1;
        gProgressVisualState = -1;
        ReleaseSRWLockExclusive(&gStateLock);
        PostMessageW(gWindow, WM_APP_UPGRADE_DONE, 0, 0);
        return 0;
    }

    for (repeat = 1; repeat <= gRepeatTotal; repeat++)
    {
        AcquireSRWLockExclusive(&gStateLock);
        gCurrentRepeat = repeat;
        gWaitingNext = FALSE;
        gUpgradeProgress = 0;
        gCurrentPacket = 0;
        gCurrentAttempt = 0;
        gProgressVisualState = 0;
        ReleaseSRWLockExclusive(&gStateLock);
        PostMessageW(gWindow, WM_APP_UPGRADE_REFRESH, 0, 0);

        wsprintfW(text, L"开始第 %d/%d 次固件升级", repeat, gRepeatTotal);
        AddLog(text);
        success = ExecuteOneUpgrade(firmware, firmwareSize);

        if (IsCancelRequested())
        {
            cancelled = TRUE;
            AcquireSRWLockExclusive(&gStateLock);
            gWorkerResult = 0;
            gProgressVisualState = 2;
            ReleaseSRWLockExclusive(&gStateLock);
            SetUpgradeStatus(L"升级任务已由用户终止", L"已终止");
            AddLog(L"升级任务已终止");
            break;
        }

        AcquireSRWLockExclusive(&gStateLock);
        gCompletedCount++;
        if (success)
        {
            gSuccessCount++;
            gRoundResults[repeat - 1] = 1;
            gUpgradeProgress = 100;
            gProgressVisualState = 1;
        }
        else
        {
            gFailureCount++;
            gRoundResults[repeat - 1] = 2;
            gProgressVisualState = -1;
        }
        ReleaseSRWLockExclusive(&gStateLock);
        PostMessageW(gWindow, WM_APP_UPGRADE_REFRESH, 0, 0);

        if (success)
        {
            wsprintfW(text, L"第 %d/%d 次升级成功", repeat, gRepeatTotal);
            AddLog(text);
            SetUpgradeStatus(text, L"单轮升级成功");
        }
        else
        {
            wsprintfW(text, L"第 %d/%d 次升级失败，已记录结果", repeat, gRepeatTotal);
            AddLog(text);
            SetUpgradeStatus(text, L"单轮升级失败");
        }

        if (repeat < gRepeatTotal)
        {
            int remaining;
            AcquireSRWLockExclusive(&gStateLock);
            gWaitingNext = TRUE;
            gWaitRemaining = gWaitSeconds;
            ReleaseSRWLockExclusive(&gStateLock);

            for (remaining = gWaitSeconds; remaining > 0; remaining--)
            {
                wsprintfW(text, L"第 %d 次已记录，%d 秒后执行下一次", repeat, remaining);
                SetUpgradeStatus(text, L"重复升级等待");
                AcquireSRWLockExclusive(&gStateLock);
                gWaitRemaining = remaining;
                ReleaseSRWLockExclusive(&gStateLock);
                if (WaitForSingleObject(gCancelEvent, 1000) == WAIT_OBJECT_0)
                {
                    cancelled = TRUE;
                    AcquireSRWLockExclusive(&gStateLock);
                    gWorkerResult = 0;
                    gProgressVisualState = 2;
                    ReleaseSRWLockExclusive(&gStateLock);
                    SetUpgradeStatus(L"重复升级等待期间已终止任务", L"已终止");
                    break;
                }
            }
            if (cancelled) { break; }
        }
    }

    if (!cancelled)
    {
        AcquireSRWLockExclusive(&gStateLock);
        gWorkerResult = (gFailureCount == 0) ? 1 : -1;
        gProgressVisualState = (gFailureCount == 0) ? 1 : -1;
        ReleaseSRWLockExclusive(&gStateLock);
        wsprintfW(text, L"连续升级完成：总计 %d，成功 %d，失败 %d", gRepeatTotal, gSuccessCount, gFailureCount);
        SetUpgradeStatus(text, (gFailureCount == 0) ? L"全部升级成功" : L"测试完成（存在失败）");
        AddLog(text);
    }

    VirtualFree(firmware, 0, MEM_RELEASE);
    PostMessageW(gWindow, WM_APP_UPGRADE_DONE, 0, 0);
    return 0;
}

static void SetUpgradeInputsEnabled(BOOL enabled)
{
    EnableWindow(gComCombo, enabled);
    EnableWindow(gBaudCombo, enabled);
    EnableWindow(gRepeatEdit, enabled);
    EnableWindow(gWaitEdit, enabled);
}

static void StopUpgrade(BOOL userStopped)
{
    if (gUpgradeRunning && gCancelEvent != NULL)
    {
        if (userStopped)
        {
            gUserStopRequested = TRUE;
            SetUpgradeStatus(L"正在终止升级，请等待当前串口操作退出", L"正在终止");
            AddLog(L"用户请求终止升级任务");
        }
        SetEvent(gCancelEvent);
        if (gCurrentPage == 2 && gSerialHandle != INVALID_HANDLE_VALUE) { PurgeComm(gSerialHandle, PURGE_RXABORT | PURGE_TXABORT); }
    }
}

static void StartUpgrade(void)
{
    wchar_t repeatText[32];
    wchar_t waitText[32];

    if (gUpgradeRunning) { return; }
    if (!gSerialConnected)
    {
        MessageBoxW(gWindow, L"请先连接串口设备。", L"无法开始", MB_OK | MB_ICONWARNING);
        return;
    }
    if (!gFirmwareReady)
    {
        MessageBoxW(gWindow, L"请先选择并成功解析升级固件。", L"无法开始", MB_OK | MB_ICONWARNING);
        return;
    }

    GetWindowTextW(gRepeatEdit, repeatText, 32);
    GetWindowTextW(gWaitEdit, waitText, 32);
    gRepeatTotal = ParsePositiveInt(repeatText, 1, 1, 9999);
    gWaitSeconds = ParsePositiveInt(waitText, 3, 0, 86400);

    if (gCancelEvent == NULL) { gCancelEvent = CreateEventW(NULL, TRUE, FALSE, NULL); }
    if (gCancelEvent == NULL)
    {
        MessageBoxW(gWindow, L"无法创建升级控制事件。", L"启动失败", MB_OK | MB_ICONERROR);
        return;
    }
    ResetEvent(gCancelEvent);

    AcquireSRWLockExclusive(&gStateLock);
    gUpgradeRunning = TRUE;
    gUserStopRequested = FALSE;
    gWaitingNext = FALSE;
    gUpgradeProgress = 0;
    gCurrentPacket = 0;
    gTotalPackets = (int)((gFirmwareFileSize + 1023ULL) / 1024ULL);
    gCurrentAttempt = 0;
    gCurrentRepeat = 0;
    gCompletedCount = 0;
    gSuccessCount = 0;
    gFailureCount = 0;
    gProgressVisualState = 0;
    memset(gRoundResults, 0, sizeof(gRoundResults));
    gWorkerResult = 0;
    gLastError[0] = 0;
    ReleaseSRWLockExclusive(&gStateLock);

    SetUpgradeInputsEnabled(FALSE);
    SetUpgradeStatus(L"正在准备升级固件", L"任务启动");
    AddLog(L"真实串口升级任务已启动（Modbus + XMODEM-1K）");

    gWorkerThread = CreateThread(NULL, 0, UpgradeWorker, NULL, 0, NULL);
    if (gWorkerThread == NULL)
    {
        AcquireSRWLockExclusive(&gStateLock);
        gUpgradeRunning = FALSE;
        ReleaseSRWLockExclusive(&gStateLock);
        SetUpgradeInputsEnabled(TRUE);
        SetUpgradeStatus(L"无法创建升级工作线程", L"启动失败");
        MessageBoxW(gWindow, L"升级工作线程创建失败。", L"启动失败", MB_OK | MB_ICONERROR);
    }
    InvalidateRect(gWindow, NULL, FALSE);
}



static void SetCanUpgradeInputsEnabled(BOOL enabled)
{
    EnableWindow(gCanDeviceCombo, enabled);
    EnableWindow(gCanChannelCombo, enabled);
    EnableWindow(gCanBaudCombo, enabled);
    EnableWindow(gCanLocalEdit, enabled);
    EnableWindow(gCanTargetEdit, enabled && gCanMode == 1);
    EnableWindow(gCanRepeatEdit, enabled);
    EnableWindow(gCanWaitEdit, enabled);
}

static void StartCanUpgrade(void)
{
    wchar_t repeatText[32];
    wchar_t waitText[32];
    wchar_t localText[32];
    wchar_t targetText[32];
    if (gUpgradeRunning) { return; }
    if (!gCanConnected)
    {
        MessageBoxW(gWindow, L"请先连接 CANalyst-II。", L"无法开始", MB_OK | MB_ICONWARNING);
        return;
    }
    if (!gFirmwareReady)
    {
        MessageBoxW(gWindow, L"请先选择并成功解析升级固件。", L"无法开始", MB_OK | MB_ICONWARNING);
        return;
    }
    GetWindowTextW(gCanLocalEdit, localText, 32);
    GetWindowTextW(gCanTargetEdit, targetText, 32);
    gCanLocalAddress = ParseHexByte(localText, 0x10);
    gCanTargetAddress = gCanMode == 2 ? 0xFF : ParseHexByte(targetText, 0x16);
    if (gCanMode == 1 && gCanLocalAddress == gCanTargetAddress)
    {
        MessageBoxW(gWindow, L"本地地址和目标地址不能相同。", L"地址配置错误", MB_OK | MB_ICONWARNING);
        return;
    }
    GetWindowTextW(gCanRepeatEdit, repeatText, 32);
    GetWindowTextW(gCanWaitEdit, waitText, 32);
    gRepeatTotal = ParsePositiveInt(repeatText, 1, 1, 9999);
    gWaitSeconds = ParsePositiveInt(waitText, 3, 0, 86400);
    if (gCancelEvent == NULL) { gCancelEvent = CreateEventW(NULL, TRUE, FALSE, NULL); }
    if (gCancelEvent == NULL)
    {
        MessageBoxW(gWindow, L"无法创建升级控制事件。", L"启动失败", MB_OK | MB_ICONERROR);
        return;
    }
    ResetEvent(gCancelEvent);
    ResetCanReceiveCache();
    if (gVciClearBuffer != NULL) { gVciClearBuffer(VCI_USBCAN2, gCanDeviceIndex, gCanChannelIndex); }
    AcquireSRWLockExclusive(&gStateLock);
    gUpgradeRunning = TRUE;
    gUserStopRequested = FALSE;
    gWaitingNext = FALSE;
    gUpgradeProgress = 0;
    gCurrentPacket = 0;
    gTotalPackets = (int)((gImageSize + 1023u) / 1024u);
    gCurrentAttempt = 0;
    gCurrentRepeat = 0;
    gCompletedCount = 0;
    gSuccessCount = 0;
    gFailureCount = 0;
    gProgressVisualState = 0;
    memset(gRoundResults, 0, sizeof(gRoundResults));
    gWorkerResult = 0;
    gLastError[0] = 0;
    ReleaseSRWLockExclusive(&gStateLock);
    if (gCanMode == 2) { ResetCanNodeList(); }
    SetCanUpgradeInputsEnabled(FALSE);
    SetUpgradeStatus(L"正在准备 CAN OTA 数据", L"任务启动");
    AddLog(gCanMode == 2 ? L"CAN 广播升级任务已启动" : L"CAN 单节点升级任务已启动");
    gWorkerThread = CreateThread(NULL, 0, CanUpgradeWorker, NULL, 0, NULL);
    if (gWorkerThread == NULL)
    {
        AcquireSRWLockExclusive(&gStateLock); gUpgradeRunning = FALSE; ReleaseSRWLockExclusive(&gStateLock);
        SetCanUpgradeInputsEnabled(TRUE);
        SetUpgradeStatus(L"无法创建 CAN 升级工作线程", L"启动失败");
        MessageBoxW(gWindow, L"CAN 升级工作线程创建失败。", L"启动失败", MB_OK | MB_ICONERROR);
    }
    InvalidateRect(gWindow, NULL, FALSE);
}

static int HitTest(int x, int y)
{
    int index;
    if (gChipDialogOpen)
    {
        if (gChipSelectorOpen)
        {
            for (index = 0; index < 5; index++) { if (PointInRectSimple(&gChipSelectorOptionRects[index], x, y)) { return 171 + index; } }
        }
        if (PointInRectSimple(&gChipSelectorRect, x, y)) { return 170; }
        if (PointInRectSimple(&gChipConfirmRect, x, y)) { return 20; }
        if (PointInRectSimple(&gChipCancelRect, x, y)) { return 21; }
        return 0;
    }
    if (gUiSelectorOpen > 0)
    {
        for (index = 0; index < gUiSelectorVisibleCount; index++) { if (PointInRectSimple(&gUiSelectorOptionRects[index], x, y)) { return 190 + index; } }
        if (PointInRectSimple(&gUiSelectorPopupUpRect, x, y) && gUiSelectorPopupUpRect.bottom > gUiSelectorPopupUpRect.top) { return 208; }
        if (PointInRectSimple(&gUiSelectorPopupDownRect, x, y) && gUiSelectorPopupDownRect.bottom > gUiSelectorPopupDownRect.top) { return 209; }
    }
    if (PointInRectSimple(&gThemeRect, x, y)) { return 30; }
    if (gCurrentPage == 0)
    {
        for (index = 0; index < 3; index++) { if (PointInRectSimple(&gCardRects[index], x, y)) { return index + 1; } }
    }
    else
    {
        if (PointInRectSimple(&gBackRect, x, y)) { return 10; }
        if (gCurrentPage == 1)
        {
            if (gBleConnected && PointInRectSimple(&gBleIntervalSelectorRect, x, y)) { return 185; }
            if (gBleConnected && PointInRectSimple(&gBleVersionUpRect, x, y)) { return 186; }
            if (gBleConnected && PointInRectSimple(&gBleVersionDownRect, x, y)) { return 187; }
            if (PointInRectSimple(&gBleScanRect, x, y)) { return 50; }
            if (PointInRectSimple(&gBleConnectRect, x, y)) { return 51; }
            if (PointInRectSimple(&gBleDisconnectRect, x, y)) { return 52; }
            if (gBleConnected && PointInRectSimple(&gBleAcControlRect, x, y)) { return 53; }
            if (gBleConnected && PointInRectSimple(&gBleDcControlRect, x, y)) { return 54; }
            if (gBleConnected && PointInRectSimple(&gBleRefreshRect, x, y)) { return 55; }
            if (gBleConnected && PointInRectSimple(&gBleSlaveApplyRect, x, y)) { return 56; }
            if (gBleConnected && PointInRectSimple(&gBleModbusSendRect, x, y)) { return 57; }
            if (gBleConnected && PointInRectSimple(&gBleOtaEntryRect, x, y)) { return 58; }
            for (index = 0; index < BLE_VISIBLE_ROWS; index++) { if (PointInRectSimple(&gBleDeviceRows[index], x, y)) { return 60 + index; } }
        }
        if (gCurrentPage == 6)
        {
            if (gBleOtaChipSelectorOpen)
            {
                for (index = 0; index < 5; index++) { if (PointInRectSimple(&gBleOtaChipOptionRects[index], x, y)) { return 160 + index; } }
            }
            for (index = 0; index < BLE_OTA_VISIBLE_ROWS; index++) { if (PointInRectSimple(&gBleOtaChipSelectorRects[index], x, y)) { return 150 + index; } }
            if (PointInRectSimple(&gBleOtaAddRect, x, y)) { return 70; }
            if (PointInRectSimple(&gBleOtaClearRect, x, y)) { return 71; }
            if (PointInRectSimple(&gBleOtaRemoveRect, x, y)) { return 73; }
            if (!gBleOtaRunning && PointInRectSimple(&gBleOtaStartRect, x, y)) { return 74; }
            if (gBleOtaRunning && PointInRectSimple(&gBleOtaStopRect, x, y)) { return 75; }
            if (PointInRectSimple(&gBleOtaUpRect, x, y)) { return 76; }
            if (PointInRectSimple(&gBleOtaDownRect, x, y)) { return 77; }
            for (index = 0; index < BLE_OTA_VISIBLE_ROWS; index++) { if (PointInRectSimple(&gBleOtaVerifyRects[index], x, y)) { return 90 + index; } }
            for (index = 0; index < BLE_OTA_VISIBLE_ROWS; index++) { if (PointInRectSimple(&gBleOtaRows[index], x, y)) { return 80 + index; } }
        }
        if (gCurrentPage == 3)
        {
            for (index = 0; index < 2; index++) { if (PointInRectSimple(&gCanModeRects[index], x, y)) { return 40 + index; } }
        }
        if (gCurrentPage == 2 || gCurrentPage == 4 || gCurrentPage == 5)
        {
            if (gCurrentPage == 2)
            {
                if (PointInRectSimple(&gComSelectorRect, x, y)) { return 180; }
                if (PointInRectSimple(&gBaudSelectorRect, x, y)) { return 181; }
            }
            else
            {
                if (PointInRectSimple(&gCanDeviceSelectorRect, x, y)) { return 182; }
                if (PointInRectSimple(&gCanChannelSelectorRect, x, y)) { return 183; }
                if (PointInRectSimple(&gCanBaudSelectorRect, x, y)) { return 184; }
            }
            if (PointInRectSimple(&gConnectRect, x, y)) { return 11; }
            if (PointInRectSimple(&gFirmwareRect, x, y)) { return 12; }
            if (PointInRectSimple(&gStartRect, x, y)) { return 13; }
            if (PointInRectSimple(&gStopRect, x, y)) { return 14; }
        }
    }
    return 0;
}


static void CreateFonts(void)
{
    gFontLogo = CreateFontW(-46, 0, 0, 0, FW_MEDIUM, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
    gFontMission = CreateFontW(-16, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
    gFontTitle = CreateFontW(-30, 0, 0, 0, FW_MEDIUM, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
    gFontSubtitle = CreateFontW(-20, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    gFontCardTitle = CreateFontW(-23, 0, 0, 0, FW_MEDIUM, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
    gFontBody = CreateFontW(-16, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
    gFontSmall = CreateFontW(-14, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
    gFontTiny = CreateFontW(-12, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
    gFontPercent = CreateFontW(-54, 0, 0, 0, FW_MEDIUM, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

static void CreateControls(HWND hwnd)
{
    int index;
    DWORD comboStyle = WS_CHILD | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS;
    gComCombo = CreateWindowExW(0, L"COMBOBOX", L"", comboStyle, 0,0,0,0, hwnd, (HANDLE)(UINT_PTR)ID_COM_COMBO, gInstance, NULL);
    gBaudCombo = CreateWindowExW(0, L"COMBOBOX", L"", comboStyle, 0,0,0,0, hwnd, (HANDLE)(UINT_PTR)ID_BAUD_COMBO, gInstance, NULL);
    gRepeatEdit = CreateWindowExW(0, L"EDIT", L"1", WS_CHILD | WS_TABSTOP | ES_NUMBER, 0,0,0,0, hwnd, (HANDLE)(UINT_PTR)ID_REPEAT_EDIT, gInstance, NULL);
    gWaitEdit = CreateWindowExW(0, L"EDIT", L"3", WS_CHILD | WS_TABSTOP | ES_NUMBER, 0,0,0,0, hwnd, (HANDLE)(UINT_PTR)ID_WAIT_EDIT, gInstance, NULL);
    gChipCombo = CreateWindowExW(0, L"COMBOBOX", L"", comboStyle, 0,0,0,0, hwnd, (HANDLE)(UINT_PTR)ID_CHIP_COMBO, gInstance, NULL);

    gCanDeviceCombo = CreateWindowExW(0, L"COMBOBOX", L"", comboStyle, 0,0,0,0, hwnd, (HANDLE)(UINT_PTR)ID_CAN_DEVICE_COMBO, gInstance, NULL);
    gCanChannelCombo = CreateWindowExW(0, L"COMBOBOX", L"", comboStyle, 0,0,0,0, hwnd, (HANDLE)(UINT_PTR)ID_CAN_CHANNEL_COMBO, gInstance, NULL);
    gCanBaudCombo = CreateWindowExW(0, L"COMBOBOX", L"", comboStyle, 0,0,0,0, hwnd, (HANDLE)(UINT_PTR)ID_CAN_BAUD_COMBO, gInstance, NULL);
    gCanLocalEdit = CreateWindowExW(0, L"EDIT", L"10", WS_CHILD | WS_TABSTOP, 0,0,0,0, hwnd, (HANDLE)(UINT_PTR)ID_CAN_LOCAL_EDIT, gInstance, NULL);
    gCanTargetEdit = CreateWindowExW(0, L"EDIT", L"16", WS_CHILD | WS_TABSTOP, 0,0,0,0, hwnd, (HANDLE)(UINT_PTR)ID_CAN_TARGET_EDIT, gInstance, NULL);
    gCanRepeatEdit = CreateWindowExW(0, L"EDIT", L"1", WS_CHILD | WS_TABSTOP | ES_NUMBER, 0,0,0,0, hwnd, (HANDLE)(UINT_PTR)ID_CAN_REPEAT_EDIT, gInstance, NULL);
    gCanWaitEdit = CreateWindowExW(0, L"EDIT", L"3", WS_CHILD | WS_TABSTOP | ES_NUMBER, 0,0,0,0, hwnd, (HANDLE)(UINT_PTR)ID_CAN_WAIT_EDIT, gInstance, NULL);
    gBleFilterEdit = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_TABSTOP, 0,0,0,0, hwnd, (HANDLE)(UINT_PTR)ID_BLE_FILTER_EDIT, gInstance, NULL);
    gBleIntervalCombo = CreateWindowExW(0, L"COMBOBOX", L"", comboStyle, 0,0,0,0, hwnd, (HANDLE)(UINT_PTR)ID_BLE_INTERVAL_COMBO, gInstance, NULL);
    gBleSlaveEdit = CreateWindowExW(0, L"EDIT", L"1", WS_CHILD | WS_TABSTOP | ES_NUMBER, 0,0,0,0, hwnd, (HANDLE)(UINT_PTR)ID_BLE_SLAVE_EDIT, gInstance, NULL);
    gBleModbusSlaveEdit = CreateWindowExW(0, L"EDIT", L"1", WS_CHILD | WS_TABSTOP | ES_NUMBER, 0,0,0,0, hwnd, (HANDLE)(UINT_PTR)ID_BLE_MODBUS_SLAVE_EDIT, gInstance, NULL);
    gBleModbusFunctionEdit = CreateWindowExW(0, L"EDIT", L"03", WS_CHILD | WS_TABSTOP, 0,0,0,0, hwnd, (HANDLE)(UINT_PTR)ID_BLE_MODBUS_FUNCTION_EDIT, gInstance, NULL);
    gBleModbusRegisterEdit = CreateWindowExW(0, L"EDIT", L"100", WS_CHILD | WS_TABSTOP, 0,0,0,0, hwnd, (HANDLE)(UINT_PTR)ID_BLE_MODBUS_REGISTER_EDIT, gInstance, NULL);
    gBleModbusValueEdit = CreateWindowExW(0, L"EDIT", L"1", WS_CHILD | WS_TABSTOP, 0,0,0,0, hwnd, (HANDLE)(UINT_PTR)ID_BLE_MODBUS_VALUE_EDIT, gInstance, NULL);
    gBleModbusTimeoutEdit = CreateWindowExW(0, L"EDIT", L"1800", WS_CHILD | WS_TABSTOP | ES_NUMBER, 0,0,0,0, hwnd, (HANDLE)(UINT_PTR)ID_BLE_MODBUS_TIMEOUT_EDIT, gInstance, NULL);
    gBleModbusResultEdit = CreateWindowExW(0, L"EDIT", L"填写参数后点击发送；逻辑RTU按旧版协议加密后直接写入FF02。", WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY, 0,0,0,0, hwnd, (HANDLE)(UINT_PTR)ID_BLE_MODBUS_RESULT_EDIT, gInstance, NULL);
    gBleVersionListEdit = CreateWindowExW(0, L"EDIT", L"等待设备版本信息……", WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY, 0,0,0,0, hwnd, (HANDLE)(UINT_PTR)ID_BLE_VERSION_LIST_EDIT, gInstance, NULL);
    gBleOtaGapEdit = CreateWindowExW(0, L"EDIT", L"18", WS_CHILD | WS_TABSTOP | ES_NUMBER, 0,0,0,0, hwnd, (HANDLE)(UINT_PTR)ID_BLE_OTA_GAP_EDIT, gInstance, NULL);
    gBleOtaTimeoutEdit = CreateWindowExW(0, L"EDIT", L"30", WS_CHILD | WS_TABSTOP | ES_NUMBER, 0,0,0,0, hwnd, (HANDLE)(UINT_PTR)ID_BLE_OTA_TIMEOUT_EDIT, gInstance, NULL);
    gBleOtaChannelCombo = CreateWindowExW(0, L"COMBOBOX", L"", comboStyle, 0,0,0,0, hwnd, (HANDLE)(UINT_PTR)ID_BLE_OTA_CHANNEL_COMBO, gInstance, NULL);
    gBleOtaChipCombo = CreateWindowExW(0, L"COMBOBOX", L"", comboStyle, 0,0,0,0, hwnd, (HANDLE)(UINT_PTR)ID_BLE_OTA_CHIP_COMBO, gInstance, NULL);
    for (index = 0; index < BLE_OTA_VISIBLE_ROWS; index++) { gBleOtaRowChipCombos[index] = CreateWindowExW(0, L"COMBOBOX", L"", comboStyle, 0,0,0,0, hwnd, (HANDLE)(UINT_PTR)(ID_BLE_OTA_ROW_CHIP_BASE + index), gInstance, NULL); }

    SendMessageW(gComCombo, WM_SETFONT, (WPARAM)gFontSmall, TRUE);
    SendMessageW(gBaudCombo, WM_SETFONT, (WPARAM)gFontSmall, TRUE);
    SendMessageW(gRepeatEdit, WM_SETFONT, (WPARAM)gFontSmall, TRUE);
    SendMessageW(gWaitEdit, WM_SETFONT, (WPARAM)gFontSmall, TRUE);
    SendMessageW(gChipCombo, WM_SETFONT, (WPARAM)gFontSmall, TRUE);
    SendMessageW(gCanDeviceCombo, WM_SETFONT, (WPARAM)gFontSmall, TRUE);
    SendMessageW(gCanChannelCombo, WM_SETFONT, (WPARAM)gFontSmall, TRUE);
    SendMessageW(gCanBaudCombo, WM_SETFONT, (WPARAM)gFontSmall, TRUE);
    SendMessageW(gCanLocalEdit, WM_SETFONT, (WPARAM)gFontSmall, TRUE);
    SendMessageW(gCanTargetEdit, WM_SETFONT, (WPARAM)gFontSmall, TRUE);
    SendMessageW(gCanRepeatEdit, WM_SETFONT, (WPARAM)gFontSmall, TRUE);
    SendMessageW(gCanWaitEdit, WM_SETFONT, (WPARAM)gFontSmall, TRUE);
    SendMessageW(gBleFilterEdit, WM_SETFONT, (WPARAM)gFontSmall, TRUE);
    SendMessageW(gBleIntervalCombo, WM_SETFONT, (WPARAM)gFontSmall, TRUE);
    SendMessageW(gBleSlaveEdit, WM_SETFONT, (WPARAM)gFontSmall, TRUE);
    SendMessageW(gBleModbusSlaveEdit, WM_SETFONT, (WPARAM)gFontSmall, TRUE);
    SendMessageW(gBleModbusFunctionEdit, WM_SETFONT, (WPARAM)gFontSmall, TRUE);
    SendMessageW(gBleModbusRegisterEdit, WM_SETFONT, (WPARAM)gFontSmall, TRUE);
    SendMessageW(gBleModbusValueEdit, WM_SETFONT, (WPARAM)gFontSmall, TRUE);
    SendMessageW(gBleModbusTimeoutEdit, WM_SETFONT, (WPARAM)gFontSmall, TRUE);
    SendMessageW(gBleModbusResultEdit, WM_SETFONT, (WPARAM)gFontTiny, TRUE);
    SendMessageW(gBleVersionListEdit, WM_SETFONT, (WPARAM)gFontTiny, TRUE);
    SendMessageW(gBleOtaGapEdit, WM_SETFONT, (WPARAM)gFontSmall, TRUE);
    SendMessageW(gBleOtaTimeoutEdit, WM_SETFONT, (WPARAM)gFontSmall, TRUE);
    SendMessageW(gBleOtaChannelCombo, WM_SETFONT, (WPARAM)gFontSmall, TRUE);
    SendMessageW(gBleOtaChipCombo, WM_SETFONT, (WPARAM)gFontSmall, TRUE);
    for (index = 0; index < BLE_OTA_VISIBLE_ROWS; index++) { SendMessageW(gBleOtaRowChipCombos[index], WM_SETFONT, (WPARAM)gFontSmall, TRUE); }

    SendMessageW(gRepeatEdit, EM_SETLIMITTEXT, 4, 0);
    SendMessageW(gWaitEdit, EM_SETLIMITTEXT, 5, 0);
    SendMessageW(gCanLocalEdit, EM_SETLIMITTEXT, 4, 0);
    SendMessageW(gCanTargetEdit, EM_SETLIMITTEXT, 4, 0);
    SendMessageW(gCanRepeatEdit, EM_SETLIMITTEXT, 4, 0);
    SendMessageW(gCanWaitEdit, EM_SETLIMITTEXT, 5, 0);
    SendMessageW(gBleFilterEdit, EM_SETLIMITTEXT, 80, 0);
    SendMessageW(gBleSlaveEdit, EM_SETLIMITTEXT, 3, 0);
    SendMessageW(gBleModbusSlaveEdit, EM_SETLIMITTEXT, 3, 0);
    SendMessageW(gBleModbusFunctionEdit, EM_SETLIMITTEXT, 4, 0);
    SendMessageW(gBleModbusRegisterEdit, EM_SETLIMITTEXT, 10, 0);
    SendMessageW(gBleModbusValueEdit, EM_SETLIMITTEXT, 10, 0);
    SendMessageW(gBleModbusTimeoutEdit, EM_SETLIMITTEXT, 5, 0);
    SendMessageW(gBleModbusResultEdit, EM_SETLIMITTEXT, 8000, 0);
    SendMessageW(gBleVersionListEdit, EM_SETLIMITTEXT, 1000, 0);
    SendMessageW(gBleOtaGapEdit, EM_SETLIMITTEXT, 3, 0);
    SendMessageW(gBleOtaTimeoutEdit, EM_SETLIMITTEXT, 3, 0);
    UpdateBleVersionListControl();
    for (index = 1; index <= 10; index++)
    {
        wchar_t intervalText[24];
        wsprintfW(intervalText, L"%d 秒", index);
        SendMessageW(gBleIntervalCombo, CB_ADDSTRING, 0, (LPARAM)intervalText);
    }
    SendMessageW(gBleIntervalCombo, CB_SETCURSEL, 1, 0);
    SendMessageW(gBleOtaChannelCombo, CB_ADDSTRING, 0, (LPARAM)L"自动（优先加密，失败回退明文）");
    SendMessageW(gBleOtaChannelCombo, CB_ADDSTRING, 0, (LPARAM)L"强制加密通道");
    SendMessageW(gBleOtaChannelCombo, CB_ADDSTRING, 0, (LPARAM)L"明文 BLE 通道");
    SendMessageW(gBleOtaChannelCombo, CB_SETCURSEL, 1, 0);
    for (index = 0; index < 5; index++)
    {
        SendMessageW(gChipCombo, CB_ADDSTRING, 0, (LPARAM)CHIP_NAMES[index]);
        SendMessageW(gBleOtaChipCombo, CB_ADDSTRING, 0, (LPARAM)CHIP_NAMES[index]);
    }
    for (index = 0; index < BLE_OTA_VISIBLE_ROWS; index++)
    {
        int chipIndex;
        SendMessageW(gBleOtaRowChipCombos[index], CB_ADDSTRING, 0, (LPARAM)L"请选择芯片平台");
        for (chipIndex = 0; chipIndex < 5; chipIndex++) { SendMessageW(gBleOtaRowChipCombos[index], CB_ADDSTRING, 0, (LPARAM)CHIP_NAMES[chipIndex]); }
        SendMessageW(gBleOtaRowChipCombos[index], CB_SETCURSEL, 0, 0);
    }
    SendMessageW(gChipCombo, CB_SETCURSEL, 0, 0);
    SendMessageW(gBleOtaChipCombo, CB_SETCURSEL, 0, 0);
    PopulatePorts();
    PopulateBauds();
    PopulateCanChannels();
    PopulateCanBauds();
    ScanCanDevices();
    HideAllUpgradeControls();
}

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_CREATE:
            CreateFonts();
            CreateControls(hwnd);
            AddLog(L"BLUETTI Device Studio V1.0 正式版已启动 · 串口 + CAN + BLE连接");
            SetTimer(hwnd, 1, 400, NULL);
            return 0;

        case WM_SIZE:
        {
            RECT client;
            GetClientRect(hwnd, &client);
            if (gCurrentPage == 1) { LayoutBleControls(&client); }
            else if (gCurrentPage == 6) { LayoutBleOtaControls(&client); }
            else if (gCurrentPage == 2) { LayoutSerialControls(&client); }
            else if (gCurrentPage == 4 || gCurrentPage == 5) { LayoutCanControls(&client); }
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }

        case WM_MEASUREITEM:
        {
            MEASUREITEMSTRUCT* measure = (MEASUREITEMSTRUCT*)lParam;
            if (measure != NULL && measure->CtlType == ODT_COMBOBOX)
            {
                measure->itemHeight = 28;
                return TRUE;
            }
            break;
        }

        case WM_DRAWITEM:
        {
            DRAWITEMSTRUCT* item = (DRAWITEMSTRUCT*)lParam;
            if (item != NULL && item->CtlType == ODT_COMBOBOX)
            {
                wchar_t value[128];
                RECT textRect = item->rcItem;
                BOOL listSelection = ((item->itemState & ODS_SELECTED) != 0) && ((item->itemState & ODS_COMBOBOXEDIT) == 0);
                COLORREF fill = listSelection ? ThemeAccentSoft() : ThemeInputFill();
                COLORREF textColor = listSelection ? ThemeAccent() : ThemeText();
                HBRUSH brush = CreateSolidBrush(fill);
                value[0] = 0;
                FillRect(item->hDC, &item->rcItem, brush);
                DeleteObject(brush);
                if (item->itemID != 0xFFFFFFFFU)
                {
                    SendMessageW(item->hwndItem, CB_GETLBTEXT, item->itemID, (LPARAM)value);
                }
                textRect.left += 4;
                textRect.right -= 4;
                SetBkMode(item->hDC, TRANSPARENT);
                SetTextColor(item->hDC, textColor);
                SelectObject(item->hDC, gFontSmall);
                DrawTextW(item->hDC, value, -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                return TRUE;
            }
            break;
        }

        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORLISTBOX:
        {
            HDC controlDc = (HDC)wParam;
            SetTextColor(controlDc, ThemeText());
            SetBkColor(controlDc, ThemeInputFill());
            SetDCBrushColor(controlDc, ThemeInputFill());
            return (LRESULT)GetStockObject(DC_BRUSH);
        }

        case WM_COMMAND:
        {
            if (LOWORD(wParam) >= ID_BLE_OTA_ROW_CHIP_BASE && LOWORD(wParam) < ID_BLE_OTA_ROW_CHIP_BASE + BLE_OTA_VISIBLE_ROWS && HIWORD(wParam) == CBN_SELCHANGE)
            {
                int visibleRow = (int)LOWORD(wParam) - ID_BLE_OTA_ROW_CHIP_BASE;
                int itemIndex = gBleOtaListOffset + visibleRow;
                if (!gBleOtaRunning && itemIndex >= 0 && itemIndex < gBleOtaCount)
                {
                    int selection = (int)SendMessageW(gBleOtaRowChipCombos[visibleRow], CB_GETCURSEL, 0, 0);
                    int chip = selection - 1;
                    BLE_OTA_ITEM* item = &gBleOtaItems[itemIndex];
                    gBleOtaSelected = itemIndex;
                    if (chip >= 0 && chip < 5)
                    {
                        if (item->Chip != chip || item->Ready)
                        {
                            item->Chip = chip;
                            item->Ready = FALSE;
                            item->State = 0;
                            item->Progress = 0;
                            WCopy(item->DevModel, L"待验证", 64);
                            WCopy(item->Message, L"芯片已选择，点击读取验证", 128);
                        }
                        wsprintfW(gBleOtaMessage, L"第%d个固件已选择 %s · 请点击该行“读取验证”", itemIndex + 1, CHIP_NAMES[chip]);
                    }
                    else
                    {
                        item->Chip = -1;
                        item->Ready = FALSE;
                        item->State = 0;
                        item->Progress = 0;
                        WCopy(item->DevModel, L"待验证", 64);
                        WCopy(item->Message, L"请选择芯片平台，然后读取验证", 128);
                        wsprintfW(gBleOtaMessage, L"第%d个固件尚未选择芯片平台", itemIndex + 1);
                    }
                    InvalidateRect(hwnd, NULL, FALSE);
                }
                return 0;
            }
            if (LOWORD(wParam) == ID_BLE_INTERVAL_COMBO && HIWORD(wParam) == CBN_SELCHANGE)
            {
                int selection = (int)SendMessageW(gBleIntervalCombo, CB_GETCURSEL, 0, 0);
                wchar_t command[64];
                if (selection >= 0)
                {
                    gBlePollInterval = selection + 1;
                    wsprintfW(command, L"INTERVAL\t%d", gBlePollInterval);
                    SendBleCommand(command);
                    wsprintfW(gBleStatusText, L"Modbus轮询周期已设置为 %d 秒", gBlePollInterval);
                    InvalidateRect(hwnd, NULL, FALSE);
                }
                return 0;
            }
            break;
        }

        case WM_MOUSEMOVE:
        {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            int hit = HitTest(x, y);
            if (!gTrackingMouse)
            {
                TRACKMOUSEEVENT track;
                track.cbSize = sizeof(track);
                track.dwFlags = TME_LEAVE;
                track.hwndTrack = hwnd;
                track.dwHoverTime = 0;
                TrackMouseEvent(&track);
                gTrackingMouse = TRUE;
            }
            if (hit != gHoverItem)
            {
                gHoverItem = hit;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_MOUSELEAVE:
            gTrackingMouse = FALSE;
            gHoverItem = 0;
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;

        case WM_SETCURSOR:
            if (gHoverItem != 0) { SetCursor(LoadCursorW(NULL, IDC_HAND)); return TRUE; }
            break;

        case WM_LBUTTONUP:
        {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            int hit = HitTest(x, y);
            if (gChipDialogOpen)
            {
                if (hit == 170) { gChipSelectorOpen = !gChipSelectorOpen; InvalidateRect(hwnd, NULL, FALSE); }
                else if (hit >= 171 && hit < 176) { gSelectedChip = hit - 171; gChipSelectorOpen = FALSE; InvalidateRect(hwnd, NULL, FALSE); }
                else if (hit == 20) { ConfirmChipSelection(); }
                else if (hit == 21) { CloseChipDialog(); }
                else if (gChipSelectorOpen) { gChipSelectorOpen = FALSE; InvalidateRect(hwnd, NULL, FALSE); }
                return 0;
            }
            /* Unified page-level selectors. A click outside closes the popup but still executes its normal target. */
            if (gUiSelectorOpen > 0)
            {
                if (hit >= 190 && hit < 190 + UI_SELECTOR_MAX_VISIBLE)
                {
                    ApplyUiSelectorSelection(hit - 190);
                    gUiSelectorOpen = 0;
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
                if (hit == 208)
                {
                    gUiSelectorFirstOption -= UI_SELECTOR_MAX_VISIBLE;
                    if (gUiSelectorFirstOption < 0) { gUiSelectorFirstOption = 0; }
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
                if (hit == 209)
                {
                    HWND combo = UiSelectorBackingCombo(gUiSelectorOpen);
                    int count = combo ? (int)SendMessageW(combo, CB_GETCOUNT, 0, 0) : 0;
                    gUiSelectorFirstOption += UI_SELECTOR_MAX_VISIBLE;
                    if (gUiSelectorFirstOption + UI_SELECTOR_MAX_VISIBLE > count) { gUiSelectorFirstOption = count > UI_SELECTOR_MAX_VISIBLE ? count - UI_SELECTOR_MAX_VISIBLE : 0; }
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
                if (hit == UiSelectorHitCode(gUiSelectorOpen))
                {
                    gUiSelectorOpen = 0;
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
                gUiSelectorOpen = 0;
            }
            if (hit >= 180 && hit <= 185)
            {
                OpenUiSelector(hit - 179);
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            if (hit == 186)
            {
                if (gBleVersionScroll > 0) { gBleVersionScroll--; }
                InvalidateRect(hwnd, &gBleVersionViewportRect, FALSE);
                return 0;
            }
            if (hit == 187)
            {
                int lineCount = BleVersionLineCount();
                int visibleRows = (gBleVersionViewportRect.bottom - gBleVersionViewportRect.top - 16) / 20;
                int maxScroll = lineCount - (visibleRows > 0 ? visibleRows : 1);
                if (maxScroll < 0) { maxScroll = 0; }
                if (gBleVersionScroll < maxScroll) { gBleVersionScroll++; }
                InvalidateRect(hwnd, &gBleVersionViewportRect, FALSE);
                return 0;
            }
            if (hit == 30)
            {
                gUiSelectorOpen = 0;
                gDarkMode = !gDarkMode;
                RefreshChildControlTheme();
                InvalidateRect(hwnd, NULL, TRUE);
                return 0;
            }
            if (gCurrentPage == 0 && hit >= 1 && hit <= 3)
            {
                RECT client;
                HideAllUpgradeControls();
                gCurrentPage = hit;
                GetClientRect(hwnd, &client);
                if (gCurrentPage == 1)
                {
                    SetBleControlsVisible(TRUE);
                    LayoutBleControls(&client);
                    StartBleScan();
                    AddLog(L"蓝牙连接页面已进入 · 内置扫描后台启动");
                }
                else if (gCurrentPage == 2)
                {
                    SetSerialControlsVisible(TRUE);
                    LayoutSerialControls(&client);
                    PopulatePorts();
                }
                else if (gCurrentPage == 3)
                {
                    ScanCanDevices();
                }
                InvalidateRect(hwnd, NULL, TRUE);
            }
            else if (gCurrentPage == 3 && (hit == 40 || hit == 41))
            {
                RECT client;
                gCanMode = hit == 40 ? 1 : 2;
                gCurrentPage = gCanMode == 1 ? 4 : 5;
                SetCanControlsVisible(TRUE);
                SetWindowTextW(gCanTargetEdit, gCanMode == 2 ? L"FF" : L"16");
                EnableWindow(gCanTargetEdit, gCanMode == 1);
                GetClientRect(hwnd, &client);
                LayoutCanControls(&client);
                ScanCanDevices();
                WCopy(gUpgradeStatus, gCanMode == 2 ? L"等待连接 CANalyst-II 并选择广播升级固件" : L"等待连接 CANalyst-II 并选择目标固件", 192);
                WCopy(gProtocolStage, L"CAN 协议空闲", 96);
                InvalidateRect(hwnd, NULL, TRUE);
            }
            else if (hit == 10)
            {
                if (gCurrentPage == 6)
                {
                    if (gBleOtaRunning) { MessageBoxW(gWindow, L"请先停止 BLE OTA 任务，再返回设备页面。", L"OTA进行中", MB_OK | MB_ICONWARNING); }
                    else
                    {
                        RECT client;
                        HideAllUpgradeControls();
                        gCurrentPage = 1;
                        GetClientRect(hwnd, &client);
                        SetBleControlsVisible(TRUE);
                        LayoutBleControls(&client);
                        InvalidateRect(hwnd, NULL, TRUE);
                    }
                }
                else if (gUpgradeRunning) { MessageBoxW(gWindow, L"请先终止当前升级任务，再返回。", L"升级进行中", MB_OK | MB_ICONWARNING); }
                else
                {
                    if (gCurrentPage == 1) { StopBleScan(); }
                    HideAllUpgradeControls();
                    if (gCurrentPage == 4 || gCurrentPage == 5)
                    {
                        gCurrentPage = 3;
                        gCanMode = 0;
                    }
                    else { gCurrentPage = 0; }
                    InvalidateRect(hwnd, NULL, TRUE);
                }
            }
            else if (gCurrentPage == 1)
            {
                if (hit == 50)
                {
                    if (gBleScanning) { StopBleScan(); WCopy(gBleStatusText, L"蓝牙状态：扫描已停止", 192); }
                    else { StartBleScan(); }
                    InvalidateRect(hwnd, NULL, FALSE);
                }
                else if (hit == 51) { ConnectSelectedBleDevice(); InvalidateRect(hwnd, NULL, FALSE); }
                else if (hit == 52)
                {
                    RECT client;
                    DisconnectBleDevice();
                    SetBleControlsVisible(TRUE);
                    GetClientRect(hwnd, &client);
                    LayoutBleControls(&client);
                    RefreshParentAfterChildVisibility();
                }
                else if (hit == 53 && gBleDataValid)
                {
                    int slaveId;
                    if (ReadBleSlaveAddress(TRUE, &slaveId))
                    {
                        wchar_t command[64];
                        wsprintfW(command, L"WRITE_AC\t%d\t%d", gBleAcState > 0 ? 0 : 1, slaveId);
                        SendBleCommand(command);
                        WCopy(gBleStatusText, gBleAcState > 0 ? L"正在关闭 AC 输出……" : L"正在开启 AC 输出……", 192);
                    }
                    InvalidateRect(hwnd, NULL, FALSE);
                }
                else if (hit == 54 && gBleDataValid)
                {
                    int slaveId;
                    if (ReadBleSlaveAddress(TRUE, &slaveId))
                    {
                        wchar_t command[64];
                        wsprintfW(command, L"WRITE_DC\t%d\t%d", gBleDcState > 0 ? 0 : 1, slaveId);
                        SendBleCommand(command);
                        WCopy(gBleStatusText, gBleDcState > 0 ? L"正在关闭 DC 输出……" : L"正在开启 DC 输出……", 192);
                    }
                    InvalidateRect(hwnd, NULL, FALSE);
                }
                else if (hit == 55)
                {
                    int slaveId;
                    if (ReadBleSlaveAddress(TRUE, &slaveId))
                    {
                        wchar_t command[64];
                        wsprintfW(command, L"READNOW\t%d", slaveId);
                        SendBleCommand(command);
                        WCopy(gBleDataStatus, L"正在请求最新 Modbus 数据……", 192);
                    }
                    InvalidateRect(hwnd, NULL, FALSE);
                }
                else if (hit == 56)
                {
                    if (ApplyBleSlaveAddress(TRUE)) { WCopy(gBleDataStatus, L"正在使用指定从机地址读取数据……", 192); }
                    InvalidateRect(hwnd, NULL, FALSE);
                }
                else if (hit == 57)
                {
                    SendBleManualModbusRequest();
                    InvalidateRect(hwnd, NULL, FALSE);
                }
                else if (hit == 58 && gBleConnected)
                {
                    RECT client;
                    HideAllUpgradeControls();
                    gCurrentPage = 6;
                    GetClientRect(hwnd, &client);
                    SetBleOtaControlsVisible(TRUE);
                    LayoutBleOtaControls(&client);
                    WCopy(gBleOtaMessage, gBleOtaCount > 0 ? L"OTA队列已保留 · 请确认每个固件均已完成芯片选择与读取验证" : L"添加固件后，在对应队列行选择芯片平台并读取验证", 192);
                    InvalidateRect(hwnd, NULL, TRUE);
                }
                else if (hit >= 60 && hit < 60 + BLE_VISIBLE_ROWS)
                {
                    int row = hit - 60;
                    if (row < gBleFilteredCount) { SelectBleDeviceByIndex(gBleFilteredIndices[row]); }
                    InvalidateRect(hwnd, NULL, FALSE);
                }
            }
            else if (gCurrentPage == 6)
            {
                if (gBleOtaChipSelectorOpen)
                {
                    if (hit >= 160 && hit < 165)
                    {
                        SelectBleOtaRowChip(gBleOtaChipSelectorRow, hit - 160);
                        gBleOtaChipSelectorOpen = FALSE;
                        gBleOtaChipSelectorRow = -1;
                        InvalidateRect(hwnd, NULL, FALSE);
                    }
                    else if (hit == 150 + gBleOtaChipSelectorRow)
                    {
                        gBleOtaChipSelectorOpen = FALSE;
                        gBleOtaChipSelectorRow = -1;
                        InvalidateRect(hwnd, NULL, FALSE);
                    }
                    else
                    {
                        gBleOtaChipSelectorOpen = FALSE;
                        gBleOtaChipSelectorRow = -1;
                        InvalidateRect(hwnd, NULL, FALSE);
                    }
                }
                else if (hit >= 150 && hit < 150 + BLE_OTA_VISIBLE_ROWS && !gBleOtaRunning)
                {
                    int row = hit - 150;
                    int itemIndex = gBleOtaListOffset + row;
                    if (itemIndex >= 0 && itemIndex < gBleOtaCount)
                    {
                        gBleOtaSelected = itemIndex;
                        gBleOtaChipSelectorRow = row;
                        gBleOtaChipSelectorOpen = TRUE;
                        InvalidateRect(hwnd, NULL, FALSE);
                    }
                }
                else if (hit == 70) { AddBleOtaFirmwareFiles(); }
                else if (hit == 71) { ResetBleOtaQueue(); InvalidateRect(hwnd, NULL, FALSE); }
                else if (hit == 73) { RemoveBleOtaSelected(); }
                else if (hit == 74) { StartBleOtaQueue(); }
                else if (hit == 75) { StopBleOtaQueue(); }
                else if (hit == 76) { if (gBleOtaListOffset > 0) { RECT client; gBleOtaListOffset--; GetClientRect(hwnd, &client); LayoutBleOtaControls(&client); InvalidateRect(hwnd, NULL, FALSE); } }
                else if (hit == 77) { if (gBleOtaListOffset + BLE_OTA_VISIBLE_ROWS < gBleOtaCount) { RECT client; gBleOtaListOffset++; GetClientRect(hwnd, &client); LayoutBleOtaControls(&client); InvalidateRect(hwnd, NULL, FALSE); } }
                else if (hit >= 90 && hit < 90 + BLE_OTA_VISIBLE_ROWS) { ApplyBleOtaRowChip(hit - 90); }
                else if (hit >= 80 && hit < 80 + BLE_OTA_VISIBLE_ROWS)
                {
                    int row = hit - 80;
                    int itemIndex = gBleOtaListOffset + row;
                    if (itemIndex < gBleOtaCount)
                    {
                        gBleOtaSelected = itemIndex;
                        InvalidateRect(hwnd, NULL, FALSE);
                    }
                }
            }
            else if (gCurrentPage == 2)
            {
                if (hit == 11 && !gUpgradeRunning) { ConnectSerial(); }
                else if (hit == 12 && !gUpgradeRunning) { SelectFirmware(); }
                else if (hit == 13 && !gUpgradeRunning) { StartUpgrade(); }
                else if (hit == 14 && gUpgradeRunning) { StopUpgrade(TRUE); }
            }
            else if (gCurrentPage == 4 || gCurrentPage == 5)
            {
                if (hit == 11 && !gUpgradeRunning) { ConnectCan(); }
                else if (hit == 12 && !gUpgradeRunning) { SelectFirmware(); }
                else if (hit == 13 && !gUpgradeRunning) { StartCanUpgrade(); }
                else if (hit == 14 && gUpgradeRunning) { StopUpgrade(TRUE); }
            }
            return 0;
        }

        case WM_TIMER:
            if (wParam == 1)
            {
                BOOL wasConnected = gBleConnected;
                if (gBleBackendStarted)
                {
                    RefreshBleDevices();
                    RefreshBleStatus();
                    RefreshBleData();
                    RefreshBleModbusResult();
                    RefreshBleOtaStatus();
                    if (gBleConnected && !wasConnected)
                    {
                        RECT client;
                        GetClientRect(hwnd, &client);
                        if (gCurrentPage == 6)
                        {
                            SetBleControlsVisible(FALSE);
                            SetBleOtaControlsVisible(TRUE);
                            LayoutBleOtaControls(&client);
                        }
                        else
                        {
                            SetBleControlsVisible(TRUE);
                            LayoutBleControls(&client);
                            ApplyBleSlaveAddress(FALSE);
                            AddLog(L"蓝牙连接成功，已进入 Modbus 产品监控界面");
                        }
                    }
                    else if (!gBleConnected && wasConnected && gCurrentPage == 1)
                    {
                        RECT client;
                        /* Explicitly hide dashboard-only EDIT controls after a disconnect.
                         * This fixes the stray '1 / 03 / 100 / 1800' text and white scrollbars
                         * previously left over on the scan page. */
                        gUiSelectorOpen = 0;
                        SetBleControlsVisible(TRUE);
                        GetClientRect(hwnd, &client);
                        LayoutBleControls(&client);
                        RefreshParentAfterChildVisibility();
                    }
                    if (gCurrentPage == 1 || gCurrentPage == 6) { InvalidateRect(hwnd, NULL, FALSE); }
                }
            }
            return 0;

        case WM_APP_UPGRADE_REFRESH:
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;

        case WM_APP_UPGRADE_DONE:
        {
            int result;
            AcquireSRWLockExclusive(&gStateLock);
            gUpgradeRunning = FALSE;
            gWaitingNext = FALSE;
            result = gWorkerResult;
            ReleaseSRWLockExclusive(&gStateLock);
            if (gCurrentPage == 2) { SetUpgradeInputsEnabled(TRUE); }
            else if (gCurrentPage == 4 || gCurrentPage == 5) { SetCanUpgradeInputsEnabled(TRUE); }
            if (gWorkerThread != NULL)
            {
                CloseHandle(gWorkerThread);
                gWorkerThread = NULL;
            }
            InvalidateRect(hwnd, NULL, FALSE);
            /* Completion is reported in the dashboard. No modal dialog interrupts batch tests. */
            (void)result;
            return 0;
        }

        case WM_PAINT:
        {
            PAINTSTRUCT paint;
            RECT client;
            HDC dc = BeginPaint(hwnd, &paint);
            HDC memoryDc;
            HGDIOBJ bitmap;
            HGDIOBJ oldBitmap;
            GetClientRect(hwnd, &client);

            /* Full double buffering removes the visible flicker/stutter of the first prototype. */
            memoryDc = CreateCompatibleDC(dc);
            bitmap = CreateCompatibleBitmap(dc, client.right, client.bottom);
            oldBitmap = SelectObject(memoryDc, bitmap);

            DrawBackground(memoryDc, &client);
            DrawTopBar(memoryDc, &client);
            if (gCurrentPage == 0) { DrawHomePage(memoryDc, &client); }
            else if (gCurrentPage == 1) { DrawBlePage(memoryDc, &client); }
            else if (gCurrentPage == 2) { DrawSerialPage(memoryDc, &client); }
            else if (gCurrentPage == 3) { DrawCanModePage(memoryDc, &client); }
            else if (gCurrentPage == 6) { DrawBleOtaPage(memoryDc, &client); }
            else { DrawCanUpgradePage(memoryDc, &client); }
            DrawFooter(memoryDc, &client);
            if (!gChipDialogOpen) { DrawUiSelectorPopup(memoryDc, &client); }
            if (gChipDialogOpen) { DrawChipDialog(memoryDc, &client); }

            BitBlt(dc, 0, 0, client.right, client.bottom, memoryDc, 0, 0, SRCCOPY);
            SelectObject(memoryDc, oldBitmap);
            DeleteObject(bitmap);
            DeleteDC(memoryDc);
            EndPaint(hwnd, &paint);
            return 0;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_DESTROY:
            KillTimer(hwnd, 1);
            if (gBleBackendStarted)
            {
                SendBleCommand(L"EXIT");
                if (gBleBackendProcess != NULL) { WaitForSingleObject(gBleBackendProcess, 1500); CloseHandle(gBleBackendProcess); gBleBackendProcess = NULL; }
                DeleteFileW(gBleCommandPath); DeleteFileW(gBleDevicePath); DeleteFileW(gBleStatusPath); DeleteFileW(gBleDataPath); DeleteFileW(gBleModbusPath); DeleteFileW(gBleTrafficPath); DeleteFileW(gBleOtaStatusPath); DeleteFileW(gBleOtaManifestPath);
                gBleBackendStarted = FALSE;
            }
            if (gCancelEvent != NULL) { SetEvent(gCancelEvent); }
            if (gSerialHandle != INVALID_HANDLE_VALUE) { PurgeComm(gSerialHandle, PURGE_RXABORT | PURGE_TXABORT); }
            if (gWorkerThread != NULL)
            {
                /* Never unload ControlCAN.dll while an OTA worker may still execute inside it. */
                WaitForSingleObject(gWorkerThread, 0xFFFFFFFFu);
                CloseHandle(gWorkerThread);
                gWorkerThread = NULL;
            }
            if (gCancelEvent != NULL)
            {
                CloseHandle(gCancelEvent);
                gCancelEvent = NULL;
            }
            if (gSerialHandle != INVALID_HANDLE_VALUE)
            {
                CloseHandle(gSerialHandle);
                gSerialHandle = INVALID_HANDLE_VALUE;
            }
            if (gCanConnected) { DisconnectCan(); }
            UnloadControlCanLibrary();
            if (gFontLogo) DeleteObject(gFontLogo);
            if (gFontMission) DeleteObject(gFontMission);
            if (gFontTitle) DeleteObject(gFontTitle);
            if (gFontSubtitle) DeleteObject(gFontSubtitle);
            if (gFontCardTitle) DeleteObject(gFontCardTitle);
            if (gFontBody) DeleteObject(gFontBody);
            if (gFontSmall) DeleteObject(gFontSmall);
            if (gFontTiny) DeleteObject(gFontTiny);
            if (gFontPercent) DeleteObject(gFontPercent);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

void WINAPI WinMainCRTStartup(void)
{
    WNDCLASSEXW windowClass;
    MSG message;
    int index;

    SetProcessDPIAware();
    gInstance = GetModuleHandleW(NULL);
    for (index = 0; index < (int)(sizeof(windowClass) / sizeof(BYTE)); index++) { ((BYTE*)&windowClass)[index] = 0; }
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = gInstance;
    windowClass.hCursor = LoadCursorW(NULL, IDC_ARROW);
    windowClass.hbrBackground = (HBRUSH)(UINT_PTR)(COLOR_WINDOW + 1);
    windowClass.lpszClassName = APP_CLASS;

    if (RegisterClassExW(&windowClass) == 0) { ExitProcess(1); }

    gWindow = CreateWindowExW(0, APP_CLASS, APP_TITLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 1480, 940, NULL, NULL, gInstance, NULL);
    if (gWindow == NULL) { ExitProcess(2); }

    ShowWindow(gWindow, SW_SHOW);
    UpdateWindow(gWindow);
    while (GetMessageW(&message, NULL, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    ExitProcess((UINT)message.wParam);
}
