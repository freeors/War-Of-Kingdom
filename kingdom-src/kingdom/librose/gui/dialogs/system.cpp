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

#include "gui/dialogs/system.hpp"

#include "gui/dialogs/helper.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/listbox.hpp"
#include <boost/bind.hpp>
#include "gettext.hpp"

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

REGISTER_DIALOG(system)

const int max_items = 6;
tsystem::tsystem(const std::vector<titem>& items)
	: items_(items)
	, retval_(twindow::OK)
{
}

void tsystem::pre_show(CVideo& /*video*/, twindow& window)
{
	std::stringstream strstr;

	for (int n = 0; n < max_items; n ++) {
		strstr.str("");
		strstr << "item" << n;

		tbutton& button = find_widget<tbutton>(&window, strstr.str(), false);

		if (n < (int)items_.size()) {
			button.set_label(items_[n].name);
			if (!items_[n].enable) {
				button.set_active(items_[n].enable);
			}

			connect_signal_mouse_left_click(
				button
				, boost::bind(
					&tsystem::set_retval
					, this
					, boost::ref(window)
					, n));
		} else {
			button.set_visible(twidget::INVISIBLE);
		}
	}
}

void tsystem::set_retval(twindow& window, int val)
{
	retval_ = val;
	window.close();
}

}
