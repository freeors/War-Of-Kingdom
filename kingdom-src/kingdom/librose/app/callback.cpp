#include "map.hpp"
#include "display.hpp"
#include "help.hpp"
#include "wml_exception.hpp"

void set_zoom_to_default(int zoom)
{
	display::default_zoom_ = zoom;
	image::set_zoom(display::default_zoom_);
}

namespace help {
bool find_topic2(const std::string& dst)
{
	return true;
}
}

namespace help {
std::vector<topic> generate_topics(const bool sort_generated, const std::string &generator)
{
	std::vector<topic> res;
	return res;
}

void generate_sections(const config *help_cfg, const std::string &generator, section &sec, int level)
{
	return;
}
}

void shrouded_and_fogged(const map_location& loc, const void* t, bool& shrouded, bool& fogged)
{
}

bool cb_terrain_matches(const map_location& loc, const t_translation::t_match& terrain_types_match)
{
	return false;
}

void cb_build_terrains(std::map<t_translation::t_terrain, std::vector<map_location> >& terrain_by_type)
{
}

// anim_area
int app_fill_anim_tags(std::map<const std::string, int>& tags)
{
	int index = area_anim::MAX_ROSE_ANIM;
	tags.insert(std::make_pair("card", index ++));
	tags.insert(std::make_pair("pass_scenario", index ++));
	tags.insert(std::make_pair("blade", index ++));
	tags.insert(std::make_pair("pierce", index ++));
	tags.insert(std::make_pair("impact", index ++));
	tags.insert(std::make_pair("archery", index ++));
	tags.insert(std::make_pair("collapse", index ++));
	tags.insert(std::make_pair("arcane", index ++));
	tags.insert(std::make_pair("fire", index ++));
	tags.insert(std::make_pair("cold", index ++));
	tags.insert(std::make_pair("strike", index ++));
	tags.insert(std::make_pair("magic", index ++));
	tags.insert(std::make_pair("heal", index ++));
	tags.insert(std::make_pair("destruct", index ++));
	tags.insert(std::make_pair("formation_defend", index ++));
	tags.insert(std::make_pair("income", index ++));

	return tags.size() - 1;
	// return area_anim::MAX_ROSE_ANIM;
}

void app_fill_anim(int type, const config& cfg)
{
}