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

#define GETTEXT_DOMAIN "rose-lib"

#include "gui/dialogs/combo_box2.hpp"

#include "gettext.hpp"

#include "gui/dialogs/helper.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/label.hpp"

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

REGISTER_DIALOG(combo_box2)

tcombo_box2::tcombo_box2(const std::string& title, const std::vector<tval_str>& items, int val)
	: title_(title)
	, cursel_(items.empty()? twidget::npos: 0)
	, items_(items)
{
	for (std::vector<tval_str>::const_iterator it = items.begin(); it != items.end(); ++ it) {
		if (it->val == val) {
			cursel_ = std::distance(items.begin(), it);
			break;
		}
	}
	initial_select_ = cursel_;
}

void tcombo_box2::pre_show(CVideo& /*video*/, twindow& window)
{
	// window.set_transition_fade(ttransition::fade_left);
	window.canvas()[0].set_variable("background_image",	variant("dialogs/decorate-background.png"));

	tlabel* title = find_widget<tlabel>(&window, "title", false, true);
	title->set_label(title_);

	ok_ = find_widget<tbutton>(&window, "ok", false, true);
	ok_->set_visible(twidget::HIDDEN);

	tlistbox& list = find_widget<tlistbox>(&window, "listbox", false);
	window.keyboard_capture(&list);

	std::map<std::string, string_map> data;

	int at = 0;
	for (std::vector<tval_str>::const_iterator it = items_.begin(); it != items_.end(); ++ it) {
		const tval_str& item = *it;
		
		data["label"]["label"] = item.str;
		list.add_row(data);

		at ++;
	}
	list.set_callback_value_change(dialog_callback3<tcombo_box2, tlistbox, &tcombo_box2::item_selected>);

	if (cursel_ != twidget::npos) {
		list.select_row(cursel_);
	}
}

void tcombo_box2::item_selected(twindow& window, tlistbox& list, const int type)
{
	cursel_ = list.get_selected_row();
	ok_->set_visible(cursel_ != initial_select_? twidget::VISIBLE: twidget::HIDDEN);

	if (callback_item_change_) {
		callback_item_change_(list, cursel_);
	}
}

void tcombo_box2::post_show(twindow& window)
{
	tlistbox& list = find_widget<tlistbox>(&window, "listbox", false);
	cursel_ = list.get_selected_row();
}

}
