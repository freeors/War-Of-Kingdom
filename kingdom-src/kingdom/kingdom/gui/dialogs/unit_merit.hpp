/* $Id: mp_connect.hpp 48784 2011-03-06 14:14:07Z mordante $ */
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

#ifndef GUI_DIALOGS_UNIT_MERIT_HPP_INCLUDED
#define GUI_DIALOGS_UNIT_MERIT_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"

class game_display;
class unit;

namespace gui2 {

class tunit_merit : public tdialog
{
public:
	explicit tunit_merit(game_display& gui, const unit& u);

private:

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

private:
	game_display& gui_;
	const unit& u_;
};

} // namespace gui2

#endif

