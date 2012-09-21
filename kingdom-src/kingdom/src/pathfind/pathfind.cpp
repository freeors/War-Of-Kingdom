/* $Id: pathfind.cpp 46186 2010-09-01 21:12:38Z silene $ */
/*
   Copyright (C) 2003 by David White <dave@whitevine.net>
   Copyright (C) 2005 - 2010 by Guillaume Melquiond <guillaume.melquiond@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 * @file
 * Various pathfinding functions and utilities.
 */

#include "global.hpp"

#include "pathfind/pathfind.hpp"

#include "foreach.hpp"
#include "game_display.hpp"
#include "gettext.hpp"
#include "log.hpp"
#include "map.hpp"
#include "resources.hpp"
#include "team.hpp"
#include "unit.hpp"
#include "unit_map.hpp"
#include "wml_exception.hpp"

#include <iostream>
#include <vector>
#include <algorithm>

static lg::log_domain log_engine("engine");
#define ERR_PF LOG_STREAM(err, log_engine)

map_location pathfind::find_vacant_tile(const gamemap& map,
				const unit_map& units,
				const map_location& loc,
				pathfind::VACANT_TILE_TYPE vacancy,
				const unit* pass_check)
{
	if (!map.on_board(loc)) return map_location();
	std::set<map_location> pending_tiles_to_check, tiles_checked;
	pending_tiles_to_check.insert(loc);
	// Iterate out 50 hexes from loc
	for (int distance = 0; distance < 50; ++distance) {
		if (pending_tiles_to_check.empty())
			return map_location();
		//Copy over the hexes to check and clear the old set
		std::set<map_location> tiles_checking;
		tiles_checking.swap(pending_tiles_to_check);
		//Iterate over all the hexes we need to check
		foreach (const map_location &loc, tiles_checking)
		{
			//If the unit cannot reach this area or it's not a castle but should, skip it.
			if ((vacancy == pathfind::VACANT_CASTLE && !map.is_castle(loc))
			|| (pass_check && pass_check->movement_cost(map[loc])
					== unit_movement_type::UNREACHABLE))
				continue;
			//If the hex is empty, return it.
			if (units.find(loc) == units.end())
				return loc;
			map_location adjs[6];
			get_adjacent_tiles(loc,adjs);
			foreach (const map_location &loc, adjs)
			{
				if (!map.on_board(loc)) continue;
				// Add the tile to be checked if it hasn't already been and
				// isn't being checked.
				if (tiles_checked.find(loc) == tiles_checked.end() &&
				    tiles_checking.find(loc) == tiles_checking.end())
				{
					pending_tiles_to_check.insert(loc);
				}
			}
		}
		tiles_checked.swap(tiles_checking);
	}
	return map_location();
}

bool pathfind::enemy_zoc(std::vector<team> const &teams,
	map_location const &loc, team const &viewing_team, int side, bool see_all, bool ignore_city)
{
	map_location locs[6];
	const team &current_team = teams[side-1];
	get_adjacent_tiles(loc,locs);
	for (int i = 0; i != 6; ++i)
	{
		const unit *u = get_visible_unit(locs[i], viewing_team, see_all);
		if (u && u->side() != side && current_team.is_enemy(u->side()) &&
		    u->emits_zoc())
		{
			return (!ignore_city || !u->is_city())? true: false;
		}
	}

	return false;
}

std::set<map_location> pathfind::get_teleport_locations(const unit &u,
	const team &viewing_team,
	bool see_all, bool ignore_units)
{
	std::set<map_location> res;
	if (!u.get_ability_bool("teleport")) return res;

	const team &current_team = (*resources::teams)[u.side() - 1];
	const map_location &loc = u.get_location();
	foreach (const map_location &l, current_team.villages())
	{
		// This must be a vacant village (or occupied by the unit)
		// to be able to teleport.
		if (!see_all && viewing_team.is_enemy(u.side()) && viewing_team.fogged(l))
			continue;
		if (!ignore_units && l != loc &&
		    get_visible_unit(l, viewing_team, see_all))
			continue;
		res.insert(l);
	}
	return res;
}

static unsigned search_counter;

namespace {

struct node {
	int movement_left, move_cost, turns_left;
	map_location prev, curr;

	/**
	 * If equal to search_counter, the node is off the list.
	 * If equal to search_counter + 1, the node is on the list.
	 * Otherwise it is outdated.
	 */
	unsigned in;

	node(int moves, int cost, int turns, const map_location &p, const map_location &c)
		: movement_left(moves)
		, move_cost(cost)
		, turns_left(turns)
		, prev(p)
		, curr(c)
		, in(0)
	{
	}

	node()
		: movement_left(0)
		, move_cost(0)
		, turns_left(0)
		, prev()
		, curr()
		, in(0)
	{
	}

	bool operator<(const node& o) const {
		return turns_left > o.turns_left
				|| (turns_left == o.turns_left
						&& movement_left > o.movement_left);
	}
	bool operator<=(const node& o) const {
		return turns_left > o.turns_left
				|| (turns_left == o.turns_left
						&& movement_left >= o.movement_left);
	}
};

struct indexer {
	int w, h;
	indexer(int a, int b) : w(a), h(b) { }
	int operator()(const map_location& loc) const {
		return loc.y * w + loc.x;
	}
};

struct comp {
	const std::vector<node>& nodes;
	comp(const std::vector<node>& n) : nodes(n) { }
	bool operator()(int l, int r) const {
		return nodes[r] < nodes[l];
	}
};
}

static void find_routes(const gamemap& map, const unit_map& units,
		const unit& u, const map_location& loc,
		int move_left, pathfind::paths::dest_vect &destinations,
		std::vector<team> const &teams,
		bool force_ignore_zocs, bool allow_teleport, int turns_left,
		const team &viewing_team,
		bool see_all, bool ignore_units)
{
	const team& current_team = teams[u.side() - 1];
	std::set<map_location> teleports;
	if (allow_teleport) {
	  teleports = pathfind::get_teleport_locations(u, viewing_team, see_all, ignore_units);
	}

	const int total_movement = u.total_movement();

	// prepare self-city grid condition
	void* expediting_city_cookie = NULL;
	if (units.expediting()) {
		expediting_city_cookie = units.expediting_city_node();
	} else if (units.city_from_loc(loc)) {
		expediting_city_cookie = units.get_cookie(loc);
	}

	std::vector<map_location> locs(6 + teleports.size());
	std::copy(teleports.begin(), teleports.end(), locs.begin() + 6);

	search_counter += 2;
	if (search_counter == 0) search_counter = 2;

	static std::vector<node> nodes;
	nodes.resize(map.w() * map.h());

	indexer index(map.w(), map.h());
	comp node_comp(nodes);

	int xmin = loc.x, xmax = loc.x, ymin = loc.y, ymax = loc.y, nb_dest = 1;

	nodes[index(loc)] = node(move_left, 0, turns_left, map_location::null_location, loc);
	std::vector<int> pq;
	if (move_left || turns_left) {
		pq.push_back(index(loc));
	}

	while (!pq.empty()) {
		node& n = nodes[pq.front()];
		std::pop_heap(pq.begin(), pq.end(), node_comp);
		pq.pop_back();
		n.in = search_counter;

		pathfind::last_location = n.curr;
		get_adjacent_tiles(n.curr, &locs[0]);
		for (int i = teleports.count(n.curr) ? locs.size() : 6; i-- > 0; ) {
			if (!locs[i].valid(map.w(), map.h())) continue;

			node& next = nodes[index(locs[i])];

			// be strange, display next in vs.net 2008 debug is mistake, it is direct to node in astrsearch.cpp.
			bool next_visited = next.in - search_counter <= 1u;

			// Classic Dijkstra allow to skip chosen nodes (with next.in==search_counter)
			// But the cost function and hex grid allow to also skip visited nodes:
			// if next was visited, then we already have a path 'src-..-n2-next'
			// - n2 was chosen before n, meaning that it is nearer to src.
			// - the cost of 'n-next' can't be smaller than 'n2-next' because
			//   cost is independent of direction and we don't have more MP at n
			//   (important because more MP may allow to avoid waiting next turn)
			// Thus, 'src-..-n-next' can't be shorter.
			// if (next_visited) continue;

			// because add wall, n2-next cost is dependent of direction! below statement is fall short.
			// u.movement_cost is a bit complex, try reduce call times.
			// only wall, only it dependent of direction!
			unit_map::node* curr_node = units.get_cookie(locs[i], false);
			if (!curr_node || !curr_node->second->wall()) {
				if (next_visited) continue;
			}

			int move_cost;
			if (expediting_city_cookie && units.get_cookie(locs[i]) == expediting_city_cookie) {
				move_cost = 0;
			} else {
				if (curr_node && curr_node->second->wall()) {
					move_cost = pathfind::location_cost(units, u, false);

				} else {
					move_cost = u.movement_cost(map[locs[i]], &locs[i]);
				}
			}

			node t = node(n.movement_left, move_cost, n.turns_left, n.curr, locs[i]);
			if (t.movement_left < move_cost) {
				t.movement_left = total_movement;
				t.turns_left--;
			}

			if (t.movement_left < move_cost || t.turns_left < 0) continue;

			t.movement_left -= move_cost;

			if (!ignore_units) {
				const unit *v = get_visible_unit(locs[i], viewing_team, see_all);
				if (v && current_team.is_enemy(v->side()))
					continue;

				// move cost in any terrain cannot be zero. it is zero indicate loc is self-city grid.
				if (move_cost && (!v || !v->cancel_zoc()) && !force_ignore_zocs && t.movement_left > 0
				    && pathfind::enemy_zoc(teams, locs[i], viewing_team, u.side(), see_all)
						&& !u.get_ability_bool("skirmisher", locs[i])) {
					t.movement_left = 0;
				}
			}

			if (next_visited && next <= t) continue;

			++nb_dest;
			int x = locs[i].x;
			if (x < xmin) xmin = x;
			if (xmax < x) xmax = x;
			int y = locs[i].y;
			if (y < ymin) ymin = y;
			if (ymax < y) ymax = y;

			bool in_list = next.in == search_counter + 1;
			t.in = search_counter + 1;
			next = t;
			
			// if already in the priority queue then we just update it, else push it.
			if (in_list) { // never happen see next_visited above
				std::push_heap(pq.begin(), std::find(pq.begin(), pq.end(), index(locs[i])) + 1, node_comp);
			} else {
				pq.push_back(index(locs[i]));
				std::push_heap(pq.begin(), pq.end(), node_comp);
			}
		}
	}

	// Build the routes for every map_location that we reached.
	// The ordering must be compatible with map_location::operator<.
	destinations.reserve(nb_dest);
	for (int x = xmin; x <= xmax; ++x) {
		for (int y = ymin; y <= ymax; ++y)
		{
			const node &n = nodes[index(map_location(x, y))];
			if (n.in - search_counter > 1u) continue;
			pathfind::paths::step s =
				{ n.curr, n.prev, n.movement_left + n.turns_left * total_movement };
			destinations.push_back(s);
		}
	}
}

static pathfind::paths::dest_vect::iterator lower_bound(pathfind::paths::dest_vect &v, const map_location &loc)
{
	size_t sz = v.size(), pos = 0;
	while (sz)
	{
		if (v[pos + sz / 2].curr < loc) {
			pos = pos + sz / 2 + 1;
			sz = sz - sz / 2 - 1;
		} else sz = sz / 2;
	}
	return v.begin() + pos;
}

pathfind::paths::dest_vect::const_iterator pathfind::paths::dest_vect::find(const map_location &loc) const
{
	const_iterator i = lower_bound(const_cast<dest_vect &>(*this), loc), i_end = end();
	if (i != i_end && i->curr != loc) i = i_end;
	return i;
}

void pathfind::paths::dest_vect::insert(const map_location &loc)
{
	iterator i = lower_bound(*this, loc), i_end = end();
	if (i != i_end && i->curr == loc) return;
	paths::step s = { loc, map_location(), 0 };
	std::vector<step>::insert(i, s);
}

/**
 * Returns the path going from the source point (included) to the
 * destination point @a j (excluded).
 */
std::vector<map_location> pathfind::paths::dest_vect::get_path(const const_iterator &j) const
{
	std::vector<map_location> path;
	if (!j->prev.valid()) {
		path.push_back(j->curr);
	} else {
		const_iterator i = j;
		do {
			i = find(i->prev);
			assert(i != end());
			path.push_back(i->curr);
		} while (i->prev.valid());
	}
	std::reverse(path.begin(), path.end());
	return path;
}

bool pathfind::paths::dest_vect::contains(const map_location &loc) const
{
	return find(loc) != end();
}

pathfind::paths::paths(gamemap const &map, unit_map const &units,
		map_location const &loc, std::vector<team> const &teams,
		bool force_ignore_zoc, bool allow_teleport, const team &viewing_team,
		int additional_turns, bool see_all, bool ignore_units)
	: destinations()
{
	const unit_map::const_iterator i = units.find(loc);
	if(i == units.end()) {
		ERR_PF << "paths::paths() -- unit not found\n";
		return;
	}

	if (i->side() < 1 || i->side() > int(teams.size())) {
		return;
	}

	find_routes(map, units, *i, loc,
		i->movement_left(), destinations, teams, force_ignore_zoc,
		allow_teleport,additional_turns,viewing_team,
		see_all, ignore_units);
}

pathfind::paths::paths(gamemap const &map, unit_map const &units, const unit &u,
		map_location const &loc, std::vector<team> const &teams,
		bool force_ignore_zoc, bool allow_teleport, const team &viewing_team,
		int additional_turns, bool see_all, bool ignore_units)
	: destinations()
{
	if(u.side() < 1 || u.side() > int(teams.size())) {
		return;
	}

	find_routes(map, units, u, loc,
		u.movement_left(), destinations, teams, force_ignore_zoc,
		allow_teleport,additional_turns,viewing_team,
		see_all, ignore_units);
}

pathfind::marked_route pathfind::mark_route(const plain_route &rt,
		const std::vector<map_location>& waypoints)
{
	marked_route res;

	if (rt.steps.empty()) return marked_route();
	res.route = rt;

	unit_map& units = *resources::units;
	unit_map::const_iterator it = resources::units->find(rt.steps.front());
	if (it == resources::units->end()) return marked_route();
	unit const& u = *it;

	int turns = 0;
	int movement = u.movement_left();
	const team& unit_team = (*resources::teams)[u.side()-1];
	bool zoc = false;

	std::vector<map_location>::const_iterator i = rt.steps.begin(),
			w = waypoints.begin();

	for (; i !=rt.steps.end(); ++i) {
		bool last_step = (i+1 == rt.steps.end());

		// move_cost of the next step is irrelevant for the last step
		assert(last_step || resources::game_map->on_board(*(i+1)));

		pathfind::last_location = *i;
		int move_cost;
		if (last_step) {
			move_cost = 0;

		} else {
			unit_map::node* curr_node = reinterpret_cast<unit_map::node*>(units.get_cookie(*(i + 1), false));
			if (curr_node && curr_node->second->wall()) {
				move_cost = location_cost(units, u, false);

			} else {
				move_cost = u.movement_cost((*resources::game_map)[*(i+1)]);
			}
		}

		// const int move_cost = last_step ? 0 : u.movement_cost((*resources::game_map)[*(i+1)]);
		bool pass_here = false;
		if (w != waypoints.end() && *i == *w) {
			++w;
			pass_here = true;
		}

		team const& viewing_team = (*resources::teams)[resources::screen->viewing_team()];

		// check if there is a village that we could capture
		// if it's an enemy unit and a fogged village, we assume it's free
		// (if he already owns it, we can't know that)
		// if it's not an enemy, we can always know if he owns the village
		bool village = resources::game_map->is_village(*i) && ( !unit_team.owns_village(*i)
				|| (viewing_team.is_enemy(u.side()) && viewing_team.fogged(*i)) );

		// NOTE if there is a waypoint on a not owned village, then we will always stop there
		if (last_step || zoc || move_cost > movement || (pass_here && village)) {
			// check if we stop an a village and so maybe capture it
			// if it's an enemy unit and a fogged village, we assume a capture
			// (if he already owns it, we can't know that)
			// if it's not an enemy, we can always know if he owns the village
			bool capture = resources::game_map->is_village(*i) && ( !unit_team.owns_village(*i)
				 || (viewing_team.is_enemy(u.side()) && viewing_team.fogged(*i)) );

			++turns;

			bool invisible = u.invisible(*i,false);

			res.marks[*i] = marked_route::mark(turns, pass_here, zoc, capture, invisible);

			if (last_step) break; // finished and we used dummy move_cost

			movement = u.total_movement();
			if(move_cost > movement) {
				return res; //we can't reach destination
			}
		} else if (pass_here) {
			bool invisible = u.invisible(*i,false);
			res.marks[*i] = marked_route::mark(0, pass_here, zoc, false, invisible);
		}

		unit_map::iterator itor = units.find(*(i + 1));
		if (!itor.valid() || !itor->cancel_zoc()) {
			zoc = enemy_zoc((*resources::teams), *(i + 1), viewing_team,u.side()) && !u.get_ability_bool("skirmisher", *(i+1));
		} else {
			zoc = false;
		}

		if (zoc) {
			movement = 0;
		} else {
			movement -= move_cost;
		}
	}

	return res;
}

pathfind::shortest_path_calculator::shortest_path_calculator(unit const &u, team const &t,
		unit_map const &units, std::vector<team> const &teams, gamemap const &map,
		bool ignore_unit, bool ignore_defense, bool see_all)
	: unit_(u), viewing_team_(t), units_(units), teams_(teams), map_(map),
	  movement_left_(unit_.movement_left()),
	  total_movement_(unit_.total_movement()),
	  ignore_unit_(ignore_unit), ignore_defense_(ignore_defense),
	  see_all_(see_all),
	  expediting_city_cookie_(NULL)
{
	const map_location& loc = u.get_location();
	if (units.expediting()) {
		expediting_city_cookie_ = units.expediting_city_node();
	} else if (units.city_from_loc(loc)) {
		expediting_city_cookie_ = units.get_cookie(loc);
	}
}

namespace pathfind {

map_location last_location;
bool is_wall;

}
/*
int pathfind::location_cost(const unit_map& units, const unit& u, bool ignore_wall)
{
	unit_map::node* last_node = reinterpret_cast<unit_map::node*>(units.get_cookie(pathfind::last_location, false));
	if (!last_node) {
		last_node = reinterpret_cast<unit_map::node*>(units.get_cookie(pathfind::last_location));
	}
	bool slowed = u.get_state(unit::STATE_SLOWED);
	const unit_type* ut = u.type();
	if (u.packed()) {
		ut = u.packee_type();
	}
	if (ignore_wall) {
		return slowed? 2: 1;
	} else if (!ut->land_wall()) {
		return 2 * u.total_movement();
	} else if (last_node && last_node->second->walk_wall()) {
		// keep/wall ---> wall, cost: 1
		return slowed? 2: 1;
	} else {
		return slowed? 8: 4;
	}
}
*/
int pathfind::location_cost(const unit_map& units, const unit& u, bool ignore_wall)
{
	unit_map::node* last_node = reinterpret_cast<unit_map::node*>(units.get_cookie(pathfind::last_location, false));
	if (!last_node) {
		last_node = reinterpret_cast<unit_map::node*>(units.get_cookie(pathfind::last_location));
	}
	const unit_type* ut = u.type();
	if (u.packed()) {
		ut = u.packee_type();
	}
	if (ignore_wall) {
		return 1;
	} else if (!ut->land_wall()) {
		return 2 * u.total_movement();
	} else if (last_node && last_node->second->walk_wall()) {
		// keep/wall ---> wall, cost: 1
		return 1;
	} else {
		return 4;
	}
}

double pathfind::shortest_path_calculator::cost(const map_location& loc, const double so_far) const
{
	assert(map_.on_board(loc));

	pathfind::is_wall = false;

	if (total_movement_ == 0) {
		// to unit that total_movement == 0, all cost is getNoPathValue().
		return getNoPathValue();
	}
	if (expediting_city_cookie_ && units_.get_cookie(loc) == expediting_city_cookie_) {
		// move cost in self-city grid is 0.
		return 0;
	}
	// loc is shrouded, consider it impassable
	// NOTE: This is why AI must avoid to use shroud
	if (!see_all_ && viewing_team_.shrouded(loc))
		return getNoPathValue();

	bool ignore_city = false;
	if (so_far > 1.5 * total_movement_) {
		ignore_city = true;
	}

	const t_translation::t_terrain terrain = map_[loc];
	int terrain_cost;

	// cost of wall
	unit_map::node* curr_node = reinterpret_cast<unit_map::node*>(units_.get_cookie(loc, false));
	
	if (so_far == 0.8888) {
		// when so_far = 0.8888, it indicates judging dst.
		terrain_cost = 1;

	} else if (curr_node && curr_node->second->wall()) {
		pathfind::is_wall = true;
		terrain_cost = location_cost(units_, unit_, ignore_city);

	} else {
		terrain_cost = unit_.movement_cost(terrain);
	}

	// Pathfinding heuristic: the cost must be at least 1
	VALIDATE(terrain_cost >= 1, _("Terrain with a movement cost less than 1 encountered."));

	// total MP is not enough to move on this terrain: impassable
	if (total_movement_ < terrain_cost)
		return getNoPathValue();

	int other_unit_subcost = 0;
	const unit* other_unit = NULL;
	if (!ignore_unit_) {
		other_unit = get_visible_unit(loc, viewing_team_, see_all_);

		// We can't traverse visible enemy and we also prefer empty hexes
		// (less blocking in multi-turn moves and better when exploring fog,
		// because we can't stop on a friend)

		if (other_unit) {
			if (teams_[unit_.side() - 1].is_enemy(other_unit->side())) {
				if (!other_unit->is_city()) {
					return getUnitHoldValue(); // getNoPathValue();
				} else if (!ignore_city) {
					return 2 * total_movement_;
				}
			} else {
				// This value will be used with the defense_subcost (see below)
				// The 1 here means: consider occupied hex as a -1% defense
				// (less important than 10% defense because friends may move)
				other_unit_subcost = 1;
			}
		}
	}

	// Compute how many movement points are left in the game turn
	// needed to reach the previous hex.
	// total_movement_ is not zero, thanks to the pathfinding heuristic
	int remaining_movement = movement_left_ - static_cast<int>(so_far);
	if (remaining_movement <= 0)
		remaining_movement = total_movement_ - (-remaining_movement) % total_movement_;

	// this will sum all different costs of this move
	int move_cost = 0;

	// Suppose that we have only 2 remaining MP and want to move onto a hex
	// costing 3 MP. We don't have enough MP now, so we must end our turn here,
	// thus spend our remaining MP by waiting (next turn, with full MP, we will
	// be able to move on that hex)
/*
	if (remaining_movement < terrain_cost) {
		move_cost += remaining_movement;
		remaining_movement = total_movement_; // we consider having full MP now
	}
*/

	// check ZoC
	if (!ignore_unit_ && (!other_unit || !other_unit->cancel_zoc()) && remaining_movement > terrain_cost
	    && !unit_.get_ability_bool("skirmisher", loc) && enemy_zoc(teams_, loc, viewing_team_, unit_.side(), see_all_, ignore_city)) {
		// entering ZoC cost all remaining MP
		move_cost += remaining_movement;
	} else {
		// empty hex, pay only the terrain cost
		move_cost += terrain_cost;
	}

	// We will add a tiny cost based on terrain defense, so the pathfinding
	// will prefer good terrains between 2 with the same MP cost
	// Keep in mind that defense_modifier is inverted (= 100 - defense%)
	const int defense_subcost = ignore_defense_ ? 0 : unit_.defense_modifier(terrain);

	// We divide subcosts by 100 * 100, because defense is 100-based and
	// we don't want any impact on move cost for less then 100-steps path
	// (even ~200 since mean defense is around ~50%)
	return move_cost + (defense_subcost + other_unit_subcost) / 10000.0;
}

pathfind::emergency_path_calculator::emergency_path_calculator(const unit& u, const gamemap& map)
	: unit_(u), map_(map)
{}

double pathfind::emergency_path_calculator::cost(const map_location& loc, const double) const
{
	assert(map_.on_board(loc));

	return unit_.movement_cost(map_[loc]);
}

pathfind::dummy_path_calculator::dummy_path_calculator(const unit&, const gamemap&)
{}

double pathfind::dummy_path_calculator::cost(const map_location&, const double) const
{
	return 1.0;
}
