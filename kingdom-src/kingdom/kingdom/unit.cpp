/* $Id: unit.cpp 47840 2010-12-05 18:10:01Z mordante $ */
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
 *  Routines to manage units.
 */

#include "unit.hpp"

#include "callable_objects.hpp"
#include "formula.hpp"
#include "game_display.hpp"
#include "game_preferences.hpp"
#include "gamestatus.hpp"
#include "gettext.hpp"
#include "halo.hpp"
#include "log.hpp"
#include "resources.hpp"
#include "unit_abilities.hpp"
#include "terrain_filter.hpp"
#include "formula_string_utils.hpp"
#include "scripting/lua.hpp"
#include "play_controller.hpp"
#include "artifical.hpp"
#include "unit_display.hpp"
#include "wml_exception.hpp"
#include "serialization/parser.hpp"
#include "dialogs.hpp"
#include "integrate.hpp"

#include <boost/foreach.hpp>

namespace {
	/**
	 * Pointers to units which have data in their internal caches. The
	 * destructor of an unit removes itself from the cache, so the pointers are
	 * always valid.
	 */
	static std::vector<const unit *> units_with_cache;
	static std::map<unit*, int> alert_cache;
	static std::multimap<unit*, unit*> provoke_cache;
}

unit* loc_in_alert_area(const std::vector<team>& teams, unit& interloper, const map_location& from, const map_location& to)
{
	unit* alert = NULL;
	for (std::map<unit*, int>::const_iterator it = alert_cache.begin(); it != alert_cache.end(); ++ it) {
		unit& u = *it->first;
		if (&interloper == &u || !teams[interloper.side() - 1].is_enemy(u.side())) {
			continue;
		}
		if (to.in_area(u.get_location(), it->second)) {
			alert = &u;
			break;
		}
	}
	return alert;
}

bool provoke_cache_ready;
std::string provoke_cache_2_str()
{
	std::stringstream strstr;
	for (std::multimap<unit*, unit*>::const_iterator it = provoke_cache.begin(); it != provoke_cache.end(); ++ it) {
		if (it != provoke_cache.begin()) {
			strstr << ", ";
		}
		int h1, h2;
		h1 = it->first->master().number_;
		h2 = it->second->master().number_;
		strstr << h1 << ", " << h2;
	}
	return strstr.str();
}

void str_2_provoke_cache(unit_map& units, hero_map& heros, const std::string& str)
{
	const std::vector<std::string> vstr = utils::split(str);
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		int h1 = lexical_cast_default<int>(*it);
		++ it;
		int h2 = lexical_cast_default<int>(*it);
		provoke_cache.insert(std::make_pair(units.find_unit(heros[h1]), units.find_unit(heros[h2])));
	}
	provoke_cache_ready = true;
}

unit* find_provoke(const unit* provoked)
{
	for (std::multimap<unit*, unit*>::const_iterator it = provoke_cache.begin(); it != provoke_cache.end(); ++ it) {
		if (it->second == provoked) return it->first;
	}
	return NULL;
}

namespace slot_cache {

std::vector<tslot> cache;
bool ready;
unit* selected_u = NULL;

int tslot::cost() const
{
	if (type == CAST_TACTIC) {
		const ttactic& t = unit_types.tactic(tactic.h->tactic_);
		return t.cost();

	} else if (type == CLEAR_FORMATIONED) {
		return u->calculate_clear_formated_cost();

	} else if (type == MOVE) {
		return game_config::cost_intervene_move;

	}
	return 0;
}

std::string tslot::label() const
{
	std::stringstream strstr;
	bool spend = false;
	if (type == ACTIVE_TACTIC) {
		const ttactic& t = unit_types.tactic(u->tactic_hero()->tactic_);
		strstr << t.name();

	} else if (type == CAST_TACTIC) {
		const ttactic& t = unit_types.tactic(tactic.h->tactic_);
		strstr << t.name();
		if (u->tactic_degree() < t.point() * game_config::tactic_degree_per_point && !u->hot_turns()) {
			spend = true;;
		}

	} else if (type == CLEAR_FORMATIONED) {
		spend = true;
		strstr << _("Formation");

	} else if (type == MOVE) {
		spend = true;
		strstr << _("Move");

	}

	if (spend) {
		strstr << " (" << cost() << ")";
	}
	return strstr.str();
}

std::string cache_2_str()
{
	std::stringstream strstr;
	for (std::vector<tslot>::const_iterator it = cache.begin(); it != cache.end(); ++ it) {
		const tslot& slot = *it;
		if (slot.type != tslot::ACTIVE_TACTIC) {
			continue;
		}
		if (it != cache.begin()) {
			strstr << ", ";
		}
		int h = slot.u->tactic_hero()->number_;
		strstr << h;
	}
	return strstr.str();
}

void str_2_cache(unit_map& units, hero_map& heros, const std::string& str)
{
	const std::vector<std::string> vstr = utils::split(str);
	// see remakr
	VALIDATE(vstr.size() == cache.size(), "slot_cache::str_2_cache, generate and wml size don't match!");
	cache.clear();
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		int h = lexical_cast_default<int>(*it);
		insert_active_tactic(*units.find_unit(heros[h]));
	}
	ready = true;
}

bool find(const unit& u)
{
	for (std::vector<tslot>::const_iterator it = cache.begin(); it != cache.end(); ++ it) {
		const tslot& slot = *it;
		if (slot.u == &u) {
			return true;
		}
	}
	return false;
}

void insert_active_tactic(unit& u)
{
	cache.push_back(tslot(tslot::ACTIVE_TACTIC, &u));
}

void insert_cast_tactic(unit& tactician, hero& h, unit* special)
{
	cache.push_back(tslot(tslot::CAST_TACTIC, &tactician));
	tslot& slot = cache.back();
	slot.tactic = tslot::tcast_tactic(&h, special);
}

void insert_clear_formationed(unit& u)
{
	cache.push_back(tslot(tslot::CLEAR_FORMATIONED, &u));
}

void insert_intervene_move(unit& u, const map_location& to)
{
	cache.push_back(tslot(tslot::MOVE, &u));
	tslot& slot = cache.back();
	slot.move = tslot::tmove(to);
}

}

namespace guard_cache {
tguard null_guard = tguard(map_location(-1000, -1000)); // map_location::null_location is static variable, maybe not initialize so far.
std::map<unit*, tguard> cache;
bool ready;

std::string cache_2_str()
{
	std::stringstream strstr;
	for (std::map<unit*, tguard>::const_iterator it = cache.begin(); it != cache.end(); ++ it) {
		const unit& u = *(it->first);
		const tguard& guard = it->second;
		if (it != cache.begin()) {
			strstr << "; ";
		}
		int h = u.master().number_;
		strstr << h << ", " << guard.loc.x << ", " << guard.loc.y << ", " << guard.radius;
	}
	return strstr.str();
}

void str_2_cache(unit_map& units, hero_map& heros, const std::string& str)
{
	cache.clear();
	const std::vector<std::string> vstr = utils::split(str, ';');
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		const std::vector<std::string> vstr2 = utils::split(*it);
		VALIDATE(vstr2.size() == 4, "guard_cache::str_2_cache, items must be 4!");

		int h = lexical_cast_default<int>(vstr2[0]);
		map_location loc(lexical_cast_default<int>(vstr2[1]), lexical_cast_default<int>(vstr2[2]));
		int radius = lexical_cast_default<int>(vstr2[3]);
		cache.insert(std::make_pair(units.find_unit(heros[h]), tguard(loc, radius)));
	}
	ready = true;
}

tguard& find(const unit& u)
{
	return cache.find(const_cast<unit*>(&u))->second;
}

}

uint8_t mask0_2bit[4] = {0xfc, 0xf3, 0xcf, 0x3f};

std::vector<std::pair<int, unit*> > unit::null_int_unitp_pair;
map_location unit::pack_base_loc;

bool actor_valid()
{
	return !!unit::actor;
}

bool actor_can_action(const unit_map& units)
{
	if (!unit::actor) {
		return false;
	}
	if (unit::actor->is_city()) {
		return true;
	}
	return !units.city_from_loc(unit::actor->get_location());
}

bool actor_can_continue_action(const unit_map& units, int actor_side)
{
	if (tent::turn_based) {
		return true;
	}
	if (!unit::actor) {
		return false;
	}
	if (unit::actor->side() != actor_side) {
		return false;
	}
	if (unit::actor->is_city()) {
		return true;
	}
	return !units.city_from_loc(unit::actor->get_location());
}

bool current_can_action(const unit& u)
{
	if (u.is_commoner() || u.is_soldier()) {
		return false;
	}

	if (!tent::turn_based) {
		if (u.get_state(ustate_tag::REVIVALED)) {
			return true;
		}

		if (!unit::actor) {
			return false;
		}

		if (&u == unit::actor) {
			return true;
		}

		if (unit::actor->is_city() && unit::actor->side() == u.side()) {
			if (u.is_resided()) {
				// reside troop
				return true;
			} else if (u.get_state(ustate_tag::EXPEDITED)) {
				return true;
			}
		}

		return false;

	} else {
		return u.side() == unit_map::top_side;
	}
}

int calculate_end_ticks()
{
	return (unit_map::main_ticks / game_config::ticks_per_turn + 1) * game_config::ticks_per_turn;
}

static const unit_type &get_unit_type(const std::string &type_id)
{
	const unit_type *i = unit_types.find(type_id);
	if (!i) {
		throw game::game_error("unknown unit type: " + type_id);
	}
	return *i;
}

unit_merit::unit_merit()
	: field_turns_(0),
	cause_damage_(0),
	been_damage_(0),
	defeat_units_(0)
{
}

unit_merit::unit_merit(const unit_merit& o):
	field_turns_(o.field_turns_),
	cause_damage_(o.cause_damage_),
	been_damage_(o.been_damage_),
	defeat_units_(o.defeat_units_)
{
}

bool unit::draw_desc_ = true;
bool unit::ignore_pack = false;
bool unit::dont_wander = false;
unit* unit::actor = NULL;
size_t unit::global_signature = 0;

dont_wander_lock::dont_wander_lock()
{
	unit::dont_wander = true;
}

dont_wander_lock::~dont_wander_lock()
{
	unit::dont_wander = false;
}

// Copy constructor
unit::unit(const unit& o):
	base_unit(o),
	unit_merit(o),
	/* unit */
	units_(o.units_),
	heros_(o.heros_),
	teams_(o.teams_),
	cfg_(o.cfg_),
	advances_to_(o.advances_to_),
	type_(o.type_),
	packee_type_(o.packee_type_),
	unit_type_(o.unit_type_),
	packee_unit_type_(o.packee_unit_type_),
	especial_(o.especial_),
	race_(o.race_),
	type_name_(o.type_name_),
	undead_variation_(o.undead_variation_),
	variation_(o.variation_),

	hit_points_(o.hit_points_),
	max_hit_points_(o.max_hit_points_),
	experience_(o.experience_),
	max_experience_(o.max_experience_),
	ticks_(o.ticks_),
	max_ticks_(o.max_ticks_),
	drawn_ticks_(o.drawn_ticks_),
	ticks_increase_(o.ticks_increase_),
	level_(o.level_),
	upkeep_(o.upkeep_),
	alignment_(o.alignment_),
	flag_rgb_(o.flag_rgb_),
	image_mods_(o.image_mods_),
	traits_(o.traits_),
	tactics_(o.tactics_),
	modifications_(o.modifications_),
	side_(o.side_),
	gender_(o.gender_),

	alpha_(o.alpha_),

	movement_(o.movement_),
	max_movement_(o.max_movement_),
	movement_costs_(o.movement_costs_),
	defense_mods_(o.defense_mods_),
	resting_(o.resting_),
	tactic_degree_(o.tactic_degree_),
	signature(o.signature),
	ticks_adjusting(o.ticks_adjusting),
	attacks_left_(o.attacks_left_),
	max_attacks_(o.max_attacks_),

	states_(o.states_),
	emit_zoc_(o.emit_zoc_),
	cancel_zoc_(o.cancel_zoc_),

	attacks_(o.attacks_),
	trait_names_(o.trait_names_),
	unit_value_(o.unit_value_),
	goto_(o.goto_),
	from_(o.from_),
	flying_(o.flying_),

	unit_halo_(halo::NO_HALO),
	hidden_(o.hidden_),

	invisibility_cache_(),
	cityno_(o.cityno_),
	gold_income_(o.gold_income_),
	technology_income_(o.technology_income_),
	food_income_(o.food_income_),
	miss_income_(o.miss_income_),
	heal_(o.heal_),
	turn_experience_(o.turn_experience_),
	food_(o.food_),
	block_turns_(o.block_turns_),
	hide_turns_(o.hide_turns_),
	alert_turns_(0), // set this later
	provoked_turns_(0), // set this later
	hot_turns_(o.hot_turns_),
	tactic_hero_(NULL), // set this later
	task_(TASK_NONE), // set this later
	human_(o.human_),
	commoner_(o.commoner_),
	path_along_road_(o.path_along_road_),
	artifical_(o.artifical_),
	can_recruit_(o.can_recruit_),
	can_reside_(o.can_reside_),
	wall_(o.wall_),
	fort_(o.fort_),
	transport_(o.transport_),
	land_wall_(o.land_wall_),
	walk_wall_(o.walk_wall_),
	terrain_(o.terrain_),
	arms_(o.arms_),
	master_(o.master_),
	second_(o.second_),
	third_(o.third_),
	master_number_(o.master_number_),
	second_number_(o.second_number_),
	third_number_(o.third_number_),
	leadership_(o.leadership_),
	force_(o.force_),
	intellect_(o.intellect_),
	spirit_(o.spirit_),
	charm_(o.charm_),
	characters_(o.characters_),
	adjacent_size_(o.adjacent_size_),
	adjacent_size_2_(o.adjacent_size_2_),
	adjacent_size_3_(o.adjacent_size_3_),
	base_resistance_(o.base_resistance_),
	move_spectator_(o.move_spectator_),
	guard_attack_(o.guard_attack_),
	formation_flag_(o.formation_flag_),
	temporary_state_(o.temporary_state_),
	attackable_rect_(o.attackable_rect_),
	tactic_compare_damage_(o.tactic_compare_damage_),
	tactic_compare_resistance_(o.tactic_compare_resistance_),
	tactic_compare_movement_(o.tactic_compare_movement_)
{
	if (o.alert_turns_) {
		set_alert_turns(o.alert_turns_);
	}
	if (o.provoked_turns_) {
		if (provoke_cache_ready) {
			set_provoked_turns_next(o, o.provoked_turns_);
		} else {
			provoked_turns_ = o.provoked_turns_;
		}
	}
	if (o.tactic_hero_) {
		set_tactic_hero(*o.tactic_hero_);
	}
	if (o.task_ == TASK_GUARD) {
		if (guard_cache::ready) {
			set_guard(guard_cache::find(o).loc);
		} else {
			task_ = o.task_;
		}
	} else {
		task_ = o.task_;
	}
	std::copy(o.adaptability_, o.adaptability_ + HEROS_MAX_ARMS, adaptability_);
	std::copy(o.skill_, o.skill_ + HEROS_MAX_SKILL, skill_);
	std::copy(o.feature_, o.feature_ + HEROS_FEATURE_M2BYTES, feature_);
	std::copy(o.adjacent_, o.adjacent_ + adjacent_size_, adjacent_);
	std::copy(o.adjacent_2_, o.adjacent_2_ + adjacent_size_2_, adjacent_2_);
	std::copy(o.adjacent_3_, o.adjacent_3_ + adjacent_size_3_, adjacent_3_);

	if (human_) {
		rpg::humans.insert(this);
	}
}
           
unit::unit(unit_map& units, hero_map& heros, std::vector<team>& teams, game_state& state, const config& cfg, bool use_traits, bool is_artifical) 
	: base_unit(units)
	, unit_merit(),
	units_(units),
	heros_(heros),
	teams_(teams),
	cfg_(cfg),
	advances_to_(),
	type_(cfg["type"]),
	packee_type_(),
	unit_type_(NULL),
	packee_unit_type_(NULL),
	especial_(-1),
	race_(NULL),
	type_name_(),
	undead_variation_(),
	variation_(cfg["variation"]),
	hit_points_(1),
	max_hit_points_(0),
	experience_(0),
	max_experience_(0),
	ticks_(NONE_TICKS),
	max_ticks_(0),
	drawn_ticks_(NONE_TICKS),
	ticks_increase_(0),
	level_(0),
	upkeep_(-1),
	alignment_(),
	flag_rgb_(),
	image_mods_(),
	traits_(),
	tactics_(),
	modifications_(),
	side_(0),
	gender_(unit_race::MALE),
	alpha_(),
	movement_(0),
	max_movement_(0),
	movement_costs_(),
	defense_mods_(),
	resting_(false),
	tactic_degree_(0),
	signature(global_signature ++),
	ticks_adjusting(false),
	attacks_left_(0),
	max_attacks_(0),
	states_(0),
	emit_zoc_(0),
	cancel_zoc_(false),
	attacks_(),
	trait_names_(),
	unit_value_(),
	goto_(),
	from_(),
	flying_(false),
	unit_halo_(halo::NO_HALO),
	hidden_(false),
	invisibility_cache_(),
	cityno_(-1),
	gold_income_(),
	technology_income_(0),
	food_income_(0),
	miss_income_(0),
	heal_(0),
	turn_experience_(0),
	food_(0),
	block_turns_(0),
	hide_turns_(0),
	alert_turns_(0),
	provoked_turns_(0),
	hot_turns_(0),
	tactic_hero_(NULL),
	task_(TASK_NONE),
	human_(false),
	commoner_(false),
	path_along_road_(false),
	artifical_(is_artifical),
	can_recruit_(false),
	can_reside_(false),
	wall_(false),
	fort_(false),
	transport_(false),
	land_wall_(true),
	walk_wall_(false),
	terrain_(t_translation::NONE_TERRAIN),
	arms_(0),
	adjacent_size_(0),
	adjacent_size_2_(0),
	adjacent_size_3_(0),
	base_resistance_(),
	move_spectator_(units_),
	guard_attack_(-2),
	formation_flag_(FORMATION_NONE),
	temporary_state_(0),
	tactic_compare_damage_(),
	tactic_compare_resistance_(),
	tactic_compare_movement_(0)
{
	if (type_.empty()) {
		throw game::game_error("creating unit with an empty type field");
	}

	if (cfg["cityno"].empty()) {
		throw game::load_game_failed("Attempt to create a unit with an empty 'cityno' field");
	}

	cityno_ = cfg["cityno"].to_int();
	if (cityno_ < 0) {
		throw game::load_game_failed("Attempt to create a unit with an invalid 'cityno' field");
	}

	side_ = cfg["side"];
	if (side_ <= 0) {
		side_ = units_.city_from_cityno(cityno_)->side();
	}
	
	validate_side(side_);

	// hero
	if (cfg_["heros_army"].empty()) {
		throw game::load_game_failed("Attempt to create a unit with an empty 'heros_army' field");
	}
	std::vector<std::string> vstr = utils::split(cfg_["heros_army"]);
	master_number_ = lexical_cast_default<int>(vstr[0]);
	master_ = &heros_[master_number_];
	master_->status_ = hero_status_military;

	gender_ = static_cast<unit_race::GENDER>(master_->gender_);
	if (vstr.size() > 1) {
		second_number_ = lexical_cast_default<int>(vstr[1]);
		second_ = &heros_[second_number_];
		second_->status_ = hero_status_military;
	} else {
		second_ = &hero_invalid;
	}
	if (vstr.size() > 2) {
		third_number_ = lexical_cast_default<int>(vstr[2]);
		third_ = &heros_[third_number_];
		third_->status_ = hero_status_military;
	} else {
		third_ = &hero_invalid;
	}
	
	facing_ = map_location::parse_direction(cfg["facing"]);
	if(facing_ == map_location::NDIRECTIONS) facing_ = map_location::SOUTH_EAST;

	if (const config &mods = cfg.child("modifications")) {
		VALIDATE(false, "Now don't support [modifications] in unit/artifical. check name: " + master_->name());
	}
	if (!cfg["traits"].empty()) {
		traits_ = utils::split(cfg["traits"].str());
		const traits_map& unit_traits = unit_types.traits();
		for (std::vector<std::string>::const_iterator it = traits_.begin(); it != traits_.end(); ++ it) {
			if (unit_traits.find(*it) == unit_traits.end()) {
				VALIDATE(false, "Don't support trait: " + *it);
			}
		}
	}
	modifications_ = utils::split(cfg["modifications"].str());

	unit_type_ = &get_unit_type(type_).get_gender_unit_type(gender_).get_variation(variation_);
	if (artifical_) {
		especial_ = unit_types.especial_from_id(cfg["especial"].str());
	} else {
		especial_ = unit_type_->especial();
	}
	
	advance_to(unit_type_, use_traits, &state);

	// set city_ and side_. only when is troop or city. don't set when artifical.
	if (!artifical_ || this_is_city()) {
		master_->city_ = cityno_;
		master_->side_ = side_ - 1;
		if (second_->valid()) {
			second_->city_ = cityno_;
			second_->side_ = side_ - 1;
		}
		if (third_->valid()) {
			third_->city_ = cityno_;
			third_->side_ = side_ - 1;
		}
	}

	calculate_5fields();
	modify_according_to_hero(true, true);

	if (const config::attribute_value *v = cfg.get("max_attacks")) {
		max_attacks_ = std::max(0, v->to_int(1));
	}
	attacks_left_ = std::max(0, cfg["attacks_left"].to_int(max_attacks_));

	if (const config::attribute_value *v = cfg.get("alpha")) {
		alpha_ = lexical_cast_default<fixed_t>(*v);
	}

	if (!cfg["states"].empty()) {
		std::vector<std::string> vstr = utils::split(cfg["states"].str());
		for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
			ustate_tag::state_t val = ustate_tag::find(*it);
			if (val != ustate_tag::NONE) {
				set_state(val, true);
			}
		}
	}

	if (const config &status_flags = cfg.child("status")) {
		BOOST_FOREACH (const config::attribute &st, status_flags.attribute_range()) {
			if (st.second.to_bool()) {
				set_state(find_state(st.first), true);
			}
		}
	}

	// Remove animations from private cfg, they're not needed there now
	BOOST_FOREACH (const std::string& tag_name, unit_animation::all_tag_names()) {
		cfg_.clear_children(tag_name);
	}

	if (const config::attribute_value *v = cfg.get("hitpoints")) {
		hit_points_ = *v;
	} else {
		hit_points_ = max_hit_points_;
	}

	goto_.x = cfg["goto_x"].to_int() - 1;
	goto_.y = cfg["goto_y"].to_int() - 1;

	if (const config::attribute_value *v = cfg.get("moves")) {
		movement_ = *v;
		if(movement_ < 0) {
			attacks_left_ = 0;
			movement_ = 0;
		}
	} else {
		movement_ = max_movement_;
	}
	experience_ = cfg["experience"];
	resting_ = cfg["resting"].to_bool();
	tactic_degree_ = cfg["tactic_degree"].to_int();

	// unit_merit
	field_turns_ = cfg["field_turns"].to_int();
	cause_damage_ = cfg["cause_damage"].to_int();
	been_damage_ = cfg["been_damage"].to_int();
	defeat_units_ = cfg["defeat_units"].to_int();

	if (cfg_.has_attribute("food")) {
		food_ = cfg_["food"].to_int();
	} else {
		food_ = max_food();
	}
	if (cfg_.has_attribute("block_turns")) {
		block_turns_ = cfg_["block_turns"].to_int();
	}
	if (cfg_.has_attribute("task")) {
		task_ = cfg_["task"].to_int();
	}
	if (cfg_.has_attribute("human")) {
		human_ = cfg_["human"].to_bool();
	}

	/** @todo Are these modified by read? if not they can be removed. */
	refreshing_  = false;
	hidden_ = false;
	game_config::add_color_info(cfg);

	static char const *internalized_attrs[] = { "type", "name",
		"gender", "states",
		"side", "facing", "race",
		"level", "undead_variation", "max_attacks",
		"attacks_left", "alpha", "flying", "cost",
		"max_hitpoints", "max_moves", "max_experience",
		"advances_to", "hitpoints", "goto_x", "goto_y", "moves",
		"experience", "resting", "alignment",
		"x", "y"};
	BOOST_FOREACH (const char *attr, internalized_attrs) {
		cfg_.remove_attribute(attr);
	}

	if (human_) {
		rpg::humans.insert(this);
	}
	commoner_ = !artifical_ && (unit_type_->master() != HEROS_INVALID_NUMBER);
	path_along_road_ = commoner_ && !transport_;
}

unit::unit(unit_map& units, hero_map& heros, std::vector<team>& teams, const uint8_t* mem, bool use_traits, bool artifical) 
	: base_unit(units),
	unit_merit(),
	units_(units),
	heros_(heros),
	teams_(teams),
	cfg_(),
	advances_to_(),
	type_(),
	packee_type_(),
	unit_type_(NULL),
	packee_unit_type_(NULL),
	especial_(-1),
	race_(NULL),
	type_name_(),
	undead_variation_(),
	variation_(),
	hit_points_(1),
	max_hit_points_(0),
	experience_(0),
	max_experience_(0),
	level_(0),
	upkeep_(-1),
	alignment_(),
	flag_rgb_(),
	image_mods_(),
	traits_(),
	tactics_(),
	modifications_(),
	side_(0),
	gender_(unit_race::MALE),
	alpha_(),
	movement_(0),
	max_movement_(0),
	ticks_(NONE_TICKS),
	max_ticks_(0),
	drawn_ticks_(NONE_TICKS),
	ticks_increase_(0),
	movement_costs_(),
	defense_mods_(),
	resting_(false),
	tactic_degree_(0),
	signature(0), // signature is from to mem.
	ticks_adjusting(false),
	attacks_left_(0),
	max_attacks_(0),
	states_(0),
	emit_zoc_(0),
	cancel_zoc_(false),
	attacks_(),
	trait_names_(),
	unit_value_(),
	goto_(),
	from_(),
	flying_(false),
	unit_halo_(halo::NO_HALO),
	hidden_(false),
	invisibility_cache_(),
	cityno_(-1),
	gold_income_(),
	technology_income_(0),
	food_income_(0),
	miss_income_(0),
	heal_(0),
	turn_experience_(0),
	food_(0),
	block_turns_(0),
	hide_turns_(0),
	alert_turns_(0),
	provoked_turns_(0),
	hot_turns_(0),
	tactic_hero_(NULL),
	task_(TASK_NONE),
	human_(false),
	commoner_(false),
	path_along_road_(false),
	artifical_(artifical),
	can_recruit_(false),
	can_reside_(false),
	wall_(false),
	fort_(false),
	transport_(false),
	land_wall_(true),
	walk_wall_(false),
	terrain_(t_translation::NONE_TERRAIN),
	arms_(0),
	adjacent_size_(0),
	adjacent_size_2_(0),
	adjacent_size_3_(0),
	base_resistance_(),
	move_spectator_(units_),
	guard_attack_(-2),
	formation_flag_(FORMATION_NONE),
	temporary_state_(0),
	tactic_compare_damage_(),
	tactic_compare_resistance_(),
	tactic_compare_movement_(0)
{
	unit_fields_t* fields = (unit_fields_t*)mem;

	// first read, read necessary fields for advance_to(...)
	read(mem, false);

	if (type_.empty()) {
		throw game::game_error("creating unit with an empty type field");
	}

	if (cityno_ < 0) {
		throw game::load_game_failed("Attempt to create a unit with an invalid 'cityno' field");
	}

	if (side_ <= 0) {
		side_ = units_.city_from_cityno(cityno_)->side();
	}
	
	validate_side(side_);

	master_number_ = master_->number_;
	master_->status_ = hero_status_military;

	gender_ = static_cast<unit_race::GENDER>(master_->gender_);
	if (second_->valid()) {
		second_number_ = second_->number_;
		second_->status_ = hero_status_military;
	}

	if (third_->valid()) {
		third_number_ = third_->number_;
		third_->status_ = hero_status_military;
	}

	if (facing_ == map_location::NDIRECTIONS) {
		facing_ = map_location::SOUTH_EAST;
	}

	unit_type_ = &get_unit_type(type_).get_gender_unit_type(gender_).get_variation(variation_);
	advance_to(unit_type_, use_traits, NULL);

	calculate_5fields();
	modify_according_to_hero(true, true);

	// it is time to read full fields.
	read(mem);
	
	// set city_ and side_. only when is troop or city. don't set when artifical.
	if (!artifical_ || this_is_city()) {
		master_->city_ = cityno_;
		master_->side_ = side_ - 1;
		if (second_->valid()) {
			second_->city_ = cityno_;
			second_->side_ = side_ - 1;
		}
		if (third_->valid()) {
			third_->city_ = cityno_;
			third_->side_ = side_ - 1;
		}
	}

	// Remove animations from private cfg, they're not needed there now
	BOOST_FOREACH (const std::string& tag_name, unit_animation::all_tag_names()) {
		cfg_.clear_children(tag_name);
	}

	/** @todo Are these modified by read? if not they can be removed. */
	refreshing_  = false;
	hidden_ = false;
	// game_config::add_color_info(cfg);

	if (human_) {
		rpg::humans.insert(this);
	}
	commoner_ = !artifical_ && (unit_type_->master() != HEROS_INVALID_NUMBER);
	path_along_road_ = commoner_ && !transport_;
}

void unit::clear_status_caches()
{
	for(std::vector<const unit *>::const_iterator itor = units_with_cache.begin();
			itor != units_with_cache.end(); ++itor) {
		(*itor)->clear_visibility_cache();
	}

	units_with_cache.clear();
}

unit::unit(unit_map& units, hero_map& heros, std::vector<team>& teams, game_state& state, type_heros_pair& t, int cityno, bool real_unit, bool is_artifical) 
	: base_unit(units),
	unit_merit(),
	units_(units),
	heros_(heros),
	teams_(teams),
	cfg_(),
	advances_to_(),
	type_(),
	packee_type_(),
	especial_(-1),
	unit_type_(NULL),
	packee_unit_type_(NULL),
	race_(NULL),
	type_name_(),
	undead_variation_(),
	variation_(),
	hit_points_(0),
	max_hit_points_(0),
	experience_(0),
	max_experience_(0),
	ticks_(NONE_TICKS),
	max_ticks_(0),
	drawn_ticks_(NONE_TICKS),
	ticks_increase_(0),
	level_(0),
	upkeep_(-1),
	alignment_(),
	flag_rgb_(),
	image_mods_(),
	traits_(),
	tactics_(),
	modifications_(),
	side_(0),
	alpha_(),
	movement_(0),
	max_movement_(0),
	movement_costs_(),
	defense_mods_(),
	resting_(false),
	tactic_degree_(0),
	signature(global_signature ++),
	ticks_adjusting(false),
	attacks_left_(0),
	max_attacks_(0),
	states_(0),
	emit_zoc_(0),
	cancel_zoc_(false),
	attacks_(),
	trait_names_(),
	unit_value_(),
	goto_(),
	from_(),
	flying_(false),
	unit_halo_(halo::NO_HALO),
	hidden_(false),
	invisibility_cache_(),
	cityno_(cityno),
	gold_income_(0),
	technology_income_(0),
	food_income_(0),
	miss_income_(0),
	heal_(0),
	turn_experience_(0),
	food_(0),
	block_turns_(0),
	hide_turns_(0),
	alert_turns_(0),
	provoked_turns_(0),
	hot_turns_(0),
	tactic_hero_(NULL),
	task_(TASK_NONE),
	human_(false),
	commoner_(false),
	path_along_road_(false),
	artifical_(is_artifical),
	can_recruit_(false),
	can_reside_(false),
	wall_(false),
	fort_(false),
	transport_(false),
	land_wall_(true),
	walk_wall_(false),
	terrain_(t_translation::NONE_TERRAIN),
	arms_(0),
	adjacent_size_(0),
	adjacent_size_2_(0),
	adjacent_size_3_(0),
	base_resistance_(),
	move_spectator_(units_),
	guard_attack_(-2),
	formation_flag_(FORMATION_NONE),
	temporary_state_(0),
	tactic_compare_damage_(),
	tactic_compare_resistance_(),
	tactic_compare_movement_(0)
{
	if (cityno != HEROS_ROAM_CITY) {
		side_ = units.city_from_cityno(cityno)->side();
	} else {
		side_ = team::empty_side;
	}

	master_ = const_cast<hero*>(t.second[0]);
	master_number_ = master_->number_;
	master_->status_ = hero_status_military;
	if (t.second.size() > 1) {
		second_ = const_cast<hero*>(t.second[1]);
		second_number_ = second_->number_;
		second_->status_ = hero_status_military;
	} else {
		second_ = &hero_invalid;
	}
	if (t.second.size() > 2) {
		third_ = const_cast<hero*>(t.second[2]);
		third_number_ = third_->number_;
		third_->status_ = hero_status_military;
	} else {
		third_ = &hero_invalid;
	}
	
	gender_ = static_cast<unit_race::GENDER>(master_->gender_);

	unit_type_ = t.first;

	// first from desired unit type, if no, from city.
	especial_ = unit_type_->especial();
	if (cityno != HEROS_ROAM_CITY && unit_type_->especial() == NO_ESPECIAL) {
		artifical& city = *units_.city_from_cityno(cityno_);
		especial_ = city.especial();
	}

	advance_to(unit_type_, real_unit, &state);

	// fill those after traits and modifs to have correct max
	movement_ = max_movement_;
	hit_points_ = max_hit_points_;
	attacks_left_ = max_attacks_;

	/**
	 * @todo Test whether the calls above modify these values if not they can
	 * removed, since already set in the initialization list.
	 */
	anim_ = NULL;
	next_idling_ = 0;
	frame_begin_time_ = 0;
	unit_halo_ = halo::NO_HALO;

	calculate_5fields();
	modify_according_to_hero(true, true);

	if (human_) {
		rpg::humans.insert(this);
	}
	commoner_ = !artifical_ && (unit_type_->master() != HEROS_INVALID_NUMBER);
	path_along_road_ = commoner_ && !transport_;
}

unit::~unit()
{
	clear_haloes();

	// Remove us from rpg::humans
	if (human_) {
		size_t s = rpg::humans.erase(this);
		VALIDATE(s, "unit::~unit, this is human, but cannot find it in rpg::humans");
	}

	// Remove us from the status cache
	std::vector<const unit *>::iterator itor =std::find(units_with_cache.begin(), units_with_cache.end(), this);
	if (itor != units_with_cache.end()) {
		units_with_cache.erase(itor);
	}

	// Remove us from this alert cache
	std::map<unit*, int>::iterator it = alert_cache.find(this);
	if (it != alert_cache.end()) {
		alert_cache.erase(it);
	}

	// Remove us from provoke cache
	remove_from_provoke_cache();

	// Remove us from slot cache
	remove_from_slot_cache();

	// Remove us from guard cache
	remove_from_guard_cache();

	if (this == actor) {
		actor = NULL;
	}
}

unit& unit::operator=(const unit& u)
{
	// Use copy constructor to make sure we are coherant
	if (this != &u) {
		this->~unit();
		new (this) unit(u) ;
	}
	return *this ;
}

#define has_state_flag(s)		(states_ & s)
#define set_state_flag(s)		(states_ |= s)
#define clear_state_flag(s)		(states_ &= ~s)

#define has_field_bool_flag(field, s)		((field) & s)
#define set_field_bool_flag(field, s)		((field) |= s)
#define clear_field_bool_flag(field, s)		((field) &= ~s)

#define set_field_bool(field, flag, value)	(value)? (field) |= flag: (field) &= ~(flag)
#define get_field_bool						has_field_bool_flag

void unit::write(uint8_t* mem) const
{
	unit_fields_t* fields = (unit_fields_t*)mem;

	fields->states_ = states_;
	if (this_is_city()) {
		fields->artifical_ = 2;
	} else if (artifical_) {
		fields->artifical_ = 1;
	} else {
		fields->artifical_ = 0;
	}
	fields->been_damage_ = been_damage_;
	fields->cause_damage_ = cause_damage_;
	fields->defeat_units_ = defeat_units_;
	fields->field_turns_ = field_turns_;
	fields->hitpoints_ = hit_points_;
	fields->experience_ = experience_;
	fields->moves_ = movement_;
	fields->attacks_left_ = attacks_left_;
	fields->side_ = side_;
	fields->cityno_ = cityno_;
	fields->x_ = loc_.x;
	fields->y_ = loc_.y;
	fields->goto_x_ = goto_.x;
	fields->goto_y_ = goto_.y;
	fields->from_x_ = from_.x;
	fields->from_y_ = from_.y;
	fields->facing_ = facing_;
	fields->food_ = food_;
	fields->block_turns_ = block_turns_;
	fields->hide_turns_ = hide_turns_;
	fields->alert_turns_ = alert_turns_;
	fields->provoked_turns_ = provoked_turns_;
	fields->hot_turns_ = hot_turns_;
	fields->tactic_hero_ = tactic_hero_? tactic_hero_->number_: HEROS_INVALID_NUMBER;
	fields->task_ = task_;
	fields->especial_ = especial_;
	fields->ticks_ = ticks_;
	fields->bools_ = 0; // memory block doesn't been zero!
	set_field_bool(fields->bools_, FIELD_HUMAN, human_);
	set_field_bool(fields->bools_, FIELD_RESTING, resting_);
	fields->tactic_degree_ = tactic_degree_;
	fields->signature_ = signature;

	int offset;
	if (artifical_) {
		offset = sizeof(artifical_fields_t);
	} else {
		offset = sizeof(unit_fields_t);
	}
	std::string str;

	// tactics
	fields->tactics_.offset_ = offset;
	fields->tactics_.size_ = tactics_.size();
	offset += sizeof(tactic_fields) * fields->tactics_.size_;
	for (int i = 0; i < fields->tactics_.size_; i ++) {
		const tunit_tactic& t = tactics_[i];
		tactic_fields* t_fields = (tactic_fields*)(mem + fields->tactics_.offset_) + i;

		t_fields->t_ = t.tactic_->index();
		t_fields->turn_ = t.turn_;
		t_fields->part_ = t.part_;
	}
/*
	// attack
	fields->attacks_.offset_ = offset;
	fields->attacks_.size_ = attacks_.size();
	offset += sizeof(attack_fields) * fields->attacks_.size_;

	for (int i = 0; i < fields->attacks_.size_; i ++) {
		const attack_type& attack = attacks_[i];
		attack_fields* a_fields = (attack_fields*)(mem + fields->attacks_.offset_) + i;
		// id
		if (!attack.id().empty()) {
			a_fields->id_.offset_ = offset; 
			a_fields->id_.size_ = attack.id().length();
			memcpy(mem + offset, attack.id().c_str(), a_fields->id_.size_);
		} else {
			a_fields->id_.offset_ = -1;
			a_fields->id_.size_ = 0;
		}
		offset += a_fields->id_.size_;
		// type
		if (!attack.type().empty()) {
			a_fields->type_.offset_ = offset; 
			a_fields->type_.size_ = attack.type().length();
			memcpy(mem + offset, attack.type().c_str(), a_fields->type_.size_);
		} else {
			a_fields->type_.offset_ = -1;
			a_fields->type_.size_ = 0;
		}
		offset += a_fields->type_.size_;
		// icon
		if (!attack.icon().empty()) {
			a_fields->icon_.offset_ = offset; 
			a_fields->icon_.size_ = attack.icon().length();
			memcpy(mem + offset, attack.icon().c_str(), a_fields->icon_.size_);
		} else {
			a_fields->icon_.offset_ = -1;
			a_fields->icon_.size_ = 0;
		}
		offset += a_fields->icon_.size_;
		// specials
		if (!attack.specials().empty()) {
			a_fields->specials_.offset_ = offset; 
			a_fields->specials_.size_ = attack.specials().length();
			memcpy(mem + offset, attack.specials().c_str(), a_fields->specials_.size_);
		} else {
			a_fields->specials_.offset_ = -1;
			a_fields->specials_.size_ = 0;
		}
		offset += a_fields->specials_.size_;

		const std::string& range = attack.range();
		if (range == "melee") {
			a_fields->range_ = attack_type::MELEE;
		} else if (range == "ranged") {
			a_fields->range_ = attack_type::RANGED;
		} else {
			a_fields->range_ = attack_type::CAST;
		}
		a_fields->damage_ = attack.damage();
		a_fields->number_ = attack.num_attacks();
	}
*/
	// heros_army
	fields->heros_army_.offset_ = offset;
	memcpy(mem + offset, &(master_->number_), 2);
	fields->heros_army_.size_ = 2;
	offset += 2;
	if (second_->valid()) {
		memcpy(mem + offset, &(second_->number_), 2);
		fields->heros_army_.size_ += 2;
		offset += 2;
	}
	if (third_->valid()) {
		memcpy(mem + offset, &(third_->number_), 2);
		fields->heros_army_.size_ += 2;
		offset += 2;
	}
	// type
	if (packed()) {
		str = packee_type_;
	} else {
		str = type_;
	}
	fields->type_.offset_ = offset; 
	fields->type_.size_ = str.length();
	memcpy(mem + offset, str.c_str(), fields->type_.size_);
	offset += fields->type_.size_;

	// traits
	str = utils::join(traits_);
	if (!str.empty()) {
		fields->traits_.offset_ = offset; 
		fields->traits_.size_ = str.length();
		memcpy(mem + offset, str.c_str(), fields->traits_.size_);
	} else {
		fields->traits_.offset_ = -1;
		fields->traits_.size_ = 0;
	}
	offset += fields->traits_.size_;
	// modifications
	str = utils::join(modifications_);
	if (!str.empty()) {
		fields->modifications_.offset_ = offset; 
		fields->modifications_.size_ = str.length();
		memcpy(mem + offset, str.c_str(), fields->modifications_.size_);
	} else {
		fields->modifications_.offset_ = -1;
		fields->modifications_.size_ = 0;
	}
	offset += fields->modifications_.size_;

	// align 4
	offset = (offset + 3) & ~3;

	// total size
	fields->size_ = offset;
}

void unit::read(const uint8_t* mem, bool lower)
{
	unit_fields_t* fields = (unit_fields_t*)mem;
	
	side_ = fields->side_;
	cityno_ = fields->cityno_;

	std::string str;
	int offset;
	if (artifical_) {
		offset = sizeof(artifical_fields_t);
	} else {
		offset = sizeof(unit_fields_t);
	}
	uint16_t val16;

	if (!lower) {
		// type_
		type_.assign((const char*)mem + fields->type_.offset_, fields->type_.size_);

		// heros_army
		memcpy(&val16, mem + fields->heros_army_.offset_, 2);
		master_ = &heros_[val16];
		if (fields->heros_army_.size_ > 2) {
			memcpy(&val16, mem + fields->heros_army_.offset_ + 2, 2);
			second_ = &heros_[val16];
		} else {
			second_ = &hero_invalid;
		}
		if (fields->heros_army_.size_ > 4) {
			memcpy(&val16, mem + fields->heros_army_.offset_ + 4, 2);
			third_ = &heros_[val16];
		} else {
			third_ = &hero_invalid;
		}

		// fllowing advances_to(...) used it!
		especial_ = fields->especial_;

		ticks_ = fields->ticks_;

		facing_ = (map_location::DIRECTION)fields->facing_;

		// traits
		str.assign((const char*)mem + fields->traits_.offset_, fields->traits_.size_);
		traits_ = utils::split(str);

		// tactics
		tactics_.clear();
		for (int i = 0; i < fields->tactics_.size_; i ++) {
			tactic_fields* t_fields = (tactic_fields*)(mem + fields->tactics_.offset_) + i;
			tactics_.push_back(tunit_tactic(&unit_types.tactic(t_fields->t_), t_fields->turn_, t_fields->part_));
		}

		// modifications
		str.assign((const char*)mem + fields->modifications_.offset_, fields->modifications_.size_);
		modifications_ = utils::split(str);
		return;
	}

	states_ = fields->states_;
	been_damage_ = fields->been_damage_;
	cause_damage_ = fields->cause_damage_;
	defeat_units_ = fields->defeat_units_;
	field_turns_ = fields->field_turns_;
	hit_points_ = fields->hitpoints_;
	experience_ = fields->experience_;
	movement_ = fields->moves_;
	attacks_left_ = fields->attacks_left_;
	// in order to validate set_location, don't set to fields->x/fields->y, invalidate loc_.
	map_location loc;
	loc_.x = loc.x;
	loc_.y = loc.y;
	goto_.x = fields->goto_x_;
	goto_.y = fields->goto_y_;
	from_.x = fields->from_x_;
	from_.y = fields->from_y_;
	food_ = fields->food_;
	block_turns_ = fields->block_turns_;
	hide_turns_ = fields->hide_turns_;
	set_alert_turns(fields->alert_turns_);
	provoked_turns_ = fields->provoked_turns_;
	hot_turns_ = fields->hot_turns_;
	tactic_hero_ = (fields->tactic_hero_ != HEROS_INVALID_NUMBER)? &heros_[fields->tactic_hero_]: NULL;
	task_ = fields->task_;
	human_ = get_field_bool(fields->bools_, FIELD_HUMAN);
	resting_ = get_field_bool(fields->bools_, FIELD_RESTING);
	tactic_degree_ = fields->tactic_degree_;
	signature = fields->signature_;
}

void unit::delete_anim()
{
	if (anim_) {
		delete anim_;
		anim_ = NULL;
	}
}


// Apply mandatory traits (e.g. undead, mechanical) to a unit and then
// fill out with avaiable (leaders have a restircted set of available traits)
// traits until no more are available or the unit has its maximum number
// of traits.
// This routine does not apply the effects of added traits to a unit.
// That must be done by the caller.
// Note that random numbers used in config files don't work in multiplayer,
// so that leaders should be barred from all random traits until that
// is fixed. Later the restrictions will be based on play balance.
// musthavepnly is true when you don't want to generate random traits or
// you don't want to give any optional traits to a unit.

void unit::generate_traits(bool musthaveonly, game_state* state)
{
	const unit_type *type = unit_types.find(type_id());
	// Calculate the unit's traits
	if (!type) {
		std::string error_message = _("Unknown unit type '$type|' while generating traits");
		utils::string_map symbols;
		symbols["type"] = type_id();
		error_message = utils::interpolate_variables_into_string(error_message, &symbols);
		throw game::game_error(error_message);
	}

	std::vector<std::string> candidate_traits;

	const std::vector<const config*>& possible_traits = type->possible_traits();
	for (std::vector<const config*>::const_iterator it = possible_traits.begin(); it != possible_traits.end(); ++ it) {
		const config& t = **it;
		// Skip the trait if the unit already has it.
		const std::string &tid = t["id"];
		if (std::find(traits_.begin(), traits_.end(), tid) != traits_.end()) {
			continue;
		}

		// Add the trait if it is mandatory.
		const std::string& avl = t["availability"];
		if (avl == "musthave") {
			traits_.push_back(tid);
			continue;
		}
		
		const std::string& filter = t["filter"];
		if (!filter.empty()) {
			if (filter == "moveable") {
				if (!max_movement_) {
					continue;
				}
			} else if (filter == "melee" || filter == "ranged" || filter == "cast") {
				int w = weapon(null_str, filter);
				if (w < 0 || attacks_[w].damage() < game_config::min_valid_damage) {
					continue;
				}
			}
		}
		
		// The trait is still available, mark it as a candidate for randomizing.
		if (!musthaveonly) {
			candidate_traits.push_back(tid);
		}
	}

	if (musthaveonly) return;

	// Now randomly fill out to the number of traits required or until
	// there aren't any more traits.
	int nb_traits = traits_.size();
	int max_traits = type->num_traits();
	for (; nb_traits < max_traits && !candidate_traits.empty(); ++nb_traits) {
		int num = state->rng().get_next_random() % candidate_traits.size();
		traits_.push_back(candidate_traits[num]);
		candidate_traits.erase(candidate_traits.begin() + num);
	}
}

std::vector<std::string> unit::get_traits_list() const
{
	return traits_;
}

const std::vector<std::string>& unit::advances_to() const 
{ 
	return advances_to_;
}

// t must be packer=no unit type
void unit::advance_to(const unit_type *t, bool use_random_traits, game_state *state)
{
	VALIDATE(!t->packer(), "unit::advance_to, packer unit_type: " + t->id());

	const std::string mod_childs[] = {"movement_costs", "defense", "resistance"};

	// Clear modification-related caches
	movement_costs_.clear();
	defense_mods_.clear();

	// Clear modified configs
	BOOST_FOREACH (const std::string& tag, mod_childs) {
		cfg_.clear_children(tag);
	}
	// Remove old type's halo, animations, abilities, attacks and advancement options (AMLA)
	BOOST_FOREACH (const std::string& tag_name, unit_animation::all_tag_names()) {
		cfg_.clear_children(tag_name);
	}

	if(t->movement_type().get_parent()) {
		cfg_.merge_with(t->movement_type().get_parent()->get_cfg());
	}
	cfg_.merge_with(t->get_cfg());

	// animation tags is more, don't write it to save file
	BOOST_FOREACH (const std::string& tag_name, unit_animation::all_tag_names()) {
		cfg_.clear_children(tag_name);
	}

	cfg_.clear_children("male");
	cfg_.clear_children("female");

	advances_to_ = t->advances_to(especial_);

	race_ = t->race_;
	type_name_ = t->type_name();
	undead_variation_ = t->undead_variation();
	max_experience_ = t->experience_needed(false);
	if (desc_rect_ && level_ != t->level()) {
		desc_rect_.assign(NULL);
	}
	level_ = t->level();
	alignment_ = t->alignment();
	alpha_ = t->alpha();
	// hit_points_ = t->hitpoints();
	max_hit_points_ = t->hitpoints();
	max_movement_ = t->movement();
	emit_zoc_ = t->has_zoc();
	cancel_zoc_ = t->cancel_zoc();
	attacks_ = t->attacks();
	unit_value_ = t->cost();
	gold_income_ = t->gold_income();
	technology_income_ = t->technology_income();
	food_income_ = t->food_income();
	miss_income_ = t->miss_income();
	heal_ = t->heal();
	turn_experience_ = t->turn_experience();
	flying_ = t->movement_type().is_flying();

	max_attacks_ = t->max_attacks();

	can_recruit_ = t->can_recruit_;
	can_reside_ = t->can_reside_;
	base_ = t->base_;
	wall_ = t->wall_;
	fort_ = t->fort_;
	transport_ = t->transport_;
	land_wall_ = t->land_wall_;
	walk_wall_ = t->walk_wall_;
	arms_ = t->arms_;
	terrain_ = t->terrain_;

	flag_rgb_ = t->flag_rgb();

	type_ = t->id();
	unit_type_ = t;

	generate_traits(!use_random_traits, state);

	// anim
	static char const* anim_attrs[] = { "die_sound" };
	BOOST_FOREACH (const char *attr, anim_attrs) {
		cfg_.remove_attribute(attr);
	}

	set_state(ustate_tag::POISONED, false);
	set_state(ustate_tag::SLOWED, false);
	set_state(ustate_tag::BROKEN, false);
	set_state(ustate_tag::PETRIFIED, false);
	refreshing_  = false;
	hidden_ = false;

	// set now to "base" status.
	config::child_itors cfg_range = cfg_.child_range("resistance");
	if (cfg_range.first != cfg_range.second) {
		// only one [resistance] block in one unit
		base_resistance_ = *cfg_range.first;
	}
}

void unit::pack_to(const unit_type* to)
{
	if (is_artifical()) {
		return;
	}

	const unit_type* t;
	const unit_type* original_type = type();

	if (to) {
		t = to;
		if (!original_type->packer()) {
			packee_type_ = type_;
			packee_unit_type_ = unit_type_;
		}
		max_hit_points_ = packee_unit_type_->hitpoints();
		max_experience_ = packee_unit_type_->experience_needed(false);
		max_movement_ = packee_unit_type_->movement();
		attacks_ = packee_unit_type_->attacks();
	} else {
		t = packee_unit_type_;
		packee_type_ = "";
		packee_unit_type_ = NULL;
		max_hit_points_ = t->hitpoints();
		max_experience_ = t->experience_needed(false);
		max_movement_ = t->movement();
		attacks_ = t->attacks();
	}
	type_ = t->id();
	unit_type_ = t;

	const std::string mod_childs[] = {"resistance"};

	// Clear modification-related caches
	movement_costs_.clear();
	defense_mods_.clear();

	// Clear modified configs
	BOOST_FOREACH (const std::string& tag, mod_childs) {
		cfg_.clear_children(tag);
	}
	// Remove old type's halo, animations, abilities, attacks and advancement options (AMLA)
	BOOST_FOREACH (const std::string& tag_name, unit_animation::all_tag_names()) {
		cfg_.clear_children(tag_name);
	}

	// if (t->movement_type().get_parent() && (const config& resistance = t->movement_type().get_parent()->get_cfg().child("resistance"))) {
	const unit_movement_type* movement_type = t->movement_type().get_parent();
	config* resistance_cfg;
	if (movement_type && movement_type->get_cfg().child("resistance")) {
		// cfg_.merge_with(t->movement_type().get_parent()->get_cfg());
		resistance_cfg = &cfg_.add_child("resistance", movement_type->get_cfg().child("resistance"));
	} else {
		// "before" is default resistance
		resistance_cfg = &cfg_.add_child("resistance", base_resistance_);
	}

	// animation tags is more, don't write it to save file
	BOOST_FOREACH (const std::string& tag_name, unit_animation::all_tag_names()) {
		cfg_.clear_children(tag_name);
	}

	race_ = t->race_;
	type_name_ = t->type_name();
	undead_variation_ = t->undead_variation();
	alignment_ = t->alignment();
	alpha_ = t->alpha();
	emit_zoc_ = t->has_zoc();
	cancel_zoc_ = t->cancel_zoc();
	flying_ = t->movement_type().is_flying();
	arms_ = t->arms_;

	flag_rgb_ = t->flag_rgb();

	if (to) {
		if (const config& pack = t->get_cfg().child("pack")) {
			add_modification(pack, true);
		}
	}

	// modify cfg_
	if (const config& resistance = t->get_cfg().child("resistance")) {
		resistance_cfg->merge_with(resistance);
	}

	// [pack] maybe update max_hit_points, max_experience, max_movement,
	// attack damage and resistance, set now to "base" status.
	base_resistance_ = *resistance_cfg;

	calculate_5fields();
	modify_according_to_hero(false, false);
}

void unit::change_to(game_display& gui, const unit_type* t)
{
	bool unit_is_packed = packed();
	std::string original_type = type_id();

	if (!t->advances_to().empty()) {
		modifications_.clear();
	}
	especial_ = t->especial();
	advance_to(t);
	calculate_5fields();
	modify_according_to_hero(false);

	if (unit_is_packed) {
		pack_to(unit_types.find(original_type));
	}

	rect_of_hexes& draw_area = gui.draw_area();
	bool force_scroll = preferences::scroll_to_action();
	bool animate = force_scroll || point_in_rect_of_hexes(get_location().x, get_location().y, draw_area);
	if (!gui.video().update_locked() && animate) {
		unit_animator animator;
		bool with_bars = true;
		animator.add_animation(this, "levelout", get_location(), map_location(), 0, with_bars);
		animator.start_animations();
		animator.wait_for_end(*resources::controller);
		set_standing();
	}
}

void unit::increase_food(int addend)
{
	food_ += addend;
	if (food_ < 0) {
		food_ = 0;
	} else if (!artifical_ && food_ > max_food()) {
		food_ = max_food();
	}
}

bool unit::lack_of_food() const
{
	return !artifical_ && !commoner_ && !food_;
}

int unit::max_food() const
{
	if (unit_feature_val(hero_feature_granary)) {
		return game_config::max_troop_food * 3 / 2;
	}
	return game_config::max_troop_food;
}

int unit::value_consider_food(int value, bool decrease) const
{
	if (!lack_of_food() || unit_feature_val(hero_feature_inspirit)) {
		return value;
	}
	if (decrease) {
		value = value * 70 / 100;
	} else {
		value = value * 100 / 70;
	}
	return value;
}

// if increase is true, attempt to increase block_turns, else clear block_turns
void unit::inching_block_turns(bool increase)
{
	VALIDATE(is_commoner(), "inching_block_turns, block unit must be commoner!");

	if (increase) {
		if (!get_state(ustate_tag::BLOCKED) && total_movement() == movement_left()) {
			block_turns_ ++;
			set_state(ustate_tag::BLOCKED, true);
		}
	} else if (block_turns_) {
		block_turns_ = 0;
	}
}

int unit::max_range() const
{
	int max = -1;
	std::map<std::string, int> range_map;
	range_map.insert(std::make_pair("melee", 1));
	range_map.insert(std::make_pair("ranged", 2));
	range_map.insert(std::make_pair("cast", 3));

	for (std::vector<attack_type>::const_iterator it = attacks_.begin(); it != attacks_.end(); ++ it) {
		if (!it->attack_weight()) {
			continue;
		}
		std::map<std::string, int>::iterator find = range_map.find(it->range());
		if (find->second > max) {
			max = find->second;
		}
	}
	return max;
}

void unit::set_alert_turns(int turns) 
{
	if (alert_turns_ == turns) {
		return;
	}
	if (alert_turns_ == 0) {
		// Insert us into the alert cache
		std::map<unit*, int>::iterator it = alert_cache.find(this);
		VALIDATE(it == alert_cache.end(), "unit::set_alert_turns, insert, but find this in alert cache!");

		alert_cache.insert(std::make_pair(this, max_range()));

	} else if (turns == 0) {
		// Remove us from the alert cache
		std::map<unit*, int>::iterator it = alert_cache.find(this);
		VALIDATE(it != alert_cache.end(), "unit::set_alert_turns, erase, but cannnot find this in alert cache!");
		alert_cache.erase(it);
	}

	alert_turns_ = turns;
}

void unit::do_provoked(unit& tactician, int turns)
{
	if (provoked_turns_) {
		// Only one tactician to a provoked. if exist, erase it before set.
		set_provoked_turns_next(*this, 0);
	} else {
		// provoked cannot is tactician.
		std::multimap<unit*, unit*>::iterator it = provoke_cache.find(this);
		while (it != provoke_cache.end()) {
			it->second->set_provoked_turns(*it->first, 0);
			it = provoke_cache.find(this);
		}
	}
	set_provoked_turns(tactician, turns);
}

void unit::set_provoked_turns_next(const unit& provoked, int turns)
{
	std::multimap<unit*, unit*>::iterator it;
	for (it = provoke_cache.begin(); it != provoke_cache.end(); ++ it) {
		if (it->second == &provoked) {
			break;
		}
	}
	set_provoked_turns(*it->first, turns);
}

void unit::set_provoked_turns(unit& tactician, int turns) 
{
	if (provoked_turns_ == turns) {
		return;
	}
	if (provoke_cache_ready) {
		if (provoked_turns_ == 0) {
			// Insert us into the provoke cache
			for (std::multimap<unit*, unit*>::iterator it = provoke_cache.begin(); it != provoke_cache.end(); ++ it) {
				VALIDATE(it->second != this, "unit::set_provoked_turns, insert, but find this in provoke cache!");
			}
			provoke_cache.insert(std::make_pair(&tactician, this));


		} else if (turns == 0) {
			// Remove us from the provoke cache
			std::multimap<unit*, unit*>::iterator it;
			for (it = provoke_cache.begin(); it != provoke_cache.end(); ++ it) {
				if (it->first == &tactician && it->second == this) {
					break;
				}
			}
			VALIDATE(it != provoke_cache.end(), "unit::set_provoke_turns, erase, but cannnot find <this, target> pair in provoke cache!");
			provoke_cache.erase(it);
			set_goto(map_location());
		}
	}
	provoked_turns_ = turns;
}

void unit::remove_from_provoke_cache()
{
	// Remove us from this provoke cache
	if (provoked_turns_) {
		for (std::multimap<unit*, unit*>::iterator it = provoke_cache.begin(); it != provoke_cache.end(); ++ it) {
			if (it->second == this) {
				provoke_cache.erase(it);
				break;
			}
		}
		provoked_turns_ = 0;
	} else {
		std::multimap<unit*, unit*>::iterator it = provoke_cache.find(this);
		while (it != provoke_cache.end()) {
			it->second->set_provoked_turns(*it->first, 0);
			it = provoke_cache.find(this);
		}
	}
}

int unit::max_hot_turns() const
{
	int max = max_tactic_point();
	if (!max) {
		return 0;
	}
	int turns = 100 * max * game_config::tactic_degree_per_point / tactic_degree_increment_per_turn();
	turns = turns % 100? (turns / 100 + 1): (turns / 100);
	return turns - 1;
}

void unit::set_hot_turns(int turns)
{
	const int max_unit_hot_turns = 9;

	hot_turns_ = turns;
	if (hot_turns_ < 0) {
		hot_turns_ = 0;
	} else if (hot_turns_ > max_unit_hot_turns) {
		hot_turns_ = max_unit_hot_turns;
	}
}

void unit::set_tactic_hero(hero& h)
{
	VALIDATE(!slot_cache::find(*this), "insert, but find this in slot cache!");
	VALIDATE(h.tactic_ != HEROS_NO_TACTIC, "insert, hero hasn't tactic!");

	slot_cache::insert_active_tactic(*this);
	tactic_hero_ = &h;

	if (resources::screen && resources::teams && resources::controller) {
		if (resources::controller->current_side() == side_) {
			game_display& disp = *resources::screen;
			teams_[side_ - 1].refresh_tactic_slots(disp);
		}
	}
}

void unit::insert_cast_tactic(game_display& disp, hero& h, unit* special)
{
	slot_cache::insert_cast_tactic(*this, h, special);
	teams_[side_ - 1].refresh_tactic_slots(disp);
}

void unit::insert_clear_formationed(game_display& disp)
{
	slot_cache::insert_clear_formationed(*this);
	teams_[side_ - 1].refresh_tactic_slots(disp);
}

void unit::insert_intervene_move(game_display& disp, const map_location& to)
{
	slot_cache::insert_intervene_move(*this, to);
	teams_[side_ - 1].refresh_tactic_slots(disp);
}

void unit::remove_from_slot_cache(int pos)
{
	bool slot_dirty = false;

	std::vector<slot_cache::tslot>::iterator it = slot_cache::cache.begin();
	if (pos >= 0) {
		VALIDATE(pos < (int)slot_cache::cache.size(), "desire pos is larger than cache size!");
		if (pos) {
			std::advance(it, pos);
		}
		slot_cache::cache.erase(it);
		slot_dirty = true;
	} else {
		for (; it != slot_cache::cache.end(); ) {
			const slot_cache::tslot& slot = *it;
			if (slot.u == this || (slot.type == slot_cache::tslot::CAST_TACTIC && slot.tactic.special == this)) {
				it = slot_cache::cache.erase(it);
				slot_dirty = true;
			} else {
				++ it;
			}
		}
		if (slot_cache::selected_u == this && resources::screen && resources::teams && resources::controller) {
			resources::controller->clear_slot_cache_selected();
		}
	}
	tactic_hero_ = NULL;

	if (slot_dirty && resources::screen && resources::teams && resources::controller) {
		if (resources::controller->current_side() == side_) {
			game_display& disp = *resources::screen;
			teams_[side_ - 1].refresh_tactic_slots(disp);
		}
	}
}

void unit::set_task(int t)
{
	if (t == task_) {
		return;
	}

	if (task_ == TASK_GUARD) {
		remove_from_guard_cache();
	} else {
		task_ = t; 
	}
}

void unit::set_guard(const map_location& loc)
{
	const int guard_radius = 5;

	std::map<unit*, guard_cache::tguard>::iterator find = guard_cache::cache.find(this);
	if (task_ != TASK_GUARD) {
		task_ = TASK_GUARD;
		VALIDATE(find == guard_cache::cache.end(), "want to guard, but find this in guard cache!");
		guard_cache::cache.insert(std::make_pair(this, guard_cache::tguard(loc, guard_radius)));

	} else {
		VALIDATE(find != guard_cache::cache.end(), "want to modify parameter, but cannot find this in guard cache!");
		find->second.loc = loc;
	}
}

void unit::set_guard2(const map_location& loc)
{
	task_ = TASK_NONE;
	set_guard(loc);
}

void unit::remove_from_guard_cache()
{
	if (task_ == TASK_GUARD) {
		for (std::map<unit*, guard_cache::tguard>::iterator it = guard_cache::cache.begin(); it != guard_cache::cache.end(); ++ it) {
			if (it->first == this) {
				guard_cache::cache.erase(it);
				break;
			}
		}
		task_ = TASK_NONE;
	}
}

void unit::set_human(bool val)
{
	VALIDATE(!(val && commoner_), "unit::set_human, Cannot set commoner to huamn!");
	VALIDATE(!(val && is_soldier()), "unit::set_human, Cannot set soldier to huamn!");

	if (human_ == val) {
		return;
	}
	human_ = val;
	if (human_) {
		rpg::humans.insert(this);
	} else {
		rpg::humans.erase(this);
	} 
}

void unit::do_revival(unit& tactician)
{
	VALIDATE(tactician.side() == side_, "tactician must be same side as revivaled!");

	if (tent::turn_based) {
		end_turn(false);
	}
	new_turn_core();
	set_state(ustate_tag::REVIVALED, true);

	game_display& disp = *resources::screen;
	disp.refresh_access_troops(side_ - 1, game_display::REFRESH_DRAW, this);


	std::stringstream strstr;
	strstr << tintegrate::generate_img("misc/revival.png");
	unit_display::unit_text(*this, false, strstr.str());
}

bool unit::consider_food() const
{
	return !tent::tower_mode();
}

bool unit::alert_food() const
{
	return food_ < max_food() / 3;
}

int unit::gold_income() const 
{
	if (!gold_income_ || get_state(ustate_tag::BUILDING)) {
		return 0;
	} else if (commoner_) {
		return gold_income_;
	} else {
		artifical* city = units_.city_from_cityno(cityno_);
		return city->commercial_exploiture() * gold_income_ / 100;
	}
}

int unit::technology_income() const
{
	if (!technology_income_ || get_state(ustate_tag::BUILDING)) {
		return 0;
	} else if (commoner_) {
		return technology_income_;
	} else {
		artifical* city = units_.city_from_cityno(cityno_);
		return city->technology_exploiture() * technology_income_ / 100;
	}
}

int unit::food_income() const
{
	return food_income_;
}

int unit::miss_income() const
{
	return miss_income_;
}

std::string unit::absolute_image() const 
{ 
	return unit_type_->image(); 
}

std::string unit::image_halo() const 
{ 
	if (packed()) {
		return packee_unit_type_->halo();
	} else {
		return unit_type_->halo();
	}
}

bool unit::packed() const
{
	return unit_type_->packer();
}

const unit_type* unit::type() const
{
	return unit_type_;
}

const unit_type* unit::packee_type() const
{
	if (unit_type_->packer()) {
		return packee_unit_type_;
	} else {
		return unit_type_;
	}
}

std::string unit::profile() const
{
	return absolute_image();
}

t_string unit::unit_description() const 
{
	return unit_type_->unit_description();
}

bool unit::can_formation_master() const
{
	if (master_->noble_ != HEROS_NO_NOBLE) {
		const tnoble& noble = unit_types.noble(master_->noble_);
		if (noble.formation()) {
			return true;
		}
	}
	if (second_->valid() && second_->noble_ != HEROS_NO_NOBLE) {
		const tnoble& noble = unit_types.noble(second_->noble_);
		if (noble.formation()) {
			return true;
		}
	}
	if (third_->valid() && third_->noble_ != HEROS_NO_NOBLE) {
		const tnoble& noble = unit_types.noble(third_->noble_);
		if (noble.formation()) {
			return true;
		}
	}
	return packee_type()->require() == unit_type::REQUIRE_LEADER;
}

bool unit::can_formation_attack() const
{
	return !artifical_ && !commoner_;
}

bool unit::formation_attack_enable() const
{
	return (!tent::turn_based || attacks_left()) && !get_state(ustate_tag::FORMATION_ATTACKED);
}

bool unit::human_team_can_ai() const
{
	return !human_ || tent::tower_mode() || provoked_turns_ || task_ == unit::TASK_GUARD;
}

bool unit::can_increase_experience() const
{
	return master_->number_ >= hero::number_city_min;
}

void unit::get_experience(const increase_xp::ublock& ub, int xp, bool master, bool second, bool third) 
{ 
	game_display &disp = *resources::screen;
	bool has_carry = false;
	bool advance_packs = false;
	bool has_rpg = false;
	team& this_team = teams_[side_ - 1];

	if (!hdata_variable()) {
		experience_ += xp;
		if (xp > 0 && unit_feature_val(hero_feature_xp)) {
			// when advance, xp is less than 0. 
			experience_ += xp;
		}
		return;
	}

	// When executing to advance, xp < 0.
	if (xp > 0) {
		uint16_t inc_ability_xp = xp * increase_xp::ability_per_xp;
		if (ub.abilityx2) {
			inc_ability_xp = inc_ability_xp * 2;
		}
		uint16_t inc_arms_xp = xp * increase_xp::arms_per_xp;
		if (ub.armsx2) {
			inc_arms_xp = inc_arms_xp * 2;
		}
		uint16_t inc_skill_xp = xp * increase_xp::skill_per_xp;
		if (ub.skillx2) {
			inc_skill_xp = inc_skill_xp * 2;
		}
		uint16_t inc_meritorious_xp = xp * increase_xp::meritorious_per_xp;
		uint16_t inc_navigation_xp = xp * increase_xp::navigation_per_xp;

		hblock& hb = increase_xp::generic_hblock();

		if (ub.leadership) {
			hb.leadership = inc_ability_xp * (this_team.leadership_speed_ + ub.leadership_speed) / 100;
		}
		if (ub.force) {
			hb.force = inc_ability_xp * (this_team.force_speed_ + ub.force_speed) / 100;
		}
		if (ub.intellect) {
			hb.intellect = inc_ability_xp * (this_team.intellect_speed_ + ub.intellect_speed) / 100;
		}
		if (ub.spirit) {
			hb.spirit = inc_ability_xp * (this_team.spirit_speed_ + ub.spirit_speed) / 100;
		}
		if (ub.charm) {
			hb.charm = inc_ability_xp * (this_team.charm_speed_ + ub.charm_speed) / 100;
		}
		if (ub.arms) {
			hb.arms[arms_] = inc_arms_xp * (this_team.arms_speed_[arms_] + ub.arms_speed) / 100;
			if (packee_unit_type_) {
				hb.arms[packee_unit_type_->arms()] = inc_arms_xp * (this_team.arms_speed_[packee_unit_type_->arms()] + ub.arms_speed) / 100;
			}
		}
		for (int i = 0; i < HEROS_MAX_SKILL; i ++) {
			if (ub.skill[i]) {
				hb.skill[i] = inc_skill_xp * (100 + ub.skill_speed[i]) / 100;
			}
		}
		if (ub.meritorious) {
			hb.meritorious = inc_meritorious_xp;
		}

		if (master) {
			if (master_->get_xp(hb)) {
				has_carry = true;
			}
			if (master_ == rpg::h) {
				has_rpg = true;
			}
		}
		if (second && second_->valid()) {
			if (second_->get_xp(hb)) {
				has_carry = true;
			}
			if (second_ == rpg::h) {
				has_rpg = true;
			}
		}
		if (third && third_->valid()) {
			if (third_->get_xp(hb)) {
				has_carry = true;
			}
			if (third_ == rpg::h) {
				has_rpg = true;
			}
		}
		if (has_carry) {
			calculate_5fields();
		}
		if (arms_ == 4) {
			int prev_navigation = this_team.navigation();
			this_team.add_navigation(inc_navigation_xp * this_team.navigation_civi_increase_ / 100);
			if (unit_types.navigation_can_advance(prev_navigation, this_team.navigation())) {
				advance_packs = true;
			}
		}
		// when xp>0, ub.xp controls whehter xp increase or not.
		if (ub.xp) {
			experience_ += xp;
			if (unit_feature_val(hero_feature_xp)) {
				// when advance, xp is less than 0.
				experience_ += xp;
			}
		}
	} else {
		experience_ += xp;
	}

	if (advance_packs) {
		// field troops
		const std::pair<unit**, size_t> p = this_team.field_troop();
		unit** troops = p.first;
		size_t troops_vsize = p.second;
		const unit_type* packer = unit_types.find(unit_types.id_from_navigation(this_team.navigation()));
		for (size_t i = 0; i < troops_vsize; i ++) {
			if (troops[i]->packed()) {
				// troops[i]->pack_to(NULL);
				troops[i]->pack_to(packer);
				// since packer is changed, appearance of troops need be updated.
				troops[i]->delete_anim();
			}
		}
	} else if (has_carry) {
		const unit_type* current_type = type();
		if (packed()) {
			pack_to(current_type);
		} else {
			max_hit_points_ = current_type->hitpoints();
			max_experience_ = current_type->experience_needed(false);
			max_movement_ = current_type->movement();
			attacks_ = current_type->attacks();

			modify_according_to_hero(false, false);
		}
	}

	if (has_rpg && xp > 0 && rpg::stratum != hero_stratum_leader) {
		resources::controller->rpg_update();
	}
	return;
}

SDL_Color unit::hp_color() const
{
	double unit_energy = 0.0;
	SDL_Color energy_color = {0,0,0,0};

	if(max_hitpoints() > 0) {
		unit_energy = double(hitpoints())/double(max_hitpoints());
	}

	if(1.0 == unit_energy){
		energy_color.r = 33;
		energy_color.g = 225;
		energy_color.b = 0;
	} else if(unit_energy > 1.0) {
		energy_color.r = 100;
		energy_color.g = 255;
		energy_color.b = 100;
	} else if(unit_energy >= 0.75) {
		energy_color.r = 170;
		energy_color.g = 255;
		energy_color.b = 0;
	} else if(unit_energy >= 0.5) {
		energy_color.r = 255;
		energy_color.g = 175;
		energy_color.b = 0;
	} else if(unit_energy >= 0.25) {
		energy_color.r = 255;
		energy_color.g = 155;
		energy_color.b = 0;
	} else {
		energy_color.r = 255;
		energy_color.g = 0;
		energy_color.b = 0;
	}
	return energy_color;
}

SDL_Color unit::xp_color() const
{
	const SDL_Color near_advance_color = {255,255,255,0};
	const SDL_Color mid_advance_color  = {150,255,255,0};
	const SDL_Color far_advance_color  = {0,205,205,0};
	const SDL_Color normal_color 	  = {0,160,225,0};
	const SDL_Color near_amla_color	  = {225,0,255,0};
	const SDL_Color mid_amla_color	  = {169,30,255,0};
	const SDL_Color far_amla_color	  = {139,0,237,0};
	const SDL_Color amla_color		  = {170,0,255,0};
	const bool near_advance = max_experience() - experience() <= game_config::kill_experience;
	const bool mid_advance  = max_experience() - experience() <= game_config::kill_experience*2;
	const bool far_advance  = max_experience() - experience() <= game_config::kill_experience*3;

	SDL_Color color=normal_color;
	if(advances_to().size()){
		if(near_advance){
			color=near_advance_color;
		} else if(mid_advance){
			color=mid_advance_color;
		} else if(far_advance){
			color=far_advance_color;
		}
	} else if (get_modification_advances().size()){
		if(near_advance){
			color=near_amla_color;
		} else if(mid_advance){
			color=mid_amla_color;
		} else if(far_advance){
			color=far_amla_color;
		} else {
			color=amla_color;
		}
	}
	return(color);
}

bool unit::uncleared() const
{
	return has_state_flag(ustate_tag::SLOWED) || has_state_flag(ustate_tag::BROKEN) || has_state_flag(ustate_tag::POISONED) || provoked_turns_;
}

bool unit::healable() const
{
	return max_hit_points_ / 2 >= hit_points_;
}

void unit::set_movement(int moves)
{
	movement_ = std::max<int>(0, moves);
}

bool unit::can_move() const
{
	const team& current_team = teams_[side_ - 1];
	const gamemap& map = *resources::game_map;

	if (artifical_) {
		return true;
	}

	if (!attacks_left() && movement_left() == 0) {
		return false;
	}

	// Units with goto commands that have already done their gotos this turn
	// (i.e. don't have full movement left) should have red globes.
	if (has_moved() && has_goto()) {
		return false;
	}

	for (size_t n = 0; n != adjacent_size_; ++ n) {
		const map_location& loc = adjacent_[n];
		const unit* that = units_.find_unit(loc);
		if (that) {
			if (!that->incapacitated() && current_team.is_enemy(that->side_)) {
				return true;
			}
		}

		if (movement_cost(map[loc]) <= movement_left()) {
			return true;
		}
	}

	return false;
}

bool unit::consider_ticks() const
{	
	if (artifical_ && !can_reside_) {
		return false;
	}
	return !commoner_;
}

int unit::increase_ticks(int inc, int min, bool refresh_gui)
{
	if (!consider_ticks()) {
		return ticks_;
	}
	if (ticks_ < min) {
		return ticks_;
	}

	int orignal = ticks_;
	ticks_ += inc;
	if (ticks_ > game_config::ticks_per_turn) {
		ticks_ = game_config::ticks_per_turn;

	} else if (ticks_ < min) {
		ticks_ = min;

	}

	if (refresh_gui && orignal != ticks_) {
		game_display& disp = *resources::screen;
		disp.resort_access_troops(*this);
	}

	return ticks_;
}

int unit::percent_ticks() const
{
	return 100 * (game_config::ticks_per_turn - ticks_) / game_config::ticks_per_turn;
}

bool unit::is_resided() const
{
	return ticks_ == RESIDE_TICKS;
}

const int min_decreased_ticks = 50;	// normal is 100, indicate not adjust.

int unit::forward_ticks(int val) const
{
	int adjust = val;
	if (!is_artifical()) {
		int bonus = teams_[side_ - 1].ticks_increase + ticks_increase_;
		if (unit_feature_val(hero_feature_spirit)) {
			bonus += game_config::feature_increase_spirit;
		}
		if (bonus < min_decreased_ticks) {
			// reference to http://www.freeors.com/bbs/forum.php?mod=viewthread&tid=22045&extra=page%3D1
			bonus = min_decreased_ticks;
		}
		adjust = 1.0 * val * bonus / 100;
	}

	return adjust;
}

int unit::backward_ticks(int val) const
{
	int adjust = val;
	if (!is_artifical()) {
		int bonus = teams_[side_ - 1].ticks_increase + ticks_increase_;
		if (unit_feature_val(hero_feature_spirit)) {
			bonus += game_config::feature_increase_spirit;
		}
		if (bonus < min_decreased_ticks) {
			// reference to http://www.freeors.com/bbs/forum.php?mod=viewthread&tid=22045&extra=page%3D1
			bonus = min_decreased_ticks;
		}
		// make sure this value >= forward_ticks calcualted.
		adjust = std::ceil(1.0 * val * 100 / bonus);
	}

	return adjust;
}

void unit::set_attacks(int left) 
{ 
	attacks_left_ = std::max<int>(0,std::min<int>(left,max_attacks_)); 
}

void unit::new_turn_tactics()
{
	bool tactics_changed = false;
	for (std::vector<tunit_tactic>::iterator it = tactics_.begin(); it != tactics_.end(); ) {
		if (-- it->turn_) {
			++ it;
		} else {
			it = tactics_.erase(it);
			tactics_changed = true;
		}
	}
	if (tactics_changed) {
		adjust();
	}
}

void unit::terminate_noble(bool show_message) const
{
	team& t = teams_[side_ - 1];
	if (master_->has_nomal_noble()) {
		t.appoint_noble(*master_, HEROS_NO_NOBLE, show_message);
	}
	if (second_->valid() && second_->has_nomal_noble()) {
		t.appoint_noble(*second_, HEROS_NO_NOBLE, show_message);
	}
	if (third_->valid() && third_->has_nomal_noble()) {
		t.appoint_noble(*third_, HEROS_NO_NOBLE, show_message);
	}
}

bool unit::tactic_existed() const
{
	if (master_->tactic_ != HEROS_NO_TACTIC) {
		return true;
	}
	if (second_->valid() && second_->tactic_ != HEROS_NO_TACTIC) {
		return true;
	}
	if (third_->valid() && third_->tactic_ != HEROS_NO_TACTIC) {
		return true;
	}
	return false;
}

bool unit::can_cast_tactic() const
{
	int point = tactic_degree_ / game_config::tactic_degree_per_point;
	if (!point) {
		return false;
	}
	if (master_->tactic_ != HEROS_NO_TACTIC) {
		if (unit_types.tactic(master_->tactic_).point() <= point) {
			return true;
		}
	}
	if (second_->valid() && second_->tactic_ != HEROS_NO_TACTIC) {
		if (unit_types.tactic(second_->tactic_).point() <= point) {
			return true;
		}
	}
	if (third_->valid() && third_->tactic_ != HEROS_NO_TACTIC) {
		if (unit_types.tactic(third_->tactic_).point() <= point) {
			return true;
		}
	}
	return false;
}

int unit::max_tactic_point() const
{
	int point, max_point = 0;
	if (master_->tactic_ != HEROS_NO_TACTIC) {
		max_point = unit_types.tactic(master_->tactic_).point();
	}
	if (second_->valid() && second_->tactic_ != HEROS_NO_TACTIC) {
		point = unit_types.tactic(second_->tactic_).point();
		if (point > max_point) {
			max_point = point;
		}
	}
	if (third_->valid() && third_->tactic_ != HEROS_NO_TACTIC) {
		point = unit_types.tactic(third_->tactic_).point();
		if (point > max_point) {
			max_point = point;
		}
	}
	return max_point;
}

hero* unit::max_tactic_hero() const
{
	int point, max_point = 0;
	hero* max_hero = NULL;
	if (master_->tactic_ != HEROS_NO_TACTIC) {
		max_point = unit_types.tactic(master_->tactic_).point();
		max_hero = master_;
	}
	if (second_->valid() && second_->tactic_ != HEROS_NO_TACTIC) {
		point = unit_types.tactic(second_->tactic_).point();
		if (point > max_point) {
			max_point = point;
			max_hero = second_;
		}
	}
	if (third_->valid() && third_->tactic_ != HEROS_NO_TACTIC) {
		point = unit_types.tactic(third_->tactic_).point();
		if (point > max_point) {
			max_point = point;
			max_hero = third_;
		}
	}
	return max_hero;
}

int unit::percent_tactic_degree() const
{
	int max_point = max_tactic_point();

	if (max_point > 0) {
		if (tactic_degree_ >= max_point * game_config::tactic_degree_per_point) {
			return 100;
		} 
		return 100 * tactic_degree_/ (max_point * game_config::tactic_degree_per_point);
	}
	return max_point;
}

int unit::increase_tactic_degree(int inc)
{
	int max = max_tactic_point() * game_config::tactic_degree_per_point;
	if (max <= 0) {
		return tactic_degree_;
	}

	tactic_degree_ += inc;
	if (tactic_degree_ < 0) {
		tactic_degree_ = 0;

	} else if (tactic_degree_ > max) {
		tactic_degree_ = max;
	}

	return tactic_degree_;
}

std::pair<hero*, int> unit::calculate_tactic_score() const
{
	size_t friends = 0;
	size_t unhides = 0;
	size_t unnormals = 0;
	size_t healables = 0;
	size_t unprovoks = 0;
	size_t our_side_selectable = 0;
	size_t enemies = 0;
	std::set<const unit*> calculated;
	const team& current_team = teams_[side_ - 1];

	const map_location* tiles = adjacent_;
	size_t adjance_size = adjacent_size_;
	for (int step = 0; step < 2; step ++) {
		if (step == 1) {
			tiles = adjacent_2_;
			adjance_size = adjacent_size_2_;
		}

		for (size_t adj = 0; adj < adjance_size; adj ++) {
			const map_location& loc = tiles[adj];
			// overlay
			for (int layer = 0; layer < 2; layer ++) {
				const unit* u = NULL;
				if (layer == 0) {
					u = units_.find_unit(loc, true);
				} else {
					u = units_.find_unit(loc, false);
				}

				if (u) {
					if (calculated.find(u) != calculated.end()) {
						continue;
					}

					if (u->side() != side_) {
						if (current_team.is_enemy(u->side())) {
							if (!u->is_artifical() && !u->is_commoner() && !u->invisible(u->get_location())) {
								enemies ++;
								if (!u->provoked_turns()) {
									unprovoks ++;
								}
							}
						} else {
							if (!u->is_artifical() && !u->is_commoner()) {
								friends ++;
								if (!u->hide_turns()) {
									unhides ++;
								}
								if (u->uncleared()) {
									unnormals ++;
								}
								if (u->healable()) {
									healables ++;
								}
							}
						}
					} else if (!u->is_artifical() && !u->is_commoner()) {
						friends ++;
						if (!tent::turn_based || !u->attacks_left()) {
							our_side_selectable ++;
						}
						if (!u->hide_turns()) {
							unhides ++;
						}
						if (u->uncleared()) {
							unnormals ++;
						}
						if (u->healable()) {
							healables ++;
						}
					}
					calculated.insert(u);
				} 
			}
		}
	}

	int best_score = -1;
	hero* best_hero = NULL;

	for (int step = 0; step < 3; step ++) {
		hero* h;
		if (step == 0) {
			h = master_;
		} else if (step == 1) {
			if (!second_->valid()) {
				continue;
			}
			h = second_;
		} else if (step == 2) {
			if (!third_->valid()) {
				continue;
			}
			h = third_;
		}
		if (h->tactic_ == HEROS_NO_TACTIC) {
			continue;
		}

		const ttactic& t = unit_types.tactic(h->tactic_);

		int tag = t.select_one_internal();
		if (tag == apply_to_tag::PROVOKE) {
			if (!unprovoks) {
				continue;
			}
		} else if (tag == apply_to_tag::REVIVAL) {
			if (!our_side_selectable) {
				continue;
			}
		}

		// self. if no enemies, this profit divided by 2. 
		int score = t.self_profit();
		if (!hide_turns_) {
			score += t.self_hide_profit();
		}
		if (!alert_turns_) {
			score += t.self_alert_profit();
		}
		if (uncleared()) {
			score += t.self_clear_profit();
		}
		if (healable()) {
			score += t.self_heal_profit();
		}

		// friend. if no enemies, this profit divided by 2.
		score += t.friend_profit() * friends;
		score += t.friend_hide_profit() * unhides;
		score += t.friend_clear_profit() * unnormals;
		score += t.friend_heal_profit() * healables;
		score += our_side_selectable? t.friend_select_one_profit(): 0;
		if (!enemies) {
			score /= 2;
		}

		// enemy
		score += t.enemy_profit() * enemies;
		if (unprovoks) {
			score += t.enemy_provoke_profit();
		}
		
		// don't conside tactic's point

		if (score > best_score) {
			best_hero = h;
			best_score = score;
		}
	}

	return std::make_pair(best_hero, best_score);
}

bool unit::judge_and_cast_tactic(int threshold)
{
	if (is_artifical() || is_commoner() || percent_tactic_degree() < 100) {
		return false;
	}

	std::pair<hero*, int> score = calculate_tactic_score();
	if (!score.first || score.second < threshold) {
		return false;
	}
	cast_tactic(teams_, units_, *this, *(score.first), NULL);

	return true;
}

int unit::hot_level() const
{
	int level = max_tactic_point() * game_config::hot_level_per_point;
	if (level < 10) {
		return 10;
	} else if (level > 50) {
		return 50;
	}
	return level;
}

int unit::tactic_degree_increment_per_turn() const
{
	int incrememt = game_config::tactic_degree_per_turn;
	if (unit_feature_val(hero_feature_tactic)) {
		incrememt += game_config::tactic_degree_per_turn / 2;
	}
	const team& t = teams_[side_ - 1];
	incrememt += t.tactic_degree_increase - 100;

	return incrememt;
}

bool captain_can_form(std::vector<hero*>& captain, const unit_type* ut)
{
	if (captain.empty()) {
		return false;
	}
	std::sort(captain.begin(), captain.end(), sort_recruit(ut));
	if (ut->require() == unit_type::REQUIRE_FEMALE && captain[0]->gender_ != hero_gender_female) {
		return false;
	}
	return true;
}

void unit::new_turn_core()
{
	team& t = teams_[side_ - 1];
	if (!t.restrict_movement && task_ != unit::TASK_STATION) {
		movement_ = total_movement();
	}
	// attacks_left_ = max_attacks_;
	set_state(ustate_tag::UNCOVERED, false);
	set_state(ustate_tag::FORMATION_ATTACKED, false);

	// below changed movement_, require draw.
	if (drawn_ticks_ != HIDDEN_TICKS) {
		drawn_ticks_ = NONE_TICKS;
	}
}

bool unit::new_turn(play_controller& controller, int)
{
	bool erase = false;
	team& this_team = teams_[side_ - 1];

	new_turn_core();

	// when in camp system, teams_ is empty. of course artifical_ is false.
	if (artifical_) {
		new_turn_tactics();
		return false;
	}
	VALIDATE(!teams_.empty(), "unit::new_turn, teams must not empty!");

	if (consider_loyalty() && controller.turn() > 1) {
		hero& leader = *(this_team.leader());
		// wander
		std::vector<hero*> captains, wanders;
		if (master_ != rpg::h && master_->loyalty(leader) < game_config::wander_loyalty_threshold) {
			wanders.push_back(master_);
		} else {
			captains.push_back(master_);
		}
		if (second_->valid()) {
			if (second_ != rpg::h && second_->loyalty(leader) < game_config::wander_loyalty_threshold) {
				wanders.push_back(second_);
			} else {
				captains.push_back(second_);
			}
		}
		if (third_->valid()) {
			if (third_ != rpg::h && third_->loyalty(leader) < game_config::wander_loyalty_threshold) {
				wanders.push_back(third_);
			} else {
				captains.push_back(third_);
			}
		}
		if (!wanders.empty()) {
			artifical* to_city = units_.city_from_seed(max_hit_points_);
			for (std::vector<hero*>::iterator itor = wanders.begin(); itor != wanders.end(); ++ itor) {
				game_events::show_hero_message(*itor, units_.city_from_cityno((*itor)->city_), 
					_("In misfortune time, it is necessary to search chance that can carry out value of life."),
					game_events::INCIDENT_WANDER);
				to_city->wander_into(**itor, this_team.is_human()? false: true);
			}
			if (!captain_can_form(captains, packee_type())) {
				erase = true;
				// this troops will be erase.
				to_city = units_.city_from_cityno(cityno_);
				for (std::vector<hero*>::iterator it = captains.begin(); it != captains.end(); ++ it) {
					if (to_city) {
						to_city->finish_into(**it, hero_status_backing);
					}
				}
			} else {
				replace_captains(captains);
			}
		}
	}

	if (!dont_wander) {
		field_turns_ ++;

		if (hide_turns_) {
			hide_turns_ --;
		}
		if (alert_turns_) {
			set_alert_turns(alert_turns_ - 1);
		}
		if (provoked_turns_) {
			set_provoked_turns_next(*this, provoked_turns_ - 1);
		}

		if (!commoner_) {
			// field troop, increase activity
			increase_activity(15);
			
			if (!artifical_) {
				get_experience(increase_xp::turn_ublock(*this), 3);
			}
		}
		increase_feeling(game_config::increase_feeling);

	} else {
		if (hide_turns_) {
			set_hide_turns(0);
		}
		if (alert_turns_) {
			set_alert_turns(0);
		}
		if (provoked_turns_) {
			set_provoked_turns_next(*this, 0);
		}
	}

	// tactic degree
	if (is_artifical() || is_commoner() || percent_tactic_degree() < 100) {
		if (hot_turns_) {
			hot_turns_ --;
		}
	} else {
		set_hot_turns(max_hot_turns());
		tactic_degree_ = 0;
	}
	if (tactic_existed()) {
		increase_tactic_degree(tactic_degree_increment_per_turn());
	}

	new_turn_tactics();


	return erase;
}


bool unit::no_enter_resting() const
{
	return provoked_turns_;
}

void unit::end_turn(bool normal)
{
	// for formation attack. place set attacks here.
	attacks_left_ = max_attacks_;

	set_state(ustate_tag::SLOWED, false);
	set_state(ustate_tag::BROKEN, false);
	set_state(ustate_tag::REVIVALED, false);
	if (normal) {
		if (no_enter_resting() || movement_ != total_movement()) {
			resting_ = false;
		}
	}
	set_state(ustate_tag::BLOCKED, false);

}

void unit::new_scenario()
{
	// Set the goto-command to be going to no-where
	goto_ = map_location();
	from_ = map_location();

	heal_all();
	set_state(ustate_tag::SLOWED, false);
	set_state(ustate_tag::BROKEN, false);
	set_state(ustate_tag::POISONED, false);
	set_state(ustate_tag::PETRIFIED, false);
}

void unit::heal(int amount)
{
	int max_hp = max_hit_points_;
	if (hit_points_ < max_hp) {
		hit_points_ += amount;
		if (hit_points_ > max_hp) {
			hit_points_ = max_hp;
		}
	}
	if (hit_points_ < 1) {
		hit_points_ = 1;
	}
}

const std::map<std::string,std::string> unit::get_states() const
{
	std::map<std::string, std::string> all_states;
	for (std::map<const std::string, ustate_tag::state_t>::const_iterator it = ustate_tag::tags.begin(); it != ustate_tag::tags.end(); ++ it) {
		if (has_state_flag(it->second)) {
			all_states.insert(make_pair(it->first, "yes"));
		}
	}
	return all_states;
}

void unit::set_state(ustate_tag::state_t state, bool value)
{
	if (value) {
		set_state_flag(state);
	} else {
		clear_state_flag(state);
	}
}

bool unit::get_state(ustate_tag::state_t state) const
{
	return has_state_flag(state);
}

ustate_tag::state_t unit::find_state(const std::string& id)
{
	ustate_tag::state_t s = ustate_tag::find(id);

	if (s == ustate_tag::NONE) {
		std::stringstream err;
		err << "find_state, invalid state id: " << id;
		VALIDATE(false, err.str());
	}

	return s;
}

bool unit::is_robber() const
{
	return master_->get_flag(hero_flag_robber) || get_state(ustate_tag::ROBBER);
}

bool unit::has_ability_by_id(const std::string& ability) const
{
	const std::multimap<const std::string, const config*>* abilities;
	if (packed()) {
		abilities = &packee_unit_type_->abilities_cfg();
	} else {
		abilities = &unit_type_->abilities_cfg();
	}
	for (std::multimap<const std::string, const config*>::const_iterator it = abilities->begin(); it != abilities->end(); ++ it) {
		const config& ab_cfg = *(it->second);
		if (ab_cfg["id"] == ability) {
			return true;
		}
	}
	return false;
}

bool unit::matches_filter(const vconfig& cfg, const map_location& loc, bool use_flat_tod) const
{
	bool matches = true;

	VALIDATE(loc.valid(), "unit::matches_filter, loc is invalid.");

	// I am strange to "this_unit". but system not save unit's fields until access fields. attention in future.
	scoped_xy_unit auto_store("this_unit", loc.x, loc.y, *resources::units);
	matches = internal_matches_filter(cfg, loc, use_flat_tod);

	// Handle [and], [or], and [not] with in-order precedence
	vconfig::all_children_iterator cond = cfg.ordered_begin();
	vconfig::all_children_iterator cond_end = cfg.ordered_end();
	while(cond != cond_end)
	{

		const std::string& cond_name = cond.get_key();
		const vconfig& cond_filter = cond.get_child();

		// Handle [and]
		if(cond_name == "and") {
			matches = matches && matches_filter(cond_filter,loc,use_flat_tod);
		}
		// Handle [or]
		else if(cond_name == "or") {
			matches = matches || matches_filter(cond_filter,loc,use_flat_tod);
		}
		// Handle [not]
		else if(cond_name == "not") {
			matches = matches && !matches_filter(cond_filter,loc,use_flat_tod);
		}

		++cond;
	}
	return matches;
}

bool unit::internal_matches_filter(const vconfig& cfg, const map_location& loc, bool use_flat_tod) const
{
	std::vector<std::string> vstr;

	if (!cfg["hp"].blank()) {
		vstr = utils::split(cfg["hp"].str());
		if (vstr.size() == 1) {
			if (lexical_cast_default<int>(vstr[0], -1) != hit_points_) {
				return false;
			}
		} else if (vstr.size() == 2) {
			int hit_points;
			if (vstr[1][vstr[1].size() - 1] == '%') {
				hit_points = lexical_cast_default<int>(vstr[1]) * max_hit_points_ / 100;
			} else {
				hit_points = lexical_cast_default<int>(vstr[1]);
			}

			if (vstr[0] == "<=") {
				if (hit_points_ > hit_points) {
					return false;
				}
			} else if (vstr[0] == ">=") {
				if (hit_points_ < hit_points) {
					return false;
				}
			} else if (vstr[0] == "<") {
				if (hit_points_ >= hit_points) {
					return false;
				}
			} else if (vstr[0] == ">") {
				if (hit_points_ <= hit_points) {
					return false;
				}
			}
		} else {
			return false;
		}
	}

	if (!cfg["must_heros"].blank()) {
		const std::vector<std::string> must_heros = utils::split(cfg["must_heros"]);
		for (std::vector<std::string>::const_iterator tmp = must_heros.begin(); tmp != must_heros.end(); ++ tmp) {
			int number = lexical_cast_default<int>(*tmp);
			if (master_->number_ == number) {
				continue;
			}
			if (second_->valid() && second_->number_ == number) {
				continue;
			}
			if (third_->valid() && third_->number_ == number) {
				continue;
			}
			return false;
		}
	}

	if (!cfg["cityno"].blank()) {
		if (cfg["cityno"].to_int(0) != cityno_) {
			return false;
		}
	}

	// 1. this must is city
	// 2. yes: this is noly one city
	// 3. no: this is at least tow city
	if (!cfg["last_city"].blank()) {
		if (!unit_is_city(this)) {
			return false;
		}
		std::vector<artifical*>& citys = teams_[side_ - 1].holden_cities();
		if (citys.size() > 1) {
			if (cfg["last_city"].to_bool()) {
				return false;
			}
		} else if (!cfg["last_city"].to_bool()) {
			return false;
		}
	}

	if (!cfg["exist_reside_troop"].blank()) {
		if (!unit_is_city(this)) {
			return false;
		}
		int hit_hero = cfg["exist_reside_troop"].to_int();
		const artifical* city = const_unit_2_artifical(this);
		std::vector<unit*> reside_troops = city->reside_troops();
		std::vector<unit*>::const_iterator itor;
		for (itor = reside_troops.begin(); itor != reside_troops.end(); ++ itor) {
			unit& u = **itor;
			if (u.master().number_ == hit_hero) {
				break;
			}
			if (u.second().valid() && u.second().number_ == hit_hero) {
				break;
			}
			if (u.third().valid() && u.third().number_ == hit_hero) {
				break;
			}
		}
		if (itor == reside_troops.end()) {
			return false;
		}
	}

	if (cfg.has_child("filter_location")) {
		VALIDATE(resources::game_map != NULL, "unit::internal_matches_filter, game_map == NULL!");
		VALIDATE(resources::teams != NULL, "unit::internal_matches_filter, teams == NULL!");
		VALIDATE(resources::tod_manager != NULL, "unit::internal_matches_filter, tod_manager == NULL!");
		VALIDATE(resources::units != NULL, "unit::internal_matches_filter, units == NULL!");
		const vconfig& t_cfg = cfg.child("filter_location");
		terrain_filter t_filter(t_cfg, units_, use_flat_tod);
		if(!t_filter.match(loc)) {
			return false;
		}
	}
	// Also allow filtering on location ranges outside of the location filter
	config::attribute_value cfg_x = cfg["x"];
	config::attribute_value cfg_y = cfg["y"];
	if (!cfg_x.blank() || !cfg_y.blank()){
		if(cfg_x.empty() && cfg_y.empty()) {
			return false;
		} else if(!loc.matches_range(cfg_x, cfg_y)) {
			return false;
		}
	}

	// The type could be a comma separated list of types
	config::attribute_value cfg_type = cfg["type"];
	if (!cfg_type.blank()) {
		std::string type = cfg_type;
		const std::string& this_type = type_id();

		// We only do the full CSV search if we find a comma in there,
		// and if the subsequence is found within the main sequence.
		// This is because doing the full CSV split is expensive.
		if (type == this_type) {
			// pass
		} else if (std::find(type.begin(),type.end(), ',') != type.end() &&
			std::search(type.begin(), type.end(), this_type.begin(), this_type.end()) != type.end()) {

			const std::vector<std::string>& vals = utils::split(type);

			if (std::find(vals.begin(),vals.end(),this_type) == vals.end()) {
				return false;
			}
		} else {
			return false;
		}
	}

	if (!cfg["family"].blank()) {
		if (!family_tag::filter(*this, cfg["family"].str())) {
			return false;
		}
	}

	config::attribute_value cfg_ability = cfg["ability"];
	if (!cfg_ability.blank())
	{
		std::string ability = cfg_ability;
		if(has_ability_by_id(ability)) {
			// pass
		} else if(std::find(ability.begin(),ability.end(),',') != ability.end()) {
			const std::vector<std::string>& vals = utils::split(ability);
			bool has_ability = false;
			for(std::vector<std::string>::const_iterator this_ability = vals.begin(); this_ability != vals.end(); ++this_ability) {
				if(has_ability_by_id(*this_ability)) {
					has_ability = true;
					break;
				}
			}
			if(!has_ability) {
				return false;
			}
		} else {
			return false;
		}
	}

	config::attribute_value cfg_race = cfg["race"];
	if (!cfg_race.blank() && cfg_race.str() != race_->id()) {
		return false;
	}

	config::attribute_value cfg_gender = cfg["gender"];
	if (!cfg_gender.blank() && string_gender(cfg_gender) != gender()) {
		return false;
	}

	config::attribute_value cfg_side = cfg["side"];
	if (!cfg_side.blank() && cfg_side.to_int() != side()) {
		std::string side = cfg_side;
		if (std::find(side.begin(), side.end(), ',') == side.end()) {
			return false;
		}
		std::vector<std::string> vals = utils::split(side);
		if (std::find(vals.begin(), vals.end(), str_cast(side_)) == vals.end()) {
			return false;
		}
	}

	if (!cfg["controller"].blank()) {
		if (!controller_tag::filter(teams_[side_ - 1].controller(), cfg["controller"].str())) {
			return false;
		}
	}

	config::attribute_value cfg_has_weapon = cfg["has_weapon"];
	if (!cfg_has_weapon.blank()) {
		std::string weapon = cfg_has_weapon;
		bool has_weapon = false;
		const std::vector<attack_type>& attacks = this->attacks();
		for(std::vector<attack_type>::const_iterator i = attacks.begin();
			i != attacks.end(); ++i) {
			if(i->id() == weapon) {
				has_weapon = true;
				break;
			}
		}
		if(!has_weapon) {
			return false;
		}
	}

	config::attribute_value cfg_level = cfg["level"];
	if (!cfg_level.blank()) {
		vstr = utils::split(cfg_level.str());
		if (vstr.size() == 1) {
			if (lexical_cast_default<int>(vstr[0], -1) != level_) {
				return false;
			}
		} else if (vstr.size() == 2) {
			int level = lexical_cast_default<int>(vstr[1], -1);
			if (vstr[0] == "<=") {
				if (level_ > level) {
					return false;
				}
			} else if (vstr[0] == ">=") {
				if (level_ < level) {
					return false;
				}
			} else if (vstr[0] == "<") {
				if (level_ >= level) {
					return false;
				}
			} else if (vstr[0] == ">") {
				if (level_ <= level) {
					return false;
				}
			}
		} else {
			return false;
		}
	}

	config::attribute_value cfg_defense = cfg["defense"];
	if (!cfg_defense.blank() && cfg_defense.to_int(-1) != defense_modifier(resources::game_map->get_terrain(loc))) {
		return false;
	}

	config::attribute_value cfg_movement = cfg["movement_cost"];
	if (!cfg_movement.blank() && cfg_movement.to_int(-1) != movement_cost(resources::game_map->get_terrain(loc))) {
		return false;
	}

	// Now start with the new WML based comparison.
	// If a key is in the unit and in the filter, they should match
	// filter only => not for us
	// unit only => not filtered
	const vconfig::child_list& wmlcfgs = cfg.get_children("filter_wml");
	if (!wmlcfgs.empty()) {
		config unit_cfg;
		for (unsigned i = 0; i < wmlcfgs.size(); ++i)
		{
			config fwml = wmlcfgs[i].get_parsed_config();
			/* Check if the filter only cares about variables.
			   If so, no need to serialize the whole unit. */
			config::const_attr_itors ai = fwml.attribute_range();
			config::all_children_itors ci = fwml.all_children_range();
			if (std::distance(ai.first, ai.second) == 0 &&
			    std::distance(ci.first, ci.second) == 1 &&
			    ci.first->key == "variables") {
				
			} else {
				if (unit_cfg.empty())
					write(unit_cfg);
				if (!unit_cfg.matches(fwml))
					return false;
			}
		}
	}

	if (cfg.has_child("filter_vision")) {
		const vconfig::child_list& vis_filt = cfg.get_children("filter_vision");
		vconfig::child_list::const_iterator i, i_end = vis_filt.end();
		for (i = vis_filt.begin(); i != i_end; ++i) {
			bool visible = (*i)["visible"].to_bool(true);
			std::set<int> viewers;
			if (i->has_attribute("viewing_side")) {
				std::vector<std::pair<int,int> > ranges = utils::parse_ranges((*i)["viewing_side"]);
				std::vector<std::pair<int,int> >::const_iterator range, range_end = ranges.end();
				for (range = ranges.begin(); range != range_end; ++range) {
					for (int i=range->first; i<=range->second; ++i) {
						if (i > 0 && static_cast<size_t>(i) <= teams_manager::get_teams().size()) {
							viewers.insert(i);
						}
					}
				}
			} else {
				//if viewing_side is not defined, default to all enemies
				const team& my_team = teams_manager::get_teams()[this->side()-1];
				for (size_t i = 1; i <= teams_manager::get_teams().size(); ++i) {
					if (my_team.is_enemy(i)) {
						viewers.insert(i);
					}
				}
			}
			if (viewers.empty()) {
				return false;
			}
			std::set<int>::const_iterator viewer, viewer_end = viewers.end();
			for (viewer = viewers.begin(); viewer != viewer_end; ++viewer) {
				bool not_fogged = !teams_manager::get_teams()[*viewer - 1].fogged(loc);
				bool not_hiding = !this->invisible(loc/*, false(?) */);
				if (visible != not_fogged && not_hiding) {
					return false;
				}
			}
		}
	}

	if (cfg.has_child("filter_adjacent")) {
		assert(resources::units && resources::game_map);
		const unit_map& units = *resources::units;
		map_location adjacent[6];
		get_adjacent_tiles(loc, adjacent);
		vconfig::child_list::const_iterator i, i_end;
		const vconfig::child_list& adj_filt = cfg.get_children("filter_adjacent");
		for (i = adj_filt.begin(), i_end = adj_filt.end(); i != i_end; ++i) {
			int match_count=0;
			static std::vector<map_location::DIRECTION> default_dirs
				= map_location::parse_directions("n,ne,se,s,sw,nw");
			config::attribute_value i_adjacent = (*i)["adjacent"];
			std::vector<map_location::DIRECTION> dirs = !i_adjacent.blank() ?
				map_location::parse_directions(i_adjacent) : default_dirs;
			std::vector<map_location::DIRECTION>::const_iterator j, j_end = dirs.end();
			for (j = dirs.begin(); j != j_end; ++j) {
				unit_map::const_iterator unit_itor = units.find(adjacent[*j]);
				if (unit_itor == units.end()) {
					continue;
				}
				const unit* u = dynamic_cast<const unit*>(&*unit_itor);
				if (!u->matches_filter(*i, u->get_location(), use_flat_tod)) {
					continue;
				}
				config::attribute_value i_is_enemy = (*i)["is_enemy"];
				if (i_is_enemy.blank() || i_is_enemy.to_bool() ==
				    teams_manager::get_teams()[this->side() - 1].is_enemy(u->side())) {
					++match_count;
				}
			}
			static std::vector<std::pair<int,int> > default_counts = utils::parse_ranges("1-6");
			config::attribute_value i_count = (*i)["count"];
			std::vector<std::pair<int,int> > counts = !i_count.blank()
				? utils::parse_ranges(i_count) : default_counts;
			if(!in_ranges(match_count, counts)) {
				return false;
			}
		}
	}

	config::attribute_value cfg_find_in = cfg["find_in"];
	if (!cfg_find_in.blank()) {
		// Allow filtering by searching a stored variable of units
		variable_info vi(cfg_find_in, false, variable_info::TYPE_CONTAINER);
		if(!vi.is_valid) return false;
		if(vi.explicit_index) {
			config::const_child_iterator i = vi.vars->child_range(vi.key).first;
			std::advance(i, vi.index);
		} else {
		}
	}
	config::attribute_value cfg_formula = cfg["formula"];
	if (!cfg_formula.blank()) {
		return false;
	}

	config::attribute_value cfg_lua_function = cfg["lua_function"];
	if (!cfg_lua_function.blank()) {
		bool b = resources::lua_kernel->run_filter(cfg_lua_function.str().c_str(), *this);
		if (!b) return false;
	}

	return true;
}

void unit::write(config& cfg) const
{
	cfg.append(cfg_);
	// regardless city or troop, sub-unit cannot write to 
	cfg.clear_children("unit");

	cfg.clear_children("variables");
	// cfg.clear_children("modifications");

	const unit_type *ut = unit_type_;

	cfg["hitpoints"] = hit_points_;
	cfg["max_hitpoints"] = max_hit_points_;

	cfg["experience"] = experience_;
	cfg["max_experience"] = max_experience_;

	cfg["moves"] = movement_;
	cfg["max_moves"] = max_movement_;

	cfg["side"] = side_;

	if (packed()) {
		cfg["type"] = packee_type_;
	} else {
		cfg["type"] = type_;
	}

	cfg["especial"] = unit_types.especial_id(especial_);

	cfg["cityno"] = cityno_;

	// heros_army
	std::stringstream strstr;
	strstr.str("");
	strstr << master_->number_;
	if (second_->valid()) {
		strstr << "," << second_->number_;
	}
	if (third_->valid()) {
		strstr << "," << third_->number_;
	}
	cfg["heros_army"] = strstr.str();
	cfg["master_hero"] = master_->number_;

	strstr.str("");
	int n = 0;
	for (std::set<ustate_tag::state_t>::const_iterator it = ustate_tag::profilable_tags.begin(); it != ustate_tag::profilable_tags.end(); ++ it) {
		if (!has_state_flag(*it)) {
			continue;
		}
		if (n ++) {
			strstr << ",";
		}
		strstr << ustate_tag::rfind(*it);
	}
	cfg["states"] = strstr.str();

	cfg["facing"] = map_location::write_direction(facing_);

	cfg["goto_x"] = goto_.x + 1;
	cfg["goto_y"] = goto_.y + 1;

	cfg["resting"] = resting_;

	cfg["attacks_left"] = attacks_left_;
	cfg.clear_children("attack");
	for (std::vector<attack_type>::const_iterator i = attacks_.begin(); i != attacks_.end(); ++i) {
		config& att = cfg.add_child("attack",i->get_cfg());
		// don't save description of [specials]
		config& specials = att.child("specials");
		if (!specials) continue;
		BOOST_FOREACH (const config::any_child &sp, specials.all_children_range()) {
			const_cast<config *>(&sp.cfg)->remove_attribute("description");
		}
	}

	cfg["traits"] = utils::join(traits_);
	cfg["modifications"] = utils::join(modifications_);

	cfg["food"] = food_;
	cfg["hide_turns"] = hide_turns_;
	cfg["alert_turns"] = alert_turns_;
	cfg["task"] = task_;
	cfg["human"] = human_;

	// unit_merit
	cfg["field_turns"] = field_turns_;
	cfg["cause_damage"] = cause_damage_;
	cfg["been_damage"] = been_damage_;
	cfg["defeat_units"] = defeat_units_;
}

#define ATTACK_0POINT2		0.2
#define ATTACK_0POINT1		0.1
#define ATTACK_0POINT075	0.075

void unit::modify_according_to_hero(bool fill_up_hp, bool fill_up_movement)
{
	int current_movement = movement_;
	
	// Reset the scalar values first
	trait_names_.clear();

	// Apply modifications etc, refresh the unit.
	// This needs to be after type and gender are fixed,
	// since there can be filters on the modifications
	// that may result in different effects after the advancement.
	apply_modifications();

	// max_hit_points_, (原HP * (100 + (统率值-80))) / 100
	if (leadership_ > 80) {
		int bonus = (leadership_ - 80) / 2; // "/ 2" can extend effect range. max leadership is about 177.
		max_hit_points_ = max_hit_points_ * (100 + bonus) / 100;
	} else {
		// dont' forget it!!
		max_hit_points_ = max_hit_points_;
	}

	// max_movement_
	// max_experience_, (原XP * (100 - (魅力-100))) / 100
	if (charm_ > 90) {
		int bonus = (charm_ - 90) * 2 / 3; // "* 2 / 3" can extend effect range. max charm is about 177.
		max_experience_ = max_experience_ * std::max<int>(40, (100 - bonus)) / 100;
	} else {
		max_experience_ = max_experience_;
	}

	// max ticks
	if (is_city()) {
		max_ticks_ = game_config::ticks_per_turn * 2 / 3;
	} else if (consider_ticks()) {
		max_ticks_ = game_config::ticks_per_turn - 1.0 * spirit_ / 180 * (game_config::ticks_per_turn / 2);
	} else {
		max_ticks_ = NONACTOR_TICKS;
	}

	double arms_feature_bonus = unit_feature_val(arms_)? ATTACK_0POINT075: 0;
	double arms_feature_resistance_bonus = unit_feature_val(arms_)? RESISTANCE_DIV3: 0;
	if (unit_feature_val(HEROS_MAX_ARMS + arms_)) {
		arms_feature_bonus = ATTACK_0POINT1;
		arms_feature_resistance_bonus = RESISTANCE_DIV2;
	}
	double range_feature_bonus[3];
	range_feature_bonus[0] = unit_feature_val(hero_feature_range_min)? ATTACK_0POINT1: 0;
	if (unit_feature_val(3 + hero_feature_range_min)) {
		range_feature_bonus[0] = ATTACK_0POINT2;
	}
	range_feature_bonus[1] = unit_feature_val(hero_feature_range_min + 1)? ATTACK_0POINT1: 0;
	if (unit_feature_val(3 + hero_feature_range_min + 1)) {
		range_feature_bonus[1] = ATTACK_0POINT2;
	}
	range_feature_bonus[2] = unit_feature_val(hero_feature_range_min + 2)? ATTACK_0POINT1: 0;
	if (unit_feature_val(3 + hero_feature_range_min + 2)) {
		range_feature_bonus[2] = ATTACK_0POINT2;
	}

	// attack
	double hero_bonus = 0;
	if ((leadership_ > 80) || (force_ > 80) || (intellect_ > 80)) {
		int field = (std::max<uint16_t>(80, leadership_) - 80) * 60 + (std::max<uint16_t>(80, force_) - 80) * 20 + (std::max<uint16_t>(80, intellect_) - 80) * 20;
		hero_bonus = 1.0 * field * ATTACK_0POINT1 / (100 * (MAX_UNIT_FIELD - 80));
	}
	double adaptability_bonus = 1.0 * adaptability_[arms_] * ATTACK_0POINT1 / MAX_UNIT_ADAPTABILITY;
	for (std::vector<attack_type>::iterator a = attacks_.begin(); a != attacks_.end(); ++a) {
		double increase_percent = hero_bonus;

		// effect of adaptability
		increase_percent += adaptability_bonus;

		// effect of arms feature
		increase_percent += arms_feature_bonus;

		// effect of range feature
		const std::string& range = a->range();
		if (range == "melee") {
			increase_percent += range_feature_bonus[0];
		} else if (range == "ranged") {
			increase_percent += range_feature_bonus[1];
		} else {
			increase_percent += range_feature_bonus[2];
		}

		a->apply_modification_damage2(increase_percent);
	}

	// resistance
	hero_bonus = 0;
	if (leadership_ > 100) {
		hero_bonus = 1.0 * (leadership_ - 100) * RESISTANCE_DIV2 / (MAX_UNIT_FIELD - 100);
	}
	adaptability_bonus = 1.0 * adaptability_[arms_] * RESISTANCE_DIV2 / MAX_UNIT_ADAPTABILITY;
	config::child_itors cfg_range = cfg_.child_range("resistance");
	if (cfg_range.first != cfg_range.second) {
		config &target = *cfg_range.first;
		BOOST_FOREACH (const config::attribute &istrmap, base_resistance_.attribute_range()) {
			int value = lexical_cast<int>(istrmap.second);
			value -= hero_bonus + adaptability_bonus + arms_feature_resistance_bonus;

			// effect of other feature
			if (unit_feature_val(hero_feature_firm)) {
				value -= RESISTANCE_FIRM;
			}

			target[istrmap.first] = lexical_cast<std::string>(value);
		}
	}

	// modify according feature
	if (unit_feature_val(hero_feature_renown)) {
		max_hit_points_ += max_hit_points_ * 20 / 100;
	}
	if (unit_feature_val(hero_feature_innovate)) {
		max_experience_ = std::max<int>(16, max_experience_ * 80 / 100);
	}
	// if max_movement_ is 0, indicate it cannot move.
	if (max_movement_ && unit_feature_val(HEROS_MAX_ARMS * 2 + arms_)) {
		max_movement_ = max_movement_ + 1;
	}

	// redo technology
	if (!teams_.empty()) {
		int type = family_tag::ARTIFICAL;
		if (!is_artifical()) {
			type = family_tag::TROOP;
		} else if (is_city()) {
			type = family_tag::CITY;
		}
		int packee_arms = packee_type()->arms();
		const team& this_team = teams_[side_ - 1];

		const std::vector<const ttechnology*>& holded_technologies = this_team.holded_technologies();
		for (std::vector<const ttechnology*>::const_iterator it = holded_technologies.begin(); it != holded_technologies.end(); ++ it) {
			const std::vector<const ttechnology*>& parts = (*it)->parts();
			for (std::vector<const ttechnology*>::const_iterator it2 = parts.begin(); it2 != parts.end(); ++ it2) {
				const ttechnology& t = **it2;
				if (t.occasion() == ttechnology::MODIFY && t.filter(type, packee_arms)) {
					add_modification_internal(t.apply_to(), t.effect_cfg(), false);
				}
			}
		}
	}

	tactic_compare_damage_.clear();
	for (std::vector<attack_type>::iterator a = attacks_.begin(); a != attacks_.end(); ++a) {
		// remember for comapre in future.
		tactic_compare_damage_[a->id()] = a->damage();
	}
	tactic_compare_resistance_.clear();
	const config& resistance_cfg = cfg_.child("resistance");
	if (resistance_cfg) {
		BOOST_FOREACH (const config::attribute &istrmap, resistance_cfg.attribute_range()) {
			// remember for comapre in future.
			tactic_compare_resistance_[istrmap.first] = lexical_cast<int>(istrmap.second);
		}
	}
	tactic_compare_movement_ = max_movement_;

	// redo tactic
	ticks_increase_ = 0;
	for (std::vector<tunit_tactic>::const_iterator it = tactics_.begin(); it != tactics_.end(); ++ it) {
		const tunit_tactic& unit_t = *it;
		const ttactic* t;
		
		const std::vector<const ttactic*>& parts = unit_t.tactic_->parts();
		t = parts[unit_t.part_];

		apply_tactic(NULL, *t);
	}

	// redo AMLA
	const modifications_map& modifications = unit_types.modifications();
	// Only support a kind AMLA to a unit_type.
	config can_redo_amla;
	for (std::vector<std::string>::const_iterator it = modifications_.begin(); it != modifications_.end(); ++ it) {
		const config& item = modifications.find(*it)->second;
		if (can_redo_amla.empty()) {
			// amla config to can_redo_amla config is according to instance.
			can_redo_amla.merge_attributes(item);
			// need removed block: apply_to = status
			BOOST_FOREACH (const config &effect, item.child_range("effect")) {
				std::string apply_to = effect["apply_to"].str();
				if (apply_to != "status") {
					config& new1 = can_redo_amla.add_child("effect", effect);
					if (apply_to == "hitpoints") {
						new1.remove_attribute("increase");
					}
				} 
			}
		}
		// It is simulate advance, don't write to modifications_.[advance]
		add_modification(can_redo_amla, true);
	}

	int max = max_tactic_point() * game_config::tactic_degree_per_point;
	if (tactic_degree_ > max) {
		tactic_degree_ = max;
	}

	max = max_hot_turns();
	if (hot_turns_ > max) {
		hot_turns_ = max;
	}

	if (fill_up_hp) {
		hit_points_ = max_hit_points_;
	} else {
		if (hit_points_ > max_hit_points_) {
			hit_points_ = max_hit_points_;
		}
	}
	if (fill_up_movement) {
		movement_ = max_movement_;
	} if (current_movement > max_movement_) {
		movement_ = max_movement_;
	} else if (movement_ != current_movement) {
		movement_ = current_movement;
	}
}

// 2.5, 3, 3.5, 100
void unit::calculate_5fields()
{
	hero* leader = NULL;
	const team* t = NULL;
	if (!teams_.empty()) {
		leader = teams_[side_ - 1].leader();
		t = &(teams_[side_ - 1]);
	} else {
		leader = tent::leader;
	}

	hero_5fields_t max_assistant;
	uint32_t second_relation = 350, third_relation = 350;
	uint16_t tmp;
	int activity_ajdusted;
	bool second_valid = false, third_valid = false; 

	if (second_->valid()) {
		second_valid = true;
		if (master_->is_parent(*second_) || master_->is_consort(*second_) || master_->is_oath(*second_)) {
			second_relation = 250;
		} else if (master_->is_hate(*second_)) {
			second_relation = 10000; // a almost large value
		}
		activity_ajdusted = 1 * second_->leadership_ * second_->activity_ / HEROS_FULL_ACTIVITY;
		max_assistant.leadership_ = fxptoi9(activity_ajdusted) * 100 / second_relation;
		activity_ajdusted = 1 * second_->force_ * second_->activity_ / HEROS_FULL_ACTIVITY;
		max_assistant.force_ = fxptoi9(activity_ajdusted) * 100 / second_relation;
		activity_ajdusted = 1 * second_->intellect_ * second_->activity_ / HEROS_FULL_ACTIVITY;
		max_assistant.intellect_ = fxptoi9(activity_ajdusted) * 100 / second_relation;
		activity_ajdusted = 1 * second_->spirit_ * second_->activity_ / HEROS_FULL_ACTIVITY;
		max_assistant.spirit_ = fxptoi9(activity_ajdusted) * 100 / second_relation;
		activity_ajdusted = 1 * second_->charm_ * second_->activity_ / HEROS_FULL_ACTIVITY;
		max_assistant.charm_ = fxptoi9(activity_ajdusted) * 100 / second_relation;
	} else {
		memset(&max_assistant, 0, sizeof(hero_5fields_t));
	}
	if (third_->valid()) {
		third_valid = true;
		if (master_->is_parent(*third_) || master_->is_consort(*third_) || master_->is_oath(*third_)) {
			third_relation = 250;
		} else if (master_->is_hate(*third_)) {
			third_relation = 10000; // a almost large value
		}
		// Second and Third has same intimate. select better value every one
		activity_ajdusted = 1 * third_->leadership_ * third_->activity_ / HEROS_FULL_ACTIVITY;
		tmp = fxptoi9(activity_ajdusted) * 100 / third_relation;
		max_assistant.leadership_ = std::max<uint16_t>(max_assistant.leadership_, tmp);
		activity_ajdusted = 1 * third_->force_ * third_->activity_ / HEROS_FULL_ACTIVITY;
		tmp = fxptoi9(activity_ajdusted) * 100 / third_relation;
		max_assistant.force_ = std::max<uint16_t>(max_assistant.force_, tmp);
		activity_ajdusted = 1 * third_->intellect_ * third_->activity_ / HEROS_FULL_ACTIVITY;
		tmp = fxptoi9(activity_ajdusted) * 100 / third_relation;
		max_assistant.intellect_ = std::max<uint16_t>(max_assistant.intellect_, tmp);
		activity_ajdusted = 1 * third_->spirit_ * third_->activity_ / HEROS_FULL_ACTIVITY;
		tmp = fxptoi9(activity_ajdusted) * 100 / third_relation;
		max_assistant.spirit_ = std::max<uint16_t>(max_assistant.spirit_, tmp);
		activity_ajdusted = 1 * third_->charm_ * third_->activity_ / HEROS_FULL_ACTIVITY;
		tmp = fxptoi9(activity_ajdusted) * 100 / third_relation;
		max_assistant.charm_ = std::max<uint16_t>(max_assistant.charm_, tmp);
	}

	// leadership=master_leadership + secondary_leadership/relation
	activity_ajdusted = 1 * master_->leadership_ * master_->activity_ / HEROS_FULL_ACTIVITY;
	leadership_ = fxptoi9(activity_ajdusted) + max_assistant.leadership_;
	activity_ajdusted = 1 * master_->force_ * master_->activity_ / HEROS_FULL_ACTIVITY;
	force_ = fxptoi9(activity_ajdusted) + max_assistant.force_;
	activity_ajdusted = 1 * master_->intellect_ * master_->activity_ / HEROS_FULL_ACTIVITY;
	intellect_ = fxptoi9(activity_ajdusted) + max_assistant.intellect_;
	activity_ajdusted = 1 * master_->spirit_ * master_->activity_ / HEROS_FULL_ACTIVITY;
	spirit_ = fxptoi9(activity_ajdusted) + max_assistant.spirit_;
	activity_ajdusted = 1 * master_->charm_ * master_->activity_ / HEROS_FULL_ACTIVITY;
	charm_ = fxptoi9(activity_ajdusted) + max_assistant.charm_;

	// arms adaptability
	for (int arms = 0; arms < HEROS_MAX_ARMS; arms ++) {
		adaptability_[arms] = master_->arms_[arms];
		if (second_valid && second_->arms_[arms] > adaptability_[arms]) {
			adaptability_[arms] = second_->arms_[arms];
		}
		if (third_valid && third_->arms_[arms] > adaptability_[arms]) {
			adaptability_[arms] = third_->arms_[arms];
		}
		adaptability_[arms] = fxptoi12(adaptability_[arms]);
	}

	// skill adaptability
	for (int skill = 0; skill < HEROS_MAX_SKILL; skill ++) {
		skill_[skill] = master_->skill_[skill];
		if (second_valid && second_->skill_[skill] > skill_[skill]) {
			skill_[skill] = second_->skill_[skill];
		}
		if (third_valid && third_->skill_[skill] > skill_[skill]) {
			skill_[skill] = third_->skill_[skill];
		}
		skill_[skill] = fxptoi12(skill_[skill]);
	}

	// feature
	const complex_feature_map& complex_feature = unit_types.complex_feature();
	memset(feature_, 0, HEROS_FEATURE_M2BYTES);

	std::set<int> holded_features;
	if (master_->feature_ != HEROS_NO_FEATURE) {
		holded_features.insert(master_->feature_);
	}
	if (master_->treasure_ != HEROS_NO_TREASURE) {
		const ttreasure& t = unit_types.treasure(master_->treasure_);
		holded_features.insert(t.feature());
	}
	if (second_valid) {
		if (second_->feature_ != HEROS_NO_FEATURE) {
			holded_features.insert(second_->feature_);
		}
		if (second_->treasure_ != HEROS_NO_TREASURE) {
			const ttreasure& t = unit_types.treasure(second_->treasure_);
			holded_features.insert(t.feature());
		}
	}
	if (third_valid) {
		if (third_->feature_ != HEROS_NO_FEATURE) {
			holded_features.insert(third_->feature_);
		}
		if (third_->treasure_ != HEROS_NO_TREASURE) {
			const ttreasure& t = unit_types.treasure(third_->treasure_);
			holded_features.insert(t.feature());
		}
	}

	if (leader->side_feature_ != HEROS_NO_FEATURE) {
		holded_features.insert(leader->side_feature_);
	}
	if (t) {
		const std::vector<arms_feature>& features = t->features();
		for (std::vector<arms_feature>::const_iterator it = features.begin(); it != features.end(); ++ it) {
			const arms_feature& f = *it;
			if (arms_ == f.arms_ && level_ >= f.level_) {
				holded_features.insert(f.feature_);
			}
			if (packee_unit_type_ && packee_unit_type_->arms() == f.arms_ && packee_unit_type_->level() >= f.level_) {
				holded_features.insert(f.feature_);	
			}
		}
	}
	for (std::set<int>::const_iterator it = holded_features.begin(); it != holded_features.end(); ++ it) {
		tmp = *it;
		VALIDATE(tmp < HEROS_MAX_FEATURE, "error feature when calculate_5fields unit_type: " + unit_type_->id());
		unit_feature_set(tmp, hero_feature_single_result);
	
		// parse complex feature to base feature
		if (tmp >= HEROS_BASE_FEATURE_COUNT) {
			complex_feature_map::const_iterator complex_itor = complex_feature.find(tmp);
			if (complex_itor == complex_feature.end()) {
				continue;
			}
			for (std::vector<int>::const_iterator it2 = complex_itor->second.begin(); it2 != complex_itor->second.end(); ++ it2) {
				int single_feature = *it2;
				if (!unit_feature_val(single_feature)) {
					unit_feature_set(single_feature, hero_feature_complex_result);
				}
			}
		}
	}

	characters_.clear();
	for (int i = 0; i < 3; i ++) {
		hero* h = master_;
		if (i == 1) {
			if (!second_valid) continue;
			h = second_;
		} else if (i == 2) {
			if (!third_valid) continue;
			h = third_;
		}
		if (h->character_ != HEROS_NO_CHARACTER) {
			const tcharacter& ch = unit_types.character(h->character_);
			for (std::vector<const tcharacter*>::const_iterator it = ch.parts().begin(); it != ch.parts().end(); ++ it) {
				const tcharacter& ch = **it;
				int apply_to = ch.apply_to();
				int l = ch.level(*h);
				std::map<int, character_data>::iterator find = characters_.find(apply_to);
				if (find == characters_.end()) {
					characters_.insert(std::make_pair(apply_to, character_data(h, ch.index(), l)));
				} else if (l > find->second.level) {
					find->second = character_data(h, ch.index(), l);
				}
			}
		}
	}
}

void unit::adjust() 
{
	const unit_type* current_type = type();

	calculate_5fields();
	if (packed()) {
		int navigation = teams_[side_ - 1].navigation();
		const unit_type* to_type = unit_types.find(unit_types.id_from_navigation(navigation));
		pack_to(to_type);

		if (to_type != current_type) {
			// since appearance of unit has changed, current animation should became invalid.
			if (anim_) {
				delete anim_;
				anim_ = NULL;
			}
		}
	} else {
		max_hit_points_ = current_type->hitpoints();
		max_experience_ = current_type->experience_needed(false);
		max_movement_ = current_type->movement();
		attacks_ = current_type->attacks();

		modify_according_to_hero(false, false);
	}
}

bool unit::has_female() const
{
	if (master_->gender_ == hero_gender_female) {
		return true;
	}
	if (second_->valid() && second_->gender_ == hero_gender_female) {
		return true;
	}
	if (third_->valid() && third_->gender_ == hero_gender_female) {
		return true;
	}
	return false;
}

bool unit::has_guard_center() const
{
	const team& current_team = teams_[side_ - 1];
	const map_location* tiles = adjacent_;
	size_t adjance_size = adjacent_size_;
	for (int step = 0; step < 2; step ++) {
		if (step == 1) {
			tiles = adjacent_2_;
			adjance_size = adjacent_size_2_;
		}
		for (size_t adj = 0; adj < adjance_size; adj ++) {
			const map_location& loc = tiles[adj];
			const unit* that = units_.find_unit(loc);
			if (!that || current_team.is_enemy(that->side())) {
				continue;
			}
			if (that->is_city() || that->fort() || that->packee_type()->require() == unit_type::REQUIRE_LEADER) {
				return true;
			}
		}
	}
	return false;
}

int unit::tactic_effect(int type, const std::string& field) const
{
	std::map<int, std::string>::iterator find = ttactic::type_name_map.find(type);
	if (find == ttactic::type_name_map.end()) {
		return 0;
	}
	std::string apply_to = find->second;

	int ret = 0;

	for (std::vector<tunit_tactic>::const_iterator it = tactics_.begin(); it != tactics_.end(); ++ it) {
		const tunit_tactic& unit_t = *it;
		const ttactic* t;
		int compare;

		const std::vector<const ttactic*>& parts = unit_t.tactic_->parts();
		t = parts[unit_t.part_];

		if (type == ttactic::ATTACK) {
			for (std::vector<attack_type>::const_iterator a_it = attacks_.begin(); a_it != attacks_.end(); ++ a_it) {
				if (!field.empty() && a_it->id() != field) {
					continue;
				}
				compare = tactic_compare_damage_.find(a_it->id())->second;
				ret += a_it->damage() - compare;
			}
		} else if (type == ttactic::RESISTANCE) {
			const config& resistance = cfg_.child("resistance");
			BOOST_FOREACH (const config::attribute &i, resistance.attribute_range()) {
				if (!field.empty() && i.first != field) {
					continue;
				}
				compare = tactic_compare_resistance_.find(i.first)->second;
				ret += -1 * (i.second.to_int() - compare);
			}

		} else if (type == ttactic::ENCOURAGE) {
			compare = master_->skill_[hero_skill_encourage];
			if (second_->valid() && second_->skill_[hero_skill_encourage] > compare) {
				compare = second_->skill_[hero_skill_encourage];
			}
			if (third_->valid() && third_->skill_[hero_skill_encourage] > compare) {
				compare = third_->skill_[hero_skill_encourage];
			}
			compare = fxptoi12(compare);
			ret += skill_[hero_skill_encourage] - compare;

		} else if (type == ttactic::DEMOLISH) {
			compare = master_->skill_[hero_skill_demolish];
			if (second_->valid() && second_->skill_[hero_skill_demolish] > compare) {
				compare = second_->skill_[hero_skill_demolish];
			}
			if (third_->valid() && third_->skill_[hero_skill_demolish] > compare) {
				compare = third_->skill_[hero_skill_demolish];
			}
			compare = fxptoi12(compare);
			ret += skill_[hero_skill_demolish] - compare;
		}
	}

	return ret;
}

std::string unit::form_tip(bool gui2) const
{ 
	if (!gui2 && game_config::tiny_gui) {
		return form_tiny_tip();
	}

	team& current_team = teams_[side_ - 1];
	std::stringstream text, strstr, loyalty_str, activity_str;
	
	const artifical* owner_city = NULL;
	if (!artifical_) {
		owner_city = units_.city_from_cityno(cityno_);
		if (gui2) {
			strstr << tintegrate::generate_format(_("City"), "green");
		} else {
			strstr << _("City");
		}
		if (owner_city) {
			strstr << ": " << owner_city->master().name();
			if (gui2) {
				strstr << " (";
				strstr << (get_location() == owner_city->get_location()? _("Reside"): _("Field"));
				strstr << ")";
			}
		} else {
			strstr << ": --";
		}

		if (!gui2) {
			strstr << " (" << teams_[side_ - 1].name() << ")";
		}
		strstr << "\n";

		if (gui2) {
			strstr << tintegrate::generate_format(_("Hero"), "green");
		} else {
			strstr << _("Hero");
		}
		strstr << ": ";

		if (gui2 || master_->activity_ == 255) {
			strstr << master_->name();
		} else {
			strstr << tintegrate::generate_format(master_->name(), "red", 0, false, false);
		}
		if (!commoner_) {
			if (!gui2) {
				strstr << tintegrate::generate_format("(", "blue", 0, false, false);
				strstr << master_->loyalty(*teams_[master_->side_].leader());
				strstr << tintegrate::generate_format(")", "blue", 0, false, false);
			} else {
				loyalty_str << master_->loyalty(*teams_[master_->side_].leader());
				activity_str << (int)master_->activity_;
			}
		}
		if (second_->valid()) {
			if (!gui2) {
				strstr << "\n    ";
			} else {
				strstr << "    ";
			}
			if (gui2 || second_->activity_ == 255) {
				strstr << second_->name();
			} else {
				strstr << tintegrate::generate_format(second_->name(), "red", 0, false, false);
			}
			if (!commoner_) {
				if (!gui2) {
					strstr << tintegrate::generate_format("(", "blue", 0, false, false);
					strstr << second_->loyalty(*teams_[second_->side_].leader());
					strstr << tintegrate::generate_format(")", "blue", 0, false, false);
				} else {
					loyalty_str << ", " << second_->loyalty(*teams_[second_->side_].leader());
					activity_str << ", " << (int)second_->activity_;
				}
			}
		}
		if (third_->valid()) {
			strstr << "  ";
			if (gui2 || third_->activity_ == 255) {
				strstr << third_->name();
			} else {
				strstr << tintegrate::generate_format(third_->name(), "red", 0, false, false);
			}
			if (!commoner_) {
				if (!gui2) {
					strstr << tintegrate::generate_format("(", "blue", 0, false, false);
					strstr << third_->loyalty(*teams_[third_->side_].leader());
					strstr << tintegrate::generate_format(")", "blue", 0, false, false);
				} else {
					loyalty_str << ", " << third_->loyalty(*teams_[third_->side_].leader());
					activity_str << ", " << (int)third_->activity_;
				}
			}
		}

		if (gui2 && !commoner_) {
			strstr << "\n";

			// loyalty
			strstr << tintegrate::generate_format(_("Loyalty"), "green");
			strstr << ": (" << loyalty_str.str() << ")\n";

			// activity
			strstr << tintegrate::generate_format(_("Activity"), "green");
			strstr << ": (" << activity_str.str() << ")";
		}

	} else if (this_is_city()) {
		const artifical& city = *const_unit_2_artifical(this);

		owner_city = &city;
		if (gui2) {
			strstr << tintegrate::generate_format(_("City"), "green");
		} else {
			strstr << _("City");
		}
		if (owner_city) {
			strstr << ": " << owner_city->master().name();
		} else {
			strstr << ": --";
		}

		strstr << " (" << teams_[side_ - 1].name() << ")\n";

		if (gui2) {
			strstr << tintegrate::generate_format(_("Mayor"), "green");
		} else {
			strstr << _("Mayor");
		}
		strstr << ": ";
		if (city.mayor()->valid()) {
			strstr << city.mayor()->name();
		} else {
			strstr << _("Void");
		}

	} else {
		const artifical& art = *const_unit_2_artifical(this);

		owner_city = units_.city_from_cityno(art.cityno());
		if (gui2) {
			strstr << tintegrate::generate_format(_("City"), "green");
		} else {
			strstr << _("City");
		}
		if (owner_city) {
			strstr << ": " << owner_city->master().name();
		} else {
			strstr << ": --";
		}

		strstr << " (" << teams_[side_ - 1].name() << ")";

	}
	strstr << "\n";

	if (gui2) {
		strstr << tintegrate::generate_format(_("troop^Type"), "green");
	} else {
		strstr << _("troop^Type");
	}
	if (!packed()) {
		strstr << ": " << type_name() << "(Lv" << level() << ")";
	} else {
		strstr << ": " << packee_unit_type_->type_name() << "(Lv" << level() << ")";
		strstr << "[" << type_name() << "]";
	}
	// adaptability
	strstr << "(" << hero::adaptability_str2(ftofxp12(adaptability_[arms()])) << ")" << "\n";

	strstr << tintegrate::generate_img("misc/tintegrate-split-line.png");
	strstr << "\n";

	if (gui2 || !artifical_) {
		// leadership
		strstr << tintegrate::generate_img("misc/leadership.png~SCALE(20, 20)");
		strstr << leadership_;
		strstr << (gui2? " ": "  ");

		strstr << tintegrate::generate_img("misc/force.png~SCALE(20, 20)");
		strstr << force_;
		strstr << (gui2? " ": "  ");

		strstr << tintegrate::generate_img("misc/intellect.png~SCALE(20, 20)");
		strstr << intellect_;
		strstr << (gui2? " ": "\n");

		strstr << tintegrate::generate_img("misc/spirit.png~SCALE(20, 20)");
		strstr << spirit_;
		strstr << (gui2? " ": "  ");

		strstr << tintegrate::generate_img("misc/charm.png~SCALE(20, 20)");
		strstr << charm_ << "\n";
	}

	if (gui2) {
		// traits
		strstr << tintegrate::generate_format(_("Traits"), "green");
		strstr << ": " << utils::join(trait_names(), ", ");
		strstr << "\n";
	} else {
		strstr << "HP: " << hitpoints() << "/";
		text.str("");
		text << max_hitpoints();
		// default font_size is font::SIZE_SMALL
		strstr << tintegrate::generate_format(text.str(), "", 0, false, false);
		strstr << "  ";

		strstr << "XP: " << experience() << "/";
		text.str("");
		text << max_experience();
		strstr << tintegrate::generate_format(text.str(), "", 0, false, false);
		strstr << "\n";
	}

	if (!is_artifical()) {
		if (gui2) {
			strstr << tintegrate::generate_format(_("Moves"), "green");
			strstr << ": " << movement_left() << "/" << total_movement() << "  ";
		}
	
		if (!owner_city || get_location() != owner_city->get_location()) {
			strstr << tintegrate::generate_img("misc/food.png") << food();
			strstr << "/" << max_food() << "  ";
		}
	}

	// AMLA
	if (gui2) {
		strstr << tintegrate::generate_format(_("AMLA"), "green");
	} else {
		strstr << _("AMLA");
	}
	strstr << ": " << amla();
	strstr << "\n";

	std::map<int, std::string> task_str;
	task_str.insert(std::make_pair((int)TASK_NONE, _("None")));
	task_str.insert(std::make_pair((int)TASK_BACK, _("task^Back")));
	task_str.insert(std::make_pair((int)TASK_TRADE, _("task^Trade")));
	task_str.insert(std::make_pair((int)TASK_TRANSFER, _("task^Transfer")));
	task_str.insert(std::make_pair((int)TASK_STATION, _("task^Station")));
	task_str.insert(std::make_pair((int)TASK_GUARD, _("Guard")));
	if (!artifical_ && (commoner_ || gui2)) {
		if (commoner_) {
			if (gui2) {
				strstr << tintegrate::generate_format(_("Task"), "green");
			} else {
				strstr << _("Task");
			} 
			strstr << ": " << task_str.find(task_)->second;
			if (task_ == TASK_TRADE) {
				if (from_.valid()) {
					strstr << "(" << _("trade^Back") << ")";
				}
			}
		} else {
			strstr << tintegrate::generate_format(_("Task"), "green");
			strstr << ": " << task_str.find(task_)->second;
		}
		if (task_ == TASK_GUARD) {
			strstr << format_loc(units_, guard_cache::find(*this).loc, side_);
		}

		strstr << tintegrate::generate_img("misc/to.png");
		if (goto_.valid()) {
			strstr << format_loc(units_, goto_, side_);
		} else {
			strstr << "(-)";
		}
		strstr << "\n";
	}

	if (commoner_) {
		if (gui2) {
			strstr << tintegrate::generate_format(_("Income"), "green");
		} else {
			strstr << _("Income");
		}
		int income = gold_income()? gold_income(): technology_income();
		if (task_ == TASK_TRADE) {
			if (!from_.valid()) {
				income /= 2;
			}
		} else {
			income = 0;
		}
		strstr << ": " << income << "\n";
		if (gui2) {
			strstr << tintegrate::generate_format(_("Block turns"), "green");
		} else {
			strstr << _("Block turns");
		}
		strstr << ": " << block_turns_ << "\n";
	} else if (this_is_city()) {
		const artifical* city = const_unit_2_artifical(this);
		if (gui2) {
			strstr << tintegrate::generate_format(_("Decree"), "green");
		} else {
			strstr << _("Decree");
		}
		strstr << ": ";
		if (city->decree()) {
			strstr << city->decree()->name();
		} else {
			strstr << _("None");
		}
		strstr << "\n";
		if (gui2) {
			strstr << tintegrate::generate_format(_("Police"), "green");
		} else {
			strstr << _("Police");
		}
		strstr << ": ";
		if (city->police() < game_config::min_tradable_police) {
			strstr << tintegrate::generate_format(city->police(), "red", 0, false, false);
		} else {
			strstr << tintegrate::generate_format(city->police(), "white", 0, false, false);
		}
		strstr << "\n";

		// soldiers
		if (gui2) {
			strstr << tintegrate::generate_format(_("Soldiers"), "green");
		} else {
			strstr << _("Soldiers");
		}
		strstr << ": " << city->current_soldiers() << "/" << city->soldiers() << "\n";

		// commoner
		if (gui2) {
			strstr << tintegrate::generate_format(_("Commoner"), "green");
		} else {
			strstr << _("Commoner");
		}
		strstr << ": " << city->field_commoners().size() + city->reside_commoners().size() << "\n";

		// front
		if (gui2) {
			strstr << tintegrate::generate_format(_("Front"), "green");
		} else {
			strstr << _("Front");
		}
		strstr << ": " << city->fronts() << "/" << mr_data::max_fronts << "\n";
		if (gui2) {
			strstr << tintegrate::generate_format(_("Gold income"), "green");
		} else {
			strstr << _("Gold income");
		}
		strstr << ": " << city->total_gold_income(current_team.market_increase_) << "\n";
		if (gui2) {
			strstr << tintegrate::generate_format(_("Technology income"), "green");
		} else { 
			strstr << _("Technology income");
		} 
		strstr << ": " << city->total_technology_income(current_team.technology_increase_) << "\n";
	} else if (artifical_) {
		if (unit_type_->gold_income()) {
			if (gui2) {
				strstr << tintegrate::generate_format(_("Gold income"), "green");
			} else {
				strstr << _("Gold income");
			}
			strstr << ": " << gold_income() << "\n";
		}
		if (unit_type_->technology_income()) {
			if (gui2) {
				strstr << tintegrate::generate_format(_("Technology income"), "green");
			} else { 
				strstr << _("Technology income");
			} 
			strstr << ": " << technology_income() << "\n";
		}
	}


	// abilities
	std::vector<std::string> abilities_tt = ability_tooltips(true);

	if (gui2) {
		strstr << tintegrate::generate_format(_("Abilities"), "green");
	} else {
		strstr << _("Abilities");
	}
	strstr << ": ";
	if (!abilities_tt.empty()) {
		std::vector<t_string> abilities;
		for (std::vector<std::string>::const_iterator a = abilities_tt.begin(); a != abilities_tt.end(); a += 2) {
			abilities.push_back(*a);
		}

		for (std::vector<t_string>::const_iterator a = abilities.begin(); a != abilities.end(); a++) {
			if (a != abilities.begin()) {
				if (gui2 || a - abilities.begin() != 2) {
					strstr << ", ";
				} else {
					strstr << "\n    ";
				}
			}
			strstr << (*a);
		}
	}
	strstr << "\n";

	// features
	if (gui2) {
		strstr << tintegrate::generate_format(dgettext("wesnoth-hero", "feature"), "green");
	} else {
		strstr << dgettext("wesnoth-hero", "feature");
	}
	strstr << ": ";
	int features = 0;
	for (int i = 0; i < HEROS_MAX_FEATURE; i ++) {
		if (unit_feature_val(i) == hero_feature_single_result) {
			if (features) {
				if (!gui2 && features == 8) {
					strstr << " ...";
					break;
				} else if (gui2 || features % 2) {
					strstr << ", ";
				} else {
					strstr << "\n    ";
				}
			}
			features ++;
			strstr << master().feature_str(i);
		}
	}
	strstr << "\n";

	// tactic
	if (!artifical_) {
		if (gui2) {
			strstr << tintegrate::generate_format(dgettext("wesnoth-hero", "tactic"), "green");
		} else {
			strstr << dgettext("wesnoth-hero", "tactic");
		}

		if (tactic_existed()) {
			int turns = 100 * (max_tactic_point() * game_config::tactic_degree_per_point - tactic_degree_) / tactic_degree_increment_per_turn();
			turns = turns % 100? (turns / 100 + 1): (turns / 100);
			if (turns) {
				strstr << "(" << tintegrate::generate_img("misc/tactic.png") << tintegrate::generate_img(single_digit_image(turns)) << ")";
				// strstr << "(" << tactic_degree_ << "/" << max_tactic_point() * game_config::tactic_degree_per_point << ")";
			}
		}

		strstr << ": ";
		std::set<const ttactic*> holded;
		if (master_->tactic_ != HEROS_NO_TACTIC) {
			holded.insert(&unit_types.tactic(master_->tactic_));
		}
		if (second_->valid() && second_->tactic_ != HEROS_NO_TACTIC) {
			holded.insert(&unit_types.tactic(second_->tactic_));
		}
		if (third_->valid() && third_->tactic_ != HEROS_NO_TACTIC) {
			holded.insert(&unit_types.tactic(third_->tactic_));
		}
		for (std::set<const ttactic*>::iterator it = holded.begin(); it != holded.end(); ++ it) {
			const ttactic* t = *it;
			int index = std::distance(holded.begin(), it);
			if (gui2) {
				if (index == 2) {
					strstr << "\n";
				} else if (index != 0) {
					strstr << ", ";
				}
			} else {
				if (index == 1) {
					strstr << "\n    ";
				} else if (index != 0) {
					strstr << ", ";
				}
			}
			strstr << t->name();
		}
		strstr << "\n";
	}

/*
	for (std::map<int, character_data>::const_iterator it = characters_.begin(); it != characters_.end(); ++ it) {
		const tcharacter& ch = unit_types.character(it->second.ch);
		hero& h = *it->second.h;
		if (it == characters_.begin()) {
			strstr << ch.name() << "(" << h.name() << ")" << "(" << it->second.level << ")";
		} else {
			strstr << ", " << ch.name() << "(" << h.name() << ")" << "(" << it->second.level << ")";
		}
	}

	strstr << "   (" << provoke_cache.size() << ")";
	strstr << "\n";
*/

	// skill
	if (gui2) {
		strstr << tintegrate::generate_format(_("Skill"), "green");
	} else {
		strstr << _("Skill");
	}
	strstr << ": ";
	strstr << tintegrate::generate_img("misc/encourage.png");

	int effect = tactic_effect(ttactic::ENCOURAGE, "");
	text.str("");
	text << hero::adaptability_str2(ftofxp12(value_consider_food(skill_[hero_skill_encourage])));
	if (effect > 0) {
		strstr << tintegrate::generate_format(text.str(), "green", 0, false, false);
	} else if (effect < 0) {
		strstr << tintegrate::generate_format(text.str(), "red", 0, false, false);
	} else {
		strstr << hero::adaptability_str2(ftofxp12(value_consider_food(skill_[hero_skill_encourage])));
	}

	if (!artifical_) {
		strstr << "  ";
		strstr << tintegrate::generate_img("misc/demolish.png");
		
		effect = tactic_effect(ttactic::DEMOLISH, "");
		text.str("");
		text << hero::adaptability_str2(ftofxp12(skill_[hero_skill_demolish]));
		if (effect > 0) {
			strstr << tintegrate::generate_format(text.str(), "green", 0, false, false);
		} else if (effect < 0) {
			strstr << tintegrate::generate_format(text.str(), "red", 0, false, false);
		} else {
			strstr << hero::adaptability_str2(ftofxp12(skill_[hero_skill_demolish]));
		}		

		strstr << "  ";
		strstr << tintegrate::generate_img("misc/formation.png");
		strstr << hero::adaptability_str2(ftofxp12(skill_[hero_skill_formation]));
	}
	strstr << "\n";

	strstr << tintegrate::generate_img("misc/tintegrate-split-line.png");
	strstr << "\n";
	
	// attack 
	for (std::vector<attack_type>::const_iterator at_it = attacks_.begin(); at_it != attacks_.end(); ++ at_it) {
		// see generate_report() in generate_report.cpp
		strstr << at_it->name() << " (" << _(at_it->type().c_str()) << ")";
		if (!at_it->attack_weight()) {
			strstr << tintegrate::generate_img("misc/attack.png~GS()");
		}
		strstr << "\n";

		std::string accuracy = at_it->accuracy_parry_description();
		if(accuracy.empty() == false) {
			accuracy += " ";
		}

		strstr << "  ";
		int effect = tactic_effect(ttactic::ATTACK, at_it->id());
		text.str("");
		text << value_consider_food(at_it->damage());
		if (effect > 0) {
			strstr << tintegrate::generate_format(text.str(), "green", 0, false, false);
		} else if (effect < 0) {
			strstr << tintegrate::generate_format(text.str(), "red", 0, false, false);
		} else {
			strstr << value_consider_food(at_it->damage());
		}
		strstr << "-" << at_it->num_attacks()
			<< " " << accuracy << "-- " << _(at_it->range().c_str());

		std::string special = at_it->weapon_specials(true);
		if (!special.empty()) {
			strstr << "(" << special << ")";
		}
		strstr << "\n";
	}

	if (gui2) {
		strstr << tintegrate::generate_img("misc/tintegrate-split-line.png");
		strstr << "\n";
		// resistance
		int idx_in_resistances;
		std::set<std::string> resistances_table;
		utils::string_map resistances = get_base_resistances();
		bool att_def_diff = false;
		const map_location& loc = get_location();
		idx_in_resistances = 0;
		for (utils::string_map::iterator resist = resistances.begin(); resist != resistances.end(); ++resist, idx_in_resistances ++) {
			text.str("");
			text << "misc/resistance-" << resist->first << ".png";
			strstr << tintegrate::generate_img(text.str());

			const map_location& loc = get_location();
			if (loc.valid()) {
				// Some units have different resistances when
				// attacking or defending.
				int res_att = 100 - value_consider_food(resistance_against(resist->first, true, loc), false);
				int res_def = 100 - value_consider_food(resistance_against(resist->first, false, loc), false);
				if (res_att == res_def) {
					strstr << res_def;
				} else {
					strstr << res_att << "% / " << res_def; // (Att / Def)
					att_def_diff = true;
				}
			} else {
				strstr << 100 - lexical_cast_default<int>(resist->second.c_str());
			}
			if (idx_in_resistances % 2) {
				strstr << "%\n";
			} else {
				strstr << "%  ";
			}
		}
	} else {
		strstr << tintegrate::generate_img("misc/resistance.png");
		effect = tactic_effect(ttactic::RESISTANCE, "");
		if (effect > 0) {
			strstr << tintegrate::generate_img("misc/mini-increase.png", tintegrate::BACK);
		} else if (effect < 0) {
			strstr << tintegrate::generate_img("misc/mini-decrease.png", tintegrate::BACK);
		}

		if (!artifical_) {
			strstr << tintegrate::generate_img("misc/movement.png");
			if (max_movement_ > tactic_compare_movement_) {
				strstr << tintegrate::generate_img("misc/mini-increase.png", tintegrate::BACK);
			} else if (max_movement_ < tactic_compare_movement_) {
				strstr << tintegrate::generate_img("misc/mini-decrease.png", tintegrate::BACK);
			}
		}

		if (consider_ticks()) {
			strstr << tintegrate::generate_img("misc/action.png");
			if (ticks_increase_ > 0) {
				strstr << tintegrate::generate_img("misc/mini-increase.png", tintegrate::BACK);
			} else if (ticks_increase_ < 0) {
				strstr << tintegrate::generate_img("misc/mini-decrease.png", tintegrate::BACK);
			}
		}
		strstr << "    ";

		if (hide_turns_) {
			strstr << tintegrate::generate_img("misc/hide.png");
		}
		if (alert_turns_) {
			strstr << tintegrate::generate_img("misc/alert.png");
		}
		if (provoked_turns_) {
			strstr << tintegrate::generate_img("misc/provoked.png");
		}
		if (invisible(loc_)) {
			strstr << tintegrate::generate_img("misc/invisible.png");
		}
		if (get_state(ustate_tag::SLOWED)) {
			strstr << tintegrate::generate_img("misc/slowed.png");
		}
		if (get_state(ustate_tag::BROKEN)) {
			strstr << tintegrate::generate_img("misc/broken.png");
		}
		if (get_state(ustate_tag::POISONED)) {
			strstr << tintegrate::generate_img("misc/poisoned.png");
		}
		if (get_state(ustate_tag::PETRIFIED)) {
			strstr << tintegrate::generate_img("petrified.png");
		}
	}

	return strstr.str();
}

std::string unit::form_tiny_tip() const
{
	std::stringstream strstr;

	strstr << tintegrate::generate_img("misc/attack.png");
	int effect = tactic_effect(ttactic::ATTACK, "");
	if (effect > 0) {
		strstr << tintegrate::generate_img("misc/mini-increase.png", tintegrate::BACK);
	} else if (effect < 0) {
		strstr << tintegrate::generate_img("misc/mini-decrease.png", tintegrate::BACK);
	}
	strstr << " ";

	strstr << tintegrate::generate_img("misc/resistance.png");
	effect = tactic_effect(ttactic::RESISTANCE, "");
	if (effect > 0) {
		strstr << tintegrate::generate_img("misc/mini-increase.png", tintegrate::BACK);
	} else if (effect < 0) {
		strstr << tintegrate::generate_img("misc/mini-decrease.png", tintegrate::BACK);
	}
	strstr << " ";

	strstr << tintegrate::generate_img("misc/encourage.png");
	effect = tactic_effect(ttactic::ENCOURAGE, "");
	if (effect > 0) {
		strstr << tintegrate::generate_img("misc/mini-increase.png", tintegrate::BACK);
	} else if (effect < 0) {
		strstr << tintegrate::generate_img("misc/mini-decrease.png", tintegrate::BACK);
	}
	
	strstr << " ";
	strstr << tintegrate::generate_img("misc/demolish.png");
	effect = tactic_effect(ttactic::DEMOLISH, "");
	if (effect > 0) {
		strstr << tintegrate::generate_img("misc/mini-increase.png", tintegrate::BACK);
	} else if (effect < 0) {
		strstr << tintegrate::generate_img("misc/mini-decrease.png", tintegrate::BACK);
	}

	if (!artifical_) {
		strstr << " ";
		strstr << tintegrate::generate_img("misc/movement.png");
		if (max_movement_ > tactic_compare_movement_) {
			strstr << tintegrate::generate_img("misc/mini-increase.png", tintegrate::BACK);
		} else if (max_movement_ < tactic_compare_movement_) {
			strstr << tintegrate::generate_img("misc/mini-decrease.png", tintegrate::BACK);
		}
	}

	if (consider_ticks()) {
		strstr << tintegrate::generate_img("misc/action.png");
		if (ticks_increase_ > 0) {
			strstr << tintegrate::generate_img("misc/mini-increase.png", tintegrate::BACK);
		} else if (ticks_increase_ < 0) {
			strstr << tintegrate::generate_img("misc/mini-decrease.png", tintegrate::BACK);
		}
	}

	if (tactic_existed()) {
		int turns = 100 * (max_tactic_point() * game_config::tactic_degree_per_point - tactic_degree_) / tactic_degree_increment_per_turn();
		turns = turns % 100? (turns / 100 + 1): (turns / 100);
		if (turns) {
			strstr << " ";
			strstr << tintegrate::generate_img("misc/tactic.png") << tintegrate::generate_img(single_digit_image(turns));
		}
	}

	bool first_state_icon = true;
	
	if (hide_turns_) {
		if (first_state_icon) {
			strstr << "    ";
			first_state_icon = false;
		}
		strstr << tintegrate::generate_img("misc/hide.png");
	}
	if (alert_turns_) {
		if (first_state_icon) {
			strstr << "    ";
			first_state_icon = false;
		}
		strstr << tintegrate::generate_img("misc/alert.png");
	}
	if (provoked_turns_) {
		if (first_state_icon) {
			strstr << "    ";
			first_state_icon = false;
		}
		strstr << tintegrate::generate_img("misc/provoked.png");
	}
	if (invisible(loc_)) {
		if (first_state_icon) {
			strstr << "    ";
			first_state_icon = false;
		}
		strstr << tintegrate::generate_img("misc/invisible.png");
	}
	if (get_state(ustate_tag::SLOWED)) {
		if (first_state_icon) {
			strstr << "    ";
			first_state_icon = false;
		}
		strstr << tintegrate::generate_img("misc/slowed.png");
	}
	if (get_state(ustate_tag::BROKEN)) {
		if (first_state_icon) {
			strstr << "    ";
			first_state_icon = false;
		}
		strstr << tintegrate::generate_img("misc/broken.png");
	}
	if (get_state(ustate_tag::POISONED)) {
		if (first_state_icon) {
			strstr << "    ";
			first_state_icon = false;
		}
		strstr << tintegrate::generate_img("misc/poisoned.png");
	}
	if (get_state(ustate_tag::PETRIFIED)) {
		if (first_state_icon) {
			strstr << "    ";
			first_state_icon = false;
		}
		strstr << tintegrate::generate_img("petrified.png");
	}
	return strstr.str();
}

bool unit::wall2() const 
{ 
	return wall_ && !get_state(ustate_tag::BUILDING);
}

// mover can stand on this or not. (stop at)
bool unit::can_stand(const unit& mover) const 
{
	const team& mover_team = teams_[mover.side() - 1];
	if (fort_) {
		if (mover.transport() && side_ == mover.side()) {
			return true;
		}
	} else if (wall2()) {
		if (mover.land_wall() && (!mover_team.is_enemy(side_) || mover_team.land_enemy_wall_)) {
			return true;
		}
	}
	return false;
}

std::string unit::form_recruit_tip() const
{
	std::stringstream strstr;
	// hp
	strstr << "HP: " << max_hitpoints() << "\n";
	strstr << "XP: " << max_experience() << "\n";
	strstr << tintegrate::generate_img("misc/movement.png");
	strstr << total_movement() << "\n";

	// arm adaptability
	strstr << tintegrate::generate_format(_("troop^Type"), "green");
	strstr << ": ";
	if (!game_config::tiny_gui) {
		strstr << type_name();
	}
	strstr << "(Lv" << level() << ")";
	strstr << "(" << hero::adaptability_str2(ftofxp12(adaptability_[arms()])) << ")" << "\n";

	// abilities
	strstr << tintegrate::generate_img("misc/tintegrate-split-line.png");
	strstr << "\n";
	strstr << tintegrate::generate_format(_("Abilities"), "green");
	strstr << ": ";
	std::vector<std::string> abilities_tt;
	abilities_tt = ability_tooltips(true);
	if (!abilities_tt.empty()) {
		std::vector<t_string> abilities;
		for (std::vector<std::string>::const_iterator a = abilities_tt.begin(); a != abilities_tt.end(); a += 2) {
			abilities.push_back(*a);
		}

		for (std::vector<t_string>::const_iterator a = abilities.begin(); a != abilities.end(); a++) {
			if (a != abilities.begin()) {
				strstr << ", ";
			}
			strstr << (*a);
		}
	} else {
		strstr << "\n";
	}
	strstr << "\n";
	
	// feature
	strstr << tintegrate::generate_format(dgettext("wesnoth-hero", "feature"), "green");
	strstr << ": ";

	int index = 0;
	for (int i = 0; i < HEROS_MAX_FEATURE; i ++) {
		if (unit_feature_val(i) == hero_feature_single_result) {
			if (index > 0) {
				if (game_config::tiny_gui && !(index % 2)) {
					strstr << "\n";
				} else {
					strstr << ", ";
				}
			}
			index ++;
			strstr << master().feature_str(i);
		}
	}
	if (!index) {
		strstr << "\n";
	}
	strstr << "\n";

	// tactic
	strstr << tintegrate::generate_format(dgettext("wesnoth-hero", "tactic"), "green");
	strstr << ": ";

	std::set<const ttactic*> holded;
	if (master().tactic_ != HEROS_NO_TACTIC) {
		holded.insert(&unit_types.tactic(master().tactic_));
	}
	if (second().valid() && second().tactic_ != HEROS_NO_TACTIC) {
		holded.insert(&unit_types.tactic(second().tactic_));
	}
	if (third().valid() && third().tactic_ != HEROS_NO_TACTIC) {
		holded.insert(&unit_types.tactic(third().tactic_));
	}
	for (std::set<const ttactic*>::iterator it = holded.begin(); it != holded.end(); ++ it) {
		const ttactic* t = *it;
		int index = std::distance(holded.begin(), it);
		if (index != 0) {
			strstr << ", ";
		}
		strstr << t->name();
	}
	if (holded.empty()) {
		strstr << "\n";
	}
	strstr << "\n";

	// attack
	strstr << tintegrate::generate_img("misc/tintegrate-split-line.png");
	strstr << "\n";
	std::vector<attack_type>* attacks_ptr = const_cast<std::vector<attack_type>*>(&attacks());
	for (std::vector<attack_type>::const_iterator at_it = attacks_ptr->begin(); at_it != attacks_ptr->end(); ++at_it) {
		// see generate_report() in generate_report.cpp
		strstr << at_it->name() << " (" << _(at_it->type().c_str()) << ")\n";

		std::string accuracy = at_it->accuracy_parry_description();
		if(accuracy.empty() == false) {
			accuracy += " ";
		}

		strstr << "  " << at_it->damage() << "-" << at_it->num_attacks()
			<< " " << accuracy << "- " << _(at_it->range().c_str());

		std::string special = at_it->weapon_specials(true);
		if (!special.empty()) {
			strstr << "(" << special << ")";
		}
		strstr << "\n";
	}
	strstr << "\n";
	
	// resistance
	utils::string_map resistances = get_base_resistances();
	bool att_def_diff = false;
	const map_location& loc = get_location();
	int idx_in_resistances = 0;
	std::stringstream text;
	for (utils::string_map::iterator resist = resistances.begin(); resist != resistances.end(); ++ resist, idx_in_resistances ++) {
		text.str("");
		text << "misc/resistance-" << resist->first << ".png";
		strstr << tintegrate::generate_img(text.str());
		if (loc.valid()) {
			// Some units have different resistances when
			// attacking or defending.
			int res_att = 100 - resistance_against(resist->first, true, loc);
			int res_def = 100 - resistance_against(resist->first, false, loc);
			if (res_att == res_def) {
				strstr << res_def;
			} else {
				strstr << res_att << "% / " << res_def; // (Att / Def)
				att_def_diff = true;
			}
		} else {
			strstr << 100 - lexical_cast_default<int>(resist->second.c_str());
		}
		strstr << "%";
		if (idx_in_resistances & 1) {
			strstr << "\n";
		} else {
			strstr << "  ";
		}
	}
	return strstr.str();
}

int unit::weapon(const std::string& type_id, const std::string& range) const
{
	int index = 0;
	for (std::vector<attack_type>::const_iterator it = attacks_.begin(); it != attacks_.end(); ++ it, index ++) {
		if (!it->attack_weight()) {
			continue;
		}
		if (!type_id.empty() && it->type() != type_id) {
			continue;
		}
		if (!range.empty() && it->range() != range) {
			continue;
		}
		return index;
	}
	return -1;
}

int unit::weapon(const std::set<std::string>& type_id, const std::string& range) const
{
	int index = 0;
	for (std::vector<attack_type>::const_iterator it = attacks_.begin(); it != attacks_.end(); ++ it, index ++) {
		if (!it->attack_weight()) {
			continue;
		}
		if (!type_id.empty() && type_id.find(it->type()) == type_id.end()) {
			continue;
		}
		if (!range.empty() && it->range() != range) {
			continue;
		}
		return index;
	}
	return -1;
}

bool unit::has_attack_weapon() const
{
	for (std::vector<attack_type>::const_iterator it = attacks_.begin(); it != attacks_.end(); ++ it) {
		if (it->attack_weight()) {
			return true;
		}
	}
	return false;
}

const surface unit::still_image(bool scaled) const
{
	return surface(image::get_image(master_->image()));
	
}

void unit::set_standing(bool with_bars)
{
	game_display *disp = resources::screen;

	if (!incapacitated()) {
		start_animation(INT_MAX, choose_animation(*disp, loc_, "standing"),
			with_bars, true, "", 0, STATE_STANDING);
	} else {
		start_animation(INT_MAX, choose_animation(*disp, loc_, "_disabled_"),
			with_bars, true, "", 0, STATE_STANDING);
	}
}

void unit::set_ghosted(bool with_bars)
{
	game_display *disp = resources::screen;
	start_animation(INT_MAX, choose_animation(*disp, loc_, "ghosted"),
			with_bars, true);
}

void unit::set_disabled_ghosted(bool with_bars)
{
	game_display *disp = resources::screen;
	start_animation(INT_MAX, choose_animation(*disp, loc_, "disabled_ghosted"),
			with_bars, true);
}

void unit::set_idling()
{
	game_display *disp = resources::screen;
	start_animation(INT_MAX, choose_animation(*disp, loc_, "idling"),
		true, false, "", 0, STATE_FORGET);
}

void unit::set_selecting()
{
	const game_display *disp =  resources::screen;
	if (!get_state(ustate_tag::PETRIFIED)) {
		start_animation(INT_MAX, choose_animation(*disp, loc_, "selected"),
			true, false, "", 0, STATE_FORGET);
	} else {
		start_animation(INT_MAX, choose_animation(*disp, loc_, "_disabled_selected_"),
			true, false, "", 0, STATE_FORGET);
	}
}

animation* unit::create_animation(const animation& anim)
{
	return new unit_animation(anim);
}

int unit::calculate_next_idling() const
{
	return get_current_animation_tick() + (artifical_? 0: static_cast<int>(2000 + rand() % 3000));
}

void unit::redraw_unit()
{
	game_display& disp = *resources::screen;
	const gamemap& map = disp.get_map();
	
	if (!loc_.valid() || hidden_ || disp.fogged(loc_) ||
	    (invisible(loc_)
	&& disp.get_teams()[disp.viewing_team()].is_enemy(side())))
	{
		clear_haloes();
		if(anim_) {
			anim_->update_last_draw_time();
		}
		return;
	}

	if (!anim_) {
		set_standing();
		if (!anim_) return;
	}

	if (refreshing_) return;
	refreshing_ = true;
	redraw_counter_ ++;

	anim_->update_last_draw_time();
	frame_parameters params;
	params.area_mode = anim_->area_mode();
	const t_translation::t_terrain terrain = map.get_terrain(loc_);
	const terrain_type& terrain_info = map.get_terrain_info(terrain);
	// do not set to 0 so we can distinguish the flying from the "not on submerge terrain"
	// instead use -1.0 (as in "negative depth", it will be ignored by rendering)
	params.submerge= is_flying() ? -1.0 : terrain_info.unit_submerge();

	if (invisible(loc_) && params.highlight_ratio > 0.5) {
		params.highlight_ratio = 0.5;
	}
	if (loc_ == disp.selected_hex() && params.highlight_ratio == 1.0) {
		params.highlight_ratio = 1.5;
	}
	int height_adjust = static_cast<int>(terrain_info.unit_height_adjust() * disp.get_zoom_factor());
	if (is_flying() && height_adjust < 0) {
		height_adjust = 0;
	}
	params.y -= height_adjust;
	params.halo_y -= height_adjust;
	if (get_state(ustate_tag::POISONED)) {
		params.blend_with = disp.rgb(0, 255, 0);
		params.blend_ratio = 0.25;
	} else if (temporary_state_ & BIT_2_MASK(BIT_STRONGER)) {
		if (temporary_state_ & BIT_2_MASK(BIT_DEFENDING)) {
			params.blend_with = disp.rgb(32, 32, 255);
		} else {
			params.blend_with = disp.rgb(255, 32, 32);
		}
		params.blend_ratio = 0.40;
	} else if (provoked_turns_) {
		params.blend_with = disp.rgb(255, 0, 0);
		params.blend_ratio = 0.25;
	}
	//hackish : see unit_frame::merge_parameters
	// we use image_mod on the primary image
	// and halo_mod on secondary images and all haloes
	params.image_mod = image_mods();
	params.halo_mod = TC_image_mods();
	params.image= absolute_image();
	params.sound_filter = sound_filter_tag::rfind(sound_filter_tag::from_hero_gender(master_->gender_));


	if (get_state(ustate_tag::PETRIFIED)) {
		params.image_mod +="~GS()";
	}
	params.primary_frame = t_true;

	const frame_parameters adjusted_params = anim_->get_current_params(params);



	const map_location dst = loc_.get_direction(facing_);
	const int xsrc = disp.get_location_x(loc_);
	const int ysrc = disp.get_location_y(loc_);
	const int xdst = disp.get_location_x(dst);
	const int ydst = disp.get_location_y(dst);
	int d2 = disp.hex_size() / 2;




	const int x = static_cast<int>(adjusted_params.offset_x * xdst + (1.0-adjusted_params.offset_x) * xsrc) + d2;
	const int y = static_cast<int>(adjusted_params.offset_y * ydst + (1.0-adjusted_params.offset_y) * ysrc) + d2;


	if(unit_halo_ == halo::NO_HALO && !image_halo().empty()) {
		unit_halo_ = halo::add(0, 0, image_halo()+TC_image_mods(), map_location(-1, -1));
	}
	if(unit_halo_ != halo::NO_HALO && image_halo().empty()) {
		halo::remove(unit_halo_);
		unit_halo_ = halo::NO_HALO;
	} else if(unit_halo_ != halo::NO_HALO) {
		halo::set_location(unit_halo_, x, y - height_adjust);
	}



	// We draw bars only if wanted, visible on the map view
	bool draw_bars = draw_bars_ ;
	if (draw_bars) {
		const int d = disp.hex_size();
		SDL_Rect unit_rect = create_rect(xsrc, ysrc +adjusted_params.y, d, d);
		draw_bars = rects_overlap(unit_rect, disp.map_outside_area());
	}

	surface ellipse_front(NULL);
	surface ellipse_back(NULL);
	int ellipse_floating = 0;
	if(draw_bars) {
		if(adjusted_params.submerge > 0.0) {
			// The division by 2 seems to have no real meaning,
			// It just works fine with the current center of ellipse
			// and prevent a too large adjust if submerge = 1.0
			ellipse_floating = static_cast<int>(adjusted_params.submerge * disp.hex_size() / 2);
		}

		std::string ellipse = "misc/ellipse";

		const char* const selected = disp.selected_hex() == loc_ ? "selected-" : "";

		// Load the ellipse parts recolored to match team color
		char buf[100];
		std::string tc=team::get_side_color_index(side_);

		snprintf(buf,sizeof(buf),"%s-%stop.png~RC(ellipse_red>%s)",ellipse.c_str(),selected,tc.c_str());
		ellipse_back.assign(image::get_image(image::locator(buf), image::SCALED_TO_ZOOM));
		snprintf(buf,sizeof(buf),"%s-%sbottom.png~RC(ellipse_red>%s)",ellipse.c_str(),selected,tc.c_str());
		ellipse_front.assign(image::get_image(image::locator(buf), image::SCALED_TO_ZOOM));
	}

	std::string formation_img;
	if (formation_flag_ != FORMATION_NONE) {
		if (formation_flag_ % 100 == FORMATION_MASTER || formation_flag_ % 100 == FORMATION_MASTER_DISABLE) {
			if (formation_flag2_ & 0x1) {
				disp.drawing_buffer_add(display::LAYER_UNIT_BG, loc_,
					xsrc, ysrc, image::get_image("misc/attack-indicator-src-n.png", image::SCALED_TO_HEX));
			}
			if (formation_flag2_ & 0x2) {
				disp.drawing_buffer_add(display::LAYER_UNIT_BG, loc_,
					xsrc, ysrc, image::get_image("misc/attack-indicator-src-ne.png", image::SCALED_TO_HEX));
			}
			if (formation_flag2_ & 0x4) {
				disp.drawing_buffer_add(display::LAYER_UNIT_BG, loc_,
					xsrc, ysrc, image::get_image("misc/attack-indicator-src-se.png", image::SCALED_TO_HEX));
			}
			if (formation_flag2_ & 0x8) {
				disp.drawing_buffer_add(display::LAYER_UNIT_BG, loc_,
					xsrc, ysrc, image::get_image("misc/attack-indicator-src-s.png", image::SCALED_TO_HEX));
			}
			if (formation_flag2_ & 0x10) {
				disp.drawing_buffer_add(display::LAYER_UNIT_BG, loc_,
					xsrc, ysrc, image::get_image("misc/attack-indicator-src-sw.png", image::SCALED_TO_HEX));
			}
			if (formation_flag2_ & 0x20) {
				disp.drawing_buffer_add(display::LAYER_UNIT_BG, loc_,
					xsrc, ysrc, image::get_image("misc/attack-indicator-src-nw.png", image::SCALED_TO_HEX));
			}
		} else {
			formation_img = "misc/formation-hex.png";
			disp.drawing_buffer_add(display::LAYER_UNIT_BG, loc_,
				xsrc, ysrc, image::get_image(formation_img, image::SCALED_TO_HEX));
		}

		formation_img = "";
		if (redraw_counter_ % 30 < 15 && formation_flag_ > 100) {
			const tformation_profile& profile = unit_types.formation(formation_flag_ / 100 - 1);
			formation_img = profile.icon();
			if (formation_flag_ % 100 == FORMATION_MASTER_DISABLE) {
				formation_img = formation_img + "~GS()";
			}
		}
		if (!formation_img.empty()) {
			disp.drawing_buffer_add(display::LAYER_REACHMAP, loc_, xsrc + 24, ysrc, 
				image::get_image(formation_img, image::SCALED_TO_ZOOM));
		}
	}

	if (ellipse_back != NULL) {
		disp.drawing_buffer_add(display::LAYER_UNIT_BG, loc_,
			xsrc, ysrc +adjusted_params.y-ellipse_floating, ellipse_back);
	}

	if (ellipse_front != NULL) {
		//disp.drawing_buffer_add(display::LAYER_UNIT_FG, loc,
		disp.drawing_buffer_add(display::LAYER_UNIT_FIRST, loc_,
			xsrc, ysrc +adjusted_params.y-ellipse_floating, ellipse_front);
	}
	if ((draw_desc_ || (temporary_state_ & BIT_2_MASK(BIT_ATTACKING)) || (temporary_state_ & BIT_2_MASK(BIT_DEFENDING))) && draw_bars) {
		if (!commoner_) {
			const surface* food_img = NULL;
			if (is_robber()) {
				food_img = &unit_map::robber;
			} else if (!alert_food()) {
				// food_img = &unit_map::normal_food;
			} else if (food_) {
				if (redraw_counter_ % 20 < 10) {
					food_img = &unit_map::normal_food;
				} else {
					food_img = &unit_map::lack_food;
				}
			} else {
				food_img = &unit_map::lack_food;
			}

			if (food_img) {
				if (display::initial_zoom == display::ZOOM_48) {
					disp.drawing_buffer_add(display::LAYER_UNIT_BAR, loc_, xsrc + 50, ysrc - 9, *food_img);
				} else if (display::initial_zoom == display::ZOOM_56) {
					disp.drawing_buffer_add(display::LAYER_UNIT_BAR, loc_, xsrc + 60, ysrc - 11, *food_img);
				} else if (display::initial_zoom == display::ZOOM_64) {
					disp.drawing_buffer_add(display::LAYER_UNIT_BAR, loc_, xsrc + 68, ysrc - 13, *food_img);
				} else {
					disp.drawing_buffer_add(display::LAYER_UNIT_BAR, loc_, xsrc + 76, ysrc - 15, *food_img);
				}
			}
		}

		const surface* orb_img = NULL;

		if (size_t(side()) != disp.viewing_team() + 1) {
			if (disp.team_valid() && disp.get_teams()[disp.viewing_team()].is_enemy(side())) {
				orb_img = &unit_map::enemy_orb_;
			} else {
				orb_img = &unit_map::ally_orb_;
			}
		} else {
			orb_img = &unit_map::moved_orb_;
			if (disp.playing_team() == disp.viewing_team()) {
				if (!human()) {
					if (!commoner_) {
						orb_img = &unit_map::self_orb_;
					} else {
						orb_img = &unit_map::automatic_orb_;
					}
				} else if (current_can_action(*this)) {
					if (movement_left() == total_movement()) {
						if (max_movement_ || attacks_left_) { 
							orb_img = &unit_map::unmoved_orb_;
						}
					} else if (unit_can_action(*this)) {
						orb_img = &unit_map::partmoved_orb_;

					} else if (goto_.valid()) {
						orb_img = &unit_map::automatic_orb_;
					}
				}
			}
		}
		if (*orb_img) {
			if (display::initial_zoom == display::ZOOM_48) {
				disp.drawing_buffer_add(display::LAYER_UNIT_BAR, 
					loc_, xsrc + 61, ysrc - 9, *orb_img);
			} else if (display::initial_zoom == display::ZOOM_56) {
				disp.drawing_buffer_add(display::LAYER_UNIT_BAR, 
					loc_, xsrc + 72, ysrc - 11, *orb_img);
			} else if (display::initial_zoom == display::ZOOM_64) {
				disp.drawing_buffer_add(display::LAYER_UNIT_BAR, 
					loc_, xsrc + 82, ysrc - 13, *orb_img);
			} else {
				disp.drawing_buffer_add(display::LAYER_UNIT_BAR, 
					loc_, xsrc + 92, ysrc - 15, *orb_img);
			}
		}

		std::string* energy_file;
		if (display::initial_zoom == display::ZOOM_48) {
			energy_file = &game_config::images::bar_hrl_48;
		} else if (display::initial_zoom == display::ZOOM_56) {
			energy_file = &game_config::images::bar_hrl_56;
		} else if (display::initial_zoom == display::ZOOM_64) {
			energy_file = &game_config::images::bar_hrl_64;
		} else {
			energy_file = &game_config::images::bar_hrl_72;
		}

		double unit_energy = 0.0;
		if (max_hitpoints() > 0) {
			unit_energy = double(hitpoints()) / double(max_hitpoints());
		}
		// const int bar_shift = static_cast<int>(-5*disp.get_zoom_factor());
		const int hp_bar_height = static_cast<int>(max_hitpoints()*game_config::hp_bar_scaling);

		const fixed_t bar_alpha = (loc_ == disp.mouseover_hex() || loc_ == disp.selected_hex()) ? ftofxp(1.0): ftofxp(0.8);

		int xsrc_zoom, ysrc_zoom;
		if (display::initial_zoom == display::ZOOM_48) {
			xsrc_zoom = xsrc + 24;
			ysrc_zoom = ysrc - 10;
		} else if (display::initial_zoom == display::ZOOM_56) {
			xsrc_zoom = xsrc + 28;
			ysrc_zoom = ysrc - 12;
		} else if (display::initial_zoom == display::ZOOM_64) {
			xsrc_zoom = xsrc + 32;
			ysrc_zoom = ysrc - 14;
		} else {
			xsrc_zoom = xsrc + 36;
			ysrc_zoom = ysrc - 16;
		}

		if (display::initial_zoom == display::ZOOM_48) {
			disp.draw_bar(*energy_file, xsrc_zoom + 19, ysrc_zoom + 11,
				loc_, hp_bar_height, unit_energy, hp_color(), bar_alpha, false);
		} else if (display::initial_zoom == display::ZOOM_56) {
			disp.draw_bar(*energy_file, xsrc_zoom + 22, ysrc_zoom + 14,
				loc_, hp_bar_height, unit_energy, hp_color(), bar_alpha, false);
		} else if (display::initial_zoom == display::ZOOM_64) {
			disp.draw_bar(*energy_file, xsrc_zoom + 25, ysrc_zoom + 16,
				loc_, hp_bar_height, unit_energy, hp_color(), bar_alpha, false);
		} else {
			disp.draw_bar(*energy_file, xsrc_zoom + 28, ysrc_zoom + 20,
				loc_, hp_bar_height, unit_energy, hp_color(), bar_alpha, false);
		}

		if (can_advance()) {
			const double filled = double(experience())/double(max_experience());

			const int xp_bar_height = static_cast<int>(max_experience()*game_config::xp_bar_scaling / std::max<int>(level_,1));

			SDL_Color color = xp_color();
			if (display::initial_zoom == display::ZOOM_48) {
				disp.draw_bar(*energy_file, xsrc_zoom + 19, ysrc_zoom + 16, 
					loc_, xp_bar_height, filled, color, bar_alpha, false);
			} else if (display::initial_zoom == display::ZOOM_56) {
				disp.draw_bar(*energy_file, xsrc_zoom + 22, ysrc_zoom + 20, 
					loc_, xp_bar_height, filled, color, bar_alpha, false);
			} else if (display::initial_zoom == display::ZOOM_64) {
				disp.draw_bar(*energy_file, xsrc_zoom + 25, ysrc_zoom + 23, 
					loc_, xp_bar_height, filled, color, bar_alpha, false);
			} else {
				disp.draw_bar(*energy_file, xsrc_zoom + 28, ysrc_zoom + 26, 
					loc_, xp_bar_height, filled, color, bar_alpha, false);
			}
		}

		if (!desc_rect_) {
			SDL_Rect dst = {0, 0, 0, 0};
			// desc_rect_.assign(create_neutral_surface(unit_map::desc_bg_[level_]->w, unit_map::desc_bg_[level_]->h));
			// unit desc frame
			desc_rect_ = make_neutral_surface(unit_map::desc_bg_[level_]);

			// portrait
			if (small_portrait_ == NULL) {
				small_portrait_ = image::get_image(master_->image());
				SDL_Rect clip = {4, 5, 40, 50};
				if (display::initial_zoom == display::ZOOM_48) {
					small_portrait_ = scale_surface(cut_surface(small_portrait_, clip), 16, 19);
				} else if (display::initial_zoom == display::ZOOM_56) {
					small_portrait_ = scale_surface(cut_surface(small_portrait_, clip), 18, 23);
				} else if (display::initial_zoom == display::ZOOM_64) {
					small_portrait_ = scale_surface(cut_surface(small_portrait_, clip), 21, 26);
				} else {
					small_portrait_ = scale_surface(cut_surface(small_portrait_, clip), 24, 30);
				}
				SDL_SetAlpha(small_portrait_.get(), 0, SDL_ALPHA_OPAQUE);
			}
			if (display::initial_zoom == display::ZOOM_48) {
				dst.x = 2;
				dst.y = 2;
			} else if (display::initial_zoom == display::ZOOM_56) {
				dst.x = 3;
				dst.y = 2;
			} else if (display::initial_zoom == display::ZOOM_64) {
				dst.x = 3;
				dst.y = 2;
			} else {
				dst.x = 3;
				dst.y = 2;
			}
			sdl_blit(small_portrait_, NULL, desc_rect_, &dst);
		}

		int vtl_bar_x_offset = 0;
		int vtl_bar_y_offset = 0;
		int vtl_outline_height = 4;

		disp.drawing_buffer_add(display::LAYER_UNIT_LAST, loc_, xsrc_zoom, ysrc_zoom, desc_rect_);

		if (!tent::turn_based && current_can_action(*this)) {
			if (redraw_counter_ % 40 < 20) {
				disp.drawing_buffer_add(display::LAYER_UNIT_LAST, loc_, xsrc_zoom, ysrc_zoom, unit_map::desc_hot);
			}
		}

		if (tactic_existed()) {
			int percent = percent_tactic_degree();
			double unit_energy = 1.0 * percent / 100;
			const int hp_bar_height = desc_rect_->h - vtl_outline_height;
			SDL_Color color = can_cast_tactic()? font::GOOD_COLOR: font::BAD_COLOR;
			std::string tactic_file = unit_map::bar_vtl_png;
			if (percent == 100 && redraw_counter_ % 20 < 10) {
				tactic_file = unit_map::bar_vtl_hot_png;
			}
			disp.draw_bar(tactic_file, xsrc_zoom - vtl_bar_x_offset, ysrc_zoom - vtl_bar_y_offset,
				loc_, hp_bar_height, unit_energy, color, bar_alpha, true);
		}
		if (hot_turns_) {
			int y_offset = desc_rect_->h - 12;
			disp.drawing_buffer_add(display::LAYER_UNIT_BAR, loc_, xsrc_zoom + 2, ysrc_zoom + y_offset, image::get_image(single_digit_image(hot_turns_)));
		}

		formation_img.clear();
		if (get_state(ustate_tag::FORMATION_ATTACKED)) {
			if (can_formation_master()) {
				formation_img = "misc/formation-can-master-attacked.png";
			} else {
				formation_img = "misc/formation-attacked.png";
			}
		} else if (can_formation_master()) {
			formation_img = "misc/formation-can-master.png";
		}
		if (!formation_img.empty()) {
			disp.drawing_buffer_add(display::LAYER_UNIT_LAST, loc_, xsrc_zoom, ysrc_zoom, image::get_image(formation_img, image::SCALED_TO_ZOOM));
		}
		if (task_ == TASK_STATION) {
			// task iocn place left-top coner, overlap with tactic-bar
			disp.drawing_buffer_add(display::LAYER_UNIT_BAR, loc_, xsrc_zoom + 5, ysrc_zoom, image::get_image("misc/station.png", image::SCALED_TO_ZOOM));

		} else if (task_ == TASK_GUARD) {
			// task iocn place left-top coner, overlap with tactic-bar
			disp.drawing_buffer_add(display::LAYER_UNIT_BAR, loc_, xsrc_zoom + 5, ysrc_zoom, image::get_image("misc/guard.png", image::SCALED_TO_ZOOM));
		}
	}

	if (disp.expedite_city() && loc_ == disp.expedite_city()->get_location()) {
		params.drawing_layer = display::LAYER_UNIT_FG - display::LAYER_UNIT_FIRST;
	}

	anim_->redraw(params);
	refreshing_ = false;
}

void unit::clear_haloes()
{
	if(unit_halo_ != halo::NO_HALO) {
		halo::remove(unit_halo_);
		unit_halo_ = halo::NO_HALO;
	}
	if(anim_ ) anim_->clear_haloes();
}
bool unit::invalidate(const map_location &loc)
{
	bool result = false;


	game_display * disp = resources::screen;
	disp->invalidate(touch_locs_);

	// Very early calls, anim not initialized yet
	if (get_animation()) {
		frame_parameters params;
		const gamemap & map = disp->get_map();
		const t_translation::t_terrain terrain = map.get_terrain(loc);
		const terrain_type& terrain_info = map.get_terrain_info(terrain);

		int height_adjust = static_cast<int>(terrain_info.unit_height_adjust() * disp->get_zoom_factor());
		if (is_flying() && height_adjust < 0) {
			height_adjust = 0;
		}
		params.y -= height_adjust;
		params.halo_y -= height_adjust;
		params.image_mod = image_mods();

		result |= get_animation()->invalidate(disp->video().getSurface(), params);
	}

	return result;

}

int unit::upkeep() const
{
	if (tent::tower_mode() || commoner_ || unit_feature_val(hero_feature_magnate)) {
		return 0;
	}
	// Leaders do not incur upkeep.
	if (upkeep_ == -1) {
		return level() * 2;
	}
	return upkeep_;
}

bool unit::loyal() const
{
	return unit_feature_val(hero_feature_magnate) || !upkeep_;
}

int unit::movement_cost(const t_translation::t_terrain terrain, const map_location* loc) const
{
	if (has_state_flag(ustate_tag::LEGERITIED)) {
		return 1;
	}

	team& this_team = teams_[side_ - 1];
	if (!loc || !this_team.has_avoid() || !this_team.avoid().match(*loc)) {
		VALIDATE(resources::game_map != NULL, "unit::movement_cost, game_map is null!");
		gamemap& map = *resources::game_map;
		
		const int res = movement_cost_internal(movement_costs_, cfg_, NULL, map, terrain);

		if (res == unit_movement_type::UNREACHABLE) {
			return res;
		} else if (get_state(ustate_tag::SLOWED)) {
			return res*2;
		}
		return res;
	} else {
		return unit_movement_type::UNREACHABLE;
	}	
}

int unit::defense_modifier(t_translation::t_terrain terrain) const
{
	assert(resources::game_map != NULL);

	int def = defense_modifier_internal(defense_mods_, cfg_, NULL, *resources::game_map, terrain);
#if 0
	// A [defense] ability is too costly and doesn't take into account target locations.
	// Left as a comment in case someone ever wonders why it isn't a good idea.
	unit_ability_list defense_abilities = get_abilities("defense");
	if (!defense_abilities.empty()) {
		unit_abilities::effect defense_effect(defense_abilities, def, false);
		def = defense_effect.get_composite_value();
	}
#endif
	return def;
}

bool unit::resistance_filter_matches(const config& cfg, bool attacker, const std::string& damage_name, int res) const
{
	if(!(cfg["active_on"]=="" || (attacker && cfg["active_on"]=="offense") || (!attacker && cfg["active_on"]=="defense"))) {
		return false;
	}
	const std::string& apply_to = cfg["apply_to"];
	if(!apply_to.empty()) {
		if(damage_name != apply_to) {
			if(std::find(apply_to.begin(),apply_to.end(),',') != apply_to.end() &&
				std::search(apply_to.begin(),apply_to.end(),
				damage_name.begin(),damage_name.end()) != apply_to.end()) {
				const std::vector<std::string>& vals = utils::split(apply_to);
				if(std::find(vals.begin(),vals.end(),damage_name) == vals.end()) {
					return false;
				}
			} else {
				return false;
			}
		}
	}
	if (!unit_abilities::filter_base_matches(cfg, res)) return false;
	return true;
}


int unit::resistance_against(const std::string& damage_name,bool attacker,const map_location& loc) const
{
	int res = 0;

	if (const config &resistance = cfg_.child("resistance")) {
		res = 100 - resistance[damage_name].to_int(100);
	}

	unit_ability_list resistance_abilities = get_abilities("resistance",loc);
	for (std::vector<std::pair<const config *, unit *> >::iterator i = resistance_abilities.cfgs.begin(); i != resistance_abilities.cfgs.end();) {
		if(!resistance_filter_matches(*i->first, attacker, damage_name, res)) {
			i = resistance_abilities.cfgs.erase(i);
		} else {
			++i;
		}
	}

	resist_helper_.clear();
	if (!resistance_abilities.empty()) {
		unit_abilities::effect resist_effect(resistance_abilities, res, false);

		for (std::vector<unit_abilities::individual_effect>::const_iterator resist_loc = resist_effect.begin(); resist_loc != resist_effect.end(); ++ resist_loc) {
			if (resist_loc->u != this && resist_loc->u->can_formation_attack() && std::find(resist_helper_.begin(), resist_helper_.end(), resist_loc->u) == resist_helper_.end()) {
				resist_helper_.push_back(resist_loc->u);
			}
		}

		int new_res = std::min<int>(resist_effect.get_composite_value(),resistance_abilities.highest("max_value").first);
		if (new_res != res) {
			const team& t = teams_[side_ - 1 ];
			if (new_res > res) {
				res = res + (new_res - res) * t.cooperate_increase_ / 100;
			} else {
				res = res + (res - new_res) * 100 / t.cooperate_increase_;
			}
		} else {
			res = new_res;
		}
	}
	return 100 - res;
}

utils::string_map unit::get_base_resistances() const
{
	if (const config &resistance = cfg_.child("resistance"))
	{
		utils::string_map res;
		BOOST_FOREACH (const config::attribute &i, resistance.attribute_range()) {
			res[i.first] = i.second;
		}
		return res;
	}
	return utils::string_map();
}

#if 0
std::map<terrain_type::TERRAIN,int> unit::movement_type() const
{
	return movement_costs_;
}
#endif

std::map<std::string,std::string> unit::advancement_icons() const
{
	std::map<std::string,std::string> temp;
	if (!can_advance())
		return temp;

	if (!advances_to_.empty())
	{
		std::ostringstream tooltip;
		const std::string &image = game_config::images::level;
		BOOST_FOREACH (const std::string &s, advances_to())
		{
			if (!s.empty())
				tooltip << s << '\n';
		}
		temp[image] = tooltip.str();
	}

	BOOST_FOREACH (const config &adv, get_modification_advances())
	{
		const std::string &image = adv["image"];
		if (image.empty()) continue;
		std::ostringstream tooltip;
		tooltip << temp[image];
		const std::string &tt = adv["description"];
		if (!tt.empty())
			tooltip << tt << '\n';
		temp[image] = tooltip.str();
	}
	return(temp);
}
std::vector<std::pair<std::string,std::string> > unit::amla_icons() const
{
	std::vector<std::pair<std::string,std::string> > temp;
	std::pair<std::string,std::string> icon; //<image,tooltip>

	BOOST_FOREACH (const config &adv, get_modification_advances())
	{
		icon.first = adv["icon"].str();
		icon.second = adv["description"].str();

		for (unsigned j = 0, j_count = modification_count(adv["id"]);
		     j < j_count; ++j)
		{
			temp.push_back(icon);
		}
	}
	return(temp);
}

bool unit::can_advance() const 
{ 
	if (!advances_to_.empty()) {
		return true;
	}
	if (packed()) {
		if (!packee_unit_type_->advancement().empty()) {
			return true;
		}
	} else if (!unit_type_->advancement().empty()) {
		return true;
	}
	return false;
}

std::vector<config> unit::get_modification_advances() const
{
	std::string advancement;
	if (packed()) {
		advancement = packee_unit_type_->advancement();
	} else {
		advancement = unit_type_->advancement();
	}
	std::vector<config> res;
	if (advancement.empty()) {
		return res;
	}
	// now only support one [advancement]
	const modifications_map& modifications = unit_types.modifications();
	const config& adv = modifications.find(advancement)->second;
	while (adv) {
		if (adv["strict_amla"].to_bool() && !advances_to_.empty())
			break;
		if (modification_count(adv["id"]) >= unsigned(adv["max_times"].to_int(1)))
			break;

		std::vector<std::string> temp = utils::split(adv["require_amla"]);
		if (temp.empty()) {
			res.push_back(adv);
			break;
		}

		std::sort(temp.begin(), temp.end());
		std::vector<std::string> uniq;
		std::unique_copy(temp.begin(), temp.end(), std::back_inserter(uniq));

		bool requirements_done = true;
		BOOST_FOREACH (const std::string &s, uniq)
		{
			int required_num = std::count(temp.begin(), temp.end(), s);
			int mod_num = modification_count(s);
			if (required_num > mod_num) {
				requirements_done = false;
				break;
			}
		}
		if (requirements_done) {
			res.push_back(adv);
		}
		break;
	}

	return res;
}

size_t unit::modification_count(const std::string& id) const
{
	size_t res = 0;
	for (std::vector<std::string>::const_iterator it = modifications_.begin(); it != modifications_.end(); ++ it) {
		if (*it == id) {
			++ res;
		}
	}

	return res;
}

/** Helper function for add_modifications */
static void mod_mdr_merge(config& dst, const config& mod, bool delta, bool reverse, int min, int max)
{
	BOOST_FOREACH (const config::attribute &i, mod.attribute_range()) {
		int v = 0;
		if (delta) v = dst[i.first];
		if (reverse) {
			v = v - i.second.to_int();
		} else {
			v = v + i.second.to_int();
		}
		if (v < min) v = min;
		if (v > max) v = max;
		dst[i.first] = v;
	}
}

void unit::add_modification(const config& mod, bool no_add, bool anim)
{
	if (no_add == false) {
		modifications_.push_back(mod["id"]);
	}

	BOOST_FOREACH (const config &effect, mod.child_range("effect")) {
		// See if the effect only applies to certain unit types
		/** @todo The above two filters can be removed in 1.7 they're covered by the SUF. */
		// Apply SUF. (Filtering on location is probably a bad idea though.)
		if (const config &afilter = effect.child("filter")) {
		    if (!matches_filter(vconfig(afilter), map_location(cfg_, NULL))) continue;
		}

		const std::string& apply_to = effect["apply_to"];
		const std::string& apply_times = effect["times"];
		int times = 1;

		int tag = apply_to_tag::find(apply_to);
		if (tag == apply_to_tag::NONE) continue;

		if (apply_times == "per level") {
			times = level_;
		}
		if (times) {
			while (times > 0) {
				times --;

				add_modification_internal(tag, effect, anim);
			}
		}
	}
}

void unit::add_modification_internal(int apply_to, const config& effect, bool anim, int turns)
{
	std::stringstream strstr;
	bool increased;
	bool decreased;
	team& current_team = teams_[side_ - 1];

	// Apply unit type/variation changes last to avoid double applying effects on advance.
	if (apply_to == apply_to_tag::ATTACK) {
		bool first_attack = true;

		std::string attack_names;
		std::string desc;

		increased = decreased = false;
		for (std::vector<attack_type>::iterator a = attacks_.begin(); a != attacks_.end(); ++a) {
			int damage = a->damage();
			bool affected = a->apply_modification(effect, &desc);
			if (affected && desc != "") {
				if (first_attack) {
					first_attack = false;
				} 
			}
			if (affected) {
				if (damage < a->damage()) {
					increased = true;
				} else if (damage > a->damage()) {
					decreased = true;
				}
			}
		}
		if (anim && (increased || decreased)) {
			strstr.str("");
			strstr << tintegrate::generate_img("misc/attack.png");
			if (increased) {
				strstr << tintegrate::generate_img("misc/increase.png");
			}
			if (decreased) {
				strstr << tintegrate::generate_img("misc/decrease.png");
			}
			unit_display::unit_text(*this, decreased, strstr.str());
		}
	} else if(apply_to == apply_to_tag::HITPOINTS) {
		const std::string &increase_hp = effect["increase"];
		const std::string &increase_total = effect["increase_total"];
		const std::string &set_hp = effect["set"];
		const std::string &set_total = effect["set_total"];

		// If the hitpoints are allowed to end up greater than max hitpoints
		const bool violate_max = effect["violate_maximum"].to_bool();

		if(set_hp.empty() == false) {
			if(set_hp[set_hp.size()-1] == '%') {
				hit_points_ = lexical_cast_default<int>(set_hp)*max_hit_points_/100;
			} else {
				hit_points_ = lexical_cast_default<int>(set_hp);
			}
		}
		if(set_total.empty() == false) {
			if(set_total[set_total.size()-1] == '%') {
				max_hit_points_ = lexical_cast_default<int>(set_total)*max_hit_points_/100;
			} else {
				max_hit_points_ = lexical_cast_default<int>(set_total);
			}
		}

		if (increase_total.empty() == false) {
			// A percentage on the end means increase by that many percent
			max_hit_points_ = utils::apply_modifier(max_hit_points_, increase_total);
		}

		if(max_hit_points_ < 1)
			max_hit_points_ = 1;

		if (effect["heal_full"].to_bool()) {
			heal_all();
		}

		if (increase_hp.empty() == false) {
			hit_points_ = utils::apply_modifier(hit_points_, increase_hp, 0, max_hit_points_);
		}
/*
		if(hit_points_ > max_hit_points_ && !violate_max) {
			hit_points_ = max_hit_points_;
		}
*/
		if (hit_points_ < 1) {
			hit_points_ = 1;
		}
	} else if (apply_to == apply_to_tag::MOVEMENT) {
		if (current_team.restrict_movement || task_ == TASK_STATION) {
			return;
		}

		const std::string &increase = effect["increase"];
		const std::string &increase_curr = effect["increase_curr"];
		int base_max_movement = max_movement_;

		if (!increase.empty()) {
			max_movement_ = utils::apply_modifier(max_movement_, increase);
		}
		max_movement_ = effect["set"].to_int(max_movement_);

		if (!increase_curr.empty()) {
			movement_ = utils::apply_modifier(movement_, increase_curr);
		}

		// verify max_movement_
		int type_max_movement = packee_type()->max_movement();
		if (type_max_movement != -1 && max_movement_ > type_max_movement) {
			max_movement_ = type_max_movement;
		} else if (max_movement_ < 0) {
			max_movement_ = 0;
		}
		// verify movement_
		if (movement_ > max_movement_) {
			movement_ = max_movement_;
		} else if (movement_ < 0) {
			movement_ = 0;
		}

		increased = decreased = false;
		if (max_movement_ > base_max_movement) {
			increased = true;
		} else if (max_movement_ < base_max_movement) {
			decreased = true;
		}
		if (anim && (increased || decreased)) {
			strstr.str("");
			strstr << tintegrate::generate_img("misc/movement.png");
			if (increased) {
				strstr << tintegrate::generate_img("misc/increase.png");
			}
			if (decreased) {
				strstr << tintegrate::generate_img("misc/decrease.png");
			}
			unit_display::unit_text(*this, decreased, strstr.str());
		}
	} else if (apply_to == apply_to_tag::MUNITION) {
		int increase = effect["increase"].to_int();

		if (increase > 0 || (increase < 0 && food_)) {
			food_ += increase;
			if (food_ < 0) food_ = 0;
			if (anim) {
				std::stringstream str;
				str << _("Munition") << "\n";
				std::vector<unit*> touchers(1, this);
				unit_display::unit_touching(*this, touchers, increase, str.str());
			}
		}

	} else if(apply_to == apply_to_tag::MAX_EXPERIENCE) {
		const std::string &increase = effect["increase"];

		if (increase.empty() == false) {
			max_experience_ = utils::apply_modifier(max_experience_, increase, 1);
		}

	} else if (apply_to == apply_to_tag::LOYAL) {
		upkeep_ = 0;

	} else if(apply_to == apply_to_tag::STATUS) {
		const std::string &add = effect["add"];
		const std::string &remove = effect["remove"];

		if (add.empty() == false) {
			set_state(find_state(add), true);
		}

		if (remove.empty() == false) {
			set_state(find_state(remove), false);
		}
	} else if (apply_to == apply_to_tag::MOVEMENT_COSTS) {
		config &mv = cfg_.child_or_add("movement_costs");
		if (const config &ap = effect.child("movement_costs")) {
			mod_mdr_merge(mv, ap, !effect["replace"].to_bool(), false, 1, 1000);
		}
		movement_costs_.clear();
	} else if (apply_to == apply_to_tag::DEFENSE) {
		config &def = cfg_.child_or_add("defense");
		if (const config &ap = effect.child("defense")) {
			bool replace = effect["replace"].to_bool();
			BOOST_FOREACH (const config::attribute &i, ap.attribute_range()) {
				int v = i.second.to_int();
				config::attribute_value &dst = def[i.first];
				if (!replace) {
					int w = dst.to_int(100);
					v += w;
					if ((w >= 0 && v < 0) || (w < 0 && v > 0)) v = 0;
					else if (v < -100) v = -100;
					else if (v > 100) v = 100;
				}
				dst = v;
			}
		}
		defense_mods_.clear();
	} else if (apply_to == apply_to_tag::RESISTANCE) {
		config& mv = cfg_.child_or_add("resistance");
		increased = decreased = false;
		if (const config& ap = effect.child("resistance")) {
			if (anim) {
				BOOST_FOREACH (const config::attribute &i, ap.attribute_range()) {
					int v = i.second.to_int();
					if (v > 0) {
						increased = true;
					} else if (v < 0) {
						decreased = true;
					}
				}
			}
			mod_mdr_merge(mv, ap, !effect["replace"].to_bool(), true, 60, 500);
		}
		if (anim && (increased || decreased)) {
			strstr.str("");
			strstr << tintegrate::generate_img("misc/resistance.png");
			if (increased) {
				strstr << tintegrate::generate_img("misc/increase.png");
			}
			if (decreased) {
				strstr << tintegrate::generate_img("misc/decrease.png");
			}
			unit_display::unit_text(*this, decreased, strstr.str());
		}
	} else if (apply_to == apply_to_tag::ENCOURAGE) {
		int increase = effect["increase"].to_int();
		int value = increase + skill_[hero_skill_encourage];
		if (value < 0) {
			value = 0;
		} else if (value > fxptoi12(65535)) {
			value = fxptoi12(65535);
		}
		skill_[hero_skill_encourage] = value;

		if (anim && increase) {
			strstr.str("");
			strstr << tintegrate::generate_img("misc/encourage.png");
			if (increase > 0) {
				decreased = false;
				strstr << tintegrate::generate_img("misc/increase.png");
			} else {
				decreased = true;
				strstr << tintegrate::generate_img("misc/decrease.png");
			}
			unit_display::unit_text(*this, decreased, strstr.str());
		}

	} else if (apply_to == apply_to_tag::DEMOLISH) {
		int increase = effect["increase"].to_int();
		int value = increase + skill_[hero_skill_demolish];
		if (value < 0) {
			value = 0;
		} else if (value > fxptoi12(65535)) {
			value = fxptoi12(65535);
		}
		skill_[hero_skill_demolish] = value;

		if (anim && increase) {
			strstr.str("");
			strstr << tintegrate::generate_img("misc/demolish.png");
			if (increase > 0) {
				decreased = false;
				strstr << tintegrate::generate_img("misc/increase.png");
			} else {
				decreased = true;
				strstr << tintegrate::generate_img("misc/decrease.png");
			}
			unit_display::unit_text(*this, decreased, strstr.str());
		}

	} else if (apply_to == apply_to_tag::ZOC) {
		if (const config::attribute_value *v = effect.get("value")) {
			emit_zoc_ = v->to_bool();
		}
	} else if (apply_to == apply_to_tag::IMAGE_MOD) {
		std::string mod = effect["replace"];
		if (!mod.empty()){
			image_mods_ = mod;
		}
		mod = effect["add"].str();
		if (!mod.empty()){
			image_mods_ += mod;
		}

		game_config::add_color_info(effect);

	} else if (apply_to == apply_to_tag::ADVANCE) {
		int increase = effect["increase"].to_int();
		while (increase > 0) {
			experience_ += max_experience_;
			dialogs::advance_unit(*this, true);
			increase --;
		}

	} else if (apply_to == apply_to_tag::TRAIN) {
		// arms adaptability
		int increase = effect["increase"].to_int();
		if (increase && !hero::is_artifical(master_->number_) && !hero::is_commoner(master_->number_)) {
			int inc_adaptability_xp = increase * ftofxp12(1);

			u16_get_experience_i12(&(master_->arms_[arms_]), inc_adaptability_xp);
			if (second_->valid()) {
				u16_get_experience_i12(&(second_->arms_[arms_]), inc_adaptability_xp);
			}
			if (third_->valid()) {
				u16_get_experience_i12(&(third_->arms_[arms_]), inc_adaptability_xp);
			}
/*
			if (packee_unit_type_) {
				int packee_arms = packee_unit_type_->arms();
				u16_get_experience_i12(&(master_->arms_[packee_arms]), inc_adaptability_xp);
				if (second_->valid()) {
					u16_get_experience_i12(&(second_->arms_[packee_arms]), inc_adaptability_xp);
				}
				if (third_->valid()) {
					u16_get_experience_i12(&(third_->arms_[packee_arms]), inc_adaptability_xp);
				}
			}
*/
			adjust();
			if (anim) {
				std::stringstream str;
				str << _("Adaptability") << "\n";
				std::vector<unit*> touchers;
				if (artifical* city = units_.city_from_loc(loc_)) {
					touchers.push_back((unit*)(city));
				} else {
					touchers.push_back(this);
				}
				unit_display::unit_touching(*this, touchers, increase, str.str());
			}
		}
	} else if (apply_to == apply_to_tag::MOVE) {
		if (!resources::controller->is_replaying()) {
			events::mouse_handler::get_singleton()->set_unit_placing(*this);
		}

	} else if (apply_to == apply_to_tag::DAMAGE) {
		const std::string& type = effect["type"];
		const std::string& relative = effect["relative"];
		int ratio = effect["ratio"].to_int(100);
		damage_range(units_, teams_, *this, type, relative, ratio);

	} else if (apply_to == apply_to_tag::HIDE) {
		if (hide_turns_ < turns) hide_turns_ = turns;

	} else if (apply_to == apply_to_tag::ALERT) {
		if (alert_turns_ < turns) {
			set_alert_turns(turns);
		}

	} else if (apply_to == apply_to_tag::CLEAR) {
		int increase = effect["increase_activity"].to_int();
		increased = false;
		if (get_state(ustate_tag::SLOWED)) {
			set_state(ustate_tag::SLOWED, false);
			increased = true;
		}
		if (get_state(ustate_tag::BROKEN)) {
			set_state(ustate_tag::BROKEN, false);
			increased = true;
		}
		if (get_state(ustate_tag::POISONED)) {
			set_state(ustate_tag::POISONED, false);
			increased = true;
		}
		if (provoked_turns_) {
			remove_from_provoke_cache();
			increased = true;
		}
		if (increase_activity(increase)) {
			strstr.str("");
			strstr << increase;
			unit_display::unit_text(*this, false, strstr.str());
		} else if (increased) {
			unit_display::unit_text(*this, false, "");
		}

	} else if (apply_to == apply_to_tag::HEAL) {
		increased = false;
		int increase = effect["increase"].to_int();
		if (increase && max_hit_points_ > hit_points_) {
			heal(increase);
			increased = true;
		}
		if (increased) {
			strstr.str("");
			strstr << increase;
			unit_display::unit_text(*this, false, strstr.str());
		}

	} else if (apply_to == apply_to_tag::ACTION) {
		if (consider_ticks()) {
			int increase = effect["increase"].to_int();
			if (increase) {
				ticks_increase_ += increase;
			}
			if (anim && increase) {
				strstr.str("");
				strstr << tintegrate::generate_img("misc/action.png");
				if (increase > 0) {
					decreased = false;
					strstr << tintegrate::generate_img("misc/increase.png");
				} else {
					decreased = true;
					strstr << tintegrate::generate_img("misc/decrease.png");
				}
				unit_display::unit_text(*this, decreased, strstr.str());
			}
		}

	} else if (apply_to == apply_to_tag::PROVOKE) {
		VALIDATE(false, "apply_to_tag::PROVOKE don't process by add_modification_internal!");

	} else if (apply_to == apply_to_tag::REVIVAL) {
		VALIDATE(false, "apply_to_tag::REVIVAL don't process by add_modification_internal!");

	}
}

void unit::apply_tactic(const ttactic* contain, const ttactic& effect, int part, int turn)
{
	bool redo = contain? false: true;
	add_modification_internal(effect.apply_to(), effect.effect_cfg(), !redo, turn);
	if (!redo && !contain->parts()[part]->oneoff()) {
		tactics_.push_back(tunit_tactic(contain, turn, part));
	}
}

bool unit::select_tactic_can_effect(const unit& tactician, const ttactic& t)
{
	int apply_to = t.apply_to();
	if (apply_to == apply_to_tag::PROVOKE) {
		if (provoked_turns()) {
			return false;
		}
	} else if (apply_to == apply_to_tag::REVIVAL) {
		if (get_state(ustate_tag::REVIVALED)) {
			return false;
		}
	}
	return true;
}

void unit::add_trait_description(const config& trait)
{
	const t_string& name = trait["male_name"];

	if(!name.empty()) {
		trait_names_.push_back(name);
	}
}

const unit_animation* unit::choose_animation(const game_display& disp, const map_location& loc,const std::string& event,
		const map_location& second_loc,const int value,const unit_animation::hit_type hit,
		const attack_type* attack, const std::string& second_attack, int swing_num) const
{
	// Select one of the matching animations at random
	std::vector<const unit_animation*> options;
	int max_val = unit_animation::MATCH_FAIL;

	const std::vector<unit_animation>& animations = unit_type_->animations();
	for (std::vector<unit_animation>::const_iterator i = animations.begin(); i != animations.end(); ++i) {
		int matching = i->matches(disp, loc, second_loc, this, event, value, hit, attack, second_attack, swing_num);
		if (matching > unit_animation::MATCH_FAIL && matching == max_val) {
			options.push_back(&*i);
		} else if(matching > max_val) {
			max_val = matching;
			options.erase(options.begin(),options.end());
			options.push_back(&*i);
		}
	}

	if (max_val == unit_animation::MATCH_FAIL) {
		return NULL;
	}
	return options[rand()%options.size()];
}


void unit::apply_modifications()
{
	for (std::vector<std::string>::const_iterator it = traits_.begin(); it != traits_.end(); ++ it) {
		const config& mod = unit_types.traits().find(*it)->second;

		add_modification(mod, true);
		add_trait_description(mod);
	}

	//apply the experience acceleration last
	int exp_accel = unit_type::experience_accelerator::get_acceleration();
	max_experience_ = std::max<int>(1, (max_experience_ * exp_accel + 50)/100);
}

bool unit::invisible(const map_location& loc, bool see_all) const
{
	// Fetch from cache
	/**
	 * @todo FIXME: We use the cache only when using the default see_all=true
	 * Maybe add a second cache if the see_all=false become more frequent.
	 */
	if (see_all) {
		std::map<map_location, bool>::const_iterator itor = invisibility_cache_.find(loc);
		if (itor != invisibility_cache_.end()) {
			return itor->second;
		}
	}

	// Test hidden status
	bool is_inv = !get_state(ustate_tag::UNCOVERED) && !artifical_;
	if (is_inv) {
		unit* curr_node = units_.find_unit(loc, false);
		is_inv = !curr_node && get_ability_bool("hides", loc);
	}
	if (is_inv) {
		// range=1
		const map_location* tiles = adjacent_;
		size_t adjance_size = adjacent_size_;

		for (int step = 0; step < 3 && is_inv; step ++) {
			if (step == 1) {
				tiles = adjacent_2_;
				adjance_size = adjacent_size_2_;
			} else if (step == 2) {
				tiles = adjacent_3_;
				adjance_size = adjacent_size_3_;
			}

			for (size_t i = 0; i < adjance_size; i ++) {
				const map_location& u_loc = tiles[i];
				
				// when expedite, center of city using unit_map::find is expediting troop, other is city.
				// because city is 7 grids, so range = 1 don't touch center grid.
				unit* u = units_.find_unit(u_loc);
				if (!u) {
					continue;
				}
				if (u->wall()) {
					continue;
				}

				if (step && !unit_feature_val2(*u, hero_feature_scout)) {
					continue;
				}
				
				if (teams_[side_-1].is_enemy(u->side())) {
					// Enemy spotted in adjacent tiles, check if we can see him.
					// Watch out to call invisible with see_all=true to avoid infinite recursive calls!
					if (see_all) {
						is_inv = false;
						break;
					} else if (!teams_[side_-1].fogged(u_loc) && !u->invisible(u_loc, true)) {
						is_inv = false;
						break;
					}
				}
			}
		}
	}

	if (see_all) {
		// Add to caches
		if (invisibility_cache_.empty()) {
			units_with_cache.push_back(this);
		}
		invisibility_cache_[loc] = is_inv;

	}

	return is_inv;
}

unit& unit::clone(bool is_temporary)
{
	return *this;
}

void unit::increase_loyalty(int inc)
{
	hero& leader = *(teams_[side_ - 1].leader());

	master_->increase_loyalty(inc, leader);
	if (second_->valid()) {
		second_->increase_loyalty(inc, leader);
	}
	if (third_->valid()) {
		third_->increase_loyalty(inc, leader);
	}
}

// @level: desired to loyalty.
// @fixed: true: set all heros's loyalty to level. false: set all heros's loyalty to level if his loyalty is less than level.
void unit::set_loyalty(int level, bool fixed)
{
	hero& leader = *(teams_[side_ - 1].leader());

	master_->set_loyalty2(leader, level, fixed);
	if (second_->valid()) {
		second_->set_loyalty2(leader, level, fixed);
	}
	if (third_->valid()) {
		third_->set_loyalty2(leader, level, fixed);
	}
}

bool unit::has_less_loyalty(int loyalty, const hero& leader)
{
	int l = master_->loyalty(leader);
	if (l < loyalty) {
		return true;
	}
	if (second_->valid()) {
		l = second_->loyalty(leader);
		if (l < loyalty) {
			return true;
		}
	}
	if (third_->valid()) {
		l = third_->loyalty(leader);
		if (l < loyalty) {
			return true;
		}
	}
	return false;
}

bool unit::consider_loyalty() const
{
	return master_->number_ >= hero::number_normal_min && tent::mode == mode_tag::RPG && !dont_wander;
}

void unit::increase_feeling(int inc)
{
	if (master_->number_ < hero::number_normal_min) {
		return;
	}

	int carry_to, descent_number;
	bool changed = false;
	std::set<int> descents;
	if (second_->valid()) {
		carry_to = master_->increase_feeling(*second_, inc, descent_number);
		if (carry_to != hero::FEELING_NONE) {
			game_events::show_relation_message(units_, heros_, *master_, *second_, carry_to);
			changed = true;
		}
		if (descent_number != HEROS_INVALID_NUMBER) {
			descents.insert(descent_number);
		}
		carry_to = second_->increase_feeling(*master_, inc, descent_number);
		if (carry_to != hero::FEELING_NONE) {
			game_events::show_relation_message(units_, heros_, *second_, *master_, carry_to);
			changed = true;
		}
		if (descent_number != HEROS_INVALID_NUMBER) {
			descents.insert(descent_number);
		}
	} else {
		return;
	}
	if (third_->valid()) {
		carry_to = master_->increase_feeling(*third_, inc, descent_number);
		if (carry_to != hero::FEELING_NONE) {
			game_events::show_relation_message(units_, heros_, *master_, *third_, carry_to);
			changed = true;
		}
		if (descent_number != HEROS_INVALID_NUMBER) {
			descents.insert(descent_number);
		}
		carry_to = third_->increase_feeling(*master_, inc, descent_number);
		if (carry_to != hero::FEELING_NONE) {
			game_events::show_relation_message(units_, heros_, *third_, *master_, carry_to);
			changed = true;
		}
		if (descent_number != HEROS_INVALID_NUMBER) {
			descents.insert(descent_number);
		}

		carry_to = second_->increase_feeling(*third_, inc, descent_number);
		if (carry_to != hero::FEELING_NONE) {
			game_events::show_relation_message(units_, heros_, *second_, *third_, carry_to);
			changed = true;
		}
		if (descent_number != HEROS_INVALID_NUMBER) {
			descents.insert(descent_number);
		}
		carry_to = third_->increase_feeling(*second_, inc, descent_number);
		if (carry_to != hero::FEELING_NONE) {
			game_events::show_relation_message(units_, heros_, *third_, *second_, carry_to);
			changed = true;
		}
		if (descent_number != HEROS_INVALID_NUMBER) {
			descents.insert(descent_number);
		}
	}

	std::vector<unit*> adjusted;
	if (changed) {
		adjust();
		adjusted.push_back(this);
	}
	
	for (std::set<int>::const_iterator i = descents.begin(); i != descents.end(); ++ i) {
		unit* u = units_.find_unit(heros_[*i]);
		// The descented hero may be unstage.
		if (u && !u->is_artifical() && std::find(adjusted.begin(), adjusted.end(), u) == adjusted.end()) {
			u->adjust();
			adjusted.push_back(u);
		}
	}
}

void unit::goto_hate_if(const team& new_team, const hero& original_leader, int random)
{
	master_->goto_hate_if(new_team, original_leader, random);
	if (second_->valid()) {
		second_->goto_hate_if(new_team, original_leader, random);
	}
	if (third_->valid()) {
		third_->goto_hate_if(new_team, original_leader, random);
	}
}

std::pair<hero*, hero*> unit::exist_hate(const unit& target) const
{
	hero* h1;
	hero* h2;

	h1 = master_;
	std::vector<hero*> vh2;
	vh2.push_back(&target.master());
	if (target.second().valid()) {
		vh2.push_back(&target.second());
		if (target.third().valid()) {
			vh2.push_back(&target.third());
		}
	}
	for (int i = 0; i < 3; i ++) {
		if (i == 1) {
			if (second_->valid()) {
				h1 = second_;
			} else {
				break;
			}
		} else if (i == 2) {
			if (third_->valid()) {
				h1 = third_;
			} else {
				break;
			}
		}
		for (std::vector<hero*>::const_iterator it = vh2.begin(); it != vh2.end(); ++ it) {
			h2 = *it;
			if (h1->is_hate(*h2)) {
				return std::make_pair(h1, h2);
			}
		}
	}
	return std::make_pair(reinterpret_cast<hero*>(NULL), reinterpret_cast<hero*>(NULL));
}

int unit::calculate_clear_formated_cost() const
{
	if (master_->number_ < hero::number_normal_min) {
		return 0;
	}
	const team& current_team = teams_[side_ - 1];

	int desired = get_state(ustate_tag::FORMATION_ATTACKED)? 1: 0;
	for (size_t adj = 0; adj < adjacent_size_; adj ++) {
		const unit* u = units_.find_unit(adjacent_[adj]);
		if (u && u->get_state(ustate_tag::FORMATION_ATTACKED) && !current_team.is_enemy(u->side())) {
			desired ++;
		}
	}
	return desired * game_config::cost_clear_formationed;
}

bool unit::increase_activity(int inc, bool adjusting)
{
	bool changed = false;

	if (inc < 0 && master_->activity_ > game_config::minimal_activity) {
		if (inc + master_->activity_ > 0) {
			master_->activity_ += inc;
		} else {
			master_->activity_ = game_config::minimal_activity / 2;
		}
		changed = true;
	} else if (inc > 0 && master_->activity_ != HEROS_FULL_ACTIVITY) {
		if (inc + master_->activity_ < HEROS_FULL_ACTIVITY) {
			master_->activity_ += inc;
		} else {
			master_->activity_ = HEROS_FULL_ACTIVITY;
		}
		changed = true;
	}
	if (second_->valid()) {
		if (inc < 0 && second_->activity_ > game_config::minimal_activity) {
			if (inc + second_->activity_ > 0) {
				second_->activity_ += inc;
			} else {
				second_->activity_ = game_config::minimal_activity / 2;
			}
			changed = true;
		} else if (inc > 0 && second_->activity_ != HEROS_FULL_ACTIVITY) {
			if (inc + second_->activity_ < HEROS_FULL_ACTIVITY) {
				second_->activity_ += inc;
			} else {
				second_->activity_ = HEROS_FULL_ACTIVITY;
			}
			changed = true;
		}
	}
	if (third_->valid()) {
		if (inc < 0 && third_->activity_ > game_config::minimal_activity) {
			if (inc + third_->activity_ > 0) {
				third_->activity_ += inc;
			} else {
				third_->activity_ = game_config::minimal_activity / 2;
			}
			changed = true;
		} else if (inc > 0 && third_->activity_ != HEROS_FULL_ACTIVITY) {
			if (inc + third_->activity_ < HEROS_FULL_ACTIVITY) {
				third_->activity_ += inc;
			} else {
				third_->activity_ = HEROS_FULL_ACTIVITY;
			}
			changed = true;
		}
	}

	if (changed && adjusting) {
		adjust();
	}
	return changed;
}

unit_movement_resetter::unit_movement_resetter(unit &u, bool operate) :
	u_(u), moves_(u.movement_)
{
	if (operate) {
		u.movement_ = u.total_movement();
	}
}

unit_movement_resetter::~unit_movement_resetter()
{
	u_.movement_ = moves_;
}

int side_units(int side)
{
	int res = 0;
	for (unit_map::const_iterator it = resources::units->begin(); it != resources::units->end(); ++ it) {
		const unit* u = dynamic_cast<const unit*>(&*it);
		if (u->side() == side) ++res;
	}
	return res;
}

int side_units_cost(int side)
{
	int res = 0;
	for (unit_map::const_iterator it = resources::units->begin(); it != resources::units->end(); ++ it) {
		const unit* u = dynamic_cast<const unit*>(&*it);
		if (u->side() == side) res += u->cost();
	}
	return res;
}

unit* find_visible_unit(const map_location &loc, const team& current_team, bool see_all)
{
	unit_map& units = *resources::units;
	if (!resources::game_map->on_board(loc)) return NULL;
	unit* u = units.find_unit(loc);
	if (see_all) return u;

	if (!u) {
		return u;
	}
	if (current_team.fogged(loc) || (current_team.is_enemy(u->side()) && u->invisible(loc))) {
		return NULL;
	}
	return u;
}

unit* get_visible_unit(const map_location &loc,	const team &current_team, bool see_all)
{
	return find_visible_unit(loc, current_team, see_all);
}

void unit::refresh()
{
	if (state_ == STATE_FORGET && anim_ && anim_->animation_finished_potential()) {
		set_standing();
		return;
	}
	game_display &disp = *resources::screen;
	if (state_ != STATE_STANDING || get_current_animation_tick() < next_idling_ ||
	    !disp.tile_nearly_on_screen(loc_) || incapacitated())
	{
		return;
	}

	if (get_current_animation_tick() > next_idling_ + 1000) {
		// prevent all units animating at the same time
		next_idling_ = get_current_animation_tick()	+ (artifical_? 0: static_cast<int>(2000 + rand() % 3000));
	} else {
		set_idling();
	}
}

team_data calculate_team_data(const team& tm, int)
{
	team_data res;
	res.units = side_units(tm.side());
	res.upkeep = tm.side_upkeep();
	res.villages = 0;
	res.net_income = tm.total_income() - res.upkeep;
	res.gold = tm.gold();
	res.technology_net_income = tm.total_technology_income();
	return res;
}

std::pair<map_location, base_unit*> temporary_unit_placer::aggressing_;

temporary_unit_placer::temporary_unit_placer(unit_map& m, const map_location& loc, unit* u)
	: m_(m)
	, loc_(loc)
	, temp_(std::make_pair(u->get_location(), m.extract(loc)))
{
	aggressing_.first = loc;
	aggressing_.second = u;

	m.place(aggressing_.first, aggressing_.second);
}

temporary_unit_placer::~temporary_unit_placer()
{
	m_.extract(loc_);
	if (temp_.second) {
		m_.place(temp_.first, temp_.second);
	}
	// In construct, m.place(&aggressing_) will update loc_ of u,
	// so after temporary_unit_placer, loc_ of u is updated.
	// It doesn't reset loc_ of u, becuase temporary_unit_placer used in move_unit only,
	// there don't consider loc_ of u. but if use temporary_unit_placer to other, need consider it.
}

std::string unit::TC_image_mods() const{
	std::stringstream modifier;
	if(flag_rgb_.size()){
		modifier << "~RC("<< flag_rgb_ << ">" << team::get_side_color_index(side()) << ")";
	}
	return modifier.str();
}
std::string unit::image_mods() const{
	std::stringstream modifier;
	modifier << TC_image_mods();
	if(image_mods_.size()){
		modifier << "~" << image_mods_;
	}
	return modifier.str();
}

void unit::remove_attacks_ai()
{
	set_attacks(0);
}


void unit::remove_movement_ai()
{
	set_movement(0);
}


void unit::set_hidden(bool state) {
	hidden_ = state;
	if(!state) return;
	// We need to get rid of haloes immediately to avoid display glitches
	clear_haloes();
}

std::set<map_location> unit::get_touch_locations2(const gamemap& map, const map_location& loc) const
{
	std::set<map_location> touch_locs;

	touch_locs.insert(loc);
	const std::set<map_location::DIRECTION>& touch_dirs = get_touch_dirs();
	for (std::set<map_location::DIRECTION>::const_iterator it = touch_dirs.begin(); it != touch_dirs.end(); ++ it) {
		map_location offset = loc.get_direction(*it);
		if (!map.on_board(offset)) {
			std::stringstream str;
			str << "A location of " << master_->name() << "(" << loc << ") is out of map!";
			throw game::load_game_failed(str.str());
		}
		touch_locs.insert(offset);
	}
	return touch_locs;
}

const std::set<map_location::DIRECTION>& unit::get_touch_dirs() const 
{ 
	return packee_type()->touch_dirs(); 
}

void unit::set_location(const map_location &loc) 
{ 
	const gamemap& map = *resources::game_map;

	if (loc_ == loc) {
		if (pack_base_loc.valid()) {
			do_pack(map);
		}
		return;
	}
	// This is centor location
	loc_ = loc;

	if (!ignore_pack) {
		if (consider_ticks()) {
			// calculate ticks
			if (!this_is_city()) {
				if (!units_.city_from_loc(loc_)) {
					// field
					if (ticks_ == NONE_TICKS) {
						ticks_ = max_ticks_;
					} else if (ticks_ == RESIDE_TICKS) {
						ticks_ = 0;
						if (!tent::turn_based) {
							set_state(ustate_tag::EXPEDITED, true);
						}
					}
				} else {
					ticks_ = RESIDE_TICKS;
					drawn_ticks_ = NONE_TICKS;
				}
			} else if (ticks_ == NONE_TICKS) {
				if (ticks_ == NONE_TICKS) {
					ticks_ = max_ticks_;
				}
			}
		} else {
			ticks_ = max_ticks_;
		}
	}

	touch_locs_.clear();
	draw_locs_.clear();
	adjacent_size_ = 0;
	adjacent_size_2_ = 0;
	adjacent_size_3_ = 0;

	// Centor location must be in touch locations for all unit.
	touch_locs_.insert(loc_);
	
	if (!this_is_city() || get_touch_dirs().empty()) {
		
		map_offset* adjacent_ptr;
		size_t i, size;

		// range=1
		size = (sizeof(adjacent_1) / sizeof(map_offset)) >> 1;
		adjacent_ptr = adjacent_1[loc_.x & 0x1];
		for (i = 0; i < size; i ++) {
			adjacent_[adjacent_size_].x = loc_.x + adjacent_ptr[i].x;
			adjacent_[adjacent_size_].y = loc_.y + adjacent_ptr[i].y;
			if (map.on_board(adjacent_[adjacent_size_])) {
				adjacent_size_ ++;
			}
		}
		// range=2
		size = (sizeof(adjacent_2) / sizeof(map_offset)) >> 1;
		adjacent_ptr = adjacent_2[loc_.x & 0x1];
		for (i = 0; i < size; i ++) {
			adjacent_2_[adjacent_size_2_].x = loc_.x + adjacent_ptr[i].x;
			adjacent_2_[adjacent_size_2_].y = loc_.y + adjacent_ptr[i].y;
			if (map.on_board(adjacent_2_[adjacent_size_2_])) {
				adjacent_size_2_ ++;
			}
		}
		// range=3
		size = (sizeof(adjacent_3) / sizeof(map_offset)) >> 1;
		adjacent_ptr = adjacent_3[loc_.x & 0x1];
		for (i = 0; i < size; i ++) {
			adjacent_3_[adjacent_size_3_].x = loc_.x + adjacent_ptr[i].x;
			adjacent_3_[adjacent_size_3_].y = loc_.y + adjacent_ptr[i].y;
			if (map.on_board(adjacent_3_[adjacent_size_3_])) {
				adjacent_size_3_ ++;
			}
		}

		draw_locs_.insert(loc_);
		draw_locs_.insert(adjacent_[0]);
		draw_locs_.insert(adjacent_[1]);

		do_pack(map);
	}
}

void unit::do_pack(const gamemap& map)
{
	if (ignore_pack) {
		return;
	}
	// pack/unpack current unit if necessary
	t_translation::t_terrain terrain = map.get_terrain(pack_base_loc.valid()? pack_base_loc: loc_);
	bool match = t_translation::terrain_matches(terrain, t_translation::DEEP_WATER_MATCH);
	if (match) {
		if (!packed()) {
			// pack!
			// std::string packer;
			int navigation = teams_[side_ - 1].navigation();
			pack_to(unit_types.find(unit_types.id_from_navigation(navigation)));

			// since appearance of unit has changed, current animation should became invalid.
			if (anim_) {
				delete anim_;
				anim_ = NULL;
			}
		}
	} else if (packed()) {
		// unpack!
		pack_to(NULL);

		// since appearance of unit has changed, current animation should became invalid.
		if (anim_) {
			delete anim_;
			anim_ = NULL;
		}
	}
}

void unit::set_goto(const map_location& new_goto) 
{
	goto_ = new_goto;
}

void unit::set_from(const map_location& new_from)
{
	from_ = new_from;
}

void unit::replace_captains(const std::vector<hero*>& captains)
{
	if (master_ != captains[0]) {
		name_ = captains[0]->name();
		small_portrait_.assign(NULL);
		desc_rect_.assign(NULL);
	}

	if (tactic_hero_) {
		if (std::find(captains.begin(), captains.end(), tactic_hero_) == captains.end()) {
			remove_from_slot_cache();
		}
	}

	master_ = captains[0];
	master_->city_ = cityno_;
	master_->status_ = hero_status_military;
	master_number_ = master_->number_;
	
	if (captains.size() >= 2) {
		second_ = captains[1];
		second_->city_ = cityno_;
		second_->status_ = hero_status_military;
		second_number_ = second_->number_;
	} else {
		second_ = &hero_invalid;
	}
	if (captains.size() >= 3) {
		third_ = captains[2];
		third_->city_ = cityno_;
		third_->status_ = hero_status_military;
		third_number_ = third_->number_;
	} else {
		third_ = &hero_invalid;
	}

	adjust();
}

void unit::replace_captains_internal(hero& selected_hero, std::vector<hero*>& captains) const
{
	captains.clear();

	if (master_->number_ != selected_hero.number_) {
		captains.push_back(master_);
	}
	if (second_->valid()) {
		if (second_->number_ != selected_hero.number_) {
			captains.push_back(second_);
		}
	}
	if (third_->valid()) {
		if (third_->number_ != selected_hero.number_) {
			captains.push_back(third_);
		}
	}

	if (!captains.empty() && !captain_can_form(captains, packee_type())) {
		artifical* city = units_.city_from_cityno(cityno_);
		for (std::vector<hero*>::const_iterator it = captains.begin(); it != captains.end(); ++ it) {
			if (city) {
				city->finish_into(**it, hero_status_backing);
			}			
		}
		captains.clear();
	}
}

void unit::extract_heros_number()
{
	master_number_ = master_->number_;
	if (second_->valid()) {
		second_number_ = second_->number_;
	}
	if (third_->valid()) {
		third_number_ = third_->number_;
	}
}

void unit::recalculate_heros_pointer()
{
	master_ = &heros_[master_number_];
	if (second_->valid()) {
		second_ = &heros_[second_number_];
	}
	if (third_->valid()) {
		third_ = &heros_[third_number_];
	}
}

bool unit::hdata_variable() const
{
	return master_->number_ >= hero::number_city_min;
}

void unit::set_cityno(int new_cityno)
{ 
	cityno_ = new_cityno;

	if (!artifical_) {
		master_->city_ = cityno_;
		if (second_->valid()) {
			second_->city_ = cityno_;
		}
		if (third_->valid()) {
			third_->city_ = cityno_;
		}
	}
}

void unit::set_side(unsigned int new_side)
{ 
	side_ = new_side;

	if (!artifical_) {
		master_->side_ = side_ - 1;
		if (second_->valid()) {
			second_->side_ = side_ - 1;
		}
		if (third_->valid()) {
			third_->side_ = side_ - 1;
		}
	}
}

void unit::to_unstage()
{
	master_->to_unstage();
	if (second_->valid()) {
		second_->to_unstage();
	}
	if (third_->valid()) {
		third_->to_unstage();
	}
}

// true: all hero are unstage
// false: at least one hero is exist
bool unit::unstage_if_member(const hero& leader)
{
	std::vector<hero*> captains, unstages;
	if (!runtime_groups::exist_member(*master_, leader)) {
		captains.push_back(master_);
	} else {
		unstages.push_back(master_);
	}
	if (second_->valid()) {
		if (!runtime_groups::exist_member(*second_, leader)) {
			captains.push_back(second_);
		} else {
			unstages.push_back(second_);
		}
	}
	if (third_->valid()) {
		if (!runtime_groups::exist_member(*third_, leader)) {
			captains.push_back(third_);
		} else {
			unstages.push_back(third_);
		}
	}
	for (std::vector<hero*>::const_iterator it = unstages.begin(); it != unstages.end(); ++ it) {
		hero& h = **it;
		h.to_unstage(hero::UNSTAGE_GROUP);
	}
	if (!captains.empty() && !unstages.empty()) {
		replace_captains(captains);
	}
	return captains.empty();
}

move_unit_spectator& unit::get_move_spectator()
{
	return move_spectator_;
}

const move_unit_spectator& unit::get_move_spectator() const
{
	return move_spectator_;
}

int unit::guard_attack()
{
	if (guard_attack_ == -2) {
		guard_attack_ = unit_type_->guard();
		if (guard_attack_ >= (int)attacks_.size()) {
			guard_attack_ = -1;
		}
	}
	return guard_attack_;
}

// return value
//  true: h is non-unstage 
//  false: h is unstage
bool extract_hero(unit_map& units, const hero& h)
{
	artifical* city = units.city_from_cityno(h.city_);
	if (!city) {
		// unstage hero
		return false;
	}
	if (city->master().number_ == h.number_) {
		// cann't extract city
		assert(false);
		return false;
	}

	if (h.status_ == hero_status_military) {
		// in troop
		std::vector<unit*>& reside_troops = city->reside_troops();
		for (std::vector<unit*>::iterator itor = reside_troops.begin(); itor != reside_troops.end();) {
			unit& u = **itor;
			std::vector<hero*> captains;
			bool found = false;
			if (u.master().number_ == h.number_) {
				found = true;
			} else {
				captains.push_back(&u.master());
			}
			if (u.second().valid()) {
				if (u.second().number_ == h.number_) {
					found = true;
				} else {
					captains.push_back(&u.second());
				}
			} 
			if (u.third().valid()) {
				if (u.third().number_ == h.number_) {
					found = true;
				} else {
					captains.push_back(&u.third());
				}
			}
			if (captains.empty()) {
				delete &u;
				itor = reside_troops.erase(itor);
			} else if (found) {
				++ itor;
				u.replace_captains(captains);
			} else {
				++ itor;
			}
			if (found) {
				return true;
			}
		}
		std::vector<unit*> field_troops = city->field_troops();
		for (std::vector<unit*>::iterator itor = field_troops.begin(); itor != field_troops.end(); ++ itor) {
			unit& u = **itor;
			std::vector<hero*> captains;
			bool found = false;
			if (u.master().number_ == h.number_) {
				found = true;
			} else {
				captains.push_back(&u.master());
			}
			if (u.second().valid()) {
				if (u.second().number_ == h.number_) {
					found = true;
				} else {
					captains.push_back(&u.second());
				}
			} 
			if (u.third().valid()) {
				if (u.third().number_ == h.number_) {
					found = true;
				} else {
					captains.push_back(&u.third());
				}
			}
			if (captains.empty()) {
				units.erase2(&u);
			} else if (found) {
				u.replace_captains(captains);
			}
			if (found) {
				return true;
			}
		}
	}
	for (int type = 0; type < 3; type ++) { 
		std::vector<hero*>* list;
		if (type == 0) {
			list = &city->fresh_heros();
		} else if (type == 1) {
			list = &city->finish_heros();
		} else {
			list = &city->wander_heros();
		}
		for (std::vector<hero*>::iterator i3 = list->begin(); i3 != list->end();) {
			hero& h2 = **i3;
			if (h2.number_ == h.number_) {
				i3 = list->erase(i3);
				return true;
			} else {
				++ i3;
			}
		}
	}

	assert(false);
	return false;
}

const t_string& unit::name() const
{
	if (name_.empty()) {
		name_ = master_->name();
	}
	return name_;
}

void unit::regenerate_name()
{
	name_ = master_->name();
}

hero* unit::can_encourage()
{
	for (int i = 0; i < 2; i ++) {
		hero* h2 = NULL;
		if (i == 0) {
			if (second_->valid()) {
				h2 = second_;
			} else {
				break;
			}
		} else if (i == 1) {
			if (third_->valid()) {
				h2 = third_;
			}
		}
		if (h2 && (master_->is_consort(*h2) || master_->is_oath(*h2))) {
			return h2;
		}
	}
	return NULL;
}

void unit::do_encourage(hero& h1, hero& h2)
{
	game_display& gui = *resources::screen;
	rect_of_hexes& draw_area = gui.draw_area();
	if (point_in_rect_of_hexes(loc_.x, loc_.y, draw_area)) {
		utils::string_map symbols;
		std::stringstream strstr;
		symbols["first"] = tintegrate::generate_format(h1.name(), tintegrate::hero_color);
		symbols["second"] = tintegrate::generate_format(h2.name(), tintegrate::hero_color);
		int incident = game_events::INCIDENT_ENCOURAGECONSORT;
		if (h1.is_oath(h2)) {
			if (h1.gender_ == hero_gender_male) {
				incident = game_events::INCIDENT_MALEOATH;
			} else {
				incident = game_events::INCIDENT_FEMALEOATH;
			}
		}
		game_events::show_hero_message(&h1, NULL, vgettext2("$second! let improve morale, prepare to better battle.", symbols), incident);
	}

	heal(max_hit_points_ / 3);
	increase_feeling(game_config::increase_feeling);
}

void unit::set_temporary_state(int bit, bool set)
{
	if (set) {
		temporary_state_ |= 1 << bit;
	} else {
		temporary_state_ &= ~(1 << bit);
	}
}

int unit::character_level(int apply_to) const
{
	std::map<int, character_data>::const_iterator find = characters_.find(apply_to);
	if (find != characters_.end()) {
		return find->second.level;
	}
	return 0;		
}

hero* unit::character_hero(int apply_to) const
{
	std::map<int, character_data>::const_iterator find = characters_.find(apply_to);
	if (find != characters_.end()) {
		return find->second.h;
	}
	return NULL;	
}

bool unit::has_mayor() const
{
	if (master_->number_ < hero::number_normal_min) return false;
	if (master_->official_ == hero_official_mayor) return true;
	if (second_->valid() && second_->official_ == hero_official_mayor) return true;
	if (third_->valid() && third_->official_ == hero_official_mayor) return true;
	return false;
}

void unit::set_formation_flag(int flag, int flag2) 
{ 
	formation_flag_ = flag;
	formation_flag2_ = flag2;
}

int unit::amla() const
{
	int n = 0;
	const modifications_map& unit_modifications = unit_types.modifications();
	for (std::vector<std::string>::const_iterator it = modifications_.begin(); it != modifications_.end(); ++ it) {
		if (unit_modifications.find(*it) != unit_modifications.end()) {
			n ++;
		}
	}
	return n;
}

void unit::manage_attackable_rect(bool start)
{
	if (start) {
		// start
		attackable_rect_.x = loc_.x;
		attackable_rect_.y = loc_.y;
		attackable_rect_.w = attackable_rect_.h = 1;
	} else {
		// stop
		attackable_rect_ = extend_rectangle(*resources::game_map, attackable_rect_, max_range());
	}
}

void unit::fill_movable_loc(const map_location& loc)
{
	expand_rect_loc(attackable_rect_, loc);
}

bool unit::loc_is_align_with(const map_location& relative, bool x) const
{
	for (std::set<map_location>::const_iterator it = touch_locs_.begin(); it != touch_locs_.end(); ++ it) {
		if (it->x == relative.x) {
			return true;
		}
	}
	return false;
}

// true: this is priority than that.
bool unit::compare_action_order(const unit& that) const
{
	if (ticks_adjusting) {
		return true;
	} else if (that.ticks_adjusting) {
		return false;
	}

	if (!tent::turn_based) {
		if (ticks_ < that.ticks_) {
			return true;
		} else if (ticks_ > that.ticks_) {
			return false;
		}

		// 1)city, 2)troop
		if (!is_city()) {
			if (that.is_city()) {
				return false;
			}
			// in multiplayer, signature will not synchronization.
			if (loc_ != that.loc_) {
				return loc_ < that.loc_;
			}
			return !base_;

		} else if (!that.is_city()) {
			return true;
		}

		// city. 1)side, 2)cityno
		if (side_ != that.side_) {
			return side_ < that.side_;
		}

	} else {
		if (side_ != that.side_) {
			int a_side = side_ >= unit_map::top_side? side_: teams_.size() + side_;
			int b_side = that.side_ >= unit_map::top_side? that.side_: teams_.size() + that.side_;
			return a_side < b_side;
		}

		// 1)city, 2)troop
		if (!is_city()) {
			if (that.is_city()) {
				return false;
			}
			const team& t = teams_[side_ - 1];
			if (t.is_human()) {
				bool a_can_move = can_move();
				bool b_can_move = that.can_move();
				if (a_can_move && b_can_move) {
					if (!goto_.valid() && that.goto_.valid()) {
						return true;
					} else if (goto_.valid() && !that.goto_.valid()) {
						return false;
					}
				} else if (a_can_move && !b_can_move) {
					return true;
				} else if (!a_can_move && b_can_move) {
					return false;
				}
			}
			// in multiplayer, signature will not synchronization.
			if (loc_ != that.loc_) {
				return loc_ < that.loc_;
			}
			return !base_;

		} else if (!that.is_city()) {
			return true;
		}
	}

	if (is_capital(*resources::teams)) return true;
	if (that.is_capital(*resources::teams)) return false;
	return cityno_ < that.cityno_;
}

bool unit::is_capital(const std::vector<team>& teams) const
{
	const team& t = teams[side_ - 1];
	return master_->number_ == t.capital_number();
}

bool unit::sort_compare(const base_unit& that_base) const
{
	const unit* that = dynamic_cast<const unit*>(&that_base);
	return !that->consider_ticks() || compare_action_order(*that);
}

surface unit::generate_access_surface(int width, int height, bool greyscale) const
{
	surface stem_image = image::get_image(master_->image());
	if (greyscale) {
		stem_image = greyscale_image(stem_image);
	}
	stem_image = scale_surface(stem_image, width, height);
	surface image_ = stem_image;

	const int bar_vtl_ticks_width = 6;
	SDL_Rect dst_clip = create_rect(0, 0, 0, 0);

	if (get_state(ustate_tag::REVIVALED)) {
		image_ = adjust_surface_color(image_, 255, 0, 0);
	}

	int degree = percent_ticks();
	SDL_Color color = font::GOOD_COLOR;
	if (degree < 50) {
		color = font::BAD_COLOR;
	} else if (degree < 100) {
		color = font::YELLOW_COLOR;
	}

	std::string img_name = "misc/bar-vtl-ticks.png";
	if (current_can_action(*this)) {
		img_name = "misc/bar-vtl-ticks-hot.png";
	}
	if (!tent::turn_based || preferences::developer()) {
		draw_bar_to_surf(img_name, image_, 0, 12, height - 4 - (12 + 1), 1.0 * degree / 100, color, ftofxp(0.8), true);
	}

	if (is_city()) {
		// city name
		int font_size = 12;
		surface text_surf = font::get_rendered_text2(name(), -1, font_size, font::BIGMAP_COLOR);
		surface back_surf = font::get_rendered_text2(name(), -1, font_size, font::BLACK_COLOR);

		dst_clip.x = bar_vtl_ticks_width;
		dst_clip.y = height - 16;
		SDL_Rect dst;
		for (int dy=-1; dy <= 1; ++dy) {
			for (int dx=-1; dx <= 1; ++dx) {
				if (dx!=0 || dy!=0) {
					dst.x = dst_clip.x + dx;
					dst.y = dst_clip.y + dy;
					sdl_blit(back_surf, NULL, image_, &dst);
				}
			}
		}
		sdl_blit(text_surf, NULL, image_, &dst_clip);
	}

	// top-middle
	dst_clip.x = 8;
	dst_clip.y = 0;
	if (get_state(ustate_tag::DEPUTE)) {
		sdl_blit(image::get_image("misc/depute.png"), NULL, image_, &dst_clip);
	}
	if (has_mayor()) {
		sdl_blit(image::get_image("misc/special-unit.png"), NULL, image_, &dst_clip);
	}

	int integer = preferences::developer()? ticks(): level();
	blit_integer_surface(integer, image_, 0, 0);

	std::stringstream text;
	dst_clip = create_rect(0, 0, 0, 0);

	std::string lb_icon = can_formation_master()? "misc/formation.png": null_str;
	if (!lb_icon.empty()) {
		text.str("");
		text << lb_icon << "~SCALE(16, 16)";
		dst_clip.x = bar_vtl_ticks_width;
		dst_clip.y = image_->h - 16;
		sdl_blit(image::get_image(text.str()), NULL, image_, &dst_clip);
	}
	return image_;
}