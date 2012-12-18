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
#include "foreach.hpp"
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

#include <iostream>
#include <ctime>

#ifndef DISABLE_EDITOR
namespace editor {
extern std::string selected_terrain, left_button_function;
}
#endif

namespace reports {

static void add_status(report &r,
	char const *path, char const *desc1, char const *desc2)
{
	std::ostringstream s;
	s << gettext(desc1) << gettext(desc2);
	r.add_image(path, s.str());
}

static std::string flush(std::ostringstream &s)
{
	std::string r(s.str());
	s.str(std::string());
	return r;
}

static char const *naps = "</span>";

report generate_report(TYPE type,
                       const team &viewing_team, int current_side, int playing_side,
                       const map_location& loc, const map_location& mouseover,
                       const std::set<std::string> &observers,
                       const config& level, bool show_everything)
{
	unit_map& units = *resources::units;
	gamemap& map = *resources::game_map;
	std::vector<team>& teams = *resources::teams;

	const unit *u = NULL;

	if ((int(type) >= int(UNIT_REPORTS_BEGIN) && int(type) < int(UNIT_REPORTS_END)) || type == POSITION){
		u = get_visible_unit(loc, viewing_team, show_everything);
		if (!u && type != POSITION) {
			return report();
		}
	}

	std::ostringstream str;
	std::ostringstream tooltip;
	using utils::signed_percent;
	using font::span_color;

	switch(type) {
	case UNIT_NAME:
		// str << font::SMALL_TEXT << u->name();
		str << u->name();

		tooltip << _("Name: ") << u->name();

		return report(str.str(), "", tooltip.str());
	case UNIT_TYPE: {
		if (!u->packed()) {
			str << "<245,230,193>" << u->type_name();
		} else {
			str << "<245,230,193>" << u->packee_type()->type_name();
			str << "[" << u->type_name() << "]";
		}
		if (game_config::tiny_gui) {
			str << "(" << hero::adaptability_str2(ftofxp12(u->adaptability_[u->arms()])) << ")";
		}

		tooltip << _("Type: ")
			<< u->type_name();

		const std::string help_page = "unit_" + u->type_id();

		return report(str.str(), "", tooltip.str(), help_page);
	}
	case UNIT_RACE: {
		str << "<166,146,117>" << u->race()->name(u->gender());

		const std::string help_page = "..race_" + u->race()->id();

		return report(str.str(), "", tooltip.str(), help_page);
	}
	case UNIT_SIDE: {
		std::string flag_icon = teams[u->side() - 1].flag_icon();
		std::string old_rgb = game_config::flag_rgb;
		std::string new_rgb = team::get_side_color_index(u->side());
		std::string mods = "~RC(" + old_rgb + ">" + new_rgb + ")";

		if(flag_icon.empty()) {
			flag_icon = game_config::images::flag_icon;
		}

		image::locator flag_icon_img(flag_icon, mods);
		return report("", flag_icon_img, teams[u->side() - 1].current_player());
	}
	case UNIT_LEVEL: {
		str << u->level();

		tooltip << _("Level: ")
			<< "<b>" << u->level() << "</b>\n";

		const std::vector<std::string>& adv_to = u->advances_to();
		if(adv_to.empty()) {
			tooltip << _("No advancement");
		} else {
			tooltip << _("Advances to:") << "\n"
					<< "<b>\t" << utils::join(adv_to, "\n\t") << "</b>";
		}

		return report(str.str(), "", tooltip.str());
	}
	case UNIT_AMLA: {
		report res;
		typedef std::pair<std::string, std::string> pair_string;
		foreach(const pair_string& ps, u->amla_icons()) {
			res.add_image(ps.first,ps.second);
		}
		return(res);
	}
	case UNIT_TRAITS: {
		report res;
		const std::vector<t_string>& traits = u->trait_names();
		unsigned int nb = traits.size();
		for(unsigned int i = 0; i < nb; ++i) {
			str << traits[i];
			if(i != nb - 1 )
				str << ", ";
			tooltip << _("Trait: ")
				<< "<b>" << traits[i] << "</b>\n";

			res.add_text(flush(str), flush(tooltip));
		}

		return res;
	}
	case UNIT_STATUS: {
		report res;
		if (map.on_board(loc) && u->invisible(loc)) {
			add_status(res, "misc/invisible.png", N_("invisible: "),
				N_("This unit is invisible. It cannot be seen or attacked by enemy units."));
		}
		if (u->get_state(unit::STATE_SLOWED)) {
			add_status(res, "misc/slowed.png", N_("slowed: "),
				N_("This unit has been slowed. It will only deal half its normal damage when attacking and its movement cost is doubled."));
		}
		if (u->get_state(unit::STATE_BROKEN)) {
			add_status(res, "misc/broken.png", N_("broken: "),
				N_("This unit has been broken. It will suffer 1.5 times its normal damage when defending."));
		}
		if (u->get_state(unit::STATE_POISONED)) {
			add_status(res, "misc/poisoned.png", N_("poisoned: "),
				N_("This unit is poisoned. It will lose 8 HP every turn until it can seek a cure to the poison in a village or from a friendly unit with the 'cures' ability.\n\nUnits cannot be killed by poison alone. The poison will not reduce it below 1 HP."));
		}
		if (u->get_state(unit::STATE_PETRIFIED)) {
			add_status(res, "misc/petrified.png", N_("petrified: "),
				N_("This unit has been petrified. It may not move or attack."));
		}
		return res;
	}

	case UNIT_ALIGNMENT: {
		const std::string &align = unit_type::alignment_description(u->alignment(), u->gender());
		const std::string &align_id = unit_type::alignment_id(u->alignment());
		int cm = combat_modifier(loc, u->alignment(), u->is_fearless());

		str << align << " (" << signed_percent(cm) << ")";
		tooltip << _("Alignment: ")
			<< "<b>" << align << "</b>\n"
			<< string_table[align_id + "_description"];
		return report(str.str(), "", tooltip.str(), "time_of_day");
	}
	case UNIT_ABILITIES: {
		report res;
		const std::vector<std::string> &abilities = u->ability_tooltips();
		for(std::vector<std::string>::const_iterator i = abilities.begin(); i != abilities.end(); ++i) {
			const std::string& name = gettext(i->c_str());
			str << name;
			if(i+2 != abilities.end())
				str << ", ";
			++i;
			//FIXME pull out ability's name from description
			tooltip << _("Ability: ")
				<< *i;
			const std::string help_page = "ability_" + name;

			res.add_text(flush(str), flush(tooltip), help_page);
		}

		return res;
	}
	case UNIT_HP: {
		str << font::color2markup(u->hp_color()) << u->hitpoints()
			<< '/' << u->max_hitpoints();

		return report(str.str(), "", tooltip.str());
	}
	case UNIT_XP: {
		str << font::color2markup(u->xp_color()) << u->experience()
			<< '/' << u->max_experience();

		return report(str.str(), "", tooltip.str());
	}
	case UNIT_ADVANCEMENT_OPTIONS: {
		report res;
		typedef std::pair<std::string, std::string> pair_string;
		foreach(const pair_string& ps, u->advancement_icons()){
			res.add_image(ps.first,ps.second);
		}
		return res;
	}
	case UNIT_WEAPONS: {
		if (u->second().valid()) {
			str << u->second().name();
		}
		if (u->third().valid()) {
			str << " " << u->third().name();
		}
		return report(str.str(), "", tooltip.str());
	}
	case UNIT_IMAGE:
	{
//		const std::vector<Uint32>& old_rgb = u->second.team_rgb_range();
//		color_range new_rgb = team::get_side_color_range(u->second.side());

		report res("", image::locator(u->absolute_image(), u->image_mods()), "");
		if (!u->is_artifical() && u->character() != NO_CHARACTER) {
			res.add_image(unit_types.character(u->character()).image_, "");
			res.back().rect.w = 16;
			res.back().rect.h = 16;
		}
		return res;
	}
	case TIME_OF_DAY: {
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

		if (game_config::tiny_gui) {
			return report("", tod_image, "");
		} else {
			return report("", tod_image, tooltip.str(), "time_of_day");
		}
	}
	case TURN: {
		str << 	teams[playing_side-1].name() << "[" << resources::tod_manager->turn() << "]";
		int nb = resources::tod_manager->number_of_turns();
		if (nb != -1) str << '/' << nb;
		break;
	}
	// For the following status reports, show them in gray text
	// when it is not the active player's turn.
	case GOLD: {
		//Supposes the full/"pathfind" unit map is applied
		int fake_gold = viewing_team.gold();
		if (current_side != playing_side) {
			str << font::GRAY_TEXT;
		} else if (fake_gold < 0) {
			str << font::RED_TEXT;
		}

		int cost_exponent = viewing_team.cost_exponent();
		int hundred = cost_exponent / 100;

		str << viewing_team.gold() << "(" << hundred << "." << (cost_exponent - hundred * 100) / 10 << ")";
		break;
	}
	case VILLAGES: {
		const team_data data = calculate_team_data(viewing_team,current_side);
		if (current_side != playing_side)
			str << font::GRAY_TEXT;
		str << data.villages << '/';
		if (viewing_team.uses_shroud()) {
			int unshrouded_villages = 0;
			std::vector<map_location>::const_iterator i = map.villages().begin();
			for (; i != map.villages().end(); ++i) {
				if (!viewing_team.shrouded(*i))
					++unshrouded_villages;
			}
			str << unshrouded_villages;
		} else {
			str << map.villages().size();
		}
		break;
	}
	case UPKEEP: {
		const team_data data = calculate_team_data(viewing_team,current_side);
		if (current_side != playing_side)
			str << font::GRAY_TEXT;
		str << data.expenses << "(" << data.upkeep << ")";
		break;
	}
	case EXPENSES: {
		const team_data data = calculate_team_data(viewing_team,current_side);
		if (current_side != playing_side)
			str << font::GRAY_TEXT;
		str << data.expenses;
		break;
	}
	case INCOME: {
		team_data data = calculate_team_data(viewing_team, current_side);
		if (current_side != playing_side)
			str << font::GRAY_TEXT;
		else if (data.net_income < 0)
			str << font::BAD_TEXT;

		str << data.net_income;
		break;
	}
	case TACTIC: {
		int tactic_point = viewing_team.tactic_point();
		if (current_side != playing_side) {
			str << font::GRAY_TEXT;
		} else if (tactic_point >= game_config::max_tactic_point) {
			str << font::RED_TEXT;
		} else if (tactic_point > game_config::max_tactic_point * 2 / 3) {
			str << "<255,255,0>";
		} else {
			str << font::GOOD_TEXT;
		}

		str << viewing_team.tactic_point();
		break;
	}
	case TERRAIN: {
		if(!map.on_board(mouseover) || viewing_team.shrouded(mouseover))
			break;

		const t_translation::t_terrain terrain = map.get_terrain(mouseover);
		if (terrain == t_translation::OFF_MAP_USER)
			break;

		const t_translation::t_list& underlying = map.underlying_union_terrain(terrain);

		if(map.is_village(mouseover)) {
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

		if(underlying.size() != 1 || underlying.front() != terrain) {
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
		break;
	}
	case POSITION: {
		// coordinate  [terrain] resitance
		if (!map.on_board(mouseover)) {
			break;
		}

		const t_translation::t_terrain terrain = map[mouseover];

		if (terrain == t_translation::OFF_MAP_USER)
			break;

		str << mouseover;
		if (game_config::tiny_gui) {
			str << "\n";
		}

		if (!viewing_team.shrouded(mouseover)) {
			const t_translation::t_list& underlying = map.underlying_union_terrain(terrain);

			if (!game_config::tiny_gui) {
				str << _("Terrains") << ":";
			}

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
			} else if (terrain == t_translation::TERRAIN_ECONOMY_AREA) {
				str << "(--)";
			}

			str << "    ";
			if (game_config::tiny_gui) {
				str << "\n";
			}
		}

		if (!u)
			break;
		if (viewing_team.shrouded(mouseover))
			break;

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
	case STRATUM: {
		const hero* rpg_hero = rpg::h;
		std::string stratum_icon;
		if (rpg::stratum == hero_stratum_wander) {
			stratum_icon = "/misc/stratum-wander.png";
		} else if (rpg::stratum == hero_stratum_citizen) {
			stratum_icon = "/misc/stratum-citizen.png";
		} else if (rpg::stratum == hero_stratum_mayor) {
			stratum_icon = "/misc/stratum-mayor.png";
		} else if (rpg::stratum == hero_stratum_leader) {
			stratum_icon = "/misc/stratum-leader.png";
		}
		image::locator stratum_icon_img(stratum_icon);
		return report("", stratum_icon_img, "");
	}
	case MERITORIOUS: {
		str << rpg::h->meritorious_;
		break;
	}
	case SIDE_PLAYING: {
		std::string flag_icon = teams[playing_side-1].flag_icon();
		std::string old_rgb = game_config::flag_rgb;
		std::string new_rgb = team::get_side_color_index(playing_side);
		std::string mods = "~RC(" + old_rgb + ">" + new_rgb + ")";

		if(flag_icon.empty()) {
			flag_icon = game_config::images::flag_icon;
		}

		image::locator flag_icon_img(flag_icon, mods);
		return report("", flag_icon_img, teams[playing_side-1].name());
	}

	case OBSERVERS: {
		if(observers.empty()) {
			return report();
		}

		str << _("Observers:") << "\n";

		for(std::set<std::string>::const_iterator i = observers.begin(); i != observers.end(); ++i) {
			str << *i << "\n";
		}

		return report("",game_config::images::observer,str.str());
	}
#ifdef DISABLE_EDITOR
	case EDITOR_SELECTED_TERRAIN:
	case EDITOR_LEFT_BUTTON_FUNCTION:
		return report();
#else
	case EDITOR_SELECTED_TERRAIN: {
		if (editor::selected_terrain.empty())
			return report();
		else
			return report(editor::selected_terrain);
	}
	case EDITOR_LEFT_BUTTON_FUNCTION: {
		if (editor::left_button_function.empty())
			return report();
		else
			return report(editor::left_button_function);
	}
#endif
	case REPORT_COUNTDOWN: {
		int min;
		int sec;
		if (viewing_team.countdown_time() > 0){
			sec = viewing_team.countdown_time() / 1000;
			char const *end = naps;
			if (current_side != playing_side)
				str << span_color(font::GRAY_COLOR);
			else if (sec < 60)
				str << "<span foreground=\"#c80000\">";
			else if (sec < 120)
				str << "<span foreground=\"#c8c800\">";
			else
				end = "";

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
	case REPORT_CLOCK: {
		time_t t = std::time(NULL);
		struct tm *lt = std::localtime(&t);
		if (lt) {
			char temp[10];
			size_t s = std::strftime(temp, 10, preferences::clock_format().c_str(), lt);
			if(s>0) {
				return report(temp);
			} else {
				return report();
			}
		} else {
			return report();
		}
	}
	default:
		assert(false);
		break;
	}
	return report(str.str());
}

} // end namespace reports

