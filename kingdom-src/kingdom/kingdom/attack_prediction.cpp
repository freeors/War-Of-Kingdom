/* $Id: attack_prediction.cpp 40500 2010-01-01 17:11:52Z mordante $ */
/*
   Copyright (C) 2006 - 2010 by Rusty Russell <rusty@rustcorp.com.au>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.

   Full algorithm by Yogin.  Typing and optimization by Rusty.

   This code has lots of debugging.  It is there for a reason:
   this code is kinda tricky.  Do not remove it.
*/

/**
 * @file attack_prediction.cpp
 * Simulate combat to calculate attacks. Standalone program.
 */

#include "attack_prediction.hpp"
#include "game_config.hpp"

combatant::combatant(const battle_context::unit_stats &u, const combatant *prev)
	: untouched(0.0),
	  poisoned(0.0),
	  slowed(0.0),
	  u_(u),
	  hit_chances_(u.num_blows, u.chance_to_hit / 100.0)
{
	// We inherit current state from previous combatant.
	if (prev) {
		hp_ = prev->hp_;
		dead_ = prev->dead_;
		poisoned = prev->poisoned;
		untouched = prev->untouched;
		slowed = prev->slowed;
	} else {
		hp_ = u_.hp;
		dead_ = hp_? 0: 1;
		untouched = 1.0;
		poisoned = u.is_poisoned ? 1.0 : 0.0;
		slowed = u.is_slowed ? 1.0 : 0.0;
	}
}

// Copy constructor (except use this copy of unit_stats)
combatant::combatant(const combatant &that, const battle_context::unit_stats &u)
	: hp_(that.hp_), dead_(that.dead_), untouched(that.untouched), poisoned(that.poisoned), slowed(that.slowed), u_(u), hit_chances_(that.hit_chances_)
{
}

void combatant::consider_levelup(combatant &opp)
{
	// adjust the probabilities to take into consideration the
	// probability of us levelling up, and hence healing.
	// We do not currently estimate how many HP we will have.
	int get_experientcce_per_level = (opp.dead_ == 1)? game_config::kill_experience: 1;

	if (u_.experience + (opp.u_.level * get_experientcce_per_level) >= u_.max_experience) {
		// if we survive the combat, we will level up. So the probability
		// of death is unchanged, but all other cases get merged into
		// the fully healed case.
		hp_ = u_.max_hp;
	}
}

// Two man enter.  One man leave!
// ... Or maybe two.  But definitely not three.
// Of course, one could be a woman.  Or both.
// And neither could be human, too.
// Um, ok, it was a stupid thing to say.
void combatant::fight(combatant &opp, bool levelup_considered)
{
	unsigned int rounds = std::max<unsigned int>(u_.rounds, opp.u_.rounds);

	// If defender has firststrike and we don't, reverse.
	if (opp.u_.firststrike && !u_.firststrike) {
		opp.fight(*this, levelup_considered);
		return;
	}

	double orig_hp = hp_;
	double orig_opp_hp = opp.hp_;
	// 非常简单攻击：剩余HP = 原HP - 被攻击次数*每次攻击造成的损伤
	hp_ = std::max<double>(hp_ - opp.u_.num_blows * opp.u_.damage, 0);
	opp.hp_ = std::max<double>(opp.hp_ - u_.num_blows * u_.damage, 0);

	// dead_ = hp_? 0: 1;
	// opp.dead_ = opp.hp_? 0: 1;
	// orig_hp or orig_opp_hp may be 0, should avoid div 0!
	dead_ = orig_hp? (orig_hp - hp_) / orig_hp: 1;
	opp.dead_ = orig_opp_hp? (orig_opp_hp - opp.hp_) / orig_opp_hp: 1;

	if (levelup_considered) {
		consider_levelup(opp);
		opp.consider_levelup(*this);
	}

	// 计算中毒和被减速概率
	// 强制认为每次攻击造成中素或减速概率是50%，将来这个概率可能是由mod可改写
	if (opp.u_.poisons) {
		poisoned += (1 - poisoned) * 0.5;
	}
	if (u_.poisons) {
		opp.poisoned += (1 - opp.poisoned) * 0.5;
	}

	if (opp.u_.slows) {
		slowed += (1 - slowed) * 0.5;
	}
	if (u_.slows) {
		opp.slowed += (1 - opp.slowed) * 0.5;
	}
}

double combatant::average_hp(unsigned int healing) const
{
	return std::min<double>(hp_ + healing, u_.max_hp);
}

