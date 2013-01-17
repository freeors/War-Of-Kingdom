#ifndef __DLGCOREPROC_HPP_
#define __DLGCOREPROC_HPP_

#include "global.hpp"
#include "stdafx.h"
#include <windowsx.h>
#include "config.hpp"
#include "editor.hpp"
#include "hero.hpp"
#include "unit_types.hpp"

#define core_enable_save_btn(fEnable)	ToolBar_EnableButton(gdmgr._htb_core, IDM_SAVE, fEnable)
#define core_get_save_btn()				(ToolBar_GetState(gdmgr._htb_core, IDM_SAVE) & TBSTATE_ENABLED)

extern editor editor_;
extern const std::string null_str;

extern std::vector<std::string> attack_range_vstr;
extern std::vector<std::string> attack_type_vstr;

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
		, max_movement_(-1)
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
		if (max_movement_ != that.max_movement_) return false;
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
	int max_movement_;
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

	void vertify_core_images() const;
	void vertify_healing_anim_images() const;
	void vertify_resistance_anim_images() const;
	void vertify_leading_anim_images() const;
	void vertify_idle_anim_images() const;
	void vertify_attack_anim_melee_images() const;
	void vertify_attack_anim_ranged_images() const;
	
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
	static std::set<hero*> artifical_hero_;

	std::vector<tattack> attacks_;
	tunit_type_ utype_from_cfg_;
	std::vector<const unit_type*> candidate_advances_to_;

private:
	std::vector<tattack> attacks_from_cfg_;
};

class tcore
{
public:
	enum {UNIT_TYPE = 0, TACTIC, TECH, SECTIONS};

	static std::map<int, std::string> name_map;
	static std::map<int, int> idd_map;
	static std::map<int, DLGPROC> dlgproc_map;

	tcore();
	~tcore();

	HWND init_toolbar(HINSTANCE hinst, HWND hdlgP);
	void refresh_utype(HWND hdlgP);
	void refresh_tactic(HWND hdlgP);
	void refresh_technology(HWND hdlgP);
	void refresh_technology2(HWND hdlgP);

	void switch_section(HWND hdlgP, int to, bool force);

	void utype_tree_2_tv(HWND hctl, HTREEITEM htvroot);
	void utype_tree_2_tv_internal(HWND hctl, HTREEITEM htvroot, const std::vector<advance_tree::node>& advances_to, const std::vector<std::string>& advances_from2);

	void technology_tree_2_tv(HWND hctl, HTREEITEM htvroot);
	void technology_tree_2_tv_internal(HWND hctl, HTREEITEM htvroot, const std::vector<advance_tree::node>& advances_to, const std::vector<std::string>& advances_from2);

	bool new_utype();
	void erase_utype(int index, HWND hdlgP);

	bool types_dirty() const;

	enum {BIT_UTYPE = 0, BIT_CHARACTER};
	void set_dirty(int bit, bool set);

	bool save_if_dirty();
	void save(HWND hdlgP);
	void generate_utypes() const;

private:
	void generate_utype_tree();

public:
	int section_;
	// second: unit type that can advance to
	std::vector<std::pair<std::string, std::vector<std::string> > > tv_tree_;
	std::vector<std::pair<std::string, std::vector<std::string> > > technology_tv_;

	gamemap* map_;
	t_translation::t_list alias_terrains_;

	HTREEITEM htvroot_utype_;
	HTREEITEM htvroot_technology_;

	std::map<int, tunit_type> types_updating_;

	HTREEITEM htvroot_tactic_atom_;
	HTREEITEM htvroot_tactic_complex_;

	HTREEITEM htvroot_technology_atom_;
	HTREEITEM htvroot_technology_complex_;

	uint32_t dirty_;
};

namespace ns {
	extern const std::string default_utype_textdomain;
	extern const std::string default_utype_id;
	extern const std::string default_attack_id;
	extern const std::string default_attack_icon;

	extern tcore core;
	extern tunit_type utype;

	extern int clicked_utype;
	extern int clicked_attack;
	extern int clicked_mtype;
	
	extern int action_utype;
	extern int action_attack;

	extern int type;
}

extern BOOL CALLBACK DlgUTypeProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam);
extern BOOL CALLBACK DlgTacticProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam);
extern BOOL CALLBACK DlgTechnologyProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam);

#endif // __DLGCOREPROC_HPP_
