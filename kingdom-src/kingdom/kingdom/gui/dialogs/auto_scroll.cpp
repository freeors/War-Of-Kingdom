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
#include "gui/dialogs/auto_scroll.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/scroll_label.hpp"

#include "gettext.hpp"

namespace gui2 {

REGISTER_DIALOG(auto_scroll)

tauto_scroll::tauto_scroll(const std::string& txt)
	: txt_(txt)
{
}

void tauto_scroll::pre_show(CVideo& /*video*/, twindow& window)
{
	tscroll_label* widget = &find_widget<tscroll_label>(&window, "txt", false);
	widget->set_auto_scroll(true);
	widget->set_label(txt_);
}

} //end namespace gui2
