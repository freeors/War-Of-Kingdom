/* $Id: menu_events.hpp 41353 2010-02-22 20:08:38Z boucman $ */
/*
   Copyright (C) 2006 - 2010 by Joerg Hinrichs <joerg.hinrichs@alice-dsl.de>
   wesnoth playturn Copyright (C) 2003 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef MENU_EVENTS_H_INCLUDED
#define MENU_EVENTS_H_INCLUDED

#include "global.hpp"
#include "actions.hpp"
#include "chat_events.hpp"

class game_state;
class tod_manager;
class hero_map;
class card_map;

namespace events {
	class mouse_handler;
}

struct fallback_ai_to_human_exception {};

namespace events {

class menu_handler : private chat_handler {
public:
	menu_handler(play_controller& controller, game_display* gui, unit_map& units, hero_map& heros, card_map& cards, std::vector<team>& teams,
		const config& level, const gamemap& map,
		const config& game_config, const tod_manager& tod_mng, game_state& gamestate,
		undo_list& undo_stack);
	virtual ~menu_handler();

	const undo_list& get_undo_list() const;
	void set_gui(game_display* gui) { gui_ = gui; }

	std::string get_title_suffix(int side_num);
	void objectives(int side_num);
	void reside_unit_list_in_city(const artifical* city, bool troop = true, bool commoner = false);
	void field_unit_list_in_city(const artifical* city, bool troop = true, bool commoner = false);
	int hero_list(std::vector<hero*>& heros);

	void interior(int side_num);
	void technology_tree(int side_num);
	void list(int side_num);
	void system(int side_num);

	void save_map();
	void preferences();
	void speak();
	void whisper();
	void shout();
	// void recruit(bool browse, int side_num, const map_location &last_hex);
	void move(bool browse, int side_num, const map_location &last_hex);
	void build(const std::string& type, mouse_handler& mousehandler, unit& builder);
	void guard(mouse_handler& mousehandler, unit& u);
	void abolish(mouse_handler& mousehandler, unit& u);
	void extract(mouse_handler& mousehandler, unit& u);
	void advance(mouse_handler& mousehandler, unit* u);
	void demolish(mouse_handler& mousehandler, unit* u);
	void armory(mouse_handler& mousehandler, int side_num, artifical& art);
	void expedite(int side_num, const map_location& last_hex);
	void undo(int side_num);
	void redo(int side_num);
	void unit_detail(const unit* interviewee);
	void play_card(bool browse, int side_num);
	void toggle_shroud_updates(int side_num);
	void update_shroud_now(int side_num);
	bool end_turn(int side_num);
	void switch_list(int side_num);
	void change_side(mouse_handler& mousehandler);
	void clear_labels();
	void execute_gotos(mouse_handler &mousehandler, int side_num);
	void toggle_grid();
	void clear_messages();

	unit* current_unit();
	const unit* current_unit(const mouse_handler &mousehandler) const
	{ return const_cast<menu_handler *>(this)->current_unit(); }
	void do_speak();
	void clear_undo_stack(int side_num);
protected:
	void add_chat_message(const time_t& time, const std::string& speaker,
			int side, const std::string& message,
			events::chat_handler::MESSAGE_TYPE type=events::chat_handler::MESSAGE_PRIVATE);
	void send_chat_message(const std::string& message, bool allies_only=false);
private:
	//void do_speak(const std::string& message, bool allies_only);
//	std::vector<std::string> create_unit_table(const statistics::stats::str_int_map& m,unsigned int team);
	bool has_friends() const;
	bool clear_shroud(int side_num);
	static void change_controller(const std::string& side, const std::string& controller);
	static void change_side_controller(const std::string& side, const std::string& player, bool own_side=false);

	play_controller& controller_;
	game_display* gui_;
	unit_map& units_;
	hero_map& heros_;
	card_map& cards_;
	std::vector<team>& teams_;
	const config& level_;
	const gamemap& map_;
	const config& game_config_;
	const tod_manager& tod_manager_;
	game_state& gamestate_;

	undo_list& undo_stack_;
	std::string last_search_;
	map_location last_search_hit_;

	std::string last_recruit_;
};

}
#endif
