#ifndef __EVENT_HPP_
#define __EVENT_HPP_

#ifndef _ROSE_EDITOR

#include "global.hpp"
#include "stdafx.h"
#include <windowsx.h>

namespace digit_op {
enum {NONE, GREATER, LESS, GREATER_EQUAL, LESS_EQUAL};
}

class tcommand
{
public:
	tcommand(int type)
		: type_(type)
		, can_append_(false)
	{}
	virtual ~tcommand() {};

	enum {SET_VARIABLE, KILL, ENDLEVEL, JOIN, UNIT, MODIFY_UNIT, MODIFY_SIDE, MODIFY_CITY, 
		MESSAGE, PRINT, LABEL, ALLOW_UNDO, SIDEHEROS, STORE_UNIT, RENAME, OBJECTIVES,
		EVENT, FILTER, FILTER_HERO, IF, CONDITION};

	virtual void from_config(const config& cfg) {};
	virtual void from_ui_special(HWND) {};
	virtual void update_to_ui_event_edit(HWND, HTREEITEM) const {};
	virtual void update_to_ui_special(HWND) const {};
	virtual void generate(std::stringstream& strstr, const std::string& prefix) const {};

	virtual bool is_null() const { return false; }

public:
	int type_;
	bool can_append_;
};

class ttranslatable_msgid : public tcommand
{
public:
	ttranslatable_msgid(int type);

	virtual void from_ui_special(HWND);
	virtual void update_to_ui_special(HWND) const;

	bool on_command(HWND hwnd, int id, HWND hwndCtrl, UINT codeNotify);
	bool is_null() const 
	{
		if (!value_.empty()) return false;
		return true;
	}
	bool operator==(const ttranslatable_msgid& that) const
	{
		if (value_ != that.value_) return false;
		if (textdomain_ != that.textdomain_) return false;
		return true;
	}
	bool operator!=(const ttranslatable_msgid& that) const { return !operator==(that); }
public:
	std::string value_;
	std::string textdomain_;
};

class tevent : public tcommand
{
public:
	class tevent_name 
	{
	public:
		tevent_name(const std::string& id = null_str, const std::string& name = null_str)
			: id_(id)
			, name_(name)
			, filter_(std::make_pair(false, ""))
			, filter_second_(std::make_pair(false, ""))
			, filter_hero_(std::make_pair(false, ""))
		{}

		std::string id_;
		std::string name_;
		std::pair<bool, std::string> filter_;
		std::pair<bool, std::string> filter_second_;
		std::pair<bool, std::string> filter_hero_;
	};
	static std::vector<tevent_name> name_map;

	enum {PARAM_NONE = 0, PARAM_THEN, PARAM_ELSE};
	// filter + filter second + filter hero
	static const int fixed_commands = 3;

	class tfilter_  : public tcommand
	{
	public:
		tfilter_()
			: tcommand(FILTER)
			, hp_()
			, must_heros_()
			, city_(-1)
			, side_(HEROS_INVALID_SIDE)
			, controller_(controller_tag::NONE)
			, type_()
			, family_()
			, x_()
			, y_()
			, filter_location_()
		{}

		virtual void from_config(const config& cfg);
		virtual void from_ui_special(HWND hdlgP);
		virtual void update_to_ui_event_edit(HWND hwndtv, HTREEITEM branch) const;
		virtual void update_to_ui_special(HWND hdlgP) const;

		std::string description(HWND hwndtv, HTREEITEM branch, bool newline) const;

		virtual void generate(std::stringstream& strstr, const std::string& prefix) const;

		virtual bool is_null() const
		{
			if (hp_.first != digit_op::NONE && !hp_.second.empty()) return false;
			if (!must_heros_.empty()) return false;
			if (city_ >= 0) return false;
			if (side_ != HEROS_INVALID_SIDE) return false;
			if (controller_ != controller_tag::NONE) return false;
			if (!type_.empty()) return false;
			if (!family_.empty()) return false;
			if (!x_.empty() && !y_.empty()) return false;
			if (!filter_location_.empty()) return false;
			return true;
		}
		bool operator==(const tfilter_& that) const
		{
			if (hp_ != that.hp_) return false;
			if (must_heros_ != that.must_heros_) return false;
			if (city_ != that.city_) return false;
			if (side_ != that.side_) return false;
			if (controller_ != that.controller_) return false;
			if (type_ != that.type_) return false;
			if (family_ != that.family_) return false;
			if (x_ != that.x_) return false;
			if (y_ != that.y_) return false;
			if (filter_location_ != that.filter_location_) return false;
			return true;
		}
		bool operator!=(const tfilter_& that) const { return !operator==(that); }

	public:
		std::pair<int, std::string> hp_;
		std::string must_heros_;
		int city_;
		int side_;
		controller_tag::CONTROLLER controller_;
		std::string type_;
		std::string family_;
		std::string x_;
		std::string y_;
		config filter_location_;
	};

	class tfilter_hero_  : public tcommand
	{
	public:
		tfilter_hero_()
			: tcommand(FILTER_HERO)
			, number_(HEROS_INVALID_NUMBER)
		{}

		virtual void from_config(const config& cfg);
		virtual void from_ui_special(HWND hdlgP);
		virtual void update_to_ui_event_edit(HWND hwndtv, HTREEITEM branch) const;
		virtual void update_to_ui_special(HWND hdlgP) const;

		virtual void generate(std::stringstream& strstr, const std::string& prefix) const;

		virtual bool is_null() const
		{
			if (number_ != HEROS_INVALID_NUMBER) return false;
			return true;
		}
		bool operator==(const tfilter_hero_& that) const
		{
			if (number_ != that.number_) return false;
			return true;
		}
		bool operator!=(const tfilter_hero_& that) const { return !operator==(that); }

	public:
		int number_;
	};

	// [set_variable]
	class tset_variable : public ttranslatable_msgid
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

		void generate(std::stringstream& strstr, const std::string& prefix) const;

		bool is_null() const
		{
			if (!ttranslatable_msgid::is_null()) return false;
			if (!name_.empty()) return false;
			return true;
		}
		bool operator==(const tset_variable& that) const
		{
			if (ttranslatable_msgid::operator!=(that)) return false;
			if (name_ != that.name_) return false;
			if (op_ != that.op_) return false;
			return true;
		}
		bool operator!=(const tset_variable& that) const { return !operator==(that); }
	public:
		std::string name_;
		std::string op_;
	};

	// [kill]
	class tkill : public tcommand
	{
	public:
		tkill()
			: tcommand(KILL)
			, master_hero_(lexical_cast_default<std::string>(HEROS_INVALID_NUMBER))
			, side_()
			, direct_hero_(false)
		{}
		void from_config(const config& cfg);
		void from_ui_special(HWND hdlgP);
		void update_to_ui_event_edit(HWND hctl, HTREEITEM branch) const;
		void update_to_ui_special(HWND hdlgP) const;

		void generate(std::stringstream& strstr, const std::string& prefix) const;

		bool is_null() const
		{
			if (master_hero_ != lexical_cast_default<std::string>(HEROS_INVALID_NUMBER)) return false;
			return true;
		}
		bool operator==(const tkill& that) const
		{
			if (master_hero_ != that.master_hero_) return false;
			if (side_ != that.side_) return false;
			if (direct_hero_ != that.direct_hero_) return false;
			return true;
		}
		bool operator!=(const tkill& that) const { return !operator==(that); }
	public:
		std::string master_hero_;
		std::string side_; // same as cfg, side number/variable
		bool direct_hero_;
	};

	// [endlevel]
	class tendlevel : public tcommand
	{
	public:
		tendlevel()
			: tcommand(ENDLEVEL)
			, result_("victory")
		{}
		void from_config(const config& cfg);
		void from_ui_special(HWND hdlgP);
		void update_to_ui_event_edit(HWND hctl, HTREEITEM branch) const;
		void update_to_ui_special(HWND hdlgP) const;

		void generate(std::stringstream& strstr, const std::string& prefix) const
		{
			strstr << prefix << "[endlevel]\n";
			strstr << prefix << "\tresult = " << result_ << "\n";
			strstr << prefix << "[/endlevel]\n";
		}

		bool is_null() const
		{
			if (!result_.empty()) return false;
			return true;
		}
		bool operator==(const tendlevel& that) const
		{
			if (result_ != that.result_) return false;
			return true;
		}
		bool operator!=(const tendlevel& that) const { return !operator==(that); }
	public:
		std::string result_;
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
			strstr << prefix << "\tto = " << master_hero_ << "\n";
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
			if (traits_ != that.traits_) return false;
			if (state_ != that.state_) return false;
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
		std::set<std::string> traits_;
		std::set<std::string> state_;
	};

	// [modify_unit]
	class tmodify_unit : public tcommand
	{
	public:
		tmodify_unit()
			: tcommand(MODIFY_UNIT)
			, master_hero_(lexical_cast_default<std::string>(HEROS_INVALID_NUMBER))
			, effect_()
		{}

		void from_config(const config& cfg);
		void from_ui_special(HWND hdlgP);
		void update_to_ui_event_edit(HWND hctl, HTREEITEM branch) const;
		void update_to_ui_special(HWND hdlgP) const;

		void generate(std::stringstream& strstr, const std::string& prefix) const;

		bool is_null() const
		{
			if (master_hero_ == lexical_cast_default<std::string>(HEROS_INVALID_NUMBER)) return true;
			if (effect_.empty()) return true;
			return false;
		}
		bool operator==(const tmodify_unit& that) const
		{
			if (master_hero_ != that.master_hero_) return false;
			if (effect_ != that.effect_) return false;
			return true;
		}
		bool operator!=(const tmodify_unit& that) const { return !operator==(that); }
	public:
		std::string master_hero_;
		config effect_;
	};

	// [modify_side]
	class tmodify_side : public tcommand
	{
	public:
		tmodify_side()
			: tcommand(MODIFY_SIDE)
			, side_(HEROS_INVALID_SIDE)
			, leader_(HEROS_INVALID_NUMBER)
			, controller_(controller_tag::NONE)
			, gold_(-1)
			, income_(-1)
			, agree_()
			, terminate_()
			, exclude_human_(false)
			, technology_()
		{}

		void from_config(const config& cfg);
		void from_ui_special(HWND hdlgP);
		void update_to_ui_event_edit(HWND hctl, HTREEITEM branch) const;
		void update_to_ui_special(HWND hdlgP) const;

		void generate(std::stringstream& strstr, const std::string& prefix) const;

		bool is_null() const
		{
			if (side_ != HEROS_INVALID_SIDE) return false;
			if (leader_ != HEROS_INVALID_SIDE) return false;
			if (controller_ != controller_tag::NONE) return false;
			if (gold_ != -1) return false;
			if (income_ != -1) return false;
			if (!agree_.empty()) return false;
			if (!terminate_.empty()) return false;
			if (!technology_.empty()) return false;
			return true;
		}
		bool operator==(const tmodify_side& that) const
		{
			if (side_ != that.side_) return false;
			if (leader_ != that.leader_) return false;
			if (controller_ != that.controller_) return false;
			if (gold_ != that.gold_) return false;
			if (income_ != that.income_) return false;
			if (agree_ != that.agree_) return false;
			if (terminate_ != that.terminate_) return false;
			if ((!agree_.empty() || !terminate_.empty()) && exclude_human_ != that.exclude_human_) return false;
			if (technology_ != that.technology_) return false;
			return true;
		}
		bool operator!=(const tmodify_side& that) const { return !operator==(that); }

		void update_to_ui_ally(HWND hdlgP) const;
		void update_ally(HWND hdlgP, int side, bool agree, bool insert);
	public:
		int side_;
		int leader_;
		controller_tag::CONTROLLER controller_;
		int gold_;
		int income_;
		std::set<int> agree_;
		std::set<int> terminate_;
		bool exclude_human_;
		std::set<std::string> technology_;
	};

	// [modify_city]
	class tmodify_city : public tcommand
	{
	public:
		tmodify_city()
			: tcommand(MODIFY_CITY)
			, city_(std::make_pair(null_str, HEROS_INVALID_NUMBER))
			, soldiers_(-1)
			, service_()
		{}

		void from_config(const config& cfg);
		void from_ui_special(HWND hdlgP);
		void update_to_ui_event_edit(HWND hctl, HTREEITEM branch) const;
		void update_to_ui_special(HWND hdlgP) const;

		void generate(std::stringstream& strstr, const std::string& prefix) const;

		bool is_null() const
		{
			if (city_.first.empty() && city_.second == HEROS_INVALID_NUMBER) return true;
			return false;
		}
		bool operator==(const tmodify_city& that) const
		{
			if (city_.first != that.city_.first) return false;
			if (city_.first.empty() && city_.second != that.city_.second) return false;
			if (soldiers_ != that.soldiers_) return false;
			if (service_ != that.service_) return false;
			return true;
		}
		bool operator!=(const tmodify_city& that) const { return !operator==(that); }

	public:
		std::pair<std::string, int> city_;
		int soldiers_;
		std::set<int> service_;
	};

	// [message]
	class tmessage : public tcommand
	{
	public:
		tmessage()
			: tcommand(MESSAGE)
			, hero_(lexical_cast_default<std::string>(HEROS_INVALID_NUMBER))
			, incident_(-1)
			, yesno_(false)
			, caption_()
			, message_()
		{}

		void from_config(const config& cfg);
		void from_ui_special(HWND hdlgP);
		void update_to_ui_event_edit(HWND hctl, HTREEITEM branch) const;
		void update_to_ui_special(HWND hdlgP) const;

		void generate(std::stringstream& strstr, const std::string& prefix) const;

		bool is_null() const
		{
			if (hero_ == lexical_cast_default<std::string>(HEROS_INVALID_NUMBER)) return true;
			if (caption_.empty() && message_.empty()) return true;
			return false;
		}
		bool operator==(const tmessage& that) const
		{
			if (hero_ != that.hero_) return false;
			if (incident_ != that.incident_) return false;
			if (yesno_ != that.yesno_) return false;
			if (caption_ != that.caption_) return false;
			if (message_ != that.message_) return false;
			return true;
		}
		bool operator!=(const tmessage& that) const { return !operator==(that); }
	public:
		std::string hero_;
		int incident_;
		bool yesno_;
		std::string caption_;
		std::string message_;
		std::string textdomain_;
	};

	// [print]
	class tprint : public tcommand
	{
	public:
		tprint()
			: tcommand(PRINT)
			, text_()
			, size_(18)
			, duration_(10000)
		{
			color_.push_back(255); // red
			color_.push_back(255); // green
			color_.push_back(255); // blue
		}

		void from_config(const config& cfg);
		void from_ui_special(HWND hdlgP);
		void update_to_ui_event_edit(HWND hctl, HTREEITEM branch) const;
		void update_to_ui_special(HWND hdlgP) const;

		void generate(std::stringstream& strstr, const std::string& prefix) const;

		bool is_null() const
		{
			return false;
		}
		bool operator==(const tprint& that) const
		{
			if (text_ != that.text_) return false;
			if (!text_.empty()) {
				if (size_ != that.size_) return false;
				if (duration_ != that.duration_) return false;
				if (color_ != that.color_) return false;
			}
			return true;
		}
		bool operator!=(const tprint& that) const { return !operator==(that); }
	public:
		std::string text_;
		int size_;
		int duration_;
		std::vector<int> color_;
	};

	// [label]
	class tlabel : public tcommand
	{
	public:
		tlabel()
			: tcommand(LABEL)
			, text_()
			, x_("")
			, y_("")
		{
		}

		void from_config(const config& cfg);
		void from_ui_special(HWND hdlgP);
		void update_to_ui_event_edit(HWND hctl, HTREEITEM branch) const;
		void update_to_ui_special(HWND hdlgP) const;

		void generate(std::stringstream& strstr, const std::string& prefix) const;

		bool is_null() const
		{
			return false;
		}
		bool operator==(const tlabel& that) const
		{
			if (text_ != that.text_) return false;
			if (x_ != that.x_) return false;
			if (y_ != that.y_) return false;
			return true;
		}
		bool operator!=(const tlabel& that) const { return !operator==(that); }
	public:
		std::string text_;
		std::string x_;
		std::string y_;
	};

	// [allow_undo]
	class tallow_undo : public tcommand
	{
	public:
		tallow_undo()
			: tcommand(ALLOW_UNDO)
		{}

		void from_config(const config& cfg) {};
		void from_ui_special(HWND hdlgP) {};
		void update_to_ui_event_edit(HWND hwndtv, HTREEITEM branch) const;
		void update_to_ui_special(HWND hdlgP) const {}

		void generate(std::stringstream& strstr, const std::string& prefix) const
		{
			strstr << prefix << "[allow_undo]\n";
			strstr << prefix << "[/allow_undo]\n";
		}

		bool is_null() const
		{
			return false;
		}
		bool operator==(const tallow_undo& that) const
		{
			return true;
		}
		bool operator!=(const tallow_undo& that) const { return !operator==(that); }
	};

	// [sideheros]
	class tsideheros : public tcommand
	{
	public:
		tsideheros()
			: tcommand(SIDEHEROS)
			, side_()
			, heros_()
		{
		}

		void from_config(const config& cfg);
		void from_ui_special(HWND hdlgP);
		void update_to_ui_event_edit(HWND hctl, HTREEITEM branch) const;
		void update_to_ui_special(HWND hdlgP) const;

		void generate(std::stringstream& strstr, const std::string& prefix) const;

		bool is_null() const
		{
			if (side_.empty()) return true;
			if (heros_.empty()) return true;
			return false;
		}
		bool operator==(const tsideheros& that) const
		{
			if (side_ != that.side_) return false;
			if (heros_ != that.heros_) return false;
			return true;
		}
		bool operator!=(const tsideheros& that) const { return !operator==(that); }
	public:
		std::string side_;
		std::set<int> heros_;
	};

	// [store_unit]
	class tstore_unit : public tcommand
	{
	public:
		enum {ALWAYS_CLEAR = 0, REPLACE, APPEND};
		static std::map<int, std::pair<std::string, std::string> > mode_map;

		tstore_unit();

		void from_config(const config& cfg);
		void from_ui_special(HWND hdlgP);
		void update_to_ui_event_edit(HWND hctl, HTREEITEM branch) const;
		void update_to_ui_special(HWND hdlgP) const;

		void generate(std::stringstream& strstr, const std::string& prefix) const;

		bool is_null() const
		{
			if (filter_.is_null()) return true;
			if (variable_.empty()) return true;
			return false;
		}
		bool operator==(const tstore_unit& that) const
		{
			if (filter_ != that.filter_) return false;
			if (variable_ != that.variable_) return false;
			if (mode_ != that.mode_) return false;
			if (kill_ != that.kill_) return false;
			return true;
		}
		bool operator!=(const tstore_unit& that) const { return !operator==(that); }
	public:
		tfilter_ filter_;
		std::string variable_;
		int mode_;
		bool kill_;
	};

	// [rename]
	class trename : public ttranslatable_msgid
	{
	public:
		trename();

		void from_config(const config& cfg);
		void from_ui_special(HWND hdlgP);
		void update_to_ui_event_edit(HWND hctl, HTREEITEM branch) const;
		void update_to_ui_special(HWND hdlgP) const;

		void generate(std::stringstream& strstr, const std::string& prefix) const;

		bool is_null() const
		{
			if (ttranslatable_msgid::is_null()) return true;
			if (number_ == HEROS_INVALID_NUMBER) return true;
			return false;
		}
		bool operator==(const trename& that) const
		{
			if (ttranslatable_msgid::operator!=(that)) return false;
			if (number_ != that.number_) return false;
			return true;
		}
		bool operator!=(const trename& that) const { return !operator==(that); }
	public:
		int number_;
	};

	// [objectives]
	class tobjectives : public tcommand
	{
	public:
		tobjectives()
			: tcommand(OBJECTIVES)
			, summary_()
			, win_()
			, lose_()
		{}

		void from_config(const config& cfg);
		void from_ui_special(HWND hdlgP);
		void update_to_ui_event_edit(HWND hctl, HTREEITEM branch) const;
		void update_to_ui_special(HWND hdlgP) const;

		void from_ui_special_msgid(HWND hdlgP);

		void generate(std::stringstream& strstr, const std::string& prefix) const;

		bool is_null() const
		{
			if (!summary_.empty()) return false;
			if (!win_.empty()) return false;
			if (!lose_.empty()) return false;
			return true;
		}
		bool operator==(const tobjectives& that) const
		{
			if (summary_ != that.summary_) return false;
			if (win_ != that.win_) return false;
			if (lose_ != that.lose_) return false;
			return true;
		}
		bool operator!=(const tobjectives& that) const { return !operator==(that); }
	public:
		std::string summary_;
		std::vector<std::string> win_;
		std::vector<std::string> lose_;
	};

	class tcondition: public tcommand 
	{
	public:
		enum {NONE, VARIABLE, HAVE_UNIT};
		enum {LOGIC_NONE, LOGIC_NOT, LOGIC_AND, LOGIC_OR};

		class tjudge : public tfilter_
		{
		public:
			static std::vector<std::pair<std::string, std::string> > op_map;
			
			tjudge(int type, int logic);
			
			void from_config(const config& cfg);
			void from_ui_special(HWND hdlgP);
			void update_to_ui_event_edit(HWND hwndtv, HTREEITEM branch) const;
			void update_to_ui_special(HWND hdlgP) const;

			void generate(std::stringstream& strstr, const std::string& prefix) const
			{
				if (type_ == VARIABLE) {
					strstr << prefix << "[variable]\n";
					strstr << prefix << "\tname = " << left_ << "\n";
					strstr << prefix << "\t" << op_ << " = " << right_ << "\n";
					strstr << prefix << "[/variable]\n";
				} else if (type_ == HAVE_UNIT) {
					strstr << prefix << "[have_unit]\n";
					tfilter_::generate(strstr, prefix + "\t");
					strstr << prefix << "\tcount = " << count_ << "\n";
					strstr << prefix << "[/have_unit]\n";
				}
			}

			bool operator==(const tjudge& that) const
			{
				if (type_ != that.type_) return false;
				if (logic_ != that.logic_) return false;
				if (type_ == VARIABLE) {
					if (left_ != that.left_) return false;
					if (op_ != that.op_) return false;
					if (right_ != that.right_) return false;
				} else if (type_ == HAVE_UNIT) {
					if (tfilter_::operator!=(that)) return false;
					if (count_ != that.count_) return false;
				}
				return true;
			}
			bool operator!=(const tjudge& that) const { return !operator==(that); }

			std::string description(bool to_tree) const;

		public:
			int type_;
			int logic_;

			// variable depended on
			std::string left_;
			std::string op_;
			std::string right_;

			// have_unit depended on
			std::string count_;
		};

		static std::map<int, std::pair<std::string, std::string> > logic_map;
		
		tcondition();

		void from_config(const config& cfg);
		void from_ui_special(HWND hdlgP);
		void update_to_ui_event_edit(HWND hwndtv, HTREEITEM branch) const;
		void update_to_ui_special(HWND hdlgP) const;

		void generate(std::stringstream& strstr, const std::string& prefix) const
		{
			for (std::vector<tjudge>::const_iterator it = judges_.begin(); it != judges_.end(); ++ it) {
				const tjudge& judge = *it;
				if (judge.logic_ != LOGIC_NONE) {
					std::string sub_prefix = prefix + "\t";
					strstr << prefix << "[" << logic_map.find(judge.logic_)->second.first << "]\n";
					judge.generate(strstr, sub_prefix);
					strstr << prefix << "[/" << logic_map.find(judge.logic_)->second.first << "]\n";
				} else {
					judge.generate(strstr, prefix);
				}
			}
		}

		bool operator==(const tcondition& that) const
		{
			if (judges_ != that.judges_) return false;
			return true;
		}
		bool operator!=(const tcondition& that) const { return !operator==(that); }

		void fill_logic_cmb(HWND hctl) const;
	private:
		void from_config_internal(const config& cfg, int logic);

	public:
		std::vector<tjudge> judges_;
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

	static void from_config_branch(const config& cfg, std::vector<tcommand*>& b);
	static void copy_commands(const std::vector<tcommand*>& src, std::vector<tcommand*>& dst);
	static bool compare_commands(const std::vector<tevent::tcommand*>& left, const std::vector<tevent::tcommand*>& right);
	static void fill_name_map();
	static std::vector<tevent_name>::const_iterator find_name(const std::string& id);

	tevent();
	tevent(const tevent& that);
	~tevent();

	tevent& operator=(const tevent& that);

	void from_config(const config& event_cfg);
	void from_ui(HWND hdlgP);
	
	void update_to_ui(HWND hdlgP, int index = -1) const;
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
	// in order to use same "location" function, commands include "fixed" command, ie. filter, second fitler
	std::vector<tcommand*> commands_;
};

namespace ns {
	extern tevent::tcommand* clicked_command;

	extern int clicked_item;
}

#endif // _ROSE_EDITOR

#endif // __EVENT_HPP_
