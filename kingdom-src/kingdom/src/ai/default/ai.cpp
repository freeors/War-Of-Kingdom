/* $Id: ai.cpp 37764 2009-08-15 08:09:08Z ilor $ */
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

/**
 * @file ai/default/ai.cpp
 * Artificial intelligence - The computer commands the enemy.
 */

#include "ai.hpp"

#include "../manager.hpp"

#include "../../array.hpp"
#include "../../foreach.hpp"
#include "../../game_end_exceptions.hpp"
#include "../../game_events.hpp"
#include "../../game_preferences.hpp"
#include "../../log.hpp"
#include "../../mouse_handler_base.hpp"
#include "../../replay.hpp"
#include "../../statistics.hpp"
#include "../../unit_display.hpp"
#include "../../wml_exception.hpp"
#include "resources.hpp"
#include "play_controller.hpp"
#include "callable_objects.hpp"
#include "artifical.hpp"
#include "terrain_filter.hpp"
// #include "wml_exception.hpp"
#include "gettext.hpp"
#include "filesystem.hpp"

#include <fstream>

#ifdef _MSC_VER
#pragma warning(push)
//silence "inherits via dominance" warnings
#pragma warning(disable:4250)
#endif

extern uint32_t total_analyzing;
extern uint32_t total_recruit;
extern uint32_t total_combat;
extern uint32_t total_build;
extern uint32_t total_move;
extern uint32_t total_a_star_search;
extern int total_a_star_searches;
extern uint32_t total_diplomatism;

namespace ai {

typedef util::array<map_location,6> adjacent_tiles_array;

#ifdef _MSC_VER
#pragma warning(pop)
#endif

std::map<std::pair<const unit*, const unit_type*>, battle_context*> ai_default::unit_stats_cache_;

ai_default::ai_default(int side, const config &cfg)
	: side_(side)
	, cfg_(cfg)
	, disp_(get_info().disp)
	, map_(get_info().map)
	, units_(get_info().units)
	, heros_(get_info().heros_)
	, gamestate_(get_info().game_state_)
	, teams_(get_info().teams)
	, current_team_(get_info().teams[side - 1])
	, leader_(*get_info().teams[side - 1].leader())
	, tod_manager_(get_info().tod_manager_)
	, controller_(*resources::controller)
	, rpg_hero_(rpg::h)
	, cost_exponent_(100)
	, consider_combat_(true)
	, plan_to_()
	, td_mode_(false)
	, aggression_(0.5)
	, attack_depth_(5)
	, side_cache_int_(NULL)
	, side_cache_size_(0)
	, capturing_villages_()
{
	manager::add_gamestate_observer(this);

	// read attribute in [ai]
	td_mode_ = cfg["td_mode"].to_bool();
	aggression_ = cfg["aggression"].to_double(0.5);
	attack_depth_ = cfg["attack_depth"].to_int(5);
}

ai_default::~ai_default()
{
	manager::remove_gamestate_observer(this);
	if (side_cache_int_) {
		free(side_cache_int_);
		side_cache_int_ = NULL;
		side_cache_size_ = 0;
	}
}

void ai_default::new_turn()
{
}

std::string ai_default::describe_self(){
	return "[default_ai]";
}

config ai_default::to_config() const
{
	config cfg;
	cfg["td_mode"] = td_mode_;
	cfg["aggression"] = aggression_;
	cfg["attack_depth"] = attack_depth_;
	
	return cfg;
}

std::map<std::pair<const unit*, const unit_type*>, battle_context*>& ai_default::unit_stats_cache()
{
	return unit_stats_cache_;
}

void ai_default::handle_generic_event(const std::string& /*event_name*/)
{
}

const game_info& ai_default::get_info() const
{
	return manager::get_active_ai_info_for_side(side_);
}

map_location ai_default::move_unit(const std::pair<unit*, int>& pair, map_location to, bool dst_must_reachable)
{
	int side = side_;
	unit* unit_ptr = NULL;
	if (pair.second >= 0) {
		unit_ptr = unit_2_artifical(pair.first)->reside_troops()[pair.second];
	} else {
		unit_ptr = pair.first;
	}
	map_location from = unit_ptr->get_location();
	map_location unit_location_ = from;

	if (from != to) {
		// it shouldn't use move_spectator of unit_ptr! when expediate, un will became invalid.
		move_unit_spectator move_spectator(units_);
		// do_init_for_execution
		// move_spectator.reset(units_);
		// check_before
		//do an A*-search
		const pathfind::shortest_path_calculator calc(*unit_ptr, current_team_, units_, teams_, map_, false, true);
		// double stop_at = dst_must_reachable? 10000.0: calc.getUnitHoldValue() * 3 + 10000.0;
		double stop_at = dst_must_reachable? 10000.0: calc.getUnitHoldValue() + 10000.0;
		//allowed teleports
		std::set<map_location> allowed_teleports;
		uint32_t start = SDL_GetTicks();
		pathfind::plain_route route_ = a_star_search(from, to, stop_at, &calc, map_.w(), map_.h(), &allowed_teleports);
		total_a_star_searches ++;
		total_a_star_search += SDL_GetTicks() - start;

		if (route_.steps.empty()) {
			return map_location();
		}
		// modify! last location must no unit on it.
		while (!dst_must_reachable) {
			to = route_.steps.back();
			if (!units_.count(to)) {
				break;
			} else if (artifical* c = units_.city_from_loc(to)) {
				if (c->side() == unit_ptr->side()) { 
					// same side's city. 
					break;
				}
			}
			if (to == from) {
				// A(city)(city), A will enter it
				return map_location();
			}
			route_.steps.pop_back();
		}
		
		// do_execute
		move_spectator.set_unit(units_.find(from));

		bool gamestate_changed = false;

		// if (units_.count(to) == 0 || from == to) {
		if (!route_.steps.empty()) {
			if (pair.second >= 0) {
				artifical* expediting_city = unit_2_artifical(pair.first);
				// std::vector<unit*>& reside_troops = expediting_city->reside_troops();
				disp_.place_expedite_city(*expediting_city);
				units_.set_expediting(expediting_city, pair.second);
				::move_unit(
					/*move_unit_spectator* move_spectator*/ &move_spectator,
					/*std::vector<map_location> route*/ route_.steps,
					/*replay* move_recorder*/ &recorder,
					/*undo_list* undo_stack*/ NULL,
					/*bool show_move*/ true,
					/*map_location *next_unit*/ NULL,
					/*bool continue_move*/ true, //@todo: 1.7 set to false after implemeting interrupt awareness
					/*bool should_clear_shroud*/ true,
					/*bool is_replay*/ false);
				disp_.remove_expedite_city();
				units_.set_expediting();
				if (move_spectator.get_unit().valid() && move_spectator.get_unit()->get_location() != from) {
					// move_unit may be not move! reside troop ratain in city.
					expediting_city->troop_go_out(units_.last_expedite_index());
				} else {
					unit_ptr->remove_movement_ai();
				}

			} else {
				::move_unit(
					/*move_unit_spectator* move_spectator*/ &move_spectator,
					/*std::vector<map_location> route*/ route_.steps,
					/*replay* move_recorder*/ &recorder,
					/*undo_list* undo_stack*/ NULL,
					/*bool show_move*/ true,
					/*map_location *next_unit*/ NULL,
					/*bool continue_move*/ true, //@todo: 1.7 set to false after implemeting interrupt awareness
					/*bool should_clear_shroud*/ true,
					/*bool is_replay*/ false);
			}
			// once unit moved one or more grid, unit_ptr may invalid. below code must not use un!

			if (move_spectator.get_ambusher().valid() || !move_spectator.get_seen_enemies().empty() || !move_spectator.get_seen_friends().empty() ) {
				unit_location_ = move_spectator.get_unit()->get_location();
				gamestate_changed = true;
			} else if (move_spectator.get_unit().valid()){
				unit_location_ = move_spectator.get_unit()->get_location();
				if (unit_location_ != from) {
					gamestate_changed = true;
				}
			}
		}

		if (unit_location_ != from) {
			unit_map::iterator un = units_.find(unit_location_);
			if (unit_location_ != from && un->movement_left() > 0) {
				bool remove_movement_ = true;
				bool remove_attacks_ = false;
				if (remove_movement_){
					un->remove_movement_ai();
					gamestate_changed = true;
				}
				if (remove_attacks_){
					un->remove_attacks_ai();
					gamestate_changed = true;
				}
			}
		} else {
			unit_location_ = map_location();
		}

		try {
			if (gamestate_changed) {
				manager::raise_gamestate_changed();
			}
			resources::controller->check_victory();
			resources::controller->check_end_level();
		} catch (...) {
			throw;
		}
		// check_after
		if (unit_location_ != from && unit_location_ != to) {
/*
			// We've been ambushed; find the ambushing unit and attack them.
			adjacent_tiles_array locs;
			get_adjacent_tiles(unit_location_, locs.data());
			for(adjacent_tiles_array::const_iterator adj_i = locs.begin(); adj_i != locs.end(); ++adj_i) {
				const unit_map::const_iterator itor = units_.find(*adj_i);
				if(itor != units_.end() && current_team_.is_enemy(itor->second.side()) &&
				   !itor->second.incapacitated()) {
					// here, only melee weapon is valid, need modify!!
					battle_context bc(units_, unit_location_, *adj_i, -1, -1, aggression_);
					attack_enemy(unit_location_,itor->first,bc.get_attacker_stats().attack_num,bc.get_defender_stats().attack_num);
					break;
				}
			}
*/
		}

		return unit_location_;
	} else {
		return from;
	}
}

void ai_default::play_turn()
{
	// Protect against a memory over commitment:
	/**
	 * @todo 2.0 Not in the mood to figure out the exact cause:
	 * For some reason -1 hitpoints cause a segmentation fault.
	 * If -1 hitpoints are sent, we crash :/
	 */
	try {
		if (teams_.size() > side_cache_size_) {
			if (side_cache_int_) {
				free(side_cache_int_);
			}
			side_cache_size_ = teams_.size();
			side_cache_int_ = (int*)malloc(sizeof(int) * side_cache_size_);
		}

		consider_combat_ = true;
		cost_exponent_ = current_team_.cost_exponent();
		game_events::fire("ai turn");
		if (td_mode_) {
			do_move_td();
		} else {
			do_move();
		}
	} catch(std::bad_alloc) {
		lg::wml_error << "Memory exhausted - a unit has either a lot of hitpoints or a negative amount.\n";
	}

}

const terrain_filter* ai_default::get_avoid() const
{
	team& curr_team = teams_[side_ - 1];

	if (curr_team.has_avoid()) {
		return &curr_team.avoid();
	}
	return NULL;
}

void ai_default::calculate_possible_moves2(std::vector<std::pair<unit*, int> > troops,
		std::map<int, pathfind::paths>& res, std::multimap<int, map_location>& srcdst,
		std::multimap<map_location, int>& dstsrc, bool enemy, bool assume_full_movement,
		const terrain_filter* remove_destinations, bool see_all) const
{
	int index = 0;
	for (std::vector<std::pair<unit*, int> >::const_iterator un_it = troops.begin(); un_it != troops.end(); ++un_it, index ++) {
		// If we are looking for the movement of enemies, then this unit must be an enemy unit.
		// If we are looking for movement of our own units, it must be on our side.
		// If we are assuming full movement, then it may be a unit on our side, or allied.
		unit* unit_ptr = un_it->first;
		const map_location& un_loc = unit_ptr->get_location();

		// If it's an enemy unit, reset its moves while we do the calculations.
		unit* held_unit = unit_ptr;
		const unit_movement_resetter move_resetter(*held_unit, enemy || assume_full_movement);

		// Insert the trivial moves of staying on the same map location.
		// for reside troop, it cann't reach original location(avoid ataack in city)
		if (un_it->second < 0 && unit_ptr->movement_left() > 0 ) {
			srcdst.insert(std::make_pair<int, map_location>(index, un_loc));
			dstsrc.insert(std::make_pair<map_location, int>(un_loc, index));
		}
		res.insert(std::pair<int, pathfind::paths>(
					index, pathfind::paths(map_, units_, *unit_ptr, un_loc, teams_, false, false, current_team_, 0, see_all)));
	}

	for (std::map<int, pathfind::paths>::iterator m = res.begin(); m != res.end(); ++m) {
		foreach (const pathfind::paths::step &dest, m->second.destinations) {
			const map_location& src = troops[m->first].first->get_location();
			const map_location& dst = dest.curr;

			if (troops[m->first].second >= 0 && dst == src) {
				continue;
			}

			if (remove_destinations != NULL && remove_destinations->match(dst)) {
				continue;
			}

			bool friend_owns = false;

			// Don't take friendly villages
			if (!enemy && map_.is_village(dst)) {
				for (size_t n = 0; n != teams_.size(); ++n) {
					if (teams_[n].owns_village(dst)) {
						int side = n + 1;
						if (side_ != side && !current_team_.is_enemy(side)) {
							friend_owns = true;
						}

						break;
					}
				}
			}

			if (friend_owns) {
				continue;
			}

			if (src != dst && units_.find(dst) == units_.end()) {
				srcdst.insert(std::pair<int, map_location>(m->first, dst));
				dstsrc.insert(std::pair<map_location, int>(dst, m->first));
			}
		}
	}
}

void ai_default::calculate_possible_moves2(std::pair<unit*, int>& troop,
		pathfind::paths& res, std::vector<map_location>& srcdst,
		bool enemy, bool assume_full_movement,
		const terrain_filter* remove_destinations, bool see_all) const
{
		// If we are looking for the movement of enemies, then this unit must be an enemy unit.
		// If we are looking for movement of our own units, it must be on our side.
		// If we are assuming full movement, then it may be a unit on our side, or allied.
		unit* unit_ptr = unit_ptr = troop.first;;
		const map_location& un_loc = unit_ptr->get_location();

		// If it's an enemy unit, reset its moves while we do the calculations.
		unit* held_unit = unit_ptr;
		const unit_movement_resetter move_resetter(*held_unit, enemy || assume_full_movement);

		// Insert the trivial moves of staying on the same map location.
		// for reside troop, it cann't reach original location(avoid ataack in city)
		if (troop.second < 0 && unit_ptr->movement_left() > 0) {
			srcdst.push_back(un_loc);
		}
		res = pathfind::paths(map_, units_, *unit_ptr, un_loc, teams_, false, false, current_team_, 0, see_all);


	// size of res must is 1. there is use std::map insteal std::pair, in order to compatible withing tow calculate_possible_moves2
		foreach (const pathfind::paths::step &dest, res.destinations) {
			const map_location& src = troop.first->get_location();
			const map_location& dst = dest.curr;

			if (troop.second >= 0 && dst == src) {
				continue;
			}

			if (remove_destinations != NULL && remove_destinations->match(dst)) {
				continue;
			}

			bool friend_owns = false;

			// Don't take friendly villages
			if (!enemy && map_.is_village(dst)) {
				for (size_t n = 0; n != teams_.size(); ++n) {
					if (teams_[n].owns_village(dst)) {
						int side = n + 1;
						if (side_ != side && !current_team_.is_enemy(side)) {
							friend_owns = true;
						}

						break;
					}
				}
			}

			if (friend_owns) {
				continue;
			}

			if (src != dst && units_.find(dst) == units_.end()) {
				srcdst.push_back(dst);
			}
		}
}

ai_plan& ai_default::plan(bool action)
{
	int side = side_;
	team& curr_team = teams_[side - 1];

	plan_to_.reset();
	units_.calculate_mrs_data(plan_to_.mrs_, side, action);
	return plan_to_;
}

void ai_default::satisfy_hero_requirement(int index)
{
	mr_data& mr = plan_to_.mrs_[index];

	if (mr.target == mr_data::TARGET_CHAOTIC || mr.own_cities.size() <= 1) {
		return;
	}

	artifical* aggressing = NULL;
	if (mr.target == mr_data::TARGET_AGGRESS) {
		aggressing = mr.own_front_cities[0];
	}

	std::map<artifical*, int> input;
	std::vector<std::pair<artifical*, int> > output;
	int fronts;
	for (std::vector<artifical*>::iterator c_itor = mr.own_back_cities.begin(); c_itor != mr.own_back_cities.end(); ++ c_itor) {
		artifical* city = *c_itor;
		int office_heros = city->office_heros();
		fronts = city->fronts();

		if (fronts > 0) {
			// this city maybe aggressed still. keep maximum heros!
			if (office_heros < mr_data::min_front_requirement) {
				if (city->mayor() != rpg::h) {
					input[city] = office_heros - mr_data::min_front_requirement;
				}
			} else if (fronts < mr_data::max_fronts - 4 && office_heros > mr_data::max_front_requirement) {
				output.push_back(std::make_pair<artifical*, int>(city, office_heros - mr_data::max_front_requirement));
			}
		} else {
			if (office_heros < mr_data::min_interior_requirement) {
				if (city->mayor() != rpg::h) {
					input[city] = office_heros - mr_data::min_interior_requirement;
				}
			} else if (office_heros > mr_data::max_interior_requirement) {
				output.push_back(std::make_pair<artifical*, int>(city, office_heros - mr_data::max_interior_requirement));
			}
		}
	}
	for (std::vector<artifical*>::reverse_iterator city_ritor = mr.own_front_cities.rbegin(); city_ritor != mr.own_front_cities.rend(); ++ city_ritor) {
		artifical* city = *city_ritor;
		int office_heros = city->office_heros();
		fronts = city->fronts();

		if (city == aggressing) {
			if (office_heros > mr_data::aggress_requirement) {
				// let aggressing inching
				output.push_back(std::make_pair<artifical*, int>(city, 2));
			}
		} else if (fronts >= mr_data::min_alert_fronts) {
			int min_requirement = (fronts == mr_data::max_fronts)? (mr_data::max_front_requirement - 1): mr_data::mid_front_requirement;
			if (office_heros < min_requirement) {
				if (city->mayor() != rpg::h) {
					input[city] = office_heros - min_requirement;
				}
			} else if (fronts < mr_data::max_fronts - 4 && office_heros > mr_data::max_front_requirement) {
				output.push_back(std::make_pair<artifical*, int>(city, office_heros - mr_data::max_front_requirement));
			}
		} else {
			if (office_heros < mr_data::min_front_requirement) {
				if (city->mayor() != rpg::h) {
					input[city] = office_heros - mr_data::min_front_requirement;
				}
			} else if (office_heros > mr_data::mid_front_requirement) {
				output.push_back(std::make_pair<artifical*, int>(city, office_heros - mr_data::mid_front_requirement));
			}
		}
	}
	if (input.empty() || output.empty()) {
		return;
	}
	for (std::map<artifical*, int>::iterator c_itor = input.begin(); c_itor != input.end(); ++ c_itor) {
		artifical* to = c_itor->first;
		int lack = c_itor->second;
		
		for (std::vector<std::pair<artifical*, int> >::iterator o_itor = output.begin(); o_itor != output.end();) {
			artifical* from = o_itor->first;
			int more = o_itor->second;
			move_hero(*from, *to, lack, more);
			if (more <= 0) {
				o_itor = output.erase(o_itor);
			} else {
				o_itor->second = more;
				++ o_itor;
			}
			if (lack >= 0) {
				break;
			}
		}
		if (output.empty()) {
			break;
		}
	}
	return;
}

void ai_default::calculate_mr_target(int index)
{
	mr_data& mr = plan_to_.mrs_[index];

	capturing_villages_.clear();

	if (mr.target == mr_data::TARGET_INTERIOR) {
		// all field are back city, reside are in city.
		for (std::map<int, mr_data::enemy_data>::const_iterator i = mr.own_cities.begin(); i != mr.own_cities.end(); ++ i) {
			artifical& city = *units_.city_from_cityno(i->first);
			std::vector<unit*>& resides = city.reside_troops();
			std::vector<hero*>& hero_list = city.fresh_heros();
			size_t min_requirement = true? 0: mr_data::min_interior_requirement - 1;
			while (hero_list.size() > min_requirement) {
				// recruit! until balance in this military-region
				if (!do_recruitment(city)) {
					// cannot recurite more, my be not enough gold
					break;
				}
				// unit& new_troop = resides.back();
				// new_troop.set_goto(city.get_location()); // target to centor of this military region
			}
			for (std::vector<unit*>::iterator u_itor = resides.begin(); u_itor != resides.end(); ++ u_itor) {
				unit& u = **u_itor;
				if (!u.human()) {
					u.set_goto(map_location());
				}
			}
			std::vector<unit*>& fields = city.field_troops();
			for (std::vector<unit*>::iterator u_itor = fields.begin(); u_itor != fields.end(); ++ u_itor) {
				unit* troop = *u_itor;
				if (!troop->human()) {
					const map_location& goto_loc = troop->get_goto();
					if (goto_loc.valid() && map_.is_village(goto_loc) && !current_team_.owns_village(goto_loc)) {
						capturing_villages_.insert(goto_loc);
					} else {
						troop->set_goto(city.get_location());
					}
				}
			}
		}		

	} else if (mr.target == mr_data::TARGET_CHAOTIC) {
		std::pair<unit**, size_t> fields = current_team_.field_troop();
		unit** troops = fields.first;
		size_t troops_vsize = fields.second;
		for (size_t i = 0; i < troops_vsize; i ++) {
			if (!troops[i]->is_artifical()) {
				// field troops
				artifical* city = mr.enemy_cities[rand() % mr.enemy_cities.size()];
				if (!troops[i]->human()) {
					troops[i]->set_goto(city->get_location());
				}
			}
		}

	} else if (mr.target == mr_data::TARGET_AGGRESS) {
		// prepare aggressing hero
		artifical* aggressing = mr.own_front_cities[0];
		artifical* aggressed = mr.enemy_cities[0];

		// To non-aggressing cities, all field are back city, reside are in city.
		// following dispatch may change field's ownership, reside's goto.
		for (std::map<int, mr_data::enemy_data>::const_iterator i = mr.own_cities.begin(); i != mr.own_cities.end(); ++ i) {
			if (i->first == aggressing->cityno()) {
				continue;
			}
			artifical& city = *units_.city_from_cityno(i->first);
			
			std::vector<unit*>& resides = city.reside_troops();
			std::vector<hero*>& hero_list = city.fresh_heros();
			size_t min_requirement = true? 0: mr_data::min_interior_requirement - 1;
			while (hero_list.size() > min_requirement) {
				// recruit! until balance in this military-region
				if (!do_recruitment(city)) {
					// cannot recurite more, my be not enough gold
					break;
				}
				// unit& new_troop = resides.back();
				// new_troop.set_goto(city.get_location()); // target to centor of this military region
			}
			for (std::vector<unit*>::iterator u_itor = resides.begin(); u_itor != resides.end(); ++ u_itor) {
				unit& u = **u_itor;
				if (!u.human()) {
					u.set_goto(map_location());
				}
			}
			std::vector<unit*>& fields = city.field_troops();
			for (std::vector<unit*>::iterator u_itor = fields.begin(); u_itor != fields.end(); ++ u_itor) {
				unit* troop = *u_itor;
				if (!troop->human()) {
					const map_location& goto_loc = troop->get_goto();
					if (goto_loc.valid() && map_.is_village(goto_loc) && !current_team_.owns_village(goto_loc)) {
						capturing_villages_.insert(goto_loc);
					} else {
						troop->set_goto(city.get_location());
					}
				}
			}
		}

		// recruit all fresh heros
		std::vector<unit*>& reside_troops = aggressing->reside_troops();
		for (std::vector<unit*>::iterator i = reside_troops.begin(); i != reside_troops.end(); ++ i) {
			unit& u = **i;
			if (!u.human()) {
				u.set_goto(aggressed->get_location());
			}
		}
		std::vector<hero*>& hero_list = aggressing->fresh_heros();
		while (!hero_list.empty()) {
			// recruit! until balance in this military-region
			if (!do_recruitment(*aggressing)) {
				// cannot recurite more, my be not enough gold
				break;
			}
			unit* new_troop = reside_troops.back();
			if (!new_troop->human()) {
				new_troop->set_goto(aggressed->get_location()); // target to centor of this military region
			}
		}
		
		int aggressing_heros = aggressing->office_heros();
		aggressing_heros -= 50;

		if (aggressing_heros < 0) {
			// need dispatch hero from other city.
			std::vector<artifical*> cache = mr.own_back_cities;
			size_t minimum_front_index = cache.size();
			for (std::vector<artifical*>::reverse_iterator city_ritor = mr.own_front_cities.rbegin(); city_ritor != mr.own_front_cities.rend(); ++ city_ritor) {
				artifical* city = *city_ritor;
				if (city != aggressing) {
					cache.push_back(city);
				}
			}

			for (size_t i = 0; i < cache.size(); i ++) {
				artifical& city = *cache[i];

				int size = city.office_heros();
				int minimum_reserve_heros = (i >= minimum_front_index)? mr_data::mid_front_requirement: mr_data::max_interior_requirement;
				if (city.fronts() >= mr_data::max_fronts - 4) {
					continue;
				} else if (city.fronts()) {
					minimum_reserve_heros = mr_data::max_front_requirement;
				}
				int more = size - minimum_reserve_heros;
				if (more <= 0) {
					continue;
				}
				move_hero(city, *aggressing, aggressing_heros, more);
				if (aggressing_heros >= 0) {
					break;
				}
			}
			// aggressed city field analyse
		}
		// set all field troops to aggressed city
		std::vector<unit*>& fieldes = aggressing->field_troops();
		for (std::vector<unit*>::iterator i = fieldes.begin(); i != fieldes.end(); ++ i) {
			unit* troop = *i;
			if (!troop->human()) {
				troop->set_goto(aggressed->get_location());
			}
		}

	} else {
		// mr_data::TARGET_GUARD;
		// all field are back city, reside are in city.
		for (std::map<int, mr_data::enemy_data>::const_iterator i = mr.own_cities.begin(); i != mr.own_cities.end(); ++ i) {
			artifical& city = *units_.city_from_cityno(i->first);
			std::vector<unit*>& resides = city.reside_troops();
			std::vector<hero*>& hero_list = city.fresh_heros();
			size_t min_requirement = true? 0: mr_data::min_interior_requirement - 1;
			while (hero_list.size() > min_requirement) {
				// recruit! until balance in this military-region
				if (!do_recruitment(city)) {
					// cannot recurite more, my be not enough gold
					break;
				}
				// unit& new_troop = resides.back();
				// new_troop.set_goto(city.get_location()); // target to centor of this military region
			}
			for (std::vector<unit*>::iterator u_itor = resides.begin(); u_itor != resides.end(); ++ u_itor) {
				unit& u = **u_itor;
				if (!u.human()) {
					u.set_goto(map_location());
				}
			}
			std::vector<unit*>& fields = city.field_troops();
			for (std::vector<unit*>::iterator u_itor = fields.begin(); u_itor != fields.end(); ++ u_itor) {
				unit* troop = *u_itor;
				if (!troop->human()) {
					const map_location& goto_loc = troop->get_goto();
					if (goto_loc.valid() && map_.is_village(goto_loc) && !current_team_.owns_village(goto_loc)) {
						capturing_villages_.insert(goto_loc);
					} else {
						troop->set_goto(city.get_location());
					}
				}
			}
		}
	}
}

bool ai_default::can_allied(const team& to, int target_side, artifical* aggressed)
{
	if (to.is_human() || to.is_empty()) {
		return false;
	}
	if (to.holded_cities().empty()) {
		return false;
	}
	if (current_team_.ally_forbided(to.side())) {
		return false;
	}
	if (!to.is_enemy(target_side)) {
		return false;
	}
	if (aggressed && to.side() == aggressed->side()) {
		return false;
	}
	return true;
}

void ai_default::do_diplomatism(int index)
{
	mr_data& mr = plan_to_.mrs_[index];
	int to_ally_side = -1;
	int target_side;
	int strategy_index = 0;
	
	if (mr.encountered_sides.size() < 2 || mr.target == mr_data::TARGET_CHAOTIC) {
		return;
	}
	VALIDATE(!mr.own_front_cities.empty(), "do_diplomatism, there is no front city on " + current_team_.name());

	const std::set<int>& encountered_sides = mr.encountered_sides;
	artifical* diplomatism_city = NULL;

	const std::vector<strategy>& strategies = current_team_.strategies();
	for (std::vector<strategy>::const_iterator it = strategies.begin(); it != strategies.end(); ++ it, strategy_index ++) {
		const strategy& s = *it;
		if (s.type_ == strategy::AGGRESS) {
			// in order to aggress, ally side to union.
			artifical* aggressed = units_.city_from_cityno(s.target_);
			const map_location& loc = aggressed->get_location();
			SDL_Rect consider_rect = extend_rectangle(map_, loc.x, loc.y, mr_data::consider_rect_radius);
			for (std::set<int>::const_iterator it = encountered_sides.begin(); it != encountered_sides.end(); ++ it) {
				int val = *it;
				if (val == aggressed->side()){
					continue;
				} 
				if (!can_allied(teams_[val - 1], aggressed->side(), aggressed)) {
					continue;
				}
				const std::vector<artifical*>& holded_cities = teams_[val - 1].holded_cities();
				for (std::vector<artifical*>::const_iterator it_city = holded_cities.begin(); it_city != holded_cities.end(); ++ it_city) {
					const map_location& other_city_loc = (*it_city)->get_location();
					if (point_in_rect(other_city_loc.x, other_city_loc.y, consider_rect)) {
						to_ally_side = val;
						target_side = aggressed->side();
						diplomatism_city = mr.own_front_cities[0];
						break;
					}
				}
			}
		} else if (s.type_ == strategy::DEFEND) {
			// in order to defender, ally side to union.
			// side: 1)must in encountered_sides, 2)has city in city's rectage, 3)isn't most side.
			// side_cache_size_ is accessorial variable, that C style instead C++ style.
			artifical* own_city = units_.city_from_cityno(s.target_);
			const map_location& loc = own_city->get_location();
			SDL_Rect consider_rect = extend_rectangle(map_, loc.x, loc.y, mr_data::consider_rect_radius);
			for (size_t i = 0; i < side_cache_size_; i ++) {
				side_cache_int_[i] = -1;
				if (encountered_sides.count(i + 1)) {
					const std::vector<artifical*>& holded_cities = teams_[i].holded_cities();
					for (std::vector<artifical*>::const_iterator it_city = holded_cities.begin(); it_city != holded_cities.end(); ++ it_city) {
						const map_location& other_city_loc = (*it_city)->get_location();
						if (point_in_rect(other_city_loc.x, other_city_loc.y, consider_rect)) {
							side_cache_int_[i] = 0;
							break;
						}
					}
				}
			}
			const std::vector<unit*>& enemy_troops = mr.own_cities[own_city->cityno()].troops;
			if (enemy_troops.empty()) {
				break;
			}
			for (std::vector<unit*>::const_iterator it_u = enemy_troops.begin(); it_u != enemy_troops.end(); ++ it_u) {
				const unit& u = **it_u;
				int s = u.side();
				side_cache_int_[s - 1] ++;
			}
			int max_index;
			int max_val = -1;
			for (size_t i = 0; i < side_cache_size_; i++) {
				if (side_cache_int_[i] > max_val) {
					max_index = i;
					max_val = side_cache_int_[i];
				}
			}
			for (size_t i = 0; i < side_cache_size_; i++) {
				if (side_cache_int_[i] < 0 || i == max_index) {
					continue;
				}
				if (can_allied(teams_[i], max_index + 1, teams_[max_index].is_human()? NULL: mr.enemy_cities[0])) {
					to_ally_side = i + 1;
					target_side = max_index + 1;
					diplomatism_city = own_city;
					break;
				}
			}
		}
		if (to_ally_side != -1) {
			break;
		}
	}
	if (to_ally_side == -1) {
		return;
	}

	// find a fresh hero to ally
	int emissary = HEROS_INVALID_NUMBER;
	std::vector<hero*>& freshes = diplomatism_city->fresh_heros();
	for (std::vector<hero*>::iterator it = freshes.begin(); it != freshes.end(); ++ it) {
		const hero* h = *it;
		if (h == rpg_hero_) {
			continue;
		}
		emissary = h->number_;
		break;
	}
	if (emissary == HEROS_INVALID_NUMBER) {
		for (std::map<int, mr_data::enemy_data>::const_iterator i = mr.own_cities.begin(); i != mr.own_cities.end(); ++ i) {
			artifical& city = *units_.city_from_cityno(i->first);
			if (&city == diplomatism_city) {
				continue;
			}
			std::vector<hero*>& freshes = city.fresh_heros();
			for (std::vector<hero*>::iterator it = freshes.begin(); it != freshes.end(); ++ it) {
				const hero* h = *it;
				if (h == rpg_hero_) {
					continue;
				}
				emissary = h->number_;
				break;
			}
		}
	}
	if (emissary == HEROS_INVALID_NUMBER) {
		return;
	}
			
	// do ally
	if (!controller_.do_ally(true, side_, to_ally_side, emissary, target_side, strategy_index)) {
		// Fail.
		current_team_.set_forbid_turns(to_ally_side, mr_data::ally_forbid_turns);
	} else {
		// Success.
		recorder.add_diplomatism("ally", side_, to_ally_side, true, emissary, target_side, strategy_index);
	}
}

bool ai_default::do_tactic(int index, bool first)
{
	int side_point = current_team_.tactic_point();
	if (current_team_.is_human() && rpg::stratum != hero_stratum_leader) {
		if (side_point < current_team_.max_tactic_point() * 3 / 4) { // 15
			return false;
		}
	} else {
		if (side_point < current_team_.max_tactic_point() / 2) { // 10
			return false;
		}
	}

	mr_data& mr = plan_to_.mrs_[index];
	mr.calculate_mass(units_, current_team_);

	int best_score = -1, score;
	hero* best_hero = NULL;
	unit* best_troop = NULL;

	for (std::vector<tmess_data>::const_iterator it = mr.messes.begin(); it != mr.messes.end(); ++ it) {
		const tmess_data& mess = *it;

		for (std::map<unit*, tmess_data::tadjacent_data>::const_iterator it2 = mess.selfs.begin(); it2 != mess.selfs.end(); ++ it2) {
			unit& u = *it2->first;
			if (u.human()) {
				continue;
			}
			if (!u.attacks_left()) {
				continue;
			}

			for (int step = 0; step < 3; step ++) {
				hero* h;
				if (step == 0) {
					h = &u.master();
				} else if (step == 1) {
					if (!u.second().valid()) {
						continue;
					}
					h = &u.second();
				} else if (step == 2) {
					if (!u.third().valid()) {
						continue;
					}
					h = &u.third();
				}
				if (h->tactic_ == HEROS_NO_TACTIC) {
					continue;
				}
				const ttactic& t = unit_types.tactic(h->tactic_);
				if (t.point() > side_point) {
					continue;
				}

				// self. if no enemies, this profit divided by 2. 
				if (!mess.enemies && !mess.enemy_arts) {
					score = 0;
				} else {
					score = t.self_profit() * u.hitpoints() / u.max_hitpoints();
					if (!u.hide_turns()) {
						score += t.self_hide_profit();
					}
					if (!mess.enemies) {
						score /= 4;
					}
				}

				// friend. if no enemies, this profit divided by 2.
				score += t.friend_profit() * it2->second.friends;
				score += t.friend_hide_profit() * it2->second.unhides;
				if (!mess.enemies) {
					score /= 4;
				}

				// enemy
				if (mess.selfs.size() > 1 || mess.allys) {
					score += t.enemy_profit() * it2->second.enemies;
				}
				
				// if total score < 50, don't conside this tactic.
				if (score < 50) {
					continue;
				}

				score += 50 * (current_team_.max_tactic_point() - t.point());
				if (score > best_score) {
					best_hero = h;
					best_troop = &u;

					best_score = score;
				}
			}
		}
	}
	if (best_troop) {
		cast_tactic(units_, *best_troop, *best_hero);
		return true;
	}
	return false;
}

void ai_default::move_fresh_hero(artifical& from, artifical& to, int index)
{
	std::vector<hero*>& src_heros = from.fresh_heros();
	
	std::set<size_t> checked_heros;
	checked_heros.insert(index);

	recorder.add_move_heros(from.get_location(), to.get_location(), checked_heros);

	hero* h = src_heros[index];
	to.move_into(*h);
	src_heros.erase(src_heros.begin() + index);

	disp_.invalidate(from.get_location());
	disp_.invalidate(to.get_location());

	game_events::fire("post_moveheros", from.get_location(), to.get_location());
}

void ai_default::move_hero(artifical& from, artifical& to, int& lack, int& more)
{
	if (lack >= 0 || more <= 0) {
		int ii = 0;
	}
	if (from.cityno() == to.cityno()) {
		int ii = 0;
	}

	hero* mayor = from.mayor();
	if (mayor != rpg::h) {
		std::vector<hero*>& fresh_heros = from.fresh_heros(); 
		size_t freshes = from.fresh_heros().size();
		int moving_hero_index = 0;
		for (size_t i = 0; i < freshes; i ++) {
			if (fresh_heros[moving_hero_index] == rpg_hero_) {
				moving_hero_index ++;
				continue;
			}
			if (fresh_heros[moving_hero_index] == mayor) {
				moving_hero_index ++;
				continue;
			}
			move_fresh_hero(from, to, moving_hero_index);
			lack ++;
			more --;
			if (lack >= 0 || more <= 0) {
				return;
			}
		}
	}

	std::vector<unit*> fields = from.field_troops();
	for (std::vector<unit*>::iterator u_itor = fields.begin(); u_itor != fields.end(); ++ u_itor) {
		unit* troop = *u_itor;
		if (troop->human()) {
			continue;
		}
		if (troop->master().number_ == mayor->number_) {
			continue;
		}
		if (troop->second().valid() && troop->second().number_ == mayor->number_) {
			continue;
		}
		if (troop->third().valid() && troop->third().number_ == mayor->number_) {
			continue;
		}
		to.unit_belong_to(troop, false, true);
		troop->set_goto(to.get_location());

		lack += 2;
		more -= 2;
		if (lack >= 0 || more <= 0) {
			return;
		}
	}

	std::vector<unit*>& resides = from.reside_troops();
	for (std::vector<unit*>::iterator u_itor = resides.begin(); u_itor != resides.end(); ++ u_itor) {
		unit& troop = **u_itor;
		if (troop.human() || troop.has_less_loyalty(game_config::ai_keep_loyalty_threshold, leader_)) {
			continue;
		}
		if (troop.master().number_ == mayor->number_) {
			continue;
		}
		if (troop.second().valid() && troop.second().number_ == mayor->number_) {
			continue;
		}
		if (troop.third().valid() && troop.third().number_ == mayor->number_) {
			continue;
		}
		troop.set_goto(to.get_location());

		lack += 2;
		more -= 2;
		if (lack >= 0 || more <= 0) {
			return;
		}
	}

	if (lack < 0 && more > 0) {
		// update more. more maybe include finish heors, reside troops.
		more = 0;
	}
}

int ai_default::build(artifical& owner, std::vector<std::pair<unit*, int> >& builder_troops, const unit_type* art, const map_location& at)
{
	std::map<int, pathfind::paths> possible_moves2;
	std::multimap<int, map_location> srcdst2;
	std::multimap<map_location, int> dstsrc2; 
	int builder_index_in_troops = -1;

	// 8. internal affairs(current build market only)
	map_location adjacent_tiles[6];
	get_adjacent_tiles(at, adjacent_tiles);

	calculate_possible_moves2(builder_troops, possible_moves2, srcdst2, dstsrc2, false, false, get_avoid());

	unit* builder = NULL;
	// int second;
	map_location builder_to_loc;
	for (size_t n = 0; (n != 6) && !builder; ++n) {
		typedef std::multimap<map_location, int>::const_iterator Itor;
		std::pair<Itor, Itor> range = dstsrc2.equal_range(adjacent_tiles[n]);
		while (range.first != range.second) {
			const map_location& dst = range.first->first;
			builder = builder_troops[range.first->second].first;
			// second = builder_troops[range.first->second].second;
			builder_to_loc = dst;
			builder_index_in_troops = range.first->second;
			break;
		}
	}
	if (!builder) {
		return -1;
	}

	std::pair<unit*, int> tmppair = builder_troops[builder_index_in_troops];
	if (tmppair.second >= 0) {
		tmppair.first = units_.city_from_loc(tmppair.first->get_location());
	}
	const map_location arrived_at = move_unit(tmppair, builder_to_loc);
	if (arrived_at != builder_to_loc) {
		// because some reason cannot reach builder_to_loc, for example ambished.
		// still return builder_index_in_troops, not -1! indicate it moved, but cann't build.
		return builder_index_in_troops;
	}
	if (builder_troops[builder_index_in_troops].second >= 0) {
		// std::vector of reside_troops is changed, unit's pointer maybe changed!
		builder = &*units_.find(builder_to_loc);
	}

	// artifical* city = units_.city_from_cityno(builder->master().city_);

	recorder.add_build(art, owner.get_location(), builder_to_loc, at, art->cost());

	std::vector<const hero*> v;
	if (art->master() != HEROS_INVALID_NUMBER) {
		v.push_back(&heros_[art->master()]);
	} else {
		v.push_back(&owner.master());
	}
	type_heros_pair pair(art, v);
	artifical new_artifical(units_, heros_, pair, owner.cityno(), true);

	current_team_.spend_gold(new_artifical.cost());

	units_.add(at, &new_artifical);

	// end action of builder
	builder->set_movement(0);
	builder->set_attacks(0);

	game_events::fire("post_build", at, builder_to_loc);

	return builder_index_in_troops;
}

void ai_default::do_move()
{
	manager::raise_user_interact();

	if (current_team_.is_human() && rpg::stratum == hero_stratum_leader) {
		while (consider_combat_) {
			consider_combat_ = do_combat();
		}
		return;
	}

	int side = side_;

	plan_to_.reset();
	std::vector<mr_data>& mrs = plan_to_.mrs_;
	units_.calculate_mrs_data(mrs, side);
	recorder.init_ai();

	int maximum_side_build_walls = 2;
	int maximum_side_build_markets = 2;

	const std::set<const unit_type*>& can_build = current_team_.builds();
	std::set<const unit_type*>::const_iterator end = can_build.end();

	if (can_build.find(unit_types.find_keep()) == end || can_build.find(unit_types.find_wall()) == end) {
		maximum_side_build_walls = 0;
	}
	bool can_build_market = can_build.find(unit_types.find_market()) != end;
	bool can_build_technology = can_build.find(unit_types.find_technology()) != end;
	if (!can_build_market && !can_build_technology) {
		maximum_side_build_markets = 0;
	}

	int size = mrs.size();
	for (int index = 0; index < size; index ++) {
		mr_data& mr = mrs[index];
		if (gamestate_.classification().rpg_mode) {
			do_diplomatism(index);
		}
		satisfy_hero_requirement(index);
		calculate_mr_target(index);

		bool ret = true;
		while (ret) {
			ret = do_tactic(index, true);
		}

		// attack! own field/reside troops and cities vs enemy field/reside troops and cities
		while (consider_combat_) {
			consider_combat_ = do_combat();
		}

		if (mr.target == mr_data::TARGET_AGGRESS && mr.enemy_cities[0]->side() == side) {
			// capture aggressing city.
			// don't do action that maybe result into no-async with replay, for example mr.own_front_cities[0]->set_fronts(0).
			units_.ai_capture_aggressed(*mr.enemy_cities[0], side_, true);
		}

		// build artifical
		for (std::map<int, mr_data::enemy_data>::iterator itor = mr.own_cities.begin(); itor != mr.own_cities.end(); ++ itor) {
			artifical& owner = *units_.city_from_cityno(itor->first);
			if (owner.side() != side_) {
				continue;
			}
			if (!maximum_side_build_walls && !maximum_side_build_markets) {
				continue;
			}

			int maximum_build_walls = maximum_side_build_walls;
			int maximum_build_markets = maximum_side_build_markets;
			std::vector<std::pair<unit*, int> > builder_troops;
			int minimum_field_index;

			do {
				if (builder_troops.empty()) {
					int index;
					std::vector<unit*>& reside_troops = owner.reside_troops();
					index = reside_troops.size() - 1;
					for (std::vector<unit*>::reverse_iterator u_ritor = reside_troops.rbegin(); u_ritor != reside_troops.rend(); ++ u_ritor, index --) {
						unit& u = **u_ritor;
						if (!u.human() && u.movement_left() && u.attacks_left() && !u.has_less_loyalty(game_config::ai_keep_loyalty_threshold, leader_)) {
							builder_troops.push_back(std::make_pair<unit*, int>(&u, index));
						}
					}
					minimum_field_index = builder_troops.size();

					std::vector<unit*>& field_troops = owner.field_troops();
					for (std::vector<unit*>::iterator u_itor = field_troops.begin(); u_itor != field_troops.end(); ++ u_itor) {
						unit* troop = *u_itor;
						if (!troop->human() && troop->attacks_left()) {
							builder_troops.push_back(std::make_pair<unit*, int>(troop, -1));
						}
					}
					if (builder_troops.empty()) {
						break;
					}
				}

				const unit_type* art = NULL;
				const map_location* at = NULL;
				int builder_index_in_troops;

				// wall
				std::vector<map_location*> keeps;
				std::vector<map_location*> wall_vacants;
				std::vector<map_location*> keep_vacants;

				while (maximum_build_walls) {
					if (current_team_.gold() < (unit_types.find_wall()->cost() + unit_types.find_keep()->cost()) * 2) {
						maximum_build_walls = 0;
						break;
					}
					calculate_wall_tiles(units_, map_, owner, keeps, wall_vacants, keep_vacants);
					if (wall_vacants.empty() && keep_vacants.empty()) {
						maximum_build_walls = 0;
						break;
					}
					art = unit_types.find_keep();
					if (keeps.size() < 2 && !keep_vacants.empty()) {
						// attemp to build keep
						if (keeps.empty()) {
							at = keep_vacants[rand() % keep_vacants.size()];
						} else {
							// side has a keep, build second. this keep isn't adjacent to first.
							std::vector<const map_location*> new_keep_vacants;
							for (std::vector<map_location*>::const_iterator loc_itor = keep_vacants.begin(); loc_itor != keep_vacants.end(); ++ loc_itor) {
								const map_location& loc = **loc_itor;
								if (!tiles_adjacent(*keeps[0], loc)) {
									new_keep_vacants.push_back(&loc);
								}
							}
							if (new_keep_vacants.empty()) {
								at = keep_vacants[rand() % keep_vacants.size()];
							} else {
								at = new_keep_vacants[rand() % new_keep_vacants.size()];
							}
						}
					}
					if (!at && !wall_vacants.empty()) {
						// attemp to build wall
						art = unit_types.find_wall();
						at = wall_vacants[rand() % wall_vacants.size()];
					}
					if (at) {
						int builder_index_in_troops = build(owner, builder_troops, art, *at);
						maximum_build_walls --;
						if (builder_index_in_troops != -1) {
							if (builder_index_in_troops >= minimum_field_index) {
								builder_troops.erase(builder_troops.begin() + builder_index_in_troops);
							} else {
								builder_troops.clear();
							}
						}
					} else {
						maximum_build_walls = 0;
					}
					break;
				}
				if (builder_troops.empty()) {
					continue;
				}

				// market
				std::vector<const map_location*> ea_vacants;

				at = NULL;
				while (maximum_build_markets) {
					if (current_team_.gold() < std::max(unit_types.find_market()->cost(), unit_types.find_technology()->cost())) {
						maximum_build_markets = 0;
						break;
					}
					int markets, technologies;
					owner.calculate_ea_tiles(ea_vacants, markets, technologies);
					if (ea_vacants.empty()) {
						maximum_build_markets = 0;
						break;
					}
					if (can_build_market && markets == 0) {
						art = unit_types.find_market();
					} else if (can_build_technology && technologies == 0) {
						art = unit_types.find_technology();
					} else {
						// rand in market or technology
						std::vector<const unit_type*> candidate;

						if (can_build_market) {
							candidate.push_back(unit_types.find_market());
						}
						if (can_build_technology) {
							candidate.push_back(unit_types.find_technology());
						}

						art = candidate[rand() % candidate.size()];
					}
					at = ea_vacants[rand() % ea_vacants.size()];
					builder_index_in_troops = build(owner, builder_troops, art, *at);
					if (builder_index_in_troops != -1) {
						maximum_build_markets --;
						if (builder_index_in_troops >= minimum_field_index) {
							builder_troops.erase(builder_troops.begin() + builder_index_in_troops);
						} else {
							builder_troops.clear();
						}
					} else {
						maximum_build_markets = 0;
					}
					break;
				}
			} while (maximum_build_walls || maximum_build_markets);
		}

		// 9. all unmoved field/reside troops execute move. (according to goto)
		std::vector<std::pair<unit*, int> > gotos;
		map_location goto_loc;
		// calcuate best attackable city. removeable unit will move to it.

		if (!mr.own_cities.empty()) {
			for (std::map<int, mr_data::enemy_data>::iterator itor = mr.own_cities.begin(); itor != mr.own_cities.end(); ++ itor) {
				artifical* city = units_.city_from_cityno(itor->first);
				if (city->side() != side_) {
					continue;
				}

				std::vector<unit*>& reside_troops = city->reside_troops();
				// capture village
				if (!reside_troops.empty()) {
					const std::set<map_location>& villages = city->villages();
					for (std::set<map_location>::const_iterator vill = villages.begin(); vill != villages.end(); ++ vill) {
						if (capturing_villages_.count(*vill) || current_team_.owns_village(*vill)) {
							continue;
						}
						for (std::vector<unit*>::iterator it_u = reside_troops.begin(); it_u != reside_troops.end(); ++ it_u) {
							unit& u = **it_u;
							if (u.human()) {
								continue;
							}
							const map_location& goto_loc = u.get_goto();
							if (map_.is_village(goto_loc)) {
								continue;
							}
							const pathfind::shortest_path_calculator calc(u, current_team_, units_, teams_, map_, false, true);
							// estimate value, three turns
							double stop_at = 100;
							// allowed teleports
							std::set<map_location> allowed_teleports;
							pathfind::plain_route route = a_star_search(city->get_location(), *vill, stop_at, &calc, map_.w(), map_.h(), &allowed_teleports);
							if (!route.steps.empty()) {
								u.set_goto(*vill);
								break;
							}
						}
					}
				}

				// goto queue.
				int index = reside_troops.size() - 1;
				for (std::vector<unit*>::reverse_iterator itor = reside_troops.rbegin(); itor != reside_troops.rend(); ++ itor, index --) {
					unit& u = **itor;
					if (u.human() || !u.movement_left() || u.has_less_loyalty(game_config::ai_keep_loyalty_threshold, leader_)) {
						continue;
					}
					gotos.push_back(std::make_pair<unit*, int>(city, index));
				}

				std::vector<unit*>& field_troops = city->field_troops();
				for (std::vector<unit*>::iterator itor = field_troops.begin(); itor != field_troops.end(); ++ itor) {
					unit* troop = *itor;
					if (troop->human() || !troop->movement_left()) {
						continue;
					}
					gotos.push_back(std::make_pair<unit*, int>(troop, -1));
				}
			}
		} else {
			std::pair<unit**, size_t> fields = current_team_.field_troop();
			unit** troops = fields.first;
			size_t troops_vsize = fields.second;
			for (size_t i = 0; i < troops_vsize; i ++) {
				if (!troops[i]->human() && !troops[i]->is_artifical() && troops[i]->movement_left()) {
					// field troops
					gotos.push_back(std::make_pair<unit*, int>(troops[i], -1));
				}
			}
		}

		total_a_star_search = 0;
		total_a_star_searches = 0;
		for (std::vector<std::pair<unit*, int> >::iterator g = gotos.begin(); g != gotos.end(); ++g) {
			// desired move troop may move into one city. after it, g->first became to access denied.
			// so update goto before move_unit.
			if (g->second >= 0) {
				goto_loc = unit_2_artifical(g->first)->reside_troops()[g->second]->get_goto();
				// unit_2_artifical(g->first)->reside_troops()[g->second]->set_goto(map_location());
			} else {
				goto_loc = g->first->get_goto();
				// g->first->set_goto(map_location());
			}
			if (map_.on_board(goto_loc)) {
				move_unit(*g, goto_loc, false);
			}
		}
/*
		ret = true;
		while (ret) {
			ret = do_tactic(index, false);
		}
*/
	}
}

void ai_default::do_move_td()
{
	manager::raise_user_interact();

	if (current_team_.is_human() && rpg::stratum == hero_stratum_leader) {
		return;
	}

	int side = side_;
	std::vector<artifical*>& holded_cities = current_team_.holded_cities();

	plan_to_.reset();
	std::vector<mr_data>& mrs = plan_to_.mrs_;
	units_.calculate_mrs_data(mrs, side);
	recorder.init_ai();

	// 2. recruits as soon as possible
	for (std::vector<mr_data>::iterator mr = mrs.begin(); mr != mrs.end(); ++ mr) {
		// recruit!
		for (std::map<int, mr_data::enemy_data>::iterator itor = mr->own_cities.begin(); itor != mr->own_cities.end(); ++ itor) {
			artifical* city = units_.city_from_cityno(itor->first);
			const map_location& city_loc = city->get_location();

			// std::vector<unit*>& reside_troops = city->reside_troops();
			std::vector<hero*>& hero_list = city->fresh_heros();
			
			while (!hero_list.empty()) {
				// recruit! until balance in this military-region
				if (!do_recruitment(*city)) {
					// cannot recurite more, my be not enough gold
					break;
				}
				// unit& new_troop = reside_troops.back();
				// new_troop.set_movement(0); // reside troop don't attack forwardly.
			}
		}
	}

	size_t loop = 1;
	do {
		// 3/6. move all field troops forward 1 step
		std::map<int, std::vector<unit*> > plan_move_troops;
		for (unit_map::iterator ui = units_.begin(); ui != units_.end(); ++ui) {
			if (ui->side() != side) {
				continue;
			}
			if (ui->is_artifical()) {
				continue;
			}
			const map_location& loc = ui->get_location();
			std::map<int, std::vector<unit*> >::iterator move_itor = plan_move_troops.find(loc.x);
			if (move_itor == plan_move_troops.end()) {
				plan_move_troops.insert(std::make_pair<int, std::vector<unit*> >(loc.x, std::vector<unit*>(1, &*ui)));
			} else {
				std::vector<unit*>& colum = move_itor->second;
				colum.push_back(&*ui);
			}
		}
		for (std::map<int, std::vector<unit*> >::reverse_iterator colum = plan_move_troops.rbegin(); colum != plan_move_troops.rend(); ++ colum) {
			for (std::vector<unit*>::iterator u = colum->second.begin(); u != colum->second.end(); ++ u) {
				const map_location& loc = (*u)->get_location();
				const map_location loc_plus_1 = map_location(loc.x + 1, loc.y);
				if (units_.count(loc_plus_1)) {
					continue;
				}
				std::pair<unit*, int> move_param = std::make_pair<unit*, int>(*u, -1);
				move_unit(move_param, loc_plus_1);
			}
		}
		// 4/7. expedite reside troops as soon as posibble
		for (std::vector<mr_data>::iterator mr = mrs.begin(); mr != mrs.end(); ++ mr) {
			for (std::map<int, mr_data::enemy_data>::iterator itor = mr->own_cities.begin(); itor != mr->own_cities.end(); ++ itor) {
				artifical* city = units_.city_from_cityno(itor->first);
				const map_location& city_loc = city->get_location();

				int x_pos = city_loc.x + 2;
				int h = map_.h();
				std::vector<map_location> empty_locs;
				for (int y = 0; y < h; y ++) {
					if (!units_.count(map_location(x_pos, y))) {
						empty_locs.push_back(map_location(x_pos, y));
					}
				}

				std::vector<unit*>& reside_troops = city->reside_troops();
				
				while (size_t troops_size = reside_troops.size()) {
					// recruit! until balance in this military-region
					size_t empty_size = empty_locs.size();
					if (!empty_size) break;
					int loc_index = rand() % empty_size;
					int troop_index = rand() % troops_size;
					
					std::pair<unit*, int> move_param = std::make_pair<unit*, int>(city, troop_index);
					move_unit(move_param, empty_locs[loc_index]);
					empty_locs.erase(empty_locs.begin() + loc_index);
				}
			}
		}

		// 5. attack! field troops (上回合留下及本回合新出征/征兵）对敌方城外部队/城市
		while (consider_combat_) {
			consider_combat_ = do_combat(true);
		}
	} while (loop --);
}

bool ai_default::do_combat(bool unmovementable)
{
	std::map<std::pair<const unit*, const unit_type*>, battle_context*>::iterator usc;
	std::map<std::pair<const unit*, const unit_type*>, battle_context*>& us = unit_stats_cache_;

	std::vector<attack_analysis> analysis;
	int start = SDL_GetTicks();
	analyze_targets(analysis, unmovementable);
	total_analyzing += SDL_GetTicks() - start;

	const int max_sims = 50000;
	int num_sims = analysis.empty() ? 0 : max_sims/analysis.size();
	if(num_sims < 20)
		num_sims = 20;
	if(num_sims > 40)
		num_sims = 40;

	const int max_positions = 30000;
	const int skip_num = analysis.size()/max_positions;

	std::vector<attack_analysis>::iterator choice_it = analysis.end();
	double choice_rating = -1000.0;
	for (std::vector<attack_analysis>::iterator it = analysis.begin(); it != analysis.end(); ++it) {

		if(skip_num > 0 && ((it - analysis.begin())%skip_num) && it->movements.size() > 1)
			continue;

		const double rating = it->rating(aggression_);

		if (rating > choice_rating) {
			choice_it = it;
			choice_rating = rating;
		}
	}

	// suokko tested the rating against current_team_.caution()
	// Bad mistake -- the AI became extremely reluctant to attack anything.
	// Documenting this in case someone has this bright idea again...*don't*...
	if (choice_rating > 0.0) {
		bool attacker_base = choice_it->movements[0].first.first->base();
		const map_location from = choice_it->movements[0].first.first->get_location();
		const map_location to   = choice_it->movements[0].second;

		// Never used:
		//		const unit_map::const_iterator tgt = units_.find(target_loc);
		std::pair<unit*, int> move_param = choice_it->movements[0].first;
		if (choice_it->movements[0].first.second >= 0) {
			artifical* city = units_.city_from_loc(from);
			move_param = std::make_pair<unit*, int>(city, choice_it->movements[0].first.second);
		}
		const map_location arrived_at = move_unit(move_param, to);
		if (arrived_at != to || units_.find(to, !attacker_base) == units_.end()) {
			// end unit's movement in this turn
			move_param.first->set_movement(0);

			std::stringstream strstr;
			strstr << "!!! " << utf8_2_ansi(move_param.first->name().c_str());
			strstr << " moving to attack has ended up unexpectedly at (";
			strstr << arrived_at.x << ", " << arrived_at.y << "), when moving to (";
			strstr << to.x << ", " << to.y << ") from (";
			strstr << from.x << ", " << from.y << ")\n";
			posix_print(strstr.str().c_str());
			return true;
		}

		// from "from" loc to "to" loc, unit type may change between packee and packer. 
		unit* to_ptr = &*units_.find(to, !attacker_base);
		unit* target_ptr = choice_it->target;
		const unit_type* to_type_ptr;
		if (!to_ptr->packed()) {
			to_type_ptr = to_ptr->type();
		} else {
			to_type_ptr = unit_types.find(to_ptr->packee_type_id());
		}

		// Recalc appropriate weapons here: AI uses approximations.
		usc = us.find(std::pair<const unit*, const unit_type*>(target_ptr, to_type_ptr));
		if (usc == us.end()) {
			throw game::game_error("cannot find item from unit_states_cache when attack_enemy");
		}

		attack_enemy(to_ptr, target_ptr, usc->second->get_attacker_stats().attack_num, usc->second->get_defender_stats().attack_num);

		total_combat += SDL_GetTicks() - start;
		return true;

	} else {

		total_combat += SDL_GetTicks() - start;
		return false;
	}
}

// return value
//   true: recruit one unit in this time.
//   false: no unit recruited in this time.
bool ai_default::do_recruitment(artifical& city)
{
	int gold = current_team_.gold();
	int max_recruit_cost = city.max_recruit_cost() * cost_exponent_ / 100;

	if (gold < 2 * max_recruit_cost) {
		return false;
	}
	if (city.mayor() == rpg::h) {
		return false;
	}

	manager::raise_user_interact();

	int side = side_;

	// expedite_heros
	std::vector<const hero*> expedite_heros;
	// field troops
	const std::pair<unit**, size_t> p = current_team_.field_troop();
	size_t maybe_total_troops = p.second;
	// citys
	const std::vector<artifical*>& holded_cities = current_team_.holded_cities();
	for (std::vector<artifical*>::const_iterator i = holded_cities.begin(); i != holded_cities.end(); ++ i) {
		artifical& city1 = **i;
		maybe_total_troops += city1.reside_troops().size() + city1.fresh_heros().size() + city1.finish_heros().size();
	}
	std::vector<hero*>& full_fresh_heros = city.fresh_heros();
	// fresh_heros doesn't independ on city.fresh_heros(), don't use reference.
	std::vector<hero*> fresh_heros;

	size_t index = 0;
	for (std::vector<hero*>::const_iterator itor = full_fresh_heros.begin(); itor != full_fresh_heros.end(); ++ itor, index ++) {
		if ((*itor)->activity_ >= game_config::minimal_activity) {
			fresh_heros.push_back(*itor);
		}
	}
	if (fresh_heros.empty()) {
		return false;
	}
	size_t max_leadership = 0;
	index = 1;
	for (std::vector<hero*>::const_iterator itor = fresh_heros.begin() + 1; itor != fresh_heros.end(); ++ itor, index ++) {
		if ((*itor)->leadership_ > fresh_heros[max_leadership]->leadership_) {
			max_leadership = index;
		}
	}

	const hero& master = *fresh_heros[max_leadership];
	size_t max_army_heros = 3;
	if (maybe_total_troops <= 6) {
		max_army_heros = 1;
	} else if ((city.field_troops().size() + city.reside_troops().size()) * 2 + city.fresh_heros().size() + city.finish_heros().size() <= 12) {
		max_army_heros = 2;
	}

	expedite_heros.push_back(fresh_heros[max_leadership]);
	fresh_heros.erase(fresh_heros.begin() + max_leadership);
	while (expedite_heros.size() < max_army_heros && !fresh_heros.empty()) {
		// select one relation hero if have
		do {
			hero* candidate_second = NULL;

			// master is candidate's consort.
			for (index = 0; index < HEROS_MAX_CONSORT; index ++) {
				if (master.consort_[index].hero_ != HEROS_INVALID_NUMBER) {
					candidate_second = &heros_[master.consort_[index].hero_];
					if (candidate_second->activity_ >= game_config::minimal_activity && candidate_second->city_ == master.city_ && candidate_second->status_ == hero_status_idle && candidate_second->number_ != master.number_) {
						std::vector<hero*>::iterator itor = std::find(fresh_heros.begin(), fresh_heros.end(), candidate_second);
						// candidate_second may be push into expedite_heros in this while, need verify.
						if (itor != fresh_heros.end()) {
							index = itor - fresh_heros.begin();
							break;
						}
					}
					candidate_second = NULL;
				}
			}
			if (candidate_second) {
				break;
			}
			// master is candidate's oath.
			for (index = 0; index < HEROS_MAX_OATH; index ++) {
				if (master.oath_[index].hero_ != HEROS_INVALID_NUMBER) {
					candidate_second = &heros_[master.oath_[index].hero_];
					if (candidate_second->activity_ >= game_config::minimal_activity && candidate_second->city_ == master.city_ && candidate_second->status_ == hero_status_idle && candidate_second->number_ != master.number_) {
						std::vector<hero*>::iterator itor = std::find(fresh_heros.begin(), fresh_heros.end(), candidate_second);
						if (itor != fresh_heros.end()) {
							index = itor - fresh_heros.begin();
							break;
						}
					}
					candidate_second = NULL;
				}
			}
			if (candidate_second) {
				break;
			}
			// candidate is master's parent.
			for (index = 0; index < HEROS_MAX_PARENT; index ++) {
				if (master.parent_[index].hero_ != HEROS_INVALID_NUMBER) {
					candidate_second = &heros_[master.parent_[index].hero_];
					if (candidate_second->activity_ >= game_config::minimal_activity && candidate_second->city_ == master.city_ && candidate_second->status_ == hero_status_idle && candidate_second->number_ != master.number_) {
						std::vector<hero*>::iterator itor = std::find(fresh_heros.begin(), fresh_heros.end(), candidate_second);
						if (itor != fresh_heros.end()) {
							index = itor - fresh_heros.begin();
							break;
						}
					}
					candidate_second = NULL;
				}
			}
			if (candidate_second) {
				break;
			}
			// master is candidate's parent.
			for (std::vector<hero*>::const_iterator itor = fresh_heros.begin(); itor != fresh_heros.end(); ++ itor) {
				candidate_second = *itor;
				for (index = 0; index < HEROS_MAX_PARENT; index ++) {
					if (candidate_second->parent_[index].hero_ == master.number_) {
						index = itor - fresh_heros.begin();
						break;
					}
				}
				if (index != HEROS_MAX_PARENT) {
					break;
				}
				candidate_second = NULL;
			}
			if (candidate_second) {
				break;
			}
			// master is candidate's intimate.
			for (index = 0; index < HEROS_MAX_INTIMATE; index ++) {
				if (master.intimate_[index].hero_ != HEROS_INVALID_NUMBER) {
					candidate_second = &heros_[master.intimate_[index].hero_];
					if (candidate_second->activity_ >= game_config::minimal_activity && candidate_second->city_ == master.city_ && candidate_second->status_ == hero_status_idle && candidate_second->number_ != master.number_) {
						std::vector<hero*>::iterator itor = std::find(fresh_heros.begin(), fresh_heros.end(), candidate_second);
						if (itor != fresh_heros.end()) {
							index = itor - fresh_heros.begin();
							break;
						}
					}
					candidate_second = NULL;
				}
			}
			if (candidate_second) {
				break;
			}
			if (fresh_heros.size() != 1) {
				index = rand() % fresh_heros.size();
			} else {
				index = 0;
			}
		} while (false);
		expedite_heros.push_back(fresh_heros[index]);
		fresh_heros.erase(fresh_heros.begin() + index);
	}
	
	if (current_team_.is_human()) {
		// play_controller& controller = *resources::controller;
		bool found_rpg = false;
		for (std::vector<const hero*>::const_iterator itor = expedite_heros.begin(); itor != expedite_heros.end(); ++ itor) {
			if (*itor == rpg_hero_) {
				found_rpg = true;
				break;
			}
		}
		if (found_rpg) {
			// display "recruit" dialog
			std::string message = _("Prepare to battle, please recruit!");
			game_events::show_hero_message(&heros_[229], NULL, message, game_events::INCIDENT_INVALID);

			controller_.do_recruit(&city, cost_exponent_, true);
			return true;
		}
	}

	std::sort(expedite_heros.begin(), expedite_heros.end(), compare_recruit);
	bool leader = (expedite_heros[0]->official_ == hero_official_leader);

	// which unit_type
	const std::string usage = "";

	std::vector<std::string> options;
	int adaptability = 0;
	// Find an available unit that can be recruited,
	// matches the desired usage type, and comes in under budget.
	int bonus = 0;
	if (gold > 5 * max_recruit_cost) {
		bonus = rand() % 4;
	} else if (gold > 4 * max_recruit_cost) {
		bonus = rand() % 3;
	} else if (gold > 3 * max_recruit_cost) {
		bonus = rand() % 2;
	}
	if (game_config::default_ai_level + bonus > game_config::max_level) {
		bonus = game_config::max_level - game_config::default_ai_level;
	}
	const std::vector<const unit_type*>& recruits = city.recruits(game_config::default_ai_level + bonus);
	const std::vector<arms_feature>& features = current_team_.features();
	const unit_type* leader_ut = NULL;
	for (std::vector<const unit_type*>::const_iterator itor = recruits.begin(); itor != recruits.end(); ++ itor) {
		const unit_type* ut = *itor;
		const std::string& name = ut->id();

		if (!leader_ut && ut->leader()) {
			leader_ut = ut;
		}

		bonus = 0;
		for (std::vector<arms_feature>::const_iterator it2 = features.begin(); it2 != features.end(); ++ it2) {
			const arms_feature& f = *it2;
			if (ut->arms() == f.arms_) {
				bonus += ftofxp12(1) / 2;
			}
		}
		if (ut->character() != NO_CHARACTER) {
			bonus += ftofxp12(1);
		}

		if (ut->leader()) {
			if (leader) {
				if (!options.empty() && !unit_types.find(options.back())->leader()) {
					options.clear();
				}
				// set adaptability to most value, avoid non-leader unit type to add it
				adaptability = 65536;
				options.push_back(name);
			}
		} else if (master.arms_[ut->arms()] + bonus > adaptability) {
			options.clear();
			adaptability = master.arms_[ut->arms()] + bonus;
			options.push_back(name);
		} else if (master.arms_[ut->arms()] + bonus == adaptability) {
			options.push_back(name);
		}
	}
	if (options.empty()) {
		return false;
	}

	index = rand() % options.size();
	const unit_type* ut = unit_types.find(options[index]);

	// std::set<const unit_type*>::const_iterator recruit = recruits.find(ut);
	// size_t index_in_recruits = std::distance(recruits.begin(), recruit);

	for (std::vector<const hero*>::const_iterator itor = expedite_heros.begin(); itor != expedite_heros.end(); ++ itor) {
		city.hero_go_out(**itor);
	}

	// recruit
	do_recruit(units_, heros_, current_team_, ut, expedite_heros, city, cost_exponent_, false);
	manager::raise_gamestate_changed();

	return true;
}

int		callerlayer = 0;
uint32_t	total_time_slow = 0;
uint32_t	total_time_unmove = 0;
uint32_t	total_time_move = 0;
uint32_t	total_time_bonus = 0;
uint32_t	total_time_analyze = 0;
uint32_t	total_time_back = 0;

uint32_t	total_time_stack = 0;

bool used_locations_[12];
bool used_locations_2_[18];
bool used_locations_3_[24];

void expand_rect(SDL_Rect& rect, const map_location& loc)
{
	if (rect.x != -1) {
		if (loc.x < rect.x) {
			rect.w += rect.x - loc.x;
			rect.x = loc.x;
		} else if (loc.x > rect.x + rect.w) {
			rect.w = loc.x - rect.x;
		}
		if (loc.y < rect.y) {
			rect.h += rect.y - loc.y;
			rect.y = loc.y;
		} else if (loc.y > rect.y + rect.h) {
			rect.h = loc.y - rect.y;
		}
	} else {
		rect.x = loc.x;
		rect.y = loc.y;
		rect.w = rect.h = 0;
	}
}

void ai_default::clear_stats_cache()
{
	std::map<std::pair<const unit*, const unit_type*>, battle_context*>::iterator usc;
	for (usc = unit_stats_cache_.begin(); usc != unit_stats_cache_.end(); ++ usc) {
		delete usc->second;
	}
	unit_stats_cache_.clear();
}

void ai_default::analyze_targets(std::vector<attack_analysis>& res, bool unmovementable)
{
/*
	posix_print("ai_default::analyzing targets------\n");
*/
	unsigned int	prevsize = 0, incsize = 0;

	uint32_t	time_taken_cal, time_taken, max_time = 0;

	uint32_t	max_total_time_slow = 0;
	uint32_t	max_total_time_unmove = 0;
	uint32_t	max_total_time_move = 0;
	uint32_t	max_total_time_bonus = 0;
	uint32_t	max_total_time_analyze = 0;
	uint32_t	max_total_time_back = 0;

	uint32_t	max_total_time_stack = 0;

	uint32_t	ticks_e;
	uint32_t	ticks = SDL_GetTicks();

	// In order to speed analysize, allow don't execute pack/unpack even if need.
	unit::ignore_pack_ = true;

	// clear unit stats cache every attack analysis.
	clear_stats_cache();
	for (std::map<unit*, std::pair<map_location, unit*>* >::iterator itor = reside_cache_.begin(); itor != reside_cache_.end(); ++ itor) {
		delete itor->second;
	}
	reside_cache_.clear();

	unit_map& units_ = get_info().units;

	std::vector<std::pair<unit*, int> > unit_locs;
	std::vector<std::pair<unit*, int> > unit_locs2;
	std::vector<int> movements;
	SDL_Rect consider_enemy_rect = current_team_.city_rect();
	// citys
	const std::vector<artifical*>& holded_cities = current_team_.holded_cities();
	int index;
	for (std::vector<artifical*>::const_iterator i = holded_cities.begin(); i != holded_cities.end(); ++ i) {
		artifical* city = *i;
		if (city->attacks_left()) {
			unit_locs.push_back(std::make_pair<unit*, int>(city, -1));
			if (unmovementable) {
				movements.push_back(city->movement_left());
				city->set_movement(0);
			}
		}
		std::vector<unit*>& reside_troops = city->reside_troops();
		index = reside_troops.size() - 1;
		for (std::vector<unit*>::reverse_iterator itor = reside_troops.rbegin(); itor != reside_troops.rend(); ++ itor, index --) {
			unit& u = **itor;
			if (u.human() || u.has_less_loyalty(game_config::ai_keep_loyalty_threshold, leader_)) {
				continue;
			}
			unit_locs.push_back(std::make_pair<unit*, int>(&u, index));
			if (unmovementable) {
				movements.push_back(u.movement_left());
				u.set_movement(0);
			}
		}
	}
	// field troops
	const std::pair<unit**, size_t> p = current_team_.field_troop();
	unit** troops = p.first;
	size_t troops_vsize = p.second;
	for (size_t i = 0; i < troops_vsize; i ++) {
		if (!troops[i]->human() && troops[i]->attacks_left() && !troops[i]->wall()) {
			unit_locs.push_back(std::make_pair<unit*, int>(troops[i], -1));
			if (unmovementable) {
				movements.push_back(troops[i]->movement_left());
				troops[i]->set_movement(0);
			}
			expand_rect(consider_enemy_rect, troops[i]->get_location());
		}
	}
	unit_locs2 = unit_locs;

	// more 10 grid
	consider_enemy_rect = extend_rectangle(map_, consider_enemy_rect, 10);
/*
	consider_enemy_rect.x = std::max<int>(0, consider_enemy_rect.x - 10);
	consider_enemy_rect.w = std::min<int>(map_.w() - consider_enemy_rect.x, consider_enemy_rect.w + 10 * 2);
	consider_enemy_rect.y = std::max<int>(0, consider_enemy_rect.y - 10);
	consider_enemy_rect.h = std::min<int>(map_.h() - consider_enemy_rect.y, consider_enemy_rect.h + 10 * 2);
*/	
	// 占7个格子时，邻近有12个格子
	std::fill(used_locations_, used_locations_ + 12, false);
	std::fill(used_locations_2_, used_locations_2_ + 18, false);
	std::fill(used_locations_3_, used_locations_3_ + 24, false);

	std::map<int, pathfind::paths> possible_moves2;
	std::multimap<int, map_location> srcdst2;
	std::multimap<map_location, int> dstsrc2; 
	calculate_possible_moves2(unit_locs, possible_moves2, srcdst2, dstsrc2, false, false, get_avoid());

	time_taken_cal = SDL_GetTicks() - ticks;

	for (unit_map::const_iterator j = units_.begin(); j != units_.end(); ++j) {
		const map_location& candidate_loc = j->get_location();
		if (j->wall() && units_.find(candidate_loc).valid()) {
			// if has unit on it, cannot attack this wall.
			continue;
		}
		if (!point_in_rect(candidate_loc.x, candidate_loc.y, consider_enemy_rect)) {
			continue;
		}
		// Attack anyone who is on the enemy side,
		// and who is not invisible or petrified.
		if (current_team_.is_enemy(j->side()) && !j->incapacitated() && j->invisible(candidate_loc) == false) {
			attack_analysis analysis;
			analysis.target = &*j;

			total_time_slow = 0;
			total_time_unmove = 0;
			total_time_move = 0;
			total_time_bonus = 0;
			total_time_analyze = 0;
			total_time_back = 0;

			total_time_stack = 0;


			ticks = SDL_GetTicks();

			do_attack_analysis(&*j, unit_locs2, srcdst2, dstsrc2, unit_locs, res, analysis);

			ticks_e = SDL_GetTicks();
			time_taken = ticks_e - ticks;
			if(time_taken > max_time) {
				max_time = time_taken;
				incsize = res.size() - prevsize;

				max_total_time_slow = total_time_slow;
				max_total_time_unmove = total_time_unmove;
				max_total_time_move = total_time_move;
				max_total_time_bonus = total_time_bonus;
				max_total_time_analyze = total_time_analyze;
				max_total_time_back = total_time_back;

				max_total_time_stack = total_time_stack;
			}
			prevsize = res.size();

		}
	}

	if (unmovementable) {
		size_t i = 0;
		for (std::vector<int>::iterator mov = movements.begin(); mov != movements.end(); ++ mov, i ++) {
			unit_locs2[i].first->set_movement(*mov);
		}
	}

	unit::ignore_pack_ = false;
/*
	posix_print("-->[%u]cal: %u + max_time: %u(slow: %u, unmove: %u, move: %u, bonus: %u, analyze: %u, stack: %u, back: %u), incsize: %u\n", 
		unit_stats_cache_.size(),
		time_taken_cal, max_time,
		max_total_time_slow, max_total_time_unmove, max_total_time_move, max_total_time_bonus, max_total_time_analyze, max_total_time_stack, max_total_time_back,
		incsize);
*/
}

void ai_default::do_attack_analysis(
	                 unit* target_ptr,
					 const std::vector<std::pair<unit*, int> >& units2,
	                 const std::multimap<int, map_location>& srcdst2, const std::multimap<map_location, int>& dstsrc2,
					 std::vector<std::pair<unit*, int> >& units,
	                 std::vector<attack_analysis>& result,
					 attack_analysis& cur_analysis
	                )
{
	map_location* tiles;
	bool* used_locations;
	
	// This function is called fairly frequently, so interact with the user here.
	uint32_t ticks_fun = SDL_GetTicks();

	callerlayer ++;

	if (cur_analysis.movements.size() >= size_t(attack_depth_)) {
		// std::cerr << "ANALYSIS " << cur_analysis.movements.size() << " >= " << attack_depth_ << "\n";
		callerlayer --;
		return;
	}
	gamemap &map_ = get_info().map;
	unit_map &units_ = get_info().units;
	std::vector<team> &teams_ = get_info().teams;

	const map_location& loc = target_ptr->get_location();
	
	const size_t max_positions = 1000;
	if (result.size() > max_positions && !cur_analysis.movements.empty()) {
		callerlayer --;
		return;
	}

	for (size_t i = 0; i != units.size(); ++i) {
		uint32_t ticks_slow = SDL_GetTicks();

		// curr_pair is colne of units[i], NOT be refrence! it may be erase.
		std::pair<unit*, int> curr_pair = units[i];
		unit* src_ptr = units[i].first;
		const map_location current_unit = src_ptr->get_location();
		

		// See if the unit has the backstab ability.
		// Units with backstab will want to try to have a
		// friendly unit opposite the position they move to.
		//
		// See if the unit has the slow ability -- units with slow only attack first.
		bool backstab = false, slow = false;
		std::vector<attack_type>& attacks = src_ptr->attacks();
/*
		for (std::vector<attack_type>::iterator a = attacks.begin(); a != attacks.end(); ++a) {
			a->set_specials_context(map_location(), map_location(), units_, true, NULL);
			if (a->get_special_bool("backstab")) {
				backstab = true;
			}

			if (a->get_special_bool("slow")) {
				slow = true;
			}
		}
*/
		if (slow && cur_analysis.movements.empty() == false) {
			continue;
		}

		int best_rating = 0;
		int cur_position = -1;

		// 根据src_ptr(攻击部队)，target_ptr(防御部队)，计算出range
		// battle_context是要费点时间的，为节省时间消耗，不要求所有都计算，只计算“距离”上认为可能能攻击到的
		// distance_between计算出的是个近似值, 为让更多可能的攻击能到达，给出两个放宽条件
		// 1.移动每个隔子只需1点移动力
		// 2.给个加2值的放大值
		if (src_ptr->movement_left() + 2 < (int)distance_between(loc, current_unit)) {
			continue;
		}
		
		battle_context* bc = NULL;

		// This cache is only about 99% correct, but speeds up evaluation by about 1000 times.
		// We recalculate when we actually attack.
		std::map<std::pair<const unit*, const unit_type*>, battle_context*>::iterator usc;

		const unit_type* src_type;
		if (!src_ptr->packed()) {
			src_type = src_ptr->type();
		} else {
			src_type = unit_types.find(src_ptr->packee_type_id());
		}

		usc = unit_stats_cache_.find(std::pair<const unit*, const unit_type*>(target_ptr, src_type));
		// Just check this attack is valid for this attacking unit (may be modified)
		const battle_context::unit_stats* att;
		if (usc != unit_stats_cache_.end()) {
			att = &usc->second->get_attacker_stats();
		} else {
			bc = new battle_context(units_, *src_ptr, *target_ptr, -1, -1, aggression_, NULL);
			att = &(bc->get_attacker_stats());

			unit_stats_cache_.insert(std::pair<std::pair<const unit*, const unit_type*>, battle_context*>
				(std::pair<const unit*, const unit_type*>(target_ptr, src_type), bc));
		}
		// range
		if (!att->weapon) {
			// if attack hasn't attack, att->weapon is NULL.
			continue;
		}
		size_t adjacent_size;
		const std::string& range = att->weapon->range();

		if (range == "melee") {
			// range: 1
			tiles = target_ptr->adjacent_;
			adjacent_size = target_ptr->adjacent_size_;
			used_locations = used_locations_;
		} else if (range == "ranged") {
			// range: 2
			tiles = target_ptr->adjacent_2_;
			adjacent_size = target_ptr->adjacent_size_2_;
			used_locations = used_locations_2_;
		} else if (range == "cast") {
			// other to range: 3
			tiles = target_ptr->adjacent_3_;
			adjacent_size = target_ptr->adjacent_size_3_;
			used_locations = used_locations_3_;
		} else {
			VALIDATE(false, _("do_attack_analysis, unknown range."));
		}

		total_time_slow += SDL_GetTicks() - ticks_slow;

		// Iterate over positions adjacent to the unit, finding the best rated one.
		for (int j = 0; j != adjacent_size; ++j) {

			uint32_t ticks_move = SDL_GetTicks();

			// If in this planned attack, a unit is already in this location.
			if (used_locations[j]) {
				continue;
			}

			// See if the current unit can reach that position.
			if (tiles[j] != current_unit) {
				typedef std::multimap<map_location, int>::const_iterator Itor;
				std::pair<Itor, Itor> its = dstsrc2.equal_range(tiles[j]);
				while (its.first != its.second) {
					// if (its.first->second == current_unit) {
					if (units2[its.first->second].first == src_ptr) {
						break;
					}
					++ its.first;
				}

				// If the unit can't move to this location.
				if (its.first == its.second || units_.find(tiles[j]) != units_.end()) {
					total_time_unmove += SDL_GetTicks() - ticks_move;
					continue;
				}
			} else if (curr_pair.second >= 0) {
				// reside troop cannot attack in residing-city
				total_time_unmove += SDL_GetTicks() - ticks_move;
				continue;
			}

			uint32_t ticks_bonus = SDL_GetTicks();
			total_time_move += ticks_bonus - ticks_move;
/*
			unit_ability_list abil = src_ptr->get_abilities("leadership", tiles[j]);
			int best_leadership_bonus = abil.highest("value").first;
			double leadership_bonus = static_cast<double>(best_leadership_bonus+100)/100.0;
			if (leadership_bonus > 1.1) {
				LOG_AI << src_ptr->name() << " is getting leadership " << leadership_bonus << "\n";
			}
*/
			double leadership_bonus = 1.0;


			// See if this position is the best rated we've seen so far.
			int rating = static_cast<int>(rate_terrain(*src_ptr, tiles[j]) * leadership_bonus);

			uint32_t ticks_vulnerability = SDL_GetTicks();
			total_time_bonus += ticks_vulnerability - ticks_bonus;

			if (cur_position >= 0 && rating < best_rating) {
				continue;
			}

			// If this is a position with equal defense to another position,
			// but more vulnerability then we don't want to use it.
			if (cur_position >= 0 && rating == best_rating) {
				continue;
			}

			cur_position = j;
			best_rating = rating;
		}

		if (cur_position != -1) {
			uint32_t ticks_analyze = SDL_GetTicks();

			units.erase(units.begin() + i);

			// don't use units[i].first, i is invalid because of erase.
			cur_analysis.movements.push_back(std::make_pair<std::pair<unit*, int>, map_location>(curr_pair, tiles[cur_position]));

			{
				attack_analysis_lock lock(cur_analysis);

				cur_analysis.analyze(map_, units_, unit_stats_cache_, units2, dstsrc2, srcdst2, reside_cache_, aggression_);

				uint32_t end_analyze = SDL_GetTicks();
				total_time_analyze += end_analyze - ticks_analyze;

				result.push_back(cur_analysis);
				used_locations[cur_position] = true;

				total_time_stack += SDL_GetTicks() - end_analyze;

				do_attack_analysis(target_ptr, units2, srcdst2, dstsrc2, units, result, cur_analysis);
			
				used_locations[cur_position] = false;
			}

			uint32_t ticks_end_do_attack_analysis = SDL_GetTicks();

			cur_analysis.movements.pop_back();

			// don't use units[i].first, i is invalid because of erase.
			units.insert(units.begin() + i, curr_pair);

			total_time_back += SDL_GetTicks() - ticks_end_do_attack_analysis;
		}
	}
	callerlayer --;
}

int ai_default::rate_terrain(const unit& u, const map_location& loc) const
{
	gamemap &map_ = get_info().map;
	const t_translation::t_terrain terrain = map_.get_terrain(loc);
	const int defense = u.defense_modifier(terrain);
	int rating = 100 - defense;

	const int healing_value = 10;
	const int friendly_village_value = 5;
	const int neutral_village_value = 10;
	const int enemy_village_value = 15;
	const int wall_value = 150;

	if(map_.gives_healing(terrain) && u.get_ability_bool("regenerates",loc) == false) {
		rating += healing_value;
	}

	if (map_.is_village(terrain)) {
		int owner = village_owner(loc, get_info().teams) + 1;

		if(owner == side_) {
			rating += friendly_village_value;
		} else if(owner == 0) {
			rating += neutral_village_value;
		} else {
			rating += enemy_village_value;
		}
	}

	unit_map::node* curr_node = units_.get_cookie(loc, false);
	if (curr_node && curr_node->second->wall()) {
		rating += wall_value;
	}

	return rating;
}

} //end of namespace ai

