/* $Id: interface.hpp 37696 2009-08-12 11:19:17Z crab $ */
/*
   Copyright (C) 2003 - 2009 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 * @file ai/interface.hpp
 * Interface to the AI.
 */

#ifndef AI_INTERFACE_HPP_INCLUDED
#define AI_INTERFACE_HPP_INCLUDED

#include "../global.hpp"

#include "game_info.hpp"

class ai_plan;

namespace ai {

class ai_interface : savegame::savegame_config {
public:
	/**
	 * The constructor.
	 */
	ai_interface()
		: savegame_config()
	{
	}


	virtual ~ai_interface() {}


	/**
	 * Function that is called when the AI must play its turn.
	 * Derived classes should implement their AI algorithm in this function.
	 */
	virtual void play_turn() = 0;

	/**
	 * Function called when a a new turn is played
	 */
	virtual void new_turn() = 0;

	/**
	 * Function called after the new ai is created
	 *
	 */
	virtual void on_create() {
	}

    /** Evaluate */
    virtual std::string evaluate(const std::string& /*str*/)
			{ return "evaluate command not implemented by this AI"; }

	/** Describe self*/
	virtual std::string describe_self() const;

	virtual ai_plan& plan(bool action) = 0;

	/** serialize to config **/
	virtual config to_config() const = 0;
};

} //end of namespace ai

#endif
