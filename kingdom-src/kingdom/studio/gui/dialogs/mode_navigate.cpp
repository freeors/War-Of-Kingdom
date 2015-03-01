/* $Id: campaign_difficulty.cpp 49602 2011-05-22 17:56:13Z mordante $ */
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

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "gui/dialogs/mode_navigate.hpp"

#include "display.hpp"
#include "formula_string_utils.hpp"
#include "gettext.hpp"
#include "filesystem.hpp"
#include "game_config.hpp"
#include "mkwin_controller.hpp"

#include "gui/dialogs/helper.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/report.hpp"
#include "gui/dialogs/edit_box.hpp"
#include "gui/dialogs/message.hpp"

#include <boost/bind.hpp>

namespace gui2 {

/*WIKI
 * @page = GUIWindowDefinitionWML
 * @order = 2_campaign_difficulty
 *
 * == Campaign difficulty ==
 *
 * The campaign mode difficulty menu.
 *
 * @begin{table}{dialog_widgets}
 *
 * title & & label & m &
 *         Dialog title label. $
 *
 * message & & scroll_label & o &
 *         Text label displaying a description or instructions. $
 *
 * listbox & & listbox & m &
 *         Listbox displaying user choices, defined by WML for each campaign. $
 *
 * -icon & & control & m &
 *         Widget which shows a listbox item icon, first item markup column. $
 *
 * -label & & control & m &
 *         Widget which shows a listbox item label, second item markup column. $
 *
 * -description & & control & m &
 *         Widget which shows a listbox item description, third item markup column. $
 *
 * @end{table}
 */

tmode_navigate::tmode_navigate(mkwin_controller& controller, display& disp, tdialog& dialog, bool read_only)
	: controller_(controller)
	, disp_(disp)
	, dialog_(dialog)
	, read_only_(read_only)
	, navigate_(true, false, "sheet")
	, current_tab_(0)
{
}

void tmode_navigate::pre_show(twindow& window)
{
	tbutton* button = find_widget<tbutton>(&window, "_append_mode", false, true);
	if (!read_only_) {
		connect_signal_mouse_left_click(
			  *button
			, boost::bind(
				&tmode_navigate::append_mode
				, this
				, boost::ref(window)));
	} else {
		button->set_visible(twidget::INVISIBLE);
	}

	navigate_.set_report(find_widget<treport>(&window, "navigate", false, true));
	reload_navigate(window);

}

void tmode_navigate::reload_tab_label() const
{
	const tgrid::tchild* children = navigate_.report()->content_grid()->children();
	int childs = ttabbar::front_childs + navigate_.childs();
	for (int i = ttabbar::front_childs; i < childs; i ++) {
		tcontrol* widget = dynamic_cast<tcontrol*>(children[i].widget_);
		widget->set_label(form_tab_label(i - ttabbar::front_childs));
	}
}

void tmode_navigate::reload_navigate(twindow& window)
{
	navigate_.erase_children();

	const std::vector<tmode>& modes = controller_.modes();

	std::stringstream ss;
	int index = 0;
	for (std::vector<tmode>::const_iterator it = modes.begin(); it != modes.end(); ++ it) {
		tcontrol* widget = navigate_.create_child(null_str, null_str, NULL, null_str);
		widget->set_label(form_tab_label(index));
		widget->set_cookie(reinterpret_cast<void*>(index ++));
		navigate_.insert_child(*widget);
	}
	navigate_.select(0);
	navigate_.replacement_children();
}

void tmode_navigate::append_mode(twindow& window)
{
	gui2::tedit_box::tparam param(null_str, null_str, dgettext("wesnoth-lib", "theme^Mode"), "_untitled");
	{
		param.verify = boost::bind(&mkwin_controller::verify_new_mode, &controller_, _1);
		gui2::tedit_box dlg(disp_, param);
		try {
			dlg.show(disp_.video());
		} catch(twml_exception& e) {
			e.show(disp_);
		}
		int res = dlg.get_retval();
		if (res != gui2::twindow::OK) {
			return;
		}
	}
	controller_.insert_mode(param.result);
	reload_navigate(window);
}

void tmode_navigate::toggle_tabbar(twidget* widget)
{
	current_tab_ = (int)reinterpret_cast<long>(widget->cookie());
	dialog_.tdialog::toggle_tabbar(widget);
}

}
