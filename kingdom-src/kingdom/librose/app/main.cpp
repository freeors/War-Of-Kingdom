#include "map.hpp"
#include "display.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/chat.hpp"
#include "gui/dialogs/rose.hpp"
#include "gui/dialogs/language_selection.hpp"
#include "gui/dialogs/combo_box.hpp"
#include "gui/widgets/window.hpp"
#include "help.hpp"
#include "posix.h"
#include "preferences.hpp"
#include "preferences_display.hpp"
#include "filesystem.hpp"
#include "map_exception.hpp"
#include "game_end_exceptions.hpp"
#include "wml_exception.hpp"
#include "formula.hpp"
#include "gettext.hpp"
#include "hero.hpp"
#include "sound.hpp"
#include "builder.hpp"
#include "language.hpp"
#include "hotkeys.hpp"
#include "cursor.hpp"
#include "loadscreen.hpp"
#include "serialization/binary_or_text.hpp"
#include "serialization/parser.hpp"
#include "serialization/preprocessor.hpp"
#include "serialization/string_utils.hpp"
#include "config_cache.hpp"
#include "formula_string_utils.hpp"
#include "anim_display.hpp"
#include "version.hpp"
#include "mkwin_controller.hpp"

#include <errno.h>
#include <iostream>
#include <clocale>

#include <boost/foreach.hpp>

extern std::string app_id;

/**
 * I would prefer to setup locale first so that early error
 * messages can get localized, but we need the game_controller
 * initialized to have get_intl_dir() to work.  Note: setlocale()
 * does not take GUI language setting into account.
 */
static void init_locale() 
{
#ifdef _WIN32
    std::setlocale(LC_ALL, "English");
#else
	std::setlocale(LC_ALL, "C");
	std::setlocale(LC_MESSAGES, "");
#endif
	const std::string& intl_dir = get_intl_dir();
	bindtextdomain(PACKAGE, intl_dir.c_str());
	bind_textdomain_codeset(PACKAGE, "UTF-8");
	bindtextdomain(PACKAGE "-lib", intl_dir.c_str());
	bind_textdomain_codeset (PACKAGE "-lib", "UTF-8");
	bindtextdomain(PACKAGE "-hero", intl_dir.c_str());
	bind_textdomain_codeset(PACKAGE "-hero", "UTF-8");
	textdomain(PACKAGE);
}

void regenerate_heros(hero_map& heros)
{
	const std::string hero_data_path = game_config::path + "/xwml/" + "hero.dat";
	heros.map_from_file(hero_data_path);
	if (!heros.map_from_file(hero_data_path)) {
		// allow no hero.dat
		heros.realloc_hero_map(HEROS_MAX_HEROS);
	}
	hero_map::map_size_from_dat = heros.size();
	hero player_hero(hero_map::map_size_from_dat);
	if (!preferences::get_hero(player_hero, heros.size())) {
		// write [hero] to preferences
		preferences::set_hero(heros, player_hero);
	}

	group.reset();
	heros.add(player_hero);
	hero& leader = heros[heros.size() - 1];
	group.set_leader(leader);
	leader.set_uid(preferences::uid());
	group.set_noble(preferences::noble());
	group.set_coin(preferences::coin());
	group.set_score(preferences::score());
	group.interior_from_str(preferences::interior());
	group.signin_from_str(preferences::signin());
	group.reload_heros_from_string(heros, preferences::member(), preferences::exile());
	group.associate_from_str(preferences::associate());
	// group.set_layout(preferences::layout()); ?????
	group.set_map(preferences::map());

	group.set_city(heros[hero::number_local_player_city]);
	group.city().set_name(preferences::city());

	other_group.clear();
}

class game_controller
{
public:
	game_controller(int argc, char** argv);
	~game_controller();

	bool init_language();
	bool init_video();
	bool init_config(const bool force = false);

	display& disp();
	const config& game_config() const { return game_config_; }
	bool is_loading() { return false; }
	bool change_language();
	void start_mkwin(bool theme);

private:
	void load_game_cfg(const bool force);

private:
	surface icon_;
	CVideo video_;
	hero_map heros_;

	int force_bpp_;
	const font::manager font_manager_;
	const preferences::base_manager prefs_manager_;
	const image::manager image_manager_;
	const events::event_context main_event_context_;
	sound::music_thinker music_thinker_;
	binary_paths_manager paths_manager_;

	util::scoped_ptr<display> disp_;

	config game_config_;
	config game_config_core_;
	preproc_map old_defines_map_;
	game_config::config_cache& cache_;
};

game_controller::game_controller(int argc, char** argv)
	: icon_()
	, video_()
	, force_bpp_(-1)
	, font_manager_()
	, prefs_manager_()
	, image_manager_()
	, main_event_context_()
	, music_thinker_()
	, paths_manager_()
	, heros_(game_config::path)
	, disp_(NULL)
	, game_config_()
	, game_config_core_()
	, old_defines_map_()
	, cache_(game_config::config_cache::instance())
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
	}

	for (int arg_ = 1; arg_ != argc; ++ arg_) {
		const std::string val(argv[arg_]);
		if (val.empty()) {
			continue;
		}

		if (val == "--config-dir") {
			if (argc <= ++arg_)
				break;
		} else if(val == "--nosound") {
			no_sound = true;
		} else if(val == "--nomusic") {
			no_music = true;
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

	std::replace(game_config::path.begin(), game_config::path.end(), '\\', '/');

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
	game_config::reserve_players.insert("kingdom");
	game_config::reserve_players.insert("Player");
	game_config::reserve_players.insert(_("Player"));

	//
	// initialize player group
	//
	upgrade::fill_require();
	regenerate_heros(heros_);
}

game_controller::~game_controller()
{
	if (icon_) {
		icon_.get()->refcount --;
	}
	if (game_config::savegame_cache) {
		free(game_config::savegame_cache);
		game_config::savegame_cache = NULL;
	}
	terrain_builder::release_heap();
	sound::close_sound();
}

bool game_controller::init_language()
{
	if (!::load_language_list()) {
		return false;
	}

	if (!::set_language(get_locale())) {
		return false;
	}

	// hotkey::load_descriptions();

	return true;
}

display& game_controller::disp()
{
	if (disp_.get() == NULL) {
		if (get_video_surface() == NULL) {
			throw CVideo::error();
		}
		disp_.assign(display::create_dummy_display(video_));
	}
	return *disp_.get();
}

surface icon2;
bool game_controller::init_video()
{
	std::pair<int,int> resolution;
	int bpp = 0;
	int video_flags = 0;

#if (defined(__APPLE__) && TARGET_OS_IPHONE) || defined(ANDROID)
    SDL_Rect rc = video_.bound();
	resolution = std::make_pair(rc.w, rc.h);
	bpp = 32;
	// on iphone/ipad, don't set SDL_WINDOW_FULLSCREEN, it will result cannot find orientation.
	// video_flags = SDL_WINDOW_FULLSCREEN;

#else

	bool found_matching = preferences::detect_video_settings(video_, resolution, bpp, video_flags);
    

	if (force_bpp_ > 0) {
		bpp = force_bpp_;
	}

	if (!found_matching) {
		std::cerr << "Video mode " << resolution.first << 'x'
			<< resolution.second << 'x' << bpp
			<< " is not supported.\n";

		if ((video_flags & SDL_WINDOW_FULLSCREEN)) {
			std::cerr << "Try running the program with the --windowed option "
				<< "using a " << bpp << "bpp setting for your display adapter.\n";
		} else {
			std::cerr << "Try running the program with the --fullscreen option.\n";
		}

		return false;
	}
#endif

    std::cerr << "setting mode to " << resolution.first << "x" << resolution.second << "x" << bpp << "\n";
	const int res = video_.setMode(resolution.first, resolution.second, bpp, video_flags);
	std::cerr << "using mode " << video_.getx() << "x" << video_.gety() << "x" << bpp << "\n";
	video_.setBpp(bpp);

	if (res == 0) {
		std::cerr << "required video mode, " << resolution.first << "x"
		          << resolution.second << "x" << bpp << " is not supported\n";
		return false;
	}

	std::string wm_title_string = _("Rose");
	wm_title_string += " - " + game_config::version;
	SDL_SetWindowTitle(video_.getWindow(), wm_title_string.c_str());

#ifdef _WIN32
	icon2 = image::get_image("game-icon.png", image::UNSCALED);
	if (icon2) {
		SDL_SetWindowIcon(video_.getWindow(), icon2);
	}
#endif

	return true;
}

bool game_controller::init_config(const bool force)
{
	cache_.clear_defines();

	load_game_cfg(force);

	const config &cfg = game_config().child("game_config");
	game_config::load_config(cfg ? &cfg : NULL);

	paths_manager_.set_paths(game_config());
	::init_textdomains(game_config());
	// about::set_about(game_config());
	// ai::configuration::init(game_config());

	return true;
}

#define BASENAME_DATA		"data.bin"

void game_controller::load_game_cfg(const bool force)
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

		config tmpcfg;
		
		wml_config_from_file(game_config::path + "/xwml/" + BASENAME_DATA, game_config_);
		// once only duration one game running.
		// game_config_.clear_children("card");
		// game_config_.clear_children("card_anim");

		// set_unit_data(game_config_.child("units"));
		// game_config_.clear_children("units");

		area_anim::fill_anims(game_config_.child("units"));
	

		const config::const_child_itors& terrains = game_config_.child_range("terrain_type");
		BOOST_FOREACH (const config &t, terrains) {
			gamemap::terrain_types.add_child("terrain_type", t);
		}
		game_config_.clear_children("terrain_type");

		// save this to game_config_core_
		game_config_core_ = game_config_;

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
				// ERR_CONFIG << "error reading usermade add-on '" << *uc << "'\n";
				error_addons.push_back(*uc);
				user_error_log += err.message + "\n";
			} catch(preproc_config::error& err) {
				// ERR_CONFIG << "error reading usermade add-on '" << *uc << "'\n";
				error_addons.push_back(*uc);
				user_error_log += err.message + "\n";
			} catch(io_exception&) {
				// ERR_CONFIG << "error reading usermade add-on '" << *uc << "'\n";
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
		// extract_preload_scripts(game_config_);
		game_config_.clear_children("lua");

		// game_config_.merge_children("units");
		// game_config_.splice_children(core_terrain_rules, "terrain_graphics");

		config& hashes = game_config_.add_child("multiplayer_hashes");
		BOOST_FOREACH (const config &ch, game_config_.child_range("multiplayer")) {
			hashes[ch["id"]] = ch.hash();
		}

	} catch(game::error& e) {
		// ERR_CONFIG << "Error loading game configuration files\n";
		gui2::show_error_message(disp().video(), _("Error loading game configuration files: '") +
			e.message + _("' (The game will now exit)"));
		throw;
	}
}

bool game_controller::change_language()
{
	gui2::tlanguage_selection dlg;
	dlg.show(disp().video());
	if (dlg.get_retval() != gui2::twindow::OK) return false;

	std::string wm_title_string = _("The War of Kingdom");
	wm_title_string += " - " + game_config::version;
	SDL_SetWindowTitle(disp().video().getWindow(), wm_title_string.c_str());
	return true;
}

void game_controller::start_mkwin(bool theme)
{
	display_lock lock(disp());
	hotkey::scope_changer changer(game_config(), "hotkey_mkwin");

	mkwin_controller mkwin(game_config(), video_, theme);
	mkwin.main_loop();
}

namespace gui2 {
int app_show_preferences_dialog(display& disp, bool first)
{
	std::vector<gui2::tval_str> items;

	int fullwindowed = preferences::fullscreen()? preferences::MAKE_WINDOWED: preferences::MAKE_FULLSCREEN;
	items.push_back(gui2::tval_str(fullwindowed, dgettext("wesnoth-lib", "Full Screen")));
	items.push_back(gui2::tval_str(preferences::CHANGE_RESOLUTION, dgettext("wesnoth-lib", "Change Resolution")));
	items.push_back(gui2::tval_str(gui2::twindow::OK, dgettext("wesnoth-lib", "OK")));

	gui2::tcombo_box dlg(items, preferences::CHANGE_RESOLUTION);
	dlg.show(disp.video());

	return dlg.selected_val();
}
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

	const std::string program = argv[0];
	game_config::wesnoth_program_dir = directory_name(program);

	game_config::version = game_config::rose_version;
	game_config::wesnoth_version = version_info(game_config::version);
	game_config::logo_png = "misc/rose-logo.png";

	// always connect to lobby server.
	const network::manager net_manager(1, 1);
	lobby.set_host("www.leagor.com", 15000);
	// lobby.set_host("192.168.1.100", 15000);
	lobby.join();

	game_controller game(argc, argv);
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
	if (res == false) {
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


	loadscreen::start_stage("titlescreen");
/*
	game_config::checksum = calculate_res_checksum(game.disp(), game.game_config());
*/	
	try {
		for (;;) {
			// reset the TC, since a game can modify it, and it may be used
			// by images in add-ons or campaigns dialogs
			image::set_team_colors();

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

			loadscreen_manager.reset();

			gui2::trose::tresult res = gui2::trose::NOTHING;

			const font::floating_label_context label_manager(game.disp().video().getSurface());

			cursor::set(cursor::NORMAL);

			if (res == gui2::trose::NOTHING) {
				// load/reload hero_map from file
				
				gui2::trose dlg(game.disp(), group.leader());
				dlg.show(game.disp().video());
				res = static_cast<gui2::trose::tresult>(dlg.get_retval());
			}

			if (res == gui2::trose::QUIT_GAME) {
				posix_print("do_gameloop, received QUIT_GAME, will exit!\n");
				return 0;

			} else if (res == gui2::trose::EDIT_THEME) {
				game.start_mkwin(true);

			} else if (res == gui2::trose::HELP) {

			} else if (res == gui2::trose::EDIT_DIALOG) {
				game.start_mkwin(false);

			} else if (res == gui2::trose::PLAYER) {

			} else if (res == gui2::trose::PLAYER_SIDE) {

			} else if (res == gui2::trose::REPORT) {

			} else if (res == gui2::trose::MULTIPLAYER) {
				anim_display::start_title_screen_anim();

			} else if (res == gui2::trose::CHANGE_LANGUAGE) {
				if (game.change_language()) {
					t_string::reset_translations();
					image::flush_cache();
				}

			} else if (res == gui2::trose::MESSAGE) {
				gui2::tchat2 dlg(game.disp(), group);
				display& disp = *display::get_singleton();
				dlg.show(disp.video());	
			
			} else if (res == gui2::trose::SIGNIN) {

			} else if (res == gui2::trose::DESIGN) {

			} else if (res == gui2::trose::INAPP_PURCHASE) {

			} else if (res == gui2::trose::EDIT_PREFERENCES) {
				preferences::show_preferences_dialog(game.disp());

			} else if (res == gui2::trose::START_MAP_EDITOR) {
				
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
/*
		if (gui2::tinapp_purchase::get_singleton()) {
			return;

		} else if (resources::screen && resources::screen->in_theme()) {
			// throw end_level_exception(QUIT);
		}
*/
		longjmp(env1, 2);

	} else if (type == SDL_APP_DIDENTERBACKGROUND) {

	} else if (type == SDL_APP_WILLENTERFOREGROUND) {
		
	} else if (type == SDL_APP_DIDENTERFOREGROUND) {
		
	}
}

int main(int argc, char** argv)
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
		preferences::write_preferences();
		return 1;
	}

	app_id = "studio";
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
		exit(res);
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