#include "map.hpp"
#include "display.hpp"
#include "help.hpp"
#include "gettext.hpp"
#include "filesystem.hpp"

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
