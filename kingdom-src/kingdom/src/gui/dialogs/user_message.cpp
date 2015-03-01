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
#include "gui/dialogs/group.hpp"
#include "gui/dialogs/hero.hpp"
#include "gui/dialogs/send_message.hpp"
#include "preferences_display.hpp"
#include "formula_string_utils.hpp"
#include "filesystem.hpp"

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

tuser_message::tuser_message(display& disp, hero_map& heros, const config& game_config)
	: tchat_(disp, group, MIN_PAGE, CHAT_PAGE, CHATING_PAGE)
	, disp_(disp)
	, heros_(heros)
	, game_config_(game_config)
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
	window.set_escape_disabled(true);

	sheet_.insert(std::make_pair((int)CHAT_PAGE, find_widget<ttoggle_button>(&window, "chat", false, true)));
	sheet_.insert(std::make_pair((int)MESSAGE_PAGE, find_widget<ttoggle_button>(&window, "message", false, true)));
	sheet_.insert(std::make_pair((int)SIEGE_RECORD_PAGE, find_widget<ttoggle_button>(&window, "siege_record", false, true)));
	for (std::map<int, ttoggle_button*>::iterator it = sheet_.begin(); it != sheet_.end(); ++ it) {
		it->second->set_callback_state_change(boost::bind(&tuser_message::sheet_toggled, this, _1));
		it->second->set_data(it->first);
	}

	std::stringstream strstr;
	if (group.message(tgroup::msg_message)) {
		strstr.str("");
		strstr << dgettext("wesnoth-lib", "Message");
		strstr << tintegrate::generate_img("misc/mini-new.png");
		find_widget<ttoggle_button>(&window, "message", false, true)->set_label(strstr.str());
	}
	if (group.message(tgroup::msg_siege)) {
		strstr.str("");
		strstr << dgettext("wesnoth-lib", "Siege record");
		strstr << tintegrate::generate_img("misc/mini-new.png");
		find_widget<ttoggle_button>(&window, "siege_record", false, true)->set_label(strstr.str());
	}

	page_panel_ = find_widget<tscrollbar_panel>(&window, "page", false, true);
	swap_page(window, CHAT_PAGE, false);
	sheet_.begin()->second->set_value(true);

	update_network_status(window, lobby.ready());
	join();
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
			symbols["day"] = tintegrate::generate_format(vstr[2], "green");
			parsed = vgettext("wesnoth-lib", "Inapp-purchase successfully! Increase $day days VIP membership.", symbols);

		} else if (subid > 0 && subid <= 4) {
			if (vstr.size() != 4) {
				return unknown_standard_message;
			}
			symbols["coin"] = tintegrate::generate_format(vstr[2], "green");
			symbols["score"] = tintegrate::generate_format(vstr[3], "green");
			parsed = vgettext("wesnoth-lib", "Inapp-purchase successfully! Increase $coin coin and $score score.", symbols);

		} else if (subid == 5) {
			if (vstr.size() != 4) {
				return unknown_standard_message;
			}
			symbols["coin"] = tintegrate::generate_format(vstr[2], "yellow");
			symbols["score"] = tintegrate::generate_format(vstr[3], "yellow");
			symbols["contact"] = tintegrate::generate_format(game_config::service_email, "green");
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
	gui2::tsend_message dlg(disp_, heros_, initial_str.str(), gui2::tsend_message::SEND_MESSAGE);
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
	message.sender = HTTP_INVALID_UID;
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
			strstr << tintegrate::generate_img("misc/up.png");
		} else {
			strstr << tintegrate::generate_img("misc/down.png");
		}
		if (message.create_time > group.browsed_time()) {
			strstr << tintegrate::generate_img("misc/mini-new.png", tintegrate::BACK);
		}
		item["label"] = strstr.str();
		data.insert(std::make_pair("flag", item));

		strstr.str("");
		if (!message.sender_username.empty()) {
			if (message.sender_username == username) {
				strstr << tintegrate::generate_format(message.sender_username, "yellow");
			} else if (message.sender_username == game_config::broadcast_username) {
				strstr << tintegrate::generate_img("misc/broadcast.png");
			} else {
				strstr << message.sender_username;
			}
		}
		item["label"] = strstr.str();
		data.insert(std::make_pair("sender", item));

		strstr.str("");
		if (!message.receiver_username.empty()) {
			if (message.receiver_username == username) {
				strstr << tintegrate::generate_format(message.receiver_username, "yellow");
			} else if (message.receiver_username == game_config::broadcast_username) {
				strstr << tintegrate::generate_img("misc/broadcast.png");
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

void tuser_message::keyboard_shown(twindow& window)
{
	tchat_::keyboard_shown(window);
	find_widget<ttoggle_button>(&window, "chat", false, true)->set_visible(twidget::INVISIBLE);
	find_widget<ttoggle_button>(&window, "message", false, true)->set_visible(twidget::INVISIBLE);
	find_widget<ttoggle_button>(&window, "siege_record", false, true)->set_visible(twidget::INVISIBLE);
}

void tuser_message::keyboard_hidden(twindow& window)
{
	tchat_::keyboard_hidden(window);
	find_widget<ttoggle_button>(&window, "chat", false, true)->set_visible(twidget::VISIBLE);
	find_widget<ttoggle_button>(&window, "message", false, true)->set_visible(twidget::VISIBLE);
	find_widget<ttoggle_button>(&window, "siege_record", false, true)->set_visible(twidget::VISIBLE);
}

bool tuser_message::handle(tlobby::ttype type, const config& data)
{
	if (type == tlobby::t_connected || type == tlobby::t_disconnected) {
		update_network_status(*page_panel_->get_window(), type == tlobby::t_connected);
		process_network_status(type == tlobby::t_connected);
	}

	if (type != tlobby::t_data) {
		return false;
	}

	bool halt = true;
	if (const config &c = data.child("message")) {
		process_message(c);

	} else if (const config &c = data.child("whisper")) {
		process_message(c, true);

	} else if(data.child("gamelist")) {
		process_userlist(data);

	} else if (const config &c = data.child("gamelist_diff")) {
		process_userlist_diff(c);

	} else {
		halt = false;
	}
	return halt;
}

void tuser_message::update_network_status(twindow& window, bool connected)
{
	std::stringstream strstr;
	strstr << dgettext("wesnoth-lib", "Chat");
	if (!connected) {
		strstr << tintegrate::generate_img("misc/network-disconnected.png");
	} else {
		strstr << tintegrate::generate_img("misc/network-connected.png");
	}
	find_widget<ttoggle_button>(&window, "chat", false, true)->set_label(strstr.str());
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
	strstr << tintegrate::generate_format(_("Send"), "blue");
	button->set_label(strstr.str());
	if (group.leader().name() != "ancientcc") {
		button->set_visible(twidget::INVISIBLE);
	}
}

std::string score_str(int score)
{
	std::stringstream strstr;
	if (score) {
		strstr << " " << tintegrate::generate_img("misc/score.png~SCALE(24, 24)");
		if (score > 0) {
			strstr << tintegrate::generate_format(score, "green");
		} else {
			strstr << tintegrate::generate_format(score, "red");
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
				strstr << tintegrate::generate_format(rec.attacker_username, "yellow");
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
				strstr << tintegrate::generate_format(rec.defender_username, "yellow");
			} else {
				strstr << rec.defender_username;
			}
		}
		item["label"] = strstr.str();
		data.insert(std::make_pair("defender", item));

		strstr.str("");
		if (!rec.atk_reinforce_username.empty()) {
			if (rec.atk_reinforce_username == username) {
				strstr << tintegrate::generate_format(rec.atk_reinforce_username, "yellow");
			} else {
				strstr << rec.atk_reinforce_username;
			}
		}
		item["label"] = strstr.str();
		data.insert(std::make_pair("atk_reinforce", item));

		strstr.str("");
		if (!rec.def_reinforce_username.empty()) {
			if (rec.def_reinforce_username == username) {
				strstr << tintegrate::generate_format(rec.def_reinforce_username, "yellow");
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
			strstr << ", " << tintegrate::generate_format(heros_[rec.employee].name(), "red");
		}
		item["label"] = strstr.str();
		data.insert(std::make_pair("result", item));

		list->add_row(data);
	}
	window.invalidate_layout();
}

void tuser_message::desire_swap_page(twindow& window, int page, bool open)
{
	if (open) {
		if (page == MESSAGE_PAGE) {
			fill_message(window);

		} else if (page == SIEGE_RECORD_PAGE) {
			fill_siege_record(window);

		}
	}
}

} // namespace gui2

