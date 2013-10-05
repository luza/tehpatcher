#pragma once
#include <windows.h>

#define OPTSFILE	"patcher.ini"
#define APPNAME		"TehPatcher"

#define GRF_REPACK_EDGE		3 * 1024 * 1024 // 32 Mb

struct _options {
	CHAR base_url[64];
	CHAR patch_list[64];
	CHAR motd_file[64];
	BOOL proxy_enabled;
	CHAR proxy_address[32];
	INT proxy_port;
	BOOL proxy_auth_required;
	CHAR proxy_auth_user[32];
	CHAR proxy_auth_password[32];
	INT last_patch;
	CHAR run_command[64];
	CHAR run_command2[64];
};

extern struct _options options;
extern CHAR opts_full_path[MAX_PATH];

void options_load();
void options_save();
void options_save_last_patch();
