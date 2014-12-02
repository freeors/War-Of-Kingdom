/* $Id: playmp_controller.cpp 47582 2010-11-16 23:28:08Z ai0867 $ */
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

#include "playmp_controller.hpp"

#include "dialogs.hpp"
#include "game_end_exceptions.hpp"
#include "gettext.hpp"
#include "log.hpp"
#include "preferences.hpp"
#include "resources.hpp"
#include "savegame.hpp"
#include "sound.hpp"
#include "formula_string_utils.hpp"
#include "wml_exception.hpp"

#include <boost/foreach.hpp>

static lg::log_domain log_engine("engine");
#define LOG_NG LOG_STREAM(info, log_engine)

unsigned int playmp_controller::replay_last_turn_ = 0;

playmp_controller::playmp_controller(const config& level,
		game_state& state_of_game, hero_map& heros, hero_map& heros_start, 
		card_map& cards,
		const int ticks, const int num_turns, const config& game_config, CVideo& video,
		bool skip_replay, bool is_host) :
	playsingle_controller(level, state_of_game, heros, heros_start, cards, 
		ticks, num_turns, game_config, video, skip_replay),
	turn_data_(NULL),
	received_data_cfg_(),
	beep_warning_time_(0),
	network_processing_stopped_(false)
{
	is_host_ = is_host;
	// We stop quick replay if play isn't yet past turn 1
	if (replay_last_turn_ <= 1) {
		skip_replay_ = false;
	}
}

playmp_controller::~playmp_controller()
{
	//halt and cancel the countdown timer
	if(beep_warning_time_ < 0) {
		sound::stop_bell();
	}
}

void playmp_controller::set_replay_last_turn(unsigned int turn)
{
	 replay_last_turn_ = turn;
}

void playmp_controller::speak()
{
	menu_handler_.speak();
}

void playmp_controller::whisper()
{
	menu_handler_.whisper();
}

void playmp_controller::shout()
{
	menu_handler_.shout();
}

void playmp_controller::start_network()
{
	network_processing_stopped_ = false;
	LOG_NG << "network processing activated again";
}

void playmp_controller::stop_network()
{
	network_processing_stopped_ = true;
	LOG_NG << "network processing stopped";
}

void playmp_controller::play_side()
{
	utils::string_map player;
	player["name"] = current_team().current_player();
	std::string turn_notification_msg = _("$name has taken control");
	turn_notification_msg = utils::interpolate_variables_into_string(turn_notification_msg, &player);
	gui_->send_notification(_("Turn changed"), turn_notification_msg);

	int end_ticks = calculate_end_ticks();

	player_type_changed_ = false;
	while (unit_map::main_ticks < end_ticks && !player_type_changed_) {
		VALIDATE(!unit::actor, "playmp_controller::play_side, unit::actor isn't NULL!");

		if (!skip_next_turn_) {
			end_turn_ = false;
		}

		unit* u = &units_.current_unit();
		if (!tent::turn_based) {
			int past_ticks = u->backward_ticks(u->ticks());
			if (unit_map::main_ticks + past_ticks >= end_ticks) {
				units_.do_escape_ticks_uh(teams_, *gui_, end_ticks - unit_map::main_ticks, false);
				autosave_ticks_ = -1;
				continue;
			}
			units_.do_escape_ticks_uh(teams_, *gui_, past_ticks, true);
		} else {
			if (u->side() < player_number_) {
				unit_map::main_ticks = end_ticks;
				player_number_ = 1;
				continue;
			}
		}
		// if exist spirit feature/technology, second maybe goto first. need reget
		u = &units_.current_unit();

		player_number_ = u->side(); // init_turn_data need player_number_ be valid.
		team& t = teams_[player_number_ - 1];

		bool local = t.is_local();
		bool new_side = false;
		if (local || loading_game_) {
			if (local) {
				init_turn_data();
			}
			new_side = do_prefix_unit(end_ticks, loading_game_, false);
		}

		if (t.is_network()) {
			play_network_turn();

		} else if (actor_can_continue_action(units_, player_number_)) {
			// we can't call playsingle_controller::play_side because
			// we need to catch exception here
			if (t.is_human()) {
				try {
					if (!loading_game_ && unit::actor->human_team_can_ai()) {
						play_ai_turn(turn_data_);
					}
					before_human_turn(new_side);
					play_human_turn();
					after_human_turn();
				} catch (end_turn_exception& end_turn) {
					if (end_turn.redo == player_number_ - 1) {
						player_type_changed_ = true;
						// if new controller is not human,
						// reset gui to prev human one
						if (!teams_[player_number_ - 2].is_human()) {
							browse_ = true;
							int t = find_human_team_before(player_number_ - 1);

							if (t <= 0)
								t = gui_->get_playing_team() + 1;

							gui_->set_team(t-1);
							gui_->recalculate_minimap();
							gui_->invalidate_all();
							gui_->draw(true,true);
						}
					} else {
						after_human_turn();
					}
				}

			} else if (t.is_ai()) {
				play_ai_turn(turn_data_);

			}
		}

		if (local) {
			do_post_unit(false);
			release_turn_data();
		}

		loading_game_ = false;
	}
	//keep looping if the type of a team (human/ai/networked) has changed mid-turn

	skip_next_turn_ = false;
}

void playmp_controller::before_human_turn(bool save)
{
	playsingle_controller::before_human_turn(save);
}

bool playmp_controller::counting_down() {
	return beep_warning_time_ > 0;
}

namespace {
	const int WARNTIME = 20000; //start beeping when 20 seconds are left (20,000ms)
	unsigned timer_refresh = 0;
	const unsigned timer_refresh_rate = 50; // prevents calling SDL_GetTicks() too frequently
}

//make sure we think about countdown even while dialogs are open
bool playmp_controller::handle(tlobby::ttype type, const config& data)
{
	playsingle_controller::handle(type, data);

	if (type == tlobby::t_disconnected && has_network_player()) {
		throw network::error("shut down");
	}
	if (playmp_controller::counting_down()) {
		// if (info.ticks(&timer_refresh, timer_refresh_rate)) {
		//	playmp_controller::think_about_countdown(info.ticks());
		// }
	}
	if (type != tlobby::t_data) {
		return false;
	}
	received_data_cfg_.push_back(data);
	return true;
}

void playmp_controller::reset_countdown()
{
	if (beep_warning_time_ < 0)
		sound::stop_bell();
	beep_warning_time_ = 0;
}


//check if it is time to start playing the timer warning
void playmp_controller::think_about_countdown(int ticks) 
{
	if (ticks >= beep_warning_time_) {
		const bool bell_on = preferences::turn_bell();
		if(bell_on || preferences::sound_on() || preferences::UI_sound_on()) {
			const int loop_ticks = WARNTIME - (ticks - beep_warning_time_);
			const int fadein_ticks = (loop_ticks > WARNTIME / 2) ? loop_ticks - WARNTIME / 2 : 0;
			sound::play_timer(game_config::sounds::timer_bell, loop_ticks, fadein_ticks);
			beep_warning_time_ = -1;
		}
	}
}

namespace {
	struct command_disabled_resetter
	{
		command_disabled_resetter() : val_(events::commands_disabled) {}
		~command_disabled_resetter() { events::commands_disabled = val_; }
	private:
		int val_;
	};
}

void playmp_controller::play_human_turn()
{
	command_disabled_resetter reset_commands;
	int cur_ticks = SDL_GetTicks();
	show_turn_dialog();
	execute_gotos();

	bool auto_end_turn = can_auto_end_turn(true);
	if (!auto_end_turn) {
		if (!unit::actor->is_city() && unit::actor->task() == unit::TASK_NONE) {
			gui_->scroll_to_tile(unit::actor->get_location(), game_display::ONSCREEN, true, true);
		}
	} else {
		return;
	}

	if ((!linger_) || (is_host_)) {
		gui_->enable_menu("endturn", true);
	}

	while (!end_turn_ && !auto_end_turn) {
		try {
			play_slice();
			check_end_level();

			std::deque<config> backlog;
			std::vector<config> cfgs = received_data_cfg_;
			received_data_cfg_.clear();
			for (std::vector<config>::const_iterator it = cfgs.begin(); it != cfgs.end(); ++ it) {
				const config& cfg = *it;
				if (turn_data_->process_network_data(cfg, lobby.sock, backlog, skip_replay_) == turn_info::PROCESS_RESTART_TURN) {
					// Clean undo stack if turn has to be restarted (losing control)
					if (!undo_stack_.empty()) {
						font::floating_label flabel(_("Undoing moves not yet transmitted to the server."));

						SDL_Color color = {255,255,255,255};
						flabel.set_color(color);
						SDL_Rect rect = gui_->map_area();
						flabel.set_position(rect.w/2, rect.h/2);
						flabel.set_lifetime(150);
						flabel.set_clip_rect(rect);

						font::add_floating_label(flabel);
					}

					while(!undo_stack_.empty()) {
						menu_handler_.undo(gui_->get_playing_team() + 1);
					}
					throw end_turn_exception(gui_->get_playing_team() + 1);
				}
			}

		} catch (end_level_exception& e) {
			turn_data_->send_data();
			throw e;
		}

		if (!linger_ && (current_team().countdown_time() > 0) && gamestate_.mp_settings().mp_countdown) {
			SDL_Delay(1);
			const int ticks = SDL_GetTicks();
			int new_time = current_team().countdown_time()-std::max<int>(1,(ticks - cur_ticks));
			if (new_time > 0 ){
				current_team().set_countdown_time(new_time);
				cur_ticks = ticks;
				if(current_team().is_human() && !beep_warning_time_) {
					beep_warning_time_ = new_time - WARNTIME + ticks;
				}
				if(counting_down()) {
					think_about_countdown(ticks);
				}
			} else {
				// Clock time ended
				// If no turn bonus or action bonus -> defeat
				const int action_increment = gamestate_.mp_settings().mp_countdown_action_bonus;
				if ( (gamestate_.mp_settings().mp_countdown_turn_bonus == 0 )
					&& (action_increment == 0 || current_team().action_bonus_count() == 0)) {
					// Not possible to end level in MP with throw end_level_exception(DEFEAT);
					// because remote players only notice network disconnection
					// Current solution end remaining turns automatically
					current_team().set_countdown_time(10);
				} else {
					const int maxtime = gamestate_.mp_settings().mp_countdown_reservoir_time;
					int secs = gamestate_.mp_settings().mp_countdown_turn_bonus;
					secs += action_increment  * current_team().action_bonus_count();
					current_team().set_action_bonus_count(0);
					secs = (secs > maxtime) ? maxtime : secs;
					current_team().set_countdown_time(1000 * secs);
				}
				turn_data_->send_data();

				throw end_turn_exception();
			}
		}

		gui_->draw();

		turn_data_->send_data();

		auto_end_turn = can_auto_end_turn(false);
	}
	menu_handler_.clear_undo_stack(player_number_);
}

void playmp_controller::set_end_scenario_button()
{
	// Modify the end-turn button
	if (!is_host_) {
		gui::button* btn_end = gui_->find_button("endturn");
		btn_end->enable(false);
	}
	gui_->get_theme().refresh_title2("endturn", "title2");
	gui_->invalidate_theme();
	gui_->redraw_everything();
}

void playmp_controller::reset_end_scenario_button()
{
	// revert the end-turn button text to its normal label
	gui_->get_theme().refresh_title2("endturn", "title");
	gui_->invalidate_theme();
	gui_->redraw_everything();
	gui_->set_game_mode(game_display::RUNNING);
}

void playmp_controller::linger()
{
	mouse_handler_.do_right_click(false);

	browse_ = true;
	linger_ = true;
	// If we need to set the status depending on the completion state
	// we're needed here.
	gui_->set_game_mode(game_display::LINGER_MP);

	// this is actually for after linger mode is over -- we don't want to
	// stay stuck in linger state when the *next* scenario is over.
	gamestate_.classification().completion = "running";
	//current_team().set_countdown_time(0);
	//halt and cancel the countdown timer
	reset_countdown();

	set_end_scenario_button();

	if ( get_end_level_data().reveal_map ) {
		// switch to observer viewpoint
		gui_->set_team(0,true);
		gui_->recalculate_minimap();
		gui_->invalidate_all();
		gui_->draw(true,true);
	}
	bool quit;
	start_pass_scenario_anim(get_end_level_data().result);
	do {
		quit = true;
		try {
			// reimplement parts of play_side()
			player_number_ = first_player_;
			init_turn_data();

			play_human_turn();
			after_human_turn();
			release_turn_data();
			LOG_NG << "finished human turn" << std::endl;
		} catch (game::load_game_exception&) {
			LOG_NG << "caught load-game-exception" << std::endl;
			// this should not happen, the option to load a game is disabled
			throw;
		} catch (end_level_exception&) {
			// thrown if the host ends the scenario and let us advance
			// to the next level
			LOG_NG << "caught end-level-exception" << std::endl;
			reset_end_scenario_button();
			throw;
		} catch (end_turn_exception&) {
			// thrown if the host leaves the game (sends [leave_game]), we need
			// to stay in this loop to stay in linger mode, otherwise the game
			// gets aborted
			LOG_NG << "caught end-turn-exception" << std::endl;
			quit = false;
		} catch (network::error&) {
			LOG_NG << "caught network-error-exception" << std::endl;
			quit = false;
		}
	} while (!quit);

	if (gui_->pass_scenario_anim_id() != -1) {
		gui_->erase_area_anim(gui_->pass_scenario_anim_id());
	}

	reset_end_scenario_button();

	LOG_NG << "ending end-of-scenario linger\n";
}

void playmp_controller::wait_for_upload()
{
	// If the host is here we'll never leave since we wait for the host to
	// upload the next scenario.
	assert(!is_host_);

	const bool set_turn_data = (turn_data_ == 0);
	if(set_turn_data) {
		init_turn_data();
	}

	while(true) {
		try {
			config cfg;
			const network::connection res = dialogs::network_receive_dialog(*gui_, _("Waiting for next scenario..."), cfg);

			std::deque<config> backlog;
			if(res != network::null_connection) {
				if (turn_data_->process_network_data(cfg, res, backlog, skip_replay_)
						== turn_info::PROCESS_END_LINGER) {
					break;
				}
			}

		} catch(end_level_exception& e) {
			turn_data_->send_data();
			throw e;
		}
	}

	if(set_turn_data) {
		release_turn_data();
	}
}

void playmp_controller::after_human_turn(){
	if ( gamestate_.mp_settings().mp_countdown ){
		const int action_increment = gamestate_.mp_settings().mp_countdown_action_bonus;
		const int maxtime = gamestate_.mp_settings().mp_countdown_reservoir_time;
		int secs = (current_team().countdown_time() / 1000) + gamestate_.mp_settings().mp_countdown_turn_bonus;
		secs += action_increment  * current_team().action_bonus_count();
		current_team().set_action_bonus_count(0);
		secs = (secs > maxtime) ? maxtime : secs;
		current_team().set_countdown_time(1000 * secs);
		recorder.add_countdown_update(current_team().countdown_time(),player_number_);
	}
	LOG_NG << "playmp::after_human_turn...\n";

	// Normal post-processing for human turns (clear undos, end the turn, etc.)
	playsingle_controller::after_human_turn();
}

void playmp_controller::finish_side_turn()
{
	play_controller::finish_side_turn();

	//halt and cancel the countdown timer
	reset_countdown();
}

void playmp_controller::play_network_turn(){
	LOG_NG << "is networked...\n";

	gui_->enable_menu("undo", false);
	gui_->enable_menu("endturn", false);
	turn_info turn_data(player_number_, replay_sender_, undo_stack_);
	turn_data.host_transfer().attach_handler(this);

	for(;;) {

		if (!network_processing_stopped_){
			bool turn_ended = false;
			config cfg;

			network::connection from = network::null_connection;

			if (data_backlog_.empty() == false) {
				cfg = data_backlog_.front();
				data_backlog_.pop_front();

			} else if (!received_data_cfg_.empty()) {
				cfg = received_data_cfg_.front();
				received_data_cfg_.erase(received_data_cfg_.begin());
				from = lobby.sock;
			}

			if (!cfg.empty()) {
				if (skip_replay_ && replay_last_turn_ <= turn()) {
					skip_replay_ = false;
				}
				const turn_info::PROCESS_DATA_RESULT result = turn_data.process_network_data(cfg, from, data_backlog_, skip_replay_);
				if (result == turn_info::PROCESS_RESTART_TURN) {
					player_type_changed_ = true;
					return;
				} else if (result == turn_info::PROCESS_END_TURN) {
					turn_ended = true;
					break;
				}
			}
			if (turn_ended) {
				break;
			}
		}

		play_slice();
		check_end_level();

		if (!network_processing_stopped_){
			turn_data.send_data();
		}

		gui_->draw();
	}

	turn_data.host_transfer().detach_handler(this);
	LOG_NG << "finished networked...\n";
	return;
}

void playmp_controller::init_turn_data() 
{
	turn_data_ = new turn_info(player_number_, replay_sender_, undo_stack_);
	turn_data_->host_transfer().attach_handler(this);
}

void playmp_controller::release_turn_data()
{
	//send one more time to make sure network is up-to-date.
	turn_data_->send_data();
	turn_data_->host_transfer().detach_handler(this);
	delete turn_data_;
	turn_data_ = NULL;
}

void playmp_controller::process_oos(const std::string& err_msg) const {
	// Notify the server of the oos error.
	config cfg;
	config& info = cfg.add_child("info");
	info["type"] = "termination";
	info["condition"] = "out of sync";
	network::send_data(cfg, 0);

	std::stringstream temp_buf;
	std::vector<std::string> err_lines = utils::split(err_msg,'\n');
	temp_buf << _("The game is out of sync, and cannot continue. There are a number of reasons this could happen: this can occur if you or another player have modified their game settings. This may mean one of the players is attempting to cheat. It could also be due to a bug in the game, but this is less likely.\n\nDo you want to save an error log of your game?");
	if(!err_msg.empty()) {
		temp_buf << " \n \n"; //and now the "Details:"
		for(std::vector<std::string>::iterator i=err_lines.begin(); i!=err_lines.end(); ++i)
		{
			temp_buf << "`#" << *i << '\n';
		}
		temp_buf << " \n";
	}

	config snapshot;
	to_config(snapshot);
	savegame::oos_savegame save(heros_, heros_start_, snapshot);
	save.save_game_interactive(resources::screen->video(), temp_buf.str(), gui::YES_NO);
}

void playmp_controller::handle_generic_event(const std::string& name){
	turn_info turn_data(player_number_, replay_sender_, undo_stack_);

	if (name == "ai_user_interact"){
		playsingle_controller::handle_generic_event(name);
		turn_data.send_data();
	}
	else if ((name == "ai_gamestate_changed") || (name == "ai_sync_network")){
		turn_data.sync_network();
	}
	else if (name == "host_transfer"){
		is_host_ = true;
		if (linger_){
			gui::button* btn_end = gui_->find_button("endturn");
			btn_end->enable(true);
			gui_->invalidate_theme();
		}
	}
/*
	// why remark, reference http://www.freeors.com/bbs/forum.php?mod=viewthread&tid=21952&page=1&extra=#pid30175
	if (end_turn_) {
		throw end_turn_exception();
	}
*/
}

bool playmp_controller::can_execute_command(hotkey::HOTKEY_COMMAND command, int index) const
{
	bool res = true;
	switch (command){
		case hotkey::HOTKEY_SPEAK:
		case hotkey::HOTKEY_SPEAK_ALLY:
		case hotkey::HOTKEY_SPEAK_ALL:
			res = network::nconnections() > 0;
			break;
		case hotkey::HOTKEY_START_NETWORK:
		case hotkey::HOTKEY_STOP_NETWORK:
			res = is_observer();
			break;
		case hotkey::HOTKEY_STOP_REPLAY:
			if (is_observer()){
				network_processing_stopped_ = true;
				LOG_NG << "network processing stopped";
			}
			break;
	    default:
			return playsingle_controller::can_execute_command(command, index);
	}
	return res;
}

void playmp_controller::sync_undo()
{
	play_controller::sync_undo();
	turn_data_->send_data();
}