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

BOOL CALLBACK DlgMultiplayerCoreProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam);
extern BOOL CALLBACK DlgCampaignScenarioProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

VOID WINAPI OnSelChanged2(HWND hwndDlg)
{ 
    DLGHDR *pHdr = (DLGHDR *) GetWindowLong(hwndDlg, GWL_USERDATA); 
    int iSel = TabCtrl_GetCurSel(pHdr->hwndTab); 
 
    // Destroy the current child dialog box, if any. 
	if (pHdr->hwndDisplay != NULL) {
        DestroyWindow(pHdr->hwndDisplay); 
	}
 
    // Create the new child dialog box.
	if (iSel == 0) {
		pHdr->hwndDisplay = CreateDialogIndirect(gdmgr._hinst, pHdr->apRes[iSel], hwndDlg, DlgMultiplayerCoreProc);
	} else {
		pHdr->hwndDisplay = CreateDialogIndirect(gdmgr._hinst, pHdr->apRes[iSel], hwndDlg, DlgCampaignScenarioProc);
	}
	ShowWindow(pHdr->hwndDisplay, SW_RESTORE);

	core_enable_delete_btn(iSel > 0);
}

VOID WINAPI OnSelChanging2(HWND hwndDlg)
{
    DLGHDR *pHdr = (DLGHDR *) GetWindowLong(hwndDlg, GWL_USERDATA); 
    int iSel = TabCtrl_GetCurSel(pHdr->hwndTab); 
 
    // Create the new child dialog box.
	if (iSel == 0) {
		ns::_main.from_ui(pHdr->hwndDisplay);
	} else {
		ns::_scenario[iSel - 1].from_ui(pHdr->hwndDisplay);
	}
}

/*
namespace ns {
	int clicked_faction;
	
	int action_faction;
}

void tfaction::from_config(const config& cfg, std::set<int>& used_heros, std::set<int>& used_cities)
{
	int number;

	const config& art_cfg = cfg.child("artifical");

	number = art_cfg["heros_army"].to_int();
	if (used_cities.find(number) == used_cities.end()) {
		city_ = number;
		used_cities.insert(city_);
	}

	std::vector<std::string> vstr = utils::split(art_cfg["service_heros"].str());
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		number = lexical_cast_default<int>(*it);
		if (used_heros.find(number) == used_heros.end()) {
			freshes_.insert(number);
			used_heros.insert(number);
		}
	}

	vstr = utils::split(art_cfg["wander_heros"].str());
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		number = lexical_cast_default<int>(*it);
		if (used_heros.find(number) == used_heros.end()) {
			wanderes_.insert(number);
			used_heros.insert(number);
		}
	}

	number = cfg["leader"].to_int();
	if (freshes_.find(number) != freshes_.end()) {
		leader_ = number;
	}

	faction_from_cfg_ = *this;
}

void tfaction::from_ui_faction_edit(HWND hdlgP)
{
	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_FACTIONEDIT_LEADER);
	int selected = ComboBox_GetCurSel(hctl);
	if (selected >= 0) {
		leader_ = ComboBox_GetItemData(hctl, selected);
	}

	hctl = GetDlgItem(hdlgP, IDC_CMB_FACTIONEDIT_CITY);
	selected = ComboBox_GetCurSel(hctl);
	if (selected >= 0) {
		city_ = ComboBox_GetItemData(hctl, selected);
	}
}

void tfaction::update_to_ui_faction_edit(HWND hdlgP) const
{
	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_FACTIONEDIT_LEADER);
	int selected = -1;
	for (std::set<int>::const_iterator it = freshes_.begin(); it != freshes_.end(); ++ it) {
		hero& h = gdmgr.heros_[*it];
		ComboBox_AddString(hctl, utf8_2_ansi(h.name().c_str()));
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, h.number_);
		if (h.number_ == leader_) {
			selected = ComboBox_GetCount(hctl) - 1;
		}
	}
	if (selected != -1) {
		ComboBox_SetCurSel(hctl, selected);
	}

	std::set<int> candidate_cities;
	std::set<int> used_cities;
	for (std::vector<tfaction>::const_iterator it = ns::core.factions_updating_.begin(); it != ns::core.factions_updating_.end(); ++ it) {
		const tfaction& f = *it;
		if (f.city_ != city_) {
			used_cities.insert(f.city_);
		}
	}
	for (hero_map::const_iterator it = gdmgr.heros_.begin(); it != gdmgr.heros_.end(); ++ it) {
		const hero& h = *it;
		if (hero::is_artifical(h.number_)) {
			continue;
		}
		if (h.gender_ != hero_gender_neutral) {
			continue;
		}
		if (used_cities.find(h.number_) != used_cities.end()) {
			continue;
		}
		candidate_cities.insert(h.number_);
	}

	hctl = GetDlgItem(hdlgP, IDC_CMB_FACTIONEDIT_CITY);
	ComboBox_ResetContent(hctl);
	selected = -1;
	for (std::set<int>::const_iterator it = candidate_cities.begin(); it != candidate_cities.end(); ++ it) {
		int number = *it;
		ComboBox_AddString(hctl, utf8_2_ansi(gdmgr.heros_[number].name().c_str()));
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, number);
		if (city_ == number) {
			selected = ComboBox_GetCount(hctl) - 1;
		}
	}
	if (selected != -1) {
		ComboBox_SetCurSel(hctl, selected);
	}
}
*/

bool tmultiplayer_::dirty() const 
{
	for (std::vector<tscenario>::const_iterator it = ns::_scenario.begin(); it != ns::_scenario.end(); ++ it) {
		if (it->dirty_) {
			return true;
		}
	}
	return false;
}


void tmultiplayer::generate() const
{
	for (std::vector<tscenario>::iterator it = ns::_scenario.begin(); it != ns::_scenario.end(); ++ it) {
		if (it->dirty_) {
			it->generate();
		}
	}
}

void tmultiplayer::clear()
{
	ns::_scenario.clear();
}

void tcore::update_to_ui_multiplayer(HWND hdlgP)
{
	std::stringstream strstr;
	char text[_MAX_PATH];
	TCITEM tie;

	DLGHDR* pHdr = (DLGHDR*)GetWindowLong(hdlgP, GWL_USERDATA);
	
	if (pHdr->hwndDisplay != NULL) {
		DestroyWindow(pHdr->hwndDisplay);
		pHdr->hwndDisplay = NULL;
	}
	int index;
	for (index = 1; index < pHdr->valid_pages; index ++) {
		TabCtrl_DeleteItem(pHdr->hwndTab, 1);
	}

	pHdr->valid_pages = 1 + ns::_scenario.size();
	if (pHdr->valid_pages > pHdr->reserved_pages) {
		DLGTEMPLATE** apRes = pHdr->apRes;
		pHdr->apRes = (DLGTEMPLATE**)malloc(pHdr->valid_pages * sizeof(DLGTEMPLATE*));
		memcpy(pHdr->apRes, apRes, pHdr->reserved_pages * sizeof(DLGTEMPLATE*));
		pHdr->reserved_pages = pHdr->valid_pages;
		free(apRes);
	}

	for (index = 1; index < pHdr->valid_pages; index ++) {
		tie.mask = TCIF_TEXT | TCIF_IMAGE; 
		tie.iImage = -1;
		strstr.str("");
		strstr << std::setw(2) << std::setfill('0') << index;
		strstr << "(" << ns::_scenario[index - 1].id_ << ")";
		strcpy(text, strstr.str().c_str());
		tie.pszText = text; 
		TabCtrl_InsertItem(pHdr->hwndTab, index, &tie); 

		pHdr->apRes[index] = editor_config::DoLockDlgRes(MAKEINTRESOURCE(IDD_CAMPAIGN_SCENARIO));
	}

	// Simulate selection of the first item.
	if (TabCtrl_GetCurSel(pHdr->hwndTab)) {
		TabCtrl_SetCurSel(pHdr->hwndTab, 0);
	}
	OnSelChanged2(hdlgP);
}

bool tcore::multiplayer_dirty() const
{
	return multiplayer_.dirty();
}
/*
void tfaction::update_to_ui_row(HWND hdlgP) const
{
	std::stringstream strstr;
	HWND hctl;
	int value, index, count;
	LVITEM lvi;
	char text[_MAX_PATH];

	strstr << _("Hero") << "(" << gdmgr.heros_[leader_].name() << ")";
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_HERO), utf8_2_ansi(strstr.str().c_str()));

	// service hero
	hctl = GetDlgItem(hdlgP, IDC_LV_FACTION_SERVICE);
	count = ListView_GetItemCount(hctl);
	ListView_DeleteAllItems(hctl);
	index = 0;
	for (std::set<int>::const_iterator it = freshes_.begin(); it != freshes_.end(); ++ it) {
		hero& h = gdmgr.heros_[*it];

		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		// 姓名
		lvi.iItem = index ++;
		lvi.iSubItem = 0;
		strcpy(text, utf8_2_ansi(h.name().c_str()));
		lvi.pszText = text;
		lvi.lParam = (LPARAM)h.number_;
		ListView_InsertItem(hctl, &lvi);

		// 相性
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 1;
		strstr.str("");
		value = h.base_catalog_;
		strstr << value;
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// 特技
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 2;
		strcpy(text, utf8_2_ansi(hero::feature_str(h.feature_).c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// 战法
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 3;
		strstr.str("");
		if (h.tactic_ != HEROS_NO_TACTIC) {
			strstr << unit_types.tactic(h.tactic_).name();
		}
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);
	}

	// wander hero
	hctl = GetDlgItem(hdlgP, IDC_LV_FACTION_WANDER);
	count = ListView_GetItemCount(hctl);
	ListView_DeleteAllItems(hctl);
	index = 0;
	for (std::set<int>::const_iterator it = wanderes_.begin(); it != wanderes_.end(); ++ it) {
		hero& h = gdmgr.heros_[*it];

		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		// 姓名
		lvi.iItem = index ++;
		lvi.iSubItem = 0;
		strcpy(text, utf8_2_ansi(h.name().c_str()));
		lvi.pszText = text;
		lvi.lParam = (LPARAM)h.number_;
		ListView_InsertItem(hctl, &lvi);

		// 相性
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 1;
		strstr.str("");
		value = h.base_catalog_;
		strstr << value;
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// 特技
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 2;
		strcpy(text, utf8_2_ansi(hero::feature_str(h.feature_).c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// 战法
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 3;
		strstr.str("");
		if (h.tactic_ != HEROS_NO_TACTIC) {
			strstr << unit_types.tactic(h.tactic_).name();
		}
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);
	}	
}

bool tfaction::dirty() const
{
	if (*this != faction_from_cfg_) {
		return true;
	}
	return false;
}
*/

BOOL On_DlgMultiplayerCoreInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	HWND hwndParent = GetParent(hdlgP);
    DLGHDR *pHdr = (DLGHDR *) GetWindowLong(hwndParent, GWL_USERDATA);
    SetWindowPos(hdlgP, HWND_TOP, pHdr->rcDisplay.left, pHdr->rcDisplay.top, 0, 0, SWP_NOSIZE); 

	return FALSE;
}

BOOL On_DlgMultiplayerCoreNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	if (lpNMHdr->code == NM_RCLICK) {

	} else if (lpNMHdr->code == NM_DBLCLK) {

	}
	return FALSE;
}

BOOL CALLBACK DlgMultiplayerCoreProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_INITDIALOG:
		return On_DlgMultiplayerCoreInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_NOTIFY,  On_DlgMultiplayerCoreNotify);
	}
	
	return FALSE;
}

BOOL On_DlgMultiplayerInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	HWND hwndParent = GetParent(hdlgP); 
    DLGHDR *pHdr2 = (DLGHDR *) GetWindowLong(hwndParent, GWL_USERDATA);
    SetWindowPos(hdlgP, HWND_TOP, pHdr2->rcDisplay.left, pHdr2->rcDisplay.top, 0, 0, SWP_NOSIZE); 


	std::stringstream strstr;
	TCITEM tie;

	DLGHDR* pHdr = (DLGHDR*)GetWindowLong(hdlgP, GWL_USERDATA);
	if (!pHdr) {
		pHdr = (DLGHDR*)malloc(sizeof(DLGHDR));
		memset(pHdr, 0, sizeof(DLGHDR));
		// Save a pointer to the DLGHDR structure. 
		SetWindowLong(hdlgP, GWL_USERDATA, (LONG) pHdr); 

		pHdr->hwndTab = GetDlgItem(hdlgP, IDC_TAB_MULTIPLAYER_EXPLORER);

		RECT rc;

		// Add a tab for each of the three child dialog boxes. 
		tie.mask = TCIF_TEXT | TCIF_IMAGE; 
		tie.iImage = -1; 
		tie.pszText = "_base"; 
		TabCtrl_InsertItem(pHdr->hwndTab, 0, &tie); 

		pHdr->reserved_pages = 1;
		pHdr->apRes = (DLGTEMPLATE**)malloc(pHdr->reserved_pages * sizeof(DLGTEMPLATE*));
		pHdr->valid_pages = 1;

		// Lock the resources for the three child dialog boxes. 
		pHdr->apRes[0] = editor_config::DoLockDlgRes(MAKEINTRESOURCE(IDD_MULTIPLAYERBASE));
		
		// Calculate how large to make the tab control, so 
		// the display area can accommodate all the child dialog boxes.
		GetWindowRect(hdlgP, &rc);
		GetWindowRect(pHdr->hwndTab, &pHdr->rcDisplay);
		OffsetRect(&pHdr->rcDisplay, -1 * rc.left, -1 * rc.top);
		TabCtrl_GetItemRect(pHdr->hwndTab, 0, &rc);
		OffsetRect(&pHdr->rcDisplay, 0, rc.bottom);
	}
	return FALSE;
}

BOOL On_DlgMultiplayerNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	if (lpNMHdr->code == TCN_SELCHANGE) {
		if (lpNMHdr->idFrom == IDC_TAB_MULTIPLAYER_EXPLORER) {
			OnSelChanged2(hdlgP);
		}
	} else if (lpNMHdr->code == TCN_SELCHANGING) {
		if (lpNMHdr->idFrom == IDC_TAB_MULTIPLAYER_EXPLORER) {
			OnSelChanging2(hdlgP);
		}
	}
	return FALSE;
}

void On_DlgMultiplayerDestroy(HWND hdlgP)
{
	DLGHDR* pHdr = (DLGHDR*)GetWindowLong(hdlgP, GWL_USERDATA);
	if (pHdr) {
		if (scenario_selector::multiplayer && !ns::_scenario.empty()) {
			OnSelChanging2(hdlgP);
		}
		delete pHdr->apRes;
		delete pHdr;
	}
	return;
}

BOOL CALLBACK DlgMultiplayerProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_INITDIALOG:
		return On_DlgMultiplayerInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_NOTIFY,  On_DlgMultiplayerNotify);
	HANDLE_MSG(hdlgP, WM_DESTROY,  On_DlgMultiplayerDestroy);
	}
	
	return FALSE;
}

#endif