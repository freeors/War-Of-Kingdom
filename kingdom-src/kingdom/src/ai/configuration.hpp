/* $Id: configuration.hpp 37709 2009-08-13 14:27:40Z crab $ */
/*
   Copyright (C) 2009 by Yurii Chernyi <terraninfo@terraninfo.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 * Managing the AIs configuration - headers
 * @file ai/configuration.hpp
 * */

#ifndef AI_CONFIGURATION_HPP_INCLUDED
#define AI_CONFIGURATION_HPP_INCLUDED

#include "../global.hpp"

#include "../config.hpp"


#include <map>
#include <vector>

namespace ai {

/**
 * AI parameters. class to deal with AI parameters. It is an implementation detail.
 * @todo 1.7 AI parameter/AI memory/AI effective parameter system must be reworked
 * We need implementation which will allow easy access to all the parameters
 * which match any of the pre-defined set of filters
 * such as 'select from ai_parameters where time_of_day=first watch'
 * or 'select from ai_parameters where active(current_game_state)=true'
 * it should be noted that there may be several variables with a same name but
 * different filters. the proposed rules, in general, are:
 * 1) scenario_parameter_in_SIDE_section > scenario_parameter_in_AI_section > default_value
 * then, if (1) is equal:
 * 2) use scenario-creator supplied priority ( 'not set' = 0)
 * then, if (2) is equal:
 * 3) more restricted parameter > less restricted parameter
 * then, if (3) is equal:
 * use any and loudly complain.
 */

struct description {
public:
	std::string text;
	std::string id;
	config cfg;
};

class configuration {
public:

	/**
	 * Init the parameters of ai configuration parser
	 * @param game_config_ game config
	 */
	static void init(const config &game_config);


	/**
	 * get default AI parameters
	 * @return default AI parameters
	 */
	static const config& get_default_ai_parameters();


	/**
	 * Return the config for a specified ai
	 */
	static const config& get_ai_config_for(const std::string &id);


	/**
	 * Returns a list of available AIs.
	 * @return the list of available AIs.
	 */
	static std::vector<description*> get_available_ais();


	/**
	 * get side config from file
	 * @param file the file name to open. follows usual WML convention.
	 * @param[out] cfg the config to be written from file.
	 * @return was all ok?
	 * @retval true success
	 * @retval false failure
	 */
	static bool get_side_config_from_file( const std::string& file, config& cfg );



	/**
	 * @param[in] cfg the config to be read
	 * @param[out] parsed_cfg parsed config
	 * @return was all ok?
	 * @retval true success
	 * @retval false failure
	 */
	static bool parse_side_config(const config& cfg, config &parsed_cfg);


private:
	typedef std::map<std::string, description> description_map;
	static description_map ai_configurations_;
	static config default_config_;

};

} //end of namespace ai
#endif
