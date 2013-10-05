#pragma once
#include <windows.h>

void dialog_main(HINSTANCE instance, HWND hWnd);
void dialog_settings(HINSTANCE instance, HWND hWnd);
void dialog_set_motd(CHAR *text, INT day, INT month, INT year);
void dialog_set_status(CHAR *status);
void dialog_set_status_w(WCHAR *status);
void dialog_set_progress(DWORD val);
void dialog_enable_start();
