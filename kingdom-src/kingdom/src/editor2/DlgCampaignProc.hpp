#ifndef __DLGCAMPAIGNPROC_HPP_
#define __DLGCAMPAIGNPROC_HPP_

#include "global.hpp"
#include "stdafx.h"
#include <windowsx.h>
#include "config.hpp"
#include "editor.hpp"
#include "hero.hpp"
#include "unit_types.hpp"

#define campaign_enable_save_btn(fEnable)	ToolBar_EnableButton(gdmgr._htb_campaign, IDM_SAVE, fEnable)
#define campaign_get_save_btn()				(ToolBar_GetState(gdmgr._htb_campaign, IDM_SAVE) & TBSTATE_ENABLED)

extern editor editor_;
extern const std::string null_str;

class tcampaign
{
public:
	tcampaign();

	HWND init_toolbar(HINSTANCE hinst, HWND hdlgP);

	void init_hero_state(hero_map& heros);
	void do_state(int h, bool allocate, int city = -1, int state = STATE_UNKNOWN);

	const std::string& arms(const std::string& id);
	int arms_int(const std::string& id);

	const std::string& navigation(const std::string& id);

	std::string city_trait(const std::string& id);
	std::string troop_trait(const std::string& id);

public:
	config game_config_;
	std::string id_;

	enum {STATE_UNKNOWN, STATE_SERVICE, STATE_WANDER, STATE_ARMY};
	class hero_state 
	{
	public:
		hero_state(bool allocated = false, int city = -1, int state = STATE_UNKNOWN)
			: allocated_(allocated)
			, city_(city)
			, state_(state)
		{}
		bool allocated_;
		int city_; // hero of city.
		int state_;
	};
	std::vector<std::pair<std::string, std::string> > arms_;
	std::vector<std::pair<std::string, const unit_type*> > artifical_utype_;
	std::vector<std::pair<std::string, const unit_type*> > city_utypes_;
	std::vector<std::pair<std::string, const unit_type*> > troop_utypes_;
	std::vector<std::string> navigation_;
	std::vector<std::pair<std::string, const config*> > city_traits_;
	std::vector<std::pair<std::string, const config*> > troop_traits_;
	std::map<int, hero_state> persons_;
	std::map<int, hero_state> artificals_;
	std::set<int> reserved_heros_;
};

class tmain_
{
public:
	tmain_(const std::string& id = null_str, const std::string& firstscenario_id = null_str) 
		: textdomain_(PACKAGE)
		, id_(id)
		, rank_(0)
		// , abbrev_(id)
		, abbrev_()
		, first_scenario_(firstscenario_id)
		, hero_data_("^xwml/hero.dat")
		, rpg_mode_(false)
	{}

public:
	std::string textdomain_;
	std::string id_;
	int rank_;
	std::string abbrev_;
	std::string first_scenario_;
	std::string hero_data_;
	bool rpg_mode_;
};

class tmain: public tmain_
{
public:
	tmain(const std::string& id = null_str, const std::string& firstscenario_id = null_str);
	void from_config(const config& cfg);
	void from_ui(HWND hdlgP);
	void update_to_ui(HWND hdlgP);
	void generate();

	std::string description() const;
	std::string icon(bool absolute = false) const;
	std::string image(bool absolute = false) const;

	enum {BIT_ID = 0, BIT_TEXTDOMAIN, BIT_RANK, 
		BIT_ABBREV, BIT_FIRSTSCENARIO, BIT_ICON, 
		BIT_IMAGE, BIT_RPGMODE};
	void set_dirty(int bit, bool set);
public:

	std::string file_;

	uint32_t dirty_;
	tmain_ main_from_cfg_;
};

class tside
{
public:
	class tcity 
	{
	public:
		tcity() 
			: character_(NO_ESPECIAL)
			, mayor_(HEROS_INVALID_NUMBER)
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
			if (character_!= that.character_) return false;
			if (loc_ != that.loc_) return false;
			if (traits_ != that.traits_) return false;
			if (not_recruit_ != that.not_recruit_) return false;
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
		map_location loc_;
		std::set<std::string> traits_;
		std::vector<std::string> not_recruit_;

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

	tside();
	void from_config(const config& side_cfg);
	void from_ui(HWND hdlgP);
	void update_to_ui(HWND hdlgP) const;
	void update_to_ui_side_edit(HWND hdlgP, bool partial = true);

	bool new_feature();
	void erase_feature(int index, HWND hdlgP);

	bool new_city();
	void erase_city(int index, HWND hdlgP);

	bool new_troop();
	void erase_troop(int index, HWND hdlgP);

	bool operator==(const tside& that) const;
	bool operator!=(const tside& that) const { return !operator==(that); }

	enum CONTROLLER {HUMAN, HUMAN_AI, AI, NETWORK, NETWORK_AI, EMPTY};

	std::string generate_features(CONTROLLER cntl = AI) const;
public:
	int side_;
	std::string name_;
	int leader_;
	CONTROLLER controller_;
	bool shroud_;
	bool fog_;
	std::vector<size_t> holded_cards_;
	std::vector<arms_feature> features_;
	int navigation_;
	int gold_;
	int income_;
	std::string flag_;
	std::set<std::string> build_;
	std::set<std::string> technologies_;

	std::vector<tcity> cities_;
	std::vector<tunit> troops_;

	bool except_;
};

class tevent
{
public:
	enum {PARAM_NONE = 0, PARAM_EVENT, PARAM_FILTER, PARAM_SECONDFILTER, 
		PARAM_THEN, PARAM_ELSE, PARAM_COMMAND};

	class tfilter_
	{
	public:
		tfilter_()
			: master_hero_(HEROS_INVALID_NUMBER)
			, must_heros_()
			, city_(-1)
		{}

		virtual void from_config(const config& cfg);
		virtual void from_ui_special(HWND hdlgP);
		virtual void update_to_ui_event_edit(HWND hwndtv, HTREEITEM branch) const;
		virtual void update_to_ui_special(HWND hdlgP) const;

		virtual void generate(std::stringstream& strstr, const std::string& prefix) const;

		virtual bool is_null() const
		{
			if (master_hero_ != HEROS_INVALID_NUMBER) return false;
			if (!must_heros_.empty()) return false;
			if (city_ >= 0) return false;
			return true;
		}
		bool operator==(const tfilter_& that) const
		{
			if (master_hero_ != that.master_hero_) return false;
			if (must_heros_ != that.must_heros_) return false;
			if (city_ != that.city_) return false;
			return true;
		}
		bool operator!=(const tfilter_& that) const { return !operator==(that); }

	public:
		int master_hero_;
		std::set<int> must_heros_;
		int city_;
	};

	class tfilter : public tfilter_
	{
	public:
		tfilter()
			: tfilter_()
		{}
	};

	class tcommand
	{
	public:
		tcommand(int type)
			: type_(type)
		{}
		virtual ~tcommand() {};

		enum {SET_VARIABLE, KILL, JOIN, UNIT, IF, CONDITION};

		virtual void from_config(const config& cfg) {};
		virtual void from_ui_special(HWND) {};
		virtual void update_to_ui_event_edit(HWND, HTREEITEM) const {};
		virtual void update_to_ui_special(HWND) const {};
		virtual void generate(std::stringstream& strstr, const std::string& prefix) const {};

		virtual bool is_null() const { return false; }

	public:
		int type_;
	};

	// [set_variable]
	class tset_variable : public tcommand
	{
	public:
		static const std::string null_value;
		static const std::pair<long, long> null_rand;
		static std::vector<std::pair<std::string, std::string> > op_map;

		tset_variable();

		void from_config(const config& cfg);
		void from_ui_special(HWND hdlgP);
		void update_to_ui_event_edit(HWND hctl, HTREEITEM htvi_root) const;
		void update_to_ui_special(HWND hdlgP) const;

		void generate(std::stringstream& strstr, const std::string& prefix) const
		{
			strstr << prefix << "[set_variable]\n";
			strstr << prefix << "\tname = " << name_ << "\n";
			strstr << prefix << "\t" << op_ << " = " << value_ << "\n";
			strstr << prefix << "[/set_variable]\n";
		}

		bool is_null() const
		{
			if (!name_.empty()) return false;
			return true;
		}
		bool operator==(const tset_variable& that) const
		{
			if (name_ != that.name_) return false;
			if (op_ != that.op_) return false;
			if (value_ != that.value_) return false;
			return true;
		}
		bool operator!=(const tset_variable& that) const { return !operator==(that); }
	public:
		std::string name_;
		std::string op_;
		std::string value_;
	};

	// [kill]
	class tkill : public tcommand
	{
	public:
		tkill()
			: tcommand(KILL)
			, master_hero_(lexical_cast_default<std::string>(HEROS_INVALID_NUMBER))
		{}
		void from_config(const config& cfg);
		void from_ui_special(HWND hdlgP);
		void update_to_ui_event_edit(HWND hctl, HTREEITEM branch) const;
		void update_to_ui_special(HWND hdlgP) const;

		void generate(std::stringstream& strstr, const std::string& prefix) const
		{
			strstr << prefix << "[kill]\n";
			strstr << prefix << "\tmaster_hero = " << master_hero_ << "\n";
			strstr << prefix << "[/kill]\n";
		}

		bool is_null() const
		{
			if (master_hero_ != lexical_cast_default<std::string>(HEROS_INVALID_NUMBER)) return false;
			return true;
		}
		bool operator==(const tkill& that) const
		{
			if (master_hero_ != that.master_hero_) return false;
			return true;
		}
		bool operator!=(const tkill& that) const { return !operator==(that); }
	public:
		std::string master_hero_;
	};

	// [join]
	class tjoin : public tcommand
	{
	public:
		tjoin()
			: tcommand(JOIN)
			, master_hero_(lexical_cast_default<std::string>(HEROS_INVALID_NUMBER))
			, join_(lexical_cast_default<std::string>(HEROS_INVALID_NUMBER))
		{}

		void from_config(const config& cfg);
		void from_ui_special(HWND hdlgP);
		void update_to_ui_event_edit(HWND hwndtv, HTREEITEM branch) const;
		void update_to_ui_special(HWND hdlgP) const;

		void generate(std::stringstream& strstr, const std::string& prefix) const
		{
			strstr << prefix << "[join]\n";
			strstr << prefix << "\tmaster_hero = " << master_hero_ << "\n";
			strstr << prefix << "\tjoin = " << join_ << "\n";
			strstr << prefix << "[/join]\n";
		}

		bool is_null() const
		{
			if (master_hero_ != lexical_cast_default<std::string>(HEROS_INVALID_NUMBER)) return false;
			if (join_ != lexical_cast_default<std::string>(HEROS_INVALID_NUMBER)) return false;
			return true;
		}
		bool operator==(const tjoin& that) const
		{
			if (master_hero_ != that.master_hero_) return false;
			if (join_ != that.join_) return false;
			return true;
		}
		bool operator!=(const tjoin& that) const { return !operator==(that); }
	public:
		std::string master_hero_;
		std::string join_;
	};

	// [unit]
	class tunit : public tcommand
	{
	public:
		tunit();

		void from_config(const config& cfg);
		void from_ui_special(HWND hdlgP);
		void update_to_ui_event_edit(HWND hctl, HTREEITEM branch) const;
		void update_to_ui_special(HWND hdlgP) const;

		void generate(std::stringstream& strstr, const std::string& prefix) const;

		bool is_null() const
		{
			if (!heros_army_.empty()) return false;
			return true;
		}
		bool operator==(const tunit& that) const
		{
			if (type_ != that.type_) return false;
			if (heros_army_ != that.heros_army_) return false;
			if (side_ != that.side_) return false;
			if (city_ != that.city_) return false;
			if (x_ != that.x_) return false;
			if (y_ != that.y_) return false;
			return true;
		}
		bool operator!=(const tunit& that) const { return !operator==(that); }
	public:
		std::string type_;
		std::string heros_army_;
		std::string side_; // diff cfg, side index
		std::string city_; // diff cfg, hero number of city
		std::string x_;
		std::string y_;
	};

	// condition.[variable]
	class tvariable
	{
	public:
		static const std::string null_value;
		static const std::pair<long, long> null_rand;
		static std::vector<std::pair<std::string, std::string> > op_map;

		tvariable();

		void from_config(const config& cfg);
		void from_ui_special(HWND hdlgP);
		void update_to_ui_event_edit(HWND hwndtv, HTREEITEM branch) const;
		void update_to_ui_special(HWND hdlgP) const;

		void generate(std::stringstream& strstr, const std::string& prefix) const
		{
			strstr << prefix << "[variable]\n";
			strstr << prefix << "\tname = " << left_ << "\n";
			strstr << prefix << "\t" << op_ << " = " << right_ << "\n";
			strstr << prefix << "[/variable]\n";
		}
		bool operator==(const tvariable& that) const
		{
			if (left_ != that.left_) return false;
			if (op_ != that.op_) return false;
			if (right_ != that.right_) return false;
			return true;
		}
		bool operator!=(const tvariable& that) const { return !operator==(that); }
	public:
		std::string left_;
		std::string op_;
		std::string right_;
	};

	// condition.[have_unit]
	class thave_unit : public tfilter_
	{
	public:
		thave_unit()
			: tfilter_()
		{}
		void from_config(const config& cfg);
		void update_to_ui_event_edit(HWND hwndtv, HTREEITEM branch) const;
		void update_to_ui_special(HWND hdlgP) const;

		void generate(std::stringstream& strstr, const std::string& prefix) const
		{
			strstr << prefix << "[have_unit]\n";
			tfilter_::generate(strstr, prefix + "\t");
			strstr << prefix << "[/have_unit]\n";
		}
	};

	class tcondition: public tcommand 
	{
	public:
		enum {NONE, VARIABLE, HAVE_UNIT};

		static std::map<int, std::string> name_map;
		static std::map<int, int> idd_map;
		static std::map<int, DLGPROC> dlgproc_map;
		
		tcondition();

		void from_config(const config& cfg);
		void from_ui_special(HWND hdlgP);
		void update_to_ui_event_edit(HWND hwndtv, HTREEITEM branch) const;
		void update_to_ui_special(HWND hdlgP) const;

		void generate(std::stringstream& strstr, const std::string& prefix) const
		{
			if (type_ == VARIABLE) {
				variable_.generate(strstr, prefix);
			} else if (type_ == HAVE_UNIT) {
				have_unit_.generate(strstr, prefix);
			}
		}

		bool operator==(const tcondition& that) const
		{
			if (type_ == VARIABLE) {
				return variable_ == that.variable_;
			} else if (type_ == HAVE_UNIT) {
				return have_unit_ == that.have_unit_;
			}
			return true;
		}
		bool operator!=(const tcondition& that) const { return !operator==(that); }

		void switch_type(HWND hdlgP, int to);

	public:
		int type_;
		tvariable variable_;
		thave_unit have_unit_;
	};

	// [if]
	class tif : public tcommand
	{
	public:
		tif()
			: tcommand(IF)
			, then_()
			, else_()
		{}
		tif(const tif& that);

		~tif()
		{
			for (std::vector<tcommand*>::iterator it = then_.begin(); it != then_.end(); ++ it) {
				delete *it;
			}
			for (std::vector<tcommand*>::iterator it = else_.begin(); it != else_.end(); ++ it) {
				delete *it;
			}
		}

		tif& operator=(const tif& that);

		void from_config(const config& cfg);
		void update_to_ui_event_edit(HWND hctl, HTREEITEM branch) const;

		void generate(std::stringstream& strstr, const std::string& prefix) const
		{
			strstr << prefix << "[if]\n";
			condition_.generate(strstr, prefix + "\t");
			strstr << prefix << "\t[then]\n";
			for (std::vector<tcommand*>::const_iterator it = then_.begin(); it != then_.end(); ++ it) {
				const tcommand* cmd = *it;
				cmd->generate(strstr, prefix + "\t\t");
			}
			strstr << prefix << "\t[/then]\n";
			strstr << prefix << "\t[else]\n";
			for (std::vector<tcommand*>::const_iterator it = else_.begin(); it != else_.end(); ++ it) {
				const tcommand* cmd = *it;
				cmd->generate(strstr, prefix + "\t\t");
			}
			strstr << prefix << "\t[/else]\n";
			strstr << prefix << "[/if]\n";
		}

		bool is_null() const
		{
			if (!then_.empty()) return false;
			if (!else_.empty()) return false;
			return true;
		}
		bool operator==(const tif& that) const
		{
			if (condition_ != that.condition_) return false;
			if (!compare_commands(then_, that.then_)) return false;
			if (!compare_commands(else_, that.else_)) return false;
			return true;
		}
		bool operator!=(const tif& that) const { return !operator==(that); }
	public:
		tcondition condition_;
		std::vector<tcommand*> then_;
		std::vector<tcommand*> else_;
	};

	static std::vector<std::pair<std::string, std::string> > name_map;
	static void from_config_branch(const config& cfg, std::vector<tcommand*>& b);
	static void copy_commands(const std::vector<tcommand*>& src, std::vector<tcommand*>& dst);
	static bool compare_commands(const std::vector<tevent::tcommand*>& left, const std::vector<tevent::tcommand*>& right);

	tevent();
	tevent(const tevent& that);
	~tevent();

	tevent& operator=(const tevent& that);

	void from_config(const config& event_cfg);
	void from_ui(HWND hdlgP);
	
	void update_to_ui(HWND hdlgP, int index = -1) const;
	void update_to_ui_event_edit(HWND hdlgP);
	void update_to_ui_event_edit(HWND htv, HTREEITEM branch) const;

	void generate(std::stringstream& strstr, const std::string& prefix) const;

	bool is_null() const
	{
		if (!commands_.empty()) return false;
		return true;
	}
	bool operator==(const tevent& that) const;
	bool operator!=(const tevent& that) const { return !operator==(that); }

	std::map<std::string, std::string> cumulate_variables(const tcommand* until) const;

	bool locate(const tcommand* until, std::vector<tcommand*>& commands, std::vector<tcommand*>** destination, int& pos);
	void new_command(tcommand* new_cmd, HWND hdlgP);
	void erase_command(const tcommand* cmd, HWND hdlgP);

	bool do_edit(HWND hwndtv);

private:
	bool cumulate_variables_internal(const tcommand* until, const std::vector<tcommand*>& commands, std::map<std::string, std::string>& ret) const;

public:
	std::string name_;
	bool first_time_only_;
	tfilter filter_;
	tfilter second_filter_;
	std::vector<tcommand*> commands_;
};

class tscenario_
{
public:
	tscenario_::tscenario_(const std::string& id = null_str)
		: id_(id)
		, next_scenario_("null")
		, map_data_()
		, turns_(-1)
		, maximal_defeated_activity_(0)
		, win_()
		, lose_()
		, treasures_()
		, roads_()
	{}

public:
	std::string id_;
	std::string next_scenario_;
	std::string map_data_;
	int turns_;
	int maximal_defeated_activity_;
	std::string win_;
	std::string lose_;
	std::map<int, int> treasures_;
	std::multimap<int, int> roads_;
};

class tscenario: public tscenario_
{
public:
	static std::string default_map_data();

	tscenario(const std::string& campaign_id = null_str, const std::string& id = null_str);
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

	enum {BIT_ID = 0, BIT_NEXTSCENARIO, BIT_MAP, 
		BIT_TURNS, BIT_MDACTIVITY, BIT_TREASURES, BIT_ROADS, BIT_WIN, 
		BIT_LOSE, BIT_SIDE, BIT_EVENT};
	void set_dirty(int bit, bool set);
	
	void clear_except_dirty();
public:
	static const int max_event_count = 50;

	std::vector<tside> side_;
	std::vector<tevent> event_;

	std::string campaign_id_;
	int index_;
	std::string file_;

	uint32_t dirty_;
	tscenario_ scenario_from_cfg_;
private:
	std::vector<tside> side_from_cfg_;
	std::vector<tevent> event_from_cfg_;
};

namespace ns {
	extern int empty_leader;

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
}

extern void OnEventAddBt(HWND hdlgP, const std::string& name);
extern void OnEventEditBt(HWND hdlgP);
extern void OnEventDelBt(HWND hdlgP);

#endif // __DLGCAMPAIGNPROC_HPP_
