/* $Id: hero_list.cpp 49598 2011-05-22 17:55:54Z mordante $ */
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

#include "gui/dialogs/side_list.hpp"

#include "formula_string_utils.hpp"
#include "gettext.hpp"
#include "team.hpp"
#include "artifical.hpp"
#include "game_display.hpp"
#include "gamestatus.hpp"
#include "gui/dialogs/helper.hpp"
#include "gui/dialogs/noble_list.hpp"
#include "gui/dialogs/group.hpp"
#include "gui/dialogs/technology_tree.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/image.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/toggle_panel.hpp"
#include "gui/widgets/scroll_label.hpp"
#ifdef GUI2_EXPERIMENTAL_LISTBOX
#include "gui/widgets/list.hpp"
#else
#include "gui/widgets/listbox.hpp"
#endif
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "network.hpp"
#include "ai/manager.hpp"
#include "resources.hpp"
#include "play_controller.hpp"
#include <preferences.hpp>

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

REGISTER_DIALOG(side_list)

tside_list::tside_list(game_display& disp, std::vector<team>& teams, unit_map& units, hero_map& heros, game_state& gamestate, const config& game_config)
	: disp_(disp)
	, teams_(teams)
	, units_(units)
	, heros_(heros)
	, gamestate_(gamestate)
	, game_config_(game_config)
	, side_(-1)
	, hero_table_(NULL)
{
}

void tside_list::pre_show(CVideo& /*video*/, twindow& window)
{
	hero_table_ = find_widget<tlistbox>(&window, "hero_table", false, true);

	catalog_page(window, STATUS_PAGE, false);

	hero_table_->set_callback_value_change(dialog_callback<tside_list, &tside_list::hero_changed>);

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "status", false)
		, boost::bind(
			&tside_list::catalog_page
			, this
			, boost::ref(window)
			, (int)STATUS_PAGE
			, true));
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "military", false)
		, boost::bind(
			&tside_list::catalog_page
			, this
			, boost::ref(window)
			, (int)MILITARY_PAGE
			, true));
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "diplomatism", false)
		, boost::bind(
			&tside_list::catalog_page
			, this
			, boost::ref(window)
			, (int)DIPLOMATISM_PAGE
			, true));
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "gold", false)
		, boost::bind(
			&tside_list::catalog_page
			, this
			, boost::ref(window)
			, (int)GOLD_PAGE
			, true));
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "feature", false)
		, boost::bind(
			&tside_list::catalog_page
			, this
			, boost::ref(window)
			, (int)FEATURE_PAGE
			, true));
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "technology", false)
		, boost::bind(
			&tside_list::catalog_page
			, this
			, boost::ref(window)
			, (int)TECHNOLOGY_PAGE
			, true));
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "artifical", false)
		, boost::bind(
			&tside_list::catalog_page
			, this
			, boost::ref(window)
			, (int)ARTIFICAL_PAGE
			, true));

	if (resources::controller->is_replaying()) {
		find_widget<tbutton>(&window, "plan", false).set_active(false);
	} else {
		connect_signal_mouse_left_click(
			find_widget<tbutton>(&window, "plan", false)
			, boost::bind(
				&tside_list::catalog_page
				, this
				, boost::ref(window)
				, (int)PLAN_PAGE
				, true));
	}
}

void tside_list::post_show(twindow& window)
{
}

void tside_list::hero_changed(twindow& window)
{
	tgrid* grid_ptr = hero_table_->get_row_grid(hero_table_->get_selected_row());
	unsigned int selected_side = dynamic_cast<ttoggle_panel*>(grid_ptr->find("_toggle", true))->get_data();

	twindow::tinvalidate_layout_blocker invalidate_layout_blocker(window);

	tcontrol* portrait = find_widget<tcontrol>(&window, "portrait", false, true);

	hero* leader = teams_[selected_side].leader();
	portrait->set_label(leader->image(true));
}

void tside_list::noble_list(int n)
{
	std::vector<std::pair<int, unit*> > human_pairs;
	
	if (n == rpg::h->side_) {
		human_pairs = form_human_pairs(teams_, troop_can_appoint_noble, resources::controller);
	}
	gui2::tnoble_list dlg(teams_, units_, heros_, human_pairs, n + 1);
	try {
		dlg.show(disp_.video());
	} catch(twml_exception& e) {
		e.show(disp_);
		return;
	}
}

void tside_list::browse_group(int n)
{
	team& t = teams_[n];
	tgroup& g = runtime_groups::get(t.leader()->number_);
	gui2::tgroup2 dlg(disp_, heros_, game_config_, g, true);
	try {
		dlg.show(disp_.video());
	} catch (twml_exception& e) {
		e.show(disp_);
		return;
	}
}

void tside_list::technology_tree(int n)
{
	gui2::ttechnology_tree dlg(disp_, teams_, units_, heros_, n + 1, true);
	try {
		dlg.show(disp_.video());
	} catch(twml_exception& e) {
		e.show(disp_);
		return;
	}
}

void tside_list::fill_table(int catalog)
{
	const team& viewing_team = teams_[disp_.viewing_team()];

	std::map<int, size_t> art_map;
	art_map[hero::number_market] = 0;
	art_map[hero::number_tower] = 0;
	
	for (size_t n = 0; n != teams_.size(); ++n) {
		if (teams_[n].is_empty() && !preferences::developer()) {
			continue;
		}
		
		const bool known = viewing_team.knows_about_team(n, network::nconnections() > 0);
		const bool enemy = viewing_team.is_enemy(n + 1);
		std::stringstream str;

		const team_data data = calculate_team_data(teams_[n], n + 1);
		const team& t = teams_[n];

		/*** Add list item ***/
		string_map table_item;
		std::map<std::string, string_map> table_item_item;

		// table_item["label"] = data.teamname.empty() ? teams_[n].team_name() : data.teamname;
		table_item["label"] = teams_[n].name();
		table_item_item.insert(std::make_pair("side", table_item));

		if (catalog == STATUS_PAGE) {
			hero* leader = t.leader();
			std::string leader_name;
			leader_name = leader->name();
			table_item["label"] = leader->name();
			table_item_item.insert(std::make_pair("leader", table_item));

			str.str("");
			if (leader->noble_ != HEROS_NO_NOBLE) {
				const tnoble& noble = unit_types.noble(leader->noble_);
				str << noble.name();
			}
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("noble", table_item));

			str.str("");
			if (teams_[n].is_human()) {
				str << _("Local Player");
			} else if (teams_[n].is_ai()) {
				str << _("Computer Player");
			} else if (teams_[n].is_network()) {
				str << _("Network Player");
			} else if (teams_[n].is_empty()) {
				str << _("(Empty slot)");
			} else {
				str << _("Unknown");
			}
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("controller", table_item));

			table_item["label"] = teams_[n].uses_fog()? _("yes") : _("no");
			table_item_item.insert(std::make_pair("fog", table_item));

			table_item["label"] = teams_[n].uses_shroud()? _("yes") : _("no");
			table_item_item.insert(std::make_pair("shroud", table_item));

		} else if (catalog == MILITARY_PAGE) {
			const artifical* capital = t.capital();
			str.str("");
			if (capital) {
				str << capital->name();
			}
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("capital", table_item));

			str.str("");
			str << data.villages;
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("villages", table_item));

			str.str("");
			str << data.units;
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("units", table_item));

			str.str("");
			int navigation = teams_[n].navigation();
			str << unit_types.find(unit_types.id_from_navigation(navigation))->type_name();
			str << "(" << navigation / game_config::navigation_per_level << "." << navigation % game_config::navigation_per_level << ")";
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("navigation_civi", table_item));

			str.str("");
			str << teams_[n].holded_cards().size();
			str << "(" << help::tintegrate::generate_format(teams_[n].candidate_cards().size(), "green") << ")";
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("card", table_item));

		} else if (catalog == DIPLOMATISM_PAGE) {
			const std::map<int, diplomatism_data>& diplomatisms = teams_[n].diplomatisms();
			str.str("");
			for (std::map<int, diplomatism_data>::const_iterator it = diplomatisms.begin(); it != diplomatisms.end(); ++ it) {
				if (it->first - 1 != n && it->second.ally_) {
					str << " " << teams_[it->first - 1].name();
				}
			}
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("ally", table_item));

			str.str("");
			for (std::map<int, diplomatism_data>::const_iterator it = diplomatisms.begin(); it != diplomatisms.end(); ++ it) {
				if (it->second.forbid_turns_) {
					str << " " << teams_[it->first - 1].name() << "(" << it->second.forbid_turns_ << ")";
				}
			}
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("forbid", table_item));

		} else if (catalog == GOLD_PAGE) {
			str.str("");
			if (game_config::debug) {
				str << data.gold;
			} else if(enemy && viewing_team.uses_fog()) {
				str << "--";
			} else {
				str << data.gold;
			}
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("gold_gold", table_item));

			str.str("");
			str << data.upkeep;
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("upkeep", table_item));

			str.str("");
			str << data.net_income;
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("net_income", table_item));

			str.str("");
			str << teams_[n].start_gold();
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("start_gold", table_item));

			str.str("");
			str << teams_[n].base_income();
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("base_gold", table_item));

			str.str("");
			str << teams_[n].village_gold();
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("village_gold", table_item));

		} else if (catalog == FEATURE_PAGE) {
			hero* leader = teams_[n].leader();

			str.str("");
			if (teams_[n].character() != HEROS_NO_CHARACTER) {
				str << unit_types.character(teams_[n].character()).name();
			}
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("character", table_item));

			table_item["label"] = leader->feature_str(leader->side_feature_);
			table_item_item.insert(std::make_pair("feature", table_item));

			str.str("");
			const std::vector<arms_feature>& features = teams_[n].features();
			for (std::vector<arms_feature>::const_iterator it = features.begin(); it != features.end(); ++ it) {
				if (it != features.begin()) {
					str << ", (";
				} else {
					str << "(";
				}
				str << ">=" << it->level_;
				str << " " << hero::arms_str(it->arms_);
				str << "-->" << hero::feature_str(it->feature_) << ")";
			}
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("features", table_item));

		} else if (catalog == TECHNOLOGY_PAGE) {
			hero* leader = teams_[n].leader();
			const std::vector<const ttechnology*>& holden = teams_[n].holded_technologies();
			
			str.str("");
			str << teams_[n].total_technology_income();
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("net_income", table_item));

			str.str("");
			if (teams_[n].ing_technology()) {
				str << teams_[n].ing_technology()->name();
			} else {
				str << "---";
			}
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("researching", table_item));

			str.str("");
			str << holden.size();
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("holden", table_item));

			str.str("");
			for (std::vector<const ttechnology*>::const_iterator it = holden.begin(); it != holden.end(); ++ it) {
				const ttechnology& t = **it;
				if (!t.stratagem()) {
					continue;
				}
				if (!str.str().empty()) {
					str << ", ";
				}
				str << t.name();
			}
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("stratagem", table_item));

		} else if (catalog == ARTIFICAL_PAGE) {

			const std::vector<artifical*>& holden_cities = teams_[n].holden_cities();
			for (std::map<int, size_t>::iterator itor = art_map.begin(); itor != art_map.end(); ++ itor) {
				itor->second = 0;
			}
			for (std::vector<artifical*>::const_iterator itor = holden_cities.begin(); itor != holden_cities.end(); ++ itor) {
				artifical* city = *itor;
				std::vector<artifical*>& arts = city->field_arts();
				for (std::vector<artifical*>::const_iterator itor2 = arts.begin(); itor2 != arts.end(); ++ itor2) {
					artifical* art = *itor2;
					int master = art->type()->master();
					if (art_map.find(master) != art_map.end()) {
						art_map[master] = art_map[master] + 1;
					}
				}
			}

			for (std::map<int, size_t>::const_iterator itor = art_map.begin(); itor != art_map.end(); ++ itor) {
				str.str("");
				str << itor->second;
				table_item["label"] = str.str();
				if (itor->first == hero::number_market) {
					table_item_item.insert(std::make_pair("market", table_item));
				} else {
					table_item_item.insert(std::make_pair("tower", table_item));
				}
			}

		} else if (catalog == PLAN_PAGE) {
			const bool survived = units_.side_survived(n + 1);
			ai_plan& plan = ai::manager::get_active_ai_plan_for_side(n + 1);
			
			str.str("");
			if (survived) {
				if (teams_[n].is_ai() || rpg::stratum != hero_stratum_leader) {
					if (plan.mrs_[0].target == mr_data::TARGET_INTERIOR) {
						str << _("Interior");
					} else if (plan.mrs_[0].target == mr_data::TARGET_GUARD) {
						str << _("Guard");
					} else if (plan.mrs_[0].target == mr_data::TARGET_AGGRESS) {
						str << _("Aggress");
					} else if (plan.mrs_[0].target == mr_data::TARGET_CHAOTIC) {
						str << _("Chaotic");
					} 
				}
				str << "(" << teams_[n].city_rect().x << ", " << teams_[n].city_rect().y << ", " << teams_[n].city_rect().w << ", " << teams_[n].city_rect().h << ")";
				
			} else {
				str << "---";
			}
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("city_rectangle", table_item));

			str.str("");
			if (survived && (teams_[n].is_ai() || rpg::stratum != hero_stratum_leader)) {
				for (std::vector<mr_data>::const_iterator mr = plan.mrs_.begin(); mr != plan.mrs_.end(); ++ mr) {
					if (mr != plan.mrs_.begin()) {
						str << ", ";
					}
					str << "(" << mr->consider_rect.x << ", " << mr->consider_rect.y << ", " << mr->consider_rect.w << ", " << mr->consider_rect.h << ")";
				}
			} else {
				str << "---";
			}
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("consider_rectangle", table_item));

			str.str("");
			if (survived && (teams_[n].is_ai() || rpg::stratum != hero_stratum_leader) && !plan.mrs_[0].own_front_cities.empty()) {
				str << "(" << plan.mrs_[0].own_front_cities.size() << ")";
				str << "(" << plan.mrs_[0].own_front_cities[0]->get_location().x << ", " << plan.mrs_[0].own_front_cities[0]->get_location().y << ")";
				str << plan.mrs_[0].own_front_cities[0]->name();
			} else {
				str << "---";
			}
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("front_cities[0]", table_item));

			str.str("");
			if (survived && (teams_[n].is_ai() || rpg::stratum != hero_stratum_leader) && !plan.mrs_[0].enemy_cities.empty()) {
				str << "(" << plan.mrs_[0].enemy_cities.size() << ")";
				str << "(" << plan.mrs_[0].enemy_cities[0]->get_location().x << ", " << plan.mrs_[0].enemy_cities[0]->get_location().y << ")";
				str << plan.mrs_[0].enemy_cities[0]->name();
			} else {
				str << "---";
			}

/*
			plan.mrs_[0].calculate_mass(units_, teams_[n]);
			str.str("");
			for (std::vector<tmess_data>::const_iterator it = plan.mrs_[0].messes.begin(); it != plan.mrs_[0].messes.end(); ++ it) {
				if (it != plan.mrs_[0].messes.begin()) {
					str << "\n";
				}
				const tmess_data& mess = *it;
				str << "[" << mess.selfs.size() << "{" << mess.cumulate_x / mess.selfs.size() << ", " << mess.cumulate_y / mess.selfs.size() << "}";
				str << mess.allys << ",A " << mess.friend_arts << ", " << mess.enemies << ",A " << mess.enemy_arts;
				str << "]";
				for (std::map<unit*, tmess_data::tadjacent_data>::const_iterator it2 = mess.selfs.begin(); it2 != mess.selfs.end(); ++ it2) {
					str << "(" << it2->first->name() << ", " << it2->second.friends << ", " << it2->second.unhides << ", " << it2->second.enemies << ")";
				}
			}
*/

			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("enemy_cities[0]", table_item));
		}
		hero_table_->add_row(table_item_item);
		tgrid* grid_ptr = hero_table_->get_row_grid(hero_table_->get_item_count() - 1);
		ttoggle_panel* toggle = dynamic_cast<ttoggle_panel*>(grid_ptr->find("_toggle", true));
		toggle->set_data(n);

		if (catalog == STATUS_PAGE) {
			connect_signal_mouse_left_click(
				find_widget<tbutton>(grid_ptr, "noble", true)
				, boost::bind(
					&tside_list::noble_list
					, this
					, (int)n));
			if (teams_[n].leader()->noble_ == HEROS_NO_NOBLE) {
				find_widget<tbutton>(grid_ptr, "noble", true).set_active(false);
			}

			connect_signal_mouse_left_click(
				find_widget<tbutton>(grid_ptr, "group", true)
				, boost::bind(
					&tside_list::browse_group
					, this
					, (int)n));
			find_widget<tbutton>(grid_ptr, "group", true).set_active(runtime_groups::get(teams_[n].leader()->number_).valid());

		} else if (catalog == TECHNOLOGY_PAGE) {
			connect_signal_mouse_left_click(
				find_widget<tbutton>(grid_ptr, "technology_tree", true)
				, boost::bind(
					&tside_list::technology_tree
					, this
					, (int)n));
		}
	}
}

void tside_list::catalog_page(twindow& window, int catalog, bool swap)
{
	if (catalog < MIN_PAGE || catalog > MAX_PAGE) {
		return;
	}
	int index = catalog - MIN_PAGE;

	if (window.alternate_index() == index) {
		// desired page is the displaying page, do nothing.
		return;
	}
	
	int selected_row = 0;
	if (swap) {
		selected_row = hero_table_->get_selected_row();
	}

	window.alternate_uh(hero_table_, index);

	fill_table(catalog);

	if (swap) {
		window.alternate_bh(hero_table_, index);
		hero_table_->select_row(selected_row);
	} else {
		hero_changed(window);
	}
}

} // namespace gui2
