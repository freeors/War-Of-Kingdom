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

#include "gui/dialogs/book2.hpp"

#include "formula_string_utils.hpp"
#include "gettext.hpp"
#include "display.hpp"
#include "artifical.hpp"
#include "gui/dialogs/helper.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "unit_types.hpp"
#include "integrate.hpp"

#include <boost/bind.hpp>

namespace help {
// Helpers for making generation of topics easier.
const std::string faction_prefix = "faction_";

std::string bold(const std::string &s)
{
	std::stringstream ss;
	ss << "<bold>text='" << escape(s) << "'</bold>";
	return ss.str();
}

std::string jump(const unsigned amount)
{
	std::stringstream ss;
	ss << "<jump>amount=" << amount << "</jump>";
	return ss.str();
}

std::string jump_to(const unsigned pos)
{
	std::stringstream ss;
	ss << "<jump>to=" << pos << "</jump>";
	return ss.str();
}

// Return the width for the image with filename.
unsigned image_width(const std::string &filename)
{
	image::locator loc(filename);
	surface surf(image::get_image(loc));
	if (surf != NULL) {
		return surf->w;
	}
	return 0;
}

void push_tab_pair(std::vector<std::pair<std::string, unsigned int> > &v, const std::string &s)
{
	v.push_back(std::make_pair(s, font::line_width(s, normal_font_size)));
}

typedef std::vector<std::vector<std::pair<std::string, unsigned int > > > table_spec;
// Create a table using the table specs. Return markup with jumps
// that create a table. The table spec contains a vector with
// vectors with pairs. The pairs are the markup string that should
// be in a cell, and the width of that cell.
std::string generate_table(const table_spec &tab, const unsigned int spacing = 20)
{
	table_spec::const_iterator row_it;
	std::vector<std::pair<std::string, unsigned> >::const_iterator col_it;
	unsigned int num_cols = 0;
	for (row_it = tab.begin(); row_it != tab.end(); ++row_it) {
		if (row_it->size() > num_cols) {
			num_cols = row_it->size();
		}
	}
	std::vector<unsigned int> col_widths(num_cols, 0);
	// Calculate the width of all columns, including spacing.
	for (row_it = tab.begin(); row_it != tab.end(); ++row_it) {
		unsigned int col = 0;
		for (col_it = row_it->begin(); col_it != row_it->end(); ++col_it) {
			if (col_widths[col] < col_it->second + spacing) {
				col_widths[col] = col_it->second + spacing;
			}
			++col;
		}
	}
	std::vector<unsigned int> col_starts(num_cols);
	// Calculate the starting positions of all columns
	for (unsigned int i = 0; i < num_cols; ++i) {
		unsigned int this_col_start = 0;
		for (unsigned int j = 0; j < i; ++j) {
			this_col_start += col_widths[j];
		}
		col_starts[i] = this_col_start;
	}
	std::stringstream ss;
	for (row_it = tab.begin(); row_it != tab.end(); ++row_it) {
		unsigned int col = 0;
		for (col_it = row_it->begin(); col_it != row_it->end(); ++col_it) {
			ss << jump_to(col_starts[col]) << col_it->first;
			++col;
		}
		ss << "\n";
	}
	return ss.str();
}


std::string make_link(const std::string& text, const std::string& dst)
{
	// some sorting done on list of links may rely on the fact that text is first
	return "<ref>text='" + escape(text) + "' dst='" + escape(dst) + "'</ref>";
}

std::string make_unit_link(const std::string& type_id)
{
	std::string link;

	const unit_type *type = unit_types.find(type_id);
	if (!type) {
		std::cerr << "Unknown unit type : " << type_id << "\n";
		// don't return an hyperlink (no page)
		// instead show the id (as hint)
		link = type_id;
	} else if (!type->hide_help()) {
		std::string name = type->type_name();
		std::string ref_id;
		ref_id = unit_prefix + type->id();
		link =  make_link(name, ref_id);
	} // if hide_help then link is an empty string

	return link;
}

std::vector<std::string> make_unit_links_list(const std::vector<std::string>& type_id_list, bool ordered)
{
	std::vector<std::string> links_list;
	BOOST_FOREACH (const std::string &type_id, type_id_list) {
		std::string unit_link = make_unit_link(type_id);
		if (!unit_link.empty())
			links_list.push_back(unit_link);
	}

	if (ordered)
		std::sort(links_list.begin(), links_list.end());

	return links_list;
}

}

namespace gui2 {

std::vector<help::topic> tbook2::generate_ability_topics(const bool sort_generated)
{
	std::vector<help::topic> topics;
	std::map<t_string, std::string> ability_description;
	std::map<t_string, std::set<std::string, help::string_less> > ability_units;
	// Look through all the unit types, check if a unit of this type
	// should have a full description, if so, add this units abilities
	// for display. We do not want to show abilities that the user has
	// not encountered yet.
	BOOST_FOREACH (const unit_type_data::unit_type_map::value_type &i, unit_types.types())
	{
		const unit_type &type = i.second;

		std::vector<t_string> const* abil_vecs[2];
		abil_vecs[0] = &type.abilities();

		std::vector<std::string> const* desc_vecs[2];
		desc_vecs[0] = &type.ability_tooltips();

		for(int i=0; i<1; ++i) {
			std::vector<t_string> const& abil_vec = *abil_vecs[i];
			std::vector<std::string> const& desc_vec = *desc_vecs[i];
			for(size_t j=0; j < abil_vec.size(); ++j) {
				t_string const& abil_name = abil_vec[j];
				if (ability_description.find(abil_name) == ability_description.end()) {
					//new ability, generate a descripion
					if(j >= desc_vec.size()) {
						ability_description[abil_name] = "";
					} else {
						std::string const& abil_desc = desc_vec[j];
						const size_t colon_pos = abil_desc.find(':');
						if(colon_pos != std::string::npos && colon_pos + 1 < abil_desc.length()) {
							// Remove the first colon and the following newline.
							ability_description[abil_name] = abil_desc.substr(colon_pos + 2);
						} else {
							ability_description[abil_name] = abil_desc;
						}
					}
				}

				if (!type.hide_help()) {
					//add a link in the list of units having this ability
					std::string type_name = type.type_name();
					std::string ref_id = help::unit_prefix +  type.id();
					//we put the translated name at the beginning of the hyperlink,
					//so the automatic alphabetic sorting of std::set can use it
					std::string link =  "<ref>text='" + help::escape(type_name) + "' dst='" + help::escape(ref_id) + "'</ref>";
					ability_units[abil_name].insert(link);
				}
			}
		}
	}

	for (std::map<t_string, std::string>::iterator a = ability_description.begin(); a != ability_description.end(); ++a) {
		// we generate topic's id using the untranslated version of the ability's name
		std::string id = "ability_" + a->first.base_str();
		std::stringstream text;
		text << a->second;  //description
		text << "\n\n" << _("<header>text='Units having this ability'</header>") << "\n";
		std::set<std::string, help::string_less> &units = ability_units[a->first];
		for (std::set<std::string, help::string_less>::iterator u = units.begin(); u != units.end(); ++u) {
			text << (*u) << "\n";
		}

		topics.push_back(help::topic(a->first, id, text.str()) );
	}

	if (sort_generated) {
		std::sort(topics.begin(), topics.end(), help::title_less());
	}
	return topics;
}

std::vector<help::topic> tbook2::generate_weapon_special_topics(const bool sort_generated)
{
	std::vector<help::topic> topics;

	std::map<t_string, std::string> special_description;
	std::map<t_string, std::set<std::string, help::string_less> > special_units;

	BOOST_FOREACH (const unit_type_data::unit_type_map::value_type &i, unit_types.types())
	{
		const unit_type &type = i.second;
		// Only show the weapon special if we find it on a unit that
		// detailed description should be shown about.
		std::vector<attack_type> attacks = type.attacks();
		for (std::vector<attack_type>::const_iterator it = attacks.begin();
					it != attacks.end(); ++it) {
			std::vector<t_string> specials = (*it).special_tooltips(true);

			for (std::vector<t_string>::iterator sp_it = specials.begin();
					sp_it != specials.end() && sp_it+1 != specials.end(); sp_it+=2)
			{
				if (special_description.find(*sp_it) == special_description.end()) {
					std::string description = *(sp_it+1);
					const size_t colon_pos = description.find(':');
					if (colon_pos != std::string::npos) {
						// Remove the first colon and the following newline.
						description.erase(0, colon_pos + 2);
					}
					special_description[*sp_it] = description;
				}

				if (!type.hide_help()) {
					//add a link in the list of units having this special
					std::string type_name = type.type_name();
					std::string ref_id = help::unit_prefix + type.id();
					//we put the translated name at the beginning of the hyperlink,
					//so the automatic alphabetic sorting of std::set can use it
					std::string link =  "<ref>text='" + help::escape(type_name) + "' dst='" + help::escape(ref_id) + "'</ref>";
					special_units[*sp_it].insert(link);
				}
			}
		}
	}

	for (std::map<t_string, std::string>::iterator s = special_description.begin();
			s != special_description.end(); ++s) {
		// use untranslated name to have universal topic id
		std::string id = "weaponspecial_" + s->first.base_str();
		std::stringstream text;
		text << s->second;
		text << "\n\n" << _("<header>text='Units having this special attack'</header>") << "\n";
		std::set<std::string, help::string_less> &units = special_units[s->first];
		for (std::set<std::string, help::string_less>::iterator u = units.begin(); u != units.end(); ++u) {
			text << (*u) << "\n";
		}

		topics.push_back(help::topic(s->first, id, text.str()) );
	}

	if (sort_generated)
		std::sort(topics.begin(), topics.end(), help::title_less());
	return topics;
}

std::vector<help::topic> tbook2::generate_faction_topics(const bool sort_generated)
{
	std::vector<help::topic> topics;
	const config& era = help::game_cfg->child("era");
	if (era) {
		std::vector<std::string> faction_links;
		BOOST_FOREACH (const config &f, era.child_range("multiplayer_side")) {
			const std::string& id = f["id"];
			if (id == "Random")
				continue;

			std::stringstream text;

			const std::string& description = f["description"];
			if (!description.empty()) {
				text << description << "\n";
				text << "\n";
			}

			text << "<header>text='" << _("Leaders:") << "'</header>" << "\n";
			const std::vector<std::string> leaders =
				help::make_unit_links_list( utils::split(f["leader"]), true );
			BOOST_FOREACH (const std::string &link, leaders) {
				text << link << "\n";
			}

			text << "\n";

			text << "<header>text='" << _("Recruits:") << "'</header>" << "\n";
			const std::vector<std::string> recruits =
				help::make_unit_links_list( utils::split(f["recruit"]), true );
			BOOST_FOREACH (const std::string &link, recruits) {
				text << link << "\n";
			}

			const std::string name = f["name"];
			const std::string ref_id = help::faction_prefix + id;
			topics.push_back(help::topic(name, ref_id, text.str()) );
			faction_links.push_back(help::make_link(name, ref_id));
		}

		std::stringstream text;
		text << "<header>text='" << _("Era:") << " " << era["name"] << "'</header>" << "\n";
		text << "\n";
		const std::string& description = era["description"];
		if (!description.empty()) {
			text << description << "\n";
			text << "\n";
		}

		text << "<header>text='" << _("Factions:") << "'</header>" << "\n";

		std::sort(faction_links.begin(), faction_links.end());
		BOOST_FOREACH (const std::string &link, faction_links) {
			text << link << "\n";
		}

		topics.push_back(help::topic(_("Factions"), "..factions_section", text.str()) );
	} else {
		topics.push_back(help::topic( _("Factions"), "..factions_section",
			_("Factions are only used in multiplayer")) );
	}

	if (sort_generated)
		std::sort(topics.begin(), topics.end(), help::title_less());
	return topics;
}

class unit_topic_generator: public help::topic_generator
{
	const unit_type& type_;
	typedef std::pair<std::string, unsigned > item;
	void push_header(std::vector< item > &row, char const *name) const {
		row.push_back(item(help::bold(name), font::line_width(name, help::normal_font_size, TTF_STYLE_BOLD)));
	}
public:
	unit_topic_generator(const unit_type &t): type_(t) {}
	virtual std::string operator()() const {
		// this will force the lazy loading to build this unit
		unit_types.find(type_.id(), unit_type::WITHOUT_ANIMATIONS);

		std::stringstream ss;
		std::string clear_stringstream;
		const std::string detailed_description = type_.unit_description();
		const unit_type& female_type = type_.get_gender_unit_type(unit_race::FEMALE);
		const unit_type& male_type = type_.get_gender_unit_type(unit_race::MALE);

		// Show the unit's image and its level.
		ss << "<img>src='" << male_type.image() << "~RC(" << male_type.flag_rgb() << ">1)" << "'</img> ";

		if (&female_type != &male_type) {
			ss << "<img>src='" << female_type.image() << "~RC(" << female_type.flag_rgb() << ">1)" << "'</img> ";
		}

		ss << "<format>font_size=" << 11 << " text=' " << help::escape(_("level"))
		   << " " << type_.level() << "'</format>";

		const std::string male_portrait = "";
		const std::string female_portrait = "";

		if (male_portrait.empty() == false && male_portrait != male_type.image()) {
			ss << "<img>src='" << male_portrait << "' align='right'</img> ";
		}

		if (female_portrait.empty() == false && female_portrait != male_portrait && female_portrait != female_type.image()) {
			ss << "<img>src='" << female_portrait << "' align='right'</img> ";
		}

		ss << "\n";

		// Print cross-references to units that this unit advances from/to.
		// Cross reference to the topics containing information about those units.
		const bool first_reverse_value = true;
		bool reverse = first_reverse_value;
		do {
			std::vector<std::string> adv_units =
				reverse ? type_.advances_from() : type_.advances_to();
			bool first = true;

			BOOST_FOREACH (const std::string &adv, adv_units)
			{
				const unit_type *type = unit_types.find(adv);
				if (!type || type->hide_help()) continue;

				if (first) {
					if (reverse)
						ss << _("Advances from: ");
					else
						ss << _("Advances to: ");
					first = false;
				} else
					ss << ", ";

				std::string lang_unit = type->type_name();
				std::string ref_id;
				ref_id = help::unit_prefix + type->id();
				ss << "<ref>dst='" << help::escape(ref_id) << "' text='" << help::escape(lang_unit) << "'</ref>";
			}
			ss << "\n"; //added even if empty, to avoid shifting

			reverse = !reverse; //switch direction
		} while(reverse != first_reverse_value); // don't restart

		// Print the race of the unit, cross-reference it to the
		// respective topic.
		const std::string race_id = type_.race();
		std::string race_name;
		if (const unit_race *r = unit_types.find_race(race_id)) {
			race_name = r->plural_name();
		} else {
			race_name = _("race^Miscellaneous");
		}
		ss << _("Race: ");
		ss << "<ref>dst='" << help::escape("..race_"+race_id) << "' text='" << help::escape(race_name) << "'</ref>";
		ss << "\n";

		// Print the abilities the units has, cross-reference them
		// to their respective topics.
		if (!type_.abilities().empty()) {
			ss << _("Abilities: ");
			for(std::vector<t_string>::const_iterator ability_it = type_.abilities().begin(),
				 ability_end = type_.abilities().end();
				 ability_it != ability_end; ++ability_it) {
				const std::string ref_id = "ability_" + ability_it->base_str();
				std::string lang_ability = _(ability_it->c_str());
				ss << "<ref>dst='" << help::escape(ref_id) << "' text='" << help::escape(lang_ability)
				   << "'</ref>";
				if (ability_it + 1 != ability_end)
					ss << ", ";
			}
			ss << "\n";
		}

		ss << "\n";

		// Print some basic information such as HP and movement points.
		ss << _("HP: ") << type_.hitpoints() << help::jump(30)
			<< _("Moves: ") << type_.movement() << help::jump(30)
			<< _("Cost: ") << type_.cost() << help::jump(30)
		   << _("Alignment: ")
		   << "<ref>dst='time_of_day' text='"
		   << type_.alignment_description(type_.alignment(), type_.genders().front())
		   << "'</ref>"
		   << help::jump(30);
		if (type_.can_advance())
			ss << _("Required XP: ") << type_.experience_needed();

		// Print the detailed description about the unit.
		ss << "\n\n" << detailed_description;

		// Print the different attacks a unit has, if it has any.
		std::vector<attack_type> attacks = type_.attacks();
		if (!attacks.empty()) {
			// Print headers for the table.
			ss << "\n\n<header>text='" << help::escape(_("unit help^Attacks"))
			   << "'</header>\n\n";
			help::table_spec table;

			std::vector<item> first_row;
			// Dummy element, icons are below.
			first_row.push_back(item("", 0));
			push_header(first_row, _("unit help^Name"));
			push_header(first_row, _("Type"));
			push_header(first_row, _("Strikes"));
			push_header(first_row, _("Range"));
			push_header(first_row, _("Special"));
			table.push_back(first_row);
			// Print information about every attack.
			for(std::vector<attack_type>::const_iterator attack_it = attacks.begin(),
				 attack_end = attacks.end();
				 attack_it != attack_end; ++attack_it) {
				std::string lang_weapon = attack_it->name();
				std::string lang_type = _(attack_it->type().c_str());
				std::vector<item> row;
				std::stringstream attack_ss;
				attack_ss << "<img>src='" << (*attack_it).icon() << "'</img>";
				row.push_back(std::make_pair(attack_ss.str(), help::image_width(attack_it->icon())));
				help::push_tab_pair(row, lang_weapon);
				help::push_tab_pair(row, lang_type);
				attack_ss.str(clear_stringstream);
				attack_ss << attack_it->damage() << '-' << attack_it->num_attacks() << " " << attack_it->accuracy_parry_description();
				help::push_tab_pair(row, attack_ss.str());
				attack_ss.str(clear_stringstream);
				help::push_tab_pair(row, _((*attack_it).range().c_str()));
				// Show this attack's special, if it has any. Cross
				// reference it to the section describing the
				// special.
				std::vector<t_string> specials = attack_it->special_tooltips(true);

				if (!specials.empty()) {
					std::string lang_special = "";
					std::vector<t_string>::iterator sp_it;
					for (sp_it = specials.begin(); sp_it != specials.end(); ++sp_it) {
						const std::string ref_id = std::string("weaponspecial_")
							+ sp_it->base_str();
						lang_special = (*sp_it);
						attack_ss << "<ref>dst='" << help::escape(ref_id)
							<< "' text='" << help::escape(lang_special) << "'</ref>";
						if((sp_it + 1) != specials.end() && (sp_it + 2) != specials.end())
						{
							attack_ss << ", "; //comma placed before next special
						}
						++sp_it; //skip description
					}
					row.push_back(std::make_pair(attack_ss.str(),
						font::line_width(lang_special, help::normal_font_size)));

				}
				table.push_back(row);
			}
			ss << help::generate_table(table);
		}

		// Print the resistance table of the unit.
		ss << "\n\n<header>text='" << help::escape(_("Resistances"))
		   << "'</header>\n\n";
		help::table_spec resistance_table;
		std::vector<item> first_res_row;
		push_header(first_res_row, _("Attack Type"));
		push_header(first_res_row, _("Resistance"));
		resistance_table.push_back(first_res_row);
		const unit_movement_type &movement_type = type_.movement_type();
		utils::string_map dam_tab = movement_type.damage_table();
		for(utils::string_map::const_iterator dam_it = dam_tab.begin(), dam_end = dam_tab.end();
			 dam_it != dam_end; ++dam_it) {
			std::vector<item> row;
			int resistance = 100 - atoi((*dam_it).second.c_str());
			char resi[16];
			snprintf(resi,sizeof(resi),"% 4d%%",resistance);	// range: -100% .. +70%
			std::string color;
			if (resistance < 0)
				color = "red";
			else if (resistance <= 20)
				color = "yellow";
			else if (resistance <= 40)
				color = "white";
			else
				color = "green";

			std::string lang_weapon = _(dam_it->first.c_str());
			help::push_tab_pair(row, lang_weapon);
			std::stringstream str;
			str << "<format>color=" << color << " text='"<< resi << "'</format>";
			const std::string markup = str.str();
			str.str(clear_stringstream);
			str << resi;
			row.push_back(std::make_pair(markup,
				font::line_width(str.str(), help::normal_font_size)));
			resistance_table.push_back(row);
		}
		ss << help::generate_table(resistance_table);

		if (help::map != NULL) {
			// Print the terrain modifier table of the unit.
			ss << "\n\n<header>text='" << help::escape(_("Terrain Modifiers"))
			   << "'</header>\n\n";
			std::vector<item> first_row;
			help::table_spec table;
			push_header(first_row, _("Terrain"));
			push_header(first_row, _("Defense"));
			push_header(first_row, _("Movement Cost"));

			table.push_back(first_row);

			const t_translation::t_list& terrain_list = help::map->get_terrain_list();
			for (t_translation::t_list::const_iterator terrain_it = terrain_list.begin(); terrain_it != terrain_list.end(); ++ terrain_it) {
				const t_translation::t_terrain terrain = *terrain_it;
				if (terrain == t_translation::FOGGED || terrain == t_translation::VOID_TERRAIN || terrain == t_translation::OFF_MAP_USER)
					continue;
				const terrain_type& info = help::map->get_terrain_info(terrain);

				if (info.union_type().size() == 1 && info.union_type()[0] == info.number() && info.is_nonnull()) {
					std::vector<item> row;
					const std::string& name = info.name();
					const std::string id = info.id();
					const int moves = movement_type.movement_cost(*help::map,terrain);
					std::stringstream str;
					str << "<ref>text='" << help::escape(name) << "' dst='"
						<< help::escape(std::string("terrain_") + id) << "'</ref>";
					row.push_back(std::make_pair(str.str(),
						font::line_width(name, help::normal_font_size)));

					//defense  -  range: +10 % .. +70 %
					str.str(clear_stringstream);
					const int defense =
						100 - movement_type.defense_modifier(*help::map, terrain);
					std::string color;
					if (defense < 0)
						color = "red";
					else if (defense < 20)
						color = "yellow";
					else if (defense < 40)
						color = "white";
					else
						color = "green";

					str << "<format>color=" << color << " text='"<< defense << "%'</format>";
					const std::string markup = str.str();
					str.str(clear_stringstream);
					str << defense << "%";
					row.push_back(std::make_pair(markup,
						font::line_width(str.str(), help::normal_font_size)));

					//movement  -  range: 1 .. 5, unit_movement_type::UNREACHABLE=impassable
					str.str(clear_stringstream);
					const bool cannot_move = moves > type_.movement();
					if (cannot_move)		// cannot move in this terrain
						color = "red";
					else if (moves > 1)
						color = "yellow";
					else
						color = "white";

					str << "<format>color=" << color << " text='";
					// A 5 MP margin; if the movement costs go above
					// the unit's max moves + 5, we replace it with dashes.
					if(cannot_move && (moves > type_.movement() + 5)) {
						str << "'-'";
					} else {
						str << moves;
					}
					str << "'</format>";
					help::push_tab_pair(row, str.str());

					table.push_back(row);
				}
			}
			ss << help::generate_table(table);
		}
		return ss.str();
	}
};

std::vector<help::topic> tbook2::generate_unit_topics(const bool sort_generated, const std::string& race)
{
	std::vector<help::topic> topics;
	std::set<std::string, help::string_less> race_units;

	BOOST_FOREACH (const unit_type_data::unit_type_map::value_type &i, unit_types.types())
	{
		const unit_type &type = i.second;

		if (type.race() != race)
			continue;

		const std::string type_name = type.type_name();
		const std::string ref_id = help::hidden_symbol(type.hide_help()) + help::unit_prefix +  type.id();
		help::topic unit_topic(type_name, ref_id, "");
		unit_topic.text = new unit_topic_generator(type);
		topics.push_back(unit_topic);

		if (!type.hide_help()) {
			// we also record an hyperlink of this unit
			// in the list used for the race topic
			std::string link =  "<ref>text='" + help::escape(type_name) + "' dst='" + help::escape(ref_id) + "'</ref>";
			race_units.insert(link);
		}
	}

	//generate the hidden race description topic
	std::string race_id = "..race_"+race;
	std::string race_name;
	std::string race_description;
	if (const unit_race *r = unit_types.find_race(race)) {
		race_name = r->plural_name();
		race_description = r->description();
		// if (description.empty()) description =  _("No description Available");
	} else {
		race_name = _("race^Miscellaneous");
		// description =  _("Here put the description of the Miscellaneous race");
	}

	std::stringstream text;
	text << race_description;
	text << "\n\n" << _("<header>text='Units of this race'</header>") << "\n";
	for (std::set<std::string, help::string_less>::iterator u = race_units.begin(); u != race_units.end(); ++u) {
		text << (*u) << "\n";
	}
	topics.push_back(help::topic(race_name, race_id, text.str()) );

	if (sort_generated) {
		std::sort(topics.begin(), topics.end(), help::title_less());
	}
	return topics;
}

void tbook2::generate_races_sections(const config *help_cfg, help::section &sec, int level)
{
	std::set<std::string, help::string_less> races;
	std::set<std::string, help::string_less> visible_races;

	BOOST_FOREACH (const unit_type_data::unit_type_map::value_type &i, unit_types.types())
	{
		const unit_type &type = i.second;
		races.insert(type.race());
		if (!type.hide_help())
			visible_races.insert(type.race());
	}

	std::stringstream text;

	for (std::set<std::string, help::string_less>::iterator it = races.begin(); it != races.end(); ++it) {
		help::section race_section;
		config section_cfg;

		bool hidden = (visible_races.count(*it) == 0);

		section_cfg["id"] = help::hidden_symbol(hidden) + help::race_prefix + *it;

		std::string title;
		if (const unit_race *r = unit_types.find_race(*it)) {
			title = r->plural_name();
		} else {
			title = _("race^Miscellaneous");
		}
		section_cfg["title"] = title;

		section_cfg["generator"] = "units:" + *it;

		parse_config_internal(this, help_cfg, &section_cfg, race_section, level+1);
		sec.add_section(race_section);
	}
}

tbook2::tbook2(display& disp, gamemap* map, hero_map& heros, const config& game_config, const std::string& tag)
	: tbook(disp, map, heros, game_config, tag)
{}

std::vector<help::topic> tbook2::generate_topics(const bool sort_generated, const std::string &generator)
{
	std::vector<help::topic> res;
	if (help::editor || generator.empty()) {
		return res;
	}

	if (generator == "abilities") {
		res = generate_ability_topics(sort_generated);
	} else if (generator == "weapon_specials") {
		res = generate_weapon_special_topics(sort_generated);
	} else if (generator == "factions") {
		res = generate_faction_topics(sort_generated);
	} else {
		std::vector<std::string> parts = utils::split(generator, ':', utils::STRIP_SPACES);
		if (parts[0] == "units" && parts.size()>1) {
			res = generate_unit_topics(sort_generated, parts[1]);
		}
	}
	return res;
}

void tbook2::generate_sections(const config* help_cfg, const std::string &generator, help::section &sec, int level)
{
	if (help::editor) {
		return;
	}

	if (generator == "races") {
		generate_races_sections(help_cfg, sec, level);
	}
}

} // namespace gui2