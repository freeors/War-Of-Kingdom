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

#include "gui/dialogs/technology_tree.hpp"

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
#include "gui/widgets/tree_view_node.hpp"
#include "gui/widgets/tree_view.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "help.hpp"

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

REGISTER_DIALOG(technology_tree)

ttechnology_tree::ttechnology_tree(game_display& gui, std::vector<team>& teams, unit_map& units, hero_map& heros, int side, bool browse)
	: gui_(gui)
	, teams_(teams)
	, units_(units)
	, heros_(heros)
	, browse_(browse)
	, current_team_(teams_[side - 1])
	, technology_tv_()
	, ing_technology_(NULL)
{
}

void ttechnology_tree::signal_handler_mouse_enter(ttoggle_button& widget)
{
	int toggled_index = widget.get_data();
	const ttechnology& t = unit_types.technologies().find(technology_tv_[toggled_index].first)->second;

	show_tip(t);
}

std::string ttechnology_tree::exp_str(const ttechnology& t)
{
	std::map<const ttechnology*, int>& halvies = current_team_.half_technologies();
	std::vector<const ttechnology*>& holded = current_team_.holded_technologies();
	std::map<const ttechnology*, int>::const_iterator find = halvies.find(&t);

	std::stringstream strstr;
	if (find != halvies.end()) {
		strstr << find->second;
	} else if (std::find(holded.begin(), holded.end(), &t) != holded.end()) {
		strstr << "---";
	} else {
		strstr << "0";
	}
	return strstr.str();
}

void ttechnology_tree::show_tip(const ttechnology& t)
{
	std::stringstream strstr;
	
	strstr << exp_str(t) << "/" << t.max_experience();
	int max_experience = current_team_.technology_max_experience(t);
	if (max_experience != t.max_experience()) {
		strstr << "(";
		strstr << unit_types.character(current_team_.character()).name();
		strstr << ")";
	}
		
	tcontrol* control = find_widget<tcontrol>(window_, "point", false, true);
	control->set_label(strstr.str());
	control = find_widget<tcontrol>(window_, "description", false, true);
	control->set_label(t.description());
}

void ttechnology_tree::signal_handler_mouse_leave()
{
	if (ing_technology_) {
		show_tip(*ing_technology_);
	}
}

void ttechnology_tree::technology_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	int toggled_index = toggle->get_data();
	const ttechnology& t = unit_types.technologies().find(technology_tv_[toggled_index].first)->second;

	ing_technology_ = &t;
}

void ttechnology_tree::technology_tree_2_tv_internal(ttree_view_node* htvroot, const std::vector<advance_tree::node>& advances_to, const std::vector<std::string>& advances_from2, bool base_holded)
{
	std::stringstream strstr;
	std::vector<std::string> advances_from = advances_from2;
	ttree_view_node* htvi;

	string_map tree_group_field;
	std::map<std::string, string_map> tree_group_item;

	const std::vector<const ttechnology*>& holded = current_team_.holded_technologies();
	for (std::vector<advance_tree::node>::const_iterator it = advances_to.begin(); it != advances_to.end(); ++ it) {
		const ttechnology* current = dynamic_cast<const ttechnology*>(it->current);
		bool hold = std::find(holded.begin(), holded.end(), current) != holded.end();

		strstr.str("");
		strstr << current->name() << "\n";
		strstr << "<format>";
		int max_experience = current_team_.technology_max_experience(*current);
		if (max_experience != current->max_experience()) {
			strstr << "color=blue ";
		}
		strstr << "text='(" << exp_str(*current) << "/" << max_experience << ")'</format>";
		tree_group_field["label"] = strstr.str();
		if (hold) {
			tree_group_field["icon"] = "misc/ok2.png";
		} else {
			tree_group_field["icon"] = "";
		}
		tree_group_item["tree_view_node_label"] = tree_group_field;
		htvi = &htvroot->add_child("item", tree_group_item);

		ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(htvi->find("tree_view_node_label", true));
		toggle->set_callback_state_change(boost::bind(&ttechnology_tree::technology_toggled, this, _1));
		toggle->set_data(technology_tv_.size());
		if (browse_ || hold || !base_holded) {
			toggle->set_active(false);
		}
		if (current == ing_technology_) {
			tree_->set_select_item(htvi);
		}

		toggle->connect_signal<event::MOUSE_ENTER>(boost::bind(
				&ttechnology_tree::signal_handler_mouse_enter, this, boost::ref(*toggle)),
				event::tdispatcher::front_child);
		toggle->connect_signal<event::MOUSE_LEAVE>(boost::bind(
				&ttechnology_tree::signal_handler_mouse_leave, this),
				event::tdispatcher::front_child);

		technology_tv_.push_back(std::make_pair(current->id(), advances_from));
		if (!it->advances_to.empty()) {
			advances_from.push_back(current->id());
			technology_tree_2_tv_internal(htvi, it->advances_to, advances_from, hold);
		}
	}
}

void ttechnology_tree::pre_show(CVideo& /*video*/, twindow& window)
{
	window_ = &window;
	ing_technology_ = current_team_.ing_technology();

	std::stringstream strstr;
	tlabel* label = find_widget<tlabel>(&window, "title", false, true);
	strstr << _("Technology tree") << "(" << current_team_.name() << ")";
	label->set_label(strstr.str());
	if (browse_) {
		label = find_widget<tlabel>(&window, "flag", false, true);
		strstr.str("");
		strstr << "(" << _("Browse") << ")";
		label->set_label(strstr.str());
	}

	ttree_view& parent_tree = find_widget<ttree_view>(&window, "player_tree", false);
	tree_= &parent_tree;
	parent_tree.set_left_align();

	string_map tree_group_field;
	std::map<std::string, string_map> tree_group_item;

	utils::string_map symbols;
	std::vector<std::string> advances_from;
	ttree_view_node* htvi;

	const std::vector<const ttechnology*>& holded = current_team_.holded_technologies();

	const std::vector<advance_tree::node*>& technology_tree = unit_types.technology_tree();
	for (std::vector<advance_tree::node*>::const_iterator it = technology_tree.begin(); it != technology_tree.end(); ++ it) {
		const advance_tree::node* n = *it;
		const ttechnology* current = dynamic_cast<const ttechnology*>(n->current);
		bool hold = std::find(holded.begin(), holded.end(), current) != holded.end();

		strstr.str("");
		strstr << current->name() << "\n";
		strstr << "<format>";
		int max_experience = current_team_.technology_max_experience(*current);
		if (max_experience != current->max_experience()) {
			strstr << "color=blue ";
		}
		strstr << "text='(" << exp_str(*current) << "/" << max_experience << ")'</format>";
		tree_group_field["label"] = strstr.str();
		if (hold) {
			tree_group_field["icon"] = "misc/ok2.png";
		} else {
			tree_group_field["icon"] = "";
		}
		tree_group_item["tree_view_node_label"] = tree_group_field;
		htvi = &parent_tree.add_node("item", tree_group_item);

		ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(htvi->find("tree_view_node_label", true));
		toggle->set_callback_state_change(boost::bind(&ttechnology_tree::technology_toggled, this, _1));
		toggle->set_data(technology_tv_.size());
		toggle->set_active(!browse_ && !hold);
		if (current == ing_technology_) {
			tree_->set_select_item(htvi);
		}

		toggle->connect_signal<event::MOUSE_ENTER>(boost::bind(
				&ttechnology_tree::signal_handler_mouse_enter, this, boost::ref(*toggle)),
				event::tdispatcher::front_child);
		toggle->connect_signal<event::MOUSE_LEAVE>(boost::bind(
				&ttechnology_tree::signal_handler_mouse_leave, this),
				event::tdispatcher::front_child);

		advances_from.clear();
		technology_tv_.push_back(std::make_pair(current->id(), advances_from));
		if (!n->advances_to.empty()) {
			advances_from.push_back(current->id());
			technology_tree_2_tv_internal(htvi, n->advances_to, advances_from, hold);
		}
	}
	if (ing_technology_) {
		show_tip(*ing_technology_);
	}
	if (browse_) {
		// tree_->set_active(false);
	}
}

void ttechnology_tree::post_show(twindow& window)
{
}

} // namespace gui2
