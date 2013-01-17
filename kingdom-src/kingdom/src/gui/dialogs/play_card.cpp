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

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "gui/dialogs/play_card.hpp"

#include "foreach.hpp"
#include "formula_string_utils.hpp"
#include "gettext.hpp"
#include "game_display.hpp"
#include "team.hpp"
#include "card.hpp"
#include "gui/dialogs/helper.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/image.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/toggle_button.hpp"
#ifdef GUI2_EXPERIMENTAL_LISTBOX
#include "gui/widgets/list.hpp"
#else
#include "gui/widgets/listbox.hpp"
#endif
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"

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

REGISTER_DIALOG(play_card)

tplay_card::tplay_card(game_display& gui, std::vector<team>& teams, unit_map& units, hero_map& heros, card_map& cards, int side, button_action* discard)
	: gui_(gui)
	, teams_(teams)
	, units_(units)
	, heros_(heros)
	, cards_(cards)
	, current_team_(teams[side - 1])
	, discard_(discard)
	, card_index_(0)
	, hero_table_(NULL)
	, window_(NULL)
{
}

void tplay_card::card_selected(twindow& window)
{
	tlistbox& list = find_widget<tlistbox>(&window, "card_list", false);

	card_index_ = list.get_selected_row();
	refresh_tooltip(window);

	int size = current_team_.holded_cards().size();
	for (int index = 0; index < size; index ++) {
		tgrid* grid_ptr = list.get_row_grid(index);
		tbutton& discard = *dynamic_cast<tbutton*>(grid_ptr->find("discard", false));
		if (index == card_index_) {
			discard.set_visible(twidget::VISIBLE);
		} else {
			discard.set_visible(twidget::INVISIBLE);
		}
	}
}

void tplay_card::refresh_tooltip(twindow& window)
{
	std::stringstream text;
	std::vector<size_t>& holded_cards = current_team_.holded_cards();
	
	if (holded_cards.empty()) {
		// It is necessary set all relative tips to empty.
		tlabel* label = find_widget<tlabel>(&window, "tip_name", false, true);
		label->set_label("");
/*
		// tip_loyalty
		label = find_widget<tlabel>(&window, "tip_loyalty", false, true);
		label->set_label("");

		// leadership
		label = find_widget<tlabel>(&window, "tip_leadership", false, true);
		label->set_label("");

		// force
		label = find_widget<tlabel>(&window, "tip_force", false, true);
		label->set_label("");

		// intellect
		label = find_widget<tlabel>(&window, "tip_intellect", false, true);
		label->set_label("");

		// politics
		label = find_widget<tlabel>(&window, "tip_politics", false, true);
		label->set_label("");

		// charm
		label = find_widget<tlabel>(&window, "tip_charm", false, true);
		label->set_label("");

		// traits
		label = find_widget<tlabel>(&window, "tip_traits", false, true);
		label->set_label("");

		// adaptability
		label = find_widget<tlabel>(&window, "tip_adaptability", false, true);
		label->set_label("");

		// abilities
		label = find_widget<tlabel>(&window, "tip_abilities", false, true);
		label->set_label("");

		// feature
		label = find_widget<tlabel>(&window, "tip_feature", false, true);
		label->set_label("");

		// attack
		label = find_widget<tlabel>(&window, "tip_attack", false, true);
		label->set_label("");
*/	
		// desc
		label = find_widget<tlabel>(&window, "tip_desc", false, true);
		label->set_label("");
		return;
	}
	card& c = cards_[holded_cards[card_index_]];
/*
	std::stringstream str;
*/
	// refresh to gui
	tlabel* label = find_widget<tlabel>(&window, "tip_name", false, true);
	label->set_label(c.name());
/*
	str.str("");
	label = find_widget<tlabel>(&window, "tip_loyalty", false, true);
	str << dgettext("wesnoth", "loyalty") << ": " << loyalty / hero_count << "/(" << text.str() << ")";
	label->set_label(str.str());

	// leadership
	label = find_widget<tlabel>(&window, "tip_leadership", false, true);
	label->set_label(lexical_cast<std::string>(temp.leadership_));

	// force
	label = find_widget<tlabel>(&window, "tip_force", false, true);
	label->set_label(lexical_cast<std::string>(temp.force_));

	// intellect
	label = find_widget<tlabel>(&window, "tip_intellect", false, true);
	label->set_label(lexical_cast<std::string>(temp.intellect_));

	// politics
	label = find_widget<tlabel>(&window, "tip_politics", false, true);
	label->set_label(lexical_cast<std::string>(temp.politics_));

	// charm
	label = find_widget<tlabel>(&window, "tip_charm", false, true);
	label->set_label(lexical_cast<std::string>(temp.charm_));

	// traits
	str.str("");
	str << dgettext("wesnoth", "Traits") << ": " << utils::join(temp.trait_names(), ", ");
	label = find_widget<tlabel>(&window, "tip_traits", false, true);
	label->set_label(str.str());

	// abilities
	str.str("");
	std::vector<std::string> abilities_tt;
	abilities_tt = temp.ability_tooltips(true);
	str << dgettext("wesnoth", "Abilities") << ": ";
	if (!abilities_tt.empty()) {
		std::vector<t_string> abilities;
		for (std::vector<std::string>::const_iterator a = abilities_tt.begin(); a != abilities_tt.end(); a += 2) {
			abilities.push_back(*a);
		}

		for (std::vector<t_string>::const_iterator a = abilities.begin(); a != abilities.end(); a++) {
			if (a != abilities.begin()) {
				str << ", ";
			}
			str << (*a);
		}
	}
	label = find_widget<tlabel>(&window, "tip_abilities", false, true);
	label->set_label(str.str());

	// feature
	str.str("");
	index = 0;
	str << dgettext("wesnoth", "feature") << ": ";
	for (int i = 0; i < HEROS_MAX_FEATURE; i ++) {
		if (unit_feature_val2(temp, i) == hero_feature_single_result) {
			if (index > 0) {
				str << ", ";
			}
			index ++;
			str << temp.master().feature_str(i);
		}
	}
	label = find_widget<tlabel>(&window, "tip_feature", false, true);
	label->set_label(str.str());

	// adaptability
	str.str("");
	str << dgettext("wesnoth", "adaptability") << ": ";
	str << hero::arms_str(temp.arms()) << "(" << hero::adaptability_str2(ftofxp12(temp.adaptability_[temp.arms()])) << ")";
	label = find_widget<tlabel>(&window, "tip_adaptability", false, true);
	label->set_label(str.str());

	// attack
	str.str("");
	std::vector<attack_type>* attacks_ptr = const_cast<std::vector<attack_type>*>(&temp.attacks());
	for (std::vector<attack_type>::const_iterator at_it = attacks_ptr->begin(); at_it != attacks_ptr->end(); ++at_it) {
		// see generate_report() in generate_report.cpp
		str << at_it->name() << " (" << dgettext("wesnoth", at_it->type().c_str()) << ")\n";

		std::string accuracy = at_it->accuracy_parry_description();
		if(accuracy.empty() == false) {
			accuracy += " ";
		}

		str << "  " << at_it->damage() << "-" << at_it->num_attacks()
			<< " " << accuracy << "- " << dgettext("wesnoth", at_it->range().c_str());

		std::string special = at_it->weapon_specials(true);
		if (!special.empty()) {
			str << "(" << special << ")";
		}
		str << "\n";
	}
	label = find_widget<tlabel>(&window, "tip_attack", false, true);
	label->set_label(str.str());
*/
	// description
	label = find_widget<tlabel>(&window, "tip_desc", false, true);
	label->set_label(c.desc());
}

void tplay_card::pre_show(CVideo& /*video*/, twindow& window)
{
	std::stringstream str;

	window_ = &window;
	tlistbox* list = find_widget<tlistbox>(&window, "card_list", false, true);

	std::vector<size_t>& holded_cards = current_team_.holded_cards();
	int card_index = 0;
	for (std::vector<size_t>::const_iterator itor = holded_cards.begin(); itor != holded_cards.end(); ++ itor, card_index ++) {
		card& c = cards_[*itor];
		/*** Add list item ***/
		string_map list_item;
		std::map<std::string, string_map> list_item_item;

		list_item["label"] = c.image();
		list_item_item.insert(std::make_pair("icon", list_item));

		// name
		str.str("");
		str << c.name();
		list_item["label"] = str.str();
		list_item_item.insert(std::make_pair("name", list_item));

		// points
		str.str("");
		str << c.points();
		list_item["label"] = str.str();
		list_item_item.insert(std::make_pair("points", list_item));

		// range
		str.str("");
		int range = c.range();
		if (range == card::NONE) {
			str << dgettext("wesnoth-card", "None");
		} else if (range == card::SINGLE) {
			str << dgettext("wesnoth-card", "Single");
		} else if (range == card::MULTI) {
			str << dgettext("wesnoth-card", "Multi");
		} 
		list_item["label"] = str.str();
		list_item_item.insert(std::make_pair("range", list_item));
/*
		// movement
		str.str("");
		str << it->movement_left() << "/" << it->total_movement();
		list_item["label"] = str.str();
		list_item_item.insert(std::make_pair("movement", list_item));

		str.str("");
		str << it->cost() * cost_exponent / 100 << "/" << it->cost() * 2 / 3;
		list_item["label"] = str.str();
		list_item_item.insert(std::make_pair("cost", list_item));
*/
		list->add_row(list_item_item);

		tgrid* grid_ptr = list->get_row_grid(card_index);
		tbutton& discard = *dynamic_cast<tbutton*>(grid_ptr->find("discard", false));
		connect_signal_mouse_left_click(
			discard
			, boost::bind(
				&tplay_card::discard
				, this
				, _3, _4
				, card_index));
		if (card_index) {
			discard.set_visible(twidget::INVISIBLE);
		} else {
			discard.set_visible(twidget::VISIBLE);
		}
	}

	list->set_callback_value_change(dialog_callback<tplay_card, &tplay_card::card_selected>);

	refresh_tooltip(window);
}

void tplay_card::post_show(twindow& window)
{
}

void tplay_card::discard(bool& handled, bool& halt, int index)
{
	twindow& window = *window_;
	tlistbox* list = find_widget<tlistbox>(&window, "card_list", false, true);

	if (!discard_->button_pressed(index)) {
		return;
	}
	list->remove_row(index);

	int size = current_team_.holded_cards().size();
	for (int i = index; i < size; i ++) {
		tgrid* grid_ptr = list->get_row_grid(i);
		tbutton& discard = *dynamic_cast<tbutton*>(grid_ptr->find("discard", false));

		disconnect_signal_mouse_left_click(
			discard
			, boost::bind(
				&tplay_card::discard
				, this
				, _3, _4
				, i + 1));

		connect_signal_mouse_left_click(
			discard
			, boost::bind(
				&tplay_card::discard
				, this
				, _3, _4
				, i));
	}

	if (!current_team_.holded_cards().empty()) {
		if (index == size) {
			index --;
		}
		list->select_row(index);
	} else {
		tbutton* ok = find_widget<tbutton>(&window, "ok", false, true);
		ok->set_active(false);
	}
	card_selected(window);

	handled = true;
	halt = true;
}

} // namespace gui2
