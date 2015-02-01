#ifndef __HERO_HPP_
#define __HERO_HPP_

#include "posix.h"
#include "config.hpp"
#include <set>

class hero;
class hero_map;

// change between actual value and display value. actual value is fixed_t, display value is float or int.
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

// 256 + 64 + 704 = 1024
#define HEROS_MAX_HEROS			8192
#define HEROS_BYTES_PER_HERO	240
#define HEROS_FILE_PREFIX_BYTES	16

#define HEROS_INVALID_NUMBER	0xffff
#define HEROS_INVALID_SIDE		0xff
#define HEROS_ROAM_CITY			0
#define HEROS_DEFAULT_STATUS	hero_status_unstage
#define HEROS_NO_FEATURE		0xff

#define HEROS_NO_OFFICIAL		0xff
#define HEROS_DEFAULT_OFFICIAL	HEROS_NO_OFFICIAL

#define HEROS_DEFAULT_STRATUM	hero_stratum_wander

#define HEROS_FULL_ACTIVITY		0xff
#define HEROS_DEFAULT_ACTIVITY	HEROS_FULL_ACTIVITY

#define HERO_PREFIX_STR_NAME	"name-"
#define HERO_PREFIX_STR_SURNAME	"surname-"

enum {
	hero_field_min,
	hero_field_leadership = hero_field_min,
	hero_field_force,
	hero_field_intellect,
	hero_field_spirit,
	hero_field_charm,
	hero_field_max = hero_field_charm,
};

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
	hero_status_employable,
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

#define HEROS_FLAGS					8
#define HERO_PREFIX_STR_FLAG	"flag-"
enum {
	hero_flag_min = 0,
	hero_flag_robber = hero_flag_min,
	hero_flag_roam,
	hero_flag_employee,
	hero_flag_npc,
	hero_flag_max = hero_flag_npc,
};

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
	hero_skill_formation,
	hero_skill_t7,
	hero_skill_max = hero_skill_t7,
};

#define HEROS_MAX_RANGE		3 // melee, ranged, cast
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
	hero_feature_arms,
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
	hero_featrue_backstab, // --> 60
	hero_feature_inspirit,
	hero_feature_granary,
	hero_feature_ground,
	hero_feature_tactic,
	hero_feature_carom,
	hero_feature_disturb,
	hero_feature_spirit,

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

#define HERO_PREFIX_STR_FORMATION		"formation-"
#define HERO_PREFIX_STR_FORMATION_DESC	"formation_desc-"
#define HERO_PREFIX_STR_FORMATION_ICON	"icon-"
#define HERO_PREFIX_STR_FORMATION_BG	"bg_image-"

#define HEROS_NO_CHARACTER		0xff
#define HERO_PREFIX_STR_CHARACTER	"character-"
#define HERO_PREFIX_STR_CHARACTER_DESC	"character_desc-"

#define NO_DECREE				-1
#define HERO_PREFIX_STR_DECREE		"decree-"
#define HERO_PREFIX_STR_DECREE_DESC	"decree_desc-"

#define HEROS_NO_NOBLE			0xff
#define HERO_PREFIX_STR_NOBLE		"noble-"

struct hero_5fields_t {
	uint16_t leadership_;
	uint16_t force_;
	uint16_t intellect_;
	uint16_t spirit_;
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
	uint8_t flags_;	\
	uint8_t character_;	\
	uint8_t side_;	\
	uint8_t heart_;	\
	uint8_t side_feature_;	\
	uint8_t treasure_;	\
	uint8_t tactic_;	\
	uint8_t cost_;	\
	uint8_t noble_;	\
	uint16_t image_;	\
	uint16_t leadership_;	\
	uint16_t force_;	\
	uint16_t intellect_;	\
	uint16_t spirit_;	\
	uint16_t charm_;	\
	uint16_t meritorious_;	\
	uint16_t city_;	\
	uint16_t birth_;	\
	uint16_t float_catalog_;	\
	uint16_t arms_[6];	\
	uint16_t skill_[8];	\
	uint16_t number_;	\
	uint16_t player_;	\
	uint16_t name_;	\
	uint16_t surname_;	\
	uint16_t desc_;	\
	uint8_t reserve2_[88];	\
	hero_feeling parent_[HEROS_MAX_PARENT];	\
	hero_feeling consort_[HEROS_MAX_CONSORT];	\
	hero_feeling oath_[HEROS_MAX_OATH];	\
	hero_feeling intimate_[HEROS_MAX_INTIMATE];	\
	hero_feeling hate_[HEROS_MAX_HATE];	\
	char reserve3_[14];

struct hero_fields_t {
	HERO_FIELDS;
}; 

// use to increase hero's xp
struct hblock {
	int leadership;
	int force;
	int intellect;
	int spirit;
	int charm;
	int meritorious;
	int arms[HEROS_MAX_ARMS];
	int skill[HEROS_MAX_SKILL];
};

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
	bool spirit;
	int spirit_speed;
	bool charm;
	int charm_speed;
	bool meritorious;
	bool abilityx2;
	bool arms;
	int arms_speed;
	bool armsx2;
	bool skill[HEROS_MAX_SKILL];
	int skill_speed[HEROS_MAX_SKILL];
	bool skillx2;
};

extern hblock hb;
hblock& generic_hblock();
hblock& exploiture_hblock(int markets, int technologies, int business_speed, int technology_speed, bool abilityx2, bool skillx2, int xp);

extern int ability_per_xp;
extern int arms_per_xp;
extern int skill_per_xp;
extern int meritorious_per_xp;
extern int navigation_per_xp;
}

class hero_: public hero_fields_t
{
public:
	static std::string image_file_root_;

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
	static const std::string& flag_str(int flag);

	static int number_system;
	static int number_civilian;
	static int number_scout;
	static int number_empty_leader;
	static int number_system_min;
	static int number_system_max;
	static bool is_system(int h);

	static int number_soldier_min;
	static int number_soldier_max;
	static bool is_soldier(int h);

	static int number_market;
	static int number_technology;
	static int number_wall;
	static int number_keep;
	static int number_fort;
	static int number_tower;
	static int number_tactic;
	static int number_school;
	static int number_artifical_min;
	static int number_artifical_max;
	static bool is_artifical(int h);
	static bool is_ea_artifical(int h);

	static int number_businessman;
	static int number_scholar;
	static int number_transport;
	static int number_commoner_min;
	static int number_commoner_max;
	static bool is_commoner(int h);

	static bool data_variable(int h);

	static int number_local_player_city;
	static int number_network_player_city_min;
	static int number_network_player_city_max;

	static int number_city_min;
	static int number_normal_min;
	static int number_employee_min;

	hero_(const hero_& that);
	hero_(uint16_t number, uint16_t leadership, uint16_t force, uint16_t intellect, uint16_t spirit, uint16_t charm);
	hero_(const uint8_t* mem);

	void write(uint8_t* mem);
	void read(const uint8_t* mem);

	const bool valid() const;
	void clear_relation();

	virtual bool check_valid() const { return true; }

	enum FEELING {FEELING_PARENT, FEELING_CONSORT, FEELING_OATH, FEELING_INTIMATE, FEELING_HATE, FEELING_NONE};
	bool is_parent(const hero_& h, uint32_t* index = NULL) const;
	bool is_consort(const hero_& h, uint32_t* index = NULL) const;
	bool is_oath(const hero_& h, uint32_t* index = NULL) const;
	bool is_intimate(const hero_& h, uint32_t* index = NULL) const;
	bool is_hate(const hero_& h, uint32_t* index = NULL) const;

	int increase_catalog(int inc, const hero_& leader);

	int loyalty(const hero_& leader) const;
	int base_loyalty(const hero_& leader) const;
	int increase_loyalty(int inc, hero_& leader);
	void set_loyalty(const hero_& leader, int level, bool fixed = false);
	void set_loyalty2(hero_& leader, int level, bool fixed = false);
	void set_loyalty3(const hero_& leader, int level);

	void set_flag(int flag);
	void clear_flag(int flag);
	bool get_flag(int flag) const;

	bool can_goto_hate(const hero_& h) const;
	void do_hate_relation(hero_& to, bool set);
		
	void change_language();

	static std::string image_from_uid(int uid, int gender, bool big);
	const char* image(bool big = false);
	void set_image(int image);
	
	std::string& name();
	void set_name(const std::string& name);

	int uid() const { return uid_ ; }
	void set_uid(int _uid);

	std::string& surname();
	void set_surname(const std::string& surname);

	const char* biography();
	std::string biography2(const hero_map& heros);
	void set_biography(const std::string& biography);

	std::string heart_str();

	enum {ARMS, SKILL};

	bool get_xp(const hblock& hb);

	void adjust_according_to_level(const hero_& base, int level, const hero_& leader);
	bool is_roam_member() const;
	void field_set_minmum();
	std::string field_to_str();
	void field_from_str(const std::string& str);
	std::string translatable_to_str();
	void translatable_from_str(const std::string& str);
	static int gender_from_filed_str(const std::string& str);

protected:
	friend class hero_map;
	static std::string gender_str_[HEROS_MAX_GENDER];
	static std::string feature_str_[HEROS_MAX_FEATURE];
	static std::string feature_desc_str_[HEROS_MAX_FEATURE];
	static std::string arms_str_[HEROS_MAX_ARMS];
	static std::string skill_str_[HEROS_MAX_SKILL];
	static std::string stratum_str_[HEROS_STRATUMS];
	static std::string status_str_[HEROS_STATUSES];
	static std::string official_str_[HEROS_OFFICIALS];
	static std::string flag_str_[HEROS_FLAGS];
	static std::vector<int> valid_features_;

	std::string imgfile_;
	std::string imgfile2_;
	std::string name_str_;
	std::string surname_str_;
	int uid_;
	std::string biography_str_;
};

#if defined(_KINGDOM_EXE) || !defined(_WIN32)
class unit_map;
class team;
class vconfig;
class display;
class game_display;
class game_state;
class ttechnology;
class unit;
namespace http {
	struct membership;
	struct temployee;
}

namespace rpg {
	extern hero* h;
	extern int stratum;
	extern std::set<unit*> humans;
	extern int forbids;
}

namespace increase_xp {

extern ublock ub;
ublock& generic_ublock();
ublock& attack_ublock(const unit& attack, bool opp_is_artifical = false);
ublock& turn_ublock(const unit& u);
ublock& exploiture_ublock(int markets, int technologies, int business_speed, int technology_speed, bool abilityx2, bool skillx2);
ublock& duel_ublock(const unit& u);

}
#endif

class hero: public hero_
{
public:
	hero(const hero& that)
		: hero_(that)
	{}

	hero(uint16_t number, uint16_t leadership = 0, uint16_t force = 0, uint16_t intellect = 0, uint16_t spirit = 0, uint16_t charm = 0)
		: hero_(number, leadership, force, intellect, spirit, charm)
	{}

	hero(const uint8_t* mem)
		: hero_(mem)
	{}

	int increase_feeling(hero& to, int inc, int& descent_number);
	bool confirm_carry_to(hero& h2, int carry_to);
	bool check_valid() const;

#if defined(_KINGDOM_EXE) || !defined(_WIN32)
	enum {UNSTAGE_NONE, UNSTAGE_GROUP};
	void to_unstage(int reason = UNSTAGE_NONE);
	bool has_nomal_noble() const;

	void add_modification(unit_map& units, hero_map& heros, std::vector<team>& teams, const config& mod, unit* u, hero* leader);
	bool internal_matches_filter(const vconfig& cfg);
	void goto_hate_if(const team& new_team, const hero& original_leader, int random);
	void increase_feeling_each(unit_map& units, hero_map& heros, hero& to, int inc);
	int calculate_used_point() const;
#endif
};

bool compare_leadership(const hero* lhs, const hero* rhs);
bool compare_spirit(const hero* lhs, const hero* rhs);
bool compare_recruit(const hero* lhs, const hero* rhs);

extern hero hero_invalid;
extern std::vector<hero*> empty_vector_hero_ptr;
extern std::vector<size_t> empty_vector_size_t;

class hero_map
{
public:
	static size_t map_size_from_dat;

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
	bool map_to_file(const std::string& fname);
	bool map_to_file_fp(posix_file_t fp);
	bool map_from_file(const std::string& fname);
	bool map_from_file_fp(posix_file_t fp, uint32_t file_offset = 0, uint32_t valid_bytes = 0);

	bool map_from_mem(const uint8_t* mem, int len);
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

	hero& operator[](const uint16_t num) const;

	void reset_to_unstage();
	void change_language();

private:
	size_t map_size_;

	hero** map_;
	uint16_t map_vsize_;
};

//
// group section
//
namespace upgrade {
struct trequire {
	trequire(int _coin = 0, int _score = 0)
		: coin(_coin)
		, score(_score)
	{}

	int coin;
	int score;
};
extern std::vector<trequire> require_member, require_noble;

void fill_require();
const trequire& member_require(int level);
trequire member_upgrade_cost(int from, int to);
const trequire& noble_require(int noble);
trequire noble_upgrade_cost(int from, int to);
}

struct tlocation_anim
{
	tlocation_anim(int _xoffset, int _yoffset, std::string _text, Uint32 _color)
		: xoffset(_xoffset)
		, yoffset(_yoffset)
		, text(_text)
		, color(_color)
	{}

	int xoffset;
	int yoffset;
	std::string text;
	Uint32 color;
};

class tgroup_
{
public:
	struct tmember {
		tmember()
			: base(NULL)
			, level(0)
			, h(NULL)
		{}

		bool operator<(const tmember& that) const
		{
			return base->number_ < that.base->number_;
		}
		bool operator==(const tmember& that) const
		{
			return base->number_ == that.base->number_;
		}
		
		hero* base;
		int level;
		hero* h;
	};
	static tmember null_member;

	struct tassociate {
		enum {none, requestally, ally, requestterminate, all};
		tassociate(const std::string& _username, int _uid, int _agreement, time_t _t)
			: username(_username)
			, uid(_uid)
			, agreement(_agreement)
			, t(_t)
		{}

		std::string username;
		int uid;
		int agreement;
		time_t t;
	};

	struct tsignin {
		tsignin(bool _today = false, int _continue = 0, int _break = 0)
			: today(_today)
			, continue_days(_continue)
			, break_days(_break)
		{}

		bool today;
		int continue_days;
		int break_days;
	};

	tgroup_()
		: leader_(NULL)
		, members_()
		, exiles_()
		, city_(NULL)
		, vip_(0)
		, expire_(0)
		, noble_(0)
		, signin_(tsignin())
		, coin_(0)
		, score_(0)
		, tax_(0)
		, layout_()
		, map_()
		, browsed_(0)
		, faction_cfg_()
	{
		memset(interiors_, 0, sizeof(interiors_));
		memset(messages_, 0, sizeof(messages_));
	}
	void reset();
	bool valid() const;
	
	void set_leader(hero& h) { leader_ = &h; }
	hero& leader() const { return *leader_; }

	void set_city(hero& h) { city_ = &h; }
	hero& city() const { return *city_; }

	tmember& member(const hero& h);
	const tmember& member(const hero& h) const;

	const tmember& exile(const hero& h) const;

	tmember& member_from_base(const hero& h);
	const tmember& member_from_base(const hero& h) const;

	hero* exist_member(int number) const;
	const std::vector<tmember>& members() const { return members_; }
	void insert_member(hero_map& heros, int number, int level);
	void unstage_member(hero_map& heros, int number);
	void adjust_members_according_to_leader(hero_map& heros);
	std::vector<tmember*> part_members(hero_map& heros, bool roam);

	hero* exist_exile(int number) const;
	void insert_exile(hero_map& heros, int number, int level);
	void unstage_exile(hero_map& heros, int number);
	const std::vector<tmember>& exiles() const { return exiles_; }

	const std::vector<tassociate>& associates() const { return associates_; }
	std::vector<tassociate>& associates() { return associates_; }
	std::vector<tassociate> associates_from_agreement(int agreement) const;
	std::vector<tassociate> associates_from_agreement(std::set<int>& agreement) const;
	const tassociate& associate(int uid) const;

	void set_vip(int val) { vip_ = val; }
	int vip() const { return vip_; }
	
	void set_expire(time_t val) { expire_ = val; }
	time_t expire() const { return expire_; }

	void set_noble(int val) { noble_ = val; }
	int noble() const { return noble_; }

	void set_coin(int val) { coin_ = val; }
	int coin() const { return coin_; }

	void set_score(int val) { score_ = val; }
	int score() const { return score_; }

	void set_tax(int val) { tax_ = val; }
	int tax() const { return tax_; }

	enum {interior_military, interior_culture, interior_economy, interior_science, interior_count};
	int interior(int type) const { return interiors_[type]; }
	static void interior_from_str_internal(const std::string& str, int* interiors);
	void interior_from_str(const std::string& str);
	std::string interior_to_str() const;

	void set_map(const std::string& str);
	const std::string& map() const { return map_; }

	enum {msg_message, msg_siege, msg_count};
	int message(int type) const { return messages_[type]; }
	int message_count() const;
	void message_from_str(const std::string& str);
	time_t browsed_time() const { return browsed_; }

	const tsignin& signin() const { return signin_; }
	void signin_from_str(const std::string& str);
	std::string signin_to_str() const;

	void reload_heros_from_string(hero_map& heros, const std::string& member_str, const std::string& exile_str);
	void member_from_str(hero_map& heros, const std::string& str, bool erase = true);
	void member_from_str2(hero_map& heros, const std::string& str);
	void exile_from_str(hero_map& heros, const std::string& str);
	void exile_from_str2(hero_map& heros, const std::string& str);
	static std::string member_to_str_internal(const std::vector<tmember>& v);
	static std::string member_to_str2_internal(const std::vector<tmember>& v);
	std::string member_to_str() const;
	std::string member_to_str2() const;
	std::string exile_to_str() const;
	std::string exile_to_str2() const;

	static std::vector<tassociate> associate_from_str2(const std::string& str);
	void associate_from_str(const std::string& str);
	std::string associate_to_str() const;

	void to_side_cfg(config& side, int random, int max_service_member = 0xffff) const;

	size_t fp_size() const;
	int to_fp(uint8_t* mem) const;
	int from_fp(hero_map& heros, uint8_t* mem);

	void redirect_hero_map(hero_map& heros);
	void remove_unstage_member();

protected:
	hero* leader_;
	std::vector<tmember> members_;
	std::vector<tmember> exiles_;
	std::vector<tassociate> associates_;
	hero* city_;

	int vip_;
	time_t expire_;
	int interiors_[interior_count];
	int noble_;
	int coin_;
	int score_;
	int tax_;
	std::string layout_;
	std::string map_;
	time_t browsed_;
	int messages_[msg_count];
	tsignin signin_;
	config faction_cfg_;
};

class tgroup: public tgroup_
{
public:
	tgroup()
		: tgroup_()
	{}

#if defined(_KINGDOM_EXE) || !defined(_WIN32)
	bool upgrade_leader(display& disp, hero_map& heros);
	bool upgrade_member(display& disp, hero_map& heros, hero& h);
	bool upgrade_internal(display& disp, hero_map& heros, tmember* m, std::map<int, http::temployee>* employees);
	int calculate_total_point() const;
	void from_membership(hero_map& heros, const http::membership& m, int city = HEROS_INVALID_NUMBER);
	void from_local_membership(display& disp, hero_map& heros, const http::membership& m, bool enhance);
	http::membership to_membership() const;

	void set_layout(const std::string& str);
	const std::string& layout() const { return layout_; }

	void layout_from_team(game_display& disp, team& t);
	bool layout_to_team(game_display& disp, unit_map& units, hero_map& heros, std::vector<team>& teams, game_state& state, team& t, bool mirror, bool conside_stratagem, bool wall);

	static std::string layout_from_team_internal(team& t, const tgroup& g);
	static bool layout_to_team_internal(unit_map& units, hero_map& heros, std::vector<team>& teams, game_state& state, const std::string& from, team& t, bool mirror, bool conside_stratagem, bool wall, const tgroup& g);

	static const ttechnology& stratagem_from_layout_str_internal(const std::string& layout);
	const ttechnology& stratagem_from_layout_str() const;
	static std::vector<int> interior_increase_from_layout_str_internal(const std::string& layout);
	std::vector<int> interior_increase_from_layout_str() const;

	std::vector<const ttechnology*> calculate_allocatable_technology(int mode, std::vector<const ttechnology*>& exclude, int random) const;
	std::string increase_allocatable_technology(int mode, const std::string& exclude_str, int random) const;
	std::map<int, int> adjust_hero_field(int field);
	std::vector<size_t> increase_card(team& t, int mode);
	const config& to_faction_cfg(bool exile, bool multigrid);

private:
	int layout_number_from_hero(const hero& h) const;
	hero& hero_from_layout_number(hero_map& heros, int number, int layout_leader_number) const;
#endif
	
};
extern tgroup group, null_group;
extern std::map<int, tgroup> other_group;

namespace runtime_groups {
extern std::map<int, tgroup> gs;

void insert(tgroup& g);
tgroup& get(int leader);
void redirect_hero_map(hero_map& heros);
void remove_unstage_member();

void from_fp(hero_map& heros, posix_file_t fp, int len);
size_t size();
void to_fp(posix_file_t fp);

void from_mem(hero_map& heros, uint8_t* mem, int len);
void to_mem(uint8_t* mem);

bool exist_member(const hero& h, const hero& leader);
}

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