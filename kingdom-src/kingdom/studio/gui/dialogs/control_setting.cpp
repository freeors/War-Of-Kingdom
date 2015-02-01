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

#include "gui/dialogs/control_setting.hpp"

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
#include "gui/dialogs/message.hpp"
#include "gui/auxiliary/window_builder.hpp"
#include "unit.hpp"
#include <cctype>

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

REGISTER_DIALOG(control_setting)

const std::string untitled = "_untitled";

void show_id_error(display& disp, const std::string& id, const std::string& errstr)
{
	std::stringstream err;
	utils::string_map symbols;

	symbols["id"] = tintegrate::generate_format(id, "yellow");
	err << vgettext("wesnoth-lib", "Invalid '$id' value!", symbols);
	err << "\n\n" << errstr;
	gui2::show_message(disp.video(), "", err.str());
}

std::map<int, tlayout> horizontal_layout;
std::map<int, tlayout> vertical_layout;

std::map<int, tscroll_mode> horizontal_mode;
std::map<int, tscroll_mode> vertical_mode;

void init_layout_mode()
{
	if (horizontal_layout.empty()) {
		horizontal_layout.insert(std::make_pair(tgrid::HORIZONTAL_GROW_SEND_TO_CLIENT, 
			tlayout(tgrid::HORIZONTAL_GROW_SEND_TO_CLIENT, "Align left. Maybe stretch")));
		horizontal_layout.insert(std::make_pair(tgrid::HORIZONTAL_ALIGN_LEFT, 
			tlayout(tgrid::HORIZONTAL_ALIGN_LEFT, "Align left")));
		horizontal_layout.insert(std::make_pair(tgrid::HORIZONTAL_ALIGN_CENTER, 
			tlayout(tgrid::HORIZONTAL_ALIGN_CENTER, "Align center")));
		horizontal_layout.insert(std::make_pair(tgrid::HORIZONTAL_ALIGN_RIGHT, 
			tlayout(tgrid::HORIZONTAL_ALIGN_RIGHT, "Align right")));
	}

	if (vertical_layout.empty()) {
		vertical_layout.insert(std::make_pair(tgrid::VERTICAL_GROW_SEND_TO_CLIENT, 
			tlayout(tgrid::VERTICAL_GROW_SEND_TO_CLIENT, "Align top. Maybe stretch")));
		vertical_layout.insert(std::make_pair(tgrid::VERTICAL_ALIGN_TOP, 
			tlayout(tgrid::VERTICAL_ALIGN_TOP, "Align top")));
		vertical_layout.insert(std::make_pair(tgrid::VERTICAL_ALIGN_CENTER, 
			tlayout(tgrid::VERTICAL_ALIGN_CENTER, "Align center")));
		vertical_layout.insert(std::make_pair(tgrid::VERTICAL_ALIGN_BOTTOM, 
			tlayout(tgrid::VERTICAL_ALIGN_BOTTOM, "Align bottom")));
	}

	if (horizontal_mode.empty()) {
		horizontal_mode.insert(std::make_pair(tscrollbar_container::auto_visible_first_run,
			tscroll_mode(tscrollbar_container::auto_visible_first_run, "Auto visible first run")));
		horizontal_mode.insert(std::make_pair(tscrollbar_container::always_visible, 
			tscroll_mode(tscrollbar_container::always_visible, "Always visible")));
		horizontal_mode.insert(std::make_pair(tscrollbar_container::always_invisible, 
			tscroll_mode(tscrollbar_container::always_invisible, "Always invisible")));
		horizontal_mode.insert(std::make_pair(tscrollbar_container::auto_visible, 
			tscroll_mode(tscrollbar_container::auto_visible, "Auto visible")));
	}

	if (vertical_mode.empty()) {
		vertical_mode.insert(std::make_pair(tscrollbar_container::auto_visible_first_run,
			tscroll_mode(tscrollbar_container::auto_visible_first_run, "Auto visible first run")));
		vertical_mode.insert(std::make_pair(tscrollbar_container::always_visible, 
			tscroll_mode(tscrollbar_container::always_visible, "Always visible")));
		vertical_mode.insert(std::make_pair(tscrollbar_container::always_invisible, 
			tscroll_mode(tscrollbar_container::always_invisible, "Always invisible")));
		vertical_mode.insert(std::make_pair(tscrollbar_container::auto_visible, 
			tscroll_mode(tscrollbar_container::auto_visible, "Auto visible")));
	}
}

tcontrol_setting::tcontrol_setting(display& disp, const unit& u, const std::vector<std::string>& textdomains, const std::vector<tlinked_group>& linkeds)
	: tsetting_dialog(u.cell())
	, disp_(disp)
	, u_(u)
	, textdomains_(textdomains)
	, linkeds_(linkeds)
{
}

void tcontrol_setting::pre_show(CVideo& /*video*/, twindow& window)
{
	std::stringstream ss;
	const std::pair<std::string, gui2::tcontrol_definition_ptr>& widget = u_.widget();

	ss.str("");
	ss << widget.first;
	if (widget.second.get()) {
		ss << "(" << tintegrate::generate_format(widget.second->id, "yellow") << ")"; 
		// ss << "    " << widget.second->description;
	}
	tlabel* label = find_widget<tlabel>(&window, "title", false, true);
	label->set_label(ss.str());

	// border size

	// horizontal layout
	set_horizontal_layout_label(window);
	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "_set_horizontal_layout", false)
			, boost::bind(
				&tcontrol_setting::set_horizontal_layout
				, this
				, boost::ref(window)));

	// vertical layout
	set_vertical_layout_label(window);
	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "_set_vertical_layout", false)
			, boost::bind(
				&tcontrol_setting::set_vertical_layout
				, this
				, boost::ref(window)));

	// linked_group
	set_linked_group_label(window);
	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "_set_linked_group", false)
			, boost::bind(
				&tcontrol_setting::set_linked_group
				, this
				, boost::ref(window)));

	// linked_group
	set_textdomain_label(window, true);
	set_textdomain_label(window, false);
	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "textdomain_label", false)
			, boost::bind(
				&tcontrol_setting::set_textdomain
				, this
				, boost::ref(window)
				, true));
	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "textdomain_tooltip", false)
			, boost::bind(
				&tcontrol_setting::set_textdomain
				, this
				, boost::ref(window)
				, false));
	
	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "save", false)
			, boost::bind(
				&tcontrol_setting::save
				, this
				, boost::ref(window)
				, _3, _4));

	ttext_box* text_box = find_widget<ttext_box>(&window, "_border", false, true);
	text_box->set_label(str_cast(cell_.widget.cell.border_size_));
	if (cell_.widget.cell.flags_ & tgrid::BORDER_LEFT) {
		find_widget<ttoggle_button>(&window, "_border_left", false, true)->set_value(true);
	}
	if (cell_.widget.cell.flags_ & tgrid::BORDER_RIGHT) {
		find_widget<ttoggle_button>(&window, "_border_right", false, true)->set_value(true);
	}
	if (cell_.widget.cell.flags_ & tgrid::BORDER_TOP) {
		find_widget<ttoggle_button>(&window, "_border_top", false, true)->set_value(true);
	}
	if (cell_.widget.cell.flags_ & tgrid::BORDER_BOTTOM) {
		find_widget<ttoggle_button>(&window, "_border_bottom", false, true)->set_value(true);
	}

	text_box = find_widget<ttext_box>(&window, "_id", false, true);
	text_box->set_maximum_length(max_id_len);
	text_box->set_label(cell_.widget.id);

	find_widget<ttext_box>(&window, "_width", false, true)->set_value(cell_.widget.width);
	find_widget<ttext_box>(&window, "_height", false, true)->set_value(cell_.widget.height);
	find_widget<tscroll_text_box>(&window, "_label", false, true)->set_label(cell_.widget.label);
	find_widget<tscroll_text_box>(&window, "_tooltip", false, true)->set_label(cell_.widget.tooltip);
}

void tcontrol_setting::save(twindow& window, bool& handled, bool& halt)
{
	ttext_box* text_box = find_widget<ttext_box>(&window, "_border", false, true);
	int border = lexical_cast_default<int>(text_box->label());
	if (border < 0 || border > 20) {
		handled = true;
		halt = true;
		return;
	}
	cell_.widget.cell.border_size_ = border;

	ttoggle_button* toggle = find_widget<ttoggle_button>(&window, "_border_left", false, true);
	if (toggle->get_value()) {
		cell_.widget.cell.flags_ |= tgrid::BORDER_LEFT;
	}
	toggle = find_widget<ttoggle_button>(&window, "_border_right", false, true);
	if (toggle->get_value()) {
		cell_.widget.cell.flags_ |= tgrid::BORDER_RIGHT;
	}
	toggle = find_widget<ttoggle_button>(&window, "_border_top", false, true);
	if (toggle->get_value()) {
		cell_.widget.cell.flags_ |= tgrid::BORDER_TOP;
	}
	toggle = find_widget<ttoggle_button>(&window, "_border_bottom", false, true);
	if (toggle->get_value()) {
		cell_.widget.cell.flags_ |= tgrid::BORDER_BOTTOM;
	}

	text_box = find_widget<ttext_box>(&window, "_id", false, true);
	cell_.widget.id = text_box->label();
	// id maybe empty.
	if (!cell_.widget.id.empty() && !utils::isvalid_id(cell_.widget.id, false, min_id_len, max_id_len)) {
		gui2::show_message(disp_.video(), "", utils::errstr);
		return ;
	}
	utils::transform_tolower(cell_.widget.id);

	text_box = find_widget<ttext_box>(&window, "_width", false, true);
	cell_.widget.width = text_box->get_value();
	text_box = find_widget<ttext_box>(&window, "_height", false, true);
	cell_.widget.height = text_box->get_value();
	tscroll_text_box* scroll_text_box = find_widget<tscroll_text_box>(&window, "_label", false, true);
	cell_.widget.label = scroll_text_box->label();
	scroll_text_box = find_widget<tscroll_text_box>(&window, "_tooltip", false, true);
	cell_.widget.tooltip = scroll_text_box->label();

	window.set_retval(twindow::OK);
}

void tcontrol_setting::set_horizontal_layout(twindow& window)
{
	std::stringstream ss;
	std::vector<tval_str> items;

	for (std::map<int, tlayout>::const_iterator it = horizontal_layout.begin(); it != horizontal_layout.end(); ++ it) {
		ss.str("");
		ss << tintegrate::generate_img(it->second.icon + "~SCALE(24, 24)") << it->second.description;
		items.push_back(tval_str(it->first, ss.str()));
	}

	unsigned h_flag = cell_.widget.cell.flags_ & tgrid::HORIZONTAL_MASK;
	gui2::tcombo_box dlg(items, h_flag);
	dlg.show(disp_.video());

	h_flag = dlg.selected_val();
	cell_.widget.cell.flags_ = (cell_.widget.cell.flags_ & ~tgrid::HORIZONTAL_MASK) | h_flag;

	set_horizontal_layout_label(window);
}

void tcontrol_setting::set_vertical_layout(twindow& window)
{
	std::stringstream ss;
	std::vector<tval_str> items;

	for (std::map<int, tlayout>::const_iterator it = vertical_layout.begin(); it != vertical_layout.end(); ++ it) {
		ss.str("");
		ss << tintegrate::generate_img(it->second.icon + "~SCALE(24, 24)") << it->second.description;
		items.push_back(tval_str(it->first, ss.str()));
	}

	unsigned h_flag = cell_.widget.cell.flags_ & tgrid::VERTICAL_MASK;
	gui2::tcombo_box dlg(items, h_flag);
	dlg.show(disp_.video());

	h_flag = dlg.selected_val();
	cell_.widget.cell.flags_ = (cell_.widget.cell.flags_ & ~tgrid::VERTICAL_MASK) | h_flag;

	set_vertical_layout_label(window);
}

void tcontrol_setting::set_horizontal_layout_label(twindow& window)
{
	const unsigned h_flag = cell_.widget.cell.flags_ & tgrid::HORIZONTAL_MASK;
	std::stringstream ss;

	ss << horizontal_layout.find(h_flag)->second.description;
	tlabel* label = find_widget<tlabel>(&window, "_horizontal_layout", false, true);
	label->set_label(ss.str());
}

void tcontrol_setting::set_vertical_layout_label(twindow& window)
{
	const unsigned v_flag = cell_.widget.cell.flags_ & tgrid::VERTICAL_MASK;
	std::stringstream ss;

	ss << vertical_layout.find(v_flag)->second.description;
	tlabel* label = find_widget<tlabel>(&window, "_vertical_layout", false, true);
	label->set_label(ss.str());
}

void tcontrol_setting::set_linked_group(twindow& window)
{
	std::stringstream ss;
	std::vector<tval_str> items;

	int index = -1;
	int def = -1;
	items.push_back(tval_str(index ++, ""));
	for (std::vector<tlinked_group>::const_iterator it = linkeds_.begin(); it != linkeds_.end(); ++ it) {
		const tlinked_group& linked = *it;
		ss.str("");
		ss << tintegrate::generate_format(linked.id, "yellow");
		if (linked.fixed_width) {
			ss << "  " << tintegrate::generate_format("width", "blue");
		}
		if (linked.fixed_height) {
			ss << "  " << tintegrate::generate_format("height", "blue");
		}
		if (cell_.widget.linked_group == linked.id) {
			def = index;
		}
		items.push_back(tval_str(index ++, ss.str()));
	}

	gui2::tcombo_box dlg(items, def);
	dlg.show(disp_.video());

	index = dlg.selected_val();
	if (index >= 0) {
		cell_.widget.linked_group = linkeds_[index].id;
	} else {
		cell_.widget.linked_group.clear();
	}

	set_linked_group_label(window);
}

void tcontrol_setting::set_linked_group_label(twindow& window)
{
	std::stringstream ss;

	ss << cell_.widget.linked_group;
	ttext_box* text_box = find_widget<ttext_box>(&window, "_linked_group", false, true);
	text_box->set_label(ss.str());
	text_box->set_active(false);

	if (linkeds_.empty()) {
		find_widget<tbutton>(&window, "_set_linked_group", false, true)->set_active(false);
	}
}

void tcontrol_setting::set_textdomain(twindow& window, bool label)
{
	std::stringstream ss;
	std::vector<tval_str> items;

	std::string& textdomain = label? cell_.widget.label_textdomain: cell_.widget.tooltip_textdomain;
	
	int index = -1;
	int def = -1;
	items.push_back(tval_str(index ++, ""));
	for (std::vector<std::string>::const_iterator it = textdomains_.begin(); it != textdomains_.end(); ++ it) {
		ss.str("");
		ss << *it;
		if (*it == textdomain) {
			def = index;
		}
		items.push_back(tval_str(index ++, ss.str()));
	}

	gui2::tcombo_box dlg(items, def);
	dlg.show(disp_.video());

	index = dlg.selected_val();
	if (index >= 0) {
		textdomain = textdomains_[index];
	} else {
		textdomain.clear();
	}

	set_textdomain_label(window, label);
}

void tcontrol_setting::set_textdomain_label(twindow& window, bool label)
{
	std::stringstream ss;

	const std::string id = label? "textdomain_label": "textdomain_tooltip";
	std::string str = label? cell_.widget.label_textdomain: cell_.widget.tooltip_textdomain;
	if (str.empty()) {
		str = "---";
	}

	tbutton* button = find_widget<tbutton>(&window, id, false, true);
	button->set_label(str);

	if (textdomains_.empty()) {
		button->set_active(false);
	}
}

}
