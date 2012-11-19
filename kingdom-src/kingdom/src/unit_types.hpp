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

class gamemap;
class unit;
class artifical;
class unit_ability_list;
class unit_map;
class unit_type_data;


class department
{
public:
	enum { commercial };
	department(int type, const std::string& name, const std::string& image, const std::string& portrait);
	
	int type_;
	std::string name_;
	std::string image_;
	std::string portrait_;
	int exploiture_;
};

class tcharacter 
{
public:
	tcharacter(int index, const std::string& id);

	int index_;
	std::string id_;
	std::string name_;
	std::string image_;
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

class unit_type
{
public:
	friend class unit;
	friend class artifical;
	friend class unit_type_data;

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
	void build_full(const movement_type_map &movement_types,
		const race_map &races, const config::const_child_itors &traits);
	/** Partially load data into an empty unit_type */
	void build_help_index(const movement_type_map &movement_types,
		const race_map &races, const config::const_child_itors &traits);
	/** Load the most needed data into an empty unit_type */
	void build_created(const movement_type_map &movement_types,
		const race_map &races, const config::const_child_itors &traits);

	/** Get the advancement tree
	 *  Build a set of unit type's id of this unit type's advancement tree */
	std::set<std::string> advancement_tree() const;

	std::vector<std::string> advances_to(int character) const;
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
	int max_attacks() const { return max_attacks_; }
	int cost() const { return cost_; }
	int gold_income() const { return gold_income_; }
	int heal() const { return heal_; }
	int turn_experience() const { return turn_experience_; }
	const std::string& image() const { return image_; }
	const std::string& halo() const { return halo_; }

#if defined(_KINGDOM_EXE) || !defined(_WIN32)
	const std::vector<unit_animation>& animations() const;
#endif

	const std::string& flag_rgb() const { return flag_rgb_; }

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
	int character() const { return character_; }
	int master() const { return master_; }
	int guard() const { return guard_; }
	bool packer() const { return packer_; }

	bool has_zoc() const { return zoc_; }
	bool cancel_zoc() const { return cancel_zoc_; }
	bool leader() const { return leader_; }

	bool has_ability_by_id(const std::string& ability) const;

	const std::vector<const config*>& possible_traits() const
	{ return possibleTraits_; }

	const std::vector<unit_race::GENDER>& genders() const { return genders_; }

	const std::string& race() const { return race_->id(); }
	bool hide_help() const;

    enum BUILD_STATUS {NOT_BUILT, CREATED, BS_HELP_INDEX, WITHOUT_ANIMATIONS, FULL};

    BUILD_STATUS build_status() const { return build_status_; }

	const config &get_cfg() const { return cfg_; }

private:
	void operator=(const unit_type& o);
	void fill_abilities_cfg(const std::string& abilities);

	config &cfg_;

	std::string id_;
    t_string type_name_;
    t_string description_;
    int hitpoints_;
    int level_;
    int movement_;
    int max_attacks_;
    int cost_;
	int gold_income_;
	int heal_;
	int turn_experience_;
    std::string halo_;
    std::string undead_variation_;

    std::string image_;
	std::string flag_rgb_;

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
	int character_;
	int master_;
	int guard_;
	bool packer_;
};

class unit_type_data
{
public:
	unit_type_data();

	typedef std::map<std::string,unit_type> unit_type_map;

	const unit_type_map &types() const { return types_; }
	const std::map<std::string, const unit_type*> &artifical_types() const { return artifical_types_; }
	const movement_type_map& movement_types() const { return movement_types_; }
	const race_map &races() const { return races_; }
	const traits_map& traits() const { return traits_; }
	const modifications_map& modifications() const { return modifications_; }
	const complex_feature_map& complex_feature() const { return complex_feature_; }
	const treasure_map& treasures() const { return treasures_; }
	const abilities_map& abilities() const { return abilities_; }
	const specials_map& specials() const { return specials_; }
	const std::vector<std::string>& arms_ids() const { return arms_ids_; }
	int arms_from_id(const std::string& id) const;
	const std::vector<tcharacter>& characters() const { return characters_; }
	const tcharacter& character(int index) const { return characters_[index]; }
	const std::string& character_id(int character) const;
	int character_from_id(const std::string& id) const;
	const std::vector<const unit_type*>& can_recruit() const { return can_recruit_; }
	const navigation_types& navigation_threshold() const { return navigation_types_; }
	const std::string& id_from_navigation(int navigation) const;
	bool navigation_can_advance(int prev, int next) const;

	void set_config(config &cfg);

	const unit_type *find(const std::string &key, unit_type::BUILD_STATUS status = unit_type::FULL) const;
	const unit_race *find_race(const std::string &) const;
	const unit_type *find_wall() const { return wall_type_; }
	const unit_type *find_keep() const { return keep_type_; }
	const unit_type *find_market() const { return market_type_; }
	const unit_type *find_tower() const { return tower_type_; }

	void build_all(unit_type::BUILD_STATUS status);

	/** Checks if the [hide_help] tag contains these IDs. */
	bool hide_help(const std::string &type_id, const std::string &race_id) const;

	void clear();
private:
	// unit_type_data(const unit_type_data &);

	/** Parses the [hide_help] tag. */
	void read_hide_help(const config &cfg);

	void set_unit_config(const config& unit_cfg) { unit_cfg_ = &unit_cfg; }

	const config &find_config(const std::string &key) const;
	std::pair<unit_type_map::iterator, bool> insert(const std::pair<std::string, unit_type> &utype) { return types_.insert(utype); }
	
	unit_type& build_unit_type(const unit_type_map::iterator &ut, unit_type::BUILD_STATUS status) const;
	void add_advancefrom(const config& unit_cfg) const;

	mutable unit_type_map types_;
	std::map<std::string, const unit_type*> artifical_types_;
	movement_type_map movement_types_;
	race_map races_;
	modifications_map modifications_;
	traits_map traits_;
	complex_feature_map complex_feature_;
	treasure_map treasures_;
	abilities_map abilities_;
	specials_map specials_;
	std::vector<std::string> arms_ids_;
	std::vector<tcharacter> characters_;
	std::vector<const unit_type*> can_recruit_;
	navigation_types navigation_types_;

	unit_type* wall_type_;
	unit_type* keep_type_;
	unit_type* market_type_;
	unit_type* tower_type_;

	/** True if [hide_help] contains a 'all=yes' at its root. */
	bool hide_help_all_;
	// vectors containing the [hide_help] and its sub-tags [not]
	std::vector< std::set<std::string> > hide_help_type_;
	std::vector< std::set<std::string> > hide_help_race_;

	const config *unit_cfg_;
	unit_type::BUILD_STATUS build_status_;
};

extern unit_type_data unit_types;

#endif
