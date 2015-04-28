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

#include "gui/dialogs/employ.hpp"

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
#include "gui/dialogs/hero.hpp"
#include "integrate.hpp"

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

REGISTER_DIALOG(employ)

temploy::temploy(game_display& gui, std::vector<team>& teams, unit_map& units, hero_map& heros, play_controller& controller, std::vector<hero*>& emploies, int side, double ratio, bool browse)
	: gui_(gui)
	, teams_(teams)
	, units_(units)
	, heros_(heros)
	, controller_(controller)
	, current_team_(teams[side - 1])
	, emploies_(emploies)
	, ratio_(ratio)
	, browse_(browse)
	, cursel_(0)
{
}

void temploy::type_selected(twindow& window, tlistbox& list, const int type)
{
	cursel_ = list.get_selected_row();

	set_ok_active(window);

	refresh_tooltip(window);
}

void temploy::refresh_tooltip(twindow& window)
{
/*
	tbutton* button = find_widget<tbutton>(&window, "merit", false, true);
	button->set_active(partial_troops_.empty()? false: true);

	tscroll_label* tip = find_widget<tscroll_label>(&window, "tip", false, true);
	if (partial_troops_.empty()) {
		// It is necessary set all relative tips to empty.
		tip->set_label("");
		return;
	}
	const unit& temp = *(partial_troops_[troop_index_]);
	tip->set_label(temp.form_tip(true));
*/
}

void temploy::pre_show(CVideo& /*video*/, twindow& window)
{
	window.set_canvas_variable("border", variant("default-border"));

	std::stringstream strstr;
	int value;
	
	if (browse_) {
		tlabel* label = find_widget<tlabel>(&window, "flag", false, true);
		strstr.str("");
		strstr << "(" << _("Browse") << ")";
		label->set_label(strstr.str());
	}

	tlistbox* list = find_widget<tlistbox>(&window, "type_list", false, true);

	for (std::vector<hero*>::const_iterator it = emploies_.begin(); it != emploies_.end(); ++ it) {
		hero& h = **it;

		/*** Add list item ***/
		string_map list_item;
		std::map<std::string, string_map> list_item_item;

		strstr.str("");
		strstr << h.image();
		if (current_team_.gold() < h.cost_ * ratio_) {
			strstr << "~GS()";
		}
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

		// cost
		strstr.str("");
		value = h.cost_ * ratio_;
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
		}
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("tactic", list_item));

		list->add_row(list_item_item);

		twidget* grid = list->get_row_panel(list->get_item_count() - 1);
		twidget* widget = grid->find("human", false);
		widget->set_visible(twidget::INVISIBLE);
	}

	list->set_callback_value_change(dialog_callback3<temploy, tlistbox, &temploy::type_selected>);

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "detail", false)
		, boost::bind(
		&temploy::detail
		, this
		, boost::ref(window)));

	set_ok_active(window);

	refresh_tooltip(window);
}

void temploy::set_ok_active(twindow& window)
{
	tbutton* ok = find_widget<tbutton>(&window, "ok", false, true);
	ok->set_active(!browse_ && !emploies_.empty() && current_team_.gold() >= emploies_[cursel_]->cost_ * ratio_);
}

void temploy::detail(twindow& window)
{
	hero& h = *(emploies_[cursel_]);
	gui2::thero dlg(heros_, h);
	try {
		dlg.show(gui_.video());
	} catch (twml_exception& e) {
		e.show(gui_);
	}
}

void temploy::post_show(twindow& window)
{
}

} // namespace gui2
