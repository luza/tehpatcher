#include <windows.h>
#include <commctrl.h>
#include <stdio.h>

#include "resource.h"
#include "dialog.h"
#include "tehpatcher.h"
#include "options.h"

HWND hMainDialog = NULL;
DWORD progress = 0, last_progress = 0;
WCHAR pb_text[256] = { 0 };

struct {
	INT day;
	INT month;
	INT year;
	WCHAR text[2048];
} motd =
	{ 0, -1, 0, 0 };

CHAR month_names[12][10] = {
	"€нвар€", "феврал€", "марта", "апрел€", "ма€", "июн€",
	"июл€", "августа", "сент€бр€", "окт€бр€", "но€бр€", "декабр€"
};

#define CLEARTYPE_QUALITY 5

void
dialog_paint_motd(HDC hDC)
{
	HANDLE hFont;
	CHAR date_line[64];
	RECT r = { 300, 90, 565, 330 };

	// invalid month
	if (motd.month < 1 || motd.month > 12)
		return;

	SetBkMode(hDC, TRANSPARENT);

	// title
	hFont = CreateFont(16, 0, 0, 0, FW_BOLD, FALSE, TRUE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
                CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, TEXT("Tahoma"));
	SelectObject(hDC, hFont);
	sprintf_s(date_line, sizeof(date_line), "%d %s %d", motd.day, month_names[motd.month-1], motd.year);
	DrawText(hDC, date_line, -1, &r, DT_WORDBREAK);

	r.top = 115;

	// text
	hFont = CreateFont(16, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
                CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, TEXT("Tahoma"));
	SelectObject(hDC, hFont);
	DrawTextW(hDC, motd.text, -1, &r, DT_WORDBREAK);
}

void
dialog_paint_pb(HDC hDC)
{
	HDC tdc, memdc;
	HBITMAP tbmp, membmp;
	HFONT hFont;

	memdc = CreateCompatibleDC(hDC);
	membmp = CreateCompatibleBitmap(hDC, 565, 23);
	SelectObject(memdc, membmp);

	// paint background
	tbmp = LoadBitmap(GetModuleHandle(NULL), MAKEINTRESOURCE(IDB_BG));
	tdc = CreateCompatibleDC(hDC);
	SelectObject(tdc, tbmp);
	BitBlt(memdc, 0, 0, 565, 23, tdc, 16, 334, SRCCOPY);
	DeleteDC(tdc);
	DeleteObject(tbmp);

	// paint foreground
	tbmp = LoadBitmap(GetModuleHandle(NULL), MAKEINTRESOURCE(IDB_PB));
	tdc = CreateCompatibleDC(hDC);
	SelectObject(tdc, tbmp);
	BitBlt(memdc, 0, 0, progress * 565 / 100, 23, tdc, 0, 0, SRCCOPY);
	DeleteDC(tdc);
	DeleteObject(tbmp);

	// paint text
	hFont = CreateFont(16, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
                CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, TEXT("Tahoma"));

	SetBkMode(memdc, TRANSPARENT);
	SetTextAlign(memdc, TA_CENTER);
	SelectObject(memdc, hFont);

	TextOutW(memdc, 282, 3, pb_text, (INT)wcslen(pb_text));
	DeleteObject(hFont);

	// paint membmp onto window
	BitBlt(hDC, 16, 334, 565, 23, memdc, 0, 0, SRCCOPY);
	DeleteDC(memdc);
	DeleteObject(membmp);

	last_progress = progress;
}

LRESULT WINAPI
dialog_main_proc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_INITDIALOG:
		{
			HICON hIcon;

			hIcon = LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON),
				IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
			SendMessage(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);

			hIcon = LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON),
				IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0);
			SendMessage(hDlg, WM_SETICON, ICON_BIG, (LPARAM)hIcon);

			hMainDialog = hDlg;

			// start patching process
			main_start_routine();
		}
		break;

		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HBITMAP bmpBg;
			HDC hDC, memDCBg;
			
			hDC = BeginPaint(hDlg, &ps);

			// Load the bitmap from the resource
			bmpBg = LoadBitmap(GetModuleHandle(NULL), MAKEINTRESOURCE(IDB_BG));
			
			// Create a memory device compatible with the above DC variable
			memDCBg = CreateCompatibleDC(hDC);
			
			// Select the new bitmap
			SelectObject(memDCBg, bmpBg);

			// Copy the bits from the memory DC into the current dc
			BitBlt(hDC, 0, 0, 600, 400, memDCBg, 0, 0, SRCCOPY);

			// Restore the old bitmap
			DeleteDC(memDCBg);
			DeleteObject(bmpBg);

			dialog_paint_pb(hDC);
			dialog_paint_motd(hDC);

			EndPaint(hDlg, &ps);
		}
		break;

		case WM_COMMAND:
		{
			if (LOWORD(wParam) == IDOK ||
				LOWORD(wParam) == IDOK2)
			{
				char *command = options.run_command;
				if (LOWORD(wParam) != IDOK)
					command = options.run_command2;
				if (WinExec(command, SW_SHOW) <= 31)
				{
					CHAR mes[256];

					EnableWindow(GetDlgItem(hMainDialog, IDOK), FALSE);
					EnableWindow(GetDlgItem(hMainDialog, IDOK2), FALSE);
					sprintf_s(mes, sizeof(mes), "Ќевозможно выполнить: %s", command);
					dialog_set_status(mes);
					return FALSE;
				}

				EndDialog(hDlg, LOWORD(wParam));
				return TRUE;
			}
			else if (LOWORD(wParam) == IDCANCEL)
			{
				EndDialog(hDlg, LOWORD(wParam));
				return TRUE;
			}
			else if (LOWORD(wParam) == IDSETTINGS)
			{
				dialog_settings(NULL, hDlg);
			}
		}
		break;
	}

    return FALSE;
}

LRESULT WINAPI
dialog_settings_proc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_INITDIALOG:
		{
			CHAR port_tmp[16];

			SetWindowText(GetDlgItem(hDlg, IDC_ADDRESS), options.proxy_address);
			SetWindowText(GetDlgItem(hDlg, IDC_USERNAME), options.proxy_auth_user);
			SetWindowText(GetDlgItem(hDlg, IDC_PASSWORD), options.proxy_auth_password);

			SendMessage(GetDlgItem(hDlg, IDC_PROXY), BM_SETCHECK,
				options.proxy_enabled ? BST_CHECKED : BST_UNCHECKED, 0);

			SendMessage(GetDlgItem(hDlg, IDC_AUTH), BM_SETCHECK,
				options.proxy_auth_required ? BST_CHECKED : BST_UNCHECKED, 0);

			_itoa_s(options.proxy_port, port_tmp, sizeof(port_tmp), 10);
			SetWindowText(GetDlgItem(hDlg, IDC_PORT), port_tmp);

			// actualize enabled/disabled controls state
			dialog_settings_proc(hDlg, WM_COMMAND, IDC_PROXY, 0);
		}
		break;

		case WM_COMMAND:
		{
			if (LOWORD(wParam) == IDOK) 
			{
				CHAR port_tmp[16];
				LRESULT proxy = SendMessage(GetDlgItem(hDlg, IDC_PROXY), BM_GETCHECK, 0, 0),
					auth = SendMessage(GetDlgItem(hDlg, IDC_AUTH), BM_GETCHECK, 0, 0);

				options.proxy_enabled = (proxy == BST_CHECKED);
				options.proxy_auth_required = (auth == BST_CHECKED);

				GetWindowText(GetDlgItem(hDlg, IDC_ADDRESS), options.proxy_address,
					sizeof(options.proxy_address));
				GetWindowText(GetDlgItem(hDlg, IDC_USERNAME), options.proxy_auth_user,
					sizeof(options.proxy_auth_user));
				GetWindowText(GetDlgItem(hDlg, IDC_PASSWORD), options.proxy_auth_password,
					sizeof(options.proxy_auth_password));

				GetWindowText(GetDlgItem(hDlg, IDC_PORT), port_tmp, sizeof(port_tmp));
				options.proxy_port = atoi(port_tmp);

				options_save();

				EndDialog(hDlg, LOWORD(wParam));
				return TRUE;
			}
			else if (LOWORD(wParam) == IDCANCEL)
			{
				EndDialog(hDlg, LOWORD(wParam));
				return TRUE;
			}
			else if (LOWORD(wParam) == IDC_PROXY ||
					 LOWORD(wParam) == IDC_AUTH)
			{
				LRESULT proxy = SendMessage(GetDlgItem(hDlg, IDC_PROXY), BM_GETCHECK, 0, 0),
					auth = SendMessage(GetDlgItem(hDlg, IDC_AUTH), BM_GETCHECK, 0, 0);

				EnableWindow(GetDlgItem(hDlg, IDC_ADDRESS), (proxy == BST_CHECKED));
				EnableWindow(GetDlgItem(hDlg, IDC_PORT), (proxy == BST_CHECKED));
				EnableWindow(GetDlgItem(hDlg, IDC_AUTH), (proxy == BST_CHECKED));

				EnableWindow(GetDlgItem(hDlg, IDC_USERNAME), (proxy == BST_CHECKED && auth == BST_CHECKED));
				EnableWindow(GetDlgItem(hDlg, IDC_PASSWORD), (proxy == BST_CHECKED && auth == BST_CHECKED));
			}
		}
		break;
	}

    return FALSE;
}

void
dialog_main(HINSTANCE instance, HWND hWnd)
{
	INITCOMMONCONTROLSEX InitCtrlEx;

	InitCtrlEx.dwSize = sizeof(INITCOMMONCONTROLSEX);
	InitCtrlEx.dwICC  = ICC_PROGRESS_CLASS;
	InitCommonControlsEx(&InitCtrlEx);

	OleInitialize(NULL);
	DialogBox(instance, (LPCTSTR)IDD_MAINDIALOG, hWnd, (DLGPROC)dialog_main_proc);
	OleUninitialize();
}

void
dialog_settings(HINSTANCE instance, HWND hWnd)
{
	DialogBox(instance, (LPCTSTR)IDD_OPTSDIALOG, hWnd, (DLGPROC)dialog_settings_proc);
}

void
dialog_set_motd(CHAR *text, INT day, INT month, INT year)
{
	HDC hDC;

	motd.day = day;
	motd.month = month;
	motd.year = year;

	MultiByteToWideChar(CP_UTF8, 0, text, -1, motd.text, sizeof(motd.text));

	// repaint motd
	hDC = GetDC(hMainDialog);
	dialog_paint_motd(hDC);
	ReleaseDC(hMainDialog, hDC);
}

void
dialog_set_status(CHAR *status)
{
	HDC hDC;
	size_t numconverted;

	//_mbstowcs_s_l(&numconverted, pb_text, sizeof(pb_text)/sizeof(WCHAR),
	//	status, sizeof(pb_text)/sizeof(WCHAR) - 1, );
	mbstowcs_s(&numconverted, pb_text, sizeof(pb_text)/sizeof(WCHAR),
		status, sizeof(pb_text)/sizeof(WCHAR) - 1);

	// repaint pb
	hDC = GetDC(hMainDialog);
	dialog_paint_pb(hDC);
	ReleaseDC(hMainDialog, hDC);
}

void
dialog_set_status_w(WCHAR *status)
{
	HDC hDC;

	wcscpy_s(pb_text, sizeof(pb_text), status);

	// repaint pb
	hDC = GetDC(hMainDialog);
	dialog_paint_pb(hDC);
	ReleaseDC(hMainDialog, hDC);
}

void
dialog_set_progress(DWORD val)
{
	HDC hDC;

	progress = val;

	// repaint pb
	hDC = GetDC(hMainDialog);
	dialog_paint_pb(hDC);
	ReleaseDC(hMainDialog, hDC);
}

void
dialog_enable_start()
{
	EnableWindow(GetDlgItem(hMainDialog, IDOK), TRUE);
	EnableWindow(GetDlgItem(hMainDialog, IDOK2), TRUE);
}
