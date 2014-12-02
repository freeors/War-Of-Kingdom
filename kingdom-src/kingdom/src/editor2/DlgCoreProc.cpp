#define GETTEXT_DOMAIN "wesnoth-maker"

#include "global.hpp"
#include "game_config.hpp"
#include "loadscreen.hpp"
#include "DlgCoreProc.hpp"
#include "gettext.hpp"
#include "formula_string_utils.hpp"
#include <string.h>

#include "resource.h"

#include "xfunc.h"
#include "win32x.h"
#include "struct.h"

#include "serialization/parser.hpp"
#include <boost/foreach.hpp>
#include "map.hpp"
#include "builder.hpp"
#include "area_anim.hpp"

BOOL CALLBACK DlgTreasureEditProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

std::vector<std::string> attack_range_vstr;
std::vector<std::string> attack_type_vstr;

#ifndef _ROSE_EDITOR

namespace explorer_technology {
int explorer = NONE;

void update_to_ui_special(HWND hdlgP, int flag)
{
	if (flag == NONE) {
		return;
	}

	std::stringstream strstr;
	std::set<std::string>* technology = NULL;
	if (flag == explorer_technology::SCENARIO) {
		tscenario& scenario = ns::_scenario[ns::current_scenario];
		tside& side = scenario.side_[ns::clicked_side];
		technology = &side.technologies_;
	} else if (flag == explorer_technology::MODIFY_SIDE) {
		tevent::tmodify_side* modify_side = dynamic_cast<tevent::tmodify_side*>(ns::clicked_command);
		technology = &modify_side->technology_;
	}
	strstr.str("");
	for (std::set<std::string>::const_iterator it = technology->begin(); it != technology->end(); ++ it) {
		if (it != technology->begin()) {
			strstr << ", ";
		}
		const ttechnology& t = unit_types.technology(*it);
		strstr << t.name();
	}
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_TECHNOLOGY), utf8_2_ansi(strstr.str().c_str()));

	strstr.str("");
	strstr << dgettext_2_ansi("wesnoth-lib", "Technology") << "(" << technology->size() << ")";
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_TECHNOLOGY), strstr.str().c_str());
}

};
#endif

namespace ns {
	const std::string default_utype_textdomain = "wesnoth-tk-units";
	const std::string default_utype_id = "utype_id_";
	const std::string default_attack_id = "attack_id_";
	const std::string default_attack_icon = "sword-human.png";

	tcore core;
#ifndef _ROSE_EDITOR
	tunit_type utype;
#endif
	HIMAGELIST himl_tactic;
	int iico_tactic_tactic;	
	int iico_tactic_range;
	int iico_tactic_action;
	int iico_tactic_txt;

	HIMAGELIST himl_technology;
	int iico_technology_technology;	
	int iico_technology_experience;
	int iico_technology_action;
	int iico_technology_txt;
}

std::map<int, std::string> tcore::name_map;
std::map<int, int> tcore::idd_map;
std::map<int, DLGPROC> tcore::dlgproc_map;

tcore::tcore()
	: section_(CONFIG)
	, map_(NULL)
#ifndef _ROSE_EDITOR
	, types_updating_()
	, features_from_cfg_()
	, features_updating_()
	, treasures_from_cfg_()
	, treasures_updating_()
	, factions_from_cfg_()
	, factions_updating_()
#endif
	, terrains_from_cfg_()
	, terrains_updating_()
	, brules_from_cfg_()
	, brules_updating_()
	, anims_from_cfg_()
	, anims_updating_()
	, books()
	, current_book_()
	, config_from_cfg_()
	, config_updating_()
	, dirty_(0)
{
	attack_range_vstr.push_back("melee");
	attack_range_vstr.push_back("ranged");
	attack_range_vstr.push_back("cast");

	attack_type_vstr.push_back("blade");
	attack_type_vstr.push_back("pierce");
	attack_type_vstr.push_back("impact");
	attack_type_vstr.push_back("archery");
	attack_type_vstr.push_back("collapse");
	attack_type_vstr.push_back("arcane");
	attack_type_vstr.push_back("fire");
	attack_type_vstr.push_back("cold");
}

tcore::~tcore()
{
	if (map_) {
		delete map_;
	}
	if (builder_) {
		terrain_builder::release_heap();
		delete builder_;
	}
	attack_range_vstr.clear();
	attack_type_vstr.clear();

	release_builder();
}

HWND tcore::init_toolbar(HINSTANCE hinst, HWND hdlgP)
{
	// Create a toolbar
	gdmgr._htb_core = CreateWindowEx(0, TOOLBARCLASSNAME, (LPSTR)NULL, 
		WS_CHILD | /*CCS_ADJUSTABLE |*/ TBSTYLE_TOOLTIPS | TBSTYLE_FLAT, 0, 0, 0, 0, hdlgP, 
		(HMENU)IDR_WGENMENU, gdmgr._hinst, NULL);

	//Enable multiple image lists
    SendMessage(gdmgr._htb_core, CCM_SETVERSION, (WPARAM) 5, 0); 

	// Send the TB_BUTTONSTRUCTSIZE message, which is required for backward compatibility
	ToolBar_ButtonStructSize(gdmgr._htb_core, sizeof(TBBUTTON));
	
	gdmgr._tbBtns_core[0].iBitmap = MAKELONG(gdmgr._iico_save, 0);
	gdmgr._tbBtns_core[0].idCommand = IDM_SAVE;	
	gdmgr._tbBtns_core[0].fsState = TBSTATE_ENABLED;
	gdmgr._tbBtns_core[0].fsStyle = BTNS_BUTTON;
	gdmgr._tbBtns_core[0].dwData = 0L;
	gdmgr._tbBtns_core[0].iString = -1;

	gdmgr._tbBtns_core[1].iBitmap = 100;
	gdmgr._tbBtns_core[1].idCommand = 0;	
	gdmgr._tbBtns_core[1].fsState = 0;
	gdmgr._tbBtns_core[1].fsStyle = TBSTYLE_SEP;
	gdmgr._tbBtns_core[1].dwData = 0L;
	gdmgr._tbBtns_core[1].iString = 0;

	gdmgr._tbBtns_core[2].iBitmap = MAKELONG(gdmgr._iico_new, 0);
	gdmgr._tbBtns_core[2].idCommand = IDM_NEW;	
	gdmgr._tbBtns_core[2].fsState = TBSTATE_ENABLED;
	gdmgr._tbBtns_core[2].fsStyle = BTNS_BUTTON;
	gdmgr._tbBtns_core[2].dwData = 0L;
	gdmgr._tbBtns_core[2].iString = -1;


	gdmgr._tbBtns_core[3].iBitmap = MAKELONG(gdmgr._iico_del, 0);
	gdmgr._tbBtns_core[3].idCommand = IDM_DELETE;	
	gdmgr._tbBtns_core[3].fsState = TBSTATE_ENABLED;
	gdmgr._tbBtns_core[3].fsStyle = BTNS_BUTTON;
	gdmgr._tbBtns_core[3].dwData = 0L;
	gdmgr._tbBtns_core[3].iString = -1;

	ToolBar_AddButtons(gdmgr._htb_core, 4, &gdmgr._tbBtns_core);

	ToolBar_AutoSize(gdmgr._htb_core);
	
	ToolBar_SetExtendedStyle(gdmgr._htb_core, TBSTYLE_EX_DRAWDDARROWS);
	
	ToolBar_SetImageList(gdmgr._htb_core, gdmgr._himl_24x24, 0);
	ToolBar_SetDisabledImageList(gdmgr._htb_core, gdmgr._himl_24x24_dis);
	
	ShowWindow(gdmgr._htb_core, SW_SHOW);

	return gdmgr._htb_core;
}

void tcore::set_dirty(int bit, bool set)
{
	if (set) {
		dirty_ |= 1 << bit;
	} else {
		dirty_ &= ~(1 << bit);
	}
	core_enable_save_btn(dirty_? TRUE: FALSE);
}

bool tcore::bit_dirty(int bit) const
{
	return dirty_ & (1 << bit);
}

bool tcore::save_if_dirty()
{
	if (core_get_save_btn()) {
		std::stringstream title, message;
		HWND hdlgP = gdmgr._hdlg_core;

		title << utf8_2_ansi(_("Save modify")); 
		message << utf8_2_ansi(_("Core data is dirty, do you want to save modify?"));

		int retval = MessageBox(gdmgr._htb_core, message.str().c_str(), title.str().c_str(), MB_YESNO);
		bool ret;
		if (retval == IDYES) {
			save(gdmgr._hdlg_core);
			ret = true;
		} else {
			core_enable_save_btn(FALSE);
			ret = false;
		}
#ifndef _ROSE_EDITOR
		// clear updating flag
		types_updating_.clear();
#endif
		dirty_ = 0;
		return ret;
	}
	return false;
}

void tcore::save(HWND hdlgP)
{
	std::stringstream strstr;
	DLGHDR* pHdr = (DLGHDR *) GetWindowLong(hdlgP, GWL_USERDATA);

	//
	// verify core data
	//
#ifndef _ROSE_EDITOR
	for (std::vector<tfeature>::iterator it = features_updating_.begin(); it != features_updating_.end(); ++ it) {
		const tfeature& f = *it;
		int feature = std::distance(features_updating_.begin(), it);
		if (feature >= HEROS_BASE_FEATURE_COUNT && f.items_.empty()) {
			strstr << "#" << feature << "没设置原子特技";
			posix_print_mb(strstr.str().c_str());
			return;
		}
	}

	for (std::vector<tfaction>::iterator it = factions_updating_.begin(); it != factions_updating_.end(); ++ it) {
		const tfaction& f = *it;
		if (f.leader_ == HEROS_INVALID_NUMBER) {
			strstr << "#" << std::distance(factions_updating_.begin(), it) << "集团没设置君主";
			posix_print_mb(strstr.str().c_str());
			return;
		}
		if (f.city_ == HEROS_INVALID_NUMBER) {
			strstr << "#" << std::distance(factions_updating_.begin(), it) << "集团没设置城市";
			posix_print_mb(strstr.str().c_str());
			return;
		}
	}

	if (bit_dirty(BIT_MULTIPLAYER)) {
		if (!campaign_can_save(section_ == MULTIPLAYER? pHdr->hwndDisplay: NULL, false)) {
			return;
		}
	}

	//
	// write section
	//
	// one or more utype.id changed, delete <units>/<race>/<id>.cfg.
	for (std::map<int, tunit_type>::const_iterator it = types_updating_.begin(); it != types_updating_.end(); ++ it) {
		const tunit_type& type = it->second;
		if (type.utype_from_cfg_.id_ != type.id_) {
			strstr.str("");
			strstr << type.cfg_file(true);

			delfile1(strstr.str().c_str());
		}
		if (!type.id_.empty() && type.dirty()) {
			type.generate(false);
		}
	}
	types_updating_.clear();

	// noble
	if (bit_dirty(BIT_NOBLE)) {
		generate_noble_cfg();
	}
	nobles_from_cfg_.clear();
	nobles_updating_.clear();

	// units_internal.cfg, include feature/treasure
	if (bit_dirty(BIT_FEATURE) || bit_dirty(BIT_TREASURE)) {
		generate_units_internal();
	}

	// faction
	if (bit_dirty(BIT_FACTION)) {
		generate_factions_cfg();
	}
	factions_from_cfg_.clear();
	factions_updating_.clear();

	// multiplayer
	if (bit_dirty(BIT_MULTIPLAYER)) {
		multiplayer_.generate();
	}
	multiplayer_.clear();

#endif
	// terrain
	if (bit_dirty(BIT_TERRAIN)) {
		generate_terrain_cfg();
	}
	terrains_from_cfg_.clear();
	terrains_updating_.clear();

	// builder
	if (bit_dirty(BIT_BUILDER)) {
		generate_terrain_graphics_cfg();
	}
	release_builder();

	// anim
	if (bit_dirty(BIT_ANIM)) {
		generate_anims_cfg();
	}
	anims_from_cfg_.clear();
	anims_updating_.clear();

	// game config
	if (bit_dirty(BIT_CONFIG)) {
		generate_config_cfg();
	}
	config_from_cfg_.clear();
	config_updating_.clear();

	// book
	if (bit_dirty(BIT_BOOK)) {
		generate_books_cfg();
	}
	books.clear();

	core_enable_save_btn(FALSE);
	// clear updating flag
	dirty_ = 0;

	editor_config::campaign_id = "multiplayer";
	sync_refresh_sync();
}

#ifndef _ROSE_EDITOR
void tcore::utype_tree_2_tv_internal(HWND hctl, HTREEITEM htvroot, const std::vector<advance_tree::node>& advances_to, const std::vector<std::string>& advances_from2)
{
	char text[_MAX_PATH];
	HTREEITEM htvi;
	std::stringstream strstr;
	utils::string_map symbols;
	std::vector<std::string> advances_from = advances_from2;

	size_t index = 0;
	for (std::vector<advance_tree::node>::const_iterator it = advances_to.begin(); it != advances_to.end(); ++ it, index ++) {
		const unit_type* current = dynamic_cast<const unit_type*>(it->current);
		htvi = TreeView_AddLeaf(hctl, htvroot);
		strstr.str("");
		strstr << utf8_2_ansi(current->type_name().c_str()) << "(" << current->id() << ")[";
		symbols["level"] = lexical_cast_default<std::string>(current->level());
		strstr << utf8_2_ansi(vgettext2("Lv$level", symbols).c_str()) << "]";
		strcpy(text, strstr.str().c_str());
		// 枚举到此为止,此个config一定有孩子,强制让出来前面+符号
		LPARAM lParam = tv_tree_.size();
		tv_tree_.push_back(std::make_pair(current->id(), advances_from));
		if (it->advances_to.empty()) {
			TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM, lParam, gdmgr._iico_dir, gdmgr._iico_dir, 1, text);
		} else {
			TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_CHILDREN, lParam, gdmgr._iico_dir, gdmgr._iico_dir, 1, text);
			advances_from.push_back(current->id());
			utype_tree_2_tv_internal(hctl, htvi, it->advances_to, advances_from);
			TreeView_Expand(hctl, htvi, TVE_EXPAND);
		}
	}
}

// void advancement_tree_internal(const unit_type& current, 
void tcore::utype_tree_2_tv(HWND hctl, HTREEITEM htvroot)
{
	char text[_MAX_PATH];
	HTREEITEM htvi;
	std::stringstream strstr;
	utils::string_map symbols;
	std::vector<std::string> advances_from;

	size_t index = 0;
	const std::vector<advance_tree::node*>& utype_tree = unit_types.utype_tree();
	for (std::vector<advance_tree::node*>::const_iterator it = utype_tree.begin(); it != utype_tree.end(); ++ it, index ++) {
		const advance_tree::node* n = *it;
		const unit_type* current = dynamic_cast<const unit_type*>(n->current);
		htvi = TreeView_AddLeaf(hctl, htvroot);
		strstr.str("");
		strstr << std::setw(2) << std::setfill('0') << index << ": " << utf8_2_ansi(current->type_name().c_str()) << "(" << current->id() << ")";
		if (!current->packer()) {
			symbols["level"] = lexical_cast_default<std::string>(current->level());
			strstr << "[" << utf8_2_ansi(vgettext2("Lv$level", symbols).c_str()) << "]";
		} else {
			strstr << "[" << utf8_2_ansi(_("Packer")) << "]";
		}
		strcpy(text, strstr.str().c_str());
		// 枚举到此为止,此个config一定有孩子,强制让出来前面+符号
		LPARAM lParam = tv_tree_.size();
		advances_from.clear();
		tv_tree_.push_back(std::make_pair(current->id(), advances_from));
		if (n->advances_to.empty()) {
			TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM, lParam, gdmgr._iico_dir, gdmgr._iico_dir, 1, text);
		} else {
			TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_CHILDREN, lParam, gdmgr._iico_dir, gdmgr._iico_dir, 1, text);
			advances_from.push_back(current->id());
			utype_tree_2_tv_internal(hctl, htvi, n->advances_to, advances_from);
			TreeView_Expand(hctl, htvi, TVE_EXPAND);
		}
	}
}

void tcore::technology_tree_2_tv_internal(HWND hctl, HTREEITEM htvroot, const std::vector<advance_tree::node>& advances_to, const std::vector<std::string>& advances_from2)
{
	char text[_MAX_PATH];
	HTREEITEM htvi;
	std::stringstream strstr;
	utils::string_map symbols;
	std::vector<std::string> advances_from = advances_from2;

	size_t index = 0;
	for (std::vector<advance_tree::node>::const_iterator it = advances_to.begin(); it != advances_to.end(); ++ it, index ++) {
		const ttechnology* current = dynamic_cast<const ttechnology*>(it->current);
		htvi = TreeView_AddLeaf(hctl, htvroot);
		strstr.str("");
		strstr << utf8_2_ansi(current->name().c_str()) << "(" << current->id() << ")[";
		symbols["level"] = lexical_cast_default<std::string>(current->max_experience());
		strstr << utf8_2_ansi(vgettext2("Lv$level", symbols).c_str()) << "]";
		strcpy(text, strstr.str().c_str());
		// 枚举到此为止,此个config一定有孩子,强制让出来前面+符号
		LPARAM lParam = technology_tv_.size();
		technology_tv_.push_back(std::make_pair(current->id(), advances_from));
		if (it->advances_to.empty()) {
			TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM, lParam, gdmgr._iico_dir, gdmgr._iico_dir, 1, text);
		} else {
			TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_CHILDREN, lParam, gdmgr._iico_dir, gdmgr._iico_dir, 1, text);
			advances_from.push_back(current->id());
			technology_tree_2_tv_internal(hctl, htvi, it->advances_to, advances_from);
			TreeView_Expand(hctl, htvi, TVE_EXPAND);
		}
	}
}

void tcore::technology_tree_2_tv(HWND hctl, HTREEITEM htvroot)
{
	char text[_MAX_PATH];
	HTREEITEM htvi;
	std::stringstream strstr;
	utils::string_map symbols;
	std::vector<std::string> advances_from;

	size_t index = 0;
	const std::vector<advance_tree::node*>& technology_tree = unit_types.technology_tree();
	for (std::vector<advance_tree::node*>::const_iterator it = technology_tree.begin(); it != technology_tree.end(); ++ it, index ++) {
		const advance_tree::node* n = *it;
		const ttechnology* current = dynamic_cast<const ttechnology*>(n->current);
		if (explorer_technology::explorer != explorer_technology::NONE && n->advances_to.empty()) {
			continue;
		}
		htvi = TreeView_AddLeaf(hctl, htvroot);
		strstr.str("");
		strstr << std::setw(2) << std::setfill('0') << index << ": " << utf8_2_ansi(current->name().c_str()) << "(" << current->id() << ")";
		symbols["level"] = lexical_cast_default<std::string>(current->max_experience());
		strstr << "[" << utf8_2_ansi(vgettext2("Lv$level", symbols).c_str()) << "]";
		strcpy(text, strstr.str().c_str());
		// 枚举到此为止,此个config一定有孩子,强制让出来前面+符号
		LPARAM lParam = technology_tv_.size();
		advances_from.clear();
		technology_tv_.push_back(std::make_pair(current->id(), advances_from));
		if (n->advances_to.empty()) {
			TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM, lParam, gdmgr._iico_dir, gdmgr._iico_dir, 1, text);
		} else {
			TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_CHILDREN, lParam, gdmgr._iico_dir, gdmgr._iico_dir, 1, text);
			advances_from.push_back(current->id());
			technology_tree_2_tv_internal(hctl, htvi, n->advances_to, advances_from);
			TreeView_Expand(hctl, htvi, TVE_EXPAND);
		}
	}
}

void tcore::generate_utype_tree()
{
	types_updating_.clear();
}

bool tcore::new_utype()
{
	std::stringstream new_id;
	new_id << ns::default_utype_id;
	for (std::vector<std::pair<std::string, std::vector<std::string> > >::const_iterator it = tv_tree_.begin(); it != tv_tree_.end(); ++ it) {
		if (it->first == new_id.str()) {
			new_id << "_";
			it = tv_tree_.begin();
			continue;
		}
	}	

	tv_tree_.push_back(std::make_pair(new_id.str(), std::vector<std::string>()));
	types_updating_[tv_tree_.size() - 1] = tunit_type(new_id.str());

	tunit_type& utype = types_updating_.find(tv_tree_.size() - 1)->second;
	utype.utype_from_cfg_.id_ = utype.id_; // avoid delete.
	utype.movement_type_ = unit_types.movement_types().begin()->first;
	for (std::vector<std::string>::const_iterator it = attack_type_vstr.begin(); it != attack_type_vstr.end(); ++ it) {
		utype.resistance_[*it] = 0;
	}
	// advances_to
	const std::vector<advance_tree::node*>& utype_tree = unit_types.utype_tree();
	for (std::vector<advance_tree::node*>::const_iterator it = utype_tree.begin(); it != utype_tree.end(); ++ it) {
		const unit_type* that = dynamic_cast<const unit_type*>((*it)->current);
		if (that->packer()) {
			continue;
		}
		utype.candidate_advances_to_.push_back(that);
	}

	return true;
}

void tcore::erase_utype(int index, HWND hdlgP)
{
	if (index < (int)unit_types.types().size()) {
		return;
	}
	types_updating_.erase(tv_tree_.size() - 1);
	tv_tree_.erase(tv_tree_.begin() + index);

	if (hdlgP) {
		// update_to_ui_utype_edit(hdlgP);
	}
}

bool tcore::types_dirty() const
{
	for (std::map<int, tunit_type>::const_iterator it = types_updating_.begin(); it != types_updating_.end(); ++ it) {
		if (it->second.dirty()) {
			return true;
		}
	}
	return false;
}

void tcore::generate_utypes(bool with_anim) const
{
	const unit_type_data::unit_type_map& types = unit_types.types();
	for (unit_type_data::unit_type_map::const_iterator it = types.begin(); it != types.end(); ++ it) {
		const unit_type& ut = it->second;
		ns::utype.from_config(&ut);
		ns::utype.generate(with_anim);
	}
	core_enable_save_btn(TRUE);
}

void tcore::refresh_utype(HWND hdlgP)
{
	char text[_MAX_PATH];
	std::stringstream strstr;

	if (tunit_type::type_map_.empty()) {
		tunit_type::type_map_[tunit_type::TYPE_TROOP] = utf8_2_ansi(_("Troop"));
		tunit_type::type_map_[tunit_type::TYPE_COMMONER] = utf8_2_ansi(_("Commoner"));
		tunit_type::type_map_[tunit_type::TYPE_CITY] = utf8_2_ansi(_("City"));
		tunit_type::type_map_[tunit_type::TYPE_ARTIFICAL] = utf8_2_ansi(_("Artifical"));
	}
	tunit_type::artifical_hero_.clear();
	if (gdmgr.heros_.size()) {
		for (int number = hero::number_artifical_min; number <= hero::number_artifical_max; number ++) {
			tunit_type::artifical_hero_.insert(&gdmgr.heros_[number]);
		}
	}
	tunit_type::commoner_hero_.clear();
	if (gdmgr.heros_.size()) {
		tunit_type::commoner_hero_.insert(&gdmgr.heros_[hero::number_businessman]);
		tunit_type::commoner_hero_.insert(&gdmgr.heros_[hero::number_scholar]);
		tunit_type::commoner_hero_.insert(&gdmgr.heros_[hero::number_transport]);
	}
	if (types_updating_.empty()) {
		generate_utype_tree();
	}

	HWND hctl = GetDlgItem(hdlgP, IDC_TV_UTYPE_EXPLORER);

	// 1. 删除Treeview中所有项
	TreeView_DeleteAllItems(hctl);
	tv_tree_.clear();

	// 2. 向TreeView添加一级内容
	htvroot_utype_ = TreeView_AddLeaf(hctl, TVI_ROOT);
	strstr.str("");
	strstr << utf8_2_ansi(_("arms^Type")) << "(" << unit_types.types().size() << ")";
	strcpy(text, strstr.str().c_str());
	// 这里一定要设TVIF_CHILDREN, 否则接下折叠后将判断出其cChildren为0, 再不能展开
	TreeView_SetItem1(hctl, htvroot_utype_, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN, 0, 0, 0, 
		unit_types.types().size()? 1: 0, text);
	utype_tree_2_tv(hctl, htvroot_utype_);

	TreeView_Expand(hctl, htvroot_utype_, TVE_EXPAND);
}

void tcore::refresh_feature(HWND hdlgP)
{
	update_to_ui_feature(hdlgP);
}

void tcore::refresh_tactic(HWND hdlgP)
{
	char text[_MAX_PATH];
	std::stringstream strstr;

	HWND hctl_atom = GetDlgItem(hdlgP, IDC_TV_TACTIC_ATOM);
	HWND hctl_complex = GetDlgItem(hdlgP, IDC_TV_TACTIC_COMPLEX);

	// 1. clear treeview
	TreeView_DeleteAllItems(hctl_atom);
	TreeView_DeleteAllItems(hctl_complex);

	// 2. fill content
	htvroot_tactic_atom_ = TreeView_AddLeaf(hctl_atom, TVI_ROOT);
	strstr.str("");
	strstr << utf8_2_ansi(_("Atomic tactic"));
	strcpy(text, strstr.str().c_str());
	// 这里一定要设TVIF_CHILDREN, 否则接下折叠后将判断出其cChildren为0, 再不能展开
	TreeView_SetItem1(hctl_atom, htvroot_tactic_atom_, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN, 0, 0, 0, 
		1, text);

	htvroot_tactic_complex_ = TreeView_AddLeaf(hctl_complex, TVI_ROOT);
	strstr.str("");
	strstr << utf8_2_ansi(_("Complex tactic"));
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hctl_complex, htvroot_tactic_complex_, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN, 0, 0, 0, 
		1, text);

	HTREEITEM htvi_tactic, htvi;

	const std::map<int, ttactic>& tactics = unit_types.tactics();
	for (std::map<int, ttactic>::const_iterator it = tactics.begin(); it != tactics.end(); ++ it) {
		const ttactic& t = it->second;
		HWND hctl;
		HTREEITEM htvroot;
		int index;
		if (t.index() < ttactic::min_complex_index) {
			hctl = hctl_atom;
			htvroot = htvroot_tactic_atom_;
			index = t.index();
		} else {
			hctl = hctl_complex;
			htvroot = htvroot_tactic_complex_;
			index = t.index() - ttactic::min_complex_index;
		}
		htvi_tactic = TreeView_AddLeaf(hctl, htvroot);
		LPARAM lParam = t.index();
		strstr.str("");
		strstr << std::setw(2) << std::setfill('0') << index << ": " << utf8_2_ansi(t.name().c_str()) << "(" << t.id() << ")";
		strcpy(text, strstr.str().c_str());
		TreeView_SetItem2(hctl, htvi_tactic, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_CHILDREN, lParam, gdmgr._iico_dir, gdmgr._iico_dir, 1, text);

		htvroot = htvi_tactic;
		// description
		htvi = TreeView_AddLeaf(hctl, htvroot);
		strstr.str("");
		strstr << utf8_2_ansi(_("Description")) << ": ";
		strstr << utf8_2_ansi(t.description().c_str());
		strcpy(text, strstr.str().c_str());
		TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM, lParam, ns::iico_tactic_txt, ns::iico_tactic_txt, 0, text);

		htvi = TreeView_AddLeaf(hctl, htvroot);
		strstr.str("");
		strstr << utf8_2_ansi(_("Point")) << ": " << t.point();
		strstr << "    " << utf8_2_ansi(_("Level")) << ": " << t.level();
		strcpy(text, strstr.str().c_str());
		TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 0, ns::iico_tactic_txt, ns::iico_tactic_txt, 0, text);

		if (t.index() < ttactic::min_complex_index) {
			// range
			bool first = true;
			htvi = TreeView_AddLeaf(hctl, htvroot);
			strstr.str("");
			strstr << utf8_2_ansi(_("Range")) << ": ";
			if (t.range() & ttactic::SELF) {
				strstr << utf8_2_ansi(_("Myself"));
				first = false;
			}
			if (t.range() & ttactic::FRIEND) {
				if (!first) {
					strstr << ", ";
				}
				strstr << utf8_2_ansi(_("troop^Friend"));
				first = false;
			}
			if (t.range() & ttactic::ENEMY) {
				if (!first) {
					strstr << ", ";
				}
				strstr << utf8_2_ansi(_("troop^Enemy"));
				first = false;
			}
			strcpy(text, strstr.str().c_str());
			TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 0, ns::iico_tactic_range, ns::iico_tactic_range, 0, text);

				int apply_to = t.apply_to();
				htvi = TreeView_AddLeaf(hctl, htvroot);
				strstr.str("");
				if (apply_to == apply_to_tag::RESISTANCE) {
					strstr << utf8_2_ansi(_("Defend"));
				} else if (apply_to == apply_to_tag::ATTACK) {
					strstr << utf8_2_ansi(_("Attack"));
				} else if (apply_to == apply_to_tag::ENCOURAGE) {
					strstr << utf8_2_ansi(_("Will"));
				} else if (apply_to == apply_to_tag::DEMOLISH) {
					strstr << utf8_2_ansi(_("Demolish"));
				} else if (apply_to == apply_to_tag::ACTION) {
					strstr << utf8_2_ansi(_("Action"));
				}
				strcpy(text, strstr.str().c_str());
				TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_CHILDREN, 0, ns::iico_tactic_action, ns::iico_tactic_action, 1, text);
		} else {
			strstr.str("");
			strstr << utf8_2_ansi(_("tactic^Child")) << ": ";
			const std::vector<const ttactic*>& parts = t.parts();
			for (std::vector<const ttactic*>::const_iterator it2 = parts.begin(); it2 != parts.end(); ++ it2) {
				const ttactic& part = **it2;
				if (it2 != parts.begin()) {
					strstr << ", ";
				}
				strstr << std::setw(2) << std::setfill('0') << part.index() << ": " << utf8_2_ansi(part.name().c_str()) << "(" << part.id() << ")";
			}
			strcpy(text, strstr.str().c_str());
			htvi = TreeView_AddLeaf(hctl, htvroot);
			TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 0, ns::iico_tactic_txt, ns::iico_tactic_txt, 0, text);
		}

		TreeView_Expand(hctl, htvi_tactic, TVE_EXPAND);
	}

	TreeView_Expand(hctl_atom, htvroot_tactic_atom_, TVE_EXPAND);
	TreeView_Expand(hctl_complex, htvroot_tactic_complex_, TVE_EXPAND);
}

void tcore::refresh_character(HWND hdlgP)
{
	char text[_MAX_PATH];
	std::stringstream strstr;

	HWND hctl_atom = GetDlgItem(hdlgP, IDC_TV_CHARACTER_ATOM);
	HWND hctl_complex = GetDlgItem(hdlgP, IDC_TV_CHARACTER_COMPLEX);

	// 1. clear treeview
	TreeView_DeleteAllItems(hctl_atom);
	TreeView_DeleteAllItems(hctl_complex);

	// 2. fill content
	htvroot_character_atom_ = TreeView_AddLeaf(hctl_atom, TVI_ROOT);
	strstr.str("");
	strstr << utf8_2_ansi(_("Atomic character"));
	strcpy(text, strstr.str().c_str());
	// 这里一定要设TVIF_CHILDREN, 否则接下折叠后将判断出其cChildren为0, 再不能展开
	TreeView_SetItem1(hctl_atom, htvroot_character_atom_, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN, 0, 0, 0, 
		1, text);

	htvroot_character_complex_ = TreeView_AddLeaf(hctl_complex, TVI_ROOT);
	strstr.str("");
	strstr << utf8_2_ansi(_("Complex character"));
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hctl_complex, htvroot_character_complex_, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN, 0, 0, 0, 
		1, text);

	HTREEITEM htvi_character, htvi;

	const std::map<int, tcharacter>& characters = unit_types.characters();
	for (std::map<int, tcharacter>::const_iterator it = characters.begin(); it != characters.end(); ++ it) {
		const tcharacter& t = it->second;
		HWND hctl;
		HTREEITEM htvroot;
		int index;
		if (t.index() < tcharacter::min_complex_index) {
			hctl = hctl_atom;
			htvroot = htvroot_character_atom_;
			index = t.index();
		} else {
			hctl = hctl_complex;
			htvroot = htvroot_character_complex_;
			index = t.index() - tcharacter::min_complex_index;
		}
		htvi_character = TreeView_AddLeaf(hctl, htvroot);
		LPARAM lParam = t.index();
		strstr.str("");
		strstr << std::setw(2) << std::setfill('0') << index << ": " << utf8_2_ansi(t.name().c_str()) << "(" << t.id() << ")";
		strstr << utf8_2_ansi(_("Description")) << ": ";
		strstr << utf8_2_ansi(t.description().c_str());
		strcpy(text, strstr.str().c_str());
		TreeView_SetItem2(hctl, htvi_character, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_CHILDREN, lParam, gdmgr._iico_dir, gdmgr._iico_dir, 1, text);

		htvroot = htvi_character;

		if (t.index() < tcharacter::min_complex_index) {
			htvi = TreeView_AddLeaf(hctl, htvroot);
			strcpy(text, utf8_2_ansi(t.expression().c_str()));
			TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_CHILDREN, 0, ns::iico_tactic_action, ns::iico_tactic_action, 0, text);

		} else {
			strstr.str("");
			strstr << utf8_2_ansi(_("character^Child")) << ": ";
			const std::vector<const tcharacter*>& parts = t.parts();
			for (std::vector<const tcharacter*>::const_iterator it2 = parts.begin(); it2 != parts.end(); ++ it2) {
				const tcharacter& part = **it2;
				if (it2 != parts.begin()) {
					strstr << ", ";
				}
				strstr << std::setw(2) << std::setfill('0') << part.index() << ": " << utf8_2_ansi(part.name().c_str()) << "(" << part.id() << ")";
			}
			strcpy(text, strstr.str().c_str());
			htvi = TreeView_AddLeaf(hctl, htvroot);
			TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 0, ns::iico_tactic_txt, ns::iico_tactic_txt, 0, text);
		}

		TreeView_Expand(hctl, htvi_character, TVE_EXPAND);
	}

	TreeView_Expand(hctl_atom, htvroot_character_atom_, TVE_EXPAND);
	TreeView_Expand(hctl_complex, htvroot_character_complex_, TVE_EXPAND);
}

void tcore::refresh_decree(HWND hdlgP)
{
	char text[_MAX_PATH];
	std::stringstream strstr;

	HWND hctl_atom = GetDlgItem(hdlgP, IDC_TV_DECREE_ATOM);
	HWND hctl_complex = GetDlgItem(hdlgP, IDC_TV_DECREE_COMPLEX);

	// 1. clear treeview
	TreeView_DeleteAllItems(hctl_atom);
	TreeView_DeleteAllItems(hctl_complex);

	// 2. fill content
	htvroot_decree_atom_ = TreeView_AddLeaf(hctl_atom, TVI_ROOT);
	strstr.str("");
	strstr << utf8_2_ansi(_("Atomic decree"));
	strcpy(text, strstr.str().c_str());
	// 这里一定要设TVIF_CHILDREN, 否则接下折叠后将判断出其cChildren为0, 再不能展开
	TreeView_SetItem1(hctl_atom, htvroot_decree_atom_, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN, 0, 0, 0, 
		1, text);

	htvroot_decree_complex_ = TreeView_AddLeaf(hctl_complex, TVI_ROOT);
	strstr.str("");
	strstr << utf8_2_ansi(_("Complex decree"));
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hctl_complex, htvroot_decree_complex_, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN, 0, 0, 0, 
		1, text);

	HTREEITEM htvi_decree, htvi;
	utils::string_map symbols;

	const std::map<int, tdecree>& decrees = unit_types.decrees();
	for (std::map<int, tdecree>::const_iterator it = decrees.begin(); it != decrees.end(); ++ it) {
		const tdecree& t = it->second;
		HWND hctl;
		HTREEITEM htvroot;
		int index;
		if (t.index() < tdecree::min_complex_index) {
			hctl = hctl_atom;
			htvroot = htvroot_decree_atom_;
			index = t.index();
		} else {
			hctl = hctl_complex;
			htvroot = htvroot_decree_complex_;
			index = t.index() - tdecree::min_complex_index;
		}
		htvi_decree = TreeView_AddLeaf(hctl, htvroot);
		LPARAM lParam = t.index();
		strstr.str("");
		strstr << std::setw(2) << std::setfill('0') << index << ": " << utf8_2_ansi(t.name().c_str()) << "(" << t.id() << ")";
		strstr << utf8_2_ansi(_("Description")) << ": ";
		strstr << utf8_2_ansi(t.description().c_str());
		strcpy(text, strstr.str().c_str());
		TreeView_SetItem2(hctl, htvi_decree, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_CHILDREN, lParam, gdmgr._iico_dir, gdmgr._iico_dir, 1, text);

		htvroot = htvi_decree;

		htvi = TreeView_AddLeaf(hctl, htvroot);
		strstr.str("");
		if (t.front()) {
			strcpy(text, "Front");
			strstr << dgettext("wesnoth-lib", text);
		}
		if (t.min_level()) {
			if (!strstr.str().empty()) {
				strstr << "  ";
			}
			symbols["level"] = lexical_cast_default<std::string>(t.min_level());
			strstr << vgettext2("At least Lv$level", symbols);
		}
		if (!t.require_artifical().empty()) {
			if (!strstr.str().empty()) {
				strstr << "  ";
			}
			const std::set<int>& require_artifical = t.require_artifical();
			std::stringstream artifical_str;
			for (std::set<int>::const_iterator it2 = require_artifical.begin(); it2 != require_artifical.end(); ++ it2) {
				if (it2 != require_artifical.begin()) {
					artifical_str << ", ";
				}
				artifical_str << gdmgr.heros_[*it2].name();
			}
			symbols.clear();
			symbols["artifical"] = artifical_str.str();
			strstr << vgettext2("Require $artifical", symbols);
		}
		if (strstr.str().empty()) {
			strstr << _("None");
		}
		strcpy(text, strstr.str().c_str());
		strstr.str("");
		strstr << _("Condition") << ": " << text;
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_CHILDREN, 0, ns::iico_tactic_action, ns::iico_tactic_action, 0, text);

		if (t.index() < tdecree::min_complex_index) {
			htvi = TreeView_AddLeaf(hctl, htvroot);
			strcpy(text, utf8_2_ansi(t.expression().c_str()));

			TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_CHILDREN, 0, ns::iico_tactic_action, ns::iico_tactic_action, 0, text);

		} else {
			strstr.str("");
			strstr << utf8_2_ansi(_("decree^Child")) << ": ";
			const std::vector<const tdecree*>& parts = t.parts();
			for (std::vector<const tdecree*>::const_iterator it2 = parts.begin(); it2 != parts.end(); ++ it2) {
				const tdecree& part = **it2;
				if (it2 != parts.begin()) {
					strstr << ", ";
				}
				strstr << std::setw(2) << std::setfill('0') << part.index() << ": " << utf8_2_ansi(part.name().c_str()) << "(" << part.id() << ")";
			}
			strcpy(text, strstr.str().c_str());
			htvi = TreeView_AddLeaf(hctl, htvroot);
			TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 0, ns::iico_tactic_txt, ns::iico_tactic_txt, 0, text);
		}

		TreeView_Expand(hctl, htvi_decree, TVE_EXPAND);
	}

	TreeView_Expand(hctl_atom, htvroot_decree_atom_, TVE_EXPAND);
	TreeView_Expand(hctl_complex, htvroot_decree_complex_, TVE_EXPAND);
}

void tcore::refresh_technology(HWND hdlgP)
{
	char text[_MAX_PATH];
	std::stringstream strstr;

	HWND hctl = GetDlgItem(hdlgP, IDC_TV_TECHNOLOGY_EXPLORER);

	// 1. 删除Treeview中所有项
	TreeView_DeleteAllItems(hctl);
	technology_tv_.clear();

	// 2. 向TreeView添加一级内容
	htvroot_technology_ = TreeView_AddLeaf(hctl, TVI_ROOT);
	strstr.str("");
	strstr << dgettext_2_ansi("wesnoth-lib", "Technology tree") << "(" << unit_types.technologies().size() << ")";
	strcpy(text, strstr.str().c_str());
	// 这里一定要设TVIF_CHILDREN, 否则接下折叠后将判断出其cChildren为0, 再不能展开
	TreeView_SetItem1(hctl, htvroot_technology_, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN, 0, 0, 0, 
		unit_types.technologies().size()? 1: 0, text);
	technology_tree_2_tv(hctl, htvroot_technology_);

	TreeView_Expand(hctl, htvroot_technology_, TVE_EXPAND);
}

void tcore::refresh_formation(HWND hdlgP)
{
	update_to_ui_formation(hdlgP);
}

void tcore::refresh_noble(HWND hdlgP)
{
	update_to_ui_noble(hdlgP);
}

void tcore::refresh_treasure(HWND hdlgP)
{
	update_to_ui_treasure(hdlgP);
}

void tcore::refresh_faction(HWND hdlgP)
{
	update_to_ui_faction(hdlgP, -1);
}

void tcore::refresh_multiplayer(HWND hdlgP)
{
	update_to_ui_multiplayer(hdlgP);
}

#endif

void tcore::refresh_terrain(HWND hdlgP)
{
	update_to_ui_terrain(hdlgP);
}

void tcore::refresh_builder(HWND hdlgP)
{
	update_to_ui_builder(hdlgP);
}

void tcore::refresh_anim(HWND hdlgP)
{
	update_to_ui_anim(hdlgP);
}

void tcore::refresh_book(HWND hdlgP)
{
	update_to_ui_book(hdlgP);
}

void tcore::refresh_config(HWND hdlgP)
{
	update_to_ui_config(hdlgP);
}

void fill_param_area_anim(tanim_type2& anim)
{
	anim.variables_.insert(std::make_pair("8800", "x"));
	anim.variables_.insert(std::make_pair("8801", "y"));
	anim.variables_.insert(std::make_pair("8802", "offset_x"));
	anim.variables_.insert(std::make_pair("8803", "offset_y"));
	anim.variables_.insert(std::make_pair("8810", "alpha"));
	anim.variables_.insert(std::make_pair("__image", "image"));
	anim.variables_.insert(std::make_pair("__text", "text"));
}

void tcore::init_cache()
{
	// unit type
	if (map_) {
		delete map_;
	}
	if (builder_) {
		terrain_builder::release_heap();
		delete builder_;
	}
	const config::const_child_itors& terrains = editor_config::data_cfg.child_range("terrain_type");
	gamemap::terrain_types.clear();
	BOOST_FOREACH (const config &t, terrains) {
		gamemap::terrain_types.add_child("terrain_type", t);
	}
	map_ = new gamemap(editor_config::data_cfg, "");
	// builder_ = new terrain_builder(true, map_, "off-map/alpha.png");

	terrains_.clear();
	alias_terrains_.clear();
	const t_translation::t_list& terrainList = map_->get_terrain_list();
	for (t_translation::t_list::const_iterator it = terrainList.begin(); it != terrainList.end(); ++ it) {
		const t_translation::t_terrain& terrain = *it;
		const terrain_type& info = map_->get_terrain_info(terrain);
		if (info.union_type().size() == 1 && info.union_type()[0] == info.number() && info.is_nonnull()) {
			alias_terrains_.push_back(terrain);
		}
		terrains_.push_back(terrain);
	}
#ifndef _ROSE_EDITOR	
	// feature
	features_from_cfg_.clear();
	features_updating_.clear();

	const std::vector<int>& features = hero::valid_features();
	const std::vector<int>& features_level = unit_types.features_level();
	for (std::vector<int>::const_iterator it = features_level.begin(); it != features_level.end(); ++ it) {
		int feature = std::distance(features_level.begin(), it);
		int level = *it;
		if (std::find(features.begin(), features.end(), feature) == features.end()) {
			if (feature >= HEROS_BASE_FEATURE_COUNT) {
				break;
			} else {
				level = -1;
			}
		}
		features_from_cfg_.push_back(tfeature(level));
		features_from_cfg_.back().feature_from_cfg_ = features_from_cfg_.back();
	}
	const complex_feature_map& complex_feature = unit_types.complex_feature();
	for (std::map<int, std::vector<int> >::const_iterator it = complex_feature.begin(); it != complex_feature.end(); ++ it) {
		features_from_cfg_[it->first].from_config(it->first, it->second);
	}
	features_updating_ = features_from_cfg_;

	// noble
	nobles_from_cfg_.clear();
	nobles_updating_.clear();

	for (int i = 0; i < unit_types.noble_count(); i ++) {
		const tnoble& noble = unit_types.noble(i);
		tnoble2 nbl;
		nbl.from_config(noble);
		nobles_from_cfg_.push_back(nbl);
	}
	nobles_updating_ = nobles_from_cfg_;

	// treasure
	treasures_from_cfg_.clear();
	treasures_updating_.clear();

	const std::vector<ttreasure>& treasures = unit_types.treasures();
	for (std::vector<ttreasure>::const_iterator it = treasures.begin(); it != treasures.end(); ++ it) {
		const ttreasure& t = *it;
		treasures_from_cfg_.insert(std::make_pair(t.index(), t.feature()));
	}
	treasures_updating_ = treasures_from_cfg_;

	// faction
	factions_from_cfg_.clear();
	factions_updating_.clear();

	const config::const_child_itors& factions = editor_config::data_cfg.child_range("faction");
	std::set<int> used_heros, used_cities;
	BOOST_FOREACH (const config &cfg, factions) {
		tfaction f;
		f.from_config(cfg, used_heros, used_cities);
		factions_from_cfg_.push_back(f);
	}
	factions_updating_ = factions_from_cfg_;

	// multiplayer
	ns::_scenario.clear();
	ns::current_scenario = 0;
	const config::const_child_itors& multiplayers = editor_config::data_cfg.child_range("multiplayer");
	BOOST_FOREACH (const config &cfg, multiplayers) {
		if (cfg["map_generation"].str() == "default") {
			continue;
		}
		const std::string mode = cfg["mode"].str();
		if (mode == "tower") {
			continue;
		}
		if (mode == "siege") {
			continue;
		}
		ns::_scenario.push_back(tscenario("multiplayer"));
		tscenario& s = ns::_scenario.back();
		s.multiplayer_ = true;
		s.init_hero_state(gdmgr.heros_);
		// sub-object's from_config will use ns::current_scenario, set it correctly.
		s.from_config(ns::current_scenario, cfg);
		ns::current_scenario ++;
	}
	ns::current_scenario = 0;
#endif

	// terrain
	terrains_from_cfg_.clear();
	terrains_updating_.clear();

	for (t_translation::t_list::const_iterator it = terrains_.begin(); it != terrains_.end(); ++ it) {
		const terrain_type& info = map_->get_terrain_info(*it);

		tterrain t;
		t.from_config(info);
		terrains_from_cfg_.push_back(t);
	}
	terrains_updating_ = terrains_from_cfg_;

	// builder
	release_builder();
	config cfg;
	scoped_istream stream = istream_file(terrain_graphics_tpl(true));
	read(cfg, *stream);
	BOOST_FOREACH (const config &c, cfg.child_range("macro")) {
		int type = tbuilding_rule::macro_type(c["type"].str());
		tbuilding_rule* rule = NULL;
		if (type == tbuilding_rule::NONE) {
			rule = new tnone_brule();
		} else if (type == tbuilding_rule::TRANSITION_COMPLETE_LF) {
			rule = new ttransition_complete_lf_brule();
		} else if (type == tbuilding_rule::TRANSITION_COMPLETE_LB) {
			rule = new ttransition_complete_lb_brule();
		} else if (type == tbuilding_rule::TRANSITION_COMPLETE_L) {
			rule = new ttransition_complete_l_brule();
		} else if (type == tbuilding_rule::EDITOR_OVERLAY) {
			rule = new teditor_overlay_brule();
		} else if (type == tbuilding_rule::BRIDGE_STRAIGHTS) {
			rule = new tbridge_straights_brule();
		} else if (type == tbuilding_rule::BRIDGE_ENDS) {
			rule = new tbridge_ends_brule();
		} else if (type == tbuilding_rule::BRIDGE_JOINTS) {
			rule = new tbridge_joints_brule();
		} else if (type == tbuilding_rule::BRIDGE_CORNERS) {
			rule = new tbridge_corners_brule();
		} else if (type == tbuilding_rule::LAYOUT_TRACKS_F) {
			rule = new tlayout_tracks_f_brule();
		} else if (type == tbuilding_rule::TRACK_COMPLETE) {
			rule = new ttrack_complete_brule();
		} else if (type == tbuilding_rule::TRACK_BORDER_RESTRICTED_PLF) {
			rule = new ttrack_border_restricted_plf_brule();
		} else if (type == tbuilding_rule::NEW_FOREST) {
			rule = new tnew_forest_brule();
		} else if (type == tbuilding_rule::OVERLAY_RANDOM) {
			rule = new toverlay_random_brule();
		} else if (type == tbuilding_rule::OVERLAY_RANDOM_LF) {
			rule = new toverlay_random_lf_brule();
		} else if (type == tbuilding_rule::OVERLAY_RANDOM_L) {
			rule = new toverlay_random_l_brule();
		} else if (type == tbuilding_rule::OVERLAY_LF) {
			rule = new toverlay_lf_brule();
		} else if (type == tbuilding_rule::OVERLAY_F) {
			rule = new toverlay_f_brule();
		} else if (type == tbuilding_rule::OVERLAY_P) {
			rule = new toverlay_p_brule();
		} else if (type == tbuilding_rule::OVERLAY_B) {
			rule = new toverlay_b_brule();
		} else if (type == tbuilding_rule::OVERLAY) {
			rule = new toverlay_brule();
		} else if (type == tbuilding_rule::TERRAIN_BASE_RANDOM) {
			rule = new tterrain_base_random_brule();
		} else if (type == tbuilding_rule::TERRAIN_BASE_P) {
			rule = new tterrain_base_p_brule();
		} else if (type == tbuilding_rule::TERRAIN_BASE_F) {
			rule = new tterrain_base_f_brule();
		} else if (type == tbuilding_rule::TERRAIN_BASE) {
			rule = new tterrain_base_brule();
		} else if (type == tbuilding_rule::KEEP_BASE) {
			rule = new tkeep_base_brule();
		} else if (type == tbuilding_rule::KEEP_BASE_RANDOM) {
			rule = new tkeep_base_random_brule();
		} else if (type == tbuilding_rule::TERRAIN_BASE_SINGLEHEX_LB) {
			rule = new tterrain_base_singlehex_lb_brule();
		} else if (type == tbuilding_rule::TERRAIN_BASE_SINGLEHEX_B) {
			rule = new tterrain_base_singlehex_b_brule();
		} else if (type == tbuilding_rule::OVERLAY_COMPLETE_LF) {
			rule = new toverlay_complete_lf_brule();
		} else if (type == tbuilding_rule::OVERLAY_COMPLETE_L) {
			rule = new toverlay_complete_l_brule();
		} else if (type == tbuilding_rule::OVERLAY_COMPLETE_F) {
			rule = new toverlay_complete_f_brule();
		} else if (type == tbuilding_rule::OVERLAY_COMPLETE) {
			rule = new toverlay_complete_brule();
		} else if (type == tbuilding_rule::VOLCANO_2x2) {
			rule = new tvolcano_2x2_brule();
		} else if (type == tbuilding_rule::OVERLAY_RESTRICTED3_F) {
			rule = new toverlay_restricted3_f_brule();
		} else if (type == tbuilding_rule::OVERLAY_RESTRICTED2_F) {
			rule = new toverlay_restricted2_f_brule();
		} else if (type == tbuilding_rule::OVERLAY_RESTRICTED_P) {
			rule = new toverlay_restricted_p_brule();
		} else if (type == tbuilding_rule::OVERLAY_RESTRICTED_F) {
			rule = new toverlay_restricted_f_brule();
		} else if (type == tbuilding_rule::OVERLAY_ROTATION_RESTRICTED2_F) {
			rule = new toverlay_rotation_restricted2_f_brule();
		} else if (type == tbuilding_rule::OVERLAY_ROTATION_RESTRICTED_F) {
			rule = new toverlay_rotation_restricted_f_brule();
		} else if (type == tbuilding_rule::MOUNTAINS_2x4_NW_SE) {
			rule = new tmountains_2x4_nw_se_brule();
		} else if (type == tbuilding_rule::MOUNTAINS_2x4_SW_NE) {
			rule = new tmountains_2x4_sw_ne_brule();
		} else if (type == tbuilding_rule::MOUNTAINS_1x3_NW_SE) {
			rule = new tmountains_1x3_nw_se_brule();
		} else if (type == tbuilding_rule::MOUNTAINS_1x3_SW_NE) {
			rule = new tmountains_1x3_sw_ne_brule();
		} else if (type == tbuilding_rule::MOUNTAINS_2x2) {
			rule = new tmountains_2x2_brule();
		} else if (type == tbuilding_rule::MOUNTAIN_SINGLE_RANDOM) {
			rule = new tmountain_single_random_brule();
		} else if (type == tbuilding_rule::PEAKS_1x2_SW_NE) {
			rule = new tpeaks_1x2_sw_ne_brule();
		} else if (type == tbuilding_rule::PEAKS_LARGE) {
			rule = new tpeaks_large_brule();
		} else if (type == tbuilding_rule::VILLAGE) {
			rule = new tvillage_brule();
		} else if (type == tbuilding_rule::VILLAGE_A3) {
			rule = new tvillage_a3_brule();
		} else if (type == tbuilding_rule::VILLAGE_A4) {
			rule = new tvillage_a4_brule();
		} else if (type == tbuilding_rule::NEW_FENCE) {
			rule = new tnew_fence_brule();
		} else if (type == tbuilding_rule::NEW_WALL_P) {
			rule = new tnew_wall_p_brule();
		} else if (type == tbuilding_rule::NEW_WALL) {
			rule = new tnew_wall_brule();
		} else if (type == tbuilding_rule::NEW_WALL2_P) {
			rule = new tnew_wall2_p_brule();
		} else if (type == tbuilding_rule::NEW_WALL2) {
			rule = new tnew_wall2_brule();
		} else if (type == tbuilding_rule::NEW_WAVES) {
			rule = new tnew_waves_brule();
		} else if (type == tbuilding_rule::NEW_BEACH) {
			rule = new tnew_beach_brule();
		} else if (type == tbuilding_rule::WALL_TRANSITION3) {
			rule = new twall_transition3_brule();
		} else if (type == tbuilding_rule::WALL_TRANSITION2_LF) {
			rule = new twall_transition2_lf_brule();
		} else if (type == tbuilding_rule::WALL_TRANSITION_LF) {
			rule = new twall_transition_lf_brule();
		} else if (type == tbuilding_rule::WALL_ADJACENT_TRANSITION) {
			rule = new twall_adjacent_transition_brule();
		} else if (type == tbuilding_rule::ANIMATED_WATER_15) {
			rule = new tanimated_water_15_brule();
		} else if (type == tbuilding_rule::ANIMATED_WATER_15_TRANSITION) {
			rule = new tanimated_water_15_transition_brule();
		} else if (type == tbuilding_rule::DISABLE_BASE_TRANSITIONS) {
			rule = new tdisable_base_transitions_brule();
		}
		
								
		rule->from_config(c.child("cfg"), c["postfix"].str());
		brules_from_cfg_.push_back(rule);

	}
	copy_brules(brules_from_cfg_, brules_updating_);

	// anim
	anims_from_cfg_.clear();
	anims_updating_.clear();
	if (tanim::anim_types.empty()) {
		tanim::anim_types.push_back(tanim_type2("defend", _("anim^defend")));
		tanim::anim_types.back().variables_.insert(std::make_pair("$base_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$hit_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$miss_png", null_str));

		tanim::anim_types.push_back(tanim_type2("resistance", _("anim^resistance")));
		tanim::anim_types.back().variables_.insert(std::make_pair("$leading_png", null_str));

		tanim::anim_types.push_back(tanim_type2("leading", _("anim^leading")));
		tanim::anim_types.back().variables_.insert(std::make_pair("$leading_png", null_str));

		tanim::anim_types.push_back(tanim_type2("healing", _("anim^healing")));
		tanim::anim_types.back().variables_.insert(std::make_pair("$ranged_attack_1_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$ranged_attack_2_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$ranged_attack_3_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$ranged_attack_4_png", null_str));

		tanim::anim_types.push_back(tanim_type2("idle", _("anim^idle")));
		tanim::anim_types.back().variables_.insert(std::make_pair("$idle_1_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$idle_2_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$idle_3_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$idle_4_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$sound_ogg", null_str));

		tanim::anim_types.push_back(tanim_type2("multi_idle", _("anim^multi_idle")));
		tanim::anim_types.back().variables_.insert(std::make_pair("$idle_1_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$idle_2_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$idle_3_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$idle_4_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$sound_ogg", null_str));

		tanim::anim_types.push_back(tanim_type2("healed", _("anim^healed")));
		tanim::anim_types.back().variables_.insert(std::make_pair("$idle_1_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$idle_2_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$idle_3_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$idle_4_png", null_str));

		tanim::anim_types.push_back(tanim_type2("movement", _("anim^movement")));
		tanim::anim_types.back().variables_.insert(std::make_pair("$move_1_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$move_2_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$sound_ogg", null_str));

		tanim::anim_types.push_back(tanim_type2("build", _("anim^build")));
		tanim::anim_types.back().variables_.insert(std::make_pair("$build_1_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$build_2_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$build_3_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$build_4_png", null_str));

		tanim::anim_types.push_back(tanim_type2("repair", _("anim^repair")));
		tanim::anim_types.back().variables_.insert(std::make_pair("$repair_1_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$repair_2_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$repair_3_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$repair_4_png", null_str));

		tanim::anim_types.push_back(tanim_type2("die", _("anim^die")));
		tanim::anim_types.back().variables_.insert(std::make_pair("$die_1_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$die_2_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$sound_ogg", null_str));

		tanim::anim_types.push_back(tanim_type2("multi_die", _("anim^multi_die")));
		tanim::anim_types.back().variables_.insert(std::make_pair("$die_1_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$die_2_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$sound_ogg", null_str));

		tanim::anim_types.push_back(tanim_type2("melee_attack", _("anim^melee_attack")));
		tanim::anim_types.back().variables_.insert(std::make_pair("$attack_id", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$range", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$hit_sound", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$miss_sound", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$melee_attack_1_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$melee_attack_2_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$melee_attack_3_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$melee_attack_4_png", null_str));

		tanim::anim_types.push_back(tanim_type2("ranged_attack", _("anim^ranged_attack")));
		tanim::anim_types.back().variables_.insert(std::make_pair("$attack_id", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$range", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$image_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$image_diagonal_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$image_horizontal_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$hit_sound", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$miss_sound", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$ranged_attack_1_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$ranged_attack_2_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$ranged_attack_3_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$ranged_attack_4_png", null_str));

		tanim::anim_types.push_back(tanim_type2("magic_missile_attack", _("anim^magic_missile_attack")));
		tanim::anim_types.back().variables_.insert(std::make_pair("$attack_id", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$range", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$ranged_attack_1_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$ranged_attack_2_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$ranged_attack_3_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$ranged_attack_4_png", null_str));

		tanim::anim_types.push_back(tanim_type2("lightbeam_attack", _("anim^lightbeam_attack")));
		tanim::anim_types.back().variables_.insert(std::make_pair("$attack_id", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$range", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$ranged_attack_1_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$ranged_attack_2_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$ranged_attack_3_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$ranged_attack_4_png", null_str));

		tanim::anim_types.push_back(tanim_type2("fireball_attack", _("anim^fireball_attack")));
		tanim::anim_types.back().variables_.insert(std::make_pair("$attack_id", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$range", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$ranged_attack_1_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$ranged_attack_2_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$ranged_attack_3_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$ranged_attack_4_png", null_str));

		tanim::anim_types.push_back(tanim_type2("iceball_attack", _("anim^iceball_attack")));
		tanim::anim_types.back().variables_.insert(std::make_pair("$attack_id", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$range", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$ranged_attack_1_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$ranged_attack_2_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$ranged_attack_3_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$ranged_attack_4_png", null_str));

		tanim::anim_types.push_back(tanim_type2("lightning_attack", _("anim^lightning_attack")));
		tanim::anim_types.back().variables_.insert(std::make_pair("$attack_id", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$range", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$ranged_attack_1_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$ranged_attack_2_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$ranged_attack_3_png", null_str));
		tanim::anim_types.back().variables_.insert(std::make_pair("$ranged_attack_4_png", null_str));

		tanim::anim_types.push_back(tanim_type2("card", _("anim^card")));
		tanim::anim_types.push_back(tanim_type2("reinforce", _("anim^reinforce")));
		tanim::anim_types.push_back(tanim_type2("individuality", _("anim^individuality")));
		tanim::anim_types.push_back(tanim_type2("tactic", _("anim^tactic")));
		tanim::anim_types.push_back(tanim_type2("blade", _("anim^blade")));
		tanim::anim_types.push_back(tanim_type2("fire", _("anim^fire")));
		tanim::anim_types.push_back(tanim_type2("magic", _("anim^magic")));
		tanim::anim_types.push_back(tanim_type2("heal", _("anim^heal")));
		tanim::anim_types.push_back(tanim_type2("destruct", _("anim^destruct")));
		tanim::anim_types.push_back(tanim_type2("formation_attack", _("anim^formation attack")));
		tanim::anim_types.push_back(tanim_type2("formation_defend", _("anim^formation defend")));
		tanim::anim_types.push_back(tanim_type2("pass_scenario", _("anim^pass scenario")));
		tanim::anim_types.push_back(tanim_type2("perfect", _("anim^perfect")));
		tanim::anim_types.push_back(tanim_type2("income", _("anim^income")));
		tanim::anim_types.push_back(tanim_type2("stratagem_up", _("anim^stratagem_up")));
		tanim::anim_types.push_back(tanim_type2("stratagem_down", _("anim^stratagem_down")));
		tanim::anim_types.push_back(tanim_type2("location", _("anim^location")));
		tanim::anim_types.push_back(tanim_type2("hscroll_text", _("anim^hscroll text")));
		tanim::anim_types.push_back(tanim_type2("title_screen", _("anim^title_screen")));
		tanim::anim_types.push_back(tanim_type2("load_scenario", _("anim^load_scenario")));

		tanim::anim_types.push_back(tanim_type2("flags", _("anim^flags")));
		fill_param_area_anim(tanim::anim_types.back());

		tanim::anim_types.push_back(tanim_type2("text", _("anim^text")));
		fill_param_area_anim(tanim::anim_types.back());

		tanim::anim_types.push_back(tanim_type2("place", _("anim^place")));
		fill_param_area_anim(tanim::anim_types.back());
	}

	const config::const_child_itors& utype_anims = editor_config::data_cfg.child("units").child_range("utype_anim");
	BOOST_FOREACH (const config &cfg, utype_anims) {
		tanim anim;
		anim.from_config(cfg, false);
		anims_from_cfg_.push_back(anim);
	}
	const config::const_child_itors& area_anims = editor_config::data_cfg.child("units").child_range("area_anim");
	BOOST_FOREACH (const config &cfg, area_anims) {
		tanim anim;
		anim.from_config(cfg, true);
		anims_from_cfg_.push_back(anim);
	}

	anims_updating_ = anims_from_cfg_;

	// book
	books.clear();
	BOOST_FOREACH (const config &cfg, editor_config::data_cfg.child_range("book")) {
		tbook book;
		book.from_config(cfg);
		if (book.valid()) {
			books.insert(std::make_pair(book.id_, book));
		}
	}

	// game config
	config_from_cfg_.clear();
	config_updating_.clear();

	const config& game_config = editor_config::data_cfg.child("game_config");
	config_from_cfg_.from_config(game_config);

	config_updating_ = config_from_cfg_;
}

void tcore::switch_section(HWND hdlgP, int to, bool init)
{
	char text[_MAX_PATH];
	std::stringstream strstr;

	if (name_map.empty()) {
		name_map[CONFIG] = utf8_2_ansi(_("game^Config"));
		name_map[BOOK] = utf8_2_ansi(_("Book"));
		name_map[ANIM] = utf8_2_ansi(_("Animation"));
		name_map[TERRAIN] = dgettext_2_ansi("wesnoth", "Terrain");
		name_map[BUILDER] = utf8_2_ansi(_("Building rule"));
#ifndef _ROSE_EDITOR
		name_map[UNIT_TYPE] = utf8_2_ansi(_("arms^Type"));
		name_map[FEATURE] = dgettext_2_ansi("wesnoth-hero", "feature");
		strstr.str("");
		strstr << dsgettext("wesnoth-hero", "tactic") << "(" << _("Read only") << ")";
		name_map[TACTIC] = utf8_2_ansi(strstr.str().c_str());
		strstr.str("");
		strcpy(text, "Character");
		strstr << dgettext("wesnoth-lib", text) << "(" << _("Read only") << ")";
		name_map[CHARACTER] = utf8_2_ansi(strstr.str().c_str());
		strstr.str("");
		strcpy(text, "Decree");
		strstr << dgettext("wesnoth-lib", text) << "(" << _("Read only") << ")";
		name_map[DECREE] = utf8_2_ansi(strstr.str().c_str());
		strstr.str("");
		strcpy(text, "Technology");
		strstr << dgettext("wesnoth-lib", text) << "(" << _("Read only") << ")";
		name_map[TECH] = utf8_2_ansi(strstr.str().c_str());
		strstr.str("");
		strcpy(text, "tactical^Formation");
		strstr << dgettext("wesnoth-lib", text) << "(" << _("Read only") << ")";
		name_map[FORMATION] = utf8_2_ansi(strstr.str().c_str());
		name_map[NOBLE] = dgettext_2_ansi("wesnoth-hero", "noble");
		name_map[TREASURE] = dgettext_2_ansi("wesnoth-hero", "treasure");
		name_map[FACTION] = dgettext_2_ansi("wesnoth-lib", "Faction");
		name_map[MULTIPLAYER] = utf8_2_ansi(_("Multiplayer"));
#endif
	}
	if (idd_map.empty()) {
		idd_map[CONFIG] = IDD_CONFIG;
		idd_map[BOOK] = IDD_BOOK;
		idd_map[ANIM] = IDD_ANIM;
		idd_map[TERRAIN] = IDD_TERRAIN;
		idd_map[BUILDER] = IDD_BUILDER;
#ifndef _ROSE_EDITOR
		idd_map[UNIT_TYPE] = IDD_UTYPE;
		idd_map[FEATURE] = IDD_FEATURE;
		idd_map[TACTIC] = IDD_TACTIC;
		idd_map[CHARACTER] = IDD_CHARACTER;
		idd_map[DECREE] = IDD_DECREE;
		idd_map[TECH] = IDD_TECHNOLOGY;
		idd_map[FORMATION] = IDD_FORMATION;
		idd_map[NOBLE] = IDD_NOBLE;
		idd_map[TREASURE] = IDD_TREASURE;
		idd_map[FACTION] = IDD_FACTION;
		idd_map[MULTIPLAYER] = IDD_MULTIPLAYER;
#endif
	}
	if (dlgproc_map.empty()) {
		dlgproc_map[CONFIG] = DlgConfigProc;
		dlgproc_map[BOOK] = DlgBookProc;
		dlgproc_map[ANIM] = DlgAnimProc;
		dlgproc_map[TERRAIN] = DlgTerrainProc;
		dlgproc_map[BUILDER] = DlgBuilderProc;
#ifndef _ROSE_EDITOR
		dlgproc_map[UNIT_TYPE] = DlgUTypeProc;
		dlgproc_map[FEATURE] = DlgFeatureProc;
		dlgproc_map[TACTIC] = DlgTacticProc;
		dlgproc_map[CHARACTER] = DlgCharacterProc;
		dlgproc_map[DECREE] = DlgDecreeProc;
		dlgproc_map[TECH] = DlgTechnologyProc;
		dlgproc_map[FORMATION] = DlgFormationProc;
		dlgproc_map[NOBLE] = DlgNobleProc;
		dlgproc_map[TREASURE] = DlgTreasureProc;
		dlgproc_map[FACTION] = DlgFactionProc;
		dlgproc_map[MULTIPLAYER] = DlgMultiplayerProc;
#endif
	}
	
	DLGHDR* pHdr = (DLGHDR*)GetWindowLong(hdlgP, GWL_USERDATA);

	if (!init && pHdr && to == section_) {
		return;
	}
	
	if (!pHdr) {
		pHdr = (DLGHDR*)malloc(sizeof(DLGHDR));
		memset(pHdr, 0, sizeof(DLGHDR));
		// Save a pointer to the DLGHDR structure. 
		SetWindowLong(hdlgP, GWL_USERDATA, (LONG) pHdr); 

		pHdr->hwndTab = GetDlgItem(hdlgP, IDC_TAB_CORE_SECTION);

		pHdr->reserved_pages = SECTIONS;
		pHdr->apRes = (DLGTEMPLATE**)malloc(pHdr->reserved_pages * sizeof(DLGTEMPLATE*));
		pHdr->valid_pages = pHdr->reserved_pages;

		TCITEM tie;
		for (int s = 0; s < pHdr->reserved_pages; s ++) {
			// Add a tab for each of the three child dialog boxes. 
			tie.mask = TCIF_TEXT | TCIF_IMAGE; 
			tie.iImage = -1; 
			strcpy(text, name_map.find(s)->second.c_str());
			tie.pszText = text;
			TabCtrl_InsertItem(pHdr->hwndTab, s, &tie);

			// Lock the resources for the three child dialog boxes. 
			pHdr->apRes[s] = editor_config::DoLockDlgRes(MAKEINTRESOURCE(idd_map.find(s)->second));
		}

		RECT rc;
		// Calculate how large to make the tab control, so 
		// the display area can accommodate all the child dialog boxes.
		GetWindowRect(hdlgP, &rc);
		GetWindowRect(pHdr->hwndTab, &pHdr->rcDisplay);
		OffsetRect(&pHdr->rcDisplay, -1 * rc.left, -1 * rc.top);
		TabCtrl_GetItemRect(pHdr->hwndTab, 0, &rc);

		OffsetRect(&pHdr->rcDisplay, 0, rc.bottom);

	} else if (pHdr->hwndDisplay && section_ != to) {
		int retval = DestroyWindow(pHdr->hwndDisplay);
		DWORD err = GetLastError();
		pHdr->hwndDisplay = NULL;
	}

	if (!pHdr->hwndDisplay) {
		// Create the new child dialog box.
		pHdr->hwndDisplay = CreateDialogIndirect(gdmgr._hinst, pHdr->apRes[to], hdlgP, dlgproc_map.find(to)->second);
		ShowWindow(pHdr->hwndDisplay, SW_RESTORE);
	}

	section_ = to;

	if (init) {
		init_cache();
	}
#ifndef _ROSE_EDITOR
	core_enable_new_btn(section_ == MULTIPLAYER);
	core_enable_delete_btn(section_ == MULTIPLAYER);
#endif
	if (section_ == CONFIG) {
		refresh_config(pHdr->hwndDisplay);
	} else if (section_ == BOOK) {
		refresh_book(pHdr->hwndDisplay);
	} else if (section_ == ANIM) {
		refresh_anim(pHdr->hwndDisplay);
	} else if (section_ == TERRAIN) {
		refresh_terrain(pHdr->hwndDisplay);
	} else if (section_ == BUILDER) {
		refresh_builder(pHdr->hwndDisplay);
#ifndef _ROSE_EDITOR
	} else if (section_ == UNIT_TYPE) {
		refresh_utype(pHdr->hwndDisplay);
	} else if (section_ == FEATURE) {
		refresh_feature(pHdr->hwndDisplay);
	} else if (section_ == TACTIC) {
		refresh_tactic(pHdr->hwndDisplay);
	} else if (section_ == CHARACTER) {
		refresh_character(pHdr->hwndDisplay);
	} else if (section_ == DECREE) {
		refresh_decree(pHdr->hwndDisplay);
	} else if (section_ == TECH) {
		refresh_technology(pHdr->hwndDisplay);
	} else if (section_ == FORMATION) {
		refresh_formation(pHdr->hwndDisplay);
	} else if (section_ == NOBLE) {
		refresh_noble(pHdr->hwndDisplay);
	} else if (section_ == TREASURE) {
		refresh_treasure(pHdr->hwndDisplay);
	} else if (section_ == FACTION) {
		refresh_faction(pHdr->hwndDisplay);
	} else if (section_ == MULTIPLAYER) {
		refresh_multiplayer(pHdr->hwndDisplay);
#endif
	}
}

#ifndef _ROSE_EDITOR
void tcore::update_to_ui_treasure(HWND hdlgP)
{
	HWND hctl = GetDlgItem(hdlgP, IDC_LV_TREASURE_EXPLORER);
	std::stringstream strstr;
	std::string str;
	char text[_MAX_PATH];

	ListView_DeleteAllItems(hctl);
	// fill data
	LVITEM lvi;
	int iItem = 0;
	for (std::map<int, int>::const_iterator it = treasures_updating_.begin(); it != treasures_updating_.end(); ++ it, iItem ++) {
		int index = 0;

		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		// number
		lvi.iItem = iItem;
		lvi.iSubItem = index ++;
		strstr.str("");
		strstr << it->first;
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		lvi.lParam = (LPARAM)0;
		ListView_InsertItem(hctl, &lvi);

		// name
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = index ++;
		strstr.str("");
		strstr << unit_types.treasure(it->first).name();
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// feature
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = index ++;
		strstr.str("");
		strstr << hero::feature_str(it->second);
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// image
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = index ++;
		strstr.str("");
		strstr << game_config::path << "/data/core/images/";
		strstr << "treasure/" << it->first << ".png";
		str = strstr.str();
		std::replace(str.begin(), str.end(), '/', '\\');
		{
			strstr.str("");
			uint32_t fsize_low, fsize_high;
			posix_fsize_byname(str.c_str(), fsize_low, fsize_high);
			if (!fsize_low && !fsize_high) {
				strstr << "(" << _("Not exist") << ")";
			}
			strstr << str;
		}
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);
	}
}

void tcore::update_to_ui_treasure_edit(HWND hdlgP)
{
	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_TREASUREEDIT_FEATURE);
	int count = ComboBox_GetCount(hctl);
	int clicked_feature = treasures_updating_.find(ns::clicked_treasure)->second;
	for (int i = 0; i < count; i ++) {
		if (clicked_feature == ComboBox_GetItemData(hctl, i)) {
			ComboBox_SetCurSel(hctl, i);
			break;
		}
	}
}

void tcore::from_ui_treasure_edit(HWND hdlgP)
{
	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_TREASUREEDIT_FEATURE);
	std::map<int, int>::iterator find = treasures_updating_.find(ns::clicked_treasure);
	find->second = ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));
}

std::string tcore::units_internal(bool absolute) const
{
	std::stringstream strstr;
	if (absolute) {
		strstr << game_config::path << "\\data\\core\\units-internal\\units-internal.cfg";
	} else {
		strstr << "data/core/units-internal/units-internal.cfg";
	}
	return strstr.str();
}

void tcore::generate_units_internal()
{
	std::stringstream strstr;
	uint32_t bytertd;

	posix_file_t fp = INVALID_FILE;
	posix_fopen(units_internal(true).c_str(), GENERIC_WRITE, CREATE_ALWAYS, fp);
	if (fp == INVALID_FILE) {
		return;
	}

	// feature
	strstr << "[feature]\n";
	strstr << "\tlevel = ";
	for (std::vector<tfeature>::const_iterator it = features_updating_.begin(); it != features_updating_.end(); ++ it) {
		if (it != features_updating_.begin()) {
			strstr << ",";
		}
		strstr << (it->level_ != -1? it->level_: 0);
	}
	strstr << "\n";

	for (std::vector<tfeature>::iterator it = features_updating_.begin(); it != features_updating_.end(); ++ it) {
		int feature = std::distance(features_updating_.begin(), it);
		if (feature < HEROS_BASE_FEATURE_COUNT) {
			continue;
		}
		strstr << it->generate("\t", feature);
	}
	strstr << "[/feature]\n";

	features_from_cfg_.clear();
	features_updating_.clear();

	strstr << "\n";

	// treasure
	strstr << "[treasure]\n";
	strstr << "\tfeature = ";
	for (std::map<int, int>::const_iterator it = treasures_updating_.begin(); it != treasures_updating_.end(); ++ it) {
		if (it != treasures_updating_.begin()) {
			strstr << ", ";
		}
		strstr << it->second;
	}
	strstr << "\n";
	strstr << "[/treasure]\n";

	treasures_from_cfg_.clear();
	treasures_updating_.clear();

	posix_fwrite(fp, strstr.str().c_str(), strstr.str().length(), bytertd);
	posix_fclose(fp);
}

std::string tcore::noble_cfg(bool absolute) const
{
	std::stringstream strstr;
	if (absolute) {
		strstr << game_config::path << "\\data\\core\\units-internal\\noble.cfg";
	} else {
		strstr << "data/core/units-internal/noble.cfg";
	}
	return strstr.str();
}

void tcore::generate_noble_cfg() const
{
	std::stringstream strstr;
	uint32_t bytertd;

	posix_file_t fp = INVALID_FILE;
	posix_fopen(noble_cfg(true).c_str(), GENERIC_WRITE, CREATE_ALWAYS, fp);
	if (fp == INVALID_FILE) {
		return;
	}

	for (std::vector<tnoble2>::const_iterator it = nobles_updating_.begin(); it != nobles_updating_.end(); ++ it) {
		if (it != nobles_updating_.begin()) {
			strstr << "\n";
		}
		const tnoble2& n = *it;
		strstr << n.generate("");
	}

	posix_fwrite(fp, strstr.str().c_str(), strstr.str().length(), bytertd);
	posix_fclose(fp);
}

std::string tcore::factions_cfg(bool absolute) const
{
	std::stringstream strstr;
	if (absolute) {
		strstr << game_config::path << "\\data\\multiplayer\\factions.cfg";
	} else {
		strstr << "data/multiplayer/factions.cfg";
	}
	return strstr.str();
}

void tcore::generate_factions_cfg() const
{
	std::stringstream strstr;
	uint32_t bytertd;

	posix_file_t fp = INVALID_FILE;
	posix_fopen(factions_cfg(true).c_str(), GENERIC_WRITE, CREATE_ALWAYS, fp);
	if (fp == INVALID_FILE) {
		return;
	}

	strstr << "#textdomain wesnoth-multiplayer\n";
	strstr << "\n";

	for (std::vector<tfaction>::const_iterator it = factions_updating_.begin(); it != factions_updating_.end(); ++ it) {
		if (it != factions_updating_.begin()) {
			strstr << "\n";
		}
		const tfaction& f = *it;
		strstr << f.generate();
	}

	posix_fwrite(fp, strstr.str().c_str(), strstr.str().length(), bytertd);
	posix_fclose(fp);
}

#endif

std::string tcore::terrain_cfg(bool absolute) const
{
	std::stringstream strstr;
	if (absolute) {
		strstr << game_config::path << "\\data\\core\\terrain.cfg";
	} else {
		strstr << "data/core/terrain.cfg";
	}
	return strstr.str();
}

void tcore::generate_terrain_cfg() const
{
	std::stringstream strstr;
	uint32_t bytertd;

	posix_file_t fp = INVALID_FILE;
	posix_fopen(terrain_cfg(true).c_str(), GENERIC_WRITE, CREATE_ALWAYS, fp);
	if (fp == INVALID_FILE) {
		return;
	}

	strstr << "#textdomain wesnoth-lib\n";
	strstr << "# Terrain configuration file. Defines how the terrain _work_ in the game. How\n";
	strstr << "# the terrains _look_ is defined in terrain_graphics.cfg .\n";
	strstr << "\n";
	strstr << "# NOTE: terrain id's are used implicitly by the in-game help:\n";
	strstr << "# each \"[terrain_type] id=some_id\" corresponds to \"[section] id=terrain_some_id\"\n";
	strstr << "# or \"[topic] id=terrain_some_id\" identifying its description in [help]\n";
	strstr << "\n";
	strstr << "# NOTE: this list is sorted to group things comprehensibly in the editor\n";
	strstr << "\n";

	for (std::vector<tterrain>::const_iterator it = terrains_updating_.begin(); it != terrains_updating_.end(); ++ it) {
		const tterrain& t = *it;
		t.generate(strstr);
	}

	posix_fwrite(fp, strstr.str().c_str(), strstr.str().length(), bytertd);
	posix_fclose(fp);
}

std::string tcore::terrain_graphics_tpl(bool absolute) const
{
	std::stringstream strstr;
	if (absolute) {
		strstr << game_config::path << "\\data\\core\\terrain-graphics.tpl";
	} else {
		strstr << "data/core/terrain-graphics.tpl";
	}
	return strstr.str();
}

std::string tcore::terrain_graphics_cfg(bool absolute) const
{
	std::stringstream strstr;
	if (absolute) {
		strstr << game_config::path << "\\data\\core\\terrain-graphics.cfg";
	} else {
		strstr << "data/core/terrain-graphics.cfg";
	}
	return strstr.str();
}

void tcore::generate_terrain_graphics_cfg() const
{
	std::stringstream strstr;
	uint32_t bytertd;

	posix_file_t fp = INVALID_FILE, tpl = INVALID_FILE;
	posix_fopen(terrain_graphics_cfg(true).c_str(), GENERIC_WRITE, CREATE_ALWAYS, fp);
	if (fp == INVALID_FILE) {
		return;
	}
	posix_fopen(terrain_graphics_tpl(true).c_str(), GENERIC_WRITE, CREATE_ALWAYS, tpl);
	if (tpl == INVALID_FILE) {
		posix_fclose(fp);
		return;
	}

	strstr << "#textdomain wesnoth\n";
	strstr << "#wmlindent: start ignoring\n";
	strstr << "# This file needs to be processed *after* all others in this directory\n";
	strstr << "#\n";
	strstr << "# The following flags are defined to have a meaning\n";
	strstr << "#\n";
	strstr << "# * base : the corresponding tile has already graphics for the terrain\n";
	strstr << "# base. No other one should be added.\n";
	strstr << "# * transition-$direction : the corresponding tile already has the transition\n";
	strstr << "# in the given direction (or should not have one). No other one should be\n";
	strstr << "# added.\n";
	strstr << "#\n";
	strstr << "# when adding new probabilities update the commented line\n";
	strstr << "# the proper way to calculate the propabilities is described here\n";
	strstr << "# http://www.wesnoth.org/wiki/TerrainGraphicsTutorial#Cumulative_Probabilities\n";
	strstr << "\n";
	strstr << "# NOTE the terrain _off^_usr gets its definition from the code since it's\n";
	strstr << "# themable\n";

	for (std::vector<tbuilding_rule*>::const_iterator it = brules_updating_.begin(); it != brules_updating_.end(); ++ it) {
		const tbuilding_rule& r = **it;
		if (it != brules_updating_.begin()) {
			strstr << "\n";
		}
		strstr << r.generate();
	}

	posix_fwrite(fp, strstr.str().c_str(), strstr.str().length(), bytertd);
	posix_fclose(fp);

	// tpl
	strstr.str("");
	for (std::vector<tbuilding_rule*>::const_iterator it = brules_updating_.begin(); it != brules_updating_.end(); ++ it) {
		const tbuilding_rule& r = **it;
		if (it != brules_updating_.begin()) {
			strstr << "\n";
		}
		strstr << r.generate(true);
	}

	posix_fwrite(tpl, strstr.str().c_str(), strstr.str().length(), bytertd);
	posix_fclose(tpl);
}

std::pair<std::string, std::string> tcore::anims_cfg(bool absolute) const
{
	std::stringstream rose_ss, app_ss;
	if (absolute) {
		rose_ss << game_config::path << "\\data\\core\\units-internal\\animation.cfg";
		app_ss << game_config::path << "\\data\\core\\units-internal\\app_animation.cfg";
	} else {
		rose_ss << "data/core/units-internal/animation.cfg";
		app_ss << "data/core/units-internal/app_animation.cfg";
	}
	return std::make_pair(rose_ss.str(), app_ss.str());
}

void tcore::generate_anims_cfg() const
{
	std::stringstream rose_ss, app_ss;
	uint32_t bytertd;

	std::pair<std::string, std::string> file_name = anims_cfg(true);
	posix_file_t rose_fp = INVALID_FILE;
	posix_file_t app_fp = INVALID_FILE;
	posix_fopen(file_name.first.c_str(), GENERIC_WRITE, CREATE_ALWAYS, rose_fp);
	if (rose_fp == INVALID_FILE) {
		return;
	}
	posix_fopen(file_name.second.c_str(), GENERIC_WRITE, CREATE_ALWAYS, app_fp);
	if (app_fp == INVALID_FILE) {
		posix_fclose(rose_fp);
		return;
	}

	for (std::vector<tanim>::const_iterator it = anims_updating_.begin(); it != anims_updating_.end(); ++ it) {
		const tanim& anim = *it;
		int type = area_anim::find(anim.id_);
		std::stringstream& ss = anim.screen_mode_ && type != area_anim::NONE && type <= area_anim::MAX_ROSE_ANIM? rose_ss: app_ss;
		if (!ss.str().empty()) {
			ss << "\n";
		}
		ss << anim.generate();
	}

	posix_fwrite(rose_fp, rose_ss.str().c_str(), rose_ss.str().length(), bytertd);
	posix_fwrite(app_fp, app_ss.str().c_str(), app_ss.str().length(), bytertd);

	posix_fclose(rose_fp);
	posix_fclose(app_fp);
}

std::string tcore::config_cfg(bool absolute) const
{
	std::stringstream strstr;
	if (absolute) {
		strstr << game_config::path << "\\data\\game_config-internal.cfg";
	} else {
		strstr << "data/game_config-internal.cfg";
	}
	return strstr.str();
}

void tcore::generate_config_cfg() const
{
	std::stringstream strstr;
	uint32_t bytertd;

	posix_file_t fp = INVALID_FILE;
	posix_fopen(config_cfg(true).c_str(), GENERIC_WRITE, CREATE_ALWAYS, fp);
	if (fp == INVALID_FILE) {
		return;
	}

	strstr << config_updating_.generate();

	posix_fwrite(fp, strstr.str().c_str(), strstr.str().length(), bytertd);
	posix_fclose(fp);
}

std::string tcore::book_cfg(const std::string& id, bool absolute) const
{
	std::stringstream strstr;
	if (absolute) {
		strstr << game_config::path << "\\data\\core\\book\\" << id << ".cfg";
	} else {
		strstr << "data/core/book/" << id << ".cfg";
	}
	return strstr.str();
}

void tcore::generate_books_cfg()
{
	uint32_t bytertd;
	posix_file_t fp;

	for (std::map<std::string, tbook>::iterator it = books.begin(); it != books.end(); ++ it) {
		tbook& book = it->second;
		if (book.toplevel_.is_null()) {
			continue;
		}
		posix_fopen(book_cfg(book.id_, true).c_str(), GENERIC_WRITE, CREATE_ALWAYS, fp);
		if (fp == INVALID_FILE) {
			continue;
		}
		const std::string& str = book.generate();

		posix_fwrite(fp, str.c_str(), str.length(), bytertd);
		posix_fclose(fp);
	}
}

void core_enter_ui(void)
{
#ifndef _ROSE_EDITOR
	scenario_selector::switch_to(true);
#endif
	StatusBar_Idle();

	strcpy(gdmgr.cfg_fname_, gdmgr._menu_text);
	StatusBar_SetText(gdmgr._hwnd_status, 0, gdmgr.cfg_fname_);

	ns::core.switch_section(gdmgr._hdlg_core, ns::core.section_, true);
	return;
}

BOOL core_hide_ui(void)
{
	if (ns::core.save_if_dirty()) {
		return FALSE;
	}
	return TRUE;
}

#ifndef _ROSE_EDITOR
//
// tactic section
//
BOOL On_DlgTacticInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	HWND hwndParent = GetParent(hdlgP); 
    DLGHDR *pHdr = (DLGHDR *) GetWindowLong(hwndParent, GWL_USERDATA);
    SetWindowPos(hdlgP, HWND_TOP, pHdr->rcDisplay.left, pHdr->rcDisplay.top, 0, 0, SWP_NOSIZE); 

	ns::himl_tactic = ImageList_Create(15, 15, ILC_COLOR24, 3, 0);
	ImageList_SetBkColor(ns::himl_tactic, RGB(236, 233, 216));

    HICON hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_EVT_EVENT));
	ns::iico_tactic_tactic = ImageList_AddIcon(ns::himl_tactic, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_EVT_FILTER));
	ns::iico_tactic_range = ImageList_AddIcon(ns::himl_tactic, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_EVT_COMMAND));
	ns::iico_tactic_action = ImageList_AddIcon(ns::himl_tactic, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_EVT_ATTRIBUTE));
	ns::iico_tactic_txt = ImageList_AddIcon(ns::himl_tactic, hicon);

	TreeView_SetImageList(GetDlgItem(hdlgP, IDC_TV_TACTIC_ATOM), ns::himl_tactic, TVSIL_NORMAL);
	TreeView_SetImageList(GetDlgItem(hdlgP, IDC_TV_TACTIC_COMPLEX), ns::himl_tactic, TVSIL_NORMAL);

	return FALSE;
}

void On_DlgTacticCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	return;
}

void tactic_notify_handler_rclick(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	LPNMTREEVIEW			lpnmitem;
	HTREEITEM				htvi;
	TVITEMEX				tvi;
	POINT					point;

	if (lpNMHdr->idFrom != IDC_TV_TACTIC_ATOM && lpNMHdr->idFrom != IDC_TV_TACTIC_COMPLEX) {
		return;
	}

	lpnmitem = (LPNMTREEVIEW)lpNMHdr;
	GetCursorPos(&point);	// 得到的是屏幕坐标
	TreeView_HitTest1(lpNMHdr->hwndFrom, point, htvi);

	TreeView_GetItem1(lpNMHdr->hwndFrom, htvi, &tvi, TVIF_PARAM | TVIF_CHILDREN, NULL);
}

void tactic_notify_handler_dblclk(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	if (lpNMHdr->idFrom != IDC_TV_TACTIC_ATOM && lpNMHdr->idFrom != IDC_TV_TACTIC_COMPLEX) {
		return;
	}
}

BOOL On_DlgTacticNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
/*
	if (lpNMHdr->code == NM_RCLICK) {
		tactic_notify_handler_rclick(hdlgP, DlgItem, lpNMHdr);
	} else if (lpNMHdr->code == NM_DBLCLK) {
		tactic_notify_handler_dblclk(hdlgP, DlgItem, lpNMHdr);
	}
*/
	return FALSE;
}

void On_DlgTacticDestroy(HWND hdlgP)
{
	ImageList_Destroy(ns::himl_tactic);

	return;
}

BOOL CALLBACK DlgTacticProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_INITDIALOG:
		return On_DlgTacticInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgTacticCommand);
	HANDLE_MSG(hdlgP, WM_NOTIFY,  On_DlgTacticNotify);
	HANDLE_MSG(hdlgP, WM_DESTROY,  On_DlgTacticDestroy);
	}
	
	return FALSE;
}

//
// character section
//
BOOL On_DlgCharacterInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	HWND hwndParent = GetParent(hdlgP); 
    DLGHDR *pHdr = (DLGHDR *) GetWindowLong(hwndParent, GWL_USERDATA);
    SetWindowPos(hdlgP, HWND_TOP, pHdr->rcDisplay.left, pHdr->rcDisplay.top, 0, 0, SWP_NOSIZE); 

	return FALSE;
}

void On_DlgCharacterCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	return;
}

void On_DlgCharacterDestroy(HWND hdlgP)
{
	return;
}

BOOL CALLBACK DlgCharacterProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_INITDIALOG:
		return On_DlgCharacterInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgCharacterCommand);
	HANDLE_MSG(hdlgP, WM_DESTROY,  On_DlgCharacterDestroy);
	}
	
	return FALSE;
}

//
// character section
//
BOOL On_DlgDecreeInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	HWND hwndParent = GetParent(hdlgP); 
    DLGHDR *pHdr = (DLGHDR *) GetWindowLong(hwndParent, GWL_USERDATA);
    SetWindowPos(hdlgP, HWND_TOP, pHdr->rcDisplay.left, pHdr->rcDisplay.top, 0, 0, SWP_NOSIZE); 

	return FALSE;
}

void On_DlgDecreeCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	return;
}

void On_DlgDecreeDestroy(HWND hdlgP)
{
	return;
}

BOOL CALLBACK DlgDecreeProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_INITDIALOG:
		return On_DlgDecreeInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgDecreeCommand);
	HANDLE_MSG(hdlgP, WM_DESTROY,  On_DlgDecreeDestroy);
	}
	
	return FALSE;
}

void cb_treeview_walk_mark(HWND hctl, HTREEITEM htvi, uint32_t* ctx)
{
	TVITEMEX tvi;
	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tside& side = scenario.side_[ns::clicked_side];
	
	TreeView_GetItem1(hctl, htvi, &tvi, TVIF_PARAM, NULL);
	const std::string& id = ns::core.technology_tv_[tvi.lParam].first;
	if (explorer_technology::explorer == explorer_technology::SCENARIO) {
		if (side.technologies_.find(id) != side.technologies_.end()) {
			TreeView_SetItemState(hctl, htvi, INDEXTOSTATEIMAGEMASK(1), TVIS_STATEIMAGEMASK);
		}

	} else if (explorer_technology::explorer == explorer_technology::MODIFY_SIDE) {
		if (side.technologies_.find(id) != side.technologies_.end()) {
			TreeView_SetItemState(hctl, htvi, TVIS_BOLD, TVIS_BOLD);
		}
		const tevent::tmodify_side* modify_side = dynamic_cast<const tevent::tmodify_side*>(ns::clicked_command);
		if (modify_side->technology_.find(id) != modify_side->technology_.end()) {
			TreeView_SetItemState(hctl, htvi, INDEXTOSTATEIMAGEMASK(1), TVIS_STATEIMAGEMASK);
		}
	}
}

void explorer_technology_mark_tree(HWND hdlgP)
{
	HWND htv = GetDlgItem(hdlgP, IDC_TV_TECHNOLOGY_EXPLORER);
	
	TreeView_Walk(htv, TVI_ROOT, TRUE, cb_treeview_walk_mark, NULL, FALSE);
}

//
// technology section
//
BOOL On_DlgTechnologyInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	if (explorer_technology::explorer == explorer_technology::NONE) {
		HWND hwndParent = GetParent(hdlgP);
		DLGHDR *pHdr = (DLGHDR *) GetWindowLong(hwndParent, GWL_USERDATA);
		SetWindowPos(hdlgP, HWND_TOP, pHdr->rcDisplay.left, pHdr->rcDisplay.top, 0, 0, SWP_NOSIZE); 
	} else {
		editor_config::move_subcfg_right_position(hdlgP, lParam);
	
		std::stringstream strstr;
		strstr << utf8_2_ansi(_("Edit technology"));
		ShowWindow(GetDlgItem(hdlgP, IDCANCEL), SW_HIDE);
		SetWindowText(hdlgP, strstr.str().c_str());
	}

	ns::himl_technology = ImageList_Create(15, 15, ILC_COLOR24, 3, 0);
	ImageList_SetBkColor(ns::himl_technology, RGB(236, 233, 216));

    HICON hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_EVT_EVENT));
	ns::iico_technology_technology = ImageList_AddIcon(ns::himl_technology, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_EVT_FILTER));
	ns::iico_technology_experience = ImageList_AddIcon(ns::himl_technology, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_EVT_COMMAND));
	ns::iico_technology_action = ImageList_AddIcon(ns::himl_technology, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_EVT_ATTRIBUTE));
	ns::iico_technology_txt = ImageList_AddIcon(ns::himl_technology, hicon);

	TreeView_SetImageList(GetDlgItem(hdlgP, IDC_TV_TECHNOLOGY_ATOM), ns::himl_technology, TVSIL_NORMAL);
	TreeView_SetImageList(GetDlgItem(hdlgP, IDC_TV_TECHNOLOGY_COMPLEX), ns::himl_technology, TVSIL_NORMAL);

	if (explorer_technology::explorer != explorer_technology::NONE) {
		TreeView_SetImageList(GetDlgItem(hdlgP, IDC_TV_TECHNOLOGY_EXPLORER), gdmgr._himl_checkbox, LVSIL_STATE); 

		ns::core.refresh_technology(hdlgP);
		explorer_technology_mark_tree(hdlgP);
	}
	return FALSE;
}

BOOL On_DlgReportTechnologyInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	std::stringstream strstr;
	strstr << dgettext_2_ansi("wesnoth-lib", "Technology") << "--";
	if (ns::type == IDM_TECHNOLOGY_ATOMIC) {
		strstr << utf8_2_ansi(_("Atomic technology"));
	} else if (ns::type == IDM_TECHNOLOGY_COMPLEX) {
		strstr << utf8_2_ansi(_("Complex technology"));
	} else if (ns::type == IDM_TECHNOLOGY_STRATAGEM) {
		strstr << utf8_2_ansi(_("Stratagem"));
	}
	SetWindowText(hdlgP, strstr.str().c_str());
	Button_SetText(GetDlgItem(hdlgP, IDOK), utf8_2_ansi(_("Close")));

	HWND hctl = GetDlgItem(hdlgP, IDC_LV_VISUAL2_EXPLORER);
	LVCOLUMN lvc;
	int index = 0;
	char text[_MAX_PATH];

	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 80;
	strcpy(text, utf8_2_ansi(_("Name")));
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = index;
	ListView_InsertColumn(hctl, index ++, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 50;
	lvc.iSubItem = index;
	strcpy(text, utf8_2_ansi(_("Experience")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, index ++, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 80;
	lvc.iSubItem = index;
	strcpy(text, utf8_2_ansi(_("Relative")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, index ++, &lvc);

	if (ns::type == IDM_TECHNOLOGY_ATOMIC) {
		lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
		lvc.cx = 80;
		lvc.iSubItem = index;
		strcpy(text, utf8_2_ansi(_("Occasion")));
		lvc.pszText = text;
		ListView_InsertColumn(hctl, index ++, &lvc);

		lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
		lvc.cx = 80;
		lvc.iSubItem = index;
		strcpy(text, utf8_2_ansi(_("Type filter")));
		lvc.pszText = text;
		ListView_InsertColumn(hctl, index ++, &lvc);

		lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
		lvc.cx = 80;
		lvc.iSubItem = index;
		strcpy(text, utf8_2_ansi(_("Arms filter")));
		lvc.pszText = text;
		ListView_InsertColumn(hctl, index ++, &lvc);

		lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
		lvc.cx = 50;
		lvc.iSubItem = index;
		strcpy(text, utf8_2_ansi(_("Action")));
		lvc.pszText = text;
		ListView_InsertColumn(hctl, index ++, &lvc);

	} else if (ns::type == IDM_TECHNOLOGY_COMPLEX) {

		lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
		lvc.cx = 500;
		lvc.iSubItem = index;
		strcpy(text, utf8_2_ansi(_("technology^Child")));
		lvc.pszText = text;
		ListView_InsertColumn(hctl, index ++, &lvc);

	} else if (ns::type == IDM_TECHNOLOGY_STRATAGEM) {

		lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
		lvc.cx = 500;
		lvc.iSubItem = index;
		strcpy(text, utf8_2_ansi(_("Description")));
		lvc.pszText = text;
		ListView_InsertColumn(hctl, index ++, &lvc);

	}
	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	// fill data
	LVITEM lvi;
	int iItem = 0;
	bool first;
	int apply_to;
	for (std::map<std::string, ttechnology>::const_iterator it = unit_types.technologies().begin(); it != unit_types.technologies().end(); ++ it) {
		const ttechnology& t = it->second;

		if (ns::type == IDM_TECHNOLOGY_ATOMIC) {
			if (t.complex()) {
				continue;
			}
		} else if (ns::type == IDM_TECHNOLOGY_COMPLEX) {
			if (!t.complex()) {
				continue;
			}
		} else if (ns::type == IDM_TECHNOLOGY_STRATAGEM) {
			if (!t.stratagem()) {
				continue;
			}
		}

		index = 0;

		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		// 名称
		lvi.iItem = iItem;
		lvi.iSubItem = index ++;
		strstr.str("");
		strstr << utf8_2_ansi(t.name().c_str());
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		lvi.lParam = (LPARAM)0;
		ListView_InsertItem(hctl, &lvi);

		// Exp.
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = index ++;
		strstr.str("");
		strstr << t.max_experience();
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// Relative
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = index ++;
		strstr.str("");
		if (t.relative() != HEROS_NO_CHARACTER) {
			strstr << utf8_2_ansi(unit_types.character(t.relative()).name().c_str());
		}
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		if (ns::type == IDM_TECHNOLOGY_ATOMIC) {
			lvi.mask = LVIF_TEXT;
			lvi.iSubItem = index ++;
			strstr.str("");
			if (t.occasion() == ttechnology::MODIFY) {
				strstr << utf8_2_ansi(_("Adjust unit"));
			} else if (t.occasion() == ttechnology::FINISH) {
				strstr << utf8_2_ansi(_("Finish research"));
			}
			strcpy(text, strstr.str().c_str());
			lvi.pszText = text;
			ListView_SetItem(hctl, &lvi);

			lvi.mask = LVIF_TEXT;
			lvi.iSubItem = index ++;
			strstr.str("");
			first = true;
			if (t.type_filter() & family_tag::TROOP) {
				strstr << utf8_2_ansi(_("Troop"));
				first = false;
			}
			if (t.type_filter() & family_tag::ARTIFICAL) {
				if (!first) {
					strstr << ", ";
				} else {
					first = false;
				}
				strstr << utf8_2_ansi(_("Artifical"));
			}
			if (t.type_filter() & family_tag::CITY) {
				if (!first) {
					strstr << ", ";
				} else {
					first = false;
				}
				strstr << utf8_2_ansi(_("City"));
			}
			strcpy(text, strstr.str().c_str());
			lvi.pszText = text;
			ListView_SetItem(hctl, &lvi);

			lvi.mask = LVIF_TEXT;
			lvi.iSubItem = index ++;
			strstr.str("");
			first = true;
			const std::vector<std::string>& arms_ids = unit_types.arms_ids();
			for (int i = 0; i < (int)arms_ids.size(); i ++) {
				if (t.arms_filter() & 1 << i) {
					if (!first) {
						strstr << ", ";
					} else {
						first = false;
					}
					strstr << utf8_2_ansi(hero::arms_str(i).c_str());
				}
			}
			strcpy(text, strstr.str().c_str());
			lvi.pszText = text;
			ListView_SetItem(hctl, &lvi);

			lvi.mask = LVIF_TEXT;
			lvi.iSubItem = index ++;
			strstr.str("");
			apply_to = t.apply_to();
			if (apply_to == apply_to_tag::RESISTANCE) {
				strstr << utf8_2_ansi(_("Defend"));
			} else if (apply_to == apply_to_tag::ATTACK) {
				strstr << utf8_2_ansi(_("Attack"));
			} else if (apply_to == apply_to_tag::ENCOURAGE) {
				strstr << utf8_2_ansi(_("Will"));
			} else if (apply_to == apply_to_tag::DEMOLISH) {
				strstr << utf8_2_ansi(_("Demolish"));
			}
			strcpy(text, strstr.str().c_str());
			lvi.pszText = text;
			ListView_SetItem(hctl, &lvi);

		} else if (ns::type == IDM_TECHNOLOGY_COMPLEX) {
			lvi.mask = LVIF_TEXT;
			lvi.iSubItem = index ++;
			strstr.str("");
			const std::vector<const ttechnology*>& parts = t.parts();
			for (std::vector<const ttechnology*>::const_iterator it2 = parts.begin(); it2 != parts.end(); ++ it2) {
				const ttechnology& part = **it2;
				if (it2 != parts.begin()) {
					strstr << ", ";
				}
				strstr << utf8_2_ansi(part.name().c_str()) << "(" << part.id() << ")";
			}
			strcpy(text, strstr.str().c_str());
			lvi.pszText = text;
			ListView_SetItem(hctl, &lvi);

		} else if (ns::type == IDM_TECHNOLOGY_STRATAGEM) {
			lvi.mask = LVIF_TEXT;
			lvi.iSubItem = index ++;
			strstr.str("");
			strstr << utf8_2_ansi(t.description().c_str());
			strcpy(text, strstr.str().c_str());
			lvi.pszText = text;
			ListView_SetItem(hctl, &lvi);
		}
		iItem ++;
	}

	return FALSE;
}

void On_DlgReportTechnologyCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;
	switch (id) {
	case IDOK:
		changed = TRUE;
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

BOOL CALLBACK DlgReportTechnologyProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message) {
	case WM_INITDIALOG:
		return On_DlgReportTechnologyInitDialog(hDlg, (HWND)(wParam), lParam);
	HANDLE_MSG(hDlg, WM_COMMAND, On_DlgReportTechnologyCommand);
	}
	
	return FALSE;
}

extern void from_ui_technology(HWND hdlgP, std::set<std::string>& technologies);

void On_DlgTechnologyCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;

	switch (id) {
	case IDM_TECHNOLOGY_ATOMIC:
	case IDM_TECHNOLOGY_COMPLEX:
	case IDM_TECHNOLOGY_STRATAGEM:
		ns::type = id;
		DialogBox(gdmgr._hinst, MAKEINTRESOURCE(IDD_VISUAL2), NULL, DlgReportTechnologyProc);
		break;

	case IDOK:
		changed = TRUE;
		if (explorer_technology::explorer != explorer_technology::NONE) {
			tscenario& scenario = ns::_scenario[ns::current_scenario];
			tside& side = scenario.side_[ns::clicked_side];
			if (explorer_technology::explorer == explorer_technology::SCENARIO) {
				from_ui_technology(hdlgP, side.technologies_);
			} else {
				tevent::tmodify_side* modify_side = dynamic_cast<tevent::tmodify_side*>(ns::clicked_command);
				from_ui_technology(hdlgP, modify_side->technology_);
			}
		}
	case IDCANCEL:
		if (explorer_technology::explorer != explorer_technology::NONE) {
			EndDialog(hdlgP, changed? 1: 0);
		}
		break;
	}
	return;
}

void technology_notify_handler_rclick(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	LPNMTREEVIEW			lpnmitem;
	HTREEITEM				htvi;
	TVITEMEX				tvi;
	POINT					point;
	std::stringstream strstr;

	if (explorer_technology::explorer != explorer_technology::NONE) {
		return;
	}
	if (lpNMHdr->idFrom != IDC_TV_TECHNOLOGY_EXPLORER) {
		return;
	}

	lpnmitem = (LPNMTREEVIEW)lpNMHdr;

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
		strstr << utf8_2_ansi(_("Atomic technology")) << "...";
		AppendMenu(hpopup_explorer, MF_STRING, IDM_TECHNOLOGY_ATOMIC, strstr.str().c_str());
		strstr.str("");
		strstr << utf8_2_ansi(_("Complex technology")) << "...";
		AppendMenu(hpopup_explorer, MF_STRING, IDM_TECHNOLOGY_COMPLEX, strstr.str().c_str());
		strstr.str("");
		strstr << utf8_2_ansi(_("Stratagem")) << "...";
		AppendMenu(hpopup_explorer, MF_STRING, IDM_TECHNOLOGY_STRATAGEM, strstr.str().c_str());

		HMENU hpopup_technology = CreatePopupMenu();
		AppendMenu(hpopup_technology, MF_POPUP, (UINT_PTR)(hpopup_explorer), utf8_2_ansi(_("Report format")));

		if (core_get_save_btn()) {
			EnableMenuItem(hpopup_technology, (UINT_PTR)(hpopup_explorer), MF_BYCOMMAND | MF_GRAYED);
		}
		
		TrackPopupMenuEx(hpopup_technology, 0, 
			point.x, 
			point.y, 
			hdlgP, 
			NULL);

		// DestroyMenu(hpopup_explorer);
		DestroyMenu(hpopup_technology);

		// ns::clicked_utype = tvi.lParam;
	}
}

void technology_notify_handler_dblclk(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	LPNMTREEVIEW			lpnmitem;
	HTREEITEM				htvi;
	POINT					point;

	if (explorer_technology::explorer == explorer_technology::NONE) {
		return;
	}

	if (lpNMHdr->idFrom != IDC_TV_TECHNOLOGY_EXPLORER) {
		return;
	}
	lpnmitem = (LPNMTREEVIEW)lpNMHdr;

	GetCursorPos(&point);	// 得到的是屏幕坐标
	TreeView_HitTest1(lpNMHdr->hwndFrom, point, htvi);
	if (!htvi || htvi == ns::core.htvroot_technology_) {
		return;
	}

	if (explorer_technology::explorer != explorer_technology::SCENARIO) {
		if (TreeView_GetItemState(lpNMHdr->hwndFrom, htvi, 0) & TVIS_BOLD) {
			return;
		}
	}

	if (TreeView_GetItemState(lpNMHdr->hwndFrom, htvi, TVIS_STATEIMAGEMASK) & INDEXTOSTATEIMAGEMASK(1)) {
		TreeView_SetItemState(lpNMHdr->hwndFrom, htvi, 0, TVIS_STATEIMAGEMASK);
	} else {
		TreeView_SetItemState(lpNMHdr->hwndFrom, htvi, INDEXTOSTATEIMAGEMASK(1), TVIS_STATEIMAGEMASK);
	}
}

BOOL technology_notify_handler_itemexpanding(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	LPNMTREEVIEW			lpnmitem;
	HTREEITEM				htvi;
	POINT					point;
	std::stringstream strstr;

	if (explorer_technology::explorer == explorer_technology::NONE) {
		return FALSE;
	}

	if (lpNMHdr->idFrom != IDC_TV_TECHNOLOGY_EXPLORER) {
		return FALSE;
	}
	lpnmitem = (LPNMTREEVIEW)lpNMHdr;
	GetCursorPos(&point);	// 得到的是屏幕坐标
	TreeView_HitTest1(lpNMHdr->hwndFrom, point, htvi);

	if (!htvi || lpnmitem->action != TVE_COLLAPSE) {
		return FALSE;
	}

	// TRUE prevents the list from expanding or collapsing.
	// return value should set both SetWindowLong and function return.
	SetWindowLong(hdlgP, DWL_MSGRESULT, TRUE);
	return TRUE;
}

BOOL On_DlgTechnologyNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	if (lpNMHdr->code == NM_RCLICK) {
		technology_notify_handler_rclick(hdlgP, DlgItem, lpNMHdr);
	} else if (lpNMHdr->code == NM_DBLCLK) {
		technology_notify_handler_dblclk(hdlgP, DlgItem, lpNMHdr);
	} else if (lpNMHdr->code == TVN_ITEMEXPANDING) {
		return technology_notify_handler_itemexpanding(hdlgP, DlgItem, lpNMHdr);
	}
	return FALSE;
}

void On_DlgTechnologyDestroy(HWND hdlgP)
{
	ImageList_Destroy(ns::himl_technology);

	return;
}

BOOL CALLBACK DlgTechnologyProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_INITDIALOG:
		return On_DlgTechnologyInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgTechnologyCommand);
	HANDLE_MSG(hdlgP, WM_NOTIFY,  On_DlgTechnologyNotify);
	HANDLE_MSG(hdlgP, WM_DESTROY,  On_DlgTechnologyDestroy);
	}
	
	return FALSE;
}

//
// treasure
//
BOOL On_DlgTreasureInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	HWND hwndParent = GetParent(hdlgP); 
    DLGHDR *pHdr = (DLGHDR *) GetWindowLong(hwndParent, GWL_USERDATA);
    SetWindowPos(hdlgP, HWND_TOP, pHdr->rcDisplay.left, pHdr->rcDisplay.top, 0, 0, SWP_NOSIZE); 

	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_REMARK), utf8_2_ansi(_("Treasure remark")));

	std::stringstream strstr;

	HWND hctl = GetDlgItem(hdlgP, IDC_LV_TREASURE_EXPLORER);
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
	lvc.cx = 60;
	lvc.iSubItem = index;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "feature"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, index ++, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 400;
	lvc.iSubItem = index;
	strcpy(text, utf8_2_ansi(_("Image")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, index ++, &lvc);

	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	return FALSE;
}

void OnTreasureAddBt(HWND hdlgP)
{
	// default to first feature
	ns::core.treasures_updating_.insert(std::make_pair(ns::core.treasures_updating_.size(), 0));

	ns::core.update_to_ui_treasure(hdlgP);
	ns::core.set_dirty(tcore::BIT_TREASURE, ns::core.treasures_dirty()); 
}

void OnTreasureDelBt(HWND hdlgP)
{
	// default to first feature
	std::map<int, int>::iterator it = ns::core.treasures_updating_.begin();
	std::advance(it, ns::core.treasures_updating_.size() - 1);
	ns::core.treasures_updating_.erase(it);

	ns::core.update_to_ui_treasure(hdlgP);
	ns::core.set_dirty(tcore::BIT_TREASURE, ns::core.treasures_dirty()); 
}

void OnTreasureEditBt(HWND hdlgP)
{
	RECT rcBtn;
	LPARAM lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_LV_TREASURE_EXPLORER), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	ns::action_treasure = ma_edit;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_TREASUREEDIT), hdlgP, DlgTreasureEditProc, lParam)) {
		ns::core.update_to_ui_treasure(hdlgP);
		ns::core.set_dirty(tcore::BIT_TREASURE, ns::core.treasures_dirty()); 
	}

	return;
}

void On_DlgTreasureCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	switch (id) {
	case IDM_ADD:
		OnTreasureAddBt(hdlgP);
		break;
	case IDM_DELETE:
		OnTreasureDelBt(hdlgP);
		break;
	case IDM_EDIT:
		OnTreasureEditBt(hdlgP);
		break;
	}

	return;
}

BOOL On_DlgTreasureEditInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	std::stringstream strstr;
	strstr << _("Edit treasure");
	strstr << "(" << unit_types.treasure(ns::clicked_treasure).name() << ")"; 
	SetWindowText(hdlgP, utf8_2_ansi(strstr.str().c_str()));

	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_FEATURE), dgettext_2_ansi("wesnoth-hero", "feature"));

	// movement_type
	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_TREASUREEDIT_FEATURE);
	std::vector<int>& features = hero::valid_features();
	for (std::vector<int>::const_iterator it = features.begin(); it != features.end(); ++ it) {
		ComboBox_AddString(hctl, utf8_2_ansi(hero::feature_str(*it).c_str())); 
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, *it);
	}

	ns::core.update_to_ui_treasure_edit(hdlgP);
	return FALSE;
}

void On_DlgTreasureEditCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;

	switch (id) {
	case IDOK:
		changed = TRUE;
		ns::core.from_ui_treasure_edit(hdlgP);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

BOOL CALLBACK DlgTreasureEditProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message) {
	case WM_INITDIALOG:
		return On_DlgTreasureEditInitDialog(hDlg, (HWND)(wParam), lParam);
	HANDLE_MSG(hDlg, WM_COMMAND, On_DlgTreasureEditCommand);
	HANDLE_MSG(hDlg, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	}
	
	return FALSE;
}

void treasure_notify_handler_rclick(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	LVITEM					lvi;
	LPNMITEMACTIVATE		lpnmitem;
	int						icount;

	// NM_表示对通用控件都通用,范围丛(0, 99)
	// TVN_表示只能TreeView通用,范围丛(400, 499)
	if (DlgItem != IDC_LV_TREASURE_EXPLORER) {
		return;
	}
	lpnmitem = (LPNMITEMACTIVATE)lpNMHdr;
	// 如果单击到的是复选框位置,把复选框状态置反
	// 当前定义的图标大小是16x16. ptAction反回的(x,y)是整个列表视图内的坐标,因而y值不大好判断
	// 认为如果x是小于16的就认为是击中复选框
	
	// ListView_SetCheckState(lpNMHdr->hwndFrom, lpnmitem->iItem, !ListView_GetCheckState(lpNMHdr->hwndFrom, lpnmitem->iItem));
	POINT point = {lpnmitem->ptAction.x, lpnmitem->ptAction.y};
	MapWindowPoints(lpNMHdr->hwndFrom, NULL, &point, 1);

	lvi.iItem = lpnmitem->iItem;
	lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
	lvi.iSubItem = 0;
	lvi.pszText = gdmgr._menu_text;
	lvi.cchTextMax = _MAX_PATH;
	ListView_GetItem(lpNMHdr->hwndFrom, &lvi);

	icount = ListView_GetItemCount(lpNMHdr->hwndFrom);

	HMENU hpopup_explorer = CreatePopupMenu();
	AppendMenu(hpopup_explorer, MF_STRING, IDM_ADD, utf8_2_ansi(_("Add...")));
	AppendMenu(hpopup_explorer, MF_STRING, IDM_EDIT, utf8_2_ansi(_("Edit...")));
	AppendMenu(hpopup_explorer, MF_STRING, IDM_DELETE, utf8_2_ansi(_("Delete...")));

	if (lpnmitem->iItem < 0) {
		EnableMenuItem(hpopup_explorer, IDM_EDIT, MF_BYCOMMAND | MF_GRAYED);
		EnableMenuItem(hpopup_explorer, IDM_DELETE, MF_BYCOMMAND | MF_GRAYED);
	} else if (lpnmitem->iItem != ListView_GetItemCount(lpNMHdr->hwndFrom) - 1) {
		EnableMenuItem(hpopup_explorer, IDM_DELETE, MF_BYCOMMAND | MF_GRAYED);
	}

	TrackPopupMenuEx(hpopup_explorer, 0, 
		point.x, 
		point.y, 
		hdlgP, 
		NULL);

	DestroyMenu(hpopup_explorer);

	ns::clicked_treasure = lpnmitem->iItem;
}

void treasure_notify_handler_dblclk(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
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
		if (lpNMHdr->idFrom == IDC_LV_TREASURE_EXPLORER) {
			ns::clicked_treasure = lpnmitem->iItem;
			OnTreasureEditBt(hdlgP);
		}
	}
    return;
}

BOOL On_DlgTreasureNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	if (lpNMHdr->code == NM_RCLICK) {
		treasure_notify_handler_rclick(hdlgP, DlgItem, lpNMHdr);
	} else if (lpNMHdr->code == NM_DBLCLK) {
		treasure_notify_handler_dblclk(hdlgP, DlgItem, lpNMHdr);
	}
	return FALSE;
}

BOOL CALLBACK DlgTreasureProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_INITDIALOG:
		return On_DlgTreasureInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgTreasureCommand);
	HANDLE_MSG(hdlgP, WM_NOTIFY,  On_DlgTreasureNotify);
	}
	
	return FALSE;
}
#endif

BOOL On_DlgCoreInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	gdmgr._hdlg_core = hdlgP;
	ns::core.init_toolbar(gdmgr._hinst, hdlgP);
	core_enable_save_btn(FALSE);

	return FALSE;
}

void On_DlgCoreCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	switch (id) {
#ifndef _ROSE_EDITOR
	case IDM_NEW:
		ns::new_scenario();
		break;
	case IDM_DELETE:
		ns::delete_scenario(hdlgP);
		break;
#endif
	case IDM_SAVE:
		ns::core.save(hdlgP);
		break;
	}
	return;
}

BOOL On_DlgCoreNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	DLGHDR *pHdr = (DLGHDR *) GetWindowLong(hdlgP, GWL_USERDATA); 
    
	if (lpNMHdr->code == TCN_SELCHANGE) {
		if (lpNMHdr->idFrom == IDC_TAB_CORE_SECTION) {
			int iSel = TabCtrl_GetCurSel(pHdr->hwndTab);
			ns::core.switch_section(hdlgP, iSel, false);
		}
	}
	return FALSE;
}

void On_DlgCoreDestroy(HWND hdlgP)
{
	DLGHDR* pHdr = (DLGHDR*)GetWindowLong(hdlgP, GWL_USERDATA);
	if (pHdr) {
		delete pHdr->apRes;
		delete pHdr;
	}
	return;
}

BOOL CALLBACK DlgCoreProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_INITDIALOG:
		return On_DlgCoreInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgCoreCommand);
	HANDLE_MSG(hdlgP, WM_NOTIFY,  On_DlgCoreNotify);
	HANDLE_MSG(hdlgP, WM_DESTROY, On_DlgCoreDestroy);
	}
	
	return FALSE;
}

bool core_can_execute_tack(int task)
{
	if (task == TASK_NEW || task == TASK_DELETE || task == TASK_EXPLORER) {
		if (core_get_save_btn()) {
			return false;
		}
	}
	return true;
}