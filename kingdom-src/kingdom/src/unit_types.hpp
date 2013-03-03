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

class tespecial 
{
public:
	tespecial(int index, const std::string& id);

	int index_;
	std::string id_;
	std::string name_;
	std::string image_;
};

namespace apply_to_tag {
	enum {NONE, ATTACK, HITPOINTS, MOVEMENT, MUNITION, MAX_EXPERIENCE,
		LOYAL, STATUS, MOVEMENT_COSTS, DEFENSE, RESISTANCE, ENCOURAGE,
		DEMOLISH, ZOC, IMAGE_MOD, ADVANCE, TRAIN, 
		DAMAGE, ONEOFF_MIN = DAMAGE, HIDE, ALERT, PROVOKE, CLEAR, ONEOFF_MAX = CLEAR,
		DECREE, UNIT_END = DECREE,
		// side <=
		CIVILIZATION, POLITICS, HEAL, STRATEGIC,
		// character <=
		AGGRESSIVE, CHARACTER_MIN = AGGRESSIVE, UNITED, CHARISMATIC, CREATIVE, 
		EXPANSIVE, FINANCIAL, INDUSTRIOUS, ORGANIZED, PROTECTIVE,
		PHILOSOPHICAL, SPIRITUAL, CHARACTER_MAX = SPIRITUAL
	};

	extern std::map<const std::string, int> tags;
	int find(const std::string& tag);
}

namespace sound_filter_tag {
	enum {NONE, MALE, FEMALE};

	extern std::map<const std::string, int> tags;
	int find(const std::string& tag);
	const std::string& rfind(int tag);
	std::string filter(const std::string& src, const std::string& f);
}

namespace filter {
enum {TROOP = 1, ARTIFICAL = 2, CITY = 4};
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

	ttactic()
		: index_(-1)
		, id_()
		, range_(0)
		, point_(0)
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
		, friend_profit_(0)
		, friend_hide_profit_(0)
		, friend_clear_profit_(0)
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

	int self_profit() const { return self_profit_; }
	int self_hide_profit() const { return self_hide_profit_; }
	int self_alert_profit() const { return self_alert_profit_; }
	int self_clear_profit() const { return self_clear_profit_; }
	int friend_profit() const { return friend_profit_; }
	int friend_hide_profit() const { return friend_hide_profit_; }
	int friend_clear_profit() const { return friend_clear_profit_; }
	int enemy_profit() const { return enemy_profit_; }
	int enemy_provoke_profit() const { return enemy_provoke_profit_; }
	bool complex() const { return complex_; }
	bool oneoff() const;
	bool select_one() const;

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
	std::vector<const ttactic*> parts_;
	int range_;
	config effect_cfg_;
	int apply_to_;
	int type_filter_;

	int self_profit_;
	int self_hide_profit_;
	int self_alert_profit_;
	int self_clear_profit_;
	int friend_profit_;
	int friend_hide_profit_;
	int friend_clear_profit_;
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
		, politics_(0)
		, charm_(0)
	{}

public:
	int leadership_;
	int force_;
	int intellect_;
	int politics_;
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

class technology : public advance_tree::base
{
public:
	enum {NONE = 0, MODIFY, FINISH};
	
	technology()
		: id_()
		, advances_to_()
		, name_()
		, description_()
		, occasion_(NONE)
		, max_experience_(0)
		, apply_to_(apply_to_tag::NONE)
		, relative_(HEROS_NO_CHARACTER)
		, parts_()
		, complex_(false)
		, type_filter_(0)
		, arms_filter_(0)
	{}

	technology(const config& cfg);

	const std::string& id() const { return id_; }
	const std::vector<std::string>& advances_to() const { return advances_to_; }
	const std::string& name() const { return name_; }
	const std::string& description() const { return description_; }
	int occasion() const { return occasion_; }
	const std::vector<const technology*>& parts() const { return parts_; }

	int max_experience() const { return max_experience_; }

	int relative() const { return relative_; }
	const config& effect_cfg() const { return effect_cfg_; }
	int apply_to() const { return apply_to_; }

	bool complex() const { return complex_; }

	void set_atom_part() { parts_.push_back(this); }

	int type_filter() const { return type_filter_; }
	int arms_filter() const { return arms_filter_; }
	bool filter(int type, int arms) const;
private:
	std::string id_;
	std::vector<std::string> advances_to_;
	std::string name_;
	std::string description_;
	int occasion_;
	int max_experience_;
	std::vector<const technology*> parts_;
	int relative_;
	config effect_cfg_;
	int apply_to_;
	int type_filter_;
	int arms_filter_;
		
	bool complex_;
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
typedef std::map<int, std::vector<int> > treasure_map;
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
	static std::string attack_image(const std::string& race, const std::string& id, bool terrain, int range, int number);

	static bool has_resistance_anim(const std::set<std::string>& abilities);
	static bool has_leading_anim(const std::set<std::string>& abilities);
	static bool has_healing_anim(const std::set<std::string>& abilities);

	static void defend_anim(const std::string& race, const std::string& id, bool terrain, config& cfg);
	static void resistance_anim(const std::string& race, const std::string& id, bool terrain, config& cfg);
	static void leading_anim(const std::string& race, const std::string& id, bool terrain, config& cfg);
	static void healing_anim(const std::string& race, const std::string& id, bool terrain, config& cfg);
	static void idle_anim(const std::string& race, const std::string& id, bool terrain, config& cfg);
	static void attack_anim_melee(const std::string& aid, const std::string& aicon, bool troop, const std::string& race, const std::string& id, bool terrain, config& cfg);
	static void attack_anim_ranged(const std::string& aid, const std::string& aicon, const std::string& race, const std::string& id, bool terrain, config& cfg);
	static void attack_anim_ranged_magic_missile(const std::string& aid, const std::string& race, const std::string& id, bool terrain, config& cfg);
	static void attack_anim_ranged_lightbeam(const std::string& aid, const std::string& race, const std::string& id, bool terrain, config& cfg);
	static void attack_anim_ranged_fireball(const std::string& aid, const std::string& race, const std::string& id, bool terrain, config& cfg);
	static void attack_anim_ranged_iceball(const std::string& aid, const std::string& race, const std::string& id, bool terrain, config& cfg);
	static void attack_anim_ranged_lightning(const std::string& aid, const std::string& race, const std::string& id, bool terrain, config& cfg);

	/**
	 * Creates a unit type for the given config, but delays its build
	 * till later.
	 * @note @a cfg is not copied, so it has to point to some permanent
	 *       storage, that is, a child of unit_type_data::unit_cfg.
	 */
	unit_type(config &cfg);
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
	int heal() const { return heal_; }
	int turn_experience() const { return turn_experience_; }
	const std::string& image() const { return image_; }
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
	bool land_wall() const { return land_wall_; }
	bool walk_wall() const { return walk_wall_; }
	const std::string& match() const { return match_; }
	int arms() const { return arms_; }
	int especial() const { return special_; }
	int master() const { return master_; }
	int guard() const { return guard_; }
	bool packer() const { return packer_; }

	bool has_zoc() const { return zoc_; }
	bool cancel_zoc() const { return cancel_zoc_; }
	bool leader() const { return leader_; }

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
	int heal_;
	int turn_experience_;
    std::string halo_;
    std::string undead_variation_;

    std::string image_;
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
	bool leader_;

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
	t_translation::t_terrain terrain_;
	bool can_recruit_;
	bool can_reside_;
	bool base_;
	bool wall_;
	bool land_wall_;
	bool walk_wall_;
	int arms_;
	int special_;
	int master_;
	int guard_;
	bool packer_;
};

class unit_type_data
{
public:
	unit_type_data();
	~unit_type_data();

	typedef std::map<std::string,unit_type> unit_type_map;

	const unit_type_map &types() const { return types_; }
	const std::map<std::string, const unit_type*> &artifical_types() const { return artifical_types_; }
	const unit_type* master_type(int number) const { return master_types_.find(number)->second; }
	const movement_type_map& movement_types() const { return movement_types_; }
	const race_map &races() const { return races_; }
	const traits_map& traits() const { return traits_; }
	const modifications_map& modifications() const { return modifications_; }
	const complex_feature_map& complex_feature() const { return complex_feature_; }
	const treasure_map& treasures() const { return treasures_; }
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

	const std::map<std::string, technology>& technologies() const { return technologies_; }
	const std::vector<std::string>& arms_ids() const { return arms_ids_; }
	int arms_from_id(const std::string& id) const;
	const std::vector<std::string>& range_ids() const { return range_ids_; }
	const std::vector<tespecial>& especials() const { return especials_; }
	const tespecial& especial(int index) const { return especials_[index]; }
	const std::string& especial_id(int character) const;
	int especial_from_id(const std::string& id) const;
	const std::map<std::string, config>& utype_anims() const { return utype_anims_; }
	const std::vector<const unit_type*>& can_recruit() const { return can_recruit_; }
	const navigation_types& navigation_threshold() const { return navigation_types_; }
	const std::string& id_from_navigation(int navigation) const;
	bool navigation_can_advance(int prev, int next) const;

	void generate_utype_tree();
	const std::vector<advance_tree::node*>& utype_tree() const { return utype_tree_; }
	void generate_technology_tree();
	const std::vector<advance_tree::node*>& technology_tree() const { return technology_tree_; }

	void set_config(config &cfg);

	const unit_type *find(const std::string &key, unit_type::BUILD_STATUS status = unit_type::FULL) const;
	const unit_race *find_race(const std::string &) const;
	const unit_type *find_wall() const { return wall_type_; }
	const unit_type *find_keep() const { return keep_type_; }
	const unit_type *find_market() const { return market_type_; }
	const unit_type *find_technology() const { return technology_type_; }
	const unit_type *find_tower() const { return tower_type_; }

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
	std::map<std::string, const unit_type*> artifical_types_;
	std::map<int, const unit_type*> master_types_;
	movement_type_map movement_types_;
	race_map races_;
	modifications_map modifications_;
	traits_map traits_;
	complex_feature_map complex_feature_;
	treasure_map treasures_;
	abilities_map abilities_;
	specials_map specials_;
	std::map<int, ttactic> tactics_;
	std::map<std::string, int> tactics_id_;
	std::map<int, tcharacter> characters_;
	std::map<std::string, int> characters_id_;
	std::map<std::string, technology> technologies_;
	std::map<std::string, config> utype_anims_;
	std::vector<std::string> arms_ids_;
	std::vector<std::string> range_ids_;
	std::vector<tespecial> especials_;
	std::vector<const unit_type*> can_recruit_;
	navigation_types navigation_types_;

	std::vector<advance_tree::node*> utype_tree_;
	std::vector<advance_tree::node*> technology_tree_;

	unit_type* wall_type_;
	unit_type* keep_type_;
	unit_type* market_type_;
	unit_type* technology_type_;
	unit_type* tower_type_;

	unit_type* businessman_type_;
	unit_type* scholar_type_;

	/** True if [hide_help] contains a 'all=yes' at its root. */
	bool hide_help_all_;
	// vectors containing the [hide_help] and its sub-tags [not]
	std::vector< std::set<std::string> > hide_help_type_;
	std::vector< std::set<std::string> > hide_help_race_;

	unit_type::BUILD_STATUS build_status_;
};

extern unit_type_data unit_types;

#endif
