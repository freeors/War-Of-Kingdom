/* $Id: random.hpp 41185 2010-02-13 13:40:11Z ilor $ */
/*
   Copyright (C) 2003 by David White <dave@whitevine.net>
   Copyright (C) 2005 - 2010 by Yann Dirson <ydirson@altern.org>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/** @file random.hpp */

#ifndef RANDOM_HPP_INCLUDED
#define RANDOM_HPP_INCLUDED

class config;

#define POOL_ALLOC_DATA_SIZE	1048576	// 1M
#define POOL_ALLOC_POS_SIZE		131072 // 128K, One allocate may save 1024x128 packet pos.
#define POOL_ALLOC_CACHE_SIZE	262144 // 256K
#define POOL_COMMAND_RESERVE_BYTES	100	// Assume, one command reserve 100 bytes.
class command_pool
{
public:
	enum TYPE {UNGZIP = 1, START, EMPLOY, INIT_SIDE, PREFIX_UNIT, POST_UNIT, DO_COMMONER, END_TURN, INIT_AI, FRESH_HEROS, RECRUIT, EXPEDITE,
		BOMB, DISBAND, BUILD, CAST_TACTIC, COUNTDOWN_UPDATE, MOVEMENT, ATTACK, SET_STATES, CLEAR_FORMATIONED, SET_TASK, CHOOSE, LABLE,
		RENAME, SPEAK, MOVE_HEROS, BELONG_TO, EVENT, CHECKSUM_CHECK, INCHING_BLOCK, CLEAR_LABELS, DIPLOMATISM,
		FINAL_BATTLE, INPUT, REFORM_CAPTAIN, ACTIVE_TACTIC, ADD_CARD, ERASE_CARD, ARMORY, INTERIOR, ASSEMBLE_TREASURE, 
		RPG_EXCHANGE, ING_TECHNOLOGY, SCENARIO_ENV, FORMATION_ATTACK, PURCHASE, APPOINT_NOBLE};
	struct command {
		TYPE type;
		unsigned int flags;
	};

	struct gzip_segment_t {
		int len;
		int min;
		int max;
	};

	command_pool();
	explicit command_pool(const command_pool& that);
	~command_pool();
	command_pool& operator=(const command_pool& that);

	void read(unsigned char* mem);
	void write(unsigned char* mem);

	void clear();

	unsigned char* pool_data() const { return pool_data_; }
	int pool_data_size() const { return pool_data_size_; }
	int pool_data_vsize() const { return pool_data_vsize_; }
	int pool_data_gzip_size() const { return pool_data_gzip_size_; }
	unsigned int* pool_pos() const { return pool_pos_; }
	int pool_pos_size() const { return pool_pos_size_; }
	int pool_pos_vsize() const { return pool_pos_vsize_; }

	command* command_addr(int pool_pos_index, bool queue = true);
	command* command_addr2(int pool_pos_index, gzip_segment_t& seg, int& pool_data_gzip_size, unsigned char* cache);
	command* add_command();
	int gzip_codec(unsigned char* data, int data_len, bool encode, unsigned char* to = NULL);

	void set_use_gzip(bool val);

	void load_for_queue(int pool_pos_index, gzip_segment_t& seg, int& pool_data_gzip_size, unsigned char* dest);

protected:
	bool use_gzip_;
	
	unsigned char* pool_data_;
	unsigned int* pool_pos_;
	int pool_data_size_;
	int pool_data_vsize_;
	int pool_data_gzip_size_;
	int pool_pos_size_;
	int pool_pos_vsize_;

	gzip_segment_t current_gzip_seg_;

	unsigned char* cache_;
};

int get_random();
int get_random_nocheck();
const config* get_random_results();
void set_random_results(const config& cfg);

namespace rand_rng
{

typedef unsigned int seed_t;

class rng;

struct set_random_generator {
	set_random_generator(rng* r);
	~set_random_generator();
private:
	rng* old_;
};

} // ends rand_rng namespace

#endif
