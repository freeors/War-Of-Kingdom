#define GETTEXT_DOMAIN "wesnoth-maker"

#include "stdafx.h"
#include <windowsx.h>
#include <string.h>
#include "win32x.h"
#include "resource.h"

#include "serialization/parser.hpp"
#include <boost/foreach.hpp>
#include "struct.h"
#include "dlgcoreproc.hpp"
#include "gettext.hpp"

BOOL CALLBACK DlgBruleNoneProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK DlgBruleNormalProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

namespace ns {
	int clicked_rule;
	int clicked_tile;
	int clicked_image;
	
	int action_rule;
}

std::map<const std::string, int> tbuilding_rule::macro_tags;
std::map<int, int> tbuilding_rule::idd_map;
std::map<int, DLGPROC> tbuilding_rule::dlgproc_map;

int tbuilding_rule::macro_type(const std::string& tag)
{
	std::map<const std::string, int>::const_iterator it = macro_tags.find(tag);
	if (it != macro_tags.end()) {
		return it->second;
	}
	return NONE;
}

const std::string& tbuilding_rule::macro_tag(int type)
{
	for (std::map<const std::string, int>::const_iterator it = macro_tags.begin(); it != macro_tags.end(); ++ it) {
		if (it->second == type) {
			return it->first;
		}
	}
	return null_str;
}

tbuilding_rule::tbuilding_rule(int type)
	: type_(type)
	, postfix_()
{
	if (macro_tags.empty()) {
		macro_tags.insert(std::make_pair("NONE", (int)NONE));
		macro_tags.insert(std::make_pair("TRANSITION_COMPLETE_LF", (int)TRANSITION_COMPLETE_LF));
		macro_tags.insert(std::make_pair("TRANSITION_COMPLETE_LB", (int)TRANSITION_COMPLETE_LB));
		macro_tags.insert(std::make_pair("TRANSITION_COMPLETE_L", (int)TRANSITION_COMPLETE_L));
		macro_tags.insert(std::make_pair("EDITOR_OVERLAY", (int)EDITOR_OVERLAY));
		macro_tags.insert(std::make_pair("BRIDGE:STRAIGHTS", (int)BRIDGE_STRAIGHTS));
		macro_tags.insert(std::make_pair("BRIDGE:ENDS", (int)BRIDGE_ENDS));
		macro_tags.insert(std::make_pair("BRIDGE:JOINTS", (int)BRIDGE_JOINTS));
		macro_tags.insert(std::make_pair("BRIDGE:CORNERS", (int)BRIDGE_CORNERS));
		macro_tags.insert(std::make_pair("LAYOUT_TRACKS_F", (int)LAYOUT_TRACKS_F));
		macro_tags.insert(std::make_pair("TRACK_COMPLETE", (int)TRACK_COMPLETE));
		macro_tags.insert(std::make_pair("TRACK_BORDER_RESTRICTED_PLF", (int)TRACK_BORDER_RESTRICTED_PLF));
		macro_tags.insert(std::make_pair("NEW:FOREST", (int)NEW_FOREST));
		macro_tags.insert(std::make_pair("OVERLAY_RANDOM", (int)OVERLAY_RANDOM));
		macro_tags.insert(std::make_pair("OVERLAY_RANDOM_LF", (int)OVERLAY_RANDOM_LF));
		macro_tags.insert(std::make_pair("OVERLAY_RANDOM_L", (int)OVERLAY_RANDOM_L));
		macro_tags.insert(std::make_pair("OVERLAY_LF", (int)OVERLAY_LF));
		macro_tags.insert(std::make_pair("OVERLAY_P", (int)OVERLAY_P));
		macro_tags.insert(std::make_pair("OVERLAY_F", (int)OVERLAY_F));
		macro_tags.insert(std::make_pair("OVERLAY_B", (int)OVERLAY_B));
		macro_tags.insert(std::make_pair("OVERLAY", (int)OVERLAY));
		macro_tags.insert(std::make_pair("TERRAIN_BASE_RANDOM", (int)TERRAIN_BASE_RANDOM));
		macro_tags.insert(std::make_pair("TERRAIN_BASE_P", (int)TERRAIN_BASE_P));
		macro_tags.insert(std::make_pair("TERRAIN_BASE_F", (int)TERRAIN_BASE_F));
		macro_tags.insert(std::make_pair("TERRAIN_BASE", (int)TERRAIN_BASE));
		macro_tags.insert(std::make_pair("KEEP_BASE_RANDOM", (int)KEEP_BASE_RANDOM));
		macro_tags.insert(std::make_pair("KEEP_BASE", (int)KEEP_BASE));
		macro_tags.insert(std::make_pair("TERRAIN_BASE_SINGLEHEX_LB", (int)TERRAIN_BASE_SINGLEHEX_LB));
		macro_tags.insert(std::make_pair("TERRAIN_BASE_SINGLEHEX_B", (int)TERRAIN_BASE_SINGLEHEX_B));
		macro_tags.insert(std::make_pair("OVERLAY_COMPLETE_LF", (int)OVERLAY_COMPLETE_LF));
		macro_tags.insert(std::make_pair("OVERLAY_COMPLETE_L", (int)OVERLAY_COMPLETE_L));
		macro_tags.insert(std::make_pair("OVERLAY_COMPLETE_F", (int)OVERLAY_COMPLETE_F));
		macro_tags.insert(std::make_pair("OVERLAY_COMPLETE", (int)OVERLAY_COMPLETE));
		macro_tags.insert(std::make_pair("VOLCANO_2x2", (int)VOLCANO_2x2));
		macro_tags.insert(std::make_pair("OVERLAY_RESTRICTED3_F", (int)OVERLAY_RESTRICTED3_F));
		macro_tags.insert(std::make_pair("OVERLAY_RESTRICTED2_F", (int)OVERLAY_RESTRICTED2_F));
		macro_tags.insert(std::make_pair("OVERLAY_RESTRICTED_P", (int)OVERLAY_RESTRICTED_P));
		macro_tags.insert(std::make_pair("OVERLAY_RESTRICTED_F", (int)OVERLAY_RESTRICTED_F));
		macro_tags.insert(std::make_pair("OVERLAY_ROTATION_RESTRICTED2_F", (int)OVERLAY_ROTATION_RESTRICTED2_F));
		macro_tags.insert(std::make_pair("OVERLAY_ROTATION_RESTRICTED_F", (int)OVERLAY_ROTATION_RESTRICTED_F));

		macro_tags.insert(std::make_pair("MOUNTAINS_2x4_NW_SE", (int)MOUNTAINS_2x4_NW_SE));
		macro_tags.insert(std::make_pair("MOUNTAINS_2x4_SW_NE", (int)MOUNTAINS_2x4_SW_NE));
		macro_tags.insert(std::make_pair("MOUNTAINS_1x3_NW_SE", (int)MOUNTAINS_1x3_NW_SE));
		macro_tags.insert(std::make_pair("MOUNTAINS_1x3_SW_NE", (int)MOUNTAINS_1x3_SW_NE));
		macro_tags.insert(std::make_pair("MOUNTAINS_2x2", (int)MOUNTAINS_2x2));
		macro_tags.insert(std::make_pair("MOUNTAIN_SINGLE_RANDOM", (int)MOUNTAIN_SINGLE_RANDOM));
		macro_tags.insert(std::make_pair("PEAKS_1x2_SW_NE", (int)PEAKS_1x2_SW_NE));
		macro_tags.insert(std::make_pair("PEAKS_LARGE", (int)PEAKS_LARGE));
		macro_tags.insert(std::make_pair("NEW:VILLAGE", (int)VILLAGE));
		macro_tags.insert(std::make_pair("NEW:VILLAGE_A3", (int)VILLAGE_A3));
		macro_tags.insert(std::make_pair("NEW:VILLAGE_A4", (int)VILLAGE_A4));
		macro_tags.insert(std::make_pair("NEW:FENCE", (int)NEW_FENCE));
		macro_tags.insert(std::make_pair("NEW:WALL_P", (int)NEW_WALL_P));
		macro_tags.insert(std::make_pair("NEW:WALL", (int)NEW_WALL));
		macro_tags.insert(std::make_pair("NEW:WALL2_P", (int)NEW_WALL2_P));
		macro_tags.insert(std::make_pair("NEW:WALL2", (int)NEW_WALL2));
		macro_tags.insert(std::make_pair("NEW:WAVES", (int)NEW_WAVES));
		macro_tags.insert(std::make_pair("NEW:BEACH", (int)NEW_BEACH));
		macro_tags.insert(std::make_pair("WALL_TRANSITION3", (int)WALL_TRANSITION3));
		macro_tags.insert(std::make_pair("WALL_TRANSITION2_LF", (int)WALL_TRANSITION2_LF));
		macro_tags.insert(std::make_pair("WALL_TRANSITION_LF", (int)WALL_TRANSITION_LF));
		macro_tags.insert(std::make_pair("WALL_ADJACENT_TRANSITION", (int)WALL_ADJACENT_TRANSITION));
		macro_tags.insert(std::make_pair("ANIMATED_WATER_15", (int)ANIMATED_WATER_15));
		macro_tags.insert(std::make_pair("ANIMATED_WATER_15_TRANSITION", (int)ANIMATED_WATER_15_TRANSITION));
		macro_tags.insert(std::make_pair("DISABLE_BASE_TRANSITIONS", (int)DISABLE_BASE_TRANSITIONS));
																												
	}
	if (idd_map.empty()) {
		idd_map[NONE] = IDD_BRULE_NONE;
		for (int i = NORMAL_FIRST; i <= NORMAL_LAST; i ++) {
			idd_map[i] = IDD_BRULE_NORMAL;
		}
		
	}
	if (dlgproc_map.empty()) {
		dlgproc_map[NONE] = DlgBruleNoneProc;
		for (int i = NORMAL_FIRST; i <= NORMAL_LAST; i ++) {
			dlgproc_map[i] = DlgBruleNormalProc;
		}
	}
}

void tbuilding_rule::from_config(const config& cfg, const std::string& postfix) 
{ 
	cfg_ = cfg; 
	postfix_ = postfix;
}

void tbuilding_rule::from_ui_brule_edit(HWND hdlgP)
{
	char text[_MAX_PATH];
	int i = 0;
	std::map<std::string, std::string> diff;
	BOOST_FOREACH (const config::attribute &istrmap, cfg_.attribute_range()) {
		Static_GetText(GetDlgItem(hdlgP, IDC_ET_BRULENORMAL_PARAM0 + i), text, MAX_PATH);
		if (text != istrmap.second) {
			diff.insert(std::make_pair(istrmap.first, text));
		}
		i ++;
	}
	for (std::map<std::string, std::string>::const_iterator it = diff.begin(); it != diff.end(); ++ it) {
		cfg_[it->first] = it->second;
	}
}

void tbuilding_rule::update_to_ui_brule_edit(HWND hdlgP) const
{
	static int param_count = 10;

	std::string name;
	int i = 0;
	BOOST_FOREACH (const config::attribute &istrmap, cfg_.attribute_range()) {
		size_t pos = istrmap.first.find("_");
		if (pos != std::string::npos) {
			name = istrmap.first.substr(pos);
		} else {
			name = istrmap.first;
		}
		Edit_SetText(GetDlgItem(hdlgP, IDC_STATIC_PARAM0 + i), name.c_str());
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_BRULENORMAL_PARAM0 + i), istrmap.second.str().c_str());
		i ++;
	}
	for (; i < param_count; i ++) {
		ShowWindow(GetDlgItem(hdlgP, IDC_STATIC_PARAM0 + i), SW_HIDE);
		ShowWindow(GetDlgItem(hdlgP, IDC_ET_BRULENORMAL_PARAM0 + i), SW_HIDE);
	}
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_BRULENORMAL_POSTFIX), utf8_2_ansi(postfix_.c_str()));
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_BRULENORMAL_DESCRIPTION), utf8_2_ansi(description().c_str()));
}

std::string tbuilding_rule::generate(bool tpl) const
{
	std::stringstream strstr;
	if (!tpl) {
		strstr << "{";
		strstr << macro_tag(type_);
		BOOST_FOREACH (const config::attribute &istrmap, cfg_.attribute_range()) {
			strstr << " ";
			bool quot = !istrmap.second.str().empty() &&  istrmap.second.str().at(0) == '~';
			if (quot) {
				strstr << "\"";
			}
			strstr << istrmap.second;
			if (quot) {
				strstr << "\"";
			}
		}
		strstr << "}";
		if (!postfix_.empty()) {
			strstr << postfix_;
		}
	} else {
		config cfg;
		config& sub = cfg.add_child("macro");
		sub["type"] = macro_tag(type_);
		if (!postfix_.empty()) {
			sub["postfix"] = postfix_;
		}
		sub.add_child("cfg", cfg_);
		::write(strstr, cfg);
	}

	return strstr.str();
}

tnone_brule::tnone_brule()
	: tbuilding_rule(NONE)
{
}

std::pair<std::string, std::string> tnone_brule::get_flag(const config& tile) const
{
	if (tile.has_attribute("set_no_flag")) {
		return std::make_pair("set_no_flag", tile["set_no_flag"].str());
	}
	return std::make_pair("", "");
}

static bool is_return_char(char c)
{
	return c == '\r';
}

std::string get_rid_of_return(const std::string& str)
{
	std::string ret = str;
	ret.erase(std::remove_if(ret.begin(), ret.end(), is_return_char), ret.end());
	return ret;
}

std::string insert_return(const std::string& str)
{
	size_t n = 0;
	std::stringstream strstr;
	const std::vector<std::string> vstr = utils::split(str, '\n', 0);
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it, n ++) {
		if (it != vstr.begin()) {
			if (it->empty() || it->at(it->size() - 1) != '\r') {
				strstr << '\r';
			}
			strstr << '\n';
		}
		strstr << *it;
	}
	return strstr.str();
}

void tnone_brule::from_ui_brule_edit(HWND hdlgP)
{
	char text[_MAX_PATH];

	Static_GetText(GetDlgItem(hdlgP, IDC_ET_BRULENONE_MAP), text, _MAX_PATH);
	cfg_["map"] = get_rid_of_return(text);
}

void tnone_brule::update_to_ui_brule_edit(HWND hdlgP) const
{
	Static_SetText(GetDlgItem(hdlgP, IDC_ET_BRULENONE_MAP), insert_return(cfg_["map"].str()).c_str());

	std::stringstream strstr;
	// tile
	LVCOLUMN lvc;
	LVITEM lvi;
	char text[_MAX_PATH];
	int column = 0;

	HWND hctl = GetDlgItem(hdlgP, IDC_LV_BRULENONE_TILE);
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 30;
	strcpy(text, "pos");
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = column;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 80;
	lvc.iSubItem = column;
	strcpy(text, "type");
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 200;
	lvc.iSubItem = column;
	strstr.str("");
	strcpy(text, "flag");
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	BOOST_FOREACH (const config& tile, cfg_.child_range("tile")) {
		column = 0;

		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		// pos
		lvi.iItem = ListView_GetItemCount(hctl);
		lvi.iSubItem = column ++;
		strstr.str("");
		strstr << tile["pos"].to_int();
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		lvi.lParam = (LPARAM)0;
		ListView_InsertItem(hctl, &lvi);

		// type
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = column ++;
		strstr.str("");
		strstr << tile["type"].str();
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// flag
		strstr.str("");
		std::pair<std::string, std::string> flag = get_flag(tile);
		if (!flag.first.empty()) {
			lvi.mask = LVIF_TEXT;
			lvi.iSubItem = column ++;
			strstr << flag.first << " = " << flag.second;
			strcpy(text, utf8_2_ansi(strstr.str().c_str()));
			lvi.pszText = text;
			ListView_SetItem(hctl, &lvi);
		}
	}

	// image
	column = 0;
	hctl = GetDlgItem(hdlgP, IDC_LV_BRULENONE_IMAGE);
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 40;
	strcpy(text, "layer");
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = column;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 150;
	lvc.iSubItem = column;
	strcpy(text, "name");
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 100;
	lvc.iSubItem = column;
	strstr.str("");
	strcpy(text, "center");
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	BOOST_FOREACH (const config& img, cfg_.child_range("image")) {
		column = 0;

		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		// layer
		lvi.iItem = ListView_GetItemCount(hctl);
		lvi.iSubItem = column ++;
		strstr.str("");
		strstr << img["layer"].to_int();
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		lvi.lParam = (LPARAM)0;
		ListView_InsertItem(hctl, &lvi);

		// name
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = column ++;
		strstr.str("");
		strstr << img["name"].str();
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// center
		strstr.str("");
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = column ++;
		strstr.str("");
		strstr << img["center"].str();
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);
	}
}

void tnone_brule::from_ui_tile_edit(HWND hdlgP)
{
}

void tnone_brule::update_to_ui_tile_edit(HWND hdlgP) const
{
	const config& tile = cfg_.child("tile", ns::clicked_tile);
	// pos
	HWND hctl = GetDlgItem(hdlgP, IDC_UD_TILEEDIT_POS);
	UpDown_SetRange(hctl, 1, 8);	// [1, 8]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_TILEEDIT_POS));
	UpDown_SetPos(hctl, tile["pos"].to_int());

	Static_SetText(GetDlgItem(hdlgP, IDC_ET_TILEEDIT_TYPE), tile["type"].str().c_str());

	std::pair<std::string, std::string> flag = get_flag(tile);
	if (!flag.first.empty()) {
		std::stringstream strstr;
		strstr << flag.first << " = " << flag.second;
		Static_SetText(GetDlgItem(hdlgP, IDC_ET_TILEEDIT_FLAG), strstr.str().c_str());
	}
}

void tnone_brule::from_ui_image_edit(HWND hdlgP)
{
}

void tnone_brule::update_to_ui_image_edit(HWND hdlgP) const
{
	const config& tile = cfg_.child("image", ns::clicked_image);
	Static_SetText(GetDlgItem(hdlgP, IDC_ET_IMAGEEDIT_LAYER), tile["layer"].str().c_str());
	Static_SetText(GetDlgItem(hdlgP, IDC_ET_IMAGEEDIT_NAME), tile["name"].str().c_str());
	Static_SetText(GetDlgItem(hdlgP, IDC_ET_IMAGEEDIT_CENTER), tile["center"].str().c_str());
}

std::string tnone_brule::generate(bool tpl) const
{
	std::stringstream strstr;

	if (!tpl) {
		config cfg;
		cfg.add_child("terrain_graphics", cfg_);
		::write(strstr, cfg);
	} else {
		return tbuilding_rule::generate(tpl);
	}

	return strstr.str();
}

std::string ttransition_complete_lf_brule::description() const
{
	utils::string_map symbols;
	symbols["terrainlist"] = cfg_["0_terrainlist"].str();
	symbols["adjacent"] = cfg_["1_adjacent"].str();
	symbols["flag"] = cfg_["3_flag"].str();
	symbols["imagestem"] = cfg_["4_imagestem"].str();

	return vgettext2("TRANSITION_COMPLETE_LFB($terrainlist, $adjacent, $flag, $imagestem)", symbols);
}

std::string ttransition_complete_lb_brule::description() const
{
	utils::string_map symbols;
	symbols["terrainlist"] = cfg_["0_terrainlist"].str();
	symbols["adjacent"] = cfg_["1_adjacent"].str();
	symbols["flag"] = "transition";
	symbols["imagestem"] = cfg_["4_imagestem"].str();

	return vgettext2("TRANSITION_COMPLETE_LFB($terrainlist, $adjacent, $flag, $imagestem)", symbols);
}

std::string ttransition_complete_l_brule::description() const
{
	utils::string_map symbols;
	symbols["terrainlist"] = cfg_["0_terrainlist"].str();
	symbols["adjacent"] = cfg_["1_adjacent"].str();
	symbols["flag"] = "transition";
	symbols["imagestem"] = cfg_["3_imagestem"].str();

	return vgettext2("TRANSITION_COMPLETE_LFB($terrainlist, $adjacent, $flag, $imagestem)", symbols);
}

std::string teditor_overlay_brule::description() const
{
	utils::string_map symbols;
	symbols["type"] = cfg_["0_type"].str();
	symbols["image"] = cfg_["1_image"].str();

	return vgettext2("EDITOR_OVERLAY($type, $image)", symbols);
}

std::string tbridge_straights_brule::description() const
{
	utils::string_map symbols;
	symbols["nw_se_overlay"] = cfg_["0_nw_se_overlay"].str();
	symbols["n_s_overlay"] = cfg_["1_n_s_overlay"].str();
	symbols["ne_sw_overlay"] = cfg_["2_ne_sw_overlay"].str();
	symbols["bn_terrain"] = cfg_["3_bn_terrain"].str();
	symbols["bs_terrain"] = cfg_["4_bs_terrain"].str();
	symbols["s_terrain"] = cfg_["5_s_terrain"].str();
	symbols["image_group"] = cfg_["8_image_group"].str();

	return vgettext2("BRIDGE:STRAIGHTS($nw_se_overlay, $n_s_overlay, $ne_sw_overlay, $bn_terrain, $bs_terrain, $s_terrain, $image_group)", symbols);
}

std::string tbridge_ends_brule::description() const
{
	utils::string_map symbols;
	symbols["nw_se_overlay"] = cfg_["0_nw_se_overlay"].str();
	symbols["n_s_overlay"] = cfg_["1_n_s_overlay"].str();
	symbols["ne_sw_overlay"] = cfg_["2_ne_sw_overlay"].str();
	symbols["b_terrain"] = cfg_["3_b_terrain"].str();
	symbols["e_terrain"] = cfg_["4_e_terrain"].str();
	symbols["s_terrain"] = cfg_["5_s_terrain"].str();
	symbols["image_group"] = cfg_["8_image_group"].str();

	return vgettext2("BRIDGE:STRAIGHTS($nw_se_overlay, $n_s_overlay, $ne_sw_overlay, $b_terrain, $e_terrain, $s_terrain, $image_group)", symbols);
}

std::string tbridge_joints_brule::description() const
{
	utils::string_map symbols;
	symbols["nw_se_overlay"] = cfg_["0_nw_se_overlay"].str();
	symbols["n_s_overlay"] = cfg_["1_n_s_overlay"].str();
	symbols["ne_sw_overlay"] = cfg_["2_ne_sw_overlay"].str();
	symbols["b_terrain"] = cfg_["3_b_terrain"].str();
	symbols["s_terrain"] = cfg_["4_s_terrain"].str();
	symbols["image_group"] = cfg_["7_image_group"].str();

	return vgettext2("BRIDGE:JOINTS($nw_se_overlay, $n_s_overlay, $ne_sw_overlay, $b_terrain, $s_terrain, $image_group)", symbols);
}

std::string tbridge_corners_brule::description() const
{
	utils::string_map symbols;
	symbols["nw_se_overlay"] = cfg_["0_nw_se_overlay"].str();
	symbols["n_s_overlay"] = cfg_["1_n_s_overlay"].str();
	symbols["ne_sw_overlay"] = cfg_["2_ne_sw_overlay"].str();
	symbols["b_terrain"] = cfg_["3_b_terrain"].str();
	symbols["s_terrain"] = cfg_["4_s_terrain"].str();
	symbols["image_group"] = cfg_["7_image_group"].str();

	return vgettext2("BRIDGE:CORNERS($nw_se_overlay, $n_s_overlay, $ne_sw_overlay, $b_terrain, $s_terrain, $image_group)", symbols);
}

std::string tlayout_tracks_f_brule::description() const
{
	utils::string_map symbols;
	symbols["se_nw_value"] = cfg_["0_se_nw_value"].str();
	symbols["n_s_value"] = cfg_["1_n_s_value"].str();
	symbols["ne_sw_value"] = cfg_["2_ne_sw_value"].str();
	symbols["flag"] = cfg_["3_flag"].str();

	return vgettext2("LAYOUT_TRACKS_F($se_nw_value, $n_s_value, $ne_sw_value, $flag)", symbols);
}

std::string ttrack_complete_brule::description() const
{
	utils::string_map symbols;
	symbols["se_nw_value"] = cfg_["0_se_nw_value"].str();
	symbols["n_s_value"] = cfg_["1_n_s_value"].str();
	symbols["ne_sw_value"] = cfg_["2_ne_sw_value"].str();
	symbols["flag"] = cfg_["3_flag"].str();
	symbols["imagestem"] = cfg_["4_imagestem"].str();

	return vgettext2("TRACK_COMPLETE($se_nw_value, $n_s_value, $ne_sw_value, $flag, $imagestem)", symbols);
}

std::string ttrack_border_restricted_plf_brule::description() const
{
	utils::string_map symbols;
	symbols["terrainlist"] = cfg_["0_terrainlist"].str();
	symbols["adjacent"] = cfg_["1_adjacent"].str();
	symbols["flag"] = cfg_["4_flag"].str();
	symbols["imagestem"] = cfg_["5_imagestem"].str();

	return vgettext2("TRACK_BORDER_RESTRICTED_PLF($terrainlist, $adjacent, $flag, $imagestem)", symbols);
}

std::string tnew_forest_brule::description() const
{
	utils::string_map symbols;
	symbols["terrainlist"] = cfg_["0_terrainlist"].str();
	symbols["adjacent"] = cfg_["1_adjacent"].str();
	symbols["imagestem"] = cfg_["2_imagestem"].str();

	return vgettext2("NEW:FOREST($terrainlist, $adjacent, $imagestem)", symbols);
}

std::string toverlay_random_brule::description() const
{
	utils::string_map symbols;
	symbols["terrainlist"] = cfg_["0_terrainlist"].str();
	symbols["prob"] = "100";
	symbols["flag"] = "overlay";
	symbols["imagestem"] = cfg_["1_imagestem"].str();

	return vgettext2("GENERIC_SINGLE_PLFB($terrainlist, $prob, $flag, $imagestem)", symbols);
}

std::string toverlay_random_lf_brule::description() const
{
	utils::string_map symbols;
	symbols["terrainlist"] = cfg_["0_terrainlist"].str();
	symbols["prob"] = "100";
	symbols["flag"] = cfg_["2_flag"].str();
	symbols["imagestem"] = cfg_["3_imagestem"].str();

	return vgettext2("GENERIC_SINGLE_PLFB($terrainlist, $prob, $flag, $imagestem)", symbols);
}

std::string toverlay_random_l_brule::description() const
{
	utils::string_map symbols;
	symbols["terrainlist"] = cfg_["0_terrainlist"].str();
	symbols["prob"] = "100";
	symbols["flag"] = "overlay";
	symbols["imagestem"] = cfg_["2_imagestem"].str();

	return vgettext2("GENERIC_SINGLE_PLFB($terrainlist, $prob, $flag, $imagestem)", symbols);
}

std::string toverlay_lf_brule::description() const
{
	utils::string_map symbols;
	symbols["terrainlist"] = cfg_["0_terrainlist"].str();
	symbols["prob"] = "100";
	symbols["flag"] = cfg_["2_flag"].str();
	symbols["imagestem"] = cfg_["3_imagestem"].str();

	return vgettext2("GENERIC_SINGLE_PLFB($terrainlist, $prob, $flag, $imagestem)", symbols);
}

std::string toverlay_f_brule::description() const
{
	utils::string_map symbols;
	symbols["terrainlist"] = cfg_["0_terrainlist"].str();
	symbols["prob"] = "100";
	symbols["flag"] = cfg_["1_flag"].str();;
	symbols["imagestem"] = cfg_["2_imagestem"].str();

	return vgettext2("GENERIC_SINGLE_PLFB($terrainlist, $prob, $flag, $imagestem)", symbols);
}

std::string toverlay_p_brule::description() const
{
	utils::string_map symbols;
	symbols["terrainlist"] = cfg_["0_terrainlist"].str();
	symbols["prob"] = cfg_["1_prob"].str();
	symbols["flag"] = "overlay";
	symbols["imagestem"] = cfg_["2_imagestem"].str();

	return vgettext2("GENERIC_SINGLE_PLFB($terrainlist, $prob, $flag, $imagestem)", symbols);
}

std::string toverlay_b_brule::description() const
{
	utils::string_map symbols;
	symbols["terrainlist"] = cfg_["0_terrainlist"].str();
	symbols["prob"] = "100";
	symbols["flag"] = "overlay";
	symbols["imagestem"] = cfg_["2_imagestem"].str();

	return vgettext2("GENERIC_SINGLE_PLFB($terrainlist, $prob, $flag, $imagestem)", symbols);
}

std::string toverlay_brule::description() const
{
	utils::string_map symbols;
	symbols["terrainlist"] = cfg_["0_terrainlist"].str();
	symbols["prob"] = "100";
	symbols["flag"] = "overlay";
	symbols["imagestem"] = cfg_["2_imagestem"].str();

	return vgettext2("GENERIC_SINGLE_PLFB($terrainlist, $prob, $flag, $imagestem)", symbols);
}

std::string tterrain_base_random_brule::description() const
{
	utils::string_map symbols;
	symbols["terrainlist"] = cfg_["0_terrainlist"].str();
	symbols["prob"] = "100";
	symbols["flag"] = "base";
	symbols["imagestem"] = cfg_["1_imagestem"].str();

	return vgettext2("GENERIC_SINGLE_PLFB($terrainlist, $prob, $flag, $imagestem)", symbols);
}

std::string tterrain_base_p_brule::description() const
{
	utils::string_map symbols;
	symbols["terrainlist"] = cfg_["0_terrainlist"].str();
	symbols["prob"] = cfg_["1_prob"].str();
	symbols["flag"] = "base";
	symbols["imagestem"] = cfg_["2_imagestem"].str();

	return vgettext2("GENERIC_SINGLE_PLFB($terrainlist, $prob, $flag, $imagestem)", symbols);
}

std::string tterrain_base_f_brule::description() const
{
	utils::string_map symbols;
	symbols["terrainlist"] = cfg_["0_terrainlist"].str();
	symbols["prob"] = "100";
	symbols["flag"] = cfg_["1_flag"].str();
	symbols["imagestem"] = cfg_["2_imagestem"].str();

	return vgettext2("GENERIC_SINGLE_PLFB($terrainlist, $prob, $flag, $imagestem)", symbols);
}

std::string tterrain_base_brule::description() const
{
	utils::string_map symbols;
	symbols["terrainlist"] = cfg_["0_terrainlist"].str();
	symbols["prob"] = "100";
	symbols["flag"] = "base";
	symbols["imagestem"] = cfg_["1_imagestem"].str();

	return vgettext2("GENERIC_SINGLE_PLFB($terrainlist, $prob, $flag, $imagestem)", symbols);
}

std::string tkeep_base_random_brule::description() const
{
	utils::string_map symbols;
	symbols["terrainlist"] = cfg_["0_terrainlist"].str();
	symbols["prob"] = "100";
	symbols["flag"] = "base";
	symbols["imagestem"] = cfg_["1_imagestem"].str();

	return vgettext2("GENERIC_SINGLE_PLFB($terrainlist, $prob, $flag, $imagestem)", symbols);
}

std::string tkeep_base_brule::description() const
{
	utils::string_map symbols;
	symbols["terrainlist"] = cfg_["0_terrainlist"].str();
	symbols["prob"] = "100";
	symbols["flag"] = "base";
	symbols["imagestem"] = cfg_["1_imagestem"].str();

	return vgettext2("GENERIC_SINGLE_PLFB($terrainlist, $prob, $flag, $imagestem)", symbols);
}

std::string tterrain_base_singlehex_lb_brule::description() const
{
	utils::string_map symbols;
	symbols["terrainlist"] = cfg_["0_terrainlist"].str();
	symbols["prob"] = "100";
	symbols["flag"] = "base";
	symbols["imagestem"] = cfg_["3_imagestem"].str();

	return vgettext2("GENERIC_SINGLEHEX_PLFB($terrainlist, $prob, $flag, $imagestem)", symbols);
}

std::string tterrain_base_singlehex_b_brule::description() const
{
	utils::string_map symbols;
	symbols["terrainlist"] = cfg_["0_terrainlist"].str();
	symbols["prob"] = "100";
	symbols["flag"] = "base";
	symbols["imagestem"] = cfg_["2_imagestem"].str();

	return vgettext2("GENERIC_SINGLEHEX_PLFB($terrainlist, $prob, $flag, $imagestem)", symbols);
}

std::string toverlay_complete_lf_brule::description() const
{
	utils::string_map symbols;
	symbols["terrainlist"] = cfg_["0_terrainlist"].str();
	symbols["adjacent"] = cfg_["1_adjacent"].str();
	symbols["prob"] = "100";
	symbols["flag"] = cfg_["2_flag"].str();
	symbols["imagestem"] = cfg_["3_imagestem"].str();

	return vgettext2("GENERIC_COMPLETE_LFB($terrainlist, $adjacent, $prob, $flag, $imagestem)", symbols);
}

std::string toverlay_complete_l_brule::description() const
{
	utils::string_map symbols;
	symbols["terrainlist"] = cfg_["0_terrainlist"].str();
	symbols["adjacent"] = cfg_["1_adjacent"].str();
	symbols["prob"] = "100";
	symbols["flag"] = "overlay";
	symbols["imagestem"] = cfg_["4_imagestem"].str();

	return vgettext2("GENERIC_COMPLETE_LFB($terrainlist, $adjacent, $prob, $flag, $imagestem)", symbols);
}

std::string toverlay_complete_f_brule::description() const
{
	utils::string_map symbols;
	symbols["terrainlist"] = cfg_["0_terrainlist"].str();
	symbols["adjacent"] = cfg_["1_adjacent"].str();
	symbols["prob"] = "100";
	symbols["flag"] = cfg_["3_flag"].str();
	symbols["imagestem"] = cfg_["4_imagestem"].str();

	return vgettext2("GENERIC_COMPLETE_LFB($terrainlist, $adjacent, $prob, $flag, $imagestem)", symbols);
}

std::string toverlay_complete_brule::description() const
{
	utils::string_map symbols;
	symbols["terrainlist"] = cfg_["0_terrainlist"].str();
	symbols["adjacent"] = cfg_["1_adjacent"].str();
	symbols["prob"] = "100";
	symbols["flag"] = "overlay";
	symbols["imagestem"] = cfg_["2_imagestem"].str();

	return vgettext2("GENERIC_COMPLETE_LFB($terrainlist, $adjacent, $prob, $flag, $imagestem)", symbols);
}

std::string tvolcano_2x2_brule::description() const
{
	utils::string_map symbols;
	symbols["terrain"] = cfg_["0_terrain"].str();
	symbols["adjacent"] = cfg_["1_adjacent"].str();
	symbols["prob"] = "100";
	symbols["flag"] = cfg_["3_flag"].str();
	symbols["imagestem"] = cfg_["4_imagestem"].str();

	return vgettext2("VOLCANO_2x2($terrain, $adjacent, $prob, $flag, $imagestem)", symbols);
}

std::string toverlay_restricted3_f_brule::description() const
{
	utils::string_map symbols;
	symbols["terrain"] = cfg_["0_terrain"].str();
	symbols["adjacent"] = cfg_["1_adjacent"].str();
	symbols["prob"] = "100";
	symbols["flag"] = cfg_["2_flag"].str();
	symbols["imagestem"] = cfg_["3_imagestem"].str();

	return vgettext2("GENERIC_RESTRICTED3_PLFB($terrain, $adjacent, $prob, $flag, $imagestem)", symbols);
}

std::string toverlay_restricted2_f_brule::description() const
{
	utils::string_map symbols;
	symbols["terrain"] = cfg_["0_terrain"].str();
	symbols["adjacent"] = cfg_["1_adjacent"].str();
	symbols["prob"] = "100";
	symbols["flag"] = cfg_["2_flag"].str();
	symbols["imagestem"] = cfg_["3_imagestem"].str();

	return vgettext2("GENERIC_RESTRICTED2_PLFB($terrain, $adjacent, $prob, $flag, $imagestem)", symbols);
}

std::string toverlay_restricted_p_brule::description() const
{
	utils::string_map symbols;
	symbols["terrain"] = cfg_["0_terrain"].str();
	symbols["adjacent"] = cfg_["1_adjacent"].str();
	symbols["prob"] = cfg_["2_prob"].str();
	symbols["flag"] = "overlay";
	symbols["imagestem"] = cfg_["3_imagestem"].str();

	return vgettext2("GENERIC_RESTRICTED_PLFB($terrain, $adjacent, $prob, $flag, $imagestem)", symbols);
}

std::string toverlay_restricted_f_brule::description() const
{
	utils::string_map symbols;
	symbols["terrain"] = cfg_["0_terrain"].str();
	symbols["adjacent"] = cfg_["1_adjacent"].str();
	symbols["prob"] = "100";
	symbols["flag"] = cfg_["2_flag"].str();
	symbols["imagestem"] = cfg_["3_imagestem"].str();

	return vgettext2("GENERIC_RESTRICTED_PLFB($terrain, $adjacent, $prob, $flag, $imagestem)", symbols);
}

std::string toverlay_rotation_restricted2_f_brule::description() const
{
	utils::string_map symbols;
	symbols["terrain"] = cfg_["0_terrain"].str();
	symbols["adjacent"] = cfg_["1_adjacent"].str();
	symbols["prob"] = "100";
	symbols["flag"] = cfg_["2_flag"].str();
	symbols["imagestem"] = cfg_["3_imagestem"].str();

	return vgettext2("GENERIC_RESTRICTED2_PLFB($terrain, $adjacent, $prob, $flag, $imagestem)", symbols);
}

std::string toverlay_rotation_restricted_f_brule::description() const
{
	utils::string_map symbols;
	symbols["terrain"] = cfg_["0_terrain"].str();
	symbols["adjacent"] = cfg_["1_adjacent"].str();
	symbols["prob"] = "100";
	symbols["flag"] = cfg_["2_flag"].str();
	symbols["imagestem"] = cfg_["3_imagestem"].str();

	return vgettext2("GENERIC_RESTRICTED_PLFB($terrain, $adjacent, $prob, $flag, $imagestem)", symbols);
}

std::string tmountains_2x4_nw_se_brule::description() const
{
	utils::string_map symbols;
	symbols["terrain"] = cfg_["0_terrain"].str();
	symbols["prob"] = cfg_["1_prob"].str();
	symbols["flag"] = cfg_["2_flag"].str();
	symbols["imagestem"] = cfg_["3_imagestem"].str();

	return vgettext2("MOUNTAINS_2x4_NW_SE($terrain, $adjacent, $prob, $flag, $imagestem)", symbols);
}

std::string tmountains_2x4_sw_ne_brule::description() const
{
	utils::string_map symbols;
	symbols["terrain"] = cfg_["0_terrain"].str();
	symbols["prob"] = cfg_["1_prob"].str();
	symbols["flag"] = cfg_["2_flag"].str();
	symbols["imagestem"] = cfg_["3_imagestem"].str();

	return vgettext2("MOUNTAINS_2x4_SW_NE($terrain, $adjacent, $prob, $flag, $imagestem)", symbols);
}

std::string tmountains_1x3_nw_se_brule::description() const
{
	utils::string_map symbols;
	symbols["terrain"] = cfg_["0_terrain"].str();
	symbols["prob"] = cfg_["1_prob"].str();
	symbols["flag"] = cfg_["2_flag"].str();
	symbols["imagestem"] = cfg_["3_imagestem"].str();

	return vgettext2("MOUNTAINS_1x3_NW_SE($terrain, $adjacent, $prob, $flag, $imagestem)", symbols);
}

std::string tmountains_1x3_sw_ne_brule::description() const
{
	utils::string_map symbols;
	symbols["terrain"] = cfg_["0_terrain"].str();
	symbols["prob"] = cfg_["1_prob"].str();
	symbols["flag"] = cfg_["2_flag"].str();
	symbols["imagestem"] = cfg_["3_imagestem"].str();

	return vgettext2("MOUNTAINS_1x3_SW_NE($terrain, $adjacent, $prob, $flag, $imagestem)", symbols);
}

std::string tmountains_2x2_brule::description() const
{
	utils::string_map symbols;
	symbols["terrain"] = cfg_["0_terrain"].str();
	symbols["prob"] = cfg_["1_prob"].str();
	symbols["flag"] = cfg_["2_flag"].str();
	symbols["imagestem"] = cfg_["3_imagestem"].str();

	return vgettext2("MOUNTAINS_2x2($terrain, $adjacent, $prob, $flag, $imagestem)", symbols);
}

std::string tmountain_single_random_brule::description() const
{
	utils::string_map symbols;
	symbols["terrain"] = cfg_["0_terrain"].str();
	symbols["prob"] = "100";
	symbols["flag"] = cfg_["1_flag"].str();
	symbols["imagestem"] = cfg_["2_imagestem"].str();

	return vgettext2("MOUNTAIN_SINGLE($terrain, $adjacent, $prob, $flag, $imagestem)", symbols);
}

std::string tpeaks_1x2_sw_ne_brule::description() const
{
	utils::string_map symbols;
	symbols["terrain"] = cfg_["0_terrain"].str();
	symbols["prob"] = cfg_["1_prob"].str();
	symbols["flag"] = cfg_["2_flag"].str();
	symbols["imagestem"] = cfg_["3_imagestem"].str();

	return vgettext2("PEAKS_1x2_SW_NE($terrain, $adjacent, $prob, $flag, $imagestem)", symbols);
}

std::string tpeaks_large_brule::description() const
{
	utils::string_map symbols;
	symbols["terrain"] = cfg_["0_terrain"].str();
	symbols["prob"] = cfg_["1_prob"].str();
	symbols["flag"] = cfg_["2_flag"].str();
	symbols["imagestem"] = cfg_["3_imagestem"].str();

	return vgettext2("PEAKS_LARGE($terrain, $adjacent, $prob, $flag, $imagestem)", symbols);
}

std::string tvillage_brule::description() const
{
	utils::string_map symbols;
	symbols["terrainlist"] = cfg_["0_terrainlist"].str();
	symbols["imagestem"] = cfg_["1_imagestem"].str();

	return vgettext2("VILLAGE($terrainlist, $imagestem)", symbols);
}

std::string tvillage_a3_brule::description() const
{
	utils::string_map symbols;
	symbols["terrainlist"] = cfg_["0_terrainlist"].str();
	symbols["time"] = cfg_["1_time"].str();
	symbols["imagestem"] = cfg_["2_imagestem"].str();

	return vgettext2("VILLAGE_A3($terrainlist, $time, $imagestem)", symbols);
}

std::string tvillage_a4_brule::description() const
{
	utils::string_map symbols;
	symbols["terrainlist"] = cfg_["0_terrainlist"].str();
	symbols["time"] = cfg_["1_time"].str();
	symbols["imagestem"] = cfg_["2_imagestem"].str();

	return vgettext2("VILLAGE_A4($terrainlist, $time, $imagestem)", symbols);
}

std::string tnew_fence_brule::description() const
{
	utils::string_map symbols;
	symbols["terrainlist"] = cfg_["0_terrainlist"].str();
	symbols["imagestem"] = cfg_["1_imagestem"].str();

	return vgettext2("NEW_FENCE($terrainlist, $imagestem)", symbols);
}

std::string tnew_wall_p_brule::description() const
{
	utils::string_map symbols;
	symbols["terrainlist"] = cfg_["0_terrainlist"].str();
	symbols["adjacent"] = cfg_["1_adjacent"].str();
	symbols["prob"] = cfg_["2_prob"].str();
	symbols["imagestem"] = cfg_["3_imagestem"].str();

	return vgettext2("NEW_WALL($terrainlist, $adjacent, $prob, $imagestem)", symbols);
}

std::string tnew_wall_brule::description() const
{
	utils::string_map symbols;
	symbols["terrainlist"] = cfg_["0_terrainlist"].str();
	symbols["adjacent"] = cfg_["1_adjacent"].str();
	symbols["prob"] = "100";
	symbols["imagestem"] = cfg_["2_imagestem"].str();

	return vgettext2("NEW_WALL($terrainlist, $adjacent, $prob, $imagestem)", symbols);
}

std::string tnew_wall2_p_brule::description() const
{
	utils::string_map symbols;
	symbols["terrainlist"] = cfg_["0_terrainlist"].str();
	symbols["adjacent1"] = cfg_["1_adjacent1"].str();
	symbols["adjacent2"] = cfg_["2_adjacent2"].str();
	symbols["prob"] = cfg_["3_prob"].str();
	symbols["imagestem"] = cfg_["4_imagestem"].str();

	return vgettext2("NEW_WALL2($terrainlist, $adjacent1, $adjacent2, $prob, $imagestem)", symbols);
}

std::string tnew_wall2_brule::description() const
{
	utils::string_map symbols;
	symbols["terrainlist"] = cfg_["0_terrainlist"].str();
	symbols["adjacent1"] = cfg_["1_adjacent1"].str();
	symbols["adjacent2"] = cfg_["2_adjacent2"].str();
	symbols["prob"] = "100";
	symbols["imagestem"] = cfg_["3_imagestem"].str();

	return vgettext2("NEW_WALL2($terrainlist, $adjacent1, $adjacent2, $prob, $imagestem)", symbols);
}

std::string tnew_waves_brule::description() const
{
	utils::string_map symbols;
	symbols["terrainlist"] = cfg_["0_terrainlist"].str();
	symbols["adjacent"] = cfg_["1_adjacent"].str();
	symbols["imagestem"] = cfg_["3_imagestem"].str();

	return vgettext2("NEW_WAVES($terrainlist, $adjacent, $imagestem)", symbols);
}

std::string tnew_beach_brule::description() const
{
	utils::string_map symbols;
	symbols["terrainlist"] = cfg_["0_terrainlist"].str();
	symbols["adjacent"] = cfg_["1_adjacent"].str();
	symbols["imagestem"] = cfg_["2_imagestem"].str();

	return vgettext2("NEW_BEACH($terrainlist, $adjacent, $imagestem)", symbols);
}

std::string twall_transition3_brule::description() const
{
	utils::string_map symbols;
	symbols["terrainlist1"] = cfg_["0_terrainlist1"].str();
	symbols["terrainlist2"] = cfg_["1_terrainlist2"].str();
	symbols["terrainlist3"] = cfg_["2_terrainlist3"].str();
	symbols["prob"] = "100";
	symbols["flag"] = "overlay";
	symbols["imagestem"] = cfg_["3_imagestem"].str();

	return vgettext2("WALL_TRANSITION3_PLFB($terrainlist1, $terrainlist2, $terrainlist3, $prob, $flag, $imagestem)", symbols);
}

std::string twall_transition2_lf_brule::description() const
{
	utils::string_map symbols;
	symbols["terrainlist1"] = cfg_["0_terrainlist1"].str();
	symbols["terrainlist2"] = cfg_["1_terrainlist2"].str();
	symbols["adjacent"] = cfg_["2_adjacent"].str();
	symbols["prob"] = "100";
	symbols["flag"] = cfg_["4_flag"].str();
	symbols["imagestem"] = cfg_["5_imagestem"].str();

	return vgettext2("WALL_TRANSITION2_PLFB($terrainlist1, $terrainlist2, $terrainlist3, $prob, $flag, $imagestem)", symbols);
}

std::string twall_transition_lf_brule::description() const
{
	utils::string_map symbols;
	symbols["terrainlist"] = cfg_["0_terrainlist"].str();
	symbols["adjacent"] = cfg_["1_adjacent"].str();
	symbols["prob"] = "100";
	symbols["flag"] = cfg_["3_flag"].str();
	symbols["imagestem"] = cfg_["4_imagestem"].str();

	return vgettext2("WALL_TRANSITION_PLFB($terrainlist, $adjacent, $prob, $flag, $imagestem)", symbols);
}

std::string twall_adjacent_transition_brule::description() const
{
	utils::string_map symbols;
	symbols["terrainlist"] = cfg_["0_terrainlist"].str();
	symbols["adjacent"] = cfg_["1_adjacent"].str();
	symbols["builder"] = cfg_["2_builder"].str();
	symbols["imagestem"] = cfg_["3_imagestem"].str();

	return vgettext2("WALL_ADJACENT_TRANSITION($terrainlist, $adjacent, $builder, $imagestem)", symbols);
}

std::string tanimated_water_15_brule::description() const
{
	utils::string_map symbols;
	symbols["terrainlist"] = cfg_["0_terrainlist"].str();
	symbols["ipf"] = cfg_["1_ipf"].str();
	symbols["time"] = cfg_["2_time"].str();
	symbols["imagestem"] = cfg_["3_imagestem"].str();

	return vgettext2("ANIMATED_WATER_15($terrainlist, $ipf, $time, $imagestem)", symbols);
}

std::string tanimated_water_15_transition_brule::description() const
{
	utils::string_map symbols;
	symbols["terrainlist"] = cfg_["0_terrainlist"].str();
	symbols["adjacent"] = cfg_["1_adjacent"].str();
	symbols["ipf"] = cfg_["3_ipf"].str();
	symbols["time"] = cfg_["4_time"].str();
	symbols["imagestem"] = cfg_["5_imagestem"].str();

	return vgettext2("ANIMATED_WATER_15_TRANSITION($terrainlist, $adjacent, $ipf, $time, $imagestem)", symbols);
}

std::string tdisable_base_transitions_brule::description() const
{
	utils::string_map symbols;
	symbols["terrainlist"] = cfg_["0_terrainlist"].str();
	symbols["flag"] = "transition";

	return vgettext2("DISABLE_BASE_TRANSITIONS_F($terrainlist, $flag)", symbols);
}

void tcore::update_to_ui_builder(HWND hdlgP)
{
	HWND hctl = GetDlgItem(hdlgP, IDC_LV_BUILDER_EXPLORER);
	LVITEM lvi;
	ListView_DeleteAllItems(hctl);
	std::stringstream strstr;
	char text[4096];
	int row = 0;

	for (std::vector<tbuilding_rule*>::const_iterator it = brules_updating_.begin(); it != brules_updating_.end(); ++ it) {
		const tbuilding_rule& rule = **it;
		int column = 0;

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

		// type
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = column ++;
		strstr.str("");
		strstr << tbuilding_rule::macro_tag(rule.type_);
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// description
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = column ++;
		strstr.str("");
		strstr << rule.description();
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);
	}
}

void tcore::release_builder()
{
	for (std::vector<tbuilding_rule*>::iterator it = brules_from_cfg_.begin(); it != brules_from_cfg_.end(); ++ it) {
		delete *it;
	}
	brules_from_cfg_.clear();

	for (std::vector<tbuilding_rule*>::iterator it = brules_updating_.begin(); it != brules_updating_.end(); ++ it) {
		delete *it;
	}
	brules_updating_.clear();
}

bool tcore::brules_dirty() const
{
	if (brules_updating_.size() != brules_from_cfg_.size()) {
		return true;
	}
	for (size_t i = 0; i < brules_updating_.size(); i ++) {
		const tbuilding_rule& r1 = *brules_updating_[i];
		const tbuilding_rule& r2 = *brules_from_cfg_[i];
		if (r1 != r2) return true;
	}
	return false;
}

void tcore::copy_brules(const std::vector<tbuilding_rule*>& src, std::vector<tbuilding_rule*>& dst)
{
	for (std::vector<tbuilding_rule*>::iterator it = dst.begin(); it != dst.end(); ++ it) {
		delete *it;
	}
	dst.clear();
	for (std::vector<tbuilding_rule*>::const_iterator it = src.begin(); it != src.end(); ++ it) {
		const tbuilding_rule* rule = *it;
		tbuilding_rule* new_rule = NULL;
		if (rule->type_ == tbuilding_rule::NONE) {
			new_rule = new tnone_brule(*dynamic_cast<const tnone_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::TRANSITION_COMPLETE_LF) {
			new_rule = new ttransition_complete_lf_brule(*dynamic_cast<const ttransition_complete_lf_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::TRANSITION_COMPLETE_LB) {
			new_rule = new ttransition_complete_lb_brule(*dynamic_cast<const ttransition_complete_lb_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::TRANSITION_COMPLETE_L) {
			new_rule = new ttransition_complete_l_brule(*dynamic_cast<const ttransition_complete_l_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::EDITOR_OVERLAY) {
			new_rule = new teditor_overlay_brule(*dynamic_cast<const teditor_overlay_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::BRIDGE_STRAIGHTS) {
			new_rule = new tbridge_straights_brule(*dynamic_cast<const tbridge_straights_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::BRIDGE_ENDS) {
			new_rule = new tbridge_ends_brule(*dynamic_cast<const tbridge_ends_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::BRIDGE_JOINTS) {
			new_rule = new tbridge_joints_brule(*dynamic_cast<const tbridge_joints_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::BRIDGE_CORNERS) {
			new_rule = new tbridge_corners_brule(*dynamic_cast<const tbridge_corners_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::LAYOUT_TRACKS_F) {
			new_rule = new tlayout_tracks_f_brule(*dynamic_cast<const tlayout_tracks_f_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::TRACK_COMPLETE) {
			new_rule = new ttrack_complete_brule(*dynamic_cast<const ttrack_complete_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::TRACK_BORDER_RESTRICTED_PLF) {
			new_rule = new ttrack_border_restricted_plf_brule(*dynamic_cast<const ttrack_border_restricted_plf_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::NEW_FOREST) {
			new_rule = new tnew_forest_brule(*dynamic_cast<const tnew_forest_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::OVERLAY_RANDOM) {
			new_rule = new toverlay_random_brule(*dynamic_cast<const toverlay_random_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::OVERLAY_RANDOM_LF) {
			new_rule = new toverlay_random_lf_brule(*dynamic_cast<const toverlay_random_lf_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::OVERLAY_RANDOM_L) {
			new_rule = new toverlay_random_l_brule(*dynamic_cast<const toverlay_random_l_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::OVERLAY_LF) {
			new_rule = new toverlay_lf_brule(*dynamic_cast<const toverlay_lf_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::OVERLAY_F) {
			new_rule = new toverlay_f_brule(*dynamic_cast<const toverlay_f_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::OVERLAY_P) {
			new_rule = new toverlay_p_brule(*dynamic_cast<const toverlay_p_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::OVERLAY_B) {
			new_rule = new toverlay_b_brule(*dynamic_cast<const toverlay_b_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::OVERLAY) {
			new_rule = new toverlay_brule(*dynamic_cast<const toverlay_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::TERRAIN_BASE_RANDOM) {
			new_rule = new tterrain_base_random_brule(*dynamic_cast<const tterrain_base_random_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::TERRAIN_BASE_P) {
			new_rule = new tterrain_base_p_brule(*dynamic_cast<const tterrain_base_p_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::TERRAIN_BASE_F) {
			new_rule = new tterrain_base_f_brule(*dynamic_cast<const tterrain_base_f_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::TERRAIN_BASE) {
			new_rule = new tterrain_base_brule(*dynamic_cast<const tterrain_base_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::KEEP_BASE_RANDOM) {
			new_rule = new tkeep_base_random_brule(*dynamic_cast<const tkeep_base_random_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::KEEP_BASE) {
			new_rule = new tkeep_base_brule(*dynamic_cast<const tkeep_base_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::TERRAIN_BASE_SINGLEHEX_LB) {
			new_rule = new tterrain_base_singlehex_lb_brule(*dynamic_cast<const tterrain_base_singlehex_lb_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::TERRAIN_BASE_SINGLEHEX_B) {
			new_rule = new tterrain_base_singlehex_b_brule(*dynamic_cast<const tterrain_base_singlehex_b_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::OVERLAY_COMPLETE_LF) {
			new_rule = new toverlay_complete_lf_brule(*dynamic_cast<const toverlay_complete_lf_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::OVERLAY_COMPLETE_L) {
			new_rule = new toverlay_complete_l_brule(*dynamic_cast<const toverlay_complete_l_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::OVERLAY_COMPLETE_F) {
			new_rule = new toverlay_complete_f_brule(*dynamic_cast<const toverlay_complete_f_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::OVERLAY_COMPLETE) {
			new_rule = new toverlay_complete_brule(*dynamic_cast<const toverlay_complete_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::VOLCANO_2x2) {
			new_rule = new tvolcano_2x2_brule(*dynamic_cast<const tvolcano_2x2_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::OVERLAY_RESTRICTED3_F) {
			new_rule = new toverlay_restricted3_f_brule(*dynamic_cast<const toverlay_restricted3_f_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::OVERLAY_RESTRICTED2_F) {
			new_rule = new toverlay_restricted2_f_brule(*dynamic_cast<const toverlay_restricted2_f_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::OVERLAY_RESTRICTED_P) {
			new_rule = new toverlay_restricted_p_brule(*dynamic_cast<const toverlay_restricted_p_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::OVERLAY_RESTRICTED_F) {
			new_rule = new toverlay_restricted_f_brule(*dynamic_cast<const toverlay_restricted_f_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::OVERLAY_ROTATION_RESTRICTED2_F) {
			new_rule = new toverlay_rotation_restricted2_f_brule(*dynamic_cast<const toverlay_rotation_restricted2_f_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::OVERLAY_ROTATION_RESTRICTED_F) {
			new_rule = new toverlay_rotation_restricted_f_brule(*dynamic_cast<const toverlay_rotation_restricted_f_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::MOUNTAINS_2x4_NW_SE) {
			new_rule = new tmountains_2x4_nw_se_brule(*dynamic_cast<const tmountains_2x4_nw_se_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::MOUNTAINS_2x4_SW_NE) {
			new_rule = new tmountains_2x4_sw_ne_brule(*dynamic_cast<const tmountains_2x4_sw_ne_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::MOUNTAINS_1x3_NW_SE) {
			new_rule = new tmountains_1x3_nw_se_brule(*dynamic_cast<const tmountains_1x3_nw_se_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::MOUNTAINS_1x3_SW_NE) {
			new_rule = new tmountains_1x3_sw_ne_brule(*dynamic_cast<const tmountains_1x3_sw_ne_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::MOUNTAINS_2x2) {
			new_rule = new tmountains_2x2_brule(*dynamic_cast<const tmountains_2x2_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::MOUNTAIN_SINGLE_RANDOM) {
			new_rule = new tmountain_single_random_brule(*dynamic_cast<const tmountain_single_random_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::PEAKS_1x2_SW_NE) {
			new_rule = new tpeaks_1x2_sw_ne_brule(*dynamic_cast<const tpeaks_1x2_sw_ne_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::PEAKS_LARGE) {
			new_rule = new tpeaks_large_brule(*dynamic_cast<const tpeaks_large_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::VILLAGE) {
			new_rule = new tvillage_brule(*dynamic_cast<const tvillage_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::VILLAGE_A3) {
			new_rule = new tvillage_a3_brule(*dynamic_cast<const tvillage_a3_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::VILLAGE_A4) {
			new_rule = new tvillage_a4_brule(*dynamic_cast<const tvillage_a4_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::NEW_FENCE) {
			new_rule = new tnew_fence_brule(*dynamic_cast<const tnew_fence_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::NEW_WALL_P) {
			new_rule = new tnew_wall_p_brule(*dynamic_cast<const tnew_wall_p_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::NEW_WALL) {
			new_rule = new tnew_wall_brule(*dynamic_cast<const tnew_wall_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::NEW_WALL2_P) {
			new_rule = new tnew_wall2_p_brule(*dynamic_cast<const tnew_wall2_p_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::NEW_WALL2) {
			new_rule = new tnew_wall2_brule(*dynamic_cast<const tnew_wall2_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::NEW_WAVES) {
			new_rule = new tnew_waves_brule(*dynamic_cast<const tnew_waves_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::NEW_BEACH) {
			new_rule = new tnew_beach_brule(*dynamic_cast<const tnew_beach_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::WALL_TRANSITION3) {
			new_rule = new twall_transition3_brule(*dynamic_cast<const twall_transition3_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::WALL_TRANSITION2_LF) {
			new_rule = new twall_transition2_lf_brule(*dynamic_cast<const twall_transition2_lf_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::WALL_TRANSITION_LF) {
			new_rule = new twall_transition_lf_brule(*dynamic_cast<const twall_transition_lf_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::WALL_ADJACENT_TRANSITION) {
			new_rule = new twall_adjacent_transition_brule(*dynamic_cast<const twall_adjacent_transition_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::ANIMATED_WATER_15) {
			new_rule = new tanimated_water_15_brule(*dynamic_cast<const tanimated_water_15_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::ANIMATED_WATER_15_TRANSITION) {
			new_rule = new tanimated_water_15_transition_brule(*dynamic_cast<const tanimated_water_15_transition_brule*>(rule));
		} else if (rule->type_ == tbuilding_rule::DISABLE_BASE_TRANSITIONS) {
			new_rule = new tdisable_base_transitions_brule(*dynamic_cast<const tdisable_base_transitions_brule*>(rule));
		}
												
		dst.push_back(new_rule);
	}
}

BOOL On_DlgTileEditInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	std::stringstream strstr;
	if (ns::action_rule == ma_edit) {
		strstr << _("Edit building rule");
	} else {
		strstr << _("Add building rule");
	}
	strstr << "(" << _("Number") << ":" << ns::clicked_rule << ")";
	SetWindowText(hdlgP, utf8_2_ansi(strstr.str().c_str()));

	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_MAP), utf8_2_ansi(_("brule^Map string")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_TILE), utf8_2_ansi(_("brule^Tile")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_IMAGE), utf8_2_ansi(_("brule^Image")));

	const tnone_brule& r = *dynamic_cast<const tnone_brule*>(ns::core.brules_updating_[ns::clicked_rule]);
	r.update_to_ui_tile_edit(hdlgP);
	return FALSE;
}

void On_DlgTileEditCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;

	switch (id) {

	case IDOK:
		changed = TRUE;
		(dynamic_cast<tnone_brule*>(ns::core.brules_updating_[ns::clicked_rule]))->from_ui_tile_edit(hdlgP);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

BOOL CALLBACK DlgTileEditProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message) {
	case WM_INITDIALOG:
		return On_DlgTileEditInitDialog(hDlg, (HWND)(wParam), lParam);
	HANDLE_MSG(hDlg, WM_COMMAND, On_DlgTileEditCommand);
	HANDLE_MSG(hDlg, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	}
	
	return FALSE;
}

void OnTileAddBt(HWND hdlgP)
{
/*
	// default to first feature
	ns::core.treasures_updating_.insert(std::make_pair(ns::core.treasures_updating_.size(), 0));

	ns::core.update_to_ui_treasure(hdlgP);
	ns::core.set_dirty(tcore::BIT_TREASURE, ns::core.treasures_dirty()); 
*/
}

void OnTileDelBt(HWND hdlgP)
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

void OnTileEditBt(HWND hdlgP)
{
	RECT rcBtn;
	LPARAM lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_LV_BRULENONE_TILE), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	ns::action_rule = ma_edit;
	int type = ns::core.brules_updating_[ns::clicked_rule]->type_;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_TILEEDIT), hdlgP, DlgTileEditProc, lParam)) {
		// ns::core.update_to_ui_terrain(hdlgP);
	}

	return;
}

BOOL On_DlgImageEditInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	std::stringstream strstr;
	if (ns::action_rule == ma_edit) {
		strstr << _("Edit building rule");
	} else {
		strstr << _("Add building rule");
	}
	strstr << "(" << _("Number") << ":" << ns::clicked_rule << ")";
	SetWindowText(hdlgP, utf8_2_ansi(strstr.str().c_str()));

	const tnone_brule& r = *dynamic_cast<const tnone_brule*>(ns::core.brules_updating_[ns::clicked_rule]);
	r.update_to_ui_image_edit(hdlgP);
	return FALSE;
}

void On_DlgImageEditCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;

	switch (id) {

	case IDOK:
		changed = TRUE;
		(dynamic_cast<tnone_brule*>(ns::core.brules_updating_[ns::clicked_rule]))->from_ui_image_edit(hdlgP);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

BOOL CALLBACK DlgImageEditProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message) {
	case WM_INITDIALOG:
		return On_DlgImageEditInitDialog(hDlg, (HWND)(wParam), lParam);
	HANDLE_MSG(hDlg, WM_COMMAND, On_DlgImageEditCommand);
	HANDLE_MSG(hDlg, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	}
	
	return FALSE;
}

void OnImageAddBt(HWND hdlgP)
{
/*
	// default to first feature
	ns::core.treasures_updating_.insert(std::make_pair(ns::core.treasures_updating_.size(), 0));

	ns::core.update_to_ui_treasure(hdlgP);
	ns::core.set_dirty(tcore::BIT_TREASURE, ns::core.treasures_dirty()); 
*/
}

void OnImageDelBt(HWND hdlgP)
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

void OnImageEditBt(HWND hdlgP)
{
	RECT rcBtn;
	LPARAM lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_LV_BRULENONE_IMAGE), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	ns::action_rule = ma_edit;
	int type = ns::core.brules_updating_[ns::clicked_rule]->type_;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_IMAGEEDIT), hdlgP, DlgImageEditProc, lParam)) {
		// ns::core.update_to_ui_terrain(hdlgP);
	}

	return;
}

// 对话框消息处理函数
BOOL On_DlgBuilderInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	HWND hwndParent = GetParent(hdlgP); 
    DLGHDR *pHdr = (DLGHDR *) GetWindowLong(hwndParent, GWL_USERDATA);
    SetWindowPos(hdlgP, HWND_TOP, pHdr->rcDisplay.left, pHdr->rcDisplay.top, 0, 0, SWP_NOSIZE); 

	HWND hctl = GetDlgItem(hdlgP, IDC_LV_BUILDER_EXPLORER);
	LVCOLUMN lvc;
	char text[_MAX_PATH];
	std::stringstream strstr;
	int column = 0;

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
	lvc.cx = 150;
	lvc.iSubItem = column;
	strcpy(text, utf8_2_ansi(_("Type")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 450;
	lvc.iSubItem = column;
	strstr.str("");
	strcpy(text, utf8_2_ansi(_("Description")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	ListView_SetImageList(hctl, gdmgr._himl, LVSIL_SMALL);
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	return FALSE;
}

void On_DlgBuilderCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
/*
	switch(id) {
	default:
		break;
	}
*/
	return;
}

BOOL On_DlgBruleNoneInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	std::stringstream strstr;
	if (ns::action_rule == ma_edit) {
		strstr << _("Edit building rule");
	} else {
		strstr << _("Add building rule");
	}
	strstr << "(" << _("Number") << ":" << ns::clicked_rule << ")";
	SetWindowText(hdlgP, utf8_2_ansi(strstr.str().c_str()));

	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_MAP), utf8_2_ansi(_("brule^Map string")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_TILE), utf8_2_ansi(_("brule^Tile")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_IMAGE), utf8_2_ansi(_("brule^Image")));

	const tbuilding_rule& r = *ns::core.brules_updating_[ns::clicked_rule];
	r.update_to_ui_brule_edit(hdlgP);
	return FALSE;
}

void On_DlgBruleNoneCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;

	switch (id) {

	case IDOK:
		changed = TRUE;
		ns::core.brules_updating_[ns::clicked_rule]->from_ui_brule_edit(hdlgP);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

void brulenone_notify_handler_dblclk(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
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
		if (lpNMHdr->idFrom == IDC_LV_BRULENONE_TILE) {
			ns::clicked_tile = lpnmitem->iItem;
			OnTileEditBt(hdlgP);
		} else if (lpNMHdr->idFrom == IDC_LV_BRULENONE_IMAGE) {
			ns::clicked_image = lpnmitem->iItem;
			OnImageEditBt(hdlgP);
		}
	}
    return;
}

BOOL On_DlgBruleNoneNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	if (lpNMHdr->code == NM_RCLICK) {
		// brulenone_notify_handler_rclick(hdlgP, DlgItem, lpNMHdr);
	} else if (lpNMHdr->code == NM_DBLCLK) {
		brulenone_notify_handler_dblclk(hdlgP, DlgItem, lpNMHdr);
	}
	return FALSE;
}

BOOL CALLBACK DlgBruleNoneProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message) {
	case WM_INITDIALOG:
		return On_DlgBruleNoneInitDialog(hDlg, (HWND)(wParam), lParam);
	HANDLE_MSG(hDlg, WM_COMMAND, On_DlgBruleNoneCommand);
	HANDLE_MSG(hDlg, WM_NOTIFY, On_DlgBruleNoneNotify);
	HANDLE_MSG(hDlg, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	}
	
	return FALSE;
}

BOOL On_DlgBruleNormalInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	const tbuilding_rule& r = *ns::core.brules_updating_[ns::clicked_rule];
	std::stringstream strstr;
	if (ns::action_rule == ma_edit) {
		strstr << _("Edit building rule");
	} else {
		strstr << _("Add building rule");
	}
	strstr << "(" << _("Number") << ":" << ns::clicked_rule << ")";
	strstr << "(" << tbuilding_rule::macro_tag(r.type_) << ")";
	SetWindowText(hdlgP, utf8_2_ansi(strstr.str().c_str()));

	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_DESCRIPTION), utf8_2_ansi(_("Description")));

	r.update_to_ui_brule_edit(hdlgP);
	return FALSE;
}

void On_DlgBruleNormalCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;

	switch (id) {

	case IDOK:
		changed = TRUE;
		ns::core.brules_updating_[ns::clicked_rule]->from_ui_brule_edit(hdlgP);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

BOOL CALLBACK DlgBruleNormalProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message) {
	case WM_INITDIALOG:
		return On_DlgBruleNormalInitDialog(hDlg, (HWND)(wParam), lParam);
	HANDLE_MSG(hDlg, WM_COMMAND, On_DlgBruleNormalCommand);
	HANDLE_MSG(hDlg, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	}
	
	return FALSE;
}

void OnBuilderAddBt(HWND hdlgP)
{
/*
	// default to first feature
	ns::core.treasures_updating_.insert(std::make_pair(ns::core.treasures_updating_.size(), 0));

	ns::core.update_to_ui_treasure(hdlgP);
	ns::core.set_dirty(tcore::BIT_TREASURE, ns::core.treasures_dirty()); 
*/
}

void OnBuilderDelBt(HWND hdlgP)
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

void OnBuilderEditBt(HWND hdlgP)
{
	RECT rcBtn;
	LPARAM lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_LV_BUILDER_EXPLORER), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	ns::action_rule = ma_edit;
	int type = ns::core.brules_updating_[ns::clicked_rule]->type_;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(tbuilding_rule::idd_map[type]), hdlgP, tbuilding_rule::dlgproc_map[type], lParam)) {
		// ns::core.update_to_ui_terrain(hdlgP);
		ns::core.set_dirty(tcore::BIT_BUILDER, ns::core.brules_dirty());
	}

	return;
}

void builder_notify_handler_dblclk(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
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
		if (lpNMHdr->idFrom == IDC_LV_BUILDER_EXPLORER) {
			ns::clicked_rule = lpnmitem->iItem;
			OnBuilderEditBt(hdlgP);
		}
	}
    return;
}

BOOL On_DlgBuilderNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	if (lpNMHdr->code == NM_RCLICK) {
		// builder_notify_handler_rclick(hdlgP, DlgItem, lpNMHdr);
	} else if (lpNMHdr->code == NM_DBLCLK) {
		builder_notify_handler_dblclk(hdlgP, DlgItem, lpNMHdr);
	}
	return FALSE;
}

void On_DlgBuilderDestroy(HWND hdlgP)
{
	return;
}

BOOL CALLBACK DlgBuilderProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	LPPSHNOTIFY			lppsn = (LPPSHNOTIFY)lParam;

	switch(uMsg)
	{
	case WM_INITDIALOG:
		return On_DlgBuilderInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgBuilderCommand);
	HANDLE_MSG(hdlgP, WM_NOTIFY,  On_DlgBuilderNotify);
	HANDLE_MSG(hdlgP, WM_DESTROY, On_DlgBuilderDestroy);
	}
	
	return FALSE;
}

