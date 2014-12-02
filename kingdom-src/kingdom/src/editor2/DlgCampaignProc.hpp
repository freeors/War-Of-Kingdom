#ifndef __DLGCAMPAIGNPROC_HPP_
#define __DLGCAMPAIGNPROC_HPP_

#ifndef _ROSE_EDITOR

#include "global.hpp"
#include "stdafx.h"
#include <windowsx.h>
#include "config.hpp"
#include "editor.hpp"
#include "hero.hpp"
#include "unit_types.hpp"
#include "event.hpp"
#include "wml_exception.hpp"

namespace scenario_selector {
	extern bool multiplayer;
	extern void switch_to(bool multiplayer);
}

namespace explorer_technology {
	enum {NONE, SCENARIO, MODIFY_SIDE};
	extern int explorer;
	void update_to_ui_special(HWND hdlgP, int flag);
};

void campaign_enable_save_btn(bool enable);
#define campaign_get_save_btn()				(ToolBar_GetState(gdmgr._htb_campaign, IDM_SAVE) & TBSTATE_ENABLED)

#define campaign_enable_new_btn(fEnable)	ToolBar_EnableButton(gdmgr._htb_campaign, IDM_NEW, fEnable)
#define campaign_enable_delete_btn(fEnable)	ToolBar_EnableButton(gdmgr._htb_campaign, IDM_DELETE, fEnable)
#define campaign_enable_herostate_btn(fEnable)	ToolBar_EnableButton(gdmgr._htb_campaign, IDM_HEROSTATE, fEnable)

extern editor editor_;

enum {NONE_CATALOG = 0, TUTORIAL_CATALOG};
enum {NO_DUEL, RANDOM_DUEL, ALWAYS_DUEL};

class tcampaign
{
public:
	tcampaign();

	HWND init_toolbar(HINSTANCE hinst, HWND hdlgP);

	const std::string& arms(const std::string& id);
	int arms_int(const std::string& id);

	const std::string& navigation(const std::string& id);

	std::string city_trait(const std::string& id);
	std::string troop_trait(const std::string& id);

public:
	config game_config_;
	std::string id_;
};

class tmain_
{
public:
	tmain_(const std::string& id = null_str, const std::string& firstscenario_id = null_str) 
		: textdomain_(PACKAGE)
		, id_(id)
		// , abbrev_(id)
		, abbrev_()
		, first_scenario_(firstscenario_id)
		, hero_data_("^xwml/hero.dat")
		, subcontinent_(false)
		, mode_(mode_tag::NONE)
		, catalog_(NONE_CATALOG)
	{}

	bool must_exist_map() const
	{
		return mode_ == mode_tag::RPG || (mode_ == mode_tag::SCENARIO && !subcontinent_);
	}

public:
	std::string textdomain_;
	std::string id_;
	std::string abbrev_;
	std::string first_scenario_;
	std::string hero_data_;
	bool subcontinent_;
	mode_tag::tmode mode_;
	int catalog_;
};

class tmain: public tmain_
{
public:
	static std::vector<std::pair<std::string, std::string> > catalog_map;
	static int find_catalog(const std::string& id);

	tmain(const std::string& id = null_str, const std::string& firstscenario_id = null_str);
	void from_config(const config& cfg);
	void from_ui(HWND hdlgP);
	void update_to_ui(HWND hdlgP);
	void generate();

	std::string description() const;
	std::string icon(bool absolute = false) const;
	std::string image(bool absolute = false) const;

	enum {BIT_ID = 0, BIT_TEXTDOMAIN, 
		BIT_ABBREV, BIT_FIRSTSCENARIO, BIT_ICON, 
		BIT_IMAGE, BIT_MODE, BIT_SUBCONTINENT, BIT_CATALOG};
	void set_dirty(int bit, bool set);
public:

	std::string file_;

	uint32_t dirty_;
	tmain_ main_from_cfg_;
};

class tscenario;

class tside
{
public:
	class tcity 
	{
	public:
		tcity() 
			: character_(NO_ESPECIAL)
			, mayor_(HEROS_INVALID_NUMBER)
			, soldiers_(0)
			, except_(false)
		{}

		void from_ui(HWND hdlgP, tside& side);
		void update_to_ui_side_edit(HWND hdlgP, int index = -1) const;
		void update_to_ui_city_edit(HWND hdlgP, tside& side, bool partial = true);

		bool operator==(const tcity& that) const
		{
			if (except_) return false;
			if (heros_army_ != that.heros_army_) return false;
			if (type_ != that.type_) return false;
			if (character_ != that.character_) return false;
			if (soldiers_ != that.soldiers_) return false;
			if (loc_ != that.loc_) return false;
			if (traits_ != that.traits_) return false;
			if (not_recruit_ != that.not_recruit_) return false;
			if (alias_ != that.alias_) return false;
			if (mayor_ != that.mayor_) return false;
			if (service_heros_ != that.service_heros_) return false;
			if (wander_heros_ != that.wander_heros_) return false;
			if (economy_area_ != that.economy_area_) return false;
			return true;
		}
		bool operator!=(const tcity& that) const { return !operator==(that); }

		std::vector<int> heros_army_;
		std::string type_;
		int character_;
		int soldiers_;
		map_location loc_;
		std::set<std::string> traits_;
		std::vector<std::string> not_recruit_;
		std::string alias_;

		int mayor_;
		std::set<int> service_heros_;
		std::set<int> wander_heros_;
		std::set<map_location> economy_area_;
		bool except_;
	};

	class tunit 
	{
	public:
		tunit()
			: character_(NO_ESPECIAL)
			, except_(false)
		{}
		void from_ui(HWND hdlgP, tside& side);
		void update_to_ui_side_edit(HWND hdlgP, int index = -1) const;
		void update_to_ui_troop_edit(HWND hdlgP, tside& side, bool partial = true);

		bool operator==(const tunit& that) const
		{
			if (except_) return false;
			if (heros_army_ != that.heros_army_) return false;
			if (city_ != that.city_) return false;
			if (type_ != that.type_) return false;
			if (character_!= that.character_) return false;
			if (loc_ != that.loc_) return false;
			if (traits_ != that.traits_) return false;
			return true;
		}
		bool operator!=(const tunit& that) const { return !operator==(that); }

		std::vector<int> heros_army_;
		int city_;
		std::string type_;
		int character_;
		map_location loc_;
		std::set<std::string> traits_;
		bool except_;
	};

	class arms_feature 
	{
	public:
		arms_feature(int arms = -1, int level = -1, int feature = -1) 
			: arms_(arms)
			, level_(level)
			, feature_(feature)
		{}

		void from_ui(HWND hdlgP);
		void update_to_ui_side_edit(HWND hdlgP, int index = -1) const;
		void update_to_ui_feature_edit(HWND hdlgP);

		bool operator==(const arms_feature& that) const
		{
			if (arms_ != that.arms_) return false;
			if (level_ != that.level_) return false;
			if (feature_ != that.feature_) return false;
			return true;
		}
		bool operator!=(const arms_feature& that) const { return !operator==(that); }

		int arms_;
		int level_;
		int feature_;
	};

	tside(tscenario* scenario = NULL);
	void from_config(const config& side_cfg);
	void from_ui(HWND hdlgP);
	void update_to_ui(HWND hdlgP) const;
	void update_to_ui_side_edit(HWND hdlgP, bool partial = true);
	
	std::string generate(const std::string& prefix) const;

	bool new_feature();
	void erase_feature(int index, HWND hdlgP);

	bool new_city();
	void erase_city(int index, HWND hdlgP);

	bool new_troop();
	void erase_troop(int index, HWND hdlgP);

	bool operator==(const tside& that) const;
	bool operator!=(const tside& that) const { return !operator==(that); }

	std::string generate_features(controller_tag::CONTROLLER cntl = controller_tag::AI) const;
public:
	tscenario* scenario_;
	int side_;
	std::string name_;
	int leader_;
	controller_tag::CONTROLLER controller_;
	std::string candidate_cards_;
	std::vector<size_t> holded_cards_;
	std::vector<arms_feature> features_;
	int navigation_;
	int gold_;
	int income_;
	std::string flag_;
	std::set<std::string> build_;
	std::set<std::string> technologies_;
	std::set<int> ally_;
	std::set<int> reserve_heros_;

	// multiplayer dependent
	bool allow_player_;
	
	std::vector<tcity> cities_;
	std::vector<tunit> troops_;
	
	bool except_;
};

class tscenario_
{
public:
	tscenario_::tscenario_(const std::string& id = null_str)
		: id_(id)
		, next_scenario_("null")
		, map_data_()
		, turns_(-1)
		, duel_(RANDOM_DUEL)
		, prelude_()
		, objectives_()
		, enemy_no_city_(true)
		, fallen_to_unstage_(false)
		, tent_(false)
		, shroud_(false)
		, fog_(false)
		, treasures_()
		, roads_()
	{}

public:
	std::string id_;
	std::string next_scenario_;
	std::string map_data_;
	int turns_;
	int duel_;
	config prelude_;
	tevent::tobjectives objectives_;
	bool enemy_no_city_;
	bool fallen_to_unstage_;
	bool tent_;
	bool shroud_;
	bool fog_;
	std::map<int, int> treasures_;
	std::multimap<int, int> roads_;
};

class tscenario: public tscenario_
{
public:
	static std::string default_map_data();
	static std::string file(bool multiplayer, const std::string& campaign_id, const std::string& scenario_id, int index, bool absolute);
	static std::string map_file(bool multiplayer, const std::string& campaign_id, const std::string& scenario_id, int index, bool absolute);

	tscenario(const std::string& campaign_id = null_str, const std::string& id = null_str, int index = 0);
	~tscenario();
	void from_config(int index, const config& cfg);
	void from_ui(HWND hdlgP);
	void from_ui_treasure(HWND hdlgP, bool edit);
	void from_ui_road(HWND hdlgP, bool edit);
	void generate();
	void update_to_ui(HWND hdlgP);
	void update_to_ui_treasures(HWND hdlgP);
	void update_to_ui_roads(HWND hdlgP);
	
	std::string file(bool absolute = false) const;
	std::string map_file(bool absolute = false) const;
	std::string map_data_from_file(const std::string& file) const;
	map_location map_data_size(const std::string& map_data) const;
	int null_side() const;
	bool side_equal() const;
	bool event_equal() const;

	bool new_side();
	void erase_side(int index, HWND hdlgP);

	bool new_event(const std::string& name);
	void erase_event(int index, HWND hdlgP);

	std::map<int, int> generate_cityno_map() const;
	int hero_from_cityno_map(const std::map<int, int>& cityno_map, int no) const;
	std::set<int> get_cities() const;
	bool validate_roads();

	bool rpg_mode() const;

	enum {BIT_ID = 0, BIT_NEXTSCENARIO, BIT_MAP, 
		BIT_TURNS, BIT_MDACTIVITY, BIT_DUEL, BIT_TREASURES, BIT_ROADS, BIT_PRELUDE, BIT_OBJECTIVES, BIT_ENEMYNOCITY, BIT_FALLENTOUNSTAGE, BIT_TENT,
		BIT_SIDE, BIT_EVENT};
	void set_dirty(int bit, bool set);
	
	void clear_except_dirty();

	static std::string state_name(int state);
	enum {STATE_UNKNOWN, STATE_SERVICE, STATE_WANDER, STATE_ARMY, STATE_RESERVE};
	class hero_state 
	{
	public:
		struct tstate {
			tstate(int state = STATE_UNKNOWN, int city = -1) :
				state(state),
				city(city)
			{}
			bool valid() const { return state != STATE_UNKNOWN; }

			int state;
			int city; // hero of city.
		};
		tstate null_state;

		hero_state()
			: states_()
		{}

		void insert(int side, int s, int city)
		{
			states_.insert(std::make_pair(side, tstate(s, city)));
		}

		void erase(int side)
		{
			std::map<int, tstate>::iterator it = states_.find(side);

			std::stringstream err;
			err << "hero_state::erase, this hero hasn't been allocated to #" << (side + 1) << "!";
			VALIDATE(it != states_.end(), err.str());
			states_.erase(it);
		}

		bool allocated(int side, bool exclude_reserve = false) const 
		{ 
			if (side != HEROS_INVALID_SIDE) {
				const std::map<int, tstate>::const_iterator it = states_.find(side);
				if (it == states_.end()) {
					return false;
				}
				if (exclude_reserve && it->second.state == STATE_RESERVE) {
					return false;
				}
				return true;

			} else if (exclude_reserve) {
				for (std::map<int, tstate>::const_iterator it = states_.begin(); it != states_.end(); ++ it) {
					if (it->second.state != STATE_RESERVE) {
						return true;
					}
				}
				return false;

			} else {
				return !states_.empty();
			}
		}

		const tstate& state(int side) const
		{
			std::map<int, tstate>::const_iterator it = states_.find(side);
			if (it != states_.end()) {
				return it->second; 
			}
			return null_state;
		}

		tstate& state(int side)
		{
			std::map<int, tstate>::iterator it = states_.find(side);
			if (it != states_.end()) {
				return it->second; 
			}
			return null_state;
		}

	public:
		std::map<int, tstate> states_;
	};
	void init_hero_state(hero_map& heros);
	void do_state(int h, int side, int state = STATE_UNKNOWN, int city = -1);

public:
	static const int max_event_count = 50;

	std::vector<tside> side_;
	std::vector<tevent> event_;

	std::string campaign_id_;
	int index_;
	std::string file_;

	std::map<int, hero_state> persons_;
	std::map<int, hero_state> artificals_;

	uint32_t dirty_;
	tscenario_ scenario_from_cfg_;

	bool multiplayer_;
private:
	std::vector<tside> side_from_cfg_;
	std::vector<tevent> event_from_cfg_;
};

namespace ns {
	extern tcampaign campaign;
	extern tmain _main;
	extern std::vector<tscenario> _scenario;
	extern const config* campaign_cfg_ptr;

	extern std::map<int, int> cityno_map;

	extern HIMAGELIST himl_checkbox_side;
	extern HIMAGELIST himl_checkbox;

	extern int clicked_side;
	extern int clicked_event;
	extern int clicked_feature;
	extern int clicked_city;
	extern int clicked_troop;
	extern int clicked_hero;

	extern int type;
	extern int current_scenario;
	extern int action_side;
	extern int action_event;
	extern int action_feature;
	extern int action_city;
	extern int action_troop;

	extern void set_dirty();
	extern void new_campaign(const std::string& id, const std::string& firstscenario_id);
	extern bool new_scenario();
	extern void delete_scenario(HWND hdlgP);
}

extern bool campaign_can_save(HWND hdlgP, bool save);

extern void OnEventAddBt(HWND hdlgP, const std::string& name);
extern void OnEventEditBt(HWND hdlgP);
extern void OnEventDelBt(HWND hdlgP);

#endif // _ROSE_EDITOR

#endif // __DLGCAMPAIGNPROC_HPP_
