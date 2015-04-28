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

#ifndef GUI_DIALOGS_VISUAL_STUDIO_HPP_INCLUDED
#define GUI_DIALOGS_VISUAL_STUDIO_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "gui/widgets/grid.hpp"
#include <vector>

class display;

namespace gui2 {

class treport;
class tbutton;
class tlabel;

class tvisual_layout : public tdialog
{
public:
	explicit tvisual_layout(display& disp, const twindow& target, const std::string& reason);

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	std::string generate_widget_str(int row, int col, int width, int height, const tgrid::tchild& child);
	void toggle_report(twidget* widget);
	void goto_parent(twindow& window);
	void into_child(twindow& window);

	void reload_layout_report(const tgrid& grid);
	const twidget* calculate_goto_parent(twindow& window, const twidget* widget) const;

private:
	display& disp_;
	const twindow& target_;
	const std::string& reason_;

	twidget* current_widget_;
	treport* layout_report_;
	tbutton* goto_parent_;
	tlabel* current_label_;
	tbutton* into_child_;
};


}


#endif /* ! GUI_DIALOGS_CAMPAIGN_DIFFICULTY_HPP_INCLUDED */
