/* $Id: playsingle_controller.cpp 47582 2010-11-16 23:28:08Z ai0867 $ */

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
 *  Logic for single-player game.
 */

#include "playsingle_controller.hpp"

#include "actions.hpp"
#include "ai/manager.hpp"
#include "ai/game_info.hpp"
#include "dialogs.hpp"
#include "foreach.hpp"
#include "game_end_exceptions.hpp"
#include "game_events.hpp"
#include "game_preferences.hpp"
#include "gettext.hpp"
#include "gui/dialogs/transient_message.hpp"
#include "gui/dialogs/story_screen.hpp"
#include "log.hpp"
#include "map_label.hpp"
#include "marked-up_text.hpp"
#include "playturn.hpp"
#include "resources.hpp"
#include "savegame.hpp"
#include "sound.hpp"
#include "formula_string_utils.hpp"
#include "events.hpp"
#include "save_blocker.hpp"
#include "soundsource.hpp"
#include "storyscreen/interface.hpp"
#include "util.hpp"
#include "unit_display.hpp"
#include "artifical.hpp"

static lg::log_domain log_engine("engine");
#define ERR_NG LOG_STREAM(err, log_engine)
#define LOG_NG LOG_STREAM(info, log_engine)

uint32_t total_draw, total_analyzing, total_recruit, total_combat, total_build, total_move, total_a_star_search, total_diplomatism;
int total_draws, total_a_star_searches;

playsingle_controller::playsingle_controller(const config& level,
		game_state& state_of_game, hero_map& heros, hero_map& heros_start, 
		card_map& cards,
		const int ticks, const int num_turns,
		const config& game_config, CVideo& video, bool skip_replay) :
	play_controller(level, state_of_game, heros, heros_start, cards, 
		ticks, num_turns, game_config, video, skip_replay, false),
	cursor_setter(cursor::NORMAL),
	data_backlog_(),
	textbox_info_(),
	replay_sender_(recorder),
	end_turn_(false),
	player_type_changed_(false),
	turn_over_(false),
	skip_next_turn_(false),
	level_result_(NONE)
{
	// game may need to start in linger mode
	if (state_of_game.classification().completion == "victory" || state_of_game.classification().completion == "defeat")
	{
		LOG_NG << "Setting linger mode.\n";
		browse_ = linger_ = true;
	}

	ai::game_info ai_info(*gui_, map_, units_, heros_, teams_, tod_manager_, gamestate_);
	ai::manager::set_ai_info(ai_info);
	ai::manager::add_observer(this) ;
}

playsingle_controller::~playsingle_controller()
{
	ai::manager::remove_observer(this) ;
	ai::manager::clear_ais() ;
}

void playsingle_controller::init_gui()
{
	uint32_t start = SDL_GetTicks();
	LOG_NG << "Initializing GUI... " << (SDL_GetTicks() - ticks_) << "\n";
	play_controller::init_gui();
	
	uint32_t end_parent_init_gui = SDL_GetTicks();
	posix_print("playsingle_controller::init_gui, play_controller::init_gui(), used time: %u ms\n", end_parent_init_gui - start);
	
	if(first_human_team_ != -1) {
		gui_->scroll_to_tile(map_.starting_position(first_human_team_ + 1), game_display::WARP);
	}
	gui_->scroll_to_tile(map_.starting_position(1), game_display::WARP);
	
	uint32_t end_scroll_to_tile = SDL_GetTicks();
	posix_print("playsingle_controller::init_gui, scroll_to_tile, used time: %u ms\n", end_scroll_to_tile - end_parent_init_gui);

	update_locker lock_display(gui_->video(),recorder.is_skipping());
	events::raise_draw_event();
	gui_->draw();
	for(std::vector<team>::iterator t = teams_.begin(); t != teams_.end(); ++t) {
		::clear_shroud(t - teams_.begin() + 1);
	}

	uint32_t end_draw = SDL_GetTicks();
	posix_print("playsingle_controller::init_gui, gui_->draw, used time: %u ms\n", end_draw - end_scroll_to_tile);
}

void playsingle_controller::recruit()
{
	if (!browse_) {
		artifical* city = units_.city_from_loc(mouse_handler_.get_selected_hex());
		do_recruit(city, teams_[player_number_ -1].cost_exponent(), false);
	}
}

void playsingle_controller::move()
{
	if (!browse_)
		menu_handler_.move(browse_, player_number_, mouse_handler_.get_selected_hex());
}

void playsingle_controller::armory()
{
	if (!browse_ && units_.count(mouse_handler_.get_selected_hex())) {
		menu_handler_.armory(mouse_handler_, player_number_, *unit_2_artifical(&*units_.find(mouse_handler_.get_selected_hex())));
	}
}

void playsingle_controller::build(const std::string& type)
{
	// 执行“建造”命令时出现非法，此处提供给menu_handler.build的参数builder就是个无效对象，
	// 也就是units_.find(mouse_handler_.get_selected_hex())->second得到是个无效对象！！
	// units_.find(mouse_handler_.get_selected_hex())->second为何无效？看来只能是mouse_handler_.get_selected_hex()指向的是一个没有站了单位的格子。
	// 为何会出现这种情况呢，——在没有选中单位的情况下仍然让显示建造菜单？——查下来是程序上一个bug，
	// 1. 鼠标左键选中一个部队，底下出现“部队可操作”菜单，再进入“可建造建筑物”菜单
	// 2. 这时按下部队快捷列表！！鼠标出现未选中任何部队，但“可建造建筑物”菜单不消失。此次选择一种建筑物，导致非法。
	// ——这个问题说来是“可建造建筑物”菜单不消失造成的，但这里为安全考虑，也作一下是否有效判断。
	if (!browse_ && units_.count(mouse_handler_.get_selected_hex())) {
		menu_handler_.build(type, mouse_handler_, *units_.find(mouse_handler_.get_selected_hex()));
	}
}

void playsingle_controller::demolish()
{
	unit_map::iterator u_itor = find_visible_unit(mouse_handler_.get_selected_hex(), teams_[player_number_ -1]);
	if (!browse_ && u_itor.valid()) {
		menu_handler_.demolish(mouse_handler_, *unit_2_artifical(&*u_itor));
	}
}

void playsingle_controller::play_card()
{
	if (!browse_)
		menu_handler_.play_card(browse_, player_number_);
}

void playsingle_controller::expedite()
{
	if (!browse_)
		menu_handler_.expedite(player_number_, mouse_handler_.get_selected_hex());
}

void playsingle_controller::toggle_shroud_updates(){
	menu_handler_.toggle_shroud_updates(gui_->viewing_team()+1);
}

void playsingle_controller::update_shroud_now(){
	menu_handler_.update_shroud_now(gui_->viewing_team()+1);
}

void playsingle_controller::end_turn(){
	if (linger_)
		end_turn_ = true;
	else if (!browse_){
		browse_ = true;
		end_turn_ = menu_handler_.end_turn(player_number_);
		browse_ = end_turn_;
	}
}

void playsingle_controller::force_end_turn(){
	skip_next_turn_ = true;
	end_turn_ = true;
}

void playsingle_controller::check_end_level()
{
	if (level_result_ == NONE || linger_)
	{
		team &t = teams_[gui_->viewing_team()];
		if (!browse_ && t.objectives_changed()) {
			dialogs::show_objectives(level_, t.objectives());
			t.reset_objectives_changed();
		}
		return;
	}
	throw end_level_exception(level_result_);
}

void playsingle_controller::change_side(){
	menu_handler_.change_side(mouse_handler_);
}

void playsingle_controller::label_terrain(bool team_only){
	menu_handler_.label_terrain(mouse_handler_, team_only);
}

void playsingle_controller::clear_labels(){
	menu_handler_.clear_labels();
}

void playsingle_controller::clear_messages(){
	menu_handler_.clear_messages();
}

void playsingle_controller::report_victory(
	std::ostringstream &report, int player_gold, int remaining_gold,
	int finishing_bonus_per_turn, int turns_left, int finishing_bonus)
{
	end_level_data &end_level = get_end_level_data();
	report << _("Remaining gold: ")
		   << remaining_gold << "\n";
	if(end_level.gold_bonus) {
		if (turns_left > -1) {
			report << _("Early finish bonus: ")
				   << finishing_bonus_per_turn
				   << " " << _("per turn") << "\n"
				   << font::LARGE_TEXT << _("Turns finished early: ")
				   << turns_left << "\n"
				   << _("Bonus: ")
				   << finishing_bonus << "\n";
		}
		report << _("Gold: ")
		       << (remaining_gold + finishing_bonus);
	}
	if (remaining_gold > 0) {
		report << '\n' << _("Carry over percentage: ") << end_level.carryover_percentage;
	}
	if(end_level.carryover_add) {
		report << "\n" << font::LARGE_TEXT << _("Bonus Gold: ") << player_gold;
	} else {
		report << "\n" << font::LARGE_TEXT << _("Retained Gold: ") << player_gold;
	}

	std::string goldmsg;
	utils::string_map symbols;

	symbols["gold"] = lexical_cast_default<std::string>(player_gold);

	// Note that both strings are the same in english, but some languages will
	// want to translate them differently.
	if(end_level.carryover_add) {
		if(player_gold > 0) {
			goldmsg = vngettext(
					"You will start the next scenario with $gold "
					"on top of the defined minimum starting gold.",
					"You will start the next scenario with $gold "
					"on top of the defined minimum starting gold.",
					player_gold, symbols);

		} else {
			goldmsg = vngettext(
					"You will start the next scenario with "
					"the defined minimum starting gold.",
					"You will start the next scenario with "
					"the defined minimum starting gold.",
					player_gold, symbols);
		}
	} else {
		goldmsg = vngettext(
			"You will start the next scenario with $gold "
			"or its defined minimum starting gold, "
			"whichever is higher.",
			"You will start the next scenario with $gold "
			"or its defined minimum starting gold, "
			"whichever is higher.",
			player_gold, symbols);
	}

	// xgettext:no-c-format
	report << '\n' << goldmsg;
}

LEVEL_RESULT playsingle_controller::play_scenario(
	const config::const_child_itors &story,
	bool skip_replay)
{
	uint32_t start = SDL_GetTicks();
	// level_
	//   save game: [snapshort] in save file
	//   new game: [scenario] in <scenarios>.cfg

	// Start music.
	foreach (const config &m, level_.child_range("music")) {
		sound::play_music_config(m);
	}
	sound::commit_music_changes();

	if(!skip_replay) {
		// show_story(*gui_, level_["name"], story);
		// gui2::show_story(*gui_, level_["name"], story);
	}
	gui_->labels().read(level_);

	// Read sound sources
	assert(soundsources_manager_ != NULL);
	foreach (const config &s, level_.child_range("sound_source")) {
		soundsource::sourcespec spec(s);
		soundsources_manager_->add(spec);
	}

	uint32_t end_sound_sources = SDL_GetTicks();
	posix_print("playsingle_controller::play_scenario, sound sources, used time: %u ms\n", end_sound_sources - start);
				
	set_victory_when_enemies_defeated(level_["victory_when_enemies_defeated"].to_bool(true));
	end_level_data &end_level = get_end_level_data();
	end_level.carryover_percentage = level_["carryover_percentage"].to_int(game_config::gold_carryover_percentage);
	end_level.carryover_add = level_["carryover_add"].to_bool();

	try {

		uint32_t end_entering_try = SDL_GetTicks();
		posix_print("playsingle_controller::play_scenario, entering try, used time: %u ms\n", end_entering_try - end_sound_sources);
		
		fire_prestart(!loading_game_);
		uint32_t end_fire_prestart = SDL_GetTicks();
		posix_print("playsingle_controller::play_scenario, fire_prestrt, used time: %u ms\n", end_fire_prestart - end_entering_try);
		
		init_gui();

		fire_start(!loading_game_);
		gui_->recalculate_minimap();

		// replaying_ = (recorder.at_end() == false);

		// Initialize countdown clock.
		std::vector<team>::iterator t;
		for(t = teams_.begin(); t != teams_.end(); ++t) {
			if (gamestate_.mp_settings().mp_countdown && !loading_game_ ){
				t->set_countdown_time(1000 * gamestate_.mp_settings().mp_countdown_init_time);
			}
		}

		// if we loaded a save file in linger mode, skip to it.
		if (linger_) {
			//determine the bonus gold handling for this scenario
			const config end_cfg = level_.child_or_empty("endlevel");
			end_level.carryover_percentage = level_["carryover_percentage"].to_int(game_config::gold_carryover_percentage);
			end_level.carryover_add = level_["carr`yover_add"].to_bool();
			end_level.gold_bonus = end_cfg["bonus"].to_bool(true);
			end_level.carryover_report = false;
			throw end_level_exception(SKIP_TO_LINGER);
		}

		// Avoid autosaving after loading, but still
		// allow the first turn to have an autosave.
		bool save = !loading_game_;
		for(; ; first_player_ = 1) {
			play_turn(save);
			save = true;
		} //end for loop

	} catch(game::load_game_exception& lge) {
		// Loading a new game is effectively a quit.
		//
		if (lge.game != "") {
			gamestate_ = game_state();
		}
		throw lge;
	} catch (end_level_exception &end_level_exn) {
		LEVEL_RESULT end_level_result = end_level_exn.result;
		if (!end_level.custom_endlevel_music.empty()) {
			if (end_level_result == DEFEAT) {
				set_defeat_music_list(end_level.custom_endlevel_music);
			} else {
				set_victory_music_list(end_level.custom_endlevel_music);
			}
		}

		if (teams_.empty())
		{
			//store persistent teams
			gamestate_.snapshot = config();

			return VICTORY; // this is probably only a story scenario, i.e. has its endlevel in the prestart event
		}
		const bool obs = is_observer();
		if (game_config::exit_at_end) {
			exit(0);
		}
		if (end_level_result == DEFEAT || end_level_result == VICTORY)
		{
			gamestate_.classification().completion = (end_level_exn.result == VICTORY) ? "victory" : "defeat";
			// If we're a player, and the result is victory/defeat, then send
			// a message to notify the server of the reason for the game ending.
			if (!obs) {
				config cfg;
				config& info = cfg.add_child("info");
				info["type"] = "termination";
				info["condition"] = "game over";
				info["result"] = gamestate_.classification().completion;
				network::send_data(cfg, 0);
			} else {
				gui2::show_transient_message(gui_->video(),_("Game Over"),
									_("The game is over."));
				return OBSERVER_END;
			}
		}

		if (end_level_result == QUIT) {
			return QUIT;
		}
		else if (end_level_result == DEFEAT)
		{
			gamestate_.classification().completion = "defeat";
			game_events::fire("defeat");

			if (!obs) {
				const std::string& defeat_music = select_defeat_music();
				if(defeat_music.empty() != true)
					sound::play_music_once(defeat_music);

				return DEFEAT;
			} else {
				return QUIT;
			}
		}
		else if (end_level_result == VICTORY)
		{
			gamestate_.classification().completion =
				!end_level.linger_mode ? "running" : "victory";
			game_events::fire("victory");

			//
			// Play victory music once all victory events
			// are finished, if we aren't observers.
			//
			// Some scenario authors may use 'continue'
			// result for something that is not story-wise
			// a victory, so let them use [music] tags
			// instead should they want special music.
			//
			if (!obs && end_level.linger_mode) {
				const std::string& victory_music = select_victory_music();
				if(victory_music.empty() != true)
					sound::play_music_once(victory_music);
			}

			// Add all the units that survived the scenario.
			LOG_NG << "Add units that survived the scenario to the recall list.\n";
			//store all units that survived (recall list for the next scenario) in snapshot
			gamestate_.snapshot = config();
			//store gold and report victory
			store_gold(obs);
			return VICTORY;
		}
		else if (end_level_result == SKIP_TO_LINGER)
		{
			LOG_NG << "resuming from loaded linger state...\n";
			//as carryover information is stored in the snapshot, we have to re-store it after loading a linger state
			gamestate_.snapshot = config();
			store_gold();
			return VICTORY;
		}
	} // end catch
	catch(network::error& e) {
		bool disconnect = false;
		if(e.socket) {
			e.disconnect();
			disconnect = true;
		}

		config snapshot;
		to_config(snapshot);
		savegame::game_savegame save(heros_, gamestate_, *gui_, snapshot);
		save.save_game_interactive(gui_->video(), _("A network disconnection has occurred, and the game cannot continue. Do you want to save the game?"), gui::YES_NO);
		if(disconnect) {
			throw network::error();
		} else {
			return QUIT;
		}
	}

	return QUIT;
}

// @save: It isn't mean this is loaded game. it mean whether execute auto-save. 
//        false: this is first turn of loaded game, true: this isn't first turn or this isn't loaded game.
void playsingle_controller::play_turn(bool save)
{
	gui_->new_turn();
	gui_->invalidate_game_status();
	events::raise_draw_event();

	if (non_interactive())
		std::cout << "Turn " << turn() << ":" << std::endl;


	for (player_number_ = first_player_; player_number_ <= int(teams_.size()); ++player_number_) {
		unit_display::player_number_ = player_number_;
		// If a side is empty skip over it.
		if (current_team().is_empty()) continue;
		try {
			save_blocker blocker;
			init_side(player_number_ - 1, false, save || (player_number_ != first_player_));
		} catch (end_turn_exception) {
			if (current_team().is_network() == false) {
				turn_info turn_data(player_number_, replay_sender_, undo_stack_);
				recorder.end_turn();
				turn_data.sync_network();
			}
			continue;
		}

		/* if (replaying_) {
			LOG_NG << "doing replay " << player_number_ << "\n";
			replaying_ = ::do_replay(player_number_);
			LOG_NG << "result of replay: " << (replaying_?"true":"false") << "\n";
		} else */ {
			// If a side is dead end the turn.
			if (current_team().is_human() && side_units(player_number_) == 0) {
				turn_info turn_data(player_number_, replay_sender_, undo_stack_);
				recorder.end_turn();
				turn_data.sync_network();
				continue;
			}

			play_side(player_number_, save);
		}

		finish_side_turn();

		if(non_interactive()) {
			std::cout << " Player " << player_number_ << ": " <<
				current_team().villages().size() << " Villages" <<
				std::endl;
		}

		check_victory();

		//if loading a savegame, network turns might not have reset this yet
		loading_game_ = false;
	}

	// Time has run out
	check_time_over();

	finish_turn();
}

void playsingle_controller::play_side(const unsigned int team_index, bool save)
{
	//check for team-specific items in the scenario
	gui_->parse_team_overlays();

	//flag used when we fallback from ai and give temporarily control to human
	do {
		// Although this flag is used only in this method,
		// it has to be a class member since derived classes
		// rely on it
		player_type_changed_ = false;
		if (!skip_next_turn_)
			end_turn_ = false;


		statistics::reset_turn_stats(teams_[team_index - 1].save_id());

		if (current_team().is_human()) {
			try {
				if (save) {
					play_ai_turn();
				}
				before_human_turn(save);
				play_human_turn();
				after_human_turn();
			} catch(end_turn_exception& end_turn) {
				if (end_turn.redo == team_index) {
					player_type_changed_ = true;
					// If new controller is not human,
					// reset gui to prev human one
					if (!teams_[team_index-1].is_human()) {
						browse_ = true;
						int t = find_human_team_before(team_index);
						if (t > 0) {
							gui_->set_team(t-1);
							gui_->recalculate_minimap();
							gui_->invalidate_all();
							gui_->draw(true,true);
						}
					}
				}
			}
		} else if(current_team().is_ai()) {
			play_ai_turn();
		}
	} while (player_type_changed_);
	// Keep looping if the type of a team (human/ai/networked)
	// has changed mid-turn
	skip_next_turn_ = false;
}

void playsingle_controller::before_human_turn(bool save)
{
	browse_ = false;
	linger_ = false;

	gui_->refresh_access_troops(player_number_ - 1);
	ai::manager::raise_turn_started();

	if (save) {
		// uint32_t start = SDL_GetTicks();
		config snapshot;
		to_config(snapshot);
		// uint32_t mid1 = SDL_GetTicks();
		// posix_print("(mid1)used time: %u ms\n", mid1 - start);
		savegame::autosave_savegame save(heros_, gamestate_, *gui_, snapshot);
		// uint32_t mid2 = SDL_GetTicks();
		// posix_print("(mid2)used time: %u ms\n", mid2 - mid1);
		save.autosave(game_config::disable_autosave, preferences::autosavemax(), preferences::INFINITE_AUTO_SAVES);
		// uint32_t mid3 = SDL_GetTicks();
		// posix_print("(mid3)used time: %u ms\n", mid3 - mid2);
	}

	if (preferences::turn_bell()) {
		sound::play_bell(game_config::sounds::turn_bell);
	}
	// button: undo
	undo_stack_.clear();
	if (teams_[player_number_-1].uses_shroud() || teams_[player_number_-1].uses_fog()) {
		gui_->hide_menu("undo", true);
	} else {
		gui_->hide_menu("undo", false);
		gui_->enable_menu("undo", false);
	}
	// button: endturn
	gui_->enable_menu("play_card", true);
	gui_->enable_menu("endturn", true);
/*
	if (rpg::stratum == hero_stratum_leader) {
		if (card_mode_) {
			execute_card_uh(turn(), player_number_);
		}
		execute_guard_attack(player_number_);
	}
*/	
	// card
	refresh_card_button(current_team(), *gui_);
	// if (!network::nconnections()) {
	//	show_context_menu(NULL, *gui_);
	// } else {
		// Multiplayer mod, there is context-button when network player playing.
		// In order to display correct, need hide "before" button.
		gui_->goto_main_context_menu();
	// }
}

void playsingle_controller::show_turn_dialog(){
	if(preferences::turn_dialog() && (level_result_ == NONE) ) {
		std::string message = _("It is now $name|'s turn");
		utils::string_map symbols;
		symbols["name"] = teams_[player_number_ - 1].current_player();
		message = utils::interpolate_variables_into_string(message, &symbols);
		gui2::show_transient_message(gui_->video(), "", message);
	}
}

void playsingle_controller::execute_gotos()
{
	menu_handler_.execute_gotos(mouse_handler_, player_number_);
}

void playsingle_controller::execute_card_uh(size_t turn, int team_index)
{
	if (turn == 1) {
		int sum = game_config::start_cards - ((team_index == 1)? 1: 0);
		team& current_team = teams_[team_index - 1];
		for (int index = 0; index < sum; index ++) {
			if (!current_team.add_card(CARDS_INVALID_NUMBER)) break;
		}
		if (current_team.is_human()) {
			// card
			refresh_card_button(current_team, *gui_);
		}
	}
}

void playsingle_controller::execute_card_bh(size_t turn, int team_index)
{
	team& current_team = teams_[team_index - 1];

	int sum = game_config::cards_per_turn + ((turn == 1 && team_index == 1)? 1: 0);
	for (int index = 0; index < sum; index ++) {
		if (!current_team.add_card(CARDS_INVALID_NUMBER)) break;
	}
	if (current_team.is_human()) {
		// card
		refresh_card_button(current_team, *gui_);
	}
}

void playsingle_controller::execute_guard_attack(int team_index)
{
	// art maybe advance, tiles must be copy
	static map_location tiles[24];
	team& current_team = teams_[team_index - 1];
	battle_context* bc = NULL;
	const std::vector<artifical*> holded_cities = current_team.holded_cities();
	for (std::vector<artifical*>::const_iterator itor = holded_cities.begin(); itor != holded_cities.end(); ++ itor) {
		artifical& city = **itor;

		std::vector<artifical*> arts = city.field_arts();
		for (std::vector<artifical*>::iterator i2 = arts.begin(); i2 != arts.end(); ++ i2) {
			unit* art = *i2;

			int weapon_index = art->guard_attack();
			if (weapon_index < 0) {
				continue;
			}
			std::vector<attack_type>& attacks = art->attacks();
			const attack_type& weapon = attacks[weapon_index];
			const std::string& range = weapon.range();
			size_t adjacent_size;

			if (range == "melee") {
				// range: 1
				adjacent_size = art->adjacent_size_;
				std::copy(art->adjacent_, art->adjacent_ + adjacent_size, tiles);
			} else if (range == "ranged") {
				// range: 2
				adjacent_size = art->adjacent_size_2_;
				std::copy(art->adjacent_2_, art->adjacent_2_ + adjacent_size, tiles);
			} else {
				// other to range: 3
				adjacent_size = art->adjacent_size_3_;
				std::copy(art->adjacent_3_, art->adjacent_3_ + adjacent_size, tiles);
			}
			
			for (size_t j = 0; j != adjacent_size; ++j) {
				unit_map::iterator opp = find_visible_unit(tiles[j], current_team);
				if (!opp.valid() || !current_team.is_enemy(opp->side())) {
					continue;
				}
				if (bc) {
					*bc = battle_context(units_, *art, *opp, weapon_index, -1, 0.5, NULL);
				} else {
					bc = new battle_context(units_, *art, *opp, weapon_index, -1, 0.5, NULL);
				}
				std::pair<map_location, map_location> to_locs = attack_enemy(art, &*opp, bc->get_attacker_stats().attack_num, bc->get_defender_stats().attack_num);
				// attacker maybe advance. it is necessary that reget art.
				unit_map::iterator u = units_.find(to_locs.first);
				if (u == units_.end()) {
					break;
				} else {
					art = &*u;
					art->set_attacks(art->attacks_total());
				}
			}

		}
	}
	if (bc) {
		delete bc;
	}
}

void playsingle_controller::play_human_turn() {
	show_turn_dialog();
	execute_gotos();

	while(!end_turn_) {
		play_slice();
		check_end_level();
		gui_->draw();
	}
}
struct set_completion
{
	set_completion(game_state& state, const std::string& completion) :
		state_(state), completion_(completion)
	{
	}
	~set_completion()
	{
		state_.classification().completion = completion_;
	}
	private:
	game_state& state_;
	const std::string completion_;
};

void playsingle_controller::linger()
{
	LOG_NG << "beginning end-of-scenario linger\n";
	browse_ = true;
	linger_ = true;

	// If we need to set the status depending on the completion state
	// the key to it is here.
	gui_->set_game_mode(game_display::LINGER_SP);

	// this is actually for after linger mode is over -- we don't
	// want to stay stuck in linger state when the *next* scenario
	// is over.
	set_completion setter(gamestate_,"running");

	// change the end-turn button text to its alternate label
	gui_->get_theme().refresh_title2("button-endturn", "title2");
	gui_->invalidate_theme();
	gui_->redraw_everything();

	// End all unit moves
	for (unit_map::iterator u = units_.begin(); u != units_.end(); ++u) {
		u->set_user_end_turn(true);
	}
	try {
		// Same logic as single-player human turn, but
		// *not* the same as multiplayer human turn.
		gui_->enable_menu("endturn", true);
		while(!end_turn_) {
			// Reset the team number to make sure we're the right team.
			player_number_ = first_player_;
			play_slice();
			gui_->draw();
		}
	} catch(game::load_game_exception& lge) {
		// Loading a new game is effectively a quit.
		if (lge.game != "") {
			gamestate_ = game_state();
		}
		throw lge;
	}

	// revert the end-turn button text to its normal label
	gui_->get_theme().refresh_title2("button-endturn", "title");
	gui_->invalidate_theme();
	gui_->redraw_everything();
	gui_->set_game_mode(game_display::RUNNING);

	LOG_NG << "ending end-of-scenario linger\n";
}

void playsingle_controller::end_turn_record()
{
	if (!turn_over_)
	{
		turn_over_ = true;
		recorder.end_turn();
	}
}
void playsingle_controller::end_turn_record_unlock()
{
	turn_over_ = false;
}



void playsingle_controller::after_human_turn()
{
	// clear access troops
	gui_->refresh_access_troops(player_number_ - 1, game_display::REFRESH_CLEAR);
	// hide context-menu
	gui_->hide_context_menu(NULL, true);

	browse_ = true;
	end_turn_record();
	end_turn_record_unlock();
	menu_handler_.clear_undo_stack(player_number_);

	if(teams_[player_number_-1].uses_fog()) {
		// needed because currently fog is only recalculated when a hex is /un/covered
		recalculate_fog(player_number_);
	}

	gui_->set_route(NULL);
	gui_->unhighlight_reach();

	// add cards if in card mode
	if (card_mode_) {
		execute_card_bh(turn(), player_number_);
	}

}

void playsingle_controller::play_ai_turn()
{
	uint32_t start = SDL_GetTicks();
	total_draw = 0;
	total_draws = 0;
	total_analyzing = 0;
	total_recruit = 0;
	total_combat = 0;
	total_build = 0;
	total_move = 0;
	total_diplomatism = 0;

	if (card_mode_) {
		execute_card_uh(turn(), player_number_);
	}
	execute_guard_attack(player_number_);

	gui_->enable_menu("play_card", false);
	gui_->enable_menu("undo", false);
	gui_->enable_menu("endturn", false);
	browse_ = true;
	gui_->recalculate_minimap();

	const cursor::setter cursor_setter(cursor::WAIT);

	turn_info turn_data(player_number_, replay_sender_, undo_stack_);

	uint32_t before = SDL_GetTicks();
	try {
		ai::manager::play_turn(player_number_);
	} catch (end_turn_exception&) {
	}
	uint32_t after = SDL_GetTicks();

	if (!teams_[player_number_ - 1].is_human()) {
		recorder.end_turn();
	}
	turn_data.sync_network();

	if (!teams_[player_number_ - 1].is_human()) {
		gui_->recalculate_minimap();
		::clear_shroud(player_number_);
		gui_->invalidate_unit();
		gui_->invalidate_game_status();
		gui_->invalidate_all();
		
		gui_->draw();

		if (card_mode_) {
			execute_card_bh(turn(), player_number_);
		}
	}

	uint32_t stop = SDL_GetTicks();
	posix_print("#%i, play_ai_turn %u ms, (draw: %u(%i), analyzing: %u), [%u](%u+[%u]+%u)(recruit: %u, combat: %u, build: %u, move: %u(%u)(%i), diplomatism: %u)\n", 
		player_number_, stop - start, total_draw, total_draws, total_analyzing, 
		before - start + total_recruit + total_combat + total_build + total_move + total_diplomatism + stop - after,
		before - start, total_recruit + total_combat + total_build + total_move + total_diplomatism, stop - after,
		total_recruit, total_combat, total_build, total_move, total_a_star_search, total_a_star_searches, total_diplomatism);
}

void playsingle_controller::handle_generic_event(const std::string& name)
{
	if (name == "ai_user_interact"){
		play_slice(false);
	}
	if (end_turn_){
		throw end_turn_exception();
	}
}

void playsingle_controller::check_time_over()
{
	bool b = tod_manager_.next_turn();
	if(!b) {

		LOG_NG << "firing time over event...\n";
		game_events::fire("time over");
		LOG_NG << "done firing time over event...\n";
		//if turns are added while handling 'time over' event
		if (tod_manager_.is_time_left()) {
			return;
		}

		if(non_interactive()) {
			std::cout << "time over (draw)\n";
		}

		check_end_level();
		throw end_level_exception(DEFEAT);
	}
}

void playsingle_controller::store_gold(bool obs)
{
	bool has_next_scenario = !gamestate_.classification().next_scenario.empty() &&
		gamestate_.classification().next_scenario != "null";

	std::ostringstream report;
	std::string title;

	if (obs) {
		title = _("Scenario Report");
	} else {
		persist_.end_transaction();
		title = _("Victory");
		report << font::LARGE_TEXT << _("You have emerged victorious!") << "\n\n";
	}

	int persistent_teams = 0;
	foreach (const team &t, teams_) {
		if (t.persistent()) ++persistent_teams;
	}

	end_level_data &end_level = get_end_level_data();

	if (persistent_teams > 0 && (has_next_scenario ||
		gamestate_.classification().campaign_type == "test"))
	{
		int finishing_bonus_per_turn =
			map_.villages().size() * game_config::village_income +
			game_config::base_income;
		int turns_left = std::max<int>(0, tod_manager_.number_of_turns() - turn());
		int finishing_bonus = (end_level.gold_bonus && turns_left > -1) ?
			finishing_bonus_per_turn * turns_left : 0;
		foreach (const team &t, teams_)
		{
			if (!t.persistent()) continue;
			int carryover_gold = div100rounded((t.gold() + finishing_bonus) * end_level.carryover_percentage);
			config::child_itors side_range = gamestate_.snapshot.child_range("side");
			config::child_iterator side_it = side_range.first;

			// Check if this side already exists in the snapshot.
			while (side_it != side_range.second) {
				if ((*side_it)["save_id"] == t.save_id()) {
					(*side_it)["gold"] = str_cast<int>(carryover_gold);
					(*side_it)["gold_add"] = end_level.carryover_add;
					///@deprecated 1.9.2 'colour' also written in save
					(*side_it)["colour"] = t.color();
					(*side_it)["color"] = t.color();
					(*side_it)["current_player"] = t.current_player();
					(*side_it)["name"] = t.name();
					break;
				}
				++side_it;
			}

			// If it doesn't, add a new child.
			if (side_it == side_range.second) {
				config &new_side = gamestate_.snapshot.add_child("side");
				new_side["save_id"] = t.save_id();
				new_side["gold"] = str_cast<int>(carryover_gold);
				new_side["gold_add"] = end_level.carryover_add;
				new_side["color"] = t.color();
				///@deprecated 1.9.2 'colour' also written in save
				new_side["colour"] = t.color();
				new_side["current_player"] = t.current_player();
				new_side["name"] = t.name();
			}

			// Only show the report for ourselves.
			if (!t.is_human()) continue;

			if (persistent_teams > 1) {
				report << "\n" << font::LARGE_TEXT << t.current_player() << "\n";
			}

			report_victory(report, carryover_gold, t.gold(), finishing_bonus_per_turn, turns_left, finishing_bonus);
		}
	}

	if (end_level.carryover_report) {
		gui2::show_transient_message(gui_->video(), title, report.str(), "", true);
	}
}

bool playsingle_controller::can_execute_command(hotkey::HOTKEY_COMMAND command, int index) const
{
	bool res = true;
	switch (command){
		case hotkey::HOTKEY_PLAY_CARD:
			return !browse_ && !linger_ && !events::commands_disabled;
		case hotkey::HOTKEY_ENDTURN:
			return (!browse_ || linger_) && !events::commands_disabled;

		case hotkey::HOTKEY_UNIT_DETAIL:
			return !events::commands_disabled;

		case hotkey::HOTKEY_DELAY_SHROUD:
			return false; 
		case hotkey::HOTKEY_UPDATE_SHROUD:
			return !linger_
				&& player_number_ == gui_->viewing_side()
				&& !events::commands_disabled
				&& teams_[gui_->viewing_team()].auto_shroud_updates() == false;

		// Commands we can only do if in debug mode
		case hotkey::HOTKEY_CHANGE_SIDE:
			return !events::commands_disabled && game_config::debug && map_.on_board(mouse_handler_.get_last_hex());

		case hotkey::HOTKEY_CLEAR_LABELS:
			res = !is_observer();
			break;
		case hotkey::HOTKEY_LABEL_TEAM_TERRAIN:
		case hotkey::HOTKEY_LABEL_TERRAIN: {
			const terrain_label *label = resources::screen->labels().get_label(mouse_handler_.get_last_hex());
			res = !events::commands_disabled && map_.on_board(mouse_handler_.get_last_hex())
				&& !gui_->shrouded(mouse_handler_.get_last_hex())
				&& !is_observer()
				&& (!label || !label->immutable());
			break;
		}

		default: 
			return play_controller::can_execute_command(command, index);
	}
	return res;
}
