/* $Id: attack.cpp 37456 2009-08-03 22:03:13Z crab $ */
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
 * @file ai/default/attack.cpp
 * Calculate & analyse attacks of the default ai
 */

#include "../../global.hpp"

#include "ai.hpp"
#include "../manager.hpp"

#include "../../attack_prediction.hpp"
#include "foreach.hpp"
#include "../../map.hpp"
#include "../../log.hpp"
#include "artifical.hpp"
#include "wml_exception.hpp"
#include "gettext.hpp"

namespace ai {

attack_analysis::attack_analysis() :
	target(),
	movements(),
	target_value(0.0),
	target_starting_damage(0),
	avg_damage_inflicted(0.0),
	target_dead(false),
	resources_used(0.0),
	terrain_quality(0.0),
	avg_losses(0.0),
	chance_to_kill(0.0),
	def_avg_experience(0.0),
	prob_dead_already(0.0)
{
}

attack_analysis::attack_analysis(const attack_analysis& that) :
	target(that.target),
	movements(that.movements),
	target_value(that.target_value),
	target_starting_damage(that.target_starting_damage),
	avg_damage_inflicted(that.avg_damage_inflicted),
	target_dead(that.target_dead),
	resources_used(that.resources_used),
	terrain_quality(that.terrain_quality),
	avg_losses(that.avg_losses),
	chance_to_kill(that.chance_to_kill),
	def_avg_experience(0.0),
	prob_dead_already(0.0)
{
}

attack_analysis::~attack_analysis()
{
}

attack_analysis& attack_analysis::operator=(const attack_analysis& that)
{
	target = that.target;
	movements = that.movements;
	target_value = that.target_value;
	target_starting_damage = that.target_starting_damage;
	avg_damage_inflicted = that.avg_damage_inflicted;
	target_dead = that.target_dead;
	resources_used = that.resources_used;
	terrain_quality = that.terrain_quality;
	avg_losses = that.avg_losses;
	chance_to_kill = that.chance_to_kill;

	return *this;
}

void attack_analysis::analyze(const gamemap& map, unit_map& units,
				std::map<std::pair<const unit*, const unit*>, battle_context*>& unit_stats_cache,
				const std::vector<std::pair<unit*, int> >& units2,
				const std::multimap<map_location, int>& srcdst2, const std::multimap<int, map_location>& dstsrc2)
{
	const int target_cost = target->cost();
	target_value = target_cost;
	target_value += 0.5 * (double(target->experience()) / double(target->max_experience())) * target_value;
	target_starting_damage = target->max_hitpoints() - target->hitpoints();

	VALIDATE(!movements.empty(), _("ai::attack_analisis::analyze, movements is empty!"));
	
	std::pair<std::pair<unit*, int>, map_location>& m = movements.back();
	map_location orig_loc = m.first.first->get_location();

	if (m.first.second >= 0 && m.first.first->get_location() == m.second) {
		throw game::game_error(std::string("ai::attack_analisis::analyze, reside troop(") +  m.first.first->name() + ") mustnot at city!");
	}

	// see remark#42. why up is point and not object.
	std::pair<map_location, unit*>* up;
	std::pair<map_location, unit*> up_reside;

	if (m.first.second < 0) {
		// We fix up units map to reflect what this would look like.
		if (m.second == orig_loc) {
			up = units.get_cookie(m.second, !m.first.first->base());
		} else {
			up = units.extract(orig_loc);
			up->first = m.second;
			units.place(up);
		}
	} else {
		up_reside = std::make_pair(m.second, m.first.first);
		up = &up_reside;
		units.place(up);
	}

	unit_map::node* base = units.get_cookie(m.second, false);
	bool on_wall = base && base->second->wall();

	int att_weapon = -1, def_weapon = -1;
	bool from_cache = false;
	battle_context *bc;
	const unit* src_ptr = up->second;

	// This cache is only about 99% correct, but speeds up evaluation by about 1000 times.
	// We recalculate when we actually attack.
	std::map<std::pair<const unit*, const unit*>, battle_context*>::iterator usc;
	usc = unit_stats_cache.find(std::pair<const unit*, const unit*>(target, src_ptr));
	// Just check this attack is valid for this attacking unit (may be modified)
	if (usc != unit_stats_cache.end() && usc->second->get_attacker_stats().attack_num < static_cast<int>(src_ptr->attacks().size())) {
		from_cache = true;
		bc = usc->second;
	} else {
		VALIDATE(false, _("ai::attack_analisis::analyze, cannot find <target, src> pair in usc."));
	}

	// because save battle_context into usc direct, don't support prev_def.
	const combatant &att = bc->get_attacker_combatant(NULL);
	const combatant &def = bc->get_defender_combatant(NULL);

	target_dead = def.dead_ >= 1;

	// Note we didn't fight at all if defender is already dead.
	double cost = up->second->cost();
	const bool on_village = map.is_village(m.second);
	// Up to double the value of a unit based on experience
	cost += 0.5 * (double(up->second->experience())/double(up->second->max_experience())) * cost;
	
	// We should encourage multi-phase attack.
	resources_used += cost / movements.size();
	avg_losses += cost * att.dead_;

	// add factor of slow/break
	if (bc->get_attacker_stats().slows && !target->get_state(unit::STATE_SLOWED)) {
		chance_to_kill += 0.06 / movements.size();
	}
	if (bc->get_attacker_stats().breaks && !target->get_state(unit::STATE_BROKEN)) {
		chance_to_kill += 0.04 / movements.size();
	}

	// terrain factor
	double quality = (double(bc->get_defender_stats().chance_to_hit) / 100.0) * cost;
	if (on_village) {
		quality *= 0.8;
	}
	if (on_wall) {
		quality *= 0.5;
	}
	terrain_quality += quality;

	//
	// defender maybe advance, first calculate defneder, second attacker.
	// For each level of attacker def gets 1 xp or
	// kill_experience.
	//
	int fight_xp = game_config::attack_xp(up->second->level());
	int kill_xp = game_config::kill_xp(up->second->level());
	def_avg_experience += att.dead_ >= 1? kill_xp: fight_xp;
	
	if (!target_dead && target->can_advance() && def_avg_experience >= target->max_experience() - target->experience()) {
		// It's likely to advance: only if we can kill with first blow.
		chance_to_kill = 0;
		target_value -= 0.3 * target_cost;
	} else {
		double dmage = 1.0 * bc->get_attacker_stats().num_blows * bc->get_attacker_stats().damage / movements.size();
		chance_to_kill += dmage / target->max_hitpoints();
	}

	// The reward for advancing a unit is to get a 'negative' loss of that unit
	if (up->second->can_advance()) {
		int xp_for_advance = up->second->max_experience() - up->second->experience();
		int kill_xp, fight_xp;

		// See bug #6272... in some cases, unit already has got enough xp to advance,
		// but hasn't (bug elsewhere?).  Can cause divide by zero.
		if (xp_for_advance <= 0) {
			xp_for_advance = 1;
		}

		fight_xp = game_config::attack_xp(target->level());
		kill_xp = game_config::kill_xp(target->level());

		if (fight_xp >= xp_for_advance) {
			avg_losses -= cost * 0.3;
		} else if (chance_to_kill >= 1) {
			if (kill_xp >= xp_for_advance) {
				avg_losses -= cost * 0.3;
			} else {
				avg_losses -= 0.5 * (kill_xp / double(up->second->max_experience())) * cost;
			}
		} else {
			avg_losses -= 0.5 * (fight_xp / double(up->second->max_experience())) * cost;
		}
	}

	// Restore the units to their original positions.
	if (m.first.second < 0) {
		if (m.second != orig_loc) {
			units.move(m.second, orig_loc);
		}
	} else {
		units.extract(m.second);
		// set_location() will update loc_ of up->second, it is time to be back to orig_loc.
		up->second->set_location(orig_loc);
	}
}

double attack_analysis::rating(double aggression) const
{
	double adjusted_target_value = target_value;
	if (target->is_city()) {
		adjusted_target_value += 2 * target_value;
	} else if (target->type()->master() == hero::number_keep) {
		adjusted_target_value += target_value;;
	} else if (target->type()->master() == hero::number_tactic) {
		adjusted_target_value += target_value;;
	}

	//FIXME: One of suokko's reverted changes removed this.  Should it be gone?
	// Only use the leader if we do a serious amount of damage,
	// compared to how much they do to us.
	double value = chance_to_kill * adjusted_target_value - avg_losses * (1.0 - aggression);
	value /= (resources_used + 2 * terrain_quality) / 3;

	// ensure provoked must attack provoke.
	if (movements.size() == 1 && movements.front().first.second == -1) {
		unit* u = movements.front().first.first;
		if (u->provoked_turns() && find_provoke(u) == target) {
			if (value < 0.01) {
				value = 0.01;
			}
		}
	}

	return value;
}

attack_analysis_lock::attack_analysis_lock(attack_analysis& analysis)
	: analysis_(analysis)
	, chance_to_kill_(analysis.chance_to_kill)
	, avg_losses_(analysis.avg_losses)
	, avg_damage_inflicted_(analysis.avg_damage_inflicted)
	, target_dead_(analysis.target_dead)
	, resources_used_(analysis.resources_used)
	, terrain_quality_(analysis.terrain_quality)
	, def_avg_experience_(analysis.def_avg_experience)
	, prob_dead_already_(analysis.prob_dead_already)
{
}

attack_analysis_lock::~attack_analysis_lock()
{
	analysis_.chance_to_kill = chance_to_kill_;
	analysis_.avg_losses = avg_losses_;
	analysis_.avg_damage_inflicted = avg_damage_inflicted_;
	analysis_.target_dead = target_dead_;
	analysis_.resources_used = resources_used_;
	analysis_.terrain_quality = terrain_quality_;
	analysis_.def_avg_experience = def_avg_experience_;
	analysis_.prob_dead_already = prob_dead_already_;
}

} //end of namespace ai
