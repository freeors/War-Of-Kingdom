/* $Id: multiplayer.hpp 47847 2010-12-05 21:12:23Z shadowmaster $ */
/*
   Copyright (C) 2005 - 2010 Philippe Plantier <ayin@anathas.org>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/
#ifndef MULTIPLAYER_HPP_INCLUDED
#define MULTIPLAYER_HPP_INCLUDED

#include "multiplayer_ui.hpp"

class config;
class game_display;
class hero_map;
class card_map;

namespace savegame {

struct www_save_info;

}

namespace mp {

/*
 * This is the main entry points of multiplayer mode.
 */

/** Starts a multiplayer game in single-user mode.
 *
 * @param disp        The global display
 * @param game_config The global, top-level WML configuration for the game
 * @param default_controller The default controller type
 */
void start_local_game(game_display& disp, const config& game_config, hero_map& heros, hero_map& heros_start, 
		card_map& cards, mp::controller default_controller);

/** Starts a multiplayer game in client mode.
 *
 * @param disp        The global display
 * @param game_config The global, top-level WML configuration for the game
 * @param host        The host to connect to.
 */
void start_client(game_display& disp, const config& game_config, hero_map& heros, hero_map& heros_start, 
		card_map& cards, const std::string& host);


}

namespace http {

bool version(game_display& disp, time_t time);
bool register_user(game_display& disp, bool check_exist = false);
struct membership 
{
	membership():
	  vip(-1)
	  , score(0)
	  , coin(0)
	 {}

	int vip; // vip = -1 indicate network fail.
	int score;
	int coin;
};
membership membership_hero(game_display& disp, const hero& h, bool quiet);
bool avatar_hero(game_display& disp, const hero& h);
bool download_avatar(game_display& disp);
bool upload_save(game_display& disp, const std::string& name);
std::string download_save(game_display& disp, int sid);
std::vector<savegame::www_save_info> list_save(game_display& disp);

struct pass_statistic {
	std::string username;
	time_t create_time;
	int duration;
	int version;
	int coin;
	int score;
	int type;
	int hash;
};

enum BOARD_TYPE {BOARD_SCORE, BOARD_PASS};

struct board_statistic {
	int type;
	std::string username;
	struct {
		int vip;
		int coin;
		int score;
	} score;
	struct {
		time_t create_time;
		int duration;
		int version;
		int coin;
		int score;
		int type;
	} pass;
};

bool upload_pass(game_display& disp, const pass_statistic& stat);
std::vector<pass_statistic> list_pass(game_display& disp);
std::vector<board_statistic> list_board(game_display& disp, BOARD_TYPE type);
std::pair<bool, int> renew(game_display& disp, time_t time);

}

#endif
