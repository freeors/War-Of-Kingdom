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

#include "gui/dialogs/mp_side_creator.hpp"

#include "ai/configuration.hpp"
#include "game_display.hpp"
#include "game_preferences.hpp"
#include "gettext.hpp"
#include "gui/auxiliary/timer.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/dialogs/helper.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/tree_view_node.hpp"
#include "gui/dialogs/combo_box.hpp"
#include "gui/dialogs/message.hpp"
#include "formula_string_utils.hpp"
#include "map.hpp"
#include "savegame.hpp"
#include "replay.hpp"
#include "wml_separators.hpp"
#include "serialization/parser.hpp"
#include "actions.hpp"

#include <boost/foreach.hpp>
#include <boost/bind.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/gzip.hpp>

extern void regenerate_heros(hero_map& heros);

namespace gui2 {

const char* controller_names[] = {
	"network",
	"human",
	"ai",
	"null"
};

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

std::vector<std::set<int> > generate_team_names_from_side(const config& cfg)
{
	std::vector<std::set<int> > ret;

	BOOST_FOREACH (const config &s, cfg.child_range("side")) {
		const std::string& team_name = s["team_name"].str();
		std::vector<std::string> vstr = utils::split(team_name);
		std::set<int> mess;
		for (std::vector<std::string>::iterator it = vstr.begin(); it != vstr.end(); ++ it) {
			int to = lexical_cast_default<int>(*it);
			mess.insert(to);
		}
		ret.push_back(mess);
	}
	return ret;
}

std::vector<int> generate_allies_from_team_names(const std::vector<std::set<int> >& team_names)
{
	std::vector<int> ret;

	int current_ally_number = 1;
	size_t side_count = team_names.size();
	ret.resize(side_count, 0);
	for (size_t s = 0; s < side_count; s ++) {
		bool allied = false;
		for (std::vector<std::set<int> >::const_iterator it = team_names.begin(); it != team_names.end(); ++ it) {
			size_t s1 = std::distance(team_names.begin(), it);
			if (s1 == s || ret[s1]) {
				continue;
			}
			if (it->find(s + 1) != it->end()) {
				ret[s1] = current_ally_number;
				allied = true;
			}
		}
		if (allied) {
			ret[s] = current_ally_number ++;
		}		
	}
	return ret;
}

REGISTER_DIALOG(mp_side_creator)

void tplayer_list_side_creator::init(twindow & w)
{
	active_game.init(w, _("Selected game"));

	tree = find_widget<ttree_view>(&w
			, "player_tree"
			, false
			, true);
}

tmp_side_creator::tmp_side_creator(hero_map& heros, hero_map& heros_start, display& disp, gamemap& gmap, const config& game_config,
			config& gamelist, const mp_game_settings& params, const int num_turns,
			tcontroller default_controller, bool local_players_only)
	: legacy_result_(QUIT)
	, heros_(heros)
	, heros_start_(heros_start)
	, disp_(disp)
	, gmap_(gmap)
	, game_config_(game_config)
	, local_only_(local_players_only)
	, level_()
	, state_()
	, params_(params)
	, num_turns_(num_turns)
	, era_cfg_(NULL)
	, era_sides_()
	, factions_()
	, player_types_()
	, player_factions_()
	, allies_()
	, sides_()
	, users_()
	, default_controller_((tcontroller)default_controller)
	, gamelist_(gamelist)
	, player_list_()
{
}

tmp_side_creator::~tmp_side_creator()
{
}

void tmp_side_creator::pre_show(CVideo& /*video*/, twindow& window)
{
	std::stringstream strstr;

	player_list_.init(window);

	waiting_ = find_widget<tlabel>(&window, "waiting", false, true);
	launch_ = find_widget<tbutton>(&window, "launch", false, true);

	connect_signal_mouse_left_click(
			  *launch_
			, boost::bind(
				&tmp_side_creator::launch
				, this
				, boost::ref(window)));

	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "cancel_", false)
			, boost::bind(
				  &tmp_side_creator::cancel
				, this
				, boost::ref(window)));

	tlistbox& list = find_widget<tlistbox>(&window, "sides", false);
	sides_table_ = &list;

	load_game(window);
	if (legacy_result_ == CREATE) {
		join();
		return;
	}

	strstr.str("");
	strstr << tintegrate::generate_img(unit_types.genus(tent::turn_based? tgenus::TURN_BASED: tgenus::HALF_REALTIME).icon());
	strstr << _("Set Side");
	strstr << ": " << params_.name << " - " << level_["name"].t_str();
	tlabel* label = find_widget<tlabel>(&window, "title", false, true);
	label->set_label(strstr.str());

	if (level_["id"].empty()) {
		throw config::error(_("The scenario is invalid because it has no id."));
	}
	lists_init();
	if (sides_.empty()) {
		throw config::error(_("The scenario is invalid because it has no sides."));
	}
	// Send Initial information
	config response;
	config& create_game = response.add_child("create_game");
	create_game["name"] = params_.name;
	if (params_.password.empty() == false) {
		create_game["password"] = params_.password;
	}

	/*
	// The number of human-controlled sides is important to know
	// to let the server decide how many players can join this game
	int human_sides = 0;
	config::child_list cfg_sides = current_config()->get_children("side");
	for (config::child_list::const_iterator side = cfg_sides.begin(); side != cfg_sides.end(); side++){
		if ((**side)["controller"] == "human"){
			human_sides++;
		}
	}
	create_game["human_sides"] = lexical_cast<std::string>(human_sides);*/
	network::send_data(lobby->chat, response);

	update_user_combos();
	// Take the first available side or available side with id == login

	// Updates the "level_" variable, now that sides are loaded
	update_level();
	refresh_launch();
	
	// If we are connected, send data to the connected host
	network::send_data(lobby->chat, level_);

	join();
}

const game_state& tmp_side_creator::get_state()
{
	return state_;
}

std::string tmp_side_creator::form_binary_header(int type, int len) const
{
	std::stringstream header;

	config cfg;
	config& binary = cfg.add_child("change_faction");
	binary["type"] = type;
	binary["len"] = len;
	::write(header, cfg);

	return header.str();
}

void tmp_side_creator::send_binary_data(char* buf, int data_len, int buf_len) const
{
	std::stringstream strstr;

	std::iostream ifile(strstr.rdbuf());
	ifile.write(buf, data_len);

	boost::iostreams::filtering_stream<boost::iostreams::input> in;
	in.push(boost::iostreams::gzip_compressor());
	in.push(ifile);

	int len = in.read(buf, buf_len).gcount();
	network::send_raw_data(buf, len, 0);
}

void tmp_side_creator::start_game()
{
	// Resolves the "random faction", "random gender" and "random message"
	for (side_list::iterator itor = sides_.begin(); itor != sides_.end();
			++itor) {

		itor->resolve_random();
	}
	// Make other clients not show the results of resolve_random().
	config lock;
	lock.add_child("stop_updates");
	network::send_data(lobby->chat, lock);
	update_and_send_diff(true);

	// Send replay_data_
	std::string binary_header;
	const int min_buf_len = 1024;
	size_t pos;
	int buf_len = 0;
	char* buf = NULL;

	if (params_.saved_game) {
		binary_header = form_binary_header(BINARY_HEROS, heros_.file_size());

		int hero_data_len = binary_header.size() + 1 + heros_.file_size();
		hero_data_len = std::max(hero_data_len, min_buf_len);
		if (buf_len < hero_data_len) {
			if (buf) {
				free(buf);
			}
			buf_len = hero_data_len;
			buf = (char*)malloc(buf_len);
		}

		memcpy(buf, binary_header.c_str(), binary_header.size());
		buf[binary_header.size()] = '\0';

		pos = binary_header.size() + 1;
		heros_.map_to_mem((uint8_t*)buf + pos);
		pos += heros_.file_size();

		send_binary_data(buf, pos, buf_len);

		// start heros
		binary_header = form_binary_header(BINARY_HEROS_START, heros_.file_size());

		memcpy(buf, binary_header.c_str(), binary_header.size());
		buf[binary_header.size()] = '\0';

		pos = binary_header.size() + 1;
		heros_start_.map_to_mem((uint8_t*)buf + pos);
		pos += heros_.file_size();

		send_binary_data(buf, pos, buf_len);

		// group
		binary_header = form_binary_header(BINARY_GROUP, runtime_groups::size());

		int group_len = binary_header.size() + 1 + runtime_groups::size();
		group_len = std::max(group_len, min_buf_len);
		if (buf_len < group_len) {
			if (buf) {
				free(buf);
			}
			buf_len = group_len;
			buf = (char*)malloc(buf_len);
		}
		memcpy(buf, binary_header.c_str(), binary_header.size());
		buf[binary_header.size()] = '\0';

		pos = binary_header.size() + 1;
		runtime_groups::to_mem((uint8_t*)buf + pos);
		pos += runtime_groups::size();
		send_binary_data(buf, pos, buf_len);

		// replay data
		if (replay_data_.pool_pos_vsize()) {
			int data_len = 4 * sizeof(int) + replay_data_.pool_data_gzip_size() + replay_data_.pool_pos_vsize() * sizeof(unsigned int);
			binary_header = form_binary_header(BINARY_REPLAY, data_len);

			int replay_len = binary_header.size() + 1 + data_len;
			replay_len = std::max(replay_len, min_buf_len);
			if (buf_len < replay_len) {
				if (buf) {
					free(buf);
				}
				buf_len = replay_len;
				buf = (char*)malloc(buf_len);
			}

			memcpy(buf, binary_header.c_str(), binary_header.size());
			buf[binary_header.size()] = '\0';

			pos = binary_header.size() + 1;
			int size = replay_data_.pool_data_size();
			memcpy(buf + pos, &size, sizeof(int));
			pos += sizeof(int);
			size = replay_data_.pool_data_gzip_size();
			memcpy(buf + pos, &size, sizeof(int));
			pos += sizeof(int);
			size = replay_data_.pool_pos_size();
			memcpy(buf + pos, &size, sizeof(int));
			pos += sizeof(int);
			size = replay_data_.pool_pos_vsize();
			memcpy(buf + pos, &size, sizeof(int));
			pos += sizeof(int);

			// pool data
			memcpy(buf + pos, replay_data_.pool_data(), replay_data_.pool_data_gzip_size());
			pos += replay_data_.pool_data_gzip_size();
			// pool pos
			memcpy(buf + pos, replay_data_.pool_pos(), replay_data_.pool_pos_vsize() * sizeof(unsigned int));
			pos += replay_data_.pool_pos_vsize() * sizeof(unsigned int);

			send_binary_data(buf, pos, buf_len);
		}

		// side data.
		// waiter should receive it, but not special command.
		// it must send at last, make game_config::savegame_cache be side data.
		unit_segment2* sides = (unit_segment2*)game_config::savegame_cache;
		if (sides->size_) {
			binary_header = form_binary_header(BINARY_SIDE, sides->size_);

			int side_len = binary_header.size() + 1 + sides->size_;
			side_len = std::max(side_len, min_buf_len);
			if (buf_len < side_len) {
				if (buf) {
					free(buf);
				}
				buf_len = side_len;
				buf = (char*)malloc(buf_len);
			}
			memcpy(buf, binary_header.c_str(), binary_header.size());
			buf[binary_header.size()] = '\0';

			pos = binary_header.size() + 1;
			memcpy(buf + pos, game_config::savegame_cache, sides->size_);
			pos += sides->size_;

			send_binary_data(buf, pos, buf_len);
		}

	} else {
		// heros_ is generated automaticly, it is necessary to keep same at start.
		heros_start_ = heros_;
	}
	if (buf) {
		free(buf);
	}

	// Build the gamestate object after updating the level
	state_.classification().mode = mode_tag::rfind(mode_tag::SCENARIO);
	level_to_gamestate(level_, replay_data_, state_);

	if (!params_.saved_game) {
		users_2_groups(users_, member_users_);
	}

	network::send_data(lobby->chat, config("start_game"));
}

void tmp_side_creator::launch(twindow& window)
{
	legacy_result_ = PLAY;
	window.close();
}

void tmp_side_creator::cancel(twindow& window)
{
/*
	if (network::nconnections() > 0) {
		config cfg;
		cfg.add_child("leave_game");
		network::send_data(lobby->chat, cfg);
	}
*/
	legacy_result_ = QUIT;
	window.close();
}

void tmp_side_creator::player(twindow& window, int side)
{
	sides_[side].player(window);
}

void tmp_side_creator::faction(twindow& window, int side)
{
	sides_[side].faction(window);
}

void tmp_side_creator::ally(twindow& window, int side)
{
	sides_[side].ally(window);
}

void tmp_side_creator::gold(twindow& window, int side)
{
	sides_[side].gold(window);
}

void tmp_side_creator::income(twindow& window, int side)
{
	sides_[side].income(window);
}

const tgroup& tmp_side_creator::saved_allow_username(const std::string& username)
{
	// once load, runtime_groups::gs became valid.
	std::vector<std::string> usernames;
	std::map<int, tgroup>::const_iterator choice = runtime_groups::gs.end();
	for (std::map<int, tgroup>::const_iterator it = runtime_groups::gs.begin(); it != runtime_groups::gs.end(); ++ it) {
		const std::string username2 = it->second.leader().name();
		if (username2 == username) {
			choice = it;
		}
		if (member_users_.find(username2) == member_users_.end()) {
			member_users_.insert(std::make_pair(username2, it->second.to_membership()));
		}
		usernames.push_back(username2);
	}
	if (choice == runtime_groups::gs.end()) {
		std::stringstream allow, err;
		for (std::vector<std::string>::const_iterator it2 = usernames.begin(); it2 != usernames.end(); ++ it2) {
			if (it2 != usernames.begin()) {
				allow << ", ";
			}
			allow << *it2;
		}
		utils::string_map i18n_symbols;
		i18n_symbols["allow"] = tintegrate::generate_format(allow.str(), "green");
		i18n_symbols["exclude"] = tintegrate::generate_format(username, "red");
		std::string message = vgettext("The save game reserved for $allow, don't support $exclude.", i18n_symbols);
		gui2::show_error_message(disp_.video(), message);
		return null_group;
	}

	connected_user_list::iterator it = users_.begin();
	for (; it != users_.end(); ++ it) {
		connected_user& user = *it;
		if (user.name == username) {
			user.group.reset();
			user.group = choice->second;
			break;
		}
	}
	return choice->second;
}

void tmp_side_creator::load_game(twindow& window)
{
	std::vector<std::set<int> > team_names;

	// Adds the current user as default user.
	// load_game may result group invalid(save_game), use group before it.
	users_.push_back(connected_user(preferences::login(), CNTR_LOCAL, 0));
	
	if (params_.saved_game) {
		try {
			savegame::loadgame load(disp_, heros_, game_config_, state_);
			load.load_multiplayer_game(heros_, heros_start_);
			// heros in tgroup is pointer to heros_, now heros_ content is changed, these pointer shoud be invalid!
			// in order to clue error on immediately, reset group.
			// remide one rlue: don't use tgroup duration play_controller!
			group.reset();

			// heros_ changed, update groups in users_.
			if (!saved_allow_username(preferences::login()).valid()) {
				throw load_game_cancelled_exception();
			}

			load.fill_mplevel_config(level_);
			replay_data_ = state_.replay_data;

			team_names = generate_team_names_from_side_mem(game_config::savegame_cache);
		}
		catch (load_game_cancelled_exception){
			regenerate_heros(heros_);
			heros_start_ = heros_;

			legacy_result_ = CREATE;
			window.close();
			return;
		}
	} else {
		member_users_.insert(std::make_pair(group.leader().name(), group.to_membership()));
		regenerate_hero_map_from_users(disp_, heros_, users_, member_users_);

		if (params_.siege_mode) {
			const config& campaign = game_config_.find_child("campaign", "id", game_config::campaign_id_siege);
			std::string id = campaign["id"].str();
			std::string scenario = campaign["first_scenario"].str();
			std::string campaign_type = "scenario";
			params_.scenario_data = load_campagin_scenario(id, scenario, campaign_type);

			int sides = params_.scenario_data.child_count("side");
			for (int n = 0; n < sides; n ++) {
				config& side = params_.scenario_data.child("side", n);
				config& sub_cfg = side.child("artifical");
				std::string economy_area = sub_cfg["economy_area"].str();
				map_location city_loc = map_location(sub_cfg["x"].to_int(), sub_cfg["y"].to_int());
				side.clear();
				side["side"] = n + 1;
				side["team_name"] = n + 1;
				side["controller"] = "human";
				side["economy_area"] = economy_area;
				side["city_x"] = city_loc.x;
				side["city_y"] = city_loc.y;
			}
		} 
		level_.clear();
		params_.mp_scenario = params_.scenario_data["id"].str();
		level_.merge_with(params_.scenario_data);
		level_["turns"] = num_turns_;
		level_.add_child("multiplayer", params_.to_config());

		params_.hash = level_.hash();

		level_["experience_modifier"] = params_.xp_modifier;
		level_["random_seed"] = state_.rng().get_random_seed();
		level_["fog"] = params_.fog_game;
		level_["shroud"] = params_.shroud_game;
/*
		BOOST_FOREACH (const config &s, level_.child_range("side")) {
			const std::string& team_name = s["team_name"].str();
			std::vector<std::string> vstr = utils::split(team_name);
			std::set<int> mess;
			for (std::vector<std::string>::iterator it = vstr.begin(); it != vstr.end(); ++ it) {
				int to = lexical_cast_default<int>(*it);
				mess.insert(to);
			}
			team_names.push_back(mess);
		}
*/
		team_names = generate_team_names_from_side(level_);
	}

	allies_ = generate_allies_from_team_names(team_names);
/*
	int current_ally_number = 1;
	size_t side_count = team_names.size();
	allies_.resize(side_count, 0);
	for (size_t s = 0; s < side_count; s ++) {
		bool allied = false;
		for (std::vector<std::set<int> >::iterator it = team_names.begin(); it != team_names.end(); ++ it) {
			size_t s1 = std::distance(team_names.begin(), it);
			if (s1 == s || allies_[s1]) {
				continue;
			}
			if (it->find(s + 1) != it->end()) {
				allies_[s1] = current_ally_number;
				allied = true;
			}
		}
		if (allied) {
			allies_[s] = current_ally_number ++;
		}		
	}
*/

	std::string era = params_.mp_era;
	if (params_.saved_game) {
		if (const config &c = level_.child("snapshot").child("era"))
			era = c["id"].str();
	}

	// Initialize the list of forces.
	const config& topest_cfg = game_config_;
	factions_.clear();
	BOOST_FOREACH (const config &e, topest_cfg.child_range("faction")) {
		factions_.push_back(std::make_pair(&e, HEROS_INVALID_SIDE));
	}

	// Initialize the list of sides available for the current era.
	const config &era_cfg = game_config_.find_child("era", "id", era);
	era_cfg_ = const_cast<config*>(&era_cfg);
	if (!era_cfg) {
		if (!params_.saved_game) {
			utils::string_map i18n_symbols;
			i18n_symbols["era"] = era;
			throw config::error(vgettext("Cannot find era $era", i18n_symbols));
		}
		// FIXME: @todo We should tell user about missing era but still load game
		// WRN_CF << "Missing era in MP load game " << era << "\n";
	} else {
		era_sides_.clear();
		BOOST_FOREACH (const config &e, era_cfg.child_range("multiplayer_side")) {
			era_sides_.push_back(&e);
		}
		level_.add_child("era", era_cfg);
	}

	// gold_title_label_.hide(params_.saved_game);
	// income_title_label_.hide(params_.saved_game);

	// This will force connecting clients to be using the same version number as us.
	level_["version"] = game_config::version;

	level_["observer"] = params_.allow_observers;

	if(level_["objectives"].empty()) {
		level_["objectives"] = "*" + t_string(N_("Victory:"), "wesnoth") + "\n. " +
			t_string(N_("Defeat enemy leader(s)"), "wesnoth");
	}
}

config* tmp_side_creator::current_config()
{
	config* cfg_level = NULL;

	//It might make sense to invent a mechanism of some sort to check whether a config node contains information
	//that you can load from(side information, specifically)
	config &snapshot = level_.child("snapshot");
	if (snapshot && snapshot.child("side")) {
		// Savegame
		cfg_level = &snapshot;
	} else if (!level_.child("side")) {
		// Start-of-scenario save, the info has to be taken from the starting_pos
		cfg_level = &state_.starting_pos;
	} else {
		// Fresh game, no snapshot available
		cfg_level = &level_;
	}

	return cfg_level;
}

void tmp_side_creator::lists_init()
{
	// Options
	if (!local_only_) {
		player_types_.push_back(std::make_pair(_("Network Player"), CNTR_NETWORK));
	} else {
		player_types_.push_back(std::make_pair(_("Local Player"), CNTR_LOCAL));
	}
	player_types_.push_back(std::make_pair(_("Computer Player"), CNTR_COMPUTER));
	player_types_.push_back(std::make_pair(_("Empty"), CNTR_EMPTY));

	BOOST_FOREACH (const config *faction, era_sides_) {
		player_factions_.push_back((*faction)["name"]);
	}

	// AI algorithms
	const config &era = level_.child("era");
	// ai::configuration::add_era_ai_from_config(era);

	// Factions
	config::child_itors sides = current_config()->child_range("side");

	// Populates "sides_" from the level configuration
	int index = 0;
	BOOST_FOREACH (const config &s, sides) {
		sides_.push_back(side(sides_table_, *this, s, index++));
	}
	int offset=0;
	// This function must be called after the sides_ vector is fully populated.
	for(side_list::iterator s = sides_.begin(); s != sides_.end(); ++s) {
		const int side_num = s - sides_.begin();
		const int spos = 60 * (side_num-offset);
		if(!s->allow_player()) {
			offset++;
			continue;
		}

		// s->add_widgets_to_scrollpane(scroll_pane_, spos);
	}
}

void tmp_side_creator::update_level()
{
	// Import all sides into the level
	level_.clear_children("side");
	for(side_list::const_iterator itor = sides_.begin(); itor != sides_.end();
			++itor) {

		level_.add_child("side", itor->get_config());
	}
}

// @map: directly from tgroup or membership.
std::string get_player_map_data(const std::string& map, const config& game_config)
{
	std::string map_data;
	if (!map.empty()) {
		if (verify_siege_map_data(game_config, map)) {
			map_data = map;
		}
	}
	if (map_data.empty()) {
		const config& map_scenario = game_config.find_child("multiplayer", "id", game_config::campaign_id_siege);
		map_data = map_scenario["map_data"].str();
	}
	return map_data;
}

void tmp_side_creator::update_and_send_diff(bool update_time_of_day)
{
	config old_level = level_;
	update_level();
	if (update_time_of_day) {
		// Set random start ToD
		tod_manager tod_mng(level_, level_["turns"], &state_);

		if (params_.siege_mode && !params_.saved_game) {
			std::string attacker_map_data = get_player_map_data(member_users_.find(sides_[0].player_id_)->second.map, game_config_);
			std::string defender_map_data = get_player_map_data(member_users_.find(sides_[1].player_id_)->second.map, game_config_);
			level_["map_data"] = combine_map(defender_map_data, attacker_map_data, true);
		}
	}

	config diff = level_.get_diff(old_level);
	if (!diff.empty()) {
		config scenario_diff;
		scenario_diff.add_child("scenario_diff", diff);
		network::send_data(lobby->chat, scenario_diff);
	}
}

bool tmp_side_creator::sides_ready() const
{
	for (side_list::const_iterator itor = sides_.begin(); itor != sides_.end(); ++itor) {
		if (!itor->ready_for_start()) {
			return false;
		}
	}
	return true;
}

bool tmp_side_creator::sides_available() const
{
	for(side_list::const_iterator itor = sides_.begin(); itor != sides_.end(); ++itor) {
		if (itor->available())
			return true;
	}
	return false;
}

bool tmp_side_creator::can_start_game() const
{
	if (!sides_ready()) {
		return false;
	}

	/*
	 * If at least one human player is slotted with a player/ai we're allowed
	 * to start. Before used a more advanced test but it seems people are
	 * creative in what is used in multiplayer [1] so use a simpler test now.
	 * [1] http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=568029
	 */
	size_t local_players = 0;
	size_t empty_players = 0;
	std::set<int> ally_count;
	std::set<std::string> network_players;
	std::vector<std::string> player_ids;
	BOOST_FOREACH (const side& s, sides_) {
		if (!s.player_id_.empty()) {
			player_ids.push_back(s.player_id_);
		}
		if (s.get_controller() == CNTR_LOCAL) {
			local_players ++;
			ally_count.insert(s.ally_);
		} else if (s.get_controller() == CNTR_EMPTY) {
			empty_players ++;
		} else {
			ally_count.insert(s.ally_);
			if (s.get_controller() == CNTR_NETWORK) {
				if (network_players.find(s.get_player_id()) == network_players.end()) {
					network_players.insert(s.get_player_id());
				} else {
					return false;
				}
			}
		}
	}
	bool can = local_players == 1 && (local_players + empty_players) < sides_.size() && (ally_count.size() > 1 || !*ally_count.begin());
	if (can && params_.siege_mode) {
		can = player_ids.size() == 2;
	}
	return can;
}

void tmp_side_creator::refresh_launch()
{
	waiting_->set_label(can_start_game() ? ""
			: sides_available()
				? _("Waiting for players to join...")
				: _("Waiting for players to choose factions..."));
	launch_->set_active(can_start_game());
}

connected_user_list::iterator tmp_side_creator::find_player(const std::string& id)
{
	connected_user_list::iterator itor;
	for (itor = users_.begin(); itor != users_.end(); ++itor) {
		if (itor->name == id)
			break;
	}
	return itor;
}

int tmp_side_creator::find_player_side(const std::string& id) const
{
	side_list::const_iterator itor;
	for (itor = sides_.begin(); itor != sides_.end(); ++itor) {
		if (itor->get_player_id() == id)
			break;
	}

	if (itor == sides_.end())
		return -1;

	return itor - sides_.begin();
}

void tmp_side_creator::update_user_combos()
{
	player_xtypes_ = player_types_;
	if (!local_only_) {
		for (connected_user_list::const_iterator it = users_.begin(); it != users_.end(); ++ it) {
			player_xtypes_.push_back(std::make_pair(it->name, it->controller + CNTR_LAST));
		}
	}
	for (side_list::iterator it = sides_.begin(); it != sides_.end(); ++ it) {
		it->update_player_id();
	}
			
	update_playerlist();
}

void tmp_side_creator::update_playerlist()
{
	VALIDATE(player_list_.active_game.tree, "tmp_side_create::update_playerlist, active_game.tree is null");

	player_list_.active_game.tree->clear();

	connected_user_list::iterator it;
	for (it = users_.begin(); it != users_.end(); ++ it) {
		connected_user& user = *it;
		tsub_player_list* target_list = &player_list_.active_game;

		std::string name = user.name;

		string_map tree_group_field;
		std::map<std::string, string_map> tree_group_item;

		/*** Add tree item ***/
		tree_group_field["label"] = decide_player_iocn(it->controller);
		tree_group_item["icon"] = tree_group_field;

		tree_group_field["label"] = name;
		tree_group_field["use_markup"] = "true";
		tree_group_item["name"] = tree_group_field;

		ttree_view_node& player =
				target_list->tree->add_child("player", tree_group_item);
	}

	player_list_.active_game.auto_hide();
}

bool tmp_side_creator::handle(int tag, tsock::ttype type, const config& data)
{
	if (type == tsock::t_disconnected && !local_only_) {
		legacy_result_ = QUIT;
		sides_table_->get_window()->set_retval(twindow::CANCEL);
	}
	if (type != tsock::t_data) {
		return false;
	}

	if (data.child("leave_game")) {
		twindow& window = *launch_->get_window();
		legacy_result_ = QUIT;
		window.close();
		return true;
	}

	if (!data["side_drop"].empty()) {
		unsigned side_drop = data["side_drop"].to_int() - 1;
		if (side_drop < sides_.size()) {
			connected_user_list::iterator player = find_player(sides_[side_drop].get_player_id());
			sides_[side_drop].reset(sides_[side_drop].get_controller());
			if (player != users_.end()) {
				users_.erase(player);
				update_user_combos();
			}
			update_and_send_diff();
			refresh_launch();
			return true;
		}
	}

	if (!data["side"].empty()) {
		unsigned side_taken = data["side"].to_int() - 1;

		// Checks if the connecting user has a valid and unique name.
		const std::string name = data["name"];
		if (name.empty()) {
			config response;
			response["failed"] = true;
			network::send_data(lobby->chat, response);
			// ERR_CF << "ERROR: No username provided with the side.\n";
			return true;
		}

		connected_user_list::iterator player = find_player(name);
		if(player != users_.end()) {
			/**
			 * @todo Seems like a needless limitation to only allow one side
			 * per player.
			 */
			if(find_player_side(name) != -1) {
				config response;
				response["failed"] = true;
				response["message"] = "The nick '" + name + "' is already in use.";
				network::send_data(lobby->chat, response);
				return true;
			} else {
				users_.erase(player);
				config observer_quit;
				observer_quit.add_child("observer_quit")["name"] = name;
				network::send_data(lobby->chat, observer_quit);
				update_user_combos();
			}
		}

		// Assigns this user to a side
		if (side_taken < sides_.size())
		{
			if(!sides_[side_taken].available(name)) {
				// This side is already taken.
				// Try to reassing the player to a different position.
				side_list::const_iterator itor;
				side_taken = 0;
				for (itor = sides_.begin(); itor != sides_.end();
						++itor, ++side_taken) {
					if(itor->available())
						break;
				}

				if(itor == sides_.end()) {
					config response;
					response["failed"] = true;
					network::send_data(lobby->chat, response);
					config kick;
					kick["username"] = data["name"];
					config res;
					res.add_child("kick", kick);
					network::send_data(lobby->chat, res);
					update_user_combos();
					update_and_send_diff();
					// ERR_CF << "ERROR: Couldn't assign a side to '" << name << "'\n";
					return true;
				}
			}

			if (params_.saved_game) {
				const tgroup& g = saved_allow_username(name);
				if (!g.valid()) {
					return true;
				}
				// Adds the name to the list
				users_.push_back(connected_user(name, CNTR_NETWORK, lobby->chat.conn()));
				users_.back().group = g;
			} else {
				// Adds the name to the list				
				users_.push_back(connected_user(name, CNTR_NETWORK, lobby->chat.conn()));
				// it will modify heros_, don't execute when saved_game.
				regenerate_hero_map_from_users(disp_, heros_, users_, member_users_);
			}

			sides_[side_taken].import_network_user(data);
			update_user_combos();

			// Go thought and check if more sides are reserved
			// For this player
			std::for_each(sides_.begin(), sides_.end(), boost::bind(&tmp_side_creator::take_reserved_side, this,_1, data));
			refresh_launch();
			update_and_send_diff();

			// LOG_NW << "sent player data\n";

		} else {
			// ERR_CF << "tried to take illegal side: " << side_taken << '\n';
			config response;
			response["failed"] = true;
			network::send_data(lobby->chat, response);
		}
		return true;
	}

	if (const config &change_faction = data.child("change_faction")) {
		int side_taken = find_player_side(change_faction["name"]);
		if(side_taken != -1) {
			sides_[side_taken].import_network_user(change_faction);
			refresh_launch();
			update_and_send_diff();
		}
		return true;
	}

	if (const config &c = data.child("observer")) {
		const t_string &observer_name = c["name"];
		if(!observer_name.empty()) {
			connected_user_list::iterator player = find_player(observer_name);
			if(player == users_.end()) {
				users_.push_back(connected_user(observer_name, CNTR_NETWORK, lobby->chat.conn()));
				update_user_combos();
				refresh_launch();
				update_and_send_diff();
			}
		}
		return true;
	}
	if (const config &c = data.child("observer_quit")) {
		const t_string &observer_name = c["name"];
		if(!observer_name.empty()) {
			connected_user_list::iterator player = find_player(observer_name);
			if(player != users_.end() && find_player_side(observer_name) == -1) {
				users_.erase(player);
				update_user_combos();
				refresh_launch();
				update_and_send_diff();
			}
		}
		return true;
	}
	return false;
}

void tmp_side_creator::take_reserved_side(tmp_side_creator::side& side, const config& data)
{
}

void tmp_side_creator::process_network_error(network::error& error)
{
	// If the problem isn't related to any specific connection,
	// it's a general error and we should just re-throw the error.
	// Likewise if we are not a server, we cannot afford any connection
	// to go down, so also re-throw the error.
	if(!error.socket || !network::is_server()) {
		error.disconnect();
		throw network::error(error.message);
	}

	bool changes = false;

	// A socket has disconnected. Remove it, and resets its side
	connected_user_list::iterator user;
	for(user = users_.begin(); user != users_.end(); ++user) {
		if(user->connection == error.socket) {
			changes = true;

			int i = find_player_side(user->name);
			if (i != -1)
				sides_[i].reset(default_controller_);

			break;
		}
	}
	if(user != users_.end()) {
		users_.erase(user);
		update_user_combos();
	}

	// Now disconnect the socket
	error.disconnect();

	// If there have been changes to the positions taken,
	// then notify other players
	if (changes) {
		update_and_send_diff();
		refresh_launch();
	}
}

std::map<int, std::string> tmp_side_creator::get_used_user_players() const
{
	std::map<int, std::string> ret;
	for (side_list::const_iterator it = sides_.begin(); it != sides_.end(); ++ it) {
		const side& s = *it;
		if (!s.player_id_.empty()) {
			ret.insert(std::make_pair(s.index_, s.player_id_));
		}
	}
	return ret;
}

int tmp_side_creator::get_faction(int side) const
{
	int size = (int)factions_.size();

	for (int i = 0; i < size; i ++) {
		if (factions_[i].second == side) {
			return i;
		}
	}
	for (connected_user_list::const_iterator it = users_.begin(); it != users_.end(); ++ it) {
		if (it->side == side) {
			return size + std::distance(users_.begin(), it);
		}
	}
	return RANDOM_FACTION;
}

const config& tmp_side_creator::get_faction_cfg(int side)
{
	int faction_index = get_faction(side);
	int fix_factions = factions_.size();
	if (faction_index < fix_factions) {
		return *factions_[faction_index].first;
	} else {
		return users_[faction_index - fix_factions].group.to_faction_cfg(true, !params_.siege_mode);
	}
}

// get candiate factions from fix faction.
std::vector<int> tmp_side_creator::get_candidate_factions(int side) const
{
	std::vector<int> candidate;
	int size = (int)factions_.size();

	for (int i = 0; i < size; i ++) {
		if (factions_[i].second == HEROS_INVALID_SIDE || factions_[i].second == side) {
			candidate.push_back(i);
		}
	}
	return candidate;
}

// get candiate factions from username's faction.
std::vector<int> tmp_side_creator::get_candidate_factions_user(int side) const
{
	std::vector<int> candidate;

	for (connected_user_list::const_iterator it = users_.begin(); it != users_.end(); ++ it) {
		if (it->side == HEROS_INVALID_SIDE || it->side == side) {
			candidate.push_back(std::distance(users_.begin(), it));
		}
	}
	return candidate;
}

// according to side and username, find special index of users_.
int tmp_side_creator::get_user_faction(int side, std::string& username) const
{
	for (connected_user_list::const_iterator it = users_.begin(); it != users_.end(); ++ it) {
		if ((it->side == HEROS_INVALID_SIDE || it->side == side) && it->name == username) {
			return std::distance(users_.begin(), it);
		}
	}
	std::stringstream err;
	err << "tmp_side_creator::get_user_faction, cannot find ";
	err << tintegrate::generate_format(username, "red") << " to side #";
	err << tintegrate::generate_format(side + 1, "yellow");
	VALIDATE(false, err.str());
	return -1;
}

// factions[i].second
//   HEROS_INVALID_SIDE: idle
//   side_index: used by side_index
// @faction: RANDOM_FACTION, faction_index, or users_index
void tmp_side_creator::set_faction(int side, int faction)
{
	int size = (int)factions_.size();
	int i;
	bool found = false;
	for (i = 0; i < size; i ++) {
		if (factions_[i].second == side) {
			found = true;
			factions_[i].second = HEROS_INVALID_SIDE;
			break;
		}
	}
	if (!found) {
		for (connected_user_list::iterator it = users_.begin(); it != users_.end(); ++ it, i ++) {
			if (it->side == side) {
				found = true;
				it->side = HEROS_INVALID_SIDE;
				break;
			}
		}
	}
	if (faction != RANDOM_FACTION) {
		if (faction < size) {
			factions_[faction].second = side;
		} else {
			users_[faction - size].side = side;
		}
	}
}

void tmp_side_creator::post_show(twindow& window)
{
}

tmp_side_creator::side::side(tlistbox* sides_table, tmp_side_creator& parent, const config& cfg, int index) 
	: parent_(&parent)
	, cfg_(cfg)
	, index_(index)
	, player_id_(cfg["current_player"])
	, controller_(CNTR_NETWORK)
	, ally_(0)
	, color_(index)
	, gold_(cfg["gold"].to_int(100))
	, income_(cfg["income"])
	, faction_(cfg["faction"])
	, allow_player_(cfg["allow_player"].to_bool(true))
	, allow_changes_(cfg["allow_changes"].to_bool(true))
	, faction_changeable_(!parent_->params_.saved_game && cfg["leader"].empty())
	, changed_(false)
{
	// convert ai controllers
	if (cfg_["controller"] == "human_ai" || cfg_["controller"] == "network_ai") {
		cfg_["controller"] = "ai";
	}
	if (allow_player_ && faction_changeable_) {
		controller_ = parent_->default_controller_;
	} else {
		size_t i = CNTR_NETWORK;
		if(!allow_player_)
		{
			if(cfg_["controller"] == "null") {
				controller_ = CNTR_EMPTY;
			} else {
				cfg_["controller"] = controller_names[CNTR_COMPUTER];
				controller_ = static_cast<tcontroller>(CNTR_COMPUTER);
			}
		} else {
			for(; i != CNTR_LAST; ++i) {
				if(cfg_["controller"] == controller_names[i]) {
					controller_ = static_cast<tcontroller>(i);
					break;
				}
			}

		}
	}

	if (!parent_->local_only_ && parent_->params_.saved_game && !player_id_.empty()) {
		if (player_id_ == preferences::login()) {
			controller_ = CNTR_LOCAL;
		} else {
			controller_ = CNTR_NETWORK;
		}
	} else if (parent_->local_only_ && controller_ == CNTR_NETWORK) {
		controller_ = CNTR_LOCAL;
	}

	std::stringstream strstr;
	twindow& window = *sides_table->get_window();
	std::map<std::string, string_map> data;
	
	strstr.str("");
	strstr << index_ + 1;
	data["number"]["label"] = strstr.str();
	data["label"]["use_markup"] = "true";

	sides_table->add_row(data);
	tgrid* grid_ptr = sides_table->get_row_grid(index_);
	player_button_ = find_widget<tbutton>(grid_ptr, "player", false, true);
	connect_signal_mouse_left_click(
		*player_button_, boost::bind(&tmp_side_creator::player, parent_, boost::ref(window), index_));

	faction_button_ = find_widget<tbutton>(grid_ptr, "faction", false, true);
	connect_signal_mouse_left_click(
		*faction_button_, boost::bind(&tmp_side_creator::faction, parent_, boost::ref(window), index_));

	feature_button_ = find_widget<tbutton>(grid_ptr, "feature", false, true);

	ally_button_ = find_widget<tbutton>(grid_ptr, "ally", false, true);
	connect_signal_mouse_left_click(
		*ally_button_, boost::bind(&tmp_side_creator::ally, parent_, boost::ref(window), index_));

	gold_button_ = find_widget<tbutton>(grid_ptr, "gold", false, true);
	connect_signal_mouse_left_click(
		*gold_button_, boost::bind(&tmp_side_creator::gold, parent_, boost::ref(window), index_));

	income_button_ = find_widget<tbutton>(grid_ptr, "income", false, true);
	connect_signal_mouse_left_click(
		*income_button_, boost::bind(&tmp_side_creator::income, parent_, boost::ref(window), index_));

	feature_button_->set_active(false);
	if (!allow_player_ || parent_->params_.saved_game) {
		player_button_->set_active(false);
		faction_button_->set_active(false);
		ally_button_->set_active(false);
		gold_button_->set_active(false);
		income_button_->set_active(false);
	}
	
	ally_ = parent_->allies_[index_];

	// "Faction name" hack
	if (!faction_changeable_) {
		faction_ = 0;

		hero& leader = parent_->heros_[cfg["leader"].to_int()];

		faction_button_->set_active(false);
		faction_button_->set_label(leader.name());
		feature_button_->set_label(hero::feature_str(leader.side_feature_));
	
	} else {
		faction_button_->set_label(_("Random"));
		feature_button_->set_label(hero::feature_str(HEROS_NO_FEATURE));
	}

	if (cfg["not_recruit"].empty() && parent_->era_cfg_->has_attribute("not_recruit")) {
		cfg_["not_recruit"] = parent_->era_cfg_->get("not_recruit")->str();
	}
	if (cfg["build"].empty()) {
		strstr.str("");
		strstr << unit_types.find_wall()->id();
		strstr << ", " << unit_types.find_market()->id();
		// strstr << ", " << unit_types.find_tower()->id();
		cfg_["build"] = strstr.str();
	}

	update_ui();
}

bool tmp_side_creator::side::ready_for_start() const
{
	// sides without players are always ready
	if (!allow_player_) {
		return true;
	}

	//the host and the AI are always ready
	if ((controller_ == CNTR_COMPUTER) ||
		(controller_ == CNTR_EMPTY) ||
		(controller_ == CNTR_LOCAL))
		return true;

	// CNTR_NETWORK, decide by player_id
	return !player_id_.empty();
}

bool tmp_side_creator::side::available(const std::string& name) const
{
	if (name.empty()) {
		return allow_player_ && (controller_ == CNTR_NETWORK && player_id_.empty());
	}

	return allow_player_ && (controller_ == CNTR_NETWORK && player_id_.empty());
}

bool tmp_side_creator::side::allow_player() const
{
	return allow_player_;
}

void tmp_side_creator::side::update_controller_ui()
{
	if (player_id_.empty()) {
		// combo_controller_->set_selected(controller_ - (parent_->local_only_ ? 1 : 0));
	} else {
		connected_user_list::iterator player = parent_->find_player(player_id_);
	}
}

void tmp_side_creator::side::update_ui()
{
	std::stringstream strstr;
	if (ally_) {
		strstr << ally_;
	} else {
		strstr << "-";
	}
	ally_button_->set_label(strstr.str());

	strstr.str("");
	strstr << gold_;
	gold_button_->set_label(strstr.str());

	strstr.str("");
	strstr << income_;
	income_button_->set_label(strstr.str());
}

config tmp_side_creator::side::get_config() const
{
	std::stringstream strstr;
	config res;

	// If the user is allowed to change type, faction, leader etc,
	// then import their new values in the config.
	if (faction_changeable_ && !parent_->era_sides_.empty()) {
		// Merge the faction data to res
		res.append(*(parent_->era_sides_[faction_]));
		res["faction_name"] = res["name"];
	}
	res.append(cfg_);
	if (!cfg_.has_attribute("side") || cfg_["side"].to_int() != index_ + 1) {
		res["side"] = index_ + 1;
	}
	res["controller"] = controller_names[controller_];
	if (controller_ != CNTR_LOCAL || !player_id_.empty()) {
		res["current_player"] = player_id_;
	} else {
		res["current_player"] = preferences::login();
	}

	res["name"] = player_id_;

	res["allow_changes"] = faction_changeable_ && allow_changes_;

	if (faction_changeable_) {
		if (parent_->get_faction(index_) != RANDOM_FACTION) {
			// add content in [faction][/faction] to this side
			const config& faction_cfg = parent_->get_faction_cfg(index_);
			res["leader"] = faction_cfg["leader"];
			if (config& art_cfg = res.child("artifical")) {
				if (!art_cfg.has_attribute("service_heros")) {
					art_cfg["service_heros"] = faction_cfg["service_heros"];
					art_cfg["wander_heros"] = faction_cfg["wander_heros"];
				}
				art_cfg["economy_area"] = res["economy_area"].str();
			} else {
				config& new_art_cfg = res.add_child("artifical", faction_cfg.child("artifical"));
			
				new_art_cfg["side"] = index_ + 1;
				new_art_cfg["cityno"] = index_ + 1;

				if (res.has_attribute("city_x")) {
					new_art_cfg["x"] = res["city_x"].to_int();
					new_art_cfg["y"] = res["city_y"].to_int();
				} else {
					new_art_cfg["x"] = map_location().x;
					new_art_cfg["y"] = map_location().y;
				}

				new_art_cfg["service_heros"] = faction_cfg["service_heros"];
				new_art_cfg["wander_heros"] = faction_cfg["wander_heros"];
				new_art_cfg["economy_area"] = res["economy_area"].str();
			}
			res.remove_attribute("economy_area");
		}
	}

	strstr.str("");
	for (std::vector<side>::const_iterator it = parent_->sides_.begin(); it != parent_->sides_.end(); ++ it) {
		const side& s = *it;
		if (s.index_ != index_ && ally_ && s.ally_ == ally_) {
			if (!strstr.str().empty()) {
				strstr << ", ";
			}
			strstr << s.index_ + 1;
		}
	}
	res["team_name"] = strstr.str();

	res["allow_player"] = allow_player_;
	res["gold"] = gold_;
	res["income"] = income_;

	res["share_maps"] = parent_->params_.share_maps;
	res["share_view"] =  parent_->params_.share_view;
	res["village_gold"] = parent_->params_.village_gold;
	
	return res;
}

void tmp_side_creator::side::set_controller(tcontroller controller)
{
	controller_ = controller;
	player_id_ = "";

	update_ui();
}

tcontroller tmp_side_creator::side::get_controller() const
{
	return controller_;
}

void tmp_side_creator::side::update_player_id()
{
	int xtypes_index = -1;
	
	int i = 0, first_controller_index = -1;
	std::vector<std::pair<std::string, int> >::const_iterator itor;
	// same local and network as exchangable.
	for (itor = parent_->player_xtypes_.begin(); itor != parent_->player_xtypes_.end(); ++itor, i ++) {
		if (first_controller_index == -1 && (itor->second % CNTR_LAST == controller_)) {
			first_controller_index = i;
		}
		if (itor->first == player_id_ && (itor->second % CNTR_LAST == controller_)) {
			xtypes_index = i;
		}
	}
	if (xtypes_index == -1) {
		xtypes_index = first_controller_index;
	}
	if (parent_->player_xtypes_[xtypes_index].second >= CNTR_LAST) {
		player_id_ = parent_->player_xtypes_[xtypes_index].first;
	} else {
		player_id_ = "";
	}

	if (!parent_->local_only_ && parent_->player_xtypes_[xtypes_index].second >= CNTR_LAST) {
		// if select username player, change faction automaticly
		int distance = parent_->get_user_faction(index_, parent_->player_xtypes_[xtypes_index].first);
		const tgroup& g = parent_->users_[distance].group;

		faction_button_->set_label(g.leader().name());
		faction_button_->set_active(false);
		feature_button_->set_label(hero::feature_str(g.leader().side_feature_));

		int fix_factions = parent_->factions_.size();
		parent_->set_faction(index_, fix_factions + distance);

	} else {
		if (!parent_->params_.saved_game) {
			faction_button_->set_active(true);
		}
		int faction = parent_->get_faction(index_);
		if (faction != RANDOM_FACTION && faction >= (int)parent_->factions_.size()) {
			faction_button_->set_label(_("Random"));
			parent_->set_faction(index_, RANDOM_FACTION);
			feature_button_->set_label(hero::feature_str(HEROS_NO_FEATURE));
		}
	}

	player_button_->set_label(parent_->player_xtypes_[xtypes_index].first);
}

const std::string& tmp_side_creator::side::get_player_id() const
{
	return player_id_;
}

void tmp_side_creator::side::import_network_user(const config& data)
{
	player_id_ = data["name"].str();
	controller_ = CNTR_NETWORK;
}

void tmp_side_creator::side::reset(tcontroller controller_)
{
	player_id_ = "";
	controller_ = controller_;

	update_ui();
}

void tmp_side_creator::side::resolve_random()
{
	if (!faction_changeable_) {
		return;
	}
	
	if (parent_->get_faction(index_) == RANDOM_FACTION) {
		// Choose a random faction
		std::vector<int> candidate = parent_->get_candidate_factions(index_);
		// BUG!! candidate.size() may be zero
		parent_->set_faction(index_, candidate[rand() % candidate.size()]);
	}
}

void tmp_side_creator::side::player(twindow& window)
{
	std::stringstream strstr;

	std::vector<tval_str> items;
	std::map<int, std::string> used_user_players = parent_->get_used_user_players();

	std::vector<std::pair<std::string, int> >::iterator itor;
	for (itor = parent_->player_xtypes_.begin(); itor != parent_->player_xtypes_.end(); ++ itor) {
		if (itor->second >= CNTR_LAST) {
			bool usable = true;
			for (std::map<int, std::string>::const_iterator it2 = used_user_players.begin(); it2 != used_user_players.end(); ++ it2) {
				if (it2->second == itor->first && it2->first != index_) {
					// this username is used for other side.
					usable = false;
					break;
				}
			}
			if (!usable) {
				continue;
			}
		}
		strstr.str("");
		strstr << "<format>";
		if (itor->second % CNTR_LAST == CNTR_LOCAL) {
			strstr << "color='green' ";
		} else if (itor->second % CNTR_LAST == CNTR_NETWORK) {
			strstr << "color='blue' ";
		}
		strstr << "text='" << itor->first << "'</format>\n";

		items.push_back(tval_str(std::distance(parent_->player_xtypes_.begin(), itor), itor->first));
	}
	
	gui2::tcombo_box dlg(items, items.front().val);
	dlg.show(parent_->disp_.video());

	int selected = dlg.selected_val();
	tcontroller selected_controller = (tcontroller)(parent_->player_xtypes_[selected].second % CNTR_LAST);
	std::string selected_player_id = null_str;
	if (parent_->player_xtypes_[selected].second >= CNTR_LAST) {
		selected_player_id = parent_->player_xtypes_[selected].first;
	}

	if (selected_controller == controller_ && selected_player_id == player_id_) {
		// if not change, do nothing.
		return;
	}
	controller_ = selected_controller;
	player_id_ = selected_player_id;

	update_player_id();

	// changed, send!
	parent_->refresh_launch();
	parent_->update_and_send_diff();
}

void tmp_side_creator::side::faction(twindow& window)
{
	std::vector<std::string> names;
	std::vector<int> side_features;
	std::vector<tval_str> items;
	std::stringstream strstr;

	int fix_factions = parent_->factions_.size();
	std::vector<int> candidate;
	
	if (controller_ == CNTR_LOCAL || controller_ == CNTR_NETWORK) {
		candidate = parent_->get_candidate_factions_user(index_);
		for (std::vector<int>::const_iterator it = candidate.begin(); it != candidate.end(); ++ it) {
			int distance = *it;
			hero& leader = parent_->users_[distance].group.leader();

			items.push_back(tval_str(fix_factions + distance, IMAGE_PREFIX + std::string(leader.image()) + std::string("~SCALE(32, 40)") + COLUMN_SEPARATOR + leader.name()));
			names.push_back(leader.name());
			side_features.push_back(leader.side_feature_);
		}
	}

	items.push_back(tval_str(RANDOM_FACTION, IMAGE_PREFIX + std::string("units/random-dice.png") + std::string("~SCALE(32, 40)") + COLUMN_SEPARATOR + _("Random")));
	names.push_back(_("Random"));
	side_features.push_back(HEROS_NO_FEATURE);

	candidate = parent_->get_candidate_factions(index_);
	for (std::vector<int>::const_iterator i = candidate.begin(); i != candidate.end(); ++ i) {
		hero& leader = parent_->heros_[parent_->factions_[*i].first->get("leader")->to_int()];
		items.push_back(tval_str(*i ,IMAGE_PREFIX + std::string(leader.image()) + std::string("~SCALE(32, 40)") + COLUMN_SEPARATOR + leader.name()));
		names.push_back(leader.name());
		side_features.push_back(leader.side_feature_);
	}

	gui2::tcombo_box dlg(items, parent_->get_faction(index_));
	dlg.show(parent_->disp_.video());
	int selected = dlg.selected_index();

	faction_button_->set_label(names[selected]);
	feature_button_->set_label(hero::feature_str(side_features[selected]));

	parent_->set_faction(index_, items[selected].val);

	// changed, send!
	parent_->refresh_launch();
	parent_->update_and_send_diff();
}

void tmp_side_creator::side::ally(twindow& window)
{
	ally_ ++;
	ally_ %= parent_->sides_.size() + 1;

	std::stringstream strstr;
	if (ally_) {
		strstr << ally_;
	} else {
		strstr << "-";
	}
	ally_button_->set_label(strstr.str());

	// changed, send!
	parent_->refresh_launch();
	parent_->update_and_send_diff();
}

void tmp_side_creator::side::gold(twindow& window)
{
	std::vector<tval_str> items;
	
	items.push_back(tval_str(100, "100"));
	items.push_back(tval_str(150, "150"));
	items.push_back(tval_str(200, "200"));
	items.push_back(tval_str(300, "300"));

	gui2::tcombo_box dlg(items, gold_);
	dlg.show(parent_->disp_.video());

	int selected = dlg.selected_index();
	gold_ = items[selected].val;

	gold_button_->set_label(items[selected].str);

	// changed, send!
	parent_->refresh_launch();
	parent_->update_and_send_diff();
}

void tmp_side_creator::side::income(twindow& window)
{
	std::vector<tval_str> items;
	
	items.push_back(tval_str(0, "0"));
	items.push_back(tval_str(20, "20"));
	items.push_back(tval_str(50, "50"));
	items.push_back(tval_str(100, "100"));
	items.push_back(tval_str(150, "150"));
	items.push_back(tval_str(200, "200"));

	gui2::tcombo_box dlg(items, income_);
	dlg.show(parent_->disp_.video());

	int selected = dlg.selected_index();
	income_ = items[selected].val;

	income_button_->set_label(items[selected].str);

	// changed, send!
	parent_->refresh_launch();
	parent_->update_and_send_diff();
}

}
