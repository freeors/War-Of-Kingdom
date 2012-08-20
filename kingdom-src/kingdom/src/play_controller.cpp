/* $Id: play_controller.cpp 47608 2010-11-21 01:56:29Z shadowmaster $ */
/*
   Copyright (C) 2006 - 2010 by Joerg Hinrichs <joerg.hinrichs@alice-dsl.de>
   wesnoth playlevel Copyright (C) 2003 by David White <dave@whitevine.net>
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
 *  Handle input via mouse & keyboard, events, schedule commands.
 */

#include "play_controller.hpp"
#include "dialogs.hpp"
#include "foreach.hpp"
#include "game_events.hpp"
#include "gettext.hpp"
#include "halo.hpp"
#include "loadscreen.hpp"
#include "log.hpp"
#include "resources.hpp"
#include "savegame.hpp"
#include "sound.hpp"
#include "unit_id.hpp"
#include "terrain_filter.hpp"
#include "save_blocker.hpp"
#include "preferences_display.hpp"
#include "replay.hpp"
#include "soundsource.hpp"
#include "tooltips.hpp"
#include "game_preferences.hpp"
#include "wml_exception.hpp"
#include "formula_string_utils.hpp"
#include "ai/manager.hpp"
#include "scripting/lua.hpp"
#include "artifical.hpp"

#include "gui/dialogs/camp_armory.hpp"
#include "gui/dialogs/transient_message.hpp"
#include "gui/dialogs/recruit.hpp"
#include "gui/dialogs/rpg_detail.hpp"
#include "gui/dialogs/select_unit.hpp"
#include "gui/dialogs/final_battle.hpp"
#include "gui/widgets/window.hpp"

static lg::log_domain log_engine("engine");
#define LOG_NG LOG_STREAM(info, log_engine)
#define DBG_NG LOG_STREAM(debug, log_engine)

static lg::log_domain log_display("display");
#define ERR_DP LOG_STREAM(err, log_display)

static void clear_resources()
{
	resources::game_map = NULL;
	resources::units = NULL;
	resources::heros = NULL;
	resources::teams = NULL;
	resources::state_of_game = NULL;
	resources::controller = NULL;
	resources::screen = NULL;
	resources::soundsources = NULL;
	resources::tod_manager = NULL;
	resources::persist = NULL;
}

play_controller::play_controller(const config& level, game_state& state_of_game, hero_map& heros, hero_map& heros_start,
		card_map& cards,
		int ticks, int num_turns, const config& game_config, CVideo& video,
		bool skip_replay, bool replaying) :
	controller_base(ticks, game_config, video),
	observer(),
	// savegame_config(),
	animation_cache(),
	prefs_disp_manager_(),
	tooltips_manager_(),
	events_manager_(),
	halo_manager_(),
	labels_manager_(),
	help_manager_(&game_config, &map_),
	mouse_handler_(NULL, teams_, units_, heros, map_, tod_manager_, undo_stack_),
	menu_handler_(NULL, units_, heros, cards, teams_, level, map_, game_config, tod_manager_, state_of_game, undo_stack_),
	soundsources_manager_(),
	tod_manager_(level, num_turns, &state_of_game),
	persist_(),
	gui_(),
	statistics_context_(level["name"]),
	level_(level),
	teams_(),
	gamestate_(state_of_game),
	map_(game_config, level["map_data"]),
	units_(),
	heros_(heros),
	heros_start_(heros_start),
	cards_(cards),
	undo_stack_(),
	xp_mod_(level["experience_modifier"].to_int(100)),
	loading_game_(level["playing_team"].empty() == false),
	first_human_team_(-1),
	player_number_(1),
	first_player_(level_["playing_team"].to_int() + 1),
	start_turn_(tod_manager_.turn()), // tod_manager_ constructed above
	is_host_(true),
	skip_replay_(skip_replay),
	replaying_(replaying),
	linger_(false),
	previous_turn_(0),
	savenames_(),
	wml_commands_(),
	victory_when_enemies_defeated_(true),
	end_level_data_(),
	victory_music_(),
	defeat_music_(),
	all_ai_allied_(level_["ai_allied"].to_bool()),
	final_capital_(std::make_pair<artifical*, artifical*>(NULL, NULL)),
	human_players_(0),
	fallen_to_unstage_(level_["fallen_to_unstage"].to_bool()),
	card_mode_(level_["card_mode"].to_bool()),
	always_duel_(level_["always_duel"].to_bool())
{
	resources::game_map = &map_;
	resources::units = &units_;
	resources::heros = &heros_;
	resources::teams = &teams_;
	resources::state_of_game = &gamestate_;
	resources::controller = this;
	resources::tod_manager = &tod_manager_;
	resources::persist = &persist_;
	persist_.start_transaction();

	// Setup victory and defeat music
	set_victory_music_list(level_["victory_music"]);
	set_defeat_music_list(level_["defeat_music"]);

	game_config::add_color_info(level);
	hotkey::deactivate_all_scopes();
	hotkey::set_scope_active(hotkey::SCOPE_GENERAL);
	hotkey::set_scope_active(hotkey::SCOPE_GAME);
	try {
		init(video);
		
		if (!level_["final_capital"].empty()) {
			const std::vector<std::string> capitals = utils::split(level_["final_capital"]);
			final_capital_.first = units_.city_from_cityno(lexical_cast_default<int>(capitals[0]));
			final_capital_.second = units_.city_from_cityno(lexical_cast_default<int>(capitals[1]));
		}
		if (!level_["maximal_defeated_activity"].empty()) {
			game_config::maximal_defeated_activity = level_["maximal_defeated_activity"].to_int(100);
		}
	} catch (...) {
		clear_resources();
		throw;
	}
}

std::string vector_2_str(const std::vector<size_t>& vec)
{
	std::stringstream str;
	if (!vec.empty()) {
		bool first = true;
		str.str("");
		for (std::vector<size_t>::const_iterator i = vec.begin(); i != vec.end(); ++ i) {
			if (!first) {
				str << ",";
			} else {
				first = false;
			}
			str << *i;
		}
	}
	return str.str();
}

play_controller::~play_controller()
{
	// update player variables from human team.
	if (!gamestate_.get_variables().empty()) {
		// when load-game use menu command, gamestate_.get_variables() is empty. in this case don't update player variable.
		for (size_t side = 0; side != teams_.size(); side ++) {
			if (teams_[side].is_human()) {
				team& t = teams_[side];
				const std::vector<size_t>& holded_cards = t.holded_cards();
				gamestate_.set_variable("player.holded_cards", vector_2_str(holded_cards));

				gamestate_.set_variable("player.navigation", lexical_cast<std::string>(t.navigation()));
				gamestate_.set_variable("player.stratum", lexical_cast<std::string>(rpg::stratum));
				gamestate_.set_variable("player.forbids", lexical_cast<std::string>(rpg::forbids));

				gamestate_.write_armory_2_variable(t);
				break;
			}
		}
	}

	clear_resources();
}

void play_controller::init(CVideo& video)
{
	uint32_t start = SDL_GetTicks();

	util::scoped_resource<loadscreen::global_loadscreen_manager*, util::delete_item> scoped_loadscreen_manager;
	loadscreen::global_loadscreen_manager* loadscreen_manager = loadscreen::global_loadscreen_manager::get();
	if (!loadscreen_manager) {
		scoped_loadscreen_manager.assign(new loadscreen::global_loadscreen_manager(video));
		loadscreen_manager = scoped_loadscreen_manager.get();
	}

	loadscreen::start_stage("load level");
	// If the recorder has no event, adds an "game start" event
	// to the recorder, whose only goal is to initialize the RNG
	if (recorder.empty()) {
		recorder.add_start();
	} else {
		recorder.pre_replay();
	}
	recorder.set_skip(false);

	bool snapshot = level_["snapshot"].to_bool();

	if (level_["modify_placing"].to_bool()) {
		// modifying placing...
		place_sides_in_preferred_locations();
	}

	foreach (const config &t, level_.child_range("time_area")) {
		tod_manager_.add_time_area(t);
	}

	// en-clean unit_map
	units_.create_coor_map(resources::game_map->w(), resources::game_map->h());

	while (!replaying_ && level_.child_count("camp")) {
		std::vector<unit*> candidate_troops;
		std::vector<hero*> candidate_heros;

		config& player_cfg = gamestate_.get_variables().child("player");
		if (!player_cfg) {
			break;
		}
		team::set_player_leader(&heros_[player_cfg["leader"].to_int()]);

		config& armory_cfg = player_cfg.child("armory");
		if (!armory_cfg) {
			break;
		}
		// troops of armory
		foreach (config &i, armory_cfg.child_range("unit")) {
			// i["cityno"] = 0;
			unit* u = new unit(units_, heros_, i);
			u->new_turn();
			// u->heal_all();
			u->set_state(unit::STATE_SLOWED, false);
			u->set_state(unit::STATE_BROKEN, false);
			u->set_state(unit::STATE_POISONED, false);
			u->set_state(unit::STATE_PETRIFIED, false);
			if (u->packed()) {
				u->pack_to(NULL);
			}
			candidate_troops.push_back(u);
		}

		// heros of armory
		const std::vector<std::string> armory_heros = utils::split(armory_cfg["heros"]);
		for (std::vector<std::string>::const_iterator itor = armory_heros.begin(); itor != armory_heros.end(); ++ itor) {
			candidate_heros.push_back(&heros_[lexical_cast_default<int>(*itor)]);
		}

		gui2::tcamp_armory dlg(candidate_troops, candidate_heros);
		dlg.show(video);
		int res = dlg.get_retval();
		if (res == gui2::twindow::CANCEL) {
			// release resource
			for (std::vector<unit*>::iterator itor = candidate_troops.begin(); itor != candidate_troops.end(); ++ itor) {
				delete *itor;
			}
			candidate_troops.clear();
			candidate_heros.clear();
			throw game::load_game_failed("init");
		}
		// save result to [player].[armory]
		std::stringstream armory_heros_str;

		armory_cfg.clear_children("unit");

		for (std::vector<unit*>::iterator itor = candidate_troops.begin(); itor != candidate_troops.end(); ++ itor) {
			config& u = armory_cfg.add_child("unit");
			(*itor)->get_location().write(u);
			(*itor)->write(u);
		}

		for (std::vector<hero*>::iterator h = candidate_heros.begin(); h != candidate_heros.end(); ++ h) {
			if (!armory_heros_str.str().empty()) {
				armory_heros_str << ", ";
			}
			armory_heros_str << lexical_cast<std::string>((*h)->number_);
		}
		armory_cfg["heros"] = armory_heros_str.str();

		// (support replay)save result to starting_pos. it is sure that there is armory block in starting_pos(reference play_game in playcampaign.cpp).
		// gui2::tcamp_armory may change [armory], write changed [armory] to starting_pos.
		config& player_cfg_dst = gamestate_.starting_pos.child("variables").child("player");
		player_cfg_dst.clear_children("armory");

		player_cfg_dst.add_child("armory", armory_cfg);

		// release resource
		for (std::vector<unit*>::iterator itor = candidate_troops.begin(); itor != candidate_troops.end(); ++ itor) {
			delete *itor;
		}
		candidate_troops.clear();
		candidate_heros.clear();
		break;
	}

	// initialized teams...
	loadscreen::start_stage("init teams");

	// This *needs* to be created before the show_intro and show_map_scene
	// as that functions use the manager state_of_game
	// Has to be done before registering any events!
	events_manager_.reset(new game_events::manager(level_));

	// if restart, rpg::h's memory maybe change. reset.
	rpg::h = &hero_invalid;
	rpg::stratum = hero_stratum_wander;
	rpg::forbids = 0;

	int team_num = 0;
	if (level_.child_count("side")) {
		foreach (const config &side, level_.child_range("side")) {
			config side_cfg = side;
			game_events::wml_expand_if(side_cfg);
			if (first_human_team_ == -1) {
				const std::string &controller = side_cfg["controller"];
				if (controller == preferences::client_type() &&
					side_cfg["id"] == preferences::login()) {
					first_human_team_ = team_num;
				} else if (controller == "human") {
					first_human_team_ = team_num;
				}
			}
			gamestate_.build_team(side_cfg, teams_, level_, map_, units_, heros_, cards_, snapshot);
			if (teams_[team_num].is_human()) {
				human_players_ ++;
			}
			++ team_num;
		}
	} else {
		unit_segment2* sides = (unit_segment2*)unit::savegame_cache_;
		int offset = sizeof(unit_segment2);
		for (uint32_t i = 0; i < sides->count_; i ++) {
			gamestate_.build_team(unit::savegame_cache_ + offset, teams_, level_, map_, units_, heros_, cards_, snapshot);
			offset += ((unit_segment2*)(unit::savegame_cache_ + offset))->size_;

			if (teams_[team_num].is_human()) {
				if (first_human_team_ == -1) {
					first_human_team_ = team_num;
				}
				human_players_ ++;
			}
			++ team_num;
		}
	}
	if (first_human_team_ == -1) {
		VALIDATE(false, "Must define one human team at least.");
	}

	config& player_cfg = gamestate_.get_variables().child("player");
	if (!player_cfg || !player_cfg.has_attribute("hero")) {
		team& human = teams_[first_human_team_];
		rpg::h = &heros_[human.leader()->number_];
		// rpg::h->official_ = hero_official_leader;
		rpg::stratum = hero_stratum_leader;
		rpg::forbids = 0;
		
	} else {
		rpg::h = &heros_[player_cfg["hero"].to_int()];
		rpg::stratum = player_cfg["stratum"].to_int();
		rpg::forbids = player_cfg["forbids"].to_int();
/*
		if (rpg::h->official_ == HEROS_NO_OFFICIAL) {
			if (rpg::stratum == hero_stratum_mayor) {
				rpg::h->official_ = hero_official_mayor;
			} else if (rpg::stratum == hero_stratum_leader) {
				rpg::h->official_ = hero_official_leader;
			}
		}
*/
	}

	if (level_.child_count("side")) {
		// if necessary, set begin fields troop to human.
		team& current_team = teams_[rpg::h->side_];
		const std::pair<unit**, size_t> p = current_team.field_troop();
		unit** troops = p.first;
		size_t troops_vsize = p.second;
		for (size_t i = 0; i < troops_vsize; i ++) {
			if (troops[i]->is_artifical()) {
				continue;
			}
			if (rpg::stratum == hero_stratum_leader) {
				troops[i]->set_human(true);
			} else if (rpg::stratum == hero_stratum_mayor) {
				if (troops[i]->cityno() == rpg::h->city_) {
					troops[i]->set_human(true);
				}
			} else if (rpg::stratum == hero_stratum_citizen) {
				if (&troops[i]->master() == rpg::h || &troops[i]->second() == rpg::h || &troops[i]->third() == rpg::h) {
					troops[i]->set_human(true);
				}
			}
		}
		
	}

	// mouse_handler expects at least one team for linger mode to work.
	if (teams_.empty()) end_level_data_.linger_mode = false;

	// loading units...
	loadscreen::start_stage("load units");
	preferences::encounter_recruitable_units(teams_);
	preferences::encounter_start_units(units_);
	preferences::encounter_recallable_units(teams_);
	preferences::encounter_map_terrain(map_);

	// initializing theme...
	loadscreen::start_stage("init theme");
	const config &theme_cfg = get_theme(game_config_, level_["theme"]);

	// building terrain rules...
	loadscreen::start_stage("build terrain");
	gui_.reset(new game_display(units_, video, map_, tod_manager_, teams_, theme_cfg, level_));
	if (!gui_->video().faked()) {
		if (gamestate_.mp_settings().mp_countdown)
			gui_->get_theme().modify_label("time-icon", _ ("time left for current turn"));
		else
			gui_->get_theme().modify_label("time-icon", _ ("current local time"));
	}

	loadscreen::start_stage("init display");
	mouse_handler_.set_gui(gui_.get());
	menu_handler_.set_gui(gui_.get());
	resources::screen = gui_.get();
	theme::set_known_themes(&game_config_);

	// done initializing display...
	if (first_human_team_ != -1) {
		gui_->set_team(first_human_team_);
	} else if (is_observer()) {
		// Find first team that is allowed to be observered.
		// If not set here observer would be without fog untill
		// the first turn of observerable side
		size_t i;
		for (i=0;i < teams_.size();++i) {
			if (!teams_[i].get_disallow_observers()) {
				gui_->set_team(i);
			}
		}
	}

	browse_ = true;

	init_managers();
	// add era events for MP game
	if (const config &era_cfg = level_.child("era")) {
		game_events::add_events(era_cfg.child_range("event"), "era_events");
	}

	loadscreen::global_loadscreen->start_stage("draw theme");
	// loadscreen_manager->reset();

	uint32_t stop = SDL_GetTicks();
	posix_print("play_controller::init, used time: %u ms\n", stop - start);
}

void play_controller::init_managers()
{
	LOG_NG << "initializing managers... " << (SDL_GetTicks() - ticks_) << "\n";
	prefs_disp_manager_.reset(new preferences::display_manager(gui_.get()));
	tooltips_manager_.reset(new tooltips::manager(gui_->video()));
	soundsources_manager_.reset(new soundsource::manager(*gui_));

	resources::soundsources = soundsources_manager_.get();

	halo_manager_.reset(new halo::manager(*gui_));
	LOG_NG << "done initializing managers... " << (SDL_GetTicks() - ticks_) << "\n";
}

static int placing_score(const config& side, const gamemap& map, const map_location& pos)
{
	int positions = 0, liked = 0;
	const t_translation::t_list terrain = t_translation::read_list(side["terrain_liked"]);

	for(int i = pos.x-8; i != pos.x+8; ++i) {
		for(int j = pos.y-8; j != pos.y+8; ++j) {
			const map_location pos(i,j);
			if(map.on_board(pos)) {
				++positions;
				if(std::count(terrain.begin(),terrain.end(),map[pos])) {
					++liked;
				}
			}
		}
	}

	return (100*liked)/positions;
}

struct placing_info {

	placing_info() :
		side(0),
		score(0),
		pos()
	{
	}

	int side, score;
	map_location pos;
};

static bool operator<(const placing_info& a, const placing_info& b) { return a.score > b.score; }

void play_controller::place_sides_in_preferred_locations()
{
	std::vector<placing_info> placings;

	int num_pos = map_.num_valid_starting_positions();

	int side_num = 1;
	foreach (const config &side, level_.child_range("side"))
	{
		for(int p = 1; p <= num_pos; ++p) {
			const map_location& pos = map_.starting_position(p);
			int score = placing_score(side, map_, pos);
			placing_info obj;
			obj.side = side_num;
			obj.score = score;
			obj.pos = pos;
			placings.push_back(obj);
		}
		++side_num;
	}

	std::sort(placings.begin(),placings.end());
	std::set<int> placed;
	std::set<map_location> positions_taken;

	for (std::vector<placing_info>::const_iterator i = placings.begin(); i != placings.end() && int(placed.size()) != side_num - 1; ++i) {
		if(placed.count(i->side) == 0 && positions_taken.count(i->pos) == 0) {
			placed.insert(i->side);
			positions_taken.insert(i->pos);
			map_.set_starting_position(i->side,i->pos);
			LOG_NG << "placing side " << i->side << " at " << i->pos << '\n';
		}
	}
}

void play_controller::objectives(){
	menu_handler_.objectives(gui_->viewing_team()+1);
}

void play_controller::show_statistics(){
	menu_handler_.show_statistics(gui_->viewing_team() + 1);
}

void play_controller::show_rpg()
{
	gui2::trpg_detail dlg(*gui_, teams_, units_, heros_);
	try {
		dlg.show(gui_->video());
	} catch(twml_exception& e) {
		e.show(*gui_);
		return;
	}
}

void play_controller::interior()
{
	menu_handler_.interior(gui_->viewing_team() + 1);
}

void play_controller::final_battle(int side_num, int human_capital, int ai_capital)
{
	// menu_handler_.final_battle(gui_->viewing_team()+1, human_capital, ai_capital);
	if (side_num == -1) {
		side_num = gui_->viewing_team() + 1;
	}
	artifical* capital_of_human;
	artifical* capital_of_ai;

	if (final_capital().first) {
		utils::string_map symbols;
		symbols["first"] = final_capital().first->master().name();
		symbols["second"] = final_capital().second? final_capital().second->master().name(): dgettext("wesnoth-lib", "Void");
		gui2::show_transient_message(gui_->video(), _("final battle"), vgettext("human capital: $first\n\n\n\nai capital: $second", symbols));
		return;
	}

	if (human_capital < 0) {
		// popup dialog, let player select capital
		std::vector<artifical*>& human_holded_cities = teams_[side_num - 1].holded_cities();
		if (human_holded_cities.size() >= 1) {

			gui2::tfinal_battle dlg(*gui_, teams_, units_, heros_, side_num);
			try {
				dlg.show(gui_->video());
			} catch(twml_exception& e) {
				e.show(*gui_);
				return;
			}
			if (dlg.get_retval() != gui2::twindow::OK) {
				return;
			}
			capital_of_human = dlg.dst_city();

		} else if (human_holded_cities.size() == 1) {
			capital_of_human = human_holded_cities[0];
		} else {
			return;
		}
	} else {
		capital_of_human = units_.city_from_cityno(human_capital);
	}

	if (!all_ai_allied()) {
		ally_all_ai(true);
	}

	if (ai_capital < 0) {
		capital_of_ai = decide_ai_capital();
	} else {
		capital_of_ai = units_.city_from_cityno(ai_capital);
	}
	set_final_capital(capital_of_human, capital_of_ai);

	utils::string_map symbols;
	symbols["first"] = final_capital().first->master().name();
	symbols["second"] = final_capital().second? final_capital().second->master().name(): dgettext("wesnoth-lib", "Void");
	gui2::show_transient_message(gui_->video(), _("final battle"), vgettext("human capital: $first\n\n\n\nai capital: $second", symbols));

	if (human_capital < 0) {
		recorder.add_final_battle(capital_of_human, capital_of_ai);
	}
}

void play_controller::list()
{
	menu_handler_.list(gui_->viewing_team() + 1);
}

void play_controller::system()
{
	menu_handler_.system(gui_->viewing_team() + 1);
}

void play_controller::unit_detail()
{
	unit_map::iterator u_itor = find_visible_unit(mouse_handler_.get_selected_hex(), teams_[gui_->viewing_team()]);
	if (u_itor.valid()) {
		menu_handler_.unit_detail(&*u_itor);
	}
}

void play_controller::save_game()
{
	if(save_blocker::try_block()) {
		save_blocker::save_unblocker unblocker;
		config snapshot;
		to_config(snapshot);
		savegame::game_savegame save(heros_, gamestate_, *gui_, snapshot);
		save.save_game_interactive(gui_->video(), "", gui::OK_CANCEL);
	} else {
		save_blocker::on_unblock(this,&play_controller::save_game);
	}
}

void play_controller::save_replay()
{
	if(save_blocker::try_block()) {
		save_blocker::save_unblocker unblocker;
		savegame::replay_savegame save(heros_, gamestate_);
		save.save_game_interactive(gui_->video(), "", gui::OK_CANCEL);
	} else {
		save_blocker::on_unblock(this,&play_controller::save_replay);
	}
}

void play_controller::save_map()
{
	if(save_blocker::try_block()) {
		save_blocker::save_unblocker unblocker;
		menu_handler_.save_map();
	} else {
		save_blocker::on_unblock(this,&play_controller::save_map);
	}
}

void play_controller::load_game()
{
	// 这里用了偷懒办法，真正正确应该是在玩家确实已经加载了一个存档文件之后再清空
	gamestate_.start_scenario_ss.str("");
	// gamestate_.start_hero_ss.str("");
	gamestate_.clear_start_hero_data();

	game_state state_copy;
	savegame::loadgame load(*gui_, game_config_, state_copy);
	load.load_game();
}

void play_controller::preferences()
{
	menu_handler_.preferences();
}

void play_controller::show_chat_log(){
	menu_handler_.show_chat_log();
}

void play_controller::show_help(){
	menu_handler_.show_help();
}

void play_controller::undo(){
	// deselect unit (only here, not to be done when undoing attack-move)
	mouse_handler_.deselect_hex();
	menu_handler_.undo(player_number_);
}

void play_controller::redo(){
	// deselect unit (only here, not to be done when undoing attack-move)
	mouse_handler_.deselect_hex();
	menu_handler_.redo(player_number_);
}

void play_controller::toggle_ellipses(){
	menu_handler_.toggle_ellipses();
}

void play_controller::toggle_grid(){
	menu_handler_.toggle_grid();
}

void play_controller::fire_prestart(bool execute)
{
	// Run initialization scripts, even if loading from a snapshot.
	resources::state_of_game->set_phase(game_state::PRELOAD);
	resources::lua_kernel->initialize();
	uint32_t start = SDL_GetTicks();
	game_events::fire("preload");
	uint32_t end_fire_preload = SDL_GetTicks();
	posix_print("play_controller::fire_prestart, fire preload, used time: %u ms\n", end_fire_preload - start);

	// pre-start events must be executed before any GUI operation,
	// as those may cause the display to be refreshed.
	if (execute){
		update_locker lock_display(gui_->video());
		resources::state_of_game->set_phase(game_state::PRESTART);

		uint32_t end_set_phase = SDL_GetTicks();
		posix_print("play_controller::fire_prestart, set_phase, used time: %u ms\n", end_set_phase - end_fire_preload);

		game_events::fire("prestart");
		uint32_t end_fire_prestart = SDL_GetTicks();
		posix_print("play_controller::fire_prestart, fire prestart, used time: %u ms\n", end_fire_prestart - end_set_phase);

		check_end_level();
		// prestart event may modify start turn with WML, reflect any changes.
		start_turn_ = turn();
	}
}

void play_controller::fire_start(bool execute){
	if(execute) {
		resources::state_of_game->set_phase(game_state::START);
		game_events::fire("start");
		check_end_level();
		// start event may modify start turn with WML, reflect any changes.
		start_turn_ = turn();
		gamestate_.get_variable("turn_number") = int(start_turn_);
	} else {
		previous_turn_ = turn();
	}
	resources::state_of_game->set_phase(game_state::PLAY);
}

void play_controller::init_gui(){
	gui_->begin_game();
	gui_->adjust_colors(0,0,0);

	for(std::vector<team>::iterator t = teams_.begin(); t != teams_.end(); ++t) {
		::clear_shroud(t - teams_.begin() + 1);
	}
}

// @team_index: base 0
void play_controller::init_side(const unsigned int team_index, bool is_replay, bool need_record)
{
	log_scope("player turn");
	team& current_team = teams_[team_index];

	mouse_handler_.set_side(team_index + 1);

	// If we are observers we move to watch next team if it is allowed
	if (is_observer()
		&& !current_team.get_disallow_observers()) {
		gui_->set_team(size_t(team_index));
	}
	gui_->set_playing_team(size_t(team_index));

	gamestate_.get_variable("side_number") = player_number_;
	gamestate_.last_selected = map_location::null_location;

	/**
	 * We do this only for local side when we are not replaying.
	 * For all other sides it is recorded in replay and replay handler has to handle calling do_init_side()
	 * functions.
	 **/
	if (!current_team.is_local() || is_replay)
		return;
	if (need_record) {
		recorder.init_side();
	}
	do_init_side(team_index);
}

/**
 * Called by replay handler or init_side() to do actual work for turn change.
 */
void play_controller::do_init_side(const unsigned int team_index)
{
	team& current_team = teams_[team_index];

	const std::string turn_num = str_cast(turn());
	const std::string side_num = str_cast(team_index + 1);

	// calculate interior exploiture
	if (!loading_game_) {
		current_team.active_commercial();
	}
	unit::commercial_exploiture_ = current_team.commercial_exploiture();
	
	// If this is right after loading a game we don't need to fire events and such. It was already done before saving.
	if (!loading_game_) {
		if (turn() != previous_turn_)
		{
			game_events::fire("turn " + turn_num);
			game_events::fire("new turn");
			previous_turn_ = turn();
			// turn or new turn may trigger endlevel, but if here will result end directly.
			// check_end_level();
		}

		game_events::fire("side turn");
		game_events::fire("side " + side_num + " turn");
		game_events::fire("side turn " + turn_num);
		game_events::fire("side " + side_num + " turn " + turn_num);
	}

	if (current_team.is_human()) {
		gui_->set_team(player_number_ - 1);
		gui_->recalculate_minimap();
		gui_->invalidate_all();
		gui_->draw(true,true);
	}
	// We want to work out if units for this player should get healed,
	// and the player should get income now.
	// Healing/income happen if it's not the first turn of processing,
	// or if we are loading a game.
	if (!loading_game_ && turn() > 1) {
		if (player_number_ == rpg::h->side_ + 1 && rpg::forbids) {
			rpg::forbids --;
		}
		uint32_t start = SDL_GetTicks();
		for (unit_map::iterator i = units_.begin(); i != units_.end(); ++i) {
			if (i->side() == player_number_) {
				i->new_turn();
			}
		}
		
		current_team.new_turn();
		uint32_t new_turn = SDL_GetTicks();
		posix_print("(new_turn)used time: %u ms\n", new_turn - start);

		// If the expense is less than the number of villages owned,
		// then we don't have to pay anything at all
		int expense = side_upkeep(player_number_) -
								current_team.villages().size();
		if (expense > 0) {
			current_team.spend_gold(expense);
		}
		uint32_t gold = SDL_GetTicks();
		posix_print("(gold)used time: %u ms\n", gold - new_turn);

		calculate_healing(player_number_, !skip_replay_);
		uint32_t healing = SDL_GetTicks();
		posix_print("(healing)used time: %u ms\n", healing - new_turn);

		reset_resting(units_, player_number_);
		uint32_t resting = SDL_GetTicks();
		posix_print("(resting)used time: %u ms\n", resting - healing);
	}

	if (!loading_game_) {
		game_events::fire("turn refresh");
		game_events::fire("side " + side_num + " turn refresh");
		game_events::fire("turn " + turn_num + " refresh");
		game_events::fire("side " + side_num + " turn " + turn_num + " refresh");
	}

	const time_of_day &tod = tod_manager_.get_time_of_day();

	if (int(team_index) + 1 == first_player_)
		sound::play_sound(tod.sounds, sound::SOUND_SOURCES);

	if (!recorder.is_skipping()){
		::clear_shroud(team_index + 1);
		gui_->invalidate_all();
	}

	if (!recorder.is_skipping() && !skip_replay_ && current_team.get_scroll_to_leader()){
		gui_->scroll_to_leader(units_, player_number_,game_display::ONSCREEN,false);
	}
	loading_game_ = false;
}

//builds the snapshot config from its members and their configs respectively
void play_controller::to_config(config& cfg) const
{
	// config cfg;
	cfg.clear();

	cfg.merge_attributes(level_);
	if (final_capital_.first && !cfg.has_attribute("final_capital")) {
		std::stringstream str;
		str << final_capital_.first->cityno() << "," << (final_capital_.second? final_capital_.second->cityno(): 0);
		cfg["final_capital"] = str.str();
	}
	cfg["maximal_defeated_activity"] = game_config::maximal_defeated_activity;

	if (all_ai_allied_) {
		cfg["ai_allied"] = all_ai_allied_;
	}
	cfg["runtime"] = true;

	unit_segment2* top = (unit_segment2*)unit::savegame_cache_;
	top->count_ = teams_.size();
	int offset = sizeof(unit_segment2);
	
	for (std::vector<team>::const_iterator t = teams_.begin(); t != teams_.end(); ++t) {
		const std::vector<artifical*>& holded_cities = t->holded_cities();
		const std::pair<unit**, size_t> p = t->field_troop();

		unit_segment2* side = (unit_segment2*)(unit::savegame_cache_ + offset);
		side->count_ = holded_cities.size() + p.second;
		side->size_ = offset;
		offset += sizeof(unit_segment2);

		t->write(unit::savegame_cache_ + offset);
		offset += ((team_fields_t*)(unit::savegame_cache_ + offset))->size_;

		// current visible units
		for (std::vector<artifical*>::const_iterator i = holded_cities.begin(); i != holded_cities.end(); ++ i) {
			artifical* city = *i;
			// this city
			// Attention: Config of city must be written prior to other artifical. When loading, any artifical added to city, this city must be evalued.
			city->write(unit::savegame_cache_ + offset);
			unit_fields_t* fields = (unit_fields_t*)(unit::savegame_cache_ + offset);
			offset += fields->size_;
		}
		unit** troops = p.first;
		size_t troops_vsize = p.second;
		for (size_t i = 0; i < troops_vsize; i ++) {
			if (troops[i]->is_artifical()) {
				// field artifcals
				troops[i]->write(unit::savegame_cache_ + offset);
				unit_fields_t* fields = (unit_fields_t*)(unit::savegame_cache_ + offset);
				offset += fields->size_;
			} else {
				// field troops
				troops[i]->write(unit::savegame_cache_ + offset);
				unit_fields_t* fields = (unit_fields_t*)(unit::savegame_cache_ + offset);
				offset += fields->size_;
			}
		}
		side->size_ = offset - side->size_;
	}
	top->size_ = offset;

	cfg.merge_with(tod_manager_.to_config());

	// Write terrain_graphics data in snapshot, too
	foreach (const config &tg, level_.child_range("terrain_graphics")) {
		cfg.add_child("terrain_graphics", tg);
	}

	//write out the current state of the map
	cfg["map_data"] = map_.write();
}

void play_controller::finish_side_turn()
{
	for (unit_map::iterator uit = units_.begin(); uit != units_.end(); ++uit) {
		if (uit->side() == player_number_)
			uit->end_turn();
	}

	const std::string turn_num = str_cast(turn());
	const std::string side_num = str_cast(player_number_);
	game_events::fire("side turn end");
	game_events::fire("side "+ side_num + " turn end");
	game_events::fire("side turn " + turn_num + " end");
	game_events::fire("side " + side_num + " turn " + turn_num + " end");

	// This implements "delayed map sharing."
	// It is meant as an alternative to shared vision.
	if(current_team().copy_ally_shroud()) {
		gui_->recalculate_minimap();
		gui_->invalidate_all();
	}

	mouse_handler_.deselect_hex();
	n_unit::id_manager::instance().reset_fake();
	game_events::pump();
}

void play_controller::finish_turn()
{
  	const std::string turn_num = str_cast(turn() - 1);
	const std::string side_num = str_cast(player_number_);
	game_events::fire("turn end");
	game_events::fire("turn " + turn_num + " end");

	LOG_NG << "turn event..." << (recorder.is_skipping() ? "skipping" : "no skip") << '\n';
	update_locker lock_display(gui_->video(),recorder.is_skipping());
	gamestate_.get_variable("turn_number") = int(turn());
}

bool play_controller::enemies_visible() const
{
	// If we aren't using fog/shroud, this is easy :)
	if(current_team().uses_fog() == false && current_team().uses_shroud() == false)
		return true;

	// See if any enemies are visible
	for(unit_map::const_iterator u = units_.begin(); u != units_.end(); ++u)
		if (current_team().is_enemy(u->side()) && !gui_->fogged(u->get_location()))
			return true;

	return false;
}

bool play_controller::execute_command(hotkey::HOTKEY_COMMAND command, int index, std::string str)
{
	if (index >= 0) {
		unsigned i = static_cast<unsigned>(index);
		if (i < savenames_.size() && !savenames_[i].empty()) {
			// Load the game by throwing load_game_exception
			throw game::load_game_exception(savenames_[i],false,false);

		}
	}
	return command_executor::execute_command(command, index, str);
}

bool play_controller::can_execute_command(hotkey::HOTKEY_COMMAND command, int index) const
{
	if (index >= 0) {
		unsigned i = static_cast<unsigned>(index);
		if((i < savenames_.size() && !savenames_[i].empty())
		|| (i < wml_commands_.size() && wml_commands_[i] != NULL)) {
			return true;
		}
	}
	switch(command) {

	// Commands we can always do:
	case hotkey::HOTKEY_ZOOM_IN:
	case hotkey::HOTKEY_ZOOM_OUT:
	case hotkey::HOTKEY_ZOOM_DEFAULT:
	case hotkey::HOTKEY_FULLSCREEN:
	case hotkey::HOTKEY_SCREENSHOT:
	case hotkey::HOTKEY_MAP_SCREENSHOT:
	case hotkey::HOTKEY_ACCELERATED:
	case hotkey::HOTKEY_TOGGLE_ELLIPSES:
	case hotkey::HOTKEY_TOGGLE_GRID:
	case hotkey::HOTKEY_MOUSE_SCROLL:
	case hotkey::HOTKEY_ANIMATE_MAP:
	case hotkey::HOTKEY_MUTE:
	case hotkey::HOTKEY_PREFERENCES:
	case hotkey::HOTKEY_OBJECTIVES:
	case hotkey::HOTKEY_STATISTICS:
	case hotkey::HOTKEY_QUIT_GAME:
	case hotkey::HOTKEY_CLEAR_MSG:
	case hotkey::HOTKEY_UNIT_DETAIL:
		return true;
	case hotkey::HOTKEY_HELP:
		return game_config::tiny_gui? false: true;

	case hotkey::HOTKEY_RPG:
		return network::nconnections() == 0; // Can only load games if not in a network game

	case hotkey::HOTKEY_CHAT_LOG:
		return network::nconnections() > 0;

	case hotkey::HOTKEY_UNDO:
		return !linger_ && !undo_stack_.empty() && !events::commands_disabled && !browse_;

	default:
		return false;
	}
}

void play_controller::enter_textbox()
{
	if(menu_handler_.get_textbox().active() == false) {
		return;
	}

	const std::string str = menu_handler_.get_textbox().box()->text();
	const unsigned int team_num = player_number_;
	events::mouse_handler& mousehandler = mouse_handler_;

	switch(menu_handler_.get_textbox().mode()) {
	case gui::TEXTBOX_MESSAGE:
		menu_handler_.do_speak();
		menu_handler_.get_textbox().close(*gui_);  //need to close that one after executing do_speak() !
		break;
	default:
		menu_handler_.get_textbox().close(*gui_);
		ERR_DP << "unknown textbox mode\n";
	}

}

void play_controller::tab()
{
	gui::TEXTBOX_MODE mode = menu_handler_.get_textbox().mode();

	std::set<std::string> dictionary;
	switch(mode) {
	case gui::TEXTBOX_MESSAGE:
	{
		foreach(const team& t, teams_) {
			if(!t.is_empty())
				dictionary.insert(t.current_player());
		}

		// Add observers
		foreach(const std::string& o, gui_->observers()){
			dictionary.insert(o);
		}
		//Exclude own nick from tab-completion.
		//NOTE why ?
		dictionary.erase(preferences::login());

		break;
	}
	default:
		ERR_DP << "unknown textbox mode\n";
	} //switch(mode)

	menu_handler_.get_textbox().tab(dictionary);
}


team& play_controller::current_team()
{
	assert(player_number_ > 0 && player_number_ <= int(teams_.size()));
	return teams_[player_number_-1];
}

const team& play_controller::current_team() const
{
	assert(player_number_ > 0 && player_number_ <= int(teams_.size()));
	return teams_[player_number_-1];
}

int play_controller::find_human_team_before(const size_t team_num) const
{
	if (team_num > teams_.size())
		return -2;

	int human_side = -2;
	for (int i = team_num-2; i > -1; --i) {
		if (teams_[i].is_human()) {
			human_side = i;
			break;
		}
	}
	if (human_side == -2) {
		for (size_t i = teams_.size()-1; i > team_num-1; --i) {
			if (teams_[i].is_human()) {
				human_side = i;
				break;
			}
		}
	}
	return human_side+1;
}

void play_controller::slice_before_scroll() {
	soundsources_manager_->update();
}

events::mouse_handler& play_controller::get_mouse_handler_base() {
	return mouse_handler_;
}

game_display& play_controller::get_display() {
	return *gui_;
}

bool play_controller::have_keyboard_focus()
{
	return !menu_handler_.get_textbox().active();
}

void play_controller::process_focus_keydown_event(const SDL_Event& event)
{
	if(event.key.keysym.sym == SDLK_ESCAPE) {
		menu_handler_.get_textbox().close(*gui_);
	} else if(event.key.keysym.sym == SDLK_TAB) {
		tab();
	} else if(event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER) {
		enter_textbox();
	}
}

void play_controller::process_keydown_event(const SDL_Event& event) {
}

void play_controller::process_keyup_event(const SDL_Event& event) {
	// If the user has pressed 1 through 9, we want to show
	// how far the unit can move in that many turns
	if(event.key.keysym.sym >= '1' && event.key.keysym.sym <= '7') {
		const int new_path_turns = (event.type == SDL_KEYDOWN) ?
		                           event.key.keysym.sym - '1' : 0;

		if(new_path_turns != mouse_handler_.get_path_turns()) {
			mouse_handler_.set_path_turns(new_path_turns);

			const unit_map::iterator u = mouse_handler_.selected_unit();

			if(u != units_.end()) {
				bool teleport = u->get_ability_bool("teleport");

				// if it's not the unit's turn, we reset its moves
				unit_movement_resetter move_reset(*u, u->side() != player_number_);

				mouse_handler_.set_current_paths(pathfind::paths(map_, units_, u->get_location(),
				                       teams_,false,teleport, teams_[gui_->viewing_team()],
				                       mouse_handler_.get_path_turns()));

				gui_->highlight_reach(mouse_handler_.current_paths());
			}
		}
	}
}

void play_controller::post_mouse_press(const SDL_Event& /*event*/)
{
	if (mouse_handler_.get_undo()) {
		mouse_handler_.set_undo(false);
		menu_handler_.undo(player_number_);
	}
}

static void trim_items(std::vector<std::string>& newitems) {
	if (newitems.size() > 5) {
		std::vector<std::string> subitems;
		subitems.push_back(newitems[0]);
		subitems.push_back(newitems[1]);
		subitems.push_back(newitems[newitems.size() / 3]);
		subitems.push_back(newitems[newitems.size() * 2 / 3]);
		subitems.push_back(newitems.back());
		newitems = subitems;
	}
}

void play_controller::expand_autosaves(std::vector<std::string>& items)
{
}

void play_controller::expand_wml_commands(std::vector<std::string>& items)
{
	wml_commands_.clear();
	for (unsigned int i = 0; i < items.size(); ++i) {
		if (items[i] == "wml") {
			items.erase(items.begin() + i);
			std::map<std::string, wml_menu_item*>& gs_wmi = gamestate_.wml_menu_items;
			if(gs_wmi.empty())
				break;
			std::vector<std::string> newitems;

			const map_location& hex = mouse_handler_.get_last_hex();
			gamestate_.get_variable("x1") = hex.x + 1;
			gamestate_.get_variable("y1") = hex.y + 1;
			scoped_xy_unit highlighted_unit("unit", hex.x, hex.y, units_);

			std::map<std::string, wml_menu_item*>::iterator itor;
			for (itor = gs_wmi.begin(); itor != gs_wmi.end()
				&& newitems.size() < MAX_WML_COMMANDS; ++itor) {
				config& show_if = itor->second->show_if;
				config filter_location = itor->second->filter_location;
				if ((show_if.empty()
					|| game_events::conditional_passed(vconfig(show_if)))
				&& (filter_location.empty()
					|| terrain_filter(vconfig(filter_location), units_)(hex))
				&& (!itor->second->needs_select
					|| gamestate_.last_selected.valid()))
				{
					wml_commands_.push_back(itor->second);
					std::string newitem = itor->second->description;

					// Prevent accidental hotkey binding by appending a space
					newitem.push_back(' ');
					newitems.push_back(newitem);
				}
			}
			items.insert(items.begin()+i, newitems.begin(), newitems.end());
			break;
		}
		wml_commands_.push_back(NULL);
	}
}

// @m: memu object taht will be display
// @gui: global game_display reference
void play_controller::show_context_menu(theme::menu* m, display& gui)
{
	uint32_t show_flags = 0, idx_in_u32 = 0, disable_flags = 0;

	const theme::menu* m_adjusted = m;
	if (!m) {
		m_adjusted = gui.get_theme().context_menu("");
	}
	if (!m_adjusted) {
		return;
	}

	const std::vector<std::string>& items = m_adjusted->items();
	hotkey::HOTKEY_COMMAND command;
	std::vector<std::string>::const_iterator i = items.begin();
	while (i != items.end()) {
		command = hotkey::get_hotkey(*i).get_id();
		// Remove commands that can't be executed or don't belong in this type of menu
		if (in_context_menu(command)) {
			show_flags |= 1 << idx_in_u32;
			if (!enable_context_menu(command, mouse_handler_.get_selected_hex())) {
				disable_flags |= 1 << idx_in_u32;
			}
		}
		idx_in_u32 ++;
		++ i;
	}
	if (!show_flags) {
		return;
	}

	gui.hide_context_menu(m_adjusted, false, show_flags, disable_flags);
}

void play_controller::show_menu(const std::vector<std::string>& items_arg, int xloc, int yloc, bool context_menu)
{
	std::vector<std::string> items = items_arg;
	hotkey::HOTKEY_COMMAND command;
	std::vector<std::string>::iterator i = items.begin();
	while(i != items.end()) {
		command = hotkey::get_hotkey(*i).get_id();
		// Remove WML commands if they would not be allowed here
		if(*i == "wml") {
			if(!context_menu || gui_->viewing_team() != gui_->playing_team()
			|| events::commands_disabled || !teams_[gui_->viewing_team()].is_human()) {
				i = items.erase(i);
				continue;
			}
		// Remove commands that can't be executed or don't belong in this type of menu
		} else if(!can_execute_command(command)
		|| (context_menu && !in_context_menu(command))) {
			i = items.erase(i);
			continue;
		}
		++i;
	}

	// Add special non-hotkey items to the menu and remember their indices
	// don't show autosave files
	expand_autosaves(items);
	expand_wml_commands(items);

	if(items.empty())
		return;

	command_executor::show_menu(items, xloc, yloc, context_menu, *gui_);
}

bool play_controller::in_context_menu(hotkey::HOTKEY_COMMAND command) const
{
	// current side must human player
	const team& current_team = teams_[player_number_ - 1];

	if (command != hotkey::HOTKEY_LIST && command != hotkey::HOTKEY_SYSTEM && !replaying_ && !current_team.is_human()) {
		return false;
	}
	if (linger_) {
		return command == hotkey::HOTKEY_LIST || command == hotkey::HOTKEY_SYSTEM;
	}
	if (replaying_ && (command == hotkey::HOTKEY_INTERIOR || command == hotkey::HOTKEY_FINAL_BATTLE)) {
		// current don't support, remark them.
		return false;
	}

	const artifical* city = NULL;
	const map_location& loc = mouse_handler_.get_selected_hex();
	unit_map::const_iterator itor;
	if (!replaying_) {
		itor = find_visible_unit(loc, current_team);
	}

	switch(command) {
	// Only display these if the mouse is over a castle or keep tile
	case hotkey::HOTKEY_RECRUIT:
	case hotkey::HOTKEY_RECALL:
	case hotkey::HOTKEY_ARMORY:
	case hotkey::HOTKEY_MOVE:
		// loc is in myself city.
		city = units_.city_from_loc(loc);
		if (itor.valid() && city && (city->side() == player_number_)) {
			return true;
		}
		break;

	case hotkey::HOTKEY_BUILD_M:
		if (current_team.builds().empty()) {
			return false;
		}
	case hotkey::HOTKEY_BUILD:
		if (itor.valid() && !itor->is_artifical() && itor->human() && itor->attacks_left()) {
			return true;
		}
		break;

	case hotkey::HOTKEY_DEMOLISH:
		if (rpg::stratum == hero_stratum_citizen) {
			return false;
		}
		if (itor.valid() && itor->is_artifical() && (itor->side() == player_number_) && !artifical_is_city(&*itor)) {
			return true;
		}
		break;

	case hotkey::HOTKEY_INTERIOR:
		return rpg::stratum == hero_stratum_leader && !events::commands_disabled && !itor.valid();

	case hotkey::HOTKEY_FINAL_BATTLE:
		return !events::commands_disabled && (teams_.size() >= 3) && !itor.valid() && (human_players_ == 1) && !network::nconnections();

	case hotkey::HOTKEY_LIST:
	case hotkey::HOTKEY_SYSTEM:
		return replaying_ || (!events::commands_disabled && !itor.valid() && current_team.is_human());

	default:
		return true;
	}

	return false;
}

bool play_controller::enable_context_menu(hotkey::HOTKEY_COMMAND command, const map_location& loc) const
{
	const team& current_team = teams_[player_number_ - 1];
	unit_map::const_iterator itor;
	if (!replaying_) {
		itor = find_visible_unit(loc, current_team);
	}
	const artifical* city = units_.city_from_loc(loc);

	switch(command) {
	// Only display these if the mouse is over a castle or keep tile
	case hotkey::HOTKEY_RECRUIT:
		if (rpg::stratum == hero_stratum_citizen) {
			return false;
		} else if (rpg::stratum == hero_stratum_mayor && city->mayor() != rpg::h) {
			return false;
		}
		if (city->fresh_heros().empty()) {
			return false;
		}
		break;
	case hotkey::HOTKEY_RECALL:
		if (city->reside_troops().empty()) {
			return false;
		}
		if (city->is_surrounded()) {
			return false;
		}
		break;
	case hotkey::HOTKEY_ARMORY:
		{
			const std::vector<unit>& reside_troops = city->reside_troops();
			const std::vector<hero*>& fresh_heros = city->fresh_heros();
			size_t reside_troops_size = reside_troops.size();
			if (!reside_troops_size || (reside_troops_size == 1 && !reside_troops[0].second().valid() && fresh_heros.empty())) {
				return false;
			}
			for (std::vector<unit>::const_iterator it = reside_troops.begin(); it != reside_troops.end(); ++ it) {
				if (it->human()) {
					return true;
				}
			}
			return false;
		}
	case hotkey::HOTKEY_MOVE:
		if (rpg::stratum == hero_stratum_citizen) {
			if (city->cityno() != rpg::h->city_ || rpg::h->status_ != hero_status_idle) {
				return false;
			}
		} else if (rpg::stratum == hero_stratum_mayor && city->mayor() != rpg::h) {
			return false;
		}
		if (current_team.holded_cities().size() < 2 || city->fresh_heros().empty()) {
			return false;
		}
		break;
	case hotkey::HOTKEY_DEMOLISH:
		if (itor.valid() && itor->is_artifical() && (itor->side() == player_number_) && !artifical_is_city(&*itor)) {
			const artifical* art = unit_2_artifical(&*itor);
			const map_location* tiles;
			size_t adjacent_size;
			// to mayor, can domolish it that owner his city.
			if (rpg::stratum == hero_stratum_mayor) {
				if (art->type() == unit_types.find_market()) {
					std::map<const map_location, int>::const_iterator it = unit_map::economy_areas_.find(loc);
					if (units_.city_from_cityno(it->second)->mayor() != rpg::h) {
						return false;
					}
				} else if (art->type() == unit_types.find_keep() || art->type() == unit_types.find_wall()) {
					bool found_rpg_city = false;
					tiles = itor->adjacent_2_;
					adjacent_size = itor->adjacent_size_2_;
					for (int i = 0; i != adjacent_size; ++i) {
						unit_map::const_iterator another = units_.find(tiles[i]);
						if (another.valid() && another->is_city()) {
							const artifical* city = unit_2_artifical(&*another);
							if (city->mayor() == rpg::h) {
								found_rpg_city = true;
								break;
							}
						}
					}
					if (!found_rpg_city) {
						return false;
					}
				}
			}
			// there is enemy unit in 2 grids, cannot demolish.
			for (int times = 0; times < 2; times ++) {
				if (times == 0) {
					tiles = art->adjacent_;
					adjacent_size = art->adjacent_size_;
				} else {
					tiles = itor->adjacent_2_;
					adjacent_size = itor->adjacent_size_2_;
				}
				for (int i = 0; i != adjacent_size; ++i) {
					unit_map::const_iterator another = units_.find(tiles[i]);
					if (another.valid() && current_team.is_enemy(another->side())) {
						return false;
					}
					another = units_.find(tiles[i], false);
					if (another.valid() && current_team.is_enemy(another->side())) {
						return false;
					}
				}
			}
			return true;			
		}
		break;
	}

	return true;
}

void play_controller::refresh_city_buttons(const artifical& city) const
{
	static hotkey::HOTKEY_COMMAND city_buttons[] = {
		hotkey::HOTKEY_RECRUIT, 
		hotkey::HOTKEY_RECALL, 
		hotkey::HOTKEY_ARMORY, 
		hotkey::HOTKEY_MOVE
	};
	static int nb = sizeof(city_buttons) / sizeof(hotkey::HOTKEY_COMMAND);
	for (int i = 0; i < nb; i ++) {
		gui::button* b = gui_->find_button(hotkey::get_hotkey(city_buttons[i]).get_command());
		if (b) {
			b->enable(enable_context_menu(city_buttons[i], city.get_location()));
		}
	}
}

std::string play_controller::get_action_image(hotkey::HOTKEY_COMMAND command, int index) const
{
	if(index >= 0 && index < static_cast<int>(wml_commands_.size())) {
		wml_menu_item* const& wmi = wml_commands_[index];
		if(wmi != NULL) {
			return wmi->image.empty() ? game_config::images::wml_menu : wmi->image;
		}
	}
	return command_executor::get_action_image(command, index);
}

hotkey::ACTION_STATE play_controller::get_action_state(hotkey::HOTKEY_COMMAND command, int /*index*/) const
{
	switch(command) {
	case hotkey::HOTKEY_DELAY_SHROUD:
		return teams_[gui_->viewing_team()].auto_shroud_updates() ? hotkey::ACTION_OFF : hotkey::ACTION_ON;
	default:
		return hotkey::ACTION_STATELESS;
	}
}

namespace {
	static const std::string empty_str = "";
}

const std::string& play_controller::select_victory_music() const
{
	if(victory_music_.empty())
		return empty_str;

	const size_t p = gamestate_.rng().get_next_random() % victory_music_.size();
	assert(p < victory_music_.size());
	return victory_music_[p];
}

const std::string& play_controller::select_defeat_music() const
{
	if(defeat_music_.empty())
		return empty_str;

	const size_t p = gamestate_.rng().get_next_random() % defeat_music_.size();
	assert(p < defeat_music_.size());
	return defeat_music_[p];
}


void play_controller::set_victory_music_list(const std::string& list)
{
	victory_music_ = utils::split(list);
	if(victory_music_.empty())
		victory_music_ = utils::split(game_config::default_victory_music);
}

void play_controller::set_defeat_music_list(const std::string& list)
{
	defeat_music_  = utils::split(list);
	if(defeat_music_.empty())
		defeat_music_ = utils::split(game_config::default_defeat_music);
}

void play_controller::check_victory()
{
	check_end_level();

	// cities_teams saved this teams that holded one or more city
	std::vector<unsigned> cities_teams;

	bool human_defeated = false;

	// Clear villages for teams that have no city && no field troop
	for (std::vector<team>::iterator tm_beg = teams_.begin(), tm = tm_beg, tm_end = teams_.end(); tm != tm_end; ++tm) {
		if (!tm->holded_cities().empty()) {
			// don't conside side that hasn't city
			cities_teams.push_back(tm->side());
		} else if (teams_[tm->side() - 1].is_human()) {
			human_defeated = true;
		}
		const std::pair<unit**, size_t> p = tm->field_troop();
		if (!p.second && tm->holded_cities().empty() && !tm->villages().empty()) {
			tm->clear_villages();
			// invalidate_all() is overkill and expensive but this code is
			// run rarely so do it the expensive way.
			gui_->invalidate_all();
		}
	}
	if (human_defeated) {
		throw end_level_exception(DEFEAT);
	}

	if (final_capital_.first) {
		if (final_capital_.second && teams_[final_capital_.second->side() - 1].is_human()) {
			throw end_level_exception(VICTORY);
		} else if (!teams_[final_capital_.first->side() - 1].is_human()) {
			throw end_level_exception(DEFEAT);
		} 
	}

	bool found_player = false;

	// found_player: 
	//   ture: human控制的阵营已不存在敌对阵营
	//   false: 
	for (size_t n = 0; n != cities_teams.size(); ++n) {
		size_t side = cities_teams[n] - 1;

		for (size_t m = n +1 ; m != cities_teams.size(); ++m) {
			if (teams_[side].is_enemy(cities_teams[m])) {
				return;
			}
		}

		if (teams_[side].is_human()) {
			found_player = true;
		}
	}

	if (found_player) {
		game_events::fire("enemies defeated");
		check_end_level();
	}

	if (!victory_when_enemies_defeated_ && (found_player || is_observer())) {
		// This level has asked not to be ended by this condition.
		return;
	}

	if (non_interactive()) {
		std::cout << "winner: ";
		foreach (unsigned l, cities_teams) {
			std::string ai = ai::manager::get_active_ai_identifier_for_side(l);
			if (ai.empty()) ai = "default ai";
			std::cout << l << " (using " << ai << ") ";
		}
		std::cout << '\n';
	}

	DBG_NG << "throwing end level exception...\n";
	throw end_level_exception(VICTORY);
}

void play_controller::process_oos(const std::string& msg) const
{
	if (game_config::ignore_replay_errors) return;

	std::stringstream message;
	message << _("The game is out of sync. It might not make much sense to continue. Do you want to save your game?");
	message << "\n\n" << _("Error details:") << "\n\n" << msg;

	config snapshot;
	to_config(snapshot);
	savegame::oos_savegame save(heros_, snapshot);
	save.save_game_interactive(resources::screen->video(), message.str(), gui::YES_NO); // can throw end_level_exception
}

void play_controller::set_final_capital(artifical* human, artifical* ai)
{
	final_capital_.first = human;
	final_capital_.second = ai;
}

bool play_controller::do_recruit(artifical* city, int cost_exponent, bool rpg_mode)
{
	int side_num = city->side();
	const map_location& city_loc = city->get_location();
	team& current_team = teams_[side_num - 1];

	const hero *master, *second, *third;
	const unit_type* ut;
	if (city->fresh_heros().empty()) {
		gui2::show_transient_message(gui_->video(), "", _("There are no hero available to recruit"));
		return false;
	}
	if (current_team.recruits().empty()) {
		gui2::show_transient_message(gui_->video(), "", _("There are no unit type available to recruit."));
		return false;
	}

	{
		gui2::trecruit dlg(*gui_, teams_, units_, heros_, *city, cost_exponent, rpg_mode);
		try {
			dlg.show(gui_->video());
		} catch(twml_exception& e) {
			e.show(*gui_);
			return false;
		}
		// player may generate troop when select heors, and these heros were changed to hero_status_military status.
		// it is time to be back hero_status_idle.
		std::vector<hero*>& fresh_heros = city->fresh_heros();
		for (std::vector<hero*>::iterator itor = fresh_heros.begin(); itor != fresh_heros.end(); ++ itor) {
			(*itor)->status_ = hero_status_idle;
		}
		if (dlg.get_retval() != gui2::twindow::OK) {
			return false;
		}
		ut = dlg.unit_type_ptr();
		master = dlg.master();
		second = dlg.second();
		third = dlg.third();
		if (!master->valid()) {
			return false;
		}
	}

	if (ut->cost() * cost_exponent / 100 > current_team.gold()) {
		gui2::show_transient_message(gui_->video(), "", _("You don't have enough gold to recruit that unit"));
		return false;
	}
	const events::command_disabler disable_commands;

	bool commercial_changed = false;

	// generated troop: 1)can full movement; 2)can full attack.
	std::vector<const hero*> v;
	v.push_back(&heros_[master->number_]);
	city->hero_go_out(*master);
	if (v.back()->official_ == hero_official_commercial) {
		current_team.erase_commercial(&heros_[master->number_]);
		commercial_changed = true;
	}
	if (second->valid()) {
		v.push_back(&heros_[second->number_]);
		city->hero_go_out(*second);
		if (v.back()->official_ == hero_official_commercial) {
			current_team.erase_commercial(&heros_[second->number_]);
			commercial_changed = true;
		}
	}
	if (third->valid()) {
		v.push_back(&heros_[third->number_]);
		city->hero_go_out(*third);
		if (v.back()->official_ == hero_official_commercial) {
			current_team.erase_commercial(&heros_[third->number_]);
			commercial_changed = true;
		}
	}
	if (commercial_changed) {
		unit::commercial_exploiture_ = current_team.commercial_exploiture();
	}

	type_heros_pair pair(ut, v);

	// create a unit with traits
	recorder.add_recruit(ut->id(), city_loc, v, ut->cost() * cost_exponent / 100, true);
	
	unit new_unit(units_, heros_, pair, city->cityno(), true);
	new_unit.set_movement(new_unit.total_movement());
	new_unit.set_attacks(new_unit.attacks_total());
	current_team.spend_gold(ut->cost() * cost_exponent / 100);

	city->troop_come_into(new_unit);
	city->reside_troops().back().set_human(true);

	menu_handler_.clear_undo_stack(side_num);

	map_location loc2(MAGIC_RESIDE, city->reside_troops().size() - 1);
	game_events::fire("post_recruit", city_loc, loc2);

	refresh_city_buttons(*city);

	return true;
}

void play_controller::rpg_update()
{
	std::string message;
	int incident = game_events::INCIDENT_APPOINT;
	if (rpg::stratum == hero_stratum_citizen) {
		bool control_more = false;
		if (rpg::h->meritorious_ >= 8000) {
			artifical* city = units_.city_from_cityno(rpg::h->city_);

			utils::string_map symbols;
			symbols["city"] = city->name();
			message = vgettext("Congratulate! You are appointed $city mayor.", symbols);
			game_events::show_hero_message(&heros_[229], city, message, incident);

			city->select_mayor(rpg::h);

			// all reside/field troop to human
			std::vector<unit>& resides = city->reside_troops();
			for (std::vector<unit>::iterator it = resides.begin(); it != resides.end(); ++ it) {
				unit& u = *it;
				u.set_human(true);
			}
			std::vector<unit*>& fields = city->field_troops();
			for (std::vector<unit*>::iterator it = fields.begin(); it != fields.end(); ++ it) {
				unit& u = **it;
				if (!u.human()) {
					u.set_human(true);
					u.set_goto(map_location());
					gui_->refresh_access_troops(u.side() - 1, game_display::REFRESH_INSERT, NULL, &u);
					if (!unit_can_move(u)) {
						gui_->refresh_access_troops(u.side() - 1, game_display::REFRESH_DISABLE, NULL, &u);
					}
				}
			}
			// rpg::forbids = 10;
		} else if (rpg::h->meritorious_ >= 5000 && rpg::humans.size() < 4) {
			control_more = true;
		} else if (rpg::h->meritorious_ >= 2500 && rpg::humans.size() < 3) {
			control_more = true;
		} else if (rpg::h->meritorious_ >= 1000 && rpg::humans.size() < 2) {
			control_more = true;
		}
		if (control_more) {
			std::vector<const unit*> units_ptr;
			team& current_team = (*resources::teams)[rpg::h->side_];
			std::vector<artifical*>& holded_cities = current_team.holded_cities();
			for (std::vector<artifical*>::iterator it = holded_cities.begin(); it != holded_cities.end(); ++ it) {
				artifical* city = *it;
				std::vector<unit>& resides = city->reside_troops();
				for (std::vector<unit>::iterator it = resides.begin(); it != resides.end(); ++ it) {
					unit& u = *it;
					if (!u.human()) {
						units_ptr.push_back(&u);
					}
				}
			
				std::vector<unit*>& fields = city->field_troops();
				for (std::vector<unit*>::iterator it = fields.begin(); it != fields.end(); ++ it) {
					unit& u = **it;
					if (!u.human()) {
						units_ptr.push_back(&u);
					}
				}
			}

			if (units_ptr.empty()) {
				return;
			}
			message = _("You can control more troop.");
			game_events::show_hero_message(&heros_[229], NULL, message, incident);

			int res = -1;
			std::string name = "input";
			if (!replaying_) {
				gui2::tselect_unit dlg(*gui_, teams_, units_, heros_, units_ptr, gui2::tselect_unit::HUMAN);
				try {
					dlg.show(gui_->video());
				} catch(twml_exception& e) {
					e.show(*gui_);
				}

				res = dlg.troop_index();

				config cfg;
				cfg["value"] = res;
				recorder.user_input(name, cfg);
			} else {
				do_replay_handle(player_number_, name);
				const config *action = get_replay_source().get_next_action();
				if (!action || !*(action = &action->child(name))) {
					replay::process_error("[" + name + "] expected but none found\n");
				}
				res = action->get("value")->to_int(-1);
			}
			unit& u = *(const_cast<unit*>(units_ptr[res]));
			u.set_human(true);
			u.set_goto(map_location());
			if (!units_.city_from_loc(u.get_location())) {
				gui_->refresh_access_troops(u.side() - 1, game_display::REFRESH_INSERT, NULL, &u);
				if (!unit_can_move(u)) {
					gui_->refresh_access_troops(u.side() - 1, game_display::REFRESH_DISABLE, NULL, &u);
				}
			}
			// rpg::forbids = 10;
		}
	}
}

// return value:
//   true: executed ally operation. so next need do some post operation, for example 
bool play_controller::ally_all_ai(bool force)
{
	if (all_ai_allied_) {
		return false;
	}
	if (network::nconnections() > 0) {
		// in network state, don't ally all ai.
		return false;
	}

	std::vector<team>& teams = *resources::teams;
	int human_team_index = -1;
	int allied_teams = 0;

	std::vector<artifical*> cities;
	for (size_t side = 0; side != teams.size(); side ++) {
		if (teams[side].is_human()) {
			human_team_index = side;
		}
		std::vector<artifical*>& side_cities = teams[side].holded_cities();
		for (std::vector<artifical*>::iterator itor = side_cities.begin(); itor != side_cities.end(); ++ itor) {
			artifical* city = *itor;
			cities.push_back(city);
		}
	}
	if (human_team_index < 0) {
		return false;
	}
	if (force || teams[human_team_index].holded_cities().size() * 3 >= cities.size()) {
		// all ai sides concluded treaty of alliance
		for (int from_side = 0; from_side != teams.size(); from_side ++) {
			if (from_side == human_team_index || !teams[from_side].holded_cities().size() || !teams[human_team_index].is_enemy(from_side + 1)) {
				continue;
			}
			for (int to_side = 0; to_side != teams.size(); to_side ++) {
				if (to_side == human_team_index || !teams[to_side].holded_cities().size() || !teams[human_team_index].is_enemy(to_side + 1)) {
					continue;
				}
				if (teams[from_side].is_enemy(to_side + 1)) {
					teams[from_side].set_ally(to_side + 1);
				}
			}
			allied_teams ++;
		}
		all_ai_allied_ = true;
		return allied_teams >= 2? true: false;
	}
	return false;
}

artifical* play_controller::decide_ai_capital() const
{
	artifical* capital_of_ai = NULL;

	int score, best_score = -1;
	std::vector<artifical*> ai_holded_cities;
	for (size_t side = 0; side != teams_.size(); side ++) {
		if (teams_[side].is_human()) {
			continue;
		}
		const std::vector<artifical*>& side_cities = teams_[side].holded_cities();
		for (std::vector<artifical*>::const_iterator itor = side_cities.begin(); itor != side_cities.end(); ++ itor) {
			artifical* city = *itor;
			ai_holded_cities.push_back(city);
			score = city->hitpoints() + (city->reside_troops().size() + city->field_troops().size() + city->fresh_heros().size() + city->finish_heros().size()) * 10;
			if (score > best_score) {
				best_score = score;
				capital_of_ai = city;
			}
		}
	}
	return capital_of_ai;
}

int play_controller::human_players()
{
	return human_players_;
}