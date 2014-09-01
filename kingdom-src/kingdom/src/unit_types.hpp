/* $Id: unit_types.hpp 47388 2010-11-03 07:19:44Z silene $ */
/*
   Copyright (C) 2003 - 2010 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/
#ifndef UNIT_TYPES_H_INCLUDED
#define UNIT_TYPES_H_INCLUDED

#include "unit_animation.hpp"
#include "race.hpp"
#include "hero.hpp"

class gamemap;
class unit;
class artifical;
class unit_ability_list;
class unit_map;
class unit_type_data;

class department
{
public:
	enum { commercial = 0, technology };
	department(int type, const std::string& name, const std::string& image, const std::string& portrait);
	
	int type_;
	std::string name_;
	std::string image_;
	std::string portrait_;
	int exploiture_;
};

std::string form_title_str(int title);

class tespecial 
{
public:
	tespecial(int index, const std::string& id);

	int index_;
	std::string id_;
	std::string name_;
	std::string image_;
};

class tgenus
{
public:
	enum genus_t { NONE, TURN_BASED = 0x1, HALF_REALTIME = 0x2 };
	static std::map<const std::string, genus_t> tags;
	static std::string name_prefix_str;

	static void fill_tags();
	static genus_t find(const std::string& tag);

	tgenus(const config& cfg);

	genus_t index() const { return index_; }
	const std::string& id() const { return id_; }
	const std::string& name() const { return name_; }
	const std::string& icon() const { return icon_; }

private:
	genus_t index_;
	std::string id_;
	std::string name_;
	std::string icon_;
};

namespace mode_tag {
	enum tmode {NONE, SCENARIO = 0x1, RPG = 0x2, TOWER = 0x4, SIEGE = 0x8, LAYOUT = 0x10};
	extern std::map<const std::string, tmode> tags;
	tmode find(const std::string& tag);
	const std::string& rfind(tmode tag);
}

namespace controller_tag {
	enum CONTROLLER {NONE, HUMAN = 0x1, HUMAN_AI = 0x2, AI = 0x4, NETWORK = 0x8, EMPTY = 0x10};

	extern std::map<const std::string, CONTROLLER> tags;
	extern std::set<CONTROLLER> profile_side_tags;
	extern std::set<CONTROLLER> game_running_tags;

	CONTROLLER find(const std::string& tag);
	const std::string& rfind(CONTROLLER tag);
	bool valid(const std::string& str);
	bool filter(int controller, const std::string& filters_str);
	bool filter(int controller, const int filters);
}

namespace apply_to_tag {
	enum {NONE, ATTACK, HITPOINTS, MOVEMENT, MUNITION, MAX_EXPERIENCE,
		LOYAL, STATUS, MOVEMENT_COSTS, DEFENSE, RESISTANCE, ENCOURAGE,
		DEMOLISH, ZOC, IMAGE_MOD, ADVANCE, TRAIN, MOVE, ACTION,
		DAMAGE, ONEOFF_MIN = DAMAGE, HIDE, ALERT, PROVOKE, CLEAR, HEAL, REVIVAL, ONEOFF_MAX = REVIVAL,
		DECREE, UNIT_END = DECREE,
		// side <=
		CIVILIZATION, SPIRIT, STRATEGIC, STRATAGEM,
		// character <=
		AGGRESSIVE, CHARACTER_MIN = AGGRESSIVE, UNITED, CHARISMATIC, CREATIVE, 
		EXPANSIVE, FINANCIAL, INDUSTRIOUS, ORGANIZED, PROTECTIVE,
		PHILOSOPHICAL, SPIRITUAL, CHARACTER_MAX = SPIRITUAL,
		// decree <=
		BUSINESS, TECH,
	};

	extern std::map<const std::string, int> tags;
	int find(const std::string& tag);
}

namespace sound_filter_tag {
	enum {NONE, MALE, FEMALE, NEUTRAL};

	extern std::map<const std::string, int> tags;
	int find(const std::string& tag);
	const std::string& rfind(int tag);
	std::string filter(const std::string& src, const std::string& f);

	int from_hero_gender(int gender);
}

namespace family_tag {
	enum {NONE, TROOP = 1, ARTIFICAL = 2, CITY = 4};

	extern std::map<const std::string, int> tags;

	int find(const std::string& tag);
	const std::string& rfind(int tag);
	bool valid(const std::string& str);
	bool filter(const unit& u, const std::string& filters_str);
	bool filter(const unit& u, int filters);
}

namespace ustate_tag {
	enum state_t { NONE, SLOWED = 0x1, BROKEN = 0x2, POISONED = 0x4, PETRIFIED = 0x8,
		UNCOVERED = 0x10, FORMATION_ATTACKED = 0x20, LEGERITIED = 0x40, EXPEDITED = 0x80, 
		BLOCKED = 0x100, ROBBER = 0x200, DEPUTE = 0x400, BUILDING = 0x800,
		REVIVALED = 0x1000
	};

	extern std::map<const std::string, state_t> tags;
	extern std::set<ustate_tag::state_t> profilable_tags;

	state_t find(const std::string& tag);
	const std::string& rfind(state_t tag);
	bool valid(const std::string& str);
}

namespace global_anim_tag {
	enum ttype {NONE, CARD, REINFORCE, INDIVIDUALITY, TACTIC, 
		BLADE, PIERCE, IMPACT, ARCHERY, COLLAPSE, ARCANE, FIRE, 
		COLD, STRIKE, MAGIC, HEAL, DESTRUCT, 
		FORMATION_ATTACK, FORMATION_DEFEND, PASS_SCENARIO, PERFECT, INCOME,
		STRATAGEM_UP, STRATAGEM_DOWN, LOCATION, HSCROLL_TEXT,
		TITLE_SCREEN, LOAD_SCENARIO, FLAGS, TEXT, PLACE};

	extern std::map<const std::string, ttype> tags;
	ttype find(const std::string& tag);
	const std::string& rfind(ttype tag);
}

class ttactic 
{
public:
	static int min_complex_index;

	enum {ATTACK = 0, RESISTANCE, ENCOURAGE, DEMOLISH};
	enum {SELF = 0x1, FRIEND = 0x2, ENEMY = 0x4};

	static std::map<int, std::string> type_name_map;
	static std::map<std::string, int> range_id_map;

	static int calculate_turn(int force, int intellect);
	static bool select_one(int apply_to);

	ttactic()
		: index_(-1)
		, id_()
		, range_(0)
		, point_(0)
		, level_(0)
		, name_()
		, description_()
		, bg_image_()
		, apply_to_(apply_to_tag::NONE)
		, type_filter_(0)
		, parts_()
		, self_profit_(0)
		, self_hide_profit_(0)
		, self_alert_profit_(0)
		, self_clear_profit_(0)
		, self_heal_profit_(0)
		, friend_profit_(0)
		, friend_hide_profit_(0)
		, friend_clear_profit_(0)
		, friend_heal_profit_(0)
		, friend_select_one_profit_(0)
		, enemy_profit_(0)
		, enemy_provoke_profit_(0)
		, complex_(false)
	{}

	ttactic(int index, int complex_index, const config& cfg);

	int index() const { return index_; }
	const std::string& id() const { return id_; }
	const std::string& name() const { return name_; }
	const std::string& description() const { return description_; }
	const std::string& bg_image() const { return bg_image_; }
	const std::vector<const ttactic*>& parts() const { return parts_; }

	const config& effect_cfg() const { return effect_cfg_; }
	int apply_to() const { return apply_to_; }
	int type_filter() const { return type_filter_; }

	int range() const { return range_; }
	int point() const { return point_; }
	int level() const { return level_; }
	int cost() const;

	int self_profit() const { return self_profit_; }
	int self_hide_profit() const { return self_hide_profit_; }
	int self_alert_profit() const { return self_alert_profit_; }
	int self_clear_profit() const { return self_clear_profit_; }
	int self_heal_profit() const { return self_heal_profit_; }
	int friend_profit() const { return friend_profit_; }
	int friend_hide_profit() const { return friend_hide_profit_; }
	int friend_clear_profit() const { return friend_clear_profit_; }
	int friend_heal_profit() const { return friend_heal_profit_; }
	int friend_select_one_profit() const { return friend_select_one_profit_; }
	int enemy_profit() const { return enemy_profit_; }
	int enemy_provoke_profit() const { return enemy_provoke_profit_; }
	bool complex() const { return complex_; }
	bool oneoff() const;
	bool select_one() const;
	int select_one_internal() const;

	void set_atom_part() { parts_.push_back(this); }

	std::vector<std::pair<const ttactic*, std::vector<map_location> > > touch_locs(const map_location& loc) const;
	std::map<int, std::vector<map_location> > touch_units(unit_map& units, unit& u) const;
private:
	int index_;
	std::string id_;
	std::string name_;
	std::string description_;
	std::string bg_image_;
	int point_;
	int level_;
	std::vector<const ttactic*> parts_;
	int range_;
	config effect_cfg_;
	int apply_to_;
	int type_filter_;

	int self_profit_;
	int self_hide_profit_;
	int self_alert_profit_;
	int self_clear_profit_;
	int self_heal_profit_;
	int friend_profit_;
	int friend_hide_profit_;
	int friend_clear_profit_;
	int friend_heal_profit_;
	int friend_select_one_profit_;
	int enemy_profit_;
	int enemy_provoke_profit_;

	bool complex_;
};

class tcharacter_
{
public:
	tcharacter_():
		leadership_(0)
		, force_(0)
		, intellect_(0)
		, spirit_(0)
		, charm_(0)
	{}
	std::string expression() const;

public:
	int leadership_;
	int force_;
	int intellect_;
	int spirit_;
	int charm_;
};

class tcharacter: public tcharacter_
{
public:
	static int min_complex_index;

	tcharacter()
		: index_(-1)
		, id_()
		, name_()
		, description_()
		, apply_to_(apply_to_tag::NONE)
		, parts_()
		, complex_(false)
	{}

	tcharacter(int index, int complex_index, const config& cfg);

	int index() const { return index_; }
	const std::string& id() const { return id_; }
	const std::string& name() const { return name_; }
	const std::string& description() const { return description_; }
	const std::vector<const tcharacter*>& parts() const { return parts_; }
	bool complex() const { return complex_; }

	int apply_to() const { return apply_to_; }
	const config& effect_cfg() const { return effect_cfg_; }
	int level(const hero& h) const;

	void set_atom_part() { parts_.push_back(this); }
private:
	int index_;
	std::string id_;
	std::string name_;
	std::string description_;
	std::vector<const tcharacter*> parts_;
	int apply_to_;
	config effect_cfg_;

	bool complex_;
};

class tdecree: public tcharacter_
{
public:
	static int min_complex_index;

	tdecree()
		: index_(-1)
		, id_()
		, name_()
		, description_()
		, apply_to_(apply_to_tag::NONE)
		, parts_()
		, front_(false)
		, min_level_(0)
		, require_artifical_()
		, complex_(false)
	{}

	tdecree(int index, int complex_index, const config& cfg);

	int index() const { return index_; }
	const std::string& id() const { return id_; }
	const std::string& name() const { return name_; }
	const std::string& description() const { return description_; }
	const std::vector<const tdecree*>& parts() const { return parts_; }
	bool complex() const { return complex_; }

	int apply_to() const { return apply_to_; }
	const config& effect_cfg() const { return effect_cfg_; }
	int level(const hero& h) const;
	bool front() const { return front_; }
	int min_level() const { return min_level_; }
	const std::set<int>& require_artifical() const { return require_artifical_; }

	void set_atom_part() { parts_.push_back(this); }
private:
	int index_;
	std::string id_;
	std::string name_;
	std::string description_;
	std::vector<const tdecree*> parts_;
	int apply_to_;
	config effect_cfg_;

	// filter
	bool front_;
	int min_level_;
	std::set<int> require_artifical_;

	bool complex_;
};

class tnoble
{
public:
	struct tcondition {
		int city;
		int meritorious;
	};

	static std::string form_name_msgid(const std::string& id);
	static std::string form_name(const std::string& id);

	tnoble()
		: index_(-1)
		, id_()
		, level_(-1)
		, leader_(false)
		, formation_(false)
		, name_()
	{
		memset(&condition_, 0, sizeof(hblock));
		memset(&effect_, 0, sizeof(hblock));
	}
	tnoble(int index, const config& cfg);

	const std::string& id() const { return id_; }
	int index() const { return index_; }
	int level() const { return level_; }
	bool leader() const { return leader_; }
	bool formation() const { return formation_; }
	const std::string& name() const { return name_; }

	const tcondition& condition() const { return condition_; }
	const hblock& effect() const { return effect_; }

	bool operator<(const tnoble& that) const;

private:
	int index_;
	std::string id_;
	int level_;
	bool leader_;
	bool formation_;
	tcondition condition_;
	hblock effect_;
	std::string name_;
};

class ttreasure
{
public:
	ttreasure()
		: index_(-1)
		, feature_(0)
		, name_()
		, image_()
	{}
	ttreasure(int index, int feature);

	int index() const { return index_; }
	int feature() const { return feature_; }
	const std::string& name() const { return name_; }
	const std::string& image() const { return image_; }
private:
	int index_;
	int feature_;
	std::string name_;
	std::string image_;
};

namespace advance_tree {

class base
{
public:
	virtual const std::string& id() const = 0;
	virtual const std::vector<std::string>& advances_to() const = 0;
};

struct node {
	node(const base* ut)
		: current(ut)
		, advances_to()
	{}

	const base* current;
	std::vector<node> advances_to;
};

void generate_advance_tree(const std::map<std::string, const base*>& src, std::vector<node*>& dst);
void generate_advance_tree_internal(const base* current, std::vector<node>& advances_to, bool& to_branch);
void hang_branch_internal(std::vector<node*>& dst, std::vector<node>& advances_to, bool& hang_branch);
}

class ttechnology : public advance_tree::base
{
public:
	enum {NONE = 0, MODIFY, FINISH};
	
	ttechnology()
		: id_()
		, advances_to_()
		, name_()
		, description_()
		, occasion_(NONE)
		, max_experience_(0)
		, apply_to_(apply_to_tag::NONE)
		, relative_(HEROS_NO_CHARACTER)
		, parts_()
		, stratagem_(false)
		, complex_(false)
		, type_filter_(0)
		, arms_filter_(0)
		, mode_filter_(0)
	{}

	ttechnology(const config& cfg);

	const std::string& id() const { return id_; }
	const std::vector<std::string>& advances_to() const { return advances_to_; }
	const std::string& name() const { return name_; }
	const std::string& description() const { return description_; }
	int occasion() const { return occasion_; }
	const std::vector<const ttechnology*>& parts() const { return parts_; }

	int max_experience() const { return max_experience_; }

	int relative() const { return relative_; }
	const config& effect_cfg() const { return effect_cfg_; }
	int apply_to() const { return apply_to_; }

	bool stratagem() const { return stratagem_; }
	bool complex() const { return complex_; }

	void set_atom_part() { parts_.push_back(this); }

	int type_filter() const { return type_filter_; }
	int arms_filter() const { return arms_filter_; }
	int mode_filter() const { return mode_filter_; }
	bool filter(int type, int arms) const;
	bool filter_mode(int mode) const;
private:
	std::string id_;
	std::vector<std::string> advances_to_;
	std::string name_;
	std::string description_;
	int occasion_;
	int max_experience_;
	std::vector<const ttechnology*> parts_;
	int relative_;
	config effect_cfg_;
	int apply_to_;
	int type_filter_;
	int arms_filter_;
	int mode_filter_;
	
	bool stratagem_;
	bool complex_;
};
extern ttechnology null_technology;

class tformation_profile
{
public:
	enum {SLOW = 0x1, BREAK = 0x2, POISON = 0x4};
	
	tformation_profile()
		: index_(-1)
		, id_()
		, name_()
		, description_()
		, attack_flag_(0)
		, arms_type_()
		, arms_range_id_()
		, arms_range_(0)
		, arms_count_(0)
		, icon_()
		, bg_image_()
	{}

	tformation_profile(int index, const config& cfg);

	int index() const { return index_; }
	const std::string& id() const { return id_; }
	const std::string& name() const { return name_; }
	const std::string& description() const { return description_; }

	const std::string icon() const { return icon_; }
	const std::string bg_image() const { return bg_image_; }

public:
	int attack_flag_;

	std::set<std::string> arms_type_;
	std::string arms_range_id_;
	int arms_range_;
	int arms_count_;

private:
	int index_;
	std::string id_;
	std::string name_;
	std::string description_;

	std::string icon_;
	std::string bg_image_;
};

//and how much damage it does.
class attack_type
{
public:
	enum {MELEE = 0, RANGED, CAST};

	attack_type(const config& cfg);
	/** Default implementation, but defined out-of-line for efficiency reasons. */
	~attack_type();
	const t_string& name() const { return description_; }
	const std::string& id() const { return id_; }
	const std::string& type() const { return type_; }
	const std::string& icon() const { return icon_; }
	const std::string& range() const { return range_; }
	const std::string& specials() const { return specials_; }
	std::string accuracy_parry_description() const;
	int accuracy() const { return accuracy_; }
	int parry() const { return parry_; }
	int damage() const { return damage_; }
	int num_attacks() const { return num_attacks_; }
	double attack_weight() const { return attack_weight_; }
	double defense_weight() const { return defense_weight_; }

	bool get_special_bool(const std::string& special,bool force=false) const;
	unit_ability_list get_specials(const std::string& special) const;
	std::vector<t_string> special_tooltips(bool force=false) const;
	std::string weapon_specials(bool force=false) const;
	void set_specials_context(const unit* aloc,const unit* dloc,
		const unit_map &unitmap, bool attacker, const attack_type *other_attack) const;
	void set_specials_context(const unit* loc,const unit* dloc, const unit& un, bool attacker =true) const;

	bool matches_filter(const config& cfg,bool self=false) const;
	bool apply_modification(const config& cfg,std::string* description);
	bool describe_modification(const config& cfg,std::string* description);
	void apply_modification_damage(int increase_damage);
	void apply_modification_damage2(double increase_percent);
	void set_damage(int damage);

	int movement_used() const { return cfg_["movement_used"].to_int(100000); }

	config& get_cfg() { return cfg_; }
	const config& get_cfg() const { return cfg_; }
	mutable unit* aloc_;
	mutable unit* dloc_;
	mutable bool attacker_;
	mutable const unit_map* unitmap_;
	mutable const attack_type* other_attack_;
	/*
	 * cfg: a weapon special WML structure
	 */
	bool special_active(const config& cfg, bool self) const;
	bool special_affects_opponent(const config& cfg) const;
	bool special_affects_self(const config& cfg) const;

	//this function returns a random animation out of the possible
	//animations for this attack. It will not return the same attack
	//each time.
	const unit_animation* animation(const game_display& disp, const map_location& loc,const unit* my_unit,const unit_animation::hit_type hit,const attack_type* secondary_attack,int swing_num,int damage) const;
private:
	config cfg_;
	t_string description_;
	std::string id_;
	std::string type_;
	std::string icon_;
	std::string range_;
	std::string specials_;
	int damage_;
	int num_attacks_;
	double attack_weight_;
	double defense_weight_;
	
	int accuracy_;
	int parry_;
};

class unit_movement_type;

/**
 * Possible range of the defense. When a single value is needed, #max_
 * (maximum defense) is selected, unless #min_ is bigger.
 */
struct defense_range
{
	int min_, max_;
};

typedef std::map<t_translation::t_terrain, defense_range> defense_cache;

const defense_range &defense_range_modifier_internal(defense_cache &defense_mods,
	const config &cfg, const unit_movement_type *parent,
	const gamemap &map, t_translation::t_terrain terrain, int recurse_count = 0);

int defense_modifier_internal(defense_cache &defense_mods,
	const config &cfg, const unit_movement_type *parent,
	const gamemap &map, t_translation::t_terrain terrain, int recurse_count = 0);

int movement_cost_internal(std::map<t_translation::t_terrain, int> &move_costs,
	const config &cfg, const unit_movement_type *parent,
	const gamemap &map, t_translation::t_terrain terrain, int recurse_count = 0);

//the 'unit movement type' is the basic size of the unit - flying, small land,
//large land, etc etc.
class unit_movement_type
{
public:
        //this move distance means a hex is unreachable
	//if there is an UNREACHABLE macro declared in the data tree
	//it should match this value.
	static const int UNREACHABLE = 99;

	//this class assumes that the passed in reference will remain valid
	//for at least as long as the class instance
	unit_movement_type(const config& cfg, const unit_movement_type* parent=NULL);
	unit_movement_type();

	std::string name() const;
	int movement_cost(const gamemap &map, t_translation::t_terrain terrain) const
	{ return movement_cost_internal(moveCosts_, cfg_, parent_, map, terrain); }
	int defense_modifier(const gamemap &map, t_translation::t_terrain terrain) const
	{ return defense_modifier_internal(defenseMods_, cfg_, parent_, map, terrain); }
	const defense_range &defense_range_modifier(const gamemap &map, t_translation::t_terrain terrain) const
	{ return defense_range_modifier_internal(defenseMods_, cfg_, parent_, map, terrain); }
	int damage_against(const attack_type& attack) const { return resistance_against(attack); }
	int resistance_against(const attack_type& attack) const;

	utils::string_map damage_table() const;

	void set_parent(const unit_movement_type* parent) { parent_ = parent; }

	bool is_flying() const;

	const config& get_cfg() const { return cfg_; }
	const unit_movement_type* get_parent() const { return parent_; }
private:
	mutable std::map<t_translation::t_terrain, int> moveCosts_;
	mutable defense_cache defenseMods_;

	const unit_movement_type* parent_;

	bool is_flying_;
	std::string name_;

	config cfg_;
};

typedef std::map<std::string, unit_movement_type> movement_type_map;
typedef std::map<std::string, const config> traits_map;
typedef std::map<std::string, const config> modifications_map;
typedef std::map<int, std::vector<int> > complex_feature_map;
typedef std::map<std::string, const config> abilities_map;
typedef std::map<std::string, const config> specials_map;
typedef std::vector<std::string> navigation_types;

#define NO_GUARD				-1

class unit_type : public advance_tree::base
{
public:
	friend class unit;
	friend class artifical;
	friend class unit_type_data;

	static std::string image(const std::string& race, const std::string& id, bool terrain);
	static std::string hit(const std::string& race, const std::string& id, bool terrain);
	static std::string miss(const std::string& race, const std::string& id, bool terrain);
	static std::string leading(const std::string& race, const std::string& id, bool terrain);
	static std::string idle(const std::string& race, const std::string& id, bool terrain, int number);
	static std::string move(const std::string& race, const std::string& id, bool terrain, int number);
	static std::string build(const std::string& race, const std::string& id, bool terrain, int number);
	static std::string repair(const std::string& race, const std::string& id, bool terrain, int number);
	static std::string die(const std::string& race, const std::string& id, bool terrain, int number);
	static std::string attack_image(const std::string& race, const std::string& id, bool terrain, int range, int number);

	static bool has_resistance_anim(const std::set<std::string>& abilities);
	static bool has_leading_anim(const std::set<std::string>& abilities);
	static bool has_healing_anim(const std::set<std::string>& abilities);

	static void defend_anim(const std::string& tag, const std::string& race, const std::string& id, bool terrain, config& cfg);
	static void resistance_anim(const std::string& tag, const std::string& race, const std::string& id, bool terrain, config& cfg);
	static void leading_anim(const std::string& tag, const std::string& race, const std::string& id, bool terrain, config& cfg);
	static void healing_anim(const std::string& tag, const std::string& race, const std::string& id, bool terrain, config& cfg);
	static void idle_anim(const std::string& tag, const std::string& race, const std::string& id, bool terrain, const std::string& idle_sound, config& cfg, bool multigrid);
	static void healed_anim(const std::string& tag, const std::string& race, const std::string& id, bool terrain, config& cfg);
	static void movement_anim(const std::string& tag, const std::string& race, const std::string& id, bool terrain, const std::string& movement_sound, config& cfg);
	static void build_anim(const std::string& tag, const std::string& race, const std::string& id, bool terrain, config& cfg);
	static void repair_anim(const std::string& tag, const std::string& race, const std::string& id, bool terrain, config& cfg);
	static void die_anim(const std::string& tag, const std::string& race, const std::string& id, bool terrain, const std::string& die_sound, config& cfg, bool multigrid);
	static void attack_anim_melee(const std::string& tag, const std::string& aid, const std::string& aicon, bool troop, const std::string& race, const std::string& id, bool terrain, config& cfg);
	static void attack_anim_multi_melee(const std::string& tag, const std::string& aid, const std::string& aicon, bool troop, const std::string& race, const std::string& id, bool terrain, config& cfg);
	static void attack_anim_ranged(const std::string& tag, const std::string& aid, const std::string& aicon, const std::string& race, const std::string& id, bool terrain, config& cfg);
	static void attack_anim_multi_ranged(const std::string& tag, const std::string& aid, const std::string& aicon, const std::string& race, const std::string& id, bool terrain, config& cfg);
	static void attack_anim_ranged_magic_missile(const std::string& tag, const std::string& aid, const std::string& race, const std::string& id, bool terrain, config& cfg);
	static void attack_anim_ranged_lightbeam(const std::string& tag, const std::string& aid, const std::string& race, const std::string& id, bool terrain, config& cfg);
	static void attack_anim_ranged_fireball(const std::string& tag, const std::string& aid, const std::string& race, const std::string& id, bool terrain, config& cfg);
	static void attack_anim_ranged_iceball(const std::string& tag, const std::string& aid, const std::string& race, const std::string& id, bool terrain, config& cfg);
	static void attack_anim_ranged_lightning(const std::string& tag, const std::string& aid, const std::string& race, const std::string& id, bool terrain, config& cfg);

	/**
	 * Creates a unit type for the given config, but delays its build
	 * till later.
	 * @note @a cfg is not copied, so it has to point to some permanent
	 *       storage, that is, a child of unit_type_data::unit_cfg.
	 */
	unit_type(const config &cfg);
	unit_type(const unit_type& o);

	~unit_type();

	/** Load data into an empty unit_type */
	void build_full(const config& cfg, const movement_type_map &movement_types,
		const race_map &races);
	/** Partially load data into an empty unit_type */
	void build_help_index(const config& cfg, const movement_type_map &movement_types,
		const race_map &races);
	/** Load the most needed data into an empty unit_type */
	void build_created(const config& cfg, const movement_type_map &movement_types,
		const race_map &races);

	/** Get the advancement tree
	 *  Build a set of unit type's id of this unit type's advancement tree */
	std::set<std::string> advancement_tree() const;

	std::vector<std::string> advances_to(int especial) const;
	const std::vector<std::string>& advances_to() const { return advances_to_; }
	const std::vector<std::string> advances_from() const;
	const std::string& advancement() const { return advancement_; }

	const unit_type& get_gender_unit_type(unit_race::GENDER gender) const;

	const unit_type& get_variation(const std::string& name) const;
	/** Info on the type of unit that the unit reanimates as. */
	const std::string& undead_variation() const { return undead_variation_; }

	unsigned int num_traits() const { return num_traits_; }

	/** The name of the unit in the current language setting. */
	const t_string& type_name() const { return type_name_; }

	const std::string& id() const { return id_; }
	// NOTE: this used to be a const object reference, but it messed up with the
	// translation engine upon changing the language in the same session.
	const t_string unit_description() const;
	int hitpoints() const { return hitpoints_; }
	int level() const { return level_; }
	int movement() const { return movement_; }
	int max_movement() const { return max_movement_; }
	int max_attacks() const { return max_attacks_; }
	int cost() const { return cost_; }
	int gold_income() const { return gold_income_; }
	int technology_income() const { return technology_income_; }
	int food_income() const { return food_income_; }
	int miss_income() const { return miss_income_; }
	int heal() const { return heal_; }
	int turn_experience() const { return turn_experience_; }
	const std::string& image() const { return image_; }
	const std::string& weak_image() const { return weak_image_; }
	const std::string& halo() const { return halo_; }

#if defined(_KINGDOM_EXE) || !defined(_WIN32)
	const std::vector<unit_animation>& animations() const;
#endif

	const std::string& flag_rgb() const { return flag_rgb_; }
	const std::string& die_sound() const { return die_sound_; }

	std::vector<attack_type> attacks() const;
	const unit_movement_type& movement_type() const { return movementType_; }
	const std::string& movementType_id() const { return movementType_id_; }

	int experience_needed(bool with_acceleration=true) const;

	struct experience_accelerator {
		experience_accelerator(int modifier);
		~experience_accelerator();
		static int get_acceleration();
	private:
		int old_value_;
	};

	enum ALIGNMENT { LAWFUL, NEUTRAL, CHAOTIC, LIMINAL };

	ALIGNMENT alignment() const { return alignment_; }
	static const char* alignment_description(ALIGNMENT align, unit_race::GENDER gender = unit_race::MALE);
	static const char* alignment_id(ALIGNMENT align);

	fixed_t alpha() const { return alpha_; }

	const std::multimap<const std::string, const config*>& abilities_cfg() const { return abilities_cfg_; }
	const std::vector<t_string>& abilities() const { return abilities_; }
	const std::vector<std::string>& ability_tooltips() const { return ability_tooltips_; }

	bool can_advance() const { return !advances_to_.empty(); }

	bool can_recruit() const { return can_recruit_; }
	bool can_reside() const { return can_reside_; }
	bool base() const { return base_; }
	bool wall() const { return wall_; }
	bool fort() const { return fort_; }
	bool transport() const { return transport_; }
	bool land_wall() const { return land_wall_; }
	bool walk_wall() const { return walk_wall_; }
	const std::string& match() const { return match_; }
	bool terrain_matches(t_translation::t_terrain tcode) const;
	const std::string& raw_icon() const { return raw_icon_; }
	std::string icon() const;
	const std::string& raw_movement_sound() const { return raw_movement_sound_; }
	std::string movement_sound() const;
	const std::string& raw_idle_sound() const { return raw_idle_sound_; }
	std::string idle_sound() const;
	int arms() const { return arms_; }
	int especial() const { return especial_; }
	int master() const { return master_; }
	int guard() const { return guard_; }
	const std::set<map_location::DIRECTION>& touch_dirs() const { return touch_dirs_; }
	bool packer() const { return packer_; }

	bool has_zoc() const { return zoc_; }
	bool cancel_zoc() const { return cancel_zoc_; }

	enum {REQUIRE_NONE = 0, REQUIRE_LEADER, REQUIRE_FEMALE};
	int require() const { return require_; }
	bool can_range(int range) const;

	// bool has_ability_by_id(const std::string& ability) const;

	const std::vector<const config*>& possible_traits() const
	{ return possibleTraits_; }

	const std::vector<unit_race::GENDER>& genders() const { return genders_; }

	const std::string& race() const { return race_->id(); }
	bool hide_help() const;

    enum BUILD_STATUS {NOT_BUILT, CREATED, BS_HELP_INDEX, WITHOUT_ANIMATIONS, FULL};

    BUILD_STATUS build_status() const { return build_status_; }

	const config& get_cfg() const { return cfg_; }

	bool use_terrain_image() const;

private:
	void operator=(const unit_type& o);
	void fill_abilities_cfg(const std::string& abilities);

	config cfg_;

	std::string id_;
    t_string type_name_;
    t_string description_;
    int hitpoints_;
    int level_;
    int movement_;
	int max_movement_;
    int max_attacks_;
    int cost_;
	int gold_income_;
	int technology_income_;
	int food_income_;
	int miss_income_;
	int heal_;
	int turn_experience_;
    std::string halo_;
    std::string undead_variation_;

    std::string image_;
	std::string weak_image_;
	std::string flag_rgb_;
	std::string die_sound_;

    unsigned int num_traits_;

	typedef std::map<std::string, unit_type*> variations_map;
	variations_map gender_types_;
	variations_map variations_;

	const unit_race* race_;

	fixed_t alpha_;

	std::multimap<const std::string, const config*> abilities_cfg_;
	std::vector<t_string> abilities_;
	std::vector<std::string> ability_tooltips_;

	bool zoc_, hide_help_;
	bool cancel_zoc_;
	int require_;
	int can_ranges_;

	std::vector<std::string> advances_to_;
	std::string advancement_;
	int experience_needed_;
	bool in_advancefrom_;


	ALIGNMENT alignment_;

	unit_movement_type movementType_;
	std::string movementType_id_;

	std::vector<const config*> possibleTraits_;

	std::vector<unit_race::GENDER> genders_;
	std::vector<attack_type> attacks_;

	// animations are loaded only after the first animations() call
#if defined(_KINGDOM_EXE) || !defined(_WIN32)
	mutable std::vector<unit_animation> animations_;
#endif

	BUILD_STATUS build_status_;

	std::string match_;
	std::string raw_icon_;
	std::string raw_movement_sound_;
	std::string raw_idle_sound_;
	t_translation::t_terrain terrain_;
	bool can_recruit_;
	bool can_reside_;
	bool base_;
	bool wall_;
	bool fort_;
	bool transport_;
	bool land_wall_;
	bool walk_wall_;
	int arms_;
	int especial_;
	int master_;
	int guard_;
	std::set<map_location::DIRECTION> touch_dirs_;
	bool packer_;
};

class sort_recruit
{
public:
	sort_recruit(const unit_type* ut) 
		: ut_(ut)
	{}

	bool operator()(const hero* lhs, const hero* rhs) const;

private:
	const unit_type* ut_;
};

class unit_type_data
{
public:
	unit_type_data();
	~unit_type_data();

	typedef std::map<std::string,unit_type> unit_type_map;

	const unit_type_map &types() const { return types_; }
	const std::map<std::string, const unit_type*> &artifical_types() const { return artifical_types_; }
	const std::map<int, const unit_type*>& keytypes() const { return keytypes_; }
	const unit_type* keytype(int index) const { return keytypes_.find(index)->second; }
	const unit_type* keytype2(int index) const;
	const unit_type* master_type(int number) const;
	const unit_type* id_type(const std::string& id) const;
	const movement_type_map& movement_types() const { return movement_types_; }
	const race_map &races() const { return races_; }
	const traits_map& traits() const { return traits_; }
	const modifications_map& modifications() const { return modifications_; }
	const complex_feature_map& complex_feature() const { return complex_feature_; }
	const std::vector<int>& features_level() const { return features_level_; }
	int feature_level(int f) const { return features_level_[f]; }
	const std::vector<ttreasure>& treasures() const { return treasures_; }
	const ttreasure& treasure(int index) const { return treasures_[index]; }
	const abilities_map& abilities() const { return abilities_; }
	const specials_map& specials() const { return specials_; }

	const std::map<int, ttactic>& tactics() const { return tactics_; }
	const ttactic& tactic(int index) const { return tactics_.find(index)->second; }
	const std::map<std::string, int>& tactics_id() const { return tactics_id_; }
	const ttactic& tactic(const std::string& id) const { return tactics_.find(tactics_id_.find(id)->second)->second; }

	const std::map<int, tcharacter>& characters() const { return characters_; }
	const tcharacter& character(int index) const { return characters_.find(index)->second; }
	const std::map<std::string, int>& characters_id() const { return characters_id_; }
	const tcharacter& character(const std::string& id) const { return characters_.find(characters_id_.find(id)->second)->second; }

	const std::map<int, tdecree>& decrees() const { return decrees_; }
	const tdecree& decree(int index) const { return decrees_.find(index)->second; }
	const std::map<std::string, int>& decrees_id() const { return decrees_id_; }
	const tdecree& decree(const std::string& id) const { return decrees_.find(decrees_id_.find(id)->second)->second; }

	const std::map<tgenus::genus_t, tgenus>& genera() const { return genera_; }
	const tgenus& genus(tgenus::genus_t g) const { return genera_.find(g)->second; }

	const std::map<std::string, ttechnology>& technologies() const { return technologies_; }
	const ttechnology& technology(const std::string& id) const;

	const std::map<int, std::set<tnoble> >& nobles() const { return nobles_; }
	const std::set<tnoble>& level_nobles(int level) const { return nobles_.find(level)->second; }
	int noble_count() const { return sort_nobles_.size(); }
	int level_noble_count(int level) const;
	const tnoble& noble(int index) const;
	const std::vector<const tnoble*>& leader_nobles() const { return leader_nobles_; }
	const tnoble& leader_noble(int level) const { return *leader_nobles_[level]; }
	int max_noble_level() const { return leader_nobles_.back()->level(); }

	const std::map<int, tformation_profile>& formations() const { return formations_; }
	const tformation_profile& formation(int index) const { return formations_.find(index)->second; }
	const std::map<std::string, int>& formations_id() const { return formations_id_; }
	const tformation_profile& formation(const std::string& id) const { return formations_.find(formations_id_.find(id)->second)->second; }

	const std::vector<std::string>& arms_ids() const { return arms_ids_; }
	int arms_from_id(const std::string& id) const;
	const std::vector<std::string>& range_ids() const { return range_ids_; }
	int range_from_id(const std::string& id) const;
	const std::vector<std::string>& atype_ids() const { return atype_ids_; }
	const std::vector<tespecial>& especials() const { return especials_; }
	const tespecial& especial(int index) const { return especials_[index]; }
	const std::string& especial_id(int character) const;
	int especial_from_id(const std::string& id) const;
	const std::multimap<std::string, const config>& utype_anims() const { return utype_anims_; }
#if defined(_KINGDOM_EXE) || !defined(_WIN32)
	const std::map<int, unit_animation>& global_anims() const { return global_anims_; }
	const unit_animation* global_anim(int type) const;
#endif
	const std::vector<const unit_type*>& can_recruit() const { return can_recruit_; }
	const navigation_types& navigation_threshold() const { return navigation_types_; }
	const std::string& id_from_navigation(int navigation) const;
	bool navigation_can_advance(int prev, int next) const;

	void generate_utype_tree();
	const std::vector<advance_tree::node*>& utype_tree() const { return utype_tree_; }
	void generate_technology_tree();
	const std::vector<advance_tree::node*>& technology_tree() const { return technology_tree_; }
	const std::set<const ttechnology*>& selectable_technologies() const { return selectable_technologies_; }

	void set_config(const config &cfg);

	const unit_type* find(const std::string &key, unit_type::BUILD_STATUS status = unit_type::FULL) const;
	const unit_race* find_race(const std::string &) const;
	const unit_type* find_wall() const { return wall_type_; }
	const unit_type* find_keep() const { return keep_type_; }
	const unit_type* find_market() const { return market_type_; }
	const unit_type* find_technology() const { return technology_type_; }
	const unit_type* find_tactic() const { return tactic_type_; }
	const unit_type* find_tower() const { return tower_type_; }
	const unit_type* find_fort() const { return fort_type_; }
	const unit_type* find_school() const { return school_type_; }
	const unit_type* find_city(int level) const { return city_type_.find(level)->second; }

	const unit_type* find_scout() const { return scout_type_; }

	/** Checks if the [hide_help] tag contains these IDs. */
	bool hide_help(const std::string &type_id, const std::string &race_id) const;

	void clear();
private:
	// unit_type_data(const unit_type_data &);

	/** Parses the [hide_help] tag. */
	void read_hide_help(const config &cfg);

	std::pair<unit_type_map::iterator, bool> insert(const std::pair<std::string, unit_type> &utype) { return types_.insert(utype); }
	
	unit_type& build_unit_type(unit_type &ut, const config& cfg, unit_type::BUILD_STATUS status = unit_type::FULL) const;
	void add_advancefrom(const config& unit_cfg) const;

	mutable unit_type_map types_;
	std::map<int, const unit_type*> keytypes_;
	std::map<std::string, const unit_type*> artifical_types_;
	std::map<int, const unit_type*> master_types_;
	std::map<std::string, int> master_ids_;
	movement_type_map movement_types_;
	race_map races_;
	modifications_map modifications_;
	traits_map traits_;
	complex_feature_map complex_feature_;
	std::vector<int> features_level_;
	std::vector<ttreasure> treasures_;
	abilities_map abilities_;
	specials_map specials_;
	std::map<int, ttactic> tactics_;
	std::map<std::string, int> tactics_id_;
	std::map<int, tcharacter> characters_;
	std::map<std::string, int> characters_id_;
	std::map<int, tdecree> decrees_;
	std::map<std::string, int> decrees_id_;
	std::map<tgenus::genus_t, tgenus> genera_;
	std::map<std::string, ttechnology> technologies_;

	std::map<int, std::set<tnoble> > nobles_;
	std::vector<const tnoble*> sort_nobles_;
	std::vector<const tnoble*> leader_nobles_;

	std::map<int, tformation_profile> formations_;
	std::map<std::string, int> formations_id_;

	std::multimap<std::string, const config> utype_anims_;
#if defined(_KINGDOM_EXE) || !defined(_WIN32)
	std::map<int, unit_animation> global_anims_;
#endif
	std::vector<std::string> arms_ids_;
	std::vector<std::string> atype_ids_;
	std::vector<std::string> range_ids_;
	std::vector<tespecial> especials_;
	std::vector<const unit_type*> can_recruit_;
	navigation_types navigation_types_;

	std::vector<advance_tree::node*> utype_tree_;
	std::vector<advance_tree::node*> technology_tree_;
	std::set<const ttechnology*> selectable_technologies_;

	unit_type* wall_type_;
	unit_type* keep_type_;
	unit_type* market_type_;
	unit_type* technology_type_;
	unit_type* tactic_type_;
	unit_type* tower_type_;
	unit_type* fort_type_;
	unit_type* school_type_;
	std::map<int, const unit_type*> city_type_;

	unit_type* businessman_type_;
	unit_type* scholar_type_;
	unit_type* transport_type_;

	unit_type* scout_type_;

	/** True if [hide_help] contains a 'all=yes' at its root. */
	bool hide_help_all_;
	// vectors containing the [hide_help] and its sub-tags [not]
	std::vector< std::set<std::string> > hide_help_type_;
	std::vector< std::set<std::string> > hide_help_race_;

	unit_type::BUILD_STATUS build_status_;
};

namespace stratagem_tag {
	enum {none, xue_zhong_song_tan, min = xue_zhong_song_tan, miao_shou_hui_chun, 
		xiao_li_cang_dao, sheng_dong_ji_xi, jiang_lang_cai_jin, max = jiang_lang_cai_jin,
	};

	extern std::map<const std::string, int> tags;
	extern std::map<int, const ttechnology*> technologies;
	int find(const std::string& tag);
	const ttechnology& technology(int tag);
}

extern unit_type_data unit_types;

#endif
