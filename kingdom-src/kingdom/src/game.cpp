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
#include "SDL.h"
#include "SDL_mixer.h"

#ifndef DISABLE_EDITOR
#include "SDL_stdinc.h"
#endif

#include "about.hpp"
#include "ai/configuration.hpp"
#include "config.hpp"
#include "config_cache.hpp"
#include "construct_dialog.hpp"
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
#include "gui/dialogs/campaign_selection.hpp"
#include "gui/dialogs/player_selection.hpp"
#include "gui/dialogs/tower_tent.hpp"
#include "gui/dialogs/language_selection.hpp"
#include "gui/dialogs/inapp_purchase.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/mp_method_selection.hpp"
#include "gui/dialogs/title_screen.hpp"
#include "gui/dialogs/mp_login.hpp"
#include "gui/dialogs/preferences.hpp"
#include "gui/dialogs/create_hero.hpp"
#include "gui/dialogs/transient_message.hpp"
#ifdef DEBUG_WINDOW_LAYOUT_GRAPHS
#include "gui/widgets/debug.hpp"
#endif
#include "gui/auxiliary/event/handler.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "help.hpp"
#include "hotkeys.hpp"
#include "intro.hpp"
#include "language.hpp"
#include "loadscreen.hpp"
#include "log.hpp"
#include "map_exception.hpp"
#include "widgets/menu.hpp"
#include "marked-up_text.hpp"
#include "multiplayer.hpp"
#include "network.hpp"
#include "playcampaign.hpp"
#include "preferences_display.hpp"
#include "replay.hpp"
#include "savegame.hpp"
#include "scripting/lua.hpp"
#include "sound.hpp"
#include "statistics.hpp"
#include "thread.hpp"
#include "wml_exception.hpp"
#include "wml_separators.hpp"
#include "serialization/binary_or_text.hpp"
#include "serialization/parser.hpp"
#include "serialization/preprocessor.hpp"
#include "serialization/string_utils.hpp"
#include "sha1.hpp"
#include "version.hpp"
#include "resources.hpp"
#include "card.hpp"
#include "formula_string_utils.hpp"

#ifndef DISABLE_EDITOR
#include "editor/editor_main.hpp"
#endif

#include "wesconfig.h"

#include <cerrno>
#include <clocale>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>


#include <boost/foreach.hpp>
// #include <boost/iostreams/copy.hpp>
// #include <boost/iostreams/filtering_streambuf.hpp>
// #include <boost/iostreams/filter/gzip.hpp>

#ifdef ANDROID
#include <android/log.h>
#endif

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

extern bool exit_app;

namespace tent {

std::vector<tlobby_side_param> lobby_side_params;
int io_type;

// only used for calculate_5fields and when teams is empty.
hero* leader;

int human_leader_number = HEROS_INVALID_NUMBER;
int human_count = 0;
int ai_count = 0;
int employ_count = 0;
int mode;
int turns = -1;
int duel = -1;

void reset()
{
	lobby_side_params.clear();

	human_leader_number = HEROS_INVALID_NUMBER;
	human_count = 0;
	ai_count = 0;
	employ_count = 0;
	mode;
	turns = -1;
	duel = -1;
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

class game_controller
{
public:
	game_controller(int argc, char** argv);
	~game_controller();

	game_display& disp();

	bool init_video();
	bool init_config(const bool force=false);
	bool init_language();
	bool play_screenshot_mode();
	std::string calculate_res_checksum() const;

	void reload_changed_game_config();

	bool is_loading() const;
	void clear_loaded_game() { game::load_game_exception::game.clear(); }
	bool load_game();
	void set_tutorial();

	void level_to_gamestate(bool fog, bool shroud, const std::string& map_data = null_str, bool modify_placing = false);
	int new_campaign(int catalog);
	bool goto_multiplayer();
	bool play_multiplayer(bool random_map = false);
	bool change_language();
	bool inapp_purchase();
	
	void show_preferences();

	enum RELOAD_GAME_DATA { RELOAD_DATA, NO_RELOAD_DATA };
	void launch_game(RELOAD_GAME_DATA reload=RELOAD_DATA);
	void play_replay();
#ifndef DISABLE_EDITOR
	editor::EXIT_STATUS start_editor(const std::string& filename = "");
#endif
	void start_wesnothd();
	const config& game_config() const { return game_config_; }

	hero_map& heros() { return heros_; }

private:
	game_controller(const game_controller&);
	void operator=(const game_controller&);

	void load_game_cfg(const bool force=false);
	void load_campaign_cfg();
	void set_unit_data(config& gc);

	void mark_completed_campaigns(std::vector<config>& campaigns);

	const int argc_;
	int arg_;
	const char* const * const argv_;

	//this should get destroyed *after* the video, since we want
	//to clean up threads after the display disappears.
	const threading::manager thread_manager;

	surface icon_;
	CVideo video_;

	const font::manager font_manager_;
	const preferences::manager prefs_manager_;
	const image::manager image_manager_;
	const events::event_context main_event_context_;
	const hotkey::manager hotkey_manager_;
	sound::music_thinker music_thinker_;
	resize_monitor resize_monitor_;
	binary_paths_manager paths_manager_;

	bool screenshot_mode_;
	std::string screenshot_map_, screenshot_filename_;
	int force_bpp_;

	config game_config_;
	config game_config_core_;
	hero_map heros_;
	hero_map heros_start_;
	preproc_map old_defines_map_;
	card_map cards_;

	std::vector<bool> checked_card_;

	util::scoped_ptr<game_display> disp_;

	game_state state_;

	std::string multiplayer_server_;
	bool jump_to_multiplayer_;
	game_config::config_cache& cache_;
};

game_controller::game_controller(int argc, char** argv) :
	argc_(argc),
	arg_(1),
	argv_(argv),
	thread_manager(),
	icon_(),
	video_(),
	font_manager_(),
	prefs_manager_(),
	image_manager_(),
	main_event_context_(),
	hotkey_manager_(),
	music_thinker_(),
	resize_monitor_(),
	paths_manager_(),
	screenshot_mode_(false),
	screenshot_map_(),
	screenshot_filename_(),
	force_bpp_(-1),
	game_config_(),
	game_config_core_(),
	heros_(game_config::path),
	cards_(game_config::path),
	checked_card_(),
	old_defines_map_(),
	disp_(NULL),
	state_(),
	multiplayer_server_(),
	jump_to_multiplayer_(false),
	cache_(game_config::config_cache::instance())
{
	bool no_music = false;
	bool no_sound = false;

	// if allocate static, iOS may be not align 4! 
	// it necessary to ensure align great equal than 4.
	game_config::savegame_cache = (unsigned char*)malloc(game_config::savegame_cache_size);

	// The path can be hardcoded and it might be a relative path.
	if(!game_config::path.empty() &&
#ifdef _WIN32
		// use c_str to ensure that index 1 points to valid element since c_str() returns null-terminated string
		game_config::path.c_str()[1] != ':'
#else
		game_config::path[0] != '/'
#endif
	)
	{
		game_config::path = get_cwd() + '/' + game_config::path;
		font_manager_.update_font_path();
	}
	if (!game_config::path.empty()) {
		heros_.set_path(game_config::path);
		cards_.set_path(game_config::path);
	}

	for (arg_ = 1; arg_ != argc_; ++arg_) {
		const std::string val(argv_[arg_]);
		if(val.empty()) {
			continue;
		}

		if(val == "--fps") {
			preferences::set_show_fps(true);
		} else if(val == "--nocache") {
			cache_.set_use_cache(false);
		} else if(val == "--max-fps") {
			if(arg_+1 != argc_) {
				++arg_;
				int fps = lexical_cast_default<int>(argv_[arg_], 50);
				fps = std::min<int>(fps, 1000);
				fps = std::max<int>(fps, 1);
				fps = 1000 / fps;
				// increase the delay to avoid going above the maximum
				if(1000 % fps != 0) {
					++fps;
				}
				preferences::set_draw_delay(fps);
			}
		} else if(val == "--validcache") {
			cache_.set_force_valid_cache(true);
		} else if(val == "--resolution" || val == "-r") {
			if(arg_+1 != argc_) {
				++arg_;
				const std::string val(argv_[arg_]);
				const std::vector<std::string> res = utils::split(val, 'x');
				if(res.size() == 2) {
					const int xres = lexical_cast_default<int>(res.front());
					const int yres = lexical_cast_default<int>(res.back());
					if(xres > 0 && yres > 0) {
						const std::pair<int,int> resolution(xres,yres);
						preferences::set_resolution(resolution);
					}
				}
			}
		} else if(val == "--bpp") {
			if(arg_+1 != argc_) {
				++arg_;
				force_bpp_ = lexical_cast_default<int>(argv_[arg_],-1);
			}
		} else if(val == "--load" || val == "-l") {
			if(arg_+1 != argc_) {
				++arg_;
				game::load_game_exception::game = argv_[arg_];
			}
		} else if(val == "--with-replay") {
			game::load_game_exception::show_replay = true;

		}
#ifndef DISABLE_EDITOR
		else if(val == "--screenshot") {
			if(arg_+2 != argc_) {
				++arg_;
				screenshot_map_ = argv_[arg_];
				++arg_;
				screenshot_filename_ = argv_[arg_];
				no_sound = true;
				screenshot_mode_ = true;
				preferences::disable_preferences_save();
				force_bpp_ = 32;
			}
		}
#endif
		else if(val == "--config-dir") {
			if (argc_ <= ++arg_)
				break;
		} else if(val == "--windowed" || val == "-w") {
			preferences::set_fullscreen(false);
		} else if(val == "--fullscreen" || val == "-f") {
			preferences::set_fullscreen(true);

		} else if(val == "--server" || val == "-s"){
			jump_to_multiplayer_ = true;
			//Do we have any server specified ?
			if(argc_ > arg_+1){
				multiplayer_server_ = argv_[arg_+1];
				++arg_;
			//Pick the first server in config
			}else{
				if(game_config::server_list.size() > 0)
					multiplayer_server_ = preferences::network_host();
				else
					multiplayer_server_ = "";
			}

		} else if(val == "--no-delay") {
			game_config::no_delay = true;
		} else if (val == "--rng-seed") {
			++arg_;
		} else if(val == "--nosound") {
			no_sound = true;
		} else if(val == "--nomusic") {
			no_music = true;
		} else if(val == "--new-storyscreens") {
			// This is a hidden option to help testing
			// the work-in-progress new storyscreen code.
			// Don't document.
			set_new_storyscreen(true);
        }  //These commented lines should be used to implement support of connection
             //through a proxy via command line options.
             //The ANA network module should implement these methods (while the SDL_net won't.)
            else if(val == "--proxy") {
            network::enable_connection_through_proxy();
        } else if(val == "--proxy-address") {
            if ( argv_[ arg_ + 1][0] != '-')
            {
                network::enable_connection_through_proxy();
                network::set_proxy_address( argv_[ ++arg_ ] );
            }
            else
                throw std::runtime_error("Proxy address option requires address");

        } else if(val == "--proxy-port") {
            if ( argv_[ arg_ + 1][0] != '-')
            {
                network::enable_connection_through_proxy();
                network::set_proxy_port( argv_[ ++arg_ ] );
            }
            else
                throw std::runtime_error("Proxy port option requires port");
        } else if(val == "--proxy-user") {
            if ( argv_[ arg_ + 1][0] != '-')
            {
                network::enable_connection_through_proxy();
                network::set_proxy_user( argv_[ ++arg_ ] );
            }
            else
                throw std::runtime_error("Proxy user option requires user name");
        } else if(val == "--proxy-password") {
            if ( argv_[ arg_ + 1][0] != '-')
            {
                network::enable_connection_through_proxy();
                network::set_proxy_password( argv_[ ++arg_ ] );
            }
            else
                throw std::runtime_error("Proxy password option requires password");
        } else if(val == "--new-widgets") {
			// This is a hidden option to enable the new widget toolkit.
			gui2::new_widgets = true;
		} else if(val[0] == '-') {
			std::cerr << "unknown option: " << val << std::endl;
			throw config::error("unknown option");
		} else {
			std::cerr << "Overriding data directory with " << val << std::endl;
#ifdef _WIN32
			// use c_str to ensure that index 1 points to valid element since c_str() returns null-terminated string
			if(val.c_str()[1] == ':') {
#else
			if(val[0] == '/') {
#endif
				game_config::path = val;
			} else {
				game_config::path = get_cwd() + '/' + val;
			}
			heros_.set_path(game_config::path);
			cards_.set_path(game_config::path);

			if(!is_directory(game_config::path)) {
				std::cerr << "Could not find directory '" << game_config::path << "'\n";
				throw config::error("directory not found");
			}

			font_manager_.update_font_path();
		}
	}

#if defined(_WIN32)
	// game_config::path is ansi format.
	for (std::string::const_iterator it = game_config::path.begin(); it != game_config::path.end(); ++ it) {
		if (*it & 0x80) {
			std::stringstream err;
			std::string path = game_config::path;
			std::replace(path.begin(), path.end(), '/', '\\');

			err << path << " include invalid character! Please use english path.";
			MessageBox(NULL, err.str().c_str(), "Error", MB_OK);
			throw game::error(err.str());
		}
	}
#endif

	std::cerr << '\n';
	std::cerr << "Data directory: " << game_config::path
		<< "\nUser configuration directory: " << get_user_config_dir()
		<< "\nUser data directory: " << get_user_data_dir()
		<< "\nCache directory: " << get_cache_dir()
		<< '\n';

#if defined(_WIN32) && defined(_DEBUG)
	// sound::init_sound make no memory leak output.
	// By this time, I doesn't find what result it, to easy, don't call sound::init_sound. 
	no_sound = true;
#endif

	// disable sound in nosound mode, or when sound engine failed to initialize
	if (no_sound || ((preferences::sound_on() || preferences::music_on() ||
	                  preferences::turn_bell() || preferences::UI_sound_on()) &&
	                 !sound::init_sound())) {
		preferences::set_sound(false);
		preferences::set_music(false);
		preferences::set_turn_bell(false);
		preferences::set_UI_sound(false);
	} else if (no_music) { // else disable the music in nomusic mode
		preferences::set_music(false);
	}

	game_config::reserve_players.insert("");
	game_config::reserve_players.insert("Player");
	game_config::reserve_players.insert(_("Player"));

	heros_.map_from_file(game_config::path + "/xwml/" + "hero.dat");
	hero& player_hero = group.leader();
	if (!preferences::get_hero(player_hero, heros_.size())) {
		// write [hero] to preferences
		preferences::set_hero(player_hero);
	}
	hero_map::map_size_from_dat = heros_.size();
	player_hero.number_ = (int)heros_.size();
}

game_display& game_controller::disp()
{
	if(disp_.get() == NULL) {
		if(get_video_surface() == NULL) {
			throw CVideo::error();
		}
		disp_.assign(game_display::create_dummy_display(video_));
	}
	return *disp_.get();
}

bool game_controller::init_video()
{
#ifdef _WIN32
	icon_ = image::get_image("game-icon.png", image::UNSCALED);
	if (icon_ != NULL) {
		// must be called after SDL_Init() and before setting video mode
		::SDL_WM_SetIcon(icon_, NULL);
	}
#endif

	std::pair<int,int> resolution;
	int bpp = 0;
	int video_flags = 0;

	bool found_matching = preferences::detect_video_settings(video_, resolution, bpp, video_flags);

	if (force_bpp_ > 0) {
		bpp = force_bpp_;
	}

	if(!found_matching) {
		std::cerr << "Video mode " << resolution.first << 'x'
			<< resolution.second << 'x' << bpp
			<< " is not supported.\n";

		if ((video_flags & FULL_SCREEN)) {
			std::cerr << "Try running the program with the --windowed option "
				<< "using a " << bpp << "bpp setting for your display adapter.\n";
		} else {
			std::cerr << "Try running the program with the --fullscreen option.\n";
		}

		return false;
	}

	std::cerr << "setting mode to " << resolution.first << "x" << resolution.second << "x" << bpp << "\n";
	const int res = video_.setMode(resolution.first, resolution.second, bpp, video_flags);
	std::cerr << "using mode " << video_.getx() << "x" << video_.gety() << "x" << bpp << "\n";
	video_.setBpp(bpp);

	if (res == 0) {
		std::cerr << "required video mode, " << resolution.first << "x"
		          << resolution.second << "x" << bpp << " is not supported\n";
		return false;
	}
	return true;
}

std::string game_controller::calculate_res_checksum() const
{
	uint32_t bin_nfiles, bin_sum_size, bin_modified;
	std::stringstream strstr;

	std::string str;

	// requrie check file: data.bin, gui.bin, hero.dat.
	if (!wml_checksum_from_file(game_config::path + "/xwml/data.bin", &bin_nfiles, &bin_sum_size, &bin_modified)) {
		bin_nfiles = bin_sum_size = bin_modified = 0;
	}
	strstr << bin_nfiles << "," << bin_sum_size << "," << bin_modified;
	str = strstr.str();

	if (!wml_checksum_from_file(game_config::path + "/xwml/gui.bin", &bin_nfiles, &bin_sum_size, &bin_modified)) {
		bin_nfiles = bin_sum_size = bin_modified = 0;
	}
	strstr << "," << bin_nfiles << "," << bin_sum_size << "," << bin_modified;
	str = strstr.str();

	int checksum = calcuate_xor_from_file(game_config::path + "/xwml/hero.dat");
	strstr << "," << checksum;
	str = strstr.str();

	sha1_hash sha(strstr.str());
	str = sha.display();
	return str;
}

bool game_controller::init_config(const bool force)
{
	cache_.clear_defines();

	load_game_cfg(force);

	const config &cfg = game_config().child("game_config");
	game_config::load_config(cfg ? &cfg : NULL);

	hotkey::deactivate_all_scopes();
	hotkey::set_scope_active(hotkey::SCOPE_GENERAL);
	hotkey::set_scope_active(hotkey::SCOPE_GAME);

	hotkey::load_hotkeys(game_config());
	paths_manager_.set_paths(game_config());
	::init_textdomains(game_config());
	about::set_about(game_config());
	ai::configuration::init(game_config());

	return true;
}

bool game_controller::init_language()
{
	if (!::load_language_list()) {
		return false;
	}

	if (!::set_language(get_locale())) {
		return false;
	}

	std::string wm_title_string = _("The War of Kingdom");
	wm_title_string += " - " + game_config::revision;
	SDL_WM_SetCaption(wm_title_string.c_str(), NULL);

	hotkey::load_descriptions();

	return true;
}

bool game_controller::play_screenshot_mode()
{
	if(!screenshot_mode_) {
		return true;
	}

#ifndef DISABLE_EDITOR
	cache_.clear_defines();
	cache_.add_define("EDITOR");
	load_game_cfg();
	const binary_paths_manager bin_paths_manager(game_config());
	::init_textdomains(game_config());

	editor::start(game_config(), video_, screenshot_map_, true, screenshot_filename_);
	return false;
#else
	return false;
#endif
}

bool game_controller::is_loading() const
{
	return !game::load_game_exception::game.empty();
}

bool game_controller::load_game()
{
	savegame::loadgame load(disp(), game_config(), state_);

	try {
		load.load_game(game::load_game_exception::game, game::load_game_exception::show_replay, game::load_game_exception::cancel_orders, heros_, &heros_start_);

		cache_.clear_defines();

		game_config::scoped_preproc_define campaign_define_def(state_.classification().campaign_define, !state_.classification().campaign_define.empty());

		game_config::scoped_preproc_define campaign_type_def("MULTIPLAYER", state_.classification().campaign_define.empty() && (state_.classification().campaign_type == "multiplayer"));

		try {
			// load_game_cfg();
		} catch(config::error&) {
			cache_.clear_defines();
			load_game_cfg();
			return false;
		}

		paths_manager_.set_paths(game_config());
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

	if (load.cancel_orders()) {
		BOOST_FOREACH (config &side, state_.snapshot.child_range("side"))
		{
			if (side["controller"] != "human") continue;
			BOOST_FOREACH (config &unit, side.child_range("unit"))
			{
				unit["goto_x"] = -999;
				unit["goto_y"] = -999;
			}
		}
	}

	return true;
}

void game_controller::set_tutorial()
{
	const std::string campaign_id = "tutorial";
	const config& tutorial_cfg = game_config().find_child("campaign", "id", campaign_id);

	state_ = game_state();
	state_.classification().campaign = campaign_id;
	state_.classification().campaign_type = "scenario";
	state_.classification().scenario = tutorial_cfg["first_scenario"].str();
	state_.classification().campaign_define = tutorial_cfg["define"].str();
	state_.classification().version = game_config::version;
	cache_.clear_defines();
	cache_.add_define("EASY");

	// load/reload hero_map from file
	if (!heros_.map_from_file(game_config::path + "/xwml/" + "hero.dat")) {
		gui2::show_error_message(disp().video(), _("Can not find valid hero.dat in <data>/xwml"));
		return;
	}
	heros_start_ = heros_;

	level_to_gamestate(false, false);
}

void game_controller::mark_completed_campaigns(std::vector<config> &campaigns)
{
	BOOST_FOREACH (config &campaign, campaigns) {
		campaign["completed"] = preferences::is_campaign_completed(campaign["id"]);
	}
}

void game_controller::level_to_gamestate(bool fog, bool shroud, const std::string& map_data, bool modify_placing)
{
	load_campaign_cfg();
	config scenario = game_config().find_child("scenario", "id", state_.classification().scenario);
	scenario["fog"] = fog;
	scenario["shroud"] = shroud;
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
		const config::const_child_itors& factions = game_config().child_range("faction");
		BOOST_FOREACH (const config &cfg, factions) {
			if (cfg["leader"].to_int() == tent::human_leader_number) {
				config& s = *human_side;
				s["leader"] = cfg["leader"];
								
				std::stringstream strstr;
				strstr << cfg["service_heros"].str() << ", " << cfg["wander_heros"].str();
				
				std::set<int> towers;
				std::vector<std::string> vstr = utils::split(cfg["tower_heros"]);
				for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
					int number = lexical_cast_default<int>(*it, -1);
					if (number != -1) {
						towers.insert(number);
					}
				}
				vstr = utils::split(strstr.str());
				std::set<int> candidate;
				for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
					int number = lexical_cast_default<int>(*it, -1);
					if (number != -1 && towers.find(number) == towers.end()) {
						candidate.insert(number);
					}
				}
				while ((int)towers.size() < game_config::tower_fix_heros && !candidate.empty()) {
					std::set<int>::iterator it = candidate.begin();
					std::advance(it, rand() % candidate.size());
					int select_number = *it;
					towers.insert(select_number);
					candidate.erase(select_number);
				}
				
				config& sub_s = *human_artifical;
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
				break;
			}
		}
		tent::human_leader_number = HEROS_INVALID_NUMBER;
	}

	command_pool replay_data;
	scenario["random_seed"] = state_.rng().get_random_seed();
	::level_to_gamestate(scenario, replay_data, state_, state_.classification().campaign_type);
}

// <0(-1): cancel
// 0: next dialog is canceled
// >0(1): start a new campaign
int game_controller::new_campaign(int catalog)
{
	state_ = game_state();
	state_.classification().campaign_type = "scenario";

	const config::const_child_itors &ci = game_config().child_range("campaign");
	std::vector<config> campaigns(ci.first, ci.second);
	mark_completed_campaigns(campaigns);
	std::sort(campaigns.begin(),campaigns.end(),less_campaigns_rank);


	if (campaigns.begin() == campaigns.end()) {
		gui2::show_error_message(disp().video(), _("No campaigns are available.\n"));
		return -1;
	}

	int campaign_num = -1;
	// No campaign selected from command line
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

	const config &campaign = campaigns[campaign_num];

	state_.classification().campaign = campaign["id"].str();
	state_.classification().abbrev = campaign["abbrev"].str();
	
	// we didn't specify in the command line the scenario to be started
	state_.classification().scenario = campaign["first_scenario"].str();
	state_.classification().end_text = campaign["end_text"].str();
	state_.classification().end_text_duration = campaign["end_text_duration"];

	std::vector<std::string> player_options;

	// load/reload hero_map from file
	// if (!heros_.map_from_file(game_config::path + "/xwml/" + "hero.dat")) {
	std::string hero_filename = get_wml_location(campaign["hero_data"]);
	if (!heros_.map_from_file(hero_filename)) {
		gui2::show_error_message(disp().video(), _("Can not find valid hero.dat in [campaign] tag"));
		return 0;
	}
	heros_start_ = heros_;

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
		if (player["hero"].to_int() == group.leader().number_) {
			heros_.add(group.leader());
			heros_start_.add(group.leader());
		}

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
		state_.classification().version = game_config::version;
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

bool game_controller::goto_multiplayer()
{
	if(jump_to_multiplayer_){
		jump_to_multiplayer_ = false;
		if(play_multiplayer()){
			;
		}else{
			return false;
		}
	}
	return true;
}

void game_controller::reload_changed_game_config()
{
	//force a reload of configuration information
	cache_.recheck_filetree_checksum();
	old_defines_map_.clear();
	clear_binary_paths_cache();
	init_config(true);
}

void game_controller::start_wesnothd()
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

bool game_controller::play_multiplayer(bool random_map)
{
	if (game_config::is_reserve_player(preferences::login())) {
		utils::string_map symbols;
		std::string message;
		symbols["username"] = help::tintegrate::generate_format(preferences::login(), "red");
		message = vgettext("$username is reserved! Please modify to your username. To modify: Enter your username in \"Player profile\" Setting.", symbols);

		gui2::show_message(disp().video(), "", message);
		return false;
	}
/*
	{
		int ii = 0;
		for (std::map<int, std::string>::const_iterator it = game_config::inapp_items.begin(); it != game_config::inapp_items.end(); ++ it) {
			preferences::set_inapp_purchased(it->first, false);
		}
		preferences::write_preferences();
		return false;
	}
*/
	int res;

	// other module may modified game_config, and more data.
	// map editor may result twice multiplayer scenario.
	if (!old_defines_map_.empty()) {
		reload_changed_game_config();
	}

	state_ = game_state();
	state_.classification().campaign_type = "multiplayer";
	state_.classification().campaign_define = "MULTIPLAYER";

	// load/reload hero_map from file
	if (!heros_.map_from_file(game_config::path + "/xwml/" + "hero.dat")) {
		gui2::show_error_message(disp().video(), _("Can not find valid hero.dat in <data>/xwml"));
		return false;
	}
	heros_start_ = heros_;

	//Print Gui only if the user hasn't specified any server
	if (random_map) {
		// local game, use random map
		res = 3;
	} else if ( multiplayer_server_.empty() ) {

		int start_server;
		do {
			start_server = 0;

			gui2::tmp_method_selection dlg;

			dlg.show(disp().video());

			if(dlg.get_retval() == gui2::twindow::OK) {
				res = dlg.get_choice();
			} else {
				return false;

			}

			if (res == 2 && preferences::mp_server_warning_disabled() < 2)
			{
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
			return false;
		}

	}else{
		res = 4;
	}

	try {
		if (res == 2)
		{
			try {
				start_wesnothd();
			} catch(game::mp_server_error&)
			{
				std::string path = preferences::show_wesnothd_server_search(disp());

				if (!path.empty())
				{
					preferences::set_mp_server_program_name(path);
					start_wesnothd();
				}
				else
				{
					throw game::mp_server_error("No path given for mp server program.");
				}
			}


		}

		{
/*
			cache_.clear_defines();
			game_config::scoped_preproc_define multiplayer(state_.classification().campaign_define);
			load_game_cfg();
*/
			events::discard(INPUT_MASK_MIN, INPUT_MASK_MAX); // prevent the "keylogger" effect
			cursor::set(cursor::NORMAL);
			// update binary paths
			paths_manager_.set_paths(game_config());
			clear_binary_paths_cache();
		}

		if(res == 3) {
			std::vector<std::string> chat;
			config game_data;

			const mp::controller cntr = mp::CNTR_LOCAL;

			mp::start_local_game(disp(), game_config(), heros_, heros_start_, cards_, cntr);

		} else if((res >= 0 && res <= 2) || res == 4) {
			std::string host;
			if(res == 0) {
				host = preferences::server_list().front().address;
			}else if(res == 2) {
				host = "localhost";
			}else if(res == 4){
				host = multiplayer_server_;
				multiplayer_server_ = "";
			}
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
					, gettext(e.message.c_str()));
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

	return false;
}

bool game_controller::change_language()
{
	gui2::tlanguage_selection dlg;
	dlg.show(disp().video());
	if (dlg.get_retval() != gui2::twindow::OK) return false;

	std::string wm_title_string = _("The War of Kingdom");
	wm_title_string += " - " + game_config::revision;
	SDL_WM_SetCaption(wm_title_string.c_str(), NULL);
	return true;
}

bool game_controller::inapp_purchase()
{
	bool browse = true;
#if defined(__APPLE__) && TARGET_OS_IPHONE
	browse = false;
#endif

	gui2::tinapp_purchase dlg(browse);
	dlg.show(disp().video());
	if (dlg.get_retval() != gui2::twindow::OK) {
		return false;
	}
	return true;
}

void game_controller::show_preferences()
{
	const preferences::display_manager disp_manager(&disp());
	gui2::show_preferences_dialog(disp());

	disp().redraw_everything();
}

void game_controller::set_unit_data(config& gc)
{
	loadscreen::start_stage("load unit types");
	unit_types.set_config(gc);
}

#define MAXLEN_BASENAME		16
#define BASENAME_DATA		"data.bin"
// #define BASENAME_DATA		"data2.bin" // used to generate building_rules_
#define BASENAME_EDITOR		"editor.bin"

typedef enum {
	ppmt_data		= 0,
	ppmt_easy,
	ppmt_editor,
} ppmap_type_t;

ppmap_type_t decide_preprocmap_type(const preproc_map &ppmap)
{
	ppmap_type_t		type = ppmt_data;

	for(preproc_map::const_iterator i = ppmap.begin(); i != ppmap.end(); ++i) {
		if (!strcmp(i->first.c_str(), "EASY")) {
			type = ppmt_easy;
			break;
		} else if (!strcmp(i->first.c_str(), "EDITOR")) {
			type = ppmt_editor;
			break;
		}
	}
	return type;
}

void game_controller::load_campaign_cfg()
{
	game_config::scoped_preproc_define campaign_define(state_.classification().campaign_define, state_.classification().campaign_define.empty() == false);

	try {
		load_game_cfg();
	} catch(config::error&) {
		cache_.clear_defines();
		load_game_cfg();
		return;
	}
}

void game_controller::load_game_cfg(const bool force)
{
	// make sure that 'debug mode' symbol is set if command line parameter is selected
	// also if we're in multiplayer and actual debug mode is disabled
	if (game_config::debug) {
		cache_.add_define("DEBUG_MODE");
	}

	if (!game_config_.empty() && !force
			&& old_defines_map_ == cache_.get_preproc_map())
		return; // game_config already holds requested config in memory
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

		} else if (ppmt == ppmt_editor) {
			game_config_ = game_config_core_;
			wml_config_from_file(game_config::path + "/xwml/" + BASENAME_EDITOR, tmpcfg);
			game_config_.append(tmpcfg);

		}
		// ::init_textdomains(game_config_);
		// cache_.get_config(game_config::path +"/data", game_config_);

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

		// terrain_builder::set_terrain_rules_cfg(game_config());

	} catch(game::error& e) {
		ERR_CONFIG << "Error loading game configuration files\n";
		gui2::show_error_message(disp().video(), _("Error loading game configuration files: '") +
			e.message + _("' (The game will now exit)"));
		throw;
	}
}


void game_controller::launch_game(RELOAD_GAME_DATA reload)
{
	loadscreen::global_loadscreen_manager loadscreen_manager(disp().video());
	loadscreen::start_stage("load data");

	const binary_paths_manager bin_paths_manager(game_config());

	try {
		const LEVEL_RESULT result = play_game(disp(),state_,game_config(), heros_, heros_start_, cards_);
		// don't show The End for multiplayer scenario
		// change this if MP campaigns are implemented
		if(result == VICTORY && (state_.classification().campaign_type.empty() || state_.classification().campaign_type != "multiplayer")) {
			preferences::add_completed_campaign(state_.classification().campaign);
			the_end(disp(), state_.classification().end_text, state_.classification().end_text_duration);
			about::show_about(disp(),state_.classification().campaign);
		}

		clear_loaded_game();
	} catch (game::load_game_exception &) {
		//this will make it so next time through the title screen loop, this game is loaded
	} catch(twml_exception& e) {
		e.show(disp());
	}
}

void game_controller::play_replay()
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

#ifndef DISABLE_EDITOR
editor::EXIT_STATUS game_controller::start_editor(const std::string& filename)
{
	while(true){
		cache_.clear_defines();
		cache_.add_define("EDITOR");
		load_game_cfg();
		const binary_paths_manager bin_paths_manager(game_config());
		::init_textdomains(game_config());

		editor::EXIT_STATUS res = editor::start(game_config(), video_, filename);

		if(res != editor::EXIT_RELOAD_DATA)
			return res;

		reload_changed_game_config();
		image::flush_cache();
	}
	return editor::EXIT_ERROR; // not supposed to happen
}
#endif

game_controller::~game_controller()
{
	if (icon_ != NULL) {
		icon_.get()->refcount --;
	}
	if (game_config::savegame_cache) {
		free(game_config::savegame_cache);
		game_config::savegame_cache = NULL;
	}
	terrain_builder::release_heap();
	pathfind::release_pq();
	delete gui::empty_menu;
	sound::close_sound();
}

// this is needed to allow identical functionality with clean refactoring
// play_game only returns on an error, all returns within play_game can
// be replaced with this
static void safe_exit(int res) {

	LOG_GENERAL << "exiting with code " << res << "\n";
#ifdef OS2 /* required to correctly shutdown SDL on OS/2 */
        SDL_Quit();
#endif
	exit(res);
}

struct preprocess_options
{
public:
	preprocess_options(): output_macros_path_("false"), input_macros_()
	{
	}
	std::string output_macros_path_;
	preproc_map input_macros_;
};

/** Process commandline-arguments */
static int process_command_args(int argc, char** argv) {
	const std::string program = argv[0];
	game_config::wesnoth_program_dir = directory_name(program);
	preprocess_options preproc;

	//parse arguments that shouldn't require a display device
	int arg;
	for(arg = 1; arg != argc; ++arg) {
		const std::string val(argv[arg]);
		if(val.empty()) {
			continue;
		}

		if(val == "--help" || val == "-h") {
			// When adding items don't forget to update doc/man/wesnoth.6
			// Options are sorted alphabetically by --long-option.
			// Please keep the output to 80 chars per line.
			std::cout << "usage: " << argv[0]
			<< " [<options>] [<data-directory>]\n"
			<< "Available options:\n"
			<< "  --bpp <number>               sets BitsPerPixel value. Example: --bpp 32\n"
			<< "  -c, --campaign[[<difficulty>] <id_c> [<id_s>]]\n"
			<< "                               goes directly to the campaign.\n"
			<< "                               - difficulty : the difficulty of the specified\n"
			<< "                                          campaign (1 to max - Default is 1)\n"
			<< "                               - id_c: the id of the campaign. A selection \n"
			<< "                                       menu will appear if none specified\n"
			<< "                               - id_s: the id of the scenario from the\n"
			<< "                                       specified campaign\n"
			<< "  --config-dir <name>          sets the path of the user config directory to\n"
			<< "                               $HOME/<name> or My Documents\\My Games\\<name> for windows.\n"
			<< "                               You can specify also an absolute path outside the\n"
			<< "                               $HOME or My Documents\\My Games directory.\n"
			<< "  --config-path                prints the path of the user config directory and\n"
			<< "                               exits.\n"
			<< "  --data-dir <directory>       overrides the data directory with the one specified.\n"
			<< "  -d, --debug                  enables additional command mode options in-game.\n"
#ifdef DEBUG_WINDOW_LAYOUT_GRAPHS
			<< "  --debug-dot-level=<level1>,<level2>,...\n"
			<< "                               sets the level of the debug dot files.\n"
			<< "                               These files are used for debugging the widgets\n"
			<< "                               especially the for the layout engine. When enabled\n"
			<< "                               the engine will produce dot files which can be\n"
			<< "                               converted to images with the dot tool.\n"
			<< "                               Available levels:\n"
			<< "                               - size  : generate the size info of the widget.\n"
			<< "                               - state : generate the state info of the widget.\n"
			<< "  --debug-dot-domain=<domain1>,<domain2>,...\n"
			<< "                               sets the domain of the debug dot files.\n"
			<< "                               see --debug-dot-level for more info.\n"
			<< "                               Available domains:\n"
			<< "                               show   : generate the data when the dialog is\n"
			<< "                                        about to be shown.\n"
			<< "                               layout : generate the data during the layout\n"
			<< "                                        phase (might result in multiple files. \n"
			<< "                               The data can also be generated when the F12 is\n"
			<< "                               pressed in a dialog.\n"
#endif
#ifndef DISABLE_EDITOR
			<< "  -e, --editor [<file>]        starts the in-game map editor directly. If <file>\n"
			<< "                               is specified, equivalent to -e --load <file>.\n"
#endif
			<< "  --fps                        displays the number of frames per second the\n"
			<< "                               game is currently running at, in a corner of\n"
			<< "                               the screen.\n"
			<< "  -f, --fullscreen             runs the game in full screen mode.\n"
			<< "  --gunzip <infile>.gz         decompresses a file (<infile>.gz) in gzip format\n"
			<< "                               and stores it without the .gz suffix.\n"
			<< "                               <infile>.gz will be removed.\n"
			<< "  --gzip <infile>              compresses a file (<infile>) in gzip format,\n"
			<< "                               stores it as <infile>.gz and removes <infile>.\n"
			<< "  -h, --help                   prints this message and exits.\n"
			<< "  -l, --load <file>            loads the save <file> from the standard save\n"
			<< "                               game directory.\n"
#ifndef DISABLE_EDITOR
			<< "                               When launching the map editor via -e, the map\n"
			<< "                               <file> is loaded, relative to the current\n"
			<< "                               directory. If it is a directory, the editor\n"
			<< "                               will start with a load map dialog opened there.\n"
#endif
			<< "  --log-<level>=<domain1>,<domain2>,...\n"
			<< "                               sets the severity level of the log domains.\n"
			<< "                               'all' can be used to match any log domain.\n"
			<< "                               Available levels: error, warning, info, debug.\n"
			<< "                               By default the 'error' level is used.\n"
			<< "  --logdomains [filter]        lists defined log domains (only the ones containing\n"
			<< "                               [filter] if used) and exits.\n"
			<< "  --max-fps                    the maximum fps the game tries to run at. Values\n"
			<< "                               should be between 1 and 1000, the default is 50.\n"
			<< "  -m, --multiplayer            starts a multiplayer game. There are additional\n"
			<< "                               options that can be used as explained below:\n"
			<< "    --ai_config<number>=value  selects a configuration file to load for this side.\n"
			<< "    --algorithm<number>=value  selects a non-standard algorithm to be used by\n"
			<< "                               the AI controller for this side.\n"
			<< "    --controller<number>=value selects the controller for this side.\n"
			<< "    --era=value                selects the era to be played in by its id.\n"
			<< "    --exit-at-end              exit Wesnoth at the end of the scenario.\n"
			<< "    --nogui                    runs the game without the GUI. Must appear before\n"
			<< "                               --multiplayer to have the desired effect.\n"
			<< "    --parm<number>=name:value  sets additional parameters for this side.\n"
			<< "    --scenario=value           selects a multiplayer scenario. The default\n"
			<< "                               scenario is \"multiplayer_The_Freelands\".\n"
			<< "    --side<number>=value       selects a faction of the current era for this\n"
			<< "                               side by id.\n"
			<< "    --turns=value              sets the number of turns. The default is \"50\".\n"
			<< "  --no-delay                   runs the game without any delays.\n"
			<< "  --nocache                    disables caching of game data.\n"
			<< "  --nomusic                    runs the game without music.\n"
			<< "  --nosound                    runs the game without sounds and music.\n"
			<< "  --path                       prints the path to the data directory and exits.\n"
			<< "  --preprocess, -p[=<define1>,<define2>,...] <file/folder> <target directory>\n"
			<< "                               preprocesses a specified file/folder. The preprocessed\n"
			<< "                               file(s) will be written in the specified target\n"
			<< "                               directory: a plain cfg file and a processed cfg file.\n"
			<< "                               define1,define2,...  - the extra defines will\n"
			<< "                               be added before processing the files. If you add\n"
			<< "                               them you must add the '=' character before.\n"
			<< "                               If 'SKIP_CORE' is in the define list the\n"
			<< "                               data/core won't be preprocessed.\n"
			<< " --preprocess-input-macros <source file>\n"
			<< "                               used only by the '--preprocess' command.\n"
			<< "                               Specifies a file that contains [preproc_define]s\n"
			<< "                               to be included before preprocessing.\n"
			<< " --preprocess-output-macros [<target file>]\n"
			<< "                               used only by the '--preprocess' command.\n"
			<< "                               Will output all preprocessed macros in the target file.\n"
			<< "                               If the file is not specified the output will be\n"
			<< "                               file '_MACROS_.cfg' in the target directory of\n"
			<< "                               preprocess's command. This switch should be typed\n"
			<< "                               before the --preprocess command.\n"
			<< "  -r, --resolution XxY         sets the screen resolution. Example: -r 800x600\n"
			<< "  --rng-seed <number>          seeds the random number generator with number\n"
			<< "                               Example: --rng-seed 0\n"
			<< "  --smallgui                   allows to use screen resolutions down to 800x480\n"
			<< "                               and resizes a few interface elements.\n"
			<< "  --screenshot <map> <output>  Saves a screenshot of <map> to <output> without\n"
			<< "                               initializing a screen. Editor must be compiled\n"
			<< "                               in for this to work.\n"
			<< "  -s, --server [<host>]        connects to the host if specified\n"
			<< "                               or to the first host in your preferences.\n"
			<< "  -t, --test                   runs the game in a small test scenario.\n"
			<< "  --validcache                 assumes that the cache is valid. (dangerous)\n"
			<< "  -v, --version                prints the game's version number and exits.\n"
			<< "  -w, --windowed               runs the game in windowed mode.\n"
			<< "  --with-replay                replays the file loaded with the --load option.\n"
			<< "  --new-widgets                there is a new WIP widget toolkit this switch\n"
			<< "                               enables the new toolkit (VERY EXPERIMENTAL don't\n"
			<< "                               file bug reports since most are known).\n"
			<< "                               Parts of the library are deemed stable and will\n"
			<< "                               work without this switch.\n"
			;
			return 0;
		} else if(val == "--version" || val == "-v") {
			std::cout << "Battle for Wesnoth" << " " << game_config::version
			          << "\n";
			return 0;
		} else if (val == "--config-path") {
			std::cout << get_user_data_dir() << '\n';
			return 0;
		} else if(val == "--path") {
			std::cout <<  game_config::path
			          << "\n";
			return 0;
		}
#ifndef DISABLE_EDITOR
		else if (val == "--screenshot" ) {
			if(!(argc > arg + 2)) {
				std::cerr << "format of " << val << " command: " << val << " <map file> <output file>\n";
				return 2;
			}
			static char opt[] = "SDL_VIDEODRIVER=dummy";
			SDL_putenv(opt);
		}
#endif
		else if(val == "--config-dir") {
			if (argc <= ++arg)
				break;
			set_preferences_dir(argv[arg]);
		} else if(val == "--data-dir") {
			if(arg +1 != argc) {
				++arg;
				const std::string datadir(argv[arg]);
				std::cerr << "Overriding data directory with " << datadir << std::endl;
#ifdef _WIN32
				// use c_str to ensure that index 1 points to valid element since c_str() returns null-terminated string
				if(datadir.c_str()[1] == ':') {
#else
				if(datadir[0] == '/') {
#endif
					game_config::path = datadir;
				} else {
					game_config::path = get_cwd() + '/' + datadir;
				}

				if(!is_directory(game_config::path)) {
					std::cerr << "Could not find directory '" << game_config::path << "'\n";
					throw config::error("directory not found");
				}

				// don't update font as we already updating it in game ctor
			}
			else
				std::cerr << "please specify a data directory\n";
		} else if (val.substr(0, 6) == "--log-") {
			size_t p = val.find('=');
			if (p == std::string::npos) {
				std::cerr << "unknown option: " << val << '\n';
				return 2;
			}
			std::string s = val.substr(6, p - 6);
			int severity;
			if (s == "error") severity = 0;
			else if (s == "warning") severity = 1;
			else if (s == "info") severity = 2;
			else if (s == "debug") severity = 3;
			else {
				std::cerr << "unknown debug level: " << s << '\n';
				return 2;
			}
			while (p != std::string::npos) {
				size_t q = val.find(',', p + 1);
				s = val.substr(p + 1, q == std::string::npos ? q : q - (p + 1));
				if (!lg::set_log_domain_severity(s, severity)) {
					std::cerr << "unknown debug domain: " << s << '\n';
					return 2;
				}
				p = q;
			}
		} else if(val == "--logdomains") {
			std::string filter;
			if(arg + 1 != argc) {
				++arg;
				filter = argv[arg];
			}
			std::cout << lg::list_logdomains(filter);
			return 0;
		} else if(val == "--rng-seed") {
			if (argc <= ++arg) {
				std::cerr << "format of \" " << val << " " << argv[arg] << " \" is bad\n";
				return 2;
			}
			srand(lexical_cast_default<unsigned int>(argv[arg]));
		} else if (val == "--preprocess-input-macros") {
			if (arg + 1 < argc)
			{
				++arg;
				std::string file = argv[arg];
				if (file_exists(file) == false)
				{
					std::cerr << "please specify an existing file. File "<< file <<" doesn't exist.\n";
					return 1;
				}

				std::cerr << SDL_GetTicks() << " Reading cached defines from: " << file << "\n";

				config cfg;
				std::string error_log;
				scoped_istream stream = istream_file(file);
				read(cfg, *stream);

				int read = 0;
				// use static preproc_define::read_pair(config) to make a object
				BOOST_FOREACH (const config::any_child &value, cfg.all_children_range()) {
					const preproc_map::value_type def = preproc_define::read_pair(value.cfg);
					preproc.input_macros_[def.first] = def.second;
					++read;
				}
				std::cerr << SDL_GetTicks() << " Read " << read << " defines.\n";
			}
			else {
				std::cerr << "please specify input macros file.\n";
				return 2;
			}
		} else if (val == "--preprocess-output-macros") {
			preproc.output_macros_path_ = "true";
			if (arg + 1 < argc && argv[arg+1][0] != '-')
			{
				++arg;
				preproc.output_macros_path_ = argv[arg];
			}
		} else if (val.find("--preprocess") == 0 || val.find("-p") == 0){
			if (arg + 2 < argc){
				++arg;
				const std::string resourceToProcess(argv[arg]);
				++arg;
				const std::string targetDir(argv[arg]);

				Uint32 startTime = SDL_GetTicks();
				// if the users add the SKIP_CORE define we won't preprocess data/core
				bool skipCore = false;
				bool skipTerrainGFX = false;
				// the 'core_defines_map' is the one got from data/core macros
				preproc_map defines_map(preproc.input_macros_);
				std::string error_log;

				// add the specified defines
				size_t pos=std::string::npos;
				if (val.find("--preprocess=") == 0)
					pos = val.find("=");
				else if (val.find("-p=") == 0)
					pos = val.find("=");

				// we have some defines specified
				if (pos != std::string::npos)
				{
					std::string tmp_val = val.substr(pos+1);
					while (pos != std::string::npos)
					{
						size_t tmpPos = val.find(',', pos+1);
						tmp_val = val.substr(pos + 1,
							tmpPos == std::string::npos ? tmpPos : tmpPos - (pos+1));
						pos = tmpPos;

						if (tmp_val.empty()){
							std::cerr << "empty define supplied\n";
							continue;
						}

						LOG_PREPROC<<"adding define: "<< tmp_val<<'\n';
						defines_map.insert(std::make_pair(tmp_val,
							preproc_define(tmp_val)));
						if (tmp_val == "SKIP_CORE")
						{
							std::cerr << "'SKIP_CORE' defined.\n";
							skipCore = true;
						}
						else if (tmp_val == "NO_TERRAIN_GFX")
						{
							std::cerr << "'NO_TERRAIN_GFX' defined.\n";
							skipTerrainGFX = true;
						}
					}
					std::cerr << "added " << defines_map.size() << " defines.\n";
				}

				// preprocess core macros first if we don't skip the core
				if (skipCore == false)
				{
					std::cerr << "preprocessing common macros from 'data/core' ...\n";

					// process each folder explicitly to gain speed
					preprocess_resource(game_config::path + "/data/core/macros",&defines_map);
					if (skipTerrainGFX == false)
						preprocess_resource(game_config::path + "/data/core/terrain-graphics",&defines_map);

					std::cerr << "acquired " << (defines_map.size() - preproc.input_macros_.size())
						<< " 'data/core' defines.\n";
				}
				else
					std::cerr << "skipped 'data/core'\n";

				// preprocess resource
				std::cerr << "preprocessing specified resource: "
						  << resourceToProcess << " ...\n";
				preprocess_resource(resourceToProcess, &defines_map, true,true, targetDir);
				std::cerr << "acquired " << (defines_map.size() - preproc.input_macros_.size())
					      << " total defines.\n";

				if (preproc.output_macros_path_ != "false")
				{
					std::string outputPath = targetDir + "/_MACROS_.cfg";
					if (preproc.output_macros_path_ != "true")
						outputPath = preproc.output_macros_path_;

					std::cerr << "writing '" << outputPath << "' with "
							  << defines_map.size() << " defines.\n";

					scoped_ostream out = ostream_file(outputPath);
					if (!out->fail())
					{
						config_writer writer(*out,false);

						for(preproc_map::iterator itor = defines_map.begin();
							itor != defines_map.end(); ++itor)
						{
							(*itor).second.write(writer, (*itor).first);
						}
					}
					else
						std::cerr << "couldn't open the file.\n";
				}

				std::cerr << "preprocessing finished. Took "<< SDL_GetTicks() - startTime << " ticks.\n";
				return 0;
			}
			else{
				std::cerr << "Please specify a source file/folder and a target folder\n";
				return 2;
			}
		}
	}

	// Not the most intuitive solution, but I wanted to leave current semantics for now
	return -1;
}

/**
 * I would prefer to setup locale first so that early error
 * messages can get localized, but we need the game_controller
 * initialized to have get_intl_dir() to work.  Note: setlocale()
 * does not take GUI language setting into account.
 */
static void init_locale() {
	#ifdef _WIN32
	    std::setlocale(LC_ALL, "English");
	#else
		std::setlocale(LC_ALL, "C");
		std::setlocale(LC_MESSAGES, "");
	#endif
	const std::string& intl_dir = get_intl_dir();
	bindtextdomain (PACKAGE, intl_dir.c_str());
	bind_textdomain_codeset (PACKAGE, "UTF-8");
	bindtextdomain (PACKAGE "-lib", intl_dir.c_str());
	bind_textdomain_codeset (PACKAGE "-lib", "UTF-8");
	bindtextdomain (PACKAGE "-hero", intl_dir.c_str());
	bind_textdomain_codeset (PACKAGE "-hero", "UTF-8");
	textdomain (PACKAGE);
}

/**
 * Setups the game environment and enters
 * the titlescreen or game loops.
 */
static int do_gameloop(int argc, char** argv)
{
	srand((unsigned int)time(NULL));

	// there is parameter bug in install program, make cannot set userdata dir
#if defined(__APPLE__) || defined(ANDROID)
	set_preferences_dir("Documents");
#else
	set_preferences_dir("kingdom");
#endif
	int finished = process_command_args(argc, argv);
	if (finished != -1) {
		return finished;
	}

	//ensure recorder has an actually random seed instead of what it got during
	//static initialization (befire any srand() call)
	recorder.set_seed(rand());

	game_controller game(argc,argv);
	const int start_ticks = SDL_GetTicks();

	init_locale();

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
	res = game.init_config();
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

	game_config::checksum = game.calculate_res_checksum();

	loadscreen::start_stage("titlescreen");

	try {
		for (;;) {
			if (exit_app) {
				posix_print("do_gameloop, exit app is ture, exit!\n");
#ifdef ANDROID
				__android_log_print(ANDROID_LOG_INFO, "SDL", "do_gameloop, exit app is ture, exit!\n");
#endif
				return 0;
			} else {
#ifdef ANDROID
				__android_log_print(ANDROID_LOG_INFO, "SDL", "do_gameloop, exit app is false!\n");
#endif
			}

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

			// Start directly a multiplayer
			// Eventually with a specified server
			if (game.goto_multiplayer() == false){
				continue; // Go to main menu
			}

			loadscreen_manager.reset();

			gui2::ttitle_screen::tresult res = game.is_loading()
					? gui2::ttitle_screen::LOAD_GAME
					: gui2::ttitle_screen::NOTHING;

			const preferences::display_manager disp_manager(&game.disp());

			const font::floating_label_context label_manager;

			cursor::set(cursor::NORMAL);
			if (game_config::is_reserve_player(group.leader().name())) {
				utils::string_map symbols;
				symbols["account"] = help::tintegrate::generate_format(dsgettext("wesnoth-lib", "Register, config account"), "yellow");
				symbols["ps"] = help::tintegrate::generate_format(dsgettext("wesnoth-lib", "PS"), "green");

				gui2::tmp_login dlg(game.disp(), vgettext("wesnoth-lib", "account^detect invalid($account, $ps)", symbols));
				dlg.show(game.disp().video());
			}
			if (res == gui2::ttitle_screen::NOTHING) {
				const hotkey::basic_handler key_handler(&game.disp());
				gui2::ttitle_screen dlg(game.disp(), game.heros(), group.leader());
				dlg.show(game.disp().video());
				res = static_cast<gui2::ttitle_screen::tresult>(dlg.get_retval());
			}

			game_controller::RELOAD_GAME_DATA should_reload = game_controller::RELOAD_DATA;

			if (exit_app || res == gui2::ttitle_screen::QUIT_GAME) {
				posix_print("do_gameloop, after received QUIT_GAME, ticks: %u\n", SDL_GetTicks());
#ifdef ANDROID
				__android_log_print(ANDROID_LOG_INFO, "SDL", "do_gameloop, after received QUIT_GAME, ticks: %u", SDL_GetTicks());
#endif
				return 0;
			} else if(res == gui2::ttitle_screen::LOAD_GAME) {
				if(game.load_game() == false) {
					game.clear_loaded_game();
					res = gui2::ttitle_screen::NOTHING;
					continue;
				}

				should_reload = game_controller::NO_RELOAD_DATA;
			} else if (res == gui2::ttitle_screen::TUTORIAL || res == gui2::ttitle_screen::NEW_CAMPAIGN) {
				int retval;
				do {
					retval = game.new_campaign(res == gui2::ttitle_screen::NEW_CAMPAIGN? NONE_CATALOG: TUTORIAL_CATALOG);
				} while (retval == 0);
				if (retval < 0) {
					continue;
				}

			} else if(res == gui2::ttitle_screen::RANDOM_MAP) {
				game_config::debug = false;
				if (game.play_multiplayer(true) == false) {
					continue;
				}
			} else if(res == gui2::ttitle_screen::MULTIPLAYER) {
				game_config::debug = false;
				if(game.play_multiplayer() == false) {
					continue;
				}
			} else if(res == gui2::ttitle_screen::CHANGE_LANGUAGE) {
				if (game.change_language()) {
					t_string::reset_translations();
					image::flush_cache();
				}
				continue;
			} else if(res == gui2::ttitle_screen::INAPP_PURCHASE) {
				if (game.inapp_purchase()) {
				}
				continue;
			} else if(res == gui2::ttitle_screen::EDIT_PREFERENCES) {
				game.show_preferences();
				continue;
			} else if(res == gui2::ttitle_screen::SHOW_ABOUT) {
				about::show_about(game.disp());
				continue;
			} else if(res == gui2::ttitle_screen::SHOW_HELP) {
				help::help_manager help_manager(&game.game_config(), NULL);
				help::show_help(game.disp());
				continue;
#ifndef DISABLE_EDITOR
			} else if(res == gui2::ttitle_screen::START_MAP_EDITOR) {
				// @todo editor can ask the game to quit completely
				if (game.start_editor() == editor::EXIT_QUIT_TO_DESKTOP) {
					return 0;
				}
				continue;
#endif
			}

			if (recorder.at_end()){
				game.launch_game(should_reload);
			} else {
				game.play_replay();
			}
		}

	} catch(twml_exception& e) {
		e.show(game.disp());
	}

	return 0;
}

#include "setjmp.h"
jmp_buf env1;
jmp_buf env2;

static Uint32 sysevent_callback(const Uint32 sysevent, void* event)
{
	if (sysevent == 1) {
		posix_print("sysevent = 1, will call throw CVideo::quit()\n");
		longjmp(env1, 1);
	} else if (sysevent == 2) {
		if (gui2::tinapp_purchase::get_singleton()) {
			posix_print("sysevent = 2, but in inapp_purchasing, do nothing!\n");
			return 0;
		} else if (!resources::units) {
			posix_print("sysevent = 2, resources::units is NULL\n");
		} else {
			posix_print("sysevent = 2, resources::units is non-null\n");
			// throw end_level_exception(QUIT);
		}
		// longjmp(env2, 2);
		longjmp(env1, 2);
	} else if (sysevent == 3) {
		if (gui2::tinapp_purchase::get_singleton()) {
			posix_print("sysevent = 3, but in inapp_purchasing, do nothing!\n");
			return 0;
		} else {
			posix_print("sysevent = 3, will call throw CVideo::quit()\n");
			longjmp(env1, 3);
		}
	}
	return 0;
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

	SDL_AddTimer(0xaaaaaaaa, sysevent_callback, NULL);
	if (setjmp(env1)) {
		posix_print("game.cpp, main, setjmp: non-zero\n");
#if defined(__APPLE__) && TARGET_OS_IPHONE
		SDL_SetHint(SDL_HINT_IDLE_TIMER_DISABLED, "0");
#endif
		preferences::write_preferences();
		return 1;
	}

	if (SDL_Init(SDL_INIT_TIMER) < 0) {
		fprintf(stderr, "Couldn't initialize SDL: %s\n", SDL_GetError());
		return(1);
	}

	try {
		std::cerr << "Battle for Wesnoth v" << game_config::revision << '\n';
		const time_t t = time(NULL);
		std::cerr << "Started on " << ctime(&t) << "\n";

		std::string exe_dir;
#ifdef ANDROID
		exit_app = false;
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
	} catch(gui::button::error&) {
		std::cerr << "Could not create button: Image could not be found\n";
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
		posix_print_mb(e.what());
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