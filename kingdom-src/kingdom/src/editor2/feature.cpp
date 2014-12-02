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

/*
namespace ns {
	int clicked_feature;
	
	int action_feature;
}
*/
void tfeature::from_config(int index, const std::vector<int>& items)
{
	for (std::vector<int>::const_iterator it = items.begin(); it != items.end(); ++ it) {
		items_.insert(*it);
	}

	feature_from_cfg_ = *this;
}

void tfeature::from_ui_feature_edit(HWND hdlgP)
{
	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_FEATUREEDIT_LEVEL);
	level_ = ComboBox_GetCurSel(hctl);
}

void tfeature::update_to_ui_feature_edit(HWND hdlgP) const
{
	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_FEATUREEDIT_LEVEL);
	ComboBox_SetCurSel(hctl, level_);

	hctl = GetDlgItem(hdlgP, IDC_LV_FEATUREEDIT_CANDIDATE);
	std::stringstream strstr;
	std::string str;
	char text[_MAX_PATH];

	ListView_DeleteAllItems(hctl);
	// fill data
	LVITEM lvi;
	int iItem = 0;
	const std::vector<int>& features = hero::valid_features();
	for (std::vector<int>::const_iterator it = features.begin(); it != features.end(); ++ it) {
		if (ns::type == IDC_LV_FEATURE_ATOM) {
			break;
		}
		if (*it >= HEROS_BASE_FEATURE_COUNT) {
			break;
		}
		if (items_.find(*it) != items_.end()) {
			continue;
		}

		int index = 0;
		
		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		// number
		lvi.iItem = iItem;
		lvi.iSubItem = index ++;
		strstr.str("");
		strstr << *it;
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		lvi.lParam = (LPARAM)*it;
		ListView_InsertItem(hctl, &lvi);

		// name
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = index ++;
		strstr.str("");
		strstr << hero::feature_str(*it);
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// description
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = index ++;
		strstr.str("");
		strstr << hero::feature_desc_str(*it);
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		iItem ++;
	}

	hctl = GetDlgItem(hdlgP, IDC_LV_FEATUREEDIT_ITEMS);
	ListView_DeleteAllItems(hctl);
	iItem = 0;
	for (std::set<int>::const_iterator it = items_.begin(); it != items_.end(); ++ it, iItem ++) {
		int index = 0;
		
		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		// number
		lvi.iItem = iItem;
		lvi.iSubItem = index ++;
		strstr.str("");
		strstr << *it;
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		lvi.lParam = (LPARAM)*it;
		ListView_InsertItem(hctl, &lvi);

		// name
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = index ++;
		strstr.str("");
		strstr << hero::feature_str(*it);
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// description
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = index ++;
		strstr.str("");
		strstr << hero::feature_desc_str(*it);
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);
	}

	if (ns::type != IDC_LV_FEATURE_ATOM) {
		Button_Enable(GetDlgItem(hdlgP, IDOK), !items_.empty());
	}
}

void tfeature::update_to_ui_row(HWND hdlgP) const
{
}

std::string tfeature::generate(const std::string& prefix, int index) const
{
	std::stringstream strstr;

	strstr << prefix << index << " = ";
	for (std::set<int>::const_iterator it = items_.begin(); it != items_.end(); ++ it) {
		if (it != items_.begin()) {
			strstr << ", ";
		}
		strstr << *it;
	}
	strstr << "\n";

	return strstr.str();
}

void tcore::update_to_ui_feature(HWND hdlgP)
{
	HWND hctl = GetDlgItem(hdlgP, IDC_LV_FEATURE_ATOM);
	std::stringstream strstr;
	std::string str;
	char text[_MAX_PATH];

	ListView_DeleteAllItems(hctl);
	// fill data
	LVITEM lvi;
	int iItem = 0;
	for (std::vector<tfeature>::iterator it = features_updating_.begin(); it != features_updating_.end(); ++ it) {
		int feature = std::distance(features_updating_.begin(), it);
		if (feature >= HEROS_BASE_FEATURE_COUNT) {
			break;
		}
		const tfeature& f = *it;
		int index = 0;
		
		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		// number
		lvi.iItem = iItem ++;
		lvi.iSubItem = index ++;
		strstr.str("");
		strstr << feature;
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		lvi.lParam = (LPARAM)feature;
		ListView_InsertItem(hctl, &lvi);

		// name
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = index ++;
		strstr.str("");
		strstr << hero::feature_str(feature);
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// level
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = index ++;
		strstr.str("");
		strstr << f.level_;
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// description
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = index ++;
		strstr.str("");
		strstr << hero::feature_desc_str(feature);
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);
	}

	// complex
	hctl = GetDlgItem(hdlgP, IDC_LV_FEATURE_COMPLEX);
	ListView_DeleteAllItems(hctl);
	// fill data
	iItem = 0;
	for (std::vector<tfeature>::iterator it = features_updating_.begin(); it != features_updating_.end(); ++ it) {
		int feature = std::distance(features_updating_.begin(), it);
		if (feature < HEROS_BASE_FEATURE_COUNT) {
			continue;
		}
		const tfeature& f = *it;
		int index = 0;
		
		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		// number
		lvi.iItem = iItem;
		lvi.iSubItem = index ++;
		strstr.str("");
		strstr << feature;
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		lvi.lParam = (LPARAM)feature;
		ListView_InsertItem(hctl, &lvi);

		// name
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = index ++;
		strstr.str("");
		strstr << hero::feature_str(feature);
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// level
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = index ++;
		strstr.str("");
		strstr << f.level_;
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// item
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = index ++;
		strstr.str("");
		for (std::set<int>::const_iterator it2 = it->items_.begin(); it2 != it->items_.end(); ++ it2) {
			if (it2 != it->items_.begin()) {
				strstr << ", ";
			}
			strstr << hero::feature_str(*it2);
		}
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// description
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = index ++;
		strstr.str("");
		strstr << hero::feature_desc_str(feature);
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		iItem ++;
	}
}

bool tcore::features_dirty() const
{
	return ns::core.features_updating_ != ns::core.features_from_cfg_;
}

bool tfeature::dirty() const
{
	if (*this != feature_from_cfg_) {
		return true;
	}
	return false;
}

BOOL On_DlgFeatureInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	HWND hwndParent = GetParent(hdlgP); 
    DLGHDR *pHdr = (DLGHDR *) GetWindowLong(hwndParent, GWL_USERDATA);
    SetWindowPos(hdlgP, HWND_TOP, pHdr->rcDisplay.left, pHdr->rcDisplay.top, 0, 0, SWP_NOSIZE); 

	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_REMARK), utf8_2_ansi(_("Atomic feature remark")));

	std::stringstream strstr;

	HWND hctl = GetDlgItem(hdlgP, IDC_LV_FEATURE_ATOM);
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
	strstr.str("");
	strstr << _("Level");
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, index ++, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 400;
	lvc.iSubItem = index;
	strcpy(text, utf8_2_ansi(_("Description")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, index ++, &lvc);

	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	// complex
	hctl = GetDlgItem(hdlgP, IDC_LV_FEATURE_COMPLEX);
	index = 0;

	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 40;
	strcpy(text, utf8_2_ansi(_("Number")));
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = index;
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
	strstr.str("");
	strstr << _("Level");
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, index ++, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 180;
	lvc.iSubItem = index;
	strstr.str("");
	strstr << _("Item");
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, index ++, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 350;
	lvc.iSubItem = index;
	strcpy(text, utf8_2_ansi(_("Description")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, index ++, &lvc);

	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	return FALSE;
}

BOOL On_DlgFeatureEditInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	std::stringstream strstr;
	if (ns::type == IDC_LV_FEATURE_ATOM) {
		strstr << _("Edit atomic feature");
	} else {
		strstr << _("Edit complex feature");
	}
	
	int feature = ns::type == IDC_LV_FEATURE_ATOM? ns::clicked_feature: HEROS_BASE_FEATURE_COUNT + ns::clicked_feature;
	tfeature& f = ns::core.features_updating_[feature];
	strstr << std::setfill('0') << std::setw(3) << feature << ": " << hero::feature_str(feature);
	SetWindowText(hdlgP, utf8_2_ansi(strstr.str().c_str()));
	ShowWindow(GetDlgItem(hdlgP, IDCANCEL), SW_HIDE);

	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_LEVEL), utf8_2_ansi(_("Level")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_CANDIDATE), utf8_2_ansi(_("Candidate")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_ITEM), utf8_2_ansi(_("Item")));

	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_FEATUREEDIT_LEVEL);
	ComboBox_AddString(hctl, "Lv0");
	ComboBox_AddString(hctl, "Lv1");
	ComboBox_AddString(hctl, "Lv2");
	ComboBox_AddString(hctl, "Lv3");
	ComboBox_AddString(hctl, "Lv4");

	hctl = GetDlgItem(hdlgP, IDC_LV_FEATUREEDIT_CANDIDATE);
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
	lvc.cx = 60;
	lvc.iSubItem = index;
	strstr.str("");
	strstr << _("Name");
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, index ++, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 320;
	lvc.iSubItem = index;
	strcpy(text, utf8_2_ansi(_("Description")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, index ++, &lvc);

	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	// item
	hctl = GetDlgItem(hdlgP, IDC_LV_FEATUREEDIT_ITEMS);
	index = 0;

	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 40;
	strcpy(text, utf8_2_ansi(_("Number")));
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = index;
	ListView_InsertColumn(hctl, index ++, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 60;
	lvc.iSubItem = index;
	strstr.str("");
	strstr << _("Name");
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, index ++, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 320;
	lvc.iSubItem = index;
	strcpy(text, utf8_2_ansi(_("Description")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, index ++, &lvc);

	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	f.update_to_ui_feature_edit(hdlgP);

	return FALSE;
}

void On_DlgFeatureEditCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;
	int feature = ns::type == IDC_LV_FEATURE_ATOM? ns::clicked_feature: HEROS_BASE_FEATURE_COUNT + ns::clicked_feature;
	tfeature& f = ns::core.features_updating_[feature];

	switch (id) {
	case IDM_ADD:
		f.items_.insert(ns::clicked_item);
		f.update_to_ui_feature_edit(hdlgP);
		break;
	case IDM_DELETE:
		f.items_.erase(ns::clicked_item);
		f.update_to_ui_feature_edit(hdlgP);
		break;

	case IDOK:
		changed = TRUE;
		ns::core.features_updating_[feature].from_ui_feature_edit(hdlgP);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

void featureedit_notify_handler_rclick(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	LVITEM					lvi;
	LPNMITEMACTIVATE		lpnmitem;

	if (lpNMHdr->idFrom == IDC_LV_FEATUREEDIT_CANDIDATE &&
		lpNMHdr->idFrom == IDC_LV_FEATUREEDIT_ITEMS) {
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

	if (lpNMHdr->idFrom == IDC_LV_FEATUREEDIT_CANDIDATE) {
		AppendMenu(hpopup, MF_STRING, IDM_ADD, utf8_2_ansi(_("Add")));
		if (lpnmitem->iItem < 0) {
			EnableMenuItem(hpopup, IDM_ADD, MF_BYCOMMAND | MF_GRAYED);
		}
	} else if (lpNMHdr->idFrom == IDC_LV_FEATUREEDIT_ITEMS) {
		AppendMenu(hpopup, MF_STRING, IDM_DELETE, utf8_2_ansi(_("Delete")));
		if (lpnmitem->iItem < 0) {
			EnableMenuItem(hpopup, IDM_DELETE, MF_BYCOMMAND | MF_GRAYED);
		}
	}

	TrackPopupMenuEx(hpopup, 0, 
		point.x, 
		point.y, 
		hdlgP, 
		NULL);

	DestroyMenu(hpopup);

	ns::clicked_item = lvi.lParam;

    return;
}

BOOL On_DlgFeatureEditNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	if (lpNMHdr->code == NM_RCLICK) {
		featureedit_notify_handler_rclick(hdlgP, DlgItem, lpNMHdr);
	}
	return FALSE;
}

BOOL CALLBACK DlgFeatureEditProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message) {
	case WM_INITDIALOG:
		return On_DlgFeatureEditInitDialog(hDlg, (HWND)(wParam), lParam);
	HANDLE_MSG(hDlg, WM_COMMAND, On_DlgFeatureEditCommand);
	HANDLE_MSG(hDlg, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	HANDLE_MSG(hDlg, WM_NOTIFY,  On_DlgFeatureEditNotify);
	}
	
	return FALSE;
}

void OnFeatureAddBt(HWND hdlgP, int after_it)
{
	std::vector<tfeature>::iterator it = ns::core.features_updating_.begin();
	std::advance(it, HEROS_BASE_FEATURE_COUNT + after_it + 1);
	
	tfeature f;
	it = ns::core.features_updating_.insert(it, f);
	ns::clicked_feature = after_it + 1;

	ns::core.update_to_ui_feature(hdlgP);
	ns::core.set_dirty(tcore::BIT_FEATURE, ns::core.features_dirty()); 
}

void OnFeatureDelBt(HWND hdlgP)
{
	std::vector<tfeature>::iterator it = ns::core.features_updating_.begin();
	std::advance(it, HEROS_BASE_FEATURE_COUNT + ns::clicked_feature);
	it = ns::core.features_updating_.erase(it);

	ns::core.update_to_ui_feature(hdlgP);
	ns::core.set_dirty(tcore::BIT_FEATURE, ns::core.features_dirty());
}

void OnFeatureEditBt(HWND hdlgP)
{
	RECT rcBtn;
	LPARAM lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_LV_FEATURE_COMPLEX), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	ns::action_feature = ma_edit;

	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_FEATUREEDIT), hdlgP, DlgFeatureEditProc, lParam)) {
		ns::core.update_to_ui_feature(hdlgP);
		ns::core.set_dirty(tcore::BIT_FEATURE, ns::core.features_dirty());
	}

	return;
}

void On_DlgFeatureCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	switch (id) {
	case IDM_ADD:
		OnFeatureAddBt(hdlgP, ns::clicked_feature);
		break;
	case IDM_DELETE:
		OnFeatureDelBt(hdlgP);
		break;
	case IDM_EDIT:
		OnFeatureEditBt(hdlgP);
		break;
	}

	return;
}

void feature_notify_handler_rclick(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	LVITEM					lvi;
	LPNMITEMACTIVATE		lpnmitem;

	if (lpNMHdr->idFrom != IDC_LV_FEATURE_COMPLEX) {
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

	if (lpNMHdr->idFrom == IDC_LV_FEATURE_COMPLEX) {
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
	ns::clicked_feature = lpnmitem->iItem;

    return;
}

void feature_notify_handler_dblclk(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
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
		if ((IDC_LV_FEATURE_ATOM && ns::core.features_updating_[lpnmitem->iItem].level_ >= 0) || lpNMHdr->idFrom == IDC_LV_FEATURE_COMPLEX) {
			ns::clicked_feature = lpnmitem->iItem;
			ns::type = DlgItem;
			OnFeatureEditBt(hdlgP);
		}
	}
    return;
}

BOOL On_DlgFeatureNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	if (lpNMHdr->code == NM_RCLICK) {
		feature_notify_handler_rclick(hdlgP, DlgItem, lpNMHdr);
	} else if (lpNMHdr->code == NM_DBLCLK) {
		feature_notify_handler_dblclk(hdlgP, DlgItem, lpNMHdr);
	}
	return FALSE;
}

BOOL CALLBACK DlgFeatureProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_INITDIALOG:
		return On_DlgFeatureInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgFeatureCommand);
	HANDLE_MSG(hdlgP, WM_NOTIFY,  On_DlgFeatureNotify);
	}
	
	return FALSE;
}

#endif