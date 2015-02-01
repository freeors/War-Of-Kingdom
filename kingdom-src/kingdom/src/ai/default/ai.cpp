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

#include "game_end_exceptions.hpp"
#include "../../game_events.hpp"
#include "../../game_preferences.hpp"
#include "log.hpp"
#include "mouse_handler_base.hpp"
#include "../../replay.hpp"
#include "../../statistics.hpp"
#include "../../unit_display.hpp"
#include "wml_exception.hpp"
#include "resources.hpp"
#include "play_controller.hpp"
#include "callable_objects.hpp"
#include "artifical.hpp"
#include "terrain_filter.hpp"
#include "gettext.hpp"
#include "filesystem.hpp"

#include <boost/foreach.hpp>
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
extern uint32_t total_diplomatism;

namespace ai {

#ifdef _MSC_VER
#pragma warning(pop)
#endif

std::map<std::pair<const unit*, const unit*>, battle_context*> ai_default::unit_stats_cache;

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
	, previous_turn_(0)
	, leader_(*get_info().teams[side - 1].leader())
	, tod_manager_(get_info().tod_manager_)
	, controller_(*resources::controller)
	, rpg_hero_(rpg::h)
	, cost_exponent_(100)
	, consider_combat_(true)
	, plan_to_()
	, aggression_(0.5)
	, attack_depth_(5)
	, side_cache_int_(NULL)
	, side_cache_size_(0)
	, capturing_villages_()
	, guard_loc_()
{
	manager::add_gamestate_observer(this);

	// read attribute in [ai]
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

config ai_default::to_config() const
{
	config cfg;
	cfg["aggression"] = aggression_;
	cfg["attack_depth"] = attack_depth_;
	
	return cfg;
}

void ai_default::handle_generic_event(const std::string& /*event_name*/)
{
}

const game_info& ai_default::get_info() const
{
	return manager::get_active_ai_info_for_side(side_);
}

void ai_default::play_turn()
{
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
		do_move();

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
			srcdst.insert(std::make_pair(index, un_loc));
			dstsrc.insert(std::make_pair(un_loc, index));
		}
		res.insert(std::make_pair(
					index, pathfind::paths(map_, units_, *unit_ptr, un_loc, teams_, false, false, current_team_, 0, see_all)));
	}

	guard_cache::tguard* guard;
	for (std::map<int, pathfind::paths>::iterator m = res.begin(); m != res.end(); ++m) {
		unit* src_ptr = troops[m->first].first;
		src_ptr->manage_attackable_rect(true);
		if (src_ptr->task() == unit::TASK_GUARD) {
			guard = &guard_cache::find(*src_ptr);
		} else {
			guard = NULL;
		}

		BOOST_FOREACH (const pathfind::paths::step &dest, m->second.destinations) {
			const map_location& src = src_ptr->get_location();
			const map_location& dst = dest.curr;

			if (troops[m->first].second >= 0 && dst == src) {
				continue;
			}

			if (remove_destinations != NULL && remove_destinations->match(dst)) {
				continue;
			}

			if (guard) {
				if ((int)distance_between(guard->loc, dst) > guard->radius) {
					continue;
				}
			}

			if (src != dst && !units_.find_unit(dst, true)) {
				unit* base = units_.find_unit(dst, false);
				if (!base || base->can_stand(*src_ptr)) {
					srcdst.insert(std::make_pair(m->first, dst));
					dstsrc.insert(std::make_pair(dst, m->first));
					src_ptr->fill_movable_loc(dst);
				}
			}
		}
		src_ptr->manage_attackable_rect(false);
	}
}

ai_plan& ai_default::plan(bool action)
{
	int side = side_;

	plan_to_.reset();
	units_.calculate_mrs_data(gamestate_, plan_to_.mrs_, side, action);
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
			} else if (fronts < mr_data::max_fronts && office_heros > mr_data::max_front_requirement) {
				output.push_back(std::make_pair(city, office_heros - mr_data::max_front_requirement));
			}
		} else {
			if (office_heros < mr_data::min_interior_requirement) {
				if (city->mayor() != rpg::h) {
					input[city] = office_heros - mr_data::min_interior_requirement;
				}
			} else if (office_heros > mr_data::max_interior_requirement) {
				output.push_back(std::make_pair(city, office_heros - mr_data::max_interior_requirement));
			}
		}
	}
	for (std::vector<artifical*>::reverse_iterator city_ritor = mr.own_front_cities.rbegin(); city_ritor != mr.own_front_cities.rend(); ++ city_ritor) {
		artifical* city = *city_ritor;
		if (city == aggressing) {
			continue;
		}
		int office_heros = city->office_heros();
		fronts = city->fronts();

		if (fronts >= mr_data::min_alert_fronts) {
			int min_requirement = (fronts == mr_data::max_fronts)? (mr_data::max_front_requirement - 1): mr_data::mid_front_requirement;
			if (office_heros < min_requirement) {
				if (city->mayor() != rpg::h) {
					input[city] = office_heros - min_requirement;
				}
			} else if (fronts < mr_data::max_fronts && office_heros > mr_data::max_front_requirement) {
				output.push_back(std::make_pair(city, office_heros - mr_data::max_front_requirement));
			}
		} else {
			if (office_heros < mr_data::min_front_requirement) {
				if (city->mayor() != rpg::h) {
					input[city] = office_heros - mr_data::min_front_requirement;
				}
			} else if (office_heros > mr_data::mid_front_requirement) {
				output.push_back(std::make_pair(city, office_heros - mr_data::mid_front_requirement));
			}
		}
	}

	if (output.empty()) {
		if (aggressing && !input.empty()) {
			int office_heros = aggressing->office_heros();
			if (office_heros > mr_data::aggress_requirement) {
				// let aggressing inching
				output.push_back(std::make_pair(aggressing, 2));
			}
		}
	} else if (input.empty()) {
		if (aggressing) {
			input[aggressing] = -10;
		}
	}

	if (output.empty() || input.empty()) {
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
			while (!hero_list.empty() || city.soldiers()) {
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
				if (current_team_.auto_move_human || !u.human()) {
					u.set_goto(map_location());
				}
			}
			std::vector<unit*>& fields = city.field_troops();
			for (std::vector<unit*>::iterator u_itor = fields.begin(); u_itor != fields.end(); ++ u_itor) {
				unit* troop = *u_itor;
				if (troop->provoked_turns()) {
					troop->set_goto(find_provoke(troop)->get_location());
				} else if (current_team_.auto_move_human || !troop->human()) {
					const map_location& goto_loc = troop->get_goto();
					if (goto_loc.valid() && troop->get_location() != goto_loc && !units_.valid2(goto_loc, false)) {
						if (map_.is_village(goto_loc) && !current_team_.owns_village(goto_loc)) {
							capturing_villages_.insert(goto_loc);
						} else {
							troop->set_goto(city.get_location());
						}
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
			unit& u = *troops[i];
			if (!u.is_artifical()) {
				// field troops
				artifical* city = mr.enemy_cities[rand() % mr.enemy_cities.size()];
				if (u.provoked_turns()) {
					u.set_goto(find_provoke(&u)->get_location());
				} else if (!u.is_commoner() && (current_team_.auto_move_human || !u.human())) {
					u.set_goto(city->get_location());
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
			while (!hero_list.empty() || city.soldiers()) {
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
				if ((current_team_.auto_move_human || !u.human()) && !u.get_goto().valid()) {
					u.set_goto(map_location());
				}
			}
			std::vector<unit*>& fields = city.field_troops();
			for (std::vector<unit*>::iterator u_itor = fields.begin(); u_itor != fields.end(); ++ u_itor) {
				unit* troop = *u_itor;
				if (troop->provoked_turns()) {
					troop->set_goto(find_provoke(troop)->get_location());
				} else if (current_team_.auto_move_human || !troop->human()) {
					const map_location& goto_loc = troop->get_goto();
					if (goto_loc.valid() && troop->get_location() != goto_loc) {
						if (!units_.valid2(goto_loc, false)) {
							if (map_.is_village(goto_loc) && !current_team_.owns_village(goto_loc)) {
								capturing_villages_.insert(goto_loc);
							} else {
								troop->set_goto(city.get_location());
							}
						}
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
			if (current_team_.auto_move_human || !u.human()) {
				u.set_goto(aggressed->get_location());
			}
		}
		std::vector<hero*>& hero_list = aggressing->fresh_heros();
		while (!hero_list.empty() || aggressing->soldiers()) {
			// recruit! until balance in this military-region
			if (!do_recruitment(*aggressing)) {
				// cannot recurite more, my be not enough gold
				break;
			}
			unit* new_troop = reside_troops.back();
			if (current_team_.auto_move_human || !new_troop->human()) {
				new_troop->set_goto(aggressed->get_location()); // target to centor of this military region
			}
		}
		
		int aggressing_heros = aggressing->office_heros();
		aggressing_heros -= 50;

		if (tent::mode == mode_tag::RPG && aggressing_heros < 0) {
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
			if (troop->provoked_turns()) {
				troop->set_goto(find_provoke(troop)->get_location());
			} else if (current_team_.auto_move_human || !troop->human()) {
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
			while (!hero_list.empty() || city.soldiers()) {
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
				if (current_team_.auto_move_human || !u.human()) {
					u.set_goto(map_location());
				}
			}
			std::vector<unit*>& fields = city.field_troops();
			for (std::vector<unit*>::iterator u_itor = fields.begin(); u_itor != fields.end(); ++ u_itor) {
				unit* troop = *u_itor;
				if (troop->provoked_turns()) {
					troop->set_goto(find_provoke(troop)->get_location());
				} else if (current_team_.auto_move_human || !troop->human()) {
					const map_location& goto_loc = troop->get_goto();
					if (goto_loc.valid() && troop->get_location() != goto_loc && !units_.valid2(goto_loc, false)) {
						if (map_.is_village(goto_loc) && !current_team_.owns_village(goto_loc)) {
							capturing_villages_.insert(goto_loc);
						} else {
							troop->set_goto(city.get_location());
						}
					} else {
						troop->set_goto(city.get_location());
					}
				}
			}
		}
	}

	if (current_team_.holden_cities().size() == 1) {
		guard_loc_ = current_team_.holden_cities().front()->get_location();
	} else {
		guard_loc_ = map_location();
	}
}

bool ai_default::can_allied(const team& to, int target_side, artifical* aggressed)
{
	if (to.is_human() || to.is_empty() || to.leader()->get_flag(hero_flag_npc)) {
		return false;
	}
	if (to.holden_cities().empty()) {
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
				const std::vector<artifical*>& holden_cities = teams_[val - 1].holden_cities();
				for (std::vector<artifical*>::const_iterator it_city = holden_cities.begin(); it_city != holden_cities.end(); ++ it_city) {
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
					const std::vector<artifical*>& holden_cities = teams_[i].holden_cities();
					for (std::vector<artifical*>::const_iterator it_city = holden_cities.begin(); it_city != holden_cities.end(); ++ it_city) {
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

void ai_default::do_tactic(unit* actor)
{
	// of course, revival troop maybe can cast tactic again.
	// but after it, when move-to-attack has chance to cast tactic still.
	std::pair<unit**, size_t> fields = current_team_.field_troop();
	unit** troops = fields.first;
	size_t troops_vsize = fields.second;
	for (size_t i = 0; i < troops_vsize; i ++) {
		unit& u = *troops[i];
		if (!actor || &u == actor) {
			u.judge_and_cast_tactic(game_config::threshold_score_ai);
		}
	}
}

void ai_default::move_fresh_hero(artifical& from, artifical& to, int index)
{
	std::vector<hero*>& src_heros = from.fresh_heros();
	
	std::set<size_t> checked_heros;
	checked_heros.insert(index);

	recorder.add_move_heros(from.get_location(), to.get_location(), checked_heros);

	hero* h = src_heros[index];
	from.hero_go_out(*h);
	to.move_into(*h);
	
	disp_.invalidate(from.get_location());
	disp_.invalidate(to.get_location());

	game_events::fire("post_moveheros", from.get_location(), to.get_location());
}

void ai_default::move_hero(artifical& from, artifical& to, int& lack, int& more)
{
	VALIDATE(lack < 0 && more > 0, "ai_default::move_hero, lack >= 0 || more <= 0, error!"); 
	VALIDATE(from.cityno() != to.cityno(), "ai_default::move_hero, from city must not samw to city!"); 

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
		if (troop->human() || troop->provoked_turns()) {
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

void ai_default::do_move()
{
	unit* actor = tent::turn_based? NULL: unit::actor;
	artifical* actor_city = (actor && actor->is_city())? unit_2_artifical(actor): NULL;
	manager::raise_user_interact();

	if (actor && tent::tower_mode()) {
		// for simple, rest goto.
		actor->set_goto(map_location());
	}

	bool leader_mode = current_team_.is_human() && rpg::stratum == hero_stratum_leader;

	std::vector<std::pair<unit*, int> > gotos;
	if (!tent::tower_mode() && leader_mode) {
		while (consider_combat_) {
			consider_combat_ = do_combat(actor);
			if (actor && !actor_can_continue_action(units_, side_)) {
				// actor is dead/come into city.
				return;
			}
		}

		std::pair<unit**, size_t> fields = current_team_.field_troop();
		unit** troops = fields.first;
		size_t troops_vsize = fields.second;
		for (size_t i = 0; i < troops_vsize; i ++) {
			unit& u = *troops[i];
			if ((!actor || actor == &u) && u.movement_left()) {
				if (u.provoked_turns()) {
					u.set_goto(find_provoke(&u)->get_location());
					gotos.push_back(std::make_pair(&u, -1));
					// "end" this unit
					// u.set_movement(0);
					// u.set_attacks(0);

				} else if (u.task() == unit::TASK_GUARD) {
					u.set_goto(guard_cache::find(u).loc);
					gotos.push_back(std::make_pair(&u, -1));
				}
			}
		}

	} else {
		plan_to_.reset();
		std::vector<mr_data>& mrs = plan_to_.mrs_;
		if (controller_.turn() != previous_turn_) {
			recorder.init_ai(); // prepare [random]
			units_.calculate_mrs_data(gamestate_, mrs, side_);
		}
		mr_data& mr = mrs[0];
		
		int maximum_side_build_walls = 3;
		int maximum_side_build_markets = 2;

		const std::set<const unit_type*>& can_build = current_team_.builds();
		std::set<const unit_type*>::const_iterator end = can_build.end();

		if (can_build.find(unit_types.find_keep()) == end || can_build.find(unit_types.find_wall()) == end) {
			maximum_side_build_walls = 0;
		}
		std::set<const unit_type*> can_build_ea;
		for (std::set<const unit_type*>::const_iterator it = can_build.begin(); it != can_build.end(); ++ it) {
			const unit_type* ut = *it;
			if (ut == unit_types.find_market() || ut == unit_types.find_technology() || ut == unit_types.find_tactic()) {
				can_build_ea.insert(ut);
			}
		}
		if (can_build_ea.empty()) {
			maximum_side_build_markets = 0;
		}

		if (tent::mode == mode_tag::RPG && (!actor || actor_city)) {
			do_diplomatism(0);
			satisfy_hero_requirement(0);
		}
	
		calculate_mr_target(0);

		do_tactic(actor);

		// attack! own field/reside troops and cities vs enemy field/reside troops and cities
		while (consider_combat_) {
			consider_combat_ = do_combat(actor);
			if (actor && !actor_can_continue_action(units_, side_)) {
				// actor is dead/come into city.
				return;
			}
		}

		if (mr.target == mr_data::TARGET_AGGRESS && mr.enemy_cities[0]->side() == side_) {
			// capture aggressing city.
			// don't do action that maybe result into no-async with replay, for example mr.own_front_cities[0]->set_fronts(0).
			units_.ai_capture_aggressed(*mr.enemy_cities[0], side_, true);
		}

		// build artifical
		for (std::map<int, mr_data::enemy_data>::iterator it = mr.own_cities.begin(); it != mr.own_cities.end(); ++ it) {
			artifical& owner = *units_.city_from_cityno(it->first);
			if (owner.side() != side_) {
				continue;
			}
			if (!maximum_side_build_walls && !maximum_side_build_markets) {
				continue;
			}
			if (leader_mode || owner.mayor() == rpg::h) {
				continue;
			}

			int maximum_build_walls = maximum_side_build_walls;
			int maximum_build_markets = maximum_side_build_markets;


			const unit_type* art = NULL;
			const map_location* at = NULL;

			// wall
			std::vector<map_location*> keeps;
			std::vector<map_location*> wall_vacants;
			std::vector<map_location*> keep_vacants;

			while ((!actor || actor_city) && maximum_build_walls) {
				if (current_team_.gold() < (unit_types.find_wall()->cost() + unit_types.find_keep()->cost()) * 2) {
					maximum_build_walls = 0;
					break;
				}
				calculate_wall_tiles(units_, map_, owner, keeps, wall_vacants, keep_vacants);
				if (wall_vacants.empty() && keep_vacants.empty()) {
					maximum_build_walls = 0;
					break;
				}
				at = NULL;
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
					controller_.do_build(current_team_, NULL, art, *at);
					maximum_build_walls --;

				} else {
					maximum_build_walls = 0;
				}
			}

			while (maximum_build_markets) {
				// market
				std::vector<const map_location*> ea_vacants;

				at = NULL;
				if (!current_team_.gold_can_build_ea()) {
					maximum_build_markets = 0;
					break;
				}

				art = owner.next_ea_utype(ea_vacants, can_build_ea);
				if (!art) {
					maximum_build_markets = 0;
					break;
				}

				at = ea_vacants[rand() % ea_vacants.size()];
				controller_.do_build(current_team_, NULL, art, *at);
				maximum_build_markets --;
			}
		}

		// 9. all unmoved field/reside troops execute move. (according to goto)
		// calcuate best attackable city. removeable unit will move to it.

		if (!actor || actor_city) {
			for (std::map<int, mr_data::enemy_data>::iterator itor = mr.own_cities.begin(); itor != mr.own_cities.end(); ++ itor) {
				artifical* city = units_.city_from_cityno(itor->first);
				if (city->side() != side_) {
					continue;
				}

				std::vector<unit*>& reside_troops = city->reside_troops();
				// capture village
				if (!reside_troops.empty()) {
					std::set<map_location> goto_locs;
					if (mr.target != mr_data::TARGET_AGGRESS || city != mr.own_front_cities[0]) {
						const std::set<map_location>& villages = city->villages();
						for (std::set<map_location>::const_iterator vill = villages.begin(); vill != villages.end(); ++ vill) {
							if (capturing_villages_.count(*vill) || current_team_.owns_village(*vill)) {
								continue;
							}
							goto_locs.insert(*vill);
						}
					}
					for (std::vector<unit*>::iterator it_u = reside_troops.begin(); it_u != reside_troops.end(); ++ it_u) {
						unit& u = **it_u;
						if (u.human() || !u.movement_left() || u.has_less_loyalty(game_config::ai_keep_loyalty_threshold, leader_)) {
							continue;
						}
						for (std::set<map_location>::const_iterator to = goto_locs.begin(); to != goto_locs.end(); ++ to) {
							const pathfind::shortest_path_calculator calc(u, current_team_, units_, teams_, map_, false, true);
							// estimate value, three turns
							double stop_at = 100;
							// allowed teleports
							std::set<map_location> allowed_teleports;
							pathfind::plain_route route = a_star_search(city->get_location(), *to, stop_at, &calc, map_.w(), map_.h(), &allowed_teleports);
							if (!route.steps.empty()) {
								if (route.move_cost > u.movement_left() || map_.is_village(*to)) {
									u.set_goto(*to);
									goto_locs.erase(to);
									break;
								}
							}
						}
					}
				}

				// goto queue.
				int index = reside_troops.size() - 1;
				for (std::vector<unit*>::reverse_iterator itor = reside_troops.rbegin(); itor != reside_troops.rend(); ++ itor, index --) {
					unit& u = **itor;
					if (u.human() && !current_team_.auto_move_human) {
						continue;
					}
					if (!u.movement_left() || (!tent::tower_mode() && u.has_less_loyalty(game_config::ai_keep_loyalty_threshold, leader_))) {
						continue;
					}
					gotos.push_back(std::make_pair(city, index));
				}
			}

		}
		if (!actor || !actor_city) {
			std::pair<unit**, size_t> fields = current_team_.field_troop();
			unit** troops = fields.first;
			size_t troops_vsize = fields.second;
			for (size_t i = 0; i < troops_vsize; i ++) {
				unit& u = *troops[i];
				if ((current_team_.auto_move_human || !u.human()) && !u.is_artifical() && u.movement_left()) {
					if ((!actor || &u == actor) || u.get_state(ustate_tag::REVIVALED)) {
						gotos.push_back(std::make_pair(&u, -1));
					}
				}
			}
		}
	}

	map_location goto_loc;
	for (std::vector<std::pair<unit*, int> >::iterator g = gotos.begin(); g != gotos.end(); ++g) {
		// desired move troop may move into one city. after it, g->first became to access denied.
		// so update goto before move_unit.
		if (g->second >= 0) {
			goto_loc = unit_2_artifical(g->first)->reside_troops()[g->second]->get_goto();
		} else {
			goto_loc = g->first->get_goto();
		}
		if (map_.on_board(goto_loc)) {
			controller_.move_unit(true, current_team_, *g, goto_loc, false);
		}
	}

	// end all provoked unit manual.
	std::pair<unit**, size_t> fields = current_team_.field_troop();
	unit** troops = fields.first;
	size_t troops_vsize = fields.second;
	for (size_t i = 0; i < troops_vsize; i ++) {
		unit& u = *troops[i];
		if (u.no_enter_resting() && (!actor || &u == actor)) {
			// "end" this unit
			u.set_movement(0);
			u.set_attacks(0);
		}
	}

	if (!current_team_.is_human() && (!actor || actor_city)) {
		controller_.do_commoner(current_team_);
	}
}

bool ai_default::do_combat(unit* actor)
{
	std::map<std::pair<const unit*, const unit*>, battle_context*>::iterator usc;
	std::map<std::pair<const unit*, const unit*>, battle_context*>& us = unit_stats_cache;

	std::vector<attack_analysis> analysis;
	int start = SDL_GetTicks();
	analyze_targets(actor, analysis);
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
	//
	std::vector<double> ratings;
	int choice_index = -1;
	//
	for (std::vector<attack_analysis>::iterator it = analysis.begin(); it != analysis.end(); ++it) {

		if(skip_num > 0 && ((it - analysis.begin())%skip_num) && it->movements.size() > 1)
			continue;

		const double rating = it->rating(aggression_, guard_loc_);

		ratings.push_back(rating);

		if (rating > choice_rating) {
			choice_it = it;
			choice_rating = rating;
			choice_index = std::distance(analysis.begin(), it);
		}
	}

	// suokko tested the rating against current_team_.caution()
	// Bad mistake -- the AI became extremely reluctant to attack anything.
	// Documenting this in case someone has this bright idea again...*don't*...
	if (choice_rating > 0.0) {
		bool attacker_base = choice_it->movements[0].first.first->base();
		const map_location from = choice_it->movements[0].first.first->get_location();
		const map_location to   = choice_it->movements[0].second;

		//
		// get weapon pair first, src unit ptr may change during move_unit below(once expedite). 
		//
		unit* target_ptr = choice_it->target;
		// Recalc appropriate weapons here: AI uses approximations.
		usc = us.find(std::make_pair(target_ptr, choice_it->movements[0].first.first));
		if (usc == us.end()) {
			throw game::game_error("cannot find item from unit_states_cache when attack_enemy");
		}

		// Never used:
		//		const unit_map::const_iterator tgt = units_.find(target_loc);
		std::pair<unit*, int> move_param = choice_it->movements[0].first;
		if (move_param.first->get_location() != to) {
			VALIDATE(move_param.first->movement_left(), "do_combat commands 0 MP to move! check program.");

			if (choice_it->movements[0].first.second >= 0) {
				artifical* city = units_.city_from_loc(from);
				move_param = std::make_pair(city, choice_it->movements[0].first.second);
			} else {
				// field troop. check can formation attck on original location.
				// if attacked, go to again.
				if (move_param.first->can_formation_attack() && move_param.first->formation_attack_enable()) {
					if (do_formation_attack(teams_, units_, disp_, current_team_, *move_param.first, false, false)) {
						return true;
					}
				}
			}

			const map_location arrived_at = controller_.move_unit(true, current_team_, move_param, to);

			if (!arrived_at.valid()) {
				// mover is died
				return true;

			} else if (arrived_at != to || units_.find(to, !attacker_base) == units_.end()) {
				// end unit's movement in this turn
				artifical* city = units_.city_from_loc(arrived_at);
				if (city) {
					// mover entered into city, it results cannot use move_param again.
					city->reside_troops().back()->set_movement(0);
					return true;
				}
				unit& mover = *units_.find_unit(arrived_at);
				mover.set_movement(0);
				if (mover.movement_left() == mover.total_movement()) {
					// see remark#41
					std::vector<map_location> steps;
					steps.push_back(arrived_at);
					recorder.add_movement(steps);
				}

				std::stringstream strstr;
				strstr << "!!! " << utf8_2_ansi(mover.name().c_str());
				strstr << " moving to attack has ended up unexpectedly at (";
				strstr << arrived_at.x << ", " << arrived_at.y << "), when moving to (";
				strstr << to.x << ", " << to.y << ") from (";
				strstr << from.x << ", " << from.y << ")\n";
				posix_print("%s", strstr.str().c_str());
				return true;
			}
		}

		unit* to_ptr = units_.find_unit(to, !attacker_base);

		bool special_attacked = false;
		if (to_ptr->can_formation_attack() && to_ptr->formation_attack_enable()) {
			special_attacked = do_formation_attack(teams_, units_, disp_, current_team_, *to_ptr, false, false);
		}
		if (!special_attacked) {
			special_attacked = to_ptr->judge_and_cast_tactic(game_config::threshold_score_ai);
		}

		if (!special_attacked) {
			attack_enemy(to_ptr, target_ptr, usc->second->get_attacker_stats().attack_num, usc->second->get_defender_stats().attack_num);
		}

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

	if (!current_team_.auto_recruit) {
		return false;
	}

	int gold = current_team_.gold();
	int max_recruit_cost = city.max_recruit_cost() * cost_exponent_ / 100;

	int min_gold_requirement = 2 * max_recruit_cost;
	if (gold < min_gold_requirement) {
		return false;
	}
	if (city.mayor() == rpg::h) {
		return false;
	}

	int holden_troops = current_team_.holden_troops();
	if (current_team_.max_troops > 0 && holden_troops >= current_team_.max_troops) {
		return false;
	}

	manager::raise_user_interact();

	// expedite_heros
	std::vector<const hero*> expedite_heros;
	// field troops
	const std::pair<unit**, size_t> p = current_team_.field_troop();
	size_t maybe_total_troops = p.second;
	// citys
	const std::vector<artifical*>& holden_cities = current_team_.holden_cities();
	for (std::vector<artifical*>::const_iterator i = holden_cities.begin(); i != holden_cities.end(); ++ i) {
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

	const unit_type* ut = NULL;
	if (!fresh_heros.empty()) {
		size_t max_leadership = -1;
		index = 0;
		for (std::vector<hero*>::const_iterator it = fresh_heros.begin(); it != fresh_heros.end(); ++ it, index ++) {
			const hero& h = **it;
			if (h.official_ == hero_official_leader) {
				max_leadership = index;
				break;
			} 
			if (max_leadership == -1 || h.leadership_ > fresh_heros[max_leadership]->leadership_) {
				max_leadership = index;
			}
		}

		const hero& master = *fresh_heros[max_leadership];
		size_t max_army_heros = 3;
		size_t threshold_1 = 6, threshold_2 = 12;
		if (tent::mode == mode_tag::SIEGE) {
			threshold_1 = 8;
			threshold_2 = 16;
		}

		if (current_team_.max_troops > 0 && holden_troops >= current_team_.max_troops - 2) {
			max_army_heros = 3;

		} else if (maybe_total_troops <= threshold_1) {
			max_army_heros = 1;

		} else if ((city.field_troops().size() + city.reside_troops().size()) * 2 + city.fresh_heros().size() + city.finish_heros().size() <= threshold_2) {
			max_army_heros = 2;
		}

		ut = select_unit_type(current_team_, master, city, cost_exponent_);

		expedite_heros.push_back(fresh_heros[max_leadership]);
		fresh_heros.erase(fresh_heros.begin() + max_leadership);

		select_expedite_heros(heros_, max_army_heros, fresh_heros, expedite_heros, *ut);
		
		if (current_team_.is_human() && tent::mode != mode_tag::SIEGE) {
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
				game_events::show_hero_message(&heros_[hero::number_civilian], NULL, message, game_events::INCIDENT_INVALID);

				controller_.do_recruit(&city, cost_exponent_, true);
				return true;
			}
		}
	} else {
		int soldiers = city.soldiers();
		if (soldiers) {
			soldiers -= city.current_soldiers();
		}
		if (soldiers <= 0) {
			return false;
		}
		for (int i = 0; i < 3; i ++) {
			int number = (rand() % (hero::number_soldier_max - hero::number_soldier_min + 1)) + hero::number_soldier_min;
			expedite_heros.push_back(&heros_[number]);
		}
		ut = select_unit_type(current_team_, *(expedite_heros[0]), city, cost_exponent_);
	}

	std::sort(expedite_heros.begin(), expedite_heros.end(), sort_recruit(ut));
	const hero& master = *expedite_heros[0];

	// recruit
	int cost = ut->cost() * cost_exponent_ / 100;
	if (current_team_.stratagem_decrease_cost) {
		cost = cost * 2 / 3;
	}
	do_recruit(units_, heros_, teams_, current_team_, ut, expedite_heros, city, cost, false, true, gamestate_);
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

void ai_default::clear_stats_cache()
{
	std::map<std::pair<const unit*, const unit*>, battle_context*>::iterator usc;
	for (usc = unit_stats_cache.begin(); usc != unit_stats_cache.end(); ++ usc) {
		delete usc->second;
	}
	unit_stats_cache.clear();
}

class ignore_pack_lock
{
public:
	ignore_pack_lock()
	{
		unit::ignore_pack = true;
	}

	~ignore_pack_lock()
	{
		unit::ignore_pack = false;
	}
};

bool guard_can_attack(const unit& u)
{
	if (u.task() != unit::TASK_GUARD) return true;
	return u.hitpoints() > u.max_hitpoints() * 3 / 10;
}

void ai_default::analyze_targets(unit* actor, std::vector<attack_analysis>& res)
{
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

	bool tower_mode = tent::tower_mode();

	// In order to speed analysize, allow don't execute pack/unpack even if need.
	ignore_pack_lock lock;

	// clear unit stats cache every attack analysis.
	clear_stats_cache();

	std::vector<std::pair<unit*, int> > unit_locs;
	std::vector<std::pair<unit*, int> > unit_locs2;
	size_t consider_size = 0;
	std::vector<int> movements;
	SDL_Rect consider_enemy_rect = current_team_.city_rect();
	artifical* actor_city = (actor && actor->is_city())? unit_2_artifical(actor): NULL;
	// citys
	const std::vector<artifical*>& holden_cities = current_team_.holden_cities();
	int index;
	for (std::vector<artifical*>::const_iterator i = holden_cities.begin(); i != holden_cities.end(); ++ i) {
		artifical* city = *i;
		if (city->attacks_left()) {
			if (!actor) {
				unit_locs.push_back(std::make_pair(city, -1));
				consider_size ++;

			} else if (actor_city) {
				unit_locs.insert(unit_locs.begin(), std::make_pair(city, -1));
				consider_size ++;

			} else {
				unit_locs.push_back(std::make_pair(city, -1));
			}
		}
		std::vector<unit*>& reside_troops = city->reside_troops();
		index = reside_troops.size() - 1;
		for (std::vector<unit*>::reverse_iterator itor = reside_troops.rbegin(); itor != reside_troops.rend(); ++ itor, index --) {
			unit& u = **itor;
			if (tent::mode == mode_tag::RPG && u.has_less_loyalty(game_config::ai_keep_loyalty_threshold, leader_)) {
				continue;
			}
			if (u.is_commoner() || !u.human_team_can_ai()) {
				continue;
			}
			if (!guard_can_attack(u)) {
				continue;
			}
			if (!u.movement_left() || !u.attacks_left()) {
				continue;
			}
			if (!u.has_attack_weapon()) {
				continue;
			}
			if (!actor) {
				unit_locs.push_back(std::make_pair(&u, index));
				consider_size ++;

			} else if (actor_city) {
				unit_locs.insert(unit_locs.begin(), std::make_pair(&u, index));
				consider_size ++;

			} else {
				unit_locs.push_back(std::make_pair(&u, index));
			}
		}
	}
	// field troops
	const std::pair<unit**, size_t> p = current_team_.field_troop();
	unit** troops = p.first;
	size_t troops_vsize = p.second;
	for (size_t i = 0; i < troops_vsize; i ++) {
		unit& u = *troops[i];
		if (u.is_commoner()) {
			continue;
		}
		if (!u.human_team_can_ai()) {
			continue;
		}
		if (!guard_can_attack(u)) {
			continue;
		}
		if (!u.attacks_left()) {
			continue;
		}
		if (current_team_.ea_artifical_neutral && hero::is_ea_artifical(u.packee_type()->master())) {
			continue;
		}
		if (!u.has_attack_weapon() || u.get_state(ustate_tag::BUILDING)) {
			continue;
		}
		if (!actor) {
			unit_locs.push_back(std::make_pair(&u, -1));
			expand_rect_loc(consider_enemy_rect, u.get_location());
			consider_size ++;

		} else if (&u == actor || u.get_state(ustate_tag::REVIVALED) || (actor_city && u.is_artifical())) {
			unit_locs.insert(unit_locs.begin(), std::make_pair(&u, -1));
			expand_rect_loc(consider_enemy_rect, u.get_location());
			consider_size ++;

		} else {
			unit_locs.push_back(std::make_pair(&u, -1));
		}
	}

	if (!consider_size) {
		return;
	}

	unit_locs2 = unit_locs;

	// more 10 grid
	consider_enemy_rect = extend_rectangle(map_, consider_enemy_rect, 10);

	// if city, adjacent has 12 grids.
	std::fill(used_locations_, used_locations_ + 12, false);
	std::fill(used_locations_2_, used_locations_2_ + 18, false);
	std::fill(used_locations_3_, used_locations_3_ + 24, false);

	std::map<int, pathfind::paths> possible_moves2;
	std::multimap<int, map_location> srcdst2;
	std::multimap<map_location, int> dstsrc2; 
	calculate_possible_moves2(unit_locs, possible_moves2, srcdst2, dstsrc2, false, false, get_avoid());

	time_taken_cal = SDL_GetTicks() - ticks;

	for (unit_map::const_iterator j = units_.begin(); j != units_.end(); ++j) {
		unit& target = *dynamic_cast<unit*>(&*j);
		const map_location& candidate_loc = target.get_location();
		if (target.wall() && units_.find(candidate_loc).valid()) {
			// if has unit on it, cannot attack this wall.
			continue;
		}
		if (!point_in_rect(candidate_loc.x, candidate_loc.y, consider_enemy_rect)) {
			continue;
		}
		if (teams_[target.side() - 1].ea_artifical_neutral && hero::is_ea_artifical(target.packee_type()->master())) {
			continue;
		}
		// Attack anyone who is on the enemy side,
		// and who is not invisible or petrified.
		if (current_team_.is_enemy(target.side()) && !target.incapacitated() && !target.invisible(candidate_loc)) {
			attack_analysis analysis;
			analysis.target = &target;

			total_time_slow = 0;
			total_time_unmove = 0;
			total_time_move = 0;
			total_time_bonus = 0;
			total_time_analyze = 0;
			total_time_back = 0;

			total_time_stack = 0;


			ticks = SDL_GetTicks();

			do_attack_analysis(&target, unit_locs2, consider_size, dstsrc2, unit_locs, res, analysis);

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
}

void ai_default::do_attack_analysis(
	                 unit* target_ptr,
					 const std::vector<std::pair<unit*, int> >& units2,
	                 const size_t consider_size, const std::multimap<map_location, int>& dstsrc2,
					 std::vector<std::pair<unit*, int> >& units,
	                 std::vector<attack_analysis>& result,
					 attack_analysis& cur_analysis
	                )
{
	map_location* tiles;
	bool* used_locations;
	
	// This function is called fairly frequently, so interact with the user here.

	callerlayer ++;

	if (cur_analysis.target_dead || cur_analysis.movements.size() >= size_t(attack_depth_)) {
		callerlayer --;
		return;
	}
	team& target_team = teams_[target_ptr->side() - 1];

	const map_location& loc = target_ptr->get_location();
	
	const size_t max_positions = 1000;
	if (result.size() > max_positions && !cur_analysis.movements.empty()) {
		callerlayer --;
		return;
	}

	for (size_t i = 0; i != units.size(); ++i) {
		if (units.size() == units2.size() && i == consider_size) {
			break;
		}

		uint32_t ticks_slow = SDL_GetTicks();

		// curr_pair is colne of units[i], NOT be refrence! it may be erase.
		std::pair<unit*, int> curr_pair = units[i];
		unit* src_ptr = units[i].first;
		const map_location current_unit = src_ptr->get_location();

		if (target_ptr->side() == team::empty_side) {
			if (src_ptr->is_robber()) {
				continue;
			}
			if (src_ptr->cityno() == HEROS_ROAM_CITY && target_ptr->fort()) {
				continue;
			}
		}

		if (src_ptr->provoked_turns()) {
			if (target_ptr != find_provoke(src_ptr)) {
				continue;
			}
		}

		// See if the unit has the backstab ability.
		// Units with backstab will want to try to have a
		// friendly unit opposite the position they move to.
		//
		// See if the unit has the slow ability -- units with slow only attack first.
		bool slow = false;
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
		// 2.给个加3值的攻击距离(necessary!! attack maybe at best 3 grid.)
		if (!point_in_rect(loc.x, loc.y, src_ptr->attackable_rect())) {
			continue;
		}

		battle_context* bc = NULL;

		// This cache is only about 99% correct, but speeds up evaluation by about 1000 times.
		// We recalculate when we actually attack.
		std::map<std::pair<const unit*, const unit*>, battle_context*>::iterator usc;

		usc = unit_stats_cache.find(std::make_pair(target_ptr, src_ptr));
		// Just check this attack is valid for this attacking unit (may be modified)
		const battle_context::unit_stats* att;
		if (usc != unit_stats_cache.end()) {
			att = &usc->second->get_attacker_stats();
		} else {
			if (src_ptr->movement_left()) {
				bc = new battle_context(units_, *src_ptr, *target_ptr, -1, -1, aggression_, NULL);
			} else {
				int weapon = calculate_weapon(*src_ptr, *target_ptr);
				if (weapon == -1) {
					continue;
				}
				bc = new battle_context(units_, *src_ptr, *target_ptr, weapon, -1, aggression_, NULL);
			}
			att = &(bc->get_attacker_stats());

			unit_stats_cache.insert(std::make_pair(std::make_pair(target_ptr, src_ptr), bc));
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
			if (!src_ptr->movement_left()) {
				// to cannot movable unit, use simple calculate.
				if (curr_pair.second >= 0 || units_.find_unit(tiles[j], !src_ptr->base()) != units_.find_unit(current_unit, !src_ptr->base())) {
					total_time_unmove += SDL_GetTicks() - ticks_move;
					continue;
				} else {
					cur_position = j;
					break;
				}
			} else if (tiles[j] != current_unit) {
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
			if (units_.find_unit(tiles[cur_position], !src_ptr->base()) != units_.find_unit(src_ptr->get_location(), !src_ptr->base())) {
				cur_analysis.movements.push_back(std::make_pair(curr_pair, tiles[cur_position]));
			} else {
				cur_analysis.movements.push_back(std::make_pair(curr_pair, src_ptr->get_location()));
			}

			{
				attack_analysis_lock lock(cur_analysis);

				cur_analysis.analyze(map_, units_, unit_stats_cache);

				uint32_t end_analyze = SDL_GetTicks();
				total_time_analyze += end_analyze - ticks_analyze;

				result.push_back(cur_analysis);
				used_locations[cur_position] = true;

				total_time_stack += SDL_GetTicks() - end_analyze;

				do_attack_analysis(target_ptr, units2, consider_size, dstsrc2, units, result, cur_analysis);
			
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
	const int wall_value = 150;

	if (map_.gives_healing(terrain) && u.get_ability_bool("regenerates",loc) == false) {
		rating += healing_value;
	}

	unit* base = units_.find_unit(loc, false);
	if (base && base->wall()) {
		rating += wall_value;
	}

	return rating;
}

} //end of namespace ai

