/* $Id: manager.cpp 37829 2009-08-16 10:29:28Z crab $ */
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
 * Managing the AI lifecycle and interface for the rest of Wesnoth
 * @file ai/manager.cpp
 */

#include "default/ai.hpp"
#include "configuration.hpp"
// #include "contexts.hpp"
#include "manager.hpp"
#include "../game_events.hpp"
#include "../game_preferences.hpp"
#include "../log.hpp"
#include "../replay.hpp"
#include "../serialization/string_utils.hpp"
#include "../statistics.hpp"

#include <map>
#include <stack>
#include <vector>

#undef assert	// boost中也定义了assert
#define assert(exp)


namespace ai {

static lg::log_domain log_ai_manager("ai/manager");
#define DBG_AI_MANAGER LOG_STREAM(debug, log_ai_manager)
#define LOG_AI_MANAGER LOG_STREAM(info, log_ai_manager)
#define ERR_AI_MANAGER LOG_STREAM(err, log_ai_manager)

holder::holder( side_number side, const config &cfg )
	: ai_(), side_(side), cfg_(cfg)
{
	DBG_AI_MANAGER << describe_ai() << "Preparing new AI holder" << std::endl;
}


void holder::init( side_number side )
{
	if (!this->ai_){
		ai_ = boost::shared_ptr<ai_default>(new ai_default(side, cfg_));
		ai_->on_create();
	}
	if (!this->ai_) {
		ERR_AI_MANAGER << describe_ai()<<"AI lazy initialization error!" << std::endl;
	}

}

holder::~holder()
{
	if (this->ai_) {
		LOG_AI_MANAGER << describe_ai() << "Managed AI will be deleted" << std::endl;
	}
}


ai_interface& holder::get_ai_ref()
{
	if (!this->ai_) {
		this->init(this->side_);
	}
	assert(this->ai_);

	return *this->ai_;
}


void holder::modify_ai_config_old( const config::const_child_itors &ai_parameters)
{
	// only handle aspects
	// transform ai_parameters to new-style config
}

config holder::to_config() const
{
	if (!this->ai_) {
		return cfg_;
	} else {
		config cfg = ai_->to_config();
		return cfg;
	}
}



const std::string holder::describe_ai()
{
	std::string sidestr = lexical_cast<std::string>(this->side_);

	if (this->ai_!=NULL) {
		return this->ai_->describe_self()+std::string(" for side ")+sidestr+std::string(" : ");
	} else {
		return std::string("not initialized ai with id=[")+cfg_["id"]+std::string("] for side ")+sidestr+std::string(" : ");
	}
}


const std::string holder::get_ai_identifier() const
{
	return cfg_["id"];
}

// =======================================================================
// AI COMMAND HISTORY ITEM
// =======================================================================

command_history_item::command_history_item()
	: number_(0), command_()
{

}


command_history_item::command_history_item( side_number number, const std::string& command )
	: number_(number), command_(command)
{

}


command_history_item::~command_history_item()
{

}


int command_history_item::get_number() const
{
	return this->number_;
}


void command_history_item::set_number( int number )
{
	this->number_ = number;
}

const std::string& command_history_item::get_command() const
{
	return this->command_;
}


void command_history_item::set_command( const std::string& command )
{
	this->command_ = command;
}

// =======================================================================
// LIFECYCLE
// =======================================================================


manager::manager()
{
}


manager::~manager()
{
}


manager::AI_map_of_stacks manager::ai_map_;
game_info *manager::ai_info_;
events::generic_event manager::user_interact_("ai_user_interact");
events::generic_event manager::sync_network_("ai_sync_network");
events::generic_event manager::gamestate_changed_("ai_gamestate_changed");
events::generic_event manager::turn_started_("ai_turn_started");
events::generic_event manager::recruit_list_changed_("ai_recruit_list_changed");
events::generic_event manager::map_changed_("ai_map_changed");
int manager::last_interact_ = 0;
int manager::num_interact_ = 0;


void manager::set_ai_info(const game_info& i)
{
	if (ai_info_!=NULL){
		clear_ai_info();
	}
	ai_info_ = new game_info(i);
	// registry::init();
}


void manager::clear_ai_info(){
	if (ai_info_ != NULL){
		delete ai_info_;
		ai_info_ = NULL;
	}
}


void manager::add_observer( events::observer* event_observer){
	user_interact_.attach_handler(event_observer);
	sync_network_.attach_handler(event_observer);
	turn_started_.attach_handler(event_observer);
	gamestate_changed_.attach_handler(event_observer);
}


void manager::remove_observer(events::observer* event_observer){
	user_interact_.detach_handler(event_observer);
	sync_network_.detach_handler(event_observer);
	turn_started_.detach_handler(event_observer);
	gamestate_changed_.detach_handler(event_observer);
}


void manager::add_gamestate_observer( events::observer* event_observer){
	gamestate_changed_.attach_handler(event_observer);
	turn_started_.attach_handler(event_observer);
}


void manager::remove_gamestate_observer(events::observer* event_observer){
	gamestate_changed_.detach_handler(event_observer);
	turn_started_.detach_handler(event_observer);
}


void manager::add_user_interact_observer( events::observer* event_observer )
{
	user_interact_.attach_handler(event_observer);
}


void manager::add_sync_network_observer( events::observer* event_observer )
{
	sync_network_.attach_handler(event_observer);
}


void manager::add_turn_started_observer( events::observer* event_observer )
{
	turn_started_.attach_handler(event_observer);
}


void manager::remove_user_interact_observer( events::observer* event_observer )
{
	user_interact_.detach_handler(event_observer);
}


void manager::delete_sync_network_observer( events::observer* event_observer )
{
	sync_network_.detach_handler(event_observer);
}


void manager::remove_turn_started_observer( events::observer* event_observer )
{
	turn_started_.detach_handler(event_observer);
}

void manager::raise_user_interact() {
        const int interact_time = 30;
        const int time_since_interact = SDL_GetTicks() - last_interact_;
        if(time_since_interact < interact_time) {
                return;
        }

	++num_interact_;
        user_interact_.notify_observers();

        last_interact_ = SDL_GetTicks();

}

void manager::raise_sync_network() {
	sync_network_.notify_observers();
}


void manager::raise_gamestate_changed() {
	gamestate_changed_.notify_observers();
}


void manager::raise_turn_started() {
	turn_started_.notify_observers();
}

void manager::raise_recruit_list_changed() {
	recruit_list_changed_.notify_observers();
}

void manager::raise_map_changed() {
	map_changed_.notify_observers();
}

// =======================================================================
// ADD, CREATE AIs, OR LIST AI TYPES
// =======================================================================

bool manager::add_ai_for_side_from_config( side_number side, const config& cfg, bool replace )
{
	config parsed_cfg;

	if (replace) {
		remove_ai_for_side(side);
	}

	holder new_holder(side, parsed_cfg);
	std::stack<holder>& ai_stack_for_specific_side = get_or_create_ai_stack_for_side(side);
	ai_stack_for_specific_side.push(new_holder);
	return true;
}

// =======================================================================
// REMOVE
// =======================================================================

void manager::remove_ai_for_side( side_number side )
{
	std::stack<holder>& ai_stack_for_specific_side = get_or_create_ai_stack_for_side(side);
	if (!ai_stack_for_specific_side.empty()){
		ai_stack_for_specific_side.pop();
	}
}


void manager::remove_all_ais_for_side( side_number side )
{
	std::stack<holder>& ai_stack_for_specific_side = get_or_create_ai_stack_for_side(side);

	//clear the stack. std::stack doesn't have a '.clear()' method to do it
	while (!ai_stack_for_specific_side.empty()){
			ai_stack_for_specific_side.pop();
	}
}


void manager::clear_ais()
{
	ai_map_.clear();
	ai::ai_default::clear_stats_cache();
}

// =======================================================================
// Work with active AI parameters
// =======================================================================

void manager::modify_active_ai_config_old_for_side ( side_number side, const config::const_child_itors &ai_parameters )
{
	get_active_ai_holder_for_side(side).modify_ai_config_old(ai_parameters);
}

std::string manager::get_active_ai_identifier_for_side( side_number side )
{
	return get_active_ai_holder_for_side(side).get_ai_identifier();
}


config manager::to_config( side_number side )
{
	return get_active_ai_holder_for_side(side).to_config();
}


game_info& manager::get_active_ai_info_for_side( side_number /*side*/ )
{
	return *ai_info_;
}

ai_plan& manager::get_active_ai_plan_for_side(side_number side, bool action)
{
	ai_interface& ai_obj = get_active_ai_for_side(side);
	return ai_obj.plan(action);
}

game_info& manager::get_ai_info()
{
	return *ai_info_;
}


// =======================================================================
// PROXY
// =======================================================================

void manager::play_turn(side_number side)
{
	last_interact_ = 0;
	num_interact_ = 0;
	const int turn_start_time = SDL_GetTicks();
	ai_interface& ai_obj = get_active_ai_for_side(side);
	raise_turn_started();
	ai_obj.new_turn();
	ai_obj.play_turn();
	const int turn_end_time= SDL_GetTicks();
	DBG_AI_MANAGER << "side " << side << ": number of user interactions: "<<num_interact_<<std::endl;
	DBG_AI_MANAGER << "side " << side << ": total turn time: "<<turn_end_time - turn_start_time << " ms "<< std::endl;
}


// =======================================================================
// PRIVATE
// =======================================================================
// =======================================================================
// AI STACKS
// =======================================================================
std::stack<holder>& manager::get_or_create_ai_stack_for_side( side_number side )
{
	AI_map_of_stacks::iterator iter = ai_map_.find(side);
	if (iter!=ai_map_.end()){
		return iter->second;
	}
	return ai_map_.insert(std::make_pair(side, std::stack<holder>())).first->second;
}

// =======================================================================
// AI HOLDERS
// =======================================================================
holder& manager::get_active_ai_holder_for_side( side_number side )
{
	std::stack<holder>& ai_stack_for_specific_side = get_or_create_ai_stack_for_side(side);

	if (!ai_stack_for_specific_side.empty()){
		return ai_stack_for_specific_side.top();
	} else {
		config cfg = config();
		holder new_holder(side, cfg);
		ai_stack_for_specific_side.push(new_holder);
		return ai_stack_for_specific_side.top();
	}
}

// =======================================================================
// AI POINTERS
// =======================================================================

ai_interface& manager::get_active_ai_for_side( side_number side )
{
	return get_active_ai_holder_for_side(side).get_ai_ref();
}


// =======================================================================
// MISC
// =======================================================================

} //end of namespace ai
