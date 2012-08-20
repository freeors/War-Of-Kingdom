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

class display;

namespace gui2 {

class tgrid;
class tbutton;
class tslider;

void show_preferences_dialog(display& disp);

class tpreferences
	: public tdialog
{
public:
	explicit tpreferences(display& disp, int start_page);

	/***** ***** ***** setters / getters for members ***** ****** *****/

private:
	friend void show_preferences_dialog(display& disp);

	enum {MIN_PAGE = 0, GENERAL_PAGE = MIN_PAGE, DISPLAY_PAGE, MUSIC_PAGE, MULTIPLAYER_PAGE, ADVANCED_PAGE, MAX_PAGE = ADVANCED_PAGE};

	/** Called when another page is selected. */
	void page_selected(twindow& window);

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
	void turn_dialog_toggled(twidget* widget);
	void save_replays_toggled(twidget* widget);
	void delete_saves_toggled(twidget* widget);
	void autosavemax_changed(tslider* widget, int value);
	// display
	void fullscreen_toggled(twidget* widget);
	void flip_time_toggled(twidget* widget);
	void show_floating_labels_toggled(twidget* widget);
	void show_haloing_toggled(twidget* widget);
	void show_team_colors_toggled(twidget* widget);
	void idle_anim_toggled(twidget* widget);
	void idle_anim_changed(tslider* widget, int value);
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
	// multiplayer
	void chat_timestamp_toggled(twidget* widget);
	// advanced
	void scroll_when_mouse_outside_toggled(twidget* widget);
	void whiteboard_on_start_toggled(twidget* widget);
	void interrupt_when_ally_sighted_toggled(twidget* widget);
	void scroll_to_action_toggled(twidget* widget);
	void show_color_cursors_toggled(twidget* widget);
	void zoom_button(twindow& window);
	void show_grid_toggled(twidget* widget);

private:
	/** The chosen campaign. */
	display& disp_;
	int start_page_;
	int page_;
	tgrid* options_grid_;
	tbutton* zoom_;
};

} // namespace gui2

#endif
