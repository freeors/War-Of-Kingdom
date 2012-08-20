// #define _WIN32_WINNT		0x501
#include "stdafx.h"
#include <windowsx.h>
#include <string.h>
// #include <shlobj.h> // SHBrowseForFolder

#include "resource.h"

#include "struct.h"

static char		gtext[_MAX_PATH];

// 执行透明处理,(255, 255, 255)[0x00ffffff] ---> COLOR_BTNFACE
DWORD SetBitmapBits(uint8_t *bmBits, DWORD biBitCount, DWORD dwCount, const void* lpBits, DWORD transparentclr) 
{ 
	DWORD		rgb = GetSysColor(COLOR_BTNFACE);		// COLOR_BTNFACE, COLOR_WINDOW
	DWORD		u32n;
	uint32_t	idx = 0;
	uint8_t		r, g, b;

	r = GetRValue(rgb);
	g = GetGValue(rgb);
	b = GetBValue(rgb);

	if (biBitCount == 24) {
		for (idx = 0; idx < dwCount; idx += 3) {
			u32n = *(DWORD *)((uint8_t *)lpBits + idx) & 0xffffff;
			if (u32n == transparentclr) {
				memcpy((uint8_t *)bmBits + idx, &rgb, 3);
				bmBits[idx] = b;
				bmBits[idx + 1] = g;
				bmBits[idx + 2] = r;
			} else {
				memcpy(bmBits + idx, (uint8_t *)lpBits + idx, 3);
			}
		}
	} else {
		memcpy(bmBits, lpBits, dwCount); 
	}
	return dwCount; 
} 

void transparent_24bmp(HBITMAP hBitmap, DWORD transparentclr)
{
    BITMAP bmp; 
	GetObject(hBitmap, sizeof(BITMAP), &bmp);
    DWORD dwCount = (DWORD)bmp.bmWidthBytes * bmp.bmHeight; 

	SetBitmapBits((uint8_t *)bmp.bmBits, 24, dwCount, bmp.bmBits, transparentclr);

	return;
}


HBITMAP CreateBitmapIndirect(LPBITMAPINFO lpBitmapInfo, const void* lpSrcBits) 
{
    LPVOID		lpBits; 
    HDC			hdc = NULL;
	HBITMAP		hBitmap = NULL;

    hdc = CreateCompatibleDC(NULL);
    hBitmap = CreateDIBSection(hdc, lpBitmapInfo, DIB_RGB_COLORS, &lpBits, NULL, 0);
	DeleteDC(hdc);
	hdc = NULL;
	if (hBitmap == NULL) {
		goto exit;
	}
    	
    BITMAP bmp; 
	GetObject(hBitmap, sizeof(BITMAP), &bmp);
    DWORD dwCount = (DWORD)bmp.bmWidthBytes * bmp.bmHeight; 

	if (SetBitmapBits((uint8_t *)bmp.bmBits, lpBitmapInfo->bmiHeader.biBitCount, dwCount, lpSrcBits, 0x00ffffff) != dwCount) { 
		DeleteObject(hBitmap);
		hBitmap = NULL;
		posix_print("DIB build error!\n");
        goto exit;
    } 
	
exit:
	if (hdc) {
		DeleteDC(hdc);
	}
	return hBitmap; 

} 

HBITMAP LoadBitmap(char *fname, DWORD *biWidth, DWORD *biHeight)
{ 
	posix_file_t		fp;
	uint32_t			byte_rtd;
	uint32_t			low_file_size, high_file_size;
	LPBITMAPINFO		lpBitmap = NULL;
	LPVOID				lpBits = NULL;
	HBITMAP				hBitmap = NULL;

	posix_fopen(fname, GENERIC_READ, OPEN_EXISTING, fp);
	if (fp == INVALID_FILE) {
		return FALSE; 
    } 
	posix_fsize(fp, low_file_size, high_file_size);
	posix_fseek(fp, 0, 0);

    BITMAPFILEHEADER bfhHeader; 
    posix_fread(fp, &bfhHeader, sizeof(BITMAPFILEHEADER), byte_rtd); 

    if(bfhHeader.bfType!=((WORD) ('M'<<8)|'B')) { 
		posix_print("The file is not a BMP file!\n");
		goto exit;
    } 
 
	if(bfhHeader.bfSize != low_file_size) { 
		posix_print("The BMP file header error!\n");
		goto exit;
    } 

	UINT uBmpInfoLen = (UINT) bfhHeader.bfOffBits - sizeof(BITMAPFILEHEADER); 
    lpBitmap = (LPBITMAPINFO) new BYTE[uBmpInfoLen]; 

    posix_fread(fp, (LPVOID)lpBitmap, uBmpInfoLen, byte_rtd); 

    if((* (LPDWORD)(lpBitmap)) != sizeof(BITMAPINFOHEADER)) { 
        posix_print("The BMP is not Windows 3.0 format!"); 
		goto exit;
	}

	if (biWidth) {
		*biWidth = lpBitmap->bmiHeader.biWidth;
	}
	if (biHeight) {
		*biHeight = lpBitmap->bmiHeader.biHeight;
	}

	// 当存在色彩表时,透时化处理(修改色采表最的一项)
	if (lpBitmap->bmiHeader.biBitCount <= 8) {
		DWORD		rgb = GetSysColor(COLOR_BTNFACE);
		RGBQUAD		*rgbquad = (RGBQUAD *)((uint8_t *)lpBitmap + bfhHeader.bfOffBits - sizeof(BITMAPFILEHEADER) - 4);
		rgbquad->rgbRed = GetRValue(rgb);
		rgbquad->rgbGreen = GetGValue(rgb);
		rgbquad->rgbBlue = GetBValue(rgb);
	}

    DWORD dwBitlen = bfhHeader.bfSize - bfhHeader.bfOffBits; 
    lpBits = new BYTE[dwBitlen]; 
	posix_fread(fp, lpBits, dwBitlen, byte_rtd);

    hBitmap = CreateBitmapIndirect(lpBitmap, lpBits); 

exit:
	if (fp != INVALID_FILE) {
		posix_fclose(fp);
	}
	if (lpBitmap) {
		delete lpBitmap; 
	}
	if (lpBits) {
		delete lpBits; 
	}
  
	return hBitmap;
} 


// 对话框消息处理函数
BOOL On_DlgAboutInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	// 别试图在WM_INITDIALOG中加载位图，不能显示。

	sprintf(gtext, "%s Desktop Software", PROG_NAME);
	Edit_SetText(GetDlgItem(hdlgP, IDC_ST_ABOUT_PRODUCTNAME), gtext);

	sprintf(gtext, "%s Firmware", PROG_NAME);
	Edit_SetText(GetDlgItem(hdlgP, IDC_ST_ABOUT_FWVERSION), gtext);

	Edit_SetText(GetDlgItem(hdlgP, IDC_ST_ABOUT_CORPNAME), gdmgr._corp_name);
	Edit_SetText(GetDlgItem(hdlgP, IDC_ST_ABOUT_HTTP), gdmgr._corp_http);
/*
    HWND hwnd = CreateWindowEx(0, (LPCTSTR)WC_LINK, 
        (LPCTSTR)L"For more information, <A HREF=\"http://www.microsoft.com\">click here</A> " \
        L"or <A ID=\"idInfo\">here</A>.", 
        WS_VISIBLE | WS_CHILD | WS_TABSTOP, 
        10, 100, 16, 16, 
        hdlgP, NULL, gdmgr._hinst, NULL);
	DWORD err = GetLastError();
	ShowWindow(hwnd, SW_SHOW);
*/
	return FALSE;
}

void On_DlgAboutCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	switch(id) {
	case IDC_BT_ABOUT_HELP:
		break;
	default:
		break;
	}
	return;
}

void On_DlgAboutPaint(HWND hdlgP) 
{
	HDC			DC, memDC;
	HBITMAP		hbitmap;
	DWORD		biWidth = 128, biHeight = 128;
	
	// hbitmap = (HBITMAP)LoadImage(gdmgr._hinst, gdmgr._markbmp, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION | LR_LOADTRANSPARENT);
	hbitmap = LoadBitmap(gdmgr._markbmp, &biWidth, &biHeight);

	DC = GetDC(hdlgP);
	memDC = CreateCompatibleDC(DC);
	SelectObject(memDC, hbitmap);
	BitBlt(DC, 0, 20, biWidth, biHeight, memDC, 0, 0, SRCCOPY);
	ReleaseDC(hdlgP, DC);
	DeleteDC(memDC);
	DeleteObject(hbitmap);

	UpdateWindow(hdlgP);
	return;
} 

BOOL CALLBACK DlgAboutProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	LPPSHNOTIFY			lppsn = (LPPSHNOTIFY)lParam;

	switch(uMsg)
	{
	case WM_INITDIALOG:
		return On_DlgAboutInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgAboutCommand);
	HANDLE_MSG(hdlgP, WM_PAINT, On_DlgAboutPaint);
	case WM_NOTIFY:
		switch(lppsn->hdr.code) {
		case PSN_SETACTIVE: // page gaining focus
			break;
		case PSN_KILLACTIVE: // page losing focus
			break;
		case PSN_APPLY: // Apply or OK code here
		case PSN_RESET: // Cancel code here
			break;
		}
	}
	
	return FALSE;
}

