#include "global.hpp"
#include "hero.hpp"
#include "gettext.hpp"
#include "filesystem.hpp"
#include "wml_exception.hpp"
#include "formula_string_utils.hpp"
#include "game_config.hpp"
#include "integrate.hpp"

hero hero_invalid = hero(HEROS_INVALID_NUMBER);

hero_map::iterator hero_map_iter_invalid = hero_map::iterator(HEROS_INVALID_NUMBER, NULL);
hero_map::const_iterator hero_map_const_iter_invalid = hero_map::const_iterator(HEROS_INVALID_NUMBER, NULL);

std::string hero_::image_file_root_ = "";
std::string hero_::gender_str_[HEROS_MAX_GENDER] = {};
std::string hero_::arms_str_[HEROS_MAX_ARMS] = {};
std::string hero_::skill_str_[HEROS_MAX_SKILL] = {};
std::string hero_::feature_str_[HEROS_MAX_FEATURE] = {};
std::string hero_::feature_desc_str_[HEROS_MAX_FEATURE] = {};
std::string hero_::stratum_str_[HEROS_STRATUMS] = {};
std::string hero_::status_str_[HEROS_STATUSES] = {};
std::string hero_::official_str_[HEROS_OFFICIALS] = {};
std::string hero_::flag_str_[HEROS_FLAGS] = {};
std::vector<int> hero_::valid_features_;

int hero_::number_system = 0;
int hero_::number_civilian = 1;
int hero_::number_empty_leader = 2;
int hero_::number_scout = 3;
int hero_::number_system_min = 0;
int hero_::number_system_max = 9;

int hero_::number_soldier_min = 10;
int hero_::number_soldier_max = 19;

int hero_::number_market = 20;
int hero_::number_wall = 21;
int hero_::number_keep = 22;
int hero_::number_tower = 23;
int hero_::number_technology = 24;
int hero_::number_tactic = 25;
int hero_::number_fort = 26;
int hero_::number_school = 27;
int hero_::number_artifical_min = 20;
int hero_::number_artifical_max = 34;

int hero_::number_businessman = 35;
int hero_::number_scholar = 36;
int hero_::number_transport = 37;
int hero_::number_commoner_min = 35;
int hero_::number_commoner_max = 49;

int hero_::number_local_player_city = 95;
int hero_::number_network_player_city_min = 96;
int hero_::number_network_player_city_max = 99;

int hero_::number_city_min = 50;
int hero_::number_normal_min = 100;
int hero_::number_employee_min = 430;

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

hero_::hero_(const hero_& that)
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

void hero_::clear_relation()
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

hero_::hero_(uint16_t number, uint16_t leadership, uint16_t force, uint16_t intellect, uint16_t spirit, uint16_t charm)
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

hero_::hero_(const uint8_t* mem)
	: name_str_()
	, surname_str_()
	, uid_(-1)
	, biography_str_()
	, imgfile_()
	, imgfile2_()
{
	read(mem);
}

void hero_::write(uint8_t* mem)
{
	hero_fields_t* parent = (hero_fields_t*)this;
	memcpy(mem, parent, sizeof(hero_fields_t));
}

void hero_::read(const uint8_t* mem)
{
	hero_fields_t* parent = (hero_fields_t*)this;
	memcpy(parent, mem, sizeof(hero_fields_t));
}

const bool hero_::valid() const
{
	if (number_ == HEROS_INVALID_NUMBER){
		return false;
	}
	return true;
}

bool hero_::is_parent(const hero_& h, uint32_t* index) const
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

bool hero_::is_consort(const hero_& h, uint32_t* index) const
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

bool hero_::is_oath(const hero_& h, uint32_t* index) const
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

bool hero_::is_intimate(const hero_& h, uint32_t* index) const
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

bool hero_::is_hate(const hero_& h, uint32_t* index) const
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
int hero_::increase_catalog(int inc, const hero_& leader)
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

int hero_::loyalty(const hero_& leader) const
{
	int mid = posix_abs((int)leader.float_catalog_ - float_catalog_);
	mid = fxptoi8(mid);
	if (mid < HERO_MAX_LOYALTY / 2) {
		mid = HERO_MAX_LOYALTY - mid;
	}
	return mid;
}

int hero_::base_loyalty(const hero_& leader) const
{
	int mid = posix_abs((int)leader.base_catalog_ - base_catalog_);
	if (mid < HERO_MAX_LOYALTY / 2) {
		mid = HERO_MAX_LOYALTY - mid;
	}
	return mid;
}

#define INC_CATALOG_HEART_RATIO		20
// @inc: >0, decrease loyalty; < 0, increase loyalty
int hero_::increase_loyalty(int inc, hero_& leader)
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

void hero_::set_loyalty(const hero_& leader, int level, bool fixed)
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

void hero_::set_loyalty2(hero_& leader, int level, bool fixed)
{
	if (base_catalog_ != leader.base_catalog_) {
		set_loyalty(leader, level, fixed);
	} else {
		float_catalog_ = ftofxp8(base_catalog_);
	}
}

//
// set both base_catalog and float_catalog;
void hero_::set_loyalty3(const hero_& leader, int level)
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

std::string hero_::image_from_uid(int uid, int gender, bool big)
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

const char* hero_::image(bool big)
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

void hero_::set_image(int image)
{
	image_ = image;
	imgfile_ = "";
	imgfile2_ = "";
}

void hero_::set_uid(int _uid) 
{ 
	uid_ = _uid;
	imgfile_ = "";
	imgfile2_ = "";
}

std::string& hero_::name()
{
	if (name_str_.empty()) {
		char text[_MAX_PATH];
		sprintf(text, "%s%u", HERO_PREFIX_STR_NAME, name_);
		name_str_ = dgettext("wesnoth-hero", text);
	}
	return name_str_;
}

void hero_::set_name(const std::string& name)
{
	name_str_ = name;
}

std::string& hero_::surname()
{
	if (surname_str_.empty()) {
		char text[_MAX_PATH];
		sprintf(text, "%s%u", HERO_PREFIX_STR_SURNAME, surname_);
		surname_str_ = dgettext("wesnoth-hero", text);
	}
	return surname_str_;
}

void hero_::set_surname(const std::string& surname)
{
	surname_str_ = surname;
}

// if caller want use retval in furture, caller need create copy.
const char* hero_::biography()
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

std::string hero_::biography2(const hero_map& heros)
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
	symbols["leader"] = tintegrate::generate_format(heros[player_].name(), "green");
	strstr << vgettext("wesnoth-lib", "Hero that belong to $leader.", symbols) << "\n";
	strstr << biography();

	return strstr.str();
}

void hero_::set_biography(const std::string& str)
{
	biography_str_ = str;
}

std::string hero_::heart_str()
{
	char text[_MAX_PATH];
	sprintf(text, "%u", heart_);
	return text;
}

const std::string& hero_::flag_str(int flag)
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

void hero_::set_flag(int flag)
{
	if (flag >= HEROS_FLAGS) {
		return;
	}
	flags_ |= 1 << flag; 
}

void hero_::clear_flag(int flag)
{
	if (flag >= HEROS_FLAGS) {
		return;
	}
	flags_ &= ~(1 << flag);
}

bool hero_::get_flag(int flag) const
{
	if (flag >= HEROS_FLAGS) {
		return false;
	}
	return flags_ & (1 << flag);
}

const std::string& hero_::stratum_str(int stratum)
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

const std::string& hero_::status_str(int status)
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

const std::string& hero_::official_str(int official)
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

const std::string& hero_::feature_str(int feature)
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

const std::string& hero_::feature_desc_str(int feature)
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

std::vector<int>& hero_::valid_features()
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

std::string& hero_::gender_str(int gender)
{
	if (gender_str_[gender].empty()) {
		char text[_MAX_PATH];
		sprintf(text, "%s%u", HERO_PREFIX_STR_GENDER, gender);
		gender_str_[gender] = dgettext("wesnoth-hero", text);
	}
	return gender_str_[gender];
}

std::string& hero_::arms_str(int arms)
{
	if (arms_str_[arms].empty()) {
		char text[_MAX_PATH];
		sprintf(text, "%s%u", HERO_PREFIX_STR_ARMS, arms);
		arms_str_[arms] = dgettext("wesnoth-hero", text);
	}
	return arms_str_[arms];
}

std::string& hero_::skill_str(int skill)
{
	if (skill_str_[skill].empty()) {
		char text[_MAX_PATH];
		sprintf(text, "%s%u", HERO_PREFIX_STR_SKILL, skill);
		skill_str_[skill] = dgettext("wesnoth-hero", text);
	}
	return skill_str_[skill];
}

std::string hero_::adaptability_str2(uint16_t adaptability)
{
	char text[_MAX_PATH];
	sprintf(text, "%s%u", HERO_PREFIX_STR_ADAPTABILITY, fxptoi12(adaptability));
	return dgettext("wesnoth-hero", text);
}

bool hero_::is_system(int h)
{
	return h >= number_system_min && h <= number_system_max;
}

bool hero_::is_soldier(int h)
{
	return h >= number_soldier_min && h <= number_soldier_max;
}

bool hero_::is_artifical(int h)
{
	return h >= number_artifical_min && h <= number_artifical_max;
}

bool hero_::is_ea_artifical(int h)
{
	return h == number_market || h == number_technology || h == number_tactic || h == number_school;
}

bool hero_::is_commoner(int h)
{
	return h >= number_commoner_min && h <= number_commoner_max;
}

bool hero_::data_variable(int h)
{
	return h >= number_city_min;
}

void hero_::adjust_according_to_level(const hero_& base, int level, const hero_& leader)
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

bool hero_::is_roam_member() const
{
	if (get_flag(hero_flag_roam)) {
		return true;
	}
	if (leadership_ >= ftofxp9(90) || charm_ >= ftofxp9(90)) {
		return true;
	}
	return false;
}

void hero_::field_set_minmum()
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

std::string hero_::field_to_str()
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

int hero_::gender_from_filed_str(const std::string& str)
{
	size_t count = 3 + 5 + HEROS_MAX_ARMS + HEROS_MAX_SKILL + 4 + 2;
	
	const std::vector<std::string> fields = utils::split(str, '|', utils::STRIP_SPACES);
	if (fields.size() != count) {
		return hero_gender_male;
	}
	return lexical_cast_default<int>(fields[2]);
}

void hero_::field_from_str(const std::string& str)
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

std::string hero_::translatable_to_str()
{
	std::stringstream strstr;

	strstr << surname();
	strstr << "|" << biography_str_;

	return strstr.str();
}

void hero_::translatable_from_str(const std::string& str)
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

bool hero_::get_xp(const hblock& hb)
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

bool hero_::can_goto_hate(const hero_& h) const
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

void hero_::do_hate_relation(hero_& to, bool set)
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
			if (!confirm_carry_to(to, carry_to)) {
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

		if (!h.check_valid()) {
			std::stringstream strstr;
			strstr << h.name() << "'s set is invalid!";
			posix_print_mb(utf8_2_ansi(strstr.str().c_str()));
		}
		
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
		hero& h = *map_[idx];

		h.set_name(null_str);
		h.side_ = HEROS_INVALID_SIDE;
		h.city_ = HEROS_ROAM_CITY;
		h.status_ = HEROS_DEFAULT_STATUS;
		h.activity_ = HEROS_DEFAULT_ACTIVITY;

		// It mean next scenario will clear commercials.
		h.official_ = HEROS_DEFAULT_OFFICIAL;
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

//
// group
//
tgroup_::tmember tgroup_::null_member;

void tgroup_::reset()
{
	leader_ = NULL;
	members_.clear();
	exiles_.clear();
	city_ = NULL;

	layout_.clear();
	map_.clear();
	faction_cfg_.clear();
}

bool tgroup_::valid() const
{
	return leader_? true: false;
}

tgroup_::tmember& tgroup_::member(const hero& h)
{
	for (std::vector<tgroup_::tmember>::iterator it = members_.begin(); it != members_.end(); ++ it) {
		if (it->h == &h) {
			return *it;
		}
	}
	VALIDATE(false, "group::member, cannot find member corresponding to this h!");
	return null_member;
}

const tgroup_::tmember& tgroup_::member(const hero& h) const
{
	for (std::vector<tgroup_::tmember>::const_iterator it = members_.begin(); it != members_.end(); ++ it) {
		if (it->h == &h) {
			return *it;
		}
	}
	VALIDATE(false, "group::member, cannot find member corresponding to this h!");
	return null_member;
}

const tgroup_::tmember& tgroup_::exile(const hero& h) const
{
	for (std::vector<tgroup_::tmember>::const_iterator it = exiles_.begin(); it != exiles_.end(); ++ it) {
		if (it->h == &h) {
			return *it;
		}
	}
	VALIDATE(false, "group::exile, cannot find exile corresponding to this h!");
	return null_member;
}

tgroup_::tmember& tgroup_::member_from_base(const hero& h)
{
	for (std::vector<tgroup_::tmember>::iterator it = members_.begin(); it != members_.end(); ++ it) {
		if (it->base == &h) {
			return *it;
		}
	}
	VALIDATE(false, "group::member, cannot find member corresponding to this h!");
	return null_member;
}

const tgroup_::tmember& tgroup_::member_from_base(const hero& h) const
{
	for (std::vector<tgroup_::tmember>::const_iterator it = members_.begin(); it != members_.end(); ++ it) {
		if (it->base == &h) {
			return *it;
		}
	}
	VALIDATE(false, "group::member, cannot find member corresponding to this h!");
	return null_member;
}

hero* tgroup_::exist_member(int number) const
{
	for (std::vector<tmember>::const_iterator it = members_.begin(); it != members_.end(); ++ it) {
		if (it->base->number_ == number) return it->h;
	}
	return NULL;
}

void tgroup_::insert_member(hero_map& heros, int number, int level)
{
	VALIDATE(!exist_member(number), "tgroup_::insert_member, duplicate number!");
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

void tgroup_::unstage_member(hero_map& heros, int number)
{
	std::vector<tmember>::iterator it = members_.begin();
	for (; it != members_.end(); ++ it) {
		if (it->h->number_ == number) {
			break;
		}
	}
	VALIDATE(it != members_.end(), "tgroup_::unstage_member, cannot find number!");

	heros[number].status_ = hero_status_unstage;
}

void tgroup_::adjust_members_according_to_leader(hero_map& heros)
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

void tgroup_::remove_unstage_member()
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

std::vector<tgroup_::tmember*> tgroup_::part_members(hero_map& heros, bool employee)
{
	std::vector<tgroup_::tmember*> ret;
	for (std::vector<tmember>::iterator it = members_.begin(); it != members_.end(); ++ it) {
		tgroup_::tmember& m = *it;
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

hero* tgroup_::exist_exile(int number) const
{
	for (std::vector<tmember>::const_iterator it = exiles_.begin(); it != exiles_.end(); ++ it) {
		if (it->base->number_ == number) return it->h;
	}
	return NULL;
}

void tgroup_::insert_exile(hero_map& heros, int number, int level)
{
	VALIDATE(!exist_exile(number), "tgroup_::insert_exile, duplicate number!");
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

void tgroup_::unstage_exile(hero_map& heros, int number)
{
	std::vector<tmember>::iterator it = exiles_.begin();
	for (; it != exiles_.end(); ++ it) {
		if (it->h->number_ == number) {
			break;
		}
	}
	VALIDATE(it != exiles_.end(), "tgroup_::unstage_exile, cannot find number!");

	heros[number].status_ = hero_status_unstage;
}

void tgroup_::set_map(const std::string& str)
{
	map_ = str;
}

void tgroup_::interior_from_str_internal(const std::string& str, int* interiors)
{
	int n;
	for (n = 0; n < tgroup_::interior_count; n ++) {
		interiors[n] = 0;
	}
	const std::vector<std::string> vstr = utils::split(str);
	if ((int)vstr.size() < tgroup_::interior_count) {
		return;
	}
	n = 0;
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end() && n < tgroup_::interior_count; ++ it) {
		interiors[n ++] = lexical_cast_default<int>(*it);
	}
}

void tgroup_::interior_from_str(const std::string& str)
{
	interior_from_str_internal(str, interiors_);
}

std::string tgroup_::interior_to_str() const
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

int tgroup_::message_count() const
{
	int ret = 0;
	for (int n = 0; n < tgroup_::msg_count; n ++) {
		ret += messages_[n];
	}
	return ret;
}

void tgroup_::message_from_str(const std::string& str)
{
	int n;
	browsed_ = time(NULL);
	for (n = 0; n < tgroup_::msg_count; n ++) {
		messages_[n] = 0;
	}
	const std::vector<std::string> vstr = utils::split(str);
	if ((int)vstr.size() < 1 + tgroup_::msg_count) {
		return;
	}
	browsed_ = lexical_cast_default<time_t>(vstr[0]);
	n = 0;
	std::vector<std::string>::const_iterator it = vstr.begin();
	for (std::advance(it, 1); it != vstr.end() && n < tgroup_::msg_count; ++ it) {
		messages_[n ++] = lexical_cast_default<int>(*it);
	}
}

void tgroup_::signin_from_str(const std::string& str)
{
	const std::vector<std::string> vstr = utils::split(str);
	if ((int)vstr.size() < 3) {
		return;
	}
	signin_.today = lexical_cast_default<int>(vstr[0]) > 0;
	signin_.continue_days = lexical_cast_default<int>(vstr[1]);
	signin_.break_days = lexical_cast_default<int>(vstr[2]);
}

std::string tgroup_::signin_to_str() const
{
	std::stringstream strstr;
	strstr << (signin_.today? 1: 0) << "," << signin_.continue_days << "," << signin_.break_days;

	return strstr.str();
}

void tgroup_::to_side_cfg(config& side, int random, int max_service_member) const
{
	side["leader"] = leader_->number_;
	config& sub_s = side.child("artifical");
	sub_s["heros_army"] = city_->number_;

	std::stringstream strstr;
	int number = leader_->number_;
	strstr << number;
	int count = 0;
	for (std::vector<tgroup_::tmember>::const_iterator it = members_.begin(); it != members_.end(); ++ it) {
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

void tgroup_::reload_heros_from_string(hero_map& heros, const std::string& member_str, const std::string& exile_str)
{
	member_from_str(heros, member_str);
	exile_from_str(heros, exile_str);
}

void tgroup_::member_from_str(hero_map& heros, const std::string& str, bool erase)
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

void tgroup_::member_from_str2(hero_map& heros, const std::string& str)
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
void tgroup_::exile_from_str(hero_map& heros, const std::string& str)
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

void tgroup_::exile_from_str2(hero_map& heros, const std::string& str)
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

std::string tgroup_::member_to_str_internal(const std::vector<tgroup_::tmember>& v)
{
	std::stringstream strstr;
	for (std::vector<tmember>::const_iterator it = v.begin(); it != v.end(); ++ it) {
		const tgroup_::tmember& m = *it;
		if (it != v.begin()) {
			strstr << ",";
		}
		int val = (m.level << 16) | m.base->number_;
		strstr << val;
	}
	return strstr.str();
}

std::string tgroup_::member_to_str2_internal(const std::vector<tgroup_::tmember>& v)
{
	std::stringstream strstr;
	for (std::vector<tmember>::const_iterator it = v.begin(); it != v.end(); ++ it) {
		const tgroup_::tmember& m = *it;
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

std::string tgroup_::member_to_str() const
{
	return member_to_str_internal(members_);
}

std::string tgroup_::member_to_str2() const
{
	return member_to_str2_internal(members_);
}

std::string tgroup_::exile_to_str() const
{
	return member_to_str_internal(exiles_);
}

std::string tgroup_::exile_to_str2() const
{
	return member_to_str2_internal(exiles_);
}

std::vector<tgroup_::tassociate> tgroup_::associate_from_str2(const std::string& str)
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

void tgroup_::associate_from_str(const std::string& str)
{
	associates_ = associate_from_str2(str);
}

std::string tgroup_::associate_to_str() const
{
	std::stringstream strstr;
	for (std::vector<tgroup_::tassociate>::const_iterator it = associates_.begin(); it != associates_.end(); ++ it) {
		const tassociate& a = *it;
		if (it != associates_.begin()) {
			strstr << "|";
		}
		int t = a.t;
		strstr << a.username << "," << a.uid << "," << a.agreement << "," << t;
	}
	return strstr.str();
}

std::vector<tgroup_::tassociate> tgroup_::associates_from_agreement(int agreement) const
{
	std::vector<tassociate> ret;
	for (std::vector<tassociate>::const_iterator it = associates_.begin(); it != associates_.end(); ++ it) {
		if (agreement == it->agreement) {
			ret.push_back(*it);
		}
	}
	return ret;
}

std::vector<tgroup_::tassociate> tgroup_::associates_from_agreement(std::set<int>& agreement) const
{
	std::vector<tassociate> ret;
	for (std::vector<tassociate>::const_iterator it = associates_.begin(); it != associates_.end(); ++ it) {
		if (agreement.find(it->agreement) != agreement.end()) {
			ret.push_back(*it);
		}
	}
	return ret;
}

const tgroup_::tassociate& tgroup_::associate(int uid) const
{
	for (std::vector<tassociate>::const_iterator it = associates_.begin(); it != associates_.end(); ++ it) {
		if (uid == it->uid) {
			return *it;
		}
	}

	static tassociate null_a(null_str, -1, tassociate::none, 0);
	return null_a;
}

size_t tgroup_::fp_size() const
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

int tgroup_::to_fp(uint8_t* mem) const
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

int tgroup_::from_fp(hero_map& heros, uint8_t* mem)
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
void tgroup_::redirect_hero_map(hero_map& heros)
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