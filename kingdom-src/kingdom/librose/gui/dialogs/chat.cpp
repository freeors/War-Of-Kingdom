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
#include "gui/dialogs/transient_message.hpp"
#include "gui/dialogs/netdiag.hpp"
#include "formula_string_utils.hpp"
#include "wml_separators.hpp"
#include <hero.hpp>
#include "filesystem.hpp"

#include <boost/foreach.hpp>
#include <boost/bind.hpp>

std::map<int, std::vector<tchat_log> > chat_logs;
void add_chat_log(int uid, const std::string& msg, bool other)
{
	std::map<int, std::vector<tchat_log> >::iterator it = chat_logs.find(uid);
	if (it == chat_logs.end()) {
		std::vector<tchat_log> v(1, tchat_log(msg, time(NULL), other));
		chat_logs.insert(std::make_pair(uid, v));
	} else {
		it->second.push_back(tchat_log(msg, time(NULL), other));
	}
}

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

const std::string ctrlid_contact = "_chat_contact";
const std::string ctrlid_history = "_chat_history";
const std::string ctrlid_qq_icon = "_chat_qq_icon";
const std::string ctrlid_qq_name = "_chat_qq_name";
const std::string ctrlid_input = "_chat_input";
const std::string ctrlid_network = "_chat_network";
const std::string ctrlid_backward = "_chat_backward";
const std::string ctrlid_send = "_chat_send";
const std::string ctrlid_send_tip = "_chat_send_tip";

tchat_::tchat_(display& disp, tgroup& g, int min_page, int chat_page, int chating_page)
	: disp_(disp)
	, group_(g)
	, signature_(0)
	, cookies_()
	, gamelist_()
	, window_(NULL)
	, history_(NULL)
	, input_(NULL)
	, input_tb_(NULL)
	, send_(NULL)
	, extend_(NULL)
	, src_pos_(0)
	, current_chat_to_(NULL)
	, swap_page_chat_to_()
	, swap_page_input_()
	, min_page_(min_page)
	, current_page_(min_page - 1)
	, chat_page_(chat_page)
	, chating_page_(chating_page)
	, page_panel_(NULL)
{
}

void tchat_::user_to_title(const tcookie& cookie) const
{
	tcontrol& icon = find_widget<tcontrol>(window_, ctrlid_qq_icon, false);
	icon.set_label("misc/chat-portrait.png");

	tlabel& label = find_widget<tlabel>(window_, ctrlid_qq_name, false);
	label.set_label(cookie.username);
}

void tchat_::contact_toggled(twidget* widget)
{
	ttoggle_panel* toggle = dynamic_cast<ttoggle_panel*>(widget);
	std::pair<std::vector<tcookie>*, tcookie* > cookie = contact_find(toggle->get_data());

	user_to_title(*cookie.second);

	current_chat_to_ = cookie.second;
	if (current_chat_to_->unread) {
		current_chat_to_->unread = 0;
		update_node_internal(*cookie.first, *cookie.second);
	}
	send_->set_active(lobby.ready() && current_chat_to_->online);
	extend_->set_active(true);

	chat_2_scroll_label(*history_, current_chat_to_->uid, current_chat_to_->username);

	// focus back to input.
	window_->keyboard_capture(input_tb_);
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
	network::send_data(data, 0);
}

bool is_blank_str(const std::string& str)
{
	const char* blank_char = " \n";
	size_t pos = str.find_first_not_of(blank_char);
	return pos == std::string::npos;
}

void tchat_::send(bool& handled, bool& halt)
{
	twindow& window = *window_;

	if (!lobby.ready()) {
		return;
	}

	if (!current_chat_to_) {
		return;
	}
	std::string input_str = input_->get_value();
	if (is_blank_str(input_str)) {
		return;
	}
	send_whisper(group_, current_chat_to_->username, input_str);

	input_->set_value(null_str);

	add_chat_log(current_chat_to_->uid, input_str, false);
	chat_2_scroll_label(*history_, current_chat_to_->uid, current_chat_to_->username);

// #if (defined(__APPLE__) && TARGET_OS_IPHONE) || defined(ANDROID)
	if (current_page_ != chat_page_) {
		swap_page2(handled, halt, chat_page_);
	}
// #endif
}

void tchat_::insert_face(twindow& window, int index)
{
	std::stringstream ss;
	ss << "face/default/" << index << ".png";
	ss.str();
	tscroll_text_box& input = find_widget<tscroll_text_box>(window_, ctrlid_input, false);
	input.insert_img(ss.str());
}

void tchat_::keyboard_shown(twindow& window)
{
#if (defined(__APPLE__) && TARGET_OS_IPHONE) || defined(ANDROID)
#else
	if (current_chat_to_) {
		chat_2_scroll_label(*history_, current_chat_to_->uid, current_chat_to_->username);
	}
#endif
}

void tchat_::keyboard_hidden(twindow& window)
{
	if (current_chat_to_) {
		chat_2_scroll_label(*history_, current_chat_to_->uid, current_chat_to_->username);
	}
	settings::keyboard_height = 0;
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
		
        if (current_page_ != chating_page_) {
            swap_page2(handled, halt, chating_page_);
        } else {
            window_->invalidate_layout();
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
		swap_page2(handled, halt, chat_page_);
	}
#endif
	if (key == SDLK_ESCAPE && current_chat_to_) {
		int to_page = current_page_ == chat_page_? chating_page_: chat_page_;
		swap_page2(handled, halt, to_page);
	}
}

void tchat_::ready_branch(twindow& window, int type)
{
	ttree_view& parent_tree = find_widget<ttree_view>(&window, ctrlid_contact, false);
	string_map tree_group_field;
	std::map<std::string, string_map> tree_group_item;
	tcookie cookie;

	std::string name;
	if (type == tgroup::tassociate::ally) {
		name = dgettext("wesnoth-lib", "Ally");
	} else if (type == tgroup::tassociate::requestterminate) {
		name = dgettext("wesnoth-lib", "Request terminate");
	} else if (type == tgroup::tassociate::none) {
		name = dgettext("wesnoth-lib", "Normal");
	} else if (type == tgroup::tassociate::requestally) {
		name = dgettext("wesnoth-lib", "Request ally");
	}

	tree_group_field["label"] = null_str;
	tree_group_item["icon"] = tree_group_field;
	tree_group_field["label"] = name;
	tree_group_item["name"] = tree_group_field;
	cookie = tcookie(signature_ ++, &parent_tree.get_root_node().add_child("item", tree_group_item), name);
	cookies_.insert(std::make_pair(type, std::vector<tcookie>(1, cookie)));
}

std::pair<std::vector<tchat_::tcookie>*, tchat_::tcookie* > tchat_::contact_find(size_t signature)
{
	std::pair<std::vector<tcookie>*, tcookie* > ret = std::make_pair(reinterpret_cast<std::vector<tcookie>*>(NULL), reinterpret_cast<tcookie*>(NULL));
	for (std::map<int, std::vector<tcookie> >::iterator it = cookies_.begin(); it != cookies_.end() && !ret.first; ++ it) {
		for (std::vector<tcookie>::iterator it2 = it->second.begin(); it2 != it->second.end() && !ret.first; ++ it2) {
			if (it2->signature == signature) {
				ret.first = &it->second;
				ret.second = &*it2;
			}
		}
	}
	return ret;
}

std::pair<std::vector<tchat_::tcookie>*, tchat_::tcookie* > tchat_::contact_find(const std::string& username)
{
	std::pair<std::vector<tcookie>*, tcookie* > ret = std::make_pair(reinterpret_cast<std::vector<tcookie>*>(NULL), reinterpret_cast<tcookie*>(NULL));
	for (std::map<int, std::vector<tcookie> >::iterator it = cookies_.begin(); it != cookies_.end() && !ret.first; ++ it) {
		for (std::vector<tcookie>::iterator it2 = it->second.begin(); it2 != it->second.end() && !ret.first; ++ it2) {
			if (it2->username == username) {
				ret.first = &it->second;
				ret.second = &*it2;
			}
		}
	}
	return ret;
}

std::pair<const std::vector<tchat_::tcookie>*, const tchat_::tcookie* > tchat_::contact_find(const std::string& username) const
{
	std::pair<const std::vector<tcookie>*, const tcookie* > ret = std::make_pair(reinterpret_cast<std::vector<tcookie>*>(NULL), reinterpret_cast<tcookie*>(NULL));
	for (std::map<int, std::vector<tcookie> >::const_iterator it = cookies_.begin(); it != cookies_.end() && !ret.first; ++ it) {
		for (std::vector<tcookie>::const_iterator it2 = it->second.begin(); it2 != it->second.end() && !ret.first; ++ it2) {
			if (it2->username == username) {
				ret.first = &it->second;
				ret.second = &*it2;
			}
		}
	}
	return ret;
}

void tchat_::update_node(twindow& window, size_t signature)
{
	std::pair<const std::vector<tcookie>*, const tcookie* > cookie = contact_find(signature);
	update_node_internal(*cookie.first, *cookie.second);
}

void tchat_::update_node_internal(const std::vector<tcookie>& cookies, const tcookie& cookie)
{
	std::stringstream strstr;

	if (cookie.is_chapter()) {
		size_t online = 0;
		for (std::vector<tcookie>::const_iterator it = cookies.begin(); it != cookies.end(); ++ it) {
			const tcookie& c2 = *it;
			if (c2.online) {
				online ++;
			}
		}
		strstr << cookie.username << "  " << online << "/ " << (cookies.size() - 1) << "";
	} else {
		std::string icon_str = "misc/chat-portrait.png";
		if (!cookie.online) {
			icon_str.append("~GS()");
		}
		tcontrol* icon = dynamic_cast<tcontrol*>(cookie.node->find("icon", true));
		icon->set_label(icon_str);

		strstr << cookie.username;

		std::stringstream flag_ss;
		if (cookie.unread) {
			flag_ss << " (" << tintegrate::generate_format(cookie.unread, "green") << ")";
		}
		if (cookie.game) {
			flag_ss << tintegrate::generate_img("misc/status-game.png");
		}
		if (!flag_ss.str().empty()) {
			strstr << "\n" << flag_ss.str();
		}
		// tlabel* label = dynamic_cast<tlabel*>(cookie.node->find("flag", true));
		// label->set_label(flag_ss.str());
	}
	tlabel* label = dynamic_cast<tlabel*>(cookie.node->find("name", true));
	label->set_label(strstr.str());
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

void tchat_::ready_chat_toolbar(twindow& window)
{
	connect_signal_mouse_left_click(
		  *extend_
		, boost::bind(
			  &tchat_::swap_page2
			, this
			, _3, _4
			, chating_page_));
#if (defined(__APPLE__) && TARGET_OS_IPHONE) || defined(ANDROID)
	extend_->set_visible(twidget::INVISIBLE);
#endif
}

void tchat_::pre_show(twindow& window)
{
	window_ = &window;
	history_ = &find_widget<tscroll_label>(window_, ctrlid_history, false);
	history_->set_scroll_to_end(true);
	input_ = &find_widget<tscroll_text_box>(window_, ctrlid_input, false);
	send_ = &find_widget<tbutton>(window_, ctrlid_send, false);
	extend_ = &find_widget<tbutton>(window_, "_chat_extend", false);

	std::stringstream ss;
	tlabel& label = find_widget<tlabel>(window_, ctrlid_send_tip, false);
	label.set_label("------");

	tlabel& send_tip = find_widget<tlabel>(window_, ctrlid_send_tip, false);
	ss.str("");
	ss << dgettext("wesnoth-lib", "Press CR same as Send");
	label.set_label(ss.str());
#if (defined(__APPLE__) && TARGET_OS_IPHONE) || defined(ANDROID)
	label.set_visible(twidget::INVISIBLE);
#endif

	tscroll_text_box& input = find_widget<tscroll_text_box>(window_, ctrlid_input, false);
	ttext_box& tb = find_widget<ttext_box>(input.content_grid(), "_text_box", false);
	input_tb_ = &tb;
	connect_signal_pre_key_press(tb, boost::bind(&tchat_::signal_handler_sdl_key_down, this, _3, _4, _5, _6, _7));
	window.keyboard_capture(&tb);

	connect_signal_mouse_left_click(
			  *send_
			, boost::bind(
				  &tchat_::send
				, this
				, _3, _4));
	send_->set_active(false);
	extend_->set_active(swap_page_chat_to_.valid_user());

	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, ctrlid_network, false)
			, boost::bind(
				  &tchat_::netdiag
				, this
				, boost::ref(window)));

	ttree_view& parent_tree = find_widget<ttree_view>(&window, ctrlid_contact, false);
	ttree_view_node& htvroot = parent_tree.get_root_node();
	
	string_map tree_group_field;
	std::map<std::string, string_map> tree_group_item;

	ready_branch(window, tgroup::tassociate::ally);
	ready_branch(window, tgroup::tassociate::requestterminate);
	ready_branch(window, tgroup::tassociate::none);
	ready_branch(window, tgroup::tassociate::requestally);

	std::vector<tgroup::tassociate>& associates = group_.associates();
	for (std::vector<tgroup::tassociate>::const_iterator it = associates.begin(); it != associates.end(); ++ it) {
		const tgroup::tassociate& a = *it;
		tree_group_field["label"] = a.username;
		tree_group_item["name"] = tree_group_field;
		std::vector<tcookie>& branch = cookies_.find(a.agreement)->second;
		ttree_view_node& branch_node = *branch.front().node;
		branch.push_back(tcookie(signature_, &branch_node.add_child("item", tree_group_item), a.username, a.uid));

		ttoggle_panel& toggle =find_widget<ttoggle_panel>(branch.back().node, "tree_view_node_label", false);
		toggle.set_data(signature_ ++);
		toggle.set_callback_state_change(boost::bind(&tchat_::contact_toggled, this, _1));

		// ready chat log
		std::map<int, std::vector<tchat_log> >::iterator find_it = chat_logs.find(a.uid);
		if (find_it == chat_logs.end()) {
			std::vector<tchat_log> v;
			chat_logs.insert(std::make_pair(a.uid, v));
		}
	}

	for (std::map<int, std::vector<tcookie> >::iterator it = cookies_.begin(); it != cookies_.end(); ++ it) {
		for (std::vector<tcookie>::iterator it2 = it->second.begin(); it2 != it->second.end(); ++ it2) {
			tcookie& cookie = *it2;
			update_node_internal(it->second, cookie);
			if (swap_page_chat_to_.valid_user() && swap_page_chat_to_.uid == cookie.uid) {
				current_chat_to_ = &cookie;
				parent_tree.set_select_item(cookie.node);
			}
		}
	}
	if (current_chat_to_) {
		user_to_title(*current_chat_to_);
	}

	input_->set_value(swap_page_input_);

	input_tb_->set_src_pos(src_pos_);

#if (defined(__APPLE__) && TARGET_OS_IPHONE) || defined(ANDROID)
	const tpoint& size = input_->best_size();
	input_->set_best_size(tpoint(size.x, 0));
#endif

	ready_chat_toolbar(window);
}

void tchat_::post_show(twindow& window)
{
	ttree_view& parent_tree = find_widget<ttree_view>(&window, ctrlid_contact, false);
	ttree_view_node& htvroot = parent_tree.get_root_node();
	htvroot.clear();

	input_ = NULL;
	send_ = NULL;
	extend_ = NULL;

	signature_ = 0;
	cookies_.clear();
	current_chat_to_ = NULL;
}

void tchat_::pre_chating_show(twindow& window)
{
	window_ = &window;
	history_ = &find_widget<tscroll_label>(window_, ctrlid_history, false);
	history_->set_scroll_to_end(true);
	input_ = &find_widget<tscroll_text_box>(window_, ctrlid_input, false);
	send_ = &find_widget<tbutton>(window_, ctrlid_send, false);

	std::stringstream ss;
	tlabel& label = find_widget<tlabel>(window_, ctrlid_send_tip, false);
	label.set_label("------");

	tlabel& send_tip = find_widget<tlabel>(window_, ctrlid_send_tip, false);
	ss.str("");
	ss << dgettext("wesnoth-lib", "Press CR same as Send");
	label.set_label(ss.str());
#if (defined(__APPLE__) && TARGET_OS_IPHONE) || defined(ANDROID)
	label.set_visible(twidget::INVISIBLE);
#endif

	tscroll_text_box& input = find_widget<tscroll_text_box>(window_, ctrlid_input, false);
	ttext_box& tb = find_widget<ttext_box>(input.content_grid(), "_text_box", false);
	input_tb_ = &tb;
	connect_signal_pre_key_press(tb, boost::bind(&tchat_::signal_handler_sdl_key_down, this, _3, _4, _5, _6, _7));
	window.keyboard_capture(&tb);

	connect_signal_mouse_left_click(
			  *send_
			, boost::bind(
				  &tchat_::send
				, this
				, _3, _4));
	send_->set_active(lobby.ready() && swap_page_chat_to_.valid_user() && swap_page_chat_to_.online);

	connect_signal_mouse_left_click(
		  find_widget<tbutton>(&window, ctrlid_backward, false)
		, boost::bind(
			  &tchat_::swap_page2
			, this
			, _3, _4
			, chat_page_));
#if (defined(__APPLE__) && TARGET_OS_IPHONE) || defined(ANDROID)
	find_widget<tbutton>(&window, ctrlid_backward, false).set_visible(twidget::INVISIBLE);
#endif

	if (swap_page_chat_to_.valid_user()) {
		current_chat_to_ = &swap_page_chat_to_;
		user_to_title(swap_page_chat_to_);
	}
	input_->set_value(swap_page_input_);
	input_tb_->set_src_pos(src_pos_);

	ready_face(window);
}

void tchat_::post_chating_show(twindow& window)
{
	input_ = NULL;
	send_ = NULL;

	current_chat_to_ = NULL;
}

void tchat_::process_message(const config &data, bool whisper)
{
	std::string sender = data["sender"].str();
	const std::string& message = data["message"].str();
	
	std::pair<std::vector<tcookie>*, tcookie* > cookie = contact_find(sender);
	if (whisper) {
		add_chat_log(cookie.second->uid, message, true);
		if (cookie.second == current_chat_to_) {
			chat_2_scroll_label(*history_, cookie.second->uid, cookie.second->username);

		} else {
			cookie.second->unread ++;
			update_node_internal(*cookie.first, *cookie.second);
		}
	}
}

void tchat_::process_userlist(const config& gamelist)
{
	// second: true(game), false(lobby)
	std::map<std::string, bool> users;
	BOOST_FOREACH (const config& c, gamelist.child_range("user")) {
		int game_id = c["game_id"].to_int();
		// game_id == 0 ? LOBBY : GAME;
		users.insert(std::make_pair(c["name"].str(), !!game_id));
	}
	for (std::map<int, std::vector<tcookie> >::iterator it = cookies_.begin(); it != cookies_.end(); ++ it) {
		for (std::vector<tcookie>::iterator it2 = it->second.begin(); it2 != it->second.end(); ++ it2) {
			tcookie& cookie = *it2;
			if (cookie.is_chapter()) {
				continue;
			}
			std::map<std::string, bool>::iterator find_it = users.find(cookie.username);
			if (find_it != users.end()) {
				cookie.online = true;
				cookie.game = find_it->second;
				users.erase(find_it);
			} else {
				cookie.online = false;
				cookie.game = false;
			}
			update_node_internal(it->second, cookie);
			if (&cookie == current_chat_to_) {
				find_widget<tbutton>(window_, ctrlid_send, false).set_active(lobby.ready() && current_chat_to_->online);
			}
		}
	}

	for (std::map<int, std::vector<tcookie> >::iterator it = cookies_.begin(); it != cookies_.end(); ++ it) {
		update_node_internal(it->second, it->second.front());
	}

	gamelist_ = gamelist;
}

void tchat_::process_userlist_diff(const config& data)
{
	try {
		// gamelist_.apply_diff(data, true);
		// gamelist_.clear_diff_track(data);
		gamelist_.apply_diff(data);
	} catch(config::error& e) {
		std::stringstream err;
		err << "Error while applying the gamelist diff: '" << e.message << "' Getting a new gamelist.\n";
		VALIDATE(false, err.str());
		network::send_data(config("refresh_lobby"), 0);
		return;
	}

	process_userlist(gamelist_);
}

void tchat_::process_network_status(bool connected)
{
	if (connected) {
		network::send_data(config("refresh_lobby"), 0);
	}
}

std::string tchat_::format_log_str(int uid, const std::string& username, std::vector<tintegrate::tlocator>& locator) const
{
	std::pair<const std::vector<tcookie>*, const tcookie*> cookie = contact_find(username);
	const std::vector<tchat_log>& log_msg = chat_logs.find(uid)->second;
	std::stringstream ss;
	int current_uid = HTTP_INVALID_UID;
	int start;
	const config& bubble_cfg = settings::bubbles.find("default")->second;
	for (std::vector<tchat_log>::const_iterator it = log_msg.begin(); it != log_msg.end(); ++ it) {
		const tchat_log& log = *it;
		if (it != log_msg.begin()) {
			ss << "\n";
			locator.push_back(tintegrate::tlocator(bubble_cfg, start, ss.str().size()));
			ss << "\n";
		}
		if (log.other) {
			ss << tintegrate::generate_format(username, "green");
			current_uid = uid;
		} else {
			ss << tintegrate::generate_format(group_.leader().name(), "yellow");
			current_uid = group_.leader().uid();
		}
		ss << "    " << format_time_date(log.t) << "\n";
		start = ss.str().size();
		ss << log.msg;
	}
	if (current_uid != HTTP_INVALID_UID) {
		locator.push_back(tintegrate::tlocator(bubble_cfg, start));
	}
	return ss.str();
}

void tchat_::chat_2_scroll_label(tscroll_label& label, int uid, const std::string& username)
{
	std::vector<tintegrate::tlocator> locator;
	std::string str = format_log_str(uid, username, locator);

	tlabel* widget = find_widget<tlabel>(label.content_grid(), "_label", false, true);

	label.set_label(str);
	widget->refresh_locator_anim(locator);
}

void tchat_::swap_page(twindow& window, int page, bool swap)
{
	if (page < min_page_ || !page_panel_) {
		return;
	}
	int index = page - min_page_;

	if (window.alternate_index() == index) {
		// desired page is the displaying page, do nothing.
		return;
	}

	if (current_page_ == chat_page_) {
		tchat_::post_show(window);
	} else if (current_page_ == chating_page_) {
		tchat_::post_chating_show(window);
	} else {
		desire_swap_page(window, current_page_, false);
	}

	window.alternate_uh(page_panel_, index);
	window.alternate_bh(swap? page_panel_: NULL, index);

	if (page == chat_page_) {
		tchat_::pre_show(window);
		network::send_data(config("refresh_lobby"), 0);

	} else if (page == chating_page_) {
		pre_chating_show(window);

	} else {
		desire_swap_page(window, page, true);
	}

	current_page_ = page;
}

void tchat_::swap_page2(bool& handled, bool& halt, int page)
{
	twindow& window = *window_;

	if (page == chating_page_) {
		if (current_chat_to_) {
			swap_page_chat_to_ = *current_chat_to_;
		} else {
			swap_page_chat_to_.uid = HTTP_INVALID_UID;
		}
	}
	src_pos_ = input_tb_->get_src_pos();
	swap_page_input_ = input_->get_value();

	swap_page(window, page, true);

	if (page == chat_page_) {
		keyboard_hidden(window);
#if (defined(__APPLE__) && TARGET_OS_IPHONE) || defined(ANDROID)
		SDL_StopTextInput();
#endif
	} else if (page == chating_page_) {
		keyboard_shown(window);
	}
	handled = true;
	halt = true;
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

	update_network_status(window, lobby.ready());
	join();
}

void tchat2::keyboard_shown(twindow& window)
{
	tchat_::keyboard_shown(window);
	find_widget<ttoggle_button>(&window, "chat", false, true)->set_visible(twidget::INVISIBLE);
}

void tchat2::keyboard_hidden(twindow& window)
{
	tchat_::keyboard_hidden(window);
	find_widget<ttoggle_button>(&window, "chat", false, true)->set_visible(twidget::VISIBLE);
}

bool tchat2::handle(tlobby::ttype type, const config& data)
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
