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

#ifndef GUI_DIALOGS_GRID_SETTING_HPP_INCLUDED
#define GUI_DIALOGS_GRID_SETTING_HPP_INCLUDED

#include "gui/dialogs/cell_setting.hpp"
#include "gui/dialogs/mode_navigate.hpp"
#include "unit.hpp"
#include <vector>

class mkwin_controller;
class display;
class unit_map;

namespace gui2 {

class tgrid_setting : public tsetting_dialog, public tmode_navigate
{
public:
	static const std::string default_ref;
	explicit tgrid_setting(mkwin_controller& controller, display& disp, unit_map& units, unit& u, const std::string& control);

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	void save(twindow& window, bool exit);
	void toggle_tabbar(twidget* widget);

	void switch_cfg(twindow& window);

	std::string form_tab_label(int at) const;

private:
	display& disp_;
	unit_map& units_;
	unit& u_;
	std::string control_;
	config current_cfg_;
};


}


#endif /* ! GUI_DIALOGS_CAMPAIGN_DIFFICULTY_HPP_INCLUDED */
