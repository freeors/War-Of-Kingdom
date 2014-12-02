#define GETTEXT_DOMAIN "wesnoth-maker"

#include "global.hpp"
#include "game_config.hpp"
#include "loadscreen.hpp"
#include "filesystem.hpp"
#include "editor.hpp"
#include "gettext.hpp"
#include "stdafx.h"
#include <windowsx.h>

#include "resource.h"
#include "struct.h"
#include "xfunc.h"
#include "win32x.h"
#include <utility>

#include "language.hpp"
#include <boost/foreach.hpp>

extern editor editor_;

static void sync_update_task_desc(HWND hctl);
static void do_sync();
static HWND init_toolbar_sync(HINSTANCE hinst, HWND hdlgP);

namespace ns {
	extern HIMAGELIST himl_checkbox;
}

#define sync_enable_refresh_btn(fEnable)	ToolBar_EnableButton(gdmgr._htb_sync, IDM_REFRESH, fEnable)
#define sync_enable_sync_btn(fEnable)	ToolBar_EnableButton(gdmgr._htb_sync, IDM_SYNC_SYNC, fEnable)
#define sync_enable_openpcdir_btn(fEnable)	ToolBar_EnableButton(gdmgr._htb_sync, IDM_SYNC_OPENPCDIR, fEnable)

void sync_enable_ui(BOOL fEnable)
{
	sync_enable_refresh_btn(fEnable);
	sync_enable_sync_btn(fEnable);
	sync_enable_openpcdir_btn(fEnable);

	return;
}

void sync_enter_ui(void) 
{
	StatusBar_Trans();

	// 保留给将来使用按钮,当前版本全都灰掉
	ToolBar_EnableButton(gdmgr._htb_sync, IDM_REFRESH, TRUE);
	ToolBar_EnableButton(gdmgr._htb_sync, IDM_SYNC_SYNC, FALSE);

	HWND hctl = GetDlgItem(gdmgr._htb_sync, IDC_CMB_SYNC_LANGUAGE);
	ComboBox_ResetContent(hctl);
	const std::vector<language_def>& languages = get_languages();
	const language_def& current_language = get_language();
	BOOST_FOREACH (const language_def& lang, languages) {
		ComboBox_AddString(hctl, utf8_2_ansi(lang.language.c_str()));
		if (lang == current_language) {
			ComboBox_SetCurSel(hctl, ComboBox_GetCount(hctl) - 1);
		}
	}

	sync_update_task_desc(GetDlgItem(gdmgr._hdlg_sync, IDC_LV_SYNC_SYNC));

	ns::himl_checkbox = ImageList_Create(15, 15, FALSE, 2, 0);
	ImageList_SetBkColor(ns::himl_checkbox, RGB(255, 255, 255));

    HICON hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_UNCHECK));
    ImageList_AddIcon(ns::himl_checkbox, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_CHECK));
    ImageList_AddIcon(ns::himl_checkbox, hicon);

	ListView_SetImageList(GetDlgItem(gdmgr._hdlg_sync, IDC_LV_SYNC_SYNC), ns::himl_checkbox, LVSIL_STATE);
}

BOOL sync_hide_ui(void)
{
	ImageList_Destroy(ns::himl_checkbox);
	return TRUE;
}

void sync_update_task_desc(HWND hctl)
{
	ListView_DeleteAllItems(hctl);
	if (!file_exists(game_config::path + "/data/core/_main.cfg") || !is_directory(game_config::path + "/xwml")) {
		return;
	}

#ifndef _ROSE_EDITOR
	editor_.reload_campaigns_cfg();
#endif

	char text[_MAX_PATH];
	editor_.get_wml2bin_desc_from_wml(game_config::path + "/data");
	const std::vector<std::pair<editor::BIN_TYPE, editor::wml2bin_desc> >& descs = editor_.wml2bin_descs();

	LVITEM lvi;

	for (std::vector<std::pair<editor::BIN_TYPE, editor::wml2bin_desc> >::const_iterator itor = descs.begin(); itor != descs.end(); ++ itor) {
		lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
		lvi.iItem = ListView_GetItemCount(hctl);
		lvi.iSubItem = 0;
		strcpy(text, itor->second.bin_name.c_str());
		lvi.pszText = text;
		lvi.lParam = itor->first;
		if (itor->second.wml_nfiles == itor->second.bin_nfiles && itor->second.wml_sum_size == itor->second.bin_sum_size && itor->second.wml_modified == itor->second.bin_modified) {
			lvi.iImage = gdmgr._iico_with_sync;
		} else {
			lvi.iImage = gdmgr._iico_without_sync;
		}
		ListView_InsertItem(hctl, &lvi);

		// (wml)checksum
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 1;
		sprintf(text, "(%u, %u, %u)", itor->second.wml_nfiles, itor->second.wml_sum_size, itor->second.wml_modified);
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);
		ListView_SetCheckState(hctl, lvi.iItem, FALSE);

		// (bin)checksum
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 2;
		sprintf(text, "(%u, %u, %u)", itor->second.bin_nfiles, itor->second.bin_sum_size, itor->second.bin_modified);
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);
		
		// sha1
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 4;
		strcpy(text, itor->second.sha1.c_str());
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);
		ListView_SetCheckState(hctl, lvi.iItem, FALSE);

		if (itor->first == editor::MAIN_DATA && (itor->second.wml_nfiles != itor->second.bin_nfiles || itor->second.wml_sum_size != itor->second.bin_sum_size || itor->second.wml_modified != itor->second.bin_modified)) {
			ListView_SetCheckState(hctl, lvi.iItem, TRUE);
		}
		if (itor->first == editor::SCENARIO_DATA && (itor->second.wml_nfiles != itor->second.bin_nfiles || itor->second.wml_sum_size != itor->second.bin_sum_size || itor->second.wml_modified != itor->second.bin_modified)) {
			const std::string id = offextname(itor->second.bin_name.c_str());
			if (editor_config::campaign_id.empty() || id == editor_config::campaign_id) {
				ListView_SetCheckState(hctl, lvi.iItem, TRUE);
			}
		}
	}
	if (editor_config::ListView_GetCheckedCount(hctl)) {
		sync_enable_sync_btn(TRUE);
	}
}

BOOL On_DlgSyncInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	HWND hctl = GetDlgItem(hdlgP, IDC_LV_SYNC_SYNC);
	LVCOLUMN lvc;
	char text[_MAX_PATH];

	init_toolbar_sync(gdmgr._hinst, hdlgP);
	SetParent(GetDlgItem(hdlgP, IDC_STATIC_SYNC_LANGUAGE), gdmgr._htb_sync);
	SetParent(GetDlgItem(hdlgP, IDC_CMB_SYNC_LANGUAGE), gdmgr._htb_sync);

	gdmgr._hdlg_sync = hdlgP;

	Static_SetText(GetDlgItem(gdmgr._htb_sync, IDC_STATIC_SYNC_LANGUAGE), utf8_2_ansi(_("Need restart if changed")));

	//
	// 初始化列视图控件
	//
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 140;
	strcpy(text, utf8_2_ansi(_("File name")));
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = 0;
	ListView_InsertColumn(hctl, 0, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 170;
	lvc.iSubItem = 1;
	sprintf(text, "(wml)%s", utf8_2_ansi(_("Checksum")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 1, &lvc);

	lvc.cx = 170;
	lvc.iSubItem = 2;
	sprintf(text, "(bin)%s", utf8_2_ansi(_("Checksum")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 2, &lvc);

	lvc.cx = 80;
	lvc.iSubItem = 3;
	strcpy(text, utf8_2_ansi(_("Status")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 3, &lvc);

	lvc.cx = 250;
	lvc.iSubItem = 4;
	strcpy(text, utf8_2_ansi(_("SHA1")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 4, &lvc);

	ListView_SetImageList(hctl, gdmgr._himl, LVSIL_SMALL);

	// 默认情况下，鼠标右键只是光亮该行的最前一个子项，并且只有在该子项上才能触发NM_RCLICK。改为光亮整行，并且在整行都能触发NM_RCLICK。
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	return FALSE;
}

void exe_pc_exe(char *cmdline, BOOL fSync)
{
	BOOL fOk = FALSE;
	
	PROCESS_INFORMATION		pi;
	STARTUPINFO				si;
	ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
	ZeroMemory(&si, sizeof(STARTUPINFO));
	si.cb = sizeof(si);


	fOk = CreateProcess(NULL, cmdline, NULL, NULL, FALSE,
		NORMAL_PRIORITY_CLASS, NULL,
		NULL, &si, &pi);
	if (fOk) {
		if (fSync) {
			// 像MediaPlayer,要同步就有得等了
			WaitForSingleObject(pi.hProcess, INFINITE);
		}
	} else {
		MessageBox(NULL, formatstr("%s Fail", cmdline), PROG_NAME, MB_OK);
	}

	// 此时应用程序应该已经退出
	CloseHandle(pi.hProcess);

	return;
}

void OnSyncCmb(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	if (codeNotify != CBN_SELCHANGE) {
		return;
	}

	int res = ComboBox_GetCurSel(hwndCtrl);
	const std::vector<language_def>& languages = get_languages();
	::set_language(languages[res]);
	preferences::set_language(languages[res].localename);
}


void On_DlgSyncCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	char text[_MAX_PATH];

	switch (id) {
	case IDM_REFRESH:
		sync_update_task_desc(GetDlgItem(hdlgP, IDC_LV_SYNC_SYNC));
		break;

	case IDM_SYNC_SYNC:
		do_sync();
		break;

	case IDC_CMB_SYNC_LANGUAGE:
		OnSyncCmb(hdlgP, id, hwndCtrl, codeNotify);
		break;

	case IDM_SYNC_OPENPCDIR:
		sprintf(text, "explorer \"%s\"", game_config::path.c_str());
		exe_pc_exe(text, TRUE);
		break;

	default:
		break;
	}

	return;
}
//
// On_DlgSyncNotify()
//
BOOL On_DlgSyncNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	LPNMITEMACTIVATE lpnmitem;
	LVITEM lvi;

	if (gdmgr._syncing) {
		return FALSE;
	}

	// NM_表示对通用控件都通用,范围丛(0, 99)
	// TVN_表示只能TreeView通用,范围丛(400, 499)
    if (lpNMHdr->code == NM_CLICK) {
		// 只对IDC_CMB_SYNC_SYNC控件有效
		if (DlgItem != IDC_LV_SYNC_SYNC) {
			goto exit;
		}
		lpnmitem = (LPNMITEMACTIVATE)lpNMHdr;
		// 如果单击到的是复选框位置,把复选框状态置反
		// 当前定义的图标大小是16x16. ptAction反回的(x,y)是整个列表视图内的坐标,因而y值不大好判断
		// 认为如果x是小于16的就认为是击中复选框
		if ((lpNMHdr->idFrom == IDC_LV_SYNC_SYNC) && (lpnmitem->ptAction.x <= 14)) {
			if (ListView_GetCheckState(lpNMHdr->hwndFrom, lpnmitem->iItem)) {
				// 当前是选中,要改为不选中
				lvi.iItem = lpnmitem->iItem;
				lvi.mask = LVIF_PARAM | LVIF_IMAGE;
				lvi.iSubItem = 0;
				ListView_GetItem(lpNMHdr->hwndFrom, &lvi);
				// 要改变的是MAIN_DATA, 并且是不同步的, 不允许修改, 一定要被汇编
				if (lvi.iImage == gdmgr._iico_with_sync || lvi.lParam != editor::MAIN_DATA) {
					ListView_SetCheckState(lpNMHdr->hwndFrom, lpnmitem->iItem, FALSE);
					// 检查视图列表中有没有需要同步的,如果没有,灰掉同步钮
					if (!editor_config::ListView_GetCheckedCount(lpNMHdr->hwndFrom)) {
						sync_enable_sync_btn(FALSE);	// 没有选中项 
					}
				}
			} else {
				// 2.既然已是选中，那就肯定有同步项了
				ListView_SetCheckState(lpNMHdr->hwndFrom, lpnmitem->iItem, TRUE);
				// 1.若是在执行同步，坚决不要使能同步按钮
				sync_enable_sync_btn(TRUE);	// 存在同步项
			}
		}

	} else if (lpNMHdr->code == NM_RCLICK) {
		// 只对IDC_CMB_SYNC_SYNC控件有效

	} else if (lpNMHdr->code == NM_DBLCLK) {
		// 只对IDC_CMB_SYNC_SYNC控件有效

	} else if (lpNMHdr->code == TTN_GETDISPINFO) { 
            
        LPTOOLTIPTEXT lpttt; 

        lpttt = (LPTOOLTIPTEXT) lpNMHdr; 
        lpttt->hinst = gdmgr._hinst; 

        // Specify the resource identifier of the descriptive 
        // text for the given button. 
		if (lpttt->hdr.idFrom == IDM_REFRESH) {
            lpttt->lpszText = MAKEINTRESOURCE(IDM_REFRESH); 
            
		} else if (lpttt->hdr.idFrom == IDM_SYNC_SYNC) {
            lpttt->lpszText = MAKEINTRESOURCE(IDM_SYNC_SYNC); 
            
		} else if (lpttt->hdr.idFrom == IDM_SYNC_OPENPCDIR) {
            lpttt->lpszText = MAKEINTRESOURCE(IDM_SYNC_OPENPCDIR); 
        } 
    }

exit:
    return FALSE;
}

void On_DlgSyncDestroy(HWND hdlgP)
{
	return;
}

BOOL CALLBACK DlgSyncProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_INITDIALOG:
		return On_DlgSyncInitDialog(hdlgP, (HWND)wParam, lParam);		
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgSyncCommand);
	HANDLE_MSG(hdlgP, WM_NOTIFY,  On_DlgSyncNotify);
	HANDLE_MSG(hdlgP, WM_DESTROY, On_DlgSyncDestroy);
	}
	return FALSE;
}

// 
// 功能函数
//

typedef struct {
	char text[_MAX_PATH];
	size_t nfiles;
	std::vector<std::pair<editor::BIN_TYPE, editor::wml2bin_desc> >::const_iterator itor;
} sync_ctx_t;

void increment_progress_cb(std::string const &name, uint32_t param1, void* param2)
{
	sync_ctx_t* ctx = (sync_ctx_t*)param2;
	// status text
	sprintf(ctx->text, "%u/%u(%s)", ctx->nfiles, ctx->itor->second.wml_nfiles, name.c_str());
	StatusBar_SetText(gdmgr._hwnd_status, 2, ctx->text);
	// progress bar
	ProgressBar_SetPos(gdmgr._hpb_task, ctx->nfiles);
	// increment 
	ctx->nfiles ++;
}

DWORD WINAPI ThdSyncProc(LPVOID ctx)
{
	HWND hctl = GetDlgItem(gdmgr._hdlg_sync, IDC_LV_SYNC_SYNC);
	LVITEM lvi;
	int count, iItem;
	char text[_MAX_PATH];
	uint32_t nfiles, sum_size, modified;
	HWND hctlPB = gdmgr._hpb_task;
	const std::vector<std::pair<editor::BIN_TYPE, editor::wml2bin_desc> >& descs = editor_.wml2bin_descs();
	sync_ctx_t increment_ctx;

	MakeDirectory(game_config::path + "/xwml/campaigns");

	gdmgr._syncing = TRUE;

	set_increment_progress progress(increment_progress_cb, &increment_ctx);

	// 执行过了同步,虽然同步列表里可能还存在能同步的同步项,但免得用户再次按下(这里又会执行那结选中的同步),灰掉
	// 强制用户必须按刷新按钮
	sync_enable_sync_btn(FALSE);
	// 灰掉相关按钮
	sync_enable_refresh_btn(FALSE);
	Button_Enable(GetDlgItem(gdmgr._hdlg_ddesc, IDC_BT_DDESC_BROWSE), FALSE);

	count = ListView_GetItemCount(hctl);
/*
	// if user checked one [campaign].bin, checked MAIN_BIN automaticly
	if (!ListView_GetCheckState(hctl, editor::MAIN_DATA)) {
		for (iItem = editor::BIN_SYSTEM_MAX + 1; iItem < count; iItem ++) {
			if (ListView_GetCheckState(hctl, iItem)) {
				// user chedked this item
				ListView_SetCheckState(hctl, editor::MAIN_DATA, TRUE);
				break;
			}
		}
	}
*/
	for (iItem = 0; iItem < count; iItem ++) {
		lvi.iItem = iItem;
		lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE | LVIF_STATE;
		lvi.iSubItem = 0;
		lvi.stateMask = (UINT)-1;
		lvi.pszText = text;
		lvi.cchTextMax = _MAX_PATH;
		ListView_GetItem(hctl, &lvi);

		if (!ListView_GetCheckState(hctl, iItem)) {
			// 用户选择不执行操作
			continue;
		}
		increment_ctx.nfiles = 0;
		// increment_ctx.itor = descs.find((editor::BIN_TYPE)(lvi.lParam));
		for (increment_ctx.itor = descs.begin(); increment_ctx.itor != descs.end(); ++ increment_ctx.itor) {
			if (!strcmp(increment_ctx.itor->second.bin_name.c_str(), text)) {
				break;
			}
		}
		ProgressBar_SetRange(gdmgr._hpb_task, 0, increment_ctx.itor->second.wml_nfiles);

		lvi.iItem = iItem;
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 3;
		lvi.pszText = "Executing...";
		ListView_SetItem(hctl, &lvi);

		// execute
		if (editor_.load_game_cfg((editor::BIN_TYPE)(lvi.lParam), text, true, increment_ctx.itor->second.wml_nfiles, increment_ctx.itor->second.wml_sum_size, (uint32_t)increment_ctx.itor->second.wml_modified)) {
			lvi.pszText = "SUCCESS";
		} else {
			lvi.pszText = "FAIL";
		}
		ListView_SetItem(hctl, &lvi);

		// (bin)checksum
		lvi.iSubItem = 2;
		std::string bin_from_path = game_config::path + "/xwml/";
		if ((editor::BIN_TYPE)(lvi.lParam) == editor::SCENARIO_DATA) {
			bin_from_path = game_config::path + "/xwml/campaigns/";
		}
		if (!wml_checksum_from_file(bin_from_path + text, &nfiles, &sum_size, &modified)) {
			nfiles = sum_size = modified = 0;
		}
		sprintf(text, "(%u, %u, %u)", nfiles, sum_size, modified);
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// image
		lvi.mask = LVIF_IMAGE;
		lvi.iSubItem = 0;
		if (increment_ctx.itor->second.wml_nfiles == nfiles && increment_ctx.itor->second.wml_sum_size == sum_size && increment_ctx.itor->second.wml_modified == modified) {
			lvi.iImage = gdmgr._iico_with_sync;
		} else {
			lvi.iImage = gdmgr._iico_without_sync;
		}
		ListView_SetItem(hctl, &lvi);
	}

	ProgressBar_SetRange(gdmgr._hpb_task, 0, 0);
	StatusBar_SetText(gdmgr._hwnd_status, 2, "");
	sync_enable_refresh_btn(TRUE);
	Button_Enable(GetDlgItem(gdmgr._hdlg_ddesc, IDC_BT_DDESC_BROWSE), TRUE);

	gdmgr._syncing = FALSE;
	editor_config::campaign_id.clear();

	return 0;
}

void do_sync()
{
	DWORD			dwThreadID;

	if (gdmgr._hthdSync) {
		// 2、曾经运行过任务，现在已执行完了任务
		WaitForSingleObject(gdmgr._hthdSync, INFINITE);
		CloseHandle(gdmgr._hthdSync);
		gdmgr._hthdSync = NULL;
	}

	gdmgr._hthdSync = CreateThread(NULL, 0, ::ThdSyncProc, NULL, 0, &dwThreadID);

	return;
}

DWORD WINAPI ThdSync2Proc(LPVOID ctx)
{
	HWND hctlPB = gdmgr._hpb_task;
	sync_ctx_t increment_ctx;

	StatusBar_Trans();

	MakeDirectory(game_config::path + "/xwml/campaigns");

	gdmgr._syncing = TRUE;

	set_increment_progress progress(increment_progress_cb, &increment_ctx);

	ToolBar_EnableButton(gdmgr._htb_sys, IDM_SYS_SYNC, FALSE);
	Button_Enable(GetDlgItem(gdmgr._hdlg_ddesc, IDC_BT_DDESC_BROWSE), FALSE);

	EnableWindow(GetDlgItem(gdmgr._hdlg_ddesc, IDC_TV_DDESC_EXPLORER), FALSE);
	if (gdmgr._da == da_core) {
		EnableWindow(gdmgr._hdlg_core, FALSE);
	} else if (gdmgr._da == da_campaign) {
		EnableWindow(gdmgr._hdlg_campaign, FALSE);
	}

	editor_.get_wml2bin_desc_from_wml(game_config::path + "/data");
	const std::vector<std::pair<editor::BIN_TYPE, editor::wml2bin_desc> >& descs = editor_.wml2bin_descs();
	for (std::vector<std::pair<editor::BIN_TYPE, editor::wml2bin_desc> >::const_iterator it = descs.begin(); it != descs.end(); ++ it) {
		bool building = false;
		if (it->first == editor::MAIN_DATA && (it->second.wml_nfiles != it->second.bin_nfiles || it->second.wml_sum_size != it->second.bin_sum_size || it->second.wml_modified != it->second.bin_modified)) {
			building = true;
		}
		if (it->first == editor::SCENARIO_DATA && (it->second.wml_nfiles != it->second.bin_nfiles || it->second.wml_sum_size != it->second.bin_sum_size || it->second.wml_modified != it->second.bin_modified)) {
			std::string id = offextname(it->second.bin_name.c_str());
			if (id == editor_config::campaign_id) {
				building = true;
			}
		}
		if (!building) {
			continue;
		}

		increment_ctx.nfiles = 0;
		increment_ctx.itor = it;

		ProgressBar_SetRange(gdmgr._hpb_task, 0, increment_ctx.itor->second.wml_nfiles);

		// execute
		int bin_type = increment_ctx.itor->first;
		bool success = editor_.load_game_cfg((editor::BIN_TYPE)bin_type, increment_ctx.itor->second.bin_name.c_str(), true, increment_ctx.itor->second.wml_nfiles, increment_ctx.itor->second.wml_sum_size, (uint32_t)increment_ctx.itor->second.wml_modified);
	}

	ProgressBar_SetRange(gdmgr._hpb_task, 0, 0);
	StatusBar_SetText(gdmgr._hwnd_status, 2, "");
	ToolBar_EnableButton(gdmgr._htb_sys, IDM_SYS_SYNC, TRUE);
	Button_Enable(GetDlgItem(gdmgr._hdlg_ddesc, IDC_BT_DDESC_BROWSE), TRUE);

	EnableWindow(GetDlgItem(gdmgr._hdlg_ddesc, IDC_TV_DDESC_EXPLORER), TRUE);
	if (gdmgr._da == da_core) {
		EnableWindow(gdmgr._hdlg_core, TRUE);
	} else if (gdmgr._da == da_campaign) {
		EnableWindow(gdmgr._hdlg_campaign, TRUE);
	}

	gdmgr._syncing = FALSE;
	editor_config::campaign_id.clear();

	// don' call statement(core_enter_ui etc.) may result operator window.
	// window function(DestroyWindow etc.) will fail because this thread is'n same as create window thread.
	return 0;
}

void do_sync2()
{
	DWORD			dwThreadID;

	if (gdmgr._hthdSync) {
		// 2、曾经运行过任务，现在已执行完了任务
		WaitForSingleObject(gdmgr._hthdSync, INFINITE);
		CloseHandle(gdmgr._hthdSync);
		gdmgr._hthdSync = NULL;
	}

	gdmgr._hthdSync = CreateThread(NULL, 0, ::ThdSync2Proc, NULL, 0, &dwThreadID);

	return;
}

void sync_refresh_sync()
{
	if (gdmgr._da != da_sync) {
		title_select(da_sync);
	} else {
		// forbid refresh
		sync_enter_ui();
	}

	do_sync();
}

// ------------------------------------------------------------------------
HWND init_toolbar_sync(HINSTANCE hinst, HWND hdlgP)
{
	// Create a toolbar
	gdmgr._htb_sync = CreateWindowEx(0, TOOLBARCLASSNAME, (LPSTR)NULL, 
		WS_CHILD | CCS_ADJUSTABLE | TBSTYLE_TOOLTIPS | TBSTYLE_FLAT, 0, 0, 0, 0, hdlgP, 
		(HMENU)IDR_SYNCMENU, gdmgr._hinst, NULL);

	// Send the TB_BUTTONSTRUCTSIZE message, which is required for backward compatibility
	ToolBar_ButtonStructSize(gdmgr._htb_sync, sizeof(TBBUTTON));

	gdmgr._tbBtns_sync[0].iBitmap = MAKELONG(gdmgr._iico_refresh, 0);
	gdmgr._tbBtns_sync[0].idCommand = IDM_REFRESH;	
	gdmgr._tbBtns_sync[0].fsState = TBSTATE_ENABLED;
	gdmgr._tbBtns_sync[0].fsStyle = TBSTYLE_BUTTON;
	gdmgr._tbBtns_sync[0].dwData = 0L;
	gdmgr._tbBtns_sync[0].iString = 0;

	gdmgr._tbBtns_sync[1].iBitmap = -1;
	gdmgr._tbBtns_sync[1].idCommand = 0;	
	gdmgr._tbBtns_sync[1].fsState = 0;
	gdmgr._tbBtns_sync[1].fsStyle = TBSTYLE_SEP;
	gdmgr._tbBtns_sync[1].dwData = 0L;
	gdmgr._tbBtns_sync[1].iString = 0;

	gdmgr._tbBtns_sync[2].iBitmap = MAKELONG(gdmgr._iico_build, 0);
	gdmgr._tbBtns_sync[2].idCommand = IDM_SYNC_SYNC;
	gdmgr._tbBtns_sync[2].fsState = TBSTATE_ENABLED;
	gdmgr._tbBtns_sync[2].fsStyle = TBSTYLE_BUTTON;
	gdmgr._tbBtns_sync[2].dwData = 0L;
	gdmgr._tbBtns_sync[2].iString = 0;

	gdmgr._tbBtns_sync[3].iBitmap = -1;
	gdmgr._tbBtns_sync[3].idCommand = 0;	
	gdmgr._tbBtns_sync[3].fsState = 0;
	gdmgr._tbBtns_sync[3].fsStyle = TBSTYLE_SEP;
	gdmgr._tbBtns_sync[3].dwData = 0L;
	gdmgr._tbBtns_sync[3].iString = 0;

	gdmgr._tbBtns_sync[4].iBitmap = 60;
	gdmgr._tbBtns_sync[4].idCommand = 0;
	gdmgr._tbBtns_sync[4].fsState = 0;
	gdmgr._tbBtns_sync[4].fsStyle = TBSTYLE_SEP;
	gdmgr._tbBtns_sync[4].dwData = 0L;
	gdmgr._tbBtns_sync[4].iString = 0;

	gdmgr._tbBtns_sync[5].iBitmap = MAKELONG(gdmgr._iico_openpcdir, 0);
	gdmgr._tbBtns_sync[5].idCommand = IDM_SYNC_OPENPCDIR;
	gdmgr._tbBtns_sync[5].fsState = TBSTATE_ENABLED;
	gdmgr._tbBtns_sync[5].fsStyle = TBSTYLE_BUTTON;
	gdmgr._tbBtns_sync[5].dwData = 0L;
	gdmgr._tbBtns_sync[5].iString = 0;

	ToolBar_AddButtons(gdmgr._htb_sync, TB_BTNS_COUNT_SYNC, &gdmgr._tbBtns_sync);

	ToolBar_AutoSize(gdmgr._htb_sync);
	
	ToolBar_SetExtendedStyle(gdmgr._htb_sync, TBSTYLE_EX_DRAWDDARROWS);
	
	ToolBar_SetImageList(gdmgr._htb_sync, gdmgr._himl_24x24, 0);
	ToolBar_SetDisabledImageList(gdmgr._htb_sync, gdmgr._himl_24x24_dis);
	
	ShowWindow(gdmgr._htb_sync, SW_SHOW);

	return gdmgr._htb_sync;
}