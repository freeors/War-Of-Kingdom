/* $Id: game_config.hpp 47641 2010-11-21 13:58:24Z mordante $ */
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
#ifndef GAME_CONFIG_H_INCLUDED
#define GAME_CONFIG_H_INCLUDED

class config;
class version_info;
class color_range;

#include "tstring.hpp"

#include <SDL_types.h>

#include <vector>
#include <map>
#include <set>

//basic game configuration information is here.
namespace game_config
{
	extern int base_income;
	extern int village_income;
	extern int hero_income;
	extern int poison_amount;
	extern int rest_heal_amount;
	extern int recall_cost;
	extern int kill_experience;
	extern unsigned lobby_network_timer;
	extern unsigned lobby_refresh;
	extern const std::string version;
	extern const std::string revision;
	extern std::string checksum;
	extern int reside_troop_increase_loyalty;
	extern int field_troop_increase_loyalty;
	extern int wander_loyalty_threshold;
	extern int move_loyalty_threshold;
	extern int ai_keep_loyalty_threshold;
	extern int ai_keep_hp_threshold;
	extern int max_troop_food;
	extern int increase_feeling;
	extern int max_police;
	extern int min_tradable_police;
	extern int max_commoners;
	extern int fixed_tactic_slots;
	extern int active_tactic_slots;

	extern int minimal_activity;
	extern int maximal_defeated_activity;
	extern int tower_fix_heros;
	extern bool score_dirty;

	extern int default_human_level;
	extern int default_ai_level;
	extern int current_level;
	extern int min_level;
	extern int max_level;

	extern int max_noble_level;
	extern int max_tactic_point;
	extern int increase_tactic_point;
	extern int formation_least_adjacent;

	extern int max_bomb_turns;
	extern bool no_messagebox;
	extern bool hide_tactic_slot;

	int kill_xp(int level);
	int attack_xp(int level);

	extern std::string wesnoth_program_dir;

	/** Default percentage gold carried over to the next scenario. */
	extern const int gold_carryover_percentage;

	extern bool debug, editor, no_delay, disable_autosave;

	extern bool use_bin;
	extern int cache_compression_level;
	extern bool tiny_gui;
	extern int start_cards;
	extern int cards_per_turn;
	extern int max_cards;

	enum {INAPP_VIP};
	extern std::map<int, std::string> inapp_items;
	extern std::string sn;

	extern std::string path;
	extern std::string preferences_dir;
	extern std::string preferences_dir_utf8;

	extern std::set<std::string> reserve_players;

	struct server_info {
		server_info() : name(""), address("") { }
		std::string name;
		std::string address; /**< may include ':' followed by port number */
	};
	extern std::vector<server_info> server_list;

	struct bbs_server_info {
		bbs_server_info() : name(""), host(""), port(80), url("") { }
		std::string name;
		std::string host;
		int port;
		std::string url;
	};
	extern bbs_server_info bbs_server;

	extern std::string title_music,
			lobby_music,
			default_victory_music,
			default_defeat_music;

	namespace images {
	extern std::string game_title,
			// orbs and hp/xp bar
			moved_orb,
			unmoved_orb,
			partmoved_orb,
			automatic_orb,
			enemy_orb,
			ally_orb,
			bar_hrl_72,
			bar_hrl_64,
			bar_hrl_56,
			bar_hrl_48,
			// flags
			flag,
			flag_icon,
			big_flag,
			// hex overlay
			terrain_mask,
			grid_top,
			grid_bottom,
			mouseover,
			selected,
			editor_brush,
			unreachable,
			linger,
			// GUI elements
			observer,
			tod_bright,
			tod_dark,
			checked_menu,
			unchecked_menu,
			wml_menu,
			level,
			ellipsis,
			missing;
	} //images


	extern std::string shroud_prefix, fog_prefix;

	extern double hp_bar_scaling, xp_bar_scaling;
	extern double hex_brightening;
	extern double hex_semi_brightening;

	extern std::string flag_rgb;
	extern std::vector<Uint32> red_green_scale;
	extern std::vector<Uint32> red_green_scale_text;

	extern std::vector<std::string> foot_speed_prefix;
	extern std::string foot_teleport_enter, foot_teleport_exit;

	extern int title_logo_x, title_logo_y, title_buttons_x, title_buttons_y, title_buttons_padding, title_tip_x, title_tip_width, title_tip_padding;

	extern std::map<std::string, color_range> team_rgb_range;
	extern std::vector<std::pair<std::string, t_string> > team_rgb_name;
	extern std::map<std::string, std::vector<Uint32> > team_rgb_colors;

	/** observer team name used for observer team chat */
	extern const std::string observer_team_name;

	/**
	 * The maximum number of hexes on a map and items in an array and also used
	 * as maximum in wml loops.
	 */
	extern const size_t max_loop;

	namespace sounds {
		extern const std::string turn_bell, timer_bell, receive_message,
				receive_message_highlight, receive_message_friend,
				receive_message_server, user_arrive, user_leave,
				game_user_arrive, game_user_leave;
		extern const std::string button_press, checkbox_release, slider_adjust,
				menu_expand, menu_contract, menu_select;
	}

	void load_config(const config* cfg);

	bool is_reserve_player(const std::string& player);

	void add_color_info(const config& v);
	const std::vector<Uint32>& tc_info(const std::string& name);
	const color_range& color_info(const std::string& name);

	/**
	 * Return a color corresponding to the value val
	 * red for val=0 to green for val=100, passing by yellow.
	 * Colors are defined by [game_config] keys
	 * red_green_scale and red_green_scale_text
	 */

	Uint32 red_to_green(int val, bool for_text = true);

	extern const version_info wesnoth_version;
	// ai part
	extern int navigation_per_level;

	// if allocate static, iOS may be not align 4! 
	// use dynamic alloc by alloc.
	extern unsigned char* savegame_cache;
	extern int savegame_cache_size;
}

#endif
