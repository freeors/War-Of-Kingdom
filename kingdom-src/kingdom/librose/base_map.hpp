/* $Id: base_map.hpp 41193 2010-02-14 00:45:51Z jhinrichs $ */
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

/** @file base_map.hpp */

#ifndef LIBROSE_BASE_MAP_H_INCLUDED
#define LIBROSE_BASE_MAP_H_INCLUDED

#include "base_unit.hpp"
#include "map_location.hpp"
#include "terrain_translation.hpp"

#include <cassert>

class gamemap;
class display;
class controller_base;
struct rect_of_hexes;

// base_map is used primarily as a map<location, unit>, but we need to be able to move a unit
// without copying it as the ai moves units a lot.
// Also, we want iterators to remain valid as much as reasonable as the underlying map is changed
// and we want a way to test an iterator for validity.

class base_map
{
public:
	class tconsistent_lock
	{
	public:
		tconsistent_lock(base_map& units)
			: units_(units)
			, original_(units.consistent_)
		{
			units_.consistent_ = true;
		}
		~tconsistent_lock()
		{
			units_.consistent_ = original_;
		}

	private:
		base_map& units_;
		bool original_;
	};

	class tplace_unsort_lock
	{
	public:
		tplace_unsort_lock(base_map& units)
			: units_(units)
			, original_(units.place_unsort_)
		{
			units_.place_unsort_ = true;
		}
		~tplace_unsort_lock()
		{
			units_.place_unsort_ = original_;
		}

	private:
		base_map& units_;
		bool original_;
	};

	enum {BASE = 1, OVERLAY};

	base_map(controller_base& controller, const gamemap& gmap, bool consistent);
	base_map& operator=(const base_map &that);
	virtual ~base_map();

// ~~~ Begin iterator code ~~~
	template <typename X, typename Y>
	struct convertible;

	template <typename iter_types>
	struct iterator_base {
		typedef typename iter_types::map_type map_type;
		typedef typename iter_types::value_type value_type;
		typedef typename iter_types::reference_type reference_type;
		typedef typename iter_types::pointer_type pointer_type;
		typedef base_unit* pointer;
		typedef base_unit& reference;

		iterator_base() : 
			map_(NULL), 
			i_(0), 
			iter_valid_(false), 
			ptr_(NULL)
		{}

		iterator_base(int i, map_type* m) : 
			map_(m), 
			i_(i),
			iter_valid_(true)
		{
			if (i != m->map_vsize_) {
				ptr_ = m->map_[i];
			} else  {
				ptr_ = NULL;
			}
		}

		template <typename that_types>
		iterator_base(const iterator_base<that_types>& that) :
			map_(that.map_),
			ptr_(that.ptr_),
			i_(that.i_),
			iter_valid_(that.iter_valid_)
		{}

		iterator_base(pointer_type ptr, map_type* m) : 
			ptr_(ptr), 
			map_(m), 
			i_(0),
			iter_valid_(false)
		{}

		pointer operator->() const { return ptr_; }
		reference operator*() const { return *ptr_; }

		iterator_base& operator++();
		iterator_base operator++(int);

		iterator_base& operator--();
		iterator_base operator--(int);

		// bool valid() const { return ptr_ && map_->valid(ptr_->first); }
		bool valid() const { return ptr_? true: false; }

		bool operator==(const iterator_base& rhs) const { return ptr_ == rhs.ptr_; }
		// bool operator!=(const iterator_base& rhs) const { return !operator==(rhs); }
		bool operator!=(const iterator_base& rhs) const { return ptr_ != rhs.ptr_; }

		map_type* get_map() const { return map_; }

		template <typename X> friend struct iterator_base;
		// friend class map_type;

	private:
		map_type* map_;

		int i_;
		base_unit* ptr_;
		bool iter_valid_;
	};

	struct standard_iter_types {
		typedef base_map map_type;
		typedef base_unit value_type;
		typedef value_type* pointer_type;
		typedef value_type& reference_type;
	};

	struct const_iter_types {
		typedef const base_map map_type;
		typedef const base_unit value_type;
		typedef value_type* pointer_type;
		typedef value_type& reference_type;
	};


// ~~~ End iterator code ~~~



	/**
	 * unit_iterators iterate over all units in the base_map. A unit_iterator is invalidated if
	 * base_map::erase, base_map::extract, or base_map::replace are called with the location of the
	 * unit that the unit_iterator points to. It will become valid again if base_map::add is called with
	 * a pair containing the unit that the iterator points to. Basically, as long as the unit is on the
	 * gamemap somewhere, the iterator will be valid.
	 * provided as a convenience as base_map used to be an std::map 
	 */
	typedef iterator_base<standard_iter_types> iterator;
	typedef iterator_base<const_iter_types> const_iterator;

	virtual void create_coor_map(int w, int h);
	virtual void clear();

	controller_base& get_controller() { return controller_; }
	const controller_base& get_controller() const { return controller_; }

	bool consistent() const { return consistent_;}
	void set_consistent(bool val) { consistent_ = val; }
	/**
	 * Return iterators are implicitly converted to other types as needed.
	 */
	iterator begin();
	const_iterator begin() const;

	// iterator end() { return iterator(map_.end(), this); }
	// const_iterator end() const { return const_iterator(map_.end(), this); }

	iterator end() { return iterator(map_vsize_, this); }
	const_iterator end() const { return const_iterator(map_vsize_, this); }

	int size() const { return map_vsize_; }

	virtual void add(const map_location& loc, const base_unit* u) = 0;

	/**
	 * Adds the pair location/unit to the map.
	 * @pre The location is empty.
	 * @pre The unit::underlying_id should not be used by the map already.
	 * @note The map takes ownership of the pointed object.
	 * @note This function should be used in conjunction with #extract only.
	 */
	virtual void insert(const map_location loc, base_unit* u);
	virtual void sort_map(const base_unit& u);

	virtual void insert2(const display& disp, base_unit* u);
	base_unit* unit_clicked_on(const int xclick, const int yclick, const map_location& mloc) const;
	map_location conflict_calculate_loc(const base_unit& u);

	/**
	 * Moves a unit from location @a src to location @a dst.
	 */
	virtual void move(const map_location src, const map_location &dst);

	/** Like add, but loc must be occupied (implicitly erased). */
	virtual void replace(const map_location &l, const base_unit* u);

	/** Erases the pair<location, unit> of this location. */
	// template <typename T>
	// void erase(const T& iter);
	virtual bool erase(const map_location& loc, bool overlay = true);
	virtual bool erase2(base_unit* u, bool delete_unit = true);

	/** Extract (like erase, but don't delete). */
	virtual base_unit* extract(const map_location& loc);
	virtual void place(const map_location loc, base_unit* u);

	bool valid2(const map_location& loc, bool overlay) const;

	virtual size_t units_from_rect(base_unit** draw_area_unit, const rect_of_hexes& draw_area_rect);

	const map_location& center_loc(const map_location& loc) const;

	base_unit* find_base_unit(const map_location& loc) const;
	base_unit* find_base_unit(const map_location& loc, bool overlay) const;
	base_unit* find_base_unit(int i) const { return map_[i]; }

	void verify_map_index() const;

	virtual bool terrain_matches(const map_location& loc, const t_translation::t_match& terrain_types_match) const { return false; }
	virtual void build_terrains(std::map<t_translation::t_terrain, std::vector<map_location> >& terrain_by_type) {}

private:
	void expand_coor_map(int w);

protected:
	const gamemap& gmap_;

	bool consistent_;
	bool place_unsort_;
	/**
	 * map_location -> base_unit
	 */
	base_unit** map_;
	int map_vsize_;

	int w_, h_;

	struct loc_cookie {
		base_unit* base;
		base_unit* overlay;
	};
	loc_cookie* coor_map_;

private:
	controller_base& controller_;
};

// define allowed conversions.
template <>
struct base_map::convertible<base_map::standard_iter_types, base_map::const_iter_types> { };

template <typename T>
struct base_map::convertible<T, T> { };

template <typename iter_types>
base_map::iterator_base<iter_types>& base_map::iterator_base<iter_types>::operator++() 
{
	if (!iter_valid_) {
		for (int i = 0; i < map_->map_vsize_; i ++) {
			if (map_->map_[i] == ptr_) {
				i_ = i;
				break;
			}
		}
		iter_valid_ = true;
	}
	++ i_;
	if (i_ != map_->map_vsize_) {
		ptr_ = map_->map_[i_];
	} else {
		ptr_ = NULL;
	}

	return *this;
}

template <typename iter_types>
base_map::iterator_base<iter_types> base_map::iterator_base<iter_types>::operator++(int) 
{
	base_map::iterator_base<iter_types> temp(*this);
	operator++();
	return temp;
}

template <typename iter_types>
base_map::iterator_base<iter_types>& base_map::iterator_base<iter_types>::operator--()
{
	if (!iter_valid_) {
		for (int i = 0; i < map_->map_vsize_; i ++) {
			if (map_->map_[i] == ptr_) {
				i_ = i;
				break;
			}
		}
		iter_valid_ = true;
	}
	-- i_;
	ptr_ = map_->map_[i_];

	return *this;
}

template <typename iter_types>
base_map::iterator_base<iter_types> base_map::iterator_base<iter_types>::operator--(int)
{
	base_map::iterator_base<iter_types> temp(*this);
	operator--();
	return temp;
}

#endif	// BASE_MAP_H_INCLUDED
