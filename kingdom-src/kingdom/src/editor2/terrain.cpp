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
	int clicked_terrain;
	
	int action_terrain;
}

void tterrain::from_config(const terrain_type& info)
{
	std::stringstream strstr;

	symbol_image_ = info.minimap_image();
	if (symbol_image_ != info.editor_image()) {
		editor_image_ = info.editor_image();
	}
	id_ = info.id();
	std::vector<t_string_base::trans_str> trans = info.name().valuex();
	for (std::vector<t_string_base::trans_str>::const_iterator ti = trans.begin(); ti != trans.end(); ti ++) {
		name_ = ti->str;
		break;
	}
	if (info.name() != info.editor_name()) {
		trans = info.editor_name().valuex();
		for (std::vector<t_string_base::trans_str>::const_iterator ti = trans.begin(); ti != trans.end(); ti ++) {
			editor_name_ = ti->str;
			break;
		}
	}
	string_ = t_terrain_2_str(info.number());

	const t_translation::t_list& mvt_type = info.mvt_type();
	const t_translation::t_list& def_type = info.def_type();
	const std::string& aliasof = info.aliasof();

	strstr.str("");
	for (t_translation::t_list::const_iterator it = mvt_type.begin(); it != mvt_type.end(); ++ it) {
		const t_translation::t_terrain& t = *it;
		if (it != mvt_type.begin()) {
			strstr << ", ";
		}
		strstr << t_terrain_2_str(t);
	}
	mvt_alias_ = strstr.str();

	strstr.str("");
	for (t_translation::t_list::const_iterator it = def_type.begin(); it != def_type.end(); ++ it) {
		const t_translation::t_terrain& t = *it;
		if (it != def_type.begin()) {
			strstr << ", ";
		}
		strstr << t_terrain_2_str(t);
	}
	def_alias_ = strstr.str();

	std::vector<std::string> vstr = utils::split(aliasof);
	strstr.str("");
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		if (it != vstr.begin()) {
			strstr << ", ";
		}
		strstr << *it;
	}
	aliasof_ = strstr.str();

	if (aliasof_ == mvt_alias_) {
		mvt_alias_ = "";
	}
	if (aliasof_ == def_alias_) {
		def_alias_ = "";
	}
	if (aliasof_.empty() && mvt_type == def_type) {
		aliasof_ = mvt_alias_;
		mvt_alias_ = "";
		def_alias_ = "";
	}

	height_adjust_ = info.unit_height_adjust();
	submerge_ = info.unit_submerge();
	light_modification_ = info.light_modification();
	max_light_ = info.max_light();
	min_light_ = info.min_light();
	heals_ = info.gives_healing();
	village_ = info.is_village();
	editor_group_ = info.editor_group();
	if (info.default_base() != t_translation::NONE_TERRAIN) {
		editor_default_base_ = t_terrain_2_str(info.default_base());
	}
	hide_in_editor_ = info.hide_in_editor();

	terrain_from_cfg_ = *this;
}

void tterrain::from_ui_terrain_edit(HWND hdlgP)
{
	char text[_MAX_PATH];
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_TERRAINEDIT_STRING), text, sizeof(text) / sizeof(text[0]));
	string_ = text;

	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_TERRAINEDIT_NAME), text, sizeof(text) / sizeof(text[0]));
	name_ = text;
}

void tterrain::update_to_ui_terrain_edit(HWND hdlgP) const
{
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_TERRAINEDIT_STRING), string_.c_str());
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_TERRAINEDIT_ID), id_.c_str());
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_TERRAINEDIT_NAME_MSGID), name_.c_str());
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_TERRAINEDIT_ALIAS), aliasof_.c_str());
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_TERRAINEDIT_MVTALIAS), mvt_alias_.c_str());
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_TERRAINEDIT_DEFALIAS), def_alias_.c_str());
}

void tterrain::generate(std::stringstream& strstr) const
{
	strstr << "[terrain_type]\n";
	strstr << "\tsymbol_image=" << symbol_image_ << "\n";
	if (!editor_image_.empty()) {
		strstr << "\teditor_image=" << editor_image_ << "\n";
	}
	if (!id_.empty()) {
		strstr << "\tid=" << id_ << "\n";
	}
	if (!name_.empty()) {
		strstr << "\tname=_ \"" << name_ << "\"\n";
	}
	if (!editor_name_.empty()) {
		strstr << "\teditor_name=_ \"" << editor_name_ << "\"\n";
	}
	strstr << "\tstring=" << string_ << "\n";
	if (!editor_default_base_.empty()) {
		strstr << "\tdefault_base=" << editor_default_base_ << "\n";
	}
	if (!aliasof_.empty() && string_ != aliasof_) {
		strstr << "\taliasof=" << aliasof_ << "\n";
	}
	if (!mvt_alias_.empty()) {
		strstr << "\tmvt_alias=" << mvt_alias_ << "\n";
	}
	if (!def_alias_.empty()) {
		strstr << "\tdef_alias=" << def_alias_ << "\n";
	}
	if (heals_ != 0) {
		strstr << "\theals=" << heals_ << "\n";
	}
	if (submerge_ != 0) {
		strstr << "\tsubmerge=" << submerge_ << "\n";
	}
	if (height_adjust_) {
		strstr << "\tunit_height_adjust=" << height_adjust_ << "\n";
	}
	if (light_modification_) {
		strstr << "\tlight=" << light_modification_ << "\n";
	}
	if (min_light_ != light_modification_) {
		strstr << "\tmin_light=" << min_light_ << "\n";
	}
	if (max_light_ != light_modification_) {
		strstr << "\tmax_light=" << max_light_ << "\n";
	}
	if (village_) {
		strstr << "\tgives_income=true\n";
	}
	if (!editor_group_.empty()) {
		strstr << "\teditor_group=" << editor_group_ << "\n";
	}
	if (hide_in_editor_) {
		strstr << "\thidden=yes\n";
	}
	strstr << "[/terrain_type]\n";
	strstr << "\n";
}

void tcore::update_to_ui_terrain(HWND hdlgP)
{
	HWND hctl = GetDlgItem(hdlgP, IDC_LV_TERRAIN_EXPLORER);
	LVITEM lvi;
	ListView_DeleteAllItems(hctl);
	std::stringstream strstr;
	char text[_MAX_PATH];
	int row = 0;

	for (std::vector<tterrain>::const_iterator it = terrains_updating_.begin(); it != terrains_updating_.end(); ++ it) {
		const tterrain& t = *it;
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

		// string
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = column ++;
		strstr.str("");
		strstr << t.string_;
		if (t.hide_in_editor_) {
			strstr << "(" << _("Hidden") << ")";
		}
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// id
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = column ++;
		strstr.str("");
		strstr << t.id_;
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// name
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = column ++;
		strstr.str("");
		if (!t.name_.empty()) {
			strstr << dsgettext(t.textdomain_.c_str(), t.name_.c_str());
			strstr << "(" << t.name_ << ")";
		}
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// aliasof
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = column ++;
		strstr.str("");
		strstr << t.aliasof_;
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// movement alias
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = column ++;
		strstr.str("");
		strstr << t.mvt_alias_;
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// defend alias
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = column ++;
		strstr.str("");
		strstr << t.def_alias_;
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// height adjust
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = column ++;
		strstr.str("");
		strstr << t.height_adjust_;
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// submerge
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = column ++;
		strstr.str("");
		strstr << t.submerge_;
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// light modification
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = column ++;
		strstr.str("");
		strstr << t.light_modification_;
		if (t.min_light_ != t.max_light_) {
			strstr << "(" << t.min_light_ << "~" << t.max_light_ << ")";
		}
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// heals
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = column ++;
		strstr.str("");
		strstr << t.heals_;
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// village
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = column ++;
		strstr.str("");
		strstr << (t.village_? _("Yes"): _("No"));
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// group(editor)
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = column ++;
		strstr.str("");
		strstr << t.editor_group_;
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// name(editor)
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = column ++;
		strstr.str("");
		if (!t.editor_name_.empty()) {
			strstr << dsgettext(t.textdomain_.c_str(), t.editor_name_.c_str());
			strstr << "(" << t.editor_name_ << ")";
		}
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// default base(editor)
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = column ++;
		strstr.str("");
		strstr << t.editor_default_base_;
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// symbol_image(minimap)
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = column ++;
		strstr.str("");
		strstr << t.symbol_image_;
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// editor image(editor)
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = column ++;
		strstr.str("");
		strstr << t.editor_image_;
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);
	}
}

void tterrain::set_edit_invalid(int bit, bool set, HWND ok)
{
	if (set) {
		edit_invalid_ |= 1 << bit;
	} else {
		edit_invalid_ &= ~(1 << bit);
	}
	Button_Enable(ok, !edit_invalid_);
}

bool tterrain::dirty() const
{
	if (*this != terrain_from_cfg_) {
		return true;
	}
	return false;
}

BOOL On_DlgTerrainInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	HWND hwndParent = GetParent(hdlgP); 
    DLGHDR *pHdr = (DLGHDR *) GetWindowLong(hwndParent, GWL_USERDATA);
    SetWindowPos(hdlgP, HWND_TOP, pHdr->rcDisplay.left, pHdr->rcDisplay.top, 0, 0, SWP_NOSIZE); 

	HWND hctl = GetDlgItem(hdlgP, IDC_LV_TERRAIN_EXPLORER);
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
	lvc.cx = 80;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << _("terrain^String");
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 120;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << _("terrain^ID");
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 90;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << "object^Name";
	strcpy(text, utf8_2_ansi(dsgettext("wesnoth-lib", strstr.str().c_str())));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 80;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << _("Alias");
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 80;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << _("Alias(mvt)");
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 80;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << _("Alias(def)");
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 80;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << _("Height");
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 80;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << _("Submerge");
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 80;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << _("terrain^Light");
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 80;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << _("Heals");
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 80;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << "Village";
	strcpy(text, dgettext_2_ansi("wesnoth-lib", strstr.str().c_str()));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);
	
	lvc.cx = 80;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << _("Group");
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 90;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << _("Editor name");
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 80;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << _("Default base");
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 150;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << _("Symbol image");
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 150;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << _("Editor image");
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	ListView_SetImageList(hctl, gdmgr._himl, LVSIL_SMALL);
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	return FALSE;
}

BOOL On_DlgTerrainEditInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

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

/*
	// movement_type
	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_TREASUREEDIT_FEATURE);
	std::vector<int>& features = hero::valid_features();
	for (std::vector<int>::const_iterator it = features.begin(); it != features.end(); ++ it) {
		ComboBox_AddString(hctl, utf8_2_ansi(hero::feature_str(*it).c_str())); 
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, *it);
	}
*/
	tterrain& t = ns::core.terrains_updating_[ns::clicked_terrain];
	t.edit_invalid_ = 0;
	t.update_to_ui_terrain_edit(hdlgP);
	return FALSE;
}

static bool is_terrain_char(const char c) 
{
	return c == '^';
}

bool is_valid_terrain_str(const std::string& str)
{
	const size_t valid_char = std::count_if(str.begin(), str.end(), is_terrain_char);
	if (valid_char >= 2) {
		return false;
	}

	int continuous_alphas = 0;
	for (size_t i = 0; i < str.size(); ++ i) {
		char c = str[i];
		if (!isalpha(c) && c != '^' && c != '_' && c != '|' && c != '/' && c != '\\') {
			return false;
		}
		if (c == '^') {
			continuous_alphas = 0;
		} else if (++ continuous_alphas > 4) {
			return false;
		} else if (continuous_alphas == 1 && !isupper(c) && c != '_') {
			return false;
		}
	}
	return t_translation::read_terrain_code(str) != t_translation::NONE_TERRAIN;
}

bool is_unique_string(const std::vector<tterrain>& terrains, const std::string& str, int except)
{
	int size = (int)terrains.size();
	for (int i = 0; i < size; i ++) {
		if (i == except) {
			continue;
		}
		if (terrains[i].string_ == str) {
			return false;
		}
	}
	return true;
}

bool is_valid_alias_str(const std::string& alias)
{
	const std::vector<std::string> vstr = utils::split(alias);
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		const std::string& str = *it;
		if (str[0] == '^') {
			return false;
		}
		if (str == "-" || str == "+") {
			continue;
		}
		if (is_valid_terrain_str(str)) {
			continue;
		}
		return false;
	}
	return true;
}

void OnTerrainEditEt(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
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
		bit = tterrain::EDITBIT_NAME;
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
	return;
}

void On_DlgTerrainEditCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;

	switch (id) {
	case IDC_ET_TERRAINEDIT_STRING:
	case IDC_ET_TERRAINEDIT_ID:
	case IDC_ET_TERRAINEDIT_NAME_MSGID:
	case IDC_ET_TERRAINEDIT_ALIAS:
	case IDC_ET_TERRAINEDIT_MVTALIAS:
	case IDC_ET_TERRAINEDIT_DEFALIAS:
		OnTerrainEditEt(hdlgP, id, hwndCtrl, codeNotify);
		break;

	case IDOK:
		changed = TRUE;
		ns::core.terrains_updating_[ns::clicked_terrain].from_ui_terrain_edit(hdlgP);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

BOOL CALLBACK DlgTerrainEditProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message) {
	case WM_INITDIALOG:
		return On_DlgTerrainEditInitDialog(hDlg, (HWND)(wParam), lParam);
	HANDLE_MSG(hDlg, WM_COMMAND, On_DlgTerrainEditCommand);
	HANDLE_MSG(hDlg, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	}
	
	return FALSE;
}

void OnTerrainAddBt(HWND hdlgP)
{
/*
	// default to first feature
	ns::core.treasures_updating_.insert(std::make_pair(ns::core.treasures_updating_.size(), 0));

	ns::core.update_to_ui_treasure(hdlgP);
	ns::core.set_dirty(tcore::BIT_TREASURE, ns::core.treasures_dirty()); 
*/
}

void OnTerrainDelBt(HWND hdlgP)
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

void OnTerrainEditBt(HWND hdlgP)
{
	RECT rcBtn;
	LPARAM lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_LV_TERRAIN_EXPLORER), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	ns::action_terrain = ma_edit;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_TERRAINEDIT), hdlgP, DlgTerrainEditProc, lParam)) {
		ns::core.update_to_ui_terrain(hdlgP);
		ns::core.set_dirty(tcore::BIT_TERRAIN, ns::core.terrains_updating_[ns::clicked_terrain].dirty());
	}

	return;
}

void On_DlgTerrainCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	return;
}

void terrain_notify_handler_rclick(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
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

void terrain_notify_handler_dblclk(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
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
		if (lpNMHdr->idFrom == IDC_LV_TERRAIN_EXPLORER) {
			ns::clicked_terrain = lpnmitem->iItem;
			OnTerrainEditBt(hdlgP);
		}
	}
    return;
}

BOOL On_DlgTerrainNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	if (lpNMHdr->code == NM_RCLICK) {
		terrain_notify_handler_rclick(hdlgP, DlgItem, lpNMHdr);
	} else if (lpNMHdr->code == NM_DBLCLK) {
		terrain_notify_handler_dblclk(hdlgP, DlgItem, lpNMHdr);
	}
	return FALSE;
}

void On_DlgTerrainDestroy(HWND hdlgP)
{
	return;
}

BOOL CALLBACK DlgTerrainProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_INITDIALOG:
		return On_DlgTerrainInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgTerrainCommand);
	HANDLE_MSG(hdlgP, WM_NOTIFY,  On_DlgTerrainNotify);
	HANDLE_MSG(hdlgP, WM_DESTROY, On_DlgTerrainDestroy);
	}
	
	return FALSE;
}