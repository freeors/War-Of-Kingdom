/* $Id: mp_connect.cpp 49307 2011-04-25 07:19:14Z mordante $ */
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

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "gui/dialogs/unit_merit.hpp"

#include "gettext.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "game_display.hpp"
#include "unit.hpp"

// #include <boost/bind.hpp>


namespace gui2 {

REGISTER_DIALOG(unit_merit)

tunit_merit::tunit_merit(game_display& gui, const unit& u)
	: gui_(gui)
	, u_(u)
{
}

void tunit_merit::pre_show(CVideo& /*video*/, twindow& window)
{
	std::stringstream str;

	tlabel* label = find_widget<tlabel>(&window, "title", false, true);
	str << u_.name() << _(" troop");
	label->set_label(str.str());

	str.str("");
	label = find_widget<tlabel>(&window, "cause_damage", false, true);
	str << u_.cause_damage_;
	label->set_label(str.str());

	str.str("");
	label = find_widget<tlabel>(&window, "been_damage", false, true);
	str << u_.been_damage_;
	label->set_label(str.str());

	str.str("");
	label = find_widget<tlabel>(&window, "defeat_units", false, true);
	str << u_.defeat_units_;
	label->set_label(str.str());

	str.str("");
	label = find_widget<tlabel>(&window, "field_turns", false, true);
	str << u_.field_turns_;
	label->set_label(str.str());
}

} // namespace


