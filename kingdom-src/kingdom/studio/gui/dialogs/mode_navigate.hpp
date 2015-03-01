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

#ifndef GUI_DIALOGS_MODE_NAVIGATE_HPP_INCLUDED
#define GUI_DIALOGS_MODE_NAVIGATE_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "gui/widgets/report.hpp"
#include "util.hpp"
#include "serialization/string_utils.hpp"
#include <vector>
#include <set>

class display;
class mkwin_controller;

namespace gui2 {


class tmode_navigate
{
public:
	explicit tmode_navigate(mkwin_controller& controller, display& disp, tdialog& dialog, bool read_only);

protected:
	void pre_show(twindow& window);

	void append_mode(twindow& window);

	void reload_navigate(twindow& window);
	virtual void toggle_tabbar(twidget* widget);

	virtual std::string form_tab_label(int at) const = 0;
	void reload_tab_label() const;
protected:
	display& disp_;
	mkwin_controller& controller_;
	tdialog& dialog_;
	bool read_only_;

	ttabbar navigate_;
	int current_tab_;
};


}


#endif /* ! GUI_DIALOGS_CAMPAIGN_DIFFICULTY_HPP_INCLUDED */
