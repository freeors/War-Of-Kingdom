#include "global.hpp"
#include "artifical.hpp"

#include "actions.hpp"
#include "pathutils.hpp"
#include "game_display.hpp"
#include "card.hpp"
#include "map.hpp"
#include "halo.hpp"
#include "game_preferences.hpp"
#include "resources.hpp"
#include "unit_display.hpp"
#include "play_controller.hpp"
#include "gettext.hpp"
#include "game_events.hpp"
#include "formula_string_utils.hpp"
#include "replay.hpp"
#include "wml_exception.hpp"
#include "dialogs.hpp"
#include "gui/dialogs/select_unit.hpp"
#include "integrate.hpp"

#include <boost/foreach.hpp>
#include <iomanip>

tartifical_::tartifical_()
	: police_speed_(100)
	, generate_speed_(100)
	, trade_speed_(100)
	, business_speed_(100)
	, business_income_ratio_(100)
	, technology_speed_(100)
	, technology_income_ratio_(100)
	, revenue_income_(100)
	, destruct_hp_(0)
	, heal_hp_(0)
{
}

void tartifical_::reset_decree_effect()
{
	police_speed_ = 100;
	generate_speed_ = 100;
	trade_speed_ = 100;
	business_speed_ = 100;
	business_income_ratio_ = 100;
	technology_speed_ = 100;
	technology_income_ratio_ = 100;
	revenue_income_ = 100;
	destruct_hp_ = 0;
	heal_hp_ = 0;
}

artifical::artifical(game_state& state, const config& cfg) :
	unit(*resources::units, *resources::heros, *resources::teams, state, cfg, true, true)
	, map_(*resources::game_map)
	, reside_troops_()
	, field_troops_()
	, reside_commoners_()
	, field_commoners_()
	, field_arts_()
	, fresh_heros_()
	, finish_heros_()
	, wander_heros_()
	, economy_area_()
	, district_()
	, district_locs_()
	, terrain_types_list_(t_translation::t_match(type()->match_).terrain)
	, alert_rect_()	
	, villages_()
	, roaded_cities_()
	, mayor_(&hero_invalid)
	, commercial_exploiture_(0)
	, technology_exploiture_(0)
	, gold_bonus_(0)
	, technology_bonus_(0)
	, max_commoners_(game_config::max_commoners)
	, research_turns_(0)
	, fronts_(0)
	, police_(game_config::max_police / 2)
	, soldiers_(0)
	, not_recruit_()
	, alias_()
	, can_recruit_map_()
	, decree_(NULL)
	, max_recruit_cost_(-1)
{
	max_level_ = level_;

	read(state, cfg);

	if (decree_) {
		apply_issur_decree(*decree_, false);
	}
	calculate_exploiture();
}

artifical::artifical(const uint8_t* mem) :
	unit(*resources::units, *resources::heros, *resources::teams, mem, true, true)
	, map_(*resources::game_map)
	, reside_troops_()
	, field_troops_()
	, reside_commoners_()
	, field_commoners_()
	, field_arts_()
	, fresh_heros_()
	, finish_heros_()
	, wander_heros_()
	, economy_area_()
	, district_()
	, district_locs_()
	, villages_()
	, roaded_cities_()
	, terrain_types_list_(t_translation::t_match(type()->match_).terrain)
	, alert_rect_()	
	, mayor_(&hero_invalid)
	, commercial_exploiture_(0)
	, technology_exploiture_(0)
	, gold_bonus_(0)
	, technology_bonus_(0)
	, max_commoners_(game_config::max_commoners)
	, research_turns_(0)
	, fronts_(0)
	, police_(game_config::max_police / 2)
	, soldiers_(0)
	, not_recruit_()
	, alias_()
	, can_recruit_map_()
	, decree_(NULL)
	, max_recruit_cost_(-1)
{
	max_level_ = level_;

	// 设置[city]级字段
	read(mem);

	if (decree_) {
		apply_issur_decree(*decree_, false);
	}
	calculate_exploiture();
}

artifical::artifical(const artifical& that) :
	unit(that)
	, tartifical_(that)
	, map_(*resources::game_map)
	// , reside_troops_(that.reside_troops_)
	, field_troops_(that.field_troops_)
	// , reside_commoners_(that.reside_commoners_)
	, field_commoners_(that.field_commoners_)
	, field_arts_(that.field_arts_)
	, fresh_heros_(that.fresh_heros_)
	, finish_heros_(that.finish_heros_)
	, wander_heros_(that.wander_heros_)
	, economy_area_(that.economy_area_)
	, district_(that.district_)
	, district_locs_(that.district_locs_)
	, villages_(that.villages_)
	, roaded_cities_(that.roaded_cities_)
	, terrain_types_list_(that.terrain_types_list_)
	, alert_rect_(that.alert_rect_)
	, mayor_(that.mayor_)
	, commercial_exploiture_(that.commercial_exploiture_)
	, technology_exploiture_(that.technology_exploiture_)
	, gold_bonus_(that.gold_bonus_)
	, technology_bonus_(that.technology_bonus_)
	, max_commoners_(that.max_commoners_)
	, research_turns_(that.research_turns_)
	, max_level_(that.max_level_)
	, fronts_(that.fronts_)
	, police_(that.police_)
	, soldiers_(that.soldiers_)
	, not_recruit_(that.not_recruit_)
	, alias_(that.alias_)
	, can_recruit_map_(that.can_recruit_map_)
	, decree_(that.decree_)
	, max_recruit_cost_(that.max_recruit_cost_)
{
	for (std::vector<unit*>::const_iterator it = that.reside_troops_.begin(); it != that.reside_troops_.end(); ++ it) {
		reside_troops_.push_back(new unit(**it));
	}
	for (std::vector<unit*>::const_iterator it = that.reside_commoners_.begin(); it != that.reside_commoners_.end(); ++ it) {
		reside_commoners_.push_back(new unit(**it));
	}
}

artifical::artifical(unit_map& units, hero_map& heros, std::vector<team>& teams, game_state& state, type_heros_pair& t, int cityno, bool use_traits) :
	unit(units, heros, teams, state, t, cityno, use_traits, true)
	, map_(*resources::game_map)
	, reside_troops_()
	, field_troops_()
	, reside_commoners_()
	, field_commoners_()
	, field_arts_()
	, fresh_heros_()
	, finish_heros_()
	, wander_heros_()
	, economy_area_()
	, district_()
	, district_locs_()
	, villages_()
	, roaded_cities_()
	, terrain_types_list_(t_translation::t_match(type()->match_).terrain)
	, alert_rect_()
	, mayor_(&hero_invalid)
	, commercial_exploiture_(0)
	, technology_exploiture_(0)
	, gold_bonus_(0)
	, technology_bonus_(0)
	, max_commoners_(game_config::max_commoners)
	, research_turns_(0)
	, fronts_(0)
	, police_(game_config::max_police / 2)
	, soldiers_(0)
	, not_recruit_()
	, alias_()
	, can_recruit_map_()
	, decree_(NULL)
	, max_recruit_cost_(-1)
{
	calculate_exploiture();
}

artifical::~artifical()
{
	for (std::vector<unit*>::const_iterator it = reside_troops_.begin(); it != reside_troops_.end(); ++ it) {
		delete *it;
	}
	for (std::vector<unit*>::const_iterator it = reside_commoners_.begin(); it != reside_commoners_.end(); ++ it) {
		delete *it;
	}
}

void artifical::read(game_state& state, const config& cfg, bool use_traits)
{
	// "service" heros
	const std::vector<std::string> fresh_heros = utils::split(cfg_["service_heros"]);
	std::vector<std::string>::const_iterator tmp;
	for (tmp = fresh_heros.begin(); tmp != fresh_heros.end(); ++ tmp) {
		fresh_heros_.push_back(&heros_[lexical_cast_default<int>(*tmp)]);
		fresh_heros_.back()->status_ = hero_status_idle;
		fresh_heros_.back()->city_ = cityno_;
		fresh_heros_.back()->side_ = side_ - 1;
	}

	// "wander" heros
	const std::vector<std::string> wander_heros = utils::split(cfg_["wander_heros"]);
	for (tmp = wander_heros.begin(); tmp != wander_heros.end(); ++ tmp) {
		wander_heros_.push_back(&heros_[lexical_cast_default<int>(*tmp)]);
		wander_heros_.back()->status_ = hero_status_wander;
		wander_heros_.back()->city_ = cityno_;
	}

	// "finish" heros (previouse code has set variable: status_)
	const std::vector<std::string> finish_heros = utils::split(cfg_["finish_heros"]);
	for (tmp = finish_heros.begin(); tmp != finish_heros.end(); ++ tmp) {
		finish_heros_.push_back(&heros_[lexical_cast_default<int>(*tmp)]);
	}

	std::stringstream err;
	// economy_area
	// 相比parenthetical_split(cfg_["economy_area"], ','), 以下是这种判断是会使得economy_area多出数个,项, 但可以省去去除()麻烦
	const std::vector<std::string> economy_area = utils::parenthetical_split(cfg_["economy_area"]);
	for (std::vector<std::string>::const_iterator itor = economy_area.begin(); itor != economy_area.end(); ++ itor) {
		const std::vector<std::string> loc_str = utils::split(*itor);
		if (loc_str.size() == 2) {
			const map_location loc(lexical_cast_default<int>(loc_str[0]) - 1, lexical_cast_default<int>(loc_str[1]) - 1);
			if (unit_map::economy_areas_.find(loc) != unit_map::economy_areas_.end()) {
				err << "reuse economy area: (" << loc_str[0] << ", " << loc_str[1] << ") by city " << master_->name();
				err << ", it was holden by " << units_.city_from_cityno(unit_map::economy_areas_.find(loc)->second)->name();
				throw game::load_game_failed(err.str());
			} else if (map_.on_board(loc)) {
				economy_area_.push_back(loc);
				unit_map::economy_areas_[loc] = cityno_;
			} else {
				err << "invalid coordinate of economy area: (" << loc_str[0] << ", " << loc_str[1] << ") on city " << master_->name();
				throw game::load_game_failed(err.str());
			}
		}
	}

	// district
	const std::vector<std::string> district = utils::parenthetical_split(cfg_["district"]);
	SDL_Rect rc;
	for (std::vector<std::string>::const_iterator itor = district.begin(); itor != district.end(); ++ itor) {
		const std::vector<std::string> loc_str = utils::split(*itor);
		if (loc_str.size() == 4) {
			rc.x = lexical_cast_default<int>(loc_str[0]) - 1;
			if (rc.x < 0 || rc.x > map_.w()) {
				err << "invalid coordinate x: " << loc_str[0] << ", on city " << master_->name();
				throw game::load_game_failed(err.str());
			}
			rc.y = lexical_cast_default<int>(loc_str[1]) - 1;
			if (rc.y < 0 || rc.y > map_.h()) {
				err << "invalid coordinate y: " << loc_str[1] << ", on city " << master_->name();
				throw game::load_game_failed(err.str());
			}
			rc.w = lexical_cast_default<int>(loc_str[2]);
			if (rc.w < 0 || rc.x + rc.w > map_.w()) {
				err << "invalid width: " << loc_str[2] << "(map width: " << map_.w() << "), on city " << master_->name();
				throw game::load_game_failed(err.str());
			}
			rc.h = lexical_cast_default<int>(loc_str[3]);
			if (rc.h < 0 || rc.y + rc.h > map_.h()) {
				err << "invalid height: " << loc_str[3] << "(map height: " << map_.h() << "), on city " << master_->name();
				throw game::load_game_failed(err.str());
			}
			// parse std::vector<SDL_Rect> to std::set<map_location>
			for (int x = 0; x < rc.w; x ++) {
				for (int y = 0; y < rc.h; y ++) {
					district_locs_.insert(map_location(rc.x + x, rc.y + y));
				}
			}
			district_.push_back(rc);
		}
	}

	if (this_is_city()) {
		mayor_ = &heros_[cfg["mayor"].to_int(HEROS_INVALID_NUMBER)];
		if (mayor_->number_ != HEROS_INVALID_NUMBER) {
			mayor_->official_ = hero_official_mayor;
		}
		fronts_ = cfg["fronts"].to_int();
		police_ = cfg["police"].to_int(50);
		soldiers_ = cfg["soldiers"].to_int();
		research_turns_ = cfg["research_turns"].to_int();

		// not recruit
		std::vector<std::string> vstr = utils::split(cfg["not_recruit"]);
		for(std::vector<std::string>::const_iterator i = vstr.begin(); i != vstr.end(); ++i) {
			const unit_type* ut = unit_types.find(*i);
			if (!ut) {
				throw game::load_game_failed("invalid not_recruit attribute in city(" + master_->name() + "), type: " + *i);
			}
			not_recruit_.insert(ut);
		}
		// alias
		alias_ = cfg["alias"].t_str();
		if (!alias_.empty()) {
			master_->set_name(alias_.str());
		}
	}

	// read [unit] to reside troop
	BOOST_FOREACH (config &i, cfg_.child_range("unit")) {
		reside_troops_.push_back(new unit(units_, heros_, teams_, state, i));
	}
	// read [commoner] to reside commoner
	BOOST_FOREACH (config &i, cfg_.child_range("commoner")) {
		reside_commoners_.push_back(new unit(units_, heros_, teams_, state, i));
	}
}

void artifical::write(config& cfg) const
{
	unit::write(cfg);

	// “未”武将
	std::stringstream str;
	for (std::vector<hero*>::const_iterator h = fresh_heros_.begin(); h != fresh_heros_.end(); ++ h) {
		if (h != fresh_heros_.begin()) {
			str << ", ";
		}
		str << lexical_cast<std::string>((*h)->number_);
	}
	cfg["service_heros"] = str.str();

	// “在野”武将
	str.str("");
	for (std::vector<hero*>::const_iterator h = wander_heros_.begin(); h != wander_heros_.end(); ++ h) {
		if (h != wander_heros_.begin()) {
			str << ", ";
		}
		str << lexical_cast<std::string>((*h)->number_);
	}
	cfg["wander_heros"] = str.str();

	// “完”武将
	str.str("");
	for (std::vector<hero*>::const_iterator h = finish_heros_.begin(); h != finish_heros_.end(); ++ h) {
		if (h != finish_heros_.begin()) {
			str << ", ";
		}
		str << lexical_cast<std::string>((*h)->number_);
	}
	cfg["finish_heros"] = str.str();

	str.str("");
	// economy area
	for (std::vector<map_location>::const_iterator itor = economy_area_.begin(); itor != economy_area_.end(); ++ itor) {
		str << "(" << lexical_cast<std::string>(itor->x + 1) << "," << lexical_cast<std::string>(itor->y + 1) << ")";
	}
	cfg["economy_area"] = str.str();

	str.str("");
	// district
	for (std::vector<SDL_Rect>::const_iterator itor = district_.begin(); itor != district_.end(); ++ itor) {
		str << "(" << lexical_cast<std::string>(itor->x + 1) << "," << lexical_cast<std::string>(itor->y + 1) << "," << lexical_cast<std::string>(itor->w) << "," << lexical_cast<std::string>(itor->h) << ")";
	}
	cfg["district"] = str.str();

	if (this_is_city()) {
		cfg["fronts"] = fronts_;
		cfg["police"] = police_;
		cfg["soldiers"] = soldiers_;
		cfg["mayor"] = mayor_->number_;

		str.str("");
		for (std::set<const unit_type*>::const_iterator cr = not_recruit_.begin(); cr != not_recruit_.end(); ++cr) {
			if (cr != not_recruit_.begin()) {
				str << ",";
			}
			str << (*cr)->id();
		}
		cfg["not_recruit"] = str.str();

		// alias
		if (!alias_.empty()) {
			cfg["alias"] = alias_;
		}
	}

	for (std::vector<unit*>::const_iterator it = reside_troops_.begin(); it != reside_troops_.end(); ++ it) {
		const unit& u = **it;
		config& ucfg = cfg.add_child("unit");
		u.write(ucfg);
	}
	for (std::vector<unit*>::const_iterator it = reside_commoners_.begin(); it != reside_commoners_.end(); ++ it) {
		const unit& u = **it;
		config& ucfg = cfg.add_child("commoner");
		u.write(ucfg);
	}
}

void artifical::write(uint8_t* mem) const
{
	unit::write(mem);
	artifical_fields_t* fields = (artifical_fields_t*)mem;

	std::stringstream str;
	int offset = fields->size_;
	int val;

	fields->mayor_ = mayor_->number_;
	fields->fronts_ = fronts_;
	fields->police_ = police_;
	fields->soldiers_ = soldiers_;
	fields->gold_bonus_ = gold_bonus_;
	fields->technology_bonus_ = technology_bonus_;
	fields->max_commoners_ = max_commoners_;
	fields->decree_ = decree_? decree_->index(): NO_DECREE;
	fields->research_turns_ = research_turns_;
	fields->max_level_ = max_level_;

	str.str("");
	// fresh_heros
	for (std::vector<hero*>::const_iterator h = fresh_heros_.begin(); h != fresh_heros_.end(); ++ h) {
		if (h != fresh_heros_.begin()) {
			str << ", ";
		}
		val = (*h)->number_;
		str << val;
	}
	fields->fresh_heros_.offset_ = offset;
	fields->fresh_heros_.size_ = str.str().length();
	memcpy(mem + offset, str.str().c_str(), fields->fresh_heros_.size_);
	offset += fields->fresh_heros_.size_;

	str.str("");
	// wander_heros
	for (std::vector<hero*>::const_iterator h = wander_heros_.begin(); h != wander_heros_.end(); ++ h) {
		if (h != wander_heros_.begin()) {
			str << ", ";
		}
		val = (*h)->number_;
		str << val;
	}
	fields->wander_heros_.offset_ = offset;
	fields->wander_heros_.size_ = str.str().length();
	memcpy(mem + offset, str.str().c_str(), fields->wander_heros_.size_);
	offset += fields->wander_heros_.size_;

	str.str("");
	// finish_heros
	for (std::vector<hero*>::const_iterator h = finish_heros_.begin(); h != finish_heros_.end(); ++ h) {
		if (h != finish_heros_.begin()) {
			str << ", ";
		}
		val = (*h)->number_;
		str << val;
	}
	fields->finish_heros_.offset_ = offset;
	fields->finish_heros_.size_ = str.str().length();
	memcpy(mem + offset, str.str().c_str(), fields->finish_heros_.size_);
	offset += fields->finish_heros_.size_;

	str.str("");
	// economy area
	for (std::vector<map_location>::const_iterator itor = economy_area_.begin(); itor != economy_area_.end(); ++ itor) {
		str << "(" << lexical_cast<std::string>(itor->x + 1) << "," << lexical_cast<std::string>(itor->y + 1) << ")";
	}
	fields->economy_area_.offset_ = offset;
	fields->economy_area_.size_ = str.str().length();
	memcpy(mem + offset, str.str().c_str(), fields->economy_area_.size_);
	offset += fields->economy_area_.size_;

	str.str("");
	// district
	for (std::vector<SDL_Rect>::const_iterator itor = district_.begin(); itor != district_.end(); ++ itor) {
		str << "(" << lexical_cast<std::string>(itor->x + 1) << "," << lexical_cast<std::string>(itor->y + 1) << "," << lexical_cast<std::string>(itor->w) << "," << lexical_cast<std::string>(itor->h) << ")";
	}
	fields->district_.offset_ = offset;
	fields->district_.size_ = str.str().length();
	memcpy(mem + offset, str.str().c_str(), fields->district_.size_);
	offset += fields->district_.size_;

	// recruit
	if (!not_recruit_.empty()) {
		str.str("");
		for (std::set<const unit_type*>::const_iterator cr = not_recruit_.begin(); cr != not_recruit_.end(); ++cr) {
			if (cr != not_recruit_.begin()) {
				str << ",";
			}
			str << (*cr)->id();
		}
		fields->not_recruit_.size_ = str.str().length();
		memcpy(mem + offset, str.str().c_str(), fields->not_recruit_.size_);
	} else {
		fields->not_recruit_.size_ = 0;
	}
	fields->not_recruit_.offset_ = offset;
	offset += fields->not_recruit_.size_;

	// alias
	if (!alias_.empty()) {
		str.str("");
		std::string textdomain, msgid;
		split_t_string(alias_, textdomain, msgid);
		str << textdomain << ", " << msgid;

		fields->alias_.size_ = str.str().length();
		memcpy(mem + offset, str.str().c_str(), fields->alias_.size_);
	} else {
		fields->alias_.size_ = 0;
	}
	fields->alias_.offset_ = offset; 
	offset += fields->alias_.size_;

	// align 4
	offset = (offset + 3) & ~3;

	fields->reside_troops_.offset_ = offset;
	fields->reside_troops_.size_ = reside_troops_.size();
	for (std::vector<unit*>::const_iterator it = reside_troops_.begin(); it != reside_troops_.end(); ++ it) {
		const unit& u = **it;
		u.write(mem + offset);
		unit_fields_t* fields = (unit_fields_t*)(mem + offset);
		offset += fields->size_;
	}

	fields->reside_commoners_.offset_ = offset;
	fields->reside_commoners_.size_ = reside_commoners_.size();
	for (std::vector<unit*>::const_iterator it = reside_commoners_.begin(); it != reside_commoners_.end(); ++ it) {
		const unit& u = **it;
		u.write(mem + offset);
		unit_fields_t* fields = (unit_fields_t*)(mem + offset);
		offset += fields->size_;
	}

	fields->size_ = offset;
}

void artifical::read(const uint8_t* mem)
{
	std::string str;
	artifical_fields_t* fields = (artifical_fields_t*)mem;
	
	mayor_ = &heros_[fields->mayor_];
	fronts_ = fields->fronts_;
	police_ = fields->police_;
	soldiers_ = fields->soldiers_;
	gold_bonus_ = fields->gold_bonus_;
	technology_bonus_ = fields->technology_bonus_;
	max_commoners_ = fields->max_commoners_;
	decree_ = fields->decree_ != NO_DECREE? &unit_types.decree(fields->decree_): NULL;
	research_turns_ = fields->research_turns_;
	max_level_ = fields->max_level_;

	// fresh_heros_
	str.assign((const char*)mem + fields->fresh_heros_.offset_, fields->fresh_heros_.size_);
	const std::vector<std::string> fresh_heros = utils::split(str);
	std::vector<std::string>::const_iterator tmp;
	for (tmp = fresh_heros.begin(); tmp != fresh_heros.end(); ++ tmp) {
		fresh_heros_.push_back(&heros_[lexical_cast_default<int>(*tmp)]);
		fresh_heros_.back()->status_ = hero_status_idle;
		fresh_heros_.back()->city_ = cityno_;
		fresh_heros_.back()->side_ = side_ - 1;
	}

	// wander_heros_
	str.assign((const char*)mem + fields->wander_heros_.offset_, fields->wander_heros_.size_);
	const std::vector<std::string> wander_heros = utils::split(str);
	for (tmp = wander_heros.begin(); tmp != wander_heros.end(); ++ tmp) {
		wander_heros_.push_back(&heros_[lexical_cast_default<int>(*tmp)]);
		wander_heros_.back()->status_ = hero_status_wander;
		wander_heros_.back()->city_ = cityno_;
	}

	// finish_heros_(to finish heros, status_ has bee set)
	str.assign((const char*)mem + fields->finish_heros_.offset_, fields->finish_heros_.size_);
	const std::vector<std::string> finish_heros = utils::split(str);
	for (tmp = finish_heros.begin(); tmp != finish_heros.end(); ++ tmp) {
		finish_heros_.push_back(&heros_[lexical_cast_default<int>(*tmp)]);
	}

	// economy_area
	// 相比parenthetical_split(cfg_["economy_area"], ','), 以下是这种判断是会使得economy_area多出数个,项, 但可以省去去除()麻烦
	str.assign((const char*)mem + fields->economy_area_.offset_, fields->economy_area_.size_);
	const std::vector<std::string> economy_area = utils::parenthetical_split(str);
	for (std::vector<std::string>::const_iterator itor = economy_area.begin(); itor != economy_area.end(); ++ itor) {
		const std::vector<std::string> loc_str = utils::split(*itor);
		if (loc_str.size() == 2) {
			const map_location loc(lexical_cast_default<int>(loc_str[0]) - 1, lexical_cast_default<int>(loc_str[1]) - 1);
			economy_area_.push_back(loc);
			unit_map::economy_areas_[loc] = cityno_;
		}
	}

	// district
	str.assign((const char*)mem + fields->district_.offset_, fields->district_.size_);
	const std::vector<std::string> district = utils::parenthetical_split(str);
	SDL_Rect rc;
	std::stringstream err;
	for (std::vector<std::string>::const_iterator itor = district.begin(); itor != district.end(); ++ itor) {
		const std::vector<std::string> loc_str = utils::split(*itor);
		if (loc_str.size() == 4) {
			rc.x = lexical_cast_default<int>(loc_str[0]) - 1;
			if (rc.x < 0 || rc.x > map_.w()) {
				err << "invalid coordinate x: " << loc_str[0] << ", on city " << master_->name();
				throw game::load_game_failed(err.str());
			}
			rc.y = lexical_cast_default<int>(loc_str[1]) - 1;
			if (rc.y < 0 || rc.y > map_.h()) {
				err << "invalid coordinate y: " << loc_str[1] << ", on city " << master_->name();
				throw game::load_game_failed(err.str());
			}
			rc.w = lexical_cast_default<int>(loc_str[2]);
			if (rc.w < 0 || rc.x + rc.w > map_.w()) {
				err << "invalid width: " << loc_str[2] << "(map width: " << map_.w() << "), on city " << master_->name();
				throw game::load_game_failed(err.str());
			}
			rc.h = lexical_cast_default<int>(loc_str[3]);
			if (rc.h < 0 || rc.y + rc.h > map_.h()) {
				err << "invalid height: " << loc_str[3] << "(map height: " << map_.h() << "), on city " << master_->name();
				throw game::load_game_failed(err.str());
			}
			// parse std::vector<SDL_Rect> to std::set<map_location>
			for (int x = 0; x < rc.w; x ++) {
				for (int y = 0; y < rc.h; y ++) {
					district_locs_.insert(map_location(rc.x + x, rc.y + y));
				}
			}
			district_.push_back(rc);
		}
	}

	std::vector<std::string> v_str;
	// recruit
	not_recruit_.clear();
	if (fields->not_recruit_.size_) {
		str.assign((const char*)mem + fields->not_recruit_.offset_, fields->not_recruit_.size_);
		v_str = utils::split(str);
		for (std::vector<std::string>::const_iterator i = v_str.begin(); i != v_str.end(); ++i) {
			const unit_type* ut = unit_types.find(*i);
			VALIDATE(ut, "team::read, unknown not-recruit unit type: " + *i);
			not_recruit_.insert(ut);
		}
	}

	// alias
	alias_ = null_str;
	if (fields->alias_.size_) {
		str.assign((const char*)mem + fields->alias_.offset_, fields->alias_.size_);
		v_str = utils::split(str);
		if (v_str.size() == 2) {
			alias_ = dgettext(v_str[0].c_str(), v_str[1].c_str());
			master_->set_name(alias_.str());
		}
	}

	int offset = fields->reside_troops_.offset_;
	for (int i = 0; i < fields->reside_troops_.size_; i ++) {
		reside_troops_.push_back(new unit(units_, heros_, teams_, mem + offset));

		unit_fields_t* u_fields = (unit_fields_t*)(mem + offset);
		offset += u_fields->size_;
	}

	offset = fields->reside_commoners_.offset_;
	for (int i = 0; i < fields->reside_commoners_.size_; i ++) {
		reside_commoners_.push_back(new unit(units_, heros_, teams_, mem + offset));

		unit_fields_t* u_fields = (unit_fields_t*)(mem + offset);
		offset += u_fields->size_;
	}
}

const std::set<map_location>& artifical::villages() const
{
	return villages_;
}

void artifical::set_roaded_cities(const std::set<int>& cities)
{
	roaded_cities_ = cities;
}

const std::set<int>& artifical::roaded_cities() const 
{
	return roaded_cities_;
}

const std::set<map_location>& artifical::district_locs() const
{
	return district_locs_;
}

#define ALERT_RECT_RADIUS		10
#define VILLAGE_RECT_RADIUS		15
void artifical::set_location(const map_location &loc)
{
	if (loc_ == loc) {
		return;
	}

	unit::set_location(loc);

	alert_rect_ = extend_rectangle(map_, loc_.x, loc_.y, ALERT_RECT_RADIUS);
	if (is_city() && villages_.empty()) {
		SDL_Rect rect = extend_rectangle(map_, loc_.x, loc_.y, VILLAGE_RECT_RADIUS);
		for (int y = rect.y; y < rect.y + rect.h; y ++) {
			for (int x = rect.x; x < rect.x + rect.w; x ++) {
				if (map_.is_village(map_location(x, y))) {
					villages_.insert(map_location(x, y));
				}
			}
		}
	}

	// reset loc_ of reside troops
	for (std::vector<unit*>::iterator it = reside_troops_.begin(); it != reside_troops_.end(); ++ it) {
		unit& u = **it;
		u.set_location(loc);
	}
	for (std::vector<unit*>::iterator it = reside_commoners_.begin(); it != reside_commoners_.end(); ++ it) {
		unit& u = **it;
		u.set_location(loc);
	}
	
	const std::set<map_location::DIRECTION>& touch_dirs = unit_type_->touch_dirs();
	if (!touch_dirs.empty()) {
		// fill up and verify touch locations.
		for (std::set<map_location::DIRECTION>::const_iterator itor = touch_dirs.begin(); itor != touch_dirs.end(); ++ itor) {
			map_location offset = loc.get_direction(*itor);
			if (!map_.on_board(offset)) {
				std::stringstream str;
				str << "A location of " << master_->name() << "(" << loc << ") is out of map!";
				throw game::load_game_failed(str.str());
			} else if (map_.is_village(offset)) {
				std::stringstream str;
				str << "A location of " << master_->name() << "(" << loc << ") is village!";
				throw game::load_game_failed(str.str());
			}
			touch_locs_.insert(offset);
		}
/*
		// multi-grid artifical's terrain must be water or falt. 
		for (std::set<map_location>::const_iterator i = touch_locs_.begin(); i != touch_locs_.end(); ++ i) {
			const std::string& group = map.get_terrain_info(map.get_terrain(*i)).editor_group();
			const std::vector<std::string> group_ver = utils::split(group);
			bool found = false;
			for (std::vector<std::string>::const_iterator g = group_ver.begin(); g != group_ver.end(); ++ g) {
				if (*g == "water" || *g == "flat") {
					found = true;
					break;
				}
			}
			if (!found) {
				std::stringstream str;
				str << "Editor group(" << group << ") of " << master_->name() << "(" << *i << ") isn't water or flat!";
				throw game::load_game_failed(str.str());
			}
		}
*/
		// recalculate adjacent grids
		map_offset* adjacent_ptr;
		size_t i, size;

		// range=1
		size = (sizeof(adjacent_2) / sizeof(map_offset)) >> 1;
		adjacent_ptr = adjacent_2[loc.x & 0x1];
		for (i = 0; i < size; i ++) {
			adjacent_[adjacent_size_].x = loc.x + adjacent_ptr[i].x;
			adjacent_[adjacent_size_].y = loc.y + adjacent_ptr[i].y;
			if (map_.on_board(adjacent_[adjacent_size_])) {
				adjacent_size_ ++;
			}
		}
		// range=2,3
		size = (sizeof(adjacent_3) / sizeof(map_offset)) >> 1;
		adjacent_ptr = adjacent_3[loc.x & 0x1];
		for (i = 0; i < size; i ++) {
			adjacent_3_[adjacent_size_3_].x = loc.x + adjacent_ptr[i].x;
			adjacent_3_[adjacent_size_3_].y = loc.y + adjacent_ptr[i].y;
			if (map_.on_board(adjacent_3_[adjacent_size_3_])) {
				adjacent_size_3_ ++;
			}
		}
		std::copy(adjacent_3_, adjacent_3_ + adjacent_size_3_, adjacent_2_);
		adjacent_size_2_ = adjacent_size_3_;
	}
}

void artifical::issue_decree(const config& effect)
{
	int apply_to = apply_to_tag::find(effect["apply_to"].str());
	const std::string& increase = effect["increase"];

	if (increase.empty()) return;

	if (apply_to == apply_to_tag::BUSINESS) {
		business_speed_ = utils::apply_modifier(business_speed_, increase);
		if (business_speed_ > 200) {
			business_speed_ = 200;
		}
		const std::string& increase_income = effect["increase_income"].str();
		if (!increase_income.empty()) {
			business_income_ratio_ = utils::apply_modifier(business_income_ratio_, increase_income);
		}
	} else if (apply_to == apply_to_tag::TECH) {
		technology_speed_ = utils::apply_modifier(technology_speed_, increase);
		if (technology_speed_ > 200) {
			technology_speed_ = 200;
		}
		const std::string& increase_income = effect["increase_income"].str();
		if (!increase_income.empty()) {
			technology_income_ratio_ = utils::apply_modifier(technology_income_ratio_, increase_income);
		}
	} else if (apply_to == apply_to_tag::DEMOLISH) {
		destruct_hp_ = atoi(increase.c_str());
	} else if (apply_to == apply_to_tag::HEAL) {
		heal_hp_ = atoi(increase.c_str());
	}
}

void artifical::redraw_unit()
{
	game_display &disp = *game_display::get_singleton();
	const gamemap &map = disp.get_map();
	if (!loc_.valid() || hidden_ || disp.fogged(loc_) || (invisible(loc_) && disp.get_teams()[disp.viewing_team()].is_enemy(side()))) {
		clear_haloes();
		if(anim_) {
			anim_->update_last_draw_time();
		}
		return;
	}
	if(refreshing_) {
		return;
	}
	refreshing_ = true;
	redraw_counter_ ++;

	std::vector<team>& teams = *resources::teams;
	play_controller& controller = *resources::controller;
	team& t = teams[side_ - 1];
	bool expediting_from_it = ((disp.expedite_city() == this)? true: false) && t.is_human();

	if (!anim_) {
		set_standing();
	}
	anim_->update_last_draw_time();
	frame_parameters params;
	const t_translation::t_terrain terrain = map.get_terrain(loc_);
	const terrain_type& terrain_info = map.get_terrain_info(terrain);
	// do not set to 0 so we can distinguih the flying from the "not on submerge terrain"
	params.submerge= is_flying() ? 0.01 : terrain_info.unit_submerge();

	if (invisible(loc_) && params.highlight_ratio > 0.5) {
		params.highlight_ratio = 0.5;
	}
	if (loc_ == units_.center_loc(disp.selected_hex()) && params.highlight_ratio == 1.0) {
		params.highlight_ratio = 1.5;
	}
	int height_adjust = static_cast<int>(terrain_info.unit_height_adjust() * disp.get_zoom_factor());
	if (is_flying() && height_adjust < 0) {
		height_adjust = 0;
	}
	params.y -= height_adjust;
	params.halo_y -= height_adjust;
	if (get_state(ustate_tag::POISONED)){
		params.blend_with = disp.rgb(0,255,0);
		params.blend_ratio = 0.25;
	} else if (temporary_state_ & BIT_2_MASK(BIT_STRONGER)) {
		params.blend_with = disp.rgb(255, 32, 32);
		params.blend_ratio = 0.40;
	} 
	params.image_mod = image_mods();
	if (get_state(ustate_tag::PETRIFIED)) params.image_mod +="~GS()";
	if (terrain_ == t_translation::NONE_TERRAIN) {
		params.image= absolute_image();
	}
	params.sound_filter = sound_filter_tag::rfind(sound_filter_tag::from_hero_gender(master_->gender_));

	const frame_parameters adjusted_params = anim_->get_current_params(params);



	const map_location dst = loc_.get_direction(facing_);
	const int xsrc = disp.get_location_x(loc_);
	const int ysrc = disp.get_location_y(loc_);
	const int xdst = disp.get_location_x(dst);
	const int ydst = disp.get_location_y(dst);
	int d2 = disp.hex_size() / 2;




	const int x = static_cast<int>(adjusted_params.offset_x * xdst + (1.0-adjusted_params.offset_x) * xsrc) + d2;
	const int y = static_cast<int>(adjusted_params.offset_y * ydst + (1.0-adjusted_params.offset_y) * ysrc) + d2;


	if(unit_halo_ == halo::NO_HALO && !image_halo().empty()) {
		unit_halo_ = halo::add(0, 0, image_halo(), map_location(-1, -1));
	}
	if(unit_halo_ != halo::NO_HALO && image_halo().empty()) {
		halo::remove(unit_halo_);
		unit_halo_ = halo::NO_HALO;
	} else if(unit_halo_ != halo::NO_HALO) {
		halo::set_location(unit_halo_, x, y);
	}



	// We draw bars only if wanted, visible on the map view
	bool draw_bars = draw_bars_ ;
	if (draw_bars) {
		const int d = disp.hex_size();
		SDL_Rect unit_rect = {xsrc, ysrc +adjusted_params.y, d, d};
		draw_bars = rects_overlap(unit_rect, disp.map_outside_area());
	}

	std::stringstream strstr;
	strstr.str("");
	if (!expediting_from_it && can_reside_) {
		strstr << fresh_heros_.size() + finish_heros_.size() << "(" << fresh_heros_.size() << ")/" << reside_troops_.size();
		if (display::default_zoom_ == display::ZOOM_48) {
			disp.draw_text_in_hex2(loc_, display::LAYER_BORDER, strstr.str(), 15, font::NORMAL_COLOR, xsrc + 24, ysrc - 22);
		} else if (display::default_zoom_ == display::ZOOM_56) {
			disp.draw_text_in_hex2(loc_, display::LAYER_BORDER, strstr.str(), 15, font::NORMAL_COLOR, xsrc + 27, ysrc - 26);
		} else if (display::default_zoom_ == display::ZOOM_64) {
			disp.draw_text_in_hex2(loc_, display::LAYER_BORDER, strstr.str(), 15, font::NORMAL_COLOR, xsrc + 30, ysrc - 30);
		} else {
			disp.draw_text_in_hex2(loc_, display::LAYER_BORDER, strstr.str(), 15, font::NORMAL_COLOR, xsrc + 32, ysrc - 32);
		}
	}

	if (!expediting_from_it && draw_bars) {
		std::string* energy_file;
		if (display::default_zoom_ == display::ZOOM_48) {
			energy_file = &game_config::images::bar_hrl_48;
		} else if (display::default_zoom_ == display::ZOOM_56) {
			energy_file = &game_config::images::bar_hrl_56;
		} else if (display::default_zoom_ == display::ZOOM_64) {
			energy_file = &game_config::images::bar_hrl_64;
		} else {
			energy_file = &game_config::images::bar_hrl_72;
		}

		double unit_energy = 0.0;
		if(max_hitpoints() > 0) {
			unit_energy = double(hitpoints())/double(max_hitpoints());
		}
		const int hp_bar_height = static_cast<int>(max_hitpoints()*game_config::hp_bar_scaling);

		int xsrc_zoom, ysrc_zoom;
		const fixed_t bar_alpha = (loc_ == disp.mouseover_hex() || loc_ == disp.selected_hex()) ? ftofxp(1.0): ftofxp(0.8);
		strstr.str("");
		strstr << food_;
		if (display::default_zoom_ == display::ZOOM_48) {
			if (is_city()) {
				if (especial_ != NO_ESPECIAL) {
					disp.drawing_buffer_add(display::LAYER_UNIT_BAR, loc_, xsrc + 2, ysrc - 16, image::get_image(unit_types.especial(especial_).image_));
				}
				xsrc_zoom = xsrc + 26;
				ysrc_zoom = ysrc + 3;
			} else {
				if (fort_) {
					disp.draw_text_in_hex2(loc_, display::LAYER_BORDER, strstr.str(), 15, font::NORMAL_COLOR, xsrc + 15, ysrc);
				}
				xsrc_zoom = xsrc + 10;
				ysrc_zoom = ysrc + 12;
			}
		} else if (display::default_zoom_ == display::ZOOM_56) {
			if (is_city()) {
				if (especial_ != NO_ESPECIAL) {
					disp.drawing_buffer_add(display::LAYER_UNIT_BAR, loc_, xsrc + 2, ysrc - 16, image::get_image(unit_types.especial(especial_).image_));
				}
				xsrc_zoom = xsrc + 26;
				ysrc_zoom = ysrc + 3;
			} else {
				if (fort_) {
					disp.draw_text_in_hex2(loc_, display::LAYER_BORDER, strstr.str(), 15, font::NORMAL_COLOR, xsrc + 15, ysrc);
				}
				xsrc_zoom = xsrc + 13;
				ysrc_zoom = ysrc + 12;
			}
		} else if (display::default_zoom_ == display::ZOOM_64) {
			if (is_city()) {
				if (especial_ != NO_ESPECIAL) {
					disp.drawing_buffer_add(display::LAYER_UNIT_BAR, loc_, xsrc + 4, ysrc - 16, image::get_image(unit_types.especial(especial_).image_));
				}
				xsrc_zoom = xsrc + 28;
				ysrc_zoom = ysrc + 2;
			} else {
				if (fort_) {
					disp.draw_text_in_hex2(loc_, display::LAYER_BORDER, strstr.str(), 15, font::NORMAL_COLOR, xsrc + 15, ysrc);
				}
				xsrc_zoom = xsrc + 13;
				ysrc_zoom = ysrc + 12;
			}
		} else {
			if (is_city()) {
				if (especial_ != NO_ESPECIAL) {
					disp.drawing_buffer_add(display::LAYER_UNIT_BAR, loc_, xsrc + 6, ysrc - 16, image::get_image(unit_types.especial(especial_).image_));
				}
				xsrc_zoom = xsrc + 30;
				ysrc_zoom = ysrc + 2;
			} else {
				if (fort_) {
					disp.draw_text_in_hex2(loc_, display::LAYER_BORDER, strstr.str(), 15, font::NORMAL_COLOR, xsrc + 15, ysrc);
				}
				xsrc_zoom = xsrc + 13;
				ysrc_zoom = ysrc + 12;
			}
		}
		if (get_state(ustate_tag::BUILDING)) {
			std::string png = "misc/build1.png";
			if (redraw_counter_ % 30 < 15) {
				png = "misc/build2.png";
			}
			disp.drawing_buffer_add(display::LAYER_UNIT_BAR, loc_, xsrc + disp.hex_size() / 4, ysrc_zoom - 12, image::get_image(png));
		}
		disp.draw_bar(*energy_file, xsrc_zoom, ysrc_zoom, loc_, hp_bar_height, unit_energy, hp_color(), bar_alpha, false);

	}
	if (!expediting_from_it && can_reside_ && tent::mode != mode_tag::TOWER) {
		if (display::default_zoom_ == display::ZOOM_48) {
			disp.draw_text_in_hex2(loc_, display::LAYER_BORDER, name(), 15, font::BIGMAP_COLOR, xsrc + 24, ysrc - 11);
		} else if (display::default_zoom_ == display::ZOOM_56) {
			disp.draw_text_in_hex2(loc_, display::LAYER_BORDER, name(), 15, font::BIGMAP_COLOR, xsrc + 26, ysrc - 13);
		} else if (display::default_zoom_ == display::ZOOM_64) {
			disp.draw_text_in_hex2(loc_, display::LAYER_BORDER, name(), 15, font::BIGMAP_COLOR, xsrc + 29, ysrc - 15);
		} else {
			disp.draw_text_in_hex2(loc_, display::LAYER_BORDER, name(), 15, font::BIGMAP_COLOR, xsrc + 32, ysrc - 16);
		}
	}

	// params.highlight_ratio = 0.5;
	params.drawing_layer = display::LAYER_UNIT_FG - display::LAYER_UNIT_FIRST;
	if (can_reside_) {
		// to city, display full image always.
		params.submerge = 0.0;
	}

	anim_->redraw(params);

	refreshing_ = false;
}

void artifical::fill_ai_hero(std::set<int>& candidate, int count, int& random)
{
	// 1/3: wander
	while (count && !candidate.empty()) {
		int pos = random % candidate.size();
		std::set<int>::iterator it = candidate.begin();
		std::advance(it, pos);
		hero& h = heros_[*it];

		if (count % 5 < 3) {
			fresh_into(h);
		} else {
			wander_into(h, false);
		}

		candidate.erase(it);
		count --;

		random = random * 1103515245 + 12345;
		random = (static_cast<unsigned>(random / 65536) % 32768);
	}
}

void artifical::fill_human_hero(const hero& leader, std::set<int>& candidate, int fill_freshes, int fill_wanders, int& random)
{
	for (int step = 0; step < 2; step ++) {
		int count = step == 0? fill_freshes: fill_wanders;
		while (count && !candidate.empty()) {
			int pos = random % candidate.size();
			std::set<int>::iterator it = candidate.begin();
			std::advance(it, pos);
			hero& h = heros_[*it];

			if (!runtime_groups::exist_member(h.number_, leader)) {
				if (step == 0) {
					fresh_into(h);
				} else {
					wander_into(h, false);
				}

				candidate.erase(it);
				count --;
			}

			random = random * 1103515245 + 12345;
			random = (static_cast<unsigned>(random / 65536) % 32768);
		}
	}
}

hero* artifical::select_leader(int random) const
{
	int best_score = -1;
	hero* best_hero = NULL;
	for (std::vector<hero*>::const_iterator it = fresh_heros_.begin(); it != fresh_heros_.end(); ++ it) {
		hero& h = **it;
		if (h.get_flag(hero_flag_roam)) {
			continue;
		}
		int score = fxptoi9(h.leadership_) + fxptoi9(h.charm_);
		if (h.utype_ != HEROS_NO_UTYPE) {
			const unit_type* ut = unit_types.keytype(h.utype_);
			if (ut->require() == unit_type::REQUIRE_LEADER) {
				score += 256;
			}
		}
		if (score > best_score) {
			best_score = score;
			best_hero = &h;
		}
	}
	return best_hero;
}

bool artifical::new_turn(play_controller& controller, int random)
{
	unit::new_turn(controller, random);

	team& current_team = teams_[side_ - 1];
	
	if (!get_state(ustate_tag::BUILDING)) {
		int healing = heal_ * current_team.repair_increase_ / 100;
		int pos_max = max_hit_points_ - hit_points_;
		int neg_max = -(hit_points_ - 1);
		if (pos_max > 0) {
			// Do not try to "heal" if HP >= max HP
			if (healing > pos_max) {
				healing = pos_max;
			} else if (healing < neg_max) {
				healing = neg_max;
			}
			std::vector<unit*> executor;
			unit_display::unit_healing(*this, loc_, executor, healing);
			heal(healing);
		}
	}

	if (!this_is_city()) {
		if (!get_state(ustate_tag::BUILDING)) {
			if (unit_map::economy_areas_.find(loc_) != unit_map::economy_areas_.end()) {
				artifical& city = *units_.city_from_cityno(cityno_);
				int turn_experience = city.turn_experience();
				if (current_team.village_gold_increase_ > 100) {
					turn_experience = turn_experience + std::min(1, turn_experience * current_team.village_gold_increase_ / 100);
				}
				get_experience(increase_xp::attack_ublock(*this), turn_experience);
			} else {
				get_experience(increase_xp::attack_ublock(*this), turn_experience_);
			}
			dialogs::advance_unit(*this, current_team.is_human(), true);
		}
		return false;
	}

	hero& leader = *current_team.leader();

	if (tent::mode == mode_tag::RPG && controller.turn() > 1) {
		for (std::vector<hero*>::iterator it = fresh_heros_.begin(); it != fresh_heros_.end();) {
			hero* h = *it;
			if (h != rpg::h && h->loyalty(leader) < game_config::wander_loyalty_threshold) {
				// wander
				artifical* to_city = units_.city_from_seed(current_team.gold());
				game_events::show_hero_message(h, units_.city_from_cityno(h->city_), 
						_("In misfortune time, it is necessary to search chance that can carry out value of life."),
						game_events::INCIDENT_WANDER);
				to_city->wander_into(*h, current_team.is_human()? false: true);
				it = fresh_heros_.erase(it);
				break;
			} else {
				++ it;
			}
		}
	}

	// select mayor
	if (side_ != team::empty_side) {
		if (!mayor_->valid()) {
			select_mayor();
		}
	}

	// field artifical (continue to build if necessary)
	int can_builds = level_ + 1;
	for (std::vector<artifical*>::iterator it = field_arts_.begin(); it != field_arts_.end() && can_builds; ++ it) {
		artifical& art = **it;
		if (!art.get_state(ustate_tag::BUILDING)) {
			continue;
		}
		VALIDATE(art.hitpoints() < art.max_hitpoints(), "artifical::new_turn, building, but hp has full!");
		int healing = std::min(game_config::build_hp_per_turn, art.max_hitpoints() - art.hitpoints());
		unit_display::unit_repair(art, healing);
		art.heal(healing);
		if (art.hitpoints() == art.max_hitpoints()) {
			art.set_state(ustate_tag::BUILDING, false);
		}
		can_builds --;
	}

	// reside troops
	for (std::vector<unit*>::iterator it = reside_troops_.begin(); it != reside_troops_.end(); ++ it) {
		unit& u = **it;
		u.set_movement(u.total_movement());
		u.set_attacks(u.attacks_total());
		if (u.consider_loyalty()) {
			u.increase_loyalty(-1 * game_config::reside_troop_increase_loyalty);
		}
		u.increase_activity(20);
		if (u.hide_turns()) {
			u.set_hide_turns(0);
		}
		if (u.alert_turns()) {
			u.set_alert_turns(0);
		}
		VALIDATE(!u.provoked_turns(), "artifical::new_turn, reside troop cannot be provoked!");
		u.get_experience(increase_xp::turn_ublock(*this), 3);
		u.new_turn_tactics();
		u.set_state(ustate_tag::SLOWED, false);
		u.set_state(ustate_tag::BROKEN, false);
		u.set_state(ustate_tag::PETRIFIED, false);
		if (u.get_state(ustate_tag::POISONED)) {
			u.set_state(ustate_tag::POISONED, false);
		} else {
			u.heal(36);
		}
	}
	for (std::vector<unit*>::iterator it = reside_commoners_.begin(); it != reside_commoners_.end(); ++ it) {
		unit& u = **it;
		u.set_movement(u.total_movement());
		u.set_attacks(u.attacks_total());
		if (u.hide_turns()) {
			u.set_hide_turns(0);
		}
		u.new_turn_tactics();
		u.set_state(ustate_tag::SLOWED, false);
		u.set_state(ustate_tag::BROKEN, false);
		u.set_state(ustate_tag::PETRIFIED, false);
		if (u.get_state(ustate_tag::POISONED)) {
			u.set_state(ustate_tag::POISONED, false);
		} else {
			u.heal(24);
		}
	}

	// reside heros
	for (std::vector<hero*>::iterator itor = fresh_heros_.begin(); itor != fresh_heros_.end(); ++ itor) {
		hero& h = **itor;
		if (h.activity_ < HEROS_FULL_ACTIVITY) {
			if (10 + h.activity_ < HEROS_FULL_ACTIVITY) {
				h.activity_ += 20;
			} else {
				h.activity_ = HEROS_FULL_ACTIVITY;
			}
		}
	}
	for (std::vector<hero*>::iterator it = finish_heros_.begin(); it != finish_heros_.end(); ++ it) {
		(*it)->status_ = hero_status_idle;
	}
	std::copy(finish_heros_.begin(), finish_heros_.end(), std::back_inserter(fresh_heros_));
	finish_heros_.clear();

	gold_bonus_ = technology_bonus_ = 0;
	increase_police(1);

	if (!tent::tower_mode()) {
		// const std::vector<size_t>& holded_cards = current_team.holded_cards();
			VALIDATE(research_turns_ >= 0, "artifical::new_turn, research must not < 0!");
			research_turns_ ++;

			// carry out decree
			int adjacented_hp = destruct_hp_ * police_ / 100;
			if (adjacented_hp) {
				// select "lowest" enemy city.
				int lowest_hp = -1;
				bool lowest_is_full = true;
				artifical* selected = NULL;
				for (std::set<int>::const_iterator it2 = roaded_cities_.begin(); it2 != roaded_cities_.end(); ++ it2) {
					artifical& roaded = *units_.city_from_cityno(*it2);
					if (roaded.side() != team::empty_side && current_team.is_enemy(roaded.side())) {
						if (lowest_is_full) {
							if (roaded.hitpoints() != roaded.max_hitpoints()) {
								lowest_is_full = false;
								lowest_hp = roaded.hitpoints();
								selected = &roaded;
							} else if (lowest_hp == -1 || roaded.hitpoints() < lowest_hp) {
								lowest_hp = roaded.hitpoints();
								selected = &roaded;
							}
						} else if (roaded.hitpoints() < lowest_hp && roaded.hitpoints() != roaded.max_hitpoints()) {
							lowest_hp = roaded.hitpoints();
							selected = &roaded;
						}
					} 
				}
				if (selected) {
					demolish_to(*selected, adjacented_hp, random);
				}
			}

			adjacented_hp = heal_hp_ * police_ / 100;
			if (adjacented_hp) {
				const std::pair<unit**, size_t> p = current_team.field_troop();
				unit** troops = p.first;
				size_t troops_vsize = p.second;
				unit* selected = NULL;
				int max_damage = -1;
				for (size_t i = 0; i < troops_vsize; i ++) {
					unit& u = *(troops[i]);
					if (u.is_artifical()) {
						continue;
					}
					if (u.is_commoner()) {
						continue;
					}
					int damage = u.max_hitpoints() - u.hitpoints();
					if (damage > max_damage) {
						max_damage = damage;
						selected = &u;
					}
				}
				if (selected) {
					selected->heal(adjacented_hp);
					unit_display::unit_heal2(teams_, units_, this, *selected, adjacented_hp);
				}
			}

			// a turn amone [16..20]
			if (research_turns_ > 15) {
				if (random % 5 < research_turns_ - 15) {
					int index = current_team.get_random_decree(*this, random);
					if (index != NO_DECREE) {
						const tdecree& d = unit_types.decree(index);
						apply_issur_decree(d, true);

						research_turns_ = 0;

					} else if (research_turns_ >= 5) {
						research_turns_ -= 5;
					}
				}
			}
	}

	get_experience(increase_xp::attack_ublock(*this), turn_experience_);
	dialogs::advance_unit(*this, current_team.is_human(), true);

	if (tent::tower_mode()) {
		finish_2_fresh();
	}

	return false;
}

void artifical::advance_to(const unit_type *t, bool use_traits, game_state* state)
{
	unit::advance_to(t, use_traits, state);
	if (is_city()) {
		max_level_ = std::max(t->level(), max_level_);
	}
}

// @damage: negative
void artifical::demolish_to(artifical& to, int damage, int random)
{
	artifical* t = to.tactic_on_ea();
	bool miss = false;

	if (t) {
		if (random % 100 < t->miss_income()) {
			miss = true;
		}
		damage /= 2;
	}

	std::string text;
	if (!miss) {
		std::map<unit*, int> touched;
		// demolish cannot capture city
		if (damage < 0 && to.hitpoints() + damage <= 0) {
			damage = -1 * (to.hitpoints() - 1);
		}
		to.take_hit(-1 * damage);
		text = lexical_cast<std::string>(-1 * damage);
	} else {
		text = _("Miss");
	}
	unit_display::unit_destruct(teams_, units_, this, to, text, t); 
}

void artifical::ally_terminate_adjust(team& adjusting_team)
{
	units_.ally_terminate_adjust(adjusting_team, alert_rect_);
}

void artifical::apply_issur_decree(const tdecree& d, bool anim)
{
	reset_decree_effect();

	decree_ = &d;

	const std::vector<const tdecree*>& parts = d.parts();
	for (std::vector<const tdecree*>::const_iterator it = parts.begin(); it != parts.end(); ++ it) {
		const tdecree& p = **it;
		issue_decree(p.effect_cfg());
	}

	if (anim) {
		const events::command_disabler disable_commands;
		std::vector<unit*> touchers(1, this);
		unit_display::unit_text(*this, false, d.name());
	}
}

void artifical::clear_decree()
{
	reset_decree_effect();
	decree_ = NULL;
}

void artifical::set_resting(bool rest)
{
	unit::set_resting(rest);
}

int artifical::upkeep() const
{
	if (tent::tower_mode()) {
		return 0;
	}
	return reside_troops_.size() * 2;
}

void artifical::get_experience(const increase_xp::ublock& ub, int xp, bool master, bool second, bool third)
{
	if (this_is_city()) {
		unit::get_experience(ub, xp, master, second, third);
	} else {
		experience_ += xp;
		if (xp > 0 && unit_feature_val(hero_feature_xp)) {
			// when advance, xp is less than 0. 
			experience_ += xp;
		}
	}
}

// retval indicates whether cannot-undo triggered, for exapmle change mayor. 
bool artifical::troop_come_into(unit* that, int pos)
{
	bool side_changed = that->side() != side_;
	unit* u = that;

	if (pos >= 0) {
		if (!u->is_commoner()) {
			reside_troops_.insert(reside_troops_.begin() + pos, u);
		} else {
			reside_commoners_.insert(reside_commoners_.begin() + pos, u);
		}
	} else {
		if (!u->is_commoner()) {
			reside_troops_.push_back(u);
		} else {
			reside_commoners_.push_back(u);
		}
	}

	if (!u->is_commoner()) {
		// get ride of (official_ == hero_official_mayor)
		if (u->cityno() != cityno_) {
			for (int i = 0; i < 3; i ++) {
				hero* h = NULL;
				if (i == 0) {
					h = &u->master();
				} else if (i == 1) {
					h = &u->second();
				} else {
					h = &u->third();
				}
				if (!h->valid()) {
					break;
				}
				if (h->official_ == hero_official_mayor) {
					artifical* from = units_.city_from_cityno(h->city_);
					from->select_mayor(&hero_invalid);
				}
			}
		}
	}
	if (side_changed) {
		VALIDATE(!u->is_commoner(), "side_changed unit must be not commoner!");
		u->terminate_noble(true);
		u->set_task(unit::TASK_NONE);
	}

	u->set_side(side_);
	u->set_cityno(cityno_);
	u->set_location(loc_);
	if (!u->transport()) {
		u->set_goto(map_location());
	}
	u->set_from(map_location());
	u->set_block_turns(0);
	u->set_resting(false);
	u->set_state(ustate_tag::FORMATION_ATTACKED, false);
	u->set_state(ustate_tag::EXPEDITED, false);
	u->set_state(ustate_tag::REVIVALED, false);
	// when move from filed into city, anim should be updated.
	u->delete_anim();
	u->clear_visibility_cache();
	if (u->alert_turns()) {
		u->set_alert_turns(0);
	}
	if (u->provoked_turns()) {
		u->set_provoked_turns_next(*u, 0);
	}
	if (u->tactic_hero()) {
		u->remove_from_slot_cache();
	}

	if (!u->is_commoner() && !u->human()) {
		if ((rpg::stratum == hero_stratum_leader && side_ == rpg::h->side_ + 1) || (rpg::stratum == hero_stratum_mayor && mayor_ == rpg::h)) {
			u->set_human(true);
		}
	}

	if (side_changed) {
		u->adjust();
	}

	return false;
}

// used to field troop come into
bool artifical::troop_come_into2(unit* troop, int pos)
{
	units_.erase(troop, false);

	// below state will change belong city of troop, must place after .erase.
	return troop_come_into(troop, pos);
}

void unit_belong_to2(std::vector<team>& teams, unit_map& units, int to_cityno, unit* troop, bool loyalty, bool to_recorder)
{
	game_display* disp = game_display::get_singleton();
	artifical* to = units.city_from_cityno(to_cityno);
	int side = to? to->side(): team::empty_side;

	for (int i = 0; i < 3; i ++) {
		hero* h = NULL;
		if (i == 0) {
			h = &troop->master();
		} else if (i == 1) {
			h = &troop->second();
		} else {
			h = &troop->third();
		}
		if (!h->valid()) {
			break;
		}
		if (h->official_ == hero_official_mayor) {
			artifical* from = units.city_from_cityno(h->city_);
			from->select_mayor(&hero_invalid);
		}
	}

	team& previous_team = teams[troop->side() - 1];
	team& current_team = teams[side - 1];

	if (previous_team.side() != current_team.side()) {
		troop->terminate_noble(true);
	}

	if (to_recorder) {
		VALIDATE(to, "unit_belong_to2, when to is ROAM city, to must not NULL!");
		recorder.add_belong_to(troop, to, loyalty);
	}
	// 1. 将troop从它原属城市的城外部队列表删除
	artifical* src_city = units.city_from_cityno(troop->cityno());
	if (src_city) {
		if (!troop->is_artifical()) {
			if (!troop->is_commoner()) {
				src_city->field_troops_erase(troop);
			} else {
				src_city->field_commoners_erase(troop);
			}
		} else {
			src_city->field_arts_erase(unit_2_artifical(troop));
		}
	} else if (troop->consider_ticks()) {
		game_display* disp = game_display::get_singleton();
		if (disp) {
			disp->refresh_access_troops(troop->side() - 1, game_display::REFRESH_ERASE, const_cast<unit*>(troop));
		}
	}

	// 2. set troop attribute(city, side, human)
	troop->set_cityno(to_cityno);
	troop->set_side(side);
	if (!troop->is_artifical() && !troop->is_commoner()) {
		troop->set_task(unit::TASK_NONE);
	}
	// set human must before field_troops_add, 
	// field_troops_add use human flag to refresh access_troops. 
	if (to && !troop->is_artifical() && !troop->human() && !troop->is_commoner()) {
		hero* mayor = to->mayor();
		if ((rpg::stratum == hero_stratum_leader && side == rpg::h->side_ + 1) || (rpg::stratum == hero_stratum_mayor && mayor == rpg::h)) {
			troop->set_human(true);
		}
	}

	// 3. 把troop加入将属城市的城外部队列表
	if (to) {
		if (!troop->is_artifical()) {
			if (!troop->is_commoner()) {
				to->field_troops_add(troop);
				if (loyalty) {
					// adjust loyalty, avoid to wander immediately.
					troop->set_loyalty(game_config::wander_loyalty_threshold + 1);
				}
			} else {
				to->field_commoners_add(troop);
			}
		} else {
			to->field_arts_add(unit_2_artifical(troop));
		}
	} else if (troop->consider_ticks()) {
		if (disp) {
			disp->refresh_access_troops(troop->side() - 1, game_display::REFRESH_INSERT, troop);
		}
	}

	// of course, current side of troop maybe equal prevous, but need call below.
	// must keep sort-consistent between team.field_troop and artifical[n].field_troop.
	// moved troop are added to end regardless team.field_troop or artifical[n].field_troop.
	previous_team.erase_troop(troop);
	current_team.add_troop(troop);

	if (previous_team.side() != current_team.side()) {
		troop->adjust();
		if (tent::turn_based && troop->consider_ticks()) {
			// sort rule is relative to side, change side should result to resort.
			if (disp) {
				disp->resort_access_troops(*troop);
			} else {
				units.resort_map(*troop);
			}
		}
	}
}

// before call unit_belong_to, don't update cityno and side!
void artifical::unit_belong_to(unit* troop, bool loyalty, bool to_recorder)
{
	unit_belong_to2(teams_, units_, cityno_, troop, loyalty, to_recorder);
}

// 出城
// Attention: For iterator, go out in default_ai::do_move don't call this function, erase itor from reside_troops_ directly.
void artifical::troop_go_out(const unit& u, bool del)
{
	std::vector<unit*>::iterator it = std::find(reside_troops_.begin(), reside_troops_.end(), &u);
	reside_troops_.erase(it);
	if (del) {
		delete &u;
	}
}

void artifical::commoner_go_out(const unit& u, bool del)
{
	std::vector<unit*>::iterator it = std::find(reside_commoners_.begin(), reside_commoners_.end(), &u);
	reside_commoners_.erase(it);
	if (del) {
		delete &u;
	}
}

void artifical::field_troops_add(unit* troop)
{
	game_display* disp = game_display::get_singleton();
	if (disp) {
		disp->refresh_access_troops(side_ - 1, game_display::REFRESH_INSERT, troop);
	}

	field_troops_.push_back(troop);
}

void artifical::field_commoners_add(unit* commoner)
{
	field_commoners_.push_back(commoner);
}

void artifical::field_arts_add(artifical* troop)
{
	field_arts_.push_back(troop);
}

void artifical::field_troops_erase(const unit* troop)
{
	std::vector<unit*>::iterator itor = std::find(field_troops_.begin(), field_troops_.end(), troop);
	if (itor != field_troops_.end()) {
		game_display* disp = game_display::get_singleton();
		if (disp) {
			disp->refresh_access_troops(troop->side() - 1, game_display::REFRESH_ERASE, const_cast<unit*>(troop));
		}

		field_troops_.erase(itor);
	}
}

void artifical::field_commoners_erase(const unit* commoner)
{
	std::vector<unit*>::iterator itor = std::find(field_commoners_.begin(), field_commoners_.end(), commoner);
	if (itor != field_commoners_.end()) {
		field_commoners_.erase(itor);
	}
}

void artifical::field_arts_erase(const artifical* art)
{
	std::vector<artifical*>::iterator itor = std::find(field_arts_.begin(), field_arts_.end(), art);
	if (itor != field_arts_.end()) {
		field_arts_.erase(itor);
	}
}

void artifical::hero_go_out(const hero& h)
{
	std::vector<hero*>::iterator find = std::find(fresh_heros_.begin(), fresh_heros_.end(), &h);
	VALIDATE(find != fresh_heros_.end(), "artifical::hero_go_out, cannot fnind going out hero!");

	game_display* disp = game_display::get_singleton();
	if (disp) {
		disp->refresh_access_heros(side_ - 1, game_display::REFRESH_ERASE, *find);
	}
	fresh_heros_.erase(find);
}

void artifical::fresh_into(hero& h)
{
	if (h.side_ != side_ - 1 && h.has_nomal_noble()) {
		teams_[h.side_].appoint_noble(h, HEROS_NO_NOBLE, true);
	}
	if (h.city_ != cityno_ && h.official_ == hero_official_mayor) {
		artifical* from = units_.city_from_cityno(h.city_);
		from->select_mayor(&hero_invalid);
	}
	h.status_ = hero_status_idle;
	h.city_ = cityno_;
	h.side_ = side_ - 1;
	fresh_heros_.push_back(&h);

	game_display* disp = game_display::get_singleton();
	if (disp) {
		disp->refresh_access_heros(side_ - 1, game_display::REFRESH_INSERT, &h);
	}
}

void artifical::fresh_into(const unit* troop)
{
	fresh_into(troop->master());
	
	if (troop->second().valid()) {
		fresh_into(troop->second());
	}
	if (troop->third().valid()) {
		fresh_into(troop->third());
	}
}

void artifical::finish_into(hero& h, uint8_t status)
{
	if (h.side_ != side_ - 1 && h.has_nomal_noble()) {
		teams_[h.side_].appoint_noble(h, HEROS_NO_NOBLE, true);
	}
	h.status_ = status;
	h.city_ = cityno_;
	h.side_ = side_ - 1;
	finish_heros_.push_back(&h);

	game_display* disp = game_display::get_singleton();
	if (disp) {
		disp->refresh_access_heros(side_ - 1, game_display::REFRESH_INSERT, &h);
	}
}

void artifical::finish_2_fresh()
{
	game_display* disp = game_display::get_singleton();
	for (std::vector<hero*>::iterator it = finish_heros_.begin(); it != finish_heros_.end(); ++ it) {
		hero& h = **it;
		h.status_ = hero_status_idle;
		fresh_heros_.push_back(&h);
		if (disp) {
			disp->refresh_access_heros(side_ - 1, game_display::REFRESH_ENABLE, &h);
		}
	}
	finish_heros_.clear();
}

void artifical::wander_into(const unit* troop, bool dialog)
{
	wander_into(troop->master(), dialog);
	if (troop->second().valid()) {
		wander_into(troop->second(), dialog);
	}
	if (troop->third().valid()) {
		wander_into(troop->third(), dialog);
	}
}

void artifical::wander_into(hero& h, bool dialog)
{
	if (h.official_ == hero_official_mayor) {
		artifical* from = units_.city_from_cityno(h.city_);
		from->select_mayor(&hero_invalid);
	}
	if (h.has_nomal_noble()) {
		teams_[h.side_].appoint_noble(h, HEROS_NO_NOBLE, dialog);
	}

	h.status_ = hero_status_wander;
	h.city_ = cityno_;
	h.side_ = HEROS_INVALID_SIDE;
	h.float_catalog_ = ftofxp8(h.base_catalog_);
	wander_heros_.push_back(&h);

	if (dialog) {
		std::string message = _("Go here, Can live the life I would like to?");
		game_events::show_hero_message(&h, this, message, game_events::INCIDENT_ENTER);
	}
	map_location loc2(MAGIC_HERO, h.number_);
	game_events::fire("post_wander", loc_, loc2);
}

void artifical::move_into(hero& h)
{
	if (h.official_ == hero_official_mayor) {
		artifical* from = units_.city_from_cityno(h.city_);
		from->select_mayor(&hero_invalid);
	}
	finish_into(h, hero_status_moving);
}

void insert_level_special(std::map<int, std::vector<const unit_type*> >& candidate, const unit_type* from, int special)
{
	std::map<int, std::vector<const unit_type*> >::iterator find;
	if (from->especial() != NO_ESPECIAL) {
		find = candidate.find(from->level());
		if (find == candidate.end()) {
			std::vector<const unit_type*> v;
			v.push_back(from);
			candidate.insert(std::make_pair(from->level(), v));
		} else if (std::find(find->second.begin(), find->second.end(), from) == find->second.end()) {
			find->second.push_back(from);
		}
	}
	std::vector<std::string> advances_to = from->advances_to(special);
	for (std::vector<std::string>::const_iterator it = advances_to.begin(); it != advances_to.end(); ++ it) {
		const unit_type* ut = unit_types.find(*it);
		if (ut->especial() != NO_ESPECIAL) {
			find = candidate.find(ut->level());
			if (find == candidate.end()) {
				std::vector<const unit_type*> v;
				v.push_back(ut);
				candidate.insert(std::make_pair(ut->level(), v));
			} else if (std::find(find->second.begin(), find->second.end(), ut) == find->second.end()) {
				find->second.push_back(ut);
			}
		}
		insert_level_special(candidate, ut, special);
	}

}

void insert_from_internal(std::set<const unit_type*>& from, const unit_type* to)
{
	std::vector<std::string> f = to->advances_from();
	for (std::vector<std::string>::const_iterator it = f.begin(); it != f.end(); ++ it) {
		const unit_type* ut = unit_types.find(*it);
		if (from.find(ut) == from.end()) {
			from.insert(ut);
		}
		insert_from_internal(from, ut);
	}
}

bool exist_from_internal(const unit_type* to, std::set<const unit_type*>& from)
{
	std::vector<std::string> f = to->advances_from();
	for (std::vector<std::string>::const_iterator it = f.begin(); it != f.end(); ++ it) {
		const unit_type* ut = unit_types.find(*it);
		if (from.find(ut) != from.end()) {
			return true;
		}
		if (exist_from_internal(ut, from)) {
			return true;
		}
	}
	return false;
}

void artifical::change_to_special_unit(game_display& disp, int attacker_side, int random)
{
	std::vector<unit*> desire;
	const map_location* tiles = adjacent_;
	size_t adjance_size = adjacent_size_;
	for (int step = 0; step < 2; step ++) {
		if (step == 1) {
			tiles = adjacent_2_;
			adjance_size = adjacent_size_2_;
		}
		for (size_t adj = 0; adj < adjance_size; adj ++) {
			const map_location& loc = tiles[adj];
			unit_map::iterator find = units_.find(loc);
			if (find.valid()) {
				unit& u = *find;
				if (!u.is_artifical() && u.side() == attacker_side && u.packee_type()->especial() == NO_ESPECIAL) {
					desire.push_back(&u);
				}
			}
		}
	}
	if (desire.empty()) {
		return;
	}

	const std::vector<const unit_type*>& r = recruits(-1);
	std::map<int, std::vector<const unit_type*> > special;
	for (std::vector<const unit_type*>::const_iterator it = r.begin(); it != r.end(); ++ it) {
		const unit_type* ut = *it;
		insert_level_special(special, ut, especial_);
	}
	if (special.empty()) {
		return;
	}

	for (std::vector<unit*>::iterator it = desire.begin(); it != desire.end(); ++ it) {
		unit& u = **it;
		std::map<int, std::vector<const unit_type*> >::iterator find = special.find(u.level());
		if (find == special.end()) {
			continue;
		}
		std::set<const unit_type*> from;
		insert_from_internal(from, u.packee_type());
		for (std::vector<const unit_type*>::const_iterator it2 = find->second.begin(); it2 != find->second.end(); ++ it2) {
			const unit_type* ut = *it2;
			if (ut->arms() == u.packee_type()->arms() && exist_from_internal(ut, from)) {
				u.change_to(disp, ut);
			}
		}
	}
}

int belong_max_loyalty_diff	= 35;

void artifical::decrease_level()
{
	modifications_.clear();
	const std::vector<std::string> from = unit_type_->advances_from();
	if (from.empty()) {
		adjust();
	} else {
		// reference to get_advanced_unit2(...)
		const unit_type* new_type = unit_types.find(from[0]);
		advance_to(new_type);
		calculate_5fields();
		modify_according_to_hero(false);
	}
	hit_points_ = std::max(max_hit_points_ / 2, 1);
	experience_ = 0;
}

artifical* find_shortest_city(const std::vector<artifical*>& holden, artifical& me)
{
	artifical* ret = NULL;
	size_t min_distance = -1;
	const map_location& center = me.get_location();
	for (std::vector<artifical*>::const_iterator it = holden.begin(); it != holden.end(); ++ it) {
		if (*it == &me) {
			continue;
		}
		artifical& city = **it;
		size_t distance = -1;
		distance = distance_between(center, city.get_location());

		if (distance < min_distance) {
			min_distance = distance;
			ret = &city;

		} else if (distance == min_distance && city.cityno() < me.cityno()) {
			// i think holden maybe not sync runtime with replay.
			// it is necessary stonger estimate, so, using cityno.
			// when distance is same, less cityno more.
			min_distance = distance;
			ret = &city;
		}
	}
	return ret;
}

void place_troop_between_cities(std::vector<team>& teams, unit_map& units, gamemap& gmap, artifical& from, artifical& to, int distance)
{
	const map_location& loc_from = from.get_location();
	const map_location& loc_to = to.get_location();
	map_location at = loc_from;

	const pathfind::emergency_path_calculator calc(*unit_map::scout_unit_, gmap);
	double stop_at = 10000.0;
	//allowed teleports
	std::set<map_location> allowed_teleports;
	// NOTICE! although start and end are same, but rank filp, result maybe differenrt! so use cityno to make sure same.
	pathfind::plain_route route = a_star_search(loc_from, loc_to, stop_at, &calc, gmap.w(), gmap.h(), &allowed_teleports);
	if (!route.steps.empty()) {
		if ((int)route.steps.size() > distance) {
			at = route.steps[distance];
		} else {
			at = route.steps.back();
		}
	} else {
		if (loc_to.x > loc_from.x) {
			at.x = loc_from.x + distance;
			if (at.x > loc_to.x) {
				at.x = loc_to.x;
			}
		} else if (loc_to.x < loc_from.x) {
			at.x = loc_from.x - distance;
			if (at.x < loc_to.x) {
				at.x = loc_to.x;
			}
		}
		if (loc_to.y > loc_from.y) {
			at.y = loc_from.y + distance;
			if (at.y > loc_to.y) {
				at.y = loc_to.y;
			}
		} else if (loc_to.y < loc_from.y) {
			at.y = loc_from.y - distance;
			if (at.y < loc_to.y) {
				at.y = loc_to.y;
			}
		}
	}

	unit& u = *to.reside_troops().back();
	at = pathfind::find_vacant_tile(gmap, teams, units, at, &u);
	do_direct_expedite(teams, units, to, to.reside_troops().size() - 1, at, false);
}

bool tow_city_map(const std::vector<team>& teams)
{
	size_t cities = 0;
	for (std::vector<team>::const_iterator it = teams.begin(); it != teams.end(); ++ it) {
		const team& t = *it;
		cities += t.holden_cities().size();
	}
	return cities <= 2;
}

float ea_damage_ratio = 0.5;

// fallen
// @attacker: troop that captures this artifical.
void artifical::fallen(int a_side, unit* attacker)
{
	std::stringstream str;
	str << "artifical::fallen, invalid a_side: " << a_side;
	VALIDATE(a_side > 0 && a_side <= (int)teams_.size(), str.str()); 

	play_controller& controller = *resources::controller;
	game_display& disp = *resources::screen;
	team& defender_team = teams_[side_ - 1];
	team& attacker_team = teams_[a_side - 1];
	std::vector<artifical*> defender_citys = defender_team.holden_cities();
	hero* attacker_leader = attacker_team.leader();
	hero& defender_leader = *defender_team.leader();
	bool original_is_capital = is_capital(teams_);
	artifical* join_to_city;
	std::string message;
	utils::string_map symbols;
	std::stringstream strstr;
	int random;
	bool force_wander;

	bool fallen_to_unstage = controller.fallen_to_unstage() || defender_leader.get_flag(hero_flag_npc);
	bool attacker_team_is_rpg = attacker_leader == rpg::h;

	// 查找是否有和该城同阵营的城
	artifical* city_same_side = find_shortest_city(defender_citys, *this);
	
	// 1.被攻占城市改为攻占者阵营(首先改掉，底下城内/城外部队会四散，可能会散到这个城市，而这个城市可能是该势力最后一个城市，导致无效阵营号）
	side_ = a_side;

	// 得到当前存在的城市列表
	std::vector<artifical*> citys;
	for (size_t i = 0; i < teams_.size(); i ++) {
		std::vector<artifical*>& side_citys = teams_[i].holden_cities();
		citys.insert(citys.end(), side_citys.begin(), side_citys.end());
	}
	
	// 处理城内部队: 如果还存在同阵营城市, 全部迁往该城市, 否则四散
	std::vector<unit*> cache_reside_troops = reside_troops_;
	reside_troops_.clear();
	for (std::vector<unit*>::iterator itor = cache_reside_troops.begin(); itor != cache_reside_troops.end() ; ++ itor) {
		// 强制置为“完”状态
		unit* current_troop = *itor;
		
		random = get_random();
		join_to_city = NULL;

		if (current_troop->packee_type()->require() == unit_type::REQUIRE_LEADER) {
			force_wander = true;
		} else {
			force_wander = false;
		}

		if (current_troop->is_soldier()) {
			delete current_troop;
			continue;
		} else if (city_same_side) {
			city_same_side->troop_come_into(current_troop, -1);
			// city maybe fallen in "one" turn again, need recove some param of reside troop, at least attacks_left_.
			current_troop->end_turn(false);
			place_troop_between_cities(teams_, units_, map_, *this, *city_same_side, 5);
		} else if (fallen_to_unstage) {
			current_troop->to_unstage();
		} else if (!force_wander && a_side != team::empty_side && (random % belong_max_loyalty_diff + current_troop->master().base_loyalty(*attacker_leader)) >= HERO_MAX_LOYALTY) {
			if (!attacker_team_is_rpg || !current_troop->unstage_if_member(*attacker_leader)) {
				troop_come_into(current_troop, -1);
				join_to_city = this;
			}
		} else {
			artifical* cobj = citys[random % citys.size()];
			hero& city_team_leader = *teams_[cobj->side() - 1].leader();
			bool city_team_is_rpg = &city_team_leader == rpg::h;

			if (cobj->side() != attacker_team.side()) {
				current_troop->goto_hate_if(attacker_team, defender_leader, random);
			}

			if (force_wander || cobj->side() == team::empty_side || random % 2) {
				cobj->wander_into(current_troop, teams_[cobj->side() - 1].is_human()? true: false);
			} else if (!city_team_is_rpg || !current_troop->unstage_if_member(city_team_leader)) {
				cobj->troop_come_into(current_troop, -1);
				join_to_city = cobj;
			}
		}
		if (join_to_city) {
			if (current_troop->second().valid()) {
				strstr.str("");
				strstr << tintegrate::generate_format(current_troop->master().name(), tintegrate::hero_color) << ", " << tintegrate::generate_format(current_troop->second().name(), tintegrate::hero_color);
				if (current_troop->third().valid()) {
					strstr << ", " << tintegrate::generate_format(current_troop->third().name(), tintegrate::hero_color);
				}
				symbols["first"] = strstr.str();
				message = vgettext("Find wise leader, $first would like to lead troop join in.", symbols);
			} else {
				message = _("Find wise leader, I would like to lead troop join in.");
			}
			join_anim(&current_troop->master(), join_to_city, message);
		}
		if (!city_same_side && !join_to_city) {
			delete current_troop;
		}
	}
	// 处理城内(未)(完)武将: 如果还存在同阵营城市, 全部迁往该城市（武将置为完）, 否则四散
	std::vector<hero*> cache_heros = fresh_heros_;
	std::copy(finish_heros_.begin(), finish_heros_.end(), std::back_inserter(cache_heros));
	fresh_heros_.clear();
	finish_heros_.clear();
	for (std::vector<hero*>::iterator itor = cache_heros.begin(); itor != cache_heros.end() ; ++ itor) {
		hero* h = *itor;

		random = get_random();
		join_to_city = NULL;

		// soldier hero can not be hero, so don't carray those.

		if (city_same_side && (h == rpg::h || !h->get_flag(hero_flag_roam))) {
			city_same_side->move_into(*h);
		} else if (fallen_to_unstage) {
			h->to_unstage();
		} else if (a_side != team::empty_side && (random % belong_max_loyalty_diff + h->base_loyalty(*attacker_leader)) >= HERO_MAX_LOYALTY) {
			if (!attacker_team_is_rpg || !runtime_groups::exist_member(*h, *attacker_leader)) {
				move_into(*h);
				join_to_city = this;
			} else {
				h->to_unstage(hero::UNSTAGE_GROUP);
			}
		} else {
			artifical* cobj = citys[random % citys.size()];
			hero& city_team_leader = *teams_[cobj->side() - 1].leader();
			bool city_team_is_rpg = &city_team_leader == rpg::h;

			if (cobj->side() == team::empty_side || random % 2) {
				cobj->wander_into(*h, teams_[cobj->side() - 1].is_human()? true: false);
			} else if (!city_team_is_rpg || !runtime_groups::exist_member(*h, city_team_leader)) {
				cobj->move_into(*h);
				join_to_city = cobj;
			} else {
				h->to_unstage(hero::UNSTAGE_GROUP);
			}

			if (h->status_ == hero_status_wander) {
				h->goto_hate_if(attacker_team, defender_leader, random);
			}
		}
		if (join_to_city) {
			message = _("Find wise leader, let me join in.");
			join_anim(h, join_to_city, message);
		}
	}
	// 处理城外部队: 如果还存在同阵营城市, 全部迁置为属于该城市, 否则随机分配到其它城市
	std::vector<unit*> cache_field_troops = field_troops_;
	std::set<unit*> lobbyisted_troops;
	for (std::vector<unit*>::iterator itor = cache_field_troops.begin(); itor != cache_field_troops.end(); ++itor) {
		// 强制置为“完”状态
		unit* current_troop = *itor;

		current_troop->remove_from_provoke_cache();
		current_troop->remove_from_slot_cache();
		current_troop->set_goto(map_location());
		current_troop->set_state(ustate_tag::EXPEDITED, false);

		random = get_random();
		join_to_city = NULL;

		if (current_troop->packee_type()->require() == unit_type::REQUIRE_LEADER) {
			force_wander = true;
		} else {
			force_wander = false;
		}

		if (current_troop->is_soldier()) {
			units_.erase(current_troop);
			continue;
			
		} else if (city_same_side) {
			city_same_side->unit_belong_to(current_troop, false);

		} else {
			if (!force_wander) {
				for (size_t i = 0; i < current_troop->adjacent_size_; i ++) {
					unit_map::iterator u_itor = units_.find(current_troop->adjacent_[i]);
					if (u_itor.valid() && unit_feature_val2(*u_itor, hero_feature_lobbyist) && u_itor->side() != current_troop->side()) {
					// should use below statement
					// if (u_itor.valid() && unit_feature_val2(*u_itor, hero_feature_lobbyist) && u_itor->side() == a_side) {
						if (lobbyisted_troops.find(&*u_itor) != lobbyisted_troops.end()) {
							// a lobbyist only face down one troop
							continue;
						}
						artifical* cobj = units_.city_from_cityno(u_itor->cityno());
						lobbyisted_troops.insert(&*u_itor);

						join_to_city = cobj;
						break;
					}
				}
			}
			if (join_to_city) {
				hero& city_team_leader = *teams_[join_to_city->side() - 1].leader();
				bool city_team_is_rpg = &city_team_leader == rpg::h;

				if (!city_team_is_rpg || !current_troop->unstage_if_member(city_team_leader)) {
					join_to_city->unit_belong_to(current_troop);
				} else {
					join_to_city = NULL;
				}

			} else if (fallen_to_unstage) {
				current_troop->to_unstage();

			} else if (!force_wander && a_side != team::empty_side && (random % belong_max_loyalty_diff + current_troop->master().base_loyalty(*attacker_leader)) >= HERO_MAX_LOYALTY) {
				if (!attacker_team_is_rpg || !current_troop->unstage_if_member(*attacker_leader)) {
					unit_belong_to(current_troop);
					join_to_city = this;
				}

			} else {
				// random a city.
				artifical* cobj = citys[random % citys.size()];
				hero& city_tream_leader = *teams_[cobj->side() - 1].leader();
				bool city_team_is_rpg = &city_tream_leader == rpg::h;

				if (cobj->side() != attacker_team.side()) {
					current_troop->goto_hate_if(attacker_team, defender_leader, random);
				}

				if (force_wander || cobj->side() == team::empty_side || random % 2) {
					cobj->wander_into(current_troop, teams_[cobj->side() - 1].is_human()? true: false);
				} else if (!city_team_is_rpg || !current_troop->unstage_if_member(city_tream_leader)) {
					cobj->unit_belong_to(current_troop);
					join_to_city = cobj;

				}
			}
		}
		if (join_to_city) {
			if (current_troop->second().valid()) {
				strstr.str("");
				strstr << tintegrate::generate_format(current_troop->master().name(), tintegrate::hero_color) << ", " << tintegrate::generate_format(current_troop->second().name(), tintegrate::hero_color);
				if (current_troop->third().valid()) {
					strstr << ", " << tintegrate::generate_format(current_troop->third().name(), tintegrate::hero_color);
				}
				symbols["first"] = strstr.str();
				message = vgettext("Find wise leader, $first would like to lead troop join in.", symbols);
			} else {
				message = _("Find wise leader, I would like to lead troop join in.");
			}
			join_anim(&current_troop->master(), join_to_city, message);
		}
		if (!city_same_side && !join_to_city) {
			units_.erase(current_troop);
		}
	}

	std::vector<unit*> cache_analyzing = field_commoners_;
	for (std::vector<unit*>::iterator itor = cache_analyzing.begin(); itor != cache_analyzing.end(); ++ itor) {
		// 强制置为“完”状态
		unit* current_troop = *itor;

		if (city_same_side && current_troop->transport()) {
			city_same_side->unit_belong_to(current_troop, false);

		} else {
			current_troop->set_goto(map_location());

			random = get_random();
			join_to_city = NULL;

			units_.erase(current_troop);
		}
	}

	// 处理城外建筑物: 如果还存在同阵营城市, 全部迁置为属于该城市, 否则随机分配到其它城市
	std::vector<artifical*> cache_field_arts = field_arts_;
	for (std::vector<artifical*>::iterator itor = cache_field_arts.begin(); itor != cache_field_arts.end(); ++itor) {
		artifical* current_art = *itor;
		std::map<const map_location, int>::const_iterator it2 = unit_map::economy_areas_.find(current_art->get_location());

		if (current_art->wall() || current_art->type() == unit_types.find_keep()) {
			units_.erase(current_art);

		} else if (it2 != unit_map::economy_areas_.end()) {
			unit_belong_to(current_art);
			if (current_art->get_state(ustate_tag::BUILDING)) {
				// change side attribute maybe make hp full, it will result builing error.
				// damage make right.
				current_art->heal(-1 * current_art->max_hitpoints() * ea_damage_ratio);
			}

		} else if (city_same_side) {
			city_same_side->unit_belong_to(current_art);

		} else if (current_art->fort()) {
			unit_belong_to2(teams_, units_, HEROS_ROAM_CITY, current_art, false, false);

		} else {
			// 随机取一个城市
			artifical* cobj = citys[get_random() % citys.size()];
			if (cobj->side() != team::empty_side) {
				cobj->unit_belong_to(current_art);

			} else {
				units_.erase(current_art);
			}
		}
	}

	std::vector<unit*> cache_reside_commoners = reside_commoners_;
	reside_commoners_.clear();
	for (std::vector<unit*>::iterator itor = cache_reside_commoners.begin(); itor != cache_reside_commoners.end() ; ++ itor) {
		// 强制置为“完”状态
		unit* current_troop = *itor;
		
		random = get_random();
		join_to_city = NULL;

		delete current_troop;
	}

	// 5. change city
	defender_team.erase_city(this);
	attacker_team.add_city(this);
	
	if (!attacker_team.is_human()) {
		symbols["first"] = tintegrate::generate_format(attacker_team.name(), tintegrate::hero_color);
		symbols["second"] = tintegrate::generate_format(name(), tintegrate::object_color);
		if (attacker_team.side() != team::empty_side) {
			game_events::show_hero_message(&heros_[hero::number_scout], NULL, vgettext("$first occupy $second.", symbols), game_events::INCIDENT_FALLEN);
		} else {
			// in order to avoid overmany tip, close this.
			// game_events::show_hero_message(&heros_[hero::number_scout], NULL, vgettext("$second is out of control, occupied by $first.", symbols), game_events::INCIDENT_FALLEN);
		}
	}

	if (this_is_city()) {
		if (defender_team.side() != team::empty_side) {
			if (defender_team.holden_cities().empty()) {
				// erase all strategy that this side holded
				defender_team.erase_strategies(false);

				// erase all ally that is this team
				for (size_t i = 0; i < teams_.size(); i ++) {
					if (i != defender_team.side() - 1) {
						teams_[i].erase_ally(defender_team.side());
					}
				}

				if (!defender_team.is_human()) {
					defender_team.change_controller(controller_tag::EMPTY);
				}

				symbols["first"] = tintegrate::generate_format(defender_team.name(), tintegrate::hero_color);
				game_events::show_hero_message(&heros_[hero::number_scout], NULL, vgettext("$first is defeated.", symbols), game_events::INCIDENT_DEFEAT);

			} else if (original_is_capital) {
				defender_team.set_capital(units_.city_from_cityno(defender_leader.city_));
			}
		}
		
		change_to_special_unit(disp, a_side, get_random());
		bool show_message = !tow_city_map(teams_);
		attacker_team.select_leader_noble(show_message);
	}

	// 4. attacker come into city
	if (attacker) {
		int original = attacker->cityno();
		troop_come_into2(attacker);

		// if this is roam troop, other troop belong to this
		if (original == HEROS_ROAM_CITY) {
			std::pair<unit**, size_t> fields = attacker_team.field_troop();
			unit** troops = fields.first;
			size_t troops_vsize = fields.second;
			std::vector<unit*> cache;
			for (size_t i = 0; i < troops_vsize; i ++) {
				cache.push_back(troops[i]);
			}
			// unit_belong_to may change order in fields.first.
			for (std::vector<unit*>::const_iterator it = cache.begin(); it != cache.end(); ++ it) {
				unit& u = **it;
				if (!u.is_artifical()) {
					VALIDATE(u.cityno() == HEROS_ROAM_CITY, "artifical::fallen, troop's city isn't roam!");
					unit_belong_to(&u, false);
				}
			}

			attacker_team.set_capital(this);
		}

	}

	// side is changed, set master_'s side_. city_ is unthouched. Only when is city. don't set when artifical.
	if (this_is_city()) {
		master_->side_ = side_ - 1;
		if (second_->valid()) {
			second_->side_ = side_ - 1;
		}
		if (third_->valid()) {
			third_->side_ = side_ - 1;
		}
	}

	// leader is changed, recalculate fields
	decrease_level();

	if (decree_) {
		clear_decree();
	}
	if (soldiers_) {
		soldiers_ = 0;
	}
	set_state(ustate_tag::DEPUTE, false);
	if (defender_team.all_city_deputed(NULL)) {
		defender_team.set_all_city_deputed(disp, false);
	}

	// get one card random
	if (!controller.is_recovering(a_side)) {
/*
		为阻止玩家靠攻城来刷卡, 先取消攻城得卡/失城掉设置, 等将来对攻城刷卡有惩罚措施后再打开
		get_random_card(attacker_team, *resources::screen, units_, heros_);
		erase_random_card(defender_team, *resources::screen, units_, heros_);
*/
	}

	// erase all strategy that target is this.
	if (this_is_city()) {
		for (size_t i = 0; i < teams_.size(); i ++) {
			teams_[i].erase_strategy(cityno_);
		}

		if (rpg::stratum != hero_stratum_leader && attacker && attacker->human()) {
			units_.ai_capture_aggressed(*this, side_, !controller.is_recovering(a_side));
		}
	}

	// city compair ticks's parameter maybe change, for example capitial, side
	disp.resort_access_troops(*this);

	// fallen maybe result to chnage road owner.
	disp.invalidate_all();

	return;
}

void artifical::fort_fallen(int a_side)
{
	std::stringstream str;
	str << "artifical::fort_fallen, invalid a_side: " << a_side;
	VALIDATE(a_side > 0 && a_side <= (int)teams_.size(), str.str()); 

	team& attacker_team = teams_[a_side - 1];
	int to_cityno = HEROS_ROAM_CITY;

	// change cityno_
	std::vector<artifical*>& holden = attacker_team.holden_cities();
	if (attacker_team.side() != team::empty_side && !holden.empty()) {
		std::vector<artifical*>::iterator it = holden.begin();
		std::vector<artifical*>::iterator choice = it;
		artifical& current = **it;

		size_t min_distance = distance_between(loc_, current.get_location());
		for (++ it; it != holden.end(); ++ it) {
			artifical& current = **it;
			size_t distance = distance_between(loc_, current.get_location());
			if (distance < min_distance) {
				min_distance = distance;
				choice = it;
			}
		}
		to_cityno = (*choice)->cityno();
	}

	if (cityno_ != to_cityno) {
		unit_belong_to2(teams_, units_, to_cityno, this, false, false);
	}
	
	decrease_level();
	food_ /= 10;
}

bool artifical::independence_vote(const artifical* aggressing) const
{
	int human_resides, ai_resides, human_fields, ai_fields;

	// reside troop
	human_resides = ai_resides = 0;
	for (std::vector<unit*>::const_iterator it = reside_troops_.begin(); it != reside_troops_.end(); ++ it) {
		if ((*it)->human()) {
			human_resides ++;
		} else {
			ai_resides ++;
		}
	}

	// field troop
	human_fields = ai_fields = 0;
	for (std::vector<unit*>::const_iterator it = field_troops_.begin(); it != field_troops_.end(); ++ it) {
		if ((*it)->human()) {
			human_fields ++;
		} else {
			ai_fields ++;
		}
	}

	// vote
	if (cityno_ == rpg::h->city_) {
		return true;
	} else if (this == aggressing) {
		return false;
	} else if (human_resides > ai_resides) {
		return true;
	} else if (human_resides == ai_resides && human_fields > ai_fields) {
		return true;
	}
	return false;
}

void artifical::independence(bool independenced, team& to_team, artifical* rpg_city, team& from_team, artifical* aggressing, hero* from_leader, unit* from_leader_unit)
{
	int to_side = to_team.side();

	if (independenced) {
		set_side(to_side);
		select_mayor(&hero_invalid);
	}
	
	// process troop
	for (std::vector<unit*>::iterator it = reside_troops_.begin(); it != reside_troops_.end(); ) {
		unit* current_troop = *it;
		if (independenced) {
			if (current_troop->is_soldier()) {
				delete current_troop;
				it = reside_troops_.erase(it);
			} else if ((current_troop != from_leader_unit && current_troop->human()) || !aggressing) {
				if (!current_troop->human()) {
					current_troop->set_human(true);
				}
				current_troop->set_side(to_side);
				current_troop->adjust();
				
				++ it;
			} else {
				if (current_troop->human()) {
					current_troop->set_human(false);
				}
				aggressing->troop_come_into(current_troop, -1);
				it = reside_troops_.erase(it);
			}
		} else {
			if (current_troop != from_leader_unit && current_troop->human()) {
				rpg_city->troop_come_into(current_troop, -1);
				it = reside_troops_.erase(it);
			} else {
				if (current_troop->human()) {
					current_troop->set_human(false);
				}
				++ it;
			}
		}
	}

	for (std::vector<unit*>::iterator it = reside_commoners_.begin(); it != reside_commoners_.end(); ) {
		unit* current_troop = *it;
		if (independenced) {
			current_troop->set_side(to_side);
			current_troop->adjust();
			++ it;
		} else {
			++ it;
		}
	}

	std::vector<hero*> cache_heros = fresh_heros_;
	std::copy(finish_heros_.begin(), finish_heros_.end(), std::back_inserter(cache_heros));
	fresh_heros_.clear();
	finish_heros_.clear();
	for (std::vector<hero*>::iterator itor = cache_heros.begin(); itor != cache_heros.end() ; ++ itor) {
		hero* h = *itor;
		if (independenced) {
			if (h != from_leader || !aggressing) {
				h->side_ = to_side - 1;
				h->status_ = hero_status_idle;
				h->set_loyalty2(*teams_[to_side - 1].leader(), game_config::wander_loyalty_threshold + 1);
				fresh_heros_.push_back(h);
			} else {
				aggressing->move_into(*h);
			}
		} else {
			h->status_ = hero_status_idle;
			fresh_heros_.push_back(h);
		}
	}

	std::vector<unit*> field_troops = field_troops_;
	// field_troops_.clear();
	for (std::vector<unit*>::iterator it = field_troops.begin(); it != field_troops.end();  ++ it) {
		unit* current_troop = *it;
		current_troop->remove_from_provoke_cache();
		current_troop->remove_from_slot_cache();
		if (independenced) {
			if (current_troop->is_soldier()) {
				units_.erase(current_troop);

			} else if ((current_troop != from_leader_unit && current_troop->human()) || !aggressing) {
				if (!current_troop->human()) {
					// current_troop->set_human(true);
					current_troop->set_goto(map_location());
				}
				unit_belong_to(current_troop);
			} else {
				if (current_troop->human()) {
					current_troop->set_human(false);
				}
				aggressing->unit_belong_to(current_troop);
			}
		} else {
			if (current_troop != from_leader_unit && current_troop->human()) {
				rpg_city->unit_belong_to(current_troop);
			} else {
				if (current_troop->human()) {
					current_troop->set_human(false);
				}
				unit_belong_to(current_troop);
			}
		}
	}

	std::vector<unit*> cache_analyzing = field_commoners_;
	// field_commoners_.clear();
	for (std::vector<unit*>::iterator it = cache_analyzing.begin(); it != cache_analyzing.end();  ++ it) {
		unit* current_troop = *it;
		if (independenced) {
			unit_belong_to(current_troop);
		} else {
			unit_belong_to(current_troop);
		}
	}

	if (independenced) {
		std::vector<artifical*> cache_field_arts = field_arts_;
		for (std::vector<artifical*>::iterator it = cache_field_arts.begin(); it != cache_field_arts.end(); ++ it) {
			artifical* current_art = *it;
			// if (current_art->wall() || current_art->type() == unit_types.find_keep()) {
			//	units_.erase(current_art);
			// }
			unit_belong_to(current_art);

			if (current_art->get_state(ustate_tag::BUILDING)) {
				// change side attribute maybe make hp full, it will result builing error.
				// damage make right.
				current_art->heal(-1 * current_art->max_hitpoints() * ea_damage_ratio);
			}
		}

		from_team.erase_city(this);
		to_team.add_city(this);

		// leader is changed, recalculate fields
		adjust();
	
		// erase all strategy that target is this.
		for (size_t i = 0; i < teams_.size(); i ++) {
			teams_[i].erase_strategy(this->cityno());
		}

		if (soldiers_) {
			soldiers_ = 0;
		}
	}

	// city compair ticks's parameter maybe change, for example capitial, side
	// disp.resort_access_troops(*this);
}

bool artifical::is_surrounded() const
{
	unit_map::const_iterator it;
	const team& city_team = teams_[side_ - 1];

	for (size_t i = 0; i < adjacent_size_; i ++) {
		it = units_.find(adjacent_[i]);
		if ((it == units_.end()) || !city_team.is_enemy(it->side())) {
			return false;
		}
	}
	return true;
}

bool artifical::is_front(bool must_enemy) const
{
	for (std::set<int>::const_iterator it = roaded_cities_.begin(); it != roaded_cities_.end(); ++ it) {
		int roaded_side = units_.city_from_cityno(*it)->side();
		if (roaded_side != team::empty_side && roaded_side != side_) {
			if (!must_enemy || teams_[side_ - 1].is_enemy(roaded_side)) {
				return true;
			}
		} 
	}
	return false;
}

std::vector<artifical*> artifical::adjacent_other_cities(bool must_enemy) const
{
	std::vector<artifical*> v;
	for (std::set<int>::const_iterator it = roaded_cities_.begin(); it != roaded_cities_.end(); ++ it) {
		artifical* roaded = units_.city_from_cityno(*it);
		int roaded_side = roaded->side();
		if (roaded_side != team::empty_side && roaded_side != side_) {
			if (!must_enemy || teams_[side_ - 1].is_enemy(roaded_side)) {
				v.push_back(roaded);
			}
		} 
	}
	return v;
}

void artifical::extract_heros_number()
{
	unit::extract_heros_number();

	fresh_heros_number_.clear();
	for (std::vector<hero*>::iterator itor = fresh_heros_.begin(); itor != fresh_heros_.end(); ++ itor) {
		fresh_heros_number_.push_back((*itor)->number_);
	}
	finish_heros_number_.clear();
	for (std::vector<hero*>::iterator itor = finish_heros_.begin(); itor != finish_heros_.end(); ++ itor) {
		finish_heros_number_.push_back((*itor)->number_);
	}
}

void artifical::recalculate_heros_pointer()
{
	unit::recalculate_heros_pointer();

	fresh_heros_.clear();
	for (std::vector<uint16_t>::iterator itor = fresh_heros_number_.begin(); itor != fresh_heros_number_.end(); ++ itor) {
		fresh_heros_.push_back(&heros_[*itor]);
	}
	finish_heros_.clear();
	for (std::vector<uint16_t>::iterator itor = finish_heros_number_.begin(); itor != finish_heros_number_.end(); ++ itor) {
		finish_heros_.push_back(&heros_[*itor]);
	}
}

bool compare_for_mayor(const hero* master, const hero* a, const hero* b, const hero* leader)
{
	if (b == rpg::h) {
		return true;
	}
	if (a == rpg::h) {
		return false;
	}
	if (!b->valid()) {
		return true;
	}
	if (!a->valid()) {
		return false;
	}
	if (b->official_ != HEROS_NO_OFFICIAL) {
		return true;
	}
	if (a->official_ != HEROS_NO_OFFICIAL) {
		return false;
	}
	int a_loyality = a->loyalty(*leader);
	int b_loyality = b->loyalty(*leader);
	if (a_loyality < game_config::move_loyalty_threshold || b_loyality < game_config::move_loyalty_threshold) {
		return a_loyality >= b_loyality;
	}
	return a->spirit_ + a->intellect_ > b->spirit_ + b->intellect_;
}

void show_mayor_message(hero_map& heros, hero& mayor, artifical& city, bool terminate)
{
	utils::string_map symbols;
	std::string message;

	int incident = game_events::INCIDENT_APPOINT;
	symbols["city"] = tintegrate::generate_format(city.name(), tintegrate::object_color);
	if (terminate) {
		if (&mayor == rpg::h) {
			message = vgettext("Your official of $city's mayor is terminated.", symbols);
		} else if (mayor.valid()) {
			symbols["hero"] = tintegrate::generate_format(mayor.name(), tintegrate::hero_color);
			message = vgettext("$hero official of $city's mayor is terminated.", symbols);
		}
		incident = game_events::INCIDENT_INVALID;
	} else {
		if (&mayor == rpg::h) {
			message = vgettext("Congratulate! You are appointed $city mayor.", symbols);
		} else if (mayor.valid()) {
			symbols["hero"] = tintegrate::generate_format(mayor.name(), tintegrate::hero_color);
			message = vgettext("Congratulate! $hero is appointed $city mayor.", symbols);
		}
		incident = game_events::INCIDENT_APPOINT;
	}
	if (!message.empty()) {
		game_events::show_hero_message(&heros[hero::number_civilian], &city, message, incident);
	}
}

// retval indicats whether mayor changed.
bool artifical::select_mayor(hero* commend, bool dialog)
{
	std::vector<team>& teams = *resources::teams;

	if (commend) {
		if (mayor_ == commend) {
			return false;
		}
		if (mayor_->valid()) {
			if (dialog) {
				show_mayor_message(heros_, *mayor_, *this, true);
			}
			mayor_->official_ = HEROS_NO_OFFICIAL;
		}
		if (mayor_ == rpg::h) {
			rpg::stratum = hero_stratum_citizen;
			teams[rpg::h->side_].rpg_changed();
		}
				
		mayor_ = commend;
		if (mayor_->valid()) {
			mayor_->official_ = hero_official_mayor;
			if (mayor_ == rpg::h && rpg::stratum != hero_stratum_mayor) {
				rpg::stratum = hero_stratum_mayor;
				teams[rpg::h->side_].rpg_changed();
			}
		}
		if (dialog) {
			show_mayor_message(heros_, *mayor_, *this, false);
		}
		calculate_exploiture();
		return true;
	}

	VALIDATE(!mayor_->valid(), std::string("select mayor(), ") + name() + " has existed mayor!");
	team& t = teams[side_ - 1];
	const hero* leader = t.leader();
	hero* max = &hero_invalid;
	for (std::vector<hero*>::iterator it = fresh_heros_.begin(); it != fresh_heros_.end(); ++ it) {
		hero* h = *it;
		if (compare_for_mayor(master_, h, max, leader)) {
			max = h;
		}
	}
	for (std::vector<hero*>::iterator it = finish_heros_.begin(); it != finish_heros_.end(); ++ it) {
		hero* h = *it;
		if (compare_for_mayor(master_, h, max, leader)) {
			max = h;
		}
	}
	for (std::vector<unit*>::iterator it = reside_troops_.begin(); it != reside_troops_.end(); ++ it) {
		unit& u = **it;
		if (!u.hdata_variable()) {
			continue;
		}
		hero* h = &u.master();
		if (compare_for_mayor(master_, h, max, leader)) {
			max = h;
		}
		if (u.second().valid()) {
			h = &u.second();
			if (compare_for_mayor(master_, h, max, leader)) {
				max = h;
			}
		}
		if (u.third().valid()) {
			h = &u.third();
			if (compare_for_mayor(master_, h, max, leader)) {
				max = h;
			}
		}
	}
	for (std::vector<unit*>::iterator it = field_troops_.begin(); it != field_troops_.end(); ++ it) {
		unit& u = **it;
		if (!u.hdata_variable()) {
			continue;
		}
		hero* h = &u.master();
		if (compare_for_mayor(master_, h, max, leader)) {
			max = h;
		}
		if (u.second().valid()) {
			h = &u.second();
			if (compare_for_mayor(master_, h, max, leader)) {
				max = h;
			}
		}
		if (u.third().valid()) {
			h = &u.third();
			if (compare_for_mayor(master_, h, max, leader)) {
				max = h;
			}
		}
	}
	if (max->official_ == HEROS_NO_OFFICIAL && max != rpg::h) {
		mayor_ = max;
		mayor_->official_ = hero_official_mayor;

		if (dialog) {
			show_mayor_message(heros_, *mayor_, *this, false);
		}
		calculate_exploiture();
		return true;
	}
	return false;
}

void artifical::calculate_exploiture()
{
	commercial_exploiture_ = ::calculate_exploiture(*master_, *mayor_, department::commercial);
	technology_exploiture_ = ::calculate_exploiture(*master_, *mayor_, department::technology);
}

std::pair<bool, bool> artifical::calculate_feature() const
{
	// whether skill feature or not.
	bool guide_feature = false;
	bool skill_feature = false;
	std::set<int> v;
	if (master_->feature_ != HEROS_NO_FEATURE) {
		v.insert(master_->feature_);
	}
	if (mayor_->valid() && mayor_->feature_ != HEROS_NO_FEATURE) {
		v.insert(mayor_->feature_);
	}

	const complex_feature_map& complex_feature = unit_types.complex_feature();

	for (std::set<int>::const_iterator it = v.begin(); it != v.end(); ++ it) {
		int feature = *it;

		if (feature == hero_feature_guide) {
			guide_feature = true;
		}
		if (feature == hero_feature_skill) {
			skill_feature = true;
		}
		if (feature >= HEROS_BASE_FEATURE_COUNT) {
			complex_feature_map::const_iterator complex_it = complex_feature.find(feature);
			if (complex_it == complex_feature.end()) {
				continue;
			}
			for (std::vector<int>::const_iterator it2 = complex_it->second.begin(); it2 != complex_it->second.end(); ++ it2) {
				int single_feature = *it2;
				if (single_feature == hero_feature_guide) {
					guide_feature = true;
				}
				if (single_feature == hero_feature_skill) {
					skill_feature = true;
				}
			}
		}
	}
	return std::make_pair(guide_feature, skill_feature);
}

void artifical::active_exploiture()
{
	int markets, technologies, tactics;
	calculate_ea_tiles(markets, technologies, tactics);

	if (!markets && !technologies) {
		return;
	}
	std::pair<bool, bool> relative_feature = calculate_feature();
	int xp = 4;

	get_experience(increase_xp::exploiture_ublock(markets, technologies, business_speed_, technology_speed_, relative_feature.first, relative_feature.second), xp, true, false, false);
	if (mayor_->valid()) {
		unit* u = units_.find_unit(*mayor_);
		if (!u->is_artifical()) {
			bool master = false;
			bool second = false;
			bool third = false;
			if (mayor_ == &u->master()) {
				master = true;
			} else if (mayor_ == &u->second()) {
				second = true;
			} else if (mayor_ == &u->third()) {
				third = true;
			} else {
				VALIDATE(false, "Cannot find mayor(" + mayor_->name() + ") in " + name() + "'s troops."); 
			}
			u->get_experience(increase_xp::exploiture_ublock(markets, technologies, business_speed_, technology_speed_, relative_feature.first, relative_feature.second), xp, master, second, third);
		} else {
			mayor_->get_xp(increase_xp::exploiture_hblock(markets, technologies, business_speed_, technology_speed_, relative_feature.first, relative_feature.second, xp));
		}
	}
	calculate_exploiture();
}

void artifical::calculate_ea_tiles(std::vector<const map_location*>& ea_vacants, int& markets, int& technologies, int& tactics) const
{
	ea_vacants.clear();
	markets = 0;
	technologies = 0;
	tactics = 0;
	for (std::vector<map_location>::const_iterator ea = economy_area_.begin(); ea != economy_area_.end(); ++ ea) {
		unit_map::const_iterator find = units_.find(*ea);
		if (!find.valid()) {
			ea_vacants.push_back(&*ea);
		} else if (find->type()->master() == hero::number_market) {
			markets ++;
		} else if (find->type()->master() == hero::number_technology) {
			technologies ++;
		} else if (find->type()->master() == hero::number_tactic) {
			tactics ++;
		}
	}
}

void artifical::calculate_ea_tiles(int& markets, int& technologies, int& tactics) const
{
	markets = 0;
	technologies = 0;
	tactics = 0;
	for (std::vector<map_location>::const_iterator ea = economy_area_.begin(); ea != economy_area_.end(); ++ ea) {
		unit_map::const_iterator find = units_.find(*ea);
		if (!find.valid()) {
			continue;
		} else if (find->type()->master() == hero::number_market) {
			markets ++;
		} else if (find->type()->master() == hero::number_technology) {
			technologies ++;
		} else if (find->type()->master() == hero::number_tactic) {
			tactics ++;
		}
	}
}

artifical* artifical::tactic_on_ea() const
{
	for (std::vector<map_location>::const_iterator ea = economy_area_.begin(); ea != economy_area_.end(); ++ ea) {
		unit* find = units_.find_unit(*ea);
		if (find && find->type()->master() == hero::number_tactic) {
			return unit_2_artifical(find);
		}
	}
	return NULL;
}

int tactic_requrie_min_adjacents = 4;
const unit_type* artifical::next_ea_utype(std::vector<const map_location*>& ea_vacants, const std::set<const unit_type*>& can_build_ea) const
{
	int markets, technologies, tactics;
	calculate_ea_tiles(ea_vacants, markets, technologies, tactics);
	if (ea_vacants.empty()) {
		return NULL;
	}
	if (can_build_ea.empty()) {
		return NULL;
	} else if (can_build_ea.size() == 1) {
		return *can_build_ea.begin();
	}

	bool can_market = can_build_ea.find(unit_types.find_market()) != can_build_ea.end();
	bool can_technology = can_build_ea.find(unit_types.find_technology()) != can_build_ea.end();
	bool can_tactic = can_build_ea.find(unit_types.find_tactic()) != can_build_ea.end();

	std::vector<artifical*> adjacent = adjacent_other_cities();
	if (can_market && (!markets || ((!can_technology || technologies) && (!can_tactic || tactics)))) {
		return unit_types.find_market();
	} else if (can_tactic && !tactics && (!can_technology || (hit_points_ < max_hit_points_ * 2 / 3 || (int)adjacent.size() >= tactic_requrie_min_adjacents))) {
		return unit_types.find_tactic();
	} else if (can_technology) {
		return unit_types.find_technology();
	} else {
		VALIDATE(false, "next_ea_utype, unknown branch!");
		return NULL;
	}
}

void artifical::demolish_ea(const std::set<const unit_type*>& can_build_ea)
{
	// don't demolish as long as has vacant ea.
	for (std::vector<map_location>::const_iterator ea = economy_area_.begin(); ea != economy_area_.end(); ++ ea) {
		unit_map::const_iterator find = units_.find(*ea);
		if (!find.valid()) {
			return;
		} else {
			int master = find->type()->master();
			if (!hero::is_ea_artifical(master)) {
				return;
			}
		}
	}

	bool can_technology = can_build_ea.find(unit_types.find_technology()) != can_build_ea.end();
	bool can_tactic = can_build_ea.find(unit_types.find_tactic()) != can_build_ea.end();

	// only demolish tactic or technology
	int markets, technologies, tactics;
	calculate_ea_tiles(markets, technologies, tactics);
	if ((!technologies || !can_tactic) && (!tactics || !can_technology)) {
		return;
	}

	team& current_team = teams_[side_ - 1];
	std::vector<artifical*> adjacent = adjacent_other_cities();
	artifical* t = tactic_on_ea();
	if (can_technology && t && t->level() < 3 && !fronts() && hit_points_ == max_hit_points_) {
		if ((int)adjacent.size() < tactic_requrie_min_adjacents) {
			// no ajdacent cities, demolish tactic
			if (!adjacent_has_enemy(units_, current_team, *t)) {
				do_demolish(*resources::screen, units_, current_team, t, 0, false);
			}
		}
	} else if (can_tactic && (hit_points_ < max_hit_points_ / 2 || (int)adjacent.size() >= tactic_requrie_min_adjacents) && !t && technologies) {
		// demolish a technologies
		for (std::vector<map_location>::const_iterator ea = economy_area_.begin(); ea != economy_area_.end(); ++ ea) {
			unit_map::const_iterator find = units_.find(*ea);
			if (find.valid() && find->type()->master() == hero::number_technology && find->level() < 3) {
				if (!adjacent_has_enemy(units_, current_team, *find)) {
					do_demolish(*resources::screen, units_, current_team, &*find, 0, false);
					break;
				}
			}
		}
	}
}

int artifical::office_heros() const
{
	// estimate
	return int(fresh_heros_.size() + finish_heros_.size() + (reside_troops_.size() + field_troops_.size()) * 2);
}

void artifical::inching_fronts(bool increase)
{
	if (increase) {
		fronts_ ++;
		if (fronts_ > mr_data::max_fronts) {
			fronts_ = mr_data::max_fronts;
		}
	} else if (fronts_ > 0) {
		fronts_ --;
		if (fronts_ == 0) {
			team& t = (*resources::teams)[side_ -1];
			t.erase_strategy(cityno_);
		}
	}
}

void artifical::set_fronts(int fronts)
{
	fronts_ = fronts;
	if (fronts_ < 0) {
		fronts_ = 0;
	} else if (fronts_ > mr_data::max_fronts) {
		fronts_ = mr_data::max_fronts;
	}
}

void artifical::increase_police(int increase)
{
	police_ += increase;
	if (police_ < 0) {
		police_ = 0;
	} else if (police_ > game_config::max_police) {
		police_ = game_config::max_police;
	}
}

void artifical::set_police(int val)
{
	police_ = val;
	if (police_ < 0) {
		police_ = 0;
	} else if (police_ > game_config::max_police) {
		police_ = game_config::max_police;
	}
}

void artifical::set_soldiers(int val)
{
	soldiers_ = val;
}

int artifical::current_soldiers() const
{
	int soldiers = 0;
	for (std::vector<unit*>::const_iterator it = reside_troops_.begin(); it != reside_troops_.end(); ++ it) {
		const unit& u = **it;
		if (u.is_soldier()) {
			soldiers ++;
		}
	}
	for (std::vector<unit*>::const_iterator it = field_troops_.begin(); it != field_troops_.end(); ++ it) {
		const unit& u = **it;
		if (u.is_soldier()) {
			soldiers ++;
		}
	}
	return soldiers;
}

int artifical::max_recruit_cost()
{
	if (max_recruit_cost_ == -1) {
		const std::vector<const unit_type*>& recruits_v = recruits(-1);
		for (std::vector<const unit_type*>::const_iterator it = recruits_v.begin(); it != recruits_v.end(); ++ it) {
			const unit_type* ut = *it;
			if (ut->cost() > max_recruit_cost_) {
				max_recruit_cost_ = ut->cost();
			}
		}
	}
	return max_recruit_cost_;
}

void artifical::reset_max_recruit_cost()
{
	max_recruit_cost_ = -1;
}

void artifical::insert_level_unit_type(int level, const unit_type* from, std::vector<const unit_type*>& to)
{
	std::vector<std::string> advances_to = from->advances_to(especial_);
	for (std::vector<std::string>::const_iterator it = advances_to.begin(); it != advances_to.end(); ++ it) {
		const unit_type* ut = unit_types.find(*it);
		if (not_recruit_.find(ut) != not_recruit_.end()) {
			continue;
		}
		if (ut->level() < level) {
			insert_level_unit_type(level, ut, to);
		} else if (ut->level() == level) {
			to.push_back(ut);
		}
	}
}

const std::vector<const unit_type*>& artifical::recruits(int level)
{ 
	if (level == -1) {
		std::vector<team>& teams = *resources::teams;
		if (teams[side_ - 1].is_human()) {
			level = game_config::default_human_level;
		} else {
			level = game_config::default_ai_level;
		}
	}

	std::map<int, std::vector<const unit_type*> >::iterator it = can_recruit_map_.find(level);
	if (it != can_recruit_map_.end()) {
		return it->second;
	}

	std::vector<const unit_type*> types;
	const std::vector<const unit_type*>& can_recruit = unit_types.can_recruit();
	for (std::vector<const unit_type*>::const_iterator it = can_recruit.begin(); it != can_recruit.end(); ++ it) {
		const unit_type* ut = *it;
		if (not_recruit_.find(ut) != not_recruit_.end()) {
			continue;
		}
		if (ut->level() >= level) {
			types.push_back(ut);
		} else {
			insert_level_unit_type(level, ut, types);
		}
	}
	can_recruit_map_[level] = types;
	it = can_recruit_map_.find(level);

	return it->second;
}

int income_per_businessman = 2;
int income_per_scholar = 2;

int artifical::total_gold_income(int market_increase) const
{
	VALIDATE(commercial_exploiture_, "artifical::total_gold_income, commercial_exploiture_ must not 0!");

	if (tent::mode == mode_tag::SIEGE) {
		return 0;
	}

	int income = gold_bonus();
	for (std::vector<map_location>::const_iterator it = economy_area_.begin(); it != economy_area_.end(); ++ it) {
		unit_map::const_iterator find = units_.find(*it);
		if (find.valid() && find->is_artifical()) {
			income += find->gold_income();
		}
	}
	income = income * market_increase / 100;

	for (std::vector<hero*>::const_iterator it = fresh_heros_.begin(); it != fresh_heros_.end(); ++ it) {
		const hero& h = **it;
		income += fxptoi9(h.spirit_) / 32;
	}
	for (std::vector<unit*>::const_iterator it = reside_commoners_.begin(); it != reside_commoners_.end(); ++ it) {
		const unit& u = **it;
		if (u.gold_income()) {
			income += income_per_businessman;
		}
	}
	income = income * business_income_ratio_ / 100;
	return income;
}

int artifical::total_technology_income(int technology_increase) const
{
	VALIDATE(technology_exploiture_, "artifical::total_gold_income, technology_exploiture_ must not 0!");

	int income = technology_bonus();
	for (std::vector<map_location>::const_iterator it = economy_area_.begin(); it != economy_area_.end(); ++ it) {
		unit_map::const_iterator find = units_.find(*it);
		if (find.valid() && find->is_artifical()) {
			income += find->technology_income();
		}
	}
	income = income * technology_increase / 100;

	for (std::vector<unit*>::const_iterator it = reside_commoners_.begin(); it != reside_commoners_.end(); ++ it) {
		const unit& u = **it;
		if (u.technology_income()) {
			income += income_per_scholar;
		}
	}
	income = income * technology_income_ratio_ / 100;
	return income;
}