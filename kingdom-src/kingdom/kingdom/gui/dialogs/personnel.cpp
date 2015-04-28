/* $Id: player_selection.cpp 49598 2011-05-22 17:55:54Z mordante $ */
/*
   Copyright (C) 2008 - 2011 by Jörg Hinrichs <joerg.hinrichs@alice-dsl.de>
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

#include "gui/dialogs/personnel.hpp"

#include "formula_string_utils.hpp"
#include "gettext.hpp"
#include "game_display.hpp"
#include "team.hpp"
#include "artifical.hpp"
#include "gui/dialogs/helper.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/image.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/scroll_label.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "help.hpp"
#include "multiplayer.hpp"
#include "game_preferences.hpp"

#include <boost/bind.hpp>

namespace gui2 {

/*WIKI
 * @page = GUIWindowDefinitionWML
 * @order = 2_game_load
 *
 * == Load a game ==
 *
 * This shows the dialog to select and load a savegame file.
 *
 * @begin{table}{dialog_widgets}
 *
 * txtFilter & & text & m &
 *         The filter for the listbox items. $
 *
 * savegame_list & & listbox & m &
 *         List of savegames. $
 *
 * -filename & & control & m &
 *         Name of the savegame. $
 *
 * -date & & control & o &
 *         Date the savegame was created. $
 *
 * preview_pane & & widget & m &
 *         Container widget or grid that contains the items for a preview. The
 *         visible status of this container depends on whether or not something
 *         is selected. $
 *
 * -minimap & & minimap & m &
 *         Minimap of the selected savegame. $
 *
 * -imgLeader & & image & m &
 *         The image of the leader in the selected savegame. $
 *
 * -lblScenario & & label & m &
 *         The name of the scenario of the selected savegame. $
 *
 * -lblSummary & & label & m &
 *         Summary of the selected savegame. $
 *
 * @end{table}
 */

REGISTER_DIALOG(personnel)

tpersonnel::tpersonnel(display& disp, hero_map& heros)
	: disp_(disp)
	, heros_(heros)
	, original_order_()
	, order_()
	, cursel_(0)
{
}

void tpersonnel::type_selected(twindow& window, tlistbox& list, const int type)
{
	cursel_ = list.get_selected_row();

	set_3button_active(window);
}

void tpersonnel::fill_member_table(twindow& window, int cursel)
{
	std::stringstream strstr;
	int value;

	tlistbox* list = find_widget<tlistbox>(&window, "type_list", false, true);
	list->clear();

	const std::vector<tgroup::tmember>& members = group.members();
	for (std::vector<int>::iterator it = order_.begin(); it != order_.end(); ++ it) {
		const tgroup::tmember& m = members[*it];
		hero& h = *m.h;

		/*** Add list item ***/
		string_map list_item;
		std::map<std::string, string_map> list_item_item;

		strstr.str("");
		strstr << (std::distance(order_.begin(), it) + 1);
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("index", list_item));

		strstr.str("");
		strstr << h.image();
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("icon", list_item));

		// name
		strstr.str("");
		if (h.utype_ != HEROS_NO_UTYPE) {
			const unit_type* ut = unit_types.keytype(h.utype_);
			strstr << tintegrate::generate_img(ut->icon()) << "\n";
		}
		strstr << h.name();
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("name", list_item));

		// level
		strstr.str("");
		if (m.level / game_config::levels_per_rank >= 2) {
			strstr << _("rank^Gold");
		} else if (m.level / game_config::levels_per_rank >= 1) {
			strstr << _("rank^Silver");
		} else {
			strstr << _("rank^Copper");
		}
		strstr << "(" << (m.level % game_config::levels_per_rank + 1) << ")";
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("level", list_item));

		// cost
		strstr.str("");
		value = h.cost_;
		strstr << value;
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("cost", list_item));

		// leadership
		strstr.str("");
		strstr << fxptoi9(h.leadership_);
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("leadership", list_item));

		// charm
		strstr.str("");
		strstr << fxptoi9(h.charm_);
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("charm", list_item));

		// feature
		strstr.str("");
		strstr << hero::feature_str(h.feature_);
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("feature", list_item));

		// tactic
		strstr.str("");
		if (h.tactic_ != HEROS_NO_TACTIC) {
			strstr << unit_types.tactic(h.tactic_).name();
		} else if (m.base->tactic_ != HEROS_NO_TACTIC) {
			strstr << tintegrate::generate_format(unit_types.tactic(m.base->tactic_).name(), "red");
		}
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("tactic", list_item));

		list->add_row(list_item_item);
	}

	if (!order_.empty()) {
		list->select_row(cursel);
		type_selected(window, *list);
	}
	window.invalidate_layout();
}

void tpersonnel::pre_show(CVideo& /*video*/, twindow& window)
{
	std::stringstream strstr;

	const std::vector<tgroup::tmember>& members = group.members();
	for (int n = 0; n < (int)members.size(); n ++) {
		if (members[n].base->get_flag(hero_flag_employee)) {
			break;
		}
		order_.push_back(n);
	}
	original_order_ = order_;

	tlistbox* list = find_widget<tlistbox>(&window, "type_list", false, true);
	fill_member_table(window, cursel_);
	list->set_callback_value_change(dialog_callback3<tpersonnel, tlistbox, &tpersonnel::type_selected>);

	utils::string_map symbols;
	tlabel* label = find_widget<tlabel>(&window, "remark", false, true);
	symbols["tower"] = tintegrate::generate_format(game_config::tower_fix_heros - 1, "yellow");
	symbols["rpg"] = tintegrate::generate_format(game_config::rpg_fix_members, "yellow");
	strstr.str("");
	strstr << tintegrate::generate_format(_("PS"), "green") << " ";
	strstr << vgettext2("First $tower heros is fixed in tower mode. First $rpg heros can be on stage in RPG mode.", symbols);
	label->set_label(strstr.str());

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "up", false)
		, boost::bind(
		&tpersonnel::up
		, this
		, boost::ref(window)));
	find_widget<tbutton>(&window, "up", false).set_label(tintegrate::generate_img("misc/increase.png"));
	
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "down", false)
		, boost::bind(
		&tpersonnel::down
		, this
		, boost::ref(window)));
	find_widget<tbutton>(&window, "down", false).set_label(tintegrate::generate_img("misc/decrease.png"));

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "save", false)
		, boost::bind(
		&tpersonnel::save
		, this
		, boost::ref(window)));
	strstr.str("");
	strstr << tintegrate::generate_format(_("OK"), "blue");
	find_widget<tbutton>(&window, "save", false).set_label(strstr.str());

	set_3button_active(window);
}

void tpersonnel::set_3button_active(twindow& window)
{
	tlistbox* list = find_widget<tlistbox>(&window, "type_list", false, true);
	tbutton* up = find_widget<tbutton>(&window, "up", false, true);
	tbutton* down = find_widget<tbutton>(&window, "down", false, true);
	tbutton* ok = find_widget<tbutton>(&window, "save", false, true);

	int count = (int)list->get_item_count();

	up->set_active(count && cursel_);
	down->set_active(count && cursel_ < count - 1);
	
	ok->set_active(original_order_ != order_);
}

void tpersonnel::up(twindow& window)
{
	std::swap(order_[cursel_], order_[cursel_ - 1]);

	fill_member_table(window, cursel_ - 1);
}

void tpersonnel::down(twindow& window)
{
	std::swap(order_[cursel_], order_[cursel_ + 1]);

	fill_member_table(window, cursel_ + 1);
}

void tpersonnel::save(twindow& window)
{
	std::vector<std::pair<int, int> > m2;
	const std::vector<tgroup::tmember>& members = group.members();
	for (std::vector<int>::iterator it = order_.begin(); it != order_.end(); ++ it) {
		const tgroup::tmember& m = members[*it];
		hero& h = *m.h;
		m2.push_back(std::make_pair(m.base->number_, m.level));
	}

	http::membership member = http::member(disp_, heros_, http::member_resort, m2, 0, 0);
	if (member.uid >= 0) {
		group.from_local_membership(disp_, heros_, member, false);

		window.set_retval(twindow::OK);
	}
}

} // namespace gui2
