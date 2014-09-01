/* $Id: replay.cpp 42123 2010-04-13 22:02:24Z crab $ */
/*
   Copyright (C) 2003 - 2010 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 *  @file replay.cpp
 *  Replay control code.
 *
 *  See http://www.wesnoth.org/wiki/ReplayWML for more info.
 */

#include "global.hpp"

#include "dialogs.hpp"
#include "game_display.hpp"
#include "game_end_exceptions.hpp"
#include "game_events.hpp"
#include "game_preferences.hpp"
#include "gamestatus.hpp"
#include "log.hpp"
#include "map_label.hpp"
#include "map_location.hpp"
#include "play_controller.hpp"
#include "replay.hpp"
#include "resources.hpp"
#include "rng.hpp"
#include "statistics.hpp"
#include "wesconfig.h"
#include "artifical.hpp"
#include "formula_string_utils.hpp"
#include "filesystem.hpp"
#include "wml_exception.hpp"
#include "gui/dialogs/preferences.hpp"

#include <boost/foreach.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/gzip.hpp>

command_pool::command_pool()
	: use_gzip_(true)
	, cache_(NULL)
{
	pool_data_ = (uint8_t*)malloc(POOL_ALLOC_DATA_SIZE);
	pool_data_size_ = POOL_ALLOC_DATA_SIZE;
	pool_data_vsize_ = 0;
	pool_data_gzip_size_ = 0;
	pool_pos_ = (unsigned int*)malloc(POOL_ALLOC_POS_SIZE * sizeof(unsigned int));
	pool_pos_size_ = POOL_ALLOC_POS_SIZE;
	pool_pos_vsize_ = 0;

	current_gzip_seg_.min = current_gzip_seg_.max = -1;
}

command_pool::command_pool(const command_pool& that)
{
	use_gzip_ = that.use_gzip_;

	VALIDATE(use_gzip_, "command_pool::operator=, invalid use_gzip_ falg");

	// pool data
	pool_data_size_ = that.pool_data_size_;
	pool_data_vsize_ = that.pool_data_vsize_;
	pool_data_gzip_size_ = that.pool_data_gzip_size_;
	if (pool_data_size_) {
		pool_data_ = (uint8_t*)malloc(pool_data_size_);
	}
	if (pool_data_gzip_size_) {
		memcpy(pool_data_, that.pool_data_, pool_data_gzip_size_);
	}
	// pool pos
	pool_pos_size_ = that.pool_pos_size_;
	pool_pos_vsize_ = that.pool_pos_vsize_;
	if (pool_pos_size_) {
		pool_pos_ = (unsigned int*)malloc(pool_pos_size_ * sizeof(unsigned int));
	}
	if (pool_pos_vsize_) {
		memcpy(pool_pos_, that.pool_pos_, pool_pos_vsize_ * sizeof(unsigned int));
	}
	// cache_
	if (pool_data_vsize_) {
		VALIDATE(false, "command_pool::command_pool, pool_data_vsize_ isn't zero.");
		cache_ = (unsigned char*)malloc(POOL_ALLOC_CACHE_SIZE);
		memcpy(cache_, that.cache_, pool_data_vsize_);
	} else {
		cache_ = NULL;
	}
}

command_pool::~command_pool()
{
	if (pool_data_) {
		free(pool_data_);
	}
	if (pool_pos_) {
		free(pool_pos_);
	}
	if (cache_) {
		free(cache_);
	}
}

command_pool& command_pool::operator=(const command_pool& that)
{
	VALIDATE(use_gzip_ && that.use_gzip_, "command_pool::operator=, invalid use_gzip_ falg");

	use_gzip_ = that.use_gzip_;

	if (pool_data_size_ < that.pool_data_size_) {
		free(pool_data_);
		pool_data_size_ = that.pool_data_size_;
		pool_data_ = (uint8_t*)malloc(pool_data_size_);
	}
	pool_data_vsize_ = that.pool_data_vsize_;
	pool_data_gzip_size_ = that.pool_data_gzip_size_;
	if (pool_data_gzip_size_) {
		memcpy(pool_data_, that.pool_data_, pool_data_gzip_size_);
	}

	if (pool_pos_size_ < that.pool_pos_size_) {
		free(pool_pos_);
		pool_pos_size_ = that.pool_pos_size_;
		pool_pos_ = (unsigned int*)malloc(pool_pos_size_ * sizeof(unsigned int));
	}
	pool_pos_vsize_ = that.pool_pos_vsize_;
	if (pool_pos_vsize_) {
		memcpy(pool_pos_, that.pool_pos_, pool_pos_vsize_ * sizeof(unsigned int));
	}
	// cache
	if (pool_data_vsize_) {
		VALIDATE(false, "command_pool::command_pool, pool_data_vsize_ isn't zero.");
		if (!cache_) {
			cache_ = (unsigned char*)malloc(POOL_ALLOC_CACHE_SIZE);
		}
		memcpy(cache_, that.cache_, pool_data_vsize_);
	}

	return *this;
}

void command_pool::read(unsigned char* mem)
{
	int pool_data_size, pool_data_gzip_size, pool_pos_size, pool_pos_vsize;
	int* ptr = (int*)mem;

	pool_data_size = *ptr;
	ptr = ptr + 1;
	pool_data_gzip_size = *ptr;
	ptr = ptr + 1;
	pool_pos_size = *ptr;
	ptr = ptr + 1;
	pool_pos_vsize = *ptr;
	ptr = ptr + 1;
	
	if (pool_data_size_ < pool_data_size) {
		free(pool_data_);
		pool_data_size_ = pool_data_size;
		pool_data_ = (uint8_t*)malloc(pool_data_size);
	}
	// pool_data_vsize_ = pool_data_vsize;
	pool_data_gzip_size_ = pool_data_gzip_size;
	if (pool_data_gzip_size_) {
		memcpy(pool_data_, (unsigned char*)ptr, pool_data_gzip_size_);
	}

	if (pool_pos_size_ < pool_pos_size) {
		free(pool_pos_);
		pool_pos_size_ = pool_pos_size;
		pool_pos_ = (unsigned int*)malloc(pool_pos_size_ * sizeof(unsigned int));
	}
	pool_pos_vsize_ = pool_pos_vsize;
	if (pool_pos_vsize_) {
		memcpy(pool_pos_, (unsigned char*)ptr + pool_data_gzip_size_, pool_pos_vsize_ * sizeof(unsigned int));
	}
}

void command_pool::clear()
{
	pool_data_vsize_ = 0;
	pool_pos_vsize_ = 0;

	pool_data_gzip_size_ = 0;
	current_gzip_seg_.min = current_gzip_seg_.max = -1;
}

void command_pool::set_use_gzip(bool val) 
{ 
	use_gzip_ = val; 
}

// #define command_addr(pool_pos_index)	((command_pool::command*)(pool_data_ + pool_pos_[pool_pos_index]))

void command_pool::load_for_queue(int pool_pos_index, gzip_segment_t& seg, int& pool_data_gzip_size, unsigned char* dest)
{
	int start;
	// ungzip necessary data to cache
	if (pool_pos_index < current_gzip_seg_.min) {
		// search back
		start = 0;
	} else if (seg.max != -1 && pool_pos_index > seg.max) {
		// search forword
		start = pool_data_gzip_size;
	} else {
		// search from 0 to max
		start = 0;
	}

	memcpy(&seg, pool_data_ + start, sizeof(gzip_segment_t));
	while (pool_pos_index < seg.min || pool_pos_index > seg.max) {
		start += sizeof(gzip_segment_t) + seg.len;
		memcpy(&seg, pool_data_ + start, sizeof(gzip_segment_t));
	}
	pool_data_gzip_size = start;
	gzip_codec(pool_data_ + pool_data_gzip_size + sizeof(gzip_segment_t), seg.len, false, dest);
}

command_pool::command* command_pool::command_addr(int pool_pos_index, bool queue)
{
	if (use_gzip_) {
		if (!cache_) {
			cache_ = (unsigned char*)malloc(POOL_ALLOC_CACHE_SIZE);
		}
		if (queue) {
			if (pool_pos_index < current_gzip_seg_.min || pool_pos_index > current_gzip_seg_.max) {
				load_for_queue(pool_pos_index, current_gzip_seg_, pool_data_gzip_size_, cache_);
			}
		} else {
			// ensure current_gzip_seg.min/max is valid.
			if (current_gzip_seg_.min == -1) {
				current_gzip_seg_.min = pool_pos_vsize_ - 1;
				current_gzip_seg_.max = INT_MAX;
			}
		}
		return (command_pool::command*)(cache_ + pool_pos_[pool_pos_index]);
	} else {
		return (command_pool::command*)(pool_data_ + pool_pos_[pool_pos_index]);
	}
}

command_pool::command* command_pool::command_addr2(int pool_pos_index, gzip_segment_t& seg, int& pool_data_gzip_size, unsigned char* cache)
{
	if (pool_pos_index < seg.min || pool_pos_index > seg.max) {
		load_for_queue(pool_pos_index, seg, pool_data_gzip_size, cache);
	}
	return (command_pool::command*)(cache + pool_pos_[pool_pos_index]);
}

// 返回下一个可以写的地址，同时已把这下个计入在了有效包中
command_pool::command* command_pool::add_command()
{
	if (!use_gzip_) {
		if (pool_data_size_ - pool_data_vsize_ < POOL_COMMAND_RESERVE_BYTES) {
			pool_data_size_ += POOL_ALLOC_DATA_SIZE;
			uint8_t* tmp = (uint8_t*)malloc(pool_data_size_);
			memcpy(tmp, pool_data_, pool_data_vsize_);
			free(pool_data_);
			pool_data_ = tmp;
		}
	}
	
	pool_pos_vsize_ ++;
	if (pool_pos_vsize_ > pool_pos_size_) {
		pool_pos_size_ += POOL_ALLOC_POS_SIZE;
		unsigned int* tmp = (unsigned int*)malloc(pool_pos_size_ * sizeof(unsigned int));
		memcpy(tmp, pool_pos_, (pool_pos_vsize_ - 1) * sizeof(unsigned int));
		free(pool_pos_);
		pool_pos_ = tmp;
	}

	pool_pos_[pool_pos_vsize_ - 1] = pool_data_vsize_;

	return command_addr(pool_pos_vsize_ - 1, false);
}

int command_pool::gzip_codec(unsigned char* data, int data_len, bool encode, unsigned char* to)
{
	int chunk_size = 8192;

	if (!data_len) {
		return 0;
	}

	int len = 0;
	int result_len = 0;
	try {
		std::stringstream gangplank;
		std::iostream ifile(gangplank.rdbuf());
		ifile.write((char*)data, data_len);

		boost::iostreams::filtering_stream<boost::iostreams::input> in;
		if (encode) {
			in.push(boost::iostreams::gzip_compressor());
		} else {
			in.push(boost::iostreams::gzip_decompressor());
		}
		in.push(ifile);

		if (encode) {
			uint8_t* dest = pool_data_ + pool_data_gzip_size_ + sizeof(gzip_segment_t);
			do {
				if (pool_data_size_ - pool_data_gzip_size_ - (int)sizeof(gzip_segment_t) - result_len - len < chunk_size) {
					pool_data_size_ += POOL_ALLOC_DATA_SIZE;
					uint8_t* tmp = (uint8_t*)malloc(pool_data_size_);
					// must copy necessary data. not forget this encoded!
					memcpy(tmp, pool_data_, pool_data_gzip_size_ + sizeof(gzip_segment_t) + result_len + len);
					free(pool_data_);
					// update dest
					dest = tmp + (dest - pool_data_);
					pool_data_ = tmp;
				}
				if (len > 0) {
					result_len += len;
					// pool_data_gzip_size_ += len;
					dest = dest + len;
					if (len < chunk_size) {
						break;
					}
				}
			} while (len = in.read((char*)dest, chunk_size).gcount());

		} else {
			uint8_t* dest = to;
			while (len = in.read((char*)dest, chunk_size).gcount()) {
				result_len += len;
				if (len != chunk_size) {
					break;
				}
				dest = dest + len;
			}
		}

	} catch(io_exception& e) {
		VALIDATE(false, std::string("IO error: ") + e.what());
	}
	return result_len;
}

static lg::log_domain log_replay("replay");
#define DBG_REPLAY LOG_STREAM(debug, log_replay)
#define LOG_REPLAY LOG_STREAM(info, log_replay)
#define WRN_REPLAY LOG_STREAM(warn, log_replay)
#define ERR_REPLAY LOG_STREAM(err, log_replay)

static lg::log_domain log_random("random");
#define DBG_RND LOG_STREAM(debug, log_random)
#define LOG_RND LOG_STREAM(info, log_random)
#define WRN_RND LOG_STREAM(warn, log_random)
#define ERR_RND LOG_STREAM(err, log_random)

std::string replay::last_replay_error;

//functions to verify that the unit structure on both machines is identical

static void verify(const unit_map& units, const config& cfg) {
	std::stringstream errbuf;
	LOG_REPLAY << "verifying unit structure...\n";

	const size_t nunits = lexical_cast_default<size_t>(cfg["num_units"]);
	if(nunits != units.size()) {
		errbuf << "SYNC VERIFICATION FAILED: number of units from data source differ: "
			   << nunits << " according to data source. " << units.size() << " locally\n";

		std::set<map_location> locs;
		BOOST_FOREACH (const config &u, cfg.child_range("unit"))
		{
			const map_location loc(u, resources::state_of_game);
			locs.insert(loc);

			if(units.count(loc) == 0) {
				errbuf << "data source says there is a unit at "
					   << loc << " but none found locally\n";
			}
		}

		for(unit_map::const_iterator j = units.begin(); j != units.end(); ++j) {
			if(locs.count(j->get_location()) == 0) {
				errbuf << "local unit at " << j->get_location()
					   << " but none in data source\n";
			}
		}
		replay::process_error(errbuf.str());
		errbuf.clear();
	}

	BOOST_FOREACH (const config &un, cfg.child_range("unit"))
	{
		const map_location loc(un, resources::state_of_game);
		const unit_map::const_iterator u = units.find(loc);
		if(u == units.end()) {
			errbuf << "SYNC VERIFICATION FAILED: data source says there is a '"
				   << un["type"] << "' (side " << un["side"] << ") at "
				   << loc << " but there is no local record of it\n";
			replay::process_error(errbuf.str());
			errbuf.clear();
		}

		config cfg;
		u->write(cfg);

		bool is_ok = true;
		static const std::string fields[] = {"type","hitpoints","experience","side",""};
		for(const std::string* str = fields; str->empty() == false; ++str) {
			if (cfg[*str] != un[*str]) {
				errbuf << "ERROR IN FIELD '" << *str << "' for unit at "
					   << loc << " data source: '" << un[*str]
					   << "' local: '" << cfg[*str] << "'\n";
				is_ok = false;
			}
		}

		if(!is_ok) {
			errbuf << "(SYNC VERIFICATION FAILED)\n";
			replay::process_error(errbuf.str());
			errbuf.clear();
		}
	}

	LOG_REPLAY << "verification passed\n";
}

// if want to unexpected end replay, for example debug, append below tow statement.
// 2: must return false.
// get_replay_source().unexpected = true;
// return false;

// FIXME: this one now has to be assigned with set_random_generator
// from play_level or similar.  We should surely hunt direct
// references to it from this very file and move it out of here.
replay recorder;

replay::replay() 
	: cfg_()
	, build_cfg_()
	, pos_(0)
	, current_(NULL)
	, skip_(false)
	, message_locations()
	, expected_advancements_()
	, unexpected(false)
{}

replay::replay(const command_pool& pool) 
	: command_pool(pool)
	, cfg_()
	, build_cfg_()
	, pos_(0)
	, current_(NULL)
	, skip_(false)
	, message_locations()
	, expected_advancements_()
	, unexpected(false)
{
}


// [write] network receiver. push cfg to local pool
void replay::append(const config& cfg)
{
	config::const_child_itors cmds = cfg.child_range("command");
	// BOOST_FOREACH (const config& t, cmds) {
	BOOST_FOREACH (config t, cmds) {
		if (!t.empty()) {
			// <attention 2>
			if (t.child("speak")) {
				t["type"] = str_cast(command_pool::SPEAK);
			}

			command_pool::command* cmd = command_pool::add_command();
			config_2_command(t, cmd);
		}
	}
}

void replay::process_error(const std::string& msg)
{
	ERR_REPLAY << msg;

	resources::controller->process_oos(msg); // might throw end_level_exception(QUIT)
}

void replay::throw_error(const std::string& msg)
{
	ERR_REPLAY << msg;
	last_replay_error = msg;
	throw replay::error(msg);
}

void replay::set_skip(bool skip)
{
	skip_ = skip;
}

bool replay::is_skipping() const
{
	return skip_;
}

void replay::add_start()
{
	config* const cmd = add_command(true);
	(*cmd)["type"] = command_pool::START;

	cmd->add_child("start");
}

void replay::add_employ(hero& h, int cost)
{
	config* const cmd = add_command(true);
	(*cmd)["type"] = command_pool::EMPLOY;

	config val;
	val["hero"] = h.number_;
	val["cost"] = cost;
	cmd->add_child("employ", val);
}

void replay::add_purchase(int type, int cost)
{
	config* const cmd = add_command(true);
	(*cmd)["type"] = command_pool::PURCHASE;

	config val;
	val["type"] = type;
	val["cost"] = cost;
	cmd->add_child("purchase", val);
}

void replay::add_recruit(const std::string& type, const map_location& loc, std::vector<const hero*>& troop_heros, int cost, bool human)
{
	config* const cmd = add_command();

	(*cmd)["type"] = command_pool::RECRUIT;

	config val;

	val["type"] = type;
	std::stringstream str;
	// heros_army
	str << troop_heros.front()->number_;
	for (std::vector<const hero*>::const_iterator itor = troop_heros.begin() + 1; itor != troop_heros.end(); ++ itor) {
		str << "," << (*itor)->number_;
	}
	val["heros"] = str.str();
	val["cost"] = cost;
	val["human"] = human? 1: 0;

	loc.write(val);

	cmd->add_child("recruit",val);
}

void replay::add_move_heros(const map_location& src, const map_location& dst, std::set<size_t>& heros)
{
	config* const cmd = add_command();
	(*cmd)["type"] = str_cast(command_pool::MOVE_HEROS);

	config val;

	std::stringstream str;
	// move heros
	std::set<size_t>::const_iterator itor = heros.begin();
	str << *itor;
	for (++ itor; itor != heros.end(); ++ itor) {
		str << "," << *itor;
	}
	val["heros"] = str.str();

	str.str("");
	str << src;
	val["src"] = str.str();

	str.str("");
	str << dst;
	val["dst"] = str.str();

	cmd->add_child("move_heros",val);
}

void replay::add_interior(const artifical& city)
{
	config* const cmd = add_command();
	(*cmd)["type"] = command_pool::INTERIOR;

	config val;

	val["mayor"] = city.mayor()->number_;
	city.get_location().write(val);

	cmd->add_child("interior", val);
}

void replay::add_belong_to(const unit* u, const artifical* to, bool loyalty)
{
	config* const cmd = add_command();
	(*cmd)["type"] = command_pool::BELONG_TO;

	config val;

	val["layer"] = u->base()? unit_map::BASE: unit_map::OVERLAY;
	val["loyalty"] = loyalty? 1: 0;
	std::stringstream str;
	str << u->get_location();
	val["src"] = str.str();

	str.str("");
	str << to->get_location();
	val["dst"] = str.str();

	cmd->add_child("belong_to", val);
}

void replay::add_set_states(const unit& u, const std::map<ustate_tag::state_t, bool>& states)
{
	config* const cmd = add_command();
	(*cmd)["type"] = command_pool::SET_STATES;

	std::stringstream strstr;
	config val;

	val["layer"] = u.base()? unit_map::BASE: unit_map::OVERLAY;
	strstr.str("");
	strstr << u.get_location();
	val["loc"] = strstr.str();

	strstr.str("");
	for (std::map<ustate_tag::state_t, bool>::const_iterator it = states.begin(); it != states.end(); ++ it) {
		if (it != states.begin()) {
			strstr << ", ";
		}
		int key = it->first;
		strstr << key << ", " << (it->second? 1: 0);
	}
	val["states"] = strstr.str();

	cmd->add_child("set_states", val);
}

void replay::add_clear_formationed(const unit& u, int cost)
{
	config* const cmd = add_command();
	(*cmd)["type"] = command_pool::CLEAR_FORMATIONED;

	config val;
	std::stringstream strstr;

	val["layer"] = u.base()? unit_map::BASE: unit_map::OVERLAY;
	strstr.str("");
	strstr << u.get_location();
	val["loc"] = strstr.str();

	val["cost"] = cost;

	cmd->add_child("clear_formationed", val);
}

void replay::add_set_task(const unit& u, int task, const map_location& at)
{
	config* const cmd = add_command();
	(*cmd)["type"] = command_pool::SET_TASK;

	std::stringstream strstr;
	config val;

	val["hero"] = u.master().number_;
	strstr.str("");
	strstr << u.get_location();
	val["loc"] = strstr.str();

	val["task"] = task;
	strstr.str("");
	strstr << at;
	val["at"] = strstr.str();

	cmd->add_child("set_task", val);
}

/*
content of build packet
--------------------------
[command]
	[build]
		artifical="25,4"
		builder="26,3"
		type="market"
	[/build]
	[random]
		value="17398"
	[/random]
	[random]
		value="13234"
	[/random]
[/command]
-------------------------
*/
void replay::add_build_begin(const std::string& type)
{
	build_cfg_.clear();
	set_random(&build_cfg_);

	config val;
	val["type"] = type;
	build_cfg_.add_child("build", val);
}

void replay::add_build_cancel()
{
	set_random(current_);
}

void replay::add_build(const unit_type* type, const map_location& city_loc, const map_location& builder_loc, const map_location& art_loc, int cost)
{
	config* const cmd = add_command();
	(*cmd)["type"] = str_cast(command_pool::BUILD);

	config build;

	build["type"] = type->id();

	std::stringstream str;
	str << city_loc;
	build["city"] = str.str();

	str.str("");
	str << builder_loc;
	build["builder"] = str.str();

	str.str("");
	str << art_loc;
	build["artifical"] = str.str();

	build["cost"] = cost;

	cmd->add_child("build", build);
}

void replay::add_cast_tactic(const unit& tactician, const hero& h, const unit* special, bool consume, int cost)
{
	config* const cmd = add_command();
	(*cmd)["type"] = str_cast(command_pool::CAST_TACTIC);

	config val;

	std::stringstream strstr;
	strstr << tactician.get_location();
	val["tactician"] = strstr.str();

	strstr.str("");
	if (special) {
		strstr << special->get_location();
	} else {
		strstr << map_location();
	}
	val["special"] = strstr.str();

	val["hero"] = h.number_;
	val["consume"] = consume? 1: 0;
	val["cost"] = cost;

	cmd->add_child("cast_tactic", val);
}

void replay::add_disband(int value, const map_location& loc, int income)
{
	config* const cmd = add_command();
	(*cmd)["type"] = command_pool::DISBAND;

	config val;

	val["value"] = value;
	val["income"] = income;

	loc.write(val);

	cmd->add_child("disband", val);
}

void replay::add_armory(const map_location& loc, const std::vector<size_t>& diff)
{
	config* const cmd = add_command();
	(*cmd)["type"] = str_cast(command_pool::ARMORY);

	config val;

	std::stringstream str;
	size_t index;
	unit* u;
	std::vector<size_t> captains;

	// rearmed reside troops
	artifical* city = resources::units->city_from_loc(loc);
	std::vector<unit*>& reside_troops = city->reside_troops();

	std::vector<size_t>::const_iterator itor = diff.begin();
	index = *itor;
	u = reside_troops[index];
	captains.push_back(u->master().number_);
	if (u->second().valid()) captains.push_back(u->second().number_);
	if (u->third().valid()) captains.push_back(u->third().number_);
	str << index << "," << captains.size() << "," << captains[0];
	if (captains.size() > 1) {
		str << "," << captains[1];
	}
	if (captains.size() > 2) {
		str << "," << captains[2];
	}
	for (++ itor; itor != diff.end(); ++ itor) {
		index = *itor;
		u = reside_troops[index];
		captains.clear();
		captains.push_back(u->master().number_);
		if (u->second().valid()) captains.push_back(u->second().number_);
		if (u->third().valid()) captains.push_back(u->third().number_);
		str << "," << index << "," << captains.size() << "," << captains[0];
		if (captains.size() > 1) {
			str << "," << captains[1];
		}
		if (captains.size() > 2) {
			str << "," << captains[2];
		}
	}
	val["diff"] = str.str();

	str.str("");
	str << loc;
	val["city"] = str.str();

	cmd->add_child("armory", val);
}

void replay::add_diplomatism(const std::string& type, int first_side, int second_side, bool alignment, int emissary, int target_side, int strategy)
{
	config* const cmd = add_command();
	(*cmd)["type"] = command_pool::DIPLOMATISM;

	config val;

	val["type"] = type;
	val["alignment"] = alignment? 1: 0;
	val["hero"] = emissary;
	val["first"] = first_side;
	val["second"] = second_side;
	val["target"] = target_side;
	val["strategy"] = strategy;

	cmd->add_child("diplomatism", val);
}

void replay::add_final_battle(artifical* human, artifical* ai)
{
	config* const cmd = add_command();
	(*cmd)["type"] = str_cast(command_pool::FINAL_BATTLE);

	config val;

	val["human"] = human->cityno();
	val["ai"] = ai? ai->cityno(): 0;

	cmd->add_child("final_battle", val);
}

void replay::add_card(int side, size_t number, bool dialog)
{
	config* const cmd = add_command();
	(*cmd)["type"] = str_cast(command_pool::ADD_CARD);

	config val;

	val["side"] = side;
	val["number"] = (int)number;
	val["dialog"] = dialog? 1: 0;

	cmd->add_child("add_card", val);
}

void replay::erase_card(int side, int index, const map_location& loc, std::vector<std::pair<int, unit*> >& touched, bool dialog)
{
	config* const cmd = add_command();
	(*cmd)["type"] = command_pool::ERASE_CARD;

	config val;

	std::stringstream str;
	// touched heros
	if (!touched.empty()) {
		std::vector<std::pair<int, unit*> >::const_iterator it = touched.begin();
		str << it->first << "," << it->second->get_location().x << "," << it->second->get_location().y;
		for (++ it; it != touched.end(); ++ it) {
			str << "," << it->first << "," << it->second->get_location().x << "," << it->second->get_location().y;
		}
	}
	val["touched"] = str.str();

	val["side"] = side;
	val["index"] = index;
	val["dialog"] = dialog? 1: 0;
	loc.write(val);

	cmd->add_child("erase_card", val);
}

void replay::add_reform_captain(const unit& u, const hero& h, bool join)
{
	config* const cmd = add_command();
	(*cmd)["type"] = command_pool::REFORM_CAPTAIN;

	config val;

	val["number"] = h.number_;
	val["join"] = join? 1: 0;
	u.get_location().write(val);

	cmd->add_child("reform_captain", val);
}

void replay::active_tactic(bool add, const unit& u, const hero& h)
{
	config* const cmd = add_command();
	(*cmd)["type"] = command_pool::ACTIVE_TACTIC;

	config val;
	val["add"] = add? 1: 0;
	val["unit"] = u.master().number_;
	val["hero"] = h.number_;

	cmd->add_child("active_tactic", val);
}

void replay::add_countdown_update(int value, int team)
{
	config* const cmd = add_command();
	config val;

	val["value"] = lexical_cast_default<std::string>(value);
	val["team"] = lexical_cast_default<std::string>(team);

	cmd->add_child("countdown_update",val);
}


void replay::add_movement(const std::vector<map_location>& steps, bool direct, int cost)
{
	if (steps.empty()) { // no move, nothing to record
		return;
	}
	VALIDATE(!direct || steps.size() == 2, "replay::add_movement, direct mode, but step size isn't 2!");

	config* const cmd = add_command();
	(*cmd)["type"] = command_pool::MOVEMENT;

	config move;
	move["direct"] = direct? 1: 0;
	move["cost"] = cost;
	write_locations(steps, move);

	cmd->add_child("move",move);
}

void replay::add_expedite(bool troop, int index, const std::vector<map_location>& steps, bool zero, bool direct)
{
	if (steps.empty()) { // no move, nothing to record
		return;
	}
	VALIDATE(!direct || steps.size() == 2, "replay::add_expedite, direct mode, but step size isn't 2!");

	config* const cmd = add_command();
	(*cmd)["type"] = command_pool::EXPEDITE;

	config expedite;

	expedite["troop"] = troop? 1: 0;
	expedite["zero"] = zero? 1: 0;
	expedite["value"] = index;
	expedite["direct"] = direct? 1: 0;
	
	write_locations(steps, expedite);

	cmd->add_child("expedite", expedite);
}

void replay::add_bomb(const team& t)
{
	config* const cmd = add_command();
	(*cmd)["type"] = command_pool::BOMB;

	config bomb;
	bomb["side"] = t.side();

	cmd->add_child("bomb", bomb);
}

void replay::add_attack(const unit& a, const unit& b,
	int att_weapon, int def_weapon, const std::string& attacker_type_id,
	const std::string& defender_type_id, int attacker_lvl,
	int defender_lvl, const size_t turn, const time_of_day t, bool move, bool constant_attacks)
{
	const map_location& a_loc = a.get_location();
	const map_location& b_loc = b.get_location();

	add_pos("attack", a_loc, b_loc);
	config &cfg = current_->child("attack");
	(*current_)["type"] = command_pool::ATTACK;

	cfg["weapon"] = att_weapon;
	cfg["defender_weapon"] = def_weapon;
	cfg["layer"] = a.base()? unit_map::BASE: unit_map::OVERLAY;
	cfg["defender_layer"] = b.base()? unit_map::BASE: unit_map::OVERLAY;
	cfg["attacker_lvl"] = str_cast(attacker_lvl);
	cfg["defender_lvl"] = str_cast(defender_lvl);
	cfg["turn"] = turn;
	cfg["move"] = move? 1: 0;
	cfg["constant_attacks"] = constant_attacks? 1: 0;
}

void replay::add_formation_attack(const map_location& master_loc)
{
	config* const cmd = add_command();
	(*cmd)["type"] = command_pool::FORMATION_ATTACK;

	std::stringstream strstr;
	config val;

	strstr.str("");
	strstr << master_loc;
	val["master"] = strstr.str();

	cmd->add_child("formation_attack", val);
}

void replay::add_duel(const hero& left, const hero& right, int percentage)
{
	config& duel = current_->child("attack").add_child("duel");
	duel["left"] = left.number_;
	duel["right"] = right.number_;
	duel["percentage"] = percentage;
}

void replay::add_seed(const char* child_name, rand_rng::seed_t seed)
{
	LOG_REPLAY << "Setting seed for child type " << child_name << ": " << seed << "\n";
	current_->child(child_name)["seed"] = lexical_cast<std::string>(seed);
}

void replay::add_pos(const std::string& type,
                     const map_location& a, const map_location& b)
{
	config* const cmd = add_command();

	config move, src, dst;
	a.write(src);
	b.write(dst);

	move.add_child("source",src);
	move.add_child("destination",dst);
	cmd->add_child(type,move);
}

void replay::add_value(const std::string& type, int value)
{
	config* const cmd = add_command();

	config val;

	char buf[100];
	snprintf(buf,sizeof(buf),"%d",value);
	val["value"] = buf;

	cmd->add_child(type,val);
}

void replay::choose_option(int index)
{
	add_value("choose",index);
	(*current_)["type"] = str_cast(command_pool::CHOOSE);
}

void replay::text_input(std::string input)
{
	config* const cmd = add_command();

	config val;
	val["text"] = input;

	cmd->add_child("input",val);
}

void replay::add_label(const terrain_label* label)
{
	assert(label);
	config* const cmd = add_command(false);

	config val;

	label->write(val);

	cmd->add_child("label",val);
}

void replay::user_input(const std::string &name, const config &input)
{
	config* const cmd = add_command();
	(*cmd)["type"] = str_cast(command_pool::INPUT);

	cmd->add_child(name, input);
}

void replay::clear_labels(const std::string& team_name)
{
	config* const cmd = add_command();
	(*cmd)["type"] = str_cast(command_pool::CLEAR_LABELS);

	config val;
	val["team_name"] = team_name;
	cmd->add_child("clear_labels",val);
}

void replay::add_rename(const std::string& name, const map_location& loc)
{
	config* const cmd = add_command(false);
	(*cmd)["async"] = "yes"; // Not undoable, but depends on moves/recruits that are
	config val;
	loc.write(val);
	val["name"] = name;
	cmd->add_child("rename", val);
}

void replay::init_side(const team& current_team)
{
	config* const cmd = add_command();
	(*cmd)["type"] = command_pool::INIT_SIDE;

	const std::vector<strategy>& strategies = current_team.strategies();
	std::stringstream strstr;
	for (std::vector<strategy>::const_iterator it = strategies.begin(); it != strategies.end(); ++ it) {
		const strategy& s = *it;
		if (!strstr.str().empty()) {
			strstr << ", ";
		}
		strstr << s.target_ << ", " << s.type_;
	}

	config val;
	val["strategies"] = strstr.str();

	cmd->add_child("init_side", val);
}

void replay::end_turn()
{
	config* const cmd = add_command();
	(*cmd)["type"] = command_pool::END_TURN;
	cmd->add_child("end_turn");
}

void replay::init_ai()
{
	config* const cmd = add_command();
	(*cmd)["type"] = command_pool::INIT_AI;

	cmd->add_child("init_ai");
}

void replay::add_prefix_unit(int side, bool new_turn, int end)
{
	config* const cmd = add_command();
	(*cmd)["type"] = command_pool::PREFIX_UNIT;

	config val;
	val["side"] = side;
	val["new_turn"] = new_turn? 1: 0;
	val["end"] = end;

	cmd->add_child("prefix_unit", val);
}

void replay::add_post_unit()
{
	config* const cmd = add_command();
	(*cmd)["type"] = command_pool::POST_UNIT;

	config val;
	val["past"] = 0; // current, it is not used.

	cmd->add_child("post_unit", val);
}

void replay::add_fresh_heros()
{
	config* const cmd = add_command();
	(*cmd)["type"] = command_pool::FRESH_HEROS;

	cmd->add_child("fresh_heros");
}

void replay::add_do_commoner()
{
	config* const cmd = add_command();
	(*cmd)["type"] = command_pool::DO_COMMONER;
	
	cmd->add_child("do_commoner");
}

void replay::add_inching_block(const unit& u, bool increase)
{
	config* const cmd = add_command();
	(*cmd)["type"] = command_pool::INCHING_BLOCK;

	config val;
	val["increase"] = increase? 1: 0;
	u.get_location().write(val);
	cmd->add_child("inching_block", val);
}

void replay::add_event(int type, const map_location& loc)
{
	config* const cmd = add_command();
	(*cmd)["type"] = command_pool::EVENT;

	config val;
	val["type"] = type;
	loc.write(val);
	cmd->add_child("event", val);
}

void replay::add_assemble_treasure(const std::map<int, int>& diff)
{
	config* const cmd = add_command();
	(*cmd)["type"] = command_pool::ASSEMBLE_TREASURE;

	config val;
	std::stringstream str;
	// diff
	std::map<int, int>::const_iterator it = diff.begin();
	str << it->first << "," << it->second;
	for (++ it; it != diff.end(); ++ it) {
		str << "," << it->first << "," << it->second;
	}
	val["diff"] = str.str();

	cmd->add_child("assemble_treasure", val);
}

void replay::add_appoint_noble(const std::map<int, int>& diff)
{
	config* const cmd = add_command();
	(*cmd)["type"] = command_pool::APPOINT_NOBLE;

	config val;
	std::stringstream str;
	// diff
	std::map<int, int>::const_iterator it = diff.begin();
	str << it->first << "," << it->second;
	for (++ it; it != diff.end(); ++ it) {
		str << "," << it->first << "," << it->second;
	}
	val["diff"] = str.str();

	cmd->add_child("appoint_noble", val);
}

void replay::add_rpg_exchange(const std::set<size_t>& checked_human, size_t checked_ai)
{
	config* const cmd = add_command();
	(*cmd)["type"] = command_pool::RPG_EXCHANGE;

	config val;

	std::stringstream str;
	std::set<size_t>::const_iterator itor = checked_human.begin();
	int number = *itor;
	str << number;
	for (++ itor; itor != checked_human.end(); ++ itor) {
		number = *itor;
		str << "," << number;
	}
	val["human"] = str.str();
	val["ai"] = (int)checked_ai;

	cmd->add_child("rpg_exchange", val);
}

void replay::add_ing_technology(const std::string& id)
{
	config* const cmd = add_command();
	(*cmd)["type"] = command_pool::ING_TECHNOLOGY;

	config val;
	val["id"] = id;

	cmd->add_child("ing_technology", val);
}

void replay::add_scenario_env(const tscenario_env& env)
{
	config* const cmd = add_command();
	(*cmd)["type"] = command_pool::SCENARIO_ENV;

	config val;
	val["duel"] = env.duel;
	val["maximal_defeated_activity"] = env.maximal_defeated_activity;
	val["vip"] = env.vip? 1: 0;

	cmd->add_child("scenario_env", val);
}

void replay::add_log_data(const std::string &key, const std::string &var)
{

}

void replay::add_log_data(const std::string &category, const std::string &key, const std::string &var)
{
/*
	config& ulog = cfg_.child_or_add("upload_log");
	config& cat = ulog.child_or_add(category);
	cat[key] = var;
*/
}

void replay::add_log_data(const std::string &category, const std::string &key, const config &c)
{
/*	
	config& ulog = cfg_.child_or_add("upload_log");
	config& cat = ulog.child_or_add(category);
	cat.add_child(key,c);
*/
}

void replay::add_expected_advancement(unit* u)
{
	expected_advancements_.push_back(u);
}

const std::deque<unit*>& replay::expected_advancements() const
{
	return expected_advancements_;
}

void replay::pop_expected_advancement()
{
	expected_advancements_.pop_front();
}

void replay::add_chat_message_location()
{
	message_locations.push_back(pos_-1);
}

void replay::speak(const config& cfg)
{
	config* const cmd = add_command();
	if(cmd != NULL) {
		cmd->add_child("speak",cfg);
		(*cmd)["type"] = str_cast(command_pool::SPEAK);
		add_chat_message_location();
	}
}

void replay::add_chat_log_entry(const config &cfg, std::ostream &str) const
{
	if (!cfg) return;

	if (!preferences::parse_should_show_lobby_join(cfg["id"], cfg["message"])) return;
	if (preferences::is_ignored(cfg["id"])) return;
	const std::string& team_name = cfg["team_name"];
	if(team_name == "") {
		str << "<" << cfg["id"] << "> ";
	} else {
		str << "*" << cfg["id"] << "* ";
	}
	str << cfg["message"] << "\n";
}

// [write]
void replay::remove_command(int index)
{
	if (!pos_ || index != -1) {
		// only allow remove last command
		posix_print_mb("replay::remove_command, only allow remove last command!");
		return;
	}
	cfg_.clear();
	// notice: this pos_ is -1 from current used.
	pos_ = pool_pos_vsize_;

	index = pos_;

	std::vector<int>::reverse_iterator loc_it;
	for (loc_it = message_locations.rbegin(); loc_it != message_locations.rend() && index < *loc_it;++loc_it)
	{
		--(*loc_it);
	}
}

// cached message log
std::stringstream message_log;


std::string replay::build_chat_log()
{
/*
	config cmd;
	std::vector<int>::iterator loc_it;
	int last_location = 0;
	for (loc_it = message_locations.begin(); loc_it != message_locations.end(); ++loc_it) {
		last_location = *loc_it;
		if (last_location != pos_ - 1) {
			cmd.clear();
			command_2_config(command_addr(last_location), cmd); 
		} else {
			cmd = cfg_;
		}
		const config &speak = cmd.child("speak");
		add_chat_log_entry(speak, message_log);

	}
	message_locations.clear();
*/
	return message_log.str();
}

// [read] network
config replay::get_data_range(int cmd_start, int cmd_end, DATA_TYPE data_type)
{
	config res;
	if ((data_type != ALL_DATA) || (cmd_start >= cmd_end)) {
		return res;
	}
	int cmd_end_from_pool = (cmd_end > pool_pos_vsize_)? cmd_end - 1: pool_pos_vsize_;
	while (cmd_start < cmd_end_from_pool) {
		if (!(command_addr(cmd_start)->flags & 1)) {
			config& cmd_cfg = res.add_child("command");
			command_2_config(command_addr(cmd_start), cmd_cfg);
			cmd_cfg["type"] = str_cast(command_addr(cmd_start)->type);
		}
		++ cmd_start;
	}
	if (cmd_end > pool_pos_vsize_) {
		if (cfg_["sent"] != "yes") {
			res.add_child("command", cfg_);
		}
	}

	return res;
}

void replay::undo()
{
	if (const config &child = cfg_.child("move")) {
		// A unit's move is being undone.
		// Repair unsynced cmds whose locations depend on that unit's location.
		const std::vector<map_location> steps =
				parse_location_range(resources::game_map, child["x"], child["y"]);

		if (steps.empty()) {
			replay::process_error("undo(), trying to undo a move using an empty path");
			return; // nothing to do, I suppose.
		}
	}
	int type = cfg_["type"].to_int();
	if (cfg_.empty()) {
		replay::process_error("undo(), unexpected cfg_, cfg_ is empty");
	} else if ((type != command_pool::EXPEDITE) && (type != command_pool::MOVEMENT)) {
		replay::process_error("undo(), unexpected type: " + cfg_["type"].str());
	}

	remove_command(-1);
	current_ = NULL;
	set_random(NULL);
}

// Only for network sender. Other scenario don't use it, use pool_pos_vsize_ directly.
int replay::ncommands()
{
	return pool_pos_vsize_ + (cfg_.empty()? 0: 1);
}

config* replay::add_command(bool update_random_context)
{
	// 把一条命令加入恢复池
	if (!cfg_.empty()) {
		command_pool::command* cmd = command_pool::add_command();
		config_2_command(cfg_, cmd);
		if (cfg_["sent"] == "yes") {
			cmd->flags = 1;
		}
	}

	pos_ = pool_pos_vsize_ + 1;
	cfg_.clear();
	current_ = &cfg_;
	if (update_random_context)
		set_random(current_);

	return current_;
}

void replay::config_2_command(const config& cfg, command_pool::command* cmd)
{
	int type = lexical_cast_default<int>(cfg["type"], 0);
	std::vector<map_location> steps;
	size_t i, size;

	if (type == command_pool::START) {
		cmd->type = (command_pool::TYPE)type;
		int* ptr = (int*)(cmd + 1);
		size = *ptr = cfg.child_count("random");
		ptr = ptr + 1; 
		for (i = 0; i < size; i ++) {
			const config& random = cfg.child("random", i);
			*ptr = random["value"].to_int();
			ptr = ptr + 1;
		}
		const config& child = cfg.child("start");

		cmd->flags = 0;
		pool_data_vsize_ += sizeof(command_pool::command) + (int)((uint8_t*)ptr - (uint8_t*)(cmd + 1));

	} else if (type == command_pool::EMPLOY) {
		cmd->type = (command_pool::TYPE)type;
		int* ptr = (int*)(cmd + 1);
		size = *ptr = cfg.child_count("random");
		ptr = ptr + 1; 
		for (i = 0; i < size; i ++) {
			const config& random = cfg.child("random", i);
			*ptr = random["value"].to_int();
			ptr = ptr + 1;
		}
		const config& child = cfg.child("employ");
		*ptr = child["hero"].to_int();
		ptr = ptr + 1;
		*ptr = child["cost"].to_int();
		ptr = ptr + 1;

		cmd->flags = 0;
		pool_data_vsize_ += sizeof(command_pool::command) + (int)((uint8_t*)ptr - (uint8_t*)(cmd + 1));

	} else if (type == command_pool::PURCHASE) {
		cmd->type = (command_pool::TYPE)type;
		int* ptr = (int*)(cmd + 1);
		size = *ptr = cfg.child_count("random");
		ptr = ptr + 1; 
		for (i = 0; i < size; i ++) {
			const config& random = cfg.child("random", i);
			*ptr = random["value"].to_int();
			ptr = ptr + 1;
		}
		const config& child = cfg.child("purchase");
		*ptr = child["type"].to_int();
		ptr = ptr + 1;
		*ptr = child["cost"].to_int();
		ptr = ptr + 1;

		cmd->flags = 0;
		pool_data_vsize_ += sizeof(command_pool::command) + (int)((uint8_t*)ptr - (uint8_t*)(cmd + 1));

	} else if (type == command_pool::INIT_SIDE) {
		cmd->type = (command_pool::TYPE)type;
		int* ptr = (int*)(cmd + 1);
		size = *ptr = cfg.child_count("random");
		ptr = ptr + 1; 
		for (i = 0; i < size; i ++) {
			const config& random = cfg.child("random", i);
			*ptr = random["value"].to_int();
			ptr = ptr + 1;

			const config& results = random.child("results", 0);
			if (!results) {
				*ptr = INT_MAX;
				ptr = ptr + 1;
				continue;
			}
			// [results].choose
			*ptr = lexical_cast_default<int>(results["choose"], 0);
			ptr = ptr + 1;
		}
		const config& child = cfg.child("init_side");

		// strategies="1,0,2,3"
		size = *ptr = child["strategies"].str().size();
		ptr = ptr + 1;
		memcpy(ptr, child["strategies"].str().c_str(), size);
		*((char*)ptr + size) = '\0';
		ptr = ptr + (size + 1 + 3) / 4; // align 4, 1 is '\0'

		cmd->flags = 0;
		pool_data_vsize_ += sizeof(command_pool::command) + (int)((uint8_t*)ptr - (uint8_t*)(cmd + 1));

	} else if (type == command_pool::END_TURN) {
		cmd->type = (command_pool::TYPE)type;
		int* ptr = (int*)(cmd + 1);
		size = *ptr = cfg.child_count("random");
		ptr = ptr + 1; 
		for (i = 0; i < size; i ++) {
			*ptr = cfg.child("random", i)["value"].to_int();
			ptr = ptr + 1; 
		}
		const config& child = cfg.child("end_turn");
		cmd->flags = 0;
		pool_data_vsize_ += sizeof(command_pool::command) + (int)((uint8_t*)ptr - (uint8_t*)(cmd + 1));

	} else if (type == command_pool::INIT_AI) {
		cmd->type = (command_pool::TYPE)type;
		int* ptr = (int*)(cmd + 1);
		size = *ptr = cfg.child_count("random");
		ptr = ptr + 1; 
		for (i = 0; i < size; i ++) {
			*ptr = cfg.child("random", i)["value"].to_int();
			ptr = ptr + 1; 
		}
		const config& child = cfg.child("init_ai");
		cmd->flags = 0;
		pool_data_vsize_ += sizeof(command_pool::command) + (int)((uint8_t*)ptr - (uint8_t*)(cmd + 1));

	} else if (type == command_pool::PREFIX_UNIT) {
		cmd->type = (command_pool::TYPE)type;
		int* ptr = (int*)(cmd + 1);
		size = *ptr = cfg.child_count("random");
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			const config& random = cfg.child("random", i);
			*ptr = random["value"].to_int();
			ptr = ptr + 1;

			const config& results = random.child("results", 0);
			if (!results) {
				*ptr = INT_MAX;
				ptr = ptr + 1;
				continue;
			}
			// [results].choose
			*ptr = lexical_cast_default<int>(results["choose"], 0);
			ptr = ptr + 1;
		}
		const config& child = cfg.child("prefix_unit");
		*ptr = child["side"].to_int();
		ptr = ptr + 1;
		*ptr = child["new_turn"].to_int();
		ptr = ptr + 1;
		*ptr = child["end"].to_int();
		ptr = ptr + 1; 

		cmd->flags = 0;
		pool_data_vsize_ += sizeof(command_pool::command) + (int)((uint8_t*)ptr - (uint8_t*)(cmd + 1));

	} else if (type == command_pool::POST_UNIT) {
		cmd->type = (command_pool::TYPE)type;
		int* ptr = (int*)(cmd + 1);
		size = *ptr = cfg.child_count("random");
		ptr = ptr + 1; 
		for (i = 0; i < size; i ++) {
			const config& random = cfg.child("random", i);
			*ptr = random["value"].to_int();
			ptr = ptr + 1;

			const config& results = random.child("results", 0);
			if (!results) {
				*ptr = INT_MAX;
				ptr = ptr + 1;
				continue;
			}
			// [results].choose
			*ptr = lexical_cast_default<int>(results["choose"], 0);
			ptr = ptr + 1;
		}
		const config& child = cfg.child("post_unit");
		*ptr = child["past"].to_int();
		ptr = ptr + 1; 

		cmd->flags = 0;
		pool_data_vsize_ += sizeof(command_pool::command) + (int)((uint8_t*)ptr - (uint8_t*)(cmd + 1));

	} else if (type == command_pool::FRESH_HEROS) {
		cmd->type = (command_pool::TYPE)type;
		int* ptr = (int*)(cmd + 1);
		size = *ptr = cfg.child_count("random");
		ptr = ptr + 1; 
		for (i = 0; i < size; i ++) {
			*ptr = cfg.child("random", i)["value"].to_int();
			ptr = ptr + 1; 
		}
		const config& child = cfg.child("fresh_heros");
		cmd->flags = 0;
		pool_data_vsize_ += sizeof(command_pool::command) + (int)((uint8_t*)ptr - (uint8_t*)(cmd + 1));

	} else if (type == command_pool::DO_COMMONER) {
		cmd->type = (command_pool::TYPE)type;
		int* ptr = (int*)(cmd + 1);
		size = *ptr = cfg.child_count("random");
		ptr = ptr + 1; 
		for (i = 0; i < size; i ++) {
			const config& random = cfg.child("random", i);
			*ptr = random["value"].to_int();
			ptr = ptr + 1;

			const config& results = random.child("results", 0);
			if (!results) {
				*ptr = INT_MAX;
				ptr = ptr + 1;
				continue;
			}
			// [results].choose
			*ptr = lexical_cast_default<int>(results["choose"], 0);
			ptr = ptr + 1;
		}
		const config& child = cfg.child("do_commoner");

		cmd->flags = 0;
		pool_data_vsize_ += sizeof(command_pool::command) + (int)((uint8_t*)ptr - (uint8_t*)(cmd + 1));

	} else if (type == command_pool::RECRUIT) {
		cmd->type = (command_pool::TYPE)type;
		int* ptr = (int*)(cmd + 1);
		size = *ptr = cfg.child_count("random");
		ptr = ptr + 1; 
		for (i = 0; i < size; i ++) {
			*ptr = cfg.child("random", i)["value"].to_int();
			ptr = ptr + 1; 
		}
		const config& child = cfg.child("recruit");
		// heros
		const std::vector<std::string> heros = utils::split(child["heros"]);
		size = *ptr = heros.size();
		ptr = ptr + 1; 
		for (i = 0; i < size; i ++) {
			*ptr = lexical_cast_default<int>(heros[i]);
			ptr = ptr + 1;
		}
		*ptr = lexical_cast_default<int>(child["human"]);
		ptr = ptr + 1;
		*ptr = lexical_cast_default<int>(child["cost"]);
		ptr = ptr + 1;
		*ptr = lexical_cast_default<int>(child["x"]) - 1;
		ptr = ptr + 1; 
		*ptr = lexical_cast_default<int>(child["y"]) - 1;
		ptr = ptr + 1;
		// type="market"
		size = *ptr = child["type"].str().size();
		ptr = ptr + 1;
		memcpy(ptr, child["type"].str().c_str(), size);
		*((char*)ptr + size) = '\0';
		ptr = ptr + (size + 1 + 3) / 4; // align 4, 1 is '\0'
		cmd->flags = 0;
		pool_data_vsize_ += sizeof(command_pool::command) + (int)((uint8_t*)ptr - (uint8_t*)(cmd + 1));

	} else if (type == command_pool::EXPEDITE) {
		cmd->type = (command_pool::TYPE)type;
		int* ptr = (int*)(cmd + 1);
		size = *ptr = cfg.child_count("random");
		ptr = ptr + 1; 
		for (i = 0; i < size; i ++) {
			const config& random = cfg.child("random", i);
			*ptr = random["value"].to_int();
			ptr = ptr + 1;

			const config& results = random.child("results", 0);
			if (!results) {
				*ptr = INT_MAX;
				ptr = ptr + 1;
				continue;
			}
			// [results].choose
			*ptr = lexical_cast_default<int>(results["choose"], 0);
			ptr = ptr + 1;
		}
		const config& child = cfg.child("expedite");
		// troop
		*ptr = child["troop"].to_int();
		ptr = ptr + 1;
		// zero
		*ptr = child["zero"].to_int();
		ptr = ptr + 1;
		// value
		*ptr = child["value"].to_int();
		ptr = ptr + 1;
		// direct
		*ptr = child["direct"].to_int();
		ptr = ptr + 1;
		// x=/y=
		const std::string x = child.get("x")->str();
		const std::string y = child.get("y")->str();
		steps = parse_location_range(resources::game_map, x, y);
		*ptr = steps.size();
		ptr = ptr + 1;
		for (std::vector<map_location>::iterator itor = steps.begin(); itor != steps.end(); ++ itor) {
			*ptr = itor->x;
			ptr = ptr + 1; 
			*ptr = itor->y;
			ptr = ptr + 1;
		}
		cmd->flags = 0;
		pool_data_vsize_ += sizeof(command_pool::command) + (int)((uint8_t*)ptr - (uint8_t*)(cmd + 1));

	} else if (type == command_pool::MOVEMENT) {
		cmd->type = (command_pool::TYPE)type;
		int* ptr = (int*)(cmd + 1);
		size = *ptr = cfg.child_count("random");
		ptr = ptr + 1; 
		for (i = 0; i < size; i ++) {
			const config& random = cfg.child("random", i);
			*ptr = random["value"].to_int();
			ptr = ptr + 1;

			const config& results = random.child("results", 0);
			if (!results) {
				*ptr = INT_MAX;
				ptr = ptr + 1;
				continue;
			}
			// [results].choose
			*ptr = lexical_cast_default<int>(results["choose"], 0);
			ptr = ptr + 1;
		}
		const config& child = cfg.child("move");
		// direct
		*ptr = child["direct"].to_int();
		ptr = ptr + 1;
		// cost
		*ptr = child["cost"].to_int();
		ptr = ptr + 1;
		// x=/y=
		const std::string x = child.get("x")->str();
		const std::string y = child.get("y")->str();
		steps = parse_location_range(resources::game_map, x, y);
		*ptr = steps.size();
		ptr = ptr + 1;
		for (std::vector<map_location>::iterator itor = steps.begin(); itor != steps.end(); ++ itor) {
			*ptr = itor->x;
			ptr = ptr + 1; 
			*ptr = itor->y;
			ptr = ptr + 1;
		}
		cmd->flags = 0;
		pool_data_vsize_ += sizeof(command_pool::command) + (int)((uint8_t*)ptr - (uint8_t*)(cmd + 1));

	} else if (type == command_pool::BOMB) {
		cmd->type = (command_pool::TYPE)type;
		int* ptr = (int*)(cmd + 1);
		size = *ptr = cfg.child_count("random");
		ptr = ptr + 1; 
		for (i = 0; i < size; i ++) {
			*ptr = cfg.child("random", i)["value"].to_int();
			ptr = ptr + 1; 
		}
		const config& child = cfg.child("bomb");
		*ptr = child["side"].to_int();
		ptr = ptr + 1;

		cmd->flags = 0;
		pool_data_vsize_ += sizeof(command_pool::command) + (int)((uint8_t*)ptr - (uint8_t*)(cmd + 1));

	} else if (type == command_pool::ATTACK) {
		cmd->type = (command_pool::TYPE)type;
		int* ptr = (int*)(cmd + 1);
		size = *ptr = cfg.child_count("random");
		ptr = ptr + 1; 
		for (i = 0; i < size; i ++) {
			const config& random = cfg.child("random", i);
			*ptr = random["value"].to_int();
			ptr = ptr + 1; 
			const config& results = random.child("results", 0);
			if (!results) {
				*ptr = INT_MAX;
				ptr = ptr + 1;
				continue;
			}
			// [results].chance
			*ptr = lexical_cast_default<int>(results["chance"], 0);
			ptr = ptr + 1;
			// [results].damage
			*ptr = lexical_cast_default<int>(results["damage"], 0);
			ptr = ptr + 1;
			// [results].dies
			*ptr = (results["dies"] == "yes")? 1: 0;
			ptr = ptr + 1;
			// [results].hits
			*ptr = (results["hits"] == "yes")? 1: 0;
			ptr = ptr + 1;
		}
		const config& child = cfg.child("attack");
		if (const config& duel = child.child("duel")) {
			*ptr = duel["left"].to_int();
			ptr = ptr + 1; 
			*ptr = duel["percentage"].to_int();
			ptr = ptr + 1; 
			*ptr = duel["right"].to_int();
			ptr = ptr + 1;
		} else {
			// indicate no [duel] block.
			*ptr = INT_MAX;
			ptr = ptr + 1; 
		}
		// attacker_lvl
		*ptr = child["attacker_lvl"].to_int();
		ptr = ptr + 1;
		// defender_layer
		*ptr = child["defender_layer"].to_int();
		ptr = ptr + 1;
		// defender_lvl
		*ptr = child["defender_lvl"].to_int();
		ptr = ptr + 1; 
		// defender_weapon
		*ptr = child["defender_weapon"].to_int();
		ptr = ptr + 1;
		// layer
		*ptr = child["layer"].to_int();
		ptr = ptr + 1;
		// seed
		*ptr = child["seed"].to_int();
		ptr = ptr + 1;
		// turn
		*ptr = child["turn"].to_int();
		ptr = ptr + 1;
		// move
		*ptr = child["move"].to_int();
		ptr = ptr + 1;
		// constant_attacks
		*ptr = child["constant_attacks"].to_int();
		ptr = ptr + 1;
		// weapon
		*ptr = lexical_cast_default<int>(child["weapon"]);
		ptr = ptr + 1;
		// [source].x/y
		const config& source = child.child("source", 0);
		*ptr = source["x"].to_int() - 1;
		ptr = ptr + 1; 
		*ptr = source["y"].to_int() - 1;
		ptr = ptr + 1;
		// [destination].x/y
		const config& destination = child.child("destination", 0);
		*ptr = destination["x"].to_int() - 1;
		ptr = ptr + 1; 
		*ptr = destination["y"].to_int() - 1;
		ptr = ptr + 1;
		cmd->flags = 0;
		pool_data_vsize_ += sizeof(command_pool::command) + (int)((uint8_t*)ptr - (uint8_t*)(cmd + 1));

	} else if (type == command_pool::FORMATION_ATTACK) {
		cmd->type = (command_pool::TYPE)type;
		int* ptr = (int*)(cmd + 1);
		size = *ptr = cfg.child_count("random");
		ptr = ptr + 1; 
		for (i = 0; i < size; i ++) {
			const config& random = cfg.child("random", i);
			*ptr = random["value"].to_int();
			ptr = ptr + 1; 
			const config& results = random.child("results", 0);
			if (!results) {
				*ptr = INT_MAX;
				ptr = ptr + 1;
				continue;
			}
			// [results].chance
			*ptr = results["chance"].to_int(0);
			ptr = ptr + 1;
			// [results].damage
			*ptr = results["damage"].to_int(0);
			ptr = ptr + 1;
			// [results].dies
			*ptr = (results["dies"] == "yes")? 1: 0;
			ptr = ptr + 1;
			// [results].hits
			*ptr = (results["hits"] == "yes")? 1: 0;
			ptr = ptr + 1;
		}
		const config& child = cfg.child("formation_attack");
		// master="47,2"
		std::vector<std::string> coor = utils::split(child["master"]);
		for (i = 0; i < 2; i ++) {
			*ptr = lexical_cast_default<int>(coor[i]);
			ptr = ptr + 1;
		}
				
		cmd->flags = 0;
		pool_data_vsize_ += sizeof(command_pool::command) + (int)((uint8_t*)ptr - (uint8_t*)(cmd + 1));

	} else if (type == command_pool::DISBAND) {
		cmd->type = (command_pool::TYPE)type;
		int* ptr = (int*)(cmd + 1);
		size = *ptr = cfg.child_count("random");
		ptr = ptr + 1; 
		for (i = 0; i < size; i ++) {
			*ptr = cfg.child("random", i)["value"].to_int();
			ptr = ptr + 1; 
		}
		const config& child = cfg.child("disband");
		// value
		*ptr = child["value"].to_int();
		ptr = ptr + 1;
		// income
		*ptr = child["income"].to_int();
		ptr = ptr + 1;
		// x/y
		*ptr = child["x"].to_int() - 1;
		ptr = ptr + 1; 
		*ptr = child["y"].to_int() - 1;
		ptr = ptr + 1;
		cmd->flags = 0;
		pool_data_vsize_ += sizeof(command_pool::command) + (int)((uint8_t*)ptr - (uint8_t*)(cmd + 1));

	} else if (type == command_pool::INCHING_BLOCK) {
		cmd->type = (command_pool::TYPE)type;
		int* ptr = (int*)(cmd + 1);
		size = *ptr = cfg.child_count("random");
		ptr = ptr + 1; 
		for (i = 0; i < size; i ++) {
			*ptr = cfg.child("random", i)["value"].to_int();
			ptr = ptr + 1; 
		}
		const config& child = cfg.child("inching_block");
		// increase
		*ptr = child["increase"].to_int();
		ptr = ptr + 1;
		// x/y
		*ptr = child["x"].to_int() - 1;
		ptr = ptr + 1; 
		*ptr = child["y"].to_int() - 1;
		ptr = ptr + 1;
		cmd->flags = 0;
		pool_data_vsize_ += sizeof(command_pool::command) + (int)((uint8_t*)ptr - (uint8_t*)(cmd + 1));

	} else if (type == command_pool::EVENT) {
		cmd->type = (command_pool::TYPE)type;
		int* ptr = (int*)(cmd + 1);
		size = *ptr = cfg.child_count("random");
		ptr = ptr + 1; 
		for (i = 0; i < size; i ++) {
			*ptr = cfg.child("random", i)["value"].to_int();
			ptr = ptr + 1; 
		}
		const config& child = cfg.child("event");
		// type
		*ptr = lexical_cast_default<int>(child["type"]);
		ptr = ptr + 1;
		// x/y
		*ptr = lexical_cast_default<int>(child["x"]) - 1;
		ptr = ptr + 1; 
		*ptr = lexical_cast_default<int>(child["y"]) - 1;
		ptr = ptr + 1;
		cmd->flags = 0;
		pool_data_vsize_ += sizeof(command_pool::command) + (int)((uint8_t*)ptr - (uint8_t*)(cmd + 1));

	} else if (type == command_pool::ASSEMBLE_TREASURE) {
		cmd->type = (command_pool::TYPE)type;
		int* ptr = (int*)(cmd + 1);
		size = *ptr = cfg.child_count("random");
		ptr = ptr + 1; 
		for (i = 0; i < size; i ++) {
			*ptr = cfg.child("random", i)["value"].to_int();
			ptr = ptr + 1; 
		}
		const config& child = cfg.child("assemble_treasure");
		// diff="0,1,2,3,4,5"
		const std::vector<std::string> diff = utils::split(child["diff"]);
		size = *ptr = diff.size();
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			*ptr = lexical_cast_default<int>(diff[i]);
			ptr = ptr + 1;
		}
		// other field-vars
		cmd->flags = 0;
		pool_data_vsize_ += sizeof(command_pool::command) + (int)((uint8_t*)ptr - (uint8_t*)(cmd + 1));

	} else if (type == command_pool::APPOINT_NOBLE) {
		cmd->type = (command_pool::TYPE)type;
		int* ptr = (int*)(cmd + 1);
		size = *ptr = cfg.child_count("random");
		ptr = ptr + 1; 
		for (i = 0; i < size; i ++) {
			*ptr = cfg.child("random", i)["value"].to_int();
			ptr = ptr + 1; 
		}
		const config& child = cfg.child("appoint_noble");
		// diff="0,1,2,3,4,5"
		const std::vector<std::string> diff = utils::split(child["diff"]);
		size = *ptr = diff.size();
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			*ptr = lexical_cast_default<int>(diff[i]);
			ptr = ptr + 1;
		}
		// other field-vars
		cmd->flags = 0;
		pool_data_vsize_ += sizeof(command_pool::command) + (int)((uint8_t*)ptr - (uint8_t*)(cmd + 1));

	} else if (type == command_pool::RPG_EXCHANGE) {
		cmd->type = (command_pool::TYPE)type;
		int* ptr = (int*)(cmd + 1);
		size = *ptr = cfg.child_count("random");
		ptr = ptr + 1; 
		for (i = 0; i < size; i ++) {
			*ptr = cfg.child("random", i)["value"].to_int();
			ptr = ptr + 1; 
		}
		const config& child = cfg.child("rpg_exchange");
		// human
		const std::vector<std::string> heros = utils::split(child["human"]);
		size = *ptr = heros.size();
		ptr = ptr + 1; 
		for (i = 0; i < size; i ++) {
			*ptr = lexical_cast_default<int>(heros[i]);
			ptr = ptr + 1;
		}
		*ptr = lexical_cast_default<int>(child["ai"]);
		ptr = ptr + 1;
		cmd->flags = 0;
		pool_data_vsize_ += sizeof(command_pool::command) + (int)((uint8_t*)ptr - (uint8_t*)(cmd + 1));

	} else if (type == command_pool::ING_TECHNOLOGY) {
		cmd->type = (command_pool::TYPE)type;
		int* ptr = (int*)(cmd + 1);
		size = *ptr = cfg.child_count("random");
		ptr = ptr + 1; 
		for (i = 0; i < size; i ++) {
			*ptr = cfg.child("random", i)["value"].to_int();
			ptr = ptr + 1; 
		}
		const config& child = cfg.child("ing_technology");
		// id="hot blooded"
		size = *ptr = child["id"].str().size();
		ptr = ptr + 1;
		memcpy(ptr, child["id"].str().c_str(), size);
		*((char*)ptr + size) = '\0';
		ptr = ptr + (size + 1 + 3) / 4; // align 4, 1 is '\0'
		
		// other field-vars
		cmd->flags = 0;
		pool_data_vsize_ += sizeof(command_pool::command) + (int)((uint8_t*)ptr - (uint8_t*)(cmd + 1));

	} else if (type == command_pool::SCENARIO_ENV) {
		cmd->type = (command_pool::TYPE)type;
		int* ptr = (int*)(cmd + 1);
		size = *ptr = cfg.child_count("random");
		ptr = ptr + 1; 
		for (i = 0; i < size; i ++) {
			*ptr = cfg.child("random", i)["value"].to_int();
			ptr = ptr + 1; 
		}
		const config& child = cfg.child("scenario_env");
		// duel
		*ptr = child["duel"].to_int();
		ptr = ptr + 1;
		// maximal_defeated_activity
		*ptr = child["maximal_defeated_activity"].to_int();
		ptr = ptr + 1;
		// vip
		*ptr = child["vip"].to_int();
		ptr = ptr + 1;
		
		// other field-vars
		cmd->flags = 0;
		pool_data_vsize_ += sizeof(command_pool::command) + (int)((uint8_t*)ptr - (uint8_t*)(cmd + 1));

	} else if (type == command_pool::BUILD) {
		cmd->type = (command_pool::TYPE)type;
		int* ptr = (int*)(cmd + 1);
		size = *ptr = cfg.child_count("random");
		ptr = ptr + 1; 
		for (i = 0; i < size; i ++) {
			*ptr = cfg.child("random", i)["value"].to_int();
			ptr = ptr + 1; 
		}
		const config& child = cfg.child("build");
		// city="19,2"
		std::vector<std::string> coor = utils::split(child["city"]);
		for (i = 0; i < 2; i ++) {
			*ptr = lexical_cast_default<int>(coor[i]);
			ptr = ptr + 1;
		}
		// artifical="46,5"
		coor = utils::split(child["artifical"]);
		for (i = 0; i < 2; i ++) {
			*ptr = lexical_cast_default<int>(coor[i]);
			ptr = ptr + 1;
		}
		// builder="45,6"
		coor = utils::split(child["builder"]);
		for (i = 0; i < 2; i ++) {
			*ptr = lexical_cast_default<int>(coor[i]);
			ptr = ptr + 1;
		}
		*ptr = lexical_cast_default<int>(child["cost"]);
		ptr = ptr + 1;
		// type="market"
		size = *ptr = child["type"].str().size();
		ptr = ptr + 1;
		memcpy(ptr, child["type"].str().c_str(), size);
		*((char*)ptr + size) = '\0';
		ptr = ptr + (size + 1 + 3) / 4; // align 4, 1 is '\0'
		cmd->flags = 0;
		pool_data_vsize_ += sizeof(command_pool::command) + (int)((uint8_t*)ptr - (uint8_t*)(cmd + 1));

	} else if (type == command_pool::CAST_TACTIC) {
		cmd->type = (command_pool::TYPE)type;
		int* ptr = (int*)(cmd + 1);
		size = *ptr = cfg.child_count("random");
		ptr = ptr + 1; 
		for (i = 0; i < size; i ++) {
			*ptr = cfg.child("random", i)["value"].to_int();
			ptr = ptr + 1; 
		}
		const config& child = cfg.child("cast_tactic");
		// tactician="19,2"
		std::vector<std::string> coor = utils::split(child["tactician"]);
		for (i = 0; i < 2; i ++) {
			*ptr = lexical_cast_default<int>(coor[i]);
			ptr = ptr + 1;
		}
		// special="19,2"
		coor = utils::split(child["special"]);
		for (i = 0; i < 2; i ++) {
			*ptr = lexical_cast_default<int>(coor[i]);
			ptr = ptr + 1;
		}
		*ptr = child["hero"].to_int();
		ptr = ptr + 1;
		*ptr = child["consume"].to_int();
		ptr = ptr + 1;
		*ptr = child["cost"].to_int();
		ptr = ptr + 1;

		cmd->flags = 0;
		pool_data_vsize_ += sizeof(command_pool::command) + (int)((uint8_t*)ptr - (uint8_t*)(cmd + 1));

	} else if (type == command_pool::ARMORY) {
		cmd->type = (command_pool::TYPE)type;
		int* ptr = (int*)(cmd + 1);
		size = *ptr = cfg.child_count("random");
		ptr = ptr + 1; 
		for (i = 0; i < size; i ++) {
			*ptr = cfg.child("random", i)["value"].to_int();
			ptr = ptr + 1; 
		}
		const config& child = cfg.child("armory");
		// city="47,2"
		std::vector<std::string> coor = utils::split(child["city"]);
		for (i = 0; i < 2; i ++) {
			*ptr = lexical_cast_default<int>(coor[i]);
			ptr = ptr + 1;
		}
		// diff="0,1,2,3,4"
		const std::vector<std::string> diff = utils::split(child["diff"]);
		size = *ptr = diff.size();
		ptr = ptr + 1; 
		for (i = 0; i < size; i ++) {
			*ptr = lexical_cast_default<int>(diff[i]);
			ptr = ptr + 1;
		}
		cmd->flags = 0;
		pool_data_vsize_ += sizeof(command_pool::command) + (int)((uint8_t*)ptr - (uint8_t*)(cmd + 1));

	} else if (type == command_pool::MOVE_HEROS) {
		cmd->type = (command_pool::TYPE)type;
		int* ptr = (int*)(cmd + 1);
		size = *ptr = cfg.child_count("random");
		ptr = ptr + 1; 
		for (i = 0; i < size; i ++) {
			*ptr = cfg.child("random", i)["value"].to_int();
			ptr = ptr + 1; 
		}
		const config& child = cfg.child("move_heros");
		// dst="47,2"
		std::vector<std::string> coor = utils::split(child["dst"]);
		for (i = 0; i < 2; i ++) {
			*ptr = lexical_cast_default<int>(coor[i]);
			ptr = ptr + 1;
		}
		// heros="0,1,2,3,4"
		const std::vector<std::string> heros = utils::split(child["heros"]);
		size = *ptr = heros.size();
		ptr = ptr + 1; 
		for (i = 0; i < size; i ++) {
			*ptr = lexical_cast_default<int>(heros[i]);
			ptr = ptr + 1;
		}
		// src="58,16"
		coor = utils::split(child["src"]);
		for (i = 0; i < 2; i ++) {
			*ptr = lexical_cast_default<int>(coor[i]);
			ptr = ptr + 1;
		}		
		cmd->flags = 0;
		pool_data_vsize_ += sizeof(command_pool::command) + (int)((uint8_t*)ptr - (uint8_t*)(cmd + 1));

	} else if (type == command_pool::INTERIOR) {
		cmd->type = (command_pool::TYPE)type;
		int* ptr = (int*)(cmd + 1);
		size = *ptr = cfg.child_count("random");
		ptr = ptr + 1; 
		for (i = 0; i < size; i ++) {
			*ptr = cfg.child("random", i)["value"].to_int();
			ptr = ptr + 1; 
		}
		const config& child = cfg.child("interior");
		// officials="0,1,2"
		*ptr = child["x"].to_int() - 1;
		ptr = ptr + 1; 
		*ptr = child["y"].to_int() - 1;
		ptr = ptr + 1;
		// mayor="0"
		*ptr = child["mayor"].to_int();
		ptr = ptr + 1;

		cmd->flags = 0;
		pool_data_vsize_ += sizeof(command_pool::command) + (int)((uint8_t*)ptr - (uint8_t*)(cmd + 1));

	} else if (type == command_pool::BELONG_TO) {
		cmd->type = (command_pool::TYPE)type;
		int* ptr = (int*)(cmd + 1);
		size = *ptr = cfg.child_count("random");
		ptr = ptr + 1; 
		for (i = 0; i < size; i ++) {
			*ptr = cfg.child("random", i)["value"].to_int();
			ptr = ptr + 1; 
		}
		const config& child = cfg.child("belong_to");
		// dst="47,2"
		std::vector<std::string> coor = utils::split(child["dst"]);
		for (i = 0; i < 2; i ++) {
			*ptr = lexical_cast_default<int>(coor[i]);
			ptr = ptr + 1;
		}
		// layer="0"
		*ptr = child["layer"].to_int();
		ptr = ptr + 1;
		// loyalty="1"
		*ptr = child["loyalty"].to_int();
		ptr = ptr + 1;
		// src="58,16"
		coor = utils::split(child["src"]);
		for (i = 0; i < 2; i ++) {
			*ptr = lexical_cast_default<int>(coor[i]);
			ptr = ptr + 1;
		}		
		cmd->flags = 0;
		pool_data_vsize_ += sizeof(command_pool::command) + (int)((uint8_t*)ptr - (uint8_t*)(cmd + 1));

	} else if (type == command_pool::SET_STATES) {
		cmd->type = (command_pool::TYPE)type;
		int* ptr = (int*)(cmd + 1);
		size = *ptr = cfg.child_count("random");
		ptr = ptr + 1; 
		for (i = 0; i < size; i ++) {
			*ptr = cfg.child("random", i)["value"].to_int();
			ptr = ptr + 1; 
		}

		const config& child = cfg.child("set_states");
		// loc="47,2"
		std::vector<std::string> coor = utils::split(child["loc"]);
		for (i = 0; i < 2; i ++) {
			*ptr = lexical_cast_default<int>(coor[i]);
			ptr = ptr + 1;
		}
		// layer="0"
		*ptr = child["layer"].to_int();
		ptr = ptr + 1;

		// states="0,1,2,0,4,1"
		const std::vector<std::string> states = utils::split(child["states"]);
		size = *ptr = states.size();
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			*ptr = lexical_cast_default<int>(states[i]);
			ptr = ptr + 1;
		}

		// other field-vars
		cmd->flags = 0;
		pool_data_vsize_ += sizeof(command_pool::command) + (int)((uint8_t*)ptr - (uint8_t*)(cmd + 1));

	} else if (type == command_pool::CLEAR_FORMATIONED) {
		cmd->type = (command_pool::TYPE)type;
		int* ptr = (int*)(cmd + 1);
		size = *ptr = cfg.child_count("random");
		ptr = ptr + 1; 
		for (i = 0; i < size; i ++) {
			*ptr = cfg.child("random", i)["value"].to_int();
			ptr = ptr + 1; 
		}

		const config& child = cfg.child("clear_formationed");
		// loc="47,2"
		std::vector<std::string> coor = utils::split(child["loc"]);
		for (i = 0; i < 2; i ++) {
			*ptr = lexical_cast_default<int>(coor[i]);
			ptr = ptr + 1;
		}
		// layer="0"
		*ptr = child["layer"].to_int();
		ptr = ptr + 1;

		// cost
		*ptr = child["cost"].to_int();
		ptr = ptr + 1;

		// other field-vars
		cmd->flags = 0;
		pool_data_vsize_ += sizeof(command_pool::command) + (int)((uint8_t*)ptr - (uint8_t*)(cmd + 1));

	} else if (type == command_pool::SET_TASK) {
		cmd->type = (command_pool::TYPE)type;
		int* ptr = (int*)(cmd + 1);
		size = *ptr = cfg.child_count("random");
		ptr = ptr + 1; 
		for (i = 0; i < size; i ++) {
			*ptr = cfg.child("random", i)["value"].to_int();
			ptr = ptr + 1; 
		}

		const config& child = cfg.child("set_task");
		// loc="47,2"
		std::vector<std::string> coor = utils::split(child["loc"]);
		for (i = 0; i < 2; i ++) {
			*ptr = lexical_cast_default<int>(coor[i]);
			ptr = ptr + 1;
		}
		// hero="0"
		*ptr = child["hero"].to_int();
		ptr = ptr + 1;
		// task
		*ptr = child["task"].to_int();
		ptr = ptr + 1;
		// at="47,2"
		coor = utils::split(child["at"]);
		for (i = 0; i < 2; i ++) {
			*ptr = lexical_cast_default<int>(coor[i]);
			ptr = ptr + 1;
		}

		// other field-vars
		cmd->flags = 0;
		pool_data_vsize_ += sizeof(command_pool::command) + (int)((uint8_t*)ptr - (uint8_t*)(cmd + 1));

	} else if (type == command_pool::CHOOSE) {
		cmd->type = (command_pool::TYPE)type;
		int* ptr = (int*)(cmd + 1);
		size = *ptr = cfg.child_count("random");
		ptr = ptr + 1; 
		for (i = 0; i < size; i ++) {
			*ptr = cfg.child("random", i)["value"].to_int();
			ptr = ptr + 1; 
		}
		const config& child = cfg.child("choose");
		// value
		*ptr = lexical_cast_default<int>(child["value"]);
		ptr = ptr + 1;
		cmd->flags = 0;
		pool_data_vsize_ += sizeof(command_pool::command) + (int)((uint8_t*)ptr - (uint8_t*)(cmd + 1));

	} else if (type == command_pool::SPEAK) {
		cmd->type = (command_pool::TYPE)type;
		int* ptr = (int*)(cmd + 1);
		size = *ptr = cfg.child_count("random");
		ptr = ptr + 1; 
		for (i = 0; i < size; i ++) {
			*ptr = cfg.child("random", i)["value"].to_int();
			ptr = ptr + 1; 
		}
		const config& child = cfg.child("speak");
		// side = 2
		*ptr = lexical_cast_default<int>(child["side"], 0);
		ptr = ptr + 1;
		// team_name
		size = *ptr = child["team_name"].str().size();
		ptr = ptr + 1;
		if (size) {
			memcpy(ptr, child["team_name"].str().c_str(), size);
			*((char*)ptr + size) = '\0';
			ptr = ptr + (size + 1 + 3) / 4; // align 4, 1 is '\0'
		}
		// id="server"
		size = *ptr = child["id"].str().size();
		ptr = ptr + 1;
		if (size) {
			memcpy(ptr, child["id"].str().c_str(), size);
			*((char*)ptr + size) = '\0';
			ptr = ptr + (size + 1 + 3) / 4; // align 4, 1 is '\0'
		}
		// message="ancientcc has left the game."
		size = *ptr = child["message"].str().size();
		ptr = ptr + 1;
		if (size) {
			memcpy(ptr, child["message"].str().c_str(), size);
			*((char*)ptr + size) = '\0';
			ptr = ptr + (size + 1 + 3) / 4; // align 4, 1 is '\0'
		}
		cmd->flags = 0;
		pool_data_vsize_ += sizeof(command_pool::command) + (int)((uint8_t*)ptr - (uint8_t*)(cmd + 1));

	} else if (type == command_pool::CLEAR_LABELS) {
		cmd->type = (command_pool::TYPE)type;
		int* ptr = (int*)(cmd + 1);
		size = *ptr = cfg.child_count("random");
		ptr = ptr + 1; 
		for (i = 0; i < size; i ++) {
			*ptr = cfg.child("random", i)["value"].to_int();
			ptr = ptr + 1; 
		}
		const config& child = cfg.child("clear_labels");
		// team_name
		size = *ptr = child["team_name"].str().size();
		ptr = ptr + 1;
		if (size) {
			memcpy(ptr, child["team_name"].str().c_str(), size);
			*((char*)ptr + size) = '\0';
			ptr = ptr + (size + 1 + 3) / 4; // align 4, 1 is '\0'
		}
		cmd->flags = 0;
		pool_data_vsize_ += sizeof(command_pool::command) + (int)((uint8_t*)ptr - (uint8_t*)(cmd + 1));

	} else if (type == command_pool::DIPLOMATISM) {
		cmd->type = (command_pool::TYPE)type;
		int* ptr = (int*)(cmd + 1);
		size = *ptr = cfg.child_count("random");
		ptr = ptr + 1; 
		for (i = 0; i < size; i ++) {
			*ptr = cfg.child("random", i)["value"].to_int();
			ptr = ptr + 1; 
		}
		const config& child = cfg.child("diplomatism");
		// type="ally"
		size = *ptr = child["type"].str().size();
		ptr = ptr + 1;
		memcpy(ptr, child["type"].str().c_str(), size);
		*((char*)ptr + size) = '\0';
		ptr = ptr + (size + 1 + 3) / 4; // align 4, 1 is '\0'
		// alignment = 1
		*ptr = lexical_cast_default<int>(child["alignment"]);
		ptr = ptr + 1;
		// first=1
		*ptr = lexical_cast_default<int>(child["first"]);
		ptr = ptr + 1;
		// hero=102
		*ptr = lexical_cast_default<int>(child["hero"]);
		ptr = ptr + 1;
		// second=2
		*ptr = lexical_cast_default<int>(child["second"]);
		ptr = ptr + 1;
		// strategy=0
		*ptr = lexical_cast_default<int>(child["strategy"]);
		ptr = ptr + 1;
		// target=2
		*ptr = lexical_cast_default<int>(child["target"]);
		ptr = ptr + 1;
		// other field-vars
		cmd->flags = 0;
		pool_data_vsize_ += sizeof(command_pool::command) + (int)((uint8_t*)ptr - (uint8_t*)(cmd + 1));

	} else if (type == command_pool::FINAL_BATTLE) {
		cmd->type = (command_pool::TYPE)type;
		int* ptr = (int*)(cmd + 1);
		size = *ptr = cfg.child_count("random");
		ptr = ptr + 1; 
		for (i = 0; i < size; i ++) {
			*ptr = cfg.child("random", i)["value"].to_int();
			ptr = ptr + 1; 
		}
		const config& child = cfg.child("final_battle");
		// human=1
		*ptr = child["human"].to_int();
		ptr = ptr + 1;
		// ai=2
		*ptr = child["ai"].to_int();
		ptr = ptr + 1;
		// other field-vars
		cmd->flags = 0;
		pool_data_vsize_ += sizeof(command_pool::command) + (int)((uint8_t*)ptr - (uint8_t*)(cmd + 1));

	} else if (type == command_pool::INPUT) {
		cmd->type = (command_pool::TYPE)type;
		int* ptr = (int*)(cmd + 1);
		size = *ptr = cfg.child_count("random");
		ptr = ptr + 1; 
		for (i = 0; i < size; i ++) {
			*ptr = cfg.child("random", i)["value"].to_int();
			ptr = ptr + 1; 
		}
		const config& child = cfg.child("input");
		// value=1
		*ptr = child["value"].to_int();
		ptr = ptr + 1;
		// other field-vars
		cmd->flags = 0;
		pool_data_vsize_ += sizeof(command_pool::command) + (int)((uint8_t*)ptr - (uint8_t*)(cmd + 1));

	} else if (type == command_pool::ADD_CARD) {
		cmd->type = (command_pool::TYPE)type;
		int* ptr = (int*)(cmd + 1);
		size = *ptr = cfg.child_count("random");
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			*ptr = cfg.child("random", i)["value"].to_int();
			ptr = ptr + 1; 
		}
		const config& child = cfg.child("add_card");
		// side=1
		*ptr = child["side"].to_int();
		ptr = ptr + 1;
		// number=2
		*ptr = child["number"].to_int();
		ptr = ptr + 1;
		// dialog=2
		*ptr = child["dialog"].to_int();
		ptr = ptr + 1;
		// other field-vars
		cmd->flags = 0;
		pool_data_vsize_ += sizeof(command_pool::command) + (int)((uint8_t*)ptr - (uint8_t*)(cmd + 1));

	} else if (type == command_pool::ERASE_CARD) {
		cmd->type = (command_pool::TYPE)type;
		int* ptr = (int*)(cmd + 1);
		size = *ptr = cfg.child_count("random");
		ptr = ptr + 1; 
		for (i = 0; i < size; i ++) {
			*ptr = cfg.child("random", i)["value"].to_int();
			ptr = ptr + 1; 
		}
		const config& child = cfg.child("erase_card");
		// heros="0,1,2,3,4"
		const std::vector<std::string> heros = utils::split(child["touched"]);
		size = *ptr = heros.size();
		ptr = ptr + 1; 
		for (i = 0; i < size; i ++) {
			*ptr = lexical_cast_default<int>(heros[i]);
			ptr = ptr + 1;
		}
		// side=1
		*ptr = child["side"].to_int();
		ptr = ptr + 1;
		// index=2
		*ptr = child["index"].to_int();
		ptr = ptr + 1;
		// dialog=2
		*ptr = child["dialog"].to_int();
		ptr = ptr + 1;
		// x/y
		*ptr = child["x"].to_int() - 1;
		ptr = ptr + 1; 
		*ptr = child["y"].to_int() - 1;
		ptr = ptr + 1;
		// other field-vars
		cmd->flags = 0;
		pool_data_vsize_ += sizeof(command_pool::command) + (int)((uint8_t*)ptr - (uint8_t*)(cmd + 1));

	} else if (type == command_pool::REFORM_CAPTAIN) {
		cmd->type = (command_pool::TYPE)type;
		int* ptr = (int*)(cmd + 1);
		size = *ptr = cfg.child_count("random");
		ptr = ptr + 1; 
		for (i = 0; i < size; i ++) {
			*ptr = cfg.child("random", i)["value"].to_int();
			ptr = ptr + 1; 
		}
		const config& child = cfg.child("reform_captain");
		// number=1
		*ptr = child["number"].to_int();
		ptr = ptr + 1;
		// join=0/1
		*ptr = child["join"].to_int();
		ptr = ptr + 1;
		// x/y
		*ptr = child["x"].to_int() - 1;
		ptr = ptr + 1; 
		*ptr = child["y"].to_int() - 1;
		ptr = ptr + 1;
		// other field-vars
		cmd->flags = 0;
		pool_data_vsize_ += sizeof(command_pool::command) + (int)((uint8_t*)ptr - (uint8_t*)(cmd + 1));

	} else if (type == command_pool::ACTIVE_TACTIC) {
		cmd->type = (command_pool::TYPE)type;
		int* ptr = (int*)(cmd + 1);
		size = *ptr = cfg.child_count("random");
		ptr = ptr + 1; 
		for (i = 0; i < size; i ++) {
			*ptr = cfg.child("random", i)["value"].to_int();
			ptr = ptr + 1; 
		}
		const config& child = cfg.child("active_tactic");
		// add=1
		*ptr = child["add"].to_int();
		ptr = ptr + 1;
		// unit=1
		*ptr = child["unit"].to_int();
		ptr = ptr + 1;
		// hero=2
		*ptr = child["hero"].to_int();
		ptr = ptr + 1;

		// other field-vars
		cmd->flags = 0;
		pool_data_vsize_ += sizeof(command_pool::command) + (int)((uint8_t*)ptr - (uint8_t*)(cmd + 1));

	} else {
		std::stringstream str;
		str << "config_2_command, unknown type(str): " << cfg["type"].str() << "; exist subconfig";
		BOOST_FOREACH (const config::any_child &value, cfg.all_children_range()) {
			str << " [" << value.key << "]";
		}
		replay::process_error(str.str());
	}
	
}

void replay::command_2_config(command_pool::command* cmd, config& cfg)
{
	std::stringstream str, xstr, ystr;
	size_t i, size;
	map_location loc;

	if (cmd->type == command_pool::START) {
		config& child = cfg.add_child("start");
		int* ptr = (int*)(cmd + 1);
		size = *ptr;
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			config& random_cfg = cfg.add_child("random");
			random_cfg["value"] = lexical_cast<std::string>(*ptr);
			ptr = ptr + 1;
		}

	} else if (cmd->type == command_pool::EMPLOY) {
		config& child = cfg.add_child("employ");
		int* ptr = (int*)(cmd + 1);
		size = *ptr;
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			config& random_cfg = cfg.add_child("random");
			random_cfg["value"] = lexical_cast<std::string>(*ptr);
			ptr = ptr + 1;
		}
		child["hero"] = (*ptr);
		ptr = ptr + 1;
		child["cost"] = (*ptr);
		ptr = ptr + 1;

	} else if (cmd->type == command_pool::PURCHASE) {
		config& child = cfg.add_child("purchase");
		int* ptr = (int*)(cmd + 1);
		size = *ptr;
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			config& random_cfg = cfg.add_child("random");
			random_cfg["value"] = lexical_cast<std::string>(*ptr);
			ptr = ptr + 1;
		}
		child["type"] = (*ptr);
		ptr = ptr + 1;
		child["cost"] = (*ptr);
		ptr = ptr + 1;

	} else if (cmd->type == command_pool::INIT_SIDE) {
		config& child = cfg.add_child("init_side");
		int* ptr = (int*)(cmd + 1);
		size = *ptr;
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			config& random_cfg = cfg.add_child("random");
			random_cfg["value"] = lexical_cast<std::string>(*ptr);
			ptr = ptr + 1;
			if (*ptr == INT_MAX) {
				ptr = ptr + 1;
				continue;
			}
			config& result_cfg = random_cfg.add_child("results");
			result_cfg["choose"] = lexical_cast<std::string>(*ptr);
			ptr = ptr + 1;
		}

		size = *ptr;
		ptr = ptr + 1;
		child["strategies"] = (char*)ptr;

	} else if (cmd->type == command_pool::END_TURN) {
		config& child = cfg.add_child("end_turn");
		int* ptr = (int*)(cmd + 1);
		size = *ptr;
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			config& random_cfg = cfg.add_child("random");
			random_cfg["value"] = lexical_cast<std::string>(*ptr);
			ptr = ptr + 1;
		}

	} else if (cmd->type == command_pool::INIT_AI) {
		config& child = cfg.add_child("init_ai");
		int* ptr = (int*)(cmd + 1);
		size = *ptr;
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			config& random_cfg = cfg.add_child("random");
			random_cfg["value"] = lexical_cast<std::string>(*ptr);
			ptr = ptr + 1;
		}

	} else if (cmd->type == command_pool::PREFIX_UNIT) {
		config& child = cfg.add_child("prefix_unit");
		int* ptr = (int*)(cmd + 1);
		size = *ptr;
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			config& random_cfg = cfg.add_child("random");
			random_cfg["value"] = lexical_cast<std::string>(*ptr);
			ptr = ptr + 1;
			if (*ptr == INT_MAX) {
				ptr = ptr + 1;
				continue;
			}
			config& result_cfg = random_cfg.add_child("results");
			result_cfg["choose"] = lexical_cast<std::string>(*ptr);
			ptr = ptr + 1;
		}

		child["side"] = *ptr;
		ptr = ptr + 1;
		child["new_turn"] = *ptr;
		ptr = ptr + 1;
		child["end"] = *ptr;
		ptr = ptr + 1;

	} else if (cmd->type == command_pool::POST_UNIT) {
		config& child = cfg.add_child("post_unit");
		int* ptr = (int*)(cmd + 1);
		size = *ptr;
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			config& random_cfg = cfg.add_child("random");
			random_cfg["value"] = lexical_cast<std::string>(*ptr);
			ptr = ptr + 1;
			if (*ptr == INT_MAX) {
				ptr = ptr + 1;
				continue;
			}
			config& result_cfg = random_cfg.add_child("results");
			result_cfg["choose"] = lexical_cast<std::string>(*ptr);
			ptr = ptr + 1;
		}

		child["past"] = *ptr;
		ptr = ptr + 1;

	} else if (cmd->type == command_pool::FRESH_HEROS) {
		config& child = cfg.add_child("fresh_heros");
		int* ptr = (int*)(cmd + 1);
		size = *ptr;
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			config& random_cfg = cfg.add_child("random");
			random_cfg["value"] = lexical_cast<std::string>(*ptr);
			ptr = ptr + 1;
		}

	} else if (cmd->type == command_pool::DO_COMMONER) {
		config& child = cfg.add_child("do_commoner");
		int* ptr = (int*)(cmd + 1);
		size = *ptr;
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			config& random_cfg = cfg.add_child("random");
			random_cfg["value"] = lexical_cast<std::string>(*ptr);
			ptr = ptr + 1;
			if (*ptr == INT_MAX) {
				ptr = ptr + 1;
				continue;
			}
			config& result_cfg = random_cfg.add_child("results");
			result_cfg["choose"] = lexical_cast<std::string>(*ptr);
			ptr = ptr + 1;
		}

	} else if (cmd->type == command_pool::RECRUIT) {
		config& child = cfg.add_child("recruit");
		int* ptr = (int*)(cmd + 1);
		size = *ptr;
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			config& random_cfg = cfg.add_child("random");
			random_cfg["value"] = lexical_cast<std::string>(*ptr);
			ptr = ptr + 1;
		}
		size = *ptr;
		ptr = ptr + 1;
		// heros
		str << *ptr;
		ptr = ptr + 1;
		for (i = 1; i < size; i ++) {
			str << "," << *ptr;
			ptr = ptr + 1;
		}
		child["heros"] = str.str();
		child["human"] = *ptr;
		ptr = ptr + 1;
		child["cost"] = *ptr;
		ptr = ptr + 1;
		loc.x = *ptr;
		ptr = ptr + 1;
		loc.y = *ptr;
		ptr = ptr + 1;
		loc.write(child);

		size = *ptr;
		ptr = ptr + 1;
		child["type"] = (char*)ptr;

	} else if (cmd->type == command_pool::EXPEDITE) {
		config& child = cfg.add_child("expedite");
		int* ptr = (int*)(cmd + 1);
		size = *ptr;
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			config& random_cfg = cfg.add_child("random");
			random_cfg["value"] = lexical_cast<std::string>(*ptr);
			ptr = ptr + 1;
			if (*ptr == INT_MAX) {
				ptr = ptr + 1;
				continue;
			}
			config& result_cfg = random_cfg.add_child("results");
			result_cfg["choose"] = lexical_cast<std::string>(*ptr);
			ptr = ptr + 1;
		}
		// troop
		child["troop"] = *ptr;
		ptr = ptr + 1;
		// zero
		child["zero"] = *ptr;
		ptr = ptr + 1;
		// value
		child["value"] = *ptr;
		ptr = ptr + 1;
		// direct
		child["direct"] = *ptr;
		ptr = ptr + 1;
		// x=/y=
		size = *ptr;
		ptr = ptr + 1;
		xstr << *ptr + 1;
		ptr = ptr + 1;
		ystr << *ptr + 1;
		ptr = ptr + 1;
		for (i = 1; i < size; i ++) {
			xstr << "," << *ptr + 1;
			ptr = ptr + 1;
			ystr << "," << *ptr + 1;
			ptr = ptr + 1;
		}
		child["x"] = xstr.str();
		child["y"] = ystr.str();

	} else if (cmd->type == command_pool::MOVEMENT) {
		config& child = cfg.add_child("move");
		int* ptr = (int*)(cmd + 1);
		size = *ptr;
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			config& random_cfg = cfg.add_child("random");
			random_cfg["value"] = lexical_cast<std::string>(*ptr);
			ptr = ptr + 1;
			if (*ptr == INT_MAX) {
				ptr = ptr + 1;
				continue;
			}
			config& result_cfg = random_cfg.add_child("results");
			result_cfg["choose"] = lexical_cast<std::string>(*ptr);
			ptr = ptr + 1;
		}
		// direct
		child["direct"] = *ptr;
		ptr = ptr + 1;
		// cost
		child["cost"] = *ptr;
		ptr = ptr + 1;
		// x=/y=
		size = *ptr;
		ptr = ptr + 1;
		xstr << *ptr + 1;
		ptr = ptr + 1;
		ystr << *ptr + 1;
		ptr = ptr + 1;
		for (i = 1; i < size; i ++) {
			xstr << "," << *ptr + 1;
			ptr = ptr + 1;
			ystr << "," << *ptr + 1;
			ptr = ptr + 1;
		}
		child["x"] = xstr.str();
		child["y"] = ystr.str();

	} else if (cmd->type == command_pool::BOMB) {
		config& child = cfg.add_child("bomb");
		int* ptr = (int*)(cmd + 1);
		size = *ptr;
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			config& random_cfg = cfg.add_child("random");
			random_cfg["value"] = lexical_cast<std::string>(*ptr);
			ptr = ptr + 1;
		}
		child["side"] = *ptr;
		ptr = ptr + 1;

	} else if (cmd->type == command_pool::ATTACK) {
		config& child = cfg.add_child("attack");
		int* ptr = (int*)(cmd + 1);
		size = *ptr;
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			config& random_cfg = cfg.add_child("random");
			random_cfg["value"] = lexical_cast<std::string>(*ptr);
			ptr = ptr + 1;
			if (*ptr == INT_MAX) {
				ptr = ptr + 1;
				continue;
			}
			config& result_cfg = random_cfg.add_child("results");
			result_cfg["chance"] = lexical_cast<std::string>(*ptr);
			ptr = ptr + 1;
			result_cfg["damage"] = lexical_cast<std::string>(*ptr);
			ptr = ptr + 1;
			result_cfg["dies"] = *ptr? "yes": "no";
			ptr = ptr + 1;
			result_cfg["hits"] = *ptr? "yes": "no";
			ptr = ptr + 1;
		}
		// [duel]
		if (*ptr != INT_MAX) {
			config& duel = child.add_child("duel");
			duel["left"] = *ptr;
			ptr = ptr + 1;
			duel["percentage"] = *ptr;
			ptr = ptr + 1;
			duel["right"] = *ptr;
			ptr = ptr + 1;
		} else {
			ptr = ptr + 1;
		}

		child["attacker_lvl"] = *ptr;
		ptr = ptr + 1;
		child["defender_layer"] = *ptr;
		ptr = ptr + 1;
		child["defender_lvl"] = *ptr;
		ptr = ptr + 1;
		child["defender_weapon"] = *ptr;
		ptr = ptr + 1;
		child["layer"] = *ptr;
		ptr = ptr + 1;
		child["seed"] = *ptr;
		ptr = ptr + 1;
		child["turn"] = *ptr;
		ptr = ptr + 1;
		child["move"] = *ptr;
		ptr = ptr + 1;
		child["constant_attacks"] = *ptr;
		ptr = ptr + 1;
		child["weapon"] = *ptr;
		ptr = ptr + 1;
		config& source = child.add_child("source");
		loc.x = *ptr;
		ptr = ptr + 1;
		loc.y = *ptr;
		ptr = ptr + 1;
		loc.write(source);
		config& destination = child.add_child("destination");
		loc.x = *ptr;
		ptr = ptr + 1;
		loc.y = *ptr;
		ptr = ptr + 1;
		loc.write(destination);

	} else if (cmd->type == command_pool::FORMATION_ATTACK) {
		config& child = cfg.add_child("formation_attack");
		int* ptr = (int*)(cmd + 1);
		size = *ptr;
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			config& random_cfg = cfg.add_child("random");
			random_cfg["value"] = *ptr;
			ptr = ptr + 1;
			if (*ptr == INT_MAX) {
				ptr = ptr + 1;
				continue;
			}
			config& result_cfg = random_cfg.add_child("results");
			result_cfg["chance"] = *ptr;
			ptr = ptr + 1;
			result_cfg["damage"] = *ptr;
			ptr = ptr + 1;
			result_cfg["dies"] = *ptr? "yes": "no";
			ptr = ptr + 1;
			result_cfg["hits"] = *ptr? "yes": "no";
			ptr = ptr + 1;
		}
		// master
		str << *ptr;
		ptr = ptr + 1;
		str << "," << *ptr;
		ptr = ptr + 1;
		child["master"] = str.str();

	} else if (cmd->type == command_pool::DISBAND) {
		config& child = cfg.add_child("disband");
		int* ptr = (int*)(cmd + 1);
		size = *ptr;
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			config& random_cfg = cfg.add_child("random");
			random_cfg["value"] = lexical_cast<std::string>(*ptr);
			ptr = ptr + 1;
		}
		child["value"] = lexical_cast<std::string>(*ptr);
		ptr = ptr + 1;
		child["income"] = lexical_cast<std::string>(*ptr);
		ptr = ptr + 1;
		loc.x = *ptr;
		ptr = ptr + 1;
		loc.y = *ptr;
		ptr = ptr + 1;
		loc.write(child);

	} else if (cmd->type == command_pool::INCHING_BLOCK) {
		config& child = cfg.add_child("inching_block");
		int* ptr = (int*)(cmd + 1);
		size = *ptr;
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			config& random_cfg = cfg.add_child("random");
			random_cfg["value"] = lexical_cast<std::string>(*ptr);
			ptr = ptr + 1;
		}
		child["increase"] = *ptr;
		ptr = ptr + 1;
		loc.x = *ptr;
		ptr = ptr + 1;
		loc.y = *ptr;
		ptr = ptr + 1;
		loc.write(child);

	} else if (cmd->type == command_pool::EVENT) {
		config& child = cfg.add_child("event");
		int* ptr = (int*)(cmd + 1);
		size = *ptr;
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			config& random_cfg = cfg.add_child("random");
			random_cfg["value"] = lexical_cast<std::string>(*ptr);
			ptr = ptr + 1;
		}
		child["type"] = lexical_cast<std::string>(*ptr);
		ptr = ptr + 1;
		loc.x = *ptr;
		ptr = ptr + 1;
		loc.y = *ptr;
		ptr = ptr + 1;
		loc.write(child);

	} else if (cmd->type == command_pool::ASSEMBLE_TREASURE) {
		config& child = cfg.add_child("assemble_treasure");
		int* ptr = (int*)(cmd + 1);
		size = *ptr;
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			config& random_cfg = cfg.add_child("random");
			random_cfg["value"] = lexical_cast<std::string>(*ptr);
			ptr = ptr + 1;
		}
		// diff
		str.str("");
		size = *ptr;
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			if (i == 0) {
				str << *ptr;
			} else {
				str << "," << *ptr;
			}
			ptr = ptr + 1;
		}
		child["diff"] = str.str();

	} else if (cmd->type == command_pool::APPOINT_NOBLE) {
		config& child = cfg.add_child("appoint_noble");
		int* ptr = (int*)(cmd + 1);
		size = *ptr;
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			config& random_cfg = cfg.add_child("random");
			random_cfg["value"] = lexical_cast<std::string>(*ptr);
			ptr = ptr + 1;
		}
		// diff
		str.str("");
		size = *ptr;
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			if (i == 0) {
				str << *ptr;
			} else {
				str << "," << *ptr;
			}
			ptr = ptr + 1;
		}
		child["diff"] = str.str();

	} else if (cmd->type == command_pool::RPG_EXCHANGE) {
		config& child = cfg.add_child("rpg_exchange");
		int* ptr = (int*)(cmd + 1);
		size = *ptr;
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			config& random_cfg = cfg.add_child("random");
			random_cfg["value"] = lexical_cast<std::string>(*ptr);
			ptr = ptr + 1;
		}
		size = *ptr;
		ptr = ptr + 1;
		// heros
		str << *ptr;
		ptr = ptr + 1;
		for (i = 1; i < size; i ++) {
			str << "," << *ptr;
			ptr = ptr + 1;
		}
		child["human"] = str.str();
		child["ai"] = lexical_cast<std::string>(*ptr);
		ptr = ptr + 1;

	} else if (cmd->type == command_pool::ING_TECHNOLOGY) {
		config& child = cfg.add_child("ing_technology");
		int* ptr = (int*)(cmd + 1);
		size = *ptr;
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			config& random_cfg = cfg.add_child("random");
			random_cfg["value"] = lexical_cast<std::string>(*ptr);
			ptr = ptr + 1;
		}
		// id
		size = *ptr;
		ptr = ptr + 1;
		child["id"] = (char*)ptr;

	} else if (cmd->type == command_pool::SCENARIO_ENV) {
		config& child = cfg.add_child("scenario_env");
		int* ptr = (int*)(cmd + 1);
		size = *ptr;
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			config& random_cfg = cfg.add_child("random");
			random_cfg["value"] = lexical_cast<std::string>(*ptr);
			ptr = ptr + 1;
		}
		// duel
		child["duel"] = *ptr;
		ptr = ptr + 1;
		// maximal_defeated_activity
		child["maximal_defeated_activity"] = *ptr;
		ptr = ptr + 1;
		// vip
		child["vip"] = *ptr;
		ptr = ptr + 1;

	} else if (cmd->type == command_pool::BUILD) {
		config& child = cfg.add_child("build");
		int* ptr = (int*)(cmd + 1);
		size = *ptr;
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			config& random_cfg = cfg.add_child("random");
			random_cfg["value"] = lexical_cast<std::string>(*ptr);
			ptr = ptr + 1;
		}
		// city
		str << *ptr;
		ptr = ptr + 1;
		str << "," << *ptr;
		ptr = ptr + 1;
		child["city"] = str.str();
		// artifical
		str.str("");
		str << *ptr;
		ptr = ptr + 1;
		str << "," << *ptr;
		ptr = ptr + 1;
		child["artifical"] = str.str();
		// builder
		str.str("");
		str << *ptr;
		ptr = ptr + 1;
		str << "," << *ptr;
		ptr = ptr + 1;
		child["builder"] = str.str();
		child["cost"] = lexical_cast<std::string>(*ptr);
		ptr = ptr + 1;

		size = *ptr;
		ptr = ptr + 1;
		child["type"] = (char*)ptr;

	} else if (cmd->type == command_pool::CAST_TACTIC) {
		config& child = cfg.add_child("cast_tactic");
		int* ptr = (int*)(cmd + 1);
		size = *ptr;
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			config& random_cfg = cfg.add_child("random");
			random_cfg["value"] = lexical_cast<std::string>(*ptr);
			ptr = ptr + 1;
		}
		// tactician
		str.str("");
		str << *ptr;
		ptr = ptr + 1;
		str << "," << *ptr;
		ptr = ptr + 1;
		child["tactician"] = str.str();
		// special
		str.str("");
		str << *ptr;
		ptr = ptr + 1;
		str << "," << *ptr;
		ptr = ptr + 1;
		child["special"] = str.str();
		// hero
		child["hero"] = *ptr;
		ptr = ptr + 1;
		// consume
		child["consume"] = *ptr;
		ptr = ptr + 1;
		// cost
		child["cost"] = *ptr;
		ptr = ptr + 1;

	} else if (cmd->type == command_pool::ARMORY) {
		config& child = cfg.add_child("armory");
		int* ptr = (int*)(cmd + 1);
		size = *ptr;
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			config& random_cfg = cfg.add_child("random");
			random_cfg["value"] = *ptr;
			ptr = ptr + 1;
		}
		// city
		str << *ptr;
		ptr = ptr + 1;
		str << "," << *ptr;
		ptr = ptr + 1;
		child["city"] = str.str();
		// diff
		str.str("");
		size = *ptr;
		ptr = ptr + 1;
		str << *ptr;
		ptr = ptr + 1;
		for (i = 1; i < size; i ++) {
			str << "," << *ptr;
			ptr = ptr + 1;
		}
		child["diff"] = str.str();

	} else if (cmd->type == command_pool::MOVE_HEROS) {
		config& child = cfg.add_child("move_heros");
		int* ptr = (int*)(cmd + 1);
		size = *ptr;
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			config& random_cfg = cfg.add_child("random");
			random_cfg["value"] = *ptr;
			ptr = ptr + 1;
		}
		// dst
		str << *ptr;
		ptr = ptr + 1;
		str << "," << *ptr;
		ptr = ptr + 1;
		child["dst"] = str.str();
		// heros
		str.str("");
		size = *ptr;
		ptr = ptr + 1;
		str << *ptr;
		ptr = ptr + 1;
		for (i = 1; i < size; i ++) {
			str << "," << *ptr;
			ptr = ptr + 1;
		}
		child["heros"] = str.str();
		// src
		str.str("");
		str << *ptr;
		ptr = ptr + 1;
		str << "," << *ptr;
		ptr = ptr + 1;
		child["src"] = str.str();

	} else if (cmd->type == command_pool::INTERIOR) {
		config& child = cfg.add_child("interior");
		int* ptr = (int*)(cmd + 1);
		size = *ptr;
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			config& random_cfg = cfg.add_child("random");
			random_cfg["value"] = *ptr;
			ptr = ptr + 1;
		}
		// loc
		loc.x = *ptr;
		ptr = ptr + 1;
		loc.y = *ptr;
		ptr = ptr + 1;
		loc.write(child);
		// mayor
		child["mayor"] = *ptr;
		ptr = ptr + 1;

	} else if (cmd->type == command_pool::BELONG_TO) {
		config& child = cfg.add_child("belong_to");
		int* ptr = (int*)(cmd + 1);
		size = *ptr;
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			config& random_cfg = cfg.add_child("random");
			random_cfg["value"] = *ptr;
			ptr = ptr + 1;
		}
		// dst
		str << *ptr;
		ptr = ptr + 1;
		str << "," << *ptr;
		ptr = ptr + 1;
		child["dst"] = str.str();
		// layer
		str.str("");
		child["layer"] = *ptr;
		ptr = ptr + 1;
		// loyalty
		str.str("");
		child["loyalty"] = *ptr;
		ptr = ptr + 1;
		// src
		str.str("");
		str << *ptr;
		ptr = ptr + 1;
		str << "," << *ptr;
		ptr = ptr + 1;
		child["src"] = str.str();

	} else if (cmd->type == command_pool::SET_STATES) {
		config& child = cfg.add_child("set_states");
		int* ptr = (int*)(cmd + 1);
		size = *ptr;
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			config& random_cfg = cfg.add_child("random");
			random_cfg["value"] = *ptr;
			ptr = ptr + 1;
		}
		// dst
		str << *ptr;
		ptr = ptr + 1;
		str << "," << *ptr;
		ptr = ptr + 1;
		child["loc"] = str.str();
		// layer
		str.str("");
		child["layer"] = *ptr;
		ptr = ptr + 1;

		// states
		str.str("");
		size = *ptr;
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			if (i == 0) {
				str << *ptr;
			} else {
				str << "," << *ptr;
			}
			ptr = ptr + 1;
		}
		child["states"] = str.str();

	} else if (cmd->type == command_pool::CLEAR_FORMATIONED) {
		config& child = cfg.add_child("clear_formationed");
		int* ptr = (int*)(cmd + 1);
		size = *ptr;
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			config& random_cfg = cfg.add_child("random");
			random_cfg["value"] = *ptr;
			ptr = ptr + 1;
		}
		// dst
		str << *ptr;
		ptr = ptr + 1;
		str << "," << *ptr;
		ptr = ptr + 1;
		child["loc"] = str.str();
		// layer
		str.str("");
		child["layer"] = *ptr;
		ptr = ptr + 1;

		// cost
		child["cost"] = *ptr;
		ptr = ptr + 1;

	} else if (cmd->type == command_pool::SET_TASK) {
		config& child = cfg.add_child("set_task");
		int* ptr = (int*)(cmd + 1);
		size = *ptr;
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			config& random_cfg = cfg.add_child("random");
			random_cfg["value"] = *ptr;
			ptr = ptr + 1;
		}
		// loc
		str << *ptr;
		ptr = ptr + 1;
		str << "," << *ptr;
		ptr = ptr + 1;
		child["loc"] = str.str();
		// hero
		child["hero"] = *ptr;
		ptr = ptr + 1;
		// task
		child["task"] = *ptr;
		ptr = ptr + 1;
		// at
		str.str("");
		str << *ptr;
		ptr = ptr + 1;
		str << "," << *ptr;
		ptr = ptr + 1;
		child["at"] = str.str();

	} else if (cmd->type == command_pool::CHOOSE) {
		config& child = cfg.add_child("choose");
		int* ptr = (int*)(cmd + 1);
		size = *ptr;
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			config& random_cfg = cfg.add_child("random");
			random_cfg["value"] = *ptr;
			ptr = ptr + 1;
		}
		child["value"] = *ptr;
		ptr = ptr + 1;

	} else if (cmd->type == command_pool::SPEAK) {
		config& child = cfg.add_child("speak");
		int* ptr = (int*)(cmd + 1);
		size = *ptr;
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			config& random_cfg = cfg.add_child("random");
			random_cfg["value"] = *ptr;
			ptr = ptr + 1;
		}
		// side
		if (*ptr) {
			child["side"] = *ptr;
		}
		ptr = ptr + 1;
		// team_name
		size = *ptr;
		ptr = ptr + 1;
		if (size) {
			child["team_name"] = (char*)ptr;
			ptr = ptr + (size + 1 + 3) / 4; // align 4, 1 is '\0'
		}
		// id
		size = *ptr;
		ptr = ptr + 1;
		if (size) {
			child["id"] = (char*)ptr;
			ptr = ptr + (size + 1 + 3) / 4; // align 4, 1 is '\0'
		}
		// message
		size = *ptr;
		ptr = ptr + 1;
		if (size) {
			child["message"] = (char*)ptr;
			ptr = ptr + (size + 1 + 3) / 4; // align 4, 1 is '\0'
		}
		
	} else if (cmd->type == command_pool::CLEAR_LABELS) {
		config& child = cfg.add_child("clear_labels");
		int* ptr = (int*)(cmd + 1);
		size = *ptr;
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			config& random_cfg = cfg.add_child("random");
			random_cfg["value"] = *ptr;
			ptr = ptr + 1;
		}
		// team_name
		size = *ptr;
		ptr = ptr + 1;
		if (size) {
			child["team_name"] = (char*)ptr;
			ptr = ptr + (size + 1 + 3) / 4; // align 4, 1 is '\0'
		}
		
	} else if (cmd->type == command_pool::DIPLOMATISM) {
		config& child = cfg.add_child("diplomatism");
		int* ptr = (int*)(cmd + 1);
		size = *ptr;
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			config& random_cfg = cfg.add_child("random");
			random_cfg["value"] = *ptr;
			ptr = ptr + 1;
		}
		// type=ally
		size = *ptr;
		ptr = ptr + 1;
		if (size) {
			child["type"] = (char*)ptr;
			ptr = ptr + (size + 1 + 3) / 4; // align 4, 1 is '\0'
		}
		// alignment=1
		child["alignment"] = *ptr;
		ptr = ptr + 1;
		// first=1
		child["first"] = *ptr;
		ptr = ptr + 1;
		// hero=102
		child["hero"] = *ptr;
		ptr = ptr + 1;
		// second=2
		child["second"] = *ptr;
		ptr = ptr + 1;
		// strategy=0
		child["strategy"] = *ptr;
		ptr = ptr + 1;
		// target=2
		child["target"] = *ptr;
		ptr = ptr + 1;

	} else if (cmd->type == command_pool::FINAL_BATTLE) {
		config& child = cfg.add_child("final_battle");
		int* ptr = (int*)(cmd + 1);
		size = *ptr;
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			config& random_cfg = cfg.add_child("random");
			random_cfg["value"] = *ptr;
			ptr = ptr + 1;
		}
		// human=1
		child["human"] = *ptr;
		ptr = ptr + 1;
		// ai=2
		child["ai"] = *ptr;
		ptr = ptr + 1;

	} else if (cmd->type == command_pool::INPUT) {
		config& child = cfg.add_child("input");
		int* ptr = (int*)(cmd + 1);
		size = *ptr;
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			config& random_cfg = cfg.add_child("random");
			random_cfg["value"] = *ptr;
			ptr = ptr + 1;
		}
		// value=1
		child["value"] = *ptr;
		ptr = ptr + 1;

	} else if (cmd->type == command_pool::ADD_CARD) {
		config& child = cfg.add_child("add_card");
		int* ptr = (int*)(cmd + 1);
		size = *ptr;
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			config& random_cfg = cfg.add_child("random");
			random_cfg["value"] = *ptr;
			ptr = ptr + 1;
		}
		// side=1
		child["side"] = *ptr;
		ptr = ptr + 1;
		// number=2
		child["number"] = *ptr;
		ptr = ptr + 1;
		// dialog=true
		child["dialog"] = *ptr;
		ptr = ptr + 1;

	} else if (cmd->type == command_pool::ERASE_CARD) {
		config& child = cfg.add_child("erase_card");
		int* ptr = (int*)(cmd + 1);
		size = *ptr;
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			config& random_cfg = cfg.add_child("random");
			random_cfg["value"] = *ptr;
			ptr = ptr + 1;
		}
		// heros
		str.str("");
		size = *ptr;
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			if (i == 0) {
				str << *ptr;
			} else {
				str << "," << *ptr;
			}
			ptr = ptr + 1;
		}
		child["touched"] = str.str();
		// side=1
		child["side"] = *ptr;
		ptr = ptr + 1;
		// index=2
		child["index"] = *ptr;
		ptr = ptr + 1;
		child["dialog"] = *ptr;
		ptr = ptr + 1;
		loc.x = *ptr;
		ptr = ptr + 1;
		loc.y = *ptr;
		ptr = ptr + 1;
		loc.write(child);

	} else if (cmd->type == command_pool::REFORM_CAPTAIN) {
		config& child = cfg.add_child("reform_captain");
		int* ptr = (int*)(cmd + 1);
		size = *ptr;
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			config& random_cfg = cfg.add_child("random");
			random_cfg["value"] = *ptr;
			ptr = ptr + 1;
		}
		// number=1
		child["number"] = *ptr;
		ptr = ptr + 1;
		// join=0/1
		child["join"] = *ptr;
		ptr = ptr + 1;
		// x/y
		loc.x = *ptr;
		ptr = ptr + 1;
		loc.y = *ptr;
		ptr = ptr + 1;
		loc.write(child);

	} else if (cmd->type == command_pool::ACTIVE_TACTIC) {
		config& child = cfg.add_child("active_tactic");
		int* ptr = (int*)(cmd + 1);
		size = *ptr;
		ptr = ptr + 1;
		for (i = 0; i < size; i ++) {
			config& random_cfg = cfg.add_child("random");
			random_cfg["value"] = *ptr;
			ptr = ptr + 1;
		}
		// add=1
		child["add"] = *ptr;
		ptr = ptr + 1;
		// unit=1
		child["unit"] = *ptr;
		ptr = ptr + 1;
		// hero=2
		child["hero"] = *ptr;
		ptr = ptr + 1;

	} else {
		std::stringstream str;
		str << "command_2_config, unknown type(int): " << cmd->type;
		replay::process_error(str.str());
	}
}

void replay::pool_2_config(config& cfg)
{
	unsigned char* cache = (unsigned char*)malloc(POOL_ALLOC_CACHE_SIZE);
	gzip_segment_t seg;
	int pool_data_gzip_size = 0;

	seg.min = seg.max = -1;
	for (int i = 0; i < pool_pos_vsize_; i ++) {
		config& cmd = cfg.add_child("command");
		command_2_config(command_addr2(i, seg, pool_data_gzip_size, cache), cmd);
		cmd["type"] = command_addr2(i, seg, pool_data_gzip_size, cache)->type;
	}
	delete cache;

	if (!cfg_.empty()) {
		cfg.add_child("command", cfg_);
		// cfg.get_children("command")[pos_ - 1]->remove_attribute("type");
		// cfg.get_children("command")[pos_ - 1]->remove_attribute("sent");
	}
}

void replay::from_config(const config& cfg)
{
	BOOST_FOREACH (const config &cmd_cfg, cfg.child_range("command")) {
		command_pool::command* cmd = command_pool::add_command();
		config_2_command(cmd_cfg, cmd);
	}
}

void replay::get_replay_data(command_pool& that)
{
	if (!cfg_.empty()) {
		command_pool::command* cmd = command_pool::add_command();
		config_2_command(cfg_, cmd);
		cfg_.clear();

		// if has a undoable command, it must be send over network before "reset" current_gzip_seg_.
		// of course, should disable "undo" button on ui.
		play_controller& controller = *resources::controller;
		if (!controller.undo_stack().empty()) {
			controller.sync_undo();
		}
	}
	if (use_gzip_ && pool_data_vsize_) {
		// encode pool_data
		gzip_segment_t seg;
		seg.min = current_gzip_seg_.min;
		seg.max = pool_pos_vsize_ - 1;
		
		seg.len = gzip_codec(cache_, pool_data_vsize_, true);
		pool_data_vsize_ = 0;
		// once recallocate, gzip_codec maybe modify pool_data_, so dest must locate after gzip_codec. 
		uint8_t* dest = pool_data_ + pool_data_gzip_size_;
		pool_data_gzip_size_ += sizeof(gzip_segment_t) + seg.len;
		current_gzip_seg_.min = current_gzip_seg_.max = -1;

		memcpy(dest, &seg, sizeof(gzip_segment_t));
	}

	that = *(dynamic_cast<command_pool*>(this));
}

void replay::start_replay()
{
	pos_ = 0;
}

// [read].replay
void replay::revert_action()
{
	if (pos_ > 0)
		--pos_;
}

// [read].replay
config* replay::get_next_action()
{
	if (pos_ >= pool_pos_vsize_)
		return NULL;

	cfg_.clear();
	command_2_config(command_addr(pos_), cfg_);
	current_ = &cfg_;
	set_random(current_);
	++ pos_;
	return current_;
}

// [read].replay
void replay::pre_replay()
{
	if ((rng::random() == NULL) && (pool_pos_vsize_ > 0)){
		if (at_end()) {
			add_command(true);
		} else {
			cfg_.clear();
			command_2_config(command_addr(pos_), cfg_);
			set_random(&cfg_);
		}
	}
}

bool replay::at_end() const
{
	return pos_ >= pool_pos_vsize_;
}

void replay::set_to_end()
{
	pos_ = pool_pos_vsize_;
	current_ = NULL;
	set_random(NULL);
}

// [write]
void replay::clear()
{
	message_locations.clear();
	message_log.str(std::string());
	cfg_ = config();
	pos_ = 0;
	current_ = NULL;
	set_random(NULL);
	skip_ = 0;
	command_pool::clear();
}

// [read]
bool replay::empty()
{
	return pool_pos_vsize_? false: true;
}

void replay::add_config(const config& cfg, MARK_SENT mark)
{
	BOOST_FOREACH (const config &cmd_cfg, cfg.child_range("command")) {
		if (!cfg_.empty()) {
			command_pool::command* cmd = command_pool::add_command();
			config_2_command(cfg_, cmd);
			if (cfg_["sent"] == "yes") {
				cmd->flags = 1;
			}
		}
		cfg_ = cmd_cfg;
		if (cfg_.child("speak")) {
			// <attention 2>
			cfg_["type"] = str_cast(command_pool::SPEAK);

			pos_ = pool_pos_vsize_ + 1;
			add_chat_message_location();
		}
		if (mark == MARK_AS_SENT) {
			cfg_["sent"] = "yes";
		}
	}
}

namespace {

replay* replay_src = NULL;

struct replay_source_manager
{
	replay_source_manager(replay* o) : old_(replay_src)
	{
		replay_src = o;
	}

	~replay_source_manager()
	{
		replay_src = old_;
	}

private:
	replay* const old_;
};

}

replay& get_replay_source()
{
	if(replay_src != NULL) {
		return *replay_src;
	} else {
		return recorder;
	}
}

bool do_replay(int side_num, replay *obj)
{
	log_scope("do replay");

	const replay_source_manager replaymanager(obj);

//	replay& replayer = (obj != NULL) ? *obj : recorder;

	if (!get_replay_source().is_skipping()){
		resources::screen->recalculate_minimap();
	}

	const rand_rng::set_random_generator generator_setter(&get_replay_source());

	update_locker lock_update(resources::screen->video(),get_replay_source().is_skipping());
	return do_replay_handle(side_num, "");
}

bool do_replay_handle(int side_num, const std::string &do_untill)
{
	//a list of units that have promoted from the last attack
	std::deque<map_location> advancing_units;

	std::vector<team>& teams = *resources::teams;
	team& current_team = teams[side_num - 1];
	unit_map& units = *resources::units;
	hero_map& heros = *resources::heros;
	play_controller& controller = *resources::controller;
	game_display& gui = *resources::screen;

	for(;;) {
		const config *cfg = get_replay_source().get_next_action();

		//do we need to recalculate shroud after this action is processed?

		bool fix_shroud = false;
		if (cfg)
		{
			DBG_REPLAY << "Replay data:\n" << *cfg << "\n";
		}
		else
		{
			DBG_REPLAY << "Replay data at end\n";
		}

		//if there is nothing more in the records
		if (cfg == NULL) {
			//replayer.set_skip(false);
			return false;
		}

		//if we are expecting promotions here`
		if (!get_replay_source().expected_advancements().empty()) {
			//if there is a promotion, we process it and go onto the next command
			//but if this isn't a promotion, we just keep waiting for the promotion
			//command -- it may have been mixed up with other commands such as messages
			if (const config &child = cfg->child("choose")) {
				const int val = lexical_cast_default<int>(child["value"]);
				unit* u = get_replay_source().expected_advancements().front();
				dialogs::animate_unit_advancement(*u, val);
				get_replay_source().pop_expected_advancement();

				//if there are no more advancing units, then we check for victory,
				//in case the battle that led to advancement caused the end of scenario
				if(advancing_units.empty()) {
					resources::controller->check_victory();
				}
				continue;
			}
		}

		// We return if caller wants it for this tag
		if (!do_untill.empty() && cfg->child(do_untill) != NULL) {
			get_replay_source().revert_action();
			return false;
		}

		config::all_children_itors ch_itors = cfg->all_children_range();
		//if there is an empty command tag, create by pre_replay() or a start tag
		if (ch_itors.first == ch_itors.second || cfg->child("start")) {
			// do nothing

		} else if (const config &child = cfg->child("speak")) {
			const std::string &team_name = child["team_name"];
			const std::string &speaker_name = child["id"];
			const std::string &message = child["message"];
			//if (!preferences::parse_should_show_lobby_join(speaker_name, message)) return;
			bool is_whisper = (speaker_name.find("whisper: ") == 0);
			get_replay_source().add_chat_message_location();
			if (!get_replay_source().is_skipping() || is_whisper) {
				const int side = lexical_cast_default<int>(child["side"],0);
				resources::screen->add_chat_message(time(NULL), speaker_name, side, message,
						(team_name.empty() ? events::chat_handler::MESSAGE_PUBLIC
						: events::chat_handler::MESSAGE_PRIVATE),
						preferences::message_bell());
			}
		}
		else if (const config &child = cfg->child("label"))
		{
			terrain_label label(resources::screen->labels(), child);

			resources::screen->labels().set_label(label.location(),
						label.text(),
						label.team_name(),
						label.color());
		}
		else if (const config &child = cfg->child("clear_labels"))
		{
			resources::screen->labels().clear(std::string(child["team_name"]), child["force"].to_bool());
		}
		else if (const config &child = cfg->child("rename"))
		{
			
		}
		else if (const config& child = cfg->child("init_side"))
		{
			// if strategy mistaken, it will result complex fault, check out as soon as early.
			const std::vector<strategy>& strategies = current_team.strategies();
			const std::vector<std::string>& vsize = utils::split(child["strategies"].str());
			std::stringstream errbuf;

			if (strategies.size() * 2 != vsize.size()) {
				errbuf << current_team.name() << "'s strategy is out of synchrnize!";
				replay::process_error(errbuf.str());
			}
			for (size_t i = 0; i < vsize.size(); i += 2) {
				int target = lexical_cast<int>(vsize[i]);
				int type = lexical_cast<int>(vsize[i + 1]);
				const strategy& s = strategies[i / 2];
				if (s.target_ != target || s.type_ != type) {
					errbuf << current_team.name() << "'s strategy is out of synchrnize!";
					replay::process_error(errbuf.str());
				}
			}

			resources::controller->do_init_side(side_num - 1);
		}

		//if there is an end turn directive
		else if (cfg->child("end_turn"))
		{
			// return false;
		}
		else if (cfg->child("init_ai"))
		{
			std::vector<mr_data> mrs;
			units.calculate_mrs_data(*resources::state_of_game, mrs, side_num);
		}
		else if (const config& child = cfg->child("prefix_unit"))
		{
			VALIDATE(!unit::actor, "do_replay_handle, unit::actor isn't NULL!");

			if (controller.is_replaying() && child["new_turn"].to_int()) {
				// first is with new_turn = 1, after process, is push back to replay pool. of course it is with new_turn = 1.
				// second should ignore this. so use "main_ticks !=" condition.
				int end_ticks = calculate_end_ticks();
				if (!tent::turn_based) {
					if (unit_map::main_ticks != (controller.turn() - 1) * game_config::ticks_per_turn) {
						if (unit_map::main_ticks < end_ticks) {
							units.do_escape_ticks_uh(teams, gui, end_ticks - unit_map::main_ticks, false);
							get_replay_source().revert_action();
							return true;
						}
					}
				} else {
					if (child["end"].to_int() != end_ticks) {
						VALIDATE(end_ticks - unit_map::main_ticks == game_config::ticks_per_turn, "ticks of turn_based must be gap one!");
						unit_map::main_ticks = end_ticks;
						get_replay_source().revert_action();
						return true;
					}
				}
			}
			
			int end_ticks = child["end"].to_int();

			const unit& u = units.current_unit();
			if (!tent::turn_based) {
				int past_ticks = u.backward_ticks(u.ticks());
				VALIDATE(unit_map::main_ticks + past_ticks < end_ticks, "do_replay_handle, unit_map::main_ticks + past_ticks >= end_ticks!");
				units.do_escape_ticks_uh(teams, gui, past_ticks, true);
			}

			controller.do_prefix_unit(end_ticks, true, true);

			return false;
		}
		else if (const config& child = cfg->child("post_unit"))
		{
			controller.do_post_unit(true);

			return true;
		}
		else if (const config& child = cfg->child("fresh_heros"))
		{
			do_fresh_heros(current_team, true);
		}
		else if (const config& child = cfg->child("do_commoner"))
		{
			std::vector<std::pair<unit*, int> > gotos;
			controller.calculate_commoner_gotos(current_team, gotos);
		}
		else if (const config &child = cfg->child("recruit")) {
			int cost = lexical_cast_default<int>(child["cost"]);

			map_location loc(child, resources::state_of_game);

			const unit_type* ut = unit_types.find(child["type"].str());
			if (!ut) {
				std::stringstream errbuf;
				errbuf << "recruiting illegal unit: '" << child["type"].str() << "'\n";
				replay::process_error(errbuf.str());
			}

			artifical* city = units.city_from_loc(loc);
			if (!city) {
				std::stringstream errbuf;
				errbuf << "cannot recruit unit: " << loc << "\n";
				replay::process_error(errbuf.str());
			}

			if (ut->master() == HEROS_INVALID_NUMBER) {
				const std::vector<std::string> heros_army = utils::split(child["heros"]);
				std::vector<const hero*> v;
				for (std::vector<std::string>::const_iterator itor = heros_army.begin(); itor != heros_army.end(); ++ itor) {
					int number = lexical_cast_default<int>(*itor);
					hero& h = heros[number];
					v.push_back(&h);
				}
				bool human = child["human"].to_int()? true: false;

				do_recruit(units, heros, teams, current_team, ut, v, *city, cost, human, false, *resources::state_of_game);
				resources::screen->invalidate(loc);

			} else {
				resources::controller->generate_commoner(*city, ut, true);
			}

		} else if (const config &child = cfg->child("armory")) {
			std::vector<std::string> vector_str = utils::split(child["city"]);
			map_location src(lexical_cast_default<int>(vector_str[0]) - 1, lexical_cast_default<int>(vector_str[1]) - 1);
			artifical* city = resources::units->city_from_loc(src);

			const std::vector<std::string> diff = utils::split(child["diff"]);
			hero_map& heros = *resources::heros;

			// move heros from src_city to dst_city
			std::vector<unit*>& reside_troops = city->reside_troops();
			std::vector<hero*>& fresh_heros = city->fresh_heros();
			std::vector<hero*> previous_captains, current_captains;

			for (std::vector<std::string>::const_iterator itor = diff.begin(); itor != diff.end(); ) {
				size_t index = lexical_cast_default<size_t>(*itor);
				++ itor;
				size_t size = lexical_cast_default<size_t>(*itor);
				++ itor;
				unit& u = *reside_troops[index];
				previous_captains.push_back(&u.master());
				if (u.second().valid()) {
					previous_captains.push_back(&u.second());
				}
				if (u.third().valid()) {
					previous_captains.push_back(&u.third());
				}
				std::vector<hero*> captains;
				for (size_t i = 0; i < size; i ++) {
					captains.push_back(&heros[lexical_cast_default<size_t>(*itor)]);
					current_captains.push_back(captains.back());
					++ itor;
				}
				u.replace_captains(captains);
			}
			std::copy(previous_captains.begin(), previous_captains.end(), std::back_inserter(fresh_heros));
			for (std::vector<hero*>::const_iterator itor = current_captains.begin(); itor != current_captains.end(); ++ itor) {
				std::vector<hero*>::iterator itor1 = std::find(fresh_heros.begin(), fresh_heros.end(), *itor);
				fresh_heros.erase(itor1);
			}
			for (std::vector<hero*>::iterator h = fresh_heros.begin(); h != fresh_heros.end(); ++ h) {
				(*h)->status_ = hero_status_idle;
			}
			std::sort(fresh_heros.begin(), fresh_heros.end(), compare_leadership);

			resources::screen->invalidate(city->get_location());

			game_events::fire("post_armory", city->get_location());

		} else if (const config &child = cfg->child("move_heros")) {
			std::vector<std::string> vector_str = utils::split(child["src"]);
			map_location src(lexical_cast_default<int>(vector_str[0]) - 1, lexical_cast_default<int>(vector_str[1]) - 1);
			artifical* src_city = resources::units->city_from_loc(src);

			vector_str = utils::split(child["dst"]);
			map_location dst(lexical_cast_default<int>(vector_str[0]) - 1, lexical_cast_default<int>(vector_str[1]) - 1);
			artifical* dst_city = resources::units->city_from_loc(dst);

			const std::vector<std::string> checked_heros = utils::split(child["heros"]);
			// hero_map& heros = *resources::heros;

			// move heros from src_city to dst_city
			std::vector<hero*>& src_heros = src_city->fresh_heros();
			std::vector<hero*>& dst_heros = dst_city->finish_heros();
			size_t has_erase_heros = 0;

			for (std::vector<std::string>::const_iterator itor = checked_heros.begin(); itor != checked_heros.end(); ++ itor) {
				size_t index = lexical_cast_default<size_t>(*itor);
				hero* h = src_heros[index - has_erase_heros];
				src_city->hero_go_out(*h);
				dst_city->move_into(*h);
				has_erase_heros ++;
			}
			resources::screen->invalidate(src_city->get_location());
			resources::screen->invalidate(dst_city->get_location());

			game_events::fire("post_moveheros", dst_city->get_location(), src_city->get_location());

		} else if (const config &child = cfg->child("interior")) {
			map_location loc(child, resources::state_of_game);
			artifical* city = units.city_from_loc(loc);
			if (!city) {
				std::stringstream errbuf;
				errbuf << "cannot find city: " << loc << "\n";
				replay::process_error(errbuf.str());
			}
			hero& mayor = heros[child["mayor"].to_int()];
			city->select_mayor(&mayor);

		} else if (const config &child = cfg->child("belong_to")) {
			int layer = child["layer"].to_int();
			bool loyalty = child["loyalty"].to_int()? true: false;
			std::vector<std::string> vector_str = utils::split(child["src"]);
			map_location src(lexical_cast_default<int>(vector_str[0]) - 1, lexical_cast_default<int>(vector_str[1]) - 1);
			unit* u = &*resources::units->find(src, (layer == unit_map::OVERLAY)? true: false);

			vector_str = utils::split(child["dst"]);
			map_location dst(lexical_cast_default<int>(vector_str[0]) - 1, lexical_cast_default<int>(vector_str[1]) - 1);
			artifical* to = resources::units->city_from_loc(dst);

			to->unit_belong_to(u, loyalty);
			resources::screen->invalidate(to->get_location());

		} else if (const config &child = cfg->child("set_states")) {
			int layer = child["layer"].to_int();

			std::vector<std::string> vstr = utils::split(child["loc"]);
			map_location loc(lexical_cast_default<int>(vstr[0]) - 1, lexical_cast_default<int>(vstr[1]) - 1);
			unit& u = *units.find(loc, (layer == unit_map::OVERLAY)? true: false);

			vstr = utils::split(child["states"]);
			for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
				ustate_tag::state_t state = (ustate_tag::state_t)lexical_cast_default<int>(*it);
				++ it;
				bool value = lexical_cast_default<int>(*it)? true: false;
				u.set_state(state, value);
			}

		} else if (const config &child = cfg->child("clear_formationed")) {
			int layer = child["layer"].to_int();
			int cost = child["cost"].to_int();

			std::vector<std::string> vstr = utils::split(child["loc"]);
			map_location loc(lexical_cast_default<int>(vstr[0]) - 1, lexical_cast_default<int>(vstr[1]) - 1);
			unit& u = *units.find(loc, (layer == unit_map::OVERLAY)? true: false);

			do_clear_formationed(gui, teams, units, u, cost, false);

		} else if (const config &child = cfg->child("set_task")) {
			int number = child["hero"].to_int();
			unit& u = *units.find_unit(heros[number]);

			std::vector<std::string> vstr = utils::split(child["loc"]);
			map_location loc(lexical_cast_default<int>(vstr[0]) - 1, lexical_cast_default<int>(vstr[1]) - 1);
			int task = child["task"].to_int();
			vstr = utils::split(child["at"]);
			map_location at(lexical_cast_default<int>(vstr[0]) - 1, lexical_cast_default<int>(vstr[1]) - 1);
			do_set_task(units, u, task, at, false);
			
		} else if (const config &child = cfg->child("build")) {
			std::vector<std::string> vector_str = utils::split(child["builder"]);
			map_location builder_loc(lexical_cast_default<int>(vector_str[0]) - 1, lexical_cast_default<int>(vector_str[1]) - 1);
			unit* builder = resources::units->find_unit(builder_loc);

			int cost = child["cost"].to_int();

			vector_str = utils::split(child["artifical"]);
			map_location art_loc(lexical_cast_default<int>(vector_str[0]) - 1, lexical_cast_default<int>(vector_str[1]) - 1);

			const unit_type* ut = unit_types.find(child["type"]);

			resources::controller->do_build(current_team, builder, ut, art_loc, cost);

			fix_shroud = !get_replay_source().is_skipping();
			game_events::fire("post_build", art_loc, builder_loc);

		} else if (const config &child = cfg->child("cast_tactic")) {
			hero_map& heros = *resources::heros;
			std::vector<std::string> vector_str = utils::split(child["tactician"]);
			map_location tactician_loc(lexical_cast_default<int>(vector_str[0]) - 1, lexical_cast_default<int>(vector_str[1]) - 1);
			unit& tactician = *units.find(tactician_loc);

			vector_str = utils::split(child["special"]);
			map_location special_loc(lexical_cast_default<int>(vector_str[0]) - 1, lexical_cast_default<int>(vector_str[1]) - 1);
			unit* special = NULL;
			if (special_loc.valid()) {
				special = &*units.find(special_loc);
			}

			hero& h = heros[child["hero"].to_int()];
			bool consume = child["consume"].to_int()? true: false;
			int cost = child["cost"].to_int();

			cast_tactic(teams, units, tactician, h, special, false, consume, cost);

			if (tactician.advances()) {
				get_replay_source().add_expected_advancement(&tactician);
			}

		} else if (const config& child = cfg->child("employ")) {
			int number = child["hero"].to_int();
			int cost = child["cost"].to_int();

			do_employ(*resources::controller, units, current_team, heros[number], cost, true);

		} else if (const config& child = cfg->child("purchase")) {
			int type = child["type"].to_int();
			int cost = child["cost"].to_int();

			do_purchase(gui, current_team, type, cost, true);

		} else if (const config &child = cfg->child("disband")) {
			const std::string& disband_index = child["value"];
			int income = child["income"].to_int(0);
			const int index = lexical_cast_default<int>(disband_index);

			map_location loc(child, resources::state_of_game);

			if (index >= 0) {
				artifical* disband_city = units.city_from_loc(loc);

				std::vector<unit*>& reside_troops = disband_city->reside_troops();
				unit& u = *reside_troops[index];
				// sort_units(reside_troops);

				if (index >= int(reside_troops.size())) {
					replay::process_error("illegal disband\n");
				}

				// add gold to the gold amount
				current_team.spend_gold(-1 * income);
				// add hero to the hero list in city
				u.master().status_ = hero_status_idle;
				disband_city->fresh_heros().push_back(&u.master());
				if (u.second().valid()) {
					u.second().status_ = hero_status_idle;
					disband_city->fresh_heros().push_back(&u.second());
				}
				if (u.third().valid()) {
					u.third().status_ = hero_status_idle;
					disband_city->fresh_heros().push_back(&u.third());
				}
				// erase this unit from reside troop
				disband_city->troop_go_out(u);
				gui.invalidate(loc);
			} else {
				unit& u = *units.find(loc, (index == -1 * unit_map::OVERLAY)? true: false);
				do_demolish(gui, units, current_team, &u, income, false);
			}

			game_events::fire("post_disband", loc);

		} else if (const config &child = cfg->child("inching_block")) {
			bool increase = child["increase"].to_int()? true: false;

			map_location loc(child, NULL);

			unit& u = *units.find(loc);
			u.inching_block_turns(increase);						

		} else if (const config &child = cfg->child("event")) {
			int type = child["type"].to_int();

			map_location loc(child, NULL);

			if (type == replay::EVENT_ENCOURAGE) {
				unit& u = *resources::units->find(loc);
				hero* h2 = u.can_encourage();
				u.do_encourage(u.master(), *h2);
				resources::screen->invalidate(loc);

			} else if (type == replay::EVENT_RPG_INDEPENDENCE) {
				resources::controller->rpg_independence(true);

			} else if (type == replay::EVENT_FIND_TREASURE) {
				current_team.find_treasure(heros, *resources::controller, loc.x);
			}			

		} else if (const config &child = cfg->child("assemble_treasure")) {
			const std::vector<std::string> diff_vstr = utils::split(child["diff"]);

			std::vector<size_t>& holded_treasures = current_team.holded_treasures();
			std::map<int, int> diff;
			std::set<unit*> diff_troops;
			for (std::vector<std::string>::const_iterator it = diff_vstr.begin(); it != diff_vstr.end(); ++ it) {
				int number = lexical_cast_default<int>(*it);
				hero& h = heros[number];
				if (h.treasure_ != HEROS_NO_TREASURE) {
					holded_treasures.push_back(h.treasure_);
					h.treasure_ = HEROS_NO_TREASURE;
				}
				++ it;
				int t0 = lexical_cast_default<int>(*it);
				diff[number] = t0;
			}
			std::sort(holded_treasures.begin(), holded_treasures.end());

			for (std::map<int, int>::const_iterator it = diff.begin(); it != diff.end(); ++ it) {
				hero& h = heros[it->first];
				h.treasure_ = it->second;
				if (h.treasure_ != HEROS_NO_TREASURE) {
					std::vector<size_t>::iterator it2 = std::find(holded_treasures.begin(), holded_treasures.end(), h.treasure_);
					holded_treasures.erase(it2);
				}

				unit* u = units.find_unit(h);
				if (!u->is_artifical()) {
					diff_troops.insert(u);
				}
			}
			for (std::set<unit*>::iterator it = diff_troops.begin(); it != diff_troops.end(); ++ it) {
				unit& u = **it;
				u.adjust();
			}
			
		} else if (const config &child = cfg->child("appoint_noble")) {
			const std::vector<std::string> diff_vstr = utils::split(child["diff"]);

			std::map<int, int> diff;
			std::set<unit*> diff_troops;
			for (std::vector<std::string>::const_iterator it = diff_vstr.begin(); it != diff_vstr.end(); ++ it) {
				int number = lexical_cast_default<int>(*it);
				++ it;
				int t0 = lexical_cast_default<int>(*it);
				diff[number] = t0;
			}

			for (std::map<int, int>::const_iterator it = diff.begin(); it != diff.end(); ++ it) {
				hero& h = heros[it->first];

				// don't use current_team.appoint_noble(h, it->second, true)
				// appoint_noble need according to adjust sequence. but diff is a result.
				h.noble_ = it->second;
				
				unit* u = units.find_unit(h);
				if (!u->is_artifical()) {
					diff_troops.insert(u);
				}
			}
			current_team.appointed_nobles(true);

			for (std::set<unit*>::iterator it = diff_troops.begin(); it != diff_troops.end(); ++ it) {
				unit& u = **it;
				u.adjust();
			}
			
		} else if (const config &child = cfg->child("rpg_exchange")) {
			const std::vector<std::string> human_pairs = utils::split(child["human"]);
			std::vector<size_t> v;
			for (std::vector<std::string>::const_iterator itor = human_pairs.begin(); itor != human_pairs.end(); ++ itor) {
				int number = lexical_cast_default<int>(*itor);
				v.push_back(number);
			}
			int ai_pair = child["ai"].to_int();
			resources::controller->rpg_exchange(v, ai_pair);

		} else if (const config &child = cfg->child("ing_technology")) {
			const std::string& id = child["id"].str();
			const ttechnology& t = unit_types.technologies().find(id)->second;
			current_team.select_ing_technology(&t);

		} else if (const config &child = cfg->child("scenario_env")) {
			int duel = child["duel"].to_int();
			int maximal_defeated_activity = child["maximal_defeated_activity"].to_int();
			bool vip = child["vip"].to_int()? true: false;

			tscenario_env env(duel, maximal_defeated_activity, vip);
			controller.do_scenario_env(env, false);

		} else if (const config &child = cfg->child("countdown_update")) {
			const std::string &num = child["value"];
			const int val = lexical_cast_default<int>(num);
			const std::string &tnum = child["team"];
			const int tval = lexical_cast_default<int>(tnum);
			if (tval <= 0  || tval > int(resources::teams->size())) {
				std::stringstream errbuf;
				errbuf << "Illegal countdown update \n"
					<< "Received update for :" << tval << " Current user :"
					<< side_num << "\n" << " Updated value :" << val;

				replay::process_error(errbuf.str());
			} else {
				(*resources::teams)[tval - 1].set_countdown_time(val);
			}
		} else if (cfg->child("move") || cfg->child("expedite")) {
			const config* child;
			if (cfg->child("move")) {
				child = &cfg->child("move");
			} else {
				child = &cfg->child("expedite");
			}

			bool direct = child->get("direct")->to_int()? true: false;
			const std::string x = *child->get("x");
			const std::string y = *child->get("y");
			std::vector<map_location> steps = parse_location_range(resources::game_map, x, y);
			int cost = *child->get("cost");

			if (steps.empty()) {
				replay::process_error("do_replay_handle, Warning: Missing path data found in [move]!");
			}
			if (direct && steps.size() != 2) {
				replay::process_error("do_replay_handle, direct mode, bust step size isn't 2!");
			}

			map_location src = steps.front();
			map_location dst = steps.back();

			unit_map::iterator u = units.find(dst);
			if (src == dst) {
				// Magic: Move with identical source and destination. set movement_ to 0.
				u->set_movement(0);
				continue;
			}
			
			// destination maybe already occupied! mover may cannot reach dst loc! for example encount to alert attack.
			if (u.valid() && !unit_is_city(&*u)) {
				std::stringstream errbuf;
				errbuf << "destination already occupied: "
				       << dst << '\n';
				replay::process_error(errbuf.str());
			}

			u = units.find(src);
			if (!u.valid()) {
				std::stringstream errbuf;
				errbuf << "unfound location for source of movement: "
				       << src << " -> " << dst << '\n';
				replay::process_error(errbuf.str());
			}

			artifical* expedite_city = NULL;
			bool expedite_troop = false;
			int expedite_index = -1;
			if (cfg->child("expedite")) {
				expedite_troop = child->get("troop")->to_int()? true: false;
				expedite_index = child->get("value")->to_int();

				expedite_city = resources::units->city_from_loc(src);

				size_t reside_size = expedite_city->reside_troops().size();
				if (!expedite_troop) {
					reside_size = expedite_city->reside_commoners().size();
				}
				if (expedite_index >= 0 && expedite_index < int(reside_size)) {
					if (!direct) {
						units.set_expediting(expedite_city, expedite_troop, expedite_index);
						gui.place_expedite_city(*expedite_city);
					}
				} else {
					replay::process_error("illegal expediting index\n");
				}

			} 
			
			if (!direct) {
				::move_unit(NULL, steps, NULL, NULL, true, NULL, true, true, true);
			} else {
				if (cfg->child("expedite")) {
					do_direct_expedite(teams, units, *expedite_city, expedite_index, dst, false);
				} else {
					do_direct_move(teams, units, *resources::game_map, *u, dst, cost, false);
				}
				continue;
			}

			// notic: mover maybe died! (when alert attack)

			if (cfg->child("expedite")) {
				bool zero_movement = child->get("zero")->to_int()? true: false;
				if (zero_movement) {
					units.find(steps.back())->set_movement(0);
				}

				gui.invalidate(steps.front());
			}

			//NOTE: The AI fire sighetd event whem moving in the FoV of team 1
			// (supposed to be the human player in SP)
			// That's ugly but let's try to make the replay works like that too
			if (side_num != 1 && teams.front().fog_or_shroud() && !teams.front().fogged(dst)
					 && (current_team.is_ai()))
			{
				// the second parameter is impossible to know
				// and the AI doesn't use it too in the local version
				game_events::fire("sighted",dst);
			}

			game_events::fire("sitdown", dst, map_location());

		} else if (const config &child = cfg->child("bomb")) {
			int bomb_side = child["side"].to_int();
			team& bomb_team = teams[bomb_side - 1];

			do_bomb(gui, bomb_team, false);
		} 

		else if (const config &child = cfg->child("attack"))
		{
			const config &destination = child.child("destination");
			const config &source = child.child("source");

			if (!destination || !source) {
				replay::process_error("no destination/source found in attack\n");
			}

			//we must get locations by value instead of by references, because the iterators
			//may become invalidated later
			const map_location src(source, resources::state_of_game);
			const map_location dst(destination, resources::state_of_game);

			const std::string &weapon = child["weapon"];
			const int weapon_num = lexical_cast_default<int>(weapon);
			
			int layer = child["layer"].to_int();
			int defender_layer = child["defender_layer"].to_int();
			bool move = child["move"].to_int()? true: false;
			bool constant_attacks = child["constant_attacks"].to_int()? true: false;

			const std::string &def_weapon = child["defender_weapon"];
			int def_weapon_num = -1;
			if (def_weapon.empty()) {
				// Let's not gratuitously destroy backwards compat.
				ERR_REPLAY << "Old data, having to guess weapon\n";
			} else {
				def_weapon_num = lexical_cast_default<int>(def_weapon);
			}

			unit_map::iterator u = resources::units->find(src, (layer == unit_map::OVERLAY)? true: false);
			if (!u.valid()) {
				std::stringstream errbuf;
				errbuf << "unfound location(" << src << ") for source of attack\n";
				replay::process_error(errbuf.str());
			}

			if (size_t(weapon_num) >= u->attacks().size()) {
				replay::process_error("illegal weapon type in attack\n");
			}

			unit_map::iterator tgt = units.find(dst, (defender_layer == unit_map::OVERLAY)? true: false);

			if (!tgt.valid()) {
				std::stringstream errbuf;
				errbuf << "unfound defender for attack: " << src << " -> " << dst << '\n';
				replay::process_error(errbuf.str());
			}

			if (def_weapon_num >=
					static_cast<int>(tgt->attacks().size())) {

				replay::process_error("illegal defender weapon type in attack\n");
			}

			rand_rng::seed_t seed = lexical_cast<rand_rng::seed_t>(child["seed"]);
			rand_rng::set_seed(seed);

			std::pair<map_location, map_location> to_locs = attack_unit(*u, *tgt, weapon_num, def_weapon_num, !get_replay_source().is_skipping(), child.child("duel"), move, constant_attacks);

			u = units.find(to_locs.first);
			tgt = units.find(to_locs.second);

			if (u.valid() && u->advances()) {
				get_replay_source().add_expected_advancement(&*u);
			}
			
			if (tgt.valid() && tgt->advances()) {
				get_replay_source().add_expected_advancement(&*tgt);
			}

			//check victory now if we don't have any advancements. If we do have advancements,
			//we don't check until the advancements are processed.
			if(get_replay_source().expected_advancements().empty()) {
				resources::controller->check_victory();
			}
			fix_shroud = !get_replay_source().is_skipping();
		}
		else if (const config &child = cfg->child("formation_attack"))
		{
			std::vector<std::string> vstr = utils::split(child["master"]);
			map_location master_loc(lexical_cast_default<int>(vstr[0]) - 1, lexical_cast_default<int>(vstr[1]) - 1);

			do_formation_attack(teams, units, gui, current_team, *units.find_unit(master_loc), true, true);

		}
		else if (const config &child = cfg->child("fire_event"))
		{
			BOOST_FOREACH (const config &v, child.child_range("set_variable")) {
				resources::state_of_game->set_variable(v["name"], v["value"]);
			}
			const std::string &event = child["raise"];
			//exclude these events here, because in a replay proper time of execution can't be
			//established and therefore we fire those events inside play_controller::init_side
			if ((event != "side turn") && (event != "turn 1") && (event != "new turn") && (event != "turn refresh")){
				if (const config &source = child.child("source")) {
					game_events::fire(event, map_location(source, resources::state_of_game));
				} else {
					game_events::fire(event);
				}
			}

		}
		else if (const config &child = cfg->child("diplomatism"))
		{
			int emissary = child["hero"].to_int();
			int my_side = child["first"].to_int();
			int to_ally_side = child["second"].to_int();
			int strategy_index = child["strategy"].to_int(-1);
			int target_side = child["target"].to_int(-1);
			
			resources::controller->do_ally(true, my_side, to_ally_side, emissary, target_side, strategy_index);

		} 
		else if (const config &child = cfg->child("final_battle")) 
		{
			resources::controller->final_battle(side_num, child["human"].to_int(-1), child["ai"].to_int(-1));

		}
		else if (const config &child = cfg->child("add_card")) 
		{
			hero_map& heros = *resources::heros;
			int side = child["side"].to_int();
			int number = child["number"].to_int();
			bool dialog = child["dialog"].to_int()? true: false;
			team& selected_team = (*resources::teams)[side - 1];
			selected_team.add_card(number, true);
			if (dialog) {
				utils::string_map symbols;
				std::stringstream strstr;
				symbols["first"] = help::tintegrate::generate_format(selected_team.holded_card(selected_team.holded_cards().size() - 1).name(), help::tintegrate::object_color);
				game_events::show_hero_message(&heros[hero::number_scout], NULL, vgettext("Get card: $first.", symbols), game_events::INCIDENT_CARD);
			}
		}
		else if (const config &child = cfg->child("erase_card")) 
		{
			int side = child["side"].to_int();
			int index = child["index"].to_int();
			bool dialog = child["dialog"].to_int()? true: false;
			
			map_location loc(child, resources::state_of_game);
			team& selected_team = (*resources::teams)[side - 1];
			const card& c = selected_team.holded_card(index);
			if (loc.valid()) {

					const std::vector<std::string> touched = utils::split(child["touched"]);
					std::vector<std::pair<int, unit*> > maps;
					for (std::vector<std::string>::const_iterator it = touched.begin(); it != touched.end(); ++ it) {
						int number = lexical_cast_default<int>(*it);
						if (c.target_hero()) {
							maps.push_back(std::make_pair(number, units.find_unit(heros[number])));
							if (!maps.back().second) {
								replay::process_error("illegal hero number in erase_card\n");
							}
							// ignore loc.x/y
							++ it;
							++ it;
						} else {
							++ it;
							int x = lexical_cast_default<int>(*it);
							++ it;
							int y = lexical_cast_default<int>(*it);
							unit_map::const_iterator it_u = units.find(map_location(x, y));
							unit* u = &*it_u;
							if (number >= 0) {
								artifical* art = unit_2_artifical(u);
								std::vector<unit*>& resides = art->reside_troops();
								u = resides[number];
							}
							maps.push_back(std::make_pair(number, u));
						}
					}
					selected_team.consume_card(c, loc, maps, index, false);

			} else {
				selected_team.erase_card(index, true);
			}
			refresh_card_button(current_team, *resources::screen);
			if (dialog) {
				utils::string_map symbols;
				std::stringstream strstr;
				symbols["first"] = c.name();
				game_events::show_hero_message(&heros[hero::number_scout], NULL, vgettext("Lost card: $first.", symbols), game_events::INCIDENT_CARD);
			}
		} else if (const config &child = cfg->child("reform_captain")) {
			map_location loc(child, resources::state_of_game);
			int number = child["number"].to_int();
			bool join = child["join"].to_int()? true: false;

			reform_captain(units, *units.find(loc), heros[number], join, true);

		} else if (const config &child = cfg->child("active_tactic")) {
			bool add = child["add"].to_int()? true: false;
			unit& u = *units.find_unit(heros[child["unit"].to_int()]);
			hero& h = heros[child["hero"].to_int()];

			if (add) {
				do_add_active_tactic(u, h, false);
			} else {
				do_remove_active_tactic(u, false);
			}

		} else {
			if (!cfg->child("checksum")) {
				replay::process_error("unrecognized action:\n" + cfg->debug());
			}
		}

		//Check if we should refresh the shroud, and redraw the minimap/map tiles.
		//This is needed for shared vision to work properly.
		if (fix_shroud && clear_shroud(side_num) && !recorder.is_skipping()) {
			resources::screen->recalculate_minimap();
			resources::screen->invalidate_game_status();
			resources::screen->invalidate_all();
			resources::screen->draw();
		}

		if (const config &child = cfg->child("verify")) {
			verify(*resources::units, child);
		}
	}

	return false; /* Never attained, but silent a gcc warning. --Zas */
}

replay_network_sender::replay_network_sender(replay& obj) : obj_(obj), upto_(obj_.ncommands())
{
}

replay_network_sender::~replay_network_sender()
{
	commit_and_sync();
}

void replay_network_sender::sync_non_undoable()
{
	if (network::nconnections() > 0) {
		int cmd_end = obj_.ncommands();
		if (upto_ >= cmd_end - 1) {
			return;
		}
		config cfg;
		const config& data = cfg.add_child("turn", obj_.get_data_range(upto_, cmd_end - 1));
		if (data.empty() == false) {
			network::send_data(cfg, 0);
			// upto_ += data.get_children("command").size();
		}

		upto_ = cmd_end - 1;
	}

/*
	if (network::nconnections() > 0) {
		config cfg;
		const config& data = cfg.add_child("turn",obj_.get_data_range(upto_,obj_.ncommands(), replay::NON_UNDO_DATA));
		if(data.empty() == false) {
			network::send_data(cfg, 0, true);
		}
	}
*/
}

void replay_network_sender::commit_and_sync()
{
	if (network::nconnections() > 0) {
		config cfg;
		const config& data = cfg.add_child("turn", obj_.get_data_range(upto_,obj_.ncommands()));
		if (data.empty() == false) {
			network::send_data(cfg, 0);
		}
		upto_ = obj_.ncommands();
	}
}

config mp_sync::get_user_choice(const std::string &name, const user_choice &uch,
	int side, bool force_sp)
{
	if (force_sp && network::nconnections() != 0 &&
	    resources::state_of_game->phase() != game_state::PLAY)
	{
		/* We are in a multiplayer game, during an early event which
		   prevents synchronization, and the WML is not interested
		   in a random result. We cannot silently ignore the issue,
		   since it would lead to a broken replay. To be sure that
		   the WML does not catch the error and keep the game going,
		   we use a sticky exception to forcefully quit. */
		ERR_REPLAY << "MP synchronization does not work during prestart and start events.";
		throw end_level_exception(QUIT);
	}
	if (resources::state_of_game->phase() == game_state::PLAY || force_sp)
	{
		/* We have to communicate with the player and store the
		   choices in the replay. So a decision will be made on
		   one host and shared amongst all of them. */

		/* process the side parameter and ensure it is within boundaries */
		if (unsigned(side - 1) >= resources::teams->size())
			side = resources::controller->current_side();

		int active_side = side;
		if ((*resources::teams)[active_side - 1].is_local() &&
		    get_replay_source().at_end())
		{
			/* The decision is ours, and it will be inserted
			   into the replay. */
			DBG_REPLAY << "MP synchronization: local choice\n";
			config cfg = uch.query_user();
			recorder.user_input(name, cfg);
			return cfg;

		} else {
			/* The decision has already been made, and must
			   be extracted from the replay. */
			DBG_REPLAY << "MP synchronization: remote choice\n";
			do_replay_handle(active_side, name);
			const config *action = get_replay_source().get_next_action();
			if (!action || !*(action = &action->child(name))) {
				replay::process_error("[" + name + "] expected but none found\n");
				return config();
			}
			return *action;
		}
	}
	else
	{
		/* Neither the user nor a replay can be consulted, so a
		   decision will be made at all hosts simultaneously.
		   The result is not stored in the replay, since the
		   other clients have already taken the same decision. */
		DBG_REPLAY << "MP synchronization: synchronized choice\n";
		return uch.random_choice(resources::state_of_game->rng());
	}
}