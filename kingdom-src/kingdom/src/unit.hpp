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
#include "base_unit.hpp"

class game_display;
class game_state;
class vconfig;
class team;
class play_controller;

#define MAX_UNIT_FIELD			180  // 127 + 50.8(127 / 2.5)
#define MAX_UNIT_ADAPTABILITY	15   // ===> S12
#define RESISTANCE_STANDARD		25
#define RESISTANCE_DIV2			12
#define RESISTANCE_DIV3			8
#define RESISTANCE_DIV4			6
#define RESISTANCE_FIRM			10

#define HIDDEN_TICKS		-3
#define NONE_TICKS			-2
#define RESIDE_TICKS		-1
#define EXPEDITE_TICKS		1
#define NONACTOR_TICKS		200000000 // non-actor ticks. they place to bh together. 
#define BIT_2_MASK(bit)		(1 << (bit))

class tscenario_env 
{
public:
	explicit tscenario_env(int _duel, int _maximal_defeated_activity, bool _vip)
		: duel(_duel)
		, maximal_defeated_activity(_maximal_defeated_activity)
		, vip(_vip)
	{}

	int duel;
	int maximal_defeated_activity;
	bool vip;
};

class move_unit_spectator {
public:
	/** add a location of a seen friend */
	void add_seen_friend(const unit* u);


	/** add the location of new seen enemy */
	void add_seen_enemy(const unit* u);


	/** get the location of an ambusher */
	const unit* get_ambusher() const;


	/** get the location of a failed teleport */
	const unit* get_failed_teleport() const;


	/** get the locations of seen enemies */
	const std::vector<const unit*>& get_seen_enemies() const;


	/** get the locations of seen friends */
	const std::vector<const unit*>& get_seen_friends() const;


	/** get new location of moved unit */
	const unit* get_unit() const;

	/** get the locations be interrupted */
	const std::vector<map_location>& get_interrupted_waypoints() const;

	/** constructor */
	move_unit_spectator(const unit_map &units);


	/** destructor */
	virtual ~move_unit_spectator();

	/** reset all locations to empty values*/
	void reset(const unit_map &units);


	/** set the location of an ambusher */
	void set_ambusher(const unit* u);


	/** set the location of a failed teleport */
	void set_failed_teleport(const unit* u);


	/** set the iterator to moved unit*/
	void set_unit(const unit* u);

	/** set the waypoints be interrupted*/
	void set_interrupted_waypoints(const std::vector<map_location>& waypoints);
private:
	const unit* ambusher_;
	const unit* failed_teleport_;
	std::vector<const unit*> seen_enemies_;
	std::vector<const unit*> seen_friends_;
	const unit* unit_;
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

struct tactic_fields {
	int32_t t_;
	int32_t turn_;
	int32_t part_;
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
	int32_t attacks_left_;	\
	int32_t side_;	\
	int32_t cityno_;	\
	int32_t x_;	\
	int32_t y_;	\
	int32_t goto_x_;	\
	int32_t goto_y_;	\
	int32_t from_x_;	\
	int32_t from_y_;	\
	int32_t facing_;	\
	int32_t food_;	\
	int32_t block_turns_;	\
	int32_t hide_turns_;	\
	int32_t alert_turns_;	\
	int32_t provoked_turns_;	\
	int32_t hot_turns_;	\
	int32_t tactic_hero_;	\
	int32_t task_;	\
	int32_t especial_; \
	int32_t bools_;	\
	int32_t ticks_;	\
	int32_t reserve_;	\
	int32_t tactic_degree_;	\
	int32_t signature_;	\
	unit_segment tactics_;	\
	unit_segment heros_army_;	\
	unit_segment type_;	\
	unit_segment traits_;	\
	unit_segment modifications_;	\

struct unit_fields_t 
{
	UNIT_FIELDS;
};

class dont_wander_lock
{
public:
	dont_wander_lock();
	~dont_wander_lock();
};

typedef std::pair<const unit_type*, std::vector<const hero*> > type_heros_pair;

#define unit_feature_val(i)		((feature_[(i) >> 2] >> (((i) % 4) << 1)) & 0x3)
#define unit_feature_val2(u, i)	(((u).feature_[(i) >> 2] >> (((i) % 4) << 1)) & 0x3)

extern uint8_t mask0_2bit[4];
#define unit_feature_set(i, v)	(feature_[(i) >> 2] = (feature_[(i) >> 2] & mask0_2bit[(i) % 4]) | (((v) & 0x3) << (((i) % 4) << 1)))
#define unit_feature_set2(u, i, v)	((u).feature_[(i) >> 2] = ((u).feature_[(i) >> 2] & mask0_2bit[(i) % 4]) | (((v) & 0x3) << (((i) % 4) << 1)))

bool actor_valid();
bool actor_can_action(const unit_map& units);
bool actor_can_continue_action(const unit_map& units, int actor_side);
bool current_can_action(const unit& u);
int calculate_end_ticks();

class unit: public base_unit, public unit_merit
{
public:
	static std::vector<std::pair<int, unit*> > null_int_unitp_pair;
	static map_location pack_base_loc;
	/**
	 * Clear the unit status cache for all units. Currently only the hidden
	 * status of units is cached this way.
	 */
	static void clear_status_caches();
	static bool draw_desc_;
	static bool ignore_pack;
	static bool dont_wander;
	static unit* actor;
	static size_t global_signature;


	friend struct unit_movement_resetter;
	// Copy constructor
	unit(const unit& u);
	/** Initilizes a unit from a config */
	unit(unit_map& units, hero_map& heros, std::vector<team>& teams, game_state& state, const config& cfg, bool use_traits=false, bool artifical = false);
	unit(unit_map& units, hero_map& heros, std::vector<team>& teams, const uint8_t* mem, bool use_traits=false, bool artifical = false);
	/** Initializes a unit from a unit type */
	unit(unit_map& units, hero_map& heros, std::vector<team>& teams, game_state& state, type_heros_pair& t, int cityno, bool real_unit, bool artifical);
	virtual ~unit();
	unit& operator=(const unit&);

	/** Advances this unit to another type */
	virtual void advance_to(const unit_type *t, bool use_traits = false, game_state *state = 0);
	const std::vector<std::string>& advances_to() const;

	void do_pack(const gamemap& map);
	void pack_to(const unit_type* t);
	void change_to(game_display& gui, const unit_type* t);

	/** The type id of the unit */
	const std::string& type_id() const { return type_; }
	const std::string& packee_type_id() const { return packee_type_; }
	const unit_type* type() const;
	const unit_type* packee_type() const;

	int especial() const { return especial_; }
	void set_special(int val) { especial_ = val; }

	/** id assigned by wml */
	const std::string& id() const { return type_; }
	/** The unique internal ID of the unit */
	size_t underlying_id() const { return 0; }

	/** The unit type name */
	const t_string& type_name() const {return type_name_;}
	const std::string& undead_variation() const {return undead_variation_;}

	/** The unit name for display */
	const t_string& name() const;
	void regenerate_name();

	/** The unit's profile */
	std::string profile() const;
	/** Information about the unit -- a detailed description of it */
	t_string unit_description() const;

	bool can_formation_master() const;
	bool can_formation_attack() const;
	bool formation_attack_enable() const;

	bool human_team_can_ai() const;

	int hitpoints() const { return hit_points_; }
	int max_hitpoints() const { return max_hit_points_; }
	int experience() const { return experience_; }
	int max_experience() const { return max_experience_; }
	int ticks() const { return ticks_; }
	int max_ticks() const { return max_ticks_; }
	int level() const { return level_; }
	void remove_movement_ai();
	void remove_attacks_ai();
	/**
	 * Adds 'xp' points to the units experience; returns true if advancement
	 * should occur
	 */
	bool can_increase_experience() const;
	virtual void get_experience(const increase_xp::ublock& ub, int xp, bool master = true, bool second = true, bool third = true);

	/** Colors for the unit's hitpoints. */
	SDL_Color hp_color() const;
	/** Colors for the unit's XP. */
	SDL_Color xp_color() const;
	/** Set to true for some scenario-specific units which should not be renamed */
	int side() const { return side_; }
	const std::string& team_color() const { return flag_rgb_; }
	unit_race::GENDER gender() const { return gender_; }
	void set_side(unsigned int new_side);
	fixed_t alpha() const { return alpha_; }

	bool is_commoner() const { return commoner_; }
	bool is_path_along_road() const { return path_along_road_; }
	bool is_artifical() const { return artifical_; } 
	bool is_city() const { return artifical_ && can_reside_; } 
	bool is_soldier() const { return hero::is_soldier(master_->number_); }
	bool hdata_variable() const;
	int cityno() const { return cityno_; } 
	void set_cityno(int new_cityno);
	void calculate_5fields();
	void modify_according_to_hero(bool fill_up_hp = true, bool fill_up_movement = false);
	void adjust();
	std::string form_tip(bool gui2 = false) const;
	std::string form_tiny_tip() const;
	std::string form_recruit_tip() const;
	// it's better name is can_recruit(), but back-compitable used
	bool can_recruits() const { return can_recruit_; }
	bool can_reside() const { return can_reside_; }
	bool wall() const { return wall_; }
	bool wall2() const;
	bool fort() const { return fort_; }
	bool transport() const { return transport_; }
	bool can_stand(const unit& mover) const;
	bool land_wall() const { return land_wall_; }
	bool walk_wall() const { return walk_wall_; }
	int arms() const { return arms_; }
	const t_translation::t_terrain& terrain() const { return terrain_; }

	hero& master() const { return *master_; }
	hero& second() const { return *second_; }
	hero& third() const { return *third_; }
	void replace_captains(const std::vector<hero*>& captains);
	void replace_captains_internal(hero& selected_hero, std::vector<hero*>& captains) const;

	int amla() const;
	int guard_attack();

	bool uncleared() const;
	bool healable() const;
	bool incapacitated() const { return get_state(ustate_tag::PETRIFIED); }
	int total_movement() const { return max_movement_; }
	int movement_left() const { return (movement_ == 0 || incapacitated()) ? 0 : movement_; }
	void set_movement(int moves);
	bool can_move() const;

	bool consider_ticks() const;
	void set_ticks(int val) { ticks_ = val; }
	int increase_ticks(int inc, int min, bool refresh_gui);
	int drawn_ticks() const { return drawn_ticks_; }
	void set_drawn_ticks(int val) { drawn_ticks_ = val; }
	int ticks_increase() const { return ticks_increase_; }
	int percent_ticks() const;
	bool is_resided() const;
	int forward_ticks(int val) const;
	int backward_ticks(int val) const;

	int attacks_left() const { return (attacks_left_ == 0 || incapacitated()) ? 0 : attacks_left_; }
	int attacks_total() const { return max_attacks_; }
	void set_attacks(int left);
	void new_turn_core();
	virtual bool new_turn(play_controller& controller, int random);
	virtual void end_turn(bool normal);
	virtual void new_scenario();
	/** Called on every draw */
	void refresh();

	void new_turn_tactics();
	void terminate_noble(bool show_message) const;

	int tactic_degree() const { return tactic_degree_; }
	void set_tactic_degree(int degree) { tactic_degree_ = degree; }
	bool tactic_existed() const;
	bool can_cast_tactic() const;
	int max_tactic_point() const;
	hero* max_tactic_hero() const;
	int percent_tactic_degree() const;
	int increase_tactic_degree(int inc);
	std::pair<hero*, int> calculate_tactic_score() const;
	bool judge_and_cast_tactic(int threshold);
	int hot_level() const;
	int tactic_degree_increment_per_turn() const;

	void set_hitpoints(int hitpoints) { hit_points_ = hitpoints; }
	bool take_hit(int damage) { hit_points_ -= damage; return hit_points_ <= 0; }
	void heal(int amount);
	void heal_all() { hit_points_ = max_hitpoints(); }
	bool resting() const { return resting_; }
	virtual void set_resting(bool rest) { resting_ = rest; }
	bool no_enter_resting() const;
	void increase_loyalty(int inc);
	void set_loyalty(int level, bool fixed = false);
	bool has_less_loyalty(int loyalty, const hero& leader);
	bool consider_loyalty() const;

	void increase_feeling(int inc);
	void goto_hate_if(const team& new_team, const hero& original_leader, int random);
	std::pair<hero*, hero*> exist_hate(const unit& target) const;

	int calculate_clear_formated_cost() const;
	bool increase_activity(int inc, bool adjusting = true);

	const std::map<std::string,std::string> get_states() const;
	void set_state(ustate_tag::state_t state, bool value);
	bool get_state(ustate_tag::state_t state) const;
	static ustate_tag::state_t find_state(const std::string& id);

	bool is_robber() const;

	// used to unit_field_t's bools field.
	enum {FIELD_HUMAN = 0x1, FIELD_RESTING = 0x2};
	
	bool has_moved() const { return movement_left() != total_movement(); }
	bool has_goto() const { return get_goto().valid(); }
	bool emits_zoc() const { return emit_zoc_ && !incapacitated();}
	bool cancel_zoc() const { return cancel_zoc_; }
	/* cfg: standard unit filter */
	bool matches_filter(const vconfig& cfg,const map_location& loc,bool use_flat_tod=false) const;

	virtual void write(config& cfg) const;

	const std::vector<attack_type>& attacks() const { return attacks_; }
	std::vector<attack_type>& attacks() { return attacks_; }
	int weapon(const std::string& id, const std::string& range = null_str) const;
	int weapon(const std::set<std::string>& id, const std::string& range = null_str) const;
	bool has_attack_weapon() const;
	
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

	int cost() const { return unit_value_; }
	int gold_income() const;
	int turn_experience() const { return turn_experience_; }

	int technology_income() const;
	int food_income() const;
	int miss_income() const;

	int food() const { return food_; }
	void set_food(int food) { food_ = food; }
	void increase_food(int addend);
	bool lack_of_food() const;
	int max_food() const;
	int value_consider_food(int value, bool decrease = true) const;
	void do_revival(unit& tactician);
	bool consider_food() const;
	bool alert_food() const;

	int block_turns() const { return block_turns_; }
	void set_block_turns(int turns) { block_turns_ = turns; }
	void inching_block_turns(bool increase);
	int hide_turns() const { return hide_turns_; }
	void set_hide_turns(int turns) { hide_turns_ = turns; }
	int alert_turns() const { return alert_turns_; }
	void set_alert_turns(int turns);
	void do_provoked(unit& tactician, int turns);
	int provoked_turns() const { return provoked_turns_; }
	void set_provoked_turns(unit& tactician, int turns);
	int max_hot_turns() const;
	int hot_turns() const { return hot_turns_; }
	void set_hot_turns(int turns);
	// it is search tactician using provoked from golbal provoke_cache.
	void set_provoked_turns_next(const unit& provoked, int turns);
	void remove_from_provoke_cache();
	hero* tactic_hero() const { return tactic_hero_; }
	void set_tactic_hero(hero& h);
	void insert_cast_tactic(game_display& disp, hero& h, unit* special);
	void insert_clear_formationed(game_display& disp);
	void insert_intervene_move(game_display& disp, const map_location& to);
	void remove_from_slot_cache(int pos = -1);
	bool human() const { return human_; }
	void set_human(bool val);

	enum {TASK_NONE, TASK_BACK, TASK_TRADE, TASK_TRANSFER, TASK_TRANSPORT, TASK_STATION, TASK_GUARD};
	int task() const { return task_; }
	void set_task(int t);
	void set_guard(const map_location& loc);
	void set_guard2(const map_location& loc);
	void remove_from_guard_cache();

	bool packed() const;

	void set_location(const map_location &loc);
	const std::set<map_location::DIRECTION>& get_touch_dirs() const;
	std::set<map_location> get_touch_locations2(const gamemap& map, const map_location& loc) const;

	const map_location& get_goto() const { return goto_; }
	void set_goto(const map_location& new_goto);
	const map_location& get_from() const { return from_; }
	void set_from(const map_location& new_from);

	virtual int upkeep() const;
	bool loyal() const;

	bool has_female() const;
	bool has_guard_center() const;
	int tactic_effect(int type, const std::string& field) const;

	size_t redraw_counter() const { return redraw_counter_; }
	void set_hidden(bool state);
	bool get_hidden() const { return hidden_; }
	bool is_flying() const { return flying_; }
	bool is_fearless() const { return false; }
	bool is_healthy() const { return false; }
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

	void add_modification(const config& modification, bool no_add = false, bool anim = false);
	void add_modification_internal(int apply_to, const config& effect, bool anim, int turns = -1);
	virtual void issue_decree(const config& effect) {}

	void apply_tactic(const ttactic* contain, const ttactic& effect, int part = 0, int turn = -1);
	bool select_tactic_can_effect(const unit& tactician, const ttactic& t);

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
			const attack_type* attack=NULL,const std::string& second_attack = null_str,
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
	void generate_traits(bool musthaveonly, game_state* state);

	void to_unstage();
	bool unstage_if_member(const hero& leader);

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

	enum {BIT_ATTACKING = 0, BIT_DEFENDING, BIT_STRONGER};
	void set_temporary_state(int bit, bool set);
	
	int character_level(int apply_to) const;
	hero* character_hero(int apply_to) const;

	int max_range() const;

	void manage_attackable_rect(bool start);
	void fill_movable_loc(const map_location& loc);
	const SDL_Rect& attackable_rect() const { return attackable_rect_; }

	bool has_mayor() const;
	enum {FORMATION_NONE, FORMATION_SECOND, FORMATION_MASTER, FORMATION_MASTER_DISABLE};
	void set_formation_flag(int flag, int flag2 = 0);
	/**
	 * Clears the cache.
	 *
	 * Since we don't change the state of the object we're marked const (also
	 * required since the objects in the cache need to be marked const).
	 */
	void clear_visibility_cache() const { invisibility_cache_.clear(); }

	bool loc_is_align_with(const map_location& relative, bool x) const;
	bool compare_action_order(const unit& that) const;

	bool is_capital(const std::vector<team>& teams) const;
	surface generate_access_surface(int width, int height, bool greyscale) const;

	bool require_sort() const { return consider_ticks(); }
	bool sort_compare(const base_unit& that) const;

public:
	uint16_t leadership_;
	uint16_t force_;
	uint16_t intellect_;
	uint16_t spirit_;
	uint16_t charm_;
	uint8_t feature_[HEROS_FEATURE_M2BYTES];
	uint16_t adaptability_[HEROS_MAX_ARMS];
	uint16_t skill_[HEROS_MAX_SKILL];
	class character_data
	{
	public:
		character_data(hero* _h, int _ch, int _level)
			: h(_h)
			, ch(_ch)
			, level(_level)
		{}
		hero* h;
		int ch;
		int level;
	};
	std::map<int, character_data> characters_;

	map_location adjacent_[12];	// 和loc_邻近的最多12个格子
	size_t adjacent_size_;

	map_location adjacent_2_[18];	// 和loc_邻近的最多18个格子
	size_t adjacent_size_2_;

	map_location adjacent_3_[24];	// 和loc_邻近的最多24个格子
	size_t adjacent_size_3_;

	mutable std::vector<unit*> resist_helper_;
	size_t signature;
	bool ticks_adjusting;

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

	std::vector<std::string> advances_to_;
	std::string type_;
	std::string packee_type_;
	const unit_type* unit_type_;
	const unit_type* packee_unit_type_;
	int especial_;
	const unit_race* race_;
	t_string type_name_;
	std::string undead_variation_;
	std::string variation_;
	
	int hit_points_;
	int max_hit_points_;
	int experience_;
	int max_experience_;
	int ticks_;
	int max_ticks_;
	int drawn_ticks_;
	int ticks_increase_; // tactic result to increase
	int level_;
	int upkeep_;
	unit_type::ALIGNMENT alignment_;
	std::string flag_rgb_;
	std::string image_mods_;

	std::vector<std::string> traits_;
	std::vector<std::string> modifications_;

	class tunit_tactic
	{
	public:
		tunit_tactic(const ttactic* tactic, int turn, int part)
			: tactic_(tactic)
			, turn_(turn)
			, part_(part)
		{}
		const ttactic* tactic_;
		int turn_;
		int part_;
	};
	std::vector<tunit_tactic> tactics_;

	int side_;
	int cityno_;
	unit_race::GENDER gender_;

	fixed_t alpha_;

	int movement_;
	int max_movement_;
	mutable std::map<t_translation::t_terrain, int> movement_costs_; // movement cost cache
	mutable defense_cache defense_mods_; // defense modifiers cache
	bool resting_;
	int tactic_degree_;
	int attacks_left_;
	int max_attacks_;

	int food_;
	int block_turns_;
	int hide_turns_;
	int alert_turns_;
	int provoked_turns_;
	int hot_turns_;
	hero* tactic_hero_;
	bool human_;

	int task_;

	int32_t states_;
	bool emit_zoc_;
	bool cancel_zoc_;
	STATE state_;

	std::vector<attack_type> attacks_;
	map_location::DIRECTION facing_;

	std::vector<t_string> trait_names_;

	int unit_value_;
	int gold_income_;
	int technology_income_;
	int food_income_;
	int miss_income_;
	int heal_;
	int turn_experience_;
	map_location goto_;
	map_location from_;

	bool flying_;
	size_t redraw_counter_;

	// Animations:
	unit_animation *anim_;
	int next_idling_;
	int frame_begin_time_;

	surface desc_rect_;

	int unit_halo_;
	bool hidden_;
	bool draw_bars_;

	friend void attack_type::set_specials_context(const unit* loc, const unit*, const unit& un, bool) const;
	unit_map& units_;
	hero_map& heros_;
	std::vector<team>& teams_;

	/** Hold the visibility status cache for a unit, mutable since it's a cache. */
	mutable std::map<map_location, bool> invisibility_cache_;
	
	// rember parameter before modify according to heros in this unit
	config base_resistance_;

	surface small_portrait_;

	move_unit_spectator move_spectator_;

	bool commoner_;
	bool path_along_road_;
	//
	// artifical
	//
	bool artifical_;
	bool can_recruit_;
	bool can_reside_;
	bool wall_;
	bool fort_;
	bool transport_;
	bool land_wall_;
	bool walk_wall_;
	t_translation::t_terrain terrain_;

	int arms_;
	int guard_attack_;

	hero* master_;
	hero* second_;
	hero* third_;
	size_t master_number_;
	size_t second_number_;
	size_t third_number_;

	std::map<std::string, int> tactic_compare_resistance_;
	std::map<std::string, int> tactic_compare_damage_;
	int tactic_compare_movement_;

	int formation_flag_;
	int formation_flag2_;
	uint32_t temporary_state_;
	SDL_Rect attackable_rect_;
};

enum {NO_DUEL, RANDOM_DUEL, ALWAYS_DUEL};
namespace tent {
struct tlobby_side_param {
	std::string current_player;
	std::string ally;
};
extern std::vector<tlobby_side_param> lobby_side_params;
extern int io_type;

extern hero* leader;
extern int human_leader_number;
extern int human_count;
extern int ai_count;
extern int employ_count;
extern mode_tag::tmode mode;
extern std::pair<std::string, int> subcontinent;
extern int turns;
extern int duel;
extern bool turn_based;

void reset();
bool tower_mode();
std::string subcontient_to_str();
void subcontient_from_str(const std::string& str);
}

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

unit* find_visible_unit(const map_location &loc, const team &current_team, bool see_all = false);

unit *get_visible_unit(const map_location &loc,
	const team &current_team, bool see_all = false);

struct team_data
{
	team_data() :
		units(0),
		upkeep(0),
		villages(0),
		net_income(0),
		gold(0),
		technology_net_income(0),
		teamname()
	{
	}

	int units, upkeep, villages, net_income, gold, technology_net_income;
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
	static std::pair<map_location, base_unit*> aggressing_;
	unit_map& m_;
	const map_location loc_;
	std::pair<map_location, base_unit*> temp_;
};

bool extract_hero(unit_map& units, const hero& h);

unit* loc_in_alert_area(const std::vector<team>& teams, unit& u, const map_location& from, const map_location& to);
extern bool provoke_cache_ready;
std::string provoke_cache_2_str();
void str_2_provoke_cache(unit_map& units, hero_map& heros, const std::string& str);
unit* find_provoke(const unit* provoked);

namespace slot_cache {

struct tslot
{
	enum {NONE, ACTIVE_TACTIC, CAST_TACTIC, CLEAR_FORMATIONED, MOVE};
	tslot(int type, unit* u)
		: type(type)
		, u(u)
		, tactic(NULL, NULL)
		, move(map_location::null_location)
	{}

	int cost() const;
	std::string label() const;

	int type;
	unit* u;
	
	struct tcast_tactic {
		tcast_tactic(hero* h, unit* special)
			: h(h)
			, special(special)
		{}

		hero* h;
		unit* special;
	};
	tcast_tactic tactic;

	struct tmove {
		tmove(const map_location& to)
			: to(to)
		{}

		map_location to;
	};
	tmove move;
};

extern std::vector<tslot> cache;
extern bool ready;
extern unit* selected_u;

std::string cache_2_str();
void str_2_cache(unit_map& units, hero_map& heros, const std::string& str);
bool find(const unit& u);
void insert_active_tactic(unit& u);
void insert_cast_tactic(unit& tactician, hero& h, unit* special);
void insert_clear_formationed(unit& u);
void insert_intervene_move(unit& u, const map_location& to);
int slot_cost(const slot_cache::tslot& slot);

}

namespace guard_cache {

struct tguard {
	tguard(const map_location& loc = map_location::null_location, int radius = -1)
		: loc(loc),
		radius(radius)
	{}
	map_location loc;
	int radius;
};
extern tguard null_guard;
extern std::map<unit*, tguard> cache;
extern bool ready;

std::string cache_2_str();
void str_2_cache(unit_map& units, hero_map& heros, const std::string& str);
tguard& find(const unit& u);

}

#endif
