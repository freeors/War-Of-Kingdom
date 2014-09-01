/* $Id: contexts.hpp 37713 2009-08-13 18:02:19Z crab $ */
/*
   Copyright (C) 2009 by Yurii Chernyi <terraninfo@terraninfo.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 * @file ai/default/attack.hpp
 * Default AI contexts
 */

#ifndef AI_DEFAULT_ATTACK_HPP_INCLUDED
#define AI_DEFAULT_ATTACK_HPP_INCLUDED

#include "../../global.hpp"

#include "../game_info.hpp"
#include <vector>


#ifdef _MSC_VER
#pragma warning(push)
//silence "inherits via dominance" warnings
#pragma warning(disable:4250)
#endif



//============================================================================
namespace ai {

class attack_analysis
{
public:
	attack_analysis();
	attack_analysis(const attack_analysis& that);

	~attack_analysis();

	attack_analysis& operator=(const attack_analysis& that);

	void analyze(const gamemap& map, unit_map& units,
				std::map<std::pair<const unit*, const unit*>, battle_context*>& unit_stats_cache);

	double rating(double aggression, const map_location& guard_loc) const;

	unit* target;
	std::vector<std::pair<std::pair<unit*, int>, map_location> > movements;

	/** The value of the unit being targeted. */
	double target_value;

	/** The value on average, of units lost in the combat. */
	double avg_losses;

	/** Estimated % chance to kill the unit. */
	double chance_to_kill;

	/** The average hitpoints damage inflicted. */
	double avg_damage_inflicted;

	int target_starting_damage;

	/** whether target had dead or not. */
	bool target_dead;

	/** The sum of the values of units used in the attack. */
	double resources_used;

	/** The weighted average of the % chance to hit each attacking unit. */
	double terrain_quality;

	// temp variable for calculate
	double def_avg_experience;
	double prob_dead_already;
};

class attack_analysis_lock
{
public:
	attack_analysis_lock(attack_analysis& analysis);
	~attack_analysis_lock();

private:
	attack_analysis& analysis_;
	double chance_to_kill_;
	double avg_losses_;
	double avg_damage_inflicted_;
	double target_dead_;
	double resources_used_;
	double terrain_quality_;
	double def_avg_experience_;
	double prob_dead_already_;
};

} //end of namespace ai

#ifdef _MSC_VER
#pragma warning(pop)
#endif


#endif
