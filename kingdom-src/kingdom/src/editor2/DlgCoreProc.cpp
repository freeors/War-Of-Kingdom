/*
	兵种树树形视图中每个结点的lParam值有特珠意义
	1)根结点: 0（未使用）
	2)非结点：该兵种在tv_tree_这个std::vector中索引
*/
#define GETTEXT_DOMAIN "wesnoth"

#include "global.hpp"
#include "game_config.hpp"
#include "foreach.hpp"
#include "loadscreen.hpp"
#include "DlgCoreProc.hpp"
#include "gettext.hpp"
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
	for (std::vector<node*>::iterator it = utype_tree_.begin(); it != utype_tree_.end(); ++ it) {
		delete *it;
	}
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

void tcore::tree_2_tv_internal(HWND hctl, HTREEITEM htvroot, const std::vector<node>& advances_to, const std::vector<std::string>& advances_from2)
{
	char text[_MAX_PATH];
	HTREEITEM htvi;
	std::stringstream strstr;
	std::vector<std::string> advances_from = advances_from2;

	size_t index = 0;
	for (std::vector<node>::const_iterator it = advances_to.begin(); it != advances_to.end(); ++ it, index ++) {
		const unit_type* current = it->current;
		htvi = TreeView_AddLeaf(hctl, htvroot);
		strstr.str("");
		strstr << utf8_2_ansi(current->type_name().c_str()) << "(" << current->id() << ")[" << current->level() << "级]";
		strcpy(text, strstr.str().c_str());
		// 枚举到此为止,此个config一定有孩子,强制让出来前面+符号
		LPARAM lParam = tv_tree_.size();
		tv_tree_.push_back(std::make_pair<std::string, std::vector<std::string> >(current->id(), advances_from));
		if (it->advances_to.empty()) {
			TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM, lParam, gdmgr._iico_dir, gdmgr._iico_dir, 1, text);
		} else {
			TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_CHILDREN, lParam, gdmgr._iico_dir, gdmgr._iico_dir, 1, text);
			advances_from.push_back(current->id());
			tree_2_tv_internal(hctl, htvi, it->advances_to, advances_from);
			TreeView_Expand(hctl, htvi, TVE_EXPAND);
		}
	}
}

// void advancement_tree_internal(const unit_type& current, 
void tcore::tree_2_tv(HWND hctl, HTREEITEM htvroot)
{
	char text[_MAX_PATH];
	HTREEITEM htvi;
	std::stringstream strstr;
	std::vector<std::string> advances_from;

	size_t index = 0;
	for (std::vector<node*>::const_iterator it = utype_tree_.begin(); it != utype_tree_.end(); ++ it, index ++) {
		const node* n = *it;
		const unit_type* current = n->current;
		htvi = TreeView_AddLeaf(hctl, htvroot);
		strstr.str("");
		strstr << std::setw(2) << std::setfill('0') << index << ": " << utf8_2_ansi(current->type_name().c_str()) << "(" << current->id() << ")";
		if (!current->packer()) {
			strstr << "[" << current->level() << "级]";
		} else {
			strstr << "[打包]";
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
			tree_2_tv_internal(hctl, htvi, n->advances_to, advances_from);
			TreeView_Expand(hctl, htvi, TVE_EXPAND);
		}
	}
}

void tcore::generate_utype_tree_internal(const unit_type& current, std::vector<node>& advances_to, bool& to_branch)
{
	for (std::vector<node>::iterator it2 = advances_to.begin(); it2 != advances_to.end(); ++ it2) {
		const std::vector<std::string>& advances_to = it2->current->advances_to();
		if (std::find(advances_to.begin(), advances_to.end(), current.id()) != advances_to.end()) {
			it2->advances_to.push_back(node(&current));
			to_branch = true;
		}
		generate_utype_tree_internal(current, it2->advances_to, to_branch);
	}
}

void tcore::hang_branch_internal(std::vector<node>& advances_to, bool& hang_branch)
{
	size_t size = utype_tree_.size();
	for (std::vector<node>::iterator it = advances_to.begin(); it != advances_to.end(); ++ it) {
		node* n = &*it;
		const std::vector<std::string>& advances_to = n->current->advances_to();
		for (size_t hanging = 0; hanging < size; hanging ++) {
			if (!utype_tree_[hanging]) {
				continue;
			}
			node* hanging_n = utype_tree_[hanging];
			if (std::find(advances_to.begin(), advances_to.end(), hanging_n->current->id()) != advances_to.end()) {
				n->advances_to.push_back(*hanging_n);
				delete hanging_n;
				utype_tree_[hanging] = NULL;
				hang_branch = true;
			}
		}
		hang_branch_internal(n->advances_to, hang_branch);
	}
}

void tcore::generate_utype_tree()
{
	for (std::vector<node*>::iterator it = utype_tree_.begin(); it != utype_tree_.end(); ++ it) {
		delete *it;
	}
	utype_tree_.clear();
	types_updating_.clear();

	const unit_type_data::unit_type_map& types = unit_types.types();
	for (std::map<std::string, unit_type>::const_iterator it = types.begin(); it != types.end(); ++ it) {
		const unit_type& current = it->second;
		bool to_branch = false;
		for (std::vector<node*>::iterator it2 = utype_tree_.begin(); it2 != utype_tree_.end(); ++ it2) {
			node* n = *it2;
			const std::vector<std::string>& advances_to = n->current->advances_to();
			if (std::find(advances_to.begin(), advances_to.end(), current.id()) != advances_to.end()) {
				n->advances_to.push_back(node(&current));
				to_branch = true;
			}
			generate_utype_tree_internal(current, n->advances_to, to_branch);
		}
		if (!to_branch) {
			utype_tree_.push_back(new node(&current));
		}
	}

	// check every teminate node, if necessary, hang root-branch to it.
	size_t size = utype_tree_.size();
	int max_hang_times = 2;
	bool hang_branch = false;
	do {
		hang_branch = false;
		for (size_t analysing = 0; analysing < size; analysing ++) {
			if (!utype_tree_[analysing]) {
				continue;
			}
			// search terminate node
			node* n = utype_tree_[analysing];
			const std::vector<std::string>& advances_to = n->current->advances_to();
			for (size_t hanging = 0; hanging < size; hanging ++) {
				if (!utype_tree_[hanging]) {
					continue;
				}
				node* hanging_n = utype_tree_[hanging];
				if (std::find(advances_to.begin(), advances_to.end(), hanging_n->current->id()) != advances_to.end()) {
					n->advances_to.push_back(*hanging_n);
					delete hanging_n;
					utype_tree_[hanging] = NULL;
					hang_branch = true;
				}
			}
			hang_branch_internal(n->advances_to, hang_branch);
		}
	} while (-- max_hang_times && hang_branch);

	if (hang_branch) {
		// I think, one while is enogh.
		int ii = 0;
	}

	// remove branch that is NULL.
	for (std::vector<node*>::iterator it = utype_tree_.begin(); it != utype_tree_.end(); ) {
		if (*it == NULL) {
			it = utype_tree_.erase(it);
		} else {
			++ it;
		}
	}
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
	for (std::vector<tcore::node*>::const_iterator it = ns::core.utype_tree_.begin(); it != ns::core.utype_tree_.end(); ++ it) {
		const unit_type* that = (*it)->current;
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

		title << "保存修改"; 
		message << "核心数据有改动，您想保存修改吗？";

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
		tunit_type::type_map_[tunit_type::TYPE_TROOP] = "部队";
		tunit_type::type_map_[tunit_type::TYPE_CITY] = "城市";
		tunit_type::type_map_[tunit_type::TYPE_ARTIFICAL] = "建筑物";
	}
	if (tunit_type::artifical_hero_.empty()) {
		tunit_type::artifical_hero_[hero::number_market] = "市场";
		tunit_type::artifical_hero_[hero::number_wall] = "城墙";
		tunit_type::artifical_hero_[hero::number_keep] = "主楼";
		tunit_type::artifical_hero_[hero::number_tower] = "箭塔";
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
	strstr << "兵种(" << unit_types.types().size() << ")";
	strcpy(text, strstr.str().c_str());
	// 这里一定要设TVIF_CHILDREN, 否则接下折叠后将判断出其cChildren为0, 再不能展开
	TreeView_SetItem1(hctl, htvroot_utype_, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN, 0, 0, 0, 
		unit_types.types().size()? 1: 0, text);
	tree_2_tv(hctl, htvroot_utype_);

	TreeView_Expand(hctl, htvroot_utype_, TVE_EXPAND);
}

void tcore::refresh_tactic(HWND hdlgP)
{
	char text[_MAX_PATH];
	std::stringstream strstr;

/*
	if (types_updating_.empty()) {
		generate_utype_tree();
	}
*/
	HWND hctl_atom = GetDlgItem(hdlgP, IDC_TV_TACTIC_ATOM);
	HWND hctl_complex = GetDlgItem(hdlgP, IDC_TV_TACTIC_COMPLEX);

	// 1. clear treeview
	TreeView_DeleteAllItems(hctl_atom);
	TreeView_DeleteAllItems(hctl_complex);

	// 2. fill content
	htvroot_tactic_atom_ = TreeView_AddLeaf(hctl_atom, TVI_ROOT);
	strstr.str("");
	strstr << "原子战法";
	strcpy(text, strstr.str().c_str());
	// 这里一定要设TVIF_CHILDREN, 否则接下折叠后将判断出其cChildren为0, 再不能展开
	TreeView_SetItem1(hctl_atom, htvroot_tactic_atom_, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN, 0, 0, 0, 
		1, text);

	htvroot_tactic_complex_ = TreeView_AddLeaf(hctl_complex, TVI_ROOT);
	strstr.str("");
	strstr << "复合战法";
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
		strstr << "描述: " << utf8_2_ansi(t.description().c_str());
		strcpy(text, strstr.str().c_str());
		TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM, lParam, ns::iico_tactic_txt, ns::iico_tactic_txt, 0, text);

		htvi = TreeView_AddLeaf(hctl, htvroot);
		strstr.str("");
		strstr << "消耗: " << t.point();
		strcpy(text, strstr.str().c_str());
		TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 0, ns::iico_tactic_txt, ns::iico_tactic_txt, 0, text);

		if (t.index() < ttactic::min_complex_index) {
			// range
			bool first = true;
			htvi = TreeView_AddLeaf(hctl, htvroot);
			strstr.str("");
			strstr << "范围: ";
			if (t.range() & ttactic::SELF) {
				strstr << "自已";
				first = false;
			}
			if (t.range() & ttactic::FRIEND) {
				if (!first) {
					strstr << ", ";
				}
				strstr << "友军";
				first = false;
			}
			if (t.range() & ttactic::ENEMY) {
				if (!first) {
					strstr << ", ";
				}
				strstr << "敌军";
				first = false;
			}
			strcpy(text, strstr.str().c_str());
			TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 0, ns::iico_tactic_range, ns::iico_tactic_range, 0, text);

			const config& action = t.action_cfg();
			foreach (const config& effect, action.child_range("effect")) {
				const std::string apply_to = effect["apply_to"].str();
				htvi = TreeView_AddLeaf(hctl, htvroot);
				strstr.str("");
				if (apply_to == "resistance") {
					strstr << "防御";
				} else if (apply_to == "attack") {
					strstr << "攻击";
				} else if (apply_to == "encourage") {
					strstr << "士气";
				} else if (apply_to == "demolish") {
					strstr << "破坏";
				}
				strcpy(text, strstr.str().c_str());
				TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_CHILDREN, 0, ns::iico_tactic_action, ns::iico_tactic_action, 1, text);
			}
		} else {
			strstr.str("");
			strstr << "子战法: ";
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

/*
		LPARAM lParam = tv_tree_.size();
		advances_from.clear();
		tv_tree_.push_back(std::make_pair<std::string, std::vector<std::string> >(current->id(), advances_from));
		if (n->advances_to.empty()) {
			TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM, lParam, gdmgr._iico_dir, gdmgr._iico_dir, 1, text);
		} else {
			TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_CHILDREN, lParam, gdmgr._iico_dir, gdmgr._iico_dir, 1, text);
			advances_from.push_back(current->id());
			tree_2_tv_internal(hctl, htvi, n->advances_to, advances_from);
			TreeView_Expand(hctl, htvi, TVE_EXPAND);
		}
*/
		TreeView_Expand(hctl, htvi_tactic, TVE_EXPAND);
	}

	TreeView_Expand(hctl_atom, htvroot_tactic_atom_, TVE_EXPAND);
	TreeView_Expand(hctl_complex, htvroot_tactic_complex_, TVE_EXPAND);
}

void tcore::switch_section(HWND hdlgP, int to, bool force)
{
	if (name_map.empty()) {
		name_map[UNIT_TYPE] = "兵种";
		name_map[TACTIC] = "战法";
	}
	if (idd_map.empty()) {
		idd_map[UNIT_TYPE] = IDD_UTYPE;
		idd_map[TACTIC] = IDD_TACTIC;
	}
	if (dlgproc_map.empty()) {
		dlgproc_map[UNIT_TYPE] = DlgUTypeProc;
		dlgproc_map[TACTIC] = DlgTacticProc;
	}

	char text[_MAX_PATH];
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
/*
	HMENU hpopup_explorer = CreatePopupMenu();
	AppendMenu(hpopup_explorer, MF_STRING, IDM_UTYPE_GENERAL, "基本信息...");
	AppendMenu(hpopup_explorer, MF_STRING, IDM_UTYPE_MTYPE, "抗性...");
	AppendMenu(hpopup_explorer, MF_STRING, IDM_UTYPE_ATTACKI, "攻击I...");
	AppendMenu(hpopup_explorer, MF_STRING, IDM_UTYPE_ATTACKII, "攻击II...");
	AppendMenu(hpopup_explorer, MF_STRING, IDM_UTYPE_ATTACKIII, "攻击III...");

	HMENU hpopup_utype = CreatePopupMenu();
	AppendMenu(hpopup_utype, MF_STRING, IDM_ADD, "添加...");
	AppendMenu(hpopup_utype, MF_STRING, IDM_EDIT, "编辑...");
	AppendMenu(hpopup_utype, MF_STRING, IDM_DELETE, "删除");
	AppendMenu(hpopup_utype, MF_SEPARATOR, 0, NULL);
	AppendMenu(hpopup_utype, MF_STRING, IDM_GENERATE_ITEM2, "重新生成所有兵种配置、相关图像");
	AppendMenu(hpopup_utype, MF_SEPARATOR, 0, NULL);
	AppendMenu(hpopup_utype, MF_POPUP, (UINT_PTR)(hpopup_explorer), "报表格式");

	if (!htvi || htvi == ns::core.htvroot_tactic_atoi_) {
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
*/
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