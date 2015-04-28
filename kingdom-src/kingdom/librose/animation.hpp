/* $Id: unit_animation.hpp 47611 2010-11-21 13:56:47Z mordante $ */
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
#ifndef ANIMATION_H_INCLUDED
#define ANIMATION_H_INCLUDED

#include "animated.hpp"
#include "config.hpp"
#include "unit_frame.hpp"
#include <boost/function.hpp>

#define INVALID_ANIM_ID				-1

enum tanim_type {anim_map, anim_canvas, anim_float};

class display;
class controller_base;
class base_unit;

class animation
{
public:
	const unit_frame& get_last_frame() const{ return unit_anim_.get_last_frame() ; };
	void add_frame(int duration, const unit_frame& value, bool force_change =false)
	{ 
		unit_anim_.add_frame(duration,value,force_change) ; 
	};

	bool need_update() const;
	bool need_minimal_update() const;
	bool animation_finished() const;
	bool animation_finished_potential() const;
	void update_last_draw_time();
	int get_begin_time() const;
	int get_end_time() const;
	int time_to_tick(int animation_time) const { return unit_anim_.time_to_tick(animation_time); };
	int get_animation_time() const{ return unit_anim_.get_animation_time() ; };
	int get_animation_time_potential() const{ return unit_anim_.get_animation_time_potential() ; };
	void start_animation(int start_time
			, const map_location &src = map_location::null_location
			, const map_location &dst = map_location::null_location
			, bool cycles = false
			, const std::string& text = ""
			, const Uint32 text_color = 0
			, const bool accelerate = true);
	void update_parameters(const map_location &src, const map_location &dst);
	void pause_animation();
	void restart_animation();
	int get_current_frame_begin_time() const{ return unit_anim_.get_current_frame_begin_time() ; };
	void redraw(frame_parameters& value);
	void clear_haloes();
	virtual bool invalidate(const surface& screen, frame_parameters& value);
	bool invalidate(const surface& screen);
	void replace_image_name(const std::string& part, const std::string& src, const std::string& dst);
	void replace_image_mod(const std::string& part, const std::string& src, const std::string& dst);
	void replace_progressive(const std::string& part, const std::string& name, const std::string& src, const std::string& dst);
	void replace_static_text(const std::string& part, const std::string& src, const std::string& dst);
	void replace_int(const std::string& part, const std::string& name, int src, int dst);
	int layer() const { return layer_; }
	bool cycles() const { return cycles_; }
	bool started() const { return started_; }

	tanim_type type() const { return type_; }
	void set_type(tanim_type type) { type_ = type; }

	virtual void redraw(surface& screen, const SDL_Rect& rect);
	virtual void undraw(surface& screen) {}

	void set_callback_finish(boost::function<void (animation*)> callback) { callback_finish_ = callback; }
	void require_release();

	bool area_mode() const { return area_mode_; }

	explicit animation(const config &cfg, const std::string &frame_string = "");

	// reserved to class unit, for the special case of redrawing the unit base frame
	const frame_parameters get_current_params(const frame_parameters & default_val = frame_parameters()) const
	{ 
		return unit_anim_.parameters(default_val); 
	};

protected:
	explicit animation(int start_time
				, const unit_frame &frame
				, const std::string& event
				, const int variation
				, const frame_builder& builder = frame_builder());

	class particular:public animated<unit_frame>
	{
		public:
			explicit particular(int start_time=0,const frame_builder &builder = frame_builder()) :
				animated<unit_frame>(start_time),
				accelerate(true),
				cycles(false),
				parameters_(builder),
				halo_id_(0),
				last_frame_begin_time_(0)
				{};
			explicit particular(const config& cfg
					, const std::string& frame_string ="frame");

			virtual ~particular();
			bool need_update() const;
			bool need_minimal_update() const;
			void override(int start_time
					, int duration
					, const std::string& highlight = ""
					, const std::string& blend_ratio =""
					, Uint32 blend_color = 0
					, const std::string& offset = ""
					, const std::string& layer = ""
					, const std::string& modifiers = "");
			void redraw( const frame_parameters& value,const map_location &src, const map_location &dst);
			std::set<map_location> get_overlaped_hex(const frame_parameters& value, const map_location &src, const map_location &dst);
			std::vector<SDL_Rect> get_overlaped_rect(const frame_parameters& value, const map_location &src, const map_location &dst);
			void start_animation(int start_time, bool cycles=false);
			const frame_parameters parameters(const frame_parameters & default_val) const { return get_current_frame().merge_parameters(get_current_frame_time(),parameters_.parameters(get_animation_time()-get_begin_time()),default_val); };
			void clear_halo();
			void replace_image_name(const std::string& src, const std::string& dst);
			void replace_image_mod(const std::string& src, const std::string& dst);
			void replace_progressive(const std::string& name, const std::string& src, const std::string& dst);
			void replace_static_text(const std::string& src, const std::string& dst);
			void replace_int(const std::string& name, int src, int dst);

			bool accelerate;
			bool cycles;
		private:

			//animation params that can be locally overridden by frames
			frame_parsed_parameters parameters_;
			int halo_id_;
			int last_frame_begin_time_;

	};

protected:
	boost::function<void (animation*)> callback_finish_;

	tanim_type type_;
	particular unit_anim_;
	std::map<std::string, particular> sub_anims_;
	/* these are drawing parameters, but for efficiancy reason they are in the anim and not in the particle */
	map_location src_;
	map_location dst_;

	// optimization
	bool invalidated_;
	bool play_offscreen_;
	bool area_mode_;
	int layer_;
	bool cycles_;
	bool started_;
	std::set<map_location> overlaped_hex_;
};

class base_animator
{
public:
	base_animator()
		: animated_units_()
		, start_time_(INT_MIN)
	{}


	void add_animation2(base_unit* animated_unit
			, const animation * anim
			, const map_location& src = map_location::null_location
			, bool with_bars = false
			, bool cycles = false
			, const std::string& text = ""
			, const Uint32 text_color = 0);
	
	void start_animations();
	void pause_animation();
	void restart_animation();
	void clear() 
	{
		start_time_ = INT_MIN; 
		animated_units_.clear();
	}
	void set_all_standing();

	bool would_end() const;
	int get_animation_time() const;
	int get_animation_time_potential() const;
	int get_end_time() const;
	void wait_for_end(controller_base& controller) const;
	void wait_until(controller_base& controller, int animation_time) const;

	struct anim_elem 
	{
		anim_elem()
			: my_unit(0)
			, anim(0)
			, text()
			, text_color(0)
			, src()
			, with_bars(false)
			, cycles(false)
		{}

		base_unit* my_unit;
		const animation* anim;
		std::string text;
		Uint32 text_color;
		map_location src;
		bool with_bars;
		bool cycles;
	};
	std::vector<anim_elem>& animated_units() { return animated_units_; }
	const std::vector<anim_elem>& animated_units() const { return animated_units_; }

protected:
	std::vector<anim_elem> animated_units_;
	int start_time_;
};

#endif
