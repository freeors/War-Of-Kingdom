/* $Id: configuration.cpp 37830 2009-08-16 11:29:44Z crab $ */
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
 * Managing the AI configuration
 * @file ai/configuration.cpp
 */

#include "configuration.hpp"

#include "../filesystem.hpp"
#include "../foreach.hpp"
#include "../log.hpp"
#include "../serialization/parser.hpp"
#include "../serialization/preprocessor.hpp"
#include "../team.hpp"
#include <boost/lexical_cast.hpp>
#include <vector>

namespace ai {

static lg::log_domain log_ai_configuration("ai/config");
#define DBG_AI_CONFIGURATION LOG_STREAM(debug, log_ai_configuration)
#define LOG_AI_CONFIGURATION LOG_STREAM(info, log_ai_configuration)
#define WRN_AI_CONFIGURATION LOG_STREAM(warn, log_ai_configuration)
#define ERR_AI_CONFIGURATION LOG_STREAM(err, log_ai_configuration)

void configuration::init(const config &game_config)
{
	ai_configurations_.clear();

	const config &ais = game_config.child("ais");
	default_config_ = ais.child("default_config");
	if (!default_config_) {
		ERR_AI_CONFIGURATION << "Missing AI [default_config]. Therefore, default_config_ set to empty." << std::endl;
		default_config_ = config();
	}
}

std::vector<description*> configuration::get_available_ais(){
	std::vector<description*> ais_list;
	for(description_map::iterator desc = ai_configurations_.begin(); desc!=ai_configurations_.end(); ++desc) {
		ais_list.push_back(&desc->second);	
		DBG_AI_CONFIGURATION << "has ai with config: "<< std::endl << desc->second.cfg<< std::endl;
	}
	return ais_list;
}

const config& configuration::get_ai_config_for(const std::string &id)
{
	description_map::iterator cfg_it = ai_configurations_.find(id);
	if (cfg_it==ai_configurations_.end()){
		return default_config_;
	}
	return cfg_it->second.cfg;
}


configuration::description_map configuration::ai_configurations_ = configuration::description_map();
config configuration::default_config_ = config();

bool configuration::get_side_config_from_file(const std::string& file, config& cfg ){
	try {
		scoped_istream stream = preprocess_file(get_wml_location(file));
		read(cfg, *stream);
		LOG_AI_CONFIGURATION << "Reading AI configuration from file '" << file  << "'" << std::endl;
	} catch(config::error &) {
		ERR_AI_CONFIGURATION << "Error while reading AI configuration from file '" << file  << "'" << std::endl;
		return false;
	}
	LOG_AI_CONFIGURATION << "Successfully read AI configuration from file '" << file  << "'" << std::endl;
	return cfg;//in boolean context
}

const config& configuration::get_default_ai_parameters()
{
	return default_config_;
}

bool configuration::parse_side_config(const config& original_cfg, config &cfg )
{
	//leave only the [ai] children
	cfg = config();
	foreach (const config &aiparam, original_cfg.child_range("ai")) {
		// decide modify wesnoth's ai code. will use attribute
		cfg.merge_attributes(aiparam);
	}
	return cfg;
}

} //end of namespace ai
