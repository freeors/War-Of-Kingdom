#ifndef __EDITOR_HPP_
#define __EDITOR_HPP_

#include "config_cache.hpp"
#include "config.hpp"
#ifndef _ROSE_EDITOR
#include "unit_types.hpp"
#endif
#include <windows.h>
#include <set>

#include <boost/bind.hpp>
#include <boost/function.hpp>

typedef enum {
	ma_new				= 1,
	ma_edit				= 3,
} mgr_action_t;

enum {BIN_WML, BIN_BUILDINGRULE};

namespace editor_config
{
	extern config data_cfg;
	extern std::string campaign_id;
	extern int type;

	extern std::vector<std::pair<std::string, std::string> > arms;
#ifndef _ROSE_EDITOR
	extern std::vector<std::pair<std::string, const unit_type*> > artifical_utype;
	extern std::vector<std::pair<std::string, const unit_type*> > city_utypes;
	extern std::vector<std::pair<std::string, const unit_type*> > troop_utypes;
#endif
	extern std::vector<std::string> navigation;
	extern std::vector<std::pair<std::string, const config*> > city_traits;
	extern std::vector<std::pair<std::string, const config*> > troop_traits;

	extern std::set<int> unallocatable_heros;

	void reload_data_bin();

	void move_subcfg_right_position(HWND hdlgP, LPARAM lParam);
	void create_subcfg_toolbar(HWND hwndP);
	void On_DlgDrawItem(HWND hdlgP, const DRAWITEMSTRUCT *lpdis);
	UINT ListView_GetCheckedCount(HWND hwnd);
	DLGTEMPLATE* WINAPI DoLockDlgRes(LPCSTR lpszResName);
}

namespace preferences 
{
	struct base_manager
	{
		base_manager();
		~base_manager();
	};

	void write_preferences();

	void set(const std::string& key, const std::string &value);
	void set(const std::string& key, char const *value);
	void set(const std::string &key, bool value);
	void set(const std::string &key, int value);
	void clear(const std::string& key);
	void set_child(const std::string& key, const config& val);
	const config &get_child(const std::string &key);
	std::string get(const std::string& key);
	bool get(const std::string &key, bool def);
	void erase(const std::string& key);

	std::string language();
	void set_language(const std::string& s);
}

class editor
{
public:
	enum BIN_TYPE {BIN_MIN = 0, MAIN_DATA = BIN_MIN, GUI, LANGUAGE, BIN_SYSTEM_MAX = LANGUAGE, TB_DAT, SCENARIO_DATA, EXTENDABLE};
	struct wml2bin_desc {
		wml2bin_desc();
		std::string bin_name;
		std::string sha1;
		size_t wml_nfiles;
		size_t wml_sum_size;
		time_t wml_modified;
		size_t bin_nfiles;
		size_t bin_sum_size;
		time_t bin_modified;
	};
	editor();

	bool load_game_cfg(const BIN_TYPE type, const char* name = NULL, bool write_file = true, uint32_t nfiles = 0, uint32_t sum_size = 0, uint32_t modified = 0);
	void get_wml2bin_desc_from_wml(std::string& path);
	void reload_extendable_cfg();
	std::string check_scenario_cfg(const config& scenario_cfg);
	std::string check_mplayer_bin(const config& mplayer_cfg);
	std::string check_data_bin(const config& data_cfg);

	std::vector<std::pair<BIN_TYPE, wml2bin_desc> >& wml2bin_descs() { return wml2bin_descs_; }
	const std::vector<std::pair<BIN_TYPE, wml2bin_desc> >& wml2bin_descs() const { return wml2bin_descs_; }

	const config& campaigns_config() const { return campaigns_config_; }

private:
	config game_config_;
	config campaigns_config_;
	config tbs_config_;
	game_config::config_cache& cache_;
	std::vector<std::pair<BIN_TYPE, wml2bin_desc> > wml2bin_descs_;
};

const config& generate_cfg(const config& data_cfg, const std::string& type);

class tcallback_lock
{
public:
	tcallback_lock(bool result, boost::function<void (bool)> callback)
		: result_(result)
		, callback_(callback)
	{}

	~tcallback_lock()
	{
		if (callback_) {
			callback_(result_);
		}
	}
	void set_result(bool val) { result_ = val; }

private:
	bool result_;
	boost::function<void (bool)> callback_;
};

class tcopier
{
public:
	static const std::string current_path_marker;
	enum res_type {res_none, res_file, res_dir, res_files};
	struct tres {
		tres(res_type type, const std::string& name, const std::string& allow_str);

		res_type type;
		std::string name;
		std::set<std::string> allow;
	};
	tcopier()
		: name_()
		, paths_()
		, copy_res_()
		, remove_res_()
	{}
	tcopier(const config& cfg);

	const std::string& name() const { return name_; }
	bool valid() const;
	bool make_path(const std::string& tag) const;
	bool do_copy(const std::string& src_tag, const std::string& dst_tag) const;
	bool do_remove(const std::string& tag) const;
	const std::string& get_path(const std::string& tag) const;

	void set_delete_paths(const std::string& paths);
	void do_delete_path(bool result);

private:
	std::string name_;
	std::map<std::string, std::string> paths_;
	std::vector<tres> copy_res_;
	std::vector<tres> remove_res_;
	std::vector<std::string> delete_paths_;
};

class tmod_config: public tcopier
{
public:
	static const std::string res_tag;
	static const std::string patch_tag;
	tmod_config()
		: tcopier()
		, res_short_path()
	{}
	tmod_config(const config& cfg);
	bool opeate_file(bool patch_2_res);

public:
	std::string res_short_path;
};
extern tmod_config mod_config;

#endif // __EDITOR_HPP_