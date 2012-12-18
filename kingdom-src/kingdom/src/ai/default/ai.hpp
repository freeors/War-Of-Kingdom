/* $Id: ai.hpp 37696 2009-08-12 11:19:17Z crab $ */
/*
   Copyright (C) 2003 - 2009 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/** @file ai/default/ai.hpp */

#ifndef AI_DEFAULT_AI_HPP_INCLUDED
#define AI_DEFAULT_AI_HPP_INCLUDED

#include "../../global.hpp"

#include "attack.hpp"

#include "../interface.hpp"

#ifdef _MSC_VER
#pragma warning(push)
//silence "inherits via dominance" warnings
#pragma warning(disable:4250)
#endif

namespace ai {

class ai_default : public ai_interface, public events::observer 
{
public:
	static std::map<std::pair<const unit*, const unit_type*>, battle_context*>& unit_stats_cache();
	static void clear_stats_cache();

	typedef map_location location;//will get rid of this later

	ai_default(int side, const config &cfg);
	~ai_default();

	void play_turn();
	void new_turn();
	std::string describe_self();
	config to_config() const;


	/** Handle generic event */
	void handle_generic_event(const std::string& event_name);

	ai_plan& plan(bool action);

	const game_info& get_info() const;

private:
	void do_move();
	void do_move_td();

	bool do_combat(bool unmovementable = false);

	bool do_recruitment(artifical& city);

	/**
	 * Our own version of 'move_unit'. It is like the version in readwrite_context
	 * however if it is the leader moving, it will first attempt recruitment.
	 */
	map_location move_unit(const std::pair<unit*, int>& pair, location to, bool dst_must_reachable = true);

	// attacks
	void analyze_targets(std::vector<attack_analysis>& res, bool unmovementable);

	void do_attack_analysis(
	                unit* target_ptr,
					const std::vector<std::pair<unit*, int> >& units2,
	                const std::multimap<int, map_location>& srcdst2, const std::multimap<map_location, int>& dstsrc2,
					std::vector<std::pair<unit*, int> >& units,
	                std::vector<attack_analysis>& result,
					attack_analysis& cur_analysis
	                );

	int rate_terrain(const unit& u, const map_location& loc) const;

	void calculate_possible_moves2(std::vector<std::pair<unit*, int> > src,
		std::map<int, pathfind::paths>& res, std::multimap<int, map_location>& srcdst,
		std::multimap<map_location, int>& dstsrc, bool enemy, bool assume_full_movement = false,
		const terrain_filter* remove_destinations = NULL,
		bool see_all = false) const;

	void calculate_possible_moves2(std::pair<unit*, int>& troop,
		pathfind::paths& res, std::vector<map_location>& srcdst,
		bool enemy, bool assume_full_movement = false,
		const terrain_filter* remove_destinations = NULL, bool see_all = false) const;

	bool can_allied(const team& to, int target_side, artifical* aggressed);
	void do_diplomatism(int index);
	bool do_tactic(int index, bool first);
	// void calculate_mr_rects_from_city_rect();
	void satisfy_hero_requirement(int index);
	void calculate_mr_target(int index);
	
	void move_fresh_hero(artifical& from, artifical& to, int index);
	void move_hero(artifical& from, artifical& to, int& lack, int& more);
	int build(artifical& owner, std::vector<std::pair<unit*, int> >& builder_troops, const unit_type* art, const map_location& at);

	const terrain_filter* get_avoid() const;

private:
	static std::map<std::pair<const unit*, const unit_type*>, battle_context*> unit_stats_cache_;

	game_display& disp_;
	gamemap& map_;
	unit_map& units_;
	hero_map& heros_;
	game_state& gamestate_;
	std::vector<team>& teams_;
	hero& leader_;
	tod_manager& tod_manager_;
	play_controller& controller_;
	bool consider_combat_;
	int side_;
	team& current_team_;

	friend class attack_analysis;

	const config cfg_;
	hero* rpg_hero_;
	int cost_exponent_;

	ai_plan plan_to_;

	// [ai] setting variable
	bool td_mode_;
	double aggression_;
	int attack_depth_;

	std::map<unit*, std::pair<map_location, unit*>* > reside_cache_;
	int* side_cache_int_;
	size_t side_cache_size_;

	std::set<map_location> capturing_villages_;
};

} //end of namespace ai

#ifdef _MSC_VER
#pragma warning(pop)
#endif


#endif
