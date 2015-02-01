/* $Id: campaign_difficulty.hpp 49603 2011-05-22 17:56:17Z mordante $ */
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

#ifndef GUI_DIALOGS_MP_SIDE_WAIT_HPP_INCLUDED
#define GUI_DIALOGS_MP_SIDE_WAIT_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "gamestatus.hpp"
#include "gui/dialogs/lobby/lobby_info.hpp"

class hero_map;
class config;
class display;
class gamemap;

namespace gui2 {

class tlabel;
class tbutton;
class tlistbox;

class tplayer_list_side_wait: public tplayer_list
{
public:
	void init(twindow& w);

public:
	tsub_player_list active_game;
};

class tmp_side_wait : public tdialog, public lobby_base, public tlobby::thandler
{
public:
	display& disp_;
	hero_map& heros_;
	gamemap& gmap_;
	const config& game_config_;

	explicit tmp_side_wait(hero_map& heros, hero_map& heros_start, display& disp, gamemap& gmap, const config& game_config,
			config& gamelist, int game_id, bool observe);

	~tmp_side_wait();

	enum legacy_result { PLAY, QUIT };

	legacy_result get_legacy_result() const { return legacy_result_; }

	/**
	 * Returns the game state, which contains all information about the current
	 * scenario.
	 */
	const game_state& get_state();

	/**
	 * Updates the current game state, resolves random factions, and sends a
	 * "start game" message to the network.
	 */
	void start_game();
private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	/** Inherited from tdialog. */
	void post_show(twindow& window);

	void cancel(twindow& window);

private:
	// Those 2 functions are actually the steps of the (complex)
	// construction of this class.
	void join_game(twindow& window, bool observe);
	void generate_menu(twindow& window);

	/**
	 * Network polling callback
	 */
	bool handle(tlobby::ttype type, const config& data);
	
	void update_playerlist();

private:
	hero_map& heros_start_;

	config level_;

	command_pool replay_data_;

	/** This is the "game state" object which is created by this dialog. */
	game_state state_;

	connected_user_list users_;
	std::map<std::string, http::membership> member_users_;

	tlistbox* sides_table_;
	tlabel* waiting_;
	
	config& gamelist_;

	/**
	 * Result flag for interfacing with other MP dialogs
	 */
	legacy_result legacy_result_;

	int game_id_;
	bool stop_updates_;
	bool observe_;

	tplayer_list_side_wait player_list_;
};


}


#endif /* ! GUI_DIALOGS_CAMPAIGN_DIFFICULTY_HPP_INCLUDED */
