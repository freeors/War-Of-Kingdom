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

#include "gui/dialogs/grid_setting.hpp"

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
#include "gui/dialogs/theme.hpp"
#include "gui/dialogs/message.hpp"
#include "unit_map.hpp"
#include "mkwin_controller.hpp"

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

REGISTER_DIALOG(grid_setting)

const std::string tgrid_setting::default_ref = "<default>";

tgrid_setting::tgrid_setting(mkwin_controller& controller, display& disp, unit_map& units, unit& u, const std::string& control)
	: tsetting_dialog(u.cell())
	, tmode_navigate(controller, disp, *this, true)
	, disp_(disp)
	, units_(units)
	, u_(u)
	, control_(control)
	, current_cfg_()
{
}

void tgrid_setting::pre_show(CVideo& /*video*/, twindow& window)
{
	tmode_navigate::pre_show(window);
	
	std::stringstream ss;
	const std::pair<std::string, gui2::tcontrol_definition_ptr>& widget = u_.widget();

	ss.str("");
	ss << "grid setting";
	ss << "(";
	ss << tintegrate::generate_format(control_, "yellow");
	ss << ")";
	tlabel* label = find_widget<tlabel>(&window, "title", false, true);
	label->set_label(ss.str());

	ttext_box* text_box = find_widget<ttext_box>(&window, "_id", false, true);
	text_box->set_maximum_length(max_id_len);
	text_box->set_label(cell_.id);

	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "save", false)
			, boost::bind(
				&tgrid_setting::save
				, this
				, boost::ref(window)
				, true));

	switch_cfg(window);
}

void tgrid_setting::switch_cfg(twindow& window)
{
	const tmode& mode = controller_.mode(current_tab_);
	tadjust adjust = u_.adjust(mode, tadjust::ADD);
	ttoggle_button* toggle = find_widget<ttoggle_button>(&window, "add", false, true);
	toggle->set_value(adjust.valid());
	toggle->set_active(current_tab_ && !(current_tab_ % tmode::res_count));

	adjust = u_.adjust(mode, tadjust::REMOVE);
	toggle = find_widget<ttoggle_button>(&window, "remove", false, true);
	toggle->set_value(adjust.valid());
	toggle->set_active(current_tab_ != 0);
}

void tgrid_setting::save(twindow& window, bool exit)
{
	ttext_box* text_box = find_widget<ttext_box>(&window, "_id", false, true);
	cell_.id = text_box->label();
	// id maybe empty.
	if (!cell_.id.empty() && !utils::isvalid_id(cell_.id, false, min_id_len, max_id_len)) {
		gui2::show_message(disp_.video(), "", utils::errstr);
		return ;
	}
	utils::transform_tolower(cell_.id);

	const tmode& mode = controller_.mode(current_tab_);

	ttoggle_button* toggle = find_widget<ttoggle_button>(&window, "add", false, true);
	tadjust adjust = u_.adjust(mode, tadjust::ADD);
	if (toggle->get_value() && !adjust.valid()) {
		u_.insert_adjust(mode, tadjust(tadjust::ADD, mode.res, null_cfg));
	} else if (!toggle->get_value() && adjust.valid()) {
		u_.erase_adjust(mode, tadjust::ADD);
	}

	toggle = find_widget<ttoggle_button>(&window, "remove", false, true);
	adjust = u_.adjust(mode, tadjust::REMOVE);
	if (toggle->get_value() && !adjust.valid()) {
		u_.insert_adjust(mode, tadjust(tadjust::REMOVE, mode.res, null_cfg));
	} else if (!toggle->get_value() && adjust.valid()) {
		u_.erase_adjust(mode, tadjust::REMOVE);
	}

	if (exit) {
		window.set_retval(twindow::OK);
	} else {
		reload_tab_label();
	}
}

void tgrid_setting::toggle_tabbar(twidget* widget)
{
	twindow* window = widget->get_window();
	save(*window, false);
	tmode_navigate::toggle_tabbar(widget);

	switch_cfg(*window);
}

std::string tgrid_setting::form_tab_label(int at) const
{
	const tmode& mode = controller_.mode(at);

	std::stringstream ss;
	ss << mode.id << "\n";
	ss << mode.res.width << "x" << mode.res.height;

	tadjust adjust = u_.adjust(mode, tadjust::ADD);
	if (adjust.valid()) {
		ss << tintegrate::generate_img("misc/plus.png~SCALE(12,12)");
	}
	adjust = u_.adjust(mode, tadjust::REMOVE);
	if (adjust.valid()) {
		ss << tintegrate::generate_img("misc/delete.png~SCALE(12,12)");
	}
	
	return ss.str();
}

}
