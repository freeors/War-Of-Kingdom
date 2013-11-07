#define GETTEXT_DOMAIN "wesnoth-maker"

#include "global.hpp"
#include "game_config.hpp"
#include "loadscreen.hpp"
#include "DlgCoreProc.hpp"
#include "gettext.hpp"
#include "serialization/binary_or_text.hpp"
#include "stdafx.h"
#include <windowsx.h>
#include <string.h>

#include "resource.h"

#include "xfunc.h"
#include "win32x.h"
#include "struct.h"

#include <boost/foreach.hpp>

namespace ns {
	// int clicked_terrain;
	
	// int action_terrain;
}

void tconfig::from_config(const config& cfg)
{
	bbs_server_["host"] = game_config::bbs_server.host;
	bbs_server_["port"] = game_config::bbs_server.port;
	bbs_server_["url"] = game_config::bbs_server.url;

	// remember side, will use to compare in future.
	dirty_ = 0;
	config_from_cfg_ = *this;
}

void tconfig::from_ui_config_edit(HWND hdlgP)
{
/*
	char text[_MAX_PATH];
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_TERRAINEDIT_STRING), text, sizeof(text) / sizeof(text[0]));
	string_ = text;
*/
}

void tconfig::update_to_ui_config_edit(HWND hdlgP) const
{
/*
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_TERRAINEDIT_STRING), string_.c_str());
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_TERRAINEDIT_ID), id_.c_str());
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_TERRAINEDIT_NAME_MSGID), name_.c_str());
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_TERRAINEDIT_ALIAS), aliasof_.c_str());
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_TERRAINEDIT_MVTALIAS), mvt_alias_.c_str());
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_TERRAINEDIT_DEFALIAS), def_alias_.c_str());
*/
}

std::string tconfig::generate() const
{
	std::stringstream strstr;

	strstr << "[bbs_server]\n";
	strstr << "\tname = _\"" << bbs_server_["host"].str() << "\"\n";
	strstr << "\taddress = " << bbs_server_["host"].str() << ":" << bbs_server_["port"].to_int() << "\n";
	strstr << "\turl = " << bbs_server_["url"].str() << "\n";
	strstr << "[/bbs_server]\n";
	strstr << "\n";

	return strstr.str();
}

void tcore::update_to_ui_config(HWND hdlgP)
{
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_CONFIG_DOMAIN), config_updating_.textdomain_.c_str());

	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_CONFIG_NAME), 
		dgettext_2_ansi("wesnoth", config_updating_.bbs_server_["host"].str().c_str()));
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_CONFIG_HOST), config_updating_.bbs_server_["host"].str().c_str());
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_CONFIG_PORT), lexical_cast_default<std::string>(config_updating_.bbs_server_["port"].to_int()).c_str());
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_CONFIG_URL), config_updating_.bbs_server_["url"].str().c_str());
}

void tconfig::set_dirty(int bit, bool set)
{
	if (set) {
		dirty_ |= 1 << bit;
	} else {
		dirty_ &= ~(1 << bit);
	}
	ns::core.set_dirty(tcore::BIT_CONFIG, dirty_);
}

bool tconfig::dirty() const
{
	if (*this != config_from_cfg_) {
		return true;
	}
	return false;
}

BOOL On_DlgConfigInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	HWND hwndParent = GetParent(hdlgP); 
    DLGHDR *pHdr = (DLGHDR *) GetWindowLong(hwndParent, GWL_USERDATA);
    SetWindowPos(hdlgP, HWND_TOP, pHdr->rcDisplay.left, pHdr->rcDisplay.top, 0, 0, SWP_NOSIZE);

	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_DOMAIN), utf8_2_ansi(_("Domain")));

	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_BBSSERVER), utf8_2_ansi(_("BBS Server")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_NAME), dgettext_2_ansi("wesnoth-lib", "object^Name"));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_HOST), utf8_2_ansi(_("Host")));

	HWND hctl = GetDlgItem(hdlgP, IDC_UD_CONFIG_PORT);
	UpDown_SetRange(hctl, 0, 10000);
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_CONFIG_PORT));

	return FALSE;
}

BOOL On_DlgConfigEditInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);
/*
	std::stringstream strstr;
	if (ns::action_terrain == ma_edit) {
		strstr << _("Edit terrain");
	} else {
		strstr << _("Add terrain");
	}
	strstr << "(" << _("Number") << ":" << ns::clicked_terrain << ")";
	SetWindowText(hdlgP, utf8_2_ansi(strstr.str().c_str()));

	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_STRING), utf8_2_ansi(_("terrain^String")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_ID), utf8_2_ansi(_("terrain^ID")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_NAME), dgettext_2_ansi("wesnoth-lib", "object^Name"));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_ALIAS), utf8_2_ansi(_("Alias")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_MVTALIAS), utf8_2_ansi(_("Alias(mvt)")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_DEFALIAS), utf8_2_ansi(_("Alias(def)")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_HEIGHT), utf8_2_ansi(_("Height")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_SUBMERGE), utf8_2_ansi(_("Submerge")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_LIGHT), utf8_2_ansi(_("terrain^Light")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_HEALS), utf8_2_ansi(_("Heals")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_GROUP), utf8_2_ansi(_("Group")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_DEFAULTBASE), utf8_2_ansi(_("Default base")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_SYMBOLIMAGE), utf8_2_ansi(_("Symbol image")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_EDITORIMAGE), utf8_2_ansi(_("Editor image")));

	tterrain& t = ns::core.terrains_updating_[ns::clicked_terrain];
	t.edit_invalid_ = 0;
	t.update_to_ui_terrain_edit(hdlgP);
*/
	return FALSE;
}

void OnConfigEditEt(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
/*
	char text[_MAX_PATH];
	HWND boddy;
	std::stringstream strstr;
	int bit = tterrain::EDITBIT_NONE;
	bool bit_invalid = false;

	if (codeNotify != EN_CHANGE) {
		return;
	}

	tterrain& terrain = ns::core.terrains_updating_[ns::clicked_terrain];

	Edit_GetText(hwndCtrl, text, sizeof(text) / sizeof(text[0]));
	strstr.str("");
	if (id == IDC_ET_TERRAINEDIT_STRING) {
		bit_invalid = !is_valid_terrain_str(text);
		if (bit_invalid) {
			strstr << _("Invalid string");
		} else {
			bit_invalid = !is_unique_string(ns::core.terrains_updating_, text, ns::clicked_terrain);
			if (bit_invalid) {
				strstr << _("Other has holden it");
			}
		}
		boddy = GetDlgItem(hdlgP, IDC_ET_TERRAINEDIT_STRINGSTATUS);
		bit = tterrain::EDITBIT_STRING;
	} else if (id == IDC_ET_TERRAINEDIT_NAME_MSGID) {
		if (text[0]) {
			strstr << dsgettext("wesnoth-lib", text);
		}
		boddy = GetDlgItem(hdlgP, IDC_ET_TERRAINEDIT_NAME);
	} else if (id == IDC_ET_TERRAINEDIT_ALIAS || id == IDC_ET_TERRAINEDIT_MVTALIAS || id == IDC_ET_TERRAINEDIT_DEFALIAS) {
		bit_invalid = !is_valid_alias_str(text);
		if (bit_invalid) {
			strstr << _("Invalid string");
		}
		if (id == IDC_ET_TERRAINEDIT_ALIAS) {
			boddy = GetDlgItem(hdlgP, IDC_ET_TERRAINEDIT_ALIASSTATUS);
		} else if (id == IDC_ET_TERRAINEDIT_MVTALIAS) {
			boddy = GetDlgItem(hdlgP, IDC_ET_TERRAINEDIT_MVTALIASSTATUS);
		} else {
			boddy = GetDlgItem(hdlgP, IDC_ET_TERRAINEDIT_DEFALIASSTATUS);
		}
		bit = tterrain::EDITBIT_ALIAS;
	}
	
	if (!strstr.str().empty()) {
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	} else {
		text[0] = '\0';
	}
	Edit_SetText(boddy, text);
	if (bit != tterrain::EDITBIT_NONE) {
		terrain.set_edit_invalid(bit, bit_invalid, GetDlgItem(hdlgP, IDOK));
	}
*/
	return;
}

void On_DlgConfigEditCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;

	switch (id) {
/*
	case IDC_ET_TERRAINEDIT_STRING:
	case IDC_ET_TERRAINEDIT_ID:
	case IDC_ET_TERRAINEDIT_NAME_MSGID:
	case IDC_ET_TERRAINEDIT_ALIAS:
	case IDC_ET_TERRAINEDIT_MVTALIAS:
	case IDC_ET_TERRAINEDIT_DEFALIAS:
		OnTerrainEditEt(hdlgP, id, hwndCtrl, codeNotify);
		break;
*/
	case IDOK:
		changed = TRUE;
		// ns::core.terrains_updating_[ns::clicked_terrain].from_ui_terrain_edit(hdlgP);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

BOOL CALLBACK DlgConfigEditProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message) {
	case WM_INITDIALOG:
		return On_DlgConfigEditInitDialog(hDlg, (HWND)(wParam), lParam);
	HANDLE_MSG(hDlg, WM_COMMAND, On_DlgConfigEditCommand);
	HANDLE_MSG(hDlg, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	}
	
	return FALSE;
}

void OnConfigAddBt(HWND hdlgP)
{
/*
	// default to first feature
	ns::core.treasures_updating_.insert(std::make_pair(ns::core.treasures_updating_.size(), 0));

	ns::core.update_to_ui_treasure(hdlgP);
	ns::core.set_dirty(tcore::BIT_TREASURE, ns::core.treasures_dirty()); 
*/
}

void OnConfigDelBt(HWND hdlgP)
{
/*
	// default to first feature
	std::map<int, int>::iterator it = ns::core.treasures_updating_.begin();
	std::advance(it, ns::core.treasures_updating_.size() - 1);
	ns::core.treasures_updating_.erase(it);

	ns::core.update_to_ui_treasure(hdlgP);
	ns::core.set_dirty(tcore::BIT_TREASURE, ns::core.treasures_dirty()); 
*/
}

void OnConfigEditBt(HWND hdlgP)
{
/*	RECT rcBtn;
	LPARAM lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_LV_TERRAIN_EXPLORER), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	ns::action_terrain = ma_edit;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_TERRAINEDIT), hdlgP, DlgTerrainEditProc, lParam)) {
		ns::core.update_to_ui_terrain(hdlgP);
		ns::core.set_dirty(tcore::BIT_TERRAIN, ns::core.terrains_updating_[ns::clicked_terrain].dirty());
	}
*/
	return;
}

void OnConfigEt(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	char text[_MAX_PATH];
	HWND boddy = NULL;
	std::stringstream strstr;
	std::string me, that;
	int bit;

	if (codeNotify != EN_CHANGE) {
		return;
	}

	tconfig& game_config = ns::core.config_updating_;

	Edit_GetText(hwndCtrl, text, sizeof(text) / sizeof(text[0]));
	me = text;
	if (id == IDC_ET_CONFIG_HOST) {
		std::transform(me.begin(), me.end(), me.begin(), std::tolower);
		bit = tconfig::BIT_BBSSERVER;
		that = game_config.config_from_cfg_.bbs_server_["host"].str();
		boddy = GetDlgItem(hdlgP, IDC_ET_CONFIG_NAME);

		game_config.bbs_server_["host"] = me;
	} else if (id == IDC_ET_CONFIG_PORT) {
		bit = tconfig::BIT_BBSSERVER;
		that = lexical_cast_default<std::string>(game_config.config_from_cfg_.bbs_server_["port"].to_int());

		game_config.bbs_server_["port"] = lexical_cast_default<int>(me);
	} else if (id == IDC_ET_CONFIG_URL) {
		std::transform(me.begin(), me.end(), me.begin(), std::tolower);
		bit = tconfig::BIT_BBSSERVER;
		that = game_config.config_from_cfg_.bbs_server_["url"].str();

		game_config.bbs_server_["url"] = me;
	} else {
		return;
	}

	strstr.str("");
	if (boddy) {
		if (!me.empty()) {
			strstr << utf8_2_ansi(dsgettext(game_config.textdomain_.c_str(), me.c_str()));
		}
		Edit_SetText(boddy, strstr.str().c_str());
	}
	
	game_config.set_dirty(bit, me != that);

	return;
}

void On_DlgConfigCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	switch (id) {
	case IDC_ET_CONFIG_HOST:
	case IDC_ET_CONFIG_PORT:
	case IDC_ET_CONFIG_URL:
		OnConfigEt(hdlgP, id, hwndCtrl, codeNotify);
		break;
	}
	return;
}

void config_notify_handler_rclick(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
/*
	LPNMTREEVIEW			lpnmitem;
	HTREEITEM				htvi;
	TVITEMEX				tvi;
	POINT					point;
	std::stringstream strstr;

	if (lpNMHdr->idFrom != IDC_TV_UTYPE_EXPLORER) {
		return;
	}

	lpnmitem = (LPNMTREEVIEW)lpNMHdr;

	// NM_RCLICK/NM_CLICK/NM_DBLCLK这些通知被发来后,其附代参数没法指定是哪个叶子句柄,
	// 需通过判断鼠标坐标来判断是哪个叶子被按下?
	// 1. GetCursorPos, 得到屏幕坐标系下的鼠标坐标
	// 2. TreeView_HitTest1(自写宏),由屏幕坐标系下的鼠标坐标返回指向的叶子句柄
	GetCursorPos(&point);	// 得到的是屏幕坐标
	TreeView_HitTest1(lpNMHdr->hwndFrom, point, htvi);
	
	// NM_表示对通用控件都通用,范围丛(0, 99)
	// TVN_表示只能TreeView通用,范围丛(400, 499)
	if (lpNMHdr->code == NM_RCLICK) {
		//
		// 右键单击: 弹出菜单
		//
		TreeView_GetItem1(lpNMHdr->hwndFrom, htvi, &tvi, TVIF_PARAM | TVIF_CHILDREN, NULL);

		HMENU hpopup_explorer = CreatePopupMenu();
		strstr.str("");
		strstr << utf8_2_ansi(_("Base information")) << "...";
		AppendMenu(hpopup_explorer, MF_STRING, IDM_UTYPE_GENERAL, strstr.str().c_str());
		strstr.str("");
		strstr << dgettext_2_ansi("wesnoth", "Resistance") << "...";
		AppendMenu(hpopup_explorer, MF_STRING, IDM_UTYPE_MTYPE, strstr.str().c_str());
		strstr.str("");
		strstr << utf8_2_ansi(_("Attack I")) << "...";
		AppendMenu(hpopup_explorer, MF_STRING, IDM_UTYPE_ATTACKI, strstr.str().c_str());
		strstr.str("");
		strstr << utf8_2_ansi(_("Attack II")) << "...";
		AppendMenu(hpopup_explorer, MF_STRING, IDM_UTYPE_ATTACKII, strstr.str().c_str());
		strstr.str("");
		strstr << utf8_2_ansi(_("Attack III")) << "...";
		AppendMenu(hpopup_explorer, MF_STRING, IDM_UTYPE_ATTACKIII, strstr.str().c_str());

		HMENU hpopup_utype = CreatePopupMenu();
		AppendMenu(hpopup_utype, MF_STRING, IDM_ADD, utf8_2_ansi(_("Add...")));
		AppendMenu(hpopup_utype, MF_STRING, IDM_EDIT, utf8_2_ansi(_("Edit...")));
		AppendMenu(hpopup_utype, MF_STRING, IDM_DELETE, utf8_2_ansi(_("Delete...")));
		AppendMenu(hpopup_utype, MF_SEPARATOR, 0, NULL);
		AppendMenu(hpopup_utype, MF_STRING, IDM_GENERATE_ITEM2, utf8_2_ansi(_("Regenerate all type's cfg and relatve image")));
		AppendMenu(hpopup_utype, MF_SEPARATOR, 0, NULL);
		AppendMenu(hpopup_utype, MF_POPUP, (UINT_PTR)(hpopup_explorer), utf8_2_ansi(_("Report format")));

		if (!htvi || htvi == ns::core.htvroot_utype_) {
			EnableMenuItem(hpopup_utype, IDM_EDIT, MF_BYCOMMAND | MF_GRAYED);
			EnableMenuItem(hpopup_utype, IDM_DELETE, MF_BYCOMMAND | MF_GRAYED);
		} else {
			const std::string& id = ns::core.tv_tree_[tvi.lParam].first;
			const unit_type* ut = unit_types.find(id);
			if (ut->packer()) {
				EnableMenuItem(hpopup_utype, IDM_DELETE, MF_BYCOMMAND | MF_GRAYED);
			}
		}
		if (core_get_save_btn()) {
			EnableMenuItem(hpopup_utype, IDM_GENERATE_ITEM2, MF_BYCOMMAND | MF_GRAYED);
			EnableMenuItem(hpopup_utype, (UINT_PTR)(hpopup_explorer), MF_BYCOMMAND | MF_GRAYED);
		}
		
		TrackPopupMenuEx(hpopup_utype, 0, 
			point.x, 
			point.y, 
			hdlgP, 
			NULL);

		DestroyMenu(hpopup_explorer);
		DestroyMenu(hpopup_utype);

		ns::clicked_utype = tvi.lParam;
	}
*/
}

void config_notify_handler_dblclk(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	LVITEM					lvi;
	LPNMITEMACTIVATE		lpnmitem;

	// NM_表示对通用控件都通用,范围丛(0, 99)
	// TVN_表示只能TreeView通用,范围丛(400, 499)
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
/*
	if (lpnmitem->iItem >= 0) {
		if (lpNMHdr->idFrom == IDC_LV_TERRAIN_EXPLORER) {
			ns::clicked_terrain = lpnmitem->iItem;
			OnConfigEditBt(hdlgP);
		}
	}
*/
    return;
}

BOOL On_DlgConfigNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	if (lpNMHdr->code == NM_RCLICK) {
		config_notify_handler_rclick(hdlgP, DlgItem, lpNMHdr);
	} else if (lpNMHdr->code == NM_DBLCLK) {
		config_notify_handler_dblclk(hdlgP, DlgItem, lpNMHdr);
	}
	return FALSE;
}

void On_DlgConfigDestroy(HWND hdlgP)
{
	return;
}

BOOL CALLBACK DlgConfigProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_INITDIALOG:
		return On_DlgConfigInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgConfigCommand);
	HANDLE_MSG(hdlgP, WM_NOTIFY,  On_DlgConfigNotify);
	HANDLE_MSG(hdlgP, WM_DESTROY, On_DlgConfigDestroy);
	}
	
	return FALSE;
}