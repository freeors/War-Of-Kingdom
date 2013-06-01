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
#include "foreach.hpp"
#include "gettext.hpp"
#include "log.hpp"
#include "game_preferences.hpp"
#include "replay.hpp"
#include "resources.hpp"
#include "statistics.hpp"
#include "team.hpp"
#include "unit.hpp"
#include "unit_id.hpp"
#include "wesconfig.h"
#include "wml_exception.hpp"
#include "formula_string_utils.hpp"
#include "map.hpp"
#include "pathfind/pathfind.hpp"
#include "artifical.hpp"
#include "resources.hpp"

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
	savegame_config(),
	label(),
	original_label(),
	parent(),
	version(),
	campaign_type(),
	campaign_define(),
	campaign_xtra_defines(),
	campaign(),
	history(),
	abbrev(),
	mode(),
	scenario(),
	next_scenario(),
	completion(),
	end_text(),
	end_text_duration(),
	create(0)
	{}

game_classification::game_classification(const config& cfg):
	savegame_config(),
	label(cfg["label"]),
	original_label(cfg["original_label"]),
	parent(cfg["parent"]),
	version(cfg["version"]),
	campaign_type(cfg["campaign_type"].empty() ? "scenario" : cfg["campaign_type"].str()),
	campaign_define(cfg["campaign_define"]),
	campaign_xtra_defines(utils::split(cfg["campaign_extra_defines"])),
	campaign(cfg["campaign"]),
	history(cfg["history"]),
	abbrev(cfg["abbrev"]),
	mode(cfg["mode"]),
	scenario(cfg["scenario"]),
	next_scenario(cfg["next_scenario"]),
	completion(cfg["completion"]),
	end_text(cfg["end_text"]),
	end_text_duration(cfg["end_text_duration"]),
	create(cfg["create"].to_long())
	{}

game_classification::game_classification(const game_classification& gc):
	savegame_config(),
	label(gc.label),
	original_label(gc.original_label),
	parent(gc.parent),
	version(gc.version),
	campaign_type(gc.campaign_type),
	campaign_define(gc.campaign_define),
	campaign_xtra_defines(gc.campaign_xtra_defines),
	campaign(gc.campaign),
	history(gc.history),
	abbrev(gc.abbrev),
	mode(gc.mode),
	scenario(gc.scenario),
	next_scenario(gc.next_scenario),
	completion(gc.completion),
	end_text(gc.end_text),
	end_text_duration(gc.end_text_duration),
	create(gc.create)
{
}

config game_classification::to_config() const
{
	config cfg;

	cfg["label"] = label;
	cfg["parent"] = parent;
	cfg["version"] = game_config::version;
	cfg["campaign_type"] = campaign_type;
	cfg["campaign_define"] = campaign_define;
	cfg["campaign_extra_defines"] = utils::join(campaign_xtra_defines);
	cfg["campaign"] = campaign;
	cfg["history"] = history;
	cfg["abbrev"] = abbrev;
	cfg["mode"] = mode;
	cfg["scenario"] = scenario;
	cfg["next_scenario"] = next_scenario;
	cfg["completion"] = completion;
	cfg["end_text"] = end_text;
	cfg["end_text_duration"] = str_cast<unsigned int>(end_text_duration);
	cfg["create"] = create;

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

void level_to_gamestate(config& level, command_pool& replay_data, game_state& state)
{
	//any replay data is only temporary and should be removed from
	//the level data in case we want to save the game later
	if (replay_data.pool_pos_vsize()) {
		state.replay_data = replay_data;
		recorder = replay(state.replay_data);
		if(!recorder.empty()) {
			recorder.set_skip(false);
			recorder.set_to_end();
		}
	}

	//set random
	const std::string seed = level["random_seed"];
	if(! seed.empty()) {
		const unsigned calls = lexical_cast_default<unsigned>(level["random_calls"]);
		state.rng().seed_random(lexical_cast<int>(seed), calls);
	} else {
		// ERR_NG << "No random seed found, random "
		//	"events will probably be out of sync.\n";
	}

	//adds the starting pos to the level
	if (!level.child("replay_start")) {
		level.add_child("replay_start", level);
		level.child("replay_start").remove_child("multiplayer", 0);
	}
	//this is important, if it does not happen, the starting position is missing and
	//will be drawn from the snapshot instead (which is not what we want since we have
	//all needed information here already)
	state.starting_pos = level.child("replay_start");

	level["campaign_type"] = "multiplayer";
	state.classification().campaign_type = "multiplayer";
	state.classification().completion = level["completion"].str();
	state.classification().version = level["version"].str();

	if (const config &vars = level.child("variables")) {
		state.set_variables(vars);
	}
	state.set_menu_items(level.child_range("menu_item"));
	state.mp_settings().set_from_config(level);

	//Check whether it is a save-game by looking for snapshot data
	const config &snapshot = level.child("snapshot");
	const bool saved_game = snapshot && snapshot.child("side");

	//It might be a MP campaign start-of-scenario save
	//In this case, it's not entirely a new game, but not a save, either
	//Check whether it is no savegame and the starting_pos contains [player] information
	bool start_of_scenario = !saved_game && state.starting_pos.child("player");

	//If we start a fresh game, there won't be any snapshot information. If however this
	//is a savegame, we got a valid snapshot here.
	if (saved_game) {
		state.snapshot = snapshot;
		if (const config &v = snapshot.child("variables")) {
			state.set_variables(v);
		}
		state.set_menu_items(snapshot.child_range("menu_item"));
	}

	//In any type of reload(normal save or start-of-scenario) the players could have
	//changed and need to be replaced
	if(saved_game || start_of_scenario){
		config::child_itors saved_sides = saved_game ?
			state.snapshot.child_range("side") :
			state.starting_pos.child_range("side");
		config::const_child_itors level_sides = level.child_range("side");

		foreach (config &side, saved_sides)
		{
			foreach (const config &lside, level_sides)
			{
				if (side["side"] == lside["side"] &&
						(side["current_player"] != lside["current_player"] ||
						 side["controller"] != lside["controller"]))
				{
					side["current_player"] = lside["current_player"];
					side["id"] = lside["id"];
					side["save_id"] = lside["save_id"];
					side["controller"] = lside["controller"];
					break;
				}
			}
		}
	}
	if(state.get_variables().empty()) {
		// LOG_NG << "No variables were found for the game_state." << std::endl;
	} else {
		// LOG_NG << "Variables found and loaded into game_state:" << std::endl;
		// LOG_NG << state.get_variables();
	}
}

void write_players(game_state& gamestate, config& cfg, const bool use_snapshot, const bool merge_side)
{
	// If there is already a player config available it means we are loading
	// from a savegame. Don't do anything then, the information is already there
	config::child_itors player_cfg = cfg.child_range("player");
	if (player_cfg.first != player_cfg.second)
		return;

	config *source = NULL;
	if (use_snapshot) {
		source = &gamestate.snapshot;
	} else {
		source = &gamestate.starting_pos;
	}

	if (merge_side) {
		//merge sides/players from starting pos with the scenario cfg
		std::vector<std::string> tags;
		tags.push_back("side");
		tags.push_back("player"); //merge [player] tags for backwards compatibility of saves

		foreach (const std::string& side_tag, tags)
		{
			foreach (config &carryover_side, source->child_range(side_tag))
			{
				config *scenario_side = NULL;

				//TODO: use the player_id instead of the save_id for that
				if (config& c = cfg.find_child("side", "save_id", carryover_side["save_id"])) {
					scenario_side = &c;
				} else if (config& c = cfg.find_child("side", "id", carryover_side["save_id"])) {
					scenario_side = &c;
				}

				if (scenario_side == NULL) {
					//no matching side in the current scenario, we add the persistent information in a [player] tag
					cfg.add_child("player", carryover_side);
					continue;
				}

				//we have a matching side in the current scenario

				//sort carryover gold
				int ngold = (*scenario_side)["gold"].to_int(100);
				int player_gold = carryover_side["gold"];
				if (carryover_side["gold_add"].to_bool()) {
					ngold += player_gold;
				} else if (player_gold >= ngold) {
					ngold = player_gold;
				}
				carryover_side["gold"] = str_cast(ngold);
				if (const config::attribute_value *v = scenario_side->get("gold_add")) {
					carryover_side["gold_add"] = *v;
				}
				//merge player information into the scenario cfg
				(*scenario_side)["save_id"] = carryover_side["save_id"];
				(*scenario_side)["gold"] = ngold;
				(*scenario_side)["gold_add"] = carryover_side["gold_add"];
				if (const config::attribute_value *v = carryover_side.get("previous_recruits")) {
					(*scenario_side)["previous_recruits"] = *v;
				} else {
					(*scenario_side)["previous_recruits"] = carryover_side["can_recruit"];
				}
				(*scenario_side)["name"] = carryover_side["name"];
				(*scenario_side)["current_player"] = carryover_side["current_player"];

				///@deprecated 1.9.2 'colour' in [side]
				const std::string colour_error = "Usage of 'colour' in [side] is deprecated, support will be removed in 1.9.2.\n";
				(*scenario_side)["color"] = carryover_side.get_old_attribute("color","colour",colour_error);
				///@deprecated 1.9.2 'colour' also written in [side]
				(*scenario_side)["colour"] = (*scenario_side)["color"];

				//add recallable units
				foreach (const config &u, carryover_side.child_range("unit")) {
					scenario_side->add_child("unit", u);
				}
			}
		}
	} else {
		foreach(const config &snapshot_side, source->child_range("side")) {
			//take all side tags and add them as players (assuming they only contain carryover information)
			cfg.add_child("player", snapshot_side);
		}
	}
}

game_state::game_state(const config& cfg, const command_pool& replay_data, bool show_replay) :
		scoped_variables(),
		wml_menu_items(),
		replay_data(replay_data),
		starting_pos(),
		start_hero_data_(NULL),
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
	n_unit::id_manager::instance().set_save_id(cfg["next_underlying_unit_id"]);
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
			foreach (const config &p, cfg.child_range("player")) {
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

	cfg["campaign_define"] = classification_.campaign_define;
	cfg["campaign_extra_defines"] = utils::join(classification_.campaign_xtra_defines);
	cfg["next_underlying_unit_id"] = str_cast(n_unit::id_manager::instance().get_save_id());

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

void game_state::write_armory_2_variable(const team& t)
{
	config& player_cfg = variables_.child("player");
	config& side = player_cfg.add_child("armory");
	std::stringstream armory_heros;

	// current visible units
	const std::pair<unit**, size_t> p = t.field_troop();
	unit** troops = p.first;
	size_t troops_vsize = p.second;
	for (size_t i = 0; i < troops_vsize; i ++) {
		if (!troops[i]->is_artifical()) {
			// field troops
			config& u = side.add_child("unit");
			troops[i]->get_location().write(u);
			troops[i]->write(u);
		}
	}
	const std::vector<artifical*>& holded_cities = t.holded_cities();
	for (std::vector<artifical*>::const_iterator i = holded_cities.begin(); i != holded_cities.end(); ++ i) {
		artifical* city = *i;

		std::vector<unit*>& reside_troops = city->reside_troops();
		for (std::vector<unit*>::const_iterator itor = reside_troops.begin(); itor != reside_troops.end(); ++ itor) {
			// reside troops
			const unit& u = **itor;
			config& ucfg = side.add_child("unit");
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
	(*player_cfg)["forbids"] = rpg::forbids;
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

game_state::~game_state() {
	clear_wmi(wml_menu_items);
	if (start_hero_data_) {
		free(start_hero_data_);
	}
}

void game_state::set_variables(const config& vars) {
	variables_ = vars;
}

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
	temp_team.set_gold_add(gold_add);
	teams.push_back(temp_team);

	// Update/fix the recall list for this side,
	// by setting the "side" of each unit in it
	// to be the "side" of the player.
	int side = side_cfg["side"].to_int(1);

	// If this team has no objectives, set its objectives
	// to the level-global "objectives"
	if (teams.back().objectives().empty()) {
		teams.back().set_objectives(level["objectives"]);
	}


	//
	// artifical(city, market...)
	// !!!first must is artifical, then troops
	//

	config& player_cfg1 = variables_.child("player");
	bool has_armory = (player_cfg1 && player_cfg1.child("armory"))? true: false;

	// If there are additional starting artificals on this side
	//add the units with a specified position to the unit map
	foreach (const config& cfg, side_cfg.child_range("artifical")) {
		config temp_cfg(cfg);

		if (has_armory && teams.back().is_human()) {
			config& armory_cfg = player_cfg1.child("armory");

			// add [player].[armory] to this team
			foreach (config &i, armory_cfg.child_range("unit")) {
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
		artifical new_unit(temp_cfg);

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
			std::set<map_location> touch_locs = new_unit.get_touch_locations(map, loc);
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
	foreach (const config& cfg, side_cfg.child_range("unit")) {
		config temp_cfg(cfg);
		temp_cfg["side"] = side; //set the side before unit creation to avoid invalid side errors
		unit new_unit(units, heros, teams, temp_cfg, true);

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
			if (units.find(loc, !new_unit.base()) != units.end()) {
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

	teams.back().set_current_player(teams.back().name());
}

void game_state::build_team(const uint8_t* mem,
					 std::vector<team>& teams,
					 const config& level, gamemap& map, unit_map& units, hero_map& heros,
					 card_map& cards,
					 bool snapshot)
{
	if (map.empty()) {
		throw game::load_game_failed("Map not found");
	}

	const unit_segment2* total = (const unit_segment2*)mem;
	int offset = sizeof(unit_segment2);

	int ngold = ((team_fields_t*)(mem + offset))->gold_;

	team temp_team(units, heros, cards, mem + offset, map, ngold);
	offset += ((team_fields_t*)(mem + offset))->size_;

	temp_team.set_gold_add(0);
	teams.push_back(temp_team);

	team& current_team = teams.back();

	int side = current_team.side();

	// If this team has no objectives, set its objectives
	// to the level-global "objectives"
	if (current_team.objectives().empty()) {
		current_team.set_objectives(level["objectives"]);
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
			std::set<map_location> touch_locs = new_unit->get_touch_locations(map, loc);
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
			foreach (config &i, armory_cfg.child_range("unit")) {
				i["cityno"] = current->cityno();
				i["side"] = current->side();
				reside_troops.push_back(new unit(units, heros, teams, i));
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

	current_team.set_current_player(current_team.name());
}

void game_state::set_menu_items(const config::const_child_itors &menu_items)
{
	clear_wmi(wml_menu_items);
	foreach (const config &item, menu_items)
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
