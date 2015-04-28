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

#include "gui/dialogs/exile.hpp"

#include "formula_string_utils.hpp"
#include "gettext.hpp"
#include "game_display.hpp"
#include "team.hpp"
#include "artifical.hpp"
#include "gui/dialogs/helper.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/hero.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/image.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/scroll_label.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/grid.hpp"
#include "gui/widgets/toggle_panel.hpp"
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

REGISTER_DIALOG(exile)

texile::texile(display& disp, hero_map& heros, tgroup& g)
	: disp_(disp)
	, heros_(heros)
	, group_(g)
	, selected_number_(0)
	, dirty_(false)
{
}

void texile::type_selected(twindow& window, tlistbox& list, const int type)
{
	twidget* grid_ptr = list.get_row_panel(list.get_selected_row());
	selected_number_ = dynamic_cast<ttoggle_panel*>(grid_ptr)->get_data();

	set_2button_active(window);
}

void texile::fill_exile_table(twindow& window, int cursel)
{
	std::stringstream strstr;
	int value;

	tlistbox* list = find_widget<tlistbox>(&window, "type_list", false, true);
	list->clear();

	const std::vector<tgroup::tmember>& exiles = group_.exiles();
	for (std::vector<tgroup::tmember>::const_iterator it = exiles.begin(); it != exiles.end(); ++ it) {
		const tgroup::tmember& m = *it;
		hero& h = *m.h;

		/*** Add list item ***/
		string_map list_item;
		std::map<std::string, string_map> list_item_item;

		strstr.str("");
		strstr << (std::distance(exiles.begin(), it) + 1);
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
		list_item["label"] = "--";
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

		twidget* grid = list->get_row_panel(list->get_item_count() - 1);
		ttoggle_panel* toggle = dynamic_cast<ttoggle_panel*>(grid);
		toggle->set_data(h.number_);
	}

	if (cursel >= (int)exiles.size()) {
		cursel = (int)exiles.size() - 1;
	}
	if (!exiles.empty()) {
		list->select_row(cursel);
	}
	selected_number_ = exiles.size()? exiles[cursel].h->number_: HEROS_INVALID_NUMBER;

	refresh_title_flag(window);
	set_2button_active(window);

	window.invalidate_layout();
}

void texile::refresh_title_flag(twindow& window) const
{
	std::stringstream strstr;
	
	tlabel* label = find_widget<tlabel>(&window, "title", false, true);
	strstr.str("");
	strstr << _("Exile");
	label->set_label(strstr.str());

	label = find_widget<tlabel>(&window, "flag", false, true);
	strstr.str("");
	strstr << "(" << tintegrate::generate_img("misc/coin.png~SCALE(24, 24)") << group_.coin();
	strstr << "  " << tintegrate::generate_img("misc/score.png~SCALE(24, 24)") << group_.score();
	// end
	strstr << ")";
	label->set_label(strstr.str());
}

void texile::pre_show(CVideo& /*video*/, twindow& window)
{
	std::stringstream strstr;

	utils::string_map symbols;
	tlabel* label = find_widget<tlabel>(&window, "remark", false, true);
	symbols["count"] = tintegrate::generate_format(game_config::max_exile, "yellow");
	strstr.str("");
	strstr << tintegrate::generate_format(_("PS"), "green") << " ";
	strstr << vgettext2("There are at most $count exiled hero. Half heros will be on stage in Siege mode.", symbols);
	label->set_label(strstr.str());

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "join", false)
		, boost::bind(
		&texile::join
		, this
		, boost::ref(window)));
	strstr.str("");
	strstr << tintegrate::generate_format(_("Join"), "blue");
	find_widget<tbutton>(&window, "join", false).set_label(strstr.str());
	
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "discard", false)
		, boost::bind(
		&texile::discard
		, this
		, boost::ref(window)));
	strstr.str("");
	strstr << tintegrate::generate_format(_("Discard"), "blue");
	find_widget<tbutton>(&window, "discard", false).set_label(strstr.str());

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "detail", false)
		, boost::bind(
		&texile::detail
		, this
		, boost::ref(window)));

	tlistbox* list = find_widget<tlistbox>(&window, "type_list", false, true);
	fill_exile_table(window, 0);
	list->set_callback_value_change(dialog_callback3<texile, tlistbox, &texile::type_selected>);
}

void texile::set_2button_active(twindow& window)
{
	tlistbox* list = find_widget<tlistbox>(&window, "type_list", false, true);
	tbutton* join = find_widget<tbutton>(&window, "join", false, true);
	tbutton* discard = find_widget<tbutton>(&window, "discard", false, true);
	tbutton* detail = find_widget<tbutton>(&window, "detail", false, true);

	int count = (int)list->get_item_count();
	join->set_active(count);
	discard->set_active(count);
	detail->set_active(count);
}

void texile::join(twindow& window)
{
	tlistbox& list = find_widget<tlistbox>(&window, "type_list", false);

	hero& h = heros_[selected_number_];
	int coin_income = 0, score_income = -1 * game_config::score_used_draw;
	
	const tgroup::tmember& m = group_.exile(h);

	std::stringstream strstr;
	utils::string_map symbols;

	if (sum_score(group_.coin(), group_.score()) < game_config::score_used_draw) {
		symbols["score"] = tintegrate::generate_format(game_config::score_used_draw, "red");
		symbols["do"] = tintegrate::generate_format(_("Join"), "yellow");
		std::string message = vgettext2("Repertory is less than $score score, cannot $do.", symbols);
		gui2::show_message(disp_.video(), "", message);
		return;
	}
	symbols["score"] = tintegrate::generate_format(-1 * score_income, "red");
	symbols["do"] = tintegrate::generate_format(_("Join"), "yellow");
	strstr.str("");
	strstr << vgettext2("Do you want to spend $score score to $do?", symbols) << "\n\n";
	symbols["name"] = tintegrate::generate_format(h.name(), "green");
	symbols["location"] = tintegrate::generate_format(_("member^Fix"), "yellow");
	strstr << vgettext2("$name will put into $location.", symbols);
	int res = gui2::show_message(disp_.video(), "", strstr.str(), gui2::tmessage::yes_no_buttons);
	if (res == gui2::twindow::CANCEL) {
		return;
	}

	std::vector<std::pair<int, int> > m2;
	m2.push_back(std::make_pair(m.base->number_, m.level));
	http::membership member = http::exile(disp_, heros_, http::exile_tag_join, m2, coin_income, score_income);
	if (member.uid < 0) {
		return;
	}
	group_.from_local_membership(disp_, heros_, member, false);

	fill_exile_table(window, list.get_selected_row());
	dirty_ = true;
}

void texile::discard(twindow& window)
{
	tlistbox& list = find_widget<tlistbox>(&window, "type_list", false);

	hero& h = heros_[selected_number_];
	int coin_income = 0, score_income = 0;
	
	const tgroup::tmember& m = group_.exile(h);

	utils::string_map symbols;
	std::string message;
	symbols["name"] = tintegrate::generate_format(h.name(), "red");
	symbols["coin"] = tintegrate::generate_format(coin_income, "green");
	symbols["score"] = tintegrate::generate_format(score_income, "green");
	message = vgettext2("May get $coin coin and $score score, do you want to discard $name?", symbols);
	int res = gui2::show_message(disp_.video(), "", message, gui2::tmessage::yes_no_buttons);
	if (res == gui2::twindow::CANCEL) {
		return;
	}

	std::vector<std::pair<int, int> > m2;
	m2.push_back(std::make_pair(m.base->number_, m.level));
	http::membership member = http::exile(disp_, heros_, http::exile_tag_erase, m2, coin_income, score_income);
	if (member.uid < 0) {
		return;
	}
	group_.from_local_membership(disp_, heros_, member, false);

	fill_exile_table(window, list.get_selected_row());
	dirty_ = true;
}

void texile::detail(twindow& window)
{
	tlistbox& list = find_widget<tlistbox>(&window, "type_list", false);
	hero& h = heros_[selected_number_];

	gui2::thero dlg(heros_, h);
	try {
		dlg.show(disp_.video());
	} catch (twml_exception& e) {
		e.show(disp_);
	}
}

} // namespace gui2
