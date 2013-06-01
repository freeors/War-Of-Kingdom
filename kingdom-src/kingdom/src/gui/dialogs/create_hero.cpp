/* $Id: title_screen.cpp 48740 2011-03-05 10:01:34Z mordante $ */
/*
   Copyright (C) 2008 - 2011 by Mark de Wever <koraq@xs4all.nl>
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

#include "gui/dialogs/create_hero.hpp"

#include "display.hpp"
#include "game_config.hpp"
#include "game_preferences.hpp"
#include "gettext.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/widgets/window.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/combo_box.hpp"
#include "preferences_display.hpp"
#include "formula_string_utils.hpp"

#include <boost/bind.hpp>

#include <algorithm>

namespace gui2 {

/*WIKI
 * @page = GUIWindowDefinitionWML
 * @order = 2_title_screen
 *
 * == Title screen ==
 *
 * This shows the title screen.
 *
 * @begin{table}{dialog_widgets}
 * tutorial & & button & m &
 *         The button to start the tutorial. $
 *
 * campaign & & button & m &
 *         The button to start a campaign. $
 *
 * multiplayer & & button & m &
 *         The button to start multiplayer mode. $
 *
 * load & & button & m &
 *         The button to load a saved game. $
 *
 * editor & & button & m &
 *         The button to start the editor. $
 *
 * addons & & button & m &
 *         The button to start managing the addons. $
 *
 * language & & button & m &
 *         The button to select the game language. $
 *
 * credits & & button & m &
 *         The button to show Wesnoth's contributors. $
 *
 * quit & & button & m &
 *         The button to quit Wesnoth. $
 *
 * tips & & multi_page & m &
 *         A multi_page to hold all tips, when this widget is used the area of
 *         the tips doesn't need to be resized when the next or previous button
 *         is pressed. $
 *
 * -tip & & label & o &
 *         Shows the text of the current tip. $
 *
 * -source & & label & o &
 *         The source (the one who's quoted or the book referenced) of the
 *         current tip. $
 *
 * next_tip & & button & m &
 *         The button show the next tip of the day. $
 *
 * previous_tip & & button & m &
 *         The button show the previous tip of the day. $
 *
 * logo & & progress_bar & o &
 *         A progress bar to "animate" the Wesnoth logo. $
 *
 * revision_number & & control & o &
 *         A widget to show the version number when the version number is
 *         known. $
 *
 * @end{table}
 */

REGISTER_DIALOG(create_hero)

tcreate_hero::tcreate_hero(display& gui, hero_map& heros, hero& player_hero)
	: gui_(gui)
	, heros_(heros)
	, h_(player_hero)
	, player_hero_(player_hero)
	, male_number_(-1)
	, female_number_(-1)
{
	male_number_ = h_.number_;
	female_number_ = male_number_ + 1;
}

tcreate_hero::~tcreate_hero()
{
}

void tcreate_hero::post_build(CVideo& video, twindow& window)
{
}

const size_t max_login_size = 20;

void tcreate_hero::set_text_box_int(twindow& window, const std::string& id, int value, int maximum_length)
{
	ttext_box* widget = find_widget<ttext_box>(&window, id, false, true);
	widget->set_value(lexical_cast<std::string>(value));
	if (maximum_length != -1) {
		widget->set_maximum_length(maximum_length);
	}
}

static int nb_catalogs = 8;
static const int catalog_items[] = {
	135,
	3,
	138,
	239,
	4,
	136,
	187,
	104
};

void tcreate_hero::pre_show(CVideo& video, twindow& window)
{
	std::stringstream str;

	window.set_enter_disabled(true);
	window.set_escape_disabled(true);

	tcontrol* portrait = find_widget<tcontrol>(&window, "portrait", false, true);
	portrait->set_label(h_.image(true));
	tcontrol* gender_button = find_widget<tcontrol>(&window, "gender", false, true);
	gender_button->set_label(hero::gender_str(h_.gender_));
	
	/**** Set the version number ****/
	ttext_box* user_widget = find_widget<ttext_box>(&window, "name", false, true);
	user_widget->set_value(h_.name());
	user_widget->set_maximum_length(max_login_size);
	user_widget = find_widget<ttext_box>(&window, "surname", false, true);
	user_widget->set_value(h_.surname());
	user_widget->set_maximum_length(max_login_size);

	set_text_box_int(window, "leadership", fxptoi9(h_.leadership_), 3);
	set_text_box_int(window, "force", fxptoi9(h_.force_), 3);
	set_text_box_int(window, "intellect", fxptoi9(h_.intellect_), 3);
	set_text_box_int(window, "politics", fxptoi9(h_.politics_), 3);
	set_text_box_int(window, "charm", fxptoi9(h_.charm_), 3);

	tcontrol* control;
	for (int i = 0; i < HEROS_MAX_ARMS; i ++) {
		str.str("");
		str << "arms" << i;
		control = find_widget<tbutton>(&window, str.str(), false, false);
		if (control) {
			connect_signal_mouse_left_click(
				*control
				, boost::bind(
				&tcreate_hero::adaptability
				, this
				, boost::ref(window)
				, (int)hero::ARMS
				, i));
		}

		str.str("");
		str << "text_arms" << i;
		control = find_widget<tcontrol>(&window, str.str(), false, false);
		if (control) {
			control->set_label(hero::adaptability_str2(h_.arms_[i]));
		}
	}
	for (int i = 0; i < HEROS_MAX_SKILL; i ++) {
		str.str("");
		str << "skill" << i;
		control = find_widget<tbutton>(&window, str.str(), false, false);
		if (control) {
			connect_signal_mouse_left_click(
				find_widget<tbutton>(&window, str.str(), false)
				, boost::bind(
				&tcreate_hero::adaptability
				, this
				, boost::ref(window)
				, (int)hero::SKILL
				, i));
		}

		str.str("");
		str << "text_skill" << i;
		control = find_widget<tcontrol>(&window, str.str(), false, false);
		if (control) {
			control->set_label(hero::adaptability_str2(h_.skill_[i]));
		}
	}

	tbutton* button = find_widget<tbutton>(&window, "feature", false, true);
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "feature", false)
		, boost::bind(
		&tcreate_hero::feature
		, this
		, boost::ref(window)
		, false));
	button->set_label(hero::feature_str(h_.feature_));

	button = find_widget<tbutton>(&window, "side_feature", false, true);
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "side_feature", false)
		, boost::bind(
		&tcreate_hero::feature
		, this
		, boost::ref(window)
		, true));
	button->set_label(hero::feature_str(h_.side_feature_));

	button = find_widget<tbutton>(&window, "tactic", false, true);
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "tactic", false)
		, boost::bind(
		&tcreate_hero::tactic
		, this
		, boost::ref(window)));
	if (h_.tactic_ != HEROS_NO_TACTIC) {
		button->set_label(unit_types.tactic(h_.tactic_).name());
	} else {
		button->set_label("");
	}

	button = find_widget<tbutton>(&window, "character", false, true);
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "character", false)
		, boost::bind(
		&tcreate_hero::character
		, this
		, boost::ref(window)));
	if (h_.character_ != HEROS_NO_CHARACTER) {
		button->set_label(unit_types.character(h_.character_).name());
	} else {
		button->set_label("");
	}

	button = find_widget<tbutton>(&window, "catalog", false, true);
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "catalog", false)
		, boost::bind(
		&tcreate_hero::catalog
		, this
		, boost::ref(window)));
	str.str("");
	for (int i = 0; i < nb_catalogs; i ++) {
		hero& h = heros_[catalog_items[i]];
		if (h_.base_catalog_ == h.base_catalog_) {
			str << h.name();
			break;
		}
	}
	if (str.str().empty()) {
		hero& h = heros_[catalog_items[0]];
		h_.base_catalog_ = h.base_catalog_;
		str << h.name();
	}
	button->set_label(str.str());

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "regenerate", false)
		, boost::bind(
		&tcreate_hero::regenerate
		, this
		, boost::ref(window)));
	find_widget<tbutton>(&window, "regenerate", false).set_label(_("Stronger"));
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "gender", false)
		, boost::bind(
		&tcreate_hero::gender
		, this
		, boost::ref(window)));
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "create", false)
		, boost::bind(
		&tcreate_hero::create
		, this
		, boost::ref(window)));
}

void tcreate_hero::post_show(twindow& window)
{
}

void tcreate_hero::regenerate(twindow& window)
{
	// stronger
	h_.leadership_ = ftofxp9(100);
	h_.force_ = ftofxp9(100);
	h_.intellect_ = ftofxp9(100);
	h_.politics_ = ftofxp9(100);
	h_.charm_ = ftofxp9(100);

	for (int i = 0; i < HEROS_MAX_ARMS; i ++) {
		h_.arms_[i] = ftofxp12(3);
	}
	for (int i = 0; i < HEROS_MAX_SKILL; i ++) {
		h_.skill_[i] = ftofxp12(3);
	}

	// refresh to ui
	set_text_box_int(window, "leadership", fxptoi9(h_.leadership_));
	set_text_box_int(window, "force", fxptoi9(h_.force_), 3);
	set_text_box_int(window, "intellect", fxptoi9(h_.intellect_));
	set_text_box_int(window, "politics", fxptoi9(h_.politics_));
	set_text_box_int(window, "charm", fxptoi9(h_.charm_));

	std::stringstream strstr;
	for (int i = 0; i < HEROS_MAX_ARMS; i ++) {
		strstr.str("");
		strstr << "text_arms" << i;
		tcontrol* temp = find_widget<tcontrol>(&window, strstr.str(), true, false);
		if (temp) {
			temp->set_label(hero::adaptability_str2(h_.arms_[i]));
		}
	}
	for (int i = 0; i < HEROS_MAX_SKILL; i ++) {
		strstr.str("");
		strstr << "text_skill" << i;
		tcontrol* temp = find_widget<tcontrol>(&window, strstr.str(), true, false);
		if (temp) {
			temp->set_label(hero::adaptability_str2(h_.skill_[i]));
		}
	}
}

void tcreate_hero::gender(twindow& window)
{
	tcontrol* regenerate_button = find_widget<tcontrol>(&window, "regenerate", false, true);
	tcontrol* gender_button = find_widget<tcontrol>(&window, "gender", false, true);
	tcontrol* portrait = find_widget<tcontrol>(&window, "portrait", false, true);
	
	if (h_.gender_ == hero_gender_male) {
		// male --> female
		h_.set_image(female_number_);
		h_.gender_ = hero_gender_female;
	} else {
		// female --> male
		h_.set_image(male_number_);
		h_.gender_ = hero_gender_male;
	}
	gender_button->set_label(hero::gender_str(h_.gender_));
	portrait->set_label(h_.image(true));

	regenerate_button->set_dirty();
}

void tcreate_hero::adaptability(twindow& window, int type, int index)
{
	char text[32];
	std::vector<std::string> items;
	int activity_index;
	std::stringstream str;

	for (int idx = 0; idx <= hero_adaptability_max; idx ++) {
		sprintf(text, "%s%i", HERO_PREFIX_STR_ADAPTABILITY, idx);
		items.push_back(dgettext("wesnoth-hero", text)); 
	}

	if (type == hero::ARMS) {
		activity_index = fxptoi12(h_.arms_[index]);
		str << "text_arms" << index;
	} else {
		activity_index = fxptoi12(h_.skill_[index]);
		str << "text_skill" << index;
	}

	gui2::tcombo_box dlg(items, activity_index);
	dlg.show(gui_.video());

	tcontrol* label = find_widget<tcontrol>(&window, str.str(), false, true);
	activity_index = dlg.selected_index();
	if (type == hero::ARMS) {
		h_.arms_[index] = ftofxp12(activity_index);
		label->set_label(hero::adaptability_str2(h_.arms_[index]));
	} else {
		h_.skill_[index] = ftofxp12(activity_index);
		label->set_label(hero::adaptability_str2(h_.skill_[index]));
	}
}

void tcreate_hero::feature(twindow& window, bool side)
{
	std::vector<std::string> items;
	int activity_index;
	std::string str;

	items.push_back(" ");
	std::vector<int> features = hero::valid_features();
	for (std::vector<int>::const_iterator itor = features.begin(); itor != features.end(); ++ itor) {
		items.push_back(hero::feature_str(*itor));
	}

	if (side) {
		if (h_.side_feature_ == HEROS_NO_FEATURE) {
			activity_index = 0;
		} else {
			// activity_index = h_.side_feature_ + 1;
			activity_index = 0;
		}
		str = "side_feature";
	} else {
		if (h_.feature_ == HEROS_NO_FEATURE) {
			activity_index = 0;
		} else {
			// activity_index ++;
			activity_index = 0;
		}
		str = "feature";
	}

	gui2::tcombo_box dlg(items, activity_index);
	dlg.show(gui_.video());

	tcontrol* label = find_widget<tcontrol>(&window, str, false, true);
	activity_index = dlg.selected_index();
	if (side) {
		if (activity_index) {
			h_.side_feature_ = features[activity_index - 1];
			str = hero::feature_str(h_.side_feature_);
		} else {
			h_.side_feature_ = HEROS_NO_FEATURE;
			str = "";
		}
		label->set_label(str);
	} else {
		if (activity_index) {
			h_.feature_ = features[activity_index - 1];
			str = hero::feature_str(h_.feature_);
		} else {
			h_.feature_ = HEROS_NO_FEATURE;
			str = "";
		}
		label->set_label(str);
	}
}

void tcreate_hero::tactic(twindow& window)
{
	std::vector<std::string> items;
	int activity_index = -1;
	std::vector<int> tactics_index;

	items.push_back(" ");
	tactics_index.push_back(HEROS_NO_TACTIC);
	const std::map<int, ttactic>& tactics = unit_types.tactics();
	for (std::map<int, ttactic>::const_iterator it = tactics.begin(); it != tactics.end(); ++ it) {
		const ttactic& t = it->second;
		items.push_back(t.name());
		tactics_index.push_back(t.index());
	}

	gui2::tcombo_box dlg(items, activity_index);
	dlg.show(gui_.video());

	activity_index = dlg.selected_index();
	h_.tactic_ = tactics_index[activity_index];

	tcontrol* label = find_widget<tcontrol>(&window, "tactic", false, true);
	label->set_label(items[activity_index]);
}

void tcreate_hero::character(twindow& window)
{
	std::vector<std::string> items;
	int activity_index = -1;
	std::vector<int> characters_index;

	items.push_back(" ");
	characters_index.push_back(HEROS_NO_CHARACTER);
	const std::map<int, tcharacter>& characters = unit_types.characters();
	for (std::map<int, tcharacter>::const_iterator it = characters.begin(); it != characters.end(); ++ it) {
		const tcharacter& t = it->second;
		items.push_back(t.name());
		characters_index.push_back(t.index());
	}

	gui2::tcombo_box dlg(items, activity_index);
	dlg.show(gui_.video());

	activity_index = dlg.selected_index();
	h_.character_ = characters_index[activity_index];

	tcontrol* label = find_widget<tcontrol>(&window, "character", false, true);
	label->set_label(items[activity_index]);
}

void tcreate_hero::catalog(twindow& window)
{
	std::stringstream str;
	std::vector<std::string> items;
	int activity_index = -1;

	for (int i = 0; i < nb_catalogs; i ++) {
		hero& h = heros_[catalog_items[i]];
		str.str("");
		str << h.name() << "(";
		int value = h.base_catalog_;
		str << value << ")";
		items.push_back(str.str());
		if (activity_index == -1 && h_.base_catalog_ == h.base_catalog_) {
			activity_index = i;
		}
	}
	if (activity_index == -1) {
		activity_index = 0;
	}

	gui2::tcombo_box dlg(items, activity_index);
	dlg.show(gui_.video());

	tcontrol* label = find_widget<tcontrol>(&window, "catalog", false, true);
	activity_index = dlg.selected_index();
	hero& h = heros_[catalog_items[activity_index]];
	h_.base_catalog_ = h.base_catalog_;
	h_.float_catalog_ = ftofxp8(h_.base_catalog_);
	label->set_label(h.name());
}

std::string tcreate_hero::text_box_str(twindow& window, const std::string& id, const std::string& name, bool allow_empty)
{
	ttext_box* widget = find_widget<ttext_box>(&window, id, false, true);
	std::string str = widget->get_value();
	if (!allow_empty && str.empty()) {
		std::stringstream err;
		utils::string_map symbols;
		symbols["key"] = name;
		
		err << vgettext("wesnoth-lib", "Don't accept empty, invalid '$key' value", symbols);
		gui2::show_message(gui_.video(), "", err.str());
		return str;
	}
	return str;
}

bool tcreate_hero::text_box_int(twindow& window, const std::string& id, const std::string& name, int& value, int min, int max)
{
	ttext_box* widget = find_widget<ttext_box>(&window, id, false, true);
	std::string str = widget->get_value();
	value = atoi(str.c_str());
	if (value < min || value > max) {
		std::stringstream err;
		utils::string_map symbols;
		symbols["key"] = name;
		err << min;
		symbols["min"] = err.str();
		err.str("");
		err << max;
		symbols["max"] = err.str();;

		err.str("");
		err << vgettext("wesnoth-lib", "Range is [$min, $max], invalid '$key' value:", symbols) << " " << str;
		gui2::show_message(gui_.video(), "", err.str());
		return false;
	}
	return true;
}

void tcreate_hero::create(twindow& window)
{
	std::string str;
	int value;

	str = text_box_str(window, "name", _("Name"));
	if (str.empty()) return;
	h_.set_name(str);
	str = text_box_str(window, "surname", _("Surname"));
	if (str.empty()) return;
	h_.set_surname(str);

	if (!text_box_int(window, "leadership", _("Leadership"), value)) return;
	h_.leadership_ = ftofxp9(value);
	if (!text_box_int(window, "force", _("Force"), value)) return;
	h_.force_ = ftofxp9(value);
	if (!text_box_int(window, "intellect", _("Intellect"), value)) return;
	h_.intellect_ = ftofxp9(value);
	if (!text_box_int(window, "politics", _("Politics"), value)) return;
	h_.politics_ = ftofxp9(value);
	if (!text_box_int(window, "charm", _("Charm"), value)) return;
	h_.charm_ = ftofxp9(value);

	player_hero_ = h_;
	preferences::set_hero(player_hero_);

	window.set_retval(twindow::OK);
}

} // namespace gui2

