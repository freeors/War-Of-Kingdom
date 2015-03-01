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

#ifndef GUI_DIALOGS_RECT_SETTING_HPP_INCLUDED
#define GUI_DIALOGS_RECT_SETTING_HPP_INCLUDED

#include "gui/dialogs/cell_setting.hpp"
#include "gui/dialogs/mode_navigate.hpp"
#include "unit.hpp"
#include <vector>

class mkwin_controller;
class display;
class unit_map;

namespace gui2 {

class trect_setting : public tsetting_dialog, public tmode_navigate
{
public:
	static const std::string default_ref;
	explicit trect_setting(mkwin_controller& controller, display& disp, unit_map& units, unit& u, bool must_use_rect);

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	void set_ref(twindow& window);
	void set_ref_label(twindow& window);
	void set_rect_label(twindow& window);
	bool collect_ref(const unit::tchild& child, std::vector<std::string>& refs) const;

	void set_xanchor(twindow& window);
	void set_xanchor_label(twindow& window) const;
	void set_yanchor(twindow& window);
	void set_yanchor_label(twindow& window) const;

	void rect_toggled(twidget* widget);
	void save(twindow& window, bool exit);
	void toggle_tabbar(twidget* widget);

	void switch_cfg(twindow& window, const config& cfg);

	std::string form_tab_label(int at) const;
	bool require_show_flag(int index) const;

private:
	display& disp_;
	unit_map& units_;
	unit& u_;
	bool must_use_rect_;
	config current_cfg_;
	tadjust current_adjust_;
};


}


#endif /* ! GUI_DIALOGS_CAMPAIGN_DIFFICULTY_HPP_INCLUDED */
