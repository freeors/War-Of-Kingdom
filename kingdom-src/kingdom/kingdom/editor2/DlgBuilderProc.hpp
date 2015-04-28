#ifndef __DLGBUILDERPROC_HPP_
#define __DLGBUILDERPROC_HPP_

#include "global.hpp"
#include "stdafx.h"
#include <windowsx.h>
#include "config.hpp"
#include "editor.hpp"
#include "builder.hpp"
#include "image.hpp"

class tbuilding_rule
{
public:
	static std::map<const std::string, int> macro_tags;
	static int macro_type(const std::string& tag);
	static const std::string& macro_tag(int type);

	static std::map<int, int> idd_map;
	static std::map<int, DLGPROC> dlgproc_map;

	enum {NONE = 0, TRANSITION_COMPLETE_LF, NORMAL_FIRST = TRANSITION_COMPLETE_LF, TRANSITION_COMPLETE_LB, TRANSITION_COMPLETE_L, EDITOR_OVERLAY,
		BRIDGE_STRAIGHTS, BRIDGE_ENDS, BRIDGE_JOINTS, BRIDGE_CORNERS,
		LAYOUT_TRACKS_F, TRACK_COMPLETE, TRACK_BORDER_RESTRICTED_PLF,
		NEW_FOREST, OVERLAY_RANDOM, OVERLAY_RANDOM_LF, OVERLAY_RANDOM_L, OVERLAY_LF, OVERLAY_F, OVERLAY_P, OVERLAY_B, OVERLAY, 
		TERRAIN_BASE_RANDOM, TERRAIN_BASE_P, TERRAIN_BASE_F, TERRAIN_BASE, KEEP_BASE, KEEP_BASE_RANDOM, 
		TERRAIN_BASE_SINGLEHEX_LB, TERRAIN_BASE_SINGLEHEX_B, 
		OVERLAY_COMPLETE_LF, OVERLAY_COMPLETE_L, OVERLAY_COMPLETE_F, OVERLAY_COMPLETE, VOLCANO_2x2,
		OVERLAY_RESTRICTED3_F, OVERLAY_RESTRICTED2_F, OVERLAY_RESTRICTED_P, OVERLAY_RESTRICTED_F, 
		OVERLAY_ROTATION_RESTRICTED2_F, OVERLAY_ROTATION_RESTRICTED_F,
		MOUNTAINS_2x4_NW_SE, MOUNTAINS_2x4_SW_NE, MOUNTAINS_1x3_NW_SE, MOUNTAINS_1x3_SW_NE, MOUNTAINS_2x2, MOUNTAIN_SINGLE_RANDOM,
		PEAKS_1x2_SW_NE, PEAKS_LARGE,
		VILLAGE, VILLAGE_A3, VILLAGE_A4, NEW_FENCE, NEW_WALL_P, NEW_WALL, NEW_WALL2_P, NEW_WALL2, NEW_WAVES, NEW_BEACH,
		WALL_TRANSITION3, WALL_TRANSITION2_LF, WALL_TRANSITION_LF, WALL_ADJACENT_TRANSITION,
		ANIMATED_WATER_15, ANIMATED_WATER_15_TRANSITION, DISABLE_BASE_TRANSITIONS,
		NORMAL_LAST = DISABLE_BASE_TRANSITIONS};

	tbuilding_rule(int type);
	virtual ~tbuilding_rule() {};

	virtual void from_config(const config& cfg, const std::string& postfix);
	void from_ui(HWND hdlgP);

	virtual void from_ui_brule_edit(HWND hdlgP);
	virtual void update_to_ui_brule_edit(HWND hdlgP) const;
	virtual std::string generate(bool tpl = false) const;

	bool operator==(const tbuilding_rule& that) const
	{
		if (type_ != that.type_) return false;
		if (cfg_ != that.cfg_) return false;
		if (postfix_ != that.postfix_) return false;
		return true;
	}
	bool operator!=(const tbuilding_rule& that) const { return !operator==(that); }

	virtual std::string description() const { return null_str; }
public:
	int type_;
	config cfg_;
	std::string postfix_;
};

class tnone_brule : public tbuilding_rule
{
public:
	tnone_brule();
	std::string generate(bool tpl = false) const;

	void from_ui_brule_edit(HWND hdlgP);
	void update_to_ui_brule_edit(HWND hdlgP) const;

	void from_ui_tile_edit(HWND hdlgP);
	void update_to_ui_tile_edit(HWND hdlgP) const;
	void from_ui_image_edit(HWND hdlgP);
	void update_to_ui_image_edit(HWND hdlgP) const;

private:
	std::pair<std::string, std::string> get_flag(const config& tile) const;
};

class ttransition_complete_lf_brule : public tbuilding_rule
{
public:
	ttransition_complete_lf_brule() 
		: tbuilding_rule(TRANSITION_COMPLETE_LF)
	{}

	std::string description() const;
};

class ttransition_complete_lb_brule : public tbuilding_rule
{
public:
	ttransition_complete_lb_brule() 
		: tbuilding_rule(TRANSITION_COMPLETE_LB)
	{}

	std::string description() const;
};

class ttransition_complete_l_brule : public tbuilding_rule
{
public:
	ttransition_complete_l_brule() 
		: tbuilding_rule(TRANSITION_COMPLETE_L)
	{}

	std::string description() const;
};

class teditor_overlay_brule : public tbuilding_rule
{
public:
	teditor_overlay_brule() 
		: tbuilding_rule(EDITOR_OVERLAY)
	{}

	std::string description() const;
};

class tbridge_straights_brule : public tbuilding_rule
{
public:
	tbridge_straights_brule() 
		: tbuilding_rule(BRIDGE_STRAIGHTS)
	{}

	std::string description() const;
};

class tbridge_ends_brule : public tbuilding_rule
{
public:
	tbridge_ends_brule() 
		: tbuilding_rule(BRIDGE_ENDS)
	{}

	std::string description() const;
};

class tbridge_joints_brule : public tbuilding_rule
{
public:
	tbridge_joints_brule() 
		: tbuilding_rule(BRIDGE_JOINTS)
	{}

	std::string description() const;
};

class tbridge_corners_brule : public tbuilding_rule
{
public:
	tbridge_corners_brule() 
		: tbuilding_rule(BRIDGE_CORNERS)
	{}

	std::string description() const;
};

class tlayout_tracks_f_brule : public tbuilding_rule
{
public:
	tlayout_tracks_f_brule() 
		: tbuilding_rule(LAYOUT_TRACKS_F)
	{}

	std::string description() const;
};

class ttrack_complete_brule : public tbuilding_rule
{
public:
	ttrack_complete_brule() 
		: tbuilding_rule(TRACK_COMPLETE)
	{}

	std::string description() const;
};

class ttrack_border_restricted_plf_brule : public tbuilding_rule
{
public:
	ttrack_border_restricted_plf_brule() 
		: tbuilding_rule(TRACK_BORDER_RESTRICTED_PLF)
	{}

	std::string description() const;
};

class tnew_forest_brule : public tbuilding_rule
{
public:
	tnew_forest_brule() 
		: tbuilding_rule(NEW_FOREST)
	{}

	std::string description() const;
};

class toverlay_random_brule : public tbuilding_rule
{
public:
	toverlay_random_brule() 
		: tbuilding_rule(OVERLAY_RANDOM)
	{}

	std::string description() const;
};

class toverlay_random_lf_brule : public tbuilding_rule
{
public:
	toverlay_random_lf_brule() 
		: tbuilding_rule(OVERLAY_RANDOM_LF)
	{}

	std::string description() const;
};

class toverlay_random_l_brule : public tbuilding_rule
{
public:
	toverlay_random_l_brule() 
		: tbuilding_rule(OVERLAY_RANDOM_L)
	{}

	std::string description() const;
};

class toverlay_lf_brule : public tbuilding_rule
{
public:
	toverlay_lf_brule() 
		: tbuilding_rule(OVERLAY_LF)
	{}

	std::string description() const;
};

class toverlay_f_brule : public tbuilding_rule
{
public:
	toverlay_f_brule() 
		: tbuilding_rule(OVERLAY_F)
	{}

	std::string description() const;
};

class toverlay_p_brule : public tbuilding_rule
{
public:
	toverlay_p_brule() 
		: tbuilding_rule(OVERLAY_P)
	{}

	std::string description() const;
};

class toverlay_b_brule : public tbuilding_rule
{
public:
	toverlay_b_brule() 
		: tbuilding_rule(OVERLAY_B)
	{}

	std::string description() const;
};

class toverlay_brule : public tbuilding_rule
{
public:
	toverlay_brule() 
		: tbuilding_rule(OVERLAY)
	{}

	std::string description() const;
};

class tterrain_base_random_brule : public tbuilding_rule
{
public:
	tterrain_base_random_brule() 
		: tbuilding_rule(TERRAIN_BASE_RANDOM)
	{}

	std::string description() const;
};

class tterrain_base_p_brule : public tbuilding_rule
{
public:
	tterrain_base_p_brule() 
		: tbuilding_rule(TERRAIN_BASE_P)
	{}

	std::string description() const;
};

class tterrain_base_f_brule : public tbuilding_rule
{
public:
	tterrain_base_f_brule() 
		: tbuilding_rule(TERRAIN_BASE_F)
	{}

	std::string description() const;
};

class tterrain_base_brule : public tbuilding_rule
{
public:
	tterrain_base_brule() 
		: tbuilding_rule(TERRAIN_BASE)
	{}

	std::string description() const;
};

class tkeep_base_random_brule : public tbuilding_rule
{
public:
	tkeep_base_random_brule() 
		: tbuilding_rule(KEEP_BASE_RANDOM)
	{}

	std::string description() const;
};

class tkeep_base_brule : public tbuilding_rule
{
public:
	tkeep_base_brule() 
		: tbuilding_rule(KEEP_BASE)
	{}

	std::string description() const;
};

class tterrain_base_singlehex_lb_brule : public tbuilding_rule
{
public:
	tterrain_base_singlehex_lb_brule() 
		: tbuilding_rule(TERRAIN_BASE_SINGLEHEX_LB)
	{}

	std::string description() const;
};

class tterrain_base_singlehex_b_brule : public tbuilding_rule
{
public:
	tterrain_base_singlehex_b_brule() 
		: tbuilding_rule(TERRAIN_BASE_SINGLEHEX_B)
	{}

	std::string description() const;
};

class toverlay_complete_lf_brule : public tbuilding_rule
{
public:
	toverlay_complete_lf_brule() 
		: tbuilding_rule(OVERLAY_COMPLETE_LF)
	{}

	std::string description() const;
};

class toverlay_complete_l_brule : public tbuilding_rule
{
public:
	toverlay_complete_l_brule() 
		: tbuilding_rule(OVERLAY_COMPLETE_L)
	{}

	std::string description() const;
};

class toverlay_complete_f_brule : public tbuilding_rule
{
public:
	toverlay_complete_f_brule() 
		: tbuilding_rule(OVERLAY_COMPLETE_F)
	{}

	std::string description() const;
};

class toverlay_complete_brule : public tbuilding_rule
{
public:
	toverlay_complete_brule() 
		: tbuilding_rule(OVERLAY_COMPLETE)
	{}

	std::string description() const;
};

class tvolcano_2x2_brule : public tbuilding_rule
{
public:
	tvolcano_2x2_brule() 
		: tbuilding_rule(VOLCANO_2x2)
	{}

	std::string description() const;
};

class toverlay_restricted3_f_brule : public tbuilding_rule
{
public:
	toverlay_restricted3_f_brule() 
		: tbuilding_rule(OVERLAY_RESTRICTED3_F)
	{}

	std::string description() const;
};

class toverlay_restricted2_f_brule : public tbuilding_rule
{
public:
	toverlay_restricted2_f_brule() 
		: tbuilding_rule(OVERLAY_RESTRICTED2_F)
	{}

	std::string description() const;
};

class toverlay_restricted_p_brule : public tbuilding_rule
{
public:
	toverlay_restricted_p_brule() 
		: tbuilding_rule(OVERLAY_RESTRICTED_P)
	{}

	std::string description() const;
};

class toverlay_restricted_f_brule : public tbuilding_rule
{
public:
	toverlay_restricted_f_brule() 
		: tbuilding_rule(OVERLAY_RESTRICTED_F)
	{}

	std::string description() const;
};

class toverlay_rotation_restricted2_f_brule : public tbuilding_rule
{
public:
	toverlay_rotation_restricted2_f_brule() 
		: tbuilding_rule(OVERLAY_ROTATION_RESTRICTED2_F)
	{}

	std::string description() const;
};

class toverlay_rotation_restricted_f_brule : public tbuilding_rule
{
public:
	toverlay_rotation_restricted_f_brule() 
		: tbuilding_rule(OVERLAY_ROTATION_RESTRICTED_F)
	{}

	std::string description() const;
};

class tmountains_2x4_nw_se_brule : public tbuilding_rule
{
public:
	tmountains_2x4_nw_se_brule() 
		: tbuilding_rule(MOUNTAINS_2x4_NW_SE)
	{}

	std::string description() const;
};

class tmountains_2x4_sw_ne_brule : public tbuilding_rule
{
public:
	tmountains_2x4_sw_ne_brule() 
		: tbuilding_rule(MOUNTAINS_2x4_SW_NE)
	{}

	std::string description() const;
};

class tmountains_1x3_nw_se_brule : public tbuilding_rule
{
public:
	tmountains_1x3_nw_se_brule() 
		: tbuilding_rule(MOUNTAINS_1x3_NW_SE)
	{}

	std::string description() const;
};

class tmountains_1x3_sw_ne_brule : public tbuilding_rule
{
public:
	tmountains_1x3_sw_ne_brule() 
		: tbuilding_rule(MOUNTAINS_1x3_SW_NE)
	{}

	std::string description() const;
};

class tmountains_2x2_brule : public tbuilding_rule
{
public:
	tmountains_2x2_brule() 
		: tbuilding_rule(MOUNTAINS_2x2)
	{}

	std::string description() const;
};

class tmountain_single_random_brule : public tbuilding_rule
{
public:
	tmountain_single_random_brule() 
		: tbuilding_rule(MOUNTAIN_SINGLE_RANDOM)
	{}

	std::string description() const;
};

class tpeaks_1x2_sw_ne_brule : public tbuilding_rule
{
public:
	tpeaks_1x2_sw_ne_brule() 
		: tbuilding_rule(PEAKS_1x2_SW_NE)
	{}

	std::string description() const;
};

class tpeaks_large_brule : public tbuilding_rule
{
public:
	tpeaks_large_brule() 
		: tbuilding_rule(PEAKS_LARGE)
	{}

	std::string description() const;
};

class tvillage_brule : public tbuilding_rule
{
public:
	tvillage_brule() 
		: tbuilding_rule(VILLAGE)
	{}

	std::string description() const;
};

class tvillage_a3_brule : public tbuilding_rule
{
public:
	tvillage_a3_brule() 
		: tbuilding_rule(VILLAGE_A3)
	{}

	std::string description() const;
};

class tvillage_a4_brule : public tbuilding_rule
{
public:
	tvillage_a4_brule() 
		: tbuilding_rule(VILLAGE_A4)
	{}

	std::string description() const;
};

class tnew_fence_brule : public tbuilding_rule
{
public:
	tnew_fence_brule() 
		: tbuilding_rule(NEW_FENCE)
	{}

	std::string description() const;
};

class tnew_wall_p_brule : public tbuilding_rule
{
public:
	tnew_wall_p_brule() 
		: tbuilding_rule(NEW_WALL_P)
	{}

	std::string description() const;
};

class tnew_wall_brule : public tbuilding_rule
{
public:
	tnew_wall_brule() 
		: tbuilding_rule(NEW_WALL)
	{}

	std::string description() const;
};

class tnew_wall2_p_brule : public tbuilding_rule
{
public:
	tnew_wall2_p_brule() 
		: tbuilding_rule(NEW_WALL2_P)
	{}

	std::string description() const;
};

class tnew_wall2_brule : public tbuilding_rule
{
public:
	tnew_wall2_brule() 
		: tbuilding_rule(NEW_WALL2)
	{}

	std::string description() const;
};

class tnew_waves_brule : public tbuilding_rule
{
public:
	tnew_waves_brule() 
		: tbuilding_rule(NEW_WAVES)
	{}

	std::string description() const;
};

class tnew_beach_brule : public tbuilding_rule
{
public:
	tnew_beach_brule() 
		: tbuilding_rule(NEW_BEACH)
	{}

	std::string description() const;
};

class twall_transition3_brule : public tbuilding_rule
{
public:
	twall_transition3_brule() 
		: tbuilding_rule(WALL_TRANSITION3)
	{}

	std::string description() const;
};

class twall_transition2_lf_brule : public tbuilding_rule
{
public:
	twall_transition2_lf_brule() 
		: tbuilding_rule(WALL_TRANSITION2_LF)
	{}

	std::string description() const;
};

class twall_transition_lf_brule : public tbuilding_rule
{
public:
	twall_transition_lf_brule() 
		: tbuilding_rule(WALL_TRANSITION_LF)
	{}

	std::string description() const;
};

class twall_adjacent_transition_brule : public tbuilding_rule
{
public:
	twall_adjacent_transition_brule() 
		: tbuilding_rule(WALL_ADJACENT_TRANSITION)
	{}

	std::string description() const;
};

class tanimated_water_15_brule : public tbuilding_rule
{
public:
	tanimated_water_15_brule() 
		: tbuilding_rule(ANIMATED_WATER_15)
	{}

	std::string description() const;
};

class tanimated_water_15_transition_brule : public tbuilding_rule
{
public:
	tanimated_water_15_transition_brule() 
		: tbuilding_rule(ANIMATED_WATER_15_TRANSITION)
	{}

	std::string description() const;
};

class tdisable_base_transitions_brule : public tbuilding_rule
{
public:
	tdisable_base_transitions_brule() 
		: tbuilding_rule(DISABLE_BASE_TRANSITIONS)
	{}

	std::string description() const;
};

#endif // __DLGBUILDERPROC_HPP_
