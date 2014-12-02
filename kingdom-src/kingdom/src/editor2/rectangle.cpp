#define GETTEXT_DOMAIN "wesnoth-maker"

#include "global.hpp"
#include "stdafx.h"
#include <windowsx.h>
#include "struct.h"
#include "rectangle.hpp"
#include "gettext.hpp"

#include "resource.h"

tzoom_rect::tzoom_rect(int id, const HWND hwnd)
	: id_(id)
	, normal_()
	, max_()
{
	RECT rc;

	GetWindowRect(hwnd, &rc);
	MapWindowPoints(NULL, GetParent(hwnd), (LPPOINT)(&rc), (sizeof(RECT)/sizeof(POINT)));
	normal_ = rc;
}

void tzoom_rect::place_rect(const HWND hwnd, const RECT& rc)
{
	MoveWindow(hwnd, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);
}

void tzoom_rect::calculate_max(const HWND hwnd)
{
	RECT rcMain;
	int vertical_diff;
	
	GetClientRect(hwnd, &rcMain);
	vertical_diff = rcMain.bottom - normal_.bottom;

	max_ = normal_;
	max_.top = max_.top + vertical_diff;
	max_.bottom = max_.bottom + vertical_diff;
}

void tzoom_rect::placement(const HWND hwnd, bool max)
{
	if (max) {
		place_rect(hwnd, max_);
	} else {
		place_rect(hwnd, normal_);
	}
}

tstatus_rect::tstatus_rect(const HWND hwnd)
	: tzoom_rect(status, hwnd)
{
}

tddesc_rect::tddesc_rect(const HWND hwnd)
	: tzoom_rect(ddesc, hwnd)
{
	RECT rc;

	// IDC_TV_DDESC_EXPLORER
	GetWindowRect(GetDlgItem(hwnd, IDC_TV_DDESC_EXPLORER), &rc);
	MapWindowPoints(NULL, hwnd, (LPPOINT)(&rc), (sizeof(RECT)/sizeof(POINT)));
	explorer_[NORMAL] = rc;

	// IDC_ET_DDESC_SUBAREA
	GetWindowRect(GetDlgItem(hwnd, IDC_ET_DDESC_SUBAREA), &rc);
	MapWindowPoints(NULL, hwnd, (LPPOINT)(&rc), (sizeof(RECT)/sizeof(POINT)));
	subarea_[NORMAL] = rc;
}

void tddesc_rect::calculate_max(const HWND hwnd)
{
	RECT rcMain;
	int vertical_diff;
	
	GetClientRect(hwnd, &rcMain);
	vertical_diff = rcMain.bottom - normal_.bottom;

	max_ = normal_;
	max_.bottom = rcMain.bottom;

	explorer_[MAX] = explorer_[NORMAL];
	explorer_[MAX].bottom = explorer_[NORMAL].bottom + vertical_diff;

	subarea_[MAX] = subarea_[NORMAL];
	subarea_[MAX].top = subarea_[NORMAL].top + vertical_diff;
	subarea_[MAX].bottom = subarea_[NORMAL].bottom + vertical_diff;
}

void tddesc_rect::placement(const HWND hwnd, bool max)
{
	tzoom_rect::placement(hwnd, max);
	if (max) {
		place_rect(GetDlgItem(hwnd, IDC_TV_DDESC_EXPLORER), explorer_[MAX]);
		place_rect(GetDlgItem(hwnd, IDC_ET_DDESC_SUBAREA), subarea_[MAX]);
	} else {
		place_rect(GetDlgItem(hwnd, IDC_TV_DDESC_EXPLORER), explorer_[NORMAL]);
		place_rect(GetDlgItem(hwnd, IDC_ET_DDESC_SUBAREA), subarea_[NORMAL]);
	}
}

tsync_rect::tsync_rect(const HWND hwnd)
	: tzoom_rect(sync, hwnd)
{
	RECT rc;

	// IDC_LV_SYNC_SYNC
	GetWindowRect(GetDlgItem(hwnd, IDC_LV_SYNC_SYNC), &rc);
	MapWindowPoints(NULL, hwnd, (LPPOINT)(&rc), (sizeof(RECT)/sizeof(POINT)));
	explorer_[NORMAL] = rc;

	// IDC_ET_SYNC_SUBAREA
	GetWindowRect(GetDlgItem(hwnd, IDC_ET_SYNC_SUBAREA), &rc);
	MapWindowPoints(NULL, hwnd, (LPPOINT)(&rc), (sizeof(RECT)/sizeof(POINT)));
	subarea_[NORMAL] = rc;

}

void tsync_rect::calculate_max(const HWND hwnd)
{
	RECT rcMain;
	int horizontal_diff, vertical_diff;
	
	GetClientRect(hwnd, &rcMain);
	horizontal_diff = rcMain.right - normal_.right;
	vertical_diff = rcMain.bottom - normal_.bottom;

	max_ = normal_;
	max_.right = rcMain.right;
	max_.bottom = rcMain.bottom;

	explorer_[MAX] = explorer_[NORMAL];
	explorer_[MAX].right = explorer_[NORMAL].right + horizontal_diff;
	explorer_[MAX].bottom = explorer_[NORMAL].bottom + vertical_diff;

	subarea_[MAX] = subarea_[NORMAL];
	subarea_[MAX].top = subarea_[NORMAL].top + vertical_diff;
	subarea_[MAX].bottom = subarea_[NORMAL].bottom + vertical_diff;
}

void tsync_rect::placement(const HWND hwnd, bool max)
{
	tzoom_rect::placement(hwnd, max);
	if (max) {
		place_rect(GetDlgItem(hwnd, IDC_LV_SYNC_SYNC), explorer_[MAX]);
		place_rect(GetDlgItem(hwnd, IDC_ET_SYNC_SUBAREA), subarea_[MAX]);
	} else {
		place_rect(GetDlgItem(hwnd, IDC_LV_SYNC_SYNC), explorer_[NORMAL]);
		place_rect(GetDlgItem(hwnd, IDC_ET_SYNC_SUBAREA), subarea_[NORMAL]);
	}
}

thero_rect::thero_rect(const HWND hwnd)
	: tzoom_rect(hero, hwnd)
{
	RECT rc;

	// IDC_LV_WGEN_EDITOR
	GetWindowRect(GetDlgItem(hwnd, IDC_LV_WGEN_EDITOR), &rc);
	MapWindowPoints(NULL, hwnd, (LPPOINT)(&rc), (sizeof(RECT)/sizeof(POINT)));
	explorer_[NORMAL] = rc;
}

void thero_rect::calculate_max(const HWND hwnd)
{
	RECT rcMain;
	int horizontal_diff, vertical_diff;
	
	GetClientRect(hwnd, &rcMain);
	horizontal_diff = rcMain.right - normal_.right;
	vertical_diff = rcMain.bottom - normal_.bottom;

	max_ = normal_;
	max_.right = rcMain.right;
	max_.bottom = rcMain.bottom;

	explorer_[MAX] = explorer_[NORMAL];
	explorer_[MAX].right = explorer_[NORMAL].right + horizontal_diff;
	explorer_[MAX].bottom = explorer_[NORMAL].bottom + vertical_diff;
}

void thero_rect::placement(const HWND hwnd, bool max)
{
	tzoom_rect::placement(hwnd, max);
	if (max) {
		place_rect(GetDlgItem(hwnd, IDC_LV_WGEN_EDITOR), explorer_[MAX]);
	} else {
		place_rect(GetDlgItem(hwnd, IDC_LV_WGEN_EDITOR), explorer_[NORMAL]);
	}
}

tcore_rect::tcore_rect(const HWND hwnd)
	: tzoom_rect(core, hwnd)
{
	RECT rc;

	// IDC_TAB_CORE_SECTION
	GetWindowRect(GetDlgItem(hwnd, IDC_TAB_CORE_SECTION), &rc);
	MapWindowPoints(NULL, hwnd, (LPPOINT)(&rc), (sizeof(RECT)/sizeof(POINT)));
	explorer_[NORMAL] = rc;
}

void tcore_rect::calculate_max(const HWND hwnd)
{
	RECT rcMain;
	int horizontal_diff, vertical_diff;
	
	GetClientRect(hwnd, &rcMain);
	horizontal_diff = rcMain.right - normal_.right;
	vertical_diff = rcMain.bottom - normal_.bottom;

	max_ = normal_;
	max_.right = rcMain.right;
	max_.bottom = rcMain.bottom;

	explorer_[MAX] = explorer_[NORMAL];
	explorer_[MAX].right = explorer_[NORMAL].right + horizontal_diff;
	explorer_[MAX].bottom = explorer_[NORMAL].bottom + vertical_diff;
}

void tcore_rect::placement(const HWND hwnd, bool max)
{
	tzoom_rect::placement(hwnd, max);
	if (max) {
		place_rect(GetDlgItem(hwnd, IDC_TAB_CORE_SECTION), explorer_[MAX]);
	} else {
		place_rect(GetDlgItem(hwnd, IDC_TAB_CORE_SECTION), explorer_[NORMAL]);
	}
}

tvisual_rect::tvisual_rect(const HWND hwnd)
	: tzoom_rect(visual, hwnd)
{
	RECT rc;

	// IDC_TV_CFG_EXPLORER
	GetWindowRect(GetDlgItem(hwnd, IDC_TV_CFG_EXPLORER), &rc);
	MapWindowPoints(NULL, hwnd, (LPPOINT)(&rc), (sizeof(RECT)/sizeof(POINT)));
	explorer_[NORMAL] = rc;
}

void tvisual_rect::calculate_max(const HWND hwnd)
{
	RECT rcMain;
	int horizontal_diff, vertical_diff;
	
	GetClientRect(hwnd, &rcMain);
	horizontal_diff = rcMain.right - normal_.right;
	vertical_diff = rcMain.bottom - normal_.bottom;

	max_ = normal_;
	max_.right = rcMain.right;
	max_.bottom = rcMain.bottom;

	explorer_[MAX] = explorer_[NORMAL];
	explorer_[MAX].right = explorer_[NORMAL].right + horizontal_diff;
	explorer_[MAX].bottom = explorer_[NORMAL].bottom + vertical_diff;
}

void tvisual_rect::placement(const HWND hwnd, bool max)
{
	tzoom_rect::placement(hwnd, max);
	if (max) {
		place_rect(GetDlgItem(hwnd, IDC_TV_CFG_EXPLORER), explorer_[MAX]);
	} else {
		place_rect(GetDlgItem(hwnd, IDC_TV_CFG_EXPLORER), explorer_[NORMAL]);
	}
}

tcampaign_rect::tcampaign_rect(const HWND hwnd)
	: tzoom_rect(campaign, hwnd)
{
	RECT rc;

	// IDC_TAB_CAMP_SCENARIO
	GetWindowRect(GetDlgItem(hwnd, IDC_TAB_CAMP_SCENARIO), &rc);
	MapWindowPoints(NULL, hwnd, (LPPOINT)(&rc), (sizeof(RECT)/sizeof(POINT)));
	explorer_[NORMAL] = rc;
}

void tcampaign_rect::calculate_max(const HWND hwnd)
{
	RECT rcMain;
	int horizontal_diff, vertical_diff;
	
	GetClientRect(hwnd, &rcMain);
	horizontal_diff = rcMain.right - normal_.right;
	vertical_diff = rcMain.bottom - normal_.bottom;

	max_ = normal_;
	max_.right = rcMain.right;
	max_.bottom = rcMain.bottom;

	explorer_[MAX] = explorer_[NORMAL];
	explorer_[MAX].right = explorer_[NORMAL].right + horizontal_diff;
	explorer_[MAX].bottom = explorer_[NORMAL].bottom + vertical_diff;
}

void tcampaign_rect::placement(const HWND hwnd, bool max)
{
	tzoom_rect::placement(hwnd, max);
	if (max) {
		place_rect(GetDlgItem(hwnd, IDC_TAB_CAMP_SCENARIO), explorer_[MAX]);
	} else {
		place_rect(GetDlgItem(hwnd, IDC_TAB_CAMP_SCENARIO), explorer_[NORMAL]);
	}
}

tintegrate_rect::tintegrate_rect(const HWND hwnd)
	: tzoom_rect(integrate, hwnd)
{
	RECT rc;

	// IDC_ET_INTEGRATE_TEXT
	GetWindowRect(GetDlgItem(hwnd, IDC_ET_INTEGRATE_TEXT), &rc);
	MapWindowPoints(NULL, hwnd, (LPPOINT)(&rc), (sizeof(RECT)/sizeof(POINT)));
	explorer_[NORMAL] = rc;
}

void tintegrate_rect::calculate_max(const HWND hwnd)
{
	RECT rcMain;
	int horizontal_diff, vertical_diff;
	
	GetClientRect(hwnd, &rcMain);
	horizontal_diff = rcMain.right - normal_.right;
	vertical_diff = rcMain.bottom - normal_.bottom;

	max_ = normal_;
	max_.right = rcMain.right;
	max_.bottom = rcMain.bottom;

	explorer_[MAX] = explorer_[NORMAL];
	explorer_[MAX].right = explorer_[NORMAL].right + horizontal_diff;
	explorer_[MAX].bottom = explorer_[NORMAL].bottom + vertical_diff;
}

void tintegrate_rect::placement(const HWND hwnd, bool max)
{
	tzoom_rect::placement(hwnd, max);
	if (max) {
		place_rect(GetDlgItem(hwnd, IDC_ET_INTEGRATE_TEXT), explorer_[MAX]);
	} else {
		place_rect(GetDlgItem(hwnd, IDC_ET_INTEGRATE_TEXT), explorer_[NORMAL]);
	}
}

std::map<int, tzoom_rect*> zoom_rects; 

void tzoom_rect::calculate_maxs(HWND hwnd)
{
	for (std::map<int, tzoom_rect*>::const_iterator it= zoom_rects.begin(); it != zoom_rects.end(); ++ it) {
		tzoom_rect& rect = *it->second;
		rect.calculate_max(hwnd);
	}
}


//
// candidate hero
//
extern void listview_find_hero(HWND hlistview, HWND find_et);

namespace candidate_hero {
int listview_id = IDC_LV_CANDIDATEHERO;
int find_et_id = IDC_ET_FINDHERO;
int find_bt_id = IDC_BT_FINDHERO;
LPARAM lParam = 0;

void fill_header(HWND hwnd)
{
	Button_SetText(GetDlgItem(hwnd, find_bt_id), utf8_2_ansi(_("Find")));

	Button_Enable(GetDlgItem(hwnd, find_bt_id), FALSE);

	char text[1024];
	HWND hctl = GetDlgItem(hwnd, listview_id);
	LVCOLUMN lvc;

	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 90;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "name"));
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = 0;
	ListView_InsertColumn(hctl, 0, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 60;
	lvc.iSubItem = 1;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "catalog"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 1, &lvc);

	lvc.cx = 48;
	lvc.iSubItem = 2;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "feature"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 2, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = 3;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "side feature"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 3, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = 4;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "leadership"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 4, &lvc);

	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);
}

void fill_row(HWND hctl, hero& h)
{
	std::stringstream strstr;
	char text[_MAX_PATH];
	LVITEM lvi;
	int column = 0;

	lvi.mask = LVIF_TEXT | LVIF_PARAM;
	// 姓名
	lvi.iItem = ListView_GetItemCount(hctl);
	lvi.iSubItem = column ++;
	int number = h.number_;
	strstr.str("");
	strstr << std::setfill('0') << std::setw(3) << number << ": " << h.name();
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	lvi.pszText = text;
	lvi.lParam = (LPARAM)h.number_;
	ListView_InsertItem(hctl, &lvi);

	// 相性
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	strstr.str("");
	int value = h.base_catalog_;
	strstr << value;
	strcpy(text, strstr.str().c_str());
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 特技
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	strcpy(text, utf8_2_ansi(hero::feature_str(h.feature_).c_str()));
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 势力特技
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	strcpy(text, utf8_2_ansi(hero::feature_str(h.side_feature_).c_str()));
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 统帅
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	sprintf(text, "%u.%u", fxptoi9(h.leadership_), fxpmod9(h.leadership_));
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);
}

bool on_command(HWND hwnd, int id, UINT codeNotify)
{
	char text[_MAX_PATH];
	if (id == find_et_id) {
		if (codeNotify == EN_CHANGE) {
			Edit_GetText(GetDlgItem(hwnd, find_et_id), text, sizeof(text) / sizeof(text[0]));
			Button_Enable(GetDlgItem(hwnd, find_bt_id), strlen(text));
		}
#ifndef _ROSE_EDITOR
	} else if (id == find_bt_id) {
		listview_find_hero(GetDlgItem(hwnd, listview_id), GetDlgItem(hwnd, find_et_id));
	} else {
#endif
		return false;
	}
	return true;
}

bool notify_handler_rclick(const std::map<int, std::string>& menu, HWND hwnd, int id, LPNMHDR lpNMHdr, is_enable_cb fn)
{
	if (id != listview_id) {
		return false;
	}
	LVITEM lvi;
	LPNMITEMACTIVATE lpnmitem = (LPNMITEMACTIVATE)lpNMHdr;

	POINT point = {lpnmitem->ptAction.x, lpnmitem->ptAction.y};
	MapWindowPoints(lpNMHdr->hwndFrom, NULL, &point, 1);

	lvi.iItem = lpnmitem->iItem;
	lvi.mask = LVIF_PARAM;
	lvi.iSubItem = 0;
	ListView_GetItem(lpNMHdr->hwndFrom, &lvi);

	HMENU hpopup_candidate = CreatePopupMenu();
	for (std::map<int, std::string>::const_iterator it = menu.begin(); it != menu.end(); ++ it) {
		AppendMenu(hpopup_candidate, MF_STRING, it->first, utf8_2_ansi(it->second.c_str()));
		if (lpnmitem->iItem < 0 || (fn && !fn(hwnd, it->first, lvi.lParam))) {
			EnableMenuItem(hpopup_candidate, it->first, MF_BYCOMMAND | MF_GRAYED);
		}
	}

	TrackPopupMenuEx(hpopup_candidate, 0, 
		point.x, 
		point.y, 
		hwnd, 
		NULL);

	DestroyMenu(hpopup_candidate);

	lParam = lvi.lParam;
	return true;
}

}

namespace scroll {

LPARAM first_visible_lparam = 0;
HTREEITEM first_visible_htvi;

void treeview_update_scroll(HWND htv, fn_treeview_update_scroll fn, void* ctx)
{
	TVITEMEX tvi;
	HTREEITEM htvi = TreeView_GetFirstVisible(htv);
	TreeView_GetItem1(htv, htvi, &tvi, TVIF_PARAM | TVIF_CHILDREN, NULL);
	if (htvi && fn) {
		scroll::first_visible_lparam = tvi.lParam;
		fn(htv, htvi, tvi, ctx);
	}

	first_visible_lparam = 0;
}


void cb_treeview_walk_find_lparam(HWND hctl, HTREEITEM htvi, uint32_t* ctx)
{
	TVITEMEX	tvi;
	TreeView_GetItem1(hctl, htvi, &tvi, TVIF_PARAM, NULL);
	if (tvi.lParam == first_visible_lparam) {
		first_visible_htvi = htvi;
	}
}

void treeview_scroll_to(HWND htv)
{
	if (first_visible_lparam) {
		TreeView_Walk(htv, TVI_ROOT, TRUE, cb_treeview_walk_find_lparam, NULL, FALSE);
		TreeView_SelectSetFirstVisible(htv, first_visible_htvi);
	}
}

}