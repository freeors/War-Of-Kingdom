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

#ifndef GUI_DIALOGS_USER_REPORT_HPP_INCLUDED
#define GUI_DIALOGS_USER_REPORT_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include <map>
#include "multiplayer.hpp"

class game_display;
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
class tuser_report : public tdialog
{
public:
	explicit tuser_report(game_display& gui, hero_map& heros);

	~tuser_report();

private:

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	/** Inherited from tdialog. */
	virtual void post_build(CVideo& video, twindow& window);

	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	void post_show(twindow& window);

	void set_label_int(twindow& window, const std::string& id, int value);

	enum {NONE_PAGE, MIN_PAGE, PLAYER_PASS_PAGE = MIN_PAGE, RANK_PASS_PAGE, RANK_SCORE_PAGE, MAX_PAGE = RANK_SCORE_PAGE};
	void sheet_toggled(twidget* widget);
	void swap_page(twindow& window, int page, bool swap);

	void fill_base(twindow& window);
	void fill_biography(twindow& window);
	void fill_score_board(twindow& window);

	void fill_pass_table(twindow& window, const std::vector<http::pass_statistic>& passes, bool board);

private:
	game_display& gui_;
	hero_map& heros_;

	int current_page_;
	std::map<int, ttoggle_button*> sheet_;
	tscrollbar_panel* page_panel_;
};

} // namespace gui2

#endif
