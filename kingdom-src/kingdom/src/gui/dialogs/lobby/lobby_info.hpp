/* $Id: lobby_info.hpp 49607 2011-05-22 17:56:29Z mordante $ */
/*
   Copyright (C) 2009 - 2011 by Tomasz Sniatowski <kailoran@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef INC_LOBBY_INFO
#define INC_LOBBY_INFO

#include "config.hpp"
#include "gui/dialogs/lobby/lobby_data.hpp"

/**
 * This class represents the collective information the client has
 * about the players and games on the server
 */
class lobby_info
{
public:
	explicit lobby_info(const config& game_config);

	~lobby_info();

	void delete_games();

	typedef std::map<int, game_info*> game_info_map;

	/**
	 * Process a full gamelist. Current info is discarded.
	 */
	void process_gamelist(const config &data);

	/**
	 * Process a gamelist diff.
	 * @return true on success, false on failure (e.g. when the
	 * diff did not apply correctly)
	 */
	bool process_gamelist_diff(const config &data);

	void sync_games_display_status();

	void make_games_vector();

	const config& gamelist() const { return gamelist_; }

	void clear_game_filter();
	void add_game_filter(game_filter_base* f);
	void set_game_filter_invert(bool value);
	void apply_game_filter();

	game_info* get_game_by_id(int id);
	const game_info* get_game_by_id(int id) const;

	void sort_users(bool by_name, bool by_relation);

	void open_room(const std::string& name);
	void close_room(const std::string& name);
	bool has_room(const std::string& name) const;
	room_info* get_room(const std::string& name);
	const room_info* get_room(const std::string& name) const;

	user_info& get_user(const std::string& name);

	chat_log& get_whisper_log(const std::string& name);

	void update_user_statuses(int game_id, const room_info* room);

	const std::vector<room_info>& rooms() const { return rooms_; }
	const std::vector<game_info*>& games() const { return games_; }
	const std::vector<bool>& games_visibility() const { return games_visibility_; }
	const std::vector<game_info*>& games_filtered() const;
	int games_shown_count() const;
	const std::vector<user_info>& users() const { return users_; }
	const std::vector<user_info*>& users_sorted() const;
private:
	void process_userlist();

	const config& game_config_;
	config gamelist_;
	bool gamelist_initialized_;
	std::vector<room_info> rooms_;

	game_info_map games_by_id_;

	std::vector<game_info*> games_;
	std::vector<game_info*> games_filtered_;
	std::vector<user_info> users_;
	std::vector<user_info*> users_sorted_;
	std::map<std::string, chat_log> whispers_;
	game_filter_and_stack game_filter_;
	bool game_filter_invert_;
	std::vector<bool> games_visibility_;
};

#include "network.hpp"
#include "gui/widgets/tree_view.hpp"

namespace gui2 {

class tlabel;

#define COMBO_FEATURES_NONE			0
#define COMBO_FEATURES_RANDOM		1
#define COMBO_FEATURES_MIN_VALID	2

enum controller { CNTR_NETWORK = 0, CNTR_LOCAL, CNTR_COMPUTER, CNTR_EMPTY, CNTR_LAST };
#define RANDOM_FACTION	-1

std::string decide_player_iocn(int controller);

struct tsub_player_list
{
	void init(twindow& w, const std::string& title);
	void auto_hide();
	std::string title_;
	ttree_view_node* tree;
	tlabel* tree_label;
};

class tplayer_list
{
public:
	virtual void init(twindow& w) = 0;

protected:
	ttree_view* tree;
};

class lobby_base
{
public:
	/**
	 * Network polling callback
	 */
	lobby_base();
	~lobby_base();

	virtual void network_handler();

	virtual void process_network_data(const config& data, const network::connection sock) = 0;

	virtual void process_network_error(network::error& error) {};

	// virtual void process_message(const config& data, bool whisper = false);

	// virtual void process_gamelist(const config& data);

	// virtual void process_gamelist_diff(const config& data);

	// virtual void process_room_join(const config& data);

	// virtual void process_room_part(const config& data);

	// virtual void process_room_query_response(const config& data);

	// virtual void join_button_callback(twindow& window);

	// virtual void observe_button_callback(twindow& window);

	// virtual void join_global_button_callback(twindow& window);

	// virtual void observe_global_button_callback(twindow& window);

	// virtual void join_or_observe(int index);

protected:
	/** Timer for updating the lobby. */
	unsigned long lobby_update_timer_;
};

};

#endif
