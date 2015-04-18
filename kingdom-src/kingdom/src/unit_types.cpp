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

#include "asserts.hpp"
#include "game_config.hpp"
#include "gettext.hpp"
#include "loadscreen.hpp"
#include "map.hpp"
#include "hero.hpp"
#include "wml_exception.hpp"
#include "filesystem.hpp"
#include "area_anim.hpp"
#include "help.hpp"

#ifndef _ROSE_EDITOR
#include "unit_types.hpp"
#include "unit_map.hpp"
#include "unit.hpp"
#include "formula_string_utils.hpp"
#include "font.hpp"

#include <boost/foreach.hpp>

// anim_area
int app_fill_anim_tags(std::map<const std::string, int>& tags)
{
#if defined(_KINGDOM_EXE) || !defined(_WIN32)
	unit_animation::share_anims.clear();
#endif

	tags.insert(std::make_pair("card", area_anim::CARD));
	tags.insert(std::make_pair("pass_scenario", area_anim::PASS_SCENARIO));
	tags.insert(std::make_pair("blade", area_anim::BLADE));
	tags.insert(std::make_pair("pierce", area_anim::PIERCE));
	tags.insert(std::make_pair("impact", area_anim::IMPACT));
	tags.insert(std::make_pair("archery", area_anim::ARCHERY));
	tags.insert(std::make_pair("collapse", area_anim::COLLAPSE));
	tags.insert(std::make_pair("arcane", area_anim::ARCANE));
	tags.insert(std::make_pair("fire", area_anim::FIRE));
	tags.insert(std::make_pair("cold", area_anim::COLD));
	tags.insert(std::make_pair("strike", area_anim::STRIKE));
	tags.insert(std::make_pair("magic", area_anim::MAGIC));
	tags.insert(std::make_pair("heal", area_anim::HEAL));
	tags.insert(std::make_pair("destruct", area_anim::DESTRUCT));
	tags.insert(std::make_pair("formation_defend", area_anim::FORMATION_DEFEND));
	tags.insert(std::make_pair("income", area_anim::INCOME));

	return area_anim::MIN_UNIT_ANIM - 1;
}

void app_fill_anim(int type, const config& cfg)
{
#if defined(_KINGDOM_EXE) || !defined(_WIN32)
	unit_animation::share_anims.insert(std::make_pair(type, unit_animation(cfg)));
#endif
}

department::department(int type, const std::string& name, const std::string& image, const std::string& portrait)
	: type_(type)
	, name_(name)
	, image_(image)
	, portrait_(portrait)
	, exploiture_(0)
{
}

std::string form_title_str(int title)
{
	std::stringstream msgid;
	msgid << "title-" << title;
	return dsgettext("wesnoth-card", msgid.str().c_str());
}

tespecial::tespecial(int index, const std::string& id)
	: index_(index)
	, id_(id)
{
	std::stringstream strstr;
	strstr << HERO_PREFIX_STR_ESPECIAL << index;
	name_ = dgettext("wesnoth-card", strstr.str().c_str());
	image_ = "utype/" + id_ + ".png";
}

std::map<const std::string, tgenus::genus_t> tgenus::tags;
std::string tgenus::name_prefix_str = "genus-";

void tgenus::fill_tags()
{
	if (!tags.empty()) return;

	tags.insert(std::make_pair("turn_based", TURN_BASED));
	tags.insert(std::make_pair("half_realtime", HALF_REALTIME));
}

tgenus::genus_t tgenus::find(const std::string& tag)
{
	std::map<const std::string, genus_t>::const_iterator it = tags.find(tag);
	if (it != tags.end()) {
		return it->second;
	}
	return NONE;
}

tgenus::tgenus(const config& cfg)
	: index_(tgenus::NONE)
	, id_()
	, name_()
	, icon_()
{
	std::stringstream strstr;

	id_ = cfg["id"].str();
	if (id_.empty()) {
		throw config::error("[genus] error, no id attribute");
	}
	index_ = find(id_);
	if (index_ == NONE) {
		throw config::error("[genus] error, unsupport id " + id_);
	}
	const std::map<genus_t, tgenus>& genera = unit_types.genera();
	if (genera.find(index_) != genera.end()) {
		throw config::error("[genus] error, duplicate id " + id_);
	}

	strstr.str("");
	strstr << name_prefix_str << id_;
	name_ = dgettext("wesnoth-card", strstr.str().c_str());

	icon_ = cfg["icon"].str();
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

bool ttactic::select_one(int apply_to)
{
	return apply_to == apply_to_tag::PROVOKE || apply_to == apply_to_tag::REVIVAL;
}

ttactic::ttactic(int index, int complex_index, const config& cfg)
	: index_(-1)
	, range_(0)
	, effect_cfg_()
	, apply_to_(apply_to_tag::NONE)
	, type_filter_(0)
	, parts_()
	, id_()
	, name_()
	, description_()
	, bg_image_()
	, self_profit_(0)
	, self_hide_profit_(0)
	, self_alert_profit_(0)
	, self_clear_profit_(0)
	, self_heal_profit_(0)
	, friend_profit_(0)
	, friend_hide_profit_(0)
	, friend_clear_profit_(0)
	, friend_heal_profit_(0)
	, friend_select_one_profit_(0)
	, enemy_profit_(0)
	, enemy_provoke_profit_(0)
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

	const int min_point = 3;
	const int max_point = 9;
	point_ = cfg["point"].to_int(min_point);
	if (point_ < min_point || point_ > max_point) {
		throw config::error("[tactic] error, " + id_ + "'s point is invalid, must be in [3, 9]!");
	}
	level_ = cfg["level"].to_int();

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
	name_ = dgettext("wesnoth-card", strstr.str().c_str());

	strstr.str("");
	strstr << HERO_PREFIX_STR_TACTIC_DESC << id_;
	description_ = dgettext("wesnoth-card", strstr.str().c_str());

	if (!complex_) {
		std::map<std::string, int>::const_iterator find = range_id_map.find(cfg["range"].str());
		if (find != range_id_map.end()) {
			range_ = find->second;
		}
		if (!range_) {
			throw config::error("[tactic] error, " + id_ + " invalid range value.");
		}
		
		const config& filter_cfg = cfg.child("filter");
		if (filter_cfg) {
			std::vector<std::string> vstr = utils::split(filter_cfg["type"]);
			for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
				const std::string& id = *it;
				if (id == "troop") {
					type_filter_ |= family_tag::TROOP;
				} else if (id == "artifical") {
					type_filter_ |= family_tag::ARTIFICAL;
				} else if (id == "city") {
					type_filter_ |= family_tag::CITY;
				} else {
					throw config::error("[tactic] error, " + id_ + " is atom tactic, unknown type: " + id);
				}
			}
		}
	
		const config& effect = cfg.child("effect");
		if (!effect) {
			throw config::error("[tactic] error, " + id_ + " is atomic tactic, no [effect].");
		}
		effect_cfg_ = effect;

		const std::string& apply_to = effect["apply_to"];
		int tag = apply_to_tag::find(apply_to);
		if (tag == apply_to_tag::NONE) {
			throw config::error("[tactic] error, " + id_ + " is atomic tactic, unknown apply to: " + apply_to);
		}
		if (tag > apply_to_tag::UNIT_END) {
			throw config::error("[tactic] error, " + id_ + " is atomic tactic, not support apply to: " + apply_to);
		}
		apply_to_ = tag;

		if (select_one(apply_to_) && range_ == SELF) {
			throw config::error("[tactic] error, " + id_ + " is select one tactic, range must not be self!");
		}

		// calculate profit
		int profit = 0; // x 100
		int hide_profit = 0;
		int alert_profit = 0;
		int clear_profit = 0;
		int heal_profit = 0;
		int select_one_profit = 0;
		int damage_profit = 0;

		int total_value = 0;
		if (apply_to_ == apply_to_tag::RESISTANCE) {
			const config& resistance = effect.child("resistance");
			BOOST_FOREACH (const config::attribute &istrmap, resistance.attribute_range()) {
				total_value += lexical_cast<int>(istrmap.second);
			}
			profit += total_value / 2;
		} else if (apply_to_ == apply_to_tag::ATTACK) {
			total_value = effect["increase_damage"].to_int();

			profit += total_value * 50;
		} else if (apply_to_ == apply_to_tag::ENCOURAGE) {
			total_value = effect["increase"].to_int();

			profit += total_value * 30;
		} else if (apply_to_ == apply_to_tag::DEMOLISH) {
			total_value = effect["increase"].to_int();

			// same value but lower profit
			profit += total_value * 30;
		} else if (apply_to_ == apply_to_tag::MOVEMENT) {
			total_value = effect["increase"].to_int();

			profit += total_value * 10;
		} else if (apply_to_ == apply_to_tag::DAMAGE) {
			total_value = effect["ratio"].to_int();

			damage_profit += total_value / 2;
		} else if (apply_to_ == apply_to_tag::HIDE) {
			hide_profit += 80;

		} else if (apply_to_ == apply_to_tag::ALERT) {
			alert_profit += 80;

		} else if (apply_to_ == apply_to_tag::CLEAR) {
			clear_profit += 30;

		} else if (apply_to_ == apply_to_tag::HEAL) {
			heal_profit += 40;

		} else if (apply_to_ == apply_to_tag::ACTION) {
			total_value = effect["increase"].to_int();
			profit += total_value * 5;

		} else if (apply_to_ == apply_to_tag::PROVOKE) {
			select_one_profit += 100;

		} else if (apply_to_ == apply_to_tag::REVIVAL) {
			select_one_profit += 150;
		}

		if (range_ == SELF) {
			self_profit_ = profit;
			self_hide_profit_ = hide_profit;
			self_alert_profit_ = alert_profit;
			self_clear_profit_ = clear_profit;
			self_heal_profit_ = heal_profit;
		} else if (range_ == FRIEND) {
			friend_profit_ = profit;
			friend_hide_profit_ = hide_profit;
			friend_clear_profit_ = clear_profit;
			friend_heal_profit_ = heal_profit;
			friend_select_one_profit_ = select_one_profit;
		} else if (range_ == ENEMY) {
			enemy_profit_ = -1 * profit + damage_profit;
			enemy_provoke_profit_ = select_one_profit;
		}
	} else {
		for (std::vector<const ttactic*>::const_iterator it = parts_.begin(); it != parts_.end(); ++ it) {
			const ttactic& t = **it;
			if (t.range() == SELF) {
				range_ |= SELF;
			} else if (t.range() == FRIEND) {
				range_ |= FRIEND;
			} else if (t.range() == ENEMY) {
				range_ |= ENEMY;
			}

			self_profit_ += t.self_profit();
			self_hide_profit_ += t.self_hide_profit();
			self_alert_profit_ += t.self_alert_profit();
			self_clear_profit_ += t.self_clear_profit();
			self_heal_profit_ += t.self_heal_profit();

			friend_profit_ += t.friend_profit();
			friend_hide_profit_ += t.friend_hide_profit();
			friend_clear_profit_ += t.friend_clear_profit();
			friend_heal_profit_ += t.friend_heal_profit();
			friend_select_one_profit_ += t.friend_select_one_profit();

			enemy_profit_ += t.enemy_profit();
			enemy_provoke_profit_ += t.enemy_provoke_profit();
		}
	}

	if ((range_ & FRIEND) || (range_ & ENEMY)) {
		bg_image_ = "tactic/bg-mess-increase.png";
	} else {
		bg_image_ = "tactic/bg-single-increase.png";
	}
}

int ttactic::cost() const
{
	return level_ * game_config::cost_per_level * 2;
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
	return ret;
}

bool ttactic::oneoff() const
{
	return apply_to_ >= apply_to_tag::ONEOFF_MIN && apply_to_ <= apply_to_tag::ONEOFF_MAX;
}

bool ttactic::select_one() const
{
	return select_one_internal() != apply_to_tag::NONE;
}

int ttactic::select_one_internal() const
{
	for (std::vector<const ttactic*>::const_iterator it = parts_.begin(); it != parts_.end(); ++ it) {
		const ttactic& t = **it;
		if (select_one(t.apply_to())) {
			return t.apply_to();
		}
	}
	return apply_to_tag::NONE;
}

#if defined(_KINGDOM_EXE) || !defined(_WIN32)
#include "team.hpp"
#include "resources.hpp"

std::map<int, std::vector<map_location> > ttactic::touch_units(unit_map& units, unit& u) const
{
	std::map<int, std::vector<map_location> > ret;
	const team& tactican_team = (*resources::teams)[u.side() - 1];
	std::vector<map_location> touched;

	int part = 0;
	bool select_one_absent = false;
	for (std::vector<const ttactic*>::const_iterator it = parts_.begin(); it != parts_.end(); ++ it, part ++) {
		const ttactic& t = **it;
		touched.clear();
		
		if (t.range() & SELF) {
			touched.push_back(u.get_location());
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
					unit* target = get_visible_unit(loc, tactican_team);
					if (!target) {
						continue;
					}
					if (target->is_commoner()) {
						continue;
					}
					if ((t.range() & ttactic::FRIEND) && tactican_team.is_enemy(target->side())) {
						continue;
					}
					if ((t.range() & ttactic::ENEMY) && !tactican_team.is_enemy(target->side())) {
						continue;
					}						
					if (std::find(touched.begin(), touched.end(), target->get_location()) != touched.end()) {
						continue;
					}
					if (t.type_filter()) {
						int type = family_tag::ARTIFICAL;
						if (!target->is_artifical()) {
							type = family_tag::TROOP;
						} else if (target->is_city()) {
							type = family_tag::CITY;
						}
						if (!(type & t.type_filter())) {
							continue;
						}
					}
					// special
					if (t.apply_to() == apply_to_tag::REVIVAL && target->side() != u.side()) {
						continue;
					}
					if (t.select_one() && !target->select_tactic_can_effect(u, t)) {
						continue;
					}
					touched.push_back(target->get_location());
				}
			}
		}
		ret[part] = touched;
		if (t.select_one() && touched.empty()) {
			select_one_absent = true;
		}
	}
	if (select_one_absent) {
		ret.clear();
	}
	return ret;
}
#endif

std::string tcharacter_::expression() const
{
	std::stringstream strstr;

	strstr << "(";
	if (leadership_) {
		strstr << dsgettext("wesnoth-hero", "leadership");
		strstr << "*" << leadership_;
	}
	if (force_) {
		if (strstr.str().size() > 1) {
			strstr << " + ";
		}
		strstr << dsgettext("wesnoth-hero", "force");
		strstr << "*" << force_;
	}
	if (intellect_) {
		if (strstr.str().size() > 1) {
			strstr << " + ";
		}
		strstr << dsgettext("wesnoth-hero", "intellect");
		strstr << "*" << intellect_;
	}
	if (spirit_) {
		if (strstr.str().size() > 1) {
			strstr << " + ";
		}
		strstr << dsgettext("wesnoth-hero", "spirit");
		strstr << "*" << spirit_;
	}
	if (charm_) {
		if (strstr.str().size() > 1) {
			strstr << " + ";
		}
		strstr << dsgettext("wesnoth-hero", "charm");
		strstr << "*" << charm_;
	}
	strstr << ") / 100";

	return strstr.str();
}

int tcharacter::min_complex_index = 100;

tcharacter::tcharacter(int index, int complex_index, const config& cfg)
	: index_(-1)
	, effect_cfg_()
	, apply_to_(apply_to_tag::NONE)
	, id_()
	, name_()
	, description_()
	, parts_()
{
	id_ = cfg["id"].str();

	std::vector<std::string> parts = utils::split(cfg["parts"].str());
	if (!parts.empty()) {
		const std::map<int, tcharacter>& characters = unit_types.characters();
		const std::map<std::string, int>& characters_id = unit_types.characters_id();
		for (std::vector<std::string>::const_iterator it = parts.begin(); it != parts.end(); ++ it) {
			std::map<std::string, int>::const_iterator find = characters_id.find(*it);
			if (find == characters_id.end()) {
				throw config::error("[character] error, " + id_ + " is complex character, but cannot find part: " + *it);
			}
			if (find->second >= min_complex_index) {
				throw config::error("[character] error, " + id_ + " is complex character, " + *it + " is complex also.");
			}
			const tcharacter* t = &(characters.find(find->second)->second);
			if (std::find(parts_.begin(), parts_.end(), t) != parts_.end()) {
				throw config::error("[character] error, " + id_ + " is complex character, its sub-character " + *it + " duplicated.");
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
	strstr << HERO_PREFIX_STR_CHARACTER << id_;
	name_ = dgettext("wesnoth-card", strstr.str().c_str());

	strstr.str("");
	strstr << HERO_PREFIX_STR_CHARACTER_DESC << id_;
	description_ = dgettext("wesnoth-card", strstr.str().c_str());

	if (!complex_) {
		if (cfg.has_attribute("leadership")) {
			leadership_ = cfg["leadership"].to_int();
		}
		if (cfg.has_attribute("force")) {
			force_ = cfg["force"].to_int();
		}
		if (cfg.has_attribute("intellect")) {
			intellect_ = cfg["intellect"].to_int();
		}
		if (cfg.has_attribute("spirit")) {
			spirit_ = cfg["spirit"].to_int();
		}
		if (cfg.has_attribute("charm")) {
			charm_ = cfg["charm"].to_int();
		}

		if (!leadership_ && !force_ && !intellect_ && !spirit_ && !charm_) {
			throw config::error("[character] error, " + id_ + " is atomic character, must define ratio!");
		}

		const std::string& apply_to = cfg["apply_to"];
		int tag = apply_to_tag::find(apply_to);
		if (tag == apply_to_tag::NONE) {
			throw config::error("[character] error, " + id_ + " is atomic character, unknown apply to: " + apply_to);
		}
		if (tag < apply_to_tag::CHARACTER_MIN || tag > apply_to_tag::CHARACTER_MAX) {
			throw config::error("[character] error, " + id_ + " is atomic character, not support apply to: " + apply_to);
		}
		apply_to_ = tag;
	}
}

int tcharacter::level(const hero& h) const
{
	int l = 0;

	if (leadership_) {
		l += fxptoi9(h.leadership_) * leadership_;
	}
	if (force_) {
		l += fxptoi9(h.force_) * force_;
	}
	if (intellect_) {
		l += fxptoi9(h.intellect_) * intellect_;
	}
	if (spirit_) {
		l += fxptoi9(h.spirit_) * spirit_;
	}
	if (charm_) {
		l += fxptoi9(h.charm_) * charm_;
	}

	l = l / 100;
	if (l < 0) l = 0;
	if (l > 100) l = 100;

	return l;
}

int tdecree::min_complex_index = 100;

tdecree::tdecree(int index, int complex_index, const config& cfg)
	: index_(-1)
	, effect_cfg_()
	, apply_to_(apply_to_tag::NONE)
	, id_()
	, name_()
	, description_()
	, parts_()
	, front_(false)
	, min_level_(0)
	, require_artifical_()
{
	id_ = cfg["id"].str();

	std::vector<std::string> parts = utils::split(cfg["parts"].str());
	if (!parts.empty()) {
		const std::map<int, tdecree>& decrees = unit_types.decrees();
		const std::map<std::string, int>& decrees_id = unit_types.decrees_id();
		for (std::vector<std::string>::const_iterator it = parts.begin(); it != parts.end(); ++ it) {
			std::map<std::string, int>::const_iterator find = decrees_id.find(*it);
			if (find == decrees_id.end()) {
				throw config::error("[decree] error, " + id_ + " is complex decree, but cannot find part: " + *it);
			}
			if (find->second >= min_complex_index) {
				throw config::error("[decree] error, " + id_ + " is complex decree, " + *it + " is complex also.");
			}
			const tdecree* t = &(decrees.find(find->second)->second);
			if (std::find(parts_.begin(), parts_.end(), t) != parts_.end()) {
				throw config::error("[decree] error, " + id_ + " is complex decree, its sub-character " + *it + " duplicated.");
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
	strstr << HERO_PREFIX_STR_DECREE << id_;
	name_ = dgettext("wesnoth-card", strstr.str().c_str());

	strstr.str("");
	strstr << HERO_PREFIX_STR_DECREE_DESC << id_;
	description_ = dgettext("wesnoth-card", strstr.str().c_str());

	if (!complex_) {
		if (cfg.has_attribute("leadership")) {
			leadership_ = cfg["leadership"].to_int();
		}
		if (cfg.has_attribute("force")) {
			force_ = cfg["force"].to_int();
		}
		if (cfg.has_attribute("intellect")) {
			intellect_ = cfg["intellect"].to_int();
		}
		if (cfg.has_attribute("spirit")) {
			spirit_ = cfg["spirit"].to_int();
		}
		if (cfg.has_attribute("charm")) {
			charm_ = cfg["charm"].to_int();
		}

		if (!leadership_ && !force_ && !intellect_ && !spirit_ && !charm_) {
			throw config::error("[decree] error, " + id_ + " is atomic decree, must define ratio!");
		}

		const config& effect = cfg.child("effect");
		if (!effect) {
			throw config::error("[decree] error, " + id_ + " is atom decree, no [effect].");
		}
		effect_cfg_ = effect;

		const std::string& apply_to = effect["apply_to"];
		int tag = apply_to_tag::find(apply_to);
		if (tag == apply_to_tag::NONE) {
			throw config::error("[decree] error, " + id_ + " is atomic decree, unknown apply to: " + apply_to);
		}
/*
		if (tag < apply_to_tag::CHARACTER_MIN || tag > apply_to_tag::CHARACTER_MAX) {
			throw config::error("[decree] error, " + id_ + " is atomic decree, not support apply to: " + apply_to);
		}
*/
		apply_to_ = tag;

	}

	if (complex_) {	
		for (std::vector<const tdecree*>::const_iterator it = parts_.begin(); it != parts_.end(); ++ it) {
			const tdecree& d = **it;
			if (!front_ && d.front_) {
				front_ = true;
			}
			if (min_level_ < d.min_level_) {
				min_level_ = d.min_level_;
			}
			require_artifical_.insert(d.require_artifical().begin(), d.require_artifical().end());
		}
	}

	const config& filter = cfg.child("filter");
	if (filter) {
		// override
		if (filter.has_attribute("front")) {
			front_ = filter["front"].to_bool();
		}
		if (filter.has_attribute("level")) {
			min_level_ = filter["level"].to_int();
		}
		if (filter.has_attribute("artifical")) {
			require_artifical_.clear();
			const std::vector<std::string> vstr = utils::split(filter["artifical"].str());
			for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
				const unit_type* ut = unit_types.id_type(*it);
				if (ut) {
					require_artifical_.insert(ut->master());
				}
			}
		}
	}
}

int tdecree::level(const hero& h) const
{
	int l = 0;

	if (leadership_) {
		l += fxptoi9(h.leadership_) * leadership_;
	}
	if (force_) {
		l += fxptoi9(h.force_) * force_;
	}
	if (intellect_) {
		l += fxptoi9(h.intellect_) * intellect_;
	}
	if (spirit_) {
		l += fxptoi9(h.spirit_) * spirit_;
	}
	if (charm_) {
		l += fxptoi9(h.charm_) * charm_;
	}

	l = l / 100;
	if (l < 0) l = 0;
	if (l > 100) l = 100;

	return l;
}

std::string tnoble::form_name_msgid(const std::string& id)
{
	std::stringstream strstr;
	strstr << HERO_PREFIX_STR_NOBLE << id;
	return strstr.str();
}

std::string tnoble::form_name(const std::string& id)
{
	std::stringstream strstr;
	strstr << HERO_PREFIX_STR_NOBLE << id;
	return dgettext("wesnoth-card", strstr.str().c_str());
}

tnoble::tnoble(int index, const config& cfg)
	: index_(index)
	, id_(cfg["id"].str())
	, level_()
	, leader_(false)
	, formation_(false)
	, name_()
{
	memset(&condition_, 0, sizeof(tcondition));
	memset(&effect_, 0, sizeof(hblock));

	level_ = cfg["level"].to_int();
	if (level_ < 0 || level_ > game_config::max_noble_level) {
		throw config::error("[noble] error, " + id_ + " is invalid level!");
	}
	leader_ = cfg["leader"].to_bool();
	if (leader_) {
		formation_ = true;
	} else {
		formation_ = cfg["formation"].to_bool();
	}

	const config& cond = cfg.child("condition");
	if (cond) {
		condition_.city = cond["city"].to_int(0);
		condition_.meritorious = cond["meritorious"].to_int(0);
	}

	const config& effect = cfg.child("effect");
	if (effect) {
		effect_.leadership = effect["leadership"].to_int(0);
		effect_.force = effect["force"].to_int(0);
		effect_.intellect = effect["intellect"].to_int(0);
		effect_.spirit = effect["spirit"].to_int(0);
		effect_.charm = effect["charm"].to_int(0);
	}

	name_ = form_name(id_);
}

bool tnoble::operator<(const tnoble& that) const
{
	return index_ <= that.index_;
}

ttreasure::ttreasure(int index, int feature)
	: index_(index)
	, feature_(feature)
{
	std::stringstream strstr;
	strstr.str("");
	strstr << HERO_PREFIX_STR_TREASURE << index;
	name_ = dgettext("wesnoth-card", strstr.str().c_str());

	strstr.str("");
	strstr << "treasure/" << index << ".png";
	image_ = strstr.str();
}

namespace advance_tree {

void generate_advance_tree_internal(const base* current, std::vector<node>& advances_to, bool& to_branch)
{
	for (std::vector<node>::iterator it2 = advances_to.begin(); it2 != advances_to.end(); ++ it2) {
		const std::vector<std::string>& advances_to = it2->current->advances_to();
		if (std::find(advances_to.begin(), advances_to.end(), current->id()) != advances_to.end()) {
			it2->advances_to.push_back(node(current));
			to_branch = true;
		}
		generate_advance_tree_internal(current, it2->advances_to, to_branch);
	}
}

void hang_branch_internal(std::vector<node*>& dst, std::vector<node>& advances_to, bool& hang_branch)
{
	size_t size = dst.size();
	for (std::vector<node>::iterator it = advances_to.begin(); it != advances_to.end(); ++ it) {
		node* n = &*it;
		const std::vector<std::string>& advances_to = n->current->advances_to();
		for (size_t hanging = 0; hanging < size; hanging ++) {
			if (!dst[hanging]) {
				continue;
			}
			node* hanging_n = dst[hanging];
			if (std::find(advances_to.begin(), advances_to.end(), hanging_n->current->id()) != advances_to.end()) {
				n->advances_to.push_back(*hanging_n);
				delete hanging_n;
				dst[hanging] = NULL;
				hang_branch = true;
			}
		}
		hang_branch_internal(dst, n->advances_to, hang_branch);
	}
}

void generate_advance_tree(const std::map<std::string, const base*>& src, std::vector<node*>& dst)
{
	for (std::map<std::string, const base*>::const_iterator it = src.begin(); it != src.end(); ++ it) {
		const base& current = *(it->second);
		bool to_branch = false;
		for (std::vector<node*>::iterator it2 = dst.begin(); it2 != dst.end(); ++ it2) {
			node* n = *it2;
			const std::vector<std::string>& advances_to = n->current->advances_to();
			if (std::find(advances_to.begin(), advances_to.end(), current.id()) != advances_to.end()) {
				n->advances_to.push_back(node(&current));
				to_branch = true;
			}
			generate_advance_tree_internal(&current, n->advances_to, to_branch);
		}
		if (!to_branch) {
			dst.push_back(new node(&current));
		}
	}

	// check every teminate node, if necessary, hang root-branch to it.
	size_t size = dst.size();
	int max_hang_times = 2;
	bool hang_branch = false;
	do {
		hang_branch = false;
		for (size_t analysing = 0; analysing < size; analysing ++) {
			if (!dst[analysing]) {
				continue;
			}
			// search terminate node
			node* n = dst[analysing];
			const std::vector<std::string>& advances_to = n->current->advances_to();
			for (size_t hanging = 0; hanging < size; hanging ++) {
				if (!dst[hanging]) {
					continue;
				}
				node* hanging_n = dst[hanging];
				if (std::find(advances_to.begin(), advances_to.end(), hanging_n->current->id()) != advances_to.end()) {
					n->advances_to.push_back(*hanging_n);
					delete hanging_n;
					dst[hanging] = NULL;
					hang_branch = true;
				}
			}
			hang_branch_internal(dst, n->advances_to, hang_branch);
		}
	} while (-- max_hang_times && hang_branch);

	VALIDATE(!hang_branch, "Must one time!");

	// remove branch that is NULL.
	for (std::vector<node*>::iterator it = dst.begin(); it != dst.end(); ) {
		if (*it == NULL) {
			it = dst.erase(it);
		} else {
			++ it;
		}
	}
}
}

ttechnology null_technology;

ttechnology::ttechnology(const config& cfg)
	: id_()
	, name_()
	, description_()
	, occasion_(NONE)
	, max_experience_(0)
	, apply_to_(apply_to_tag::NONE)
	, relative_(HEROS_NO_CHARACTER)
	, parts_()
	, type_filter_(0)
	, arms_filter_(0)
	, mode_filter_(0)
	, stratagem_(0)
{
	id_ = cfg["id"].str();
	max_experience_ = cfg["experience"].to_int(0);
	if (max_experience_ <= 0) {
		throw config::error("[technology] error, " + id_ + "'s experience must great than 0");
	}

	const std::string& advances_to_val = cfg["advances_to"];
	if (advances_to_val != "null" && advances_to_val != "") {
        advances_to_ = utils::split(advances_to_val);
	}

	std::vector<std::string> parts = utils::split(cfg["parts"].str());
	if (!parts.empty()) {
		const std::map<std::string, ttechnology>& technologies = unit_types.technologies();
		for (std::vector<std::string>::const_iterator it = parts.begin(); it != parts.end(); ++ it) {
			std::map<std::string, ttechnology>::const_iterator find = technologies.find(*it);
			if (find == technologies.end()) {
				throw config::error("[technology] error, " + id_ + " is complex technology, but cannot find part: " + *it);
			}
			if (find->second.complex()) {
				throw config::error("[technology] error, " + id_ + " is complex technology, " + *it + " is complex also.");
			}
			const ttechnology* t = &(find->second);
			if (std::find(parts_.begin(), parts_.end(), t) != parts_.end()) {
				throw config::error("[technology] error, " + id_ + " is complex technology, its sub-technology " + *it + " duplicated.");
			}
			parts_.push_back(t);
		}
		complex_ = true;
	} else {
		complex_ = false;
	}

	std::stringstream strstr;
	strstr << HERO_PREFIX_STR_TECHNOLOGY << id_;
	name_ = dgettext("wesnoth-card", strstr.str().c_str());

	strstr.str("");
	strstr << HERO_PREFIX_STR_TECHNOLOGY_DESC << id_;
	description_ = dgettext("wesnoth-card", strstr.str().c_str());

	if (!complex_) {
		const config& effect = cfg.child("effect");
		if (!effect) {
			throw config::error("[technology] error, " + id_ + " is atom technology, no [effect].");
		}
		effect_cfg_ = effect;

		const std::string& apply_to = effect["apply_to"];
		int tag = apply_to_tag::find(apply_to);
		if (tag == apply_to_tag::NONE) {
			throw config::error("[technology] error, " + id_ + " is atom technology, unknown apply to: " + apply_to);
		}
		if (tag <= apply_to_tag::UNIT_END) {
			occasion_ = MODIFY;
		} else {
			occasion_ = FINISH;
		}
		apply_to_ = tag;
		stratagem_ = apply_to_ == apply_to_tag::STRATAGEM;
		
		const std::map<std::string, ttechnology>& technologies = unit_types.technologies();
		const config& filter_cfg = cfg.child("filter");
		if (filter_cfg) {
			std::vector<std::string> vstr = utils::split(filter_cfg["type"]);
			for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
				const std::string& id = *it;
				if (id == "troop") {
					type_filter_ |= family_tag::TROOP;
				} else if (id == "artifical") {
					type_filter_ |= family_tag::ARTIFICAL;
				} else if (id == "city") {
					type_filter_ |= family_tag::CITY;
				} else {
					throw config::error("[technology] error, " + id_ + " is atom technology, unknown type: " + id);
				}
			}
			vstr = utils::split(filter_cfg["arms"]);
			for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
				int arms = unit_types.arms_from_id(*it);
				if (arms >= 0) {
					arms_filter_ |= 1 << arms;
				} else {
					throw config::error("[technology] error, " + id_ + " is atom technology, unknown arms: " + *it);
				}
			}
			vstr = utils::split(filter_cfg["mode"]);
			for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
				int mode= mode_tag::find(*it);
				if (mode != mode_tag::NONE) {
					mode_filter_ |= mode;
				} else {
					throw config::error("[technology] error, " + id_ + " is atom technology, unknown mode: " + *it);
				}
			}
		}
	}

	const std::string relative = cfg["relative"];
	if (!relative.empty()) {
		const std::map<std::string, int>& characters_id = unit_types.characters_id();
		std::map<std::string, int>::const_iterator find = characters_id.find(relative);
		if (find == characters_id.end()) {
			throw config::error("[technology] error, " + id_ + " is technology, unknown relative: " + relative);
		}
		if (unit_types.character(find->second).complex()) {
			throw config::error("[technology] error, " + id_ + "'s relative(" + relative + ") must be atomic!");
		}
		relative_ = find->second;
	}
}

bool ttechnology::filter(int type, int arms) const
{
	if (type_filter_ && !(type & type_filter_)) {
		return false;
	}
	if (arms_filter_ && !((1 << arms) & arms_filter_)) {
		return false;
	}
	return true;
}

bool ttechnology::filter_mode(int mode) const
{
	for (std::vector<const ttechnology*>::const_iterator it = parts_.begin(); it != parts_.end(); ++ it) {
		const ttechnology& t = **it;
		int filter = t.mode_filter();
		if (filter && !(mode & filter)) {
			return false;
		}
	}
	return true;
}

tformation_profile::tformation_profile(int index, const config& cfg)
	: index_(index)
	, attack_flag_(0)
	, arms_type_()
	, arms_range_id_()
	, arms_range_(0)
	, arms_count_(0)
{
	id_ = cfg["id"].str();

	std::stringstream strstr;
	strstr << HERO_PREFIX_STR_FORMATION << id_;
	name_ = dgettext("wesnoth-card", strstr.str().c_str());

	strstr.str("");
	strstr << HERO_PREFIX_STR_FORMATION_DESC << id_;
	description_ = dgettext("wesnoth-card", strstr.str().c_str());

	std::vector<std::string> vstr = utils::split(cfg["attack"].str());
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		if (*it == "slow") {
			attack_flag_ |= SLOW;
		} else if (*it == "break") {
			attack_flag_ |= BREAK;
		} else if (*it == "poison") {
			attack_flag_ |= POISON;
		} 
	}

	const config& arms = cfg.child("arms");
	if (!arms) {
		throw config::error("[formation] error, " + id_ + " has not [arms].");
	}
	vstr = utils::split(arms["type"].str());
	const std::vector<std::string>& atype_ids = unit_types.atype_ids();
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		if (std::find(atype_ids.begin(), atype_ids.end(), *it) == atype_ids.end()) {
			throw config::error("[formation] error, " + id_ + " 's [arms] hasn't invalid atype: " + *it + ".");
		}
		arms_type_.insert(*it);
	}
	const std::vector<std::string>& range_ids = unit_types.range_ids();
	std::vector<std::string>::const_iterator find = std::find(range_ids.begin(), range_ids.end(), arms["range"].str());
	if (find == range_ids.end()) {
		throw config::error("[formation] error, " + id_ + " 's [arms] hasn't valid range attribute.");
	}
	arms_range_id_ = *find;
	arms_range_ = std::distance(range_ids.begin(), find);
	arms_count_ = arms["count"].to_int();
	if (arms_count_ <= 0 || arms_count_ >= 7) {
		throw config::error("[formation] error, " + id_ + " 's [arms] hasn't valid count attribute.");
	}

	strstr.str("");
	strstr << game_config::path << "/data/core/images/formation/" << HERO_PREFIX_STR_FORMATION_ICON << id_ << ".png";
	if (file_exists(strstr.str())) {
		strstr.str("");
		strstr << "formation/" << HERO_PREFIX_STR_FORMATION_ICON << id_ << ".png";
	} else {
		strstr.str("");
		strstr << "formation/" << HERO_PREFIX_STR_FORMATION_ICON << "default.png";
	}
	icon_ = strstr.str();

	strstr.str("");
	strstr << game_config::path << "/data/core/images/formation/" << HERO_PREFIX_STR_FORMATION_BG << id_ << ".png";
	if (file_exists(strstr.str())) {
		strstr.str("");
		strstr << "formation/" << HERO_PREFIX_STR_FORMATION_BG << id_ << ".png";
	} else {
		strstr.str("");
		strstr << "formation/" << HERO_PREFIX_STR_FORMATION_BG << "default.png";
	}
	bg_image_ = strstr.str();
}

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
		BOOST_FOREACH (const config::any_child &c, result->second.all_children_range()) {
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
			BOOST_FOREACH (const config::any_child &vp, specials.all_children_range()) {
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
		BOOST_FOREACH (const config::any_child &value, set_specials.all_children_range()) {
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
		BOOST_FOREACH (const config::attribute &i, resistance.attribute_range()) {
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
	if (terrain) return "";

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

std::string unit_type::move(const std::string& race, const std::string& id, bool terrain, int number)
{
	if (terrain) return "";

	std::stringstream strstr;

	strstr << "units/";
	strstr << race << "/";
	strstr << id << "-" << "move" << "-";
	strstr << number << ".png";

	return strstr.str();
}

std::string unit_type::build(const std::string& race, const std::string& id, bool terrain, int number)
{
	if (terrain) return "";

	std::stringstream strstr;

	strstr << "units/";
	strstr << race << "/";
	strstr << id << "-" << "build" << "-";
	strstr << number << ".png";

	return strstr.str();
}

std::string unit_type::repair(const std::string& race, const std::string& id, bool terrain, int number)
{
	if (terrain) return "";

	std::stringstream strstr;

	strstr << "units/";
	strstr << race << "/";
	strstr << id << "-" << "repair" << "-";
	strstr << number << ".png";

	return strstr.str();
}

std::string unit_type::die(const std::string& race, const std::string& id, bool terrain, int number)
{
	if (terrain) return "";

	std::stringstream strstr;

	strstr << "units/";
	strstr << race << "/";
	strstr << id << "-" << "die" << "-";
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
		if (it->find("forest") != std::string::npos) {
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
	BOOST_FOREACH (const config::any_child& c, src.all_children_range()) {
		config& adding = dst.add_child(c.key);
		BOOST_FOREACH (const config::attribute &i, c.cfg.attribute_range()) {
			adding[i.first] = utils::interpolate_variables_into_string(i.second, &symbols);
		}
		replace_anim_cfg_inernal(c.cfg, adding, symbols);
	}
}

static void replace_anim_cfg(const std::string& tag, const std::string& src, config& dst, const utils::string_map& symbols)
{
	typedef std::multimap<std::string, const config>::const_iterator Itor;
	std::pair<Itor, Itor> its = unit_types.utype_anims().equal_range(src);
	while (its.first != its.second) {
		config& sub_cfg = dst.add_child(tag);
		if (symbols.count("offset_x")) {
			sub_cfg["offset_x"] = symbols.find("offset_x")->second;
		}
		if (symbols.count("offset_y")) {
			sub_cfg["offset_y"] = symbols.find("offset_y")->second;
		}
		const config& anim_cfg = its.first->second;
		// replace
		BOOST_FOREACH (const config::attribute &i, anim_cfg.attribute_range()) {
			sub_cfg[i.first] = utils::interpolate_variables_into_string(i.second, &symbols);
		}
		replace_anim_cfg_inernal(anim_cfg, sub_cfg, symbols);
		++ its.first;
	}
}

void unit_type::defend_anim(const std::string& tag, const std::string& race, const std::string& id, bool terrain, config& cfg)
{
	utils::string_map symbols;
	symbols["base_png"] = image(race, id, terrain);
	symbols["hit_png"] = hit(race, id, terrain);
	symbols["miss_png"] = miss(race, id, terrain);

	replace_anim_cfg(tag, "defend", cfg, symbols);
}

void unit_type::resistance_anim(const std::string& tag, const std::string& race, const std::string& id, bool terrain, config& cfg)
{
	utils::string_map symbols;
	symbols["leading_png"] = leading(race, id, terrain);

	replace_anim_cfg(tag, "resistance", cfg, symbols);
}

void unit_type::leading_anim(const std::string& tag, const std::string& race, const std::string& id, bool terrain, config& cfg)
{
	utils::string_map symbols;
	symbols["leading_png"] = leading(race, id, terrain);

	replace_anim_cfg(tag, "leading", cfg, symbols);
}

void unit_type::healing_anim(const std::string& tag, const std::string& race, const std::string& id, bool terrain, config& cfg)
{
	utils::string_map symbols;
	symbols["ranged_attack_1_png"] = attack_image(race, id, terrain, 1, 1);
	symbols["ranged_attack_2_png"] = attack_image(race, id, terrain, 1, 2);
	symbols["ranged_attack_3_png"] = attack_image(race, id, terrain, 1, 3);
	symbols["ranged_attack_4_png"] = attack_image(race, id, terrain, 1, 4);

	replace_anim_cfg(tag, "healing", cfg, symbols);
}

void unit_type::idle_anim(const std::string& tag, const std::string& race, const std::string& id, bool terrain, const std::string& idle_sound, config& cfg, bool multigrid)
{
	utils::string_map symbols;
	std::string idle1 = get_binary_file_location("images", idle(race, id, terrain, 1));
	if (!idle1.empty()) {
		symbols["idle_1_png"] = idle(race, id, terrain, 1);
		symbols["idle_2_png"] = idle(race, id, terrain, 2);
		symbols["idle_3_png"] = idle(race, id, terrain, 3);
		symbols["idle_4_png"] = idle(race, id, terrain, 4);
	} else {
		symbols["idle_1_png"] = miss(race, id, terrain);
		symbols["idle_2_png"] = attack_image(race, id, terrain, 1, 1);
		symbols["idle_3_png"] = attack_image(race, id, terrain, 1, 2);
		symbols["idle_4_png"] = attack_image(race, id, terrain, 1, 3);
	}
	symbols["sound_ogg"] = idle_sound;

	const std::string id_in_cfg = multigrid? "multi_idle": "idle";
	replace_anim_cfg(tag, id_in_cfg, cfg, symbols);
}

void unit_type::healed_anim(const std::string& tag, const std::string& race, const std::string& id, bool terrain, config& cfg)
{
	utils::string_map symbols;
	std::string idle1 = get_binary_file_location("images", idle(race, id, terrain, 1));
	if (!idle1.empty()) {
		symbols["idle_1_png"] = idle(race, id, terrain, 1);
		symbols["idle_2_png"] = idle(race, id, terrain, 2);
		symbols["idle_3_png"] = idle(race, id, terrain, 3);
		symbols["idle_4_png"] = idle(race, id, terrain, 4);
	} else {
		symbols["idle_1_png"] = miss(race, id, terrain);
		symbols["idle_2_png"] = attack_image(race, id, terrain, 1, 1);
		symbols["idle_3_png"] = attack_image(race, id, terrain, 1, 2);
		symbols["idle_4_png"] = attack_image(race, id, terrain, 1, 3);
	}

	replace_anim_cfg(tag, "healed", cfg, symbols);
}

void unit_type::movement_anim(const std::string& tag, const std::string& race, const std::string& id, bool terrain, const std::string& movement_sound, const std::string& src, config& cfg)
{
	utils::string_map symbols;
	std::string move1 = get_binary_file_location("images", move(race, id, terrain, 1));
	if (!move1.empty()) {
		symbols["move_1_png"] = move(race, id, terrain, 1);
		symbols["move_2_png"] = move(race, id, terrain, 2);
	} else {
		symbols["move_1_png"] = image(race, id, terrain);
		symbols["move_2_png"] = image(race, id, terrain);
	}
	symbols["sound_ogg"] = movement_sound;

	replace_anim_cfg(tag, src, cfg, symbols);
}

void unit_type::build_anim(const std::string& tag, const std::string& race, const std::string& id, bool terrain, config& cfg)
{
	utils::string_map symbols;
	std::string build1 = get_binary_file_location("images", build(race, id, terrain, 1));
	if (!build1.empty()) {
		symbols["build_1_png"] = build(race, id, terrain, 1);
		symbols["build_2_png"] = build(race, id, terrain, 2);
		symbols["build_3_png"] = build(race, id, terrain, 3);
		symbols["build_4_png"] = build(race, id, terrain, 4);
	} else {
		symbols["build_1_png"] = miss(race, id, terrain);
		symbols["build_2_png"] = attack_image(race, id, terrain, 1, 1);
		symbols["build_3_png"] = attack_image(race, id, terrain, 1, 2);
		symbols["build_4_png"] = attack_image(race, id, terrain, 1, 3);
	}

	replace_anim_cfg(tag, "build", cfg, symbols);
}

void unit_type::repair_anim(const std::string& tag, const std::string& race, const std::string& id, bool terrain, config& cfg)
{
	utils::string_map symbols;
	std::string repair1 = get_binary_file_location("images", build(race, id, terrain, 1));
	if (!repair1.empty()) {
		symbols["repair_1_png"] = repair(race, id, terrain, 1);
		symbols["repair_2_png"] = repair(race, id, terrain, 2);
		symbols["repair_3_png"] = repair(race, id, terrain, 3);
		symbols["repair_4_png"] = repair(race, id, terrain, 4);
	} else {
		symbols["repair_1_png"] = miss(race, id, terrain);
		symbols["repair_2_png"] = attack_image(race, id, terrain, 1, 1);
		symbols["repair_3_png"] = attack_image(race, id, terrain, 1, 2);
		symbols["repair_4_png"] = attack_image(race, id, terrain, 1, 3);
	}

	replace_anim_cfg(tag, "repair", cfg, symbols);
}

void unit_type::die_anim(const std::string& tag, const std::string& race, const std::string& id, bool terrain, const std::string& die_sound, config& cfg, bool multigrid)
{
	utils::string_map symbols;
	std::string die1 = get_binary_file_location("images", die(race, id, terrain, 1));
	if (!die1.empty()) {
		symbols["die_1_png"] = die(race, id, terrain, 1);
		symbols["die_2_png"] = die(race, id, terrain, 2);
	} else {
		symbols["die_1_png"] = image(race, id, terrain);
		symbols["die_2_png"] = image(race, id, terrain);
	}
	symbols["sound_ogg"] = die_sound;

	const std::string id_in_cfg = multigrid? "multi_die": "die";
	replace_anim_cfg(tag, id_in_cfg, cfg, symbols);
}

void unit_type::attack_anim_melee(const std::string& tag, const std::string& aid, const std::string& aicon, bool troop, const std::string& race, const std::string& id, bool terrain, config& cfg)
{
	std::string hit_sound = "sword-1.ogg"; // {SOUND_LIST:SWORD_SWISH}
	std::string miss_sound = "miss-1.ogg,miss-2.ogg,miss-3.ogg"; // {SOUND_LIST:MISS}
	if (aicon.find("staff") != std::string::npos) {
		hit_sound = "staff.wav";
	}

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

	std::string melee_attack_1 = get_binary_file_location("images", attack_image(race, id, terrain, 0, 1));
	if (!melee_attack_1.empty()) {
		symbols["melee_attack_1_png"] = attack_image(race, id, terrain, 0, 1);
		symbols["melee_attack_2_png"] = attack_image(race, id, terrain, 0, 2);
		symbols["melee_attack_3_png"] = attack_image(race, id, terrain, 0, 3);
		symbols["melee_attack_4_png"] = attack_image(race, id, terrain, 0, 4);
	} else {
		symbols["melee_attack_1_png"] = attack_image(race, id, terrain, 1, 1);
		symbols["melee_attack_2_png"] = attack_image(race, id, terrain, 1, 2);
		symbols["melee_attack_3_png"] = attack_image(race, id, terrain, 1, 3);
		symbols["melee_attack_4_png"] = attack_image(race, id, terrain, 1, 4);
	}

	if (!troop) {
		symbols["offset_x"] = "0";
		symbols["offset_y"] = "0";
	}

	replace_anim_cfg(tag, "melee_attack", cfg, symbols);
}

void unit_type::attack_anim_multi_melee(const std::string& tag, const std::string& aid, const std::string& aicon, bool troop, const std::string& race, const std::string& id, bool terrain, config& cfg)
{
	std::string hit_sound = "sword-1.ogg"; // {SOUND_LIST:SWORD_SWISH}
	std::string miss_sound = "miss-1.ogg,miss-2.ogg,miss-3.ogg"; // {SOUND_LIST:MISS}
	if (aicon.find("staff") != std::string::npos) {
		hit_sound = "staff.wav";
	}

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

	std::string melee_attack_1 = get_binary_file_location("images", attack_image(race, id, terrain, 0, 1));
	if (!melee_attack_1.empty()) {
		symbols["melee_attack_1_png"] = attack_image(race, id, terrain, 0, 1);
		symbols["melee_attack_2_png"] = attack_image(race, id, terrain, 0, 2);
		symbols["melee_attack_3_png"] = attack_image(race, id, terrain, 0, 3);
		symbols["melee_attack_4_png"] = attack_image(race, id, terrain, 0, 4);
	} else {
		symbols["melee_attack_1_png"] = attack_image(race, id, terrain, 1, 1);
		symbols["melee_attack_2_png"] = attack_image(race, id, terrain, 1, 2);
		symbols["melee_attack_3_png"] = attack_image(race, id, terrain, 1, 3);
		symbols["melee_attack_4_png"] = attack_image(race, id, terrain, 1, 4);
	}

	if (!troop) {
		symbols["offset_x"] = "0";
		symbols["offset_y"] = "0";
	}

	replace_anim_cfg(tag, "multi_melee_attack", cfg, symbols);
}

void unit_type::attack_anim_cyclone(const std::string& tag, const std::string& aid, const std::string& race, const std::string& id, bool terrain, config& cfg)
{
	std::string hit_sound = "sword-1.ogg"; // {SOUND_LIST:SWORD_SWISH}
	// if (aicon.find("staff") != std::string::npos) {
	//	hit_sound = "staff.wav";
	// }

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

	std::string melee_attack_1 = get_binary_file_location("images", attack_image(race, id, terrain, 0, 1));
	if (!melee_attack_1.empty()) {
		symbols["melee_attack_1_png"] = attack_image(race, id, terrain, 0, 1);
		symbols["melee_attack_2_png"] = attack_image(race, id, terrain, 0, 2);
		symbols["melee_attack_3_png"] = attack_image(race, id, terrain, 0, 3);
		symbols["melee_attack_4_png"] = attack_image(race, id, terrain, 0, 4);
	} else {
		symbols["melee_attack_1_png"] = attack_image(race, id, terrain, 1, 1);
		symbols["melee_attack_2_png"] = attack_image(race, id, terrain, 1, 2);
		symbols["melee_attack_3_png"] = attack_image(race, id, terrain, 1, 3);
		symbols["melee_attack_4_png"] = attack_image(race, id, terrain, 1, 4);
	}

	replace_anim_cfg(tag, "cyclone_attack", cfg, symbols);
}

void unit_type::attack_anim_penetrate(const std::string& tag, const std::string& aid, const std::string& race, const std::string& id, bool terrain, config& cfg)
{
	std::string hit_sound = "sword-1.ogg"; // {SOUND_LIST:SWORD_SWISH}

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

	std::string melee_attack_1 = get_binary_file_location("images", attack_image(race, id, terrain, 0, 1));
	if (!melee_attack_1.empty()) {
		symbols["melee_attack_1_png"] = attack_image(race, id, terrain, 0, 1);
		symbols["melee_attack_2_png"] = attack_image(race, id, terrain, 0, 2);
		symbols["melee_attack_3_png"] = attack_image(race, id, terrain, 0, 3);
		symbols["melee_attack_4_png"] = attack_image(race, id, terrain, 0, 4);
	} else {
		symbols["melee_attack_1_png"] = attack_image(race, id, terrain, 1, 1);
		symbols["melee_attack_2_png"] = attack_image(race, id, terrain, 1, 2);
		symbols["melee_attack_3_png"] = attack_image(race, id, terrain, 1, 3);
		symbols["melee_attack_4_png"] = attack_image(race, id, terrain, 1, 4);
	}

	replace_anim_cfg(tag, "penetrate_attack", cfg, symbols);
}

void unit_type::attack_anim_ranged(const std::string& tag, const std::string& aid, const std::string& aicon, const std::string& race, const std::string& id, bool terrain, config& cfg)
{
	std::string image = "projectiles/missile-n.png";
	std::string image_diagonal = "projectiles/missile-ne.png";
	std::string image_horizontal = "projectiles/missile-hrl.png";
	std::string hit_sound = "bow.ogg";
	std::string miss_sound = "bow-miss.ogg";
	if (aicon.find("sling") != std::string::npos) {
		image = "projectiles/stone-large.png";
		image_diagonal = "";
	}

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
	symbols["image_horizontal_png"] = image_horizontal;
	symbols["hit_sound"] = hit_sound;
	symbols["miss_sound"] = miss_sound;
	symbols["ranged_attack_1_png"] = attack_image(race, id, terrain, 1, 1);
	symbols["ranged_attack_2_png"] = attack_image(race, id, terrain, 1, 2);
	symbols["ranged_attack_3_png"] = attack_image(race, id, terrain, 1, 3);
	symbols["ranged_attack_4_png"] = attack_image(race, id, terrain, 1, 4);

	replace_anim_cfg(tag, "ranged_attack", cfg, symbols);
}

void unit_type::attack_anim_multi_ranged(const std::string& tag, const std::string& aid, const std::string& aicon, const std::string& race, const std::string& id, bool terrain, config& cfg)
{
	std::string image = "projectiles/missile-n.png";
	std::string image_diagonal = "projectiles/missile-ne.png";
	std::string hit_sound = "bow.ogg";
	std::string miss_sound = "bow-miss.ogg";
	if (aicon.find("sling") != std::string::npos) {
		image = "projectiles/stone-large.png";
		image_diagonal = "";
	}

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

	replace_anim_cfg(tag, "multi_ranged_attack", cfg, symbols);
}

void unit_type::attack_anim_ranged_magic_missile(const std::string& tag, const std::string& aid, const std::string& race, const std::string& id, bool terrain, config& cfg)
{
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

	replace_anim_cfg(tag, "magic_missile_attack", cfg, symbols);
}

void unit_type::attack_anim_ranged_lightbeam(const std::string& tag, const std::string& aid, const std::string& race, const std::string& id, bool terrain, config& cfg)
{
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

	replace_anim_cfg(tag, "lightbeam_attack", cfg, symbols);
}

void unit_type::attack_anim_ranged_fireball(const std::string& tag, const std::string& aid, const std::string& race, const std::string& id, bool terrain, config& cfg)
{
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

	replace_anim_cfg(tag, "fireball_attack", cfg, symbols);
}

void unit_type::attack_anim_ranged_iceball(const std::string& tag, const std::string& aid, const std::string& race, const std::string& id, bool terrain, config& cfg)
{
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

	replace_anim_cfg(tag, "iceball_attack", cfg, symbols);
}

void unit_type::attack_anim_ranged_lightning(const std::string& tag, const std::string& aid, const std::string& race, const std::string& id, bool terrain, config& cfg)
{
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

	replace_anim_cfg(tag, "lightning_attack", cfg, symbols);
}

void unit_type::attack_anim_ranged_archery(const std::string& tag, const std::string& aid, const std::string& race, const std::string& id, bool terrain, config& cfg)
{
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

	replace_anim_cfg(tag, "archery_attack", cfg, symbols);
}

unit_type::unit_type(const unit_type& o) :
	cfg_(o.cfg_),
	id_(o.id_),
	type_name_(o.type_name_),
	description_(o.description_),
	hitpoints_(o.hitpoints_),
	level_(o.level_),
	movement_(o.movement_),
	max_movement_(o.max_movement_),
	max_attacks_(o.max_attacks_),
	cost_(o.cost_),
	gold_income_(o.gold_income_),
	technology_income_(o.technology_income_),
	food_income_(o.food_income_),
	miss_income_(o.miss_income_),
	heal_(o.heal_),
	turn_experience_(o.turn_experience_),
	halo_(o.halo_),
	undead_variation_(o.undead_variation_),
	image_(o.image_),
	weak_image_(o.weak_image_),
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
	require_(o.require_),
	can_ranges_(o.can_ranges_),
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
	raw_icon_(o.raw_icon_),
	raw_movement_sound_(o.raw_movement_sound_),
	raw_idle_sound_(o.raw_idle_sound_),
	terrain_(o.terrain_),
	can_recruit_(o.can_recruit_),
	can_reside_(o.can_reside_),
	base_(o.base_),
	wall_(o.wall_),
	fort_(o.fort_),
	transport_(o.transport_),
	land_wall_(o.land_wall_),
	walk_wall_(o.walk_wall_),
	arms_(o.arms_),
	especial_(o.especial_),
	master_(o.master_),
	guard_(o.guard_),
	touch_dirs_(o.touch_dirs_),
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


unit_type::unit_type(const config &cfg) :
	cfg_(),
	id_(cfg["id"]),
	type_name_(),
	description_(),
	hitpoints_(0),
	level_(0),
	movement_(0),
	max_movement_(-1),
	max_attacks_(0),
	cost_(0),
	gold_income_(0),
	technology_income_(0),
	food_income_(0),
	miss_income_(0),
	heal_(0),
	turn_experience_(0),
	halo_(),
	undead_variation_(),
	image_(),
	weak_image_(),
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
	require_(REQUIRE_NONE),
	can_ranges_(0),
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
	raw_icon_(),
	raw_movement_sound_(),
	raw_idle_sound_(),
	terrain_(t_translation::NONE_TERRAIN),
	can_recruit_(false),
	can_reside_(false),
	base_(false),
	wall_(false),
	fort_(false),
	transport_(false),
	land_wall_(true),
	walk_wall_(false),
	arms_(0),
	especial_(-1),
	master_(HEROS_INVALID_NUMBER),
	guard_(NO_GUARD),
	touch_dirs_(),
	packer_(false)
{
	BOOST_FOREACH (const config::any_child& c, cfg.all_children_range()) {
		if (c.key == "attack") continue;
		cfg_.add_child(c.key, c.cfg);
	}

	BOOST_FOREACH (const config &att, cfg.child_range("attack")) {
		attacks_.push_back(attack_type(att));
		if (attacks_.size() == 3) {
			break;
		}
		const attack_type& a = attacks_.back();
		int range = unit_types.range_from_id(a.range());
		if (range < 0) {
			throw config::error("Invalid range found for " + id_ + ": '" + a.range());
		}
		if (a.damage() > game_config::min_valid_damage) {
			can_ranges_ |= 1 << range;
		}
	}

	// AI need this flag, may be before build_all status. duraton ai's interior
	master_ = cfg["master"].to_int(HEROS_INVALID_NUMBER);
	wall_ = cfg["wall"].to_bool();
	fort_ = master_ == hero::number_fort;
	transport_ = master_ == hero::number_transport;
	walk_wall_ = cfg["walk_wall"].to_bool();
	int income = cfg["income"].to_int();
	if (master_ == hero::number_market || master_ == hero::number_businessman) {
		gold_income_ = income;
		technology_income_ = 0;
		food_income_ = 0;
		miss_income_ = 0;
	} else if (master_ == hero::number_technology || master_ == hero::number_scholar) {
		gold_income_ = 0;
		technology_income_ = income;
		food_income_ = 0;
		miss_income_ = 0;
	} else if (master_ == hero::number_transport) {
		gold_income_ = 0;
		technology_income_ = 0;
		food_income_ = income;
		miss_income_ = 0;
	} else if (master_ == hero::number_tactic) {
		gold_income_ = 0;
		technology_income_ = 0;
		food_income_ = 0;
		miss_income_ = income;
	} else if (master_ == hero::number_school) {
		gold_income_ = 0;
		technology_income_ = 0;
		food_income_ = 0;
		miss_income_ = 0;
	}
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
			BOOST_FOREACH (const config &t, race_->additional_traits())
			{
				if (alignment_ != NEUTRAL || t["id"] != "fearless")
					possibleTraits_.add_child("trait", t);
			}
		}
	}
*/
	zoc_ = cfg["zoc"].to_bool(level_ > 0);
	cancel_zoc_ = cfg["cancel_zoc"].to_bool();
	require_ = cfg["require"].to_int(REQUIRE_NONE);

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
	if (!cfg["especial"].blank()) {
		especial_ = unit_types.especial_from_id(cfg["especial"].str());
		if (especial_ < 0) {
			throw config::error(id_ + "'s especial is invalid: " + cfg["especial"].str());
		}
	}
	raw_icon_ = cfg["icon"].str();
	raw_movement_sound_ = cfg["movement_sound"].str();
	raw_idle_sound_ = cfg["idle_sound"].str();
	
	land_wall_ = cfg["land_wall"].to_bool(true);
	guard_ = cfg["guard"].to_int(NO_GUARD);

	const std::vector<std::string> vstr = utils::split(cfg["touch_dirs"]);
	std::vector<std::string>::const_iterator tmp;
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		map_location::DIRECTION dir = map_location::parse_direction(*it);
		if (dir != map_location::NDIRECTIONS) {
			touch_dirs_.insert(dir);
		}
	}

	// Deprecation messages, only seen when unit is parsed for the first time.

	build_status_ = FULL;
}

bool unit_type::terrain_matches(t_translation::t_terrain tcode) const
{
	t_translation::t_list list = t_translation::t_match(match_).terrain;
	return list.empty() || t_translation::terrain_matches(tcode, list);
}

std::string unit_type::icon() const
{
	if (!raw_icon_.empty()) {
		return std::string("utype/") + raw_icon_;
	} else if (especial_ != NO_ESPECIAL) {
		return std::string("utype/") + unit_types.especial(especial_).id_ + ".png";
	}
	return null_str;
}

std::string unit_type::movement_sound() const
{
	if (!raw_movement_sound_.empty()) {
		return std::string("movement/") + raw_movement_sound_;
	} else {
		return "movement/default.wav";
	}
}

std::string unit_type::idle_sound() const
{
	if (!raw_idle_sound_.empty()) {
		return std::string("idle/") + raw_idle_sound_;
	} else {
		return null_str;
	}
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
		BOOST_FOREACH (const config::any_child &c, result->second.all_children_range()) {
			// abilities_cfg.add_child(c.key, c.cfg);
			abilities_cfg_.insert(std::make_pair(c.key, &c.cfg));
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
	movement_ = cfg["movement"].to_int(0);
	max_movement_ = cfg["max_movement"].to_int(-1);
	max_attacks_ = cfg["attacks"].to_int(1);
	cost_ = cfg["cost"].to_int(1);
	halo_ = cfg["halo"].str();
	undead_variation_ = cfg["undead_variation"].str();
	image_ = cfg["image"].str();
	weak_image_ = cfg["weak_image"].str();
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
		unit_type::defend_anim("defend", race_->id(), id_, use_terrain_image(), utype_cfg);
	}

	if (has_resistance_anim(abilities)) {
		unit_type::resistance_anim("resistance_anim", race_->id(), id_, use_terrain_image(), utype_cfg);
	}
	if (has_leading_anim(abilities)) {
		unit_type::leading_anim("leading_anim", race_->id(), id_, use_terrain_image(), utype_cfg);
	}
	if (has_healing_anim(abilities)) {
		unit_type::healing_anim("healing_anim", race_->id(), id_, use_terrain_image(), utype_cfg);
	}

	unit_type::idle_anim("idle_anim", race_->id(), id_, use_terrain_image(), idle_sound(), utype_cfg, can_reside_);
	unit_type::healed_anim("healed_anim", race_->id(), id_, use_terrain_image(), utype_cfg);

	if (!packer_) {
		if ((master_ == HEROS_INVALID_NUMBER && !can_reside_) || hero::is_commoner(master_)) {
			unit_type::movement_anim("movement_anim", race_->id(), id_, use_terrain_image(), movement_sound(), "movement", utype_cfg);
		}
	} else {
		unit_type::movement_anim("movement_anim", race_->id(), id_, use_terrain_image(), movement_sound(), "navy_movement", utype_cfg);
	}

	if (hero::is_artifical(master_)) {
		unit_type::build_anim("build_anim", race_->id(), id_, use_terrain_image(), utype_cfg);
		unit_type::repair_anim("repair_anim", race_->id(), id_, use_terrain_image(), utype_cfg);
	}

	unit_type::die_anim("death", race_->id(), id_, use_terrain_image(), die_sound_, utype_cfg, can_reside_);

	if (!packer_) {
		for (std::vector<attack_type>::const_iterator it = attacks_.begin(); it != attacks_.end(); ++ it) {
			if (it->range() == "melee") {
				bool not_artifical_master = (master_ == HEROS_INVALID_NUMBER) || hero::is_commoner(master_);
				if (it->specials().find("cyclone") != std::string::npos) {
					unit_type::attack_anim_cyclone("attack_anim", it->id(), race_->id(), id_, use_terrain_image(), utype_cfg);
				} else if (it->specials().find("penetrate") != std::string::npos) {
					unit_type::attack_anim_penetrate("attack_anim", it->id(), race_->id(), id_, use_terrain_image(), utype_cfg);
				} else if (!can_reside_) {
					unit_type::attack_anim_melee("attack_anim", it->id(), it->icon(), not_artifical_master && !can_recruit_, race_->id(), id_, use_terrain_image(), utype_cfg);
				} else {
					unit_type::attack_anim_multi_melee("attack_anim", it->id(), it->icon(), not_artifical_master && !can_recruit_, race_->id(), id_, use_terrain_image(), utype_cfg);
				}

			} else if (can_reside_) {
				unit_type::attack_anim_multi_ranged("attack_anim", it->id(), it->icon(), race_->id(), id_, use_terrain_image(), utype_cfg);

			} else if (it->icon().find("magic-missile") != std::string::npos) {
				unit_type::attack_anim_ranged_magic_missile("attack_anim", it->id(), race_->id(), id_, use_terrain_image(), utype_cfg);

			} else if (it->icon().find("lightbeam") != std::string::npos) {
				unit_type::attack_anim_ranged_lightbeam("attack_anim", it->id(), race_->id(), id_, use_terrain_image(), utype_cfg);

			} else if (it->icon().find("fireball") != std::string::npos) {
				unit_type::attack_anim_ranged_fireball("attack_anim", it->id(), race_->id(), id_, use_terrain_image(), utype_cfg);

			} else if (it->icon().find("iceball") != std::string::npos) {
				unit_type::attack_anim_ranged_iceball("attack_anim", it->id(), race_->id(), id_, use_terrain_image(), utype_cfg);

			} else if (it->icon().find("lightning") != std::string::npos) {
				unit_type::attack_anim_ranged_lightning("attack_anim", it->id(), race_->id(), id_, use_terrain_image(), utype_cfg);

			} else if (it->type() == "archery" && it->specials().find("ringattack") != std::string::npos) {
				unit_type::attack_anim_ranged_archery("attack_anim", it->id(), race_->id(), id_, use_terrain_image(), utype_cfg);

			} else {
				unit_type::attack_anim_ranged("attack_anim", it->id(), it->icon(), race_->id(), id_, use_terrain_image(), utype_cfg);
			}
		}
	} else {
		const std::vector<std::string>& range_ids = unit_types.range_ids();

		unit_type::attack_anim_melee("attack_anim", range_ids[0], "staff-magic.png", true, race_->id(), id_, use_terrain_image(), utype_cfg);

		unit_type::attack_anim_ranged_magic_missile("attack_anim", range_ids[1], race_->id(), id_, use_terrain_image(), utype_cfg);

		unit_type::attack_anim_ranged("attack_anim", range_ids[2], "sling.png", race_->id(), id_, use_terrain_image(), utype_cfg);
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

bool unit_type::can_range(int range) const
{
	if (range < 0 || range >= HEROS_MAX_RANGE) {
		return false;
	}
	return can_ranges_ & (range << 1);
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
	BOOST_FOREACH (const std::string& adv, vstr) {
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

std::vector<std::string> unit_type::advances_to(int spe) const
{
	std::vector<std::string> vstr;
	for (std::vector<std::string>::const_iterator it = advances_to_.begin(); it != advances_to_.end(); ++ it) {
		int adv = unit_types.find(*it)->especial();
		if (adv == -1 || adv == spe) {
			vstr.push_back(*it);
		}
	}
	return vstr;
}

const std::vector<std::string> unit_type::advances_from() const
{
	std::vector<std::string> adv_from;
	BOOST_FOREACH (const unit_type_data::unit_type_map::value_type &ut, unit_types.types())
	{
		std::vector<std::string> vstr = ut.second.advances_to();
		BOOST_FOREACH (const std::string& adv, vstr) {
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
	features_level_(),
	treasures_(),
	abilities_(),
	specials_(),
	tactics_(),
	tactics_id_(),
	characters_(),
	characters_id_(),
	decrees_(),
	decrees_id_(),
	genera_(),
	technologies_(),
	nobles_(),
	sort_nobles_(),
	leader_nobles_(),
	formations_(),
	formations_id_(),
	arms_ids_(),
	atype_ids_(),
	range_ids_(),
	especials_(),
	utype_anims_(),
	can_recruit_(),
	navigation_types_(),
	utype_tree_(),
	technology_tree_(),
	selectable_technologies_(),
	wall_type_(NULL),
	keep_type_(NULL),
	market_type_(NULL),
	technology_type_(NULL),
	tactic_type_(NULL),
	tower_type_(NULL),
	fort_type_(NULL),
	school_type_(NULL),
	city_type_(),
	businessman_type_(NULL),
	scholar_type_(NULL),
	transport_type_(NULL),
	scout_type_(NULL),
	hide_help_all_(false),
	hide_help_type_(),
	hide_help_race_(),
	build_status_(unit_type::NOT_BUILT)
{
}

unit_type_data::~unit_type_data()
{
	for (std::vector<advance_tree::node*>::iterator it = utype_tree_.begin(); it != utype_tree_.end(); ++ it) {
		delete *it;
	}
	for (std::vector<advance_tree::node*>::iterator it = technology_tree_.begin(); it != technology_tree_.end(); ++ it) {
		delete *it;
	}
}

std::string mode_desc(mode_tag::tmode tag)
{
	if (tag == mode_tag::SCENARIO) {
		return dsgettext("wesnoth-lib", "mode^Scenario");
	} else if (tag == mode_tag::RPG) {
		return dsgettext("wesnoth-lib", "mode^RPG");
	} else if (tag == mode_tag::TOWER) {
		return dsgettext("wesnoth-lib", "mode^Tower");
	} else if (tag == mode_tag::SIEGE) {
		return dsgettext("wesnoth-lib", "mode^Siege");
	}
	return "Unknown";
}

namespace mode_tag {
std::map<const std::string, tmode> tags;

void fill_tags()
{
	if (!tags.empty()) return;
	tags.insert(std::make_pair("scenario", SCENARIO));
	tags.insert(std::make_pair("rpg", RPG));
	tags.insert(std::make_pair("tower", TOWER));
	tags.insert(std::make_pair("siege", SIEGE));
	tags.insert(std::make_pair("layout", LAYOUT));
}

tmode find(const std::string& tag)
{
	std::map<const std::string, tmode>::const_iterator it = tags.find(tag);
	if (it != tags.end()) {
		return it->second;
	}
	return NONE;
}

const std::string& rfind(tmode tag)
{
	for (std::map<const std::string, tmode>::const_iterator it = tags.begin(); it != tags.end(); ++ it) {
		if (it->second == tag) {
			return it->first;
		}
	}
	return null_str;
}

}

namespace controller_tag {
std::map<const std::string, CONTROLLER> tags;
std::set<CONTROLLER> profile_side_tags;
std::set<CONTROLLER> game_running_tags;

void fill_tags()
{
	if (!tags.empty()) return;
	tags.insert(std::make_pair("human", HUMAN));
	tags.insert(std::make_pair("human_ai", HUMAN_AI));
	tags.insert(std::make_pair("ai", AI));
	tags.insert(std::make_pair("network", NETWORK));
	tags.insert(std::make_pair("null", EMPTY));

	profile_side_tags.insert(HUMAN);
	profile_side_tags.insert(HUMAN_AI);
	profile_side_tags.insert(AI);
	profile_side_tags.insert(EMPTY);

	game_running_tags.insert(HUMAN);
	game_running_tags.insert(AI);
	game_running_tags.insert(EMPTY);
}

CONTROLLER find(const std::string& tag)
{
	std::map<const std::string, CONTROLLER>::const_iterator it = tags.find(tag);
	if (it != tags.end()) {
		return it->second;
	}
	return NONE;
}

const std::string& rfind(CONTROLLER tag)
{
	for (std::map<const std::string, CONTROLLER>::const_iterator it = tags.begin(); it != tags.end(); ++ it) {
		if (it->second == tag) {
			return it->first;
		}
	}
	return null_str;
}

bool valid(const std::string& str)
{
	if (str.empty()) {
		return false;
	}

	std::set<int> ret;
	const std::vector<std::string>& vals = utils::split(str);
	for (std::vector<std::string>::const_iterator it = vals.begin(); it != vals.end(); ++ it) {
		int type = find(*it);
		if (type != NONE) {
			ret.insert(find(*it));
		}
	}
	return !ret.empty();
}

bool filter(int controller, const std::string& filters_str)
{
	if (filters_str.empty()) {
		return true;
	}

	int filters = 0;
	const std::vector<std::string>& vals = utils::split(filters_str);
	for (std::vector<std::string>::const_iterator it = vals.begin(); it != vals.end(); ++ it) {
		int type = find(*it);
		if (type != NONE) {
			filters |= type;
		}
	}

	return filter(controller, filters);
}

bool filter(int controller, int filters)
{
	if (!filters) {
		return true;
	}

	return !!(filters & controller);
}

}

namespace apply_to_tag {
std::map<const std::string, int> tags;

void fill_tags()
{
	if (!tags.empty()) return;

	tags.insert(std::make_pair("attack", (int)ATTACK));
	tags.insert(std::make_pair("hitpoints", (int)HITPOINTS));
	tags.insert(std::make_pair("movement", (int)MOVEMENT));
	tags.insert(std::make_pair("munition", (int)MUNITION));
	tags.insert(std::make_pair("max_experience", (int)MAX_EXPERIENCE));
	tags.insert(std::make_pair("loyal", (int)LOYAL));
	tags.insert(std::make_pair("states", (int)STATUS));
	tags.insert(std::make_pair("movement_costs", (int)MOVEMENT_COSTS));
	tags.insert(std::make_pair("defense", (int)DEFENSE));
	tags.insert(std::make_pair("resistance", (int)RESISTANCE));
	tags.insert(std::make_pair("encourage", (int)ENCOURAGE));
	tags.insert(std::make_pair("demolish", (int)DEMOLISH));
	tags.insert(std::make_pair("zoc", (int)ZOC));
	tags.insert(std::make_pair("image_mod", (int)IMAGE_MOD));
	tags.insert(std::make_pair("advance", (int)ADVANCE));
	tags.insert(std::make_pair("train", (int)TRAIN));
	tags.insert(std::make_pair("move", (int)MOVE));
	tags.insert(std::make_pair("damage", (int)DAMAGE));
	tags.insert(std::make_pair("hide", (int)HIDE));
	tags.insert(std::make_pair("alert", (int)ALERT));
	tags.insert(std::make_pair("provoke", (int)PROVOKE));
	tags.insert(std::make_pair("clear", (int)CLEAR));
	tags.insert(std::make_pair("heal", (int)HEAL));
	tags.insert(std::make_pair("action", (int)ACTION));
	tags.insert(std::make_pair("revival", (int)REVIVAL));
	tags.insert(std::make_pair("decree", (int)DECREE));

	// side <=
	tags.insert(std::make_pair("civilization", (int)CIVILIZATION));
	tags.insert(std::make_pair("spirit", (int)SPIRIT));
	tags.insert(std::make_pair("strategic", (int)STRATEGIC));
	tags.insert(std::make_pair("stratagem", (int)STRATAGEM));

	// character <=
	tags.insert(std::make_pair("aggressive", (int)AGGRESSIVE));
	tags.insert(std::make_pair("united", (int)UNITED));
	tags.insert(std::make_pair("charismatic", (int)CHARISMATIC));
	tags.insert(std::make_pair("creative", (int)CREATIVE));
	tags.insert(std::make_pair("expansive", (int)EXPANSIVE));
	tags.insert(std::make_pair("financial", (int)FINANCIAL));
	tags.insert(std::make_pair("industrious", (int)INDUSTRIOUS));
	tags.insert(std::make_pair("organized", (int)ORGANIZED));
	tags.insert(std::make_pair("protective", (int)PROTECTIVE));
	tags.insert(std::make_pair("philosophical", (int)PHILOSOPHICAL));
	tags.insert(std::make_pair("spiritual", (int)SPIRITUAL));

	// decree
	tags.insert(std::make_pair("business", (int)BUSINESS));
	tags.insert(std::make_pair("technology", (int)TECH));
}

int find(const std::string& tag) 
{
	std::map<const std::string, int>::const_iterator it = tags.find(tag);
	if (it != tags.end()) {
		return it->second;
	}
	return NONE;
}

}

namespace stratagem_tag {

std::map<const std::string, int> tags;
std::map<int, const ttechnology*> technologies;

void clear()
{
	tags.clear();
	technologies.clear();
}

void fill_stratagem(const ttechnology& t, const std::string& stratagem)
{
	int tag;
	if (stratagem == "xue zhong song tan") {
		tag = xue_zhong_song_tan;

	} else if (stratagem == "miao shou hui chun") {
		tag = miao_shou_hui_chun;

	} else if (stratagem == "xiao li cang dao") {
		tag = xiao_li_cang_dao;

	} else if (stratagem == "sheng dong ji xi") {
		tag = sheng_dong_ji_xi;

	} else if (stratagem == "jiang lang cai jin") {
		tag = jiang_lang_cai_jin;

	} else {
		throw config::error("[technology] error, stragagem has invalid id: " + stratagem);
	}
	tags.insert(std::make_pair(stratagem, tag));
	technologies.insert(std::make_pair(tag, &t));
}

int find(const std::string& tag)
{
	std::map<const std::string, int>::const_iterator it = tags.find(tag);
	if (it != tags.end()) {
		return it->second;
	}
	return none;
}

const ttechnology& technology(int tag)
{
	std::map<int, const ttechnology*>::const_iterator it = technologies.find(tag);
	if (it != technologies.end()) {
		return *it->second;
	}
	return null_technology;
}

}

namespace family_tag {
std::map<const std::string, int> tags;

void fill_tags()
{
	if (!tags.empty()) return;
	tags.insert(std::make_pair("troop", (int)TROOP));
	tags.insert(std::make_pair("artifical", (int)ARTIFICAL));
	tags.insert(std::make_pair("city", (int)CITY));
}

int find(const std::string& tag)
{
	std::map<const std::string, int>::const_iterator it = tags.find(tag);
	if (it != tags.end()) {
		return it->second;
	}
	return NONE;
}

const std::string& rfind(int tag)
{
	for (std::map<const std::string, int>::const_iterator it = tags.begin(); it != tags.end(); ++ it) {
		if (it->second == tag) {
			return it->first;
		}
	}
	return null_str;
}

bool valid(const std::string& str)
{
	if (str.empty()) {
		return false;
	}

	std::set<int> ret;
	const std::vector<std::string>& vals = utils::split(str);
	for (std::vector<std::string>::const_iterator it = vals.begin(); it != vals.end(); ++ it) {
		int id = find(*it);
		if (id != NONE) {
			ret.insert(id);
		}
	}
	return !ret.empty();
}

bool filter(const unit& u, const std::string& filters_str)
{
	if (filters_str.empty()) {
		return true;
	}

	int filters = 0;
	const std::vector<std::string>& vals = utils::split(filters_str);
	for (std::vector<std::string>::const_iterator it = vals.begin(); it != vals.end(); ++ it) {
		int id = find(*it);
		if (id != NONE) {
			filters |= id;
		}
	}

	return filter(u, filters);
}

bool filter(const unit& u, int filters)
{
	if (!filters) {
		return true;
	}

	int family = family_tag::ARTIFICAL;
	if (!u.is_artifical()) {
		family = family_tag::TROOP;
	} else if (u.is_city()) {
		family = family_tag::CITY;
	}

	return !!(filters & family);
}

}

namespace ustate_tag {
std::map<const std::string, state_t> tags;
std::set<ustate_tag::state_t> profilable_tags;

void fill_tags()
{
	if (!tags.empty()) return;
	tags.insert(std::make_pair("slowed", SLOWED));
	tags.insert(std::make_pair("broken", BROKEN));
	tags.insert(std::make_pair("poisoned", POISONED));
	tags.insert(std::make_pair("petrified", PETRIFIED));
	tags.insert(std::make_pair("uncovered", UNCOVERED));
	tags.insert(std::make_pair("formationed", FORMATION_ATTACKED));
	tags.insert(std::make_pair("legeritied", LEGERITIED));
	tags.insert(std::make_pair("expedited", EXPEDITED));
	tags.insert(std::make_pair("blocked", BLOCKED));
	tags.insert(std::make_pair("robber", ROBBER));
	tags.insert(std::make_pair("depute", DEPUTE));
	tags.insert(std::make_pair("building", BUILDING));
	tags.insert(std::make_pair("revivaled", REVIVALED));

	profilable_tags.insert(ROBBER);
}

state_t find(const std::string& tag)
{
	std::map<const std::string, state_t>::const_iterator it = tags.find(tag);
	if (it != tags.end()) {
		return it->second;
	}
	return NONE;
}

const std::string& rfind(state_t tag)
{
	for (std::map<const std::string, state_t>::const_iterator it = tags.begin(); it != tags.end(); ++ it) {
		if (it->second == tag) {
			return it->first;
		}
	}
	return null_str;
}

bool valid(const std::string& str)
{
	if (str.empty()) {
		return false;
	}

	std::set<int> ret;
	const std::vector<std::string>& vals = utils::split(str);
	for (std::vector<std::string>::const_iterator it = vals.begin(); it != vals.end(); ++ it) {
		int id = find(*it);
		if (id != NONE) {
			ret.insert(id);
		}
	}
	return !ret.empty();
}

}

void unit_type_data::set_config(const config &cfg)
{
    clear();

	tgenus::fill_tags();
	mode_tag::fill_tags();
	controller_tag::fill_tags();
	apply_to_tag::fill_tags();
	sound_filter_tag::fill_tags();
	family_tag::fill_tags();
	ustate_tag::fill_tags();
	help::fill_reserve_sections();

	std::stringstream err;

	BOOST_FOREACH (const config &mt, cfg.child_range("movetype"))
	{
		const unit_movement_type move_type(mt);
		movement_types_.insert(
			std::pair<std::string,unit_movement_type>(move_type.name(), move_type));
		loadscreen::increment_progress();
	}

	BOOST_FOREACH (const config &r, cfg.child_range("race"))
	{
		const unit_race race(r);
		races_.insert(std::pair<std::string,unit_race>(race.id(),race));
		loadscreen::increment_progress();
	}

	BOOST_FOREACH (const config &modification, cfg.child_range("modifications"))
	{
		BOOST_FOREACH (const config::any_child &c, modification.all_children_range()) {
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
	
	BOOST_FOREACH (const config &trait, cfg.child_range("traits"))
	{
		BOOST_FOREACH (const config::any_child &c, trait.all_children_range()) {
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

	const config& feature_cfg = cfg.child("feature");
	if (!feature_cfg) {
		throw config::error("unit_typ_data error, no [feature] tag!");
	}
	BOOST_FOREACH (const config::attribute &i, feature_cfg.attribute_range()) {
		if (i.first == "level") {
			std::vector<std::string> vstr = utils::split(i.second.str());
			for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
				int level = lexical_cast_default<int>(*it);
				if (level < 0 || level > 4) {
					throw config::error("[feature] tag error, invalid level: " + *it);
				}
				features_level_.push_back(level);
			}
		} else {
			int id = lexical_cast_default<int>(i.first, -1);
			if (id < HEROS_BASE_FEATURE_COUNT || id >= HEROS_MAX_FEATURE) {
				throw config::error("[feature] tag error, id=" + i.first + ", invalid id");
			}
			if (i.second.empty()) {
				throw config::error("[feature] tag error, id=" + i.first + ", no atomic feature");
			}
			const std::vector<std::string> features = utils::split(i.second.str());
			std::vector<int> v;
			for (std::vector<std::string>::const_iterator it2 = features.begin(); it2 != features.end(); ++ it2) {
				int feature = lexical_cast_default<int>(*it2, HEROS_BASE_FEATURE_COUNT);
				if (feature < 0 || feature >= HEROS_BASE_FEATURE_COUNT) {
					throw config::error("[feature] tag error, id=" + i.first + ", invalid atomic feature");
				}
				v.push_back(feature);
			}
			complex_feature_.insert(std::pair<int, std::vector<int> >(id, v));
		}
		loadscreen::increment_progress();
	}

	BOOST_FOREACH (const config &af, cfg.child_range("treasure"))
	{
		if (af["feature"].empty()) {
			throw config::error("treasure error, no feature attribute");
		}
		const std::vector<std::string> features = utils::split(af["feature"].str());
		if (features.size() >= HEROS_MAX_TREASURE) {
			throw config::error("treasure error, too more treasure, not support.");
		}
		int index = 0;
		for (std::vector<std::string>::const_iterator it = features.begin(); it != features.end(); ++ it, index ++) {
			int feature = lexical_cast_default<int>(*it);
			if (feature < 0 || feature >= HEROS_MAX_FEATURE) {
				err << "treasure error, " << index << "th's is invalid feature.";
				throw config::error(err.str());
			}
			treasures_.push_back(ttreasure(index, feature));
		}
		loadscreen::increment_progress();
	}

	BOOST_FOREACH (const config &ability, cfg.child_range("abilities"))
	{
		bool first = true;
		BOOST_FOREACH (const config::any_child &c, ability.all_children_range()) {
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

	BOOST_FOREACH (const config &anim, cfg.child_range("utype_anim"))
	{
		const std::string id = anim["id"].str();
		if (id.empty()) {
			throw config::error("[utype_anim] error, utype_anim must has id attribute");
		}
/*
		if (utype_anims_.find(id) != utype_anims_.end()) {
			throw config::error("[utype_anim] error, duplicate id: " + id);
		}
*/
		const config& sub = anim.child("anim");
		if (!sub) {
			throw config::error("[utype_anim] error, utype_anim must has [anim] tag");
		}
		utype_anims_.insert(std::make_pair(id, sub));

		loadscreen::increment_progress();
	}

	area_anim::fill_anims(cfg);

	BOOST_FOREACH (const config &sp, cfg.child_range("specials"))
	{
		BOOST_FOREACH (const config::any_child &c, sp.all_children_range()) {
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
	BOOST_FOREACH (const config &tactic, cfg.child_range("tactic"))
	{
		const std::string id = tactic["id"].str();
		if (id.empty()) {
			throw config::error("[tactic] error, no id attribute");
		}
		if (tactics_id_.find(id) != tactics_id_.end()) {
			throw config::error("[tactic] error, duplicate id " + id);
		}
		
		ttactic t(index, complex_index, tactic);
		std::pair<std::map<int, ttactic>::iterator, bool> ins = tactics_.insert(std::make_pair(t.index(), t));
		tactics_id_[id] = t.index();

		if (!t.complex()) {
			ins.first->second.set_atom_part();
			index ++;
		} else {
			complex_index ++;
		}
		
		loadscreen::increment_progress();
	}

	index = 0, complex_index = tcharacter::min_complex_index;
	BOOST_FOREACH (const config &character, cfg.child_range("character"))
	{
		const std::string id = character["id"].str();
		if (id.empty()) {
			throw config::error("[character] error, no id attribute");
		}
		if (characters_id_.find(id) != characters_id_.end()) {
			throw config::error("[character] error, duplicate id " + id);
		}
		
		tcharacter c(index, complex_index, character);
		std::pair<std::map<int, tcharacter>::iterator, bool> ins = characters_.insert(std::make_pair(c.index(), c));
		characters_id_[id] = c.index();

		if (!c.complex()) {
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

		// identifier of attack type
		if (id_cfg["atype"].empty()) {
			throw config::error("[identifier] error, no atype attribute");
		}
		ids = utils::split(id_cfg["atype"]);
		if (ids.empty()) {
			throw config::error("[identifier] error, size of atype cannot be 0.");
		}
		size = ids.size();
		for (size_t i = 0; i < size; i ++) {
			atype_ids_.push_back(ids[i]);
		}

		// identifier of range
		if (id_cfg["range"].empty()) {
			throw config::error("[identifier] error, no range attribute");
		}
		ids = utils::split(id_cfg["range"]);
		if (ids.size() != HEROS_MAX_RANGE) {
			throw config::error("[identifier] error, size of range value must is 3.");
		}
		size = ids.size();
		for (size_t i = 0; i < size; i ++) {
			range_ids_.push_back(ids[i]);
		}

		// identifier of character
		if (id_cfg["especial"].empty()) {
			throw config::error("[identifier] error, no especial attribute");
		}
		ids = utils::split(id_cfg["especial"]);
		size = ids.size();
		for (size_t i = 0; i < size; i ++) {
			especials_.push_back(tespecial(i, ids[i]));
		}

		// identifier of boat
		if (id_cfg["boat"].empty()) {
			throw config::error("[identifier] error, no boat attribute");
		}
		ids = utils::split(id_cfg["boat"]);
		size = ids.size();
		for (size_t i = 0; i < size; i ++) {
			navigation_types_.push_back(ids[i]);
		}

		// identifier of artifical
		if (id_cfg["utype"].empty()) {
			throw config::error("[identifier] error, no utype attribute");
		}
		ids = utils::parenthetical_split(id_cfg["utype"]);
		for (std::vector<std::string>::const_iterator it = ids.begin(); it != ids.end(); ++ it) {
			const std::vector<std::string>& vstr = utils::split(*it);
			if (vstr.size() != 2) {
				continue;
			}
			int number = lexical_cast_default<int>(vstr[1]);
			master_ids_.insert(std::make_pair(vstr[0], number));
		}

		loadscreen::increment_progress();
	} else {
		throw config::error("[identifier] error, must define [identifier] block in [units].");
	}

	// technology
	stratagem_tag::clear();
	BOOST_FOREACH (const config &tech, cfg.child_range("technology"))
	{
		const std::string id = tech["id"].str();
		if (id.empty()) {
			throw config::error("[technology] error, no id attribute");
		}
		if (technologies_.find(id) != technologies_.end()) {
			throw config::error("[technology] error, duplicate id " + id);
		}
		
		ttechnology t(tech);
		std::pair<std::map<std::string, ttechnology>::iterator, bool> ins = technologies_.insert(std::make_pair(t.id(), t));
		
		if (!t.complex()) {
			ttechnology& real = ins.first->second;
			real.set_atom_part();
			if (real.stratagem()) {
				stratagem_tag::fill_stratagem(real, real.effect_cfg()["stratagem"].str());
			}
		}
		
		loadscreen::increment_progress();
	}

	for (std::map<std::string, ttechnology>::const_iterator it = technologies_.begin(); it != technologies_.end(); ++ it) {
		const ttechnology& t = it->second;
		const std::vector<std::string>& advances_to = t.advances_to();
		for (std::vector<std::string>::const_iterator it2 = advances_to.begin(); it2 != advances_to.end(); ++ it2) {
			if (*it2 == it->first) {
				throw config::error("technology " + it->first + " advances_to error, cannot advance to myself.");
			}
			if (technologies_.find(*it2) == technologies_.end()) {
				throw config::error("technology " + it->first + " advances_to error, cannot find " + *it2 + " in technologies.");
			}
		}
	}

	// technology
	index = 0;
	BOOST_FOREACH (const config &n, cfg.child_range("noble"))
	{
		const std::string id = n["id"].str();
		if (id.empty()) {
			throw config::error("[noble] error, no id attribute");
		}
		for (std::map<int, std::set<tnoble> >::const_iterator it = nobles_.begin(); it != nobles_.end(); ++ it) {
			for (std::set<tnoble>::const_iterator it2 = it->second.begin(); it2 != it->second.end(); ++ it2) {
				if (it2->id() == id) {
					throw config::error("[noble] error, duplicate id " + id);
				}
			}
		}
		
		tnoble nbl(index ++, n);
		std::map<int, std::set<tnoble> >::iterator it = nobles_.find(nbl.level());
		if (it == nobles_.end()) {
			std::set<tnoble> level_nobles;
			level_nobles.insert(nbl);
			nobles_.insert(std::make_pair(nbl.level(), level_nobles));
		} else {
			it->second.insert(nbl);
		}

		loadscreen::increment_progress();
	}

	if (!nobles_.empty()) {
		std::map<int, const tnoble*> middle;
		int next_should_level = 0;
		for (std::map<int, std::set<tnoble> >::const_iterator it = nobles_.begin(); it != nobles_.end(); ++ it) {
			if (it->first != next_should_level) {
				throw config::error("[noble] error, not define level #" + str_cast(next_should_level) + " noble!");
			}
			for (std::set<tnoble>::const_iterator it2 = it->second.begin(); it2 != it->second.end(); ++ it2) {
				middle.insert(std::make_pair(it2->index(), &*it2));

				if (it2->leader()) {
					if (!leader_nobles_.empty() && leader_nobles_.back()->level() == it->first) {
						throw config::error("[noble] error, duplicate leader noble on level #" + str_cast(next_should_level) + " noble!");
					}
					leader_nobles_.push_back(&*it2);
				}
			}
			next_should_level ++;
		}

		for (std::map<int, const tnoble*>::const_iterator it = middle.begin(); it != middle.end(); ++ it) {
			sort_nobles_.push_back(it->second);
		}
				
		loadscreen::increment_progress();
	}

	index = 0;
	BOOST_FOREACH (const config &formation, cfg.child_range("formation"))
	{
		const std::string id = formation["id"].str();
		if (id.empty()) {
			throw config::error("[formation] error, no id attribute");
		}
		if (formations_id_.find(id) != formations_id_.end()) {
			throw config::error("[formation] error, duplicate id " + id);
		}
		
		tformation_profile f(index, formation);
		std::pair<std::map<int, tformation_profile>::iterator, bool> ins = formations_.insert(std::make_pair(f.index(), f));
		formations_id_[id] = f.index();

		index ++;
		loadscreen::increment_progress();
	}

	std::map<unit_type*, const config*> ut_cfg_map;
	BOOST_FOREACH (const config &ut, cfg.child_range("unit_type"))
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

	for (std::vector<std::string>::const_iterator it = navigation_types_.begin(); it != navigation_types_.end(); ++ it) {
		if (!find(*it)) {
			throw config::error("[identifier] error, invalid id: " + *it);
		}
	}

	generate_utype_tree();
	generate_technology_tree();
	
	BOOST_FOREACH (const config &ktype, cfg.child_range("keytype"))
	{
		BOOST_FOREACH (const config::attribute &i, ktype.attribute_range()) {
			int id = lexical_cast_default<int>(i.first, -1);
			if (id == -1) {
				throw config::error("keytype error, invalid keytype number.");
			}
			if (keytypes_.find(id) != keytypes_.end()) {
				throw config::error("keytype error, number=" + i.first + ", duplicate number.");
			}
			if (id >= HEROS_MAX_UTYPE) {
				throw config::error("keytype error, number=" + i.first + ", to lardge number.");
			}
			if (i.second.empty()) {
				throw config::error("keytype error, number=" + i.first + ", no type attribute");
			}
			const unit_type* ut = find(i.second);
			if (!ut) {
				throw config::error("keytype error, number=" + i.first + ", invalid type");
			}
			keytypes_.insert(std::make_pair(id, ut));
		}
		loadscreen::increment_progress();
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
		} else if (master == hero::number_technology) {
			if (!technology_type_ || technology_type_->level() > ut.level()) {
				technology_type_ = &ut;
				// build all, form other field
				find(it->first);
			}
		} else if (master == hero::number_tactic) {
			if (!tactic_type_ || tactic_type_->level() > ut.level()) {
				tactic_type_ = &ut;
				// build all, form other field
				find(it->first);
			}
		} else if (master == hero::number_tower) {
			if (!tower_type_ || tower_type_->level() > ut.level()) {
				tower_type_ = &ut;
				// build all, form other field
				find(it->first);
			}
		} else if (master == hero::number_fort) {
			if (!fort_type_ || fort_type_->level() > ut.level()) {
				fort_type_ = &ut;
				// build all, form other field
				find(it->first);
			}
		} else if (master == hero::number_school) {
			if (!school_type_ || school_type_->level() > ut.level()) {
				school_type_ = &ut;
				// build all, form other field
				find(it->first);
			}
		} else if (master == hero::number_businessman) {
			if (!businessman_type_ || businessman_type_->level() > ut.level()) {
				businessman_type_ = &ut;
				// build all, form other field
				find(it->first);
			}
		} else if (master == hero::number_scholar) {
			if (!scholar_type_ || scholar_type_->level() > ut.level()) {
				scholar_type_ = &ut;
				// build all, form other field
				find(it->first);
			}
		} else if (master == hero::number_transport) {
			if (!transport_type_ || transport_type_->level() > ut.level()) {
				transport_type_ = &ut;
				// build all, form other field
				find(it->first);
			}
		} else if (ut.id() == "scoutman") {
			if (!scout_type_ || scout_type_->level() > ut.level()) {
				scout_type_ = &ut;
				// build all, form other field
				find(it->first);
			}
		} else if (master == HEROS_INVALID_NUMBER && ut.can_reside() && ut.touch_dirs().size() >= 6) {
			std::map<int, const unit_type*>::const_iterator it2 = city_type_.find(ut.level());
			if (it2 == city_type_.end()) {
				city_type_.insert(std::make_pair(ut.level(), &ut));
				// build all, form other field
				find(it->first);
			}
		}
	}

	// form artifical types
	if (wall_type_) {
		artifical_types_[wall_type_->id()] = wall_type_;
		master_types_.insert(std::make_pair(wall_type_->master(), wall_type_));
	}
	if (keep_type_) {
		artifical_types_[keep_type_->id()] = keep_type_;
		master_types_.insert(std::make_pair(keep_type_->master(), keep_type_));
	}
	if (market_type_) {
		artifical_types_[market_type_->id()] = market_type_;
		master_types_.insert(std::make_pair(market_type_->master(), market_type_));
	}
	if (technology_type_) {
		artifical_types_[technology_type_->id()] = technology_type_;
		master_types_.insert(std::make_pair(technology_type_->master(), technology_type_));
	}
	if (tactic_type_) {
		artifical_types_[tactic_type_->id()] = tactic_type_;
		master_types_.insert(std::make_pair(tactic_type_->master(), tactic_type_));
	}
	if (tower_type_) {
		artifical_types_[tower_type_->id()] = tower_type_;
		master_types_.insert(std::make_pair(tower_type_->master(), tower_type_));
	}
	if (fort_type_) {
		artifical_types_[fort_type_->id()] = fort_type_;
		master_types_.insert(std::make_pair(fort_type_->master(), fort_type_));
	}
	if (school_type_) {
		artifical_types_[school_type_->id()] = school_type_;
		master_types_.insert(std::make_pair(school_type_->master(), school_type_));
	}
	if (businessman_type_) {
		artifical_types_[businessman_type_->id()] = businessman_type_;
		master_types_.insert(std::make_pair(businessman_type_->master(), businessman_type_));
	}
	if (scholar_type_) {
		artifical_types_[scholar_type_->id()] = scholar_type_;
		master_types_.insert(std::make_pair(scholar_type_->master(), scholar_type_));
	}
	if (transport_type_) {
		artifical_types_[transport_type_->id()] = transport_type_;
		master_types_.insert(std::make_pair(transport_type_->master(), transport_type_));
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

	index = 0, complex_index = tdecree::min_complex_index;
	BOOST_FOREACH (const config &decree, cfg.child_range("decree")) {
		const std::string id = decree["id"].str();
		if (id.empty()) {
			throw config::error("[decree] error, no id attribute");
		}
		if (decrees_id_.find(id) != decrees_id_.end()) {
			throw config::error("[decree] error, duplicate id " + id);
		}
		
		tdecree c(index, complex_index, decree);
		std::pair<std::map<int, tdecree>::iterator, bool> ins = decrees_.insert(std::make_pair(c.index(), c));
		decrees_id_[id] = c.index();

		if (!c.complex()) {
			ins.first->second.set_atom_part();
			index ++;
		} else {
			complex_index ++;
		}

		loadscreen::increment_progress();
	}

	if (const config& genera_cfg = cfg.child("genus")) {
		BOOST_FOREACH (const config& sub, cfg.child_range("genus")) {
			tgenus g(sub);
			genera_.insert(std::make_pair(g.index(), g));

			loadscreen::increment_progress();
		}
	} else {
		throw config::error("[genus] error, must define [genus] block in [units].");
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
	keytypes_.clear();
	artifical_types_.clear();
	master_types_.clear();
	master_ids_.clear();
	movement_types_.clear();
	races_.clear();
	modifications_.clear();
	traits_.clear();
	complex_feature_.clear();
	features_level_.clear();
	treasures_.clear();
	abilities_.clear();
	specials_.clear();
	tactics_.clear();
	tactics_id_.clear();
	characters_.clear();
	characters_id_.clear();
	decrees_.clear();
	decrees_id_.clear();
	genera_.clear();
	technologies_.clear();
	nobles_.clear();
	sort_nobles_.clear();
	leader_nobles_.clear();
	formations_.clear();
	formations_id_.clear();
	arms_ids_.clear();
	atype_ids_.clear();
	range_ids_.clear();
	especials_.clear();
	utype_anims_.clear();
	can_recruit_.clear();
	navigation_types_.clear();

	for (std::vector<advance_tree::node*>::iterator it = utype_tree_.begin(); it != utype_tree_.end(); ++ it) {
		delete *it;
	}
	utype_tree_.clear();
	for (std::vector<advance_tree::node*>::iterator it = technology_tree_.begin(); it != technology_tree_.end(); ++ it) {
		delete *it;
	}
	technology_tree_.clear();
	selectable_technologies_.clear();

	build_status_ = unit_type::NOT_BUILT;

	wall_type_ = NULL;
	keep_type_ = NULL;
	market_type_ = NULL;
	technology_type_ = NULL;
	tactic_type_ = NULL;
	tower_type_ = NULL;
	fort_type_ = NULL;
	school_type_ = NULL;
	city_type_.clear();
	businessman_type_ = NULL;
	scholar_type_ = NULL;
	scout_type_ = NULL;
	transport_type_ = NULL;

	hide_help_all_ = false;
	hide_help_race_.clear();
	hide_help_type_.clear();
}

const unit_type* unit_type_data::keytype2(int index) const
{
	std::map<int, const unit_type*>::const_iterator find = keytypes_.find(index);
	if (find != keytypes_.end()) {
		return find->second;
	}
	return NULL;
}

const unit_type* unit_type_data::master_type(int number) const 
{ 
	std::map<int, const unit_type*>::const_iterator find = master_types_.find(number);
	if (find == master_types_.end()) {
		return NULL;
	}
	return find->second;
}

const unit_type* unit_type_data::id_type(const std::string& id) const
{
	std::map<std::string, int>::const_iterator find = master_ids_.find(id);
	if (find == master_ids_.end()) {
		return NULL;
	}
	return master_type(find->second);
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
	BOOST_FOREACH (const std::string& t_id, trees) {
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

const ttechnology& unit_type_data::technology(const std::string& id) const
{
	std::map<std::string, ttechnology>::const_iterator it = technologies_.find(id);
	return it->second;
}

int unit_type_data::level_noble_count(int level) const
{
	std::map<int, std::set<tnoble> >::const_iterator it = nobles_.find(level);
	if (it != nobles_.end()) {
		return it->second.size();
	} else {
		return 0;
	}
}

const tnoble& unit_type_data::noble(int index) const
{
	return *sort_nobles_[index];
}

int unit_type_data::arms_from_id(const std::string& id) const
{
	std::vector<std::string>::const_iterator it = std::find(arms_ids_.begin(), arms_ids_.end(), id);
	if (it != arms_ids_.end()) {
		// return it - arms_ids_.begin();
		return std::distance(arms_ids_.begin(), it);
	}
	return -1;
}

int unit_type_data::range_from_id(const std::string& id) const
{
	std::vector<std::string>::const_iterator it = std::find(range_ids_.begin(), range_ids_.end(), id);
	if (it != range_ids_.end()) {
		return std::distance(range_ids_.begin(), it);
	}
	return -1;
}

const std::string& unit_type_data::especial_id(int character) const
{
	if (character < 0 || character >= (int)especials_.size()) {
		return null_str;
	} else {
		return especials_[character].id_;
	}
}

int unit_type_data::especial_from_id(const std::string& id) const
{
	if (id.empty()) {
		return NO_ESPECIAL;
	}
	for (std::vector<tespecial>::const_iterator it = especials_.begin(); it != especials_.end(); ++ it) {
		if (it->id_ == id) {
			return it->index_;
		}
	}
	return NO_ESPECIAL;
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

void unit_type_data::generate_utype_tree()
{
	for (std::vector<advance_tree::node*>::iterator it = utype_tree_.begin(); it != utype_tree_.end(); ++ it) {
		delete *it;
	}
	utype_tree_.clear();

	std::map<std::string, const advance_tree::base*> src;
	for (std::map<std::string, unit_type>::const_iterator it = types_.begin(); it != types_.end(); ++ it) {
		src.insert(std::make_pair(it->first, &it->second));
	}
	generate_advance_tree(src, utype_tree_);
}

void technology_tree_2_selectable_internal(const std::vector<advance_tree::node>& advances_to, std::set<const ttechnology*>& selectable_technologies)
{
	for (std::vector<advance_tree::node>::const_iterator it = advances_to.begin(); it != advances_to.end(); ++ it) {
		const ttechnology* current = dynamic_cast<const ttechnology*>(it->current);
		selectable_technologies.insert(current);
		if (!it->advances_to.empty()) {
			technology_tree_2_selectable_internal(it->advances_to, selectable_technologies);
		}
	}
}

void unit_type_data::generate_technology_tree()
{
	for (std::vector<advance_tree::node*>::iterator it = technology_tree_.begin(); it != technology_tree_.end(); ++ it) {
		delete *it;
	}
	technology_tree_.clear();

	std::map<std::string, const advance_tree::base*> src;
	for (std::map<std::string, ttechnology>::const_iterator it = technologies_.begin(); it != technologies_.end(); ++ it) {
		src.insert(std::make_pair(it->first, &it->second));
	}
	generate_advance_tree(src, technology_tree_);

#if defined(_KINGDOM_EXE) || !defined(_WIN32)
	// remove branch that is only one node.
	for (std::vector<advance_tree::node*>::iterator it = technology_tree_.begin(); it != technology_tree_.end(); ) {
		if ((*it)->advances_to.empty()) {
			delete *it;
			it = technology_tree_.erase(it);
		} else {
			++ it;
		}
	}
#endif
	selectable_technologies_.clear();
	for (std::vector<advance_tree::node*>::const_iterator it = technology_tree_.begin(); it != technology_tree_.end(); ++ it) {
		const advance_tree::node* n = *it;
		const ttechnology* current = dynamic_cast<const ttechnology*>(n->current);
		selectable_technologies_.insert(current);
		if (!n->advances_to.empty()) {
			technology_tree_2_selectable_internal(n->advances_to, selectable_technologies_);
		}
	}

}

const unit_race *unit_type_data::find_race(const std::string &key) const
{
	race_map::const_iterator i = races_.find(key);
	return i != races_.end() ? &i->second : NULL;
}


namespace help {
// Helpers for making generation of topics easier.
const std::string faction_prefix = "faction_";

std::string bold(const std::string &s)
{
	std::stringstream ss;
	ss << "<bold>text='" << escape(s) << "'</bold>";
	return ss.str();
}

std::string jump(const unsigned amount)
{
	std::stringstream ss;
	ss << "<jump>amount=" << amount << "</jump>";
	return ss.str();
}

std::string jump_to(const unsigned pos)
{
	std::stringstream ss;
	ss << "<jump>to=" << pos << "</jump>";
	return ss.str();
}

// Return the width for the image with filename.
unsigned image_width(const std::string &filename)
{
	image::locator loc(filename);
	surface surf(image::get_image(loc));
	if (surf != NULL) {
		return surf->w;
	}
	return 0;
}

void push_tab_pair(std::vector<std::pair<std::string, unsigned int> > &v, const std::string &s)
{
	v.push_back(std::make_pair(s, font::line_width(s, normal_font_size)));
}

typedef std::vector<std::vector<std::pair<std::string, unsigned int > > > table_spec;
// Create a table using the table specs. Return markup with jumps
// that create a table. The table spec contains a vector with
// vectors with pairs. The pairs are the markup string that should
// be in a cell, and the width of that cell.
std::string generate_table(const table_spec &tab, const unsigned int spacing=font::relative_size(20))
{
	table_spec::const_iterator row_it;
	std::vector<std::pair<std::string, unsigned> >::const_iterator col_it;
	unsigned int num_cols = 0;
	for (row_it = tab.begin(); row_it != tab.end(); ++row_it) {
		if (row_it->size() > num_cols) {
			num_cols = row_it->size();
		}
	}
	std::vector<unsigned int> col_widths(num_cols, 0);
	// Calculate the width of all columns, including spacing.
	for (row_it = tab.begin(); row_it != tab.end(); ++row_it) {
		unsigned int col = 0;
		for (col_it = row_it->begin(); col_it != row_it->end(); ++col_it) {
			if (col_widths[col] < col_it->second + spacing) {
				col_widths[col] = col_it->second + spacing;
			}
			++col;
		}
	}
	std::vector<unsigned int> col_starts(num_cols);
	// Calculate the starting positions of all columns
	for (unsigned int i = 0; i < num_cols; ++i) {
		unsigned int this_col_start = 0;
		for (unsigned int j = 0; j < i; ++j) {
			this_col_start += col_widths[j];
		}
		col_starts[i] = this_col_start;
	}
	std::stringstream ss;
	for (row_it = tab.begin(); row_it != tab.end(); ++row_it) {
		unsigned int col = 0;
		for (col_it = row_it->begin(); col_it != row_it->end(); ++col_it) {
			ss << jump_to(col_starts[col]) << col_it->first;
			++col;
		}
		ss << "\n";
	}
	return ss.str();
}


std::string make_link(const std::string& text, const std::string& dst)
{
	// some sorting done on list of links may rely on the fact that text is first
	return "<ref>text='" + escape(text) + "' dst='" + escape(dst) + "'</ref>";
}

std::string make_unit_link(const std::string& type_id)
{
	std::string link;

	const unit_type *type = unit_types.find(type_id);
	if (!type) {
		std::cerr << "Unknown unit type : " << type_id << "\n";
		// don't return an hyperlink (no page)
		// instead show the id (as hint)
		link = type_id;
	} else if (!type->hide_help()) {
		std::string name = type->type_name();
		std::string ref_id;
		ref_id = unit_prefix + type->id();
		link =  make_link(name, ref_id);
	} // if hide_help then link is an empty string

	return link;
}

std::vector<std::string> make_unit_links_list(const std::vector<std::string>& type_id_list, bool ordered)
{
	std::vector<std::string> links_list;
	BOOST_FOREACH (const std::string &type_id, type_id_list) {
		std::string unit_link = make_unit_link(type_id);
		if (!unit_link.empty())
			links_list.push_back(unit_link);
	}

	if (ordered)
		std::sort(links_list.begin(), links_list.end());

	return links_list;
}

std::vector<topic> generate_weapon_special_topics(const bool sort_generated)
{
	std::vector<topic> topics;

	std::map<t_string, std::string> special_description;
	std::map<t_string, std::set<std::string, string_less> > special_units;

	BOOST_FOREACH (const unit_type_data::unit_type_map::value_type &i, unit_types.types())
	{
		const unit_type &type = i.second;
		// Only show the weapon special if we find it on a unit that
		// detailed description should be shown about.
		std::vector<attack_type> attacks = type.attacks();
		for (std::vector<attack_type>::const_iterator it = attacks.begin();
					it != attacks.end(); ++it) {
#if defined(_KINGDOM_EXE) || !defined(_WIN32)
			std::vector<t_string> specials = (*it).special_tooltips(true);
#else
			std::vector<t_string> specials;
#endif
			for (std::vector<t_string>::iterator sp_it = specials.begin();
					sp_it != specials.end() && sp_it+1 != specials.end(); sp_it+=2)
			{
				if (special_description.find(*sp_it) == special_description.end()) {
					std::string description = *(sp_it+1);
					const size_t colon_pos = description.find(':');
					if (colon_pos != std::string::npos) {
						// Remove the first colon and the following newline.
						description.erase(0, colon_pos + 2);
					}
					special_description[*sp_it] = description;
				}

				if (!type.hide_help()) {
					//add a link in the list of units having this special
					std::string type_name = type.type_name();
					std::string ref_id = unit_prefix + type.id();
					//we put the translated name at the beginning of the hyperlink,
					//so the automatic alphabetic sorting of std::set can use it
					std::string link =  "<ref>text='" + escape(type_name) + "' dst='" + escape(ref_id) + "'</ref>";
					special_units[*sp_it].insert(link);
				}
			}
		}
	}

	for (std::map<t_string, std::string>::iterator s = special_description.begin();
			s != special_description.end(); ++s) {
		// use untranslated name to have universal topic id
		std::string id = "weaponspecial_" + s->first.base_str();
		std::stringstream text;
		text << s->second;
		text << "\n\n" << _("<header>text='Units having this special attack'</header>") << "\n";
		std::set<std::string, string_less> &units = special_units[s->first];
		for (std::set<std::string, string_less>::iterator u = units.begin(); u != units.end(); ++u) {
			text << (*u) << "\n";
		}

		topics.push_back( topic(s->first, id, text.str()) );
	}

	if (sort_generated)
		std::sort(topics.begin(), topics.end(), title_less());
	return topics;
}

std::vector<topic> generate_ability_topics(const bool sort_generated)
{
	std::vector<topic> topics;
	std::map<t_string, std::string> ability_description;
	std::map<t_string, std::set<std::string, string_less> > ability_units;
	// Look through all the unit types, check if a unit of this type
	// should have a full description, if so, add this units abilities
	// for display. We do not want to show abilities that the user has
	// not encountered yet.
	BOOST_FOREACH (const unit_type_data::unit_type_map::value_type &i, unit_types.types())
	{
		const unit_type &type = i.second;

		std::vector<t_string> const* abil_vecs[2];
		abil_vecs[0] = &type.abilities();

		std::vector<std::string> const* desc_vecs[2];
		desc_vecs[0] = &type.ability_tooltips();

		for(int i=0; i<1; ++i) {
			std::vector<t_string> const& abil_vec = *abil_vecs[i];
			std::vector<std::string> const& desc_vec = *desc_vecs[i];
			for(size_t j=0; j < abil_vec.size(); ++j) {
				t_string const& abil_name = abil_vec[j];
				if (ability_description.find(abil_name) == ability_description.end()) {
					//new ability, generate a descripion
					if(j >= desc_vec.size()) {
						ability_description[abil_name] = "";
					} else {
						std::string const& abil_desc = desc_vec[j];
						const size_t colon_pos = abil_desc.find(':');
						if(colon_pos != std::string::npos && colon_pos + 1 < abil_desc.length()) {
							// Remove the first colon and the following newline.
							ability_description[abil_name] = abil_desc.substr(colon_pos + 2);
						} else {
							ability_description[abil_name] = abil_desc;
						}
					}
				}

				if (!type.hide_help()) {
					//add a link in the list of units having this ability
					std::string type_name = type.type_name();
					std::string ref_id = unit_prefix +  type.id();
					//we put the translated name at the beginning of the hyperlink,
					//so the automatic alphabetic sorting of std::set can use it
					std::string link =  "<ref>text='" + escape(type_name) + "' dst='" + escape(ref_id) + "'</ref>";
					ability_units[abil_name].insert(link);
				}
			}
		}
	}

	for (std::map<t_string, std::string>::iterator a = ability_description.begin(); a != ability_description.end(); ++a) {
		// we generate topic's id using the untranslated version of the ability's name
		std::string id = "ability_" + a->first.base_str();
		std::stringstream text;
		text << a->second;  //description
		text << "\n\n" << _("<header>text='Units having this ability'</header>") << "\n";
		std::set<std::string, string_less> &units = ability_units[a->first];
		for (std::set<std::string, string_less>::iterator u = units.begin(); u != units.end(); ++u) {
			text << (*u) << "\n";
		}

		topics.push_back( topic(a->first, id, text.str()) );
	}

	if (sort_generated)
		std::sort(topics.begin(), topics.end(), title_less());
	return topics;
}

std::vector<topic> generate_faction_topics(const bool sort_generated)
{
	std::vector<topic> topics;
	const config& era = game_cfg->child("era");
	if (era) {
		std::vector<std::string> faction_links;
		BOOST_FOREACH (const config &f, era.child_range("multiplayer_side")) {
			const std::string& id = f["id"];
			if (id == "Random")
				continue;

			std::stringstream text;

			const std::string& description = f["description"];
			if (!description.empty()) {
				text << description << "\n";
				text << "\n";
			}

			text << "<header>text='" << _("Leaders:") << "'</header>" << "\n";
			const std::vector<std::string> leaders =
					make_unit_links_list( utils::split(f["leader"]), true );
			BOOST_FOREACH (const std::string &link, leaders) {
				text << link << "\n";
			}

			text << "\n";

			text << "<header>text='" << _("Recruits:") << "'</header>" << "\n";
			const std::vector<std::string> recruits =
					make_unit_links_list( utils::split(f["recruit"]), true );
			BOOST_FOREACH (const std::string &link, recruits) {
				text << link << "\n";
			}

			const std::string name = f["name"];
			const std::string ref_id = faction_prefix + id;
			topics.push_back( topic(name, ref_id, text.str()) );
			faction_links.push_back(make_link(name, ref_id));
		}

		std::stringstream text;
		text << "<header>text='" << _("Era:") << " " << era["name"] << "'</header>" << "\n";
		text << "\n";
		const std::string& description = era["description"];
		if (!description.empty()) {
			text << description << "\n";
			text << "\n";
		}

		text << "<header>text='" << _("Factions:") << "'</header>" << "\n";

		std::sort(faction_links.begin(), faction_links.end());
		BOOST_FOREACH (const std::string &link, faction_links) {
			text << link << "\n";
		}

		topics.push_back( topic(_("Factions"), "..factions_section", text.str()) );
	} else {
		topics.push_back( topic( _("Factions"), "..factions_section",
			_("Factions are only used in multiplayer")) );
	}

	if (sort_generated)
		std::sort(topics.begin(), topics.end(), title_less());
	return topics;
}

class unit_topic_generator: public topic_generator
{
	const unit_type& type_;
	typedef std::pair< std::string, unsigned > item;
	void push_header(std::vector< item > &row, char const *name) const {
		row.push_back(item(bold(name), font::line_width(name, normal_font_size, TTF_STYLE_BOLD)));
	}
public:
	unit_topic_generator(const unit_type &t): type_(t) {}
	virtual std::string operator()() const {
		// this will force the lazy loading to build this unit
		unit_types.find(type_.id(), unit_type::WITHOUT_ANIMATIONS);

		std::stringstream ss;
		std::string clear_stringstream;
		const std::string detailed_description = type_.unit_description();
		const unit_type& female_type = type_.get_gender_unit_type(unit_race::FEMALE);
		const unit_type& male_type = type_.get_gender_unit_type(unit_race::MALE);

		// Show the unit's image and its level.
		ss << "<img>src='" << male_type.image() << "~RC(" << male_type.flag_rgb() << ">1)" << "'</img> ";

		if (&female_type != &male_type) {
			ss << "<img>src='" << female_type.image() << "~RC(" << female_type.flag_rgb() << ">1)" << "'</img> ";
		}

		ss << "<format>font_size=" << font::relative_size(11) << " text=' " << escape(_("level"))
		   << " " << type_.level() << "'</format>";

		const std::string male_portrait = "";
		const std::string female_portrait = "";

		if (male_portrait.empty() == false && male_portrait != male_type.image()) {
			ss << "<img>src='" << male_portrait << "' align='right'</img> ";
		}

		if (female_portrait.empty() == false && female_portrait != male_portrait && female_portrait != female_type.image()) {
			ss << "<img>src='" << female_portrait << "' align='right'</img> ";
		}

		ss << "\n";

		// Print cross-references to units that this unit advances from/to.
		// Cross reference to the topics containing information about those units.
		const bool first_reverse_value = true;
		bool reverse = first_reverse_value;
		do {
			std::vector<std::string> adv_units =
				reverse ? type_.advances_from() : type_.advances_to();
			bool first = true;

			BOOST_FOREACH (const std::string &adv, adv_units)
			{
				const unit_type *type = unit_types.find(adv);
				if (!type || type->hide_help()) continue;

				if (first) {
					if (reverse)
						ss << _("Advances from: ");
					else
						ss << _("Advances to: ");
					first = false;
				} else
					ss << ", ";

				std::string lang_unit = type->type_name();
				std::string ref_id;
				ref_id = unit_prefix + type->id();
				ss << "<ref>dst='" << escape(ref_id) << "' text='" << escape(lang_unit) << "'</ref>";
			}
			ss << "\n"; //added even if empty, to avoid shifting

			reverse = !reverse; //switch direction
		} while(reverse != first_reverse_value); // don't restart

		// Print the race of the unit, cross-reference it to the
		// respective topic.
		const std::string race_id = type_.race();
		std::string race_name;
		if (const unit_race *r = unit_types.find_race(race_id)) {
			race_name = r->plural_name();
		} else {
			race_name = _("race^Miscellaneous");
		}
		ss << _("Race: ");
		ss << "<ref>dst='" << escape("..race_"+race_id) << "' text='" << escape(race_name) << "'</ref>";
		ss << "\n";

		// Print the abilities the units has, cross-reference them
		// to their respective topics.
		if (!type_.abilities().empty()) {
			ss << _("Abilities: ");
			for(std::vector<t_string>::const_iterator ability_it = type_.abilities().begin(),
				 ability_end = type_.abilities().end();
				 ability_it != ability_end; ++ability_it) {
				const std::string ref_id = "ability_" + ability_it->base_str();
				std::string lang_ability = gettext(ability_it->c_str());
				ss << "<ref>dst='" << escape(ref_id) << "' text='" << escape(lang_ability)
				   << "'</ref>";
				if (ability_it + 1 != ability_end)
					ss << ", ";
			}
			ss << "\n";
		}

		ss << "\n";

		// Print some basic information such as HP and movement points.
		ss << _("HP: ") << type_.hitpoints() << jump(30)
		   << _("Moves: ") << type_.movement() << jump(30)
		   << _("Cost: ") << type_.cost() << jump(30)
		   << _("Alignment: ")
		   << "<ref>dst='time_of_day' text='"
		   << type_.alignment_description(type_.alignment(), type_.genders().front())
		   << "'</ref>"
		   << jump(30);
		if (type_.can_advance())
			ss << _("Required XP: ") << type_.experience_needed();

		// Print the detailed description about the unit.
		ss << "\n\n" << detailed_description;

		// Print the different attacks a unit has, if it has any.
		std::vector<attack_type> attacks = type_.attacks();
		if (!attacks.empty()) {
			// Print headers for the table.
			ss << "\n\n<header>text='" << escape(_("unit help^Attacks"))
			   << "'</header>\n\n";
			table_spec table;

			std::vector<item> first_row;
			// Dummy element, icons are below.
			first_row.push_back(item("", 0));
			push_header(first_row, _("unit help^Name"));
			push_header(first_row, _("Type"));
			push_header(first_row, _("Strikes"));
			push_header(first_row, _("Range"));
			push_header(first_row, _("Special"));
			table.push_back(first_row);
			// Print information about every attack.
			for(std::vector<attack_type>::const_iterator attack_it = attacks.begin(),
				 attack_end = attacks.end();
				 attack_it != attack_end; ++attack_it) {
				std::string lang_weapon = attack_it->name();
				std::string lang_type = gettext(attack_it->type().c_str());
				std::vector<item> row;
				std::stringstream attack_ss;
				attack_ss << "<img>src='" << (*attack_it).icon() << "'</img>";
				row.push_back(std::make_pair(attack_ss.str(),
							     image_width(attack_it->icon())));
				push_tab_pair(row, lang_weapon);
				push_tab_pair(row, lang_type);
				attack_ss.str(clear_stringstream);
				attack_ss << attack_it->damage() << '-' << attack_it->num_attacks() << " " << attack_it->accuracy_parry_description();
				push_tab_pair(row, attack_ss.str());
				attack_ss.str(clear_stringstream);
				push_tab_pair(row, _((*attack_it).range().c_str()));
				// Show this attack's special, if it has any. Cross
				// reference it to the section describing the
				// special.
#if defined(_KINGDOM_EXE) || !defined(_WIN32)
				std::vector<t_string> specials = attack_it->special_tooltips(true);
#else
				std::vector<t_string> specials;
#endif
				if(!specials.empty())
				{
					std::string lang_special = "";
					std::vector<t_string>::iterator sp_it;
					for (sp_it = specials.begin(); sp_it != specials.end(); ++sp_it) {
						const std::string ref_id = std::string("weaponspecial_")
							+ sp_it->base_str();
						lang_special = (*sp_it);
						attack_ss << "<ref>dst='" << escape(ref_id)
								  << "' text='" << escape(lang_special) << "'</ref>";
						if((sp_it + 1) != specials.end() && (sp_it + 2) != specials.end())
						{
							attack_ss << ", "; //comma placed before next special
						}
						++sp_it; //skip description
					}
					row.push_back(std::make_pair(attack_ss.str(),
						font::line_width(lang_special, normal_font_size)));

				}
				table.push_back(row);
			}
			ss << generate_table(table);
		}

		// Print the resistance table of the unit.
		ss << "\n\n<header>text='" << escape(_("Resistances"))
		   << "'</header>\n\n";
		table_spec resistance_table;
		std::vector<item> first_res_row;
		push_header(first_res_row, _("Attack Type"));
		push_header(first_res_row, _("Resistance"));
		resistance_table.push_back(first_res_row);
		const unit_movement_type &movement_type = type_.movement_type();
		utils::string_map dam_tab = movement_type.damage_table();
		for(utils::string_map::const_iterator dam_it = dam_tab.begin(), dam_end = dam_tab.end();
			 dam_it != dam_end; ++dam_it) {
			std::vector<item> row;
			int resistance = 100 - atoi((*dam_it).second.c_str());
			char resi[16];
			snprintf(resi,sizeof(resi),"% 4d%%",resistance);	// range: -100% .. +70%
			std::string color;
			if (resistance < 0)
				color = "red";
			else if (resistance <= 20)
				color = "yellow";
			else if (resistance <= 40)
				color = "white";
			else
				color = "green";

			std::string lang_weapon = gettext(dam_it->first.c_str());
			push_tab_pair(row, lang_weapon);
			std::stringstream str;
			str << "<format>color=" << color << " text='"<< resi << "'</format>";
			const std::string markup = str.str();
			str.str(clear_stringstream);
			str << resi;
			row.push_back(std::make_pair(markup,
						     font::line_width(str.str(), normal_font_size)));
			resistance_table.push_back(row);
		}
		ss << generate_table(resistance_table);

		if (map != NULL) {
			// Print the terrain modifier table of the unit.
			ss << "\n\n<header>text='" << escape(_("Terrain Modifiers"))
			   << "'</header>\n\n";
			std::vector<item> first_row;
			table_spec table;
			push_header(first_row, _("Terrain"));
			push_header(first_row, _("Defense"));
			push_header(first_row, _("Movement Cost"));

			table.push_back(first_row);

			const t_translation::t_list& terrain_list = map->get_terrain_list();
			for (t_translation::t_list::const_iterator terrain_it = terrain_list.begin(); terrain_it != terrain_list.end(); ++ terrain_it) {
				const t_translation::t_terrain terrain = *terrain_it;
				if (terrain == t_translation::FOGGED || terrain == t_translation::VOID_TERRAIN || terrain == t_translation::OFF_MAP_USER)
					continue;
				const terrain_type& info = map->get_terrain_info(terrain);

				if (info.union_type().size() == 1 && info.union_type()[0] == info.number() && info.is_nonnull()) {
					std::vector<item> row;
					const std::string& name = info.name();
					const std::string id = info.id();
					const int moves = movement_type.movement_cost(*map,terrain);
					std::stringstream str;
					str << "<ref>text='" << escape(name) << "' dst='"
						<< escape(std::string("terrain_") + id) << "'</ref>";
					row.push_back(std::make_pair(str.str(),
						font::line_width(name, normal_font_size)));

					//defense  -  range: +10 % .. +70 %
					str.str(clear_stringstream);
					const int defense =
						100 - movement_type.defense_modifier(*map,terrain);
					std::string color;
					if (defense < 0)
						color = "red";
					else if (defense < 20)
						color = "yellow";
					else if (defense < 40)
						color = "white";
					else
						color = "green";

					str << "<format>color=" << color << " text='"<< defense << "%'</format>";
					const std::string markup = str.str();
					str.str(clear_stringstream);
					str << defense << "%";
					row.push_back(std::make_pair(markup,
						     font::line_width(str.str(), normal_font_size)));

					//movement  -  range: 1 .. 5, unit_movement_type::UNREACHABLE=impassable
					str.str(clear_stringstream);
					const bool cannot_move = moves > type_.movement();
					if (cannot_move)		// cannot move in this terrain
						color = "red";
					else if (moves > 1)
						color = "yellow";
					else
						color = "white";

					str << "<format>color=" << color << " text='";
					// A 5 MP margin; if the movement costs go above
					// the unit's max moves + 5, we replace it with dashes.
					if(cannot_move && (moves > type_.movement() + 5)) {
						str << "'-'";
					} else {
						str << moves;
					}
					str << "'</format>";
					push_tab_pair(row, str.str());

					table.push_back(row);
				}
			}
			ss << generate_table(table);
		}
		return ss.str();
	}
};

void generate_races_sections(const config *help_cfg, section &sec, int level)
{
	std::set<std::string, string_less> races;
	std::set<std::string, string_less> visible_races;

	BOOST_FOREACH (const unit_type_data::unit_type_map::value_type &i, unit_types.types())
	{
		const unit_type &type = i.second;
		races.insert(type.race());
		if (!type.hide_help())
			visible_races.insert(type.race());
	}

	std::stringstream text;

	for(std::set<std::string, string_less>::iterator it = races.begin(); it != races.end(); ++it) {
		section race_section;
		config section_cfg;

		bool hidden = (visible_races.count(*it) == 0);

		section_cfg["id"] = hidden_symbol(hidden) + race_prefix + *it;

		std::string title;
		if (const unit_race *r = unit_types.find_race(*it)) {
			title = r->plural_name();
		} else {
			title = _("race^Miscellaneous");
		}
		section_cfg["title"] = title;

		section_cfg["generator"] = "units:" + *it;

		parse_config_internal(help_cfg, &section_cfg, race_section, level+1);
		sec.add_section(race_section);
	}
}


std::vector<topic> generate_unit_topics(const bool sort_generated, const std::string& race)
{
	std::vector<topic> topics;
	std::set<std::string, string_less> race_units;

	BOOST_FOREACH (const unit_type_data::unit_type_map::value_type &i, unit_types.types())
	{
		const unit_type &type = i.second;

		if (type.race() != race)
			continue;

		const std::string type_name = type.type_name();
		const std::string ref_id = hidden_symbol(type.hide_help()) + unit_prefix +  type.id();
		topic unit_topic(type_name, ref_id, "");
		unit_topic.text = new unit_topic_generator(type);
		topics.push_back(unit_topic);

		if (!type.hide_help()) {
			// we also record an hyperlink of this unit
			// in the list used for the race topic
			std::string link =  "<ref>text='" + escape(type_name) + "' dst='" + escape(ref_id) + "'</ref>";
			race_units.insert(link);
		}
	}

	//generate the hidden race description topic
	std::string race_id = "..race_"+race;
	std::string race_name;
	std::string race_description;
	if (const unit_race *r = unit_types.find_race(race)) {
		race_name = r->plural_name();
		race_description = r->description();
		// if (description.empty()) description =  _("No description Available");
	} else {
		race_name = _("race^Miscellaneous");
		// description =  _("Here put the description of the Miscellaneous race");
	}

	std::stringstream text;
	text << race_description;
	text << "\n\n" << _("<header>text='Units of this race'</header>") << "\n";
	for (std::set<std::string, string_less>::iterator u = race_units.begin(); u != race_units.end(); ++u) {
		text << (*u) << "\n";
	}
	topics.push_back(topic(race_name, race_id, text.str()) );

	if (sort_generated)
		std::sort(topics.begin(), topics.end(), title_less());
	return topics;
}

std::vector<topic> generate_topics(const bool sort_generated, const std::string &generator)
{
	std::vector<topic> res;
	if (editor || generator.empty()) {
		return res;
	}

	if (generator == "abilities") {
		res = generate_ability_topics(sort_generated);
	} else if (generator == "weapon_specials") {
		res = generate_weapon_special_topics(sort_generated);
	} else if (generator == "factions") {
		res = generate_faction_topics(sort_generated);
	} else {
		std::vector<std::string> parts = utils::split(generator, ':', utils::STRIP_SPACES);
		if (parts[0] == "units" && parts.size()>1) {
			res = generate_unit_topics(sort_generated, parts[1]);
		}
	}

	return res;
}

void generate_sections(const config *help_cfg, const std::string &generator, section &sec, int level)
{
	if (editor) {
		return;
	}

	if (generator == "races") {
		generate_races_sections(help_cfg, sec, level);
	}
}

}

unit_type_data unit_types;

#else

// anim_area
int app_fill_anim_tags(std::map<const std::string, int>& tags)
{
	return area_anim::MAX_ROSE_ANIM;
}

void app_fill_anim(int type, const config& cfg)
{
}

namespace help {
std::vector<topic> generate_topics(const bool sort_generated, const std::string &generator)
{
	std::vector<topic> res;
	return res;
}

void generate_sections(const config *help_cfg, const std::string &generator, section &sec, int level)
{
	return;
}
}

#endif