#include "global.hpp"
#include "hero.hpp"
#include "gettext.hpp"
#include "unit_types.hpp"
#include "filesystem.hpp"
#include "wml_exception.hpp"
#include "help.hpp"
#include "formula_string_utils.hpp"
#include "game_config.hpp"


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
int hero::number_school = 27;
int hero::number_artifical_min = 20;
int hero::number_artifical_max = 34;

int hero::number_businessman = 35;
int hero::number_scholar = 36;
int hero::number_transport = 37;
int hero::number_commoner_min = 35;
int hero::number_commoner_max = 49;

int hero::number_local_player_city = 95;
int hero::number_network_player_city_min = 96;
int hero::number_network_player_city_max = 99;

int hero::number_city_min = 50;
int hero::number_normal_min = 100;
int hero::number_employee_min = 430;

std::vector<hero*> empty_vector_hero_ptr = std::vector<hero*>();
std::vector<size_t> empty_vector_size_t = std::vector<size_t>();

bool compare_leadership(const hero* lhs, const hero* rhs)
{
	return (lhs->leadership_ > rhs->leadership_) || (lhs->leadership_ == rhs->leadership_ && lhs->number_ < rhs->number_);
}

bool compare_spirit(const hero* lhs, const hero* rhs)
{
	return (lhs->spirit_ > rhs->spirit_) || (lhs->spirit_ == rhs->spirit_ && lhs->skill_[hero_skill_commercial] > rhs->skill_[hero_skill_commercial]);
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
	, uid_(that.uid_)
	, biography_str_(that.biography_str_)
	, imgfile_()
	, imgfile2_()
{
	const hero_fields_t* that_parent = (const hero_fields_t*)&that;
	hero_fields_t* this_parent = (hero_fields_t*)this;

	memcpy(this_parent, that_parent, sizeof(hero_fields_t));
}

void hero::clear_relation()
{
	int idx = 0;
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

hero::hero(uint16_t number, uint16_t leadership, uint16_t force, uint16_t intellect, uint16_t spirit, uint16_t charm)
	: name_str_()
	, surname_str_()
	, uid_(-1)
	, biography_str_()
	, imgfile_()
	, imgfile2_()
{
	number_ = number;
	leadership_ = leadership;
	force_ = force;
	intellect_ = intellect;
	spirit_ = spirit;
	charm_ = charm;

	gender_ = HEROS_DEFAULT_GENDER;
	image_ = number;
	player_ = HEROS_INVALID_NUMBER;
	name_ = number;
	surname_ = number;
	desc_ = number;
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
	clear_relation();
}

hero::hero(const uint8_t* mem)
	: name_str_()
	, surname_str_()
	, uid_(-1)
	, biography_str_()
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
int hero::increase_catalog(int inc, const hero& leader)
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

void hero::set_loyalty(const hero& leader, int level, bool fixed)
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

//
// set both base_catalog and float_catalog;
void hero::set_loyalty3(const hero& leader, int level)
{
	if (level > HERO_MAX_LOYALTY) {
		return;
	}
	int base = base_loyalty(leader);
	int inc = level - base;

	if (base_catalog_ != leader.base_catalog_ && inc) {
		if (inc > 0) {
			increase_catalog(-1 * inc, leader);
		} else {
			// don't support decrease loyalty
			// increase_catalog(-1 * inc, leader);
		}
	} else {
		float_catalog_ = ftofxp8(base_catalog_);
	}
}

std::string hero::image_from_uid(int uid, int gender, bool big)
{
	std::string full_name, save_name;
	std::stringstream strstr;
	uint32_t fsize_low = 0, fsize_high = 0;

	strstr << get_addon_campaigns_dir() << "/" << uid;
	if (!big) {
		strstr << "_small.png";
	} else {
		strstr << "_middle.png";
	}

	save_name = strstr.str(); // save_name should utf8.
#ifdef _WIN32
	full_name = conv_ansi_utf8_2(save_name, false);
#else 
	full_name = save_name;
#endif
	posix_fsize_byname(full_name.c_str(), fsize_low, fsize_high);

	if (!fsize_low && !fsize_high) {
		int i32 = hero_map::map_size_from_dat + (gender == hero_gender_female? 1: 0);
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

	return save_name;
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

	if (uid_ < 0) {
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
	} else {
		save_name = image_from_uid(uid_, gender_, big);
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

void hero::set_uid(int _uid) 
{ 
	uid_ = _uid;
	imgfile_ = "";
	imgfile2_ = "";
}

std::string& hero::name()
{
	if (name_str_.empty()) {
		char text[_MAX_PATH];
		sprintf(text, "%s%u", HERO_PREFIX_STR_NAME, name_);
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
		sprintf(text, "%s%u", HERO_PREFIX_STR_SURNAME, surname_);
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
	sprintf(text, "%s%u", HERO_PREFIX_STR_BIOGRAPHY, desc_);
	const char* trans = dgettext("wesnoth-hero", text);
	if (trans && (trans != text)) {
		// if gettext cannot find, dgettext will return "text".
		return trans;
	}
	return default_str;
}

std::string hero::biography2(const hero_map& heros)
{
	if (!biography_str_.empty()) {
		return biography_str_;
	}

	const char* b = biography();
	if (player_ == HEROS_INVALID_NUMBER) {
		return b;
	}

	std::stringstream strstr;
	utils::string_map symbols;
	symbols["leader"] = help::tintegrate::generate_format(heros[player_].name(), "green");
	strstr << vgettext("wesnoth-lib", "Hero that belong to $leader.", symbols) << "\n";
	strstr << biography();

	return strstr.str();
}

void hero::set_biography(const std::string& str)
{
	biography_str_ = str;
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
	return h == number_market || h == number_technology || h == number_tactic || h == number_school;
}

bool hero::is_commoner(int h)
{
	return h >= number_commoner_min && h <= number_commoner_max;
}

bool hero::data_variable(int h)
{
	return h >= number_city_min;
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
#include "construct_dialog.hpp"
#include "actions.hpp"
#include "multiplayer.hpp"

#include <boost/foreach.hpp>

namespace rpg {
	hero* h = &hero_invalid;
	int stratum = hero_stratum_wander;
	std::set<unit*> humans;
	int forbids = 0;
} // end of namespace rpg

void hero::to_unstage(int reason)
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

	if (reason == UNSTAGE_GROUP) {
		std::string message;
		if (tent::mode != mode_tag::RPG || treasure_ == HEROS_NO_TREASURE) {
			message = dsgettext("wesnoth-lib", "You had holden same hero as me. I see the light of officialdom and should unstage.");
		} else {
			utils::string_map symbols;
			const ttreasure& treasure = unit_types.treasure(treasure_);
			symbols["treasure"] = help::tintegrate::generate_format(treasure.name(), "green");
			message = vgettext("wesnoth-lib", "You had holden same hero as me. I see the light of officialdom and should unstage. It is $treasure, and give it to you.", symbols);
			
			team& t = (*resources::teams)[rpg::h->side_];
			std::vector<size_t>& holden = t.holded_treasures();
			holden.push_back(treasure_);
			treasure_ = HEROS_NO_TREASURE;
		}
		game_events::show_hero_message(this, NULL, message, game_events::INCIDENT_WANDER);
	}
}

bool hero::has_nomal_noble() const
{
	if (noble_ == HEROS_NO_NOBLE) return false;
	const tnoble& n = unit_types.noble(noble_);
	return !n.leader();
}

void hero::goto_hate_if(const team& new_team, const hero& original_leader, int random)
{
	if (tent::mode != mode_tag::RPG) {
		return;
	}
	hero& new_leader = *new_team.leader();
	if (&new_leader != rpg::h) {
		return;
	}
	if (base_catalog_ == original_leader.base_catalog_ && base_catalog_ != new_leader.base_catalog_ && !(random % 3)) {
		if (new_leader.can_goto_hate(new_leader)) {
			::do_hate_relation(*this, new_leader, true);
		}
	}
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
				
				hero& leader = *teams[u->side() - 1].leader();
				if (base_catalog_ == leader.base_catalog_ && is_hate(leader)) {
					::do_hate_relation(leader, *this, false);
				}
			}			
		} else if (apply_to == "office") {
			artifical* selected_city = unit_2_artifical(u);
			bool city_team_is_rpg = teams[selected_city->side() - 1].leader() == rpg::h;

			std::string message;
			if (city_team_is_rpg && runtime_groups::exist_member(*this, *rpg::h)) {
				this->to_unstage(UNSTAGE_GROUP);
			} else if (!tent::tower_mode()) {
				selected_city->finish_into(*this, hero_status_backing);
			} else {
				selected_city->fresh_into(*this);
			}
			std::vector<hero*>& wander_heros = selected_city->wander_heros();
			wander_heros.erase(std::find(wander_heros.begin(), wander_heros.end(), this));

			if (status_ != hero_status_unstage) {
				message = _("Let me join in. I will do my best to maintenance our honor.");
				join_anim(this, selected_city, message);
			}
			
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
							city->troop_go_out(*u);
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

int hero::calculate_used_point() const
{
	const int least_field = 50;
	const int point_per_adaptability = 10;
	const int point_per_feature_level = 25;
	const int point_per_tactic_level = 25;
	const int point_character_used = 50;
	int used_point = 0;

	// 5 field
	if (fxptoi9(leadership_) > least_field) {
		used_point += fxptoi9(leadership_) - least_field;
	}
	if (fxptoi9(force_) > least_field) {
		used_point += fxptoi9(force_) - least_field;
	}
	if (fxptoi9(intellect_) > least_field) {
		used_point += fxptoi9(intellect_) - least_field;
	}
	if (fxptoi9(spirit_) > least_field) {
		used_point += fxptoi9(spirit_) - least_field;
	}
	if (fxptoi9(charm_) > least_field) {
		used_point += fxptoi9(charm_) - least_field;
	}

	// arms adaptability
	for (int i = 0; i < HEROS_MAX_ARMS; i ++) {
		used_point += fxptoi12(arms_[i]) * point_per_adaptability;
	}
		
	// skill adaptability
	for (int i = 0; i < HEROS_MAX_SKILL; i ++) {
		used_point += fxptoi12(skill_[i]) * point_per_adaptability;
	}

	// feature
	if (feature_ != HEROS_NO_FEATURE) {
		used_point += (1 + unit_types.feature_level(feature_)) * point_per_feature_level;
	}
	if (side_feature_ != HEROS_NO_FEATURE) {
		used_point += (1 + unit_types.feature_level(side_feature_)) * 2 * point_per_feature_level;
	}

	// tactic
	if (tactic_ != HEROS_NO_TACTIC) {
		used_point += (1 + unit_types.tactic(tactic_).level()) * point_per_tactic_level;
	}

	// character
	if (character_ != HEROS_NO_CHARACTER) {
		used_point += point_character_used;
	}

	return used_point;
}

void hero::adjust_according_to_level(const hero& base, int level, const hero& leader)
{
	int max_catalog = game_config::wander_loyalty_threshold + 1;

	name_ = base.name_;
	surname_ = base.surname_;
	desc_ = base.desc_;
	image_ = base.image_;
	player_ = leader.number_;
	treasure_ = HEROS_NO_TREASURE;

	// 5 fields
	int increase = level - game_config::levels_per_rank;
	uint16_t* ptr;
	int value;
	for (int index = hero_field_min; index <= hero_field_max; index ++) {
		if (index == hero_field_leadership) {
			value = fxptoi9(base.leadership_);
			ptr = &leadership_;
		} else if (index ==hero_field_force) {
			value = fxptoi9(base.force_);
			ptr = &force_;
		} else if (index ==hero_field_intellect) {
			value = fxptoi9(base.intellect_);
			ptr = &intellect_;
		} else if (index ==hero_field_spirit) {
			value = fxptoi9(base.spirit_);
			ptr = &spirit_;
		} else {
			value = fxptoi9(base.charm_);
			ptr = &charm_;
		}
		value += increase;

		if (value < 1) {
			value = 1;
		} else if (value > fxptoi9(65535)) {
			value = fxptoi9(65535);
		}
		*ptr = ftofxp9(value);
	}

	// arms/skill adaptability
	increase = level / game_config::levels_per_rank - 1;
	for (int i = 0; i < HEROS_MAX_ARMS; i ++) {
		value = fxptoi12(base.arms_[i]);
		value += increase;
		if (value < 0) {
			value = 0;
		} else if (value > fxptoi12(65535)) {
			value = fxptoi12(65535);
		}
		arms_[i] = ftofxp12(value);
	}

	for (int i = 0; i < HEROS_MAX_SKILL; i ++) {
		value = fxptoi12(base.skill_[i]);
		value += increase;
		if (value < 0) {
			value = 0;
		} else if (value > fxptoi12(65535)) {
			value = fxptoi12(65535);
		}
		skill_[i] = ftofxp12(value);
	}

	// tactic
	if (level >= game_config::levels_per_rank) {
		// don't forget to set. previous staus may be lower level hero.
		tactic_ = base.tactic_;
	} else {
		tactic_ = HEROS_NO_TACTIC;
	}

	// character
	if (level >= game_config::levels_per_rank) {
		character_ = base.character_;
	} else {
		character_ = HEROS_NO_CHARACTER;
	}

	// flag
	if (get_flag(hero_flag_roam)) {
		clear_flag(hero_flag_roam);
	}

	// relation
	for (int i = 0; i < HEROS_MAX_HATE; i ++) {
		hate_[i].hero_ = HEROS_INVALID_NUMBER;
		hate_[i].feeling_ = 0;
	}

	if (level >= game_config::levels_per_rank * 2) {
		max_catalog = HERO_MAX_LOYALTY;
	} else if (level >= game_config::levels_per_rank) {
		max_catalog = game_config::move_loyalty_threshold;
	}

	base_catalog_ = base.base_catalog_;
	float_catalog_ = base.float_catalog_;
	set_loyalty3(leader, max_catalog);
	
}

bool hero::is_roam_member() const
{
	if (get_flag(hero_flag_roam)) {
		return true;
	}
	if (leadership_ >= ftofxp9(90) || charm_ >= ftofxp9(90)) {
		return true;
	}
	return false;
}

void hero::field_set_minmum()
{
	const int least_field = 50;

	leadership_ = ftofxp9(least_field);
	force_ = ftofxp9(least_field);
	intellect_ = ftofxp9(least_field);
	spirit_ = ftofxp9(least_field);
	charm_ = ftofxp9(least_field);

	for (int i = 0; i < HEROS_MAX_ARMS; i ++) {
		arms_[i] = 0;
	}
	for (int i = 0; i < HEROS_MAX_SKILL; i ++) {
		skill_[i] = 0;
	}
	side_feature_ = HEROS_NO_FEATURE;
	feature_ = HEROS_NO_FEATURE;
	tactic_ = HEROS_NO_TACTIC;
	character_ = HEROS_NO_CHARACTER;
}

std::string hero::field_to_str()
{
	std::stringstream strstr;

	strstr << null_str; // past is reserved by surname
	strstr << "|" << uid_;
	int value = gender_;
	strstr << "|" << value;

	value = fxptoi9(leadership_);
	strstr << "|" << value;
	value = fxptoi9(force_);
	strstr << "|" << value;
	value = fxptoi9(intellect_);
	strstr << "|" << value;
	value = fxptoi9(spirit_);
	strstr << "|" << value;
	value = fxptoi9(charm_);
	strstr << "|" << value;

	for (int i = 0; i < HEROS_MAX_ARMS; i ++) {
		value = fxptoi12(arms_[i]);
		strstr << "|" << value;
	}
	for (int i = 0; i < HEROS_MAX_SKILL; i ++) {
		value = fxptoi12(skill_[i]);
		strstr << "|" << value;
	}

	value = side_feature_;
	strstr << "|" << value;
	value = feature_;
	strstr << "|" << value;
	value = tactic_;
	strstr << "|" << value;
	value = character_;
	strstr << "|" << value;

	value = utype_;
	strstr << "|" << value;
	value = base_catalog_;
	strstr << "|" << value;

	return strstr.str();
}

int hero::gender_from_filed_str(const std::string& str)
{
	size_t count = 3 + 5 + HEROS_MAX_ARMS + HEROS_MAX_SKILL + 4 + 2;
	
	const std::vector<std::string> fields = utils::split(str, '|', utils::STRIP_SPACES);
	if (fields.size() != count) {
		return hero_gender_male;
	}
	return lexical_cast_default<int>(fields[2]);
}

void hero::field_from_str(const std::string& str)
{
	size_t count = 3 + 5 + HEROS_MAX_ARMS + HEROS_MAX_SKILL + 4 + 2;
	
	const std::vector<std::string> fields = utils::split(str, '|', utils::STRIP_SPACES);
	if (fields.size() != count) {
		return;
	}
	int index = 0;
	index ++; // past is reserved by surname

	uid_ = lexical_cast_default<int>(fields[index ++]);
	gender_ = lexical_cast_default<int>(fields[index ++]);
	image_ = gender_ == hero_gender_male? number_: (number_ + 1);

	leadership_ = ftofxp9(lexical_cast_default<int>(fields[index ++]));
	force_ = ftofxp9(lexical_cast_default<time_t>(fields[index ++]));
	intellect_ = ftofxp9(lexical_cast_default<int>(fields[index ++]));
	spirit_ = ftofxp9(lexical_cast_default<int>(fields[index ++]));
	charm_ = ftofxp9(lexical_cast_default<int>(fields[index ++]));

	for (int i = 0; i < HEROS_MAX_ARMS; i ++) {
		arms_[i] = ftofxp12(lexical_cast_default<int>(fields[index ++]));
	}
	for (int i = 0; i < HEROS_MAX_SKILL; i ++) {
		skill_[i] = ftofxp12(lexical_cast_default<int>(fields[index ++]));
	}

	side_feature_ = lexical_cast_default<int>(fields[index ++]);
	feature_ = lexical_cast_default<int>(fields[index ++]);
	tactic_ = lexical_cast_default<int>(fields[index ++]);
	character_ = lexical_cast_default<int>(fields[index ++]);

	utype_ = lexical_cast_default<int>(fields[index ++]);
	base_catalog_ = lexical_cast_default<int>(fields[index ++]);
	float_catalog_ = ftofxp8(base_catalog_);
}

std::string hero::translatable_to_str()
{
	std::stringstream strstr;

	strstr << surname();
	strstr << "|" << biography_str_;

	return strstr.str();
}

void hero::translatable_from_str(const std::string& str)
{
	size_t count = 1 + 1;
	
	const std::vector<std::string> fields = utils::split(str, '|', utils::STRIP_SPACES);
	if (fields.size() != count) {
		return;
	}

	int index = 0;
	set_surname(fields[index ++]);
	set_biography(fields[index ++]);
}

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
		if (unit_feature_val2(attack, hero_feature_arms)) {
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
		ub.spirit = true;
		ub.spirit_speed = business_speed - 100;
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
		hb.spirit = xp * ability_per_xp_x2 * business_speed / 100;
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
	if (u16_get_experience_i9(&spirit_, hb.spirit)) {
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

bool hero::can_goto_hate(const hero& h) const
{
	// both this and that have hollow position.
	int i;
	for (i = 0; i < HEROS_MAX_HATE; i ++) {
		if (hate_[i].hero_ == h.number_) {
			return false;
		} else if (hate_[i].hero_ == HEROS_INVALID_NUMBER) {
			break;
		}
	}
	if (i == HEROS_MAX_HATE) {
		return false;
	}
	for (i = 0; i < HEROS_MAX_HATE; i ++) {
		if (h.hate_[i].hero_ == HEROS_INVALID_NUMBER) {
			return true;
		}
	}
	return false;
}

void hero::do_hate_relation(hero& to, bool set)
{
	int i;
	hero_feeling* field = NULL;

	if (set) {
		for (i = 0; i < HEROS_MAX_HATE; i ++) {
			VALIDATE(hate_[i].hero_ != to.number_, "duplicate set!");
			if (hate_[i].hero_ == HEROS_INVALID_NUMBER) {
				field = hate_ + i;
				break;
			}
		}
		if (field) {
			field->hero_ = to.number_;
			field->feeling_ = 0;
		}

	} else {
		for (i = 0; i < HEROS_MAX_HATE; i ++) {
			if (hate_[i].hero_ == to.number_) {
				field = hate_ + i;
				break;
			}
		}
		VALIDATE(field, "not exist in hate list!");
		if (field) {
			for (; i < HEROS_MAX_HATE - 1; i ++) {
				field->hero_ = hate_[i + 1].hero_;
				field->feeling_ = hate_[i + 1].feeling_;

				field = hate_ + (i + 1);
			}
			field->hero_ = HEROS_INVALID_NUMBER;
			field->feeling_ = 0;
		}
	}
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

	for (i = 0; i < HEROS_MAX_HATE; i ++) {
		if (hate_[i].hero_ == to.number_) {
			// that in this'hate list, do nothing.
			return FEELING_NONE;
		}
	}
	for (i = 0; i < HEROS_MAX_HATE; i ++) {
		if (to.hate_[i].hero_ == number_) {
			// this in that's hate list, do nothing.
			return FEELING_NONE;
		}
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
/*
hero& hero_map::operator[](const uint16_t num)
{
	if (num >= map_vsize_) {
		return hero_invalid;
	}
	return *map_[num];
}
*/
hero& hero_map::operator[](const uint16_t num) const
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
	// 4th byte reserved to version number.
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
	// 4th byte reserved to version number.
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
	// 4th byte reserved to version number.
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

namespace upgrade {

std::vector<trequire> require_member, require_noble;

void fill_require()
{
	if (require_member.empty()) {
		// copper
		require_member.push_back(trequire(0, 75)); // Lv0
		require_member.push_back(trequire(0, 100));
		require_member.push_back(trequire(0, 125));
		require_member.push_back(trequire(0, 150));
		require_member.push_back(trequire(0, 175));

		// sliver
		require_member.push_back(trequire(0, 200)); // Lv5
		require_member.push_back(trequire(0, 250));
		require_member.push_back(trequire(0, 300));
		require_member.push_back(trequire(0, 350));
		require_member.push_back(trequire(0, 400));

		// gold
		require_member.push_back(trequire(2, 0)); // Lv10
		require_member.push_back(trequire(2, 50));
		require_member.push_back(trequire(2, 100));
		require_member.push_back(trequire(2, 150));
		// require_member.push_back(trequire(2, 200));
	}
	if (require_noble.empty()) {
		require_noble.push_back(trequire(0, 500));
		require_noble.push_back(trequire(0, 1000));
		require_noble.push_back(trequire(0, 2000));
		require_noble.push_back(trequire(20, 0));
		// require_noble.push_back(trequire(30, 0));
	}
}

const trequire& member_require(int level) 
{
	return require_member[level];
}

trequire member_upgrade_cost(int from, int to)
{
	trequire ret;
	for (int level = from; level < to; level ++) {
		ret.coin += require_member[level].coin;
		ret.score += require_member[level].score;
	}
	return ret;
}

const trequire& noble_require(int noble)
{
	return require_noble[noble];
}

trequire noble_upgrade_cost(int from, int to)
{
	trequire ret;
	for (int level = from; level < to; level ++) {
		ret.coin += require_noble[level].coin;
		ret.score += require_noble[level].score;
	}
	return ret;
}

}


tgroup group, null_group;
std::map<int, tgroup> other_group;

namespace runtime_groups {
std::map<int, tgroup> gs;

void insert(tgroup& g)
{
	gs.insert(std::make_pair(g.leader().number_, g));
}

tgroup& get(int leader)
{
	std::map<int, tgroup>::iterator it = gs.find(leader);
	if (it != gs.end()) {
		return it->second;
	}
	return null_group;
}

void redirect_hero_map(hero_map& heros)
{
	for (std::map<int, tgroup>::iterator it = gs.begin(); it != gs.end(); ++ it) {
		it->second.redirect_hero_map(heros);
	}
}

void remove_unstage_member()
{
	for (std::map<int, tgroup>::iterator it = gs.begin(); it != gs.end(); ++ it) {
		it->second.remove_unstage_member();
	}
}

size_t size()
{
	size_t s = 0;
	for (std::map<int, tgroup>::const_iterator it = gs.begin(); it != gs.end(); ++ it) {
		s += it->second.fp_size();
	}
	return s;
}

void from_fp(hero_map& heros, posix_file_t fp, int len)
{
	gs.clear();
	if (!len) {
		return;
	}

	uint32_t bytertd;
	uint8_t* data = (uint8_t*)malloc(len);
	posix_fread(fp, data, len, bytertd);

	int pos = 0;
	while (pos < len) {
		int number;
		memcpy(&number, data + pos, 4);
		std::pair<std::map<int, tgroup>::iterator, bool> ins = gs.insert(std::make_pair(number, tgroup()));
		tgroup& g = ins.first->second;
		pos += g.from_fp(heros, data + pos);
	}
	free(data);
}

void to_fp(posix_file_t fp)
{
	size_t s = size();
	if (!s) {
		return;
	}
	uint8_t* data = (uint8_t*)malloc(s);
	int pos = 0;
	for (std::map<int, tgroup>::const_iterator it = gs.begin(); it != gs.end(); ++ it) {
		const tgroup& g = it->second;
		pos += g.to_fp(data + pos);
	}

	uint32_t bytertd;
	posix_fwrite(fp, data, s, bytertd);
	free(data);
}

void from_mem(hero_map& heros, uint8_t* mem, int len)
{
	gs.clear();
	if (!len) {
		return;
	}

	int pos = 0;
	while (pos < len) {
		int number;
		memcpy(&number, mem + pos, 4);
		std::pair<std::map<int, tgroup>::iterator, bool> ins = gs.insert(std::make_pair(number, tgroup()));
		tgroup& g = ins.first->second;
		pos += g.from_fp(heros, mem + pos);
	}
}

void to_mem(uint8_t* mem)
{
	int pos = 0;
	for (std::map<int, tgroup>::const_iterator it = gs.begin(); it != gs.end(); ++ it) {
		const tgroup& g = it->second;
		pos += g.to_fp(mem + pos);
	}
}

bool exist_member(const hero& h, const hero& leader)
{
	if (h.player_ != HEROS_INVALID_NUMBER) {
		return false;
	}

	const tgroup& g = get(leader.number_);
	return g.valid() && (g.exist_member(h.number_) || g.exist_exile(h.number_));
}

}

tgroup::tmember tgroup::null_member;

void tgroup::reset()
{
	leader_ = NULL;
	members_.clear();
	exiles_.clear();
	city_ = NULL;

	layout_.clear();
	map_.clear();
	faction_cfg_.clear();
}

bool tgroup::valid() const
{
	return leader_? true: false;
}

tgroup::tmember& tgroup::member(const hero& h)
{
	for (std::vector<tgroup::tmember>::iterator it = members_.begin(); it != members_.end(); ++ it) {
		if (it->h == &h) {
			return *it;
		}
	}
	VALIDATE(false, "group::member, cannot find member corresponding to this h!");
	return null_member;
}

const tgroup::tmember& tgroup::member(const hero& h) const
{
	for (std::vector<tgroup::tmember>::const_iterator it = members_.begin(); it != members_.end(); ++ it) {
		if (it->h == &h) {
			return *it;
		}
	}
	VALIDATE(false, "group::member, cannot find member corresponding to this h!");
	return null_member;
}

const tgroup::tmember& tgroup::exile(const hero& h) const
{
	for (std::vector<tgroup::tmember>::const_iterator it = exiles_.begin(); it != exiles_.end(); ++ it) {
		if (it->h == &h) {
			return *it;
		}
	}
	VALIDATE(false, "group::exile, cannot find exile corresponding to this h!");
	return null_member;
}

tgroup::tmember& tgroup::member_from_base(const hero& h)
{
	for (std::vector<tgroup::tmember>::iterator it = members_.begin(); it != members_.end(); ++ it) {
		if (it->base == &h) {
			return *it;
		}
	}
	VALIDATE(false, "group::member, cannot find member corresponding to this h!");
	return null_member;
}

const tgroup::tmember& tgroup::member_from_base(const hero& h) const
{
	for (std::vector<tgroup::tmember>::const_iterator it = members_.begin(); it != members_.end(); ++ it) {
		if (it->base == &h) {
			return *it;
		}
	}
	VALIDATE(false, "group::member, cannot find member corresponding to this h!");
	return null_member;
}

hero* tgroup::exist_member(int number) const
{
	for (std::vector<tmember>::const_iterator it = members_.begin(); it != members_.end(); ++ it) {
		if (it->base->number_ == number) return it->h;
	}
	return NULL;
}

void tgroup::insert_member(hero_map& heros, int number, int level)
{
	VALIDATE(!exist_member(number), "tgroup::insert_member, duplicate number!");
	// because history reason, maybe exist large number hero
	if (number >= (int)hero_map::map_size_from_dat) {
		return;
	}

	hero& base = heros[number];
	heros.add(base);
	hero* h = &heros[heros.size() - 1];

	h->adjust_according_to_level(base, level, *leader_);

	tmember m;
	m.base = &base;
	m.level = level;
	m.h = h;
	members_.push_back(m);
}

void tgroup::unstage_member(hero_map& heros, int number)
{
	std::vector<tmember>::iterator it = members_.begin();
	for (; it != members_.end(); ++ it) {
		if (it->h->number_ == number) {
			break;
		}
	}
	VALIDATE(it != members_.end(), "tgroup::unstage_member, cannot find number!");

	heros[number].status_ = hero_status_unstage;
}

void tgroup::adjust_members_according_to_leader(hero_map& heros)
{
	for (std::vector<tmember>::const_iterator it = members_.begin(); it != members_.end(); ++ it) {
		const tmember& m = *it;
		m.h->adjust_according_to_level(*m.base, m.level, *leader_);
	}
	for (std::vector<tmember>::const_iterator it = exiles_.begin(); it != exiles_.end(); ++ it) {
		const tmember& m = *it;
		m.h->adjust_according_to_level(*m.base, game_config::min_exile_level, *leader_);
	}
}

void tgroup::remove_unstage_member()
{
	for (std::vector<tmember>::iterator it = members_.begin(); it != members_.end(); ) {
		const tmember& m = *it;
		if (m.h->status_ == hero_status_unstage) {
			it = members_.erase(it);
		} else {
			++ it;
		}
	}
	for (std::vector<tmember>::iterator it = exiles_.begin(); it != exiles_.end(); ) {
		const tmember& m = *it;
		if (m.h->status_ == hero_status_unstage) {
			it = exiles_.erase(it);
		} else {
			++ it;
		}
	}
}

std::vector<tgroup::tmember*> tgroup::part_members(hero_map& heros, bool employee)
{
	std::vector<tgroup::tmember*> ret;
	for (std::vector<tmember>::iterator it = members_.begin(); it != members_.end(); ++ it) {
		tgroup::tmember& m = *it;
		if (m.base->get_flag(hero_flag_employee)) {
			if (employee) {
				ret.push_back(&m);
			}
		} else if (!employee) {
			ret.push_back(&m);
		}
	}
	return ret;
}

hero* tgroup::exist_exile(int number) const
{
	for (std::vector<tmember>::const_iterator it = exiles_.begin(); it != exiles_.end(); ++ it) {
		if (it->base->number_ == number) return it->h;
	}
	return NULL;
}

void tgroup::insert_exile(hero_map& heros, int number, int level)
{
	VALIDATE(!exist_exile(number), "tgroup::insert_exile, duplicate number!");
	// because history reason, maybe exist large number hero
	if (number >= (int)hero_map::map_size_from_dat) {
		return;
	}

	hero& base = heros[number];
	heros.add(base);
	hero* h = &heros[heros.size() - 1];

	h->adjust_according_to_level(base, game_config::min_exile_level, *leader_);

	tmember m;
	m.base = &base;
	m.level = level;
	m.h = h;
	exiles_.push_back(m);
}

void tgroup::unstage_exile(hero_map& heros, int number)
{
	std::vector<tmember>::iterator it = exiles_.begin();
	for (; it != exiles_.end(); ++ it) {
		if (it->h->number_ == number) {
			break;
		}
	}
	VALIDATE(it != exiles_.end(), "tgroup::unstage_exile, cannot find number!");

	heros[number].status_ = hero_status_unstage;
}

#if defined(_KINGDOM_EXE) || !defined(_WIN32)
#include "game_preferences.hpp"
#include "gui/dialogs/message.hpp"

bool tgroup::upgrade_internal(game_display& disp, hero_map& heros, tmember* m, std::map<int, http::temployee>* employees)
{
	utils::string_map symbols;
	std::stringstream err;
	
	const upgrade::trequire* require_ptr;
	if (m) {
		if (m->level >= game_config::max_member_level) {
			return false;
		}
		require_ptr = &upgrade::member_require(m->level);

	} else {
		int noble = preferences::noble();
		if (noble >= unit_types.max_noble_level()) {
			return false;
		}
		require_ptr = &upgrade::noble_require(noble);
	}

	const upgrade::trequire& require = *require_ptr;
	if (sum_score(coin_, score_) < require.score) {
		symbols["coin"] = help::tintegrate::generate_format(require.coin, "red");
		symbols["score"] = help::tintegrate::generate_format(require.score, "red");
		err << vgettext("wesnoth-lib", "Repertory is not enough to pay $coin coin and $score score. If lack one only, can exchange between coin and score.", symbols);
		gui2::show_message(disp.video(), "", err.str());
		return false;
	}

	http::membership m2;
	if (!m) {
		m2 = http::upgrade(disp, heros, HEROS_INVALID_NUMBER, require.coin, require.score);

	} else if (!m->base->get_flag(hero_flag_employee)) {
		m2 = http::upgrade(disp, heros, m->base->number_, require.coin, require.score);

	} else {
		std::set<int> number;
		number.insert(m->base->number_);
		m2 = http::employee_common(disp, heros, http::employee_tag_upgrade, number, -1 * require.coin, -1 * require.score, employees);
	}
	if (m2.uid < 0) {
		return false;
	}
	from_local_membership(disp, heros, m2, false);

	return true;
}

bool tgroup::upgrade_leader(game_display& disp, hero_map& heros)
{
	return upgrade_internal(disp, heros, NULL, NULL);
}

bool tgroup::upgrade_member(game_display& disp, hero_map& heros, hero& h)
{
	tmember& m = member(h);
	return upgrade_internal(disp, heros, &m, NULL);
}

int tgroup::calculate_total_point() const
{
	const int allocatable[] = {300, 350, 400, 450, 500};
	int noble = preferences::noble();

	int bonus = preferences::vip2()? 150: 0;
	return allocatable[noble] + bonus;
}

void tgroup::from_membership(hero_map& heros, const http::membership& m, int city)
{
	reset();

	heros.add(hero(heros.size()));
	hero& leader = heros[heros.size() - 1];
	leader.set_name(m.name);
	leader.set_uid(m.uid);
	leader.field_from_str(m.field);
	leader.translatable_from_str(m.translatable);

	set_leader(leader);
	noble_ = m.noble;
	coin_ = m.coin;
	score_ = m.score;
	tax_ = m.tax;
	interior_from_str(m.interior);
	signin_from_str(m.signin);
	message_from_str(m.message);
	member_from_str(heros, m.member, false);
	exile_from_str(heros, m.exile);
	associate_from_str(m.associate);
	set_layout(m.layout);
	set_map(m.map);

	int number = city != HEROS_INVALID_NUMBER? city: hero::number_local_player_city + 1 + (int)other_group.size();
	if (number > hero::number_network_player_city_max) {
		number = hero::number_network_player_city_max;
	}
	hero& h = heros[number];
	set_city(h);
	city_->set_name(m.city);
}

// this function can only use for local player!
void tgroup::from_local_membership(display& disp, hero_map& heros, const http::membership& m, bool enhance)
{
	if (m.noble != noble()) {
		set_noble(m.noble);
		preferences::set_noble(m.noble);
	}

	if (m.coin != coin()) {
		set_coin(m.coin);
		preferences::set_coin(m.coin);
	}
	if (m.score != score()) {
		set_score(m.score);
		preferences::set_score(m.score);
	}
	preferences::set_vip_expire(m.expire);

	if (m.uid != leader().uid()) {
		leader().set_uid(m.uid);
		preferences::set_uid(m.uid);
	}

	if (m.city != city().name()) {
		city().set_name(m.city);
		preferences::set_city(m.city);
	}

	if (m.interior != interior_to_str()) {
		interior_from_str(m.interior);
		preferences::set_interior(m.interior);
	}

	if (m.signin != signin_to_str()) {
		signin_from_str(m.signin);
		preferences::set_signin(m.signin);
	}

	if (m.member != member_to_str() || m.exile != exile_to_str()) {
		if (group.valid()) {
			// if in runtime, don't update heros!
			reload_heros_from_string(heros, m.member, m.exile);
		}
		preferences::set_member(m.member);
		preferences::set_exile(m.exile);
	}

	if (m.associate != associate_to_str()) {
		associate_from_str(m.associate);
		preferences::set_associate(m.associate);
	}

	if (enhance) {
		// below code maybe send http command, must validate timestamp.
		game_config::timestamp = m.timestamp;
	}

	// check other's request according to new associate.
	http::membership m2;
	for (std::vector<tgroup::tassociate>::const_iterator it = associates_.begin(); it != associates_.end(); ++ it) {
		const tgroup::tassociate& a = *it;
		if (a.agreement == tgroup::tassociate::requestally && !a.t) {
			m2 = http::affirm_ally(disp, heros, a.username, true, false);
		} else if (a.agreement == tgroup::tassociate::requestterminate && !a.t) {
			m2 = http::affirm_terminate(disp, heros, a.username, true);
		}
	}
	if (m2.uid >= 0 && m2.associate != associate_to_str()) {
		associate_from_str(m2.associate);
		preferences::set_associate(m2.associate);
	}
	
	bool hero_data_dirty = false;
	if (m.field != leader().field_to_str()) {
		leader().field_from_str(m.field);
		hero_data_dirty = true;
	}

	if (m.translatable != leader().translatable_to_str()) {
		leader().translatable_from_str(m.translatable);
		hero_data_dirty = true;
	}

	if (hero_data_dirty) {
		preferences::set_hero(heros, leader());
	}

	if (enhance) {
		if (m.tax != tax()) {
			set_tax(m.tax);
		}
		if (m.layout != layout()) {
			set_layout(m.layout);
			preferences::set_layout(m.layout);
		}

		if (m.map != map()) {
			set_map(m.map);
			preferences::set_map(m.map);
		}

		message_from_str(m.message);
	}
}

http::membership tgroup::to_membership() const
{
	http::membership m;

	m.uid = leader().uid();
	m.vip = vip();
	m.name = leader().name();
	m.expire = expire();
	m.noble = noble();
	m.member = member_to_str();
	m.exile = exile_to_str();
	m.associate = associate_to_str();
	m.layout = layout();
	m.field = leader().field_to_str();
	m.translatable = leader().translatable_to_str();
	m.score = score();
	m.coin = coin();
	m.city = city().name();
	m.map = map();
	m.interior = interior_to_str();
	m.signin = signin_to_str();

	return m;
}

void tgroup::set_map(const std::string& str)
{
	map_ = str;
}

void tgroup::set_layout(const std::string& str)
{
	layout_ = str;
}

const char layout_chapter_marker = '$';
const char layout_section_marker = '|';
const std::string layout_base_prefix = "base";
const std::string layout_troop_prefix = "troop";
const std::string layout_art_prefix = "art";
const std::string layout_noble_prefix = "noble";

int tgroup::layout_number_from_hero(const hero& h) const
{
	if (h.player_ != HEROS_INVALID_NUMBER) {
		return member(h).base->number_;
	}
	return h.number_;
}

bool compare_unit_location(const unit* l, const unit* r)
{
	return l->get_location() < r->get_location();
}

std::string tgroup::layout_from_team_internal(team& t, const tgroup& g)
{
	// map_location, hero | & NOBLE_MAGIC, hero, & map_lcation, wall2
	const std::vector<artifical*>& cities = t.holden_cities();
	const artifical& city = *cities[0];

	std::stringstream strstr;

	// base
	strstr << layout_base_prefix;
	strstr << layout_section_marker;
	strstr << hero_map::map_size_from_dat;
	strstr << layout_section_marker;

	int stratagem = t.current_stratagem();
	if (stratagem == stratagem_tag::none) {
		stratagem = stratagem_tag::min;
	}
	strstr << stratagem;

	strstr << layout_chapter_marker;

	// filed troop
	strstr << layout_troop_prefix;
	std::vector<unit*> fields = city.field_troops();
	std::sort(fields.begin(), fields.end(), compare_unit_location);
	for (std::vector<unit*>::const_iterator it = fields.begin(); it != fields.end(); ++ it) {
		strstr << layout_section_marker;
		const unit& u = **it;
		strstr << u.get_location().x << "," << u.get_location().y << ",";
		strstr << g.layout_number_from_hero(u.master());
		if (u.second().valid()) {
			strstr << "," << g.layout_number_from_hero(u.second());
		}
		if (u.third().valid()) {
			strstr << "," << g.layout_number_from_hero(u.third());
		}
	}
	strstr << layout_chapter_marker;

	// field art
	strstr << layout_art_prefix;
	std::vector<artifical*> arts = city.field_arts();
	std::sort(arts.begin(), arts.end(), compare_unit_location);
	for (std::vector<artifical*>::const_iterator it = arts.begin(); it != arts.end(); ++ it) {
		strstr << layout_section_marker;
		const artifical& art = **it;
		strstr << art.get_location().x << "," << art.get_location().y << ",";
		strstr << art.type()->id();
	}
	strstr << layout_chapter_marker;
	
	// noble
	strstr << layout_noble_prefix;
	const std::map<int, hero*>& appointed_nobles = t.appointed_nobles();
	for (std::map<int, hero*>::const_iterator it = appointed_nobles.begin(); it != appointed_nobles.end(); ++ it) {
		strstr << layout_section_marker;
		strstr << it->first << "," << g.layout_number_from_hero(*it->second);
	}

	return strstr.str();
}

void tgroup::layout_from_team(game_display& disp, team& t)
{
	layout_ = layout_from_team_internal(t, *this);
}

hero& tgroup::hero_from_layout_number(hero_map& heros, int number, int layout_leader_number) const
{
	hero& h = heros[number];
	if (number == layout_leader_number) {
		return *leader_;
	} else if (h.player_ == HEROS_INVALID_NUMBER) {
		hero* h2 = exist_member(number);
		return h2? *h2: hero_invalid;
	}
	return hero_invalid;
}

bool tgroup::layout_to_team_internal(unit_map& units, hero_map& heros, std::vector<team>& teams, game_state& state, const std::string& from, team& t, bool mirror, bool conside_stratagem, bool wall, const tgroup& g)
{
	int relative = 0;
	const std::vector<artifical*>& cities = t.holden_cities();
	artifical& city = *cities[0];
	int layout_leader_number = HEROS_INVALID_NUMBER;
	// int stratagem = stratagem_tag::min;

	const std::vector<std::string> chapters = utils::split(from, layout_chapter_marker, utils::STRIP_SPACES);
	if (chapters.size() != 4) {
		return false;
	}
	for (std::vector<std::string>::const_iterator it = chapters.begin(); it != chapters.end(); ++ it) {
		const std::string& chapter = *it;
		const std::vector<std::string> sections = utils::split(chapter, layout_section_marker, utils::STRIP_SPACES);
		if (sections.empty()) {
			return false;
		}
		const std::string& name = sections[0];
		std::vector<std::string> fields;
		if (name == layout_base_prefix) {
			if (sections.size() < 3) {
				return false;
			}
			layout_leader_number = lexical_cast_default<int>(sections[1]);
			if (conside_stratagem) {
				int stratagem = lexical_cast_default<int>(sections[2]);
				if (stratagem < stratagem_tag::min || stratagem > stratagem_tag::max) {
					stratagem = stratagem_tag::min;
				}
				t.insert_stratagem(stratagem_tag::technology(stratagem), false);
			}

		} else if (name == layout_troop_prefix) {
			for (size_t n = 1; n < sections.size(); n ++) {
				if (layout_leader_number == HEROS_INVALID_NUMBER) {
					continue;
				}
				if (t.max_troops && t.holden_troops() >= t.max_troops) {
					break;
				}

				fields = utils::split(sections[n]);
				if (fields.size() < 3 || fields.size() > 5) {
					continue;
				}
				map_location at(lexical_cast_default<int>(fields[0]), lexical_cast_default<int>(fields[1]));
				if (mirror) {
					at.x = game_config::siege_map_w * 2 - at.x;
				}
				hero& master = g.hero_from_layout_number(heros, lexical_cast_default<int>(fields[2]), layout_leader_number); 
				if (!master.valid()) {
					continue;
				}
				const unit_type* ut = unit_types.keytypes().begin()->second;
				if (master.utype_ != HEROS_NO_UTYPE) {
					ut = unit_types.keytype(master.utype_);
				}

				std::vector<const hero*> v;
				v.push_back(&master);
				for (size_t n1 = 3; n1 < fields.size(); n1 ++) {
					hero& h = g.hero_from_layout_number(heros, lexical_cast_default<int>(fields[n1]), layout_leader_number);
					if (h.valid()) {
						v.push_back(&h);
					}
				}
	
				int cost = 0;
				do_recruit(units, heros, teams, t, ut, v, city, cost, true, false, state);
				do_direct_expedite(teams, units, city, city.reside_troops().size() - 1, at, false);
			}
		} else if (name == layout_art_prefix) {
			for (size_t n = 1; n < sections.size(); n ++) {
				fields = utils::split(sections[n]);
				if (fields.size() != 3) {
					continue;
				}
				map_location at(lexical_cast_default<int>(fields[0]), lexical_cast_default<int>(fields[1]));
				if (mirror) {
					at.x = game_config::siege_map_w * 2 - at.x;
				}
				const unit_type* ut = unit_types.find(fields[2]);
				if (!ut) {
					continue;
				}
				VALIDATE(ut->master() != HEROS_INVALID_NUMBER, "to_team, invalid art type");

				if (!wall && ut->master() == hero::number_wall) {
					continue;
				}

				std::vector<const hero*> v;
				v.push_back(&heros[ut->master()]);
				
				type_heros_pair pair(ut, v);
				artifical new_art(units, heros, teams, state, pair, city.cityno(), true);
				units.add(at, &new_art);
			}

		} else if (name == layout_noble_prefix) {
			for (size_t n = 1; n < sections.size(); n ++) {
				if (layout_leader_number == HEROS_INVALID_NUMBER) {
					continue;
				}
				fields = utils::split(sections[n]);
				if (fields.size() != 2) {
					continue;
				}
				int noble = lexical_cast_default<int>(fields[0]);
				hero& h = g.hero_from_layout_number(heros, lexical_cast_default<int>(fields[1]), layout_leader_number); 
				if (!h.valid()) {
					continue;
				}
				t.appoint_noble(h, noble, false);
			}
		}
	}
	return true;
}

bool tgroup::layout_to_team(game_display& disp, unit_map& units, hero_map& heros, std::vector<team>& teams, game_state& state, team& t, bool mirror, bool conside_stratagem, bool wall)
{
	if (layout_.empty()) {
		return true;
	}
	try {
		return layout_to_team_internal(units, heros, teams, state, layout_, t, mirror, conside_stratagem, wall, *this);
	} catch (twml_exception& e) {
		layout_.clear();
		e.show(disp);
	}
	return layout_to_team_internal(units, heros, teams, state, layout_, t, mirror, conside_stratagem, wall, *this);
}

const ttechnology& tgroup::stratagem_from_layout_str_internal(const std::string& layout)
{
	int stratagem = stratagem_tag::min;

	do {
		const std::vector<std::string> chapters = utils::split(layout, layout_chapter_marker, utils::STRIP_SPACES);
		if (chapters.size() != 4) {
			break;
		}
		for (std::vector<std::string>::const_iterator it = chapters.begin(); it != chapters.end(); ++ it) {
			const std::string& chapter = *it;
			const std::vector<std::string> sections = utils::split(chapter, layout_section_marker, utils::STRIP_SPACES);
			if (sections.empty()) {
				break;
			}
			const std::string& name = sections[0];
			std::vector<std::string> fields;
			if (name == layout_base_prefix) {
				if (sections.size() >= 3) {
					stratagem = lexical_cast_default<int>(sections[2]);
					if (stratagem < stratagem_tag::min || stratagem > stratagem_tag::max) {
						stratagem = stratagem_tag::min;
					}
				}
			}
		}
	} while (false);

	return stratagem_tag::technology(stratagem);
}

const ttechnology& tgroup::stratagem_from_layout_str() const
{
	return stratagem_from_layout_str_internal(layout_);
}

std::vector<int> tgroup::interior_increase_from_layout_str_internal(const std::string& layout)
{
	std::vector<int> increase(interior_count);

	do {
		const std::vector<std::string> chapters = utils::split(layout, layout_chapter_marker, utils::STRIP_SPACES);
		if (chapters.size() != 4) {
			break;
		}
		for (std::vector<std::string>::const_iterator it = chapters.begin(); it != chapters.end(); ++ it) {
			const std::string& chapter = *it;
			const std::vector<std::string> sections = utils::split(chapter, layout_section_marker, utils::STRIP_SPACES);
			if (sections.empty()) {
				break;
			}
			const std::string& name = sections[0];
			std::vector<std::string> fields;
			if (name == layout_art_prefix) {
				for (size_t n = 1; n < sections.size(); n ++) {
					fields = utils::split(sections[n]);
					if (fields.size() != 3) {
						continue;
					}
					const unit_type* ut = unit_types.find(fields[2]);
					if (!ut) {
						continue;
					}
					if (ut->master() == hero::number_tactic) {
						increase[tgroup::interior_military] += ut->level();

					} else if (ut->master() == hero::number_school) {
						increase[tgroup::interior_culture] += ut->level();

					} else if (ut->master() == hero::number_market) {
						increase[tgroup::interior_economy] += ut->level();

					} else if (ut->master() == hero::number_technology) {
						increase[tgroup::interior_science] += ut->level();
					}
				}
			}
		}
	} while (false);

	return increase;
}

std::vector<int> tgroup::interior_increase_from_layout_str() const
{
	return interior_increase_from_layout_str_internal(layout_);
}

std::vector<const ttechnology*> tgroup::calculate_allocatable_technology(int mode, std::vector<const ttechnology*>& exclude, int random) const
{
	std::vector<const ttechnology*> ret;
	int science = interiors_[interior_science];

	// 10--->625
	int allocatable = pow(1.0 * science * 39062.5, 0.5);

	std::vector<const ttechnology*> candidate;
	const std::set<const ttechnology*>& selectable = unit_types.selectable_technologies();
	const std::map<std::string, ttechnology>& technologies = unit_types.technologies();
	for (std::map<std::string, ttechnology>::const_iterator it = technologies.begin(); it != technologies.end(); ++ it) {
		const ttechnology& t = it->second;
		if (std::find(exclude.begin(), exclude.end(), &t) != exclude.end()) {
			continue;
		}
		if (t.stratagem() || selectable.find(&t) == selectable.end() || !t.filter_mode(mode)) {
			continue;
		}
		if (allocatable >= t.max_experience()) {
			candidate.push_back(&t);
		}
	}
	while (!candidate.empty()) {
		const ttechnology* t = candidate[random % candidate.size()];
		ret.push_back(t);
		allocatable -= t->max_experience();
		candidate.erase(std::find(candidate.begin(), candidate.end(), t));
		for (std::vector<const ttechnology*>::iterator it = candidate.begin(); it != candidate.end(); ) {
			const ttechnology* t = *it;
			if (allocatable < t->max_experience()) {
				it = candidate.erase(it);
			} else {
				++ it;
			}
		}
		random = random * 1103515245 + 12345;
		random = (static_cast<unsigned>(random / 65536) % 32768);
	}

	return ret;
}

std::string tgroup::increase_allocatable_technology(int mode, const std::string& exclude_str, int random) const
{
	const std::vector<std::string> vstr = utils::split(exclude_str);
	std::vector<const ttechnology*> exclude;
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		exclude.push_back(&unit_types.technology(*it));
	}
	std::vector<const ttechnology*> allocated = calculate_allocatable_technology(mode, exclude, random);
	// std::copy(allocated.begin(), allocated.end(), std::back_inserter(exclude));

	std::stringstream ret;
	ret << exclude_str;
	for (std::vector<const ttechnology*>::const_iterator it = allocated.begin(); it != allocated.end(); ++ it) {
		if (!ret.str().empty()) {
			ret << ",";
		}
		ret << (*it)->id();
	}
	return ret.str();
}

bool compare_charm_leadership_diff(const hero* l, const hero* r)
{
	int ldiff = 1 * l->charm_ - l->leadership_;
	if (l->feature_ != HEROS_NO_FEATURE) {
		ldiff += unit_types.feature_level(l->feature_) * ftofxp9(10);
	}
	if (l->tactic_ != HEROS_NO_TACTIC) {
		ldiff += unit_types.tactic(l->tactic_).level() * ftofxp9(10);
	}
	int rdiff = 1 * r->charm_ - r->leadership_;
	if (r->feature_ != HEROS_NO_FEATURE) {
		rdiff += unit_types.feature_level(r->feature_) * ftofxp9(10);
	}
	if (r->tactic_ != HEROS_NO_TACTIC) {
		rdiff += unit_types.tactic(r->tactic_).level() * ftofxp9(10);
	}
	return ldiff > rdiff || (ldiff == rdiff && l->number_ < r->number_);
}

bool compare_leadership_charm_diff(const hero* l, const hero* r)
{
	int ldiff = 1 * l->leadership_ - l->charm_;
	if (l->feature_ != HEROS_NO_FEATURE) {
		ldiff += unit_types.feature_level(l->feature_) * ftofxp9(10);
	}
	if (l->tactic_ != HEROS_NO_TACTIC) {
		ldiff += unit_types.tactic(l->tactic_).level() * ftofxp9(10);
	}
	int rdiff = 1 * r->leadership_ - r->charm_;
	if (r->feature_ != HEROS_NO_FEATURE) {
		rdiff += unit_types.feature_level(r->feature_) * ftofxp9(10);
	}
	if (r->tactic_ != HEROS_NO_TACTIC) {
		rdiff += unit_types.tactic(r->tactic_).level() * ftofxp9(10);
	}
	return ldiff > rdiff || (ldiff == rdiff && l->number_ < r->number_);
}

std::map<int, int> tgroup::adjust_hero_field(int field)
{
	VALIDATE(field == hero_field_leadership || field == hero_field_charm, "adjust_hero_field, only support leadership, charm!");

	std::map<int, int> ret;
	int interior_index = interior_count;
	std::vector<hero*> candidate;
	candidate.push_back(leader_);

	for (std::vector<tgroup::tmember>::const_iterator it = members_.begin(); it != members_.end(); ++ it) {
		candidate.push_back(it->h);
	}
	if (field == hero_field_leadership) {
		interior_index = interior_military;
		std::sort(candidate.begin(), candidate.end(), compare_charm_leadership_diff);

	} else if (field == hero_field_charm) {
		interior_index = interior_culture;
		std::sort(candidate.begin(), candidate.end(), compare_leadership_charm_diff);
	}

	std::vector<int> numbers;
	for (std::vector<hero*>::const_iterator it = candidate.begin(); it != candidate.end(); ++ it) {
		numbers.push_back((*it)->number_);
	}

	int value = interiors_[interior_index];

	// 10--->15
	int allocatable = pow(1.0 * value * 22.5, 0.5);

	const int interior_max_hero_field = 110;
	while (!candidate.empty() && allocatable > 0) {
		hero& h = *candidate.front();
		if (field == hero_field_leadership) {
			value = fxptoi9(h.leadership_);
		} else if (field == hero_field_charm) {
			value = fxptoi9(h.charm_);
		}
		if (value < interior_max_hero_field) {
			int bonus = interior_max_hero_field - value;
			if (allocatable < bonus) {
				bonus = allocatable;
			}
			allocatable -= bonus;
			if (field == hero_field_leadership) {
				h.leadership_ = ftofxp9(value + bonus);
			} else if (field == hero_field_charm) {
				h.charm_ = ftofxp9(value + bonus);
			}
			ret.insert(std::make_pair(h.number_, bonus));
		}
		candidate.erase(candidate.begin());
	}
	return ret;
}

std::vector<size_t> tgroup::increase_card(team& t, int mode)
{
	std::vector<size_t> ret;
	int economy = interiors_[interior_economy];

	// 10--->20
	int allocatable = pow(1.0 * economy * 40, 0.5);

	const std::vector<size_t>& candidate_cards = t.candidate_cards();
	const int card_cost = (mode == mode_tag::RPG)? 30: 40;
	int index = 0;
	while (!candidate_cards.empty() && allocatable >= card_cost) {
		ret.push_back(candidate_cards[index % candidate_cards.size()]);
		t.add_card(ret.back(), true);
		index ++;
		allocatable -= card_cost;
	}
	return ret;
}

#endif

void tgroup::interior_from_str_internal(const std::string& str, int* interiors)
{
	int n;
	for (n = 0; n < tgroup::interior_count; n ++) {
		interiors[n] = 0;
	}
	const std::vector<std::string> vstr = utils::split(str);
	if ((int)vstr.size() < tgroup::interior_count) {
		return;
	}
	n = 0;
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end() && n < tgroup::interior_count; ++ it) {
		interiors[n ++] = lexical_cast_default<int>(*it);
	}
}

void tgroup::interior_from_str(const std::string& str)
{
	interior_from_str_internal(str, interiors_);
}

std::string tgroup::interior_to_str() const
{
	std::stringstream strstr;
	for (int n = 0; n < interior_count; n ++) {
		if (n) {
			strstr << ",";
		}
		strstr << interiors_[n];
	}
	return strstr.str();
}

int tgroup::message_count() const
{
	int ret = 0;
	for (int n = 0; n < tgroup::msg_count; n ++) {
		ret += messages_[n];
	}
	return ret;
}

void tgroup::message_from_str(const std::string& str)
{
	int n;
	browsed_ = time(NULL);
	for (n = 0; n < tgroup::msg_count; n ++) {
		messages_[n] = 0;
	}
	const std::vector<std::string> vstr = utils::split(str);
	if ((int)vstr.size() < 1 + tgroup::msg_count) {
		return;
	}
	browsed_ = lexical_cast_default<time_t>(vstr[0]);
	n = 0;
	std::vector<std::string>::const_iterator it = vstr.begin();
	for (std::advance(it, 1); it != vstr.end() && n < tgroup::msg_count; ++ it) {
		messages_[n ++] = lexical_cast_default<int>(*it);
	}
}

void tgroup::signin_from_str(const std::string& str)
{
	const std::vector<std::string> vstr = utils::split(str);
	if ((int)vstr.size() < 3) {
		return;
	}
	signin_.today = lexical_cast_default<int>(vstr[0]) > 0;
	signin_.continue_days = lexical_cast_default<int>(vstr[1]);
	signin_.break_days = lexical_cast_default<int>(vstr[2]);
}

std::string tgroup::signin_to_str() const
{
	std::stringstream strstr;
	strstr << (signin_.today? 1: 0) << "," << signin_.continue_days << "," << signin_.break_days;

	return strstr.str();
}

const config& tgroup::to_faction_cfg(bool exile, bool multigrid)
{
	if (!faction_cfg_.empty()) {
		return faction_cfg_;
	}

	config& cfg = faction_cfg_;
	cfg["leader"] = leader_->number_;
	
	std::stringstream strstr, tower_strstr;
	int number = leader_->number_;
	strstr << number;
	tower_strstr << number;
	int count = 1;
	for (std::vector<tmember>::const_iterator it = members_.begin(); it != members_.end(); ++ it) {
		number = it->h->number_;
		strstr << "," << number;
		if (count < game_config::tower_fix_heros) {
			tower_strstr << "," << number;
		}
		count ++;
	}
	if (exile) {
		std::set<int> numbers;
		for (std::vector<tmember>::const_iterator it = exiles_.begin(); it != exiles_.end(); ++ it) {
			numbers.insert(it->h->number_);
		}
		int require_extract = numbers.size() / 2;
		while (require_extract > 0) {
			std::set<int>::iterator it = numbers.begin();
			std::advance(it, rand() % numbers.size());
			number = *it;
			strstr << "," << number;
			if (count < game_config::tower_fix_heros) {
				tower_strstr << "," << number;
			}
			count ++;
			numbers.erase(it);
			require_extract --;
		}
	}
	cfg["service_heros"] = strstr.str();
	cfg["tower_heros"] = tower_strstr.str();
	cfg["wander_heros"] = "";
	config& sub = cfg.add_child("artifical");
	sub["heros_army"] = city_->number_;
	if (multigrid) {
		sub["type"] = unit_types.find_city(1)->id();
	} else {
		sub["type"] = "forest1";
	}

	return faction_cfg_;
}

void tgroup::to_side_cfg(config& side, int random, int max_service_member) const
{
	side["leader"] = leader_->number_;
	config& sub_s = side.child("artifical");
	sub_s["heros_army"] = city_->number_;

	std::stringstream strstr;
	int number = leader_->number_;
	strstr << number;
	int count = 0;
	for (std::vector<tgroup::tmember>::const_iterator it = members_.begin(); it != members_.end(); ++ it) {
		if (!it->base->get_flag(hero_flag_employee)) {
			if (count >= max_service_member) {
				continue;
			}
			count ++;
		}
		number = it->h->number_;
		strstr << ", " << number;
	}
	if (count < max_service_member) {
		std::set<int> numbers;
		for (std::vector<tmember>::const_iterator it = exiles_.begin(); it != exiles_.end(); ++ it) {
			numbers.insert(it->h->number_);
		}
		int require_extract = numbers.size() / 2;
		while (require_extract > 0 && count < max_service_member) {
			std::set<int>::iterator it = numbers.begin();
			std::advance(it, (random * 2) % numbers.size());
			number = *it;
			strstr << "," << number;
			numbers.erase(it);
			require_extract --;
			count ++;
		}
	}
	sub_s["service_heros"] = strstr.str();
	sub_s["wander_heros"] = "";
}

void tgroup::reload_heros_from_string(hero_map& heros, const std::string& member_str, const std::string& exile_str)
{
	member_from_str(heros, member_str);
	exile_from_str(heros, exile_str);
}

void tgroup::member_from_str(hero_map& heros, const std::string& str, bool erase)
{
	// clear previous state variable.
	members_.clear();
	faction_cfg_.clear();
	if (erase) {
		for (size_t i = heros.size() - 1; i > hero_map::map_size_from_dat; i --) {
			heros.erase(i);
		}
	}

	const std::vector<std::string> vstr = utils::split(str);
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		int val = lexical_cast_default<int>(*it);
		int level = (val & 0xffff0000) >> 16;
		int number = val & 0x0000ffff;
		insert_member(heros, number, level);
	}
}

void tgroup::member_from_str2(hero_map& heros, const std::string& str)
{
	const std::vector<std::string> vstr = utils::split(str);
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		int val = lexical_cast_default<int>(*it);
		int level = (val & 0xffff0000) >> 16;
		int number = val & 0x0000ffff;
		++ it;

		hero& base = heros[number];
		hero& h = heros[lexical_cast_default<int>(*it)];
		
		tmember m;
		m.base = &base;
		m.level = level;
		m.h = &h;
		members_.push_back(m);
	}
}

// must call member_from_str before exile_from_str! (while erase == true)
void tgroup::exile_from_str(hero_map& heros, const std::string& str)
{
	// clear previous state variable.
	exiles_.clear();

	const std::vector<std::string> vstr = utils::split(str);
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		int val = lexical_cast_default<int>(*it);
		int level = (val & 0xffff0000) >> 16;
		int number = val & 0x0000ffff;
		insert_exile(heros, number, level);
	}
}

void tgroup::exile_from_str2(hero_map& heros, const std::string& str)
{
	const std::vector<std::string> vstr = utils::split(str);
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		int val = lexical_cast_default<int>(*it);
		int level = (val & 0xffff0000) >> 16;
		int number = val & 0x0000ffff;
		++ it;

		hero& base = heros[number];
		hero& h = heros[lexical_cast_default<int>(*it)];
		
		tmember m;
		m.base = &base;
		m.level = level;
		m.h = &h;
		exiles_.push_back(m);
	}
}

std::string tgroup::member_to_str_internal(const std::vector<tgroup::tmember>& v)
{
	std::stringstream strstr;
	for (std::vector<tmember>::const_iterator it = v.begin(); it != v.end(); ++ it) {
		const tgroup::tmember& m = *it;
		if (it != v.begin()) {
			strstr << ",";
		}
		int val = (m.level << 16) | m.base->number_;
		strstr << val;
	}
	return strstr.str();
}

std::string tgroup::member_to_str2_internal(const std::vector<tgroup::tmember>& v)
{
	std::stringstream strstr;
	for (std::vector<tmember>::const_iterator it = v.begin(); it != v.end(); ++ it) {
		const tgroup::tmember& m = *it;
		if (it != v.begin()) {
			strstr << ",";
		}
		int val = (m.level << 16) | m.base->number_;
		strstr << val;
		val = m.h->number_;
		strstr << "," << val;
	}
	return strstr.str();
}

std::string tgroup::member_to_str() const
{
	return member_to_str_internal(members_);
}

std::string tgroup::member_to_str2() const
{
	return member_to_str2_internal(members_);
}

std::string tgroup::exile_to_str() const
{
	return member_to_str_internal(exiles_);
}

std::string tgroup::exile_to_str2() const
{
	return member_to_str2_internal(exiles_);
}

std::vector<tgroup::tassociate> tgroup::associate_from_str2(const std::string& str)
{
	std::vector<tassociate> ret;

	const std::vector<std::string> vstr = utils::split(str, '|');
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		std::vector<std::string> fields = utils::split(*it);
		if (fields.size() == 4) {
			ret.push_back(tassociate(fields[0], lexical_cast_default<int>(fields[1]), lexical_cast_default<int>(fields[2]), lexical_cast_default<time_t>(fields[3])));
		}
	}

	return ret;
}

void tgroup::associate_from_str(const std::string& str)
{
	associates_ = associate_from_str2(str);
}

std::string tgroup::associate_to_str() const
{
	std::stringstream strstr;
	for (std::vector<tgroup::tassociate>::const_iterator it = associates_.begin(); it != associates_.end(); ++ it) {
		const tassociate& a = *it;
		if (it != associates_.begin()) {
			strstr << "|";
		}
		int t = a.t;
		strstr << a.username << "," << a.uid << "," << a.agreement << "," << t;
	}
	return strstr.str();
}

std::vector<tgroup::tassociate> tgroup::associates_from_agreement(int agreement) const
{
	std::vector<tassociate> ret;
	for (std::vector<tassociate>::const_iterator it = associates_.begin(); it != associates_.end(); ++ it) {
		if (agreement == it->agreement) {
			ret.push_back(*it);
		}
	}
	return ret;
}

std::vector<tgroup::tassociate> tgroup::associates_from_agreement(std::set<int>& agreement) const
{
	std::vector<tassociate> ret;
	for (std::vector<tassociate>::const_iterator it = associates_.begin(); it != associates_.end(); ++ it) {
		if (agreement.find(it->agreement) != agreement.end()) {
			ret.push_back(*it);
		}
	}
	return ret;
}

const tgroup::tassociate& tgroup::associate(int uid) const
{
	for (std::vector<tassociate>::const_iterator it = associates_.begin(); it != associates_.end(); ++ it) {
		if (uid == it->uid) {
			return *it;
		}
	}

	static tassociate null_a(null_str, -1, tassociate::none, 0);
	return null_a;
}

size_t tgroup::fp_size() const
{
	size_t s = 3 * sizeof(int); // leader's number, uid, city's number
	s += sizeof(int) + leader_->name().size();
	s += sizeof(int) + leader_->surname().size();
	s += sizeof(int) + city_->name().size();
	s += sizeof(int) * interior_count;
	s += sizeof(int) + member_to_str2().size();
	s += sizeof(int) + exile_to_str2().size();
	s += sizeof(int) + associate_to_str().size();
	s += sizeof(int) + layout_.size();

	return s;
}

int tgroup::to_fp(uint8_t* mem) const
{
	int pos = 0;

	int value = leader_->number_;
	memcpy(mem + pos, &value, 4);
	pos += 4;
	value = leader_->uid();
	memcpy(mem + pos, &value, 4);
	pos += 4;
	value = city_->number_;
	memcpy(mem + pos, &value, 4);
	pos += 4;

	value = leader_->name().size();
	memcpy(mem + pos, &value, 4);
	pos += 4;
	memcpy(mem + pos, leader_->name().c_str(), leader_->name().size());
	pos += leader_->name().size();

	value = leader_->surname().size();
	memcpy(mem + pos, &value, 4);
	pos += 4;
	memcpy(mem + pos, leader_->surname().c_str(), leader_->surname().size());
	pos += leader_->surname().size();

	value = city_->name().size();
	memcpy(mem + pos, &value, 4);
	pos += 4;
	memcpy(mem + pos, city_->name().c_str(), city_->name().size());
	pos += city_->name().size();

	memcpy(mem + pos, interiors_, sizeof(int) * interior_count);
	pos += sizeof(int) * interior_count;

	std::string str = member_to_str2();
	value = str.size();
	memcpy(mem + pos, &value, 4);
	pos += 4;
	memcpy(mem + pos, str.c_str(), str.size());
	pos += str.size();

	str = exile_to_str2();
	value = str.size();
	memcpy(mem + pos, &value, 4);
	pos += 4;
	memcpy(mem + pos, str.c_str(), str.size());
	pos += str.size();

	str = associate_to_str();
	value = str.size();
	memcpy(mem + pos, &value, 4);
	pos += 4;
	memcpy(mem + pos, str.c_str(), str.size());
	pos += str.size();

	value = layout_.size();
	memcpy(mem + pos, &value, 4);
	pos += 4;
	memcpy(mem + pos, layout_.c_str(), layout_.size());
	pos += layout_.size();
	
	return pos;
}

int tgroup::from_fp(hero_map& heros, uint8_t* mem)
{
	int number, uid, len;
	std::string str;
	int pos = 0;
	
	memcpy(&number, mem + pos, sizeof(int));
	pos += 4;
	hero& leader = heros[number];
	memcpy(&uid, mem + pos, sizeof(int));
	pos += 4;
	leader.set_uid(uid);
	memcpy(&number, mem + pos, sizeof(int));
	pos += 4;
	hero& city = heros[number];

	set_leader(leader);
	set_city(city);

	memcpy(&len, mem + pos, sizeof(int));
	pos += 4;
	str.assign((char*)mem + pos, len);
	pos += str.size();
	leader.set_name(str);

	memcpy(&len, mem + pos, sizeof(int));
	pos += 4;
	str.assign((char*)mem + pos, len);
	pos += str.size();
	leader.set_surname(str);

	memcpy(&len, mem + pos, sizeof(int));
	pos += 4;
	str.assign((char*)mem + pos, len);
	pos += str.size();
	city.set_name(str);

	memcpy(interiors_, mem + pos, sizeof(int) * interior_count);
	pos += sizeof(int) * interior_count;

	memcpy(&len, mem + pos, sizeof(int));
	pos += 4;
	str.assign((char*)mem + pos, len);
	pos += str.size();
	member_from_str2(heros, str);

	memcpy(&len, mem + pos, sizeof(int));
	pos += 4;
	str.assign((char*)mem + pos, len);
	pos += str.size();
	exile_from_str2(heros, str);

	memcpy(&len, mem + pos, sizeof(int));
	pos += 4;
	str.assign((char*)mem + pos, len);
	pos += str.size();
	associate_from_str(str);

	memcpy(&len, mem + pos, sizeof(int));
	pos += 4;
	layout_.assign((char*)mem + pos, len);
	pos += layout_.size();

	return pos;
}

// 1. original hero_map must valid.
// 2. original and now hero_map must same hero size, and same hero number.
void tgroup::redirect_hero_map(hero_map& heros)
{
	hero& now_leader = heros[leader_->number_];
	now_leader.set_name(leader_->name());
	now_leader.set_surname(leader_->surname());
	now_leader.set_uid(leader_->uid());
	leader_ = &now_leader;
	
	hero& now_city = heros[city_->number_];
	now_city.set_name(city_->name());
	city_ = &now_city;

	for (std::vector<tmember>::iterator it = members_.begin(); it != members_.end(); ++ it) {
		tmember& m = *it;
		m.base = &heros[m.base->number_];
		m.h = &heros[m.h->number_];
	}
	for (std::vector<tmember>::iterator it = exiles_.begin(); it != exiles_.end(); ++ it) {
		tmember& m = *it;
		m.base = &heros[m.base->number_];
		m.h = &heros[m.h->number_];
	}
}