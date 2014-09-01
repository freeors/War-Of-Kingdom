/* $Id: team.hpp 47612 2010-11-21 13:56:52Z mordante $ */
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
#ifndef TEAM_H_INCLUDED
#define TEAM_H_INCLUDED

#include "color_range.hpp"
#include "game_config.hpp"
#include "unit.hpp"
#include "terrain_filter.hpp"

class gamemap;
class card;
class card_map;
class play_controller;

struct team_fields_t 
{
	int32_t size_;
	int32_t side_;
	int32_t action_bonus_count_;
	int32_t allow_player_;
	int32_t disallow_observers_;
	int32_t fog_;
	int32_t shroud_;
	int32_t gold_;
	int32_t gold_add_;
	int32_t start_gold_;
	int32_t income_;
	int32_t leader_;
	int32_t navigation_;
	int32_t objectives_changed_;
	int32_t previous_turn_;
	int32_t capital_;
	int32_t share_maps_;
	int32_t share_view_;
	int32_t village_gold_;
	int32_t controller_;
	int32_t bomb_turns_;

	// statistic
	int32_t cause_damage_;
	int32_t been_damage_;
	int32_t defeat_units_;
	int32_t perfect_turns_;
	int32_t defeat_units_one_turn_;

	unit_segment id_;
	unit_segment save_id_;
	unit_segment current_player_;
	unit_segment build_;
	unit_segment color_;
	unit_segment flag_;
	unit_segment flag_icon_;
	unit_segment name_;
	unit_segment objectives_;
	unit_segment diplomatisms_;
	unit_segment user_team_name_;
	unit_segment strategies_;
	unit_segment features_;
	unit_segment music_;
	unit_segment countdown_time_;
	unit_segment candidate_cards_;
	unit_segment holded_cards_;
	unit_segment holded_treasures_;
	unit_segment holded_technologies_;
	unit_segment half_technologies_;
	unit_segment ing_technology_;
	unit_segment commercials_;
	unit_segment village_;
	unit_segment cfg_;
};

struct diplomatism_data_fields_t {
	int ally_;
	int forbid_turns_;
};

struct diplomatism_data {
	diplomatism_data() : ally_(false), forbid_turns_(0) {}
	bool ally_;
	int forbid_turns_;
};

struct strategy_fields_t {
	int target_;
	int type_;
	int start_turn_;
	int impletement_turns_;
	int allies_;
};

class strategy
{
public:
	static int default_impletement_turns;

	enum {NONE, AGGRESS, DEFEND};
	strategy(int target, int type, int impletement_turns);

	bool operator==(const strategy& that);

	// city maybe advance, save point is unsafe.
	int target_;
	int type_;
	std::set<int> allies_;
	int start_turn_;	// strategy's start turn 
	int impletement_turns_; // persist in turns
};

struct arms_feature {
	arms_feature(int arms = -1, int level = -1, int feature = -1) 
		: arms_(arms)
		, level_(level)
		, feature_(feature)
	{}
	int arms_;
	int level_;
	int feature_;
};

struct bh_evaluate
{
	bool valid; // it is indicate struct wheather valid or not, not team's field.

	int capital;
};

/**
 * This class stores all the data for a single 'side' (in game nomenclature).
 * E.g., there is only one leader unit per team.
 */
class team_
{
public:
	team_();

	void increase_defeat_units_one_turn(int inc);
	
	int leadership_speed_;
	int force_speed_;
	int intellect_speed_;
	int spirit_speed_;
	int charm_speed_;
	int arms_speed_[HEROS_MAX_ARMS];

	bool ignore_zoc_on_wall_;
	bool land_enemy_wall_;
	int tactic_degree_increase;
	int ticks_increase;
	int interlink_increase_;
	int cooperate_increase_;

	int navigation_civi_increase_;
	int repair_increase_;
	int village_gold_increase_;
	int market_increase_;
	int technology_increase_;

	int previous_turn;

	// statistic
	int cause_damage_;
	int been_damage_;
	int defeat_units_;
	int perfect_turns_;
	int defeat_units_one_turn_;

	// strategy parameter according to tent mode
	bool restrict_movement;
	bool auto_recruit;
	bool auto_move_human;
	bool support_bomb;
	bool kill_income;
	int max_troops;
	bool ea_artifical_neutral;
	bool double_reset_goto;
	bool allow_intervene;
	bool allow_active;

	// stratagem
	bool stratagem_ally_join;
	bool stratagem_decrease_cost;
	bool stratagem_half_kill;
	bool stratagem_half_heal;
	bool stratagem_baffle_fightback;
	bool stratagem_baffle_tactic;

	bh_evaluate bh_eval;

protected:
	std::set<const unit_type*> can_build_;
	std::vector<hero*> last_active_tactic_;
};

class team: public team_
{
	class shroud_map {
	public:
		shroud_map() : enabled_(false), data_() {}

		void place(int x, int y);
		bool clear(int x, int y);
		void reset();

		bool value(int x, int y) const;
		bool shared_value(const std::vector<const shroud_map*>& maps, int x, int y) const;

		bool copy_from(const std::vector<const shroud_map*>& maps);

		std::string write() const;
		void read(const std::string& shroud_data);
		void merge(const std::string& shroud_data);

		bool enabled() const { return enabled_; }
		void set_enabled(bool enabled) { enabled_ = enabled; }
	private:
		bool enabled_;
		std::vector<std::vector<bool> > data_;
	};
public:

	struct team_info
	{
		team_info();
		void read(const config &cfg);
		void write(config& cfg) const;
		std::string name;
		int gold;
		int start_gold;
		bool gold_add;
		int income;
		int income_per_village;
		std::string team_name;
		std::string save_id;
		// 'id' of the current player (not necessarily unique)
		std::string current_player;
		std::string countdown_time;
		int action_bonus_count;

		std::string flag;
		std::string flag_icon;

		std::string description;

		t_string objectives; /** < Team's objectives for the current level. */

		/** Set to true when the objectives for this time changes.
		 * Reset to false when the objectives for this team have been
		 * displayed to the user. */
		bool objectives_changed;

		controller_tag::CONTROLLER controller;
		char const *controller_string() const;

		bool share_maps, share_view;
		bool disallow_observers;
		bool allow_player;

		std::string music;

		std::string color;

		int side;
	};


	static std::map<int, color_range> team_color_range_;
	static const int default_team_gold;

	static int empty_side;

	team(unit_map& units, hero_map& heros, card_map& cards, const config& cfg, const gamemap& map, int gold, size_t team_size);
	team(unit_map& units, hero_map& heros, card_map& cards, const uint8_t* mem, const gamemap& map, int gold, size_t team_size);
	team(const team& that);

	~team();

	team& operator=(const team& that);

	void build(const config &cfg, const gamemap &map, int gold = default_team_gold);

	void write(config& cfg) const;

	void write(uint8_t* mem) const;
	void read(const uint8_t* mem, const gamemap& map, size_t team_size);

	bool owns_village(const map_location& loc) const;

	int side() const { return info_.side; }
	int gold() const { return gold_; }
	int start_gold() const { return info_.start_gold; }
	bool gold_add() const { return info_.gold_add; }
	int base_income() const;
	int village_gold() const { return info_.income_per_village; }
	void set_village_gold(int income) { info_.income_per_village = income; }
	bool gold_can_build_ea() const;
	int total_income() const;
	int side_upkeep() const;
	int total_technology_income() const;
	void new_turn(int random);
	void get_shared_maps();
	void set_gold(int amount) { gold_ = amount; }
	void spend_gold(const int amount) { gold_ -= amount; }
	void set_gold_add(bool b) {info_.gold_add = b; }
	void set_base_income(int amount) { info_.income = amount - game_config::base_income; }
	int countdown_time() const {  return countdown_time_; }
	void set_countdown_time(const int amount)
		{ countdown_time_ = amount; }
	int action_bonus_count() const { return action_bonus_count_; }
	void set_action_bonus_count(const int count) { action_bonus_count_ = count; }
	void set_current_player(const std::string& player)
		{ info_.current_player = player; }

	int navigation() const { return navigation_; }
	void set_navigation(int navigation) { navigation_ = navigation; }
	int add_navigation(int increment);

	int bomb_turns() const { return bomb_turns_; }
	void set_bomb_turns(int turns) { bomb_turns_ = turns; }
	void increase_bomb_turns();
	bool can_bomb() const;

	void add_modification_internal(int apply_to, const config& effect);
	void apply_holded_technologies_finish();
	void readjust_all_unit();

	void set_builds(const std::set<std::string>& builds);

	const std::set<const unit_type*>& builds() const { return can_build_; }

	const std::string& name() const
		{ return info_.name; }
	const std::string& save_id() const { return info_.save_id; }
	const std::string& current_player() const { return info_.current_player; }

	void set_objectives(const t_string& new_objectives, bool silently=false);
	void set_objectives_changed(bool c = true) { info_.objectives_changed = c; }
	void reset_objectives_changed() { info_.objectives_changed = false; }

	const t_string& objectives() const { return info_.objectives; }
	bool objectives_changed() const { return info_.objectives_changed; }

	bool is_enemy(int n) const;
	bool ally_forbided(int n) const;
	void set_ally(int n, bool alignment = true, bool dialog = false, bool adjust = false);
	void set_forbid_turns(int n, int turns);
	std::map<int, diplomatism_data>& diplomatisms() { return diplomatisms_; }
	const std::map<int, diplomatism_data>& diplomatisms() const { return diplomatisms_; }

	void add_strategy(const strategy& s);
	void erase_strategy(int target, bool dialog = true);
	strategy& get_strategy(int target, int type = strategy::NONE);
	const strategy& get_strategy(int target, int type = strategy::NONE) const;
	std::vector<strategy>& strategies() { return strategies_; };
	const std::vector<strategy>& strategies() const { return strategies_; }
	void erase_strategies(bool show = true);
	void erase_ally(int side);
	
	std::vector<arms_feature>& features() { return features_; }
	const std::vector<arms_feature>& features() const { return features_; }

	void rpg_changed(bool independence = false);

	bool has_seen(unsigned int index) const {
		if(!uses_shroud() && !uses_fog()) return true;
		if(index < seen_.size()) {
			return seen_[index];
		} else {
			return false;
		}
	}
	void see(unsigned int index) {
		if(index >= seen_.size()) {
			seen_.resize(index+1);
		}
		seen_[index] = true;
	}

	controller_tag::CONTROLLER controller() const { return info_.controller; }
	char const *controller_string() const { return info_.controller_string(); }
	const std::string& color() const { return info_.color; }
	bool is_human() const { return info_.controller == controller_tag::HUMAN; }
	bool is_network_human() const { return info_.controller == controller_tag::NETWORK; }
	bool is_ai() const { return info_.controller == controller_tag::AI; }
	bool is_empty() const { return info_.controller == controller_tag::EMPTY; }

	bool is_local() const { return is_human() || is_ai(); }
	bool is_network() const { return is_network_human(); }

	void make_human() { info_.controller = controller_tag::HUMAN; }
	void make_network() { info_.controller = controller_tag::NETWORK; }
	void make_ai() { info_.controller = controller_tag::AI; }
	// Should make the above make_*() functions obsolete, as it accepts controller
	// by lexical or numerical id
	void change_controller(controller_tag::CONTROLLER controller);
	void change_controller(const std::string& controller);

	std::string team_name() const;

	const std::string& flag() const { return info_.flag; }
	const std::string& flag_icon() const { return info_.flag_icon; }

	hero* leader() const { return &heros_[leader_]; }
	void set_leader(hero& h);
	const artifical* capital() const { return capital_; }
	int capital_number() const;
	bool set_capital(artifical* city);

	int cost_exponent() const;
	void add_city(artifical* city);
	void erase_city(const artifical* city);
	std::vector<artifical*>& holden_cities() { return holden_cities_; }
	const std::vector<artifical*>& holden_cities() const { return holden_cities_; }

	int character() const;
	int technology_max_experience(const ttechnology& t) const;

	bool add_card(size_t number, bool replay = false, bool dialog = false);
	bool erase_card(int index, bool replay = false, bool dialog = false);
	int get_random_decree(artifical& city, int random);
	bool condition_card(const card& c, const map_location& loc);
	void consume_card(const card& c, const map_location& loc, std::vector<std::pair<int, unit*> >& touched = unit::null_int_unitp_pair, int index = -1, bool to_recorder = false);
	void card_touched(const card& c, const map_location& loc, std::vector<std::pair<int, unit*> >& pairs, std::string& disable_str);
	std::vector<size_t>& candidate_cards() { return candidate_cards_; }
	const std::vector<size_t>& candidate_cards() const { return candidate_cards_; }
	std::vector<size_t>& holded_cards() { return holded_cards_; }
	const std::vector<size_t>& holded_cards() const { return holded_cards_; }
	// card& holded_card(int index);
	const card& holded_card(int index) const;

	std::vector<size_t>& holded_treasures() { return holded_treasures_; }
	const std::vector<size_t>& holded_treasures() const { return holded_treasures_; }
	void erase_treasure(int tid);
	void erase_treasure2(int index);
	void find_treasure(hero_map& heros, play_controller& controller, int pos);

	std::vector<const ttechnology*>& holded_technologies() { return holded_technologies_; }
	const std::vector<const ttechnology*>& holded_technologies() const { return holded_technologies_; }
	std::map<const ttechnology*, int>& half_technologies() { return half_technologies_; }
	const std::map<const ttechnology*, int>& half_technologies() const { return half_technologies_; }
	void select_ing_technology(const ttechnology* set = NULL);
	const ttechnology* ing_technology() { return ing_technology_; }
	const ttechnology* ing_technology() const { return ing_technology_; }
	void reselect_ing_technology();
	void insert_technology(const ttechnology& t);
	void do_technology_income(int income);
	int calculate_technology_income(int income) const;

	int current_stratagem() const;
	void insert_stratagem(const ttechnology& s, bool erase);
	void erase_stratagem();

	void select_leader_noble(bool show_message);
	void fill_normal_noble(bool init, bool show_message);
	const std::map<int, hero*>& appointed_nobles(bool clear = false);
	const std::set<int>& unappoint_nobles();
	void appoint_noble(hero& h, int noble, bool show_message);

	const SDL_Rect& city_rect() const { return city_rect_; }

	void add_troop(unit* troop);
	void erase_troop(const unit* troop);
	void clear_troop();
	std::pair<unit**, size_t> field_troop() { return std::make_pair(field_troops_, field_troops_vsize_); }
	const std::pair<unit**, size_t> field_troop() const { return std::make_pair(field_troops_, field_troops_vsize_); }
	int holden_troops() const;

	//Returns true if the hex is shrouded/fogged for this side, or
	//any other ally with shared vision.
	bool shrouded(const map_location& loc) const;
	bool fogged(const map_location& loc) const;

	bool uses_shroud() const { return shroud_.enabled(); }
	bool uses_fog() const { return fog_.enabled(); }
	bool fog_or_shroud() const { return uses_shroud() || uses_fog(); }
	bool clear_shroud(const map_location& loc) { return shroud_.clear(loc.x+1,loc.y+1); }
	void place_shroud(const map_location& loc) { shroud_.place(loc.x+1,loc.y+1); }
	bool clear_fog(const map_location& loc) { return fog_.clear(loc.x+1,loc.y+1); }
	void refog() { fog_.reset(); }
	void set_shroud(bool shroud) { shroud_.set_enabled(shroud); }
	void set_fog(bool fog) { fog_.set_enabled(fog); }

	/** Merge a WML shroud map with the shroud data of this player. */
	void merge_shroud_map_data(const std::string& shroud_data);

	bool knows_about_team(size_t index, bool is_multiplayer) const;
	bool copy_ally_shroud();

	bool auto_shroud_updates() const { return auto_shroud_updates_; }
	void set_auto_shroud_updates(bool value) { auto_shroud_updates_ = value; }
	bool get_disallow_observers() const {return info_.disallow_observers; };
	std::string map_color_to() const { return info_.color; };

	static int nteams();

	//function which, when given a 1-based side will return the color used by that side.
	static const color_range get_side_color_range(int side);
	static Uint32 get_side_rgb(int side) { return(get_side_color_range(side).mid()); }
	static Uint32 get_side_rgb_max(int side) { return(get_side_color_range(side).max()); }
	static Uint32 get_side_rgb_min(int side) { return(get_side_color_range(side).min()); }
	static const SDL_Color get_minimap_color(int side);
	static std::string get_side_color_index(int side);
	static std::string get_side_highlight(int side);

	/**set the share maps attribute */
	void set_share_maps( bool share_maps );
	/**set the share view attribute */
	void set_share_view( bool share_view );

	/** clear the shroud, fog, and enemies cache for all teams*/
	static void clear_caches();

	// this team cannot move avoid locs
	terrain_filter& avoid() { return avoid_; }
	const terrain_filter& avoid() const { return avoid_; }
	bool has_avoid() const { return has_avoid_; }

	std::vector<hero*>& commercials() { return commercials_; }
	const std::vector<hero*>& commercials() const { return commercials_; }

	bool defeat_vote() const;
	int may_build_count() const;

	std::string form_results_of_battle_tip(const std::string& prefix = null_str) const;

	std::vector<unit*> active_tactics() const;
	void save_last_active_tactic(const std::vector<unit*>& active);
	const std::vector<hero*>& last_active_tactic() const { return last_active_tactic_; }
	void refresh_tactic_slots(game_display& disp) const;

	void calculate_strategy_according_to_mode();
	void fresh_to_field_troop(int random);

	bool all_city_deputed(const artifical* exclude) const;
	void set_all_city_deputed(game_display& disp, bool set) const;

private:
	//Make these public if you need them, but look at knows_about_team(...) first.
	bool share_maps() const { return info_.share_maps; }
	bool share_view() const { return info_.share_view; }

	const std::vector<const shroud_map*>& ally_shroud(const std::vector<team>& teams) const;
	const std::vector<const shroud_map*>& ally_fog(const std::vector<team>& teams) const;

	void calculate_skill_feature();

	unit_map& units_;
	hero_map& heros_;
	card_map& cards_;

	std::vector<size_t> candidate_cards_;
	std::vector<size_t> holded_cards_;
	std::vector<size_t> holded_treasures_;

	std::vector<const ttechnology*> holded_technologies_;
	std::map<const ttechnology*, int> half_technologies_;
	const ttechnology* ing_technology_;

	std::map<int, hero*> appointed_nobles_;
	std::set<int> unappoint_nobles_;

	int leader_;
	artifical* capital_;
	std::vector<artifical*> holden_cities_;
	std::map<int, int> character_;

	int gold_;
	int navigation_;
	int bomb_turns_;

	shroud_map shroud_, fog_;

	bool auto_shroud_updates_;

	team_info info_;

	int countdown_time_;
	int action_bonus_count_;

	void calculate_diplomatisms(const std::string& team_name, size_t team_size) const;
	mutable std::map<int, diplomatism_data> diplomatisms_;

	std::vector<strategy> strategies_;
	std::vector<arms_feature> features_;

	mutable std::vector<bool> seen_;

	mutable std::vector<const shroud_map*> ally_shroud_, ally_fog_;

	terrain_filter avoid_;
	bool has_avoid_;
	// cfg form in terrain_filter is pointer, forbit pointer to avoid_cfg_.
	config avoid_cfg_;

	/**
	 * Whiteboard planned actions for this team.
	 */
	// boost::shared_ptr<wb::side_actions> planned_actions_;

	SDL_Rect city_rect_;
	std::vector<hero*> commercials_;
	bool skill_feature_;

	int w_;
	int h_;
	unit** field_troops_;
	size_t field_troops_vsize_;
};

namespace teams_manager {
	const std::vector<team> &get_teams();
}

namespace player_teams {
	int village_owner(const map_location& loc);
}

//FIXME: this global method really needs to be moved into play_controller,
//or somewhere else that makes sense.
bool is_observer();

//function which will validate a side. Trows game::game_error
//if the side is invalid
void validate_side(int side); //throw game::game_error

#endif

