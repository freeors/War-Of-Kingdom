#include "stdafx.h"
#include <windowsx.h>

#include "resource.h"
#include "struct.h"
#include "win32x.h"

static char	gtext[_MAX_PATH];

static HWND init_toolbar_sys(HINSTANCE hinst, HWND hdlgP);

#define title_enable_sync_btn(fEnable)	ToolBar_EnableButton(gdmgr._htb_sys, IDM_SYS_SYNC, fEnable)
#define title_enable_wgen_btn(fEnable)	ToolBar_EnableButton(gdmgr._htb_sys, IDM_SYS_WGEN, fEnable)
#define title_enable_xchg_btn(fEnable)	ToolBar_EnableButton(gdmgr._htb_sys, IDM_SYS_CORE, fEnable)
#define title_enable_play_btn(fEnable)	ToolBar_EnableButton(gdmgr._htb_sys, IDM_SYS_PLAY, fEnable)
#define title_enable_update_btn(fEnable)	ToolBar_EnableButton(gdmgr._htb_sys, IDM_SYS_TBOX, fEnable)
#define title_enable_terrain_btn(fEnable)	ToolBar_EnableButton(gdmgr._htb_sys, IDM_SYS_BUILDER, fEnable)

void title_enable_ui(BOOL fEnable)
{
	// title_enable_sync_btn(fEnable);
	title_enable_sync_btn(TRUE);
	title_enable_wgen_btn(fEnable);
	title_enable_xchg_btn(fEnable);
	title_enable_play_btn(fEnable);
	title_enable_update_btn(fEnable);
	title_enable_terrain_btn(fEnable);

	return;
}

BOOL On_DlgTitleInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	init_toolbar_sys(gdmgr._hinst, hdlgP);

	return TRUE;
}

void On_DlgTitleCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	switch (id) {
	case IDM_SYS_SYNC:
		title_select(da_sync);
		break;
	case IDM_SYS_WGEN:
		title_select(da_wgen);
		break;
	case IDM_SYS_CORE:
		title_select(da_core);
		break;
	case IDM_SYS_PLAY:
		title_select(da_visual);
		break;
	case IDM_SYS_TBOX:
		title_select(da_campaign);
		break;
	default:
		break;
	}
	return;
}

BOOL On_DlgTitleNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	// NM_表示对通用控件都通用,范围丛(0, 99)
	// TVN_表示只能TreeView通用,范围丛(400, 499)
    if (lpNMHdr->code == TTN_GETDISPINFO) { 
            
        LPTOOLTIPTEXT lpttt; 

        lpttt = (LPTOOLTIPTEXT) lpNMHdr; 
        lpttt->hinst = gdmgr._hinst; 

        // Specify the resource identifier of the descriptive 
        // text for the given button. 
		if (lpttt->hdr.idFrom == IDM_SYS_SYNC) {
            lpttt->lpszText = MAKEINTRESOURCE(IDM_SYS_SYNC); 
            
		} else if (lpttt->hdr.idFrom == IDM_SYS_WGEN) {
            lpttt->lpszText = MAKEINTRESOURCE(IDM_SYS_WGEN); 
            
		} else if (lpttt->hdr.idFrom == IDM_SYS_CORE) {
            lpttt->lpszText = MAKEINTRESOURCE(IDM_SYS_CORE); 

        } else if (lpttt->hdr.idFrom == IDM_SYS_TBOX) {
            lpttt->lpszText = MAKEINTRESOURCE(IDM_SYS_TBOX);

        } else if (lpttt->hdr.idFrom == IDM_SYS_BUILDER) {
            lpttt->lpszText = MAKEINTRESOURCE(IDM_SYS_BUILDER); 

        } else if (lpttt->hdr.idFrom == IDM_SYS_PLAY) {
            lpttt->lpszText = MAKEINTRESOURCE(IDM_SYS_PLAY); 
        } 
    }

    return FALSE;
}

BOOL CALLBACK DlgTitleProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_INITDIALOG:
		return On_DlgTitleInitDialog(hdlgP, (HWND)wParam, lParam);		
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgTitleCommand);
	HANDLE_MSG(hdlgP, WM_NOTIFY,  On_DlgTitleNotify);
	}
	return FALSE;
}

// ------------------------------------------------------------------------
HWND init_toolbar_sys(HINSTANCE hinst, HWND hdlgP)
{
	int		idx = 0;

	// Create a toolbar
	gdmgr._htb_sys = CreateWindowEx(0, TOOLBARCLASSNAME, (LPSTR)NULL, 
		WS_CHILD | CCS_ADJUSTABLE | TBSTYLE_TOOLTIPS | TBSTYLE_FLAT | TBSTYLE_CHECK, 0, 0, 0, 0, hdlgP, 
		(HMENU)IDR_SYSMENU, gdmgr._hinst, NULL);

	// Send the TB_BUTTONSTRUCTSIZE message, which is required for backward compatibility
	ToolBar_ButtonStructSize(gdmgr._htb_sys, sizeof(TBBUTTON));

	ToolBar_SetButtonSize(gdmgr._htb_sys, 80, 80);

	gdmgr._tbBtns_sys[idx].iBitmap = MAKELONG(gdmgr._iico_title_sync, 0);
	// 为了工具提示作到简单，IDM_FILE_SAVE最好就是字串，它就是String Table中一个值
	gdmgr._tbBtns_sys[idx].idCommand = IDM_SYS_SYNC;	
	gdmgr._tbBtns_sys[idx].fsState = TBSTATE_ENABLED;
	gdmgr._tbBtns_sys[idx].fsStyle = TBSTYLE_BUTTON;
	gdmgr._tbBtns_sys[idx].dwData = 0L;
	gdmgr._tbBtns_sys[idx].iString = 0;

	idx ++;
	gdmgr._tbBtns_sys[idx].iBitmap = MAKELONG(gdmgr._iico_title_wgen, 0);
	gdmgr._tbBtns_sys[idx].idCommand = IDM_SYS_WGEN;
	gdmgr._tbBtns_sys[idx].fsState = TBSTATE_ENABLED;
	gdmgr._tbBtns_sys[idx].fsStyle = TBSTYLE_BUTTON;
	gdmgr._tbBtns_sys[idx].dwData = 0L;
	gdmgr._tbBtns_sys[idx].iString = 0;

	idx ++;
	gdmgr._tbBtns_sys[idx].iBitmap = MAKELONG(gdmgr._iico_title_xchg, 0);
	gdmgr._tbBtns_sys[idx].idCommand = IDM_SYS_CORE;
	gdmgr._tbBtns_sys[idx].fsState = TBSTATE_ENABLED;
	gdmgr._tbBtns_sys[idx].fsStyle = TBSTYLE_BUTTON;
	gdmgr._tbBtns_sys[idx].dwData = 0L;
	gdmgr._tbBtns_sys[idx].iString = 0;

	idx ++;
	gdmgr._tbBtns_sys[idx].iBitmap = MAKELONG(gdmgr._iico_title_play, 0);
	gdmgr._tbBtns_sys[idx].idCommand = IDM_SYS_PLAY;
	gdmgr._tbBtns_sys[idx].fsState = 0;
	gdmgr._tbBtns_sys[idx].fsStyle = TBSTYLE_BUTTON;
	gdmgr._tbBtns_sys[idx].dwData = 0L;
	gdmgr._tbBtns_sys[idx].iString = 0;

	idx ++;
	gdmgr._tbBtns_sys[idx].iBitmap = MAKELONG(gdmgr._iico_title_update, 0);
	gdmgr._tbBtns_sys[idx].idCommand = IDM_SYS_TBOX;
	gdmgr._tbBtns_sys[idx].fsState = TBSTATE_ENABLED;
	gdmgr._tbBtns_sys[idx].fsStyle = TBSTYLE_BUTTON;
	gdmgr._tbBtns_sys[idx].dwData = 0L;
	gdmgr._tbBtns_sys[idx].iString = 0;

	idx ++;
	gdmgr._tbBtns_sys[idx].iBitmap = MAKELONG(gdmgr._iico_title_about, 0);
	gdmgr._tbBtns_sys[idx].idCommand = IDM_SYS_BUILDER;
	gdmgr._tbBtns_sys[idx].fsState = TBSTATE_ENABLED;
	gdmgr._tbBtns_sys[idx].fsStyle = TBSTYLE_BUTTON;
	gdmgr._tbBtns_sys[idx].dwData = 0L;
	gdmgr._tbBtns_sys[idx].iString = 0;

	ToolBar_AddButtons(gdmgr._htb_sys, TB_BTNS_COUNT_SYS, &gdmgr._tbBtns_sys);

	ToolBar_AutoSize(gdmgr._htb_sys);
	
	ToolBar_SetExtendedStyle(gdmgr._htb_sys, TBSTYLE_EX_DRAWDDARROWS);
	
	ToolBar_SetImageList(gdmgr._htb_sys, gdmgr._himl_80x80, 0);
	ToolBar_SetDisabledImageList(gdmgr._htb_sys, gdmgr._himl_80x80_dis);
	
	ShowWindow(gdmgr._htb_sys, SW_SHOW);

	return gdmgr._htb_sys;
}

// --------------------------------------------------
void title_select(do_action_t da)
{
	if (gdmgr._da == da) {
		return;
	}

	if (gdmgr._da == da_sync) {
		sync_hide_ui();
	} else if (gdmgr._da == da_wgen) {
#ifndef _ROSE_EDITOR
		wgen_hide_ui();
#endif
	} else if (gdmgr._da == da_core) {
		if (!core_hide_ui()) {
			return;
		}
	} else if (gdmgr._da == da_visual) {
		visual_hide_ui();
	} else if (gdmgr._da == da_campaign) {
#ifndef _ROSE_EDITOR
		if (!campaign_hide_ui()) {
			return;
		}
#endif
	} else if (gdmgr._da == da_integrate) {
		if (!integrate_hide_ui()) {
			return;
		}
	}
	
	ToolBar_CheckButton(gdmgr._htb_sys, IDM_SYS_SYNC, 0);
	ToolBar_CheckButton(gdmgr._htb_sys, IDM_SYS_WGEN, 0);
	ToolBar_CheckButton(gdmgr._htb_sys, IDM_SYS_CORE, 0);
	ToolBar_CheckButton(gdmgr._htb_sys, IDM_SYS_PLAY, 0);
	ToolBar_CheckButton(gdmgr._htb_sys, IDM_SYS_TBOX, 0);
	ToolBar_CheckButton(gdmgr._htb_sys, IDM_SYS_BUILDER, 0);

	ShowWindow(gdmgr._hdlg_sync, SW_HIDE);
	ShowWindow(gdmgr._hdlg_wgen, SW_HIDE);
	ShowWindow(gdmgr._hdlg_core, SW_HIDE);
	ShowWindow(gdmgr._hdlg_visual, SW_HIDE);
	ShowWindow(gdmgr._hdlg_campaign, SW_HIDE);
	ShowWindow(gdmgr._hdlg_integrate, SW_HIDE);

	// 记住当前正在执行的会话
	gdmgr._da = da;

	if (da == da_sync) {
		ShowWindow(gdmgr._hdlg_sync, SW_RESTORE);
		ToolBar_CheckButton(gdmgr._htb_sys, IDM_SYS_SYNC, 1);
		sync_enter_ui();
	
	} else if (da == da_wgen) {
		ShowWindow(gdmgr._hdlg_wgen, SW_RESTORE);
		ToolBar_CheckButton(gdmgr._htb_sys, IDM_SYS_WGEN, 1);
#ifndef _ROSE_EDITOR
		wgen_enter_ui();
#endif

	} else if (da == da_core) {
		ShowWindow(gdmgr._hdlg_core, SW_RESTORE);
		ToolBar_CheckButton(gdmgr._htb_sys, IDM_SYS_CORE, 1);
		core_enter_ui();

	} else if (da == da_visual) {
		ShowWindow(gdmgr._hdlg_visual, SW_RESTORE);
		ToolBar_CheckButton(gdmgr._htb_sys, IDM_SYS_PLAY, 1);
		visual_enter_ui();

	} else if (da == da_campaign) {
		ShowWindow(gdmgr._hdlg_campaign, SW_RESTORE);
		ToolBar_CheckButton(gdmgr._htb_sys, IDM_SYS_TBOX, 1);
#ifndef _ROSE_EDITOR
		campaign_enter_ui();
#endif

	} else if (da == da_integrate) {
		ShowWindow(gdmgr._hdlg_integrate, SW_RESTORE);
		integrate_enter_ui();
	}
	return;
}

bool can_execute_tack(int task)
{
	if (gdmgr._da == da_core) {
		if (!core_can_execute_tack(task)) {
			return false;
		}
	} else if (gdmgr._da == da_campaign) {
#ifndef _ROSE_EDITOR
		if (!campaign_can_execute_tack(task)) {
			return false;
		}
#endif
	}
	return true;
}