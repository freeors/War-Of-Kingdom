#define GETTEXT_DOMAIN "wesnoth-maker"

#include "global.hpp"
#include "game_config.hpp"
#include "foreach.hpp"
#include "loadscreen.hpp"
#include "DlgCoreProc.hpp"
#include "gettext.hpp"
#include "formula_string_utils.hpp"
#include <string.h>

#include "resource.h"

#include "xfunc.h"
#include "win32x.h"
#include "struct.h"

#include "map.hpp"

std::vector<std::string> attack_range_vstr;
std::vector<std::string> attack_type_vstr;

namespace ns {
	const std::string default_utype_textdomain = "wesnoth-tk-units";
	const std::string default_utype_id = "utype_id_";
	const std::string default_attack_id = "attack_id_";
	const std::string default_attack_icon = "sword-human.png";

	tcore core;
	tunit_type utype;

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
	: section_(UNIT_TYPE)
	, map_(NULL)
	, types_updating_()
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
	attack_range_vstr.clear();
	attack_type_vstr.clear();
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

	ToolBar_AddButtons(gdmgr._htb_core, 1, &gdmgr._tbBtns_core);

	ToolBar_AutoSize(gdmgr._htb_core);
	
	ToolBar_SetExtendedStyle(gdmgr._htb_core, TBSTYLE_EX_DRAWDDARROWS);
	
	ToolBar_SetImageList(gdmgr._htb_core, gdmgr._himl_24x24, 0);
	ToolBar_SetDisabledImageList(gdmgr._htb_core, gdmgr._himl_24x24_dis);
	
	ShowWindow(gdmgr._htb_core, SW_SHOW);

	return gdmgr._htb_core;
}

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
		tv_tree_.push_back(std::make_pair<std::string, std::vector<std::string> >(current->id(), advances_from));
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
		tv_tree_.push_back(std::make_pair<std::string, std::vector<std::string> >(current->id(), advances_from));
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
		const technology* current = dynamic_cast<const technology*>(it->current);
		htvi = TreeView_AddLeaf(hctl, htvroot);
		strstr.str("");
		strstr << utf8_2_ansi(current->name().c_str()) << "(" << current->id() << ")[";
		symbols["level"] = lexical_cast_default<std::string>(current->max_experience());
		strstr << utf8_2_ansi(vgettext2("Lv$level", symbols).c_str()) << "]";
		strcpy(text, strstr.str().c_str());
		// 枚举到此为止,此个config一定有孩子,强制让出来前面+符号
		LPARAM lParam = technology_tv_.size();
		technology_tv_.push_back(std::make_pair<std::string, std::vector<std::string> >(current->id(), advances_from));
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
		const technology* current = dynamic_cast<const technology*>(n->current);
		htvi = TreeView_AddLeaf(hctl, htvroot);
		strstr.str("");
		strstr << std::setw(2) << std::setfill('0') << index << ": " << utf8_2_ansi(current->name().c_str()) << "(" << current->id() << ")";
		symbols["level"] = lexical_cast_default<std::string>(current->max_experience());
		strstr << "[" << utf8_2_ansi(vgettext2("Lv$level", symbols).c_str()) << "]";
		strcpy(text, strstr.str().c_str());
		// 枚举到此为止,此个config一定有孩子,强制让出来前面+符号
		LPARAM lParam = technology_tv_.size();
		advances_from.clear();
		technology_tv_.push_back(std::make_pair<std::string, std::vector<std::string> >(current->id(), advances_from));
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

	tv_tree_.push_back(std::make_pair<std::string, std::vector<std::string> >(new_id.str(), std::vector<std::string>()));
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

void tcore::set_dirty(int bit, bool set)
{
	if (set) {
		dirty_ |= 1 << bit;
	} else {
		dirty_ &= ~(1 << bit);
	}
	core_enable_save_btn(dirty_? TRUE: FALSE);
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
		// clear updating flag
		types_updating_.clear();
		dirty_ = 0;
		return ret;
	}
	return false;
}

void tcore::save(HWND hdlgP)
{
	std::stringstream strstr;
	// verify unit type

	// one or more utype.id changed, delete <units>/<race>/<id>.cfg.
	for (std::map<int, tunit_type>::const_iterator it = types_updating_.begin(); it != types_updating_.end(); ++ it) {
		const tunit_type& type = it->second;
		if (type.utype_from_cfg_.id_ != type.id_) {
			strstr.str("");
			strstr << type.cfg_file(true);

			delfile1(strstr.str().c_str());
		}
		if (!type.id_.empty() && type.dirty()) {
			type.generate();
		}
	}

	core_enable_save_btn(FALSE);

	// clear updating flag
	types_updating_.clear();
	dirty_ = 0;

	editor_config::campaign_id = "";
	do_sync2();
}

void tcore::generate_utypes() const
{
	const unit_type_data::unit_type_map& types = unit_types.types();
	for (unit_type_data::unit_type_map::const_iterator it = types.begin(); it != types.end(); ++ it) {
		const unit_type& ut = it->second;
		ns::utype.from_config(&ut);
		ns::utype.generate();
	}
	core_enable_save_btn(TRUE);
}

void tcore::refresh_utype(HWND hdlgP)
{
	char text[_MAX_PATH];
	std::stringstream strstr;

	if (!map_) {
		const config::const_child_itors& terrains = editor_config::data_cfg.child_range("terrain_type");
		foreach (const config &t, terrains) {
			gamemap::terrain_types.add_child("terrain_type", t);
		}
		map_ = new gamemap(editor_config::data_cfg, "");
		alias_terrains_.clear();
		const t_translation::t_list& terrainList = map_->get_terrain_list();
		for (t_translation::t_list::const_iterator it = terrainList.begin(); it != terrainList.end(); ++ it) {
			const t_translation::t_terrain& terrain = *it;
			const terrain_type& info = map_->get_terrain_info(terrain);
			if (info.union_type().size() == 1 && info.union_type()[0] == info.number() && info.is_nonnull()) {
				alias_terrains_.push_back(terrain);
			}
		}
	}
	if (tunit_type::type_map_.empty()) {
		tunit_type::type_map_[tunit_type::TYPE_TROOP] = utf8_2_ansi(_("Troop"));
		tunit_type::type_map_[tunit_type::TYPE_CITY] = utf8_2_ansi(_("City"));
		tunit_type::type_map_[tunit_type::TYPE_ARTIFICAL] = utf8_2_ansi(_("Artifical"));
	}
	tunit_type::artifical_hero_.clear();
	if (gdmgr.heros_.size()) {
		tunit_type::artifical_hero_.insert(&gdmgr.heros_[hero::number_market]);
		tunit_type::artifical_hero_.insert(&gdmgr.heros_[hero::number_wall]);
		tunit_type::artifical_hero_.insert(&gdmgr.heros_[hero::number_keep]);
		tunit_type::artifical_hero_.insert(&gdmgr.heros_[hero::number_tower]);
		tunit_type::artifical_hero_.insert(&gdmgr.heros_[hero::number_technology]);
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

void tcore::refresh_technology2(HWND hdlgP)
{
	std::stringstream strstr;
	char text[_MAX_PATH];
	HWND hctl_atom = GetDlgItem(hdlgP, IDC_TV_TECHNOLOGY_ATOM);
	HWND hctl_complex = GetDlgItem(hdlgP, IDC_TV_TECHNOLOGY_COMPLEX);

	// 1. clear treeview
	TreeView_DeleteAllItems(hctl_atom);
	TreeView_DeleteAllItems(hctl_complex);

	// 2. fill content
	htvroot_technology_atom_ = TreeView_AddLeaf(hctl_atom, TVI_ROOT);
	strstr.str("");
	strstr << utf8_2_ansi(_("Atomic technology"));
	strcpy(text, strstr.str().c_str());
	// 这里一定要设TVIF_CHILDREN, 否则接下折叠后将判断出其cChildren为0, 再不能展开
	TreeView_SetItem1(hctl_atom, htvroot_technology_atom_, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN, 0, 0, 0, 
		1, text);

	htvroot_technology_complex_ = TreeView_AddLeaf(hctl_complex, TVI_ROOT);
	strstr.str("");
	strstr << utf8_2_ansi(_("Complex technology"));
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hctl_complex, htvroot_technology_complex_, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN, 0, 0, 0, 
		1, text);

	HTREEITEM htvi_technology, htvi;

	const std::map<std::string, technology>& technologies = unit_types.technologies();
	int atom_index = 0, complex_index = 0;
	for (std::map<std::string, technology>::const_iterator it = technologies.begin(); it != technologies.end(); ++ it) {
		const technology& t = it->second;
		HWND hctl;
		HTREEITEM htvroot;
		int index;
		if (!t.complex()) {
			hctl = hctl_atom;
			htvroot = htvroot_technology_atom_;
			index = atom_index ++;
		} else {
			hctl = hctl_complex;
			htvroot = htvroot_technology_complex_;
			index = complex_index ++;
		}
		htvi_technology = TreeView_AddLeaf(hctl, htvroot);
		LPARAM lParam = index;
		strstr.str("");
		strstr << std::setw(2) << std::setfill('0') << index << ": " << utf8_2_ansi(t.name().c_str()) << "(" << t.id() << ")";
		strcpy(text, strstr.str().c_str());
		TreeView_SetItem2(hctl, htvi_technology, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_CHILDREN, lParam, gdmgr._iico_dir, gdmgr._iico_dir, 1, text);

		htvroot = htvi_technology;
		// description
		htvi = TreeView_AddLeaf(hctl, htvroot);
		strstr.str("");
		strstr << utf8_2_ansi(_("Description")) << ": ";
		strstr << utf8_2_ansi(t.description().c_str());
		strcpy(text, strstr.str().c_str());
		TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM, lParam, ns::iico_technology_txt, ns::iico_technology_txt, 0, text);

		htvi = TreeView_AddLeaf(hctl, htvroot);
		strstr.str("");
		strstr << utf8_2_ansi(_("Experience")) << ": " << t.max_experience();
		strcpy(text, strstr.str().c_str());
		TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 0, ns::iico_technology_txt, ns::iico_technology_txt, 0, text);

		if (!t.complex()) {
			// occasion
			bool first = true;
			htvi = TreeView_AddLeaf(hctl, htvroot);
			strstr.str("");
			strstr << utf8_2_ansi(_("Occasion")) << ": ";
			if (t.occasion() == technology::MODIFY) {
				strstr << utf8_2_ansi(_("Adjust unit"));
				first = false;
			} else if (t.occasion() == technology::FINISH) {
				strstr << utf8_2_ansi(_("Finish research"));
				first = false;
			}
			strcpy(text, strstr.str().c_str());
			TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 0, ns::iico_technology_txt, ns::iico_technology_txt, 0, text);

			htvi = TreeView_AddLeaf(hctl, htvroot);
			strstr.str("");
			strstr << utf8_2_ansi(_("Type filter")) << ": ";
			if (t.type_filter() & filter::TROOP) {
				strstr << utf8_2_ansi(_("Troop"));
				first = false;
			}
			if (t.type_filter() & filter::ARTIFICAL) {
				if (!first) {
					strstr << ", ";
				} else {
					first = false;
				}
				strstr << utf8_2_ansi(_("Artifical"));
			}
			if (t.type_filter() & filter::CITY) {
				if (!first) {
					strstr << ", ";
				} else {
					first = false;
				}
				strstr << utf8_2_ansi(_("City"));
			}
			strcpy(text, strstr.str().c_str());
			TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 0, ns::iico_technology_txt, ns::iico_technology_txt, 0, text);

			htvi = TreeView_AddLeaf(hctl, htvroot);
			strstr.str("");
			strstr << utf8_2_ansi(_("Arms filter")) << ": ";
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
			TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 0, ns::iico_technology_txt, ns::iico_technology_txt, 0, text);

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
				}
				strcpy(text, strstr.str().c_str());
				TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_CHILDREN, 0, ns::iico_technology_action, ns::iico_technology_action, 1, text);
		} else {
			strstr.str("");
			strstr << utf8_2_ansi(_("technology^Child")) << ": ";
			const std::vector<const technology*>& parts = t.parts();
			for (std::vector<const technology*>::const_iterator it2 = parts.begin(); it2 != parts.end(); ++ it2) {
				const technology& part = **it2;
				if (it2 != parts.begin()) {
					strstr << ", ";
				}
				strstr << std::setw(2) << std::setfill('0') << index << ": " << utf8_2_ansi(part.name().c_str()) << "(" << part.id() << ")";
			}
			strcpy(text, strstr.str().c_str());
			htvi = TreeView_AddLeaf(hctl, htvroot);
			TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 0, ns::iico_technology_txt, ns::iico_technology_txt, 0, text);
		}

		TreeView_Expand(hctl, htvi_technology, TVE_EXPAND);
	}

	TreeView_Expand(hctl_atom, htvroot_technology_atom_, TVE_EXPAND);
	TreeView_Expand(hctl_complex, htvroot_technology_complex_, TVE_EXPAND);
}

void tcore::switch_section(HWND hdlgP, int to, bool force)
{
	char text[_MAX_PATH];

	if (name_map.empty()) {
		name_map[UNIT_TYPE] = utf8_2_ansi(_("arms^Type"));
		name_map[TACTIC] = utf8_2_ansi(_("Tactic"));
		name_map[TECH] = dgettext_2_ansi("wesnoth-lib", "Technology");
	}
	if (idd_map.empty()) {
		idd_map[UNIT_TYPE] = IDD_UTYPE;
		idd_map[TACTIC] = IDD_TACTIC;
		idd_map[TECH] = IDD_TECHNOLOGY;
	}
	if (dlgproc_map.empty()) {
		dlgproc_map[UNIT_TYPE] = DlgUTypeProc;
		dlgproc_map[TACTIC] = DlgTacticProc;
		dlgproc_map[TECH] = DlgTechnologyProc;
	}

	DLGHDR* pHdr = (DLGHDR*)GetWindowLong(hdlgP, GWL_USERDATA);

	if (!force && pHdr && to == section_) {
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
		DestroyWindow(pHdr->hwndDisplay);
		pHdr->hwndDisplay = NULL;
	}

	if (!pHdr->hwndDisplay) {
		// Create the new child dialog box.
		pHdr->hwndDisplay = CreateDialogIndirect(gdmgr._hinst, pHdr->apRes[to], hdlgP, dlgproc_map.find(to)->second);
		ShowWindow(pHdr->hwndDisplay, SW_RESTORE);
	}

	section_ = to;

	if (section_ == UNIT_TYPE) {
		ns::core.refresh_utype(pHdr->hwndDisplay);
	} else if (section_ == TACTIC) {
		ns::core.refresh_tactic(pHdr->hwndDisplay);
	} else if (section_ == TECH) {
		ns::core.refresh_technology(pHdr->hwndDisplay);
	} 
}

void core_enter_ui(void)
{
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
// technology section
//
BOOL On_DlgTechnologyInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	HWND hwndParent = GetParent(hdlgP); 
    DLGHDR *pHdr = (DLGHDR *) GetWindowLong(hwndParent, GWL_USERDATA);
    SetWindowPos(hdlgP, HWND_TOP, pHdr->rcDisplay.left, pHdr->rcDisplay.top, 0, 0, SWP_NOSIZE); 

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

	return FALSE;
}

BOOL On_DlgTechnology2InitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	std::stringstream strstr;
	strstr << dgettext_2_ansi("wesnoth-lib", "Technology tree");
	SetWindowText(hdlgP, strstr.str().c_str());

	Button_SetText(GetDlgItem(hdlgP, IDOK), utf8_2_ansi(_("Close")));

	ns::core.refresh_technology2(hdlgP);
	return FALSE;
}

void On_DlgTechnology2Command(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
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

BOOL CALLBACK DlgTechnology2Proc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message) {
	case WM_INITDIALOG:
		return On_DlgTechnology2InitDialog(hDlg, (HWND)(wParam), lParam);
	HANDLE_MSG(hDlg, WM_COMMAND, On_DlgTechnology2Command);
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

	}
	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	// fill data
	LVITEM lvi;
	int iItem = 0;
	bool first;
	int apply_to;
	for (std::map<std::string, technology>::const_iterator it = unit_types.technologies().begin(); it != unit_types.technologies().end(); ++ it) {
		const technology& t = it->second;

		if (ns::type == IDM_TECHNOLOGY_ATOMIC) {
			if (t.complex()) {
				continue;
			}
		} else if (ns::type == IDM_TECHNOLOGY_COMPLEX) {
			if (!t.complex()) {
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

		if (ns::type == IDM_TECHNOLOGY_ATOMIC) {
			lvi.mask = LVIF_TEXT;
			lvi.iSubItem = index ++;
			strstr.str("");
			if (t.occasion() == technology::MODIFY) {
				strstr << utf8_2_ansi(_("Adjust unit"));
			} else if (t.occasion() == technology::FINISH) {
				strstr << utf8_2_ansi(_("Finish research"));
			}
			strcpy(text, strstr.str().c_str());
			lvi.pszText = text;
			ListView_SetItem(hctl, &lvi);

			lvi.mask = LVIF_TEXT;
			lvi.iSubItem = index ++;
			strstr.str("");
			first = true;
			if (t.type_filter() & filter::TROOP) {
				strstr << utf8_2_ansi(_("Troop"));
				first = false;
			}
			if (t.type_filter() & filter::ARTIFICAL) {
				if (!first) {
					strstr << ", ";
				} else {
					first = false;
				}
				strstr << utf8_2_ansi(_("Artifical"));
			}
			if (t.type_filter() & filter::CITY) {
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
			const std::vector<const technology*>& parts = t.parts();
			for (std::vector<const technology*>::const_iterator it2 = parts.begin(); it2 != parts.end(); ++ it2) {
				const technology& part = **it2;
				if (it2 != parts.begin()) {
					strstr << ", ";
				}
				strstr << utf8_2_ansi(part.name().c_str()) << "(" << part.id() << ")";
			}
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

void On_DlgTechnologyCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	switch (id) {
	case IDM_TECHNOLOGY_ATOMIC:
	case IDM_TECHNOLOGY_COMPLEX:
		ns::type = id;
		DialogBox(gdmgr._hinst, MAKEINTRESOURCE(IDD_VISUAL2), NULL, DlgReportTechnologyProc);
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

	if (lpNMHdr->idFrom != IDC_TV_TECHNOLOGY_EXPLORER) {
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
		strstr << utf8_2_ansi(_("Atomic technology")) << "...";
		AppendMenu(hpopup_explorer, MF_STRING, IDM_TECHNOLOGY_ATOMIC, strstr.str().c_str());
		strstr.str("");
		strstr << utf8_2_ansi(_("Complex technology")) << "...";
		AppendMenu(hpopup_explorer, MF_STRING, IDM_TECHNOLOGY_COMPLEX, strstr.str().c_str());

		HMENU hpopup_technology = CreatePopupMenu();
/*
		AppendMenu(hpopup_utype, MF_STRING, IDM_ADD, utf8_2_ansi(_("Add...")));
		AppendMenu(hpopup_utype, MF_STRING, IDM_EDIT, utf8_2_ansi(_("Edit...")));
		AppendMenu(hpopup_utype, MF_STRING, IDM_DELETE, utf8_2_ansi(_("Delete...")));
		AppendMenu(hpopup_utype, MF_SEPARATOR, 0, NULL);
		AppendMenu(hpopup_utype, MF_STRING, IDM_GENERATE_ITEM2, utf8_2_ansi(_("Regenerate all type's cfg and relatve image")));
		AppendMenu(hpopup_utype, MF_SEPARATOR, 0, NULL);
*/
		AppendMenu(hpopup_technology, MF_POPUP, (UINT_PTR)(hpopup_explorer), utf8_2_ansi(_("Report format")));
/*
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
*/
		if (core_get_save_btn()) {
			// EnableMenuItem(hpopup_technology, IDM_GENERATE_ITEM2, MF_BYCOMMAND | MF_GRAYED);
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

BOOL On_DlgTechnologyNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	if (lpNMHdr->code == NM_RCLICK) {
		technology_notify_handler_rclick(hdlgP, DlgItem, lpNMHdr);
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

// 对话框消息处理函数
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