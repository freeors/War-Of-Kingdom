#define GETTEXT_DOMAIN "wesnoth-maker"

#include "global.hpp"
#include "malloc.h"
#include "game_config.hpp"
#include "loadscreen.hpp"
#include "filesystem.hpp"
#include "editor.hpp"
#include "serialization/parser.hpp"
#include "formula_string_utils.hpp"
#include "language.hpp"
#ifndef _ROSE_EDITOR
#include "unit_types.hpp"
#endif
#include "animation.hpp"
#include "builder.hpp"
#include "rectangle.hpp"
#include "wml_exception.hpp"

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
#include <sstream>
#include <iosfwd>
#include <boost/foreach.hpp>

extern BOOL CALLBACK DlgTitleProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam);
extern BOOL CALLBACK DlgDDescProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam);
extern BOOL CALLBACK DlgSyncProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam);
extern BOOL CALLBACK DlgWGenProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam);
extern BOOL CALLBACK DlgCoreProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam);
extern BOOL CALLBACK DlgCfgProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam);
extern BOOL CALLBACK DlgCampaignProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam);
extern BOOL CALLBACK DlgIntegrateProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam);

#define CLASSNAME			"slgmaker"

static char			gtext[_MAX_PATH];
dvrmgr_t			gdmgr;

editor editor_;
tmod_config mod_config;

#define EDITOR_ICO					"editor.ico"

namespace editor_config
{
	config data_cfg;
	int type = BIN_WML;
	std::string campaign_id;

	std::vector<std::pair<std::string, std::string> > arms;
#ifndef _ROSE_EDITOR
	std::vector<std::pair<std::string, const unit_type*> > artifical_utype;
	std::vector<std::pair<std::string, const unit_type*> > city_utypes;
	std::vector<std::pair<std::string, const unit_type*> > troop_utypes;
#endif
	std::vector<std::string> navigation;
	std::vector<std::pair<std::string, const config*> > city_traits;
	std::vector<std::pair<std::string, const config*> > troop_traits;

	std::set<int> unallocatable_heros;

void generate_unallocatable_heros()
{
	for (int number = hero::number_system_min; number <= hero::number_system_max; number ++) {
		unallocatable_heros.insert(number);
	}
	for (int number = hero::number_soldier_min; number <= hero::number_soldier_max; number ++) {
		unallocatable_heros.insert(number);
	}
	for (int number = hero::number_artifical_min; number <= hero::number_artifical_max; number ++) {
		unallocatable_heros.insert(number);
	}
	for (int number = hero::number_commoner_min; number <= hero::number_commoner_max; number ++) {
		unallocatable_heros.insert(number);
	}
}

void reload_data_bin()
{
	const config& game_cfg = data_cfg.child("game_config");
	game_config::load_config(game_cfg? &game_cfg : NULL);
#ifndef _ROSE_EDITOR
	unit_types.set_config(data_cfg.child("units"));

	const unit_type_data::unit_type_map& types = unit_types.types();

	arms.clear();
	const std::vector<std::string>& ids = unit_types.arms_ids();
	int index = 0;
	for (std::vector<std::string>::const_iterator it = ids.begin(); it != ids.end(); ++ it) {
		arms.push_back(std::make_pair(*it, utf8_2_ansi(hero::arms_str(index ++).c_str())));
	}

	artifical_utype.clear();
	for (int number = hero::number_artifical_min; number <= hero::number_artifical_max; number ++) {
		if (number != hero::number_keep) {
			const unit_type* ut = unit_types.master_type(number);
			if (ut) {
				artifical_utype.push_back(std::make_pair(ut->id(), ut));
			}
		}
	}

	city_utypes.clear();
	for (unit_type_data::unit_type_map::const_iterator it = types.begin(); it != types.end(); ++ it) {
		const unit_type& ut = it->second;
		if (ut.packer()) {
			continue;
		}
		if (ut.master() != HEROS_INVALID_NUMBER) {
			continue;
		}
		if (!ut.can_recruit()) {
			continue;
		}
		city_utypes.push_back(std::make_pair(it->first, &ut));
	}

	troop_utypes.clear();
	for (unit_type_data::unit_type_map::const_iterator it = types.begin(); it != types.end(); ++ it) {
		const unit_type& ut = it->second;
		if (ut.packer()) {
			continue;
		}
		if (ut.master() != HEROS_INVALID_NUMBER) {
			continue;
		}

		if (ut.can_recruit() || ut.can_reside()) {
			continue;
		}
		troop_utypes.push_back(std::make_pair(it->first, &ut));
	}

	if (navigation.empty()) {
		const navigation_types& navigation_types = unit_types.navigation_threshold();
		for (navigation_types::const_iterator it = navigation_types.begin(); it != navigation_types.end(); ++ it) {
			navigation.push_back(*it);
		}
	}

	city_traits.clear();
	const traits_map& traits = unit_types.traits();
	for (traits_map::const_iterator it = traits.begin(); it != traits.end(); ++ it) {
		city_traits.push_back(std::make_pair(it->first, &it->second));
	}
	troop_traits.clear();
	for (traits_map::const_iterator it = traits.begin(); it != traits.end(); ++ it) {
		troop_traits.push_back(std::make_pair(it->first, &it->second));
	}
#endif
}

void move_subcfg_right_position(HWND hdlgP, LPARAM lParam)
{
	RECT		rcMe;
	POINT		ptTrigger;
	int			cx, cy, cxScreen, cyScreen;

	cxScreen = GetSystemMetrics(SM_CXSCREEN);
	cyScreen = GetSystemMetrics(SM_CYSCREEN);

	HWND taskbar = FindWindow( "shell_traywnd", NULL);
	RECT taskbar_rect;
	GetWindowRect(taskbar, &taskbar_rect);

	if (taskbar_rect.right - taskbar_rect.left != cxScreen) {
		cxScreen -= taskbar_rect.right - taskbar_rect.left;
	}
	if (taskbar_rect.bottom - taskbar_rect.top != cyScreen) {
		cyScreen -= taskbar_rect.bottom - taskbar_rect.top;
	}

    ptTrigger.x = LOWORD(lParam);
	ptTrigger.y = HIWORD(lParam);

	GetWindowRect(hdlgP, &rcMe);
	cx = rcMe.right - rcMe.left;
	cy = rcMe.bottom - rcMe.top;

	// 是否可以显示在右上角
	if (ptTrigger.x >= cx) {
		// x轴上，左边能显示
		if (ptTrigger.y >= cy) {
			// 左上角（优先）
			MoveWindow(hdlgP, ptTrigger.x - cx, ptTrigger.y - cy, cx, cy, FALSE);
		} else {
			// 左下角
			MoveWindow(hdlgP, ptTrigger.x - cx, std::min<int>(ptTrigger.y, cyScreen - cy), cx, cy, FALSE);
		}
	} else {
		// x轴上，左边能显示，须显示右边
		if (ptTrigger.y >= cy) {
			// 右上角
			MoveWindow(hdlgP, ptTrigger.x, ptTrigger.y - cy, cx, cy, FALSE);
		} else {
			// 右下角
			MoveWindow(hdlgP, ptTrigger.x, std::min<int>(ptTrigger.y, cyScreen -cy), cx, cy, FALSE);
		}
	}
	
	return;
}

void create_subcfg_toolbar(HWND hwndP)
{
	TBBUTTON		tbBtns[5];
	int				cxblank;
	RECT			rc;


	GetClientRect(hwndP, &rc);
	cxblank = (rc.right - rc.left) - 4 * 24 - 6;

	if (gdmgr._htb_subcfg) {
		DestroyWindow(gdmgr._htb_subcfg);
	}

	// Create a toolbar
	gdmgr._htb_subcfg = CreateWindowEx(0, TOOLBARCLASSNAME, (LPSTR)NULL, 
		WS_CHILD | CCS_ADJUSTABLE | TBSTYLE_TOOLTIPS | TBSTYLE_FLAT, 0, 0, 100, 0, hwndP, 
		(HMENU)IDR_WGENMENU, gdmgr._hinst, NULL);

	// Send the TB_BUTTONSTRUCTSIZE message, which is required for backward compatibility
	ToolBar_ButtonStructSize(gdmgr._htb_subcfg, sizeof(TBBUTTON));
	ToolBar_SetButtonSize(gdmgr._htb_subcfg, 30, 30);

	tbBtns[0].iBitmap = MAKELONG(gdmgr._iico_reset, 0);
	tbBtns[0].idCommand = IDM_RESET;	
	tbBtns[0].fsState = TBSTATE_ENABLED;
	tbBtns[0].fsStyle = BTNS_BUTTON;
	tbBtns[0].dwData = 0L;
	tbBtns[0].iString = -1;

	ToolBar_AddButtons(gdmgr._htb_subcfg, 1, &tbBtns);

	ToolBar_AutoSize(gdmgr._htb_subcfg);
	ToolBar_SetExtendedStyle(gdmgr._htb_subcfg, TBSTYLE_EX_DRAWDDARROWS);
	ToolBar_SetImageList(gdmgr._htb_subcfg, gdmgr._himl_24x24, 0);
	
	ShowWindow(gdmgr._htb_subcfg, SW_SHOW);

	SetParent(GetDlgItem(hwndP, IDOK), gdmgr._htb_subcfg);
	SetParent(GetDlgItem(hwndP, IDCANCEL), gdmgr._htb_subcfg);

	TOOLINFO	ti;
    RECT		rect;
	// CREATE A TOOLTIP WINDOW for OK
	if (gdmgr._tt_ok) {
		DestroyWindow(gdmgr._tt_ok);
	}
    gdmgr._tt_ok = CreateWindowEx(WS_EX_TOPMOST,
        TOOLTIPS_CLASS,
        NULL,
        WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,		
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        GetDlgItem(gdmgr._htb_subcfg, IDOK),
        NULL,
        gdmgr._hinst,
        NULL
        );
	// SetWindowPos(hwndTT, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    // GET COORDINATES OF THE MAIN CLIENT AREA
    GetClientRect(GetDlgItem(gdmgr._htb_subcfg, IDOK), &rect);
    // INITIALIZE MEMBERS OF THE TOOLINFO STRUCTURE
    ti.cbSize = sizeof(TOOLINFO);
    ti.uFlags = TTF_SUBCLASS;
    ti.hwnd = GetDlgItem(gdmgr._htb_subcfg, IDOK);
    ti.hinst = gdmgr._hinst;
    ti.uId = 0;
    ti.lpszText = "Save and exit";
    // ToolTip control will cover the whole window
    ti.rect.left = rect.left;    
    ti.rect.top = rect.top;
    ti.rect.right = rect.right;
    ti.rect.bottom = rect.bottom;

    // SEND AN ADDTOOL MESSAGE TO THE TOOLTIP CONTROL WINDOW 
    SendMessage(gdmgr._tt_ok, TTM_ADDTOOL, 0, (LPARAM) (LPTOOLINFO) &ti);

	// CREATE A TOOLTIP WINDOW for CANCEL
	if (gdmgr._tt_cancel) {
		DestroyWindow(gdmgr._tt_cancel);
	}
    gdmgr._tt_cancel = CreateWindowEx(WS_EX_TOPMOST,
        TOOLTIPS_CLASS,
        NULL,
        WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,		
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        GetDlgItem(gdmgr._htb_subcfg, IDCANCEL),
        NULL,
        gdmgr._hinst,
        NULL
        );
	// SetWindowPos(hwndTT, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    // GET COORDINATES OF THE MAIN CLIENT AREA
    GetClientRect(GetDlgItem(gdmgr._htb_subcfg, IDCANCEL), &rect);
    // INITIALIZE MEMBERS OF THE TOOLINFO STRUCTURE
    ti.cbSize = sizeof(TOOLINFO);
    ti.uFlags = TTF_SUBCLASS;
    ti.hwnd = GetDlgItem(gdmgr._htb_subcfg, IDCANCEL);
    ti.hinst = gdmgr._hinst;
    ti.uId = 0;
    ti.lpszText = "Discard and exit";
    // ToolTip control will cover the whole window
    ti.rect.left = rect.left;    
    ti.rect.top = rect.top;
    ti.rect.right = rect.right;
    ti.rect.bottom = rect.bottom;

    // SEND AN ADDTOOL MESSAGE TO THE TOOLTIP CONTROL WINDOW 
    SendMessage(gdmgr._tt_cancel, TTM_ADDTOOL, 0, (LPARAM) (LPTOOLINFO) &ti);

	return;
}

void On_DlgDrawItem(HWND hdlgP, const DRAWITEMSTRUCT *lpdis)
{
	HDC					hdcMem; 
    hdcMem = CreateCompatibleDC(lpdis->hDC); 

	if (lpdis->CtlID == IDOK) {
		if (lpdis->itemState & ODS_SELECTED) { // if selected 
            SelectObject(hdcMem, gdmgr._hbm_ok_select); 
		} else {
			SelectObject(hdcMem, gdmgr._hbm_ok_unselect); 
		}
	} else {
		// (lpdis->CtlID == IDCANCEL)
		if (lpdis->itemState & ODS_SELECTED) { // if selected 
            SelectObject(hdcMem, gdmgr._hbm_cancel_select); 
		} else {
			SelectObject(hdcMem, gdmgr._hbm_cancel_unselect); 
		}
	}

    // Destination 
    StretchBlt( 
                lpdis->hDC,         // destination DC 
                lpdis->rcItem.left, // x upper left 
                lpdis->rcItem.top,  // y upper left 
 
                // The next two lines specify the width and 
                // height. 
                lpdis->rcItem.right - lpdis->rcItem.left, 
                lpdis->rcItem.bottom - lpdis->rcItem.top, 
                hdcMem,    // source device context 
                0, 0,      // x and y upper left 
                24,        // source bitmap width 
                24,        // source bitmap height 
                SRCCOPY);  // raster operation 

	DeleteDC(hdcMem); 

	return;
}

UINT ListView_GetCheckedCount(HWND hwnd)
{
	int	count, idx, checkedcount;

	count = ListView_GetItemCount(hwnd);
	for (idx = 0, checkedcount = 0; idx < count; idx ++) {
		if (ListView_GetCheckState(hwnd, idx)) {
			checkedcount ++;
		}
	}
	return checkedcount;
}

DLGTEMPLATE* WINAPI DoLockDlgRes(LPCSTR lpszResName)
{ 
	HRSRC hrsrc = FindResource(NULL, lpszResName, RT_DIALOG); 
	HGLOBAL hglb = LoadResource(gdmgr._hinst, hrsrc); 
	return (DLGTEMPLATE *)LockResource(hglb); 
}

}

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

#define null_2_space(c) ((c) != '\0'? (c): '$')
#define null_2_null(c) ((c) != '\0'? (c): '')
std::string format_t_terrain(const t_translation::t_terrain& t)
{
	char text[MAX_PATH];
	sprintf(text, "(%c%c%c%c, %c%c%c%c)", 
		null_2_space((t.base & 0xff000000) >> 24), null_2_space((t.base & 0xff0000) >> 16), null_2_space((t.base & 0xff00) >> 8), null_2_space(t.base & 0xff),
		null_2_space((t.overlay & 0xff000000) >> 24), null_2_space((t.overlay & 0xff0000) >> 16), null_2_space((t.overlay & 0xff00) >> 8), null_2_space(t.overlay & 0xff));

	return std::string(text);
}

std::string t_terrain_2_str(const t_translation::t_terrain& t)
{
	std::stringstream strstr;

	if (t.base != t_translation::NO_LAYER) {
		for (int i = 24; i >= 0; i -= 8) {
			int mask = 0xff << i;
			char c = (t.base & mask) >> i;
			if (c == '\0') break;
			strstr << c;
		}
	}
	if (t.overlay != t_translation::NO_LAYER) {
		strstr << '^';
		for (int i = 24; i >= 0; i -= 8) {
			int mask = 0xff << i;
			char c = (t.overlay & mask) >> i;
			if (c == '\0') break;
			strstr << c;
		}
	}
	return strstr.str();
}

std::string hex_t_terrain(const t_translation::t_terrain& t)
{
	char text[MAX_PATH];
	sprintf(text, "(0x%02x%02x%02x%02x, 0x%02x%02x%02x%02x)", 
		(t.base & 0xff000000) >> 24, (t.base & 0xff0000) >> 16, (t.base & 0xff00) >> 8, t.base & 0xff,
		(t.overlay & 0xff000000) >> 24, (t.overlay & 0xff0000) >> 16, (t.overlay & 0xff00) >> 8, t.overlay & 0xff);

	return std::string(text);
}

const char* dgettext_2_ansi(const char* domain, const char* msgid)
{
	if (!msgid || msgid[0] == '\0') {
		return null_str.c_str();
	}
	return utf8_2_ansi(dsgettext(domain, msgid));
}

std::string vgettext2(const char *msgid, const utils::string_map& symbols)
{
	return vgettext("wesnoth-maker", msgid, symbols);
}

static bool is_id_char(char c) 
{
	return ((c == '_') || (c == '-') || (c == ' '));
}

static bool is_variable_char(char c) 
{
	return ((c == '_') || (c == '-') || (c == '.'));
}

typedef bool (*is_xxx_char)(char c);

bool isvalid_id_base(const std::string& id, is_xxx_char fn)
{
	std::string str = id;
	const size_t alnum = std::count_if(str.begin(), str.end(), isalnum);
	const size_t valid_char = std::count_if(str.begin(), str.end(), fn);
	if ((alnum + valid_char != str.size()) || valid_char == str.size() || str.empty() || !isalpha(str.at(0))) {
		return false;
	}
	return str != "null";
}

bool isvalid_id(const std::string& id)
{
	std::string str = id;
	const size_t alnum = std::count_if(str.begin(), str.end(), isalnum);
	const size_t valid_char = std::count_if(str.begin(), str.end(), is_id_char);
	if ((alnum + valid_char != str.size()) || valid_char == str.size() || str.empty() || is_id_char(str.at(0))) {
		return false;
	}
	return str != "null";
}

bool isvalid_variable(const std::string& id)
{
	return isvalid_id_base(id, is_variable_char);
}

void init_dvrmgr_struct(void) 
{
	char		text[_MAX_PATH];

	// oemini文件全路径名
	GetCurrentExePath(gdmgr._curexedir);
	SHGetFolderPath(NULL, CSIDL_PERSONAL, NULL, 0, text);
	sprintf(gdmgr._userdir, "%s\\My Games\\kingdom", text);
	MakeDirectory(std::string(gdmgr._userdir));
	
	SHGetFolderPath(NULL, CSIDL_PROGRAM_FILES, NULL, 0, gdmgr._programfilesdir);
	sprintf(gtext, "%s\\%s", gdmgr._curexedir, EDITOR_ICO);
	// gdmgr._markico = (HICON)LoadImage(gdmgr._hinst, gtext, IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
	gdmgr._markico = (HICON)LoadImage(gdmgr._hinst, MAKEINTRESOURCE(IDI_EDITOR), IMAGE_ICON, 0, 0, LR_CREATEDIBSECTION);
	gdmgr._welcomebmp = (HBITMAP)LoadImage(gdmgr._hinst, MAKEINTRESOURCE(IDB_WELCOME), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);

	InitializeCriticalSection(&gdmgr._cshbeat);

	//
	// oem.ini
	//
	// 得到公司名，默认空
	strcpy(gdmgr._corp_name, "Share Software (GPL)");
	strcpy(gdmgr._corp_http, "http://www.freeors.com");

	//
	// system
	//
	std::string str = utf8_2_ansi(preferences::get("wwwroot").c_str());
	if (str.empty() || !check_wok_root_folder(str)) {
		SHGetFolderPath(NULL, CSIDL_PERSONAL, NULL, 0, text);
		game_config::path = text;
	} else {
		game_config::path = utf8_2_ansi(str.c_str());
	}
	gdmgr.heros_.set_path(game_config::path);
	std::string hero_filename = get_wml_location("^xwml/hero.dat");
	gdmgr.heros_.map_from_file(hero_filename);

	editor_config::generate_unallocatable_heros();

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

	// check/uncheck
	gdmgr._himl_checkbox = ImageList_Create(15, 15, FALSE, 2, 0);
	ImageList_SetBkColor(gdmgr._himl_checkbox, RGB(255, 255, 255));

    hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_UNCHECK));
    ImageList_AddIcon(gdmgr._himl_checkbox, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_CHECK));
    ImageList_AddIcon(gdmgr._himl_checkbox, hicon);

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
	transparent_24bmp(gdmgr._hbm_refresh_select, 0x00000000);
	gdmgr._hbm_refresh_unselect = (HBITMAP)LoadImage(gdmgr._hinst, MAKEINTRESOURCE(IDB_REFRESH_UNSELECT), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
	transparent_24bmp(gdmgr._hbm_refresh_unselect, 0x00000000);
	gdmgr._hbm_refresh_disable = (HBITMAP)LoadImage(gdmgr._hinst, MAKEINTRESOURCE(IDB_REFRESH_UNSELECT), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
	transparent_24bmp(gdmgr._hbm_refresh_disable, 0x00000000);
}

void prepare_popup_menu()
{
	utils::string_map symbols;

	// 菜单
	// menu item: generate
	gdmgr._hpopup_new = CreatePopupMenu();
	AppendMenu(gdmgr._hpopup_new, MF_STRING, IDM_NEW_EXTRAINSDIST, utf8_2_ansi(_("Extract release package to C:\\kingdom-ins")));
	AppendMenu(gdmgr._hpopup_new, MF_SEPARATOR, 0, NULL);
	AppendMenu(gdmgr._hpopup_new, MF_STRING, IDM_NEW_EXTRAROSE, utf8_2_ansi(_("Extract Rose package")));
	AppendMenu(gdmgr._hpopup_new, MF_SEPARATOR, 0, NULL);
	AppendMenu(gdmgr._hpopup_new, MF_STRING, IDM_NEW_CAMPAIGN, dgettext_2_ansi("wesnoth-lib", "Campaign"));

	// star
	if (mod_config.valid()) {
		gdmgr._hpopup_star = CreatePopupMenu();

		symbols["mod_res_path"] = mod_config.get_path(tmod_config::res_tag);
		symbols["mod_patch_path"] = mod_config.get_path(tmod_config::patch_tag);
		AppendMenu(gdmgr._hpopup_star, MF_STRING, IDM_STAR_RESOURCE, utf8_2_ansi(vgettext2("Generate resource package to $mod_res_path", symbols).c_str()));
		AppendMenu(gdmgr._hpopup_star, MF_STRING, IDM_STAR_PATCH, utf8_2_ansi(vgettext2("Extract different files to $mod_patch_path", symbols).c_str()));

		AppendMenu(gdmgr._hpopup_new, MF_SEPARATOR, 0, NULL);
		AppendMenu(gdmgr._hpopup_new, MF_POPUP, (UINT_PTR)(gdmgr._hpopup_star), utf8_2_ansi(mod_config.name().c_str()));
	} else {
		gdmgr._hpopup_star = NULL;
	}

	// menu item: coherence
	gdmgr._hpopup_explorer = CreatePopupMenu();
	AppendMenu(gdmgr._hpopup_explorer, MF_STRING, IDM_EXPLORER_WML, utf8_2_ansi(_("WML format")));
	// AppendMenu(gdmgr._hpopup_explorer, MF_SEPARATOR, 0, NULL);

	// menu item: delete
	gdmgr._hpopup_delete = CreatePopupMenu();
	AppendMenu(gdmgr._hpopup_delete, MF_STRING, IDM_DELETE_ITEM0, utf8_2_ansi(_("Delete relative *.cfg at the same time")));
	AppendMenu(gdmgr._hpopup_delete, MF_SEPARATOR, 0, NULL);
	AppendMenu(gdmgr._hpopup_delete, MF_STRING, IDM_DELETE_ITEM1, utf8_2_ansi(_("Delete")));
	AppendMenu(gdmgr._hpopup_delete, MF_SEPARATOR, 0, NULL);

	// menu item: delete2
	gdmgr._hpopup_delete2 = CreatePopupMenu();
	AppendMenu(gdmgr._hpopup_delete2, MF_STRING, IDM_DELETE_ITEM0, utf8_2_ansi(_("Delete")));
	AppendMenu(gdmgr._hpopup_delete2, MF_SEPARATOR, 0, NULL);
	AppendMenu(gdmgr._hpopup_delete2, MF_STRING, IDM_DELETE_ITEM1, utf8_2_ansi(_("Delete all")));
	AppendMenu(gdmgr._hpopup_delete2, MF_SEPARATOR, 0, NULL);
	
	// 主菜单要放在子菜单后面, 要确保子菜章句柄已有效
	gdmgr._hpopup_ddesc = CreatePopupMenu();
	AppendMenu(gdmgr._hpopup_ddesc, MF_POPUP, (UINT_PTR)(gdmgr._hpopup_new), utf8_2_ansi(_("New")));
	AppendMenu(gdmgr._hpopup_ddesc, MF_SEPARATOR, 0, NULL);
	AppendMenu(gdmgr._hpopup_ddesc, MF_STRING, IDM_INTEGRATE, utf8_2_ansi(_("Integrate")));
	AppendMenu(gdmgr._hpopup_ddesc, MF_SEPARATOR, 0, NULL);
	AppendMenu(gdmgr._hpopup_ddesc, MF_POPUP, (UINT_PTR)(gdmgr._hpopup_explorer), utf8_2_ansi(_("Explorer")));
	AppendMenu(gdmgr._hpopup_ddesc, MF_SEPARATOR, 0, NULL);
	AppendMenu(gdmgr._hpopup_ddesc, MF_POPUP, (UINT_PTR)(gdmgr._hpopup_delete), utf8_2_ansi(_("Delete")));
	
	// wgen会话时,editor控件上的弹出式菜单
	gdmgr._hpopup_editor = CreatePopupMenu();
	AppendMenu(gdmgr._hpopup_editor, MF_STRING, IDM_ADD, utf8_2_ansi(_("Add...")));
	AppendMenu(gdmgr._hpopup_editor, MF_STRING, IDM_EDIT, utf8_2_ansi(_("Edit...")));
	AppendMenu(gdmgr._hpopup_editor, MF_STRING, IDM_DELETE, utf8_2_ansi(_("Delete...")));

	// core会话时, utype控件上的弹出式菜单
	gdmgr._hpopup_mtype = CreatePopupMenu();
	AppendMenu(gdmgr._hpopup_mtype, MF_STRING, IDM_RESET, utf8_2_ansi(_("Reset all to default")));

	return;
}

void SaveCfgToIni(void)
{
	preferences::set("wwwroot", ansi_2_utf8(game_config::path.c_str()));
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
	if (gdmgr._himl_checkbox) {
		ImageList_Destroy(gdmgr._himl_checkbox);
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
	if (gdmgr._hpopup_new) {
		DestroyMenu(gdmgr._hpopup_new);
	}
	if (gdmgr._hpopup_star) {
		DestroyMenu(gdmgr._hpopup_star);
	}
	if (gdmgr._hpopup_explorer) {
		DestroyMenu(gdmgr._hpopup_explorer);
	}
	if (gdmgr._hpopup_delete) {
		DestroyMenu(gdmgr._hpopup_delete);
	}
	if (gdmgr._hpopup_delete2) {
		DestroyMenu(gdmgr._hpopup_delete2);
	}
	if (gdmgr._hpopup_editor) {
		DestroyMenu(gdmgr._hpopup_editor);
	}
	if (gdmgr._hpopup_candidate) {
		DestroyMenu(gdmgr._hpopup_candidate);
	}
	if (gdmgr._hpopup_mtype) {
		DestroyMenu(gdmgr._hpopup_mtype);
	}
	if (gdmgr._hthdSync) {
		// 2、曾经运行过任务，现在已执行完了任务
		WaitForSingleObject(gdmgr._hthdSync, INFINITE);
		CloseHandle(gdmgr._hthdSync);
		gdmgr._hthdSync = NULL;
	}

	for (std::map<int, tzoom_rect*>::const_iterator it = zoom_rects.begin(); it != zoom_rects.end(); ++ it) {
		delete it->second;
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
	gdmgr._hdlg_ddesc = CreateDialogParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_DDESC), hwndP, DlgDDescProc, NULL);
	GetClientRect(gdmgr._hdlg_ddesc, &rcDDesc);
	MoveWindow(gdmgr._hdlg_ddesc, 0, 0, rcDDesc.right, rcMain.bottom, FALSE);
	ShowWindow(gdmgr._hdlg_ddesc, SW_RESTORE);

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

	// 会话窗口: IDD_CORE
	gdmgr._hdlg_core = CreateDialogParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_CORE), hwndP, DlgCoreProc, NULL);
	MoveWindow(gdmgr._hdlg_core, rcDDesc.right, rcTitle.bottom, rcMain.right - rcDDesc.right, rcMain.bottom - rcTitle.bottom, FALSE);
	ShowWindow(gdmgr._hdlg_core, SW_HIDE);

	// 会话窗口: IDD_VISUAL
	gdmgr._hdlg_visual = CreateDialogParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_VISUAL), hwndP, DlgCfgProc, NULL);
	MoveWindow(gdmgr._hdlg_visual, rcDDesc.right, rcTitle.bottom, rcMain.right - rcDDesc.right, rcMain.bottom - rcTitle.bottom, FALSE);
	ShowWindow(gdmgr._hdlg_visual, SW_HIDE);

	// 会话窗口: IDD_CAMPAIGN
	gdmgr._hdlg_campaign = CreateDialogParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_CAMPAIGN), hwndP, DlgCampaignProc, NULL);
	MoveWindow(gdmgr._hdlg_campaign, rcDDesc.right, rcTitle.bottom, rcMain.right - rcDDesc.right, rcMain.bottom - rcTitle.bottom, FALSE);
	ShowWindow(gdmgr._hdlg_campaign, SW_HIDE);

	// section dialog: IDD_INTEGRATE
	gdmgr._hdlg_integrate = CreateDialogParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_INTEGRATE), hwndP, DlgIntegrateProc, NULL);
	MoveWindow(gdmgr._hdlg_integrate, rcDDesc.right, rcTitle.bottom, rcMain.right - rcDDesc.right, rcMain.bottom - rcTitle.bottom, FALSE);
	ShowWindow(gdmgr._hdlg_integrate, SW_HIDE);

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

	zoom_rects.insert(std::make_pair(tzoom_rect::status, new tstatus_rect(gdmgr._hwnd_status)));
	zoom_rects.insert(std::make_pair(tzoom_rect::ddesc, new tddesc_rect(gdmgr._hdlg_ddesc)));
	zoom_rects.insert(std::make_pair(tzoom_rect::hero, new thero_rect(gdmgr._hdlg_wgen)));
	zoom_rects.insert(std::make_pair(tzoom_rect::sync, new tsync_rect(gdmgr._hdlg_sync)));
	zoom_rects.insert(std::make_pair(tzoom_rect::core, new tcore_rect(gdmgr._hdlg_core)));
	zoom_rects.insert(std::make_pair(tzoom_rect::visual, new tvisual_rect(gdmgr._hdlg_visual)));
	zoom_rects.insert(std::make_pair(tzoom_rect::campaign, new tcampaign_rect(gdmgr._hdlg_campaign)));
	zoom_rects.insert(std::make_pair(tzoom_rect::integrate, new tintegrate_rect(gdmgr._hdlg_integrate)));

	return TRUE;
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
 
	if ((gdmgr._da != da_visual) || (!gdmgr._fFullScreen && !PointInVideoArea(x, y))) {
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
	return;
}

void DVR_OnSize(HWND hdlgP, UINT wParam, int cx, int cy)
{
	if (IsZoomed(hdlgP)) {
		tzoom_rect::calculate_maxs(hdlgP);
	}
	zoom_rects[tzoom_rect::status]->placement(gdmgr._hwnd_status, IsZoomed(hdlgP));
	zoom_rects[tzoom_rect::ddesc]->placement(gdmgr._hdlg_ddesc, IsZoomed(hdlgP));
	zoom_rects[tzoom_rect::sync]->placement(gdmgr._hdlg_sync, IsZoomed(hdlgP));
	zoom_rects[tzoom_rect::hero]->placement(gdmgr._hdlg_wgen, IsZoomed(hdlgP));
	zoom_rects[tzoom_rect::core]->placement(gdmgr._hdlg_core, IsZoomed(hdlgP));
	zoom_rects[tzoom_rect::visual]->placement(gdmgr._hdlg_visual, IsZoomed(hdlgP));
	zoom_rects[tzoom_rect::campaign]->placement(gdmgr._hdlg_campaign, IsZoomed(hdlgP));
	zoom_rects[tzoom_rect::integrate]->placement(gdmgr._hdlg_integrate, IsZoomed(hdlgP));
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
		// 需要主窗口句柄还是有效时调用title_select(da_unknown)
		title_select(da_unknown);

		KillTimer(hwndP, ID_STATUS_TIMER);
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
	bindtextdomain (PACKAGE "-maker", intl_dir.c_str());
	// chage default codeset to utf-8.
	bind_textdomain_codeset (PACKAGE "-hero", "UTF-8");
	bind_textdomain_codeset (PACKAGE "-maker", "UTF-8");

	// set UI language
	// SetEnvironmentVariable("LANG", "zh_CN");
}

bool init_language()
{
	if(!::load_language_list())
		return false;

	if (!::set_language(get_locale()))
		return false;

	return true;
}

void update_locale_dir()
{
	const std::string& intl_dir = get_intl_dir();
	// textdomain(PACKAGE "-hero");
	bindtextdomain (PACKAGE "-hero", intl_dir.c_str());

	// textdomain(PACKAGE "-maker");
	bindtextdomain (PACKAGE "-maker", intl_dir.c_str());

	wml_config_from_file(game_config::path + "/xwml/data.bin", editor_config::data_cfg);
	if (!editor_config::data_cfg.empty()) {
		try {
			editor_config::reload_data_bin();
			// reload languages
			init_language();
		} catch (game::error& e) {
			MessageBox(NULL, e.message.c_str(), "Error", MB_OK | MB_ICONWARNING);
#ifndef _ROSE_EDITOR
			unit_types.clear();
#endif
		}
	}

	gdmgr.heros_.set_path(game_config::path);
	std::string hero_filename = get_wml_location("^xwml/hero.dat");
	gdmgr.heros_.map_from_file(hero_filename);
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

#define MAIN_WIDTH					960
#define MAIN_HEIGHT					720

void left_top(POINT *p)
{
	
	int				cxScreen = GetSystemMetrics(SM_CXSCREEN);
	int				cyScreen = GetSystemMetrics(SM_CYSCREEN);
	POINT			point;

	point.x = (cxScreen - MAIN_WIDTH) / 2;  
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

void handle_app_event(Uint32 type)
{
}

int PASCAL WinMain(HINSTANCE inst, HINSTANCE, LPSTR lpCmdLine, int nCmdShow)
{
	POINT					lefttop;
    MSG						msg = {0};
    WNDCLASS				wc;
	INITCOMMONCONTROLSEX	cc;

	_CrtSetDbgFlag (_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	char* ptr = (char*)malloc(41);
/*
	game_config::secret_key[0] = 0x25;
	game_config::secret_key[1] = 0x7a;
	game_config::secret_key[2] = 0x4b;
	game_config::secret_key[3] = 0xa1;
	game_config::secret_key[4] = 0x98;
	game_config::secret_key[5] = 0xe2;
	// 1101 0111 0010 1000 plaintext
    // 0100 1010 1111 0101 key
	std::string plaintext = read_file("C:\\inetpub\\wwwroot\\bbs\\data\\plugindata\\kingdom\\save\\plain.bin");
	tsaes_encrypt encrypt(plaintext.c_str(), plaintext.size(), game_config::secret_key);
	write_file("C:\\inetpub\\wwwroot\\bbs\\data\\plugindata\\kingdom\\save\\cipher1.bin", encrypt.buf, encrypt.size, false);

	tsaes_decrypt decrypt(encrypt.buf, encrypt.size, game_config::secret_key);
	write_file("C:\\inetpub\\wwwroot\\bbs\\data\\plugindata\\kingdom\\save\\restore1.bin", decrypt.buf, decrypt.size, false);
*/

	if (make_run_once()) {
		// progream has run, exit.
		return 0;
	}

	set_preferences_dir("kingdom");
	preferences::base_manager base;

	try {
		memset(&gdmgr, 0, sizeof(dvrmgr_t));
		gdmgr.heros_.realloc_hero_map(HEROS_MAX_HEROS);

		gdmgr._hinst = inst;
		init_dvrmgr_struct();
		
		init_locale();

		if (check_wok_root_folder(game_config::path)) {
			init_language();
			wml_config_from_file(game_config::path + "/xwml/data.bin", editor_config::data_cfg);
			if (!editor_config::data_cfg.empty()) {
				try {
					editor_config::reload_data_bin();
				} catch (game::error& e) {
					MessageBox(NULL, e.message.c_str(), "Error", MB_OK | MB_ICONWARNING);
#ifndef _ROSE_EDITOR
					unit_types.clear();
#endif
				}
			}
		}

		BOOST_FOREACH (const config& c, editor_config::data_cfg.child_range("generate")) {
			const std::string& type = c["type"].str();
			if (type == "mod") {
				mod_config = tmod_config(c);
				break;
			}
		}

		prepare_popup_menu();
		
		//
		// Initialize the common controls
		//
		cc.dwSize = sizeof(INITCOMMONCONTROLSEX);
		cc.dwICC = ICC_BAR_CLASSES | ICC_TAB_CLASSES | ICC_INTERNET_CLASSES | ICC_PROGRESS_CLASS | ICC_LISTVIEW_CLASSES;
		InitCommonControlsEx(&cc);

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
		if (!RegisterClass(&wc)) {
			posix_print_mb("RegisterClass Failed! Error=0x%x", GetLastError());
			return 1;
		}

		left_top(&lefttop);
		// Create the main window.  The WS_CLIPCHILDREN style is required.
		gdmgr._hwnd_main = CreateWindow(CLASSNAME, PROG_NAME,
							 WS_OVERLAPPEDWINDOW | WS_CAPTION | WS_CLIPCHILDREN,
							 lefttop.x, lefttop.y, // CW_USEDEFAULT, CW_USEDEFAULT,
							 MAIN_WIDTH, MAIN_HEIGHT, // 600,
							 0, 0, inst, 0);

		SetWindowText(gdmgr._hwnd_main, formatstr("%s(%s)", PROG_NAME, __DATE__));
		ShowWindow(gdmgr._hwnd_main, nCmdShow);

		// Main message loop
		while(GetMessage(&msg, NULL, 0, 0)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	} catch (game::error& e) {
		MessageBox(NULL, e.message.c_str(), "Error", MB_OK | MB_ICONWARNING);
		return false;
	} catch (twml_exception& e) {
		e.show();
		return false;
	}

	uninit_dvrmgr_struct();

	CloseHandle(hvtdvrmgr);

	terrain_builder::release_heap();

	return (int) msg.wParam;
}