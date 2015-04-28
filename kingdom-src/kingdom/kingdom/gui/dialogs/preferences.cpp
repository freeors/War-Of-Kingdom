/* $Id: campaign_selection.cpp 48740 2011-03-05 10:01:34Z mordante $ */
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

#define GETTEXT_DOMAIN "kingdom-lib"

#include "gui/dialogs/preferences.hpp"

#include "gui/dialogs/helper.hpp"
#include "gui/dialogs/combo_box.hpp"
#include "gui/widgets/image.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/slider.hpp"
#include "gui/widgets/stacked_widget.hpp"
#include "gui/widgets/window.hpp"
#include "serialization/string_utils.hpp"
#include "gettext.hpp"
#include <preferences.hpp>
#include "game_preferences.hpp"
#include "preferences_display.hpp"
#include "display.hpp"
#include "play_controller.hpp"
#include "resources.hpp"
#include "network.hpp"

#include <boost/bind.hpp>

namespace gui2 {

/*WIKI
 * @page = GUIWindowDefinitionWML
 * @order = 2_campaign_selection
 *
 * == Campaign selection ==
 *
 * This shows the dialog which allows the user to choose which campaign to
 * play.
 *
 * @begin{table}{dialog_widgets}
 *
 * campaign_list & & listbox & m &
 *         A listbox that contains all available campaigns. $
 *
 * -icon & & image & o &
 *         The icon for the campaign. $
 *
 * -name & & control & o &
 *         The name of the campaign. $
 *
 * -victory & & image & o &
 *         The icon to show when the user finished the campaign. The engine
 *         determines whether or not the user has finished the campaign and
 *         sets the visible flag for the widget accordingly. $
 *
 * campaign_details & & multi_page & m &
 *         A multi page widget that shows more details for the selected
 *         campaign. $
 *
 * -image & & image & o &
 *         The image for the campaign. $
 *
 * -description & & control & o &
 *         The description of the campaign. $
 *
 * @end{table}
 */

REGISTER_DIALOG(preferences)

tpreferences::tpreferences(display& disp, int start_page)
	: disp_(disp)
	, start_page_(start_page)
	, env_(RANDOM_DUEL, 0, false)
	, page_(-1)
	, options_grid_(NULL)
	, current_page_(twidget::npos)
	, zoom_(NULL)
	, maximal_defeated_activity_(NULL)
	, duel_(NULL)
{
	if (disp_.in_theme() && resources::controller) {
		play_controller& controller = *resources::controller;
		env_.duel = controller.duel();
		env_.maximal_defeated_activity = game_config::maximal_defeated_activity;
		env_.vip = controller.vip();
	}
}

void tpreferences::page_selected(twindow& window, tlistbox& list, const int type)
{
	page_ = list.get_selected_row();

	swap_page(window, page_, true);
}

void tpreferences::pre_show(CVideo& /*video*/, twindow& window)
{
	options_grid_ = find_widget<tstacked_widget>(&window, "options", false, true);

	swap_page(window, start_page_, false);

	// Setup campaign list.
	tlistbox& list = find_widget<tlistbox>(&window, "option_list", false);

	list.set_callback_value_change(dialog_callback3<tpreferences, tlistbox, &tpreferences::page_selected>);

	
	for (int index = 0; index <= MAX_PAGE; index ++) {
		// Add list item
		string_map list_item;
		std::map<std::string, string_map> list_item_item;

		if (index == GENERAL_PAGE) { 
			list_item["label"] = "icons/icon-general.png";
			list_item_item.insert(std::make_pair("icon", list_item));
			list_item["label"] = _("Prefs section^General");
			list_item_item.insert(std::make_pair("name", list_item));

		} else if (index == DISPLAY_PAGE) {
			list_item["label"] = "icons/icon-display.png";
			list_item_item.insert(std::make_pair("icon", list_item));
			list_item["label"] = _("Prefs section^Display");
			list_item_item.insert(std::make_pair("name", list_item));

		} else if (index == MUSIC_PAGE) {
			list_item["label"] = "icons/icon-music.png";
			list_item_item.insert(std::make_pair("icon", list_item));
			list_item["label"] = _("Prefs section^Sound");
			list_item_item.insert(std::make_pair("name", list_item));

		} else if (index == ADVANCED_PAGE) {
			list_item["label"] = "icons/icon-advanced.png";
			list_item_item.insert(std::make_pair("icon", list_item));
			list_item["label"] = _("Advanced section^Advanced");
			list_item_item.insert(std::make_pair("name", list_item));
		} else if (index == SCENARIO_PAGE) {
			if (!disp_.in_theme() || !resources::controller) {
				continue;
			}
			list_item["label"] = "icons/icon-scenario.png";
			list_item_item.insert(std::make_pair("icon", list_item));
			list_item["label"] = _("Prefs section^Scenario");
			list_item_item.insert(std::make_pair("name", list_item));

		} 

		list.add_row(list_item_item);
	}
}

void tpreferences::post_show(twindow& window)
{
}

//
// general
//
void tpreferences::scroll_changed(tslider* widget, int value)
{
	preferences::set_scroll_speed(value);
}

void tpreferences::turbo_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	preferences::set_turbo(disp_, toggle->get_value());
}

void tpreferences::turbo_changed(tslider* widget, int value)
{
	// 0.25 0.5 1 2 4 8 16
	std::vector< double > turbo_items;
	turbo_items.push_back(0.25);
	turbo_items.push_back(0.5);
	turbo_items.push_back(0.75);
	turbo_items.push_back(1);
	turbo_items.push_back(1.5);
	turbo_items.push_back(2);
	turbo_items.push_back(4);
	turbo_items.push_back(8);
	turbo_items.push_back(16);

	preferences::set_turbo_speed(disp_, turbo_items[value]);
}

void tpreferences::eng_file_name_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	preferences::set_eng_file_name(!toggle->get_value());
}

void tpreferences::autosavemax_changed(tslider* widget, int value)
{
	preferences::set_autosavemax(value);
}

//
// display
//
void tpreferences::fullscreen_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);

	twindow* window = widget->get_window();
	window->set_retval(toggle->get_value()? preferences::MAKE_FULLSCREEN: preferences::MAKE_WINDOWED);
}

void tpreferences::flip_time_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	preferences::set_flip_time(toggle->get_value());
}

void tpreferences::default_move_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	preferences::set_default_move(toggle->get_value());
}

void tpreferences::video_mode_button(twindow& window)
{
	window.set_retval(preferences::CHANGE_RESOLUTION);
}

//
// music
//
void tpreferences::sound_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	preferences::set_sound(toggle->get_value());
}

void tpreferences::sound_changed(tslider* widget, int value)
{
	preferences::set_sound_volume(value);
}

void tpreferences::music_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	preferences::set_music(toggle->get_value());
}

void tpreferences::music_changed(tslider* widget, int value)
{
	preferences::set_music_volume(value);
}

void tpreferences::turn_bell_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	preferences::set_turn_bell(toggle->get_value());
}

void tpreferences::turn_bell_changed(tslider* widget, int value)
{
	preferences::set_bell_volume(value);
}

void tpreferences::UI_sound_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	preferences::set_UI_sound(toggle->get_value());
}

void tpreferences::UI_sound_changed(tslider* widget, int value)
{
	preferences::set_UI_volume(value);
}

//
// multiplayer
//
void tpreferences::maximal_defeated_activity(twindow& window)
{
	std::vector<tval_str> items;

	items.push_back(tval_str(0, "0"));
	items.push_back(tval_str(50, "50"));
	items.push_back(tval_str(100, "100"));
	items.push_back(tval_str(150, "150"));

	gui2::tcombo_box dlg(items, env_.maximal_defeated_activity);
	dlg.show(disp_.video());

	if (dlg.selected_val() == env_.maximal_defeated_activity) {
		return;
	}
	int selected = dlg.selected_index();
	env_.maximal_defeated_activity = items[selected].val;

	maximal_defeated_activity_->set_label(items[selected].str);
}

void tpreferences::duel(twindow& window)
{
	// The possible eras to play
	std::vector<tval_str> items;

	items.push_back(tval_str(NO_DUEL, _("Hasn't")));
	items.push_back(tval_str(RANDOM_DUEL, _("Random")));

	gui2::tcombo_box dlg(items, env_.duel);
	dlg.show(disp_.video());

	int selected = dlg.selected_index();
	env_.duel = items[selected].val;

	duel_->set_label(items[selected].str);
}

//
// advanced
//
void tpreferences::chat_timestamp_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	preferences::set_chat_timestamping(toggle->get_value());
}

void tpreferences::developer_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	preferences::set_developer(toggle->get_value());
}

void tpreferences::interrupt_when_ally_sighted_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	preferences::set_interrupt_when_ally_sighted(toggle->get_value());
}

void tpreferences::scroll_to_action_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	preferences::set_scroll_to_action(toggle->get_value());
}

void tpreferences::show_color_cursors_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	// preferences::set_color_cursors(toggle->get_value());
}

void tpreferences::zoom_button(twindow& window)
{
	std::vector<tval_str> items;
	items.push_back(tval_str(display::ZOOM_72, "72 x 72"));
	items.push_back(tval_str(display::ZOOM_64, "64 x 64"));
	items.push_back(tval_str(display::ZOOM_56, "56 x 56"));
	items.push_back(tval_str(display::ZOOM_48, "48 x 48"));

	gui2::tcombo_box dlg(items, display::initial_zoom);
	dlg.show(disp_.video());

	if (dlg.selected_val() != display::initial_zoom) {
		display::initial_zoom = dlg.selected_val();
		preferences::_set_zoom(display::initial_zoom);

		std::stringstream str;
		str << _("Grid Size") << ": " << display::initial_zoom << " x " << display::initial_zoom;
		zoom_->set_label(str.str());

		image::set_zoom(display::initial_zoom);
	}
}

void tpreferences::show_grid_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	preferences::set_grid(disp_, toggle->get_value());
}

int turbo_slider_get_value()
{
	// 0.25 0.5 1 2 4 8 16
	std::vector< double > turbo_items;
	turbo_items.push_back(0.25);
	turbo_items.push_back(0.5);
	turbo_items.push_back(0.75);
	turbo_items.push_back(1);
	turbo_items.push_back(1.5);
	turbo_items.push_back(2);
	turbo_items.push_back(4);
	turbo_items.push_back(8);
	turbo_items.push_back(16);

	std::vector<double>::iterator itor = std::find(turbo_items.begin(), turbo_items.end(), preferences::turbo_speed());
	if (itor != turbo_items.end()) {
		return itor - turbo_items.begin();
	} else {
		return 1;
	}
}

void tpreferences::no_messagebox_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	game_config::no_messagebox = !game_config::no_messagebox;
}

void tpreferences::swap_page(twindow& window, int page, bool swap)
{
	if (!options_grid_) {
		return;
	}
	if (current_page_ == page) {
		return;
	}

	options_grid_->set_radio_layer(page);

	if (swap) {
		return;
	}

	// GENERAL_PAGE
		tslider* slider = dynamic_cast<tslider*>(options_grid_->find("scroll_slider", false));
		slider->set_callback_value_change(boost::bind(&tpreferences::scroll_changed, this, _1, _2));
		slider->set_value(preferences::scroll_speed());

		ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(options_grid_->find("turbo_button", false));
		toggle->set_callback_state_change(boost::bind(&tpreferences::turbo_toggled, this, _1));
		toggle->set_value(preferences::turbo());

		slider = dynamic_cast<tslider*>(options_grid_->find("turbo_slider", false));
		slider->set_callback_value_change(boost::bind(&tpreferences::turbo_changed, this, _1, _2));
		slider->set_value(turbo_slider_get_value());

		toggle = dynamic_cast<ttoggle_button*>(options_grid_->find("eng_file_name_button", false));
		toggle->set_callback_state_change(boost::bind(&tpreferences::eng_file_name_toggled, this, _1));
		toggle->set_value(!preferences::eng_file_name());

		slider = dynamic_cast<tslider*>(options_grid_->find("autosavemax_slider", false));
		slider->set_callback_value_change(boost::bind(&tpreferences::autosavemax_changed, this, _1, _2));
		slider->set_value(preferences::autosavemax());
	
	// DISPLAY_PAGE
		toggle = dynamic_cast<ttoggle_button*>(options_grid_->find("flip_time_button", false));
		toggle->set_value(preferences::flip_time());
		toggle->set_callback_state_change(boost::bind(&tpreferences::flip_time_toggled, this, _1));

		toggle = dynamic_cast<ttoggle_button*>(options_grid_->find("default_move_button", false));
		toggle->set_value(preferences::default_move());
		toggle->set_callback_state_change(boost::bind(&tpreferences::default_move_toggled, this, _1));

#if (defined(__APPLE__) && TARGET_OS_IPHONE) || defined(ANDROID)
		options_grid_->find("fullscreen_button", false)->set_visible(twidget::INVISIBLE);
		find_widget<tbutton>(&window, "video_mode_button", false).set_visible(twidget::INVISIBLE);
#else 
		toggle = dynamic_cast<ttoggle_button*>(options_grid_->find("fullscreen_button", false));
		toggle->set_callback_state_change(boost::bind(&tpreferences::fullscreen_toggled, this, _1));
		toggle->set_value(preferences::fullscreen());

		connect_signal_mouse_left_click(
			find_widget<tbutton>(&window, "video_mode_button", false)
			, boost::bind(
				&tpreferences::video_mode_button
				, this
				, boost::ref(window)));
#endif
	// MUSIC_PAGE
		toggle = dynamic_cast<ttoggle_button*>(options_grid_->find("sound_button", false));
		toggle->set_callback_state_change(boost::bind(&tpreferences::sound_toggled, this, _1));
		toggle->set_value(preferences::sound_on());

		slider = dynamic_cast<tslider*>(options_grid_->find("sound_slider", false));
		slider->set_callback_value_change(boost::bind(&tpreferences::sound_changed, this, _1, _2));
		slider->set_value(preferences::sound_volume());

		toggle = dynamic_cast<ttoggle_button*>(options_grid_->find("music_button", false));
		toggle->set_callback_state_change(boost::bind(&tpreferences::music_toggled, this, _1));
		toggle->set_value(preferences::music_on());

		slider = dynamic_cast<tslider*>(options_grid_->find("music_slider", false));
		slider->set_callback_value_change(boost::bind(&tpreferences::music_changed, this, _1, _2));
		slider->set_value(preferences::music_volume());

		toggle = dynamic_cast<ttoggle_button*>(options_grid_->find("turn_bell_button", false));
		toggle->set_callback_state_change(boost::bind(&tpreferences::turn_bell_toggled, this, _1));
		toggle->set_value(preferences::turn_bell());

		slider = dynamic_cast<tslider*>(options_grid_->find("bell_slider", false));
		slider->set_callback_value_change(boost::bind(&tpreferences::turn_bell_changed, this, _1, _2));
		slider->set_value(preferences::bell_volume());

		toggle = dynamic_cast<ttoggle_button*>(options_grid_->find("UI_sound_button", false));
		toggle->set_callback_state_change(boost::bind(&tpreferences::UI_sound_toggled, this, _1));
		toggle->set_value(preferences::UI_sound_on());
		toggle->set_active(false);

		slider = dynamic_cast<tslider*>(options_grid_->find("UI_sound_slider", false));
		slider->set_callback_value_change(boost::bind(&tpreferences::UI_sound_changed, this, _1, _2));
		slider->set_value(preferences::UI_volume());
		slider->set_active(false);

	// ADVANCED_PAGE
		toggle = dynamic_cast<ttoggle_button*>(options_grid_->find("chat_timestamp_button", false));
		toggle->set_callback_state_change(boost::bind(&tpreferences::chat_timestamp_toggled, this, _1));
		toggle->set_value(preferences::chat_timestamping());

		toggle = dynamic_cast<ttoggle_button*>(options_grid_->find("developer", true));
		toggle->set_callback_state_change(boost::bind(&tpreferences::developer_toggled, this, _1));
		toggle->set_value(preferences::developer());

		toggle = dynamic_cast<ttoggle_button*>(options_grid_->find("interrupt_when_ally_sighted_button", false));
		toggle->set_callback_state_change(boost::bind(&tpreferences::interrupt_when_ally_sighted_toggled, this, _1));
		toggle->set_value(preferences::interrupt_when_ally_sighted());

		toggle = dynamic_cast<ttoggle_button*>(options_grid_->find("scroll_to_action_button", false));
		toggle->set_value(preferences::scroll_to_action());
		toggle->set_callback_state_change(boost::bind(&tpreferences::scroll_to_action_toggled, this, _1));

		toggle = dynamic_cast<ttoggle_button*>(options_grid_->find("show_color_cursors_button", false));
// #if (defined(__APPLE__) && TARGET_OS_IPHONE) || defined(ANDROID)
#if 1
		toggle->set_visible(twidget::INVISIBLE);
#else
		toggle->set_value(preferences::use_color_cursors());
		toggle->set_callback_state_change(boost::bind(&tpreferences::show_color_cursors_toggled, this, _1));
#endif

		zoom_ = dynamic_cast<tbutton*>(options_grid_->find("zoom_button", false));
		if (disp_.in_theme()) {
			zoom_->set_visible(twidget::INVISIBLE);
		} else {
			connect_signal_mouse_left_click(
				*zoom_
				, boost::bind(
					&tpreferences::zoom_button
					, this
					, boost::ref(window)));
			std::stringstream str;
			str << _("Grid Size") << ": " << display::initial_zoom << " x " << display::initial_zoom;
			zoom_->set_label(str.str());
		}

		toggle = dynamic_cast<ttoggle_button*>(options_grid_->find("show_grid_button", false));
		toggle->set_value(preferences::grid());
		toggle->set_callback_state_change(boost::bind(&tpreferences::show_grid_toggled, this, _1));

	// SCENARIO_PAGE
	if (disp_.in_theme() && resources::controller) {
		maximal_defeated_activity_ = dynamic_cast<tbutton*>(options_grid_->find("maximal_defeated_activity", false));
		connect_signal_mouse_left_click(
			*maximal_defeated_activity_
			, boost::bind(
				&tpreferences::maximal_defeated_activity
				, this
				, boost::ref(window)));
		std::stringstream strstr;
		strstr << env_.maximal_defeated_activity;
		maximal_defeated_activity_->set_label(strstr.str());
		if (tent::tower_mode() || resources::controller->is_replaying() || resources::controller->has_network_player()) {
			maximal_defeated_activity_->set_active(false);
		}

		duel_ = dynamic_cast<tbutton*>(options_grid_->find("duel", false));
		connect_signal_mouse_left_click(
			*duel_
			, boost::bind(
				&tpreferences::duel
				, this
				, boost::ref(window)));
		strstr.str("");
		if (env_.duel == NO_DUEL) {
			strstr << _("Hasn't");
		} else if (env_.duel == ALWAYS_DUEL) {
			strstr << _("Always");
		} else {
			strstr << _("Random");
		}
		duel_->set_label(strstr.str());
		if (tent::tower_mode() || resources::controller->is_replaying() || resources::controller->has_network_player()) {
			duel_->set_active(false);
		}

		toggle = dynamic_cast<ttoggle_button*>(options_grid_->find("card_button", false));
		toggle->set_value(env_.vip);
		toggle->set_active(false);
		if (tent::tower_mode()) {
			toggle->set_visible(twidget::INVISIBLE);
		}
		
		toggle = dynamic_cast<ttoggle_button*>(options_grid_->find("tactic_slot_button", false));
		toggle->set_value(env_.vip);
		toggle->set_active(false);
		if (!tent::tower_mode()) {
			toggle->set_visible(twidget::INVISIBLE);
		}

		toggle = dynamic_cast<ttoggle_button*>(options_grid_->find("no_messagebox_button", false));
		if (resources::controller->is_replaying()) {
			toggle->set_callback_state_change(boost::bind(&tpreferences::no_messagebox_toggled, this, _1));
			toggle->set_value(game_config::no_messagebox);
		} else {
			toggle->set_visible(twidget::INVISIBLE);
		}
	}
}

} // namespace gui2

