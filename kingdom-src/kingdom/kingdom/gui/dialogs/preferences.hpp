/* $Id: campaign_selection.hpp 49604 2011-05-22 17:56:21Z mordante $ */
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

#ifndef GUI_DIALOGS_PREFERENCES_HPP_INCLUDED
#define GUI_DIALOGS_PREFERENCES_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "unit.hpp"

class display;

namespace gui2 {

class tstacked_widget;
class tbutton;
class tslider;
class tlistbox;

class tpreferences
	: public tdialog
{
public:
	explicit tpreferences(display& disp, int start_page);

	/***** ***** ***** setters / getters for members ***** ****** *****/
	enum {GENERAL_PAGE, DISPLAY_PAGE, MUSIC_PAGE, ADVANCED_PAGE, SCENARIO_PAGE, MAX_PAGE = SCENARIO_PAGE};

	const tscenario_env& get_scenario_env() const { return env_; }
private:
	/** Called when another page is selected. */
	void page_selected(twindow& window, tlistbox& list, const int type = twidget::drag_none);

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	/** Inherited from tdialog. */
	void post_show(twindow& window);

	void swap_page(twindow& window, int page, bool swap);

	// general
	void scroll_changed(tslider* widget, int value);
	void turbo_toggled(twidget* widget);
	void turbo_changed(tslider* widget, int value);
	void eng_file_name_toggled(twidget* widget);
	void autosavemax_changed(tslider* widget, int value);
	// display
	void fullscreen_toggled(twidget* widget);
	void flip_time_toggled(twidget* widget);
	void default_move_toggled(twidget* widget);
	void video_mode_button(twindow& window);
	// music
	void sound_toggled(twidget* widget);
	void sound_changed(tslider* widget, int value);
	void music_toggled(twidget* widget);
	void music_changed(tslider* widget, int value);
	void turn_bell_toggled(twidget* widget);
	void turn_bell_changed(tslider* widget, int value);
	void UI_sound_toggled(twidget* widget);
	void UI_sound_changed(tslider* widget, int value);
	// advanced
	void chat_timestamp_toggled(twidget* widget);
	void developer_toggled(twidget* widget);
	void interrupt_when_ally_sighted_toggled(twidget* widget);
	void scroll_to_action_toggled(twidget* widget);
	void show_color_cursors_toggled(twidget* widget);
	void zoom_button(twindow& window);
	void show_grid_toggled(twidget* widget);
	// scenario
	void maximal_defeated_activity(twindow& window);
	void duel(twindow& window);
	void no_messagebox_toggled(twidget* widget);

private:
	/** The chosen campaign. */
	display& disp_;
	int start_page_;
	int page_;
	tstacked_widget* options_grid_;
	int current_page_;

	tscenario_env env_;
	tbutton* zoom_;
	tbutton* maximal_defeated_activity_;
	tbutton* duel_;
};

} // namespace gui2

#endif
