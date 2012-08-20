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
// #include "savegame_config.hpp"
#include "unit.hpp"
#include "terrain_filter.hpp"

class gamemap;
class card;
class card_map;

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
	int32_t hidden_;
	int32_t start_gold_;
	int32_t income_;
	int32_t leader_;
	int32_t navigation_;
	int32_t objectives_changed_;
	int32_t persistent_;
	int32_t recall_cost_;
	int32_t scroll_to_leader_;
	int32_t share_maps_;
	int32_t share_view_;
	int32_t village_gold_;
	int32_t controller_;
	int32_t ai_aggression_; // * 100
	int32_t ai_attack_depth_;
	int32_t ai_td_mode_;

	unit_segment id_;
	unit_segment save_id_;
	unit_segment current_player_;
	unit_segment build_;
	unit_segment recruit_;
	unit_segment color_;
	unit_segment flag_;
	unit_segment flag_icon_;
	unit_segment name_;
	unit_segment objectives_;
	unit_segment diplomatisms_;
	unit_segment user_team_name_;
	unit_segment strategies_;
	unit_segment music_;
	unit_segment countdown_time_;
	unit_segment candidate_cards_;
	unit_segment holded_cards_;
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

/**
 * This class stores all the data for a single 'side' (in game nomenclature).
 * E.g., there is only one leader unit per team.
 */
class team_
{
public:
	team_();

protected:
	std::set<const unit_type*> can_recruit_;
	std::set<const unit_type*> can_rpg_recruit_;
	std::set<const unit_type*> can_build_;
	int max_recruit_cost_;
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
		int recall_cost;
		std::string team_name;
		t_string user_team_name;
		std::string save_id;
		// 'id' of the current player (not necessarily unique)
		std::string current_player;
		std::string countdown_time;
		int action_bonus_count;

		std::string flag;
		std::string flag_icon;

		std::string description;

		bool scroll_to_leader;

		t_string objectives; /** < Team's objectives for the current level. */

		/** Set to true when the objectives for this time changes.
		 * Reset to false when the objectives for this team have been
		 * displayed to the user. */
		bool objectives_changed;

		enum CONTROLLER { HUMAN, HUMAN_AI, AI, NETWORK, NETWORK_AI, EMPTY };
		CONTROLLER controller;
		char const *controller_string() const;

		bool share_maps, share_view;
		bool disallow_observers;
		bool allow_player;
		bool hidden;

		std::string music;

		std::string color;

		int side;
		bool persistent;
	};


	static std::map<int, color_range> team_color_range_;
	static const int default_team_gold;

	static hero* player_leader() { return player_leader_; }
	static void set_player_leader(hero* leader);
	static hero* player_leader_;

	team(unit_map& units, hero_map& heros, card_map& cards, const config& cfg, const gamemap& map, int gold, size_t team_size);
	team(unit_map& units, hero_map& heros, card_map& cards, const uint8_t* mem, const gamemap& map, int gold = default_team_gold);
	team(const team& that);

	~team();

	team& operator=(const team& that);

	void build(const config &cfg, const gamemap &map, int gold = default_team_gold);

	void write(config& cfg) const;

	void write(uint8_t* mem) const;
	void read(const uint8_t* mem, const gamemap& map);

	bool get_village(const map_location&);
	void lose_village(const map_location&);
	void clear_villages() { villages_.clear(); }
	const std::set<map_location>& villages() const { return villages_; }
	bool owns_village(const map_location& loc) const
		{ return villages_.count(loc) > 0; }

	int side() const { return info_.side; }
	int gold() const { return gold_; }
	int start_gold() const { return info_.start_gold; }
	bool gold_add() const { return info_.gold_add; }
	int base_income() const;
	int village_gold() const { return info_.income_per_village; }
	int recall_cost() const { return info_.recall_cost; }
	void set_village_gold(int income) { info_.income_per_village = income; }
	void set_recall_cost(int cost) { info_.recall_cost = cost; }
	int total_income() const;
	int side_upkeep() const;
	void new_turn();
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
	int add_navigation(int increment);

	bool get_scroll_to_leader() const {return info_.scroll_to_leader;}

	int max_recruit_cost();
	const std::set<const unit_type*>& recruits();
	void add_recruit(const std::string &);
	void set_recruits(const std::set<std::string>& recruits);

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
	void set_ally(int n, bool alignment = true);
	void set_forbid_turns(int n, int turns);
	std::map<int, diplomatism_data>& diplomatisms() { return diplomatisms_; }
	const std::map<int, diplomatism_data>& diplomatisms() const { return diplomatisms_; }

	void add_strategy(const strategy& s);
	void erase_strategy(int target);
	strategy& get_strategy(int target, int type = strategy::NONE);
	const strategy& get_strategy(int target, int type = strategy::NONE) const;
	std::vector<strategy>& strategies() { return strategies_; };
	const std::vector<strategy>& strategies() const { return strategies_; }

	void rpg_changed();

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

	team_info::CONTROLLER controller() const { return info_.controller; }
	char const *controller_string() const { return info_.controller_string(); }
	const std::string& color() const { return info_.color; }
	bool is_human() const { return info_.controller == team_info::HUMAN; }
	bool is_human_ai() const { return info_.controller == team_info::HUMAN_AI; }
	bool is_network_human() const { return info_.controller == team_info::NETWORK; }
	bool is_network_ai() const { return info_.controller == team_info::NETWORK_AI; }
	bool is_ai() const { return info_.controller == team_info::AI || is_human_ai(); }
	bool is_empty() const { return info_.controller == team_info::EMPTY; }

	bool is_local() const { return is_human() || is_ai(); }
	bool is_network() const { return is_network_human() || is_network_ai(); }

	void make_human() { info_.controller = team_info::HUMAN; }
	void make_human_ai() { info_.controller = team_info::HUMAN_AI; }
	void make_network() { info_.controller = team_info::NETWORK; }
	void make_network_ai() { info_.controller = team_info::NETWORK_AI; }
	void make_ai() { info_.controller = team_info::AI; }
	// Should make the above make_*() functions obsolete, as it accepts controller
	// by lexical or numerical id
	void change_controller(team_info::CONTROLLER controller);
	void change_controller(const std::string& controller);

	std::string team_name() const;
	const t_string &user_team_name() const { return info_.user_team_name; }
	void change_team(const std::string &name, const t_string &user_name = "");

	const std::string& flag() const { return info_.flag; }
	const std::string& flag_icon() const { return info_.flag_icon; }

	hero* leader() const { return &heros_[leader_]; }
	void set_leader(hero* leader);
	int cost_exponent() const;
	void add_city(artifical* city);
	void erase_city(const artifical* city);
	std::vector<artifical*>& holded_cities() { return holded_cities_; }
	const std::vector<artifical*>& holded_cities() const { return holded_cities_; }

	bool add_card(size_t number, bool replay = false, bool dialog = false);
	bool erase_card(int index, bool replay = false);
	bool condition_card(int index, const map_location& loc);
	void consume_card(int index, const map_location& loc, bool replay = false, std::map<size_t, unit*>& touched_heros = unit::null_size_unitp_pair);
	void card_touched_heros(int index, const map_location& loc, std::vector<std::pair<size_t, unit*> >& pairs, std::string& disable_str);
	std::vector<size_t>& candidate_cards() { return candidate_cards_; }
	const std::vector<size_t>& candidate_cards() const { return candidate_cards_; }
	std::vector<size_t>& holded_cards() { return holded_cards_; }
	const std::vector<size_t>& holded_cards() const { return holded_cards_; }
	card& holded_card(int index);
	const card& holded_card(int index) const;

	SDL_Rect& city_rect() { return city_rect_; }
	const SDL_Rect& city_rect() const { return city_rect_; }

	void add_troop(unit* troop);
	void erase_troop(const unit* troop);
	void clear_troop();
	std::pair<unit**, size_t> field_troop() { return std::make_pair<unit**, size_t>(field_troops_, field_troops_vsize_); }
	const std::pair<unit**, size_t> field_troop() const { return std::make_pair<unit**, size_t>(field_troops_, field_troops_vsize_); }

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
	bool hidden() const { return info_.hidden; }
	void set_hidden(bool value) { info_.hidden=value; }
	bool persistent() const {return info_.persistent;}

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

	void set_commercials(std::vector<hero*>& commercials);
	void erase_commercial(hero* h);
	std::vector<hero*>& commercials() { return commercials_; }
	const std::vector<hero*>& commercials() const { return commercials_; }
	int commercial_exploiture(bool only_idle = false) const;
	void active_commercial();

	// config to_config() const;

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

	int leader_;
	std::vector<artifical*> holded_cities_;

	int gold_;
	std::set<map_location> villages_;
	int navigation_;

	shroud_map shroud_, fog_;

	bool auto_shroud_updates_;

	team_info info_;

	int countdown_time_;
	int action_bonus_count_;

	void calculate_diplomatisms(const std::string& team_name, size_t team_size) const;
	mutable std::map<int, diplomatism_data> diplomatisms_;

	std::vector<strategy> strategies_;

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

