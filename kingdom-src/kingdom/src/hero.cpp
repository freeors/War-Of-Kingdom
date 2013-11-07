#include "global.hpp"
#include "hero.hpp"
#include "gettext.hpp"
#include "unit_types.hpp"
#include "filesystem.hpp"

hero hero_invalid = hero(HEROS_INVALID_NUMBER);

hero_map::iterator hero_map_iter_invalid = hero_map::iterator(HEROS_INVALID_NUMBER, NULL);
hero_map::const_iterator hero_map_const_iter_invalid = hero_map::const_iterator(HEROS_INVALID_NUMBER, NULL);

std::string hero::image_file_root_ = "";
std::string hero::gender_str_[HEROS_MAX_GENDER] = {};
std::string hero::arms_str_[HEROS_MAX_ARMS] = {};
std::string hero::skill_str_[HEROS_MAX_SKILL] = {};
std::string hero::feature_str_[HEROS_MAX_FEATURE] = {};
std::string hero::feature_desc_str_[HEROS_MAX_FEATURE] = {};
std::string hero::stratum_str_[HEROS_STRATUMS] = {};
std::string hero::status_str_[HEROS_STATUSES] = {};
std::string hero::official_str_[HEROS_OFFICIALS] = {};
std::string hero::flag_str_[HEROS_FLAGS] = {};
std::vector<int> hero::valid_features_;

int hero::number_system = 0;
int hero::number_civilian = 1;
int hero::number_empty_leader = 2;
int hero::number_scout = 3;
int hero::number_system_min = 0;
int hero::number_system_max = 9;

int hero::number_soldier_min = 10;
int hero::number_soldier_max = 19;

int hero::number_market = 20;
int hero::number_wall = 21;
int hero::number_keep = 22;
int hero::number_tower = 23;
int hero::number_technology = 24;
int hero::number_tactic = 25;
int hero::number_fort = 26;
int hero::number_artifical_min = 20;
int hero::number_artifical_max = 34;

int hero::number_businessman = 35;
int hero::number_scholar = 36;
int hero::number_transport = 37;
int hero::number_commoner_min = 35;
int hero::number_commoner_max = 49;

int hero::number_city_min = 50;
int hero::number_normal_min = 100;

std::vector<hero*> empty_vector_hero_ptr = std::vector<hero*>();
std::vector<size_t> empty_vector_size_t = std::vector<size_t>();

bool compare_leadership(const hero* lhs, const hero* rhs)
{
	return (lhs->leadership_ > rhs->leadership_) || (lhs->leadership_ == rhs->leadership_ && lhs->number_ < rhs->number_);
}

bool compare_politics(const hero* lhs, const hero* rhs)
{
	return (lhs->politics_ > rhs->politics_) || (lhs->politics_ == rhs->politics_ && lhs->skill_[hero_skill_commercial] > rhs->skill_[hero_skill_commercial]);
}

bool compare_recruit(const hero* lhs, const hero* rhs)
{
	if (lhs->official_ == hero_official_leader) return true;
	if (rhs->official_ == hero_official_leader) return false;
	return (lhs->leadership_ > rhs->leadership_) || (lhs->leadership_ == rhs->leadership_ && lhs->number_ < rhs->number_);
}

bool u16_get_experience_i9(uint16_t* field, uint16_t inc_xp)
{
	int integer;
	
	integer = fxptoi9(*field);
	if (*field + inc_xp < 65536) {
		*field += inc_xp;
	} else {
		*field = 65535;
	}
	return (fxptoi9(*field) > integer)? true: false;
}

bool u16_get_experience_i12(uint16_t* field, uint16_t inc_xp)
{
	int integer;
	
	integer = fxptoi12(*field);
	if (*field + inc_xp < 65536) {
		*field += inc_xp;
	} else {
		*field = 65535;
	}
	return (fxptoi12(*field) > integer)? true: false;
}

hero::hero(const hero& that)
	: name_str_(that.name_str_)
	, surname_str_(that.surname_str_)
	, imgfile_()
	, imgfile2_()
{
	const hero_fields_t* that_parent = (const hero_fields_t*)&that;
	hero_fields_t* this_parent = (hero_fields_t*)this;

	memcpy(this_parent, that_parent, sizeof(hero_fields_t));
}

hero::hero(uint16_t number, uint16_t leadership, uint16_t force, uint16_t intellect, uint16_t politics, uint16_t charm)
	: name_str_()
	, surname_str_()
	, imgfile_()
	, imgfile2_()
{
	uint32_t idx;

	number_ = number;
	leadership_ = leadership;
	force_ = force;
	intellect_ = intellect;
	politics_ = politics;
	charm_ = charm;

	gender_ = HEROS_DEFAULT_GENDER;
	image_ = number;
	side_ = HEROS_INVALID_SIDE;
	city_ = HEROS_ROAM_CITY;
	status_ = HEROS_DEFAULT_STATUS;
	official_ = HEROS_DEFAULT_OFFICIAL;
	feature_ = HEROS_NO_FEATURE;
	side_feature_ = HEROS_NO_FEATURE;
	character_ = HEROS_NO_CHARACTER;
	tactic_ = HEROS_NO_TACTIC;
	activity_ = HEROS_DEFAULT_ACTIVITY;
	cost_ = 128;
	noble_ = HEROS_NO_NOBLE;
	meritorious_ = 0;
	base_catalog_ = HEROS_DEFAULT_BASE_CATALOG;
	float_catalog_ = ftofxp8(base_catalog_);
	flags_ = 0;
	heart_ = HEROS_DEFAULT_HEART;
	treasure_ = HEROS_NO_TREASURE;
	utype_ = HEROS_NO_UTYPE;

	//
	// initial some field
	//
	
	// arms adaptability
	memset(arms_, 0, sizeof(arms_));

	// skill adaptability
	memset(skill_, 0, sizeof(skill_));

	// relation
	for (idx = 0; idx < HEROS_MAX_PARENT; idx ++) {
		parent_[idx].hero_ = HEROS_INVALID_NUMBER;
		parent_[idx].feeling_ = 0;
	}
	for (idx = 0; idx < HEROS_MAX_CONSORT; idx ++) {
		consort_[idx].hero_ = HEROS_INVALID_NUMBER;
		consort_[idx].feeling_ = 0;
	}
	for (idx = 0; idx < HEROS_MAX_OATH; idx ++) {
		oath_[idx].hero_ = HEROS_INVALID_NUMBER;
		oath_[idx].feeling_ = 0;
	}
	for (idx = 0; idx < HEROS_MAX_INTIMATE; idx ++) {
		intimate_[idx].hero_ = HEROS_INVALID_NUMBER;
		intimate_[idx].feeling_ = 0;
	}
	for (idx = 0; idx < HEROS_MAX_HATE; idx ++) {
		hate_[idx].hero_ = HEROS_INVALID_NUMBER;
		hate_[idx].feeling_ = 0;
	}
}

hero::hero(const uint8_t* mem)
	: name_str_()
	, surname_str_()
	, imgfile_()
	, imgfile2_()
{
	read(mem);
}

void hero::write(uint8_t* mem)
{

	hero_fields_t* parent = (hero_fields_t*)this;

	memcpy(mem, parent, sizeof(hero_fields_t));
}

void hero::read(const uint8_t* mem)
{
	size_t size = sizeof(hero_fields_t);

	hero_fields_t* parent = (hero_fields_t*)this;
	memcpy(parent, mem, sizeof(hero_fields_t));
}

const bool hero::valid() const
{
	if (number_ == HEROS_INVALID_NUMBER){
		return false;
	}
	return true;
}

bool hero::is_parent(const hero& h, uint32_t* index) const
{
	uint32_t i;

	for (i = 0; i < HEROS_MAX_PARENT; i ++) {
		if (parent_[i].hero_ == h.number_) {
			if (index) {
				*index = i;
			}
			return true;
		}
	}
	for (i = 0; i < HEROS_MAX_PARENT; i ++) {
		if (h.parent_[i].hero_ == number_) {
			if (index) {
				*index = HEROS_MAX_PARENT + i;
			}
			return true;
		}
	}
	return false;
}

bool hero::is_consort(const hero& h, uint32_t* index) const
{
	for (uint32_t i = 0; i < HEROS_MAX_CONSORT; i ++) {
		if (consort_[i].hero_ == h.number_) {
			if (index) {
				*index = i;
			}
			return true;
		}
	}
	return false;
}

bool hero::is_oath(const hero& h, uint32_t* index) const
{
	for (uint32_t i = 0; i < HEROS_MAX_OATH; i ++) {
		if (oath_[i].hero_ == h.number_) {
			if (index) {
				*index = i;
			}
			return true;
		}
	}
	return false;
}

bool hero::is_intimate(const hero& h, uint32_t* index) const
{
	for (uint32_t i = 0; i < HEROS_MAX_INTIMATE; i ++) {
		if (intimate_[i].hero_ == h.number_) {
			if (index) {
				*index = i;
			}
			return true;
		}
	}
	return false;
}

bool hero::is_hate(const hero& h, uint32_t* index) const
{
	for (uint32_t i = 0; i < HEROS_MAX_HATE; i ++) {
		if (hate_[i].hero_ == h.number_) {
			if (index) {
				*index = i;
			}
			return true;
		}
	}
	return false;
}

// @inc: >0, decrease loyalty; < 0, increase loyalty
int hero::increase_catalog(int inc, hero& leader)
{
	static int mid_catalog = HERO_MAX_LOYALTY / 2;
	static int max_catalog = HERO_MAX_LOYALTY;

	if (leader.base_catalog_ == base_catalog_) {
		// this is faithful hero, don't change.
		return loyalty(leader);
	}
	
	int diff = posix_abs((int)leader.base_catalog_ - base_catalog_);
	if (diff > mid_catalog) {
		diff = max_catalog - diff;
	}

	if (inc < 0 && -1 * inc >= diff) {
		// if increase loyalty, don't over HERO_MAX_LOYALTY.
		base_catalog_ = leader.base_catalog_;
	} else {
		int inc_catalog = inc;

		// enlarger distance base_catalog_ and base_catalog_ of leader
		uint16_t leader_base_catalog = leader.base_catalog_;
		bool up_base_catalog = false;
		if (base_catalog_ > leader_base_catalog) {
			if (base_catalog_ - leader_base_catalog < mid_catalog) {
				up_base_catalog = true;
			}
		} else if (leader_base_catalog - base_catalog_ > mid_catalog) {
			up_base_catalog = true;
		}
		if ((up_base_catalog && inc_catalog > 0) || (!up_base_catalog && inc_catalog < 0)) {
			if (inc_catalog < 0) {
				inc_catalog *= -1;
			}
			base_catalog_ += inc_catalog;
			if (base_catalog_ >= max_catalog) {
				base_catalog_ -= max_catalog;
			}
		} else if ((!up_base_catalog && inc_catalog > 0) || (up_base_catalog && inc_catalog < 0)) {
			if (inc_catalog < 0) {
				inc_catalog *= -1;
			}
			if (base_catalog_ >= inc_catalog) {
				base_catalog_ -= inc_catalog;
			} else {
				base_catalog_ = max_catalog - (inc_catalog - base_catalog_);
			}
		}
	}

	// make sure loyalty >= base loyalty
	int base_loyalty = posix_abs((int)leader.base_catalog_ - base_catalog_);
	if (base_loyalty < mid_catalog) {
		base_loyalty = max_catalog - base_loyalty;
	}
	set_loyalty(leader, base_loyalty);

	return base_catalog_;
}

int hero::loyalty(const hero& leader) const
{
	int mid = posix_abs((int)leader.float_catalog_ - float_catalog_);
	mid = fxptoi8(mid);
	if (mid < HERO_MAX_LOYALTY / 2) {
		mid = HERO_MAX_LOYALTY - mid;
	}
	return mid;
}

int hero::base_loyalty(const hero& leader) const
{
	int mid = posix_abs((int)leader.base_catalog_ - base_catalog_);
	if (mid < HERO_MAX_LOYALTY / 2) {
		mid = HERO_MAX_LOYALTY - mid;
	}
	return mid;
}

#define INC_CATALOG_HEART_RATIO		20
// @inc: >0, decrease loyalty; < 0, increase loyalty
int hero::increase_loyalty(int inc, hero& leader)
{
	static int mid_loyalty16 = ftofxp8(HERO_MAX_LOYALTY / 2);
	static int max_loyalty16 = ftofxp8(HERO_MAX_LOYALTY);

	if (leader.base_catalog_ == base_catalog_) {
		// this is faithful hero, don't change.
		return loyalty(leader);
	}
	
	// effect factor: heart
	double inc_catalog_float = 1.0 * (255 - heart_) / INC_CATALOG_HEART_RATIO;
	int inc_catalog = inc * std::max<int>((int)inc_catalog_float, 4);
	// effect factor: base catalog
	int mid = posix_abs((int)leader.base_catalog_ - base_catalog_);
	if (mid > HERO_MAX_LOYALTY / 2) {
		mid = HERO_MAX_LOYALTY - mid;
	}
	inc_catalog *= (mid + 14) / 15;

	// enlarger distance float_catalog_ and base_catalog_ of leader
	uint16_t leader_base_catalog16 = ftofxp8(leader.base_catalog_);
	bool up_float_catalog = false;
	if (float_catalog_ > leader_base_catalog16) {
		if (float_catalog_ - leader_base_catalog16 < mid_loyalty16) {
			up_float_catalog = true;
		}
	} else if (leader_base_catalog16 - float_catalog_ > mid_loyalty16) {
		up_float_catalog = true;
	}
	if ((up_float_catalog && inc_catalog > 0) || (!up_float_catalog && inc_catalog < 0)) {
		if (inc_catalog < 0) {
			inc_catalog *= -1;
		}
		// float_catalog_ += inc_catalog
		float_catalog_ += inc_catalog;
		if (float_catalog_ >= max_loyalty16) {
			float_catalog_ -= max_loyalty16;
		}
	} else if ((!up_float_catalog && inc_catalog > 0) || (up_float_catalog && inc_catalog < 0)) {
		if (inc_catalog < 0) {
			inc_catalog *= -1;
		}
		// float_catalog_ -= inc_catalog
		if (float_catalog_ >= inc_catalog) {
			float_catalog_ -= inc_catalog;
		} else {
			float_catalog_ = max_loyalty16 - (inc_catalog - float_catalog_);
		}
	}

	// calculate loyalty. same as loyalty(...)
	mid = posix_abs((int)leader.float_catalog_ - float_catalog_);
	mid = fxptoi8(mid);
	if (mid < HERO_MAX_LOYALTY / 2) {
		mid = HERO_MAX_LOYALTY - mid;
	}
	return mid;
}

void hero::set_loyalty(hero& leader, int level, bool fixed)
{
	if (level > HERO_MAX_LOYALTY) {
		return;
	}
	int cur_level = loyalty(leader);
	if (cur_level == level) {
		return;
	}
	if (!fixed && cur_level >= level) {
		return;
	}

	bool up_float_catalog = false;
	int mid = posix_abs((int)leader.base_catalog_ - base_catalog_);
	if (mid < HERO_MAX_LOYALTY / 2) {
		if (leader.base_catalog_ < base_catalog_) {
			up_float_catalog = true;
		}
	} else {
		if (leader.base_catalog_ >= base_catalog_) {
			up_float_catalog = true;
		}
	}
	int to_catalog = leader.base_catalog_;
	if (up_float_catalog) {
		to_catalog = to_catalog + HERO_MAX_LOYALTY - level;
		if (to_catalog >= HERO_MAX_LOYALTY) {
			to_catalog -= HERO_MAX_LOYALTY;
		}
	} else {
		to_catalog = to_catalog - (HERO_MAX_LOYALTY - level);
		if (to_catalog < 0) {
			to_catalog = HERO_MAX_LOYALTY + to_catalog; 
		}
	}
	float_catalog_ = ftofxp8(to_catalog);
}

void hero::set_loyalty2(hero& leader, int level, bool fixed)
{
	if (base_catalog_ != leader.base_catalog_) {
		set_loyalty(leader, level, fixed);
	} else {
		float_catalog_ = ftofxp8(base_catalog_);
	}
}

const char* hero::image(bool big)
{
	if (!big) {
		if (!imgfile_.empty()) {
			return imgfile_.c_str();
		}
	} else {
		if (!imgfile2_.empty()) {
			return imgfile2_.c_str();
		}
	}

	std::string full_name, save_name;
	std::stringstream strstr;
	int i32 = image_;
	uint32_t fsize_low = 0, fsize_high = 0;

	if (number_ >= hero_map::map_size_from_dat) {
		if (!big) {
			full_name = get_addon_campaigns_dir() + "/avatar_small.png";
		} else {
			full_name = get_addon_campaigns_dir() + "/avatar_middle.png";
		}
		save_name = full_name; // save_name should utf8.
#ifdef _WIN32
		full_name = conv_ansi_utf8_2(full_name, false);
#endif
		posix_fsize_byname(full_name.c_str(), fsize_low, fsize_high);
	}
	if (!fsize_low && !fsize_high) {
		strstr.str("");
		if (!big) {
			strstr << "hero-64/" << i32 << ".png";
		} else {
			strstr << "hero-256/" << i32 << ".png";
		}
		save_name = strstr.str();
		full_name = image_file_root_ + "/" + save_name;
		posix_fsize_byname(full_name.c_str(), fsize_low, fsize_high);
	}
	if (!fsize_low && !fsize_high) {
		if (!big) {
			save_name = "hero-64/default.png";
		} else {
			save_name = "hero-256/default.png";
		}
	}

	if (!big) {
		imgfile_ = save_name;
		return imgfile_.c_str();
	} else {
		imgfile2_ = save_name;
		return imgfile2_.c_str();
	}
}

void hero::set_image(int image)
{
	image_ = image;
	imgfile_ = "";
	imgfile2_ = "";
}

std::string& hero::name()
{
	if (name_str_.empty()) {
		char text[_MAX_PATH];
		sprintf(text, "%s%u", HERO_PREFIX_STR_NAME, number_);
		name_str_ = dgettext("wesnoth-hero", text);
	}
	return name_str_;
}

void hero::set_name(const std::string& name)
{
	name_str_ = name;
}

std::string& hero::surname()
{
	if (surname_str_.empty()) {
		char text[_MAX_PATH];
		sprintf(text, "%s%u", HERO_PREFIX_STR_SURNAME, number_);
		surname_str_ = dgettext("wesnoth-hero", text);
	}
	return surname_str_;
}

void hero::set_surname(const std::string& surname)
{
	surname_str_ = surname;
}

// if caller want use retval in furture, caller need create copy.
const char* hero::biography()
{
	static const char* default_str = "Wait renewing...";
	char text[_MAX_PATH];
	sprintf(text, "%s%u", HERO_PREFIX_STR_BIOGRAPHY, number_);
	const char* trans = dgettext("wesnoth-hero", text);
	if (trans && (trans != text)) {
		// if gettext cannot find, dgettext will return "text".
		return trans;
	}
	return default_str;
}

std::string hero::heart_str()
{
	char text[_MAX_PATH];
	sprintf(text, "%u", heart_);
	return text;
}

const std::string& hero::flag_str(int flag)
{
	if (flag >= HEROS_FLAGS) {
		return null_str;
	}
	if (flag_str_[flag].empty()) {
		char text[_MAX_PATH];
		sprintf(text, "%s%u", HERO_PREFIX_STR_FLAG, flag);
		flag_str_[flag] = dgettext("wesnoth-hero", text);
	}
	return flag_str_[flag];
}

void hero::set_flag(int flag)
{
	if (flag >= HEROS_FLAGS) {
		return;
	}
	flags_ |= 1 << flag; 
}

void hero::clear_flag(int flag)
{
	if (flag >= HEROS_FLAGS) {
		return;
	}
	flags_ &= ~(1 << flag);
}

bool hero::get_flag(int flag) const
{
	if (flag >= HEROS_FLAGS) {
		return false;
	}
	return flags_ & (1 << flag);
}

const std::string& hero::stratum_str(int stratum)
{
	if (stratum >= HEROS_STRATUMS) {
		return null_str;
	}
	if (stratum_str_[stratum].empty()) {
		char text[_MAX_PATH];
		sprintf(text, "%s%u", HERO_PREFIX_STR_STRATUM, stratum);
		stratum_str_[stratum] = dgettext("wesnoth-hero", text);
	}
	return stratum_str_[stratum];
}

const std::string& hero::status_str(int status)
{
	if (status >= HEROS_STATUSES) {
		return null_str;
	}
	if (status_str_[status].empty()) {
		char text[_MAX_PATH];
		sprintf(text, "%s%u", HERO_PREFIX_STR_STATUS, status);
		status_str_[status] = dgettext("wesnoth-hero", text);
	}
	return status_str_[status];
}

const std::string& hero::official_str(int official)
{
	if (official >= HEROS_OFFICIALS) {
		return null_str;
	}
	if (official_str_[official].empty()) {
		char text[_MAX_PATH];
		sprintf(text, "%s%u", HERO_PREFIX_STR_OFFICIAL, official);
		official_str_[official] = dgettext("wesnoth-hero", text);
	}
	return official_str_[official];
}

const std::string& hero::feature_str(int feature)
{
	if (feature >= HEROS_MAX_FEATURE) {
		return null_str;
	}
	if (feature_str_[feature].empty()) {
		char text[_MAX_PATH];
		sprintf(text, "%s%i", HERO_PREFIX_STR_FEATURE, feature);
		feature_str_[feature] = dgettext("wesnoth-card", text);
	}
	return feature_str_[feature];
}

const std::string& hero::feature_desc_str(int feature)
{
	if (feature >= HEROS_MAX_FEATURE) {
		return null_str;
	}
	if (feature_desc_str_[feature].empty()) {
		char text[_MAX_PATH];
		sprintf(text, "%s%u", HERO_PREFIX_STR_FEATURE_DESC, feature);
		feature_desc_str_[feature] = dgettext("wesnoth-card", text);
	}
	return feature_desc_str_[feature];
}

std::vector<int>& hero::valid_features()
{
	if (valid_features_.empty()) {
		char text[_MAX_PATH];
		for (int idx = 0; idx <= hero_feature_max; idx ++) {
			sprintf(text, "%s%i", HERO_PREFIX_STR_FEATURE, idx);
			char* ptr = dgettext("wesnoth-card", text);
			if (strncmp(ptr, HERO_PREFIX_STR_FEATURE, strlen(HERO_PREFIX_STR_FEATURE))) {
				valid_features_.push_back(idx);
			}
		}
	}
	return valid_features_;
}

std::string& hero::gender_str(int gender)
{
	if (gender_str_[gender].empty()) {
		char text[_MAX_PATH];
		sprintf(text, "%s%u", HERO_PREFIX_STR_GENDER, gender);
		gender_str_[gender] = dgettext("wesnoth-hero", text);
	}
	return gender_str_[gender];
}

std::string& hero::arms_str(int arms)
{
	if (arms_str_[arms].empty()) {
		char text[_MAX_PATH];
		sprintf(text, "%s%u", HERO_PREFIX_STR_ARMS, arms);
		arms_str_[arms] = dgettext("wesnoth-hero", text);
	}
	return arms_str_[arms];
}

std::string& hero::skill_str(int skill)
{
	if (skill_str_[skill].empty()) {
		char text[_MAX_PATH];
		sprintf(text, "%s%u", HERO_PREFIX_STR_SKILL, skill);
		skill_str_[skill] = dgettext("wesnoth-hero", text);
	}
	return skill_str_[skill];
}

std::string hero::adaptability_str2(uint16_t adaptability)
{
	char text[_MAX_PATH];
	sprintf(text, "%s%u", HERO_PREFIX_STR_ADAPTABILITY, fxptoi12(adaptability));
	return dgettext("wesnoth-hero", text);
}

bool hero::is_system(int h)
{
	return h >= number_system_min && h <= number_system_max;
}

bool hero::is_soldier(int h)
{
	return h >= number_soldier_min && h <= number_soldier_max;
}

bool hero::is_artifical(int h)
{
	return h >= number_artifical_min && h <= number_artifical_max;
}

bool hero::is_ea_artifical(int h)
{
	return h == number_market || h == number_technology || h == number_tactic;
}

bool hero::is_commoner(int h)
{
	return h >= number_commoner_min && h <= number_commoner_max;
}

#if defined(_KINGDOM_EXE) || !defined(_WIN32)
#include "unit.hpp"
#include "artifical.hpp"
#include "unit_display.hpp"
#include "game_events.hpp"
#include "team.hpp"
#include "resources.hpp"
#include "game_display.hpp"
#include "wml_exception.hpp"
#include "gui/dialogs/hero_selection.hpp"
#include "gui/widgets/window.hpp"
#include "formula_string_utils.hpp"
#include "construct_dialog.hpp"
#include "actions.hpp"

#include <boost/foreach.hpp>

namespace rpg {
	hero* h = &hero_invalid;
	int stratum = hero_stratum_wander;
	std::set<unit*> humans;
	int forbids = 0;
} // end of namespace rpg

void hero::to_unstage()
{
	if (official_ == hero_official_mayor) {
		unit_map& units = *resources::units;
		artifical* from = units.city_from_cityno(city_);
		from->select_mayor(&hero_invalid);
	} else if (official_ != HEROS_NO_OFFICIAL) {
		official_ = HEROS_NO_OFFICIAL;
	}
	if (has_nomal_noble()) {
		std::vector<team>& teams = *resources::teams;
		teams[side_].appoint_noble(*this, HEROS_NO_NOBLE, true);
	}
	status_ = hero_status_unstage;
	city_ = HEROS_ROAM_CITY;
	side_ = HEROS_INVALID_SIDE;
}

bool hero::has_nomal_noble() const
{
	if (noble_ == HEROS_NO_NOBLE) return false;
	const tnoble& n = unit_types.noble(noble_);
	return !n.leader();
}

void hero::increase_feeling_each(unit_map& units, hero_map& heros, hero& to, int inc)
{
	int carry_to, descent_number;
	std::vector<unit*> adjusted;

	for (int i = 0; i < 2; i ++) {
		if (i == 0) {
			carry_to = increase_feeling(to, inc, descent_number);
		} else {
			carry_to = to.increase_feeling(*this, inc, descent_number);
		}
		unit* u = NULL;
		if (carry_to != FEELING_NONE) {
			game_events::show_relation_message(units, heros, i? to: *this, i? *this: to, carry_to);

			u = units.find_unit(*this);
			if (!u->is_artifical() && std::find(adjusted.begin(), adjusted.end(), u) == adjusted.end()) {
				u->adjust();
				adjusted.push_back(u);
			}
		}
		if (descent_number != HEROS_INVALID_NUMBER) {
			u = units.find_unit(heros[descent_number]);
			if (u && !u->is_artifical() && std::find(adjusted.begin(), adjusted.end(), u) == adjusted.end()) {
				u->adjust();
				adjusted.push_back(u);
			}
		}
	}
}

void hero::add_modification(unit_map& units, hero_map& heros, std::vector<team>& teams, const config& mod, unit* u, hero* leader)
{
	BOOST_FOREACH (const config &effect, mod.child_range("effect")) {
		const std::string &apply_to = effect["apply_to"];

		if (apply_to == "loyalty") {
			int increase = effect["increase"].to_int();

			if (increase) {
				increase_catalog(-1 * increase, *leader);
				if (!u->is_artifical()) {
					u->adjust();
				}
				// play animation
				std::stringstream str;
				str << dgettext("wesnoth", "Loyalty") << "\n";
				std::vector<unit*> touchers;
				if (artifical* city = units.city_from_loc(u->get_location())) {
					touchers.push_back((unit*)(city));
				} else {
					touchers.push_back(u);
				}
				unit_display::unit_touching(*u, touchers, increase, str.str());
			}			
		} else if (apply_to == "office") {
			artifical* selected_city = unit_2_artifical(u);

			if (tent::mode != TOWER_MODE) {
				selected_city->finish_into(*this, hero_status_backing);
			} else {
				selected_city->fresh_into(*this);
			}
			std::vector<hero*>& wander_heros = selected_city->wander_heros();
			wander_heros.erase(std::find(wander_heros.begin(), wander_heros.end(), this));

			std::string message = _("Let me join in. I will do my best to maintenance our honor.");
			join_anim(this, selected_city, message);

			map_location loc2(MAGIC_HERO, number_);
			game_events::fire("post_recommend", selected_city->get_location(), loc2);

		} else if (apply_to == "wander") {
			std::vector<hero*> captains;
			if (artifical* city = units.city_from_loc(u->get_location())) {
				// fresh/finish/reside troop
				if (u->is_city()) {
					for (int type = 0; type < 3; type ++) { 
						std::vector<hero*>* list;
						if (type == 0) {
							list = &city->fresh_heros();
						} else if (type == 1) {
							list = &city->finish_heros();
						} else {
							list = &city->wander_heros();
						}
						std::vector<hero*>::iterator i2 = std::find(list->begin(), list->end(), this);
						if (i2 != list->end()) {
							list->erase(i2);
							break;
						}
					}
				} else {
					std::vector<unit*>& reside_troops = city->reside_troops();
					int index = 0;
					for (std::vector<unit*>::iterator i2 = reside_troops.begin(); i2 != reside_troops.end(); ++ i2, index ++) {
						if (*i2 != u) {
							continue;
						}
						u->replace_captains_internal(*this, captains);
						if (captains.empty()) {
							city->troop_go_out(index);
						} else {
							u->replace_captains(captains);
						}
						break;
					}
				}
			} else {
				// field troop
				u->replace_captains_internal(*this, captains);
				if (captains.empty()) {
					units.erase(u);
				} else {
					u->replace_captains(captains);
				}
			}
			// select one city in myself side
			artifical* to_city = units.city_from_cityno(leader->city_);
			to_city->wander_into(*this);

		} else if (apply_to == "communicate") {
			int increase = effect["increase"].to_int();
			std::vector<std::pair<int, unit*> > pairs;

			for (hero_map::iterator i = heros.begin(); i != heros.end(); ++ i) {
				const hero& h = *i;
				if (h.side_ != side_ || h.number_ == number_) {
					continue;
				}
				unit* u = units.find_unit(h);
				if (u->is_city() && h.number_ == u->master().number_) {
					continue;
				}
				pairs.push_back(std::make_pair(h.number_, u));
			}
			if (increase && !pairs.empty()) {
				// display hero selection dialog
				gui2::thero_selection dlg(&teams, &units, heros, pairs, side_ + 1, "");
				try {
					dlg.show(resources::screen->video());
				} catch(twml_exception& e) {
					e.show(*resources::screen);
					return;
				}
				const std::set<size_t>& checked_pairs = dlg.checked_pairs();
				if (dlg.get_retval() != gui2::twindow::OK || checked_pairs.empty()) {
					return;
				}
				hero& to = heros[pairs[*checked_pairs.begin()].first];
				utils::string_map symbols;
				symbols["first"] = name();
				symbols["second"] = to.name();
				game_events::show_hero_message(&heros[hero::number_scout], NULL, vgettext("Feeling between $first and $second is increased.", symbols), game_events::INCIDENT_INVALID);
				
				// adjust troop if needed.
				increase_feeling_each(units, heros, to, increase);
			}
		}
	}
}

bool hero::internal_matches_filter(const vconfig& cfg)
{
	if (!cfg["number"].blank()) {
		if (cfg["number"].to_int() != number_) {
			return false;
		}
	}

	return true;
}

#define confirm_carry_to game_events::confirm_carry_to

#else
#define confirm_carry_to(h1, h2, carry_to)	true 
#endif

namespace increase_xp {

ublock ub;
ublock& generic_ublock() 
{
	memset(&ub, 0, sizeof(ublock));
	return ub;
}
#if defined(_KINGDOM_EXE) || !defined(_WIN32)
ublock& attack_ublock(const unit& attack, bool opp_is_artifical) 
{
	memset(&ub, 0, sizeof(ublock));
	ub.xp = true;
	if (!attack.is_commoner()) {
		ub.leadership = ub.force = ub.intellect = ub.charm = true;
		ub.arms = true;
		if (opp_is_artifical) {
			ub.skill[hero_skill_demolish] = true;
		}
		// effect: character
		int level = attack.character_level(apply_to_tag::INDUSTRIOUS);
		if (level) {
			int bonus = level / 2;
			ub.leadership_speed = ub.force_speed = ub.intellect_speed = ub.charm_speed = bonus;
		}
		level = attack.character_level(apply_to_tag::EXPANSIVE);
		if (level) {
			ub.arms_speed = level / 2;
		}
		level = attack.character_level(apply_to_tag::CREATIVE);
		if (level) {
			ub.skill_speed[hero_skill_demolish] = level / 2;
		}
		
		// effect: feature
		if (unit_feature_val2(attack, hero_feature_guide)) {
			ub.abilityx2 = true;
		}
		if (unit_feature_val2(attack, hero_feature_spirit)) {
			ub.armsx2 = true;
		}
		if (unit_feature_val2(attack, hero_feature_skill)) {
			ub.skillx2 = true;
		}
		ub.meritorious = true;
	}
	return ub;
}

ublock& turn_ublock(const unit& u)
{
	memset(&ub, 0, sizeof(ublock));
	ub.skill[hero_skill_encourage] = true;
	ub.skill[hero_skill_formation] = true;
	ub.skill_speed[hero_skill_formation] = -50;
	int level = u.character_level(apply_to_tag::CREATIVE);
	if (level) {
		ub.skill_speed[hero_skill_encourage] += level / 2;
		ub.skill_speed[hero_skill_formation] += level / 2;
	}
	if (unit_feature_val2(u, hero_feature_skill)) {
		ub.skillx2 = true;
	}
	return ub;
}

ublock& duel_ublock(const unit& u)
{
	memset(&ub, 0, sizeof(ublock));
	ub.skill[hero_skill_hero] = true;
	if (unit_feature_val2(u, hero_feature_skill)) {
		ub.skillx2 = true;
	}
	return ub;
}

#endif

ublock& exploiture_ublock(int markets, int technologies, int business_speed, int technology_speed, bool abilityx2, bool skillx2)
{
	memset(&ub, 0, sizeof(ublock));
	if (markets) {
		ub.politics = true;
		ub.politics_speed = business_speed - 100;
		ub.skill[hero_skill_commercial] = true;
		ub.skill_speed[hero_skill_commercial] = business_speed - 100;
	}
	if (technologies) {
		ub.intellect = true;
		ub.intellect_speed = technology_speed - 100;
		ub.skill[hero_skill_invent] = true;
		ub.skill_speed[hero_skill_invent] = technology_speed - 100;
	}
	ub.abilityx2 = abilityx2;
	ub.skillx2 = skillx2;

	return ub;
}

int ability_per_xp = 1;
int arms_per_xp = 3;
int skill_per_xp = 16;
int meritorious_per_xp = 5;
int navigation_per_xp = 2;

hblock hb;
hblock& generic_hblock() 
{
	memset(&hb, 0, sizeof(hblock));
	return hb;
}

hblock& exploiture_hblock(int markets, int technologies, int business_speed, int technology_speed, bool abilityx2, bool skillx2, int xp)
{
	memset(&hb, 0, sizeof(hblock));
	int ability_per_xp_x2 = ability_per_xp + (abilityx2? ability_per_xp: 0);
	int skill_per_xp_x2 = skill_per_xp + (skillx2? skill_per_xp: 0);

	if (markets) {
		hb.politics = xp * ability_per_xp_x2 * business_speed / 100;
		hb.skill[hero_skill_commercial] = xp * skill_per_xp_x2 * business_speed / 100;
	}
	if (technologies) {
		hb.intellect = xp * ability_per_xp_x2 * technology_speed / 100;
		hb.skill[hero_skill_invent] = xp * skill_per_xp_x2 * technology_speed / 100;
	}
	return hb;
}

}

bool hero::get_xp(const hblock& hb)
{
	bool has_carry = false;

	if (u16_get_experience_i9(&leadership_, hb.leadership)) {
		has_carry = true;
	}
	if (u16_get_experience_i9(&force_, hb.force)) {
		has_carry = true;
	}
	if (u16_get_experience_i9(&intellect_, hb.intellect)) {
		has_carry = true;
	}
	if (u16_get_experience_i9(&politics_, hb.politics)) {
		has_carry = true;
	}
	if (u16_get_experience_i9(&charm_, hb.charm)) {
		has_carry = true;
	}
	// arms adaptability
	for (int i = 0; i < HEROS_MAX_ARMS; i ++) {
		if (u16_get_experience_i12(arms_ + i, hb.arms[i])) {
			has_carry = true;
		}
	}
	// skill adaptability
	for (int i = 0; i < HEROS_MAX_SKILL; i ++) {
		if (u16_get_experience_i12(skill_ + i, hb.skill[i])) {
			has_carry = true;
		}
	}
	if (meritorious_ < 60000) {
		meritorious_ += hb.meritorious;
		if (meritorious_ > 60000) {
			meritorious_ = 60000;
		}
	}
	return has_carry;
}

// return value
//   FEELING_HATE: get rid of hate
//   FEELING_CONSORT/FEELING_OATH/FEELING_INTIMATE: form to consort/oath/intimate
//   FEELING_NONE: no get rid of or form to
int hero::increase_feeling(hero& to, int inc, int& descent_number)
{
	descent_number = HEROS_INVALID_NUMBER;

	// as far, only support inc > 0
	if (inc <= 0) return FEELING_NONE;

	hero_feeling* field = NULL;
	FEELING carry_to = FEELING_NONE;
	int i;

	// hate
	for (i = 0; i < HEROS_MAX_HATE; i ++) {
		if (hate_[i].hero_ == to.number_) {
			field = hate_ + i;
			break;
		}
	}
	if (i != HEROS_MAX_HATE) {
		if (inc < field->feeling_) {
			field->feeling_ -= inc;
		} else {
			field->hero_ = HEROS_INVALID_NUMBER;
			field->feeling_ = 0;
			carry_to = FEELING_HATE;
		}
		return carry_to;
	}


	for (i = 0; i < HEROS_MAX_PARENT; i ++) {
		// to is this's parent
		if (parent_[i].hero_ == to.number_) {
			field = parent_ + i;
			break;
		}
	}
	if (!field) {
		for (i = 0; i < HEROS_MAX_PARENT; i ++) {
			// this is to's parent
			if (to.parent_[i].hero_ == number_) {
				// do nothing
				return FEELING_NONE;
			}
		}
	
		if (gender_ != to.gender_) {
			for (i = 0; i < HEROS_MAX_CONSORT; i ++) {
				if (consort_[i].hero_ == to.number_) {
					field = consort_ + i;
					break;
				}
			}
		} else {
			for (i = 0; i < HEROS_MAX_OATH; i ++) {
				if (oath_[i].hero_ == to.number_) {
					field = oath_ + i;
					break;
				}
			}
		}
	}
	if (!field) {
		for (i = 0; i < HEROS_MAX_INTIMATE; i ++) {
			if (intimate_[i].hero_ == to.number_) {
				field = intimate_ + i;
				if (gender_ != to.gender_) {
					carry_to = FEELING_CONSORT;
				} else {
					carry_to = FEELING_OATH;
				}
				break;
			}
		}
	}

	int item_count = 0;
	hero_feeling* item_addr = NULL;
	if (field) {
		int feeling = inc + field->feeling_;
		if (feeling >= HERO_MAX_FEELING) {
			// if cannot carry, this keep maximum feeling.
			field->feeling_ = HERO_MAX_FEELING - 1;

			if (carry_to == FEELING_CONSORT) {
				item_count = HEROS_MAX_CONSORT;
				item_addr = consort_;
			} else if (carry_to == FEELING_OATH) {
				item_count = HEROS_MAX_OATH;
				item_addr = oath_;
			}
		} else {
			field->feeling_ += inc;
			// cannot carry, because cannot reach HERO_MAX_FEELING.
			carry_to = FEELING_NONE;
		}
	} else {
		carry_to = FEELING_INTIMATE;

		item_count = HEROS_MAX_INTIMATE;
		item_addr = intimate_;
	}

	if (item_count) {
		// check whether can carry or not.
		for (i = 0; i < item_count; i ++) {
			if (item_addr[i].hero_ == HEROS_INVALID_NUMBER || !item_addr[i].feeling_) {
				break;
			}
		}
		if (i < item_count && (carry_to == FEELING_CONSORT || carry_to == FEELING_OATH)) {
			if (!confirm_carry_to(*this, to, carry_to)) {
				field->feeling_ = HERO_MAX_FEELING / 2;
				return FEELING_NONE;
			}
		}

		for (i = 0; i < item_count; i ++) {
			if (item_addr[i].hero_ == HEROS_INVALID_NUMBER) {
				// new carry set feeling equal 1, it will keep one increase_feeling.
				item_addr[i].hero_ = to.number_;
				item_addr[i].feeling_ = 1;

				if (field) {
					// reset this field
					field->hero_ = HEROS_INVALID_NUMBER;
					field->feeling_ = 0;
				}
				break;
			}
		}
		if (i == item_count) {
			// Cannot find vacant location. execute conflict strategy.
			hero_feeling* descent_field = NULL;

			for (i = 0; i < item_count; i ++) {
				if (item_addr[i].feeling_ == 0) {
					// select it be descentd.
					descent_field = item_addr + i;
				} else if (inc < item_addr[i].feeling_) {
					item_addr[i].feeling_ -= inc;
				} else {
					item_addr[i].feeling_ = 0;
				}
			}
			if (descent_field) {
				// hero that in descent_field will be descentd.
				descent_number = descent_field->hero_;

				if (field) {
					field->hero_ = descent_field->hero_;
					field->feeling_ = HERO_MAX_FEELING - 1;
				}

				// new carry set feeling equal 1, it will keep one increase_feeling.
				descent_field->hero_ = to.number_;
				descent_field->feeling_ = 1;
			} else {
				// In despite of reach HERO_MAX_FEELING, all location's feeling is larger than this, cannot carry.
				carry_to = FEELING_NONE;
			}
		}
	}

	return (carry_to == FEELING_INTIMATE)? FEELING_NONE: carry_to;
}

size_t hero_map::map_size_from_dat = 0;

hero_map::hero_map(const std::string& path) :
	map_size_(0),
	map_(NULL),
	map_vsize_(0)
{
	hero::image_file_root_ = path + "/data/core/images";
}

hero_map::~hero_map()
{
	clear_map();
}

hero_map& hero_map::operator=(const hero_map &that)
{
	realloc_hero_map(that.map_size_);

	for (size_t i = 0; i != that.map_vsize_; ++i) {
		add(*(that.map_[i]));
	}
	return *this;
}

void hero_map::set_path(const std::string& path)
{
	hero::image_file_root_ = path + "/data/core/images";
}

void hero_map::realloc_hero_map(const size_t size)
{
	if (map_) {
		clear_map();
	}
	map_size_ = size;
	map_ = (hero**)malloc(size * sizeof(hero*));
	map_vsize_ = 0;
}

void hero_map::clear_map()
{
	for (size_t i = 0; i != map_vsize_; ++i) {
		delete(map_[i]);
	}
	free(map_);
	map_ = NULL;
	map_vsize_ = 0;
}

hero_map::iterator hero_map::begin() 
{
	size_t i = 0;
	return iterator(i, this);
}

hero_map::const_iterator hero_map::begin() const 
{
	size_t i = 0;
	return const_iterator(i, this);
}

hero_map::iterator hero_map::end()
{ 
	return hero_map_iter_invalid; 
}

hero_map::const_iterator hero_map::end() const 
{ 
	return hero_map_const_iter_invalid; 
}

void hero_map::add(const hero& h)
{
	hero* p = new hero(h);
	p->number_ = map_vsize_;
	map_[map_vsize_ ++] = p;
}

hero& hero_map::operator[](const uint16_t num)
{
	if (num >= map_vsize_) {
		return hero_invalid;
	}
	return *map_[num];
}

const hero& hero_map::operator[](const uint16_t num) const
{
	if (num >= map_vsize_) {
		return hero_invalid;
	}
	return *map_[num];
}

void hero_map::erase(const uint16_t number)
{
	uint16_t idx;
	if (number >= map_vsize_) {
		return;
	}
	delete map_[number];
	if (number != (map_vsize_ - 1)) {
		memcpy(&(map_[number]), &(map_[number + 1]), (map_vsize_ - number - 1) * sizeof(hero*));
	}
	map_vsize_ --;
	// update number_ field that has be moved
	for (idx = number; idx < map_vsize_; idx ++) {
		map_[idx]->number_ --;
	}
}

bool check_hero_valid(const hero& h)
{
	for (int i = 0; i < HEROS_MAX_PARENT; i ++) {
		if (h.number_ < hero::number_normal_min) {
			if (h.parent_[i].hero_ != HEROS_INVALID_NUMBER) {
				return false;
			}
		} else if (h.parent_[i].hero_ < hero::number_normal_min) {
			return false;
		}
	}

	for (int i = 0; i < HEROS_MAX_CONSORT; i ++) {
		if (h.number_ < hero::number_normal_min) {
			if (h.consort_[i].hero_ != HEROS_INVALID_NUMBER) {
				return false;
			}
		} else if (h.consort_[i].hero_ < hero::number_normal_min) {
			return false;
		}
	}

	for (int i = 0; i < HEROS_MAX_OATH; i ++) {
		if (h.number_ < hero::number_normal_min) {
			if (h.oath_[i].hero_ != HEROS_INVALID_NUMBER) {
				return false;
			}
		} else if (h.oath_[i].hero_ < hero::number_normal_min) {
			return false;
		}
	}

	for (int i = 0; i < HEROS_MAX_INTIMATE; i ++) {
		if (h.number_ < hero::number_normal_min) {
			if (h.intimate_[i].hero_ != HEROS_INVALID_NUMBER) {
				return false;
			}
		} else if (h.intimate_[i].hero_ < hero::number_normal_min) {
			return false;
		}
	}

	for (int i = 0; i < HEROS_MAX_HATE; i ++) {
		if (h.number_ < hero::number_normal_min) {
			if (h.hate_[i].hero_ != HEROS_INVALID_NUMBER) {
				return false;
			}
		} else if (h.hate_[i].hero_ < hero::number_normal_min) {
			return false;
		}
	}
	return true;
}

bool hero_map::map_from_file(const std::string& fname)
{
	posix_file_t fp = INVALID_FILE;
	uint32_t bytertd, fsizelow, fsizehigh, rdpos;
	uint8_t* fdata = NULL;
	bool fok = false;

	posix_fopen(fname.c_str(), GENERIC_READ, OPEN_EXISTING, fp);
	if (fp == INVALID_FILE) {
		goto exit;
	}
	posix_fsize(fp, fsizelow, fsizehigh);
	if (fsizelow < HEROS_FILE_PREFIX_BYTES) {
		goto exit;
	}
	fdata = (uint8_t*)malloc(fsizelow);
	if (!fdata) {
		goto exit;
	}
	posix_fseek(fp, HEROS_FILE_PREFIX_BYTES, 0);
	posix_fread(fp, fdata, fsizelow - HEROS_FILE_PREFIX_BYTES, bytertd);

	rdpos = 0;
	// realloc map memory in hero_map
	realloc_hero_map(HEROS_MAX_HEROS);
	while (rdpos + HEROS_BYTES_PER_HERO <= bytertd) {
		hero h(fdata + rdpos);

#if defined(_KINGDOM_EXE) || !defined(_WIN32)
#else
		if (!check_hero_valid(h)) {
			std::stringstream strstr;
			strstr << h.name() << "'s set is invalid!";
			posix_print_mb(utf8_2_ansi(strstr.str().c_str()));
		}
		
#endif
		if (h.valid()) {
			add(h);
		}
		rdpos += HEROS_BYTES_PER_HERO;
	}

	fok = true;
exit:
	if (fp != INVALID_FILE) {
		posix_fclose(fp);
	}
	if (fdata) {
		free(fdata);
	}

	return fok;
}

bool hero_map::map_from_file_fp(posix_file_t fp, uint32_t file_offset, uint32_t valid_bytes)
{
	uint32_t bytertd, fsizelow, fsizehigh, rdpos;
	uint8_t* fdata = NULL;
	bool fok = false;

	posix_fsize(fp, fsizelow, fsizehigh);
	if (fsizelow < file_offset + HEROS_FILE_PREFIX_BYTES) {
		goto exit;
	}
	if (valid_bytes) {
		fdata = (uint8_t*)malloc(valid_bytes - HEROS_FILE_PREFIX_BYTES);
	} else {
		fdata = (uint8_t*)malloc(fsizelow - file_offset - HEROS_FILE_PREFIX_BYTES);
	}
	if (!fdata) {
		goto exit;
	}
	posix_fseek(fp, file_offset + HEROS_FILE_PREFIX_BYTES, 0);
	if (valid_bytes) {
		posix_fread(fp, fdata, valid_bytes - HEROS_FILE_PREFIX_BYTES, bytertd);
	} else {
		posix_fread(fp, fdata, fsizelow - file_offset - HEROS_FILE_PREFIX_BYTES, bytertd);
	}

	rdpos = 0;
	// realloc map memory in hero_map
	realloc_hero_map(HEROS_MAX_HEROS);
	while (rdpos + HEROS_BYTES_PER_HERO <= bytertd) {
		hero h(fdata + rdpos);
		if (h.valid()) {
			add(h);
		}
		rdpos += HEROS_BYTES_PER_HERO;
	}

	fok = true;
exit:
	if (fdata) {
		free(fdata);
	}

	return fok;
}

bool hero_map::map_from_mem(const uint8_t* mem, int len)
{
	if (len < HEROS_FILE_PREFIX_BYTES) {
		return false;
	}

	int rdpos = HEROS_FILE_PREFIX_BYTES;
	// realloc map memory in hero_map
	realloc_hero_map(HEROS_MAX_HEROS);
	while (rdpos + HEROS_BYTES_PER_HERO <= len) {
		hero h(mem + rdpos);

		if (h.valid()) {
			add(h);
		}
		rdpos += HEROS_BYTES_PER_HERO;
	}

	return true;
}

/*
return value:
 false: 1)cannot open file that fname specified
*/
bool hero_map::map_to_file(const std::string& fname)
{
	uint32_t idx, bytertd, *u32ptr;
	posix_file_t fp = INVALID_FILE;
	uint8_t* data = NULL;
	bool fok = false;
	uint8_t prefix[HEROS_FILE_PREFIX_BYTES];

	if (map_vsize_ == 0) {
		goto exit;
	}
	data = (uint8_t*)malloc(map_vsize_ * HEROS_BYTES_PER_HERO);
	if (!data) {
		goto exit;
	}
	
	for (idx = 0; idx < map_vsize_; idx ++) {
		map_[idx]->write(data + idx * HEROS_BYTES_PER_HERO);	
	}
	posix_fopen(fname.c_str(), GENERIC_WRITE, CREATE_ALWAYS, fp);
	if (fp == INVALID_FILE) {
		goto exit;
	}
	memset(&prefix, 0, sizeof(prefix));
	u32ptr = (uint32_t*)prefix;
	// 第四个字节是作为版本号，当前版本V.0
	*u32ptr = mmioFOURCC('H', 'E', 'R', '0');
	posix_fwrite(fp, prefix, HEROS_FILE_PREFIX_BYTES, bytertd);
	posix_fwrite(fp, data, map_vsize_ * HEROS_BYTES_PER_HERO, bytertd);

	fok = true;
exit:
	if (fp != INVALID_FILE) {
		posix_fclose(fp);
	}
	if (data) {
		free(data);
	}
	return fok;
}

bool hero_map::map_to_file_fp(posix_file_t fp)
{
	uint32_t idx, bytertd, *u32ptr;
	uint8_t* data = NULL;
	bool fok = false;
	uint8_t prefix[HEROS_FILE_PREFIX_BYTES];

	if (map_vsize_ == 0) {
		goto exit;
	}
	data = (uint8_t*)malloc(map_vsize_ * HEROS_BYTES_PER_HERO);
	if (!data) {
		goto exit;
	}
	
	for (idx = 0; idx < map_vsize_; idx ++) {
		map_[idx]->write(data + idx * HEROS_BYTES_PER_HERO);	
	}
	memset(&prefix, 0, sizeof(prefix));
	u32ptr = (uint32_t*)prefix;
	// 第四个字节是作为版本号，当前版本V.0
	*u32ptr = mmioFOURCC('H', 'E', 'R', '0');
	posix_fwrite(fp, prefix, HEROS_FILE_PREFIX_BYTES, bytertd);
	posix_fwrite(fp, data, map_vsize_ * HEROS_BYTES_PER_HERO, bytertd);

	fok = true;
exit:
	if (data) {
		free(data);
	}
	return fok;
}

uint32_t hero_map::map_to_mem(uint8_t* data) const
{
	uint32_t idx, *u32ptr;

	memset(data, 0, HEROS_FILE_PREFIX_BYTES);
	u32ptr = (uint32_t*)data;
	// 第四个字节是作为版本号，当前版本V.0
	*u32ptr = mmioFOURCC('H', 'E', 'R', '0');

	for (idx = 0; idx < map_vsize_; idx ++) {
		map_[idx]->write(data + HEROS_FILE_PREFIX_BYTES + idx * HEROS_BYTES_PER_HERO);	
	}
	return HEROS_FILE_PREFIX_BYTES + map_vsize_ * HEROS_BYTES_PER_HERO;
}

size_t hero_map::file_size() const
{
	return HEROS_FILE_PREFIX_BYTES + map_vsize_ * HEROS_BYTES_PER_HERO;
}

void hero_map::reset_to_unstage()
{
	for (uint32_t idx = 0; idx < map_vsize_; idx ++) {
		map_[idx]->side_ = HEROS_INVALID_SIDE;
		map_[idx]->city_ = HEROS_ROAM_CITY;
		map_[idx]->status_ = HEROS_DEFAULT_STATUS;
		map_[idx]->activity_ = HEROS_DEFAULT_ACTIVITY;

		// It mean next scenario will clear commercials.
		map_[idx]->official_ = HEROS_DEFAULT_OFFICIAL;
	}
}

void hero_map::change_language()
{
	for (int i = 0; i < HEROS_MAX_ARMS; i ++) {
		hero::arms_str_[i].clear();
	}
	for (int i = 0; i < HEROS_MAX_SKILL; i ++) {
		hero::skill_str_[i].clear();
	}
	for (int i = 0; i < HEROS_MAX_FEATURE; i ++) {
		hero::feature_str_[i].clear();
	}
	for (int i = 0; i < HEROS_MAX_FEATURE; i ++) {
		hero::feature_desc_str_[i].clear();
	}
	for (int i = 0; i < HEROS_STRATUMS; i ++) {
		hero::stratum_str_[i].clear();
	}
	for (int i = 0; i < HEROS_STATUSES; i ++) {
		hero::status_str_[i].clear();
	}
	for (int i = 0; i < HEROS_OFFICIALS; i ++) {
		hero::official_str_[i].clear();
	}
}

tgroup group;