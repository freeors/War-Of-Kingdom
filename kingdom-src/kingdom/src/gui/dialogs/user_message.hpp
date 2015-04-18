/* $Id: title_screen.hpp 48740 2011-03-05 10:01:34Z mordante $ */
/*
   Copyright (C) 2008 - 2011 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_DIALOGS_USER_MESSAGE_HPP_INCLUDED
#define GUI_DIALOGS_USER_MESSAGE_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include <map>
#include "multiplayer.hpp"
#include "gui/dialogs/chat.hpp"

class display;
class hero;
class hero_map;

namespace gui2 {

class ttoggle_button;
class tscrollbar_panel;

/** Do we wish to show the button for the debug clock. */
/**
 * This class implements the title screen.
 *
 * The menu buttons return a result back to the caller with the button pressed.
 * So at the moment it only handles the tips itself.
 *
 * @todo Evaluate whether we can handle more buttons in this class.
 */
class tuser_message : public tchat_, public tlobby::thandler
{
public:
	explicit tuser_message(display& disp, hero_map& heros, const config& game_config);

	~tuser_message();

private:

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	/** Inherited from tdialog. */
	virtual void post_build(CVideo& video, twindow& window);

	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	void post_show(twindow& window);

	void set_label_int(twindow& window, const std::string& id, int value);

	enum {NONE_PAGE, MIN_PAGE, CHAT_PAGE = MIN_PAGE, MESSAGE_PAGE, SIEGE_RECORD_PAGE, CHATING_PAGE, MAX_PAGE = CHATING_PAGE};
	void sheet_toggled(twidget* widget);
	void desire_swap_page(twindow& window, int page, bool open);

	void fill_message(twindow& window);
	void fill_siege_record(twindow& window);

	void refresh_message_2_label(twindow& window, const std::string& content);
	void message_selected(twindow& window);
	void fill_message_table(twindow& window, const std::vector<http::tmessage_record>& messages);

	void detail_group(twindow& window);
	void detail_employee(twindow& window);
	void send_message(twindow& window);

	void update_network_status(twindow& window, bool connected);

	bool handle_raw(int at, tsock::ttype type, const char* param[]);

private:
	display& disp_;
	hero_map& heros_;
	const config& game_config_;
	
	std::vector<http::tmessage_record> messages_;
	std::map<int, ttoggle_button*> sheet_;
};

} // namespace gui2

#endif
