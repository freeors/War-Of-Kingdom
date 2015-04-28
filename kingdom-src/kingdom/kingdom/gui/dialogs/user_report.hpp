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

class display;
class hero;
class hero_map;

namespace gui2 {

class ttoggle_button;
class tstacked_widget;
class tlistbox;

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
	explicit tuser_report(display& disp, hero_map& heros, const config& game_config);

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

	enum {PLAYER_PASS_PAGE, RANK_PASS_PAGE, EMPLOYEE_PAGE, RANK_SCORE_PAGE, TITLE_BOARD_PAGE, NONE_PAGE};
	void sheet_toggled(twidget* widget);
	void swap_page(twindow& window, int page, bool swap);

	void fill_base(twindow& window);
	void fill_rank_pass(twindow& window);
	void fill_employee(twindow& window);
	void fill_score_board(twindow& window);
	void fill_title_board(twindow& window);

	void fill_pass_table(tlistbox& list, twindow& window, const std::vector<http::pass_statistic>& passes, bool board);

	void detail_group(twindow& window, int page);
	void detail_employee(twindow& window);

private:
	display& disp_;
	hero_map& heros_;
	const config& game_config_;

	int current_page_;
	std::map<int, ttoggle_button*> sheet_;
	tstacked_widget* page_panel_;
	std::vector<tlistbox*> layers_;
};

} // namespace gui2

#endif
