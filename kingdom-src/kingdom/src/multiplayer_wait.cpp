/* $Id: multiplayer_wait.cpp 47608 2010-11-21 01:56:29Z shadowmaster $ */
/*
   Copyright (C) 2007 - 2010
   Part of the Battle for Wesnoth Project http://www.wesnoth.org

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "global.hpp"

#include "dialogs.hpp"
#include "foreach.hpp"
#include "gettext.hpp"
#include "game_preferences.hpp"
#include "gui/dialogs/transient_message.hpp"
#include "game_display.hpp"
#include "leader_list.hpp"
#include "log.hpp"
#include "marked-up_text.hpp"
#include "multiplayer_wait.hpp"
#include "statistics.hpp"
#include "wml_separators.hpp"
#include "formula_string_utils.hpp"

static lg::log_domain log_network("network");
#define DBG_NW LOG_STREAM(debug, log_network)
#define LOG_NW LOG_STREAM(info, log_network)

namespace {
const SDL_Rect leader_pane_position = {-260,-370,260,370};
const int leader_pane_border = 10;
}

namespace mp {

wait::wait(hero_map& heros, game_display& disp, const config& cfg,
		mp::chat& c, config& gamelist) :
	heros_(heros),
	ui(disp, _("Game Lobby"), cfg, c, gamelist),
	cancel_button_(disp.video(), _("Cancel")),
	start_label_(disp.video(), _("Waiting for game to start..."), font::SIZE_SMALL, font::LOBBY_COLOR),
	game_menu_(disp.video(), std::vector<std::string>(), false, -1, -1, NULL, &gui::menu::bluebg_style),
	level_(),
	state_(),
	stop_updates_(false)
{
	game_menu_.set_numeric_keypress_selection(false);
	gamelist_updated();
}

void wait::process_event()
{
	if (cancel_button_.pressed())
		set_result(QUIT);
}

void wait::join_game(bool observe)
{
	//if we have got valid side data
	//the first condition is to make sure that we don't have another
	//WML message with a side-tag in it
	while (!level_.has_attribute("version") || !level_.child("side")) {
		network::connection data_res = dialogs::network_receive_dialog(disp(),
				_("Getting game data..."), level_);
		if (!data_res) {
			set_result(QUIT);
			return;
		}
		check_response(data_res, level_);
		if(level_.child("leave_game")) {
			set_result(QUIT);
			return;
		}
	}

	// Add the map name to the title.
	append_to_title(": " + level_["name"].t_str());

	if (!observe) {
		//search for an appropriate vacant slot. If a description is set
		//(i.e. we're loading from a saved game), then prefer to get the side
		//with the same description as our login. Otherwise just choose the first
		//available side.
		const config *side_choice = NULL;
		int side_num = -1, nb_sides = 0;
		foreach (const config &sd, level_.child_range("side"))
		{
			if (sd["controller"] == "reserved" && sd["current_player"] == preferences::login())
			{
				side_choice = &sd;
				side_num = nb_sides;
				break;
			}
			if (sd["controller"] == "network" && sd["player_id"].empty())
			{
				if (!side_choice) { // found the first empty side
					side_choice = &sd;
					side_num = nb_sides;
				}
				if (sd["current_player"] == preferences::login()) {
					side_choice = &sd;
					side_num = nb_sides;
					break;  // found the preferred one
				}
			}
			++nb_sides;
		}
		if (!side_choice) {
			set_result(QUIT);
			return;
		}

		bool allow_changes = (*side_choice)["allow_changes"].to_bool(true);

		//if the client is allowed to choose their team, instead of having
		//it set by the server, do that here.
		std::string leader_choice, gender_choice;
		int faction_choice = 0;

		if(allow_changes) {
			events::event_context context;

			const config &era = level_.child("era");
			/** @todo Check whether we have the era. If we don't inform the user. */
			if (!era)
				throw config::error(_("No era information found."));
/*
			config::const_child_itors possible_sides = era.child_range("multiplayer_side");
			if (possible_sides.first == possible_sides.second) {
				set_result(QUIT);
				throw config::error(_("No multiplayer sides found"));
				return;
			}
*/
			int color = side_num;
			const std::string color_str = (*side_choice).get_old_attribute("color","colour");
			if (!color_str.empty())
				color = game_config::color_info(color_str).index() - 1;

			config faction;
			config& change = faction.add_child("change_faction");
			change["name"] = preferences::login();
			change["faction"] = 0; // now, it is ignored
			change["leader"] = "random"; // now, it is ignored
			change["gender"] = "random"; // now, it is ignored
			network::send_data(faction, 0);
		}

	}

	generate_menu();
}

const game_state& wait::get_state()
{
	return state_;
}

void wait::start_game()
{
	if (const config &stats = level_.child("statistics")) {
		statistics::fresh_stats();
		statistics::read_stats(stats);
	}

	/**
	 * @todo Instead of using level_to_gamestate reinit the state_,
	 * this needs more testing -- Mordante
	 * It seems level_to_gamestate is needed for the start of game
	 * download, but downloads of later scenarios miss certain info
	 * and add a players section. Use players to decide between old
	 * and new way. (Of course it would be nice to unify the data
	 * stored.)
	 */
	if (!level_.child("player")) {
		level_to_gamestate(level_, replay_data_, state_);
	} else {

		state_ = game_state(level_, replay_data_);

		// When we observe and don't have the addon installed we still need
		// the old way, no clue why however. Code is a copy paste of
		// playcampaign.cpp:576 which shows an 'Unknown scenario: '$scenario|'
		// error. This seems to work and have no side effects....
		if(!state_.classification().scenario.empty() && state_.classification().scenario != "null") {
			DBG_NW << "Falling back to loading the old way.\n";
			level_to_gamestate(level_, replay_data_, state_);
		}
	}

	LOG_NW << "starting game\n";
}

void wait::layout_children(const SDL_Rect& rect)
{
	ui::layout_children(rect);

	const SDL_Rect ca = client_area();
	int y = ca.y + ca.h - cancel_button_.height();

	game_menu_.set_location(ca.x, ca.y + title().height());
	game_menu_.set_measurements(ca.w, y - ca.y - title().height()
			- gui::ButtonVPadding);
	game_menu_.set_max_width(ca.w);
	game_menu_.set_max_height(y - ca.y - title().height() - gui::ButtonVPadding);
	cancel_button_.set_location(ca.x + ca.w - cancel_button_.width(), y);
	start_label_.set_location(ca.x, y + 4);
}

void wait::hide_children(bool hide)
{
	ui::hide_children(hide);

	cancel_button_.hide(hide);
	game_menu_.hide(hide);
}

void wait::process_network_data(const config& data, const network::connection sock)
{
	ui::process_network_data(data, sock);

	if(data["message"] != "") {
		gui2::show_transient_message(disp().video()
				, _("Response")
				, data["message"]);
	}
	if (data["failed"].to_bool()) {
		set_result(QUIT);
		return;
	} else if(data.child("stop_updates")) {
		stop_updates_ = true;
	} else if(data.child("start_game")) {
		LOG_NW << "received start_game message\n";
		set_result(PLAY);
		return;
	} else if(data.child("leave_game")) {
		set_result(QUIT);
		return;
	} else if (const config &c = data.child("scenario_diff")) {
		LOG_NW << "received diff for scenario... applying...\n";
		/** @todo We should catch config::error and then leave the game. */
		level_.apply_diff(c);
		generate_menu();
	} else if(data.child("side")) {
		level_ = data;
		LOG_NW << "got some sides. Current number of sides = "
			<< level_.child_count("side") << ','
			<< data.child_count("side") << '\n';
		generate_menu();
	}
}

void wait::generate_menu()
{
	if (stop_updates_) {
		foreach (const config &sd, level_.child_range("side")) {
			hero& leader = heros_[sd["leader"].to_int()];
			int selected_feature = sd["selected_feature"].to_int();
			if (selected_feature >= COMBO_FEATURES_MIN_VALID) {
				leader.side_feature_ = hero::valid_features()[selected_feature - COMBO_FEATURES_MIN_VALID];

			} else if (selected_feature == COMBO_FEATURES_NONE) {
				leader.side_feature_ = HEROS_NO_SIDE_FEATURE;
			}
		}
		return;
	}

	std::vector<std::string> details;
	std::vector<std::string> playerlist;

	foreach (const config &sd, level_.child_range("side"))
	{
		if (!sd["allow_player"].to_bool(true)) {
			continue;
		}

		std::string description = sd["user_description"];
		const std::string faction_id = sd["player_id"];

		t_string side_name = sd["faction_name"];
		std::string leader_type = sd["type"];
		std::string gender_id = sd["gender"];

		if(!sd["player_id"].empty())
			playerlist.push_back(sd["player_id"]);

		std::string leader_name;
		std::string leader_image;
		if (!sd["leader"].empty()) {
			hero& leader = heros_[sd["leader"].to_int()];
			leader_name = leader.name();
			leader_image = std::string(leader.image()) + std::string("~CROP(36, 30, 48, 60)~SCALE(24, 30)");

		} else {
			leader_name = _("Random");
			leader_image = std::string("units/random-dice.png") + std::string("~SCALE(24, 30)");
		}

		if (!leader_image.empty()) {
			// Dumps the "image" part of the faction name, if any,
			// to replace it by a picture of the actual leader
			if(side_name.str()[0] == font::IMAGE) {
				std::string::size_type p =
					side_name.str().find_first_of(COLUMN_SEPARATOR);
				if(p != std::string::npos && p < side_name.size()) {
					side_name = IMAGE_PREFIX + leader_image + COLUMN_SEPARATOR + side_name.str().substr(p+1);
				}
			} else {
				// no image prefix, just add the leader image
				// (assuming that there is also no COLUMN_SEPARATOR)
				side_name = IMAGE_PREFIX + leader_image + COLUMN_SEPARATOR + side_name.str();
			}
		}

		std::string feature_str;
		int selected_feature = sd["selected_feature"].to_int();
		if (selected_feature == COMBO_FEATURES_RANDOM) {
			feature_str = _("Random");

		} else if (selected_feature >= COMBO_FEATURES_MIN_VALID) {
			feature_str = hero::feature_str(hero::valid_features()[selected_feature - COMBO_FEATURES_MIN_VALID]);
		}

		std::stringstream str;
		str << sd["side"] << ". " << COLUMN_SEPARATOR;
		str << description << COLUMN_SEPARATOR << side_name << COLUMN_SEPARATOR;
		// Mark parentheses translatable for languages like Japanese
		if (!leader_name.empty())
			str << _("(") << leader_name << _(")");
		str << "(" << feature_str << ")";
		str << COLUMN_SEPARATOR;
		// Don't show gold for saved games
		if (sd["allow_changes"].to_bool())
			str << sd["gold"] << ' ' << _n("multiplayer_starting_gold^Gold", "multiplayer_starting_gold^Gold", sd["gold"].to_int()) << COLUMN_SEPARATOR;

		int income_amt = sd["income"];
		if(income_amt != 0){
			str << _("(") << _("Income") << ' ';
			if(income_amt > 0)
				str << _("+");
			str << sd["income"] << _(")");
		}

		str << COLUMN_SEPARATOR << t_string::from_serialized(sd["user_team_name"].str());
		int disp_color = sd.get_old_attribute("color","colour");
		if(!sd.get_old_attribute("color","colour").empty()) {
			try {
				disp_color = game_config::color_info(sd.get_old_attribute("color","colour")).index();
			} catch(config::error&) {
				//ignore
			}
		} else {
			/**
			 * @todo we fall back to the side color, but that's ugly rather
			 * make the color mandatory in 1.5.
			 */
			disp_color = sd["side"];
		}
		str << COLUMN_SEPARATOR << get_color_string(disp_color - 1);
		details.push_back(str.str());
	}

	game_menu_.set_items(details);

	// Uses the actual connected player list if we do not have any
	// "gamelist" user data
	if (!gamelist().child("user")) {
		set_user_list(playerlist, true);
	}
}

} // namespace mp

