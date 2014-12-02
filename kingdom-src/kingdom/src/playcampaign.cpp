/* $Id: playcampaign.cpp 47582 2010-11-16 23:28:08Z ai0867 $ */
/*
   Copyright (C) 2003-2005 by David White <dave@whitevine.net>
   Copyright (C) 2005 - 2010 by Philippe Plantier <ayin@anathas.org>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org

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
 *  Controls setup, play, (auto)save and replay of campaigns.
 */

#include "global.hpp"

#include "game_preferences.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/transient_message.hpp"
#include "gui/widgets/window.hpp"
#include "playcampaign.hpp"
#include "map_create.hpp"
#include "persist_manager.hpp"
#include "playmp_controller.hpp"
#include "replay_controller.hpp"
#include "map_exception.hpp"
#include "dialogs.hpp"
#include "gettext.hpp"
#include "resources.hpp"
#include "savegame.hpp"
#include "sound.hpp"
#include "wml_exception.hpp"
#include "formula_string_utils.hpp"
#include "loadscreen.hpp"

#include <boost/foreach.hpp>

void play_replay(game_display& disp, game_state& gamestate, const config& game_config, 
	hero_map& heros, hero_map& heros_start, card_map& cards, CVideo& video)
{
	std::string type = gamestate.classification().campaign_type;
	if (type.empty()) {
		type = "scenario";
	}

	// 'starting_pos' will contain the position we start the game from.
	config starting_pos;

	if (gamestate.starting_pos.empty()){
		// Backwards compatibility code for 1.2 and 1.2.1
		const config &scenario = game_config.find_child(type,"id",gamestate.classification().scenario);
		assert(scenario);
		gamestate.starting_pos = scenario;
	}
	starting_pos = gamestate.starting_pos;

	//for replays, use the variables specified in starting_pos
	if (const config &vars = starting_pos.child("variables")) {
		gamestate.set_variables(vars);
	}

	try {
		// Preserve old label eg. replay
		if (gamestate.classification().label.empty())
			gamestate.classification().label = starting_pos["name"].str();
		//if (gamestate.abbrev.empty())
		//	gamestate.abbrev = (*scenario)["abbrev"];

		{
			game_display_lock loc(disp);
			play_replay_level(game_config, &starting_pos, video, gamestate, heros, heros_start, cards);
		}

		gamestate.snapshot = config();
		recorder.clear();
		gamestate.replay_data.clear();
		gamestate.start_scenario_ss.str("");
		// gamestate.start_hero_ss.str("");
		gamestate.clear_start_hero_data();

	} catch(game::load_game_failed& e) {
		gui2::show_error_message(disp.video(), _("The game could not be loaded: ") + e.message);
	} catch(game::game_error& e) {
		gui2::show_error_message(disp.video(), _("Error while playing the game: ") + e.message);
	} catch(incorrect_map_format_error& e) {
		gui2::show_error_message(disp.video(), std::string(_("The game map could not be loaded: ")) + e.message);
	} catch(twml_exception& e) {
		e.show(disp);
	}
}

static LEVEL_RESULT playsingle_scenario(const config& game_config,
		const config* level, display& disp, game_state& state_of_game, hero_map& heros, hero_map& heros_start,
		card_map& cards,
		const config::const_child_itors &story,
		bool skip_replay, end_level_data &end_level)
{
	const int ticks = SDL_GetTicks();
	int num_turns = (*level)["turns"].to_int();

	playsingle_controller playcontroller(*level, state_of_game, heros, heros_start, cards, ticks, num_turns, game_config, disp.video(), skip_replay);

	LEVEL_RESULT res = playcontroller.play_scenario(story, skip_replay);
	playcontroller.get_end_level_data().result = res;
	end_level = playcontroller.get_end_level_data();

	if (res == DEFEAT) {
		if (resources::persist != NULL) {
			resources::persist->end_transaction();
		}
		gui2::show_transient_message(disp.video(),
				    _("Defeat"),
				    _("You have been defeated!")
				    );
	}

	if (res != QUIT && end_level.linger_mode)
	{
		try {
			playcontroller.linger();
		} catch(end_level_exception& e) {
			if (e.result == QUIT) {
				return QUIT;
			}
		}
	}

	return res;
}


static LEVEL_RESULT playmp_scenario(const config& game_config,
		config const* level, display& disp, game_state& state_of_game, hero_map& heros, hero_map& heros_start,
		card_map& cards,
		const config::const_child_itors &story, bool skip_replay,
		io_type_t& io_type, end_level_data &end_level)
{
	const int ticks = SDL_GetTicks();
	int num_turns = (*level)["turns"].to_int();
	playmp_controller playcontroller(*level, state_of_game, heros, heros_start, cards,
		ticks, num_turns, game_config, disp.video(), skip_replay, io_type == IO_SERVER);
	LEVEL_RESULT res = playcontroller.play_scenario(story, skip_replay);
	playcontroller.get_end_level_data().result = res;
	end_level = playcontroller.get_end_level_data();

	//Check if the player started as mp client and changed to host
	if (io_type == IO_CLIENT && playcontroller.is_host())
		io_type = IO_SERVER;

	if (res == DEFEAT) {
		if (resources::persist != NULL)
			resources::persist->end_transaction();
		gui2::show_transient_message(disp.video(),
				    _("Defeat"),
				    _("You have been defeated!")
				    );
	}

	if (res != QUIT) {
		if (!end_level.linger_mode) {
			if(!playcontroller.is_host()) {
				// If we continue without lingering we need to
				// make sure the host uploads the next scenario
				// before we attempt to download it.
				playcontroller.wait_for_upload();
			}
		} else {
			try {
				playcontroller.linger();
			} catch(end_level_exception& e) {
				if (e.result == QUIT) {
					return QUIT;
				}
			}
		}
	}

	return res;
}

LEVEL_RESULT play_game(game_display& disp, game_state& gamestate, const config& game_config, hero_map& heros, hero_map& heros_start,
		card_map& cards,
		io_type_t io_type, bool skip_replay)
{
	tent::io_type = io_type;
	std::string type = gamestate.classification().campaign_type;
	if (type.empty()) {
		type = "scenario";
	}

	config const* scenario = NULL;

	// 'starting_pos' will contain the position we start the game from.
	config starting_pos;

	// Do we have any snapshot data?
	// yes => this must be a savegame
	// no  => we are starting a fresh scenario
	if (!gamestate.snapshot["runtime"].to_bool() || !recorder.at_end()) {
		gamestate.classification().completion = "running";
		// Campaign or Multiplayer?
		// If the gamestate already contains a starting_pos,
		// then we are starting a fresh multiplayer game.
		// Otherwise this is the start of a campaign scenario.
		if (gamestate.starting_pos["id"].empty() == false) {
			starting_pos = gamestate.starting_pos;
			scenario = &starting_pos;
		} else {
			// reload of the scenario, as starting_pos contains carryover information only
			starting_pos = load_campagin_scenario(gamestate.classification().campaign, gamestate.classification().scenario, type);
			VALIDATE(!starting_pos.empty(), std::string("play_game, cannot load scenario id = ") + gamestate.classification().scenario + " in game_config!");
			starting_pos["turn_based"] = gamestate.starting_pos["turn_based"];
			gamestate.starting_pos = starting_pos;
			scenario = &starting_pos;
		}
	} else {
		// This game was started from a savegame
		starting_pos = gamestate.starting_pos;
		scenario = &gamestate.snapshot;
		// When starting wesnoth --multiplayer there might be
		// no variables which leads to a segfault
		if (const config &vars = gamestate.snapshot.child("variables")) {
			gamestate.set_variables(vars);
		}
		gamestate.set_menu_items(gamestate.snapshot.child_range("menu_item"));
		// Replace game label with that from snapshot
		if (!gamestate.snapshot["label"].empty()){
			gamestate.classification().label = gamestate.snapshot["label"].str();
		}
	}

	bool fog = scenario->get("fog")->to_bool();
	bool shroud = scenario->get("shroud")->to_bool();

	while (scenario != NULL) {
		// If we are a multiplayer client, tweak the controllers
		if (io_type == IO_CLIENT) {
			if (scenario != &starting_pos) {
				starting_pos = *scenario;
				scenario = &starting_pos;
			}
		}

		config::const_child_itors story = scenario->child_range("story");
		gamestate.classification().next_scenario = (*scenario)["next_scenario"].str();

		LEVEL_RESULT res = VICTORY;
		end_level_data end_level;

		try {
			// Preserve old label eg. replay
			if (gamestate.classification().label.empty()) {
				t_string tstr = (*scenario)["name"];
				if (gamestate.classification().abbrev.empty()) {
					gamestate.classification().label = tstr;
					gamestate.classification().original_label = tstr.base_str();
				} else {
					gamestate.classification().label = std::string(gamestate.classification().abbrev);
					gamestate.classification().label.append("-");
					gamestate.classification().original_label = gamestate.classification().label;
					gamestate.classification().label.append(tstr);
					gamestate.classification().original_label.append(tstr.base_str());
				}
			}

			// If the entire scenario should be randomly generated
			if ((*scenario)["scenario_generation"] != "") {
				const cursor::setter cursor_setter(cursor::WAIT);

				static config scenario2;
				scenario2 = random_generate_scenario((*scenario)["scenario_generation"], scenario->child("generator"));
				//level_ = scenario;
				//merge carryover information into the newly generated scenario
				config temp(scenario2);
				gamestate.starting_pos = temp;
				scenario = &scenario2;
			}
			std::string map_data = (*scenario)["map_data"];
			if(map_data.empty() && (*scenario)["map"] != "") {
				map_data = read_map((*scenario)["map"]);
			}

			// If the map should be randomly generated
			if(map_data.empty() && (*scenario)["map_generation"] != "") {
				const cursor::setter cursor_setter(cursor::WAIT);
				map_data = random_generate_map((*scenario)["map_generation"],scenario->child("generator"));

				// Since we've had to generate the map,
				// make sure that when we save the game,
				// it will not ask for the map to be generated again on reload
				static config new_level;
				new_level = *scenario;
				new_level["map_data"] = map_data;
				scenario = &new_level;

				//merge carryover information into the scenario
				config temp(new_level);
				gamestate.starting_pos = temp;
			}

			sound::empty_playlist();

			//add the variables to the starting_pos unless they are already there
			const config &wmlvars = gamestate.starting_pos.child("variables");
			if (!wmlvars || wmlvars.empty()){
				gamestate.starting_pos.clear_children("variables");
				gamestate.starting_pos.add_child("variables", gamestate.get_variables());
			}

			{
				game_display_lock loc(disp);
				switch (io_type){
				case IO_NONE:
					res = playsingle_scenario(game_config, scenario, disp, gamestate, heros, heros_start, cards, story, skip_replay, end_level);
					break;
				case IO_SERVER:
				case IO_CLIENT:
					res = playmp_scenario(game_config, scenario, disp, gamestate, heros, heros_start, cards, story, skip_replay, io_type, end_level);
					break;
				}
			}

		} catch(game::load_game_failed& e) {
			gui2::show_error_message(disp.video(), _("The game could not be loaded: ") + e.message);
			return QUIT;
		} catch(game::game_error& e) {
			gui2::show_error_message(disp.video(), _("Error while playing the game: ") + e.message);
			return QUIT;
		} catch(incorrect_map_format_error& e) {
			gui2::show_error_message(disp.video(), std::string(_("The game map could not be loaded: ")) + e.message);
			return QUIT;
		} catch(config::error& e) {
			std::cerr << "caught config::error...\n";
			gui2::show_error_message(disp.video(), _("Error while reading the WML: ") + e.message);
			return QUIT;
		} catch(twml_exception& e) {
			e.show(disp);
			return QUIT;
		}


		// Save-management options fire on game end.
		// This means: (a) we have a victory, or
		// or (b) we're multiplayer live, in which
		// case defeat is also game end.  Someday,
		// if MP campaigns ever work again, we might
		// need to change this test.
		if (res == VICTORY || (io_type != IO_NONE && res == DEFEAT)) {
			if (preferences::delete_saves()) {
				savegame::manager::clean_saves(gamestate.classification().label);
			}
		}

		recorder.clear();
		gamestate.replay_data.clear();
		gamestate.start_scenario_ss.str("");
		gamestate.clear_start_hero_data();

		// On DEFEAT, QUIT, or OBSERVER_END, we're done now
		if (res != VICTORY) {
			if (res != OBSERVER_END || gamestate.classification().next_scenario.empty()) {
				gamestate.snapshot = config();
				return res;
			}

			const int dlg_res = gui2::show_message(disp.video(), _("Game Over"),
				_("This scenario has ended. Do you want to continue the campaign?"),
				gui2::tmessage::yes_no_buttons);

			if (dlg_res == gui2::twindow::CANCEL) {
				gamestate.snapshot = config();
				return res;
			}
		}

		// Switch to the next scenario.
		gamestate.classification().scenario = gamestate.classification().next_scenario;
		gamestate.rng().rotate_random();
		// gamestate.rng().seed_random(rand(), 0);

		scenario = NULL;
		if (io_type == IO_NONE) {
			starting_pos = load_campagin_scenario(gamestate.classification().campaign, gamestate.classification().scenario, type);
			if (starting_pos.empty()) {
				scenario = NULL;
			} else{
				starting_pos["fog"] = fog;
				starting_pos["shroud"] = shroud;
				starting_pos["turn_based"] = tent::turn_based;
				scenario = &starting_pos;
			}
		}

		if (scenario != NULL) {
			// to single-player campaign, there are multi-secenario, 
			// set this result to next start hero data
			heros.reset_to_unstage();
			heros_start = heros;
			if (!runtime_groups::gs.empty()) {
				group = runtime_groups::gs.begin()->second;
			}

			loadscreen::global_loadscreen_manager* loadscreen_manager = loadscreen::global_loadscreen_manager::get();
			if (loadscreen_manager) {
				loadscreen_manager->reset();
			}
			// Update the label
			// std::string oldlabel = gamestate.classification().label;
			t_string scenario_name = (*scenario)["name"];
			if (gamestate.classification().abbrev.empty()) {
				gamestate.classification().label = scenario_name;
				gamestate.classification().original_label = scenario_name.base_str();
			} else {
				gamestate.classification().label = std::string(gamestate.classification().abbrev);
				gamestate.classification().label.append("-");
				gamestate.classification().original_label = gamestate.classification().label;
				gamestate.classification().label.append(scenario_name);
				gamestate.classification().original_label.append(scenario_name.base_str());
			}

			if (gamestate.classification().campaign_type != "multiplayer"){
				gamestate.starting_pos = *scenario;
				//add the variables to the starting position
				gamestate.starting_pos.add_child("variables", gamestate.get_variables());
			}
			gamestate.start_scenario_ss.str("");
		}
		gamestate.snapshot = config();
	}

	if (!gamestate.classification().scenario.empty() && gamestate.classification().scenario != "null") {
		std::string message = _("Unknown scenario: '$scenario|'");
		utils::string_map symbols;
		symbols["scenario"] = gamestate.classification().scenario;
		message = utils::interpolate_variables_into_string(message, &symbols);
		gui2::show_error_message(disp.video(), message);
		return QUIT;
	}

	if (gamestate.classification().campaign_type == "scenario"){
		if (preferences::delete_saves()) {
			savegame::manager::clean_saves(gamestate.classification().label);
		}
	}
	return VICTORY;
}

