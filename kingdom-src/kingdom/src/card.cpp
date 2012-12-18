#include "global.hpp"
#include "card.hpp"
#include "gettext.hpp"
#include "foreach.hpp"
#include "posix.h"
#include "serialization/string_utils.hpp"
#include "map_location.hpp"
#include "unit_display.hpp"

card card::null_card;

card_map::iterator token_map_iter_invalid = card_map::iterator(CARDS_INVALID_NUMBER, NULL);
card_map::const_iterator token_map_const_iter_invalid = card_map::const_iterator(CARDS_INVALID_NUMBER, NULL);

std::string card::image_file_root_ = "";

static config invalid_cfg;

card::card()
	: cfg_()
	, condition_cfg_(invalid_cfg)
	, range_cfg_(invalid_cfg)
	, action_cfg_(invalid_cfg)
	, number_(CARDS_INVALID_NUMBER)
	, target_hero_(false)
	, multitudinous_(false)
	, points_(-1)
	, range_(NONE)
{
}

card::card(const config& card_cfg)
	: cfg_(card_cfg)
	, condition_cfg_(cfg_.child("condition"))
	, range_cfg_(cfg_.child("range"))
	, action_cfg_(cfg_.child("action"))
	, number_(CARDS_INVALID_NUMBER)
	, target_hero_(card_cfg["hero"].to_bool())
	, multitudinous_(card_cfg["multitudinous"].to_bool())
	, points_(-1)
	, range_(NONE)
{
}

card::card(const card& that)
	: cfg_(that.cfg_)
	, condition_cfg_(that.condition_cfg_)
	, range_cfg_(that.range_cfg_)
	, action_cfg_(that.action_cfg_)
	, number_(that.number_)
	, target_hero_(that.target_hero_)
	, multitudinous_(that.multitudinous_)
	, points_(that.points_)
	, range_(that.range_)
{
}

std::string& card::name()
{
	if (name_str_.empty()) {
		name_str_ = cfg_["name"].str();
	}
	return name_str_;
}

std::string& card::desc()
{
	if (desc_str_.empty()) {
		desc_str_ = cfg_["description"].str();
	}
	return desc_str_;
}

std::string& card::image()
{
	if (image_str_.empty()) {
		uint32_t fsize_low, fsize_high;
		image_str_ = image_file_root_ + "/card/" + cfg_["image"];
		posix_fsize_byname(image_str_.c_str(), fsize_low, fsize_high);
		if (!fsize_low && !fsize_high) {
			image_str_ = "card/default.png";
		} else {
			image_str_ = std::string("card/") + cfg_["image"];
		}
	}
	return image_str_;
}

int card::points()
{
	if (points_ == -1) {
		points_ = cfg_["points"].to_int();
	}
	return points_;
}

int card::range()
{
	if (range_ == NONE && range_cfg_) {
		int ranges = 0;
		if (range_cfg_["self"].to_bool()) {
			ranges ++;
		}
		std::vector<std::string> adjacent = utils::split(range_cfg_["adjacent"]);
		for (std::vector<std::string>::const_iterator dir = adjacent.begin(); dir != adjacent.end(); ++ dir) {
			map_location::DIRECTION dir_i = map_location::parse_direction(*dir);
			if (dir_i == map_location::NDIRECTIONS) {
				continue;
			}
			ranges ++;
		}
		if (ranges > 1) {
			range_ = MULTI;
		} else if (ranges == 1) {
			range_ = SINGLE;
		} else {
			range_ = NONE;
		}
	}
	return range_;
}

//
// card_map
//
card_map::card_map(const std::string& path) :
	animations_(),
	map_size_(0),
	map_(NULL),
	map_vsize_(0)
{
	card::image_file_root_ = path + "/data/core/images";
}

card_map::~card_map()
{
	clear_map();
}

card_map& card_map::operator=(const card_map &that)
{
	realloc_hero_map(that.map_size_);

	for (size_t i = 0; i != that.map_vsize_; ++i) {
		add(*(that.map_[i]));
	}
	return *this;
}

void card_map::set_path(const std::string& path)
{
	card::image_file_root_ = path + "/data/core/images";
}

void card_map::realloc_hero_map(const size_t size)
{
	if (map_) {
		clear_map();
	}
	map_size_ = size;
	map_ = (card**)malloc(size * sizeof(card*));
	map_vsize_ = 0;
}

void card_map::clear_map()
{
	for (size_t i = 0; i != map_vsize_; ++i) {
		delete(map_[i]);
	}
	free(map_);
	map_ = NULL;
	map_vsize_ = 0;
}

card_map::iterator card_map::begin() 
{
	size_t i = 0;
	return iterator(i, this);
}

card_map::const_iterator card_map::begin() const 
{
	size_t i = 0;
	return const_iterator(i, this);
}

card_map::iterator card_map::end()
{ 
	return token_map_iter_invalid; 
}

card_map::const_iterator card_map::end() const 
{ 
	return token_map_const_iter_invalid; 
}

void card_map::add(const card& h)
{
	card* p = new card(h);
	p->number_ = map_vsize_;
	map_[map_vsize_ ++] = p;
}

card& card_map::operator[](size_t num)
{
	if (num >= map_vsize_) {
		return card::null_card;
	}
	return *map_[num];
}

const card& card_map::operator[](size_t num) const
{
	if (num >= map_vsize_) {
		return card::null_card;
	}
	return *map_[num];
}

void card_map::erase(size_t number)
{
	size_t idx;
	if (number >= map_vsize_) {
		return;
	}
	delete map_[number];
	if (number != (map_vsize_ - 1)) {
		memcpy(&(map_[number]), &(map_[number + 1]), (map_vsize_ - number - 1) * sizeof(card*));
	}
	map_vsize_ --;
	// update number_ field that has be moved
	for (idx = number; idx < map_vsize_; idx ++) {
		map_[idx]->number_ --;
	}
}

void card_map::map_from_cfg(const config& cfg)
{
	// realloc map memory in card_map
	realloc_hero_map(CARDS_MAX_CARDS);

	foreach (const config &tf, cfg.child_range("card")) {
		if (tf["name"].empty()) {
			throw config::error("card config error, no id attribute");
		}
		card* t = new card(tf);
		t->number_ = map_vsize_;
		map_[map_vsize_ ++] = t;
	}

	//
	// load animations
	//
	foreach (const config &anim, cfg.child_range("card_anim")) {
		animations_.insert(std::make_pair<int, unit_animation>(ANIM_START, unit_animation(anim)));
	}

	//
	// load global animations
	//
	unit_display::load_global_animations(cfg);
}

unit_animation* card_map::animation(int type)
{
	if (type < ANIM_MIN || type > ANIM_MAX) {
		return NULL;
	}
	std::map<int, unit_animation>::iterator i = animations_.find(type);
	if (i != animations_.end()) {
		return &i->second;
	}
	return NULL;
}