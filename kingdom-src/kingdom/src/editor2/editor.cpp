#include "global.hpp"
#include "game_config.hpp"
#include "filesystem.hpp"
#include "sha1.hpp"
#include "language.hpp"
#include "loadscreen.hpp"
#include "editor.hpp"
#include <set>
#include <sys/stat.h>
#include "wml_exception.hpp"
#include "gettext.hpp"

#include "win32x.h"
#include "struct.h"

#include "animation.hpp"
#include "builder.hpp"
#include <boost/foreach.hpp>
#include <boost/bind.hpp>

// language.cpp有init_textdomains函数体,但要把language.cpp加起来会出现很多unsoluve linke, 在这里实现一个
// 这里实现和language.cpp中是一样的
static void init_textdomains(const config& cfg)
{
	BOOST_FOREACH (const config &t, cfg.child_range("textdomain"))
	{
		const std::string &name = t["name"];
		const std::string &path = t["path"];

		if(path.empty()) {
			t_string::add_textdomain(name, get_intl_dir());
		} else {
			std::string location = get_binary_dir_location("", path);

			if (location.empty()) {
				//if location is empty, this causes a crash on Windows, so we
				//disallow adding empty domains
			} else {
				t_string::add_textdomain(name, location);
			}
		}
	}
}

editor::wml2bin_desc::wml2bin_desc() :
	bin_name(),
	sha1(),
	wml_nfiles(0),
	wml_sum_size(0),
	wml_modified(0),
	bin_nfiles(0),
	bin_sum_size(0),
	bin_modified(0)
{}

#define BASENAME_DATA		"data.bin"
#define BASENAME_DATA2		"data2.bin"
#define BASENAME_GUI		"gui.bin"
#define BASENAME_LANGUAGE	"language.bin"
#define BASENAME_CAMPAIGNS	"campaigns.bin"
#define BASENAME_TB			"tb.dat"

editor::editor() :
	cache_(game_config::config_cache::instance())
	, wml2bin_descs_()
{
}

// check location:
//   1. heros_army of artifcal
//   2. service_heros of artifcal
//   3. wander_heros of artifcal
//   4. heros_army of unit
std::string editor::check_scenario_cfg(const config& scenario_cfg)
{
	std::set<std::string> holded_str;
	std::set<int> holded_number;
	std::set<std::string> officialed_str;
	std::map<std::string, std::set<std::string> > officialed_map;
	std::map<std::string, std::string> mayor_map;
	int number;
	std::vector<std::string> str_vec;
	std::vector<std::string>::const_iterator tmp;
	std::stringstream str;

	BOOST_FOREACH (const config& side, scenario_cfg.child_range("side")) {
		const std::string leader = side["leader"];
		BOOST_FOREACH (const config& art, side.child_range("artifical")) {
			officialed_str.clear();
			const std::string cityno = art["cityno"].str();
			mayor_map[cityno] = art["mayor"].str();

			str_vec = utils::split(art["heros_army"]);
			for (tmp = str_vec.begin(); tmp != str_vec.end(); ++ tmp) {
				if (holded_str.count(*tmp)) {
					str << "." << scenario_cfg["id"].str() << ", hero number: " << *tmp << " is conflicted!";
					return str.str();
				}
				number = lexical_cast_default<int>(*tmp);
				if (holded_number.count(number)) {
					str << "." << scenario_cfg["id"].str() << ", hero number: " << *tmp << " is invalid!";
					return str.str();
				}
				holded_str.insert(*tmp);
				holded_number.insert(number);
			}
			str_vec = utils::split(art["service_heros"]);
			for (tmp = str_vec.begin(); tmp != str_vec.end(); ++ tmp) {
				if (holded_str.count(*tmp)) {
					str << "." << scenario_cfg["id"].str() << ", hero number: " << *tmp << " is conflicted!";
					return str.str();
				}
				number = lexical_cast_default<int>(*tmp);
				if (holded_number.count(number)) {
					str << "." << scenario_cfg["id"].str() << ", hero number: " << *tmp << " is invalid!";
					return str.str();
				}
				holded_str.insert(*tmp);
				holded_number.insert(number);
				officialed_str.insert(*tmp);
			}
			str_vec = utils::split(art["wander_heros"]);
			for (tmp = str_vec.begin(); tmp != str_vec.end(); ++ tmp) {
				if (holded_str.count(*tmp)) {
					str << "." << scenario_cfg["id"].str() << ", hero number: " << *tmp << " is conflicted!";
					return str.str();
				}
				number = lexical_cast_default<int>(*tmp);
				if (holded_number.count(number)) {
					str << "." << scenario_cfg["id"].str() << ", hero number: " << *tmp << " is invalid!";
					return str.str();
				}
				holded_str.insert(*tmp);
				holded_number.insert(number);
			}
			officialed_map[cityno] = officialed_str;
		}
		BOOST_FOREACH (const config& u, side.child_range("unit")) {
			const std::string cityno = u["cityno"].str();
			std::map<std::string, std::set<std::string> >::iterator find_it = officialed_map.find(cityno);
			if (cityno != "0" && find_it == officialed_map.end()) {
				str << "." << scenario_cfg["id"].str() << ", heros_army=" << u["heros_army"].str() << " uses undefined cityno: " << cityno << "";
				return str.str();
			}
			str_vec = utils::split(u["heros_army"]);
			for (tmp = str_vec.begin(); tmp != str_vec.end(); ++ tmp) {
				if (holded_str.count(*tmp)) {
					str << "." << scenario_cfg["id"].str() << ", hero number: " << *tmp << " is conflicted!";
					return str.str();
				}
				number = lexical_cast_default<int>(*tmp);
				if (holded_number.count(number)) {
					str << "." << scenario_cfg["id"].str() << ", hero number: " << *tmp << " is invalid!";
					return str.str();
				}
				holded_str.insert(*tmp);
				holded_number.insert(number);
				if (find_it != officialed_map.end()) {
					find_it->second.insert(*tmp);
				}
			}
		}
		for (std::map<std::string, std::set<std::string> >::const_iterator it = officialed_map.begin(); it != officialed_map.end(); ++ it) {
			std::map<std::string, std::string>::const_iterator mayor_it = mayor_map.find(it->first);
			if (mayor_it->second.empty()) {
				continue;
			}
			if (mayor_it->second == leader) {
				str << "." << scenario_cfg["id"].str() << ", in cityno=" << it->first << " mayor(" << mayor_it->second << ") cannot be leader!";
				return str.str();
			}
			if (it->second.find(mayor_it->second) == it->second.end()) {
				str << "." << scenario_cfg["id"].str() << ", in ciytno=" << it->first << " mayor(" << mayor_it->second << ") must be in offical hero!";
				return str.str();
			}
		}
	}
	return "";
}

// check location:
//   1. heros_army of artifcal
//   2. service_heros of artifcal
//   3. wander_heros of artifcal
std::string editor::check_mplayer_bin(const config& mplayer_cfg)
{
	std::set<std::string> holded_str;
	std::set<int> holded_number;
	int number;
	std::vector<std::string> str_vec;
	std::vector<std::string>::const_iterator tmp;
	std::stringstream str;

	BOOST_FOREACH (const config& faction, mplayer_cfg.child_range("faction")) {
		BOOST_FOREACH (const config& art, faction.child_range("artifical")) {
			str_vec = utils::split(art["heros_army"]);
			for (tmp = str_vec.begin(); tmp != str_vec.end(); ++ tmp) {
				if (holded_str.count(*tmp)) {
					str << "hero number: " << *tmp << " is conflicted!";
					return str.str();
				}
				number = lexical_cast_default<int>(*tmp);
				if (holded_number.count(number)) {
					str << "hero number: " << *tmp << " is invalid!";
					return str.str();
				}
				holded_str.insert(*tmp);
				holded_number.insert(number);
			}
			str_vec = utils::split(art["service_heros"]);
			for (tmp = str_vec.begin(); tmp != str_vec.end(); ++ tmp) {
				if (holded_str.count(*tmp)) {
					str << "hero number: " << *tmp << " is conflicted!";
					return str.str();
				}
				number = lexical_cast_default<int>(*tmp);
				if (holded_number.count(number)) {
					str << "hero number: " << *tmp << " is invalid!";
					return str.str();
				}
				holded_str.insert(*tmp);
				holded_number.insert(number);
			}
			str_vec = utils::split(art["wander_heros"]);
			for (tmp = str_vec.begin(); tmp != str_vec.end(); ++ tmp) {
				if (holded_str.count(*tmp)) {
					str << "hero number: " << *tmp << " is conflicted!";
					return str.str();
				}
				number = lexical_cast_default<int>(*tmp);
				if (holded_number.count(number)) {
					str << "hero number: " << *tmp << " is invalid!";
					return str.str();
				}
				holded_str.insert(*tmp);
				holded_number.insert(number);
			}
		}
	}
	return "";
}

// check location:
std::string editor::check_data_bin(const config& data_cfg)
{
	std::stringstream str;

	BOOST_FOREACH (const config& campaign, data_cfg.child_range("campaign")) {
		if (!campaign.has_attribute("id")) {
			str << "Compaign hasn't id!";
			return str.str();
		}
	}
	return "";
}

bool editor::load_game_cfg(const editor::BIN_TYPE type, const char* name, bool write_file, uint32_t nfiles, uint32_t sum_size, uint32_t modified)
{
	config tmpcfg, foreground;

	// 以下这句如果不加，（后续）生成的cache-v0.1.1-xxxxx.define.gz会出错，大小上大于应该的字节数
	// 但测试下来，对真正的须的cache-v0.1.1-xxxx.gz这个文件没影响
	game_config::config_cache_transaction main_transaction;

	try {
		tmpcfg.clear();
		game_config_.clear();
		cache_.clear_defines();
		if (type == editor::SCENARIO_DATA) {
			std::string name_str = std::string(name);
			const size_t pos_ext = name_str.rfind(".");
			name_str = name_str.substr(0, pos_ext);

			config& campaign_cfg = campaigns_config_.find_child("campaign", "id", name_str);
			cache_.add_define(campaign_cfg["define"].str());
			cache_.add_define("EASY");
			cache_.get_config(game_config::path + "/data", game_config_);
			// ::init_textdomains(game_config_);
			
			// extract [compaign_addon] block
			config& refcfg = game_config_.child("campaign_addon");
			BOOST_FOREACH (const config &i, game_config_.child_range("textdomain")) {
				refcfg.add_child("textdomain", i);
			}

			// check scenario config valid
			BOOST_FOREACH (const config& scenario, refcfg.child_range("scenario")) {
				std::string err_str = check_scenario_cfg(scenario);
				if (!err_str.empty()) {
					throw game::error(std::string("<") + name + std::string(">") + err_str);
				}
			}

			if (write_file) {
				wml_config_to_file(game_config::path + "/xwml/campaigns/" + name, refcfg, nfiles, sum_size, modified);
			}

		} else if (type == editor::GUI) {
			// no pre-defined
			cache_.get_config(game_config::path + "/data/gui", game_config_);
			// ::init_textdomains(game_config_);
			if (write_file) {
				wml_config_to_file(game_config::path + "/xwml/" + BASENAME_GUI, game_config_, nfiles, sum_size, modified);
			}

		} else if (type == editor::LANGUAGE)  {
			// no pre-defined
			cache_.get_config(game_config::path + "/data/languages", game_config_);
			// ::init_textdomains(game_config_);
			if (write_file) {
				wml_config_to_file(game_config::path + "/xwml/" + BASENAME_LANGUAGE, game_config_, nfiles, sum_size, modified);
			}
		} else if (type == editor::CAMPAIGNS)  {
			// no pre-defined
			const std::string file = game_config::path + "/data/campaigns.cfg";
			if (file_exists(file)) {
				cache_.get_config(game_config::path + "/data/campaigns.cfg", campaigns_config_);
				if (write_file) {
					wml_config_to_file(game_config::path + "/xwml/" + name, campaigns_config_, nfiles, sum_size, modified);
				}
			}
		} else {
			// type == editor::MAIN_DATA
			cache_.add_define("CORE");
			cache_.get_config(game_config::path + "/data", game_config_);
			// ::init_textdomains(game_config_);

			// check scenario config valid
			std::string err_str = check_data_bin(game_config_);
			if (!err_str.empty()) {
				throw game::error(std::string("<") + BASENAME_DATA + std::string(">") + err_str);
			}

			if (write_file) {
				wml_config_to_file(game_config::path + "/xwml/" + BASENAME_DATA2, game_config_, nfiles, sum_size, modified);

				write_tb_dat_if();
			}
			// remove all [terrain_graphics] tags
			game_config_.clear_children("terrain_graphics");
			if (write_file) {
				wml_config_to_file(game_config::path + "/xwml/" + BASENAME_DATA, game_config_, nfiles, sum_size, modified);
			}
			editor_config::data_cfg = game_config_;

			editor_config::reload_data_bin();
		} 
	}
	catch (game::error& e) {
		// gui2::show_error_message(disp().video(), _("Error loading game configuration files: '") + e.message + _("' (The game will now exit)"));
		MessageBox(NULL, e.message.c_str(), "Error", MB_OK | MB_ICONWARNING);
		return false;
	}
	return true;
}

void editor::reload_campaigns_cfg()
{
	load_game_cfg(CAMPAIGNS, BASENAME_CAMPAIGNS, false);
	// load_game_cfg will translate relative msgid without load textdomain.
	// result of load_game_cfg used to known what campaign, not detail information.
	// To detail information, need load textdomain, so call t_string::reset_translations(), 
	// let next translate correctly.
	t_string::reset_translations();
}

// @path: c:\kingdom-res\data
void editor::get_wml2bin_desc_from_wml(std::string& path)
{
	editor::wml2bin_desc desc;
	file_tree_checksum dir_checksum;
	std::stringstream defines_string;
	std::vector<std::string> short_paths;
	std::string bin_to_path = game_config::path + "/xwml";

	std::vector<editor::BIN_TYPE> bin_types;
	for (editor::BIN_TYPE type = editor::BIN_MIN; type <= editor::BIN_SYSTEM_MAX; type = (editor::BIN_TYPE)(type + 1)) {
		bin_types.push_back(type);
	}
	// search <data>/campaigns, and form [campaign].bin
	std::string campaigns_path = path + "/campaigns";
	std::vector<std::string> campaigns;

	BOOST_FOREACH (const config& cfg, campaigns_config_.child_range("campaign")) {
		campaigns.push_back(cfg["id"].str());
		bin_types.push_back(editor::SCENARIO_DATA);
	}

	wml2bin_descs_.clear();

	size_t campaign_index = 0;
	for (std::vector<editor::BIN_TYPE>::const_iterator itor = bin_types.begin(); itor != bin_types.end(); ++ itor) {
		editor::BIN_TYPE type = *itor;

		defines_string.str("");
		short_paths.clear();
		int filter = SKIP_MEDIA_DIR;
		if (type == editor::SCENARIO_DATA) {
			short_paths.push_back("data/core");
			short_paths.push_back(std::string("data/campaigns/") + campaigns[campaign_index]);
			filter |= SKIP_GUI_DIR | SKIP_INTERNAL_DIR | SKIP_TERRAIN | SKIP_BOOK;

			desc.bin_name = campaigns[campaign_index] + ".bin";
			
			defines_string << path;
			defines_string << " " << campaigns[campaign_index];
			defines_string << " EASY";

			bin_to_path = game_config::path + "/xwml/campaigns";

			campaign_index ++;

		} else if (type == editor::GUI) {
			short_paths.push_back("data/gui");

			desc.bin_name = BASENAME_GUI;

			defines_string << path + "/gui";

		} else if (type == editor::LANGUAGE) {
			short_paths.push_back("data/languages");

			desc.bin_name = BASENAME_LANGUAGE;

			defines_string << path + "/languages";

		} else {
			// (type == editor::MAIN_DATA)
			// no pre-defined
			short_paths.push_back("data");
			filter |= SKIP_SCENARIO_DIR | SKIP_GUI_DIR;

			desc.bin_name = BASENAME_DATA;

			defines_string << path;
			defines_string << "CORE";
		}
		sha1_hash sha(defines_string.str());
		desc.sha1 = sha.display();

		data_tree_checksum(short_paths, dir_checksum, filter);

		desc.wml_nfiles = dir_checksum.nfiles;
		desc.wml_sum_size = dir_checksum.sum_size;
		desc.wml_modified = dir_checksum.modified;

		if (!wml_checksum_from_file(bin_to_path + "/" + desc.bin_name, &desc.bin_nfiles, &desc.bin_sum_size, (uint32_t*)&desc.bin_modified)) {
			desc.bin_nfiles = desc.bin_sum_size = desc.bin_modified = 0;
		}

		wml2bin_descs_.push_back(std::pair<BIN_TYPE, wml2bin_desc>(type, desc));
	}

	return;
}

void editor::write_tb_dat_if() const
{
	editor::wml2bin_desc desc;
	file_tree_checksum dir_checksum;
	std::vector<std::string> short_paths;
	const std::string bin_to_path = game_config::path + "/xwml";

	short_paths.push_back("data/core/terrain-graphics");
	short_paths.push_back("data/core/images/terrain");
	desc.bin_name = BASENAME_TB;

	int filter = 0;
	data_tree_checksum(short_paths, dir_checksum, filter);
	desc.wml_nfiles = dir_checksum.nfiles;
	desc.wml_sum_size = dir_checksum.sum_size;
	desc.wml_modified = dir_checksum.modified;

	struct stat st;
	const std::string terrain_graphics_cfg = game_config::path + "/data/core/terrain-graphics.cfg";
	if (::stat(terrain_graphics_cfg.c_str(), &st) != -1) {
		if (st.st_mtime > desc.wml_modified) {
			desc.wml_modified = st.st_mtime;
		}
		desc.wml_sum_size += st.st_size;
		desc.wml_nfiles ++;
	}

	if (!wml_checksum_from_file(bin_to_path + "/" + desc.bin_name, &desc.bin_nfiles, &desc.bin_sum_size, (uint32_t*)&desc.bin_modified)) {
		desc.bin_nfiles = desc.bin_sum_size = desc.bin_modified = 0;
	}

	if (desc.wml_nfiles != desc.bin_nfiles || desc.wml_sum_size != desc.bin_sum_size || desc.wml_modified != desc.bin_modified) {
		binary_paths_manager paths_manager(game_config_);
		terrain_builder::release_heap();
		terrain_builder::set_terrain_rules_cfg(game_config_);

		terrain_builder("off-map/alpha.png", desc.wml_nfiles, desc.wml_sum_size, desc.wml_modified);
	}
}

const config& generate_cfg(const config& data_cfg, const std::string& type)
{
	BOOST_FOREACH (const config& c, data_cfg.child_range("generate")) {
		if (type == c["type"].str()) {
			return c;
		}
	}
	return null_cfg;
}

const std::string tcopier::current_path_marker = ".";

tcopier::tres::tres(res_type type, const std::string& name, const std::string& allow_str)
	: type(type)
	, name(name)
	, allow()
{
	if (!allow_str.empty()) {
		std::vector<std::string> vstr = utils::split(allow_str, '-');
		for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
			allow.insert(*it);
		}
	}
}

tcopier::tcopier(const config& cfg)
	: name_(cfg["name"])
	, copy_res_()
	, remove_res_()
{
	const std::string path_prefix = "path-";
	BOOST_FOREACH (const config::attribute& attr, cfg.attribute_range()) {
		if (attr.first.find(path_prefix) != 0) {
			continue;
		}
		std::string tag = attr.first.substr(path_prefix.size());
		std::string path = attr.second;
		std::replace(path.begin(), path.end(), '/', '\\');
		if (path.at(path.size() - 1) == '\\') {
			path.erase(path.size() - 1);
		}
		paths_.insert(std::make_pair(tag, path));
	}

	set_delete_paths(cfg["delete_paths"].str());

	const config& res_cfg = cfg.child("resource");
	if (!res_cfg) {
		return;
	}
	std::map<std::string, std::vector<tres>* > v;
	v.insert(std::make_pair("copy", &copy_res_));
	v.insert(std::make_pair("remove", &remove_res_));

	for (std::map<std::string, std::vector<tres>* >::const_iterator it = v.begin(); it != v.end(); ++ it) {
		const config& op_cfg = res_cfg.child(it->first);
		if (op_cfg) {
			BOOST_FOREACH (const config::attribute& attr, op_cfg.attribute_range()) {
				std::vector<std::string> vstr = utils::split(attr.second);
				VALIDATE(vstr.size() == 2, "resource item must be 2!");
				res_type type = res_none;
				if (vstr[0] == "file") {
					type = res_file;
				} else if (vstr[0] == "dir") {
					type = res_dir;
				} else if (vstr[0] == "files") {
					type = res_files;
				}
				VALIDATE(type != res_none, "error resource type, must be file or dir!");
				std::string name = vstr[1];
				std::replace(name.begin(), name.end(), '/', '\\');

				std::string allow_str;
				size_t pos = attr.first.find('-');
				if (pos != std::string::npos) {
					allow_str = attr.first.substr(pos + 1);
				}
				it->second->push_back(tres(type, name, allow_str));
			}
		}
	}
}

const std::string& tcopier::get_path(const std::string& tag) const
{
	std::stringstream err;
	std::map<std::string, std::string>::const_iterator it = paths_.find(tag);
	err << "Invalid tag: " << tag;
	VALIDATE(it != paths_.end(), err.str());
	return it->second;
}

bool tcopier::valid() const
{
	if (paths_.empty()) {
		return false;
	}
	for (std::map<std::string, std::string>::const_iterator it = paths_.begin(); it != paths_.end(); ++ it) {
		const std::string& path = it->second;
		if (path.size() < 2 || path.at(1) != ':') {
			return false;
		}
	}
	return true;
}

static std::string type_name(int tag)
{
	if (tag == tcopier::res_file) {
		return _("File");
	} else if (tag == tcopier::res_dir) {
		return _("Directory");
	} else if (tag == tcopier::res_files) {
		return _("Files");
	}
	return null_str;
}

bool tcopier::make_path(const std::string& tag) const
{
	utils::string_map symbols;
	const std::string& path = get_path(tag);

	size_t pos = path.rfind("\\");
	if (pos == std::string::npos) {
		return true;
	}

	std::string subpath = path.substr(0, pos);
	MakeDirectory(subpath);

	if (!delfile1(path.c_str())) {
		symbols["type"] = _("Directory");
		symbols["dst"] = path;
		posix_print_mb(utf8_2_ansi(vgettext2("Delete $type, from $dst fail!", symbols).c_str()));
		return false;
	}
	return true;
}

bool tcopier::do_copy(const std::string& src_tag, const std::string& dst_tag) const
{
	const std::string& src_path = get_path(src_tag);
	const std::string& dst_path = get_path(dst_tag);
	utils::string_map symbols;
	bool fok = true;
	std::string src, dst;

	// copy
	if (is_directory(src_path)) {
		for (std::vector<tres>::const_iterator it = copy_res_.begin(); it != copy_res_.end(); ++ it) {
			const tres r = *it;
			if (!r.allow.empty() && r.allow.find(dst_tag) == r.allow.end()) {
				continue;
			}
			src = src_path + "\\" + r.name;
			dst = dst_path + "\\" + r.name;
			if (r.type == res_file) {
				if (!is_file(src.c_str())) {
					continue;
				}
				MakeDirectory(dst.substr(0, dst.rfind('\\')));
			} else if (r.type == res_dir || r.type == res_files) {
				if (r.name == current_path_marker) {
					src = src_path;
					dst = dst_path;
				}
				if (!is_directory(src.c_str())) {
					continue;
				}
				if (r.type == res_dir) {
					// make sure system don't exsit dst! FO_COPY requrie it.
					if (!delfile1(dst.c_str())) {
						symbols["type"] = _("Directory");
						symbols["dst"] = dst;
						posix_print_mb(utf8_2_ansi(vgettext2("Delete $type, from $dst fail!", symbols).c_str()));
						fok = false;
						break;
					}
				} else if (r.type == res_files) {
					MakeDirectory(dst);
				}
			}
			if (r.type == res_file || r.type == res_dir) {
				fok = copyfile(src.c_str(), dst.c_str());
			} else {
				fok = copy_root_files(src.c_str(), dst.c_str());
			}
			if (!fok) {
				symbols["type"] = type_name(r.type);
				symbols["src"] = src;
				symbols["dst"] = dst;
				posix_print_mb(utf8_2_ansi(vgettext2("Copy $type, from $src to $dst fail!", symbols).c_str()));
				break;
			}
		}
	}
	
	return fok;
}

bool tcopier::do_remove(const std::string& tag) const
{
	const std::string& path = get_path(tag);
	utils::string_map symbols;
	bool fok = true;
	std::string src, dst;

	// remove
	for (std::vector<tres>::const_iterator it = remove_res_.begin(); it != remove_res_.end(); ++ it) {
		const tres r = *it;
		if (!r.allow.empty() && r.allow.find(tag) == r.allow.end()) {
			continue;
		}
		dst = path + "\\" + r.name;
		if (r.type == res_file) {
			if (!is_file(dst.c_str())) {
				continue;
			}
		} else {
			if (!is_directory(dst.c_str())) {
				continue;
			}
		}
		if (!delfile1(dst.c_str())) {
			symbols["type"] = r.type == res_file? _("File"): _("Directory");
			symbols["dst"] = dst;
			posix_print_mb(utf8_2_ansi(vgettext2("Delete $type, from $dst fail!", symbols).c_str()));
			fok = false;
			break;
		}
	}

	return fok;
}

void tcopier::set_delete_paths(const std::string& paths)
{
	delete_paths_.clear();
	std::vector<std::string> vstr = utils::split(paths);
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		const std::string& path = *it;
		if (paths_.find(path) != paths_.end()) {
			delete_paths_.push_back(get_path(path));
		} else {
			delete_paths_.push_back(path);
		}
	}
}

void tcopier::do_delete_path(bool result)
{
	if (!result) {
		utils::string_map symbols;
		for (std::vector<std::string>::const_iterator it = delete_paths_.begin(); it != delete_paths_.end(); ++ it) {
			const std::string& path = *it;
			if (!is_directory(path)) {
				continue;
			}
			symbols["name"] = path;
			if (!delfile1(path.c_str())) {
				posix_print_mb(utf8_2_ansi(vgettext2("Delete: $name fail!", symbols).c_str()));
				return;
			}
		}
	}
	delete_paths_.clear();
}

const std::string tmod_config::res_tag = "res";
const std::string tmod_config::patch_tag = "patch";

tmod_config::tmod_config(const config& cfg)
	: tcopier(cfg)
	, res_short_path()
{
	const std::string& res_path = get_path(res_tag);

	if (!res_path.empty()) {
		size_t pos = res_path.rfind('\\');
		if (pos != std::string::npos) {
			res_short_path = res_path.substr(pos + 1);
		}
	}
}

bool tmod_config::opeate_file(bool patch_2_res)
{
	const std::string& patch_path = get_path(patch_tag);
	const std::string& src_tag = patch_2_res? patch_tag: res_tag;
	const std::string& dst_tag = patch_2_res? res_tag: patch_tag;

	if (!patch_2_res) {
		set_delete_paths(patch_path);
	}
	tcallback_lock lock(false, boost::bind(&tcopier::do_delete_path, this, _1));

	if (!patch_2_res && !make_path(patch_tag)) {
		return false;
	}

	// remove
	if (patch_2_res && !do_remove(res_tag)) {
		return false;
	}

	// copy
	if (!do_copy(src_tag, dst_tag)) {
		return false;
	}

	lock.set_result(true);
	
	return true;
}