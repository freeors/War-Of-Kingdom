#ifndef __CARD_HPP_
#define __CARD_HPP_

#include <string>
#include <vector>
#include <map>
#include "config.hpp"
#include "unit_animation.hpp"

class card_map;

// At best 1024 card
#define CARDS_MAX_CARDS		1024

#define CARDS_INVALID_NUMBER	0xffff

class card
{
public:
	static card null_card;

	enum DIRECTION {NONE, SINGLE, MULTI};
	card();
	card(const config& card_cfg);
	card(const card& that);

	bool valid() const { return (number_ == CARDS_INVALID_NUMBER)? false: true; }

	void set_config(const config& card_cfg) { cfg_ = card_cfg; }
	const config& get_cfg() const { return cfg_; }
	config& get_cfg() { return cfg_; }
	config& get_condition() { return condition_cfg_; }
	config& get_range() { return range_cfg_; }
	config& get_action() { return action_cfg_; }
	
	std::string& name();
	std::string& desc();
	std::string& image();
	int points();
	int range();
	bool target_hero() const { return target_hero_; }

// attribute
public:
	size_t number_;

private:
	friend class card_map;
	static std::string image_file_root_;

	config cfg_;
	config& condition_cfg_;
	config& range_cfg_;
	config& action_cfg_;

	bool target_hero_;
	std::string name_str_;
	std::string desc_str_;
	std::string image_str_;
	int points_;
	int range_;
};

class card_map
{
public:
	card_map(const std::string& path = "");
	~card_map();

	card_map& operator=(const card_map& that);
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
			i_(0)
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

		bool valid() const { return (i_ != CARDS_INVALID_NUMBER)? true: false; }

		bool operator==(const iterator_base& rhs) const { return i_ == rhs.i_; }
		bool operator!=(const iterator_base& rhs) const { return i_ != rhs.i_; }

		map_type* get_map() const { return map_; }

		template <typename X> friend struct iterator_base;

	private:
		map_type* map_;
		size_t i_;
	};

	struct standard_iter_types {
		typedef card_map map_type;
		typedef card value_type;
		typedef value_type* pointer_type;
		typedef value_type& reference_type;
	};

	struct const_iter_types {
		typedef const card_map map_type;
		typedef const card value_type;
		typedef value_type* pointer_type;
		typedef value_type& reference_type;
	};


// ~~~ End iterator code ~~~
	typedef iterator_base<standard_iter_types> iterator;
	typedef iterator_base<const_iter_types> const_iterator;

	void set_path(const std::string& path);

	// load from topest config
	void map_from_cfg(const config& cfg);

	iterator find(const size_t num);
	const_iterator find(const size_t num) const { return const_cast<card_map *>(this)->find(num); }

	void add(const card& h);

	void erase(size_t number);

	iterator begin();
	const_iterator begin() const;

	// iterator end() { return iterator(map_vsize_, this); }
	// const_iterator end() const { return const_iterator(map_vsize_, this); }
	iterator end();
	const_iterator end() const;

	size_t size() const { return map_vsize_; }
	
	void clear_map();

	void realloc_hero_map(const size_t size);

	card& operator[](size_t num);
	const card& operator[](size_t num) const;

	enum ANIM_TYPE {ANIM_START = 0, ANIM_MIN = ANIM_START, ANIM_END, ANIM_MAX = ANIM_START};
	unit_animation* animation(int type);

private:
	std::map<int, unit_animation> animations_;

	size_t map_size_;

	card** map_;
	size_t map_vsize_;
};

// define allowed conversions.
template <>
struct card_map::convertible<card_map::standard_iter_types, card_map::const_iter_types> {};

template <typename T>
struct card_map::convertible<T, T> { };

template <typename iter_types>
card_map::iterator_base<iter_types>& card_map::iterator_base<iter_types>::operator++() 
{
	if (i_ < (map_->map_vsize_ - 1)) {
		++ i_;
	} else {
		i_ = CARDS_INVALID_NUMBER;
	}

	return *this;
}

template <typename iter_types>
card_map::iterator_base<iter_types> card_map::iterator_base<iter_types>::operator++(int) 
{
	card_map::iterator_base<iter_types> temp(*this);
	operator++();
	return temp;
}

template <typename iter_types>
card_map::iterator_base<iter_types>& card_map::iterator_base<iter_types>::operator--() 
{
	if (i_ != 0) {
		-- i_;
	}
	return *this;
}

template <typename iter_types>
card_map::iterator_base<iter_types> card_map::iterator_base<iter_types>::operator--(int) 
{
	card_map::iterator_base<iter_types> temp(*this);
	operator--();
	return temp;
}

#endif // __HERO_HPP_