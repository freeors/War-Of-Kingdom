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

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "gui/dialogs/chat.hpp"

#include "display.hpp"
#include "gettext.hpp"
#include "gui/auxiliary/timer.hpp"
#include "gui/dialogs/helper.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/tree_view.hpp"
#include "gui/widgets/tree_view_node.hpp"
#include "gui/widgets/toggle_panel.hpp"
#include "gui/widgets/scroll_label.hpp"
#include "gui/widgets/scrollbar_panel.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/scroll_text_box.hpp"
#include "gui/widgets/report.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/spacer.hpp"
#include "gui/dialogs/transient_message.hpp"
#include "gui/dialogs/netdiag.hpp"
#include "gui/dialogs/message.hpp"
#include "formula_string_utils.hpp"
#include "wml_separators.hpp"
#include <hero.hpp>
#include "filesystem.hpp"
#include "proto_irc.hpp"
#include "game_config.hpp"

#include <boost/foreach.hpp>
#include <boost/bind.hpp>

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

extern std::vector<std::set<int> > generate_team_names_from_side(const config& cfg);
extern std::vector<int> generate_allies_from_team_names(const std::vector<std::set<int> >& team_names);

REGISTER_DIALOG(chat2)

#define ctrlid_chat_layer	"_layer_chat"
#define ctrlid_find_layer	"_layer_find"
#define ctrlid_channel_layer	"_layer_channel"
#define ctrlid_contact_bar	"_chat_contact_bar"
#define ctrlid_persons		"_chat_persons"
#define ctrlid_persons_layer	"_layer_chat_persons"
#define ctrlid_channels		"_chat_channels"
#define ctrlid_channels_layer	"_layer_chat_channels"
#define ctrlid_history		"_chat_history"
#define ctrlid_qq_icon		"_chat_qq_icon"
#define ctrlid_qq_name		"_chat_qq_name"
#define ctrlid_tool_bar		"_chat_tool_bar"
#define ctrlid_input		"_chat_input"
#define ctrlid_input_scale	"_chat_input_scale"
#define ctrlid_send			"_chat_send"
#define ctrlid_send_tip		"_chat_send_tip"
#define ctrlid_switch_to_chat_find	"_chat_switch_to_chat_find"
#define ctrlid_switch_to_chat_channel	"_chat_switch_to_chat_channel"
#define ctrlid_switch_to_find	"_chat_switch_to_find"
#define ctrlid_find			"_chat_find"
#define ctrlid_join			"_chat_join"
#define ctrlid_find_filter	"_chat_find_filter"
#define ctrlid_find_min_users	"_chat_find_min_users"
#define ctrlid_chanlist		"_chat_chanlist"

#define ctrlid_channel_label	"_chat_channel_label"
#define ctrlid_channel2		"_chat_channel2"
#define ctrlid_chat_to		"_chat_chat_to"
#define ctrlid_join_friend	"_chat_join_friend"

#define person_png			"misc/rose-36.png"
#define channel_png			"misc/channel.png"

#define at_person			0
#define at_channel			1

tchat_::tfunc::tfunc(int id)
	: id(id)
	, type(ft_none)
	, tooltip()
{
	if (id == f_netdiag) {
		type = ft_none | ft_person | ft_channel | ft_chan_person;
		image = "misc/network.png";
		tooltip = dgettext("wesnoth-lib", "Network");

	} else if (id == f_previous_page) {
		type = ft_person | ft_channel | ft_chan_person;
		image = "misc/up.png";
		tooltip = dgettext("wesnoth-lib", "Previous page");

	} else if (id == f_next_page) {
		type = ft_person | ft_channel | ft_chan_person;
		image = "misc/down.png";
		tooltip = dgettext("wesnoth-lib", "Next page");

	} else if (id == f_face) {
		type = 0;
		image = "misc/face.png";
		tooltip = dgettext("wesnoth-lib", "Face");

	} else if (id == f_part) {
		type = ft_channel;
		image = "misc/exit.png";
		tooltip = dgettext("wesnoth-lib", "Part channel");

	} else if (id == f_part_friend) {
		type = ft_person | ft_chan_person;
		image = "misc/exit.png";
		tooltip = dgettext("wesnoth-lib", "Part friend");

	} else if (id == f_explore) {
		type = ft_channel;
		image = "misc/explore.png";
		tooltip = dgettext("wesnoth-lib", "Explore");
	}
}

int tchat_::tsession::logs_per_page = 30;

tchat_::tsession::tsession(chat_logs::treceiver& receiver)
	: receiver(&receiver)
	, current_page(0)
{
	const chat_logs::thistory_log* choice = NULL;
	for (std::set<chat_logs::thistory_log>::const_iterator it = chat_logs::history_logs.begin(); it != chat_logs::history_logs.end(); ++ it) {
		const chat_logs::thistory_log& logs = *it;
		if (logs.nick == receiver.nick) {
			choice = &logs;
			break;
		}
	}
	if (!choice) {
		return;
	}
	chat_logs::user_from_logfile(*choice, history);
}

void tchat_::tsession::current_logs(std::vector<chat_logs::tlog>& logs) const
{
	logs.clear();
	int history_size = history.size();
	int size = history_size + receiver->logs.size();
	if (!size) {
		return;
	}

	int page = pages() - current_page - 1;
	// both start and end are index. end is last valid index.
	int start = page * logs_per_page;
	int end = start + logs_per_page - 1;
	int remainder = size % logs_per_page;
	if (remainder) {
		if (page) {
			start -= logs_per_page - remainder;
			end -= logs_per_page - remainder;
		} else {
			end -= logs_per_page - remainder;
		}
	}

	int history_start = -1, history_end = -1, now_start = -1, now_end = -1;
	if (history_size > start) {
		history_start = start;
		if (history_size > end) {
			history_end = end;
		} else {
			history_end = history_size - 1;
			now_start = 0;
			now_end = (end - start) - (history_end - history_start + 1);
		}
	} else {
		now_start = start - history_size;
		now_end = end - history_size;
	}

	std::vector<chat_logs::tlog>::const_iterator begin_it;
	std::vector<chat_logs::tlog>::const_iterator end_it;
	if (history_start != -1) {
		begin_it = history.begin();
		std::advance(begin_it, history_start);
		end_it = history.begin();
		std::advance(end_it, history_end + 1);
		std::copy(begin_it, end_it, std::back_inserter(logs));
	}
	if (now_start != -1) {
		begin_it = receiver->logs.begin();
		std::advance(begin_it, now_start);
		end_it = receiver->logs.begin();
		std::advance(end_it, now_end + 1);
		std::copy(begin_it, end_it, std::back_inserter(logs));
	}
}

int tchat_::tsession::pages() const
{
	return ceil(1.0 * (history.size() + receiver->logs.size()) / logs_per_page);
}

bool tchat_::tsession::can_previous() const 
{
	int pgs = pages();
	return pgs && current_page < pgs - 1; 
}

bool tchat_::tsession::can_next() const 
{ 
	int pgs = pages();
	return pgs && current_page > 0; 
}

std::string tchat_::err_encode_str;

tchat_::tchat_(display& disp, tgroup& g, int min_page, int chat_page, int chating_page)
	: disp_(disp)
	, group_(g)
	, signature_(0)
	, person_cookies_()
	, channel_cookies_()
	, gamelist_()
	, window_(NULL)
	, history_(NULL)
	, input_(NULL)
	, input_tb_(NULL)
	, send_(NULL)
	, previous_page_(NULL)
	, next_page_(NULL)
	, src_pos_(0)
	, current_session_(NULL)
	, inputing_(false)
	, min_page_(min_page)
	, current_page_(min_page - 1)
	, chat_page_(chat_page)
	, page_panel_(NULL)
	, current_ft_(ft_none)
	, in_find_chan_(false)
	, catalog_(new ttabbar(true, false, "icon40"))
	, toolbar_(new ttabbar(false, false, "icon36"))
{
	if (err_encode_str.empty()) {
		err_encode_str = dgettext("wesnoth-lib", "Character encoding must be UTF-8!");
	}
}

tchat_::~tchat_()
{
	delete catalog_;
	delete toolbar_;
}

void tchat_::user_to_title(const tcookie& cookie) const
{
	tcontrol& icon = find_widget<tcontrol>(window_, ctrlid_qq_icon, false);
	if (cookie.channel) {
		icon.set_label(channel_png);
	} else {
		icon.set_label(person_png);
	}

	tlabel& label = find_widget<tlabel>(window_, ctrlid_qq_name, false);
	label.set_label(cookie.nick);
}

void tchat_::contact_person_toggled(twidget* widget)
{
	ttoggle_panel* toggle = dynamic_cast<ttoggle_panel*>(widget);
	std::pair<std::vector<tcookie>*, tcookie* > pair = contact_find(true, toggle->get_data());

	tcookie& cookie = *pair.second;

	if (cookie.channel) {
		return;
	}

	switch_session(true, *pair.first, cookie);
}

void tchat_::contact_channel_toggled(twidget* widget)
{
	ttoggle_panel* toggle = dynamic_cast<ttoggle_panel*>(widget);
	std::pair<std::vector<tcookie>*, tcookie* > pair = contact_find(false, toggle->get_data());

	tcookie& cookie = *pair.second;
	switch_session(false, *pair.first, cookie);
}

void tchat_::refresh_channel2_toolbox(const tlobby_user& user)
{
	bool favor = lobby->chat.is_favor_user(user.uid);

	// now only can chato favor's user
	chat_to_->set_active(!user.me && favor);
	join_friend_->set_active(!user.me && !favor);
}

void tchat_::channel2_toggled(twidget* widget)
{
	ttoggle_panel* toggle = dynamic_cast<ttoggle_panel*>(widget);
	tcookie& cookie = channel2_branch_[reinterpret_cast<long>(toggle->cookie())];

	tlobby_user& user = lobby->chat.get_user(cookie.id);
	refresh_channel2_toolbox(user);
}

void tchat_::simulate_cookie_toggled(bool person, int cid, int id, bool channel)
{
	std::pair<std::vector<tcookie>*, tcookie* > pair = contact_find(person, cid, id, channel);
	ttree_view* tree = person? person_tree_: channel_tree_;

	tcookie& cookie = *pair.second;
	tree->set_select_item(cookie.node);

	ttoggle_panel& toggle = find_widget<ttoggle_panel>(tree->selected_item(), "tree_view_node_label", false);
	if (person) {
		contact_person_toggled(&toggle);
	} else {
		contact_channel_toggled(&toggle);
	}
}

void tchat_::netdiag(twindow& window)
{
	gui2::tnetdiag dlg;
	dlg.show(disp_.video());
}

void send_whisper(const tgroup& g, const std::string& receiver, const std::string& message)
{
	config cwhisper, data;
	cwhisper["receiver"] = receiver;
	cwhisper["message"] = message;
	cwhisper["sender"] = g.leader().name();
	data.add_child("whisper", cwhisper);
	network::send_data(lobby->chat, data);
}

bool is_blank_str(const std::string& str)
{
	const char* blank_char = " \n";
	size_t pos = str.find_first_not_of(blank_char);
	return pos == std::string::npos;
}

void tchat_::handle_multiline(const char* nick, const char* chan, const char* text)
{
	irc::server* serv = lobby->chat.serv();
	char* tmp = (char*)text;

	int i = 0, pos = 0, len = (int)strlen(text);
	while (i < len) {
		if (text[i] == '\n') {
			tmp[pos] = 0;
			if (pos) {
				irc::send_msg(serv, nick, chan, tmp);
				// serv->p_message(serv, chan, tmp);
			}
			// recover to orignal charactor
			tmp[pos] = '\n';

			tmp = (char*)(text + i + 1);
			pos = 0;
		} else {
			pos ++;
		}
		i ++;
	}
	if (pos) {
		irc::send_msg(serv, nick, chan, tmp);
		// serv->p_message(serv, chan, tmp);
	}
}

void tchat_::send(bool& handled, bool& halt)
{
	twindow& window = *window_;

	if (!lobby->chat.ready()) {
		return;
	}

	if (!current_session_) {
		return;
	}
	std::string input_str = input_->label();
	if (is_blank_str(input_str)) {
		return;
	}

	std::string orignal_input_str = input_str;

	handle_multiline(lobby->chat.nick().c_str(), current_session_->receiver->nick.c_str(), input_str.c_str());

	input_->set_label(null_str);

	chat_logs::add(current_session_->receiver->id, current_session_->receiver->channel, *lobby->chat.me, input_str);
	chat_2_scroll_label(*history_, *current_session_);
}

void tchat_::find(twindow& window)
{
	if (!in_find_chan_) {
		irc::server* serv = lobby->chat.serv();

		ttext_box* text_box = &find_widget<ttext_box>(window_, ctrlid_find_filter, false);
		std::string filter = text_box->get_value();

		text_box = &find_widget<ttext_box>(window_, ctrlid_find_min_users, false);
		cond_min_users_ = utils::to_int(text_box->get_value());
		if (cond_min_users_ < 1) {
			cond_min_users_ = 1;
		}

		std::string nick;
		if (!filter.empty()) {
			nick = std::string("#") + filter;
		}
		lobby->chat.find_channel(nick, cond_min_users_);

	} else {
		process_chanlist_end();
	}
}

void tchat_::join_channel(twindow& window)
{
	const std::string& chan = list_chans_[chanlist_->get_selected_row()];
	int cid = tlobby_channel::get_cid(chan);
	tlobby_channel& channel = lobby->chat.insert_channel(cid, chan);
	std::vector<tcookie>& branch = ready_branch(false, channel);

	lobby->chat.join_channel(channel, true);
	switch_to_chat(window);
}

void tchat_::chat_to(twindow& window)
{
	ttoggle_panel& toggle = find_widget<ttoggle_panel>(channel2_tree_->selected_item(), "tree_view_node_label", false);
	const tcookie& cookie = channel2_branch_[reinterpret_cast<long>(toggle.cookie())];
	simulate_cookie_toggled(true, tlobby_channel::t_friend, cookie.id, false);

	switch_to_chat(window);
}

void tchat_::join_friend(twindow& window)
{
	tlobby_channel& channel = lobby->chat.get_channel(tlobby_channel::t_friend);

	ttoggle_panel& toggle = find_widget<ttoggle_panel>(channel2_tree_->selected_item(), "tree_view_node_label", false);
	const tcookie& cookie = channel2_branch_[reinterpret_cast<long>(toggle.cookie())];
	tlobby_user& user = channel.insert_user(cookie.id, cookie.nick);

	std::vector<tcookie>& branch = person_cookies_.find(tlobby_channel::t_friend)->second;
	insert_user(true, branch, user);

	irc::server* serv = lobby->chat.serv();
	serv->p_monitor(serv, cookie.nick.c_str(), true);

	refresh_channel2_toolbox(user);

	std::stringstream ss;
	utils::string_map symbols;

	symbols["nick"] = tintegrate::generate_format(user.nick, "yellow");
	symbols["channel"] = tintegrate::generate_format(channel.name(), "green");
	ss << vgettext("wesnoth-lib", "You set $nick into $channel successfully!", symbols);
	gui2::show_message(disp_.video(), "", ss.str());
}

void tchat_::insert_face(twindow& window, int index)
{
	std::stringstream ss;
	ss << "face/default/" << index << ".png";
	ss.str();
	tscroll_text_box& input = find_widget<tscroll_text_box>(window_, ctrlid_input, false);
	input.insert_img(ss.str());
}

void tchat_::enter_inputing(bool enter)
{
	if ((enter && inputing_) || (!enter && !inputing_)) {
		return;
	}

	int height;
	if (enter) {
		if (settings::screen_height < 600) {
			height = settings::screen_height * 30 / 100;
		} else {
			height = 180;
		}
	} else {
		if (settings::screen_height < 600) {
			height = settings::screen_height * 10 / 100;
		} else {
			height = 60;
		}
	}
	inputing_ = enter;

	input_scale_->set_best_size(tpoint(0, height));
	window_->invalidate_layout();
}

void tchat_::signal_handler_sdl_key_down(bool& handled
		, bool& halt
		, const SDLKey key
		, SDLMod modifier
		, const Uint16 unicode)
{
#if (defined(__APPLE__) && TARGET_OS_IPHONE) || defined(ANDROID)
	if (key == SDLK_PRINTSCREEN) {
		SDL_Rect rc;
		SDL_SetTextInputRect(&rc);
		settings::keyboard_height = rc.h;
        window_->invalidate_layout();

		if (settings::screen_height == 320) {
			history_->set_visible(twidget::INVISIBLE);
			chanlist_->set_visible(twidget::INVISIBLE);
		}
	}
#endif

#ifdef _WIN32
	if ((key == SDLK_RETURN || key == SDLK_KP_ENTER) && !(modifier & KMOD_SHIFT)) {
		send(handled, halt);

		handled = true;
		halt = true;
	}
#else
	if (key == SDLK_RETURN) {
		settings::keyboard_height = 0;
		window_->invalidate_layout();

		if (settings::screen_height == 320) {
			history_->set_visible(twidget::VISIBLE);
			chanlist_->set_visible(twidget::VISIBLE);
		}
		handled = true;
		halt = true;
	}
#endif

	if (key == SDLK_ESCAPE) {
		enter_inputing(false);

	} else if (key >= SDLK_SPACE && key < SDL_SCANCODE_TO_KEYCODE(1)) {
		enter_inputing(true);
	}
}

void tchat_::generate_channel_tree(tlobby_channel& channel)
{
	if (channel.users.empty()) {
		return;
	}

	ttree_view_node& root_node = channel2_tree_->get_root_node();
	std::stringstream ss;

	ss.str("");
	ss << tintegrate::generate_img(channel_png);
	ss << channel.nick;
	ss << " (" << tintegrate::generate_format(channel.users.size(), "yellow") << ")";
	tlabel& label = find_widget<tlabel>(window_, ctrlid_channel_label, false);
	label.set_label(ss.str());

	for (std::vector<tlobby_user*>::const_iterator it = channel.users.begin(); it != channel.users.end(); ++ it) {
		tlobby_user& user = **it;
		
		string_map tree_group_field;
		std::map<std::string, string_map> tree_group_item;

		tree_group_field["label"] = user.nick;
		tree_group_item["name"] = tree_group_field;
		channel2_branch_.push_back(tcookie(signature_, &root_node.add_child("item", tree_group_item), user.nick, user.uid, false));
		tcookie& cookie = channel2_branch_.back();
		cookie.online = true;

		ttoggle_panel& toggle =find_widget<ttoggle_panel>(cookie.node, "tree_view_node_label", false);
		toggle.set_cookie(reinterpret_cast<void*>(channel2_branch_.size() - 1));
		toggle.set_callback_state_change(boost::bind(&tchat_::channel2_toggled, this, _1));

		ss.str("");
		ss << tintegrate::generate_img(person_png);
		ss << cookie.nick;

		tlabel* label = dynamic_cast<tlabel*>(cookie.node->find("name", true));
		label->set_label(ss.str());
	}

	ttoggle_panel& toggle = find_widget<ttoggle_panel>(channel2_tree_->selected_item(), "tree_view_node_label", false);
	channel2_toggled(&toggle);
}

std::vector<tchat_::tcookie>& tchat_::ready_branch(bool person, const tlobby_channel& channel)
{
	ttree_view* tree = person? person_tree_: channel_tree_;

	string_map tree_group_field;
	std::map<std::string, string_map> tree_group_item;
	tcookie cookie;

	std::string nick;
	std::stringstream label;
	if (person) {
		nick = channel.name();

	} else {
		nick = tlobby_channel::get_nick(channel.cid);
		if (channel.err) {
			label << tintegrate::generate_format("+R", "red");
		}
	}
	label << nick << "\n" << "--/--";

	if (person) {
		tree_group_field["label"] = null_str;
	} else {
		tree_group_field["label"] = std::string(channel_png) + "~GS()";
	}
	tree_group_item["icon"] = tree_group_field;
	tree_group_field["label"] = label.str();
	tree_group_item["name"] = tree_group_field;
	cookie = tcookie(signature_, &tree->get_root_node().add_child("item", tree_group_item), nick, channel.cid, true);

	ttoggle_panel& toggle =find_widget<ttoggle_panel>(cookie.node, "tree_view_node_label", false);
	toggle.set_data(signature_ ++);
	if (!person) {
		toggle.set_callback_state_change(boost::bind(&tchat_::contact_channel_toggled, this, _1));
	}

	std::map<int, std::vector<tcookie> >& cookies = person? person_cookies_: channel_cookies_;
	std::pair<std::map<int, std::vector<tcookie> >::iterator, bool> ins = 
		cookies.insert(std::make_pair(channel.cid, std::vector<tcookie>(1, cookie)));
	return ins.first->second;
}

std::pair<std::vector<tchat_::tcookie>*, tchat_::tcookie* > tchat_::contact_find(bool person, size_t signature)
{
	std::map<int, std::vector<tcookie> >& cookies = person? person_cookies_: channel_cookies_;

	std::pair<std::vector<tcookie>*, tcookie* > ret = std::make_pair(reinterpret_cast<std::vector<tcookie>*>(NULL), reinterpret_cast<tcookie*>(NULL));
	for (std::map<int, std::vector<tcookie> >::iterator it = cookies.begin(); it != cookies.end() && !ret.first; ++ it) {
		for (std::vector<tcookie>::iterator it2 = it->second.begin(); it2 != it->second.end() && !ret.first; ++ it2) {
			if (it2->signature == signature) {
				ret.first = &it->second;
				ret.second = &*it2;
			}
		}
	}
	return ret;
}

std::pair<std::vector<tchat_::tcookie>*, tchat_::tcookie* > tchat_::contact_find(bool person, int cid, int id, bool channel)
{
	std::map<int, std::vector<tcookie> >& cookies = person? person_cookies_: channel_cookies_;

	std::pair<std::vector<tcookie>*, tcookie* > ret = std::make_pair(reinterpret_cast<std::vector<tcookie>*>(NULL), reinterpret_cast<tcookie*>(NULL));
	if (cid != tlobby_channel::npos) {
		std::map<int, std::vector<tcookie> >::iterator it = cookies.find(cid);
		if (it == cookies.end()) {
			return ret;
		}
		for (std::vector<tcookie>::iterator it2 = it->second.begin(); it2 != it->second.end() && !ret.first; ++ it2) {
			if (it2->id == id && it2->channel == channel) {
				ret.first = &it->second;
				ret.second = &*it2;
			}
		}
	} else {
		for (std::map<int, std::vector<tcookie> >::iterator it = cookies.begin(); it != cookies.end() && !ret.first; ++ it) {
			for (std::vector<tcookie>::iterator it2 = it->second.begin(); it2 != it->second.end() && !ret.first; ++ it2) {
				if (it2->id == id && it2->channel == channel) {
					ret.first = &it->second;
					ret.second = &*it2;
				}
			}
		}
	}
	return ret;
}

std::pair<const std::vector<tchat_::tcookie>*, const tchat_::tcookie* > tchat_::contact_find(bool person, int cid, int id, bool channel) const
{
	const std::map<int, std::vector<tcookie> >& cookies = person? person_cookies_: channel_cookies_;

	std::pair<const std::vector<tcookie>*, const tcookie* > ret = std::make_pair(reinterpret_cast<std::vector<tcookie>*>(NULL), reinterpret_cast<tcookie*>(NULL));
	if (cid != tlobby_channel::npos) {
		std::map<int, std::vector<tcookie> >::const_iterator it = cookies.find(cid);
		if (it == cookies.end()) {
			return ret;
		}
		for (std::vector<tcookie>::const_iterator it2 = it->second.begin(); it2 != it->second.end() && !ret.first; ++ it2) {
			if (it2->id == id && it2->channel == channel) {
				ret.first = &it->second;
				ret.second = &*it2;
			}
		}
	} else {
		for (std::map<int, std::vector<tcookie> >::const_iterator it = cookies.begin(); it != cookies.end() && !ret.first; ++ it) {
			for (std::vector<tcookie>::const_iterator it2 = it->second.begin(); it2 != it->second.end() && !ret.first; ++ it2) {
				if (it2->id == id && it2->channel == channel) {
					ret.first = &it->second;
					ret.second = &*it2;
				}
			}
		}
	}
	return ret;
}

void tchat_::update_node_internal(const std::vector<tcookie>& cookies, const tcookie& cookie)
{
	std::stringstream icon_ss;
	std::stringstream ss;

	if (cookie.channel) {
		const tlobby_channel& channel = lobby->chat.get_channel(cookie.id);

		if (tlobby_channel::is_allocatable(channel.cid)) {
			icon_ss << channel_png;
			if (channel.users.empty()) {
				icon_ss << "~GS()";
			}
		}

		if (channel.err) {
			ss << tintegrate::generate_format("+R", "red");
		}
		ss << cookie.nick << "\n";
		
		if (channel.users.empty()) {
			ss << "--/--";

		} else if (tlobby_channel::is_allocatable(channel.cid)) {
			ss << tintegrate::generate_format(channel.users.size(), "yellow");

		} else {
			size_t talkable = 0;
			for (std::vector<tcookie>::const_iterator it = cookies.begin(); it != cookies.end(); ++ it) {
				const tcookie& c2 = *it;
				if (c2.online && !c2.away) {
					talkable ++;
				}
			}

			ss << talkable << "/ " << (cookies.size() - 1);
		}
		if (cookie.unread) {
			ss << " (" << tintegrate::generate_format(cookie.unread, "green") << ")";
		}

	} else {
		icon_ss << person_png;
		if (!cookie.online) {
			icon_ss << "~GS()";
		}

		ss << cookie.nick << "\n";
		if (cookie.unread) {
			ss << " (" << tintegrate::generate_format(cookie.unread, "green") << ")";
		}

		update_node_internal(cookies, cookies.front());
	}

	tcontrol* icon = dynamic_cast<tcontrol*>(cookie.node->find("icon", true));
	icon->set_label(icon_ss.str());

	tlabel* label = dynamic_cast<tlabel*>(cookie.node->find("name", true));
	label->set_label(ss.str());
}

static const char* face_items[] = {
	"face0",
	"face1",
	"face2",
	"face3",
	"face4",
	"face5",
	"face6",
	"face7",
	"face8",
	"face9",
	"face10",
	"face11"
};
static int nb_items = sizeof(face_items) / sizeof(face_items[0]);

void tchat_::ready_face(twindow& window)
{
	tbutton* b;
	std::stringstream ss;
	for (int item = 0; item < nb_items; item ++) {
		b = find_widget<tbutton>(&window, face_items[item], false, false);
		ss.str("");
		ss << "face/default/" << item << ".png";
		b->set_label(ss.str());
	
		connect_signal_mouse_left_click(
			  *b
			, boost::bind(
				  &tchat_::insert_face
				, this
				, boost::ref(window)
				, item));
	}
}

void tchat_::reload_toolbar(twindow& window)
{
	funcs_.clear();
	for (int n = f_min; n <= f_max; n ++) {
		funcs_.push_back(n);
		tfunc& func = funcs_.back();

		tcontrol* widget = toolbar_->create_child(null_str, func.tooltip, reinterpret_cast<void*>(n), null_str);
		widget->set_label(func.image);
		toolbar_->insert_child(*widget);

		if (n == f_previous_page) {
			previous_page_ = dynamic_cast<tbutton*>(widget);
		} else if (n == f_next_page) {
			next_page_ = dynamic_cast<tbutton*>(widget);
		}
	}
	toolbar_->replacement_children();
}

void tchat_::refresh_toolbar(int type, int id)
{
	int n = 0;
	for (std::vector<tfunc>::const_iterator it = funcs_.begin(); it != funcs_.end(); ++ it, n ++) {
		const tfunc& func = *it;
		bool active = true;
		toolbar_->set_visible(n, func.type & type);
		if (func.id == f_previous_page) {
			active = current_session_ && current_session_->can_previous();

		} else if (func.id == f_next_page) {
			active = current_session_ && current_session_->can_next();

		} else if (func.id == f_face) {
			active = false;

		} else {
			active = true;
		}
		toolbar_->get_widget(n)->set_active(active);
	}
	toolbar_->replacement_children();

	tcontrol* widget;
	if (type == ft_channel) {
		tlobby_channel& channel = lobby->chat.get_channel(id);
		widget = toolbar_->get_widget(toolbar_->get_index2(reinterpret_cast<void*>(f_part)));
		if ((!channel.err && channel.users.empty()) || channel.nick == game_config::app_channel) {
			widget->set_active(false);
		}
		if (channel.users.empty()) {
			widget = toolbar_->get_widget(toolbar_->get_index2(reinterpret_cast<void*>(f_explore)));
			widget->set_active(false);
		}

	} else if (type == ft_person || type == ft_chan_person) {
		bool favor = lobby->chat.is_favor_user(id);
		widget = toolbar_->get_widget(toolbar_->get_index2(reinterpret_cast<void*>(f_part_friend)));
		widget->set_active(favor && type == ft_person);
	}

	current_ft_ = type;
}

void tchat_::clear_branch(bool person, int type)
{
	std::map<int, std::vector<tcookie> >& cookies = person? person_cookies_: channel_cookies_;
	ttree_view* tree = person? person_tree_: channel_tree_;

	std::vector<tcookie>& branch = cookies.find(type)->second;
	std::vector<tcookie>::iterator it = branch.begin();
	for (++ it; it != branch.end();) {
		ttree_view_node* node = it->node;
		tree->remove_node(node);
		it = branch.erase(it);
	}
}

void tchat_::remove_branch(bool person, int type)
{
	std::map<int, std::vector<tcookie> >& cookies = person? person_cookies_: channel_cookies_;
	ttree_view* tree = person? person_tree_: channel_tree_;

	std::map<int, std::vector<tcookie> >::iterator it = cookies.find(type);
	std::vector<tcookie>& branch = it->second;

	tree->remove_node(branch.front().node);
	cookies.erase(it);
}

void tchat_::insert_user(bool person, std::vector<tcookie>& branch, const tlobby_user& user)
{
	if (person) {
		string_map tree_group_field;
		std::map<std::string, string_map> tree_group_item;

		tree_group_field["label"] = user.nick;
		tree_group_item["name"] = tree_group_field;
		ttree_view_node& branch_node = *branch.front().node;
		branch.push_back(tcookie(signature_, &branch_node.add_child("item", tree_group_item), user.nick, user.uid, false));
		tcookie& cookie = branch.back();
		cookie.online = user.online;
		cookie.away = user.away;
		cookie.unread = user.unread;

		ttoggle_panel& toggle =find_widget<ttoggle_panel>(cookie.node, "tree_view_node_label", false);
		toggle.set_data(signature_ ++);
		if (person) {
			toggle.set_callback_state_change(boost::bind(&tchat_::contact_person_toggled, this, _1));
		} else if (user.nick != lobby->chat.nick()) {
			toggle.set_callback_state_change(boost::bind(&tchat_::contact_channel_toggled, this, _1));
		}

		update_node_internal(branch, cookie);

	} else {
		update_node_internal(branch, branch.front());
	}
}

void tchat_::erase_user(bool person, std::vector<tcookie>& branch, int uid)
{
	ttree_view* tree = person? person_tree_: channel_tree_;

	for (std::vector<tcookie>::iterator it = branch.begin(); it != branch.end(); ++ it) {
		const tcookie& that = *it;
		if (!that.channel && that.id == uid) {
			ttree_view_node* node = it->node;
			tree->remove_node(node);
			branch.erase(it);

			update_node_internal(branch, branch.front());
			return;
		}
	}
}

void tchat_::pre_show(twindow& window)
{
	window_ = &window;

	person_tree_ = find_widget<ttree_view>(&window, ctrlid_persons, false, true);
	channel_tree_ = find_widget<ttree_view>(&window, ctrlid_channels, false, true);
	channel2_tree_ = find_widget<ttree_view>(&window, ctrlid_channel2, false, true);
	history_ = &find_widget<tscroll_label>(window_, ctrlid_history, false);
	history_->set_scroll_to_end(true);
	input_ = &find_widget<tscroll_text_box>(window_, ctrlid_input, false);
	input_scale_ = &find_widget<tspacer>(window_, ctrlid_input_scale, false);
	send_ = &find_widget<tbutton>(window_, ctrlid_send, false);
	switch_to_chat_find_ = &find_widget<tbutton>(window_, ctrlid_switch_to_chat_find, false);
	switch_to_chat_channel_ = &find_widget<tbutton>(window_, ctrlid_switch_to_chat_channel, false);
	find_ = &find_widget<tbutton>(window_, ctrlid_find, false);
	join_channel_ = &find_widget<tbutton>(&window, ctrlid_join, false);
	chanlist_ = &find_widget<tlistbox>(&window, ctrlid_chanlist, false);
	chat_to_ = &find_widget<tbutton>(&window, ctrlid_chat_to, false);
	join_friend_ = &find_widget<tbutton>(&window, ctrlid_join_friend, false);

	chanlist_->set_callback_value_change(dialog_callback<tchat_, &tchat_::find_chan_toggled>);

	std::stringstream ss;
	
	tscroll_text_box& input = find_widget<tscroll_text_box>(window_, ctrlid_input, false);
	ttext_box& tb = find_widget<ttext_box>(input.content_grid(), "_text_box", false);
	input_tb_ = &tb;
	connect_signal_pre_key_press(tb, boost::bind(&tchat_::signal_handler_sdl_key_down, this, _3, _4, _5, _6, _7));
	window.keyboard_capture(&tb);

	tb.connect_signal<event::SDL_TEXT_INPUT>(
		boost::bind(
			&tchat_::enter_inputing
			, this
			, (int)true)
			, event::tdispatcher::front_child);

	tb.connect_signal<event::LEFT_BUTTON_DOWN>(
		boost::bind(
			&tchat_::enter_inputing
			, this
			, (int)true)
			, event::tdispatcher::front_child);

	history_->connect_signal<event::SDL_WHEEL_DOWN>(
		boost::bind(
			&tchat_::enter_inputing
			, this
			, (int)false)
			, event::tdispatcher::front_post_child);

	history_->connect_signal<event::SDL_WHEEL_UP>(
		boost::bind(
			&tchat_::enter_inputing
			, this
			, (int)false)
			, event::tdispatcher::front_post_child);

	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, ctrlid_switch_to_find, false)
			, boost::bind(
				  &tchat_::switch_to_find
				, this
				, boost::ref(window)));
	
	// find grid
	connect_signal_mouse_left_click(
			  *switch_to_chat_find_
			, boost::bind(
				  &tchat_::switch_to_chat
				, this
				, boost::ref(window)));

	connect_signal_mouse_left_click(
			  *find_
			, boost::bind(
				  &tchat_::find
				, this
				, boost::ref(window)));

	connect_signal_mouse_left_click(
			  *join_channel_
			, boost::bind(
				  &tchat_::join_channel
				, this
				, boost::ref(window)));
	join_channel_->set_active(false);

	// channel grid
	connect_signal_mouse_left_click(
			  *switch_to_chat_channel_
			, boost::bind(
				  &tchat_::switch_to_chat
				, this
				, boost::ref(window)));

	connect_signal_mouse_left_click(
			  *chat_to_
			, boost::bind(
				  &tchat_::chat_to
				, this
				, boost::ref(window)));

	connect_signal_mouse_left_click(
			  *join_friend_
			, boost::bind(
				  &tchat_::join_friend
				, this
				, boost::ref(window)));

	connect_signal_mouse_left_click(
			  *send_
			, boost::bind(
				  &tchat_::send
				, this
				, _3, _4));
	send_->set_active(false);

	person_tree_->set_no_indentation(true);

	for (std::map<int, tlobby_channel>::const_iterator it = lobby->chat.channels.begin(); it != lobby->chat.channels.end(); ++ it) {
		bool person = !tlobby_channel::is_allocatable(it->first);
		std::vector<tcookie>& branch = ready_branch(person, it->second);

		tlobby_channel& channel = lobby->chat.get_channel(it->first);
		for (std::vector<tlobby_user*>::const_iterator it = channel.users.begin(); it != channel.users.end(); ++ it) {
			const tlobby_user& user = **it;
			if (person && user.me) {
				continue;
			}
			insert_user(person, branch, user);
		}
	}

	input_tb_->set_src_pos(src_pos_);

	toolbar_->set_report(find_widget<treport>(&window, ctrlid_tool_bar, false, true));
	reload_toolbar(window);

	catalog_->set_report(find_widget<treport>(&window, ctrlid_contact_bar, false, true));
	reload_catalog(window);
	refresh_toolbar(ft_none, tlobby_user::npos);

	switch_to_chat(window);
	contact_switch_to_person();
}

void tchat_::contact_switch_to_person()
{
	find_widget<tgrid>(window_, ctrlid_persons_layer, false).set_visible(twidget::VISIBLE);
	find_widget<tgrid>(window_, ctrlid_channels_layer, false).set_visible(twidget::INVISIBLE);

	if (person_tree_->selected_item()) {
		ttoggle_panel& toggle = find_widget<ttoggle_panel>(person_tree_->selected_item(), "tree_view_node_label", false);
		contact_person_toggled(&toggle);
	}
}

void tchat_::contact_switch_to_channel()
{
	find_widget<tgrid>(window_, ctrlid_persons_layer, false).set_visible(twidget::INVISIBLE);
	find_widget<tgrid>(window_, ctrlid_channels_layer, false).set_visible(twidget::VISIBLE);

	if (channel_tree_->selected_item()) {
		ttoggle_panel& toggle = find_widget<ttoggle_panel>(channel_tree_->selected_item(), "tree_view_node_label", false);
		contact_channel_toggled(&toggle);
	}
}

void tchat_::switch_to_chat(twindow& window)
{
	if (!channel2_branch_.empty()) {
		ttree_view_node& root_node = channel2_tree_->get_root_node();
		root_node.clear();
		channel2_branch_.clear();
	}

	find_widget<tgrid>(&window, ctrlid_chat_layer, false).set_visible(twidget::VISIBLE);
	find_widget<tgrid>(&window, ctrlid_find_layer, false).set_visible(twidget::INVISIBLE);
	find_widget<tgrid>(&window, ctrlid_channel_layer, false).set_visible(twidget::INVISIBLE);
}

void tchat_::switch_to_find(twindow& window)
{
	find_widget<tgrid>(window_, ctrlid_chat_layer, false).set_visible(twidget::INVISIBLE);
	find_widget<tgrid>(window_, ctrlid_find_layer, false).set_visible(twidget::VISIBLE);
	find_widget<tgrid>(&window, ctrlid_channel_layer, false).set_visible(twidget::INVISIBLE);

	find_chan_toggled(window);
	find_->set_active(lobby->chat.ready());
}

void tchat_::switch_to_channel(twindow& window)
{
	find_widget<tgrid>(&window, ctrlid_chat_layer, false).set_visible(twidget::INVISIBLE);
	find_widget<tgrid>(&window, ctrlid_find_layer, false).set_visible(twidget::INVISIBLE);
	find_widget<tgrid>(&window, ctrlid_channel_layer, false).set_visible(twidget::VISIBLE);
}

tchat_::tsession& tchat_::get_session(chat_logs::treceiver& receiver, bool allow_create)
{
	for (std::vector<tsession>::iterator it = sessions_.begin(); it != sessions_.end(); ++ it) {
		tsession& session = *it;
		if (session.receiver->id == receiver.id && session.receiver->channel == receiver.channel) {
			return session;
		}
	}

	VALIDATE(allow_create, "Must exist receiver!");
	sessions_.push_back(tsession(receiver));
	return sessions_.back();
}

void tchat_::switch_session(bool person, std::vector<tcookie>& branch, tcookie& cookie)
{
	// current support one session!
	sessions_.clear();
	current_session_ = NULL;

	user_to_title(cookie);

	if (cookie.unread) {
		cookie.unread = 0;
		update_node_internal(branch, cookie);
	}

	int id = cookie.id;
	bool channel = cookie.channel;

	chat_logs::treceiver& receiver = chat_logs::find_receiver(id, channel, true);
	tsession& session = get_session(receiver, true);
	current_session_ = &session;

	bool can_send = lobby->chat.ready();
	if (can_send) {
		if (!receiver.channel) {
			tlobby_user& user = lobby->chat.get_user(id);
			can_send = user.online;
		} else if (tlobby_channel::is_allocatable(id)) {
			tlobby_channel& channel = lobby->chat.get_channel(id);
			can_send = channel.joined();
		} else {
			can_send = false;
		}
	}
	send_->set_active(can_send);

	chat_2_scroll_label(*history_, *current_session_);
	if (person) {
		refresh_toolbar(ft_person, current_session_->receiver->id);
	} else {
		if (current_session_->receiver->channel) {
			refresh_toolbar(ft_channel, current_session_->receiver->id);
		} else {
			refresh_toolbar(ft_chan_person, current_session_->receiver->id);
		}
	}

	// focus back to input.
	window_->keyboard_capture(input_tb_);
}

void tchat_::toggle_tabbar(twidget* widget)
{
	tdialog::toggle_tabbar(widget);
	int catalog_tab = (int)reinterpret_cast<long>(widget->cookie());
	if (catalog_tab == 0) {
		contact_switch_to_person();

	} else if (catalog_tab == 1) {
		contact_switch_to_channel();
	}
}

void tchat_::click_tabbar(twidget* widget, const std::string& sparam)
{
	irc::server* serv = lobby->chat.serv();
	utils::string_map symbols;
	std::stringstream ss;

	bool require_reload = false;
	int id = (int)reinterpret_cast<long>(widget->cookie());
	if (id == f_netdiag) {
		netdiag(*window_);

	} else if (id == f_previous_page) {
		current_session_->current_page ++;
		chat_2_scroll_label(*history_, *current_session_);

		require_reload = true;

	} else if (id == f_next_page) {
		current_session_->current_page --;
		chat_2_scroll_label(*history_, *current_session_);

		require_reload = true;

	} else if (id == f_part) {
		tlobby_channel& channel = lobby->chat.get_channel(current_session_->receiver->id);

		symbols["part"] = tintegrate::generate_format(_("Part"), "yellow");
		symbols["channel"] = tintegrate::generate_format(channel.nick, "red");
		ss << vgettext("wesnoth-lib", "After $part, you can not receive message from $channel. Do you want to $part?", symbols);
		int res = gui2::show_message(disp_.video(), "", ss.str(), gui2::tmessage::yes_no_buttons);
		if (res != gui2::twindow::OK) {
			return;
		}
		lobby->chat.erase_channel(current_session_->receiver->id);
		remove_branch(false, current_session_->receiver->id);

		// cannot part app-id channel.
		simulate_cookie_toggled(false, tlobby_channel::get_cid(game_config::app_channel), tlobby_channel::get_cid(game_config::app_channel), true);

	} else if (id == f_part_friend) {
		tlobby_channel& channel = lobby->chat.get_channel(tlobby_channel::t_friend);
		tlobby_user& user = channel.get_user(current_session_->receiver->id);

		symbols["nick"] = tintegrate::generate_format(user.nick, "yellow");
		symbols["channel"] = tintegrate::generate_format(channel.name(), "green");
		ss << vgettext("wesnoth-lib", "Do you want remove $nick from $channel?", symbols);
		int res = gui2::show_message(disp_.video(), "", ss.str(), gui2::tmessage::yes_no_buttons);
		if (res != gui2::twindow::OK) {
			return;
		}

		channel.erase_user(user.uid);

		serv->p_monitor(serv, current_session_->receiver->nick.c_str(), false);

		std::vector<tcookie>& branch = person_cookies_.find(tlobby_channel::t_friend)->second;
		size_t pos = -1;
		for (std::vector<tcookie>::iterator it = branch.begin(); it != branch.end(); ++ it) {
			const tcookie& cookie = *it;
			if (!cookie.channel && cookie.id == current_session_->receiver->id) {
				pos = std::distance(branch.begin(), it);
				break;
			}
		}
		VALIDATE(pos < branch.size(), "must exist user!");
		erase_user(true, branch, current_session_->receiver->id);
		if (pos >= branch.size()) {
			pos --;
		}
		if (pos) {
			simulate_cookie_toggled(true, tlobby_channel::t_friend, branch[pos].id, branch[pos].channel);

		} else {
			simulate_cookie_toggled(false, tlobby_channel::get_cid(game_config::app_channel), tlobby_channel::get_cid(game_config::app_channel), true);
			toggle_tabbar(catalog_->get_widget(at_channel));
		}

	} else if (id == f_explore) {
		tlobby_channel& channel = lobby->chat.get_channel(current_session_->receiver->id);

		switch_to_channel(*window_);
		generate_channel_tree(channel);

	}

	if (require_reload) {
		refresh_toolbar(current_ft_, current_session_->receiver->id);
	}
}

void tchat_::post_show(twindow& window)
{
	person_tree_->get_root_node().clear();
	person_tree_ = NULL;
	channel_tree_->get_root_node().clear();
	channel_tree_ = NULL;

	input_ = NULL;
	send_ = NULL;
	previous_page_ = NULL;
	next_page_ = NULL;

	signature_ = 0;
	person_cookies_.clear();
	channel_cookies_.clear();
	sessions_.clear();
	current_session_ = NULL;
}

bool tchat_::gui_ready() const
{
	return signature_ != 0;
}

void tchat_::reload_catalog(twindow& window)
{
	catalog_->erase_children();

	std::vector<std::string> catalogs;
	catalogs.push_back("misc/contact-person.png");
	catalogs.push_back("misc/contact-channel.png");

	std::stringstream ss;
	int index = 0;
	for (std::vector<std::string>::const_iterator it = catalogs.begin(); it != catalogs.end(); ++ it) {
		tcontrol* widget = catalog_->create_child(null_str, null_str, NULL, null_str);
		widget->set_label(*it);
		widget->set_cookie(reinterpret_cast<void*>(index ++));
		catalog_->insert_child(*widget);
	}
	catalog_->select(0);
	catalog_->replacement_children();
}

void tchat_::process_message(const std::string& chan, const std::string& from, const std::string& text)
{
	tlobby_user& user = lobby->chat.get_user(tlobby_user::get_uid(from));
	std::pair<std::vector<tcookie>*, tcookie* > pair;
	int receive_id;
	if (chan.empty()) {
		if (lobby->chat.is_favor_user(user.uid)) {
			pair = contact_find(true, tlobby_channel::npos, user.uid, false);
		} else {
			pair = contact_find(false, tlobby_channel::npos, user.uid, false);
			// here, if a tempraral chat, current not support. cookie.first is NULL.
		}
		receive_id = user.uid;

	} else {
		int cid = tlobby_channel::get_cid(chan);
		pair = contact_find(false, cid, cid, true);
		receive_id = cid;
	}

	if (!pair.first) {
		return;
	}

	chat_logs::add(receive_id, !chan.empty(), user, utils::is_utf8str(text)? tintegrate::stuff_escape(text): err_encode_str);
	if (current_session_ && pair.second->channel == current_session_->receiver->channel && pair.second->id == current_session_->receiver->id) {
		chat_2_scroll_label(*history_, *current_session_);

	} else {
		pair.second->unread ++;
		update_node_internal(*pair.first, *pair.second);
	}
}

void tchat_::process_userlist(const std::string& chan, const std::string& names)
{
	std::vector<std::string> vstr = utils::split(names, ' ');
	tlobby_channel& channel = lobby->chat.get_channel(tlobby_channel::get_cid(chan));

	if (!channel.users_receiving) {
		clear_branch(false, channel.cid);
	}

	std::vector<tcookie>& branch = channel_cookies_.find(channel.cid)->second;
	char* nick_prefixes = lobby->chat.serv()->nick_prefixes;
	std::string nopre_nick;
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		const std::string& nick = *it;
		nopre_nick = *it;
		// Ignore prefixes so '!' won't cause issues
		if (strchr(nick_prefixes, nopre_nick[0])) {
			nopre_nick = nopre_nick.substr(1);
		}

		int uid = tlobby_user::get_uid(nopre_nick);
		const tlobby_user& user = channel.get_user(uid);
		insert_user(false, branch, user);
	}
}

void tchat_::process_userlist_end(const std::string& chan)
{
	tlobby_channel& channel = lobby->chat.get_channel(tlobby_channel::get_cid(chan));

	// std::vector<tcookie>& branch = channel_cookies_.find(channel.cid)->second;
	// branch.front().node->fold();

	if (current_ft_ == ft_channel && current_session_->receiver->id == channel.cid) {
		refresh_toolbar(current_ft_, current_session_->receiver->id);
	}
}

void tchat_::process_join(const std::string& chan, const std::string& nick)
{
	tlobby_channel& channel = lobby->chat.get_channel(tlobby_channel::get_cid(chan));
	tlobby_user& user = channel.get_user(tlobby_user::get_uid(nick));

	std::vector<tcookie>& branch = channel_cookies_.find(channel.cid)->second;
	insert_user(false, branch, user);

	if (lobby->chat.is_favor_user(user.uid)) {
		std::pair<std::vector<tchat_::tcookie>*, tchat_::tcookie* > ret = tchat_::contact_find(true, tlobby_channel::t_friend, user.uid, false);
		tcookie* cookie = ret.second;
		if (!cookie->online) {
			cookie->online = user.online;
			cookie->away = user.away;
			update_node_internal(*ret.first, *ret.second);
		}
	}
}

void tchat_::process_uparted(const std::string& chan)
{
}

void tchat_::process_part(const std::string& chan, const std::string& nick, const std::string& reason)
{
	tlobby_channel& channel = lobby->chat.get_channel(tlobby_channel::get_cid(chan));
	std::vector<tcookie>& branch = channel_cookies_.find(channel.cid)->second;
	int uid = tlobby_user::get_uid(nick, false);
	erase_user(false, branch, uid);
}

void tchat_::process_online(const char* nicks)
{
	std::vector<std::string> vstr;
	tlobby::tchat_sock::split_offline_online(nicks, vstr);

	std::pair<std::vector<tchat_::tcookie>*, tchat_::tcookie* > ret;
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		const std::string& nick = *it;
		int uid = tlobby_user::get_uid(nick);
		tlobby_user& user = lobby->chat.get_user(uid);
		if (!user.valid()) {
			continue;
		}

		if (lobby->chat.is_favor_user(uid)) {
			ret = tchat_::contact_find(true, tlobby_channel::t_friend, uid, false);
			if (ret.first) {
				tcookie* cookie = ret.second;
				if (!cookie->online) {
					cookie->online = true;
					cookie->away = false;
					update_node_internal(*ret.first, *ret.second);
				}
			}
		}

	}
}

void tchat_::process_offline(const char* nicks)
{
	std::vector<std::string> vstr;
	tlobby::tchat_sock::split_offline_online(nicks, vstr);

	std::pair<std::vector<tchat_::tcookie>*, tchat_::tcookie* > ret;
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		const std::string& nick = *it;
		int uid = tlobby_user::get_uid(nick);
		tlobby_user& user = lobby->chat.get_user(uid);
		if (!user.valid()) {
			continue;
		}

		if (lobby->chat.is_favor_user(uid)) {
			ret = tchat_::contact_find(true, tlobby_channel::t_friend, uid, false);
			if (ret.first) {
				tcookie* cookie = ret.second;
				if (cookie->online) {
					cookie->online = false;
					update_node_internal(*ret.first, *ret.second);
				}
			}
		}
	}
}

void tchat_::process_forbid_join(const std::string& chan, const std::string& reason)
{
	int cid = tlobby_channel::get_cid(chan);
	tlobby_channel& channel = lobby->chat.get_channel(cid);

	std::vector<tcookie>& branch = channel_cookies_.find(cid)->second;
	update_node_internal(branch, branch.front());
}

void tchat_::process_whois(const std::string& chan, const std::string& nick, bool online, bool away)
{
	int uid = tlobby_user::get_uid(nick);
	tlobby_user& user = lobby->chat.get_user(uid);

	std::pair<std::vector<tchat_::tcookie>*, tchat_::tcookie* > ret;
	if (irc::is_channel(lobby->chat.serv(), chan.c_str())) {
		ret = tchat_::contact_find(false, tlobby_channel::get_cid(chan), user.uid, false);
		if (ret.first) {
			tcookie* cookie = ret.second;
			if (cookie->online != online || (online && cookie->away != away)) {
				cookie->online = online;
				cookie->away = away;
				update_node_internal(*ret.first, *ret.second);
			}
		}
	}

	if (lobby->chat.is_favor_user(uid)) {
		ret = tchat_::contact_find(true, tlobby_channel::t_friend, user.uid, false);
		if (ret.first) {
			tcookie* cookie = ret.second;
			if (cookie->online != online || (online && cookie->away != away)) {
				cookie->online = online;
				cookie->away = away;
				update_node_internal(*ret.first, *ret.second);
			}
		}
	}

	if (ret.first && current_session_ && ret.second->id == current_session_->receiver->id && ret.second->channel == current_session_->receiver->channel) {
		send_->set_active(true);
	}
}

void tchat_::process_quit(const std::string& nick)
{
	tlobby_user& user = lobby->chat.get_user(tlobby_user::get_uid(nick));

	for (std::set<int>::const_iterator it = user.cids.begin(); it != user.cids.end(); ++ it) {
		int cid = *it;
		if (tlobby_channel::is_allocatable(cid)) {
			std::vector<tcookie>& branch = channel_cookies_.find(cid)->second;
			erase_user(false, branch, user.uid);
		} else {
			std::pair<std::vector<tchat_::tcookie>*, tchat_::tcookie* > ret = tchat_::contact_find(true, tlobby_channel::t_friend, user.uid, false);
			if (ret.first) {
				tcookie* cookie = ret.second;
				if (cookie->online) {
					cookie->online = false;
					update_node_internal(*ret.first, *ret.second);
				}
			}
		}
	}
}

void tchat_::find_chan_toggled(twindow& window)
{
	bool active = !list_chans_.empty() && chanlist_->get_item_count();
	if (active) {
		const tlobby_channel& channel = lobby->chat.get_channel(tlobby_channel::get_cid(list_chans_[chanlist_->get_selected_row()], false));
		active = !channel.valid();
	}
	join_channel_->set_active(active);
}

void tchat_::process_chanlist_start()
{
	in_find_chan_ = true;
	list_chans_.clear();
	chanlist_->clear();
	tlabel& label = find_widget<tlabel>(window_, "_chat_find_result", false);
	label.set_label("0/0");

	switch_to_chat_find_->set_active(false);
	find_->set_label(dgettext("wesnoth-lib", "Cancel"));
}

void tchat_::process_chanlist(const std::string& chan, int users, const std::string& topic)
{
	if (!in_find_chan_) {
		return;
	}
	if (users < cond_min_users_) {
		return;
	}

	if (std::find(list_chans_.begin(), list_chans_.end(), chan) == list_chans_.end()) {
		list_chans_.push_back(chan);
	} else {
		return;
	}
	if (list_chans_.size() <= 100) {
		string_map list_item;
		std::map<std::string, string_map> list_item_item;

		list_item["label"] = str_cast(list_chans_.size());
		list_item_item.insert(std::make_pair("number", list_item));
		list_item["label"] = chan;
		list_item_item.insert(std::make_pair("nick", list_item));
		list_item["label"] = str_cast(users);
		list_item_item.insert(std::make_pair("users", list_item));
		list_item["label"] = tintegrate::stuff_escape(utils::is_utf8str(topic)? topic: err_encode_str);
		list_item_item.insert(std::make_pair("topic", list_item));

		chanlist_->add_row(list_item_item);
	}

	std::stringstream ss;
	ss << chanlist_->get_item_count() << "/" << list_chans_.size();
	tlabel& label = find_widget<tlabel>(window_, "_chat_find_result", false);
	label.set_label(ss.str());
}

void tchat_::process_chanlist_end()
{
	if (!in_find_chan_) {
		return;
	}

	in_find_chan_ = false;
	chanlist_->invalidate_layout(true);

	switch_to_chat_find_->set_active(true);
	find_->set_label(dgettext("wesnoth-lib", "Find"));
	find_->set_active(lobby->chat.ready());
	find_chan_toggled(*window_);
}

void tchat_::process_network_status(bool connected)
{
	if (connected) {
		network::send_data(lobby->chat, config("refresh_lobby"));
	}
}

std::string tchat_::format_log_str(const tsession& sess, std::vector<tintegrate::tlocator>& locator) const
{
	std::vector<chat_logs::tlog> logs;
	sess.current_logs(logs);

	std::stringstream ss;
	int start;
	const std::string& my_nick = lobby->chat.me? lobby->chat.me->nick: lobby->nick();
	const config& bubble_cfg = settings::bubbles.find("default")->second;
	for (std::vector<chat_logs::tlog>::const_iterator it = logs.begin(); it != logs.end(); ++ it) {
		const chat_logs::tlog& log = *it;
		if (it != logs.begin()) {
			ss << "\n";
			locator.push_back(tintegrate::tlocator(bubble_cfg, start, ss.str().size()));
			ss << "\n";
		}
		if (log.nick == my_nick) {
			ss << tintegrate::generate_format(log.nick, "green");
		} else {
			ss << tintegrate::generate_format(log.nick, "yellow");
		}
		ss << "    " << format_time_date(log.t) << "\n";
		start = ss.str().size();
		ss << log.msg;
	}
	if (!logs.empty()) {
		locator.push_back(tintegrate::tlocator(bubble_cfg, start));
	}
	return ss.str();
}

void tchat_::chat_2_scroll_label(tscroll_label& label, const tsession& sess)
{
	std::vector<tintegrate::tlocator> locator;
	std::string str = format_log_str(sess, locator);

	tlabel* widget = find_widget<tlabel>(label.content_grid(), "_label", false, true);

	label.set_label(str);
	widget->refresh_locator_anim(locator);
}

bool tchat_::handle_raw(int at, tsock::ttype type, const char* param[])
{
	if (!gui_ready()) {
		return false;
	}

	if (type == tsock::t_connected) {
		find_->set_active(true);
		return false;

	} else if (type == tsock::t_disconnected) {
		for (std::map<int, tlobby_channel>::iterator it = lobby->chat.channels.begin(); it != lobby->chat.channels.end(); ++ it) {
			bool person = !tlobby_channel::is_allocatable(it->first);
			tlobby_channel& channel = it->second;
			if (person) {
				std::vector<tcookie>& branch = person_cookies_.find(it->first)->second;
				std::vector<tcookie>::iterator it2 = branch.begin();
				for (++ it2; it2 != branch.end(); ++ it2) {
					tcookie& cookie = *it2;
					if (cookie.online) {
						cookie.online = false;
						update_node_internal(branch, cookie);
					}
				}

			} else {
				clear_branch(person, it->first);
				std::vector<tcookie>& branch = channel_cookies_.find(it->first)->second;
				update_node_internal(branch, branch.front());
			}
		}
		
		if (dynamic_cast<ttoggle_button*>(catalog_->get_widget(at_channel))->get_value()) {
			simulate_cookie_toggled(false, tlobby_channel::get_cid(game_config::app_channel), tlobby_channel::get_cid(game_config::app_channel), true);
		} else {
			send_->set_active(false);
			find_->set_active(false);

			if (current_ft_ != ft_none) {
				refresh_toolbar(current_ft_, current_session_->receiver->id);
			}
		}
		return false;
	}

	bool halt = true;
	if (!strcasecmp(param[0], "NAMREPLAY")) {
		process_userlist(param[1], param[2]);

	} else if (!strcasecmp(param[0], "ENDOFNAMES")) {
		process_userlist_end(param[1]);

	} else if (!strcasecmp(param[0], "UJOINED")) {
		
	} else if (!strcasecmp(param[0], "UPARTED")) {
		process_uparted(param[1]);

	} else if (!strcasecmp(param[0], "PART")) {
		process_part(param[1], param[2], param[3]);

	} else if (!strcasecmp(param[0], "JOIN")) {
		process_join(param[1], param[2]);

	} else if (!strcasecmp(param[0], "WHOREPLAY")) {
		process_whois(param[1], param[2], utils::to_bool(param[3]), utils::to_bool(param[4]));

	} else if (!strcasecmp(param[0], "QUIT")) {
		process_quit(param[1]);

	} else if (!strcasecmp(param[0], "PRIVMSG")) {
		process_message(param[1], param[2], param[3]);

	} else if (!strcasecmp(param[0], "LISTSTART")) {
		process_chanlist_start();

	} else if (!strcasecmp(param[0], "LIST")) {
		process_chanlist(param[1], utils::to_int(param[2]), param[3]);

	} else if (!strcasecmp(param[0], "LISTEND")) {
		process_chanlist_end();

	} else if (!strcasecmp(param[0], "OFFLINE")) {
		process_offline(param[1]);

	} else if (!strcasecmp(param[0], "ONLINE")) {
		process_online(param[1]);

	} else if (!strcasecmp(param[0], "FORBIDJOIN")) {
		process_forbid_join(param[1], param[2]);

	} else {
		halt = false;
	}
	return halt;
}

void tchat_::swap_page(twindow& window, int page, bool swap)
{
	if (page < min_page_ || !page_panel_) {
		return;
	}
	int index = page - min_page_;

	if (page_panel_->current_page() == index) {
		// desired page is the displaying page, do nothing.
		return;
	}

	if (current_page_ == chat_page_) {
		tchat_::post_show(window);
	} else {
		desire_swap_page(window, current_page_, false);
	}

	page_panel_->swap_uh(window, index);
	if (swap) {
		page_panel_->swap_bh(window);
	}

	if (page == chat_page_) {
		tchat_::pre_show(window);
		network::send_data(lobby->chat, config("refresh_lobby"));

	} else {
		desire_swap_page(window, page, true);
	}

	current_page_ = page;
}

tchat2::tchat2(display& disp, tgroup& g)
	: tchat_(disp, g, MIN_PAGE, CHAT_PAGE, CHATING_PAGE)
	, disp_(disp)
{
}

void tchat2::sheet_toggled(twidget* widget)
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

void tchat2::pre_show(CVideo& /*video*/, twindow& window)
{
	window.set_escape_disabled(true);

	sheet_.insert(std::make_pair((int)CHAT_PAGE, find_widget<ttoggle_button>(&window, "chat", false, true)));
	for (std::map<int, ttoggle_button*>::iterator it = sheet_.begin(); it != sheet_.end(); ++ it) {
		it->second->set_callback_state_change(boost::bind(&tchat2::sheet_toggled, this, _1));
		it->second->set_data(it->first);
	}

	page_panel_ = find_widget<tscrollbar_panel>(&window, "page", false, true);
	swap_page(window, CHAT_PAGE, false);
	sheet_.begin()->second->set_value(true);

	update_network_status(window, lobby->chat.ready());
	join();
}

bool tchat2::handle_raw(int at, tsock::ttype type, const char* param[])
{
	if (at != tlobby::tag_chat) {
		return false;
	}

	if (type == tsock::t_connected || type == tsock::t_disconnected) {
		update_network_status(*page_panel_->get_window(), type == tsock::t_connected);
		process_network_status(type == tsock::t_connected);
	}

	return tchat_::handle_raw(at, type, param);
}

void tchat2::update_network_status(twindow& window, bool connected)
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

}
