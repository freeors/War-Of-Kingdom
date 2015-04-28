/* $Id: game.cpp 47572 2010-11-15 20:25:09Z shadowmaster $ */
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

#include "global.hpp"

#include "base_instance.hpp"
#include "SDL.h"
#include "SDL_mixer.h"

#include "SDL_stdinc.h"

#include "about.hpp"
#include "ai/configuration.hpp"
#include "config.hpp"
#include "config_cache.hpp"
#include "cursor.hpp"
#include "dialogs.hpp"
#include "game_display.hpp"
#include "game_preferences.hpp"
#include "builder.hpp"
#include "filesystem.hpp"
#include "font.hpp"
#include "formula.hpp"
#include "game_config.hpp"
#include "game_errors.hpp"
#include "gamestatus.hpp"
#include "gettext.hpp"
#include "gui/dialogs/group.hpp"
#include "gui/dialogs/campaign_selection.hpp"
#include "gui/dialogs/player_selection.hpp"
#include "gui/dialogs/tower_tent.hpp"
#include "gui/dialogs/siege_tent.hpp"
#include "gui/dialogs/language_selection.hpp"
#include "gui/dialogs/inapp_purchase.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/mp_method_selection.hpp"
#include "gui/dialogs/title_screen.hpp"
#include "gui/dialogs/preferences.hpp"
#include "gui/dialogs/create_hero.hpp"
#include "gui/dialogs/user_report.hpp"
#include "gui/dialogs/signin.hpp"
#include "gui/dialogs/user_message.hpp"
#include "gui/dialogs/transient_message.hpp"
#include "gui/dialogs/help_screen.hpp"
#include "gui/dialogs/book2.hpp"
#include "gui/dialogs/subcontinent.hpp"
#include "gui/dialogs/auto_scroll.hpp"
#include "gui/auxiliary/event/handler.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "help.hpp"
#include "hotkeys.hpp"
#include "language.hpp"
#include "loadscreen.hpp"
#include "log.hpp"
#include "map_exception.hpp"
#include "marked-up_text.hpp"
#include "multiplayer.hpp"
#include "network.hpp"
#include "playcampaign.hpp"
#include "preferences_display.hpp"
#include "replay.hpp"
#include "savegame.hpp"
#include "scripting/lua.hpp"
#include "sound.hpp"
#include "wml_exception.hpp"
#include "version.hpp"
#include "resources.hpp"
#include "card.hpp"
#include "formula_string_utils.hpp"
#include "play_controller.hpp"

#include "editor/editor_main.hpp"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>


#include <boost/foreach.hpp>

// Minimum stack cookie to prevent stack overflow on AmigaOS4
#ifdef __amigaos4__
const char __attribute__((used)) stackcookie[] = "\0$STACK: 16000000";
#endif

static lg::log_domain log_config("config");
#define ERR_CONFIG LOG_STREAM(err, log_config)
#define WRN_CONFIG LOG_STREAM(warn, log_config)
#define LOG_CONFIG LOG_STREAM(info, log_config)

#define LOG_GENERAL LOG_STREAM(info, lg::general)
#define WRN_GENERAL LOG_STREAM(warn, lg::general)
#define DBG_GENERAL LOG_STREAM(debug, lg::general)

static lg::log_domain log_network("network");
#define ERR_NET LOG_STREAM(err, log_network)

static lg::log_domain log_preprocessor("preprocessor");
#define LOG_PREPROC LOG_STREAM(info,log_preprocessor)

static bool less_campaigns_rank(const config &a, const config &b) {
	return a["rank"].to_int(1000) < b["rank"].to_int(1000);
}

namespace tent {

std::vector<tlobby_side_param> lobby_side_params;
int io_type;

// only used for calculate_5fields and when teams is empty.
hero* leader;

int human_leader_number = HEROS_INVALID_NUMBER;
int human_count = 0;
int ai_count = 0;
int employ_count = 0;
mode_tag::tmode mode = mode_tag::NONE;
std::pair<std::string, int> subcontinent;
int turns = -1;
int duel = -1;
bool turn_based = false;

void reset()
{
	lobby_side_params.clear();

	human_leader_number = HEROS_INVALID_NUMBER;
	human_count = 0;
	ai_count = 0;
	employ_count = 0;
	mode = mode_tag::NONE;
	subcontinent = std::make_pair(null_str, HEROS_ROAM_CITY);
	turns = -1;
	duel = -1;
}

bool tower_mode()
{
	return mode == mode_tag::TOWER || mode == mode_tag::SIEGE || mode == mode_tag::LAYOUT;
}

std::string subcontient_to_str()
{
	if (subcontinent.first.empty()) {
		return null_str;
	}
	std::stringstream strstr;
	strstr << subcontinent.first << "," << subcontinent.second;
	return strstr.str();
}

void subcontient_from_str(const std::string& str)
{
	std::vector<std::string> vstr = utils::split(str);
	if (vstr.size() == 2) {
		subcontinent = std::make_pair(vstr[0], lexical_cast_default<int>(vstr[1]));
	} else {
		subcontinent = std::make_pair(null_str, HEROS_ROAM_CITY);
	}
}

class lock
{
public:
	lock()
		: clear_(true)
	{}
	~lock()
	{
		if (clear_) {
			reset();
		}
	}

	void set_clear(bool value) { clear_ = value; }
private:
	bool clear_;
};

}

class game_instance: public base_instance
{
public:
	game_instance(int argc, char** argv);
	~game_instance();

	bool init_config(const bool force);
	bool play_screenshot_mode();

	void reload_changed_game_config();
	int show_preferences_dialog(display& disp, bool first);

	void fill_anim_tags(std::map<const std::string, int>& tags);
	void fill_anim(int at, const std::string& id, bool area, bool tpl, const config& cfg);

	bool is_loading() const;
	void clear_loaded_game() { game::load_game_exception::game.clear(); }
	bool load_game();

	void level_to_gamestate(bool fog, bool shroud, const std::string& map_data = null_str, bool modify_placing = false);
	int new_campaign(int catalog);
	void play_multiplayer(bool random_map);
	void inapp_purchase();
	
	void show_preferences();

	void launch_game(bool is_load_game);
	void play_replay();
	editor::EXIT_STATUS start_editor(const std::string& map_data = "");

	void start_wesnothd();

	void regenerate_heros(hero_map& heros, bool allow_empty);

	void start_siege_game(bool subcontinent);
	void start_scenario_game(bool subcontinent);
	void start_layout(const std::string& map_data);

	bool session_xmited() const { return session_xmited_; }
	void set_session_xmited(bool val) { session_xmited_ = val; }

private:
	void load_config2(const config& cfg);
	void load_game_cfg(const bool force);
	void load_campaign_cfg();
	void set_unit_data(config& gc);

	void mark_completed_campaigns(std::vector<config>& campaigns);

private:
	bool screenshot_mode_;
	std::string screenshot_map_, screenshot_filename_;

	hero_map heros_start_;
	card_map cards_;

	std::vector<bool> checked_card_;
	game_state state_;

	bool session_xmited_;
};

game_instance::game_instance(int argc, char** argv)
	: base_instance(argc, argv)
	, screenshot_mode_(false)
	, screenshot_map_()
	, screenshot_filename_()
	, cards_(game_config::path)
	, checked_card_()
	, state_()
	, session_xmited_(false)
{
	cards_.set_path(game_config::path);
}

int game_instance::show_preferences_dialog(display& disp, bool first)
{
	int start_page = gui2::tpreferences::GENERAL_PAGE;
	gui2::tpreferences dlg(disp, start_page);
	dlg.show(disp.video());
	int res = dlg.get_retval();
	if (res == gui2::twindow::OK) {
		disp.invalidate_all();
		if (disp.in_theme() && resources::controller && !resources::controller->is_replaying()) {
			play_controller& controller = *resources::controller;
			if (controller.scenario_env_changed(dlg.get_scenario_env())) {
				controller.do_scenario_env(dlg.get_scenario_env(), true);
			}
		}
	}
	return res;
}

void game_instance::fill_anim_tags(std::map<const std::string, int>& tags)
{
	tags.insert(std::make_pair("reinforce", anim2::REINFORCE));
	tags.insert(std::make_pair("individuality", anim2::INDIVIDUALITY));
	tags.insert(std::make_pair("tactic", anim2::TACTIC));
	tags.insert(std::make_pair("formation_attack", anim2::FORMATION_ATTACK));
	tags.insert(std::make_pair("perfect", anim2::PERFECT));
	tags.insert(std::make_pair("stratagem_up", anim2::STRATAGEM_UP));
	tags.insert(std::make_pair("stratagem_down", anim2::STRATAGEM_DOWN));
	tags.insert(std::make_pair("load_scenario", anim2::LOAD_SCENARIO));
	tags.insert(std::make_pair("flags", anim2::FLAGS));
	tags.insert(std::make_pair("text", anim2::TEXT));
	tags.insert(std::make_pair("place", anim2::PLACE));
	tags.insert(std::make_pair("upgrade", anim2::UPGRADE));
	tags.insert(std::make_pair("card", anim2::CARD));
	tags.insert(std::make_pair("pass_scenario", anim2::PASS_SCENARIO));

	tags.insert(std::make_pair("blade", anim2::BLADE));
	tags.insert(std::make_pair("pierce", anim2::PIERCE));
	tags.insert(std::make_pair("impact", anim2::IMPACT));
	tags.insert(std::make_pair("archery", anim2::ARCHERY));
	tags.insert(std::make_pair("collapse", anim2::COLLAPSE));
	tags.insert(std::make_pair("arcane", anim2::ARCANE));
	tags.insert(std::make_pair("fire", anim2::FIRE));
	tags.insert(std::make_pair("cold", anim2::COLD));
	tags.insert(std::make_pair("strike", anim2::STRIKE));
	tags.insert(std::make_pair("magic", anim2::MAGIC));
	tags.insert(std::make_pair("heal", anim2::HEAL));
	tags.insert(std::make_pair("destruct", anim2::DESTRUCT));
	tags.insert(std::make_pair("formation_defend", anim2::FORMATION_DEFEND));
	tags.insert(std::make_pair("income", anim2::INCOME));
}

void game_instance::fill_anim(int at, const std::string& id, bool area, bool tpl, const config& cfg)
{
	if (area) {
		anims_.insert(std::make_pair(at, new animation(cfg)));
	} else {
		if (tpl) {
			utype_anim_tpls_.insert(std::make_pair(id, cfg));
		} else {
			anims_.insert(std::make_pair(at, new unit_animation(cfg)));
		}
	}
}

void game_instance::load_config2(const config& v)
{
	game_config::foot_speed_prefix = utils::split(v["footprint_prefix"]);
	game_config::foot_teleport_enter = v["footprint_teleport_enter"].str();
	game_config::foot_teleport_exit = v["footprint_teleport_exit"].str();

	if (const config::attribute_value *a = v.get("flag_rgb")) {
		game_config::flag_rgb = a->str();
	}

	// red_green_scale = string2rgb(v["red_green_scale"]);
	if (game_config::red_green_scale.empty()) {
		game_config::red_green_scale.push_back(0x00FFFF00);
	}
	// red_green_scale_text = string2rgb(v["red_green_scale_text"]);
	if (game_config::red_green_scale_text.empty()) {
		game_config::red_green_scale_text.push_back(0x00FFFF00);
	}
}

void game_instance::regenerate_heros(hero_map& heros, bool allow_empty)
{
	base_instance::regenerate_heros(heros, false);
	group.set_layout(preferences::layout());
}

bool game_instance::init_config(const bool force)
{
	base_instance::init_config(force);
	ai::configuration::init(game_config());

	return true;
}

bool game_instance::play_screenshot_mode()
{
	if(!screenshot_mode_) {
		return true;
	}

	editor::start(game_config(), video_, heros_, editor::NONE, screenshot_map_, true, screenshot_filename_);
	return false;
}

bool game_instance::is_loading() const
{
	return !game::load_game_exception::game.empty();
}

bool game_instance::load_game()
{
	savegame::loadgame load(disp(), heros(), game_config(), state_);

	try {
		load.load_game(game::load_game_exception::game, game::load_game_exception::show_replay, true, heros_, &heros_start_);
		// heros in tgroup is pointer to heros_, now heros_ content is changed, these pointer shoud be invalid!
		// in order to clue error on immediately, reset group.
		// remide one rlue: don't use tgroup duration play_controller!
		group.reset();

		cache_.clear_defines();

		// paths_manager_.set_paths(game_config());
		load.set_gamestate();

	} catch(load_game_cancelled_exception&) {
		clear_loaded_game();
		return false;
	} catch(config::error& e) {
		if(e.message.empty()) {
			gui2::show_error_message(disp().video(), _("The file you have tried to load is corrupt"));
		}
		else {
			gui2::show_error_message(disp().video(), _("The file you have tried to load is corrupt: '") + e.message + '\'');
		}
		return false;
	} catch(twml_exception& e) {
		e.show(disp());
		return false;
	} catch(io_exception& e) {
		if(e.message.empty()) {
			gui2::show_error_message(disp().video(), _("File I/O Error while reading the game"));
		} else {
			gui2::show_error_message(disp().video(), _("File I/O Error while reading the game: '") + e.message + '\'');
		}
		return false;
	} catch(game::error& e) {
		if(e.message.empty()) {
			gui2::show_error_message(disp().video(), _("The file you have tried to load is corrupt"));
		}
		else {
			gui2::show_error_message(disp().video(), _("The file you have tried to load is corrupt: '") + e.message + '\'');
		}
		return false;
	}
	recorder = replay(state_.replay_data);
	recorder.start_replay();
	recorder.set_skip(false);

	if (state_.snapshot["runtime"].to_bool()) {
		// We have a snapshot. But does the user want to see a replay?
		if(load.show_replay()) {
			statistics::clear_current_scenario();
		} else {
			recorder.set_to_end();
		}		
	} else {
		// No snapshot; this is a start-of-scenario
		if (load.show_replay()) {
			// There won't be any turns to replay, but the
			// user gets to watch the intro sequence again ...
		} else {
			recorder.set_skip(false);
		}
	}

	if(state_.classification().campaign_type == "multiplayer") {
		BOOST_FOREACH (config &side, state_.snapshot.child_range("side"))
		{
			if (side["controller"] == "network")
				side["controller"] = "human";
			if (side["controller"] == "network_ai")
				side["controller"] = "human_ai";
		}
	}

	return true;
}

void game_instance::mark_completed_campaigns(std::vector<config> &campaigns)
{
	BOOST_FOREACH (config &campaign, campaigns) {
		campaign["completed"] = preferences::is_campaign_completed(campaign["id"]);
	}
}

void level_to_gamestate_bh(game_state& state, bool fog, bool shroud, config& scenario, bool modify_placing)
{
	scenario["fog"] = fog;
	scenario["shroud"] = shroud;

	if (modify_placing) {
		scenario["modify_placing"] = true;
		// modify all city's map_location to invalid.
		int sides = scenario.child_count("side");
		for (int n = 0; n < sides; n ++) {
			config& side = scenario.child("side", n);
			// think there is one city only.
			config& city = side.child("artifical");
			if (city) {
				city["x"] = map_location::null_location.x;
				city["y"] = map_location::null_location.y;
			}
		}
	}

	command_pool replay_data;
	scenario["version"] = game_config::version;
	// to mulitplayer, all player should use a random_seed from creator.
	scenario["random_seed"] = state.rng().get_random_seed();
	::level_to_gamestate(scenario, replay_data, state, state.classification().campaign_type);
}

void game_instance::level_to_gamestate(bool fog, bool shroud, const std::string& map_data, bool modify_placing)
{
	std::stringstream strstr;
	bool layout_mode = state_.classification().mode == "layout";
	load_campaign_cfg();
	config scenario = game_config().find_child("scenario", "id", state_.classification().scenario);
	
	if (tent::turns != -1) {
		scenario["turns"] = tent::turns;
		tent::turns = -1;
	}
	if (tent::duel != -1) {
		if (tent::duel == NO_DUEL) {
			scenario["duel"] = "no";
		} else if (tent::duel == ALWAYS_DUEL) {
			scenario["duel"] = "always";
		} else {
			scenario.remove_attribute("duel");
		}
		tent::duel = -1;
	}
	if (tent::ai_count) {
		scenario["ai_count"] = tent::ai_count;
		tent::ai_count = 0;
	}
	if (tent::employ_count) {
		scenario["employ_count"] = tent::employ_count;
		tent::employ_count = 0;
	}
	if (!map_data.empty()) {
		scenario["map_data"] = map_data;
	}
	if (layout_mode) {
		// switch controller: human and ai
		int sides = scenario.child_count("side");
		std::string build_str;
		for (int n = 0; n < sides; n ++) {
			config& side = scenario.child("side", n);
			// think there is one city only.
			if (side["controller"] == "human") {
				side["controller"] = "ai";
				side["gold"] = "0";
				build_str = side["build"].str();
			} else if (side["controller"] == "ai") {
				side["controller"] = "human";
				side["build"] = build_str;
			}
		}
	}

	if (tent::human_leader_number != HEROS_INVALID_NUMBER) {
		int sides = scenario.child_count("side");
		config* human_side = NULL;
		config* human_artifical = NULL;
		for (int n = 0; n < sides; n ++) {
			config& side = scenario.child("side", n);
			if (side["controller"].str() == "human") {
				human_side = &side;
				human_artifical = &side.child("artifical");
				break;
			}
		}

		const config* cfg_ptr = NULL;
		if (tent::human_leader_number == group.leader().number_) {
			cfg_ptr = &group.to_faction_cfg(!layout_mode, true);
		} else {
			const config::const_child_itors& factions = game_config().child_range("faction");
			BOOST_FOREACH (const config &cfg, factions) {
				if (cfg["leader"].to_int() == tent::human_leader_number) {
					cfg_ptr = &cfg;
					break;
				}
			}
		}
		const config& cfg = *cfg_ptr;
		config& s = *human_side;
		s["leader"] = cfg["leader"];
		
		std::set<int> towers;
		std::vector<std::string> vstr;
		
		if (!layout_mode) {
			vstr = utils::split(cfg["tower_heros"]);
			for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
				int number = lexical_cast_default<int>(*it, -1);
				if (number != -1) {
					towers.insert(number);
				}
			}
			if (towers.find(tent::human_leader_number) == towers.end()) {
				// leader must be on stage.
				towers.insert(tent::human_leader_number);
			}
		}

		strstr.str("");
		strstr << cfg["service_heros"].str() << ", " << cfg["wander_heros"].str();
		vstr = utils::split(strstr.str());
		std::set<int> candidate;
		for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
			int number = lexical_cast_default<int>(*it, -1);
			if (number != -1 && towers.find(number) == towers.end()) {
				candidate.insert(number);
			}
		}
		if (!layout_mode) {
			while ((int)towers.size() < game_config::tower_fix_heros && !candidate.empty()) {
				std::set<int>::iterator it = candidate.begin();
				std::advance(it, rand() % candidate.size());
				int select_number = *it;
				towers.insert(select_number);
				candidate.erase(select_number);
			}
		} else {
			towers = candidate;
			candidate.clear();
		}
		
		config& sub_s = *human_artifical;
		sub_s["heros_army"] = cfg.child("artifical")["heros_army"];
		strstr.str("");
		for (std::set<int>::const_iterator it = towers.begin(); it != towers.end(); ++ it) {
			if (it != towers.begin()) {
				strstr << ", ";
			}
			strstr << *it;
		}
		sub_s["service_heros"] = strstr.str();

		strstr.str("");
		for (std::set<int>::const_iterator it = candidate.begin(); it != candidate.end(); ++ it) {
			if (it != candidate.begin()) {
				strstr << ", ";
			}
			strstr << *it;
		}
		sub_s["wander_heros"] = strstr.str();

		tent::human_leader_number = HEROS_INVALID_NUMBER;
	}

	level_to_gamestate_bh(state_, fog, shroud, scenario, modify_placing);
}

// <0(-1): cancel
// 0: next dialog is canceled
// >0(1): start a new campaign
int game_instance::new_campaign(int catalog)
{
	const config::const_child_itors &ci = game_config().child_range("campaign");
	std::vector<config> campaigns(ci.first, ci.second);
	mark_completed_campaigns(campaigns);
	std::sort(campaigns.begin(),campaigns.end(),less_campaigns_rank);

	if (campaigns.begin() == campaigns.end()) {
		gui2::show_error_message(disp().video(), _("No campaigns are available.\n"));
		return -1;
	}

	int campaign_num = -1;
	{
		gui2::tcampaign_selection dlg(campaigns, catalog);

		try {
			dlg.show(disp().video());
		} catch(twml_exception& e) {
			e.show(disp());
			return -1;
		}

		if (dlg.get_retval() != gui2::twindow::OK) {
			return -1;
		}

		campaign_num = dlg.get_choice();
	}

	if (campaign_num == campaigns.size()) {
		// random map
		play_multiplayer(true);
		return -1;
	}

	state_ = game_state();
	state_.classification().campaign_type = "scenario";

	const config &campaign = campaigns[campaign_num];

	state_.classification().campaign = campaign["id"].str();
	state_.classification().abbrev = campaign["abbrev"].str();
	
	// we didn't specify in the command line the scenario to be started
	state_.classification().scenario = campaign["first_scenario"].str();
	state_.classification().end_text = campaign["end_text"].str();
	state_.classification().end_text_duration = campaign["end_text_duration"];

	std::vector<std::string> player_options;

	{
		tent::lock lock;
		gui2::ttent* dlg = NULL;
		const std::string mode = campaign["mode"].str();
		if (mode == "tower") {
			dlg = new gui2::ttower_tent(disp(), heros_, cards_, game_config(), campaign, group.leader());
		} else {
			dlg = new gui2::tplayer_selection(disp(), heros_, cards_, game_config(), campaign, group.leader());
		}

		try {
			dlg->show(disp().video());
		} catch(twml_exception& e) {
			e.show(disp());
			delete dlg;
			return -1;
		}

		if (dlg->get_retval() != gui2::twindow::OK) {
			// relaunch the campaign selection dialog
			delete dlg;
			return 0;
		}
		lock.set_clear(false);
		checked_card_ = dlg->checked_card();
		
		config player = dlg->player();

		// candidate_cards
		std::stringstream str;
		int index = 0;
		std::vector<size_t> candidate_cards;
		for (std::vector<bool>::iterator b = checked_card_.begin(); b != checked_card_.end(); ++ b, index ++) {
			if (*b) {
				candidate_cards.push_back(index);
			}
		}
		if (!candidate_cards.empty()) {
			size_t start, last;
			std::pair<size_t, size_t> p;
			std::vector<size_t>::const_iterator i = candidate_cards.begin();
			p.first = last = start = *i;
			for (++ i; i != candidate_cards.end(); ++ i) {
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
			player["candidate_cards"] = str.str();
		}
		state_.get_variables().add_child("player", player);

		// state_.classification().player_hero = players[dlg.result()];

		// by this time, player fields has been evaludated, and only one instance of game_state, why doesn't evaluated there to state_?
		// ----these actions need call game_state::set_variable, set_variable will call get_variable, 
		// get_variable need variable_info that need resources::state_of_game valid pointer. 
		// there is invalid here, so doesn't do it.
		// state_.set_variable("player", state_.classification().player);		

		state_.classification().mode = dlg->mode_str();
		state_.classification().campaign_define = campaign["define"].str();
		cache_.clear_defines();
		cache_.add_define("EASY");
		
		if (mode == "tower") {
			gui2::ttower_tent* tower_tent = dynamic_cast<gui2::ttower_tent*>(dlg);
			const config& scenario_data = tower_tent->scenario_data();
			level_to_gamestate(dlg->fog(), dlg->shroud(), scenario_data["map_data"].str(), scenario_data["modify_placing"].to_bool());
		} else {
			level_to_gamestate(dlg->fog(), dlg->shroud());
		}

		delete dlg;
	}
	return 1;
}

const config& state_for_scenario_uh(game_state& state, const config& game_config, const std::string& id, bool subcontinent)
{
	tent::reset();
	const config* selected = NULL;
	BOOST_FOREACH (const config& c, game_config.child_range("campaign")) {
		mode_tag::tmode mode = mode_tag::find(c["mode"]);
		if (mode != mode_tag::SCENARIO) {
			continue;
		}
		if (c["subcontinent"].to_bool()) {
			if (!subcontinent) {
				continue;
			}
		} else if (subcontinent) {
			continue;
		}
		if (!id.empty() && c["id"].str() != id) {
			continue;
		}
		selected = &c;
	}
	VALIDATE(selected, "cannot find proper scenario campaign!");
	const config& campaign = *selected;

	state = game_state();
	state.classification().campaign_type = "scenario";
	state.classification().campaign = campaign["id"].str();
	state.classification().abbrev = campaign["abbrev"].str();
	
	// we didn't specify in the command line the scenario to be started
	state.classification().scenario = campaign["first_scenario"].str();

	return campaign;
}

const config& state_for_siege_uh(game_state& state, const config& game_config, bool subcontinent)
{
	tent::reset();
	const config* selected = NULL;
	BOOST_FOREACH (const config& c, game_config.child_range("campaign")) {
		mode_tag::tmode mode = mode_tag::find(c["mode"]);
		if (mode != mode_tag::SIEGE) {
			continue;
		}
		if (c["subcontinent"].to_bool()) {
			if (!subcontinent) {
				continue;
			}
		} else if (subcontinent) {
			continue;
		}
		selected = &c;
	}
	VALIDATE(selected, "cannot find proper siege campaign!");
	const config& campaign = *selected;

	state = game_state();
	state.classification().campaign_type = "scenario";
	state.classification().campaign = campaign["id"].str();
	state.classification().abbrev = campaign["abbrev"].str();
	
	// we didn't specify in the command line the scenario to be started
	state.classification().scenario = campaign["first_scenario"].str();

	return campaign;
}

void state_for_siege_bh(game_state& state, const config& campaign, config& scenario)
{
	config player;
	player["hero"] = group.leader().number_;
	player["leader"] = group.leader().number_;
	player["stratum"] = hero_stratum_leader;
	state.get_variables().add_child("player", player);

	// state_.classification().player_hero = players[dlg.result()];

	// by this time, player fields has been evaludated, and only one instance of game_state, why doesn't evaluated there to state_?
	// ----these actions need call game_state::set_variable, set_variable will call get_variable, 
	// get_variable need variable_info that need resources::state_of_game valid pointer. 
	// there is invalid here, so doesn't do it.
	// state_.set_variable("player", state_.classification().player);		

	// state_.classification().mode = dlg.mode_str();
	state.classification().mode = campaign["mode"].str();
	state.classification().campaign_define = campaign["define"].str();

	level_to_gamestate_bh(state, false, false, scenario, scenario["modify_placing"].to_bool());
}

void game_instance::start_siege_game(bool subcontinent)
{
	if (!subcontinent) {
		const config& campaign = state_for_siege_uh(state_, game_config_, false);
		{
			gui2::tsiege_tent dlg(disp(), heros_, state_, game_config_, NULL);
			try {
				dlg.show(disp().video());
			} catch(twml_exception& e) {
				e.show(disp());
			}
			if (dlg.get_retval() != gui2::twindow::OK) {
				return;
			}
			state_for_siege_bh(state_, campaign, dlg.get_scenario());
		}
	}
	
	checked_card_.clear();
	launch_game(false);
}

void game_instance::start_scenario_game(bool subcontinent)
{
	if (!subcontinent) {
/*
		const config& campaign = state_for_siege_uh(state_, game_config_, false);
		config scenario = load_campagin_scenario(state_.classification().campaign, state_.classification().scenario, state_.classification().campaign_type);
		state_for_siege_bh(state_, campaign, dlg.get_scenario);
*/
	}
	
	checked_card_.clear();
	launch_game(false);
}

void game_instance::start_layout(const std::string& map_data)
{
	state_ = game_state();
	state_.classification().campaign_type = "scenario";

	const config& campaign = game_config_.find_child("campaign", "id", game_config::campaign_id_siege);

	state_.classification().campaign = campaign["id"].str();
	state_.classification().abbrev = campaign["abbrev"].str();
	
	// we didn't specify in the command line the scenario to be started
	state_.classification().scenario = campaign["first_scenario"].str();

	{
		checked_card_.clear();
		
		config player;
		tent::human_leader_number = hero_map::map_size_from_dat;
		player["hero"] = tent::human_leader_number;
		player["leader"] = tent::human_leader_number;
		player["stratum"] = hero_stratum_leader;
		player["map_data"] = map_data;
		state_.get_variables().add_child("player", player);

		// state_.classification().mode = dlg.mode_str();
		state_.classification().mode = mode_tag::rfind(mode_tag::LAYOUT);
		state_.classification().campaign_define = campaign["define"].str();
		cache_.clear_defines();
		cache_.add_define("EASY");
		
		tent::ai_count = 3; // leader
		level_to_gamestate(false, false, combine_map(map_data, map_data, true), false);
	}
	launch_game(false);
}

void game_instance::reload_changed_game_config()
{
	//force a reload of configuration information
	cache_.recheck_filetree_checksum();
	old_defines_map_.clear();
	clear_binary_paths_cache();
	init_config(true);
}

void game_instance::start_wesnothd()
{
	const std::string wesnothd_program =
		preferences::get_mp_server_program_name().empty() ?
		get_program_invocation("kingdomd") : preferences::get_mp_server_program_name();

	std::string config = get_user_config_dir() + "/lan_server.cfg";
	if (!file_exists(config)) {
		// copy file if it isn't created yet
		write_file(config, read_file(get_wml_location("lan_server.cfg"), true));
	}

#ifndef _WIN32
	std::string command = "\"" + wesnothd_program +"\" -c \"" + config + "\" -d -t 2 -T 5";
#else
	// start wesnoth as background job
	std::string command = "cmd /C start \"wesnoth server\" /B \"" + wesnothd_program + "\" -c \"" + config + "\" -t 2 -T 5";
#endif
	LOG_GENERAL << "Starting wesnothd: "<< command << "\n";
	if (std::system(command.c_str()) == 0) {
		// Give server a moment to start up
		SDL_Delay(50);
		return;
	}
	preferences::set_mp_server_program_name("");

	// Couldn't start server so throw error
	WRN_GENERAL << "Failed to run server start script\n";
	throw game::mp_server_error("Starting MP server failed!");
}

void game_instance::play_multiplayer(bool random_map)
{
	if (game_config::is_reserve_player(preferences::login())) {
		utils::string_map symbols;
		std::string message;
		symbols["username"] = tintegrate::generate_format(preferences::login(), "red");
		message = vgettext2("$username is reserved! Please modify to your username. To modify: Enter your username in \"Player profile\" Setting.", symbols);

		gui2::show_message(disp().video(), "", message);
		return;
	}
	const int siege_choice = 0;
	const int subcontinent_choice = 1;
	const int multiplayer_choice = 2;
	const int start_server_choice = 3;
	const int random_map_choice = 0xff;
	int res;

	// other module may modified game_config, and more data.
	// map editor may result twice multiplayer scenario.
	if (!old_defines_map_.empty()) {
		reload_changed_game_config();
	}

	state_ = game_state();
	state_.classification().campaign_type = "multiplayer";
	state_.classification().campaign_define = "MULTIPLAYER";

	//Print Gui only if the user hasn't specified any server
	if (random_map) {
		// local game, use random map
		res = random_map_choice;
	} else {

		int start_server;
		do {
			start_server = 0;

			gui2::tmp_method_selection dlg;

			dlg.show(disp().video());

			if (dlg.get_retval() == gui2::twindow::OK) {
				res = dlg.get_choice();
			} else {
				return;

			}

			if (res == start_server_choice && preferences::mp_server_warning_disabled() < 2) {
				std::string title = _("Do you really want to start the server?");
				std::string message = _("The server will run in a background process until all users have disconnected.");
				if (gui2::show_message(disp().video(), title, message, gui2::tmessage::yes_no_buttons) == gui2::twindow::OK) {
					start_server = 0;
				} else {
					start_server = 1;
				}
			}
		} while (start_server);
		if (res < 0) {
			return;
		}
	}

	try {
		if (res == start_server_choice) {
			try {
				start_wesnothd();
			} catch(game::mp_server_error&) {
				std::string path = preferences::show_wesnothd_server_search(disp());

				if (!path.empty()) {
					preferences::set_mp_server_program_name(path);
					start_wesnothd();
				} else {
					throw game::mp_server_error("No path given for mp server program.");
				}
			}
		}

		{
			cursor::set(cursor::NORMAL);
			// update binary paths
			// paths_manager_.set_paths(game_config());
			clear_binary_paths_cache();
		}

		if (res == random_map_choice) {
			const tcontroller cntr = CNTR_LOCAL;

			heros_start_ = heros_;
			mp::start_local_game(disp(), game_config(), heros_, heros_start_, cards_, cntr);

		} else if (res == siege_choice) {
			start_siege_game(false);

		} else if (res == subcontinent_choice) {
			{
				gui2::tsubcontinent dlg(disp(), heros_, state_, game_config());
				dlg.show(disp().video());
				res = dlg.get_retval();
			}
			if (res == gui2::tsubcontinent::SIEGE) {
				start_siege_game(true);

			} else if (res == gui2::tsubcontinent::SCENARIO) {
				start_scenario_game(true);
			}

		} else {
			std::string host;
			if (res == start_server_choice) {
				host = "localhost";
			}
			heros_start_ = heros_;
			mp::start_client(disp(), game_config(), heros_, heros_start_, cards_, host);

		}

	} catch(game::mp_server_error& e) {
		gui2::show_error_message(disp().video(), _("Error while starting server: ") + e.message);
	} catch(game::load_game_failed& e) {
		gui2::show_error_message(disp().video(), _("The game could not be loaded: ") + e.message);
	} catch(game::game_error& e) {
		gui2::show_error_message(disp().video(), _("Error while playing the game: ") + e.message);
	} catch(network::error& e) {
		if(e.message != "") {
			ERR_NET << "caught network::error: " << e.message << "\n";
			gui2::show_transient_message(disp().video()
					, ""
					, _(e.message.c_str()));
		} else {
			ERR_NET << "caught network::error\n";
		}
	} catch(config::error& e) {
		if(e.message != "") {
			ERR_CONFIG << "caught config::error: " << e.message << "\n";
			gui2::show_transient_message(disp().video(), "", e.message);
		} else {
			ERR_CONFIG << "caught config::error\n";
		}
	} catch(incorrect_map_format_error& e) {
		gui2::show_error_message(disp().video(), std::string(_("The game map could not be loaded: ")) + e.message);
	} catch (game::load_game_exception &) {
		//this will make it so next time through the title screen loop, this game is loaded
	} catch(twml_exception& e) {
		e.show(disp());
	}

	return;
}

void game_instance::inapp_purchase()
{
	bool browse = false;

	http::membership m = http::membership_hero(disp(), heros_, false);
	if (m.uid < 0) {
		return;
	} else if (preferences::login() == preferences::public_account) {
		std::stringstream err;
		utils::string_map symbols;

		symbols["account"] = tintegrate::generate_format(preferences::login(), "yellow");
		err << vgettext2("$account that you are logining is public account, In-App Purchases on it are shared with other player. In order to benefit to youself only, advise login again with private account.", symbols);
		gui2::show_message(disp().video(), "", err.str());
	}

	gui2::tinapp_purchase dlg(disp(), heros(), browse);
	dlg.show(disp().video());
	if (dlg.get_retval() != gui2::twindow::OK) {
		return;
	}
}

void game_instance::show_preferences()
{
	preferences::show_preferences_dialog(disp(), true);
}

void game_instance::set_unit_data(config& gc)
{
	loadscreen::start_stage("load unit types");
	unit_types.set_config(gc);
}

#define BASENAME_DATA		"data.bin"

typedef enum {
	ppmt_data		= 0,
	ppmt_easy,
} ppmap_type_t;

ppmap_type_t decide_preprocmap_type(const preproc_map &ppmap)
{
	ppmap_type_t		type = ppmt_data;

	for (preproc_map::const_iterator i = ppmap.begin(); i != ppmap.end(); ++i) {
		if (!strcmp(i->first.c_str(), "EASY")) {
			type = ppmt_easy;
			break;
		}
	}
	return type;
}

void game_instance::load_campaign_cfg()
{
	game_config::scoped_preproc_define campaign_define(state_.classification().campaign_define, state_.classification().campaign_define.empty() == false);

	try {
		load_game_cfg(false);
	} catch(config::error&) {
		cache_.clear_defines();
		load_game_cfg(false);
		return;
	}
}

void game_instance::load_game_cfg(const bool force)
{
	// make sure that 'debug mode' symbol is set if command line parameter is selected
	// also if we're in multiplayer and actual debug mode is disabled
	if (!game_config_.empty() && !force && old_defines_map_ == cache_.get_preproc_map()) {
		return; // game_config already holds requested config in memory
	}
	old_defines_map_ = cache_.get_preproc_map();
	loadscreen::global_loadscreen_manager loadscreen_manager(disp().video());
	cursor::setter cur(cursor::WAIT);
	// The loadscreen will erase the titlescreen
	// NOTE: even without loadscreen, needed after MP lobby
	try {
		/**
		 * Read all game configs
		 * First we should load data/
		 * Then handle terrains so that they are last loaded from data/
		 * 2nd everything in userdata
		 **/
		loadscreen::start_stage("verify cache");
		data_tree_checksum();
		loadscreen::start_stage("create cache");

		// start transaction so macros are shared
		game_config::config_cache_transaction main_transaction;

		ppmap_type_t ppmt = decide_preprocmap_type(cache_.get_preproc_map());
		config tmpcfg;
		if (ppmt == ppmt_data) {
			wml_config_from_file(game_config::path + "/xwml/" + BASENAME_DATA, game_config_);
			// once only duration one game running.
			cards_.map_from_cfg(game_config_);
			game_config_.clear_children("card");
			game_config_.clear_children("card_anim");

			set_unit_data(game_config_.child("units"));
			game_config_.clear_children("units");
		

			const config::const_child_itors& terrains = game_config_.child_range("terrain_type");
			BOOST_FOREACH (const config &t, terrains) {
				gamemap::terrain_types.add_child("terrain_type", t);
			}
			game_config_.clear_children("terrain_type");

			// save this to game_config_core_
			game_config_core_ = game_config_;

		} else if (ppmt == ppmt_easy) {
			game_config_ = game_config_core_;
			wml_config_from_file(game_config::path + "/xwml/campaigns/" + state_.classification().campaign + ".bin", tmpcfg);
			game_config_.append(tmpcfg);

		}

		main_transaction.lock();

		/* Put the gfx rules aside so that we can prepend the add-on
		   rules to them. */
		// config core_terrain_rules;
		// core_terrain_rules.splice_children(game_config_, "terrain_graphics");

		// load usermade add-ons
		const std::string user_campaign_dir = get_addon_campaigns_dir();
		std::vector< std::string > error_addons;
		// Scan addon directories
		std::vector<std::string> user_dirs;
		// Scan for standalone files
		std::vector<std::string> user_files;

		// The addons that we'll actually load
		std::vector<std::string> addons_to_load;

		get_files_in_dir(user_campaign_dir,&user_files,&user_dirs,ENTIRE_FILE_PATH);
		std::string user_error_log;

		// Append the $user_campaign_dir/*.cfg files to addons_to_load.
		for(std::vector<std::string>::const_iterator uc = user_files.begin(); uc != user_files.end(); ++uc) {
			const std::string file = *uc;
			if(file.substr(file.size() - 4, file.size()) == ".cfg")
				addons_to_load.push_back(file);
		}

		// Append the $user_campaign_dir/*/_main.cfg files to addons_to_load.
		for(std::vector<std::string>::const_iterator uc = user_dirs.begin(); uc != user_dirs.end(); ++uc){
			const std::string main_cfg = *uc + "/_main.cfg";
			if (file_exists(main_cfg))
				addons_to_load.push_back(main_cfg);
		}

		// Load the addons
		for(std::vector<std::string>::const_iterator uc = addons_to_load.begin(); uc != addons_to_load.end(); ++uc) {
			const std::string toplevel = *uc;
			try {
				config umc_cfg;
				cache_.get_config(toplevel, umc_cfg);

				game_config_.append(umc_cfg);
			} catch(config::error& err) {
				ERR_CONFIG << "error reading usermade add-on '" << *uc << "'\n";
				error_addons.push_back(*uc);
				user_error_log += err.message + "\n";
			} catch(preproc_config::error& err) {
				ERR_CONFIG << "error reading usermade add-on '" << *uc << "'\n";
				error_addons.push_back(*uc);
				user_error_log += err.message + "\n";
			} catch(io_exception&) {
				ERR_CONFIG << "error reading usermade add-on '" << *uc << "'\n";
				error_addons.push_back(*uc);
			}
			if(error_addons.empty() == false) {
				std::stringstream msg;
				msg << _n("The following add-on had errors and could not be loaded:",
						"The following add-ons had errors and could not be loaded:",
						error_addons.size());
				for(std::vector<std::string>::const_iterator i = error_addons.begin(); i != error_addons.end(); ++i) {
					msg << "\n" << *i;
				}

				msg << '\n' << _("ERROR DETAILS:") << '\n' << user_error_log;

				gui2::show_error_message(disp().video(),msg.str());
			}
		}

		// Extract the Lua scripts at toplevel.
		extract_preload_scripts(game_config_);
		game_config_.clear_children("lua");

		// game_config_.merge_children("units");
		// game_config_.splice_children(core_terrain_rules, "terrain_graphics");

		config& hashes = game_config_.add_child("multiplayer_hashes");
		BOOST_FOREACH (const config &ch, game_config_.child_range("multiplayer")) {
			hashes[ch["id"]] = ch.hash();
		}

	} catch(game::error& e) {
		ERR_CONFIG << "Error loading game configuration files\n";
		gui2::show_error_message(disp().video(), _("Error loading game configuration files: '") +
			e.message + _("' (The game will now exit)"));
		throw;
	}
}


void game_instance::launch_game(bool is_load_game)
{
	loadscreen::global_loadscreen_manager loadscreen_manager(disp().video());
	loadscreen::start_stage("load data");

	const binary_paths_manager bin_paths_manager(game_config());

	try {
		if (!is_load_game) {
			heros_start_ = heros_;
		}
		const LEVEL_RESULT result = play_game(disp(), state_,game_config(), heros_, heros_start_, cards_);
		// don't show The End for multiplayer scenario
		// change this if MP campaigns are implemented
		if (result == VICTORY && (state_.classification().campaign_type.empty() || state_.classification().campaign_type != "multiplayer")) {
			preferences::add_completed_campaign(state_.classification().campaign);
			// the_end(disp(), state_.classification().end_text, state_.classification().end_text_duration);
			about::show_about(disp(),state_.classification().campaign);
		}

		clear_loaded_game();
	} catch (game::load_game_exception &) {
		//this will make it so next time through the title screen loop, this game is loaded
	} catch(twml_exception& e) {
		e.show(disp());
	}
}

void game_instance::play_replay()
{
	const binary_paths_manager bin_paths_manager(game_config());

	try {
		::play_replay(disp(),state_,game_config(), heros_, heros_start_, cards_, video_);

		clear_loaded_game();
	} catch (game::load_game_exception &) {
		//this will make it so next time through the title screen loop, this game is loaded
	} catch(twml_exception& e) {
		e.show(disp());
	}
}

editor::EXIT_STATUS game_instance::start_editor(const std::string& map_data)
{
	display_lock loc(disp());
	hotkey::scope_changer changer(game_config(), "hotkey_editor");

	int mode = editor::NONE;
	std::string filename;
	
	if (!map_data.empty()) {
		mode = editor::SIEGE;
		filename = map_data;
	}

	editor::EXIT_STATUS res = editor::start(game_config(), video_, heros_, mode, filename);

	if (res != editor::EXIT_RELOAD_DATA) {
		return res;
	}

	return editor::EXIT_ERROR; // not supposed to happen
}

game_instance::~game_instance()
{
	pathfind::release_pq();
}

// this is needed to allow identical functionality with clean refactoring
// play_game only returns on an error, all returns within play_game can
// be replaced with this
static void safe_exit(int res) 
{
	exit(res);
}

class about_text_formatter2
{
public:
	std::string operator()(const std::string &s) 
	{
		if (s.empty()) return s;
		// Format + as headers, and the rest as normal text.
		char s0 = s[0];
		const std::string ns = tintegrate::stuff_escape(s);

		if (s0 == '+') {
			return tintegrate::generate_format(ns.substr(1), null_str, font::SIZE_NORMAL);
		}
		if (s0 == '-') {
			return tintegrate::generate_format(ns.substr(1), null_str, font::SIZE_SMALL);
		}
		return ns;
	}
};

std::string generate_about_text2()
{
	std::vector<std::string> about_lines = about::get_text();

	std::vector<std::string> res_lines;
	std::transform(about_lines.begin(), about_lines.end(), std::back_inserter(res_lines),
				   about_text_formatter2());
	res_lines.erase(std::remove(res_lines.begin(), res_lines.end(), ""), res_lines.end());
	std::string text = utils::join(res_lines, "\n");
	return text;
}

class tlobby_manager
{
public:
	tlobby_manager()
	{
		lobby = new tlobby(new tlobby::tchat_sock(), new tlobby::thttp_sock(), new tlobby::ttransit_sock());
	}
	~tlobby_manager()
	{
		if (lobby) {
			delete lobby;
		}
	}
};

/**
 * Setups the game environment and enters
 * the titlescreen or game loops.
 */
static int do_gameloop(int argc, char** argv)
{
	srand((unsigned int)time(NULL));

	// setup userdata directory
#if defined(__APPLE__) || defined(ANDROID)
	set_preferences_dir("Documents");
#else
	set_preferences_dir("kingdom");
#endif

	// modify some game_config variable
	game_config::init("kingdom", "War of Kingdom", "#war-of-kingdom", false, true);
	game_config::wesnoth_program_dir = directory_name(argv[0]);

	//ensure recorder has an actually random seed instead of what it got during
	//static initialization (befire any srand() call)
	recorder.set_seed(rand());

	game_instance& game = create_instance<game_instance>(argc, argv);

	// always connect to lobby server.
	tlobby_manager lobby_manager;
	const network::manager net_manager(1, 1);
	lobby->chat->set_host("chat.freenode.net", 6665);

	// server isn't running, disable transit.
	// lobby->transit.set_host("localhost", 15000);

	lobby->join();
	lobby->set_nick2(group.leader().name());

	bool res;

	// do initialize fonts before reading the game config, to have game
	// config error messages displayed. fonts will be re-initialized later
	// when the language is read from the game config.
	res = font::load_font_config();
	if(res == false) {
		std::cerr << "could not initialize fonts\n";
		return 1;
	}

	res = game.init_language();
	if(res == false) {
		std::cerr << "could not initialize the language\n";
		return 1;
	}

	res = game.init_video();
	if(res == false) {
		std::cerr << "could not initialize display\n";
		return 1;
	}

	const cursor::manager cursor_manager;
	cursor::set(cursor::WAIT);

	loadscreen::global_loadscreen_manager loadscreen_manager(game.disp().video());

	loadscreen::start_stage("init gui");
	gui2::init();
	const gui2::event::tmanager gui_event_manager;

	loadscreen::start_stage("load config");
	res = game.init_config(false);
	if (res == false) {
		std::cerr << "could not initialize game config\n";
		return 1;
	}

	loadscreen::start_stage("init fonts");

	res = font::load_font_config();
	if (res == false) {
		std::cerr << "could not re-initialize fonts for the current language\n";
		return 1;
	}

#if defined(_X11) && !defined(__APPLE__)
	SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
#endif

	loadscreen::start_stage("titlescreen");

	game_config::checksum = calculate_res_checksum(game.disp(), game.game_config());
	
	try {
		for (;;) {
			// reset the TC, since a game can modify it, and it may be used
			// by images in add-ons or campaigns dialogs
			image::set_team_colors();

			statistics::fresh_stats();

			if (!game.is_loading()) {
				const config &cfg = game.game_config().child("titlescreen_music");
				if (cfg) {
					sound::play_music_repeatedly(game_config::title_music);
					BOOST_FOREACH (const config &i, cfg.child_range("music")) {
						sound::play_music_config(i);
					}
					sound::commit_music_changes();
				} else {
					sound::empty_playlist();
					sound::stop_music();
				}
			}

			if (game.play_screenshot_mode() == false) {
				return 0;
			}

			recorder.clear();

			loadscreen_manager.reset();

			gui2::ttitle_screen::tresult res = game.is_loading()
					? gui2::ttitle_screen::LOAD_GAME
					: gui2::ttitle_screen::NOTHING;

			const font::floating_label_context label_manager(game.disp().video().getSurface());

			cursor::set(cursor::NORMAL);

			if (res == gui2::ttitle_screen::NOTHING) {
				// load/reload hero_map from file
				if (game.session_xmited()) {
					game.regenerate_heros(game.heros(), false);
				} else {
					http::membership m = http::session(game.disp(), game.heros());
					if (m.uid >= 0) {
						group.from_local_membership(game.disp(), game.heros(), m, true);

						int used_point = group.leader().calculate_used_point();
						int resi_point = group.calculate_total_point() - used_point;
						if (resi_point < 0) {
							hero& h = group.leader();
							h.field_set_minmum();
							preferences::set_hero(game.heros(), h);

							std::string message = _("Detectd capability of player hero is more than allocatable point, system has reset it. To modify, set capability in \"Player profile\".");
							gui2::show_message(game.disp().video(), "", message);
						}
					}
					game_config::local_only = m.uid < 0;
					game.set_session_xmited(true);
				}
				
				gui2::ttitle_screen dlg(game.disp(), game.heros(), group.leader());
				dlg.show(game.disp().video());
				res = static_cast<gui2::ttitle_screen::tresult>(dlg.get_retval());
			}

			if (res == gui2::ttitle_screen::QUIT_GAME) {
				posix_print("do_gameloop, received QUIT_GAME, will exit!\n");
				return 0;

			} else if(res == gui2::ttitle_screen::LOAD_GAME) {
				if (game.load_game() == false) {
					game.clear_loaded_game();
					res = gui2::ttitle_screen::NOTHING;
					continue;
				}
				if (recorder.at_end()){
					game.launch_game(true);
				} else {
					game.play_replay();
				}

			} else if (res == gui2::ttitle_screen::HELP) {
				gui2::thelp_screen dlg(game.disp(), game.heros(), group.leader());
				dlg.show(game.disp().video());
				gui2::thelp_screen::tresult retval = static_cast<gui2::thelp_screen::tresult>(dlg.get_retval());
				if (retval == gui2::thelp_screen::TUTORIAL) {
					int retval2;
					do {
						retval2 = game.new_campaign(res == gui2::ttitle_screen::NEW_CAMPAIGN? NONE_CATALOG: TUTORIAL_CATALOG);
					} while (retval2 == 0);
					if (retval2 < 0) {
						continue;
					}
					game.launch_game(false);

				} else if (retval == gui2::thelp_screen::GUIDE) {
					gui2::tbook2 dlg(game.disp(), NULL, game.heros(), game.game_config(), "help");
					dlg.show(game.disp().video());

				} else if (retval == gui2::thelp_screen::HISTORY) {
					gui2::tbook2 dlg(game.disp(), NULL, game.heros(), game.game_config(), "history");
					dlg.show(game.disp().video());
				}

			} else if (res == gui2::ttitle_screen::NEW_CAMPAIGN) {
				int retval;
				do {
					retval = game.new_campaign(res == gui2::ttitle_screen::NEW_CAMPAIGN? NONE_CATALOG: TUTORIAL_CATALOG);
				} while (retval == 0);
				if (retval < 0) {
					continue;
				}
				game.launch_game(false);

			} else if (res == gui2::ttitle_screen::PLAYER) {
				gui2::tcreate_hero dlg(game.disp(), game.heros());
				dlg.show(game.disp().video());

			} else if (res == gui2::ttitle_screen::PLAYER_SIDE) {
				gui2::tgroup2 dlg(game.disp(), game.heros(), game.game_config(), group);
				dlg.show(game.disp().video());
				int retval = dlg.get_retval();
				if (retval != gui2::twindow::OK) {
					std::string map_data = dlg.get_selected_map_data();
					if (retval == gui2::tgroup2::LAYOUT) {
						game.start_layout(map_data);
					} else if (retval == gui2::tgroup2::START_MAP_EDITOR) {
						game.start_editor(map_data);
					}
				}

			} else if (res == gui2::ttitle_screen::REPORT) {
				gui2::tuser_report dlg(game.disp(), game.heros(), game.game_config());
				dlg.show(game.disp().video());

			} else if (res == gui2::ttitle_screen::MULTIPLAYER) {
/*
				{
					int ii = 0;
					gui2::tauto_scroll dlg(generate_about_text2());
					dlg.show(game.disp().video());
				}
*/
				game.play_multiplayer(false);

			} else if (res == gui2::ttitle_screen::CHANGE_LANGUAGE) {
				if (game.change_language()) {
					t_string::reset_translations();
					image::flush_cache();
				}

			} else if (res == gui2::ttitle_screen::MESSAGE) {
				gui2::tuser_message dlg(game.disp(), game.heros(), game.game_config());
				dlg.show(game.disp().video());	
			
			} else if (res == gui2::ttitle_screen::SIGNIN) {
				gui2::tsignin dlg(game.disp(), game.heros());
				dlg.show(game.disp().video());

			} else if (res == gui2::ttitle_screen::INAPP_PURCHASE) {
				game.inapp_purchase();

			} else if (res == gui2::ttitle_screen::EDIT_PREFERENCES) {
				game.show_preferences();

			} else if (res == gui2::ttitle_screen::START_MAP_EDITOR) {
				// @todo editor can ask the game to quit completely
				if (game.start_editor() == editor::EXIT_QUIT_TO_DESKTOP) {
					return 0;
				}
				continue;
			}
		}

	} catch (twml_exception& e) {
		e.show(game.disp());
	}

	return 0;
}

#include "setjmp.h"
jmp_buf env1;

// I think stack is regular, if you want, you can do.
// But kingdom spend huge memory, it is better clear to start.
// If want normal may wait reduce memory.
void handle_app_event(Uint32 type)
{
	if (type == SDL_APP_TERMINATING) {
		longjmp(env1, 1);

	} else if (type == SDL_APP_WILLENTERBACKGROUND) {
		if (gui2::tinapp_purchase::get_singleton()) {
			return;

		} else if (resources::screen && resources::screen->in_theme()) {
			// throw end_level_exception(QUIT);
		}
		longjmp(env1, 2);

	} else if (type == SDL_APP_DIDENTERBACKGROUND) {

	} else if (type == SDL_APP_WILLENTERFOREGROUND) {
		
	} else if (type == SDL_APP_DIDENTERFOREGROUND) {
		
	}
}

#if defined(__APPLE__) && !TARGET_OS_IPHONE
int SDL_main(int argc, char** argv)
#else
int main(int argc, char** argv)
#endif
{
#if defined(__APPLE__) && TARGET_OS_IPHONE
	SDL_SetHint(SDL_HINT_IDLE_TIMER_DISABLED, "1");
#endif

#ifdef _WIN32
	_CrtSetDbgFlag (_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	// result to memroy leak on purpose.
	char * leak_p = (char*)malloc(13);
#endif

	if (setjmp(env1)) {
		posix_print("game.cpp, main, setjmp: non-zero\n");
#if defined(__APPLE__) && TARGET_OS_IPHONE
		SDL_SetHint(SDL_HINT_IDLE_TIMER_DISABLED, "0");
#endif
		if (lobby) {
			delete lobby;
		}
		preferences::write_preferences();
		return 1;
	}

	if (SDL_Init(SDL_INIT_TIMER) < 0) {
		fprintf(stderr, "Couldn't initialize SDL: %s\n", SDL_GetError());
		return(1);
	}

	try {
		std::cerr << "Battle for Wesnoth v" << game_config::version << '\n';
		const time_t t = time(NULL);
		std::cerr << "Started on " << ctime(&t) << "\n";

		std::string exe_dir;
#ifdef ANDROID
		exe_dir = argv[0];
#else
		exe_dir = get_exe_dir();
#endif

		if(!exe_dir.empty() && file_exists(exe_dir + "/data/_main.cfg")) {
			std::cerr << "Automatically found a possible data directory at "
			          << exe_dir << '\n';
			game_config::path = exe_dir;
		}

		const int res = do_gameloop(argc,argv);
		safe_exit(res);
	} catch(CVideo::error&) {
		std::cerr << "Could not initialize video. Exiting.\n";
		return 1;
	} catch(font::manager::error&) {
		std::cerr << "Could not initialize fonts. Exiting.\n";
		return 1;
	} catch(config::error& e) {
		std::cerr << e.message << "\n";
		return 1;
	} catch(CVideo::quit&) {
		//just means the game should quit
		posix_print("SDL_main, catched CVideo::quit\n");
	} catch(end_level_exception&) {
		std::cerr << "caught end_level_exception (quitting)\n";
	} catch(twml_exception& e) {
		std::cerr << "WML exception:\nUser message: "
			<< e.user_message << "\nDev message: " << e.dev_message << '\n';
		return 1;
	} catch(game_logic::formula_error& e) {
		posix_print_mb("%s", e.what());
		std::cerr << e.what()
			<< "\n\nGame will be aborted.\n";
		return 1;
	} catch(game::error &) {
		// A message has already been displayed.
		return 1;
	} catch(std::bad_alloc&) {
		std::cerr << "Ran out of memory. Aborted.\n";
		return ENOMEM;
	}

	return 0;
} // end main