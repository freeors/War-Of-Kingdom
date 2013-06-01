/* $Id: actions.cpp 44617 2010-07-24 07:24:27Z silene $ */
/*
   Copyright (C) 2003 - 2010 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 * @file src/actions.cpp
 * Recruiting, Fighting.
 */

#include "attack_prediction.hpp"
#include "foreach.hpp"
#include "game_display.hpp"
#include "game_end_exceptions.hpp"
#include "game_events.hpp"
#include "game_preferences.hpp"
#include "gettext.hpp"
#include "hotkeys.hpp"
#include "log.hpp"
#include "map.hpp"
#include "map_label.hpp"
#include "mouse_handler_base.hpp"
#include "pathfind/pathfind.hpp"
#include "replay.hpp"
#include "resources.hpp"
#include "statistics.hpp"
#include "unit_abilities.hpp"
#include "unit_display.hpp"
#include "wml_exception.hpp"
#include "formula_string_utils.hpp"
#include "tod_manager.hpp"
#include "artifical.hpp"
#include "actions.hpp"
#include "ai/manager.hpp"
#include "dialogs.hpp"
#include "play_controller.hpp"
#include "gui/dialogs/duel.hpp"

#include <boost/bind.hpp>
#include <boost/scoped_ptr.hpp>
#include <list>


static lg::log_domain log_engine("engine");
#define DBG_NG LOG_STREAM(debug, log_engine)
#define LOG_NG LOG_STREAM(info, log_engine)
#define ERR_NG LOG_STREAM(err, log_engine)

static lg::log_domain log_config("config");
#define LOG_CF LOG_STREAM(info, log_config)

static lg::log_domain log_ai_testing("ai/testing");
#define LOG_AI_TESTING LOG_STREAM(info, log_ai_testing)

struct castle_cost_calculator : pathfind::cost_calculator
{
	castle_cost_calculator(const gamemap& map) : map_(map)
	{}

	virtual double cost(const map_location& loc, const double) const
	{
		if(!map_.is_castle(loc))
			return 10000;

		return 1;
	}

private:
	const gamemap& map_;
};

void move_unit_spectator::add_seen_friend(const unit_map::const_iterator &u)
{
	seen_friends_.push_back(u);
}


void move_unit_spectator::add_seen_enemy(const unit_map::const_iterator &u)
{
	seen_enemies_.push_back(u);
}


const unit_map::const_iterator& move_unit_spectator::get_ambusher() const
{
	return ambusher_;
}


const unit_map::const_iterator& move_unit_spectator::get_failed_teleport() const
{
	return failed_teleport_;
}


const std::vector<unit_map::const_iterator>& move_unit_spectator::get_seen_enemies() const
{
	return seen_enemies_;
}


const std::vector<unit_map::const_iterator>& move_unit_spectator::get_seen_friends() const
{
	return seen_friends_;
}

const std::vector<map_location>& move_unit_spectator::get_interrupted_waypoints() const
{
	return interrupted_waypoints_;
}

const unit_map::const_iterator& move_unit_spectator::get_unit() const
{
	return unit_;
}


move_unit_spectator::move_unit_spectator(const unit_map &units)
	: ambusher_(units.end()), failed_teleport_(units.end()), seen_enemies_(), seen_friends_(), interrupted_waypoints_(), unit_(units.end())
{
}


move_unit_spectator::~move_unit_spectator()
{
}

void move_unit_spectator::reset(const unit_map &units)
{
	ambusher_ = units.end();
	failed_teleport_ = units.end();
	seen_enemies_.clear();
	seen_friends_.clear();
	interrupted_waypoints_.clear();
	unit_ = units.end();
}


void move_unit_spectator::set_ambusher(const unit_map::const_iterator &u)
{
	ambusher_ = u;
}


void move_unit_spectator::set_failed_teleport(const unit_map::const_iterator &u)
{
	failed_teleport_ = u;
}

void move_unit_spectator::set_interrupted_waypoints(const std::vector<map_location>& waypoints)
{
	interrupted_waypoints_ = waypoints;
}

void move_unit_spectator::set_unit(const unit_map::const_iterator &u)
{
	unit_ = u;
}

map_location under_leadership(const unit_map& units,
		const map_location& loc, int* bonus)
{

	const unit_map::const_iterator un = units.find(loc);
	if(un == units.end()) {
		return map_location::null_location;
	}
	unit_ability_list abil = un->get_abilities("leadership");
	if(bonus) {
		*bonus = abil.highest("value").first;
	}
	return abil.highest("value").second;
}

battle_context::battle_context(const unit_map &units,
		const unit& attacker, const unit& defender,
		int attacker_weapon, int defender_weapon)
: attacker_stats_(NULL), defender_stats_(NULL), attacker_combatant_(NULL), defender_combatant_(NULL)
{
	const map_location& attacker_loc = attacker.get_location();
	const map_location& defender_loc = defender.get_location();
	// If those didn't have to generate statistics, do so now.
	const attack_type *adef = NULL;
	const attack_type *ddef = NULL;
	if (attacker_weapon >= 0) {
		VALIDATE(attacker_weapon < static_cast<int>(attacker.attacks().size()),
				_("An invalid attacker weapon got selected."));
		adef = &attacker.attacks()[attacker_weapon];
	}
	if (defender_weapon >= 0) {
		VALIDATE(defender_weapon < static_cast<int>(defender.attacks().size()),
				_("An invalid defender weapon got selected."));
		ddef = &defender.attacks()[defender_weapon];
	}
	assert(!defender_stats_ && !attacker_combatant_ && !defender_combatant_);
	attacker_stats_ = new unit_stats(attacker, attacker_loc, attacker_weapon,
			true, defender, defender_loc, ddef, units);
	defender_stats_ = new unit_stats(defender, defender_loc, defender_weapon, false,
			attacker, attacker_loc, adef, units);
}

map_location best_attacker_location_from_range(const unit_map& units_, const unit& u, const std::string& range, const map_location& def)
{
	const map_location* tiles;
	size_t adjacent_size;

	if (range == "melee") {
		// range: 1
		tiles = u.adjacent_;
		adjacent_size = u.adjacent_size_;
	} else if (range == "ranged") {
		// range: 2
		tiles = u.adjacent_2_;
		adjacent_size = u.adjacent_size_2_;
	} else if (range == "cast") {
		// other to range: 3
		tiles = u.adjacent_3_;
		adjacent_size = u.adjacent_size_3_;
	} else {
		VALIDATE(false, _("do_attack_analysis, unknown range."));
	}
	for (int i = 0; i != adjacent_size; ++i) {
		unit_map::node* base = units_.get_cookie(tiles[i], false);
		unit_map::node* overlay = units_.get_cookie(tiles[i]);
		if (!overlay && base && base->second->wall()) {
			return tiles[i];
		}
	}
	return def;
}

battle_context::battle_context(const unit_map& units,
		const unit& attacker, const unit& defender,
		int attacker_weapon, int defender_weapon, double aggression, const combatant *prev_def)
: attacker_stats_(NULL), defender_stats_(NULL), attacker_combatant_(NULL), defender_combatant_(NULL)
{
	const double harm_weight = 1.0 - aggression;
	const map_location& attacker_loc = attacker.get_location();
	const map_location& defender_loc = defender.get_location();

	if (attacker_weapon == -1 && attacker.attacks().size() == 1 && attacker.attacks()[0].attack_weight() > 0 )
		attacker_weapon = 0;

	if (attacker_weapon == -1) {
		attacker_weapon = choose_attacker_weapon(attacker, defender, units,
				harm_weight, &defender_weapon, prev_def);
	} else if (defender_weapon == -1) {
		defender_weapon = choose_defender_weapon(attacker, defender, attacker_weapon,
			units, attacker_loc, defender_loc, prev_def);
	}

	// If those didn't have to generate statistics, do so now.
	if (!attacker_stats_) {
		const attack_type *adef = NULL;
		const attack_type *ddef = NULL;
		if (attacker_weapon >= 0) {
			VALIDATE(attacker_weapon < static_cast<int>(attacker.attacks().size()),
					_("An invalid attacker weapon got selected."));
			adef = &attacker.attacks()[attacker_weapon];
		}
		if (defender_weapon >= 0) {
			VALIDATE(defender_weapon < static_cast<int>(defender.attacks().size()),
					_("An invalid defender weapon got selected."));
			ddef = &defender.attacks()[defender_weapon];
		}
		assert(!defender_stats_ && !attacker_combatant_ && !defender_combatant_);
		attacker_stats_ = new unit_stats(attacker, attacker_loc, attacker_weapon,
				true, defender, defender_loc, ddef, units);
		defender_stats_ = new unit_stats(defender, defender_loc, defender_weapon, false,
				attacker, attacker_loc, adef, units);
	}
}

battle_context::battle_context(const battle_context &other)
: attacker_stats_(NULL), defender_stats_(NULL), attacker_combatant_(NULL), defender_combatant_(NULL)
{
	*this = other;
}

battle_context::battle_context(const unit_stats &att, const unit_stats &def) :
	attacker_stats_(new unit_stats(att)),
	defender_stats_(new unit_stats(def)),
	attacker_combatant_(0),
	defender_combatant_(0)
{
}

battle_context::~battle_context()
{
	delete attacker_stats_;
	delete defender_stats_;
	delete attacker_combatant_;
	delete defender_combatant_;
}
battle_context& battle_context::operator=(const battle_context &other)
{
	if (&other != this) {
		delete attacker_stats_;
		delete defender_stats_;
		delete attacker_combatant_;
		delete defender_combatant_;
		attacker_stats_ = new unit_stats(*other.attacker_stats_);
		defender_stats_ = new unit_stats(*other.defender_stats_);
		attacker_combatant_ = other.attacker_combatant_ ? new combatant(*other.attacker_combatant_, *attacker_stats_) : NULL;
		defender_combatant_ = other.defender_combatant_ ? new combatant(*other.defender_combatant_, *defender_stats_) : NULL;
	}
	return *this;
}

/** @todo FIXME: Hand previous defender unit in here. */
int battle_context::choose_defender_weapon(const unit &attacker, const unit &defender, unsigned attacker_weapon,
	const unit_map& units,
		const map_location& attacker_loc, const map_location& defender_loc,
		const combatant *prev_def)
{
	VALIDATE(attacker_weapon < attacker.attacks().size(),
			_("An invalid attacker weapon got selected."));
	const attack_type &att = attacker.attacks()[attacker_weapon];
	std::vector<unsigned int> choices;

	// What options does defender have?
	unsigned int i;
	for (i = 0; i < defender.attacks().size(); ++i) {
		const attack_type &def = defender.attacks()[i];
		if (def.range() == att.range() && def.defense_weight() > 0) {
			choices.push_back(i);
		}
	}
	if (choices.empty())
		return -1;
	if (choices.size() == 1)
		return choices[0];

	// Multiple options:
	// First pass : get the best weight and the minimum simple rating for this weight.
	// simple rating = number of blows * damage per blows (resistance taken in account) * cth * weight
	// Elligible attacks for defense should have a simple rating greater or equal to this weight.

	int min_rating = 0;

	// Multiple options: simulate them, save best.
	for (i = 0; i < choices.size(); ++i) {
		const attack_type &def = defender.attacks()[choices[i]];
		unit_stats *att_stats = new unit_stats(attacker, attacker_loc, attacker_weapon,
				true, defender, defender_loc, &def, units);
		unit_stats *def_stats = new unit_stats(defender, defender_loc, choices[i], false,
				attacker, attacker_loc, &att, units);

		combatant *att_comb = new combatant(*att_stats);
		combatant *def_comb = new combatant(*def_stats, prev_def);
		att_comb->fight(*def_comb);

		int simple_rating = static_cast<int>(def_stats->num_blows *
				def_stats->damage * def_stats->chance_to_hit * def.defense_weight());

		if (simple_rating >= min_rating && (!attacker_combatant_ || better_combat(*def_comb, *att_comb, *defender_combatant_, *attacker_combatant_, 1.0))) {
			delete attacker_combatant_;
			delete defender_combatant_;
			delete attacker_stats_;
			delete defender_stats_;
			attacker_combatant_ = att_comb;
			defender_combatant_ = def_comb;
			attacker_stats_ = att_stats;
			defender_stats_ = def_stats;
		} else {
			delete att_comb;
			delete def_comb;
			delete att_stats;
			delete def_stats;
		}
	}

	return defender_stats_->attack_num;
}

int battle_context::choose_attacker_weapon(const unit &attacker, const unit &defender,
	const unit_map& units, double harm_weight, int *defender_weapon, const combatant *prev_def)
{
	const map_location& defender_loc = defender.get_location();
	std::vector<unsigned int> choices;

	// What options does attacker have?
	unsigned int i;
	for (i = 0; i < attacker.attacks().size(); ++i) {
		const attack_type &att = attacker.attacks()[i];
		if (att.attack_weight() > 0) {
			choices.push_back(i);
		}
	}
	if (choices.empty())
		return -1;
	if (choices.size() == 1) {
		*defender_weapon = choose_defender_weapon(attacker, defender, choices[0], units,
			best_attacker_location_from_range(units, defender, attacker.attacks()[choices[0]].range(), attacker.get_location()),
			defender_loc, prev_def);
		return choices[0];
	}

	// Multiple options: simulate them, save best.
	unit_stats *best_att_stats = NULL, *best_def_stats = NULL;
	combatant *best_att_comb = NULL, *best_def_comb = NULL;

	for (i = 0; i < choices.size(); ++i) {
		const attack_type &att = attacker.attacks()[choices[i]];

		const map_location attacker_loc = best_attacker_location_from_range(units, defender, att.range(), attacker.get_location());

		int def_weapon = choose_defender_weapon(attacker, defender, choices[i], units,
			attacker_loc, defender_loc, prev_def);
		// If that didn't simulate, do so now.
		if (!attacker_combatant_) {
			const attack_type *def = NULL;
			if (def_weapon >= 0) {
				def = &defender.attacks()[def_weapon];
			}

			attacker_stats_ = new unit_stats(attacker, attacker_loc, choices[i],
				true, defender, defender_loc, def, units);
			defender_stats_ = new unit_stats(defender, defender_loc, def_weapon, false,
				attacker, attacker_loc, &att, units);
			attacker_combatant_ = new combatant(*attacker_stats_);
			defender_combatant_ = new combatant(*defender_stats_, prev_def);
			attacker_combatant_->fight(*defender_combatant_);
		}
		if (!best_att_comb || better_combat(*attacker_combatant_, *defender_combatant_,
					*best_att_comb, *best_def_comb, harm_weight)) {
			delete best_att_comb;
			delete best_def_comb;
			delete best_att_stats;
			delete best_def_stats;
			best_att_comb = attacker_combatant_;
			best_def_comb = defender_combatant_;
			best_att_stats = attacker_stats_;
			best_def_stats = defender_stats_;
		} else {
			delete attacker_combatant_;
			delete defender_combatant_;
			delete attacker_stats_;
			delete defender_stats_;
		}
		attacker_combatant_ = NULL;
		defender_combatant_ = NULL;
		attacker_stats_ = NULL;
		defender_stats_ = NULL;
	}

	attacker_combatant_ = best_att_comb;
	defender_combatant_ = best_def_comb;
	attacker_stats_ = best_att_stats;
	defender_stats_ = best_def_stats;

	*defender_weapon = defender_stats_->attack_num;
	return attacker_stats_->attack_num;
}

// Does combat A give us a better result than combat B?
bool battle_context::better_combat(const combatant &us_a, const combatant &them_a,
		const combatant &us_b, const combatant &them_b,
		double harm_weight)
{
	double a, b;

	// Compare: P(we kill them) - P(they kill us).
	a = them_a.dead_ - us_a.dead_ * harm_weight;
	b = them_b.dead_ - us_b.dead_ * harm_weight;
	if (a - b < -0.01)
		return false;
	if (a - b > 0.01)
		return true;

	// Add poison to calculations
	double poison_a_us = (us_a.poisoned) * game_config::poison_amount;
	double poison_a_them = (them_a.poisoned) * game_config::poison_amount;
	double poison_b_us = (us_b.poisoned) * game_config::poison_amount;
	double poison_b_them = (them_b.poisoned) * game_config::poison_amount;
	// Compare: damage to them - damage to us (average_hp replaces -damage)
	a = (us_a.average_hp()-poison_a_us)*harm_weight - (them_a.average_hp()-poison_a_them);
	b = (us_b.average_hp()-poison_b_us)*harm_weight - (them_b.average_hp()-poison_b_them);
	if (a - b < -0.01)
		return false;
	if (a - b > 0.01)
		return true;

	// All else equal: go for most damage.
	return them_a.average_hp() < them_b.average_hp();
}

/** @todo FIXME: better to initialize combatant initially (move into unit_stats?), just do fight() when required. */
const combatant &battle_context::get_attacker_combatant(const combatant *prev_def)
{
	// We calculate this lazily, since AI doesn't always need it.
	if (!attacker_combatant_) {
		assert(!defender_combatant_);
		attacker_combatant_ = new combatant(*attacker_stats_);
		defender_combatant_ = new combatant(*defender_stats_, prev_def);
		attacker_combatant_->fight(*defender_combatant_);
	}
	return *attacker_combatant_;
}

const combatant &battle_context::get_defender_combatant(const combatant *prev_def)
{
	// We calculate this lazily, since AI doesn't always need it.
	if (!defender_combatant_) {
		assert(!attacker_combatant_);
		attacker_combatant_ = new combatant(*attacker_stats_);
		defender_combatant_ = new combatant(*defender_stats_, prev_def);
		attacker_combatant_->fight(*defender_combatant_);
	}
	return *defender_combatant_;
}

battle_context::unit_stats::unit_stats(const unit &u, const map_location& u_loc,
		int u_attack_num, bool attacking,
		const unit &opp, const map_location& opp_loc,
		const attack_type *opp_weapon,
		const unit_map& units) :
	weapon(0),
	attack_num(u_attack_num),
	is_attacker(attacking),
	is_poisoned(u.get_state(unit::STATE_POISONED)),
	is_slowed(u.get_state(unit::STATE_SLOWED)),
	is_broken(u.get_state(unit::STATE_BROKEN)),
	slows(false),
	breaks(false),
	drains(false),
	petrifies(false),
	plagues(false),
	poisons(false),
	backstab_pos(false),
	swarm(false),
	firststrike(false),
	experience(u.experience()),
	max_experience(u.max_experience()),
	level(u.level()),
	rounds(1),
	hp(0),
	max_hp(u.max_hitpoints()),
	chance_to_hit(0),
	damage(0),
	num_blows(0),
	swarm_min(0),
	swarm_max(0),
	plague_type(),
	effecting_tactic()
{
	std::vector<team>& teams = *resources::teams;
	team& u_team = teams[u.side() - 1];

	// Get the current state of the unit.
	if (attack_num >= 0) {
		weapon = &u.attacks()[attack_num];
	}
	if (u.hitpoints() < 0) {
		LOG_CF << "Unit with " << u.hitpoints() << " hitpoints found, set to 0 for damage calculations\n";
		hp = 0;
	} else if (u.hitpoints() > u.max_hitpoints()) {
		// If a unit has more hp as it's maximum the engine will fail
		// with an assertion failure due to accessing the prob_matrix
		// out of bounds.
		hp = u.max_hitpoints();
	} else {
		hp = u.hitpoints();
	}
	const unit* aloc = &u;
	const unit* dloc = &opp;

	if (!attacking) {
		aloc = &opp;
		dloc = &u;
	}

	// Get the weapon characteristics, if any.
	if (weapon) {
		weapon->set_specials_context(aloc, dloc, units, attacking, opp_weapon);
		if (opp_weapon)
			opp_weapon->set_specials_context(aloc, dloc, units, !attacking, weapon);
		bool not_living = opp.get_state("not_living");
		if (unit_feature_val2(u, hero_feature_pure) && !unit_feature_val2(opp, hero_feature_penetrate)) {
			slows = true;
		} else {
			slows = weapon->get_special_bool("slow");
		}
		if (!opp.is_artifical()) {
			if (attacking && u.hide_turns()) {
				breaks = true;
			} else if (unit_feature_val2(u, hero_feature_break) && !unit_feature_val2(opp, hero_feature_penetrate)) {
				breaks = true;
			}
		}
		if (unit_feature_val2(u, hero_feature_drain)) {
			drains = true;
		} else {
			drains = !not_living && weapon->get_special_bool("drains");
		}
		petrifies = weapon->get_special_bool("petrifies");
		if (!attacking || opp.is_artifical() || unit_feature_val2(opp, hero_feature_poison)) {
			// cannot make artifcal be poison
			poisons = false;
		} else if (unit_feature_val2(u, hero_feature_poison)) {
			poisons = true;
		} else {
			poisons = !not_living && weapon->get_special_bool("poison") && !opp.get_state(unit::STATE_POISONED);
		}
		backstab_pos = is_attacker && backstab_check(u_loc, opp_loc, units, *resources::teams);
		rounds = weapon->get_specials("berserk").highest("value", 1).first;
		firststrike = weapon->get_special_bool("firststrike");

		// Handle plague.
		unit_ability_list plague_specials = weapon->get_specials("plague");
		plagues = !not_living && !plague_specials.empty() &&
			strcmp(opp.undead_variation().c_str(), "null") && !resources::game_map->is_village(opp_loc);

		if (plagues) {
			plague_type = (*plague_specials.cfgs.front().first)["type"].str();
			if (plague_type.empty())
				plague_type = u.type_id();
		}

		// Compute chance to hit.
		chance_to_hit = opp.defense_modifier(resources::game_map->get_terrain(opp_loc)) + weapon->accuracy() - (opp_weapon ? opp_weapon->parry() : 0);

		// Compute base damage done with the weapon.
		int base_damage = weapon->damage();
		unit_ability_list dmg_specials = weapon->get_specials("damage");
		unit_abilities::effect dmg_effect(dmg_specials, base_damage, backstab_pos);
		base_damage = dmg_effect.get_composite_value();

		if (backstab_pos) {
			if (unit_feature_val2(u, hero_featrue_backstab) || weapon->get_special_bool("backstab")) {
				base_damage += base_damage;
			}
		}

		// Get the damage multiplier applied to the base damage of the weapon.
		int damage_multiplier = 100;

		// Time of day bonus.
		damage_multiplier += combat_modifier(u_loc, u.alignment(), u.is_fearless());

		if (unit_feature_val2(u, hero_feature_dayattack) || unit_feature_val2(u, hero_feature_nightattack)) {
			// const time_of_day &tod = resources::tod_manager->time_of_day_at(u_loc);
			const time_of_day &tod = resources::tod_manager->get_time_of_day(u_loc);
			if (tod.lawful_bonus > 0 && unit_feature_val2(u, hero_feature_dayattack)) {
				damage_multiplier += tod.lawful_bonus;
			} else if (tod.lawful_bonus < 0 && unit_feature_val2(u, hero_feature_nightattack)) {
				damage_multiplier -= tod.lawful_bonus;
			}
		}

		// Leadership bonus.
		int leader_bonus = 0;
		if (under_leadership(units, u_loc, &leader_bonus).valid()) {
			damage_multiplier += leader_bonus * u_team.cooperate_increase_ / 100;
		}

		if (opp.is_artifical()) {
			damage_multiplier += u.skill_[hero_skill_demolish] * 5;
		}

		// Resistance modifier.
		damage_multiplier *= opp.damage_from(*weapon, !attacking, opp_loc) * chance_to_hit;

		// Compute both the normal and slowed damage. For the record,
		damage = round_damage(base_damage, damage_multiplier, 1000000);
		
		if (opp.get_state(unit::STATE_BROKEN)) {
			damage = damage * 3 / 2;
		}
				
		// conside wall
		if (!u.wall()) {
			unit_map::const_iterator itor = units.find(u_loc, false);
			if (itor.valid() && itor->wall()) {
				damage += damage / 10;
			}
		}
		if (!opp.wall()) {
			unit_map::const_iterator itor = units.find(opp_loc, false);
			if (itor.valid() && itor->wall()) {
				damage -= damage / 3;
			}
		}

		if (u.provoked_turns()) {
			damage += damage / 5;
		}
		if (opp.provoked_turns()) {
			damage += damage / 5;
		}

		// tactic on ea effect
		std::vector<artifical*>& holded = u_team.holded_cities();
		for (std::vector<artifical*>::const_iterator it = holded.begin(); it != holded.end(); ++ it) {
			artifical& city = **it;
			artifical* t = city.tactic_on_ea();
			if (!t || t->get_location() == u_loc) {
				continue;
			}
			int distance = 2 + t->level();
			if ((int)distance_between(u_loc, city.get_location()) <= distance) {
				damage += damage / 5;
				effecting_tactic = t->get_location();
				break;
			}
		}			

		if (is_slowed) {
			damage = damage / 2;
		}
		
		// Compute the number of blows and handle swarm.
		unit_ability_list swarm_specials = weapon->get_specials("swarm");

		if (!swarm_specials.empty()) {
			swarm = true;
			swarm_min = swarm_specials.highest("swarm_attacks_min").first;
			swarm_max = swarm_specials.highest("swarm_attacks_max", weapon->num_attacks()).first;
			num_blows = swarm_min + (swarm_max - swarm_min) * hp / max_hp;
		} else {
			swarm = false;
			num_blows = weapon->num_attacks();
			unit_ability_list attacks_specials = weapon->get_specials("attacks");
			unit_abilities::effect attacks_effect(attacks_specials,num_blows,backstab_pos);
			num_blows = attacks_effect.get_composite_value();
			swarm_min = num_blows;
			swarm_max = num_blows;
		}
	}
}

battle_context::unit_stats::~unit_stats()
{
}

void battle_context::unit_stats::dump() const
{
	printf("==================================\n");
	printf("is_attacker:	%d\n", static_cast<int>(is_attacker));
	printf("is_poisoned:	%d\n", static_cast<int>(is_poisoned));
	printf("is_slowed:	%d\n", static_cast<int>(is_slowed));
	printf("slows:		%d\n", static_cast<int>(slows));
	printf("drains:		%d\n", static_cast<int>(drains));
	printf("petrifies:	%d\n", static_cast<int>(petrifies));
	printf("poisons:	%d\n", static_cast<int>(poisons));
	printf("swarm:		%d\n", static_cast<int>(swarm));
	printf("rounds:	%d\n", static_cast<int>(rounds));
	printf("firststrike:	%d\n", static_cast<int>(firststrike));
	printf("\n");
	printf("hp:		%d\n", hp);
	printf("max_hp:		%d\n", max_hp);
	printf("chance_to_hit:	%d\n", chance_to_hit);
	printf("damage:		%d\n", damage);
	printf("num_blows:	%d\n", num_blows);
	printf("swarm_min:	%d\n", swarm_min);
	printf("swarm_max:	%d\n", swarm_max);
	printf("\n");
}

// Given this harm_weight, are we better than this other context?
bool battle_context::better_attack(class battle_context &that, double harm_weight)
{
	return better_combat(get_attacker_combatant(), get_defender_combatant(),
			that.get_attacker_combatant(), that.get_defender_combatant(), harm_weight);
}

/** Helper class for performing an attack. */
class attack
{
public:
	/** Structure holding unit info used in the attack action. */
	struct unit_info
	{
		unit_map& units_;
		map_location loc_;
		int weapon_;
		bool base_;
		std::string weap_id_;
		int orig_attacks_;
		int n_attacks_; /**< Number of attacks left. */
		int cth_;
		int damage_;
		int xp_;
		bool reinforced_;

		unit_info(unit& u, int weapon, unit_map &units);
		unit &get_unit();
		bool valid();

		int cause_damage_;
		int been_damage_;
		int defeat_units_;
		int activity_;

		std::string dump();
	};
	friend std::pair<map_location, map_location> attack_unit(unit &, unit &, int, int, bool, const config&, bool);

	attack(unit& attacker, unit& defender, int attack_with, int defend_with, bool update_display = true, const config& = config::invalid, bool move = true);
	std::pair<map_location, map_location> perform();
	bool perform_hit(bool, statistics::attack_context &);
	bool weapon_move(unit& u, const map_location& from, const map_location& to, std::set<map_location>& invalid_locs);
	~attack();

	class attack_end_exception {};
	void fire_event(const std::string& n);
	void refresh_bc();

	battle_context *bc_;
	const battle_context::unit_stats *a_stats_;
	const battle_context::unit_stats *d_stats_;

	int abs_n_attack_, abs_n_defend_;
	bool update_att_fog_, update_def_fog_, update_minimap_;

	unit_ability_list a_attackpoints_;
	unit_ability_list d_attackpoints_;

	unit_info a_, d_;
	unit_map &units_;
	hero_map& heros_;
	game_display& gui_;
	std::vector<team>& teams_;
	play_controller& controller_;
	const config& duel_;
	std::ostringstream errbuf_;
	bool move_;

	bool update_display_;
	bool OOS_error_;
};

void attack::fire_event(const std::string& n)
{
	LOG_NG << "firing " << n << " event\n";
	//prepare the event data for weapon filtering
	config ev_data;
	config& a_weapon_cfg = ev_data.add_child("first");
	config& d_weapon_cfg = ev_data.add_child("second");
	if(a_stats_->weapon != NULL && a_.valid()) {
		a_weapon_cfg = a_stats_->weapon->get_cfg();
	}
	if(d_stats_->weapon != NULL && d_.valid()) {
		d_weapon_cfg = d_stats_->weapon->get_cfg();
	}
	if(a_weapon_cfg["name"].empty()) {
		a_weapon_cfg["name"] = "none";
	}
	if(d_weapon_cfg["name"].empty()) {
		d_weapon_cfg["name"] = "none";
	}
	if(n == "attack_end") {
		// We want to fire attack_end event in any case! Even if one of units was removed by WML
		game_events::fire(n, a_.loc_, d_.loc_, ev_data);
		return;
	}
	const int defender_side = d_.get_unit().side();
	const int attacker_side = a_.get_unit().side();
	game_events::fire(n, game_events::entity_location(a_.loc_, 0),
		game_events::entity_location(d_.loc_, 0), ev_data);

	// The event could have killed either the attacker or
	// defender, so we have to make sure they still exist
	refresh_bc();
	if(!a_.valid() || !d_.valid()) {
		if (update_display_){
			recalculate_fog(attacker_side);
			recalculate_fog(defender_side);
			resources::screen->recalculate_minimap();
			resources::screen->draw(true, true);
		}
		fire_event("attack_end");
		throw attack_end_exception();
	}
}

namespace {
	void refresh_weapon_index(int& weap_index, std::string const& weap_id, std::vector<attack_type> const& attacks) {
		if(attacks.empty()) {
			//no attacks to choose from
			weap_index = -1;
			return;
		}
		if(weap_index >= 0 && weap_index < static_cast<int>(attacks.size()) && attacks[weap_index].id() == weap_id) {
			//the currently selected attack fits
			return;
		}
		if(!weap_id.empty()) {
			//lookup the weapon by id
			for(int i=0; i<static_cast<int>(attacks.size()); ++i) {
				if(attacks[i].id() == weap_id) {
					weap_index = i;
					return;
				}
			}
		}
		//lookup has failed
		weap_index = -1;
		return;
	}
} //end anonymous namespace

void attack::refresh_bc()
{
	// Fix index of weapons
	if (a_.valid()) {
		refresh_weapon_index(a_.weapon_, a_.weap_id_, a_.get_unit().attacks());
	}
	if (d_.valid()) {
		refresh_weapon_index(d_.weapon_, d_.weap_id_, d_.get_unit().attacks());
	}
	if(!a_.valid() || !d_.valid()) {
		// Fix pointer to weapons
		battle_context::unit_stats* a_stats = const_cast<battle_context::unit_stats*>(a_stats_);
		if (a_.valid() && a_.weapon_ >= 0) {
			a_stats->weapon = &a_.get_unit().attacks()[a_.weapon_];
			a_stats->weapon->set_specials_context(&a_.get_unit(), NULL, units_, true, NULL);
		} else {
			a_stats->weapon = NULL;
		}

		battle_context::unit_stats* d_stats = const_cast<battle_context::unit_stats*>(d_stats_);
		if (d_.valid() && d_.weapon_ >= 0) {
			d_stats->weapon = &d_.get_unit().attacks()[d_.weapon_];
			d_stats->weapon->set_specials_context(NULL, &d_.get_unit(), units_, false, NULL);
		} else {
			d_stats->weapon = NULL;
		}

		return;
	}

	*bc_ =	battle_context(units_, a_.get_unit(), d_.get_unit(), a_.weapon_, d_.weapon_);
	a_stats_ = &bc_->get_attacker_stats();
	d_stats_ = &bc_->get_defender_stats();
	a_.cth_ = a_stats_->chance_to_hit;
	d_.cth_ = d_stats_->chance_to_hit;
	a_.damage_ = a_stats_->damage;
	d_.damage_ = d_stats_->damage;
}

attack::unit_info::unit_info(unit& u, int weapon, unit_map& units)
	: units_(units)
	, loc_(u.get_location())
	, weapon_(weapon)
	, base_(u.base())
	, weap_id_()
	, orig_attacks_(0)
	, n_attacks_(0)
	, cth_(0)
	, damage_(0)
	, xp_(0)
	, reinforced_(false)
	, cause_damage_(0)
	, been_damage_(0)
	, defeat_units_(0)
	, activity_(0)
{
}

unit& attack::unit_info::get_unit()
{
	return *units_.find(loc_, !base_);
}

bool attack::unit_info::valid()
{
	unit_map::const_iterator u_itor = units_.find(loc_, !base_);
	return u_itor.valid() && u_itor->verifying();
}

std::string attack::unit_info::dump()
{
	std::stringstream s;
	s << get_unit().type_id() << " (" << loc_.x + 1 << ',' << loc_.y + 1 << ')';
	return s.str();
}

class event_lock
{
public:
	event_lock(unit_map& units, unit& defender) 
		: units_(units)
		, loc_(defender.get_location())
		, base_(defender.base())
		, verify_(defender.verifying())
	{
		defender.set_verifying();
	}

	bool valid()
	{
		unit_map::iterator itor = units_.find(loc_, !base_);
		if (itor == units_.end()) {
			return false; 
		}
		return itor->verifying();
	}

	~event_lock() 
	{
		unit_map::iterator itor = units_.find(loc_, !base_);
		if (!itor.valid()) {
			return; 
		}
		if (itor->verifying()) {
			itor->set_verifying(verify_);
		}
	}

private:
	unit_map& units_;
	const map_location& loc_;
	bool base_;
	bool verify_;
};

// @a: attack. 这个attack并不总是attack::attack中的a_, 有可能是d_, 它是胜利一方
// @die: unit of die. 它是被击败一方
void unit_die(unit_map& units, unit& die, void* a_info_p, int die_activity, int a_side)
{
	std::vector<team>& teams = *resources::teams;
	attack::unit_info* a_info = NULL;
	unit* attacker = NULL;
	artifical* cobj = units.city_from_cityno(die.cityno());

	if (a_info_p) {
		a_info = reinterpret_cast<attack::unit_info*>(a_info_p);
		attacker = &a_info->get_unit();

		team& t = teams[attacker->side() - 1];
		if (tent::mode == TOWER_MODE && t.is_human()) {
			t.spend_gold(-1 * die.cost());
		}
	}

	if (!die.is_artifical()) {
		int merit = die.cause_damage_ + std::min<int>(75, die.field_turns_ * 25);
		int activity;
		if (merit < 150) {
			activity = -1 * game_config::maximal_defeated_activity * (150 - merit) / 150;
		} else {
			activity = 0;
		}
		activity += die_activity;
		if (activity) {
			die.increase_activity(activity, false);
		}

		std::vector<hero*> back, roam;
		if (!die.is_commoner()) {
			hero* h = &die.master();
			if (h != rpg::h && h->get_flag(hero_flag_roam)) {
				// <闲云野鹤>，所属部队被击溃后随机去一个城市
				roam.push_back(h);
			} else if (cobj) {
				back.push_back(h);
			}

			if (die.second().valid()) {
				h = &die.second();
				if (h != rpg::h && h->get_flag(hero_flag_roam)) {
					roam.push_back(h);
				} else if (cobj) {
					back.push_back(h);
				}
			}
			if (die.third().valid()) {
				h = &die.third();
				if (h != rpg::h && h->get_flag(hero_flag_roam)) {
					roam.push_back(h);
				} else if (cobj) {
					back.push_back(h);
				}
			}
		}

		if (back.size()) {
			// 武将回到所在城市
			for (std::vector<hero*>::const_iterator itor = back.begin(); itor != back.end(); ++ itor) {
				cobj->finish_into(**itor, hero_status_backing);
			}
			resources::screen->invalidate(cobj->get_location());
		}
		if (roam.size()) {
			// 武将随机流浪到一个城市
			std::vector<artifical*> citys;
			for (size_t i = 0; i < (*resources::teams).size(); i ++) {
				std::vector<artifical*>& side_citys = (*resources::teams)[i].holded_cities();
				citys.insert(citys.end(), side_citys.begin(), side_citys.end());
			}
			cobj = citys[get_random() % citys.size()];
			for (std::vector<hero*>::iterator itor = roam.begin(); itor != roam.end(); ++ itor) {
				hero* h = *itor;
				if (cobj->side() == team::empty_side_) {
					cobj->wander_into(*h, false);
				} else {
					// recommand! let float_catalog to base catalog.
					h->float_catalog_ = ftofxp8(h->base_catalog_);
					cobj->move_into(*h);
				}

				game_events::show_hero_message(h, cobj, _("Let me join in. I will do my best to maintenance our honor."), game_events::INCIDENT_RECOMMENDONESELF);
			}
			resources::screen->invalidate(cobj->get_location());
		}

		units.erase(&die);

	} else if (die.can_reside()) { 
		// defend type: city
		// alternate.
		VALIDATE(a_info_p || (a_side > 0 && a_side <= (int)teams.size()), "unit_die, die is city, both a_info_p and a_ side must valid one!");

		if (a_side == HEROS_INVALID_SIDE) {
			a_info->xp_ += 16; // 攻下城的部队xp=原得xp+16
			cobj = dynamic_cast<artifical*>(&die);
			if (attacker->is_artifical() || attacker->master().get_flag(hero_flag_robber)) {
				// artifical/<流寇>，攻下城后不进城
				cobj->fallen(team::empty_side_);
			} else {
				attacker->cause_damage_ += a_info->cause_damage_;
				attacker->been_damage_ += a_info->been_damage_;
				attacker->defeat_units_ += a_info->defeat_units_;

				team& attacker_team = teams[attacker->side() - 1];
				attacker_team.cause_damage_ += a_info->cause_damage_;
				attacker_team.been_damage_ += a_info->been_damage_;
				attacker_team.defeat_units_ += a_info->defeat_units_;

				if (a_info->activity_) {
					attacker->increase_activity(a_info->activity_);
				}
				attacker->get_experience(increase_xp::attack_ublock(*attacker, true), a_info->xp_);
				// dialogs::advance_unit(a.get_location());
				cobj->fallen(attacker->side(), attacker);
				units.erase(attacker);
			}
		} else {
			cobj->fallen(a_side);
		}
		// @todo FIXME: need normalize to call clear_status_caches()
		// unit::clear_status_caches();
	}  else {
		// defend type: artifical
		if (die.wall()) {
			units.city_from_cityno(die.cityno())->increase_police(-4);
		}
		units.erase(&die);
	}
}

std::pair<map_location, map_location> attack_unit(unit& attacker, unit& defender,
	int attack_with, int defend_with, bool update_display, const config& duel, bool move)
{
	attack dummy(attacker, defender, attack_with, defend_with, update_display, duel, move);
	return dummy.perform();
}

attack::attack(unit& attacker, unit& defender,
		int attack_with,
		int defend_with,
		bool update_display,
		const config& duel,
		bool move) :
	bc_(0),
	a_stats_(0),
	d_stats_(0),
	abs_n_attack_(0),
	abs_n_defend_(0),
	update_att_fog_(false),
	update_def_fog_(false),
	update_minimap_(false),
	a_(attacker, attack_with, *resources::units),
	d_(defender, defend_with, *resources::units),
	units_(*resources::units),
	heros_(*resources::heros),
	gui_(*resources::screen),
	teams_(*resources::teams),
	controller_(*resources::controller),
	errbuf_(),
	update_display_(update_display),
	duel_(duel),
	move_(move),
	OOS_error_(false)
{
	attacker.set_verifying();
	defender.set_verifying();
}

attack::~attack()
{
	delete bc_;
	if (a_.valid()) {
		a_.get_unit().set_verifying(false);
	}
	if (d_.valid()) {
		d_.get_unit().set_verifying(false);
	}
}

bool attack::perform_hit(bool attacker_turn, statistics::attack_context &stats)
{
	unit_info &attacker = *(attacker_turn ? &a_ : &d_), &defender = *(attacker_turn ? &d_ : &a_);
	unit& attacker_u = attacker.get_unit();
	unit* center_defender_ptr = &defender.get_unit();
	const battle_context::unit_stats
		*&attacker_stats = *(attacker_turn ? &a_stats_ : &d_stats_),
		*&defender_stats = *(attacker_turn ? &d_stats_ : &a_stats_);
	int &abs_n = *(attacker_turn ? &abs_n_attack_ : &abs_n_defend_);
	bool &update_fog = *(attacker_turn ? &update_def_fog_ : &update_att_fog_);
	unit_ability_list& attackpoints = *(attacker_turn? &a_attackpoints_: &d_attackpoints_);
	const team& attack_team = teams_[attacker.get_unit().side() - 1];

	int ran_num = get_random();
	// bool hits = (ran_num % 100) < attacker.cth_;
	bool hits = true;
	if (unit_feature_val2(defender.get_unit(), hero_feature_diamond)) {
		// 30% miss/ 70% hit
		hits = (ran_num % 100) < 70;
	}
	bool injured = false;
	if (attacker_turn && unit_feature_val2(attacker.get_unit(), hero_feature_fearless) && !center_defender_ptr->is_artifical() && !center_defender_ptr->is_commoner()) {
		// 20% miss/ 80% hit
		if ((ran_num % 100) < 20) {
			defender.activity_ -= 30;
			injured = true;
		}
	}

	// encourage section
	bool stronger = false;
	if (attacker_turn && (!attacker_u.is_artifical() || attacker_u.is_city()) && 
		(!center_defender_ptr->is_artifical() || center_defender_ptr->is_city())) {

		int encourage_diff = attacker_u.skill_[hero_skill_encourage] - center_defender_ptr->skill_[hero_skill_encourage];
		if (attacker_u.get_ability_bool("encourage")) {
			encourage_diff += 2; // 20%
		}
		if (center_defender_ptr->get_ability_bool("encourage")) {
			encourage_diff -= 2; // 20%
		}
		if (encourage_diff > 0) {
			const int encourage_threshold = 8; // 80%
			encourage_diff += (3 * attacker_u.character_level(apply_to_tag::PHILOSOPHICAL)) / 100;
			if (encourage_diff > encourage_threshold) {
				encourage_diff = encourage_threshold;
			}
			int normalize_encourage = encourage_threshold * 5 / 4;
			if ((ran_num % normalize_encourage) < encourage_diff) {
				stronger = true;
			}
		}
	}

	// indomitable section
	bool indomitable = false;
	if (center_defender_ptr->hitpoints() < center_defender_ptr->max_hitpoints() / 2) {
		if (!(ran_num % 4) && center_defender_ptr->get_ability_bool("indomitable")) {
			indomitable = true;
		}
	}
	
	int damage = 0;
	if (hits) {
		damage = attacker.damage_;
		if (attacker_turn) {
			if (attacker.reinforced_) {
				damage = damage * 3 / 2;
			} 
			if (defender.reinforced_) {
				damage = damage * 2 / 3;
			}
		}
		if (stronger) {
			damage = damage * 3 / 2;
		}
		if (indomitable) {
			damage = damage / 2;
		}
		resources::state_of_game->set_variable("damage_inflicted", str_cast<int>(damage));
	}

	// Make sure that if we're serializing a game here,
	// we got the same results as the game did originally.
	const config *ran_results = get_random_results();
	if (ran_results)
	{
		int results_chance = (*ran_results)["chance"].to_int();
		bool results_hits = (*ran_results)["hits"].to_bool();
		int results_damage = (*ran_results)["damage"].to_int();

		if (results_chance != attacker.cth_)
		{
			errbuf_ << "SYNC: In attack " << a_.dump() << " vs " << d_.dump()
				<< ": chance to hit is inconsistent. Data source: "
				<< results_chance << "; Calculation: " << attacker.cth_
				<< " (over-riding game calculations with data source results)\n";
			attacker.cth_ = results_chance;
			OOS_error_ = true;
		}

		if (results_hits != hits)
		{
			errbuf_ << "SYNC: In attack " << a_.dump() << " vs " << d_.dump()
				<< ": the data source says the hit was "
				<< (results_hits ? "successful" : "unsuccessful")
				<< ", while in-game calculations say the hit was "
				<< (hits ? "successful" : "unsuccessful")
				<< " random number: " << ran_num << " = "
				<< (ran_num % 100) << "/" << results_chance
				<< " (over-riding game calculations with data source results)\n";
			hits = results_hits;
			OOS_error_ = true;
		}

		if (results_damage != damage)
		{
			errbuf_ << "SYNC: In attack " << a_.dump() << " vs " << d_.dump()
				<< ": the data source says the hit did " << results_damage
				<< " damage, while in-game calculations show the hit doing "
				<< damage
				<< " damage (over-riding game calculations with data source results)\n";
			damage = results_damage;
			OOS_error_ = true;
		}
	}

	int drains_damage = 0;
	if (attacker_stats->drains) {
		// don't drain so much that the attacker gets more than his maximum hitpoints
		drains_damage = std::min<int>(damage / 4, attacker.get_unit().max_hitpoints() - attacker.get_unit().hitpoints());
		// don't drain more than the defenders remaining hitpoints
		drains_damage = std::min<int>(drains_damage, defender.get_unit().hitpoints() / 2);
	}
	int damage_done = std::min<int>(defender.get_unit().hitpoints(), attacker.damage_);
	bool defender_dies = defender.get_unit().hitpoints() <= attacker.damage_;
	if (attacker_turn) {
		stats.attack_result(hits
			? (defender_dies ? statistics::attack_context::KILLS : statistics::attack_context::HITS)
			: statistics::attack_context::MISSES, damage_done, drains_damage);
	} else {
		stats.defend_result(hits
			? (defender_dies ? statistics::attack_context::KILLS : statistics::attack_context::HITS)
			: statistics::attack_context::MISSES, damage_done, drains_damage);
	}

	if (!ran_results) {
		log_scope2(log_engine, "setting random results");
		config cfg;
		cfg["hits"] = hits ? "yes" : "no";
		cfg["dies"] = defender_dies ? "yes" : "no";
		cfg["unit_hit"] = "defender";
		cfg["damage"] = lexical_cast<std::string>(damage);
		cfg["chance"] = lexical_cast<std::string>(attacker.cth_);

		set_random_results(cfg);
	} else {
		bool results_dies = (*ran_results)["dies"] == "yes";
		if (results_dies != defender_dies) {
			errbuf_ << "SYNC: In attack " << a_.dump() << " vs " << d_.dump()
				<< ": the data source says the "
				<< (attacker_turn ? "defender" : "attacker")
				<< (results_dies ? " perished" : " survived")
				<< " while in-game calculations show it "
				<< (defender_dies ? "perished" : "survived")
				<< " (over-riding game calculations with data source results)\n";
			defender_dies = results_dies;
			// Set hitpoints to 0 so later checks don't invalidate the death.
			// Maybe set to > 0 for the else case to avoid more errors?
			if (results_dies) defender.get_unit().set_hitpoints(0);
			OOS_error_ = true;
		}
	}

	if (drains_damage > 0) {
		attacker.get_unit().heal(drains_damage);
	}

	std::vector<std::pair<unit*, int> > damage_locs;
	std::set<unit*> assist_find;
	
	assist_find.insert(&defender.get_unit());
	if (!attacker_turn || attackpoints.empty()) {
		damage_locs.push_back(std::make_pair<unit*, int>(&defender.get_unit(), damage));
	} else {
		std::pair<const config *, unit *>& cfg1 = attackpoints.cfgs.front();
		const config& cfg = *(cfg1.first);
		bool affect_self = cfg["affect_self"].to_bool(true);
		std::vector<std::string> attack_points = utils::split(cfg["points"]);
		std::vector<std::string> attack_values = utils::split(cfg["values"]);
		size_t points = attack_points.size(), values = attack_values.size();

		if (affect_self) {
			damage_locs.push_back(std::make_pair<unit*, int>(&defender.get_unit(), damage));
		} else {
			damage_locs.push_back(std::make_pair<unit*, int>(&defender.get_unit(), 0));
		}
		
		const map_location& attacker_loc = attacker.get_unit().get_location();
		const map_location& defender_loc = defender.get_unit().get_location();

		int bonus = 0;
		if (unit_feature_val2(attacker.get_unit(), hero_feature_interlink)) {
			bonus = 50;
		} else if (unit_feature_val2(attacker.get_unit(), hero_feature_faction)) {
			bonus = 20;
		}
		for (size_t i = 0; i < points; i ++) {
			map_location loc = defender_loc.get_direction(attack_points[i], attacker_loc);
			if (!loc.valid()) {
				continue;
			}
			int value; 
			if (values > i) {
				value = lexical_cast_default<int>(attack_values[i]);
			} else {
				// default is 7/20
				value = 35;
			}
			value = value + bonus;

			unit_map::iterator itor = units_.find(loc);
			if (!itor.valid()) {
				itor = units_.find(loc, false);
			}
			if (itor.valid() && attack_team.is_enemy(itor->side()) && !itor->invisible(loc)) {
				unit* u_ptr = &*itor;
				// must make u_ptr is exclusive. 
				// if isn't exclusive it may result in second or later same u_ptr to become invalid and program exit exceptional  
				if (assist_find.find(u_ptr) != assist_find.end()) {
					continue;
				}
				battle_context bc(*resources::units, attacker.get_unit(), *itor, attacker.weapon_, -1);
				const battle_context::unit_stats* a_stats = &bc.get_attacker_stats();
				damage_locs.push_back(std::make_pair(u_ptr, a_stats->damage * value * attack_team.interlink_increase_ / 10000));
				assist_find.insert(u_ptr);
			}
		}
	}
	// unit_attack parameters
	std::vector<unit*> def_ptr_vec;
	std::vector<int> damage_vec;
	std::vector<std::string> hit_text_vec;
	for (std::vector<std::pair<unit*, int> >::const_iterator loc = damage_locs.begin(); loc != damage_locs.end(); ++ loc) {
		unit* defender_ptr = loc->first;
		std::ostringstream float_text;
		if (hits) {
			const unit &defender_unit = *defender_ptr;
			// special only effect center(primary) defender.
			// for example, second defender has city, poisoned cannot effect it.
			if (defender_ptr == center_defender_ptr) {
				if (attacker_stats->poisons && !defender_unit.get_state(unit::STATE_POISONED)) {
					// help::tintegrate::generate_img(float_text, "misc/poisoned.png");
					// float_text << '\n';
					float_text << (defender_unit.gender() == unit_race::FEMALE ?
						_("female^poisoned") : _("poisoned")) << '\n';
				}

				if (attacker_stats->slows && !defender_unit.get_state(unit::STATE_SLOWED)) {
					// help::tintegrate::generate_img(float_text, "misc/slowed.png");
					// float_text << '\n';
					float_text << (defender_unit.gender() == unit_race::FEMALE ?
						_("female^slowed") : _("slowed")) << '\n';
				}

				if (attacker_stats->breaks && !defender_unit.get_state(unit::STATE_BROKEN)) {
					// help::tintegrate::generate_img(float_text, "misc/broken.png");
					// float_text << '\n';
					float_text << (defender_unit.gender() == unit_race::FEMALE ?
						_("female^broken") : _("broken")) << '\n';
				}

				if (attacker_stats->petrifies) {
					// help::tintegrate::generate_img(float_text, "misc/petrified.png");
					// float_text << '\n';
					float_text << (defender_unit.gender() == unit_race::FEMALE ?
						_("female^petrified") : _("petrified")) << '\n';
				}

				if (injured) {
					float_text << (defender_unit.gender() == unit_race::FEMALE ?
						_("female^injured") : _("injured")) << '\n';
				}

				if (indomitable) {
					float_text << _("indomitable") << '\n';
				}
			}
		}
		def_ptr_vec.push_back(defender_ptr);
		damage_vec.push_back(loc->second);
		hit_text_vec.push_back(float_text.str());
	}

	unit_map::iterator find = units_.find(attacker_stats->effecting_tactic);
	artifical* t = (find.valid() && find->type()->master() == hero::number_tactic)? unit_2_artifical(&*find): NULL;
	unit_display::unit_attack(attacker.get_unit(), def_ptr_vec, damage_vec,
		attacker_stats->weapon, defender_stats->weapon,
		abs_n, hit_text_vec, attacker_stats->drains, stronger, "", t);

	bool center_defender_survived = true;
	size_t index_in_locs = 0;
	for (std::vector<std::pair<unit*, int> >::const_iterator loc = damage_locs.begin(); loc != damage_locs.end(); ++ loc, index_in_locs ++) {
		unit* defender_ptr = loc->first;

		// defender_loc must use instance instead of reference. last breath/die may earse defender_ptr. 
		const map_location defender_loc = defender_ptr->get_location();
		const attack_type* defender_weapon = defender_ptr == center_defender_ptr? defender_stats->weapon: NULL;
		
		bool dies = loc->first->take_hit(loc->second);

		if (defender_ptr == center_defender_ptr) {
			if (hits) {
				try {
					fire_event(attacker_turn ? "attacker_hits" : "defender_hits");
				} catch (attack_end_exception) {
					refresh_bc();
					return false;
				}
			} else {
				try {
					fire_event(attacker_turn ? "attacker_misses" : "defender_misses");
				} catch (attack_end_exception) {
					refresh_bc();
					return false;
				}
			}
			refresh_bc();
		}

		attacker.cause_damage_ += loc->second;
		defender.been_damage_ += loc->second;

		if (dies) {
			// when dies, hitpoints may be negative, below tow addition is subtration actually.
			attacker.cause_damage_ += loc->first->hitpoints();
			defender.been_damage_ += loc->first->hitpoints();
			attacker.defeat_units_ ++;

			if (defender_ptr == center_defender_ptr) {
				attacker.xp_ = game_config::kill_xp(3 + defender.get_unit().level());
				defender.xp_ = 0;
				resources::screen->invalidate(attacker.loc_);
			}			

			game_events::entity_location death_loc(defender_loc, 0);
			game_events::entity_location attacker_loc(attacker.loc_, 0);
			std::string undead_variation = defender_ptr->undead_variation();
			if (defender_ptr == center_defender_ptr) {
				fire_event("attack_end");
				refresh_bc();
			}
						
			// get weapon info for last_breath and die events
			config dat;
			config a_weapon_cfg = attacker_stats->weapon && attacker.valid() ?
				attacker_stats->weapon->get_cfg() : config();
			config d_weapon_cfg = (defender_ptr == center_defender_ptr) && defender_stats->weapon && defender.valid() ?
				defender_stats->weapon->get_cfg() : config();
			if (a_weapon_cfg["name"].empty())
				a_weapon_cfg["name"] = "none";
			if (d_weapon_cfg["name"].empty())
				d_weapon_cfg["name"] = "none";
			dat.add_child("first",  d_weapon_cfg);
			dat.add_child("second", a_weapon_cfg);

			{
				event_lock lock(units_, *defender_ptr);

				if (game_events::fire("last breath", death_loc, attacker_loc, dat)) {
					refresh_bc();
				}
				// recalculate defender_pointer
				if (!lock.valid()) {
					if (defender_ptr == center_defender_ptr) {
						attacker_stats->weapon->other_attack_ = NULL;
						center_defender_survived = false;
					}
					// WML has invalidated the dying unit, abort
					continue;
				}
			}

			if (!attacker.valid()) {
				unit_display::unit_die(defender_loc, *defender_ptr, NULL, defender_weapon);
			} else {
				unit_display::unit_die(defender_loc, *defender_ptr, attacker_stats->weapon, defender_weapon,
					attacker.loc_, &attacker.get_unit());
			}

			{
				event_lock lock(units_, *defender_ptr);

				if (game_events::fire("die", death_loc, attacker_loc, dat)) {
					refresh_bc();
				}
				// recalculate defender_pointer
				if (!lock.valid()) {
					if (defender_ptr == center_defender_ptr) {
						attacker_stats->weapon->other_attack_ = NULL;
						center_defender_survived = false;
					}
					// WML has invalidated the dying unit, abort
					continue;
				}
			}

			unit_die(units_, *defender_ptr, &attacker, defender_ptr == center_defender_ptr? defender.activity_: 0);
			refresh_bc();

			if (defender_ptr == center_defender_ptr) {
				if (attacker.valid()) {
					// attacker may be erase!!
					// for example, capture one city, attack will be erase. 
					// attacker_stats->weapon is a pointer that pointed to attack() of unit(attacker), must not be NULL when attacker became invalid.
					attacker_stats->weapon->other_attack_ = NULL;
				}
				center_defender_survived = false;

				if (attacker.valid() && attacker_stats->plagues) {
					// plague units make new units on the target hex
					LOG_NG << "trying to reanimate " << attacker_stats->plague_type << '\n';
					const unit_type *reanimator = unit_types.find(attacker_stats->plague_type);
					if (reanimator) {
						LOG_NG << "found unit type:" << reanimator->id() << '\n';

						std::vector<const hero*> v;
						v.push_back(&heros_[0]);
						type_heros_pair pair(reanimator, v);
						unit newunit(units_, heros_, teams_, pair, attacker.get_unit().side(), true, true);
						newunit.set_attacks(0);
						newunit.set_movement(0);
						// Apply variation
						if (undead_variation != "null") {
							config mod;
							config &variation = mod.add_child("effect");
							variation["apply_to"] = "variation";
							variation["name"] = undead_variation;
							newunit.add_modification(mod);
							newunit.heal_all();
						}
						units_.add(death_loc, &newunit);
						preferences::encountered_units().insert(newunit.type_id());
						if (update_display_) {
							resources::screen->invalidate(death_loc);
						}
					}
				} else {
					LOG_NG << "unit not reanimated\n";
				}
			}

			update_fog = true;
			update_minimap_ = true;
		} else if (hits) {

			unit& defender_unit = *defender_ptr;
			if (defender_ptr == center_defender_ptr) {
				if (attacker_stats->poisons && !defender_unit.get_state(unit::STATE_POISONED)) {
					defender_unit.set_state(unit::STATE_POISONED, true);
				}

				if (attacker_stats->slows && !defender_unit.get_state(unit::STATE_SLOWED)) {
					defender_unit.set_state(unit::STATE_SLOWED, true);
					update_fog = true;
					if (defender_ptr == center_defender_ptr) {
						// defneder may be broken, need recalculate all.
						refresh_bc();
					}
				}

				if (attacker_stats->breaks && !defender_unit.get_state(unit::STATE_BROKEN)) {
					defender_unit.set_state(unit::STATE_BROKEN, true);
					update_fog = true;
					if (defender_ptr == center_defender_ptr) {
						// attacker may be slowed, need recalculate all.
						refresh_bc();
					}
				}

				// If the defender is petrified, the fight stops immediately
				if (attacker_stats->petrifies) {
					defender_unit.set_state(unit::STATE_PETRIFIED, true);
					update_fog = true;
					if (defender_ptr == center_defender_ptr) {
						attacker.n_attacks_ = 0;
						defender.n_attacks_ = -1; // Petrified.
						game_events::fire("petrified", defender.loc_, attacker.loc_);
						refresh_bc();
					}
				}
			}
		}
	}

	-- attacker.n_attacks_;

	// true: both center defender and attacker are survived.
	return center_defender_survived && attacker.valid();
}

bool attack::weapon_move(unit& u, const map_location& from, const map_location& to, std::set<map_location>& invalid_locs)
{
	gamemap& map = *resources::game_map;

	unit_map::const_iterator u_base_itor = units_.find(to, false);
	if (map.on_board(to) && !units_.count(to) && (u.movement_cost(map[to], &to) != unit_movement_type::UNREACHABLE) && (!u_base_itor.valid() || !u_base_itor->wall())) {
		for (size_t i = 0; i < u.adjacent_size_; i ++) {
			invalid_locs.insert(u.adjacent_[i]);
		}
		resources::units->move(from, to);
		invalid_locs.insert(to);
		if (map.is_village(to)) {
			int orig_village_owner = village_owner(to, *resources::teams);
			size_t team_num = u.side() -1;
			if (size_t(orig_village_owner) != team_num) {
				get_village(to, team_num + 1);
			}
		}

		return true;
	}
	return false;
}

std::pair<map_location, map_location> attack::perform()
{
	// Stop the user from issuing any commands while the units are fighting
	const events::command_disabler disable_commands;

	std::pair<map_location, map_location> new_locs = std::make_pair<map_location, map_location>(a_.loc_, d_.loc_);

	unit& attacker = a_.get_unit();
	unit& defender = d_.get_unit();
	bool attacker_is_artifical = attacker.is_artifical();
	bool attacker_is_tower = (tent::mode == TOWER_MODE) && teams_[attacker.side() - 1].is_human();
	bool defender_is_artifical = defender.is_artifical();
	bool defender_is_commoner = defender.is_commoner();

	a_.get_unit().set_attacks(a_.get_unit().attacks_left() - 1);
	VALIDATE(a_.weapon_ < static_cast<int>(a_.get_unit().attacks().size()), _("An invalid attacker weapon got selected."));
	a_.get_unit().set_movement(a_.get_unit().movement_left() - a_.get_unit().attacks()[a_.weapon_].movement_used());

	const rect_of_hexes& draw_area = gui_.draw_area();
	//
	// duel
	//
	int random = get_random();

	bool dueled = false;
	if (duel_) {
		hero& left = heros_[duel_["left"].to_int()];
		hero& right = heros_[duel_["right"].to_int()];
		int percentage = duel_["percentage"].to_int();
		
		unit& left_u = *find_unit(units_, left);
		unit& right_u = *find_unit(units_, right);

		if (point_in_rect_of_hexes(left_u.get_location().x, left_u.get_location().y, draw_area) || point_in_rect_of_hexes(right_u.get_location().x, right_u.get_location().y, draw_area)) {
			utils::string_map symbols;
			symbols["second"] = right.name();
			std::string message;
			if (percentage > 50) {
				message = vgettext("I take a duel with $second, and victory.", symbols);
			} else if (percentage < 50) {
				message = vgettext("I take a duel with $second, but defeated.", symbols);
			} else {
				message = vgettext("I take a duel with $second, and tied.", symbols);
			}
			game_events::show_hero_message(&left, NULL, message, game_events::INCIDENT_DUEL);
		}

		post_duel(left_u, left, right_u, right, percentage);
		dueled = true;
	}

	int duel_mode = controller_.duel();
	while (duel_mode != NO_DUEL && !controller_.is_replaying() && !network::nconnections()) {
		if (attacker_is_artifical || defender_is_artifical || defender_is_commoner) {
			break;
		}

		bool attacker_is_human = attacker.human();
		if (!attacker_is_human && !defender.human()) {
			break;
		}
		if (duel_mode != ALWAYS_DUEL) {
			if (random % 5) {
				break;
			}
			if (attacker.hitpoints() <= gui2::tduel::min_hp_ || defender.hitpoints() <= gui2::tduel::min_hp_) {
				break;
			}
		}
		std::vector<hero*> a;
		a.push_back(&attacker.master());
		if (attacker.second().valid()) a.push_back(&attacker.second());
		if (attacker.third().valid()) a.push_back(&attacker.third());

		std::vector<hero*> d;
		d.push_back(&defender.master());
		if (defender.second().valid()) d.push_back(&defender.second());
		if (defender.third().valid()) d.push_back(&defender.third());

		hero& left = attacker_is_human? *a[random % a.size()]: *d[random % d.size()];
		hero& right = attacker_is_human? *d[(random + 1) % d.size()]: *a[random % a.size()];

		if (duel_mode != ALWAYS_DUEL) {
			int v1 = fxptoi9(left.force_);
			int v2 = fxptoi9(right.force_);
			// don't use left/right.force_ directly. because decimal fraction, it will result mistaken.
			int distance = (v1 > v2)? v1 - v2: v2 - v1;
			if (distance > 15) {
				break;
			}
			v1 = fxptoi12(left.skill_[hero_skill_hero]);
			v2 = fxptoi12(right.skill_[hero_skill_hero]);
			distance = (v1 > v2)? v1 - v2: v2 - v1;
			if (distance > 5) {
				break;
			}
		}

		utils::string_map symbols;
		symbols["first"] = attacker_is_human? left.name(): right.name();
		game_events::show_hero_message(attacker_is_human? &left: &right, NULL, vgettext("$first is here, who dares to take a duel with me!", symbols), game_events::INCIDENT_DUEL);

		int hp_percentage;
		{
			gui2::tduel dlg(left, right);
			dlg.show(gui_.video());

			hp_percentage = dlg.hp_percentage();
			gui_.draw();
		}

		// write result to recorder
		recorder.add_duel(left, right, hp_percentage);

		post_duel(attacker_is_human? attacker: defender, left, attacker_is_human? defender: attacker, right, hp_percentage);

		dueled = true;
		break;
	}
	if (dueled) {
		bool should_return = false;
		if (!a_.valid()) {
			new_locs.first = map_location();
			should_return = true;
		} 
		if (!d_.valid()) {
			new_locs.second = map_location();
			should_return = true;
		}
		if (should_return) return new_locs;
	}

	//
	// check reinforce attack
	//
	for (size_t i = 0; i < attacker.adjacent_size_; i ++) {
		unit_map::const_iterator itor = units_.find(attacker.adjacent_[i]);
		if (!itor.valid() || attacker.side() != itor->side()) {
			continue;
		}
		hero& h1 = attacker.master();
		hero* h2 = NULL;
		const unit& that = *itor;
		int united_level = that.character_level(apply_to_tag::UNITED);
		if (united_level) {
			int reinforce_percent = 10 - (5 * united_level) / 100;
			if (!((random + that.hitpoints()) % reinforce_percent)) {
				h2 = that.character_hero(apply_to_tag::UNITED);
			}
		}
		if (!h2 && !((random + that.hitpoints()) % 5)) {
			h2 = &that.master();
			if (!h1.is_consort(*h2) && !h1.is_oath(*h2)) {
				h2 = NULL;
			}
		}
		if (h2) {
			if (point_in_rect_of_hexes(attacker.get_location().x, attacker.get_location().y, draw_area) || point_in_rect_of_hexes(itor->get_location().x, itor->get_location().y, draw_area)) {
				unit_display::global_anim_2(unit_display::ANIM_REINFORCE, h1.image(true), h2->image(true));
			}
			a_.reinforced_ = true;
			break;
		}
	}

	// defender reinforced
	hero* h2 = NULL;
	int protective_level = defender.character_level(apply_to_tag::PROTECTIVE);
	if (protective_level) {
		int reinforce_percent = 10 - (5 * protective_level) / 100;
		if (!((random + defender.hitpoints()) % reinforce_percent)) {
			h2 = defender.character_hero(apply_to_tag::PROTECTIVE);
		}
	}
	if (!h2 && !((random + defender.hitpoints()) % 8)) {
		h2 = defender.can_encourage();
	}
	if (h2) {
		hero* h1 = &defender.master();
		if (point_in_rect_of_hexes(attacker.get_location().x, attacker.get_location().y, draw_area) || point_in_rect_of_hexes(defender.get_location().x, defender.get_location().y, draw_area)) {
			if (h1 != h2) {
				unit_display::global_anim_2(unit_display::ANIM_REINFORCE, h1->image(true), h2->image(true));
			} else {
				unit_display::global_anim_2(unit_display::ANIM_INDIVIDUALITY, h1->image(true), "projectiles/strike-n-3.png");
			}
		}
		d_.reinforced_ = true;
	}

	a_.get_unit().set_state(unit::STATE_NOT_MOVED,false);
	d_.get_unit().set_resting(false);

	// If the attacker was invisible, she isn't anymore!
	a_.get_unit().set_state(unit::STATE_UNCOVERED, true);

	bc_ = new battle_context(units_, a_.get_unit(), d_.get_unit(), a_.weapon_, d_.weapon_);
	a_stats_ = &bc_->get_attacker_stats();
	d_stats_ = &bc_->get_defender_stats();
	if (a_stats_->weapon) {
		a_.weap_id_ = a_stats_->weapon->id();
	}
	if (d_stats_->weapon) {
		d_.weap_id_ = d_stats_->weapon->id();
	}

	try {
		fire_event("attack");
	} catch (attack_end_exception) {
		return new_locs;
	}
	refresh_bc();

	statistics::attack_context attack_stats(a_.get_unit(), d_.get_unit(), a_stats_->chance_to_hit, d_stats_->chance_to_hit);

	{
		// Calculate stats for battle
		combatant attacker(bc_->get_attacker_stats());
		combatant defender(bc_->get_defender_stats());
		attacker.fight(defender,false);
		const double attacker_inflict = static_cast<double>(d_.get_unit().hitpoints()) - defender.average_hp();
		const double defender_inflict = static_cast<double>(a_.get_unit().hitpoints()) - attacker.average_hp();

		attack_stats.attack_expected_damage(attacker_inflict,defender_inflict);
	}

	a_.orig_attacks_ = a_stats_->num_blows;
	if (unit_feature_val2(attacker, hero_featrue_frighten)) {
		d_.orig_attacks_ = 0;
	} else {
		d_.orig_attacks_ = d_stats_->num_blows;
	}
	a_.n_attacks_ = a_.orig_attacks_;
	d_.n_attacks_ = d_.orig_attacks_;
	a_.xp_ = game_config::attack_xp(d_.get_unit().level());
	d_.xp_ = game_config::attack_xp(a_.get_unit().level());

	bool defender_strikes_first = (d_stats_->firststrike && !a_stats_->firststrike);
	unsigned int rounds = std::max<unsigned int>(a_stats_->rounds, d_stats_->rounds) - 1;
	const int attacker_side = a_.get_unit().side();
	const int defender_side = d_.get_unit().side();
	team& attacker_team = teams_[attacker_side - 1];
	team& defender_team = teams_[defender_side - 1];
	bool defender_contain_entertainer = unit_feature_val2(d_.get_unit(), hero_feature_entertainer)? true: false;
	bool has_unit_in_eyeshot = point_in_rect_of_hexes(a_.loc_.x, a_.loc_.y, draw_area) || point_in_rect_of_hexes(d_.loc_.x, d_.loc_.y, draw_area);

	// Play the pre-fight animation
	unit_display::unit_draw_weapon(a_.loc_,a_.get_unit(),a_stats_->weapon,d_stats_->weapon,d_.loc_,&d_.get_unit());

	if (a_stats_->weapon) {
		a_attackpoints_ = a_stats_->weapon->get_specials("attackpoints");
	}
	if (d_stats_->weapon) {
		d_attackpoints_ = d_stats_->weapon->get_specials("attackpoints");
	}

	for (;;) {
		++ abs_n_attack_;

		if (a_.n_attacks_ > 0 && !defender_strikes_first) {
			if (!perform_hit(true, attack_stats)) break;
		}

		// If the defender got to strike first, they use it up here.
		defender_strikes_first = false;
		++abs_n_defend_;

		if (d_.n_attacks_ > 0) {
			if (!perform_hit(false, attack_stats)) break;
		}

		// Continue the fight to death; if one of the units got petrified,
		// either n_attacks or n_defends is -1
		if (rounds > 0 && d_.n_attacks_ == 0 && a_.n_attacks_ == 0) {
			a_.n_attacks_ = a_.orig_attacks_;
			d_.n_attacks_ = d_.orig_attacks_;
			--rounds;
			defender_strikes_first = (d_stats_->firststrike && ! a_stats_->firststrike);
		}

		if (a_.n_attacks_ <= 0 && d_.n_attacks_ <= 0) {
			fire_event("attack_end");
			refresh_bc();
			break;
		}
	}

	// TODO: if we knew the viewing team, we could skip some of these display update
	if (update_att_fog_ && (*resources::teams)[attacker_side - 1].uses_fog()) {
		recalculate_fog(attacker_side);
		if (update_display_) {
			resources::screen->invalidate_all();
			resources::screen->recalculate_minimap();
		}
	}
	if (update_def_fog_ && (*resources::teams)[defender_side - 1].uses_fog()) {
		recalculate_fog(defender_side);
		if (update_display_) {
			resources::screen->invalidate_all();
			resources::screen->recalculate_minimap();
		}
	}

	if (update_minimap_ && update_display_) {
		resources::screen->recalculate_minimap();
	}

	unit_display::unit_sheath_weapon(a_.loc_,a_.valid()?&a_.get_unit():NULL,a_stats_->weapon,
			d_stats_->weapon,d_.loc_,d_.valid()?&d_.get_unit():NULL);

	// move after attack
	// attation! weapon in a_stats_/d_stats is invalid by by this time
	std::set<map_location> invalid_locs;
	invalid_locs.insert(a_.loc_);
	invalid_locs.insert(d_.loc_);
	while (move_ && a_.valid() && a_stats_->weapon && !attacker_is_artifical && !defender_is_artifical && !defender_is_commoner) {
		unit_map::const_iterator a_base_itor = units_.find(a_.loc_, false);
		unit_map::const_iterator d_base_itor = units_.find(d_.loc_, false);
		if ((a_base_itor.valid() && a_base_itor->wall()) || (d_base_itor.valid() && d_base_itor->wall())) {
			break;
		}

		// get move_specials
		unit_ability_list move_specials = a_stats_->weapon->get_specials("move");
		if (!move_specials.cfgs.empty()) {
			std::pair<const config *, unit *>& cfg = move_specials.cfgs.front();
			int grids = cfg.first->get("grids")->to_int();
			int opp_grids = cfg.first->get("opp_grids")->to_int();
			bool drag = cfg.first->get("drag")->to_bool();
			
			map_location to;
			map_location::DIRECTION dir = a_.loc_.get_relative_dir(d_.loc_);
			if (!drag) {
				// first defender, second attacker
				if (!d_.valid()) {
					new_locs.second = map_location();
				} else if (opp_grids) {
					to = d_.loc_.get_direction(dir, opp_grids);
					if (weapon_move(d_.get_unit(), d_.loc_, to, invalid_locs)) {
						new_locs.second = to;
					}
				}
				if (grids && !attacker_is_tower) {
					to = a_.loc_.get_direction(dir, grids);
					if (weapon_move(a_.get_unit(), a_.loc_, to, invalid_locs)) {
						new_locs.first = to;
					}
				}
			} else {
				// first attacker, second defender
				if (grids && !attacker_is_tower) {
					to = a_.loc_.get_direction(dir, grids);
					if (weapon_move(a_.get_unit(), a_.loc_, to, invalid_locs)) {
						new_locs.first = to;
					}
				}
				if (!d_.valid()) {
					new_locs.second = map_location();
				} else if (opp_grids) {
					to = d_.loc_.get_direction(dir, opp_grids);
					if (weapon_move(d_.get_unit(), d_.loc_, to, invalid_locs)) {
						new_locs.second = to;
					} 
				}
			}
			// update both attacker and defender's loc_, in order to follow-up a_/d_.valid().
			a_.loc_ = new_locs.first;
			d_.loc_ = new_locs.second;
		}
		// some unit has been moved, clear status_caches
		unit::clear_status_caches();
		break;
	}
	// attation: a_.loc/d_.loc maybe not initialize value. (if move_specials is triggered.)
	if (a_.valid()) {
		unit& u = a_.get_unit();

		u.set_hide_turns(0);
		u.set_standing();

		if (a_.xp_) {
			u.get_experience(increase_xp::attack_ublock(u, defender_is_artifical), defender_contain_entertainer? a_.xp_ * 2: a_.xp_);
		}
		if (a_.activity_) {
			u.increase_activity(a_.activity_);
		}
		u.cause_damage_ += a_.cause_damage_;
		u.been_damage_ += a_.been_damage_;
		u.defeat_units_ += a_.defeat_units_;

		attacker_team.cause_damage_ += a_.cause_damage_;
		attacker_team.been_damage_ += a_.been_damage_;
		attacker_team.defeat_units_ += a_.defeat_units_;

	} else {
		// must set loc returned is invalid. may result in advance twice!
		new_locs.first = map_location();
	}
	if (d_.valid()) {
		unit& u = d_.get_unit();

		u.set_standing();
		if (d_.xp_) {
			u.get_experience(increase_xp::attack_ublock(u, attacker_is_artifical), d_.xp_);
		}
		if (d_.activity_) {
			u.increase_activity(d_.activity_);
		}
		if (u.is_city()) {
			unit_2_artifical(&u)->increase_police(-2);
		}
		u.cause_damage_ += d_.cause_damage_;
		u.been_damage_ += d_.been_damage_;
		u.defeat_units_ += d_.defeat_units_;

		defender_team.cause_damage_ += d_.cause_damage_;
		defender_team.been_damage_ += d_.been_damage_;
		defender_team.defeat_units_ += d_.defeat_units_;

	} else {
		new_locs.second = map_location();
	}

	if (update_display_) {
		resources::screen->invalidate_unit();
		resources::screen->invalidate(invalid_locs);
		bool attacker_is_ai = (*resources::teams)[attacker_side - 1].is_ai() || resources::controller->is_replaying();
		if (!attacker_is_ai || has_unit_in_eyeshot) {
			resources::screen->draw(true, true);
		}
	}

	if (OOS_error_) {
		replay::process_error(errbuf_.str());
	}

	std::vector<team>& teams = *resources::teams;
	game_display& disp = *resources::screen;
	if (!resources::controller->is_replaying()) {
		int r = rand();
		if (!(r & 0x3f)) {
			get_random_card((r & 1)? teams[attacker_side - 1]: teams[defender_side - 1], disp, units_, heros_);
		}
	}

	return new_locs;
}


int village_owner(const map_location& loc, const std::vector<team>& teams)
{
	for(size_t i = 0; i != teams.size(); ++i) {
		if(teams[i].owns_village(loc))
			return i;
	}

	return -1;
}

bool get_village(const map_location& loc, int side, int *action_timebonus)
{
	std::vector<team> &teams = *resources::teams;
	team *t = unsigned(side - 1) < teams.size() ? &teams[side - 1] : NULL;
	if (t && t->owns_village(loc)) {
		return false;
	}

	const bool has_leader = resources::units->side_survived(side);
	bool grants_timebonus = false;

	// We strip the village off all other sides, unless it is held by an ally
	// and we don't have a leader (and thus can't occupy it)
	for(std::vector<team>::iterator i = teams.begin(); i != teams.end(); ++i) {
		int i_side = i - teams.begin() + 1;
		if (!t || has_leader || t->is_enemy(i_side)) {
			i->lose_village(loc);
			if (side != i_side && action_timebonus) {
				grants_timebonus = true;
			}
		}
	}

	if (!t) return false;

	if(grants_timebonus) {
		t->set_action_bonus_count(1 + t->action_bonus_count());
		*action_timebonus = 1;
	}

	if(has_leader) {
		if (resources::screen != NULL) {
			resources::screen->invalidate(loc);
		}
		return t->get_village(loc);
	}

	return false;
}

// Simple algorithm: no maximum number of patients per healer.
void reset_resting(unit_map& units, int side)
{
	for (unit_map::iterator i = units.begin(); i != units.end(); ++i) {
		if (i->side() == side)
			i->set_resting(true);
	}
}

/* Contains all the data used to display healing */
struct unit_healing_struct {
	unit *healed;
	std::vector<unit *> healers;
	int healing;
};

void calculate_healing(int side, bool update_display)
{
	game_display& gui = *resources::screen;
	std::vector<team>& teams = *resources::teams;

	std::list<unit_healing_struct> l;

	// We look for all allied units, then we see if our healer is near them.

	for (size_t n = 0; n < teams.size(); n ++) {
		team& t = teams[n];
		if (n != side - 1 && t.is_enemy(side)) {
			continue;
		}

		const std::pair<unit**, size_t> p = t.field_troop();
		unit** troops = p.first;
		size_t troops_vsize = p.second;
		for (size_t i = 0; i < troops_vsize; i ++) {
			unit& u = *(troops[i]);
			if (u.is_artifical()) {
				continue;
			}

			if (u.get_state("unhealable") || u.incapacitated())
				continue;

			unit* surveillanced = NULL;
			std::vector<unit *> healers;

			int healing = 0;

			bool curing = false;
			bool surveillancing = false;

			unit_ability_list heal = u.get_abilities("heals");

			const bool is_poisoned = u.get_state(unit::STATE_POISONED);

			// For heal amounts and surveillance, only consider healers on side which is starting now.
			// Remove all healers not on this side.
			for (std::vector<std::pair<const config *, unit *> >::iterator h_it =
					heal.cfgs.begin(); h_it != heal.cfgs.end();) {

				unit* potential_healer = h_it->second;
				if (potential_healer->side() != side) {
					h_it = heal.cfgs.erase(h_it);
				} else {
					++h_it;
				}
			}
			for (std::vector<std::pair<const config *, unit *> >::const_iterator
					heal_it = heal.cfgs.begin(); heal_it != heal.cfgs.end(); ++heal_it) {

				if ((*heal_it->first)["surveillanced"].to_bool()) {
					surveillanced = heal_it->second;
					// Full curing either occurs on the healer turn
					surveillancing = true;
					break;
				}
			}

			unit_abilities::effect heal_effect(heal, 0, false);
			healing = heal_effect.get_composite_value() * t.cooperate_increase_ / 100;
			if (is_poisoned && healing > 0) {
				curing = true;
				healing = 0;
			}

			for (std::vector<unit_abilities::individual_effect>::const_iterator heal_loc = heal_effect.begin(); heal_loc != heal_effect.end(); ++heal_loc) {
				if (std::find(healers.begin(), healers.end(), heal_loc->u) == healers.end()) {
					healers.push_back(heal_loc->u);
				}
			}

			if (u.side() == side) {
				if (int h = resources::game_map->gives_healing(u.get_location())) {
					if (is_poisoned) {
						curing = true;
					} else {
						if (h > healing) {
							healing = h;
							healers.clear();
						}
					}
				}
				if (!curing && u.resting()) {
					healing += game_config::rest_heal_amount;
				}

				// field troop, decrease loyalty
				int keep_turns = u.keep_turns();
				if (keep_turns > 0) {
					u.set_keep_turns(-- keep_turns);
				} else if (!surveillancing && !u.is_commoner()) {
					u.increase_loyalty(game_config::field_troop_increase_loyalty);
				}
			}
			if (is_poisoned && curing) {
				u.set_state(unit::STATE_POISONED, false);
				unit_display::unit_text(u, false, "");
			} else if (is_poisoned) {
				if (u.side() == side) {
					healing -= game_config::poison_amount;
				}
			}

			if (!curing && healing == 0) {
				continue;
			}

			int pos_max = u.max_hitpoints() - u.hitpoints();
			int neg_max = -(u.hitpoints() - 1);
			if (healing > 0 && pos_max <= 0) {
				// Do not try to "heal" if HP >= max HP
				continue;
			}

			healing += (healing * u.character_level(apply_to_tag::CHARISMATIC)) / 300;
			if (healing > pos_max) {
				healing = pos_max;
			} else if(healing < neg_max) {
				healing = neg_max;
			}

			if (!healers.empty()) {
				DBG_NG << "Just before healing animations, unit has " << healers.size() << " potential healers\n";
			}


			if (!recorder.is_skipping() && update_display &&
				!(u.invisible(u.get_location()) &&
				  teams[gui.viewing_team()].is_enemy(side)))
			{
				unit_healing_struct uhs = { &u, healers, healing };
				l.push_front(uhs);
			}
			if (healing > 0) {
				u.heal(healing);
			} else if (healing < 0) {
				u.take_hit(-healing);
			}
			gui.invalidate_unit();


			for (std::vector<unit*>::iterator it = healers.begin(); it != healers.end(); ++ it) {
				unit& healer = **it;
				healer.get_experience(increase_xp::attack_ublock(healer), 2);
				dialogs::advance_unit(healer, !teams[healer.side() - 1].is_human(), true);
			}
		}
	}

	// Display healing with nearest first algorithm.
	if (!l.empty()) {

		// The first unit to be healed is chosen arbitrarily.
		unit_healing_struct uhs = l.front();
		l.pop_front();

		unit_display::unit_healing(*uhs.healed, uhs.healed->get_location(),
			uhs.healers, uhs.healing);

		// next unit to be healed is nearest from uhs left in list l 
		while (!l.empty()) {

			std::list<unit_healing_struct>::iterator nearest;
			int min_d = INT_MAX;

			// for each unit in l, remember nearest 
			for (std::list<unit_healing_struct>::iterator i =
			     l.begin(), i_end = l.end(); i != i_end; ++i)
			{
				int d = distance_between(uhs.healed->get_location(), i->healed->get_location());
				if (d < min_d) {
					min_d = d;
					nearest = i;
				}
			}

			uhs = *nearest;
			l.erase(nearest);

			unit_display::unit_healing(*uhs.healed, uhs.healed->get_location(),
				uhs.healers, uhs.healing);
		}
	}
}

int maximun_road_damage = 50;
void road_guarding(unit_map& units, std::vector<team>& teams, const std::map<map_location, std::vector<std::pair<artifical*, artifical*> > >& road_locs, int side)
{
	team& current_team = teams[side - 1];
	std::map<unit*, int> touched;
	unit_map::iterator u_it;

	for (std::map<map_location, std::vector<std::pair<artifical*, artifical*> > >::const_iterator it = road_locs.begin(); it != road_locs.end(); ++ it) {
		const map_location& loc = it->first;
		const std::vector<std::pair<artifical*, artifical*> >& v = it->second;

		for (std::vector<std::pair<artifical*, artifical*> >::const_iterator it2 = v.begin(); it2 != v.end(); ++ it2) {
			int a_side = it2->first->side();
			int b_side = it2->second->side();
			if (a_side == b_side && a_side == side) {
				u_it = units.find(loc);
				if (u_it != units.end() && current_team.is_enemy(u_it->side())) {
					unit& u = *u_it;
					size_t a_distance = distance_between(loc, it2->first->get_location());
					size_t b_distance = distance_between(loc, it2->second->get_location());
					// at middle, max damage.
					int short_dist = a_distance <= b_distance? a_distance: b_distance;
					int damage = maximun_road_damage * short_dist / ((a_distance + b_distance) / 2);
					touched.insert(std::make_pair(&u, damage));
				}
			}
		}
	}
	if (!touched.empty()) {
		multi_attack(units, NULL, "", touched, side);
	}
}

unit* get_advanced_unit(const unit* u, const std::string& advance_to, bool real)
{
	const unit_type *new_type = unit_types.find(advance_to);
	if (!new_type) {
		throw game::game_error("Could not find the unit being advanced"
			" to: " + advance_to);
	}
	unit* new_unit;
	if (!u->is_artifical()) {
		new_unit = new unit(*u);
	} else {
		new_unit = new artifical(*(const_unit_2_artifical(u)));
	}
	if (real) {
		new_unit->get_experience(increase_xp::generic_ublock(), -new_unit->max_experience());
	} else {
		new_unit->set_special(new_type->especial());
	}
	new_unit->advance_to(new_type);
	new_unit->calculate_5fields();
	new_unit->modify_according_to_hero(false);
	if (real) {
		if (!u->is_artifical()) {
			new_unit->heal(new_unit->max_hitpoints() / 2);
		} else {
			new_unit->heal_all();
		}
	}
	return new_unit;
}

void get_advanced_unit2(unit* u, const std::string& advance_to)
{
	const unit_type *new_type = unit_types.find(advance_to);
	if (!new_type) {
		throw game::game_error("Could not find the unit being advanced"
			" to: " + advance_to);
	}
	u->get_experience(increase_xp::generic_ublock(), -u->max_experience());
	bool fill_up_movement = u->movement_left() == u->total_movement();
	u->advance_to(new_type);
	u->calculate_5fields();
	u->modify_according_to_hero(false, fill_up_movement);
	u->heal(u->max_hitpoints() / 2);
}

void advance_unit(unit& u, const std::string &advance_to)
{
	bool unit_is_packed = u.packed();
	// original_type is not a reference, since the unit may disappear at any moment.
	std::string original_type = u.type_id();

	game_events::fire("advance", u.get_location());

	if (u.experience() < u.max_experience() || u.type_id() != original_type) {
		return;
	}

	get_advanced_unit2(&u, advance_to);

	if (unit_is_packed) {
		u.pack_to(unit_types.find(original_type));
	}
	
	game_events::fire("post_advance", u.get_location());
}

int combat_modifier(const map_location &loc,
	unit_type::ALIGNMENT alignment, bool is_fearless)
{
	const time_of_day &tod = resources::tod_manager->time_of_day_at(loc);

	int bonus;
	int lawful_bonus = tod.lawful_bonus;
	int liminal_bonus = tod.liminal_bonus;

	switch(alignment) {
		case unit_type::LAWFUL:
			bonus = lawful_bonus;
			break;
		case unit_type::NEUTRAL:
			bonus = 0;
			break;
		case unit_type::CHAOTIC:
			bonus = -lawful_bonus;
			break;
		case unit_type::LIMINAL:
			bonus = liminal_bonus;
			break;
		default:
			bonus = 0;
	}

	if(is_fearless)
		bonus = std::max<int>(bonus, 0);

	return bonus;
}

namespace {

	bool clear_shroud_loc(team &tm,
			const map_location& loc,
			std::vector<map_location>* cleared)
	{
		gamemap &map = *resources::game_map;
		bool result = false;
		map_location adj[7];
		get_adjacent_tiles(loc,adj);
		adj[6] = loc;
		bool on_board_loc = map.on_board(loc);
		for(int i = 0; i != 7; ++i) {

			// We clear one past the edge of the board, so that the half-hexes
			// at the edge can also be cleared of fog/shroud.
			if (on_board_loc || map.on_board_with_border(adj[i])) {
				// Both functions should be executed so don't use || which
				// uses short-cut evaluation.
				const bool res = tm.clear_shroud(adj[i]) | tm.clear_fog(adj[i]);

				if(res) {
					if(res) {
						result = true;
						// If we're near the corner it might be the corner also needs to be cleared
						// this always happens at the lower left corner and depending on the with
						// at the upper or lower right corner.
						if(adj[i].x == 0 && adj[i].y == map.h() - 1) { // Lower left corner
							const map_location corner(-1 , map.h());
							tm.clear_shroud(corner);
							tm.clear_fog(corner);
						} else if(map.w() % 2 && adj[i].x == map.w() - 1 && adj[i].y == map.h() - 1) { // Lower right corner
							const map_location corner(map.w() , map.h());
							tm.clear_shroud(corner);
							tm.clear_fog(corner);
						} else if(!(map.w() % 2) && adj[i].x == map.w() - 1 && adj[i].y == 0) { // Upper right corner
							const map_location corner(map.w() , -1);
							tm.clear_shroud(corner);
							tm.clear_fog(corner);
						}
						if(cleared) {
							cleared->push_back(adj[i]);
						}
					}
				}
			}
		}

		return result;
	}

	/**
	 * Returns true if some shroud is cleared.
	 * seen_units will return new units that have been seen by this unit.
	 * If known_units is NULL, seen_units can be NULL and will not be changed.
	 */
	bool clear_shroud_unit(const map_location &loc, int side,
			const std::set<map_location>* known_units = NULL,
			std::set<map_location>* seen_units = NULL,
			std::set<map_location>* petrified_units = NULL)
	{
		std::vector<map_location> cleared_locations;
		team& tm = (*resources::teams)[side - 1];
		unit_map& units = *resources::units;

		unit_map::iterator u = units.find(loc);
		if (!u.valid()) {
			u = units.find(loc, false);
		}
		unit& un = *u;
		if (!u.valid()) {
			return false;
		}

		if (un.is_artifical() || unit_feature_val2(un, hero_feature_scout)) {
			un.set_state(unit::STATE_LEGERITIED, true);
			if (un.is_artifical()) {
				un.set_movement(2);
			}
		}

		pathfind::paths p(*resources::game_map, *resources::units, loc, *resources::teams, true, false, tm, 0, false, true);
		foreach (const pathfind::paths::step &dest, p.destinations) {
			clear_shroud_loc(tm, dest.curr, &cleared_locations);
		}

		if (un.is_artifical() || unit_feature_val2(un, hero_feature_scout)) {
			un.set_state(unit::STATE_LEGERITIED, false);
			if (un.is_artifical()) {
				un.set_movement(0);
			}
		}

		// clear_shroud_loc is supposed not introduce repetition in cleared_locations
		for(std::vector<map_location>::const_iterator it =
				cleared_locations.begin(); it != cleared_locations.end(); ++it) {

			const unit_map::const_iterator sighted = resources::units->find(*it);
			if (sighted.valid() &&
			    (!sighted->invisible(*it)
			     || !tm.is_enemy(sighted->side())))
			{
				//check if we know this unit, but we always know ourself
				//just in case we managed to move on a fogged hex (teleport)
				if(seen_units != NULL && known_units != NULL
						&& known_units->count(*it) == 0 && *it != loc && *it == sighted->get_location()) {
					if (!(sighted->get_state(unit::STATE_PETRIFIED)))
					{
						seen_units->insert(*it);
					}
					else if (petrified_units != NULL)
					{
						petrified_units->insert(*it);
					}
				}
			}
		}

		return cleared_locations.empty() == false;
	}

}

void recalculate_fog(int side)
{
	team &tm = (*resources::teams)[side - 1];

	if (!tm.uses_fog())
		return;

	tm.refog();

	for (unit_map::iterator i = resources::units->begin(); i != resources::units->end(); ++i)
	{
		if (i->side() == side) {
			const unit_movement_resetter move_resetter(*i);

			clear_shroud_unit(i->get_location(), side);
		}
	}

	//FIXME: This pump don't catch any sighted events (they are not fired by
	// clear_shroud_unit) and if it caches another old event, maybe the caller
	// don't want to pump it here
	game_events::pump();
}

bool clear_shroud(int side)
{
	team &tm = (*resources::teams)[side - 1];
	if (!tm.uses_shroud() && !tm.uses_fog())
		return false;

	bool result = false;

	unit_map::iterator i;
	for (i = resources::units->begin(); i != resources::units->end(); ++i) {
		if (i->side() == side) {
			const unit_movement_resetter move_resetter(*i);

			result |= clear_shroud_unit(i->get_location(), side);
		}
	}

	//FIXME: This pump don't catch any sighted events (they are not fired by
	// clear_shroud_unit) and if it caches another old event, maybe the caller
	// don't want to pump it here
	game_events::pump();

	if (tm.uses_fog()) {
		recalculate_fog(side);
	}

	resources::screen->labels().recalculate_labels();
	resources::screen->labels().recalculate_shroud();

	return result;
}

size_t move_unit(move_unit_spectator* move_spectator,
		 const std::vector<map_location>& route,
		 replay* move_recorder, undo_list* undo_stack,
		 bool show_move,
		 map_location* next_unit, bool continue_move,
		 bool should_clear_shroud, bool is_replay)
{
	VALIDATE(route.size() > 2 || route.front() != route.back(),  "Ignore an unit trying to jump on its hex");

	// Stop the user from issuing any commands while the unit is moving
	const events::command_disabler disable_commands;

	gamemap& map = *resources::game_map;
	unit_map& units = *resources::units;
	hero_map& heros = *resources::heros;
	std::vector<team>& teams = *resources::teams;
	game_display& disp = *resources::screen;

	unit_map::iterator ui = units.find(route.front());
	bool mover_is_commoner = ui->is_commoner();

	if (mover_is_commoner) {
		VALIDATE(ui->task() != unit::TASK_NONE, "move_unit, field commoner's task must not be TASK_NONE!");
	}

	bool spectator_in_mover = (move_spectator && move_spectator == &ui->get_move_spectator())? true: false;

	// get city-owner grid condition. move_unit is called must in expediting state used for expedit.  
	void* expediting_city_cookie = NULL;
	if (units.expediting()) {
		expediting_city_cookie = units.expediting_city_node();
	}

	// don't modify goto if we're have a spectator
	// if it is present, then the caller code is responsible for modifying gotos
	if (move_spectator==NULL) {
		// ui->set_goto(map_location());
	}

	size_t team_num = ui->side()-1;
	team* tm = &teams[team_num];
	play_controller* controller = resources::controller;
	bool unit_is_ai = tm->is_ai() || controller->is_replaying();

	const bool check_shroud = should_clear_shroud && tm->auto_shroud_updates() &&
		(tm->uses_shroud() || tm->uses_fog());

	std::set<map_location> known_units;
	if (check_shroud) {
		for(unit_map::const_iterator u = units.begin(); u != units.end(); ++u) {
			if (!tm->fogged(u->get_location())) {
				known_units.insert(u->get_location());
				tm->see(u->side() - 1);
			}
		}
	}

	// See how far along the given path we can move.
	const int starting_moves = ui->movement_left();
	int moves_left = starting_moves;
	std::set<map_location> seen_units;
	std::set<map_location> petrified_units;
	bool discovered_unit = false;
	unit* alerted_unit = NULL;
	bool should_clear_stack = false;
	std::vector<map_location>::const_iterator step;
	std::string ambushed_string;

	pathfind::last_location = *route.begin();
	for (step = route.begin()+1; step != route.end(); ++step) {
		if (expediting_city_cookie && units.get_cookie(*step) == expediting_city_cookie) {
			pathfind::last_location = *step;
			continue;
		}
		const bool skirmisher = ui->get_ability_bool("skirmisher", *step);

		int cost;
		unit_map::node* curr_node = reinterpret_cast<unit_map::node*>(units.get_cookie(*step, false));
		if (curr_node && curr_node->second->wall()) {
			const unit* w = curr_node->second;
			cost = pathfind::location_cost(units, *tm, *ui, tm->is_enemy(w->side()), false);
		} else {
			const t_translation::t_terrain terrain = map[*step];
			cost = ui->movement_cost(terrain);
		}
		
		//check whether a unit was sighted and whether it should interrupt move
		bool sighted_interrupts = false;
		if (continue_move == false && preferences::interrupt_when_ally_sighted() == false) {
			//check whether any sighted unit is an enemy
			for (std::set<map_location>::iterator it = seen_units.begin(); it != seen_units.end(); ++it)
			{
				const unit_map::const_iterator u = units.find(*it);

				// Unit may have been removed by an event.
				if(u == units.end()) {
					DBG_NG << "was removed\n";
					continue;
				}

				if (tm->is_enemy(u->side())) {
					sighted_interrupts = true;
					break;
				}
			}
		} else {
			sighted_interrupts = seen_units.empty() == false; //interrupt if any unit was sighted
		}

		if (cost >moves_left || discovered_unit || alerted_unit || (continue_move == false && sighted_interrupts)) {
			// if (!skirmisher) {
				break; // not enough MP or spotted new enemies
			// }
		}

		const unit_map::const_iterator enemy_unit = units.find(*step);
		if (enemy_unit != units.end()) {
			if (tm->is_enemy(enemy_unit->side())) {
				if (move_spectator != NULL) {
					move_spectator->set_ambusher(enemy_unit);
				}
				// can't traverse enemy (bug in fog or pathfinding?)
				should_clear_stack = true; // assuming that this enemy was hidden somehow
				break;
			}
		}

		moves_left -= cost;

		// If we use fog or shroud, see if we have sighted an enemy unit,
		// in which case we should stop immediately.
		// Cannot use check shroud, because also need to check if delay shroud is on.
		if (should_clear_shroud && (tm->uses_shroud() || tm->uses_fog())) {
			//we don't want to interrupt our move when we are on an other unit
			//or a uncaptured village (except if it was our plan to end there)
			if (units.count(*step) == 0 &&
			    (!map.is_village(*step) || tm->owns_village(*step) || step + 1 == route.end()))
			{
				DBG_NG << "checking for units from " << (step->x+1) << "," << (step->y+1) << "\n";

				// Temporarily reset the unit's moves to full
				const unit_movement_resetter move_resetter(*ui);

				// We have to swap out any unit that is already in the hex,
				// so we can put our unit there, then we'll swap back at the end.
				unit temp_unit(*ui);
				const temporary_unit_placer unit_placer(units, *step, &temp_unit);
				if (tm->auto_shroud_updates()) {
					should_clear_stack |= clear_shroud_unit(*step,
						ui->side(), &known_units, &seen_units, &petrified_units);
				} else {
					clear_shroud_unit(*step, ui->side(),
						&known_units, &seen_units, &petrified_units);
				}
				if (should_clear_stack) {
					disp.invalidate_all();
				}
			}
		}
		// we also refreh its side, just in case if an event change it
		team_num = ui->side()-1;
		tm = &teams[team_num];

		if (!skirmisher && pathfind::enemy_zoc(teams, *step, *tm, ui->side())) {
			moves_left = 0;
		}

		// check alert attack
		if (!enemy_unit.valid()) {
			alerted_unit = loc_in_alert_area(*ui, pathfind::last_location, *step);
			if (alerted_unit) {
				should_clear_stack = true;
				moves_left = 0;
				alerted_unit->set_alert_turns(0);
				if (move_spectator != NULL) {
					move_spectator->set_ambusher(units.find(alerted_unit->get_location()));
				}
				ambushed_string = _("Alerted!");
			}
		}

		// Check if we have discovered an invisible enemy unit
		map_location adjacent[6];
		get_adjacent_tiles(*step,adjacent);

		for (int i = 0; i != 6; ++i) {
			// Check if we are checking ourselves
			if (adjacent[i] == ui->get_location())
				continue;

			const unit_map::const_iterator it = units.find(adjacent[i]);
			if (it != units.end() && tm->is_enemy(it->side()) && it->invisible(it->get_location())) {
				discovered_unit = true;
				should_clear_stack = true;
				moves_left = 0;
				if (it->hide_turns()) {
					it->set_hide_turns(0);
				}
				if (move_spectator!=NULL) {
					move_spectator->set_ambusher(it);
				}

				unit_ability_list hides = it->get_abilities("hides");

				std::vector<std::pair<const config *, unit *> >::const_iterator hide_it = hides.cfgs.begin();
				// we only use the first valid alert message
				for(;hide_it != hides.cfgs.end() && !ambushed_string.empty(); ++hide_it) {
					ambushed_string = (*hide_it->first)["alert"].str();
				}
			}
		}
		pathfind::last_location = *step;
	}

	// Make sure we don't tread on another unit.
	std::vector<map_location>::const_iterator begin = route.begin();

	// step: 
	//  1. generally, step is equal to route.end();
	//  2. once stop at midway, step is next loc of resulting to stop;
	//  three case may result stop at midway. 
	//  1)movement isn't enough to; 
	//  2)ambushed;
	//  3)seen ally or enemy unit;

	// if possible, get rid of back locs. 
	// strategy:
	//   get rid until coming up against 'valid' loc. 'valid' loc:
	//   1)no unit on this loc;
	//   2)center loc of myself city;
	//   3)other locs of myself city;
	std::vector<map_location> steps(begin, step);

	// why import unremoved_steps?
	// below steps.pop_back maybe shorten steps, it maybe result ui cannot into "target" loc.
	// for example, "target" loc result to alert event, if remove "target", replay will not alert, result to not synchronization.
	// std::vector<map_location> unremoved_steps = steps;

	// size of steps: above for loop, step initializes route.begin() + 1, it will result to sizeo must >= 1.
	// don't be rid of first loc. below std::copy(route.begin() + steps.size() - 1, ...) commands steps not null.
	while (steps.size() >= 2) {
		map_location const &loc = steps.back();
		unit_map::iterator itor = units.find(loc);
		if (!itor.valid() || 
			(unit_is_city(&*itor) && itor->side() == ui->side() && 
			(!expediting_city_cookie || units.get_cookie(loc) != expediting_city_cookie) )) {
			break;
		}
		moves_left += ui->movement_cost(map[loc]);
		steps.pop_back();
	}

	// If we can't get all the way there and have to set a go-to.
	// if (steps.size() != route.size() && discovered_unit == false) {
	if (steps.size() != route.size()) {
		if (move_spectator != NULL) {
			std::vector<map_location> waypoints;
			std::copy(route.begin() + steps.size() - 1, route.end(), std::back_inserter(waypoints));
			move_spectator->set_interrupted_waypoints(waypoints);
		}
		if (seen_units.empty()) {
			// don't modify goto if we're have a spectator
			// if it is present, then the caller code is responsible for modifying gotos
			if (!mover_is_commoner && move_spectator==NULL) {
				// I don't understand why call below, wesnoth call it!
				// But I can sure, commoner cannnot call it. goto_ is fixed to city.
				ui->set_goto(route.back());
			}
		}
	}

	if (steps.size() < 2) {
		if (mover_is_commoner && !units.expediting()) {
			ui->inching_block_turns(true);
			move_recorder->add_inching_block(*ui, true);
		}
		return 0;
	}
	if (mover_is_commoner) {
		ui->inching_block_turns(false);
	}
	
	if (next_unit != NULL) {
		*next_unit = steps.back();
	}

	unit_display::move_unit(steps, *ui, teams, show_move); // show_move maybe false when automicly of human.

	// before moving the real unit, check if it may uncover invisible units
	// and, if any, invalidate their hexes to show their visibility's change
	map_location adjacent[6];
	get_adjacent_tiles(steps.back(), adjacent);
	for (int i = 0; i != 6; ++i) {
		const unit_map::const_iterator it = units.find(adjacent[i]);
		if (it != units.end() && tm->is_enemy(it->side()) &&
				it->invisible(it->get_location()))
			disp.invalidate(adjacent[i]);
	}

	bool event_mutated = false;

	// move the real unit
	// units.move may move ui to city, this will clone into reside troops, set left movement before clone it.
	ui->set_movement(moves_left);
	
	// commoner enter-into city, section I
	artifical* belong_city = units.city_from_cityno(ui->cityno());
	artifical* entered_city = units.city_from_loc(steps.back());
	artifical* target_city = NULL;
	bool forward;
	if (mover_is_commoner && entered_city) {
		unit& commoner = *ui;
		forward = !commoner.get_from().valid();
		if (forward) {
			target_city = entered_city;
		} else {
			target_city = units.city_from_loc(commoner.get_from());
		}
	}

	event_mutated |= !units.move(ui->get_location(), steps.back());
	unit::clear_status_caches();

	ui = units.find(steps.back());
	ui->set_standing();

	if (discovered_unit || alerted_unit) {
		if (discovered_unit) {
			if (!ui->get_state(unit::STATE_SLOWED)) {
				ui->set_state(unit::STATE_SLOWED, true);
			}
		} else {
			ui->set_state(unit::STATE_UNCOVERED, true);
		}
		ui->set_hide_turns(0);
	}

	disp.invalidate_unit_after_move(steps.front(), steps.back());

	int orig_village_owner = -1;
	int action_time_bonus = 0;

	if(map.is_village(steps.back())) {
		orig_village_owner = village_owner(steps.back(),teams);

		if(size_t(orig_village_owner) != team_num) {
			ui->set_movement(0);
			get_village(steps.back(), team_num + 1, &action_time_bonus);
		}
	}


	// Show the final move animation step
	if (!unit_is_ai) {
		disp.draw();
	}

	if ( teams[ui->side()-1].uses_shroud() || teams[ui->side()-1].uses_fog())
	{
		std::set<map_location>::iterator sight_it;
		const std::string sighted_str("sighted");
		// Fire sighted event here
		for (sight_it = seen_units.begin();
				sight_it != seen_units.end(); ++sight_it)
		{
			game_events::raise(sighted_str,*sight_it,steps.back());
		}

		for (sight_it = petrified_units.begin();
				sight_it != petrified_units.end(); ++sight_it)
		{
			game_events::raise(sighted_str,*sight_it,steps.back());
		}
	}

	artifical* to_city = units.city_from_loc(steps.back());
	if (to_city) {
		map_location loc2(MAGIC_RESIDE, to_city->reside_troops().size() - 1);
		game_events::raise("comeinto", steps.back(), loc2);
	} else {
		game_events::raise("moveto", steps.back(), steps.front());
	}

	event_mutated |= game_events::pump();

	//NOTE: an wml event may have removed the unit pointed by ui
	unit_map::iterator maybe_ui = units.find(steps.back());
	// and always disable the previous iterator
	ui = units.end();

	// mover maybe come into city. In that case, don't update spectator that is part of mover.
	if (move_spectator && (!spectator_in_mover || !units.city_from_loc(steps.back()))) {
		move_spectator->set_unit(maybe_ui);
	}

	if (undo_stack != NULL) {
		should_clear_stack = should_clear_shroud && (tm->uses_shroud() || tm->uses_fog());
		if (event_mutated || should_clear_stack || maybe_ui == units.end()) {
			apply_shroud_changes(*undo_stack, team_num + 1);
		}
		undo_stack->clear();
		// 正移动时，鼠标右键的“撤消”功能需要undo_stack中单元，即使打开了战争迷雾/黑幕
		// 至此，“撤消”是否有效不能根据undo_stack是否为空，而是强制就不让出现该按钮
		if (units.city_from_loc(route.front())) {
			undo_stack->push_back(undo_action(*maybe_ui, steps, starting_moves, action_time_bonus, orig_village_owner, units.last_expedite_index()));
		} else {
			undo_stack->push_back(undo_action(*maybe_ui, steps, starting_moves, action_time_bonus, orig_village_owner));
		}
	}

	bool redraw = false;

	// Show messages on the screen here
	if (!ambushed_string.empty()) {
		rect_of_hexes& draw_area = disp.draw_area();
		if (discovered_unit) {
			if (!point_in_rect_of_hexes(steps.back().x, steps.back().y, draw_area)) {
				ambushed_string = "";
			}
		} else if (alerted_unit) {
			if (!point_in_rect_of_hexes(steps.back().x, steps.back().y, draw_area) && !point_in_rect_of_hexes(alerted_unit->get_location().x, alerted_unit->get_location().y, draw_area)) {
				ambushed_string = "";
			}
		}
		// We've been ambushed, display an appropriate message
		disp.announce(ambushed_string, font::BAD_COLOR);
		redraw = true;
	}

	if (continue_move == false && seen_units.empty() == false) {
		// The message depends on how many units have been sighted,
		// and whether they are allies or enemies, so calculate that out here
		int nfriends = 0, nenemies = 0;
		for(std::set<map_location>::const_iterator i = seen_units.begin(); i != seen_units.end(); ++i) {
			DBG_NG << "processing unit at " << (i->x+1) << "," << (i->y+1) << "\n";
			const unit_map::const_iterator u = units.find(*i);

			// Unit may have been removed by an event.
			if(u == units.end()) {
				DBG_NG << "was removed\n";
				continue;
			}

			if (tm->is_enemy(u->side())) {
				++nenemies;
				if (move_spectator!=NULL) {
					move_spectator->add_seen_enemy(u);
				}
			} else {
				++nfriends;
				if (move_spectator!=NULL) {
					move_spectator->add_seen_friend(u);
				}
			}

			DBG_NG << "processed...\n";
			tm->see(u->side() - 1);
		}

		// The message we display is different depending on
		// whether the units sighted were enemies or friends,
		// and their respective number.
		utils::string_map symbols;
		symbols["friends"] = lexical_cast<std::string>(nfriends);
		symbols["enemies"] = lexical_cast<std::string>(nenemies);
		std::string message;
		SDL_Color msg_colour;
		if(nfriends == 0 || nenemies == 0) {
			if(nfriends > 0) {
				message = vngettext("Friendly unit sighted", "$friends friendly units sighted", nfriends, symbols);
				msg_colour = font::GOOD_COLOR;
			} else if(nenemies > 0) {
				message = vngettext("Enemy unit sighted!", "$enemies enemy units sighted!", nenemies, symbols);
				msg_colour = font::BAD_COLOR;
			}
		}
		else {
			symbols["friendphrase"] = vngettext("Part of 'Units sighted! (...)' sentence^1 friendly", "$friends friendly", nfriends, symbols);
			symbols["enemyphrase"] = vngettext("Part of 'Units sighted! (...)' sentence^1 enemy", "$enemies enemy", nenemies, symbols);
			message = vgettext("Units sighted! ($friendphrase, $enemyphrase)", symbols);
			msg_colour = font::NORMAL_COLOR;
		}
/*
		if(steps.size() < route.size()) {
			// See if the "Continue Move" action has an associated hotkey
			const hotkey::hotkey_item& hk = hotkey::get_hotkey(hotkey::HOTKEY_CONTINUE_MOVE);
			if(!hk.null()) {
				symbols["hotkey"] = hk.get_name();
				message += "\n" + vgettext("(press $hotkey to keep moving)", symbols);
			}
		}
*/
		disp.announce(message, msg_colour);
		redraw = true;
	}

	if (!unit_is_ai) {
		disp.recalculate_minimap();
	}
	if (redraw) {
		disp.draw();
	}

	if (move_recorder != NULL) {
		// use unremoved_steps not steps
		if (units.city_from_loc(route.front())) {
			move_recorder->add_expedite(units.last_expedite_troop(), units.last_expedite_index(), steps);
		} else {
			move_recorder->add_movement(steps);
		}
	}

	// commoner enter-into city, section II
	if (mover_is_commoner && entered_city) {
		// do_trade will display animation, it should after units.move. 
		// because in order to display animation right, commoner should stand right poistion, unis.move can it.
		unit& commoner = *entered_city->reside_commoners().back();
		int task = commoner.task();
		VALIDATE(!(task == unit::TASK_BACK && forward), "move_unit, forward commoner's task must not be TASK_BACK!");

		if (task == unit::TASK_TRADE) {
			do_trade(*tm, commoner, *belong_city, *target_city, forward);
			if (forward) {
				controller->commoner_back(entered_city, belong_city, steps.front());
				if (move_spectator && !spectator_in_mover) {
					move_spectator->set_unit(units.find(steps.front()));
				}
				if (commoner.movement_left() > 0) {
					commoner.set_movement(0);
				}
				steps.clear();
			}
		} else if (task == unit::TASK_TRANSFER || task == unit::TASK_BACK) {
			commoner.set_task(unit::TASK_NONE);
		}
	}

	// because undo, can select: 
	// 1)get one card random must before add movement/expedite to recorder. 
	// 2)if get one card, cannot undo.
	// I select 2).
	if (!controller->is_replaying() && alerted_unit) {
		// attack no defend!
		if (!no_fightback_attack(*alerted_unit, *maybe_ui)) {
			move_spectator->set_unit(units.end());
		}
		event_mutated = true;

	} else if (!controller->is_replaying() && !to_city && maybe_ui.valid()) {
		int random = rand();
		unit& mover = *maybe_ui;
		// 1/16---1/8
		int encourage_percent = 16 - (8 * mover.character_level(apply_to_tag::UNITED)) / 100;
		if (!(random & 0x1ff)) {
			get_random_card(*tm, disp, units, heros);
			event_mutated = true;
			
		} else if (random % encourage_percent == 1) {
			// suport to encourage
			if (mover.hitpoints() < mover.max_hitpoints() * 2 / 3) {
				hero* h2 = mover.can_encourage();
				if (h2) {
					// must call add_event(replay::EVENT_ENCOURAGE, ...) before mover.do_encourage.
					// when replaying, increase_feeling in add_event may enquire choice, it is said [input] must after add_event. 
					move_recorder->add_event(replay::EVENT_ENCOURAGE, mover.get_location());
					mover.do_encourage(mover.master(), *h2);
					event_mutated = true;
				}
			}

		} else if (tm->is_human() && !controller->treasures().empty() && ((random % 0xff) == 2)) {
			int pos = random % controller->treasures().size();
			tm->find_treasure(heros, *controller, pos);
			move_recorder->add_event(replay::EVENT_FIND_TREASURE, map_location(pos, -1000));
			event_mutated = true;
		}
	}

	if (event_mutated && undo_stack) {
		undo_stack->clear();
	}

	VALIDATE(steps.size() <= route.size(),  "::move_unit, moved steps.size > route.size!");
	return steps.size();
}

std::pair<map_location, map_location> attack_enemy(unit* a_, unit* d_, int att_weapon, int def_weapon, bool move)
{
	unit_map& units_ = *resources::units;
	std::vector<team>& teams_ = *resources::teams;
	
	const map_location& attacker_loc = a_->get_location();
	const map_location& defender_loc = d_->get_location();

	std::pair<map_location, map_location> to_locs = std::make_pair<map_location, map_location>(attacker_loc, defender_loc);

	if (a_->incapacitated()) {
		return to_locs;
	}

	if (d_->incapacitated()) {
		return to_locs;
	}

	if (!a_->attacks_left()) {
		return to_locs;
	}

	if ((att_weapon < 0) || (att_weapon > static_cast<int>(a_->attacks().size()))) {
		return to_locs;
	}

	// if defender hasn't weapon reattack, def_weapon will be -1.

	//
	// do_execute
	//
	// Stop the user from issuing any commands while the unit is attacking
	const events::command_disabler disable_commands;

	// @note: yes, this is a decision done here. It's that way because we want to allow a simpler attack 'with whatever weapon is considered best', and because we want to allow the defender to pick it's weapon. That's why aggression is needed. a cleaner solution is needed.
	
	//@TODO: change ToD to be location specific for the defender unit
	recorder.add_attack(*a_, *d_, att_weapon, def_weapon, a_->type_id(),
		d_->type_id(), a_->level(), d_->level(), resources::tod_manager->turn(), 
		resources::tod_manager->get_time_of_day(), move);
	rand_rng::invalidate_seed();
	rand_rng::clear_new_seed_callback();
	while (!rand_rng::has_valid_seed()) {
		ai::manager::raise_user_interact();
		ai::manager::raise_sync_network();
		SDL_Delay(10);
	}
	recorder.add_seed("attack", rand_rng::get_last_seed());


	to_locs = attack_unit(*a_, *d_, att_weapon, def_weapon, true, config::invalid, move);

	unit_map::iterator attacker = units_.find(to_locs.first);
	if (attacker != units_.end()) {
		dialogs::advance_unit(*attacker, true);
	}

	unit_map::iterator defender = units_.find(to_locs.second);
	if (defender != units_.end()) {
		const size_t defender_team = size_t(defender->side()) - 1;
		if (defender_team < teams_.size()) {
			dialogs::advance_unit(*defender, !teams_[defender_team].is_human());
		}
	}

	try {
		ai::manager::raise_gamestate_changed();
		resources::controller->check_victory();
		resources::controller->check_end_level();
	} catch (...) {
		throw;
	}
	// reutrn true indicate is_gamestate_changed is true
	return to_locs;
}

bool unit_can_move(const unit &u)
{
	const team &current_team = (*resources::teams)[u.side() - 1];

	if (!u.attacks_left() && u.movement_left()==0)
		return false;

	// Units with goto commands that have already done their gotos this turn
	// (i.e. don't have full movement left) should have red globes.
	if(u.has_moved() && u.has_goto()) {
		return false;
	}

	map_location locs[6];
	get_adjacent_tiles(u.get_location(), locs);
	for(int n = 0; n != 6; ++n) {
		if (resources::game_map->on_board(locs[n])) {
			const unit_map::const_iterator i = resources::units->find(locs[n]);
			if (i.valid()) {
				if(!i->incapacitated() && current_team.is_enemy(i->side())) {
					return true;
				}
			}

			if (u.movement_cost((*resources::game_map)[locs[n]]) <= u.movement_left()) {
				return true;
			}
		}
	}

	return false;
}

bool unit_can_action(const unit &u)
{
	if (!u.attacks_left()) {
		return false;
	}

	if (u.has_moved() && u.has_goto()) {
		return false;
	}

	return true;
}

void apply_shroud_changes(undo_list &undos, int side)
{
	team &tm = (*resources::teams)[side - 1];
	// No need to do this if the team isn't using fog or shroud.
	if (!tm.uses_shroud() && !tm.uses_fog())
		return;

	game_display &disp = *resources::screen;

	/*
	   This function works thusly:
	   1. run through the list of undo_actions
	   2. for each one, play back the unit's move
	   3. for each location along the route, clear any "shrouded" hexes that the unit can see
	      and record sighted events
	   4. render shroud/fog cleared.
	   5. pump all events
	   6. call clear_shroud to update the fog of war for each unit
	   7. fix up associated display stuff (done in a similar way to turn_info::undo())
	*/
	// refog and invalidate stuff
	disp.invalidate_unit();
	disp.invalidate_game_status();
	clear_shroud(side);
	disp.recalculate_minimap();
	disp.invalidate_all();
}

bool backstab_check(const map_location& attacker_loc,
		const map_location& defender_loc,
		const unit_map& units, const std::vector<team>& teams)
{
	const unit_map::const_iterator defender =
		units.find(defender_loc);
	if(defender == units.end()) return false; // No defender

	map_location adj[6];
	get_adjacent_tiles(defender_loc, adj);
	int i;
	for(i = 0; i != 6; ++i) {
		if(adj[i] == attacker_loc)
			break;
	}
	if(i >= 6) return false;  // Attack not from adjacent location

	const unit_map::const_iterator opp =
		units.find(adj[(i+3)%6]);
	if(opp == units.end()) return false; // No opposite unit
	if(opp->incapacitated() || opp->wall()) return false;
	if(size_t(defender->side()-1) >= teams.size() ||
			size_t(opp->side()-1) >= teams.size())
		return true; // If sides aren't valid teams, then they are enemies
	if(teams[defender->side()-1].is_enemy(opp->side()))
		return true; // Defender and opposite are enemies
	return false; // Defender and opposite are friends
}

void refresh_card_button(const team& t, game_display& disp)
{
	std::stringstream strstr;
	theme::menu *theme_b = disp.get_theme().get_menu_item("card");
	gui::button* btn = disp.find_button("card");

	std::stringstream title;
	title << t.holded_cards().size();
	theme_b->set_title(title.str());
	btn->set_label(title.str());

	int size = t.holded_cards().size();
	if (size < game_config::max_cards * 3 / 4) {
		btn->set_color(font::BUTTON_COLOR);
	} else if (size < game_config::max_cards) {
		btn->set_color(font::YELLOW_COLOR);
	} else {
		btn->set_color(font::BAD_COLOR);
	}
}

void get_random_card(team& t, game_display& disp, unit_map& units, hero_map& heros)
{
	bool added = t.add_card(CARDS_INVALID_NUMBER, false, true);
	if (added && t.is_human()) {
		refresh_card_button(t, disp);
		// card
		utils::string_map symbols;
		symbols["first"] = help::tintegrate::generate_format(t.holded_card(t.holded_cards().size() - 1).name(), help::tintegrate::object_color);
		game_events::show_hero_message(&heros[hero::number_scout], NULL, vgettext("Get card: $first.", symbols), game_events::INCIDENT_CARD);
	}
}

void erase_random_card(team& t, game_display& disp, unit_map& units, hero_map& heros)
{
	const std::vector<size_t>& holded_cards = t.holded_cards();
	if (holded_cards.empty()) {
		return;
	}
	int index = rand() % holded_cards.size();
	const card& c = t.holded_card(index);
	t.erase_card(index, false, true);
	if (t.is_human()) {
		refresh_card_button(t, disp);
		// card
		utils::string_map symbols;
		symbols["first"] = c.name();
		game_events::show_hero_message(&heros[hero::number_scout], NULL, vgettext("Lost card: $first.", symbols), game_events::INCIDENT_CARD);
	}
}

artifical* artifical_existed_ea(unit_map& units, const map_location& loc)
{
	unit_map::iterator find = units.find(loc);
	if (!find.valid()) {
		return NULL;
	}
	int master = find->type()->master();
	return hero::is_ea_artifical(master)? unit_2_artifical(&*find): NULL;
}

void calculate_wall_tiles(unit_map& units, gamemap& map, artifical& owner, std::vector<map_location*>& keeps, std::vector<map_location*>& wall_vacants, std::vector<map_location*>& keep_vacants)
{
	keeps.clear();
	wall_vacants.clear();
	keep_vacants.clear();

	const unit_type* wall_type = unit_types.find_wall();
	const unit_type* keep_type = unit_types.find_keep();

	map_location* tiles = owner.adjacent_2_;
	size_t adjacent_size = owner.adjacent_size_2_;

	for (int i = 0; i != adjacent_size; ++ i) {
		// find count of keep
		// find vacant tile
		unit_map::const_iterator itor = units.find(tiles[i]);
		if (itor.valid()) {
			if (itor->type() == keep_type) {
				keeps.push_back(&tiles[i]);
			}
			continue;
		}
		itor = units.find(tiles[i], false);
		if (itor.valid()) {
			if (itor->type() == keep_type) {
				keeps.push_back(&tiles[i]);
			}
			continue;
		}

		if (wall_type->match().empty() || t_translation::terrain_matches(map.get_terrain(tiles[i]), t_translation::t_match(wall_type->match()))) {
			wall_vacants.push_back(&tiles[i]);
		}
		if (keep_type->match().empty() || t_translation::terrain_matches(map.get_terrain(tiles[i]), t_translation::t_match(keep_type->match()))) {
			keep_vacants.push_back(&tiles[i]);
		}
	}
}

size_t calculate_keeps(unit_map& units, const artifical& owner)
{
	size_t keeps = 0;
	const unit_type* keep_type = unit_types.find_keep();

	const map_location* tiles = owner.adjacent_2_;
	size_t adjacent_size = owner.adjacent_size_2_;

	for (int i = 0; i != adjacent_size; ++ i) {
		// find count of keep
		unit_map::const_iterator itor = units.find(tiles[i]);
		if (itor.valid()) {
			if (itor->type() == keep_type) {
				keeps ++;
			}
			continue;
		}
		itor = units.find(tiles[i], false);
		if (itor.valid()) {
			if (itor->type() == keep_type) {
				keeps ++;
			}
			continue;
		}
	}
	return keeps;
}

artifical* find_city_for_wall(unit_map& units, const map_location& loc)
{
	map_offset* adjacent2_ptr = adjacent_2[loc.x & 0x1];
	size_t size2 = (sizeof(adjacent_2) / sizeof(map_offset)) >> 1;
	for (size_t i2 = 0; i2 < size2; i2 ++) {
		artifical* city = units.city_from_loc(map_location(loc.x + adjacent2_ptr[i2].x, loc.y + adjacent2_ptr[i2].y));
		if (city) {
			return city;
		}
	}
	return NULL;
}

void expand_rect_loc(SDL_Rect& rect, const map_location& loc)
{
	if (rect.x != -1) {
		if (loc.x < rect.x) {
			rect.w += rect.x - loc.x;
			rect.x = loc.x;
		} else if (loc.x >= rect.x + rect.w) {
			rect.w = loc.x - rect.x + 1;
		}
		if (loc.y < rect.y) {
			rect.h += rect.y - loc.y;
			rect.y = loc.y;
		} else if (loc.y >= rect.y + rect.h) {
			rect.h = loc.y - rect.y + 1;
		}
	} else {
		rect.x = loc.x;
		rect.y = loc.y;
		rect.w = rect.h = 1;
	}
}

SDL_Rect extend_rectangle(const gamemap& map, const SDL_Rect& src, int radius)
{
	SDL_Rect result;

	int min = src.x - radius;
	int max = src.x + src.w + radius;
	result.x = std::max<int>(0, min);
	result.w = std::min<int>(map.w(), max) - result.x;

	min = src.y - radius;
	max = src.y + src.h + radius;
	result.y = std::max<int>(0, min);
	result.h = std::min<int>(map.h(), max) - result.y;

	return result;
}

SDL_Rect extend_rectangle(const gamemap& map, int x, int y, int radius)
{
	SDL_Rect result;

	int min = x - radius;
	int max = x + radius;
	result.x = std::max<int>(0, min);
	result.w = std::min<int>(map.w(), max) - result.x;

	min = y - radius;
	max = y + radius;
	result.y = std::max<int>(0, min);
	result.h = std::min<int>(map.h(), max) - result.y;

	return result;
}

void post_duel(unit& left_u, hero& left, unit& right_u, hero& right, int percentage)
{
	std::stringstream float_text;
	int kill;
	int left_skill = 150, right_skill = 150;

	if (percentage > 50) {
		left_u.set_state(unit::STATE_BROKEN, false);
		left_u.set_state(unit::STATE_SLOWED, false);

		if (!right_u.get_state(unit::STATE_BROKEN)) {
			float_text << _("broken");
			right_u.set_state(unit::STATE_BROKEN, true);
		}
		if (percentage > 70) {
			if (!right_u.get_state(unit::STATE_SLOWED)) {
				if (!float_text.str().empty()) {
					float_text << '\n';
				}
				float_text << _("slowed");
				right_u.set_state(unit::STATE_SLOWED, true);
			}
		}

		if (percentage == 100) {
			kill = std::min(gui2::tduel::min_hp_, right_u.hitpoints() / 3);
			right_u.take_hit(kill);
			if (!float_text.str().empty()) {
				float_text << '\n';
			}
			float_text << kill;

			left_skill = 400;
			right_skill = 50;
		} else {
			left_skill = 200;
			right_skill = 100;
		}

		if (!float_text.str().empty()) {
			unit_display::unit_text(right_u, true, float_text.str());
		}

	} else if (percentage < 50) {
		right_u.set_state(unit::STATE_BROKEN, false);
		right_u.set_state(unit::STATE_SLOWED, false);

		if (!left_u.get_state(unit::STATE_BROKEN)) {
			float_text << _("broken");
			left_u.set_state(unit::STATE_BROKEN, true);
		}
		if (percentage < 30) {
			if (!left_u.get_state(unit::STATE_SLOWED)) {
				if (!float_text.str().empty()) {
					float_text << '\n';
				}
				float_text << _("slowed");
				left_u.set_state(unit::STATE_SLOWED, true);
			}
		}
		if (percentage == 0) {
			kill = std::min(gui2::tduel::min_hp_, left_u.hitpoints() / 3);
			left_u.take_hit(kill);
			if (!float_text.str().empty()) {
				float_text << '\n';
			}
			float_text << kill;

			left_skill = 50;
			right_skill = 400;
		} else {
			left_skill = 100;
			right_skill = 200;
		}

		if (!float_text.str().empty()) {
			unit_display::unit_text(left_u, true, float_text.str());
		}
	}
	if (unit_feature_val2(left_u, hero_feature_skill)) {
		left_skill *= 2;
	}
	if (unit_feature_val2(right_u, hero_feature_skill)) {
		right_skill *= 2;
	}
	u16_get_experience_i12(&left.skill_[hero_skill_hero], left_skill);
	u16_get_experience_i12(&right.skill_[hero_skill_hero], right_skill);

	config duel;
	duel["left"] = left.number_;
	duel["right"] = right.number_;
	duel["percentage"] = percentage;

	game_events::fire("post_duel", left_u.get_location(), right_u.get_location());

	resources::state_of_game->clear_variable_cfg("duel");
}

int calculate_exploiture(const hero& h1, const hero& h2, int type)
{
	int assistant_ability, skill, exploiture;

	if (h2.valid()) {
		if (type == department::commercial) {
			assistant_ability = fxptoi9(h2.politics_) * 100 / 250; // oath relative
			skill = fxptoi12(h2.skill_[hero_skill_commercial]);
		} else if (type == department::technology) {
			assistant_ability = fxptoi9(h2.intellect_) * 100 / 250;
			skill = fxptoi12(h2.skill_[hero_skill_invent]);
		}
	} else {
		assistant_ability = 0;
		skill = 0;
	}
	if (type == department::commercial) {
		exploiture = fxptoi9(h1.politics_) + assistant_ability;
		skill += fxptoi12(h1.skill_[hero_skill_commercial]);
	} else if (type == department::technology) {
		exploiture = fxptoi9(h1.intellect_) + assistant_ability;
		skill += fxptoi12(h1.skill_[hero_skill_invent]);
	}

	// boath exploiture and skill maybe is 0.
	exploiture = exploiture * (1 + skill) / 5;
	if (exploiture < 80) {
		exploiture = 80;
	} else if (exploiture > 200) {
		exploiture = 200;
	}

	return exploiture;
}

int calculate_disband_income(const unit& u, int cost_exponent, bool hp)
{
	int income = u.cost() * (80 + 20 * u.amla()) * cost_exponent / 10000;
	if (hp) {
		income = income * u.hitpoints() / u.max_hitpoints();
	}
	return income;
}

void do_recruit(unit_map& units, hero_map& heros, std::vector<team>& teams, team& current_team, const unit_type* ut, std::vector<const hero*>& v, artifical& city, int cost, bool human, bool to_recorder)
{
	type_heros_pair pair(ut, v);

	if (to_recorder) {
		recorder.add_recruit(ut->id(), city.get_location(), v, cost, human);
	}

	unit* new_unit = new unit(units, heros, teams, pair, city.cityno(), true);
	new_unit->set_movement(new_unit->total_movement());
	new_unit->set_attacks(new_unit->attacks_total());

	current_team.spend_gold(cost);

	new_unit->set_human(human);
	city.troop_come_into(new_unit, -1, false);

	for (std::vector<const hero*>::const_iterator it = v.begin(); it != v.end(); ++ it) {
		city.hero_go_out(**it);
	}

	map_location loc2(MAGIC_RESIDE, city.reside_troops().size() - 1);
	game_events::fire("post_recruit", city.get_location(), loc2);
}

unit* select_provoke_target(unit_map& units, const team& current_team, unit& tactician, const ttactic& t)
{
	unit* special = NULL;

	const std::map<int, std::vector<map_location> > touched = t.touch_units(units, tactician);
	const std::vector<const ttactic*>& parts = t.parts();
	for (std::map<int, std::vector<map_location> >::const_iterator it = touched.begin(); it != touched.end(); ++ it) {
		const ttactic& part_tactic = *(parts[it->first]);
		if (part_tactic.apply_to() == apply_to_tag::PROVOKE) {
			std::vector<map_location> locs = it->second;
			for (std::vector<map_location>::iterator it2 = locs.begin(); it2 != locs.end();) {
				unit* u = &*find_visible_unit(*it2, current_team);
				if (u->provoked_turns()) {
					it2 = locs.erase(it2);
				} else {
					++ it2;
				}
			}
			if (!locs.empty()) {
				const map_location& loc = locs[rand() % locs.size()];
				special = &*find_visible_unit(loc, current_team);
			} else if (!it->second.empty()) {
				// all enemy has been provoked, select first.
				special = &*find_visible_unit(it->second[0], current_team);
			}
			break;
		}
	}
	return special;
}

void cast_tactic(std::vector<team>& teams, unit_map& units, unit& tactician, hero& h, unit* special, bool to_recorder, bool consume)
{
	team& current_team = teams[h.side_];
	const ttactic& t = unit_types.tactic(h.tactic_);
	const std::map<int, std::vector<map_location> > touched = t.touch_units(units, tactician);
	int part, turn;

	if (t.select_one() && !special) {
		special = select_provoke_target(units, current_team, tactician, t);
	}

	if (to_recorder) {
		recorder.add_cast_tactic(tactician, h, special, consume);
		// must called before tactician.get_experience(2).
		// tactician.get_experience(2) may generate [input](select human troop),
		// replay should known [input] is generate by cast_tactic.
	}

	const rect_of_hexes& draw_area = resources::screen->draw_area();
	if (point_in_rect_of_hexes(tactician.get_location().x, tactician.get_location().y, draw_area)) {
		unit_display::tactic_start(h);
	}
	for (std::map<int, std::vector<map_location> >::const_iterator it = touched.begin(); it != touched.end(); ++ it) {
		const std::vector<const ttactic*>& parts = t.parts();
		part = it->first;
		turn = ttactic::calculate_turn(fxptoi9(h.force_), fxptoi9(h.intellect_));
		const ttactic& part_tactic = *(parts[part]);
	
		if (part_tactic.apply_to() == apply_to_tag::DAMAGE) {
			tactician.apply_tactic(&t, part_tactic, part, turn);
		} else if (part_tactic.apply_to() == apply_to_tag::PROVOKE) {
			if (special) {
				special->do_provoked(tactician, turn);
			}
		} else { 
			for (std::vector<map_location>::const_iterator it2 = it->second.begin(); it2 != it->second.end(); ++ it2) {
				unit* u = find_unit(units, *it2);
				if (!u) {
					// this unit may be erased by previous tactic. for example "damage".
					continue;
				}
				u->apply_tactic(&t, part_tactic, part, turn);
			}
		}
	}

	if (consume) {
		current_team.add_tactic_point(-1 * t.point());
	}

	tactician.get_experience(increase_xp::attack_ublock(tactician), 2);

	if (to_recorder) {
		if (tactician.advances()) {
			dialogs::advance_unit(tactician, !current_team.is_human());
		}
	}
	// some tactic maybe "add/erase" unit on map, for example "damage".
	unit::clear_status_caches();
}

void damage_range(unit_map& units, std::vector<team>& teams, unit& u, const std::string& type, const std::string& relative, int ratio)
{
	team& u_team = teams[u.side() - 1];
	
	unit_map::iterator u_it;
	std::map<unit*, int> touched;

	const map_location* tiles = u.adjacent_;
	size_t adjance_size = u.adjacent_size_;

	for (int step = 0; step < 2; step ++) {
		if (step == 1) {
			tiles = u.adjacent_2_;
			adjance_size = u.adjacent_size_2_;
		}
	
		for (size_t adj = 0; adj != adjance_size; adj ++) {
			const map_location& loc = tiles[adj];
			u_it = find_visible_unit(loc, u_team);
			if (!u_it.valid()) {
				continue;
			}

			if (!u_team.is_enemy(u_it->side())) {
				continue;
			}
			if (touched.find(&*u_it) != touched.end()) {
				continue;
			}
			unit& opp = *u_it;
			const map_location& opp_loc = opp.get_location();
						
			int damage;
			if (relative == "leadership") {
				damage = u.leadership_ - opp.leadership_;
			} else if (relative == "force") {
				damage = u.force_ - opp.force_;
			} else if (relative == "intellect") {
				damage = u.intellect_ - opp.intellect_;
			} else if (relative == "politics") {
				damage = u.politics_ - opp.politics_;
			} else {
				damage = u.charm_ - opp.charm_;
			}
			if (damage < 10) {
				damage = 10;
			} else if (damage > 50) {
				damage = 50;
			}
			damage = damage * ratio / 100;

			int defense = opp.defense_modifier(resources::game_map->get_terrain(opp_loc));
			int multiplier = defense * opp.resistance_against(type, false, opp.get_location());
			damage = round_damage(damage, multiplier, 10000);
			
			touched.insert(std::make_pair(&opp, damage));
		}
	}

	multi_attack(units, &u, type, touched, u.side());
/*	
	std::vector<unit*> def_ptr_vec;
	std::vector<int> damage_vec;
	std::vector<std::string> hit_text_vec;
	for (std::map<unit*, int>::const_iterator it = touched.begin(); it != touched.end(); ++ it) {
		def_ptr_vec.push_back(it->first);
		damage_vec.push_back(it->second);
		hit_text_vec.push_back("");
	}
	unit_display::unit_attack2(u, type, def_ptr_vec, damage_vec, hit_text_vec);

	config dat;
	dat.add_child("first",  config());
	dat.add_child("second", config());
	for (std::map<unit*, int>::const_iterator it = touched.begin(); it != touched.end(); ++ it) {
		unit& opp = *it->first;
		// display animation
		bool dies = opp.take_hit(it->second);
		if (dies) {
			{
				event_lock lock(units, opp);

				game_events::fire("last breath", opp.get_location(), u_loc, dat);
				// recalculate defender_pointer
				if (!lock.valid()) {
					// WML has invalidated the dying unit, abort
					continue;
				}
			}
			unit_display::unit_die(opp.get_location(), opp, NULL, NULL, u.get_location(), &u);
			{
				event_lock lock(units, opp);

				game_events::fire("die", opp.get_location(), u_loc, dat);
				// recalculate defender_pointer
				if (!lock.valid()) {
					// WML has invalidated the dying unit, abort
					continue;
				}
			}
			unit_die(units, opp, NULL, 0, u.side());
		}
	}
*/
}

void multi_attack(unit_map& units, unit* attacker, const std::string& type, const std::map<unit*, int>& touched, int a_side)
{
	if (touched.empty()) {
		return;
	}

	const map_location u_loc = attacker? attacker->get_location(): map_location::null_location;
	std::vector<unit*> def_ptr_vec;
	std::vector<int> damage_vec;
	std::vector<std::string> hit_text_vec;
	for (std::map<unit*, int>::const_iterator it = touched.begin(); it != touched.end(); ++ it) {
		def_ptr_vec.push_back(it->first);
		damage_vec.push_back(it->second);
		hit_text_vec.push_back("");
	}
	unit_display::unit_attack2(attacker, type, def_ptr_vec, damage_vec, hit_text_vec);

	config dat;
	dat.add_child("first",  config());
	dat.add_child("second", config());
	for (std::map<unit*, int>::const_iterator it = touched.begin(); it != touched.end(); ++ it) {
		unit& opp = *it->first;
		// display animation
		bool dies = opp.take_hit(it->second);
		if (dies) {
			{
				event_lock lock(units, opp);

				game_events::fire("last breath", opp.get_location(), u_loc, dat);
				// recalculate defender_pointer
				if (!lock.valid()) {
					// WML has invalidated the dying unit, abort
					continue;
				}
			}
			unit_display::unit_die(opp.get_location(), opp, NULL, NULL, u_loc, attacker);
			{
				event_lock lock(units, opp);

				game_events::fire("die", opp.get_location(), u_loc, dat);
				// recalculate defender_pointer
				if (!lock.valid()) {
					// WML has invalidated the dying unit, abort
					continue;
				}
			}
			unit_die(units, opp, NULL, 0, a_side);
		}
	}
}

void do_trade(team& current_team, unit& commoner, artifical& owner, artifical& target, bool forward)
{
	std::stringstream strstr;
	int divisor = forward? 200: 100;
	artifical& bonus_city = forward? target: owner;
	int gold_income = commoner.gold_income() * bonus_city.revenue_income_ / divisor;

	if (gold_income) {
		bonus_city.add_gold_bonus(gold_income);
		strstr << help::tintegrate::generate_img("misc/gold.png");
		// help::tintegrate::generate_img(strstr, "misc/increase.png");
		strstr << gold_income;
	}

	int technology_income = commoner.technology_income() * bonus_city.revenue_income_ / divisor;
	if (technology_income) {
		bonus_city.add_technology_bonus(technology_income);
		if (gold_income) {
			strstr << "\n";
		}
		strstr << help::tintegrate::generate_img("misc/technology.png");
		// help::tintegrate::generate_img(strstr, "misc/increase.png");
		strstr << technology_income;
		
	}

	if (!strstr.str().empty()) {
		unit_display::unit_text(bonus_city, false, strstr.str());
	}

	commoner.get_experience(increase_xp::attack_ublock(commoner), 8);
	dialogs::advance_unit(commoner, !current_team.is_human(), true);
}

int calculate_weapon(const unit& attacker, const unit& defender)
{
	const map_location& defender_loc = defender.get_location();

	std::string range = "melee";
	const map_location* tiles = attacker.adjacent_;
	size_t adjance_size = attacker.adjacent_size_;
	int step = 0;
	for (; step < 3; step ++) {
		if (step == 1) {
			tiles = attacker.adjacent_2_;
			adjance_size = attacker.adjacent_size_2_;
			range = "ranged";
		} else if (step == 2) {
			tiles = attacker.adjacent_3_;
			adjance_size = attacker.adjacent_size_3_;
			range = "cast";
		}
		size_t adj = 0;
		for (; adj < adjance_size; adj ++) {
			const map_location& loc = tiles[adj];
			if (loc == defender_loc) break;
		}
		if (adj < adjance_size) break;
	}
	if (step == 3) return -1;

	const std::vector<attack_type>& attacks = attacker.attacks();
	int weapon_index = -1;
	for (std::vector<attack_type>::const_iterator it = attacks.begin(); it != attacks.end(); ++ it) {
		if (it->range() == range) {
			weapon_index = std::distance(attacks.begin(), it);
			break;
		}
	}
	return weapon_index;
}

bool no_fightback_attack(unit& attacker, unit& defender)
{
	VALIDATE(!attacker.is_artifical(), "no_fightback_attack, current not support no fightback using artifical!");

	int weapon_index = calculate_weapon(attacker, defender);
	if (weapon_index == -1) {
		// see remark#44
		return true;
	}

	// after attacker side, left of attacker is zero if attacker is attack.
	attacker.set_attacks(attacker.attacks_total());
	std::pair<map_location, map_location> to_locs = attack_enemy(&attacker, &defender, weapon_index, -1, false);
	return to_locs.second.valid();
}

void reform_captain(unit_map& units, unit& u, hero& h, bool join, bool replay)
{
	if (!replay) {
		recorder.add_reform_captain(u, h, join);
	}

	std::vector<hero*> captains;
	captains.push_back(&u.master());
	if (u.second().valid()) {
		captains.push_back(&u.second());
	}
	if (u.third().valid()) {
		captains.push_back(&u.third());
	}
	artifical* city = units.city_from_cityno(h.city_);

	if (join) {
		VALIDATE(captains.size() <= 2, "reform_captain, Cannot join because too more hero!");
		city->hero_go_out(h);
		captains.push_back(&h);

		u.replace_captains(captains);
	} else {
		VALIDATE(captains.size() > 1, "reform_captain, Cannot extra because too few hero!");
		
		std::vector<hero*>::iterator it = std::find(captains.begin(), captains.end(), &h);
		captains.erase(it);
		u.replace_captains(captains);

		city->fresh_into(h);	
	}
}

void do_demolish(game_display& gui, unit_map& units, team& current_team, unit* u, int income, bool to_recorder)
{
	artifical* city = units.city_from_cityno(u->cityno());
	map_location loc = u->get_location();

	if (to_recorder) {
		recorder.add_disband(u->base()? -1 * unit_map::BASE: -1 * unit_map::OVERLAY, loc, income);
	}

	std::vector<hero*> vh;
	if (!u->is_artifical()) {
		vh.push_back(&u->master());
		if (u->second().valid()) {
			vh.push_back(&u->second());
		}
		if (u->third().valid()) {
			vh.push_back(&u->third());
		}
	}

	units.erase(u);
	current_team.spend_gold(-1 * income);

	if (!u->is_artifical()) {
		for (std::vector<hero*>::iterator it = vh.begin(); it != vh.end(); ++ it) {
			city->fresh_into(**it);
		}
	}

	// invalidate adjacent locs
	gui.invalidate(loc);
	size_t size = (sizeof(adjacent_1) / sizeof(map_offset)) >> 1;
	map_offset* adjacent_ptr = adjacent_1[loc.x & 0x1];
	for (size_t i = 0; i < size; i ++) {
		gui.invalidate(map_location(loc.x + adjacent_ptr[i].x, loc.y + adjacent_ptr[i].y));
	}
}

void do_employ(play_controller& controller, unit_map& units, team& current_team, hero& h, int cost, bool replay)
{
	if (!replay) {
		recorder.add_employ(h, cost);
	}
	artifical& to_city = *units.city_from_cityno(current_team.leader()->city_);
	to_city.fresh_into(h);
	controller.erase_employ(h);
	current_team.spend_gold(cost);
}

void do_fresh_heros(team& current_team, bool replay)
{
	if (!replay) {
		recorder.add_fresh_heros();
	}
	std::vector<artifical*>& holden_cities = current_team.holded_cities();
	for (std::vector<artifical*>::iterator it = holden_cities.begin(); it != holden_cities.end(); ++ it) {
		artifical& city = **it;
		city.finish_2_fresh();		
	}
}


bool adjacent_has_enemy(const unit_map& units, const team& current_team, const unit& u)
{
	const map_location* tiles = u.adjacent_;
	size_t adjacent_size = u.adjacent_size_;

	for (int i = 0; i != adjacent_size; ++ i) {
		unit_map::const_iterator another = units.find(tiles[i]);
		if (another.valid() && current_team.is_enemy(another->side())) {
			return true;
		}
		another = units.find(tiles[i], false);
		if (another.valid() && current_team.is_enemy(another->side())) {
			return true;
		}
	}
	return false;
}

void do_add_active_tactic(unit& u, hero& h, bool to_recorder)
{
	if (to_recorder) {
		recorder.active_tactic(true, u, h);
	}
	u.set_tactic_hero(h);
}

void do_remove_active_tactic(unit& u, bool to_recorder)
{
	if (to_recorder) {
		recorder.active_tactic(false, u, hero_invalid);
	}
	u.remove_from_tactic_cache();
}

void do_direct_expedite(std::vector<team>& teams, unit_map& units, gamemap& map, artifical& city, int index, const map_location to, bool to_recorder)
{
	if (to_recorder) {
		std::vector<map_location> steps;
		steps.push_back(city.get_location());
		steps.push_back(to);
		recorder.add_expedite(true, index, steps, true, true);
	}

	units.set_expediting(&city, true, index);
	units.move(city.get_location(), to);
	city.troop_go_out(units.last_expedite_index());
	units.find(to)->set_movement(0);

	if (map.is_village(to)) {
		int orig_village_owner = village_owner(to, teams);
		if (size_t(orig_village_owner) != city.side() - 1) {
			get_village(to, city.side());
		}
	}
}

void do_direct_move(std::vector<team>& teams, unit_map& units, gamemap& map, unit&u, const map_location from, const map_location to, bool to_recorder) 
{
	if (to_recorder) {
		std::vector<map_location> steps;
		steps.push_back(from);
		steps.push_back(to);
		recorder.add_movement(steps, true);
	}

	units.move(from, to);
	VALIDATE(&u == &*units.find(to), "mouse_handler::direct_move_unit, move modify u incorrectly!");
	if (map.is_village(to)) {
		int orig_village_owner = village_owner(to, teams);
		if (size_t(orig_village_owner) != u.side() - 1) {
			get_village(to, u.side());
		}
	}

	u.set_movement(0);
	u.set_standing();
}

void do_bomb(game_display& gui, team& t, bool to_recorder)
{
	if (to_recorder) {
		recorder.add_bomb(t);
	}
	t.set_bomb_turns(0);
	t.refresh_tactic_slots(gui);
}

void do_scenario_env(const tscenario_env& env, play_controller& controller, bool to_recorder)
{
	if (to_recorder) {
		recorder.add_scenario_env(env);
	}

	controller.set_duel(env.duel);
	game_config::maximal_defeated_activity = env.maximal_defeated_activity;
}