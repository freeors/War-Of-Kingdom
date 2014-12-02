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

#ifndef GUI_DIALOGS_CHAT_HPP_INCLUDED
#define GUI_DIALOGS_CHAT_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "lobby.hpp"
#include <integrate.hpp>

class config;
class display;
class tgroup;

namespace gui2 {

class tlabel;
class tbutton;
class ttree_view_node;
class ttoggle_button;
class tscroll_label;
class ttext_box;
class tscroll_text_box;
class tscrollbar_panel;

class tchat_
{
public:
	struct tcookie {
		tcookie(size_t signature = 0, ttree_view_node* node = NULL, const std::string& username = null_str, int uid = HTTP_INVALID_UID)
			: signature(signature)
			, node(node)
			, username(username)
			, uid(uid)
			, unread(0)
			, online(false)
			, game(false)
		{}

		bool is_chapter() const { return uid == HTTP_INVALID_UID; }
		bool valid_user() const { return uid != HTTP_INVALID_UID; }

		size_t signature;
		ttree_view_node* node;
		std::string username;
		int uid;
		int unread;
		bool online;
		bool game;
	};

	tchat_(display& disp, tgroup& g, int min_page, int chat_page, int chating_page);

protected:
	/** Inherited from tdialog. */
	void pre_show(twindow& window);
	void pre_chating_show(twindow& window);

	/** Inherited from tdialog. */
	void post_show(twindow& window);
	void post_chating_show(twindow& window);

	void process_message(const config &data, bool whisper = false);
	void process_userlist(const config& gamelist);
	void process_userlist_diff(const config& data);
	void process_network_status(bool connected);

	virtual void keyboard_shown(twindow& window);
	virtual void keyboard_hidden(twindow& window);

	virtual void desire_swap_page(twindow& window, int page, bool open) {}

	void contact_toggled(twidget* widget);
	void user_double_click_callback(twidget* widget);
	void netdiag(twindow& window);
	void send(bool& handled, bool& halt);
	void insert_face(twindow& window, int index);
	void signal_handler_sdl_key_down(bool& halt
		, bool& handled
		, const SDLKey key
		, SDLMod modifier
		, const Uint16 unicode);

	void ready_branch(twindow& window, int type);
	void update_node(twindow& window, size_t signature);
	void update_node_internal(const std::vector<tcookie>& cookies, const tcookie& cookie);
	std::pair<std::vector<tcookie>*, tcookie* > contact_find(size_t signature);
	std::pair<std::vector<tcookie>*, tcookie* > contact_find(const std::string& username);
	std::pair<const std::vector<tcookie>*, const tcookie* > contact_find(const std::string& username) const;

	std::string format_log_str(int uid, const std::string& username, std::vector<tintegrate::tlocator>& locator) const;
	void chat_2_scroll_label(tscroll_label& label, int uid, const std::string& username);
	void ready_chat_toolbar(twindow& window);
	void ready_face(twindow& window);

	void swap_page(twindow& window, int page, bool swap);
	void swap_page2(bool& handled, bool& halt, int page);

private:
	void user_to_title(const tcookie& cookie) const;

protected:
	size_t signature_;
	std::map<int, std::vector<tcookie> > cookies_;
	config gamelist_;

	int current_page_;
	tscrollbar_panel* page_panel_;


private:
	display& disp_;
	tgroup& group_;
	twindow* window_;
	tscroll_label* history_;
	tscroll_text_box* input_;
	ttext_box* input_tb_;
	tbutton* send_;
	tbutton* extend_;
	tcookie* current_chat_to_;

	int src_pos_;
	tcookie swap_page_chat_to_;
	std::string swap_page_input_;
	int min_page_;
	int chat_page_;
	int chating_page_;
};

class tchat2: public tdialog, public tchat_, public tlobby::thandler
{
public:
	explicit tchat2(display& disp, tgroup& g);

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	enum {NONE_PAGE, MIN_PAGE, CHAT_PAGE = MIN_PAGE, CHATING_PAGE, MAX_PAGE = CHATING_PAGE};
	void sheet_toggled(twidget* widget);
	
	bool handle(tlobby::ttype type, const config& data);

	void keyboard_shown(twindow& window);
	void keyboard_hidden(twindow& window);
	void update_network_status(twindow& window, bool connected);

private:
	display& disp_;
	std::map<int, ttoggle_button*> sheet_;
};


}


#endif /* ! GUI_DIALOGS_CAMPAIGN_DIFFICULTY_HPP_INCLUDED */
