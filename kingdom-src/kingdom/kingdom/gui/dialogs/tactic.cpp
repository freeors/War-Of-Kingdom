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

#define GETTEXT_DOMAIN "kingdom-lib"

#include "gui/dialogs/tactic.hpp"

#include "gui/dialogs/helper.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/listbox.hpp"
#include <boost/bind.hpp>
#include "game_display.hpp"
#include "team.hpp"
#include "unit_map.hpp"
#include "gamestatus.hpp"
#include "play_controller.hpp"
#include "resources.hpp"
#include "marked-up_text.hpp"
#include "gettext.hpp"

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

REGISTER_DIALOG(tactic2)

ttactic2::ttactic2(game_display& gui, std::vector<team>& teams, unit_map& units, hero_map& heros, unit& tactician, int operate)
	: gui_(gui)
	, teams_(teams)
	, units_(units)
	, heros_(heros)
	, tactician_(tactician)
	, operate_(operate)
	, candidate_()
	, valid_()
	, cannot_valid_(false)
	, selected_(-1)
{
	if (tactician_.master().tactic_ != HEROS_NO_TACTIC) {
		candidate_.push_back(&tactician_.master());
	}
	if (tactician_.second().valid() && tactician_.second().tactic_ != HEROS_NO_TACTIC) {
		candidate_.push_back(&tactician_.second());
	}
	if (tactician_.third().valid() && tactician_.third().tactic_ != HEROS_NO_TACTIC) {
		candidate_.push_back(&tactician_.third());
	}
	if (operate_ == ACTIVE) {
		std::vector<unit*> actives = teams_[tactician.side() - 1].active_tactics();
		if ((int)actives.size() >= game_config::active_tactic_slots || 
			std::find(actives.begin(), actives.end(), &tactician) != actives.end()) {
			cannot_valid_ = true;
		}
	}
}

void ttactic2::tactic_selected(twindow& window, tlistbox& list, const int type)
{
	selected_ = list.get_selected_row();

	tbutton* ok = find_widget<tbutton>(&window, "ok", false, true);
	ok->set_active(valid_[selected_]);
}

std::string generate_image_desc(const std::map<int, int>& effect)
{
	std::stringstream strstr;

	for (std::map<int, int>::const_iterator it = effect.begin(); it != effect.end(); ++ it) {
		int apply_to = it->first;
		int level = it->second;
		std::stringstream img;

		if (apply_to == apply_to_tag::ATTACK) {
			strstr << tintegrate::generate_img("misc/attack.png");
			img << "misc/digit.png~CROP(" << ((level > 0? level: -1 * level) * 8) << ", 0, 8, 12)";
			strstr << tintegrate::generate_img(img.str(), tintegrate::BACK);
			strstr << tintegrate::generate_img(level > 0? "misc/mini-increase.png": "misc/mini-decrease.png", tintegrate::BACK);

		} else if (apply_to == apply_to_tag::RESISTANCE) {
			strstr << tintegrate::generate_img("misc/resistance.png");
			img << "misc/digit.png~CROP(" << ((level > 0? level: -1 * level) * 8) << ", 0, 8, 12)";
			strstr << tintegrate::generate_img(img.str(), tintegrate::BACK);
			strstr << tintegrate::generate_img(level > 0? "misc/mini-increase.png": "misc/mini-decrease.png", tintegrate::BACK);

		} else if (apply_to == apply_to_tag::MOVEMENT) {
			strstr << tintegrate::generate_img("misc/movement.png");
			strstr << tintegrate::generate_img(level > 0? "misc/mini-increase.png": "misc/mini-decrease.png", tintegrate::BACK);

		} else if (apply_to == apply_to_tag::ENCOURAGE) {
			strstr << tintegrate::generate_img("misc/encourage.png");
			img << "misc/digit.png~CROP(" << ((level > 0? level: -1 * level) * 8) << ", 0, 8, 12)";
			strstr << tintegrate::generate_img(img.str(), tintegrate::BACK);
			strstr << tintegrate::generate_img(level > 0? "misc/mini-increase.png": "misc/mini-decrease.png", tintegrate::BACK);

		} else if (apply_to == apply_to_tag::DEMOLISH) {
			strstr << tintegrate::generate_img("misc/demolish.png");
			img << "misc/digit.png~CROP(" << ((level > 0? level: -1 * level) * 8) << ", 0, 8, 12)";
			strstr << tintegrate::generate_img(img.str(), tintegrate::BACK);
			strstr << tintegrate::generate_img(level > 0? "misc/mini-increase.png": "misc/mini-decrease.png", tintegrate::BACK);

		} else if (apply_to == apply_to_tag::DAMAGE) {
			img << "misc/resistance-" << unit_types.atype_ids()[level] << ".png";
			strstr << tintegrate::generate_img(img.str());
			strstr << tintegrate::generate_img("misc/mini-decrease.png", tintegrate::BACK);

		} else if (apply_to == apply_to_tag::ALERT) {
			strstr << tintegrate::generate_img("misc/alert.png");

		} else if (apply_to == apply_to_tag::HIDE) {
			strstr << tintegrate::generate_img("misc/hide.png");

		} else if (apply_to == apply_to_tag::PROVOKE) {
			strstr << tintegrate::generate_img("misc/provoked.png");

		} else if (apply_to == apply_to_tag::CLEAR) {
			strstr << tintegrate::generate_img("misc/clear.png");

		} else if (apply_to == apply_to_tag::HEAL) {
			strstr << tintegrate::generate_img("misc/heal.png");

		} else if (apply_to == apply_to_tag::ACTION) {
			strstr << tintegrate::generate_img("misc/action.png");
			img << "misc/digit.png~CROP(" << ((level > 0? level: -1 * level) * 8) << ", 0, 8, 12)";
			strstr << tintegrate::generate_img(img.str(), tintegrate::BACK);
			strstr << tintegrate::generate_img(level > 0? "misc/mini-increase.png": "misc/mini-decrease.png", tintegrate::BACK);

		} else if (apply_to == apply_to_tag::REVIVAL) {
			strstr << tintegrate::generate_img("misc/revival.png");

		}
	}
	return strstr.str();
}

void ttactic2::pre_show(CVideo& /*video*/, twindow& window)
{
	window.set_canvas_variable("border", variant("default-border"));

	std::vector<hero*> captain;
	std::stringstream strstr;
	std::string color;
	const team& current_team = teams_[tactician_.side() - 1];

	tlabel* title = find_widget<tlabel>(&window, "title", false, true);
	tbutton* ok = find_widget<tbutton>(&window, "ok", false, true);
	if (operate_ == CAST) {
		strstr.str("");
		strstr << _("Cast tactic");
		if (current_team.allow_intervene) {
			strstr << " (" << current_team.gold() << ")";
		}
		title->set_label(strstr.str());

		ok->set_label(_("tactic^Cast"));
	} else if (operate_ == ACTIVE) {
		title->set_label(_("Active tactic"));
		ok->set_label(_("tactic^Active"));
	} else {
		ok->set_visible(twidget::INVISIBLE);
	}

	tlistbox& tactic_list = find_widget<tlistbox>(&window, "tactic_list", false);
	std::map<int, std::vector<map_location> > touched;
	bool has_effect_unit;

	for (std::vector<hero*>::iterator it = candidate_.begin(); it != candidate_.end(); ++ it) {
		hero& h = **it;
		const ttactic& t = unit_types.tactic(h.tactic_);
		bool can_cast = tactician_.tactic_degree() >= t.point() * game_config::tactic_degree_per_point;
		if (!can_cast) {
			if (current_team.allow_intervene && (tactician_.hot_turns() || current_team.gold() >= t.cost())) {
				can_cast = true;
			}
		}

		std::map<std::string, string_map> data;
		string_map item;

		has_effect_unit = false;
		touched = t.touch_units(units_, tactician_);
		for (std::map<int, std::vector<map_location> >::const_iterator it2 = touched.begin(); it2 != touched.end(); ++ it2) {
			if (!it2->second.empty()) {
				has_effect_unit =true;
				break;
			}
		}

		strstr.str("");
		strstr << h.image();
		if (operate_ == CAST && (tactician_.provoked_turns() || !has_effect_unit)) {
			strstr << "~GS()";
		}
		item["label"] = strstr.str();
		data.insert(std::make_pair("icon", item));

		strstr.str("");
		if (!can_cast) {
			color = "red";
		} else {
			color = "";
		}
		if (operate_ != CAST) {
			strstr << "--/";
		}
		strstr << tintegrate::generate_format(t.point(), color, 16, true);
		if (current_team.allow_intervene && tactician_.tactic_degree() < t.point() * game_config::tactic_degree_per_point && !tactician_.hot_turns()) {
			strstr << "\n";
			strstr << tintegrate::generate_img("misc/gold.png");
			strstr << tintegrate::generate_format(t.cost(), "", 16, true);
		}
		item["label"] = strstr.str();
		data.insert(std::make_pair("point", item));

		item["label"] = t.name();
		data.insert(std::make_pair("name", item));

		strstr.str("");
		strstr << tintegrate::generate_format(ttactic::calculate_turn(fxptoi9(h.force_), fxptoi9(h.intellect_)), "blue", 16);
		item["label"] = strstr.str();
		data.insert(std::make_pair("turn", item));

		std::map<int, int> self_effect, friend_effect, enemy_effect;
		const std::vector<std::string>& atype_ids = unit_types.atype_ids();
		const std::vector<const ttactic*>& parts = t.parts();
		for (std::vector<const ttactic*>::const_iterator it2 = parts.begin(); it2 != parts.end(); ++ it2) {
			const ttactic& part = **it2;
			int range = part.range();
			const config& effect_cfg = part.effect_cfg();
			
			int apply_to = part.apply_to();
			int level = 0;
			if (part.apply_to() == apply_to_tag::ATTACK) {
				level = effect_cfg["increase_damage"].to_int();

			} else if (part.apply_to() == apply_to_tag::RESISTANCE) {
				const config& resistance_cfg = effect_cfg.child("resistance");
				BOOST_FOREACH (const config::attribute &i, resistance_cfg.attribute_range()) {
					int resistance = i.second.to_int();
					if (resistance == 35) {
						level = 3;
					} else if (resistance == 25) {
						level = 2;
					} else if (resistance == 15) {
						level = 1;
					} else if (resistance == -15) {
						level = -1;
					} else if (resistance == -25) {
						level = -2;
					} else {
						level = -3;
					}
					break;
				}

			} else if (part.apply_to() == apply_to_tag::MOVEMENT) {
				level = effect_cfg["increase"].to_int();

			} else if (part.apply_to() == apply_to_tag::ENCOURAGE) {
				level = effect_cfg["increase"].to_int() / 2;

			} else if (part.apply_to() == apply_to_tag::DEMOLISH) {
				level = effect_cfg["increase"].to_int() / 2;
				
			} else if (part.apply_to() == apply_to_tag::DAMAGE) {
				std::vector<std::string>::const_iterator find = std::find(atype_ids.begin(), atype_ids.end(), effect_cfg["type"].str());
				level = std::distance(atype_ids.begin(), find);
				
			} else if (part.apply_to() == apply_to_tag::ACTION) {
				level = effect_cfg["increase"].to_int();
				if (level == -40) {
					level = -2;
				} else if (level == -20) {
					level = -1;
				} else if (level == 20) {
					level = 1;
				} else if (level == 40) {
					level = 2;
				}
				
			}
			if (range == ttactic::SELF) {
				self_effect.insert(std::make_pair(apply_to, level));
			} else if (range == ttactic::FRIEND) {
				friend_effect.insert(std::make_pair(apply_to, level));
			} else {
				enemy_effect.insert(std::make_pair(apply_to, level));
			}
		}
		strstr.str("");
		if (!self_effect.empty()) {
			strstr << tintegrate::generate_img("misc/range-self.png") << "  " << generate_image_desc(self_effect);
		}
		if (!friend_effect.empty()) {
			if (!strstr.str().empty()) {
				strstr << "\n";
			}
			strstr << tintegrate::generate_img("misc/range-friend.png") << "  " << generate_image_desc(friend_effect);
		}
		if (!enemy_effect.empty()) {
			if (!strstr.str().empty()) {
				strstr << "\n";
			}
			strstr << tintegrate::generate_img("misc/range-enemy.png") << "  " << generate_image_desc(enemy_effect);
		}
		item["label"] = strstr.str();
		data.insert(std::make_pair("effect", item));

		if (cannot_valid_) {
			valid_.push_back(false);
		} else if (operate_ == CAST) {
			valid_.push_back(!tactician_.provoked_turns() && has_effect_unit && can_cast);
		} else if (operate_ != ACTIVE) {
			valid_.push_back(!tactician_.provoked_turns());
		} else {
			valid_.push_back(true);
		}
		tactic_list.add_row(data);
	}

	tactic_list.set_callback_value_change(dialog_callback3<ttactic2, tlistbox, &ttactic2::tactic_selected>);

	tactic_selected(window, tactic_list);
}

void ttactic2::post_show(twindow& window)
{
	if (get_retval() == twindow::OK) {
		selected_ = find_widget<tlistbox>(&window, "tactic_list", false).get_selected_row();
	}
}

hero* ttactic2::get_selected_hero() const
{
	if (selected_ < 0) {
		return &hero_invalid;
	}
	return candidate_[selected_]; 
}

}
