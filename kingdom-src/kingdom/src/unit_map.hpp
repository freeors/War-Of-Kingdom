/* $Id: unit_map.hpp 41193 2010-02-14 00:45:51Z jhinrichs $ */
/*
   Copyright (C) 2006 - 2010 by Rusty Russell <rusty@rustcorp.com.au>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/** @file unit_map.hpp */

#ifndef UNIT_MAP_H_INCLUDED
#define UNIT_MAP_H_INCLUDED

#include "base_map.hpp"

class unit;
class hero;
class team;
class artifical;
class unit_map;
class game_state;
class game_display;
class strategy;

#define CITYS_INVALID_NUMBER	0xffffffff

class city_map
{
public:
	city_map();
	~city_map();

// ~~~ Begin iterator code ~~~
	template <typename X, typename Y>
	struct convertible;

	template <typename iter_types>
	struct iterator_base {
		typedef typename iter_types::map_type map_type;
		typedef typename iter_types::value_type value_type;
		typedef typename iter_types::reference_type reference_type;
		typedef typename iter_types::pointer_type pointer_type;

		iterator_base() : 
			map_(NULL), 
			i_(CITYS_INVALID_NUMBER)
		{}

		iterator_base(size_t i, map_type* m) : 
			map_(m), 
			i_(i)
		{}

		template <typename that_types>
		iterator_base(const iterator_base<that_types>& that) :
			map_(that.map_),
			i_(that.i_)
		{}

		pointer_type operator->() const { return map_->map_[i_]; }
		reference_type operator*() const { return *map_->map_[i_]; }

		iterator_base& operator++();
		iterator_base operator++(int);

		iterator_base& operator--();
		iterator_base operator--(int);

		bool valid() const { return (i_ != CITYS_INVALID_NUMBER)? true: false; }

		bool operator==(const iterator_base& rhs) const { return i_ == rhs.i_; }
		bool operator!=(const iterator_base& rhs) const { return i_ != rhs.i_; }

		map_type* get_map() const { return map_; }

		template <typename X> friend struct iterator_base;

	private:
		map_type* map_;
		size_t i_;
	};

	struct standard_iter_types {
		typedef city_map map_type;
		typedef artifical value_type;
		typedef value_type* pointer_type;
		typedef value_type& reference_type;
	};

	struct const_iter_types {
		typedef const city_map map_type;
		typedef const artifical value_type;
		typedef value_type* pointer_type;
		typedef value_type& reference_type;
	};


// ~~~ End iterator code ~~~
	typedef iterator_base<standard_iter_types> iterator;
	typedef iterator_base<const_iter_types> const_iterator;

	void add(artifical* city);

	void erase(const artifical* city);

	iterator begin();
	const_iterator begin() const;

	// extern iterator hero_map_iter_invalid;
	// extern const_iterator hero_map_const_iter_invalid;
	// iterator end() { return iterator(map_vsize_, this); }
	// const_iterator end() const { return const_iterator(map_vsize_, this); }
	iterator end();
	const_iterator end() const;

	void realloc_map(const size_t size);

	void clear_map();

	artifical* city_from_cityno(int cityno);

private:
	friend class unit_map;

	artifical** map_;
	size_t map_vsize_;
};

// define allowed conversions.
template <>
struct city_map::convertible<city_map::standard_iter_types, city_map::const_iter_types> {};

template <typename T>
struct city_map::convertible<T, T> { };

template <typename iter_types>
city_map::iterator_base<iter_types>& city_map::iterator_base<iter_types>::operator++() 
{
	while (i_ < (map_->map_vsize_ - 1)) {
		++ i_;
		if (map_->map_[i_]) {
			return *this;
		}
	}
	i_ = CITYS_INVALID_NUMBER;

	return *this;
}

template <typename iter_types>
city_map::iterator_base<iter_types> city_map::iterator_base<iter_types>::operator++(int) 
{
	city_map::iterator_base<iter_types> temp(*this);
	operator++();
	return temp;
}

template <typename iter_types>
city_map::iterator_base<iter_types>& city_map::iterator_base<iter_types>::operator--() 
{
	while (i_ != 0) {
		-- i_;
		if (map_->map_[i_]) {
			return *this;
		}
	}
	i_ = CITYS_INVALID_NUMBER;

	return *this;
}

template <typename iter_types>
city_map::iterator_base<iter_types> city_map::iterator_base<iter_types>::operator--(int) 
{
	city_map::iterator_base<iter_types> temp(*this);
	operator--();
	return temp;
}

struct tmess_data {
	struct tadjacent_data {
		tadjacent_data(size_t _friends = 0, size_t _unhides = 0, size_t _unnormals = 0, size_t _healables = 0, size_t _unprovoks = 0, size_t _enemies = 0) 
			: friends(_friends)
			, unhides(_unhides)
			, unnormals(_unnormals)
			, healables(_healables)
			, unprovoks(_unprovoks)
			, enemies(_enemies)
		{}

		void clear() 
		{
			friends = 0;
			unhides = 0;
			unnormals = 0;
			unprovoks = 0;
			enemies = 0;
		}

		size_t friends;
		size_t unhides;
		size_t unnormals;
		size_t healables;
		size_t unprovoks;
		size_t enemies;
	};
	tmess_data() 
		: cumulate_x(0)
		, cumulate_y(0)
		, selfs()
		, allys(0)
		, friend_arts(0)
		, enemies(0)
		, enemy_arts(0)
	{}

	void combine(const tmess_data& that);

	int cumulate_x;
	int cumulate_y;
	std::map<unit*, tadjacent_data> selfs;
	size_t allys;
	size_t friend_arts;
	size_t enemies;
	size_t enemy_arts;
};

class mr_data
{
public:
	static int min_interior_requirement, max_interior_requirement;
	static int min_front_requirement, mid_front_requirement, max_front_requirement, min_alert_fronts, max_fronts;
	static int min_interior_lack_, aggress_requirement;
	static int ally_forbid_turns;
	static int minimum_enemy_in_front;
	static int consider_rect_radius;

	mr_data(gamemap& map, SDL_Rect& city_rect);

	artifical* calculate_center_city(const map_location& center);
	void calculate_mass(const unit_map& units, const team& current_team);

	enum {TARGET_AGGRESS, TARGET_GUARD, TARGET_INTERIOR, TARGET_CHAOTIC};

	SDL_Rect consider_rect;

	int target;
	size_t own_heros; // own hero count in this mr.
	std::vector<artifical*> own_front_cities; // front cities in this mr. (sort by important)
	std::vector<artifical*> own_back_cities; // back cities in this mr. (not sort)
	struct enemy_data {
		enemy_data()
			: troops()
			, cities()
		{}
		std::vector<unit*> troops;
		std::vector<artifical*> cities;
	};
	// pointer of city is random, 
	// this is std::map, map is sorted, use unvarious cityno, although cityno is complex.
	std::map<int, enemy_data> own_cities; // front/back cities in this mr. (not sort)
	std::vector<unit*> enemy_troops;
	std::vector<artifical*> enemy_cities; // enemy cities. (sort by distance starting at own_font_cities[0])
	std::set<int> encountered_sides;

	artifical* center_city;
	strategy* aggressing_strategy;

	std::vector<tmess_data> messes;
};

class ai_plan
{
public:
	ai_plan();
	void reset();

	// SDL_Rect city_rect_;
	std::vector<mr_data> mrs_;
	// map_location front_;
};

// unit_map is used primarily as a map<location, unit>, but we need to be able to move a unit
// without copying it as the ai moves units a lot.
// Also, we want iterators to remain valid as much as reasonable as the underlying map is changed
// and we want a way to test an iterator for validity.

class unit_map: public base_map
{
public:
	static surface desc_bg_[10];
	static surface desc_hot;
	static surface enemy_orb_, ally_orb_, moved_orb_, unmoved_orb_, partmoved_orb_, automatic_orb_, self_orb_;
	static surface normal_food, lack_food, robber;
	static std::string bar_vtl_png, bar_vtl_hot_png;

	static unit* scout_unit_;
	static std::map<std::pair<int, int>, size_t> inter_city_move_cost_;

	static int main_ticks;
	static int top_side;

	static void set_zoom();

	static std::map<const map_location, int> economy_areas_;

	unit_map(const gamemap& gmap);
	unit_map& operator=(const unit_map &that);

	~unit_map();

	void create_coor_map(int w, int h);

	city_map& get_city_map() { return citys_; }

	/**
	 * Lookups can be done by map_location, by unit::underlying_id().
	 * Returned iterators can be implicitly converted to the other types.
	 */
	iterator find(const map_location& loc, bool overlay = true);

	const_iterator find(const map_location &loc, bool overlay = true) const
	{ return const_cast<unit_map *>(this)->find(loc, overlay); }

	iterator find_leader(int side);
	const_iterator find_leader(int side) const
	{ return const_cast<unit_map *>(this)->find_leader(side); }

	void multi_resort_map(game_display* disp, const std::vector<unit*>& v, bool full);

	/**
	 * Adds a copy of unit @a u at location @a l of the map.
	 */
	// void add(const map_location &l, const unit& u);
	void add(const map_location& l, const base_unit* u);

	/**
	 * Adds the pair location/unit to the map.
	 * @pre The location is empty.
	 * @pre The unit::underlying_id should not be used by the map already.
	 * @note The map takes ownership of the pointed object.
	 * @note This function should be used in conjunction with #extract only.
	 */
	void insert(const map_location loc, base_unit* u);

	/**
	 * Moves a unit from location @a src to location @a dst.
	 */
	void move(const map_location src, const map_location& dst);

	/** Erases the pair<location, unit> of this location. */
	bool erase2(base_unit* u, bool delete_unit = true);

	/** Extract (like erase, but don't delete). */

	void set_expediting(artifical* city = NULL, bool troop = true, int index = 0);
	bool last_expedite_troop(void) const;
	int last_expedite_index(void) const;
	bool expediting(void) const;
	artifical* expediting_city(void) const;

	artifical* city_from_cityno(int cityno) const;
	artifical* city_from_loc(const map_location& loc) const;
	artifical* city_from_seed(size_t seed);
	const artifical* city_from_seed(size_t seed) const;

	bool side_survived(int side, int* residuals = NULL) const;

	void ally_terminate_adjust(team& adjusting_team, const SDL_Rect& rect);

	void calculate_mrs_data(game_state& state, std::vector<mr_data>& mrs, int side, bool action = true);
	void calculate_mr_rects_from_city_rect(std::vector<team>& teams, gamemap& map, std::vector<mr_data>& mrs, int side);
	void ai_capture_aggressed(artifical& aggressed, int side, bool to_recorder);

	void extract_heros_number();
	void recalculate_heros_pointer();

	unit* find_unit(const hero& h) const;
	unit* find_unit(const map_location& loc) const;
	unit* find_unit(const map_location& loc, bool overlay) const;

	unit& current_unit();
	void do_escape_ticks_uh(const std::vector<team>& teams, game_display& disp, int escape, bool first_zero);
	void do_escape_ticks_bh(const std::vector<team>& teams, game_display& disp, int player_number);

	// bool compare_front_cities(const mr_data& mr, artifical& a, artifical& b);
	bool compare_enemy_cities(const mr_data& mr, artifical& a, artifical& b);

private:
	// used to spread all city
	city_map citys_;

	bool expediting_;
	artifical* expediting_city_;
	bool expediting_troop_;
	int expediting_index_;
};

#endif	// UNIT_MAP_H_INCLUDED
