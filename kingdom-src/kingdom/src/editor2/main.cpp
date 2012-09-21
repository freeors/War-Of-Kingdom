#include "global.hpp"
#include "malloc.h"
#include "game_config.hpp"
#include "filesystem.hpp"
#include "editor.hpp"

#include "stdafx.h"
#include <windowsx.h>
#include "resource.h"

#include "xfunc.h"
#include "win32x.h"
#include "struct.h"

#include <shlobj.h>	// CSIDL_PROGRAM_FILES

#include "wesconfig.h"
#include <clocale>
#include "gettext.hpp"


extern BOOL CALLBACK DlgTitleProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam);
extern BOOL CALLBACK DlgDDescProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam);
extern BOOL CALLBACK DlgSyncProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam);
extern BOOL CALLBACK DlgWGenProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam);
extern BOOL CALLBACK DlgTbProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam);
extern BOOL CALLBACK DlgCfgProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam);
extern BOOL CALLBACK DlgTBoxProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam);
extern BOOL CALLBACK DlgAboutProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

#define CLASSNAME			"wokeditor"

static char			gtext[_MAX_PATH];
dvrmgr_t			gdmgr;

char				gszSrcFileInCmdLine[_MAX_PATH];

editor editor_;

// oem.ini相关
#define OEM_INI						"oem.ini"
#define SECNAME_PRODUCT				"product"
#define SECNAME_CORP				"corporation"

#define KEYNAME_CORP_NAME			"name"
#define KEYNAME_CORP_HTTP			"http"
#define KEYNAME_CORP_OEM_DESC		"oem_desc"
#define KEYNAME_PRODUCT_NAME		"name"


#define EDITOR_INI					"editor.ini"
#define MARK_BMP					"mark.bmp"
#define PC_BMP						"pc.bmp"
#define DVR_BMP						"dvr.bmp"
#define EDITOR_ICO					"editor.ico"
#define WELCOME_BMP					"welcome.bmp"
#define CF_BMP						"cf.bmp"
#define NETWORK_BMP					"network.bmp"

void init_dvrmgr_struct(void) 
{
	int			retval;
	char		text[_MAX_PATH];

	// oemini文件全路径名
	GetCurrentExePath(gdmgr._curexedir);
	SHGetFolderPath(NULL, CSIDL_PERSONAL, NULL, 0, text);
	sprintf(gdmgr._userdir, "%s\\My Games\\kingdom", text);
	MakeDirectory(std::string(gdmgr._userdir));
	
	SHGetFolderPath(NULL, CSIDL_PROGRAM_FILES, NULL, 0, gdmgr._programfilesdir);
	sprintf(gdmgr._fulloemini, "%s\\%s", gdmgr._curexedir, OEM_INI);
	sprintf(gdmgr._fulldvrmgrini, "%s\\%s", gdmgr._userdir, EDITOR_INI);
	sprintf(gdmgr._markbmp, "%s\\%s", gdmgr._curexedir, MARK_BMP);
	sprintf(gdmgr._pcbmp, "%s\\%s", gdmgr._curexedir, PC_BMP);
	sprintf(gdmgr._dvrbmp, "%s\\%s", gdmgr._curexedir, DVR_BMP);
	sprintf(gtext, "%s\\%s", gdmgr._curexedir, EDITOR_ICO);
	// gdmgr._markico = (HICON)LoadImage(gdmgr._hinst, gtext, IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
	gdmgr._markico = (HICON)LoadImage(gdmgr._hinst, MAKEINTRESOURCE(IDI_EDITOR), IMAGE_ICON, 0, 0, LR_CREATEDIBSECTION);
	// gdmgr._welcomebmp = (HBITMAP)LoadImage(gdmgr._hinst, formatstr("%s\\%s", gdmgr._curexedir, WELCOME_BMP), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION | LR_LOADTRANSPARENT);
	gdmgr._welcomebmp = (HBITMAP)LoadImage(gdmgr._hinst, MAKEINTRESOURCE(IDB_WELCOME), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);

	gdmgr._dvrsrc = dsrc_unknown;
	
	gdmgr._has_osdlib_page = 0;	// osdlib页

	InitializeCriticalSection(&gdmgr._cshbeat);

	//
	// oem.ini
	//
	// 得到公司名，默认空
	retval = CfgQueryValueWin(gdmgr._fulloemini, SECNAME_CORP, KEYNAME_CORP_NAME, gdmgr._corp_name);
	if (retval) {
		strcpy(gdmgr._corp_name, "Share Software (GPL)");
	}
	retval = CfgQueryValueWin(gdmgr._fulloemini, SECNAME_CORP, KEYNAME_CORP_HTTP, gdmgr._corp_http);
	if (retval) {
		strcpy(gdmgr._corp_http, "http://www.freeors.com");
	}

	//
	// system
	//
	// 工作目录
	if (CfgQueryValueWin(gdmgr._fulldvrmgrini, SECNAME_SYSTEM, KEYNAME_WWWROOT, text)) {
		// 配置文件不存在,创建一个,并设好默认值
		SHGetFolderPath(NULL, CSIDL_PERSONAL, NULL, 0, text);
		game_config::path = text;
		CfgSetValueWin(gdmgr._fulldvrmgrini, SECNAME_SYSTEM, KEYNAME_WWWROOT, text);
	} else {
		game_config::path = text;
	}
	gdmgr.heros_.set_path(game_config::path);
	set_preferences_dir("kingdom");

	if (CfgQueryValueWin(gdmgr._fulldvrmgrini, SECNAME_SYSTEM, KEYNAME_STARTUPSTRATEGY, text)) {
		gdmgr._appointed_dvrsrc = dsrc_auto;
		CfgSetValueWin(gdmgr._fulldvrmgrini, SECNAME_SYSTEM, KEYNAME_STARTUPSTRATEGY, formatstr("%u", dsrc_auto));
	} else {
		sscanf(text, "%u", &gdmgr._appointed_dvrsrc);
	}

	// 
	// sync
	//
	if (CfgQueryValueWin(gdmgr._fulldvrmgrini, SECNAME_SYNC, KEYNAME_AUTOCALENDAR, text)) {
		gdmgr._autocalendar = TRUE;
		CfgSetValueWin(gdmgr._fulldvrmgrini, SECNAME_SYNC, KEYNAME_AUTOCALENDAR, "TRUE");
	} else if (!strcmp(text, "TRUE")) {
		gdmgr._autocalendar = TRUE;
	} else {
		gdmgr._autocalendar = FALSE;
	}


	//
	// wgen
	//
	if (CfgQueryValueWin(gdmgr._fulldvrmgrini, SECNAME_WGEN, KEYNAME_AUTOSAVE, text)) {
		gdmgr._autosave = TRUE;
		CfgSetValueWin(gdmgr._fulldvrmgrini, SECNAME_WGEN, KEYNAME_AUTOSAVE, "TRUE");
	} else if (!strcmp(text, "TRUE")) {
		gdmgr._autosave = TRUE;
	} else {
		gdmgr._autosave = FALSE;
	}

	// 
	// xchg
	//
	if (CfgQueryValueWin(gdmgr._fulldvrmgrini, SECNAME_XCHG, KEYNAME_AUTOXCHG, text)) {
		gdmgr._autoxchg = FALSE;
		CfgSetValueWin(gdmgr._fulldvrmgrini, SECNAME_XCHG, KEYNAME_AUTOXCHG, "FALSE");
	} else if (!strcmp(text, "TRUE")) {
		gdmgr._autoxchg = TRUE;
	} else {
		gdmgr._autoxchg = FALSE;
	}
	if (CfgQueryValueWin(gdmgr._fulldvrmgrini, SECNAME_XCHG, KEYNAME_AUTOEXIT, text)) {
		gdmgr._autoexit = FALSE;
		CfgSetValueWin(gdmgr._fulldvrmgrini, SECNAME_XCHG, KEYNAME_AUTOEXIT, "FALSE");
	} else if (!strcmp(text, "TRUE")) {
		gdmgr._autoexit = TRUE;
	} else {
		gdmgr._autoexit = FALSE;
	}
	
	// 列表视图中的图标
	HICON                           hicon;

	gdmgr._himl = ImageList_Create(15, 15, ILC_COLOR24, 3, 0);
	ImageList_SetBkColor(gdmgr._himl, RGB(236, 233, 216));

    hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_DIR));
    gdmgr._iico_dir = ImageList_AddIcon(gdmgr._himl, hicon);

    hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_WITH_SYNC));
    gdmgr._iico_trans_min = gdmgr._iico_with_sync = ImageList_AddIcon(gdmgr._himl, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_WITHOUT_SYNC));
    gdmgr._iico_without_sync = ImageList_AddIcon(gdmgr._himl, hicon);

	hicon = (HICON)LoadImage(gdmgr._hinst, formatstr("%s\\csf.ico", gdmgr._curexedir), IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION | LR_LOADTRANSPARENT);
    gdmgr._iico_csf = ImageList_AddIcon(gdmgr._himl, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_TXT));
    gdmgr._iico_txt = ImageList_AddIcon(gdmgr._himl, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_TXT));
    gdmgr._iico_nma = ImageList_AddIcon(gdmgr._himl, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_INI));
    gdmgr._iico_trans_max = gdmgr._iico_ini = ImageList_AddIcon(gdmgr._himl, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_UNKNOWN));
    gdmgr._iico_unknown = ImageList_AddIcon(gdmgr._himl, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_DOWNLOAD));
    gdmgr._iico_download = ImageList_AddIcon(gdmgr._himl, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_UPLOAD));
    gdmgr._iico_upload = ImageList_AddIcon(gdmgr._himl, hicon);

	gdmgr._himl_24x24 = ImageList_Create(24, 24, ILC_COLOR24, 10, 0);
	ImageList_SetBkColor(gdmgr._himl_24x24, RGB(236, 233, 216));

	gdmgr._himl_24x24_dis = ImageList_Create(24, 24, ILC_COLOR24, 10, 0);
	ImageList_SetBkColor(gdmgr._himl_24x24_dis, RGB(236, 233, 216));

	sprintf(gtext, "%s\\open.ico", gdmgr._curexedir);

	// open.ico
	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_OPEN));
	gdmgr._iico_open = ImageList_AddIcon(gdmgr._himl_24x24, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_OPEN_DIS));
	ImageList_AddIcon(gdmgr._himl_24x24_dis, hicon);

	// reset.ico
	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_RESET));
	gdmgr._iico_reset = ImageList_AddIcon(gdmgr._himl_24x24, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_RESET_DIS));
	ImageList_AddIcon(gdmgr._himl_24x24_dis, hicon);

	// save.ico
	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_SAVE));
	gdmgr._iico_save = ImageList_AddIcon(gdmgr._himl_24x24, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_SAVE_DIS));
	ImageList_AddIcon(gdmgr._himl_24x24_dis, hicon);

	// new.ico
	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_NEW));
	gdmgr._iico_new = ImageList_AddIcon(gdmgr._himl_24x24, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_NEW_DIS));
	ImageList_AddIcon(gdmgr._himl_24x24_dis, hicon);

	// del.ico
	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_DEL));
	gdmgr._iico_del = ImageList_AddIcon(gdmgr._himl_24x24, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_DEL_DIS));
	ImageList_AddIcon(gdmgr._himl_24x24_dis, hicon);

	// rename.ico
	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_RENAME));
	gdmgr._iico_rename = ImageList_AddIcon(gdmgr._himl_24x24, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_RENAME_DIS));
	ImageList_AddIcon(gdmgr._himl_24x24_dis, hicon);

	// xchg.ico
	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_XCHG));
	gdmgr._iico_xchg = ImageList_AddIcon(gdmgr._himl_24x24, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_XCHG_DIS));
	ImageList_AddIcon(gdmgr._himl_24x24_dis, hicon);

	// refresh.ico
	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_REFRESH));
	gdmgr._iico_refresh = ImageList_AddIcon(gdmgr._himl_24x24, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_REFRESH_DIS));
	ImageList_AddIcon(gdmgr._himl_24x24_dis, hicon);

	// sync.ico
	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_SYNC));
	gdmgr._iico_sync = ImageList_AddIcon(gdmgr._himl_24x24, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_SYNC_DIS));
	ImageList_AddIcon(gdmgr._himl_24x24_dis, hicon);

	// build.ico
	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_BUILD));
	gdmgr._iico_build = ImageList_AddIcon(gdmgr._himl_24x24, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_BUILD_DIS));
	ImageList_AddIcon(gdmgr._himl_24x24_dis, hicon);

	// openpcdir.ico
	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_OPENPCDIR));
	gdmgr._iico_openpcdir = ImageList_AddIcon(gdmgr._himl_24x24, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_OPENPCDIR_DIS));
	ImageList_AddIcon(gdmgr._himl_24x24_dis, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_BACKUP));
	gdmgr._iico_backup = ImageList_AddIcon(gdmgr._himl_24x24, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_BACKUP_DIS));
	ImageList_AddIcon(gdmgr._himl_24x24_dis, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_OK));
	gdmgr._iico_ok = ImageList_AddIcon(gdmgr._himl_24x24, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_CANCEL));
	gdmgr._iico_cancel = ImageList_AddIcon(gdmgr._himl_24x24, hicon);

	// 
	// 题标大图标
	//
	gdmgr._himl_80x80 = ImageList_Create(80, 80, ILC_COLOR24, 6, 0);
	ImageList_SetBkColor(gdmgr._himl_80x80, RGB(236, 233, 216));

	gdmgr._himl_80x80_dis = ImageList_Create(80, 80, ILC_COLOR24, 6, 0);
	ImageList_SetBkColor(gdmgr._himl_80x80_dis, RGB(236, 233, 216));

	// title_sync.ico
	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_TITLE_SYNC));
	gdmgr._iico_title_sync = ImageList_AddIcon(gdmgr._himl_80x80, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_TITLE_SYNC_DIS));
	ImageList_AddIcon(gdmgr._himl_80x80_dis, hicon);

	// title_wgen.ico
	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_TITLE_WGEN));
	gdmgr._iico_title_wgen = ImageList_AddIcon(gdmgr._himl_80x80, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_TITLE_WGEN_DIS));
	ImageList_AddIcon(gdmgr._himl_80x80_dis, hicon);

	// title_xchg.ico
	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_TITLE_XCHG));
	gdmgr._iico_title_xchg = ImageList_AddIcon(gdmgr._himl_80x80, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_TITLE_XCHG_DIS));
	ImageList_AddIcon(gdmgr._himl_80x80_dis, hicon);

	// title_play.ico
	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_TITLE_PLAY));
	gdmgr._iico_title_play = ImageList_AddIcon(gdmgr._himl_80x80, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_TITLE_PLAY_DIS));
	ImageList_AddIcon(gdmgr._himl_80x80_dis, hicon);

	// title_update.ico
	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_TITLE_UPDATE));
	gdmgr._iico_title_update = ImageList_AddIcon(gdmgr._himl_80x80, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_TITLE_UPDATE_DIS));
	ImageList_AddIcon(gdmgr._himl_80x80_dis, hicon);

	// title_about.ico
	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_TITLE_ABOUT));
	gdmgr._iico_title_about = ImageList_AddIcon(gdmgr._himl_80x80, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_TITLE_ABOUT_DIS));
	ImageList_AddIcon(gdmgr._himl_80x80_dis, hicon);

	gdmgr._hbm_ok_select = LoadBitmap(gdmgr._hinst, MAKEINTRESOURCE(IDB_OK_SELECT));
	gdmgr._hbm_ok_unselect = LoadBitmap(gdmgr._hinst, MAKEINTRESOURCE(IDB_OK_UNSELECT));
	gdmgr._hbm_cancel_select = LoadBitmap(gdmgr._hinst, MAKEINTRESOURCE(IDB_CANCEL_SELECT)); 
	gdmgr._hbm_cancel_unselect = LoadBitmap(gdmgr._hinst, MAKEINTRESOURCE(IDB_CANCEL_UNSELECT));

	gdmgr._hbm_reboot_select = LoadBitmap(gdmgr._hinst, MAKEINTRESOURCE(IDB_REBOOT_SELECT)); 
	gdmgr._hbm_reboot_unselect = LoadBitmap(gdmgr._hinst, MAKEINTRESOURCE(IDB_REBOOT_UNSELECT));
	gdmgr._hbm_reboot_unselect_light = LoadBitmap(gdmgr._hinst, MAKEINTRESOURCE(IDB_REBOOT_UNSELECT_LIGHT));
	gdmgr._hbm_reboot_disable = LoadBitmap(gdmgr._hinst, MAKEINTRESOURCE(IDB_REBOOT_DISABLE)); 

	// 针对refresh按钮的位图
	gdmgr._hbm_refresh_select = (HBITMAP)LoadImage(gdmgr._hinst, MAKEINTRESOURCE(IDB_REFRESH_UNSELECT), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
    // gdmgr._hbm_refresh_select = LoadBitmap(gdmgr._hinst, MAKEINTRESOURCE(IDB_REFRESH_UNSELECT/*IDB_REFRESH_SELECT*/)); 
	transparent_24bmp(gdmgr._hbm_refresh_select, 0x00000000);
	gdmgr._hbm_refresh_unselect = (HBITMAP)LoadImage(gdmgr._hinst, MAKEINTRESOURCE(IDB_REFRESH_UNSELECT), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
	// gdmgr._hbm_refresh_unselect = LoadBitmap(gdmgr._hinst, MAKEINTRESOURCE(IDB_REFRESH_UNSELECT));
	transparent_24bmp(gdmgr._hbm_refresh_unselect, 0x00000000);
	gdmgr._hbm_refresh_disable = (HBITMAP)LoadImage(gdmgr._hinst, MAKEINTRESOURCE(IDB_REFRESH_UNSELECT), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
	// gdmgr._hbm_refresh_disable = LoadBitmap(gdmgr._hinst, MAKEINTRESOURCE(IDB_REFRESH_UNSELECT/*IDB_REFRESH_DISABLE*/));
	transparent_24bmp(gdmgr._hbm_refresh_disable, 0x00000000);

	// 菜单
	// menu item: generate
	gdmgr._hpopup_generate = CreatePopupMenu();
	AppendMenu(gdmgr._hpopup_generate, MF_STRING, IDM_GENERATE_ITEM0, "Extract install-material to C:\\kingdom-ins");
	// AppendMenu(gdmgr._hpopup_generate, MF_STRING, IDM_GENERATE_ITEM1, "WML to *.bin");

	// menu item: coherence
	gdmgr._hpopup_coherence = CreatePopupMenu();
	AppendMenu(gdmgr._hpopup_coherence, MF_STRING, IDM_COHERENCT_ITEM0, "List files");
	AppendMenu(gdmgr._hpopup_coherence, MF_SEPARATOR, 0, NULL);
	AppendMenu(gdmgr._hpopup_coherence, MF_STRING, IDM_COHERENCT_ITEM1, "XCopy using last modified file");
	AppendMenu(gdmgr._hpopup_coherence, MF_STRING, IDM_COHERENCT_ITEM2, "XCopy using this file");

	// menu item: delete
	gdmgr._hpopup_delete = CreatePopupMenu();
	AppendMenu(gdmgr._hpopup_delete, MF_STRING, IDM_DELETE_ITEM0, "Delete this");
	AppendMenu(gdmgr._hpopup_delete, MF_SEPARATOR, 0, NULL);
	AppendMenu(gdmgr._hpopup_delete, MF_STRING, IDM_DELETE_ITEM1, "Empty Directory");
	AppendMenu(gdmgr._hpopup_delete, MF_SEPARATOR, 0, NULL);
	AppendMenu(gdmgr._hpopup_delete, MF_STRING, IDM_DELETE_ITEM2, "List x_gen_*.htm/php");
	AppendMenu(gdmgr._hpopup_delete, MF_STRING, IDM_DELETE_ITEM3, "Delete x_gen_*.htm/php");
	
	// 主菜单要放在子菜单后面, 要确保子菜章句柄已有效
	gdmgr._hpopup_ddesc = CreatePopupMenu();
	AppendMenu(gdmgr._hpopup_ddesc, MF_POPUP, (UINT_PTR)(gdmgr._hpopup_generate), "Generate");
	AppendMenu(gdmgr._hpopup_ddesc, MF_POPUP, (UINT_PTR)(gdmgr._hpopup_coherence), "Coherence");
	AppendMenu(gdmgr._hpopup_ddesc, MF_POPUP, (UINT_PTR)(gdmgr._hpopup_delete), "Delete");
	
	gdmgr._hpopup_pc = CreatePopupMenu();
	AppendMenu(gdmgr._hpopup_pc, MF_STRING, IDM_OPEN, "Open");
	AppendMenu(gdmgr._hpopup_pc, MF_STRING, IDM_UPLOAD, "Upload");
	// AppendMenu(gdmgr._hpopup_pc, MF_STRING, IDM_FIND, "Find");
	AppendMenu(gdmgr._hpopup_pc, MF_STRING, IDM_DELETE, "Delete");

	// wgen会话时,editor控件上的弹出式菜单
	gdmgr._hpopup_editor = CreatePopupMenu();
	AppendMenu(gdmgr._hpopup_editor, MF_STRING, IDM_ADD, "Add...");
	AppendMenu(gdmgr._hpopup_editor, MF_STRING, IDM_EDIT, "Edit...");
	AppendMenu(gdmgr._hpopup_editor, MF_STRING, IDM_DELETE, "Delete");

	// OSD项中的弹出式菜单
	gdmgr._hpopup_osditem = CreatePopupMenu();
	AppendMenu(gdmgr._hpopup_osditem, MF_STRING, IDM_ADD, "Add...");
	AppendMenu(gdmgr._hpopup_osditem, MF_STRING, IDM_EDIT, "Edit...");
	AppendMenu(gdmgr._hpopup_osditem, MF_STRING, IDM_DELETE, "Delete");
	
	return;
}

void SaveCfgToIni(void)
{
	//
	// system
	//
	CfgSetValueWin(gdmgr._fulldvrmgrini, SECNAME_SYSTEM, KEYNAME_WWWROOT, game_config::path.c_str());
	CfgSetValueWin(gdmgr._fulldvrmgrini, SECNAME_SYSTEM, KEYNAME_STARTUPSTRATEGY, formatstr("%u", gdmgr._appointed_dvrsrc));
	
	//
	// sync
	//
	CfgSetValueWin(gdmgr._fulldvrmgrini, SECNAME_SYNC, KEYNAME_AUTOCALENDAR, gdmgr._autocalendar? "TRUE": "FALSE");

	//
	// wgen
	//
	CfgSetValueWin(gdmgr._fulldvrmgrini, SECNAME_WGEN, KEYNAME_AUTOSAVE, gdmgr._autosave? "TRUE": "FALSE");

	//
	// xchg
	//
	CfgSetValueWin(gdmgr._fulldvrmgrini, SECNAME_XCHG, KEYNAME_AUTOXCHG, gdmgr._autoxchg? "TRUE": "FALSE");
	CfgSetValueWin(gdmgr._fulldvrmgrini, SECNAME_XCHG, KEYNAME_AUTOEXIT, gdmgr._autoexit? "TRUE": "FALSE");

	return;
}

void uninit_dvrmgr_struct(void)
{
	DeleteCriticalSection(&gdmgr._cshbeat);

	SaveCfgToIni();

	if (gdmgr._markico) {
		DestroyIcon(gdmgr._markico);
	}
	if (gdmgr._welcomebmp) {
		DeleteObject(gdmgr._welcomebmp);
	}
	if (gdmgr._himl) {
		ImageList_Destroy(gdmgr._himl);
	}
	if (gdmgr._himl_24x24) {
		ImageList_Destroy(gdmgr._himl_24x24);
	}
	if (gdmgr._himl_24x24_dis) {
		ImageList_Destroy(gdmgr._himl_24x24_dis);
	}
	if (gdmgr._himl_80x80) {
		ImageList_Destroy(gdmgr._himl_80x80);
	}
	if (gdmgr._himl_80x80_dis) {
		ImageList_Destroy(gdmgr._himl_80x80_dis);
	}
	if (gdmgr._hbm_ok_select) {
		DeleteObject(gdmgr._hbm_ok_select);
	}
	if (gdmgr._hbm_ok_unselect) {
		DeleteObject(gdmgr._hbm_ok_unselect);
	}
	if (gdmgr._hbm_cancel_select) {
		DeleteObject(gdmgr._hbm_cancel_select);
	}
	if (gdmgr._hbm_cancel_unselect) {
		DeleteObject(gdmgr._hbm_cancel_unselect);
	}

	if (gdmgr._hbm_reboot_select) {
		DeleteObject(gdmgr._hbm_reboot_select);
	}
	if (gdmgr._hbm_reboot_unselect) {
		DeleteObject(gdmgr._hbm_reboot_unselect);
	}
	if (gdmgr._hbm_reboot_unselect_light) {
		DeleteObject(gdmgr._hbm_reboot_unselect_light);
	}
	if (gdmgr._hbm_reboot_disable) {
		DeleteObject(gdmgr._hbm_reboot_disable);
	}

	if (gdmgr._hbm_refresh_select) {
		DeleteObject(gdmgr._hbm_refresh_select);
	}
	if (gdmgr._hbm_refresh_unselect) {
		DeleteObject(gdmgr._hbm_refresh_unselect);
	}
	if (gdmgr._hbm_refresh_disable) {
		DeleteObject(gdmgr._hbm_refresh_disable);
	}

	if (gdmgr._hpopup_ddesc) {
		DestroyMenu(gdmgr._hpopup_ddesc);
	}
	if (gdmgr._hpopup_generate) {
		DestroyMenu(gdmgr._hpopup_generate);
	}
	if (gdmgr._hpopup_coherence) {
		DestroyMenu(gdmgr._hpopup_coherence);
	}
	if (gdmgr._hpopup_delete) {
		DestroyMenu(gdmgr._hpopup_delete);
	}
	if (gdmgr._hpopup_pc) {
		DestroyMenu(gdmgr._hpopup_pc);
	}
	if (gdmgr._hpopup_editor) {
		DestroyMenu(gdmgr._hpopup_editor);
	}
	if (gdmgr._hpopup_osditem) {
		DestroyMenu(gdmgr._hpopup_osditem);
	}
	if (gdmgr._hthdSync) {
		// 2、曾经运行过任务，现在已执行完了任务
		WaitForSingleObject(gdmgr._hthdSync, INFINITE);
		CloseHandle(gdmgr._hthdSync);
		gdmgr._hthdSync = NULL;
	}

	return;
}

#define TASKPB_STARTX_FULL			400
#define TASKPB_STARTX_PART			320
void StatusBar_Trans(void)
{
	int	parts[3] = {TASKPB_STARTX_PART, 426, -1};
	SendMessage(gdmgr._hwnd_status, SB_SETPARTS, 3, (LPARAM)&parts);
	ShowWindow(gdmgr._hpb_task, SW_RESTORE);
	return;
}

void StatusBar_Idle(void)
{
	int	parts[1] = {-1};

	SendMessage(gdmgr._hwnd_status, SB_SETPARTS, 1, (LPARAM)&parts);
	ShowWindow(gdmgr._hpb_task, SW_HIDE);
	return;
}

typedef enum {
	rcidx_hwnd_status,
	rcidx_dlg_ddesc,
	rcidx_explorer,
	rcidx_subarea,

	rcidx_dlg_wgen,
	rcidx_editor,

	rcidx_dlg_sync,
	rcidx_sync_sync,
	rcidx_sync_subarea,

	rcidx_dlg_tb,
	rcidx_tb_explorer,

	rcidx_dlg_cfg,
	rcidx_cfg_explorer,
	rcidx_max = rcidx_cfg_explorer,
};
RECT max_rc[rcidx_max + 1];
RECT normal_rc[rcidx_max + 1];

BOOL DVR_OnCreate(HWND hwndP, LPCREATESTRUCT lpCreateStruct)
{
	RECT		rcMain, rcTitle, rcDDesc;
	LONG		style;

	style = GetWindowLong(hwndP, GWL_STYLE);
	style &= ~WS_SIZEBOX;
	// style &= ~WS_MAXIMIZEBOX & ~WS_SIZEBOX;
	// SetWindowLong(hwndP, GWL_STYLE, style);

	// 此时是在CreateWindow中,gdmgr._hdlp_ddesc还是无效的
	GetWindowRect(hwndP, &gdmgr._main_rect);
	GetClientRect(hwndP, &rcMain);

	// 设备浏览器窗口
	if (gdmgr._dvrsrc != dsrc_unknown) {
		gdmgr._hdlg_ddesc = CreateDialogParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_DDESC), hwndP, DlgDDescProc, NULL);
		GetClientRect(gdmgr._hdlg_ddesc, &rcDDesc);
		MoveWindow(gdmgr._hdlg_ddesc, 0, 0, rcDDesc.right, rcMain.bottom, FALSE);
		ShowWindow(gdmgr._hdlg_ddesc, SW_RESTORE);
	} else {
		gdmgr._hdlg_ddesc = NULL;
		rcDDesc.left = rcDDesc.top = 0;
		rcDDesc.right = 5;
		rcDDesc.bottom = 0;
	}

	// 标题栏窗口
	gdmgr._hdlg_title = CreateDialogParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_TITLE), hwndP, DlgTitleProc, NULL);
	GetClientRect(gdmgr._hdlg_title, &rcTitle);
	MoveWindow(gdmgr._hdlg_title, rcDDesc.right, 0, rcMain.right - rcDDesc.right, rcTitle.bottom, FALSE);
	ShowWindow(gdmgr._hdlg_title, SW_RESTORE);

	// 会话窗口: IDD_SYNC
	gdmgr._hdlg_sync = CreateDialogParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_SYNC), hwndP, DlgSyncProc, NULL);
	MoveWindow(gdmgr._hdlg_sync, rcDDesc.right, rcTitle.bottom, rcMain.right - rcDDesc.right, rcMain.bottom - rcTitle.bottom, FALSE);
	ShowWindow(gdmgr._hdlg_sync, SW_HIDE);

	// 会话窗口: IDD_WGEN
	gdmgr._hdlg_wgen = CreateDialogParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_WGEN), hwndP, DlgWGenProc, NULL);
	MoveWindow(gdmgr._hdlg_wgen, rcDDesc.right, rcTitle.bottom, rcMain.right - rcDDesc.right, rcMain.bottom - rcTitle.bottom, FALSE);
	ShowWindow(gdmgr._hdlg_wgen, SW_HIDE);

	// 会话窗口: IDD_TB
	gdmgr._hdlg_tb = CreateDialogParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_TB), hwndP, DlgTbProc, NULL);
	MoveWindow(gdmgr._hdlg_tb, rcDDesc.right, rcTitle.bottom, rcMain.right - rcDDesc.right, rcMain.bottom - rcTitle.bottom, FALSE);
	ShowWindow(gdmgr._hdlg_tb, SW_HIDE);

	// 会话窗口: IDD_CFG
	gdmgr._hdlg_cfg = CreateDialogParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_CFG), hwndP, DlgCfgProc, NULL);
	MoveWindow(gdmgr._hdlg_cfg, rcDDesc.right, rcTitle.bottom, rcMain.right - rcDDesc.right, rcMain.bottom - rcTitle.bottom, FALSE);
	ShowWindow(gdmgr._hdlg_cfg, SW_HIDE);

	// 会话窗口: IDD_TBOX
	gdmgr._hdlg_tbox = CreateDialogParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_TBOX), hwndP, DlgTBoxProc, NULL);
	MoveWindow(gdmgr._hdlg_tbox, rcDDesc.right, rcTitle.bottom, rcMain.right - rcDDesc.right, rcMain.bottom - rcTitle.bottom, FALSE);
	ShowWindow(gdmgr._hdlg_tbox, SW_HIDE);

	// 会话窗口: IDD_ABOUT
	gdmgr._hdlg_about = CreateDialogParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_ABOUT), hwndP, DlgAboutProc, NULL);
	MoveWindow(gdmgr._hdlg_about, rcDDesc.right, rcTitle.bottom, rcMain.right - rcDDesc.right, rcMain.bottom - rcTitle.bottom, FALSE);
	ShowWindow(gdmgr._hdlg_about, SW_HIDE);

	gdmgr._video_area.left = rcDDesc.right;
	gdmgr._video_area.top = rcTitle.bottom;
	gdmgr._video_area.right = rcMain.right;
	gdmgr._video_area.bottom = rcMain.bottom;

	// 状态窗口
	gdmgr._hwnd_status = CreateWindow(STATUSCLASSNAME, "", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwndP, NULL, gdmgr._hinst, NULL);
	gdmgr._hpb_task = CreateWindowEx(0, // no extended style
		PROGRESS_CLASS,
		"Progress Bar",
		WS_CHILD | PBS_SMOOTH | WS_BORDER, 
		TASKPB_STARTX_PART, 2, 110, 20,
		gdmgr._hwnd_status, NULL, gdmgr._hinst, NULL);
	ProgressBar_SetBarColor(gdmgr._hpb_task, RGB(255, 255,0));	// 黄色
			
	// 一个1秒定时器
	SetTimer(hwndP, ID_STATUS_TIMER, 1000, NULL);

	title_select(da_sync);
	title_enable_ui(FALSE);

	// 收集全屏和非全屏状态时须要改变尺寸的控件位置(看来还较复杂，_hdlg_ddesc这些窗口也得缩放，而不仅仅是控件而已，暂时还是不实现吧）
	RECT rc;

	//
	// status window
	//
	GetWindowRect(gdmgr._hwnd_status, &rc);
	MapWindowPoints(NULL, GetParent(gdmgr._hwnd_status), (LPPOINT)(&rc), (sizeof(RECT)/sizeof(POINT)));
	normal_rc[rcidx_hwnd_status] = rc;

	//
	// IDD_DDESC
	//
	GetWindowRect(gdmgr._hdlg_ddesc, &rc);
	MapWindowPoints(NULL, GetParent(gdmgr._hdlg_ddesc), (LPPOINT)(&rc), (sizeof(RECT)/sizeof(POINT)));
	normal_rc[rcidx_dlg_ddesc] = rc;
	// IDC_TV_DDESC_EXPLORER
	GetWindowRect(GetDlgItem(gdmgr._hdlg_ddesc, IDC_TV_DDESC_EXPLORER), &rc);
	MapWindowPoints(NULL, gdmgr._hdlg_ddesc, (LPPOINT)(&rc), (sizeof(RECT)/sizeof(POINT)));
	normal_rc[rcidx_explorer] = rc;
	// IDC_ET_DDESC_SUBAREA
	GetWindowRect(GetDlgItem(gdmgr._hdlg_ddesc, IDC_ET_DDESC_SUBAREA), &rc);
	MapWindowPoints(NULL, gdmgr._hdlg_ddesc, (LPPOINT)(&rc), (sizeof(RECT)/sizeof(POINT)));
	normal_rc[rcidx_subarea] = rc;

	//
	// IDD_WGEN
	//
	GetWindowRect(gdmgr._hdlg_wgen, &rc);
	MapWindowPoints(NULL, GetParent(gdmgr._hdlg_wgen), (LPPOINT)(&rc), (sizeof(RECT)/sizeof(POINT)));
	normal_rc[rcidx_dlg_wgen] = rc;
	// IDC_LV_WGEN_EDITOR
	GetWindowRect(GetDlgItem(gdmgr._hdlg_wgen, IDC_LV_WGEN_EDITOR), &rc);
	MapWindowPoints(NULL, gdmgr._hdlg_wgen, (LPPOINT)(&rc), (sizeof(RECT)/sizeof(POINT)));
	normal_rc[rcidx_editor] = rc;

	//
	// IDD_SYNC
	//
	GetWindowRect(gdmgr._hdlg_sync, &rc);
	MapWindowPoints(NULL, GetParent(gdmgr._hdlg_sync), (LPPOINT)(&rc), (sizeof(RECT)/sizeof(POINT)));
	normal_rc[rcidx_dlg_sync] = rc;
	// IDC_LV_SYNC_SYNC
	GetWindowRect(GetDlgItem(gdmgr._hdlg_sync, IDC_LV_SYNC_SYNC), &rc);
	MapWindowPoints(NULL, gdmgr._hdlg_sync, (LPPOINT)(&rc), (sizeof(RECT)/sizeof(POINT)));
	normal_rc[rcidx_sync_sync] = rc;
	// IDC_ET_SYNC_SUBAREA
	GetWindowRect(GetDlgItem(gdmgr._hdlg_sync, IDC_ET_SYNC_SUBAREA), &rc);
	MapWindowPoints(NULL, gdmgr._hdlg_sync, (LPPOINT)(&rc), (sizeof(RECT)/sizeof(POINT)));
	normal_rc[rcidx_sync_subarea] = rc;

	//
	// IDD_TB
	//
	GetWindowRect(gdmgr._hdlg_tb, &rc);
	MapWindowPoints(NULL, GetParent(gdmgr._hdlg_tb), (LPPOINT)(&rc), (sizeof(RECT)/sizeof(POINT)));
	normal_rc[rcidx_dlg_tb] = rc;
	// IDC_TV_TB_EXPLORER
	GetWindowRect(GetDlgItem(gdmgr._hdlg_tb, IDC_TV_TB_EXPLORER), &rc);
	MapWindowPoints(NULL, gdmgr._hdlg_tb, (LPPOINT)(&rc), (sizeof(RECT)/sizeof(POINT)));
	normal_rc[rcidx_tb_explorer] = rc;

	//
	// IDD_CFG
	//
	GetWindowRect(gdmgr._hdlg_cfg, &rc);
	MapWindowPoints(NULL, GetParent(gdmgr._hdlg_cfg), (LPPOINT)(&rc), (sizeof(RECT)/sizeof(POINT)));
	normal_rc[rcidx_dlg_cfg] = rc;
	// IDC_TV_CFG_EXPLORER
	GetWindowRect(GetDlgItem(gdmgr._hdlg_cfg, IDC_TV_CFG_EXPLORER), &rc);
	MapWindowPoints(NULL, gdmgr._hdlg_cfg, (LPPOINT)(&rc), (sizeof(RECT)/sizeof(POINT)));
	normal_rc[rcidx_cfg_explorer] = rc;

	return TRUE;
}

// 处理窗口缩放
// 1. 缩放时须要被处理窗口的尺寸都放置在normal_rc和max_rc这两个矩形数组中。normal_rc对应窗口状态，max_rc对应全屏状态；
// 2. normal_rc/max_rc存储的矩形坐标都相对于它们的直接父窗口，注：MoveWindow要求这种坐标；
// 3. normal_rc在DVR_OnCreate被全部赋值。也就是在DVR_OnCreate时程序主窗口须是窗口状态；
// 4. max_rc在calculate_max_rects被全部赋值。它在WM_SIZE中将是全屏时被调用。calculate_max_rects就是计算几个坐标，就让每次WM_SIZE都调用
// 5. WM_SIZE时对每个窗口使用MoveWindow
void calculate_max_rects(HWND hwndP)
{
	RECT rcMain;
	uint32_t vertical_diff, horizontal_diff;
	
	GetClientRect(hwndP, &rcMain);

	vertical_diff = rcMain.bottom - normal_rc[rcidx_dlg_ddesc].bottom;
	//
	// status window
	//
	max_rc[rcidx_hwnd_status] = normal_rc[rcidx_hwnd_status];
	max_rc[rcidx_hwnd_status].top = normal_rc[rcidx_hwnd_status].top + vertical_diff;
	max_rc[rcidx_hwnd_status].bottom = normal_rc[rcidx_hwnd_status].bottom + vertical_diff;

	//
	// IDD_DDESC
	//
	max_rc[rcidx_dlg_ddesc] = normal_rc[rcidx_dlg_ddesc];
	max_rc[rcidx_dlg_ddesc].bottom = rcMain.bottom;

	max_rc[rcidx_explorer] = normal_rc[rcidx_explorer];
	max_rc[rcidx_explorer].bottom = normal_rc[rcidx_explorer].bottom + vertical_diff;

	max_rc[rcidx_subarea] = normal_rc[rcidx_subarea];
	max_rc[rcidx_subarea].top = normal_rc[rcidx_subarea].top + vertical_diff;
	max_rc[rcidx_subarea].bottom = normal_rc[rcidx_subarea].bottom + vertical_diff;

	horizontal_diff = rcMain.right - normal_rc[rcidx_dlg_wgen].right;
	vertical_diff = rcMain.bottom - normal_rc[rcidx_dlg_wgen].bottom;
	//
	// IDD_WGEN
	//
	max_rc[rcidx_dlg_wgen] = normal_rc[rcidx_dlg_wgen];
	max_rc[rcidx_dlg_wgen].right = rcMain.right;
	max_rc[rcidx_dlg_wgen].bottom = rcMain.bottom;

	max_rc[rcidx_editor] = normal_rc[rcidx_editor];
	max_rc[rcidx_editor].right = normal_rc[rcidx_editor].right + horizontal_diff;
	max_rc[rcidx_editor].bottom = normal_rc[rcidx_editor].bottom + vertical_diff;

	horizontal_diff = rcMain.right - normal_rc[rcidx_dlg_sync].right;
	vertical_diff = rcMain.bottom - normal_rc[rcidx_dlg_sync].bottom;
	//
	// IDD_SYNC
	//
	max_rc[rcidx_dlg_sync] = normal_rc[rcidx_dlg_sync];
	max_rc[rcidx_dlg_sync].right = rcMain.right;
	max_rc[rcidx_dlg_sync].bottom = rcMain.bottom;

	max_rc[rcidx_sync_sync] = normal_rc[rcidx_sync_sync];
	max_rc[rcidx_sync_sync].right = normal_rc[rcidx_sync_sync].right + horizontal_diff;
	max_rc[rcidx_sync_sync].bottom = normal_rc[rcidx_sync_sync].bottom + vertical_diff;

	max_rc[rcidx_sync_subarea] = normal_rc[rcidx_sync_subarea];
	max_rc[rcidx_sync_subarea].top = normal_rc[rcidx_sync_subarea].top + vertical_diff;
	max_rc[rcidx_sync_subarea].bottom = normal_rc[rcidx_sync_subarea].bottom + vertical_diff;

	horizontal_diff = rcMain.right - normal_rc[rcidx_dlg_tb].right;
	vertical_diff = rcMain.bottom - normal_rc[rcidx_dlg_tb].bottom;
	//
	// IDD_TB
	//
	max_rc[rcidx_dlg_tb] = normal_rc[rcidx_dlg_tb];
	max_rc[rcidx_dlg_tb].right = rcMain.right;
	max_rc[rcidx_dlg_tb].bottom = rcMain.bottom;

	max_rc[rcidx_tb_explorer] = normal_rc[rcidx_tb_explorer];
	max_rc[rcidx_tb_explorer].right = normal_rc[rcidx_tb_explorer].right + horizontal_diff;
	max_rc[rcidx_tb_explorer].bottom = normal_rc[rcidx_tb_explorer].bottom + vertical_diff;


	horizontal_diff = rcMain.right - normal_rc[rcidx_dlg_cfg].right;
	vertical_diff = rcMain.bottom - normal_rc[rcidx_dlg_cfg].bottom;
	//
	// IDD_CFG
	//
	max_rc[rcidx_dlg_cfg] = normal_rc[rcidx_dlg_cfg];
	max_rc[rcidx_dlg_cfg].right = rcMain.right;
	max_rc[rcidx_dlg_cfg].bottom = rcMain.bottom;

	max_rc[rcidx_cfg_explorer] = normal_rc[rcidx_cfg_explorer];
	max_rc[rcidx_cfg_explorer].right = normal_rc[rcidx_cfg_explorer].right + horizontal_diff;
	max_rc[rcidx_cfg_explorer].bottom = normal_rc[rcidx_cfg_explorer].bottom + vertical_diff;
}

void DVR_OnPaint(HWND hdlgP) 
{
    PAINTSTRUCT ps; 
    HDC         hdc; 
	    
	hdc = BeginPaint(hdlgP, &ps); 
    // When using VMR Windowless mode, you must explicitly tell the
    // renderer when to repaint the video in response to WM_PAINT
    // messages.  This is most important when the video is stopped
    // or paused, since the VMR won't be automatically updating the
    // window as the video plays.
    EndPaint(hdlgP, &ps); 
	return;
} 

BOOL PointInVideoArea(int x, int y)
{
	if ((x >= gdmgr._video_area.left) && (x <= gdmgr._video_area.right)) {
		if ((y >= gdmgr._video_area.top) && (y <= gdmgr._video_area.bottom)) {
			return TRUE;
		}
	}
	return FALSE;
}

void Visable_AllCtrl(BOOL visable)
{
	if (visable) {
		ShowWindow(gdmgr._hdlg_ddesc, SW_RESTORE);
		ShowWindow(gdmgr._hdlg_title, SW_RESTORE);
		ShowWindow(gdmgr._hwnd_status, SW_RESTORE);
	} else {
		ShowWindow(gdmgr._hdlg_ddesc, SW_HIDE);
		ShowWindow(gdmgr._hdlg_title, SW_HIDE);
		ShowWindow(gdmgr._hwnd_status, SW_HIDE);
	}
	return;
}

// MoveWindow函数执行时系统会发hWnd的WM_SIZE消息,为不搅乱,On_DlgMainLButtonDblClk将不调用
// 任何SetVideoPosition,改而由On_DlgMainSize调用
void DVR_OnLButtonDblClk(HWND hdlgP, BOOL fDobule, int x, int y, UINT wParam)
{
	int				cxScreen = GetSystemMetrics(SM_CXSCREEN);
	int				cyScreen = GetSystemMetrics(SM_CYSCREEN);
	
	LONG			lStyle;
	RECT			rc;
 
	if ((gdmgr._da != da_cfg) || (!gdmgr._fFullScreen && !PointInVideoArea(x, y))) {
		return;
	}

	lStyle = GetWindowLong(hdlgP, GWL_STYLE);
	
	if (!gdmgr._fFullScreen) {
		// 当前是非全屏幕模式,双击后要让它变为全屏幕模式
		lStyle &= ~WS_CAPTION;
		
		Visable_AllCtrl(FALSE);
		MoveWindow(gdmgr._hwnd_main, 0, 0, cxScreen, cyScreen, TRUE);

		rc.left = rc.top = 0;
		rc.right = cxScreen;
		rc.bottom = cyScreen;

	} else {
		// 当前是全屏幕模式,双击后要让它变为非全屏幕模式
		lStyle |= WS_CAPTION;
		SetWindowLong(hdlgP, GWL_STYLE, lStyle);

		Visable_AllCtrl(TRUE);
		MoveWindow(gdmgr._hwnd_main, gdmgr._main_rect.left, gdmgr._main_rect.top, gdmgr._main_rect.right - gdmgr._main_rect.left, gdmgr._main_rect.bottom - gdmgr._main_rect.top, TRUE);
		rc = gdmgr._video_area;
	}
	SetWindowLong(hdlgP, GWL_STYLE, lStyle);
	
	gdmgr._fFullScreen = !gdmgr._fFullScreen;

	return;
}

// 这里遇到个搞笑的巧合
// 由于子窗口关系,在非play时,主窗口其它画面几乎都被子窗口覆盖,致使收不到鼠标右键消息
// 这个巧合好的话,不用判断鼠标右键被点击的区域
void DVR_OnRButtonUp(HWND hdlgP, int x, int y, UINT wParam)
{
	POINT		point = {x, y};

	MapWindowPoints(hdlgP, NULL, &point, 1);

	TrackPopupMenuEx(gdmgr._hpopup_editor, 0, 
					point.x, 
					point.y, 
					hdlgP, 
					NULL);
}

void DVR_OnCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	switch(id) {
	case IDM_FMTSETTING:
		break;
	default:
		break;
	}
	return;
}

void DVR_OnSize(HWND hdlgP, UINT wParam, int cx, int cy)
{
	if (IsZoomed(hdlgP)) {
		calculate_max_rects(hdlgP);
		MoveWindow(gdmgr._hwnd_status, max_rc[rcidx_hwnd_status].left, max_rc[rcidx_hwnd_status].top, max_rc[rcidx_hwnd_status].right - max_rc[rcidx_hwnd_status].left, max_rc[rcidx_hwnd_status].bottom - max_rc[rcidx_hwnd_status].top, TRUE);

		MoveWindow(gdmgr._hdlg_ddesc, max_rc[rcidx_dlg_ddesc].left, max_rc[rcidx_dlg_ddesc].top, max_rc[rcidx_dlg_ddesc].right - max_rc[rcidx_dlg_ddesc].left, max_rc[rcidx_dlg_ddesc].bottom - max_rc[rcidx_dlg_ddesc].top, TRUE);
		MoveWindow(GetDlgItem(gdmgr._hdlg_ddesc, IDC_TV_DDESC_EXPLORER), max_rc[rcidx_explorer].left, max_rc[rcidx_explorer].top, max_rc[rcidx_explorer].right - max_rc[rcidx_explorer].left, max_rc[rcidx_explorer].bottom - max_rc[rcidx_explorer].top, TRUE);
		MoveWindow(GetDlgItem(gdmgr._hdlg_ddesc, IDC_ET_DDESC_SUBAREA), max_rc[rcidx_subarea].left, max_rc[rcidx_subarea].top, max_rc[rcidx_subarea].right - max_rc[rcidx_subarea].left, max_rc[rcidx_subarea].bottom - max_rc[rcidx_subarea].top, TRUE);

		MoveWindow(gdmgr._hdlg_wgen, max_rc[rcidx_dlg_wgen].left, max_rc[rcidx_dlg_wgen].top, max_rc[rcidx_dlg_wgen].right - max_rc[rcidx_dlg_wgen].left, max_rc[rcidx_dlg_wgen].bottom - max_rc[rcidx_dlg_wgen].top, TRUE);
		MoveWindow(GetDlgItem(gdmgr._hdlg_wgen, IDC_LV_WGEN_EDITOR), max_rc[rcidx_editor].left, max_rc[rcidx_editor].top, max_rc[rcidx_editor].right - max_rc[rcidx_editor].left, max_rc[rcidx_editor].bottom - max_rc[rcidx_editor].top, TRUE);

		MoveWindow(gdmgr._hdlg_sync, max_rc[rcidx_dlg_sync].left, max_rc[rcidx_dlg_sync].top, max_rc[rcidx_dlg_sync].right - max_rc[rcidx_dlg_sync].left, max_rc[rcidx_dlg_sync].bottom - max_rc[rcidx_dlg_sync].top, TRUE);
		MoveWindow(GetDlgItem(gdmgr._hdlg_sync, IDC_LV_SYNC_SYNC), max_rc[rcidx_sync_sync].left, max_rc[rcidx_sync_sync].top, max_rc[rcidx_sync_sync].right - max_rc[rcidx_sync_sync].left, max_rc[rcidx_sync_sync].bottom - max_rc[rcidx_sync_sync].top, TRUE);
		MoveWindow(GetDlgItem(gdmgr._hdlg_sync, IDC_ET_SYNC_SUBAREA), max_rc[rcidx_sync_subarea].left, max_rc[rcidx_sync_subarea].top, max_rc[rcidx_sync_subarea].right - max_rc[rcidx_sync_subarea].left, max_rc[rcidx_sync_subarea].bottom - max_rc[rcidx_sync_subarea].top, TRUE);

		MoveWindow(gdmgr._hdlg_tb, max_rc[rcidx_dlg_tb].left, max_rc[rcidx_dlg_tb].top, max_rc[rcidx_dlg_tb].right - max_rc[rcidx_dlg_tb].left, max_rc[rcidx_dlg_tb].bottom - max_rc[rcidx_dlg_tb].top, TRUE);
		MoveWindow(GetDlgItem(gdmgr._hdlg_tb, IDC_TV_TB_EXPLORER), max_rc[rcidx_tb_explorer].left, max_rc[rcidx_tb_explorer].top, max_rc[rcidx_tb_explorer].right - max_rc[rcidx_tb_explorer].left, max_rc[rcidx_tb_explorer].bottom - max_rc[rcidx_tb_explorer].top, TRUE);

		MoveWindow(gdmgr._hdlg_cfg, max_rc[rcidx_dlg_cfg].left, max_rc[rcidx_dlg_cfg].top, max_rc[rcidx_dlg_cfg].right - max_rc[rcidx_dlg_cfg].left, max_rc[rcidx_dlg_cfg].bottom - max_rc[rcidx_dlg_cfg].top, TRUE);
		MoveWindow(GetDlgItem(gdmgr._hdlg_cfg, IDC_TV_CFG_EXPLORER), max_rc[rcidx_cfg_explorer].left, max_rc[rcidx_cfg_explorer].top, max_rc[rcidx_cfg_explorer].right - max_rc[rcidx_cfg_explorer].left, max_rc[rcidx_cfg_explorer].bottom - max_rc[rcidx_cfg_explorer].top, TRUE);
	} else {
		MoveWindow(gdmgr._hwnd_status, normal_rc[rcidx_hwnd_status].left, normal_rc[rcidx_hwnd_status].top, normal_rc[rcidx_hwnd_status].right - normal_rc[rcidx_hwnd_status].left, normal_rc[rcidx_hwnd_status].bottom - normal_rc[rcidx_hwnd_status].top, TRUE);

		MoveWindow(gdmgr._hdlg_ddesc, normal_rc[rcidx_dlg_ddesc].left, normal_rc[rcidx_dlg_ddesc].top, normal_rc[rcidx_dlg_ddesc].right - normal_rc[rcidx_dlg_ddesc].left, normal_rc[rcidx_dlg_ddesc].bottom - normal_rc[rcidx_dlg_ddesc].top, TRUE);
		MoveWindow(GetDlgItem(gdmgr._hdlg_ddesc, IDC_TV_DDESC_EXPLORER), normal_rc[rcidx_explorer].left, normal_rc[rcidx_explorer].top, normal_rc[rcidx_explorer].right - normal_rc[rcidx_explorer].left, normal_rc[rcidx_explorer].bottom - normal_rc[rcidx_explorer].top, TRUE);
		MoveWindow(GetDlgItem(gdmgr._hdlg_ddesc, IDC_ET_DDESC_SUBAREA), normal_rc[rcidx_subarea].left, normal_rc[rcidx_subarea].top, normal_rc[rcidx_subarea].right - normal_rc[rcidx_subarea].left, normal_rc[rcidx_subarea].bottom - normal_rc[rcidx_subarea].top, TRUE);

		MoveWindow(gdmgr._hdlg_wgen, normal_rc[rcidx_dlg_wgen].left, normal_rc[rcidx_dlg_wgen].top, normal_rc[rcidx_dlg_wgen].right - normal_rc[rcidx_dlg_wgen].left, normal_rc[rcidx_dlg_wgen].bottom - normal_rc[rcidx_dlg_wgen].top, TRUE);
		MoveWindow(GetDlgItem(gdmgr._hdlg_wgen, IDC_LV_WGEN_EDITOR), normal_rc[rcidx_editor].left, normal_rc[rcidx_editor].top, normal_rc[rcidx_editor].right - normal_rc[rcidx_editor].left, normal_rc[rcidx_editor].bottom - normal_rc[rcidx_editor].top, TRUE);

		MoveWindow(gdmgr._hdlg_sync, normal_rc[rcidx_dlg_sync].left, normal_rc[rcidx_dlg_sync].top, normal_rc[rcidx_dlg_sync].right - normal_rc[rcidx_dlg_sync].left, normal_rc[rcidx_dlg_sync].bottom - normal_rc[rcidx_dlg_sync].top, TRUE);
		MoveWindow(GetDlgItem(gdmgr._hdlg_sync, IDC_LV_SYNC_SYNC), normal_rc[rcidx_sync_sync].left, normal_rc[rcidx_sync_sync].top, normal_rc[rcidx_sync_sync].right - normal_rc[rcidx_sync_sync].left, normal_rc[rcidx_sync_sync].bottom - normal_rc[rcidx_sync_sync].top, TRUE);
		MoveWindow(GetDlgItem(gdmgr._hdlg_sync, IDC_ET_SYNC_SUBAREA), normal_rc[rcidx_sync_subarea].left, normal_rc[rcidx_sync_subarea].top, normal_rc[rcidx_sync_subarea].right - normal_rc[rcidx_sync_subarea].left, normal_rc[rcidx_sync_subarea].bottom - normal_rc[rcidx_sync_subarea].top, TRUE);

		MoveWindow(gdmgr._hdlg_tb, normal_rc[rcidx_dlg_tb].left, normal_rc[rcidx_dlg_tb].top, normal_rc[rcidx_dlg_tb].right - normal_rc[rcidx_dlg_tb].left, normal_rc[rcidx_dlg_tb].bottom - normal_rc[rcidx_dlg_tb].top, TRUE);
		MoveWindow(GetDlgItem(gdmgr._hdlg_tb, IDC_TV_TB_EXPLORER), normal_rc[rcidx_tb_explorer].left, normal_rc[rcidx_tb_explorer].top, normal_rc[rcidx_tb_explorer].right - normal_rc[rcidx_tb_explorer].left, normal_rc[rcidx_tb_explorer].bottom - normal_rc[rcidx_tb_explorer].top, TRUE);

		MoveWindow(gdmgr._hdlg_cfg, normal_rc[rcidx_dlg_cfg].left, normal_rc[rcidx_dlg_cfg].top, normal_rc[rcidx_dlg_cfg].right - normal_rc[rcidx_dlg_cfg].left, normal_rc[rcidx_dlg_cfg].bottom - normal_rc[rcidx_dlg_cfg].top, TRUE);
		MoveWindow(GetDlgItem(gdmgr._hdlg_cfg, IDC_TV_CFG_EXPLORER), normal_rc[rcidx_cfg_explorer].left, normal_rc[rcidx_cfg_explorer].top, normal_rc[rcidx_cfg_explorer].right - normal_rc[rcidx_cfg_explorer].left, normal_rc[rcidx_cfg_explorer].bottom - normal_rc[rcidx_cfg_explorer].top, TRUE);
	}

	// GetClientRect(GetDlgItem(gdmgr._htb_wgen, IDC_BT_WGEN_REBOOT), &rect);
}


#define MAXINTERVAL_NOHBEAT			7	// 心跳包理论上2秒一个,理论上应该有三个心跳包

void repaint_reboot_btn(HBITMAP hbmp)
{
	RECT				rc;
	HDC					hDC, hdcMem; 

	GetClientRect(GetDlgItem(gdmgr._htb_wgen, IDC_BT_WGEN_REBOOT), &rc);
	hDC = GetDC(GetDlgItem(gdmgr._htb_wgen, IDC_BT_WGEN_REBOOT));
    hdcMem = CreateCompatibleDC(hDC); 

	SelectObject(hdcMem, hbmp); 
	
    // Destination 
    StretchBlt( 
                hDC,         // destination DC 
                rc.left, // x upper left 
                rc.top,  // y upper left 
 
                // The next two lines specify the width and 
                // height. 
                rc.right - rc.left, 
                rc.bottom - rc.top, 
                hdcMem,    // source device context 
                0, 0,      // x and y upper left 
                24,        // source bitmap width 
                24,        // source bitmap height 
                SRCCOPY);  // raster operation 

	DeleteDC(hdcMem); 
	return;
}

// 频率:一秒定时器
void DVR_OnTimer(HWND hwndP, UINT id)
{
	gdmgr._main_timer_count ++;
	return;
}

LRESULT CALLBACK WndMainProc(HWND hwndP, UINT msg, WPARAM wParam, LPARAM lParam)
{
	BOOL		fRet = FALSE;

    switch(msg) {
	case WM_CREATE:
		DVR_OnCreate(hwndP, (LPCREATESTRUCT)lParam);
		break;
	HANDLE_MSG(hwndP, WM_COMMAND, DVR_OnCommand);
	HANDLE_MSG(hwndP, WM_TIMER, DVR_OnTimer);
	HANDLE_MSG(hwndP, WM_PAINT, DVR_OnPaint);
	HANDLE_MSG(hwndP, WM_RBUTTONUP, DVR_OnRButtonUp);
	HANDLE_MSG(hwndP, WM_LBUTTONDBLCLK, DVR_OnLButtonDblClk);
	HANDLE_MSG(hwndP, WM_SIZE, DVR_OnSize);
    case WM_CLOSE:
		if (gdmgr._syncing) {
			posix_print_mb("Now is synchronizing, don't exit");
			return 0;
		}
        posix_print("<WinMain.cpp>::WndMainProc, WM_CLOSE------, 0\n");
		// 需要主窗口句柄还是有效时调用title_select(da_unknown)
		title_select(da_unknown);

		KillTimer(hwndP, ID_STATUS_TIMER);
		posix_print("<WinMain.cpp>::WndMainProc, WM_CLOSE------, 1\n");
		break;		

    case WM_DESTROY:
		PostQuitMessage(0);
		break;

    default:
        return DefWindowProc(hwndP, msg, wParam, lParam);
    } // Window msgs handling

    return DefWindowProc(hwndP, msg, wParam, lParam);
}

/**
 * I would prefer to setup locale first so that early error
 * messages can get localized, but we need the game_controller
 * initialized to have get_intl_dir() to work.  Note: setlocale()
 * does not take GUI language setting into account.
 */
static void init_locale()
{
#ifdef _WIN32
	std::setlocale(LC_ALL, "English");
#else
	std::setlocale(LC_ALL, "C");
	std::setlocale(LC_MESSAGES, "");
#endif
	const std::string& intl_dir = get_intl_dir();
	textdomain(PACKAGE "-hero");
	bindtextdomain (PACKAGE "-hero", intl_dir.c_str());
	// bind_textdomain_codeset (PACKAGE "-hero", "UTF-8");

	SetEnvironmentVariable("LANG", "zh_CN");
}

void update_locale_dir()
{
	const std::string& intl_dir = get_intl_dir();
	textdomain(PACKAGE "-hero");
	bindtextdomain (PACKAGE "-hero", intl_dir.c_str());
}

void main_ui_sysmenu(BOOL fEnable)
{
	LONG		lStyle = GetWindowLong(gdmgr._hwnd_main, GWL_STYLE);
	
	if (fEnable) {
		lStyle |= WS_SYSMENU;
	} else {
		lStyle &= ~WS_SYSMENU;
	}
	SetWindowLong(gdmgr._hwnd_main, GWL_STYLE, lStyle);
	return;
}

#define MAIN_WIDTH_FULL				760
#define MAIN_WIDTH_PART				540
#define MAIN_HEIGHT					600

void left_top(POINT *p)
{
	
	int				cxScreen = GetSystemMetrics(SM_CXSCREEN);
	int				cyScreen = GetSystemMetrics(SM_CYSCREEN);
	POINT			point;

	point.x = (cxScreen - ((gdmgr._dvrsrc != dsrc_unknown)? MAIN_WIDTH_FULL: MAIN_WIDTH_PART)) / 2;  
	point.y = (cyScreen - MAIN_HEIGHT) / 2;  

	*p = point;
	return;
}

#define dvrmgr_guid_str		"FEBB75D4-3985-4f16-A326-9343E58C1925"	// this is generated by GuidGen.exe
// 返回值：
//  FALSE: 此是程序第一次运行
//  TRUE: 程序已在运行
HANDLE		hvtdvrmgr = NULL;
BOOL make_run_once(void)
{
	HWND			hwnd;

	hvtdvrmgr = CreateMutex(NULL, FALSE, dvrmgr_guid_str);
	if (GetLastError() == ERROR_ALREADY_EXISTS) {	
		// 寻找主窗口，并把它置到前台运行
		hwnd = FindWindow(CLASSNAME, NULL);
		if (IsWindow(hwnd)) {
			ShowWindow(hwnd, SW_RESTORE);
			SetForegroundWindow(hwnd);
		}
		return TRUE;	
	}
	return FALSE;
}

int PASCAL WinMain(HINSTANCE inst, HINSTANCE, LPSTR lpCmdLine, int nCmdShow)
{
	POINT					lefttop;
    MSG						msg = {0};
    WNDCLASS				wc;
	INITCOMMONCONTROLSEX	cc;
/*
	errno_t err = _putenv_s("LC_ALL", "en_GB");
	BOOL fok = SetEnvironmentVariable("LC_ALL", "en_GB");
*/
	_CrtSetDbgFlag (_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	char* ptr = (char*)malloc(41);


	if (make_run_once()) {
		// 程序已在运行，直接退出
		return 0;
	}

	memset(&gdmgr, 0, sizeof(dvrmgr_t));
	gdmgr.heros_.realloc_hero_map(HEROS_MAX_HEROS);

	if (lpCmdLine[0]) {
		if (lpCmdLine[0] == '\"') {
            strncpy(gszSrcFileInCmdLine, lpCmdLine + 1,  strlen(lpCmdLine) - 2);
		} else {
			strcpy(gszSrcFileInCmdLine, lpCmdLine);
		}
		// 如果字符串是被" "包围的,首先去掉这字符串
		if (gszSrcFileInCmdLine[1] != ':') {
			posix_print_mb("lpCmdLine is not NULL, buf isn't a valid full path. %s", lpCmdLine);
			goto exit;
		}
	}  else {
		gszSrcFileInCmdLine[0] = 0;
	}
		
	gdmgr._hinst = inst;
	init_dvrmgr_struct();
	
	init_locale();

	//
	// Initialize the common controls
	//
	cc.dwSize = sizeof(INITCOMMONCONTROLSEX);
	cc.dwICC = ICC_BAR_CLASSES | ICC_TAB_CLASSES | ICC_INTERNET_CLASSES | ICC_PROGRESS_CLASS /*| ICC_LINK_CLASS*/;
	InitCommonControlsEx(&cc);

	gdmgr._dvrsrc = dsrc_removabledisk;

	// Register the window class
    ZeroMemory(&wc, sizeof wc);
    wc.hInstance		= inst;
    wc.lpfnWndProc		= WndMainProc;
    wc.lpszClassName	= CLASSNAME;
    wc.lpszMenuName		= NULL; // MAKEINTRESOURCE(IDR_MENU);
	// wc.hbrBackground	= (HBRUSH)GetStockObject(/*WHITE_BRUSH*/ /* LTGRAY_BRUSH */ NULL_BRUSH);
	wc.hbrBackground    = GetSysColorBrush(COLOR_BTNFACE);
    wc.hCursor			= LoadCursor(NULL, IDC_ARROW);
    wc.hIcon			= gdmgr._markico; // LoadIcon(hInstC, MAKEINTRESOURCE(IDI_VISUALFMTFILE));
	wc.style			= CS_DBLCLKS;	// 使窗口能接收鼠标双击事件
    if(!RegisterClass(&wc)) {
        posix_print_mb("RegisterClass Failed! Error=0x%x", GetLastError());
        return 1;
    }

	left_top(&lefttop);
    // Create the main window.  The WS_CLIPCHILDREN style is required.
    gdmgr._hwnd_main = CreateWindow(CLASSNAME, PROG_NAME,
                         WS_OVERLAPPEDWINDOW | WS_CAPTION | WS_CLIPCHILDREN,
                         lefttop.x, lefttop.y, // CW_USEDEFAULT, CW_USEDEFAULT,
						 (gdmgr._dvrsrc != dsrc_unknown)? MAIN_WIDTH_FULL: MAIN_WIDTH_PART, 600, // 600,
                         0, 0, inst, 0);

	SetWindowText(gdmgr._hwnd_main, formatstr("%s(%s)", PROG_NAME, __DATE__));
	ShowWindow(gdmgr._hwnd_main, nCmdShow);

    if(gdmgr._hwnd_main) {
        // Main message loop
        while(GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    } else {
        posix_print_mb("Failed to create the main window! Error=0x%x", GetLastError());
    }

exit:
	uninit_dvrmgr_struct();

	CloseHandle(hvtdvrmgr);
	
	return (int) msg.wParam;
}