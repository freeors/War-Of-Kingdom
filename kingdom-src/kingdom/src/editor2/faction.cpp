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

	std::vector<std::string> vstr = utils::split(cfg["service_heros"].str());
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		number = lexical_cast_default<int>(*it);
		if (used_heros.find(number) == used_heros.end()) {
			freshes_.insert(number);
			used_heros.insert(number);
		}
	}

	vstr = utils::split(cfg["wander_heros"].str());
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		number = lexical_cast_default<int>(*it);
		if (used_heros.find(number) == used_heros.end()) {
			wanderes_.insert(number);
			used_heros.insert(number);
		}
	}

	// tower_heros must be from service_heros or wander_heros.
	vstr = utils::split(cfg["tower_heros"].str());
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		number = lexical_cast_default<int>(*it);
		if (freshes_.find(number) != freshes_.end() || wanderes_.find(number) != wanderes_.end()) {
			towers_.insert(number);
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
		if (hero::is_system(h.number_)) {
			continue;
		}
		if (hero::is_soldier(h.number_)) {
			continue;
		}
		if (hero::is_artifical(h.number_)) {
			continue;
		}
		if (hero::is_commoner(h.number_)) {
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

	// tower hero
	LVITEM lvi;
	std::stringstream strstr;
	char text[_MAX_PATH];
	hctl = GetDlgItem(hdlgP, IDC_LV_FACTIONEDIT_TOWERHERO);
	ListView_DeleteAllItems(hctl);
	int row = 0;
	for (std::set<int>::const_iterator it = towers_.begin(); it != towers_.end(); ++ it) {
		hero& h = gdmgr.heros_[*it];

		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		// 姓名
		lvi.iItem = row ++;
		lvi.iSubItem = 0;
		strcpy(text, utf8_2_ansi(h.name().c_str()));
		lvi.pszText = text;
		lvi.lParam = (LPARAM)h.number_;
		ListView_InsertItem(hctl, &lvi);

		// types
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 1;
		strstr.str("");
		if (h.utype_ != HEROS_NO_UTYPE) {
			const unit_type* ut = unit_types.keytype(h.utype_);
			strstr << ut->type_name();
		} else {
			strstr << "--";
		}
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
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

		// 统帅
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 4;
		sprintf(text, "%u.%u", fxptoi9(h.leadership_), fxpmod9(h.leadership_));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);
	}
}

std::string tfaction::generate() const
{
	std::stringstream strstr;

	strstr << "[faction]\n";
	strstr << "\tleader=" << leader_ << "\n";

	strstr << "\tservice_heros = ";
	for (std::set<int>::const_iterator it = freshes_.begin(); it != freshes_.end(); ++ it) {
		if (it == freshes_.begin()) {
			strstr << *it;
		} else {
			strstr << ", " << *it;
		}
	}
	strstr << "\n";
	strstr << "\twander_heros = ";
	for (std::set<int>::const_iterator it = wanderes_.begin(); it != wanderes_.end(); ++ it) {
		if (it == wanderes_.begin()) {
			strstr << *it;
		} else {
			strstr << ", " << *it;
		}
	}
	strstr << "\n";
	strstr << "\ttower_heros = ";
	for (std::set<int>::const_iterator it = towers_.begin(); it != towers_.end(); ++ it) {
		if (it == towers_.begin()) {
			strstr << *it;
		} else {
			strstr << ", " << *it;
		}
	}
	strstr << "\n";
	
	strstr << "\t{ANONYMITY_LOYAL_MERITORIOUS_CITY_LOW 0 0 city1 1 1 ";
	strstr << "(" << city_ << ")}\n";

	strstr << "[/faction]\n";
	strstr << "\n";

	return strstr.str();
}

void tcore::update_to_ui_faction(HWND hdlgP, int clicked_faction)
{
	HWND hctl = GetDlgItem(hdlgP, IDC_LV_FACTION_EXPLORER);
	LVITEM lvi;
	ListView_DeleteAllItems(hctl);
	std::stringstream strstr;
	char text[_MAX_PATH];
	int value, row = 0;
	std::set<int> allocated;

	for (std::vector<tfaction>::const_iterator it = factions_updating_.begin(); it != factions_updating_.end(); ++ it) {
		const tfaction& f = *it;
		int column = 0;

		allocated.insert(f.freshes_.begin(), f.freshes_.end());
		allocated.insert(f.wanderes_.begin(), f.wanderes_.end());

		lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
		// number
		lvi.iItem = row;
		lvi.iSubItem = column ++;
		strstr.str("");
		strstr << row ++;
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		lvi.iImage = select_iimage_according_fname(text, 0);
		lvi.lParam = (LPARAM)0;
		ListView_InsertItem(hctl, &lvi);

		// leader
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = column ++;
		strstr.str("");
		strstr << gdmgr.heros_[f.leader_].name();
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// city
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = column ++;
		strstr.str("");
		strstr << gdmgr.heros_[f.city_].name();
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// fresh heros
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = column ++;
		strstr.str("");
		strstr << f.freshes_.size();
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// wander heros
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = column ++;
		strstr.str("");
		strstr << f.wanderes_.size();
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// tower heros
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = column ++;
		strstr.str("");
		for (std::set<int>::const_iterator it2 = f.towers_.begin(); it2 != f.towers_.end(); ++ it2) {
			if (it2 != f.towers_.begin()) {
				strstr << ", ";
			}
			strstr << gdmgr.heros_[*it2].name();
		}
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);
	}
	
	// candidate hero
	hctl = GetDlgItem(hdlgP, IDC_LV_FACTION_CANDIDATE);
	ListView_DeleteAllItems(hctl);
	row = 0;
	for (hero_map::iterator it = gdmgr.heros_.begin(); it != gdmgr.heros_.end(); ++ it) {
		hero& h = *it;

		if (hero::is_system(h.number_) || hero::is_soldier(h.number_) || hero::is_commoner(h.number_)) {
			continue;
		}
		if (h.gender_ == hero_gender_neutral) {
			continue;
		}
		if (h.get_flag(hero_flag_roam) || h.get_flag(hero_flag_robber) || h.get_flag(hero_flag_employee)) {
			continue;;
		}
		if (allocated.find(h.number_) != allocated.end()) {
			continue;
		}

		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		// 姓名
		lvi.iItem = row ++;
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

		// 势力特技
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 3;
		strcpy(text, utf8_2_ansi(hero::feature_str(h.side_feature_).c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// 战法
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 4;
		strstr.str("");
		if (h.tactic_ != HEROS_NO_TACTIC) {
			strstr << unit_types.tactic(h.tactic_).name();
		}
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// 统帅
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 5;
		sprintf(text, "%u.%u", fxptoi9(h.leadership_), fxpmod9(h.leadership_));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);
	}

	if (clicked_faction == -1) {
		ns::clicked_faction = 0;
		clicked_faction = 0;
	}
	if (!factions_updating_.empty()) {
		factions_updating_[clicked_faction].update_to_ui_row(hdlgP);
	}
}

bool tcore::factions_dirty() const
{
	return ns::core.factions_updating_ != ns::core.factions_from_cfg_;
}

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

BOOL On_DlgFactionInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	HWND hwndParent = GetParent(hdlgP); 
    DLGHDR *pHdr = (DLGHDR *) GetWindowLong(hwndParent, GWL_USERDATA);
    SetWindowPos(hdlgP, HWND_TOP, pHdr->rcDisplay.left, pHdr->rcDisplay.top, 0, 0, SWP_NOSIZE); 

	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_CANDIDATE), utf8_2_ansi(_("Candidate")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_OFFICE), utf8_2_ansi(_("Office")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_WANDER), utf8_2_ansi(_("Wander")));

	HWND hctl = GetDlgItem(hdlgP, IDC_LV_FACTION_EXPLORER);
	LVCOLUMN lvc;
	char text[_MAX_PATH];
	std::stringstream strstr;
	int column = 0;

	//
	// factions
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
	lvc.cx = 80;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << _("Leader");
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 80;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << _("City");
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 120;
	lvc.iSubItem = column;
	strcpy(text, dgettext_2_ansi("wesnoth-lib", "Fresh Hero"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 90;
	lvc.iSubItem = column;
	strcpy(text, dgettext_2_ansi("wesnoth-lib", "Wander Hero"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 300;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << _("Tower hero");
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	ListView_SetImageList(hctl, gdmgr._himl, LVSIL_SMALL);
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	gdmgr._hpopup_candidate = CreatePopupMenu();
	AppendMenu(gdmgr._hpopup_candidate, MF_STRING, IDM_TOSERVICE, "到在职");
	AppendMenu(gdmgr._hpopup_candidate, MF_STRING, IDM_TOWANDER, "到在野");

	hctl = GetDlgItem(hdlgP, IDC_LV_FACTION_CANDIDATE);
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 50;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "name"));
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = 0;
	ListView_InsertColumn(hctl, 0, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 40;
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

	lvc.cx = 60;
	lvc.iSubItem = 4;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "tactic"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 4, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = 5;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "leadership"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 5, &lvc);

	// ListView_SetImageList(hctl, gdmgr._himl, LVSIL_SMALL);
	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);

	// 默认情况下，鼠标右键只是光亮该行的最前一个子项，并且只有在该子项上才能触发NM_RCLICK。改为光亮整行，并且在整行都能触发NM_RCLICK。
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	// service hero
	hctl = GetDlgItem(hdlgP, IDC_LV_FACTION_SERVICE);
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 50;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "name"));
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = 0;
	ListView_InsertColumn(hctl, 0, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 35;
	lvc.iSubItem = 1;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "catalog"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 1, &lvc);

	lvc.cx = 45;
	lvc.iSubItem = 2;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "feature"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 2, &lvc);

	lvc.cx = 60;
	lvc.iSubItem = 3;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "tactic"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 3, &lvc);

	// ListView_SetImageList(hctl, gdmgr._himl, LVSIL_SMALL);
	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);

	// 默认情况下，鼠标右键只是光亮该行的最前一个子项，并且只有在该子项上才能触发NM_RCLICK。改为光亮整行，并且在整行都能触发NM_RCLICK。
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	// wander hero
	hctl = GetDlgItem(hdlgP, IDC_LV_FACTION_WANDER);
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 50;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "name"));
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = 0;
	ListView_InsertColumn(hctl, 0, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 35;
	lvc.iSubItem = 1;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "catalog"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 1, &lvc);

	lvc.cx = 45;
	lvc.iSubItem = 2;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "feature"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 2, &lvc);

	lvc.cx = 60;
	lvc.iSubItem = 3;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "tactic"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 3, &lvc);

	// ListView_SetImageList(hctl, gdmgr._himl, LVSIL_SMALL);
	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);

	// 默认情况下，鼠标右键只是光亮该行的最前一个子项，并且只有在该子项上才能触发NM_RCLICK。改为光亮整行，并且在整行都能触发NM_RCLICK。
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	return FALSE;
}

BOOL On_DlgFactionEditInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	std::stringstream strstr;
	if (ns::action_faction == ma_edit) {
		strstr << _("Edit faction");
	} else {
		strstr << _("Add faction");
	}
	strstr << "(" << _("Number") << ":" << ns::clicked_faction << ")";
	SetWindowText(hdlgP, utf8_2_ansi(strstr.str().c_str()));

	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_LEADER), utf8_2_ansi(_("Leader")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_CITY), utf8_2_ansi(_("City")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_TOWERHERO), utf8_2_ansi(_("Tower hero")));
	
	LVCOLUMN lvc;
	char text[_MAX_PATH];
	HWND hctl = GetDlgItem(hdlgP, IDC_LV_FACTIONEDIT_TOWERHERO);
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 60;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "name"));
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = 0;
	ListView_InsertColumn(hctl, 0, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 90;
	lvc.iSubItem = 1;
	strcpy(text, utf8_2_ansi(_("arms^Type")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 1, &lvc);

	lvc.cx = 60;
	lvc.iSubItem = 2;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "feature"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 2, &lvc);

	lvc.cx = 65;
	lvc.iSubItem = 3;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "tactic"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 3, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = 4;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "leadership"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 4, &lvc);

	// ListView_SetImageList(hctl, gdmgr._himl, LVSIL_SMALL);
	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	tfaction& f = ns::core.factions_updating_[ns::clicked_faction];
	f.update_to_ui_faction_edit(hdlgP);

	return FALSE;
}

void On_DlgFactionEditCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;
	tfaction& f = ns::core.factions_updating_[ns::clicked_faction];

	switch (id) {
	case IDM_DELETE_ITEM0:
		f.towers_.erase(ns::clicked_hero);
		f.update_to_ui_faction_edit(hdlgP);
		ns::core.set_dirty(tcore::BIT_FACTION, ns::core.factions_dirty());
		break;

	case IDOK:
		changed = TRUE;
		ns::core.factions_updating_[ns::clicked_faction].from_ui_faction_edit(hdlgP);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

void factionedit_notify_handler_rclick(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	LVITEM					lvi;
	LPNMITEMACTIVATE		lpnmitem;

	if (lpNMHdr->idFrom != IDC_LV_FACTIONEDIT_TOWERHERO) {
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

	// menu item: delete2
	HMENU hpopup_delete2 = CreatePopupMenu();
	AppendMenu(hpopup_delete2, MF_STRING, IDM_DELETE_ITEM0, utf8_2_ansi(_("Delete")));
	
	if (lpnmitem->iItem < 0) {
		EnableMenuItem(hpopup_delete2, IDM_DELETE_ITEM0, MF_BYCOMMAND | MF_GRAYED);
	}

	TrackPopupMenuEx(hpopup_delete2, 0, 
		point.x, 
		point.y, 
		hdlgP, 
		NULL);

	DestroyMenu(hpopup_delete2);
	
	ns::type = DlgItem;
	ns::clicked_hero = lvi.lParam;

    return;
}

BOOL On_DlgFactionEditNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	if (lpNMHdr->code == NM_RCLICK) {
		factionedit_notify_handler_rclick(hdlgP, DlgItem, lpNMHdr);
	}
	return FALSE;
}

BOOL CALLBACK DlgFactionEditProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message) {
	case WM_INITDIALOG:
		return On_DlgFactionEditInitDialog(hDlg, (HWND)(wParam), lParam);
	HANDLE_MSG(hDlg, WM_COMMAND, On_DlgFactionEditCommand);
	HANDLE_MSG(hDlg, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	HANDLE_MSG(hDlg, WM_NOTIFY, On_DlgFactionEditNotify);
	}
	
	return FALSE;
}

void OnFactionAddBt(HWND hdlgP)
{
	ns::core.factions_updating_.push_back(tfaction());
	ns::clicked_faction = ns::core.factions_updating_.size() - 1;

	ns::core.update_to_ui_faction(hdlgP, ns::clicked_faction);
	ns::core.set_dirty(tcore::BIT_FACTION, ns::core.factions_dirty()); 
}

void OnFactionDelBt(HWND hdlgP)
{
	std::vector<tfaction>::iterator it = ns::core.factions_updating_.begin();
	std::advance(it, ns::clicked_faction);
	ns::core.factions_updating_.erase(it);

	int clicked = ns::clicked_faction;
	if (clicked >= (int)ns::core.factions_updating_.size()) {
		clicked = -1;
	}
	ns::core.update_to_ui_faction(hdlgP, clicked);
	ns::core.set_dirty(tcore::BIT_FACTION, ns::core.factions_dirty());
}

void OnFactionEditBt(HWND hdlgP)
{
	RECT rcBtn;
	LPARAM lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_LV_FACTION_EXPLORER), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	ns::action_faction = ma_edit;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_FACTIONEDIT), hdlgP, DlgFactionEditProc, lParam)) {
		ns::core.update_to_ui_faction(hdlgP, ns::clicked_faction);
		ns::core.set_dirty(tcore::BIT_FACTION, ns::core.factions_dirty());
	}

	return;
}

void On_DlgFactionCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	tfaction& f = ns::core.factions_updating_[ns::clicked_faction];

	switch (id) {
	case IDM_ADD:
		OnFactionAddBt(hdlgP);
		break;
	case IDM_DELETE:
		OnFactionDelBt(hdlgP);
		break;
	case IDM_EDIT:
		OnFactionEditBt(hdlgP);
		break;

	case IDM_TOSERVICE:
	case IDM_TOWANDER:
		if (id == IDM_TOSERVICE) {
			f.freshes_.insert(ns::clicked_hero);
		} else {
			f.wanderes_.insert(ns::clicked_hero);
		}
		ns::core.update_to_ui_faction(hdlgP, ns::clicked_faction);
		ns::core.set_dirty(tcore::BIT_FACTION, ns::core.factions_dirty());
		break;

	case IDM_TOTOWER:
		f.towers_.insert(ns::clicked_hero);

		ns::core.update_to_ui_faction(hdlgP, ns::clicked_faction);
		ns::core.set_dirty(tcore::BIT_FACTION, ns::core.factions_dirty());
		break;

	case IDM_DELETE_ITEM0:
		if (ns::type == IDC_LV_FACTION_SERVICE) {
			if (f.leader_ == ns::clicked_hero) {
				f.leader_ = HEROS_INVALID_NUMBER;
			}
			f.freshes_.erase(ns::clicked_hero);
		} else if (ns::type == IDC_LV_FACTION_WANDER) {
			f.wanderes_.erase(ns::clicked_hero);
		}
		if (f.towers_.find(ns::clicked_hero) != f.towers_.end()) {
			f.towers_.erase(ns::clicked_hero);
		}
		ns::core.update_to_ui_faction(hdlgP, ns::clicked_faction);
		ns::core.set_dirty(tcore::BIT_FACTION, ns::core.factions_dirty());
		break;
	case IDM_DELETE_ITEM1:
		if (ns::type == IDC_LV_FACTION_SERVICE) {
			f.leader_ = HEROS_INVALID_NUMBER;
			f.freshes_.clear();
		} else if (ns::type == IDC_LV_FACTION_WANDER) {
			f.wanderes_.clear();
		}
		if (f.towers_.find(ns::clicked_hero) != f.towers_.end()) {
			f.towers_.erase(ns::clicked_hero);
		}
		ns::core.update_to_ui_faction(hdlgP, ns::clicked_faction);
		ns::core.set_dirty(tcore::BIT_FACTION, ns::core.factions_dirty());
		break;
	}
	return;
}

void faction_notify_handler_click(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	LVITEM					lvi;
	LPNMITEMACTIVATE		lpnmitem;

	lpnmitem = (LPNMITEMACTIVATE)lpNMHdr;
	
	lvi.iItem = lpnmitem->iItem;
	lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
	lvi.iSubItem = 0;
	lvi.pszText = gdmgr._menu_text;
	lvi.cchTextMax = _MAX_PATH;
	ListView_GetItem(lpNMHdr->hwndFrom, &lvi);

	if (lpnmitem->iItem >= 0) {
		if (lpNMHdr->idFrom == IDC_LV_FACTION_EXPLORER) {
			ns::clicked_faction = lpnmitem->iItem;
			ns::core.factions_updating_[ns::clicked_faction].update_to_ui_row(hdlgP);
		}
	}
    return;
}

void faction_notify_handler_rclick(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	LVITEM					lvi;
	LPNMITEMACTIVATE		lpnmitem;
	int						icount;

	if (lpNMHdr->idFrom != IDC_LV_FACTION_EXPLORER &&
		lpNMHdr->idFrom != IDC_LV_FACTION_CANDIDATE &&
		lpNMHdr->idFrom != IDC_LV_FACTION_SERVICE &&
		lpNMHdr->idFrom != IDC_LV_FACTION_WANDER) {
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

	icount = ListView_GetItemCount(lpNMHdr->hwndFrom);

	if (lpNMHdr->idFrom == IDC_LV_FACTION_EXPLORER) {
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

		EnableMenuItem(gdmgr._hpopup_editor, IDM_ADD, MF_BYCOMMAND | MF_ENABLED);
		EnableMenuItem(gdmgr._hpopup_editor, IDM_EDIT, MF_BYCOMMAND | MF_ENABLED);
		EnableMenuItem(gdmgr._hpopup_editor, IDM_DELETE, MF_BYCOMMAND | MF_ENABLED);

		if (lpnmitem->iItem >= 0) {
			ns::clicked_faction = lpnmitem->iItem;
		}
		return;

	} else if (lpNMHdr->idFrom == IDC_LV_FACTION_CANDIDATE) {
		if (lpnmitem->iItem < 0) {
			EnableMenuItem(gdmgr._hpopup_candidate, IDM_TOSERVICE, MF_BYCOMMAND | MF_GRAYED);
			EnableMenuItem(gdmgr._hpopup_candidate, IDM_TOWANDER, MF_BYCOMMAND | MF_GRAYED);
		}		

		TrackPopupMenuEx(gdmgr._hpopup_candidate, 0, 
			point.x, 
			point.y, 
			hdlgP, 
			NULL);

		// 恢复回去
		EnableMenuItem(gdmgr._hpopup_candidate, IDM_TOSERVICE, MF_BYCOMMAND | MF_ENABLED);
		EnableMenuItem(gdmgr._hpopup_candidate, IDM_TOWANDER, MF_BYCOMMAND | MF_ENABLED);
	} else {
		// menu item: delete2
		HMENU hpopup_delete2 = CreatePopupMenu();
		AppendMenu(hpopup_delete2, MF_STRING, IDM_TOTOWER, utf8_2_ansi(_("To tower hero")));
		AppendMenu(hpopup_delete2, MF_SEPARATOR, 0, NULL);

		AppendMenu(hpopup_delete2, MF_STRING, IDM_DELETE_ITEM0, utf8_2_ansi(_("Delete")));
		AppendMenu(hpopup_delete2, MF_SEPARATOR, 0, NULL);
		AppendMenu(hpopup_delete2, MF_STRING, IDM_DELETE_ITEM1, utf8_2_ansi(_("Delete all")));
		AppendMenu(hpopup_delete2, MF_SEPARATOR, 0, NULL);
	
		const std::set<int>& towers = ns::core.factions_updating_[ns::clicked_faction].towers_;

		if (lpnmitem->iItem < 0) {
			EnableMenuItem(hpopup_delete2, IDM_TOTOWER, MF_BYCOMMAND | MF_GRAYED);
			EnableMenuItem(hpopup_delete2, IDM_DELETE_ITEM0, MF_BYCOMMAND | MF_GRAYED);
			EnableMenuItem(hpopup_delete2, IDM_DELETE_ITEM1, MF_BYCOMMAND | MF_GRAYED);

		} else if (towers.size() >= 4 || towers.find(lvi.lParam) != towers.end()) {
			EnableMenuItem(hpopup_delete2, IDM_TOTOWER, MF_BYCOMMAND | MF_GRAYED);
		}

		TrackPopupMenuEx(hpopup_delete2, 0, 
			point.x, 
			point.y, 
			hdlgP, 
			NULL);

		DestroyMenu(hpopup_delete2);
	}
	ns::type = DlgItem;
	ns::clicked_hero = lvi.lParam;

    return;
}

void faction_notify_handler_dblclk(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
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
		if (lpNMHdr->idFrom == IDC_LV_FACTION_EXPLORER) {
			ns::clicked_faction = lpnmitem->iItem;
			OnFactionEditBt(hdlgP);
		}
	}
    return;
}

BOOL On_DlgFactionNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	if (lpNMHdr->code == NM_CLICK) {
		faction_notify_handler_click(hdlgP, DlgItem, lpNMHdr);
	} else if (lpNMHdr->code == NM_RCLICK) {
		faction_notify_handler_rclick(hdlgP, DlgItem, lpNMHdr);
	} else if (lpNMHdr->code == NM_DBLCLK) {
		faction_notify_handler_dblclk(hdlgP, DlgItem, lpNMHdr);
	}
	return FALSE;
}

void On_DlgFactionDestroy(HWND hdlgP)
{
	DestroyMenu(gdmgr._hpopup_candidate);
	return;
}

BOOL CALLBACK DlgFactionProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_INITDIALOG:
		return On_DlgFactionInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgFactionCommand);
	HANDLE_MSG(hdlgP, WM_NOTIFY,  On_DlgFactionNotify);
	HANDLE_MSG(hdlgP, WM_DESTROY, On_DlgFactionDestroy);
	}
	
	return FALSE;
}

#endif