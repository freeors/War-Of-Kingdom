/* $Id: playsingle_controller.hpp 47662 2010-11-21 13:59:28Z mordante $ */
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

#ifndef PLAYSINGLE_CONTROLLER_H_INCLUDED
#define PLAYSINGLE_CONTROLLER_H_INCLUDED

#include "play_controller.hpp"
#include "replay.hpp"

class playsingle_controller : public play_controller
{
public:
	playsingle_controller(const config& level, game_state& state_of_game, hero_map& heros, hero_map& heros_start,
		card_map& cards,
		const int ticks, const int num_turns, const config& game_config, CVideo& video, bool skip_replay);
	virtual ~playsingle_controller();

	LEVEL_RESULT play_scenario(const config::const_child_itors &story,
		bool skip_replay);

	virtual void handle_generic_event(const std::string& name);

	virtual void recruit();
	virtual void build(const std::string& type);
	virtual void extract();
	virtual void demolish();
	virtual void armory();
	virtual void play_card();
	virtual void expedite();
	virtual void move();
	virtual bool can_execute_command(hotkey::HOTKEY_COMMAND command, int index=-1) const;
	virtual void toggle_shroud_updates();
	virtual void update_shroud_now();
	virtual void end_turn();
	virtual void force_end_turn();
	virtual void change_side();
	virtual void label_terrain(bool);
	virtual void clear_labels();
	virtual void clear_messages();

	void linger();

	virtual void force_end_level(LEVEL_RESULT res)
	{ level_result_ = res; }
	virtual void check_end_level();

protected:
	virtual void play_turn(bool no_save);
	virtual void play_side(const unsigned int team_index, bool save);
	virtual void before_human_turn(bool save);
	void show_turn_dialog();
	void execute_gotos();
	void execute_card_uh(size_t turn, int team_index);
	void execute_card_bh(size_t turn, int team_index);
	void execute_guard_attack(int team_index);
	virtual void play_human_turn();
	virtual void after_human_turn();
	void end_turn_record();
	void end_turn_record_unlock();
	void play_ai_turn();
	virtual void init_gui();
	void check_time_over();
	void store_gold(bool obs = false);

	const cursor::setter cursor_setter;
	std::deque<config> data_backlog_;
	gui::floating_textbox textbox_info_;
	replay_network_sender replay_sender_;

	bool end_turn_;
	bool player_type_changed_;
	bool turn_over_;
	bool skip_next_turn_;
	LEVEL_RESULT level_result_;
private:
	void report_victory(std::ostringstream &report, int player_gold,
		int remaining_gold, int finishing_bonus_per_turn,
		int turns_left, int finishing_bonus);
};


#endif
