#ifndef __EDITOR_HPP_
#define __EDITOR_HPP_

#include "config_cache.hpp"
#include "config.hpp"
#include <windows.h>

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

	extern void move_subcfg_right_position(HWND hdlgP, LPARAM lParam);
	extern void create_subcfg_toolbar(HWND hwndP);
	extern void On_DlgDrawItem(HWND hdlgP, const DRAWITEMSTRUCT *lpdis);
	extern UINT ListView_GetCheckedCount(HWND hwnd);
	extern DLGTEMPLATE* WINAPI DoLockDlgRes(LPCSTR lpszResName);
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
	enum BIN_TYPE {BIN_MIN = 0, MAIN_DATA = BIN_MIN, MULTI_PLAYER, EDITOR, GUI, LANGUAGE, BIN_SYSTEM_MAX = LANGUAGE, SCENARIO_DATA, CAMPAIGNS, BIN_MAX = CAMPAIGNS};
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
	void reload_campaigns_cfg();
	std::string check_scenario_cfg(const config& scenario_cfg);
	std::string check_mplayer_bin(const config& mplayer_cfg);
	std::string check_data_bin(const config& data_cfg);

	std::vector<std::pair<BIN_TYPE, wml2bin_desc> >& wml2bin_descs() { return wml2bin_descs_; }
	const std::vector<std::pair<BIN_TYPE, wml2bin_desc> >& wml2bin_descs() const { return wml2bin_descs_; }

	const config& campaigns_config() const { return campaigns_config_; }

private:
	config game_config_;
	config campaigns_config_;
	game_config::config_cache& cache_;
	std::vector<std::pair<BIN_TYPE, wml2bin_desc> > wml2bin_descs_;
};



#endif // __EDITOR_HPP_