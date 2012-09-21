/* $Id: unit.hpp 47608 2010-11-21 01:56:29Z shadowmaster $ */
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

/** @file */

#ifndef UNIT_H_INCLUDED
#define UNIT_H_INCLUDED

#include "formula_callable.hpp"
#include "unit_types.hpp"
#include "unit_map.hpp"
#include "hero.hpp"

class game_display;
class game_state;
class vconfig;
class team;

class move_unit_spectator {
public:
	/** add a location of a seen friend */
	void add_seen_friend(const unit_map::const_iterator &u);


	/** add the location of new seen enemy */
	void add_seen_enemy(const unit_map::const_iterator &u);


	/** get the location of an ambusher */
	const unit_map::const_iterator& get_ambusher() const;


	/** get the location of a failed teleport */
	const unit_map::const_iterator& get_failed_teleport() const;


	/** get the locations of seen enemies */
	const std::vector<unit_map::const_iterator>& get_seen_enemies() const;


	/** get the locations of seen friends */
	const std::vector<unit_map::const_iterator>& get_seen_friends() const;


	/** get new location of moved unit */
	const unit_map::const_iterator& get_unit() const;

	/** get the locations be interrupted */
	const std::vector<map_location>& get_interrupted_waypoints() const;

	/** constructor */
	move_unit_spectator(const unit_map &units);


	/** destructor */
	virtual ~move_unit_spectator();

	/** reset all locations to empty values*/
	void reset(const unit_map &units);


	/** set the location of an ambusher */
	void set_ambusher(const unit_map::const_iterator &u);


	/** set the location of a failed teleport */
	void set_failed_teleport(const unit_map::const_iterator &u);


	/** set the iterator to moved unit*/
	void set_unit(const unit_map::const_iterator &u);

	/** set the waypoints be interrupted*/
	void set_interrupted_waypoints(const std::vector<map_location>& waypoints);
private:
	unit_map::const_iterator ambusher_;
	unit_map::const_iterator failed_teleport_;
	std::vector<unit_map::const_iterator> seen_enemies_;
	std::vector<unit_map::const_iterator> seen_friends_;
	unit_map::const_iterator unit_;
	std::vector<map_location> interrupted_waypoints_;
};

class unit_ability_list
{
public:
	unit_ability_list() :
		cfgs()
	{
	}

	bool empty() const;

	std::pair<int,map_location> highest(const std::string& key, int def=0) const;
	std::pair<int,map_location> lowest(const std::string& key, int def=0) const;

	void merge(const unit_ability_list& other);

	std::vector<std::pair<const config *, unit *> > cfgs;
};

class unit_merit
{
public:
	unit_merit();
	unit_merit(const unit_merit& o);

public:
	int field_turns_;
	int cause_damage_;
	int been_damage_;
	int defeat_units_;
};

struct unit_segment {
	uint16_t offset_;
	uint16_t size_;
};

struct unit_segment2 {
	uint32_t count_;
	uint32_t size_;
};

struct attack_fields {
	unit_segment id_;
	unit_segment type_;
	unit_segment icon_;
	unit_segment specials_;
	int32_t range_;
	int32_t damage_;
	int32_t number_;
};

#define UNIT_FIELDS	\
	int32_t states_;	\
	int32_t size_;	\
	int32_t artifical_;	\
	int32_t been_damage_;	\
	int32_t cause_damage_;	\
	int32_t defeat_units_;	\
	int32_t field_turns_;	\
	int32_t hitpoints_;	\
	int32_t experience_;	\
	int32_t moves_;	\
	int32_t max_hitpoints_;	\
	int32_t max_experience_;	\
	int32_t max_moves_;	\
	int32_t attacks_left_;	\
	int32_t side_;	\
	int32_t cityno_;	\
	int32_t x_;	\
	int32_t y_;	\
	int32_t goto_x_;	\
	int32_t goto_y_;	\
	int32_t facing_;	\
	int32_t upkeep_;	\
	int32_t keep_turns_;	\
	int32_t human_;	\
	int32_t random_traits_;	\
	int32_t resting_;	\
	int32_t modified_;	\
	unit_segment attacks_;	\
	unit_segment heros_army_;	\
	unit_segment type_;	\
	unit_segment traits_;	\
	unit_segment modifications_;	\
	unit_segment cfg_;	

struct unit_fields_t 
{
	UNIT_FIELDS;
};

typedef std::pair<const unit_type*, std::vector<const hero*> > type_heros_pair;

#define unit_feature_val(i)		((feature_[(i) >> 2] >> (((i) % 4) << 1)) & 0x3)
#define unit_feature_val2(u, i)	(((u).feature_[(i) >> 2] >> (((i) % 4) << 1)) & 0x3)

extern uint8_t mask0_2bit[4];
#define unit_feature_set(i, v)	(feature_[(i) >> 2] = (feature_[(i) >> 2] & mask0_2bit[(i) % 4]) | (((v) & 0x3) << (((i) % 4) << 1)))
#define unit_feature_set2(u, i, v)	((u).feature_[(i) >> 2] = ((u).feature_[(i) >> 2] & mask0_2bit[(i) % 4]) | (((v) & 0x3) << (((i) % 4) << 1)))

class unit: public unit_merit
{
public:
	static std::vector<std::pair<int, unit*> > null_int_unitp_pair;
	/**
	 * Clear the unit status cache for all units. Currently only the hidden
	 * status of units is cached this way.
	 */
	static void clear_status_caches();
	static bool draw_desc_;
	static bool ignore_pack_;
	static int commercial_exploiture_;

	static unsigned char savegame_cache_[CONSTANT_1M];

	friend struct unit_movement_resetter;
	// Copy constructor
	unit(const unit& u);
	/** Initilizes a unit from a config */
	unit(unit_map& units, hero_map& heros, const config& cfg, bool use_traits=false, game_state* state = 0, bool artifical = false);
	unit(unit_map& units, hero_map& heros, const uint8_t* mem, bool use_traits=false, game_state* state = 0, bool artifical = false);
	/** Initializes a unit from a unit type */
	unit(unit_map& units, hero_map& heros, type_heros_pair& t, int cityno, bool real_unit, bool artifical = false);
	virtual ~unit();
	unit& operator=(const unit&);

	/** Advances this unit to another type */
	void advance_to(const unit_type *t, bool use_traits = false,
		game_state *state = 0);
	const std::vector<std::string>& advances_to() const;

	void pack_to(const unit_type* t);
	void form_packed_animations(const unit_type* packee);

	/** The type id of the unit */
	const std::string& type_id() const { return type_; }
	const std::string& packee_type_id() const { return packee_type_; }
	const unit_type* type() const;
	const unit_type* packee_type() const;

	/** id assigned by wml */
	const std::string& id() const { return type_; }
	/** The unique internal ID of the unit */
	size_t underlying_id() const { return 0; }

	/** The unit type name */
	const t_string& type_name() const {return type_name_;}
	const std::string& undead_variation() const {return undead_variation_;}

	/** The unit name for display */
	const t_string &name() const;

	/** The unit's profile */
	std::string profile() const;
	/** Information about the unit -- a detailed description of it */
	t_string unit_description() const;

	int hitpoints() const { return hit_points_; }
	int max_hitpoints() const { return max_hit_points_; }
	int experience() const { return experience_; }
	int max_experience() const { return max_experience_; }
	int level() const { return level_; }
	void remove_movement_ai();
	void remove_attacks_ai();
	/**
	 * Adds 'xp' points to the units experience; returns true if advancement
	 * should occur
	 */
	virtual void get_experience(int xp, bool opp_is_artifical = false);
	/** Colors for the unit's hitpoints. */
	SDL_Color hp_color() const;
	/** Colors for the unit's XP. */
	SDL_Color xp_color() const;
	/** Set to true for some scenario-specific units which should not be renamed */
	int side() const { return side_; }
	std::string side_id() const;
	const std::string& team_color() const { return flag_rgb_; }
	unit_race::GENDER gender() const { return gender_; }
	void set_side(unsigned int new_side);
	fixed_t alpha() const { return alpha_; }

	bool is_artifical() const { return artifical_; } 
	bool is_city() const { return artifical_ && can_reside_; } 
	int cityno() const { return cityno_; } 
	void set_cityno(int new_cityno);
	void calculate_5fields();
	void modify_according_to_hero(bool fill_up_hp = true, bool fill_up_movement = false);
	void adjust();
	std::string form_tooltip() const;
	// it's better name is can_recruit(), but back-compitable used
	bool can_recruits() const { return can_recruit_; }
	bool can_reside() const { return can_reside_; }
	bool base() const { return base_; }
	bool wall() const { return wall_; }
	bool land_wall() const { return land_wall_; }
	bool walk_wall() const { return walk_wall_; }
	bool attack_destroy() const { return attack_destroy_; }
	int arms() const { return arms_; }
	const t_translation::t_terrain& terrain() const { return terrain_; }

	hero& master() const { return *master_; }
	hero& second() const { return *second_; }
	hero& third() const { return *third_; }
	void replace_captains(const std::vector<hero*>& captains);
	void replace_captains_internal(hero& selected_hero, std::vector<hero*>& captains) const;

	int guard_attack();

	bool incapacitated() const { return get_state(STATE_PETRIFIED); }
	int total_movement() const { return max_movement_; }
	int movement_left() const { return (movement_ == 0 || incapacitated()) ? 0 : movement_; }
	void set_movement(int moves);
	void set_user_end_turn(bool value=true) { end_turn_ = value; }
	bool user_end_turn() const { return end_turn_; }
	int attacks_left() const { return (attacks_left_ == 0 || incapacitated()) ? 0 : attacks_left_; }
	int attacks_total() const { return max_attacks_; }
	void set_attacks(int left);
	virtual void new_turn();
	virtual void end_turn();
	virtual void new_scenario();
	/** Called on every draw */
	void refresh();

	void set_hitpoints(int hitpoints) { hit_points_ = hitpoints; }
	bool take_hit(int damage) { hit_points_ -= damage; return hit_points_ <= 0; }
	void heal(int amount);
	void heal_all() { hit_points_ = max_hitpoints(); }
	bool resting() const { return resting_; }
	virtual void set_resting(bool rest) { resting_ = rest; }
	void increase_loyalty(int inc);
	void set_loyalty(int level, bool fixed = false);

	void increase_feeling(int inc);

	void increase_activity(int inc, bool adjusting = true);

	const std::map<std::string,std::string> get_states() const;
	bool get_state(const std::string& state) const;
	void set_state(const std::string &state, bool value);
	enum state_t { STATE_MIN = 0, STATE_SLOWED = STATE_MIN, STATE_BROKEN, STATE_POISONED, STATE_PETRIFIED,
		STATE_UNCOVERED, STATE_NOT_MOVED, STATE_REINFORCED, STATE_LEGERITIED, STATE_COUNT, STATE_UNKNOWN = -1 };
	void set_state(state_t state, bool value);
	bool get_state(state_t state) const;
	static state_t get_known_boolean_state_id(const std::string &state);
	static std::map<std::string, state_t> get_known_boolean_state_names();

	bool has_moved() const { return movement_left() != total_movement(); }
	bool has_goto() const { return get_goto().valid(); }
	bool emits_zoc() const { return emit_zoc_ && !incapacitated();}
	bool cancel_zoc() const { return cancel_zoc_; }
	/* cfg: standard unit filter */
	bool matches_filter(const vconfig& cfg,const map_location& loc,bool use_flat_tod=false) const;

	virtual void write(config& cfg) const;

	const std::vector<attack_type>& attacks() const { return attacks_; }
	std::vector<attack_type>& attacks() { return attacks_; }

	int damage_from(const attack_type& attack,bool attacker,const map_location& loc) const { return resistance_against(attack,attacker,loc); }

	/** A SDL surface, ready for display for place where we need a still-image of the unit. */
	const surface still_image(bool scaled = false) const;

	/** draw a unit.  */
	virtual void redraw_unit();
	/** Clear unit_halo_  */
	void clear_haloes();

	void set_standing(bool with_bars = true);

	void set_ghosted(bool with_bars = true);
	void set_disabled_ghosted(bool with_bars = true);

	void set_idling();
	void set_selecting();
	unit_animation* get_animation() {  return anim_;};
	const unit_animation* get_animation() const {  return anim_;};
	void delete_anim();
	void set_facing(map_location::DIRECTION dir);
	map_location::DIRECTION facing() const { return facing_; }

	bool invalidate(const map_location &loc);
	const std::vector<t_string>& trait_names() const { return trait_names_; }
	std::vector<std::string> get_traits_list() const;

	int cost () const { return unit_value_; }
	int gold_income () const;

	int keep_turns() const { return keep_turns_; }
	void set_keep_turns(int turns) { keep_turns_ = turns; }
	bool human() const { return human_; }
	void set_human(bool val);

	bool verifying() const { return verifying_; }
	void set_verifying(bool verifying = true) { verifying_ = verifying; }

	bool packed() const;

	const map_location &get_location() const { return loc_; }
	const std::set<map_location>& get_touch_locations() const { return touch_locs_; }
	const std::set<map_location>& get_overlapped_locations() const { return overlapped_locs_; }
	const std::set<map_location::DIRECTION>& get_touch_dirs() const { return touch_dirs_; }
	/** To be called by unit_map or for temporary units only. */
	virtual void set_location(const map_location &loc);

	const map_location& get_goto() const { return goto_; }
	void set_goto(const map_location& new_goto) { goto_ = new_goto; }

	virtual int upkeep() const;
	bool loyal() const;

	void set_hidden(bool state);
	bool get_hidden() const { return hidden_; }
	bool is_flying() const { return flying_; }
	bool is_fearless() const { return is_fearless_; }
	bool is_healthy() const { return is_healthy_; }
	int movement_cost(const t_translation::t_terrain terrain, const map_location* loc = NULL) const;
	int defense_modifier(t_translation::t_terrain terrain) const;
	int resistance_against(const std::string& damage_name,bool attacker,const map_location& loc) const;
	int resistance_against(const attack_type& damage_type,bool attacker,const map_location& loc) const
		{return resistance_against(damage_type.type(), attacker, loc);};

	//return resistances without any abililities applied
	utils::string_map get_base_resistances() const;
	move_unit_spectator& get_move_spectator();
	const move_unit_spectator& get_move_spectator() const;

	bool can_advance() const;
	bool advances() const { return experience_ >= max_experience() && can_advance(); }

    std::map<std::string,std::string> advancement_icons() const;
    std::vector<std::pair<std::string,std::string> > amla_icons() const;

	std::vector<config> get_modification_advances() const;

	size_t modification_count(const std::string& id) const;

	void add_modification(const std::string& type, const config& modification,
	                  bool no_add = false, bool anim = false);

	/** States for animation. */
	enum STATE {
		STATE_STANDING,   /** anim must fit in a hex */
		STATE_FORGET,     /** animation will be automatically replaced by a standing anim when finished */
		STATE_ANIM};      /** normal anims */
	void start_animation(int start_time, const unit_animation *animation,
		bool with_bars, bool cycles = false, const std::string &text = "",
		Uint32 text_color = 0, STATE state = STATE_ANIM);

	/** The name of the file to game_display (used in menus). */
	std::string absolute_image() const;
	std::string image_halo() const;

	unit_type::ALIGNMENT alignment() const { return alignment_; }
	const unit_race* race() const { return race_; }

	const unit_animation* choose_animation(const game_display& disp,
		       	const map_location& loc, const std::string& event,
		       	const map_location& second_loc = map_location::null_location,
			const int damage=0,
			const unit_animation::hit_type hit_type = unit_animation::INVALID,
			const attack_type* attack=NULL,const attack_type* second_attack = NULL,
			int swing_num =0) const;

	bool get_ability_bool(const std::string& ability, const map_location& loc) const;
	bool get_ability_bool(const std::string &ability) const
	{ return get_ability_bool(ability, loc_); }
	unit_ability_list get_abilities(const std::string &ability, const map_location& loc) const;
	unit_ability_list get_abilities(const std::string &ability) const
	{ return get_abilities(ability, loc_); }
	std::vector<std::string> ability_tooltips(bool force_active = false) const;
	bool has_ability_type(const std::string& ability) const;

	void apply_modifications();
	void generate_traits(bool musthaveonly=false, game_state* state = 0);

	void to_unstage();

	// Only see_all=true use caching
	bool invisible(const map_location& loc, bool see_all=true) const;

	hero* can_encourage();
	void do_encourage(hero& h1, hero& h2);

	/** Mark this unit as clone so it can be insterted to unit_map
	 * @returns                   self (for convenience)
	 **/
	unit& clone(bool is_temporary=true);

	std::string TC_image_mods() const;
	std::string image_mods() const;

	virtual void extract_heros_number();
	virtual void recalculate_heros_pointer();

	virtual void write(uint8_t* mem) const;
	void read(const uint8_t* mem, bool full = true);

	uint16_t leadership_;
	uint16_t force_;
	uint16_t intellect_;
	uint16_t politics_;
	uint16_t charm_;
	uint8_t feature_[HEROS_FEATURE_M2BYTES];
	uint16_t adaptability_[HEROS_MAX_ARMS];
	uint16_t skill_[HEROS_MAX_SKILL];

	map_location last_location_;

	map_location adjacent_[12];	// 和loc_邻近的最多12个格子
	size_t adjacent_size_;

	map_location adjacent_2_[18];	// 和loc_邻近的最多18个格子
	size_t adjacent_size_2_;

	map_location adjacent_3_[24];	// 和loc_邻近的最多24个格子
	size_t adjacent_size_3_;
protected:
	bool internal_matches_filter(const vconfig& cfg,const map_location& loc,
		bool use_flat_tod) const;
	/*
	 * cfg: an ability WML structure
	 */
	bool ability_active(const std::string& ability,const config& cfg,const map_location& loc) const;
	bool ability_affects_adjacent(const std::string& ability,const config& cfg,int dir,const map_location& loc) const;
	bool ability_affects_self(const std::string& ability,const config& cfg,const map_location& loc) const;
	bool resistance_filter_matches(const config& cfg,bool attacker,const std::string& damage_name, int res) const;

	bool has_ability_by_id(const std::string& ability) const;

	/** register a trait's name and its description for UI's use*/
	void add_trait_description(const config& trait);

	config cfg_;
	map_location loc_;
	std::set<map_location> touch_locs_;
	std::set<map_location> overlapped_locs_;
	std::set<map_location::DIRECTION> touch_dirs_;

	std::vector<std::string> advances_to_;
	std::string type_;
	std::string packee_type_;
	const unit_type* unit_type_;
	const unit_type* packee_unit_type_;
	const unit_race* race_;
	mutable t_string name_;
	t_string type_name_;
	std::string undead_variation_;
	std::string variation_;

	int hit_points_;
	int max_hit_points_;
	int experience_;
	int max_experience_;
	int level_;
	int upkeep_;
	bool random_traits_;
	unit_type::ALIGNMENT alignment_;
	std::string flag_rgb_;
	std::string image_mods_;

	std::vector<std::string> traits_;
	std::vector<std::string> modifications_;

	int side_;
	int cityno_;
	unit_race::GENDER gender_;

	fixed_t alpha_;

	int movement_;
	int max_movement_;
	mutable std::map<t_translation::t_terrain, int> movement_costs_; // movement cost cache
	mutable defense_cache defense_mods_; // defense modifiers cache
	bool end_turn_;
	bool resting_;
	int attacks_left_;
	int max_attacks_;

	int keep_turns_;
	bool human_;
	bool verifying_;

	std::set<std::string> states_;
	std::vector<bool> known_boolean_states_;
	static std::map<std::string, state_t> known_boolean_state_names_;
	bool emit_zoc_;
	bool cancel_zoc_;
	STATE state_;

	std::vector<attack_type> attacks_;
	map_location::DIRECTION facing_;

	std::vector<t_string> trait_names_;

	int unit_value_;
	int gold_income_;
	int heal_;
	map_location goto_;

	bool flying_, is_fearless_, is_healthy_;

	// utils::string_map modification_descriptions_;
	// Animations:
	std::vector<unit_animation> animations_;

	unit_animation *anim_;
	int next_idling_;
	int frame_begin_time_;

	surface desc_rect_;

	int unit_halo_;
	bool refreshing_; // avoid infinite recursion
	bool hidden_;
	bool draw_bars_;

	friend void attack_type::set_specials_context(const unit* loc, const unit*, const unit& un, bool) const;
	unit_map& units_;
	hero_map& heros_;

	/** Hold the visibility status cache for a unit, mutable since it's a cache. */
	mutable std::map<map_location, bool> invisibility_cache_;
	
	/**
	 * Clears the cache.
	 *
	 * Since we don't change the state of the object we're marked const (also
	 * required since the objects in the cache need to be marked const).
	 */
	void clear_visibility_cache() const { invisibility_cache_.clear(); }

	// rember parameter before modify according to heros in this unit
	config base_resistance_;

	surface small_portrait_;

	move_unit_spectator move_spectator_;
	//
	// artifical
	//
	bool artifical_;
	bool can_recruit_;
	bool can_reside_;
	bool base_;
	bool wall_;
	bool land_wall_;
	bool walk_wall_;
	bool attack_destroy_;
	t_translation::t_terrain terrain_;

	int arms_;
	int guard_attack_;

	hero* master_;
	hero* second_;
	hero* third_;
	size_t master_number_;
	size_t second_number_;
	size_t third_number_;
};

/** Object which temporarily resets a unit's movement */
struct unit_movement_resetter
{
	unit_movement_resetter(unit& u, bool operate=true);
	~unit_movement_resetter();

private:
	unit& u_;
	int moves_;
};

/** Returns the number of units of the side @a side_num. */
int side_units(int side_num);

/** Returns the total cost of units of side @a side_num. */
int side_units_cost(int side_num);

int side_upkeep(int side_num);

unit_map::iterator find_visible_unit(const map_location &loc,
	const team &current_team, bool see_all = false);

unit *get_visible_unit(const map_location &loc,
	const team &current_team, bool see_all = false);

struct team_data
{
	team_data() :
		units(0),
		upkeep(0),
		villages(0),
		expenses(0),
		net_income(0),
		gold(0),
		teamname()
	{
	}

	int units, upkeep, villages, expenses, net_income, gold;
	std::string teamname;
};

team_data calculate_team_data(const class team& tm, int side);

/**
 * This object is used to temporary place a unit in the unit map, swapping out
 * any unit that is already there.  On destruction, it restores the unit map to
 * its original.
 */
struct temporary_unit_placer
{
	temporary_unit_placer(unit_map& m, const map_location& loc, unit* u);
	virtual  ~temporary_unit_placer();

private:
	static std::pair<map_location, unit*> aggressing_;
	unit_map& m_;
	const map_location loc_;
	std::pair<map_location, unit*>* temp_;
};

unit* find_unit(unit_map& units, const hero& h);
bool extract_hero(unit_map& units, const hero& h);

#endif
