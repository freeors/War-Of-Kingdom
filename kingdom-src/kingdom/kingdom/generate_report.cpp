/* $Id: generate_report.cpp 47060 2010-10-16 18:47:30Z esr $ */
/*
   Copyright (C) 2003 - 2010 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 * @file
 * Formatted output of various stats about units and the game.
 * Used for the right sidebar and the top line of the main game-display.
 */

#include "global.hpp"

#include "actions.hpp"
#include "font.hpp"
#include "game_preferences.hpp"
#include "gettext.hpp"
#include "language.hpp"
#include "map.hpp"
#include "marked-up_text.hpp"
#include "reports.hpp"
#include "resources.hpp"
#include "team.hpp"
#include "tod_manager.hpp"
#include "game_display.hpp"
#include "artifical.hpp"
#include "play_controller.hpp"
#include "wml_exception.hpp"
#include "gui/dialogs/theme2.hpp"

#include <iostream>
#include <ctime>

namespace editor {
extern std::string selected_terrain, left_button_function;
}

namespace reports {

static char const *naps = "</span>";

report generate_report(int num,
                       const team &viewing_team, int current_side, int playing_side,
                       const map_location& loc, const map_location& mouseover,
                       const std::set<std::string> &observers,
                       const config& level, bool show_everything)
{
	unit_map& units = *resources::units;
	gamemap& map = *resources::game_map;
	std::vector<team>& teams = *resources::teams;
	play_controller& controller = *resources::controller;
	team& playing_team = teams[playing_side - 1];
	game_display& disp = *resources::screen;

	const unit *u = NULL;

	if ((num >= gui2::tgame_theme::UNIT_REPORTS_BEGIN && num < gui2::tgame_theme::UNIT_REPORTS_END) || num == gui2::tgame_theme::POSITION) {
		u = get_visible_unit(loc, viewing_team, show_everything);
		if (!u && num != gui2::tgame_theme::POSITION) {
			report r = report();
			r.type = disp.cached_report(num).type;
			return r;
		}
	}

	std::ostringstream str;
	std::ostringstream tooltip;
	using utils::signed_percent;

	switch (num) {
	case gui2::tgame_theme::UNIT_NAME:
		str << u->name();
		break;

	case gui2::tgame_theme::UNIT_TYPE: {
		if (!u->packed()) {
			str << tintegrate::generate_format(u->type_name(), "245,230,193,0");
		} else {
			str << tintegrate::generate_format(u->packee_type()->type_name(), "245,230,193,0");
			str << "(" << u->type_name() << ")";
		}
		if (game_config::tiny_gui) {
			str << "(" << hero::adaptability_str2(ftofxp12(u->adaptability_[u->arms()])) << ")";
		}
		break;
	}
	case gui2::tgame_theme::UNIT_STATUS: {
		if (!u->is_city()) {
			break;
		}
		const artifical* city = const_unit_2_artifical(u);
		str << city->field_commoners().size() + city->reside_commoners().size();
		str << "/" << city->police();
		break;
	}

	case gui2::tgame_theme::UNIT_HP: {
		str << tintegrate::generate_format(u->hitpoints(), font::color2markup(u->hp_color()));
		str	<< '/' << u->max_hitpoints();
		break;
	}
	case gui2::tgame_theme::UNIT_XP: {
		str << tintegrate::generate_format(u->experience(), font::color2markup(u->xp_color()));
		str << '/' << u->max_experience();
		break;
	}
	case gui2::tgame_theme::UNIT_SECOND: {
		if (!u->is_city()) {
			if (u->second().valid()) {
				str << u->second().name();
			}
			if (u->third().valid()) {
				str << " " << u->third().name();
			}
		} else {
			const artifical* city = const_unit_2_artifical(u);
			if (city->decree()) {
				str << city->decree()->name();
			} else {
				// str << _("None");
			}
		}
		break;
	}
	case gui2::tgame_theme::UNIT_IMAGE:
	{
		const map_location& loc = u->get_location();
		str << loc.x << "," << loc.y;
		return report(report::SURFACE, str.str(), null_str);
	}
	case gui2::tgame_theme::TIME_OF_DAY: {
		time_of_day tod;

		if (viewing_team.shrouded(mouseover)) {
			// Don't show time on shrouded tiles.
			tod = resources::tod_manager->get_time_of_day();
		} else if (viewing_team.fogged(mouseover)) {
			// Don't show illuminated time on fogged tiles.
			tod = resources::tod_manager->get_time_of_day(mouseover);
		} else {
			tod = resources::tod_manager->time_of_day_at(mouseover);
		}

		int b = tod.lawful_bonus;
		int c = tod.liminal_bonus;
		tooltip << tod.name << '\n'
			<< _("Lawful units: ") << signed_percent(b) << "\n"
			<< _("Neutral units: ") << signed_percent(0) << "\n"
			<< _("Chaotic units: ") << signed_percent(-b);

		if (tod.liminal_present)
		tooltip <<  "\n" << _("Liminal units: ") << signed_percent(c);

		std::string tod_image = tod.image;
		if (tod.lawful_bonus_modified > 0) tod_image += "~BRIGHTEN()";
		else if (tod.lawful_bonus_modified < 0) tod_image += "~DARKEN()";
		if (preferences::flip_time()) tod_image += "~FL(horiz)";

		str << tod_image;
		if (game_config::tiny_gui) {
			tooltip.str("");
		}
		break;
	}
	case gui2::tgame_theme::TURN: {
		str << unit_map::main_ticks << "," << controller.autosave_ticks() << ",";
		str << controller.turn() << "," << resources::tod_manager->number_of_turns();
		return report(report::SURFACE, str.str(), null_str);
	}
	// For the following status reports, show them in gray text
	// when it is not the active player's turn.
	case gui2::tgame_theme::GOLD: {
		// Supposes the full/"pathfind" unit map is applied
		std::stringstream ss;
		std::string color;
		int fake_gold = playing_team.gold();
		if (current_side != playing_side) {
			color = "gray";
		} else if (fake_gold < 0) {
			color = "red";
		}

		int cost_exponent = playing_team.cost_exponent();
		int hundred = cost_exponent / 100;

		ss << playing_team.gold();
		ss << "(" << hundred << "." << (cost_exponent - hundred * 100) / 10 << ")";
		if (!color.empty()) {
			str << tintegrate::generate_format(ss.str(), color);
		} else {
			str << ss.str();
		}
		break;
	}
	case gui2::tgame_theme::VILLAGES: {
		const team_data data = calculate_team_data(playing_team, playing_side);
		std::stringstream ss;
		std::string color;
		if (current_side != playing_side) {
			color = "gray";
		}
		ss << data.villages << '/';
		if (viewing_team.uses_shroud()) {
			int unshrouded_villages = 0;
			std::vector<map_location>::const_iterator i = map.villages().begin();
			for (; i != map.villages().end(); ++i) {
				if (!viewing_team.shrouded(*i))
					++unshrouded_villages;
			}
			ss << unshrouded_villages;
		} else {
			ss << map.villages().size();
		}
		if (!color.empty()) {
			str << tintegrate::generate_format(ss.str(), color);
		} else {
			str << ss.str();
		}
		break;
	}
	case gui2::tgame_theme::UPKEEP: {
		const team_data data = calculate_team_data(playing_team, playing_side);
		std::stringstream ss;
		std::string color;
		if (current_side != playing_side) {
			color = "gray";
		}
		ss << data.upkeep;
		if (!color.empty()) {
			str << tintegrate::generate_format(ss.str(), color);
		} else {
			str << ss.str();
		}
		break;
	}
	case gui2::tgame_theme::INCOME: {
		team_data data = calculate_team_data(playing_team, playing_side);
		std::stringstream ss;
		std::string color;
		if (current_side != playing_side) {
			color = "gray";
		} else if (data.net_income < 0) {
			color = "red";
		}

		ss << data.net_income;
		if (!color.empty()) {
			str << tintegrate::generate_format(ss.str(), color);
		} else {
			str << ss.str();
		}
		break;
	}
	 case gui2::tgame_theme::TECH_INCOME: {
		team_data data = calculate_team_data(playing_team, playing_side);
		std::stringstream ss;
		std::string color;
		if (current_side != playing_side) {
			color = "gray";
		}

		ss << data.technology_net_income;
		if (!color.empty()) {
			str << tintegrate::generate_format(ss.str(), color);
		} else {
			str << ss.str();
		}
		break;
	}
	case gui2::tgame_theme::TACTIC: {
		int tactic_point = 0;
		str << tactic_point;
		break;
	}

	case gui2::tgame_theme::POSITION: {
		// coordinate  [terrain] resitance
		if (!map.on_board(mouseover)) {
			break;
		}

		const t_translation::t_terrain terrain = map[mouseover];

		if (terrain == t_translation::OFF_MAP_USER)
			break;

		if (!viewing_team.shrouded(mouseover)) {
			const t_translation::t_list& underlying = map.underlying_union_terrain(terrain);

			if (map.is_village(mouseover)) {
				int owner = village_owner(mouseover, teams) + 1;
				if(owner == 0 || viewing_team.fogged(mouseover)) {
					str << map.get_terrain_info(terrain).income_description();
				} else if(owner == current_side) {
					str << map.get_terrain_info(terrain).income_description_own();
				} else if(viewing_team.is_enemy(owner)) {
					str << map.get_terrain_info(terrain).income_description_enemy();
				} else {
					str << map.get_terrain_info(terrain).income_description_ally();
				}
				str << " ";
			} else {
				str << map.get_terrain_info(terrain).description();
			}

			if (underlying.size() != 1 || underlying.front() != terrain) {
				str << " (";

				for(t_translation::t_list::const_iterator i =
						underlying.begin(); i != underlying.end(); ++i) {

				str << map.get_terrain_info(*i).name();
					if(i+1 != underlying.end()) {
						str << ",";
					}
				}
				str << ")";
			}

			// display ownership
			std::map<const map_location, int>::const_iterator it = unit_map::economy_areas_.find(mouseover);
			if (it != unit_map::economy_areas_.end()) {
				str << "(" << units.city_from_cityno(it->second)->name() << ")";
			} else if (terrain == t_translation::ECONOMY_AREA) {
				str << "(--)";
			}

			str << "    ";
			str << "\n";
		}

		str << mouseover;
		if (!u) {
			break;
		}
		if (viewing_team.shrouded(mouseover)) {
			break;
		}
		str << "    ";

		int move_cost = u->movement_cost(terrain);
		int defense = 100 - u->defense_modifier(terrain);

		if (!game_config::tiny_gui) {
			str << " ";
		}
		if (move_cost < unit_movement_type::UNREACHABLE) {
			str << "(" << defense << "%," << move_cost << ")";
		} else if (mouseover == loc) {
			str << "(" << defense << "%,-)";
		} else {
			str << "(-,-)";
		}

		break;
	}
	case gui2::tgame_theme::STRATUM: {
		std::string stratum_icon;
		if (rpg::stratum == hero_stratum_wander) {
			stratum_icon = "misc/stratum-wander.png";
		} else if (rpg::stratum == hero_stratum_citizen) {
			stratum_icon = "misc/stratum-citizen.png";
		} else if (rpg::stratum == hero_stratum_mayor) {
			stratum_icon = "misc/stratum-mayor.png";
		} else if (rpg::stratum == hero_stratum_leader) {
			stratum_icon = "misc/stratum-leader.png";
		}
		str << stratum_icon;
		break;
	}
	case gui2::tgame_theme::MERITORIOUS: {
		str << rpg::h->meritorious_;
		break;
	}
	case gui2::tgame_theme::SIDE_PLAYING: {
		std::string flag_icon = teams[playing_side-1].flag_icon();
		std::string old_rgb = game_config::flag_rgb;
		std::string new_rgb = team::get_side_color_index(playing_side);
		std::string mods = "~RC(" + old_rgb + ">" + new_rgb + ")";

		if (flag_icon.empty()) {
			flag_icon = game_config::images::flag_icon;
		}

		str << flag_icon << mods;

		if (unit::actor) {
			tooltip << unit::actor->name() << "(" << teams[unit::actor->side() - 1].name() << ")";
		} else {
			tooltip << "-----";
		}
		break;
	}

	case gui2::tgame_theme::OBSERVERS: {
		if(observers.empty()) {
			return report();
		}

		str << _("Observers:") << "\n";

		for(std::set<std::string>::const_iterator i = observers.begin(); i != observers.end(); ++i) {
			str << *i << "\n";
		}

		// return report("",game_config::images::observer,str.str());
		break;
	}

	case gui2::tgame_theme::EDITOR_SELECTED_TERRAIN:
		if (editor::selected_terrain.empty()) {
			return report();
		} else {
			str << editor::selected_terrain;
		}
		break;

	case gui2::tgame_theme::EDITOR_LEFT_BUTTON_FUNCTION:
		if (editor::left_button_function.empty()) {
			return report();
		} else {
			str << editor::left_button_function;
		}
		break;

	case gui2::tgame_theme::REPORT_COUNTDOWN: {
		int min;
		int sec;
		if (viewing_team.countdown_time() > 0){
			sec = viewing_team.countdown_time() / 1000;
			char const *end = naps;
			
			min = sec / 60;
			str << min << ":";
			sec = sec % 60;
			if (sec < 10) {
				str << "0";
			}
			str << sec << end;
			break;
		} // Intentional fall-through to REPORT_CLOCK
		  // if the time countdown isn't valid.
		  // If there is no turn time limit,
		  // then we display the clock instead.
		}
	case gui2::tgame_theme::REPORT_CLOCK: {
		time_t t = std::time(NULL);
		struct tm *lt = std::localtime(&t);
		if (lt) {
			char temp[10];
			size_t s = std::strftime(temp, 10, preferences::clock_format().c_str(), lt);
			if (s > 0) {
				str << temp;
			} else {
				return report();
			}
		} else {
			return report();
		}
		break;
	}
	default:
		str.str("");
		str << "Not impletement report type: " << num;
		VALIDATE(false, str.str());
		break;
	}
	return report(report::LABEL, str.str(), tooltip.str());
}

}