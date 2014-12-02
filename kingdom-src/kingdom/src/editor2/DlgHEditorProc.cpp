
#define GETTEXT_DOMAIN "wesnoth-maker"

#include "global.hpp"
#include "game_config.hpp"
#include "stdafx.h"
#include <windowsx.h>

#include "resource.h"
#ifndef _ROSE_EDITOR
#include "xfunc.h"
#include "win32x.h"
#include "struct.h"
#include "rectangle.hpp"

#include "wesconfig.h"
#include "gettext.hpp"
#include "filesystem.hpp"
#include "editor.hpp"
#include "unit_types.hpp"
#include "font.hpp"
#include "help.hpp"
#include <integrate.hpp>

#include <sstream>
#include <iosfwd>

BOOL CALLBACK DlgHeroEditProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK DlgAdjustProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

#define wgen_enable_save_btn(fEnable)	ToolBar_EnableButton(gdmgr._htb_wgen, IDM_SAVE, fEnable)
#define wgen_get_save_btn()				(ToolBar_GetState(gdmgr._htb_wgen, IDM_SAVE) & TBSTATE_ENABLED)

typedef struct {
	mgr_action_t	_ma;
} cfgmgr_t;
cfgmgr_t			gcfgmgr;

static char	gtext[_MAX_PATH];
HINSTANCE	hinst = NULL;

// 对话框按钮函数
void OnHeroAddBt(HWND hdlgP);
void OnHeroEditBt(HWND hdlgP);

static void OnBrowseSrcFile(HWND hdlgP);
static void OnBrowseDstFile(HWND hdlgP);
static void OnSaveBt(HWND hdlgP);
static void OnHeroDelBt(HWND hdlgP);
static void OnFindEt(HWND hdlgP, UINT codeNotify);
static void OnFindBt(HWND hdlgP);

BOOL UpdateUIFromFile(HWND hdlgP, char *filename);
static HWND init_toolbar(HINSTANCE hinst, HWND hdlgP);

void extract_and_append(HWND hdlgP, hero_map& heros, int number);
void exchange_hero(HWND hdlgP, hero_map& heros, int a_number, int b_number);

namespace ns {
	int id;
	int that_number;
}

void wgen_enter_ui(void)
{
	// 免得误解
	EnableWindow(GetDlgItem(gdmgr._hdlg_ddesc, IDC_LV_DDESC_EXPLORER), FALSE);	// 灰掉右边视图列表

	StatusBar_Idle();

	strcpy(gdmgr.heros_fname_, gdmgr._menu_text);
	StatusBar_SetText(gdmgr._hwnd_status, 0, gdmgr.heros_fname_);

	wgen_enable_save_btn(FALSE);

	UpdateUIFromFile(gdmgr._hdlg_wgen, gdmgr.heros_fname_);

	// 后面是保留给将来使用按钮,当前版本全都灰掉
	ToolBar_EnableButton(gdmgr._htb_wgen, IDM_NEW, FALSE);
	ToolBar_EnableButton(gdmgr._htb_wgen, IDM_RESET, FALSE);
	Edit_SetText(GetDlgItem(gdmgr._htb_wgen, IDC_ET_WGEN_FIND), NULL);
	ToolBar_EnableButton(gdmgr._htb_wgen, IDM_FIND, FALSE);
	ToolBar_EnableButton(gdmgr._htb_wgen, IDM_DELETE, FALSE);
	ToolBar_EnableButton(gdmgr._htb_wgen, IDM_REFRESH, FALSE);
	ToolBar_EnableButton(gdmgr._htb_wgen, IDM_BACKUP, FALSE);
	ToolBar_EnableButton(gdmgr._htb_wgen, IDM_DELETE, FALSE);
	return;
}

BOOL wgen_hide_ui(void)
{
	int			retval;

	// main_ui_sysmenu(TRUE);

	if (ToolBar_GetState(gdmgr._htb_wgen, IDM_SAVE) & TBSTATE_ENABLED) {
		// 用户修改过了配置,但没保存
		std::stringstream title, message;
		message << utf8_2_ansi(_("Do you want to save modify?"));
		title << utf8_2_ansi(_("Save modify"));
		retval = MessageBox(gdmgr._hwnd_main, message.str().c_str(), title.str().c_str(), MB_YESNO);
		if (retval == IDYES) {
			OnSaveBt(gdmgr._htb_wgen);
		}
	}
	return TRUE;
}

static BOOL file_is_valid(char *filename) 
{
	BYTE		data[10];
	DWORD		byteRtd;
	int			retval;

	if (!filename) {
		return FALSE;
	}

	retval = _access(filename, 00);
	HANDLE h = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, \
			OPEN_EXISTING, 0, NULL);
	if (h == INVALID_HANDLE_VALUE) {
		return FALSE;
	}
	ReadFile(h, data, 10, &byteRtd, NULL);
	if ((byteRtd != 10) || memcmp(data, "#\r\n# <!Autom", 10)) {
		CloseHandle(h);
		return FALSE;
	}
	CloseHandle(h);
	return TRUE;
}

void create_wgen_toolinfo(HWND hwndP)
{
	TOOLINFO	ti;
    RECT		rect;

	// CREATE A TOOLTIP WINDOW for OK
	SetParent(GetDlgItem(hwndP, IDC_BT_WGEN_REBOOT), gdmgr._htb_wgen);

	if (gdmgr._tt_reboot) {
		DestroyWindow(gdmgr._tt_reboot);
	}
	gdmgr._tt_reboot = CreateWindowEx(WS_EX_TOPMOST,
        TOOLTIPS_CLASS,
        NULL,
        WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,		
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        GetDlgItem(gdmgr._htb_wgen, IDC_BT_WGEN_REBOOT),
        NULL,
        gdmgr._hinst,
        NULL
        );
	// SetWindowPos(hwndTT, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    // GET COORDINATES OF THE MAIN CLIENT AREA
    GetClientRect(GetDlgItem(gdmgr._htb_wgen, IDC_BT_WGEN_REBOOT), &rect);
    // INITIALIZE MEMBERS OF THE TOOLINFO STRUCTURE
    ti.cbSize = sizeof(TOOLINFO);
    ti.uFlags = TTF_SUBCLASS;
    ti.hwnd = GetDlgItem(gdmgr._htb_wgen, IDC_BT_WGEN_REBOOT);
    ti.hinst = gdmgr._hinst;
    ti.uId = 0;
    ti.lpszText = "Reboot";
    // ToolTip control will cover the whole window
    ti.rect.left = rect.left;    
    ti.rect.top = rect.top;
    ti.rect.right = rect.right;
    ti.rect.bottom = rect.bottom;

    // SEND AN ADDTOOL MESSAGE TO THE TOOLTIP CONTROL WINDOW 
    SendMessage(gdmgr._tt_reboot, TTM_ADDTOOL, 0, (LPARAM) (LPTOOLINFO) &ti);

	return;
}
/*
const std::string& utf8_2_ansi(const std::string& utf8) 
{
	static std::string tstr = general.name();
	conv_ansi_utf8(tstr, false);
	return tstr;
}
*/
// general.number_决定了向列表控件是添加还是修改
void hero_data_2_lv(HWND hdlgP, hero& general)
{
	LVITEM lvi;
	char text[_MAX_PATH];
	std::string tstr;
	std::stringstream strstr;
	uint16_t count;
	int i, val, column = 0;
	HWND hctl = GetDlgItem(hdlgP, IDC_LV_WGEN_EDITOR);

	count = ListView_GetItemCount(hctl);

	lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
	// 序号
	if (general.number_ >= count) {
		lvi.iItem = count;
	} else {
		lvi.iItem = general.number_;
	}
	lvi.iSubItem = column ++;
	sprintf(text, "%u", general.number_);
	lvi.pszText = text;
	lvi.lParam = (LPARAM)general.number_;
	lvi.iImage = select_iimage_according_fname(text, 0);
	if (lvi.iItem != count) {
		ListView_SetItem(hctl, &lvi);
	} else {
		ListView_InsertItem(hctl, &lvi);
	}

	// 姓名
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	strcpy(text, utf8_2_ansi(general.name().c_str()));
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 姓
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	strcpy(text, utf8_2_ansi(general.surname().c_str()));
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 统率
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	sprintf(text, "%u.%u", fxptoi9(general.leadership_), fxpmod9(general.leadership_));
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 武力
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	sprintf(text, "%u.%u", fxptoi9(general.force_), fxpmod9(general.force_));
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 智力
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	sprintf(text, "%u.%u", fxptoi9(general.intellect_), fxpmod9(general.intellect_));
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 政治
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	sprintf(text, "%u.%u", fxptoi9(general.spirit_), fxpmod9(general.spirit_));
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 魅力
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	sprintf(text, "%u.%u", fxptoi9(general.charm_), fxpmod9(general.charm_));
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 性别
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	strcpy(text, utf8_2_ansi(hero::gender_str(general.gender_).c_str()));
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 图像编号
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	sprintf(text, "%u", general.image_);
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 基本/浮动相性
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	sprintf(text, "%u/%u.%u", general.base_catalog_, fxptoi8(general.float_catalog_), fxpmod8(general.float_catalog_));
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// flag
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	strstr.str("");
	for (i = hero_flag_min; i <= hero_flag_max; i ++) {
		if (general.get_flag(i)) {
			if (!strstr.str().empty()) {
				strstr << ", ";
			}
			strstr << hero::flag_str(i);
		}
	}
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 义理
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	strcpy(text, utf8_2_ansi(general.heart_str().c_str()));
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 特技
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	if (general.feature_ != HEROS_NO_FEATURE) {
		strcpy(text, utf8_2_ansi(hero::feature_str(general.feature_).c_str()));
		lvi.pszText = text;
	} else {
		lvi.pszText = NULL;
	}
	ListView_SetItem(hctl, &lvi);

	// 势力特技
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	if (general.side_feature_ != HEROS_NO_FEATURE) {
		strcpy(text, utf8_2_ansi(hero::feature_str(general.side_feature_).c_str()));
		lvi.pszText = text;
	} else {
		lvi.pszText = NULL;
	}
	ListView_SetItem(hctl, &lvi);

	// Cost
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	sprintf(text, "%i", general.cost_);
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 性格
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	if (general.character_ != HEROS_NO_CHARACTER) {
		strcpy(text, utf8_2_ansi(unit_types.character(general.character_).name().c_str()));
		lvi.pszText = text;
	} else {
		lvi.pszText = NULL;
	}
	ListView_SetItem(hctl, &lvi);

	// 战法
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	if (general.tactic_ != HEROS_NO_TACTIC) {
		strcpy(text, utf8_2_ansi(unit_types.tactic(general.tactic_).name().c_str()));
		lvi.pszText = text;
	} else {
		lvi.pszText = NULL;
	}
	ListView_SetItem(hctl, &lvi);

	// 兵种
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	if (general.utype_ != HEROS_NO_UTYPE) {
		const unit_type* ut = unit_types.keytype2(general.utype_);
		if (ut) {
			strcpy(text, utf8_2_ansi(ut->type_name().c_str()));
		} else {
			strcpy(text, utf8_2_ansi(_("Invalid")));
			posix_print_mb("Invalid unit type in %s.", utf8_2_ansi(general.name().c_str()));
		}
		lvi.pszText = text;
	} else {
		lvi.pszText = NULL;
	}
	ListView_SetItem(hctl, &lvi);

	// 步兵适性
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	sprintf(text, "%s.%u", utf8_2_ansi(hero::adaptability_str2(general.arms_[hero_arms_t0]).c_str()), fxpmod12(general.arms_[0]));
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 骑兵适性
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	sprintf(text, "%s.%u", utf8_2_ansi(hero::adaptability_str2(general.arms_[hero_arms_t1]).c_str()), fxpmod12(general.arms_[1]));
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 兵器适性
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	sprintf(text, "%s.%u", utf8_2_ansi(hero::adaptability_str2(general.arms_[hero_arms_t2]).c_str()), fxpmod12(general.arms_[2]));
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 学院适性
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	sprintf(text, "%s.%u", utf8_2_ansi(hero::adaptability_str2(general.arms_[hero_arms_t3]).c_str()), fxpmod12(general.arms_[3]));
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 水军适性
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	sprintf(text, "%s.%u", utf8_2_ansi(hero::adaptability_str2(general.arms_[hero_arms_t4]).c_str()), fxpmod12(general.arms_[4]));
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// skill: commercial
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	sprintf(text, "%s.%u", utf8_2_ansi(hero::adaptability_str2(general.skill_[hero_skill_commercial]).c_str()), fxpmod12(general.skill_[hero_skill_commercial]));
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// skill: technology
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	sprintf(text, "%s.%u", utf8_2_ansi(hero::adaptability_str2(general.skill_[hero_skill_invent]).c_str()), fxpmod12(general.skill_[hero_skill_invent]));
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// skill: encourage
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	sprintf(text, "%s.%u", utf8_2_ansi(hero::adaptability_str2(general.skill_[hero_skill_encourage]).c_str()), fxpmod12(general.skill_[hero_skill_encourage]));
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// skill: hero
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	sprintf(text, "%s.%u", utf8_2_ansi(hero::adaptability_str2(general.skill_[hero_skill_hero]).c_str()), fxpmod12(general.skill_[hero_skill_hero]));
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// skill: demolish
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	sprintf(text, "%s.%u", utf8_2_ansi(hero::adaptability_str2(general.skill_[hero_skill_demolish]).c_str()), fxpmod12(general.skill_[hero_skill_demolish]));
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// skill: formation
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	sprintf(text, "%s.%u", utf8_2_ansi(hero::adaptability_str2(general.skill_[hero_skill_formation]).c_str()), fxpmod12(general.skill_[hero_skill_formation]));
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 父母
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	strstr.str("");
	if (general.parent_[0].hero_ != HEROS_INVALID_NUMBER) {
		strstr << utf8_2_ansi(gdmgr.heros_[general.parent_[0].hero_].name().c_str());
	} else {
		text[0] = '\0';
	}
	val = general.parent_[0].feeling_;
	strstr << "(" << val << ")";

	if (general.parent_[1].hero_ != HEROS_INVALID_NUMBER) {
		strstr << ", " << utf8_2_ansi(gdmgr.heros_[general.parent_[1].hero_].name().c_str());
	}
	val = general.parent_[1].feeling_;
	strstr << "(" << val << ")";
	strcpy(text, strstr.str().c_str());
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 配偶
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	strstr.str("");
	for (i = 0; i < HEROS_MAX_CONSORT; i ++) {
		if (general.consort_[i].hero_ != HEROS_INVALID_NUMBER) {
			if (i == 0) {
				strstr << utf8_2_ansi(gdmgr.heros_[general.consort_[i].hero_].name().c_str());
			} else {
				strstr << ", " << utf8_2_ansi(gdmgr.heros_[general.consort_[i].hero_].name().c_str());
			}
		}
		val = general.consort_[i].feeling_;
		strstr << "(" << val << ")";
	}
	strcpy(text, strstr.str().c_str());
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 义兄弟
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	strstr.str("");
	for (i = 0; i < HEROS_MAX_OATH; i ++) {
		if (general.oath_[i].hero_ != HEROS_INVALID_NUMBER) {
			if (i == 0) {
				strstr << utf8_2_ansi(gdmgr.heros_[general.oath_[i].hero_].name().c_str());
			} else {
				strstr << ", " << utf8_2_ansi(gdmgr.heros_[general.oath_[i].hero_].name().c_str());
			}
		}
		val = general.oath_[i].feeling_;
		strstr << "(" << val << ")";
	}
	strcpy(text, strstr.str().c_str());
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 亲近武将
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	strstr.str("");
	for (i = 0; i < HEROS_MAX_INTIMATE; i ++) {
		if (general.intimate_[i].hero_ != HEROS_INVALID_NUMBER) {
			if (i == 0) {
				strstr << utf8_2_ansi(gdmgr.heros_[general.intimate_[i].hero_].name().c_str());
			} else {
				strstr << ", " << utf8_2_ansi(gdmgr.heros_[general.intimate_[i].hero_].name().c_str());
			}
		}
		val = general.intimate_[i].feeling_;
		strstr << "(" << val << ")";
	}
	strcpy(text, strstr.str().c_str());
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 厌恶武将
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	strstr.str("");
	for (i = 0; i < HEROS_MAX_HATE; i ++) {
		if (general.hate_[i].hero_ != HEROS_INVALID_NUMBER) {
			if (i == 0) {
				strstr << utf8_2_ansi(gdmgr.heros_[general.hate_[i].hero_].name().c_str());
			} else {
				strstr << ", " << utf8_2_ansi(gdmgr.heros_[general.hate_[i].hero_].name().c_str());
			}
		}
		val = general.hate_[i].feeling_;
		strstr << "(" << val << ")";
	}
	strcpy(text, strstr.str().c_str());
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 功勋
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	sprintf(text, "%u", general.meritorious_);
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 状态
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	sprintf(text, "%s/%u", utf8_2_ansi(hero::status_str(general.status_).c_str()), general.activity_);
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 官职
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	sprintf(text, utf8_2_ansi(hero::official_str(general.official_).c_str()));
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 宝物
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	strstr.str("");
	if (general.treasure_ != HEROS_NO_TREASURE) {
		strstr << unit_types.treasure(general.treasure_).name();
	}
	val = general.treasure_;
	strstr << "(" << val << ")";
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 列传
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	lvi.pszText = const_cast<char*>(utf8_2_ansi(general.biography()));

	try {
		// It is deathful fault if there is error in biography. 
		// Check it first before kingdom.exe!
		tintegrate integrate(general.biography(), 480, -1, 0, font::BIGMAP_COLOR);
	}
	catch (game::error& e) {
		std::stringstream msg;
		int number = general.number_;
		msg << "Parse " << general.name() << "(" << number << ")'s biography error! Detail: " << e.message;
		MessageBox(NULL, utf8_2_ansi(msg.str().c_str()), "Error", MB_OK | MB_ICONWARNING);
	}

	ListView_SetItem(hctl, &lvi);

	return;
}

#define GetCfgDescHandle()	GetDlgItem(gdmgr._htb_wgen, IDC_CMB_WGEN_CFGDESC_SRC)
// 对话框消息处理函数
BOOL On_DlgWGenInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	HWND hctl = GetDlgItem(hdlgP, IDC_LV_WGEN_EDITOR);
	LVCOLUMN lvc;
	char text[_MAX_PATH];
	std::stringstream strstr;
	int column = 0;

	gdmgr._hdlg_wgen = hdlgP;
	gdmgr._htb_wgen = init_toolbar(gdmgr._hinst, hdlgP);
	SetParent(GetDlgItem(hdlgP, IDC_ET_WGEN_FIND), gdmgr._htb_wgen);

	//
	// 初始化列视图控件
	//
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 40;
	strcpy(text, utf8_2_ansi(_("Number")));
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = column;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 60;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << "name";
	strcpy(text, utf8_2_ansi(dgettext("wesnoth-hero", strstr.str().c_str())));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 48;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << "surname";
	strcpy(text, utf8_2_ansi(dgettext("wesnoth-hero", strstr.str().c_str())));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 54;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << "leadership";
	strcpy(text, utf8_2_ansi(dgettext("wesnoth-hero", strstr.str().c_str())));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 54;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << "force";
	strcpy(text, utf8_2_ansi(dgettext("wesnoth-hero", strstr.str().c_str())));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 54;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << "intellect";
	strcpy(text, utf8_2_ansi(dgettext("wesnoth-hero", strstr.str().c_str())));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 54;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << "spirit";
	strcpy(text, utf8_2_ansi(dgettext("wesnoth-hero", strstr.str().c_str())));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 54;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << "charm";
	strcpy(text, utf8_2_ansi(dgettext("wesnoth-hero", strstr.str().c_str())));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << "gender";
	strcpy(text, utf8_2_ansi(dgettext("wesnoth-hero", strstr.str().c_str())));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = column;
	strstr.str("");
	strcpy(text, utf8_2_ansi(_("Picture number")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 90;
	lvc.iSubItem = column;
	strcpy(text, utf8_2_ansi(_("Base/Float catalog")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 60;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << "flag";
	strcpy(text, utf8_2_ansi(dgettext("wesnoth-hero", strstr.str().c_str())));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << "heart";
	strcpy(text, utf8_2_ansi(dsgettext("wesnoth-hero", strstr.str().c_str())));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << "feature";
	strcpy(text, utf8_2_ansi(dgettext("wesnoth-hero", strstr.str().c_str())));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << "side feature";
	strcpy(text, utf8_2_ansi(dgettext("wesnoth-hero", strstr.str().c_str())));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << "Cost";
	strcpy(text, utf8_2_ansi(dgettext("wesnoth-lib", strstr.str().c_str())));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 80;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << "Character";
	strcpy(text, utf8_2_ansi(dgettext("wesnoth-lib", strstr.str().c_str())));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 80;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << "tactic";
	strcpy(text, utf8_2_ansi(dgettext("wesnoth-hero", strstr.str().c_str())));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 80;
	lvc.iSubItem = column;
	strstr.str("");
	strcpy(text, utf8_2_ansi(_("arms^Type")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << "arms-0";
	strcpy(text, utf8_2_ansi(dgettext("wesnoth-hero", strstr.str().c_str())));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << "arms-1";
	strcpy(text, utf8_2_ansi(dgettext("wesnoth-hero", strstr.str().c_str())));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << "arms-2";
	strcpy(text, utf8_2_ansi(dgettext("wesnoth-hero", strstr.str().c_str())));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << "arms-3";
	strcpy(text, utf8_2_ansi(dgettext("wesnoth-hero", strstr.str().c_str())));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << "arms-4";
	strcpy(text, utf8_2_ansi(dgettext("wesnoth-hero", strstr.str().c_str())));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << "skill-0";
	strcpy(text, utf8_2_ansi(dgettext("wesnoth-hero", strstr.str().c_str())));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << "skill-1";
	strcpy(text, utf8_2_ansi(dgettext("wesnoth-hero", strstr.str().c_str())));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << "skill-3";
	strcpy(text, utf8_2_ansi(dgettext("wesnoth-hero", strstr.str().c_str())));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << "skill-4";
	strcpy(text, utf8_2_ansi(dgettext("wesnoth-hero", strstr.str().c_str())));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << "skill-5";
	strcpy(text, utf8_2_ansi(dgettext("wesnoth-hero", strstr.str().c_str())));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << "skill-6";
	strcpy(text, utf8_2_ansi(dgettext("wesnoth-hero", strstr.str().c_str())));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 70;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << "parent";
	strcpy(text, utf8_2_ansi(dgettext("wesnoth-hero", strstr.str().c_str())));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 60;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << "consort";
	strcpy(text, utf8_2_ansi(dgettext("wesnoth-hero", strstr.str().c_str())));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 70;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << "oath";
	strcpy(text, utf8_2_ansi(dgettext("wesnoth-hero", strstr.str().c_str())));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 70;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << "intimate";
	strcpy(text, utf8_2_ansi(dgettext("wesnoth-hero", strstr.str().c_str())));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 70;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << "hate";
	strcpy(text, utf8_2_ansi(dgettext("wesnoth-hero", strstr.str().c_str())));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << "meritorious";
	strcpy(text, utf8_2_ansi(dgettext("wesnoth-hero", strstr.str().c_str())));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 80;
	lvc.iSubItem = column;
	strcpy(text, utf8_2_ansi(_("Status/Activity")));
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 60;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << "official";
	strcpy(text, utf8_2_ansi(dgettext("wesnoth-hero", strstr.str().c_str())));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 100;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << "treasure";
	strcpy(text, utf8_2_ansi(dgettext("wesnoth-hero", strstr.str().c_str())));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 400;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << "biography";
	strcpy(text, utf8_2_ansi(dgettext("wesnoth-hero", strstr.str().c_str())));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	ListView_SetImageList(hctl, gdmgr._himl, LVSIL_SMALL);

	// 默认情况下，鼠标右键只是光亮该行的最前一个子项，并且只有在该子项上才能触发NM_RCLICK。改为光亮整行，并且在整行都能触发NM_RCLICK。
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);
	
	SetFocus(GetCfgDescHandle());

	return FALSE;
}

void On_DlgWGenCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	char		*ptr;

	switch(id) {
	case IDM_ADD:
		OnHeroAddBt(hdlgP);
		break;
	case IDM_DELETE:
		OnHeroDelBt(hdlgP);
		break;

	case IDM_EXCHANGE:
		ns::id = id;
		if (DialogBox(gdmgr._hinst, MAKEINTRESOURCE(IDD_ADJUST), NULL, DlgAdjustProc)) {
			exchange_hero(hdlgP, gdmgr.heros_, gdmgr._menu_lparam, ns::that_number);
		}
		break;
	case IDM_APPEND:
		extract_and_append(hdlgP, gdmgr.heros_, gdmgr._menu_lparam);
		break;

	case IDC_ET_WGEN_FIND:
		OnFindEt(hdlgP, codeNotify);
		break;

	case IDC_BT_WGEN_EDIT:
	case IDM_EDIT:
		OnHeroEditBt(hdlgP);
		break;
	case IDC_CMB_WGEN_CFGDESC_SRC:
		break;
	
		// tool bar button
	case IDM_SAVE:
		OnSaveBt(hdlgP);
		break;
	case IDM_FIND:
		OnFindBt(hdlgP);
		break;
	case IDM_REFRESH:
		wgen_enable_save_btn(FALSE);
		UpdateUIFromFile(hdlgP, gdmgr.heros_fname_);
		break;
	case IDM_IMPORT:
		ptr = GetBrowseFileName_title(game_config::path.c_str(), "*.ini\0*.ini\0\0", TRUE, "Import");
		if (ptr) {
			if (file_is_valid(ptr)) {
				wgen_enable_save_btn(TRUE);	// 导入的，总认为是不同文件
				UpdateUIFromFile(hdlgP, ptr);
			} else {
				posix_print_mb("%s is invalid configuration file", ptr);
			}
		}
		break;
	case IDM_EXPORT:
		ptr = GetBrowseFileName_title(game_config::path.c_str(), "*.ini\0*.ini\0\0", FALSE, "Export");
		if (ptr) {
		}
		break;
	case IDC_BT_WGEN_REBOOT:
		
		break;
	}
}

BOOL UpdateUIFromFile(HWND hdlgP, char* fname)
{
	if (!gdmgr.heros_.map_from_file(fname)) {
		return FALSE;
	}
	for (hero_map::iterator iter = gdmgr.heros_.begin(); iter != gdmgr.heros_.end(); ++ iter) {
		hero_data_2_lv(hdlgP, *iter);
	}

	return TRUE;
}

#define EVAR_DEF_TARGETRATE			6000
#define EVAR_DEF_PEAKRATE			7200
#define EVAR_DEF_MODE				ENCODER_MODE_MPEG4

void OnHeroAddBt(HWND hdlgP)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_LV_WGEN_EDITOR), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);
	
	hero h(gdmgr.heros_.size());
	
	gdmgr.heros_.add(h);
	gdmgr._menu_lparam = h.number_;
	gcfgmgr._ma = ma_new;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_HEROEDIT), hdlgP, DlgHeroEditProc, lParam)) {
		hero_data_2_lv(hdlgP, gdmgr.heros_[gdmgr._menu_lparam]);
		wgen_enable_save_btn(TRUE);
	} else {
		gdmgr.heros_.erase(h.number_);
	}
	return;
}

void OnHeroEditBt(HWND hdlgP)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_LV_WGEN_EDITOR), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);
	gcfgmgr._ma = ma_edit;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_HEROEDIT), hdlgP, DlgHeroEditProc, lParam)) {
		hero_data_2_lv(hdlgP, gdmgr.heros_[gdmgr._menu_lparam]);
		wgen_enable_save_btn(TRUE);
	}
	return;
}

void refresh_editor_use_heros(HWND hdlgP)
{
	HWND hctl = GetDlgItem(hdlgP, IDC_LV_WGEN_EDITOR);

	ListView_DeleteAllItems(hctl);
	hero_map::iterator iter;
	for (iter = gdmgr.heros_.begin(); iter != gdmgr.heros_.end(); ++ iter) {
		hero_data_2_lv(hdlgP, *iter);
	}
}

static void OnHeroDelBt(HWND hdlgP)
{
	int retval;
	char text[_MAX_PATH];
	std::stringstream strstr;
	utils::string_map symbols;

	symbols["name"] = gdmgr.heros_[gdmgr._menu_lparam].name();
	symbols["number"] = lexical_cast_default<std::string>(gdmgr._menu_lparam);
	strstr << utf8_2_ansi(vgettext2("Do you want to delete hero($name) that number is $number?", symbols).c_str());

	strcpy(text, utf8_2_ansi(_("Confirm delete")));
	retval = MessageBox(gdmgr._hwnd_main, strstr.str().c_str(), text, MB_YESNO);
	if (retval == IDNO) {
		return;
	}
	gdmgr.heros_.erase(gdmgr._menu_lparam);
	refresh_editor_use_heros(hdlgP);

	wgen_enable_save_btn(TRUE);

	return;
}

bool verify_hero_map(hero_map& heros)
{
	utils::string_map symbols;
	std::stringstream err;

	for (hero_map::iterator it = heros.begin(); it != heros.end(); ++ it) {
		hero& h = *it;
		for (int i = 0; i < HEROS_MAX_HATE; i ++) {
			if (h.hate_[i].hero_ != HEROS_INVALID_NUMBER) {
				hero& h2 = heros[h.hate_[i].hero_];
				int i2;
				for (i2 = 0; i2 < HEROS_MAX_HATE; i2 ++) {
					if (h2.hate_[i2].hero_ == HEROS_INVALID_NUMBER) {
						// make sure valid number at begin.
						i2 = HEROS_MAX_HATE - 1;
					} else if (h2.hate_[i2].hero_ == h.number_) {
						break;
					}
				}
				if (i2 == HEROS_MAX_HATE) {
					symbols["h1"] = h.name();
					symbols["h2"] = h2.name();
					err << utf8_2_ansi(vgettext2("$h1 hate $h2, but $h2 doesn't hate $h1!", symbols).c_str());
					MessageBox(gdmgr._hwnd_main, err.str().c_str(), "Error", MB_OK);
					return false;
				}
			}
		}
	}
	return true;
}

// 将当前hero_map存成文件
void OnSaveBt(HWND hdlgP)
{
	if (!verify_hero_map(gdmgr.heros_)) {
		return;
	}
	if (gdmgr.heros_.map_to_file(gdmgr.heros_fname_)) {
		wgen_enable_save_btn(FALSE);
	}
}

void OnFindEt(HWND hdlgP, UINT codeNotify)
{
	char text[_MAX_PATH];
	if (codeNotify == EN_CHANGE) {
		Edit_GetText(GetDlgItem(gdmgr._htb_wgen, IDC_ET_WGEN_FIND), text, sizeof(text) / sizeof(text[0]));
		if (strlen(text)) {
			ToolBar_EnableButton(gdmgr._htb_wgen, IDM_FIND, TRUE);
		} else {
			ToolBar_EnableButton(gdmgr._htb_wgen, IDM_FIND, FALSE);
		}
	}
	return;
}

void listview_find_hero(HWND hlistview, HWND find_et)
{
	char text[_MAX_PATH];
	LVITEM lvi;

	Edit_GetText(find_et, text, sizeof(text) / sizeof(text[0]));
	if (!strlen(text)) {
		return;
	}

	RECT line0_rect;
	ListView_GetItemRect(hlistview, 0, &line0_rect, LVIR_BOUNDS);
	int data_line_height = line0_rect.bottom - line0_rect.top;
	size_t first_viewed_line = ListView_GetTopIndex(hlistview);
	size_t count_per_page = ListView_GetCountPerPage(hlistview);

	size_t found, idx, count, hero_size = ListView_GetItemCount(hlistview);
	size_t first_find_line = first_viewed_line + 1;
	
	if (first_viewed_line + count_per_page == hero_size) {
		// current display is last screen. 
		// if select item in this view, first_find_line is next to select item. or next to first viewed item.
		for (idx = first_viewed_line; idx < hero_size; idx ++) {
			if (ListView_GetItemState(hlistview, idx, LVIS_SELECTED)) {
				break;
			}
		}
		if (idx != hero_size) {
			first_find_line = idx + 1;
		}
	}

	for (found = idx = first_find_line; idx < hero_size; found ++, idx ++) {
		lvi.iItem = idx;
		lvi.mask = LVIF_PARAM;
		lvi.iSubItem = 0;
		ListView_GetItem(hlistview, &lvi);
		hero& h = gdmgr.heros_[lvi.lParam];
		size_t size = strlen(text);
		if (!strncasecmp(text, utf8_2_ansi(h.name().c_str()), size)) {
			break;
		}
	}
	if (found == hero_size) {
		for (found = 0, idx = 0; idx < first_find_line; found ++, idx ++) {
			lvi.iItem = idx;
			lvi.mask = LVIF_PARAM;
			lvi.iSubItem = 0;
			ListView_GetItem(hlistview, &lvi);
			hero& h = gdmgr.heros_[lvi.lParam];
			size_t size = strlen(text);
			if (!strncasecmp(text, utf8_2_ansi(h.name().c_str()), size)) {
				break;
			}
		}
		if (found == first_find_line) {
			found = hero_size;
		}
	}

	if (found != hero_size) {
		SetFocus(hlistview);
		count = ListView_GetItemCount(hlistview);
		for (idx = 0; idx < count; idx ++) {
			if (idx != found) {
				ListView_SetItemState(hlistview, idx, 0, LVIS_SELECTED | LVIS_FOCUSED);
			} else {
				ListView_SetItemState(hlistview, idx, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
			}
		}
		ListView_Scroll(hlistview, 0, (found - first_viewed_line) * data_line_height);
	}
	return;
}

void OnFindBt(HWND hdlgP)
{
	listview_find_hero(GetDlgItem(hdlgP, IDC_LV_WGEN_EDITOR), GetDlgItem(gdmgr._htb_wgen, IDC_ET_WGEN_FIND));
/*
	HWND hctl = GetDlgItem(hdlgP, IDC_LV_WGEN_EDITOR);
	char text[_MAX_PATH];

	Edit_GetText(GetDlgItem(gdmgr._htb_wgen, IDC_ET_WGEN_FIND), text, sizeof(text) / sizeof(text[0]));
	if (!strlen(text)) {
		return;
	}

	RECT line0_rect;
	ListView_GetItemRect(hctl, 0, &line0_rect, LVIR_BOUNDS);
	int data_line_height = line0_rect.bottom - line0_rect.top;
	size_t first_viewed_line = ListView_GetTopIndex(hctl);
	size_t count_per_page = ListView_GetCountPerPage(hctl);

	size_t found, idx, count, hero_size = gdmgr.heros_.size();
	size_t first_find_line = first_viewed_line + 1;
	
	if (first_viewed_line + count_per_page == hero_size) {
		// current display is last screen. 
		// if select item in this view, first_find_line is next to select item. or next to first viewed item.
		for (idx = first_viewed_line; idx < hero_size; idx ++) {
			if (ListView_GetItemState(hctl, idx, LVIS_SELECTED)) {
				break;
			}
		}
		if (idx != hero_size) {
			first_find_line = idx + 1;
		}
	}

	for (found = idx = first_find_line; idx < hero_size; found ++, idx ++) {
		hero& h = gdmgr.heros_[idx];
		size_t size = strlen(text);
		if (!strncasecmp(text, utf8_2_ansi(h.name().c_str()), size)) {
			break;
		}
	}
	if (found == hero_size) {
		for (found = 0, idx = 0; idx < first_find_line; found ++, idx ++) {
			hero& h = gdmgr.heros_[idx];
			size_t size = strlen(text);
			if (!strncasecmp(text, utf8_2_ansi(h.name().c_str()), size)) {
				break;
			}
		}
		if (found == first_find_line) {
			found = hero_size;
		}
	}

	if (found != hero_size) {
		SetFocus(hctl);
		count = ListView_GetItemCount(hctl);
		for (idx = 0; idx < count; idx ++) {
			if (idx != found) {
				ListView_SetItemState(hctl, idx, 0, LVIS_SELECTED | LVIS_FOCUSED);
			} else {
				ListView_SetItemState(hctl, idx, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
			}
		}
		ListView_Scroll(hctl, 0, (found - first_viewed_line) * data_line_height);
	}
*/
	return;
}

void On_DlgWGenDestroy(HWND hdlgP)
{
	return;
}

void On_DlgWGenDrawItem(HWND hdlgP, const DRAWITEMSTRUCT *lpdis)
{
	HDC					hdcMem; 
    hdcMem = CreateCompatibleDC(lpdis->hDC); 

	if (lpdis->itemState & ODS_SELECTED) { // if selected 
		SelectObject(hdcMem, gdmgr._hbm_reboot_select); 
	} else if (lpdis->itemState & ODS_DISABLED) {
		SelectObject(hdcMem, gdmgr._hbm_reboot_disable); 
	} else {
		SelectObject(hdcMem, gdmgr._hbm_reboot_unselect); 
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

void wgen_notify_handler_rclick(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	LVITEM					lvi;
	LPNMITEMACTIVATE		lpnmitem;
	int						icount;

	// NM_表示对通用控件都通用,范围丛(0, 99)
	// TVN_表示只能TreeView通用,范围丛(400, 499)
	if ((lpNMHdr->code == NM_RCLICK) && lpNMHdr->hwndFrom == GetDlgItem(hdlgP, IDC_LV_WGEN_EDITOR)) {
		lpnmitem = (LPNMITEMACTIVATE)lpNMHdr;
		// 如果单击到的是复选框位置,把复选框状态置反
		// 当前定义的图标大小是16x16. ptAction反回的(x,y)是整个列表视图内的坐标,因而y值不大好判断
		// 认为如果x是小于16的就认为是击中复选框
		
		// ListView_SetCheckState(lpNMHdr->hwndFrom, lpnmitem->iItem, !ListView_GetCheckState(lpNMHdr->hwndFrom, lpnmitem->iItem));
		POINT		point = {lpnmitem->ptAction.x, lpnmitem->ptAction.y};
		MapWindowPoints(lpNMHdr->hwndFrom, NULL, &point, 1);

		lvi.iItem = lpnmitem->iItem;
		lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
		lvi.iSubItem = 0;
		lvi.pszText = gdmgr._menu_text;
		lvi.cchTextMax = _MAX_PATH;
		ListView_GetItem(lpNMHdr->hwndFrom, &lvi);

		icount = ListView_GetItemCount(lpNMHdr->hwndFrom);

		std::stringstream strstr;
		HMENU hpopup_adjust = CreatePopupMenu();
		strstr.str("");
		strstr << _("Exchange") << "...";
		AppendMenu(hpopup_adjust, MF_STRING, IDM_EXCHANGE, utf8_2_ansi(strstr.str().c_str()));
		strstr.str("");
		strstr << _("Extract and append") << "...";
		AppendMenu(hpopup_adjust, MF_STRING, IDM_APPEND, utf8_2_ansi(strstr.str().c_str()));

		HMENU hpopup_editor = CreatePopupMenu();
		AppendMenu(hpopup_editor, MF_STRING, IDM_ADD, utf8_2_ansi(_("Add...")));
		AppendMenu(hpopup_editor, MF_STRING, IDM_EDIT, utf8_2_ansi(_("Edit...")));

		AppendMenu(hpopup_editor, MF_SEPARATOR, 0, NULL);
		AppendMenu(hpopup_editor, MF_POPUP, (UINT_PTR)(hpopup_adjust), utf8_2_ansi(_("Adjust number")));
		AppendMenu(hpopup_editor, MF_SEPARATOR, 0, NULL);

		AppendMenu(hpopup_editor, MF_STRING, IDM_DELETE, utf8_2_ansi(_("Delete...")));

		EnableMenuItem(hpopup_editor, IDM_ADD, MF_BYCOMMAND);
		if (lpnmitem->iItem < 0) {
			EnableMenuItem(hpopup_editor, IDM_EDIT, MF_BYCOMMAND | MF_GRAYED);
			EnableMenuItem(hpopup_editor, IDM_DELETE, MF_BYCOMMAND | MF_GRAYED);

			EnableMenuItem(hpopup_editor, (UINT_PTR)(hpopup_adjust), MF_BYCOMMAND | MF_GRAYED);
		} else if (wgen_get_save_btn()) {
			EnableMenuItem(hpopup_editor, (UINT_PTR)(hpopup_adjust), MF_BYCOMMAND | MF_GRAYED);
		}

		TrackPopupMenuEx(hpopup_editor, 0, 
			point.x, 
			point.y, 
			hdlgP, 
			NULL);

		DestroyMenu(hpopup_editor);

		gdmgr._menu_lparam = lpnmitem->iItem;
	
	}
    return;
}

void wgen_notify_handler_dblclk(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	LVITEM					lvi;
	LPNMITEMACTIVATE		lpnmitem;

	// NM_表示对通用控件都通用,范围丛(0, 99)
	// TVN_表示只能TreeView通用,范围丛(400, 499)
	if ((lpNMHdr->code == NM_DBLCLK) && lpNMHdr->hwndFrom == GetDlgItem(hdlgP, IDC_LV_WGEN_EDITOR)) {
		lpnmitem = (LPNMITEMACTIVATE)lpNMHdr;
		// 如果单击到的是复选框位置,把复选框状态置反
		// 当前定义的图标大小是16x16. ptAction反回的(x,y)是整个列表视图内的坐标,因而y值不大好判断
		// 认为如果x是小于16的就认为是击中复选框
		
		lvi.iItem = lpnmitem->iItem;
		lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
		lvi.iSubItem = 0;
		lvi.pszText = gdmgr._menu_text;
		lvi.cchTextMax = _MAX_PATH;
		ListView_GetItem(lpNMHdr->hwndFrom, &lvi);

		gdmgr._menu_lparam = lpnmitem->iItem;
		OnHeroEditBt(hdlgP);	
	}
    return;
}
#endif

BOOL CALLBACK DlgWGenProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#ifndef _ROSE_EDITOR
	LPTOOLTIPTEXT		lpTt;

#define lpnm   ((LPNMHDR)lParam)
#define lpnmTB ((LPNMTOOLBAR)lParam)

   RECT      rc;
   TPMPARAMS tpm;
   HMENU     hPopupMenu;
   BOOL      bRet = FALSE;

	switch(uMsg)
	{
	case WM_INITDIALOG:
		return On_DlgWGenInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgWGenCommand);
	HANDLE_MSG(hdlgP, WM_DESTROY, On_DlgWGenDestroy);
	HANDLE_MSG(hdlgP, WM_DRAWITEM, On_DlgWGenDrawItem);
	case WM_NOTIFY:
		switch(((NMHDR *)lParam)->code) {
		case NM_RCLICK:
			wgen_notify_handler_rclick(hdlgP, 0, (LPNMHDR)lParam);
			break;
		case NM_DBLCLK:
			wgen_notify_handler_dblclk(hdlgP, 0, (LPNMHDR)lParam);
			break;
		case TTN_NEEDTEXT:
			lpTt = (LPTOOLTIPTEXT)lParam;
			if (lpTt->hdr.idFrom == IDM_RESET) {
				strcpy(lpTt->szText, "Reset selected profile item to default");
			} else if (lpTt->hdr.idFrom == IDM_REFRESH) {
				strcpy(lpTt->szText, "Reload last saved configuration");
			} else {
				LoadString(gdmgr._hinst, (UINT)lpTt->hdr.idFrom, lpTt->szText, NUMELMS(lpTt->szText));
			}
			break;
		case PSN_SETACTIVE: // page gaining focus
			break;
		case PSN_KILLACTIVE: // page losing focus
			break;
		case PSN_APPLY: // Apply or OK code here
			// OnApplyBt(hdlgP);
			break;
		case PSN_RESET: // Cancel code here
			break;
		case TBN_DROPDOWN:
			SendMessage(lpnmTB->hdr.hwndFrom, TB_GETRECT, (WPARAM)lpnmTB->iItem, (LPARAM)&rc);

			MapWindowPoints(lpnmTB->hdr.hwndFrom, HWND_DESKTOP, (LPPOINT)&rc, 2);                         

			tpm.cbSize = sizeof(TPMPARAMS);
			tpm.rcExclude = rc;

            hPopupMenu = CreatePopupMenu();
			if (lpnmTB->iItem == IDM_BACKUP) {
				AppendMenu(hPopupMenu, MF_STRING, IDM_IMPORT, "Import...");
				AppendMenu(hPopupMenu, MF_STRING, IDM_EXPORT, "Export...");
			}

			TrackPopupMenuEx(hPopupMenu,
				TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_VERTICAL,               
				rc.left, rc.bottom, hdlgP, &tpm); 

			DestroyMenu(hPopupMenu);			
			return (FALSE);
		}
	}
#endif	
	return FALSE;
}

#ifndef _ROSE_EDITOR  
// ------------------------------------------------------------------------
HWND init_toolbar(HINSTANCE hinst, HWND hdlgP)
{
	// Create a toolbar
	gdmgr._htb_wgen = CreateWindowEx(0, TOOLBARCLASSNAME, (LPSTR)NULL, 
		WS_CHILD | /*CCS_ADJUSTABLE |*/ TBSTYLE_TOOLTIPS | TBSTYLE_FLAT, 0, 0, 0, 0, hdlgP, 
		(HMENU)IDR_WGENMENU, gdmgr._hinst, NULL);

	//Enable multiple image lists
    SendMessage(gdmgr._htb_wgen, CCM_SETVERSION, (WPARAM) 5, 0); 

	// Send the TB_BUTTONSTRUCTSIZE message, which is required for backward compatibility
	ToolBar_ButtonStructSize(gdmgr._htb_wgen, sizeof(TBBUTTON));
	
	gdmgr._tbBtns_wgen[0].iBitmap = MAKELONG(gdmgr._iico_save, 0);
	gdmgr._tbBtns_wgen[0].idCommand = IDM_SAVE;	
	gdmgr._tbBtns_wgen[0].fsState = TBSTATE_ENABLED;
	gdmgr._tbBtns_wgen[0].fsStyle = BTNS_BUTTON;
	gdmgr._tbBtns_wgen[0].dwData = 0L;
	gdmgr._tbBtns_wgen[0].iString = -1;

	gdmgr._tbBtns_wgen[1].iBitmap = -1;
	gdmgr._tbBtns_wgen[1].idCommand = 0;	
	gdmgr._tbBtns_wgen[1].fsState = 0;
	gdmgr._tbBtns_wgen[1].fsStyle = TBSTYLE_SEP;
	gdmgr._tbBtns_wgen[1].dwData = 0L;
	gdmgr._tbBtns_wgen[1].iString = 0;

	gdmgr._tbBtns_wgen[2].iBitmap = MAKELONG(gdmgr._iico_new, 0);
	gdmgr._tbBtns_wgen[2].idCommand = IDM_NEW;	
	gdmgr._tbBtns_wgen[2].fsState = TBSTATE_ENABLED;
	gdmgr._tbBtns_wgen[2].fsStyle = BTNS_BUTTON;
	gdmgr._tbBtns_wgen[2].dwData = 0L;
	gdmgr._tbBtns_wgen[2].iString = -1;

	gdmgr._tbBtns_wgen[3].iBitmap = -1;
	gdmgr._tbBtns_wgen[3].idCommand = 0;	
	gdmgr._tbBtns_wgen[3].fsState = 0;
	gdmgr._tbBtns_wgen[3].fsStyle = TBSTYLE_SEP;
	gdmgr._tbBtns_wgen[3].dwData = 0L;
	gdmgr._tbBtns_wgen[3].iString = 0;

	gdmgr._tbBtns_wgen[4].iBitmap = MAKELONG(gdmgr._iico_reset, 0);
	gdmgr._tbBtns_wgen[4].idCommand = IDM_RESET;
	gdmgr._tbBtns_wgen[4].fsState = TBSTATE_ENABLED;
	gdmgr._tbBtns_wgen[4].fsStyle = BTNS_BUTTON; // BTNS_DROPDOWN;
	gdmgr._tbBtns_wgen[4].dwData = 0L;
	gdmgr._tbBtns_wgen[4].iString = 0;

	gdmgr._tbBtns_wgen[5].iBitmap = -1;
	gdmgr._tbBtns_wgen[5].idCommand = 0;	
	gdmgr._tbBtns_wgen[5].fsState = 0;
	gdmgr._tbBtns_wgen[5].fsStyle = TBSTYLE_SEP;
	gdmgr._tbBtns_wgen[5].dwData = 0L;
	gdmgr._tbBtns_wgen[5].iString = 0;

	gdmgr._tbBtns_wgen[6].iBitmap = 155;
	gdmgr._tbBtns_wgen[6].idCommand = 0;	
	gdmgr._tbBtns_wgen[6].fsState = TBSTATE_ENABLED;
	gdmgr._tbBtns_wgen[6].fsStyle = BTNS_SEP;
	gdmgr._tbBtns_wgen[6].dwData = 0L;
	gdmgr._tbBtns_wgen[6].iString = -1;

	gdmgr._tbBtns_wgen[7].iBitmap = -1;
	gdmgr._tbBtns_wgen[7].idCommand = 0;	
	gdmgr._tbBtns_wgen[7].fsState = 0;
	gdmgr._tbBtns_wgen[7].fsStyle = TBSTYLE_SEP;
	gdmgr._tbBtns_wgen[7].dwData = 0L;
	gdmgr._tbBtns_wgen[7].iString = 0;

	gdmgr._tbBtns_wgen[8].iBitmap = MAKELONG(gdmgr._iico_rename, 0);
	gdmgr._tbBtns_wgen[8].idCommand = IDM_FIND;	
	gdmgr._tbBtns_wgen[8].fsState = TBSTATE_ENABLED;
	gdmgr._tbBtns_wgen[8].fsStyle = BTNS_BUTTON;
	gdmgr._tbBtns_wgen[8].dwData = 0L;
	gdmgr._tbBtns_wgen[8].iString = -1;

	gdmgr._tbBtns_wgen[9].iBitmap = -1;
	gdmgr._tbBtns_wgen[9].idCommand = 0;	
	gdmgr._tbBtns_wgen[9].fsState = 0;
	gdmgr._tbBtns_wgen[9].fsStyle = TBSTYLE_SEP;
	gdmgr._tbBtns_wgen[9].dwData = 0L;
	gdmgr._tbBtns_wgen[9].iString = 0;

	gdmgr._tbBtns_wgen[10].iBitmap = MAKELONG(gdmgr._iico_del, 0);
	gdmgr._tbBtns_wgen[10].idCommand = IDM_DELETE;	
	gdmgr._tbBtns_wgen[10].fsState = TBSTATE_ENABLED;
	gdmgr._tbBtns_wgen[10].fsStyle = BTNS_BUTTON;
	gdmgr._tbBtns_wgen[10].dwData = 0L;
	gdmgr._tbBtns_wgen[10].iString = -1;

	gdmgr._tbBtns_wgen[11].iBitmap = -1;
	gdmgr._tbBtns_wgen[11].idCommand = 0;	
	gdmgr._tbBtns_wgen[11].fsState = 0;
	gdmgr._tbBtns_wgen[11].fsStyle = TBSTYLE_SEP;
	gdmgr._tbBtns_wgen[11].dwData = 0L;
	gdmgr._tbBtns_wgen[11].iString = 0;

	gdmgr._tbBtns_wgen[12].iBitmap = MAKELONG(gdmgr._iico_refresh, 0);
	gdmgr._tbBtns_wgen[12].idCommand = IDM_REFRESH;	
	gdmgr._tbBtns_wgen[12].fsState = TBSTATE_ENABLED;
	gdmgr._tbBtns_wgen[12].fsStyle = BTNS_BUTTON;
	gdmgr._tbBtns_wgen[12].dwData = 0L;
	gdmgr._tbBtns_wgen[12].iString = -1;

	gdmgr._tbBtns_wgen[13].iBitmap = 8;
	gdmgr._tbBtns_wgen[13].idCommand = 0;	
	gdmgr._tbBtns_wgen[13].fsState = 0;
	gdmgr._tbBtns_wgen[13].fsStyle = TBSTYLE_SEP;
	gdmgr._tbBtns_wgen[13].dwData = 0L;
	gdmgr._tbBtns_wgen[13].iString = 0;

	gdmgr._tbBtns_wgen[14].iBitmap = -1;
	gdmgr._tbBtns_wgen[14].idCommand = 0;	
	gdmgr._tbBtns_wgen[14].fsState = 0;
	gdmgr._tbBtns_wgen[14].fsStyle = TBSTYLE_SEP;
	gdmgr._tbBtns_wgen[14].dwData = 0L;
	gdmgr._tbBtns_wgen[14].iString = 0;

	gdmgr._tbBtns_wgen[15].iBitmap = MAKELONG(gdmgr._iico_backup, 0);
	gdmgr._tbBtns_wgen[15].idCommand = IDM_BACKUP;	
	gdmgr._tbBtns_wgen[15].fsState = TBSTATE_ENABLED;
	gdmgr._tbBtns_wgen[15].fsStyle = BTNS_DROPDOWN;
	gdmgr._tbBtns_wgen[15].dwData = 0L;
	gdmgr._tbBtns_wgen[15].iString = -1;

	ToolBar_AddButtons(gdmgr._htb_wgen, 16, &gdmgr._tbBtns_wgen);

	ToolBar_AutoSize(gdmgr._htb_wgen);
	
	ToolBar_SetExtendedStyle(gdmgr._htb_wgen, TBSTYLE_EX_DRAWDDARROWS);
	
	ToolBar_SetImageList(gdmgr._htb_wgen, gdmgr._himl_24x24, 0);
	ToolBar_SetDisabledImageList(gdmgr._htb_wgen, gdmgr._himl_24x24_dis);
	
	ShowWindow(gdmgr._htb_wgen, SW_SHOW);

	return gdmgr._htb_wgen;
}


















void On_DlgPopupDrawItem(HWND hdlgP, const DRAWITEMSTRUCT *lpdis)
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

std::vector<int> to_vector_int2(const std::string& value)
{
	std::vector<std::string> vstr = utils::split(value);
	std::vector<int> ret;
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		const std::string& str = *it;
		ret.push_back(lexical_cast_default<int>(str.substr(0, str.find("("))));
	}
	return ret;
}

void strcat_heros_str2(HWND hdlgP, int id, int append, int max, bool display_name)
{
	std::stringstream strstr;
	char text[_MAX_PATH];

	Edit_GetText(GetDlgItem(hdlgP, id), text, sizeof(text) / sizeof(text[0]));
	
	std::vector<int> vstr = to_vector_int2(text);
	if (std::find(vstr.begin(), vstr.end(), append) != vstr.end()) {
		return;
	}
	if (max > 0 && (int)vstr.size() >= max) {
		return;
	}
	vstr.push_back(append);
	for (std::vector<int>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		int number = *it;
		if (it != vstr.begin()) {
			strstr << ", ";
		}
		strstr << number;
		if (display_name) {
			strstr << "(" << utf8_2_ansi(gdmgr.heros_[number].name().c_str()) << ")";
		}
	}
	Edit_SetText(GetDlgItem(hdlgP, id), strstr.str().c_str());
}

//
// 4 Channel DVR Confinguration Popup Dialog
//

void update_hero_edit(HWND hdlgP, hero& h)
{
	std::stringstream strstr;
	HWND hctl;

	// number
	strstr.str("");
	int value = h.number_;
	strstr << value << " (";
	if (gcfgmgr._ma == ma_new) {
		strstr << utf8_2_ansi(_("Add"));
	} else if (gcfgmgr._ma == ma_edit) {
		strstr << utf8_2_ansi(_("Edit"));
	} else {
		strstr << utf8_2_ansi(_("Unknown action"));
	}
	strstr << ")";
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_HEROEDIT_NUMBER), strstr.str().c_str());

	// name
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_HEROEDIT_NAME), utf8_2_ansi(h.name().c_str()));

	// image number
    UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_IMAGE), h.image_);

	// cost
    UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_COST), h.cost_);

	// leadership
    UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_LEADERSHIP), fxptoi9(h.leadership_));
	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_LEADERSHIPXP), fxpmod9(h.leadership_));

	// force
	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_FORCE), fxptoi9(h.force_));
	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_FORCEXP), fxpmod9(h.force_));

	// intellect
	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_INTELLECT), fxptoi9(h.intellect_));
	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_INTELLECTXP), fxpmod9(h.intellect_));

	// spirit
	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_POLITICS), fxptoi9(h.spirit_));
	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_POLITICSXP), fxpmod9(h.spirit_));

	// charm
	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_CHARM), fxptoi9(h.charm_));
	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_CHARMXP), fxpmod9(h.charm_));

	// catalog
	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_CATALOG), h.base_catalog_);

	// heart
	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_HEART), h.heart_);

	// biography
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_HEROEDIT_BIOGRAPHY), utf8_2_ansi(h.biography()));

	// gender
	ComboBox_SetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_GENDER), h.gender_);

	// flags
	Button_SetCheck(GetDlgItem(hdlgP, IDC_CHK_HEROEDIT_ROBBER), h.get_flag(hero_flag_robber)); 
	Button_SetCheck(GetDlgItem(hdlgP, IDC_CHK_HEROEDIT_ROAM), h.get_flag(hero_flag_roam));
	Button_SetCheck(GetDlgItem(hdlgP, IDC_CHK_HEROEDIT_EMPLOYEE), h.get_flag(hero_flag_employee)); 
	Button_SetCheck(GetDlgItem(hdlgP, IDC_CHK_HEROEDIT_NPC), h.get_flag(hero_flag_npc)); 

	// treasure
	hctl = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_TREASURE0);
	for (int idx = 0; idx < ComboBox_GetCount(hctl); idx ++) {
		if (ComboBox_GetItemData(hctl, idx) == h.treasure_) {
			ComboBox_SetCurSel(hctl, idx);
			break;
		}
	}

	// unit type
	hctl = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_UTYPE);
	for (int idx = 0; idx < ComboBox_GetCount(hctl); idx ++) {
		if (ComboBox_GetItemData(hctl, idx) == h.utype_) {
			ComboBox_SetCurSel(hctl, idx);
			break;
		}
	}
	
	// feature
	hctl = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_FEATURE);
	for (int idx = 0; idx < ComboBox_GetCount(hctl); idx ++) {
		if (ComboBox_GetItemData(hctl, idx) == h.feature_) {
			ComboBox_SetCurSel(hctl, idx);
			break;
		}
	}

	// side feature
	hctl = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_SIDEFEATURE);
	for (int idx = 0; idx < ComboBox_GetCount(hctl); idx ++) {
		if (ComboBox_GetItemData(hctl, idx) == h.side_feature_) {
			ComboBox_SetCurSel(hctl, idx);
			break;
		}
	}

	// tactic
	hctl = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_TACTIC);
	for (int idx = 0; idx < ComboBox_GetCount(hctl); idx ++) {
		if (ComboBox_GetItemData(hctl, idx) == h.tactic_) {
			ComboBox_SetCurSel(hctl, idx);
			break;
		}
	}

	// character
	hctl = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_CHARACTER);
	for (int idx = 0; idx < ComboBox_GetCount(hctl); idx ++) {
		if (ComboBox_GetItemData(hctl, idx) == h.character_) {
			ComboBox_SetCurSel(hctl, idx);
			break;
		}
	}

	// adaptability-arms0
	ComboBox_SetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_ARMS0), fxptoi12(h.arms_[0]));
	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_ARMS0XP), fxpmod12(h.arms_[0]));

	// adaptability-arms1
	ComboBox_SetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_ARMS1), fxptoi12(h.arms_[1]));
	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_ARMS1XP), fxpmod12(h.arms_[1]));

	// adaptability-arms2
	ComboBox_SetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_ARMS2), fxptoi12(h.arms_[2]));
	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_ARMS2XP), fxpmod12(h.arms_[2]));

	// adaptability-arms3
	ComboBox_SetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_ARMS3), fxptoi12(h.arms_[3]));
	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_ARMS3XP), fxpmod12(h.arms_[3]));

	// adaptability-arms4
	ComboBox_SetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_ARMS4), fxptoi12(h.arms_[4]));
	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_ARMS4XP), fxpmod12(h.arms_[4]));

	// adaptability-skill_commercial
	ComboBox_SetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_SKILL0), fxptoi12(h.skill_[hero_skill_commercial]));
	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_SKILL0XP), fxpmod12(h.skill_[hero_skill_commercial]));

	// adaptability-skill_invent
	ComboBox_SetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_SKILL1), fxptoi12(h.skill_[hero_skill_invent]));
	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_SKILL1XP), fxpmod12(h.skill_[hero_skill_invent]));

	// adaptability-skill_encourage
	ComboBox_SetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_SKILL3), fxptoi12(h.skill_[hero_skill_encourage]));
	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_SKILL3XP), fxpmod12(h.skill_[hero_skill_encourage]));

	// adaptability-skill_hero
	ComboBox_SetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_SKILL4), fxptoi12(h.skill_[hero_skill_hero]));
	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_SKILL4XP), fxpmod12(h.skill_[hero_skill_hero]));

	// adaptability-skill_demolish
	ComboBox_SetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_SKILL5), fxptoi12(h.skill_[hero_skill_demolish]));
	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_SKILL5XP), fxpmod12(h.skill_[hero_skill_demolish]));

	// adaptability-skill_formation
	ComboBox_SetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_SKILL6), fxptoi12(h.skill_[hero_skill_formation]));
	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_SKILL6XP), fxpmod12(h.skill_[hero_skill_formation]));
}


BOOL save_hero_edit(HWND hdlgP, BOOL *changed)
{
	uint32_t u32n, u32n1, u32n2;
	int idx;
	char text[_MAX_PATH];

	// 列传可能大于_MAX_PATH
	// char		text[1024];

	// 1.检查参数是否合法
	*changed = FALSE;

	// image number
	u32n = (uint32_t)UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_IMAGE));
	if (gdmgr.heros_[gdmgr._menu_lparam].image_ != u32n) {
		*changed = TRUE;
		gdmgr.heros_[gdmgr._menu_lparam].image_ = u32n;
	}

	// cost
	u32n = (uint32_t)UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_COST));
	if (gdmgr.heros_[gdmgr._menu_lparam].cost_ != u32n) {
		*changed = TRUE;
		gdmgr.heros_[gdmgr._menu_lparam].cost_ = u32n;
	}

	// leadership
	u32n1 = (uint32_t)UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_LEADERSHIP));
	u32n2 = (uint32_t)UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_LEADERSHIPXP));
	u32n = ftofxp9(u32n1) + u32n2;
	if (gdmgr.heros_[gdmgr._menu_lparam].leadership_ != u32n) {
		*changed = TRUE;
		gdmgr.heros_[gdmgr._menu_lparam].leadership_ = u32n;
	}
	// force
	u32n1 = (uint32_t)UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_FORCE));
	u32n2 = (uint32_t)UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_FORCEXP));
	u32n = ftofxp9(u32n1) + u32n2;
	if (gdmgr.heros_[gdmgr._menu_lparam].force_ != u32n) {
		*changed = TRUE;
		gdmgr.heros_[gdmgr._menu_lparam].force_ = u32n;
	}
	// intellect
	u32n1 = (uint32_t)UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_INTELLECT));
	u32n2 = (uint32_t)UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_INTELLECTXP));
	u32n = ftofxp9(u32n1) + u32n2;
	if (gdmgr.heros_[gdmgr._menu_lparam].intellect_ != u32n) {
		*changed = TRUE;
		gdmgr.heros_[gdmgr._menu_lparam].intellect_ = u32n;
	}
	// spirit
	u32n1 = (uint32_t)UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_POLITICS));
	u32n2 = (uint32_t)UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_POLITICSXP));
	u32n = ftofxp9(u32n1) + u32n2;
	if (gdmgr.heros_[gdmgr._menu_lparam].spirit_ != u32n) {
		*changed = TRUE;
		gdmgr.heros_[gdmgr._menu_lparam].spirit_ = u32n;
	}
	// charm
	u32n1 = (uint32_t)UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_CHARM));
	u32n2 = (uint32_t)UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_CHARMXP));
	u32n = ftofxp9(u32n1) + u32n2;
	if (gdmgr.heros_[gdmgr._menu_lparam].charm_ != u32n) {
		*changed = TRUE;
		gdmgr.heros_[gdmgr._menu_lparam].charm_ = u32n;
	}

	// catalog
	u32n = (uint32_t)UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_CATALOG));
	if (gdmgr.heros_[gdmgr._menu_lparam].base_catalog_ != u32n) {
		*changed = TRUE;
		gdmgr.heros_[gdmgr._menu_lparam].base_catalog_ = u32n;
		gdmgr.heros_[gdmgr._menu_lparam].float_catalog_ = ftofxp8(u32n);
	}

	// heart
	u32n = (uint32_t)UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_HEART));
	if (gdmgr.heros_[gdmgr._menu_lparam].heart_ != u32n) {
		*changed = TRUE;
		gdmgr.heros_[gdmgr._menu_lparam].heart_ = u32n;
	}

	// gender
	u32n = ComboBox_GetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_GENDER));
	if (gdmgr.heros_[gdmgr._menu_lparam].gender_ != u32n) {
		*changed = TRUE;
		gdmgr.heros_[gdmgr._menu_lparam].gender_ = u32n;
	}

	// ambition
	u32n = 0;
	if (Button_GetCheck(GetDlgItem(hdlgP, IDC_CHK_HEROEDIT_ROBBER))) {
		u32n |= 1 << hero_flag_robber;
	}
	if (Button_GetCheck(GetDlgItem(hdlgP, IDC_CHK_HEROEDIT_ROAM))) {
		u32n |= 1 << hero_flag_roam;
	}
	if (Button_GetCheck(GetDlgItem(hdlgP, IDC_CHK_HEROEDIT_EMPLOYEE))) {
		u32n |= 1 << hero_flag_employee;
	}
	if (Button_GetCheck(GetDlgItem(hdlgP, IDC_CHK_HEROEDIT_NPC))) {
		u32n |= 1 << hero_flag_npc;
	}
	if (gdmgr.heros_[gdmgr._menu_lparam].flags_ != u32n) {
		*changed = TRUE;
		gdmgr.heros_[gdmgr._menu_lparam].flags_ = u32n;
	}

	// treasure
	u32n = ComboBox_GetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_TREASURE0));
	idx = ComboBox_GetItemData(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_TREASURE0), u32n);
	if (gdmgr.heros_[gdmgr._menu_lparam].treasure_ != idx) {
		*changed = TRUE;
		gdmgr.heros_[gdmgr._menu_lparam].treasure_ = idx;
	}

	// unit type
	u32n = ComboBox_GetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_UTYPE));
	idx = ComboBox_GetItemData(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_UTYPE), u32n);
	if (gdmgr.heros_[gdmgr._menu_lparam].utype_ != idx) {
		*changed = TRUE;
		gdmgr.heros_[gdmgr._menu_lparam].utype_ = idx;
	}

	// feature
	u32n = ComboBox_GetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_FEATURE));
	idx = ComboBox_GetItemData(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_FEATURE), u32n);
	if (gdmgr.heros_[gdmgr._menu_lparam].feature_ != idx) {
		*changed = TRUE;
		gdmgr.heros_[gdmgr._menu_lparam].feature_ = idx;
	}

	// side feature
	u32n = ComboBox_GetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_SIDEFEATURE));
	idx = ComboBox_GetItemData(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_SIDEFEATURE), u32n);
	if (gdmgr.heros_[gdmgr._menu_lparam].side_feature_ != idx) {
		*changed = TRUE;
		gdmgr.heros_[gdmgr._menu_lparam].side_feature_ = idx;
	}

	// tactic
	u32n = ComboBox_GetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_TACTIC));
	idx = ComboBox_GetItemData(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_TACTIC), u32n);
	if (gdmgr.heros_[gdmgr._menu_lparam].tactic_ != idx) {
		*changed = TRUE;
		gdmgr.heros_[gdmgr._menu_lparam].tactic_ = idx;
	}

	// character
	u32n = ComboBox_GetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_CHARACTER));
	idx = ComboBox_GetItemData(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_CHARACTER), u32n);
	if (gdmgr.heros_[gdmgr._menu_lparam].character_ != idx) {
		*changed = TRUE;
		gdmgr.heros_[gdmgr._menu_lparam].character_ = idx;
	}

	// adaptability-arms0
	u32n1 = (uint32_t)ComboBox_GetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_ARMS0));
	u32n2 = (uint32_t)UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_ARMS0XP));
	u32n = ftofxp12(u32n1) + u32n2;
	if (gdmgr.heros_[gdmgr._menu_lparam].arms_[0] != u32n) {
		*changed = TRUE;
		gdmgr.heros_[gdmgr._menu_lparam].arms_[0] = u32n;
	}

	// adaptability-arms1
	u32n1 = (uint32_t)ComboBox_GetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_ARMS1));
	u32n2 = (uint32_t)UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_ARMS1XP));
	u32n = ftofxp12(u32n1) + u32n2;
	if (gdmgr.heros_[gdmgr._menu_lparam].arms_[1] != u32n) {
		*changed = TRUE;
		gdmgr.heros_[gdmgr._menu_lparam].arms_[1] = u32n;
	}

	// adaptability-arms2
	u32n1 = (uint32_t)ComboBox_GetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_ARMS2));
	u32n2 = (uint32_t)UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_ARMS2XP));
	u32n = ftofxp12(u32n1) + u32n2;
	if (gdmgr.heros_[gdmgr._menu_lparam].arms_[2] != u32n) {
		*changed = TRUE;
		gdmgr.heros_[gdmgr._menu_lparam].arms_[2] = u32n;
	}

	// adaptability-arms3
	u32n1 = (uint32_t)ComboBox_GetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_ARMS3));
	u32n2 = (uint32_t)UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_ARMS3XP));
	u32n = ftofxp12(u32n1) + u32n2;
	if (gdmgr.heros_[gdmgr._menu_lparam].arms_[3] != u32n) {
		*changed = TRUE;
		gdmgr.heros_[gdmgr._menu_lparam].arms_[3] = u32n;
	}

	// adaptability-arms4
	u32n1 = (uint32_t)ComboBox_GetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_ARMS4));
	u32n2 = (uint32_t)UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_ARMS4XP));
	u32n = ftofxp12(u32n1) + u32n2;
	if (gdmgr.heros_[gdmgr._menu_lparam].arms_[4] != u32n) {
		*changed = TRUE;
		gdmgr.heros_[gdmgr._menu_lparam].arms_[4] = u32n;
	}

	// adaptability-skill_commercial
	u32n1 = (uint32_t)ComboBox_GetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_SKILL0));
	u32n2 = (uint32_t)UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_SKILL0XP));
	u32n = ftofxp12(u32n1) + u32n2;
	if (gdmgr.heros_[gdmgr._menu_lparam].skill_[hero_skill_commercial] != u32n) {
		*changed = TRUE;
		gdmgr.heros_[gdmgr._menu_lparam].skill_[hero_skill_commercial] = u32n;
	}

	// adaptability-skill_invent
	u32n1 = (uint32_t)ComboBox_GetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_SKILL1));
	u32n2 = (uint32_t)UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_SKILL1XP));
	u32n = ftofxp12(u32n1) + u32n2;
	if (gdmgr.heros_[gdmgr._menu_lparam].skill_[hero_skill_invent] != u32n) {
		*changed = TRUE;
		gdmgr.heros_[gdmgr._menu_lparam].skill_[hero_skill_invent] = u32n;
	}

	// adaptability-skill_encourage
	u32n1 = (uint32_t)ComboBox_GetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_SKILL3));
	u32n2 = (uint32_t)UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_SKILL3XP));
	u32n = ftofxp12(u32n1) + u32n2;
	if (gdmgr.heros_[gdmgr._menu_lparam].skill_[hero_skill_encourage] != u32n) {
		*changed = TRUE;
		gdmgr.heros_[gdmgr._menu_lparam].skill_[hero_skill_encourage] = u32n;
	}

	// adaptability-skill_hero
	u32n1 = (uint32_t)ComboBox_GetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_SKILL4));
	u32n2 = (uint32_t)UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_SKILL4XP));
	u32n = ftofxp12(u32n1) + u32n2;
	if (gdmgr.heros_[gdmgr._menu_lparam].skill_[hero_skill_hero] != u32n) {
		*changed = TRUE;
		gdmgr.heros_[gdmgr._menu_lparam].skill_[hero_skill_hero] = u32n;
	}

	// adaptability-skill_demolish
	u32n1 = (uint32_t)ComboBox_GetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_SKILL5));
	u32n2 = (uint32_t)UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_SKILL5XP));
	u32n = ftofxp12(u32n1) + u32n2;
	if (gdmgr.heros_[gdmgr._menu_lparam].skill_[hero_skill_demolish] != u32n) {
		*changed = TRUE;
		gdmgr.heros_[gdmgr._menu_lparam].skill_[hero_skill_demolish] = u32n;
	}

	// adaptability-skill_formation
	u32n1 = (uint32_t)ComboBox_GetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_SKILL6));
	u32n2 = (uint32_t)UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_SKILL6XP));
	u32n = ftofxp12(u32n1) + u32n2;
	if (gdmgr.heros_[gdmgr._menu_lparam].skill_[hero_skill_formation] != u32n) {
		*changed = TRUE;
		gdmgr.heros_[gdmgr._menu_lparam].skill_[hero_skill_formation] = u32n;
	}

	// parent[0]
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_HEROEDIT_FATHER), text, sizeof(text));
	std::vector<int> vrelation = to_vector_int2(text);
	u32n = vrelation.empty()? HEROS_INVALID_NUMBER: vrelation.front();
	if (gdmgr.heros_[gdmgr._menu_lparam].parent_[0].hero_ != u32n) {
		*changed = TRUE;
		gdmgr.heros_[gdmgr._menu_lparam].parent_[0].hero_ = u32n;
	}

	// parent[1]
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_HEROEDIT_MOTHER), text, sizeof(text));
	vrelation = to_vector_int2(text);
	u32n = vrelation.empty()? HEROS_INVALID_NUMBER: vrelation.front();
	if (gdmgr.heros_[gdmgr._menu_lparam].parent_[1].hero_ != u32n) {
		*changed = TRUE;
		gdmgr.heros_[gdmgr._menu_lparam].parent_[1].hero_ = u32n;
	}

	// consort
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_HEROEDIT_CONSORT), text, sizeof(text));
	vrelation = to_vector_int2(text);
	for (int i = 0; i < HEROS_MAX_CONSORT; i ++) {
		u32n = (int)vrelation.size() > i? vrelation[i]: HEROS_INVALID_NUMBER;
		if (gdmgr.heros_[gdmgr._menu_lparam].consort_[i].hero_ != u32n) {
			*changed = TRUE;
			gdmgr.heros_[gdmgr._menu_lparam].consort_[i].hero_ = u32n;
		}
	}

	// oath
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_HEROEDIT_OATH), text, sizeof(text));
	vrelation = to_vector_int2(text);
	for (int i = 0; i < HEROS_MAX_OATH; i ++) {
		u32n = (int)vrelation.size() > i? vrelation[i]: HEROS_INVALID_NUMBER;
		if (gdmgr.heros_[gdmgr._menu_lparam].oath_[i].hero_ != u32n) {
			*changed = TRUE;
			gdmgr.heros_[gdmgr._menu_lparam].oath_[i].hero_ = u32n;
		}
	}

	// intimate
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_HEROEDIT_INTIMATE), text, sizeof(text));
	vrelation = to_vector_int2(text);
	for (int i = 0; i < HEROS_MAX_INTIMATE; i ++) {
		u32n = (int)vrelation.size() > i? vrelation[i]: HEROS_INVALID_NUMBER;
		if (gdmgr.heros_[gdmgr._menu_lparam].intimate_[i].hero_ != u32n) {
			*changed = TRUE;
			gdmgr.heros_[gdmgr._menu_lparam].intimate_[i].hero_ = u32n;
		}
	}

	// hate
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_HEROEDIT_HATE), text, sizeof(text));
	vrelation = to_vector_int2(text);
	for (int i = 0; i < HEROS_MAX_HATE; i ++) {
		u32n = (int)vrelation.size() > i? vrelation[i]: HEROS_INVALID_NUMBER;
		if (gdmgr.heros_[gdmgr._menu_lparam].hate_[i].hero_ != u32n) {
			*changed = TRUE;
			gdmgr.heros_[gdmgr._menu_lparam].hate_[i].hero_ = u32n;
		}
	}

	return TRUE;
}

void refresh_hero_name_list(HWND hdlgP, hero& h)
{
	// parent0
	if (h.parent_[0].hero_ != HEROS_INVALID_NUMBER) {
		strcat_heros_str2(hdlgP, IDC_ET_HEROEDIT_FATHER, h.parent_[0].hero_, 1, true);
	}

	// parent1
	if (h.parent_[1].hero_ != HEROS_INVALID_NUMBER) {
		strcat_heros_str2(hdlgP, IDC_ET_HEROEDIT_MOTHER, h.parent_[1].hero_, 1, true);
	}

	// consort
	for (int i = 0; i < HEROS_MAX_CONSORT; i ++) {
		if (h.consort_[i].hero_ != HEROS_INVALID_NUMBER) {
			strcat_heros_str2(hdlgP, IDC_ET_HEROEDIT_CONSORT, h.consort_[i].hero_, HEROS_MAX_CONSORT, true);
		}
	}

	// oath
	for (int i = 0; i < HEROS_MAX_OATH; i ++) {
		if (h.oath_[i].hero_ != HEROS_INVALID_NUMBER) {
			strcat_heros_str2(hdlgP, IDC_ET_HEROEDIT_OATH, h.oath_[i].hero_, HEROS_MAX_OATH, true);
		}
	}

	// intimate
	for (int i = 0; i < HEROS_MAX_INTIMATE; i ++) {
		if (h.intimate_[i].hero_ != HEROS_INVALID_NUMBER) {
			strcat_heros_str2(hdlgP, IDC_ET_HEROEDIT_INTIMATE, h.intimate_[i].hero_, HEROS_MAX_INTIMATE, true);
		}
	}

	// hate
	for (int i = 0; i < HEROS_MAX_HATE; i ++) {
		if (h.hate_[i].hero_ != HEROS_INVALID_NUMBER) {
			strcat_heros_str2(hdlgP, IDC_ET_HEROEDIT_HATE, h.hate_[i].hero_, HEROS_MAX_HATE, true);
		}
	}
}

void set_language_text(HWND hdlgP, int id, const char* domain, const char* msgid)
{
	Static_SetText(GetDlgItem(hdlgP, id), utf8_2_ansi(dgettext(domain, msgid)));
}

// LSBIT_SGLFILECAP ==> LSBIT_SGLFILECAP
// 定义了LSBIT_SGLFILECAP，则采用单文件循环录像
// 定义了LSBIT_CIRCULARCAP，而没定义LSBIT_SGLFILECAP，则采用单文件循环录像
// 同时定义LSBIT_CIRCULARCAP和LSBIT_SGLFILECAP，优先采用单文件循环录像
// 既没定义LSBIT_CIRCULARCAP，也没定义LSBIT_SGLFILECAP，普通方式录像
BOOL On_DlgHeroEditInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	HWND hctl;
	char text[_MAX_PATH];
	std::stringstream strstr;

	editor_config::move_subcfg_right_position(hdlgP, lParam);
	// editor_config::create_subcfg_toolbar(hdlgP);

	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_NUMBER), utf8_2_ansi(_("NO.")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_PICTURENUMBER), utf8_2_ansi(_("Picture NO.")));
	set_language_text(hdlgP, IDC_STATIC_COST, "wesnoth-lib", "Cost");
	set_language_text(hdlgP, IDC_STATIC_NAME, "wesnoth-hero", "name");
	set_language_text(hdlgP, IDC_STATIC_GENDER, "wesnoth-hero", "gender");
	set_language_text(hdlgP, IDC_STATIC_CATALOG, "wesnoth-hero", "catalog");
	set_language_text(hdlgP, IDC_STATIC_HEART, "wesnoth-hero", "heart");
	set_language_text(hdlgP, IDC_STATIC_LEADERSHIP, "wesnoth-hero", "leadership");
	set_language_text(hdlgP, IDC_STATIC_FORCE, "wesnoth-hero", "force");
	set_language_text(hdlgP, IDC_STATIC_INTELLECT, "wesnoth-hero", "intellect");
	set_language_text(hdlgP, IDC_STATIC_POLITICS, "wesnoth-hero", "spirit");
	set_language_text(hdlgP, IDC_STATIC_CHARM, "wesnoth-hero", "charm");
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_EXPERIENCE), utf8_2_ansi(_("Experience")));
	set_language_text(hdlgP, IDC_STATIC_ARMS, "wesnoth-lib", "Arms");
	set_language_text(hdlgP, IDC_STATIC_ARMS0, "wesnoth-hero", "arms-0");
	set_language_text(hdlgP, IDC_STATIC_ARMS1, "wesnoth-hero", "arms-1");
	set_language_text(hdlgP, IDC_STATIC_ARMS2, "wesnoth-hero", "arms-2");
	set_language_text(hdlgP, IDC_STATIC_ARMS3, "wesnoth-hero", "arms-3");
	set_language_text(hdlgP, IDC_STATIC_ARMS4, "wesnoth-hero", "arms-4");
	set_language_text(hdlgP, IDC_STATIC_SKILL, "wesnoth-lib", "Skill");
	set_language_text(hdlgP, IDC_STATIC_SKILL0, "wesnoth-hero", "skill-0");
	set_language_text(hdlgP, IDC_STATIC_SKILL1, "wesnoth-hero", "skill-1");
	set_language_text(hdlgP, IDC_STATIC_SKILL3, "wesnoth-hero", "skill-3");
	set_language_text(hdlgP, IDC_STATIC_SKILL4, "wesnoth-hero", "skill-4");
	set_language_text(hdlgP, IDC_STATIC_SKILL5, "wesnoth-hero", "skill-5");
	set_language_text(hdlgP, IDC_STATIC_SKILL6, "wesnoth-hero", "skill-6");
	set_language_text(hdlgP, IDC_STATIC_RELATION, "wesnoth-lib", "Relation");
	set_language_text(hdlgP, IDC_STATIC_PARENT, "wesnoth-hero", "parent");
	strstr.str("");
	strstr << dgettext("wesnoth-hero", "consort") << "(" << HEROS_MAX_CONSORT;
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_CONSORT), utf8_2_ansi(strstr.str().c_str()));
	strstr.str("");
	strstr << dgettext("wesnoth-hero", "oath") << "(" << HEROS_MAX_OATH;
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_OATH), utf8_2_ansi(strstr.str().c_str()));
	strstr.str("");
	strstr << dgettext("wesnoth-hero", "intimate") << "(" << HEROS_MAX_INTIMATE;
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_INTIMATE), utf8_2_ansi(strstr.str().c_str()));
	strstr.str("");
	strstr << dgettext("wesnoth-hero", "hate") << "(" << HEROS_MAX_HATE;
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_HATE), utf8_2_ansi(strstr.str().c_str()));
	set_language_text(hdlgP, IDC_STATIC_FEATURE, "wesnoth-hero", "feature");
	set_language_text(hdlgP, IDC_STATIC_SIDEFEATURE, "wesnoth-hero", "side feature");
	set_language_text(hdlgP, IDC_STATIC_TACTIC, "wesnoth-hero", "tactic");
	set_language_text(hdlgP, IDC_STATIC_CHARACTER, "wesnoth-lib", "Character");
	set_language_text(hdlgP, IDC_STATIC_TREASURE, "wesnoth-hero", "treasure");
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_UTYPE), utf8_2_ansi(_("arms^Type")));
	set_language_text(hdlgP, IDC_STATIC_BIOGRAPHY, "wesnoth-hero", "biography");

	// gender
	hctl = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_GENDER);
	for (int idx = 0; idx <= hero_gender_max; idx ++) {
		sprintf(text, "%s%i", HERO_PREFIX_STR_GENDER, idx);
		ComboBox_AddString(hctl, utf8_2_ansi(dgettext("wesnoth-hero", text)));
	}

	// flags
	Button_SetText(GetDlgItem(hdlgP, IDC_CHK_HEROEDIT_ROBBER), utf8_2_ansi(hero::flag_str(hero_flag_robber).c_str()));
	Button_SetText(GetDlgItem(hdlgP, IDC_CHK_HEROEDIT_ROAM), utf8_2_ansi(hero::flag_str(hero_flag_roam).c_str()));
	Button_SetText(GetDlgItem(hdlgP, IDC_CHK_HEROEDIT_EMPLOYEE), utf8_2_ansi(hero::flag_str(hero_flag_employee).c_str()));
	Button_SetText(GetDlgItem(hdlgP, IDC_CHK_HEROEDIT_NPC), utf8_2_ansi(hero::flag_str(hero_flag_npc).c_str()));

	// treasure
	hctl = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_TREASURE0);
	ComboBox_AddString(hctl, "");
	ComboBox_SetItemData(hctl, 0, HEROS_NO_TREASURE);
	const std::vector<ttreasure>& treasures = unit_types.treasures();
	for (std::vector<ttreasure>::const_iterator it = treasures.begin(); it != treasures.end(); ++ it) {
		const ttreasure& t = *it;
		ComboBox_AddString(hctl, utf8_2_ansi(t.name().c_str()));
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, t.index());
	}

	// unit type
	hctl = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_UTYPE);
	ComboBox_AddString(hctl, "");
	ComboBox_SetItemData(hctl, 0, HEROS_NO_UTYPE);
	const std::map<int, const unit_type*>& keytypes = unit_types.keytypes();
	for (std::map<int, const unit_type*>::const_iterator it = keytypes.begin(); it != keytypes.end(); ++ it) {
		const unit_type* ut = it->second;
		ComboBox_AddString(hctl, utf8_2_ansi(ut->type_name().c_str()));
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, it->first);
	}

	// feature
	hctl = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_FEATURE);
	ComboBox_AddString(hctl, "");
	ComboBox_SetItemData(hctl, 0, HEROS_NO_FEATURE);

	std::vector<int>& features = hero::valid_features();
	for (std::vector<int>::const_iterator itor = features.begin(); itor != features.end(); ++ itor) {
		sprintf(text, "%s%i", HERO_PREFIX_STR_FEATURE, *itor);
		ComboBox_AddString(hctl, utf8_2_ansi(dgettext("wesnoth-card", text))); 
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, *itor);
	}

	// side feature
	hctl = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_SIDEFEATURE);
	ComboBox_AddString(hctl, "");
	ComboBox_SetItemData(hctl, 0, HEROS_NO_FEATURE);
	for (std::vector<int>::const_iterator itor = features.begin(); itor != features.end(); ++ itor) {
		sprintf(text, "%s%i", HERO_PREFIX_STR_FEATURE, *itor);
		ComboBox_AddString(hctl, utf8_2_ansi(dgettext("wesnoth-card", text))); 
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, *itor);
	}

	// tactic
	hctl = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_TACTIC);
	ComboBox_AddString(hctl, "");
	ComboBox_SetItemData(hctl, 0, HEROS_NO_TACTIC);

	const std::map<int, ttactic>& tactics = unit_types.tactics();
	for (std::map<int, ttactic>::const_iterator it = tactics.begin(); it != tactics.end(); ++ it) {
		ComboBox_AddString(hctl, utf8_2_ansi(it->second.name().c_str())); 
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, it->second.index());
	}

	// character
	hctl = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_CHARACTER);
	ComboBox_AddString(hctl, "");
	ComboBox_SetItemData(hctl, 0, HEROS_NO_CHARACTER);

	const std::map<int, tcharacter>& characters = unit_types.characters();
	for (std::map<int, tcharacter>::const_iterator it = characters.begin(); it != characters.end(); ++ it) {
		ComboBox_AddString(hctl, utf8_2_ansi(it->second.name().c_str())); 
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, it->second.index());
	}

	// candidate hero
	candidate_hero::fill_header(hdlgP);
	hctl = GetDlgItem(hdlgP, IDC_LV_CANDIDATEHERO);
	for (int number = hero::number_normal_min; number < (int)gdmgr.heros_.size(); number ++) {
		if (number != gdmgr._menu_lparam) {
			hero& h = gdmgr.heros_[number];
			candidate_hero::fill_row(hctl, h);
		}
	}
	refresh_hero_name_list(hdlgP, gdmgr.heros_[gdmgr._menu_lparam]);

	// image number
	hctl = GetDlgItem(hdlgP, IDC_UD_HEROEDIT_IMAGE);
	if (gdmgr.heros_.size() > 0) {
		UpDown_SetRange(hctl, 0, gdmgr.heros_.size() - 1);	// [0, count of hero)
	} else {
		UpDown_SetRange(hctl, 0, 0);	// [0, 0]
	}
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_HEROEDIT_IMAGE));

	// cost
	hctl = GetDlgItem(hdlgP, IDC_UD_HEROEDIT_COST);
	UpDown_SetRange(hctl, 0, 255);	// [0, 255]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_HEROEDIT_COST));

	// leadership
	hctl = GetDlgItem(hdlgP, IDC_UD_HEROEDIT_LEADERSHIP);
	UpDown_SetRange(hctl, 1, 127);	// [1, 127]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_HEROEDIT_LEADERSHIP));
	hctl = GetDlgItem(hdlgP, IDC_UD_HEROEDIT_LEADERSHIPXP);
	UpDown_SetRange(hctl, 0, 511);	// [0, 511]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_HEROEDIT_LEADERSHIPXP));

	// force
	hctl = GetDlgItem(hdlgP, IDC_UD_HEROEDIT_FORCE);
	UpDown_SetRange(hctl, 1, 127);	// [1, 127]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_HEROEDIT_FORCE));
	hctl = GetDlgItem(hdlgP, IDC_UD_HEROEDIT_FORCEXP);
	UpDown_SetRange(hctl, 0, 511);	// [0, 511]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_HEROEDIT_FORCEXP));

	// intellect
	hctl = GetDlgItem(hdlgP, IDC_UD_HEROEDIT_INTELLECT);
	UpDown_SetRange(hctl, 1, 127);	// [1, 127]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_HEROEDIT_INTELLECT));
	hctl = GetDlgItem(hdlgP, IDC_UD_HEROEDIT_INTELLECTXP);
	UpDown_SetRange(hctl, 0, 511);	// [0, 511]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_HEROEDIT_INTELLECTXP));

	// spirit
	hctl = GetDlgItem(hdlgP, IDC_UD_HEROEDIT_POLITICS);
	UpDown_SetRange(hctl, 1, 127);	// [1, 127]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_HEROEDIT_POLITICS));
	hctl = GetDlgItem(hdlgP, IDC_UD_HEROEDIT_POLITICSXP);
	UpDown_SetRange(hctl, 0, 511);	// [0, 511]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_HEROEDIT_POLITICSXP));

	// charm
	hctl = GetDlgItem(hdlgP, IDC_UD_HEROEDIT_CHARM);
	UpDown_SetRange(hctl, 1, 127);	// [1, 127]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_HEROEDIT_CHARM));
	hctl = GetDlgItem(hdlgP, IDC_UD_HEROEDIT_CHARMXP);
	UpDown_SetRange(hctl, 0, 511);	// [0, 511]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_HEROEDIT_CHARMXP));

	// adaptability-arms0
	hctl = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_ARMS0);
	for (int idx = 0; idx <= hero_adaptability_max; idx ++) {
		sprintf(text, "%s%i", HERO_PREFIX_STR_ADAPTABILITY, idx);
		ComboBox_AddString(hctl, utf8_2_ansi(dgettext("wesnoth-hero", text))); 
	}
	hctl = GetDlgItem(hdlgP, IDC_UD_HEROEDIT_ARMS0XP);
	UpDown_SetRange(hctl, 0, 4095);	// [0, 4095]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_HEROEDIT_ARMS0XP));

	// adaptability-arms1
	hctl = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_ARMS1);
	for (int idx = 0; idx <= hero_adaptability_max; idx ++) {
		sprintf(text, "%s%i", HERO_PREFIX_STR_ADAPTABILITY, idx);
		ComboBox_AddString(hctl, utf8_2_ansi(dgettext("wesnoth-hero", text))); 
	}
	hctl = GetDlgItem(hdlgP, IDC_UD_HEROEDIT_ARMS1XP);
	UpDown_SetRange(hctl, 0, 4095);	// [0, 4095]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_HEROEDIT_ARMS1XP));

	// adaptability-arms2
	hctl = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_ARMS2);
	for (int idx = 0; idx <= hero_adaptability_max; idx ++) {
		sprintf(text, "%s%i", HERO_PREFIX_STR_ADAPTABILITY, idx);
		ComboBox_AddString(hctl, utf8_2_ansi(dgettext("wesnoth-hero", text))); 
	}
	hctl = GetDlgItem(hdlgP, IDC_UD_HEROEDIT_ARMS2XP);
	UpDown_SetRange(hctl, 0, 4095);	// [0, 4095]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_HEROEDIT_ARMS2XP));

	// adaptability-arms3
	hctl = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_ARMS3);
	for (int idx = 0; idx <= hero_adaptability_max; idx ++) {
		sprintf(text, "%s%i", HERO_PREFIX_STR_ADAPTABILITY, idx);
		ComboBox_AddString(hctl, utf8_2_ansi(dgettext("wesnoth-hero", text))); 
	}
	hctl = GetDlgItem(hdlgP, IDC_UD_HEROEDIT_ARMS3XP);
	UpDown_SetRange(hctl, 0, 4095);	// [0, 4095]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_HEROEDIT_ARMS3XP));

	// adaptability-arms4
	hctl = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_ARMS4);
	for (int idx = 0; idx <= hero_adaptability_max; idx ++) {
		sprintf(text, "%s%i", HERO_PREFIX_STR_ADAPTABILITY, idx);
		ComboBox_AddString(hctl, utf8_2_ansi(dgettext("wesnoth-hero", text))); 
	}
	hctl = GetDlgItem(hdlgP, IDC_UD_HEROEDIT_ARMS4XP);
	UpDown_SetRange(hctl, 0, 4095);	// [0, 4095]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_HEROEDIT_ARMS4XP));

	// adaptability-skill_commercial
	hctl = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_SKILL0);
	for (int idx = 0; idx <= hero_adaptability_max; idx ++) {
		sprintf(text, "%s%i", HERO_PREFIX_STR_ADAPTABILITY, idx);
		ComboBox_AddString(hctl, utf8_2_ansi(dgettext("wesnoth-hero", text))); 
	}
	hctl = GetDlgItem(hdlgP, IDC_UD_HEROEDIT_SKILL0XP);
	UpDown_SetRange(hctl, 0, 4095);	// [0, 4095]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_HEROEDIT_SKILL0XP));

	// adaptability-skill_commercial
	hctl = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_SKILL1);
	for (int idx = 0; idx <= hero_adaptability_max; idx ++) {
		sprintf(text, "%s%i", HERO_PREFIX_STR_ADAPTABILITY, idx);
		ComboBox_AddString(hctl, utf8_2_ansi(dgettext("wesnoth-hero", text))); 
	}
	hctl = GetDlgItem(hdlgP, IDC_UD_HEROEDIT_SKILL1XP);
	UpDown_SetRange(hctl, 0, 4095);	// [0, 4095]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_HEROEDIT_SKILL1XP));

	// adaptability-skill_encourage
	hctl = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_SKILL3);
	for (int idx = 0; idx <= hero_adaptability_max; idx ++) {
		sprintf(text, "%s%i", HERO_PREFIX_STR_ADAPTABILITY, idx);
		ComboBox_AddString(hctl, utf8_2_ansi(dgettext("wesnoth-hero", text))); 
	}
	hctl = GetDlgItem(hdlgP, IDC_UD_HEROEDIT_SKILL3XP);
	UpDown_SetRange(hctl, 0, 4095);	// [0, 4095]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_HEROEDIT_SKILL3XP));

	// adaptability-skill_hero
	hctl = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_SKILL4);
	for (int idx = 0; idx <= hero_adaptability_max; idx ++) {
		sprintf(text, "%s%i", HERO_PREFIX_STR_ADAPTABILITY, idx);
		ComboBox_AddString(hctl, utf8_2_ansi(dgettext("wesnoth-hero", text))); 
	}
	hctl = GetDlgItem(hdlgP, IDC_UD_HEROEDIT_SKILL4XP);
	UpDown_SetRange(hctl, 0, 4095);	// [0, 4095]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_HEROEDIT_SKILL4XP));

	// adaptability-skill_demolish
	hctl = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_SKILL5);
	for (int idx = 0; idx <= hero_adaptability_max; idx ++) {
		sprintf(text, "%s%i", HERO_PREFIX_STR_ADAPTABILITY, idx);
		ComboBox_AddString(hctl, utf8_2_ansi(dgettext("wesnoth-hero", text))); 
	}
	hctl = GetDlgItem(hdlgP, IDC_UD_HEROEDIT_SKILL5XP);
	UpDown_SetRange(hctl, 0, 4095);	// [0, 4095]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_HEROEDIT_SKILL5XP));

	// adaptability-skill_formation
	hctl = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_SKILL6);
	for (int idx = 0; idx <= hero_adaptability_max; idx ++) {
		sprintf(text, "%s%i", HERO_PREFIX_STR_ADAPTABILITY, idx);
		ComboBox_AddString(hctl, utf8_2_ansi(dgettext("wesnoth-hero", text))); 
	}
	hctl = GetDlgItem(hdlgP, IDC_UD_HEROEDIT_SKILL6XP);
	UpDown_SetRange(hctl, 0, 4095);	// [0, 4095]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_HEROEDIT_SKILL6XP));

	// catalog
	hctl = GetDlgItem(hdlgP, IDC_UD_HEROEDIT_CATALOG);
	UpDown_SetRange(hctl, 0, HERO_MAX_LOYALTY - 1);	// [0, HERO_MAX_LOYALTY - 1]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_HEROEDIT_CATALOG));

	// heart
	hctl = GetDlgItem(hdlgP, IDC_UD_HEROEDIT_HEART);
	UpDown_SetRange(hctl, 0, 255);	// [0, 255]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_HEROEDIT_HEART));

	update_hero_edit(hdlgP, gdmgr.heros_[gdmgr._menu_lparam]);

	SetFocus(GetDlgItem(hdlgP, IDC_ET_HEROEDIT_NAME));

	return FALSE;
}

void On_DlgHeroEditCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	if (candidate_hero::on_command(hdlgP, id, codeNotify)) {
		return;
	}
	BOOL changed = FALSE;
	int relation_id = -1;
	int relation_max = -1;

	switch (id) {
	case IDM_VARIABLE_ITEM0:
		relation_id = IDC_ET_HEROEDIT_FATHER;
		relation_max = 1;
	case IDM_VARIABLE_ITEM1:
		if (relation_id == -1) {
			relation_id = IDC_ET_HEROEDIT_MOTHER;
			relation_max = 1;
		}
	case IDM_VARIABLE_ITEM2:
		if (relation_id == -1) {
			relation_id = IDC_ET_HEROEDIT_CONSORT;
			relation_max = HEROS_MAX_CONSORT;
		}
	case IDM_VARIABLE_ITEM3:
		if (relation_id == -1) {
			relation_id = IDC_ET_HEROEDIT_OATH;
			relation_max = HEROS_MAX_OATH;
		}
	case IDM_VARIABLE_ITEM4:
		if (relation_id == -1) {
			relation_id = IDC_ET_HEROEDIT_INTIMATE;
			relation_max = HEROS_MAX_INTIMATE;
		}
	case IDM_VARIABLE_ITEM5:
		if (relation_id == -1) {
			relation_id = IDC_ET_HEROEDIT_HATE;
			relation_max = HEROS_MAX_HATE;
		}
		strcat_heros_str2(hdlgP, relation_id, candidate_hero::lParam, relation_max, true);
		break;

	case IDM_RESET:
		update_hero_edit(hdlgP, gdmgr.heros_[gdmgr._menu_lparam]);
		break;
	case IDOK:
		if (!save_hero_edit(hdlgP, &changed)) {
			break;
		}
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
	return;
}

void On_DlgHeroEditHScroll(HWND hdlgP, HWND hwndCtrl, UINT code, int pos)
{
	HRESULT		hr = S_OK;

	LRESULT posMin, posMax, posCnt;
	posCnt = TrackBar_GetPos(hwndCtrl);
	TrackBar_GetRange(hwndCtrl, &posMin, &posMax);

	switch(code) {
	case TB_LINEDOWN:
		posCnt -= 1;
		break;
	case TB_LINEUP:
		posCnt += 1;
		break;
	case TB_PAGEDOWN:
		posCnt -= (posMax-posMin+1) / PAGE_NUM;
		break;
	case TB_PAGEUP:
		posCnt += (posMax-posMin+1) / PAGE_NUM;
		break;
	case TB_THUMBTRACK: // 当按住鼠标的所有轨迹,会形成一大串
		posCnt = pos;
		break;
		return; // 太频繁,先不处理
	case TB_THUMBPOSITION: // 放开鼠标时的值
		posCnt = pos;
		break;
	case TB_BOTTOM:
		posCnt = posMin;
		break;
	case TB_TOP:
		posCnt = posMax;
		break;
	default:
		return;
	}
	if(posCnt > posMax) posCnt = posMax;
	if(posCnt < posMin) posCnt = posMin;
	
	TrackBar_SetPos(hwndCtrl, posCnt, TRUE);

	return ;
}

bool is_relation_enable(HWND hdlgP, int id, LPARAM lparam)
{
	char text[_MAX_PATH];
	std::vector<int> v;
	std::map<int, int> assembled;
	std::set<int> ids;
	const hero& curr = gdmgr.heros_[gdmgr._menu_lparam];
	const hero& h = gdmgr.heros_[lparam];

	ids.insert(IDC_ET_HEROEDIT_FATHER);
	ids.insert(IDC_ET_HEROEDIT_MOTHER);
	ids.insert(IDC_ET_HEROEDIT_CONSORT);
	ids.insert(IDC_ET_HEROEDIT_OATH);
	ids.insert(IDC_ET_HEROEDIT_INTIMATE);
	ids.insert(IDC_ET_HEROEDIT_HATE);

	for (std::set<int>::const_iterator it = ids.begin(); it != ids.end(); ++ it) {
		Edit_GetText(GetDlgItem(hdlgP, *it), text, _MAX_PATH);
		v = to_vector_int2(text);
		if (std::find(v.begin(), v.end(), h.number_) != v.end()) {
			return false;
		}
		assembled.insert(std::make_pair(*it, v.size()));
	}
	
	if (h.gender_ != hero_gender_male && h.gender_ != hero_gender_female) {
		return false;
	}

	int relation_max = -1;
	int hit_et_id;
	if (id == IDM_VARIABLE_ITEM0) {
		relation_max = 1;
		hit_et_id = IDC_ET_HEROEDIT_FATHER;
		if (h.gender_ != hero_gender_male) {
			return false;
		}

	} else if (id == IDM_VARIABLE_ITEM1) {
		relation_max = 1;
		hit_et_id = IDC_ET_HEROEDIT_MOTHER;
		if (h.gender_ != hero_gender_female) {
			return false;
		}

	} else if (id == IDM_VARIABLE_ITEM2) {
		relation_max = HEROS_MAX_CONSORT;
		hit_et_id = IDC_ET_HEROEDIT_CONSORT;
		if (h.gender_ == curr.gender_) {
			return false;
		}

	} else if (id == IDM_VARIABLE_ITEM3) {
		relation_max = HEROS_MAX_OATH;
		hit_et_id = IDC_ET_HEROEDIT_OATH;
		if (h.gender_ != curr.gender_) {
			return false;
		}

	} else if (id == IDM_VARIABLE_ITEM4) {
		relation_max = HEROS_MAX_INTIMATE;
		hit_et_id = IDC_ET_HEROEDIT_INTIMATE;

	} else if (id == IDM_VARIABLE_ITEM5) {
		relation_max = HEROS_MAX_HATE;
		hit_et_id = IDC_ET_HEROEDIT_HATE;
	}

	if (relation_max != -1 && assembled.find(hit_et_id)->second >= relation_max) {
		return false;
	}

	return true;
}

BOOL On_DlgHeroEditNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	if (lpNMHdr->code == NM_RCLICK) {
		std::map<int, std::string> candidate_menu;
		candidate_menu.insert(std::make_pair(IDM_VARIABLE_ITEM0, _("To Father")));
		candidate_menu.insert(std::make_pair(IDM_VARIABLE_ITEM1, _("To Mother")));
		candidate_menu.insert(std::make_pair(IDM_VARIABLE_ITEM2, _("To Consort")));
		candidate_menu.insert(std::make_pair(IDM_VARIABLE_ITEM3, _("To Oath")));
		candidate_menu.insert(std::make_pair(IDM_VARIABLE_ITEM4, _("To Intimate")));
		candidate_menu.insert(std::make_pair(IDM_VARIABLE_ITEM5, _("To Hate")));

		if (candidate_hero::notify_handler_rclick(candidate_menu, hdlgP, DlgItem, lpNMHdr, is_relation_enable)) {
		}
	}
	return FALSE;
}

BOOL CALLBACK DlgHeroEditProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_INITDIALOG:
		return On_DlgHeroEditInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgHeroEditCommand);
	HANDLE_MSG(hdlgP, WM_HSCROLL, On_DlgHeroEditHScroll);
	HANDLE_MSG(hdlgP, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	HANDLE_MSG(hdlgP, WM_NOTIFY, On_DlgHeroEditNotify);
	}
	
	return FALSE;
}

void exchange_number(hero& h, int a, int b)
{
	if (h.parent_[0].hero_ == a) {
		h.parent_[0].hero_ = b;
	} else if (h.parent_[0].hero_ == b) {
		h.parent_[0].hero_ = a;
	}
	if (h.parent_[1].hero_ == a) {
		h.parent_[1].hero_ = b;
	} else if (h.parent_[1].hero_ == b) {
		h.parent_[1].hero_ = a;
	}

	for (int i = 0; i < HEROS_MAX_CONSORT; i ++) {
		if (h.consort_[i].hero_ == a) {
			h.consort_[i].hero_ = b;
		} else if (h.consort_[i].hero_ == b) {
			h.consort_[i].hero_ = a;
		}
	}

	for (int i = 0; i < HEROS_MAX_OATH; i ++) {
		if (h.oath_[i].hero_ == a) {
			h.oath_[i].hero_ = b;
		} else if (h.oath_[i].hero_ == b) {
			h.oath_[i].hero_ = a;
		}
	}

	for (int i = 0; i < HEROS_MAX_INTIMATE; i ++) {
		if (h.intimate_[i].hero_ == a) {
			h.intimate_[i].hero_ = b;
		} else if (h.intimate_[i].hero_ == b) {
			h.intimate_[i].hero_ = a;
		}
	}

	for (int i = 0; i < HEROS_MAX_HATE; i ++) {
		if (h.hate_[i].hero_ == a) {
			h.hate_[i].hero_ = b;
		} else if (h.hate_[i].hero_ == b) {
			h.hate_[i].hero_ = a;
		}
	}
}

void copyfile2(const std::string& src, const std::string& dst)
{
	BOOL fok = CopyFile(src.c_str(), dst.c_str(), FALSE);
	if (!fok) {
		std::stringstream strstr;
		utils::string_map symbols;

		symbols["src"] = src;
		symbols["dst"] = dst;
		symbols["result"] = fok? "success": "fail";
		strstr.str("");
		strstr << utf8_2_ansi(vgettext2("Copy \"$src\" to \"$dst\" $result!", symbols).c_str());
		posix_print_mb(strstr.str().c_str());
	}
}

void exchange_image(int from, int to)
{
	std::stringstream strstr;
	std::string from_image, to_image, temporary;
	uint32_t fsize_low, fsize_high;
	bool from_existed, to_existed;

	for (int step = 0; step < 2; step ++) {
		strstr.str("");
		if (step == 0) {
			strstr << hero::image_file_root_ + "/" << "hero-64/" << from << ".png";
			from_image = strstr.str();

			strstr.str("");
			strstr << hero::image_file_root_ + "/" << "hero-64/" << to << ".png";
			to_image = strstr.str();

		} else {
			strstr << hero::image_file_root_ + "/" << "hero-256/" << from << ".png";
			from_image = strstr.str();

			strstr.str("");
			strstr << hero::image_file_root_ + "/" << "hero-256/" << to << ".png";
			to_image = strstr.str();
		}

		posix_fsize_byname(from_image.c_str(), fsize_low, fsize_high);
		from_existed = fsize_low || fsize_high;

		posix_fsize_byname(to_image.c_str(), fsize_low, fsize_high);
		to_existed = fsize_low || fsize_high;

		if (from_existed && !to_existed) {
			copyfile2(from_image, to_image);
			delfile1(from_image.c_str());

		} else if (!from_existed && to_existed) {
			copyfile2(to_image, from_image);
			delfile1(to_image.c_str());

		} else if (from_existed && to_existed) {
			const std::string temporary = get_user_data_dir() + "/_temporary.png";
			copyfile2(from_image, temporary);
			copyfile2(to_image, from_image);
			copyfile2(temporary, to_image);
			delfile1(temporary.c_str());
		}
	}	
}

void exchange_po(int from, int to)
{
	return;
	// 非清楚代码功能的不要使用这函数
	// 要确保这函数起作, 须具备以下条件
	// 1.zh_CN.po/en_GB.po中不存在fuzzy翻译. 一些msgid可能在fuzzy中, 修改fuzzy会破坏poedit要求.
	// 2.zh_CN.po/en_GB.po中必须已在相关msgid. 先得手工在pot添加, 然后倒入po.
	std::string name = game_config::path + "/po/wesnoth-hero/zh_CN.po";
	posix_file_t fp;

	uint32_t bytertd, fsizelow, fsizehigh;
	char* fdata = NULL;
	int from_msgstr_len, to_msgstr_len;
	char* from_msgstr = NULL;
	char* to_msgstr = NULL;
	std::stringstream from_key, to_key;

	for (int lang = 0; lang < 2; lang ++) {
		if (lang == 1) {
			name = game_config::path + "/po/wesnoth-hero/en_GB.po";
		}
		posix_fopen(name.c_str(), GENERIC_READ | GENERIC_WRITE, OPEN_EXISTING, fp);
		if (fp == INVALID_FILE) {
			goto exit;
		}

		posix_fsize(fp, fsizelow, fsizehigh);
		if (!fsizelow && !fsizehigh) {
			goto exit;
		}
		fdata = (char*)malloc(fsizelow);
		if (!fdata) {
			goto exit;
		}
		posix_fseek(fp, 0, 0);
		posix_fread(fp, fdata, fsizelow, bytertd);

		int step;
		for (step = 0; step < 3; step ++) {
			from_key.str("");
			to_key.str("");
			if (step == 0) {
				from_key << HERO_PREFIX_STR_NAME << from;
				to_key << HERO_PREFIX_STR_NAME << to;
			} else if (step == 1) {
				from_key << HERO_PREFIX_STR_SURNAME << from;
				to_key << HERO_PREFIX_STR_SURNAME << to;
			} else if (step == 2) {
				from_key << HERO_PREFIX_STR_BIOGRAPHY << from;
				to_key << HERO_PREFIX_STR_BIOGRAPHY << to;
			}

			char* from_ptr = strstr(fdata, from_key.str().c_str());
			if (!from_ptr) {
				break;
			}
			from_ptr = strstr(from_ptr, "msgstr");
			if (!from_ptr) {
				break;
			}

			char* to_ptr = strstr(fdata, to_key.str().c_str());
			if (!to_ptr) {
				break;
			}
			to_ptr = strstr(to_ptr, "msgstr");
			if (!to_ptr) {
				break;
			}
			// ensure from_ptr less than to_ptr
			if (from_ptr > to_ptr) {
				char* temp = to_ptr;
				to_ptr = from_ptr;
				from_ptr = temp;
			}

			char* end = strstr(from_ptr, "\n\n");
			if (!end) {
				// if need replace last msgid, nextlline!
				break;
			}
			from_msgstr_len = end - from_ptr;

			end = strstr(to_ptr, "\n\n");
			if (!end) {
				// if need replace last msgid, nextlline!
				break;
			}
			to_msgstr_len = end - to_ptr;

			from_msgstr = (char*)malloc(from_msgstr_len + 1);
			memcpy(from_msgstr, from_ptr, from_msgstr_len);
			from_msgstr[from_msgstr_len] = '\0';
			
			to_msgstr = (char*)malloc(to_msgstr_len + 1);
			memcpy(to_msgstr, to_ptr, to_msgstr_len);
			to_msgstr[to_msgstr_len] = '\0';

			int middle_len = to_ptr - from_ptr - from_msgstr_len;
			// move
			memcpy(from_ptr + to_msgstr_len, from_ptr + from_msgstr_len, middle_len);
			memcpy(from_ptr, to_msgstr, to_msgstr_len);
			memcpy(from_ptr + to_msgstr_len + middle_len, from_msgstr, from_msgstr_len);

			free(from_msgstr);
			free(to_msgstr);

		}
		if (step == 3) {
			posix_fseek(fp, 0, 0);
			posix_fwrite(fp, fdata, fsizelow, bytertd);
		}
		posix_fclose(fp);
		fp = INVALID_FILE;

		free(fdata);
		fdata = NULL;
	}
exit:
	if (fp != INVALID_FILE) {
		posix_fclose(fp);
	}
	if (fdata) {
		free(fdata);
	}
}

void exchange_hero(HWND hdlgP, hero_map& heros, int a_number, int b_number)
{
	if (a_number < 0 || a_number >= (int)heros.size()) {
		return;
	}
	if (b_number < 0 || b_number >= (int)heros.size()) {
		return;
	}
	if (a_number == b_number) {
		return;
	}

	for (hero_map::iterator it = heros.begin(); it != heros.end(); ++ it) {
		hero& h = *it;
		exchange_number(h, a_number, b_number);
	}

	hero a = heros[a_number];
	hero temp = a;

	temp.number_ = b_number;
	temp.image_ = heros[b_number].image_;
	temp.set_name("");
	temp.set_surname("");
	
	hero b = heros[b_number];
	heros[b_number] = temp;
	temp = b;

	temp.number_ = a_number;
	temp.image_ = a.image_;
	temp.set_name("");
	temp.set_surname("");
	heros[a_number] = temp;

	exchange_image(a_number, b_number);
	exchange_po(a_number, b_number);

	OnSaveBt(hdlgP);
	refresh_editor_use_heros(hdlgP);

	std::stringstream strstr;
	utils::string_map symbols;
	symbols["a"] = heros[a_number].name();
	symbols["b"] = heros[b_number].name();
	strstr << vgettext2("Exchange $a and $b success! To complete, you need edit wesnoth-hero.po manually.", symbols);
	posix_print_mb(utf8_2_ansi(strstr.str().c_str()));
}

void extract_and_append(HWND hdlgP, hero_map& heros, int number)
{
	if (number < 0 || number >= (int)heros.size()) {
		return;
	}

	std::stringstream strstr;
	utils::string_map symbols;
	symbols["hero"] = heros[number].name();
	strstr << utf8_2_ansi(vgettext2("Do you want to extract $hero and append?", symbols).c_str());

	int retval = MessageBox(gdmgr._hwnd_main, strstr.str().c_str(), utf8_2_ansi(_("Confirm")), MB_YESNO);
	if (retval == IDNO) {
		return;
	}

	hero append = heros[number];
	append.number_ = heros.size();
	append.image_ = append.number_;
	append.set_name("");
	append.set_surname("");
	
	for (hero_map::iterator it = heros.begin(); it != heros.end(); ++ it) {
		hero& h = *it;
		exchange_number(h, number, append.number_);
	}
	exchange_image(number, append.number_);
	exchange_po(number, append.number_);

	heros.add(append);
	OnSaveBt(hdlgP);
	refresh_editor_use_heros(hdlgP);

	strstr.str("");
	strstr << vgettext2("Extract $hero and append success! To complete, you need edit wesnoth-hero.po manually.", symbols);
	posix_print_mb(utf8_2_ansi(strstr.str().c_str()));
}

BOOL On_DlgAdjustInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	std::stringstream strstr;
	int this_number = gdmgr._menu_lparam;

	if (ns::id == IDM_EXCHANGE) {
		strstr << _("Exchange");
	}
	SetWindowText(hdlgP, utf8_2_ansi(strstr.str().c_str()));

	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_ADJUST), utf8_2_ansi(_("Exchange")));

	HWND hctl = GetDlgItem(hdlgP, IDC_ET_ADJUST_THIS);
	strstr.str("");
	hero& this_hero = gdmgr.heros_[this_number];
	strstr << std::setfill('0') << std::setw(3) << this_number << ": " << this_hero.name();
	Edit_SetText(hctl, utf8_2_ansi(strstr.str().c_str()));

	hctl = GetDlgItem(hdlgP, IDC_CMB_ADJUST_THAT);
	for (hero_map::iterator it = gdmgr.heros_.begin(); it != gdmgr.heros_.end(); ++ it) {
		hero& h = *it;
		if (h.number_ == this_number) {
			continue;
		}
		strstr.str("");
		int number = h.number_;
		strstr << std::setfill('0') << std::setw(3) << number << ": " << h.name();
		ComboBox_AddString(hctl, utf8_2_ansi(strstr.str().c_str()));
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, h.number_);
	}
	ComboBox_SetCurSel(hctl, 0);
	return FALSE;
}

void On_DlgAdjustCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;
	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_ADJUST_THAT);

	switch (id) {
	case IDOK:
		ns::that_number = ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));
		changed = TRUE;
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

BOOL CALLBACK DlgAdjustProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message) {
	case WM_INITDIALOG:
		return On_DlgAdjustInitDialog(hDlg, (HWND)(wParam), lParam);
	HANDLE_MSG(hDlg, WM_COMMAND, On_DlgAdjustCommand);
	HANDLE_MSG(hDlg, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	}
	
	return FALSE;
}

#endif