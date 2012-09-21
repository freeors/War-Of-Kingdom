#include "global.hpp"
#include "game_config.hpp"
#include "stdafx.h"
#include <windowsx.h>

#include "resource.h"
#include "xfunc.h"
#include "win32x.h"
#include "struct.h"
// #include "utf8.h"

#include "gettext.hpp"

#include <sstream>
#include <iosfwd>

BOOL CALLBACK DlgHeroEditProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam);

#define wgen_enable_save_btn(fEnable)	ToolBar_EnableButton(gdmgr._htb_wgen, IDM_SAVE, fEnable)

#define MAX_CFGITEMS			9		// 第一项变成了自定义项,不在显示,在此最大项数变成9，位在内核中还是10
#define INVALID_CFGIDX			-1		// 判断简单点

#define NAME_CUSTOM_PROFILE		"Custom"

static environ_var_t	*evar = NULL;
static int				gcfgidx = -1;

typedef enum {
	ma_unkonwn			= 0,
	ma_new				= 1,
	ma_rename			= 2,

	ma_edit				= 3,
	ma_add				= 4,
} mgr_action_t;

typedef struct {
	mgr_action_t	_ma;
	int				_cfgidx;
	char			_cfgname[MAX_CFGITEM_CHARS + 1];
} cfgmgr_t;
cfgmgr_t			gcfgmgr;

static char	gtext[_MAX_PATH];
HINSTANCE	hinst = NULL;
HMENU		ghmenuMain = NULL;

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

void wgen_enter_ui(void)
{
	// 免得误解
	EnableWindow(GetDlgItem(gdmgr._hdlg_ddesc, IDC_LV_DDESC_EXPLORER), FALSE);	// 灰掉右边视图列表

	StatusBar_Idle();

	strcpy(gdmgr.heros_fname_, gdmgr._menu_text);
	StatusBar_SetText(gdmgr._hwnd_status, 0, gdmgr.heros_fname_);

	wgen_enable_save_btn(FALSE);

	UpdateUIFromFile(gdmgr._hdlg_wgen, gdmgr.heros_fname_);

	if (!gdmgr.list_header_height_) {
		RECT rc;
		ListView_GetItemRect(GetDlgItem(gdmgr._hdlg_wgen, IDC_LV_WGEN_EDITOR), 0, &rc, LVIR_BOUNDS);
		gdmgr.list_header_height_ = rc.top;
	}

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
		retval = MessageBox(gdmgr._hwnd_main, "Do you want to save the changes?", "Save Changes", MB_YESNO);
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

// general.number_决定了向列表控件是添加还是修改
void hero_data_2_lv(HWND hdlgP, hero& general)
{
	LVITEM lvi;
	char text[_MAX_PATH];
	std::string tstr;
	std::stringstream strstr;
	uint16_t count;
	int i, val;
	HWND hctl = GetDlgItem(hdlgP, IDC_LV_WGEN_EDITOR);

	count = ListView_GetItemCount(hctl);

	lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
	// 序号
	if (general.number_ >= count) {
		lvi.iItem = count;
	} else {
		lvi.iItem = general.number_;
	}
	lvi.iSubItem = 0;
	sprintf(text, "%u", general.number_);
	lvi.pszText = text;
	lvi.lParam = (LPARAM)0;
	lvi.iImage = select_iimage_according_fname(text, 0);
	if (lvi.iItem != count) {
		ListView_SetItem(hctl, &lvi);
	} else {
		ListView_InsertItem(hctl, &lvi);
	}

	// 姓名
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 1;

	lvi.pszText = const_cast<char*>(general.name().c_str());
	ListView_SetItem(hctl, &lvi);

	// 姓
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 2;
	lvi.pszText = const_cast<char*>(general.surname().c_str());
	ListView_SetItem(hctl, &lvi);

	// 统率
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 3;
	sprintf(text, "%u.%u", fxptoi9(general.leadership_), fxpmod9(general.leadership_));
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 武力
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 4;
	sprintf(text, "%u.%u", fxptoi9(general.force_), fxpmod9(general.force_));
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 智力
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 5;
	sprintf(text, "%u.%u", fxptoi9(general.intellect_), fxpmod9(general.intellect_));
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 政治
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 6;
	sprintf(text, "%u.%u", fxptoi9(general.politics_), fxpmod9(general.politics_));
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 魅力
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 7;
	sprintf(text, "%u.%u", fxptoi9(general.charm_), fxpmod9(general.charm_));
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 性别
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 8;
	lvi.pszText = const_cast<char*>(hero::gender_str(general.gender_).c_str());
	ListView_SetItem(hctl, &lvi);

	// 图像编号
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 9;
	sprintf(text, "%u", general.image_);
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 基本/浮动相性
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 10;
	sprintf(text, "%u/%u.%u", general.base_catalog_, fxptoi8(general.float_catalog_), fxpmod8(general.float_catalog_));
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 野心
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 11;
	lvi.pszText = const_cast<char*>(general.ambition_str().c_str());
	ListView_SetItem(hctl, &lvi);

	// 义理
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 12;
	lvi.pszText = const_cast<char*>(general.heart_str().c_str());
	ListView_SetItem(hctl, &lvi);

	// 特技
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 13;
	for (i = hero_feature_min; i <= hero_feature_max; i ++) {
		if (hero_feature_val2(general, i)) {
			break;
		}
	}
	if (i <= hero_feature_max) {
		lvi.pszText = const_cast<char*>(general.feature_str(i).c_str());
	} else {
		lvi.pszText = NULL;
	}
	ListView_SetItem(hctl, &lvi);

	// 阵营特技
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 14;
	if (general.side_feature_ != HEROS_NO_SIDE_FEATURE) {
		lvi.pszText = const_cast<char*>(general.feature_str(general.side_feature_).c_str());
	} else {
		lvi.pszText = NULL;
	}
	ListView_SetItem(hctl, &lvi);

	// 步兵适性
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 15;
	sprintf(text, "%s.%u", hero::adaptability_str2(general.arms_[hero_arms_t0]).c_str(), fxpmod12(general.arms_[0]));
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 骑兵适性
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 16;
	sprintf(text, "%s.%u", hero::adaptability_str2(general.arms_[hero_arms_t1]).c_str(), fxpmod12(general.arms_[1]));
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 兵器适性
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 17;
	sprintf(text, "%s.%u", hero::adaptability_str2(general.arms_[hero_arms_t2]).c_str(), fxpmod12(general.arms_[2]));
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 学院适性
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 18;
	sprintf(text, "%s.%u", hero::adaptability_str2(general.arms_[hero_arms_t3]).c_str(), fxpmod12(general.arms_[3]));
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 水军适性
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 19;
	sprintf(text, "%s.%u", hero::adaptability_str2(general.arms_[hero_arms_t4]).c_str(), fxpmod12(general.arms_[4]));
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// skill: commercial
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 20;
	sprintf(text, "%s.%u", hero::adaptability_str2(general.skill_[hero_skill_commercial]).c_str(), fxpmod12(general.skill_[hero_skill_commercial]));
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// skill: hero
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 21;
	sprintf(text, "%s.%u", hero::adaptability_str2(general.skill_[hero_skill_hero]).c_str(), fxpmod12(general.skill_[hero_skill_hero]));
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// skill: demolish
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 22;
	sprintf(text, "%s.%u", hero::adaptability_str2(general.skill_[hero_skill_demolish]).c_str(), fxpmod12(general.skill_[hero_skill_demolish]));
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 父母
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 23;
	strstr.str("");
	if (general.parent_[0].hero_ != HEROS_INVALID_NUMBER) {
		strstr << gdmgr.heros_[general.parent_[0].hero_].name().c_str();
	} else {
		text[0] = '\0';
	}
	val = general.parent_[0].feeling_;
	strstr << "(" << val << ")";

	if (general.parent_[1].hero_ != HEROS_INVALID_NUMBER) {
		strstr << ", " << gdmgr.heros_[general.parent_[1].hero_].name().c_str();
	}
	val = general.parent_[1].feeling_;
	strstr << "(" << val << ")";
	strcpy(text, strstr.str().c_str());
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 配偶
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 24;
	strstr.str("");
	for (i = 0; i < HEROS_MAX_CONSORT; i ++) {
		if (general.consort_[i].hero_ != HEROS_INVALID_NUMBER) {
			if (i == 0) {
				strstr << gdmgr.heros_[general.consort_[i].hero_].name().c_str();
			} else {
				strstr << ", " << gdmgr.heros_[general.consort_[i].hero_].name().c_str();
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
	lvi.iSubItem = 25;
	strstr.str("");
	for (i = 0; i < HEROS_MAX_OATH; i ++) {
		if (general.oath_[i].hero_ != HEROS_INVALID_NUMBER) {
			if (i == 0) {
				strstr << gdmgr.heros_[general.oath_[i].hero_].name().c_str();
			} else {
				strstr << ", " << gdmgr.heros_[general.oath_[i].hero_].name().c_str();
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
	lvi.iSubItem = 26;
	strstr.str("");
	for (i = 0; i < HEROS_MAX_INTIMATE; i ++) {
		if (general.intimate_[i].hero_ != HEROS_INVALID_NUMBER) {
			if (i == 0) {
				strstr << gdmgr.heros_[general.intimate_[i].hero_].name().c_str();
			} else {
				strstr << ", " << gdmgr.heros_[general.intimate_[i].hero_].name().c_str();
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
	lvi.iSubItem = 27;
	strstr.str("");
	for (i = 0; i < HEROS_MAX_HATE; i ++) {
		if (general.hate_[i].hero_ != HEROS_INVALID_NUMBER) {
			if (i == 0) {
				strstr << gdmgr.heros_[general.hate_[i].hero_].name().c_str();
			} else {
				strstr << ", " << gdmgr.heros_[general.hate_[i].hero_].name().c_str();
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
	lvi.iSubItem = 28;
	sprintf(text, "%u", general.meritorious_);
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 状态
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 29;
	sprintf(text, "%s/%u", hero::status_str(general.status_).c_str(), general.activity_);
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 官职
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 30;
	sprintf(text, hero::official_str(general.official_).c_str());
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 列传
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 31;
	const char* biography = general.biography();
	// lvi.pszText = const_cast<char*>(biography.c_str());
	lvi.pszText = const_cast<char*>(general.biography());
	ListView_SetItem(hctl, &lvi);

	return;
}

#define GetCfgDescHandle()	GetDlgItem(gdmgr._htb_wgen, IDC_CMB_WGEN_CFGDESC_SRC)
// 对话框消息处理函数
BOOL On_DlgWGenInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	HWND			hctl = GetDlgItem(hdlgP, IDC_LV_WGEN_EDITOR);
	LVCOLUMN		lvc;

	gdmgr._hdlg_wgen = hdlgP;
	gdmgr._htb_wgen = init_toolbar(gdmgr._hinst, hdlgP);
	SetParent(GetDlgItem(hdlgP, IDC_ET_WGEN_FIND), gdmgr._htb_wgen);
	if (gdmgr._dvrsrc == dsrc_network) {
		create_wgen_toolinfo(gdmgr._hdlg_wgen);
	}

	if (!evar) {
        evar = (environ_var_t *)malloc(sizeof(environ_var_t) * MAX_CFGITEMS);
		memset(evar, 0, sizeof(environ_var_t) * MAX_CFGITEMS);
	}

	//
	// 初始化列视图控件
	//
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 40;
	lvc.pszText = "序号";
	lvc.cchTextMax = 0;
	lvc.iSubItem = 0;
	ListView_InsertColumn(hctl, 0, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 60;
	lvc.iSubItem = 1;
	lvc.pszText = "姓名";
	ListView_InsertColumn(hctl, 1, &lvc);

	lvc.cx = 48;
	lvc.iSubItem = 2;
	lvc.pszText = "姓";
	ListView_InsertColumn(hctl, 2, &lvc);

	lvc.cx = 54;
	lvc.iSubItem = 3;
	lvc.pszText = "统率";
	ListView_InsertColumn(hctl, 3, &lvc);

	lvc.cx = 54;
	lvc.iSubItem = 4;
	lvc.pszText = "武力";
	ListView_InsertColumn(hctl, 4, &lvc);

	lvc.cx = 54;
	lvc.iSubItem = 5;
	lvc.pszText = "智力";
	ListView_InsertColumn(hctl, 5, &lvc);

	lvc.cx = 54;
	lvc.iSubItem = 6;
	lvc.pszText = "政治";
	ListView_InsertColumn(hctl, 6, &lvc);

	lvc.cx = 54;
	lvc.iSubItem = 7;
	lvc.pszText = "魅力";
	ListView_InsertColumn(hctl, 7, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = 8;
	lvc.pszText = "性别";
	ListView_InsertColumn(hctl, 8, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = 9;
	lvc.pszText = "图像编号";
	ListView_InsertColumn(hctl, 9, &lvc);

	lvc.cx = 90;
	lvc.iSubItem = 10;
	lvc.pszText = "基本/浮动相性";
	ListView_InsertColumn(hctl, 10, &lvc);

	lvc.cx = 60;
	lvc.iSubItem = 11;
	lvc.pszText = "野心";
	ListView_InsertColumn(hctl, 11, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = 12;
	lvc.pszText = "义理";
	ListView_InsertColumn(hctl, 12, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = 13;
	lvc.pszText = "特技";
	ListView_InsertColumn(hctl, 13, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = 14;
	lvc.pszText = "阵营特技";
	ListView_InsertColumn(hctl, 14, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = 15;
	lvc.pszText = "步兵";
	ListView_InsertColumn(hctl, 15, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = 16;
	lvc.pszText = "骑兵";
	ListView_InsertColumn(hctl, 16, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = 17;
	lvc.pszText = "兵器";
	ListView_InsertColumn(hctl, 17, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = 18;
	lvc.pszText = "学院";
	ListView_InsertColumn(hctl, 18, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = 19;
	lvc.pszText = "水军";
	ListView_InsertColumn(hctl, 19, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = 20;
	lvc.pszText = "商才";
	ListView_InsertColumn(hctl, 20, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = 21;
	lvc.pszText = "一骑";
	ListView_InsertColumn(hctl, 21, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = 22;
	lvc.pszText = "攻城";
	ListView_InsertColumn(hctl, 22, &lvc);

	lvc.cx = 70;
	lvc.iSubItem = 23;
	lvc.pszText = "父母";
	ListView_InsertColumn(hctl, 23, &lvc);

	lvc.cx = 60;
	lvc.iSubItem = 24;
	lvc.pszText = "配偶";
	ListView_InsertColumn(hctl, 24, &lvc);

	lvc.cx = 70;
	lvc.iSubItem = 25;
	lvc.pszText = "义兄弟";
	ListView_InsertColumn(hctl, 25, &lvc);

	lvc.cx = 70;
	lvc.iSubItem = 26;
	lvc.pszText = "亲近武将";
	ListView_InsertColumn(hctl, 26, &lvc);

	lvc.cx = 70;
	lvc.iSubItem = 27;
	lvc.pszText = "厌恶武将";
	ListView_InsertColumn(hctl, 27, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = 28;
	lvc.pszText = "功勋";
	ListView_InsertColumn(hctl, 28, &lvc);

	lvc.cx = 80;
	lvc.iSubItem = 29;
	lvc.pszText = "状态/活力";
	ListView_InsertColumn(hctl, 29, &lvc);

	lvc.cx = 60;
	lvc.iSubItem = 30;
	lvc.pszText = "官职";
	ListView_InsertColumn(hctl, 30, &lvc);

	lvc.cx = 400;
	lvc.iSubItem = 31;
	lvc.pszText = "列传";
	ListView_InsertColumn(hctl, 31, &lvc);

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

// 跟据evar内容重新刷新存储CFGDESC的CMB控件
// 它只是更新内容不负责选责某条一条
void update_cfgdesc_use_evar(HWND hdlgP)
{
	int		idx;
	char	text[_MAX_PATH];
	HWND	hctl = GetCfgDescHandle();

	ComboBox_ResetContent(hctl);
	if ((gdmgr._cfgidx == 0) && (gdmgr._dvrsrc == dsrc_network)) {
		ComboBox_AddString(hctl, formatstr("* %s", NAME_CUSTOM_PROFILE));	// 添加一项默认的
	} else {
		ComboBox_AddString(hctl, NAME_CUSTOM_PROFILE);		// 添加一项默认的
	}
	for (idx = 0; idx < MAX_CFGITEMS; idx ++) {
		if (evar[idx]._cfgname[0]) {
			if (idx == 0) {
				sprintf(text, "%ist: %s", idx + 1, evar[idx]._cfgname);
			} else if (idx == 1) {
				sprintf(text, "%ind: %s", idx + 1, evar[idx]._cfgname);
			} else if (idx == 2) {
                sprintf(text, "%ird: %s", idx + 1, evar[idx]._cfgname);
			} else {
				sprintf(text, "%ith: %s", idx + 1, evar[idx]._cfgname);
			}
			if (gdmgr._dvrsrc == dsrc_network) {
				if ((gdmgr._cfgidx - 1)== idx) {
					strcpy(text, formatstr("* %s", text));
				} else {
					strcpy(text, formatstr("  %s", text));
				}
			}
			ComboBox_AddString(hctl, text);
		} else {
			break;
		}
	}
	return;
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

	sprintf(text, "要删除序号%u吗？", gdmgr._menu_lparam);
	retval = MessageBox(gdmgr._hwnd_main, text, "确认删除", MB_YESNO);
	if (retval == IDNO) {
		return;
	}
	gdmgr.heros_.erase(gdmgr._menu_lparam);
	refresh_editor_use_heros(hdlgP);

	wgen_enable_save_btn(TRUE);

	return;
}

// 将当前hero_map存成文件
void OnSaveBt(HWND hdlgP)
{
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

void OnFindBt(HWND hdlgP)
{
	HWND hctl = GetDlgItem(hdlgP, IDC_LV_WGEN_EDITOR);
	char text[_MAX_PATH];

	Edit_GetText(GetDlgItem(gdmgr._htb_wgen, IDC_ET_WGEN_FIND), text, sizeof(text) / sizeof(text[0]));
	if (!strlen(text)) {
		return;
	}

	RECT line0_rcct;
	ListView_GetItemRect(hctl, 0, &line0_rcct, LVIR_BOUNDS);
	int data_line_height = line0_rcct.bottom - line0_rcct.top;
	// size_t first_viewed_line = posix_abs(line0_rcct.top - gdmgr.list_header_height_) / data_line_height;
	size_t first_viewed_line = ListView_GetTopIndex(hctl);
	// size_t lines_per_screen = ListView_GetCountPerPage(hctl);

	size_t found, idx, count, hero_size = gdmgr.heros_.size();
	for (found = idx = first_viewed_line + 1; idx < hero_size; found ++, idx ++) {
		hero& h = gdmgr.heros_[idx];
		size_t size = strlen(text);
		if (!strncasecmp(text, h.name().c_str(), size)) {
			break;
		}
	}
	if (found == hero_size) {
		for (found = 0, idx = 0; idx < first_viewed_line + 1; found ++, idx ++) {
			hero& h = gdmgr.heros_[idx];
			size_t size = strlen(text);
			if (!strncasecmp(text, h.name().c_str(), size)) {
				break;
			}
		}
		if (found == first_viewed_line + 1) {
			found = hero_size;
		}
	}

	if (found != hero_size) {
		RECT rc;
		LONG need_scroll_to_y;
		
		SetFocus(hctl);
		count = ListView_GetItemCount(hctl);
		for (idx = 0; idx < count; idx ++) {
			if (idx != found) {
				ListView_SetItemState(hctl, idx, 0, LVIS_SELECTED | LVIS_FOCUSED);
			} else {
				ListView_SetItemState(hctl, idx, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
			}
		}
		ListView_GetItemRect(hctl, 0, &rc, LVIR_BOUNDS);
		need_scroll_to_y = found * (rc.bottom - rc.top);
		ListView_Scroll(hctl, 0, -1 * count * (rc.bottom - rc.top));
		ListView_Scroll(hctl, 0, need_scroll_to_y);
	}
	return;
}

void On_DlgWGenDestroy(HWND hdlgP)
{
	free(evar);
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

		EnableMenuItem(gdmgr._hpopup_editor, IDM_ADD, MF_BYCOMMAND);
		if (lpnmitem->iItem < 0) {
			EnableMenuItem(gdmgr._hpopup_editor, IDM_EDIT, MF_BYCOMMAND | MF_GRAYED);
			EnableMenuItem(gdmgr._hpopup_editor, IDM_DELETE, MF_BYCOMMAND | MF_GRAYED);
		}		

		TrackPopupMenuEx(gdmgr._hpopup_editor, 0, 
			point.x, 
			point.y, 
			hdlgP, 
			NULL);

		// 恢复回去
		EnableMenuItem(gdmgr._hpopup_editor, IDM_ADD, MF_BYCOMMAND | MF_ENABLED);
		EnableMenuItem(gdmgr._hpopup_editor, IDM_EDIT, MF_BYCOMMAND | MF_ENABLED);
		EnableMenuItem(gdmgr._hpopup_editor, IDM_DELETE, MF_BYCOMMAND | MF_ENABLED);

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

#ifndef NUMELMS
   #define NUMELMS(aa) (sizeof(aa)/sizeof((aa)[0]))
#endif

BOOL CALLBACK DlgWGenProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
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
			} else {
				AppendMenu(hPopupMenu, MF_STRING, IDM_WGEN_RESET_NTSC_M, "NTSC_M");
				AppendMenu(hPopupMenu, MF_STRING, IDM_WGEN_RESET_NTSC_M_J, "NTSC_M_J");
				AppendMenu(hPopupMenu, MF_STRING, IDM_WGEN_RESET_PAL_B, "PAL_B");
				AppendMenu(hPopupMenu, MF_STRING, IDM_WGEN_RESET_PAL_D, "PAL_D");
				AppendMenu(hPopupMenu, MF_STRING, IDM_WGEN_RESET_PAL_G, "PAL_G");
				AppendMenu(hPopupMenu, MF_STRING, IDM_WGEN_RESET_SECAM_D, "SECAM_D");
			}

			TrackPopupMenuEx(hPopupMenu,
				TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_VERTICAL,               
				rc.left, rc.bottom, hdlgP, &tpm); 

			DestroyMenu(hPopupMenu);			
			return (FALSE);
		}
	}
	
	return FALSE;
}
  
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

	gdmgr._tbBtns_wgen[16].iBitmap = MAKELONG(gdmgr._iico_backup, 0);
	gdmgr._tbBtns_wgen[16].idCommand = IDM_BACKUP;	
	gdmgr._tbBtns_wgen[16].fsState = TBSTATE_ENABLED;
	gdmgr._tbBtns_wgen[16].fsStyle = BTNS_BUTTON;
	gdmgr._tbBtns_wgen[16].dwData = 0L;
	gdmgr._tbBtns_wgen[16].iString = 0;


	ToolBar_AddButtons(gdmgr._htb_wgen, 16, &gdmgr._tbBtns_wgen);

	ToolBar_AutoSize(gdmgr._htb_wgen);
	
	ToolBar_SetExtendedStyle(gdmgr._htb_wgen, TBSTYLE_EX_DRAWDDARROWS);
	
	ToolBar_SetImageList(gdmgr._htb_wgen, gdmgr._himl_24x24, 0);
	ToolBar_SetDisabledImageList(gdmgr._htb_wgen, gdmgr._himl_24x24_dis);
	
	ShowWindow(gdmgr._htb_wgen, SW_SHOW);

	return gdmgr._htb_wgen;
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

void move_subcfg_right_position(HWND hdlgP, LPARAM lParam)
{
	RECT		rcMe;
	POINT		ptTrigger;
	int			cx, cy, cxScreen, cyScreen;

	cxScreen = GetSystemMetrics(SM_CXSCREEN);
	cyScreen = GetSystemMetrics(SM_CYSCREEN);;

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

BOOL On_DlgPopupNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	LVITEM					lvi;
	LPNMITEMACTIVATE		lpnmitem;
	int						icount;

	// NM_表示对通用控件都通用,范围丛(0, 99)
	// TVN_表示只能TreeView通用,范围丛(400, 499)
	if (lpNMHdr->code == TTN_GETDISPINFO) { 
            
        LPTOOLTIPTEXT lpttt; 

        lpttt = (LPTOOLTIPTEXT) lpNMHdr; 
        lpttt->hinst = gdmgr._hinst; 

        // Specify the resource identifier of the descriptive 
        // text for the given button. 
		if (lpttt->hdr.idFrom == IDM_RESET) {
            strcpy(lpttt->lpszText, "Reset");
            
		} else if (lpttt->hdr.idFrom == IDM_OK) {
            strcpy(lpttt->lpszText, "Save and exit");
            
		} else if (lpttt->hdr.idFrom == IDM_CANCEL) {
            strcpy(lpttt->lpszText, "Discard and exit"); 
        } 
    } else if (lpNMHdr->code == NM_RCLICK) {
		lpnmitem = (LPNMITEMACTIVATE)lpNMHdr;
		// 如果单击到的是复选框位置,把复选框状态置反
		// 当前定义的图标大小是16x16. ptAction反回的(x,y)是整个列表视图内的坐标,因而y值不大好判断
		// 认为如果x是小于16的就认为是击中复选框
		if ((lpnmitem->iItem >= 0) && (lpnmitem->iSubItem == 0)) {
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
			EnableMenuItem(gdmgr._hpopup_editor, IDM_ADD, MF_BYCOMMAND | MF_GRAYED);
			EnableMenuItem(gdmgr._hpopup_editor, IDM_EDIT, MF_BYCOMMAND | MF_GRAYED);
			EnableMenuItem(gdmgr._hpopup_editor, IDM_DELETE, MF_BYCOMMAND | MF_GRAYED);

			TrackPopupMenuEx(gdmgr._hpopup_editor, 0, 
				point.x, 
				point.y, 
				hdlgP, 
				NULL);

			// 恢复回去
			EnableMenuItem(gdmgr._hpopup_editor, IDM_ADD, MF_BYCOMMAND | MF_ENABLED);
			EnableMenuItem(gdmgr._hpopup_editor, IDM_EDIT, MF_BYCOMMAND | MF_ENABLED);
			EnableMenuItem(gdmgr._hpopup_editor, IDM_DELETE, MF_BYCOMMAND | MF_ENABLED);

			gdmgr._menu_lparam = lpnmitem->iItem;
		}
	
	}

    return FALSE;
}


//
// 4 Channel DVR Confinguration Popup Dialog
//

void update_hero_edit(HWND hdlgP, hero& h)
{
	char text[_MAX_PATH];
	HWND hctl;

	// number
	if (gcfgmgr._ma == ma_new) {
		sprintf(text, "%u (添加)", h.number_);
	} else if (gcfgmgr._ma == ma_edit) {
		sprintf(text, "%u (修改)", h.number_);
	} else {
		sprintf(text, "%u (未知动作)", h.number_);
	}
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_HEROEDIT_NUMBER), text);

	// name
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_HEROEDIT_NAME), h.name().c_str());

	// image number
    UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_IMAGE), h.image_);

	// leadership
    UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_LEADERSHIP), fxptoi9(h.leadership_));
	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_LEADERSHIPXP), fxpmod9(h.leadership_));

	// force
	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_FORCE), fxptoi9(h.force_));
	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_FORCEXP), fxpmod9(h.force_));

	// intellect
	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_INTELLECT), fxptoi9(h.intellect_));
	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_INTELLECTXP), fxpmod9(h.intellect_));

	// politics
	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_POLITICS), fxptoi9(h.politics_));
	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_POLITICSXP), fxpmod9(h.politics_));

	// charm
	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_CHARM), fxptoi9(h.charm_));
	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_CHARMXP), fxpmod9(h.charm_));

	// catalog
	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_CATALOG), h.base_catalog_);

	// heart
	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_HEART), h.heart_);

	// biography
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_HEROEDIT_BIOGRAPHY), h.biography());

	// gender
	ComboBox_SetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_GENDER), h.gender_);

	// ambition
	ComboBox_SetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_AMBITION), h.ambition_);

	// feature
	hctl = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_FEATURE);
	int first_feature = h.first_feature();
	for (int idx = 0; idx < ComboBox_GetCount(hctl); idx ++) {
		if (ComboBox_GetItemData(hctl, idx) == first_feature) {
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

	// adaptability-skill_hero
	ComboBox_SetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_SKILL1), fxptoi12(h.skill_[hero_skill_hero]));
	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_SKILL1XP), fxpmod12(h.skill_[hero_skill_hero]));

	// adaptability-skill_demolish
	ComboBox_SetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_SKILL2), fxptoi12(h.skill_[hero_skill_demolish]));
	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_SKILL2XP), fxpmod12(h.skill_[hero_skill_demolish]));
}


BOOL save_hero_edit(HWND hdlgP, BOOL *changed)
{
	uint32_t	u32n, u32n1, u32n2;
	int idx;
	// 列传可能大于_MAX_PATH
	// char		text[1024];

	// 1.检查参数是否合法

	// 2.参数放入全局结构evar
	*changed = FALSE;

	// image number
	u32n = (uint32_t)UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_IMAGE));
	if (gdmgr.heros_[gdmgr._menu_lparam].image_ != u32n) {
		*changed = TRUE;
		gdmgr.heros_[gdmgr._menu_lparam].image_ = u32n;
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
	// politics
	u32n1 = (uint32_t)UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_POLITICS));
	u32n2 = (uint32_t)UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_POLITICSXP));
	u32n = ftofxp9(u32n1) + u32n2;
	if (gdmgr.heros_[gdmgr._menu_lparam].politics_ != u32n) {
		*changed = TRUE;
		gdmgr.heros_[gdmgr._menu_lparam].politics_ = u32n;
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
	u32n = ComboBox_GetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_AMBITION));
	if (gdmgr.heros_[gdmgr._menu_lparam].ambition_ != u32n) {
		*changed = TRUE;
		gdmgr.heros_[gdmgr._menu_lparam].ambition_ = u32n;
	}

	// feature
	u32n = ComboBox_GetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_FEATURE));
	idx = ComboBox_GetItemData(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_FEATURE), u32n);
	if (gdmgr.heros_[gdmgr._menu_lparam].first_feature() != idx) {
		*changed = TRUE;
		memset(gdmgr.heros_[gdmgr._menu_lparam].feature_, 0, HEROS_FEATURE_BYTES);
		if (idx != HEROS_MAX_FEATURE) {
			hero_feature_set2(gdmgr.heros_[gdmgr._menu_lparam], idx);
		}
	}

	// side feature
	u32n = ComboBox_GetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_SIDEFEATURE));
	idx = ComboBox_GetItemData(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_SIDEFEATURE), u32n);
	if (gdmgr.heros_[gdmgr._menu_lparam].side_feature_ != idx) {
		*changed = TRUE;
		gdmgr.heros_[gdmgr._menu_lparam].side_feature_ = idx;
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

	// adaptability-skill_hero
	u32n1 = (uint32_t)ComboBox_GetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_SKILL1));
	u32n2 = (uint32_t)UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_SKILL1XP));
	u32n = ftofxp12(u32n1) + u32n2;
	if (gdmgr.heros_[gdmgr._menu_lparam].skill_[hero_skill_hero] != u32n) {
		*changed = TRUE;
		gdmgr.heros_[gdmgr._menu_lparam].skill_[hero_skill_hero] = u32n;
	}

	// adaptability-skill_demolish
	u32n1 = (uint32_t)ComboBox_GetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_SKILL2));
	u32n2 = (uint32_t)UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_HEROEDIT_SKILL2XP));
	u32n = ftofxp12(u32n1) + u32n2;
	if (gdmgr.heros_[gdmgr._menu_lparam].skill_[hero_skill_demolish] != u32n) {
		*changed = TRUE;
		gdmgr.heros_[gdmgr._menu_lparam].skill_[hero_skill_demolish] = u32n;
	}

	// consort[0]
	idx = ComboBox_GetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_CONSORT));
	if (idx >= 0) {
		u32n = ComboBox_GetItemData(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_CONSORT), idx);
		if (gdmgr.heros_[gdmgr._menu_lparam].consort_[0].hero_ != u32n) {
			*changed = TRUE;
			gdmgr.heros_[gdmgr._menu_lparam].consort_[0].hero_ = u32n;
		}
	}

	// parent[0]
	idx = ComboBox_GetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_PARENT0));
	if (idx >= 0) {
		u32n = ComboBox_GetItemData(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_PARENT0), idx);
		if (gdmgr.heros_[gdmgr._menu_lparam].parent_[0].hero_ != u32n) {
			*changed = TRUE;
			gdmgr.heros_[gdmgr._menu_lparam].parent_[0].hero_ = u32n;
		}
	}

	// parent[1]
	idx = ComboBox_GetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_PARENT1));
	if (idx >= 0) {
		u32n = ComboBox_GetItemData(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_PARENT1), idx);
		if (gdmgr.heros_[gdmgr._menu_lparam].parent_[1].hero_ != u32n) {
			*changed = TRUE;
			gdmgr.heros_[gdmgr._menu_lparam].parent_[1].hero_ = u32n;
		}
	}

	// oath[0]
	idx = ComboBox_GetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_OATH0));
	if (idx >= 0) {
		u32n = ComboBox_GetItemData(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_OATH0), idx);
		if (gdmgr.heros_[gdmgr._menu_lparam].oath_[0].hero_ != u32n) {
			*changed = TRUE;
			gdmgr.heros_[gdmgr._menu_lparam].oath_[0].hero_ = u32n;
		}
	}

	// oath[1]
	idx = ComboBox_GetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_OATH1));
	if (idx >= 0) {
		u32n = ComboBox_GetItemData(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_OATH1), idx);
		if (gdmgr.heros_[gdmgr._menu_lparam].oath_[1].hero_ != u32n) {
			*changed = TRUE;
			gdmgr.heros_[gdmgr._menu_lparam].oath_[1].hero_ = u32n;
		}
	}

	// intimate[0]
	idx = ComboBox_GetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_INTIMATE0));
	if (idx >= 0) {
		u32n = ComboBox_GetItemData(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_INTIMATE0), idx);
		if (gdmgr.heros_[gdmgr._menu_lparam].intimate_[0].hero_ != u32n) {
			*changed = TRUE;
			gdmgr.heros_[gdmgr._menu_lparam].intimate_[0].hero_ = u32n;
		}
	}

	// intimate[1]
	idx = ComboBox_GetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_INTIMATE1));
	if (idx >= 0) {
		u32n = ComboBox_GetItemData(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_INTIMATE1), idx);
		if (gdmgr.heros_[gdmgr._menu_lparam].intimate_[1].hero_ != u32n) {
			*changed = TRUE;
			gdmgr.heros_[gdmgr._menu_lparam].intimate_[1].hero_ = u32n;
		}
	}

	// intimate[2]
	idx = ComboBox_GetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_INTIMATE2));
	if (idx >= 0) {
		u32n = ComboBox_GetItemData(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_INTIMATE2), idx);
		if (gdmgr.heros_[gdmgr._menu_lparam].intimate_[2].hero_ != u32n) {
			*changed = TRUE;
			gdmgr.heros_[gdmgr._menu_lparam].intimate_[2].hero_ = u32n;
		}
	}

	// hate[0]
	idx = ComboBox_GetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_HATE0));
	if (idx >= 0) {
		u32n = ComboBox_GetItemData(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_HATE0), idx);
		if (gdmgr.heros_[gdmgr._menu_lparam].hate_[0].hero_ != u32n) {
			*changed = TRUE;
			gdmgr.heros_[gdmgr._menu_lparam].hate_[0].hero_ = u32n;
		}
	}

	// hate[1]
	idx = ComboBox_GetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_HATE1));
	if (idx >= 0) {
		u32n = ComboBox_GetItemData(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_HATE1), idx);
		if (gdmgr.heros_[gdmgr._menu_lparam].hate_[1].hero_ != u32n) {
			*changed = TRUE;
			gdmgr.heros_[gdmgr._menu_lparam].hate_[1].hero_ = u32n;
		}
	}

	// hate[2]
	idx = ComboBox_GetCurSel(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_HATE2));
	if (idx >= 0) {
		u32n = ComboBox_GetItemData(GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_HATE2), idx);
		if (gdmgr.heros_[gdmgr._menu_lparam].hate_[2].hero_ != u32n) {
			*changed = TRUE;
			gdmgr.heros_[gdmgr._menu_lparam].hate_[2].hero_ = u32n;
		}
	}

	return TRUE;
}

void OnOathCmb(HWND hdlgP, HWND hwndCtrl, UINT codeNotify, hero& h)
{
	if (codeNotify != CBN_SELCHANGE) {
		return;
	}

	HWND hctlOath0 = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_OATH0);
	HWND hctlOath1 = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_OATH1);
	uint16_t selected_hero0, selected_hero1;
	hero_map::iterator itor;
	char text[_MAX_PATH];
	int idx;

	idx = ComboBox_GetCurSel(hctlOath0);
	if (idx >= 0) {
		selected_hero0 = (uint16_t)ComboBox_GetItemData(hctlOath0, idx);
	} else {
		selected_hero0 = HEROS_INVALID_NUMBER;
	}
	idx = ComboBox_GetCurSel(hctlOath1);
	if (idx >= 0) {
		selected_hero1 = (uint16_t)ComboBox_GetItemData(hctlOath1, idx);
	} else {
		selected_hero1 = HEROS_INVALID_NUMBER;
	}

	// oath[0]
	ComboBox_ResetContent(hctlOath0);
	ComboBox_AddString(hctlOath0, "");
	ComboBox_SetItemData(hctlOath0, 0, HEROS_INVALID_NUMBER);
	ComboBox_SetCurSel(hctlOath0, 0);
	for (idx = 1, itor = gdmgr.heros_.begin(); itor != gdmgr.heros_.end(); ++ itor) {
		if ((h.gender_ == itor->gender_) && (h.number_ != itor->number_) && (selected_hero1 != itor->number_)) {
			sprintf(text, "%04u: %s", itor->number_, itor->name().c_str());
			ComboBox_AddString(hctlOath0, text); 
			ComboBox_SetItemData(hctlOath0, idx, itor->number_);

			// don't use h.oath_[0].hero_
			if (selected_hero0 == itor->number_) {
				ComboBox_SetCurSel(hctlOath0, idx); 
			}
			idx ++;
		}
	}

	// oath[1]
	ComboBox_ResetContent(hctlOath1);
	ComboBox_AddString(hctlOath1, ""); 
	ComboBox_SetItemData(hctlOath1, 0, HEROS_INVALID_NUMBER);
	ComboBox_SetCurSel(hctlOath1, 0);
	for (idx = 1, itor = gdmgr.heros_.begin(); itor != gdmgr.heros_.end(); ++ itor) {
		if ((h.gender_ == itor->gender_) && (h.number_ != itor->number_) && (selected_hero0 != itor->number_)) {
			sprintf(text, "%04u: %s", itor->number_, itor->name().c_str());
			ComboBox_AddString(hctlOath1, text); 
			ComboBox_SetItemData(hctlOath1, idx, itor->number_);

			if (selected_hero1 == itor->number_) {
				ComboBox_SetCurSel(hctlOath1, idx); 
			}
			idx ++;
		}
	}

	return;
}

void On3HeroCmb(HWND hdlgP, UINT codeNotify, hero& h, bool intimate)
{
	if (codeNotify != CBN_SELCHANGE) {
		return;
	}

	HWND hctl0, hctl1, hctl2;

	if (intimate) {
		hctl0 = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_INTIMATE0);
		hctl1 = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_INTIMATE1);
		hctl2 = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_INTIMATE2);
	} else {
		hctl0 = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_HATE0);
		hctl1 = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_HATE1);
		hctl2 = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_HATE2);
	}
	uint16_t selected_hero0, selected_hero1, selected_hero2;
	hero_map::iterator itor;
	char text[_MAX_PATH];
	int idx;

	idx = ComboBox_GetCurSel(hctl0);
	if (idx >= 0) {
		selected_hero0 = (uint16_t)ComboBox_GetItemData(hctl0, idx);
	} else {
		selected_hero0 = HEROS_INVALID_NUMBER;
	}
	idx = ComboBox_GetCurSel(hctl1);
	if (idx >= 0) {
		selected_hero1 = (uint16_t)ComboBox_GetItemData(hctl1, idx);
	} else {
		selected_hero1 = HEROS_INVALID_NUMBER;
	}
	idx = ComboBox_GetCurSel(hctl2);
	if (idx >= 0) {
		selected_hero2 = (uint16_t)ComboBox_GetItemData(hctl2, idx);
	} else {
		selected_hero2 = HEROS_INVALID_NUMBER;
	}

	// hate[0]
	ComboBox_ResetContent(hctl0);
	ComboBox_AddString(hctl0, "");
	ComboBox_SetItemData(hctl0, 0, HEROS_INVALID_NUMBER);
	ComboBox_SetCurSel(hctl0, 0);
	for (idx = 1, itor = gdmgr.heros_.begin(); itor != gdmgr.heros_.end(); ++ itor) {
		if ((h.number_ != itor->number_) && (selected_hero1 != itor->number_) && (selected_hero2 != itor->number_)) {
			sprintf(text, "%04u: %s", itor->number_, itor->name().c_str());
			ComboBox_AddString(hctl0, text); 
			ComboBox_SetItemData(hctl0, idx, itor->number_);

			// don't use h.hate_[0].hero_
			if (selected_hero0 == itor->number_) {
				ComboBox_SetCurSel(hctl0, idx); 
			}
			idx ++;
		}
	}

	// hate[1]
	ComboBox_ResetContent(hctl1);
	ComboBox_AddString(hctl1, "");
	ComboBox_SetItemData(hctl1, 0, HEROS_INVALID_NUMBER);
	ComboBox_SetCurSel(hctl1, 0);
	for (idx = 1, itor = gdmgr.heros_.begin(); itor != gdmgr.heros_.end(); ++ itor) {
		if ((h.number_ != itor->number_) && (selected_hero0 != itor->number_) && (selected_hero2 != itor->number_)) {
			sprintf(text, "%04u: %s", itor->number_, itor->name().c_str());
			ComboBox_AddString(hctl1, text); 
			ComboBox_SetItemData(hctl1, idx, itor->number_);

			// don't use h.hate_[1].hero_
			if (selected_hero1 == itor->number_) {
				ComboBox_SetCurSel(hctl1, idx); 
			}
			idx ++;
		}
	}

	// hate[2]
	ComboBox_ResetContent(hctl2);
	ComboBox_AddString(hctl2, "");
	ComboBox_SetItemData(hctl2, 0, HEROS_INVALID_NUMBER);
	ComboBox_SetCurSel(hctl2, 0);
	for (idx = 1, itor = gdmgr.heros_.begin(); itor != gdmgr.heros_.end(); ++ itor) {
		if ((h.number_ != itor->number_) && (selected_hero0 != itor->number_) && (selected_hero1 != itor->number_)) {
			sprintf(text, "%04u: %s", itor->number_, itor->name().c_str());
			ComboBox_AddString(hctl2, text);
			ComboBox_SetItemData(hctl2, idx, itor->number_);

			// don't use h.Intimate_[2].hero_
			if (selected_hero2 == itor->number_) {
				ComboBox_SetCurSel(hctl2, idx); 
			}
			idx ++;
		}
	}

	return;
}

void refresh_hero_name_list(HWND hdlgP, hero& h)
{
	HWND hctl;
	hero_map::iterator itor;
	char text[_MAX_PATH];
	int idx;

	// consort
	hctl = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_CONSORT);
	ComboBox_AddString(hctl, ""); 
	ComboBox_SetItemData(hctl, 0, HEROS_INVALID_NUMBER);
	for (idx = 1, itor = gdmgr.heros_.begin(); itor != gdmgr.heros_.end(); ++ itor) {
		if (h.gender_ != itor->gender_) {
			sprintf(text, "%04u: %s", itor->number_, itor->name().c_str());
			ComboBox_AddString(hctl, text); 
			ComboBox_SetItemData(hctl, idx, itor->number_);

			if (h.consort_[0].hero_ == itor->number_) {
				ComboBox_SetCurSel(hctl, idx); 
			}
			idx ++;
		}
	}

	// parent0
	hctl = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_PARENT0);
	ComboBox_AddString(hctl, ""); 
	ComboBox_SetItemData(hctl, 0, HEROS_INVALID_NUMBER);
	for (idx = 1, itor = gdmgr.heros_.begin(); itor != gdmgr.heros_.end(); ++ itor) {
		if ((itor->gender_ == hero_gender_male) && (h.number_ != itor->number_)) {
			sprintf(text, "%04u: %s", itor->number_, itor->name().c_str());
			ComboBox_AddString(hctl, text); 
			ComboBox_SetItemData(hctl, idx, itor->number_);

			if (h.parent_[0].hero_ == itor->number_) {
				ComboBox_SetCurSel(hctl, idx); 
			}
			idx ++;
		}
	}

	// parent1
	hctl = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_PARENT1);
	ComboBox_AddString(hctl, "");
	ComboBox_SetItemData(hctl, 0, HEROS_INVALID_NUMBER);
	for (idx = 1, itor = gdmgr.heros_.begin(); itor != gdmgr.heros_.end(); ++ itor) {
		if ((itor->gender_ == hero_gender_female) && (h.number_ != itor->number_)) {
			sprintf(text, "%04u: %s", itor->number_, itor->name().c_str());
			ComboBox_AddString(hctl, text); 
			ComboBox_SetItemData(hctl, idx, itor->number_);

			if (h.parent_[1].hero_ == itor->number_) {
				ComboBox_SetCurSel(hctl, idx); 
			}
			idx ++;
		}
	}

	// oath0
	hctl = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_OATH0);
	ComboBox_AddString(hctl, ""); 
	ComboBox_SetItemData(hctl, 0, HEROS_INVALID_NUMBER);
	for (idx = 1, itor = gdmgr.heros_.begin(); itor != gdmgr.heros_.end(); ++ itor) {
		if ((h.gender_ == itor->gender_) && (h.number_ != itor->number_) && (h.oath_[1].hero_ != itor->number_)) {
			sprintf(text, "%04u: %s", itor->number_, itor->name().c_str());
			ComboBox_AddString(hctl, text); 
			ComboBox_SetItemData(hctl, idx, itor->number_);

			if (h.oath_[0].hero_ == itor->number_) {
				ComboBox_SetCurSel(hctl, idx); 
			}
			idx ++;
		}
	}

	// oath1
	hctl = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_OATH1);
	ComboBox_AddString(hctl, ""); 
	ComboBox_SetItemData(hctl, 0, HEROS_INVALID_NUMBER);
	for (idx = 1, itor = gdmgr.heros_.begin(); itor != gdmgr.heros_.end(); ++ itor) {
		if ((h.gender_ == itor->gender_) && (h.number_ != itor->number_) && (h.oath_[0].hero_ != itor->number_)) {
			sprintf(text, "%04u: %s", itor->number_, itor->name().c_str());
			ComboBox_AddString(hctl, text); 
			ComboBox_SetItemData(hctl, idx, itor->number_);

			if (h.oath_[1].hero_ == itor->number_) {
				ComboBox_SetCurSel(hctl, idx); 
			}
			idx ++;
		}
	}

	// intimate0
	hctl = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_INTIMATE0);
	ComboBox_AddString(hctl, ""); 
	ComboBox_SetItemData(hctl, 0, HEROS_INVALID_NUMBER);
	for (idx = 1, itor = gdmgr.heros_.begin(); itor != gdmgr.heros_.end(); ++ itor) {
		if ((h.number_ != itor->number_) && (h.intimate_[1].hero_ != itor->number_) && (h.intimate_[2].hero_ != itor->number_)) {
			sprintf(text, "%04u: %s", itor->number_, itor->name().c_str());
			ComboBox_AddString(hctl, text); 
			ComboBox_SetItemData(hctl, idx, itor->number_);

			if (h.intimate_[0].hero_ == itor->number_) {
				ComboBox_SetCurSel(hctl, idx); 
			}
			idx ++;
		}
	}

	// intimate1
	hctl = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_INTIMATE1);
	ComboBox_AddString(hctl, ""); 
	ComboBox_SetItemData(hctl, 0, HEROS_INVALID_NUMBER);
	for (idx = 1, itor = gdmgr.heros_.begin(); itor != gdmgr.heros_.end(); ++ itor) {
		if ((h.number_ != itor->number_) && (h.intimate_[0].hero_ != itor->number_) && (h.intimate_[2].hero_ != itor->number_)) {
			sprintf(text, "%04u: %s", itor->number_, itor->name().c_str());
			ComboBox_AddString(hctl, text); 
			ComboBox_SetItemData(hctl, idx, itor->number_);

			if (h.intimate_[1].hero_ == itor->number_) {
				ComboBox_SetCurSel(hctl, idx); 
			}
			idx ++;
		}
	}

	// intimate2
	hctl = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_INTIMATE2);
	ComboBox_AddString(hctl, ""); 
	ComboBox_SetItemData(hctl, 0, HEROS_INVALID_NUMBER);
	for (idx = 1, itor = gdmgr.heros_.begin(); itor != gdmgr.heros_.end(); ++ itor) {
		if ((h.number_ != itor->number_) && (h.intimate_[0].hero_ != itor->number_) && (h.intimate_[1].hero_ != itor->number_)) {
			sprintf(text, "%04u: %s", itor->number_, itor->name().c_str());
			ComboBox_AddString(hctl, text); 
			ComboBox_SetItemData(hctl, idx, itor->number_);

			if (h.intimate_[2].hero_ == itor->number_) {
				ComboBox_SetCurSel(hctl, idx); 
			}
			idx ++;
		}
	}

	// hate0
	hctl = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_HATE0);
	ComboBox_AddString(hctl, ""); 
	ComboBox_SetItemData(hctl, 0, HEROS_INVALID_NUMBER);
	for (idx = 1, itor = gdmgr.heros_.begin(); itor != gdmgr.heros_.end(); ++ itor) {
		if ((h.number_ != itor->number_) && (h.hate_[1].hero_ != itor->number_) && (h.hate_[2].hero_ != itor->number_)) {
			sprintf(text, "%04u: %s", itor->number_, itor->name().c_str());
			ComboBox_AddString(hctl, text); 
			ComboBox_SetItemData(hctl, idx, itor->number_);

			if (h.hate_[0].hero_ == itor->number_) {
				ComboBox_SetCurSel(hctl, idx); 
			}
			idx ++;
		}
	}

	// hate1
	hctl = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_HATE1);
	ComboBox_AddString(hctl, "");
	ComboBox_SetItemData(hctl, 0, HEROS_INVALID_NUMBER);
	for (idx = 1, itor = gdmgr.heros_.begin(); itor != gdmgr.heros_.end(); ++ itor) {
		if ((h.number_ != itor->number_) && (h.hate_[0].hero_ != itor->number_) && (h.hate_[2].hero_ != itor->number_)) {
			sprintf(text, "%04u: %s", itor->number_, itor->name().c_str());
			ComboBox_AddString(hctl, text); 
			ComboBox_SetItemData(hctl, idx, itor->number_);

			if (h.hate_[1].hero_ == itor->number_) {
				ComboBox_SetCurSel(hctl, idx); 
			}
			idx ++;
		}
	}

	// hate2
	hctl = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_HATE2);
	ComboBox_AddString(hctl, ""); 
	ComboBox_SetItemData(hctl, 0, HEROS_INVALID_NUMBER);
	for (idx = 1, itor = gdmgr.heros_.begin(); itor != gdmgr.heros_.end(); ++ itor) {
		if ((h.number_ != itor->number_) && (h.hate_[0].hero_ != itor->number_) && (h.hate_[1].hero_ != itor->number_)) {
			sprintf(text, "%04u: %s", itor->number_, itor->name().c_str());
			ComboBox_AddString(hctl, text); 
			ComboBox_SetItemData(hctl, idx, itor->number_);

			if (h.hate_[2].hero_ == itor->number_) {
				ComboBox_SetCurSel(hctl, idx); 
			}
			idx ++;
		}
	}
}

// LSBIT_SGLFILECAP ==> LSBIT_SGLFILECAP
// 定义了LSBIT_SGLFILECAP，则采用单文件循环录像
// 定义了LSBIT_CIRCULARCAP，而没定义LSBIT_SGLFILECAP，则采用单文件循环录像
// 同时定义LSBIT_CIRCULARCAP和LSBIT_SGLFILECAP，优先采用单文件循环录像
// 既没定义LSBIT_CIRCULARCAP，也没定义LSBIT_SGLFILECAP，普通方式录像
BOOL On_DlgHeroEditInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	HWND hctl;
	char text[_MAX_PATH], *ptr;

	move_subcfg_right_position(hdlgP, lParam);
	create_subcfg_toolbar(hdlgP);

	// gender
	hctl = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_GENDER);
	sprintf(text, "%s0", HERO_PREFIX_STR_GENDER);
	ComboBox_AddString(hctl, dgettext("wesnoth-hero", text)); 
	sprintf(text, "%s1", HERO_PREFIX_STR_GENDER);
	ComboBox_AddString(hctl, dgettext("wesnoth-hero", text)); 

	// ambition
	hctl = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_AMBITION);
	for (int idx = 0; idx <= hero_ambition_max; idx ++) {
		sprintf(text, "%s%i", HERO_PREFIX_STR_AMBITION, idx);
		ComboBox_AddString(hctl, dgettext("wesnoth-hero", text)); 
	}

	// feature
	hctl = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_FEATURE);
	ComboBox_AddString(hctl, "");
	ComboBox_SetItemData(hctl, 0, HEROS_MAX_FEATURE);

	std::vector<int> features = hero::valid_features();
	for (std::vector<int>::const_iterator itor = features.begin(); itor != features.end(); ++ itor) {
		sprintf(text, "%s%i", HERO_PREFIX_STR_FEATURE, *itor);
		ComboBox_AddString(hctl, dgettext("wesnoth-hero", text)); 
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, *itor);
	}

	// side feature
	hctl = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_SIDEFEATURE);
	ComboBox_AddString(hctl, "");
	ComboBox_SetItemData(hctl, 0, HEROS_NO_SIDE_FEATURE);
	for (int idx = 0; idx <= hero_feature_max; idx ++) {
		sprintf(text, "%s%i", HERO_PREFIX_STR_FEATURE, idx);
		ptr = dgettext("wesnoth-hero", text);
		if (strncmp(ptr, HERO_PREFIX_STR_FEATURE, strlen(HERO_PREFIX_STR_FEATURE))) {
			ComboBox_AddString(hctl, dgettext("wesnoth-hero", text)); 
			ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, idx);
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

	// leadership
	hctl = GetDlgItem(hdlgP, IDC_UD_HEROEDIT_LEADERSHIP);
	UpDown_SetRange(hctl, 1, 100);	// [1, 100]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_HEROEDIT_LEADERSHIP));
	hctl = GetDlgItem(hdlgP, IDC_UD_HEROEDIT_LEADERSHIPXP);
	UpDown_SetRange(hctl, 0, 511);	// [0, 511]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_HEROEDIT_LEADERSHIPXP));

	// force
	hctl = GetDlgItem(hdlgP, IDC_UD_HEROEDIT_FORCE);
	UpDown_SetRange(hctl, 1, 100);	// [1, 100]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_HEROEDIT_FORCE));
	hctl = GetDlgItem(hdlgP, IDC_UD_HEROEDIT_FORCEXP);
	UpDown_SetRange(hctl, 0, 511);	// [0, 511]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_HEROEDIT_FORCEXP));

	// intellect
	hctl = GetDlgItem(hdlgP, IDC_UD_HEROEDIT_INTELLECT);
	UpDown_SetRange(hctl, 1, 100);	// [1, 100]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_HEROEDIT_INTELLECT));
	hctl = GetDlgItem(hdlgP, IDC_UD_HEROEDIT_INTELLECTXP);
	UpDown_SetRange(hctl, 0, 511);	// [0, 511]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_HEROEDIT_INTELLECTXP));

	// politics
	hctl = GetDlgItem(hdlgP, IDC_UD_HEROEDIT_POLITICS);
	UpDown_SetRange(hctl, 1, 100);	// [1, 100]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_HEROEDIT_POLITICS));
	hctl = GetDlgItem(hdlgP, IDC_UD_HEROEDIT_POLITICSXP);
	UpDown_SetRange(hctl, 0, 511);	// [0, 511]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_HEROEDIT_POLITICSXP));

	// charm
	hctl = GetDlgItem(hdlgP, IDC_UD_HEROEDIT_CHARM);
	UpDown_SetRange(hctl, 1, 100);	// [1, 100]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_HEROEDIT_CHARM));
	hctl = GetDlgItem(hdlgP, IDC_UD_HEROEDIT_CHARMXP);
	UpDown_SetRange(hctl, 0, 511);	// [0, 511]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_HEROEDIT_CHARMXP));

	// adaptability-arms0
	hctl = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_ARMS0);
	for (int idx = 0; idx <= hero_adaptability_max; idx ++) {
		sprintf(text, "%s%i", HERO_PREFIX_STR_ADAPTABILITY, idx);
		ComboBox_AddString(hctl, dgettext("wesnoth-hero", text)); 
	}
	hctl = GetDlgItem(hdlgP, IDC_UD_HEROEDIT_ARMS0XP);
	UpDown_SetRange(hctl, 0, 4095);	// [0, 4095]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_HEROEDIT_ARMS0XP));

	// adaptability-arms1
	hctl = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_ARMS1);
	for (int idx = 0; idx <= hero_adaptability_max; idx ++) {
		sprintf(text, "%s%i", HERO_PREFIX_STR_ADAPTABILITY, idx);
		ComboBox_AddString(hctl, dgettext("wesnoth-hero", text)); 
	}
	hctl = GetDlgItem(hdlgP, IDC_UD_HEROEDIT_ARMS1XP);
	UpDown_SetRange(hctl, 0, 4095);	// [0, 4095]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_HEROEDIT_ARMS1XP));

	// adaptability-arms2
	hctl = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_ARMS2);
	for (int idx = 0; idx <= hero_adaptability_max; idx ++) {
		sprintf(text, "%s%i", HERO_PREFIX_STR_ADAPTABILITY, idx);
		ComboBox_AddString(hctl, dgettext("wesnoth-hero", text)); 
	}
	hctl = GetDlgItem(hdlgP, IDC_UD_HEROEDIT_ARMS2XP);
	UpDown_SetRange(hctl, 0, 4095);	// [0, 4095]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_HEROEDIT_ARMS2XP));

	// adaptability-arms3
	hctl = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_ARMS3);
	for (int idx = 0; idx <= hero_adaptability_max; idx ++) {
		sprintf(text, "%s%i", HERO_PREFIX_STR_ADAPTABILITY, idx);
		ComboBox_AddString(hctl, dgettext("wesnoth-hero", text)); 
	}
	hctl = GetDlgItem(hdlgP, IDC_UD_HEROEDIT_ARMS3XP);
	UpDown_SetRange(hctl, 0, 4095);	// [0, 4095]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_HEROEDIT_ARMS3XP));

	// adaptability-arms4
	hctl = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_ARMS4);
	for (int idx = 0; idx <= hero_adaptability_max; idx ++) {
		sprintf(text, "%s%i", HERO_PREFIX_STR_ADAPTABILITY, idx);
		ComboBox_AddString(hctl, dgettext("wesnoth-hero", text)); 
	}
	hctl = GetDlgItem(hdlgP, IDC_UD_HEROEDIT_ARMS4XP);
	UpDown_SetRange(hctl, 0, 4095);	// [0, 4095]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_HEROEDIT_ARMS4XP));

	// adaptability-skill_commercial
	hctl = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_SKILL0);
	for (int idx = 0; idx <= hero_adaptability_max; idx ++) {
		sprintf(text, "%s%i", HERO_PREFIX_STR_ADAPTABILITY, idx);
		ComboBox_AddString(hctl, dgettext("wesnoth-hero", text)); 
	}
	hctl = GetDlgItem(hdlgP, IDC_UD_HEROEDIT_SKILL0XP);
	UpDown_SetRange(hctl, 0, 4095);	// [0, 4095]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_HEROEDIT_SKILL0XP));

	// adaptability-skill_hero
	hctl = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_SKILL1);
	for (int idx = 0; idx <= hero_adaptability_max; idx ++) {
		sprintf(text, "%s%i", HERO_PREFIX_STR_ADAPTABILITY, idx);
		ComboBox_AddString(hctl, dgettext("wesnoth-hero", text)); 
	}
	hctl = GetDlgItem(hdlgP, IDC_UD_HEROEDIT_SKILL1XP);
	UpDown_SetRange(hctl, 0, 4095);	// [0, 4095]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_HEROEDIT_SKILL1XP));

	// adaptability-skill_demolish
	hctl = GetDlgItem(hdlgP, IDC_CMB_HEROEDIT_SKILL2);
	for (int idx = 0; idx <= hero_adaptability_max; idx ++) {
		sprintf(text, "%s%i", HERO_PREFIX_STR_ADAPTABILITY, idx);
		ComboBox_AddString(hctl, dgettext("wesnoth-hero", text)); 
	}
	hctl = GetDlgItem(hdlgP, IDC_UD_HEROEDIT_SKILL2XP);
	UpDown_SetRange(hctl, 0, 4095);	// [0, 4095]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_HEROEDIT_SKILL2XP));

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
	BOOL		changed = FALSE;

	switch (id) {
	case IDC_CMB_HEROEDIT_OATH0:
	case IDC_CMB_HEROEDIT_OATH1:
		OnOathCmb(hdlgP, hwndCtrl, codeNotify, gdmgr.heros_[gdmgr._menu_lparam]);
		break;
	case IDC_CMB_HEROEDIT_INTIMATE0:
	case IDC_CMB_HEROEDIT_INTIMATE1:
	case IDC_CMB_HEROEDIT_INTIMATE2:
		On3HeroCmb(hdlgP, codeNotify, gdmgr.heros_[gdmgr._menu_lparam], true);
		break;
	case IDC_CMB_HEROEDIT_HATE0:
	case IDC_CMB_HEROEDIT_HATE1:
	case IDC_CMB_HEROEDIT_HATE2:
		On3HeroCmb(hdlgP, codeNotify, gdmgr.heros_[gdmgr._menu_lparam], false);
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

BOOL CALLBACK DlgHeroEditProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_INITDIALOG:
		return On_DlgHeroEditInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgHeroEditCommand);
	HANDLE_MSG(hdlgP, WM_HSCROLL, On_DlgHeroEditHScroll);
	HANDLE_MSG(hdlgP, WM_DRAWITEM, On_DlgPopupDrawItem);
	HANDLE_MSG(hdlgP, WM_NOTIFY, On_DlgPopupNotify);
	}
	
	return FALSE;
}
