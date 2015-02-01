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

#include "gui/dialogs/column_setting.hpp"

#include "display.hpp"
#include "formula_string_utils.hpp"
#include "gettext.hpp"

#include "gui/dialogs/helper.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/widgets/scroll_text_box.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/dialogs/combo_box.hpp"
#include "unit.hpp"

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

REGISTER_DIALOG(column_setting)

tcolumn_setting::tcolumn_setting(display& disp, const unit& u)
	: tsetting_dialog(u.cell())
	, disp_(disp)
	, u_(u)
{
}

void tcolumn_setting::pre_show(CVideo& /*video*/, twindow& window)
{
	std::stringstream ss;
	const std::pair<std::string, gui2::tcontrol_definition_ptr>& widget = u_.widget();

	ss.str("");
	ss << "COLUMN#" << tintegrate::generate_format(u_.get_location().x, "yellow"); 
	tlabel* label = find_widget<tlabel>(&window, "title", false, true);
	label->set_label(ss.str());

	ttext_box* text_box = find_widget<ttext_box>(&window, "_grow_factor", false, true);
	text_box->set_label(str_cast(cell_.column.grow_factor));

	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "save", false)
			, boost::bind(
				&tcolumn_setting::save
				, this
				, boost::ref(window)
				, _3, _4));
}

void tcolumn_setting::save(twindow& window, bool& handled, bool& halt)
{
	ttext_box* text_box = find_widget<ttext_box>(&window, "_grow_factor", false, true);
	int border = lexical_cast_default<int>(text_box->label());
	if (border < 0 || border > 9) {
		handled = true;
		halt = true;
		return;
	}
	cell_.column.grow_factor = border;

	window.set_retval(twindow::OK);
}

}
