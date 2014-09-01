/* $Id: multiplayer.hpp 47847 2010-12-05 21:12:23Z shadowmaster $ */
/*
   Copyright (C) 2005 - 2010 Philippe Plantier <ayin@anathas.org>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/
#ifndef MULTIPLAYER_HPP_INCLUDED
#define MULTIPLAYER_HPP_INCLUDED

#include "network.hpp"
#include "sdl_utils.hpp"
#include "hero.hpp"

class config;
class display;
class game_display;
class card_map;

namespace savegame {

struct www_save_info;

}

enum tcontroller { CNTR_NETWORK = 0, CNTR_LOCAL, CNTR_COMPUTER, CNTR_EMPTY, CNTR_LAST };

namespace mp {

enum result { CONTINUE, JOIN, OBSERVE, CREATE, PREFERENCES, PLAY, QUIT };

void check_response(network::connection res, const config& data);

std::string get_color_string(int id);

/*
 * This is the main entry points of multiplayer mode.
 */

/** Starts a multiplayer game in single-user mode.
 *
 * @param disp        The global display
 * @param game_config The global, top-level WML configuration for the game
 * @param default_controller The default controller type
 */
void start_local_game(game_display& disp, const config& game_config, hero_map& heros, hero_map& heros_start, 
		card_map& cards, tcontroller default_controller);

/** Starts a multiplayer game in client mode.
 *
 * @param disp        The global display
 * @param game_config The global, top-level WML configuration for the game
 * @param host        The host to connect to.
 */
void start_client(game_display& disp, const config& game_config, hero_map& heros, hero_map& heros_start, 
		card_map& cards, const std::string& host);


}

namespace http {

extern int INVALID_UID;

bool register_user(game_display& disp, hero_map& heros, bool check_exist = false);
struct membership 
{
	membership():
	  uid(-1)
	  , vip(0)
	  , name()
	  , expire(0)
	  , noble(0)
	  , member()
	  , exile()
	  , associate()
	  , layout()
	  , field()
	  , translatable()
	  , score(0)
	  , coin(0)
	  , credit(0)
	  , city()
	  , map()
	  , interior()
	  , signin()
	  , message()
	  , title()
	  , timestamp(0)
	  , tax(0)
	  , version(0)
	 {}


	int uid; // uid = -1 indicate network fail.
	int vip;
	std::string name;
	time_t expire;
	int noble;
	std::string member;
	std::string exile;
	std::string associate;
	std::string layout;
	std::string field;
	std::string translatable;
	int score;
	int coin;
	int credit;
	std::string city;
	std::string map;
	std::string interior;
	std::string signin;
	std::string message;
	std::string title;
	size_t timestamp;
	int tax;
	int version;
};
membership session(game_display& disp, hero_map& heros);
membership membership_hero(game_display& disp, hero_map& heros, bool quiet, const std::string& username = null_str);
membership membership_from_uid(game_display& disp, hero_map& heros, bool quiet, int uid);
membership membership_hero2(game_display& disp, hero_map& heros);
std::vector<membership> membershiplist_vector(game_display& disp, hero_map& heros, bool quiet, const std::string& request_str, bool uid);
std::map<int, membership> membershiplist_map(game_display& disp, hero_map& heros, bool quiet, const std::string& request_str, bool uid);
membership affirm_ally(display& disp, hero_map& heros, const std::string& username, bool dialog, bool ok);
membership affirm_terminate(display& disp, hero_map& heros, const std::string& username, bool dialog);
bool avatar_hero(game_display& disp, hero_map& heros, int uid, bool quiet);
bool download_avatar(game_display& disp, hero_map& heros);
bool upload_save(game_display& disp, hero_map& heros, const std::string& name);
std::string download_save(game_display& disp, hero_map& heros, int sid);
std::vector<savegame::www_save_info> list_save(game_display& disp, hero_map& heros);

struct pass_statistic {
	std::string username;
	time_t create_time;
	int duration;
	int version;
	int coin;
	int score;
	int type;
	int hash;
	std::pair<std::string, int> subcontinent;
};

membership upload_pass(game_display& disp, hero_map& heros, const pass_statistic& stat);
std::vector<pass_statistic> list_pass(game_display& disp, hero_map& heros);

enum {
	siege_result_victory,
	siege_result_defeat,
	siege_result_fallen
};
struct tsiege_record {
	tsiege_record():
	  attacker(0)
	  , defender(0)
	  , atk_reinforce(0)
	  , def_reinforce(0)
	  , attacker_username()
	  , defender_username()
	  , atk_reinforce_username()
	  , def_reinforce_username()
	  , score(0)
	  , result(0)
	  , create_time(0)
	  , hash(0)
	  , subcontinent(std::make_pair(null_str, -1))
	  , employee(HEROS_INVALID_NUMBER)
	 {}

	int attacker;
	int defender;
	int atk_reinforce;
	int def_reinforce;
	std::string attacker_username;
	std::string defender_username;
	std::string atk_reinforce_username;
	std::string def_reinforce_username;
	int score;
	int result;
	time_t create_time;
	int hash;
	std::pair<std::string, int> subcontinent;
	int employee;
};
membership upload_siege(game_display& disp, hero_map& heros, const tsiege_record& record);
std::vector<tsiege_record> list_siege(game_display& disp, hero_map& heros);

struct tsubcontinent_city {
	tsubcontinent_city(int cityno, int uid, int endurance, int times, int relation)
		: cityno(cityno)
		, uid(uid)
		, endurance(endurance)
		, times(times)
		, relation(relation)
	{}

	int cityno;
	int uid;
	int endurance;
	int times;
	int relation;
};

struct tsubcontinent_record {
	tsubcontinent_record()
		: id()
		, city()
		, speciality()
	{}
	bool valid() const { return !id.empty(); }

	std::string id;
	std::map<int, tsubcontinent_city> city;
	std::string speciality;
};
extern tsubcontinent_record null_subcontinent;
std::map<std::string, tsubcontinent_record> list_subcontinent(display& disp, hero_map& heros);

struct ttitle_record {
	ttitle_record():
	  title(0)
	  , uid(0)
	  , username()
	  , vip(0)
	  , member()
	  , coin(0)
	  , score(0)
	  , credit(0)
	 {}

	int title;
	int uid;
	std::string username;
	int vip;
	std::string member;
	int coin;
	int score;
	int credit;
};
std::vector<ttitle_record> list_title(display& disp, hero_map& heros);

struct tmessage_record {
	tmessage_record():
	  sender(0)
	  , receiver(0)
	  , content()
	  , sender_username()
	  , receiver_username()
	  , create_time(0)
	 {}

	int sender;
	int receiver;
	std::string sender_username;
	std::string receiver_username;
	std::string content;
	time_t create_time;
};
membership upload_message(display& disp, hero_map& heros, const tmessage_record& record);
std::vector<tmessage_record> list_message(display& disp, hero_map& heros);

std::vector<pass_statistic> list_board_pass(game_display& disp, hero_map& heros);
std::vector<membership> list_board_score(game_display& disp, hero_map& heros);

membership upgrade(game_display& disp, hero_map& heros, int number, int coin, int score);
enum {
	member_reload,
	member_insert, 
	member_erase, 
	member_resort
};
membership member(game_display& disp, hero_map& heros, int op, const std::vector<std::pair<int, int> >& m, int coin, int score);
enum {
	exile_tag_erase, 
	exile_tag_join
};
membership exile(game_display& disp, hero_map& heros, int tag, const std::vector<std::pair<int, int> >& m, int coin, int score);

membership score(game_display& disp, hero_map& heros, int coin, int _score, bool transaction);
enum {
	block_tag_city,
	block_tag_field, 
	block_tag_translatable,
	block_tag_layout,
	block_tag_map,
	block_tag_member,
	block_tag_coin,
	block_tag_score,
};
membership upload_data(display& disp, hero_map& heros, const std::map<int, std::string>& block, bool quiet, int uid = -1);
membership upload_layout(game_display& disp, hero_map& heros, const std::string& layout_str, const std::string& map_str);

membership insert_transaction(game_display& disp, hero_map& heros, const std::string& number, const std::string& buyer, int request_uid, int type);
enum {
	associate_tag_insert, 
	associate_tag_erase, 
	associate_tag_requestally,
	associate_tag_ally,
	associate_tag_requestterminate,
	associate_tag_terminate,
	associate_tag_undo,
	associate_tag_refuse,
};
membership associate(display& disp, hero_map& heros, const std::string& username, int tag, bool check_network = true);

enum {
	employee_tag_insert, 
	employee_tag_erase, 
	employee_tag_employ,
	employee_tag_fire,
	employee_tag_upgrade,
	employee_tag_lock,
	employee_tag_unlock,
};
membership employee_insert(game_display& disp, hero_map& heros);
membership employee_common(game_display& disp, hero_map& heros, int tag, const std::set<int>& number, int coin, int score, std::map<int, temployee>* employees);
struct temployee {
	temployee():
	  number(HEROS_INVALID_NUMBER)
	  , level(0)
	  , score(0)
	  , uid(0)
	  , lock()
	  , username()
	 {}

	int number;
	int level;
	int score;
	int uid;
	int lock;
	std::string username;
};
std::map<int, temployee> list_employee(game_display& disp, hero_map& heros);
bool do_employ_bh(game_display& disp, hero_map& heros, bool employ, int number, int coin, int score, std::map<int, temployee>* employees);
bool employee_employ(game_display& disp, hero_map& heros, hero& h, int score, std::map<int, temployee>* employees);
bool employee_fire(game_display& disp, hero_map& heros, hero& h, int level, std::map<int, temployee>* employees);

enum {
	signin_tag_fillup, 
};
membership signin(display& disp, hero_map& heros, int tag);

enum {
	subcontinent_tag_repair,
	subcontinent_tag_discard,
};
std::pair<tsubcontinent_record, membership> subcontinent(display& disp, hero_map& heros, int tag, const std::string& id, int cityno);

}
std::string calculate_res_checksum(display& disp, const config& game_config);

#endif
