/* $Id: multiplayer.cpp 47847 2010-12-05 21:12:23Z shadowmaster $ */
/*
   Copyright (C) 2007 - 2010
   Part of the Battle for Wesnoth Project http://www.wesnoth.org

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "global.hpp"

#include "dialogs.hpp"
#include "gettext.hpp"
#include "game_preferences.hpp"
#include "log.hpp"
#include "gui/dialogs/lobby_main.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/mp_connect.hpp"
#include "gui/dialogs/mp_create_game.hpp"
#include "gui/dialogs/mp_side_creator.hpp"
#include "gui/dialogs/mp_side_wait.hpp"
#include "gui/dialogs/mp_login.hpp"
#include "gui/dialogs/preferences.hpp"
#include "gui/dialogs/transient_message.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "hash.hpp"
#include "multiplayer.hpp"
#include "multiplayer_error_codes.hpp"
#include "playmp_controller.hpp"
#include "playcampaign.hpp"
#include "formula_string_utils.hpp"
#include "sound.hpp"
#include "resources.hpp"
#include "filesystem.hpp"
#include "version.hpp"
#include "savegame.hpp"
#include "SDL_image.h"

#include <boost/foreach.hpp>
#include <boost/bind.hpp>
#include <iomanip>

static lg::log_domain log_network("network");
#define LOG_NW LOG_STREAM(info, log_network)

namespace network {

class http_data_lock
{
public:
	http_data_lock();
	~http_data_lock();
};

}

namespace {

class network_game_manager
{
public:
	// Add a constructor to avoid stupid warnings with some versions of GCC
	network_game_manager() {};

	~network_game_manager()
	{
		if(network::nconnections() > 0) {
			LOG_NW << "sending leave_game\n";
			config cfg;
			cfg.add_child("leave_game");
			network::send_data(cfg, 0);
			LOG_NW << "sent leave_game\n";
		}
	};
};

}

static void run_lobby_loop(display& disp, mp::ui& ui)
{
	disp.video().modeChanged();
	bool first = true;
	font::cache_mode(font::CACHE_LOBBY);
	while (ui.get_result() == mp::ui::CONTINUE) {
		if (disp.video().modeChanged() || first) {
			SDL_Rect lobby_pos = create_rect(0
					, 0
					, disp.video().getx()
					, disp.video().gety());
			ui.set_location(lobby_pos);
			first = false;
		}
		// process network data first so user actions can override the result
		// or uptodate data can prevent invalid actions
		// i.e. press cancel while you receive [start_game] or press start game while someone leaves
		ui.process_network();

		events::pump();
		events::raise_process_event();
		events::raise_draw_event();

		disp.flip();
		disp.delay(20);
	}
	font::cache_mode(font::CACHE_GAME);
}

namespace gui2 {
extern void check_response(network::connection res, const config& data);
}

namespace {

enum server_type {
	ABORT_SERVER,
	WESNOTHD_SERVER,
	SIMPLE_SERVER
};

}

static server_type open_connection(game_display& disp, const std::string& original_host)
{
	std::string h = original_host;

	if(h.empty()) {
		gui2::tmp_connect dlg;

		dlg.show(disp.video());
		if(dlg.get_retval() == gui2::twindow::OK) {
			h = preferences::network_host();
		} else {
			return ABORT_SERVER;
		}
	}

	network::connection sock;

	const int pos = h.find_first_of(":");
	std::string host;
	unsigned int port;

	if(pos == -1) {
		host = h;
		port = 15000;
	} else {
		host = h.substr(0, pos);
		port = lexical_cast_default<unsigned int>(h.substr(pos + 1), 15000);
	}

	// shown_hosts is used to prevent the client being locked in a redirect
	// loop.
	typedef std::pair<std::string, int> hostpair;
	std::set<hostpair> shown_hosts;
	shown_hosts.insert(hostpair(host, port));

	config data;
	sock = dialogs::network_connect_dialog(disp, "", host, port);

	do {

		if (!sock) {
			return ABORT_SERVER;
		}

		data.clear();

		network::connection data_res = dialogs::network_receive_dialog(disp, "", data);
		if (!data_res) {
			return ABORT_SERVER;
		}
		gui2::check_response(data_res, data);

		// Backwards-compatibility "version" attribute
		const std::string& version = data["version"];
		if(version.empty() == false && version != game_config::version) {
			utils::string_map i18n_symbols;
			i18n_symbols["version1"] = version;
			i18n_symbols["version2"] = game_config::version;
			const std::string errorstring = vgettext("The server requires version '$version1' while you are using version '$version2'", i18n_symbols);
			throw network::error(errorstring);
		}

		// Check for "redirect" messages
		if (const config &redirect = data.child("redirect"))
		{
			host = redirect["host"].str();
			port =redirect["port"].to_int(15000);

			if(shown_hosts.find(hostpair(host,port)) != shown_hosts.end()) {
				throw network::error(_("Server-side redirect loop"));
			}
			shown_hosts.insert(hostpair(host, port));

			if(network::nconnections() > 0)
				network::disconnect();
			sock = dialogs::network_connect_dialog(disp, "", host, port);
			continue;
		}

		if(data.child("version")) {
			config cfg;
			config res;
			// cfg["version"] = game_config::version;
			// fake version, in order to login in wesnoth server
			// cfg["version"] = "1.9.10";
			cfg["version"] = "test";
			res.add_child("version", cfg);
			network::send_data(res, 0);
		}

		//if we got a direction to login
		if(data.child("mustlogin")) {

			for(;;) {
				std::string password_reminder = "";

				std::string login = preferences::login();

				config response ;
				config &sp = response.add_child("login") ;
				sp["username"] = login ;

				// Login and enable selective pings -- saves server bandwidth
				// If ping_timeout has a non-zero value, do not enable
				// selective pings as this will cause clients to falsely
				// believe the server has died and disconnect.
				if (preferences::get_ping_timeout()) {
					// Pings required so disable selective pings
					sp["selective_ping"] = false;
				} else {
					// Client is bandwidth friendly so allow
					// server to optimize ping frequency as needed.
					sp["selective_ping"] = true;
				}
				network::send_data(response, 0);

				// Get response for our login request...
				// network::connection data_res = network::receive_data(data, 0, 3000);
				network::connection data_res = dialogs::network_receive_dialog(disp, "", data);
				if (!data_res) {
					throw network::error(_("Connection timed out"));
				}

				config *error = &data.child("error");

				// ... and get us out of here if the server did not complain
				if (!*error) break;

				do {
					std::string password = preferences::password();

					bool fall_through = (*error)["force_confirmation"].to_bool() ?
						(gui2::show_message(disp.video(), _("Confirm"), (*error)["message"], gui2::tmessage::ok_cancel_buttons) == gui2::twindow::CANCEL) :
						false;

					const bool is_pw_request = !((*error)["password_request"].empty()) && !(password.empty());

					// If the server asks for a password, provide one if we can
					// or request a password reminder.
					// Otherwise or if the user pressed 'cancel' in the confirmation dialog
					// above go directly to the username/password dialog
					if((is_pw_request || !password_reminder.empty()) && !fall_through) {
						if(is_pw_request) {
							if ((*error)["phpbb_encryption"].to_bool())
							{

								// Apparently HTML key-characters are passed to the hashing functions of phpbb in this escaped form.
								// I will do closer investigations on this, for now let's just hope these are all of them.

								// Note: we must obviously replace '&' first, I wasted some time before I figured that out... :)
								for(std::string::size_type pos = 0; (pos = password.find('&', pos)) != std::string::npos; ++pos )
									password.replace(pos, 1, "&amp;");
								for(std::string::size_type pos = 0; (pos = password.find('\"', pos)) != std::string::npos; ++pos )
									password.replace(pos, 1, "&quot;");
								for(std::string::size_type pos = 0; (pos = password.find('<', pos)) != std::string::npos; ++pos )
									password.replace(pos, 1, "&lt;");
								for(std::string::size_type pos = 0; (pos = password.find('>', pos)) != std::string::npos; ++pos )
									password.replace(pos, 1, "&gt;");

								const std::string salt = (*error)["salt"];

								if (salt.length() < 12) {
									throw network::error(_("Bad data received from server"));
								}

								sp["password"] = util::create_hash(util::create_hash(password, util::get_salt(salt),
										util::get_iteration_count(salt)), salt.substr(12, 8));

							} else {
								sp["password"] = password;
							}
						}

						sp["password_reminder"] = password_reminder;

						// Once again send our request...
						network::send_data(response, 0);

						network::connection data_res = network::receive_data(data, 0, 3000);
						if(!data_res) {
							throw network::error(_("Connection timed out"));
						}

						error = &data.child("error");

						// ... and get us out of here if the server is happy now
						if (!*error) break;


					}

					password_reminder = "";

					// Providing a password either was not attempted because we did not
					// have any or failed:
					// Now show a dialog that displays the error and allows to
					// enter a new user name and/or password

					std::string error_message;
					utils::string_map i18n_symbols;
					i18n_symbols["nick"] = login;

					if((*error)["error_code"] == MP_MUST_LOGIN) {
						error_message = _("You must login first.");
					} else if((*error)["error_code"] == MP_NAME_TAKEN_ERROR) {
						error_message = vgettext("The nick '$nick' is already taken.", i18n_symbols);
					} else if((*error)["error_code"] == MP_INVALID_CHARS_IN_NAME_ERROR) {
						error_message = vgettext("The nick '$nick' contains invalid "
								"characters. Only alpha-numeric characters, underscores and "
								"hyphens are allowed.", i18n_symbols);
					} else if((*error)["error_code"] == MP_NAME_TOO_LONG_ERROR) {
						error_message = vgettext("The nick '$nick' is too long. Nicks must "
								"be 20 characters or less.", i18n_symbols);
					} else if((*error)["error_code"] == MP_NAME_RESERVED_ERROR) {
						error_message = vgettext("The nick '$nick' is reserved and cannot be used by players.", i18n_symbols);
					} else if((*error)["error_code"] == MP_NAME_UNREGISTERED_ERROR) {
						error_message = vgettext("The nick '$nick' is not registered on this server.", i18n_symbols)
								+ _(" This server disallows unregistered nicks.");
					} else if((*error)["error_code"] == MP_PASSWORD_REQUEST) {
						error_message = vgettext("The nick '$nick' is registered on this server.", i18n_symbols);
					} else if((*error)["error_code"] == MP_PASSWORD_REQUEST_FOR_LOGGED_IN_NAME) {
						error_message = vgettext("The nick '$nick' is registered on this server.", i18n_symbols)
								+ "\n\n" + _("WARNING: There is already a client using this nick, "
								"logging in will cause that client to be kicked!");
					} else if((*error)["error_code"] == MP_NO_SEED_ERROR) {
						error_message = _("Error in the login procedure (the server had no "
								"seed for your connection).");
					} else if((*error)["error_code"] == MP_INCORRECT_PASSWORD_ERROR) {
						error_message = _("The password you provided was incorrect.");
					} else {
						error_message = (*error)["message"].str();
					}

					// gui2::tmp_login dlg(error_message, !((*error)["password_request"].empty()));
					gui2::tmp_login dlg(disp, error_message);
					dlg.show(disp.video());

					switch(dlg.get_retval()) {
						//Log in with password
						case gui2::twindow::OK:
							break;
						//Request a password reminder
						case 1:
							password_reminder = "yes";
							break;
						// Cancel
						default: return ABORT_SERVER;
					}

				// If we have got a new username we have to start all over again
				} while(login == preferences::login());

				// Somewhat hacky...
				// If we broke out of the do-while loop above error
				// is still going to be NULL
				if(!*error) break;
			} // end login loop
		}
	} while(!(data.child("join_lobby") || data.child("join_game")));

	if (h != preferences::server_list().front().address)
		preferences::set_network_host(h);

	if (data.child("join_lobby")) {
		return WESNOTHD_SERVER;
	} else {
		return SIMPLE_SERVER;
	}

}

namespace http {

int http_2_cfg(const std::vector<char>& http, config& cfg)
{
	std::string str;
	std::vector<std::string> res;
	int content_start = -1;

	std::vector<char>::const_iterator first;
	std::vector<char>::const_iterator end = http.end();
	std::vector<char>::const_iterator i = http.begin();
	while (i != http.end() && (*i == ' ' || *i == '\t' || *i == '\r' || *i == '\n')) {
		++ i;
	}
	first = i;
	
	cfg.clear();
	while (i != end) {
		if (*i == '\r') {
			++ i;
			if (i == end) { 
				break;
			}
			if (*i == '\n') {
				str.resize((i - 1) - first);
				std::copy(first, i - 1, str.begin());
				if (!str.empty()) {
					if (!cfg.has_attribute("__version")) {
						size_t pos = str.find("HTTP/");
						if (pos == 0) {
							std::vector<std::string> vstr = utils::split(str, ' ');
							if (vstr.size() >= 3) {
								cfg["__version"] = vstr[0];
								cfg["__status"] = lexical_cast<int>(vstr[1]);
								std::stringstream phrase;
								for (size_t t = 2; t < vstr.size(); t ++) {
									if (!phrase.str().empty()) {
										phrase << " ";
									}
									phrase << vstr[t];
								}
								cfg["__phrase"] = phrase.str();
							}
						}
					} else {
						size_t pos = str.find(":");
						if (pos != std::string::npos) {
							std::string key = str.substr(0, pos);
							std::string val = str.substr(pos + 1);
							cfg[utils::strip(key)] = utils::strip(val);
						}
					}
				} else {
					content_start = std::distance(http.begin(), i) + 1;
					break;
				}
				res.push_back(str);
				first = i + 1;
			}
		}
		++ i;
	}

	return content_start;
}

class http_agent
{
public:
	http_agent(game_display& disp);

	bool do_version(time_t time);
	bool do_register(bool check_exist);
	membership do_membership(const hero& h, bool quiet);
	bool do_avatar(const hero& h);
	bool do_uploadsave(const std::string& name);
	std::string do_downloadsave(int sid);
	std::vector<savegame::www_save_info> do_listsave();
	bool do_uploadpass(const pass_statistic& stat);
	std::vector<pass_statistic> do_listpass();
	std::vector<board_statistic> do_listboard(BOARD_TYPE type);
	std::pair<bool, int> do_renew(time_t time);

private:
	bool do_prepare(bool quiet = false);
	std::string form_url(const std::string& task);
	std::string form_request(const std::string& url, size_t content_length);
	bool access_denied(const config& cfg, const char* buf, int len) const;

private:
	game_display& disp_;
	std::string username_;
	std::string password_;
	network::connection sock_;

	std::string magic_;
};

http_agent::http_agent(game_display& disp)
	: disp_(disp)
	, magic_("5a7c")
	, username_(preferences::login())
	, password_(preferences::password())
{
}

bool http_agent::do_prepare(bool quiet)
{
	sock_ = dialogs::network_connect_dialog(disp_, "", game_config::bbs_server.host, game_config::bbs_server.port, quiet);

	return sock_;
}

std::string http_agent::form_url(const std::string& task)
{
	std::stringstream url;
	url << game_config::bbs_server.url << "/plugin.php?id=kingdom:kingdom&";
	url << "username=" << username_ << "&password=" << password_;
	url << "&do=" << task;

	return url.str();
}

std::string http_agent::form_request(const std::string& task, size_t content_length)
{
	std::stringstream request;
	// request << "GET /bbs HTTP/1.1\r\n";
	// request << "GET /bbs/forum.php HTTP/1.1\r\n";
	// request << "POST /bbs/forum.php HTTP/1.1\r\n";
	request << "POST " << form_url(task) << " HTTP/1.1\r\n";

	// request << "Accept: image/jpeg, application/x-ms-application, image/gif, application/xaml+xml, image/pjpeg, application/x-ms-xbap, application/vnd.ms-excel, application/vnd.ms-powerpoint, application/msword, */*\r\n";
	// request << "Accept-Language: zh-CN\r\n";
	// request << "Accept-Encoding: gzip, deflate\r\n";
	request << "Host: " << game_config::bbs_server.host << "\r\n";
	request << "Connection: Keep-Alive\r\n";
	request << "Content-Length: " << content_length << "\r\n";
	request << "\r\n";

	return request.str();
}

bool http_agent::access_denied(const config& cfg, const char* buf, int len) const
{
	static std::string access_denied = "Access denied";
	static std::string html_prefix = "<!DOCTYPE";
	static std::string misversion = "misversion";

	utils::string_map i18n_symbols;
	std::string errmsg;
	std::stringstream strstr;

	if (cfg["__status"].to_int() != 200) {
		strstr.str("");
		strstr << cfg["__version"].str() << " " << cfg["__status"].to_int() << " " << cfg["__phrase"].str() << "\n";
		i18n_symbols["response"] = help::tintegrate::generate_format(strstr.str(), "red");
		errmsg = vgettext("$response\n\nPlease check setting of BBS Server!", i18n_symbols);

	} else if (len >= (int)access_denied.size() && !memcmp(buf, access_denied.c_str(), access_denied.size())) {
		i18n_symbols["server"] = help::tintegrate::generate_format(game_config::bbs_server.name, "green");
		i18n_symbols["host"] = help::tintegrate::generate_format(game_config::bbs_server.host, "green");
		i18n_symbols["account"] = help::tintegrate::generate_format(dsgettext("wesnoth-lib", "Register, config account"), "yellow");
		errmsg = vgettext("Invalid access! Please check your $server login information. To set up: Press $account in \"Player profile\" Setting.", i18n_symbols);

	} else if (len >= (int)misversion.size() && !memcmp(buf, misversion.c_str(), misversion.size())) {
		i18n_symbols["server"] = help::tintegrate::generate_format(game_config::bbs_server.name, "green");
		errmsg = vgettext("Invalid access! pre_kingdom_version table on $server isn't ready.", i18n_symbols);

	} else if (len > (int)html_prefix.size() * 2 && !memcmp(buf, html_prefix.c_str(), html_prefix.size())) {
		return false;

	} else if (len < (int)magic_.size() || memcmp(buf, magic_.c_str(), magic_.size())) {
		errmsg = vgettext("Receive unkonwn data!\n\nPlease check if your version is the newest.", i18n_symbols);

	} 

	if (!errmsg.empty()) {
		gui2::show_message(disp_.video(), "", errmsg);
		return true;
	}

	return false;
}

bool http_agent::do_version(time_t time)
{
	std::pair<bool, int> ret = std::make_pair(false, 0);
	if (!do_prepare()) {
		return false;
	}

	std::stringstream content;
	content << "version=" << version_info(game_config::version).transfer_format().first;
	content << "&createtime=" << time;

	std::string str = content.str();
	
	std::stringstream request;
	request << form_request("version", content.str().size());
	request << content.str();
	
	dialogs::network_send_dialog(disp_, "", request.str().c_str(), request.str().size(), sock_);

	std::vector<char> buf;
	dialogs::network_receive_dialog(disp_, "", buf, sock_);

	config data;
	int content_start = http_2_cfg(buf, data);
	if (content_start != -1 && content_start < (int)buf.size()) {
		char* content = &buf[content_start];
		int content_length = (int)buf.size() - content_start;

		if (access_denied(data, content, content_length)) {
			return false;
		}
		content = content + magic_.size();
		content_length -= magic_.size();

		// buf maybe not end with '\0'
		std::string str;
		str.assign(content, content_length);

		const std::vector<std::string> fields = utils::split(str);
		if (fields.size() == 2) {
			return true;
		}
	}
	return false;
}

bool http_agent::do_register(bool check_exist)
{
	if (!do_prepare()) {
		return false;
	}

	std::stringstream content;
	
	std::stringstream request;
	request << form_request("register", content.str().size());
	request << content.str();
	
	dialogs::network_send_dialog(disp_, "", request.str().c_str(), request.str().size(), sock_);

	std::vector<char> buf;
	dialogs::network_receive_dialog(disp_, "", buf, sock_);

	bool ret = true;
	config data;
	int content_start = http_2_cfg(buf, data);

	if (content_start != -1 && content_start < (int)buf.size()) {
		char* content = &buf[content_start];
		int content_length = (int)buf.size() - content_start;

		utils::string_map i18n_symbols;
		i18n_symbols["server"] = help::tintegrate::generate_format(game_config::bbs_server.name, "green");

		if (access_denied(data, content, content_length)) {
			return false;
		}
		content = content + magic_.size();
		content_length -= magic_.size();

		static std::string success = "success";
		static std::string duplicate = "profile_username_duplicate";
		static std::string protect = "profile_username_protect";
		std::string message;

		if (content_length >= (int)success.size() && !memcmp(content, success.c_str(), success.size())) {
			if (!check_exist) {
				message = _("Register success!");
			}
		} else if (content_length >= (int)duplicate.size() && !memcmp(content, duplicate.c_str(), duplicate.size())) {
			if (!check_exist) {
				message = _("Username is duplicated!");
			}
		} else if (content_length >= (int)protect.size() && !memcmp(content, protect.c_str(), protect.size())) {
			message = _("Username is protected!");
			ret = false;
		} else {
			message = _("Unknown error!");
			ret = false;
		}
		if (!message.empty()) {
			gui2::show_message(disp_.video(), "", message);
		}
	}

	return ret;
}

membership http_agent::do_membership(const hero& h, bool quiet)
{
	membership member;
	if (!do_prepare(quiet)) {
		return member;
	}

	int value;
	std::stringstream content;
	value = fxptoi9(h.leadership_);
	content << "leadership=" << value << "&";
	value = fxptoi9(h.force_);
	content << "force1=" << value << "&";
	value = fxptoi9(h.intellect_);
	content << "intellect=" << value << "&";
	value = fxptoi9(h.politics_);
	content << "politics=" << value << "&";
	value = fxptoi9(h.charm_);
	content << "charm=" << value;
	
	std::stringstream request;
	request << form_request("membership", content.str().size());
	request << content.str();
	
	dialogs::network_send_dialog(disp_, "", request.str().c_str(), request.str().size(), sock_);

	std::vector<char> buf;
	dialogs::network_receive_dialog(disp_, "", buf, sock_);

	config data;
	int content_start = http_2_cfg(buf, data);

	if (content_start != -1 && content_start < (int)buf.size()) {
		char* content = &buf[content_start];
		int content_length = (int)buf.size() - content_start;

		if (access_denied(data, content, content_length)) {
			return member;
		}
		content = content + magic_.size();
		content_length -= magic_.size();

		// buf maybe not end with '\0'
		std::string str;
		str.assign(content, content_length);

		const std::vector<std::string> fields = utils::split(str);
		if (fields.size() == 3) {
			// sid, user, name, uploadtime
			member.vip = lexical_cast_default<int>(fields[0]);
			member.coin = lexical_cast_default<int>(fields[1]);
			member.score = lexical_cast_default<time_t>(fields[2]);
		}
	}

	return member;
}

bool http_agent::do_avatar(const hero& h)
{
	if (!do_prepare()) {
		return false;
	}

	std::stringstream content;

	std::stringstream request;
	request << form_request("avatar", content.str().size());
	request << content.str();
	
	dialogs::network_send_dialog(disp_, "", request.str().c_str(), request.str().size(), sock_);

	std::vector<char> buf;
	dialogs::network_receive_dialog(disp_, "", buf, sock_);

	config data;
	int content_start = http_2_cfg(buf, data);

	if (content_start != -1 && content_start < (int)buf.size()) {
		char* content = &buf[content_start];
		int content_length = (int)buf.size() - content_start;

		utils::string_map i18n_symbols;
		i18n_symbols["server"] = help::tintegrate::generate_format(game_config::bbs_server.name, "green");

		if (access_denied(data, content, content_length)) {
			return false;
		}
		content = content + magic_.size();
		content_length -= magic_.size();

		if (content_length <= 8) {
			i18n_symbols["middle"] = help::tintegrate::generate_format(_("middle avatar"), "green");
			i18n_symbols["small"] = help::tintegrate::generate_format(_("small avatar"), "green");
			gui2::show_message(disp_.video(), "", 
				vgettext("No avater on $server. You can upload $middle and $small to custom visualization.", i18n_symbols));
			return false;
		}
		std::string middle_size_str, small_size_str;
		middle_size_str.assign(content, 8);
		char* endptr;
		int middle_size = (int)strtol(middle_size_str.c_str(), &endptr, 16);
		if (content_length <= 8 + middle_size) {
			return false;
		}
		small_size_str.assign(content + 8 + middle_size, 8);
		int small_size = (int)strtol(small_size_str.c_str(), &endptr, 16);
		if (content_length != 8 + middle_size + 8 + small_size) {
			return false;
		}

		std::string name = "avatar_middle.png";
		std::string temp = get_addon_campaigns_dir() + "/__temp.png";
		if (middle_size) {
			write_file(temp, content + 8, middle_size, true);
			// don't use image::get_image, it has cache. Same name return first data until clear cache, 
			surface surf = IMG_Load(temp.c_str());
#ifdef _WIN32
			conv_ansi_utf8(temp, false);
#endif
			remove(temp.c_str());

			if (surf->w == 180 && surf->h == 256) {
				write_file(get_addon_campaigns_dir() + "/" + name, content + 8, middle_size, true);
			} else {
				i18n_symbols["name"] = help::tintegrate::generate_format(_("middle avatar"), "red");
				i18n_symbols["size"] = help::tintegrate::generate_format("180x256", "red");
				i18n_symbols["server"] = help::tintegrate::generate_format(game_config::bbs_server.name, "green");
				gui2::show_message(disp_.video(), "", vgettext("Download $name fail! $name format (size: $size, type: png), please check uploaded file on $server.", i18n_symbols));
			}
		}
		name = "avatar_small.png";
		temp = get_addon_campaigns_dir() + "/__temp.png";
		if (small_size) {
			write_file(temp, content + 8 + middle_size + 8, small_size, true);
			// don't use image::get_image, it has cache. Same name return first data until clear cache, 
			surface surf = IMG_Load(temp.c_str());
#ifdef _WIN32
			conv_ansi_utf8(temp, false);
#endif
			remove(temp.c_str());

			if (surf->w == 48 && surf->h == 60) {
				write_file(get_addon_campaigns_dir() + "/" + name, content + 8 + middle_size + 8, small_size, true);
			} else {
				i18n_symbols["name"] = help::tintegrate::generate_format(_("small avatar"), "red");
				i18n_symbols["size"] = help::tintegrate::generate_format("48x60", "red");
				i18n_symbols["server"] = help::tintegrate::generate_format(game_config::bbs_server.name, "green");
				gui2::show_message(disp_.video(), "", vgettext("Download $name fail! $name format (size: $size, type: png), please check uploaded file on $server.", i18n_symbols));
			}
		}
	}

	return true;
}

const std::string www_save_prefix = "WWW_";

bool http_agent::do_uploadsave(const std::string& name)
{
	if (!do_prepare()) {
		return false;
	}

	scoped_istream file_stream = istream_file(name, true);
	if (!file_stream->good()) {
		return false;
	}
	file_stream->seekg(0, std::ios_base::end);
	size_t filesize = file_stream->tellg();
	file_stream->seekg(0, std::ios_base::beg);
	if (!filesize) {
		return false;
	}
	
	std::string base_name = file_name(name);
	base_name = base_name.substr(0, base_name.rfind(".sav"));
	if (base_name.find(www_save_prefix) == 0) {
		base_name = base_name.substr(www_save_prefix.size());
	}
	size_t content_length = 8 + 8 + base_name.size() + filesize;

	std::stringstream request;
	request << form_request("uploadsave", content_length);

	char* request_data = (char*)malloc(request.str().size() + content_length);
	memcpy(request_data, request.str().c_str(), request.str().size());

	// SDLNet_Write32(version, request_data + request.str().size());
	std::stringstream strstr;
	std::string str = game_config::wesnoth_version.transfer_format().second;
	memcpy(request_data + request.str().size(), str.c_str(), 8);
	// SDLNet_Write32(base_name.size(), request_data + request.str().size() + 4);
	strstr.str("");
	strstr << std::setbase(16) << std::setfill('0') << std::setw(8) << base_name.size();
	str = strstr.str();
	memcpy(request_data + request.str().size() + 8, str.c_str(), 8);
	memcpy(request_data + request.str().size() + 16, base_name.c_str(), base_name.size());
	file_stream->read(request_data + request.str().size() + 16 + base_name.size(), filesize);
	
	dialogs::network_send_dialog(disp_, "", request_data,  request.str().size() + content_length, sock_);
	free(request_data);

	std::vector<char> buf;
	dialogs::network_receive_dialog(disp_, "", buf, sock_);

	config data;
	int content_start = http_2_cfg(buf, data);
	if (content_start != -1 && content_start < (int)buf.size()) {
		char* content = &buf[content_start];
		int content_length = (int)buf.size() - content_start;

		if (access_denied(data, content, content_length)) {
			return false;
		}
		content = content + magic_.size();
		content_length -= magic_.size();
	}

	return true;
}

std::string http_agent::do_downloadsave(int sid)
{
	if (!do_prepare()) {
		return null_str;
	}

	std::stringstream content;
	content << "sid=" << sid;

	std::stringstream request;
	request << form_request("downloadsave", content.str().size());
	request << content.str();
	
	dialogs::network_send_dialog(disp_, "", request.str().c_str(), request.str().size(), sock_);

	std::vector<char> buf;
	dialogs::network_receive_dialog(disp_, "", buf, sock_);

	config data;
	int content_start = http_2_cfg(buf, data);

	utils::string_map symbols;
	if (content_start != -1 && content_start < (int)buf.size()) {
		char* content = &buf[content_start];
		int content_length = (int)buf.size() - content_start;

		if (access_denied(data, content, content_length)) {
			return null_str;
		}
		content = content + magic_.size();
		content_length -= magic_.size();

		if (content_length <= 8) {
			return null_str;
		}
		std::string name_size_str;
		name_size_str.assign(content, 8);
		char* endptr;
		int name_size = (int)strtol(name_size_str.c_str(), &endptr, 16);
		if (content_length <= 8 + name_size) {
			return null_str;
		}
		std::string name;
		name.assign(content + 8, name_size);
		if (content_length == 8 + name_size) {
			std::stringstream err;
			symbols["file"] = help::tintegrate::generate_format(name, "red");
			err << vgettext("$file maybe deleted by server. Download fail!", symbols);
			gui2::show_message(disp_.video(), "", err.str());
			return null_str;
		}
		name = www_save_prefix + name + ".sav";

		write_file(get_saves_dir() + "/" + name, content + 8 + name_size, content_length - 8 - name_size, true);

		return name;
	}

	return null_str;
}

std::vector<savegame::www_save_info> http_agent::do_listsave()
{
	std::vector<savegame::www_save_info> list;
	if (!do_prepare()) {
		return list;
	}

	std::stringstream content;
	content << "version=" << game_config::wesnoth_version.transfer_format().second;

	std::stringstream request;
	request << form_request("listsave", content.str().size());
	request << content.str();
	
	dialogs::network_send_dialog(disp_, "", request.str().c_str(), request.str().size(), sock_);

	std::vector<char> buf;
	dialogs::network_receive_dialog(disp_, "", buf, sock_);

	config data;
	int content_start = http_2_cfg(buf, data);
	if (content_start != -1 && content_start < (int)buf.size()) {
		char* content = &buf[content_start];
		int content_length = (int)buf.size() - content_start;

		if (access_denied(data, content, content_length)) {
			return list;
		}
		content = content + magic_.size();
		content_length -= magic_.size();

		// buf maybe not end with '\0'
		std::string list_str;
		list_str.assign(content, content_length);

		const std::vector<std::string> saves = utils::split(list_str, '&');
		for (std::vector<std::string>::const_iterator it = saves.begin(); it != saves.end(); ++ it) {
			const std::vector<std::string> fields = utils::split(*it);
			if (fields.size() != 5) {
				continue;
			}
			// sid, user, name, uploadtime
			int sid = lexical_cast_default<int>(fields[0]);
			time_t uploadtime = lexical_cast_default<time_t>(fields[3]);
			list.push_back(savegame::www_save_info(sid, fields[1], fields[2], uploadtime));
		}
	}

	return list;
}

bool http_agent::do_uploadpass(const pass_statistic& stat)
{
	if (!do_prepare()) {
		return false;
	}

	size_t content_length = 56;

	std::stringstream request;
	request << form_request("uploadpass", content_length);

	char* request_data = (char*)malloc(request.str().size() + content_length);
	memcpy(request_data, request.str().c_str(), request.str().size());
	
	std::stringstream strstr;
	// createtime
	strstr.str("");
	strstr << std::setbase(16) << std::setfill('0') << std::setw(8) << stat.create_time;
	std::string str = strstr.str();
	memcpy(request_data + request.str().size(), str.c_str(), 8);
	// duration
	strstr.str("");
	strstr << std::setbase(16) << std::setfill('0') << std::setw(8) << stat.duration;
	str = strstr.str();
	memcpy(request_data + request.str().size() + 8, str.c_str(), 8);
	// version
	str = game_config::wesnoth_version.transfer_format().second;
	memcpy(request_data + request.str().size() + 16, str.c_str(), 8);
	// coin
	strstr.str("");
	strstr << std::setbase(16) << std::setfill('0') << std::setw(8) << stat.coin;
	str = strstr.str();
	memcpy(request_data + request.str().size() + 24, str.c_str(), 8);
	// score
	strstr.str("");
	strstr << std::setbase(16) << std::setfill('0') << std::setw(8) << stat.score;
	str = strstr.str();
	memcpy(request_data + request.str().size() + 32, str.c_str(), 8);
	// type
	strstr.str("");
	strstr << std::setbase(16) << std::setfill('0') << std::setw(8) << stat.type;
	str = strstr.str();
	memcpy(request_data + request.str().size() + 40, str.c_str(), 8);
	// hash
	strstr.str("");
	strstr << std::setbase(16) << std::setfill('0') << std::setw(8) << stat.hash;
	str = strstr.str();
	memcpy(request_data + request.str().size() + 48, str.c_str(), 8);
	
	dialogs::network_send_dialog(disp_, "", request_data,  request.str().size() + content_length, sock_);
	free(request_data);

	std::vector<char> buf;
	dialogs::network_receive_dialog(disp_, "", buf, sock_);

	static std::string duplicate = "duplicate";
	utils::string_map symbols;
	config data;
	int content_start = http_2_cfg(buf, data);

	if (content_start != -1 && content_start < (int)buf.size()) {
		char* content = &buf[content_start];
		int content_length = (int)buf.size() - content_start;

		if (access_denied(data, content, content_length)) {
			return false;
		}
		content = content + magic_.size();
		content_length -= magic_.size();
		if (content_length >= (int)duplicate.size() || !memcmp(content, duplicate.c_str(), duplicate.size())) {
			std::stringstream err;
			symbols["create"] = help::tintegrate::generate_format(format_time_date(stat.create_time), "red");
			err << vgettext("Had uploaded pass that create at $create, cannot upload again!", symbols);
			gui2::show_message(disp_.video(), "", err.str());
			return false;
		}
	}

	return true;
}

std::vector<pass_statistic> http_agent::do_listpass()
{
	std::vector<pass_statistic> list;
	if (!do_prepare()) {
		return list;
	}

	std::stringstream content;
	content << "version=" << game_config::wesnoth_version.transfer_format().second;

	std::stringstream request;
	request << form_request("listpass", content.str().size());
	request << content.str();
	
	dialogs::network_send_dialog(disp_, "", request.str().c_str(), request.str().size(), sock_);

	std::vector<char> buf;
	dialogs::network_receive_dialog(disp_, "", buf, sock_);

	config data;
	int content_start = http_2_cfg(buf, data);
	if (content_start != -1 && content_start < (int)buf.size()) {
		char* content = &buf[content_start];
		int content_length = (int)buf.size() - content_start;

		if (access_denied(data, content, content_length)) {
			return list;
		}
		content = content + magic_.size();
		content_length -= magic_.size();

		// buf maybe not end with '\0'
		std::string list_str;
		list_str.assign(content, content_length);

		pass_statistic pass;
		const std::vector<std::string> passes = utils::split(list_str, '&');
		for (std::vector<std::string>::const_iterator it = passes.begin(); it != passes.end(); ++ it) {
			const std::vector<std::string> fields = utils::split(*it);
			if (fields.size() != 8) {
				continue;
			}
			// pid, username, createtime, duration, score, coin, version, type
			pass.username = fields[1];
			pass.create_time = lexical_cast_default<time_t>(fields[2]);
			pass.duration = lexical_cast_default<int>(fields[3]);
			pass.coin = lexical_cast_default<int>(fields[4]);
			pass.score = lexical_cast_default<int>(fields[5]);
			pass.version = lexical_cast_default<int>(fields[6]);
			pass.type = lexical_cast_default<int>(fields[7]);
			list.push_back(pass);
		}
	}

	return list;
}

std::vector<board_statistic> http_agent::do_listboard(BOARD_TYPE type)
{
	std::vector<board_statistic> list;
	if (!do_prepare()) {
		return list;
	}

	std::stringstream content;
	content << "version=" << game_config::wesnoth_version.transfer_format().second;
	if (type == BOARD_SCORE) {
		content << "&filter=score";
	} else if (type == BOARD_PASS) {
		content << "&filter=pass";
	} else {
		return list;
	}

	std::stringstream request;
	request << form_request("listboard", content.str().size());
	request << content.str();
	
	dialogs::network_send_dialog(disp_, "", request.str().c_str(), request.str().size(), sock_);

	std::vector<char> buf;
	dialogs::network_receive_dialog(disp_, "", buf, sock_);

	config data;
	int content_start = http_2_cfg(buf, data);
	if (content_start != -1 && content_start < (int)buf.size()) {
		char* content = &buf[content_start];
		int content_length = (int)buf.size() - content_start;

		if (access_denied(data, content, content_length)) {
			return list;
		}
		content = content + magic_.size();
		content_length -= magic_.size();

		// buf maybe not end with '\0'
		std::string list_str;
		list_str.assign(content, content_length);

		board_statistic b;
		const std::vector<std::string> passes = utils::split(list_str, '&');
		for (std::vector<std::string>::const_iterator it = passes.begin(); it != passes.end(); ++ it) {
			const std::vector<std::string> fields = utils::split(*it);
			if (type == BOARD_SCORE) {
				if (fields.size() != 4) {
					continue;
				}
				// pid, username, createtime, duration, score, coin, version, type
				b.username = fields[0];
				b.score.vip = lexical_cast_default<int>(fields[1]);
				b.score.coin = lexical_cast_default<int>(fields[2]);
				b.score.score = lexical_cast_default<int>(fields[3]);

			} else if (type == BOARD_PASS) {
				if (fields.size() != 8) {
					continue;
				}
				// pid, username, createtime, duration, score, coin, version, type
				b.username = fields[1];
				b.pass.create_time = lexical_cast_default<time_t>(fields[2]);
				b.pass.duration = lexical_cast_default<int>(fields[3]);
				b.pass.coin = lexical_cast_default<int>(fields[4]);
				b.pass.score = lexical_cast_default<int>(fields[5]);
				b.pass.version = lexical_cast_default<int>(fields[6]);
				b.pass.type = lexical_cast_default<int>(fields[7]);
			}
			list.push_back(b);
		}
	}

	return list;
}

std::pair<bool, int> http_agent::do_renew(time_t time)
{
	std::pair<bool, int> ret = std::make_pair(false, 0);
	if (!do_prepare()) {
		return ret;
	}

	std::stringstream content;
	content << "renew=" << time;

	std::string str = content.str();
	
	std::stringstream request;
	request << form_request("renew", content.str().size());
	request << content.str();
	
	dialogs::network_send_dialog(disp_, "", request.str().c_str(), request.str().size(), sock_);

	std::vector<char> buf;
	dialogs::network_receive_dialog(disp_, "", buf, sock_);

	config data;
	int content_start = http_2_cfg(buf, data);
	if (content_start != -1 && content_start < (int)buf.size()) {
		char* content = &buf[content_start];
		int content_length = (int)buf.size() - content_start;

		if (access_denied(data, content, content_length)) {
			return ret;
		}
		content = content + magic_.size();
		content_length -= magic_.size();

		// buf maybe not end with '\0'
		std::string str;
		str.assign(content, content_length);

		const std::vector<std::string> fields = utils::split(str);
		if (fields.size() != 2) {
			return ret;
		}
		ret.second = lexical_cast_default<int>(fields[0]);
		time_t ret_time = lexical_cast_default<time_t>(fields[1]);
		ret.first = ret_time == time;
	}
	return ret;
}

bool version(game_display& disp, time_t time)
{
	network::http_data_lock data_lock;
	const network::manager net_manager(1, 1);

	http_agent agent(disp);
	return agent.do_version(time);
}

bool register_user(game_display& disp, bool check_exist)
{
	network::http_data_lock data_lock;
	const network::manager net_manager(1, 1);

	http_agent agent(disp);
	return agent.do_register(check_exist);
}

membership membership_hero(game_display& disp, const hero& h, bool quiet)
{
	network::http_data_lock data_lock;
	const network::manager net_manager(1, 1);

	http_agent agent(disp);
	return agent.do_membership(h, quiet);
}

bool avatar_hero(game_display& disp, const hero& h)
{
	network::http_data_lock data_lock;
	const network::manager net_manager(1, 1);

	http_agent agent(disp);
	return agent.do_avatar(h);
}

bool upload_save(game_display& disp, const std::string& name)
{
	network::http_data_lock data_lock;
	const network::manager net_manager(1, 1);

	http_agent agent(disp);
	return agent.do_uploadsave(name);
}

std::string download_save(game_display& disp, int sid)
{
	network::http_data_lock data_lock;
	const network::manager net_manager(1, 1);

	http_agent agent(disp);
	return agent.do_downloadsave(sid);
}

std::vector<savegame::www_save_info> list_save(game_display& disp)
{
	network::http_data_lock data_lock;
	const network::manager net_manager(1, 1);

	http_agent agent(disp);
	return agent.do_listsave();
}

bool upload_pass(game_display& disp, const pass_statistic& stat)
{
	network::http_data_lock data_lock;
	const network::manager net_manager(1, 1);

	http_agent agent(disp);
	return agent.do_uploadpass(stat);
}

std::vector<pass_statistic> list_pass(game_display& disp)
{
	network::http_data_lock data_lock;
	const network::manager net_manager(1, 1);

	http_agent agent(disp);
	return agent.do_listpass();
}

std::vector<board_statistic> list_board(game_display& disp, BOARD_TYPE type)
{
	network::http_data_lock data_lock;
	const network::manager net_manager(1, 1);

	http_agent agent(disp);
	return agent.do_listboard(type);
}

std::pair<bool, int> renew(game_display& disp, time_t time)
{
	network::http_data_lock data_lock;
	const network::manager net_manager(1, 1);

	http_agent agent(disp);
	return agent.do_renew(time);
}

}

// The multiplayer logic consists in 4 screens:
//
// lobby <-> create <-> connect <-> (game)
//       <------------> wait    <-> (game)
//
// To each of this screen corresponds a dialog, each dialog being defined in
// the multiplayer_(screen) file.
//
// The functions enter_(screen)_mode are simple functions that take care of
// creating the dialogs, then, according to the dialog result, of calling other
// of those screen functions.

static void enter_wait_mode(game_display& disp, const config& game_config, hero_map& heros, hero_map& heros_start, card_map& cards, config& gamelist, bool observe)
{
	mp::ui::result res;
	game_state state;
	network_game_manager m;

	gamelist.clear();
	statistics::fresh_stats();

	gui2::tmp_side_wait dlg(heros, heros_start, disp, *resources::game_map, game_config, gamelist, observe);
	dlg.show(disp.video());
	switch (dlg.get_legacy_result()) {
	case gui2::tmp_side_wait::PLAY:
		res = mp::ui::PLAY;
		dlg.start_game();
		state = dlg.get_state();
		// lobby may modify hero's side_feature
		// heros_start = heros;
		break;
	default:
		res = mp::ui::QUIT;
	}

	switch (res) {
	case mp::ui::PLAY:
		play_game(disp, state, game_config, heros, heros_start, cards, IO_CLIENT,
			preferences::skip_mp_replay() && observe);
		recorder.clear();

		break;
	case mp::ui::QUIT:
	default:
		break;
	}
}

static void enter_create_mode(game_display& disp, const config& game_config, hero_map& heros, hero_map& heros_start, card_map& cards, config& gamelist, mp::controller default_controller, bool local_players_only = false);

static void enter_connect_mode(game_display& disp, const config& game_config, hero_map& heros, hero_map& heros_start,
		card_map& cards,
		config& gamelist, const mp_game_settings& params,
		const int num_turns, mp::controller default_controller, bool local_players_only = false)
{
	mp::ui::result res;
	game_state state;
	const network::manager net_manager(1,1);
	network_game_manager m;

	gamelist.clear();
	statistics::fresh_stats();

	// if (gui2::new_widgets) {
		gui2::tmp_side_creator dlg(heros, heros_start, disp, *resources::game_map, game_config, gamelist, params, num_turns, default_controller, local_players_only);
		dlg.show(disp.video());

		switch (dlg.get_legacy_result()) {
		case gui2::tmp_side_creator::PLAY:
			res = mp::ui::PLAY;
			dlg.start_game();
			state = dlg.get_state();
			break;
		case gui2::tmp_side_creator::CREATE:
			res = mp::ui::CREATE;
			break;
		default:
			res = mp::ui::QUIT;
		}

	switch (res) {
	case mp::ui::PLAY:
		play_game(disp, state, game_config, heros, heros_start, cards, IO_SERVER);
		recorder.clear();

		break;
	case mp::ui::CREATE:
		enter_create_mode(disp, game_config, heros, heros_start, cards, gamelist, default_controller, local_players_only);
		break;
	case mp::ui::QUIT:
	default:
		network::send_data(config("refresh_lobby"), 0);
		break;
	}
}

static void enter_create_mode(game_display& disp, const config& game_config, hero_map& heros, hero_map& heros_start, 
		card_map& cards, config& gamelist, mp::controller default_controller, bool local_players_only)
{
	mp::ui::result res;
	mp_game_settings params;
	int num_turns;

	// if (gui2::new_widgets) {
		gui2::tmp_create_game dlg(disp, game_config);
		dlg.show(disp.video());
		int retval = dlg.get_retval();
		if (retval == gui2::twindow::OK) {
			res = mp::ui::CREATE;
		} else {
			res = mp::ui::QUIT;
		}
		params = dlg.get_parameters();
		num_turns = dlg.num_turns();

		// network::send_data(config("refresh_lobby"), 0);

	switch (res) {
	case mp::ui::CREATE:
		enter_connect_mode(disp, game_config, heros, heros_start, cards, gamelist, params, num_turns, default_controller, local_players_only);
		break;
	case mp::ui::QUIT:
	default:
		//update lobby content
		network::send_data(config("refresh_lobby"), 0);
		break;
	}
}

static void do_preferences_dialog(game_display& disp, const config& game_config)
{
	const preferences::display_manager disp_manager(&disp);
	gui2::show_preferences_dialog(disp);
	return;
}

static void enter_lobby_mode(game_display& disp, const config& game_config, hero_map& heros, hero_map& heros_start, 
		card_map& cards, config& gamelist)
{


	mp::ui::result res;

	while (true) {
		const config &cfg = game_config.child("lobby_music");
		if (cfg) {
			BOOST_FOREACH (const config &i, cfg.child_range("music")) {
				sound::play_music_config(i);
			}
			sound::commit_music_changes();
		} else {
			sound::empty_playlist();
			sound::stop_music();
		}
		lobby_info li(game_config);

		// Force a black background
		const Uint32 color = SDL_MapRGBA(disp.video().getSurface()->format
				, 0
				, 0
				, 0
				, 255);

		sdl_fill_rect(disp.video().getSurface(), NULL, color);

		{
			// load game will update heros, reset it.
			heros = heros_start;

			gui2::tlobby_main dlg(game_config, li, disp);
			dlg.set_preferences_callback(
				boost::bind(do_preferences_dialog,
					boost::ref(disp), boost::ref(game_config)));
			dlg.show(disp.video());
			//ugly kludge for launching other dialogs like the old lobby
			switch (dlg.get_legacy_result()) {
				case gui2::tlobby_main::CREATE:
					res = mp::ui::CREATE;
					break;
				case gui2::tlobby_main::JOIN:
					res = mp::ui::JOIN;
					break;
				case gui2::tlobby_main::OBSERVE:
					res = mp::ui::OBSERVE;
					break;
				default:
					res = mp::ui::QUIT;
			}
		}

		switch (res) {
		case mp::ui::JOIN:
			try {
				enter_wait_mode(disp, game_config, heros, heros_start, cards, gamelist, false);
			} catch(config::error& error) {
				if(!error.message.empty()) {
					gui2::show_error_message(disp.video(), error.message);
				}
				//update lobby content
				network::send_data(config("refresh_lobby"), 0);
			}
			break;
		case mp::ui::OBSERVE:
			try {
				enter_wait_mode(disp, game_config, heros, heros_start, cards, gamelist, true);
			} catch(config::error& error) {
				if(!error.message.empty()) {
					gui2::show_error_message(disp.video(), error.message);
				}
			}
			// update lobby content unconditionally because we might have left only after the
			// game ended in which case we ignored the gamelist and need to request it again
			network::send_data(config("refresh_lobby"), 0);
			break;
		case mp::ui::CREATE:
			try {
				enter_create_mode(disp, game_config, heros, heros_start, cards, gamelist, mp::CNTR_NETWORK);
			} catch(config::error& error) {
				if (!error.message.empty())
					gui2::show_error_message(disp.video(), error.message);
				//update lobby content
				network::send_data(config("refresh_lobby"), 0);
			}
			break;
		case mp::ui::QUIT:
			return;
		case mp::ui::PREFERENCES:
			{
				do_preferences_dialog(disp, game_config);
				//update lobby content
				network::send_data(config("refresh_lobby"), 0);
			}
			break;
		default:
			return;
		}
	}
}

namespace mp {

void start_local_game(game_display& disp, const config& game_config, hero_map& heros, hero_map& heros_start,
		card_map& cards, mp::controller default_controller)
{
	const rand_rng::set_random_generator generator_setter(&recorder);
	config gamelist;
	playmp_controller::set_replay_last_turn(0);
	preferences::set_message_private(false);
	enter_create_mode(disp, game_config, heros, heros_start, cards, gamelist, default_controller, true);
}

void start_client(game_display& disp, const config& game_config, hero_map& heros, hero_map& heros_start,
		card_map& cards, const std::string& host)
{
	const rand_rng::set_random_generator generator_setter(&recorder);
	const network::manager net_manager(1,1);

	config gamelist;
	server_type type = open_connection(disp, host);

	switch(type) {
	case WESNOTHD_SERVER:
		enter_lobby_mode(disp, game_config, heros, heros_start, cards, gamelist);
		break;
	case SIMPLE_SERVER:
		playmp_controller::set_replay_last_turn(0);
		preferences::set_message_private(false);
		enter_wait_mode(disp, game_config, heros, heros_start, cards, gamelist, false);
		break;
	case ABORT_SERVER:
		break;
	}
}

}

