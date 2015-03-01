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

#include "gui/dialogs/rect_setting.hpp"

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

REGISTER_DIALOG(rect_setting)

const std::string trect_setting::default_ref = "<default>";

trect_setting::trect_setting(mkwin_controller& controller, display& disp, unit_map& units, unit& u, bool must_use_rect)
	: tsetting_dialog(u.cell())
	, tmode_navigate(controller, disp, *this, true)
	, disp_(disp)
	, units_(units)
	, u_(u)
	, must_use_rect_(must_use_rect)
	, current_cfg_()
	, current_adjust_(tadjust::NONE, tres(theme::XDim, theme::YDim), null_cfg)
{
}

void trect_setting::pre_show(CVideo& /*video*/, twindow& window)
{
	tmode_navigate::pre_show(window);
	
	std::stringstream ss;
	const std::pair<std::string, gui2::tcontrol_definition_ptr>& widget = u_.widget();

	ss.str("");
	ss << "rect setting";
	ss << " (";
	ss << tintegrate::generate_format(u_.cell().id, "yellow");
	ss << ")";
	tlabel* label = find_widget<tlabel>(&window, "title", false, true);
	label->set_label(ss.str());

	ttoggle_button* toggle = find_widget<ttoggle_button>(&window, "rect", false, true);
	if (must_use_rect_) {
		toggle->set_value(true);
		toggle->set_visible(twidget::INVISIBLE);
	} else {
		toggle->set_value(!cell_.cfg.empty());
		toggle->set_callback_state_change(boost::bind(&trect_setting::rect_toggled, this, _1));
	}

	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "set_ref", false)
			, boost::bind(
				&trect_setting::set_ref
				, this
				, boost::ref(window)));

	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "set_xanchor", false)
			, boost::bind(
				&trect_setting::set_xanchor
				, this
				, boost::ref(window)));

	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "set_yanchor", false)
			, boost::bind(
				&trect_setting::set_yanchor
				, this
				, boost::ref(window)));

	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "save", false)
			, boost::bind(
				&trect_setting::save
				, this
				, boost::ref(window)
				, true));

	switch_cfg(window, cell_.cfg);
}

void trect_setting::switch_cfg(twindow& window, const config& cfg)
{
	current_cfg_ = cfg;

	set_ref_label(window);
	set_rect_label(window);
	set_xanchor_label(window);
	set_yanchor_label(window);
}

void trect_setting::rect_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	twindow* window = toggle->get_window();

	std::vector<std::string> ids;
	ids.push_back("set_ref");
	ids.push_back("x");
	ids.push_back("y");
	ids.push_back("width");
	ids.push_back("height");

	bool active = toggle->get_value();
	for (std::vector<std::string>::const_iterator it = ids.begin(); it != ids.end(); ++ it) {
		tcontrol* control = find_widget<tcontrol>(window, *it, false, true);
		control->set_active(active);
	}
}

bool trect_setting::collect_ref(const unit::tchild& child, std::vector<std::string>& refs) const
{
	for (std::vector<unit*>::const_iterator it = child.units.begin(); it != child.units.end(); ++ it) {
		const unit* u = *it;
		const std::vector<unit::tchild>& children = u->children();
		for (std::vector<unit::tchild>::const_iterator it2 = children.begin(); it2 != children.end(); ++ it2) {
			if (!collect_ref(*it2, refs)) {
				return false;
			}
		}
		if (u == &u_) {
			return false;
		}
		if (!u->cell().id.empty()) {
			refs.push_back(u->cell().id);
		}
	}
	return true;
}

void trect_setting::set_ref(twindow& window)
{
	std::stringstream ss;
	std::vector<tval_str> items;
	std::vector<std::string> refs;

	int index = 0;
	int def = index;
	refs.push_back(theme::id_screen);

	std::vector<unit*> units = controller_.form_top_units();
	for (std::vector<unit*>::const_iterator it = units.begin(); it != units.end(); ++ it) {
		const unit* u = *it;
		if (u->type() != unit::WIDGET) {
			continue;
		}
		const std::vector<unit::tchild>& children = u->children();
		for (std::vector<unit::tchild>::const_iterator it2 = children.begin(); it2 != children.end(); ++ it2) {
			if (!collect_ref(*it2, refs)) {
				break;
			}
		}
		if (u->parent_at_top() == u_.parent_at_top()) {
			break;
		}
		if (!u->cell().id.empty()) {
			refs.push_back(u->cell().id);
		}
	}
	for (std::vector<std::string>::const_iterator it = refs.begin(); it != refs.end(); ++ it) {
		const std::string& ref = *it;
		if (ref == current_cfg_["ref"].str()) {
			def = index;
		}
		ss.str("");
		if (ref == theme::id_screen) {
			ss << tintegrate::generate_format(ref, "green");
		} else if (ref == theme::id_main_map) {
			ss << tintegrate::generate_format(ref, "yellow");
		} else {
			ss << ref;
		}
		items.push_back(tval_str(index ++, ss.str()));
	}

	gui2::tcombo_box dlg(items, def);
	dlg.show(disp_.video());

	current_cfg_["ref"] = refs[dlg.selected_index()];

	set_ref_label(window);
}

void trect_setting::set_xanchor_label(twindow& window) const
{
	int xanchor = theme::read_anchor(current_cfg_["xanchor"]);

	std::stringstream ss;
	ss << horizontal_anchor.find(xanchor)->second.description;
	tlabel* label = find_widget<tlabel>(&window, "xanchor", false, true);
	label->set_label(ss.str());
}

void trect_setting::set_xanchor(twindow& window)
{
	std::stringstream ss;
	std::vector<tval_str> items;

	for (std::map<int, tanchor>::const_iterator it = horizontal_anchor.begin(); it != horizontal_anchor.end(); ++ it) {
		ss.str("");
		ss << it->second.description;
		items.push_back(tval_str(it->first, ss.str()));
	}

	int xanchor = theme::read_anchor(current_cfg_["xanchor"]);
	gui2::tcombo_box dlg(items, xanchor);
	dlg.show(disp_.video());

	xanchor = dlg.selected_val();
	current_cfg_["xanchor"] = horizontal_anchor.find(xanchor)->second.id;

	set_xanchor_label(window);
}

void trect_setting::set_yanchor_label(twindow& window) const
{
	int yanchor = theme::read_anchor(current_cfg_["yanchor"]);

	std::stringstream ss;
	ss << vertical_anchor.find(yanchor)->second.description;
	tlabel* label = find_widget<tlabel>(&window, "yanchor", false, true);
	label->set_label(ss.str());
}

void trect_setting::set_yanchor(twindow& window)
{
	std::stringstream ss;
	std::vector<tval_str> items;

	for (std::map<int, tanchor>::const_iterator it = vertical_anchor.begin(); it != vertical_anchor.end(); ++ it) {
		ss.str("");
		ss << it->second.description;
		items.push_back(tval_str(it->first, ss.str()));
	}

	int yanchor = theme::read_anchor(current_cfg_["yanchor"]);
	gui2::tcombo_box dlg(items, yanchor);
	dlg.show(disp_.video());

	yanchor = dlg.selected_val();
	current_cfg_["yanchor"] = vertical_anchor.find(yanchor)->second.id;

	set_yanchor_label(window);
}

void trect_setting::save(twindow& window, bool exit)
{
	ttoggle_button* toggle = find_widget<ttoggle_button>(&window, "rect", false, true);

	if (toggle->get_value()) {
		std::stringstream ss;
		ttext_box* text_box = find_widget<ttext_box>(&window, "x", false, true);
		ss << text_box->get_value();

		text_box = find_widget<ttext_box>(&window, "y", false, true);
		ss << "," << text_box->get_value();

		text_box = find_widget<ttext_box>(&window, "width", false, true);
		ss << "," << text_box->get_value();

		text_box = find_widget<ttext_box>(&window, "height", false, true);
		ss << "," << text_box->get_value();

		current_cfg_["rect"] = ss.str();

	} else {
		current_cfg_.remove_attribute("rect");
	}

	const tmode& current_mode = controller_.mode(current_tab_);
	if (current_adjust_.type == tadjust::NONE) {
		cell_.cfg = current_cfg_;
	} else {
		current_adjust_.cfg = current_cfg_;
		u_.insert_adjust(current_mode, current_adjust_);
	}

	if (exit) {
		window.set_retval(twindow::OK);
	} else {
		reload_tab_label();
	}
}

void trect_setting::set_ref_label(twindow& window)
{
	std::string str = current_cfg_["ref"].str();
	if (str.empty()) {
		str = default_ref;
	}

	tlabel* label = find_widget<tlabel>(&window, "ref", false, true);
	label->set_label(str);
}

void trect_setting::set_rect_label(twindow& window)
{
	std::vector<std::string> vstr = utils::split(current_cfg_["rect"].str());
	for (size_t n = vstr.size(); n < 4; n ++) {
		vstr.push_back("");
	}

	ttext_box* text_box = find_widget<ttext_box>(&window, "x", false, true);
	text_box->set_label(vstr[0]);

	text_box = find_widget<ttext_box>(&window, "y", false, true);
	text_box->set_label(vstr[1]);

	text_box = find_widget<ttext_box>(&window, "width", false, true);
	text_box->set_label(vstr[2]);

	text_box = find_widget<ttext_box>(&window, "height", false, true);
	text_box->set_label(vstr[3]);
}

void trect_setting::toggle_tabbar(twidget* widget)
{
	twindow* window = widget->get_window();
	save(*window, false);

	tmode_navigate::toggle_tabbar(widget);

	const tmode& mode = controller_.mode(current_tab_);

	if (!current_tab_) {
		current_adjust_ = tadjust(tadjust::NONE, tres(theme::XDim, theme::YDim), cell_.cfg);
	} else {
		current_adjust_ = u_.adjust(mode, tadjust::CHANGE);
		if (!current_adjust_.valid()) {
			current_adjust_ = tadjust(tadjust::CHANGE, mode.res, cell_.cfg);
		}
	}

	switch_cfg(*window, current_adjust_.cfg);
}

std::string trect_setting::form_tab_label(int at) const
{
	const tmode& mode = controller_.mode(at);

	std::stringstream ss;
	ss << mode.id << "\n";
	ss << mode.res.width << "x" << mode.res.height;
	if (require_show_flag(at)) {
		ss << tintegrate::generate_img("misc/dot.png");
	}
	return ss.str();
}

bool trect_setting::require_show_flag(int index) const
{
	if (!index) {
		return false;
	}
	const tmode& mode = controller_.mode(index);
	tadjust adjust = u_.adjust(mode, tadjust::CHANGE);
	return adjust.valid() &&  tadjust::different_cfg(cell_.cfg, adjust.cfg);
}

}
