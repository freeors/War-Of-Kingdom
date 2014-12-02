#ifndef _ROSE_EDITOR

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

#include "map.hpp"
#include <boost/foreach.hpp>

namespace ns {
	int clicked_noble;
	
	int action_noble;
}

void tnoble2::from_config(const tnoble& nbl)
{
	id_ = nbl.id();
	level_ = nbl.level();
	leader_ = nbl.leader();
	formation_ = nbl.formation();
	condition_ = nbl.condition();
	effect_ = nbl.effect();

	noble_from_cfg_ = *this;
}

void tnoble2::from_ui_noble_edit(HWND hdlgP)
{
	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_NOBLEEDIT_LEVEL);
	level_ = ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));

	leader_ = Button_GetCheck(GetDlgItem(hdlgP, IDC_CHK_NOBLEEDIT_LEADER));
	formation_ = Button_GetCheck(GetDlgItem(hdlgP, IDC_CHK_NOBLEEDIT_FORMATION));

	condition_.city = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_NOBLEEDIT_CITY));
	condition_.meritorious = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_NOBLEEDIT_MERITORIOUS));

	effect_.leadership = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_NOBLEEDIT_LEADERSHIP));
	effect_.force = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_NOBLEEDIT_FORCE));
	effect_.intellect = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_NOBLEEDIT_INTELLECT));
	effect_.spirit = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_NOBLEEDIT_POLITICS));
	effect_.charm = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_NOBLEEDIT_CHARM));
}

void tnoble2::update_to_ui_noble_edit(HWND hdlgP) const
{
	std::stringstream strstr;
	//id
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_NOBLEEDIT_ID), id_.c_str());
	// name
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_NOBLEEDIT_NAME_MSGID), tnoble::form_name_msgid(id_).c_str());
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_NOBLEEDIT_NAME), utf8_2_ansi(tnoble::form_name(id_).c_str()));

	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_NOBLEEDIT_LEVEL);
	ComboBox_ResetContent(hctl);
	for (int i = 0; i <= game_config::max_noble_level; i ++) {
		ComboBox_AddString(hctl, str_cast(i).c_str());
		ComboBox_SetItemData(hctl, i, i);
		if (level_ == i) {
			ComboBox_SetCurSel(hctl, i);
		}
	}

	Button_SetCheck(GetDlgItem(hdlgP, IDC_CHK_NOBLEEDIT_LEADER), leader_);
	Button_SetCheck(GetDlgItem(hdlgP, IDC_CHK_NOBLEEDIT_FORMATION), formation_);

	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_NOBLEEDIT_CITY), condition_.city);
	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_NOBLEEDIT_MERITORIOUS), condition_.meritorious);

	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_NOBLEEDIT_LEADERSHIP), effect_.leadership);
	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_NOBLEEDIT_FORCE), effect_.force);
	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_NOBLEEDIT_INTELLECT), effect_.intellect);
	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_NOBLEEDIT_POLITICS), effect_.spirit);
	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_NOBLEEDIT_CHARM), effect_.charm);
}

std::string tnoble2::generate(const std::string& prefix) const
{
	std::stringstream strstr;

	strstr << prefix << "[noble]\n";
	strstr << prefix << "\tid = " << id_ << "\n";
	strstr << prefix << "\tlevel = " << level_ << "\n";
	strstr << prefix << "\tleader = " << (leader_? "yes": "no") << "\n";
	if (!leader_ && formation_) {
		strstr << prefix << "\tformation = " << (formation_? "yes": "no") << "\n";
	}

	strstr << prefix << "\t[condition]\n";
	if (condition_.city) {
		strstr << prefix << "\t\tcity = " << condition_.city << "\n";
	}
	if (condition_.meritorious) {
		strstr << prefix << "\t\tmeritorious = " << condition_.meritorious << "\n";
	}
	strstr << prefix << "\t[/condition]\n";

	strstr << prefix << "\t[effect]\n";
	if (effect_.leadership) {
		strstr << prefix << "\t\tleadership = " << effect_.leadership << "\n";
	}
	if (effect_.force) {
		strstr << prefix << "\t\tforce = " << effect_.force << "\n";
	}
	if (effect_.intellect) {
		strstr << prefix << "\t\tintellect = " << effect_.intellect << "\n";
	}
	if (effect_.spirit) {
		strstr << prefix << "\t\tspirit = " << effect_.spirit << "\n";
	}
	if (effect_.charm) {
		strstr << prefix << "\t\tcharm = " << effect_.charm << "\n";
	}
	strstr << prefix << "\t[/effect]\n";

	strstr << prefix << "[/noble]\n";

	return strstr.str();
}

void tcore::update_to_ui_noble(HWND hdlgP)
{
	HWND hctl = GetDlgItem(hdlgP, IDC_LV_NOBLE_EXPLORER);
	std::stringstream strstr;
	std::string str;
	char text[_MAX_PATH];

	ListView_DeleteAllItems(hctl);
	// fill data
	LVITEM lvi;
	int iItem = 0;
	for (std::vector<tnoble2>::const_iterator it = nobles_updating_.begin(); it != nobles_updating_.end(); ++ it, iItem ++) {
		int index = 0;
		const tnoble2& noble = *it;
		
		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		// number
		lvi.iItem = iItem;
		lvi.iSubItem = index ++;
		strstr.str("");
		strstr << iItem;
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		lvi.lParam = (LPARAM)iItem;
		ListView_InsertItem(hctl, &lvi);

		// level
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = index ++;
		strstr.str("");
		strstr << "Lv" << noble.level_;
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// name
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = index ++;
		strstr.str("");
		strstr << tnoble::form_name(noble.id_) << "(" << noble.id_ << ")";
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// leader
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = index ++;
		strstr.str("");
		strstr << (noble.leader_? _("Yes"): _("No"));
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// formation
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = index ++;
		strstr.str("");
		strstr << (noble.formation_? _("Yes"): _("No"));
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// condition
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = index ++;
		strstr.str("");
		if (noble.condition_.city) {
			strstr << dsgettext("wesnoth-lib", "City") << " >= " << noble.condition_.city;
		}
		if (noble.condition_.meritorious) {
			if (!strstr.str().empty()) {
				strstr << " && ";
			}
			strstr << dsgettext("wesnoth-hero", "meritorious") << " >= " << noble.condition_.meritorious;
		}
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// effect
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = index ++;
		strstr.str("");
		if (noble.effect_.leadership) {
			strstr << dsgettext("wesnoth-hero", "leadership") << " + " << noble.effect_.leadership;
		}
		if (noble.effect_.force) {
			if (!strstr.str().empty()) {
				strstr << " && ";
			}
			strstr << dsgettext("wesnoth-hero", "force") << " + " << noble.effect_.force;
		}
		if (noble.effect_.intellect) {
			if (!strstr.str().empty()) {
				strstr << " && ";
			}
			strstr << dsgettext("wesnoth-hero", "intellect") << " + " << noble.effect_.intellect;
		}
		if (noble.effect_.spirit) {
			if (!strstr.str().empty()) {
				strstr << " && ";
			}
			strstr << dsgettext("wesnoth-hero", "spirit") << " + " << noble.effect_.spirit;
		}
		if (noble.effect_.charm) {
			if (!strstr.str().empty()) {
				strstr << " && ";
			}
			strstr << dsgettext("wesnoth-hero", "charm") << " + " << noble.effect_.charm;
		}
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);
	}
}

bool tcore::nobles_dirty() const
{
	return ns::core.nobles_updating_ != ns::core.nobles_from_cfg_;
}

bool tnoble2::dirty() const
{
	if (*this != noble_from_cfg_) {
		return true;
	}
	return false;
}

BOOL On_DlgNobleInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	HWND hwndParent = GetParent(hdlgP); 
    DLGHDR *pHdr = (DLGHDR *) GetWindowLong(hwndParent, GWL_USERDATA);
    SetWindowPos(hdlgP, HWND_TOP, pHdr->rcDisplay.left, pHdr->rcDisplay.top, 0, 0, SWP_NOSIZE); 

	std::stringstream strstr;

	HWND hctl = GetDlgItem(hdlgP, IDC_LV_NOBLE_EXPLORER);
	LVCOLUMN lvc;
	int index = 0;
	char text[_MAX_PATH];

	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 40;
	strcpy(text, utf8_2_ansi(_("Number")));
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = index;
	ListView_InsertColumn(hctl, index ++, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 40;
	lvc.iSubItem = index;
	strstr.str("");
	strstr << _("Level");
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, index ++, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 90;
	lvc.iSubItem = index;
	strstr.str("");
	strstr << _("Name");
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, index ++, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 40;
	lvc.iSubItem = index;
	strcpy(text, utf8_2_ansi(_("Leader")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, index ++, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 40;
	lvc.iSubItem = index;
	strcpy(text, dgettext_2_ansi("wesnoth-lib", "tactical^Formation"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, index ++, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 200;
	lvc.iSubItem = index;
	strcpy(text, utf8_2_ansi(_("Condition")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, index ++, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 200;
	lvc.iSubItem = index;
	strcpy(text, utf8_2_ansi(_("Effect")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, index ++, &lvc);

	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	return FALSE;
}

BOOL On_DlgNobleEditInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	std::stringstream strstr;
	if (ns::action_noble == ma_edit) {
		strstr << _("Edit noble");
	} else {
		strstr << _("Add noble");
	}
	SetWindowText(hdlgP, utf8_2_ansi(strstr.str().c_str()));
	ShowWindow(GetDlgItem(hdlgP, IDCANCEL), SW_HIDE);

	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_NAME), utf8_2_ansi(_("Name")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_LEVEL), utf8_2_ansi(_("Level")));
	Button_SetText(GetDlgItem(hdlgP, IDC_CHK_NOBLEEDIT_LEADER), utf8_2_ansi(_("Leader")));
	Button_SetText(GetDlgItem(hdlgP, IDC_CHK_NOBLEEDIT_FORMATION), dgettext_2_ansi("wesnoth-lib", "tactical^Formation"));

	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_CONDITION), utf8_2_ansi(_("Condition")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_CITY), utf8_2_ansi(_("City")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_MERITORIOUS), dgettext_2_ansi("wesnoth-hero", "meritorious"));

	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_EFFECT), utf8_2_ansi(_("Effect")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_LEADERSHIP), dgettext_2_ansi("wesnoth-hero", "leadership"));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_FORCE), dgettext_2_ansi("wesnoth-hero", "force"));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_INTELLECT), dgettext_2_ansi("wesnoth-hero", "intellect"));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_POLITICS), dgettext_2_ansi("wesnoth-hero", "spirit"));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_CHARM), dgettext_2_ansi("wesnoth-hero", "charm"));

	HWND hctl = GetDlgItem(hdlgP, IDC_UD_NOBLEEDIT_CITY);
	UpDown_SetRange(hctl, 0, 10);
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_NOBLEEDIT_CITY));

	hctl = GetDlgItem(hdlgP, IDC_UD_NOBLEEDIT_MERITORIOUS);
	UpDown_SetRange(hctl, 0, 50000);
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_NOBLEEDIT_MERITORIOUS));

	hctl = GetDlgItem(hdlgP, IDC_UD_NOBLEEDIT_LEADERSHIP);
	UpDown_SetRange(hctl, 0, 20);
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_NOBLEEDIT_LEADERSHIP));

	hctl = GetDlgItem(hdlgP, IDC_UD_NOBLEEDIT_FORCE);
	UpDown_SetRange(hctl, 0, 20);
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_NOBLEEDIT_FORCE));

	hctl = GetDlgItem(hdlgP, IDC_UD_NOBLEEDIT_INTELLECT);
	UpDown_SetRange(hctl, 0, 20);
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_NOBLEEDIT_INTELLECT));

	hctl = GetDlgItem(hdlgP, IDC_UD_NOBLEEDIT_POLITICS);
	UpDown_SetRange(hctl, 0, 20);
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_NOBLEEDIT_POLITICS));

	hctl = GetDlgItem(hdlgP, IDC_UD_NOBLEEDIT_CHARM);
	UpDown_SetRange(hctl, 0, 20);
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_NOBLEEDIT_CHARM));

	tnoble2& n = ns::core.nobles_updating_[ns::clicked_noble];
	n.update_to_ui_noble_edit(hdlgP);

	return FALSE;
}

void OnNobleEditEt(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	char text[_MAX_PATH];
	std::stringstream strstr;

	if (codeNotify != EN_CHANGE || id != IDC_ET_NOBLEEDIT_ID) {
		return;
	}

	tnoble2& noble = ns::core.nobles_updating_[ns::clicked_noble];

	Edit_GetText(hwndCtrl, text, sizeof(text) / sizeof(text[0]));
	std::string str = text;
	std::transform(str.begin(), str.end(), str.begin(), std::tolower);
	if (!isvalid_id(str)) {
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_NOBLEEDIT_IDSTATUS), utf8_2_ansi(_("Invalid string")));
		return;
	}
	// cannot be exist id.
	for (size_t i = 0; i < ns::core.nobles_updating_.size(); i ++) {
		if (i != ns::clicked_noble && str == ns::core.nobles_updating_[i].id_) {
			Edit_SetText(GetDlgItem(hdlgP, IDC_ET_NOBLEEDIT_IDSTATUS), utf8_2_ansi(_("Other noble has holded ID")));
			return;
		}
	}
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_NOBLEEDIT_IDSTATUS), "");
	if (noble.id_ == str) {
		return;
	}
	
	// update relative scenario id.
	// cannot be exist id.

	noble.id_ = str;
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_NOBLEEDIT_NAME_MSGID), tnoble::form_name_msgid(noble.id_).c_str());
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_NOBLEEDIT_NAME), utf8_2_ansi(tnoble::form_name(noble.id_).c_str()));

	return;
}

void On_DlgNobleEditCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;

	switch (id) {
	case IDC_ET_NOBLEEDIT_ID:
		OnNobleEditEt(hdlgP, id, hwndCtrl, codeNotify);
		break;

	case IDOK:
		changed = TRUE;
		ns::core.nobles_updating_[ns::clicked_noble].from_ui_noble_edit(hdlgP);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

BOOL CALLBACK DlgNobleEditProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message) {
	case WM_INITDIALOG:
		return On_DlgNobleEditInitDialog(hDlg, (HWND)(wParam), lParam);
	HANDLE_MSG(hDlg, WM_COMMAND, On_DlgNobleEditCommand);
	HANDLE_MSG(hDlg, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	}
	
	return FALSE;
}

void OnNobleAddBt(HWND hdlgP, int after_it)
{
	std::vector<tnoble2>::iterator it = ns::core.nobles_updating_.begin();
	std::advance(it, after_it + 1);
	
	tnoble2 n;
	it = ns::core.nobles_updating_.insert(it, n);
	ns::clicked_noble = after_it + 1;

	ns::core.update_to_ui_noble(hdlgP);
	ns::core.set_dirty(tcore::BIT_NOBLE, ns::core.nobles_dirty()); 
}

void OnNobleDelBt(HWND hdlgP)
{
	std::vector<tnoble2>::iterator it = ns::core.nobles_updating_.begin();
	std::advance(it, ns::clicked_noble);
	it = ns::core.nobles_updating_.erase(it);

	ns::core.update_to_ui_noble(hdlgP);
	ns::core.set_dirty(tcore::BIT_NOBLE, ns::core.nobles_dirty());
}

void OnNobleEditBt(HWND hdlgP)
{
	RECT rcBtn;
	LPARAM lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_LV_NOBLE_EXPLORER), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	ns::action_noble = ma_edit;

	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_NOBLEEDIT), hdlgP, DlgNobleEditProc, lParam)) {
		ns::core.update_to_ui_noble(hdlgP);
		ns::core.set_dirty(tcore::BIT_NOBLE, ns::core.nobles_dirty());
	}
	return;
}

void On_DlgNobleCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	tnoble2& f = ns::core.nobles_updating_[ns::clicked_noble];

	switch (id) {
	case IDM_ADD:
		OnNobleAddBt(hdlgP, ns::clicked_noble);
		break;
	case IDM_DELETE:
		OnNobleDelBt(hdlgP);
		break;
	case IDM_EDIT:
		OnNobleEditBt(hdlgP);
		break;
	}

	return;
}

void noble_notify_handler_rclick(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	LVITEM					lvi;
	LPNMITEMACTIVATE		lpnmitem;

	if (lpNMHdr->idFrom != IDC_LV_NOBLE_EXPLORER) {
		return;
	}

	// NM_表示对通用控件都通用,范围丛(0, 99)
	// TVN_表示只能TreeView通用,范围丛(400, 499)
	lpnmitem = (LPNMITEMACTIVATE)lpNMHdr;
	// 如果单击到的是复选框位置,把复选框状态置反
	// 当前定义的图标大小是16x16. ptAction反回的(x,y)是整个列表视图内的坐标,因而y值不大好判断
	// 认为如果x是小于16的就认为是击中复选框
	
	POINT point = {lpnmitem->ptAction.x, lpnmitem->ptAction.y};
	MapWindowPoints(lpNMHdr->hwndFrom, NULL, &point, 1);

	lvi.iItem = lpnmitem->iItem;
	lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
	lvi.iSubItem = 0;
	lvi.pszText = gdmgr._menu_text;
	lvi.cchTextMax = _MAX_PATH;
	ListView_GetItem(lpNMHdr->hwndFrom, &lvi);

	HMENU hpopup = CreatePopupMenu();

	if (lpNMHdr->idFrom == IDC_LV_NOBLE_EXPLORER) {
		AppendMenu(hpopup, MF_STRING, IDM_ADD, utf8_2_ansi(_("Append after it")));
		AppendMenu(hpopup, MF_STRING, IDM_EDIT, utf8_2_ansi(_("Edit...")));
		AppendMenu(hpopup, MF_STRING, IDM_DELETE, utf8_2_ansi(_("Delete...")));

		if (lpnmitem->iItem < 0) {
			EnableMenuItem(hpopup, IDM_ADD, MF_BYCOMMAND | MF_GRAYED);
			EnableMenuItem(hpopup, IDM_EDIT, MF_BYCOMMAND | MF_GRAYED);
			EnableMenuItem(hpopup, IDM_DELETE, MF_BYCOMMAND | MF_GRAYED);
		}
	}

	TrackPopupMenuEx(hpopup, 0, 
		point.x, 
		point.y, 
		hdlgP, 
		NULL);

	DestroyMenu(hpopup);

	ns::type = DlgItem;
	ns::clicked_noble = lpnmitem->iItem;

    return;
}

void noble_notify_handler_dblclk(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
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

	if (lpnmitem->iItem >= 0) {
		if (lpNMHdr->idFrom == IDC_LV_NOBLE_EXPLORER) {
			ns::clicked_noble = lpnmitem->iItem;
			OnNobleEditBt(hdlgP);
		}
	}
    return;
}

BOOL On_DlgNobleNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	if (lpNMHdr->code == NM_RCLICK) {
		noble_notify_handler_rclick(hdlgP, DlgItem, lpNMHdr);
	} else if (lpNMHdr->code == NM_DBLCLK) {
		noble_notify_handler_dblclk(hdlgP, DlgItem, lpNMHdr);
	}
	return FALSE;
}

BOOL CALLBACK DlgNobleProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_INITDIALOG:
		return On_DlgNobleInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgNobleCommand);
	HANDLE_MSG(hdlgP, WM_NOTIFY,  On_DlgNobleNotify);
	}
	
	return FALSE;
}

#endif