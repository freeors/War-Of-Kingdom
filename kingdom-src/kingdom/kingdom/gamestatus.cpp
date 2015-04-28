//* $Id: gamestatus.cpp 47504 2010-11-07 20:16:07Z silene $ */
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
 * @file
 * Maintain status of a game, load&save games.
 */

#include "global.hpp"
#include "config.hpp"

#include "gamestatus.hpp"

#include "actions.hpp"
#include "gettext.hpp"
#include "log.hpp"
#include "game_preferences.hpp"
#include "replay.hpp"
#include "resources.hpp"
#include "statistics.hpp"
#include "team.hpp"
#include "unit.hpp"
#include "wml_exception.hpp"
#include "formula_string_utils.hpp"
#include "map.hpp"
#include "pathfind/pathfind.hpp"
#include "artifical.hpp"
#include "resources.hpp"

#include <boost/foreach.hpp>
#include <boost/bind.hpp>

#ifndef _MSC_VER
#include <sys/time.h>
#endif

static lg::log_domain log_engine("engine");
#define ERR_NG LOG_STREAM(err, log_engine)
#define WRN_NG LOG_STREAM(warn, log_engine)
#define LOG_NG LOG_STREAM(info, log_engine)
#define DBG_NG LOG_STREAM(debug, log_engine)

static lg::log_domain log_engine_tc("engine/team_construction");
#define ERR_NG_TC LOG_STREAM(err, log_engine_tc)
#define WRN_NG_TC LOG_STREAM(warn, log_engine_tc)
#define LOG_NG_TC LOG_STREAM(info, log_engine_tc)
#define DBG_NG_TC LOG_STREAM(debug, log_engine_tc)

game_classification::game_classification():
	savegame_config()
	, label()
	, original_label()
	, parent()
	, version()
	, campaign_type()
	, campaign_define()
	, campaign()
	, history()
	, abbrev()
	, mode()
	, scenario()
	, next_scenario()
	, completion()
	, end_text()
	, end_text_duration()
	, create(0)
	, duration(0)
	, hash(0)
	{}

game_classification::game_classification(const config& cfg):
	savegame_config()
	, label(cfg["label"])
	, original_label(cfg["original_label"])
	, parent(cfg["parent"])
	, version(cfg["version"])
	, campaign_type(cfg["campaign_type"].empty() ? "scenario" : cfg["campaign_type"].str())
	, campaign_define(cfg["campaign_define"])
	, campaign(cfg["campaign"])
	, history(cfg["history"])
	, abbrev(cfg["abbrev"])
	, mode(cfg["mode"])
	, scenario(cfg["scenario"])
	, next_scenario(cfg["next_scenario"])
	, completion(cfg["completion"])
	, end_text(cfg["end_text"])
	, end_text_duration(cfg["end_text_duration"])
	, create(cfg["create"].to_long())
	, duration(cfg["duration"].to_int())
	, hash(cfg["hash"].to_int())
	{}

game_classification::game_classification(const game_classification& gc):
	savegame_config()
	, label(gc.label)
	, original_label(gc.original_label)
	, parent(gc.parent)
	, version(gc.version)
	, campaign_type(gc.campaign_type)
	, campaign_define(gc.campaign_define)
	, campaign(gc.campaign)
	, history(gc.history)
	, abbrev(gc.abbrev)
	, mode(gc.mode)
	, scenario(gc.scenario)
	, next_scenario(gc.next_scenario)
	, completion(gc.completion)
	, end_text(gc.end_text)
	, end_text_duration(gc.end_text_duration)
	, create(gc.create)
	, duration(gc.duration)
	, hash(gc.hash)
{
}

config game_classification::to_config() const
{
	config cfg;

	cfg["label"] = label;
	cfg["original_label"] = original_label;
	cfg["abbrev"] = abbrev;
	cfg["mode"] = mode;
	cfg["version"] = version;
	cfg["scenario"] = scenario;
	cfg["next_scenario"] = next_scenario;
	cfg["completion"] = completion;
	cfg["campaign"] = campaign;
	cfg["campaign_type"] = campaign_type;
	cfg["create"] = create;
	// cfg["duration"] = _duration;
	cfg["hash"] = hash;
	cfg["campaign_define"] = campaign_define;
	cfg["end_text"] = end_text;
	cfg["end_text_duration"] = str_cast<unsigned int>(end_text_duration);

	return cfg;
}

#ifdef __UNUSED__
std::string generate_game_uuid()
{
	struct timeval ts;
	std::stringstream uuid;
	gettimeofday(&ts, NULL);

	uuid << preferences::login() << "@" << ts.tv_sec << "." << ts.tv_usec;

	return uuid.str();
}
#endif

game_state::game_state()  :
		scoped_variables(),
		wml_menu_items(),
		replay_data(),
		starting_pos(),
		start_hero_data_(NULL),
		session_start_(0),
		snapshot(),
		last_selected(map_location::null_location),
		rng_(),
		variables_(),
		temporaries_(),
		generator_setter_(&recorder),
		classification_(),
		mp_settings_(),
		phase_(INITIAL)
		{}

void level_to_gamestate(config& level, command_pool& replay_data, game_state& state, const std::string& campaign_type)
{
	// any replay data is only temporary and should be removed from
	// the level data in case we want to save the game later
	if (replay_data.pool_pos_vsize()) {
		state.replay_data = replay_data;
		recorder = replay(state.replay_data);
		if (!recorder.empty()) {
			recorder.set_skip(false);
			recorder.set_to_end();
		}
	}

	// set random
	const std::string seed = level["random_seed"];
	VALIDATE(!seed.empty(), "No random seed found, random events will probably be out of sync.");

	const unsigned calls = lexical_cast_default<unsigned>(level["random_calls"]);
	state.rng().seed_random(lexical_cast<int>(seed), calls);
	
	//adds the starting pos to the level
	if (!level.has_attribute("turn_based")) {
		level["turn_based"] = tent::turn_based;
	}
	if (!level.child("replay_start")) {
		level.add_child("replay_start", level);
		level.child("replay_start").remove_child("multiplayer", 0);
	}
	//this is important, if it does not happen, the starting position is missing and
	//will be drawn from the snapshot instead (which is not what we want since we have
	//all needed information here already)
	state.starting_pos = level.child("replay_start");

	level["campaign_type"] = campaign_type;
	state.classification().campaign_type = campaign_type;
	state.classification().completion = level["completion"].str();
	state.classification().version = level["version"].str();
	
	if (const config &vars = level.child("variables")) {
		state.set_variables(vars);
	}
	state.set_menu_items(level.child_range("menu_item"));
	state.mp_settings().set_from_config(level);

	//Check whether it is a save-game by looking for snapshot data
	const config &snapshot = level.child("snapshot");
	const bool saved_game = snapshot;

	//If we start a fresh game, there won't be any snapshot information. If however this
	//is a savegame, we got a valid snapshot here.
	if (saved_game) {
		state.snapshot = snapshot;
		if (const config &v = snapshot.child("variables")) {
			state.set_variables(v);
		}
		state.set_menu_items(snapshot.child_range("menu_item"));
	}

	// why "+ 1". play_controller may be append one "empty" side.
	// create one dummy param according to this "empty" side;
	tent::lobby_side_params.resize(level.child_count("side") + 1);
	tent::lobby_side_params.back().current_player = "";
	tent::lobby_side_params.back().ally = "";
	config::const_child_itors level_sides = level.child_range("side");
	BOOST_FOREACH (const config &lside, level_sides) {
		tent::tlobby_side_param& param = tent::lobby_side_params[lside["side"].to_int() - 1];

		param.current_player = lside["current_player"].str();
		param.ally = lside["team_name"].str();
	}
}

game_state::game_state(const config& cfg, const command_pool& replay_data, bool show_replay) :
		scoped_variables(),
		wml_menu_items(),
		replay_data(replay_data),
		starting_pos(),
		start_hero_data_(NULL),
		session_start_(0),
		snapshot(),
		last_selected(map_location::null_location),
		rng_(cfg),
		variables_(),
		temporaries_(),
		generator_setter_(&recorder),
		classification_(cfg),
		mp_settings_(cfg),
		phase_(INITIAL)
{
	log_scope("read_game");

	const config &snapshot = cfg.child("snapshot");
	const config &replay_start = cfg.child("replay_start");
	// We're loading a snapshot if we have it and the user didn't request a replay.
	bool load_snapshot = !show_replay && snapshot && !snapshot.empty();

	if (load_snapshot) {
		this->snapshot = snapshot;

		rng_.seed_random(snapshot["random_calls"]);
	} else {
		assert(replay_start);
	}

	LOG_NG << "scenario: '" << classification_.scenario << "'\n";
	LOG_NG << "next_scenario: '" << classification_.next_scenario << "'\n";

	//priority of populating wml variables:
	//snapshot -> replay_start -> root
	if (load_snapshot) {
		if (const config &vars = cfg.child("variables")) {
			set_variables(vars);
		}
	}
	else if (const config &vars = replay_start.child("variables")) {
		set_variables(vars);
	}
	else if (const config &vars = cfg.child("variables")) {
		set_variables(vars);
	}
	set_menu_items(cfg.child_range("menu_item"));

	if (replay_start) {
		starting_pos = replay_start;
		//This is a quick hack to make replays for campaigns work again:
		//The [player] information needs to be stored somewhere within the gamestate,
		//because we need it later on when creating the replay savegame.
		//We therefore put it inside the starting_pos, so it doesn't get lost.
		//See also playcampaign::play_game, where after finishing the scenario the replay
		//will be saved.
		if(!starting_pos.empty()) {
			BOOST_FOREACH (const config &p, cfg.child_range("player")) {
				config& cfg_player = starting_pos.add_child("player");
				cfg_player.merge_with(p);
			}
		}
	}
/*
	if (const config &stats = cfg.child("statistics")) {
		statistics::fresh_stats();
		statistics::read_stats(stats);
	}
*/
}

void game_state::write_snapshot(config& cfg) const
{
	cfg["label"] = classification_.label;
	cfg["history"] = classification_.history;
	cfg["abbrev"] = classification_.abbrev;
	cfg["mode"] = classification_.mode;
	cfg["version"] = game_config::version;

	cfg["scenario"] = classification_.scenario;
	cfg["next_scenario"] = classification_.next_scenario;

	cfg["completion"] = classification_.completion;

	cfg["campaign"] = classification_.campaign;
	cfg["campaign_type"] = classification_.campaign_type;
	cfg["create"] = classification_.create;
	cfg["duration"] = classification_.duration;
	cfg["hash"] = classification_.hash;

	cfg["campaign_define"] = classification_.campaign_define;

	cfg["random_seed"] = rng_.get_random_seed();
	cfg["random_calls"] = rng_.get_random_calls();

	cfg["end_text"] = classification_.end_text;
	cfg["end_text_duration"] = str_cast<unsigned int>(classification_.end_text_duration);

	// cfg.add_child("variables", variables);

	for(std::map<std::string, wml_menu_item *>::const_iterator j=wml_menu_items.begin();
	    j!=wml_menu_items.end(); ++j) {
		config new_cfg;
		new_cfg["id"]=j->first;
		new_cfg["image"]=j->second->image;
		new_cfg["description"]=j->second->description;
		new_cfg["needs_select"] = j->second->needs_select;
		if(!j->second->show_if.empty())
			new_cfg.add_child("show_if", j->second->show_if);
		if(!j->second->filter_location.empty())
			new_cfg.add_child("filter_location", j->second->filter_location);
		if(!j->second->command.empty())
			new_cfg.add_child("command", j->second->command);
		cfg.add_child("menu_item", new_cfg);
	}
}

config::attribute_value &game_state::get_variable(const std::string& key)
{
	return variable_info(key, true, variable_info::TYPE_SCALAR).as_scalar();
}

config::attribute_value game_state::get_variable_const(const std::string &key) const
{
	variable_info to_get(key, false, variable_info::TYPE_SCALAR);
	if (!to_get.is_valid)
	{
		config::attribute_value &to_return = temporaries_[key];
		if (key.size() > 7 && key.substr(key.size() - 7) == ".length") {
			// length is a special attribute, so guarantee its correctness
			to_return = 0;
		}
		return to_return;
	}
	return to_get.as_scalar();
}

config& game_state::get_variable_cfg(const std::string& key)
{
	return variable_info(key, true, variable_info::TYPE_CONTAINER).as_container();
}

void game_state::set_variable(const std::string& key, const t_string& value)
{
	get_variable(key) = value;
}

config& game_state::add_variable_cfg(const std::string& key, const config& value)
{
	variable_info to_add(key, true, variable_info::TYPE_ARRAY);
	return to_add.vars->add_child(to_add.key, value);
}

void game_state::clear_variable_cfg(const std::string& varname)
{
	variable_info to_clear(varname, false, variable_info::TYPE_CONTAINER);
	if(!to_clear.is_valid) return;
	if(to_clear.explicit_index) {
		to_clear.vars->remove_child(to_clear.key, to_clear.index);
	} else {
		to_clear.vars->clear_children(to_clear.key);
	}
}

void untouch_unit(unit& u)
{
	u.new_turn(*resources::controller, 0);
	u.set_state(ustate_tag::SLOWED, false);
	u.set_state(ustate_tag::BROKEN, false);
	u.set_state(ustate_tag::POISONED, false);
	u.set_state(ustate_tag::PETRIFIED, false);
	if (u.packed()) {
		u.pack_to(NULL);
	}
}

void game_state::write_armory_2_variable(const team& t)
{
	config& player_cfg = variables_.child("player");
	config& side = player_cfg.add_child("armory");
	std::stringstream armory_heros;

	dont_wander_lock lock;

	// current visible units
	const std::pair<unit**, size_t> p = t.field_troop();
	unit** troops = p.first;
	size_t troops_vsize = p.second;
	for (size_t i = 0; i < troops_vsize; i ++) {
		unit& u = *troops[i];
		if (!u.is_artifical()) {
			// field troops
			config& ucfg = side.add_child("unit");
			
			untouch_unit(u);
			u.get_location().write(ucfg);
			u.write(ucfg);
		}
	}
	const std::vector<artifical*>& holden_cities = t.holden_cities();
	for (std::vector<artifical*>::const_iterator i = holden_cities.begin(); i != holden_cities.end(); ++ i) {
		artifical* city = *i;

		std::vector<unit*>& reside_troops = city->reside_troops();
		for (std::vector<unit*>::const_iterator itor = reside_troops.begin(); itor != reside_troops.end(); ++ itor) {
			// reside troops
			unit& u = **itor;
			config& ucfg = side.add_child("unit");

			untouch_unit(u);
			u.get_location().write(ucfg);
			u.write(ucfg);
		}

		std::vector<hero*>& fresh_heros = city->fresh_heros();
		for (std::vector<hero*>::const_iterator h = fresh_heros.begin(); h != fresh_heros.end(); ++ h) {
			if (!armory_heros.str().empty()) {
				armory_heros << ", ";
			}
			armory_heros << lexical_cast<std::string>((*h)->number_);
		}
		std::vector<hero*>& finish_heros = city->finish_heros();
		for (std::vector<hero*>::const_iterator h = finish_heros.begin(); h != finish_heros.end(); ++ h) {
			if (!armory_heros.str().empty()) {
				armory_heros << ", ";
			}
			armory_heros << lexical_cast<std::string>((*h)->number_);
		}
		std::vector<hero*>& wander_heros = city->wander_heros();
		for (std::vector<hero*>::const_iterator h = wander_heros.begin(); h != wander_heros.end(); ++ h) {
			if (!armory_heros.str().empty()) {
				armory_heros << ", ";
			}
			armory_heros << lexical_cast<std::string>((*h)->number_);
		}
	}
	side["heros"] = armory_heros.str();
}

void game_state::rpg_2_variable()
{
	config* player_cfg = NULL;
	if (variables_.child_count("player")) {
		player_cfg = &variables_.child("player");
	} else {
		player_cfg = &variables_.add_child("player");
	}
	(*player_cfg)["stratum"] = rpg::stratum;
}

void game_state::clear_start_hero_data()
{
	if (start_hero_data_) {
		free(start_hero_data_);
		start_hero_data_ = NULL;
	}
}

void game_state::clear_variable(const std::string& varname)
{
	variable_info to_clear(varname, false);
	if(!to_clear.is_valid) return;
	if(to_clear.explicit_index) {
		to_clear.vars->remove_child(to_clear.key, to_clear.index);
	} else {
		to_clear.vars->clear_children(to_clear.key);
		to_clear.vars->remove_attribute(to_clear.key);
	}
}

static void clear_wmi(std::map<std::string, wml_menu_item *> &gs_wmi)
{
	for (std::map<std::string, wml_menu_item *>::iterator i = gs_wmi.begin(),
	     i_end = gs_wmi.end(); i != i_end; ++i)
	{
		delete i->second;
	}
	gs_wmi.clear();
}

game_state::game_state(const game_state& state) :
	/* default construct everything to silence compiler warnings. */
	variable_set(),
	scoped_variables(),
	wml_menu_items(),
	replay_data(),
	starting_pos(),
	snapshot(),
	last_selected(),
	rng_(),
	variables_(),
	temporaries_(),
	generator_setter_(&recorder),
	classification_(),
	mp_settings_(),
	phase_(INITIAL)
{
	*this = state;
	start_hero_data_ = NULL;
}

game_state& game_state::operator=(const game_state& state)
{
	if(this == &state) {
		return *this;
	}

	rng_ = state.rng_;
	scoped_variables = state.scoped_variables;
	classification_ = game_classification(state.classification());
	mp_settings_ = mp_game_settings(state.mp_settings());

	clear_wmi(wml_menu_items);
	std::map<std::string, wml_menu_item*>::const_iterator itor;
	for (itor = state.wml_menu_items.begin(); itor != state.wml_menu_items.end(); ++itor) {
		wml_menu_item*& mref = wml_menu_items[itor->first];
		mref = new wml_menu_item(*(itor->second));
	}

	replay_data = state.replay_data;
	starting_pos = state.starting_pos;
	snapshot = state.snapshot;
	last_selected = state.last_selected;
	variables_ = state.variables_;

	return *this;
}

game_state::~game_state() 
{
	clear_wmi(wml_menu_items);
	if (start_hero_data_) {
		free(start_hero_data_);
	}
}

void game_state::set_variables(const config& vars) {
	variables_ = vars;
}

class bh_eval_lock
{
public:
	bh_eval_lock(team& t)
		: t_(t)
	{
		t_.bh_eval.valid = true;
	}
	~bh_eval_lock()
	{
		t_.bh_eval.valid = false;
	}

private:
	team& t_;
};

void game_state::build_team(const config& side_cfg,
					 std::vector<team>& teams,
					 const config& level, gamemap& map, unit_map& units, hero_map& heros,
					 card_map& cards,
					 bool snapshot,
					 size_t team_size)
{
	const config *player_cfg = NULL;
	//track whether a [player] tag with persistence information exists (in addition to the [side] tag)
	bool player_exists = false;

	if (map.empty()) {
		throw game::load_game_failed("Map not found");
	}

	if (side_cfg["controller"] == "human" ||
		side_cfg["controller"] == "network" ||
		side_cfg["controller"] == "network_ai" ||
		side_cfg["controller"] == "human_ai" ||
		utils::string_bool(side_cfg["persistent"])) {

		player_exists = true;
	}

	LOG_NG << "initializing team...\n";

	int ngold = side_cfg["gold"].to_int(100);

	/* This is the gold carry-over mechanism for subsequent campaign
	scenarios. Snapshots and replays are loaded from savegames and
	got their own gold information, which must not be altered here
	*/
	bool gold_add = utils::string_bool((side_cfg)["gold_add"]);
	if ( (player_cfg != NULL)  && (!snapshot) ) {
		try {
			int player_gold = lexical_cast_default<int>((*player_cfg)["gold"]);
			if (!player_exists) {
				//if we get the persistence information from [side], carryover gold is already sorted
				ngold = player_gold;
				gold_add = utils::string_bool((*player_cfg)["gold_add"]);
			} else if(utils::string_bool((*player_cfg)["gold_add"])) {
				ngold +=  player_gold;
				gold_add = true;
			} else if(player_gold >= ngold) {
				ngold = player_gold;
			}
		} catch (config::error&) {
			
		}			
	}

	team temp_team(units, heros, cards, side_cfg, map, ngold, team_size);
	if (temp_team.side() != teams.size() + 1) {
		std::stringstream err;
		err << "side#" << (teams.size() + 1) << "'s side value is mistaken. error value: " << temp_team.side();
		throw game::load_game_failed(err.str());
	}
	temp_team.set_fog(level["fog"].to_bool());
	temp_team.set_shroud(level["shroud"].to_bool());
	temp_team.set_gold_add(gold_add);
	teams.push_back(temp_team);
	team& t = teams.back();
	bh_eval_lock lock(t);

	// Update/fix the recall list for this side,
	// by setting the "side" of each unit in it
	// to be the "side" of the player.
	int side = side_cfg["side"].to_int(1);

	// If this team has no objectives, set its objectives
	// to the level-global "objectives"
	if (t.objectives().empty()) {
		t.set_objectives(level["objectives"]);
	}


	//
	// artifical(city, market...)
	// !!!first must is artifical, then troops
	//

	config& player_cfg1 = variables_.child("player");
	bool has_armory = (player_cfg1 && player_cfg1.child("armory"))? true: false;

	// If there are additional starting artificals on this side
	//add the units with a specified position to the unit map
	BOOST_FOREACH (const config& cfg, side_cfg.child_range("artifical")) {
		config temp_cfg(cfg);

		if (has_armory && teams.back().is_human()) {
			config& armory_cfg = player_cfg1.child("armory");

			// add [player].[armory] to this team
			BOOST_FOREACH (config &i, armory_cfg.child_range("unit")) {
				i["cityno"] = cfg["cityno"];
				i["side"] = cfg["side"];
				temp_cfg.add_child("unit", i);
			}
			std::string service_heros = temp_cfg["service_heros"];
			if (service_heros.empty()) {
				service_heros = armory_cfg["heros"].str();
			} else {
				service_heros = service_heros + ", " + armory_cfg["heros"];
			}
			temp_cfg["service_heros"] = service_heros;
			player_cfg1.clear_children("armory");
		}

		temp_cfg["side"] = side; //set the side before unit creation to avoid invalid side errors
		// why use state
		// in multiplayer, it is necessary to sync players.
		artifical new_unit(*this, temp_cfg);

		const std::string x = cfg["x"].str();
		const std::string y = cfg["y"].str();

		map_location loc(cfg, this);
		if (!loc.valid()) {
			loc = map.starting_position(side);
		}
		if(x.empty() && y.empty()) {
			//if there is no player tag, this means this team is either not persistent or the units have been added from a [side] tag earlier
		} else if(!loc.valid() || !map.on_board(loc)) {
			throw game::load_game_failed(
				"Invalid starting position (" +
				lexical_cast<std::string>(loc.x+1) +
				"," + lexical_cast<std::string>(loc.y+1) +
				") for a city on side " +
				lexical_cast<std::string>(side) + ".");
		} else {
			std::set<map_location> touch_locs = new_unit.get_touch_locations2(map, loc);
			for (std::set<map_location>::const_iterator it = touch_locs.begin(); it != touch_locs.end(); ++ it) {
				if (units.find(*it) != units.end()) {
					unit_map::iterator find = units.find(*it, !new_unit.base());
					std::stringstream str;
					str << "City " << new_unit.name() << " trying to overwrite existing unit " << find->name() << " at (";
					str << loc.x + 1 << "," << loc.y + 1 << ")";
					throw game::load_game_failed(str.str());
				}
			}
			units.add(loc, &new_unit);
		}
	}

	//
	// field troops
	//
	// If there are additional starting units on this side
		// the recall list has been filled by loading the [player]-section already.
	// However, we need to get the information from the snapshot,
	// so we start from scratch here.
	// This is rather a quick hack, originating from keeping changes
	// as minimal as possible for 1.2.

	//add the units with a specified position to the unit map
	BOOST_FOREACH (const config& cfg, side_cfg.child_range("unit")) {
		config temp_cfg(cfg);
		temp_cfg["side"] = side; //set the side before unit creation to avoid invalid side errors
		unit new_unit(units, heros, teams, *this, temp_cfg, true);

		const std::string x = cfg["x"].str();
		const std::string y = cfg["y"].str();

		map_location loc(cfg, this);
		if(x.empty() && y.empty()) {
			//if there is no player tag, this means this team is either not persistent or the units have been added from a [side] tag earlier
		} else if(!loc.valid() || !map.on_board(loc)) {
			throw game::load_game_failed(
				"Invalid starting position (" +
				lexical_cast<std::string>(loc.x+1) +
				"," + lexical_cast<std::string>(loc.y+1) +
				") for a unit on side " +
				lexical_cast<std::string>(side) + ".");
		} else {
			if (map.is_village(loc)) {
				std::stringstream str;
				str << new_unit.name() << " trying to stand on village at (";
				str << loc.x + 1 << "," << loc.y + 1 << ")";
				throw game::load_game_failed(str.str());
			} else if (units.find(loc, !new_unit.base()) != units.end()) {
				unit_map::iterator it = units.find(loc, !new_unit.base());
				std::stringstream str;
				str << new_unit.name() << " trying to overwrite existing unit " << it->name() << " at (";
				str << loc.x + 1 << "," << loc.y + 1 << ")";
				throw game::load_game_failed(str.str());
			} else {
				units.add(loc, &new_unit);
			}
		}
	}

	if (t.bh_eval.capital != HEROS_INVALID_NUMBER) {
		t.set_capital(unit_2_artifical(units.find_unit(heros[t.bh_eval.capital])));
	}
}

void game_state::build_team(const uint8_t* mem,
					 std::vector<team>& teams,
					 const config& level, gamemap& map, unit_map& units, hero_map& heros,
					 card_map& cards,
					 bool snapshot, size_t team_size)
{
	if (map.empty()) {
		throw game::load_game_failed("Map not found");
	}

	const unit_segment2* total = (const unit_segment2*)mem;
	int offset = sizeof(unit_segment2);

	int ngold = ((team_fields_t*)(mem + offset))->gold_;

	team temp_team(units, heros, cards, mem + offset, map, ngold, team_size);
	offset += ((team_fields_t*)(mem + offset))->size_;

	temp_team.set_gold_add(0);

	teams.push_back(temp_team);

	team& t = teams.back();
	bh_eval_lock lock(t);

	int side = t.side();

	// If this team has no objectives, set its objectives
	// to the level-global "objectives"
	if (t.objectives().empty()) {
		t.set_objectives(level["objectives"]);
	}

	//
	// artifical(city, market...)
	// !!!first must is artifical, then troops
	//

	config& player_cfg1 = variables_.child("player");
	bool has_armory = (player_cfg1 && player_cfg1.child("armory"))? true: false;

	// If there are additional starting artificals on this side
	// add the units with a specified position to the unit map
	for (uint32_t i = 0; i < total->count_; i ++) {
		unit_fields_t* fields = (unit_fields_t*)(mem + offset);

		unit* new_unit;

		if (fields->artifical_) {
			new_unit = new artifical(mem + offset);
		} else {
			new_unit = new unit(units, heros, teams, mem + offset);
		}
		offset += fields->size_;

		map_location loc(fields->x_, fields->y_);

		if (!loc.valid()) {
			loc = map.starting_position(side);
		}
		if (!loc.valid() || !map.on_board(loc)) {
			delete new_unit;
			throw game::load_game_failed(
				"Invalid starting position (" +
				lexical_cast<std::string>(loc.x+1) +
				"," + lexical_cast<std::string>(loc.y+1) +
				") for a city on side " +
				lexical_cast<std::string>(side) + ".");
		} else {
			std::set<map_location> touch_locs = new_unit->get_touch_locations2(map, loc);
			for (std::set<map_location>::const_iterator it = touch_locs.begin(); it != touch_locs.end(); ++ it) {
				if (units.find(*it, !new_unit->base()) != units.end()) {
					unit_map::iterator find = units.find(*it, !new_unit->base());
					std::stringstream str;
					str << new_unit->name() << " trying to overwrite existing unit " << find->name() << " at (";
					str << loc.x + 1 << "," << loc.y + 1 << ")";
					delete new_unit;
					throw game::load_game_failed(str.str());
				}
			}
			units.add(loc, new_unit);
		}
		delete new_unit;

		if (fields->artifical_ == 2 && has_armory && teams.back().is_human()) {
			artifical* current = units.city_from_loc(loc);

			config& armory_cfg = player_cfg1.child("armory");

			// add [player].[armory] to this team

			// reside troops
			std::vector<unit*>& reside_troops = current->reside_troops();
			BOOST_FOREACH (config &i, armory_cfg.child_range("unit")) {
				i["cityno"] = current->cityno();
				i["side"] = current->side();
				reside_troops.push_back(new unit(units, heros, teams, *this, i));
			}
			// fresh heros
			std::vector<hero*>& freshes = current->fresh_heros();
			const std::vector<std::string> fresh_heros = utils::split(armory_cfg["heros"].str());
			std::vector<std::string>::const_iterator tmp;
			for (tmp = fresh_heros.begin(); tmp != fresh_heros.end(); ++ tmp) {
				hero& h = heros[lexical_cast_default<int>(*tmp)];
				h.status_ = hero_status_idle;
				h.city_ = current->cityno();
				h.side_ = current->side() - 1;
				freshes.push_back(&h);
			}
			// clear
			player_cfg1.clear_children("armory");
			has_armory = false;
		}
	}

	if (t.bh_eval.capital != HEROS_INVALID_NUMBER) {
		t.set_capital(unit_2_artifical(units.find_unit(heros[t.bh_eval.capital])));
	}
}

void game_state::set_session_start(time_t t)
{
	session_start_ = t;
}

int game_state::duration(time_t stop) const
{
	return classification_.duration + stop - session_start_;
}

void game_state::set_menu_items(const config::const_child_itors &menu_items)
{
	clear_wmi(wml_menu_items);
	BOOST_FOREACH (const config &item, menu_items)
	{
		std::string id = item["id"];
		wml_menu_item*& mref = wml_menu_items[id];
		if(mref == NULL) {
			mref = new wml_menu_item(id, &item);
		} else {
			WRN_NG << "duplicate menu item (" << id << ") while loading gamestate\n";
		}
	}
}

void game_state::write_config(config_writer& out, bool write_variables) const
{
	out.write(classification_.to_config());
	if (classification_.campaign_type == "multiplayer")
		out.write_child("multiplayer", mp_settings_.to_config());
	out.write_key_val("random_seed", lexical_cast<std::string>(rng_.get_random_seed()));
	out.write_key_val("random_calls", lexical_cast<std::string>(rng_.get_random_calls()));
	if (write_variables) {
		out.write_child("variables", variables_);
	}

	for(std::map<std::string, wml_menu_item *>::const_iterator j = wml_menu_items.begin();
	    j != wml_menu_items.end(); ++j) {
		out.open_child("menu_item");
		out.write_key_val("id", j->first);
		out.write_key_val("image", j->second->image);
		out.write_key_val("description", j->second->description);
		out.write_key_val("needs_select", (j->second->needs_select) ? "yes" : "no");
		if(!j->second->show_if.empty())
			out.write_child("show_if", j->second->show_if);
		if(!j->second->filter_location.empty())
			out.write_child("filter_location", j->second->filter_location);
		if(!j->second->command.empty())
			out.write_child("command", j->second->command);
		out.close_child("menu_item");
	}

	out.write_child("replay_start",starting_pos);
}

wml_menu_item::wml_menu_item(const std::string& id, const config* cfg) :
		name(),
		image(),
		description(),
		needs_select(false),
		show_if(),
		filter_location(),
		command()

{
	std::stringstream temp;
	temp << "menu item";
	if(!id.empty()) {
		temp << ' ' << id;
	}
	name = temp.str();
	if(cfg != NULL) {
		image = (*cfg)["image"].str();
		description = (*cfg)["description"];
		needs_select = (*cfg)["needs_select"].to_bool();
		if (const config &c = cfg->child("show_if")) show_if = c;
		if (const config &c = cfg->child("filter_location")) filter_location = c;
		if (const config &c = cfg->child("command")) command = c;
	}
}

std::vector<std::set<int> > generate_team_names_from_side_mem(uint8_t* mem)
{
	unit_segment2* sides = (unit_segment2*)mem;
	int offset = sizeof(unit_segment2);

	std::vector<std::set<int> > ret;
	for (uint32_t i = 0; i < sides->count_; i ++) {
		int side_offset = sizeof(unit_segment2);
		uint8_t* side_mem = mem + offset + side_offset;

		team_fields_t* fields = (team_fields_t*)side_mem;
		std::set<int> mess;
		for (int s = 0; s < fields->diplomatisms_.size_; s ++) {
			diplomatism_data_fields_t* data = (diplomatism_data_fields_t*)(side_mem + fields->diplomatisms_.offset_ + s * sizeof(diplomatism_data_fields_t));
			if (s != i && data->ally_) {
				mess.insert(s + 1);
			}
		}
		ret.push_back(mess);

		offset += ((unit_segment2*)(mem + offset))->size_;
	}

	return ret;
}
