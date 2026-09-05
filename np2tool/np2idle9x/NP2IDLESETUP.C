/* NP2IDLESETUP.C
 *
 * Windows 9x installer and uninstaller for NP2IDLE.
 * Build with Visual C++ 6.0.  Only Win32 APIs available on Windows 95 are used.
 *
 * Distribution directory requirements:
 *   SETUP.EXE
 *   NP2IDLE.VXD
 *   NP2ITRAY.EXE
 *   NP2ICTRL.EXE   (optional)
 *
 * Installation layout:
 *   %WinDir%\SYSTEM\NP2IDLE.VXD
 *   <system drive>\np2idle9\NP2IDLE.VXD  (repair source copy)
 *   <system drive>\np2idle9\NP2ITRAY.EXE
 *   <system drive>\np2idle9\NP2ICTRL.EXE
 *   <system drive>\np2idle9\SETUP.EXE
 *
 * The VxD is registered through:
 *   HKLM\System\CurrentControlSet\Services\VxD\NP2IDLE
 *       StaticVxD = "NP2IDLE.VXD"
 *
 * The tray controller is registered through the normal machine Run key.
 * An Add/Remove Programs entry invokes this executable with /uninstall.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "NP2IDLE.H"
#include "VERSION.H"

#define PRODUCT_NAME            "NP2IDLE"
#define SETUP_EXE_NAME          "SETUP.EXE"
#define TRAY_EXE_NAME           "NP2ITRAY.EXE"
#define CTL_EXE_NAME            "NP2ICTRL.EXE"
#define VXD_FILE_NAME           "NP2IDLE.VXD"

#define INSTALL_SUBDIR          "np2idle9"
#define TRAY_WINDOW_CLASS       "NP2IDLETrayWindow"
#define TRAY_WINDOW_TITLE       "NP2IDLE Tray Controller"

#define VXD_REG_KEY             "System\\CurrentControlSet\\Services\\VxD\\NP2IDLE"
#define RUN_REG_KEY             "Software\\Microsoft\\Windows\\CurrentVersion\\Run"
#define UNINSTALL_REG_KEY       "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\NP2IDLE"
#define SETTINGS_REG_KEY        "Software\\NP2IDLE"

#ifndef ARRAYSIZE
#define ARRAYSIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

static BOOL g_silent = FALSE;

static BOOL query_installed_path(char *path, DWORD capacity);

static void show_error_text(const char *operation, const char *detail)
{
    char text[768];

    wsprintfA(text,
              "%sに失敗しました。\r\n\r\n%s\r\n\r\nWin32エラー: %lu",
              operation,
              (detail != NULL) ? detail : "",
              GetLastError());

    MessageBoxA(NULL, text, PRODUCT_NAME " セットアップ", MB_OK | MB_ICONSTOP);
}

static void show_simple_error(const char *text)
{
    MessageBoxA(NULL, text, PRODUCT_NAME " セットアップ", MB_OK | MB_ICONSTOP);
}

static BOOL is_windows_9x(void)
{
    OSVERSIONINFOA version;

    ZeroMemory(&version, sizeof(version));
    version.dwOSVersionInfoSize = sizeof(version);
    if (!GetVersionExA(&version)) {
        return FALSE;
    }

    return (version.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS);
}

static BOOL path_append(char *path, DWORD capacity, const char *name)
{
    DWORD length;
    DWORD name_length;

    length = (DWORD)lstrlenA(path);
    name_length = (DWORD)lstrlenA(name);

    if (length != 0 && path[length - 1] != '\\') {
        if (length + 1 >= capacity) {
            return FALSE;
        }
        path[length++] = '\\';
        path[length] = '\0';
    }

    if (length + name_length >= capacity) {
        return FALSE;
    }

    lstrcatA(path, name);
    return TRUE;
}

static BOOL get_module_directory(char *directory, DWORD capacity)
{
    DWORD length;
    char *slash;

    length = GetModuleFileNameA(NULL, directory, capacity);
    if (length == 0 || length >= capacity) {
        return FALSE;
    }

    slash = strrchr(directory, '\\');
    if (slash == NULL) {
        return FALSE;
    }

    *slash = '\0';
    return TRUE;
}

static BOOL build_source_path(char *path,
                              DWORD capacity,
                              const char *source_directory,
                              const char *file_name)
{
    if ((DWORD)lstrlenA(source_directory) >= capacity) {
        return FALSE;
    }

    lstrcpyA(path, source_directory);
    return path_append(path, capacity, file_name);
}

static BOOL file_exists(const char *path)
{
    DWORD attributes;

    attributes = GetFileAttributesA(path);
    return (attributes != 0xffffffffUL &&
            (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0);
}

static BOOL paths_equal(const char *left, const char *right)
{
    char left_full[MAX_PATH];
    char right_full[MAX_PATH];
    DWORD left_length;
    DWORD right_length;

    left_length = GetFullPathNameA(left, ARRAYSIZE(left_full), left_full, NULL);
    right_length = GetFullPathNameA(right, ARRAYSIZE(right_full), right_full, NULL);

    if (left_length == 0 || left_length >= ARRAYSIZE(left_full) ||
        right_length == 0 || right_length >= ARRAYSIZE(right_full)) {
        return (lstrcmpiA(left, right) == 0);
    }

    return (lstrcmpiA(left_full, right_full) == 0);
}

/* Build the product directory on the same drive as the Windows directory. */
static BOOL build_install_directory(const char *windows_directory,
                                    char *install_directory,
                                    DWORD capacity)
{
    if (windows_directory == NULL ||
        windows_directory[0] == '\0' ||
        windows_directory[1] != ':') {
        SetLastError(ERROR_INVALID_DRIVE);
        return FALSE;
    }

    if (capacity < 4) {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return FALSE;
    }

    install_directory[0] = windows_directory[0];
    install_directory[1] = ':';
    install_directory[2] = '\\';
    install_directory[3] = '\0';
    if (!path_append(install_directory, capacity, INSTALL_SUBDIR)) {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return FALSE;
    }
    return TRUE;
}

#define NP2_DIR_MISSING   0
#define NP2_DIR_EMPTY     1
#define NP2_DIR_NONEMPTY  2
#define NP2_DIR_ERROR    -1

/* Distinguish an absent directory from an empty or occupied directory. */
static int get_directory_state(const char *directory)
{
    DWORD attributes;
    DWORD error;
    char pattern[MAX_PATH];
    WIN32_FIND_DATAA find_data;
    HANDLE find_handle;

    attributes = GetFileAttributesA(directory);
    if (attributes == 0xffffffffUL) {
        error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
            return NP2_DIR_MISSING;
        }
        return NP2_DIR_ERROR;
    }
    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        SetLastError(ERROR_ALREADY_EXISTS);
        return NP2_DIR_ERROR;
    }

    lstrcpynA(pattern, directory, ARRAYSIZE(pattern));
    if (!path_append(pattern, ARRAYSIZE(pattern), "*.*")) {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return NP2_DIR_ERROR;
    }

    find_handle = FindFirstFileA(pattern, &find_data);
    if (find_handle == INVALID_HANDLE_VALUE) {
        error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND) {
            return NP2_DIR_EMPTY;
        }
        return NP2_DIR_ERROR;
    }

    for (;;) {
        if (lstrcmpA(find_data.cFileName, ".") != 0 &&
            lstrcmpA(find_data.cFileName, "..") != 0) {
            FindClose(find_handle);
            return NP2_DIR_NONEMPTY;
        }
        if (!FindNextFileA(find_handle, &find_data)) {
            error = GetLastError();
            FindClose(find_handle);
            if (error == ERROR_NO_MORE_FILES) {
                return NP2_DIR_EMPTY;
            }
            SetLastError(error);
            return NP2_DIR_ERROR;
        }
    }
}

/* Create the install directory, warning only when unrelated contents exist. */
static BOOL prepare_install_directory(const char *directory)
{
    int state;
    char message[MAX_PATH + 256];

    state = get_directory_state(directory);
    if (state == NP2_DIR_ERROR) {
        show_error_text("インストール先の確認", directory);
        return FALSE;
    }

    if (state == NP2_DIR_MISSING) {
        if (!CreateDirectoryA(directory, NULL)) {
            show_error_text("インストール先の作成", directory);
            return FALSE;
        }
        return TRUE;
    }

    if (state == NP2_DIR_NONEMPTY && !g_silent) {
        char registered_directory[MAX_PATH];
        BOOL registered_here;

        registered_here = query_installed_path(registered_directory,
                                               ARRAYSIZE(registered_directory)) &&
                          paths_equal(registered_directory, directory);
        if (!registered_here) {
            wsprintfA(message,
                      "%s は既に存在し、空ではありません。\r\n\r\n"
                      "既存のファイルを上書きする可能性があります。\r\n"
                      "このディレクトリへインストールしますか?",
                      directory);
            if (MessageBoxA(NULL,
                            message,
                            PRODUCT_NAME " セットアップ",
                            MB_YESNO | MB_ICONEXCLAMATION) != IDYES) {
                return FALSE;
            }
        }
    }
    return TRUE;
}

static BOOL copy_one_file(const char *source,
                          const char *destination,
                          BOOL required)
{
    if (!file_exists(source)) {
        if (!required) {
            return TRUE;
        }
        SetLastError(ERROR_FILE_NOT_FOUND);
        show_error_text("インストール", source);
        return FALSE;
    }

    if (paths_equal(source, destination)) {
        return TRUE;
    }

    if (!CopyFileA(source, destination, FALSE)) {
        show_error_text("ファイルのコピー", destination);
        return FALSE;
    }

    return TRUE;
}

static BOOL set_reg_string(HKEY root,
                           const char *key_name,
                           const char *value_name,
                           const char *value)
{
    HKEY key;
    LONG result;

    result = RegCreateKeyExA(root,
                             key_name,
                             0,
                             NULL,
                             REG_OPTION_NON_VOLATILE,
                             KEY_SET_VALUE,
                             NULL,
                             &key,
                             NULL);
    if (result != ERROR_SUCCESS) {
        SetLastError((DWORD)result);
        return FALSE;
    }

    result = RegSetValueExA(key,
                            value_name,
                            0,
                            REG_SZ,
                            (const BYTE *)value,
                            (DWORD)lstrlenA(value) + 1);
    RegCloseKey(key);

    if (result != ERROR_SUCCESS) {
        SetLastError((DWORD)result);
        return FALSE;
    }

    return TRUE;
}

static BOOL set_reg_dword(HKEY root,
                          const char *key_name,
                          const char *value_name,
                          DWORD value)
{
    HKEY key;
    LONG result;

    result = RegCreateKeyExA(root,
                             key_name,
                             0,
                             NULL,
                             REG_OPTION_NON_VOLATILE,
                             KEY_SET_VALUE,
                             NULL,
                             &key,
                             NULL);
    if (result != ERROR_SUCCESS) {
        SetLastError((DWORD)result);
        return FALSE;
    }

    result = RegSetValueExA(key,
                            value_name,
                            0,
                            REG_DWORD,
                            (const BYTE *)&value,
                            sizeof(value));
    RegCloseKey(key);

    if (result != ERROR_SUCCESS) {
        SetLastError((DWORD)result);
        return FALSE;
    }

    return TRUE;
}

static void delete_reg_value(HKEY root,
                             const char *key_name,
                             const char *value_name)
{
    HKEY key;

    if (RegOpenKeyExA(root, key_name, 0, KEY_SET_VALUE, &key) == ERROR_SUCCESS) {
        RegDeleteValueA(key, value_name);
        RegCloseKey(key);
    }
}

static void delete_known_key(HKEY root,
                             const char *key_name,
                             const char *const *value_names,
                             int value_count)
{
    HKEY key;
    int index;

    if (RegOpenKeyExA(root, key_name, 0, KEY_SET_VALUE, &key) == ERROR_SUCCESS) {
        for (index = 0; index < value_count; ++index) {
            RegDeleteValueA(key, value_names[index]);
        }
        RegCloseKey(key);
    }

    RegDeleteKeyA(root, key_name);
}

static BOOL query_installed_path(char *path, DWORD capacity)
{
    HKEY key;
    DWORD type;
    DWORD size;
    LONG result;

    result = RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                           UNINSTALL_REG_KEY,
                           0,
                           KEY_QUERY_VALUE,
                           &key);
    if (result != ERROR_SUCCESS) {
        return FALSE;
    }

    size = capacity;
    type = 0;
    result = RegQueryValueExA(key,
                              "InstallLocation",
                              NULL,
                              &type,
                              (BYTE *)path,
                              &size);
    RegCloseKey(key);

    if (result != ERROR_SUCCESS || type != REG_SZ || size == 0) {
        return FALSE;
    }

    path[capacity - 1] = '\0';
    return TRUE;
}

static BOOL is_installed(void)
{
    HKEY key;

    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                      VXD_REG_KEY,
                      0,
                      KEY_QUERY_VALUE,
                      &key) == ERROR_SUCCESS) {
        RegCloseKey(key);
        return TRUE;
    }

    return FALSE;
}


static BOOL slice_equals_i(const char *data,
                           DWORD start,
                           DWORD end,
                           const char *text)
{
    DWORD text_length;
    DWORD index;

    while (start < end && (data[start] == ' ' || data[start] == '\t')) {
        ++start;
    }
    while (end > start && (data[end - 1] == ' ' || data[end - 1] == '\t')) {
        --end;
    }

    text_length = (DWORD)lstrlenA(text);
    if (end - start != text_length) {
        return FALSE;
    }

    for (index = 0; index < text_length; ++index) {
        if (tolower((unsigned char)data[start + index]) !=
            tolower((unsigned char)text[index])) {
            return FALSE;
        }
    }

    return TRUE;
}

static BOOL slice_contains_i(const char *data,
                             DWORD start,
                             DWORD end,
                             const char *text)
{
    DWORD text_length;
    DWORD index;
    DWORD compare_index;

    text_length = (DWORD)lstrlenA(text);
    if (text_length == 0 || end - start < text_length) {
        return FALSE;
    }

    for (index = start; index + text_length <= end; ++index) {
        for (compare_index = 0;
             compare_index < text_length;
             ++compare_index) {
            if (tolower((unsigned char)data[index + compare_index]) !=
                tolower((unsigned char)text[compare_index])) {
                break;
            }
        }
        if (compare_index == text_length) {
            return TRUE;
        }
    }

    return FALSE;
}

static BOOL line_is_section(const char *data,
                            DWORD start,
                            DWORD end,
                            const char *section_name)
{
    while (start < end && (data[start] == ' ' || data[start] == '\t')) {
        ++start;
    }
    while (end > start && (data[end - 1] == ' ' || data[end - 1] == '\t')) {
        --end;
    }

    if (end - start < 3 || data[start] != '[' || data[end - 1] != ']') {
        return FALSE;
    }

    return slice_equals_i(data, start + 1, end - 1, section_name);
}

static BOOL line_is_any_section(const char *data,
                                DWORD start,
                                DWORD end)
{
    while (start < end && (data[start] == ' ' || data[start] == '\t')) {
        ++start;
    }
    while (end > start && (data[end - 1] == ' ' || data[end - 1] == '\t')) {
        --end;
    }

    return (end - start >= 2 && data[start] == '[' && data[end - 1] == ']');
}

static BOOL line_is_np2idle_device(const char *data,
                                   DWORD start,
                                   DWORD end)
{
    DWORD equals;
    DWORD cursor;

    while (start < end && (data[start] == ' ' || data[start] == '\t')) {
        ++start;
    }

    if (start >= end || data[start] == ';') {
        return FALSE;
    }

    equals = start;
    while (equals < end && data[equals] != '=') {
        ++equals;
    }
    if (equals >= end) {
        return FALSE;
    }

    if (!slice_equals_i(data, start, equals, "device")) {
        return FALSE;
    }

    cursor = equals + 1;
    while (cursor < end && (data[cursor] == ' ' || data[cursor] == '\t')) {
        ++cursor;
    }

    return slice_contains_i(data, cursor, end, VXD_FILE_NAME);
}

static BOOL write_whole_file(const char *path,
                             const char *data,
                             DWORD length)
{
    HANDLE file;
    DWORD total;
    DWORD written;

    file = CreateFileA(path,
                       GENERIC_WRITE,
                       0,
                       NULL,
                       CREATE_ALWAYS,
                       FILE_ATTRIBUTE_NORMAL,
                       NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    total = 0;
    while (total < length) {
        written = 0;
        if (!WriteFile(file,
                       data + total,
                       length - total,
                       &written,
                       NULL) ||
            written == 0) {
            CloseHandle(file);
            return FALSE;
        }
        total += written;
    }

    CloseHandle(file);
    return TRUE;
}

static BOOL remove_legacy_system_ini_entry(void)
{
    char windows_directory[MAX_PATH];
    char system_ini[MAX_PATH];
    char backup_ini[MAX_PATH];
    HANDLE file;
    DWORD file_size;
    DWORD read_count;
    char *input;
    char *output;
    DWORD input_pos;
    DWORD output_pos;
    DWORD line_start;
    DWORD line_end;
    DWORD line_full_end;
    BOOL in_386enh;
    BOOL changed;

    if (GetWindowsDirectoryA(windows_directory, ARRAYSIZE(windows_directory)) == 0) {
        return FALSE;
    }

    lstrcpyA(system_ini, windows_directory);
    lstrcpyA(backup_ini, windows_directory);
    if (!path_append(system_ini, ARRAYSIZE(system_ini), "SYSTEM.INI") ||
        !path_append(backup_ini, ARRAYSIZE(backup_ini), "SYSTEM.NP2")) {
        return FALSE;
    }

    file = CreateFileA(system_ini,
                       GENERIC_READ,
                       FILE_SHARE_READ,
                       NULL,
                       OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL,
                       NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return TRUE;
    }

    file_size = GetFileSize(file, NULL);
    if (file_size == 0xffffffffUL || file_size > 1024UL * 1024UL) {
        CloseHandle(file);
        return FALSE;
    }

    input = (char *)LocalAlloc(LPTR, file_size + 1);
    output = (char *)LocalAlloc(LPTR, file_size + 1);
    if (input == NULL || output == NULL) {
        if (input != NULL) {
            LocalFree(input);
        }
        if (output != NULL) {
            LocalFree(output);
        }
        CloseHandle(file);
        return FALSE;
    }

    read_count = 0;
    if (file_size != 0 &&
        (!ReadFile(file, input, file_size, &read_count, NULL) ||
         read_count != file_size)) {
        LocalFree(input);
        LocalFree(output);
        CloseHandle(file);
        return FALSE;
    }
    CloseHandle(file);

    input_pos = 0;
    output_pos = 0;
    in_386enh = FALSE;
    changed = FALSE;

    while (input_pos < file_size) {
        line_start = input_pos;
        while (input_pos < file_size &&
               input[input_pos] != '\r' &&
               input[input_pos] != '\n') {
            ++input_pos;
        }
        line_end = input_pos;
        while (input_pos < file_size &&
               (input[input_pos] == '\r' || input[input_pos] == '\n')) {
            ++input_pos;
        }
        line_full_end = input_pos;

        if (line_is_section(input, line_start, line_end, "386Enh")) {
            in_386enh = TRUE;
        } else if (line_is_any_section(input, line_start, line_end)) {
            in_386enh = FALSE;
        }

        if (in_386enh &&
            line_is_np2idle_device(input, line_start, line_end)) {
            changed = TRUE;
            continue;
        }

        if (line_full_end > line_start) {
            CopyMemory(output + output_pos,
                       input + line_start,
                       line_full_end - line_start);
            output_pos += line_full_end - line_start;
        }
    }

    if (!changed) {
        LocalFree(input);
        LocalFree(output);
        return TRUE;
    }

    /* Keep a one-time recovery copy without replacing an existing backup. */
    CopyFileA(system_ini, backup_ini, TRUE);

    if (!write_whole_file(system_ini, output, output_pos)) {
        /* Best-effort immediate restoration from the in-memory original. */
        write_whole_file(system_ini, input, file_size);
        LocalFree(input);
        LocalFree(output);
        return FALSE;
    }

    LocalFree(input);
    LocalFree(output);
    return TRUE;
}

static void disable_loaded_vxd(void)
{
    HANDLE device;
    DWORD returned;

    device = CreateFileA(NP2IDLE_DEVICE_NAME,
                         0,
                         0,
                         NULL,
                         OPEN_EXISTING,
                         FILE_ATTRIBUTE_NORMAL,
                         NULL);

    if (device == INVALID_HANDLE_VALUE) {
        return;
    }

    DeviceIoControl(device,
                    NP2IDLE_DIOC_DISABLE,
                    NULL,
                    0,
                    NULL,
                    0,
                    &returned,
                    NULL);

    CloseHandle(device);
}

static void stop_tray_controller(void)
{
    HWND window;
    int attempt;

    window = FindWindowA(TRAY_WINDOW_CLASS, TRAY_WINDOW_TITLE);
    if (window == NULL) {
        return;
    }

    PostMessageA(window, WM_CLOSE, 0, 0);

    for (attempt = 0; attempt < 20; ++attempt) {
        Sleep(100);
        if (!IsWindow(window)) {
            break;
        }
    }
}

static int ascii_casecmp(const char *left, const char *right)
{
    unsigned char l;
    unsigned char r;

    while (*left != '\0' && *right != '\0') {
        l = (unsigned char)tolower((unsigned char)*left);
        r = (unsigned char)tolower((unsigned char)*right);
        if (l != r) {
            return (int)l - (int)r;
        }
        ++left;
        ++right;
    }

    return (unsigned char)*left - (unsigned char)*right;
}

static BOOL buffer_contains_path(const char *buffer,
                                 DWORD length,
                                 const char *path)
{
    DWORD path_length;
    DWORD index;

    path_length = (DWORD)lstrlenA(path);
    if (path_length == 0 || path_length > length) {
        return FALSE;
    }

    for (index = 0; index + path_length <= length; ++index) {
        if (_strnicmp(buffer + index, path, path_length) == 0) {
            return TRUE;
        }
    }

    return FALSE;
}

static BOOL schedule_wininit_delete(const char *file_path)
{
    char windows_directory[MAX_PATH];
    char wininit_path[MAX_PATH];
    char short_path[MAX_PATH];
    HANDLE file;
    DWORD file_size;
    DWORD read_count;
    char *old_data;
    char *new_data;
    DWORD insert_offset;
    DWORD index;
    DWORD line_start;
    DWORD line_end;
    DWORD new_length;
    char delete_line[MAX_PATH + 16];
    BOOL found_section;
    BOOL success;

    if (GetWindowsDirectoryA(windows_directory, ARRAYSIZE(windows_directory)) == 0) {
        return FALSE;
    }

    lstrcpyA(wininit_path, windows_directory);
    if (!path_append(wininit_path, ARRAYSIZE(wininit_path), "WININIT.INI")) {
        return FALSE;
    }

    if (GetShortPathNameA(file_path, short_path, ARRAYSIZE(short_path)) == 0) {
        lstrcpynA(short_path, file_path, ARRAYSIZE(short_path));
    }

    file = CreateFileA(wininit_path,
                       GENERIC_READ,
                       FILE_SHARE_READ,
                       NULL,
                       OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL,
                       NULL);

    if (file == INVALID_HANDLE_VALUE) {
        old_data = NULL;
        file_size = 0;
    } else {
        file_size = GetFileSize(file, NULL);
        if (file_size == 0xffffffffUL || file_size > 1024UL * 1024UL) {
            CloseHandle(file);
            return FALSE;
        }

        old_data = (char *)LocalAlloc(LPTR, file_size + 1);
        if (old_data == NULL) {
            CloseHandle(file);
            return FALSE;
        }

        read_count = 0;
        if (file_size != 0 &&
            (!ReadFile(file, old_data, file_size, &read_count, NULL) ||
             read_count != file_size)) {
            CloseHandle(file);
            LocalFree(old_data);
            return FALSE;
        }
        CloseHandle(file);
        old_data[file_size] = '\0';
    }

    if (old_data != NULL && buffer_contains_path(old_data, file_size, short_path)) {
        LocalFree(old_data);
        return TRUE;
    }

    wsprintfA(delete_line, "NUL=%s\r\n", short_path);

    found_section = FALSE;
    insert_offset = file_size;
    index = 0;

    while (old_data != NULL && index < file_size) {
        line_start = index;
        while (index < file_size && old_data[index] != '\r' && old_data[index] != '\n') {
            ++index;
        }
        line_end = index;
        while (index < file_size && (old_data[index] == '\r' || old_data[index] == '\n')) {
            ++index;
        }

        if (line_end > line_start) {
            char saved;
            saved = old_data[line_end];
            old_data[line_end] = '\0';
            if (ascii_casecmp(old_data + line_start, "[Rename]") == 0) {
                found_section = TRUE;
                insert_offset = index;
                old_data[line_end] = saved;
                break;
            }
            old_data[line_end] = saved;
        }
    }

    if (found_section) {
        new_length = file_size + (DWORD)lstrlenA(delete_line);
        new_data = (char *)LocalAlloc(LPTR, new_length + 1);
        if (new_data == NULL) {
            if (old_data != NULL) {
                LocalFree(old_data);
            }
            return FALSE;
        }

        if (insert_offset != 0) {
            CopyMemory(new_data, old_data, insert_offset);
        }
        CopyMemory(new_data + insert_offset,
                   delete_line,
                   lstrlenA(delete_line));
        if (file_size > insert_offset) {
            CopyMemory(new_data + insert_offset + lstrlenA(delete_line),
                       old_data + insert_offset,
                       file_size - insert_offset);
        }
    } else {
        const char *header;
        DWORD header_length;
        BOOL need_newline;

        header = "[Rename]\r\n";
        header_length = (DWORD)lstrlenA(header);
        need_newline = (file_size != 0 && old_data != NULL &&
                        old_data[file_size - 1] != '\r' &&
                        old_data[file_size - 1] != '\n');

        new_length = file_size + (need_newline ? 2 : 0) +
                     header_length + (DWORD)lstrlenA(delete_line);
        new_data = (char *)LocalAlloc(LPTR, new_length + 1);
        if (new_data == NULL) {
            if (old_data != NULL) {
                LocalFree(old_data);
            }
            return FALSE;
        }

        insert_offset = 0;
        if (file_size != 0 && old_data != NULL) {
            CopyMemory(new_data, old_data, file_size);
            insert_offset = file_size;
        }
        if (need_newline) {
            new_data[insert_offset++] = '\r';
            new_data[insert_offset++] = '\n';
        }
        CopyMemory(new_data + insert_offset, header, header_length);
        insert_offset += header_length;
        CopyMemory(new_data + insert_offset,
                   delete_line,
                   lstrlenA(delete_line));
    }

    success = write_whole_file(wininit_path, new_data, new_length);
    if (!success) {
        if (old_data != NULL) {
            write_whole_file(wininit_path, old_data, file_size);
        } else {
            DeleteFileA(wininit_path);
        }
    }

    if (old_data != NULL) {
        LocalFree(old_data);
    }
    LocalFree(new_data);

    return success;
}

static BOOL delete_or_schedule(const char *path, BOOL *scheduled)
{
    DWORD attributes;

    attributes = GetFileAttributesA(path);
    if (attributes == 0xffffffffUL) {
        return TRUE;
    }

    SetFileAttributesA(path, FILE_ATTRIBUTE_NORMAL);
    if (DeleteFileA(path)) {
        return TRUE;
    }

    if (schedule_wininit_delete(path)) {
        *scheduled = TRUE;
        return TRUE;
    }

    return FALSE;
}

static BOOL write_cleanup_batch(const char *installed_setup,
                                const char *install_directory)
{
    char temp_directory[MAX_PATH];
    char batch_path[MAX_PATH];
    char short_setup[MAX_PATH];
    char short_directory[MAX_PATH];
    char short_batch[MAX_PATH];
    char command[MAX_PATH * 2];
    char batch_text[MAX_PATH * 4];
    HANDLE file;
    DWORD written;

    if (GetTempPathA(ARRAYSIZE(temp_directory), temp_directory) == 0) {
        return FALSE;
    }

    wsprintfA(batch_path,
              "%sNP2D%04X.BAT",
              temp_directory,
              (unsigned int)(GetTickCount() & 0xffff));

    if (GetShortPathNameA(installed_setup, short_setup, ARRAYSIZE(short_setup)) == 0) {
        lstrcpynA(short_setup, installed_setup, ARRAYSIZE(short_setup));
    }
    if (GetShortPathNameA(install_directory,
                          short_directory,
                          ARRAYSIZE(short_directory)) == 0) {
        lstrcpynA(short_directory, install_directory, ARRAYSIZE(short_directory));
    }

    wsprintfA(batch_text,
              "@ECHO OFF\r\n"
              ":RETRY\r\n"
              "DEL %s >NUL\r\n"
              "IF EXIST %s GOTO RETRY\r\n"
              "RD %s >NUL\r\n"
              "DEL %%0 >NUL\r\n",
              short_setup,
              short_setup,
              short_directory);

    file = CreateFileA(batch_path,
                       GENERIC_WRITE,
                       0,
                       NULL,
                       CREATE_ALWAYS,
                       FILE_ATTRIBUTE_NORMAL,
                       NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    written = 0;
    if (!WriteFile(file,
                   batch_text,
                   (DWORD)lstrlenA(batch_text),
                   &written,
                   NULL)) {
        CloseHandle(file);
        DeleteFileA(batch_path);
        return FALSE;
    }
    CloseHandle(file);

    if (GetShortPathNameA(batch_path, short_batch, ARRAYSIZE(short_batch)) == 0) {
        lstrcpynA(short_batch, batch_path, ARRAYSIZE(short_batch));
    }

    wsprintfA(command, "COMMAND.COM /C %s", short_batch);
    return (WinExec(command, SW_HIDE) > 31);
}

static BOOL install_np2idle(void)
{
    char source_directory[MAX_PATH];
    char windows_directory[MAX_PATH];
    char system_directory[MAX_PATH];
    char install_directory[MAX_PATH];
    char source_path[MAX_PATH];
    char destination_path[MAX_PATH];
    char tray_path[MAX_PATH];
    char setup_path[MAX_PATH];
    char quoted_tray[MAX_PATH + 4];
    char uninstall_command[MAX_PATH + 32];

    if (!is_windows_9x()) {
        show_simple_error("このインストーラはWindows 9x専用です。");
        return FALSE;
    }

    if (!get_module_directory(source_directory, ARRAYSIZE(source_directory)) ||
        GetWindowsDirectoryA(windows_directory, ARRAYSIZE(windows_directory)) == 0 ||
        GetSystemDirectoryA(system_directory, ARRAYSIZE(system_directory)) == 0) {
        show_error_text("インストール", "WindowsまたはSYSTEMディレクトリを取得できませんでした。");
        return FALSE;
    }

    /* Validate the distribution before changing the installed configuration. */
    if (!build_source_path(source_path, ARRAYSIZE(source_path),
                           source_directory, VXD_FILE_NAME) ||
        !file_exists(source_path) ||
        !build_source_path(source_path, ARRAYSIZE(source_path),
                           source_directory, TRAY_EXE_NAME) ||
        !file_exists(source_path) ||
        !build_source_path(source_path, ARRAYSIZE(source_path),
                           source_directory, SETUP_EXE_NAME) ||
        !file_exists(source_path)) {
        SetLastError(ERROR_FILE_NOT_FOUND);
        show_error_text("インストール",
                        "NP2IDLE.VXD、NP2ITRAY.EXE、SETUP.EXEを"
                        "同じディレクトリに置いてください。");
        return FALSE;
    }

    if (!build_install_directory(windows_directory,
                                 install_directory,
                                 ARRAYSIZE(install_directory))) {
        show_error_text("インストール先の決定", "システムドライブを取得できませんでした。");
        return FALSE;
    }

    if (!prepare_install_directory(install_directory)) {
        return FALSE;
    }

    /* An installed tray executable cannot be replaced while it is running. */
    stop_tray_controller();

    /* Remove a SYSTEM.INI device entry so registry loading is unambiguous. */
    if (!remove_legacy_system_ini_entry()) {
        show_error_text("SYSTEM.INIの移行", "旧NP2IDLEのdevice行を削除できませんでした。");
        return FALSE;
    }

    if (!build_source_path(source_path,
                           ARRAYSIZE(source_path),
                           source_directory,
                           VXD_FILE_NAME)) {
        return FALSE;
    }
    lstrcpyA(destination_path, system_directory);
    if (!path_append(destination_path,
                     ARRAYSIZE(destination_path),
                     VXD_FILE_NAME) ||
        !copy_one_file(source_path, destination_path, TRUE)) {
        return FALSE;
    }

    /* Keep a source copy beside the installed setup for repair/update. */
    lstrcpyA(destination_path, install_directory);
    if (!path_append(destination_path,
                     ARRAYSIZE(destination_path),
                     VXD_FILE_NAME) ||
        !copy_one_file(source_path, destination_path, TRUE)) {
        return FALSE;
    }

    if (!build_source_path(source_path,
                           ARRAYSIZE(source_path),
                           source_directory,
                           TRAY_EXE_NAME)) {
        return FALSE;
    }
    lstrcpyA(tray_path, install_directory);
    if (!path_append(tray_path, ARRAYSIZE(tray_path), TRAY_EXE_NAME) ||
        !copy_one_file(source_path, tray_path, TRUE)) {
        return FALSE;
    }

    if (!build_source_path(source_path,
                           ARRAYSIZE(source_path),
                           source_directory,
                           CTL_EXE_NAME)) {
        return FALSE;
    }
    lstrcpyA(destination_path, install_directory);
    if (!path_append(destination_path,
                     ARRAYSIZE(destination_path),
                     CTL_EXE_NAME) ||
        !copy_one_file(source_path, destination_path, FALSE)) {
        return FALSE;
    }

    if (!build_source_path(source_path,
                           ARRAYSIZE(source_path),
                           source_directory,
                           SETUP_EXE_NAME)) {
        return FALSE;
    }
    lstrcpyA(setup_path, install_directory);
    if (!path_append(setup_path,
                     ARRAYSIZE(setup_path),
                     SETUP_EXE_NAME) ||
        !copy_one_file(source_path, setup_path, TRUE)) {
        return FALSE;
    }

    if (!set_reg_string(HKEY_LOCAL_MACHINE,
                        VXD_REG_KEY,
                        "StaticVxD",
                        VXD_FILE_NAME) ||
        !set_reg_string(HKEY_LOCAL_MACHINE,
                        VXD_REG_KEY,
                        "Description",
                        "NP21/W Windows 9x アイドルHLT補助")) {
        show_error_text("VxDの登録", VXD_REG_KEY);
        return FALSE;
    }

    wsprintfA(quoted_tray, "\"%s\"", tray_path);
    if (!set_reg_string(HKEY_LOCAL_MACHINE,
                        RUN_REG_KEY,
                        PRODUCT_NAME,
                        quoted_tray)) {
        show_error_text("自動起動の登録", RUN_REG_KEY);
        return FALSE;
    }

    wsprintfA(uninstall_command, "\"%s\" /uninstall", setup_path);
    if (!set_reg_string(HKEY_LOCAL_MACHINE,
                        UNINSTALL_REG_KEY,
                        "DisplayName",
                        "NP2IDLE for Neko Project 21/W") ||
        !set_reg_string(HKEY_LOCAL_MACHINE,
                        UNINSTALL_REG_KEY,
                        "DisplayVersion",
                        NP2IDLE_VERSION_STRING) ||
        !set_reg_string(HKEY_LOCAL_MACHINE,
                        UNINSTALL_REG_KEY,
                        "Publisher",
                        "Neko Project 21/W utility") ||
        !set_reg_string(HKEY_LOCAL_MACHINE,
                        UNINSTALL_REG_KEY,
                        "InstallLocation",
                        install_directory) ||
        !set_reg_string(HKEY_LOCAL_MACHINE,
                        UNINSTALL_REG_KEY,
                        "DisplayIcon",
                        setup_path) ||
        !set_reg_string(HKEY_LOCAL_MACHINE,
                        UNINSTALL_REG_KEY,
                        "UninstallString",
                        uninstall_command) ||
        !set_reg_dword(HKEY_LOCAL_MACHINE,
                       UNINSTALL_REG_KEY,
                       "NoModify",
                       1) ||
        !set_reg_dword(HKEY_LOCAL_MACHINE,
                       UNINSTALL_REG_KEY,
                       "NoRepair",
                       1)) {
        show_error_text("アンインストール情報の登録", UNINSTALL_REG_KEY);
        return FALSE;
    }

    if (!g_silent) {
        if (MessageBoxA(NULL,
                        "NP2IDLEをインストールしました。\r\n\r\n"
                        "NP2IDLE.VXDを読み込むにはWindowsの再起動が必要です。\r\n"
                        "再起動後、トレイ常駐プログラムが自動的に起動します。\r\n\r\n"
                        "今すぐWindowsを再起動しますか?",
                        PRODUCT_NAME " セットアップ",
                        MB_YESNO | MB_ICONINFORMATION) == IDYES) {
            ExitWindowsEx(EWX_REBOOT, 0);
        }
    }

    return TRUE;
}

static BOOL uninstall_np2idle(void)
{
    static const char *const vxd_values[] = {
        "StaticVxD",
        "Description"
    };
    static const char *const settings_values[] = {
        "AudioHltInhibit"
    };
    static const char *const uninstall_values[] = {
        "DisplayName",
        "DisplayVersion",
        "Publisher",
        "InstallLocation",
        "DisplayIcon",
        "UninstallString",
        "NoModify",
        "NoRepair"
    };

    char windows_directory[MAX_PATH];
    char system_directory[MAX_PATH];
    char install_directory[MAX_PATH];
    char path[MAX_PATH];
    char current_module[MAX_PATH];
    char installed_setup[MAX_PATH];
    BOOL delayed_delete;
    BOOL current_is_installed_setup;

    if (!is_windows_9x()) {
        show_simple_error("このアンインストーラはWindows 9x専用です。");
        return FALSE;
    }

    if (!query_installed_path(install_directory, ARRAYSIZE(install_directory))) {
        if (GetWindowsDirectoryA(windows_directory, ARRAYSIZE(windows_directory)) == 0) {
            show_error_text("アンインストール", "Windowsディレクトリを取得できませんでした。");
            return FALSE;
        }
        if (!build_install_directory(windows_directory,
                                     install_directory,
                                     ARRAYSIZE(install_directory))) {
            return FALSE;
        }
    }

    if (GetSystemDirectoryA(system_directory, ARRAYSIZE(system_directory)) == 0) {
        show_error_text("アンインストール", "SYSTEMディレクトリを取得できませんでした。");
        return FALSE;
    }

    stop_tray_controller();
    disable_loaded_vxd();

    if (!remove_legacy_system_ini_entry()) {
        show_error_text("SYSTEM.INIの整理", "旧NP2IDLEのdevice行を削除できませんでした。");
    }

    delete_reg_value(HKEY_LOCAL_MACHINE, RUN_REG_KEY, PRODUCT_NAME);
    delete_known_key(HKEY_CURRENT_USER,
                     SETTINGS_REG_KEY,
                     settings_values,
                     (int)ARRAYSIZE(settings_values));
    delete_known_key(HKEY_LOCAL_MACHINE,
                     VXD_REG_KEY,
                     vxd_values,
                     (int)ARRAYSIZE(vxd_values));
    delete_known_key(HKEY_LOCAL_MACHINE,
                     UNINSTALL_REG_KEY,
                     uninstall_values,
                     (int)ARRAYSIZE(uninstall_values));

    delayed_delete = FALSE;

    lstrcpyA(path, system_directory);
    if (path_append(path, ARRAYSIZE(path), VXD_FILE_NAME)) {
        if (!delete_or_schedule(path, &delayed_delete)) {
            show_error_text("VxDの削除", path);
        }
    }

    lstrcpyA(path, install_directory);
    if (path_append(path, ARRAYSIZE(path), VXD_FILE_NAME)) {
        DeleteFileA(path);
    }

    lstrcpyA(path, install_directory);
    if (path_append(path, ARRAYSIZE(path), TRAY_EXE_NAME)) {
        DeleteFileA(path);
    }

    lstrcpyA(path, install_directory);
    if (path_append(path, ARRAYSIZE(path), CTL_EXE_NAME)) {
        DeleteFileA(path);
    }

    lstrcpyA(installed_setup, install_directory);
    if (!path_append(installed_setup,
                     ARRAYSIZE(installed_setup),
                     SETUP_EXE_NAME)) {
        return FALSE;
    }

    current_module[0] = '\0';
    GetModuleFileNameA(NULL, current_module, ARRAYSIZE(current_module));
    current_is_installed_setup = paths_equal(current_module, installed_setup);

    if (!current_is_installed_setup) {
        DeleteFileA(installed_setup);
        RemoveDirectoryA(install_directory);
    }

    if (!g_silent) {
        if (MessageBoxA(NULL,
                        delayed_delete ?
                        "NP2IDLEをアンインストールしました。\r\n\r\n"
                        "削除を完了するにはWindowsの再起動が必要です。\r\n\r\n"
                        "今すぐWindowsを再起動しますか?" :
                        "NP2IDLEをアンインストールしました。\r\n\r\n"
                        "Windowsを再起動すると完全に解除されます。\r\n\r\n"
                        "今すぐWindowsを再起動しますか?",
                        PRODUCT_NAME " セットアップ",
                        MB_YESNO | MB_ICONINFORMATION) == IDYES) {
            /* The installed setup executable is still mapped by this process.
             * Queue its deletion for early boot; a requested reboot needs no
             * auxiliary cleanup process. */
            if (current_is_installed_setup &&
                !schedule_wininit_delete(installed_setup)) {
                show_error_text("セットアッププログラムの削除", installed_setup);
            }
            if (ExitWindowsEx(EWX_REBOOT, 0)) {
                return TRUE;
            }
        }
    }

    /* When Windows remains running, start self-cleanup immediately before
     * exit so it can remove the setup executable and installation directory
     * after this process releases them. */
    if (current_is_installed_setup) {
        write_cleanup_batch(installed_setup, install_directory);
    }

    return TRUE;
}

static BOOL confirm_install(void)
{
    if (g_silent) {
        return TRUE;
    }

    return (MessageBoxA(NULL,
                        "NP2IDLEをインストールしますか？\r\n\r\n"
                        "インストール先はシステムドライブ直下の \\np2idle9 です。",
                        PRODUCT_NAME " セットアップ",
                        MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES);
}

static BOOL command_has_switch(const char *command_line, const char *name)
{
    const char *cursor;
    char token[64];
    int length;

    cursor = command_line;
    while (*cursor != '\0') {
        while (*cursor == ' ' || *cursor == '\t') {
            ++cursor;
        }
        if (*cursor == '\0') {
            break;
        }

        length = 0;
        while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t') {
            if (length + 1 < (int)sizeof(token)) {
                token[length++] = *cursor;
            }
            ++cursor;
        }
        token[length] = '\0';

        if (token[0] == '/' || token[0] == '-') {
            if (lstrcmpiA(token + 1, name) == 0) {
                return TRUE;
            }
        }
    }

    return FALSE;
}

int WINAPI WinMain(HINSTANCE instance,
                   HINSTANCE previous_instance,
                   LPSTR command_line,
                   int show_command)
{
    BOOL uninstall;
    BOOL install;
    int answer;

    (void)instance;
    (void)previous_instance;
    (void)show_command;

    uninstall = command_has_switch(command_line, "uninstall") ||
                command_has_switch(command_line, "u");
    install = command_has_switch(command_line, "install") ||
              command_has_switch(command_line, "i");
    g_silent = command_has_switch(command_line, "silent") ||
               command_has_switch(command_line, "s");

    if (uninstall) {
        if (!g_silent) {
            answer = MessageBoxA(NULL,
                                 "NP2IDLEをアンインストールしますか?",
                                 PRODUCT_NAME " セットアップ",
                                 MB_YESNO | MB_ICONQUESTION);
            if (answer != IDYES) {
                return 0;
            }
        }
        return uninstall_np2idle() ? 0 : 1;
    }

    if (install) {
        if (!confirm_install()) {
            return 0;
        }
        return install_np2idle() ? 0 : 1;
    }

    if (is_installed()) {
        answer = MessageBoxA(NULL,
                             "NP2IDLEは既にインストールされています。\r\n\r\n"
                             "はい: 再インストールまたは更新\r\n"
                             "いいえ: アンインストール\r\n"
                             "キャンセル: 終了",
                             PRODUCT_NAME " セットアップ",
                             MB_YESNOCANCEL | MB_ICONQUESTION);
        if (answer == IDYES) {
            return install_np2idle() ? 0 : 1;
        }
        if (answer == IDNO) {
            return uninstall_np2idle() ? 0 : 1;
        }
        return 0;
    }

    if (!confirm_install()) {
        return 0;
    }
    return install_np2idle() ? 0 : 1;
}
