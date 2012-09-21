/* $Id: campaign_difficulty.cpp 49602 2011-05-22 17:56:13Z mordante $ */
/*
   Copyright (C) 2010 - 2011 by Ignacio Riquelme Morelle <shadowm2006@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "gui/dialogs/mp_side_wait.hpp"

#include "dialogs.hpp"
#include "game_display.hpp"
#include "game_preferences.hpp"
#include "gettext.hpp"
#include "foreach.hpp"
#include "gui/auxiliary/timer.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/dialogs/helper.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/tree_view_node.hpp"
#include "gui/dialogs/combo_box.hpp"
#include "gui/dialogs/transient_message.hpp"
#include "formula_string_utils.hpp"
#include "map.hpp"
#include "savegame.hpp"
#include "unit_id.hpp"
#include "hero.hpp"
#include "replay.hpp"
#include "wml_separators.hpp"
#include "statistics.hpp"

#include <boost/bind.hpp>

namespace gui2 {

/*WIKI
 * @page = GUIWindowDefinitionWML
 * @order = 2_campaign_difficulty
 *
 * == Campaign difficulty ==
 *
 * The campaign mode difficulty menu.
 *
 * @begin{table}{dialog_widgets}
 *
 * title & & label & m &
 *         Dialog title label. $
 *
 * message & & scroll_label & o &
 *         Text label displaying a description or instructions. $
 *
 * listbox & & listbox & m &
 *         Listbox displaying user choices, defined by WML for each campaign. $
 *
 * -icon & & control & m &
 *         Widget which shows a listbox item icon, first item markup column. $
 *
 * -label & & control & m &
 *         Widget which shows a listbox item label, second item markup column. $
 *
 * -description & & control & m &
 *         Widget which shows a listbox item description, third item markup column. $
 *
 * @end{table}
 */

extern void check_response(network::connection res, const config& data);
extern void level_to_gamestate(config& level, command_pool& replay_data, game_state& state);
extern std::string get_color_string(int id);

REGISTER_DIALOG(mp_side_wait)

void tplayer_list_side_wait::init(twindow & w)
{
	active_game.init(w, _("Selected game"));

	tree = find_widget<ttree_view>(&w
			, "player_tree"
			, false
			, true);
}

tmp_side_wait::tmp_side_wait(hero_map& heros, game_display& gui, gamemap& gmap, const config& game_config,
			config& gamelist, bool observe)
	: legacy_result_(QUIT)
	, heros_(heros)
	, gui_(gui)
	, gmap_(gmap)
	, game_config_(game_config)
	, level_()
	, state_()
	, users_()
	, gamelist_(gamelist)
	, observe_(observe)
	, stop_updates_(false)
	, player_list_()
{
}

tmp_side_wait::~tmp_side_wait()
{
}

void tmp_side_wait::pre_show(CVideo& /*video*/, twindow& window)
{
	player_list_.init(window);

	waiting_ = find_widget<tlabel>(&window, "waiting", false, true);

	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "cancel_", false)
			, boost::bind(
				  &tmp_side_wait::cancel
				, this
				, boost::ref(window)));

	tlistbox& list = find_widget<tlistbox>(&window, "sides", false);
	sides_table_ = &list;

	join_game(window, observe_);

	// Add the map name to the title.
	tlabel* label = find_widget<tlabel>(&window, "title", false, true);
	label->set_label(std::string(_("Set Side")) + ": " + level_["name"].t_str());

	waiting_->set_label(_("Waiting for game to start..."));

	// Force first update to be directly.
	lobby_base::network_handler();
	lobby_update_timer_ = add_timer(game_config::lobby_network_timer
			, boost::bind(&lobby_base::network_handler, this)
			, true);
}

void tmp_side_wait::join_game(twindow& window, bool observe)
{
	//if we have got valid side data
	//the first condition is to make sure that we don't have another
	//WML message with a side-tag in it
	while (!level_.has_attribute("version") || !level_.child("side")) {
		network::connection data_res = dialogs::network_receive_dialog(gui_,
				_("Getting game data..."), level_);
		if (!data_res) {
			legacy_result_ = QUIT;
			window.close();
			return;
		}
		check_response(data_res, level_);
		if (level_.child("leave_game")) {
			legacy_result_ = QUIT;
			window.close();
			return;
		}
	}

	if (!observe) {
		//search for an appropriate vacant slot. If a description is set
		//(i.e. we're loading from a saved game), then prefer to get the side
		//with the same description as our login. Otherwise just choose the first
		//available side.
		const config *side_choice = NULL;
		int side_num = -1, nb_sides = 0;
		foreach (const config &sd, level_.child_range("side")) {
			if (sd["controller"] == "network" && sd["current_player"].empty()) {
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
			legacy_result_ = QUIT;
			window.close();
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

	generate_menu(window);
}

const game_state& tmp_side_wait::get_state()
{
	return state_;
}

void tmp_side_wait::start_game()
{
/*
	if (const config &stats = level_.child("statistics")) {
		statistics::fresh_stats();
		statistics::read_stats(stats);
	}
*/
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
			// DBG_NW << "Falling back to loading the old way.\n";
			level_to_gamestate(level_, replay_data_, state_);
		}
	}

	// LOG_NW << "starting game\n";
}


void tmp_side_wait::cancel(twindow& window)
{
	legacy_result_ = QUIT;
	window.close();
}

void tmp_side_wait::process_network_data(const config& data, const network::connection sock)
{
	twindow& window = *sides_table_->get_window();
	// ui::process_network_data(data, sock);
	
	if (data["message"] != "") {
		gui2::show_transient_message(gui_.video()
				, _("Response")
				, data["message"]);
	}
	if (data["failed"].to_bool()) {
		legacy_result_ = QUIT;
		window.close();
		return;
	} else if(data.child("stop_updates")) {
		stop_updates_ = true;
	} else if(data.child("start_game")) {
		// LOG_NW << "received start_game message\n";
		legacy_result_ = PLAY;
		window.close();
		return;
	} else if(data.child("leave_game")) {
		legacy_result_ = QUIT;
		window.close();
		return;
	} else if (const config &c = data.child("scenario_diff")) {
		// LOG_NW << "received diff for scenario... applying...\n";
		/** @todo We should catch config::error and then leave the game. */
		level_.apply_diff(c);
		generate_menu(window);
	} else if(data.child("side")) {
		level_ = data;
/*
		LOG_NW << "got some sides. Current number of sides = "
			<< level_.child_count("side") << ','
			<< data.child_count("side") << '\n';
*/
		generate_menu(window);
	}
}

void tmp_side_wait::update_playerlist()
{
	VALIDATE(player_list_.active_game.tree, "tmp_side_wait::update_playerlist, active_game.tree is null");

	player_list_.active_game.tree->clear();

	connected_user_list::iterator it;
	for (it = users_.begin(); it != users_.end(); ++ it) {
		connected_user& user = *it;
		tsub_player_list* target_list = &player_list_.active_game;

		assert(target_list->tree);

		std::string name = user.name;

		string_map tree_group_field;
		std::map<std::string, string_map> tree_group_item;

		/*** Add tree item ***/
		tree_group_field["label"] = decide_player_iocn(it->controller_);
		tree_group_item["icon"] = tree_group_field;

		tree_group_field["label"] = name;
		tree_group_field["use_markup"] = "true";
		tree_group_item["name"] = tree_group_field;

		ttree_view_node& player =
				target_list->tree->add_child("player", tree_group_item);
	}

	player_list_.active_game.auto_hide();
}

void tmp_side_wait::generate_menu(twindow& window)
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
/*
	if (sides_table_->get_item_count()) {
		return;
	}
*/
	std::vector<std::string> details;

	users_.clear();
	sides_table_->clear();
	int side = 0;
	foreach (const config &sd, level_.child_range("side")) {
		if (!sd["allow_player"].to_bool(true)) {
			side ++;
			continue;
		}

		if (!sd["current_player"].empty()) {
			const std::string player_id = sd["current_player"].str();
			connected_user_list::const_iterator it = users_.begin();
			for (; it != users_.end(); ++ it) {
				if (it->name == player_id) {
					break;
				}
			}
			if (it == users_.end()) {
				users_.push_back(connected_user(player_id, (player_id == preferences::login())? CNTR_LOCAL: CNTR_NETWORK, 0));
			}
		}

		// std::stringstream str;
		std::map<std::string, string_map> data;

		data["number"]["label"] = sd["side"].str();

		data["player"]["label"] = sd["user_description"];

		if (!sd["leader"].empty()) {
			hero& leader = heros_[sd["leader"].to_int()];
			data["faction"]["label"] = leader.name();
			data["portrait"]["label"] = leader.image();
		} else {
			data["faction"]["label"] = _("Random");
			data["portrait"]["label"] = "hero-64/random.png";
		}

		std::string feature_str;
		int selected_feature = sd["selected_feature"].to_int();
		if (selected_feature == COMBO_FEATURES_RANDOM) {
			data["feature"]["label"] = _("Random");

		} else if (selected_feature >= COMBO_FEATURES_MIN_VALID) {
			data["feature"]["label"] = hero::feature_str(hero::valid_features()[selected_feature - COMBO_FEATURES_MIN_VALID]);
		}

		data["income"]["label"] = sd["income"].str();

		data["color"]["label"] = get_color_string(side);
		// data["label"]["use_markup"] = "true";

		sides_table_->add_row(data);

		tgrid* grid_ptr = sides_table_->get_row_grid(side);
		tlabel* label = find_widget<tlabel>(grid_ptr, "color", false, true);
		label->set_use_markup(true);

		side ++;
	}

	update_playerlist();

	window.invalidate_layout();
}

void tmp_side_wait::post_show(twindow& window)
{
	remove_timer(lobby_update_timer_);
	lobby_update_timer_ = 0;
}

}
