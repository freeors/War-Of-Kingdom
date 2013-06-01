#ifndef __DLGCOREPROC_HPP_
#define __DLGCOREPROC_HPP_

#include "global.hpp"
#include "stdafx.h"
#include <windowsx.h>
#include "config.hpp"
#include "editor.hpp"
#include "hero.hpp"
#include "unit_types.hpp"
#include "terrain.hpp"
#include "dlgbuilderproc.hpp"

class terrain_builder;

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
		, raw_icon_()
		, alignment_(unit_type::LAWFUL)
		, character_(NO_ESPECIAL)
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
		, multi_grid_(false)
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
		if (raw_icon_ != that.raw_icon_) return false;
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
		if (multi_grid_ != that.multi_grid_) return false;
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
	std::string raw_icon_;
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
	bool multi_grid_;

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

	enum {TYPE_TROOP = 0, TYPE_COMMONER, TYPE_CITY, TYPE_ARTIFICAL};
	int type() const;
	int type_from_cfg() const;

	bool dirty() const;

public:
	static std::map<int, std::string> type_map_;
	static std::set<hero*> artifical_hero_;
	static std::set<hero*> commoner_hero_;

	std::vector<tattack> attacks_;
	tunit_type_ utype_from_cfg_;
	std::vector<const unit_type*> candidate_advances_to_;

private:
	std::vector<tattack> attacks_from_cfg_;
};

class tterrain_
{
public:
	tterrain_() 
		: textdomain_(PACKAGE "-lib")
		, symbol_image_()
		, editor_image_()
		, string_()
		, id_()
		, name_()
		, editor_name_()
		, aliasof_()
		, mvt_alias_()
		, def_alias_()
		, height_adjust_(0)
		, submerge_(0)
		, light_modification_(0)
		, max_light_(0)
		, min_light_(0)
		, heals_(0)
		, village_(false)
		, editor_group_()
		, editor_default_base_()
		, hide_in_editor_(false)
	{}

	bool operator==(const tterrain_& that) const
	{
		if (textdomain_ != that.textdomain_) return false;
		if (symbol_image_ != that.symbol_image_) return false;
		if (editor_image_ != that.editor_image_) return false;
		if (string_ != that.string_) return false;
		if (id_ != that.id_) return false;
		if (name_ != that.name_) return false;
		if (editor_name_ != that.editor_name_) return false;
		if (aliasof_ != that.aliasof_) return false;
		if (mvt_alias_ != that.mvt_alias_) return false;
		if (def_alias_ != that.def_alias_) return false;
		if (height_adjust_ != that.height_adjust_) return false;
		if (submerge_ != that.submerge_) return false;
		if (light_modification_ != that.light_modification_) return false;
		if (max_light_ != that.max_light_) return false;
		if (min_light_ != that.min_light_) return false;
		if (heals_ != that.heals_) return false;
		if (village_ != that.village_) return false;
		if (editor_group_ != that.editor_group_) return false;
		if (editor_default_base_ != that.editor_default_base_) return false;
		if (hide_in_editor_ != that.hide_in_editor_) return false;
		return true;
	}
	bool operator!=(const tterrain_& that) const { return !operator==(that); }

public:
	std::string textdomain_;
	std::string symbol_image_;
	std::string string_;
	std::string id_;
	std::string name_;
	std::string editor_name_;
	std::string aliasof_;
	std::string mvt_alias_;
	std::string def_alias_;
	int height_adjust_;
	double submerge_;
	int light_modification_;
	int max_light_;
	int min_light_;
	int heals_;
	bool village_;
	std::string editor_group_;
	std::string editor_image_;
	std::string editor_default_base_;
	bool hide_in_editor_;
};

class tterrain: public tterrain_
{
public:
	tterrain()
		: tterrain_()
		, terrain_from_cfg_()
		, edit_invalid_(0)
	{}

	void from_config(const terrain_type& info);
	void from_ui(HWND hdlgP);
	void from_ui_terrain_edit(HWND hdlgP);
	void from_ui_utype_type(HWND hdlgP);
	void update_to_ui_terrain_edit(HWND hdlgP) const;
	void update_to_ui_utype_edit_mtype(HWND hdlgP) const;
	void update_to_ui_utype_edit_type(HWND hdlgP) const;
	void update_to_ui_mtype_edit(HWND hdlgP) const;
	void update_to_ui_utype_type(HWND hdlgP) const;
	void generate(std::stringstream& strstr) const;

	bool dirty() const;

	enum {EDITBIT_NONE = 0, EDITBIT_STRING, EDITBIT_ID, EDITBIT_NAME,
	EDITBIT_ALIAS, EDITBIT_MVTALIAS, EDITBIT_DEFALIAS};
	void set_edit_invalid(int bit, bool set, HWND ok);

public:
	tterrain_ terrain_from_cfg_;
	int edit_invalid_;
};

class tfaction_
{
public:
	tfaction_() 
		: leader_(HEROS_INVALID_NUMBER)
		, city_(HEROS_INVALID_NUMBER)
		, freshes_()
		, wanderes_()
	{}

	bool operator==(const tfaction_& that) const
	{
		if (leader_ != that.leader_) return false;
		if (city_ != that.city_) return false;
		if (freshes_ != that.freshes_) return false;
		if (wanderes_ != that.wanderes_) return false;
		return true;
	}
	bool operator!=(const tfaction_& that) const { return !operator==(that); }

public:
	int leader_;
	int city_;
	std::set<int> freshes_;
	std::set<int> wanderes_;
};

class tfaction: public tfaction_
{
public:
	tfaction()
		: tfaction_()
		, faction_from_cfg_()
	{}

	void from_config(const config& cfg, std::set<int>& used_heros, std::set<int>& used_cities);
	void from_ui(HWND hdlgP);
	void from_ui_faction_edit(HWND hdlgP);
	void update_to_ui_faction_edit(HWND hdlgP) const;
	void update_to_ui_row(HWND hdlgP) const;
	std::string generate() const;

	bool dirty() const;

public:
	tfaction_ faction_from_cfg_;
};

class tcore
{
public:
	enum {UNIT_TYPE = 0, TACTIC, CHARACTER, DECREE, TECH, TREASURE, TERRAIN, BUILDER, FACTION, SECTIONS};

	static std::map<int, std::string> name_map;
	static std::map<int, int> idd_map;
	static std::map<int, DLGPROC> dlgproc_map;

	tcore();
	~tcore();

	HWND init_toolbar(HINSTANCE hinst, HWND hdlgP);
	void refresh_utype(HWND hdlgP);
	void refresh_tactic(HWND hdlgP);
	void refresh_character(HWND hdlgP);
	void refresh_decree(HWND hdlgP);
	void refresh_technology(HWND hdlgP);
	void refresh_treasure(HWND hdlgP);
	void refresh_terrain(HWND hdlgP);
	void refresh_builder(HWND hdlgP);
	void refresh_faction(HWND hdlgP);

	void init_cache();
	void switch_section(HWND hdlgP, int to, bool init);

	void utype_tree_2_tv(HWND hctl, HTREEITEM htvroot);
	void utype_tree_2_tv_internal(HWND hctl, HTREEITEM htvroot, const std::vector<advance_tree::node>& advances_to, const std::vector<std::string>& advances_from2);

	void technology_tree_2_tv(HWND hctl, HTREEITEM htvroot);
	void technology_tree_2_tv_internal(HWND hctl, HTREEITEM htvroot, const std::vector<advance_tree::node>& advances_to, const std::vector<std::string>& advances_from2);

	bool new_utype();
	void erase_utype(int index, HWND hdlgP);

	bool types_dirty() const;

	enum {BIT_UTYPE = 0, BIT_CHARACTER, BIT_TREASURE, BIT_TERRAIN, BIT_BUILDER, BIT_FACTION};
	void set_dirty(int bit, bool set);
	bool bit_dirty(int bit) const;

	bool save_if_dirty();
	void save(HWND hdlgP);
	void generate_utypes() const;

	// section: treasure
	void update_to_ui_treasure(HWND hdlgP);
	void update_to_ui_treasure_edit(HWND hdlgP);
	void from_ui_treasure_edit(HWND hdlgP);
	bool treasures_dirty() const { return treasures_from_cfg_ != treasures_updating_; }

	// section: terrain
	void update_to_ui_terrain(HWND hdlgP);
	// void update_to_ui_treasure_edit(HWND hdlgP);
	// void from_ui_treasure_edit(HWND hdlgP);
	// bool treasures_dirty() const { return treasures_from_cfg_ != treasures_updating_; }

	// section: builder
	void update_to_ui_builder(HWND hdlgP);
	void release_builder();
	void copy_brules(const std::vector<tbuilding_rule*>& src, std::vector<tbuilding_rule*>& dst);
	bool brules_dirty() const;
	std::string terrain_graphics_tpl(bool absolute = false) const;
	std::string terrain_graphics_cfg(bool absolute = false) const;
	void generate_terrain_graphics_cfg() const;
		
	// section: faction
	void update_to_ui_faction(HWND hdlgP, int clicked_faction);
	bool factions_dirty() const;
	std::string factions_cfg(bool absolute = false) const;
	void generate_factions_cfg() const;
	
private:
	std::string units_internal(bool absolute = false) const;
	void generate_units_internal() const;
	std::string terrain_cfg(bool absolute = false) const;
	void generate_terrain_cfg() const;

	void generate_utype_tree();

public:
	int section_;
	// second: unit type that can advance to
	std::vector<std::pair<std::string, std::vector<std::string> > > tv_tree_;
	std::vector<std::pair<std::string, std::vector<std::string> > > technology_tv_;

	gamemap* map_;
	t_translation::t_list terrains_;
	t_translation::t_list alias_terrains_;
	terrain_builder* builder_;

	std::map<int, tunit_type> types_updating_;

	std::map<int, int> treasures_from_cfg_;
	std::map<int, int> treasures_updating_;

	std::vector<tterrain> terrains_from_cfg_;
	std::vector<tterrain> terrains_updating_;

	std::vector<tbuilding_rule*> brules_from_cfg_;
	std::vector<tbuilding_rule*> brules_updating_;

	std::vector<tfaction> factions_from_cfg_;
	std::vector<tfaction> factions_updating_;

	HTREEITEM htvroot_utype_;
	HTREEITEM htvroot_technology_;

	HTREEITEM htvroot_tactic_atom_;
	HTREEITEM htvroot_tactic_complex_;

	HTREEITEM htvroot_character_atom_;
	HTREEITEM htvroot_character_complex_;

	HTREEITEM htvroot_decree_atom_;
	HTREEITEM htvroot_decree_complex_;

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
	extern int clicked_treasure;
	
	extern int action_utype;
	extern int action_attack;
	extern int action_treasure;

	extern int type;
	extern int clicked_hero;
}

BOOL CALLBACK DlgUTypeProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK DlgTacticProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK DlgCharacterProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK DlgDecreeProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK DlgTechnologyProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK DlgTreasureProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK DlgTerrainProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK DlgBuilderProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK DlgFactionProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam);

#endif // __DLGCOREPROC_HPP_
