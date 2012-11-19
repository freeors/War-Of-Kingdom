/*
	兵种树树形视图中每个结点的lParam值有特珠意义
	1)根结点: 0（未使用）
	2)非结点：该兵种在tv_tree_这个std::vector中索引
*/
#define GETTEXT_DOMAIN "wesnoth"

#include "global.hpp"
#include "game_config.hpp"
#include "config.hpp"
#include "foreach.hpp"
#include "loadscreen.hpp"
#include "gettext.hpp"
#include "editor.hpp"
#include "stdafx.h"
#include <windowsx.h>
#include <string.h>
#include <shlobj.h> // SHBrowseForFolder

#include "resource.h"

#include "xfunc.h"
#include "win32x.h"
#include "struct.h"

#include "unit_types.hpp"
#include "map.hpp"

#define core_enable_save_btn(fEnable)	ToolBar_EnableButton(gdmgr._htb_core, IDM_SAVE, fEnable)
#define core_get_save_btn()				(ToolBar_GetState(gdmgr._htb_core, IDM_SAVE) & TBSTATE_ENABLED)

static const std::string null_str = "";
std::vector<std::string> attack_range_vstr;
std::vector<std::string> attack_type_vstr;

class tunit_type_
{
public:
	tunit_type_(const std::string& id = null_str, const std::string& firstscenario_id = null_str) 
		: textdomain_(PACKAGE)
		, id_(id)
		, race_("human")
		, arms_(0)
		, hitpoints_(1)
		, experience_needed_(1)
		, movement_(1)
		, level_(1)
		, cost_(1)
		, alignment_(unit_type::LAWFUL)
		, character_(NO_CHARACTER)
		, movement_type_()
		, resistance_()
		, zoc_(true)
		, land_wall_(true)
		, leader_(false)
		, advances_to_()
		, abilities_()
		, can_recruit_(false)
		, master_(HEROS_INVALID_NUMBER)
		, turn_experience_(0)
		, heal_(0)
		, guard_(NO_GUARD)
		, income_(0)
		, packer_(false)
		, melee_increase_(0)
		, ranged_increase_(0)
		, cast_increase_(0)
	{}

	bool operator==(const tunit_type_& that) const
	{
		if (textdomain_ != that.textdomain_) return false;
		if (id_ != that.id_) return false;
		if (race_ != that.race_) return false;
		if (arms_ != that.arms_) return false;
		if (hitpoints_ != that.hitpoints_) return false;
		if (experience_needed_ != that.experience_needed_) return false;
		if (level_ != that.level_) return false;
		if (cost_ != that.cost_) return false;
		if (alignment_ != that.alignment_) return false;
		if (movement_type_ != that.movement_type_) return false;
		if (resistance_ != that.resistance_) return false;
		if (zoc_ != that.zoc_) return false;
		if (advances_to_ != that.advances_to_) return false;
		if (abilities_ != that.abilities_) return false;
		if (can_recruit_ != that.can_recruit_) return false;
		if (master_ != that.master_) return false;
		if (movement_ != that.movement_) return false;
		if (character_ != that.character_) return false;
		if (land_wall_ != that.land_wall_) return false;
		if (leader_ != that.leader_) return false;
		if (turn_experience_ != that.turn_experience_) return false;
		if (heal_ != that.heal_) return false;
		if (guard_ != that.guard_) return false;
		if (income_ != that.income_) return false;
		if (packer_ != that.packer_) return false;
		if (melee_increase_ != that.melee_increase_) return false;
		if (ranged_increase_ != that.ranged_increase_) return false;
		if (cast_increase_ != that.cast_increase_) return false;
		return true;
	}
	bool operator!=(const tunit_type_& that) const { return !operator==(that); }

public:
	std::string textdomain_;
	std::string id_;
	std::string race_;
	int arms_;
	int hitpoints_;
	int experience_needed_;
	int level_;
	int cost_;
	int alignment_;
	std::string movement_type_;
	std::map<std::string, int> resistance_;
	bool zoc_;
	std::set<std::string> advances_to_;
	std::set<std::string> abilities_;
	bool can_recruit_;
	int master_;
	// valid on troop
	int movement_;
	int character_;
	bool land_wall_;
	bool leader_;
	// valid on city/artifical
	int turn_experience_;
	int heal_;
	int guard_;
	int income_;

	bool packer_;
	int melee_increase_;
	int ranged_increase_;
	int cast_increase_;
};

class tunit_type: public tunit_type_
{
public:
	class tattack 
	{
	public:
		tattack() 
			: id_()
			, icon_()
			, type_()
			, range_(0)
			, specials_()
			, damage_(1)
			, number_(1)
		{}

		void from_config(const attack_type& attack);
		void from_ui(HWND hdlgP);
		void update_to_ui_utype_edit(HWND hdlgP, int index = -1) const;
		void update_to_ui_attack_edit(HWND hdlgP) const;
		void generate(std::stringstream& strstr, const std::string& prefix) const;

		bool operator==(const tattack& that) const
		{
			if (id_ != that.id_) return false;
			if (icon_ != that.icon_) return false;
			if (type_ != that.type_) return false;
			if (range_ != that.range_) return false;
			if (specials_ != that.specials_) return false;
			if (damage_ != that.damage_) return false;
			if (number_ != that.number_) return false;
			return true;
		}
		bool operator!=(const tattack& that) const { return !operator==(that); }

		std::string icon(bool absolute = false) const;

	public:
		std::string id_;
		std::string icon_;
		std::string type_;
		int range_;
		std::set<std::string> specials_;
		int damage_;
		int number_;
	};

	tunit_type(const std::string& id = null_str);

	void from_config(const unit_type* ut);
	void from_ui(HWND hdlgP);
	void from_ui_mtype_edit(HWND hdlgP);
	void from_ui_utype_type(HWND hdlgP);
	void update_to_ui_utype_edit(HWND hdlgP) const;
	void update_to_ui_utype_edit_mtype(HWND hdlgP) const;
	void update_to_ui_utype_edit_type(HWND hdlgP) const;
	void update_to_ui_mtype_edit(HWND hdlgP) const;
	void update_to_ui_utype_type(HWND hdlgP) const;
	void generate() const;

	bool has_halo() const;

	bool has_healing_anim() const;
	bool has_resistance_anim() const;
	bool has_leading_anim() const;

	void vertify_core_images() const;
	void healing_anim(std::stringstream& strstr, const std::string& prefix) const;
	void vertify_healing_anim_images() const;
	void resistance_anim(std::stringstream& strstr, const std::string& prefix) const;
	void vertify_resistance_anim_images() const;
	void leading_anim(std::stringstream& strstr, const std::string& prefix) const;
	void vertify_leading_anim_images() const;
	void idle_anim(std::stringstream& strstr, const std::string& prefix) const;
	void vertify_idle_anim_images() const;
	void attack_anim_melee(const std::string& aid, const std::string& aicon, std::stringstream& strstr, const std::string& prefix) const;
	void vertify_attack_anim_melee_images() const;
	void attack_anim_ranged(const std::string& aid, const std::string& aicon, std::stringstream& strstr, const std::string& prefix) const;
	void vertify_attack_anim_ranged_images() const;
	void attack_anim_ranged_magic_missile(const std::string& aid, std::stringstream& strstr, const std::string& prefix) const;
	void attack_anim_ranged_lightbeam(const std::string& aid, std::stringstream& strstr, const std::string& prefix) const;
	void attack_anim_ranged_fireball(const std::string& aid, std::stringstream& strstr, const std::string& prefix) const;
	void attack_anim_ranged_iceball(const std::string& aid, std::stringstream& strstr, const std::string& prefix) const;
	void attack_anim_ranged_lightning(const std::string& aid, std::stringstream& strstr, const std::string& prefix) const;
	
	bool new_attack();
	void erase_attack(int index, HWND hdlgP);

	bool use_terrain_image() const;
	std::string model_image() const;
	std::string file(bool absolute) const;
	std::string image(bool absolute) const;
	std::string hit(bool absolute) const;
	std::string miss(bool absolute) const;
	std::string leading(bool absolute) const;
	std::string idle(bool absolute, int number) const;
	std::string attack_image(bool absolute, int range, int number) const;
	std::string cfg_file(bool absolute) const;
	std::string description() const;

	enum {TYPE_TROOP = 0, TYPE_CITY, TYPE_ARTIFICAL};
	int type() const;
	int type_from_cfg() const;

	bool dirty() const;

public:
	static std::map<int, std::string> type_map_;
	static std::map<int, std::string> artifical_hero_;

	std::vector<tattack> attacks_;
	tunit_type_ utype_from_cfg_;
	std::vector<const unit_type*> candidate_advances_to_;

private:
	std::vector<tattack> attacks_from_cfg_;
};

class tcore
{
public:
	tcore();
	~tcore();

	struct node {
		node(const unit_type* ut)
			: current(ut)
			, advances_to()
		{}

		const unit_type* current;
		std::vector<node> advances_to;
	};

	HWND init_toolbar(HINSTANCE hinst, HWND hdlgP);
	void refresh(HWND hctl);
	void tree_2_tv(HWND hctl, HTREEITEM htvroot);
	void tree_2_tv_internal(HWND hctl, HTREEITEM htvroot, const std::vector<node>& advances_to, const std::vector<std::string>& advances_from2);

	bool new_utype();
	void erase_utype(int index, HWND hdlgP);

	bool types_dirty() const;

	enum {BIT_UTYPE = 0, BIT_CHARACTER};
	void set_dirty(int bit, bool set);

	bool save_if_dirty();
	void save(HWND hdlgP);
	void generate_utypes() const;

private:
	void generate_tree();
	void generate_tree_internal(const unit_type& current, std::vector<node>& advances_to, bool& to_branch);
	void hang_branch_internal(std::vector<node>& advances_to, bool& hang_branch);

public:
	// second: unit type that can advance to
	std::vector<node*> tree_;
	std::vector<std::pair<std::string, std::vector<std::string> > > tv_tree_;

	gamemap* map_;
	t_translation::t_list alias_terrains_;
	HTREEITEM htvroot_utype_;

	std::map<int, tunit_type> types_updating_;

	uint32_t dirty_;
};

namespace ns {
	const std::string default_utype_textdomain = "wesnoth-tk-units";
	const std::string default_utype_id = "utype_id_";
	const std::string default_attack_id = "attack_id_";
	const std::string default_attack_icon = "sword-human.png";

	tcore core;
	tunit_type utype;

	int clicked_utype;
	int clicked_attack;
	int clicked_mtype;
	
	int action_utype;
	int action_attack;

	extern int type;
}

tcore::tcore()
	: map_(NULL)
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
	for (std::vector<node*>::iterator it = tree_.begin(); it != tree_.end(); ++ it) {
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
	for (std::vector<node*>::const_iterator it = tree_.begin(); it != tree_.end(); ++ it, index ++) {
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

void tcore::generate_tree_internal(const unit_type& current, std::vector<node>& advances_to, bool& to_branch)
{
	for (std::vector<node>::iterator it2 = advances_to.begin(); it2 != advances_to.end(); ++ it2) {
		const std::vector<std::string>& advances_to = it2->current->advances_to();
		if (std::find(advances_to.begin(), advances_to.end(), current.id()) != advances_to.end()) {
			it2->advances_to.push_back(node(&current));
			to_branch = true;
		}
		generate_tree_internal(current, it2->advances_to, to_branch);
	}
}

void tcore::hang_branch_internal(std::vector<node>& advances_to, bool& hang_branch)
{
	size_t size = tree_.size();
	for (std::vector<node>::iterator it = advances_to.begin(); it != advances_to.end(); ++ it) {
		node* n = &*it;
		const std::vector<std::string>& advances_to = n->current->advances_to();
		for (size_t hanging = 0; hanging < size; hanging ++) {
			if (!tree_[hanging]) {
				continue;
			}
			node* hanging_n = tree_[hanging];
			if (std::find(advances_to.begin(), advances_to.end(), hanging_n->current->id()) != advances_to.end()) {
				n->advances_to.push_back(*hanging_n);
				delete hanging_n;
				tree_[hanging] = NULL;
				hang_branch = true;
			}
		}
		hang_branch_internal(n->advances_to, hang_branch);
	}
}

void tcore::generate_tree()
{
	for (std::vector<node*>::iterator it = tree_.begin(); it != tree_.end(); ++ it) {
		delete *it;
	}
	tree_.clear();
	tv_tree_.clear();
	types_updating_.clear();

	const unit_type_data::unit_type_map& types = unit_types.types();
	for (std::map<std::string, unit_type>::const_iterator it = types.begin(); it != types.end(); ++ it) {
		const unit_type& current = it->second;
		bool to_branch = false;
		for (std::vector<node*>::iterator it2 = tree_.begin(); it2 != tree_.end(); ++ it2) {
			node* n = *it2;
			const std::vector<std::string>& advances_to = n->current->advances_to();
			if (std::find(advances_to.begin(), advances_to.end(), current.id()) != advances_to.end()) {
				n->advances_to.push_back(node(&current));
				to_branch = true;
			}
			generate_tree_internal(current, n->advances_to, to_branch);
		}
		if (!to_branch) {
			tree_.push_back(new node(&current));
		}
	}

	// check every teminate node, if necessary, hang root-branch to it.
	size_t size = tree_.size();
	int max_hang_times = 2;
	bool hang_branch = false;
	do {
		hang_branch = false;
		for (size_t analysing = 0; analysing < size; analysing ++) {
			if (!tree_[analysing]) {
				continue;
			}
			// search terminate node
			node* n = tree_[analysing];
			const std::vector<std::string>& advances_to = n->current->advances_to();
			for (size_t hanging = 0; hanging < size; hanging ++) {
				if (!tree_[hanging]) {
					continue;
				}
				node* hanging_n = tree_[hanging];
				if (std::find(advances_to.begin(), advances_to.end(), hanging_n->current->id()) != advances_to.end()) {
					n->advances_to.push_back(*hanging_n);
					delete hanging_n;
					tree_[hanging] = NULL;
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
	for (std::vector<node*>::iterator it = tree_.begin(); it != tree_.end(); ) {
		if (*it == NULL) {
			it = tree_.erase(it);
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
	for (std::vector<tcore::node*>::const_iterator it = ns::core.tree_.begin(); it != ns::core.tree_.end(); ++ it) {
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
		if (retval == IDYES) {
			save(gdmgr._hdlg_core);
			return true;
		} else {
			dirty_ = 0;
			core_enable_save_btn(FALSE);
			return false;
		}
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
	// clear updating flag
	types_updating_.clear();
	dirty_ = 0;

	core_enable_save_btn(FALSE);

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

void core_enter_ui(void)
{
/*
	if (ns::core.save_if_dirty()) {
		return;
	}
*/
	HWND hctl = GetDlgItem(gdmgr._hdlg_core, IDC_TV_CORE_EXPLORER);

	StatusBar_Idle();

	strcpy(gdmgr.cfg_fname_, gdmgr._menu_text);
	StatusBar_SetText(gdmgr._hwnd_status, 0, gdmgr.cfg_fname_);

	ns::core.refresh(hctl);
	return;
}

BOOL core_hide_ui(void)
{
	if (ns::core.save_if_dirty()) {
		return FALSE;
	}
	return TRUE;
}

std::map<int, std::string> tunit_type::type_map_;
std::map<int, std::string> tunit_type::artifical_hero_;

tunit_type::tunit_type(const std::string& id)
	: tunit_type_(id)
	, candidate_advances_to_()
{
}

void tunit_type::from_config(const unit_type* ut)
{
	id_ = ut->id();
	race_ = ut->race();
	arms_ = ut->arms();
	std::vector<t_string_base::trans_str> trans = ut->type_name().valuex();
	for (std::vector<t_string_base::trans_str>::const_iterator ti = trans.begin(); ti != trans.end(); ti ++) {
		// only support one textdomain
		textdomain_ = ti->td;
		break;
	}
	hitpoints_ = ut->hitpoints();
	experience_needed_ = ut->experience_needed(false);
	movement_ = ut->movement();
	level_ = ut->level();
	cost_ = ut->cost();
	alignment_ = ut->alignment();
	character_ = ut->character();
	movement_type_ = ut->movementType_id();
	// [resistance]
	resistance_.clear();
	const unit_movement_type &movement_type = ut->movement_type();
	utils::string_map dam_tab = movement_type.damage_table();
	for (std::vector<std::string>::const_iterator it = attack_type_vstr.begin(); it != attack_type_vstr.end(); ++ it) {
		// 使用值
		utils::string_map::iterator dam_it = dam_tab.find(*it);
		if (dam_it != dam_tab.end()) {
			int resistance = 100 - atoi((*dam_it).second.c_str());
			resistance_.insert(std::make_pair<std::string, int>(*it, resistance));
		} else {
			// cannot enter it.
			resistance_.insert(std::make_pair<std::string, int>(*it, -500));
		}
	}
	zoc_ = ut->has_zoc();
	land_wall_ = ut->land_wall();
	leader_ = ut->leader();
	advances_to_.clear();
	for (std::vector<std::string>::const_iterator it = ut->advances_to().begin(); it != ut->advances_to().end(); ++ it) {
		advances_to_.insert(*it);
	}
	// advancement_ = ut->advancement();
	abilities_.clear();
	const std::multimap<const std::string, const config*>& abilities_cfg = ut->abilities_cfg();
	for (std::multimap<const std::string, const config*>::const_iterator it = abilities_cfg.begin(); it != abilities_cfg.end(); ++ it) {
		const config& ab_cfg = *(it->second);
		const std::string& id = ab_cfg["id"].str();
		if (!id.empty()) {
			abilities_.insert(id);
		}
	}
	can_recruit_ = ut->can_recruit();
	master_ = ut->master();
	turn_experience_ = ut->turn_experience();
	heal_ = ut->heal();
	guard_ = ut->guard();
	income_ = ut->gold_income();

	packer_ = ut->packer();
	if (packer_) {
		const config& pack_cfg = ut->get_cfg().child("pack");
		foreach (const config &effect, pack_cfg.child_range("effect")) {
			const std::string &apply_to = effect["apply_to"];
			if (apply_to != "attack") {
				continue;
			}
			const std::string &range = effect["range"];
			int increase = effect["increase_damage"].to_int(0);
			if (range == attack_range_vstr[0]) {
				melee_increase_ = increase;
			} else if (range == attack_range_vstr[1]) {
				ranged_increase_ = increase;
			} else if (range == attack_range_vstr[2]) {
				cast_increase_ = increase;
			}
		}
	}

	// generate can advacne to unit type.
	candidate_advances_to_.clear();
	for (std::set<std::string>::const_iterator it = advances_to_.begin(); it != advances_to_.end(); ++ it) {
		const unit_type* that = unit_types.find(*it);
		candidate_advances_to_.push_back(that);
	}
	for (std::vector<tcore::node*>::const_iterator it = ns::core.tree_.begin(); it != ns::core.tree_.end(); ++ it) {
		const unit_type* that = (*it)->current;
		if (that->packer()) {
			continue;
		}
		if (ut->master() != that->master()) {
			continue;
		}
		if (ut->can_recruit() != that->can_recruit()) {
			continue;
		}

		if (ns::core.tv_tree_[ns::clicked_utype].second.empty()) {
			if (ns::core.tv_tree_[ns::clicked_utype].first == that->id()) {
				continue;
			}
		} else if (ns::core.tv_tree_[ns::clicked_utype].second[0] == that->id()) {
			continue;
		}
		candidate_advances_to_.push_back(that);
	}
	
	attacks_.clear();
	std::vector<attack_type> attacks = ut->attacks();
	for (std::vector<attack_type>::const_iterator it = attacks.begin(); it != attacks.end(); ++ it) {
		attacks_.push_back(tattack());
		attacks_.back().from_config(*it);
	}

	// remember attack, will use to compare in future.
	attacks_from_cfg_ = attacks_;
	utype_from_cfg_ = *this;
}


void tunit_type::from_ui(HWND hdlgP)
{
	HWND hctl;

	if (!packer_) {
		hitpoints_ = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_UTYPEEDIT_HP));
		experience_needed_ = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_UTYPEEDIT_XP));
		cost_ = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_UTYPEEDIT_COST));

		hctl = GetDlgItem(hdlgP, IDC_CMB_UTYPEEDIT_LEVEL);
		level_ = ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));

		hctl = GetDlgItem(hdlgP, IDC_CMB_UTYPEEDIT_ALIGNMENT);
		alignment_ = ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));

		
		advances_to_.clear();
		hctl = GetDlgItem(hdlgP, IDC_LV_UTYPEEDIT_ADVANCESTO);
		int index;
		for (index = 0; index < ListView_GetItemCount(hctl); index ++) {
			if (ListView_GetCheckState(hctl, index)) {
				advances_to_.insert(candidate_advances_to_[index]->id());
			}
		}

		// abilities
		abilities_.clear();
		hctl = GetDlgItem(hdlgP, IDC_LV_UTYPEEDIT_ABILITIES);
		std::set<int> items;
		const abilities_map& abilities_cfg = unit_types.abilities();
		for (index = 0; index < ListView_GetItemCount(hctl); index ++) {
			if (ListView_GetCheckState(hctl, index)) {
				items.insert(index);
			}
		}
		index = 0;
		for (abilities_map::const_iterator it = abilities_cfg.begin(); it != abilities_cfg.end(); ++ it) {
			foreach (const config::any_child &c, it->second.all_children_range()) {
				const config& ab_cfg = c.cfg;
				const std::string& id = ab_cfg["id"].str();
				if (id.empty()) {
					continue;
				}
				if (items.find(index) != items.end()) {
					abilities_.insert(id);
				}
				index ++;
			}
		}
	} else {
		melee_increase_ = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_UTYPEEDIT_MELEEINCREASE));
		ranged_increase_ = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_UTYPEEDIT_RANGEDINCREASE));
		cast_increase_ = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_UTYPEEDIT_CASTINCREASE));
	}

	hctl = GetDlgItem(hdlgP, IDC_CMB_UTYPEEDIT_ARMS);
	arms_ = ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));

	zoc_ = Button_GetCheck(GetDlgItem(hdlgP, IDC_CHK_UTYPEEDIT_ZOC))? true: false;

	// remember them!
	if (id_ != ns::core.tv_tree_[ns::clicked_utype].first) {
		ns::core.tv_tree_[ns::clicked_utype].first = id_;
	}
	ns::core.types_updating_[ns::clicked_utype] = *this;
}

void tunit_type::from_ui_mtype_edit(HWND hdlgP)
{
	char text[_MAX_PATH];
	LVITEM lvi;
	HWND hctl = GetDlgItem(hdlgP, IDC_LV_MTYPEEDIT_RESISTANCE);
	for (int index = 0; index < ListView_GetItemCount(hctl); index ++) {
		// default value
		lvi.iItem = index;
		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		lvi.iSubItem = 2;
		lvi.pszText = text;
		lvi.cchTextMax = _MAX_PATH;
		ListView_GetItem(hctl, &lvi);
		int value = atoi(text);
		resistance_[attack_type_vstr[index]] = value;
	}
}

void tunit_type::from_ui_utype_type(HWND hdlgP)
{
	HWND hctl;
	if (ns::utype.type() == tunit_type::TYPE_TROOP) {
		movement_ = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_UTYPETROOP_MOVEMENT));

		hctl = GetDlgItem(hdlgP, IDC_CMB_UTYPETROOP_CHARACTER);
		character_ = ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));

		land_wall_ = Button_GetCheck(GetDlgItem(hdlgP, IDC_CHK_UTYPETROOP_LANDWALL))? true: false;
		leader_ = Button_GetCheck(GetDlgItem(hdlgP, IDC_CHK_UTYPETROOP_LEADER))? true: false;

	} else if (ns::utype.type() == tunit_type::TYPE_CITY) {
		turn_experience_ = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_UTYPECITY_TURNEXPERIENCE));
		heal_ = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_UTYPECITY_HEAL));

	} else if (ns::utype.type() == tunit_type::TYPE_ARTIFICAL) {
		heal_ = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_UTYPEARTIFICAL_HEAL));

		hctl = GetDlgItem(hdlgP, IDC_CMB_UTYPEARTIFICAL_MASTER);
		master_ = ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));

		hctl = GetDlgItem(hdlgP, IDC_CMB_UTYPEARTIFICAL_GUARD);
		guard_ = ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));

		if (master_ == hero::number_market) {
			income_ = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_UTYPEARTIFICAL_INCOME));
		}
	}
}

void tunit_type::update_to_ui_utype_edit(HWND hdlgP) const
{
	std::stringstream strstr;
	char text[1024];

	HWND hctl = GetDlgItem(hdlgP, IDC_LV_UTYPEEDIT_ADVANCESTO);
	ListView_DeleteAllItems(hctl);
	int index = 0;
	LVITEM lvi;
	for (std::vector<const unit_type*>::const_iterator it = candidate_advances_to_.begin(); it != candidate_advances_to_.end(); ++ it) {
		const unit_type* ut = *it;
		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		// 名称
		lvi.iItem = index ++;
		lvi.iSubItem = 0;
		strstr.str("");
		strstr << utf8_2_ansi(ut->type_name().c_str()) << "(" << ut->id() << ")";
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		lvi.lParam = (LPARAM)0;
		ListView_InsertItem(hctl, &lvi);

		// 等级
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 1;
		strstr.str("");
		strstr << ut->level();
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		if (advances_to_.find(ut->id()) != advances_to_.end()) {
			ListView_SetCheckState(hctl, lvi.iItem, TRUE);
		} else {
			ListView_SetCheckState(hctl, lvi.iItem, FALSE);
		}
	}
	strstr.str("");
	strstr << "升级到(" << editor_config::ListView_GetCheckedCount(hctl) << ")";
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_UTYPEEDIT_ADVANCESTO), strstr.str().c_str());

	hctl = GetDlgItem(hdlgP, IDC_LV_UTYPEEDIT_ABILITIES);
	ListView_DeleteAllItems(hctl);
	index = 0;
	const abilities_map& abilities_cfg = unit_types.abilities();
	for (abilities_map::const_iterator it = abilities_cfg.begin(); it != abilities_cfg.end(); ++ it) {
		foreach (const config::any_child &c, it->second.all_children_range()) {
			const config& ab_cfg = c.cfg;
			const std::string& id = ab_cfg["id"].str();
			if (id.empty()) {
				continue;
			}

			lvi.mask = LVIF_TEXT | LVIF_PARAM;
			// 名称
			lvi.iItem = index ++;
			lvi.iSubItem = 0;
			strstr.str("");
			strstr << utf8_2_ansi(ab_cfg["name"].str().c_str()) << "(" << id << ")";
			strcpy(text, strstr.str().c_str());
			lvi.pszText = text;
			lvi.lParam = (LPARAM)0;
			ListView_InsertItem(hctl, &lvi);

			// 描述
			lvi.mask = LVIF_TEXT;
			lvi.iSubItem = 1;
			strstr.str("");
			strstr << utf8_2_ansi(ab_cfg["description"].str().c_str());
			strcpy(text, strstr.str().c_str());
			lvi.pszText = text;
			ListView_SetItem(hctl, &lvi);

			if (abilities_.find(id) != abilities_.end()) {
				ListView_SetCheckState(hctl, lvi.iItem, TRUE);
			} else {
				ListView_SetCheckState(hctl, lvi.iItem, FALSE);
			}
		}
	}
	strstr.str("");
	strstr << "能力(" << editor_config::ListView_GetCheckedCount(hctl) << ")";
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_UTYPEEDIT_ABILITIES), strstr.str().c_str());

	update_to_ui_utype_edit_mtype(hdlgP);

	hctl = GetDlgItem(hdlgP, IDC_LV_UTYPEEDIT_ATTACK);
	ListView_DeleteAllItems(hctl);
	for (std::vector<tattack>::const_iterator it = attacks_.begin(); it != attacks_.end(); ++ it) {
		it->update_to_ui_utype_edit(hdlgP);
	}
}

void tunit_type::update_to_ui_utype_edit_mtype(HWND hdlgP) const
{
	std::stringstream strstr;
	char text[1024];

	HWND hctl = GetDlgItem(hdlgP, IDC_LV_UTYPEEDIT_MTYPE);
	ListView_DeleteAllItems(hctl);
	int index = 0;
	LVITEM lvi;
	for (std::vector<std::string>::const_iterator it = attack_type_vstr.begin(); it != attack_type_vstr.end(); ++ it) {
		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		// 名称
		lvi.iItem = index ++;
		lvi.iSubItem = 0;
		strstr.str("");
		strstr << utf8_2_ansi(gettext(it->c_str()));
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		lvi.lParam = (LPARAM)0;
		ListView_InsertItem(hctl, &lvi);

		// 抗性
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 1;
		strstr.str("");
		std::map<std::string, int>::const_iterator resistance_it = resistance_.find(*it);
		strstr << resistance_it->second;
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);
	}
}

void tunit_type::update_to_ui_utype_edit_type(HWND hdlgP) const
{
	std::stringstream strstr;
	if (type() == TYPE_TROOP) {
		strstr << "移动力: " << movement_ << "\r\n";
		strstr << "特色: ";
		if (character_ != NO_CHARACTER) {
			strstr << utf8_2_ansi(unit_types.character(character_).name_.c_str());
		}
		strstr << "\r\n";
		strstr << "是否能上城墙: " << (land_wall_? "能": "不能") << "\r\n";
		strstr << "君主专属兵种: " << (leader_? "是": "不是");

	} else if (type() == TYPE_CITY) {
		strstr << "回合自增加XP: " << turn_experience_ << "\r\n";
		strstr << "回合自恢复HP: " << heal_;

	} else if (type() == TYPE_ARTIFICAL) {
		strstr << "类型: " << tunit_type::artifical_hero_.find(master_)->second << "\r\n";
		strstr << "回合自恢复HP: " << heal_ << "\r\n";
		strstr << "警戒攻击: ";
		if (guard_ != NO_GUARD) {
			strstr << utf8_2_ansi(dgettext(PACKAGE, attacks_[guard_].id_.c_str()));
		}
		if (master_ == hero::number_market) {
			strstr << "\r\n";
			strstr << "基本收入金: " << income_;
		}
	}

	HWND hctl = GetDlgItem(hdlgP, IDC_ET_UTYPEEDIT_TYPE);
	Edit_SetText(hctl, strstr.str().c_str());
}

void tunit_type::update_to_ui_mtype_edit(HWND hdlgP) const
{
	std::stringstream strstr;
	LVITEM lvi;
	char text[_MAX_PATH];
	HWND hctl = GetDlgItem(hdlgP, IDC_LV_MTYPEEDIT_RESISTANCE);
	ListView_DeleteAllItems(hctl);
	int index = 0;
	// used to default
	const movement_type_map& movement_types = unit_types.movement_types();
	const unit_movement_type& movement_type_default = movement_types.find(movement_type_)->second;
	utils::string_map dam_tab_default = movement_type_default.damage_table();
	for (std::vector<std::string>::const_iterator it = attack_type_vstr.begin(); it != attack_type_vstr.end(); ++ it) {
		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		// 名称
		lvi.iItem = index ++;
		lvi.iSubItem = 0;
		strstr.str("");
		strstr << utf8_2_ansi(gettext(it->c_str()));
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		lvi.lParam = (LPARAM)0;
		ListView_InsertItem(hctl, &lvi);

		// 默认值
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 1;
		strstr.str("");
		utils::string_map::const_iterator dam_it = dam_tab_default.find(*it);
		if (dam_it != dam_tab_default.end()) {
			int resistance = 100 - atoi((*dam_it).second.c_str());
			strstr << resistance;
		} else {
			strstr << "--";
		}
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// 使用值
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 2;
		strstr.str("");
		std::map<std::string, int>::const_iterator resistance_it = resistance_.find(*it);
		strstr << resistance_it->second;
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);
	}

	const config& movement_type_default_cfg = movement_type_default.get_cfg();
	const config& defense_cfg = movement_type_default_cfg.child("defense");
	const config& movement_costs_cfg = movement_type_default_cfg.child("movement_costs");
	ListView_DeleteAllItems(GetDlgItem(hdlgP, IDC_LV_MTYPEEDIT_DEFENSE));
	ListView_DeleteAllItems(GetDlgItem(hdlgP, IDC_LV_MTYPEEDIT_MOVEMENTCOSTS));
	index = 0;
	for (t_translation::t_list::const_iterator it = ns::core.alias_terrains_.begin(); it != ns::core.alias_terrains_.end(); ++ it) {
		const t_translation::t_terrain& terrain = *it;
		if ((terrain == t_translation::OFF_MAP_USER) ||
			(terrain == t_translation::FAKE_MAP_EDGE) ||
			(terrain == t_translation::FOGGED)) {
			continue;
		}
		const terrain_type& info = ns::core.map_->get_terrain_info(terrain);
		hctl = GetDlgItem(hdlgP, IDC_LV_MTYPEEDIT_DEFENSE);

		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		// 名称
		lvi.iItem = index;
		lvi.iSubItem = 0;
		strstr.str("");
		strstr << utf8_2_ansi(info.name().c_str()) << "(" << info.id() << ")";
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		lvi.lParam = (LPARAM)0;
		ListView_InsertItem(hctl, &lvi);

		// 默认值
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 1;

		int value = 100;
		if (const config::attribute_value* val = defense_cfg.get(info.id())) {
			value = *val;
		}
		strstr.str("");
		if (value < 0) {
			strstr << 100 - (-1 * value);
		} else {
			strstr << 100 - value;
		}
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		hctl = GetDlgItem(hdlgP, IDC_LV_MTYPEEDIT_MOVEMENTCOSTS);
		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		// 名称
		lvi.iItem = index ++;
		lvi.iSubItem = 0;
		strstr.str("");
		strstr << utf8_2_ansi(info.name().c_str()) << "(" << info.id() << ")";
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		lvi.lParam = (LPARAM)0;
		ListView_InsertItem(hctl, &lvi);

		// 默认值
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 1;
		value = unit_movement_type::UNREACHABLE;
		if (const config::attribute_value* val = movement_costs_cfg.get(info.id())) {
			value = *val;
		}
		strstr.str("");
		strstr << value;
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);
	}
}

void tunit_type::update_to_ui_utype_type(HWND hdlgP) const
{
}

void tunit_type::vertify_core_images() const
{
	if (!is_file(image(true).c_str())) {
		CopyFile(model_image().c_str(), image(true).c_str(), TRUE);
	}

	if (use_terrain_image()) return;

	if (!is_file(hit(true).c_str())) {
		CopyFile(image(true).c_str(), hit(true).c_str(), TRUE);
	}
	if (!is_file(miss(true).c_str())) {
		CopyFile(image(true).c_str(), miss(true).c_str(), TRUE);
	}
	if (!is_file(leading(true).c_str())) {
		CopyFile(image(true).c_str(), leading(true).c_str(), TRUE);
	}
}

void tunit_type::generate() const
{
	std::stringstream strstr;
	uint32_t bytertd;

	MakeDirectory(dirname(file(true).c_str()));

	posix_file_t fp = INVALID_FILE;
	posix_fopen(file(true).c_str(), GENERIC_WRITE, CREATE_ALWAYS, fp);
	if (fp == INVALID_FILE) {
		return;
	}

	vertify_core_images();

	strstr << "#textdomain " << textdomain_ << "\n";
	strstr << "\n";

	strstr << "[unit_type]\n";
	strstr << "\tid = " << id_ << "\n";
	strstr << "\tname = _\"" << id_ << "\"\n";
	strstr << "\trace = " << race_ << "\n";
	strstr << "\tarms = " << unit_types.arms_ids()[arms_] << "\n";
	strstr << "\tgender = male\n";
	strstr << "\timage = \"" << image(false) << "\"\n";
	strstr << "\t{MAGENTA_IS_THE_TEAM_COLOR}\n";
	strstr << "\talignment = " << unit_type::alignment_id((unit_type::ALIGNMENT)(alignment_)) << "\n";
	strstr << "\tmovement_type = " << movement_type_ << "\n";
	// [resistance]
	std::map<std::string, int> diff;
	const movement_type_map& movement_types = unit_types.movement_types();
	const unit_movement_type& movement_type_default = movement_types.find(movement_type_)->second;
	utils::string_map dam_tab_default = movement_type_default.damage_table();
	for (std::vector<std::string>::const_iterator it = attack_type_vstr.begin(); it != attack_type_vstr.end(); ++ it) {
		utils::string_map::const_iterator dam_it = dam_tab_default.find(*it);
		int resistance = 100 - atoi((*dam_it).second.c_str());
		std::map<std::string, int>::const_iterator resistance_it = resistance_.find(*it);
		if (resistance != resistance_it->second) {
			diff[*it] = 100 - resistance_it->second;
		}
	}
	if (!diff.empty()) {
		strstr << "\t[resistance]\n";
		for (std::map<std::string, int>::const_iterator it = diff.begin(); it != diff.end(); ++ it) {
			strstr << "\t\t" << it->first << " = " << it->second << "\n";
		}
		strstr << "\t[/resistance]\n";
	}
	strstr << "\tzoc = " << (zoc_? "yes": "no") << "\n";

	if (!packer_) {
		strstr << "\thitpoints = " << hitpoints_ << "\n";
		strstr << "\texperience = " << experience_needed_ << "\n";
		strstr << "\tcost = " << cost_ << "\n";
		strstr << "\tlevel = " << level_ << "\n";
				
		if (type() == TYPE_TROOP) {
			strstr << "\tmovement = " << movement_ << "\n";
			if (character_ != NO_CHARACTER) {
				strstr << "\tcharacter = " << unit_types.character(character_).id_ << "\n";
			}
			strstr << "\tland_wall = " << (land_wall_? "yes": "no") << "\n";
			if (leader_) {
				strstr << "\tleader = yes\n";
			}
		} else if (type() == TYPE_CITY) {
			strstr << "\tcan_recruit = yes\n";
			strstr << "\tcan_reside = yes\n";
			strstr << "\tturn_experience = " << turn_experience_ << "\n";
			strstr << "\theal = " << heal_ << "\n";
		} else if (type() == TYPE_ARTIFICAL) {
			strstr << "\tmovement = 0\n";
			strstr << "\tmaster = " << master_ << "\n";
			strstr << "\tattack_destroy = yes" << "\n";
			if (guard_ != NO_GUARD) {
				strstr << "\tguard = " << guard_ << "\n";
			}
			if (master_ == hero::number_wall) {
				strstr << "\tmatch = G*^*,R*^*,S*^*,H*^*" << "\n";
				strstr << "\tterrain = Ch" << "\n";
				strstr << "\tbase = yes" << "\n";
				strstr << "\twall = yes" << "\n";
				strstr << "\twalk_wall = yes" << "\n";
			} else if (master_ == hero::number_keep) {
				strstr << "\tmatch = G*^*,R*^*,S*^*,H*^*" << "\n";
				strstr << "\tterrain = Kud" << "\n";
				strstr << "\twalk_wall = yes" << "\n";
				strstr << "\tcancel_zoc = yes" << "\n";
			} else if (master_ == hero::number_market) {
				strstr << "\tmatch = Ea" << "\n";
				strstr << "\tgold_income = " << income_ << "\n";
			} else if (master_ == hero::number_tower) {
				strstr << "\tmatch = G*^*,R*^*" << "\n";
			}

			strstr << "\theal = " << heal_ << "\n";
		}

		if (has_halo()) {
			strstr << "\thalo = halo/illuminates-aura.png\n";
		}
		strstr << "\tadvances_to = ";
		if (advances_to_.empty()) {
			strstr << "null";
		} else {
			for (std::set<std::string>::const_iterator it = advances_to_.begin(); it != advances_to_.end(); ++ it) {
				if (it == advances_to_.begin()) {
					strstr << *it;
				} else {
					strstr << ", " << *it;
				}
			}
		}
		strstr << "\n";
		if (advances_to_.empty()) {
			const modifications_map& modifications = unit_types.modifications();
			if (modifications.empty()) {
				strstr << "\tadvancement = \n";
			} else {
				strstr << "\tadvancement = " << modifications.begin()->first << "\n";
			}
		}
		if (!abilities_.empty()) {
			strstr << "\tabilities = ";
			for (std::set<std::string>::const_iterator it = abilities_.begin(); it != abilities_.end(); ++ it) {
				if (it == abilities_.begin()) {
					strstr << *it;
				} else {
					strstr << ", " << *it;
				}
			}
			strstr << "\n";
		}
	} else {
		strstr << "\tpacker = yes\n";
		
		strstr << "\t[pack]\n";

		strstr << "\t\t[effect]\n";
		strstr << "\t\t\tapply_to = attack\n";
		strstr << "\t\t\trange = melee\n";
		strstr << "\t\t\tincrease_damage = " << melee_increase_ << "\n";
		strstr << "\t\t[/effect]\n";

		strstr << "\t\t[effect]\n";
		strstr << "\t\t\tapply_to = attack\n";
		strstr << "\t\t\trange = ranged\n";
		strstr << "\t\t\tincrease_damage = " << ranged_increase_ << "\n";
		strstr << "\t\t[/effect]\n";

		strstr << "\t\t[effect]\n";
		strstr << "\t\t\tapply_to = attack\n";
		strstr << "\t\t\trange = cast\n";
		strstr << "\t\t\tincrease_damage = " << cast_increase_ << "\n";
		strstr << "\t\t[/effect]\n";

		strstr << "\t[/pack]\n";
	}

	strstr << "\tdescription = _\"" << description() << "\"\n";
	strstr << "\tdie_sound = {SOUND_LIST:HUMAN_DIE}\n";

	if (!use_terrain_image()) {
		strstr << "\t{DEFENSE_ANIM_ALLRANGE \"" << hit(false);
		strstr << "\" \"" << miss(false) << "\" \"" << image(false) << "\" ";
		strstr << "{SOUND_LIST:HUMAN_HIT}}\n";
	}

	if (has_resistance_anim()) {
		resistance_anim(strstr, "\t");
	}
	if (has_leading_anim()) {
		leading_anim(strstr, "\t");
	}
	if (has_healing_anim()) {
		healing_anim(strstr, "\t");
	}
	idle_anim(strstr, "\t");

	if (!packer_) {
		for (std::vector<tattack>::const_iterator it = attacks_.begin(); it != attacks_.end(); ++ it) {
			it->generate(strstr, "\t");
			if (it->range_ == 0) {
				attack_anim_melee(it->id_, it->icon_, strstr, "\t");

			} else if (it->icon_.find("magic-missile") != std::string::npos) {
				attack_anim_ranged_magic_missile(it->id_, strstr, "\t");

			} else if (it->icon_.find("lightbeam") != std::string::npos) {
				attack_anim_ranged_lightbeam(it->id_, strstr, "\t");

			} else if (it->icon_.find("fireball") != std::string::npos) {
				attack_anim_ranged_fireball(it->id_, strstr, "\t");

			} else if (it->icon_.find("iceball") != std::string::npos) {
				attack_anim_ranged_iceball(it->id_, strstr, "\t");

			} else if (it->icon_.find("lightning") != std::string::npos) {
				attack_anim_ranged_lightning(it->id_, strstr, "\t");

			} else {
				attack_anim_ranged(it->id_, it->icon_, strstr, "\t");
			}
		}
	} else {
		attack_anim_melee("melee", "staff-magic.png", strstr, "\t");
		attack_anim_ranged_magic_missile("ranged", strstr, "\t");
		attack_anim_ranged("cast", "sling.png", strstr, "\t");
	}

	strstr << "[/unit_type]\n";

	posix_fwrite(fp, strstr.str().c_str(), strstr.str().length(), bytertd);
	posix_fclose(fp);
}

bool tunit_type::has_halo() const
{
	for (std::set<std::string>::const_iterator it = abilities_.begin(); it != abilities_.end(); ++ it) {
		if (it->find("illumination") != std::string::npos) {
			return true;
		}
	}
	return false;
}

bool tunit_type::has_healing_anim() const
{
	for (std::set<std::string>::const_iterator it = abilities_.begin(); it != abilities_.end(); ++ it) {
		if (it->find("heals") != std::string::npos) {
			return true;
		}
		if (it->find("curing") != std::string::npos) {
			return true;
		}
	}
	return false;
}

bool tunit_type::has_resistance_anim() const
{
	for (std::set<std::string>::const_iterator it = abilities_.begin(); it != abilities_.end(); ++ it) {
		if (it->find("firm") != std::string::npos) {
			return true;
		}
	}
	return false;
}

bool tunit_type::has_leading_anim() const
{
	for (std::set<std::string>::const_iterator it = abilities_.begin(); it != abilities_.end(); ++ it) {
		if (it->find("leadership") != std::string::npos) {
			return true;
		}
		if (it->find("encourage") != std::string::npos) {
			return true;
		}
	}
	return false;
}

void tunit_type::vertify_healing_anim_images() const
{
	if (use_terrain_image()) return;

	int attack_ranged_count = 4;
	for (int index = 1; index <= attack_ranged_count; index ++) {
		if (!is_file(attack_image(true, 1, index).c_str())) {
			CopyFile(image(true).c_str(), attack_image(true, 1, index).c_str(), TRUE);
		}
	}
}

void tunit_type::healing_anim(std::stringstream& strstr, const std::string& prefix) const
{
	vertify_healing_anim_images();

	strstr << prefix << "[healing_anim]\n";

	strstr << prefix << "\tstart_time = -525\n";
	strstr << prefix << "\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 75\n";
	strstr << prefix << "\t\timage = \"" << attack_image(false, 1, 1) << "\"\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 75\n";
	strstr << prefix << "\t\timage = \"" << attack_image(false, 1, 2) << "\"\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 75\n";
	strstr << prefix << "\t\timage = \"" << attack_image(false, 1, 3) << "\"\n";
	strstr << prefix << "\t\thalo = haol/holy/halo6.png\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 75\n";
	strstr << prefix << "\t\timage = \"" << attack_image(false, 1, 4) << "\"\n";
	strstr << prefix << "\t\thalo = haol/holy/halo1.png\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 75\n";
	strstr << prefix << "\t\timage = \"" << attack_image(false, 1, 4) << "\"\n";
	strstr << prefix << "\t\thalo = haol/holy/halo2.png\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 75\n";
	strstr << prefix << "\t\timage = \"" << attack_image(false, 1, 4) << "\"\n";
	strstr << prefix << "\t\thalo = haol/holy/halo3.png\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 75\n";
	strstr << prefix << "\t\timage = \"" << attack_image(false, 1, 4) << "\"\n";
	strstr << prefix << "\t\thalo = haol/holy/halo4.png\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 75\n";
	strstr << prefix << "\t\timage = \"" << attack_image(false, 1, 4) << "\"\n";
	strstr << prefix << "\t\thalo = haol/holy/halo5.png\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 75\n";
	strstr << prefix << "\t\timage = \"" << attack_image(false, 1, 3) << "\"\n";
	strstr << prefix << "\t\thalo = haol/holy/halo6.png\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 75\n";
	strstr << prefix << "\t\timage = \"" << attack_image(false, 1, 2) << "\"\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 75\n";
	strstr << prefix << "\t\timage = \"" << attack_image(false, 1, 1) << "\"\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "[/healing_anim]\n";
}

void tunit_type::vertify_resistance_anim_images() const
{
	if (use_terrain_image()) return;

	if (!is_file(leading(true).c_str())) {
		CopyFile(image(true).c_str(), leading(true).c_str(), TRUE);
	}
}

void tunit_type::resistance_anim(std::stringstream& strstr, const std::string& prefix) const
{
	vertify_resistance_anim_images();

	strstr << prefix << "[resistance_anim]\n";

	strstr << prefix << "\tstart_time = -150\n";
	strstr << prefix << "\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 300\n";
	strstr << prefix << "\t\timage = \"" << leading(false) << "\"\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "[/resistance_anim]\n";
}

void tunit_type::vertify_leading_anim_images() const
{
	if (use_terrain_image()) return;

	if (!is_file(leading(true).c_str())) {
		CopyFile(image(true).c_str(), leading(true).c_str(), TRUE);
	}
}

void tunit_type::leading_anim(std::stringstream& strstr, const std::string& prefix) const
{
	vertify_leading_anim_images();

	strstr << prefix << "[leading_anim]\n";

	strstr << prefix << "\tstart_time = -150\n";
	strstr << prefix << "\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 300\n";
	strstr << prefix << "\t\timage = \"" << leading(false) << "\"\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "[/leading_anim]\n";
}

void tunit_type::vertify_idle_anim_images() const
{
	if (use_terrain_image()) return;

	int idle_count = 4;
	for (int index = 1; index <= idle_count; index ++) {
		if (!is_file(idle(true, index).c_str())) {
			CopyFile(image(true).c_str(), idle(true, index).c_str(), TRUE);
		}
	}
}

void tunit_type::idle_anim(std::stringstream& strstr, const std::string& prefix) const
{
	vertify_idle_anim_images();

	strstr << prefix << "[idle_anim]\n";

	strstr << prefix << "\tstart_time = 0\n";
	strstr << prefix << "\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 100\n";
	strstr << prefix << "\t\timage = \"" << idle(false, 1) << "\"\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 100\n";
	strstr << prefix << "\t\timage = \"" << idle(false, 2) << "\"\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 100\n";
	strstr << prefix << "\t\timage = \"" << idle(false, 3) << "\"\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 200\n";
	strstr << prefix << "\t\timage = \"" << idle(false, 4) << "\"\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 100\n";
	strstr << prefix << "\t\timage = \"" << idle(false, 3) << "\"\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 100\n";
	strstr << prefix << "\t\timage = \"" << idle(false, 2) << "\"\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 100\n";
	strstr << prefix << "\t\timage = \"" << idle(false, 3) << "\"\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 200\n";
	strstr << prefix << "\t\timage = \"" << idle(false, 4) << "\"\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 100\n";
	strstr << prefix << "\t\timage = \"" << idle(false, 3) << "\"\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 100\n";
	strstr << prefix << "\t\timage = \"" << idle(false, 2) << "\"\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 100\n";
	strstr << prefix << "\t\timage = \"" << idle(false, 1) << "\"\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "[/idle_anim]\n";
}

void tunit_type::vertify_attack_anim_melee_images() const
{
	if (use_terrain_image()) return;

	int attack_melee_count = 4;
	for (int index = 1; index <= attack_melee_count; index ++) {
		if (!is_file(attack_image(true, 0, index).c_str())) {
			CopyFile(image(true).c_str(), attack_image(true, 0, index).c_str(), TRUE);
		}
	}
}

void tunit_type::attack_anim_melee(const std::string& aid, const std::string& aicon, std::stringstream& strstr, const std::string& prefix) const
{
	std::string hit_sound = "{SOUND_LIST:SWORD_SWISH}";
	std::string miss_sound = "{SOUND_LIST:MISS}";
	if (aicon.find("staff") != std::string::npos) {
		hit_sound = "staff.wav";
	}

	vertify_attack_anim_melee_images();

	strstr << prefix << "[attack_anim]\n";

	strstr << prefix << "\t[filter_attack]\n";
	strstr << prefix << "\t\tname = " << aid << "\n";
	strstr << prefix << "\t[/filter_attack]\n";
	
	strstr << prefix << "\tstart_time = -275\n";
	if (type() != TYPE_TROOP) {
		strstr << prefix << "\toffset = 0\n";
	}
	strstr << prefix << "\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 50\n";
	strstr << prefix << "\t\timage = \"" << attack_image(false, 0, 1) << "\"\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 100\n";
	strstr << prefix << "\t\timage = \"" << attack_image(false, 0, 1) << "\"\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "\t[if]\n";
	strstr << prefix << "\t\thits = yes\n";
	strstr << prefix << "\t\t[frame]\n";
	strstr << prefix << "\t\t\tduration = 100\n";
	strstr << prefix << "\t\t\timage = \"" << attack_image(false, 0, 2) << "\"\n";
	strstr << prefix << "\t\t\tsound = " << hit_sound << "\n";
	strstr << prefix << "\t\t[/frame]\n";
	strstr << prefix << "\t[/if]\n";

	strstr << prefix << "\t[else]\n";
	strstr << prefix << "\t\thits = no\n";
	strstr << prefix << "\t\t[frame]\n";
	strstr << prefix << "\t\t\tduration = 100\n";
	strstr << prefix << "\t\t\timage = \"" << attack_image(false, 0, 2) << "\"\n";
	strstr << prefix << "\t\t\tsound = " << miss_sound << "\n";
	strstr << prefix << "\t\t[/frame]\n";
	strstr << prefix << "\t[/else]\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 100\n";
	strstr << prefix << "\t\timage = \"" << attack_image(false, 0, 3) << "\"\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 100\n";
	strstr << prefix << "\t\timage = \"" << attack_image(false, 0, 4) << "\"\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 50\n";
	strstr << prefix << "\t\timage = \"" << attack_image(false, 0, 1) << "\"\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "[/attack_anim]\n";
}

void tunit_type::vertify_attack_anim_ranged_images() const
{
	if (use_terrain_image()) return;

	int attack_ranged_count = 4;
	for (int index = 1; index <= attack_ranged_count; index ++) {
		if (!is_file(attack_image(true, 1, index).c_str())) {
			CopyFile(image(true).c_str(), attack_image(true, 1, index).c_str(), TRUE);
		}
	}
}

void tunit_type::attack_anim_ranged(const std::string& aid, const std::string& aicon, std::stringstream& strstr, const std::string& prefix) const
{
	std::string image = "projectiles/missile-n.png";
	std::string image_diagonal = "projectiles/missile-ne.png";
	std::string hit_sound = "bow.ogg";
	std::string miss_sound = "bow-miss.ogg";
	if (aicon.find("sling") != std::string::npos) {
		image = "projectiles/stone-large.png";
		image_diagonal = "";
	}

	vertify_attack_anim_ranged_images();

	strstr << prefix << "[attack_anim]\n";

	strstr << prefix << "\t[filter_attack]\n";
	strstr << prefix << "\t\tname = " << aid << "\n";
	strstr << prefix << "\t[/filter_attack]\n";

	strstr << prefix << "\tstart_time = -445\n";
	strstr << prefix << "\tmissile_start_time = -150\n";
	strstr << prefix << "\n";

	strstr << prefix << "\t[missile_frame]\n";
	strstr << prefix << "\t\tduration = 150\n";
	strstr << prefix << "\t\timage = " << image << "\n";
	if (!image_diagonal.empty()) {
		strstr << prefix << "\t\timage_diagonal = " << image_diagonal << "\n";
	}
	strstr << prefix << "\t[/missile_frame]\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 75\n";
	strstr << prefix << "\t\timage = \"" << attack_image(false, 1, 1) << "\"\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 75\n";
	strstr << prefix << "\t\timage = \"" << attack_image(false, 1, 2) << "\"\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "\t[if]\n";
	strstr << prefix << "\t\thits = yes\n";
	strstr << prefix << "\t\t[frame]\n";
	strstr << prefix << "\t\t\tduration = 100\n";
	strstr << prefix << "\t\t\timage = \"" << attack_image(false, 1, 3) << "\"\n";
	strstr << prefix << "\t\t\tsound = " << hit_sound << "\n";
	strstr << prefix << "\t\t[/frame]\n";
	strstr << prefix << "\t[/if]\n";

	strstr << prefix << "\t[else]\n";
	strstr << prefix << "\t\thits = no\n";
	strstr << prefix << "\t\t[frame]\n";
	strstr << prefix << "\t\t\tduration = 100\n";
	strstr << prefix << "\t\t\timage = \"" << attack_image(false, 1, 3) << "\"\n";
	strstr << prefix << "\t\t\tsound = " << miss_sound << "\n";
	strstr << prefix << "\t\t[/frame]\n";
	strstr << prefix << "\t[/else]\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 130\n";
	strstr << prefix << "\t\timage = \"" << attack_image(false, 1, 4) << "\"\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 65\n";
	strstr << prefix << "\t\timage = \"" << attack_image(false, 1, 1) << "\"\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "[/attack_anim]\n";
}

void tunit_type::attack_anim_ranged_magic_missile(const std::string& aid, std::stringstream& strstr, const std::string& prefix) const
{
	vertify_attack_anim_ranged_images();

	strstr << prefix << "[attack_anim]\n";

	strstr << prefix << "\t[filter_attack]\n";
	strstr << prefix << "\t\tname = " << aid << "\n";
	strstr << prefix << "\t[/filter_attack]\n";

	strstr << prefix << "\tstart_time = -445\n";
	strstr << prefix << "\toffset = 0\n";
	strstr << prefix << "\t{MAGIC_MISSILE 11 -20}\n";
	strstr << prefix << "\t{MAGIC_MISSILE_STAFF_FLARE -750 600 11 -20}\n";
	strstr << prefix << "\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 75\n";
	strstr << prefix << "\t\timage = \"" << attack_image(false, 1, 1) << "\"\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 75\n";
	strstr << prefix << "\t\timage = \"" << attack_image(false, 1, 2) << "\"\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "\t[if]\n";
	strstr << prefix << "\t\thits = yes\n";
	strstr << prefix << "\t\t[frame]\n";
	strstr << prefix << "\t\t\tduration = 100\n";
	strstr << prefix << "\t\t\timage = \"" << attack_image(false, 1, 3) << "\"\n";
	strstr << prefix << "\t\t\tsound = magic-missile-1.ogg,magic-missile-2.ogg,magic-missile-3.ogg\n";
	strstr << prefix << "\t\t[/frame]\n";
	strstr << prefix << "\t[/if]\n";

	strstr << prefix << "\t[else]\n";
	strstr << prefix << "\t\thits = no\n";
	strstr << prefix << "\t\t[frame]\n";
	strstr << prefix << "\t\t\tduration = 100\n";
	strstr << prefix << "\t\t\timage = \"" << attack_image(false, 1, 3) << "\"\n";
	strstr << prefix << "\t\t\tsound = magic-missile-1-miss.ogg,magic-missile-2-miss.ogg,magic-missile-3-miss.ogg\n";
	strstr << prefix << "\t\t[/frame]\n";
	strstr << prefix << "\t[/else]\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 130\n";
	strstr << prefix << "\t\timage = \"" << attack_image(false, 1, 4) << "\"\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 65\n";
	strstr << prefix << "\t\timage = \"" << attack_image(false, 1, 1) << "\"\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "[/attack_anim]\n";
}

void tunit_type::attack_anim_ranged_lightbeam(const std::string& aid, std::stringstream& strstr, const std::string& prefix) const
{
	vertify_attack_anim_ranged_images();

	strstr << prefix << "[attack_anim]\n";

	strstr << prefix << "\t[filter_attack]\n";
	strstr << prefix << "\t\tname = " << aid << "\n";
	strstr << prefix << "\t[/filter_attack]\n";

	strstr << prefix << "\tstart_time = -445\n";
	strstr << prefix << "\t{MISSILE_FRAME_LIGHT_BEAM}\n";
	strstr << prefix << "\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 75\n";
	strstr << prefix << "\t\timage = \"" << attack_image(false, 1, 1) << "\"\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 75\n";
	strstr << prefix << "\t\timage = \"" << attack_image(false, 1, 2) << "\"\n";
	strstr << prefix << "\t\thalo = halo/holy/halo6.png\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "\t[if]\n";
	strstr << prefix << "\t\thits = yes\n";
	strstr << prefix << "\t\t[frame]\n";
	strstr << prefix << "\t\t\tduration = 100\n";
	strstr << prefix << "\t\t\timage = \"" << attack_image(false, 1, 3) << "\"\n";
	strstr << prefix << "\t\t\tsound = {SOUND_LIST:HOLY}\n";
	strstr << prefix << "\t\t\thalo = halo/holy/halo1.png\n";
	strstr << prefix << "\t\t[/frame]\n";
	strstr << prefix << "\t[/if]\n";

	strstr << prefix << "\t[else]\n";
	strstr << prefix << "\t\thits = no\n";
	strstr << prefix << "\t\t[frame]\n";
	strstr << prefix << "\t\t\tduration = 100\n";
	strstr << prefix << "\t\t\timage = \"" << attack_image(false, 1, 3) << "\"\n";
	strstr << prefix << "\t\t\tsound = {SOUND_LIST:HOLY}\n";
	strstr << prefix << "\t\t\thalo = halo/holy/halo1.png\n";
	strstr << prefix << "\t\t[/frame]\n";
	strstr << prefix << "\t[/else]\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 130\n";
	strstr << prefix << "\t\timage = \"" << attack_image(false, 1, 4) << "\"\n";
	strstr << prefix << "\t\thalo = halo/holy/halo3.png\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 65\n";
	strstr << prefix << "\t\timage = \"" << attack_image(false, 1, 1) << "\"\n";
	strstr << prefix << "\t\thalo = halo/holy/halo5.png\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "[/attack_anim]\n";
}

void tunit_type::attack_anim_ranged_fireball(const std::string& aid, std::stringstream& strstr, const std::string& prefix) const
{
	vertify_attack_anim_ranged_images();

	strstr << prefix << "[attack_anim]\n";

	strstr << prefix << "\t[filter_attack]\n";
	strstr << prefix << "\t\tname = " << aid << "\n";
	strstr << prefix << "\t[/filter_attack]\n";

	strstr << prefix << "\tstart_time = -575\n";
	strstr << prefix << "\t{MISSILE_FRAME_FIREBALL}\n";
	strstr << prefix << "\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 75\n";
	strstr << prefix << "\t\timage = \"" << attack_image(false, 1, 1) << "\"\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 75\n";
	strstr << prefix << "\t\timage = \"" << attack_image(false, 1, 2) << "\"\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 100\n";
	strstr << prefix << "\t\timage = \"" << attack_image(false, 1, 3) << "\"\n";
	strstr << prefix << "\t\tsound = fire.wav\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 130\n";
	strstr << prefix << "\t\timage = \"" << attack_image(false, 1, 4) << "\"\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 65\n";
	strstr << prefix << "\t\timage = \"" << attack_image(false, 1, 1) << "\"\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "[/attack_anim]\n";
}

void tunit_type::attack_anim_ranged_iceball(const std::string& aid, std::stringstream& strstr, const std::string& prefix) const
{
	vertify_attack_anim_ranged_images();

	strstr << prefix << "[attack_anim]\n";

	strstr << prefix << "\t[filter_attack]\n";
	strstr << prefix << "\t\tname = " << aid << "\n";
	strstr << prefix << "\t[/filter_attack]\n";

	strstr << prefix << "\tstart_time = -450\n";
	strstr << prefix << "\t{MISSILE_FRAME_ICEBALL}\n";
	strstr << prefix << "\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 55\n";
	strstr << prefix << "\t\timage = \"" << attack_image(false, 1, 1) << "\"\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 75\n";
	strstr << prefix << "\t\timage = \"" << attack_image(false, 1, 2) << "\"\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 20\n";
	strstr << prefix << "\t\timage = \"" << attack_image(false, 1, 3) << "\"\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 50\n";
	strstr << prefix << "\t\timage = \"" << attack_image(false, 1, 3) << "\"\n";
	strstr << prefix << "\t\thalo = halo/undead/dark-magic-1.png\n";
	strstr << prefix << "\t\thalo_x,halo_y = 0,-12\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 50\n";
	strstr << prefix << "\t\timage = \"" << attack_image(false, 1, 3) << "\"\n";
	strstr << prefix << "\t\thalo = halo/undead/dark-magic-2.png\n";
	strstr << prefix << "\t\thalo_x,halo_y = 0,-12\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 50\n";
	strstr << prefix << "\t\timage = \"" << attack_image(false, 1, 3) << "\"\n";
	strstr << prefix << "\t\thalo = halo/undead/dark-magic-3.png\n";
	strstr << prefix << "\t\thalo_x,halo_y = 0,-12\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 50\n";
	strstr << prefix << "\t\timage = \"" << attack_image(false, 1, 3) << "\"\n";
	strstr << prefix << "\t\thalo = halo/undead/dark-magic-4.png\n";
	strstr << prefix << "\t\thalo_x,halo_y = 0,-12\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 50\n";
	strstr << prefix << "\t\timage = \"" << attack_image(false, 1, 3) << "\"\n";
	strstr << prefix << "\t\thalo = halo/undead/dark-magic-5.png\n";
	strstr << prefix << "\t\thalo_x,halo_y = 0,-12\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "\t[if]\n";
	strstr << prefix << "\t\thits = yes\n";
	strstr << prefix << "\t\t[frame]\n";
	strstr << prefix << "\t\t\tduration = 50\n";
	strstr << prefix << "\t\t\timage = \"" << attack_image(false, 1, 3) << "\"\n";
	strstr << prefix << "\t\t\thalo = halo/undead/dark-magic-6.png\n";
	strstr << prefix << "\t\t\thalo_x,halo_y = 0,-12\n";
	strstr << prefix << "\t\t\tsound = magic-dark.ogg\n";
	strstr << prefix << "\t\t[/frame]\n";
	strstr << prefix << "\t[/if]\n";

	strstr << prefix << "\t[else]\n";
	strstr << prefix << "\t\thits = no\n";
	strstr << prefix << "\t\t[frame]\n";
	strstr << prefix << "\t\t\tduration = 100\n";
	strstr << prefix << "\t\t\timage = \"" << attack_image(false, 1, 3) << "\"\n";
	strstr << prefix << "\t\t\thalo_x,halo_y = 0,-12\n";
	strstr << prefix << "\t\t\thalo = halo/undead/dark-magic-6.png\n";
	strstr << prefix << "\t\t\tsound = magic-dark-miss.ogg\n";
	strstr << prefix << "\t\t[/frame]\n";
	strstr << prefix << "\t[/else]\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 50\n";
	strstr << prefix << "\t\timage = \"" << attack_image(false, 1, 4) << "\"\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 40\n";
	strstr << prefix << "\t\timage = \"" << attack_image(false, 1, 2) << "\"\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "\t[frame]\n";
	strstr << prefix << "\t\tduration = 10\n";
	strstr << prefix << "\t\timage = \"" << attack_image(false, 1, 1) << "\"\n";
	strstr << prefix << "\t[/frame]\n";

	strstr << prefix << "[/attack_anim]\n";
}

void tunit_type::attack_anim_ranged_lightning(const std::string& aid, std::stringstream& strstr, const std::string& prefix) const
{
	// vertify_attack_anim_ranged_images();

	strstr << "#define DUDU_LIGHTNING DIRECTION_NUMBER\n";
	strstr << prefix << "[attack_anim]\n";

	strstr << prefix << "\t[filter_attack]\n";
	strstr << prefix << "\t\tname = " << aid << "\n";
	strstr << prefix << "\t[/filter_attack]\n";

	strstr << prefix << "\t{LIGHTNING_BOLT {DIRECTION_NUMBER} }\n";
	strstr << prefix << "\n";

	strstr << prefix << "[/attack_anim]\n";
	strstr << "#enddef\n";

	strstr << prefix << "{DUDU_LIGHTNING 1}\n";
	strstr << prefix << "{DUDU_LIGHTNING 2}\n";
	strstr << prefix << "{DUDU_LIGHTNING 3}\n";

	strstr << "#undef DUDU_LIGHTNING\n";
}

bool tunit_type::new_attack()
{
	std::stringstream new_id;
	new_id << ns::default_attack_id;
	for (std::vector<tattack>::const_iterator it = attacks_.begin(); it != attacks_.end(); ++ it) {
		if (it->id_ == new_id.str()) {
			new_id << "_";
			it = attacks_.begin();
			continue;
		}
	}

	attacks_.push_back(tattack());
	tattack& attack = attacks_.back();

	attack.id_ = new_id.str();
	attack.type_ = attack_type_vstr[0];
	attack.icon_ = ns::default_attack_icon;

	return true;
}

void tunit_type::erase_attack(int index, HWND hdlgP)
{
	attacks_.erase(attacks_.begin() + index);

	if (hdlgP) {
		update_to_ui_utype_edit(hdlgP);
	}
}

std::string tunit_type::file(bool absolute) const
{
	std::stringstream strstr;
	if (absolute) {
		strstr << game_config::path << "\\data\\core\\units\\";
		strstr << race_ << "\\";
		strstr << id_ << ".cfg";
	} else {
		strstr << "data/core/units";
		strstr << race_ << "/";
		strstr << id_ << ".cfg";
	}
	return strstr.str();
}

std::string tunit_type::cfg_file(bool absolute) const
{
	std::stringstream strstr;
	if (absolute) {
		strstr << game_config::path << "\\data\\core\\units\\";
		strstr << utype_from_cfg_.race_ << "\\";
		strstr << utype_from_cfg_.id_ << ".cfg";
	} else {
		strstr << "data/core/units";
		strstr << utype_from_cfg_.race_ << "/";
		strstr << utype_from_cfg_.id_ << ".cfg";
	}
	return strstr.str();
}

bool tunit_type::use_terrain_image() const
{
	return (master_ == hero::number_keep || master_ == hero::number_wall)? true: false;
}

std::string tunit_type::model_image() const
{
	return game_config::path + "\\data\\core\\images\\units\\utype_model.png";
}

std::string tunit_type::image(bool absolute) const
{
	std::stringstream strstr;
	if (absolute) {
		strstr << game_config::path << "\\data\\core\\images\\units\\";
		strstr << race_ << "\\";
		strstr << id_ << ".png";
	} else {
		strstr << "units/";
		strstr << race_ << "/";
		strstr << id_ << ".png";
	}
	return strstr.str();
}

std::string tunit_type::hit(bool absolute) const
{
	if (use_terrain_image()) return "";

	std::stringstream strstr;
	if (absolute) {
		strstr << game_config::path << "\\data\\core\\images\\units\\";
		strstr << race_ << "\\";
		strstr << id_ << "-defend-hit.png";
	} else {
		strstr << "units/";
		strstr << race_ << "/";
		strstr << id_ << "-defend-hit.png";
	}
	return strstr.str();
}

std::string tunit_type::miss(bool absolute) const
{
	if (use_terrain_image()) return "";

	std::stringstream strstr;
	if (absolute) {
		strstr << game_config::path << "\\data\\core\\images\\units\\";
		strstr << race_ << "\\";
		strstr << id_ << "-defend-miss.png";
	} else {
		strstr << "units/";
		strstr << race_ << "/";
		strstr << id_ << "-defend-miss.png";
	}
	return strstr.str();
}

std::string tunit_type::leading(bool absolute) const
{
	if (use_terrain_image()) return "";

	std::stringstream strstr;
	if (absolute) {
		strstr << game_config::path << "\\data\\core\\images\\units\\";
		strstr << race_ << "\\";
		strstr << id_ << "-leading.png";
	} else {
		strstr << "units/";
		strstr << race_ << "/";
		strstr << id_ << "-leading.png";
	}
	return strstr.str();
}

std::string tunit_type::idle(bool absolute, int number) const
{
	if (use_terrain_image()) return "";

	std::stringstream strstr;
	if (absolute) {
		strstr << game_config::path << "\\data\\core\\images\\units\\";
		strstr << race_ << "\\";
		strstr << id_ << "-" << "idle" << "-";
		strstr << number << ".png";
	} else {
		strstr << "units/";
		strstr << race_ << "/";
		strstr << id_ << "-" << "idle" << "-";
		strstr << number << ".png";
	}
	return strstr.str();
}

std::string tunit_type::attack_image(bool absolute, int range, int number) const
{
	if (use_terrain_image()) return "";

	std::stringstream strstr;
	if (absolute) {
		strstr << game_config::path << "\\data\\core\\images\\units\\";
		strstr << race_ << "\\";
		strstr << id_ << "-" << attack_range_vstr[range] << "-";
		strstr << "attack-" << number << ".png";
	} else {
		strstr << "units/";
		strstr << race_ << "/";
		strstr << id_ << "-" << attack_range_vstr[range] << "-";
		strstr << "attack-" << number << ".png";
	}
	return strstr.str();
}

std::string tunit_type::description() const
{
	return id_ + " description";
}

int tunit_type::type() const
{
	if (master_ != HEROS_INVALID_NUMBER) {
		return TYPE_ARTIFICAL;
	} else if (can_recruit_) {
		return TYPE_CITY;
	} else {
		return TYPE_TROOP;
	}
}

int tunit_type::type_from_cfg() const
{
	if (utype_from_cfg_.master_ != HEROS_INVALID_NUMBER) {
		return TYPE_ARTIFICAL;
	} else if (utype_from_cfg_.can_recruit_) {
		return TYPE_CITY;
	} else {
		return TYPE_TROOP;
	}
}

bool tunit_type::dirty() const
{
	if (*this != utype_from_cfg_) {
		return true;
	}

	if (attacks_.size() != attacks_from_cfg_.size()) {
		return true;
	}
	for (size_t i = 0; i < attacks_.size(); i ++) {
		if (attacks_[i] != attacks_from_cfg_[i]) {
			return true;
		}
	}
	return false;
}

void tunit_type::tattack::from_config(const attack_type& attack)
{
	id_ = attack.id();
	type_ = attack.type();
	const std::string& str = attack.icon();
	icon_ = str.substr(str.rfind("/") + 1);
	std::vector<std::string>::const_iterator find_it = std::find(attack_range_vstr.begin(), attack_range_vstr.end(), attack.range());
	if (find_it != attack_range_vstr.end()) {
		range_ = find_it - attack_range_vstr.begin();
	} else {
		range_ = -1;
	}

	specials_.clear();
	std::vector<std::string> vstr = utils::split(attack.specials());
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		specials_.insert(*it);
	}

	damage_ = attack.damage();
	number_ = attack.num_attacks();
}

void tunit_type::tattack::from_ui(HWND hdlgP)
{
	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_ATTACKEDIT_TYPE);
	type_ = attack_type_vstr[ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl))];

	hctl = GetDlgItem(hdlgP, IDC_CMB_ATTACKEDIT_RANGE);
	range_ = ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));

	damage_ = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_ATTACKEDIT_DAMAGE));
	number_ = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_ATTACKEDIT_NUMBER));

	specials_.clear();
	hctl = GetDlgItem(hdlgP, IDC_LV_ATTACKEDIT_SPECIALS);
	std::set<int> items;
	int idx = 0;
	for (; idx < ListView_GetItemCount(hctl); idx ++) {
		if (ListView_GetCheckState(hctl, idx)) {
			items.insert(idx);
		}
	}
	const specials_map& units_specials = unit_types.specials();
	idx = 0;
	for (specials_map::const_iterator it = units_specials.begin(); it != units_specials.end(); ++ it, idx ++) {
		if (items.find(idx) != items.end()) {
			specials_.insert(it->first);
		}
	}
}

void tunit_type::tattack::update_to_ui_utype_edit(HWND hdlgP, int index) const
{
	char text[_MAX_PATH];
	std::stringstream strstr;
	HWND hctl = GetDlgItem(hdlgP, IDC_LV_UTYPEEDIT_ATTACK);

	LVITEM lvi;
	int count = ListView_GetItemCount(hctl);

	lvi.mask = LVIF_TEXT | LVIF_PARAM;
	// 名称
	if (index < 0 || index >= count) {
		lvi.iItem = count;
	} else {
		lvi.iItem = index;
	}
	lvi.iSubItem = 0;
	strstr.str("");
	strstr << utf8_2_ansi(dgettext(PACKAGE, id_.c_str())) << "(" << id_ << ")";
	strcpy(text, strstr.str().c_str());
	lvi.pszText = text;
	lvi.lParam = (LPARAM)0;
	if (lvi.iItem != count) {
		ListView_SetItem(hctl, &lvi);
	} else {
		ListView_InsertItem(hctl, &lvi);
	}

	// 类型
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 1;
	strstr.str("");
	strstr << utf8_2_ansi(dgettext(PACKAGE, type_.c_str()));
	strcpy(text, strstr.str().c_str());
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 图标
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 2;
	strcpy(text, icon_.c_str());
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 距离
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 3;
	strstr.str("");
	if (range_ < 0 || range_ >= (int)attack_range_vstr.size()) {
		strstr << "无效距离";
	} else {
		strstr << utf8_2_ansi(dgettext(PACKAGE, attack_range_vstr[range_].c_str()));
	}
	strcpy(text, strstr.str().c_str());
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 特色
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 4;
	strstr.str("");
	const specials_map& units_specials = unit_types.specials();
	for (std::set<std::string>::const_iterator it = specials_.begin(); it != specials_.end(); ++ it) {
		const config& cfg = units_specials.find(*it)->second;
		BOOST_FOREACH (const config::any_child &sp, cfg.all_children_range()) {
			if (it == specials_.begin()) {
				strstr << utf8_2_ansi(sp.cfg["name"].str().c_str());
			} else {
				strstr << "," << utf8_2_ansi(sp.cfg["name"].str().c_str());
			}
		}
	}
	strcpy(text, strstr.str().c_str());
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 损伤
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 5;
	strstr.str("");
	strstr << damage_;
	strcpy(text, strstr.str().c_str());
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 次数
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 6;
	strstr.str("");
	strstr << number_;
	strcpy(text, strstr.str().c_str());
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);
}

void tunit_type::tattack::update_to_ui_attack_edit(HWND hdlgP) const
{
}

void tunit_type::tattack::generate(std::stringstream& strstr, const std::string& prefix) const
{
	strstr << prefix << "[attack]\n";
	strstr << prefix << "\tname = " << id_ << "\n";
	strstr << prefix << "\ticon = " << icon() << "\n";
	strstr << prefix << "\ttype = " << type_ << "\n";
	strstr << prefix << "\trange = " << attack_range_vstr[range_] << "\n";
	if (!specials_.empty()) {
		strstr << prefix << "\tspecials = ";
		for (std::set<std::string>::const_iterator it = specials_.begin(); it != specials_.end(); ++ it) {
			if (it == specials_.begin()) {
				strstr << *it;
			} else {
				strstr << ", " << *it;
			}
		}
		strstr << prefix << "\n";
	}
	strstr << prefix << "\tdamage = " << damage_ << "\n";
	strstr << prefix << "\tnumber = " << number_ << "\n";
	strstr << prefix << "[/attack]\n";
}

std::string tunit_type::tattack::icon(bool absolute) const
{ 
	if (absolute) {
		return game_config::path + "\\data\\core\\images\\attacks\\" + icon_; 
	} else {
		return std::string("attacks/") + icon_; 
	}
}

// 对话框消息处理函数
BOOL On_DlgCoreInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	HWND hctl = GetDlgItem(hdlgP, IDC_TV_CORE_EXPLORER);

	gdmgr._hdlg_core = hdlgP;
	ns::core.init_toolbar(gdmgr._hinst, hdlgP);
	core_enable_save_btn(FALSE);

	TreeView_SetImageList(hctl, gdmgr._himl, TVSIL_NORMAL);

	return FALSE;
}

BOOL On_DlgUTypeEditInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	if (ns::action_utype == ma_edit) {
		SetWindowText(hdlgP, "编辑兵种");
		ShowWindow(GetDlgItem(hdlgP, IDCANCEL), SW_HIDE);
	} else {
		SetWindowText(hdlgP, "添加兵种");
	}

	std::map<int, tunit_type>::const_iterator find_it = ns::core.types_updating_.find(ns::clicked_utype);
	if (find_it != ns::core.types_updating_.end()) {
		ns::utype = find_it->second;
	} else {
		const std::string& id = ns::core.tv_tree_[ns::clicked_utype].first;
		ns::utype.from_config(unit_types.find(id));
	}

	std::stringstream strstr;

	HWND hctl = GetDlgItem(hdlgP, IDC_ET_UTYPEEDIT_FILE);
	Edit_SetText(hctl, ns::utype.file(true).c_str());

	// textdomain
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_UTYPEEDIT_TEXTDOMAIN), ns::utype.textdomain_.c_str());
	// id
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_UTYPEEDIT_ID), ns::utype.id_.c_str());
	// name
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_UTYPEEDIT_NAME_MSGID), ns::utype.id_.c_str());
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_UTYPEEDIT_NAME), utf8_2_ansi(dgettext(ns::utype.textdomain_.c_str(), ns::utype.id_.c_str())));
	// description
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_UTYPEEDIT_DESC_MSGID), ns::utype.description().c_str());
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_UTYPEEDIT_DESC), utf8_2_ansi(dgettext(ns::utype.textdomain_.c_str(), ns::utype.description().c_str())));

	// race
	strstr.str("");
	const race_map& races = unit_types.races();
	strstr << utf8_2_ansi(races.find(ns::utype.race_)->second.name().c_str());
	strstr << "(" << ns::utype.race_ << ")";
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_UTYPEEDIT_RACE), strstr.str().c_str());
	
	// arms
	hctl = GetDlgItem(hdlgP, IDC_CMB_UTYPEEDIT_ARMS);
	const std::vector<std::string>& arms_ids = unit_types.arms_ids();
	int selected_row = -1;
	for (int i = 0; i < (int)arms_ids.size(); i ++) {
		ComboBox_AddString(hctl, utf8_2_ansi(hero::arms_str(i).c_str()));
		ComboBox_SetItemData(hctl, i, i);
		if (ns::utype.arms_ == i) {
			selected_row = ComboBox_GetCount(hctl) - 1;
		}
	}
	ComboBox_SetCurSel(hctl, selected_row);

	// HP
	hctl = GetDlgItem(hdlgP, IDC_UD_UTYPEEDIT_HP);
	UpDown_SetRange(hctl, 1, 1000);	// [1, 1000]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_UTYPEEDIT_HP));
	UpDown_SetPos(hctl, ns::utype.hitpoints_);

	// XP
	hctl = GetDlgItem(hdlgP, IDC_UD_UTYPEEDIT_XP);
	UpDown_SetRange(hctl, 1, 1000);	// [1, 1000]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_UTYPEEDIT_XP));
	UpDown_SetPos(hctl, ns::utype.experience_needed_);

	// cost
	hctl = GetDlgItem(hdlgP, IDC_UD_UTYPEEDIT_COST);
	UpDown_SetRange(hctl, 1, 1000);	// [1, 1000]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_UTYPEEDIT_COST));
	UpDown_SetPos(hctl, ns::utype.cost_);

	// level
	hctl = GetDlgItem(hdlgP, IDC_CMB_UTYPEEDIT_LEVEL);
	for (int i = 0; i < 10; i ++) {
		strstr.str("");
		strstr << i;
		ComboBox_AddString(hctl, strstr.str().c_str());
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, i);
	}
	ComboBox_SetCurSel(hctl, ns::utype.level_);

	// alignment
	selected_row = -1;
	hctl = GetDlgItem(hdlgP, IDC_CMB_UTYPEEDIT_ALIGNMENT);
	for (int i = unit_type::LAWFUL; i <= unit_type::LIMINAL; i ++) {
		ComboBox_AddString(hctl, unit_type::alignment_description((unit_type::ALIGNMENT)i));
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, i);
		if (i == ns::utype.alignment_) {
			selected_row = ComboBox_GetCount(hctl) - 1;
		}
	}
	ComboBox_SetCurSel(hctl, selected_row);

	std::vector<std::string> utype_type;
	// type
	selected_row = -1;
	hctl = GetDlgItem(hdlgP, IDC_CMB_UTYPEEDIT_TYPE);
	for (std::map<int, std::string>::const_iterator it = ns::utype.type_map_.begin(); it != ns::utype.type_map_.end(); ++ it) {
		ComboBox_AddString(hctl, it->second.c_str());
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, it->first);
	}
	ComboBox_SetCurSel(hctl, ns::utype.type());

	ns::utype.update_to_ui_utype_edit_type(hdlgP);

	// zoc
	Button_SetCheck(GetDlgItem(hdlgP, IDC_CHK_UTYPEEDIT_ZOC), ns::utype.zoc_);

	LVCOLUMN lvc;
	// advances_to
	hctl = GetDlgItem(hdlgP, IDC_LV_UTYPEEDIT_ADVANCESTO);
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 100;
	lvc.pszText = "名称(标识)";
	lvc.cchTextMax = 0;
	lvc.iSubItem = 0;
	ListView_InsertColumn(hctl, 0, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 60;
	lvc.iSubItem = 1;
	lvc.pszText = "等级";
	ListView_InsertColumn(hctl, 1, &lvc);

	ListView_SetImageList(hctl, gdmgr._himl_checkbox, LVSIL_STATE);
	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);
	// 默认情况下，鼠标右键只是光亮该行的最前一个子项，并且只有在该子项上才能触发NM_RCLICK。改为光亮整行，并且在整行都能触发NM_RCLICK。
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	// abilities
	hctl = GetDlgItem(hdlgP, IDC_LV_UTYPEEDIT_ABILITIES);
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 120;
	lvc.pszText = "名称(标识)";
	lvc.cchTextMax = 0;
	lvc.iSubItem = 0;
	ListView_InsertColumn(hctl, 0, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 700;
	lvc.iSubItem = 1;
	lvc.pszText = "描述";
	ListView_InsertColumn(hctl, 1, &lvc);

	ListView_SetImageList(hctl, gdmgr._himl_checkbox, LVSIL_STATE);
	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);
	// 默认情况下，鼠标右键只是光亮该行的最前一个子项，并且只有在该子项上才能触发NM_RCLICK。改为光亮整行，并且在整行都能触发NM_RCLICK。
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	// movment_type(resistance)
	hctl = GetDlgItem(hdlgP, IDC_LV_UTYPEEDIT_MTYPE);
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 80;
	lvc.pszText = "类型";
	lvc.cchTextMax = 0;
	lvc.iSubItem = 0;
	ListView_InsertColumn(hctl, 0, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 50;
	lvc.iSubItem = 1;
	lvc.pszText = "抗性";
	ListView_InsertColumn(hctl, 1, &lvc);

	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);
	// 默认情况下，鼠标右键只是光亮该行的最前一个子项，并且只有在该子项上才能触发NM_RCLICK。改为光亮整行，并且在整行都能触发NM_RCLICK。
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	// attacks
	hctl = GetDlgItem(hdlgP, IDC_LV_UTYPEEDIT_ATTACK);
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 100;
	lvc.pszText = "名称";
	lvc.cchTextMax = 0;
	lvc.iSubItem = 0;
	ListView_InsertColumn(hctl, 0, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 60;
	lvc.iSubItem = 1;
	lvc.pszText = "类型";
	ListView_InsertColumn(hctl, 1, &lvc);

	lvc.cx = 120;
	lvc.iSubItem = 2;
	lvc.pszText = "图标(attacks/)";
	ListView_InsertColumn(hctl, 2, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = 3;
	lvc.pszText = "距离";
	ListView_InsertColumn(hctl, 3, &lvc);

	lvc.cx = 80;
	lvc.iSubItem = 4;
	lvc.pszText = "特色";
	ListView_InsertColumn(hctl, 4, &lvc);

	lvc.cx = 38;
	lvc.iSubItem = 5;
	lvc.pszText = "损伤";
	ListView_InsertColumn(hctl, 5, &lvc);

	lvc.cx = 38;
	lvc.iSubItem = 6;
	lvc.pszText = "次数";
	ListView_InsertColumn(hctl, 6, &lvc);

	// ListView_SetImageList(hctl, gdmgr._himl, LVSIL_SMALL);
	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);

	// 默认情况下，鼠标右键只是光亮该行的最前一个子项，并且只有在该子项上才能触发NM_RCLICK。改为光亮整行，并且在整行都能触发NM_RCLICK。
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	ns::utype.update_to_ui_utype_edit(hdlgP);

	return FALSE;
}

BOOL On_DlgAttackEditInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	if (ns::action_attack == ma_edit) {
		SetWindowText(hdlgP, "编辑攻击");
		ShowWindow(GetDlgItem(hdlgP, IDCANCEL), SW_HIDE);
	} else {
		SetWindowText(hdlgP, "添加攻击");
	}

	char text[_MAX_PATH];
	std::stringstream strstr;
	HWND hctl;
	tunit_type::tattack& attack = ns::utype.attacks_[ns::clicked_attack];

	// textdomain
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_ATTACKEDIT_TEXTDOMAIN), PACKAGE);
	// id
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_ATTACKEDIT_ID), attack.id_.c_str());
	// name
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_ATTACKEDIT_NAME_MSGID), attack.id_.c_str());
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_ATTACKEDIT_NAME), utf8_2_ansi(dgettext(PACKAGE, attack.id_.c_str())));
	// icon
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_ATTACKEDIT_ICON), attack.icon().c_str());

	// type
	hctl = GetDlgItem(hdlgP, IDC_CMB_ATTACKEDIT_TYPE);
	int selected_row = -1;
	int index = 0;
	for (std::vector<std::string>::const_iterator it = attack_type_vstr.begin(); it != attack_type_vstr.end(); ++ it, index ++) {
		ComboBox_AddString(hctl, utf8_2_ansi(dgettext(PACKAGE, it->c_str())));
		ComboBox_SetItemData(hctl, index, index); 
		if (attack.type_ == *it) {
			selected_row = index;
		}
	}
	ComboBox_SetCurSel(hctl, selected_row);

	// range
	hctl = GetDlgItem(hdlgP, IDC_CMB_ATTACKEDIT_RANGE);
	selected_row = -1;
	index = 0;
	for (std::vector<std::string>::const_iterator it = attack_range_vstr.begin(); it != attack_range_vstr.end(); ++ it, index ++) {
		ComboBox_AddString(hctl, utf8_2_ansi(dgettext(PACKAGE, it->c_str())));
		ComboBox_SetItemData(hctl, index, index); 
		if (attack.range_ == index) {
			selected_row = index;
		}
	}
	ComboBox_SetCurSel(hctl, selected_row);

	// damage
	hctl = GetDlgItem(hdlgP, IDC_UD_ATTACKEDIT_DAMAGE);
	UpDown_SetRange(hctl, 1, 100);	// [1, 100]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_ATTACKEDIT_DAMAGE));
	UpDown_SetPos(hctl, attack.damage_);

	// number
	hctl = GetDlgItem(hdlgP, IDC_UD_ATTACKEDIT_NUMBER);
	UpDown_SetRange(hctl, 1, 10);	// [1, 10]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_ATTACKEDIT_NUMBER));
	UpDown_SetPos(hctl, attack.number_);

	LVCOLUMN lvc;
	// attacks
	hctl = GetDlgItem(hdlgP, IDC_LV_ATTACKEDIT_SPECIALS);
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 60;
	lvc.pszText = "名称";
	lvc.cchTextMax = 0;
	lvc.iSubItem = 0;
	ListView_InsertColumn(hctl, 0, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 500;
	lvc.iSubItem = 1;
	lvc.pszText = "描述";
	ListView_InsertColumn(hctl, 1, &lvc);

	ListView_SetImageList(hctl, gdmgr._himl_checkbox, LVSIL_STATE);
	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);

	// 默认情况下，鼠标右键只是光亮该行的最前一个子项，并且只有在该子项上才能触发NM_RCLICK。改为光亮整行，并且在整行都能触发NM_RCLICK。
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	LVITEM lvi;
	index = 0;
	const specials_map& units_specials = unit_types.specials();
	for (specials_map::const_iterator it = units_specials.begin(); it != units_specials.end(); ++ it) {
		const config* cfg = NULL;
		BOOST_FOREACH (const config::any_child &sp, it->second.all_children_range()) {
			cfg = &sp.cfg;
			break;
		}
		
		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		// 名称
		lvi.iItem = index ++;
		lvi.iSubItem = 0;
		strstr.str("");
		strstr << utf8_2_ansi(cfg->get("name")->str().c_str());
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		lvi.lParam = (LPARAM)0;
		ListView_InsertItem(hctl, &lvi);

		// 描述
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 1;
		strstr.str("");
		strstr << utf8_2_ansi(cfg->get("description")->str().c_str());
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		if (attack.specials_.find(it->first) != attack.specials_.end()) {
			ListView_SetCheckState(hctl, lvi.iItem, TRUE);
		} else {
			ListView_SetCheckState(hctl, lvi.iItem, FALSE);
		}
	}
	strstr.str("");
	strstr << "特色(" << editor_config::ListView_GetCheckedCount(hctl) << ")";
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_ATTACKEDIT_SPECIALS), strstr.str().c_str());

	return FALSE;
}

void OnAttackEditEt2(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	char text[_MAX_PATH];
	std::stringstream strstr;

	if (codeNotify != EN_CHANGE || id != IDC_ET_ATTACKEDIT_ID) {
		return;
	}

	tunit_type::tattack& attack = ns::utype.attacks_[ns::clicked_attack];

	Edit_GetText(hwndCtrl, text, sizeof(text) / sizeof(text[0]));
	std::string str = text;
	std::transform(str.begin(), str.end(), str.begin(), std::tolower);
	if (!isvalid_id(str)) {
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_ATTACKEDIT_IDSTATUS), "无效字符串");
		return;
	}
	// cannot be exist id.
	for (size_t i = 0; i < ns::utype.attacks_.size(); i ++) {
		if (i != ns::clicked_attack && str == ns::utype.attacks_[i].id_) {
			Edit_SetText(GetDlgItem(hdlgP, IDC_ET_ATTACKEDIT_IDSTATUS), "已存在该ID的其它攻击");
			return;
		}
	}
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_ATTACKEDIT_IDSTATUS), "");
	if (attack.id_ == str) {
		return;
	}
	
	// update relative ui conrol id.
	attack.id_ = str;
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_ATTACKEDIT_NAME_MSGID), attack.id_.c_str());
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_ATTACKEDIT_NAME), utf8_2_ansi(dgettext(PACKAGE, attack.id_.c_str())));

	return;
}

void OnAttackEditBt(HWND hdlgP, int id, UINT codeNotify)
{
	tunit_type::tattack& attack = ns::utype.attacks_[ns::clicked_attack];
	std::string icon = attack.icon(true);

	char* ptr = GetBrowseFileName(dirname(icon.c_str()), "*.png\0*.png\0\0", TRUE);
	if (!ptr) return;

	std::string new_icon = ptr;
	std::transform(icon.begin(), icon.end(), icon.begin(), std::tolower);
	std::transform(new_icon.begin(), new_icon.end(), new_icon.begin(), std::tolower);
	if (icon == new_icon) return;

	attack.icon_ = new_icon.substr(new_icon.rfind("\\") + 1);
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_ATTACKEDIT_ICON), attack.icon().c_str());
}

void On_DlgAttackEditCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	tunit_type::tattack& attack = ns::utype.attacks_[ns::clicked_attack];
	BOOL changed = FALSE;

	switch (id) {
	case IDC_ET_ATTACKEDIT_ID:
		OnAttackEditEt2(hdlgP, id, hwndCtrl, codeNotify);
		break;
	case IDC_BT_ATTACKEDIT_BROWSEICON:
		OnAttackEditBt(hdlgP, id, codeNotify);
		break;

	case IDOK:
		changed = TRUE;
		attack.from_ui(hdlgP);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

BOOL On_DlgAttackEditNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	std::stringstream strstr;

	if (lpNMHdr->code == NM_CLICK) {
		LPNMITEMACTIVATE lpnmitem = (LPNMITEMACTIVATE)lpNMHdr;
		if ((lpNMHdr->idFrom == IDC_LV_ATTACKEDIT_SPECIALS) && (lpnmitem->ptAction.x <= 14)) {
			if (ListView_GetCheckState(lpNMHdr->hwndFrom, lpnmitem->iItem)) {
				ListView_SetCheckState(lpNMHdr->hwndFrom, lpnmitem->iItem, FALSE);
			} else {
				if (editor_config::ListView_GetCheckedCount(lpNMHdr->hwndFrom) < 4) {
					ListView_SetCheckState(lpNMHdr->hwndFrom, lpnmitem->iItem, TRUE);
				}
			}
			if (lpNMHdr->idFrom == IDC_LV_ATTACKEDIT_SPECIALS) {
				strstr.str("");
				strstr << "特色(" << editor_config::ListView_GetCheckedCount(lpNMHdr->hwndFrom) << ")";
				Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_ATTACKEDIT_SPECIALS), strstr.str().c_str());
			} 
		}
	} 
	return FALSE;
}

BOOL CALLBACK DlgAttackEditProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message) {
	case WM_INITDIALOG:
		return On_DlgAttackEditInitDialog(hDlg, (HWND)(wParam), lParam);
	HANDLE_MSG(hDlg, WM_COMMAND, On_DlgAttackEditCommand);
	HANDLE_MSG(hDlg, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	HANDLE_MSG(hDlg, WM_NOTIFY, On_DlgAttackEditNotify);
	}
	
	return FALSE;
}

void OnAttackAddBt(HWND hdlgP)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_LV_UTYPEEDIT_ATTACK), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	if (!ns::utype.new_attack()) {
		return;
	}
	// set current attack to new added attack.
	ns::clicked_attack = ns::utype.attacks_.size() - 1;
	tunit_type::tattack& attack = ns::utype.attacks_[ns::clicked_attack];

	ns::action_attack = ma_new;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_ATTACKEDIT), hdlgP, DlgAttackEditProc, lParam)) {
		attack.update_to_ui_utype_edit(hdlgP, ns::clicked_attack);
	} else {
		ns::utype.erase_attack(ns::clicked_attack, NULL);
	}
	return;
}

void OnAttackEditBt(HWND hdlgP)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_LV_UTYPEEDIT_ATTACK), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	tunit_type::tattack& attack = ns::utype.attacks_[ns::clicked_attack];

	ns::action_attack = ma_edit;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_ATTACKEDIT), hdlgP, DlgAttackEditProc, lParam)) {
		attack.update_to_ui_utype_edit(hdlgP, ns::clicked_attack);
	}

	return;
}

void OnAttackDelBt(HWND hdlgP)
{
	ns::utype.erase_attack(ns::clicked_attack, hdlgP);

	if (ns::utype.type() == tunit_type::TYPE_ARTIFICAL) {
		if (ns::utype.guard_ != NO_GUARD && ns::utype.guard_ >= (int)ns::utype.attacks_.size()) {
			if (!ns::utype.attacks_.empty()) {
				ns::utype.guard_ = ns::utype.attacks_.size() - 1;
			} else {
				ns::utype.guard_ = NO_GUARD;
			}
			ns::utype.update_to_ui_utype_edit_type(hdlgP);
		}
	}

	return;
}

BOOL On_DlgMTypeEditInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	ns::clicked_mtype = -1;

	std::stringstream strstr;
	strstr << "抗性/防御指数/移动消耗";
	SetWindowText(hdlgP, strstr.str().c_str());
	ShowWindow(GetDlgItem(hdlgP, IDCANCEL), SW_HIDE);

	// movement_type
	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_MTYPEEDIT_TYPE);
	const movement_type_map& movement_types = unit_types.movement_types();
	int selected_row = -1;
	for (movement_type_map::const_iterator it = movement_types.begin(); it != movement_types.end(); ++ it) {
		ComboBox_AddString(hctl, it->first.c_str());
		if (ns::utype.movement_type_ == it->first) {
			selected_row = ComboBox_GetCount(hctl) - 1;
		}
	}
	ComboBox_SetCurSel(hctl, selected_row);

	LVCOLUMN lvc;
	// movment_type(resistance)
	hctl = GetDlgItem(hdlgP, IDC_LV_MTYPEEDIT_RESISTANCE);
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 80;
	lvc.pszText = "类型";
	lvc.cchTextMax = 0;
	lvc.iSubItem = 0;
	ListView_InsertColumn(hctl, 0, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 60;
	lvc.iSubItem = 1;
	lvc.pszText = "默认值";
	ListView_InsertColumn(hctl, 1, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 60;
	lvc.iSubItem = 2;
	lvc.pszText = "使用值";
	ListView_InsertColumn(hctl, 2, &lvc);

	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);
	// 默认情况下，鼠标右键只是光亮该行的最前一个子项，并且只有在该子项上才能触发NM_RCLICK。改为光亮整行，并且在整行都能触发NM_RCLICK。
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	// movment_type(defense)
	hctl = GetDlgItem(hdlgP, IDC_LV_MTYPEEDIT_DEFENSE);
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 80;
	lvc.pszText = "地形";
	lvc.cchTextMax = 0;
	lvc.iSubItem = 0;
	ListView_InsertColumn(hctl, 0, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 60;
	lvc.iSubItem = 1;
	lvc.pszText = "默认值";
	ListView_InsertColumn(hctl, 1, &lvc);

	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);
	// 默认情况下，鼠标右键只是光亮该行的最前一个子项，并且只有在该子项上才能触发NM_RCLICK。改为光亮整行，并且在整行都能触发NM_RCLICK。
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	// movment_type(movement_costs)
	hctl = GetDlgItem(hdlgP, IDC_LV_MTYPEEDIT_MOVEMENTCOSTS);
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 80;
	lvc.pszText = "地形";
	lvc.cchTextMax = 0;
	lvc.iSubItem = 0;
	ListView_InsertColumn(hctl, 0, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 60;
	lvc.iSubItem = 1;
	lvc.pszText = "默认值";
	ListView_InsertColumn(hctl, 1, &lvc);

	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);
	// 默认情况下，鼠标右键只是光亮该行的最前一个子项，并且只有在该子项上才能触发NM_RCLICK。改为光亮整行，并且在整行都能触发NM_RCLICK。
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	hctl = GetDlgItem(hdlgP, IDC_UD_MTYPEEDIT_VALUE);
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_MTYPEEDIT_VALUE));

	ns::utype.update_to_ui_mtype_edit(hdlgP);
	return FALSE;
}

void OnMTypeEditEt(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	char text[_MAX_PATH];
	std::stringstream strstr;

	if (codeNotify != EN_CHANGE || id != IDC_ET_MTYPEEDIT_VALUE || ns::clicked_mtype < 0) {
		return;
	}

	HWND hctl = GetDlgItem(hdlgP, IDC_UD_MTYPEEDIT_VALUE);
	int value = UpDown_GetPos(hctl);

	// Edit_GetText(hwndCtrl, text, sizeof(text) / sizeof(text[0]));
	// int value = atoi(text);
	hctl = GetDlgItem(hdlgP, IDC_LV_MTYPEEDIT_RESISTANCE);

	LVITEM lvi;

	lvi.iItem = ns::clicked_mtype;
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 2;
	sprintf(text, "%i", value);
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	return;
}

void OnMTypeEditCmb(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	if (codeNotify != CBN_SELCHANGE) {
		return;
	}
	if (id != IDC_CMB_MTYPEEDIT_TYPE) {
		return;
	}

	std::stringstream strstr;
	char text[_MAX_PATH];
	const movement_type_map& movement_types = unit_types.movement_types();
	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_MTYPEEDIT_TYPE);
	int selected_row = ComboBox_GetCurSel(hctl);
	int index = 0;

	for (movement_type_map::const_iterator it = movement_types.begin(); it != movement_types.end(); ++ it, index ++) {
		if (index == selected_row) {
			ns::utype.movement_type_ = it->first;
			break;;
		}
	}
	const unit_movement_type& movement_type_default = movement_types.find(ns::utype.movement_type_)->second;
	utils::string_map dam_tab_default = movement_type_default.damage_table();
	LVITEM lvi;
	hctl = GetDlgItem(hdlgP, IDC_LV_MTYPEEDIT_RESISTANCE);
	for (index = 0; index < ListView_GetItemCount(hctl); index ++) {
		// default value
		lvi.iItem = index;
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 1;
		strstr.str("");
		utils::string_map::const_iterator dam_it = dam_tab_default.find(attack_type_vstr[index]);
		if (dam_it != dam_tab_default.end()) {
			int resistance = 100 - atoi((*dam_it).second.c_str());
			strstr << resistance;
		} else {
			strstr << "--";
		}
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);
	}

	const config& movement_type_default_cfg = movement_type_default.get_cfg();
	const config& defense_cfg = movement_type_default_cfg.child("defense");
	const config& movement_costs_cfg = movement_type_default_cfg.child("movement_costs");
	index = 0;
	for (t_translation::t_list::const_iterator it = ns::core.alias_terrains_.begin(); it != ns::core.alias_terrains_.end(); ++ it) {
		const t_translation::t_terrain& terrain = *it;
		if ((terrain == t_translation::OFF_MAP_USER) ||
			(terrain == t_translation::FAKE_MAP_EDGE) ||
			(terrain == t_translation::FOGGED)) {
			continue;
		}
		const terrain_type& info = ns::core.map_->get_terrain_info(terrain);
		hctl = GetDlgItem(hdlgP, IDC_LV_MTYPEEDIT_DEFENSE);

		// 默认值
		lvi.iItem = index;
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 1;

		int value = 100;
		if (const config::attribute_value* val = defense_cfg.get(info.id())) {
			value = *val;
		}
		strstr.str("");
		if (value < 0) {
			strstr << 100 - (-1 * value);
		} else {
			strstr << 100 - value;
		}
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		hctl = GetDlgItem(hdlgP, IDC_LV_MTYPEEDIT_MOVEMENTCOSTS);
		// 默认值
		lvi.iItem = index ++;
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 1;
		value = unit_movement_type::UNREACHABLE;
		if (const config::attribute_value* val = movement_costs_cfg.get(info.id())) {
			value = *val;
		}
		strstr.str("");
		strstr << value;
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);
	}
	return;
}

void mtypeedit_reset(HWND hdlgP)
{
	char text[_MAX_PATH];
	HWND hctl = GetDlgItem(hdlgP, IDC_LV_MTYPEEDIT_RESISTANCE);
	LVITEM lvi;

	for (int index = 0; index < ListView_GetItemCount(hctl); index ++) {
		lvi.iItem = index;
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 1;
		lvi.pszText = text;
		lvi.cchTextMax = _MAX_PATH;
		ListView_GetItem(hctl, &lvi);

		lvi.iSubItem = 2;
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);
	}
}

void On_DlgMTypeEditCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;

	switch (id) {
	case IDC_ET_MTYPEEDIT_VALUE:
		OnMTypeEditEt(hdlgP, id, hwndCtrl, codeNotify);
		break;
	case IDC_CMB_MTYPEEDIT_TYPE:
		OnMTypeEditCmb(hdlgP, id, hwndCtrl, codeNotify);
		break;

	case IDM_RESET:
		mtypeedit_reset(hdlgP);
		break;

	case IDOK:
		changed = TRUE;
		ns::utype.from_ui_mtype_edit(hdlgP);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

void mtypeedit_notify_handler_rclick(HWND hdlgP, int id, LPNMHDR lpNMHdr)
{
	LVITEM					lvi;
	LPNMITEMACTIVATE		lpnmitem;

	if (lpNMHdr->code != NM_RCLICK) {
		return;
	}

	// NM_表示对通用控件都通用,范围丛(0, 99)
	// TVN_表示只能TreeView通用,范围丛(400, 499)
	if (lpNMHdr->hwndFrom == GetDlgItem(hdlgP, IDC_LV_MTYPEEDIT_RESISTANCE)) {
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

		if (lpnmitem->iItem < 0) {
			EnableMenuItem(gdmgr._hpopup_mtype, IDM_RESET, MF_BYCOMMAND | MF_GRAYED);
		}
		
		TrackPopupMenuEx(gdmgr._hpopup_mtype, 0, 
			point.x, 
			point.y, 
			hdlgP, 
			NULL);

		// 恢复回去
		EnableMenuItem(gdmgr._hpopup_mtype, IDM_RESET, MF_BYCOMMAND | MF_ENABLED);
	}

	if (lpNMHdr->hwndFrom == GetDlgItem(hdlgP, IDC_LV_MTYPEEDIT_RESISTANCE)) {
		ns::clicked_mtype = lpnmitem->iItem;

	}
    return;
}

BOOL On_DlgMTypeEditNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	if (lpNMHdr->code == NM_RCLICK) {
		mtypeedit_notify_handler_rclick(hdlgP, DlgItem, lpNMHdr);
		return FALSE;
	} else if (lpNMHdr->code != NM_DBLCLK) {
		return FALSE;
	}
	if (DlgItem != IDC_LV_MTYPEEDIT_RESISTANCE) {
		return FALSE;
	}

	LVITEM					lvi;
	LPNMITEMACTIVATE		lpnmitem = (LPNMITEMACTIVATE)lpNMHdr;
	char text[_MAX_PATH];

	lvi.iItem = lpnmitem->iItem;
	lvi.mask = LVIF_TEXT | LVIF_PARAM;
	lvi.iSubItem = 0;
	lvi.pszText = text;
	lvi.cchTextMax = _MAX_PATH;
	ListView_GetItem(lpNMHdr->hwndFrom, &lvi);
	ns::clicked_mtype = lvi.iItem;

	// update ui
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_MTYPEEDIT_NAME), text);

	lvi.iItem = lpnmitem->iItem;
	lvi.mask = LVIF_TEXT | LVIF_PARAM;
	lvi.iSubItem = 2;
	lvi.pszText = text;
	lvi.cchTextMax = _MAX_PATH;
	ListView_GetItem(lpNMHdr->hwndFrom, &lvi);
	int value = atoi(text);
	UpDown_SetRange(GetDlgItem(hdlgP, IDC_UD_MTYPEEDIT_VALUE), -500, 50);	// [-500, 50]
	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_MTYPEEDIT_VALUE), value);

	return FALSE;
}

BOOL CALLBACK DlgMTypeEditProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message) {
	case WM_INITDIALOG:
		return On_DlgMTypeEditInitDialog(hDlg, (HWND)(wParam), lParam);
	HANDLE_MSG(hDlg, WM_COMMAND, On_DlgMTypeEditCommand);
	HANDLE_MSG(hDlg, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	HANDLE_MSG(hDlg, WM_NOTIFY, On_DlgMTypeEditNotify);
	}
	
	return FALSE;
}

void OnMTypeEditBt(HWND hdlgP)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_LV_UTYPEEDIT_MTYPE), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_MTYPEEDIT), hdlgP, DlgMTypeEditProc, lParam)) {
		ns::utype.update_to_ui_utype_edit_mtype(hdlgP);
	}

	return;
}

BOOL On_DlgUTypeTroopInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	ns::clicked_mtype = -1;

	std::stringstream strstr;
	strstr << "部队兵种特有属性";
	SetWindowText(hdlgP, strstr.str().c_str());
	ShowWindow(GetDlgItem(hdlgP, IDCANCEL), SW_HIDE);

	// movement
	HWND hctl = GetDlgItem(hdlgP, IDC_UD_UTYPETROOP_MOVEMENT);
	UpDown_SetRange(hctl, 1, 30);	// [1, 30]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_UTYPETROOP_MOVEMENT));
	UpDown_SetPos(hctl, ns::utype.movement_);

	// character
	hctl = GetDlgItem(hdlgP, IDC_CMB_UTYPETROOP_CHARACTER);
	ComboBox_AddString(hctl, "");
	ComboBox_SetItemData(hctl, 0, NO_CHARACTER);
	int selected_row = 0;
	const std::vector<tcharacter>& characters = unit_types.characters();
	for (std::vector<tcharacter>::const_iterator it = characters.begin(); it != characters.end(); ++ it) {
		const tcharacter& character = *it;
		ComboBox_AddString(hctl, utf8_2_ansi(character.name_.c_str()));
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, character.index_);
		if (character.index_ == ns::utype.character_) {
			selected_row = ComboBox_GetCount(hctl) - 1;
		}
	}
	ComboBox_SetCurSel(hctl, selected_row);

	// walk_wall
	Button_SetCheck(GetDlgItem(hdlgP, IDC_CHK_UTYPETROOP_LANDWALL), ns::utype.land_wall_);
	// leader
	Button_SetCheck(GetDlgItem(hdlgP, IDC_CHK_UTYPETROOP_LEADER), ns::utype.leader_);

	ns::utype.update_to_ui_utype_type(hdlgP);
	return FALSE;
}


void On_DlgUTypeTroopCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;

	switch (id) {
	case IDOK:
		changed = TRUE;
		ns::utype.from_ui_utype_type(hdlgP);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

BOOL On_DlgUTypeTroopNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	if (lpNMHdr->code != NM_DBLCLK) {
		return FALSE;
	}
	if (DlgItem != IDC_LV_MTYPEEDIT_RESISTANCE) {
		return FALSE;
	}

	return FALSE;
}

BOOL CALLBACK DlgUTypeTroopProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message) {
	case WM_INITDIALOG:
		return On_DlgUTypeTroopInitDialog(hDlg, (HWND)(wParam), lParam);
	HANDLE_MSG(hDlg, WM_COMMAND, On_DlgUTypeTroopCommand);
	HANDLE_MSG(hDlg, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	HANDLE_MSG(hDlg, WM_NOTIFY, On_DlgUTypeTroopNotify);
	}
	
	return FALSE;
}

BOOL On_DlgUTypeCityInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	std::stringstream strstr;
	strstr << "城市兵种特有属性";
	SetWindowText(hdlgP, strstr.str().c_str());
	ShowWindow(GetDlgItem(hdlgP, IDCANCEL), SW_HIDE);

	// turn experience
	HWND hctl = GetDlgItem(hdlgP, IDC_UD_UTYPECITY_TURNEXPERIENCE);
	UpDown_SetRange(hctl, 0, 100);	// [0, 100]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_UTYPECITY_TURNEXPERIENCE));
	UpDown_SetPos(hctl, ns::utype.turn_experience_);

	// heal
	hctl = GetDlgItem(hdlgP, IDC_UD_UTYPECITY_HEAL);
	UpDown_SetRange(hctl, 0, 100);	// [0, 100]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_UTYPECITY_HEAL));
	UpDown_SetPos(hctl, ns::utype.heal_);

	ns::utype.update_to_ui_utype_type(hdlgP);
	return FALSE;
}


void On_DlgUTypeCityCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;

	switch (id) {
	case IDOK:
		changed = TRUE;
		ns::utype.from_ui_utype_type(hdlgP);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

BOOL On_DlgUTypeCityNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	if (lpNMHdr->code != NM_DBLCLK) {
		return FALSE;
	}
	if (DlgItem != IDC_LV_MTYPEEDIT_RESISTANCE) {
		return FALSE;
	}

	return FALSE;
}

BOOL CALLBACK DlgUTypeCityProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message) {
	case WM_INITDIALOG:
		return On_DlgUTypeCityInitDialog(hDlg, (HWND)(wParam), lParam);
	HANDLE_MSG(hDlg, WM_COMMAND, On_DlgUTypeCityCommand);
	HANDLE_MSG(hDlg, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	HANDLE_MSG(hDlg, WM_NOTIFY, On_DlgUTypeCityNotify);
	}
	
	return FALSE;
}

BOOL On_DlgUTypeArtificalInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	std::stringstream strstr;
	strstr << "建筑物兵种特有属性";
	SetWindowText(hdlgP, strstr.str().c_str());
	ShowWindow(GetDlgItem(hdlgP, IDCANCEL), SW_HIDE);

	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_UTYPEARTIFICAL_MASTER);
	int selected_row = -1;
	for (std::map<int, std::string>::iterator it = tunit_type::artifical_hero_.begin(); it != tunit_type::artifical_hero_.end(); ++ it) {
		ComboBox_AddString(hctl, it->second.c_str());
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, it->first);
		if (ns::utype.master_ == it->first) {
			selected_row = ComboBox_GetCount(hctl) - 1;
		}
	}
	ComboBox_SetCurSel(hctl, selected_row);

	// heal
	hctl = GetDlgItem(hdlgP, IDC_UD_UTYPEARTIFICAL_HEAL);
	UpDown_SetRange(hctl, 0, 100);	// [0, 100]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_UTYPEARTIFICAL_HEAL));
	UpDown_SetPos(hctl, ns::utype.heal_);

	hctl = GetDlgItem(hdlgP, IDC_CMB_UTYPEARTIFICAL_GUARD);
	ComboBox_AddString(hctl, "");
	ComboBox_SetItemData(hctl, 0, NO_GUARD);
	selected_row = 0;
	int index = 0;
	for (std::vector<tunit_type::tattack>::const_iterator it = ns::utype.attacks_.begin(); it != ns::utype.attacks_.end(); ++ it, index ++) {
		ComboBox_AddString(hctl, utf8_2_ansi(dgettext(PACKAGE, it->id_.c_str())));
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, index);
		if (ns::utype.guard_ == index) {
			selected_row = ComboBox_GetCount(hctl) - 1;
		}
	}
	ComboBox_SetCurSel(hctl, selected_row);

	// income
	hctl = GetDlgItem(hdlgP, IDC_UD_UTYPEARTIFICAL_INCOME);
	UpDown_SetRange(hctl, 0, 1000);	// [0, 1000]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_UTYPEARTIFICAL_INCOME));
	if (ns::utype.master_ == hero::number_market) {
		UpDown_SetPos(hctl, ns::utype.income_);
	} else {
		UpDown_SetPos(hctl, 0);
		Edit_Enable(GetDlgItem(hdlgP, IDC_ET_UTYPEARTIFICAL_INCOME), FALSE);
		EnableWindow(hctl, FALSE);
	}

	ns::utype.update_to_ui_utype_type(hdlgP);
	return FALSE;
}

void OnUTypeArtificalCmb(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	if (codeNotify != CBN_SELCHANGE) {
		return;
	}
	if (id != IDC_CMB_UTYPEARTIFICAL_MASTER) {
		return;
	}

	int master = ComboBox_GetItemData(hwndCtrl, ComboBox_GetCurSel(hwndCtrl));
	
	HWND hctl = GetDlgItem(hdlgP, IDC_UD_UTYPEARTIFICAL_INCOME);
	if (master == hero::number_market) {
		UpDown_SetPos(hctl, ns::utype.income_);
		Edit_Enable(GetDlgItem(hdlgP, IDC_ET_UTYPEARTIFICAL_INCOME), TRUE);
		EnableWindow(hctl, TRUE);
	} else {
		UpDown_SetPos(hctl, 0);
		Edit_Enable(GetDlgItem(hdlgP, IDC_ET_UTYPEARTIFICAL_INCOME), FALSE);
		EnableWindow(hctl, FALSE);
	}

	return;
}

void On_DlgUTypeArtificalCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;

	switch (id) {
	case IDC_CMB_UTYPEARTIFICAL_MASTER:
		OnUTypeArtificalCmb(hdlgP, id, hwndCtrl, codeNotify);
		break;

	case IDOK:
		changed = TRUE;
		ns::utype.from_ui_utype_type(hdlgP);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

BOOL CALLBACK DlgUTypeArtificalProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message) {
	case WM_INITDIALOG:
		return On_DlgUTypeArtificalInitDialog(hDlg, (HWND)(wParam), lParam);
	HANDLE_MSG(hDlg, WM_COMMAND, On_DlgUTypeArtificalCommand);
	HANDLE_MSG(hDlg, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	}
	
	return FALSE;
}

void OnUTypeTypeBt(HWND hdlgP)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_BT_UTYPEEDIT_SPECIAL), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	int retval = 0;
	if (ns::utype.type() == tunit_type::TYPE_TROOP) {
		retval = DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_UTYPETROOP), hdlgP, DlgUTypeTroopProc, lParam);
	} else if (ns::utype.type() == tunit_type::TYPE_CITY) {
		retval = DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_UTYPECITY), hdlgP, DlgUTypeCityProc, lParam);
	} else if (ns::utype.type() == tunit_type::TYPE_ARTIFICAL) {
		retval = DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_UTYPEARTIFICAL), hdlgP, DlgUTypeArtificalProc, lParam);
	}
	if (retval) {
		ns::utype.update_to_ui_utype_edit_type(hdlgP);
	}

	return;
}

void OnUTypeEditEt(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	char text[_MAX_PATH];
	std::stringstream strstr;

	if (codeNotify != EN_CHANGE || id != IDC_ET_UTYPEEDIT_TEXTDOMAIN) {
		return;
	}

	Edit_GetText(hwndCtrl, text, sizeof(text) / sizeof(text[0]));
	std::string str = text;
	std::transform(str.begin(), str.end(), str.begin(), std::tolower);
	if (!isvalid_id(str)) {
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_UTYPEEDIT_TEXTDOMAINSTATUS), "无效字符串");
		return;
	}
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_UTYPEEDIT_TEXTDOMAINSTATUS), "");
	if (ns::utype.textdomain_ == str) {
		return;
	}

	ns::utype.textdomain_ = str;
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_UTYPEEDIT_NAME), utf8_2_ansi(dgettext(ns::utype.textdomain_.c_str(), ns::utype.id_.c_str())));
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_UTYPEEDIT_DESC), utf8_2_ansi(dgettext(ns::utype.textdomain_.c_str(), ns::utype.description().c_str())));
	
	return;
}

void OnUTypeEditEt2(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	char text[_MAX_PATH];
	std::stringstream strstr;

	if (codeNotify != EN_CHANGE || id != IDC_ET_UTYPEEDIT_ID) {
		return;
	}

	Edit_GetText(hwndCtrl, text, sizeof(text) / sizeof(text[0]));
	std::string str = text;
	std::transform(str.begin(), str.end(), str.begin(), std::tolower);
	if (!isvalid_id(str)) {
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_UTYPEEDIT_IDSTATUS), "无效字符串");
		return;
	}
	// cannot be exist id.
	for (size_t i = 0; i < ns::core.tv_tree_.size(); i ++) {
		if (i != ns::clicked_utype && str == ns::core.tv_tree_[i].first) {
			Edit_SetText(GetDlgItem(hdlgP, IDC_ET_UTYPEEDIT_IDSTATUS), "已存在该ID的其它兵种");
			return;
		}
	}
	if (ns::action_utype == ma_new) {
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_UTYPEEDIT_IDSTATUS), "请设置新加兵种ID");
	} else {
		if (ns::utype.utype_from_cfg_.id_ != str) {
			Edit_SetText(GetDlgItem(hdlgP, IDC_ET_UTYPEEDIT_IDSTATUS), "修改ID会导致自动重汇编");
		} else {
			Edit_SetText(GetDlgItem(hdlgP, IDC_ET_UTYPEEDIT_IDSTATUS), "");
		}
	}
	if (ns::utype.id_ == str) {
		return;
	}
		
	// update relative control.
	ns::utype.id_ = str;
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_UTYPEEDIT_NAME_MSGID), ns::utype.id_.c_str());
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_UTYPEEDIT_NAME), utf8_2_ansi(dgettext(ns::utype.textdomain_.c_str(), ns::utype.id_.c_str())));
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_UTYPEEDIT_DESC_MSGID), ns::utype.description().c_str());
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_UTYPEEDIT_DESC), utf8_2_ansi(dgettext(ns::utype.textdomain_.c_str(), ns::utype.description().c_str())));

	return;
}

void OnUTypeEditCmb(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	if (codeNotify != CBN_SELCHANGE) {
		return;
	}
	if (id != IDC_CMB_UTYPEEDIT_TYPE) {
		return;
	}

	int type = ComboBox_GetItemData(hwndCtrl, ComboBox_GetCurSel(hwndCtrl));
	if (type == tunit_type::TYPE_TROOP) {
		ns::utype.master_ = HEROS_INVALID_NUMBER;
		ns::utype.can_recruit_ = false;

	} else if (type == tunit_type::TYPE_CITY) {
		ns::utype.master_ = HEROS_INVALID_NUMBER;
		ns::utype.can_recruit_ = true;
		
	} else if (type == tunit_type::TYPE_ARTIFICAL) {
		if (ns::utype.utype_from_cfg_.master_ != HEROS_INVALID_NUMBER) {
			ns::utype.master_ = ns::utype.utype_from_cfg_.master_;
		} else {
			ns::utype.master_ = tunit_type::artifical_hero_.begin()->first;
		}
		ns::utype.can_recruit_ = false;
	}
	if (type == tunit_type::TYPE_TROOP && !ns::utype.utype_from_cfg_.movement_) {
		ns::utype.movement_ = 5;
	} else {
		ns::utype.movement_ = ns::utype.utype_from_cfg_.movement_;
	}
	ns::utype.character_ = ns::utype.utype_from_cfg_.character_;
	ns::utype.land_wall_ = ns::utype.utype_from_cfg_.land_wall_;
	ns::utype.leader_ = ns::utype.utype_from_cfg_.leader_;
	ns::utype.turn_experience_ = ns::utype.utype_from_cfg_.turn_experience_;
	ns::utype.heal_ = ns::utype.utype_from_cfg_.heal_;
	if (type == tunit_type::TYPE_ARTIFICAL && ns::utype.utype_from_cfg_.guard_ != NO_GUARD && ns::utype.utype_from_cfg_.guard_ >= (int)ns::utype.attacks_.size()) {
		ns::utype.guard_ = ns::utype.attacks_.size() - 1;
	} else {
		ns::utype.guard_ = ns::utype.utype_from_cfg_.guard_;
	}
	ns::utype.income_ = ns::utype.utype_from_cfg_.income_;

	ns::utype.update_to_ui_utype_edit_type(hdlgP);
	return;
}

void On_DlgUTypeEditCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;

	switch (id) {
	case IDC_ET_UTYPEEDIT_TEXTDOMAIN:
		OnUTypeEditEt(hdlgP, id, hwndCtrl, codeNotify);
		break;
	case IDC_ET_UTYPEEDIT_ID:
		OnUTypeEditEt2(hdlgP, id, hwndCtrl, codeNotify);
		break;

	case IDM_ADD:
		OnAttackAddBt(hdlgP);
		break;
	case IDM_DELETE:
		OnAttackDelBt(hdlgP);
		break;
	case IDM_EDIT:
		OnAttackEditBt(hdlgP);
		break;

	case IDC_CMB_UTYPEEDIT_TYPE:
		OnUTypeEditCmb(hdlgP, id, hwndCtrl, codeNotify);
		break;
	case IDC_BT_UTYPEEDIT_SPECIAL:
		OnUTypeTypeBt(hdlgP);
		break;

	case IDOK:
		changed = TRUE;
		ns::utype.from_ui(hdlgP);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

void utypeedit_notify_handler_rclick(HWND hdlgP, int id, LPNMHDR lpNMHdr)
{
	LVITEM					lvi;
	LPNMITEMACTIVATE		lpnmitem;
	int						icount;

	if (lpNMHdr->code != NM_RCLICK) {
		return;
	}

	// NM_表示对通用控件都通用,范围丛(0, 99)
	// TVN_表示只能TreeView通用,范围丛(400, 499)
	if (lpNMHdr->hwndFrom == GetDlgItem(hdlgP, IDC_LV_UTYPEEDIT_ATTACK)) {
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

		if (ns::utype.attacks_.size() >= 3) {
			EnableMenuItem(gdmgr._hpopup_editor, IDM_ADD, MF_BYCOMMAND | MF_GRAYED);
		}
		if (lpnmitem->iItem < 0) {
			EnableMenuItem(gdmgr._hpopup_editor, IDM_EDIT, MF_BYCOMMAND | MF_GRAYED);
			EnableMenuItem(gdmgr._hpopup_editor, IDM_DELETE, MF_BYCOMMAND | MF_GRAYED);
		}
		
		TrackPopupMenuEx(gdmgr._hpopup_editor, 0, 
			point.x, 
			point.y, 
			hdlgP, 
			NULL);

		// 恢复回去
		EnableMenuItem(gdmgr._hpopup_editor, IDM_ADD, MF_BYCOMMAND | MF_ENABLED);
		EnableMenuItem(gdmgr._hpopup_editor, IDM_EDIT, MF_BYCOMMAND | MF_ENABLED);
		EnableMenuItem(gdmgr._hpopup_editor, IDM_DELETE, MF_BYCOMMAND | MF_ENABLED);
	}

	if (lpNMHdr->hwndFrom == GetDlgItem(hdlgP, IDC_LV_UTYPEEDIT_ATTACK)) {
		ns::clicked_attack = lpnmitem->iItem;

	}
    return;
}

void utypeedit_notify_handler_dblclk(HWND hdlgP, LPNMHDR lpNMHdr)
{
	LVITEM					lvi;
	LPNMITEMACTIVATE		lpnmitem;

	if (lpNMHdr->code != NM_DBLCLK) {
		return;
	}

	// NM_表示对通用控件都通用,范围丛(0, 99)
	// TVN_表示只能TreeView通用,范围丛(400, 499)
	if (lpNMHdr->hwndFrom == GetDlgItem(hdlgP, IDC_LV_UTYPEEDIT_MTYPE)) {
		OnMTypeEditBt(hdlgP);
		return;

	} else if (lpNMHdr->hwndFrom == GetDlgItem(hdlgP, IDC_LV_UTYPEEDIT_ATTACK)) {
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
	}
	if (lpNMHdr->hwndFrom == GetDlgItem(hdlgP, IDC_LV_UTYPEEDIT_ATTACK)) {
		if (lpnmitem->iItem >= 0) {
			ns::clicked_attack = lpnmitem->iItem;
			OnAttackEditBt(hdlgP);
		}
	}
    return;
}

BOOL On_DlgUTypeEditNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	std::stringstream strstr;

	if (lpNMHdr->code == NM_CLICK) {
		LPNMITEMACTIVATE lpnmitem = (LPNMITEMACTIVATE)lpNMHdr;
		if ((lpNMHdr->idFrom == IDC_LV_UTYPEEDIT_ADVANCESTO || lpNMHdr->idFrom == IDC_LV_UTYPEEDIT_ABILITIES) && (lpnmitem->ptAction.x <= 14)) {
			if (ListView_GetCheckState(lpNMHdr->hwndFrom, lpnmitem->iItem)) {
				ListView_SetCheckState(lpNMHdr->hwndFrom, lpnmitem->iItem, FALSE);
			} else {
				ListView_SetCheckState(lpNMHdr->hwndFrom, lpnmitem->iItem, TRUE);
			}
			strstr.str("");
			if (lpNMHdr->idFrom == IDC_LV_UTYPEEDIT_ADVANCESTO) {
				strstr << "升级到(" << editor_config::ListView_GetCheckedCount(lpNMHdr->hwndFrom) << ")";
				Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_UTYPEEDIT_ADVANCESTO), strstr.str().c_str());

			} else if (lpNMHdr->idFrom == IDC_LV_UTYPEEDIT_ABILITIES) {
				strstr << "能力(" << editor_config::ListView_GetCheckedCount(lpNMHdr->hwndFrom) << ")";
				Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_UTYPEEDIT_ABILITIES), strstr.str().c_str());
			} 
		}
	} else if (lpNMHdr->code == NM_RCLICK) {
		utypeedit_notify_handler_rclick(hdlgP, DlgItem, lpNMHdr);

	} else if (lpNMHdr->code == NM_DBLCLK) {
		utypeedit_notify_handler_dblclk(hdlgP, lpNMHdr);

	}
	return FALSE;
}

BOOL CALLBACK DlgUTypeEditProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message) {
	case WM_INITDIALOG:
		return On_DlgUTypeEditInitDialog(hDlg, (HWND)(wParam), lParam);
	HANDLE_MSG(hDlg, WM_COMMAND, On_DlgUTypeEditCommand);
	HANDLE_MSG(hDlg, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	HANDLE_MSG(hDlg, WM_NOTIFY, On_DlgUTypeEditNotify);
	// HANDLE_MSG(hDlg, WM_DESTROY, On_DlgUTypeEditDestroy);
	}
	
	return FALSE;
}

BOOL On_DlgUType2EditInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	if (ns::action_utype == ma_edit) {
		SetWindowText(hdlgP, "编辑打包兵种");
		ShowWindow(GetDlgItem(hdlgP, IDCANCEL), SW_HIDE);
	} else {
		SetWindowText(hdlgP, "添加打包兵种");
	}

	std::map<int, tunit_type>::const_iterator find_it = ns::core.types_updating_.find(ns::clicked_utype);
	if (find_it != ns::core.types_updating_.end()) {
		ns::utype = find_it->second;
	} else {
		const std::string& id = ns::core.tv_tree_[ns::clicked_utype].first;
		ns::utype.from_config(unit_types.find(id));
	}

	std::stringstream strstr;

	HWND hctl = GetDlgItem(hdlgP, IDC_ET_UTYPEEDIT_FILE);
	Edit_SetText(hctl, ns::utype.file(true).c_str());

	// textdomain
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_UTYPEEDIT_TEXTDOMAIN), ns::utype.textdomain_.c_str());
	Edit_Enable(GetDlgItem(hdlgP, IDC_ET_UTYPEEDIT_TEXTDOMAIN), FALSE);
	// id
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_UTYPEEDIT_ID), ns::utype.id_.c_str());
	Edit_Enable(GetDlgItem(hdlgP, IDC_ET_UTYPEEDIT_ID), FALSE);
	// name
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_UTYPEEDIT_NAME_MSGID), ns::utype.id_.c_str());
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_UTYPEEDIT_NAME), utf8_2_ansi(dgettext(ns::utype.textdomain_.c_str(), ns::utype.id_.c_str())));
	// description
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_UTYPEEDIT_DESC_MSGID), ns::utype.description().c_str());
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_UTYPEEDIT_DESC), utf8_2_ansi(dgettext(ns::utype.textdomain_.c_str(), ns::utype.description().c_str())));

	// race
	strstr.str("");
	const race_map& races = unit_types.races();
	strstr << utf8_2_ansi(races.find(ns::utype.race_)->second.name().c_str());
	strstr << "(" << ns::utype.race_ << ")";
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_UTYPEEDIT_RACE), strstr.str().c_str());
	
	// arms
	hctl = GetDlgItem(hdlgP, IDC_CMB_UTYPEEDIT_ARMS);
	const std::vector<std::string>& arms_ids = unit_types.arms_ids();
	int selected_row = -1;
	for (int i = 0; i < (int)arms_ids.size(); i ++) {
		ComboBox_AddString(hctl, utf8_2_ansi(hero::arms_str(i).c_str()));
		ComboBox_SetItemData(hctl, i, i);
		if (ns::utype.arms_ == i) {
			selected_row = ComboBox_GetCount(hctl) - 1;
		}
	}
	ComboBox_SetCurSel(hctl, selected_row);
	ComboBox_Enable(hctl, FALSE);

	// zoc
	Button_SetCheck(GetDlgItem(hdlgP, IDC_CHK_UTYPEEDIT_ZOC), ns::utype.zoc_);

	// melee increase
	hctl = GetDlgItem(hdlgP, IDC_UD_UTYPEEDIT_MELEEINCREASE);
	UpDown_SetRange(hctl, -100, 100);	// [-100, 100]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_UTYPEEDIT_MELEEINCREASE));
	UpDown_SetPos(hctl, ns::utype.melee_increase_);

	// ranged increase
	hctl = GetDlgItem(hdlgP, IDC_UD_UTYPEEDIT_RANGEDINCREASE);
	UpDown_SetRange(hctl, -100, 100);	// [-100, 100]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_UTYPEEDIT_RANGEDINCREASE));
	UpDown_SetPos(hctl, ns::utype.ranged_increase_);

	// cast increase
	hctl = GetDlgItem(hdlgP, IDC_UD_UTYPEEDIT_CASTINCREASE);
	UpDown_SetRange(hctl, -100, 100);	// [-100, 100]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_UTYPEEDIT_CASTINCREASE));
	UpDown_SetPos(hctl, ns::utype.cast_increase_);

	LVCOLUMN lvc;
	// movment_type(resistance)
	hctl = GetDlgItem(hdlgP, IDC_LV_UTYPEEDIT_MTYPE);
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 80;
	lvc.pszText = "类型";
	lvc.cchTextMax = 0;
	lvc.iSubItem = 0;
	ListView_InsertColumn(hctl, 0, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 50;
	lvc.iSubItem = 1;
	lvc.pszText = "抗性";
	ListView_InsertColumn(hctl, 1, &lvc);

	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);
	// 默认情况下，鼠标右键只是光亮该行的最前一个子项，并且只有在该子项上才能触发NM_RCLICK。改为光亮整行，并且在整行都能触发NM_RCLICK。
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	ns::utype.update_to_ui_utype_edit_mtype(hdlgP);

	return FALSE;
}

void On_DlgUType2EditCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;

	switch (id) {
	case IDC_ET_UTYPEEDIT_TEXTDOMAIN:
		OnUTypeEditEt(hdlgP, id, hwndCtrl, codeNotify);
		break;
	case IDC_ET_UTYPEEDIT_ID:
		OnUTypeEditEt2(hdlgP, id, hwndCtrl, codeNotify);
		break;

	case IDOK:
		changed = TRUE;
		ns::utype.from_ui(hdlgP);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

BOOL CALLBACK DlgUType2EditProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message) {
	case WM_INITDIALOG:
		return On_DlgUType2EditInitDialog(hDlg, (HWND)(wParam), lParam);
	HANDLE_MSG(hDlg, WM_COMMAND, On_DlgUType2EditCommand);
	HANDLE_MSG(hDlg, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	HANDLE_MSG(hDlg, WM_NOTIFY, On_DlgUTypeEditNotify);
	}
	
	return FALSE;
}

void OnUTypeAddBt(HWND hdlgP)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_TV_CORE_EXPLORER), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	if (!ns::core.new_utype()) {
		return;
	}

	// set current unit type to new added unit type.
	ns::clicked_utype = ns::core.tv_tree_.size() - 1;

	ns::action_utype = ma_new;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_UTYPEEDIT), hdlgP, DlgUTypeEditProc, lParam)) {
		ns::core.save(hdlgP);
	} else {
		ns::core.erase_utype(ns::clicked_utype, NULL);
	}

	return;
}

void OnUTypeEditBt(HWND hdlgP)
{
	RECT		rcBtn;
	LPARAM		lParam;
		
	GetWindowRect(GetDlgItem(hdlgP, IDC_TV_CORE_EXPLORER), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	const std::string& id = ns::core.tv_tree_[ns::clicked_utype].first;
	const unit_type* ut = unit_types.find(id);

	ns::action_utype = ma_edit;
	int retval;
	if (ut->packer()) {
		retval = DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_UTYPE2EDIT), hdlgP, DlgUType2EditProc, lParam);
	} else {
		retval = DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_UTYPEEDIT), hdlgP, DlgUTypeEditProc, lParam);
	}
	if (retval) {
		if (ns::utype.id_ != ns::utype.utype_from_cfg_.id_ || ns::utype.advances_to_ != ns::utype.utype_from_cfg_.advances_to_) {
			ns::core.save(hdlgP);
		} else {
			ns::core.set_dirty(tcore::BIT_UTYPE, ns::core.types_dirty()); 
		}
	}

	return;
}

void OnUTypeDelBt(HWND hdlgP)
{
	// delete relative .cfg
	const std::string& id = ns::core.tv_tree_[ns::clicked_utype].first;
	const unit_type* ut = unit_types.find(id);

	std::stringstream message;
	message << "您想删除“" << utf8_2_ansi(ut->type_name().c_str()) << "”兵种吗？";
	if (MessageBox(hdlgP, message.str().c_str(), "确认删除", MB_YESNO | MB_DEFBUTTON2) != IDYES) {
		return;
	}

	ns::utype.from_config(ut);
	delfile1(ns::utype.cfg_file(true).c_str());

	ns::core.save(hdlgP);
	return;
}

BOOL On_DlgVisual2InitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	if (ns::type == IDM_UTYPE_GENERAL) {
		SetWindowText(hdlgP, "兵种--基本信息");
	} else if (ns::type == IDM_UTYPE_MTYPE) {
		SetWindowText(hdlgP, "兵种--抗性");
	} else if (ns::type == IDM_UTYPE_ATTACKI) {
		SetWindowText(hdlgP, "兵种--攻击I");
	} else if (ns::type == IDM_UTYPE_ATTACKII) {
		SetWindowText(hdlgP, "兵种--攻击II");
	} else if (ns::type == IDM_UTYPE_ATTACKIII) {
		SetWindowText(hdlgP, "兵种--攻击III");
	}

	HWND hctl = GetDlgItem(hdlgP, IDC_LV_VISUAL2_EXPLORER);
	LVCOLUMN lvc;
	int index = 0;
	std::stringstream strstr;
	char text[_MAX_PATH];

	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 80;
	lvc.pszText = "名称";
	lvc.cchTextMax = 0;
	lvc.iSubItem = index;
	ListView_InsertColumn(hctl, index ++, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 50;
	lvc.iSubItem = index;
	lvc.pszText = "等级";
	ListView_InsertColumn(hctl, index ++, &lvc);

	if (ns::type == IDM_UTYPE_GENERAL) {
		lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
		lvc.cx = 50;
		lvc.iSubItem = index;
		lvc.pszText = "HP";
		ListView_InsertColumn(hctl, index ++, &lvc);

		lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
		lvc.cx = 50;
		lvc.iSubItem = index;
		lvc.pszText = "XP";
		ListView_InsertColumn(hctl, index ++, &lvc);

		lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
		lvc.cx = 50;
		lvc.iSubItem = index;
		lvc.pszText = "移动力";
		ListView_InsertColumn(hctl, index ++, &lvc);

		lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
		lvc.cx = 50;
		lvc.iSubItem = index;
		lvc.pszText = "兵科";
		ListView_InsertColumn(hctl, index ++, &lvc);

		lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
		lvc.cx = 50;
		lvc.iSubItem = index;
		lvc.pszText = "成本";
		ListView_InsertColumn(hctl, index ++, &lvc);

		lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
		lvc.cx = 50;
		lvc.iSubItem = index;
		lvc.pszText = "上城墙";
		ListView_InsertColumn(hctl, index ++, &lvc);

		lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
		lvc.cx = 50;
		lvc.iSubItem = index;
		lvc.pszText = "ZOC";
		ListView_InsertColumn(hctl, index ++, &lvc);

		lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
		lvc.cx = 150;
		lvc.iSubItem = index;
		lvc.pszText = "能力";
		ListView_InsertColumn(hctl, index ++, &lvc);
		
	} else if (ns::type == IDM_UTYPE_MTYPE) {

		lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
		lvc.cx = 75;
		lvc.iSubItem = index;
		lvc.pszText = "模板";
		ListView_InsertColumn(hctl, index ++, &lvc);

		for (std::vector<std::string>::const_iterator it = attack_type_vstr.begin(); it != attack_type_vstr.end(); ++ it) {
			lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
			lvc.cx = 50;
			lvc.iSubItem = index;
			strstr.str("");
			strstr << utf8_2_ansi(gettext(it->c_str()));
			strcpy(text, strstr.str().c_str());
			lvc.pszText = text;
			ListView_InsertColumn(hctl, index ++, &lvc);
		}

	} else if (ns::type == IDM_UTYPE_ATTACKI || ns::type == IDM_UTYPE_ATTACKII || ns::type == IDM_UTYPE_ATTACKIII) {
		lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
		lvc.cx = 100;
		lvc.iSubItem = index;
		lvc.pszText = "名称";
		ListView_InsertColumn(hctl, index ++, &lvc);

		lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
		lvc.cx = 50;
		lvc.iSubItem = index;
		lvc.pszText = "类型";
		ListView_InsertColumn(hctl, index ++, &lvc);

		lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
		lvc.cx = 120;
		lvc.iSubItem = index;
		lvc.pszText = "图标";
		ListView_InsertColumn(hctl, index ++, &lvc);

		lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
		lvc.cx = 50;
		lvc.iSubItem = index;
		lvc.pszText = "距离";
		ListView_InsertColumn(hctl, index ++, &lvc);

		lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
		lvc.cx = 100;
		lvc.iSubItem = index;
		lvc.pszText = "特色";
		ListView_InsertColumn(hctl, index ++, &lvc);

		lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
		lvc.cx = 40;
		lvc.iSubItem = index;
		lvc.pszText = "伤害";
		ListView_InsertColumn(hctl, index ++, &lvc);

		lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
		lvc.cx = 40;
		lvc.iSubItem = index;
		lvc.pszText = "次数";
		ListView_InsertColumn(hctl, index ++, &lvc);
	}

	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);


	// fill data
	LVITEM lvi;
	int iItem = 0;
	for (std::vector<std::pair<std::string, std::vector<std::string> > >::const_iterator it = ns::core.tv_tree_.begin(); it != ns::core.tv_tree_.end(); ++ it, iItem ++) {
		const unit_type* ut = unit_types.find(it->first);

		index = 0;

		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		// 名称
		lvi.iItem = iItem;
		lvi.iSubItem = index ++;
		strstr.str("");
		strstr << utf8_2_ansi(ut->type_name().c_str());
		if (ut->leader()) {
			strstr << "(君主)";
		}
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		lvi.lParam = (LPARAM)0;
		ListView_InsertItem(hctl, &lvi);

		// 等级
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = index ++;
		strstr.str("");
		strstr << ut->level();
		if (ut->character() != NO_CHARACTER) {
			strstr << "(" << utf8_2_ansi(unit_types.character(ut->character()).name_.c_str()) << ")";
		}
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		if (ns::type == IDM_UTYPE_GENERAL) {
			lvi.mask = LVIF_TEXT;
			lvi.iSubItem = index ++;
			strstr.str("");
			strstr << ut->hitpoints();
			strcpy(text, strstr.str().c_str());
			lvi.pszText = text;
			ListView_SetItem(hctl, &lvi);

			lvi.mask = LVIF_TEXT;
			lvi.iSubItem = index ++;
			strstr.str("");
			strstr << ut->experience_needed();
			strcpy(text, strstr.str().c_str());
			lvi.pszText = text;
			ListView_SetItem(hctl, &lvi);

			lvi.mask = LVIF_TEXT;
			lvi.iSubItem = index ++;
			strstr.str("");
			strstr << ut->movement();
			strcpy(text, strstr.str().c_str());
			lvi.pszText = text;
			ListView_SetItem(hctl, &lvi);

			lvi.mask = LVIF_TEXT;
			lvi.iSubItem = index ++;
			strstr.str("");
			strstr << utf8_2_ansi(hero::arms_str(ut->arms()).c_str());
			strcpy(text, strstr.str().c_str());
			lvi.pszText = text;
			ListView_SetItem(hctl, &lvi);

			lvi.mask = LVIF_TEXT;
			lvi.iSubItem = index ++;
			strstr.str("");
			strstr << ut->cost();
			strcpy(text, strstr.str().c_str());
			lvi.pszText = text;
			ListView_SetItem(hctl, &lvi);

			lvi.mask = LVIF_TEXT;
			lvi.iSubItem = index ++;
			strstr.str("");
			strstr << (ut->land_wall()? "能": "不能");
			strcpy(text, strstr.str().c_str());
			lvi.pszText = text;
			ListView_SetItem(hctl, &lvi);

			lvi.mask = LVIF_TEXT;
			lvi.iSubItem = index ++;
			strstr.str("");
			strstr << (ut->has_zoc()? "有": "没有");
			strcpy(text, strstr.str().c_str());
			lvi.pszText = text;
			ListView_SetItem(hctl, &lvi);

			lvi.mask = LVIF_TEXT;
			lvi.iSubItem = index ++;
			strstr.str("");
			const std::multimap<const std::string, const config*>& abilities_cfg = ut->abilities_cfg();
			for (std::multimap<const std::string, const config*>::const_iterator it = abilities_cfg.begin(); it != abilities_cfg.end(); ++ it) {
				const config& ab_cfg = *(it->second);
				const std::string& id = ab_cfg["id"].str();
				if (!id.empty()) {
					if (strstr.str().empty()) {
						strstr << utf8_2_ansi(ab_cfg["name"].str().c_str());
					} else {
						strstr << ", " << utf8_2_ansi(ab_cfg["name"].str().c_str());
					}
				}
			}
			strcpy(text, strstr.str().c_str());
			lvi.pszText = text;
			ListView_SetItem(hctl, &lvi);
			
		} else if (ns::type == IDM_UTYPE_MTYPE) {
			lvi.mask = LVIF_TEXT;
			lvi.iSubItem = index ++;
			strstr.str("");
			strstr << ut->movementType_id();
			strcpy(text, strstr.str().c_str());
			lvi.pszText = text;
			ListView_SetItem(hctl, &lvi);

			const unit_movement_type &movement_type = ut->movement_type();
			utils::string_map dam_tab = movement_type.damage_table();
			for (std::vector<std::string>::const_iterator it = attack_type_vstr.begin(); it != attack_type_vstr.end(); ++ it) {
				// 使用值
				lvi.iSubItem = index ++;
				strstr.str("");
				utils::string_map::iterator dam_it = dam_tab.find(*it);
				if (dam_it != dam_tab.end()) {
					strstr << 100 - atoi((*dam_it).second.c_str());
				} else {
					strstr << "--";
				}
				strcpy(text, strstr.str().c_str());
				lvi.pszText = text;
				ListView_SetItem(hctl, &lvi);
			}			
			
		} else if (ns::type == IDM_UTYPE_ATTACKI || ns::type == IDM_UTYPE_ATTACKII || ns::type == IDM_UTYPE_ATTACKIII) {
			int attack_index = ns::type - IDM_UTYPE_ATTACKI;
			if (attack_index + 1 > (int)ut->attacks().size()) {
				continue;
			}
			attack_type attack = ut->attacks()[attack_index];

			lvi.mask = LVIF_TEXT;
			lvi.iSubItem = index ++;
			strstr.str("");
			strstr << utf8_2_ansi(dgettext(PACKAGE, attack.id().c_str())) << "(" << attack.id() << ")";
			strcpy(text, strstr.str().c_str());
			lvi.pszText = text;
			ListView_SetItem(hctl, &lvi);

			// 类型
			lvi.mask = LVIF_TEXT;
			lvi.iSubItem = index ++;
			strstr.str("");
			strstr << utf8_2_ansi(dgettext(PACKAGE, attack.type().c_str()));
			strcpy(text, strstr.str().c_str());
			lvi.pszText = text;
			ListView_SetItem(hctl, &lvi);

			// 图标
			lvi.mask = LVIF_TEXT;
			lvi.iSubItem = index ++;
			const std::string& icon = attack.icon();
			strcpy(text, icon.substr(icon.rfind("/") + 1).c_str());
			lvi.pszText = text;
			ListView_SetItem(hctl, &lvi);

			// 距离
			lvi.mask = LVIF_TEXT;
			lvi.iSubItem = index ++;
			strstr.str("");
			int range = -1;
			std::vector<std::string>::const_iterator find_it = std::find(attack_range_vstr.begin(), attack_range_vstr.end(), attack.range());
			if (find_it != attack_range_vstr.end()) {
				range = find_it - attack_range_vstr.begin();
			}
			if (range < 0 || range >= (int)attack_range_vstr.size()) {
				strstr << "无效距离";
			} else {
				strstr << utf8_2_ansi(dgettext(PACKAGE, attack_range_vstr[range].c_str()));
			}
			strcpy(text, strstr.str().c_str());
			lvi.pszText = text;
			ListView_SetItem(hctl, &lvi);

			// 特色
			lvi.mask = LVIF_TEXT;
			lvi.iSubItem = index ++;
			strstr.str("");
			std::set<std::string> specials;
			std::vector<std::string> vstr = utils::split(attack.specials());
			for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
				specials.insert(*it);
			}
			const specials_map& units_specials = unit_types.specials();
			for (std::set<std::string>::const_iterator it2 = specials.begin(); it2 != specials.end(); ++ it2) {
				const config& cfg = units_specials.find(*it2)->second;
				BOOST_FOREACH (const config::any_child &sp, cfg.all_children_range()) {
					if (it2 == specials.begin()) {
						strstr << utf8_2_ansi(sp.cfg["name"].str().c_str());
					} else {
						strstr << "," << utf8_2_ansi(sp.cfg["name"].str().c_str());
					}
				}
			}
			strcpy(text, strstr.str().c_str());
			lvi.pszText = text;
			ListView_SetItem(hctl, &lvi);

			// 损伤
			lvi.mask = LVIF_TEXT;
			lvi.iSubItem = index ++;
			strstr.str("");
			strstr << attack.damage();
			strcpy(text, strstr.str().c_str());
			lvi.pszText = text;
			ListView_SetItem(hctl, &lvi);

			// 次数
			lvi.mask = LVIF_TEXT;
			lvi.iSubItem = index ++;
			strstr.str("");
			strstr << attack.num_attacks();
			strcpy(text, strstr.str().c_str());
			lvi.pszText = text;
			ListView_SetItem(hctl, &lvi);

		}
	}

	return FALSE;
}

void On_DlgVisual2Command(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
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

BOOL CALLBACK DlgVisual2Proc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message) {
	case WM_INITDIALOG:
		return On_DlgVisual2InitDialog(hDlg, (HWND)(wParam), lParam);
	HANDLE_MSG(hDlg, WM_COMMAND, On_DlgVisual2Command);
	}
	
	return FALSE;
}

void On_DlgCoreCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	switch (id) {
	case IDM_ADD:
		OnUTypeAddBt(hdlgP);
		break;
	case IDM_DELETE:
		OnUTypeDelBt(hdlgP);
		break;
	case IDM_EDIT:
		OnUTypeEditBt(hdlgP);
		break;
	case IDM_GENERATE_ITEM2:
		ns::core.generate_utypes();
		posix_print_mb("重生新成所有兵种配置、相关图像成功");
		break;

	case IDM_SAVE:
		ns::core.save(hdlgP);
		break;

	case IDM_UTYPE_GENERAL:
	case IDM_UTYPE_MTYPE:
	case IDM_UTYPE_ATTACKI:
	case IDM_UTYPE_ATTACKII:
	case IDM_UTYPE_ATTACKIII:
		ns::type = id;
		DialogBox(gdmgr._hinst, MAKEINTRESOURCE(IDD_VISUAL2), NULL, DlgVisual2Proc);
		break;
	}
	return;
}

void core_notify_handler_dropdown(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
}

void core_notify_handler_rclick(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	LPNMTREEVIEW			lpnmitem;
	HTREEITEM				htvi;
	TVITEMEX				tvi;
	POINT					point;

	if (lpNMHdr->idFrom != IDC_TV_CORE_EXPLORER) {
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
}

BOOL On_DlgCoreNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	if (lpNMHdr->code == TBN_DROPDOWN) {
		core_notify_handler_dropdown(hdlgP, DlgItem, lpNMHdr);

	} else if (lpNMHdr->code == NM_RCLICK) {
		core_notify_handler_rclick(hdlgP, DlgItem, lpNMHdr);
	}
	return FALSE;
}

void On_DlgCoreDestroy(HWND hdlgP)
{
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

// 根据指示config刷新树形视图控件
void tcore::refresh(HWND hctl)
{
	char text[_MAX_PATH];
	std::stringstream strstr;

	// 1. 删除Treeview中所有项
	TreeView_DeleteAllItems(hctl);

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

	generate_tree();

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

bool core_can_execute_tack(int task)
{
	if (task == TASK_NEW || task == TASK_DELETE || task == TASK_EXPLORER) {
		if (core_get_save_btn()) {
			return false;
		}
	}
	return true;
}