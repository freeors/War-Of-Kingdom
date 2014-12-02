/* $Id: callable_objects.cpp 46186 2010-09-01 21:12:38Z silene $ */
/*
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "callable_objects.hpp"

template <typename T, typename K>
variant convert_map( const std::map<T, K>& input_map ) {
	std::map<variant,variant> tmp;

	for(typename std::map< T, K>::const_iterator i = input_map.begin(); i != input_map.end(); ++i) {
			tmp[ variant(i->first) ] = variant( i->second );
	}

	return variant( &tmp );
}


template <typename T>
variant convert_vector( const std::vector<T>& input_vector )
{
	std::vector<variant> tmp;

	for(typename std::vector<T>::const_iterator i = input_vector.begin(); i != input_vector.end(); ++i) {
			tmp.push_back( variant( *i ) );
	}

	return variant( &tmp );
}


variant location_callable::get_value(const std::string& key) const
{
	if(key == "x") {
		return variant(loc_.x+1);
	} else if(key == "y") {
		return variant(loc_.y+1);
	} else {
		return variant();
	}
}

void location_callable::get_inputs(std::vector<game_logic::formula_input>* inputs) const
{
	using game_logic::FORMULA_READ_ONLY;
	inputs->push_back(game_logic::formula_input("x", FORMULA_READ_ONLY));
	inputs->push_back(game_logic::formula_input("y", FORMULA_READ_ONLY));
}

int location_callable::do_compare(const game_logic::formula_callable* callable) const
{
	const location_callable* loc_callable = dynamic_cast<const location_callable*>(callable);
	if(loc_callable == NULL) {
		return formula_callable::do_compare(callable);
	}

	const map_location& other_loc = loc_callable->loc();
	return loc_.do_compare(other_loc);
}

void location_callable::serialize_to_string(std::string& str) const
{
	std::ostringstream s;
	s << "loc(" << (loc_.x+1) << "," << (loc_.y+1) << ")";
	str += s.str();
}


variant terrain_callable::get_value(const std::string& key) const
{
	if(key == "x") {
		return variant(loc_.x+1);
	} else if(key == "y") {
		return variant(loc_.y+1);
	} else if(key == "loc") {
		return variant(new location_callable(loc_));
	} else if(key == "id") {
		return variant(std::string(t_.id()));
	} else
		return variant();
}

void terrain_callable::get_inputs(std::vector<game_logic::formula_input>* inputs) const
{
	using game_logic::FORMULA_READ_ONLY;
	inputs->push_back(game_logic::formula_input("x", FORMULA_READ_ONLY));
	inputs->push_back(game_logic::formula_input("y", FORMULA_READ_ONLY));
	inputs->push_back(game_logic::formula_input("loc", FORMULA_READ_ONLY));
	inputs->push_back(game_logic::formula_input("id", FORMULA_READ_ONLY));
}

int terrain_callable::do_compare(const formula_callable* callable) const
{
	const terrain_callable* terr_callable = dynamic_cast<const terrain_callable*>(callable);
	if(terr_callable == NULL) {
		return formula_callable::do_compare(callable);
	}

	const map_location& other_loc = terr_callable->loc_;

	return loc_.do_compare(other_loc);
}



