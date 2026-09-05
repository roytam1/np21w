/* NP2IDLETRAY.C
 *
 * Windows 9x task-tray controller for NP2IDLE.VXD.
 * The application controls the VxD, monitors WinMM WaveOut activity, and
 * persists user preferences in HKEY_CURRENT_USER.
 */

#define WIN32_LEAN_AND_MEAN
#ifndef WINVER
#define WINVER 0x0400
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0400
#endif

#include <windows.h>
#include <mmsystem.h>

/* Libraries used by WinMM monitoring and per-user registry settings. */
#if defined(_MSC_VER)
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "advapi32.lib")
#endif

/* Use only the Windows 95 notification-area ABI. */
#ifndef NIM_ADD
#define NIM_ADD        0x00000000
#define NIM_MODIFY     0x00000001
#define NIM_DELETE     0x00000002
#endif

#ifndef NIF_MESSAGE
#define NIF_MESSAGE    0x00000001
#define NIF_ICON       0x00000002
#define NIF_TIP        0x00000004
#endif

#ifdef __cplusplus
extern "C" {
#endif
/* Declared locally so the source does not depend on newer shell headers. */
BOOL WINAPI Shell_NotifyIconA(DWORD dwMessage, PVOID lpData);
#ifdef __cplusplus
}
#endif

#include "NP2IDLE.H"
#include "RESOURCE.H"
#include "VERSION.H"

#define APP_CLASS_NAME          "NP2IDLETrayWindow"
#define APP_WINDOW_NAME         "NP2IDLE Tray Controller"
#define APP_MUTEX_NAME          "NP2IDLETrayControllerMutex"

#define SETTINGS_REG_KEY        "Software\\NP2IDLE"
#define SETTINGS_AUDIO_GUARD    "AudioHltInhibit"

#define WM_TRAYICON             (WM_USER + 10)
#define TRAY_ICON_ID            1
#define TRAY_RETRY_TIMER_ID     1
#define TRAY_RETRY_INTERVAL     1000
#define TRAY_RETRY_LIMIT        10
#define AUDIO_POLL_TIMER_ID     2
#define AUDIO_POLL_INTERVAL     500

/* Original Windows 95 notification-area structure. */
typedef struct tagNP2_NOTIFYICONDATAA_V1 {
    DWORD cbSize;
    HWND  hWnd;
    UINT  uID;
    UINT  uFlags;
    UINT  uCallbackMessage;
    HICON hIcon;
    CHAR  szTip[64];
} NP2_NOTIFYICONDATAA_V1;

static HICON     g_icon;
static BOOL      g_icon_owned;
static BOOL      g_icon_added;
static UINT      g_taskbar_created;
static UINT      g_tray_retry_count;
static HANDLE    g_mutex;

static BOOL      g_audio_guard_enabled;
static BOOL      g_audio_active;
static UINT      g_audio_active_lines;
static UINT      g_audio_mixer_count;
static UINT      g_audio_probe_errors;
static MMRESULT  g_audio_last_mmerror;
static HANDLE    g_audio_vxd_handle = INVALID_HANDLE_VALUE;

static void update_tray_tooltip(HWND hwnd);

/* -------------------------------------------------------------------------
 * VxD communication
 * ------------------------------------------------------------------------- */

static HANDLE open_np2idle(void)
{
    HANDLE h;

    h = CreateFileA(NP2IDLE_DEVICE_NAME, 0, 0, NULL, 0, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        h = CreateFileA(NP2IDLE_DEVICE_NAME,
                        0, 0, NULL, OPEN_EXISTING, 0, NULL);
    }
    return h;
}

static BOOL send_np2idle_command(DWORD control_code, DWORD *error_code)
{
    HANDLE h;
    DWORD returned;
    BOOL ok;

    if (error_code != NULL) {
        *error_code = ERROR_SUCCESS;
    }

    h = open_np2idle();
    if (h == INVALID_HANDLE_VALUE) {
        if (error_code != NULL) {
            *error_code = GetLastError();
        }
        return FALSE;
    }

    returned = 0;
    ok = DeviceIoControl(h,
                         control_code,
                         NULL, 0,
                         NULL, 0,
                         &returned,
                         NULL);
    if (!ok && error_code != NULL) {
        *error_code = GetLastError();
    }
    CloseHandle(h);
    return ok;
}

static BOOL query_np2idle_dword(DWORD control_code,
                                DWORD *value,
                                DWORD *error_code)
{
    HANDLE h;
    DWORD returned;
    DWORD local_value;
    BOOL ok;

    if (value != NULL) {
        *value = 0;
    }
    if (error_code != NULL) {
        *error_code = ERROR_SUCCESS;
    }

    h = open_np2idle();
    if (h == INVALID_HANDLE_VALUE) {
        if (error_code != NULL) {
            *error_code = GetLastError();
        }
        return FALSE;
    }

    returned = 0;
    local_value = 0;
    ok = DeviceIoControl(h,
                         control_code,
                         NULL, 0,
                         &local_value, sizeof(local_value),
                         &returned,
                         NULL);
    if (!ok) {
        if (error_code != NULL) {
            *error_code = GetLastError();
        }
        CloseHandle(h);
        return FALSE;
    }
    CloseHandle(h);

    if (returned != sizeof(local_value)) {
        if (error_code != NULL) {
            *error_code = ERROR_INVALID_DATA;
        }
        return FALSE;
    }

    if (value != NULL) {
        *value = local_value;
    }
    return TRUE;
}

static BOOL query_np2idle_state(DWORD *state, DWORD *error_code)
{
    DWORD value;

    if (!query_np2idle_dword(NP2IDLE_DIOC_GETSTATE, &value, error_code)) {
        return FALSE;
    }
    if (value != NP2IDLE_STATE_OFF && value != NP2IDLE_STATE_ON) {
        if (error_code != NULL) {
            *error_code = ERROR_INVALID_DATA;
        }
        return FALSE;
    }
    if (state != NULL) {
        *state = value;
    }
    return TRUE;
}

static BOOL query_audio_inhibit(DWORD *state, DWORD *error_code)
{
    DWORD value;

    if (!query_np2idle_dword(NP2IDLE_DIOC_GETAUDIO, &value, error_code)) {
        return FALSE;
    }
    if (value > 1) {
        if (error_code != NULL) {
            *error_code = ERROR_INVALID_DATA;
        }
        return FALSE;
    }
    if (state != NULL) {
        *state = value;
    }
    return TRUE;
}

static BOOL send_audio_vxd_command(DWORD control_code)
{
    DWORD returned;
    BOOL ok;

    if (g_audio_vxd_handle == INVALID_HANDLE_VALUE) {
        g_audio_vxd_handle = open_np2idle();
        if (g_audio_vxd_handle == INVALID_HANDLE_VALUE) {
            return FALSE;
        }
    }

    returned = 0;
    ok = DeviceIoControl(g_audio_vxd_handle,
                         control_code,
                         NULL, 0,
                         NULL, 0,
                         &returned,
                         NULL);
    if (!ok) {
        CloseHandle(g_audio_vxd_handle);
        g_audio_vxd_handle = INVALID_HANDLE_VALUE;
        return FALSE;
    }
    return TRUE;
}

static BOOL renew_audio_inhibit(void)
{
    return send_audio_vxd_command(NP2IDLE_DIOC_AUDIO_BUSY);
}

static void clear_audio_inhibit(void)
{
    (void)send_audio_vxd_command(NP2IDLE_DIOC_AUDIO_CLEAR);
}

/* -------------------------------------------------------------------------
 * Persistent preferences
 * ------------------------------------------------------------------------- */

static BOOL write_audio_guard_setting(BOOL enabled)
{
    HKEY key;
    DWORD disposition;
    DWORD value;
    LONG result;

    result = RegCreateKeyExA(HKEY_CURRENT_USER,
                             SETTINGS_REG_KEY,
                             0,
                             NULL,
                             REG_OPTION_NON_VOLATILE,
                             KEY_SET_VALUE,
                             NULL,
                             &key,
                             &disposition);
    if (result != ERROR_SUCCESS) {
        return FALSE;
    }

    value = enabled ? 1UL : 0UL;
    result = RegSetValueExA(key,
                            SETTINGS_AUDIO_GUARD,
                            0,
                            REG_DWORD,
                            (const BYTE *)&value,
                            sizeof(value));
    RegCloseKey(key);
    return (result == ERROR_SUCCESS);
}

static BOOL read_audio_guard_setting(void)
{
    HKEY key;
    DWORD type;
    DWORD value;
    DWORD size;
    LONG result;

    result = RegOpenKeyExA(HKEY_CURRENT_USER,
                           SETTINGS_REG_KEY,
                           0,
                           KEY_QUERY_VALUE,
                           &key);
    if (result == ERROR_SUCCESS) {
        type = 0;
        value = 1;
        size = sizeof(value);
        result = RegQueryValueExA(key,
                                  SETTINGS_AUDIO_GUARD,
                                  NULL,
                                  &type,
                                  (BYTE *)&value,
                                  &size);
        RegCloseKey(key);
        if (result == ERROR_SUCCESS &&
            type == REG_DWORD &&
            size == sizeof(value) &&
            (value == 0 || value == 1)) {
            return value ? TRUE : FALSE;
        }
    }

    /* Missing or malformed values use the default ON setting and are
     * written back as a normalized REG_DWORD. */
    (void)write_audio_guard_setting(TRUE);
    return TRUE;
}

/* -------------------------------------------------------------------------
 * WinMM audio monitor
 * ------------------------------------------------------------------------- */

static BOOL scan_waveout_activity(void)
{
    UINT mixer_count;
    UINT mixer_id;
    UINT active_lines;
    UINT errors;
    MMRESULT last_error;
    MIXERLINEA line;
    MMRESULT mmresult;

    mixer_count = mixerGetNumDevs();
    active_lines = 0;
    errors = 0;
    last_error = MMSYSERR_NOERROR;

    for (mixer_id = 0; mixer_id < mixer_count; ++mixer_id) {
        ZeroMemory(&line, sizeof(line));
        line.cbStruct = sizeof(line);
        line.dwComponentType = MIXERLINE_COMPONENTTYPE_SRC_WAVEOUT;

        mmresult = mixerGetLineInfoA((HMIXEROBJ)(DWORD)mixer_id,
                                     &line,
                                     MIXER_GETLINEINFOF_COMPONENTTYPE |
                                     MIXER_OBJECTF_MIXER);
        if (mmresult == MMSYSERR_NOERROR) {
            if ((line.fdwLine & MIXERLINE_LINEF_ACTIVE) != 0) {
                ++active_lines;
            }
        } else if (mmresult != MIXERR_INVALLINE) {
            ++errors;
            last_error = mmresult;
        }
    }

    g_audio_mixer_count = mixer_count;
    g_audio_active_lines = active_lines;
    g_audio_probe_errors = errors;
    g_audio_last_mmerror = last_error;
    return (active_lines != 0);
}

static void reset_audio_observation(void)
{
    g_audio_active = FALSE;
    g_audio_active_lines = 0;
    g_audio_mixer_count = 0;
    g_audio_probe_errors = 0;
    g_audio_last_mmerror = MMSYSERR_NOERROR;
}

static void poll_audio_activity(HWND hwnd)
{
    BOOL was_active;

    if (!g_audio_guard_enabled) {
        if (g_audio_active) {
            reset_audio_observation();
            update_tray_tooltip(hwnd);
        }
        return;
    }

    was_active = g_audio_active;
    g_audio_active = scan_waveout_activity();
    if (g_audio_active) {
        (void)renew_audio_inhibit();
    }
    if (was_active != g_audio_active) {
        update_tray_tooltip(hwnd);
    }
}

static void set_audio_guard(HWND hwnd, BOOL enabled)
{
    g_audio_guard_enabled = enabled ? TRUE : FALSE;
    if (!write_audio_guard_setting(g_audio_guard_enabled)) {
        MessageBoxA(hwnd,
                    "オーディオ再生時のHLT禁止設定を保存できませんでした。",
                    "NP2IDLE",
                    MB_OK | MB_ICONEXCLAMATION);
    }

    if (g_audio_guard_enabled) {
        poll_audio_activity(hwnd);
        SetTimer(hwnd, AUDIO_POLL_TIMER_ID, AUDIO_POLL_INTERVAL, NULL);
    } else {
        KillTimer(hwnd, AUDIO_POLL_TIMER_ID);
        reset_audio_observation();
        clear_audio_inhibit();
    }
    update_tray_tooltip(hwnd);
}

/* -------------------------------------------------------------------------
 * User interface helpers
 * ------------------------------------------------------------------------- */

static void format_system_error(DWORD error_code, CHAR *buffer, DWORD size)
{
    DWORD length;

    if (buffer == NULL || size == 0) {
        return;
    }

    buffer[0] = '\0';
    length = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM |
                            FORMAT_MESSAGE_IGNORE_INSERTS,
                            NULL,
                            error_code,
                            0,
                            buffer,
                            size,
                            NULL);
    if (length == 0) {
        wsprintfA(buffer, "Win32エラー %lu", (unsigned long)error_code);
        return;
    }

    while (length != 0 &&
           (buffer[length - 1] == '\r' || buffer[length - 1] == '\n')) {
        buffer[--length] = '\0';
    }
}

static void show_vxd_error(HWND hwnd, const CHAR *operation, DWORD error_code)
{
    CHAR system_text[256];
    CHAR message[512];

    format_system_error(error_code, system_text, sizeof(system_text));
    wsprintfA(message,
              "%sに失敗しました。\r\n\r\nエラー %lu: %s\r\n\r\n"
              "NP2IDLEがインストール済みで、Windowsを再起動済みか確認してください。",
              operation,
              (unsigned long)error_code,
              system_text);
    MessageBoxA(hwnd, message, "NP2IDLE", MB_OK | MB_ICONEXCLAMATION);
}

static void get_tooltip_text(CHAR *text, DWORD size)
{
    DWORD state;
    DWORD error_code;

    if (!query_np2idle_state(&state, &error_code)) {
        lstrcpynA(text, "NP2IDLE - VxD未使用", size);
        return;
    }

    if (state == NP2IDLE_STATE_OFF) {
        lstrcpynA(text, "NP2IDLE - OFF", size);
    } else if (g_audio_guard_enabled && g_audio_active) {
        lstrcpynA(text, "NP2IDLE - ON (Wave再生中)", size);
    } else {
        lstrcpynA(text, "NP2IDLE - ON", size);
    }
}

static void initialize_notify_data(HWND hwnd,
                                   NP2_NOTIFYICONDATAA_V1 *nid,
                                   UINT flags)
{
    ZeroMemory(nid, sizeof(*nid));
    nid->cbSize = sizeof(*nid);
    nid->hWnd = hwnd;
    nid->uID = TRAY_ICON_ID;
    nid->uFlags = flags;
    nid->uCallbackMessage = WM_TRAYICON;
    nid->hIcon = g_icon;
    get_tooltip_text(nid->szTip, sizeof(nid->szTip));
}

static BOOL add_tray_icon(HWND hwnd)
{
    NP2_NOTIFYICONDATAA_V1 nid;

    initialize_notify_data(hwnd, &nid, NIF_MESSAGE | NIF_ICON | NIF_TIP);
    if (!Shell_NotifyIconA(NIM_ADD, (PVOID)&nid)) {
        g_icon_added = FALSE;
        return FALSE;
    }
    g_icon_added = TRUE;
    return TRUE;
}

static void update_tray_tooltip(HWND hwnd)
{
    NP2_NOTIFYICONDATAA_V1 nid;

    if (!g_icon_added) {
        return;
    }
    initialize_notify_data(hwnd, &nid, NIF_TIP);
    Shell_NotifyIconA(NIM_MODIFY, (PVOID)&nid);
}

static void remove_tray_icon(HWND hwnd)
{
    NP2_NOTIFYICONDATAA_V1 nid;

    if (!g_icon_added) {
        return;
    }
    initialize_notify_data(hwnd, &nid, 0);
    Shell_NotifyIconA(NIM_DELETE, (PVOID)&nid);
    g_icon_added = FALSE;
}

static void begin_tray_retry(HWND hwnd)
{
    g_tray_retry_count = 0;
    SetTimer(hwnd, TRAY_RETRY_TIMER_ID, TRAY_RETRY_INTERVAL, NULL);
}

static void show_status(HWND hwnd)
{
    DWORD state;
    DWORD inhibit_state;
    DWORD error_code;
    DWORD inhibit_error;
    BOOL inhibit_ok;
    CHAR message[640];

    if (!query_np2idle_state(&state, &error_code)) {
        show_vxd_error(hwnd, "状態の取得", error_code);
        update_tray_tooltip(hwnd);
        return;
    }

    inhibit_state = 0;
    inhibit_error = ERROR_SUCCESS;
    inhibit_ok = query_audio_inhibit(&inhibit_state, &inhibit_error);

    wsprintfA(message,
              "NP2IDLE.VXDは読み込まれています。\r\n\r\n"
              "アイドル時HLT: %s\r\n"
              "オーディオ再生時HLT禁止: %s\r\n"
              "WinMM WaveOut再生中: %s\r\n"
              "再生中WaveOutライン数: %u\r\n"
              "Mixerデバイス数: %u\r\n"
              "Mixer検出エラー: %u (最終=%u)\r\n"
              "VxD側オーディオ禁止リース: %s",
              (state == NP2IDLE_STATE_ON) ? "ON" : "OFF",
              g_audio_guard_enabled ? "ON" : "OFF",
              g_audio_guard_enabled ? (g_audio_active ? "はい" : "いいえ") : "監視していません",
              g_audio_active_lines,
              g_audio_mixer_count,
              g_audio_probe_errors,
              (UINT)g_audio_last_mmerror,
              inhibit_ok ? (inhibit_state ? "有効" : "無効") : "取得失敗");

    MessageBoxA(hwnd, message, "NP2IDLE 状態", MB_OK | MB_ICONINFORMATION);
}

static void show_about(HWND hwnd)
{
    CHAR message[384];

    wsprintfA(message,
              "NP2IDLE トレイ常駐プログラム\r\n"
              "バージョン %s",
              NP2IDLE_VERSION_STRING);
    MessageBoxA(hwnd, message, "NP2IDLE について", MB_OK | MB_ICONINFORMATION);
}

static void change_state(HWND hwnd, DWORD control_code)
{
    DWORD error_code;
    const CHAR *operation;

    operation = (control_code == NP2IDLE_DIOC_ENABLE) ? "HLTの有効化" : "HLTの無効化";
    if (!send_np2idle_command(control_code, &error_code)) {
        show_vxd_error(hwnd, operation, error_code);
    }
    update_tray_tooltip(hwnd);
}

static void show_tray_menu(HWND hwnd)
{
    HMENU menu;
    POINT point;
    DWORD state;
    DWORD error_code;
    BOOL available;
    UINT command;
    CHAR status_text[80];

    available = query_np2idle_state(&state, &error_code);
    menu = CreatePopupMenu();
    if (menu == NULL) {
        return;
    }

    if (available) {
        wsprintfA(status_text,
                  "状態: %s%s...",
                  (state == NP2IDLE_STATE_ON) ? "ON" : "OFF",
                  (state == NP2IDLE_STATE_ON &&
                   g_audio_guard_enabled &&
                   g_audio_active) ? " / Wave再生中" : "");
    } else {
        lstrcpynA(status_text, "状態: VxD未使用...", sizeof(status_text));
    }

    AppendMenuA(menu, MF_STRING, IDM_STATUS, status_text);
    AppendMenuA(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(menu,
                MF_STRING | (available ? 0 : MF_GRAYED),
                IDM_ENABLE,
                "ON");
    AppendMenuA(menu,
                MF_STRING | (available ? 0 : MF_GRAYED),
                IDM_DISABLE,
                "OFF");

    if (available) {
        CheckMenuRadioItem(menu,
                           IDM_ENABLE,
                           IDM_DISABLE,
                           (state == NP2IDLE_STATE_ON) ? IDM_ENABLE : IDM_DISABLE,
                           MF_BYCOMMAND);
    }

    AppendMenuA(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(menu,
                MF_STRING | (g_audio_guard_enabled ? MF_CHECKED : 0),
                IDM_AUDIO_GUARD,
                "Wave再生中のHLT禁止");
    AppendMenuA(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(menu, MF_STRING, IDM_ABOUT, "このプログラムについて...");
    AppendMenuA(menu, MF_STRING, IDM_EXIT, "終了");

    GetCursorPos(&point);
    SetForegroundWindow(hwnd);
    command = TrackPopupMenu(menu,
                             TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY,
                             point.x,
                             point.y,
                             0,
                             hwnd,
                             NULL);
    PostMessageA(hwnd, WM_NULL, 0, 0);
    DestroyMenu(menu);

    if (command != 0) {
        SendMessageA(hwnd, WM_COMMAND, MAKEWPARAM(command, 0), 0);
    }
}

/* -------------------------------------------------------------------------
 * Window procedure and startup
 * ------------------------------------------------------------------------- */

static LRESULT CALLBACK tray_window_proc(HWND hwnd,
                                         UINT message,
                                         WPARAM wparam,
                                         LPARAM lparam)
{
    if (g_taskbar_created != 0 && message == g_taskbar_created) {
        g_icon_added = FALSE;
        if (!add_tray_icon(hwnd)) {
            begin_tray_retry(hwnd);
        }
        return 0;
    }

    switch (message) {
    case WM_COMMAND:
        switch (LOWORD(wparam)) {
        case IDM_STATUS:
            show_status(hwnd);
            return 0;
        case IDM_ENABLE:
            change_state(hwnd, NP2IDLE_DIOC_ENABLE);
            return 0;
        case IDM_DISABLE:
            change_state(hwnd, NP2IDLE_DIOC_DISABLE);
            return 0;
        case IDM_AUDIO_GUARD:
            set_audio_guard(hwnd, !g_audio_guard_enabled);
            return 0;
        case IDM_ABOUT:
            show_about(hwnd);
            return 0;
        case IDM_EXIT:
            DestroyWindow(hwnd);
            return 0;
        }
        break;

    case WM_TRAYICON:
        switch ((UINT)lparam) {
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU:
            show_tray_menu(hwnd);
            return 0;
        case WM_LBUTTONDBLCLK:
            show_status(hwnd);
            return 0;
        }
        break;

    case WM_TIMER:
        if (wparam == AUDIO_POLL_TIMER_ID) {
            poll_audio_activity(hwnd);
            return 0;
        }
        if (wparam == TRAY_RETRY_TIMER_ID) {
            if (add_tray_icon(hwnd)) {
                KillTimer(hwnd, TRAY_RETRY_TIMER_ID);
                return 0;
            }
            ++g_tray_retry_count;
            if (g_tray_retry_count >= TRAY_RETRY_LIMIT) {
                KillTimer(hwnd, TRAY_RETRY_TIMER_ID);
                MessageBoxA(hwnd,
                            "タスクトレイアイコンを作成できませんでした。",
                            "NP2IDLE",
                            MB_OK | MB_ICONEXCLAMATION);
                DestroyWindow(hwnd);
            }
            return 0;
        }
        break;

    case WM_DESTROY:
        KillTimer(hwnd, TRAY_RETRY_TIMER_ID);
        KillTimer(hwnd, AUDIO_POLL_TIMER_ID);
        clear_audio_inhibit();
        if (g_audio_vxd_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(g_audio_vxd_handle);
            g_audio_vxd_handle = INVALID_HANDLE_VALUE;
        }
        remove_tray_icon(hwnd);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcA(hwnd, message, wparam, lparam);
}

int WINAPI WinMain(HINSTANCE instance,
                   HINSTANCE previous_instance,
                   LPSTR command_line,
                   int show_command)
{
    WNDCLASSA window_class;
    HWND hwnd;
    MSG message;
    int result;

    (void)previous_instance;
    (void)command_line;
    (void)show_command;

    g_icon_owned = FALSE;
    g_icon_added = FALSE;
    g_tray_retry_count = 0;
    g_audio_guard_enabled = TRUE;
    reset_audio_observation();
    g_audio_vxd_handle = INVALID_HANDLE_VALUE;

    g_mutex = CreateMutexA(NULL, FALSE, APP_MUTEX_NAME);
    if (g_mutex == NULL) {
        return 1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(g_mutex);
        g_mutex = NULL;
        return 0;
    }

    g_audio_guard_enabled = read_audio_guard_setting();

    g_icon = (HICON)LoadImageA(instance,
                               MAKEINTRESOURCEA(IDI_NP2IDLE),
                               IMAGE_ICON,
                               GetSystemMetrics(SM_CXSMICON),
                               GetSystemMetrics(SM_CYSMICON),
                               LR_DEFAULTCOLOR);
    if (g_icon != NULL) {
        g_icon_owned = TRUE;
    } else {
        g_icon = LoadIconA(NULL, IDI_APPLICATION);
        g_icon_owned = FALSE;
    }

    ZeroMemory(&window_class, sizeof(window_class));
    window_class.lpfnWndProc = tray_window_proc;
    window_class.hInstance = instance;
    window_class.hIcon = g_icon;
    window_class.hCursor = LoadCursorA(NULL, IDC_ARROW);
    window_class.lpszClassName = APP_CLASS_NAME;

    if (!RegisterClassA(&window_class)) {
        if (g_icon_owned && g_icon != NULL) {
            DestroyIcon(g_icon);
        }
        CloseHandle(g_mutex);
        return 1;
    }

    g_taskbar_created = RegisterWindowMessageA("TaskbarCreated");
    hwnd = CreateWindowExA(0,
                           APP_CLASS_NAME,
                           APP_WINDOW_NAME,
                           WS_OVERLAPPED,
                           0, 0, 0, 0,
                           NULL, NULL,
                           instance,
                           NULL);
    if (hwnd == NULL) {
        UnregisterClassA(APP_CLASS_NAME, instance);
        if (g_icon_owned && g_icon != NULL) {
            DestroyIcon(g_icon);
        }
        CloseHandle(g_mutex);
        return 1;
    }

    if (!add_tray_icon(hwnd)) {
        begin_tray_retry(hwnd);
    }
    if (g_audio_guard_enabled) {
        poll_audio_activity(hwnd);
        SetTimer(hwnd, AUDIO_POLL_TIMER_ID, AUDIO_POLL_INTERVAL, NULL);
    } else {
        clear_audio_inhibit();
    }

    while ((result = GetMessageA(&message, NULL, 0, 0)) > 0) {
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }

    result = (result < 0) ? 1 : (int)message.wParam;
    if (IsWindow(hwnd)) {
        DestroyWindow(hwnd);
    }

    UnregisterClassA(APP_CLASS_NAME, instance);
    if (g_icon_owned && g_icon != NULL) {
        DestroyIcon(g_icon);
    }
    g_icon = NULL;
    g_icon_owned = FALSE;

    if (g_mutex != NULL) {
        CloseHandle(g_mutex);
        g_mutex = NULL;
    }
    return result;
}
