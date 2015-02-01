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

#include "game_display.hpp"
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
#include "gui/dialogs/mp_login.hpp"
#include "preferences_display.hpp"
#include "formula_string_utils.hpp"
#include "multiplayer.hpp"
#include "lobby.hpp"
#include "integrate.hpp"
#include <time.h>

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

tcreate_hero::tcreate_hero(display& disp, hero_map& heros)
	: disp_(disp)
	, heros_(heros)
	, h_(group.leader())
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

void tcreate_hero::set_button_int(twindow& window, const std::string& id, int value)
{
	tbutton* widget = find_widget<tbutton>(&window, id, false, true);
	widget->set_label(str_cast(value));
}

static int nb_catalogs = 8;
static const int catalog_items[] = {
	135,
	103,
	393,
	239,
	104,
	136,
	187,
	374
};

const char* field_ids[] = {
	"leadership",
	"force",
	"intellect",
	"spirit",
	"charm"
};

void refresh_username(twindow& window)
{
	std::stringstream strstr;

	tcontrol* control = find_widget<tcontrol>(&window, "username_png", false, true);
	if (!preferences::vip2()) {
		control->set_label(tintegrate::generate_img("misc/username.png"));
	} else {
		int day = 30;
		if (preferences::vip_expire() >= time(NULL)) {
			day = (preferences::vip_expire() - time(NULL)) / (24 * 3600);
		}
		strstr.str("");
		strstr << tintegrate::generate_img("misc/username-vip.png") << "(" << tintegrate::generate_format(day, "green") << ")";
		control->set_label(strstr.str());
	}
}

void tcreate_hero::pre_show(CVideo& video, twindow& window)
{
	std::stringstream strstr;

	refresh_username(window);
	refresh_resi_point(window);
	
	/**** Set the version number ****/
	ttext_box* user_widget = find_widget<ttext_box>(&window, "name", false, true);
	user_widget->set_label(h_.name());
	user_widget->set_active(false);

	user_widget = find_widget<ttext_box>(&window, "surname", false, true);
	const int max_surname_size = 5;
	user_widget->set_maximum_length(5);

	tcontrol* control;
	for (int i = 0; i < sizeof(field_ids) / sizeof(field_ids[0]); i ++) {
		control = find_widget<tbutton>(&window, field_ids[i], false, false);
		if (control) {
			connect_signal_mouse_left_click(
				*control
				, boost::bind(
				&tcreate_hero::field
				, this
				, boost::ref(window)
				, i));
		}
	}

	for (int i = 0; i < HEROS_MAX_ARMS; i ++) {
		strstr.str("");
		strstr << "arms" << i;
		control = find_widget<tbutton>(&window, strstr.str(), false, false);
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
	}
	for (int i = 0; i < HEROS_MAX_SKILL; i ++) {
		strstr.str("");
		strstr << "skill" << i;
		control = find_widget<tbutton>(&window, strstr.str(), false, false);
		if (control) {
			connect_signal_mouse_left_click(
				find_widget<tbutton>(&window, strstr.str(), false)
				, boost::bind(
				&tcreate_hero::adaptability
				, this
				, boost::ref(window)
				, (int)hero::SKILL
				, i));
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

	button = find_widget<tbutton>(&window, "side_feature", false, true);
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "side_feature", false)
		, boost::bind(
		&tcreate_hero::feature
		, this
		, boost::ref(window)
		, true));

	button = find_widget<tbutton>(&window, "tactic", false, true);
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "tactic", false)
		, boost::bind(
		&tcreate_hero::tactic
		, this
		, boost::ref(window)));

	button = find_widget<tbutton>(&window, "utype", false, true);
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "utype", false)
		, boost::bind(
		&tcreate_hero::utype
		, this
		, boost::ref(window)));

	button = find_widget<tbutton>(&window, "character", false, true);
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "character", false)
		, boost::bind(
		&tcreate_hero::character
		, this
		, boost::ref(window)));

	button = find_widget<tbutton>(&window, "catalog", false, true);
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "catalog", false)
		, boost::bind(
		&tcreate_hero::catalog
		, this
		, boost::ref(window)));

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
		&tcreate_hero::change_avatar
		, this
		, boost::ref(window)
		, true));

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "account", false)
		, boost::bind(
		&tcreate_hero::account
		, this
		, boost::ref(window)));
	strstr.str("");
	strstr << tintegrate::generate_img("misc/config.png~SCALE(24, 24)");
	find_widget<tbutton>(&window, "account", false).set_label(strstr.str());

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "avatar", false)
		, boost::bind(
		&tcreate_hero::synchronize
		, this
		, boost::ref(window)));
	strstr.str("");
	strstr << tintegrate::generate_format(_("Avatar"), "blue");
	find_widget<tbutton>(&window, "avatar", false).set_label(strstr.str());

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "create", false)
		, boost::bind(
		&tcreate_hero::create
		, this
		, boost::ref(window)));
	strstr.str("");
	strstr << tintegrate::generate_format(_("OK"), "blue");
	find_widget<tbutton>(&window, "create", false).set_label(strstr.str());

	refresh_field_ui(window);
}

void tcreate_hero::post_show(twindow& window)
{
}

void tcreate_hero::refresh_resi_point(twindow& window) const
{
	int used_point = h_.calculate_used_point();
	int resi_point = group.calculate_total_point() - used_point;

	std::string color;
	if (resi_point > 0) {
		color = "green";
	} else if (resi_point == 0) {
		color = "yellow";
	} else {
		color = "red";
	}

	const int noble = preferences::noble();
	std::stringstream strstr;
	strstr.str("");
	strstr << tintegrate::generate_format(unit_types.leader_noble(noble).name());
	strstr << tintegrate::generate_format(resi_point, color);

	tcontrol* control = find_widget<tcontrol>(&window, "point", false, true);
	control->set_label(strstr.str());

	control = find_widget<tcontrol>(&window, "create", false, true);
	control->set_active(resi_point >= 0);
}

void tcreate_hero::regenerate(twindow& window)
{
	// stronger
	h_.leadership_ = ftofxp9(90);
	h_.force_ = ftofxp9(60);
	h_.intellect_ = ftofxp9(60);
	h_.spirit_ = ftofxp9(50);
	h_.charm_ = ftofxp9(95);

	for (int i = 0; i < HEROS_MAX_ARMS; i ++) {
		if (i == hero_arms_t1) {
			h_.arms_[i] = ftofxp12(3);
		} else if (i == hero_arms_t4) {
			h_.arms_[i] = ftofxp12(2);
		} else {
			h_.arms_[i] = ftofxp12(0);
		}
	}
	for (int i = 0; i < HEROS_MAX_SKILL; i ++) {
		if (i == hero_skill_encourage) {
			h_.skill_[i] = ftofxp12(3);
		} else if (i == hero_skill_formation) {
			h_.skill_[i] = ftofxp12(2);
		} else {
			h_.skill_[i] = ftofxp12(0);
		}
	}

	if (h_.feature_ == HEROS_NO_FEATURE) {
		h_.feature_ = 82;
	}
	if (h_.side_feature_ == HEROS_NO_FEATURE) {
		h_.side_feature_ = hero_feature_arms;
	}
	if (h_.tactic_ == HEROS_NO_TACTIC) {
		h_.tactic_ = ttactic::min_complex_index + 2;
	}
	if (h_.utype_ == HEROS_NO_UTYPE) {
		h_.utype_ = 0;
	}
	if (h_.character_ == HEROS_NO_CHARACTER) {
		h_.character_ = 9;
	}

	refresh_resi_point(window);
	refresh_field_ui(window);

	// portrait dirty
	window.invalidate_layout();
}

void tcreate_hero::refresh_field_ui(twindow& window)
{
	tcontrol* portrait = find_widget<tcontrol>(&window, "portrait", false, true);
	portrait->set_label(h_.image(true));

	tcontrol* control = find_widget<tcontrol>(&window, "gender", false, true);
	control->set_label(hero::gender_str(h_.gender_));
	
	ttext_box* user_widget = find_widget<ttext_box>(&window, "surname", false, true);
	user_widget->set_label(h_.surname());

	set_button_int(window, "leadership", fxptoi9(h_.leadership_));
	set_button_int(window, "force", fxptoi9(h_.force_));
	set_button_int(window, "intellect", fxptoi9(h_.intellect_));
	set_button_int(window, "spirit", fxptoi9(h_.spirit_));
	set_button_int(window, "charm", fxptoi9(h_.charm_));

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

	control = find_widget<tcontrol>(&window, "feature", false, true);
	control->set_label(hero::feature_str(h_.feature_));
	
	control = find_widget<tcontrol>(&window, "side_feature", false, true);
	control->set_label(hero::feature_str(h_.side_feature_));
	
	control = find_widget<tcontrol>(&window, "tactic", false, true);
	if (h_.tactic_ != HEROS_NO_TACTIC) {
		control->set_label(unit_types.tactic(h_.tactic_).name());
	} else {
		control->set_label("");
	}
	
	control = find_widget<tcontrol>(&window, "utype", false, true);
	if (h_.utype_ != HEROS_NO_UTYPE) {
		control->set_label(unit_types.keytype(h_.utype_)->type_name());
	} else {
		control->set_label("");
	}

	control = find_widget<tcontrol>(&window, "character", false, true);
	if (h_.character_ != HEROS_NO_CHARACTER) {
		control->set_label(unit_types.character(h_.character_).name());
	} else {
		control->set_label("");
	}

	control = find_widget<tcontrol>(&window, "catalog", false, true);
	strstr.str("");
	for (int i = 0; i < nb_catalogs; i ++) {
		hero& h = heros_[catalog_items[i]];
		if (h_.base_catalog_ == h.base_catalog_) {
			strstr << h.name();
			break;
		}
	}
	if (strstr.str().empty()) {
		hero& h = heros_[catalog_items[0]];
		h_.base_catalog_ = h.base_catalog_;
		strstr << h.name();
	}
	control->set_label(strstr.str());
}

void tcreate_hero::account(twindow& window)
{
	gui2::tmp_login dlg(disp_, heros_, "");
	dlg.show(disp_.video());

	if (dlg.dirty()) {
		// so far maybe not check version. for example, error password result reinput, and this is right.
		// force "seesion" to check version.
		http::membership m = http::session(disp_, heros_);
		if (m.uid >= 0) {
			group.from_local_membership(disp_, heros_, m, true);
		}
		game_config::local_only = m.uid < 0;
		

		h_ = group.leader();
		h_.set_name(group.leader().name());
		ttext_box* user_widget = find_widget<ttext_box>(&window, "name", false, true);
		user_widget->set_label(h_.name());

		refresh_username(window);
		refresh_resi_point(window);
		refresh_field_ui(window);

		// portrait dirty
		window.invalidate_layout();

		lobby.process_error("Login changed!");
	}
}

void tcreate_hero::synchronize(twindow& window)
{
	if (http::avatar_hero(disp_, heros_, h_.uid(), false)) {
		change_avatar(window, false);
	}
}

void tcreate_hero::change_avatar(twindow& window, bool gender)
{
	tcontrol* regenerate_button = find_widget<tcontrol>(&window, "regenerate", false, true);
	tcontrol* gender_button = find_widget<tcontrol>(&window, "gender", false, true);
	tcontrol* portrait = find_widget<tcontrol>(&window, "portrait", false, true);
	
	if (gender) {
		if (h_.gender_ == hero_gender_male) {
			// male --> female
			h_.set_image(female_number_);
			h_.gender_ = hero_gender_female;
		} else {
			// female --> male
			h_.set_image(male_number_);
			h_.gender_ = hero_gender_male;
		}
	} else {
		h_.set_image(h_.image_);
		image::flush_cache(true);
		portrait->set_label("");
	}
	gender_button->set_label(hero::gender_str(h_.gender_));
	portrait->set_label(h_.image(true));

	// regenerate_button->set_dirty();
	// find_widget<tbutton>(&window, "avatar", false).set_dirty();
	// portrait dirty
	window.invalidate_layout();
}

void tcreate_hero::field(twindow& window, int index)
{
	std::vector<tval_str> items;

	items.push_back(tval_str(50, "+50"));
	items.push_back(tval_str(20, "+20"));
	items.push_back(tval_str(10, "+10"));
	items.push_back(tval_str(5, "+5"));
	items.push_back(tval_str(-5, "-5"));
	items.push_back(tval_str(-10, "-10"));
	items.push_back(tval_str(-20, "-20"));
	items.push_back(tval_str(-50, "-50"));

	gui2::tcombo_box dlg(items, 50);
	dlg.show(disp_.video());

	int increase = dlg.selected_val();
	int value;
	uint16_t* ptr;

	if (!strcmp(field_ids[index], "leadership")) {
		ptr = &h_.leadership_;
	} else if (!strcmp(field_ids[index], "force")) {
		ptr = &h_.force_;
	} else if (!strcmp(field_ids[index], "intellect")) {
		ptr = &h_.intellect_;
	} else if (!strcmp(field_ids[index], "spirit")) {
		ptr = &h_.spirit_;
	} else {
		ptr = &h_.charm_;
	}
	value = fxptoi9(*ptr);
	value += increase;

	if (value < 1) {
		value = 1;
	} else if (value > fxptoi9(65535)) {
		value = fxptoi9(65535);
	}
	*ptr = ftofxp9(value);

	tcontrol* label = find_widget<tcontrol>(&window, field_ids[index], false, true);
	label->set_label(str_cast(value));

	refresh_resi_point(window);
}

void tcreate_hero::adaptability(twindow& window, int type, int index)
{
	char text[32];
	std::vector<tval_str> items;
	int def;
	std::stringstream str;

	for (int idx = 0; idx <= hero_adaptability_max; idx ++) {
		sprintf(text, "%s%i", HERO_PREFIX_STR_ADAPTABILITY, idx);
		items.push_back(tval_str(idx, dgettext("wesnoth-hero", text))); 
	}

	if (type == hero::ARMS) {
		def = fxptoi12(h_.arms_[index]);
		str << "text_arms" << index;
	} else {
		def = fxptoi12(h_.skill_[index]);
		str << "text_skill" << index;
	}

	gui2::tcombo_box dlg(items, def);
	dlg.show(disp_.video());

	tcontrol* label = find_widget<tcontrol>(&window, str.str(), false, true);
	int selected_val = dlg.selected_val();
	if (type == hero::ARMS) {
		h_.arms_[index] = ftofxp12(selected_val);
		label->set_label(hero::adaptability_str2(h_.arms_[index]));
	} else {
		h_.skill_[index] = ftofxp12(selected_val);
		label->set_label(hero::adaptability_str2(h_.skill_[index]));
	}

	refresh_resi_point(window);
}

int max_side_feature_level()
{
	int noble = preferences::noble();
	if (noble <= 2) {
		return 0;
	}
	return 1;
}

void tcreate_hero::feature(twindow& window, bool side)
{
	std::vector<tval_str> items;
	int def = HEROS_NO_FEATURE;
	std::stringstream strstr;
	const int max_feature_level = side? max_side_feature_level(): 4;

	items.push_back(tval_str(HEROS_NO_FEATURE, " "));
	std::vector<int> features = hero::valid_features();
	for (std::vector<int>::const_iterator itor = features.begin(); itor != features.end(); ++ itor) {
		int level = unit_types.feature_level(*itor);
		if (level > max_feature_level) {
			continue;
		}
		strstr.str("");
		strstr << hero::feature_str(*itor) << "(" << level << ")";
		items.push_back(tval_str(*itor, strstr.str()));
		if (side) {
			if (h_.side_feature_ == *itor) {
				def = h_.side_feature_;
			}
		} else if (h_.feature_ == *itor) {
			def = h_.feature_;
		}
	}

	std::string str = side? "side_feature": "feature";
	
	gui2::tcombo_box dlg(items, def);
	dlg.show(disp_.video());

	tcontrol* label = find_widget<tcontrol>(&window, str, false, true);
	if (side) {
		h_.side_feature_ = dlg.selected_val();
		str = hero::feature_str(h_.side_feature_);
	} else {
		h_.feature_ = dlg.selected_val();
		str = hero::feature_str(h_.feature_);
	}
	label->set_label(str);

	refresh_resi_point(window);
}

void tcreate_hero::tactic(twindow& window)
{
	std::stringstream strstr;
	std::vector<tval_str> items;

	items.push_back(tval_str(HEROS_NO_TACTIC, " "));
	const std::map<int, ttactic>& tactics = unit_types.tactics();
	for (std::map<int, ttactic>::const_iterator it = tactics.begin(); it != tactics.end(); ++ it) {
		const ttactic& t = it->second;

		strstr.str("");
		strstr << t.name() << "(" << t.level() << ")";
		items.push_back(tval_str(t.index(), strstr.str()));
	}

	gui2::tcombo_box dlg(items, h_.tactic_);
	dlg.show(disp_.video());

	h_.tactic_ = dlg.selected_val();

	tcontrol* label = find_widget<tcontrol>(&window, "tactic", false, true);
	if (h_.tactic_ != HEROS_NO_TACTIC) {
		label->set_label(unit_types.tactic(h_.tactic_).name());
	} else {
		label->set_label("");
	}

	refresh_resi_point(window);
}

void tcreate_hero::utype(twindow& window)
{
	std::vector<tval_str> items;

	items.push_back(tval_str(HEROS_NO_UTYPE, " "));
	const std::map<int, const unit_type*>& keytypes = unit_types.keytypes();
	for (std::map<int, const unit_type*>::const_iterator it = keytypes.begin(); it != keytypes.end(); ++ it) {
		const unit_type& ut = *(it->second);
		items.push_back(tval_str(it->first, ut.type_name()));
	}

	gui2::tcombo_box dlg(items, h_.utype_);
	dlg.show(disp_.video());

	h_.utype_ = dlg.selected_val();

	tcontrol* label = find_widget<tcontrol>(&window, "utype", false, true);
	label->set_label(items[dlg.selected_index()].str);
}

void tcreate_hero::character(twindow& window)
{
	std::vector<tval_str> items;

	items.push_back(tval_str(HEROS_NO_CHARACTER, " "));
	const std::map<int, tcharacter>& characters = unit_types.characters();
	for (std::map<int, tcharacter>::const_iterator it = characters.begin(); it != characters.end(); ++ it) {
		const tcharacter& t = it->second;
		items.push_back(tval_str(t.index(), t.name()));
	}

	gui2::tcombo_box dlg(items, h_.character_);
	dlg.show(disp_.video());

	h_.character_ = dlg.selected_val();

	tcontrol* label = find_widget<tcontrol>(&window, "character", false, true);
	label->set_label(items[dlg.selected_index()].str);

	refresh_resi_point(window);
}

void tcreate_hero::catalog(twindow& window)
{
	std::stringstream str;
	std::vector<tval_str> items;
	int activity_index = -1;

	for (int i = 0; i < nb_catalogs; i ++) {
		hero& h = heros_[catalog_items[i]];
		str.str("");
		str << h.name() << "(";
		int value = h.base_catalog_;
		str << value << ")";
		items.push_back(tval_str(h.base_catalog_, str.str()));
	}

	gui2::tcombo_box dlg(items, h_.base_catalog_);
	dlg.show(disp_.video());

	tcontrol* label = find_widget<tcontrol>(&window, "catalog", false, true);
	hero& h = heros_[catalog_items[dlg.selected_index()]];
	h_.base_catalog_ = h.base_catalog_;
	h_.float_catalog_ = ftofxp8(h_.base_catalog_);
	label->set_label(h.name());
}

std::string tcreate_hero::text_box_str(twindow& window, const std::string& id, const std::string& name, bool allow_empty)
{
	std::stringstream err;
	utils::string_map symbols;

	ttext_box* widget = find_widget<ttext_box>(&window, id, false, true);
	std::string str = widget->label();
	if (!allow_empty && str.empty()) {
		symbols["key"] = tintegrate::generate_format(name, "red");
		
		err << vgettext("wesnoth-lib", "Invalid '$key' value, not accept empty", symbols);
		gui2::show_message(disp_.video(), "", err.str());
		return str;
	} else if (id == "name" || id == "surname") {
		if (str.find("<") != std::string::npos) {
			symbols["str"] = name;
			symbols["char"] = "<";
			err << vgettext("wesnoth-lib", "Include unsupportable character: $char, invalid '$str' value", symbols);
			gui2::show_message(disp_.video(), "", err.str());
			return null_str;
		}
	}
	return str;
}

bool tcreate_hero::text_box_int(twindow& window, const std::string& id, const std::string& name, int& value, int min, int max)
{
	ttext_box* widget = find_widget<ttext_box>(&window, id, false, true);
	std::string str = widget->label();
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
		gui2::show_message(disp_.video(), "", err.str());
		return false;
	}
	return true;
}

bool tcreate_hero::create(twindow& window)
{
	std::string str = text_box_str(window, "name", _("Name"));
	if (str.empty()) return false;

	str = text_box_str(window, "surname", _("Surname"));
	if (str.empty()) return false;
	h_.set_surname(str);

	std::string field_str = h_.field_to_str();
	std::string translatable_str = h_.translatable_to_str();
	hero& player = group.leader();
	if (player.field_to_str() != field_str || player.translatable_to_str() != translatable_str) {
		std::map<int, std::string> block;
		block.insert(std::make_pair((int)http::block_tag_field, field_str));
		block.insert(std::make_pair((int)http::block_tag_translatable, translatable_str));
		http::upload_data(disp_, heros_, block, true);
		player = h_;
		preferences::set_hero(heros_, player);

		group.adjust_members_according_to_leader(heros_);
	}
	window.set_retval(twindow::OK);

	return true;
}

} // namespace gui2

