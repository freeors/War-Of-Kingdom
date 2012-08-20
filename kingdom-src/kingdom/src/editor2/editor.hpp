#ifndef __EDITOR_HPP_
#define __EDITOR_HPP_

#include "config_cache.hpp"
#include "config.hpp"

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

	bool load_game_cfg(BIN_TYPE type, char* name = NULL, bool write_file = true, uint32_t nfiles = 0, uint32_t sum_size = 0, uint32_t modified = 0);
	void get_wml2bin_desc_from_wml(std::string& path);
	void reload_campaigns_cfg();
	std::string check_scenario_cfg(const config& scenario_cfg);
	std::string check_mplayer_bin(const config& mplayer_cfg);
	std::string check_data_bin(const config& data_cfg);

	std::vector<std::pair<BIN_TYPE, wml2bin_desc> >& wml2bin_descs() { return wml2bin_descs_; }
	const std::vector<std::pair<BIN_TYPE, wml2bin_desc> >& wml2bin_descs() const { return wml2bin_descs_; }

private:
	config game_config_;
	config campaigns_config_;
	game_config::config_cache& cache_;
	std::vector<std::pair<BIN_TYPE, wml2bin_desc> > wml2bin_descs_;
};



#endif // __EDITOR_HPP_