/* $Id: unit_abilities.cpp 46186 2010-09-01 21:12:38Z silene $ */
/*
   Copyright (C) 2006 - 2010 by Dominic Bolin <dominic.bolin@exong.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 *  @file
 *  Manage unit-abilities, like heal, cure, and weapon_specials.
 */

#include "foreach.hpp"
#include "gamestatus.hpp"
#include "resources.hpp"
#include "terrain_filter.hpp"
#include "unit.hpp"
#include "team.hpp"
#include "unit_abilities.hpp"



/*
 *
 * [abilities]
 * ...
 *
 * [heals]
 *	value=4
 *	max_value=8
 *	cumulative=no
 *	affect_allies=yes
 *	name= _ "heals"
// *	name_inactive=null
 *	description=  _ "Heals:
Allows the unit to heal adjacent friendly units at the beginning of each turn.

A unit cared for by a healer may heal up to 4 HP per turn.
A poisoned unit cannot be cured of its poison by a healer, and must seek the care of a village or a unit that can cure."
// *	description_inactive=null
 *	icon="misc/..."
// *	icon_inactive=null
 *	[adjacent_description]
 *		name= _ "heals"
// *		name_inactive=null
 *		description=  _ "Heals:
Allows the unit to heal adjacent friendly units at the beginning of each turn.

A unit cared for by a healer may heal up to 4 HP per turn.
A poisoned unit cannot be cured of its poison by a healer, and must seek the care of a village or a unit that can cure."
// *		description_inactive=null
 *		icon="misc/..."
// *		icon_inactive=null
 *	[/adjacent_description]
 *
 *	affect_self=yes
 *	[filter] // SUF
 *		...
 *	[/filter]
 *	[filter_location]
 *		terrain=f
 *		tod=lawful
 *	[/filter_location]
 *	[filter_self] // SUF
 *		...
 *	[/filter_self]
 *	[filter_adjacent] // SUF
 *		adjacent=n,ne,nw
 *		...
 *	[/filter_adjacent]
 *	[filter_adjacent_location]
 *		adjacent=n,ne,nw
 *		...
 *	[/filter_adjacent]
 *	[affect_adjacent]
 *		adjacent=n,ne,nw
 *		[filter] // SUF
 *			...
 *		[/filter]
 *	[/affect_adjacent]
 *	[affect_adjacent]
 *		adjacent=s,se,sw
 *		[filter] // SUF
 *			...
 *		[/filter]
 *	[/affect_adjacent]
 *
 * [/heals]
 *
 * ...
 * [/abilities]
 *
 */


namespace unit_abilities {

static bool affects_side(const config& cfg, const std::vector<team>& teams, size_t side, size_t other_side)
{
	if (side == other_side)
		return cfg["affect_allies"].to_bool(true);
	if (teams[side - 1].is_enemy(other_side))
		return cfg["affect_enemies"].to_bool();
	else
		return cfg["affect_allies"].to_bool();
}

}

bool unit::get_ability_bool(const std::string& ability, const map_location& loc) const
{
	const team& current_team = (*resources::teams)[side_ - 1];

	// first check ability that feature resulted
	if (ability == "skirmisher") {
		if (unit_feature_val(hero_feature_shuttle)) {
			return true;
		}
		unit_map::iterator it = units_.find(loc);
		if (it.valid()) {
			if (it->cancel_zoc()) {
				return true;
			}
		} else if (current_team.ignore_zoc_on_wall_) {
			it = units_.find(loc, false);
			if (it.valid() && it->wall()) {
				return true;
			}
		}
	} else if (ability == "hides") {
		if (hide_turns_) {
			return true;
		}
		if (unit_feature_val(hero_feature_ambush) || unit_feature_val(hero_feature_concealment) || unit_feature_val(hero_feature_submerge)) {
			static config ambush_cfg, concealment_cfg, submerge_cfg;
			std::vector<config*> cfgs;
			if (unit_feature_val(hero_feature_ambush)) {
				if (ambush_cfg.empty()) {
					config& filter_location = ambush_cfg.add_child("filter_location");
					filter_location["terrain"] = "*^Fp,*^Fet*,*^Ft,*^Fpa,*^Fd*,*^Fm*";
				}
				cfgs.push_back(&ambush_cfg);
			}
			if (unit_feature_val(hero_feature_concealment)) {
				if (concealment_cfg.empty()) {
					config& filter_location = concealment_cfg.add_child("filter_location");
					filter_location["terrain"] = "*^V*";
				}
				cfgs.push_back(&concealment_cfg);
			}
			if (unit_feature_val(hero_feature_submerge)) {
				if (submerge_cfg.empty()) {
					config& filter_location = submerge_cfg.add_child("filter_location");
					filter_location["terrain"] = "Wo";
				}
				cfgs.push_back(&submerge_cfg);
			}
			
			for (std::vector<config*>::iterator i = cfgs.begin(); i != cfgs.end(); ++ i) {
				if (matches_filter(vconfig(**i), loc, false)) {
					return true;
				}
			}
		}
	} else if (ability == "indomitable") {
		if (unit_feature_val(hero_featrue_indomitable)) {
			return true;
		}
	}

	// second check ability that unit_type holded
	const std::multimap<const std::string, const config*>* abilities;
	if (packed()) {
		abilities = &packee_unit_type_->abilities_cfg();
	} else {
		abilities = &unit_type_->abilities_cfg();
	}
	for (std::multimap<const std::string, const config*>::const_iterator it = abilities->begin(); it != abilities->end(); ++ it) {
		if (it->first == ability) {
			const config& i = *it->second;
			if (ability_active(ability, i, loc) && ability_affects_self(ability, i, loc)) {
				return true;
			}
		}
	}

	const unit_map& units = *resources::units;
	map_location adjacent[6];
	get_adjacent_tiles(loc,adjacent);
	for(int i = 0; i != 6; ++i) {
		const unit_map::const_iterator it = units.find(adjacent[i]);
		if (it == units.end() || it->incapacitated())
			continue;
		if (it->packed()) {
			abilities = &it->packee_type()->abilities_cfg();
		} else {
			abilities = &it->type()->abilities_cfg();
		}
		if (abilities->empty())
			continue;
		for (std::multimap<const std::string, const config*>::const_iterator it2 = abilities->begin(); it2 != abilities->end(); ++ it2) {
			if (it2->first != ability) {
				continue;
			}
			const config& j = *it2->second;
			if (unit_abilities::affects_side(j, teams_manager::get_teams(), side(), it->side()) &&
			    it->ability_active(ability, j, adjacent[i]) &&
			    ability_affects_adjacent(ability,  j, i, loc))
				return true;
		}
	}


	return false;
}

unit_ability_list unit::get_abilities(const std::string& ability, const map_location& loc) const
{
	unit_ability_list res;

	while (unit_feature_val(hero_feature_healer) || unit_feature_val(hero_feature_surveillance)) {
		if (ability != "heals") {
			break;
		}
		static config heal_cfg, cure_cfg, surveillance_cfg;
		std::vector<config*> cfgs;
		if (unit_feature_val(hero_feature_healer)) {
			if (heal_cfg.empty()) {
				heal_cfg["id"] = "healing";
				heal_cfg["affect_self"] = "yes";
				heal_cfg["value"] = 32;
			}
			cfgs.push_back(&heal_cfg);
		}
		if (unit_feature_val(hero_feature_surveillance)) {
			if (surveillance_cfg.empty()) {
				surveillance_cfg["id"] = "surveillancing";
				surveillance_cfg["affect_self"] = "yes";
				surveillance_cfg["surveillanced"] = "yes";
			}
			cfgs.push_back(&surveillance_cfg);
		}
		for (std::vector<config*>::iterator i = cfgs.begin(); i != cfgs.end(); ++ i) {
			if (ability_affects_self(ability, **i, loc)) {
				res.cfgs.push_back(std::pair<const config *, unit *>(*i, const_cast<unit*>(this)));
			}
		}
		break;
	}

	// self
	const std::multimap<const std::string, const config*>* abilities;
	if (packed()) {
		abilities = &packee_unit_type_->abilities_cfg();
	} else {
		abilities = &unit_type_->abilities_cfg();
	}
	for (std::multimap<const std::string, const config*>::const_iterator it = abilities->begin(); it != abilities->end(); ++ it) {
		if (it->first == ability) {
			const config& i = *it->second;
			if (ability_active(ability, i, loc) && ability_affects_self(ability, i, loc)) {
				res.cfgs.push_back(std::pair<const config *, unit*>(&i, const_cast<unit*>(this)));
			}
		}
	}


	// adjacent
	const unit_map& units = *resources::units;
	map_location adjacent[6];
	get_adjacent_tiles(loc,adjacent);
	for(int i = 0; i != 6; ++i) {
		const unit_map::const_iterator it = units.find(adjacent[i]);
		if (it == units.end() || it->incapacitated())
			continue;
		if (it->packed()) {
			abilities = &it->packee_type()->abilities_cfg();
		} else {
			abilities = &it->type()->abilities_cfg();
		}
		if (abilities->empty())
			continue;
		for (std::multimap<const std::string, const config*>::const_iterator it2 = abilities->begin(); it2 != abilities->end(); ++ it2) {
			if (it2->first != ability) {
				continue;
			}
			const config& j = *it2->second;
			if (unit_abilities::affects_side(j, teams_manager::get_teams(), side(), it->side()) &&
			    it->ability_active(ability, j, adjacent[i]) && ability_affects_adjacent(ability, j, i, loc))
				res.cfgs.push_back(std::pair<const config *, unit*>(&j, &*it));
		}
	}


	return res;
}

std::vector<std::string> unit::ability_tooltips(bool force_active) const
{
	std::vector<std::string> res;

	const std::multimap<const std::string, const config*>* abilities;
	if (packed()) {
		abilities = &packee_unit_type_->abilities_cfg();
	} else {
		abilities = &unit_type_->abilities_cfg();
	}
	if (abilities->empty()) {
		return res;
	}

	for (std::multimap<const std::string, const config*>::const_iterator it = abilities->begin(); it != abilities->end(); ++ it) {
		const config& ab_cfg = *it->second;
		if (force_active || ability_active(it->first, *it->second, loc_)) {
			std::string const &name =
				gender_ == unit_race::MALE || ab_cfg["female_name"].empty()? ab_cfg["name"] : ab_cfg["female_name"];

			if (!name.empty()) {
				res.push_back(name);
				res.push_back(ab_cfg["description"]);
			}
		} else {
			std::string const &name =
				gender_ == unit_race::MALE || ab_cfg["female_name_inactive"].empty() ?
				ab_cfg["name_inactive"] : ab_cfg["female_name_inactive"];

			if (!name.empty()) {
				res.push_back(name);
				res.push_back(ab_cfg["description_inactive"]);
			}
		}
	}
	return res;
}

/*
 *
 * cfg: an ability WML structure
 *
 */
static bool cache_illuminates(int &cache, std::string const &ability)
{
	if (cache < 0)
		cache = (ability == "illuminates");
	return (cache != 0);
}

bool unit::ability_active(const std::string& ability,const config& cfg,const map_location& loc) const
{
	int illuminates = -1;
	assert(resources::units && resources::game_map && resources::teams && resources::tod_manager);

	if (const config &afilter = cfg.child("filter"))
		if (!matches_filter(vconfig(afilter), loc, cache_illuminates(illuminates, ability)))
			return false;

	map_location adjacent[6];
	get_adjacent_tiles(loc,adjacent);
	const unit_map& units = *resources::units;

	foreach (const config &i, cfg.child_range("filter_adjacent"))
	{
		foreach (const std::string &j, utils::split(i["adjacent"]))
		{
			map_location::DIRECTION index =
				map_location::parse_direction(j);
			if (index == map_location::NDIRECTIONS)
				continue;
			unit_map::const_iterator unit = units.find(adjacent[index]);
			if (unit == units.end())
				return false;
			if (!unit->matches_filter(vconfig(i), unit->get_location(),
				cache_illuminates(illuminates, ability)))
				return false;
		}
	}

	foreach (const config &i, cfg.child_range("filter_adjacent_location"))
	{
		foreach (const std::string &j, utils::split(i["adjacent"]))
		{
			map_location::DIRECTION index = map_location::parse_direction(j);
			if (index == map_location::NDIRECTIONS) {
				continue;
			}
			terrain_filter adj_filter(vconfig(i), units);
			adj_filter.flatten(cache_illuminates(illuminates, ability));
			if(!adj_filter.match(adjacent[index])) {
				return false;
			}
		}
	}
	return true;
}
/*
 *
 * cfg: an ability WML structure
 *
 */
bool unit::ability_affects_adjacent(const std::string& ability, const config& cfg,int dir,const map_location& loc) const
{
	int illuminates = -1;

	assert(dir >=0 && dir <= 5);
	static const std::string adjacent_names[6] = {"n","ne","se","s","sw","nw"};
	foreach (const config &i, cfg.child_range("affect_adjacent"))
	{
		std::vector<std::string> dirs = utils::split(i["adjacent"]);
		if (std::find(dirs.begin(),dirs.end(),adjacent_names[dir]) != dirs.end()) {
			if (const config &filter = i.child("filter")) {
				if (matches_filter(vconfig(filter), loc, cache_illuminates(illuminates, ability))) {
					return true;
				}
			} else {
				return true;
			}
		}
	}
	return false;
}
/*
 *
 * cfg: an ability WML structure
 *
 */
bool unit::ability_affects_self(const std::string& ability,const config& cfg,const map_location& loc) const
{
	int illuminates = -1;
	const config &filter = cfg.child("filter_self");
	bool affect_self = cfg["affect_self"].to_bool(true);
	if (!filter || !affect_self) return affect_self;
	return matches_filter(vconfig(filter), loc,cache_illuminates(illuminates, ability));
}

bool unit::has_ability_type(const std::string& ability) const
{
	const std::multimap<const std::string, const config*>* abilities;
	if (packed()) {
		abilities = &packee_unit_type_->abilities_cfg();
	} else {
		abilities = &unit_type_->abilities_cfg();
	}

	for (std::multimap<const std::string, const config*>::const_iterator it = abilities->begin(); it != abilities->end(); ++ it) {
		if (it->first == ability) {
			return true;
		}
	}
	return false;
}


bool unit_ability_list::empty() const
{
	return cfgs.empty();
}

std::pair<int,map_location> unit_ability_list::highest(const std::string& key, int def) const
{
	if (cfgs.empty()) {
		return std::make_pair(def, map_location());
	}
	// The returned location is the best non-cumulative one, if any,
	// the best absolute cumulative one otherwise.
	map_location best_loc;
	bool only_cumulative = true;
	int abs_max = 0;
	int flat = 0;
	int stack = 0;
	typedef std::pair<const config *, unit *> pt;
	foreach (pt const &p, cfgs)
	{
		int value = (*p.first)[key].to_int(def);
		if ((*p.first)["cumulative"].to_bool()) {
			stack += value;
			if (value < 0) value = -value;
			if (only_cumulative && value >= abs_max) {
				abs_max = value;
				best_loc = p.second->get_location();
			}
		} else if (only_cumulative || value > flat) {
			only_cumulative = false;
			flat = value;
			best_loc = p.second->get_location();
		}
	}
	return std::make_pair(flat + stack, best_loc);
}

std::pair<int,map_location> unit_ability_list::lowest(const std::string& key, int def) const
{
	if (cfgs.empty()) {
		return std::make_pair(def, map_location());
	}
	// The returned location is the best non-cumulative one, if any,
	// the best absolute cumulative one otherwise.
	map_location best_loc;
	bool only_cumulative = true;
	int abs_max = 0;
	int flat = 0;
	int stack = 0;
	typedef std::pair<const config *, unit *> pt;
	foreach (pt const &p, cfgs)
	{
		int value = (*p.first)[key].to_int(def);
		if ((*p.first)["cumulative"].to_bool()) {
			stack += value;
			if (value < 0) value = -value;
			if (only_cumulative && value <= abs_max) {
				abs_max = value;
				best_loc = p.second->get_location();
			}
		} else if (only_cumulative || value < flat) {
			only_cumulative = false;
			flat = value;
			best_loc = p.second->get_location();
		}
	}
	return std::make_pair(flat + stack, best_loc);
}

void unit_ability_list::merge(const unit_ability_list& other)
{
	std::vector<std::pair<const config *, unit *> > need_append_cfgs;

	for (std::vector<std::pair<const config *, unit *> >::const_iterator other_cfg = other.cfgs.begin(); other_cfg != other.cfgs.end(); ++ other_cfg) {
		std::vector<std::pair<const config *, unit *> >::const_iterator cfg = cfgs.begin();
		for (; cfg != cfgs.end(); ++ cfg) {
			if (cfg->first == other_cfg->first) {
				break;
			}
		}
		if (cfg != cfgs.end()) {
			continue;
		}
		need_append_cfgs.push_back(std::make_pair<const config *, unit *>(other_cfg->first, other_cfg->second));
	}
	std::copy(need_append_cfgs.begin(), need_append_cfgs.end(), std::back_inserter(cfgs));
}

/*
 *
 * [special]
 * [swarm]
 *	name= _ "swarm"
 *	name_inactive= _ ""
 *	description= _ ""
 *	description_inactive= _ ""
 *	cumulative=no
 *	apply_to=self  #self,opponent,defender,attacker
 *	#active_on=defend  .. offense
 *
 *	attacks_max=4
 *	attacks_min=2
 *
 *	[filter_self] // SUF
 *		...
 *	[/filter_self]
 *	[filter_opponent] // SUF
 *	[filter_attacker] // SUF
 *	[filter_defender] // SUF
 *	[filter_adjacent] // SAUF
 *	[filter_adjacent_location] // SAUF + locs
 * [/swarm]
 * [/special]
 *
 */

namespace {
	bool get_special_children(std::vector<const config*>& result, const config& parent,
	                           const std::string& id, bool just_peeking=false) {
		foreach (const config::any_child &sp, parent.all_children_range())
		{
			if (sp.key == id || sp.cfg["id"] == id) {
				if(just_peeking) {
					return true; // peek succeeded, abort
				} else {
					result.push_back(&sp.cfg);
				}
			}
		}
		return false;
	}
}

bool attack_type::get_special_bool(const std::string& special,bool force) const
{
//	log_scope("get_special_bool");
	if (const config &specials = cfg_.child("specials"))
	{
		std::vector<const config*> list;
		if (get_special_children(list, specials, special, force)) return true;
		for (std::vector<const config*>::iterator i = list.begin(),
		     i_end = list.end(); i != i_end; ++i) {
			if (special_active(**i, true))
				return true;
		}
	}
	if (force || !other_attack_) return false;
	if (const config &specials = other_attack_->cfg_.child("specials"))
	{
		std::vector<const config*> list;
		get_special_children(list, specials, special);
		for (std::vector<const config*>::iterator i = list.begin(),
		     i_end = list.end(); i != i_end; ++i) {
			if (other_attack_->special_active(**i, false))
				return true;
		}
	}
	return false;
}

unit_ability_list attack_type::get_specials(const std::string& special) const
{
//	log_scope("get_specials");
	unit_ability_list res;
	if (const config &specials = cfg_.child("specials"))
	{
		foreach (const config &i, specials.child_range(special)) {
			if (special_active(i, true))
				res.cfgs.push_back(std::pair<const config *, unit *>
					(&i, attacker_ ? aloc_ : dloc_));
		}
	}
	if (!other_attack_) return res;
	if (const config &specials = other_attack_->cfg_.child("specials"))
	{
		foreach (const config &i, specials.child_range(special)) {
			if (other_attack_->special_active(i, false))
				res.cfgs.push_back(std::pair<const config *, unit *>
					(&i, attacker_ ? dloc_ : aloc_));
		}
	}
	return res;
}
std::vector<t_string> attack_type::special_tooltips(bool force) const
{
//	log_scope("special_tooltips");
	std::vector<t_string> res;
	const config &specials = cfg_.child("specials");
	if (!specials) return res;

	foreach (const config::any_child &sp, specials.all_children_range())
	{
		if (force || special_active(sp.cfg, true)) {
			const t_string &name = sp.cfg["name"];
			if (!name.empty()) {
				res.push_back(name);
				res.push_back(sp.cfg["description"]);
			}
		} else {
			t_string const &name = sp.cfg["name_inactive"];
			if (!name.empty()) {
				res.push_back(name);
				res.push_back(sp.cfg["description_inactive"]);
			}
		}
	}
	return res;
}
std::string attack_type::weapon_specials(bool force) const
{
//	log_scope("weapon_specials");
	std::string res;
	const config &specials = cfg_.child("specials");
	if (!specials) return res;

	foreach (const config::any_child &sp, specials.all_children_range())
	{
		char const *s = force || special_active(sp.cfg, true) ?
			"name" : "name_inactive";
		std::string const &name = sp.cfg[s];

		if (!name.empty()) {
			if (!res.empty()) res += ',';
			res += name;
		}
	}

	return res;
}



/*
 *
 * cfg: a weapon special WML structure
 *
 */
bool attack_type::special_active(const config& cfg, bool self) const
{
//	log_scope("special_active");
	assert(unitmap_ != NULL);
	unit* att = aloc_;
	unit* def = dloc_;

	if(self) {
		if(!special_affects_self(cfg)) {
			return false;
		}
	} else {
		if(!special_affects_opponent(cfg)) {
			return false;
		}
	}

	if(attacker_) {
		{
			std::string const &active = cfg["active_on"];
			if (!active.empty() && active != "offense")
				return false;
		}
		if (const config &filter_self = cfg.child("filter_self"))
		{
			if (!att ||
			    !att->matches_filter(vconfig(filter_self), aloc_->get_location()))
				return false;
			if (const config &filter_weapon = filter_self.child("filter_weapon")) {
				if (!matches_filter(filter_weapon, true))
					return false;
			}
		}
		if (const config &filter_opponent = cfg.child("filter_opponent"))
		{
			if (!def ||
			    !def->matches_filter(vconfig(filter_opponent), dloc_->get_location()))
				return false;
			if (const config &filter_weapon = filter_opponent.child("filter_weapon")) {
				if (!other_attack_ ||
				    !other_attack_->matches_filter(filter_weapon, true))
					return false;
			}
		}
	} else {
		{
			std::string const &active = cfg["active_on"];
			if (!active.empty() && active != "defense")
				return false;
		}
		if (const config &filter_self = cfg.child("filter_self"))
		{
			if (!def ||
			    !def->matches_filter(vconfig(filter_self), dloc_->get_location()))
				return false;
			if (const config &filter_weapon = filter_self.child("filter_weapon")) {
				if (!matches_filter(filter_weapon, true))
					return false;
			}
		}
		if (const config &filter_opponent = cfg.child("filter_opponent"))
		{
			if (!att ||
			    !att->matches_filter(vconfig(filter_opponent), aloc_->get_location()))
				return false;
			if (const config &filter_weapon = filter_opponent.child("filter_weapon")) {
				if (!other_attack_ ||
				    !other_attack_->matches_filter(filter_weapon, true))
					return false;
			}
		}
	}
	if (const config &filter_attacker = cfg.child("filter_attacker"))
	{
		if (!att ||
		    !att->matches_filter(vconfig(filter_attacker), aloc_->get_location()))
			return false;
		if (const config &filter_weapon = filter_attacker.child("filter_weapon"))
		{
			if (attacker_) {
				if (!matches_filter(filter_weapon, true))
					return false;
			} else {
				if (!other_attack_ ||
				    !other_attack_->matches_filter(filter_weapon, true))
					return false;
			}
		}
	}
	if (const config &filter_defender = cfg.child("filter_defender"))
	{
		if (!def ||
		    !def->matches_filter(vconfig(filter_defender), dloc_->get_location()))
			return false;
		if (const config &filter_weapon = filter_defender.child("filter_weapon"))
		{
			if (!attacker_) {
				if(!matches_filter(filter_weapon, true))
					return false;
			} else {
				if (!other_attack_ ||
				    !other_attack_->matches_filter(filter_weapon, true))
					return false;
			}
		}
	}
	map_location adjacent[6];
	if(attacker_) {
		get_adjacent_tiles(aloc_->get_location(), adjacent);
	} else {
		get_adjacent_tiles(dloc_->get_location(), adjacent);
	}

	foreach (const config &i, cfg.child_range("filter_adjacent"))
	{
		foreach (const std::string &j, utils::split(i["adjacent"]))
		{
			map_location::DIRECTION index =
				map_location::parse_direction(j);
			if (index == map_location::NDIRECTIONS)
				continue;
			unit_map::const_iterator unit = unitmap_->find(adjacent[index]);
			if (unit == unitmap_->end() ||
			    !unit->matches_filter(vconfig(i), unit->get_location()))
				return false;
		}
	}

	foreach (const config &i, cfg.child_range("filter_adjacent_location"))
	{
		foreach (const std::string &j, utils::split(i["adjacent"]))
		{
			map_location::DIRECTION index =
				map_location::parse_direction(j);
			if (index == map_location::NDIRECTIONS)
				continue;
			terrain_filter adj_filter(vconfig(i), *unitmap_);
			if(!adj_filter.match(adjacent[index])) {
				return false;
			}
		}
	}
	return true;
}
/*
 *
 * cfg: a weapon special WML structure
 *
 */
bool attack_type::special_affects_opponent(const config& cfg) const
{
//	log_scope("special_affects_opponent");
	std::string const &apply_to = cfg["apply_to"];
	if (apply_to.empty())
		return false;
	if (apply_to == "both")
		return true;
	if (apply_to == "opponent")
		return true;
	if (attacker_ && apply_to == "defender")
		return true;
	if (!attacker_ && apply_to == "attacker")
		return true;
	return false;
}
/*
 *
 * cfg: a weapon special WML structure
 *
 */
bool attack_type::special_affects_self(const config& cfg) const
{
//	log_scope("special_affects_self");
	std::string const &apply_to = cfg["apply_to"];
	if (apply_to.empty())
		return true;
	if (apply_to == "both")
		return true;
	if (apply_to == "self")
		return true;
	if (attacker_ && apply_to == "attacker")
		return true;
	if (!attacker_ && apply_to == "defender")
		return true;
	return false;
}
void attack_type::set_specials_context(const unit* aloc, const unit* dloc,
	const unit_map &unitmap, bool attacker, const attack_type *other_attack) const
{
	aloc_ = const_cast<unit*>(aloc);
	dloc_ = const_cast<unit*>(dloc);
	unitmap_ = &unitmap;
	attacker_ = attacker;
	other_attack_ = other_attack;
}

void attack_type::set_specials_context(const unit* loc, const unit* dloc, const unit& /*un*/, bool attacker) const
{
	aloc_ = const_cast<unit*>(loc);
	dloc_ = const_cast<unit*>(dloc);
	unitmap_ = resources::units;
	attacker_ = attacker;
	other_attack_ = NULL;
}




namespace unit_abilities
{

void individual_effect::set(value_modifier t, int val, const config *abil, unit* un)
{
	type = t;
	value = val;
	ability = abil;
	u = un;
}

bool filter_base_matches(const config& cfg, int def)
{
	if (const config &apply_filter = cfg.child("filter_base_value")) {
		config::attribute_value cond_eq = apply_filter["equals"];
		config::attribute_value cond_ne = apply_filter["not_equals"];
		config::attribute_value cond_lt = apply_filter["less_than"];
		config::attribute_value cond_gt = apply_filter["greater_than"];
		config::attribute_value cond_ge = apply_filter["greater_than_equal_to"];
		config::attribute_value cond_le = apply_filter["less_than_equal_to"];
		return  (cond_eq.empty() || def == cond_eq.to_int()) &&
			(cond_ne.empty() || def != cond_ne.to_int()) &&
			(cond_lt.empty() || def <  cond_lt.to_int()) &&
			(cond_gt.empty() || def >  cond_gt.to_int()) &&
			(cond_ge.empty() || def >= cond_ge.to_int()) &&
			(cond_le.empty() || def <= cond_le.to_int());
	}
	return true;
}

effect::effect(const unit_ability_list& list, int def, bool backstab) :
	effect_list_(),
	composite_value_(0)
{

	int value_set = def;
	bool value_is_set = false;
	std::map<std::string,individual_effect> values_add;
	std::map<std::string,individual_effect> values_mul;

	individual_effect set_effect;

	for (std::vector< std::pair<const config *, unit *> >::const_iterator
	     i = list.cfgs.begin(), i_end = list.cfgs.end(); i != i_end; ++i) {
		const config& cfg = (*i->first);
		std::string const &effect_id = cfg[cfg["id"].empty() ? "name" : "id"];

		if (!backstab && cfg["backstab"].to_bool())
			continue;
		if (!filter_base_matches(cfg, def))
			continue;

		if (const config::attribute_value *v = cfg.get("value")) {
			int value = *v;
			bool cumulative = cfg["cumulative"].to_bool();
			if (!value_is_set && !cumulative) {
				value_set = value;
				set_effect.set(SET, value, i->first, i->second);
			} else {
				if (cumulative) value_set = std::max<int>(value_set, def);
				if (value > value_set) {
					value_set = value;
					set_effect.set(SET, value, i->first, i->second);
				}
			}
			value_is_set = true;
		}

		if (const config::attribute_value *v = cfg.get("add")) {
			int add = *v;
			std::map<std::string,individual_effect>::iterator add_effect = values_add.find(effect_id);
			if(add_effect == values_add.end() || add > add_effect->second.value) {
				values_add[effect_id].set(ADD,add,i->first,i->second);
			}
		}
		if (const config::attribute_value *v = cfg.get("multiply")) {
			int multiply = int(v->to_double() * 100);
			std::map<std::string,individual_effect>::iterator mul_effect = values_mul.find(effect_id);
			if(mul_effect == values_mul.end() || multiply > mul_effect->second.value) {
				values_mul[effect_id].set(MUL,multiply,i->first,i->second);
			}
		}
	}

	if(value_is_set && set_effect.type != NOT_USED) {
		effect_list_.push_back(set_effect);
	}

	int multiplier = 1;
	int divisor = 1;
	std::map<std::string,individual_effect>::const_iterator e, e_end;
	for (e = values_mul.begin(), e_end = values_mul.end(); e != e_end; ++e) {
		multiplier *= e->second.value;
		divisor *= 100;
		effect_list_.push_back(e->second);
	}
	int addition = 0;
	for (e = values_add.begin(), e_end = values_add.end(); e != e_end; ++e) {
		addition += e->second.value;
		effect_list_.push_back(e->second);
	}

	composite_value_ = (value_set + addition) * multiplier / divisor;
}

} // end namespace unit_abilities

