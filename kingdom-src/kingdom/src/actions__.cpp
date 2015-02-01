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
#include "gui/dialogs/combo_box.hpp"
#include "sound.hpp"
#include "loadscreen.hpp"

#include "gui/dialogs/title_screen.hpp"

#include <boost/foreach.hpp>
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
	if (un == units.end()) {
		return map_location::null_location;
	}
	const unit* u = dynamic_cast<const unit*>(&*un);
	unit_ability_list abil = u->get_abilities("leadership");
	if (bonus) {
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
		if (!overlay && base) {
			const unit* u = dynamic_cast<const unit*>(base->second);
			if (u->wall()) {
				return tiles[i];
			}
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


bool formation_defend_disable = false;

class formation_defend_disable_lock
{
public:
	formation_defend_disable_lock()
		: origin_(formation_defend_disable)
	{
		formation_defend_disable = true;
	}
		
	~formation_defend_disable_lock()
	{
		formation_defend_disable = origin_;
	}

private:
	bool origin_;
};

// Helper class for performing an attack.
class attack
{
public:
	static unit_display::formation_anim_lock* anim_lock;

	// Structure holding unit info used in the attack action.
	struct unit_info
	{
		unit_map& units_;
		map_location loc_;
		int weapon_;
		bool base_;
		size_t signature_;
		std::string weap_id_;
		int orig_attacks_;
		int n_attacks_; /**< Number of attacks left. */
		int cth_;
		int damage_;
		int xp_;
		bool reinforced_;
		bool hated_;

		unit_info(unit& u, int weapon, unit_map &units);
		unit &get_unit();
		bool valid();

		int cause_damage_;
		int been_damage_;
		int defeat_units_;
		int activity_;

		std::string dump();
	};
	friend std::pair<map_location, map_location> attack_unit(unit &, unit &, int, int, bool, const config&, bool, bool);

	attack(unit& attacker, unit& defender, int attack_with, int defend_with, bool update_display = true, const config& = config::invalid, bool move = true, bool formation = false);
	std::pair<map_location, map_location> perform();
	bool perform_hit(bool attacker_turn);
	bool weapon_move(unit& u, const map_location& from, const map_location& to, std::set<map_location>& invalid_locs);
	~attack();

	class attack_end_exception {};
	void fire_event(const std::string& n);
	void refresh_bc();

	void unit_die(unit_info& attacker, unit_info& center, unit& defender, const battle_context::unit_stats*& attacker_stats, const attack_type* defender_weapon);

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
	bool constant_attacks_;

	bool update_display_;
	bool OOS_error_;
};

int formationed_value(int value, int skill, bool attacking)
{
	if (attacking) {
		// multi ratio: y=sqrt(x + 1), x=[0, 15] ==> y=[1, 4]
		return 1.0 * value * sqrt((double)skill + 1);
	} else {
		// divide ratio: y=(x + 5)^0.5, x=[0, 3, 15] ==> y=[1.3, 1.5, 1.8]
		return 1.0 * value / pow((double)skill + 5, 0.2);
	}
}

battle_context::unit_stats::unit_stats(const unit &u, const map_location& u_loc,
		int u_attack_num, bool attacking,
		const unit &opp, const map_location& opp_loc,
		const attack_type *opp_weapon,
		const unit_map& units) :
	weapon(0),
	attack_num(u_attack_num),
	is_attacker(attacking),
	is_poisoned(u.get_state(ustate_tag::POISONED)),
	is_slowed(u.get_state(ustate_tag::SLOWED)),
	is_broken(u.get_state(ustate_tag::BROKEN)),
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
	defender_formation(),
	effecting_tactic()
{
	std::vector<team>& teams = *resources::teams;
	team& u_team = teams[u.side() - 1];

	// Get the current state of the unit.
	if (attack_num >= 0) {
		weapon = &u.attacks()[attack_num];
	}
	if (u.hitpoints() < 0) {
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
		if (opp_weapon) {
			opp_weapon->set_specials_context(aloc, dloc, units, !attacking, weapon);
		}
		if (unit_feature_val2(u, hero_feature_pure) && !unit_feature_val2(opp, hero_feature_penetrate)) {
			slows = true;
		} else {
			slows = weapon->get_special_bool("slow");
		}
		if (!opp.is_artifical()) {
			if (attack::anim_lock && (attack::anim_lock->formation_.profile_->attack_flag_ & tformation_profile::BREAK)) {
				breaks = true;
			} else if (attacking && u.hide_turns()) {
				breaks = true;
			} else if (unit_feature_val2(u, hero_feature_break) && !unit_feature_val2(opp, hero_feature_penetrate)) {
				breaks = true;
			}
		}
		if (unit_feature_val2(u, hero_feature_drain)) {
			drains = true;
		} else {
			drains = weapon->get_special_bool("drains");
		}
		petrifies = weapon->get_special_bool("petrifies");
		if (attacking && !opp.is_artifical() && !unit_feature_val2(opp, hero_feature_poison)) {
			if (attack::anim_lock && (attack::anim_lock->formation_.profile_->attack_flag_ & tformation_profile::POISON)) {
				poisons = true;
			} else if (unit_feature_val2(u, hero_feature_poison)) {
				poisons = true;
			} else {
				poisons = weapon->get_special_bool("poison") && !opp.get_state(ustate_tag::POISONED);
			}
		}
		backstab_pos = is_attacker && backstab_check(u, opp, units, *resources::teams);
		firststrike = weapon->get_special_bool("firststrike");

		// Compute chance to hit.
		chance_to_hit = opp.defense_modifier(resources::game_map->get_terrain(opp_loc)) + weapon->accuracy() - (opp_weapon ? opp_weapon->parry() : 0);

		// Compute base damage done with the weapon.
		int base_damage = weapon->damage();
		unit_ability_list dmg_specials = weapon->get_specials("damage");
		unit_abilities::effect dmg_effect(dmg_specials, base_damage, backstab_pos);
		base_damage = dmg_effect.get_composite_value();

		if (u.get_ability_bool("guard") && u.has_guard_center()) {
			base_damage += 2;
		}

		if (opp.is_artifical()) {
			base_damage += u.skill_[hero_skill_demolish] * 5 / MAX_UNIT_ADAPTABILITY;
		}

		if (backstab_pos) {
			if (unit_feature_val2(u, hero_featrue_backstab) || weapon->get_special_bool("backstab")) {
				base_damage += base_damage;
			}
		}

		if (!opp.is_artifical() && !opp.has_female() && u.get_ability_bool("bewitch")) {
			base_damage += base_damage;
		}

		// Get the damage multiplier applied to the base damage of the weapon.
		int damage_multiplier = 100;

		// Time of day bonus.
		damage_multiplier += combat_modifier(u_loc, u.alignment(), u.is_fearless());

		if (unit_feature_val2(u, hero_feature_dayattack) || unit_feature_val2(u, hero_feature_nightattack)) {
			// const time_of_day &tod = resources::tod_manager->time_of_day_at(u_loc);
			const time_of_day &tod = resources::tod_manager->get_time_of_day(u_loc);
			if (tod.lawful_bonus > 0 && unit_feature_val2(u, hero_feature_dayattack)) {
				damage_multiplier += tod.lawful_bonus / 2;
			} else if (tod.lawful_bonus < 0 && unit_feature_val2(u, hero_feature_nightattack)) {
				damage_multiplier -= tod.lawful_bonus / 2;
			}
		}

		// Resistance modifier.
		int resistance_ratio = opp.damage_from(*weapon, !attacking, opp_loc);
		if (unit_feature_val2(opp, HEROS_MAX_ARMS + u.arms())) {
			resistance_ratio -= RESISTANCE_DIV2 / 2;
		} else if (unit_feature_val2(opp, u.arms())) {
			resistance_ratio -= RESISTANCE_DIV3 / 2;
		}

		damage_multiplier *= resistance_ratio * chance_to_hit;
		if (attacking && !formation_defend_disable && !opp.is_artifical() && !opp.resist_helper_.empty()) {
			std::set<const unit*> origin;
			// select max unit when they are same effect. it will result to lose helper.
			// for exmplae, tow footman resistance opp, one footmant is can formation, ther other cannot, may be select the latter.
			// generate all candidate resistance help.
			unit_ability_list helpers = opp.get_abilities("resistance");
			for (std::vector<std::pair<const config *, unit *> >::const_iterator it = helpers.cfgs.begin(); it != helpers.cfgs.end(); ++ it) {
				if (it->second != &opp) {
					origin.insert(it->second);
				}
			}

			std::vector<tformation> formations;
			calculate_defender_formations(*resources::teams, *resources::units, u.side(), opp, origin, formations);
			if (!formations.empty()) {
				damage_multiplier = formationed_value(damage_multiplier, (formations[0].u_->skill_[hero_skill_formation] + opp.skill_[hero_skill_formation]) / 2, false);
				defender_formation = formations[0];
			}
		}

		// Compute both the normal and slowed damage. For the record,
		damage = round_damage(base_damage, damage_multiplier, 1000000);
		
		// Leadership bonus.
		int leader_bonus = 0;
		if (under_leadership(units, u_loc, &leader_bonus).valid()) {
			damage += damage * leader_bonus * u_team.cooperate_increase_ / 10000;
		}
		damage = u.value_consider_food(damage);
		damage = opp.value_consider_food(damage, false);

		if (opp.get_state(ustate_tag::BROKEN)) {
			damage = damage * 3 / 2;
		}

		// conside wall
		if (!u.wall()) {
			unit* u = units.find_unit(u_loc, false);
			if (u && u->wall()) {
				damage += damage / 10;
			}
		}
		if (!opp.wall()) {
			unit* u = units.find_unit(opp_loc, false);
			if (u && u->wall()) {
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
		if (tent::mode != mode_tag::SIEGE) {
			std::vector<artifical*>& holden = u_team.holden_cities();
			for (std::vector<artifical*>::const_iterator it = holden.begin(); it != holden.end(); ++ it) {
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
		}

		if (is_slowed) {
			damage = damage / 2;
		}

		if (attacking && u.get_state(ustate_tag::REVIVALED)) {
			damage = damage / 2;
		}
		
		if (!attacking && damage > 5) {
			damage = damage * 9 / 10;
		}

		if (attack::anim_lock) {
			damage = formationed_value(damage, (attack::anim_lock->formation_.u_->skill_[hero_skill_formation] + u.skill_[hero_skill_formation]) / 2, true);
		}

		if (tent::mode == mode_tag::SIEGE) {
			damage *= 2;
		}

		num_blows = weapon->num_attacks();
		if (attacking) {
			if (unit_feature_val2(u, hero_feature_carom)) {
				num_blows ++;
			}
			if (unit_feature_val2(opp, hero_feature_disturb) && num_blows > 1) {
				num_blows --;
			}
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
	printf("rounds:	%d\n", static_cast<int>(rounds));
	printf("firststrike:	%d\n", static_cast<int>(firststrike));
	printf("\n");
	printf("hp:		%d\n", hp);
	printf("max_hp:		%d\n", max_hp);
	printf("chance_to_hit:	%d\n", chance_to_hit);
	printf("damage:		%d\n", damage);
	printf("num_blows:	%d\n", num_blows);
	printf("\n");
}

// Given this harm_weight, are we better than this other context?
bool battle_context::better_attack(class battle_context &that, double harm_weight)
{
	return better_combat(get_attacker_combatant(), get_defender_combatant(),
			that.get_attacker_combatant(), that.get_defender_combatant(), harm_weight);
}

unit_display::formation_anim_lock* attack::anim_lock = NULL;

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
	if (n == "attack_end") {
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
	if (!a_.valid() || !d_.valid()) {
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
	, signature_(u.signature)
	, weap_id_()
	, orig_attacks_(0)
	, n_attacks_(0)
	, cth_(0)
	, damage_(0)
	, xp_(0)
	, reinforced_(false)
	, hated_(false)
	, cause_damage_(0)
	, been_damage_(0)
	, defeat_units_(0)
	, activity_(0)
{
}

unit& attack::unit_info::get_unit()
{
	return *units_.find_unit(loc_, !base_);
}

bool attack::unit_info::valid()
{
	const unit* u = units_.find_unit(loc_, !base_);
	return u && u->signature == signature_;
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
		, signature_(defender.signature)
	{
	}

	bool valid()
	{
		unit* u = units_.find_unit(loc_, !base_);
		if (!u) {
			return false; 
		}
		return u->signature == signature_;
	}

	~event_lock() 
	{
	}

private:
	unit_map& units_;
	const map_location& loc_;
	bool base_;
	size_t signature_;
};

// @a: attack.
// @die: unit of die.
void unit_die(unit_map& units, unit& die, void* a_info_p, int die_activity, int a_side)
{
	std::vector<team>& teams = *resources::teams;
	attack::unit_info* a_info = NULL;
	unit* attacker = NULL;
	team& die_team = teams[die.side() - 1];
	artifical* cobj = units.city_from_cityno(die.cityno());

	if (a_info_p) {
		a_info = reinterpret_cast<attack::unit_info*>(a_info_p);
		attacker = &a_info->get_unit();

		team& t = teams[attacker->side() - 1];
		if (t.kill_income && !die.is_artifical() && !die.is_commoner()) {
			t.spend_gold(-1 * die.cost());
			unit_display::unit_income(*attacker, die.cost());
		}
	}

	if (!die.is_artifical()) {
		int merit = die.cause_damage_ + std::min<int>(75, die.field_turns_ * 25);
		int activity;
		if (merit < 150 && !die.is_soldier() && !die.is_commoner()) {
			activity = -1 * game_config::maximal_defeated_activity * (150 - merit) / 150;
		} else {
			activity = 0;
		}
		activity += die_activity;
		if (activity) {
			die.increase_activity(activity, false);
		}

		if (!cobj && !die.master().get_flag(hero_flag_robber) && die_team.leader() != &die.master()) {
			for (int n = 0; n < (int)teams.size(); n ++) {
				team& t = teams[n];
				if (die_team.is_enemy(n + 1) || !t.stratagem_ally_join) {
					continue;
				}
				std::vector<artifical*>& holden = t.holden_cities();
				if (!holden.empty()) {
					cobj = holden.front();
					unit_display::stratagem_anim(stratagem_tag::technology(stratagem_tag::xue_zhong_song_tan), die.master().image(true), true);
					break;
				}
			}
		}

		std::vector<hero*> back, roam, unstage;
		if (die.master().number_ >= hero::number_normal_min) {
			hero* h = &die.master();
			if (h != rpg::h && h->get_flag(hero_flag_roam)) {
				roam.push_back(h);
			} else if (cobj) {
				back.push_back(h);
			} else {
				unstage.push_back(h);
			}

			if (die.second().valid()) {
				h = &die.second();
				if (h != rpg::h && h->get_flag(hero_flag_roam)) {
					roam.push_back(h);
				} else if (cobj) {
					back.push_back(h);
				} else {
					unstage.push_back(h);
				}
			}
			if (die.third().valid()) {
				h = &die.third();
				if (h != rpg::h && h->get_flag(hero_flag_roam)) {
					roam.push_back(h);
				} else if (cobj) {
					back.push_back(h);
				} else {
					unstage.push_back(h);
				}
			}
		}

		if (back.size()) {
			// be back to "ownership" city
			for (std::vector<hero*>::const_iterator itor = back.begin(); itor != back.end(); ++ itor) {
				cobj->finish_into(**itor, hero_status_backing);
			}
			resources::screen->invalidate(cobj->get_location());
		}
		if (roam.size()) {
			// roam to a radmon city
			std::vector<artifical*> citys;
			for (size_t i = 0; i < (*resources::teams).size(); i ++) {
				std::vector<artifical*>& side_citys = (*resources::teams)[i].holden_cities();
				citys.insert(citys.end(), side_citys.begin(), side_citys.end());
			}
			cobj = citys[get_random() % citys.size()];

			hero& leader = *teams[cobj->side() - 1].leader();
			bool city_team_is_rpg = &leader == rpg::h;

			for (std::vector<hero*>::iterator itor = roam.begin(); itor != roam.end(); ++ itor) {
				hero* h = *itor;
				if (cobj->side() == team::empty_side) {
					cobj->wander_into(*h, false);
				} else if (!city_team_is_rpg || !runtime_groups::exist_member(*h, leader)) {
					// recommand! let float_catalog to base catalog.
					h->float_catalog_ = ftofxp8(h->base_catalog_);
					if (tent::mode != mode_tag::TOWER) {
						cobj->move_into(*h);
					} else {
						cobj->fresh_into(*h);
					}
					join_anim(h, cobj, _("Let me join in. I will do my best to maintenance our honor."));
				} else {
					h->to_unstage(hero::UNSTAGE_GROUP);
				}
			}
			resources::screen->invalidate(cobj->get_location());
		}

		units.erase(&die);

		// there is why not process unstage.
		// if has leader, it will wrong. I assume desiger should consider these hero more.
/*
		for (std::vector<hero*>::iterator it = unstage.begin(); it != unstage.end(); ++ it) {
			hero* h = *it;
			h->to_unstage();
		}
*/

	} else if (die.can_reside()) { 
		// defend type: city
		// alternate.
		VALIDATE(a_info_p || (a_side > 0 && a_side <= (int)teams.size()), "unit_die, die is city, both a_info_p and a_ side must valid one!");

		if (a_side == HEROS_INVALID_SIDE) {
			a_info->xp_ += 16; // 
			cobj = dynamic_cast<artifical*>(&die);
			if (attacker->is_artifical() || attacker->is_robber()) {
				// artifical/robber, don't enter into
				cobj->fallen(team::empty_side);
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
		if (!die.fort()) {
			units.erase(&die);
		} else {
			cobj = dynamic_cast<artifical*>(&die);
			if (a_side != HEROS_INVALID_SIDE) {
				cobj->fort_fallen(a_side);
			} else if (attacker->is_artifical() || attacker->is_robber()) {
				cobj->fort_fallen(team::empty_side);
			} else {
				cobj->fort_fallen(attacker->side());
			}
		}
	}
}

void attack::unit_die(unit_info& attacker, unit_info& center, unit& defender_u, const battle_context::unit_stats*& attacker_stats, const attack_type* defender_weapon)
{
	unit& attacker_u = attacker.get_unit();
	// center maybe invalid. center maybe die before this second die.
	bool defender_is_center = (center.valid() && &defender_u == &center.get_unit());

	// when dies, hitpoints may be negative, below tow addition is subtration actually.
	attacker.cause_damage_ += defender_u.hitpoints();
	if (defender_is_center) {
		center.been_damage_ += defender_u.hitpoints();
	} else {
		defender_u.been_damage_ += defender_u.hitpoints();
	}
	if (!defender_u.is_commoner()) {
		attacker.defeat_units_ ++;
	}

	attacker.xp_ += game_config::kill_xp(defender_u.level());
	if (defender_is_center) {
		center.xp_ = 0;
		gui_.invalidate(attacker.loc_);
	}

	if (anim_lock && teams_[attacker_u.side() - 1].kill_income) {
		anim_lock->formation_.gold_ += 20;
	}

	game_events::entity_location death_loc(defender_u.get_location(), 0);
	game_events::entity_location attacker_loc(attacker.loc_, 0);
	if (defender_is_center) {
		fire_event("attack_end");
		refresh_bc();
	}

	// get weapon info for last_breath and die events
	config dat;
	{
		event_lock lock(units_, defender_u);

		if (game_events::fire("last breath", death_loc, attacker_loc, dat)) {
			refresh_bc();
		}
		// recalculate defender_pointer
		if (!lock.valid()) {
			if (defender_is_center && attacker.valid()) {
				attacker_stats->weapon->other_attack_ = NULL;
			}
			// WML has invalidated the dying unit, abort
			return;
		}
	}

	if (!attacker.valid()) {
		unit_display::unit_die(defender_u.get_location(), defender_u, NULL, defender_weapon);
	} else {
		unit_display::unit_die(defender_u.get_location(), defender_u, attacker_stats->weapon, defender_weapon,
			attacker.loc_, &attacker_u);
	}

	::unit_die(units_, defender_u, &attacker, defender_is_center? center.activity_: 0);
	if (defender_is_center && attacker.valid()) {
		// attacker may be erase!! for example, capture one city, attack will be erase. 
		// attacker_stats->weapon is a pointer that pointed to attack() of unit(attacker), must not be NULL when attacker became invalid.
		// attacker_stats->weapon->other_attack_ is pointer of center defender.
		attacker_stats->weapon->other_attack_ = NULL;
	}
	refresh_bc();
}

std::pair<map_location, map_location> attack_unit(unit& attacker, unit& defender,
	int attack_with, int defend_with, bool update_display, const config& duel, bool move, bool formation)
{
	attack dummy(attacker, defender, attack_with, defend_with, update_display, duel, move, formation);
	return dummy.perform();
}

attack::attack(unit& attacker, unit& defender, int attack_with, int defend_with, bool update_display, const config& duel,
		bool move, bool constant_attacks) :
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
	constant_attacks_(constant_attacks),
	OOS_error_(false)
{
}

attack::~attack()
{
	delete bc_;
}

bool attack::perform_hit(bool attacker_turn)
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
	if (attacker_turn && unit_feature_val2(attacker.get_unit(), hero_feature_fearless) && !center_defender_ptr->is_artifical() && center_defender_ptr->hdata_variable()) {
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

		int attack_encourage = attacker_u.value_consider_food(attacker_u.skill_[hero_skill_encourage]);
		int defender_encourage = center_defender_ptr->value_consider_food(center_defender_ptr->skill_[hero_skill_encourage]);

		int encourage_diff = attack_encourage - defender_encourage;
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
			if (attacker.hated_) {
				damage = damage * 3 / 2;
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
	bool defender_dies = defender.get_unit().hitpoints() <= attacker.damage_;

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
		damage_locs.push_back(std::make_pair(&defender.get_unit(), damage));
	} else {
		std::pair<const config *, unit *>& cfg1 = attackpoints.cfgs.front();
		const config& cfg = *(cfg1.first);
		bool affect_self = cfg["affect_self"].to_bool(true);
		std::vector<std::string> attack_points = utils::split(cfg["points"]);
		std::vector<std::string> attack_values = utils::split(cfg["values"]);
		size_t points = attack_points.size(), values = attack_values.size();

		if (affect_self) {
			damage_locs.push_back(std::make_pair(&defender.get_unit(), damage));
		} else {
			damage_locs.push_back(std::make_pair(&defender.get_unit(), 0));
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

			unit* involved = units_.find_unit(loc);
			if (!involved) {
				continue;
			}
			if (teams_[involved->side() - 1].ea_artifical_neutral && hero::is_ea_artifical(involved->packee_type()->master())) {
				continue;
			}
			if (!attack_team.is_enemy(involved->side()) || involved->invisible(loc)) {
				continue;
			}

			// must make involved is exclusive. 
			// if isn't exclusive it may result in second or later same u_ptr to become invalid and program exit exceptional  
			if (assist_find.find(involved) != assist_find.end()) {
				continue;
			}

			formation_defend_disable_lock lock;

			battle_context bc(units_, attacker.get_unit(), *involved, attacker.weapon_, -1);
			const battle_context::unit_stats* a_stats = &bc.get_attacker_stats();
			damage_locs.push_back(std::make_pair(involved, a_stats->damage * value * attack_team.interlink_increase_ / 10000));
			assist_find.insert(involved);
		}
	}
	// unit_attack parameters
	std::vector<unit*> def_ptr_vec;
	std::vector<int> damage_vec;
	std::vector<std::string> hit_text_vec;
	for (std::vector<std::pair<unit*, int> >::const_iterator loc = damage_locs.begin(); loc != damage_locs.end(); ++ loc) {
		unit* defender_ptr = loc->first;
		std::ostringstream float_text;

		// special only effect center(primary) defender.
		// for example, second defender has city, poisoned cannot effect it.
		if (defender_ptr == center_defender_ptr) {
			const unit &defender_unit = *defender_ptr;
			if (hits) {
				if (attacker_stats->poisons && !defender_unit.get_state(ustate_tag::POISONED)) {
					// tintegrate::generate_img(float_text, "misc/poisoned.png");
					// float_text << '\n';
					float_text << (defender_unit.gender() == unit_race::FEMALE ?
						_("female^poisoned") : _("poisoned")) << '\n';
				}

				if (attacker_stats->slows && !defender_unit.get_state(ustate_tag::SLOWED)) {
					// tintegrate::generate_img(float_text, "misc/slowed.png");
					// float_text << '\n';
					float_text << (defender_unit.gender() == unit_race::FEMALE ?
						_("female^slowed") : _("slowed")) << '\n';
				}

				if (attacker_stats->breaks && !defender_unit.get_state(ustate_tag::BROKEN)) {
					// tintegrate::generate_img(float_text, "misc/broken.png");
					// float_text << '\n';
					float_text << (defender_unit.gender() == unit_race::FEMALE ?
						_("female^broken") : _("broken")) << '\n';
				}

				if (attacker_stats->petrifies) {
					// tintegrate::generate_img(float_text, "misc/petrified.png");
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
			} else {
				float_text << _("Miss") << '\n';

			}
		}
		def_ptr_vec.push_back(defender_ptr);
		damage_vec.push_back(loc->second);
		hit_text_vec.push_back(float_text.str());
	}

	if (anim_lock) {
		anim_lock->push_defender(center_defender_ptr);
	}

	unit* find = units_.find_unit(attacker_stats->effecting_tactic);
	artifical* t = (find && find->type()->master() == hero::number_tactic)? unit_2_artifical(&*find): NULL;
	unit_display::unit_attack(attacker.get_unit(), def_ptr_vec, damage_vec,
		attacker_stats->weapon, defender_stats->weapon,
		abs_n, hit_text_vec, attacker_stats->drains, stronger, "", t, attacker_stats->defender_formation);

	bool first_die = true;
	bool center_defender_survived = true;
	size_t index_in_locs = 0;
	for (std::vector<std::pair<unit*, int> >::const_iterator loc = damage_locs.begin(); loc != damage_locs.end(); ++ loc, index_in_locs ++) {
		if (!attacker.valid()) {
			break;
		}
		unit* defender_ptr = loc->first;

		// defender_loc must use instance instead of reference. last breath/die may earse defender_ptr.
		const attack_type* defender_weapon = defender_ptr == center_defender_ptr? defender_stats->weapon: NULL;
		
		bool dies = loc->first->take_hit(loc->second);

		if (defender_ptr == center_defender_ptr) {
			refresh_bc();
		}

		attacker.cause_damage_ += loc->second;
		defender.been_damage_ += loc->second;

		if (dies) {
			if (first_die) {
				attacker.xp_ = 0;
				first_die = false;
			}
			if (anim_lock) {
				anim_lock->flush();
			}
			unit_die(attacker, defender, *defender_ptr, attacker_stats, defender_weapon);

			if (defender_ptr == center_defender_ptr) {
				center_defender_survived = false;
			}
			
			update_fog = true;
			update_minimap_ = true;
		} else if (hits) {

			unit& defender_unit = *defender_ptr;
			if (defender_ptr == center_defender_ptr) {
				if (attacker_stats->poisons && !defender_unit.get_state(ustate_tag::POISONED)) {
					defender_unit.set_state(ustate_tag::POISONED, true);
				}

				if (attacker_stats->slows && !defender_unit.get_state(ustate_tag::SLOWED)) {
					defender_unit.set_state(ustate_tag::SLOWED, true);
					update_fog = true;
					if (defender_ptr == center_defender_ptr) {
						// defneder may be broken, need recalculate all.
						refresh_bc();
					}
				}

				if (attacker_stats->breaks && !defender_unit.get_state(ustate_tag::BROKEN)) {
					defender_unit.set_state(ustate_tag::BROKEN, true);
					update_fog = true;
					if (defender_ptr == center_defender_ptr) {
						// attacker may be slowed, need recalculate all.
						refresh_bc();
					}
				}

				// If the defender is petrified, the fight stops immediately
				if (attacker_stats->petrifies) {
					defender_unit.set_state(ustate_tag::PETRIFIED, true);
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
	if (map.on_board(to) && !units_.count(to) && (u.movement_cost(map[to], &to) != unit_movement_type::UNREACHABLE) && !u_base_itor.valid()) {
		for (size_t i = 0; i < u.adjacent_size_; i ++) {
			invalid_locs.insert(u.adjacent_[i]);
		}
		resources::units->move(from, to);
		invalid_locs.insert(to);
		if (map.is_village(to)) {
			int orig_village_owner = village_owner(to, *resources::teams);
			int team_num = u.side() -1;
			if (orig_village_owner != team_num) {
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

	std::pair<map_location, map_location> new_locs = std::make_pair(a_.loc_, d_.loc_);

	unit& attacker = a_.get_unit();
	unit& defender = d_.get_unit();
	const int attacker_side = a_.get_unit().side();
	const int defender_side = d_.get_unit().side();
	team& attacker_team = teams_[attacker_side - 1];
	team& defender_team = teams_[defender_side - 1];

	bool attacker_is_artifical = attacker.is_artifical();
	bool attacker_is_soldier = attacker.is_soldier();
	bool attacker_is_tower = attacker.task() == unit::TASK_STATION || ((tent::mode == mode_tag::TOWER) && attacker_team.is_human());
	bool defender_is_artifical = defender.is_artifical();
	bool defender_is_commoner = defender.is_commoner();
	bool defender_is_soldier = defender.is_soldier();

	if (!constant_attacks_) {
		VALIDATE(a_.get_unit().attacks_left() > 0, _("Attacker is not attacks!"));
		VALIDATE(a_.weapon_ < static_cast<int>(a_.get_unit().attacks().size()), _("An invalid attacker weapon got selected."));
		a_.get_unit().set_attacks(a_.get_unit().attacks_left() - 1);
		a_.get_unit().set_movement(a_.get_unit().movement_left() - a_.get_unit().attacks()[a_.weapon_].movement_used());
	}

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
		
		unit& left_u = *units_.find_unit(left);
		unit& right_u = *units_.find_unit(right);

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
	while (!anim_lock && duel_mode != NO_DUEL && !controller_.is_recovering(attacker.side()) && !resources::controller->has_network_player()) {
		if (attacker_is_artifical || attacker_is_soldier || defender_is_artifical || defender_is_commoner || defender_is_soldier) {
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

	a_.get_unit().set_resting(false);
	d_.get_unit().set_resting(false);

	// If the attacker was invisible, she isn't anymore!
	a_.get_unit().set_state(ustate_tag::UNCOVERED, true);

	bc_ = new battle_context(units_, a_.get_unit(), d_.get_unit(), a_.weapon_, d_.weapon_);
	a_stats_ = &bc_->get_attacker_stats();
	d_stats_ = &bc_->get_defender_stats();

	if (a_stats_->weapon) {
		a_.weap_id_ = a_stats_->weapon->id();
	}
	if (d_stats_->weapon) {
		d_.weap_id_ = d_stats_->weapon->id();
	}

	//
	// check reinforce attack
	//
	std::string text;
	for (size_t i = 0; i < attacker.adjacent_size_; i ++) {
		unit* u = units_.find_unit(attacker.adjacent_[i]);
		if (!u || attacker.side() != u->side()) {
			continue;
		}
		hero& h1 = attacker.master();
		hero* h2 = NULL;
		const unit& that = *u;
		int united_level = that.character_level(apply_to_tag::UNITED);
		if (united_level) {
			int reinforce_percent = 10 - (5 * united_level) / 100;
			if (!((random + that.hitpoints()) % reinforce_percent)) {
				h2 = that.character_hero(apply_to_tag::UNITED);
				text = unit_types.character(h2->character_).name();
			}
		} else if (that.hot_turns()) {
			united_level = that.hot_level();
			if (random % 100 <= united_level) {
				h2 = that.max_tactic_hero();
				text = dgettext("wesnoth-hero", "tactic");
			}
		}
		if (!h2 && !((random + that.hitpoints()) % 5)) {
			h2 = &that.master();
			if (h1.is_consort(*h2)) {
				text = dgettext("wesnoth-hero", "consort");
			} else if (h1.is_oath(*h2)) {
				text = dgettext("wesnoth-hero", "oath");
			} else {
				h2 = NULL;
			}
		}
		if (h2) {
			if (point_in_rect_of_hexes(attacker.get_location().x, attacker.get_location().y, draw_area) || point_in_rect_of_hexes(u->get_location().x, u->get_location().y, draw_area)) {
				unit_display::global_anim_2(area_anim::REINFORCE, h1.image(true), h2->image(true), text);
			}
			a_.reinforced_ = true;
			break;
		}
	}
	// check aggressive can trigger or not.
	// if little damage, it no effect. instead generate time-consuming.
	bool attacker_more_num_blows = false;
	hero* h2 = NULL;
	if (!anim_lock && a_stats_->damage >= game_config::min_valid_damage) {
		int aggressive_level = attacker.character_level(apply_to_tag::AGGRESSIVE);
		if (aggressive_level) {
			int denominator = 14 - aggressive_level / 10;
			if (!((random + attacker.hitpoints()) % denominator)) {
				h2 = attacker.character_hero(apply_to_tag::AGGRESSIVE);
				text = unit_types.character(h2->character_).name();
			}
		} else if (attacker.hot_turns()) {
			aggressive_level = attacker.hot_level();
			if (random % 100 <= aggressive_level) {
				h2 = attacker.max_tactic_hero();
				text = dgettext("wesnoth-hero", "tactic");
			}
		}
		if (h2) {
			if (point_in_rect_of_hexes(attacker.get_location().x, attacker.get_location().y, draw_area) || point_in_rect_of_hexes(defender.get_location().x, defender.get_location().y, draw_area)) {
				unit_display::global_anim_2(area_anim::INDIVIDUALITY, h2->image(true), "projectiles/strike-n-3.png", text);
			}
			attacker_more_num_blows = true;
		}

		//
		// check hate
		//
		if (!((random + attacker.master().number_) % 2)) {
			std::pair<hero*, hero*> ret = attacker.exist_hate(defender);
			if (ret.first) {
				if (point_in_rect_of_hexes(attacker.get_location().x, attacker.get_location().y, draw_area) || point_in_rect_of_hexes(defender.get_location().x, defender.get_location().y, draw_area)) {
					unit_display::global_anim_2(area_anim::REINFORCE, ret.second->image(true), ret.first->image(true), dgettext("wesnoth-hero", "hate"));
				}
				a_.hated_ = true;
			}
		}
	}
	
	// defender reinforced
	h2 = NULL;
	int protective_level = defender.character_level(apply_to_tag::PROTECTIVE);
	if (protective_level) {
		int reinforce_percent = 10 - (5 * protective_level) / 100;
		if (!((random + defender.hitpoints()) % reinforce_percent)) {
			h2 = defender.character_hero(apply_to_tag::PROTECTIVE);
			text = unit_types.character(h2->character_).name();
			// if (!h2) {
			//	h2 = defender.max_tactic_hero();
			// }
		}
	} else if (defender.hot_turns()) {
		protective_level = defender.hot_level();
		if (random % 100 <= protective_level) {
			h2 = defender.max_tactic_hero();
			text = dgettext("wesnoth-hero", "tactic");
		}
	}
	if (!h2 && !((random + defender.hitpoints()) % 8)) {
		h2 = defender.can_encourage();
		if (h2) {
			if (defender.master().is_consort(*h2)) {
				text = dgettext("wesnoth-hero", "consort");
			} else if (defender.master().is_oath(*h2)) {
				text = dgettext("wesnoth-hero", "oath");
			}
		}
	}
	if (h2) {
		hero* h1 = &defender.master();
		if (point_in_rect_of_hexes(attacker.get_location().x, attacker.get_location().y, draw_area) || point_in_rect_of_hexes(defender.get_location().x, defender.get_location().y, draw_area)) {
			if (h1 != h2) {
				unit_display::global_anim_2(area_anim::REINFORCE, h1->image(true), h2->image(true), text, font::BAD_COLOR);
			} else {
				unit_display::global_anim_2(area_anim::INDIVIDUALITY, h1->image(true), "projectiles/strike-n-3.png", text, font::BAD_COLOR);
			}
		}
		d_.reinforced_ = true;
	}

	try {
		fire_event("attack");
	} catch (attack_end_exception) {
		return new_locs;
	}
	refresh_bc();

	a_.orig_attacks_ = a_stats_->num_blows + (attacker_more_num_blows? 1: 0);
	if (anim_lock && a_.orig_attacks_) {
		a_.orig_attacks_ = 1;
	} else if (attacker_team.stratagem_baffle_fightback || unit_feature_val2(attacker, hero_featrue_frighten)) {
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
			if (!perform_hit(true)) break;
		}

		// If the defender got to strike first, they use it up here.
		defender_strikes_first = false;
		++abs_n_defend_;

		if (d_.n_attacks_ > 0) {
			if (!perform_hit(false)) break;
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
		const unit* a_base = units_.find_unit(a_.loc_, false);
		const unit* d_base = units_.find_unit(d_.loc_, false);
		if ((a_base && a_base->wall()) || (d_base && d_base->wall())) {
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

			std::vector<unit*> v;
			if (new_locs.first.valid() && a_.loc_ != new_locs.first) {
				v.push_back(&attacker);
			}
			if (new_locs.second.valid() && d_.loc_ != new_locs.second) {
				v.push_back(&defender);
			}
			units_.multi_resort_map(&gui_, v, true);

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
		attacker_team.increase_defeat_units_one_turn(a_.defeat_units_);

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
		defender_team.increase_defeat_units_one_turn(d_.defeat_units_);

	} else {
		new_locs.second = map_location();
	}

	if (update_display_) {
		gui_.invalidate_unit();
		gui_.invalidate(invalid_locs);
		bool attacker_is_ai = teams_[attacker_side - 1].is_ai() || controller_.is_recovering(attacker_side);
		if (!attacker_is_ai || has_unit_in_eyeshot) {
			gui_.draw(true, true);
		}
	}

	if (OOS_error_) {
		replay::process_error(errbuf_.str());
	}

	if (!anim_lock && !controller_.is_recovering(attacker_side)) {
		int r = rand();
		if (!(r & 0x3f)) {
			get_random_card((r & 1)? teams_[attacker_side - 1]: teams_[defender_side - 1], gui_, units_, heros_);
		}
	}

	return new_locs;
}


int village_owner(const map_location& loc, const std::vector<team>& teams)
{
	for(int i = 0; i != (int)teams.size(); ++i) {
		if(teams[i].owns_village(loc))
			return i;
	}

	return -1;
}

bool get_village(const map_location& loc, int side, int *action_timebonus)
{
	return false;
}

void calculate_supplying(std::vector<team>& teams, unit_map& units, unit& actor)
{
	if (actor.is_artifical() || actor.is_commoner() || actor.food() >= actor.max_food()) {
		return;
	}

	int side = actor.side();
	team& current_team = teams[side - 1];
	int can_supply, addend;
	std::stringstream strstr;

	const map_location* tiles = actor.adjacent_;
	size_t adjance_size = actor.adjacent_size_;
	for (int step = 0; step < 2; step ++) {
		if (step == 1) {
			tiles = actor.adjacent_2_;
			adjance_size = actor.adjacent_size_2_;
		}
		for (size_t adj = 0; adj < adjance_size; adj ++) {
			const map_location& loc = tiles[adj];
			unit* that = units.find_unit(loc);
			if (!that || current_team.is_enemy(that->side())) {
				continue;
			}
			can_supply = 0;
			if (that->is_city()) {
				can_supply = that->max_food();
			} else if (that->fort()) {
				can_supply = that->food();
			}
			if (!can_supply) {
				continue;
			}

			addend = std::min(can_supply, actor.max_food() - actor.food());
			actor.increase_food(addend);
			if (that->fort()) {
				that->increase_food(-1 * addend);
			}

			strstr.str("");
			strstr << tintegrate::generate_img("misc/food.png");
			strstr << addend;
			unit_display::unit_text(actor, false, strstr.str());

			if (actor.food() >= actor.max_food()) {
				return;
			}
		}
	}
}

void carry_supplying(const team& current_team, unit_map& units, unit& provider)
{
	std::vector<unit*> supplies;
	std::map<unit*, int> touched;

	const map_location* tiles = provider.adjacent_;
	size_t adjance_size = provider.adjacent_size_;
	for (int step = 0; step < 2; step ++) {
		if (step == 1) {
			tiles = provider.adjacent_2_;
			adjance_size = provider.adjacent_size_2_;
		}
		for (size_t adj = 0; adj < adjance_size; adj ++) {
			const map_location& loc = tiles[adj];
			unit* u = units.find_unit(loc);
			if (!u) {
				continue;
			}
			unit& that = *u;
			if (that.is_artifical() || that.is_commoner()) {
				continue;
			} 
			if (current_team.is_enemy(that.side()) || that.food() >= that.max_food()) {
				continue;
			}
			supplies.push_back(&that);
			touched.insert(std::make_pair(&that, 0));
		}
	}
	while (!supplies.empty() && provider.food()) {
		int avg = provider.food() / supplies.size();
		if (!avg) {
			avg = 1;
		}
		for (std::vector<unit*>::iterator it = supplies.begin(); it != supplies.end() && provider.food(); ) {
			unit& that = **it;
			if (that.food() < that.max_food()) {
				int addend = std::min(avg, that.max_food() - that.food());
				that.increase_food(addend);
				provider.increase_food(-1 * addend);
				touched.find(&that)->second += addend;
			}

			if (that.food() >= that.max_food()) {
				it = supplies.erase(it);
			} else {
				++ it;
			}
		}
	}

	std::stringstream strstr;
	unit_display::tactic_anim_lock lock(*resources::screen, provider, false);
	for (std::map<unit*, int>::const_iterator it = touched.begin(); it != touched.end(); ++ it) {
		if (!it->second) {
			continue;
		}
		strstr.str("");
		strstr << tintegrate::generate_img("misc/food.png");
		strstr << it->second;
		unit_display::unit_text(*it->first, false, strstr.str());
	}
}

void calculate_supplying2(std::vector<team>& teams, unit_map& units, int side)
{
	const int max_city_food = 100;
	team& current_team = teams[side - 1];

	const std::vector<artifical*>& holden = current_team.holden_cities();
	for (std::vector<artifical*>::const_iterator it = holden.begin(); it != holden.end(); ++ it) {
		artifical& city = **it;
		city.set_food(max_city_food);
		carry_supplying(current_team, units, city);
	}

	const std::pair<unit**, size_t> p = current_team.field_troop();
	unit** troops = p.first;
	size_t troops_vsize = p.second;
	for (size_t i = 0; i < troops_vsize; i ++) {
		unit& u = *troops[i];
		if (!u.fort() || !u.food()) {
			continue;
		}
		carry_supplying(current_team, units, u);
	}
}

// Simple algorithm: no maximum number of patients per healer.
void reset_resting(std::vector<team>& teams, unit* actor)
{
	if (actor) {
		actor->set_resting(true);

	} else {
		VALIDATE(tent::turn_based, "must be in turn_based!");
		int side = unit_map::top_side;
		team& current_team = teams[side - 1];

		std::vector<artifical*> holden_cities = current_team.holden_cities();
		for (std::vector<artifical*>::iterator it = holden_cities.begin(); it != holden_cities.end(); ++ it) {
			artifical& city = **it;
			city.set_resting(true);
		}

		const std::pair<unit**, size_t> p = current_team.field_troop();
		for (size_t i = 0; i < p.second; i ++) {
			unit* u = p.first[i];
			u->set_resting(true);
		}
	}

}

void calculate_end_turn(std::vector<team>& teams, int side)
{
	team& current_team = teams[side - 1];

	std::vector<artifical*> holden_cities = current_team.holden_cities();
	for (std::vector<artifical*>::iterator it = holden_cities.begin(); it != holden_cities.end(); ++ it) {
		artifical& city = **it;
		city.end_turn(true);
	}

	const std::pair<unit**, size_t> p = current_team.field_troop();
	for (size_t i = 0; i < p.second; i ++) {
		unit* u = p.first[i];
		u->end_turn(true);
	}
}

/* Contains all the data used to display healing */
struct unit_healing_struct {
	unit *healed;
	std::vector<unit *> healers;
	int healing;
};

void calculate_healing(std::vector<team>& teams, game_display& disp, unit* actor)
{
	// field troop, decrease loyalty
	if (!actor->is_artifical() && !actor->is_robber()) {
		if (actor->consider_loyalty() && actor->alert_food() && !actor->get_ability_bool("surveillance")) {
			actor->increase_loyalty(game_config::field_troop_increase_loyalty);
		}
	}


	gamemap& gmap = *resources::game_map;
	int terrain_healing = gmap.gives_healing(actor->get_location());

	// heal self or others.
	bool only_resting = false;
	if (!actor->has_ability_type("heals") && !unit_feature_val2(*actor, hero_feature_healer)) {
		if (!actor->resting() && !terrain_healing) {
			return;
		}
		only_resting = true;
	}

	int side = actor->side();
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
			if (only_resting && actor != &u) {
				continue;
			}
			const map_location& loc = u.get_location();
			if (u.is_artifical()) {
				continue;
			}

			if (u.incapacitated()) {
				continue;
			}

			// patient must be self or adjacent with actor.
			if (actor != &u) {
				size_t adj;
				for (adj = 0; adj < actor->adjacent_size_; adj ++) {
					if (loc == actor->adjacent_[adj]) {
						break;
					}
				}
				if (adj == actor->adjacent_size_) {
					continue;
				}
			}

			int healing = 0;
			bool curing = false;
			std::vector<unit *> healers;
			const bool is_poisoned = u.get_state(ustate_tag::POISONED);

			if (!only_resting) {
				unit_ability_list heal = u.get_abilities("heals");

				// For heal amounts, only consider healers on side which is starting now.
				// Remove all healers not on this side.
				bool found = false;
				for (std::vector<std::pair<const config *, unit *> >::iterator h_it = heal.cfgs.begin(); h_it != heal.cfgs.end(); ) {
					unit* potential_healer = h_it->second;
					if (potential_healer->side() != side) {
						h_it = heal.cfgs.erase(h_it);
					} else {
						++ h_it;
						if (potential_healer == actor) {
							found = true;
						}
					}
				}

				// actor must be in potential healer.
				if (!found) {
					continue;
				}

				unit_abilities::effect heal_effect(heal, 0, false);
				healing = heal_effect.get_composite_value() * t.cooperate_increase_ / 100;
				if (is_poisoned && healing > 0) {
					curing = true;
					healing = 0;
				} else if (heal.cfgs.size() > 1) {
					healing /= heal.cfgs.size() - 1;
				}

				for (std::vector<unit_abilities::individual_effect>::const_iterator heal_loc = heal_effect.begin(); heal_loc != heal_effect.end(); ++heal_loc) {
					if (std::find(healers.begin(), healers.end(), heal_loc->u) == healers.end()) {
						healers.push_back(heal_loc->u);
					}
				}
			}
			
			if (actor == &u) {
				if (terrain_healing) {
					if (is_poisoned) {
						curing = true;
					} else {
						if (terrain_healing > healing) {
							healing = terrain_healing;
							healers.clear();
						}
					}
				}

				if (!curing && u.resting()) {
					healing += game_config::rest_heal_amount;
				}
			}
			if (is_poisoned && curing) {
				u.set_state(ustate_tag::POISONED, false);
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

			if (!recorder.is_skipping() && !(u.invisible(u.get_location()) && teams[disp.viewing_team()].is_enemy(side))) {
				unit_healing_struct uhs = { &u, healers, healing };
				l.push_front(uhs);
			}
			if (healing > 0) {
				u.heal(healing);
			} else if (healing < 0) {
				u.take_hit(-healing);
			}
			disp.invalidate_unit();


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
				int d = (int)distance_between(uhs.healed->get_location(), i->healed->get_location());
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

void calculate_healing2(std::vector<team>& teams, game_display& disp, int side)
{
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

			if (u.incapacitated()) {
				continue;
			}

			std::vector<unit *> healers;

			int healing = 0;

			bool curing = false;

			unit_ability_list heal = u.get_abilities("heals");

			const bool is_poisoned = u.get_state(ustate_tag::POISONED);

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
				if (!u.is_robber() && u.consider_loyalty() && u.alert_food() && !u.get_ability_bool("surveillance")) {
					u.increase_loyalty(game_config::field_troop_increase_loyalty);
				}
			}
			if (is_poisoned && curing) {
				u.set_state(ustate_tag::POISONED, false);
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

			if (!recorder.is_skipping() && !(u.invisible(u.get_location()) && teams[disp.viewing_team()].is_enemy(side))) {
				unit_healing_struct uhs = { &u, healers, healing };
				l.push_front(uhs);
			}
			if (healing > 0) {
				u.heal(healing);
			} else if (healing < 0) {
				u.take_hit(-healing);
			}
			disp.invalidate_unit();


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
				int d = (int)distance_between(uhs.healed->get_location(), i->healed->get_location());
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
void road_guarding(unit_map& units, std::vector<team>& teams, const std::map<map_location, std::vector<std::pair<artifical*, artifical*> > >& road_locs, unit* actor)
{
	const artifical* actor_city = NULL;
	int side;
	
	if (actor) {
		actor_city = actor->is_city()? unit_2_artifical(actor): NULL;
		if (!actor_city) {
			return;
		}
		side = actor->side();
	} else {
		VALIDATE(tent::turn_based, "must be in turn_based!");
		side = unit_map::top_side;
	}

	team& current_team = teams[side - 1];
	std::map<unit*, int> touched;
	unit_map::iterator u_it;

	for (std::map<map_location, std::vector<std::pair<artifical*, artifical*> > >::const_iterator it = road_locs.begin(); it != road_locs.end(); ++ it) {
		const map_location& loc = it->first;
		const std::vector<std::pair<artifical*, artifical*> >& v = it->second;

		for (std::vector<std::pair<artifical*, artifical*> >::const_iterator it2 = v.begin(); it2 != v.end(); ++ it2) {
			if (actor_city && it2->first != actor_city && it2->second != actor_city) {
				continue;
			}
			int a_side = it2->first->side();
			int b_side = it2->second->side();
			if (a_side == b_side && a_side == side) {
				unit* u = units.find_unit(loc);
				if (u && current_team.is_enemy(u->side())) {
					size_t a_distance = distance_between(loc, it2->first->get_location());
					size_t b_distance = distance_between(loc, it2->second->get_location());
					// at middle, max damage.
					int short_dist = a_distance <= b_distance? a_distance: b_distance;
					int damage = maximun_road_damage * short_dist / ((a_distance + b_distance) / 2);
					touched.insert(std::make_pair(u, damage));
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
			un.set_state(ustate_tag::LEGERITIED, true);
			if (un.is_artifical()) {
				un.set_movement(2);
			}
		}

		pathfind::paths p(*resources::game_map, *resources::units, loc, *resources::teams, true, false, tm, 0, false, true);
		BOOST_FOREACH (const pathfind::paths::step &dest, p.destinations) {
			clear_shroud_loc(tm, dest.curr, &cleared_locations);
		}

		if (un.is_artifical() || unit_feature_val2(un, hero_feature_scout)) {
			un.set_state(ustate_tag::LEGERITIED, false);
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
					if (!(sighted->get_state(ustate_tag::PETRIFIED)))
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

class expedite_release
{
public:
	expedite_release(game_display& disp, unit_map& units)
		: disp_(disp)
		, units_(units)
	{}

	~expedite_release()
	{
		if (units_.expediting()) {
			units_.set_expediting();
			disp_.remove_expedite_city();
		}
	}

private:
	game_display& disp_;
	unit_map& units_;
};

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

	const expedite_release release_expedite(disp, units);

	unit_map::iterator ui = units.find(route.front());
	unit& mover = *ui;
	bool mover_is_commoner = ui->is_commoner();
	bool mover_is_trader = ui->is_path_along_road();
	bool mover_is_transport = ui->transport();

	if (mover_is_trader) {
		VALIDATE(ui->task() != unit::TASK_NONE, "move_unit, field trader's task must not be TASK_NONE!");
	}

	bool spectator_in_mover = (move_spectator && move_spectator == &ui->get_move_spectator())? true: false;

	// get city-owner grid condition. move_unit is called must in expediting state used for expedit.  
	void* expediting_city_cookie = NULL;
	if (units.expediting()) {
		expediting_city_cookie = units.expediting_city_node();
	}


	size_t team_num = ui->side()-1;
	team* tm = &teams[team_num];
	play_controller* controller = resources::controller;
	bool unit_is_ai = tm->is_ai() || controller->is_recovering(team_num + 1);

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
	bool discover_by_scout = false;
	unit* alerted_unit = NULL;
	bool should_clear_stack = false;
	std::vector<map_location>::const_iterator step;
	std::string ambushed_string;

	pathfind::last_location = *route.begin();
	for (step = route.begin()+1; step != route.end(); ++step) {
		const map_location& cur_loc = *step;
		if (expediting_city_cookie && units.get_cookie(cur_loc) == expediting_city_cookie) {
			pathfind::last_location = cur_loc;
			continue;
		}
		const bool skirmisher = ui->get_ability_bool("skirmisher", cur_loc);

		int cost;
		unit_map::node* curr_node = reinterpret_cast<unit_map::node*>(units.get_cookie(*step, false));
		if (curr_node && curr_node->second->wall2()) {
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

		// check alert attack, ignore loc that target cann't stand on.
		if (!enemy_unit.valid()) {
			unit_map::const_iterator base = units.find(*step, false);
			if (!base.valid() || base->can_stand(*ui)) {
				alerted_unit = loc_in_alert_area(teams, *ui, pathfind::last_location, *step);
				if (alerted_unit) {
					should_clear_stack = true;
					moves_left = 0;
					alerted_unit->set_alert_turns(0);
					if (move_spectator != NULL) {
						move_spectator->set_ambusher(units.find(alerted_unit->get_location()));
					}
					ambushed_string = dgettext("wesnoth-lib", "Alerted!");
				}
			}
		}

		map_offset* adjacent_ptr;
		size_t adjacent_size;
		map_location adjacent_loc;
		
		adjacent_size = (sizeof(adjacent_1) / sizeof(map_offset)) >> 1;
		adjacent_ptr = adjacent_1[cur_loc.x & 0x1];
		int steps = unit_feature_val2(mover, hero_feature_scout)? 3: 1;
		for (int s = 0; s < steps && !discovered_unit; s ++) {
			if (s == 1) {
				adjacent_size = (sizeof(adjacent_2) / sizeof(map_offset)) >> 1;
				adjacent_ptr = adjacent_2[cur_loc.x & 0x1];
			} else if (s == 2) {
				adjacent_size = (sizeof(adjacent_3) / sizeof(map_offset)) >> 1;
				adjacent_ptr = adjacent_3[cur_loc.x & 0x1];
			}
			for (size_t i = 0; i < adjacent_size; i ++) {
				adjacent_loc.x = cur_loc.x + adjacent_ptr[i].x;
				adjacent_loc.y = cur_loc.y + adjacent_ptr[i].y;

				// Check if we are checking ourselves
				if (adjacent_loc == mover.get_location()) {
					continue;
				}

				unit* u = units.find_unit(adjacent_loc);
				if (u && tm->is_enemy(u->side()) && u->invisible(u->get_location())) {
					discovered_unit = true;
					should_clear_stack = true;
					if (s == 0) {
						moves_left = 0;
						if (u->hide_turns()) {
							u->set_hide_turns(0);
						}
					} else {
						discover_by_scout = true;
					}
					if (move_spectator!=NULL) {
						move_spectator->set_ambusher(units.find(adjacent_loc));
					}

					unit_ability_list hides = u->get_abilities("hides");
					if (!hides.cfgs.empty()) {
						std::vector<std::pair<const config *, unit *> >::const_iterator hide_it = hides.cfgs.begin();
						// we only use the first valid alert message
						for (; hide_it != hides.cfgs.end() && !ambushed_string.empty(); ++ hide_it) {
							ambushed_string = (*hide_it->first)["alert"].str();
						}
					} else if (s == 0) {
						ambushed_string = dgettext("wesnoth-lib", "Ambushed!");
					} else {
						ambushed_string = dgettext("wesnoth-lib", "Discover ambusher!");
					}
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
	// city maybe multi-grid, it should stand at first city-loc.
	std::pair<map_location, int> last_city_loc;
	while (steps.size() >= 2) {
		const map_location& loc = steps.back();
		const unit* u = units.find_unit(loc);
		// tow city must be not adjacent.
		if (!u || (last_city_loc.first.valid() && !u->is_city())) {
			break;
		}
		if (!u->base()) {
			if (unit_is_city(u) && u->side() == ui->side() && (!expediting_city_cookie || units.get_cookie(loc) != expediting_city_cookie)) {
				last_city_loc.first = loc;
			}
		} else if (u->can_stand(*ui)) {
			break;
		}
		int cost = ui->movement_cost(map[loc]);
		moves_left += cost;
		steps.pop_back();

		if (last_city_loc.first.valid()) {
			last_city_loc.second = cost;
		}
	}
	if (last_city_loc.first.valid()) {
		moves_left -= last_city_loc.second;
		steps.push_back(last_city_loc.first);
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
			if (!mover_is_trader && move_spectator==NULL) {
				// I don't understand why call below, wesnoth call it!
				// But I can sure, commoner cannnot call it. goto_ is fixed to city.
				ui->set_goto(route.back());
			}
		}
	}

	if (steps.size() < 2) {
		if (mover_is_trader && !units.expediting()) {
			ui->inching_block_turns(true);
			move_recorder->add_inching_block(*ui, true);
		}
		return 0;
	}
	if (mover_is_trader) {
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
	if (mover_is_trader && entered_city) {
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
			if (!mover.get_state(ustate_tag::SLOWED) && !discover_by_scout) {
				mover.set_state(ustate_tag::SLOWED, true);
			}
		} else {
			mover.set_state(ustate_tag::UNCOVERED, true);
		}
		if (!discover_by_scout) {
			ui->set_hide_turns(0);
		}
	}

	disp.invalidate_unit_after_move(steps.front(), steps.back());

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
	if (!mover_is_commoner) {
		if (to_city) {
			map_location loc2(MAGIC_RESIDE, to_city->reside_troops().size() - 1);
			game_events::raise("comeinto", steps.back(), loc2);
		} else {
			game_events::raise("moveto", steps.back(), steps.front());
		}
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
			undo_stack->push_back(undo_action(*maybe_ui, steps, starting_moves, 0, -1, units.last_expedite_index()));
		} else {
			undo_stack->push_back(undo_action(*maybe_ui, steps, starting_moves, 0, -1));
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
	if (mover_is_trader && entered_city) {
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
	} else if (mover_is_transport && maybe_ui.valid()) {
		maybe_ui->set_block_turns(0);

		unit_map::iterator it = units.find(steps.back(), false);
		if (it != units.end() && it->fort()) {
			do_supply(*tm, units, *maybe_ui, *unit_2_artifical(&*it));
			maybe_ui = units.find(steps.back());
			if (move_spectator && !spectator_in_mover) {
				move_spectator->set_unit(maybe_ui);
			}
			steps.clear();
		}
	}

	// because undo, can select: 
	// 1)get one card random must before add movement/expedite to recorder. 
	// 2)if get one card, cannot undo.
	// I select 2).
	if (!controller->is_recovering(team_num + 1) && alerted_unit) {
		// attack no defend!
		if (maybe_ui.valid() && !no_fightback_attack(*alerted_unit, *maybe_ui)) {
			move_spectator->set_unit(units.end());
		}
		event_mutated = true;

	} else if (!controller->is_recovering(team_num + 1) && !to_city && maybe_ui.valid()) {
		int random = rand();
		unit& mover2 = *maybe_ui;
		// 1/16---1/8
		int encourage_percent = 16 - (8 * mover2.character_level(apply_to_tag::UNITED)) / 100;
		if (!(random & 0x1ff)) {
			get_random_card(*tm, disp, units, heros);
			event_mutated = true;
			
		} else if (random % encourage_percent == 1) {
			// suport to encourage
			if (mover2.hitpoints() < mover2.max_hitpoints() * 2 / 3) {
				hero* h2 = mover2.can_encourage();
				if (h2) {
					// must call add_event(replay::EVENT_ENCOURAGE, ...) before mover2.do_encourage.
					// when replaying, increase_feeling in add_event may enquire choice, it is said [input] must after add_event. 
					move_recorder->add_event(replay::EVENT_ENCOURAGE, mover2.get_location());
					mover2.do_encourage(mover2.master(), *h2);
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

std::pair<map_location, map_location> attack_enemy(unit* a_, unit* d_, int att_weapon, int def_weapon, bool move, bool constant_attacks)
{
	unit_map& units_ = *resources::units;
	std::vector<team>& teams_ = *resources::teams;
	
	const map_location& attacker_loc = a_->get_location();
	const map_location& defender_loc = d_->get_location();

	std::pair<map_location, map_location> to_locs = std::make_pair(attacker_loc, defender_loc);

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
		resources::tod_manager->get_time_of_day(), move, constant_attacks);
	rand_rng::invalidate_seed();
	rand_rng::clear_new_seed_callback();
	while (!rand_rng::has_valid_seed()) {
		ai::manager::raise_user_interact();
		ai::manager::raise_sync_network();
		SDL_Delay(10);
	}
	recorder.add_seed("attack", rand_rng::get_last_seed());


	to_locs = attack_unit(*a_, *d_, att_weapon, def_weapon, true, config::invalid, move, constant_attacks);

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

bool backstab_check(const unit& attacker, const unit& defender, const unit_map& units, const std::vector<team>& teams)
{
	if (defender.adjacent_size_ != 6) {
		return false;
	}
	const map_location& attacker_loc = attacker.get_location();

	size_t i;
	for (i = 0; i != defender.adjacent_size_; ++ i) {
		if (defender.adjacent_[i] == attacker_loc)
			break;
	}
	if (i >= defender.adjacent_size_) return false;  // Attack not from adjacent location

	const unit* opp = units.find_unit(defender.adjacent_[(i + defender.adjacent_size_ / 2) % defender.adjacent_size_]);
	if (!opp) {
		return false; // No opposite unit
	}
	if (opp->incapacitated() || opp->wall()) return false;
	if (teams[defender.side()-1].is_enemy(opp->side())) {
		return true; // Defender and opposite are enemies
	}
	return false; // Defender and opposite are friends
}

void refresh_chat_button(game_display& disp, const std::string& fg)
{
	gui2::tbutton* widget = dynamic_cast<gui2::tbutton*>(disp.get_theme_object("chat"));

	surface surf = image::get_image("buttons/chat.png");
	surface fg_surf = image::get_image(fg);
	if (fg_surf) {
		surf = generate_pip_surface(surf, fg_surf);
	}

	widget->set_surface(surf, widget->get_width(), widget->get_height());
}

void refresh_card_button(const team& t, game_display& disp)
{
	std::stringstream strstr;
	gui2::tbutton* widget = dynamic_cast<gui2::tbutton*>(disp.get_theme_object("card"));	

	SDL_Color color;
	int cards = t.holded_cards().size();

	if (cards < game_config::max_cards * 3 / 4) {
		color = font::BUTTON_COLOR;
	} else if (cards < game_config::max_cards) {
		color = font::YELLOW_COLOR;
	} else {
		color = font::BAD_COLOR;
	}
	surface fg = font::get_rendered_text2(str_cast(cards), -1, font::SIZE_NORMAL, color);
	surface surf = scale_surface(image::get_image("buttons/card.png"), widget->get_width(), widget->get_height());
	surf = generate_pip_surface(surf, fg);

	widget->set_surface(surf, widget->get_width(), widget->get_height());
}

void refresh_endturn_button(game_display& disp, const std::string& bg)
{
	gui2::tbutton* widget = dynamic_cast<gui2::tbutton*>(disp.get_theme_object("endturn"));

	surface surf = image::get_image(bg);
	widget->set_surface(surf, widget->get_width(), widget->get_height());
}

void get_random_card(team& t, game_display& disp, unit_map& units, hero_map& heros)
{
	bool added = t.add_card(CARDS_INVALID_NUMBER, false, true);
	if (added && t.is_human()) {
		refresh_card_button(t, disp);
		// card
		utils::string_map symbols;
		symbols["first"] = tintegrate::generate_format(t.holded_card(t.holded_cards().size() - 1).name(), tintegrate::object_color);
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

// 1. wall include wall and keep.
// 2. terrain match.
// 3. location accord condiation. 1th no city and 2th has city(it's side == side).
artifical* loc_can_build_wall(unit_map& units, const gamemap& gmap, const map_location& loc, const team& t)
{
	int side = t.side();
	const std::set<const unit_type*>& can_build = t.builds();

	std::set<const unit_type*> candidate;
	candidate.insert(unit_types.find_keep());
	candidate.insert(unit_types.find_wall());

	bool terrain_matched = false;
	for (std::set<const unit_type*>::const_iterator it = candidate.begin(); it != candidate.end(); ++ it) {
		const unit_type* ut = *it;
		if (can_build.find(ut) == can_build.end()) {
			continue;
		}
		if (ut->terrain_matches(gmap.get_terrain(loc))) {
			terrain_matched = true;
		}
	}
	if (!terrain_matched) {
		return NULL;
	}

	artifical* ret = NULL;
	map_offset* adjacent2_ptr = adjacent_1[loc.x & 0x1];
	size_t i2, size2 = (sizeof(adjacent_1) / sizeof(map_offset)) >> 1;
	for (i2 = 0; i2 < size2; i2 ++) {
		artifical* city = units.city_from_loc(map_location(loc.x + adjacent2_ptr[i2].x, loc.y + adjacent2_ptr[i2].y));
		if (city) {
			break;
		}
	}
	if (i2 != size2) {
		return NULL;
	}
	adjacent2_ptr = adjacent_2[loc.x & 0x1];
	size2 = (sizeof(adjacent_2) / sizeof(map_offset)) >> 1;
	for (i2 = 0; i2 < size2; i2 ++) {
		artifical* city = units.city_from_loc(map_location(loc.x + adjacent2_ptr[i2].x, loc.y + adjacent2_ptr[i2].y));
		if (city && city->side() == side) {
			ret = city;
			break;
		}
	}
	return ret;
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
	int left_skill = 15, right_skill = 15;

	if (percentage > 50) {
		left_u.set_state(ustate_tag::BROKEN, false);
		left_u.set_state(ustate_tag::SLOWED, false);

		if (!right_u.get_state(ustate_tag::BROKEN)) {
			float_text << _("broken");
			right_u.set_state(ustate_tag::BROKEN, true);
		}
		if (percentage > 70) {
			if (!right_u.get_state(ustate_tag::SLOWED)) {
				if (!float_text.str().empty()) {
					float_text << '\n';
				}
				float_text << _("slowed");
				right_u.set_state(ustate_tag::SLOWED, true);
			}
		}

		if (percentage == 100) {
			kill = std::min(gui2::tduel::min_hp_, right_u.hitpoints() / 3);
			right_u.take_hit(kill);
			if (!float_text.str().empty()) {
				float_text << '\n';
			}
			float_text << kill;

			left_skill = 40;
			right_skill = 5;
		} else {
			left_skill = 20;
			right_skill = 10;
		}

		if (!float_text.str().empty()) {
			unit_display::unit_text(right_u, true, float_text.str());
		}

	} else if (percentage < 50) {
		right_u.set_state(ustate_tag::BROKEN, false);
		right_u.set_state(ustate_tag::SLOWED, false);

		if (!left_u.get_state(ustate_tag::BROKEN)) {
			float_text << _("broken");
			left_u.set_state(ustate_tag::BROKEN, true);
		}
		if (percentage < 30) {
			if (!left_u.get_state(ustate_tag::SLOWED)) {
				if (!float_text.str().empty()) {
					float_text << '\n';
				}
				float_text << _("slowed");
				left_u.set_state(ustate_tag::SLOWED, true);
			}
		}
		if (percentage == 0) {
			kill = std::min(gui2::tduel::min_hp_, left_u.hitpoints() / 3);
			left_u.take_hit(kill);
			if (!float_text.str().empty()) {
				float_text << '\n';
			}
			float_text << kill;

			left_skill = 5;
			right_skill = 40;
		} else {
			left_skill = 10;
			right_skill = 20;
		}

		if (!float_text.str().empty()) {
			unit_display::unit_text(left_u, true, float_text.str());
		}
	}
	left_u.get_experience(increase_xp::duel_ublock(left_u), left_skill);
	right_u.get_experience(increase_xp::duel_ublock(right_u), right_skill);

	config duel;
	duel["left"] = left.number_;
	duel["right"] = right.number_;
	duel["percentage"] = percentage;
	resources::state_of_game->add_variable_cfg("duel", duel);

	game_events::fire("post_duel", left_u.get_location(), right_u.get_location());

	resources::state_of_game->clear_variable_cfg("duel");
}

int calculate_exploiture(const hero& h1, const hero& h2, int type)
{
	int assistant_ability, skill, exploiture;

	if (h2.valid()) {
		if (type == department::commercial) {
			assistant_ability = fxptoi9(h2.spirit_) * 100 / 250; // oath relative
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
		exploiture = fxptoi9(h1.spirit_) + assistant_ability;
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

void do_recruit(unit_map& units, hero_map& heros, std::vector<team>& teams, team& current_team, const unit_type* ut, std::vector<const hero*>& v, artifical& city, int cost, bool human, bool to_recorder, game_state& state)
{
	type_heros_pair pair(ut, v);

	if (to_recorder) {
		recorder.add_recruit(ut->id(), city.get_location(), v, cost, human);
	}

	unit* new_unit = new unit(units, heros, teams, state, pair, city.cityno(), true, false);
	new_unit->set_movement(new_unit->total_movement());
	new_unit->set_attacks(new_unit->attacks_total());

	current_team.spend_gold(cost);

	new_unit->set_human(human);
	city.troop_come_into(new_unit, -1);

	for (std::vector<const hero*>::const_iterator it = v.begin(); it != v.end(); ++ it) {
		const hero& h = **it;
		if (h.number_ >= hero::number_normal_min) {
			city.hero_go_out(h);
		}
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
		if (ttactic::select_one(part_tactic.apply_to())) {
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

void cast_tactic(std::vector<team>& teams, unit_map& units, unit& tactician, hero& h, unit* special, bool to_recorder, bool consume, int cost)
{
	const events::command_disabler disable_commands;

	team& current_team = teams[h.side_];
	const ttactic& t = unit_types.tactic(h.tactic_);
	const std::map<int, std::vector<map_location> > touched = t.touch_units(units, tactician);
	int part, turn;

	if (t.select_one() && !special) {
		special = select_provoke_target(units, current_team, tactician, t);
	}

	if (to_recorder) {
		recorder.add_cast_tactic(tactician, h, special, consume, cost);
		// must called before tactician.get_experience(2).
		// tactician.get_experience(2) may generate [input](select human troop),
		// replay should known [input] is generate by cast_tactic.
	}

	if (consume) {
		for (size_t n = 0; n < teams.size(); n ++) {
			team& tm = teams[n];
			if (!tm.stratagem_baffle_tactic || !current_team.is_enemy(n + 1)) {
				continue;
			}
			int random = get_random();
			if (random % 3 < 2) {
				unit_display::stratagem_anim(stratagem_tag::technology(stratagem_tag::jiang_lang_cai_jin), h.image(true), false);
				
				tactician.increase_tactic_degree(-1 * t.point() * game_config::tactic_degree_per_point);
				return;
			}
		}
	}

	if (cost) {
		current_team.spend_gold(cost);
	}

	game_display& disp = *resources::screen;
	const rect_of_hexes& draw_area = disp.draw_area();
	if ((to_recorder && tactician.human()) || point_in_rect_of_hexes(tactician.get_location().x, tactician.get_location().y, draw_area)) {
		disp.scroll_to_tile(tactician.get_location(), game_display::ONSCREEN, true, true);
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
		} else if (part_tactic.apply_to() == apply_to_tag::REVIVAL) {
			if (special) {
				special->do_revival(tactician);
			}
		} else {
			unit_display::tactic_anim_lock lock(*resources::screen, tactician);
			for (std::vector<map_location>::const_iterator it2 = it->second.begin(); it2 != it->second.end(); ++ it2) {
				unit* u = units.find_unit(*it2);
				if (!u) {
					// this unit may be erased by previous tactic. for example "damage".
					continue;
				}
				u->apply_tactic(&t, part_tactic, part, turn);
			}
		}
	}

	if (consume) {
		if (tactician.hot_turns()) {
			tactician.set_hot_turns(0);
		}
		tactician.increase_tactic_degree(-1 * t.point() * game_config::tactic_degree_per_point);
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

void do_clear_formationed(game_display& disp, std::vector<team>& teams, unit_map& units, unit& u, int cost, bool to_recorder)
{
	team& current_team = teams[u.side() - 1];

	if (to_recorder) {
		recorder.add_clear_formationed(u, cost);
	}

	if (cost) {
		current_team.spend_gold(cost);
	}

	std::vector<unit*> touched;
	if (u.get_state(ustate_tag::FORMATION_ATTACKED)) {
		u.set_state(ustate_tag::FORMATION_ATTACKED, false);
		touched.push_back(&u);
	}
	for (size_t adj = 0; adj < u.adjacent_size_; adj ++) {
		unit* u2 = units.find_unit(u.adjacent_[adj]);
		if (u2 && u2->get_state(ustate_tag::FORMATION_ATTACKED) && !current_team.is_enemy(u2->side())) {
			u2->set_state(ustate_tag::FORMATION_ATTACKED, false);
			touched.push_back(u2);
		}
	}

	std::stringstream strstr;
	unit_display::tactic_anim_lock lock(disp, u, false);
	for (std::vector<unit*>::const_iterator it = touched.begin(); it != touched.end(); ++ it) {
		strstr.str("");
		strstr << tintegrate::generate_img("misc/formation.png");
		unit_display::unit_text(**it, false, strstr.str());
	}
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
			} else if (relative == "spirit") {
				damage = u.spirit_ - opp.spirit_;
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
			if (attacker) {
				attack::unit_info info(*attacker, -1, units);
				unit_die(units, opp, &info, 0, a_side);
			} else {
				unit_die(units, opp, NULL, 0, a_side);
			}
		}
	}

	try {
		ai::manager::raise_gamestate_changed();
		resources::controller->check_victory();
		resources::controller->check_end_level();
	} catch (...) {
		throw;
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
		strstr << tintegrate::generate_img("misc/gold.png");
		// tintegrate::generate_img(strstr, "misc/increase.png");
		strstr << gold_income;
	}

	int technology_income = commoner.technology_income() * bonus_city.revenue_income_ / divisor;
	if (technology_income) {
		bonus_city.add_technology_bonus(technology_income);
		if (gold_income) {
			strstr << "\n";
		}
		strstr << tintegrate::generate_img("misc/technology.png");
		// tintegrate::generate_img(strstr, "misc/increase.png");
		strstr << technology_income;
		
	}

	if (!strstr.str().empty()) {
		unit_display::unit_text(bonus_city, false, strstr.str());
	}

	commoner.get_experience(increase_xp::attack_ublock(commoner), 8);
	dialogs::advance_unit(commoner, !current_team.is_human(), true);
}

void do_supply(team& current_team, unit_map& units, unit& transport, artifical& fort)
{
	std::stringstream strstr;

	fort.increase_food(transport.food_income());

	strstr << tintegrate::generate_img("misc/food.png");
	// tintegrate::generate_img(strstr, "misc/increase.png");
	strstr << transport.food_income();

	if (!strstr.str().empty()) {
		unit_display::unit_text(fort, false, strstr.str());
	}
	units.erase(&transport);
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
		if (!it->attack_weight()) {
			continue;
		}
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

	std::pair<map_location, map_location> to_locs = attack_enemy(&attacker, &defender, weapon_index, -1, false, true);
	return to_locs.second.valid();
}

tformation::tformation()
	: u_(NULL)
	, adjacent_()
	, necessary_()
	, profile_(NULL)
	, disable_(false)
	, gold_(0)
{}

tformation::tformation(unit* u, const std::map<int, unit*>& adjacent)
	: u_(u)
	, adjacent_(adjacent)
	, necessary_()
	, profile_(NULL)
	, disable_(false)
	, gold_(0)
{}

const tformation_profile* tformation::match_to()
{
	const std::map<int, tformation_profile>& formations = unit_types.formations();
	std::set<unit*> necessary;
	for (std::map<int, tformation_profile>::const_iterator it = formations.begin(); it != formations.end(); ++ it) {
		const tformation_profile& profile = it->second;
		if (profile.arms_count_ > (int)adjacent_.size()) {
			continue;
		}
		necessary.clear();
		for (std::map<int, unit*>::const_iterator it2 = adjacent_.begin(); it2 != adjacent_.end(); ++ it2) {
			unit* u = it2->second;
			if (u->weapon(profile.arms_type_, profile.arms_range_id_) >= 0) {
				necessary.insert(it2->second);
			}
		}
		if ((int)necessary.size() >= profile.arms_count_) {
			profile_ = &it->second;
			necessary_ = necessary;
			break;
		}
	}

	return profile_;
}

bool tformation::valid(bool attacking) const
{
	if (attacking) {
		return profile_? true: false;
	} else {
		return u_? true: false;
	}
}

namespace compare_formation {
int current_side;

bool func(const tformation& a, const tformation& b)
{
	unit& a_u = *a.u_;
	unit& b_u = *b.u_;

	if (a_u.side() != b_u.side()) {
		if (a_u.side() == current_side) {
			return true;
		} else if (b_u.side() == current_side) {
			return false;
		}
	}

	if (a_u.packee_type()->require() == unit_type::REQUIRE_LEADER && b_u.packee_type()->require() != unit_type::REQUIRE_LEADER) {
		return true;
	}
	if (a_u.packee_type()->require() != unit_type::REQUIRE_LEADER && b_u.packee_type()->require() == unit_type::REQUIRE_LEADER) {
		return false;
	}
	if (a_u.skill_[hero_skill_formation] > b_u.skill_[hero_skill_formation]) {
		return true;
	} else if (a_u.skill_[hero_skill_formation] < b_u.skill_[hero_skill_formation]) {
		return false;
	}
	if (a.adjacent_.size() > b.adjacent_.size()) {
		return true;
	} else if (a.adjacent_.size() < b.adjacent_.size()) {
		return false;
	}
	return a_u.get_location() < b_u.get_location();
}

}

void calculate_formations_bh(std::vector<tformation>& formations, int current_side, bool attacking)
{
	if (formations.empty()) {
		return;
	}

	for (std::vector<tformation>::iterator it = formations.begin(); it != formations.end(); ) {
		tformation& formation = *it;
		if (attacking) {
			if (!formation.match_to()) {
				it = formations.erase(it);
			} else {
				++ it;
			}
		} else {
			for (std::map<int, unit*>::const_iterator it2 = formation.adjacent_.begin(); it2 != formation.adjacent_.end(); ++ it2) {
				formation.necessary_.insert(it2->second);
			}
			++ it;
		}
	}
	
	// sort formations
	compare_formation::current_side = current_side;
	std::sort(formations.begin(), formations.end(), compare_formation::func);

	// erase formation if necessary
	std::set<const unit*> formationed_unit;
	for (std::vector<tformation>::iterator it = formations.begin(); it != formations.end(); ) {
		tformation& formation = *it;
		if (formationed_unit.find(formation.u_) == formationed_unit.end()) {
			formationed_unit.insert(formation.u_);
		} else {
			it = formations.erase(it);
			continue;
		}
		bool disable = !formation.u_->formation_attack_enable() || formation.u_->side() != current_side;
		for (std::map<int, unit*>::iterator it2 = formation.adjacent_.begin(); it2 != formation.adjacent_.end(); ) {
			unit* adj = it2->second;
			if (formationed_unit.find(adj) == formationed_unit.end()) {
				formationed_unit.insert(adj);
			} else {
				formation.adjacent_.erase(it2 ++);
				formation.necessary_.erase(adj);
				continue;
			}
			if (!disable) {
				disable = !adj->formation_attack_enable();
			}
			++ it2;
		}
		if ((int)formation.adjacent_.size() >= game_config::formation_least_adjacent && 
			(!formation.profile_ || (int)formation.necessary_.size() >= formation.profile_->arms_count_)) {
			formation.disable_ = disable;
			++ it;
		} else {
			// release included unit
			formationed_unit.erase(formation.u_);
			for (std::map<int, unit*>::const_iterator it2 = formation.adjacent_.begin(); it2 != formation.adjacent_.end(); ++ it2) {
				formationed_unit.erase(it2->second);
			}

			it = formations.erase(it);
		}
	}
}

void calculate_formations(const std::vector<team>& teams, const unit_map& units, int current_side, const std::set<const unit*>& origin, const rect_of_hexes* rect, bool must_enable, std::vector<tformation>& formations)
{
	formations.clear();

	// generate possible tactical formation.
	std::set<const unit*> uncalculated = origin;
	std::set<const unit*> collected;
	if (!rect) {
		// if rect isn't null, origin is get rid of by rect. use rect is more effective than use collected.
		collected = origin;
	}

	std::map<int, unit*> valid_adjacent;
	while (!uncalculated.empty()) {
		unit& u = *const_cast<unit*>(*uncalculated.begin());
		uncalculated.erase(&u);

		// don't check can_formation_master of u at here.
		// need find other unit by below code.

		const map_location& u_loc = u.get_location();
		size_t size = (sizeof(adjacent_1) / sizeof(map_offset)) >> 1;
		map_offset* adjacent_ptr = adjacent_1[u_loc.x & 0x1];
		for (size_t adj = 0; adj < size; adj ++) {
			map_location loc(u_loc.x + adjacent_ptr[adj].x, u_loc.y + adjacent_ptr[adj].y);
			unit* that = units.find_unit(loc);
			if (!that || that->is_artifical() || that->is_commoner() || teams[that->side() - 1].is_enemy(u.side())) {
				continue;
			}
			if (must_enable && !that->formation_attack_enable()) {
				continue;
			}
			valid_adjacent.insert(std::make_pair(adj, that));
			if (rect && point_in_rect_of_hexes(that->get_location().x, that->get_location().y, *rect)) {
				continue;
			}
			if (collected.find(that) == collected.end()) {
				collected.insert(that);
				uncalculated.insert(that);
			}
		}

		if ((int)valid_adjacent.size() >= game_config::formation_least_adjacent && u.can_formation_master()) {
			formations.push_back(tformation(&u, valid_adjacent));
		}
		valid_adjacent.clear();
	}

	calculate_formations_bh(formations, current_side, true);
}

void calculate_defender_formations(const std::vector<team>& teams, const unit_map& units, int current_side, const unit& defender, const std::set<const unit*>& helper, std::vector<tformation>& formations)
{
	formations.clear();
	const map_location& defender_loc = defender.get_location();

	// generate possible tactical formation.
	std::set<const unit*> uncalculated = helper;
	uncalculated.insert(&defender);
	std::set<const unit*> collected = uncalculated;

	std::map<int, unit*> valid_adjacent;
	while (!uncalculated.empty()) {
		unit& u = *const_cast<unit*>(*uncalculated.begin());
		uncalculated.erase(&u);

		// don't check can_formation_master of u at here.
		// need find other unit by below code.

		const map_location& u_loc = u.get_location();
		size_t size = (sizeof(adjacent_1) / sizeof(map_offset)) >> 1;
		map_offset* adjacent_ptr = adjacent_1[u_loc.x & 0x1];
		for (size_t adj = 0; adj < size; adj ++) {
			map_location loc(u_loc.x + adjacent_ptr[adj].x, u_loc.y + adjacent_ptr[adj].y);
			unit* that = units.find_unit(loc);
			if (!that || that->is_artifical() || that->is_commoner() || teams[that->side() - 1].is_enemy(u.side())) {
				continue;
			}
			valid_adjacent.insert(std::make_pair(adj, that));
			if (tiles_adjacent(defender_loc, that->get_location()) && collected.find(that) == collected.end()) {
				collected.insert(that);
				uncalculated.insert(that);
			}
		}
		if ((int)valid_adjacent.size() >= game_config::formation_least_adjacent) {
			// check
			bool has_defender = false;
			bool has_helper = false;
			if (&u == &defender) {
				has_defender = true;
			} else if (helper.find(&u) != helper.end()) {
				has_helper = true;
			}

			for (std::map<int, unit*>::iterator it = valid_adjacent.begin(); it != valid_adjacent.end(); ++ it) {
				const unit* adj = it->second;
				if (adj == &defender) {
					has_defender = true;
				} else if (helper.find(adj) != helper.end()) {
					has_helper = true;
				}
			}

			if (has_defender && has_helper && u.can_formation_master()) {
				formations.push_back(tformation(&u, valid_adjacent));
			}
		}
		valid_adjacent.clear();
	}

	calculate_formations_bh(formations, current_side, false);
}

tformation calculate_attack_formation(const std::vector<team>& teams, const unit_map& units, const unit& attacker, bool must_enable)
{
	std::vector<tformation> formations;
	std::set<const unit*> origin;
	origin.insert(&attacker);
	calculate_formations(teams, units, attacker.side(), origin, NULL, must_enable, formations);

	tformation* choice = NULL;
	for (std::vector<tformation>::iterator it = formations.begin(); it != formations.end(); ++ it) {
		tformation& formation = *it;
		if (formation.u_ == &attacker) {
			choice = &formation;
			break;
		}
		for (std::map<int, unit*>::iterator it2 = formation.adjacent_.begin(); it2 != formation.adjacent_.end(); ++ it2) {
			if (it2->second == &attacker) {
				choice = &formation;
				break;
			}
		}
		if (choice) {
			break;
		}
	}
	if (choice) {
		return *choice;
	}
	return tformation();
}

std::vector<unit*> calculate_attackable(unit_map& units, team& current_team, const unit& attacker, int range)
{
	std::vector<unit*> attackable;
	if (range < 0 || range >= 3) {
		return attackable;
	}

	const map_location* tiles = attacker.adjacent_;
	size_t adjance_size = attacker.adjacent_size_;
	if (range == 1) {
		tiles = attacker.adjacent_2_;
		adjance_size = attacker.adjacent_size_2_;
	} else if (range == 2) {
		tiles = attacker.adjacent_3_;
		adjance_size = attacker.adjacent_size_3_;
	}

	for (size_t adj = 0; adj < adjance_size; adj ++) {
		const map_location& loc = tiles[adj];
		unit* that = units.find_unit(loc);
		if (!that || !current_team.is_enemy(that->side())) {
			continue;
		}
		if (std::find(attackable.begin(), attackable.end(), that) == attackable.end()) {
			attackable.push_back(that);
		}
	}
	return attackable;
}

bool formation_can_attack(unit_map& units, team& current_team, const tformation& formation) 
{
	const tformation_profile& profile = *formation.profile_;

	std::vector<std::pair<unit*, int> > attacks;
	int weapon = formation.u_->weapon(profile.arms_type_, profile.arms_range_id_);
	if (weapon >= 0) {
		attacks.push_back(std::make_pair(formation.u_, weapon));
	}
	for (std::map<int, unit*>::const_iterator it = formation.adjacent_.begin(); it != formation.adjacent_.end(); ++ it) {
		unit* u = it->second;
		weapon = u->weapon(profile.arms_type_, profile.arms_range_id_);
		if (weapon >= 0) {
			attacks.push_back(std::make_pair(u, weapon));
		}
	}

	std::vector<unit*> v;
	for (std::vector<std::pair<unit*, int> >::iterator it = attacks.begin(); it != attacks.end(); ++ it) {
		v = calculate_attackable(units, current_team, *it->first, profile.arms_range_);
		if (!v.empty()) {
			return true;
		}
	}
	return false;
}

bool formation_attack_internal(unit_map& units, team& current_team, tformation& formation, bool replay) 
{
	const tformation_profile& profile = *formation.profile_;

	std::vector<std::pair<unit*, int> > attacks;
	int weapon = formation.u_->weapon(profile.arms_type_, profile.arms_range_id_);
	if (weapon >= 0) {
		attacks.push_back(std::make_pair(formation.u_, weapon));
	}
	for (std::map<int, unit*>::const_iterator it = formation.adjacent_.begin(); it != formation.adjacent_.end(); ++ it) {
		unit* u = it->second;
		weapon = u->weapon(profile.arms_type_, profile.arms_range_id_);
		if (weapon >= 0) {
			attacks.push_back(std::make_pair(u, weapon));
		}
	}

	bool attacked = false;
	{
		unit_display::formation_anim_lock lock(*resources::screen, formation, &attack::anim_lock);
		
		std::map<int, std::vector<unit*> > attackable;
		std::set<map_location> attacker_locs, defender_locs;
		std::vector<unit*> v;
		for (std::vector<std::pair<unit*, int> >::iterator it = attacks.begin(); it != attacks.end(); ++ it) {
			if (current_team.is_enemy(it->first->side())) {
				// previous attack maybe triger event, result to change succedent diplomatism.
				continue;
			}
			attackable.clear();
			size_t start = std::distance(attacks.begin(), it);
			for (size_t i = start; i < attacks.size(); i ++) {
				v = calculate_attackable(units, current_team, *attacks[i].first, profile.arms_range_);
				attackable[i] = v;
			}
			if (attackable[start].size() >= 2) {
				// set that succedent single target.
				// if exsit more, erase that belong to succedent single target.
				std::set<unit*> those_single;
				for (size_t i2 = start + 1; i2 < attacks.size(); i2 ++) {
					if (attackable[i2].size() == 1) {
						those_single.insert(attackable[i2][0]);
					}
				}
				if (!those_single.empty()) {
					for (std::vector<unit*>::iterator it2 = attackable[start].begin(); it2 != attackable[start].end(); ) {
						std::set<unit*>::iterator find = those_single.find(*it2);
						if (find != those_single.end()) {
							it2 = attackable[start].erase(it2);
							if (attackable[start].size() < 2) {
								break;
							}
						} else {
							++ it2;
						}
					}
				}
			}

			if (!attackable[start].empty()) {
				if (!attacked) {
					formation.u_->set_state(ustate_tag::FORMATION_ATTACKED, true);
					for (std::map<int, unit*>::const_iterator it2 = formation.adjacent_.begin(); it2 != formation.adjacent_.end(); ++ it2) {
						it2->second->set_state(ustate_tag::FORMATION_ATTACKED, true);
					}
					attacked = true;
				}
				if (current_team.kill_income) {
					formation.gold_ += 10;
				}
				unit* target = attackable[start][it->first->hitpoints() % attackable[start].size()];

				// attack_unit maybe change attacker's location(capture city), destroy defender, place them before it. 
				attacker_locs.insert(it->first->get_location());
				defender_locs.insert(target->get_location());

				attack_unit(*it->first, *target, it->second, -1, true, config::invalid, false, true);
			}
		}
		if (formation.gold_) {
			current_team.spend_gold(-1 * formation.gold_);
		}

		lock.flush();

		if (attacked) {
			// post attack
			for (std::set<map_location>::const_iterator it = attacker_locs.begin(); it != attacker_locs.end(); ++ it) {
				unit* u = units.find_unit(*it);
				if (u && u->advances()) {
					if (replay) {
						get_replay_source().add_expected_advancement(u);
					} else {
						dialogs::advance_unit(*u, !u->human());
					}
				}
			}
			for (std::set<map_location>::const_iterator it = defender_locs.begin(); it != defender_locs.end(); ++ it) {
				unit* u = units.find_unit(*it);
				if (u && u->advances()) {
					if (replay) {
						get_replay_source().add_expected_advancement(u);
					} else {
						dialogs::advance_unit(*u, !u->human());
					}
				}
			}
			if (!attacker_locs.empty() && !defender_locs.empty()) {
				try {
					ai::manager::raise_gamestate_changed();
					resources::controller->check_victory();
					resources::controller->check_end_level();
				} catch (...) {
					throw;
				}
			}
		}
	}

	return attacked;
}

bool do_formation_attack(std::vector<team>& teams, unit_map& units, game_display& gui, team& current_team, unit& attacker, bool verify, bool replay)
{
	const events::command_disabler disable_commands;

	tformation formation = calculate_attack_formation(teams, units, attacker, true);
	if (!verify && (!formation.valid() || formation.disable_)) {
		return false;
	}

	if (!replay) {
		recorder.add_formation_attack(attacker.get_location());
	}
	VALIDATE(formation.valid() && !formation.disable_, "do_formation_attack, cannot find valid formation!");

	return formation_attack_internal(units, current_team, formation, replay);
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

	if (!u->is_artifical()) {
		for (std::vector<hero*>::iterator it = vh.begin(); it != vh.end(); ++ it) {
			city->fresh_into(**it);
		}
	}

	units.erase(u);
	current_team.spend_gold(-1 * income);

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

void do_purchase(game_display& gui, team& current_team, int type, int cost, bool replay)
{
	if (!replay) {
		recorder.add_purchase(type, cost);
	}
	if (type == PURCHASE_MOVE) {
		current_team.increase_bomb_turns();
		current_team.refresh_tactic_slots(gui);
	}
	current_team.spend_gold(cost);
}

void do_fresh_heros(team& current_team, bool replay)
{
	if (!replay) {
		recorder.add_fresh_heros();
	}
	std::vector<artifical*>& holden_cities = current_team.holden_cities();
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
	u.remove_from_slot_cache();
}

void do_direct_expedite(std::vector<team>& teams, unit_map& units, artifical& city, int index, const map_location to, bool to_recorder)
{
	if (to_recorder) {
		std::vector<map_location> steps;
		steps.push_back(city.get_location());
		steps.push_back(to);
		recorder.add_expedite(true, index, steps, true, true);
	}

	units.set_expediting(&city, true, index);
	units.move(city.get_location(), to);
	unit& u = *units.find_unit(to);
	u.set_movement(0);
}

void do_direct_move(std::vector<team>& teams, unit_map& units, gamemap& map, unit& u, const map_location to, int cost, bool to_recorder) 
{
	const map_location& from = u.get_location();
	team& current_team = teams[u.side() - 1];

	if (to_recorder) {
		std::vector<map_location> steps;
		steps.push_back(from);
		steps.push_back(to);
		recorder.add_movement(steps, true, cost);
	}
	
	units.move(from, to);
	VALIDATE(&u == &*units.find(to), "mouse_handler::direct_move_unit, move modify u incorrectly!");
	if (map.is_village(to)) {
		int orig_village_owner = village_owner(to, teams);
		if (size_t(orig_village_owner) != u.side() - 1) {
			get_village(to, u.side());
		}
	}

	if (cost) {
		current_team.spend_gold(cost);
	}
	u.set_movement(0);
	u.set_standing();

	// location changed, 
	game_display* disp = resources::screen;
	if (disp) {
		disp->resort_access_troops(u);
	} else {
		units.resort_map(u);
	}
	unit_display::unit_recruited(u.get_location());
}

void do_set_task(unit_map& units, unit& u, int task, const map_location& at, bool to_recorder)
{
	if (to_recorder) {
		recorder.add_set_task(u, task, at);
	}

	if (task == unit::TASK_GUARD) {
		map_location adjusted_at = at;
		const artifical* city = units.city_from_loc(at);
		if (city) {
			adjusted_at = city->get_location();
		}
		u.set_guard(adjusted_at);

	} else {
		u.set_task(task);
	}
}

void do_bomb(game_display& gui, team& t, bool to_recorder)
{
	if (to_recorder) {
		recorder.add_bomb(t);
	}
	t.set_bomb_turns(0);
	t.refresh_tactic_slots(gui);
}

void join_anim(hero* h, artifical* city, const std::string& message)
{
	const std::vector<team>& teams = *resources::teams;
	if (!message.empty()) {
		if (!city || teams[city->side() - 1].is_human()) {
			sound::play_sound("join.wav");
		}
		game_events::show_hero_message(h, city, message, game_events::INCIDENT_RECOMMENDONESELF);
	}
}

bool troop_can_appoint_noble(const unit& u, play_controller* controller, bool field)
{
	if (tent::tower_mode()) {
		return true;
	}
	if (!field) {
		return true;
	}
	if (controller && controller->has_network_player()) {
		return false;
	} 
	return u.can_formation_attack() && u.formation_attack_enable();
}

bool troop_can_assamble_treasure(const unit& u, play_controller* controller, bool field)
{
	if (tent::tower_mode()) {
		return true;
	}
	if (!field) {
		return true;
	}
	if (controller && controller->has_network_player()) {
		return false;
	} 
	if (u.can_formation_attack() && u.formation_attack_enable()) {
		if (u.master().treasure_ != HEROS_NO_TREASURE) {
			return true;
		}
		if (u.second().valid() && u.second().treasure_ != HEROS_NO_TREASURE) {
			return true;
		}
		if (u.third().valid() && u.third().treasure_ != HEROS_NO_TREASURE) {
			return true;
		}
	}
	return false;
}

std::vector<std::pair<int, unit*> > form_human_pairs(const std::vector<team>& teams, troop_can_xxx fn, play_controller* controller)
{
	const team& rpg_team = teams[rpg::h->side_];
	std::vector<std::pair<int, unit*> > human_pairs;

	const std::vector<artifical*>& holden_cities = rpg_team.holden_cities();
	for (std::vector<artifical*>::const_iterator it = holden_cities.begin(); it != holden_cities.end(); ++ it) {
		artifical* city = *it;

		const std::vector<unit*>& resides = city->reside_troops();
		for (std::vector<unit*>::const_iterator it2 = resides.begin(); it2 != resides.end(); ++ it2) {
			unit* u = *it2;
			if (!u->human()) {
				continue;
			}
			if (fn && !fn(*u, controller, false)) {
				continue;
			}
			human_pairs.push_back(std::make_pair(u->master().number_, u));
			if (u->second().valid()) {
				human_pairs.push_back(std::make_pair(u->second().number_, u));
			}
			if (u->third().valid()) {
				human_pairs.push_back(std::make_pair(u->third().number_, u));
			}
		}

		const std::vector<unit*>& fields = city->field_troops();
		for (std::vector<unit*>::const_iterator it2 = fields.begin(); it2 != fields.end(); ++ it2) {
			unit* u = *it2;
			if (!u->human()) {
				continue;
			}
			if (fn && !fn(*u, controller, true)) {
				continue;
			}
			human_pairs.push_back(std::make_pair(u->master().number_, u));
			if (u->second().valid()) {
				human_pairs.push_back(std::make_pair(u->second().number_, u));
			}
			if (u->third().valid()) {
				human_pairs.push_back(std::make_pair(u->third().number_, u));
			}
		}

		const std::vector<hero*>& freshes = city->fresh_heros();
		const std::vector<hero*>& finishes = city->finish_heros();
		if (city->mayor() == rpg::h || rpg::stratum == hero_stratum_leader) {
			for (std::vector<hero*>::const_iterator it2 = freshes.begin(); it2 != freshes.end(); ++ it2) {
				hero& h = **it2;
				human_pairs.push_back(std::make_pair(h.number_, city));
			}
			for (std::vector<hero*>::const_iterator it2 = finishes.begin(); it2 != finishes.end(); ++ it2) {
				hero& h = **it2;
				human_pairs.push_back(std::make_pair(h.number_, city));
			}
		}
		if (rpg::stratum == hero_stratum_citizen) {
			if (std::find(freshes.begin(), freshes.end(), rpg::h) != freshes.end()) {
				human_pairs.push_back(std::make_pair(rpg::h->number_, city));
			}
			if (std::find(finishes.begin(), finishes.end(), rpg::h) != finishes.end()) {
				human_pairs.push_back(std::make_pair(rpg::h->number_, city));
			}
		}
	}
	return human_pairs;
}

bool feature_contained(int f1, int f2)
{
	if (f1 >= hero_feature_arms_min && f1 < hero_feature_arms_min + 2 * HEROS_MAX_ARMS) {
		if (f2 >= hero_feature_arms_min && f2 < hero_feature_arms_min + 2 * HEROS_MAX_ARMS) {
			return (f1 - hero_feature_arms_min) % HEROS_MAX_ARMS == (f2 - hero_feature_arms_min) % HEROS_MAX_ARMS;
		}
	} else if (f1 >= hero_feature_range_min && f1 < hero_feature_range_min + 2 * HEROS_MAX_RANGE) {
		if (f2 >= hero_feature_range_min && f2 < hero_feature_range_min + 2 * HEROS_MAX_RANGE) {
			return (f1 - hero_feature_range_min) % HEROS_MAX_RANGE == (f2 - hero_feature_range_min) % HEROS_MAX_RANGE;
		}
	} 
	return false;
}

bool is_packee_feature(int f)
{
	return f == hero_feature_arms_min + 4 || f == hero_feature_arms_min + HEROS_MAX_ARMS + 4;
}

std::pair<int, int> feature_match_ut(const std::vector<int>& v, const unit_type& ut)
{
	int match = 0, nomatch = 0;
	for (std::vector<int>::const_iterator it = v.begin(); it != v.end(); ++ it) {
		int f = *it;
		if (is_packee_feature(f)) {
			continue;
		}
		if (f >= hero_feature_arms_min && f < hero_feature_arms_min + 2 * HEROS_MAX_ARMS) {
			int level = (f - hero_feature_arms_min) / HEROS_MAX_ARMS + 1;
			if ((f - hero_feature_arms_min) % HEROS_MAX_ARMS != ut.arms()) {
				nomatch += level;
			} else {
				match += level;
			}
		} else if (f >= hero_feature_range_min && f < hero_feature_range_min + 2 * HEROS_MAX_RANGE) {
			int level = (f - hero_feature_range_min) / HEROS_MAX_RANGE + 1;
			if (!ut.can_range((f - hero_feature_range_min) % HEROS_MAX_RANGE)) {
				nomatch += level;
			} else {
				match += level;
			}
		}
	}
	return std::make_pair(match, nomatch);
}

int calculate_candidate_second_score(const std::vector<const hero*>& existed, const hero& h2, const unit_type& ut)
{
	const int feature_duplicate = -30;
	const int feature_contain = -20;
	const int feature_ut_nomatch = -30;

	int score = 0;
	const complex_feature_map& complex_feature = unit_types.complex_feature();
	std::vector<int> v2;
	if (h2.feature_ != HEROS_NO_FEATURE) {
		if (h2.feature_ < HEROS_BASE_FEATURE_COUNT) {
			v2.push_back(h2.feature_);
		} else {
			v2 = complex_feature.find(h2.feature_)->second;
		}
		std::pair<int, int> match = feature_match_ut(v2, ut);
		if (!match.first && match.second) {
			score += feature_ut_nomatch;
		}
	}

	std::vector<int> v1;
	for (std::vector<const hero*>::const_iterator it = existed.begin(); it != existed.end(); ++ it) {
		const hero& h1 = **it;
		if (h1.feature_ == HEROS_NO_FEATURE) {
			v1.clear();
		} else if (h1.feature_ < HEROS_BASE_FEATURE_COUNT) {
			v1.push_back(h1.feature_);
		} else {
			v1 = complex_feature.find(h1.feature_)->second;
		}
		if (h2.feature_ != HEROS_NO_FEATURE) {
			for (std::vector<int>::const_iterator it2 = v2.begin(); it2 != v2.end(); ++ it2) {
				int f2 = *it2;
				for (std::vector<int>::const_iterator it1 = v1.begin(); it1 != v1.end(); ++ it1) {
					int f1 = *it1;
					if (f1 == f2) {
						score += feature_duplicate;
					} else if (feature_contained(f1, f2)) {
						score += feature_contain;
					}
				}
			}
		}
	}
	return score;
}

void select_expedite_heros(hero_map& heros, size_t max_army_heros, std::vector<hero*>& fresh_heros, std::vector<const hero*>& expedite_heros, const unit_type& ut)
{
	int index;
	const hero& master = *expedite_heros[0];
	while (expedite_heros.size() < max_army_heros && !fresh_heros.empty()) {
		// select one relation hero if have
		do {
			hero* candidate_second = NULL;

			// master is candidate's consort.
			for (index = 0; index < HEROS_MAX_CONSORT; index ++) {
				if (master.consort_[index].hero_ != HEROS_INVALID_NUMBER) {
					candidate_second = &heros[master.consort_[index].hero_];
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
					candidate_second = &heros[master.oath_[index].hero_];
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
					candidate_second = &heros[master.parent_[index].hero_];
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
					candidate_second = &heros[master.intimate_[index].hero_];
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
				int best_score = -10000;
				int random = rand();
				int times = fresh_heros.size() < 5? (int)fresh_heros.size(): 5;
				for (int n = 0; n < times; n ++, random ++) {
					int i = random % fresh_heros.size();
					int score = calculate_candidate_second_score(expedite_heros, *(fresh_heros[i]), ut);

					if (score > best_score) {
						best_score = score;
						index = i;
					}
				}
			} else {
				index = 0;
			}
		} while (false);
		expedite_heros.push_back(fresh_heros[index]);
		fresh_heros.erase(fresh_heros.begin() + index);
	}
}

const unit_type* select_unit_type(const team& current_team, const hero& master, artifical& city, int cost_exponent)
{
	// which unit_type
	const unit_type* ut;
	int gold = current_team.gold();
	int max_recruit_cost = city.max_recruit_cost() * cost_exponent / 100;
	if (!tent::tower_mode() || master.utype_ == HEROS_NO_UTYPE) {
		int max_level = std::min(game_config::max_level, city.max_level() + 1);
		bool leader = (master.official_ == hero_official_leader);
		bool has_female = master.gender_ == hero_gender_female;

		std::vector<std::string> options;
		int adaptability = 0;
		// Find an available unit that can be recruited,
		// matches the desired usage type, and comes in under budget.
		int bonus = 0;
		if (gold > 4 * max_recruit_cost) {
			bonus = rand() % 3;
		} else if (gold > 3 * max_recruit_cost) {
			bonus = rand() % 2;
		}
		if (game_config::default_ai_level + bonus > max_level) {
			bonus = max_level - game_config::default_ai_level;
		}
		std::vector<int> v2;
		if (master.feature_ != HEROS_NO_FEATURE) {
			if (master.feature_ < HEROS_BASE_FEATURE_COUNT) {
				v2.push_back(master.feature_);
			} else {
				const complex_feature_map& complex_feature = unit_types.complex_feature();
				v2 = complex_feature.find(master.feature_)->second;
			}
		}

		const std::vector<const unit_type*>& recruits = city.recruits(game_config::default_ai_level + bonus);
		const std::vector<arms_feature>& features = current_team.features();
		for (std::vector<const unit_type*>::const_iterator itor = recruits.begin(); itor != recruits.end(); ++ itor) {
			const unit_type* ut = *itor;
			const std::string& name = ut->id();

			bonus = 0;
			for (std::vector<arms_feature>::const_iterator it2 = features.begin(); it2 != features.end(); ++ it2) {
				const arms_feature& f = *it2;
				if (ut->arms() == f.arms_) {
					bonus += ftofxp12(1) / 2;
				}
			}
			if (ut->especial() != NO_ESPECIAL) {
				bonus += ftofxp12(1);
			}
			if (int require = ut->require()) {
				if (require == unit_type::REQUIRE_LEADER) {
					if (leader) {
						// don't use ftofxp12(16), it will result to 0.
						bonus += ftofxp12(15);
					} else {
						continue;
					}
				} else if (require == unit_type::REQUIRE_FEMALE) {
					if (has_female) {
						bonus += ftofxp12(3);
					} else {
						continue;
					}
				}
			}
			if (master.feature_ != HEROS_NO_FEATURE) {
				std::pair<int, int> match = feature_match_ut(v2, *ut);
				if (match.first > match.second) {
					bonus += ftofxp12(2);
				}
			}
			
			if (master.arms_[ut->arms()] + bonus > adaptability) {
				options.clear();
				adaptability = master.arms_[ut->arms()] + bonus;
				options.push_back(name);
			} else if (master.arms_[ut->arms()] + bonus == adaptability) {
				options.push_back(name);
			}
		}
		if (options.empty()) {
			return NULL;
		}

		int index = rand() % options.size();
		ut = unit_types.find(options[index]);
	} else {
		ut = unit_types.keytype(master.utype_);
	}

	return ut;
}

config load_campagin_scenario(const std::string& campaign_id, const std::string& scenario_id, const std::string& type)
{
	config campaign_cfg;
	config scenario_cfg;

	if (!scenario_id.empty() && scenario_id != "null") {
		wml_config_from_file(game_config::path + "/xwml/campaigns/" + campaign_id + ".bin", campaign_cfg);
		scenario_cfg = campaign_cfg.find_child(type, "id", scenario_id);
	} else {
		scenario_cfg.clear();
	}
	return scenario_cfg;
}

bool verify_siege_map_data(const config& game_config, const std::string& map_data)
{
	std::vector<map_location> ea_locs;
	ea_locs.push_back(map_location(0, 0));
	ea_locs.push_back(map_location(0, 7));
	ea_locs.push_back(map_location(1, 7));
	ea_locs.push_back(map_location(2, 7));

	gamemap gmap(game_config, map_data);
	if (gmap.w() != game_config::siege_map_w || gmap.h() != game_config::siege_map_h) {
		return false;
	}
	for (std::vector<map_location>::const_iterator it = ea_locs.begin(); it != ea_locs.end(); ++ it) {
		if (!gmap.is_ea(*it)) {
			return false;
		}
	}
	return true;
}

const ttechnology& browse_stratagem(display& disp, int stratagem_tag)
{
	std::vector<std::string> items;
	std::vector<gui2::tval_str> stratagem_map;
	std::stringstream strstr;
	int actived_index = 0;

	for (int n = stratagem_tag::min; n <= stratagem_tag::max; n ++) {
		const ttechnology& s = stratagem_tag::technology(n);
		strstr.str("");
		strstr << s.name();
		stratagem_map.push_back(gui2::tval_str(n, strstr.str()));

		if (stratagem_tag == n) {
			actived_index = n - stratagem_tag::min;
		}
	}

	for (std::vector<gui2::tval_str>::iterator it = stratagem_map.begin(); it != stratagem_map.end(); ++ it) {
		items.push_back(it->str);
	}
	
	gui2::tcombo_box dlg(items, actived_index);
	dlg.show(disp.video());

	return stratagem_tag::technology(stratagem_map[dlg.selected_index()].val);
}

bool has_enemy_in_3range(const unit_map& units, const gamemap& gmap, const team& current_team, const map_location& loc)
{
	// whether turn1's "end" exist enemy or not. 
	map_offset* adjacent_ptr;
	size_t i, size;
	map_location adjacent_loc;
	unit* u;

	// range=1
	size = (sizeof(adjacent_1) / sizeof(map_offset)) >> 1;
	adjacent_ptr = adjacent_1[loc.x & 0x1];
	for (i = 0; i < size; i ++) {
		adjacent_loc.x = loc.x + adjacent_ptr[i].x;
		adjacent_loc.y = loc.y + adjacent_ptr[i].y;
		if (gmap.on_board(adjacent_loc)) {
			u = units.find_unit(adjacent_loc);
			if (u && current_team.is_enemy(u->side())) {
				return true;
			}
		}
	}
	// range=2
	size = (sizeof(adjacent_2) / sizeof(map_offset)) >> 1;
	adjacent_ptr = adjacent_2[loc.x & 0x1];
	for (i = 0; i < size; i ++) {
		adjacent_loc.x = loc.x + adjacent_ptr[i].x;
		adjacent_loc.y = loc.y + adjacent_ptr[i].y;
		if (gmap.on_board(adjacent_loc)) {
			u = units.find_unit(adjacent_loc);
			if (u && current_team.is_enemy(u->side())) {
				return true;
			} 
		}
	}
	// range=3
	size = (sizeof(adjacent_3) / sizeof(map_offset)) >> 1;
	adjacent_ptr = adjacent_3[loc.x & 0x1];
	for (i = 0; i < size; i ++) {
		adjacent_loc.x = loc.x + adjacent_ptr[i].x;
		adjacent_loc.y = loc.y + adjacent_ptr[i].y;
		if (gmap.on_board(adjacent_loc)) {
			u = units.find_unit(adjacent_loc);
			if (u && current_team.is_enemy(u->side())) {
				return true;
			}
		}
	}

	return false;
}

void do_unstage_hero(unit_map& units, hero& h)
{
	if (h.status_ == hero_status_unstage) {
		return;
	}
	if (h.number_ < hero::number_normal_min) {
		return;
	}
	int found = 0;
	unit* u = units.find_unit(h);

	std::vector<hero*> leave_heros;
	std::vector<hero*> captains;

	if (!u->is_city()) {
		if (u->master().number_ == h.number_) {
			leave_heros.push_back(&u->master());
			found ++;
		} else {
			captains.push_back(&u->master());
		}
		if (u->second().valid()) {
			if (u->second().number_ == h.number_) {
				leave_heros.push_back(&u->second());
				found ++;
			} else {
				captains.push_back(&u->second());
			}
		}
		if (u->third().valid()) {
			if (u->third().number_ == h.number_) {
				leave_heros.push_back(&u->third());
				found ++;
			} else {
				captains.push_back(&u->third());
			}
		}

		if (captains.empty()) {
			if (!u->is_resided()) {
				units.erase(u);
			} else {
				artifical& city = *units.city_from_loc(u->get_location());
				city.troop_go_out(*u);
			}
		} else {
			if (leave_heros.size()) {
				u->replace_captains(captains);
			}
		}
	} else {
		artifical& city = *unit_2_artifical(u);
		for (int type = 0; type < 3; type ++) { 
			std::vector<hero*>* list;
			if (type == 0) {
				list = &city.fresh_heros();
			} else if (type == 1) {
				list = &city.finish_heros();
			} else {
				list = &city.wander_heros();
			}
			for (std::vector<hero*>::iterator it = list->begin(); it != list->end();) {
				hero& h2 = **it;
				if (h2.number_ == h.number_) {
					it = list->erase(it);
					found ++;
				} else {
					++ it;
				}
			}
		}
	}
	VALIDATE(found == 1, "found error count!");
	h.to_unstage();
}

std::string format_loc(const unit_map& units, const map_location& loc, int side)
{
	std::stringstream strstr;

	const artifical* to_city = units.city_from_loc(loc);
	if (to_city) {
		strstr << to_city->name();
	} else {
		unit_map::const_iterator base = units.find(loc, false);
		if (base.valid() && base->fort()) {
			if (side == HEROS_INVALID_SIDE || side == base->side()) {
				strstr << base->name();
			} else {
				strstr << tintegrate::generate_format(base->name(), "red");
			}
		}
		strstr << "(" << loc.x + 1 << ", " << loc.y + 1 << ")";
	}
	return strstr.str();
}

void do_hate_relation(hero& h1, hero& h2, bool set)
{
	h1.do_hate_relation(h2, set);
	h2.do_hate_relation(h1, set);

	std::string message;
	int incident;
	utils::string_map symbols;
	symbols["first"] = tintegrate::generate_format(h1.name(), tintegrate::hero_color);
	symbols["second"] = tintegrate::generate_format(h2.name(), tintegrate::hero_color);
	if (set) {
		incident = game_events::INCIDENT_INVALID;
		message = vgettext("wesnoth-lib", "Hate of national perdition! I will be mutually exclusive with $second.", symbols);
		sound::play_sound("spear.ogg");

	} else {
		incident = game_events::INCIDENT_ALLY;
		message = vgettext("I'm happy! I got rid of hate with $second.", symbols);
	}
	if (!message.empty()) {
		game_events::show_hero_message(&h1, NULL, message, incident);
	}
}

surface generate_rpg_surface(hero& h)
{
	surface genus_surf = image::get_image(unit_types.genus(tent::turn_based? tgenus::TURN_BASED: tgenus::HALF_REALTIME).icon());
	surface hero_surf = image::get_image(h.image());
	surface masked_surf = mask_surface(hero_surf, image::get_image("buttons/photo-mask.png"));
	blit_surface(genus_surf, NULL, masked_surf, NULL);
	return masked_surf;
}