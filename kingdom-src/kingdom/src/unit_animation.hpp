/* $Id: unit_animation.hpp 47611 2010-11-21 13:56:47Z mordante $ */
/*
   Copyright (C) 2006 - 2010 by Jeremy Rosen <jeremy.rosen@enst-bretagne.fr>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
   */
#ifndef UNIT_ANIMATION_H_INCLUDED
#define UNIT_ANIMATION_H_INCLUDED

#include "area_anim.hpp"

class attack_type;
class game_display;
class unit;
class artifical;

class unit_animation: public animation
{
	/** Shouldn't be used so only declared. */
	// unit_animation();
public:
	typedef enum { MATCH_FAIL=-10 , DEFAULT_ANIM=-9} variation_type;
	typedef enum { HIT, MISS, KILL, INVALID} hit_type;
	
	static const std::vector<std::string>& all_tag_names();
	static void fill_initial_animations(const std::string& default_image, std::vector<unit_animation> & animations, const config & cfg);
	static void add_anims( std::vector<unit_animation> & animations, const config & cfg);

	static std::map<int, unit_animation> share_anims;
	static void fill_share_anims();
	static const unit_animation* share_anim(int type);

	int matches(const game_display &disp,const map_location& loc,const map_location& second_loc,const unit* my_unit,const std::string & event="",const int value=0,hit_type hit=INVALID,const attack_type* attack=NULL, const std::string& second_weapon_type = null_str, int value2 =0) const;
	const std::string event0() const;

friend class unit;
friend class artifical;

	explicit unit_animation(const config &cfg, const std::string &frame_string = "");
	explicit unit_animation(const animation& anim);

private:
	explicit unit_animation(int start_time
			, const unit_frame &frame
			, const std::string& event = ""
			, const int variation=DEFAULT_ANIM
			, const frame_builder & builder = frame_builder());

	t_translation::t_list terrain_types_;
	std::vector<config> unit_filter_;
	std::vector<config> secondary_unit_filter_;
	std::vector<map_location::DIRECTION> directions_;
	std::set<std::string> secondary_weapon_type_;
	int feature_;
	int align_;
	int frequency_;

	int base_score_;
	std::vector<std::string> event_;
	std::vector<int> value_;
	std::vector<config> primary_attack_filter_;
	std::vector<config> secondary_attack_filter_;
	std::vector<hit_type> hits_;
	std::vector<int> value2_;
};

class unit_animator
{
public:
	unit_animator() :
		animated_units_(),
		start_time_(INT_MIN)
	{}


	void add_animation(unit* animated_unit
			, const unit_animation * animation
			, const map_location &src = map_location::null_location
			, bool with_bars = false
			, bool cycles = false
			, const std::string& text = ""
			, const Uint32 text_color = 0);
	void add_animation(unit* animated_unit
			, const std::string& event
			, const map_location &src = map_location::null_location
			, const map_location &dst = map_location::null_location
			, const int value = 0
			, bool with_bars = false
			, bool cycles = false
			, const std::string& text = ""
			, const Uint32 text_color = 0
			, const unit_animation::hit_type hit_type =
				unit_animation::INVALID
			, const attack_type* attack = NULL
			, const std::string& second_weapon = null_str
			, int value2 = 0);
	void replace_anim_if_invalid(unit* animated_unit
			, const std::string& event
			, const map_location &src = map_location::null_location
			, const map_location &dst = map_location::null_location
			, const int value = 0
			, bool with_bars = false
			, bool cycles = false
			, const std::string& text = ""
			, const Uint32 text_color = 0
			, const unit_animation::hit_type hit_type =
				unit_animation::INVALID
			, const attack_type* attack = NULL
			, const std::string& second_weapon = null_str
			, int value2 = 0);
	void start_animations();
	void pause_animation();
	void restart_animation();
	void clear(){start_time_ = INT_MIN ; animated_units_.clear();};
	void set_all_standing();


	bool would_end() const;
	int get_animation_time() const;
	int get_animation_time_potential() const;
	int get_end_time() const;
	void wait_for_end() const;
	void wait_until( int animation_time) const;

	struct anim_elem {

		anim_elem() :
			my_unit(0),
			animation(0),
			text(),
			text_color(0),
			src(),
			with_bars(false),
			cycles(false)
		{}

		unit *my_unit;
		const unit_animation * animation;
		std::string text;
		Uint32 text_color;
		map_location src;
		bool with_bars;
		bool cycles;
	};
	std::vector<anim_elem>& animated_units() { return animated_units_; }
	const std::vector<anim_elem>& animated_units() const { return animated_units_; }

private:
	std::vector<anim_elem> animated_units_;
	int start_time_;
};

#endif
