#include <windows.h>
#include "options.h"

struct _options options;
CHAR opts_full_path[MAX_PATH] = OPTSFILE;

void
options_load()
{
    CHAR *slash;

    // get programm path
    GetModuleFileName(NULL, opts_full_path, sizeof(opts_full_path));
    slash = strrchr(opts_full_path, '\\');

    if (slash == NULL)
        return;

    *(slash + 1) = '\0';
    strcat_s(opts_full_path, sizeof(opts_full_path), OPTSFILE);

    GetPrivateProfileString(APPNAME, "patch_list", NULL,
        options.patch_list, sizeof(options.patch_list), opts_full_path);
    GetPrivateProfileString(APPNAME, "motd_file", NULL,
        options.motd_file, sizeof(options.motd_file), opts_full_path);
    GetPrivateProfileString(APPNAME, "base_url", NULL,
        options.base_url, sizeof(options.base_url), opts_full_path);
    GetPrivateProfileString(APPNAME, "run_command", NULL,
        options.run_command, sizeof(options.run_command), opts_full_path);
    GetPrivateProfileString(APPNAME, "run_command2", NULL,
        options.run_command2, sizeof(options.run_command2), opts_full_path);

    GetPrivateProfileString(APPNAME, "proxy_address", NULL,
        options.proxy_address, sizeof(options.proxy_address), opts_full_path);
    GetPrivateProfileString(APPNAME, "proxy_auth_user", NULL,
        options.proxy_auth_user, sizeof(options.proxy_auth_user), opts_full_path);
    GetPrivateProfileString(APPNAME, "proxy_auth_password", NULL,
        options.proxy_auth_password, sizeof(options.proxy_auth_password), opts_full_path);

    options.proxy_enabled = GetPrivateProfileInt("TehPatcher", "proxy_enabled", 0, opts_full_path) ? TRUE : FALSE;
    options.proxy_port = GetPrivateProfileInt("TehPatcher", "proxy_port", 3128, opts_full_path);
    options.proxy_auth_required = GetPrivateProfileInt("TehPatcher", "proxy_auth_required", 0, opts_full_path) ? TRUE : FALSE;
    options.last_patch = GetPrivateProfileInt("TehPatcher", "last_patch", 0, opts_full_path);
}

void
options_save()
{
    CHAR proxy_port[16], proxy_enabled[16], proxy_auth_required[16];

    _itoa_s(options.proxy_enabled, proxy_enabled, sizeof(proxy_enabled), 10);
    _itoa_s(options.proxy_port, proxy_port, sizeof(proxy_port), 10);
    _itoa_s(options.proxy_auth_required, proxy_auth_required, sizeof(proxy_auth_required), 10);

    WritePrivateProfileString(APPNAME, "proxy_address", options.proxy_address, opts_full_path);
    WritePrivateProfileString(APPNAME, "proxy_auth_user", options.proxy_auth_user, opts_full_path);
    WritePrivateProfileString(APPNAME, "proxy_auth_password", options.proxy_auth_password, opts_full_path);
    WritePrivateProfileString(APPNAME, "proxy_enabled", proxy_enabled, opts_full_path);
    WritePrivateProfileString(APPNAME, "proxy_port", proxy_port, opts_full_path);
    WritePrivateProfileString(APPNAME, "proxy_auth_required", proxy_auth_required, opts_full_path);
}

void
options_save_last_patch()
{
    char last_patch[16];
    _itoa_s(options.last_patch, last_patch, sizeof(last_patch), 10);
    WritePrivateProfileString(APPNAME, "last_patch", last_patch, opts_full_path);
}
