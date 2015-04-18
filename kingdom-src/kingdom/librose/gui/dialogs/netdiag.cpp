/* $Id: lobby_player_info.cpp 48440 2011-02-07 20:57:31Z mordante $ */
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

#include "gui/dialogs/helper.hpp"
#include "gui/dialogs/netdiag.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/scroll_label.hpp"

#include "gettext.hpp"

#include <boost/bind.hpp>

namespace gui2 {

REGISTER_DIALOG(netdiag)

tnetdiag::tnetdiag()
	: log_(NULL)
{
}

tnetdiag::~tnetdiag()
{
}

void tnetdiag::pre_show(CVideo& /*video*/, twindow& window)
{
	log_ = &find_widget<tscroll_label>(&window, "log", false);
	log_->set_label(lobby->format_log_str());
	log_->set_scroll_to_end(true);

	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "connect", false)
			, boost::bind(
				  &tnetdiag::connect_button_callback
				, this
				, boost::ref(window)));
	find_widget<tbutton>(&window, "connect", false).set_active(false);

	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "clear", false)
			, boost::bind(
				  &tnetdiag::clear_button_callback
				, this
				, boost::ref(window)));

	join();
}

void tnetdiag::post_show(twindow& /*window*/)
{
}

void tnetdiag::handle(const tsock& s, const std::string& msg)
{
	log_->set_label(lobby->format_log_str());
	// log_->scroll_vertical_scrollbar(tscrollbar_::END);
}

void tnetdiag::connect_button_callback(twindow& window)
{
}

void tnetdiag::clear_button_callback(twindow& window)
{
	log_->set_label(null_str);
	lobby->clear_log();
}

} //end namespace gui2
