/* $Id: unit_animation.cpp 47268 2010-10-29 14:37:49Z boucman $ */
/*
   Copyright (C) 2006 - 2010 by Jeremy Rosen <jeremy.rosen@enst-bretagne.fr>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
   */

#include "global.hpp"

#include "unit_animation.hpp"

#include "game_display.hpp"
#include "halo.hpp"
#include "map.hpp"
#include "unit.hpp"
#include "variable.hpp"
#include "resources.hpp"
#include "play_controller.hpp"

#include <boost/foreach.hpp>
#include <algorithm>

std::map<int, unit_animation> unit_animation::share_anims;

void unit_animation::fill_share_anims()
{
	share_anims.clear();
	for (int t = area_anim::MIN_UNIT_ANIM; t <= area_anim::MAX_UNIT_ANIM; t ++) {
		if (const animation* anim = area_anim::anim(t)) {
			share_anims.insert(std::make_pair(t, unit_animation(*anim)));
		}
	}
}

const unit_animation* unit_animation::share_anim(int type)
{
	std::map<int, unit_animation>::const_iterator i = share_anims.find(type);
	if (i != share_anims.end()) {
		return &i->second;
	}
	return NULL;
}

struct tag_name_manager {
	tag_name_manager() : names() {
		names.push_back("animation");
		names.push_back("attack_anim");
		names.push_back("death");
		names.push_back("defend");
		names.push_back("extra_anim");
		names.push_back("healed_anim");
		names.push_back("healing_anim");
		names.push_back("build_anim");
		names.push_back("repair_anim");
		names.push_back("idle_anim");
		names.push_back("leading_anim");
		names.push_back("resistance_anim");
		names.push_back("levelin_anim");
		names.push_back("levelout_anim");
		names.push_back("movement_anim");
		names.push_back("poison_anim");
		names.push_back("recruit_anim");
		names.push_back("recruiting_anim");
		names.push_back("standing_anim");
		names.push_back("teleport_anim");
		names.push_back("pre_movement_anim");
		names.push_back("post_movement_anim");
		names.push_back("draw_weapon_anim");
		names.push_back("sheath_weapon_anim");
		names.push_back("victory_anim");
		names.push_back("_transparent"); // Used for WB
	}
	std::vector<std::string> names;
};
namespace {
	tag_name_manager anim_tags;
} //end anonymous namespace

const std::vector<std::string>& unit_animation::all_tag_names() {
	return anim_tags.names;
}

struct animation_branch
{
	animation_branch()
		: attributes()
		, children()
	{
	}

	config attributes;
	std::vector<config::all_children_iterator> children;
	config merge() const
	{
		config result = attributes;
		BOOST_FOREACH (const config::all_children_iterator &i, children)
			result.add_child(i->key, i->cfg);
		return result;
	}
};

typedef std::list<animation_branch> animation_branches;

struct animation_cursor
{
	config::all_children_itors itors;
	animation_branches branches;
	animation_cursor *parent;
	animation_cursor(const config &cfg):
		itors(cfg.all_children_range()), branches(1), parent(NULL)
	{
		branches.back().attributes.merge_attributes(cfg);
	}
	animation_cursor(const config &cfg, animation_cursor *p):
		itors(cfg.all_children_range()), branches(p->branches), parent(p)
	{
		BOOST_FOREACH (animation_branch &ab, branches)
			ab.attributes.merge_attributes(cfg);
	}
};

static void prepare_single_animation(const config &anim_cfg, animation_branches &expanded_anims)
{
	std::list<animation_cursor> anim_cursors;
	anim_cursors.push_back(animation_cursor(anim_cfg));
	while (!anim_cursors.empty())
	{
		animation_cursor &ac = anim_cursors.back();
		if (ac.itors.first == ac.itors.second) {
			if (!ac.parent) break;
			// Merge all the current branches into the parent.
			ac.parent->branches.splice(ac.parent->branches.end(),
				ac.branches, ac.branches.begin(), ac.branches.end());
			anim_cursors.pop_back();
			continue;
		}
		if (ac.itors.first->key != "if")
		{
			// Append current config object to all the branches in scope.
			BOOST_FOREACH (animation_branch &ab, ac.branches) {
				ab.children.push_back(ac.itors.first);
			}
			++ac.itors.first;
			continue;
		}
		int count = 0;
		do {
			/* Copies the current branches to each cursor created
			   for the conditional clauses. Merge the attributes
			   of the clause into them. */
			anim_cursors.push_back(animation_cursor(ac.itors.first->cfg, &ac));
			++ac.itors.first;
			++count;
		} while (ac.itors.first != ac.itors.second && ac.itors.first->key == "else");
		if (count > 1) {
			/* There are some "else" clauses, discard the branches
			   from the current cursor. */
			ac.branches.clear();
		}
	}

	// Create the config object describing each branch.
	assert(anim_cursors.size() == 1);
	animation_cursor &ac = anim_cursors.back();
	expanded_anims.splice(expanded_anims.end(),
		ac.branches, ac.branches.begin(), ac.branches.end());
}

static animation_branches prepare_animation(const config &cfg, const std::string &animation_tag)
{
	animation_branches expanded_animations;
	BOOST_FOREACH (const config &anim, cfg.child_range(animation_tag)) {
		prepare_single_animation(anim, expanded_animations);
	}
	return expanded_animations;
}

unit_animation::unit_animation(int start_time, const unit_frame & frame, const std::string& event, const int variation, const frame_builder& builder)
	: animation(start_time, frame, event, variation, builder)
	, terrain_types_()
	, unit_filter_()
	, secondary_unit_filter_()
	, directions_()
	, secondary_weapon_type_()
	, align_(ALIGN_NONE)
	, feature_(HEROS_NO_FEATURE)
	, frequency_(0)
	, base_score_(variation)
	, event_(utils::split(event))
	, value_()
	, primary_attack_filter_()
	, secondary_attack_filter_()
	, hits_()
	, value2_()
{
}

unit_animation::unit_animation(const animation& anim)
	: animation(anim)
	, terrain_types_()
	, unit_filter_()
	, secondary_unit_filter_()
	, directions_()
	, secondary_weapon_type_()
	, align_(ALIGN_NONE)
	, feature_(HEROS_NO_FEATURE)
	, frequency_(0)
	, base_score_(DEFAULT_ANIM)
	, event_()
	, value_()
	, primary_attack_filter_()
	, secondary_attack_filter_()
	, hits_()
	, value2_()
{
}

unit_animation::unit_animation(const config& cfg, const std::string& frame_string)
	: animation(cfg, frame_string)
	, terrain_types_(t_translation::read_list(cfg["terrain_type"]))
	, unit_filter_()
	, secondary_unit_filter_()
	, directions_()
	, secondary_weapon_type_()
	, feature_(cfg["feature"].to_int(HEROS_NO_FEATURE))
	, align_(cfg["align"].to_int(ALIGN_NONE))
	, frequency_(cfg["frequency"])
	, base_score_(cfg["base_score"])
	, event_()
	, value_()
	, primary_attack_filter_()
	, secondary_attack_filter_()
	, hits_()
	, value2_()
{
	event_ = utils::split(cfg["apply_to"]);

	const std::vector<std::string>& my_directions = utils::split(cfg["direction"]);
	for(std::vector<std::string>::const_iterator i = my_directions.begin(); i != my_directions.end(); ++i) {
		const map_location::DIRECTION d = map_location::parse_direction(*i);
		directions_.push_back(d);
	}

	std::vector<std::string> vstr = utils::split(cfg["secondary_weapon_type"]);
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		secondary_weapon_type_.insert(*it);
	}

	BOOST_FOREACH (const config &filter, cfg.child_range("filter")) {
		unit_filter_.push_back(filter);
	}

	BOOST_FOREACH (const config &filter, cfg.child_range("filter_second")) {
		secondary_unit_filter_.push_back(filter);
	}

	std::vector<std::string> value_str = utils::split(cfg["value"]);
	std::vector<std::string>::iterator value;
	for(value=value_str.begin() ; value != value_str.end() ; ++value) {
		value_.push_back(atoi(value->c_str()));
	}

	std::vector<std::string> hits_str = utils::split(cfg["hits"]);
	std::vector<std::string>::iterator hit;
	for(hit=hits_str.begin() ; hit != hits_str.end() ; ++hit) {
		if(*hit == "yes" || *hit == "hit") {
			hits_.push_back(HIT);
		}
		if(*hit == "no" || *hit == "miss") {
			hits_.push_back(MISS);
		}
		if(*hit == "yes" || *hit == "kill" ) {
			hits_.push_back(KILL);
		}
	}

	vstr = utils::split(cfg["value_second"]);
	for (std::vector<std::string>::const_iterator value2 = vstr.begin() ; value2 != vstr.end() ; ++ value2) {
		value2_.push_back(atoi(value2->c_str()));
	}

	BOOST_FOREACH (const config &filter, cfg.child_range("filter_attack")) {
		primary_attack_filter_.push_back(filter);
	}
	BOOST_FOREACH (const config &filter, cfg.child_range("filter_second_attack")) {
		secondary_attack_filter_.push_back(filter);
	}
}

int unit_animation::matches(const game_display &disp,const map_location& loc,const map_location& second_loc, const unit* my_unit,const std::string & event,const int value,hit_type hit,const attack_type* attack, const std::string& second_weapon_type, int value2) const
{
	int result = base_score_;
	if(!event.empty()&&!event_.empty()) {
		if (std::find(event_.begin(),event_.end(),event)== event_.end()) {
			return MATCH_FAIL;
		} else {
			result ++;
		}
	}
	if(terrain_types_.empty() == false) {
		if(t_translation::terrain_matches(disp.get_map().get_terrain(loc), terrain_types_)) {
			result ++;
		} else {
			return MATCH_FAIL;
		}
	}

	if(value_.empty() == false ) {
		if (std::find(value_.begin(),value_.end(),value)== value_.end()) {
			return MATCH_FAIL;
		} else {
			result ++;
		}
	}
	if (my_unit) {
		if (directions_.empty()== false) {
			if (std::find(directions_.begin(),directions_.end(),my_unit->facing())== directions_.end()) {
				return MATCH_FAIL;
			} else {
				result ++;
			}
		}

		if (align_ != ALIGN_NONE) {
			if (align_ == ALIGN_X || align_ == ALIGN_NON_X) {
				bool ok = my_unit->loc_is_align_with(second_loc, true);
				if ((align_ == ALIGN_X && !ok) || (align_ != ALIGN_X && ok)) {
					return MATCH_FAIL;
				}
			} else {
				return MATCH_FAIL;
			}
			result ++;
		}

		if (feature_ != HEROS_NO_FEATURE) {
			if (!unit_feature_val2(*my_unit, feature_)) {
				return MATCH_FAIL;
			} else {
				result ++;
			}
		}

		std::vector<config>::const_iterator myitor;
		for(myitor = unit_filter_.begin(); myitor != unit_filter_.end(); ++myitor) {
			if (!my_unit->matches_filter(vconfig(*myitor), loc)) return MATCH_FAIL;
			++result;
		}
		if(!secondary_unit_filter_.empty()) {
			const unit_map& units = disp.get_const_units();
			unit_map::const_iterator it;
			for (it = units.begin(); it != units.end(); ++ it) {
				unit* u = dynamic_cast<unit*>(&*it);
				if (u->get_location() == second_loc) {
					std::vector<config>::const_iterator second_itor;
					for(second_itor = secondary_unit_filter_.begin(); second_itor != secondary_unit_filter_.end(); ++second_itor) {
						if (!u->matches_filter(vconfig(*second_itor), second_loc)) return MATCH_FAIL;
						result++;
					}

					break;
				}
			}
			if (it == units.end()) return MATCH_FAIL;
		}

	} else if (!unit_filter_.empty()) return MATCH_FAIL;
	if(frequency_ && !(rand()%frequency_)) return MATCH_FAIL;


	if(hits_.empty() == false ) {
		if (std::find(hits_.begin(),hits_.end(),hit)== hits_.end()) {
			return MATCH_FAIL;
		} else {
			result ++;
		}
	}

	if(value2_.empty() == false ) {
		if (std::find(value2_.begin(),value2_.end(),value2)== value2_.end()) {
			return MATCH_FAIL;
		} else {
			result ++;
		}
	}

	if(!attack) {
		if(!primary_attack_filter_.empty())
			return MATCH_FAIL;
	}
	std::vector<config>::const_iterator myitor;
	for (myitor = primary_attack_filter_.begin(); myitor != primary_attack_filter_.end(); ++myitor) {
		if(!attack->matches_filter(*myitor)) return MATCH_FAIL;
		result++;
	}

	if (!secondary_weapon_type_.empty()) {
		if (secondary_weapon_type_.find(second_weapon_type) == secondary_weapon_type_.end()) {
			return MATCH_FAIL;
		} else {
			result++;
		}
	}

	return result;
}


void unit_animation::fill_initial_animations(const std::string& default_image, std::vector<unit_animation> & animations, const config & cfg)
{
	std::vector<unit_animation>  animation_base;
	std::vector<unit_animation>::const_iterator itor;
	add_anims(animations,cfg);
	for(itor = animations.begin(); itor != animations.end() ; ++itor) {
		if (std::find(itor->event_.begin(),itor->event_.end(),std::string("default"))!= itor->event_.end()) {
			animation_base.push_back(*itor);
			animation_base.back().base_score_ += unit_animation::DEFAULT_ANIM;
			animation_base.back().event_.clear();
		}
	}


	if( animation_base.empty() )
		animation_base.push_back(unit_animation(0,frame_builder().image(default_image).duration(1),"",unit_animation::DEFAULT_ANIM));

	animations.push_back(unit_animation(0,frame_builder().image(default_image).duration(1),"_disabled_",0));
	animations.push_back(unit_animation(0,frame_builder().image(default_image).duration(300).
					blend("0.0~0.3:100,0.3~0.0:200",display::rgb(255,255,255)),"_disabled_selected_",0));
	for(itor = animation_base.begin() ; itor != animation_base.end() ; ++itor ) {
		//unit_animation tmp_anim = *itor;
		// provide all default anims
		//no event, providing a catch all anim
		//animations.push_back(tmp_anim);

		animations.push_back(*itor);
		animations.back().event_ = utils::split("standing");
		animations.back().play_offscreen_ = false;

		animations.push_back(*itor);
		animations.back().unit_anim_.override(0,animations.back().unit_anim_.get_animation_duration(),"0.9","",0,"","","~GS()");
		animations.back().event_ = utils::split("ghosted");

		animations.push_back(*itor);
		animations.back().unit_anim_.override(0,1,"0.4","",0,"","","~GS()");
		animations.back().event_ = utils::split("disabled_ghosted");

		animations.push_back(*itor);
		animations.back().unit_anim_.override(0,300,"","0.0~0.3:100,0.3~0.0:200",display::rgb(255,255,255));
		animations.back().event_ = utils::split("selected");

		animations.push_back(*itor);
		animations.back().unit_anim_.override(0,600,"0~1:600");
		animations.back().event_ = utils::split("recruited");

		animations.push_back(*itor);
		animations.back().unit_anim_.override(0,600,"","1~0:600",display::rgb(255,255,255));
		animations.back().event_ = utils::split("levelin");

		animations.push_back(*itor);
		animations.back().unit_anim_.override(0,600,"","0~1:600,1",display::rgb(255,255,255));
		animations.back().event_ = utils::split("levelout");

		animations.push_back(*itor);
		animations.back().unit_anim_.override(0,1);
		animations.back().event_ = utils::split("pre_movement");

		animations.push_back(*itor);
		animations.back().unit_anim_.override(0,1);
		animations.back().event_ = utils::split("post_movement");
/*
		animations.push_back(*itor);
		animations.back().unit_anim_.override(0,6800,"","",0,"0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,",lexical_cast<std::string>(display::LAYER_UNIT_MOVE_DEFAULT-display::LAYER_UNIT_FIRST));
		animations.back().event_ = utils::split("movement");

		animations.push_back(*itor);
		animations.back().unit_anim_.override(0,225,"","0.0,0.5:75,0.0:75,0.5:75,0.0",game_display::rgb(255,0,0));
		animations.back().hits_.push_back(HIT);
		animations.back().hits_.push_back(KILL);
		animations.back().event_ = utils::split("defend");
*/
		animations.push_back(*itor);
		animations.back().unit_anim_.override(0,1);
		animations.back().event_ = utils::split("defend");

		animations.push_back(*itor);
		animations.back().unit_anim_.override(-150,300,"","",0,"0~0.6:150,0.6~0:150",lexical_cast<std::string>(display::LAYER_UNIT_MOVE_DEFAULT-display::LAYER_UNIT_FIRST));
		animations.back().event_ = utils::split("attack");
		animations.back().primary_attack_filter_.push_back(config());
		animations.back().primary_attack_filter_.back()["range"] = "melee";

		animations.push_back(*itor);
		animations.back().unit_anim_.override(-150,150);
		animations.back().event_ = utils::split("attack");
		animations.back().primary_attack_filter_.push_back(config());
		animations.back().primary_attack_filter_.back()["range"] = "ranged";
/*
		animations.push_back(*itor);
		animations.back().unit_anim_.override(0,600,"1~0:600");
		animations.back().event_ = utils::split("death");
		animations.back().sub_anims_["_death_sound"] = particular();
		animations.back().sub_anims_["_death_sound"].add_frame(1,frame_builder());
		animations.back().sub_anims_["_death_sound"].add_frame(1,frame_builder().sound(cfg["die_sound"]),true);
*/
		animations.push_back(*itor);
		animations.back().unit_anim_.override(0,1);
		animations.back().event_ = utils::split("victory");

		animations.push_back(*itor);
		animations.back().unit_anim_.override(0,150,"1~0:150");
		animations.back().event_ = utils::split("pre_teleport");

		animations.push_back(*itor);
		animations.back().unit_anim_.override(0,150,"0~1:150,1");
		animations.back().event_ = utils::split("post_teleport");

		animations.push_back(*itor);
		animations.back().unit_anim_.override(0,300,"","0:30,0.5:30,0:30,0.5:30,0:30,0.5:30,0:30,0.5:30,0:30",display::rgb(255,255,255));
		// animations.back().unit_anim_.override(-300,300,"","0:30,0.5:30,0:30,0.5:30,0:30,0.5:30,0:30,0.5:30,0:30",display::rgb(255,255,255));
		animations.back().event_ = utils::split("healed");
		animations.back().sub_anims_["_healed_sound"] = particular();
		animations.back().sub_anims_["_healed_sound"].add_frame(1,frame_builder());
		animations.back().sub_anims_["_healed_sound"].add_frame(1,frame_builder().sound("heal.wav"),true);

		animations.push_back(*itor);
		animations.back().unit_anim_.override(0,300,"","0:30,0.5:30,0:30,0.5:30,0:30,0.5:30,0:30,0.5:30,0:30",display::rgb(0,255,0));
		animations.back().event_ = utils::split("poisoned");
		animations.back().sub_anims_["_poison_sound"] = particular();
		animations.back().sub_anims_["_poison_sound"].add_frame(1,frame_builder());
		animations.back().sub_anims_["_poison_sound"].add_frame(1,frame_builder().sound("poison.ogg"),true);

	}

}

static void add_simple_anim(std::vector<unit_animation> &animations,
	const config &cfg, char const *tag_name, char const *apply_to,
	display::tdrawing_layer layer = display::LAYER_UNIT_DEFAULT,
	bool offscreen = true)
{
	BOOST_FOREACH (const animation_branch &ab, prepare_animation(cfg, tag_name))
	{
		config anim = ab.merge();
		anim["apply_to"] = apply_to;
		if (!offscreen) {
			config::attribute_value &v = anim["offscreen"];
			if (v.empty()) v = false;
		}
		config::attribute_value &v = anim["layer"];
		if (v.empty()) v = layer - display::LAYER_UNIT_FIRST;
		animations.push_back(unit_animation(anim));
	}
}

void unit_animation::add_anims( std::vector<unit_animation> & animations, const config & cfg)
{
	BOOST_FOREACH (const animation_branch &ab, prepare_animation(cfg, "animation")) {
		animations.push_back(unit_animation(ab.merge()));
	}

	int default_layer = display::LAYER_UNIT_DEFAULT - display::LAYER_UNIT_FIRST;
	int move_layer = display::LAYER_UNIT_MOVE_DEFAULT - display::LAYER_UNIT_FIRST;
	int missile_layer = display::LAYER_UNIT_MISSILE_DEFAULT - display::LAYER_UNIT_FIRST;

	add_simple_anim(animations, cfg, "resistance_anim", "resistance");
	add_simple_anim(animations, cfg, "leading_anim", "leading");
	add_simple_anim(animations, cfg, "recruit_anim", "recruited");
	add_simple_anim(animations, cfg, "recruiting_anim", "recruiting");
	add_simple_anim(animations, cfg, "standing_anim", "standing,default", display::LAYER_UNIT_DEFAULT, false);
	add_simple_anim(animations, cfg, "idle_anim", "idling", display::LAYER_UNIT_DEFAULT, false);
	add_simple_anim(animations, cfg, "levelin_anim", "levelin");
	add_simple_anim(animations, cfg, "levelout_anim", "levelout");

	BOOST_FOREACH (const animation_branch &ab, prepare_animation(cfg, "healing_anim"))
	{
		config anim = ab.merge();
		anim["apply_to"] = "healing";
		if (anim["layer"].empty()) anim["layer"] = default_layer;
		anim["value"] = anim["damage"];
		animations.push_back(unit_animation(anim));
	}

	BOOST_FOREACH (const animation_branch &ab, prepare_animation(cfg, "healed_anim"))
	{
		config anim = ab.merge();
		anim["apply_to"] = "healed";
		if (anim["layer"].empty()) anim["layer"] = default_layer;
		anim["value"] = anim["healing"];
		animations.push_back(unit_animation(anim));
		animations.back().sub_anims_["_healed_sound"] = particular();
		animations.back().sub_anims_["_healed_sound"].add_frame(1,frame_builder());
		animations.back().sub_anims_["_healed_sound"].add_frame(1,frame_builder().sound("heal.wav"),true);
	}

	BOOST_FOREACH (const animation_branch &ab, prepare_animation(cfg, "build_anim"))
	{
		config anim = ab.merge();
		anim["apply_to"] = "build";
		if (anim["layer"].empty()) anim["layer"] = default_layer;
		animations.push_back(unit_animation(anim));
	}

	BOOST_FOREACH (const animation_branch &ab, prepare_animation(cfg, "repair_anim"))
	{
		config anim = ab.merge();
		anim["apply_to"] = "repair";
		if (anim["layer"].empty()) anim["layer"] = default_layer;
		animations.push_back(unit_animation(anim));
	}

	BOOST_FOREACH (const animation_branch &ab, prepare_animation(cfg, "poison_anim"))
	{
		config anim = ab.merge();
		anim["apply_to"] ="poisoned";
		if (anim["layer"].empty()) anim["layer"] = default_layer;
		anim["value"] = anim["damage"];
		animations.push_back(unit_animation(anim));
		animations.back().sub_anims_["_poison_sound"] = particular();
		animations.back().sub_anims_["_poison_sound"].add_frame(1,frame_builder());
		animations.back().sub_anims_["_poison_sound"].add_frame(1,frame_builder().sound("poison.ogg"),true);
	}

	add_simple_anim(animations, cfg, "pre_movement_anim", "pre_movement", display::LAYER_UNIT_MOVE_DEFAULT);

	BOOST_FOREACH (const animation_branch &ab, prepare_animation(cfg, "movement_anim"))
	{
		config anim = ab.merge();
/*
		if (anim["offset_x"].empty()) {
			anim["offset_x"] = "0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,";
		}
		if (anim["offset_y"].empty()) {
			anim["offset_y"] = "0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,0~1:200,";
		}
*/
		anim["apply_to"] = "movement";
		if (anim["layer"].empty()) anim["layer"] = move_layer;
		animations.push_back(unit_animation(anim));
	}

	add_simple_anim(animations, cfg, "post_movement_anim", "post_movement", display::LAYER_UNIT_MOVE_DEFAULT);

	BOOST_FOREACH (const animation_branch &ab, prepare_animation(cfg, "defend"))
	{
		config anim = ab.merge();
		anim["apply_to"] = "defend";
		if (anim["layer"].empty()) anim["layer"] = default_layer;
		if (!anim["damage"].empty() && anim["value"].empty()) {
			anim["value"] = anim["damage"];
		}
		if (anim["hits"].empty())
		{
			anim["hits"] = false;
			animations.push_back(unit_animation(anim));
			anim["hits"] = true;
			animations.push_back(unit_animation(anim));
			animations.back().add_frame(225,frame_builder()
					.image(animations.back().get_last_frame().parameters(0).image)
					.duration(225)
					.blend("0.0,0.5:75,0.0:75,0.5:75,0.0",game_display::rgb(255,0,0)));
		}
		else
		{
			std::vector<std::string> v = utils::split(anim["hits"]);
			BOOST_FOREACH (const std::string &hit_type, v)
			{
				config tmp = anim;
				tmp["hits"] = hit_type;
				animations.push_back(unit_animation(tmp));
				if(hit_type == "yes" || hit_type == "hit" || hit_type=="kill") {
					animations.back().add_frame(225,frame_builder()
							.image(animations.back().get_last_frame().parameters(0).image)
							.duration(225)
							.blend("0.0,0.5:75,0.0:75,0.5:75,0.0",game_display::rgb(255,0,0)));
				}
			}
		}
	}

	add_simple_anim(animations, cfg, "draw_weapon_anim", "draw_wepaon", display::LAYER_UNIT_MOVE_DEFAULT);
	add_simple_anim(animations, cfg, "sheath_weapon_anim", "sheath_wepaon", display::LAYER_UNIT_MOVE_DEFAULT);

	BOOST_FOREACH (const animation_branch &ab, prepare_animation(cfg, "attack_anim"))
	{
		config anim = ab.merge();
		anim["apply_to"] = "attack";
		if (anim["layer"].empty()) anim["layer"] = move_layer;
		config::const_child_itors missile_fs = anim.child_range("missile_frame");
		if (anim["offset_x"].empty() && missile_fs.first == missile_fs.second) {
			anim["offset_x"] ="0~0.6,0.6~0";
		}
		if (anim["offset_y"].empty() && missile_fs.first == missile_fs.second) {
			anim["offset_y"] ="0~0.6,0.6~0";
		}
		if (missile_fs.first != missile_fs.second) {
			if (anim["missile_offset_x"].empty()) anim["missile_offset_x"] = "0~0.8";
			if (anim["missile_offset_y"].empty()) anim["missile_offset_y"] = "0~0.8";
			if (anim["missile_layer"].empty()) anim["missile_layer"] = missile_layer;
			config tmp;
			tmp["duration"] = 1;
			anim.add_child("missile_frame", tmp);
			anim.add_child_at("missile_frame", tmp, 0);
		}

		animations.push_back(unit_animation(anim));
	}

	BOOST_FOREACH (const animation_branch &ab, prepare_animation(cfg, "death"))
	{
		config anim = ab.merge();
		anim["apply_to"] = "death";
		if (anim["layer"].empty()) anim["layer"] = default_layer;
		animations.push_back(unit_animation(anim));
/*
		const std::string image_loc = animations.back().get_last_frame().parameters(0).image;
		animations.back().add_frame(600,frame_builder().image(image_loc).duration(600).highlight("1~0:600"));
		if (!cfg["die_sound"].empty()) {
			animations.back().sub_anims_["_death_sound"] = particular();
			animations.back().sub_anims_["_death_sound"].add_frame(1,frame_builder());
			animations.back().sub_anims_["_death_sound"].add_frame(1,frame_builder().sound(cfg["die_sound"]),true);
		}
*/
	}

	add_simple_anim(animations, cfg, "victory_anim", "victory");

	BOOST_FOREACH (const animation_branch &ab, prepare_animation(cfg, "extra_anim"))
	{
		config anim = ab.merge();
		anim["apply_to"] = anim["flag"];
		if (anim["layer"].empty()) anim["layer"] = default_layer;
		animations.push_back(unit_animation(anim));
	}

	BOOST_FOREACH (const animation_branch &ab, prepare_animation(cfg, "teleport_anim"))
	{
		config anim = ab.merge();
		if (anim["layer"].empty()) anim["layer"] = default_layer;
		anim["apply_to"] = "pre_teleport";
		animations.push_back(unit_animation(anim));
		animations.back().unit_anim_.set_end_time(0);
		anim["apply_to"] ="post_teleport";
		animations.push_back(unit_animation(anim));
		animations.back().unit_anim_.remove_frames_until(0);
	}
}

const std::string unit_animation::event0() const
{
	if (event_.empty()) {
		return null_str;
	}
	return event_.front();
}

void unit_animator::add_animation(unit* animated_unit
		, const std::string& event
		, const map_location &src
		, const map_location &dst
		, const int value
		, bool with_bars
		, bool cycles
		, const std::string& text
		, const Uint32 text_color
		, const unit_animation::hit_type hit_type
		, const attack_type* attack
		, const std::string& second_attack
		, int value2)
{
	if (!animated_unit) return;

	anim_elem tmp;
	game_display*disp = resources::screen;
	tmp.my_unit = animated_unit;
	tmp.text = text;
	tmp.text_color = text_color;
	tmp.src = src;
	tmp.with_bars= with_bars;
	tmp.cycles = cycles;
	tmp.animation = animated_unit->choose_animation(*disp,src,event,dst,value,hit_type,attack,second_attack,value2);
	if(!tmp.animation) return;



	start_time_ = std::max<int>(start_time_,tmp.animation->get_begin_time());
	animated_units_.push_back(tmp);
}
void unit_animator::add_animation(unit* animated_unit
		, const unit_animation* anim
		, const map_location &src
		, bool with_bars
		, bool cycles
		, const std::string& text
		, const Uint32 text_color)
{
	if (!animated_unit) return;

	anim_elem tmp;
	tmp.my_unit = animated_unit;
	tmp.text = text;
	tmp.text_color = text_color;
	tmp.src = src;
	tmp.with_bars= with_bars;
	tmp.cycles = cycles;
	tmp.animation = anim;
	if(!tmp.animation) return;



	start_time_ = std::max<int>(start_time_,tmp.animation->get_begin_time());
	animated_units_.push_back(tmp);
}
void unit_animator::replace_anim_if_invalid(unit* animated_unit
		, const std::string& event
		, const map_location &src
		, const map_location & dst
		, const int value
		, bool with_bars
		, bool cycles
		, const std::string& text
		, const Uint32 text_color
		, const unit_animation::hit_type hit_type
		, const attack_type* attack
		, const std::string& second_weapon
		, int value2)
{
	if(!animated_unit) return;
	game_display*disp = resources::screen;

	if(animated_unit->get_animation() &&
			!animated_unit->get_animation()->animation_finished_potential() &&
			animated_unit->get_animation()->matches(*disp,src,dst,animated_unit,event,value,hit_type,attack,second_weapon,value2) >unit_animation::MATCH_FAIL) {
		anim_elem tmp;
		tmp.my_unit = animated_unit;
		tmp.text = text;
		tmp.text_color = text_color;
		tmp.src = src;
		tmp.with_bars= with_bars;
		tmp.cycles = cycles;
		tmp.animation = NULL;
		animated_units_.push_back(tmp);
	}else {
		add_animation(animated_unit,event,src,dst,value,with_bars,cycles,text,text_color,hit_type,attack,second_weapon,value2);
	}
}
void unit_animator::start_animations()
{
	int begin_time = INT_MAX;
	std::vector<anim_elem>::iterator anim;
	
	for (anim = animated_units_.begin(); anim != animated_units_.end(); ++ anim) {
		if (anim->my_unit->get_animation()) {
			if(anim->animation) {
				begin_time = std::min<int>(begin_time,anim->animation->get_begin_time());
			} else  {
				begin_time = std::min<int>(begin_time,anim->my_unit->get_animation()->get_begin_time());
			}
		}
	}

	for (anim = animated_units_.begin(); anim != animated_units_.end();++anim) {
		if (anim->animation) {
			// TODO: start_tick_ of particular is relative to last_update_tick_(reference to animated<T,T_void_value>::start_animation), 
			//       and last_update_tick generated by new_animation_frame().
			//       On the other hand, new_animation_frame() may be delayed, for example display gui2::dialog,
			//       once it occur, last_update_tick will indicate "past" time, result to start_tick_ "too early".
			//       ----Should displayed frame in this particular will not display!
			//       there need call new_animation_frame(), update last_update_tick to current tick.
			new_animation_frame();

			anim->my_unit->start_animation(begin_time, anim->animation,
				anim->with_bars, anim->cycles, anim->text, anim->text_color);
			anim->animation = NULL;
		} else {
			anim->my_unit->get_animation()->update_parameters(anim->src,anim->src.get_direction(anim->my_unit->facing()));
		}

	}
}

bool unit_animator::would_end() const
{
	bool finished = true;
	for(std::vector<anim_elem>::const_iterator anim = animated_units_.begin(); anim != animated_units_.end();++anim) {
		finished &= anim->my_unit->get_animation()->animation_finished_potential();
	}
	return finished;
}
void unit_animator::wait_until(int animation_time) const
{
	game_display*disp = resources::screen;
	double speed = disp->turbo_speed();
	resources::controller->play_slice(false);
	int end_tick = animated_units_[0].my_unit->get_animation()->time_to_tick(animation_time);
	while (SDL_GetTicks() < static_cast<unsigned int>(end_tick)
				- std::min<int>(static_cast<unsigned int>(20/speed),20)) {

		disp->delay(std::max<int>(0,
			std::min<int>(10,
			static_cast<int>((animation_time - get_animation_time()) * speed))));
		resources::controller->play_slice(false);
                end_tick = animated_units_[0].my_unit->get_animation()->time_to_tick(animation_time);
	}
	disp->delay(std::max<int>(0,end_tick - SDL_GetTicks() +5));
	new_animation_frame();
}
void unit_animator::wait_for_end() const
{
	if (game_config::no_delay) return;
	bool finished = false;
	game_display*disp = resources::screen;
	while(!finished) {
		resources::controller->play_slice(false);
		disp->delay(10);
		finished = true;
		for(std::vector<anim_elem>::const_iterator anim = animated_units_.begin(); anim != animated_units_.end();++anim) {
			finished &= anim->my_unit->get_animation()->animation_finished_potential();
		}
	}
}
int unit_animator::get_animation_time() const{
	return animated_units_[0].my_unit->get_animation()->get_animation_time() ;
}

int unit_animator::get_animation_time_potential() const{
	return animated_units_[0].my_unit->get_animation()->get_animation_time_potential() ;
}

int unit_animator::get_end_time() const
{
        int end_time = INT_MIN;
        for(std::vector<anim_elem>::const_iterator anim = animated_units_.begin(); anim != animated_units_.end();++anim) {
	       if(anim->my_unit->get_animation()) {
                end_time = std::max<int>(end_time,anim->my_unit->get_animation()->get_end_time());
	       }
        }
        return end_time;
}
void unit_animator::pause_animation()
{
        for(std::vector<anim_elem>::iterator anim = animated_units_.begin(); anim != animated_units_.end();++anim) {
	       if(anim->my_unit->get_animation()) {
                anim->my_unit->get_animation()->pause_animation();
	       }
        }
}
void unit_animator::restart_animation()
{
        for(std::vector<anim_elem>::iterator anim = animated_units_.begin(); anim != animated_units_.end();++anim) {
	       if(anim->my_unit->get_animation()) {
                anim->my_unit->get_animation()->restart_animation();
	       }
        }
}
void unit_animator::set_all_standing()
{
        for(std::vector<anim_elem>::iterator anim = animated_units_.begin(); anim != animated_units_.end();++anim) {
		anim->my_unit->set_standing();
        }
}