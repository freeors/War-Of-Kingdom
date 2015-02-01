/* $Id: team.cpp 47841 2010-12-05 18:10:04Z mordante $ */
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
 *  Team-management, allies, setup at start of scenario.
 */

#include "team.hpp"

#include "ai/manager.hpp"
#include "game_events.hpp"
#include "gamestatus.hpp"
#include "resources.hpp"
#include "game_preferences.hpp"
#include "artifical.hpp"
#include "card.hpp"
#include "unit_display.hpp"
#include "serialization/parser.hpp"
#include "wml_exception.hpp"
#include "formula_string_utils.hpp"
#include "play_controller.hpp"
#include "gettext.hpp"
#include "integrate.hpp"

#include <boost/foreach.hpp>

static std::vector<team> *&teams = resources::teams;

//static member initialization
const int team::default_team_gold = 100;

int team::empty_side = -1;

static strategy null_strategy(0, strategy::NONE, 0);

int strategy::default_impletement_turns = 10;
strategy::strategy(int target, int type, int impletement_turns)
	: target_(target)
	, type_(type)
	, allies_()
	, start_turn_(0)
	, impletement_turns_(impletement_turns)
{
}

bool strategy::operator==(const strategy& that)
{
	return target_ == that.target_;
}

const std::vector<team>& teams_manager::get_teams()
{
	assert(teams);
	return *teams;
}

team::team_info::team_info() :
	name(),
	gold(0),
	start_gold(0),
	gold_add(false),
	income(0),
	income_per_village(0),
	save_id(),
	current_player(),
	countdown_time(),
	action_bonus_count(0),
	flag(),
	flag_icon(),
	description(),
	objectives(),
	objectives_changed(false),
	controller(),
	share_maps(false),
	share_view(false),
	disallow_observers(false),
	allow_player(false),
	music(),
	color(),
	side(0)
{
}

void team::team_info::read(const config &cfg)
{
	name = cfg["name"].str();
	gold = cfg["gold"];
	income = cfg["income"];
	save_id = cfg["save_id"].str();
	current_player = cfg["current_player"].str();
	countdown_time = cfg["countdown_time"].str();
	action_bonus_count = cfg["action_bonus_count"];
	flag = cfg["flag"].str();
	flag_icon = cfg["flag_icon"].str();
	description = cfg["id"].str();
	objectives = cfg["objectives"];
	objectives_changed = cfg["objectives_changed"].to_bool();
	disallow_observers = cfg["disallow_observers"].to_bool();
	allow_player = cfg["allow_player"].to_bool(true);
	music = cfg["music"].str();
	side = cfg["side"].to_int(1);
	color = cfg["color"].str();

	// [ai]
	ai::manager::add_ai_for_side_from_config(side, cfg, true);

	// at the start of a scenario "start_gold" is not set, we need to take the
	// value from the gold setting (or fall back to the gold default)
	if (!cfg["start_gold"].empty())
		start_gold = cfg["start_gold"];
	else if (!cfg["gold"].empty())
		start_gold = gold;
	else
		start_gold = default_team_gold;

	if(save_id.empty()) {
		save_id = description;
	}

	const std::string temp_rgb_str = cfg["team_rgb"];
	std::map<std::string, color_range>::iterator global_rgb = game_config::team_rgb_range.find(cfg["side"]);

	if(!temp_rgb_str.empty()){
		std::vector<Uint32> temp_rgb = string2rgb(temp_rgb_str);
		team_color_range_[side] = color_range(temp_rgb);
	}else if(global_rgb != game_config::team_rgb_range.end()){
		team_color_range_[side] = global_rgb->second;
	}

	income_per_village = cfg["village_gold"].to_int(game_config::village_income);

	std::string control = cfg["controller"];
	//by default, persistence of a team is set depending on the controller
	if (control == "human") {
		controller = controller_tag::HUMAN;
	} else if (control == "human_ai") {
		controller = controller_tag::HUMAN_AI;
	} else if (control == "network") {
		controller = controller_tag::NETWORK;
	} else if (control == "null") {
		disallow_observers = cfg["disallow_observers"].to_bool(true);
		controller = controller_tag::EMPTY;
		if (team::empty_side == -1) {
			// when RPG, indepdence side is empty also.
			team::empty_side = side;
		}
	} else {
		controller = controller_tag::AI;
	}

	//========================================================
	// END OF MESSY CODE

	// Share_view and share_maps can't both be enabled,
	// so share_view overrides share_maps.
	share_view = cfg["share_view"].to_bool();
	share_maps = !share_view && cfg["share_maps"].to_bool(true);
}

char const *team::team_info::controller_string() const
{
	const std::string& id = controller_tag::rfind(controller);
	if (!id.empty()) {
		return id.c_str();
	}
	return NULL;
}

void team::team_info::write(config& cfg) const
{
	cfg["gold"] = gold;
	cfg["start_gold"] = start_gold;
	cfg["gold_add"] = gold_add;
	cfg["income"] = income;
	cfg["name"] = name;
	cfg["save_id"] = save_id;
	cfg["current_player"] = current_player;
	cfg["flag"] = flag;
	cfg["flag_icon"] = flag_icon;
	cfg["id"] = description;
	cfg["objectives"] = objectives;
	cfg["objectives_changed"] = objectives_changed;
	cfg["countdown_time"]= countdown_time;
	cfg["action_bonus_count"]= action_bonus_count;
	cfg["village_gold"] = income_per_village;
	cfg["disallow_observers"] = disallow_observers;
	cfg["allow_player"] = allow_player;
	cfg["controller"] = controller_string();

	cfg["share_maps"] = share_maps;
	cfg["share_view"] = share_view;

	if (music.empty() == false) {
		cfg["music"] = music;
	}

	cfg["color"] = color;

	cfg.add_child("ai", ai::manager::to_config(side));
}

team_::team_()
	: leadership_speed_(100)
	, force_speed_(100)
	, intellect_speed_(100)
	, spirit_speed_(100)
	, charm_speed_(100)
	, ignore_zoc_on_wall_(false)
	, land_enemy_wall_(false)
	, tactic_degree_increase(100)
	, ticks_increase(100)
	, interlink_increase_(100)
	, cooperate_increase_(100)
	, navigation_civi_increase_(100)
	, repair_increase_(100)
	, village_gold_increase_(100)
	, market_increase_(100)
	, technology_increase_(100)
	, previous_turn(0)
	, cause_damage_(0)
	, been_damage_(0)
	, defeat_units_(0)
	, perfect_turns_(0)
	, defeat_units_one_turn_(0)
	, restrict_movement(false)
	, auto_recruit(false)
	, auto_move_human(false)
	, support_bomb(false)
	, kill_income(false)
	, max_troops(0)
	, ea_artifical_neutral(false)
	, double_reset_goto(false)
	, allow_intervene(false)
	, allow_active(false)
	, stratagem_ally_join(false)
	, stratagem_decrease_cost(false)
	, stratagem_half_kill(false)
	, stratagem_half_heal(false)
	, stratagem_baffle_fightback(false)
	, stratagem_baffle_tactic(false)
	, can_build_()
	, last_active_tactic_()
{
	for (int i = 0; i < sizeof(arms_speed_) / sizeof(arms_speed_[0]); i ++) {
		arms_speed_[i] = 100;
	}
	memset(&bh_eval, 0, sizeof(bh_eval));
}

void team_::increase_defeat_units_one_turn(int inc)
{
	if (!inc) {
		return;
	}
	VALIDATE(inc > 0, "team_::increase_defeat_units_one_turn, inc must great than 0!");

	int original = defeat_units_one_turn_;
	defeat_units_one_turn_ += inc;

	const int perfect_min_units = tent::tower_mode()? 5: 10;
	if (original >= perfect_min_units || defeat_units_one_turn_ < perfect_min_units) {
		return;
	}
	perfect_turns_ ++;

	unit_display::perfect_anim();
}

void team::merge_shroud_map_data(const std::string& shroud_data)
{
	shroud_.merge(shroud_data);
}

team::team(unit_map& units, hero_map& heros, card_map& cards, const config& cfg, const gamemap& map, int gold, size_t team_size) :
	units_(units),
	heros_(heros),
	cards_(cards),
	capital_(NULL),
	gold_(gold),
	navigation_(0),
	bomb_turns_(0),
	shroud_(),
	fog_(),
	auto_shroud_updates_(true),
	info_(),
	countdown_time_(0),
	action_bonus_count_(0),
	diplomatisms_(),
	strategies_(),
	features_(),
	seen_(),
	ally_shroud_(),
	ally_fog_(),
	holden_cities_(),
	character_(),
	candidate_cards_(),
	holded_cards_(),
	holded_treasures_(),
	holded_technologies_(),
	half_technologies_(),
	ing_technology_(NULL),
	appointed_nobles_(),
	unappoint_nobles_(),
	avoid_cfg_(cfg.child("avoid")? cfg.child("avoid"): config()),
	avoid_(terrain_filter(vconfig(avoid_cfg_), units_)),
	has_avoid_(cfg.child("avoid")? true: false),
	commercials_(),
	skill_feature_(false),
	w_(map.w()),
	h_(map.h())
{
	gold_ = gold;
	info_.read(cfg);

	std::stringstream err;

	// build
	std::vector<std::string> vstr = utils::split(cfg["build"]);
	bool keep_existed = false, wall_existed = false;
	std::set<const unit_type*> can_build_ea;
	for(std::vector<std::string>::const_iterator i = vstr.begin(); i != vstr.end(); ++i) {
		const unit_type* ut = unit_types.find(*i);
		if (ut) {
			if (ut == unit_types.find_keep()) {
				keep_existed = true;
			} else if (ut == unit_types.find_wall()) {
				wall_existed = true;
			} else if (ut == unit_types.find_market() || ut == unit_types.find_technology() || ut == unit_types.find_tactic()) {
				can_build_ea.insert(ut);
			}
			can_build_.insert(ut);
		}
	}
	if (can_build_ea.size() == 1 && *can_build_ea.begin() == unit_types.find_tactic()) {
		throw game::game_error("invalid build attribute in side(" + str_cast(info_.side) + ") tag, ea must not tactic only!");
	}

	// ensure both wall and keep are exist or not.
	if (!tent::tower_mode()) {
		if ((keep_existed && !wall_existed) || (!keep_existed && wall_existed)) {
			if (!keep_existed) {
				can_build_.insert(unit_types.find_keep());
			}
			if (!wall_existed) {
				can_build_.insert(unit_types.find_wall());
			}
		}
	} else {
		if (tent::mode == mode_tag::TOWER && can_build_.size() > 1) {
			throw game::game_error("invalid build attribute in side(" + str_cast(info_.side) + ") tag");
		} else if (can_build_.size() == 1 && *can_build_.begin() != unit_types.find_wall()) {
			throw game::game_error("invalid build attribute in side(" + str_cast(info_.side) + ") tag, tower mode can build wall only!");
		}
	}

	vstr = utils::parenthetical_split(cfg["feature"]);
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		const std::vector<std::string> vstr1 = utils::split(*it);
		arms_feature feature;
		if (vstr1.size() != 3) {
			if (vstr1.empty() || (vstr1.size() == 1 && vstr1[0] == ",")) {
				continue;
			} else {
				throw game::game_error("invalid feature attribute in side(" + str_cast(info_.side) + ") tag");
			}
		}
		feature.arms_ = unit_types.arms_from_id(vstr1[0]);
		if (feature.arms_ < 0 || feature.arms_ >= HEROS_MAX_ARMS) {
			throw game::game_error("invalid feature attribute in side(" + str_cast(info_.side) + ") tag");
		}
		feature.level_ = lexical_cast<int>(vstr1[1].c_str());
		if (feature.level_ < 0) {
			throw game::game_error("invalid feature attribute in side(" + str_cast(info_.side) + ") tag");
		}
		feature.feature_ = lexical_cast<int>(vstr1[2].c_str());
		if (feature.feature_ < 0 || feature.feature_ >= HEROS_MAX_FEATURE) {
			throw game::game_error("invalid feature attribute in side(" + str_cast(info_.side) + ") tag");
		}
		features_.push_back(feature);
	}

	// leader
	std::string str = cfg["leader"];
	if (str.empty() && info_.controller != controller_tag::EMPTY) {
		throw game::game_error("hasn't leader attribute in side(" + str_cast(info_.side) + ") tag");
	}
	if (!str.empty()) {
		leader_ = cfg["leader"].to_int();
	} else {
		leader_ = hero::number_empty_leader;
	}
	if (info_.controller != controller_tag::EMPTY) {
		// for some setting, empty leader maybe is general hero in other side.
		// in future, event result leader into this empty team, and change controller mode.
		heros_[leader_].official_ = hero_official_leader;
	}
	if (info_.name.empty()) {
		info_.name = heros_[leader_].name();
	}
	bh_eval.capital = cfg["capital"].to_int(HEROS_INVALID_NUMBER);
	
	// team_name
	str = cfg["team_name"].str();
	calculate_diplomatisms(str, team_size);
	if (!diplomatisms_[info_.side].ally_) {
		diplomatisms_[info_.side].ally_ = true;
	}
	// navigation
	str = cfg["navigation"].str();
	if (!str.empty() && str.at(0) == '$') {
		str = resources::state_of_game->get_variable(str.substr(1)).str();
	}
	navigation_ = atoi(str.c_str());

	// bomb_turns
	bomb_turns_ = cfg["bomb_turns"].to_int();

	// fog
	fog_.set_enabled(false);
	// shroud
	shroud_.set_enabled(false);
	shroud_.read(cfg["shroud_data"]);
	// candidate_cards
	str = cfg["candidate_cards"].str();
	if (!str.empty() && str.at(0) == '$') {
		str = resources::state_of_game->get_variable(str.substr(1)).str();
	}
	std::vector<std::pair<int, int> > ranges = utils::parse_ranges(str);
	for (std::vector<std::pair<int, int> >::const_iterator r = ranges.begin(); r != ranges.end(); ++ r) {
		for (int i = r->first; i <= r->second; i ++) {
			candidate_cards_.push_back(i);			
		}
	}
	// holded_cards
	str = cfg["holded_cards"].str();
	if (!str.empty() && str.at(0) == '$') {
		str = resources::state_of_game->get_variable(str.substr(1)).str();
	}
	std::vector<std::string> v_size = utils::split(str);
	for (std::vector<std::string>::const_iterator i = v_size.begin(); i != v_size.end(); ++ i) {
		holded_cards_.push_back(atoi(i->c_str()));
	}
	// treasures
	vstr = utils::split(cfg["treasure"]);
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		int number = lexical_cast_default<int>(*it);
		holded_treasures_.push_back(number);
	}
	// technologies
	vstr = utils::split(cfg["technologies"]);
	const std::map<std::string, ttechnology>& technologies = unit_types.technologies();
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		std::map<std::string, ttechnology>::const_iterator find = technologies.find(*it);
		if (find != technologies.end()) {
			holded_technologies_.push_back(&find->second);
		} else {
			err.str("");
			err << "Invalid technology: " << *it << "in side #" << info_.side << " tag!";
			throw game::game_error(err.str());
		}
	}
	// commercials
	str = cfg["commercials"].str();
	v_size = utils::split(str);
	for (std::vector<std::string>::const_iterator i = v_size.begin(); i != v_size.end(); ++ i) {
		commercials_.push_back(&heros_[atoi(i->c_str())]);
	}
	calculate_skill_feature();

	// To ensure some minimum starting gold,
	// gold is the maximum of 'gold' and what is given in the config file
	gold_ = std::max(gold, info_.gold);
	if (gold_ != info_.gold)
		info_.start_gold = gold;

	// Load in the villages the side controls at the start
	BOOST_FOREACH (const config &v, cfg.child_range("village"))
	{
		map_location loc(v, resources::state_of_game);
		if (map.is_village(loc)) {
		} else {
		}
	}

	countdown_time_ = cfg["countdown_time"];
	action_bonus_count_ = cfg["action_bonus_count"];

	// rectange of holded cities
	city_rect_.x = city_rect_.y = -1;
	city_rect_.w = city_rect_.h = 0;

	field_troops_ = (unit**)malloc(w_ * h_ * sizeof(unit*));
	field_troops_vsize_ = 0;

	apply_holded_technologies_finish();
}

team::team(unit_map& units, hero_map& heros, card_map& cards, const uint8_t* mem, const gamemap& map, int gold, size_t team_size) :
	units_(units),
	heros_(heros),
	cards_(cards),
	capital_(NULL),
	gold_(gold),
	navigation_(0),
	bomb_turns_(0),
	shroud_(),
	fog_(),
	auto_shroud_updates_(true),
	info_(),
	countdown_time_(0),
	action_bonus_count_(0),
	diplomatisms_(),
	strategies_(),
	features_(),
	seen_(),
	ally_shroud_(),
	ally_fog_(),
	holden_cities_(),
	character_(),
	candidate_cards_(),
	holded_cards_(),
	holded_treasures_(),
	holded_technologies_(),
	half_technologies_(),
	ing_technology_(NULL),
	appointed_nobles_(),
	unappoint_nobles_(),
	avoid_cfg_(),
	avoid_(terrain_filter(vconfig(config()), units)),
	has_avoid_(false),
	commercials_(),
	skill_feature_(false),
	w_(map.w()),
	h_(map.h())
{
	read(mem, map, team_size);
	
	gold_ = std::max(gold, info_.gold);
	if (gold_ != info_.gold) {
		info_.start_gold = gold;
	}

	// rectange of holded cities
	city_rect_.x = city_rect_.y = -1;
	city_rect_.w = city_rect_.h = 0;

	field_troops_ = (unit**)malloc(w_ * h_ * sizeof(unit*));
	field_troops_vsize_ = 0;

	apply_holded_technologies_finish();
}

team::team(const team& that) :
	team_(that),
	units_(that.units_),
	heros_(that.heros_),
	cards_(that.cards_),
	leader_(that.leader_),
	capital_(that.capital_),
	gold_(that.gold_),
	holden_cities_(that.holden_cities_),
	character_(that.character_),
	candidate_cards_(that.candidate_cards_),
	holded_cards_(that.holded_cards_),
	holded_treasures_(that.holded_treasures_),
	holded_technologies_(that.holded_technologies_),
	half_technologies_(that.half_technologies_),
	ing_technology_(that.ing_technology_),
	appointed_nobles_(that.appointed_nobles_),
	unappoint_nobles_(that.unappoint_nobles_),
	navigation_(that.navigation_),
	bomb_turns_(that.bomb_turns_),
	shroud_(that.shroud_),
	fog_(that.fog_),
	auto_shroud_updates_(that.auto_shroud_updates_),
	info_(that.info_),
	countdown_time_(that.countdown_time_),
	action_bonus_count_(that.action_bonus_count_),
	diplomatisms_(that.diplomatisms_),
	strategies_(that.strategies_),
	features_(that.features_),
	seen_(that.seen_),
	ally_shroud_(that.ally_shroud_),
	ally_fog_(that.ally_fog_),
	city_rect_(that.city_rect_),
	avoid_cfg_(that.avoid_cfg_),
	avoid_(terrain_filter(vconfig(avoid_cfg_), units_)),
	has_avoid_(that.has_avoid_),
	commercials_(that.commercials_),
	skill_feature_(that.skill_feature_),
	w_(that.w_),
	h_(that.h_),
	field_troops_vsize_(that.field_troops_vsize_)
{
	field_troops_ = (unit**)malloc(w_ * h_ * sizeof(unit*));
	if (field_troops_vsize_) {
		memcpy(field_troops_, that.field_troops_, field_troops_vsize_ * sizeof(unit*));
	}
}

team& team::operator=(const team& that)
{
	units_ = that.units_;
	heros_ = that.heros_;
	leader_ = that.leader_;
	capital_ = that.capital_;
	gold_ = that.gold_;
	character_ = character_;
	holden_cities_ = that.holden_cities_;
	navigation_ = that.navigation_;
	bomb_turns_ = that.bomb_turns_;
	shroud_ = that.shroud_;
	fog_ = that.fog_;
	auto_shroud_updates_ = that.auto_shroud_updates_;
	info_ = that.info_;
	countdown_time_ = that.countdown_time_;
	action_bonus_count_ = that.action_bonus_count_;
	diplomatisms_ = that.diplomatisms_;
	strategies_ = that.strategies_;
	features_ = that.features_;
	seen_ = that.seen_;
	ally_shroud_ = that.ally_shroud_;
	ally_fog_ = that.ally_fog_;
	city_rect_ = that.city_rect_;
	field_troops_vsize_ = that.field_troops_vsize_;

	appointed_nobles_ = appointed_nobles_;
	unappoint_nobles_ = unappoint_nobles_;

	avoid_cfg_ = that.avoid_cfg_;
	avoid_ = terrain_filter(vconfig(avoid_cfg_), units_);
	has_avoid_ = that.has_avoid_;
	commercials_ = that.commercials_;
	skill_feature_ = that.skill_feature_;

	if (w_ * h_ != that.w_ * that.h_) {
		free(field_troops_);
		field_troops_ = (unit**)malloc(that.w_ * that.h_ * sizeof(unit*));
	}
	w_ = that.w_;
	h_ = that.h_;
	if (field_troops_vsize_) {
		memcpy(field_troops_, that.field_troops_, field_troops_vsize_ * sizeof(unit*));
	}
	return *this;
}

team::~team()
{
	if (field_troops_) {
		free(field_troops_);
	}
	field_troops_vsize_ = 0;
}

void team::build(const config &cfg, const gamemap& map, int gold)
{
	throw game::game_error("invalid call, it is given up by me");
}

void team::write(config& cfg) const
{
	info_.write(cfg);

	std::stringstream can_str;
	// build
	can_str.str("");
	for (std::set<const unit_type*>::const_iterator cr = can_build_.begin(); cr != can_build_.end(); ++cr) {
		if (cr != can_build_.begin()) {
			can_str << ",";
		}
		can_str << (*cr)->id();
	}
	cfg["build"] = can_str.str();

	cfg["leader"] = leader_;
	cfg["shroud"] = uses_shroud();
	cfg["fog"] = uses_fog();
	cfg["gold"] = gold_;
	cfg["navigation"] = navigation_;
	cfg["bomb_turns"] = bomb_turns_;

	// write card
	std::stringstream str;
	if (!candidate_cards_.empty()) {
		size_t start, last;
		std::pair<size_t, size_t> p;
		std::vector<size_t>::const_iterator i = candidate_cards_.begin();
		p.first = last = start = *i;
		for (++ i; i != candidate_cards_.end(); ++ i) {
			if (*i != last + 1) {
				if (p.first != last) {
					str << p.first << "-" << last;
				} else {
					str << p.first;
				}
				str << ",";
				p.first = *i;
			}
			last = *i;
		}
		if (p.first == last) {
			str << last;
		} else {
			str << p.first << "-" << last;
		}
		cfg["candidate_cards"] = str.str();
	}
	
	if (!holded_cards_.empty()) {
		bool first = true;
		str.str("");
		for (std::vector<size_t>::const_iterator i = holded_cards_.begin(); i != holded_cards_.end(); ++ i) {
			if (!first) {
				str << ",";
			} else {
				first = false;
			}
			str << *i;
		}
		cfg["holded_cards"] = str.str();
	}

	if (!holded_treasures_.empty()) {
		bool first = true;
		str.str("");
		for (std::vector<size_t>::const_iterator i = holded_treasures_.begin(); i != holded_treasures_.end(); ++ i) {
			if (!first) {
				str << ",";
			} else {
				first = false;
			}
			str << *i;
		}
		cfg["treasure"] = str.str();
	}

	if (!holded_technologies_.empty()) {
		bool first = true;
		str.str("");
		for (std::vector<const ttechnology*>::const_iterator i = holded_technologies_.begin(); i != holded_technologies_.end(); ++ i) {
			if (!first) {
				str << ",";
			} else {
				first = false;
			}
			str << (*i)->id();
		}
		cfg["technologies"] = str.str();
	}

	if (!commercials_.empty()) {
		bool first = true;
		str.str("");
		for (std::vector<hero*>::const_iterator i = commercials_.begin(); i != commercials_.end(); ++ i) {
			if (!first) {
				str << ",";
			} else {
				first = false;
			}
			int number = (*i)->number_;
			str << number;
		}
		cfg["commercials"] = str.str();
	}

	cfg["shroud_data"] = shroud_.write();

	cfg["countdown_time"] = countdown_time_;
	cfg["action_bonus_count"] = action_bonus_count_;

	if (has_avoid_) {
		cfg.add_child("avoid", avoid_.to_config());
	}
}

void team::write(uint8_t* mem) const
{
	team_fields_t* fields = (team_fields_t*)mem;

	fields->side_ = info_.side;
	fields->start_gold_ = info_.start_gold;
	fields->gold_add_ = info_.gold_add? 1: 0;
	fields->income_ = info_.income;
	fields->objectives_changed_ = info_.objectives_changed? 1: 0;
	fields->action_bonus_count_ = info_.action_bonus_count;
	fields->village_gold_ = info_.income_per_village;
	fields->previous_turn_  = previous_turn;
	fields->disallow_observers_ = info_.disallow_observers? 1: 0;
	fields->allow_player_ = info_.allow_player? 1: 0;
	fields->capital_ = capital_? capital_->master().number_: HEROS_INVALID_NUMBER;
	fields->controller_ = info_.controller;
	// statstic
	fields->cause_damage_ = cause_damage_;
	fields->been_damage_ = been_damage_;
	fields->defeat_units_ = defeat_units_;
	fields->perfect_turns_ = perfect_turns_;
	fields->defeat_units_one_turn_ = defeat_units_one_turn_;

	fields->share_maps_ = info_.share_maps? 1: 0;
	fields->share_view_ = info_.share_view? 1: 0;
	fields->leader_ = leader_;
	fields->shroud_ = uses_shroud()? 1: 0;
	fields->fog_ = uses_fog()? 1: 0;
	fields->gold_ = gold_;
	fields->navigation_ = navigation_;
	fields->bomb_turns_ = bomb_turns_;

	bool first;
	int offset = sizeof(team_fields_t);
	// diplomatisms_, (ally, forbided)
	fields->diplomatisms_.offset_ = offset; 
	fields->diplomatisms_.size_ = teams->size();
	for (std::map<int, diplomatism_data>::const_iterator it = diplomatisms_.begin(); it != diplomatisms_.end(); ++ it) {
		diplomatism_data_fields_t* data = (diplomatism_data_fields_t*)(mem + offset);
		data->ally_ = it->second.ally_? 1: 0;
		data->forbid_turns_ = it->second.forbid_turns_;
		offset += sizeof(diplomatism_data_fields_t);
	}
	// strategies_, (target_, type_, start_turn_, impletement_turns_, allies_)
	fields->strategies_.offset_ = offset; 
	fields->strategies_.size_ = strategies_.size();
	for (std::vector<strategy>::const_iterator it = strategies_.begin(); it != strategies_.end(); ++ it) {
		strategy_fields_t* data = (strategy_fields_t*)(mem + offset);
		data->target_ = it->target_;
		data->type_ = it->type_;
		data->start_turn_ = it->start_turn_;
		data->impletement_turns_ = it->impletement_turns_;
		data->allies_ = it->allies_.size();
		offset += sizeof(strategy_fields_t);
		for (std::set<int>::const_iterator it_ally = it->allies_.begin(); it_ally != it->allies_.end(); ++ it_ally) {
			int i32 = *it_ally;
			memcpy(mem + offset, &i32, 4);
			offset += 4;
		}
	}
	// features_
	fields->features_.offset_ = offset; 
	fields->features_.size_ = features_.size();
	for (std::vector<arms_feature>::const_iterator it = features_.begin(); it != features_.end(); ++ it) {
		memcpy(mem + offset, &*it, sizeof(arms_feature));
		offset += sizeof(arms_feature);
	}
	// name
	if (!info_.name.empty()) {
		fields->name_.offset_ = offset; 
		fields->name_.size_ = info_.name.length();
		memcpy(mem + offset, info_.name.c_str(), fields->name_.size_);
	} else {
		fields->name_.offset_ = -1;
		fields->name_.size_ = 0;
	}
	offset += fields->name_.size_;
	// user_team_name
	{
		fields->user_team_name_.offset_ = -1;
		fields->user_team_name_.size_ = 0;
	}
	offset += fields->user_team_name_.size_;
	// save_id
	if (!info_.save_id.empty()) {
		fields->save_id_.offset_ = offset; 
		fields->save_id_.size_ = info_.save_id.length();
		memcpy(mem + offset, info_.save_id.c_str(), fields->save_id_.size_);
	} else {
		fields->save_id_.offset_ = -1;
		fields->save_id_.size_ = 0;
	}
	offset += fields->save_id_.size_;
	// current_player
	if (!info_.current_player.empty()) {
		fields->current_player_.offset_ = offset; 
		fields->current_player_.size_ = info_.current_player.length();
		memcpy(mem + offset, info_.current_player.c_str(), fields->current_player_.size_);
	} else {
		fields->current_player_.offset_ = -1;
		fields->current_player_.size_ = 0;
	}
	offset += fields->current_player_.size_;
	// flag
	if (!info_.flag.empty()) {
		fields->flag_.offset_ = offset; 
		fields->flag_.size_ = info_.flag.length();
		memcpy(mem + offset, info_.flag.c_str(), fields->flag_.size_);
	} else {
		fields->flag_.offset_ = -1;
		fields->flag_.size_ = 0;
	}
	offset += fields->flag_.size_;
	// flag_icon
	if (!info_.flag_icon.empty()) {
		fields->flag_icon_.offset_ = offset; 
		fields->flag_icon_.size_ = info_.flag_icon.length();
		memcpy(mem + offset, info_.flag_icon.c_str(), fields->flag_icon_.size_);
	} else {
		fields->flag_icon_.offset_ = -1;
		fields->flag_icon_.size_ = 0;
	}
	offset += fields->flag_icon_.size_;
	// id
	if (!info_.flag_icon.empty()) {
		fields->id_.offset_ = offset; 
		fields->id_.size_ = info_.description.length();
		memcpy(mem + offset, info_.description.c_str(), fields->id_.size_);
	} else {
		fields->id_.offset_ = -1;
		fields->id_.size_ = 0;
	}
	offset += fields->id_.size_;
	// objectives
	if (!info_.objectives.empty()) {
		fields->objectives_.offset_ = offset; 
		fields->objectives_.size_ = info_.objectives.str().length();
		memcpy(mem + offset, info_.objectives.str().c_str(), fields->objectives_.size_);
	} else {
		fields->objectives_.offset_ = -1;
		fields->objectives_.size_ = 0;
	}
	offset += fields->objectives_.size_;

	std::stringstream can_str;
	// build
	can_str.str("");
	for (std::set<const unit_type*>::const_iterator cr = can_build_.begin(); cr != can_build_.end(); ++ cr) {
		if (cr != can_build_.begin()) {
			can_str << ",";
		}
		can_str << (*cr)->id();
	}
	if (!can_str.str().empty()) {
		fields->build_.offset_ = offset; 
		fields->build_.size_ = can_str.str().length();
		memcpy(mem + offset, can_str.str().c_str(), fields->build_.size_);
	} else {
		fields->build_.offset_ = -1;
		fields->build_.size_ = 0;
	}
	offset += fields->build_.size_;
	// music
	if (!info_.music.empty()) {
		fields->music_.offset_ = offset; 
		fields->music_.size_ = info_.music.length();
		memcpy(mem + offset, info_.music.c_str(), fields->music_.size_);
	} else {
		fields->music_.offset_ = -1;
		fields->music_.size_ = 0;
	}
	offset += fields->music_.size_;
	// color
	if (!info_.color.empty()) {
		fields->color_.offset_ = offset; 
		fields->color_.size_ = info_.color.length();
		memcpy(mem + offset, info_.color.c_str(), fields->color_.size_);
	} else {
		fields->color_.offset_ = -1;
		fields->color_.size_ = 0;
	}
	offset += fields->color_.size_;
	// countdown_time
	if (!info_.countdown_time.empty()) {
		fields->countdown_time_.offset_ = offset; 
		fields->countdown_time_.size_ = info_.countdown_time.length();
		memcpy(mem + offset, info_.countdown_time.c_str(), fields->countdown_time_.size_);
	} else {
		fields->countdown_time_.offset_ = -1;
		fields->countdown_time_.size_ = 0;
	}
	offset += fields->countdown_time_.size_;

	std::stringstream str;

	str.str("");
	// candidate_cards
	if (!candidate_cards_.empty()) {
		size_t start, last;
		std::pair<size_t, size_t> p;
		std::vector<size_t>::const_iterator i = candidate_cards_.begin();
		p.first = last = start = *i;
		for (++ i; i != candidate_cards_.end(); ++ i) {
			if (*i != last + 1) {
				if (p.first != last) {
					str << p.first << "-" << last;
				} else {
					str << p.first;
				}
				str << ",";
				p.first = *i;
			}
			last = *i;
		}
		if (p.first == last) {
			str << last;
		} else {
			str << p.first << "-" << last;
		}
	}
	if (!str.str().empty()) {
		fields->candidate_cards_.offset_ = offset; 
		fields->candidate_cards_.size_ = str.str().length();
		memcpy(mem + offset, str.str().c_str(), fields->candidate_cards_.size_);
	} else {
		fields->candidate_cards_.offset_ = -1;
		fields->candidate_cards_.size_ = 0;
	}
	offset += fields->candidate_cards_.size_;
	// holded_cards
	str.str("");
	first = true;
	if (!holded_cards_.empty()) {
		str.str("");
		for (std::vector<size_t>::const_iterator i = holded_cards_.begin(); i != holded_cards_.end(); ++ i) {
			if (!first) {
				str << ",";
			} else {
				first = false;
			}
			str << *i;
		}
	}
	if (!str.str().empty()) {
		fields->holded_cards_.offset_ = offset; 
		fields->holded_cards_.size_ = str.str().length();
		memcpy(mem + offset, str.str().c_str(), fields->holded_cards_.size_);
	} else {
		fields->holded_cards_.offset_ = -1;
		fields->holded_cards_.size_ = 0;
	}
	offset += fields->holded_cards_.size_;
	// treasures
	str.str("");
	first = true;
	if (!holded_treasures_.empty()) {
		str.str("");
		for (std::vector<size_t>::const_iterator i = holded_treasures_.begin(); i != holded_treasures_.end(); ++ i) {
			if (!first) {
				str << ",";
			} else {
				first = false;
			}
			str << *i;
		}
	}
	if (!str.str().empty()) {
		fields->holded_treasures_.offset_ = offset; 
		fields->holded_treasures_.size_ = str.str().length();
		memcpy(mem + offset, str.str().c_str(), fields->holded_treasures_.size_);
	} else {
		fields->holded_treasures_.offset_ = -1;
		fields->holded_treasures_.size_ = 0;
	}
	offset += fields->holded_treasures_.size_;
	// technologies
	str.str("");
	first = true;
	if (!holded_technologies_.empty()) {
		str.str("");
		for (std::vector<const ttechnology*>::const_iterator i = holded_technologies_.begin(); i != holded_technologies_.end(); ++ i) {
			if (!first) {
				str << ",";
			} else {
				first = false;
			}
			str << (*i)->id();
		}
	}
	if (!str.str().empty()) {
		fields->holded_technologies_.offset_ = offset; 
		fields->holded_technologies_.size_ = str.str().length();
		memcpy(mem + offset, str.str().c_str(), fields->holded_technologies_.size_);
	} else {
		fields->holded_technologies_.offset_ = -1;
		fields->holded_technologies_.size_ = 0;
	}
	offset += fields->holded_technologies_.size_;
	// half-researched technologies
	str.str("");
	first = true;
	if (!half_technologies_.empty()) {
		str.str("");
		for (std::map<const ttechnology*, int>::const_iterator i = half_technologies_.begin(); i != half_technologies_.end(); ++ i) {
			if (!first) {
				str << ",";
			} else {
				first = false;
			}
			str << i->first->id() << "," << i->second;
		}
	}
	if (!str.str().empty()) {
		fields->half_technologies_.offset_ = offset; 
		fields->half_technologies_.size_ = str.str().length();
		memcpy(mem + offset, str.str().c_str(), fields->half_technologies_.size_);
	} else {
		fields->half_technologies_.offset_ = -1;
		fields->half_technologies_.size_ = 0;
	}
	offset += fields->half_technologies_.size_;
	// researching technology
	if (ing_technology_) {
		fields->ing_technology_.offset_ = offset; 
		fields->ing_technology_.size_ = ing_technology_->id().length();
		memcpy(mem + offset, ing_technology_->id().c_str(), fields->ing_technology_.size_);
	} else {
		fields->ing_technology_.offset_ = -1;
		fields->ing_technology_.size_ = 0;
	}
	offset += fields->ing_technology_.size_;
	// commercials
	str.str("");
	if (!commercials_.empty()) {
		bool first = true;
		for (std::vector<hero*>::const_iterator i = commercials_.begin(); i != commercials_.end(); ++ i) {
			if (!first) {
				str << ",";
			} else {
				first = false;
			}
			int number = (*i)->number_;
			str << number;
		}
	}
	if (!str.str().empty()) {
		fields->commercials_.offset_ = offset; 
		fields->commercials_.size_ = str.str().length();
		memcpy(mem + offset, str.str().c_str(), fields->commercials_.size_);
	} else {
		fields->commercials_.offset_ = -1;
		fields->commercials_.size_ = 0;
	}
	offset += fields->commercials_.size_;
	// village
	str.str("");
	// village is deleted, reserved by in future.
	if (!str.str().empty()) {
		fields->village_.offset_ = offset; 
		fields->village_.size_ = str.str().length();
		memcpy(mem + offset, str.str().c_str(), fields->village_.size_);
	} else {
		fields->village_.offset_ = -1;
		fields->village_.size_ = 0;
	}
	offset += fields->village_.size_;

	config cfg;
	cfg["shroud_data"] = shroud_.write();
	cfg.add_child("ai", ai::manager::to_config(info_.side));
	if (has_avoid_) {
		cfg.add_child("avoid", avoid_.to_config());
	}
	str.str("");
	::write(str, cfg);
	fields->cfg_.offset_ = offset;
	fields->cfg_.size_ = str.str().length() + 1;
	memcpy(mem + offset, str.str().c_str(), fields->cfg_.size_ - 1);
	mem[fields->cfg_.offset_ + fields->cfg_.size_ - 1] = '\0';
	offset += fields->cfg_.size_;

	// align 4
	offset = (offset + 3) & ~3;

	// total size
	fields->size_ = offset;
}

void team::read(const uint8_t* mem, const gamemap& map, size_t team_size)
{
	team_fields_t* fields = (team_fields_t*)mem;
	
	info_.side = fields->side_;

	config tmp_cfg;
	::read(tmp_cfg, std::string((char*)(mem + fields->cfg_.offset_)));
	shroud_.read(tmp_cfg["shroud_data"]);
	avoid_cfg_ = tmp_cfg.child("avoid")? tmp_cfg.child("avoid"): config();
	has_avoid_ = tmp_cfg.child("avoid")? true: false;
	if (has_avoid_) {
		avoid_ = terrain_filter(vconfig(avoid_cfg_), units_);
	}
	// [ai]
	ai::manager::add_ai_for_side_from_config(info_.side, tmp_cfg, true);

	info_.gold = fields->gold_;
	info_.start_gold = fields->start_gold_;
	info_.gold_add = fields->gold_add_? true: false;
	info_.income = fields->income_;
	info_.objectives_changed = fields->objectives_changed_? true: false;
	info_.action_bonus_count = fields->action_bonus_count_;
	info_.income_per_village = fields->village_gold_;
	previous_turn = fields->previous_turn_;
	info_.disallow_observers = fields->disallow_observers_? true: false;
	info_.allow_player = fields->allow_player_? true: false;
	bh_eval.capital = fields->capital_;
	info_.controller = (controller_tag::CONTROLLER)fields->controller_;
	// statistic
	cause_damage_ = fields->cause_damage_;
	been_damage_ = fields->been_damage_;
	defeat_units_ = fields->defeat_units_;
	perfect_turns_ = fields->perfect_turns_;
	defeat_units_one_turn_ = fields->defeat_units_one_turn_;

	info_.share_maps = fields->share_maps_? true: false;
	info_.share_view = fields->share_view_? true: false;
	set_shroud(fields->shroud_? true: false);
	set_fog(fields->fog_? true: false);

	leader_ = fields->leader_;
	if (info_.controller == controller_tag::EMPTY && team::empty_side == -1 && leader_ == hero::number_empty_leader) {
		team::empty_side = info_.side;
	}

	navigation_ = fields->navigation_;
	bomb_turns_ = fields->bomb_turns_;

	std::string str;
	if (tent::lobby_side_params.empty()) {
		// diplomatisms_, (ally, forbided)
		for (int i = 0; i < fields->diplomatisms_.size_; i ++) {
			diplomatism_data_fields_t* data = (diplomatism_data_fields_t*)(mem + fields->diplomatisms_.offset_ + i * sizeof(diplomatism_data_fields_t));
			diplomatisms_[i + 1].ally_ = data->ally_? true: false;
			diplomatisms_[i + 1].forbid_turns_ = data->forbid_turns_;
		}
	} else {
		calculate_diplomatisms(tent::lobby_side_params[info_.side - 1].ally, team_size);
		if (!diplomatisms_[info_.side].ally_) {
			diplomatisms_[info_.side].ally_ = true;
		}
	}
	// strategies_, (target_, type_, start_turn_, impletement_turns_, allies_)
	int tune = 0;
	for (int i = 0; i < fields->strategies_.size_; i ++) {
		strategy_fields_t* data = (strategy_fields_t*)(mem + fields->strategies_.offset_ + tune);
		strategies_.push_back(strategy(data->target_, data->type_, data->impletement_turns_));
		strategy& s = strategies_.back();
		s.start_turn_ = data->start_turn_;
		tune += sizeof(strategy_fields_t);
		for (int i2 = 0; i2 < data->allies_; i2 ++, tune += 4) {
			s.allies_.insert(*(int*)(mem + fields->strategies_.offset_ + tune));
		}
	}
	// features_
	tune = 0;
	for (int i = 0; i < fields->features_.size_; i ++) {
		arms_feature* data = (arms_feature*)(mem + fields->features_.offset_ + tune);
		features_.push_back(arms_feature(data->arms_, data->level_, data->feature_));
		tune += sizeof(arms_feature);
	}
	// name
	info_.name.assign((const char*)mem + fields->name_.offset_, fields->name_.size_);
	// save_id
	info_.save_id.assign((const char*)mem + fields->save_id_.offset_, fields->save_id_.size_);
	// current_player
	if (tent::lobby_side_params.empty()) {
		info_.current_player.assign((const char*)mem + fields->current_player_.offset_, fields->current_player_.size_);
	} else {
		info_.current_player = tent::lobby_side_params[info_.side - 1].current_player;
	}
	// flag
	info_.flag.assign((const char*)mem + fields->flag_.offset_, fields->flag_.size_);
	// flag_icon
	info_.flag_icon.assign((const char*)mem + fields->flag_icon_.offset_, fields->flag_icon_.size_);
	// id
	info_.description.assign((const char*)mem + fields->id_.offset_, fields->id_.size_);
	// objectives
	info_.objectives = std::string((const char*)mem + fields->objectives_.offset_, fields->objectives_.size_);
	// build
	can_build_.clear();
	str.assign((const char*)mem + fields->build_.offset_, fields->build_.size_);
	std::vector<std::string> v_size = utils::split(str);
	bool keep_existed = false, wall_existed = false;
	for (std::vector<std::string>::const_iterator i = v_size.begin(); i != v_size.end(); ++i) {
		const unit_type* ut = unit_types.find(*i);
		if (ut) {
			if (ut == unit_types.find_keep()) {
				keep_existed = true;
			}
			if (ut == unit_types.find_wall()) {
				wall_existed = true;
			}
			can_build_.insert(ut);
		}
	}
	// ensure both wall and keep are exist or not.
	if (!tent::tower_mode()) {
		if ((keep_existed && !wall_existed) || (!keep_existed && wall_existed)) {
			if (!keep_existed) {
				can_build_.insert(unit_types.find_keep());
			}
			if (!wall_existed) {
				can_build_.insert(unit_types.find_wall());
			}
		}
	} else {
		if (tent::mode == mode_tag::TOWER && can_build_.size() > 1) {
			throw game::game_error("invalid build attribute in side(" + str_cast(info_.side) + ") tag");
		} else if (can_build_.size() == 1 && *can_build_.begin() != unit_types.find_wall()) {
			throw game::game_error("invalid build attribute in side(" + str_cast(info_.side) + ") tag, tower mode can build wall only!");
		}
	}

	// music
	info_.music.assign((const char*)mem + fields->music_.offset_, fields->music_.size_);
	// color
	info_.color.assign((const char*)mem + fields->color_.offset_, fields->color_.size_);
	// countdown_time
	info_.countdown_time.assign((const char*)mem + fields->countdown_time_.offset_, fields->countdown_time_.size_);

	// candidate_cards
	candidate_cards_.clear();
	str.assign((const char*)mem + fields->candidate_cards_.offset_, fields->candidate_cards_.size_);
	if (!str.empty() && str.at(0) == '$') {
		str = resources::state_of_game->get_variable(str.substr(1)).str();
	}
	std::vector<std::pair<int, int> > ranges = utils::parse_ranges(str);
	for (std::vector<std::pair<int, int> >::const_iterator r = ranges.begin(); r != ranges.end(); ++ r) {
		for (int i = r->first; i <= r->second; i ++) {
			candidate_cards_.push_back(i);			
		}
	}
	// holded_cards
	holded_cards_.clear();
	str.assign((const char*)mem + fields->holded_cards_.offset_, fields->holded_cards_.size_);
	if (!str.empty() && str.at(0) == '$') {
		str = resources::state_of_game->get_variable(str.substr(1)).str();
	}
	v_size = utils::split(str);
	for (std::vector<std::string>::const_iterator i = v_size.begin(); i != v_size.end(); ++ i) {
		holded_cards_.push_back(atoi(i->c_str()));
	}

	// treasures
	holded_treasures_.clear();
	str.assign((const char*)mem + fields->holded_treasures_.offset_, fields->holded_treasures_.size_);
	v_size = utils::split(str);
	for (std::vector<std::string>::const_iterator i = v_size.begin(); i != v_size.end(); ++ i) {
		holded_treasures_.push_back(atoi(i->c_str()));
	}
	// technologies
	holded_technologies_.clear();
	str.assign((const char*)mem + fields->holded_technologies_.offset_, fields->holded_technologies_.size_);
	v_size = utils::split(str);
	const std::map<std::string, ttechnology>& technologies = unit_types.technologies();
	for (std::vector<std::string>::const_iterator it = v_size.begin(); it != v_size.end(); ++ it) {
		std::map<std::string, ttechnology>::const_iterator find = technologies.find(*it);
		VALIDATE(find != technologies.end(), "Cannot find " + *it + " in pre-defined technology.");
		holded_technologies_.push_back(&find->second);
	}
	// half-researched technologies
	half_technologies_.clear();
	if (fields->half_technologies_.size_) {
		str.assign((const char*)mem + fields->half_technologies_.offset_, fields->half_technologies_.size_);
		v_size = utils::split(str);
		for (std::vector<std::string>::const_iterator it = v_size.begin(); it != v_size.end(); ++ it) {
			std::map<std::string, ttechnology>::const_iterator find = technologies.find(*it);
			VALIDATE(find != technologies.end(), "Cannot find " + *it + " in pre-defined technology.");
			const ttechnology& t = find->second;
			++ it;
			half_technologies_.insert(std::make_pair(&t, lexical_cast_default<int>(*it)));
		}
	}
	// researching technology
	ing_technology_ = NULL;
	if (fields->ing_technology_.size_) {
		str.assign((const char*)mem + fields->ing_technology_.offset_, fields->ing_technology_.size_);
		ing_technology_ = &technologies.find(str)->second;
	}
	// commercials
	commercials_.clear();
	str.assign((const char*)mem + fields->commercials_.offset_, fields->commercials_.size_);
	v_size = utils::split(str);
	for (std::vector<std::string>::const_iterator i = v_size.begin(); i != v_size.end(); ++ i) {
		commercials_.push_back(&heros_[atoi(i->c_str())]);
	}
	calculate_skill_feature();

	// village
	str.assign((const char*)mem + fields->village_.offset_, fields->village_.size_);
	v_size = utils::parenthetical_split(str);
	for (std::vector<std::string>::const_iterator it = v_size.begin(); it != v_size.end(); ++ it) {
		const std::vector<std::string> loc_str = utils::split(*it);
		if (loc_str.size() == 2) {
			const map_location loc(lexical_cast_default<int>(loc_str[0]) - 1, lexical_cast_default<int>(loc_str[1]) - 1);
			if (map.is_village(loc)) {
			} else {
				std::stringstream err;
				err << "[side] " << name() << " [village] points to a non-village location " << loc;
				VALIDATE(false, err.str());
			}
		}
	}
}

bool team::owns_village(const map_location& loc) const
{
	unit* base = units_.find_unit(loc, false);
	if (!base || !base->fort()) {
		return false;
	}
	return base->side() == info_.side;
}

int team::base_income() const 
{
	int rpg_bonus = 0;
	if (rpg::h->side_ + 1 == info_.side) {
		if (rpg::stratum == hero_stratum_wander) {
			rpg_bonus = 120;
		} else if (rpg::stratum == hero_stratum_citizen) {
			rpg_bonus = 120;
		} else if (rpg::stratum == hero_stratum_mayor) {
			rpg_bonus = 80;
		}
	}
	return rpg_bonus + info_.income + game_config::base_income; 
}

bool team::gold_can_build_ea() const
{
	if (gold_ < unit_types.find_market()->cost()) {
		return false;
	} 
	if (gold_ < unit_types.find_technology()->cost()) {
		return false;
	}
	if (gold_ < unit_types.find_tactic()->cost()) {
		return false;
	}
	return true;
}

int team::total_income() const 
{
	// bool magnate_found;
	int income = 0;
	for (std::vector<artifical*>::const_iterator it = holden_cities_.begin(); it != holden_cities_.end(); ++ it) {
		artifical& city = **it;
		income += city.total_gold_income(market_increase_);
	}
	return income + base_income();
}

int team::side_upkeep() const 
{
	if (tent::tower_mode()) {
		return 0;
	}

	int upkeep = 0;

	for (size_t idx = 0; idx < field_troops_vsize_; idx ++) {
		const unit& un = *(field_troops_[idx]);
		upkeep += un.upkeep();
	}

	for (std::vector<artifical*>::const_iterator city_itor = holden_cities_.begin(); city_itor != holden_cities_.end(); ++ city_itor) {
		upkeep += (*city_itor)->upkeep();
	}

	return upkeep;
}

int team::calculate_technology_income(int income) const
{
	if (income) {
		int multiplier = 1;
		int divisor = holden_cities_.size();
		const int max_safe_turn = 500;

		int exponent = cost_exponent();
		if (exponent > 400) {
			multiplier *= 100;
			divisor *= 100 + (exponent - 400);
		}

		int turn = resources::controller->turn();
		if (turn > max_safe_turn) {
			multiplier *= 100;
			divisor *= 100 + (turn - max_safe_turn);
		}
		income = round_damage(income, multiplier, divisor);
	}
	return income;
}

int team::total_technology_income() const
{
	// bool magnate_found;
	int income = 0;
	for (std::vector<artifical*>::const_iterator it = holden_cities_.begin(); it != holden_cities_.end(); ++ it) {
		artifical& city = **it;
		income += city.total_technology_income(technology_increase_);
	}

	return calculate_technology_income(income);
}

void team::rpg_changed(bool independence)
{
	for (std::vector<artifical*>::const_iterator it = holden_cities_.begin(); it != holden_cities_.end(); ++ it) {
		artifical& city = **it;
		city.reset_max_recruit_cost();
	}

	if (independence) {
		size_t team_size = teams->size();
		for (size_t i = 1; i < team_size; i ++) {
			(*teams)[i - 1].diplomatisms_[team_size] = diplomatism_data();
		}
		// calculate_diplomatisms("", team_size);
		// diplomatisms_[team_size].ally_ = true;
	}
}

void team::set_builds(const std::set<std::string>& builds)
{
	can_build_.clear();
	for (std::set<std::string>::const_iterator it = builds.begin(); it != builds.end(); ++ it) {
		can_build_.insert(unit_types.find(*it));
	}
}

void team::reselect_ing_technology()
{
	if (ing_technology_) {
		half_technologies_.erase(ing_technology_);
		ing_technology_ = NULL;
	}
	select_ing_technology();
}

void team::insert_technology(const ttechnology& t)
{
	VALIDATE(std::find(holded_technologies_.begin(), holded_technologies_.end(), &t) == holded_technologies_.end(), "team::insert_technology, duplicate insert!");
	holded_technologies_.push_back(&t);

	bool adjust_unit = false;
	// apply this technology
	const std::vector<const ttechnology*>& parts = t.parts();
	for (std::vector<const ttechnology*>::const_iterator it = parts.begin(); it != parts.end(); ++ it) {
		const ttechnology& t2 = **it;
		if (t2.occasion() == ttechnology::FINISH) {
			add_modification_internal(t2.apply_to(), t2.effect_cfg());
		} else {
			adjust_unit = true;
		}
	}

	if (adjust_unit) {
		readjust_all_unit();
	}
}

int team::current_stratagem() const
{
	int tag = stratagem_tag::none;

	// first stratagem is current stratagem
	for (std::vector<const ttechnology*>::const_iterator it = holded_technologies_.begin(); it != holded_technologies_.end(); ++ it) {
		const ttechnology& t = **it;
		if (t.stratagem()) {
			tag = stratagem_tag::find(t.id());
			break;
		}
	}
	return tag;
}

void team::insert_stratagem(const ttechnology& s, bool erase)
{
	if (erase) {
		erase_stratagem();
	}
	VALIDATE(current_stratagem() == stratagem_tag::none, "team::insert_stratagem, has existed stratagem!");
	VALIDATE(s.stratagem(), "team::insert_stratagem, it isn't stratagem!");

	holded_technologies_.push_back(&s);
	add_modification_internal(s.apply_to(), s.effect_cfg());
}

void team::erase_stratagem()
{
	for (std::vector<const ttechnology*>::iterator it = holded_technologies_.begin(); it != holded_technologies_.end(); ) {
		const ttechnology& t = **it;
		if (t.stratagem()) {
			it = holded_technologies_.erase(it);
		} else {
			++ it;
		}
	}
}

void show_noble_message(unit_map& units, hero_map& heros, hero& h, const tnoble& noble, bool terminate)
{
	utils::string_map symbols;
	std::string message;

	int incident = game_events::INCIDENT_APPOINT;
	symbols["noble"] = tintegrate::generate_format(noble.name(), tintegrate::object_color);
	if (terminate) {
		if (&h == rpg::h) {
			message = vgettext("Your noble of $noble is terminated.", symbols);
		} else {
			symbols["hero"] = tintegrate::generate_format(h.name(), tintegrate::hero_color);
			message = vgettext("$hero noble of $noble is terminated.", symbols);
		}
		incident = game_events::INCIDENT_INVALID;
	} else {
		if (&h == rpg::h) {
			message = vgettext("Congratulate! You are appointed $noble.", symbols);
		} else {
			symbols["hero"] = tintegrate::generate_format(h.name(), tintegrate::hero_color);
			message = vgettext("Congratulate! $hero is appointed $noble.", symbols);
		}
		incident = game_events::INCIDENT_APPOINT;
	}
	if (!message.empty()) {
		game_events::show_hero_message(&heros[hero::number_civilian], units.city_from_cityno(h.city_), message, incident);
	}
}

void team::select_leader_noble(bool show_message)
{
	if (is_empty()) {
		return;
	}

	hero& leader_hero = heros_[leader_];
	int city_count = holden_cities_.size();

	if (leader_hero.noble_ != HEROS_NO_NOBLE) {
		const tnoble& noble = unit_types.noble(leader_hero.noble_);
		if (noble.level() == game_config::max_noble_level) {
			return;
		}
		if (city_count <= noble.condition().city) {
			return;
		}
	}

	int original_noble = leader_hero.noble_;
	for (int level = game_config::max_noble_level; level >= 0; level --) {
		const std::set<tnoble>& level_nobles = unit_types.level_nobles(level);
		for (std::set<tnoble>::const_iterator it = level_nobles.begin(); it != level_nobles.end(); ++ it) {
			const tnoble& noble = *it;
			if (noble.leader()) {
				if (city_count >= noble.condition().city || (leader_hero.noble_ == HEROS_NO_NOBLE && level == 0)) {
					leader_hero.noble_ = noble.index();

					// exit outer for loop
					level = 0;
					break;
				}
			}
		}
	}

	if (original_noble != leader_hero.noble_) {
		if (original_noble == HEROS_NO_NOBLE) {
			team::appointed_nobles();

		} else if (original_noble != HEROS_NO_NOBLE) {
			// elevate existed noble
			appointed_nobles_.clear();
			unappoint_nobles_.clear();
			fill_normal_noble(false, show_message);
		}
	}

	if (show_message && original_noble != leader_hero.noble_) {
		const tnoble& noble = unit_types.noble(leader_hero.noble_);
		show_noble_message(units_, heros_, leader_hero, noble, false);
	}
	return;
}

const std::map<int, hero*>& team::appointed_nobles(bool clear)
{
	if (clear) {
		appointed_nobles_.clear();
		unappoint_nobles_.clear();
	}
	if (is_empty() || !appointed_nobles_.empty() || !unappoint_nobles_.empty()) {
		return appointed_nobles_;
	}

	for (std::vector<artifical*>::const_iterator it = holden_cities_.begin(); it != holden_cities_.end(); ++ it) {
		artifical* city = *it;

		const std::vector<unit*>& resides = city->reside_troops();
		for (std::vector<unit*>::const_iterator it2 = resides.begin(); it2 != resides.end(); ++ it2) {
			unit& u = **it2;
			if (u.master().number_ != leader_ && u.master().noble_ != HEROS_NO_NOBLE) {
				appointed_nobles_.insert(std::make_pair(u.master().noble_, &u.master()));
			}
			if (u.second().valid() && u.second().number_ != leader_ && u.second().noble_ != HEROS_NO_NOBLE) {
				appointed_nobles_.insert(std::make_pair(u.second().noble_, &u.second()));
			}
			if (u.third().valid() && u.third().number_ != leader_ && u.third().noble_ != HEROS_NO_NOBLE) {
				appointed_nobles_.insert(std::make_pair(u.third().noble_, &u.third()));
			}
		}

		const std::vector<unit*>& fields = city->field_troops();
		for (std::vector<unit*>::const_iterator it2 = fields.begin(); it2 != fields.end(); ++ it2) {
			unit& u = **it2;
			if (u.master().number_ != leader_ && u.master().noble_ != HEROS_NO_NOBLE) {
				appointed_nobles_.insert(std::make_pair(u.master().noble_, &u.master()));
			}
			if (u.second().valid() && u.second().number_ != leader_ && u.second().noble_ != HEROS_NO_NOBLE) {
				appointed_nobles_.insert(std::make_pair(u.second().noble_, &u.second()));
			}
			if (u.third().valid() && u.third().number_ != leader_ && u.third().noble_ != HEROS_NO_NOBLE) {
				appointed_nobles_.insert(std::make_pair(u.third().noble_, &u.third()));
			}
		}

		const std::vector<hero*>& freshes = city->fresh_heros();
		const std::vector<hero*>& finishes = city->finish_heros();
		for (std::vector<hero*>::const_iterator it2 = freshes.begin(); it2 != freshes.end(); ++ it2) {
			hero& h = **it2;
			if (h.number_ != leader_ && h.noble_ != HEROS_NO_NOBLE) {
				appointed_nobles_.insert(std::make_pair(h.noble_, &h));
			}
		}
		for (std::vector<hero*>::const_iterator it2 = finishes.begin(); it2 != finishes.end(); ++ it2) {
			hero& h = **it2;
			if (h.number_ != leader_ && h.noble_ != HEROS_NO_NOBLE) {
				appointed_nobles_.insert(std::make_pair(h.noble_, &h));
			}
		}
	}
	if (holden_cities_.empty()) {
		for (size_t i = 0; i < field_troops_vsize_; i ++) {
			unit& u = *field_troops_[i];
			if (u.master().number_ != leader_ && u.master().noble_ != HEROS_NO_NOBLE) {
				appointed_nobles_.insert(std::make_pair(u.master().noble_, &u.master()));
			}
			if (u.second().valid() && u.second().number_ != leader_ && u.second().noble_ != HEROS_NO_NOBLE) {
				appointed_nobles_.insert(std::make_pair(u.second().noble_, &u.second()));
			}
			if (u.third().valid() && u.third().number_ != leader_ && u.third().noble_ != HEROS_NO_NOBLE) {
				appointed_nobles_.insert(std::make_pair(u.third().noble_, &u.third()));
			}
		}
	}
	int leader_noble_level = unit_types.noble(heros_[leader_].noble_).level();
	for (int level = leader_noble_level; level >= 0; level --) {
		const std::set<tnoble>& level_nobles = unit_types.level_nobles(level);
		for (std::set<tnoble>::const_iterator it = level_nobles.begin(); it != level_nobles.end(); ++ it) {
			if (it->leader()) {
				continue;
			}
			int index = it->index();
			if (appointed_nobles_.find(index) == appointed_nobles_.end()) {
				unappoint_nobles_.insert(index);
			}
		}
	}
	return appointed_nobles_;
}

const std::set<int>& team::unappoint_nobles()
{
	if (is_empty() || !appointed_nobles_.empty() || !unappoint_nobles_.empty()) {
		return unappoint_nobles_;
	}
	appointed_nobles();
	return unappoint_nobles_;
}

void team::fill_normal_noble(bool init, bool show_message)
{
	if (is_empty()) {
		return;
	}
	if (!init && is_network() && !info_.current_player.empty()) {
		return;
	}

	appointed_nobles();

	if (unappoint_nobles_.empty()) {
		return;
	}
	size_t can_allocate = unappoint_nobles_.size();
	size_t human_can_allocate = 0;
	if (is_human()) {
		if (rpg::stratum != hero_stratum_leader) {
			human_can_allocate = std::max<size_t>(1, (appointed_nobles_.size() + unappoint_nobles_.size())  / 3);
		} else {
			human_can_allocate = appointed_nobles_.size() + unappoint_nobles_.size();
		}

		if (can_allocate > human_can_allocate) {
			can_allocate -= human_can_allocate;
		} else {
			can_allocate = 0;
		}
	}
	if (!can_allocate && (!init || !human_can_allocate)) {
		return;
	}
	bool human_and_leader = is_human() && rpg::stratum == hero_stratum_leader;
	
	std::vector<hero*> candidate, candidate_human;
	for (std::vector<artifical*>::const_iterator it = holden_cities_.begin(); it != holden_cities_.end(); ++ it) {
		artifical* city = *it;

		const std::vector<unit*>& resides = city->reside_troops();
		for (std::vector<unit*>::const_iterator it2 = resides.begin(); it2 != resides.end(); ++ it2) {
			unit& u = **it2;
			if (!u.hdata_variable()) {
				continue;
			}
			if (!init && !troop_can_appoint_noble(u, NULL, false)) {
				continue;
			}
			if (u.master().number_ != leader_ && u.master().noble_ == HEROS_NO_NOBLE) {
				if (!u.human()) {
					candidate.push_back(&u.master());
				} else if (init) {
					candidate_human.push_back(&u.master());
				}
			}
			if (u.second().valid() && u.second().number_ != leader_ && u.second().noble_ == HEROS_NO_NOBLE) {
				if (!u.human()) {
					candidate.push_back(&u.second());
				} else if (init) {
					candidate_human.push_back(&u.second());
				}
			}
			if (u.third().valid() && u.third().number_ != leader_ && u.third().noble_ == HEROS_NO_NOBLE) {
				if (!u.human()) {
					candidate.push_back(&u.third());
				} else if (init) {
					candidate_human.push_back(&u.third());
				}
			}
		}

		const std::vector<unit*>& fields = city->field_troops();
		for (std::vector<unit*>::const_iterator it2 = fields.begin(); it2 != fields.end(); ++ it2) {
			unit& u = **it2;
			if (!u.hdata_variable()) {
				continue;
			}
			if (!init && !troop_can_appoint_noble(u, NULL, true)) {
				continue;
			}
			if (u.master().number_ != leader_ && u.master().noble_ == HEROS_NO_NOBLE) {
				if (!u.human()) {
					candidate.push_back(&u.master());
				} else if (init) {
					candidate_human.push_back(&u.master());
				}
			}
			if (u.second().valid() && u.second().number_ != leader_ && u.second().noble_ == HEROS_NO_NOBLE) {
				if (!u.human()) {
					candidate.push_back(&u.second());
				} else if (init) {
					candidate_human.push_back(&u.second());
				}
			}
			if (u.third().valid() && u.third().number_ != leader_ && u.third().noble_ == HEROS_NO_NOBLE) {
				if (!u.human()) {
					candidate.push_back(&u.third());
				} else if (init) {
					candidate_human.push_back(&u.third());
				}
			}
		}

		const std::vector<hero*>& freshes = city->fresh_heros();
		const std::vector<hero*>& finishes = city->finish_heros();
		
		for (std::vector<hero*>::const_iterator it2 = freshes.begin(); it2 != freshes.end(); ++ it2) {
			hero& h = **it2;
			if (h.number_ != leader_ && h.noble_ == HEROS_NO_NOBLE) {
				if (!human_and_leader && city->mayor() != rpg::h && h.number_ != rpg::h->number_) {
					candidate.push_back(&h);
				} else if (init) {
					candidate_human.push_back(&h);
				}
			}
		}
		for (std::vector<hero*>::const_iterator it2 = finishes.begin(); it2 != finishes.end(); ++ it2) {
			hero& h = **it2;
			if (h.number_ != leader_ && h.noble_ == HEROS_NO_NOBLE) {
				if (!human_and_leader && city->mayor() != rpg::h && h.number_ != rpg::h->number_) {
					candidate.push_back(&h);
				} else if (init) {
					candidate_human.push_back(&h);
				}
			}
		}
	}
	if (holden_cities_.empty()) {
		for (size_t i = 0; i < field_troops_vsize_; i ++) {
			unit& u = *field_troops_[i];
			if (u.master().number_ != leader_ && u.master().noble_ == HEROS_NO_NOBLE) {
				candidate.push_back(&u.master());
			}
			if (u.second().valid() && u.second().number_ != leader_ && u.second().noble_ == HEROS_NO_NOBLE) {
				candidate.push_back(&u.second());
			}
			if (u.third().valid() && u.third().number_ != leader_ && u.third().noble_ == HEROS_NO_NOBLE) {
				candidate.push_back(&u.third());
			}
		}
	}
	std::sort(candidate.begin(), candidate.end(), compare_leadership);

	// appoint
	std::set<unit*> diff_troops;
	for (int step = 0; step < 2; step ++) {
		if (step == 1) {
			if (!init) {
				break;
			}
			std::sort(candidate_human.begin(), candidate_human.end(), compare_leadership);
		}
		size_t i = 0;
		for (std::set<int>::iterator it = unappoint_nobles_.begin(); it != unappoint_nobles_.end(); i ++) {
			if (step == 0) {
				if (i >= candidate.size() || i == can_allocate) {
					break;
				}
			} else {
				if (i >= candidate_human.size() || i == human_can_allocate) {
					break;
				}
			}

			int noble_index = *it;
			hero* h;
			if (step == 0) {
				h = candidate[i];
			} else {
				h = candidate_human[i];
			}
			h->noble_ = noble_index;
			appointed_nobles_.insert(std::make_pair(noble_index, h));
			unappoint_nobles_.erase(it ++);
			
			unit* u = units_.find_unit(*h);
			if (!u->is_artifical()) {
				diff_troops.insert(u);
			}
		}
	}

	for (std::set<unit*>::iterator it = diff_troops.begin(); it != diff_troops.end(); ++ it) {
		unit& u = **it;
		u.adjust();
	}
}

void team::appoint_noble(hero& h, int noble, bool show_message)
{
	if (noble != HEROS_NO_NOBLE) {
		if (h.noble_ != HEROS_NO_NOBLE) {
			// replace
			appointed_nobles_.erase(h.noble_);
			unappoint_nobles_.insert(h.noble_);
		}
		appointed_nobles_.insert(std::make_pair(noble, &h));

		VALIDATE(unappoint_nobles_.find(noble) != unappoint_nobles_.end(), "team::appoint_noble, unappoint_nobles_.find(noble) != unappoint_nobles_.end()!");
		unappoint_nobles_.erase(noble);
	} else {
		// terminate
		VALIDATE(h.noble_ != HEROS_NO_NOBLE, std::string("team::appoint_noble, ") + h.name() + " hasn't noble, cannot terminate!");
		appointed_nobles_.erase(h.noble_);

		VALIDATE(unappoint_nobles_.find(noble) == unappoint_nobles_.end(), "team::appoint_noble, unappoint_nobles_.find(noble) == unappoint_nobles_.end()!");
		unappoint_nobles_.insert(h.noble_);
	}
	h.noble_ = noble;
}

void team::do_technology_income(int income)
{
	if (!ing_technology_ || income <= 0) {
		return;
	}

	std::map<const ttechnology*, int>::iterator find = half_technologies_.find(ing_technology_);
	if (find != half_technologies_.end()) {
		int xp = find->second + income;
		int max_experience = technology_max_experience(*ing_technology_);
		if (xp >= max_experience) {
			utils::string_map symbols;
			std::string message;

			symbols["end"] = tintegrate::generate_format(ing_technology_->name(), tintegrate::tactic_color);
			symbols["side"] = tintegrate::generate_format(name(), tintegrate::hero_color);

			holded_technologies_.push_back(ing_technology_);
			// apply this technology
			bool adjust_unit = false;
			const std::vector<const ttechnology*>& parts = ing_technology_->parts();
			for (std::vector<const ttechnology*>::const_iterator it = parts.begin(); it != parts.end(); ++ it) {
				const ttechnology& t = **it;
				if (t.occasion() == ttechnology::FINISH) {
					add_modification_internal(t.apply_to(), t.effect_cfg());
				} else {
					adjust_unit = true;
				}
			}
			if (adjust_unit) {
				readjust_all_unit();
			}

			reselect_ing_technology();

			if (ing_technology_) {
				symbols["begin"] = tintegrate::generate_format(ing_technology_->name(), tintegrate::tactic_color);
				message = vgettext("$side finished researching $end, will begin to research $begin!", symbols);
			} else {
				message = vgettext("$side finished researching $end, and have gotten all technology!", symbols);
			}
			game_events::show_hero_message(&heros_[hero::number_civilian], NULL, message, game_events::INCIDENT_TECHNOLOGY);
		} else {
			find->second = xp;
		}
	} else {
		half_technologies_.insert(std::make_pair(ing_technology_, income));
	}
}

void team::new_turn(int random)
{ 
	defeat_units_one_turn_ = 0;

	gold_ += base_income();
	increase_bomb_turns();
	refresh_tactic_slots(*resources::screen);

	if (stratagem_half_kill) {
		std::vector<unit*> candidate;
		for (size_t n = 0; n < teams->size(); n ++) {
			if (n + 1 == info_.side || !is_enemy(n + 1)) {
				continue;
			}
			const team& t = (*teams)[n];
			const std::pair<unit**, size_t> p = t.field_troop();
			unit** troops = p.first;
			size_t troops_vsize = p.second;
			for (size_t i = 0; i < troops_vsize; i ++) {
				unit& u = *(troops[i]);
				if (u.is_artifical()) {
					continue;
				}
				if (u.is_commoner()) {
					continue;
				}
				if (u.invisible(u.get_location())) {
					continue;
				}
				candidate.push_back(&u);
			}
		}
		if (!candidate.empty()) {
			unit& selected = *candidate[random % candidate.size()];
			unit_display::stratagem_anim(stratagem_tag::technology(stratagem_tag::xiao_li_cang_dao), selected.master().image(true), false);

			std::map<unit*, int> touched;
			touched.insert(std::make_pair(&selected, selected.max_hitpoints() * 4 / 5));
			multi_attack(units_, units_.find_unit(heros_[leader_]), "", touched, info_.side);
		}	
	}
	if (stratagem_half_heal) {
		unit* selected = NULL;
		int max_percent = 0;
		int max_hitpoint = 0;
		for (size_t n = 0; n < teams->size(); n ++) {
			if (is_enemy(n + 1)) {
				continue;
			}
			const team& t = (*teams)[n];
			const std::pair<unit**, size_t> p = t.field_troop();
			unit** troops = p.first;
			size_t troops_vsize = p.second;
			for (size_t i = 0; i < troops_vsize; i ++) {
				unit& u = *(troops[i]);
				if (u.is_artifical()) {
					continue;
				}
				if (u.is_commoner()) {
					continue;
				}
				int percent = 100 - (u.hitpoints() * 100 / u.max_hitpoints());
				if (percent > max_percent || (percent == max_percent && u.max_hitpoints() > max_hitpoint)) {
					max_percent = percent;
					max_hitpoint = u.max_hitpoints();
					selected = &u;
				}
			}
		}
		if (selected) {
			unit_display::stratagem_anim(stratagem_tag::technology(stratagem_tag::miao_shou_hui_chun), selected->master().image(true), true);
			int amount = selected->max_hitpoints() * 4 / 5;
			selected->heal(amount);
			unit_display::unit_heal2(*teams, units_, NULL, *selected, amount);
		}
	}

	// appoint noble
	fill_normal_noble(false, true);
	
	// update strategy
	for (std::vector<strategy>::iterator it = strategies_.begin(); it != strategies_.end(); ++ it) {
		if (it->impletement_turns_ > 0) {
			it->impletement_turns_ --;
		}
	}

	// update diplomatism. forbid_turns
	for (std::map<int, diplomatism_data>::iterator it = diplomatisms_.begin(); it != diplomatisms_.end(); ++ it) {
		if (it->second.forbid_turns_ > 0) {
			it->second.forbid_turns_ --;
		}
	}
}

bool team::is_enemy(int n) const
{
	if (!teams) {
		return false;
	}
	return !diplomatisms_.find(n)->second.ally_;
}

void team::set_ally(int n, bool alignment, bool dialog, bool adjust)
{
	if (alignment) {
		VALIDATE(!diplomatisms_[n].ally_, "team::set_ally, want to alignment with " + (*teams)[n - 1].name() + ", but has allied."); 
	} else {
		VALIDATE(diplomatisms_[n].ally_, "team::set_ally, want to release with " + (*teams)[n - 1].name() + ", but hasn't been allied."); 
	}
	diplomatisms_[n].ally_ = alignment;

	if (dialog && is_human()) {
		utils::string_map symbols;
		std::string message;

		symbols["first"] = tintegrate::generate_format(name(), tintegrate::hero_color);
		symbols["second"] = tintegrate::generate_format((*teams)[n - 1].name(), tintegrate::hero_color);
		if (alignment) {
			message = vgettext("$first and $second concluded treaty of alliance.", symbols);
		} else {
			message = vgettext("$first and $second terminate treaty of alliance.", symbols);
		}
		game_events::show_hero_message(&heros_[hero::number_scout], NULL, message, game_events::INCIDENT_ALLY);
	}

	if (adjust && !alignment) {
		for (std::vector<artifical*>::const_iterator it = holden_cities_.begin(); it != holden_cities_.end(); ++ it) {
			artifical& city = **it;
			city.ally_terminate_adjust((*teams)[n - 1]);
		}
	}
}

bool team::ally_forbided(int n) const
{
	if (!teams) {
		return false;
	}
	return diplomatisms_.find(n)->second.forbid_turns_ > 0;
}

void team::set_forbid_turns(int n, int turns)
{
	std::map<int, diplomatism_data>::iterator it = diplomatisms_.find(n);
	it->second.forbid_turns_ = turns;
}

void team::calculate_diplomatisms(const std::string& team_name, size_t team_size) const
{
	if (!diplomatisms_.empty()) {
		for (std::map<int, diplomatism_data>::iterator it = diplomatisms_.begin(); it != diplomatisms_.end(); ++ it) {
			it->second.ally_ = false;
		}
	} else {
		for (size_t i = 1; i <= team_size; i ++) {
			diplomatisms_[i] = diplomatism_data();
		}
	}
	std::vector<std::string> our_teams = utils::split(team_name);
		
	for (std::vector<std::string>::iterator it = our_teams.begin(); it != our_teams.end(); ++ it) {
		int to = lexical_cast_default<int>(*it);
		VALIDATE(to <= (int)team_size, "team::calculate_diplomatisms, invalid team_name: " + team_name + " in " + leader()->name() + " side.");
		diplomatisms_[to].ally_ = true;
	}
}

void team::add_strategy(const strategy& s)
{
	strategies_.push_back(s);
}

void team::erase_strategy(int target, bool dialog)
{
	utils::string_map symbols;
	std::string message;

	for (std::vector<strategy>::iterator it = strategies_.begin(); it != strategies_.end(); ++ it) {
		if (it->target_ == target) {
			// terminate relative treaty.
			artifical* target_city = units_.city_from_cityno(it->target_);
			symbols["city"] = tintegrate::generate_format(target_city->name(), tintegrate::object_color);

			for (std::set<int>::const_iterator it_ally = it->allies_.begin(); it_ally != it->allies_.end(); ++ it_ally) {
				team& allied_team = (*teams)[*it_ally - 1];
				
				set_ally(*it_ally, false);
				allied_team.set_ally(info_.side, false);

				if (dialog) {
					symbols["first"] = tintegrate::generate_format(name(), tintegrate::hero_color);
					symbols["second"] = tintegrate::generate_format(allied_team.name(), tintegrate::hero_color);
					if (it->type_ == strategy::AGGRESS) {
						message = vgettext("End aggressing upon $city, $first and $second terminate treaty of alliance.", symbols);
					} else if (it->type_ == strategy::DEFEND) {
						message = vgettext("End defending $city, $first and $second terminate treaty of alliance.", symbols);
					} else {
						VALIDATE(false, "team::erase_strategy, unknown strategy type.");
					}
					if (is_human() || allied_team.is_human()) {
						game_events::show_hero_message(&heros_[hero::number_scout], NULL, message, game_events::INCIDENT_ALLY);
					}
				}
			}
			strategies_.erase(it);
			break;
		}
	}
	return;
}

strategy& team::get_strategy(int target, int type)
{
	for (std::vector<strategy>::iterator it = strategies_.begin(); it != strategies_.end(); ++ it) {
		if (target && it->target_ != target) {
			continue;
		}
		if ((type != strategy::NONE) && it->type_ != type) {
			continue;
		}
		return *it;
	}
	return null_strategy;
}

const strategy& team::get_strategy(int target, int type) const
{
	return const_cast<team *>(this)->get_strategy(target, type);
}

void team::set_share_maps( bool share_maps ){
	// Share_view and share_maps can't both be enabled,
	// so share_view overrides share_maps.
	// If you want to change them, be sure to change share_view FIRST
	info_.share_maps = !info_.share_view && share_maps;
}

void team::erase_strategies(bool show)
{
	utils::string_map symbols;
	std::string message;

	for (std::vector<strategy>::iterator it = strategies_.begin(); it != strategies_.end();) {
		// terminate relative treaty.
		artifical* target_city = units_.city_from_cityno(it->target_);
		symbols["city"] = target_city->name();

		for (std::set<int>::const_iterator it_ally = it->allies_.begin(); it_ally != it->allies_.end(); ++ it_ally) {
			team& allied_team = (*teams)[*it_ally - 1];
				
			set_ally(*it_ally, false);
			allied_team.set_ally(info_.side, false);

			if (show) {
				symbols["first"] = name();
				symbols["second"] = allied_team.name();
				if (it->type_ == strategy::AGGRESS) {
					message = vgettext("End aggressing upon $city, $first and $second terminate treaty of alliance.", symbols);
				} else if (it->type_ == strategy::DEFEND) {
					message = vgettext("End defending $city, $first and $second terminate treaty of alliance.", symbols);
				} else {
					VALIDATE(false, "team::erase_strategy, unknown strategy type.");
				}
				game_events::show_hero_message(&heros_[hero::number_scout], NULL, message, game_events::INCIDENT_ALLY);
			}
		}
		it = strategies_.erase(it);
	}
	return;
}

void team::erase_ally(int side)
{
	for (std::vector<strategy>::iterator it = strategies_.begin(); it != strategies_.end(); ++ it) {
		// terminate relative treaty.
		for (std::set<int>::const_iterator it_ally = it->allies_.begin(); it_ally != it->allies_.end(); ++ it_ally) {
			if (*it_ally == side) {
				team& allied_team = (*teams)[*it_ally - 1];

				set_ally(*it_ally, false);
				allied_team.set_ally(info_.side, false);

				it->allies_.erase(it_ally);
				break;
			}
		}
	}
}

void team::set_share_view( bool share_view ){
	info_.share_view = share_view;
}

void team::change_controller(const std::string& controller)
{
	controller_tag::CONTROLLER r = controller_tag::find(controller);

	std::stringstream err;
	err << "team::change_controller, invalid controller str: " << controller;
	VALIDATE(r != controller_tag::NONE, err.str());
	change_controller(r);
}

void team::change_controller(controller_tag::CONTROLLER controller)
{ 
	if (info_.controller == controller) {
		return;
	}

	enum {none, set, clear};
	int action = none;
	if (controller != controller_tag::HUMAN && controller != controller_tag::NETWORK) {
		action = clear;
	} else if (controller == controller_tag::HUMAN && rpg::stratum == hero_stratum_leader) {
		action = set;
	}

	if (action != none) {
		for (std::vector<artifical*>::iterator it = holden_cities_.begin(); it != holden_cities_.end(); ++ it) {
			artifical& c = **it;
			std::vector<unit*>& field = c.field_troops();
			for (std::vector<unit*>::iterator it1 = field.begin(); it1 != field.end(); ++ it1) {
				unit& u = **it1;
				if (action == set) {
					if (!u.human()) {
						u.set_human(true);
					}
				} else if (action == clear) {
					if (u.human()) {
						u.set_human(false);
					}
				}
			}
			std::vector<unit*>& reside = c.reside_troops();
			for (std::vector<unit*>::iterator it1 = reside.begin(); it1 != reside.end(); ++ it1) {
				unit& u = **it1;
				if (action == set) {
					if (!u.human()) {
						u.set_human(true);
					}
				} else if (action == clear) {
					if (u.human()) {
						u.set_human(false);
					}
				}
			}
		}
	}

	controller_tag::CONTROLLER original = info_.controller;
	info_.controller = controller;

	hero& h = heros_[leader_];
	if (original == controller_tag::EMPTY) {
		// null ===> other
		VALIDATE(!h.has_nomal_noble(), "team::change_controller, null ==> other, leader has noble!");
		select_leader_noble(false);
		h.official_ = hero_official_leader;

	} else if (controller == controller_tag::EMPTY) {
		// other ===> null
		if (h.official_ == hero_official_leader) {
			h.official_ = HEROS_NO_OFFICIAL;
		}
		h.noble_ = HEROS_NO_NOBLE;
		set_capital(NULL);
	}

}

std::string team::team_name() const 
{ 
	std::stringstream str;
	bool first = true;
	for (std::map<int, diplomatism_data>::const_iterator it = diplomatisms_.begin(); it != diplomatisms_.end(); ++ it) {
		if (it->second.ally_) {
			if (!first) {
				str << ",";
			}
			first = false;
			str << it->first;
		}
	}
	return str.str();
}

void team::clear_caches()
{
	// Reset the cache of allies for all teams
	if (teams != NULL) {
		for (std::vector<team>::const_iterator i = teams->begin(); i != teams->end(); ++i) {
			// i->diplomatisms_.clear();
			i->ally_shroud_.clear();
			i->ally_fog_.clear();
		}
	}
}

void team::set_objectives(const t_string& new_objectives, bool silently)
{
	info_.objectives = new_objectives;
	if(!silently)
		info_.objectives_changed = true;
}

bool team::shrouded(const map_location& loc) const
{
	if(!teams)
		return shroud_.value(loc.x+1,loc.y+1);

	return shroud_.shared_value(ally_shroud(*teams),loc.x+1,loc.y+1);
}

bool team::fogged(const map_location& loc) const
{
	if(shrouded(loc)) return true;

	if(!teams)
		return fog_.value(loc.x+1,loc.y+1);

	return fog_.shared_value(ally_fog(*teams),loc.x+1,loc.y+1);
}

const std::vector<const team::shroud_map*>& team::ally_shroud(const std::vector<team>& teams) const
{
	if(ally_shroud_.empty()) {
		for(size_t i = 0; i < teams.size(); ++i) {
			if(!is_enemy(i + 1) && (&(teams[i]) == this || teams[i].share_view() || teams[i].share_maps())) {
				ally_shroud_.push_back(&(teams[i].shroud_));
			}
		}
	}

	return ally_shroud_;
}

const std::vector<const team::shroud_map*>& team::ally_fog(const std::vector<team>& teams) const
{
	if(ally_fog_.empty()) {
		for(size_t i = 0; i < teams.size(); ++i) {
			if(!is_enemy(i + 1) && (&(teams[i]) == this || teams[i].share_view())) {
				ally_fog_.push_back(&(teams[i].fog_));
			}
		}
	}

	return ally_fog_;
}

bool team::knows_about_team(size_t index, bool is_multiplayer) const
{
	const team& t = (*teams)[index];

	// We know about our own team
	if(this == &t) return true;

	// If we aren't using shroud or fog, then we know about everyone
	if(!uses_shroud() && !uses_fog()) return true;

	// We don't know about enemies
	if(is_enemy(index+1)) return false;

	// We know our allies in multiplayer
	if (is_multiplayer) return true;

	// We know about allies we're sharing maps with
	if(share_maps() && t.uses_shroud()) return true;

	// We know about allies we're sharing view with
	if(share_view() && (t.uses_fog() || t.uses_shroud())) return true;

	return false;
}

bool team::copy_ally_shroud()
{
	if(!teams || !share_maps())
		return false;

	return shroud_.copy_from(ally_shroud(*teams));
}

int team::nteams()
{
	if(teams == NULL) {
		return 0;
	} else {
		return teams->size();
	}
}

bool is_observer()
{
	if(teams == NULL) {
		return true;
	}

	BOOST_FOREACH (const team &t, *teams) {
		if (t.is_human())
			return false;
	}

	return true;
}

void validate_side(int side)
{
	if (teams == NULL || !teams->size()) {
		return;
	}

	if (side < 1 || side > int(teams->size())) {
		throw game::game_error("invalid side(" + str_cast(side) + ") found in unit definition");
	}
}

bool team::shroud_map::clear(int x, int y)
{
	if(enabled_ == false || x < 0 || y < 0)
		return false;

	if(x >= static_cast<int>(data_.size()))
		data_.resize(x+1);

	if(y >= static_cast<int>(data_[x].size()))
		data_[x].resize(y+1);

	if(data_[x][y] == false) {
		data_[x][y] = true;
		return true;
	} else {
		return false;
	}
}

void team::shroud_map::place(int x, int y)
{
	if(enabled_ == false || x < 0 || y < 0)
		return;

	if (x >= static_cast<int>(data_.size())) {

	} else if (y >= static_cast<int>(data_[x].size())) {

	} else {
		data_[x][y] = false;
	}
}

void team::shroud_map::reset()
{
	if(enabled_ == false)
		return;

	for(std::vector<std::vector<bool> >::iterator i = data_.begin(); i != data_.end(); ++i) {
		std::fill(i->begin(),i->end(),false);
	}
}

bool team::shroud_map::value(int x, int y) const
{
	if(enabled_ == false || x < 0 || y < 0)
		return false;

	if(x >= static_cast<int>(data_.size()))
		return true;

	if(y >= static_cast<int>(data_[x].size()))
		return true;

	if(data_[x][y])
		return false;
	else
		return true;
}

bool team::shroud_map::shared_value(const std::vector<const shroud_map*>& maps, int x, int y) const
{
	if(enabled_ == false || x < 0 || y < 0)
		return false;

	for(std::vector<const shroud_map*>::const_iterator i = maps.begin(); i != maps.end(); ++i) {
		if((*i)->enabled_ == true && (*i)->value(x,y) == false)
			return false;
	}
	return true;
}

std::string team::shroud_map::write() const
{
	std::stringstream shroud_str;
	for(std::vector<std::vector<bool> >::const_iterator sh = data_.begin(); sh != data_.end(); ++sh) {
		shroud_str << '|';

		for(std::vector<bool>::const_iterator i = sh->begin(); i != sh->end(); ++i) {
			shroud_str << (*i ? '1' : '0');
		}

		shroud_str << '\n';
	}

	return shroud_str.str();
}

void team::shroud_map::read(const std::string& str)
{
	data_.clear();
	for(std::string::const_iterator sh = str.begin(); sh != str.end(); ++sh) {
		if(*sh == '|')
			data_.resize(data_.size()+1);

		if(data_.empty() == false) {
			if(*sh == '1')
				data_.back().push_back(true);
			else if(*sh == '0')
				data_.back().push_back(false);
		}
	}
}

void team::shroud_map::merge(const std::string& str)
{
	int x=0, y=0;
	for(std::string::const_iterator sh = str.begin(); sh != str.end(); ++sh) {
		if(*sh == '|' && sh != str.begin()) {
			y=0;
			x++;
		} else if(*sh == '1') {
			clear(x,y);
			y++;
		} else if(*sh == '0') {
			y++;
		}
	}
}

bool team::shroud_map::copy_from(const std::vector<const shroud_map*>& maps)
{
	if(enabled_ == false)
		return false;

	bool cleared = false;
	for(std::vector<const shroud_map*>::const_iterator i = maps.begin(); i != maps.end(); ++i) {
		if((*i)->enabled_ == false)
			continue;

		const std::vector<std::vector<bool> >& v = (*i)->data_;
		for(size_t x = 0; x != v.size(); ++x) {
			for(size_t y = 0; y != v[x].size(); ++y) {
				if(v[x][y]) {
					cleared |= clear(x,y);
				}
			}
		}
	}
	return cleared;
}

std::map<int, color_range> team::team_color_range_;

const color_range team::get_side_color_range(int side){
  std::string index = get_side_color_index(side);
  std::map<std::string, color_range>::iterator gp=game_config::team_rgb_range.find(index);

  if(gp != game_config::team_rgb_range.end()){
    return(gp->second);
  }

  return(color_range(0x00FF0000,0x00FFFFFF,0x00000000,0x00FF0000));
}

const SDL_Color team::get_minimap_color(int side)
{
	// Note: use mid() instead of rep() unless
	// high contrast is needed over a map or minimap!
	return int_to_color(get_side_color_range(side).rep());
}

std::string team::get_side_color_index(int side)
{
	size_t index = size_t(side-1);

	if (teams != NULL && index < teams->size()) {
		const std::string side_map = (*teams)[index].map_color_to();
		if(side_map.size()) {
			return side_map;
		}
	}
	// return str_cast(side);
	std::vector<std::pair<std::string, t_string> > team_rgb_namep = game_config::team_rgb_name;
	return game_config::team_rgb_name[index % game_config::team_rgb_name.size()].first;
}

std::string team::get_side_highlight(int side)
{
	return rgb2highlight(get_side_color_range(side+1).mid());
}

void team::calculate_skill_feature()
{
	// whether skill feature or not.
	skill_feature_ = false;

	if (commercials_.empty()) {
		return;
	}

	const complex_feature_map& complex_feature = unit_types.complex_feature();

	for (std::vector<hero*>::const_iterator it = commercials_.begin(); it != commercials_.end(); ++ it) {
		const hero& h = **it;

		if (h.feature_ == hero_feature_skill || h.side_feature_ == hero_feature_skill) {
			skill_feature_ = true;
			break;
		}

		for (int feature = HEROS_BASE_FEATURE_COUNT; feature < HEROS_MAX_FEATURE; feature ++) {
			if (h.feature_ != feature) {
				continue;
			}
			complex_feature_map::const_iterator complex_itor = complex_feature.find(feature);
			if (complex_itor == complex_feature.end()) {
				continue;
			}
			for (std::vector<int>::const_iterator itor = complex_itor->second.begin(); itor != complex_itor->second.end(); ++ itor) {
				int single_feature = *itor;
				if (single_feature == hero_feature_skill) {
					skill_feature_ = true;
					break;
				}
			}
		}
		if (skill_feature_) {
			break;
		}
	}
}

bool team::defeat_vote() const
{
	int human_troops = 0;
	int ai_troops = 0;
	for (std::vector<artifical*>::const_iterator it = holden_cities_.begin(); it != holden_cities_.end(); ++ it) {
		artifical* city = *it;

		const std::vector<unit*>& resides = city->reside_troops();
		for (std::vector<unit*>::const_iterator it2 = resides.begin(); it2 != resides.end(); ++ it2) {
			const unit& u = **it2;
			if (u.human()) {
				human_troops ++;
			} else {
				ai_troops ++;
			}
		}
		const std::vector<unit*>& fields = city->field_troops();
		for (std::vector<unit*>::const_iterator it2 = fields.begin(); it2 != fields.end(); ++ it2) {
			if ((*it2)->human()) {
				human_troops ++;
			} else {
				ai_troops ++;
			}
		}
	}

	if (human_troops / 3 > ai_troops) {
		return true;
	} else {
		return false;
	}
}

int team::may_build_wall_count() const
{
	if (!tent::tower_mode()) {
		return 1;
	}
	const int max_artificals = 5;

	int exist_artificals = 0;
	for (size_t i = 0; i < field_troops_vsize_; i ++) {
		const unit_type* ut = field_troops_[i]->type();
		int master = ut->master();
		if (master == hero::number_wall) {
			exist_artificals ++;
		}
	}
	if (exist_artificals >= max_artificals) {
		return 0;
	}
	return max_artificals - exist_artificals;
}

std::string team::form_results_of_battle_tip(const std::string& prefix) const
{
	std::stringstream strstr;

	if (!prefix.empty()) {
		strstr << prefix;
		strstr << "\n\n";
		strstr << tintegrate::generate_format(dsgettext("wesnoth-lib", "Results of battle"), "", 0, true, true);
		strstr << tintegrate::generate_img("misc/tintegrate-split-line.png") << "\n";
	}
	strstr << tintegrate::generate_format(dsgettext("wesnoth-lib", "Cause damage"), "green") << ": ";
	strstr << cause_damage_ << "\n";

	strstr << tintegrate::generate_format(dsgettext("wesnoth-lib", "Been damage"), "green") << ": ";
	strstr << been_damage_ << "\n";

	strstr << tintegrate::generate_format(dsgettext("wesnoth-lib", "Defeat units"), "green") << ": ";
	strstr << defeat_units_ << "\n";

	strstr << tintegrate::generate_format(dsgettext("wesnoth-lib", "scenario^Perfect turns"), "green") << ": ";
	strstr << perfect_turns_;

	return strstr.str();
}

std::vector<unit*> team::active_tactics() const
{
	std::vector<unit*> ret;
	for (std::vector<slot_cache::tslot>::const_iterator it = slot_cache::cache.begin(); it != slot_cache::cache.end(); ++ it) {
		const slot_cache::tslot& slot = *it;
		if (slot.type != slot_cache::tslot::ACTIVE_TACTIC) {
			continue;
		}
		if (slot.u->side() == info_.side) {
			ret.push_back(slot.u);
		}
	}
	return ret;
}

void team::save_last_active_tactic(const std::vector<unit*>& active)
{
	last_active_tactic_.clear();
	for (std::vector<unit*>::const_iterator it = active.begin(); it != active.end(); ++ it) {
		const unit& u = **it;
		last_active_tactic_.push_back(u.tactic_hero());
	}
}

void set_tactic_widget(gui2::tbutton& widget, hero& h, const std::string& label)
{
	int width = widget.fix_width();
	int height = widget.fix_height();

	surface image_ = make_neutral_surface(image::get_image("themes/tactic-bg.png"));
	hero* tactic_hero;
	if (h.valid()) {
		tactic_hero = &h;

		std::stringstream ss;
		ss << h.image() << "~SCALE(36, 45)";
		surface hero_surf = image::get_image(ss.str());

		SDL_Rect clip = ::create_rect(1, 1, 0, 0);
		blit_surface(hero_surf, NULL, image_, &clip);

		if (!label.empty()) {
			surface label_surf = font::get_rendered_text2(label);
			clip.x += 36;
			clip.y = (height - label_surf->h) / 2;
			blit_surface(label_surf, NULL, image_, &clip);
		}

		image_ = scale_surface(image_, width, height);

	} else {
		tactic_hero = NULL;
	}

	widget.set_surface(image_, width, height);
	widget.set_cookie(tactic_hero);
}

void set_bomb_widget(gui2::tbutton& widget, int bomb_turns)
{
	std::stringstream ss;
	int width = widget.fix_width();
	int height = widget.fix_height();

	surface image_;
	if (bomb_turns < game_config::max_bomb_turns) {
		image_ = make_neutral_surface(image::get_image("themes/bomb-bg.png"));
	} else {
		image_ = make_neutral_surface(image::get_image("themes/bomb-full-bg.png"));
	}
	
	
	SDL_Rect dst_clip = create_rect(26, 20, 0, 0);
	ss.str("");
	ss << "misc/digit.png~CROP(" << 8 * bomb_turns << ", 0, 8, 12)";
	blit_surface(image::get_image(ss.str()), NULL, image_, &dst_clip);

	ss.str("");
	ss << "misc/digit.png~CROP(" << 8 * 10 << ", 0, 8, 12)";
	dst_clip.x += 8;
	blit_surface(image::get_image(ss.str()), NULL, image_, &dst_clip);

	ss.str("");
	ss << "misc/digit.png~CROP(" << 8 * game_config::max_bomb_turns << ", 0, 8, 12)";
	dst_clip.x += 8;
	blit_surface(image::get_image(ss.str()), NULL, image_, &dst_clip);

	image_ = scale_surface(image_, width, height);
	widget.set_surface(image_, width, height);
}

void team::refresh_tactic_slots(game_display& disp) const
{
	play_controller& controller = *resources::controller;

	static int theme_tactic_slots = 3;
	std::vector<unit*> tactics = active_tactics();
	int index = 0;
	std::stringstream name;
	gui2::tbutton* btn;
	if (!game_config::hide_tactic_slot && !controller.is_linger_mode() && (allow_active || controller.allow_intervene())) {
		for (std::vector<slot_cache::tslot>::const_iterator it = slot_cache::cache.begin(); it != slot_cache::cache.end(); ++ it, index ++) {
			const slot_cache::tslot& slot = *it;
			hero* h = &slot.u->master();
			if (slot.type == slot_cache::tslot::ACTIVE_TACTIC) {
				h = slot.u->tactic_hero();
			} else if (slot.type == slot_cache::tslot::CAST_TACTIC) {
				h = slot.tactic.h;
			}
			name.str("");
			name << "tactic" << index;
			btn = dynamic_cast<gui2::tbutton*>(disp.get_theme_object(name.str()));
			if (btn) {
				set_tactic_widget(*btn, *h, slot.label());
				btn->set_visible(gui2::twidget::VISIBLE);
			}
		}
		for (; index < game_config::active_tactic_slots; index ++) {
			name.str("");
			name << "tactic" << index;
			btn = dynamic_cast<gui2::tbutton*>(disp.get_theme_object(name.str()));
			set_tactic_widget(*btn, hero_invalid, null_str);
			btn->set_visible(is_human()? gui2::twidget::VISIBLE: gui2::twidget::INVISIBLE);
		}
		for (; index < theme_tactic_slots; index ++) {
			name.str("");
			name << "tactic" << index;
			btn = dynamic_cast<gui2::tbutton*>(disp.get_theme_object(name.str()));
			btn->set_visible(gui2::twidget::INVISIBLE);
		}
	} else {
		for (index = 0; index < theme_tactic_slots; index ++) {
			name.str("");
			name << "tactic" << index;
			btn = dynamic_cast<gui2::tbutton*>(disp.get_theme_object(name.str()));
			btn->set_visible(gui2::twidget::INVISIBLE);
		}
	}

	btn = dynamic_cast<gui2::tbutton*>(disp.get_theme_object("bomb"));
	if (!controller.is_linger_mode() && support_bomb) {
		set_bomb_widget(*btn, bomb_turns_);
		btn->set_visible(gui2::twidget::VISIBLE);
	} else {
		btn->set_visible(gui2::twidget::INVISIBLE);
	}
}

void team::add_city(artifical* city)
{
	// extend rectange of holded cities
	const map_location& loc = city->get_location();

	if (holden_cities_.empty()) {
		city_rect_.x = loc.x;
		city_rect_.y = loc.y;
		city_rect_.w = city_rect_.h = 1;
	} else {
		expand_rect_loc(city_rect_, loc);
	}

	// push this city to holded cities
	holden_cities_.push_back(city);
}

void team::erase_city(const artifical* city)
{
	std::vector<artifical*>::iterator itor = std::find(holden_cities_.begin(), holden_cities_.end(), city);
	if (itor != holden_cities_.end()) {
		holden_cities_.erase(itor);
	} else {
		return;
	}

	// reduce rectange of holded cities
	if (holden_cities_.size() > 1) {
		// recalculate
		map_location min(0x7fffffff, 0x7fffffff);
		map_location max(-1, -1);
		for (std::vector<artifical*>::iterator itor = holden_cities_.begin(); itor != holden_cities_.end(); ++ itor) {
			const map_location& loc = (*itor)->get_location();
			min.x = std::min<int>(min.x, loc.x);
			min.y = std::min<int>(min.y, loc.y);
			max.x = std::max<int>(max.x, loc.x);
			max.y = std::max<int>(max.y, loc.y);
		}
		city_rect_.x = min.x;
		city_rect_.y = min.y;
		city_rect_.w = max.x - min.x + 1;
		city_rect_.h = max.y - min.y + 1;
	} else if (holden_cities_.size() == 1) {
		city_rect_.x = holden_cities_[0]->get_location().x;
		city_rect_.y = holden_cities_[0]->get_location().y;
		city_rect_.w = city_rect_.h = 1;
	} else {
		city_rect_.x = city_rect_.y = -1;
		city_rect_.w = city_rect_.h = 0;
	}
}

int team::character() const
{
	return heros_[leader_].character_;
}

int team::technology_max_experience(const ttechnology& t) const
{
	int max_experience = t.max_experience();
	int relative = t.relative();
	if (relative != HEROS_NO_CHARACTER) {
		for (std::map<int, int>::const_iterator it = character_.begin(); it != character_.end(); ++ it) {
			if (relative == it->first) {
				max_experience = (max_experience * 90) / 100;
				break;
			}
		}
	}
	return max_experience;
}

// use scenario: that runtime executed but replay not.
bool team::add_card(size_t number, bool replay, bool dialog)
{
	size_t size = candidate_cards_.size();
	if (!size || (int)holded_cards_.size() >= game_config::max_cards) {
		return false;
	}

	if (number == CARDS_INVALID_NUMBER) {
		number = candidate_cards_[rand() % candidate_cards_.size()]; 
	}
	holded_cards_.push_back(number);

	if (!replay) {
		recorder.add_card(info_.side, number, dialog);
	}
	return true;
}

bool team::erase_card(int index, bool replay, bool dialog)
{
	int holded_cards_size = (int)holded_cards_.size();

	if (!holded_cards_size) {
		return false;
	}
	if (index == -1) {
		index = rand() % holded_cards_size;
	} else if (index >= holded_cards_size) {
		return false;
	}
	holded_cards_.erase(holded_cards_.begin() + index);

	if (!replay) {
		recorder.erase_card(info_.side, index, map_location::null_location, unit::null_int_unitp_pair, dialog);
	}
	return true;
}

int team::get_random_decree(artifical& city, int random)
{
	std::vector<int> candidate;
	const std::map<int, tdecree>& decrees = unit_types.decrees();
	bool found;

	for (std::map<int, tdecree>::const_iterator it = decrees.begin(); it != decrees.end(); ++ it) {
		const tdecree& d = it->second;
		// front
		if (d.front() && !city.is_front(true)) {
			continue;
		}
		// min level
		if (city.level() < d.min_level()) {
			continue;
		}
		// artifical
		const std::set<int>& require = d.require_artifical();
		if (!require.empty()) {
			found = false;
			const std::vector<map_location>& eas = city.economy_area();
			for (std::vector<map_location>::const_iterator ea = eas.begin(); ea != eas.end(); ++ ea) {
				unit* find = units_.find_unit(*ea, true);
				if (!find) {
					continue;
				}
				if (require.find(find->type()->master()) != require.end()) {
					found = true;
					break;
				}
			}
			if (!found) {
				continue;
			}
		}
		candidate.push_back(it->first);
	}
	
	if (!candidate.empty()) {
		return candidate[random % candidate.size()];
	}
	return NO_DECREE;
}

bool team::condition_card(const card& c, const map_location& loc)
{
	unit* itor = find_visible_unit(loc, *this);
	const config& cond_cfg = c.get_cfg().child("condition");

	std::string unit_str = cond_cfg["unit"].str();
	std::string side_str = cond_cfg["side"].str();
	std::string hero_str = c.target_hero()? cond_cfg["hero"].str(): "";

	if (unit_str.empty()) {
		if (itor) {
			return false;
		}
	} else if (!itor) {
		return false;
	} else {
		const std::vector<std::string>& types = utils::split(unit_str);
		const std::vector<std::string>& hero_types = utils::split(hero_str);
		if (!itor->is_artifical()) {
			if (itor->is_commoner()) {
				return false;
			}
			if (itor->is_soldier()) {
				return false;
			}
			if (itor->master().get_flag(hero_flag_robber)) {
				return false;
			}
			if (!tent::tower_mode() && itor->master().get_flag(hero_flag_npc)) {
				return false;
			}
			if (std::find(types.begin(), types.end(), "troop") == types.end()) {
				return false;
			}
			if (!hero_types.empty() && std::find(hero_types.begin(), hero_types.end(), "military") == hero_types.end()) {
				return false;
			}
		} else if (itor->is_city()) {
			if (std::find(types.begin(), types.end(), "city") == types.end()) {
				return false;
			}
			while (!hero_types.empty()) {
				artifical& art = *unit_2_artifical(&*itor);
				if (!art.fresh_heros().empty() && std::find(hero_types.begin(), hero_types.end(), "fresh") != hero_types.end()) {
					break;
				}
				if (!art.finish_heros().empty() && std::find(hero_types.begin(), hero_types.end(), "finish") != hero_types.end()) {
					break;
				}
				if (!art.wander_heros().empty() && std::find(hero_types.begin(), hero_types.end(), "wander") != hero_types.end()) {
					break;
				}
				if (!art.reside_troops().empty() && std::find(hero_types.begin(), hero_types.end(), "military") != hero_types.end()) {
					break;
				}
				return false;
			}
		} else if (c.target_hero() || std::find(types.begin(), types.end(), "artifical") == types.end()) {
			return false;
		}
	}

	if (!unit_str.empty() && !side_str.empty()) {
		const std::vector<std::string>& sides = utils::split(side_str);
		int itor_side = itor->side();
		if (info_.side == itor_side) {
			if (std::find(sides.begin(), sides.end(), "self") == sides.end()) {
				return false;
			}
		} else if (is_enemy(itor_side)) {
			if (std::find(sides.begin(), sides.end(), "enemy") == sides.end()) {
				return false;
			}
		} else if (std::find(sides.begin(), sides.end(), "ally") == sides.end()) {
			return false;
		}
	}
	return true;
}

void team::consume_card(const card& c, const map_location& loc, std::vector<std::pair<int, unit*> >& touched, int index, bool to_recorder)
{
	const config& range_cfg = c.get_cfg().child("range");
	const config& action_cfg = c.get_cfg().child("action");
	if (!range_cfg || !action_cfg) {
		if (index >= 0) {
			holded_cards_.erase(holded_cards_.begin() + index);
		}
		if (to_recorder) {
			recorder.erase_card(info_.side, index, loc, unit::null_int_unitp_pair, false);
		}
		return;
	}

	// must construct [erase_card] block in replay, [random] can write on.
	if (to_recorder) {
		recorder.erase_card(info_.side, index, loc, touched, false);
	}

	unit_map::iterator u_itor;
	std::vector<unit*> effected;

	if (index >= 0) {
		rect_of_hexes& draw_area = resources::screen->draw_area();
		if (point_in_rect_of_hexes(loc.x, loc.y, draw_area)) {
			unit_display::card_start(cards_, c);
		}
	}
	if (c.target_hero()) {
		for (std::vector<std::pair<int, unit*> >::iterator itor = touched.begin(); itor != touched.end(); ++ itor) {
			hero& h = heros_[itor->first];
			h.add_modification(units_, heros_, *teams, action_cfg, itor->second, leader());
		}
	} else {
		for (std::vector<std::pair<int, unit*> >::iterator itor = touched.begin(); itor != touched.end(); ++ itor) {
			unit& u = *(itor->second);
			u.add_modification(action_cfg, true, true);
		}
	}

	if (index >= 0) {
		std::vector<size_t>::iterator it = holded_cards_.begin();
		std::advance(it, index);
		holded_cards_.erase(it);
	}
}

void touched_heros_internal(unit* u_itor, const std::vector<std::string>& types, std::vector<std::pair<int, unit*> >& pairs)
{
	if (u_itor->is_artifical()) {
		artifical& art = *unit_2_artifical(&*u_itor);
		if (types.empty() || std::find(types.begin(), types.end(), "fresh") != types.end()) {
			std::vector<hero*>& fresh_heros = art.fresh_heros();
			for (std::vector<hero*>::iterator itor = fresh_heros.begin(); itor != fresh_heros.end(); ++ itor) {
				pairs.push_back(std::make_pair((*itor)->number_, &*u_itor));
			}
		}
		if (types.empty() || std::find(types.begin(), types.end(), "finish") != types.end()) {
			std::vector<hero*>& finish_heros = art.finish_heros();
			for (std::vector<hero*>::iterator itor = finish_heros.begin(); itor != finish_heros.end(); ++ itor) {
				pairs.push_back(std::make_pair((*itor)->number_, &*u_itor));
			}
		}
		if (types.empty() || std::find(types.begin(), types.end(), "wander") != types.end()) {
			std::vector<hero*>& wander_heros = art.wander_heros();
			for (std::vector<hero*>::iterator itor = wander_heros.begin(); itor != wander_heros.end(); ++ itor) {
				pairs.push_back(std::make_pair((*itor)->number_, &*u_itor));
			}
		}
		if (types.empty() || std::find(types.begin(), types.end(), "military") != types.end()) {
			std::vector<unit*>& reside_troops = art.reside_troops();
			for (std::vector<unit*>::iterator itor = reside_troops.begin(); itor != reside_troops.end(); ++ itor) {
				unit& u = **itor;
				pairs.push_back(std::make_pair(u.master().number_, &u));
				if (u.second().valid()) {
					pairs.push_back(std::make_pair(u.second().number_, &u));
				}
				if (u.third().valid()) {
					pairs.push_back(std::make_pair(u.third().number_, &u));
				}
			}
		}
	} else {
		unit& u = *u_itor;
		if (types.empty() || std::find(types.begin(), types.end(), "military") != types.end()) {
			pairs.push_back(std::make_pair(u.master().number_, &u));
			if (u.second().valid()) {
				pairs.push_back(std::make_pair(u.second().number_, &u));
			}
			if (u.third().valid()) {
				pairs.push_back(std::make_pair(u.third().number_, &u));
			}
		}
	}
}

void touched_troops_internal(unit* u_itor, const std::vector<std::string>& types, std::vector<std::pair<int, unit*> >& pairs)
{
	if (u_itor->is_city()) {
		artifical& art = *unit_2_artifical(&*u_itor);
		if (types.empty() || std::find(types.begin(), types.end(), "city") != types.end()) {
			pairs.push_back(std::make_pair(-1, &*u_itor));
		}
		if (types.empty() || std::find(types.begin(), types.end(), "reside") != types.end()) {
			std::vector<unit*>& reside_troops = art.reside_troops();
			int index = 0;
			for (std::vector<unit*>::iterator itor = reside_troops.begin(); itor != reside_troops.end(); ++ itor, index ++) {
				unit& u = **itor;
				pairs.push_back(std::make_pair(index, &u));
			}
		}

	} else if (u_itor->is_artifical()) {
		artifical& art = *unit_2_artifical(&*u_itor);
		if (types.empty() || std::find(types.begin(), types.end(), "artifical") != types.end()) {
			pairs.push_back(std::make_pair(-1, &*u_itor));
		}
				
	} else {
		unit& u = *u_itor;
		if (types.empty() || std::find(types.begin(), types.end(), "field") != types.end()) {
			pairs.push_back(std::make_pair(-1, &u));
		}
	}
}

void team::card_touched(const card& c, const map_location& loc, std::vector<std::pair<int, unit*> >& pairs, std::string& disable_str)
{
	const config& cond_cfg = c.get_cfg().child("condition");
	const config& range_cfg = c.get_cfg().child("range");
	if (!range_cfg) {
		return;
	}
	unit* u_itor = NULL;
	std::vector<unit*> effected;

	const std::string hero_str = cond_cfg["type"].str();
	const std::vector<std::string>& types = utils::split(hero_str);
	
	if (range_cfg["self"].to_bool()) {
		u_itor = units_.find_unit(loc, true);
		if (c.target_hero()) {
			touched_heros_internal(u_itor, types, pairs);
		} else {
			touched_troops_internal(u_itor, types, pairs);
		}
		effected.push_back(&*u_itor);
	}
	std::vector<std::string> adjacent = utils::split(range_cfg["adjacent"]);
	for (std::vector<std::string>::const_iterator dir = adjacent.begin(); dir != adjacent.end(); ++ dir) {
		map_location::DIRECTION dir_i = map_location::parse_direction(*dir);
		if (dir_i == map_location::NDIRECTIONS) {
			continue;
		}
		map_location loc1 = loc.get_direction(dir_i);
		if (!condition_card(c, loc1)) {
			continue;
		}
		u_itor = units_.find_unit(loc1, true);
		if (std::find(effected.begin(), effected.end(), &*u_itor) != effected.end()) {
			continue;
		}
		if (c.target_hero()) {
			touched_heros_internal(u_itor, types, pairs);
		} else {
			touched_troops_internal(u_itor, types, pairs);
		}
		effected.push_back(&*u_itor);
	}
	disable_str = cond_cfg["disable"].str();
}
/*
card& team::holded_card(int index)
{
	if (index >= (int)holded_cards_.size()) {
		return card::null_card;
	}
	return cards_[holded_cards_[index]];
}
*/
const card& team::holded_card(int index) const
{
	if (index >= (int)holded_cards_.size()) {
		return card::null_card;
	}
	return cards_[holded_cards_[index]];
}

void team::find_treasure(hero_map& heros, play_controller& controller, int pos)
{
	const std::vector<int>& hide_treasures = controller.treasures();

	holded_treasures_.push_back(hide_treasures[pos]);
	controller.erase_treasure(pos);

	// card
	utils::string_map symbols;
	const ttreasure& t = unit_types.treasure(holded_treasures_.back());
	symbols["first"] = tintegrate::generate_format(t.name(), tintegrate::object_color);
	symbols["second"] = tintegrate::generate_format(hero::feature_str(t.feature()), tintegrate::tactic_color);
	game_events::show_hero_message(&heros[hero::number_scout], NULL, vgettext("Find treasure: $first($second)!", symbols), game_events::INCIDENT_CARD);
}

void team::select_ing_technology(const ttechnology* set)
{
	const std::map<std::string, ttechnology>& technologies = unit_types.technologies();
/*
	if (holded_technologies_.size() == technologies.size()) {
		return;
	}
*/
	if (set) {
		VALIDATE(std::find(holded_technologies_.begin(), holded_technologies_.end(), set) == holded_technologies_.end(), 
			"team::select_ing_technology, error! desired set holded technology: " + set->name());
		ing_technology_ = set;
	} else {
		int random = get_random();
		if (!half_technologies_.empty()) {
			std::map<const ttechnology*, int>::iterator it = half_technologies_.begin();
			std::advance(it, random % half_technologies_.size());
			ing_technology_ = it->first;
			return;
		}
		std::map<int, std::vector<const ttechnology*> > candidate;
		std::map<int, std::vector<const ttechnology*> >::iterator find;
		for (std::vector<const ttechnology*>::const_iterator it = holded_technologies_.begin(); it != holded_technologies_.end(); ++ it) {
			const ttechnology& t = **it;
			const std::vector<std::string>& advances_to = t.advances_to();
			for (std::vector<std::string>::const_iterator it2 = advances_to.begin(); it2 != advances_to.end(); ++ it2) {
				const ttechnology& t2 = technologies.find(*it2)->second;
				if (std::find(holded_technologies_.begin(), holded_technologies_.end(), &t2) == holded_technologies_.end()) {
					std::vector<const ttechnology*> v;
					int key = 0;
					if (technology_max_experience(t2) != t2.max_experience()) {
						key = 1;
					}
					find = candidate.find(key);
					if (find == candidate.end()) {
						v.push_back(&t2);
						candidate.insert(std::make_pair(key, v));
					} else {
						find->second.push_back(&t2);
					}
				}
			}
		}
		const std::vector<advance_tree::node*>& tree = unit_types.technology_tree();
		for (std::vector<advance_tree::node*>::const_iterator it = tree.begin(); it != tree.end(); ++ it) {
			const ttechnology* t = dynamic_cast<const ttechnology*>((*it)->current);
			if (std::find(holded_technologies_.begin(), holded_technologies_.end(), t) == holded_technologies_.end()) {
				std::vector<const ttechnology*> v;
				int key = 0;
				if (technology_max_experience(*t) != t->max_experience()) {
					key = 1;
				}
				find = candidate.find(key);
				if (find == candidate.end()) {
					v.push_back(t);
					candidate.insert(std::make_pair(key, v));
				} else {
					find->second.push_back(t);
				}
			}
		}
		if (candidate.empty()) {
			return;
		}
		find = candidate.find(1);
		if (find == candidate.end()) {
			find = candidate.find(0);
		}
		ing_technology_ = find->second[random % find->second.size()];
	}
}

void team::add_troop(unit* troop)
{
	field_troops_[field_troops_vsize_ ++] = troop;
}

void team::erase_troop(const unit* troop)
{
	size_t i;
	for (i = 0; i < field_troops_vsize_; i ++) {
		if (field_troops_[i] == troop) {
			break;
		}
	}
	if (i == field_troops_vsize_) {
		posix_print_mb("team::erase_troop, cann't find troop: 0x%p, hero: %u\n", troop, troop->master().number_);
		return;
	}
	if (i < field_troops_vsize_ - 1) {
		memcpy(&(field_troops_[i]), &(field_troops_[i + 1]), (field_troops_vsize_ - i - 1) * sizeof(unit*));
	}
	field_troops_vsize_ --;
}

void team::clear_troop()
{
	field_troops_vsize_ = 0;
}

int team::holden_troops() const
{
	int ret = 0;
	for (std::vector<artifical*>::const_iterator it = holden_cities_.begin(); it != holden_cities_.end(); ++ it) {
		const artifical& city = **it;
		ret += (int)(city.field_troops().size() + city.reside_troops().size());
	}
	return ret;
}

int team::cost_exponent() const
{
	if (tent::mode != mode_tag::RPG) {
		return 100;
	}
	// 1. holed hero count, 2: holded troops
	size_t total_heros = resources::heros->size();
	size_t holded_heros = 0;

	for (std::vector<artifical*>::const_iterator itor = holden_cities_.begin(); itor != holden_cities_.end(); ++ itor) {
		holded_heros += (*itor)->fresh_heros().size() + (*itor)->finish_heros().size() + ((*itor)->reside_troops().size() + (*itor)->field_troops().size()) * 3;
	}

	int ratio = holded_heros * 1000 / total_heros;
	if (ratio < 100) {
		ratio = 100;
	}
	return ratio;
}

void team::set_leader(hero& h)
{
	if (h.number_ == leader_) {
		return;
	}
	if (h.official_ == hero_official_mayor) {
		artifical* from = units_.city_from_cityno(h.city_);
		from->select_mayor(&hero_invalid);
	}
	leader()->official_ = HEROS_NO_OFFICIAL;

	const hero& predecessor = heros_[leader_];
	leader_ = h.number_;
	h.official_ = hero_official_leader;

	info_.name = heros_[leader_].name();

	// if side feature changed, readjust all unit
	if (predecessor.side_feature_ != h.side_feature_) {
		readjust_all_unit();
	}
}

int team::capital_number() const
{
	if (bh_eval.valid) {
		return bh_eval.capital;
	}
	return capital_? capital_->master().number_: HEROS_INVALID_NUMBER;
}

bool team::set_capital(artifical* city)
{
	if (city == capital_) {
		return false;
	}
	game_display* disp = resources::screen;

	artifical* original = capital_;

	// below resort map should use capital_.
	capital_ = city;

	std::vector<unit*> v;
	if (original) {
		v.push_back(original);
	}
	// requre resort all our city.
	for (std::vector<artifical*>::const_iterator it = holden_cities_.begin(); it != holden_cities_.end(); ++ it) {
		artifical* a = *it;
		if (a == original) {
			continue;
		}
		v.push_back(a);
	}
	units_.multi_resort_map(disp, v, true);

	return true;
}

int team::add_navigation(int increment)
{
	navigation_ += increment;
	if (navigation_ < 0) {
		navigation_ = 0;
	}
	return navigation_;
}

void team::increase_bomb_turns()
{
	bomb_turns_ ++;
	if (bomb_turns_ < 0) {
		bomb_turns_ = 0;
	} else if (bomb_turns_ > game_config::max_bomb_turns) {
		bomb_turns_ = game_config::max_bomb_turns;
	}
}

bool team::can_bomb() const
{
	return bomb_turns_ >= game_config::max_bomb_turns;
}

void team::add_modification_internal(int apply_to, const config& effect)
{
	if (apply_to == apply_to_tag::CIVILIZATION) {
		const std::string& civi = effect["civilization"];
		const std::string& increase = effect["increase"];
				
		int arms = unit_types.arms_from_id(civi);
		if (arms != -1) {
			arms_speed_[arms] = utils::apply_modifier(arms_speed_[arms], increase);
		} else if (civi == "leadership") {
			leadership_speed_ = utils::apply_modifier(leadership_speed_, increase);
		} else if (civi == "force") {
			force_speed_ = utils::apply_modifier(force_speed_, increase);
		} else if (civi == "intellect") {
			intellect_speed_ = utils::apply_modifier(intellect_speed_, increase);
		} else if (civi == "spirit") {
			spirit_speed_ = utils::apply_modifier(spirit_speed_, increase);
		} else if (civi == "charm") {
			charm_speed_ = utils::apply_modifier(charm_speed_, increase);
		} else if (civi == "navigation") {
			navigation_civi_increase_ = utils::apply_modifier(navigation_civi_increase_, increase);
		} else if (civi == "repair") {
			repair_increase_ = utils::apply_modifier(repair_increase_, increase);
		} else if (civi == "police") {
			village_gold_increase_ = utils::apply_modifier(village_gold_increase_, increase);
		} else if (civi == "commercial") {
			market_increase_ = utils::apply_modifier(market_increase_, increase);
		} else if (civi == "technology") {
			technology_increase_ = utils::apply_modifier(technology_increase_, increase);
		}
	} else if (apply_to == apply_to_tag::SPIRIT) {

	} else if (apply_to == apply_to_tag::STRATEGIC) {
		const std::string& strategic= effect["strategic"];
		const std::string& increase = effect["increase"];

		if (strategic == "tactic degree") {
			tactic_degree_increase = utils::apply_modifier(tactic_degree_increase, increase);
		} else if (strategic == "ticks") {
			ticks_increase = utils::apply_modifier(ticks_increase, increase);
			VALIDATE(ticks_increase >= 100, "team::add_modification_internal, ticks must not be decrease!");
		} else if (strategic == "balefire") {
			ignore_zoc_on_wall_ = true; 
		} else if (strategic == "sneak attack") {
			land_enemy_wall_ = true; 
		} else if (strategic == "interlink") {
			interlink_increase_ = utils::apply_modifier(interlink_increase_, increase);
		} else if (strategic == "cooperate") {
			cooperate_increase_ = utils::apply_modifier(cooperate_increase_, increase);
			if (!cooperate_increase_) cooperate_increase_ = 1;
		}

	} else if (apply_to == apply_to_tag::STRATAGEM) {
		int stratagem = stratagem_tag::find(effect["stratagem"].str());

		if (stratagem == stratagem_tag::xue_zhong_song_tan) {
			stratagem_ally_join = true;

		} else if (stratagem == stratagem_tag::miao_shou_hui_chun) {
			stratagem_half_heal = true;

		} else if (stratagem == stratagem_tag::xiao_li_cang_dao) {
			stratagem_half_kill = true;

		} else if (stratagem == stratagem_tag::sheng_dong_ji_xi) {
			stratagem_baffle_fightback = true;

		} else if (stratagem == stratagem_tag::jiang_lang_cai_jin) {
			stratagem_baffle_tactic = true;
		}
	}
}

void team::apply_holded_technologies_finish()
{
	for (std::vector<const ttechnology*>::const_iterator it = holded_technologies_.begin(); it != holded_technologies_.end(); ++ it) {
		const std::vector<const ttechnology*>& parts = (*it)->parts();
		for (std::vector<const ttechnology*>::const_iterator it2 = parts.begin(); it2 != parts.end(); ++ it2) {
			const ttechnology& t = **it2;
			if (t.occasion() == ttechnology::FINISH) {
				add_modification_internal(t.apply_to(), t.effect_cfg());
			}
		}
	}

	hero& h = heros_[leader_];
	if (h.character_ != HEROS_NO_CHARACTER) {
		const tcharacter& ch = unit_types.character(h.character_);
		for (std::vector<const tcharacter*>::const_iterator it = ch.parts().begin(); it != ch.parts().end(); ++ it) {
			const tcharacter& ch = **it;
			character_.insert(std::make_pair(ch.index(), ch.level(h)));
		}
	} else {
		character_.clear();
	}
}

void team::readjust_all_unit()
{
	// city
	const std::vector<artifical*>& cities = holden_cities();
	for (std::vector<artifical*>::const_iterator it = cities.begin(); it != cities.end(); ++ it) {
		artifical& city = **it;
		city.adjust();
		std::vector<unit*>& reside_troops = city.reside_troops();
		for (std::vector<unit*>::iterator it2 = reside_troops.begin(); it2 != reside_troops.end(); ++ it2) {
			unit& u = **it2;
			u.adjust();
		}
		std::vector<unit*>& reside_commoners = city.reside_commoners();
		for (std::vector<unit*>::iterator it2 = reside_commoners.begin(); it2 != reside_commoners.end(); ++ it2) {
			unit& u = **it2;
			u.adjust();
		}
	}
	// field troops/artifical
	const std::pair<unit**, size_t> p = field_troop();
	unit** troops = p.first;
	size_t troops_vsize = p.second;
	for (size_t i = 0; i < troops_vsize; i ++) {
		unit& u = *troops[i];
		u.adjust();
	}
}

// this function must be called after some variable parpared, info_.controller etc.
void team::calculate_strategy_according_to_mode()
{
	if (is_human() && tent::mode == mode_tag::TOWER) {
		restrict_movement = true;
	} else {
		restrict_movement = false;
	}

	if (is_human() && tent::mode != mode_tag::SIEGE && leader_ == rpg::h->number_) {
		auto_recruit = false;
	} else {
		auto_recruit = true;
	}

	if (tent::mode == mode_tag::SIEGE) {
		auto_move_human = true;
	} else {
		auto_move_human = false;
	}

	if (is_human() && tent::mode == mode_tag::TOWER) {
		support_bomb = true;
	} else {
		support_bomb = false;
	}

	if (tent::mode == mode_tag::TOWER || tent::mode == mode_tag::SIEGE) {
		kill_income = true;
	} else {
		kill_income = false;
	}

	if (tent::mode == mode_tag::SIEGE || tent::mode == mode_tag::LAYOUT) {
		max_troops = game_config::max_siege_troops;
	}

	if (tent::mode == mode_tag::SIEGE) {
		ea_artifical_neutral = true;
	}

	if (is_human() && tent::mode != mode_tag::SIEGE) {
		double_reset_goto = true;
	} else {
		double_reset_goto = false;
	}

	if (is_human() && tent::mode == mode_tag::TOWER) {
		allow_active = true;
	} else {
		allow_active = false;
	}

	if (is_human() && tent::mode == mode_tag::SIEGE) {
		allow_intervene = true;
	} else {
		allow_intervene = false;
	}

	if (is_human() && tent::mode == mode_tag::SIEGE) {
		stratagem_decrease_cost = true;
	}
}

void team::fresh_to_field_troop(int random)
{
	if (holden_cities_.empty()) {
		return;
	}
	artifical& city = *holden_cities_.front();
	std::vector<hero*>& freshes = city.fresh_heros();
	std::vector<unit*> candidate_fields = city.field_troops();
	std::vector<hero*> captains;

	while (!freshes.empty() && !candidate_fields.empty()) {
		std::vector<unit*>::iterator it = candidate_fields.begin();
		std::advance(it, random % candidate_fields.size());
		unit& u = **it;

		if (!u.third().valid()) {
			captains.clear();
			captains.push_back(&u.master());
			if (u.second().valid()) {
				captains.push_back(&u.second());
			}

			hero& h = *freshes.front();
			captains.push_back(&h);
			u.replace_captains(captains);
			city.hero_go_out(h);
		}
		if (u.third().valid()) {
			candidate_fields.erase(it);
		}
	}
}

bool team::all_city_deputed(const artifical* exclude) const
{
	for (std::vector<artifical*>::const_iterator it = holden_cities_.begin(); it != holden_cities_.end(); ++ it) {
		const artifical* city = *it;
		if (city == exclude) {
			continue;
		}
		if (city->get_state(ustate_tag::DEPUTE)) {
			continue;
		}
		return false;
	}
	return true;
}

void team::set_all_city_deputed(game_display& disp, bool set) const
{
	for (std::vector<artifical*>::const_iterator it = holden_cities_.begin(); it != holden_cities_.end(); ++ it) {
		artifical* city = *it;
		if (city->get_state(ustate_tag::DEPUTE) != set) {
			city->set_state(ustate_tag::DEPUTE, set);

			disp.refresh_access_troops(city->side() - 1, game_display::REFRESH_DRAW, (unit*)city);
		}
	}
}

void shrouded_and_fogged(const map_location& loc, const void* t, bool& shrouded, bool& fogged)
{
	const team* tm = reinterpret_cast<const team*>(t);
	shrouded = tm->shrouded(loc);
	// shrouded hex are not considered fogged (no need to fog a black image)
	fogged = !shrouded && tm->fogged(loc);
}

namespace player_teams {
int village_owner(const map_location& loc)
{
	if(! teams) {
		return -1;
	}
	for(size_t i = 0; i != teams->size(); ++i) {
		if((*teams)[i].owns_village(loc))
			return i;
	}
	return -1;
}
}
