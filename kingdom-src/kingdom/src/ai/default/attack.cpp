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
	avg_damage_taken(0.0),
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
	avg_damage_taken(that.avg_damage_taken),
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
	avg_damage_taken = that.avg_damage_taken;
	resources_used = that.resources_used;
	terrain_quality = that.terrain_quality;
	avg_losses = that.avg_losses;
	chance_to_kill = that.chance_to_kill;

	return *this;
}

void attack_analysis::analyze(const gamemap& map, unit_map& units,
				std::map<std::pair<const unit*, const unit_type*>, battle_context*>& unit_stats_cache,
				const std::vector<std::pair<unit*, int> >& units2,
				const std::multimap<map_location, int>& srcdst2, const std::multimap<int, map_location>& dstsrc2,
				std::map<unit*, std::pair<map_location, unit*>* >& reside_cache,
				double aggression)
{
	target_value = target->cost();
	target_value += 0.5 * (double(target->experience()) / double(target->max_experience())) * target_value;
	target_starting_damage = target->max_hitpoints() - target->hitpoints();

	VALIDATE(!movements.empty(), _("ai::attack_analisis::analyze, movements is empty."));

	std::pair<std::pair<unit*, int>, map_location>& m = movements.back();
	map_location orig_loc = m.first.first->get_location();

	std::pair<map_location, unit*>* up;
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
		// units.add(m.second, m.first.first);
		std::map<unit*, std::pair<map_location, unit*>* >::iterator cache_itor = reside_cache.find(m.first.first);
		if (cache_itor == reside_cache.end()) {
			up = new std::pair<map_location, unit*>(m.second, m.first.first);
			reside_cache[m.first.first] = up;
		} else {
			up = cache_itor->second;
		}
		up->first = m.second;
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
		const unit_type* src_type;
		if (!src_ptr->packed()) {
			src_type = src_ptr->type();
		} else {
			src_type = unit_types.find(src_ptr->packee_type_id());
		}

		std::map<std::pair<const unit*, const unit_type*>, battle_context*>::iterator usc;
		usc = unit_stats_cache.find(std::pair<const unit*, const unit_type*>(target, src_type));
		// Just check this attack is valid for this attacking unit (may be modified)
		if (usc != unit_stats_cache.end() && usc->second->get_attacker_stats().attack_num < static_cast<int>(src_ptr->attacks().size())) {
			from_cache = true;
			bc = usc->second;
		} else {
			VALIDATE(false, _("ai::attack_analisis::analyze, cannot find unit_type pair in usc."));
		}

		// because save battle_context into usc direct, don't support prev_def.
		const combatant &att = bc->get_attacker_combatant(NULL);
		const combatant &def = bc->get_defender_combatant(NULL);

		// Note we didn't fight at all if defender is already dead.
		double prob_fought = (1.0 - prob_dead_already);

		// @todo 1.8 add combatant.prob_killed
		double prob_killed = def.dead_ - prob_dead_already;
		prob_dead_already = def.dead_;

		double prob_died = att.dead_;
		double prob_survived = (1.0 - prob_died) * prob_fought;

		double cost = up->second->cost();
		const bool on_village = map.is_village(m.second);
		// Up to double the value of a unit based on experience
		cost += 0.5 * (double(up->second->experience())/double(up->second->max_experience())) * cost;
		// We should encourage multi-phase attack.
		cost /= movements.size();

		resources_used += cost;
		avg_losses += cost * prob_died;

		// add half of cost for poisoned unit so it might get chance to heal
		avg_losses += cost * up->second->get_state(unit::STATE_POISONED) /2;

		// Double reward to emphasize getting onto villages if they survive.
		if (on_village) {
			avg_damage_taken -= game_config::poison_amount*2 * prob_survived;
		}

		double quality = (double(bc->get_defender_stats().chance_to_hit)/100.0) * cost * (on_village ? 0.5 : 1.0);
		if (on_wall) {
			quality *= 0.2;
		}
		terrain_quality += quality;

		double advance_prob = 0.0;
		// The reward for advancing a unit is to get a 'negative' loss of that unit
		if (!up->second->advances_to().empty()) {
			int xp_for_advance = up->second->max_experience() - up->second->experience();
			int kill_xp, fight_xp;

			// See bug #6272... in some cases, unit already has got enough xp to advance,
			// but hasn't (bug elsewhere?).  Can cause divide by zero.
			if (xp_for_advance <= 0)
				xp_for_advance = 1;

			fight_xp = target->level();
			kill_xp = fight_xp ? fight_xp * game_config::kill_experience :
				game_config::kill_experience / 2;

			if (fight_xp >= xp_for_advance) {
				advance_prob = prob_fought;
				avg_losses -= up->second->cost() * prob_fought;
			} else if (kill_xp >= xp_for_advance) {
				advance_prob = prob_killed;
				avg_losses -= up->second->cost() * prob_killed;
				// The reward for getting a unit closer to advancement
				// (if it didn't advance) is to get the proportion of
				// remaining experience needed, and multiply it by
				// a quarter of the unit cost.
				// This will cause the AI to heavily favor
				// getting xp for close-to-advance units.
				avg_losses -= up->second->cost() * 0.25 *
					fight_xp * (prob_fought - prob_killed)
					/ xp_for_advance;
			} else {
				avg_losses -= up->second->cost() * 0.25 *
					(kill_xp * prob_killed + fight_xp * (prob_fought - prob_killed))
					/ xp_for_advance;
			}

			// The reward for killing with a unit that plagues
			// is to get a 'negative' loss of that unit.
			if (bc->get_attacker_stats().plagues) {
				avg_losses -= prob_killed * up->second->cost();
			}
		}

		// If we didn't advance, we took this damage.
		avg_damage_taken += (up->second->hitpoints() - att.average_hp()) * (1.0 - advance_prob);

		//
		// @todo 1.8: attack_prediction.cpp should understand advancement
		// directly.  For each level of attacker def gets 1 xp or
		// kill_experience.
		//
		int fight_xp = up->second->level();
		int kill_xp = fight_xp ? fight_xp * game_config::kill_experience :
				game_config::kill_experience / 2;
		def_avg_experience += fight_xp * (1.0 - att.dead_) + kill_xp * att.dead_;
	

	if (!target->advances_to().empty() && def_avg_experience >= target->max_experience() - target->experience()) {
		// It's likely to advance: only if we can kill with first blow.
		if (movements.size() == 1) {
			chance_to_kill = def.dead_;
		}
		// Negative average damage (it will advance).
		avg_damage_inflicted += target->hitpoints() - target->max_hitpoints();
	} else {
		chance_to_kill += def.dead_;
		avg_damage_inflicted += target->hitpoints() - def.average_hp(map.gives_healing(target->get_location()));
	}

	// Restore the units to their original positions.
	if (m.first.second < 0) {
		if (m.second != orig_loc) {
			units.move(m.second, orig_loc);
		}
	} else {
		units.extract(m.second);
		// place() will update loc_ of up->second, it is time to be back to orig_loc.
		up->second->set_location(orig_loc);
		// delete up;
	}
}

double attack_analysis::rating(double aggression) const
{
	//FIXME: One of suokko's reverted changes removed this.  Should it be gone?
	// Only use the leader if we do a serious amount of damage,
	// compared to how much they do to us.
	double value = chance_to_kill*target_value - avg_losses*(1.0-aggression);

	// Prefer to attack already damaged targets.
	value += ((target_starting_damage/3 + avg_damage_inflicted) - (1.0-aggression)*avg_damage_taken)/10.0;

	value /= (resources_used + 2 * terrain_quality) / 3;

	if (target->is_city()) {
		value += 0.3;
	} else if (target->type() == unit_types.find_keep()) {
		value += 0.15;
	}

	return value;
}

attack_analysis_lock::attack_analysis_lock(attack_analysis& analysis)
	: analysis_(analysis)
	, chance_to_kill_(analysis.chance_to_kill)
	, avg_losses_(analysis.avg_losses)
	, avg_damage_inflicted_(analysis.avg_damage_inflicted)
	, avg_damage_taken_(analysis.avg_damage_taken)
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
	analysis_.avg_damage_taken = avg_damage_taken_;
	analysis_.resources_used = resources_used_;
	analysis_.terrain_quality = terrain_quality_;
	analysis_.def_avg_experience = def_avg_experience_;
	analysis_.prob_dead_already = prob_dead_already_;
}

} //end of namespace ai
