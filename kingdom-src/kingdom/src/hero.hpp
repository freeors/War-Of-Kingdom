#ifndef __HERO_HPP_
#define __HERO_HPP_

#include "posix.h"
#include <string>
#include <vector>
#include <set>
#include <map>

// 以下函数用于
// 1. 在实际值和显示值之间进行转换。存储在内存中的是fixed_t，显示值是float或int
// 2. 得到取整值。fxptoi8(x)
typedef uint16_t fixed16_t;
#define fxp8_shift 8
#define fxp8_base 256	// (1 << fxp8_shift)
#define fxp8_mask 255	// (fxp8_base - 1)

// IN: float or int - OUT: fixed_t 
#define ftofxp8(x) (fixed16_t((x) * fxp8_base))

// IN: unsigned and fixed_t - OUT: unsigned 
#define fxpmult8(x, y) (((x)*(y)) >> fxp8_shift)

// IN: unsigned and int - OUT: fixed_t 
#define fxpdiv8(x, y) (((x) << fxp8_shift) / (y))

// IN: fixed_t - OUT: int 
#define fxptoi8(x) ( ((x)>0) ? ((x) >> fxp8_shift) : (-((-(x)) >> fxp8_shift)) )

// IN: fixed_t - OUT: fixed % fxp9_base (no signal)
#define fxpmod8(x) ((x) & fxp8_mask)

// IN: fixed_t - OUT: fixed % fxp9_base (with signal)
#define fxpsmod8(x) ( ((x)>0) ? ((x) & fxp8_mask) : (-((x) & fxp8_mask)) )

//
// 9
//
#define fxp9_shift 9
#define fxp9_base 512	// (1 << fxp9_shift)
#define fxp9_mask 511	// (fxp9_base - 1)

// IN: float or int - OUT: fixed_t 
#define ftofxp9(x) (fixed16_t((x) * fxp9_base))

// IN: unsigned and fixed_t - OUT: unsigned 
#define fxpmult9(x, y) (((x)*(y)) >> fxp9_shift)

// IN: unsigned and int - OUT: fixed_t 
#define fxpdiv9(x, y) (((x) << fxp9_shift) / (y))

// IN: fixed_t - OUT: int 
#define fxptoi9(x) ( ((x)>0) ? ((x) >> fxp9_shift) : (-((-(x)) >> fxp9_shift)) )

// IN: fixed_t - OUT: fixed % fxp9_base (no signal)
#define fxpmod9(x) ((x) & fxp9_mask)

// IN: fixed_t - OUT: fixed % fxp9_base (with signal)
#define fxpsmod9(x) ( ((x)>0) ? ((x) & fxp9_mask) : (-((x) & fxp9_mask)) )


//
// 12
// 
#define fxp12_shift 12
#define fxp12_base 4096	// (1 << fxp12_shift)
#define fxp12_mask 4095	// (fxp12_base - 1)

// IN: float or int - OUT: fixed_t 
#define ftofxp12(x) (fixed16_t((x) * fxp12_base))

// IN: unsigned and fixed_t - OUT: unsigned 
#define fxpmult12(x, y) (((x)*(y)) >> fxp12_shift)

// IN: unsigned and int - OUT: fixed_t 
#define fxpdiv12(x, y) (((x) << fxp12_shift) / (y))

// IN: fixed_t - OUT: int 
#define fxptoi12(x) ( ((x)>0) ? ((x) >> fxp12_shift) : (-((-(x)) >> fxp12_shift)) )

// IN: fixed_t - OUT: fixed % fxp12_base (no signal)
#define fxpmod12(x) ((x) & fxp12_mask)

// IN: fixed_t - OUT: fixed % fxp12_base (with signal)
#define fxpsmod12(x) ( ((x)>0) ? ((x) & fxp12_mask) : (-((x) & fxp12_mask)) )

bool u16_get_experience_i9(uint16_t* field, uint16_t inc_xp);

bool u16_get_experience_i12(uint16_t* field, uint16_t inc_xp);

class hero_map;
class config;
class unit;
#if defined(_KINGDOM_EXE) || !defined(_WIN32)
class unit_map;
class team;
class vconfig;
#endif

// 最大8192个武将
// 256 + 64 + 704 = 1024
#define HEROS_MAX_HEROS			8192
#define HEROS_BYTES_PER_HERO	240
#define HEROS_FILE_PREFIX_BYTES	16

#define HEROS_INVALID_NUMBER	0xffff
#define HEROS_INVALID_SIDE		0xff
#define HEROS_DEFAULT_CITY		0
#define HEROS_DEFAULT_STATUS	hero_status_unstage
#define HEROS_NO_FEATURE		0xff

#define HEROS_NO_OFFICIAL		0xff
#define HEROS_DEFAULT_OFFICIAL	HEROS_NO_OFFICIAL

#define HEROS_DEFAULT_STRATUM	hero_stratum_wander

#define HEROS_FULL_ACTIVITY		0xff
#define HEROS_DEFAULT_ACTIVITY	HEROS_FULL_ACTIVITY

#define HERO_PREFIX_STR_NAME	"name-"
#define HERO_PREFIX_STR_SURNAME	"surname-"

#define HEROS_MAX_GENDER	3
#define HERO_PREFIX_STR_GENDER	"gender-"
enum {
	hero_gender_male = 0,
	hero_gender_female,
	hero_gender_neutral,
	hero_gender_max = hero_gender_neutral,
};
#define HEROS_DEFAULT_GENDER	hero_gender_male

#define HERO_PREFIX_STR_STATUS	"status-"
enum {
	hero_status_idle = 0,
	hero_status_commanding,
	hero_status_moving,
	hero_status_backing,
	hero_status_military,
	hero_status_max_office = hero_status_military,
	hero_status_wander,
	hero_status_unstage,
	hero_status_die,
	HEROS_STATUSES,
};

#define HERO_PREFIX_STR_OFFICIAL "official-"
enum {
	hero_official_commercial = 0,
	hero_official_mayor,
	hero_official_leader,
	HEROS_OFFICIALS,
};

#define HERO_PREFIX_STR_STRATUM "stratum-"
enum {
	hero_stratum_wander = 0,
	hero_stratum_citizen,
	hero_stratum_mayor,
	hero_stratum_leader,
	HEROS_STRATUMS,
};

#define HERO_MAX_LOYALTY			150
#define HEROS_DEFAULT_BASE_CATALOG	(HERO_MAX_LOYALTY / 2)

#define HERO_PREFIX_STR_HEART		"heart-"
enum {
	hero_heart_0 = 0,
	hero_heart_1,
	hero_heart_2,
	hero_heart_3,
	hero_heart_4,
	hero_heart_max = hero_heart_4,
};
#define HEROS_DEFAULT_HEART			hero_heart_0

#define HERO_PREFIX_STR_AMBITION	"ambition-"
enum {
	hero_ambition_0 = 0,
	hero_ambition_1,
	hero_ambition_2,
	hero_ambition_3,
	hero_ambition_4,
	hero_ambition_max = hero_ambition_4,
};
#define HEROS_DEFAULT_AMBITION		hero_ambition_3

#define HEROS_MAX_ARMS	6
#define HERO_PREFIX_STR_ARMS		"arms-"
enum {
	hero_arms_min = 0,
	hero_arms_t0 = hero_arms_min,
	hero_arms_t1,
	hero_arms_t2,
	hero_arms_t3,
	hero_arms_t4,
	hero_arms_t5,
	hero_arms_max = hero_arms_t5,
};

// treasure
#define HEROS_MAX_TREASURE			256
#define HEROS_NO_TREASURE			0xff
#define HERO_PREFIX_STR_TREASURE	"treasure-"

// especial
#define NO_ESPECIAL				-1
#define HERO_PREFIX_STR_ESPECIAL	"especial-"

// unit type
#define HEROS_MAX_UTYPE				255
#define HEROS_NO_UTYPE				0xff

// skill
#define HEROS_MAX_SKILL	8
#define HERO_PREFIX_STR_SKILL		"skill-"
enum {
	hero_skill_min = 0,
	hero_skill_commercial = hero_skill_min,
	hero_skill_invent,
	hero_skill_t2,
	hero_skill_encourage,
	hero_skill_hero,
	hero_skill_demolish,
	hero_skill_t6,
	hero_skill_t7,
	hero_skill_max = hero_skill_t7,
};

#define HEROS_MAX_RANGE		4 // melee, ranged, cast, reserve
#define HEROS_MAX_FEATURE	128
#define HEROS_FEATURE_BYTES	16 // HEROS_MAX_FEATURE / 8
#define HEROS_FEATURE_M2BYTES	32 // HEROS_FEATURE_BYTES * 2
#define HEROS_BASE_FEATURE_COUNT	70
#define HERO_PREFIX_STR_FEATURE	"feature-"
#define HERO_PREFIX_STR_FEATURE_DESC	"feature_desc-"
enum {
	hero_feature_min		= 0,
	hero_feature_arms_min	= hero_feature_min,
	hero_feature_arms_max	= (HEROS_MAX_ARMS * 4 - 1),

	hero_feature_range_min	= hero_feature_arms_max + 1,
	hero_feature_range_max	= 29,

	hero_feature_pure = hero_feature_range_max + 1, // ---> 30
	hero_feature_guide,
	hero_feature_spirit,
	hero_feature_poison,
	hero_feature_diamond,
	hero_feature_shuttle,
	hero_feature_drain,
	hero_feature_firm,
	hero_feature_scout,
	hero_feature_skill,
	hero_feature_magnate,	// ---> 40
	hero_feature_ambush,
	hero_feature_concealment,
	hero_feature_entertainer,
	hero_feature_renown,
	hero_feature_innovate,
	hero_feature_surveillance,
	hero_feature_xp,
	hero_feature_lobbyist,
	hero_feature_break,
	hero_feature_penetrate,	// ---> 50
	hero_feature_fearless,
	hero_feature_healer,
	hero_feature_faction,
	hero_feature_interlink,
	hero_feature_submerge,
	hero_feature_dayattack,
	hero_feature_nightattack,
	hero_featrue_frighten,
	hero_featrue_indomitable,

	hero_feature_complex_min = HEROS_BASE_FEATURE_COUNT,

	hero_feature_max = HEROS_MAX_FEATURE - 1,
};

enum {
	hero_feature_single_result = 1,
	hero_feature_complex_result = 2,
};

#define HERO_PREFIX_STR_ADAPTABILITY	"adaptability-"
enum {
	hero_adaptability_min = 0,
	hero_adaptability_c = hero_adaptability_min,
	hero_adaptability_b,
	hero_adaptability_a,
	hero_adaptability_s,
	hero_adaptability_s1,
	hero_adaptability_s2,
	hero_adaptability_s3,
	hero_adaptability_s4,
	hero_adaptability_s5,
	hero_adaptability_s6,
	hero_adaptability_s7,
	hero_adaptability_s8,
	hero_adaptability_s9,
	hero_adaptability_s10,
	hero_adaptability_s11,
	hero_adaptability_s12,
	hero_adaptability_max = hero_adaptability_s12,
};

#define HERO_PREFIX_STR_BIOGRAPHY	"desc"

#define HERO_MAX_FEELING		65536

#define HEROS_MAX_PARENT	2
#define HEROS_MAX_CONSORT	3
#define HEROS_MAX_OATH		3
#define HEROS_MAX_INTIMATE	4
#define HEROS_MAX_HATE		4

#define HEROS_NO_TACTIC			0xff
#define HERO_PREFIX_STR_TACTIC	"tactic-"
#define HERO_PREFIX_STR_TACTIC_DESC	"tactic_desc-"

#define HERO_PREFIX_STR_TECHNOLOGY	"technology-"
#define HERO_PREFIX_STR_TECHNOLOGY_DESC	"technology_desc-"

#define HEROS_NO_CHARACTER		0xff
#define HERO_PREFIX_STR_CHARACTER	"character-"
#define HERO_PREFIX_STR_CHARACTER_DESC	"character_desc-"

struct hero_5fields_t {
	uint16_t leadership_;
	uint16_t force_;
	uint16_t intellect_;
	uint16_t politics_;
	uint16_t charm_;
};

struct hero_feeling {
	uint16_t hero_;
	uint16_t feeling_;
};

#define HERO_FIELDS	\
	uint8_t gender_;	\
	uint8_t feature_;	\
	uint8_t utype_;	\
	uint8_t status_;	\
	uint8_t official_;	\
	uint8_t base_catalog_;	\
	uint8_t	activity_;	\
	uint8_t ambition_;	\
	uint8_t character_;	\
	uint8_t side_;	\
	uint8_t heart_;	\
	uint8_t side_feature_;	\
	uint8_t treasure_;	\
	uint8_t tactic_;	\
	uint16_t image_;	\
	uint16_t leadership_;	\
	uint16_t force_;	\
	uint16_t intellect_;	\
	uint16_t politics_;	\
	uint16_t charm_;	\
	uint16_t meritorious_;	\
	uint16_t city_;	\
	uint16_t birth_;	\
	uint16_t float_catalog_;	\
	uint16_t arms_[6];	\
	uint16_t skill_[8];	\
	uint16_t number_;	\
	uint8_t reserve2_[96];	\
	hero_feeling parent_[HEROS_MAX_PARENT];	\
	hero_feeling consort_[HEROS_MAX_CONSORT];	\
	hero_feeling oath_[HEROS_MAX_OATH];	\
	hero_feeling intimate_[HEROS_MAX_INTIMATE];	\
	hero_feeling hate_[HEROS_MAX_HATE];	\
	char reserve3_[16];

struct hero_fields_t {
	HERO_FIELDS;
}; 

#if defined(_KINGDOM_EXE) || !defined(_WIN32)
class hero;
namespace rpg {
	extern hero* h;
	extern int stratum;
	extern std::set<unit*> humans;
	extern int forbids;
}
#endif

namespace increase_xp {
// use to increase unit's xp, speed is additional.
struct ublock {
	bool xp;
	bool leadership;
	int leadership_speed;
	bool force;
	int force_speed;
	bool intellect;
	int intellect_speed;
	bool politics;
	int politics_speed;
	bool charm;
	int charm_speed;
	bool meritorious;
	bool abilityx2;
	bool arms;
	bool armsx2;
	bool skill[HEROS_MAX_SKILL];
	int skill_speed[HEROS_MAX_SKILL];
	bool skillx2;
};

// use to increase hero's xp
struct hblock {
	int leadership;
	int force;
	int intellect;
	int politics;
	int charm;
	int meritorious;
	int arms[HEROS_MAX_ARMS];
	int skill[HEROS_MAX_SKILL];
};

extern ublock ub;
ublock& generic_ublock();
ublock& attack_ublock(const unit& attack, bool opp_is_artifical = false);
ublock& turn_ublock(const unit& u);
ublock& exploiture_ublock(int markets, int technologies, int business_speed, int technology_speed, bool abilityx2, bool skillx2);

extern hblock hb;
hblock& generic_hblock();
hblock& exploiture_hblock(int markets, int technologies, int business_speed, int technology_speed, bool abilityx2, bool skillx2, int xp);

extern int ability_per_xp;
extern int arms_per_xp;
extern int skill_per_xp;
extern int meritorious_per_xp;
extern int navigation_per_xp;
}

class hero: public hero_fields_t
{
public:
	static std::string adaptability_str2(uint16_t adaptability);
	static std::string& gender_str(int gender);
	static std::string& arms_str(int arms);
	static std::string& skill_str(int skill);
	static const std::string& feature_str(int feature);
	static const std::string& feature_desc_str(int feature);
	static std::vector<int>& valid_features();
	static const std::string& stratum_str(int stratum);
	static const std::string& status_str(int status);
	static const std::string& official_str(int offical);

	static int number_market;
	static int number_technology;
	static int number_wall;
	static int number_keep;
	static int number_tower;

	static int number_businessman;
	static int number_scholar;
	static int number_commoner_min;
	static int number_commoner_max;
	static bool is_commoner(int h);

	static int number_scout;
	static int number_system_min;
	static int number_system_max;
	static bool is_system(int h);

	hero(uint16_t number, uint16_t leadership = 0, uint16_t force = 0, uint16_t intellect = 0, uint16_t politics = 0, uint16_t charm = 0);
	hero(const hero& that);
	hero(const uint8_t* mem);

	void write(uint8_t* mem);
	void read(const uint8_t* mem);

	const bool valid() const;

	enum FEELING {FEELING_PARENT, FEELING_CONSORT, FEELING_OATH, FEELING_INTIMATE, FEELING_HATE, FEELING_NONE};
	bool is_parent(const hero& h, uint32_t* index = NULL) const;
	bool is_consort(const hero& h, uint32_t* index = NULL) const;
	bool is_oath(const hero& h, uint32_t* index = NULL) const;
	bool is_intimate(const hero& h, uint32_t* index = NULL) const;
	bool is_hate(const hero& h, uint32_t* index = NULL) const;

	int increase_catalog(int inc, hero& leader);

	int loyalty(const hero& leader) const;
	int base_loyalty(const hero& leader) const;
	int increase_loyalty(int inc, hero& leader);
	void set_loyalty(hero& leader, int level, bool fixed = false);
	void set_loyalty2(hero& leader, int level, bool fixed = false);

	int increase_feeling(hero& to, int inc, int& descent_number);

	void to_unstage();
	void change_language();

	const char* image(bool big = false);
	void set_image(int image);

	std::string& name();
	void set_name(const std::string& name);

	std::string& surname();
	void set_surname(const std::string& surname);

	const char* biography();

	std::string& heart_str();

	std::string& ambition_str();

	enum {ARMS, SKILL};

#if defined(_KINGDOM_EXE) || !defined(_WIN32)
	void add_modification(unit_map& units, hero_map& heros, std::vector<team>& teams, const config& mod, unit* u, hero* leader);
	bool internal_matches_filter(const vconfig& cfg);
	void increase_feeling_each(unit_map& units, hero_map& heros, hero& to, int inc);
#endif
	bool get_xp(const increase_xp::hblock& hb);

// attribute
public:
	// HERO_FIELDS;

private:
	friend class hero_map;
	static std::string image_file_root_;
	static std::string gender_str_[HEROS_MAX_GENDER];
	static std::string feature_str_[HEROS_MAX_FEATURE];
	static std::string feature_desc_str_[HEROS_MAX_FEATURE];
	static std::string arms_str_[HEROS_MAX_ARMS];
	static std::string skill_str_[HEROS_MAX_SKILL];
	static std::string stratum_str_[HEROS_STRATUMS];
	static std::string status_str_[HEROS_STATUSES];
	static std::string official_str_[HEROS_OFFICIALS];
	static std::vector<int> valid_features_;

	char imgfile_[32];
	char imgfile2_[32];
	std::string name_str_;
	std::string surname_str_;
	std::string heart_str_;
	std::string ambition_str_;
};


bool compare_leadership(const hero* lhs, const hero* rhs);
bool compare_politics(const hero* lhs, const hero* rhs);
bool compare_recruit(const hero* lhs, const hero* rhs);

extern hero hero_invalid;
extern std::vector<hero*> empty_vector_hero_ptr;
extern std::vector<size_t> empty_vector_size_t;

class hero_map
{
public:
	hero_map(const std::string& path = "");
	~hero_map();

	hero_map& operator=(const hero_map& that);
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

		bool valid() const { return (i_ != HEROS_INVALID_NUMBER)? true: false; }

		bool operator==(const iterator_base& rhs) const { return i_ == rhs.i_; }
		bool operator!=(const iterator_base& rhs) const { return i_ != rhs.i_; }

		map_type* get_map() const { return map_; }

		template <typename X> friend struct iterator_base;

	private:
		map_type* map_;
		size_t i_;
	};

	struct standard_iter_types {
		typedef hero_map map_type;
		typedef hero value_type;
		typedef value_type* pointer_type;
		typedef value_type& reference_type;
	};

	struct const_iter_types {
		typedef const hero_map map_type;
		typedef const hero value_type;
		typedef value_type* pointer_type;
		typedef value_type& reference_type;
	};


// ~~~ End iterator code ~~~
	typedef iterator_base<standard_iter_types> iterator;
	typedef iterator_base<const_iter_types> const_iterator;

	void set_path(const std::string& path);

	// load from/save to file
	// 如果同名，碰到c++把本该调用(fname)去调用fp了，为显示只好不同名
	bool map_to_file(const std::string& fname);
	bool map_to_file_fp(posix_file_t fp);
	bool map_from_file(const std::string& fname);
	bool map_from_file_fp(posix_file_t fp, uint32_t file_offset = 0, uint32_t valid_bytes = 0);
	uint32_t map_to_mem(uint8_t* data) const;

	iterator find(const size_t num);
	const_iterator find(const size_t num) const { return const_cast<hero_map *>(this)->find(num); }

	void add(const hero& h);

	void erase(const uint16_t number);

	iterator begin();
	const_iterator begin() const;

	// extern iterator hero_map_iter_invalid;
	// extern const_iterator hero_map_const_iter_invalid;
	// iterator end() { return iterator(map_vsize_, this); }
	// const_iterator end() const { return const_iterator(map_vsize_, this); }
	iterator end();
	const_iterator end() const;

	size_t size() const { return map_vsize_; }
	size_t file_size() const;
	
	void clear_map();

	void realloc_hero_map(const size_t size);

	hero& operator[](const uint16_t num);
	const hero& operator[](const uint16_t num) const;

	void reset_to_unstage();
	void change_language();

private:
	size_t map_size_;

	hero** map_;
	uint16_t map_vsize_;
};

// define allowed conversions.
template <>
struct hero_map::convertible<hero_map::standard_iter_types, hero_map::const_iter_types> {};

template <typename T>
struct hero_map::convertible<T, T> { };

template <typename iter_types>
hero_map::iterator_base<iter_types>& hero_map::iterator_base<iter_types>::operator++() 
{
	if ((uint16_t)i_ < (map_->map_vsize_ - 1)) {
		++ i_;
	} else {
		i_ = HEROS_INVALID_NUMBER;
	}

	return *this;
}

template <typename iter_types>
hero_map::iterator_base<iter_types> hero_map::iterator_base<iter_types>::operator++(int) 
{
	hero_map::iterator_base<iter_types> temp(*this);
	operator++();
	return temp;
}

template <typename iter_types>
hero_map::iterator_base<iter_types>& hero_map::iterator_base<iter_types>::operator--() 
{
	if (i_ != 0) {
		-- i_;
	}
	return *this;
}

template <typename iter_types>
hero_map::iterator_base<iter_types> hero_map::iterator_base<iter_types>::operator--(int) 
{
	hero_map::iterator_base<iter_types> temp(*this);
	operator--();
	return temp;
}

#endif // __HERO_HPP_