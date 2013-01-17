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
#include "foreach.hpp"
#include "formula.hpp"
#include "game_display.hpp"
#include "game_preferences.hpp"
#include "gamestatus.hpp"
#include "gettext.hpp"
#include "halo.hpp"
#include "log.hpp"
#include "resources.hpp"
#include "unit_id.hpp"
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

namespace help {
	extern void generate_format(std::stringstream& strstr, const std::string& text, const std::string& color, int font_size, bool bold, bool italic);
}

namespace {
	/**
	 * Pointers to units which have data in their internal caches. The
	 * destructor of an unit removes itself from the cache, so the pointers are
	 * always valid.
	 */
	static std::vector<const unit *> units_with_cache;
}

uint8_t mask0_2bit[4] = {0xfc, 0xf3, 0xcf, 0x3f};

std::vector<std::pair<int, unit*> > unit::null_int_unitp_pair;

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
bool unit::ignore_pack_ = false;
unsigned char* unit::savegame_cache_ = NULL;

// Copy constructor
unit::unit(const unit& o):
	unit_merit(o),
	/* unit */
	units_(o.units_),
	heros_(o.heros_),
	cfg_(o.cfg_),
	loc_(o.loc_),
	advances_to_(o.advances_to_),
	type_(o.type_),
	packee_type_(o.packee_type_),
	unit_type_(o.unit_type_),
	packee_unit_type_(o.packee_unit_type_),
	character_(o.character_),
	race_(o.race_),
	name_(o.name_),
	type_name_(o.type_name_),
	undead_variation_(o.undead_variation_),
	variation_(o.variation_),

	hit_points_(o.hit_points_),
	max_hit_points_(o.max_hit_points_),
	experience_(o.experience_),
	max_experience_(o.max_experience_),
	level_(o.level_),
	upkeep_(o.upkeep_),
	random_traits_(o.random_traits_),
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
	end_turn_(o.end_turn_),
	resting_(o.resting_),
	attacks_left_(o.attacks_left_),
	max_attacks_(o.max_attacks_),

	states_(o.states_),
	known_boolean_states_(o.known_boolean_states_),
	emit_zoc_(o.emit_zoc_),
	cancel_zoc_(o.cancel_zoc_),
	state_(o.state_),

	attacks_(o.attacks_),
	facing_(o.facing_),

	trait_names_(o.trait_names_),
	unit_value_(o.unit_value_),
	goto_(o.goto_),
	flying_(o.flying_),

	anim_(NULL),
	next_idling_(0),

	frame_begin_time_(o.frame_begin_time_),
	unit_halo_(halo::NO_HALO),
	refreshing_(o.refreshing_),
	hidden_(o.hidden_),
	draw_bars_(o.draw_bars_),

	invisibility_cache_(),
	cityno_(o.cityno_),
	gold_income_(o.gold_income_),
	technology_income_(o.technology_income_),
	heal_(o.heal_),
	turn_experience_(o.turn_experience_),
	keep_turns_(o.keep_turns_),
	hide_turns_(o.hide_turns_),
	human_(o.human_),
	verifying_(o.verifying_),
	artifical_(o.artifical_),
	can_recruit_(o.can_recruit_),
	can_reside_(o.can_reside_),
	base_(o.base_),
	wall_(o.wall_),
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
	politics_(o.politics_),
	charm_(o.charm_),
	touch_locs_(o.touch_locs_),
	touch_dirs_(o.touch_dirs_),
	adjacent_size_(o.adjacent_size_),
	adjacent_size_2_(o.adjacent_size_2_),
	adjacent_size_3_(o.adjacent_size_3_),
	base_resistance_(o.base_resistance_),
	move_spectator_(o.move_spectator_),
	guard_attack_(o.guard_attack_),
	temporary_state_(o.temporary_state_),
	tactic_compare_damage_(o.tactic_compare_damage_),
	tactic_compare_resistance_(o.tactic_compare_resistance_),
	tactic_compare_movement_(o.tactic_compare_movement_)
{
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
           
unit::unit(unit_map& units, hero_map& heros, const config& cfg, bool use_traits, game_state* state, bool is_artifical) :
	unit_merit(),
	units_(units),
	heros_(heros),
	cfg_(cfg),
	loc_(), // !!! don't evalidate loc_, should use set_location to evalidate loc_
	advances_to_(),
	type_(cfg["type"]),
	packee_type_(),
	unit_type_(NULL),
	packee_unit_type_(NULL),
	character_(-1),
	race_(NULL),
	name_(),
	type_name_(),
	undead_variation_(),
	variation_(cfg["variation"]),
	hit_points_(1),
	max_hit_points_(0),
	experience_(0),
	max_experience_(0),
	level_(0),
	upkeep_(-1),
	random_traits_(true),
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
	end_turn_(false),
	resting_(false),
	attacks_left_(0),
	max_attacks_(0),
	states_(),
	known_boolean_states_(known_boolean_state_names_.size(),false),
	emit_zoc_(0),
	cancel_zoc_(false),
	state_(STATE_STANDING),
	attacks_(),
	facing_(map_location::SOUTH_EAST),
	trait_names_(),
	unit_value_(),
	goto_(),
	flying_(false),
	anim_(NULL),
	next_idling_(0),
	frame_begin_time_(0),
	unit_halo_(halo::NO_HALO),
	refreshing_(false),
	hidden_(false),
	draw_bars_(false),
	invisibility_cache_(),
	cityno_(-1),
	gold_income_(),
	technology_income_(0),
	heal_(0),
	turn_experience_(0),
	keep_turns_(game_config::max_keep_turns),
	hide_turns_(0),
	human_(false),
	verifying_(false),
	artifical_(is_artifical),
	can_recruit_(false),
	can_reside_(false),
	base_(false),
	wall_(false),
	land_wall_(true),
	walk_wall_(false),
	terrain_(t_translation::NONE_TERRAIN),
	arms_(0),
	touch_dirs_(),
	touch_locs_(),
	adjacent_size_(0),
	adjacent_size_2_(0),
	adjacent_size_3_(0),
	base_resistance_(),
	move_spectator_(units_),
	guard_attack_(-2),
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
		character_ = unit_types.character_from_id(cfg["character"].str());
	} else {
		character_ = unit_type_->character();
	}
	
	advance_to(unit_type_, use_traits, state);

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

	if (const config &status_flags = cfg.child("status")) {
		foreach (const config::attribute &st, status_flags.attribute_range()) {
			if (st.second.to_bool()) {
				set_state(st.first, true);
			}
		}
	}

	// Remove animations from private cfg, they're not needed there now
	foreach(const std::string& tag_name, unit_animation::all_tag_names()) {
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

	// variable before modify accord heros
	bool modified = cfg["modified"].to_bool();

	// unit_merit
	field_turns_ = cfg["field_turns"].to_int();
	cause_damage_ = cfg["cause_damage"].to_int();
	been_damage_ = cfg["been_damage"].to_int();
	defeat_units_ = cfg["defeat_units"].to_int();

	if (cfg_.has_attribute("keep_turns")) {
		keep_turns_ = cfg_["keep_turns"].to_int();
	}
	if (cfg_.has_attribute("human")) {
		human_ = cfg_["human"].to_bool();
	}

	/** @todo Are these modified by read? if not they can be removed. */
	end_turn_ = false;
	refreshing_  = false;
	hidden_ = false;
	game_config::add_color_info(cfg);

	static char const *internalized_attrs[] = { "type", "name",
		"gender", "random_gender", "variation",
		"side", "facing", "race",
		"level", "undead_variation", "max_attacks",
		"attacks_left", "alpha", "flying", "cost",
		"max_hitpoints", "max_moves", "max_experience",
		"advances_to", "hitpoints", "goto_x", "goto_y", "moves",
		"experience", "resting", "alignment",
		"x", "y"};
	foreach (const char *attr, internalized_attrs) {
		cfg_.remove_attribute(attr);
	}

	if (human_) {
		rpg::humans.insert(this);
	}
}

unit::unit(unit_map& units, hero_map& heros, const uint8_t* mem, bool use_traits, game_state* state, bool artifical) :
	unit_merit(),
	units_(units),
	heros_(heros),
	cfg_(),
	loc_(), // !!! don't evalidate loc_, should use set_location to evalidate loc_
	advances_to_(),
	type_(),
	packee_type_(),
	unit_type_(NULL),
	packee_unit_type_(NULL),
	character_(-1),
	race_(NULL),
	name_(),
	type_name_(),
	undead_variation_(),
	variation_(),
	hit_points_(1),
	max_hit_points_(0),
	experience_(0),
	max_experience_(0),
	level_(0),
	upkeep_(-1),
	random_traits_(true),
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
	end_turn_(false),
	resting_(false),
	attacks_left_(0),
	max_attacks_(0),
	states_(),
	known_boolean_states_(known_boolean_state_names_.size(),false),
	emit_zoc_(0),
	cancel_zoc_(false),
	state_(STATE_STANDING),
	attacks_(),
	facing_(map_location::SOUTH_EAST),
	trait_names_(),
	unit_value_(),
	goto_(),
	flying_(false),
	anim_(NULL),
	next_idling_(0),
	frame_begin_time_(0),
	unit_halo_(halo::NO_HALO),
	refreshing_(false),
	hidden_(false),
	draw_bars_(false),
	invisibility_cache_(),
	cityno_(-1),
	gold_income_(),
	technology_income_(0),
	heal_(0),
	turn_experience_(0),
	keep_turns_(game_config::max_keep_turns),
	hide_turns_(0),
	human_(false),
	verifying_(false),
	artifical_(artifical),
	can_recruit_(false),
	can_reside_(false),
	base_(false),
	wall_(false),
	land_wall_(true),
	walk_wall_(false),
	terrain_(t_translation::NONE_TERRAIN),
	arms_(0),
	touch_dirs_(),
	touch_locs_(),
	adjacent_size_(0),
	adjacent_size_2_(0),
	adjacent_size_3_(0),
	base_resistance_(),
	move_spectator_(units_),
	guard_attack_(-2),
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
	advance_to(unit_type_, use_traits, state);

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
	foreach(const std::string& tag_name, unit_animation::all_tag_names()) {
		cfg_.clear_children(tag_name);
	}

	/** @todo Are these modified by read? if not they can be removed. */
	end_turn_ = false;
	refreshing_  = false;
	hidden_ = false;
	// game_config::add_color_info(cfg);

	if (human_) {
		rpg::humans.insert(this);
	}
}

void unit::clear_status_caches()
{
	for(std::vector<const unit *>::const_iterator itor = units_with_cache.begin();
			itor != units_with_cache.end(); ++itor) {
		(*itor)->clear_visibility_cache();
	}

	units_with_cache.clear();
}

unit::unit(unit_map& units, hero_map& heros, type_heros_pair& t, int cityno, bool real_unit, bool is_artifical) :
	unit_merit(),
	units_(units),
	heros_(heros),
	cfg_(),
	loc_(),
	advances_to_(),
	type_(),
	packee_type_(),
	character_(-1),
	unit_type_(NULL),
	packee_unit_type_(NULL),
	race_(NULL),
	name_(),
	type_name_(),
	undead_variation_(),
	variation_(),
	hit_points_(0),
	max_hit_points_(0),
	experience_(0),
	max_experience_(0),
	level_(0),
	upkeep_(-1),
	random_traits_(true),
	alignment_(),
	flag_rgb_(),
	image_mods_(),
	traits_(),
	tactics_(),
	modifications_(),
	side_(units.city_from_cityno(cityno)->side()),
	alpha_(),
	movement_(0),
	max_movement_(0),
	movement_costs_(),
	defense_mods_(),
	end_turn_(false),
	resting_(false),
	attacks_left_(0),
	max_attacks_(0),
	states_(),
	known_boolean_states_(known_boolean_state_names_.size(),false),
	emit_zoc_(0),
	cancel_zoc_(false),
	state_(STATE_STANDING),
	attacks_(),
	facing_(map_location::SOUTH_EAST),
	trait_names_(),
	unit_value_(),
	goto_(),
	flying_(false),
	anim_(NULL),
	next_idling_(0),
	frame_begin_time_(0),
	unit_halo_(halo::NO_HALO),
	refreshing_(false),
	hidden_(false),
	draw_bars_(false),
	invisibility_cache_(),
	cityno_(cityno),
	gold_income_(0),
	technology_income_(0),
	heal_(0),
	turn_experience_(0),
	keep_turns_(game_config::max_keep_turns),
	hide_turns_(0),
	human_(false),
	verifying_(false),
	artifical_(is_artifical),
	can_recruit_(false),
	can_reside_(false),
	base_(false),
	wall_(false),
	land_wall_(true),
	walk_wall_(false),
	terrain_(t_translation::NONE_TERRAIN),
	arms_(0),
	touch_dirs_(),
	touch_locs_(),
	adjacent_size_(0),
	adjacent_size_2_(0),
	adjacent_size_3_(0),
	base_resistance_(),
	move_spectator_(units_),
	guard_attack_(-2),
	temporary_state_(0),
	tactic_compare_damage_(),
	tactic_compare_resistance_(),
	tactic_compare_movement_(0)
{
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

	artifical& city = *units_.city_from_cityno(cityno_);
	character_ = city.character();
	if (unit_type_->character() != NO_CHARACTER) {
		VALIDATE(unit_type_->character() == character_, "error character in recruiting unit_type: " + unit_type_->id());
	}

	advance_to(unit_type_, real_unit);

	// fill those after traits and modifs to have correct max
	movement_ = max_movement_;
	hit_points_ = max_hit_points_;
	attacks_left_ = max_attacks_;

	/**
	 * @todo Test whether the calls above modify these values if not they can
	 * removed, since already set in the initialization list.
	 */
	anim_ = NULL;
	end_turn_ = false;
	next_idling_ = 0;
	frame_begin_time_ = 0;
	unit_halo_ = halo::NO_HALO;

	calculate_5fields();
	modify_according_to_hero(true, true);

	if (human_) {
		rpg::humans.insert(this);
	}
}

unit::~unit()
{
	clear_haloes();

	delete anim_;

	// Remove us from rpg::humans
	if (human_) {
		size_t s = rpg::humans.erase(this);
		VALIDATE(s, "unit::~unit, this is human, but cannot find it in rpg::humans");
	}

	// Remove us from the status cache
	std::vector<const unit *>::iterator itor =
	std::find(units_with_cache.begin(), units_with_cache.end(), this);

	if (itor != units_with_cache.end()) {
		units_with_cache.erase(itor);
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

void unit::write(uint8_t* mem) const
{
	unit_fields_t* fields = (unit_fields_t*)mem;

	fields->states_ = 0;
	for (int t = STATE_MIN; t < STATE_COUNT; t ++) {
		fields->states_ |= get_state((state_t)t)? (1 << (t - STATE_MIN)): 0;
	}
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
	fields->facing_ = facing_;
	fields->keep_turns_ = keep_turns_;
	fields->hide_turns_ = hide_turns_;
	fields->character_ = character_;
	fields->human_ = human_? 1: 0;
	fields->random_traits_ = random_traits_? 1: 0;
	fields->resting_ = resting_? 1: 0;
	fields->modified_ = 1;

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
		character_ = fields->character_;

		random_traits_ = fields->random_traits_? true: false;

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

	for (int t = STATE_MIN; t < STATE_COUNT; t ++) {
		set_state((state_t)t, (fields->states_ & (1 << (t - STATE_MIN)))? true: false);
	}
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
	keep_turns_ = fields->keep_turns_;
	hide_turns_ = fields->hide_turns_;
	human_ = fields->human_? true: false;
	resting_ = fields->resting_? true: false;
	// fields->modified_ = 1;

/*
	// attack
	attacks_.clear();
	for (int i = 0; i < fields->attacks_.size_; i ++) {
		attack_fields* a_fields = (attack_fields*)(mem + fields->attacks_.offset_) + i;
		config attack;
				
		// id
		str.assign((const char*)mem + a_fields->id_.offset_, a_fields->id_.size_);
		attack["name"] = str;
		// type
		str.assign((const char*)mem + a_fields->type_.offset_, a_fields->type_.size_);
		attack["type"] = str;
		// icon
		str.assign((const char*)mem + a_fields->icon_.offset_, a_fields->icon_.size_);
		attack["icon"] = str;
		// specials
		str.assign((const char*)mem + a_fields->specials_.offset_, a_fields->specials_.size_);
		attack["specials"] = str;

		if (a_fields->range_ == attack_type::MELEE) {
			attack["range"] = "melee";
		} else if (a_fields->range_ == attack_type::RANGED) {
			attack["range"] = "ranged";
		} else {
			attack["range"] = "cast";
		}
		attack["damage"] = a_fields->damage_;
		attack["number"] = a_fields->number_;

		attacks_.push_back(attack_type(attack));
	}
*/
	
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
		const std::string &avl = t["availability"];
		if (avl == "musthave") {
			traits_.push_back(tid);
			continue;
		}
		if (artifical_) {
			// this is artifical, don't accept trait that effect movement
			bool effect_movement = false;
			foreach (const config& effect, t.child_range("effect")) {
				if (effect["apply_to"] == "movement") {
					effect_movement = true;
					break;
				}
			}
			if (effect_movement) {
				continue;
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
	for (; nb_traits < max_traits && !candidate_traits.empty(); ++nb_traits)
	{
		int num = (state ? state->rng().get_next_random() : get_random_nocheck())
		          % candidate_traits.size();
		traits_.push_back(candidate_traits[num]);
		candidate_traits.erase(candidate_traits.begin() + num);
	}

	// Once random traits are added, don't do it again.
	// Such as when restoring a saved character.
	random_traits_ = false;
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
void unit::advance_to(const unit_type *t, bool use_traits, game_state *state)
{
	VALIDATE(!t->packer(), "unit::advance_to, packer unit_type: " + t->id());

	const std::string mod_childs[] = {"movement_costs", "defense", "resistance"};

	// Clear modification-related caches
	movement_costs_.clear();
	defense_mods_.clear();

	// Clear modified configs
	foreach (const std::string& tag, mod_childs) {
		cfg_.clear_children(tag);
	}
	// Remove old type's halo, animations, abilities, attacks and advancement options (AMLA)
	foreach(const std::string& tag_name, unit_animation::all_tag_names()) {
		cfg_.clear_children(tag_name);
	}

	if(t->movement_type().get_parent()) {
		cfg_.merge_with(t->movement_type().get_parent()->get_cfg());
	}
	cfg_.merge_with(t->get_cfg());

	// animation tags is more, don't write it to save file
	foreach(const std::string& tag_name, unit_animation::all_tag_names()) {
		cfg_.clear_children(tag_name);
	}

	cfg_.clear_children("male");
	cfg_.clear_children("female");

	advances_to_ = t->advances_to(character_);

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
	heal_ = t->heal();
	turn_experience_ = t->turn_experience();
	flying_ = t->movement_type().is_flying();

	max_attacks_ = t->max_attacks();

	can_recruit_ = t->can_recruit_;
	can_reside_ = t->can_reside_;
	base_ = t->base_;
	wall_ = t->wall_;
	land_wall_ = t->land_wall_;
	walk_wall_ = t->walk_wall_;
	arms_ = t->arms_;
	terrain_ = t->terrain_;

	flag_rgb_ = t->flag_rgb();

	type_ = t->id();
	unit_type_ = t;

	random_traits_ = cfg_["random_traits"].to_bool(true);

	if (random_traits_) {
		generate_traits(!use_traits, state);
	} else {
		// This will add any "musthave" traits to the new unit that it doesn't already have.
		// This covers the Dark Sorcerer advancing to Lich and gaining the "undead" trait,
		// but random and/or optional traits are not added,
		// and neither are inappropiate traits removed.
		generate_traits(true);
	}

	// anim
	static char const* anim_attrs[] = { "die_sound" };
	foreach (const char *attr, anim_attrs) {
		cfg_.remove_attribute(attr);
	}

	set_state(STATE_POISONED, false);
	set_state(STATE_SLOWED, false);
	set_state(STATE_BROKEN, false);
	set_state(STATE_PETRIFIED, false);
	end_turn_ = false;
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
	foreach (const std::string& tag, mod_childs) {
		cfg_.clear_children(tag);
	}
	// Remove old type's halo, animations, abilities, attacks and advancement options (AMLA)
	foreach(const std::string& tag_name, unit_animation::all_tag_names()) {
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
	foreach(const std::string& tag_name, unit_animation::all_tag_names()) {
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

void unit::set_human(bool val)
{
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

int unit::gold_income() const 
{
	if (!gold_income_) {
		return 0;
	} else {
		artifical* city = units_.city_from_cityno(cityno_);
		return city->commercial_exploiture() * gold_income_ / 100;
	}
}

int unit::technology_income() const
{
	if (!technology_income_) {
		return 0;
	} else {
		artifical* city = units_.city_from_cityno(cityno_);
		return city->technology_exploiture() * technology_income_ / 100;
	}
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

void unit::get_experience(const increase_xp::ublock& ub, int xp, bool master, bool second, bool third) 
{ 
	game_display &disp = *game_display::get_singleton();
	bool has_carry = false;
	bool advance_packs = false;
	bool has_rpg = false;
	team& current_team = (*resources::teams)[side_ - 1];

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

		increase_xp::hblock& hb = increase_xp::generic_hblock();

		if (ub.leadership) {
			hb.leadership = inc_ability_xp * current_team.leadership_speed_ / 100;
		}
		if (ub.force) {
			hb.force = inc_ability_xp * current_team.force_speed_ / 100;
		}
		if (ub.intellect) {
			hb.intellect = inc_ability_xp * current_team.intellect_speed_ / 100;
		}
		if (ub.politics) {
			hb.politics = inc_ability_xp * current_team.politics_speed_ / 100;
		}
		if (ub.charm) {
			hb.charm = inc_ability_xp * current_team.charm_speed_ / 100;
		}
		if (ub.arms) {
			hb.arms[arms_] = inc_arms_xp * current_team.arms_speed_[arms_] / 100;
			if (packee_unit_type_) {
				hb.arms[packee_unit_type_->arms()] = inc_arms_xp * current_team.arms_speed_[packee_unit_type_->arms()] / 100;
			}
		}
		for (int i = 0; i < HEROS_MAX_SKILL; i ++) {
			if (ub.skill[i]) {
				hb.skill[i] = inc_skill_xp;
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
			int prev_navigation = current_team.navigation();
			current_team.add_navigation(inc_navigation_xp * current_team.navigation_civi_increase_ / 100);
			if (unit_types.navigation_can_advance(prev_navigation, current_team.navigation())) {
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
		const std::pair<unit**, size_t> p = current_team.field_troop();
		unit** troops = p.first;
		size_t troops_vsize = p.second;
		const unit_type* packer = unit_types.find(unit_types.id_from_navigation(current_team.navigation()));
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

	if (has_rpg && xp > 0 && rpg::stratum != hero_stratum_leader && !rpg::forbids) {
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

std::string unit::side_id() const {return teams_manager::get_teams()[side()-1].save_id(); }

void unit::set_movement(int moves)
{
	//FIXME: we shouldn't set those here, other code use this a simple setter.
	end_turn_ = false;
	movement_ = std::max<int>(0, moves);
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

bool unit::new_turn()
{
	bool erase = false;

	end_turn_ = false;
	movement_ = total_movement();
	attacks_left_ = max_attacks_;
	set_state(STATE_UNCOVERED, false);

	const std::vector<team>& teams = *resources::teams;
	if (artifical_ || !teams.size()) {
		if (artifical_) {
			new_turn_tactics();
		}
		return false;
	}

	hero& leader = *(teams[side_ - 1].leader());
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
			to_city->wander_into(**itor, teams[side_ - 1].is_human()? false: true);
		}
		if (captains.empty()) {
			erase = true;
			// this troops will be erase.
		} else {
			replace_captains(captains);
		}
	}

	field_turns_ ++;
	if (hide_turns_) {
		hide_turns_ --;
	}

	// field troop, increase activity
	increase_activity(8);

	increase_feeling(game_config::increase_feeling);

	if (!artifical_) {
		get_experience(increase_xp::turn_ublock(*this), 3);
	}

	new_turn_tactics();

	return erase;
}

void unit::end_turn()
{
	set_state(STATE_SLOWED, false);
	set_state(STATE_BROKEN, false);
	if((movement_ != total_movement()) && !(get_state(STATE_NOT_MOVED))) {
		resting_ = false;
	}
	set_state(STATE_NOT_MOVED,false);
}

void unit::new_scenario()
{
	// Set the goto-command to be going to no-where
	goto_ = map_location();

	heal_all();
	set_state(STATE_SLOWED, false);
	set_state(STATE_BROKEN, false);
	set_state(STATE_POISONED, false);
	set_state(STATE_PETRIFIED, false);
}

void unit::heal(int amount)
{
	int max_hp = max_hitpoints();
	if (hit_points_ < max_hp) {
		hit_points_ += amount;
		if (hit_points_ > max_hp) {
			hit_points_ = max_hp;
		}
	}
	if(hit_points_<1) {
		hit_points_ = 1;
	}
}

const std::map<std::string,std::string> unit::get_states() const
{
	std::map<std::string, std::string> all_states;
	foreach (std::string const &s, states_) {
		all_states[s] = "yes";
	}
	for (std::map<std::string, state_t>::const_iterator i = known_boolean_state_names_.begin(),
	     i_end = known_boolean_state_names_.end(); i != i_end; ++i)
	{
		if (get_state(i->second)) {
			all_states.insert(make_pair(i->first, "yes"));
		}

	}
	return all_states;
}

bool unit::get_state(const std::string &state) const
{
	state_t known_boolean_state_id = get_known_boolean_state_id(state);
	if (known_boolean_state_id!=STATE_UNKNOWN){
		return get_state(known_boolean_state_id);
	}
	return states_.find(state) != states_.end();
}

void unit::set_state(state_t state, bool value)
{
	known_boolean_states_[state] = value;
}

bool unit::get_state(state_t state) const
{
	return known_boolean_states_[state];
}

unit::state_t unit::get_known_boolean_state_id(const std::string &state) {
	std::map<std::string, state_t>::const_iterator i = known_boolean_state_names_.find(state);
	if (i != known_boolean_state_names_.end()) {
		return i->second;
	}
	return STATE_UNKNOWN;
}

std::map<std::string, unit::state_t> unit::known_boolean_state_names_ = get_known_boolean_state_names();

std::map<std::string, unit::state_t> unit::get_known_boolean_state_names()
{
	std::map<std::string, state_t> known_boolean_state_names_map;
	known_boolean_state_names_map.insert(std::make_pair("slowed",STATE_SLOWED));
	known_boolean_state_names_map.insert(std::make_pair("broken",STATE_BROKEN));
	known_boolean_state_names_map.insert(std::make_pair("poisoned",STATE_POISONED));
	known_boolean_state_names_map.insert(std::make_pair("petrified",STATE_PETRIFIED));
	known_boolean_state_names_map.insert(std::make_pair("uncovered", STATE_UNCOVERED));
	known_boolean_state_names_map.insert(std::make_pair("not_moved",STATE_NOT_MOVED));
	known_boolean_state_names_map.insert(std::make_pair("reinforced",STATE_REINFORCED));
	known_boolean_state_names_map.insert(std::make_pair("legeritied",STATE_LEGERITIED));
	//not sure if "guardian" is a yes/no state.
	//known_boolean_state_names_map.insert(std::make_pair("guardian",STATE_GUARDIAN));
	return known_boolean_state_names_map;
}

void unit::set_state(const std::string &state, bool value)
{
	state_t known_boolean_state_id = get_known_boolean_state_id(state);
	if (known_boolean_state_id != STATE_UNKNOWN) {
		set_state(known_boolean_state_id, value);
		return;
	}
	if (value)
		states_.insert(state);
	else
		states_.erase(state);
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
	if (!cfg["hp"].blank()) {
		if (cfg["hp"].to_bool() && hit_points_ <= 0) {
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

	if (!cfg["master_hero"].blank()) {
		if (cfg["master_hero"].to_int() != master_->number_) {
			return false;
		}
	}

	// Allow 'speaker' as an alternative to id, since people use it so often
	if (!cfg["speaker"].empty() && cfg["speaker"].to_int() != master_->number_) {
		return false;
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
		std::vector<artifical*>& citys = (*resources::teams)[side_ - 1].holded_cities();
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

	const config::attribute_value cfg_id = cfg["id"];
	if (!cfg_id.blank()) {
		const std::string& id = cfg_id;
		const std::string& this_id = this->id();

		if (id == this_id) {
		}
		else if (std::find(id.begin(), id.end(), ',') == id.end()){
			return false;
		}
		else {
			const std::vector<std::string>& ids = utils::split(id);
			if (std::find(ids.begin(), ids.end(), this_id) == ids.end()) {
				return false;
			}
		}
	}

	// Allow 'speaker' as an alternative to id, since people use it so often
	config::attribute_value cfg_speaker = cfg["speaker"];
	if (!cfg_speaker.blank() && cfg_speaker.str() != id()) {
		return false;
	}

	if(cfg.has_child("filter_location")) {
		assert(resources::game_map != NULL);
		assert(resources::teams != NULL);
		assert(resources::tod_manager != NULL);
		assert(resources::units != NULL);
		const vconfig& t_cfg = cfg.child("filter_location");
		terrain_filter t_filter(t_cfg, *resources::units, use_flat_tod);
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
	if (!cfg_type.blank())
	{
		std::string type = cfg_type;
		const std::string& this_type = type_id();

		// We only do the full CSV search if we find a comma in there,
		// and if the subsequence is found within the main sequence.
		// This is because doing the full CSV split is expensive.
		if(type == this_type) {
			// pass
		} else if(std::find(type.begin(),type.end(),',') != type.end() &&
		   std::search(type.begin(),type.end(),this_type.begin(),
					   this_type.end()) != type.end()) {
			const std::vector<std::string>& vals = utils::split(type);

			if(std::find(vals.begin(),vals.end(),this_type) == vals.end()) {
				return false;
			}
		} else {
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
		const std::vector<std::string> vstr = utils::split(cfg_level.str());
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
				if (unit_itor == units.end()
				|| !unit_itor->matches_filter(*i, unit_itor->get_location(), use_flat_tod)) {
					continue;
				}
				config::attribute_value i_is_enemy = (*i)["is_enemy"];
				if (i_is_enemy.blank() || i_is_enemy.to_bool() ==
				    teams_manager::get_teams()[this->side() - 1].is_enemy(unit_itor->side())) {
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
		const unit_callable callable(std::pair<map_location, unit>(loc,*this));
		const game_logic::formula form(cfg_formula);
		if(!form.evaluate(callable).as_bool()) {///@todo use formula_ai
			return false;
		}
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

	cfg["character"] = unit_types.character_id(character_);

	cfg["cityno"] = cityno_;

	// heros_army
	std::stringstream str;
	str << master_->number_;
	if (second_->valid()) {
		str << "," << second_->number_;
	}
	if (third_->valid()) {
		str << "," << third_->number_;
	}
	cfg["heros_army"] = str.str();
	cfg["master_hero"] = master_->number_;

	config status_flags;
	std::map<std::string,std::string> all_states = get_states();
	for(std::map<std::string,std::string>::const_iterator st = all_states.begin(); st != all_states.end(); ++st) {
		status_flags[st->first] = st->second;
	}

	cfg.clear_children("status");
	cfg.add_child("status",status_flags);

	cfg["facing"] = map_location::write_direction(facing_);

	cfg["goto_x"] = goto_.x + 1;
	cfg["goto_y"] = goto_.y + 1;

	cfg["random_traits"] = random_traits_;
	cfg["resting"] = resting_;

	cfg["attacks_left"] = attacks_left_;
	cfg.clear_children("attack");
	for (std::vector<attack_type>::const_iterator i = attacks_.begin(); i != attacks_.end(); ++i) {
		config& att = cfg.add_child("attack",i->get_cfg());
		// don't save description of [specials]
		config& specials = att.child("specials");
		if (!specials) continue;
		foreach (const config::any_child &sp, specials.all_children_range()) {
			const_cast<config *>(&sp.cfg)->remove_attribute("description");
		}
	}

	cfg["traits"] = utils::join(traits_);
	cfg["modifications"] = utils::join(modifications_);

	cfg["keep_turns"] = keep_turns_;
	cfg["hide_turns"] = hide_turns_;
	cfg["human"] = human_;

	// unit_merit
	cfg["field_turns"] = field_turns_;
	cfg["cause_damage"] = cause_damage_;
	cfg["been_damage"] = been_damage_;
	cfg["defeat_units"] = defeat_units_;

	// variables before modify according heros
	cfg["modified"] = true;
}

void unit::modify_according_to_hero(bool fill_up_hp, bool fill_up_movement)
{
	int current_movement = movement_;
	double increase_percent, decrease_percent;

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

	double arms_feature_bonus = unit_feature_val(arms_)? 0.075: 0;
	if (unit_feature_val(HEROS_MAX_ARMS + arms_)) {
		arms_feature_bonus = 0.15;
	}
	double range_feature_bonus[3];
	range_feature_bonus[0] = unit_feature_val(hero_feature_range_min)? 0.075: 0;
	if (unit_feature_val(3 + hero_feature_range_min)) {
		range_feature_bonus[0] = 0.15;
	}
	range_feature_bonus[1] = unit_feature_val(hero_feature_range_min + 1)? 0.075: 0;
	if (unit_feature_val(3 + hero_feature_range_min + 1)) {
		range_feature_bonus[1] = 0.15;
	}
	range_feature_bonus[2] = unit_feature_val(hero_feature_range_min + 2)? 0.075: 0;
	if (unit_feature_val(3 + hero_feature_range_min + 2)) {
		range_feature_bonus[2] = 0.15;
	}

	// attack
	double hero_bonus = 0;
	if ((leadership_ > 80) || (force_ > 80) || (intellect_ > 80)) {
		hero_bonus = 1.0 * ((std::max<uint16_t>(80, leadership_) - 80) * 60 + (std::max<uint16_t>(80, force_) - 80) * 25 + (std::max<uint16_t>(80, intellect_) - 80) * 25) / 10000;
		hero_bonus = hero_bonus / 10; // max hero_bonus is about 1.06, this max increase_percent is about 5 level adaptability
	}
	for (std::vector<attack_type>::iterator a = attacks_.begin(); a != attacks_.end(); ++a) {
		increase_percent = hero_bonus;

		// effect of adaptability
		increase_percent += 0.025 * adaptability_[arms_];

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
		hero_bonus = 1.0 * (leadership_ - 100) / 200;
		hero_bonus = hero_bonus / 5; // max bonus is about 0.385, this max increase_percent is about 5 level adaptability.
	}
	config::child_itors cfg_range = cfg_.child_range("resistance");
	if (cfg_range.first != cfg_range.second) {
		config &target = *cfg_range.first;
		foreach (const config::attribute &istrmap, base_resistance_.attribute_range()) {
			int value = lexical_cast<int>(istrmap.second);
			decrease_percent = hero_bonus;

			// effect of adaptability
			decrease_percent += 0.02 * adaptability_[arms_];

			// effect of arms feature
			decrease_percent += arms_feature_bonus / 2;

			// effect of other feature
			if (unit_feature_val(hero_feature_firm)) {
				decrease_percent += 0.08;
			}

			value = (int)(value * (1.0 - std::min<double>(decrease_percent, 0.6)));
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
	if (unit_feature_val(HEROS_MAX_ARMS * 2 + arms_)) {
		max_movement_ = max_movement_ + 1;
	}

	// redo technology
	if (resources::teams) {
		int type = filter::ARTIFICAL;
		if (!is_artifical()) {
			type = filter::TROOP;
		} else if (is_city()) {
			type = filter::CITY;
		}
		const team& current_team = (*resources::teams)[side_ - 1];
		const std::vector<const technology*>& holded_technologies = current_team.holded_technologies();
		for (std::vector<const technology*>::const_iterator it = holded_technologies.begin(); it != holded_technologies.end(); ++ it) {
			const std::vector<const technology*>& parts = (*it)->parts();
			for (std::vector<const technology*>::const_iterator it2 = parts.begin(); it2 != parts.end(); ++ it2) {
				const technology& t = **it2;
				if (t.occasion() == technology::MODIFY && t.filter(type, arms_)) {
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
		foreach (const config::attribute &istrmap, resistance_cfg.attribute_range()) {
			// remember for comapre in future.
			tactic_compare_resistance_[istrmap.first] = lexical_cast<int>(istrmap.second);
		}
	}
	tactic_compare_movement_ = max_movement_;

	// redo tactic
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
			foreach (const config &effect, item.child_range("effect")) {
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

	if (fill_up_hp) {
		hit_points_ = max_hit_points_;
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
	std::vector<team>& teams_ = *resources::teams;
	hero* leader = NULL;
	const team* t = NULL;
	if (teams_.size()) {
		leader = teams_[side_ - 1].leader();
		t = &(teams_[side_ - 1]);
	} else {
		leader = team::player_leader();
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
		activity_ajdusted = 1 * second_->politics_ * second_->activity_ / HEROS_FULL_ACTIVITY;
		max_assistant.politics_ = fxptoi9(activity_ajdusted) * 100 / second_relation;
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
		activity_ajdusted = 1 * third_->politics_ * third_->activity_ / HEROS_FULL_ACTIVITY;
		tmp = fxptoi9(activity_ajdusted) * 100 / third_relation;
		max_assistant.politics_ = std::max<uint16_t>(max_assistant.politics_, tmp);
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
	activity_ajdusted = 1 * master_->politics_ * master_->activity_ / HEROS_FULL_ACTIVITY;
	politics_ = fxptoi9(activity_ajdusted) + max_assistant.politics_;
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
	const treasure_map& treasures = unit_types.treasures();
	memset(feature_, 0, HEROS_FEATURE_M2BYTES);

	std::set<int> holded_features;
	if (master_->feature_ != HEROS_NO_FEATURE) {
		holded_features.insert(master_->feature_);
	}
	if (master_->treasure_ != HEROS_NO_TREASURE) {
		treasure_map::const_iterator it = treasures.find(master_->treasure_);
		if (it != treasures.end()) {
			for (std::vector<int>::const_iterator it2 = it->second.begin(); it2 != it->second.end(); ++ it2) {
				int single_feature = *it2;
				holded_features.insert(*it2);
			}
		}
	}
	if (second_valid) {
		if (second_->feature_ != HEROS_NO_FEATURE) {
			holded_features.insert(second_->feature_);
		}
		if (second_->treasure_ != HEROS_NO_TREASURE) {
			treasure_map::const_iterator it = treasures.find(second_->treasure_);
			if (it != treasures.end()) {
				for (std::vector<int>::const_iterator it2 = it->second.begin(); it2 != it->second.end(); ++ it2) {
					int single_feature = *it2;
					holded_features.insert(*it2);
				}
			}
		}
	}
	if (third_valid) {
		if (third_->feature_ != HEROS_NO_FEATURE) {
			holded_features.insert(third_->feature_);
		}
		if (third_->treasure_ != HEROS_NO_TREASURE) {
			treasure_map::const_iterator it = treasures.find(third_->treasure_);
			if (it != treasures.end()) {
				for (std::vector<int>::const_iterator it2 = it->second.begin(); it2 != it->second.end(); ++ it2) {
					int single_feature = *it2;
					holded_features.insert(*it2);
				}
			}
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
}

void unit::adjust() 
{
	const unit_type* current_type = type();

	calculate_5fields();
	if (packed()) {
		int navigation = (*resources::teams)[side_ - 1].navigation();
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
			foreach (const config::attribute &i, resistance.attribute_range()) {
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

std::string unit::form_tip() const
{ 
	if (game_config::tiny_gui) {
		return form_tiny_tip();
	}

	std::vector<team>& teams_ = *resources::teams;
	std::stringstream text, strstr;
	std::vector<std::string> abilities_tt;
	if (!artifical_) {
		text << _("Name") << ": " << name() << _("name^Troop") << "\n";

		artifical* owner_city = units_.city_from_cityno(cityno_);
		if (owner_city) {
			text << _("City") << ": " << owner_city->master().name();
		} else {
			text << _("City") << ": --";
		}

		text << " (" << teams_[side_ - 1].name() << ")\n";

		text << _("Hero") << ": ";
		if (master_->activity_ == 255) {
			text << master_->name();
		} else {
			help::generate_format(text, master_->name(), "red", 0, false, false);
		}
		help::generate_format(text, "(", "blue", 0, false, false);
		text << master_->loyalty(*teams_[master_->side_].leader());
		help::generate_format(text, ")", "blue", 0, false, false);
		if (second_->valid()) {
			text << "\n    ";
			if (second_->activity_ == 255) {
				text << second_->name();
			} else {
				help::generate_format(text, second_->name(), "red", 0, false, false);
			}
			help::generate_format(text, "(", "blue", 0, false, false);
			text << second_->loyalty(*teams_[second_->side_].leader());
			help::generate_format(text, ")", "blue", 0, false, false);
		}
		if (third_->valid()) {
			text << " ";
			if (third_->activity_ == 255) {
				text << third_->name();
			} else {
				help::generate_format(text, third_->name(), "red", 0, false, false);
			}
			help::generate_format(text, "(", "blue", 0, false, false);
			text << third_->loyalty(*teams_[third_->side_].leader());
			help::generate_format(text, ")", "blue", 0, false, false);
		}
		text << "\n";

		if (!packed()) {
			text << _("troop^Type") << ": " << type_name() << "(Lv" << level() << ")";
		} else {
			text << _("troop^Type") << ": " << packee_unit_type_->type_name() << "(Lv" << level() << ")";
			text << "[" << type_name() << "]";
		}
		// adaptability
		text << "(" << hero::adaptability_str2(ftofxp12(adaptability_[arms()])) << ")" << "\n";

		help::tintegrate::generate_img(text, "misc/tintegrate-split-line.png");
		text << "\n";


		help::tintegrate::generate_img(text, "misc/leadership.png~SCALE(20, 20)");
		text << leadership_ << "   ";

		help::tintegrate::generate_img(text, "misc/force.png~SCALE(20, 20)");
		text << force_ << "   ";

		help::tintegrate::generate_img(text, "misc/intellect.png~SCALE(20, 20)");
		text << intellect_ << "\n";

		help::tintegrate::generate_img(text, "misc/politics.png~SCALE(20, 20)");
		text << politics_ << "   ";

		help::tintegrate::generate_img(text, "misc/charm.png~SCALE(20, 20)");
		text << charm_ << "\n";

		abilities_tt = ability_tooltips(true);
	} else if (this_is_city()) {
		const artifical& city = *const_unit_2_artifical(this);
		text << _("Name") << ": " << city.name() << "\n";

		text << _("Side") << ": " << teams_[city.side() - 1].name() << "\n";

		text << dgettext("wesnoth-lib", "Mayor") << ": ";
		if (city.mayor()->valid()) {
			text << city.mayor()->name() << "\n";
		} else {
			text << dgettext("wesnoth-lib", "Void") << "\n";
		}

		text << _("troop^Type") << ": " << type_name() << "(Lv" << level() << ")\n \n";

		abilities_tt = city.ability_tooltips(true);

	} else {
		const artifical& art = *const_unit_2_artifical(this);
		text << _("Name") << ": " << art.name() << "\n";

		artifical* owner_city = units_.city_from_cityno(art.cityno());
		if (owner_city) {
			text << _("City") << ": " << owner_city->master().name();
		} else {
			text << _("City") << ": --";
		}

		text << " (" << teams_[side_ - 1].name() << ")\n";

		text << _("troop^Type") << ": " << type_name() << "(Lv" << level() << ")\n \n";

		abilities_tt = art.ability_tooltips(true);
	}

	text << "HP: " << hitpoints() << "/";
	strstr.str("");
	strstr << max_hitpoints();
	// default font_size is font::SIZE_SMALL
	help::generate_format(text, strstr.str(), "", 0, false, false);
	text << "  ";

	text << "XP: " << experience() << "/";
	strstr.str("");
	strstr << max_experience();
	help::generate_format(text, strstr.str(), "", 0, false, false);
	text << "\n";

	if (this_is_city()) {
		text << dgettext("wesnoth-lib", "Front") << ": " << const_unit_2_artifical(this)->fronts() << "/" << mr_data::max_fronts << "\n";
	}

	// abilities
	text << _("Abilities") << ": ";
	if (!abilities_tt.empty()) {
		std::vector<t_string> abilities;
		for (std::vector<std::string>::const_iterator a = abilities_tt.begin(); a != abilities_tt.end(); a += 2) {
			abilities.push_back(*a);
		}

		for (std::vector<t_string>::const_iterator a = abilities.begin(); a != abilities.end(); a++) {
			if (a != abilities.begin()) {
				if (a - abilities.begin() != 2) {
					text << ", ";
				} else {
					text << "\n    ";
				}
			}
			text << (*a);
		}
	}
	text << "\n";

	// features
	text << dgettext("wesnoth-lib", "Feature") << ": ";
	int features = 0;
	for (int i = 0; i < HEROS_MAX_FEATURE; i ++) {
		if (unit_feature_val(i) == hero_feature_single_result) {
			if (features) {
				if (features == 8) {
					text << " ...";
					break;
				} else if (features % 2) {
					text << ", ";
				} else {
					text << "\n    ";
				}
			}
			features ++;
			text << master().feature_str(i);
		}
	}
	text << "\n";

	// tactic
	if (!artifical_) {
		text << dgettext("wesnoth-lib", "Tactic") << ": ";
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
			if (index == 1) {
				text << "\n    ";
			} else if (index != 0) {
				text << ", ";
			}
			text << t->name();
		}
		text << "\n";
	}

	if (!artifical_) {
		text << _("Munition turns") << ": " << keep_turns_ << "    ";
	}

	// AMLA
	int amla = 0;
	const modifications_map& unit_modifications = unit_types.modifications();
	for (std::vector<std::string>::const_iterator it = modifications_.begin(); it != modifications_.end(); ++ it) {
		if (unit_modifications.find(*it) != unit_modifications.end()) {
			amla ++;
		}
	}
	text << _("AMLA") << ": " << amla;
/*
	for (std::vector<tunit_tactic>::const_iterator it = tactics_.begin(); it != tactics_.end(); ++ it) {
		const tunit_tactic& t = *it;
		if (it == tactics_.begin()) {
			text << t.tactic_->name() << "(" << t.turn_ << ")" << "(" << t.part_ << ")";
		} else {
			text << ", " << t.tactic_->name() << "(" << t.turn_ << ")" << "(" << t.part_ << ")";
		}
	}
*/
	text << "\n";

	// skill
	text << dgettext("wesnoth-lib", "Skill") << ": ";
	text << hero::skill_str(hero_skill_encourage) << "(";

	int effect = tactic_effect(ttactic::ENCOURAGE, "");
	strstr.str("");
	strstr << hero::adaptability_str2(ftofxp12(skill_[hero_skill_encourage]));
	if (effect > 0) {
		help::generate_format(text, strstr.str(), "green", 0, false, false);
	} else if (effect < 0) {
		help::generate_format(text, strstr.str(), "red", 0, false, false);
	} else {
		text << hero::adaptability_str2(ftofxp12(skill_[hero_skill_encourage]));
	}

	text << ")";
	if (!artifical_) {
		text << "  ";
		text << hero::skill_str(hero_skill_demolish) << "(";
		
		effect = tactic_effect(ttactic::DEMOLISH, "");
		strstr.str("");
		strstr << hero::adaptability_str2(ftofxp12(skill_[hero_skill_demolish]));
		if (effect > 0) {
			help::generate_format(text, strstr.str(), "green", 0, false, false);
		} else if (effect < 0) {
			help::generate_format(text, strstr.str(), "red", 0, false, false);
		} else {
			text << hero::adaptability_str2(ftofxp12(skill_[hero_skill_demolish]));
		}		
		text << ")";
	}
	text << "\n";

	help::tintegrate::generate_img(text, "misc/tintegrate-split-line.png");
	text << "\n";
	
	// attack 
	for (std::vector<attack_type>::const_iterator at_it = attacks_.begin(); at_it != attacks_.end(); ++ at_it) {
		// see generate_report() in generate_report.cpp
		text << at_it->name() << " (" << gettext(at_it->type().c_str()) << ")\n";

		std::string accuracy = at_it->accuracy_parry_description();
		if(accuracy.empty() == false) {
			accuracy += " ";
		}

		text << "  ";
		int effect = tactic_effect(ttactic::ATTACK, at_it->id());
		strstr.str("");
		strstr << at_it->damage();
		if (effect > 0) {
			help::generate_format(text, strstr.str(), "green", 0, false, false);
		} else if (effect < 0) {
			help::generate_format(text, strstr.str(), "red", 0, false, false);
		} else {
			text << at_it->damage();
		}
		text << "-" << at_it->num_attacks()
			<< " " << accuracy << "-- " << _(at_it->range().c_str());

		std::string special = at_it->weapon_specials(true);
		if (!special.empty()) {
			text << "(" << special << ")";
		}
		text << "\n";
	}

	help::tintegrate::generate_img(text, "misc/resistance.png");
	effect = tactic_effect(ttactic::RESISTANCE, "");
	if (effect > 0) {
		help::tintegrate::generate_img(text, "misc/mini-increase.png", help::tintegrate::BACK);
	} else if (effect < 0) {
		help::tintegrate::generate_img(text, "misc/mini-decrease.png", help::tintegrate::BACK);
	}
	if (!artifical_) {
		help::tintegrate::generate_img(text, "misc/movement.png");
		if (max_movement_ > tactic_compare_movement_) {
			help::tintegrate::generate_img(text, "misc/mini-increase.png", help::tintegrate::BACK);
		} else if (max_movement_ < tactic_compare_movement_) {
			help::tintegrate::generate_img(text, "misc/mini-decrease.png", help::tintegrate::BACK);
		}
	}
	if (hide_turns_) {
		help::tintegrate::generate_img(text, "misc/hide.png");
	}
	text << "    ";

	if (invisible(loc_)) {
		help::tintegrate::generate_img(text, "misc/invisible.png");
	}
	if (get_state(unit::STATE_SLOWED)) {
		help::tintegrate::generate_img(text, "misc/slowed.png");
	}
	if (get_state(unit::STATE_BROKEN)) {
		help::tintegrate::generate_img(text, "misc/broken.png");
	}
	if (get_state(unit::STATE_POISONED)) {
		help::tintegrate::generate_img(text, "misc/poisoned.png");
	}
	if (get_state(unit::STATE_PETRIFIED)) {
		help::tintegrate::generate_img(text, "petrified.png");
	}

	return text.str();
}

std::string unit::form_tiny_tip() const
{
	std::stringstream strstr;

	help::tintegrate::generate_img(strstr, "misc/attack.png");
	int effect = tactic_effect(ttactic::ATTACK, "");
	if (effect > 0) {
		help::tintegrate::generate_img(strstr, "misc/mini-increase.png", help::tintegrate::BACK);
	} else if (effect < 0) {
		help::tintegrate::generate_img(strstr, "misc/mini-decrease.png", help::tintegrate::BACK);
	}
	strstr << " ";

	help::tintegrate::generate_img(strstr, "misc/resistance.png");
	effect = tactic_effect(ttactic::RESISTANCE, "");
	if (effect > 0) {
		help::tintegrate::generate_img(strstr, "misc/mini-increase.png", help::tintegrate::BACK);
	} else if (effect < 0) {
		help::tintegrate::generate_img(strstr, "misc/mini-decrease.png", help::tintegrate::BACK);
	}
	strstr << " ";

	help::tintegrate::generate_img(strstr, "misc/encourage.png");
	effect = tactic_effect(ttactic::ENCOURAGE, "");
	if (effect > 0) {
		help::tintegrate::generate_img(strstr, "misc/mini-increase.png", help::tintegrate::BACK);
	} else if (effect < 0) {
		help::tintegrate::generate_img(strstr, "misc/mini-decrease.png", help::tintegrate::BACK);
	}
	
	strstr << " ";
	help::tintegrate::generate_img(strstr, "misc/demolish.png");
	effect = tactic_effect(ttactic::DEMOLISH, "");
	if (effect > 0) {
		help::tintegrate::generate_img(strstr, "misc/mini-increase.png", help::tintegrate::BACK);
	} else if (effect < 0) {
		help::tintegrate::generate_img(strstr, "misc/mini-decrease.png", help::tintegrate::BACK);
	}

	if (!artifical_) {
		strstr << " ";
		help::tintegrate::generate_img(strstr, "misc/movement.png");
		if (max_movement_ > tactic_compare_movement_) {
			help::tintegrate::generate_img(strstr, "misc/mini-increase.png", help::tintegrate::BACK);
		} else if (max_movement_ < tactic_compare_movement_) {
			help::tintegrate::generate_img(strstr, "misc/mini-decrease.png", help::tintegrate::BACK);
		}
	}

	if (hide_turns_) {
		strstr << " ";
		help::tintegrate::generate_img(strstr, "misc/hide.png");
	}

	bool first_state_icon = true;
	
	if (invisible(loc_)) {
		if (first_state_icon) {
			strstr << "    ";
			first_state_icon = false;
		}
		help::tintegrate::generate_img(strstr, "misc/invisible.png");
	}
	if (get_state(unit::STATE_SLOWED)) {
		if (first_state_icon) {
			strstr << "    ";
			first_state_icon = false;
		}
		help::tintegrate::generate_img(strstr, "misc/slowed.png");
	}
	if (get_state(unit::STATE_BROKEN)) {
		if (first_state_icon) {
			strstr << "    ";
			first_state_icon = false;
		}
		help::tintegrate::generate_img(strstr, "misc/broken.png");
	}
	if (get_state(unit::STATE_POISONED)) {
		if (first_state_icon) {
			strstr << "    ";
			first_state_icon = false;
		}
		help::tintegrate::generate_img(strstr, "misc/poisoned.png");
	}
	if (get_state(unit::STATE_PETRIFIED)) {
		if (first_state_icon) {
			strstr << "    ";
			first_state_icon = false;
		}
		help::tintegrate::generate_img(strstr, "petrified.png");
	}
	return strstr.str();
}

const surface unit::still_image(bool scaled) const
{
	return surface(image::get_image(master_->image()));
	
}

void unit::set_standing(bool with_bars)
{
	game_display *disp = game_display::get_singleton();
	if (preferences::show_standing_animations()&& !incapacitated()) {
		start_animation(INT_MAX, choose_animation(*disp, loc_, "standing"),
			with_bars, true, "", 0, STATE_STANDING);
	} else {
		start_animation(INT_MAX, choose_animation(*disp, loc_, "_disabled_"),
			with_bars, true, "", 0, STATE_STANDING);
	}
}

void unit::set_ghosted(bool with_bars)
{
	game_display *disp = game_display::get_singleton();
	start_animation(INT_MAX, choose_animation(*disp, loc_, "ghosted"),
			with_bars, true);
}

void unit::set_disabled_ghosted(bool with_bars)
{
	game_display *disp = game_display::get_singleton();
	start_animation(INT_MAX, choose_animation(*disp, loc_, "disabled_ghosted"),
			with_bars, true);
}

void unit::set_idling()
{
	game_display *disp = game_display::get_singleton();
	start_animation(INT_MAX, choose_animation(*disp, loc_, "idling"),
		true, false, "", 0, STATE_FORGET);
}

void unit::set_selecting()
{
	const game_display *disp =  game_display::get_singleton();
	if (preferences::show_standing_animations() && !get_state(STATE_PETRIFIED)) {
		start_animation(INT_MAX, choose_animation(*disp, loc_, "selected"),
			true, false, "", 0, STATE_FORGET);
	} else {
		start_animation(INT_MAX, choose_animation(*disp, loc_, "_disabled_selected_"),
			true, false, "", 0, STATE_FORGET);
	}
}

void unit::start_animation(int start_time, const unit_animation *animation,
	bool with_bars, bool cycles, const std::string &text, Uint32 text_color, STATE state)
{
	const game_display * disp =  game_display::get_singleton();
	state_ = state;
	if (!animation) {
		if (state != STATE_STANDING)
			set_standing(with_bars);
		return ;
	}
	// everything except standing select and idle
	bool accelerate = (state != STATE_FORGET && state != STATE_STANDING);
	draw_bars_ =  with_bars;
	delete anim_;
	anim_ = new unit_animation(*animation);
	const int real_start_time = start_time == INT_MAX ? anim_->get_begin_time() : start_time;
	anim_->start_animation(real_start_time, loc_, loc_.get_direction(facing_),
		cycles, text, text_color, accelerate);
	frame_begin_time_ = anim_->get_begin_time() -1;

	// idle anim
	next_idling_ = get_current_animation_tick()
			+ static_cast<int>((20000 + rand() % 20000) * disp->idle_anim_rate());
}


void unit::set_facing(map_location::DIRECTION dir) {
	if(dir != map_location::NDIRECTIONS) {
		facing_ = dir;
	}
	// Else look at yourself (not available so continue to face the same direction)
}

#define BIT_2_MASK(bit)		(1 << (bit))

void unit::redraw_unit()
{
	game_display &disp = *game_display::get_singleton();
	const gamemap &map = disp.get_map();
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

	anim_->update_last_draw_time();
	frame_parameters params;
	const t_translation::t_terrain terrain = map.get_terrain(loc_);
	const terrain_type& terrain_info = map.get_terrain_info(terrain);
	// do not set to 0 so we can distinguish the flying from the "not on submerge terrain"
	// instead use -1.0 (as in "negative depth", it will be ignored by rendering)
	params.submerge= is_flying() ? -1.0 : terrain_info.unit_submerge();

	if (invisible(loc_) &&
			params.highlight_ratio > 0.5) {
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
	if (get_state(STATE_POISONED)) {
		params.blend_with = disp.rgb(0, 255, 0);
		params.blend_ratio = 0.25;
	} else if (temporary_state_ & BIT_2_MASK(BIT_STRONGER)) {
		params.blend_with = disp.rgb(255, 32, 32);
		params.blend_ratio = 0.40;
	}
	//hackish : see unit_frame::merge_parameters
	// we use image_mod on the primary image
	// and halo_mod on secondary images and all haloes
	params.image_mod = image_mods();
	params.halo_mod = TC_image_mods();
	params.image= absolute_image();


	if(get_state(STATE_PETRIFIED)) params.image_mod +="~GS()";
	params.primary_frame = t_true;

	const frame_parameters adjusted_params = anim_->get_current_params(params);



	const map_location dst = loc_.get_direction(facing_);
	const int xsrc = disp.get_location_x(loc_);
	const int ysrc = disp.get_location_y(loc_);
	const int xdst = disp.get_location_x(dst);
	const int ydst = disp.get_location_y(dst);
	int d2 = disp.hex_size() / 2;




	const int x = static_cast<int>(adjusted_params.offset * xdst + (1.0-adjusted_params.offset) * xsrc) + d2;
	const int y = static_cast<int>(adjusted_params.offset * ydst + (1.0-adjusted_params.offset) * ysrc) + d2;


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
	if(draw_bars && preferences::show_side_colors()) {
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

	if (ellipse_back != NULL) {
		//disp.drawing_buffer_add(display::LAYER_UNIT_BG, loc,
		disp.drawing_buffer_add(display::LAYER_UNIT_FIRST, loc_,
			xsrc, ysrc +adjusted_params.y-ellipse_floating, ellipse_back);
	}

	if (ellipse_front != NULL) {
		//disp.drawing_buffer_add(display::LAYER_UNIT_FG, loc,
		disp.drawing_buffer_add(display::LAYER_UNIT_FIRST, loc_,
			xsrc, ysrc +adjusted_params.y-ellipse_floating, ellipse_front);
	}
	if ((draw_desc_ || (temporary_state_ & BIT_2_MASK(BIT_ATTACKING)) || (temporary_state_ & BIT_2_MASK(BIT_DEFENDING))) && draw_bars) {
		const surface* orb_img = NULL;

		if (size_t(side()) != disp.viewing_team() + 1) {
			if (disp.team_valid() && disp.get_teams()[disp.viewing_team()].is_enemy(side())) {
				orb_img = &unit_map::enemy_orb_;
			} else {
				orb_img = &unit_map::ally_orb_;
			}
		} else {
			orb_img = &unit_map::moved_orb_;
			if (disp.playing_team() == disp.viewing_team() && !user_end_turn()) {
				if (!human()) {
					orb_img = &unit_map::self_orb_;
				} else if (movement_left() == total_movement()) {
					orb_img = &unit_map::unmoved_orb_;
				} else if (unit_can_action(*this)) {
					orb_img = &unit_map::partmoved_orb_;
				} else if (goto_.valid()) {
					orb_img = &unit_map::automatic_orb_;
				}
			}
		}
		if (*orb_img) {
			if (display::default_zoom_ == display::ZOOM_48) {
				disp.drawing_buffer_add(display::LAYER_UNIT_BAR, 
					loc_, xsrc + 61, ysrc - 9, *orb_img);
			} else if (display::default_zoom_ == display::ZOOM_56) {
				disp.drawing_buffer_add(display::LAYER_UNIT_BAR, 
					loc_, xsrc + 72, ysrc - 11, *orb_img);
			} else if (display::default_zoom_ == display::ZOOM_64) {
				disp.drawing_buffer_add(display::LAYER_UNIT_BAR, 
					loc_, xsrc + 82, ysrc - 13, *orb_img);
			} else {
				disp.drawing_buffer_add(display::LAYER_UNIT_BAR, 
					loc_, xsrc + 92, ysrc - 15, *orb_img);
			}
		}

		std::string* energy_file;
		if (display::default_zoom_ == display::ZOOM_48) {
			energy_file = &game_config::images::bar_hrl_48;
		} else if (display::default_zoom_ == display::ZOOM_56) {
			energy_file = &game_config::images::bar_hrl_56;
		} else if (display::default_zoom_ == display::ZOOM_64) {
			energy_file = &game_config::images::bar_hrl_64;
		} else {
			energy_file = &game_config::images::bar_hrl_72;
		}

		double unit_energy = 0.0;
		if (max_hitpoints() > 0) {
			unit_energy = double(hitpoints())/double(max_hitpoints());
		}
		// const int bar_shift = static_cast<int>(-5*disp.get_zoom_factor());
		const int hp_bar_height = static_cast<int>(max_hitpoints()*game_config::hp_bar_scaling);

		const fixed_t bar_alpha = (loc_ == disp.mouseover_hex() || loc_ == disp.selected_hex()) ? ftofxp(1.0): ftofxp(0.8);

		if (display::default_zoom_ == display::ZOOM_48) {
			disp.draw_bar(*energy_file, xsrc + 36, ysrc - 26,
				loc_, hp_bar_height, unit_energy, hp_color(), bar_alpha, false);
		} else if (display::default_zoom_ == display::ZOOM_56) {
			disp.draw_bar(*energy_file, xsrc + 42, ysrc - 30,
				loc_, hp_bar_height, unit_energy, hp_color(), bar_alpha, false);
		} else if (display::default_zoom_ == display::ZOOM_64) {
			disp.draw_bar(*energy_file, xsrc + 48, ysrc - 34,
				loc_, hp_bar_height, unit_energy, hp_color(), bar_alpha, false);
		} else {
			disp.draw_bar(*energy_file, xsrc + 55, ysrc - 39,
				loc_, hp_bar_height, unit_energy, hp_color(), bar_alpha, false);
		}

		if (can_advance()) {
			const double filled = double(experience())/double(max_experience());

			const int xp_bar_height = static_cast<int>(max_experience()*game_config::xp_bar_scaling / std::max<int>(level_,1));

			SDL_Color color = xp_color();
			if (display::default_zoom_ == display::ZOOM_48) {
				disp.draw_bar(*energy_file, xsrc + 36, ysrc - 21, 
					loc_, xp_bar_height, filled, color, bar_alpha, false);
			} else if (display::default_zoom_ == display::ZOOM_56) {
				disp.draw_bar(*energy_file, xsrc + 42, ysrc - 24, 
					loc_, xp_bar_height, filled, color, bar_alpha, false);
			} else if (display::default_zoom_ == display::ZOOM_64) {
				disp.draw_bar(*energy_file, xsrc + 48, ysrc - 27, 
					loc_, xp_bar_height, filled, color, bar_alpha, false);
			} else {
				disp.draw_bar(*energy_file, xsrc + 55, ysrc - 32, 
					loc_, xp_bar_height, filled, color, bar_alpha, false);
			}
		}

		if (!desc_rect_) {
			SDL_Rect dst = {0, 0, 0, 0};
			// desc_rect_.assign(create_neutral_surface(unit_map::desc_bg_[level_]->w, unit_map::desc_bg_[level_]->h));
			// unit desc frame
			desc_rect_ = make_neutral_surface(unit_map::desc_bg_[level_]);
/*			
			// unit name		
			// disp.draw_text_in_hex2(loc_, display::LAYER_MOVE_INFO, name(), 13, font::BIGMAP_COLOR, xsrc + 38, ysrc - 32);
			surface text_surf;
			surface back_surf;
			if (display::default_zoom_ == display::ZOOM_48) {
				text_surf = font::get_rendered_text(name(), 9, font::BIGMAP_COLOR);
				back_surf = font::get_rendered_text(name(), 9, font::BLACK_COLOR);
			} else if (display::default_zoom_ == display::ZOOM_56) {
				text_surf = font::get_rendered_text(name(), 10, font::BIGMAP_COLOR);
				back_surf = font::get_rendered_text(name(), 10, font::BLACK_COLOR);
			} else if (display::default_zoom_ == display::ZOOM_64) {
				text_surf = font::get_rendered_text(name(), 11, font::BIGMAP_COLOR);
				back_surf = font::get_rendered_text(name(), 11, font::BLACK_COLOR);
			} else {
				text_surf = font::get_rendered_text(name(), 13, font::BIGMAP_COLOR);
				back_surf = font::get_rendered_text(name(), 13, font::BLACK_COLOR);
			}

			for (int dy=-1; dy <= 1; ++dy) {
				for (int dx=-1; dx <= 1; ++dx) {
					if (dx!=0 || dy!=0) {
						if (game_config::tiny_gui) {
							dst.x = 1 + dx;
							dst.y = dy;
						} else {
							dst.x = 2 + dx;
							dst.y = 1 + dy;
						}
						sdl_blit(back_surf, NULL, desc_rect_, &dst);
						// drawing_buffer_add(layer, loc, x + dx, y + dy, back_surf);
					}
				}
			}
			if (game_config::tiny_gui) {
				dst.x = 1;
				dst.y = 0;
			} else {
				dst.x = 2;
				dst.y = 1;
			}
			sdl_blit(text_surf, NULL, desc_rect_, &dst);
			// drawing_buffer_add(layer, loc, x, y, text_surf);
*/
			// portrait
			if (small_portrait_ == NULL) {
				small_portrait_ = image::get_image(master_->image());
				SDL_Rect clip = {4, 5, 40, 50};
				if (display::default_zoom_ == display::ZOOM_48) {
					small_portrait_ = scale_surface(cut_surface(small_portrait_, clip), 16, 19);
				} else if (display::default_zoom_ == display::ZOOM_56) {
					small_portrait_ = scale_surface(cut_surface(small_portrait_, clip), 18, 23);
				} else if (display::default_zoom_ == display::ZOOM_64) {
					small_portrait_ = scale_surface(cut_surface(small_portrait_, clip), 21, 26);
				} else {
					small_portrait_ = scale_surface(cut_surface(small_portrait_, clip), 24, 30);
				}
				SDL_SetAlpha(small_portrait_.get(), 0, SDL_ALPHA_OPAQUE);
			}
			if (display::default_zoom_ == display::ZOOM_48) {
				dst.x = 2;
				dst.y = 2;
			} else if (display::default_zoom_ == display::ZOOM_56) {
				dst.x = 3;
				dst.y = 2;
			} else if (display::default_zoom_ == display::ZOOM_64) {
				dst.x = 3;
				dst.y = 2;
			} else {
				dst.x = 3;
				dst.y = 2;
			}
			sdl_blit(small_portrait_, NULL, desc_rect_, &dst);
		}

		if (display::default_zoom_ == display::ZOOM_48) {
			// disp.drawing_buffer_add(display::LAYER_MOUSEOVER_BOTTOM, loc_, xsrc + 24, ysrc - 10, desc_rect_);
			disp.drawing_buffer_add(display::LAYER_UNIT_LAST, loc_, xsrc + 24, ysrc - 10, desc_rect_);
		} else if (display::default_zoom_ == display::ZOOM_56) {
			// disp.drawing_buffer_add(display::LAYER_MOUSEOVER_BOTTOM, loc_, xsrc + 28, ysrc - 12, desc_rect_);
			disp.drawing_buffer_add(display::LAYER_UNIT_LAST, loc_, xsrc + 28, ysrc - 12, desc_rect_);
		} else if (display::default_zoom_ == display::ZOOM_64) {
			// disp.drawing_buffer_add(display::LAYER_MOUSEOVER_BOTTOM, loc_, xsrc + 32, ysrc - 14, desc_rect_);
			disp.drawing_buffer_add(display::LAYER_UNIT_LAST, loc_, xsrc + 32, ysrc - 14, desc_rect_);
		} else {
			// disp.drawing_buffer_add(display::LAYER_MOUSEOVER_BOTTOM, loc_, xsrc + 36, ysrc - 16, desc_rect_);
			disp.drawing_buffer_add(display::LAYER_UNIT_LAST, loc_, xsrc + 36, ysrc - 16, desc_rect_);
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


	game_display * disp =  game_display::get_singleton();
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

		result |= get_animation()->invalidate(params);
	}

	return result;

}

int unit::upkeep() const
{
	if (unit_feature_val(hero_feature_magnate)) {
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
	if (known_boolean_states_[STATE_LEGERITIED]) {
		return 1;
	}

	team& current_team = (*resources::teams)[side_ - 1];
	if (!loc || !current_team.has_avoid() || !current_team.avoid().match(*loc)) {
		assert(resources::game_map != NULL);
		const int res = movement_cost_internal(movement_costs_,
				cfg_, NULL, *resources::game_map, terrain);

		if (res == unit_movement_type::UNREACHABLE) {
			return res;
		} else if(get_state(STATE_SLOWED)) {
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
	if(!resistance_abilities.empty()) {
		unit_abilities::effect resist_effect(resistance_abilities,res,false);

		int new_res = std::min<int>(resist_effect.get_composite_value(),resistance_abilities.highest("max_value").first);
		if (new_res != res) {
			const team& t = (*resources::teams)[side_ - 1 ];
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
		foreach (const config::attribute &i, resistance.attribute_range()) {
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
		foreach (const std::string &s, advances_to())
		{
			if (!s.empty())
				tooltip << s << '\n';
		}
		temp[image] = tooltip.str();
	}

	foreach (const config &adv, get_modification_advances())
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

	foreach (const config &adv, get_modification_advances())
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
		foreach (const std::string &s, uniq)
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
	foreach (const config::attribute &i, mod.attribute_range()) {
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
		if (anim) {
			strstr.str("");
			help::tintegrate::generate_img(strstr, "misc/attack.png");
			if (increased) {
				help::tintegrate::generate_img(strstr, "misc/increase.png");
			}
			if (decreased) {
				help::tintegrate::generate_img(strstr, "misc/decrease.png");
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
	} else if(apply_to == apply_to_tag::MOVEMENT) {
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
		if (anim) {
			strstr.str("");
			help::tintegrate::generate_img(strstr, "misc/movement.png");
			if (increased) {
				help::tintegrate::generate_img(strstr, "misc/increase.png");
			}
			if (decreased) {
				help::tintegrate::generate_img(strstr, "misc/decrease.png");
			}
			unit_display::unit_text(*this, decreased, strstr.str());
		}
	} else if (apply_to == apply_to_tag::MUNITION) {
		int increase = effect["increase"].to_int();

		if (increase > 0 || (increase < 0 && keep_turns_)) {
			keep_turns_ += increase;
			if (keep_turns_ < 0) keep_turns_ = 0;
			if (anim) {
				std::stringstream str;
				str << _("Munition") << "\n";
				std::vector<unit*> touchers(1, this);
				unit_display::unit_touching(loc_, touchers, increase, str.str());
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

		if(add.empty() == false) {
			set_state(add, true);
		}

		if(remove.empty() == false) {
			set_state(remove, false);
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
			foreach (const config::attribute &i, ap.attribute_range()) {
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
				foreach (const config::attribute &i, ap.attribute_range()) {
					int v = i.second.to_int();
					if (v > 0) {
						increased = true;
					} else if (v < 0) {
						decreased = true;
					}
				}
			}
			mod_mdr_merge(mv, ap, !effect["replace"].to_bool(), true, 50, 500);
		}
		if (anim) {
			strstr.str("");
			help::tintegrate::generate_img(strstr, "misc/resistance.png");
			if (increased) {
				help::tintegrate::generate_img(strstr, "misc/increase.png");
			}
			if (decreased) {
				help::tintegrate::generate_img(strstr, "misc/decrease.png");
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
			help::tintegrate::generate_img(strstr, "misc/encourage.png");
			if (increase > 0) {
				decreased = false;
				help::tintegrate::generate_img(strstr, "misc/increase.png");
			} else {
				decreased = true;
				help::tintegrate::generate_img(strstr, "misc/decrease.png");
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
			help::tintegrate::generate_img(strstr, "misc/demolish.png");
			if (increase > 0) {
				decreased = false;
				help::tintegrate::generate_img(strstr, "misc/increase.png");
			} else {
				decreased = true;
				help::tintegrate::generate_img(strstr, "misc/decrease.png");
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
			dialogs::advance_unit(loc_, true);
			increase --;
		}

	} else if (apply_to == apply_to_tag::TRAIN) {
		// arms adaptability
		int increase = effect["increase"].to_int();
		if (increase) {
			int inc_adaptability_xp = increase * ftofxp12(1);

			u16_get_experience_i12(&(master_->arms_[arms_]), inc_adaptability_xp);
			if (second_->valid()) {
				u16_get_experience_i12(&(second_->arms_[arms_]), inc_adaptability_xp);
			}
			if (third_->valid()) {
				u16_get_experience_i12(&(third_->arms_[arms_]), inc_adaptability_xp);
			}
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
			adjust();
			if (anim) {
				std::stringstream str;
				str << dgettext("wesnoth-lib", "Adaptability") << "\n";
				std::vector<unit*> touchers;
				if (artifical* city = units_.city_from_loc(loc_)) {
					touchers.push_back((unit*)(city));
				} else {
					touchers.push_back(this);
				}
				unit_display::unit_touching(loc_, touchers, increase, str.str());
			}
		}
	} else if (apply_to == apply_to_tag::DAMAGE) {
		const std::string& type = effect["type"];
		const std::string& relative = effect["relative"];
		int ratio = effect["ratio"].to_int(100);
		damage_range(units_, *resources::teams, *this, type, relative, ratio);

	} else if (apply_to == apply_to_tag::HIDE) {
		if (hide_turns_ < turns) hide_turns_ = turns;
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

void unit::add_trait_description(const config& trait)
{
	const t_string& name = trait["male_name"];

	if(!name.empty()) {
		trait_names_.push_back(name);
	}
}

const unit_animation* unit::choose_animation(const game_display& disp, const map_location& loc,const std::string& event,
		const map_location& second_loc,const int value,const unit_animation::hit_type hit,
		const attack_type* attack, const attack_type* second_attack, int swing_num) const
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
	const std::vector<team>& teams = *resources::teams;

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

	unit_map::node* expediting_city_node = NULL;
	if (units_.expediting()) {
		expediting_city_node = dynamic_cast<unit_map::node*>(units_.expediting_city_node());
	}

	// Test hidden status
	bool is_inv = !get_state(STATE_UNCOVERED) && !artifical_;
	if (is_inv) {
		unit_map::node* curr_node = reinterpret_cast<unit_map::node*>(units_.get_cookie(loc, false));
		is_inv = !curr_node && get_ability_bool("hides", loc);
	}
	if (is_inv) {
		map_offset* adjacent_ptr;
		size_t i, size;
		map_location u_loc;

		// range=1
		size = (sizeof(adjacent_1) / sizeof(map_offset)) >> 1;
		adjacent_ptr = adjacent_1[loc.x & 0x1];
		for (i = 0; i < size; i ++) {
			u_loc.x = loc.x + adjacent_ptr[i].x;
			u_loc.y = loc.y + adjacent_ptr[i].y;
			
			// when expedite, center of city using unit_map::find is expediting troop, other is city.
			// because city is 7 grids, so range = 1 don't touch center grid.
			unit* u = find_unit(units_, u_loc);
			if (!u) {
				continue;
			}
			if (u->wall()) {
				continue;
			}
/*
			// don't palce adjacent = false outer of for loop!
			bool adjacent = false;
			if (!u->is_artifical() && (!expediting_city_node || expediting_city_node->first != u_loc)) {
				adjacent = tiles_adjacent(loc, u_loc);
			} else {
				const artifical* art;
				if (expediting_city_node && expediting_city_node->first == u_loc) {
					// when expediting, unit in units is expediting troop, it is necessary to city.
					art = const_unit_2_artifical(expediting_city_node->second);
				} else {
					art = const_unit_2_artifical(&*u);
				}
				for (size_t i = 0; i < art->adjacent_size_; i ++) {
					if (art->adjacent_[i] == loc) {
						adjacent = true;
						break;
					}
				}
			}
*/			
			bool adjacent = true;
			if (teams[side_-1].is_enemy(u->side()) && adjacent) {
				// Enemy spotted in adjacent tiles, check if we can see him.
				// Watch out to call invisible with see_all=true to avoid infinite recursive calls!
				if (see_all) {
					is_inv = false;
					break;
				} else if (!teams[side_-1].fogged(u_loc) && !u->invisible(u_loc, true)) {
					is_inv = false;
					break;
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
	std::vector<team>& teams_ = *resources::teams;
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
	std::vector<team>& teams_ = *resources::teams;
	hero& leader = *(teams_[side_ - 1].leader());

	if (master_->base_catalog_ != leader.base_catalog_) {
		master_->set_loyalty(leader, level, fixed);
	} else {
		master_->float_catalog_ = ftofxp8(master_->base_catalog_);
	}
	if (second_->valid()) {
		if (second_->base_catalog_ != leader.base_catalog_) {
			second_->set_loyalty(leader, level, fixed);
		} else {
			second_->float_catalog_ = ftofxp8(second_->base_catalog_);
		}
	}
	if (third_->valid()) {
		if (third_->base_catalog_ != leader.base_catalog_) {
			third_->set_loyalty(leader, level, fixed);
		} else {
			third_->float_catalog_ = ftofxp8(third_->base_catalog_);
		}
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

void unit::increase_feeling(int inc)
{
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
		unit* u = find_unit(units_, heros_[*i]);
		// The descented hero may be unstage.
		if (u && !u->is_artifical() && std::find(adjusted.begin(), adjusted.end(), u) == adjusted.end()) {
			u->adjust();
			adjusted.push_back(u);
		}
	}
}

void unit::increase_activity(int inc, bool adjusting)
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
	for (unit_map::const_iterator u = resources::units->begin(); u != resources::units->end(); ++ u) {
		if (u->side() == side) ++res;
	}
	return res;
}

int side_units_cost(int side)
{
	int res = 0;
	for (unit_map::const_iterator u = resources::units->begin(); u != resources::units->end(); ++ u) {
		if (u->side() == side) res += u->cost();
	}
	return res;
}

int side_upkeep(int side)
{
	int res = 0;
	for (unit_map::const_iterator u = resources::units->begin(); u != resources::units->end(); ++ u) {
		if (u->side() == side) res += u->upkeep();
	}
	return res;
}

unit_map::iterator find_visible_unit(const map_location &loc,
	const team& current_team, bool see_all)
{
	unit_map& units = *resources::units;
	if (!resources::game_map->on_board(loc)) return units.end();
	unit_map::iterator u = units.find(loc);
	if (!u.valid()) {
		u = units.find(loc, false);
	}
	if (see_all) return u;
	if (!u.valid() || current_team.fogged(loc) || (current_team.is_enemy(u->side()) && u->invisible(loc)))
		return units.end();
	return u;
}

unit *get_visible_unit(const map_location &loc,
	const team &current_team, bool see_all)
{
	unit_map::iterator ui = find_visible_unit(loc,
		current_team, see_all);
	if (ui == resources::units->end()) return NULL;
	return &*ui;
}

void unit::refresh()
{
	if (state_ == STATE_FORGET && anim_ && anim_->animation_finished_potential())
	{
		set_standing();
		return;
	}
	game_display &disp = *game_display::get_singleton();
	if (state_ != STATE_STANDING || get_current_animation_tick() < next_idling_ ||
	    !disp.tile_nearly_on_screen(loc_) || incapacitated())
	{
		return;
	}
	if (get_current_animation_tick() > next_idling_ + 1000)
	{
		// prevent all units animating at the same time
		next_idling_ = get_current_animation_tick()
			+ static_cast<int>((10000 + rand() % 10000) * disp.idle_anim_rate());
	} else {
		set_idling();
	}
}

team_data calculate_team_data(const team& tm, int side)
{
	team_data res;
	res.units = side_units(side);
	// res.upkeep = side_upkeep(units, side);
	res.upkeep = tm.side_upkeep();
	res.villages = tm.villages().size();
	res.net_income = tm.total_income() - res.upkeep;
	res.gold = tm.gold();
	res.technology_net_income = tm.total_technology_income();
	res.teamname = tm.user_team_name();
	return res;
}

std::pair<map_location, unit*> temporary_unit_placer::aggressing_;

temporary_unit_placer::temporary_unit_placer(unit_map& m, const map_location& loc, unit* u)
	: m_(m), loc_(loc), temp_(m.extract(loc))
{
	aggressing_.first = loc;
	aggressing_.second = u;

	m.place(&aggressing_);
}

temporary_unit_placer::~temporary_unit_placer()
{
	m_.extract(loc_);
	if (temp_) {
		m_.place(temp_);
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
	if (movement_left() == total_movement()) {
		set_state(STATE_NOT_MOVED,true);
	}
	set_movement(0);
}


void unit::set_hidden(bool state) {
	hidden_ = state;
	if(!state) return;
	// We need to get rid of haloes immediately to avoid display glitches
	clear_haloes();
}

std::set<map_location> unit::get_touch_locations(const gamemap& map, const map_location& loc) const
{
	std::set<map_location> touch_locs;

	touch_locs.insert(loc);
	for (std::set<map_location::DIRECTION>::const_iterator it = touch_dirs_.begin(); it != touch_dirs_.end(); ++ it) {
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

void unit::set_location(const map_location &loc) 
{ 
	const gamemap& map = *resources::game_map;

	if (loc_ == loc) {
		return;
	}
	// This is centor location
	loc_ = loc;

	touch_locs_.clear();
	adjacent_size_ = 0;
	adjacent_size_2_ = 0;
	adjacent_size_3_ = 0;

	// Centor location must be in touch locations for all unit.
	touch_locs_.insert(loc_);
	
	if (!this_is_city()) {
		map_offset* adjacent_ptr;
		size_t i, size;

		// range=1
		size = (sizeof(adjacent_1) / sizeof(map_offset)) >> 1;
		adjacent_ptr = adjacent_1[loc.x & 0x1];
		for (i = 0; i < size; i ++) {
			adjacent_[adjacent_size_].x = loc.x + adjacent_ptr[i].x;
			adjacent_[adjacent_size_].y = loc.y + adjacent_ptr[i].y;
			if (map.on_board(adjacent_[adjacent_size_])) {
				adjacent_size_ ++;
			}
		}
		// range=2
		size = (sizeof(adjacent_2) / sizeof(map_offset)) >> 1;
		adjacent_ptr = adjacent_2[loc.x & 0x1];
		for (i = 0; i < size; i ++) {
			adjacent_2_[adjacent_size_2_].x = loc.x + adjacent_ptr[i].x;
			adjacent_2_[adjacent_size_2_].y = loc.y + adjacent_ptr[i].y;
			if (map.on_board(adjacent_2_[adjacent_size_2_])) {
				adjacent_size_2_ ++;
			}
		}
		// range=3
		size = (sizeof(adjacent_3) / sizeof(map_offset)) >> 1;
		adjacent_ptr = adjacent_3[loc.x & 0x1];
		for (i = 0; i < size; i ++) {
			adjacent_3_[adjacent_size_3_].x = loc.x + adjacent_ptr[i].x;
			adjacent_3_[adjacent_size_3_].y = loc.y + adjacent_ptr[i].y;
			if (map.on_board(adjacent_3_[adjacent_size_3_])) {
				adjacent_size_3_ ++;
			}
		}

		if (!ignore_pack_) {
			// pack/unpack current unit if necessary
			t_translation::t_terrain terrain = map.get_terrain(loc_);
			// t_translation::t_terrain wo = t_translation::DEEP_WATER;
			bool match = t_translation::terrain_matches(terrain, t_translation::DEEP_WATER);
			if (match) {
				if (!packed()) {
					// pack!
					// std::string packer;
					int navigation = (*resources::teams)[side_ - 1].navigation();
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
	}
}

void unit::set_goto(const map_location& new_goto) 
{ 
	goto_ = new_goto; 
}

void unit::replace_captains(const std::vector<hero*>& captains)
{
	if (master_ != captains[0]) {
		name_ = captains[0]->name();
		small_portrait_.assign(NULL);
		desc_rect_.assign(NULL);
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

	calculate_5fields();
	bool unit_is_packed = packed();
	const unit_type* current_type = type();
	if (unit_is_packed) {
		pack_to(current_type);
	} else {
		max_hit_points_ = current_type->hitpoints();
		max_experience_ = current_type->experience_needed(false);
		max_movement_ = current_type->movement();
		attacks_ = current_type->attacks();
		modify_according_to_hero(false, false);
	}
	if (hit_points_ > max_hit_points_) {
		hit_points_ = max_hit_points_;
	}
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
	artifical* from = units_.city_from_cityno(cityno_);

	if (master_->official_ == hero_official_mayor) {
		from->select_mayor(&hero_invalid);
	}
	master_->official_ = HEROS_NO_OFFICIAL;
	master_->status_ = hero_status_unstage;
	master_->city_ = HEROS_DEFAULT_CITY;
	master_->side_ = HEROS_INVALID_SIDE;
	if (second_->valid()) {
		if (second_->official_ == hero_official_mayor) {
			from->select_mayor(&hero_invalid);
		}
		master_->official_ = HEROS_NO_OFFICIAL;
		second_->status_ = hero_status_unstage;
		second_->city_ = HEROS_DEFAULT_CITY;
		second_->side_ = HEROS_INVALID_SIDE;
	}
	if (third_->valid()) {
		if (third_->official_ == hero_official_mayor) {
			from->select_mayor(&hero_invalid);
		}
		third_->official_ = HEROS_NO_OFFICIAL;
		third_->status_ = hero_status_unstage;
		third_->city_ = HEROS_DEFAULT_CITY;
		third_->side_ = HEROS_INVALID_SIDE;
	}
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

unit* find_unit(unit_map& units, const hero& h)
{
	artifical* city = units.city_from_cityno(h.city_);
	if (!city) {
		if (h.status_ != hero_status_military) {
			// unstage hero
			return NULL;
		} else {
			std::vector<team>& teams = *resources::teams;
			team& t = teams[h.side_];
			const std::pair<unit**, size_t> p = t.field_troop();
			unit** troops = p.first;
			size_t troops_vsize = p.second;
			for (size_t i = 0; i < troops_vsize; i ++) {
				unit& u = *troops[i];
				if (u.master().number_ == h.number_) {
					return &u;
				}
				if (u.second().valid() && u.second().number_ == h.number_) {
					return &u;
				} 
				if (u.third().valid() && u.third().number_ == h.number_) {
					return &u;
				}
			}
		}
	}
	if (city->master().number_ == h.number_) {
		return (unit*)city;
	}

	if (h.status_ == hero_status_military) {
		// in troop
		std::vector<unit*>& reside_troops = city->reside_troops();
		for (std::vector<unit*>::iterator itor = reside_troops.begin(); itor != reside_troops.end(); ++ itor) {
			unit& u = **itor;
			if (u.master().number_ == h.number_) {
				return &u;
			}
			if (u.second().valid() && u.second().number_ == h.number_) {
				return &u;
			} 
			if (u.third().valid() && u.third().number_ == h.number_) {
				return &u;
			}
		}
		std::vector<unit*>& field_troops = city->field_troops();
		for (std::vector<unit*>::iterator itor = field_troops.begin(); itor != field_troops.end(); ++ itor) {
			unit& u = **itor;
			if (u.master().number_ == h.number_) {
				return &u;
			}
			if (u.second().valid() && u.second().number_ == h.number_) {
				return &u;
			} 
			if (u.third().valid() && u.third().number_ == h.number_) {
				return &u;
			}
		}
	}
	return (unit*)city;
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
				units.erase(&u);
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

unit* find_unit(unit_map& units, const map_location& loc)
{
	if (!resources::game_map->on_board(loc)) return NULL;
	unit_map::iterator u = units.find(loc);
	if (!u.valid()) {
		u = units.find(loc, false);
	}
	if (u.valid()) {
		return &*u;
	}
	return NULL;
}

const t_string& unit::name() const
{
	if (name_.empty()) {
		name_ = master_->name();
	}
	return name_;
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
		symbols["first"] = h1.name();
		symbols["second"] = h2.name();
		int incident = game_events::INCIDENT_ENCOURAGECONSORT;
		if (h1.is_oath(h2)) {
			if (h1.gender_ == hero_gender_male) {
				incident = game_events::INCIDENT_MALEOATH;
			} else {
				incident = game_events::INCIDENT_FEMALEOATH;
			}
		}
		game_events::show_hero_message(&h1, NULL, vgettext("$second! let improve morale, prepare to better battle.", symbols), incident);
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