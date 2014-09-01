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

#include "gui/dialogs/user_message.hpp"

#include "game_display.hpp"
#include "game_config.hpp"
#include "game_preferences.hpp"
#include "gettext.hpp"
#include "gui/dialogs/helper.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/toggle_panel.hpp"
#include "gui/widgets/scroll_label.hpp"
#include "gui/widgets/scrollbar_panel.hpp"
#ifdef GUI2_EXPERIMENTAL_LISTBOX
#include "gui/widgets/list.hpp"
#else
#include "gui/widgets/listbox.hpp"
#endif
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/combo_box.hpp"
#include "gui/dialogs/group.hpp"
#include "gui/dialogs/hero.hpp"
#include "gui/dialogs/edit_box.hpp"
#include "preferences_display.hpp"
#include "formula_string_utils.hpp"
#include "filesystem.hpp"
#include "help.hpp"

#include <boost/bind.hpp>
#include <boost/foreach.hpp>

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

REGISTER_DIALOG(user_message)

tuser_message::tuser_message(game_display& disp, hero_map& heros, const config& game_config)
	: disp_(disp)
	, heros_(heros)
	, game_config_(game_config)
	, current_page_(NONE_PAGE)
	, page_panel_(NULL)
{
}

tuser_message::~tuser_message()
{
}

void tuser_message::post_build(CVideo& video, twindow& window)
{
}

void tuser_message::sheet_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	int toggled_page = toggle->get_data();

	if (!toggle->get_value()) {
		// At most select one page. recheck it!
		toggle->set_value(true);
	} else {
		for (std::map<int, ttoggle_button*>::iterator it = sheet_.begin(); it != sheet_.end(); ++ it) {
			if (it->second == toggle) {
				continue;
			}
			it->second->set_value(false);
		}
		swap_page(*toggle->get_window(), toggled_page, true);
	}
}

void tuser_message::set_label_int(twindow& window, const std::string& id, int value)
{
	tlabel* widget = find_widget<tlabel>(&window, id, false, true);
	widget->set_label(lexical_cast<std::string>(value));
}

void tuser_message::pre_show(CVideo& video, twindow& window)
{
	window.set_enter_disabled(true);
	window.set_escape_disabled(true);

	sheet_.insert(std::make_pair((int)MESSAGE_PAGE, find_widget<ttoggle_button>(&window, "message", false, true)));
	sheet_.insert(std::make_pair((int)SIEGE_RECORD_PAGE, find_widget<ttoggle_button>(&window, "siege_record", false, true)));
	sheet_.insert(std::make_pair((int)EMPLOYEE_PAGE, find_widget<ttoggle_button>(&window, "employ_hero", false, true)));
	find_widget<ttoggle_button>(&window, "employ_hero", false, true)->set_visible(twidget::INVISIBLE);
	sheet_.insert(std::make_pair((int)RANK_SCORE_PAGE, find_widget<ttoggle_button>(&window, "rank_score", false, true)));
	find_widget<ttoggle_button>(&window, "rank_score", false, true)->set_visible(twidget::INVISIBLE);
	for (std::map<int, ttoggle_button*>::iterator it = sheet_.begin(); it != sheet_.end(); ++ it) {
		it->second->set_callback_state_change(boost::bind(&tuser_message::sheet_toggled, this, _1));
		it->second->set_data(it->first);
	}

	std::stringstream strstr;
	if (group.message(tgroup::msg_message)) {
		strstr.str("");
		strstr << dgettext("wesnoth-lib", "Message");
		strstr << help::tintegrate::generate_img("misc/mini-new.png");
		find_widget<ttoggle_button>(&window, "message", false, true)->set_label(strstr.str());
	}
	if (group.message(tgroup::msg_siege)) {
		strstr.str("");
		strstr << dgettext("wesnoth-lib", "Siege record");
		strstr << help::tintegrate::generate_img("misc/mini-new.png");
		find_widget<ttoggle_button>(&window, "siege_record", false, true)->set_label(strstr.str());
	}

	page_panel_ = find_widget<tscrollbar_panel>(&window, "page", false, true);
	swap_page(window, MESSAGE_PAGE, false);
	sheet_.begin()->second->set_value(true);
}

void tuser_message::post_show(twindow& window)
{
	if (group.message_count()) {
		group.message_from_str(null_str);
	}
}

enum {msgid_signin, msgid_title, msgid_inapp, msgid_field, msgid_version};
enum {title_score, title_attacker, title_defender, title_rpg, title_tower};

std::string parse_message_content(const std::string& content)
{
	const std::string unknown_standard_message = dsgettext("wesnoth-lib", "Not conform to the format message");
	std::string standard_message_prefix = game_config::data_server_magic_word + "|";
	std::string parsed;

	if (content.size() < standard_message_prefix.size() + 1) {
		return content;
	}
	if (memcmp(content.c_str(), standard_message_prefix.c_str(), standard_message_prefix.size())) {
		return content;
	}
	std::vector<std::string> vstr = utils::split(content.substr(standard_message_prefix.size()), '|');
	if (vstr.size() < 2) {
		return unknown_standard_message;
	}

	utils::string_map symbols;
	int msgid = lexical_cast_default<int>(vstr[0]);
	if (msgid == msgid_signin) {
		int subid = lexical_cast_default<int>(vstr[1]);
		if (subid == 0) {
			if (vstr.size() != 5) {
				return unknown_standard_message;
			}
			symbols["continue"] = vstr[2];
			symbols["coin"] = vstr[3];
			symbols["score"] = vstr[4];
			parsed = vgettext("wesnoth-lib", "You continue to sign in $continue days, reward $coin coin and $score score.", symbols);
		} else if (subid == 1) {
			if (vstr.size() != 4) {
				return unknown_standard_message;
			}
			symbols["break"] = vstr[2];
			symbols["max_break"] = vstr[3];
			parsed = vgettext("wesnoth-lib", "You have broken $break days, exceeded $max_break, and result to clear Sign in data.", symbols);
		} else {
			return unknown_standard_message;
		}

	} else if (msgid == msgid_title) {
		int subid = lexical_cast_default<int>(vstr[1]);
		if (subid == 0) {
			if (vstr.size() != 8) {
				return unknown_standard_message;
			}
			int title = lexical_cast_default<int>(vstr[2]);
			symbols["title"] = form_title_str(title);
			symbols["days"] = vstr[3];
			symbols["coin"] = vstr[6];
			symbols["score"] = vstr[7];
			if (title == title_score) {
				symbols["coin2"] = vstr[4];
				symbols["score2"] = vstr[5];
				parsed = vgettext("wesnoth-lib", "You hold $coin2 coin and $score2 score, most score, so win $title as an honour. Reward $coin coin and $score score.", symbols);

			} else if (title == title_attacker) {
				symbols["count"] = vstr[5];
				parsed = vgettext("wesnoth-lib", "In the last $days days, you attack other city to $count times, most times, so win $title as an honour. Reward $coin coin and $score score.", symbols);

			} else if (title == title_defender) {
				symbols["count"] = vstr[5];
				parsed = vgettext("wesnoth-lib", "In the last $days days, your city was attacked to $count times, most times, so win $title as an honour. Reward $coin coin and $score score.", symbols);

			} else if (title == title_rpg) {
				symbols["count"] = vstr[5];
				parsed = vgettext("wesnoth-lib", "In the last $days days, you pass $count times RPG scenario, most times, so win $title as an honour. Reward $coin coin and $score score.", symbols);

			} else if (title == title_tower) {
				symbols["count"] = vstr[5];
				parsed = vgettext("wesnoth-lib", "In the last $days days, you pass $count times Tower scenario, most times, so win $title as an honour. Reward $coin coin and $score score.", symbols);

			} else {
				return unknown_standard_message;
			}
			
		} else {
			return unknown_standard_message;
		}

	} else if (msgid == msgid_inapp) {
		int subid = lexical_cast_default<int>(vstr[1]);
		if (subid == 0) {
			if (vstr.size() != 3) {
				return unknown_standard_message;
			}
			symbols["day"] = help::tintegrate::generate_format(vstr[2], "green");
			parsed = vgettext("wesnoth-lib", "Inapp-purchase successfully! Increase $day days VIP membership.", symbols);

		} else if (subid > 0 && subid <= 4) {
			if (vstr.size() != 4) {
				return unknown_standard_message;
			}
			symbols["coin"] = help::tintegrate::generate_format(vstr[2], "green");
			symbols["score"] = help::tintegrate::generate_format(vstr[3], "green");
			parsed = vgettext("wesnoth-lib", "Inapp-purchase successfully! Increase $coin coin and $score score.", symbols);

		} else if (subid == 5) {
			if (vstr.size() != 4) {
				return unknown_standard_message;
			}
			symbols["coin"] = help::tintegrate::generate_format(vstr[2], "yellow");
			symbols["score"] = help::tintegrate::generate_format(vstr[3], "yellow");
			symbols["contact"] = help::tintegrate::generate_format(game_config::service_email, "green");
			parsed = vgettext("wesnoth-lib", "System modified your data: $coin coin, $score score. If you have any questions, please contact $contact.", symbols);
		} else {
			return unknown_standard_message;
		}

	} else {
		return unknown_standard_message;
	}
	
	return parsed;
}

void tuser_message::send_message(twindow& window)
{
	// std::string initial_str = dsgettext("wesnoth-lib", "Announcement content");
	std::stringstream initial_str;
	initial_str << game_config::data_server_magic_word << "|3|0|coin=20,score=300|service@leagor.com";
	gui2::tedit_box dlg(disp_, heros_, initial_str.str(), gui2::tedit_box::SEND_MESSAGE);
	try {
		dlg.show(disp_.video());
	} catch(twml_exception& e) {
		e.show(disp_);
	}
	std::string receiver_str = dlg.get_receiver_str();
	std::string content_str = dlg.get_result_str();
	if (content_str.empty() || receiver_str.empty()) {
		return;
	}

	http::tmessage_record message;
	message.sender = http::INVALID_UID;
	message.receiver_username = receiver_str;
	message.create_time = time(NULL);
	message.content = content_str;

	http::membership m = http::upload_message(disp_, heros_, http::tmessage_record(message));
	if (m.uid >= 0) {
		gui2::show_message(disp_.video(), "", dsgettext("wesnoth-lib", "Send successfully!"));
		messages_ = http::list_message(disp_, heros_);
		fill_message_table(window, messages_);
	}
}

void tuser_message::refresh_message_2_label(twindow& window, const std::string& content)
{
	tscroll_label& label = find_widget<tscroll_label>(&window, "detail", false);
	label.set_label(content);
}

void tuser_message::message_selected(twindow& window)
{
	tlistbox& list = find_widget<tlistbox>(&window, "default", false);

	int index = list.get_selected_row();
	if (index >= 0) {
		refresh_message_2_label(window, parse_message_content(messages_[index].content));
	}
}

void tuser_message::fill_message_table(twindow& window, const std::vector<http::tmessage_record>& messages)
{
	std::stringstream strstr;
	tlistbox* list = find_widget<tlistbox>(&window, "default", false, true);
	list->clear();

	const std::string username = group.leader().name();
	BOOST_FOREACH (const http::tmessage_record& message, messages) {
		std::map<std::string, string_map> data;
		string_map item;

		strstr.str("");
		if (message.sender_username == username) {
			strstr << help::tintegrate::generate_img("misc/up.png");
		} else {
			strstr << help::tintegrate::generate_img("misc/down.png");
		}
		if (message.create_time > group.browsed_time()) {
			strstr << help::tintegrate::generate_img("misc/mini-new.png", help::tintegrate::BACK);
		}
		item["label"] = strstr.str();
		data.insert(std::make_pair("flag", item));

		strstr.str("");
		if (!message.sender_username.empty()) {
			if (message.sender_username == username) {
				strstr << help::tintegrate::generate_format(message.sender_username, "yellow");
			} else if (message.sender_username == game_config::broadcast_username) {
				strstr << help::tintegrate::generate_img("misc/broadcast.png");
			} else {
				strstr << message.sender_username;
			}
		}
		item["label"] = strstr.str();
		data.insert(std::make_pair("sender", item));

		strstr.str("");
		if (!message.receiver_username.empty()) {
			if (message.receiver_username == username) {
				strstr << help::tintegrate::generate_format(message.receiver_username, "yellow");
			} else if (message.receiver_username == game_config::broadcast_username) {
				strstr << help::tintegrate::generate_img("misc/broadcast.png");
			} else {
				strstr << message.receiver_username;
			}
		}
		item["label"] = strstr.str();
		data.insert(std::make_pair("receiver", item));

		item["label"] = format_time_date(message.create_time);
		data.insert(std::make_pair("time", item));

		item["label"] = font::make_text_ellipsis(parse_message_content(message.content), 10);
		data.insert(std::make_pair("content", item));

		list->add_row(data);
	}
	list->set_callback_value_change(dialog_callback<tuser_message, &tuser_message::message_selected>);
	if (!messages_.empty()) {
		refresh_message_2_label(window, parse_message_content(messages_.front().content));
	}

	window.invalidate_layout();
}

void tuser_message::detail_employee(twindow& window)
{
	tlistbox& list = find_widget<tlistbox>(&window, "default", false);
	tgrid* grid_ptr = list.get_row_grid(list.get_selected_row());
	int number = dynamic_cast<ttoggle_panel*>(grid_ptr->find("_toggle", true))->get_data();

	hero& base = heros_[number];
	hero* h_ptr = &base;
	if (group.exist_member(number)) {
		h_ptr = group.member_from_base(base).h;
	}
	
	hero& h = *h_ptr;
	gui2::thero dlg(heros_, h, &base);
	try {
		dlg.show(disp_.video());
	} catch (twml_exception& e) {
		e.show(disp_);
	}
}

void tuser_message::detail_group(twindow& window)
{
	tlistbox& list = find_widget<tlistbox>(&window, "score_table", false);
	tgrid* grid_ptr = list.get_row_grid(list.get_selected_row());
	int uid = dynamic_cast<ttoggle_panel*>(grid_ptr->find("_toggle", true))->get_data();

	http::membership m = http::membership_from_uid(disp_, heros_, false, uid);
	if (m.uid < 0) {
		return;
	}

	tgroup g;
	g.from_membership(heros_, m);
	gui2::tgroup2 dlg(disp_, heros_, game_config_, g, true);
	try {
		dlg.show(disp_.video());
	} catch (twml_exception& e) {
		e.show(disp_);
	}
}

void tuser_message::fill_message(twindow& window)
{
	messages_ = http::list_message(disp_, heros_);
	fill_message_table(window, messages_);

	std::stringstream strstr;
	tbutton* button = find_widget<tbutton>(&window, "send", false, true);
	connect_signal_mouse_left_click(
		*button
		, boost::bind(
		&tuser_message::send_message
		, this
		, boost::ref(window)));
	strstr.str("");
	strstr << help::tintegrate::generate_format(_("Send"), "blue");
	button->set_label(strstr.str());
	if (group.leader().name() != "ancientcc") {
		button->set_visible(twidget::INVISIBLE);
	}
}

std::string score_str(int score)
{
	std::stringstream strstr;
	if (score) {
		strstr << " " << help::tintegrate::generate_img("misc/score.png~SCALE(24, 24)");
		if (score > 0) {
			strstr << help::tintegrate::generate_format(score, "green");
		} else {
			strstr << help::tintegrate::generate_format(score, "red");
		}
	}
	return strstr.str();
}

void tuser_message::fill_siege_record(twindow& window)
{
	std::stringstream strstr;
	tlistbox* list = find_widget<tlistbox>(&window, "record_table", false, true);
	list->clear();

	const std::string username = group.leader().name();
	std::vector<http::tsiege_record> records = http::list_siege(disp_, heros_);
	for (std::vector<http::tsiege_record>::const_iterator it = records.begin(); it != records.end(); ++ it) {
		const http::tsiege_record& rec = *it;
		std::map<std::string, string_map> data;
		string_map item;

		item["label"] = format_time_date(rec.create_time);
		data.insert(std::make_pair("createtime", item));

		strstr.str("");
		if (!rec.attacker_username.empty()) {
			if (rec.attacker_username == username) {
				strstr << help::tintegrate::generate_format(rec.attacker_username, "yellow");
			} else {
				strstr << rec.attacker_username;
			}
			strstr << score_str(rec.score);
		}
		item["label"] = strstr.str();
		data.insert(std::make_pair("attacker", item));

		strstr.str("");
		if (!rec.defender_username.empty()) {
			if (rec.defender_username == username) {
				strstr << help::tintegrate::generate_format(rec.defender_username, "yellow");
			} else {
				strstr << rec.defender_username;
			}
		}
		item["label"] = strstr.str();
		data.insert(std::make_pair("defender", item));

		strstr.str("");
		if (!rec.atk_reinforce_username.empty()) {
			if (rec.atk_reinforce_username == username) {
				strstr << help::tintegrate::generate_format(rec.atk_reinforce_username, "yellow");
			} else {
				strstr << rec.atk_reinforce_username;
			}
		}
		item["label"] = strstr.str();
		data.insert(std::make_pair("atk_reinforce", item));

		strstr.str("");
		if (!rec.def_reinforce_username.empty()) {
			if (rec.def_reinforce_username == username) {
				strstr << help::tintegrate::generate_format(rec.def_reinforce_username, "yellow");
			} else {
				strstr << rec.def_reinforce_username;
			}
		}
		item["label"] = strstr.str();
		data.insert(std::make_pair("def_reinforce", item));

		strstr.str("");
		if (rec.result == http::siege_result_victory) {
			strstr << dsgettext("wesnoth-lib", "Victory");
		} else if (rec.result == http::siege_result_defeat) {
			strstr << dsgettext("wesnoth-lib", "Defeat");
		} else if (rec.result == http::siege_result_fallen) {
			strstr << dsgettext("wesnoth-lib", "City fallen");
		} else {
			strstr << "Unknown";
		}
		if (rec.employee != HEROS_INVALID_NUMBER) {
			strstr << ", " << help::tintegrate::generate_format(heros_[rec.employee].name(), "red");
		}
		item["label"] = strstr.str();
		data.insert(std::make_pair("result", item));

		list->add_row(data);
	}
	window.invalidate_layout();
}

void tuser_message::fill_employee(twindow& window)
{
	std::stringstream strstr;
	
	std::map<int, http::temployee> employees = http::list_employee(disp_, heros_);

	tlistbox* list = find_widget<tlistbox>(&window, "default", false, true);
	list->clear();

	std::vector<int> numbers;
	for (hero_map::iterator it = heros_.begin(); it != heros_.end(); ++ it) {
		hero& h = *it;
		if (h.number_ >= hero_map::map_size_from_dat) {
			break;
		}
		if (!h.get_flag(hero_flag_employee)) {
			continue;
		}
		const http::temployee* employee_ptr = NULL;
		if (!employees.empty()) {
			std::map<int, http::temployee>::iterator find = employees.find(h.number_);
			if (find != employees.end()) {
				employee_ptr = &find->second;
			}
		}
		const tgroup::tmember* member_ptr = NULL;
		if (group.exist_member(h.number_)) {
			member_ptr = &group.member_from_base(h);
		}

		/*** Add list item ***/
		string_map list_item;
		std::map<std::string, string_map> list_item_item;

		strstr.str("");
		strstr << h.image();
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("icon", list_item));

		// name
		strstr.str("");
		if (h.utype_ != HEROS_NO_UTYPE) {
			const unit_type* ut = unit_types.keytype(h.utype_);
			strstr << help::tintegrate::generate_img(ut->icon()) << "\n";
		}
		strstr << h.name();
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("name", list_item));

		// level
		strstr.str("");
		int level = -1;
		if (employee_ptr) {
			level = employee_ptr->level;
		} else if (member_ptr) {
			level = member_ptr->level;
		}
		if (level == -1) {
			strstr << "--";
		} else if (level / game_config::levels_per_rank >= 2) {
			strstr << _("rank^Gold");
		} else if (level / game_config::levels_per_rank >= 1) {
			strstr << _("rank^Silver");
		} else {
			strstr << _("rank^Copper");
		}
		if (level != -1) {
			strstr << "(" << (level % game_config::levels_per_rank + 1) << ")";
		}
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("level", list_item));

		// cost
		strstr.str("");
		if (employee_ptr) {
			strstr << employee_ptr->score;
			strstr << help::tintegrate::generate_img("misc/score.png~SCALE(24, 24)");
		} else {
			strstr << "--";
		}
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

		// ownership
		strstr.str("");
		if (employee_ptr) {
			if (employee_ptr->username == group.leader().name()) {
				strstr << help::tintegrate::generate_format(employee_ptr->username, "green");
			} else {
				strstr << employee_ptr->username;
			}
			if (employee_ptr->lock) {
				strstr << help::tintegrate::generate_img("misc/lock.png");;
			}
		} else {
			strstr << help::tintegrate::generate_img("misc/unknown.png");
		}
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("ownership", list_item));

		strstr.str("");
		strstr << help::tintegrate::generate_img("misc/browse.png");;
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("browse", list_item));

		list->add_row(list_item_item);

		tgrid* grid_ptr = list->get_row_grid(list->get_item_count() - 1);
		twidget* widget = grid_ptr->find("human", false);
		widget->set_visible(twidget::INVISIBLE);

		ttoggle_panel* toggle = dynamic_cast<ttoggle_panel*>(grid_ptr->find("_toggle", true));
		toggle->set_data(h.number_);

		connect_signal_mouse_left_click(
				find_widget<tbutton>(grid_ptr, "browse", true)
				, boost::bind(
					&tuser_message::detail_employee
					, this
					, boost::ref(window)));
	}

	window.invalidate_layout();
}

void tuser_message::fill_score_board(twindow& window)
{
	std::vector<http::membership> members = http::list_board_score(disp_, heros_);
	std::stringstream strstr;
	utils::string_map symbols;
	tlistbox* list = find_widget<tlistbox>(&window, "score_table", false, true);
	list->clear();
	for (std::vector<http::membership>::const_iterator it = members.begin(); it != members.end(); ++ it) {
		const http::membership& m = *it;

		std::map<std::string, string_map> data;
		string_map item;

		strstr.str("");
		strstr << m.name;
		if (m.vip > 0) {
			strstr << help::tintegrate::generate_img("misc/vip.png~SCALE(32, 32)");
		}
		item["label"] = strstr.str();
		data.insert(std::make_pair("username", item));

		strstr.str("");
		if (m.noble >= 0 && m.noble <= unit_types.max_noble_level()) {
			strstr << unit_types.leader_noble(m.noble).name();
			symbols["level"] = lexical_cast_default<std::string>(unit_types.max_noble_level() - m.noble + 1);
			strstr << "(" << vgettext("wesnoth-lib", "noble^Lv$level", symbols) << ")";
		}
		item["label"] = strstr.str();
		data.insert(std::make_pair("noble", item));

		strstr.str("");
		std::vector<std::string> vstr = utils::split(m.member);
		strstr << vstr.size();
		item["label"] = strstr.str();
		data.insert(std::make_pair("hero", item));

		strstr.str("");
		strstr << m.score;
		item["label"] = strstr.str();
		data.insert(std::make_pair("score", item));

		strstr.str("");
		strstr << m.coin;
		item["label"] = strstr.str();
		data.insert(std::make_pair("coin", item));

		strstr.str("");
		strstr << help::tintegrate::generate_img("misc/browse.png");;
		item["label"] = strstr.str();
		data.insert(std::make_pair("browse", item));

		list->add_row(data);

		tgrid* grid_ptr = list->get_row_grid(list->get_item_count() - 1);
		ttoggle_panel* toggle = dynamic_cast<ttoggle_panel*>(grid_ptr->find("_toggle", true));
		toggle->set_data(m.uid);
		
		connect_signal_mouse_left_click(
				find_widget<tbutton>(grid_ptr, "browse", true)
				, boost::bind(
					&tuser_message::detail_group
					, this
					, boost::ref(window)));
	}

	window.invalidate_layout();
}

void tuser_message::swap_page(twindow& window, int page, bool swap)
{
	if (page < MIN_PAGE || page > MAX_PAGE) {
		return;
	}
	int index = page - MIN_PAGE;

	if (window.alternate_index() == index) {
		// desired page is the displaying page, do nothing.
		return;
	}
	window.alternate_uh(page_panel_, index);
	window.alternate_bh(swap? page_panel_: NULL, index);

	if (page == MESSAGE_PAGE) {
		fill_message(window);

	} else if (page == SIEGE_RECORD_PAGE) {
		fill_siege_record(window);

	} else if (page == EMPLOYEE_PAGE) {
		fill_employee(window);

	} else if (page == RANK_SCORE_PAGE) {
		fill_score_board(window);

	}

	current_page_ = page;
}

} // namespace gui2

