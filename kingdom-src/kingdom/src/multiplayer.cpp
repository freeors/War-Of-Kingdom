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
#include "preferences_display.hpp"
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
#include "savegame.hpp"
#include "version.hpp"
#include "SDL_image.h"
#include "loadscreen.hpp"
#include "sha1.hpp"
#include "filesystem.hpp"

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

namespace {

enum server_type {
	ABORT_SERVER,
	WESNOTHD_SERVER,
	SIMPLE_SERVER
};

}

static server_type open_connection(display& disp, hero_map& heros, const std::string& original_host)
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
	sock = dialogs::network_connect_dialog(disp, "", host, port, false);

	do {

		if (!sock) {
			return ABORT_SERVER;
		}

		data.clear();

		network::connection data_res = dialogs::network_receive_dialog(disp, "", data, 0, 0);
		if (!data_res) {
			return ABORT_SERVER;
		}
		mp::check_response(data_res, data);

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
			sock = dialogs::network_connect_dialog(disp, "", host, port, false);
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
				// if (preferences::get_ping_timeout()) {
				if (false) {
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
				network::connection data_res = dialogs::network_receive_dialog(disp, "", data, 0, 0);
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
					gui2::tmp_login dlg(disp, heros, error_message);
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

bool commerce_protection(display& disp, const std::string& function)
{
	bool enable = true;

	if (!enable) return false;

	std::stringstream strstr;
	utils::string_map symbols;
	symbols["function"] = tintegrate::generate_format(function, "yellow");

	strstr << vgettext("wesnoth-lib", "Because of product commercialization, $function does not open source code.", symbols);
	strstr << "\n\n";
	const std::string postscript = dsgettext("wesnoth-lib", "Please do not use self-compile project to commerce, for example upload data to Data Server.");
	strstr << tintegrate::generate_format(postscript, "red", 0, true);

	gui2::show_message(disp.video(), "", strstr.str());
	return true;
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

class trequest;

class http_agent
{
public:
	http_agent(display& disp, hero_map& heros, bool check_server_match = true);
	~http_agent();

	network::connection connection() const { return sock_; }

	bool do_register(bool check_exist);
	membership do_session(bool quiet);
	membership do_membership(bool quiet, int uid, const std::string& username);
	std::vector<membership> do_membershiplist_vector(bool quiet, const std::string& request_str, bool uid);
	std::map<int, membership> do_membershiplist_map(bool quiet, const std::string& request_str, bool uid);
	bool do_avatar(int uid, bool quiet);
	bool do_uploadsave(const std::string& name);
	std::string do_downloadsave(int sid);
	std::vector<savegame::www_save_info> do_listsave();
	membership do_uploadpass(const pass_statistic& stat);
	std::vector<pass_statistic> do_listpass();
	membership do_uploadsiege(const tsiege_record& rec);
	std::vector<tsiege_record> do_listsiege();
	std::map<std::string, tsubcontinent_record> do_listsubcontinent();
	std::vector<ttitle_record> do_listtitle();
	membership do_uploadmessage(const tmessage_record& rec);
	std::vector<tmessage_record> do_listmessage();
	std::vector<pass_statistic> do_listboard_pass();
	std::vector<membership> do_listboard_score();
	membership do_upgrade(int number, int coin, int score);
	membership do_member(int op, const std::vector<std::pair<int, int> >& member, int coin, int score);
	membership do_exile(int tag, const std::vector<std::pair<int, int> >& exile, int coin, int score);
	membership do_score(int coin, int score, bool transaction);
	membership upload_data(const std::map<int, std::string>& block, bool quiet, int uid);

	membership insert_transaction(const std::string& number, const std::string& buyer, int request_uid, int type);
	membership do_associate(const std::string& username, int tag, bool check_network);

	membership do_employee_insert(hero_map& heros);
	membership do_employee_common(int tag, const std::set<int>& number, int coin, int score, std::map<int, temployee>* employees);
	std::map<int, temployee> do_list_employee();

	membership do_signin(int tag);
	std::pair<tsubcontinent_record, membership> do_subcontinent(int tag, const std::string& id, int cityno);
private:
	bool do_prepare(bool check_network, bool quiet = false);
	std::string form_url(const std::string& task) const;
	std::string form_request(const std::string& url, size_t content_length);
	bool access_denied(const config& cfg, const char* buf, int len) const;

	membership parse_membership_result(const std::string& result, bool enhance = false) const;
	std::vector<membership> parse_membershiplist_str_2_vector(const std::string& str) const;
	std::map<int, membership> parse_membershiplist_str_2_map(const std::string& str) const;

	std::string do_membershiplist_core(bool quiet, const std::string& request_str, bool uid);

	std::map<int, temployee> parse_employeelist_str_2_map(const std::string& str) const;
	tsubcontinent_record parse_subcontinent_result(const std::string& str) const;

	friend class trequest;
private:
	unsigned short secret_key_;
	display& disp_;
	hero_map& heros_;
	bool check_server_match_;
	std::string username_;
	std::string password_;
	network::connection sock_;

	std::string magic_;
};

http_agent::http_agent(display& disp, hero_map& heros, bool check_server_match)
	: disp_(disp)
	, heros_(heros)
	, check_server_match_(check_server_match)
	, magic_("5a7c")
	, sock_(network::null_connection)
	, username_(preferences::login())
	, password_(preferences::password())
{
}

http_agent::~http_agent() 
{
	if (sock_ != network::null_connection) {
		network::disconnect(sock_); 
	}
}

bool http_agent::do_prepare(bool check_network, bool quiet)
{
	if (!game_config::server_matched) {
		std::string errmsg = dsgettext("wesnoth-lib", "Invalid access! You modified resources files, or check if your version is the newest.");
		gui2::show_message(disp_.video(), "", errmsg);
		return false;
	}

	if (check_network && game_config::local_only) {
		membership m = do_session(quiet);
		if (m.uid < 0) {
			return false;
		}
		if (group.valid()) {
			group.from_local_membership(disp_, heros_, m, true);
			game_config::local_only = false;
		} else {
			game_config::timestamp = m.timestamp;
		}
	}
	sock_ = dialogs::network_connect_dialog(disp_, "", game_config::bbs_server.host, game_config::bbs_server.port, true, quiet);

	return sock_;
}

std::string http_agent::form_url(const std::string& task) const
{
	std::stringstream url;
	url << game_config::bbs_server.url << "/plugin.php?id=kingdom:kingdom&";
	url << "username=" << username_ << "&checksum=" << game_config::checksum;
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

class trequest
{
public:
	trequest(const http_agent& agent, const std::string& task, const std::string& context, const char* appendix = NULL, size_t appendix_length = 0);
	~trequest()
	{
		if (buf) {
			free(buf);
		}
	}

	char* buf;
	size_t size;
};

trequest::trequest(const http_agent& agent, const std::string& task, const std::string& context, const char* appendix, size_t appendix_length)
	: buf(NULL)
	, size(0)
{
	std::stringstream plain;
	plain << std::setbase(16) << std::setfill('0') << std::setw(2) << agent.password_.size() << agent.password_;
	size_t timestamp = 0;
	if (task != "membership") {
		timestamp = ++ game_config::timestamp;
	}
	plain << std::setbase(16) << std::setfill('0') << std::setw(8) << timestamp;
	plain << context;
	unsigned char checksum = 0;
	for (size_t n = 0; n < plain.str().size(); n ++) {
		checksum ^= plain.str().c_str()[n];
	}
	int v = checksum;
	plain << std::setbase(16) << std::setfill('0') << std::setw(2) << v;

	tsaes_encrypt encrypt(plain.str().c_str(), plain.str().size(), game_config::secret_key);

	std::stringstream request;
	// request << "GET /bbs HTTP/1.1\r\n";
	// request << "GET /bbs/forum.php HTTP/1.1\r\n";
	// request << "POST /bbs/forum.php HTTP/1.1\r\n";
	request << "POST " << agent.form_url(task) << " HTTP/1.1\r\n";

	// request << "Accept: image/jpeg, application/x-ms-application, image/gif, application/xaml+xml, image/pjpeg, application/x-ms-xbap, application/vnd.ms-excel, application/vnd.ms-powerpoint, application/msword, */*\r\n";
	// request << "Accept-Language: zh-CN\r\n";
	// request << "Accept-Encoding: gzip, deflate\r\n";
	request << "Host: " << game_config::bbs_server.host << "\r\n";
	request << "Connection: Keep-Alive\r\n";
	request << "Content-Length: " << (8 + encrypt.size + appendix_length) << "\r\n";
	request << "\r\n";

	request << std::setbase(16) << std::setfill('0') << std::setw(8) << encrypt.size;

	size = request.str().size() + encrypt.size + appendix_length;
	buf = (char*)malloc(size);
	memcpy(buf, request.str().c_str(), request.str().size());
	memcpy(buf + request.str().size(), encrypt.buf, encrypt.size);
	if (appendix_length) {
		memcpy(buf + request.str().size() + encrypt.size, appendix, appendix_length);
	}
}

bool http_agent::access_denied(const config& cfg, const char* buf, int len) const
{
	static std::string access_denied = "Access denied";
	static std::string html_prefix = "<!DOCTYPE";
	static std::string version_absent = "version_absent";
	static std::string error_checksum = "error_checksum";
	static std::string error_security = "error_security";
	static std::string error_secretkey = "error_secretkey";
	static std::string error_timestamp = "error_timestamp";

	utils::string_map i18n_symbols;
	std::string errmsg;
	std::stringstream strstr;

	if (cfg["__status"].to_int() != 200) {
		strstr.str("");
		strstr << cfg["__version"].str() << " " << cfg["__status"].to_int() << " " << cfg["__phrase"].str() << "\n";
		i18n_symbols["response"] = tintegrate::generate_format(strstr.str(), "red");
		errmsg = vgettext("$response\n\nPlease check setting of BBS Server!", i18n_symbols);

	} else if (len >= (int)access_denied.size() && !memcmp(buf, access_denied.c_str(), access_denied.size())) {
		i18n_symbols["server"] = tintegrate::generate_format(game_config::bbs_server.name, "green");
		i18n_symbols["host"] = tintegrate::generate_format(game_config::bbs_server.host, "green");
		i18n_symbols["account"] = tintegrate::generate_format(dsgettext("wesnoth-lib", "Register, config account"), "yellow");
		errmsg = vgettext("wesnoth-lib", "Invalid access! Please check your $server login information. To set up: Press $account in \"Player profile\" Setting.", i18n_symbols);

	} else if (len >= (int)version_absent.size() && !memcmp(buf, version_absent.c_str(), version_absent.size())) {
		i18n_symbols["server"] = tintegrate::generate_format(game_config::bbs_server.name, "green");
		errmsg = vgettext("wesnoth-lib", "Invalid access! pre_kingdom_version table on $server isn't ready.", i18n_symbols);

	} else if (len >= (int)error_checksum.size() && !memcmp(buf, error_checksum.c_str(), error_checksum.size())) {
		game_config::server_matched = false;
		i18n_symbols["server"] = tintegrate::generate_format(game_config::bbs_server.name, "green");
		errmsg = vgettext("wesnoth-lib", "Invalid access! You modified resources files, or check if your version is the newest.", i18n_symbols);

	} else if (len >= (int)error_security.size() && !memcmp(buf, error_security.c_str(), error_security.size())) {
		i18n_symbols["server"] = tintegrate::generate_format(game_config::bbs_server.name, "green");
		errmsg = vgettext("wesnoth-lib", "Invalid access! Security error, someone maybe attack cipher.", i18n_symbols);

	} else if (len >= (int)error_secretkey.size() && !memcmp(buf, error_secretkey.c_str(), error_secretkey.size())) {
		i18n_symbols["server"] = tintegrate::generate_format(game_config::bbs_server.name, "green");
		errmsg = vgettext("wesnoth-lib", "Invalid access! Secret key error, Secret key on $server isn't ready.", i18n_symbols);

	} else if (len >= (int)error_timestamp.size() && !memcmp(buf, error_timestamp.c_str(), error_timestamp.size())) {
		i18n_symbols["server"] = tintegrate::generate_format(game_config::bbs_server.name, "green");
		errmsg = vgettext("wesnoth-lib", "Invalid access! Timestamp error. Check whether login a username on more device at same time, or restart game.", i18n_symbols);

	} else if (len > (int)html_prefix.size() * 2 && !memcmp(buf, html_prefix.c_str(), html_prefix.size())) {
		errmsg = vgettext("wesnoth-lib", "Send a command that server doesn't support!\n\nPlease check if your version is the newest.", i18n_symbols);

	} else if (len < (int)magic_.size() || memcmp(buf, magic_.c_str(), magic_.size())) {
		errmsg = vgettext("wesnoth-lib", "Receive unkonwn data!\n\nPlease check if your version is the newest.", i18n_symbols);

	} 

	if (!errmsg.empty()) {
		gui2::show_message(disp_.video(), "", errmsg);
		return true;
	}

	return false;
}

membership http_agent::parse_membership_result(const std::string& result, bool enhance) const
{
	membership member;
	const std::vector<std::string> fields = utils::split(result, '&', utils::STRIP_SPACES);
	if (enhance) {
		if (fields.size() == 23) {
			member.uid = lexical_cast_default<int>(fields[0]);
			member.vip = lexical_cast_default<int>(fields[1]);
			member.name = fields[2];
			member.expire = lexical_cast_default<time_t>(fields[3]);
			member.noble = lexical_cast_default<int>(fields[4]);
			member.member = fields[5];
			member.exile = fields[6];
			member.associate = fields[7];
			member.field = fields[8];
			member.translatable = fields[9];
			member.city = fields[10];
			member.interior = fields[11];
			member.signin = fields[12];
			member.coin = lexical_cast_default<int>(fields[13]);
			member.score = lexical_cast_default<int>(fields[14]);
			member.credit = lexical_cast_default<int>(fields[15]);
			member.layout = fields[16];
			member.map = fields[17];
			member.message = fields[18];
			member.title = fields[19];
			member.timestamp = lexical_cast_default<size_t>(fields[20]);
			member.tax = lexical_cast_default<int>(fields[21]);
			member.version = lexical_cast_default<size_t>(fields[22]);
		}
	} else {
		if (fields.size() == 16) {
			member.uid = lexical_cast_default<int>(fields[0]);
			member.vip = lexical_cast_default<int>(fields[1]);
			member.name = fields[2];
			member.expire = lexical_cast_default<time_t>(fields[3]);
			member.noble = lexical_cast_default<int>(fields[4]);
			member.member = fields[5];
			member.exile = fields[6];
			member.associate = fields[7];
			member.field = fields[8];
			member.translatable = fields[9];
			member.city = fields[10];
			member.interior = fields[11];
			member.signin = fields[12];
			member.coin = lexical_cast_default<int>(fields[13]);
			member.score = lexical_cast_default<int>(fields[14]);
			member.credit = lexical_cast_default<int>(fields[15]);
		}
	}
	return member;
}

bool http_agent::do_register(bool check_exist)
{
	bool ret = false;
	if (commerce_protection(disp_, "http_agent::do_register()")) {
		return ret;
	}

	return ret;
}

membership http_agent::do_session(bool quiet)
{
	membership member;
	if (!do_prepare(false, quiet)) {
		return member;
	}

	std::stringstream content;
	content << "version=" << version_info(game_config::version).transfer_format().first;
	content << "&checksum=" << game_config::checksum;

	trequest request(*this, "session", content.str());
	dialogs::network_send_dialog(disp_, "", request.buf, request.size, sock_);

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

		member = parse_membership_result(str, true);
		if (member.uid < 0 && !quiet) {
			gui2::show_message(disp_.video(), "", dsgettext("wesnoth-lib", "Receive unkonwn data!\n\nPlease check if your version is the newest."));
		}
	}
	return member;
}

membership http_agent::do_membership(bool quiet, int uid, const std::string& username)
{
	membership member;
	if (!do_prepare(false, quiet)) {
		return member;
	}

	std::stringstream content;
	if (uid >= 0) {
		content << "uid=" << uid;
	} else if (!username.empty()) {
		content << "username=" << username;
	}
	
	trequest request(*this, "membership", content.str());
	dialogs::network_send_dialog(disp_, "", request.buf, request.size, sock_);

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

		member = parse_membership_result(str, true);
		if (member.uid < 0 && !quiet) {
			gui2::show_message(disp_.video(), "", dsgettext("wesnoth-lib", "Receive unkonwn data!\n\nPlease check if your version is the newest."));
		}
	}

	return member;
}

std::string http_agent::do_membershiplist_core(bool quiet, const std::string& request_str, bool uid)
{
	if (request_str.empty() || !do_prepare(true, quiet)) {
		return null_str;
	}

	std::stringstream content;
	if (uid) {
		content << "__uid,";
	}
	content << request_str;
		
	trequest request(*this, "membershiplist", content.str());
	dialogs::network_send_dialog(disp_, "", request.buf, request.size, sock_);

	std::vector<char> buf;
	dialogs::network_receive_dialog(disp_, "", buf, sock_);

	config data;
	int content_start = http_2_cfg(buf, data);

	if (content_start != -1 && content_start < (int)buf.size()) {
		char* content = &buf[content_start];
		int content_length = (int)buf.size() - content_start;

		if (access_denied(data, content, content_length)) {
			return null_str;
		}
		content = content + magic_.size();
		content_length -= magic_.size();

		// buf maybe not end with '\0'
		std::string str;
		str.assign(content, content_length);

		return str;
	}

	return null_str;
}

std::vector<membership> http_agent::parse_membershiplist_str_2_vector(const std::string& str) const
{
	std::vector<membership> members;
	const std::vector<std::string> vstr = utils::split(str, '#', utils::STRIP_SPACES);
	membership m;
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		m = parse_membership_result(*it);
		if (m.uid < 0) {
			members.clear();
			return members;
		}
		members.push_back(m);
	}

	return members;
}

std::vector<membership> http_agent::do_membershiplist_vector(bool quiet, const std::string& request_str, bool uid)
{
	return parse_membershiplist_str_2_vector(do_membershiplist_core(quiet, request_str, uid));
}

std::map<int, membership> http_agent::parse_membershiplist_str_2_map(const std::string& str) const
{
	std::map<int, membership> members;
	const std::vector<std::string> vstr = utils::split(str, '#', utils::STRIP_SPACES);
	membership m;
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		m = parse_membership_result(*it);
		if (m.uid < 0) {
			members.clear();
			return members;
		}
		members.insert(std::make_pair(m.uid, m));
	}

	return members;
}

std::map<int, membership> http_agent::do_membershiplist_map(bool quiet, const std::string& request_str, bool uid)
{
	return parse_membershiplist_str_2_map(do_membershiplist_core(quiet, request_str, uid));
}

bool http_agent::do_avatar(int uid, bool quiet)
{
	if (!do_prepare(true, quiet)) {
		return false;
	}

	std::stringstream content;
	if (uid >= 0) {
		content << "uid=" << uid;
	}

	trequest request(*this, "avatar", content.str());
	dialogs::network_send_dialog(disp_, "", request.buf, request.size, sock_);

	std::vector<char> buf;
	dialogs::network_receive_dialog(disp_, "", buf, sock_);

	config data;
	int content_start = http_2_cfg(buf, data);

	if (content_start != -1 && content_start < (int)buf.size()) {
		char* content = &buf[content_start];
		int content_length = (int)buf.size() - content_start;

		utils::string_map i18n_symbols;
		i18n_symbols["server"] = tintegrate::generate_format(game_config::bbs_server.name, "green");

		if (access_denied(data, content, content_length)) {
			return false;
		}
		content = content + magic_.size();
		content_length -= magic_.size();

		if (content_length <= 8) {
			i18n_symbols["middle"] = tintegrate::generate_format(_("middle avatar"), "green");
			i18n_symbols["small"] = tintegrate::generate_format(_("small avatar"), "green");
			if (!quiet) {
				gui2::show_message(disp_.video(), "", 
					vgettext("No avater on $server. You can upload $middle and $small to custom visualization.", i18n_symbols));
			}
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

		std::stringstream strstr;
		strstr.str("");
		strstr << uid << "_middle.png";
		std::string name = strstr.str();
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
				i18n_symbols["name"] = tintegrate::generate_format(_("middle avatar"), "red");
				i18n_symbols["size"] = tintegrate::generate_format("180x256", "red");
				i18n_symbols["server"] = tintegrate::generate_format(game_config::bbs_server.name, "green");
				gui2::show_message(disp_.video(), "", vgettext("Download $name fail! $name format (size: $size, type: png), please check uploaded file on $server.", i18n_symbols));
			}
		}
		strstr.str("");
		strstr << uid << "_small.png";
		name = strstr.str();
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
				i18n_symbols["name"] = tintegrate::generate_format(_("small avatar"), "red");
				i18n_symbols["size"] = tintegrate::generate_format("48x60", "red");
				i18n_symbols["server"] = tintegrate::generate_format(game_config::bbs_server.name, "green");
				gui2::show_message(disp_.video(), "", vgettext("Download $name fail! $name format (size: $size, type: png), please check uploaded file on $server.", i18n_symbols));
			}
		}
	}

	return true;
}

const std::string www_save_prefix = "WWW_";

bool http_agent::do_uploadsave(const std::string& name)
{
	if (!do_prepare(true)) {
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
	std::stringstream content;
	content << game_config::wesnoth_version.transfer_format().second; // version
	content << std::setbase(16) << std::setfill('0') << std::setw(8) << base_name.size();
	content << std::setbase(16) << std::setfill('0') << std::setw(8) << filesize;
	content << base_name;
	
	char* file_data = (char*)malloc(filesize);
	file_stream->read(file_data, filesize);
	
	trequest request(*this, "uploadsave", content.str(), file_data, filesize);
	dialogs::network_send_dialog(disp_, "", request.buf,  request.size, sock_, 0);
	free(file_data);

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

		std::string str;
		str.assign(content, content_length);
	}

	return true;
}

std::string http_agent::do_downloadsave(int sid)
{
	if (!do_prepare(true)) {
		return null_str;
	}

	std::stringstream content;
	content << "sid=" << sid;

	trequest request(*this, "downloadsave", content.str());
	dialogs::network_send_dialog(disp_, "", request.buf, request.size, sock_);

	std::vector<char> buf;
	dialogs::network_receive_dialog(disp_, "", buf, sock_, 0);

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
			symbols["file"] = tintegrate::generate_format(name, "red");
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
	if (!do_prepare(true)) {
		return list;
	}

	std::stringstream content;
	content << "version=" << game_config::wesnoth_version.transfer_format().second;

	trequest request(*this, "listsave", content.str());
	dialogs::network_send_dialog(disp_, "", request.buf, request.size, sock_);

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
			const std::vector<std::string> fields = utils::split(*it, ',', utils::STRIP_SPACES);
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

membership http_agent::do_uploadpass(const pass_statistic& stat)
{
	membership m;
	if (commerce_protection(disp_, "http_agent::do_uploadpass()")) {
		return m;
	}

	

	return m;
}

std::vector<pass_statistic> http_agent::do_listpass()
{
	std::vector<pass_statistic> list;
	if (!do_prepare(true)) {
		return list;
	}

	std::stringstream content;
	content << "version=" << game_config::wesnoth_version.transfer_format().second;

	trequest request(*this, "listpass", content.str());
	dialogs::network_send_dialog(disp_, "", request.buf, request.size, sock_);

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
			const std::vector<std::string> fields = utils::split(*it, ',', utils::STRIP_SPACES);
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

membership http_agent::do_uploadsiege(const tsiege_record& rec)
{
	membership m;
	if (commerce_protection(disp_, "http_agent::do_uploadsiege()")) {
		return m;
	}

	

	return m;
}

std::vector<tsiege_record> http_agent::do_listsiege()
{
	std::vector<tsiege_record> list;
	if (!do_prepare(true)) {
		return list;
	}

	std::stringstream content;

	trequest request(*this, "listsiege", content.str());
	dialogs::network_send_dialog(disp_, "", request.buf, request.size, sock_);

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

		tsiege_record rec;
		const std::vector<std::string> passes = utils::split(list_str, '#');
		for (std::vector<std::string>::const_iterator it = passes.begin(); it != passes.end(); ++ it) {
			const std::vector<std::string> fields = utils::split(*it, '&', utils::STRIP_SPACES);
			if (fields.size() != 9) {
				continue;
			}
			// pid, username, createtime, duration, score, coin, version, type
			rec.attacker_username = fields[0];
			rec.defender_username = fields[1];
			rec.atk_reinforce_username = fields[2];
			rec.def_reinforce_username = fields[3];
			rec.score = lexical_cast_default<int>(fields[4]);
			rec.create_time = lexical_cast_default<time_t>(fields[5]);
			rec.result = lexical_cast_default<int>(fields[6]);

			const std::vector<std::string> effect = utils::split(fields[8], ',');
			if (effect.size() != 2) {
				continue;
			}
			rec.employee = lexical_cast_default<int>(effect[0]);
			rec.subcontinent = std::make_pair(fields[7], lexical_cast_default<int>(effect[1]));
						
			list.push_back(rec);
		}
	}

	return list;
}

std::map<int, tsubcontinent_city> subcontinent_city_from_str(const std::string& str)
{
	std::map<int, tsubcontinent_city> ret;

	const std::vector<std::string> vstr = utils::split(str, '|', utils::STRIP_SPACES);
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		const std::vector<std::string> fields = utils::split(*it, ',', utils::STRIP_SPACES);
		if (fields.size() != 5) {
			continue;
		}
		int cityno = lexical_cast_default<int>(fields[0]);
		int uid = lexical_cast_default<int>(fields[1]);
		int endurance = lexical_cast_default<int>(fields[2]);
		int times = lexical_cast_default<int>(fields[3]);
		int relation = lexical_cast_default<int>(fields[4]);
		ret.insert(std::make_pair(cityno, tsubcontinent_city(cityno, uid, endurance, times, relation)));
	}
	return ret;
}

tsubcontinent_record http_agent::parse_subcontinent_result(const std::string& str) const
{
	tsubcontinent_record rec;
	const std::vector<std::string> fields = utils::split(str, '&', utils::STRIP_SPACES);
	if (fields.size() != 3) {
		return rec;
	}
	// id, city, speciality
	rec.id = fields[0];
	rec.city = subcontinent_city_from_str(fields[1]);
	rec.speciality = fields[2];
	return rec;
}

std::map<std::string, tsubcontinent_record> http_agent::do_listsubcontinent()
{
	std::map<std::string, tsubcontinent_record> list;
	if (!do_prepare(true)) {
		return list;
	}

	std::stringstream content;

	trequest request(*this, "listsubcontinent", content.str());
	dialogs::network_send_dialog(disp_, "", request.buf, request.size, sock_);

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

		tsubcontinent_record rec;
		const std::vector<std::string> subcontinents = utils::split(list_str, '#');
		for (std::vector<std::string>::const_iterator it = subcontinents.begin(); it != subcontinents.end(); ++ it) {
			rec = parse_subcontinent_result(*it);
			if (!rec.valid()) {
				continue;
			}

			list.insert(std::make_pair(rec.id, rec));
		}
	}

	return list;
}

std::vector<ttitle_record> http_agent::do_listtitle()
{
	std::vector<ttitle_record> list;
	if (!do_prepare(true)) {
		return list;
	}

	std::stringstream content;
	content << "filter=title";
	
	trequest request(*this, "listboard", content.str());
	dialogs::network_send_dialog(disp_, "", request.buf, request.size, sock_);

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

		ttitle_record rec;
		const std::vector<std::string> vstr = utils::split(list_str, '#');
		for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
			const std::vector<std::string> fields = utils::split(*it, '&', utils::STRIP_SPACES);
			if (fields.size() != 8) {
				continue;
			}
			// pid, username, createtime, duration, score, coin, version, type
			rec.title = lexical_cast_default<time_t>(fields[0]);
			rec.uid = lexical_cast_default<int>(fields[1]);
			rec.username = fields[2];
			rec.vip = lexical_cast_default<int>(fields[3]);
			rec.member = fields[4];
			rec.coin = lexical_cast_default<int>(fields[5]);
			rec.score = lexical_cast_default<int>(fields[6]);
			rec.credit = lexical_cast_default<int>(fields[7]);

			list.push_back(rec);
		}
	}

	return list;
}

membership http_agent::do_uploadmessage(const tmessage_record& rec)
{
	membership m;
	if (!do_prepare(true)) {
		return m;
	}

	std::stringstream content;
	content << rec.sender << "&" << rec.receiver_username << "&";
	content << rec.content << "&";
	content << rec.create_time;

	trequest request(*this, "uploadmessage", content.str());
	dialogs::network_send_dialog(disp_, "", request.buf, request.size, sock_);

	std::vector<char> buf;
	dialogs::network_receive_dialog(disp_, "", buf, sock_);

	utils::string_map symbols;
	config data;
	int content_start = http_2_cfg(buf, data);

	if (content_start != -1 && content_start < (int)buf.size()) {
		char* content = &buf[content_start];
		int content_length = (int)buf.size() - content_start;

		if (access_denied(data, content, content_length)) {
			return m;
		}
		content = content + magic_.size();
		content_length -= magic_.size();
		
		std::string str;
		str.assign(content, content_length);

		std::stringstream err;
		if (str == "duplicate") {
			symbols["create"] = tintegrate::generate_format(format_time_date(rec.create_time), "red");
			err << vgettext("wesnoth-lib", "Had uploaded message that create at $create, cannot upload again!", symbols);
			
		} else if (str == "error_receiver") {
			symbols["create"] = tintegrate::generate_format(rec.receiver_username, "red");
			err << vgettext("wesnoth-lib", "Receiver don't exist!", symbols);
			
		} 
		if (!err.str().empty()) {
			gui2::show_message(disp_.video(), "", err.str());
			return m;
		}

		m = parse_membership_result(str);
	}

	return m;
}

std::vector<tmessage_record> http_agent::do_listmessage()
{
	std::vector<tmessage_record> list;
	if (!do_prepare(true)) {
		return list;
	}

	std::stringstream content;

	trequest request(*this, "listmessage", content.str());
	dialogs::network_send_dialog(disp_, "", request.buf, request.size, sock_);

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

		tmessage_record rec;
		const std::vector<std::string> passes = utils::split(list_str, '#');
		for (std::vector<std::string>::const_iterator it = passes.begin(); it != passes.end(); ++ it) {
			const std::vector<std::string> fields = utils::split(*it, '&', utils::STRIP_SPACES);
			if (fields.size() != 4) {
				continue;
			}
			// pid, username, createtime, duration, score, coin, version, type
			rec.sender_username = fields[0];
			rec.receiver_username = fields[1];
			rec.content = fields[2];
			rec.create_time = lexical_cast_default<time_t>(fields[3]);

			list.push_back(rec);
		}
	}

	return list;
}

std::vector<pass_statistic> http_agent::do_listboard_pass()
{
	std::vector<pass_statistic> list;
	if (!do_prepare(true)) {
		return list;
	}

	std::stringstream content;
	content << "version=" << game_config::wesnoth_version.transfer_format().second;
	content << "&filter=pass";
	
	trequest request(*this, "listboard", content.str());
	dialogs::network_send_dialog(disp_, "", request.buf, request.size, sock_);

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

		pass_statistic b;
		const std::vector<std::string> passes = utils::split(list_str, '#');
		for (std::vector<std::string>::const_iterator it = passes.begin(); it != passes.end(); ++ it) {
			const std::vector<std::string> fields = utils::split(*it, ',', utils::STRIP_SPACES);
			if (fields.size() != 8) {
				continue;
			}
			// pid, username, createtime, duration, score, coin, version, type
			b.username = fields[1];
			b.create_time = lexical_cast_default<time_t>(fields[2]);
			b.duration = lexical_cast_default<int>(fields[3]);
			b.coin = lexical_cast_default<int>(fields[4]);
			b.score = lexical_cast_default<int>(fields[5]);
			b.version = lexical_cast_default<int>(fields[6]);
			b.type = lexical_cast_default<int>(fields[7]);

			list.push_back(b);
		}
	}

	return list;
}

std::vector<membership> http_agent::do_listboard_score()
{
	std::vector<membership> list;
	if (!do_prepare(true)) {
		return list;
	}

	std::stringstream content;
	content << "version=" << game_config::wesnoth_version.transfer_format().second;
	content << "&filter=score";

	trequest request(*this, "listboard", content.str());
	dialogs::network_send_dialog(disp_, "", request.buf, request.size, sock_);

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

		return parse_membershiplist_str_2_vector(list_str);
	}

	return list;
}

membership http_agent::do_upgrade(int number, int coin, int score)
{
	membership member;
	if (commerce_protection(disp_, "http_agent::do_upgrade()")) {
		return member;
	}

	
	return member;
}

struct member_pair 
{
	member_pair(const std::string& _op, const std::string& _name)
		: op(_op)
		, name(_name)
	{}

	std::string op;
	std::string name;
};

membership http_agent::do_member(int op, const std::vector<std::pair<int, int> >& m, int coin, int score)
{
	static std::vector<member_pair> ops;
	if (ops.empty()) {
		ops.push_back(member_pair("reload", dsgettext("wesnoth-lib", "member^Reload")));
		ops.push_back(member_pair("insert", dsgettext("wesnoth-lib", "member^Insert")));
		ops.push_back(member_pair("erase", dsgettext("wesnoth-lib", "member^Erase")));
		ops.push_back(member_pair("resort", dsgettext("wesnoth-lib", "member^Resort")));
	}

	membership member;
	if (commerce_protection(disp_, "http_agent::do_member()")) {
		return member;
	}

	
	return member;
}

std::string exile_tag_str(int tag)
{
	if (tag == exile_tag_erase) {
		return "erase";
	} else if (tag == exile_tag_join) {
		return "join";
	}
	return null_str;
}

membership http_agent::do_exile(int tag, const std::vector<std::pair<int, int> >& m, int coin, int score)
{
	membership member;
	if (commerce_protection(disp_, "http_agent::do_exile()")) {
		return member;
	}

	
	return member;
}

membership http_agent::do_score(int coin, int score, bool transaction)
{
	membership member;
	if (commerce_protection(disp_, "http_agent::do_score()")) {
		return member;
	}

	
	return member;
}

std::string block_tag_str(int tag)
{
	if (tag == block_tag_city) {
		return "city";
	} else if (tag == block_tag_field) {
		return "field";
	} else if (tag == block_tag_translatable) {
		return "translatable";
	} else if (tag == block_tag_layout) {
		return "layout";
	} else if (tag == block_tag_map) {
		return "map";
	} else if (tag == block_tag_member) {
		return "member";
	} else if (tag == block_tag_coin) {
		return "coin";
	} else if (tag == block_tag_score) {
		return "score";
	}
	return null_str;
}

membership http_agent::upload_data(const std::map<int, std::string>& block, bool quiet, int uid)
{
	membership member;
	if (commerce_protection(disp_, "http_agent::upload_data()")) {
		return member;
	}

	
	return member;
}

membership http_agent::insert_transaction(const std::string& number, const std::string& buyer, int request_uid, int type)
{
	membership member;
	if (commerce_protection(disp_, "http_agent::insert_transaction()")) {
		return member;
	}

	
	return member;
}

std::string associate_tag_str(int tag)
{
	if (tag == associate_tag_insert) {
		return "insert";
	} else if (tag == associate_tag_erase) {
		return "erase";
	} else if (tag == associate_tag_requestally) {
		return "requestally";
	} else if (tag == associate_tag_ally) {
		return "ally";
	} else if (tag == associate_tag_requestterminate) {
		return "requestterminate";
	} else if (tag == associate_tag_terminate) {
		return "terminate";
	} else if (tag == associate_tag_undo) {
		return "undo";
	} else if (tag == associate_tag_refuse) {
		return "refuse";
	}
	return null_str;
}

membership http_agent::do_associate(const std::string& username, int tag, bool check_network)
{
	membership member;
	if (commerce_protection(disp_, "http_agent::do_associate()")) {
		return member;
	}

	
	return member;
}

std::string employee_tag_str(int tag)
{
	if (tag == employee_tag_insert) {
		return "insert";
	} else if (tag == employee_tag_erase) {
		return "erase";
	} else if (tag == employee_tag_employ) {
		return "employ";
	} else if (tag == employee_tag_fire) {
		return "fire";
	} else if (tag == employee_tag_upgrade) {
		return "upgrade";
	} else if (tag == employee_tag_lock) {
		return "lock";
	} else if (tag == employee_tag_unlock) {
		return "unlock";
	}
	return null_str;
}

membership http_agent::do_employee_insert(hero_map& heros)
{
	membership member;
	const std::string& _do = employee_tag_str(employee_tag_insert);
	if (_do.empty()) {
		return member;
	}
	if (!do_prepare(true)) {
		return member;
	}

	std::stringstream content;
	content << "do=" << _do << "&param=";
	bool first = true;
	for (hero_map::const_iterator it = heros.begin(); it != heros.end(); ++ it) {
		const hero& h = *it;
		if (h.number_ >= hero_map::map_size_from_dat) {
			break;
		}
		if (!h.get_flag(hero_flag_employee)) {
			continue;
		}
		if (!first) {
			content << "|";
		} else {
			first = false;
		}
		int number = h.number_;
		int cost = game_config::employee_base_score + h.cost_;
		content << number << "," << game_config::min_employee_level << "," << cost;
	}

	trequest request(*this, "employee", content.str());
	dialogs::network_send_dialog(disp_, "", request.buf, request.size, sock_);

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

		std::stringstream err;
		utils::string_map symbols;
		symbols["action"] = tintegrate::generate_format(_("Employee"), "yellow");
		if (str == "id_absent") {
			err << vgettext("wesnoth-lib", "When $action, id_absent!", symbols);
		} else if (str == "misstatus") {
			err << vgettext("wesnoth-lib", "When $action, error status!", symbols);
		} else if (str == "misscore") {
			err << vgettext("wesnoth-lib", "When $action, error score!", symbols);
		} 
		if (err.str().empty()) {
			member = parse_membership_result(str);
			if (member.uid < 0) {
				err << vgettext("When $action, unknown error!", symbols);
			}
		}
		if (!err.str().empty()) {
			gui2::show_message(disp_.video(), "", err.str());
		}
	}
	return member;
}

membership http_agent::do_employee_common(int tag, const std::set<int>& number, int coin, int score, std::map<int, temployee>* employees)
{
	membership member;
	if (commerce_protection(disp_, "http_agent::do_employee_common()")) {
		return member;
	}

	
	return member;
}

std::map<int, temployee> http_agent::parse_employeelist_str_2_map(const std::string& str) const
{
	std::map<int, temployee> employees;
	const std::vector<std::string> vstr = utils::split(str, '#', utils::STRIP_SPACES);
	temployee e;
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		const std::vector<std::string> fields = utils::split(*it, ',', utils::STRIP_SPACES);
		e.number = HEROS_INVALID_NUMBER;
		if (fields.size() >= 6) {
			e.number = lexical_cast_default<int>(fields[0]);
			e.level = lexical_cast_default<int>(fields[1]);
			e.score = lexical_cast_default<int>(fields[2]);
			e.uid = lexical_cast_default<int>(fields[3]);
			e.lock = lexical_cast_default<int>(fields[4]);
			e.username = fields[5];
		}
		if (e.number == HEROS_INVALID_NUMBER) {
			employees.clear();
			return employees;
		}
		employees.insert(std::make_pair(e.number, e));
	}

	return employees;
}

std::map<int, temployee> http_agent::do_list_employee()
{
	std::map<int, temployee> employees;
	if (!do_prepare(true)) {
		return employees;
	}

	std::stringstream content;
	
	trequest request(*this, "listemployee", content.str());
	dialogs::network_send_dialog(disp_, "", request.buf, request.size, sock_);

	std::vector<char> buf;
	dialogs::network_receive_dialog(disp_, "", buf, sock_);

	config data;
	int content_start = http_2_cfg(buf, data);
	if (content_start != -1 && content_start < (int)buf.size()) {
		char* content = &buf[content_start];
		int content_length = (int)buf.size() - content_start;

		if (access_denied(data, content, content_length)) {
			return employees;
		}
		content = content + magic_.size();
		content_length -= magic_.size();

		// buf maybe not end with '\0'
		std::string str;
		str.assign(content, content_length);

		return parse_employeelist_str_2_map(str);
	}
	return employees;
}

std::string signin_tag_str(int tag)
{
	if (tag == signin_tag_fillup) {
		return "fillup";
	}
	return null_str;
}

membership http_agent::do_signin(int tag)
{
	membership member;
	if (commerce_protection(disp_, "http_agent::do_signin()")) {
		return member;
	}

	
	return member;
}

std::string subcontinent_tag_str(int tag)
{
	if (tag == subcontinent_tag_repair) {
		return "repair";
	} else if (tag == subcontinent_tag_discard) {
		return "discard";
	}
	return null_str;
}

std::pair<tsubcontinent_record, membership> http_agent::do_subcontinent(int tag, const std::string& id, int cityno)
{
	tsubcontinent_record rec;
	membership member;
	if (commerce_protection(disp_, "http_agent::do_subcontinent()")) {
		return std::make_pair(rec, member);
	}

	
	return std::make_pair(rec, member);
}

bool register_user(display& disp, hero_map& heros, bool check_exist)
{
	network::http_data_lock data_lock;
	const network::manager net_manager(1, 1);

	http_agent agent(disp, heros);
	return agent.do_register(check_exist);
}

membership session(display& disp, hero_map& heros)
{
	network::http_data_lock data_lock;
	const network::manager net_manager(1, 1);

	http_agent agent(disp, heros, false);
	return agent.do_session(true);
}

membership membership_hero(display& disp, hero_map& heros, bool quiet, const std::string& username)
{
	network::http_data_lock data_lock;
	const network::manager net_manager(1, 1);

	http_agent agent(disp, heros);
	return agent.do_membership(quiet, -1, username);
}

membership membership_from_uid(display& disp, hero_map& heros, bool quiet, int uid)
{
	network::http_data_lock data_lock;
	const network::manager net_manager(1, 1);

	http_agent agent(disp, heros);
	return agent.do_membership(quiet, uid, "");
}

// if dialog is true, ignore ok param.
membership affirm_ally(display& disp, hero_map& heros, const std::string& username, bool dialog, bool ok)
{
	std::string message;
	utils::string_map symbols;
	membership m;

	if (dialog) {
		symbols["username"] = tintegrate::generate_format(username, "green");
		message = vgettext("wesnoth-lib", "$username wants to ally, do you agree?", symbols);
		int res = gui2::show_message(disp.video(), "", message, gui2::tmessage::yes_no_buttons);
		ok = res == gui2::twindow::OK;
	}
	// this function is called by tgroup::from_local_membership only,
	// set check_network to false to avoid castnet in http_agent::do_prepare.
	return m = http::associate(disp, heros, username, ok? associate_tag_ally: associate_tag_refuse, false);
}

membership affirm_terminate(display& disp, hero_map& heros, const std::string& username, bool dialog)
{
	std::string message;
	utils::string_map symbols;
	membership m;

	if (dialog) {
		symbols["username"] = tintegrate::generate_format(username, "red");
		message = vgettext("wesnoth-lib", "Notice, $username declares to terminate with you!", symbols);
		gui2::show_message(disp.video(), "", message);
	} 
	// this function is called by tgroup::from_local_membership only,
	// set check_network to false to avoid castnet in http_agent::do_prepare.
	return http::associate(disp, heros, username, associate_tag_terminate, false);
}

membership membership_hero2(display& disp, hero_map& heros)
{
	membership member = membership_hero(disp, heros, true);
	if (member.uid < 0) {
		return member;
	}
	group.from_local_membership(disp, heros, member, true);
	return member;
}

std::vector<membership> membershiplist_vector(display& disp, hero_map& heros, bool quiet, const std::string& request_str, bool uid)
{
	network::http_data_lock data_lock;
	const network::manager net_manager(1, 1);

	http_agent agent(disp, heros);
	return agent.do_membershiplist_vector(quiet, request_str, uid);
}

std::map<int, membership> membershiplist_map(display& disp, hero_map& heros, bool quiet, const std::string& request_str, bool uid)
{
	network::http_data_lock data_lock;
	const network::manager net_manager(1, 1);

	http_agent agent(disp, heros);
	return agent.do_membershiplist_map(quiet, request_str, uid);
}

bool avatar_hero(display& disp, hero_map& heros, int uid, bool quiet)
{
	network::http_data_lock data_lock;
	const network::manager net_manager(1, 1);

	http_agent agent(disp, heros);
	return agent.do_avatar(uid, quiet);
}

bool upload_save(display& disp, hero_map& heros, const std::string& name)
{
	network::http_data_lock data_lock;
	const network::manager net_manager(1, 1);

	http_agent agent(disp, heros);
	return agent.do_uploadsave(name);
}

std::string download_save(display& disp, hero_map& heros, int sid)
{
	network::http_data_lock data_lock;
	const network::manager net_manager(1, 1);

	http_agent agent(disp, heros);
	return agent.do_downloadsave(sid);
}

std::vector<savegame::www_save_info> list_save(display& disp, hero_map& heros)
{
	network::http_data_lock data_lock;
	const network::manager net_manager(1, 1);

	http_agent agent(disp, heros);
	return agent.do_listsave();
}

membership upload_pass(display& disp, hero_map& heros, const pass_statistic& stat)
{
	network::http_data_lock data_lock;
	const network::manager net_manager(1, 1);

	http_agent agent(disp, heros);
	return agent.do_uploadpass(stat);
}

std::vector<pass_statistic> list_pass(display& disp, hero_map& heros)
{
	network::http_data_lock data_lock;
	const network::manager net_manager(1, 1);

	http_agent agent(disp, heros);
	return agent.do_listpass();
}

membership upload_siege(display& disp, hero_map& heros, const tsiege_record& rec)
{
	network::http_data_lock data_lock;
	const network::manager net_manager(1, 1);

	http_agent agent(disp, heros);
	return agent.do_uploadsiege(rec);
}

std::vector<tsiege_record> list_siege(display& disp, hero_map& heros)
{
	network::http_data_lock data_lock;
	const network::manager net_manager(1, 1);

	http_agent agent(disp, heros);
	return agent.do_listsiege();
}

tsubcontinent_record null_subcontinent;

std::map<std::string, tsubcontinent_record> list_subcontinent(display& disp, hero_map& heros)
{
	network::http_data_lock data_lock;
	const network::manager net_manager(1, 1);

	http_agent agent(disp, heros);
	return agent.do_listsubcontinent();
}

std::vector<ttitle_record> list_title(display& disp, hero_map& heros)
{
	network::http_data_lock data_lock;
	const network::manager net_manager(1, 1);

	http_agent agent(disp, heros);
	return agent.do_listtitle();
}

membership upload_message(display& disp, hero_map& heros, const tmessage_record& rec)
{
	network::http_data_lock data_lock;
	const network::manager net_manager(1, 1);

	http_agent agent(disp, heros);
	return agent.do_uploadmessage(rec);
}

std::vector<tmessage_record> list_message(display& disp, hero_map& heros)
{
	network::http_data_lock data_lock;
	const network::manager net_manager(1, 1);

	http_agent agent(disp, heros);
	return agent.do_listmessage();
}

std::vector<pass_statistic> list_board_pass(display& disp, hero_map& heros)
{
	network::http_data_lock data_lock;
	const network::manager net_manager(1, 1);

	http_agent agent(disp, heros);
	return agent.do_listboard_pass();
}

std::vector<membership> list_board_score(display& disp, hero_map& heros)
{
	network::http_data_lock data_lock;
	const network::manager net_manager(1, 1);

	http_agent agent(disp, heros);
	return agent.do_listboard_score();
}

membership upgrade(display& disp, hero_map& heros, int number, int coin, int score)
{
	network::http_data_lock data_lock;
	const network::manager net_manager(1, 1);

	http_agent agent(disp, heros);
	return agent.do_upgrade(number, coin, score);
}

membership member(display& disp, hero_map& heros, int op, const std::vector<std::pair<int, int> >& m, int coin, int score)
{
	network::http_data_lock data_lock;
	const network::manager net_manager(1, 1);

	http_agent agent(disp, heros);
	return agent.do_member(op, m, coin, score);
}

membership exile(display& disp, hero_map& heros, int tag, const std::vector<std::pair<int, int> >& m, int coin, int score)
{
	network::http_data_lock data_lock;
	const network::manager net_manager(1, 1);

	http_agent agent(disp, heros);
	return agent.do_exile(tag, m, coin, score);
}

membership score(display& disp, hero_map& heros, int coin, int _score, bool transaction)
{
	network::http_data_lock data_lock;
	const network::manager net_manager(1, 1);

	http_agent agent(disp, heros);
	return agent.do_score(coin, _score, transaction);
}

membership upload_data(display& disp, hero_map& heros, const std::map<int, std::string>& block, bool quiet, int uid)
{
	network::http_data_lock data_lock;
	const network::manager net_manager(1, 1);

	http_agent agent(disp, heros);
	return agent.upload_data(block, quiet, uid);
}

membership upload_layout(display& disp, hero_map& heros, const std::string& layout_str, const std::string& map_str)
{
	std::map<int, std::string> block;
	block.insert(std::make_pair((int)http::block_tag_layout, layout_str));
	block.insert(std::make_pair((int)http::block_tag_map, map_str));
	return http::upload_data(disp, heros, block, false, -1);
}

membership insert_transaction(display& disp, hero_map& heros, const std::string& number, const std::string& buyer, int request_uid, int type)
{
	network::http_data_lock data_lock;
	const network::manager net_manager(1, 1);

	http_agent agent(disp, heros);
	return agent.insert_transaction(number, buyer, request_uid, type);
}

membership associate(display& disp, hero_map& heros, const std::string& username, int tag, bool check_network)
{
	network::http_data_lock data_lock;
	const network::manager net_manager(1, 1);

	http_agent agent(disp, heros);
	return agent.do_associate(username, tag, check_network);
}

membership employee_insert(display& disp, hero_map& heros)
{
	network::http_data_lock data_lock;
	const network::manager net_manager(1, 1);

	http_agent agent(disp, heros);
	return agent.do_employee_insert(heros);
}

membership employee_common(display& disp, hero_map& heros, int tag, const std::set<int>& number, int coin, int score, std::map<int, temployee>* employees)
{
	network::http_data_lock data_lock;
	const network::manager net_manager(1, 1);

	http_agent agent(disp, heros);
	return agent.do_employee_common(tag, number, coin, score, employees);
}

std::map<int, temployee> list_employee(display& disp, hero_map& heros)
{
	network::http_data_lock data_lock;
	const network::manager net_manager(1, 1);

	http_agent agent(disp, heros);
	return agent.do_list_employee();
}

bool do_employ_bh(display& disp, hero_map& heros, bool employ, int number, int coin, int score, std::map<int, temployee>* employees)
{
	std::set<int> numbers;

	numbers.insert(number);
	http::membership member = employee_common(disp, heros, employ? http::employee_tag_employ: http::employee_tag_fire, numbers, coin, score, employees);
	if (member.uid < 0) {
		return false;
	}
	group.from_local_membership(disp, heros, member, false);
	return true;
}

bool employee_employ(display& disp, hero_map& heros, hero& h, int score, std::map<int, temployee>* employees)
{
	utils::string_map symbols;
	std::string message;

	if (sum_score(group.coin(), group.score()) < score) {
		symbols["cost"] = tintegrate::generate_format(score, "green");
		message = vgettext("wesnoth-lib", "Repertory is less than $cost score!", symbols);
		gui2::show_message(disp.video(), "", message);
		return false;
	}

	symbols["name"] = tintegrate::generate_format(h.name(), "green");
	symbols["cost"] = tintegrate::generate_format(score, "red");
	message = vgettext("wesnoth-lib", "Do you want to spend $cost score to employ $name?", symbols);
	int	coin_income = 0;
	int	score_income = -1 * score;
	
	int res = gui2::show_message(disp.video(), "", message, gui2::tmessage::yes_no_buttons);
	if (res == gui2::twindow::CANCEL) {
		return false;
	}

	return do_employ_bh(disp, heros, true, h.number_, coin_income, score_income, employees);
}

bool employee_fire(display& disp, hero_map& heros, hero& h, int level, std::map<int, temployee>* employees)
{
	utils::string_map symbols;
	std::string message;

	upgrade::trequire cost = upgrade::member_upgrade_cost(game_config::min_employee_level, level);
	int coin_income = cost.coin;
	int score_income = cost.score;

	game_config::recycle_score_income(coin_income, score_income);

	symbols["name"] = tintegrate::generate_format(h.name(), "red");
	symbols["coin"] = tintegrate::generate_format(coin_income, "green");
	symbols["score"] = tintegrate::generate_format(score_income, "green");
	message = vgettext("wesnoth-lib", "May get $coin coin and $score score, do you want to discard $name?", symbols);
	
	int res = gui2::show_message(disp.video(), "", message, gui2::tmessage::yes_no_buttons);
	if (res == gui2::twindow::CANCEL) {
		return false;
	}
	return do_employ_bh(disp, heros, false, h.number_, coin_income, score_income, employees);
}

membership signin(display& disp, hero_map& heros, int tag)
{
	network::http_data_lock data_lock;
	const network::manager net_manager(1, 1);

	http_agent agent(disp, heros);
	return agent.do_signin(tag);
}

std::pair<tsubcontinent_record, membership> subcontinent(display& disp, hero_map& heros, int tag, const std::string& id, int cityno)
{
	network::http_data_lock data_lock;
	const network::manager net_manager(1, 1);

	http_agent agent(disp, heros);
	return agent.do_subcontinent(tag, id, cityno);
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

static void enter_wait_mode(display& disp, const config& game_config, hero_map& heros, hero_map& heros_start, card_map& cards, config& gamelist, int game_id, bool observe)
{
	mp::result res;
	game_state state;
	network_game_manager m;

	gamelist.clear();
	statistics::fresh_stats();

	gui2::tmp_side_wait dlg(heros, heros_start, disp, *resources::game_map, game_config, gamelist, game_id, observe);
	dlg.show(disp.video());
	switch (dlg.get_legacy_result()) {
	case gui2::tmp_side_wait::PLAY:
		res = mp::PLAY;
		dlg.start_game();
		state = dlg.get_state();
		// lobby may modify hero's side_feature
		// heros_start = heros;
		break;
	default:
		res = mp::QUIT;
	}

	switch (res) {
	case mp::PLAY:
		play_game(disp, state, game_config, heros, heros_start, cards, IO_CLIENT,
			preferences::skip_mp_replay() && observe);
		recorder.clear();

		break;
	case mp::QUIT:
	default:
		break;
	}
}

static void enter_create_mode(display& disp, const config& game_config, hero_map& heros, hero_map& heros_start, card_map& cards, config& gamelist, tcontroller default_controller, bool local_players_only = false);

static void enter_connect_mode(display& disp, const config& game_config, hero_map& heros, hero_map& heros_start,
		card_map& cards,
		config& gamelist, const mp_game_settings& params,
		const int num_turns, tcontroller default_controller, bool local_players_only = false)
{
	mp::result res;
	game_state state;
	const network::manager net_manager(1,1);
	network_game_manager m;

	gamelist.clear();
	statistics::fresh_stats();

	{
		gui2::tmp_side_creator dlg(heros, heros_start, disp, *resources::game_map, game_config, gamelist, params, num_turns, default_controller, local_players_only);
		dlg.show(disp.video());

		switch (dlg.get_legacy_result()) {
		case gui2::tmp_side_creator::PLAY:
			res = mp::PLAY;
			dlg.start_game();
			state = dlg.get_state();
			break;
		case gui2::tmp_side_creator::CREATE:
			res = mp::CREATE;
			break;
		default:
			res = mp::QUIT;
		}
	}

	switch (res) {
	case mp::PLAY:
		play_game(disp, state, game_config, heros, heros_start, cards, IO_SERVER);
		recorder.clear();

		break;
	case mp::CREATE:
		enter_create_mode(disp, game_config, heros, heros_start, cards, gamelist, default_controller, local_players_only);
		break;
	case mp::QUIT:
	default:
		network::send_data(config("refresh_lobby"), 0);
		break;
	}
}

static void enter_create_mode(display& disp, const config& game_config, hero_map& heros, hero_map& heros_start, 
		card_map& cards, config& gamelist, tcontroller default_controller, bool local_players_only)
{
	mp::result res;
	mp_game_settings params;
	int num_turns;

	{
		gui2::tmp_create_game dlg(disp, game_config, local_players_only);
		dlg.show(disp.video());
		int retval = dlg.get_retval();
		if (retval == gui2::twindow::OK) {
			res = mp::CREATE;
		} else {
			res = mp::QUIT;
		}
		params = dlg.get_parameters();
		num_turns = dlg.num_turns();

		// network::send_data(config("refresh_lobby"), 0);
	}

	switch (res) {
	case mp::CREATE:
		enter_connect_mode(disp, game_config, heros, heros_start, cards, gamelist, params, num_turns, default_controller, local_players_only);
		break;
	case mp::QUIT:
	default:
		//update lobby content
		network::send_data(config("refresh_lobby"), 0);
		break;
	}
}

static void enter_lobby_mode(display& disp, const config& game_config, hero_map& heros, hero_map& heros_start, 
		card_map& cards, config& gamelist)
{


	mp::result res;

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

		int game_id;
		{
			// load game will update heros, reset it.
			// fix!! this equal maybe result group invalid. side feature cannot change.
			// heros = heros_start;

			gui2::tlobby_main dlg(game_config, li, disp, heros, heros_start);
			dlg.show(disp.video());
			//ugly kludge for launching other dialogs like the old lobby
			switch (dlg.get_legacy_result()) {
				case gui2::tlobby_main::CREATE:
					res = mp::CREATE;
					break;
				case gui2::tlobby_main::JOIN:
					game_id = dlg.joined_game_id();
					res = mp::JOIN;
					break;
				case gui2::tlobby_main::OBSERVE:
					game_id = dlg.joined_game_id();
					res = mp::OBSERVE;
					break;
				default:
					res = mp::QUIT;
			}
		}

		switch (res) {
		case mp::JOIN:
			try {
				enter_wait_mode(disp, game_config, heros, heros_start, cards, gamelist, game_id, false);
			} catch(config::error& error) {
				if(!error.message.empty()) {
					gui2::show_error_message(disp.video(), error.message);
				}
				//update lobby content
				network::send_data(config("refresh_lobby"), 0);
			}
			break;
		case mp::OBSERVE:
			try {
				enter_wait_mode(disp, game_config, heros, heros_start, cards, gamelist, game_id, true);
			} catch(config::error& error) {
				if(!error.message.empty()) {
					gui2::show_error_message(disp.video(), error.message);
				}
			}
			// update lobby content unconditionally because we might have left only after the
			// game ended in which case we ignored the gamelist and need to request it again
			network::send_data(config("refresh_lobby"), 0);
			break;
		case mp::CREATE:
			try {
				enter_create_mode(disp, game_config, heros, heros_start, cards, gamelist, CNTR_NETWORK);
			} catch(config::error& error) {
				if (!error.message.empty())
					gui2::show_error_message(disp.video(), error.message);
				//update lobby content
				network::send_data(config("refresh_lobby"), 0);
			}
			break;
		case mp::QUIT:
			return;
		default:
			return;
		}
	}
}

namespace mp {

void check_response(network::connection res, const config& data)
{
	if (!res) {
		throw network::error(_("Connection timed out"));
	}

	if (const config &err = data.child("error")) {
		throw network::error(err["message"]);
	}
}

std::string get_color_string(int id)
{
	std::string prefix = team::get_side_highlight(id);
/*
	std::map<std::string, t_string>::iterator name = game_config::team_rgb_name.find(str_cast(id + 1));
	if(name != game_config::team_rgb_name.end()){
		return prefix + name->second;
	}else{
		return prefix + _("Invalid Color");
	}
*/
	return game_config::team_rgb_name[id % game_config::team_rgb_name.size()].second;
}

void start_local_game(display& disp, const config& game_config, hero_map& heros, hero_map& heros_start,
		card_map& cards, tcontroller default_controller)
{
	const rand_rng::set_random_generator generator_setter(&recorder);
	config gamelist;
	playmp_controller::set_replay_last_turn(0);
	preferences::set_message_private(false);
	enter_create_mode(disp, game_config, heros, heros_start, cards, gamelist, default_controller, true);
}

void start_client(display& disp, const config& game_config, hero_map& heros, hero_map& heros_start,
		card_map& cards, const std::string& host)
{
	const rand_rng::set_random_generator generator_setter(&recorder);
	const network::manager net_manager(1,1);

	config gamelist;
	// server_type type = open_connection(disp, heros, host);
	server_type type = WESNOTHD_SERVER;

	switch(type) {
	case WESNOTHD_SERVER:
		enter_lobby_mode(disp, game_config, heros, heros_start, cards, gamelist);
		break;
	case ABORT_SERVER:
		break;
	}
}

}

std::string calculate_res_checksum(display& disp, const config& game_config)
{
	std::string ret = null_str;
	if (commerce_protection(disp, "calculate_res_checksum()")) {
		return ret;
	}

	

	return ret;
}

