/* $Id: multiplayer_wait.hpp 47654 2010-11-21 13:59:04Z mordante $ */
/*
   Copyright (C) 2007 - 2010
   Part of the Battle for Wesnoth Project http://www.wesnoth.org

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef MULTIPLAYER_WAIT_HPP_INCLUDED
#define MULTIPLAYER_WAIT_HPP_INCLUDED

#include "widgets/combo.hpp"

#include "leader_list.hpp"
#include "gamestatus.hpp"
#include "multiplayer_ui.hpp"
#include "show_dialog.hpp"

namespace mp {

class wait : public ui
{
public:
	wait(hero_map& heros, game_display& disp, const config& cfg, chat& c, config& gamelist);
	virtual void process_event();

	void join_game(bool observe);

	const game_state& get_state();

	void start_game();

protected:
	virtual void layout_children(const SDL_Rect& rect);
	virtual void hide_children(bool hide=true);
	virtual void process_network_data(const config& data, const network::connection sock);
private:
	void generate_menu();

	gui::button cancel_button_;
	gui::label start_label_;
	gui::menu game_menu_;

	// int team_;
	hero_map& heros_;

	config level_;
	command_pool replay_data_;
	game_state state_;

	bool stop_updates_;
};

}
#endif
