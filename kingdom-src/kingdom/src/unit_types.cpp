/* $Id: unit_types.cpp 47388 2010-11-03 07:19:44Z silene $ */
/*
   Copyright (C) 2003 - 2010 by David White <dave@whitevine.net>
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
 *  Handle unit-type specific attributes, animations, advancement.
 */

#include "global.hpp"

#include "unit_types.hpp"

#include "asserts.hpp"
#include "foreach.hpp"
#include "game_config.hpp"
#include "gettext.hpp"
#include "loadscreen.hpp"
#include "map.hpp"
#include "hero.hpp"
#include "wml_exception.hpp"

#include "unit_map.hpp"
#include "unit.hpp"

#include "formula_string_utils.hpp"

static std::string null_str = "";

department::department(int type, const std::string& name, const std::string& image, const std::string& portrait)
	: type_(type)
	, name_(name)
	, image_(image)
	, portrait_(portrait)
	, exploiture_(0)
{
}

tcharacter::tcharacter(int index, const std::string& id)
	: index_(index)
	, id_(id)
{
	name_ = hero::character_str(index_);
	image_ = "misc/character-" + id_ + ".png";
}

int ttactic::min_complex_index = 100;
std::map<int, std::string> ttactic::type_name_map;
std::map<std::string, int> ttactic::range_id_map;

int ttactic::calculate_turn(int force, int intellect)
{
	int sum = (force * 30 + intellect * 70) / 100;
	if (sum < 90) {
		return 2;
	}
	return 3;
}

ttactic::ttactic(int index, int complex_index, const config& cfg)
	: index_(-1)
	, range_(0)
	, action_cfg_()
	, parts_()
	, id_()
	, name_()
	, description_()
	, bg_image_()
	, self_profit_(0)
	, friend_profit_(0)
	, enemy_profit_(0)
{
	if (type_name_map.empty()) {
		type_name_map[RESISTANCE] = "resistance";
		type_name_map[ATTACK] = "attack";
		type_name_map[ENCOURAGE] = "encourage";
		type_name_map[DEMOLISH] = "demolish";
	}
	if (range_id_map.empty()) {
		range_id_map["self"] = SELF;
		range_id_map["friend"] = FRIEND;
		range_id_map["enemy"] = ENEMY;
	}

	id_ = cfg["id"].str();

	point_ = cfg["point"].to_int(3);

	std::vector<std::string> parts = utils::split(cfg["parts"].str());
	if (!parts.empty()) {
		const std::map<int, ttactic>& tactics = unit_types.tactics();
		const std::map<std::string, int>& tactics_id = unit_types.tactics_id();
		for (std::vector<std::string>::const_iterator it = parts.begin(); it != parts.end(); ++ it) {
			std::map<std::string, int>::const_iterator find = tactics_id.find(*it);
			if (find == tactics_id.end()) {
				throw config::error("[tactic] error, " + id_ + " is complex tactic, but cannot find part: " + *it);
			}
			if (find->second >= min_complex_index) {
				throw config::error("[tactic] error, " + id_ + " is complex tactic, " + *it + " is complex also.");
			}
			const ttactic* t = &(tactics.find(find->second)->second);
			if (std::find(parts_.begin(), parts_.end(), t) != parts_.end()) {
				throw config::error("[tactic] error, " + id_ + " is complex tactic, its sub-tactic " + *it + " duplicated.");
			}
			parts_.push_back(t);
		}
		index_ = complex_index;
		complex_ = true;
	} else {
		index_ = index; 
		complex_ = false;
	}

	std::stringstream strstr;
	strstr << HERO_PREFIX_STR_TACTIC << id_;
	name_ = dgettext("wesnoth-hero", strstr.str().c_str());

	strstr.str("");
	strstr << HERO_PREFIX_STR_TACTIC_DESC << id_;
	description_ = dgettext("wesnoth-hero", strstr.str().c_str());

	if (!complex_) {
		std::vector<std::string> range = utils::split(cfg["range"].str());
		for (std::vector<std::string>::const_iterator it = range.begin(); it != range.end(); ++ it) {
			std::map<std::string, int>::const_iterator find = range_id_map.find(*it);
			if (find != range_id_map.end()) {
				range_ |= find->second;
			}
		}
		if (!range_) {
			throw config::error("[tactic] error, " + id_ + " range is null.");
		}
	
		const config& action_cfg = cfg.child("action");
		if (!action_cfg) {
			throw config::error("[tactic] error, " + id_ + " is atom tactic, but no [action].");
		}
		action_cfg_ = action_cfg;
	}

	if (!complex_) {
		int profit = 0; // x 100

		foreach (const config& effect, action_cfg_.child_range("effect")) {
			const std::string apply_to = effect["apply_to"].str();
			int total_value = 0;
			if (apply_to == "resistance") {
				const config& resistance = effect.child("resistance");
				foreach (const config::attribute &istrmap, resistance.attribute_range()) {
					total_value += lexical_cast<int>(istrmap.second);
				}
				profit += total_value / 2;
			} else if (apply_to == "attack") {
				total_value = effect["increase_damage"].to_int();

				profit += total_value * 50;
			} else if (apply_to == "encourage") {
				total_value = effect["increase"].to_int();

				profit += total_value * 50;
			} else if (apply_to == "demolish") {
				total_value = effect["increase"].to_int();

				// same value but lower profit
				profit += total_value * 20;
			}
		}
		if (range_ & SELF) {
			self_profit_ = profit;
		}
		if (range_ & FRIEND) {
			friend_profit_ = profit;
		}
		if (range_ & ENEMY) {
			enemy_profit_ = -1 * profit;
		}
	} else {
		for (std::vector<const ttactic*>::const_iterator it = parts_.begin(); it != parts_.end(); ++ it) {
			const ttactic& t = **it;
			if (t.range() & SELF) {
				range_ |= SELF;
			}
			if (t.range() & FRIEND) {
				range_ |= FRIEND;
			}
			if (t.range() & ENEMY) {
				range_ |= ENEMY;
			}

			self_profit_ += t.self_profit();
			friend_profit_ += t.friend_profit();
			enemy_profit_ += t.enemy_profit();
		}
	}

	if ((range_ & FRIEND) || (range_ & ENEMY)) {
		bg_image_ = "tactic/bg-mess-increase.png";
	} else {
		bg_image_ = "tactic/bg-single-increase.png";
	}
}

std::vector<std::pair<const ttactic*, std::vector<map_location> > > ttactic::touch_locs(const map_location& loc) const
{
	std::vector<std::pair<const ttactic*, std::vector<map_location> > > ret;
	std::vector<const ttactic*> parts;
	if (parts_.empty()) {
		parts.push_back(this);		
	} else {
		parts = parts_;
	}
/*
	for (std::vector<const ttactic*>::const_iterator it = parts.begin(); it != parts.end(); ++ it) {
		const ttactic& t = **it;
		const config& cfg = t.range_cfg();
		ret.push_back(std::make_pair<const ttactic*, std::vector<map_location> >(&t, std::vector<map_location>()));

		if (cfg["self"].to_bool()) {
			ret.back().second.push_back(loc);
		}
		std::vector<std::string> adjacent = utils::split(cfg["adjacent"]);
		for (std::vector<std::string>::const_iterator dir = adjacent.begin(); dir != adjacent.end(); ++ dir) {
			map_location::DIRECTION dir_i = map_location::parse_direction(*dir);
			if (dir_i == map_location::NDIRECTIONS) {
				continue;
			}
			map_location loc1 = loc.get_direction(dir_i);
			ret.back().second.push_back(loc1);
		}
	}
*/
	return ret;
}

#if defined(_KINGDOM_EXE) || !defined(_WIN32)
#include "team.hpp"
#include "resources.hpp"

std::map<int, std::vector<unit*> > ttactic::touch_units(unit_map& units, unit& u) const
{
	std::map<int, std::vector<unit*> > ret;
	unit_map::iterator u_it;
	const team& tactican_team = (*resources::teams)[u.side() - 1];
	std::vector<unit*> touched;

	int part = 0;
	for (std::vector<const ttactic*>::const_iterator it = parts_.begin(); it != parts_.end(); ++ it, part ++) {
		const ttactic& t = **it;
		touched.clear();
		
		if (t.range() & SELF) {
			touched.push_back(&u);
		}
		if ((t.range() & ttactic::FRIEND) || (t.range() & ttactic::ENEMY)) {
			const map_location* tiles = u.adjacent_;
			size_t adjance_size = u.adjacent_size_;

			for (int step = 0; step < 2; step ++) {
				if (step == 1) {
					tiles = u.adjacent_2_;
					adjance_size = u.adjacent_size_2_;
				}
			
				for (size_t adj = 0; adj != adjance_size; adj ++) {
					const map_location& loc = tiles[adj];
					u_it = units.find(loc);
					if (!u_it.valid()) {
						continue;
					}
					if ((t.range() & ttactic::FRIEND) && tactican_team.is_enemy(u_it->side())) {
						continue;
					}
					if ((t.range() & ttactic::ENEMY) && !tactican_team.is_enemy(u_it->side())) {
						continue;
					}						
					if (std::find(touched.begin(), touched.end(), &*u_it) != touched.end()) {
						continue;
					}
					touched.push_back(&*u_it);
				}
			}
		}
		ret[part] = touched;
	}
	return ret;
}
#endif

attack_type::attack_type(const config& cfg) :
	aloc_(),
	dloc_(),
	attacker_(false),
	unitmap_(NULL),
	other_attack_(NULL),
	cfg_(cfg),
	id_(cfg["name"]),
	type_(cfg["type"]),
	icon_(cfg["icon"]),
	range_(cfg["range"]),
	specials_(cfg["specials"]),
	damage_(cfg["damage"]),
	num_attacks_(cfg["number"]),
	attack_weight_(cfg["attack_weight"].to_double(1.0)),
	defense_weight_(cfg["defense_weight"].to_double(1.0)),
	accuracy_(cfg["accuracy"]),
	parry_(cfg["parry"])

{
	description_ = egettext(id_.c_str());

	if (icon_.empty()) {
		if (id_ != "") {
			icon_ = "attacks/" + id_ + ".png";
		} else {
			icon_ = "attacks/blank-attack.png";
		}
	}

	// complete specials
	if (specials_.empty()) {
		return;
	}
	config& specials_cfg = cfg_.add_child("specials");
	const specials_map& units_specials = unit_types.specials();
	const std::vector<std::string> specials = utils::split(specials_);
	for (std::vector<std::string>::const_iterator it = specials.begin(); it != specials.end(); ++ it) {
		specials_map::const_iterator result = units_specials.find(*it);
		if (result == units_specials.end()) {
			throw config::error("[attack]'s specials error, cannot find " + *it);
		}
		foreach (const config::any_child &c, result->second.all_children_range()) {
			specials_cfg.add_child(c.key, c.cfg);
		}
	}
}

attack_type::~attack_type()
{
}

std::string attack_type::accuracy_parry_description() const
{
	if(accuracy_ == 0 && parry_ == 0) {
		return "";
	}

	std::ostringstream s;
	s << utils::signed_percent(accuracy_);

	if(parry_ != 0) {
		s << "/" << utils::signed_percent(parry_);
	}

	return s.str();
}

bool attack_type::matches_filter(const config& cfg,bool self) const
{
	const std::vector<std::string>& filter_range = utils::split(cfg["range"]);
	const std::string& filter_damage = cfg["damage"];
	const std::vector<std::string> filter_name = utils::split(cfg["name"]);
	const std::vector<std::string> filter_type = utils::split(cfg["type"]);
	const std::string filter_special = cfg["special"];

	if(filter_range.empty() == false && std::find(filter_range.begin(),filter_range.end(),range()) == filter_range.end())
			return false;

	if(filter_damage.empty() == false && !in_ranges(damage(), utils::parse_ranges(filter_damage))) {
		return false;
	}

	if(filter_name.empty() == false && std::find(filter_name.begin(),filter_name.end(),id()) == filter_name.end())
		return false;

	if(filter_type.empty() == false && std::find(filter_type.begin(),filter_type.end(),type()) == filter_type.end())
		return false;

#if defined(_KINGDOM_EXE) || !defined(_WIN32)
	if(!self && filter_special.empty() == false && !get_special_bool(filter_special,true))
		return false;
#endif
	return true;
}

bool attack_type::apply_modification(const config& cfg,std::string* description)
{
	if(!matches_filter(cfg,0))
		return false;

	const std::string& set_name = cfg["set_name"];
	const t_string& set_desc = cfg["set_description"];
	const std::string& set_type = cfg["set_type"];
	const std::string& del_specials = cfg["remove_specials"];
	const config &set_specials = cfg.child("set_specials");
	const std::string& increase_damage = cfg["increase_damage"];
	const std::string& increase_attacks = cfg["increase_attacks"];
	const std::string& set_attack_weight = cfg["attack_weight"];
	const std::string& set_defense_weight = cfg["defense_weight"];
	const std::string& increase_accuracy = cfg["increase_accuracy"];
	const std::string& increase_parry = cfg["increase_parry"];

	std::stringstream desc;

	if(set_name.empty() == false) {
		id_ = set_name;
		cfg_["name"] = id_;
	}

	if(set_desc.empty() == false) {
		description_ = set_desc;
		cfg_["description"] = description_;
	}

	if(set_type.empty() == false) {
		type_ = set_type;
		cfg_["type"] = type_;
	}

	if(del_specials.empty() == false) {
		const std::vector<std::string>& dsl = utils::split(del_specials);
		if (config &specials = cfg_.child("specials"))
		{
			config new_specials;
			foreach (const config::any_child &vp, specials.all_children_range()) {
				std::vector<std::string>::const_iterator found_id =
					std::find(dsl.begin(), dsl.end(), vp.cfg["id"].str());
				if (found_id == dsl.end()) {
					new_specials.add_child(vp.key, vp.cfg);
				}
			}
			cfg_.clear_children("specials");
			cfg_.add_child("specials",new_specials);
		}
	}

	if (set_specials) {
		const std::string &mode = set_specials["mode"];
		if (mode != "append") {
			cfg_.clear_children("specials");
		}
		config &new_specials = cfg_.child_or_add("specials");
		foreach (const config::any_child &value, set_specials.all_children_range()) {
			new_specials.add_child(value.key, value.cfg);
		}
	}

	if(increase_damage.empty() == false) {
		damage_ = utils::apply_modifier(damage_, increase_damage, 1);
		cfg_["damage"] = damage_;

		if(description != NULL) {
			int inc_damage = lexical_cast<int>(increase_damage);
			desc << utils::signed_value(inc_damage) << " "
				 << _n("damage","damage", inc_damage);
		}
	}

	if(increase_attacks.empty() == false) {
		num_attacks_ = utils::apply_modifier(num_attacks_, increase_attacks, 1);
		cfg_["number"] = num_attacks_;

		if(description != NULL) {
			int inc_attacks = lexical_cast<int>(increase_attacks);
			desc << utils::signed_value(inc_attacks) << " "
				 << _n("strike", "strikes", inc_attacks);
		}
	}

	if(increase_accuracy.empty() == false) {
		accuracy_ = utils::apply_modifier(accuracy_, increase_accuracy, 1);
		cfg_["accuracy"] = accuracy_;

		if(description != NULL) {
			int inc_acc = lexical_cast<int>(increase_accuracy);
			// Help xgettext with a directive to recognise the string as a non C printf-like string
			// xgettext:no-c-format
			desc << utils::signed_value(inc_acc) << _("% accuracy");
		}
	}

	if(increase_parry.empty() == false) {
		parry_ = utils::apply_modifier(parry_, increase_parry, 1);
		cfg_["parry"] = parry_;

		if(description != NULL) {
			int inc_parry = lexical_cast<int>(increase_parry);
			// xgettext:no-c-format
			desc << utils::signed_value(inc_parry) << _("% parry");
		}
	}

	if(set_attack_weight.empty() == false) {
		attack_weight_ = lexical_cast_default<double>(set_attack_weight,1.0);
		cfg_["attack_weight"] = attack_weight_;
	}

	if(set_defense_weight.empty() == false) {
		defense_weight_ = lexical_cast_default<double>(set_defense_weight,1.0);
		cfg_["defense_weight"] = defense_weight_;
	}

	if(description != NULL) {
		*description = desc.str();
	}

	return true;
}

// Same as above, except only update the descriptions
bool attack_type::describe_modification(const config& cfg,std::string* description)
{
	if(!matches_filter(cfg,0))
		return false;

	const std::string& increase_damage = cfg["increase_damage"];
	const std::string& increase_attacks = cfg["increase_attacks"];

	std::stringstream desc;

	if(increase_damage.empty() == false) {
		if(description != NULL) {
			int inc_damage = lexical_cast<int>(increase_damage);
			desc << utils::signed_value(inc_damage) << " "
				 << _n("damage","damage", inc_damage);
		}
	}

	if(increase_attacks.empty() == false) {
		if(description != NULL) {
			int inc_attacks = lexical_cast<int>(increase_attacks);
			desc << utils::signed_value(inc_attacks) << " "
				 << _n("strike", "strikes", inc_attacks);
		}
	}

	if(description != NULL) {
		*description = desc.str();
	}

	return true;
}

// @increase_damage: integer value
void attack_type::apply_modification_damage(int increase_damage)
{
	damage_ = damage_ + increase_damage;
	cfg_["damage"] = lexical_cast_default<std::string>(damage_);
}

// @increase_percent: percent value
void attack_type::apply_modification_damage2(double increase_percent)
{
	int value = div100rounded((int)(increase_percent * 100 * damage_));

	damage_ = damage_ + value;
	cfg_["damage"] = lexical_cast_default<std::string>(damage_);
}

void attack_type::set_damage(int damage)
{
	damage_ = damage;
	cfg_["damage"] = lexical_cast_default<std::string>(damage_);
}

unit_movement_type::unit_movement_type(const config& cfg, const unit_movement_type* parent) :
	moveCosts_(),
	defenseMods_(),
	parent_(parent),
	is_flying_(false),
	name_(),
	cfg_()
{
	//the unit_type give its whole cfg, we don't need all that.
	//so we filter to keep only keys related to movement_type
	//FIXME: This helps but it's still not clean, both cfg use a "name" key

	name_ = cfg["name"].str();
	is_flying_ = cfg["flies"].to_bool();

	if (const config &movement_costs = cfg.child("movement_costs"))
		cfg_.add_child("movement_costs", movement_costs);

	if (const config &defense = cfg.child("defense"))
		cfg_.add_child("defense", defense);

	if (const config &resistance = cfg.child("resistance"))
		cfg_.add_child("resistance", resistance);
}

unit_movement_type::unit_movement_type(): moveCosts_(), defenseMods_(), parent_(NULL), is_flying_(false), name_(), cfg_()
{}

std::string unit_movement_type::name() const
{
	if (name_.empty() && parent_)
		return parent_->name();
	else
		return name_;
}

int unit_movement_type::resistance_against(const attack_type& attack) const
{
	bool result_found = false;
	int res = 100;

	if (const config &resistance = cfg_.child("resistance"))
	{
		if (const::config::attribute_value *val = resistance.get(attack.type())) {
			res = *val;
			result_found = true;
		}
	}

	if(!result_found && parent_ != NULL) {
		res = parent_->resistance_against(attack);
	}

	return res;
}

utils::string_map unit_movement_type::damage_table() const
{
	utils::string_map res;
	if(parent_ != NULL)
		res = parent_->damage_table();

	if (const config &resistance = cfg_.child("resistance"))
	{
		foreach (const config::attribute &i, resistance.attribute_range()) {
			res[i.first] = i.second;
		}
	}

	return res;
}

bool unit_movement_type::is_flying() const
{
	if (!is_flying_ && parent_)
		return parent_->is_flying();

	return is_flying_;
}

int movement_cost_internal(std::map<t_translation::t_terrain, int>& move_costs,
		const config& cfg, const unit_movement_type* parent,
		const gamemap& map, t_translation::t_terrain terrain, int recurse_count)
{
	const int impassable = unit_movement_type::UNREACHABLE;

	const std::map<t_translation::t_terrain, int>::const_iterator i = move_costs.find(terrain);

	if (i != move_costs.end()) return i->second;

	// If this is an alias, then select the best of all underlying terrains.
	const t_translation::t_list& underlying = map.underlying_mvt_terrain(terrain);
	assert(!underlying.empty());

	if (underlying.size() != 1 || underlying.front() != terrain) {
		bool revert = (underlying.front() == t_translation::MINUS ? true : false);
		if (recurse_count >= 100) {
			move_costs.insert(std::pair<t_translation::t_terrain, int>(terrain, impassable));
			return impassable;
		}

		int ret_value = revert ? 0 : impassable;
		for (t_translation::t_list::const_iterator i = underlying.begin();
				i != underlying.end(); ++i)
		{
			if (*i == t_translation::PLUS) {
				revert = false;
				continue;
			} else if (*i == t_translation::MINUS) {
				revert = true;
				continue;
			}
			const int value = movement_cost_internal(move_costs, cfg,
					parent, map, *i, recurse_count + 1);

			if (value < ret_value && !revert) {
				ret_value = value;
			} else if (value > ret_value && revert) {
				ret_value = value;
			}
		}

		move_costs.insert(std::pair<t_translation::t_terrain, int>(terrain, ret_value));
		return ret_value;
	}

	bool result_found = false;
	int res = impassable;

	if (const config& movement_costs = cfg.child("movement_costs"))	{
		if (underlying.size() != 1) {
			move_costs.insert(std::pair<t_translation::t_terrain, int>(terrain, impassable));
			return impassable;
		}

		const std::string& id = map.get_terrain_info(underlying.front()).id();
		if (const config::attribute_value *val = movement_costs.get(id)) {
			res = *val;
			result_found = true;
		}
	}

	if (!result_found && parent != NULL) {
		res = parent->movement_cost(map, terrain);
	}

	if (res <= 0) {
		res = 1;
	}

	move_costs.insert(std::pair<t_translation::t_terrain, int>(terrain, res));
	return res;
}

const defense_range &defense_range_modifier_internal(defense_cache &defense_mods,
		const config& cfg, const unit_movement_type* parent,
		const gamemap& map, t_translation::t_terrain terrain, int recurse_count)
{
	defense_range dummy = { 0, 100 };
	std::pair<defense_cache::iterator, bool> ib =
		defense_mods.insert(defense_cache::value_type(terrain, dummy));
	if (!ib.second) return ib.first->second;

	defense_range &res = ib.first->second;

	// If this is an alias, then select the best of all underlying terrains.
	const t_translation::t_list& underlying = map.underlying_def_terrain(terrain);
	assert(!underlying.empty());

	if (underlying.size() != 1 || underlying.front() != terrain) {
		bool revert = underlying.front() == t_translation::MINUS;
		if (recurse_count >= 90) {

		}
		if (recurse_count >= 100) {
			return res;
		}

		if (revert) {
			res.max_ = 0;
			res.min_ = 100;
		}

		for (t_translation::t_list::const_iterator i = underlying.begin();
				i != underlying.end(); ++i) {

			if (*i == t_translation::PLUS) {
				revert = false;
				continue;
			} else if (*i == t_translation::MINUS) {
				revert = true;
				continue;
			}
			const defense_range &inh = defense_range_modifier_internal
				(defense_mods, cfg, parent, map, *i, recurse_count + 1);

			if (!revert) {
				if (inh.max_ < res.max_) res.max_ = inh.max_;
				if (inh.min_ > res.min_) res.min_ = inh.min_;
			} else {
				if (inh.max_ > res.max_) res.max_ = inh.max_;
				if (inh.min_ < res.min_) res.min_ = inh.min_;
			}
		}

		goto check;
	}

	if (const config& defense = cfg.child("defense"))
	{
		const std::string& id = map.get_terrain_info(underlying.front()).id();
		if (const config::attribute_value *val = defense.get(id)) {
			int def = *val;
			if (def >= 0) res.max_ = def;
			else res.max_ = res.min_ = -def;
			goto check;
		}
	}

	if (parent) {
		return parent->defense_range_modifier(map, terrain);
	}

	check:

	if (res.min_ < 0) {
		res.min_ = 0;
	}

	return res;
}

int defense_modifier_internal(defense_cache &defense_mods,
	const config &cfg, const unit_movement_type *parent,
	const gamemap &map, t_translation::t_terrain terrain, int recurse_count)
{
	const defense_range &def = defense_range_modifier_internal(defense_mods,
		cfg, parent, map, terrain, recurse_count);
	return (std::max)(def.max_, def.min_);
}

static const unit_race& dummy_race(){
	static unit_race ur;
	return ur;
}


#ifdef _MSC_VER
#pragma warning(push)
//silence "elements of array will be default initialized" warnings
#pragma warning(disable:4351)
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif

std::string unit_type::image(const std::string& race, const std::string& id, bool terrain)
{
	std::stringstream strstr;

	strstr << "units/";
	strstr << race << "/";
	strstr << id << ".png";
	return strstr.str();
}

std::string unit_type::hit(const std::string& race, const std::string& id, bool terrain)
{
	if (terrain) return "";

	std::stringstream strstr;

	strstr << "units/";
	strstr << race << "/";
	strstr << id << "-defend-hit.png";

	return strstr.str();
}

std::string unit_type::miss(const std::string& race, const std::string& id, bool terrain)
{
	if (terrain) return "";

	std::stringstream strstr;

	strstr << "units/";
	strstr << race << "/";
	strstr << id << "-defend-miss.png";

	return strstr.str();
}

std::string unit_type::leading(const std::string& race, const std::string& id, bool terrain)
{
	if (terrain) return "";

	std::stringstream strstr;

	strstr << "units/";
	strstr << race << "/";
	strstr << id << "-leading.png";

	return strstr.str();
}

std::string unit_type::idle(const std::string& race, const std::string& id, bool terrain, int number)
{
	if (terrain) return "";

	std::stringstream strstr;

	strstr << "units/";
	strstr << race << "/";
	strstr << id << "-" << "idle" << "-";
	strstr << number << ".png";

	return strstr.str();
}

bool unit_type::has_resistance_anim(const std::set<std::string>& abilities)
{
	for (std::set<std::string>::const_iterator it = abilities.begin(); it != abilities.end(); ++ it) {
		if (it->find("firm") != std::string::npos) {
			return true;
		}
	}
	return false;
}

bool unit_type::has_leading_anim(const std::set<std::string>& abilities)
{
	for (std::set<std::string>::const_iterator it = abilities.begin(); it != abilities.end(); ++ it) {
		if (it->find("leadership") != std::string::npos) {
			return true;
		}
		if (it->find("encourage") != std::string::npos) {
			return true;
		}
	}
	return false;
}

bool unit_type::has_healing_anim(const std::set<std::string>& abilities)
{
	for (std::set<std::string>::const_iterator it = abilities.begin(); it != abilities.end(); ++ it) {
		if (it->find("heals") != std::string::npos) {
			return true;
		}
		if (it->find("curing") != std::string::npos) {
			return true;
		}
	}
	return false;
}

std::string unit_type::attack_image(const std::string& race, const std::string& id, bool terrain, int range, int number)
{
	if (terrain) return "";

	std::stringstream strstr;

	strstr << "units/";
	strstr << race << "/";
	strstr << id << "-" << unit_types.range_ids()[range] << "-";
	strstr << "attack-" << number << ".png";
	
	return strstr.str();
}

static void replace_anim_cfg_inernal(const config& src, config& dst, const utils::string_map& symbols)
{
	foreach (const config::any_child& c, src.all_children_range()) {
		config& adding = dst.add_child(c.key);
		foreach (const config::attribute &i, c.cfg.attribute_range()) {
			adding[i.first] = utils::interpolate_variables_into_string(i.second, &symbols);
		}
		replace_anim_cfg_inernal(c.cfg, adding, symbols);
	}
}

static void replace_anim_cfg(const std::string& src, config& dst, const utils::string_map& symbols)
{
	const config& anim_cfg = unit_types.utype_anims().find(src)->second;
	// replace
	foreach (const config::attribute &i, anim_cfg.attribute_range()) {
		dst[i.first] = utils::interpolate_variables_into_string(i.second, &symbols);
	}
	replace_anim_cfg_inernal(anim_cfg, dst, symbols);
}

void unit_type::defend_anim(const std::string& race, const std::string& id, bool terrain, config& cfg)
{
	cfg.clear();

	utils::string_map symbols;
	symbols["base_png"] = image(race, id, terrain);
	symbols["hit_png"] = hit(race, id, terrain);
	symbols["miss_png"] = miss(race, id, terrain);
	symbols["hit_sound"] = "human-hit-1.ogg,human-hit-2.ogg,human-hit-3.ogg,human-hit-4.ogg,human-hit-5.ogg";

	replace_anim_cfg("defend", cfg, symbols);
}

void unit_type::resistance_anim(const std::string& race, const std::string& id, bool terrain, config& cfg)
{
	cfg.clear();

	utils::string_map symbols;
	symbols["leading_png"] = leading(race, id, terrain);

	replace_anim_cfg("resistance", cfg, symbols);
}

void unit_type::leading_anim(const std::string& race, const std::string& id, bool terrain, config& cfg)
{
	cfg.clear();

	utils::string_map symbols;
	symbols["leading_png"] = leading(race, id, terrain);

	replace_anim_cfg("leading", cfg, symbols);
}

void unit_type::healing_anim(const std::string& race, const std::string& id, bool terrain, config& cfg)
{
	cfg.clear();

	utils::string_map symbols;
	symbols["ranged_attack_1_png"] = attack_image(race, id, terrain, 1, 1);
	symbols["ranged_attack_2_png"] = attack_image(race, id, terrain, 1, 2);
	symbols["ranged_attack_3_png"] = attack_image(race, id, terrain, 1, 3);
	symbols["ranged_attack_4_png"] = attack_image(race, id, terrain, 1, 4);

	replace_anim_cfg("healing", cfg, symbols);
}

void unit_type::idle_anim(const std::string& race, const std::string& id, bool terrain, config& cfg)
{
	cfg.clear();

	utils::string_map symbols;
	symbols["idle_1_png"] = idle(race, id, terrain, 1);
	symbols["idle_2_png"] = idle(race, id, terrain, 2);
	symbols["idle_3_png"] = idle(race, id, terrain, 3);
	symbols["idle_4_png"] = idle(race, id, terrain, 4);

	replace_anim_cfg("idle", cfg, symbols);
}

void unit_type::attack_anim_melee(const std::string& aid, const std::string& aicon, bool troop, const std::string& race, const std::string& id, bool terrain, config& cfg)
{
	std::string hit_sound = "sword-1.ogg"; // {SOUND_LIST:SWORD_SWISH}
	std::string miss_sound = "miss-1.ogg,miss-2.ogg,miss-3.ogg"; // {SOUND_LIST:MISS}
	if (aicon.find("staff") != std::string::npos) {
		hit_sound = "staff.wav";
	}

	cfg.clear();

	utils::string_map symbols;
	const std::vector<std::string>& range_ids = unit_types.range_ids();
	if (std::find(range_ids.begin(), range_ids.end(), aid) == range_ids.end()) {
		symbols["attack_id"] = aid;
		symbols["range"] = "";
	} else {
		symbols["attack_id"] = "";
		symbols["range"] = aid;
	}
	symbols["hit_sound"] = hit_sound;
	symbols["miss_sound"] = miss_sound;
	symbols["melee_attack_1_png"] = attack_image(race, id, terrain, 0, 1);
	symbols["melee_attack_2_png"] = attack_image(race, id, terrain, 0, 2);
	symbols["melee_attack_3_png"] = attack_image(race, id, terrain, 0, 3);
	symbols["melee_attack_4_png"] = attack_image(race, id, terrain, 0, 4);

	replace_anim_cfg("melee_attack", cfg, symbols);

	if (!troop) {
		cfg["offset"] = 0;
	}
}

void unit_type::attack_anim_ranged(const std::string& aid, const std::string& aicon, const std::string& race, const std::string& id, bool terrain, config& cfg)
{
	std::string image = "projectiles/missile-n.png";
	std::string image_diagonal = "projectiles/missile-ne.png";
	std::string hit_sound = "bow.ogg";
	std::string miss_sound = "bow-miss.ogg";
	if (aicon.find("sling") != std::string::npos) {
		image = "projectiles/stone-large.png";
		image_diagonal = "";
	}

	cfg.clear();

	utils::string_map symbols;
	const std::vector<std::string>& range_ids = unit_types.range_ids();
	if (std::find(range_ids.begin(), range_ids.end(), aid) == range_ids.end()) {
		symbols["attack_id"] = aid;
		symbols["range"] = "";
	} else {
		symbols["attack_id"] = "";
		symbols["range"] = aid;
	}
	symbols["image_png"] = image;
	symbols["image_diagonal_png"] = image_diagonal;
	symbols["hit_sound"] = hit_sound;
	symbols["miss_sound"] = miss_sound;
	symbols["ranged_attack_1_png"] = attack_image(race, id, terrain, 1, 1);
	symbols["ranged_attack_2_png"] = attack_image(race, id, terrain, 1, 2);
	symbols["ranged_attack_3_png"] = attack_image(race, id, terrain, 1, 3);
	symbols["ranged_attack_4_png"] = attack_image(race, id, terrain, 1, 4);

	replace_anim_cfg("ranged_attack", cfg, symbols);
}

void unit_type::attack_anim_ranged_magic_missile(const std::string& aid, const std::string& race, const std::string& id, bool terrain, config& cfg)
{
	cfg.clear();

	utils::string_map symbols;
	const std::vector<std::string>& range_ids = unit_types.range_ids();
	if (std::find(range_ids.begin(), range_ids.end(), aid) == range_ids.end()) {
		symbols["attack_id"] = aid;
		symbols["range"] = "";
	} else {
		symbols["attack_id"] = "";
		symbols["range"] = aid;
	}
	symbols["ranged_attack_1_png"] = attack_image(race, id, terrain, 1, 1);
	symbols["ranged_attack_2_png"] = attack_image(race, id, terrain, 1, 2);
	symbols["ranged_attack_3_png"] = attack_image(race, id, terrain, 1, 3);
	symbols["ranged_attack_4_png"] = attack_image(race, id, terrain, 1, 4);

	replace_anim_cfg("magic_missile_attack", cfg, symbols);
}

void unit_type::attack_anim_ranged_lightbeam(const std::string& aid, const std::string& race, const std::string& id, bool terrain, config& cfg)
{
	cfg.clear();

	utils::string_map symbols;
	const std::vector<std::string>& range_ids = unit_types.range_ids();
	if (std::find(range_ids.begin(), range_ids.end(), aid) == range_ids.end()) {
		symbols["attack_id"] = aid;
		symbols["range"] = "";
	} else {
		symbols["attack_id"] = "";
		symbols["range"] = aid;
	}
	symbols["ranged_attack_1_png"] = attack_image(race, id, terrain, 1, 1);
	symbols["ranged_attack_2_png"] = attack_image(race, id, terrain, 1, 2);
	symbols["ranged_attack_3_png"] = attack_image(race, id, terrain, 1, 3);
	symbols["ranged_attack_4_png"] = attack_image(race, id, terrain, 1, 4);

	replace_anim_cfg("lightbeam_attack", cfg, symbols);
}

void unit_type::attack_anim_ranged_fireball(const std::string& aid, const std::string& race, const std::string& id, bool terrain, config& cfg)
{
	cfg.clear();

	utils::string_map symbols;
	const std::vector<std::string>& range_ids = unit_types.range_ids();
	if (std::find(range_ids.begin(), range_ids.end(), aid) == range_ids.end()) {
		symbols["attack_id"] = aid;
		symbols["range"] = "";
	} else {
		symbols["attack_id"] = "";
		symbols["range"] = aid;
	}
	symbols["ranged_attack_1_png"] = attack_image(race, id, terrain, 1, 1);
	symbols["ranged_attack_2_png"] = attack_image(race, id, terrain, 1, 2);
	symbols["ranged_attack_3_png"] = attack_image(race, id, terrain, 1, 3);
	symbols["ranged_attack_4_png"] = attack_image(race, id, terrain, 1, 4);

	replace_anim_cfg("fireball_attack", cfg, symbols);
}

void unit_type::attack_anim_ranged_iceball(const std::string& aid, const std::string& race, const std::string& id, bool terrain, config& cfg)
{
	cfg.clear();

	utils::string_map symbols;
	const std::vector<std::string>& range_ids = unit_types.range_ids();
	if (std::find(range_ids.begin(), range_ids.end(), aid) == range_ids.end()) {
		symbols["attack_id"] = aid;
		symbols["range"] = "";
	} else {
		symbols["attack_id"] = "";
		symbols["range"] = aid;
	}
	symbols["ranged_attack_1_png"] = attack_image(race, id, terrain, 1, 1);
	symbols["ranged_attack_2_png"] = attack_image(race, id, terrain, 1, 2);
	symbols["ranged_attack_3_png"] = attack_image(race, id, terrain, 1, 3);
	symbols["ranged_attack_4_png"] = attack_image(race, id, terrain, 1, 4);

	replace_anim_cfg("iceball_attack", cfg, symbols);
}

void unit_type::attack_anim_ranged_lightning(const std::string& aid, const std::string& race, const std::string& id, bool terrain, config& cfg)
{
	cfg.clear();

	utils::string_map symbols;
	const std::vector<std::string>& range_ids = unit_types.range_ids();
	if (std::find(range_ids.begin(), range_ids.end(), aid) == range_ids.end()) {
		symbols["attack_id"] = aid;
		symbols["range"] = "";
	} else {
		symbols["attack_id"] = "";
		symbols["range"] = aid;
	}
	symbols["ranged_attack_1_png"] = attack_image(race, id, terrain, 1, 1);
	symbols["ranged_attack_2_png"] = attack_image(race, id, terrain, 1, 2);
	symbols["ranged_attack_3_png"] = attack_image(race, id, terrain, 1, 3);
	symbols["ranged_attack_4_png"] = attack_image(race, id, terrain, 1, 4);

	replace_anim_cfg("lightning_attack", cfg, symbols);
}

unit_type::unit_type(const unit_type& o) :
	cfg_(o.cfg_),
	id_(o.id_),
	type_name_(o.type_name_),
	description_(o.description_),
	hitpoints_(o.hitpoints_),
	level_(o.level_),
	movement_(o.movement_),
	max_attacks_(o.max_attacks_),
	cost_(o.cost_),
	gold_income_(o.gold_income_),
	heal_(o.heal_),
	turn_experience_(o.turn_experience_),
	halo_(o.halo_),
	undead_variation_(o.undead_variation_),
	image_(o.image_),
	flag_rgb_(o.flag_rgb_),
	die_sound_(o.die_sound_),
	num_traits_(o.num_traits_),
	variations_(o.variations_),
	race_(o.race_),
	alpha_(o.alpha_),
	abilities_cfg_(o.abilities_cfg_),
	abilities_(o.abilities_),
	ability_tooltips_(o.ability_tooltips_),
	zoc_(o.zoc_),
	cancel_zoc_(o.cancel_zoc_),
	leader_(o.leader_),
	hide_help_(o.hide_help_),
	advances_to_(o.advances_to_),
	advancement_(o.advancement_),
	experience_needed_(o.experience_needed_),
	in_advancefrom_(o.in_advancefrom_),
	alignment_(o.alignment_),
	movementType_(o.movementType_),
	movementType_id_(o.movementType_id_),
	possibleTraits_(o.possibleTraits_),
	genders_(o.genders_),
#if defined(_KINGDOM_EXE) || !defined(_WIN32)
	animations_(o.animations_),
#endif
	build_status_(o.build_status_),
	match_(o.match_),
	terrain_(o.terrain_),
	can_recruit_(o.can_recruit_),
	can_reside_(o.can_reside_),
	base_(o.base_),
	wall_(o.wall_),
	land_wall_(o.land_wall_),
	walk_wall_(o.walk_wall_),
	arms_(o.arms_),
	character_(o.character_),
	master_(o.master_),
	guard_(o.guard_),
	packer_(o.packer_),
	attacks_(o.attacks_)
{
	for (variations_map::const_iterator i = o.gender_types_.begin(); i != o.gender_types_.end(); ++ i) {
		gender_types_[i->first] = new unit_type(*i->second);
	}

	for (variations_map::const_iterator i = o.variations_.begin(); i != o.variations_.end(); ++ i) {
		variations_[i->first] = new unit_type(*i->second);
	}
}


unit_type::unit_type(config &cfg) :
	cfg_(),
	id_(cfg["id"]),
	type_name_(),
	description_(),
	hitpoints_(0),
	level_(0),
	movement_(0),
	max_attacks_(0),
	cost_(0),
	gold_income_(0),
	heal_(0),
	turn_experience_(0),
	halo_(),
	undead_variation_(),
	image_(),
	flag_rgb_(),
	die_sound_(),
	num_traits_(0),
	gender_types_(),
	variations_(),
	race_(&dummy_race()),
	alpha_(),
	abilities_cfg_(),
	abilities_(),
	ability_tooltips_(),
	zoc_(false),
	cancel_zoc_(false),
	leader_(false),
	hide_help_(false),
	advances_to_(),
	advancement_(),
	experience_needed_(0),
	in_advancefrom_(false),
	alignment_(),
	movementType_(),
	movementType_id_(),
	possibleTraits_(),
	genders_(),
#if defined(_KINGDOM_EXE) || !defined(_WIN32)
	animations_(),
#endif
	build_status_(NOT_BUILT),
	match_(),
	terrain_(t_translation::NONE_TERRAIN),
	can_recruit_(false),
	can_reside_(false),
	base_(false),
	wall_(false),
	land_wall_(true),
	walk_wall_(false),
	arms_(0),
	character_(-1),
	master_(HEROS_INVALID_NUMBER),
	guard_(NO_GUARD),
	packer_(false)
{
	foreach (const config::any_child& c, cfg.all_children_range()) {
		if (c.key == "attack") continue;
		cfg_.add_child(c.key, c.cfg);
	}

	foreach (config &att, cfg.child_range("attack")) {
		attacks_.push_back(attack_type(att));
		if (attacks_.size() == 3) {
			break;
		}
	}

	// AI need this flag, may be before build_all status. duraton ai's interior
	master_ = cfg["master"].to_int(HEROS_INVALID_NUMBER);
	wall_ = cfg["wall"].to_bool();
	walk_wall_ = cfg["walk_wall"].to_bool();
	gold_income_ = cfg["gold_income"].to_int();
}

unit_type::~unit_type()
{
	for (variations_map::iterator i = gender_types_.begin(); i != gender_types_.end(); ++i) {
		delete i->second;
	}
	for (variations_map::iterator i = variations_.begin(); i != variations_.end(); ++i) {
		delete i->second;
	}
}

void unit_type::build_full(const config& cfg, const movement_type_map &mv_types,
	const race_map &races)
{
	if (build_status_ == NOT_BUILT || build_status_ == CREATED)
		build_help_index(cfg, mv_types, races);

	movementType_ = unit_movement_type(cfg);
	alpha_ = ftofxp(1.0);

	const traits_map& units_traits = unit_types.traits();
	for (traits_map::const_iterator it = units_traits.begin(); it != units_traits.end(); ++ it) {
		if (it->second["special"].to_bool()) {
			continue;
		}
		possibleTraits_.push_back(&it->second);
	}
/*		
	foreach (const config &var_cfg, cfg.child_range("variation"))
	{
		if (var_cfg["inherit"].to_bool()) {
			config nvar_cfg(cfg);
			nvar_cfg.merge_with(var_cfg);
			nvar_cfg.clear_children("variation");
			var_cfg.swap(nvar_cfg);
		}
		unit_type *ut = new unit_type(var_cfg);
		ut->build_full(var_cfg, mv_types, races);
		variations_.insert(std::make_pair(var_cfg["variation_name"], ut));
	}
*/
	for (variations_map::const_iterator it = gender_types_.begin(); it != gender_types_.end(); ++ it) {
		it->second->build_full(cfg, mv_types, races);
	}

	const std::string& align = cfg["alignment"];
	if(align == "lawful")
		alignment_ = LAWFUL;
	else if(align == "chaotic")
		alignment_ = CHAOTIC;
	else if(align == "neutral")
		alignment_ = NEUTRAL;
	else if(align == "liminal")
		alignment_ = LIMINAL;
	else {
		throw config::error("Invalid alignment found for " + id() + ": '" + align);
		alignment_ = NEUTRAL;
	}
/*
	// temporarily only support common traits
	if (race_ != &dummy_race())
	{
		if (!race_->uses_global_traits()) {
			possibleTraits_.clear();
		}
		if (cfg["ignore_race_traits"].to_bool()) {
			possibleTraits_.clear();
		} else {
			foreach (const config &t, race_->additional_traits())
			{
				if (alignment_ != NEUTRAL || t["id"] != "fearless")
					possibleTraits_.add_child("trait", t);
			}
		}
	}
*/
	zoc_ = cfg["zoc"].to_bool(level_ > 0);
	cancel_zoc_ = cfg["cancel_zoc"].to_bool();
	leader_ = cfg["leader"].to_bool();

	const std::string& alpha_blend = cfg["alpha"];
	if(alpha_blend.empty() == false) {
		alpha_ = ftofxp(atof(alpha_blend.c_str()));
	}

	const std::string& move_type = cfg["movement_type"];

	const movement_type_map::const_iterator it = mv_types.find(move_type);

	if(it != mv_types.end()) {
	    movementType_.set_parent(&(it->second));
	} else{
		throw config::error("no parent found for movement_type " + move_type);
	}

	flag_rgb_ = cfg["flag_rgb"].str();
	game_config::add_color_info(cfg);

	die_sound_ = cfg["die_sound"].str();

	match_ = cfg["match"].str();
	terrain_ = t_translation::read_terrain_code(cfg["terrain"].str());
	can_recruit_ = cfg["can_recruit"].to_bool();
	// if can_recruit, it must can_reside.
	if (can_recruit_) {
		can_reside_ = true;
	} else {
		can_reside_ = cfg["can_reside"].to_bool();
	}
	base_ = cfg["base"].to_bool();
	
	if (!cfg["arms"].blank()) {
		arms_ = unit_types.arms_from_id(cfg["arms"].str());
		if (arms_ < 0 || arms_ >= HEROS_MAX_ARMS) {
			throw config::error(id_ + "'s arms is invalid: " + cfg["arms"].str());
		}
	}
	if (!cfg["character"].blank()) {
		character_ = unit_types.character_from_id(cfg["character"].str());
		if (character_ < 0) {
			throw config::error(id_ + "'s character is invalid: " + cfg["character"].str());
		}
	}
	
	land_wall_ = cfg["land_wall"].to_bool(true);
	guard_ = cfg["guard"].to_int(NO_GUARD);

	// Deprecation messages, only seen when unit is parsed for the first time.

	build_status_ = FULL;
}

void unit_type::fill_abilities_cfg(const std::string& abilities)
{
	if (abilities.empty()) {
		return;
	}

	// config& abilities_cfg = to.add_child("abilities");
	const abilities_map& units_abilities = unit_types.abilities();
	const std::vector<std::string> v = utils::split(abilities);

	for (std::vector<std::string>::const_iterator it = v.begin(); it != v.end(); ++ it) {
		abilities_map::const_iterator result = units_abilities.find(*it);
		if (result == units_abilities.end()) {
			throw config::error("[unit_type]'s abilities error, cannot find " + *it);
		}
		foreach (const config::any_child &c, result->second.all_children_range()) {
			// abilities_cfg.add_child(c.key, c.cfg);
			abilities_cfg_.insert(std::make_pair<const std::string, const config*>(c.key, &c.cfg));
		}
	}
}

void unit_type::build_help_index(const config& cfg, const movement_type_map &mv_types,
	const race_map &races)
{
	if (build_status_ == NOT_BUILT)
		build_created(cfg, mv_types, races);

	type_name_ = cfg["name"];
	description_ = cfg["description"];
	hitpoints_ = cfg["hitpoints"].to_int(1);
	level_ = cfg["level"];
	movement_ = cfg["movement"].to_int(1);
	max_attacks_ = cfg["attacks"].to_int(1);
	cost_ = cfg["cost"].to_int(1);
	halo_ = cfg["halo"].str();
	undead_variation_ = cfg["undead_variation"].str();
	image_ = cfg["image"].str();
	heal_ = cfg["heal"].to_int();
	turn_experience_ = cfg["turn_experience"].to_int();
	movementType_id_ = cfg["movement_type"].str();

	for (variations_map::const_iterator it = gender_types_.begin(); it != gender_types_.end(); ++ it) {
		it->second->build_help_index(cfg, mv_types, races);
	}

	const race_map::const_iterator race_it = races.find(cfg["race"]);
	if(race_it != races.end()) {
		race_ = &race_it->second;
	} else {
		race_ = &dummy_race();
	}

	// if num_traits is not defined, we use the num_traits from race
	num_traits_ = cfg["num_traits"].to_int(race_->num_traits());

	const std::vector<std::string> genders = utils::split(cfg["gender"]);
	for(std::vector<std::string>::const_iterator g = genders.begin(); g != genders.end(); ++g) {
		genders_.push_back(string_gender(*g));
	}
	if(genders_.empty()) {
		genders_.push_back(unit_race::MALE);
	}

	// complete [abilities]
	fill_abilities_cfg(cfg["abilities"].str());

	for (std::multimap<const std::string, const config*>::const_iterator it = abilities_cfg_.begin(); it != abilities_cfg_.end(); ++ it) {
		const config& ab_cfg = *(it->second);
		const std::string &name = ab_cfg["name"];
		if (!name.empty()) {
			abilities_.push_back(name);
			ability_tooltips_.push_back(ab_cfg["description"]);
		}
	}

	hide_help_= cfg["hide_help"].to_bool();

	build_status_ = BS_HELP_INDEX;
}

void unit_type::build_created(const config& cfg, const movement_type_map &mv_types,
	const race_map &races)
{
	gender_types_.clear();

	packer_ = cfg["packer"].to_bool();
/*
	if (config &male_cfg = cfg.child("male"))
	{
		if (male_cfg["inherit"].to_bool(true)) {
			config m_cfg(cfg);
			m_cfg.merge_with(male_cfg);
			male_cfg.swap(m_cfg);
		}
		male_cfg.clear_children("male");
		male_cfg.clear_children("female");
		gender_types_["male"] = new unit_type(male_cfg);
	}

	if (config &female_cfg = cfg.child("female"))
	{
		if (female_cfg["inherit"].to_bool(true)) {
			config f_cfg(cfg);
			f_cfg.merge_with(female_cfg);
			female_cfg.swap(f_cfg);
		}
		female_cfg.clear_children("male");
		female_cfg.clear_children("female");
		gender_types_["female"] = new unit_type(female_cfg);
	}
*/
	for (variations_map::const_iterator it = gender_types_.begin(); it != gender_types_.end(); ++ it) {
		it->second->build_created(cfg, mv_types, races);
	}

    const std::string& advances_to_val = cfg["advances_to"];
	if (advances_to_val != "null" && advances_to_val != "") {
        advances_to_ = utils::split(advances_to_val);
	}
	for (std::vector<std::string>::const_iterator it = advances_to_.begin(); it != advances_to_.end(); ++ it) {
		const unit_type* ut = unit_types.find(*it);
		if (!ut) {
			throw config::error("unit_type " + id_ + " advances_to error, cannot find " + *it + " in unit_types.");
		}
	}

    advancement_ = cfg["advancement"].str();
	// Only support a kind AMLA to a unit_type.
	if (!advancement_.empty()) {
		modifications_map::const_iterator it = unit_types.modifications().find(advancement_);
		if (it == unit_types.modifications().end()) {
			throw config::error("[unit_type]'s advancement error, cannot find " + advancement_ + " in [units]'s modifications.");
		}
	}

	experience_needed_ = cfg["experience"].to_int(500);

	build_status_ = CREATED;
}

const unit_type& unit_type::get_gender_unit_type(unit_race::GENDER gender) const
{
	std::string gender_str = "male";
	if (gender == unit_race::FEMALE) {
		gender_str = "female";
	}

	const variations_map::const_iterator i = gender_types_.find(gender_str);
	if (i != gender_types_.end()) {
		return *i->second;
	} else {
		return *this;
	}
}

const unit_type& unit_type::get_variation(const std::string& name) const
{
	const variations_map::const_iterator i = variations_.find(name);
	if (i != variations_.end()) {
		return *i->second;
	} else {
		return *this;
	}
}

const t_string unit_type::unit_description() const
{
	if(description_.empty()) {
		return (_("No description available."));
	} else {
		return description_;
	}
}

bool unit_type::use_terrain_image() const
{
	return (terrain_ != t_translation::NONE_TERRAIN)? true: false;
}

#if defined(_KINGDOM_EXE) || !defined(_WIN32)
const std::vector<unit_animation>& unit_type::animations() const 
{
	if (!animations_.empty()) return animations_;

	std::set<std::string> abilities;
	for (std::multimap<const std::string, const config*>::const_iterator it = abilities_cfg_.begin(); it != abilities_cfg_.end(); ++ it) {
		const config& ab_cfg = *(it->second);
		const std::string& id = ab_cfg["id"];
		if (!id.empty()) {
			abilities.insert(id);
		}
	}

	config utype_cfg;

	if (!use_terrain_image()) {
		config& cfg = utype_cfg.add_child("defend");
		unit_type::defend_anim(race_->id(), id_, use_terrain_image(), cfg);
	}

	if (has_resistance_anim(abilities)) {
		config& cfg = utype_cfg.add_child("resistance_anim");
		unit_type::resistance_anim(race_->id(), id_, use_terrain_image(), cfg);
	}
	if (has_leading_anim(abilities)) {
		config& cfg = utype_cfg.add_child("leading_anim");
		unit_type::leading_anim(race_->id(), id_, use_terrain_image(), cfg);
	}
	if (has_healing_anim(abilities)) {
		config& cfg = utype_cfg.add_child("healing_anim");
		unit_type::healing_anim(race_->id(), id_, use_terrain_image(), cfg);
	}

	config& cfg = utype_cfg.add_child("idle_anim");
	unit_type::idle_anim(race_->id(), id_, use_terrain_image(), cfg);

	if (!packer_) {
		for (std::vector<attack_type>::const_iterator it = attacks_.begin(); it != attacks_.end(); ++ it) {
			config& cfg = utype_cfg.add_child("attack_anim");
			if (it->range() == "melee") {
				unit_type::attack_anim_melee(it->id(), it->icon(), master_ == HEROS_INVALID_NUMBER && !can_recruit_, race_->id(), id_, use_terrain_image(), cfg);

			} else if (it->icon().find("magic-missile") != std::string::npos) {
				unit_type::attack_anim_ranged_magic_missile(it->id(), race_->id(), id_, use_terrain_image(), cfg);

			} else if (it->icon().find("lightbeam") != std::string::npos) {
				unit_type::attack_anim_ranged_lightbeam(it->id(), race_->id(), id_, use_terrain_image(), cfg);

			} else if (it->icon().find("fireball") != std::string::npos) {
				unit_type::attack_anim_ranged_fireball(it->id(), race_->id(), id_, use_terrain_image(), cfg);

			} else if (it->icon().find("iceball") != std::string::npos) {
				unit_type::attack_anim_ranged_iceball(it->id(), race_->id(), id_, use_terrain_image(), cfg);

			} else if (it->icon().find("lightning") != std::string::npos) {
				unit_type::attack_anim_ranged_lightning(it->id(), race_->id(), id_, use_terrain_image(), cfg);

			} else {
				unit_type::attack_anim_ranged(it->id(), it->icon(), race_->id(), id_, use_terrain_image(), cfg);
			}
		}
	} else {
		const std::vector<std::string>& range_ids = unit_types.range_ids();

		config* cfg = &utype_cfg.add_child("attack_anim");
		unit_type::attack_anim_melee(range_ids[0], "staff-magic.png", true, race_->id(), id_, use_terrain_image(), *cfg);

		cfg = &utype_cfg.add_child("attack_anim");
		unit_type::attack_anim_ranged_magic_missile(range_ids[1], race_->id(), id_, use_terrain_image(), *cfg);

		cfg = &utype_cfg.add_child("attack_anim");
		unit_type::attack_anim_ranged(range_ids[2], "sling.png", race_->id(), id_, use_terrain_image(), *cfg);
	}

	const std::string& default_image = (terrain_ == t_translation::NONE_TERRAIN)? image_: "";
	utype_cfg["die_sound"] = die_sound_;
	unit_animation::fill_initial_animations(default_image, animations_, utype_cfg);

	return animations_;
}
#endif

std::vector<attack_type> unit_type::attacks() const
{
	return attacks_;
}


namespace {
	int experience_modifier = 100;
}

unit_type::experience_accelerator::experience_accelerator(int modifier) : old_value_(experience_modifier)
{
	experience_modifier = modifier;
}

unit_type::experience_accelerator::~experience_accelerator()
{
	experience_modifier = old_value_;
}

int unit_type::experience_accelerator::get_acceleration()
{
	return experience_modifier;
}

int unit_type::experience_needed(bool with_acceleration) const
{
	if(with_acceleration) {
		int exp = (experience_needed_ * experience_modifier + 50) /100;
		if(exp < 1) exp = 1;
		return exp;
	}
	return experience_needed_;
}

const char* unit_type::alignment_description(unit_type::ALIGNMENT align, unit_race::GENDER gender)
{
	static const char* aligns[] = { N_("lawful"), N_("neutral"), N_("chaotic"), N_("liminal") };
	static const char* aligns_female[] = { N_("female^lawful"), N_("female^neutral"), N_("female^chaotic"), N_("female^liminal") };
	const char** tlist = (gender == unit_race::MALE ? aligns : aligns_female);

	return (sgettext(tlist[align]));
}

const char* unit_type::alignment_id(unit_type::ALIGNMENT align)
{
	static const char* aligns[] = { "lawful", "neutral", "chaotic", "liminal" };
	return (aligns[align]);
}

bool unit_type::hide_help() const {
	return hide_help_ || unit_types.hide_help(id_, race_->id());
}

static void advancement_tree_internal(const std::string& id, std::set<std::string>& tree)
{
	const unit_type *ut = unit_types.find(id);
	if (!ut)
		return;

	std::vector<std::string> vstr = ut->advances_to();
	foreach(const std::string& adv, vstr) {
		if (tree.insert(adv).second) {
			// insertion succeed, expand the new type
			advancement_tree_internal(adv, tree);
		}
	}
}

std::set<std::string> unit_type::advancement_tree() const
{
	std::set<std::string> tree;
	advancement_tree_internal(id_, tree);
	return tree;
}

std::vector<std::string> unit_type::advances_to(int character) const
{
	std::vector<std::string> vstr;
	for (std::vector<std::string>::const_iterator it = advances_to_.begin(); it != advances_to_.end(); ++ it) {
		int adv = unit_types.find(*it)->character();
		if (adv == -1 || adv == character) {
			vstr.push_back(*it);
		}
	}
	return vstr;
}

const std::vector<std::string> unit_type::advances_from() const
{
	std::vector<std::string> adv_from;
	foreach (const unit_type_data::unit_type_map::value_type &ut, unit_types.types())
	{
		std::vector<std::string> vstr = ut.second.advances_to();
		foreach(const std::string& adv, vstr) {
			if (adv == id_)
				adv_from.push_back(ut.second.id());
		}
	}
	return adv_from;
}

unit_type_data::unit_type_data() :
	types_(),
	movement_types_(),
	races_(),
	traits_(),
	modifications_(),
	complex_feature_(),
	treasures_(),
	abilities_(),
	specials_(),
	tactics_(),
	tactics_id_(),
	arms_ids_(),
	range_ids_(),
	characters_(),
	utype_anims_(),
	can_recruit_(),
	navigation_types_(),
	wall_type_(NULL),
	hide_help_all_(false),
	hide_help_type_(),
	hide_help_race_(),
	build_status_(unit_type::NOT_BUILT)
{
}

void unit_type_data::set_config(config &cfg)
{
    clear();

	foreach (const config &mt, cfg.child_range("movetype"))
	{
		const unit_movement_type move_type(mt);
		movement_types_.insert(
			std::pair<std::string,unit_movement_type>(move_type.name(), move_type));
		loadscreen::increment_progress();
	}

	foreach (const config &r, cfg.child_range("race"))
	{
		const unit_race race(r);
		races_.insert(std::pair<std::string,unit_race>(race.id(),race));
		loadscreen::increment_progress();
	}

	foreach (const config &modification, cfg.child_range("modifications"))
	{
		foreach (const config::any_child &c, modification.all_children_range()) {
			if (c.cfg["id"].empty()) {
				throw config::error("[modification] error, no id attribute");
			}
			if (modifications_.find(c.cfg["id"]) != modifications_.end()) {
				throw config::error("[modification] error, duplicate id " + c.cfg["id"].str());
			}
			modifications_.insert(std::pair<std::string, const config>(c.cfg["id"].str(), c.cfg));
			break;
		}
		loadscreen::increment_progress();
	}
	
	foreach (const config &trait, cfg.child_range("traits"))
	{
		foreach (const config::any_child &c, trait.all_children_range()) {
			if (c.cfg["id"].empty()) {
				throw config::error("[trait] error, no id attribute");
			}
			if (traits_.find(c.cfg["id"]) != traits_.end()) {
				throw config::error("[trait] error, duplicate id " + c.cfg["id"].str());
			}
			traits_.insert(std::pair<std::string, const config>(c.cfg["id"].str(), c.cfg));
			break;
		}
		loadscreen::increment_progress();
	}

	foreach (const config &af, cfg.child_range("complexfeature"))
	{
		if (af["id"].empty()) {
			throw config::error("armsfeature error, no id attribute");
		}
		int id = af["id"].to_int();
		if (id < HEROS_BASE_FEATURE_COUNT || id >= HEROS_MAX_FEATURE) {
			throw config::error("armsfeature error, id=" + af["id"].str() + ", invalid id");
		}
		if (af["features"].empty()) {
			throw config::error("armsfeature error, id=" + af["id"].str() + ", no features attribute");
		}
		const std::vector<std::string> features = utils::split(af["features"].str());
		std::vector<int> v;
		for (std::vector<std::string>::const_iterator itor = features.begin(); itor != features.end(); ++ itor) {
			int feature = lexical_cast_default<int>(*itor, HEROS_BASE_FEATURE_COUNT);
			if (feature < 0 || feature >= HEROS_BASE_FEATURE_COUNT) {
				throw config::error("armsfeature error, id=" + af["id"].str() + ", invalid features");
			}
			v.push_back(feature);
		}
		complex_feature_.insert(std::pair<int, std::vector<int> >(id, v));
		loadscreen::increment_progress();
	}

	foreach (const config &af, cfg.child_range("treasure"))
	{
		foreach (const config::attribute &i, af.attribute_range()) {
			int id = lexical_cast_default<int>(i.first, -1);
			if (id == -1) {
				throw config::error("treasure error, invalid treasure number.");
			}
			if (treasures_.find(id) != treasures_.end()) {
				throw config::error("treasure error, number=" + i.first + ", duplicate number.");
			}
			if (id >= HEROS_MAX_TREASURE) {
				throw config::error("treasure error, number=" + i.first + ", to lardge number.");
			}
			if (i.second.empty()) {
				throw config::error("treasure error, number=" + i.first + ", no feature attribute");
			}
			const std::vector<std::string> features = utils::split(i.second.str());
			std::vector<int> v;
			for (std::vector<std::string>::const_iterator itor = features.begin(); itor != features.end(); ++ itor) {
				int feature = lexical_cast_default<int>(*itor);
				if (feature < 0 || feature >= HEROS_MAX_FEATURE) {
					throw config::error("treasure error, number=" + i.first + ", invalid feature");
				}
				v.push_back(feature);
			}
			treasures_.insert(std::pair<int, std::vector<int> >(id, v));
		}
		loadscreen::increment_progress();
	}

	foreach (const config &ability, cfg.child_range("abilities"))
	{
		bool first = true;
		foreach (const config::any_child &c, ability.all_children_range()) {
			if (first) {
				if (c.cfg["id"].empty()) {
					throw config::error("[abilities] error, first must has id attribute");
				}
				if (abilities_.find(c.cfg["id"]) != abilities_.end()) {
					throw config::error("[abilities] error, duplicate id " + c.cfg["id"].str());
				}
				if (c.cfg["name"].empty()) {
					throw config::error("[abilities] error, no name attribute");
				}
				abilities_.insert(std::pair<std::string, const config>(c.cfg["id"].str(), ability));
				first = false;
			} else {
				if (!c.cfg["id"].empty()) {
					throw config::error("[abilities] error, second or more cannot has id attribute in [" + c.key + "]");
				}
				if (!c.cfg["name"].empty()) {
					throw config::error("[abilities] error, second or more cannot has name attribute in [" + c.key + "]");
				}
			}
		}
		loadscreen::increment_progress();
	}

	foreach (const config &anim, cfg.child_range("utype_anim"))
	{
		const std::string id = anim["id"].str();
		if (id.empty()) {
			throw config::error("[utype_anim] error, utype_anim must has id attribute");
		}
		if (utype_anims_.find(id) != utype_anims_.end()) {
			throw config::error("[utype_anim] error, duplicate id: " + id);
		}
		const config& sub = anim.child("anim");
		if (!sub) {
			throw config::error("[utype_anim] error, utype_anim must has [anim] tag");
		}
		utype_anims_[id] = sub;

		loadscreen::increment_progress();
	}

	foreach (const config &sp, cfg.child_range("specials"))
	{
		foreach (const config::any_child &c, sp.all_children_range()) {
			if (c.cfg["id"].empty()) {
				throw config::error("[specials] error, no id attribute");
			}
			if (specials_.find(c.cfg["id"]) != specials_.end()) {
				throw config::error("[specials] error, duplicate id " + c.cfg["id"].str());
			}
			specials_.insert(std::pair<std::string, const config>(c.cfg["id"].str(), sp));
			break;
		}
		loadscreen::increment_progress();
	}

	int index = 0, complex_index = ttactic::min_complex_index;
	foreach (const config &tactic, cfg.child_range("tactic"))
	{
		const std::string id = tactic["id"].str();
		if (id.empty()) {
			throw config::error("[tactic] error, no id attribute");
		}
		if (tactics_id_.find(id) != tactics_id_.end()) {
			throw config::error("[tactic] error, duplicate id " + id);
		}
		
		ttactic t(index, complex_index, tactic);
		std::pair<std::map<int, ttactic>::iterator, bool> ins = tactics_.insert(std::make_pair<int, ttactic>(t.index(), t));
		tactics_id_[id] = t.index();

		if (!t.complex()) {
			ins.first->second.set_atom_part();
			index ++;
		} else {
			complex_index ++;
		}
		
		loadscreen::increment_progress();
	}

	if (const config& id_cfg = cfg.child("identifier")) {
		// identifier of arms
		if (id_cfg["arms"].empty()) {
			throw config::error("[identifier] error, no arms attribute");
		}
		std::vector<std::string> ids = utils::split(id_cfg["arms"]);
		size_t size = ids.size();
		for (size_t i = 0; i < size; i ++) {
			if (i >= HEROS_MAX_ARMS) {
				break;
			}
			arms_ids_.push_back(ids[i]);
		}

		// identifier of range
		if (id_cfg["range"].empty()) {
			throw config::error("[identifier] error, no range attribute");
		}
		ids = utils::split(id_cfg["range"]);
		if (ids.size() != 3) {
			throw config::error("[identifier] error, size of range value must is 3.");
		}
		size = ids.size();
		for (size_t i = 0; i < size; i ++) {
			range_ids_.push_back(ids[i]);
		}

		// identifier of character
		if (id_cfg["character"].empty()) {
			throw config::error("[identifier] error, no character attribute");
		}
		ids = utils::split(id_cfg["character"]);
		size = ids.size();
		for (size_t i = 0; i < size; i ++) {
			characters_.push_back(tcharacter(i, ids[i]));
		}

		loadscreen::increment_progress();
	} else {
		throw config::error("[identifier] error, must define [identifier] block in [units].");
	}

	// navigation_types_
	navigation_types_.push_back("boat0");
	navigation_types_.push_back("boat1");
	navigation_types_.push_back("boat2");
	navigation_types_.push_back("boat3");
	navigation_types_.push_back("boat4");
	navigation_types_.push_back("boat5");
	navigation_types_.push_back("boat6");

	std::map<unit_type*, const config*> ut_cfg_map;
	foreach (config &ut, cfg.child_range("unit_type"))
	{
		std::string id = ut["id"];
		// cannot support base_unit again.(wesnoth support it)

		// We insert an empty unit_type and build it after the copy (for performance).
		unit_type_map::iterator itor = insert(std::make_pair(id, unit_type(ut))).first;
		ut_cfg_map.insert(std::make_pair(&itor->second, &ut));
				
		loadscreen::increment_progress();
	}

	// Parse some unit_type'field may need other unit_type, example "advances_to",
	// So it is impossible that call build_unit_type in above foreach loop.

	// You may use member variable in unit_type to rember which config relative to this unit_type,
	// But this config maybe destroy after this call, in order to avoid confuse, 
	// don't let ths config appear in unit_type's member variable.
	for (std::map<unit_type*, const config*>::const_iterator it = ut_cfg_map.begin(); it != ut_cfg_map.end(); ++ it) {
		build_unit_type(*(it->first), *(it->second));
	}

	for (unit_type_map::iterator it = types_.begin(); it != types_.end(); ++ it) {
		unit_type& ut = it->second;
		int master = ut.master();
		if (master == hero::number_wall) {
			if (!wall_type_ || wall_type_->level() > ut.level()) {
				wall_type_ = &ut;
				// build all, form other field
				find(it->first);
			}			
		} else if (master == hero::number_keep) {
			if (!keep_type_ || keep_type_->level() > ut.level()) {
				keep_type_ = &ut;
				// build all, form other field
				find(it->first);
			}
		} else if (master == hero::number_market) {
			if (!market_type_ || market_type_->level() > ut.level()) {
				market_type_ = &ut;
				// build all, form other field
				find(it->first);
			}
		} else if (master == hero::number_tower) {
			if (!tower_type_ || tower_type_->level() > ut.level()) {
				tower_type_ = &ut;
				// build all, form other field
				find(it->first);
			}
		}
	}

	// form artifical types
	if (wall_type_) {
		artifical_types_[wall_type_->id()] = wall_type_;
	}
	if (keep_type_) {
		artifical_types_[keep_type_->id()] = keep_type_;
	}
	if (market_type_) {
		artifical_types_[market_type_->id()] = market_type_;
	}
	if (tower_type_) {
		artifical_types_[tower_type_->id()] = tower_type_;
	}

	if (const config recruit_cfg = cfg.child("recruit")) {
		if (recruit_cfg["id"].empty()) {
			throw config::error("[recruit] error, no id attribute");
		}
		std::vector<std::string> recruit_ids = utils::split(recruit_cfg["id"]);
		for (std::vector<std::string>::const_iterator it = recruit_ids.begin(); it != recruit_ids.end(); ++ it) {
			const unit_type* ut = unit_types.find(*it);
			if (!ut) {
				throw config::error("[recruit] error, cannot find unit type: " + *it);
			}
			if (std::find(can_recruit_.begin(), can_recruit_.end(), ut) != can_recruit_.end()) {
				throw config::error("[recruit] error, duplicate unit type: " + *it);
			}
			can_recruit_.push_back(ut);
		}
		loadscreen::increment_progress();
	} else {
		throw config::error("[recruit] error, must define [recruit] block in [units].");
	}

	if (const config &hide_help = cfg.child("hide_help")) {
		hide_help_all_ = hide_help["all"].to_bool();
		read_hide_help(hide_help);
	}
}

const unit_type *unit_type_data::find(const std::string& key, unit_type::BUILD_STATUS status) const
{
	if (key.empty() || key == "random") return NULL;

    const unit_type_map::iterator itor = types_.find(key);

    //This might happen if units of another era are requested (for example for savegames)
    if (itor == types_.end()){
		return NULL;
    }

	return &itor->second;
}

void unit_type_data::clear()
{
	types_.clear();
	artifical_types_.clear();
	movement_types_.clear();
	races_.clear();
	modifications_.clear();
	traits_.clear();
	complex_feature_.clear();
	treasures_.clear();
	abilities_.clear();
	specials_.clear();
	tactics_.clear();
	tactics_id_.clear();
	arms_ids_.clear();
	range_ids_.clear();
	characters_.clear();
	utype_anims_.clear();
	can_recruit_.clear();
	navigation_types_.clear();
	build_status_ = unit_type::NOT_BUILT;

	wall_type_ = NULL;
	keep_type_ = NULL;
	market_type_ = NULL;
	tower_type_ = NULL;

	hide_help_all_ = false;
	hide_help_race_.clear();
	hide_help_type_.clear();
}

unit_type& unit_type_data::build_unit_type(unit_type &ut, const config& cfg, unit_type::BUILD_STATUS status) const
{
	if (int(status) <= int(ut.build_status()))
		return ut;

	switch (status) {
	case unit_type::CREATED:
		ut.build_created(cfg, movement_types_, races_);
		break;
	case unit_type::BS_HELP_INDEX:
		// Build the data needed to feed the help index.
		ut.build_help_index(cfg, movement_types_, races_);
		break;
	default:
		ut.build_full(cfg, movement_types_, races_);
	}

	return ut;
}

void unit_type_data::read_hide_help(const config& cfg)
{
	if (!cfg)
		return;

	hide_help_race_.push_back(std::set<std::string>());
	hide_help_type_.push_back(std::set<std::string>());

	std::vector<std::string> races = utils::split(cfg["race"]);
	hide_help_race_.back().insert(races.begin(), races.end());

	std::vector<std::string> types = utils::split(cfg["type"]);
	hide_help_type_.back().insert(types.begin(), types.end());

	std::vector<std::string> trees = utils::split(cfg["type_adv_tree"]);
	hide_help_type_.back().insert(trees.begin(), trees.end());
	foreach(const std::string& t_id, trees) {
		unit_type_map::iterator ut = types_.find(t_id);
		if (ut != types_.end()) {
			std::set<std::string> adv_tree = ut->second.advancement_tree();
			hide_help_type_.back().insert(adv_tree.begin(), adv_tree.end());
		}
	}

	// we call recursively all the imbricated [not] tags
	read_hide_help(cfg.child("not"));
}

bool unit_type_data::hide_help(const std::string& type, const std::string& race) const
{
	bool res = hide_help_all_;
	int lvl = hide_help_all_ ? 1 : 0; // first level is covered by 'all=yes'

	// supposed to be equal but let's be cautious
	int lvl_nb = std::min(hide_help_race_.size(), hide_help_type_.size());

	for (; lvl < lvl_nb; ++lvl) {
		if (hide_help_race_[lvl].count(race) || hide_help_type_[lvl].count(type))
			res = !res; // each level is a [not]
	}
	return res;
}

int unit_type_data::arms_from_id(const std::string& id) const
{
	std::vector<std::string>::const_iterator it = std::find(arms_ids_.begin(), arms_ids_.end(), id);
	if (it != arms_ids_.end()) {
		return it - arms_ids_.begin();
	}
	return -1;
}

const std::string& unit_type_data::character_id(int character) const
{
	if (character < 0 || character >= (int)characters_.size()) {
		return null_str;
	} else {
		return characters_[character].id_;
	}
}

int unit_type_data::character_from_id(const std::string& id) const
{
	if (id.empty()) {
		return NO_CHARACTER;
	}
	for (std::vector<tcharacter>::const_iterator it = characters_.begin(); it != characters_.end(); ++ it) {
		if (it->id_ == id) {
			return it->index_;
		}
	}
	return NO_CHARACTER;
}

const std::string& unit_type_data::id_from_navigation(int navigation) const
{
	int quantum = navigation / game_config::navigation_per_level;
	int size = (int)navigation_types_.size();
	if (quantum < size) {
		return navigation_types_[quantum];
	} else {
		return navigation_types_.back();
	}
}

bool unit_type_data::navigation_can_advance(int prev, int next) const
{
	int prev_quantum = prev / game_config::navigation_per_level;
	int next_quantum = next / game_config::navigation_per_level;

	int size = (int)navigation_types_.size();
	if (prev_quantum >= size) {
		return false;
	}
	return prev_quantum != next_quantum;
}

const unit_race *unit_type_data::find_race(const std::string &key) const
{
	race_map::const_iterator i = races_.find(key);
	return i != races_.end() ? &i->second : NULL;
}

unit_type_data unit_types;
