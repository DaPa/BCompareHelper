// BCompareHelper.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <windows.h>
#include <iostream>
#include <winreg.h>
#include <shellapi.h>
#include <processenv.h>
#include <strsafe.h>


const wchar_t* version = L"version 0.0.1 - 8 Mar 2024";
const int MAX_STR_LEN = 1024;
static wchar_t msg_buf[MAX_STR_LEN]; // used in many places for string formating
static bool debug = false;


static int check_str_len(const wchar_t* buf) 
{
    size_t len = 0;
    // determines whether a string exceeds the specified length, in bytes.
    if (FAILED(StringCbLengthW(buf, sizeof(wchar_t) * MAX_STR_LEN, &len))) return 0;
    return (int)(len / sizeof(wchar_t));
}


static void show_message(const wchar_t* msg)
{
    MessageBeep(MB_ICONERROR);
    if (debug) {
        wprintf(L"%s\n", msg);
        std::cin.get();
    } else {
        MessageBox(NULL, msg, L"BCompareHelper", MB_OK | MB_ICONERROR);
    }
}


static void show_last_error(const wchar_t* error_context, DWORD dw_error = 0) 
{
    if (dw_error == 0) {
        // get the last-error code
        dw_error = GetLastError();
    }

    // retrieve the system error message for the last-error code
    FormatMessage(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw_error,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        msg_buf,
        MAX_STR_LEN,
        NULL);
    
    // Display the error message
    wchar_t display_buf[2 * MAX_STR_LEN];
    wsprintf(display_buf, L"%s!\nError code = %d (0x%08x), detail: %s", error_context ? error_context : L"...", dw_error, dw_error, msg_buf);
    show_message(display_buf);
}


static void show_usage(void)
{
    wsprintf(msg_buf, L"Usage: BCompareHelper <file/directory path> left|compare [debug]\n"
        "%s\n"
        "  left    => \"Select Left File\"\n"
        "  compare => \"Compare to\"\n"
        "  debug   => \"keep debug console open\"\n", version);
    show_message(msg_buf);
}

static int is_valid_path(const wchar_t* path, bool* is_directory, int* path_len)
{
    *path_len = check_str_len(path);
    if (!*path_len) {
        wsprintf(msg_buf, L"path is zero-length or exceeds %d characters!", MAX_STR_LEN);
        show_message(msg_buf);
        return 3;
    }

    DWORD ftyp = GetFileAttributesW(path);
    if (ftyp == INVALID_FILE_ATTRIBUTES) {
        show_last_error(L"GetFileAttributesW");
        return 4;
    }

    *is_directory = (ftyp & FILE_ATTRIBUTE_DIRECTORY) ? true : false;
    return 0;
}

static int check_args(int num_args, const wchar_t** str_args, bool* is_directory, bool* is_saved_left, int* path_len)
{
    debug = true;
    if (num_args == 1) {
        show_usage();
        return 1;
    }

    if ((num_args < 3) || (num_args > 4)) {
        wsprintf(msg_buf, L"Expected 2 or 3 command line arguments, found %d!", num_args - 1);
        show_message(msg_buf);
        show_usage();
        return 2;
    }

    debug = (num_args == 4) && (0 == wcscmp(str_args[3], L"debug"));

    int error = is_valid_path(str_args[1], is_directory, path_len);
    if (error) {
        return error;
    }
    
    const wchar_t* left = str_args[2];
    int len = check_str_len(left);
    if (!len) {
        wsprintf(msg_buf, L"Passed command line argument 2 exceeds %d characters!", MAX_STR_LEN);
        show_message(msg_buf);
        show_usage();
        return 5;
    }

    *is_saved_left = (0 == wcscmp(str_args[2], L"left"));
    if ((!*is_saved_left) && (0 != wcscmp(left, L"compare"))) {
        wsprintf(msg_buf, L"Passed command line argument 2 should be 'left' or 'compare', but it's '%s'!", left);
        show_message(msg_buf);
        show_usage();
        return 6;
    }

    return 0;
}


static int compare(HKEY hRegKey, const wchar_t* path, int path_len, bool is_directory, bool is_saved_left)
{
    DWORD err_nr;
    const wchar_t* subkey = L"SavedLeft";
    if (!is_saved_left) {
        // check if the other registry key is already set!
        DWORD len = 0;
        DWORD dwType; // REG_SZ
        err_nr = (DWORD)RegGetValueW(hRegKey, NULL, subkey, RRF_RT_REG_SZ, &dwType, NULL, &len);
        if (ERROR_SUCCESS != err_nr) {
            if (ERROR_FILE_NOT_FOUND != err_nr) {
                wsprintf(msg_buf, L"RegQueryValueW @ '%s'", subkey);
                show_last_error(msg_buf, err_nr);
                return 8;
            } else {
                wsprintf(msg_buf, L"Compared asked, but no saved left file!");
                show_message(msg_buf);
                return 9;
            }
        } else if (dwType != REG_SZ) {
            wsprintf(msg_buf, L"RegQueryValueW @ '%s' expected %d type, but found %d", subkey, REG_SZ, dwType);
            show_message(msg_buf);
            return 10;
        } else if (len > MAX_STR_LEN) {
            wsprintf(msg_buf, L"RegQueryValueW @ '%s' returned %d > %d", subkey, len, MAX_STR_LEN);
            show_message(msg_buf);
            return 11;
        } else {
            wchar_t left_buf[MAX_STR_LEN];
            len = MAX_STR_LEN;
            err_nr = (DWORD)RegGetValueW(hRegKey, NULL, subkey, RRF_RT_REG_SZ, NULL, left_buf, &len);
            if (ERROR_SUCCESS != err_nr) {
                wsprintf(msg_buf, L"RegQueryValueW @ '%s'", subkey);
                show_last_error(msg_buf, err_nr);
                return 12;
            }

            bool is_left_directory;
            if (left_buf[0] == 'D') {
                is_left_directory = true;
            } else if (left_buf[0] == 'F') {
                is_left_directory = false;
            } else {
                wsprintf(msg_buf, L"%s starts with unrecognized character %c", subkey, left_buf[0]);
                show_message(msg_buf);
                return 13;
            }

            if (is_directory != is_left_directory) {
                wsprintf(msg_buf, L"File/Directory comparison mismatch (%d vs %d)", is_directory, is_left_directory);
                show_message(msg_buf);
                return 14;
            }

            bool check_is_left_directory;
            int check_left_path_len;
            const wchar_t* left_path = &left_buf[1];
            int error = is_valid_path(left_path, &check_is_left_directory, &check_left_path_len);
            if (error) {
                return 20 + error;
            }

            if (check_is_left_directory != is_left_directory) {
                wsprintf(msg_buf, L"Saved left File/Directory comparison mismatch (%d vs %d)", check_is_left_directory, is_left_directory);
                show_message(msg_buf);
                return 15;
            }

            wchar_t params_buf[2 * MAX_STR_LEN + 256];
            wsprintf(params_buf, L"\"%s\" \"%s\"", left_path, path);
            // launch BCompare with left_buf and path
            /*
                ShellExecuteW possible error codes:
                ERROR_FILE_NOT_FOUND
                ERROR_PATH_NOT_FOUND
                ERROR_BAD_FORMAT
                SE_ERR_ACCESSDENIED
                SE_ERR_ASSOCINCOMPLETE
                SE_ERR_DDEBUSY
                SE_ERR_DDEFAIL
                SE_ERR_DDETIMEOUT
                SE_ERR_DLLNOTFOUND
                SE_ERR_FNF
                SE_ERR_NOASSOC
                SE_ERR_OOM
                SE_ERR_PNF
                SE_ERR_SHARE
            */
            err_nr = (DWORD)(size_t)ShellExecuteW(NULL, L"open", L"BCompare", params_buf, NULL, SW_SHOW);
            if (err_nr <= SE_ERR_DLLNOTFOUND) {
                wsprintf(msg_buf, L"ShellExecuteW");
                show_last_error(msg_buf);
                return 16;
            }

            err_nr = (DWORD)RegDeleteValueW(hRegKey, subkey);
            if (ERROR_SUCCESS != err_nr) {
                wsprintf(msg_buf, L"RegDeleteValueW @ '%s'", subkey);
                show_last_error(msg_buf, err_nr);
                return 17;
            }
        }

    } else { // is_saved_left == true!

        wchar_t save_left_buf[MAX_STR_LEN + 1];
        wsprintf(save_left_buf, L"%c%s", is_directory ? 'D' : 'F', path);
        err_nr = (DWORD)RegSetKeyValueW(hRegKey, NULL, subkey, REG_SZ, save_left_buf, sizeof(wchar_t) * (path_len + 2U));
        if (ERROR_SUCCESS != err_nr) {
            wsprintf(msg_buf, L"RegSetKeyValueW @ '%s'", subkey);
            show_last_error(msg_buf, err_nr);
            return 18;
        }

    }

    // success
    return 0;
}

int main()
{
    int num_args;
    wchar_t** str_args = CommandLineToArgvW(GetCommandLineW(), &num_args);
    
    bool is_directory;
    bool is_saved_left;
    int path_len;
    int error = check_args(num_args, (const wchar_t**)str_args, &is_directory, &is_saved_left, &path_len);
    if (error) return error;
    if(!debug) FreeConsole();

    HKEY hRegKey;
    const wchar_t* key = L"SOFTWARE\\Scooter Software\\Beyond Compare 4\\BcShellEx";
    DWORD err_nr = (DWORD)RegOpenKeyExW(HKEY_CURRENT_USER, key, 0, KEY_READ | KEY_WRITE, &hRegKey);
    if (ERROR_SUCCESS != err_nr) {
        wsprintf(msg_buf, L"RegOpenKeyExW @ '%s'", key);
        show_last_error(msg_buf, err_nr);
        return 7;
    }

    error = compare(hRegKey, str_args[1], path_len, is_directory, is_saved_left);

    err_nr = (DWORD)RegCloseKey(hRegKey);
    if (ERROR_SUCCESS != err_nr) {
        wsprintf(msg_buf, L"RegCloseKey");
        show_last_error(msg_buf, err_nr);
        return 19;
    }

    return error;
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
