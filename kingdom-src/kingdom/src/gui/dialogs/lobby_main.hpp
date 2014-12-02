/* $Id: lobby_main.hpp 49600 2011-05-22 17:56:04Z mordante $ */
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

#ifndef GUI_DIALOGS_LOBBY_HPP_INCLUDED
#define GUI_DIALOGS_LOBBY_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "gui/widgets/tree_view.hpp"
#include "chat_events.hpp"
#include "gui/dialogs/lobby/lobby_info.hpp"
#include "gui/dialogs/chat.hpp"

#include <boost/scoped_ptr.hpp>

class display;
class hero_map;

#ifdef GUI2_EXPERIMENTAL_LISTBOX
#include "gui/widgets/list.hpp"
#endif

namespace gui2 {

class tgrid;
class tlabel;
#ifndef GUI2_EXPERIMENTAL_LISTBOX
class tlistbox;
#endif
class ttext_box;
class twindow;
class ttoggle_button;
class tscrollbar_panel;

struct tlobby_chat_window
{
	tlobby_chat_window(const std::string& name, bool whisper)
		: name(name), whisper(whisper), pending_messages(0)
	{
	}
	std::string name;
	bool whisper;
	int pending_messages;
};

class tlobby_main : public tdialog, public lobby_base, public tlobby::thandler, public tchat_
{
public:
	tlobby_main(const config& game_config, lobby_info& info, display& disp, hero_map& heros, hero_map& heros_start);

	~tlobby_main();

	void update_gamelist();

protected:
	void update_gamelist_header();

	void update_gamelist_diff();

	void update_gamelist_filter();

	std::map<std::string, string_map> make_game_row_data(const game_info& game);

	void adjust_game_row_contents(const game_info& game, int idx, tgrid* grid);

public:

	enum legacy_result { QUIT, JOIN, OBSERVE, CREATE, PREFERENCES };

	legacy_result get_legacy_result() const { return legacy_result_; }
	int joined_game_id() const { return joined_game_id_; }

	enum t_notify_mode {
		NOTIFY_NONE,
		NOTIFY_MESSAGE,
		NOTIFY_MESSAGE_OTHER_WINDOW,
		NOTIFY_SERVER_MESSAGE,
		NOTIFY_OWN_NICK,
		NOTIFY_FRIEND_MESSAGE,
		NOTIFY_WHISPER,
		NOTIFY_WHISPER_OTHER_WINDOW,
		NOTIFY_LOBBY_JOIN,
		NOTIFY_LOBBY_QUIT,
		NOTIFY_COUNT
	};

	void do_notify(t_notify_mode mode);

protected:
	void keyboard_shown(twindow& window);
	void keyboard_hidden(twindow& window);
	void update_network_status(twindow& window, bool connected);

private:
	void update_selected_game();

	/**
	 * Result flag for interfacing with other MP dialogs
	 */
	legacy_result legacy_result_;

	int joined_game_id_;

	/**
	 * Get the room* corresponding to the currently active window, or NULL
	 * if a whisper window is active at the moment
	 */
	room_info* active_window_room();

	/**
	 * Check if a room window for "room" is open, if open_new is true
	 * then it will be created if not found.
	 * @return valid ptr if the window was found or added, null otherwise
	 */
	tlobby_chat_window* room_window_open(const std::string& room, bool open_new);

	/**
	 * Check if a whisper window for user "name" is open, if open_new is true
	 * then it will be created if not found.
	 * @return valid ptr if the window was found or added, null otherwise
	 */
	tlobby_chat_window* whisper_window_open(const std::string& name, bool open_new);

	/**
	 * Helper function to find and open a new window, used by *_window_open
	 */
	tlobby_chat_window* search_create_window(const std::string& name, bool whisper, bool open_new);

	/**
	 * Network polling callback
	 */
	bool handle(tlobby::ttype type, const config& data);

	// void process_message(const config& data, bool whisper = false);

	void process_gamelist(const config& data);

	void process_gamelist_diff(const config& data);

	void process_room_join(const config& data);

	void process_room_part(const config& data);

	void process_room_query_response(const config& data);

	void join_button_callback(twindow& window);

	void observe_button_callback(twindow& window);

	void join_global_button_callback(twindow& window);

	void observe_global_button_callback(twindow& window);

	void join_or_observe(int index);

	/**
	 * Assemble and send a game join request. Ask for password if the game
	 * requires one.
	 * @return true iif the request was actually sent, false if not for some
	 *         reason like user canceled the password dialog or idx was invalid.
	 */
	bool do_game_join(int idx, bool observe);

	void create_button_callback(twindow& window);

	void refresh_button_callback(twindow& window);

	void quit_button_callback(twindow& window);

	void game_filter_reload();

	void gamelist_change_callback(twindow& window);

	void skip_replay_changed_callback(twidget* w);

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	/** Inherited from tdialog. */
	virtual void post_build(CVideo& video, twindow& window);

	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	/** Inherited from tdialog. */
	void post_show(twindow& window);

	void fill_lobby(twindow& window);
	void close_lobby(twindow& window);
	void fill_chat(twindow& window);
	void close_chat(twindow& window);

	enum {NONE_PAGE, MIN_PAGE, LOBBY_PAGE = MIN_PAGE, CHAT_PAGE, CHATING_PAGE, MAX_PAGE = CHATING_PAGE};
	void sheet_toggled(twidget* widget);

	void desire_swap_page(twindow& window, int page, bool open);

private:
	const config& game_config_;

	tlistbox* gamelistbox_;

	twindow* window_;

	lobby_info& lobby_info_;

	/**
	 * This represents the open chat windows (rooms and whispers at the moment)
	 * with 1 to 1 correspondence to what the user sees in the interface
	 */
	std::vector<tlobby_chat_window> open_windows_;

	size_t active_window_;

	int selected_game_id_;

	bool gamelist_dirty_;

	unsigned last_gamelist_update_;

	bool gamelist_diff_update_;

	display& disp_;

	hero_map& heros_;

	hero_map& heros_start_;

	std::vector<int> gamelist_id_at_row_;

	bool delay_gamelist_update_;

	friend struct lobby_delay_gamelist_update_guard;

	std::map<int, ttoggle_button*> sheet_;
};

} // namespace gui2

#endif

