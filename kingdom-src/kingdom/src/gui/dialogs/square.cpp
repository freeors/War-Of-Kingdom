/* $Id: transient_message.cpp 48961 2011-03-20 18:26:19Z mordante $ */
/*
   Copyright (C) 2009 - 2011 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "gui/dialogs/square.hpp"

#include "gettext.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/scroll_label.hpp"
#include "gui/widgets/window.hpp"

namespace gui2 {

REGISTER_DIALOG(square)

tsquare::tsquare(const std::string& title, const std::string& message)
	: title_(title)
	, message_(message)
{
}

void tsquare::pre_show(CVideo& video, twindow& window)
{
	tlabel* label = find_widget<tlabel>(&window, "title", false, true);
	label->set_label(title_);

	tscroll_label* scroll_label = find_widget<tscroll_label>(&window, "message0", false, true);
	scroll_label->set_label(message_);
	scroll_label->set_visible(twidget::INVISIBLE);

	scroll_label = find_widget<tscroll_label>(&window, "message1", false, true);
	scroll_label->set_visible(twidget::INVISIBLE);

	scroll_label = find_widget<tscroll_label>(&window, "message2", false, true);
	scroll_label->set_visible(twidget::INVISIBLE);

	scroll_label = find_widget<tscroll_label>(&window, "message3", false, true);
	scroll_label->set_visible(twidget::INVISIBLE);
}

void show_square_message(CVideo& video, const std::string& title, const std::string& message)
{
	tsquare dlg(title, message);
	dlg.show(video);
}

} // namespace gui2

