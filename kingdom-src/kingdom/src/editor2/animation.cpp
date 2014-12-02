#define GETTEXT_DOMAIN "wesnoth-maker"

#include "global.hpp"
#include "game_config.hpp"
#include "loadscreen.hpp"
#include "DlgCoreProc.hpp"
#include "gettext.hpp"
#include "serialization/binary_or_text.hpp"
#include "serialization/parser.hpp"
#include "stdafx.h"
#include <windowsx.h>
#include <string.h>

#include "resource.h"

#include "xfunc.h"
#include "win32x.h"
#include "struct.h"
#include "rectangle.hpp"

#include <boost/foreach.hpp>

const std::string default_particular_frame_tag = "__new_frame";
std::set<std::string> reserved_particular_frame_tag;

const char* align_filter_names[] = {
	"none",
	"x",
	"non-x",
	"y",
	"non-y"
};

namespace ns {
	int clicked_anim;
	
	int action_anim;

	HIMAGELIST himl_checkbox;
	LPARAM clicked_param;
	HTREEITEM clicked_htvi;

	HIMAGELIST himl_anim;
	int iico_anim_anim;
	int iico_anim_particular;
	int iico_anim_filter;
	int iico_anim_time;
	int iico_anim_frame;
	int iico_anim_attribute;
}

std::vector<tanim_type2> tanim::anim_types;
tanim_type2 tanim::null_anim_type;

const tanim_type2& tanim::anim_type(const std::string& id)
{
	std::vector<tanim_type2>::const_iterator it = anim_types.begin();
	for (; it != anim_types.end(); ++ it) {
		const tanim_type2& type = *it;
		if (type.id_ == id) {
			return type;
		}
	}
	return null_anim_type;
}

tparticular::tparticular(const config& cfg, const std::string& frame_string)
	: animated<unit_frame>()
{
	config::const_child_itors range = cfg.child_range(frame_string + "frame");
	starting_frame_time_ = INT_MAX;
	if (cfg[frame_string + "start_time"].empty() &&range.first != range.second) {
		BOOST_FOREACH (const config &frame, range) {
			starting_frame_time_ = std::min(starting_frame_time_, frame["begin"].to_int());
		}
	} else {
		starting_frame_time_ = cfg[frame_string + "start_time"];
	}

	cycles_ = cfg[frame_string + "cycles"].to_bool();

	BOOST_FOREACH (const config &frame, range)
	{
		unit_frame tmp_frame(frame);
		add_frame(tmp_frame.duration(),tmp_frame,!tmp_frame.does_not_change());
	}
	parameters_ = frame_parsed_parameters(frame_builder(cfg,frame_string),get_animation_duration());
	if(!parameters_.does_not_change()  ) {
			force_change();
	}

	layer_ = cfg[frame_string + "layer"].to_int();
	zoom_x_ = cfg[frame_string + "zoom_x"].to_int();
	zoom_y_ = cfg[frame_string + "zoom_y"].to_int();
}

std::string tparticular::description() const
{
	std::stringstream strstr;

	if (!frames_.empty()) {
		strstr << "(" << get_begin_time() << ", " << get_end_time() << ") ";
		strstr << _("frame^Count") << "(" << frames_.size() << ") ";
	} else {
		strstr << _("None");
	}
	return strstr.str();
}

void tparticular::paramsters_update_to_ui_anim_edit(HWND hctl, HTREEITEM branch, const std::string& name, LPARAM param) const
{
	std::stringstream strstr;
	char text[4096];
	HTREEITEM htvi;

	HTREEITEM htvi_particular = TreeView_AddLeaf(hctl, branch);

	strstr.str("");
	strstr << name << ": " << description();
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	TreeView_SetItem1(hctl, htvi_particular, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		param, ns::iico_anim_particular, ns::iico_anim_particular, 1, text);

	htvi = TreeView_AddLeaf(hctl, htvi_particular);
	strstr.str("");
	strstr << _("Start time") << ": ";
	strstr << starting_frame_time_;
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		0, ns::iico_anim_attribute, ns::iico_anim_attribute, 0, text);

	htvi = TreeView_AddLeaf(hctl, htvi_particular);
	strstr.str("");
	strstr << "cycles" << ": ";
	strstr << (cycles_? "yes": "no");
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		0, ns::iico_anim_attribute, ns::iico_anim_attribute, 0, text);

	if (!parameters_.x_.get_original().empty()) {
		htvi = TreeView_AddLeaf(hctl, htvi_particular);
		strstr.str("");
		strstr << "x" << ": ";
		strstr << parameters_.x_.get_original();
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_anim_attribute, ns::iico_anim_attribute, 0, text);
	}

	if (!parameters_.y_.get_original().empty()) {
		htvi = TreeView_AddLeaf(hctl, htvi_particular);
		strstr.str("");
		strstr << "y" << ": ";
		strstr << parameters_.y_.get_original();
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_anim_attribute, ns::iico_anim_attribute, 0, text);
	}

	if (!parameters_.offset_x_.get_original().empty()) {
		htvi = TreeView_AddLeaf(hctl, htvi_particular);
		strstr.str("");
		strstr << "offset_x" << ": ";
		strstr << parameters_.offset_x_.get_original();
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_anim_attribute, ns::iico_anim_attribute, 0, text);
	}

	if (!parameters_.offset_y_.get_original().empty()) {
		htvi = TreeView_AddLeaf(hctl, htvi_particular);
		strstr.str("");
		strstr << "offset_y" << ": ";
		strstr << parameters_.offset_y_.get_original();
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_anim_attribute, ns::iico_anim_attribute, 0, text);
	}

	if (layer_) {
		htvi = TreeView_AddLeaf(hctl, htvi_particular);
		strstr.str("");
		strstr << "layer" << ": ";
		strstr << layer_;
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_anim_attribute, ns::iico_anim_attribute, 0, text);
	}

	if (zoom_x_) {
		htvi = TreeView_AddLeaf(hctl, htvi_particular);
		strstr.str("");
		strstr << "zoom_x" << ": ";
		strstr << zoom_x_;
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_anim_attribute, ns::iico_anim_attribute, 0, text);
	}

	if (zoom_y_) {
		htvi = TreeView_AddLeaf(hctl, htvi_particular);
		strstr.str("");
		strstr << "zoom_y" << ": ";
		strstr << zoom_y_;
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_anim_attribute, ns::iico_anim_attribute, 0, text);
	}
}

void tparticular::frame_update_to_ui_anim_edit(HWND hctl, HTREEITEM branch, const std::string& name, size_t n, size_t cookie_index) const
{
	std::stringstream strstr;
	char text[4096];
	HTREEITEM htvi;

	const animated<unit_frame>::frame& f = frames_[n];

	const frame_parsed_parameters& builder = f.value_.get_builder();

	HTREEITEM htvi_frame = TreeView_AddLeaf(hctl, branch);
	strstr << name;
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	TreeView_SetItem1(hctl, htvi_frame, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		tanim_::PARAM_FRAME + cookie_index, ns::iico_anim_frame, ns::iico_anim_frame, 1, text);

	htvi = TreeView_AddLeaf(hctl, htvi_frame);
	strstr.str("");
	strstr << "duration" << ": ";
	strstr << f.duration_;
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		0, ns::iico_anim_attribute, ns::iico_anim_attribute, 0, text);

	if (!builder.image_.empty()) {
		htvi = TreeView_AddLeaf(hctl, htvi_frame);
		strstr.str("");
		strstr << "image" << ": ";
		strstr << builder.image_;
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_anim_attribute, ns::iico_anim_attribute, 0, text);
	}

	if (!builder.image_mod_.empty()) {
		htvi = TreeView_AddLeaf(hctl, htvi_frame);
		strstr.str("");
		strstr << "image_mod" << ": ";
		strstr << builder.image_mod_;
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_anim_attribute, ns::iico_anim_attribute, 0, text);
	}

	if (!builder.image_diagonal_.get_filename().empty()) {
		htvi = TreeView_AddLeaf(hctl, htvi_frame);
		strstr.str("");
		strstr << "image_diagonal" << ": ";
		strstr << builder.image_diagonal_.get_filename();
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_anim_attribute, ns::iico_anim_attribute, 0, text);
	}

	if (!builder.image_horizontal_.get_filename().empty()) {
		htvi = TreeView_AddLeaf(hctl, htvi_frame);
		strstr.str("");
		strstr << "image_horizontal" << ": ";
		strstr << builder.image_horizontal_.get_filename();
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_anim_attribute, ns::iico_anim_attribute, 0, text);
	}

	if (!builder.x_.get_original().empty()) {
		htvi = TreeView_AddLeaf(hctl, htvi_frame);
		strstr.str("");
		strstr << "x" << ": ";
		strstr << builder.x_.get_original();
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_anim_attribute, ns::iico_anim_attribute, 0, text);
	}

	if (!builder.y_.get_original().empty()) {
		htvi = TreeView_AddLeaf(hctl, htvi_frame);
		strstr.str("");
		strstr << "y" << ": ";
		strstr << builder.y_.get_original();
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_anim_attribute, ns::iico_anim_attribute, 0, text);
	}

	if (!builder.halo_.get_original().empty()) {
		htvi = TreeView_AddLeaf(hctl, htvi_frame);
		strstr.str("");
		strstr << "halo" << ": ";
		strstr << builder.halo_.get_original();
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_anim_attribute, ns::iico_anim_attribute, 0, text);
	}

	if (!builder.halo_x_.get_original().empty()) {
		htvi = TreeView_AddLeaf(hctl, htvi_frame);
		strstr.str("");
		strstr << "halo_x" << ": ";
		strstr << builder.halo_x_.get_original();
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_anim_attribute, ns::iico_anim_attribute, 0, text);
	}

	if (!builder.halo_y_.get_original().empty()) {
		htvi = TreeView_AddLeaf(hctl, htvi_frame);
		strstr.str("");
		strstr << "halo_y" << ": ";
		strstr << builder.halo_y_.get_original();
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_anim_attribute, ns::iico_anim_attribute, 0, text);
	}

	if (!builder.offset_x_.get_original().empty()) {
		htvi = TreeView_AddLeaf(hctl, htvi_frame);
		strstr.str("");
		strstr << "offset_x" << ": ";
		strstr << builder.offset_x_.get_original();
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_anim_attribute, ns::iico_anim_attribute, 0, text);
	}

	if (!builder.offset_y_.get_original().empty()) {
		htvi = TreeView_AddLeaf(hctl, htvi_frame);
		strstr.str("");
		strstr << "offset_y" << ": ";
		strstr << builder.offset_y_.get_original();
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_anim_attribute, ns::iico_anim_attribute, 0, text);
	}

	if (!builder.auto_hflip_) {
		htvi = TreeView_AddLeaf(hctl, htvi_frame);
		strstr.str("");
		strstr << "auto_hflip" << ": no";
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_anim_attribute, ns::iico_anim_attribute, 0, text);
	}

	if (!builder.auto_vflip_) {
		htvi = TreeView_AddLeaf(hctl, htvi_frame);
		strstr.str("");
		strstr << "auto_vflip" << ": no";
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_anim_attribute, ns::iico_anim_attribute, 0, text);
	}

	if (!builder.sound_.empty()) {
		htvi = TreeView_AddLeaf(hctl, htvi_frame);
		strstr.str("");
		strstr << "sound" << ": ";
		strstr << builder.sound_;
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_anim_attribute, ns::iico_anim_attribute, 0, text);
	}

	if (!builder.stext_.empty()) {
		htvi = TreeView_AddLeaf(hctl, htvi_frame);
		strstr.str("");
		strstr << "stext" << ": ";
		strstr << builder.stext_;
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_anim_attribute, ns::iico_anim_attribute, 0, text);
	}

	if (builder.font_size_) {
		htvi = TreeView_AddLeaf(hctl, htvi_frame);
		strstr.str("");
		strstr << "font_size" << ": ";
		strstr << builder.font_size_;
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_anim_attribute, ns::iico_anim_attribute, 0, text);
	}

	if (builder.text_color_ != frame_builder::default_text_color) {
		htvi = TreeView_AddLeaf(hctl, htvi_frame);
		strstr.str("");
		strstr << "text_color" << ": ";
		strstr << ((builder.text_color_ & 0x00FF0000) >> 16) << "," << ((builder.text_color_ & 0x0000FF00) >> 8) << "," << (builder.text_color_ & 0x000000FF);
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_anim_attribute, ns::iico_anim_attribute, 0, text);
	}

	if (!builder.highlight_ratio_.get_original().empty()) {
		htvi = TreeView_AddLeaf(hctl, htvi_frame);
		strstr.str("");
		strstr << "alpha" << ": ";
		strstr << builder.highlight_ratio_.get_original();
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_anim_attribute, ns::iico_anim_attribute, 0, text);
	}
}

std::string prefix_from_tag(const std::string& tag)
{
	if (tag == "frame") {
		return null_str;
	}
	return tag.substr(0, tag.size() - 6);
}

void tparticular::update_to_ui_particular_edit(HWND hdlgP, const std::string& frame_tag) const
{
	std::string prefix = prefix_from_tag(frame_tag);
	if (prefix.empty()) {
		prefix = _("Master particular");
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_ANIMPARTICULAR_PREFIX), utf8_2_ansi(prefix.c_str()));
		Edit_Enable(GetDlgItem(hdlgP, IDC_ET_ANIMPARTICULAR_PREFIX), FALSE);
	} else {
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_ANIMPARTICULAR_PREFIX), prefix.c_str());
	}
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_ANIMPARTICULAR_TAG), frame_tag.c_str());

	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_ANIMPARTICULAR_STARTTIME), starting_frame_time_);
	
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_ANIMPARTICULAR_X), parameters_.x_.get_original().c_str());
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_ANIMPARTICULAR_Y), parameters_.y_.get_original().c_str());
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_ANIMPARTICULAR_OFFSET_X), parameters_.offset_x_.get_original().c_str());
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_ANIMPARTICULAR_OFFSET_Y), parameters_.offset_y_.get_original().c_str());

	Button_SetCheck(GetDlgItem(hdlgP, IDC_CHK_ANIMPARTICULAR_CYCLES), cycles_);
	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_ANIMPARTICULAR_LAYER), layer_);
	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_ANIMPARTICULAR_ZOOM_X), zoom_x_);
	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_ANIMPARTICULAR_ZOOM_Y), zoom_y_);
}

void tparticular::update_to_ui_frame_edit(HWND hdlgP, int n) const
{
	const animated<unit_frame>::frame& f = frames_[n];

	const frame_parsed_parameters& builder = f.value_.get_builder();

	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_ANIMFRAME_DURATION), builder.duration_);

	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_ANIMFRAME_IMAGE), builder.image_.c_str());
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_ANIMFRAME_IMAGEMOD), builder.image_mod_.c_str());
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_ANIMFRAME_IMAGEDIAGONAL), builder.image_diagonal_.get_filename().c_str());
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_ANIMFRAME_IMAGEHORIZONTAL), builder.image_horizontal_.get_filename().c_str());
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_ANIMFRAME_X), builder.x_.get_original().c_str());
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_ANIMFRAME_Y), builder.y_.get_original().c_str());
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_ANIMFRAME_HALO), builder.halo_.get_original().c_str());
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_ANIMFRAME_HALOX), builder.halo_x_.get_original().c_str());
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_ANIMFRAME_HALOY), builder.halo_y_.get_original().c_str());
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_ANIMFRAME_OFFSET_X), builder.offset_x_.get_original().c_str());
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_ANIMFRAME_OFFSET_Y), builder.offset_y_.get_original().c_str());
	Button_SetCheck(GetDlgItem(hdlgP, IDC_CHK_ANIMFRAME_HFLIP), builder.auto_hflip_);
	Button_SetCheck(GetDlgItem(hdlgP, IDC_CHK_ANIMFRAME_VFLIP), builder.auto_vflip_);
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_ANIMFRAME_SOUND), builder.sound_.c_str());

	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_ANIMFRAME_STEXT), builder.stext_.c_str());
	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_ANIMFRAME_FONTSIZE), builder.font_size_);

	SDL_Color color = int_to_color(builder.text_color_);
	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_ANIMFRAME_RED), color.r);
	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_ANIMFRAME_GREEN), color.g);
	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_ANIMFRAME_BLUE), color.b);

	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_ANIMFRAME_ALPHA), builder.highlight_ratio_.get_original().c_str());
}

void tanim::from_config(const config& cfg, bool global)
{
	id_ = cfg["id"].str();
	cfg_ = cfg.child("anim");
	global_ = global;
	anim_from_cfg_ = *this;

	parse(cfg_);
}

void tanim::parse(const config& cfg)
{
	const std::string& frame_string = "";

	sub_anims_.clear();
	
	directions_.clear();
	hits_.clear();
	primary_attack_filter_.clear();
	secondary_attack_filter_.clear();
	secondary_weapon_type_.clear();

	unit_anim_ = tparticular(cfg, "");
	BOOST_FOREACH (const config::any_child &fr, cfg.all_children_range())
	{
		if (fr.key == frame_string) continue;
		if (fr.key.find("_frame", fr.key.size() - 6) == std::string::npos) continue;
		if (sub_anims_.find(fr.key) != sub_anims_.end()) continue;
		sub_anims_[fr.key] = tparticular(cfg_, fr.key.substr(0, fr.key.size() - 5));
	}

	const std::vector<std::string>& my_directions = utils::split(cfg["direction"]);
	for(std::vector<std::string>::const_iterator i = my_directions.begin(); i != my_directions.end(); ++i) {
		const map_location::DIRECTION d = map_location::parse_direction(*i);
		directions_.insert(d);
	}

	feature_ = cfg["feature"].to_int(HEROS_NO_FEATURE);
	align_ = cfg["align"].to_int(ALIGN_NONE);


	std::vector<std::string> hits_str = utils::split(cfg["hits"]);
	std::vector<std::string>::iterator hit;
	for (hit=hits_str.begin() ; hit != hits_str.end() ; ++hit) {
		if(*hit == "yes" || *hit == "hit") {
			hits_.insert(HIT);
		}
		if(*hit == "no" || *hit == "miss") {
			hits_.insert(MISS);
		}
		if(*hit == "yes" || *hit == "kill" ) {
			hits_.insert(KILL);
		}
	}
	BOOST_FOREACH (const config &filter, cfg.child_range("filter_attack")) {
		primary_attack_filter_.push_back(filter);
		break;
	}
	BOOST_FOREACH (const config &filter, cfg.child_range("filter_second_attack")) {
		secondary_attack_filter_.push_back(filter);
		break;
	}

	std::vector<std::string> vstr = utils::split(cfg["secondary_weapon_type"]);
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		secondary_weapon_type_.insert(*it);
	}
	screen_mode_ = cfg["screen_mode"].to_bool();
}

void tanim::from_ui_anim_edit(HWND hdlgP)
{

}

void tanim::change_particular_name(const std::string& from, const std::string& to)
{
	const std::string prefix_from = prefix_from_tag(from);
	const std::string prefix_to = prefix_from_tag(to);

	std::map<std::string, std::string> modifies;
	modifies.insert(std::make_pair(prefix_from + "_start_time", prefix_to + "_start_time"));
	modifies.insert(std::make_pair(prefix_from + "_x", prefix_to + "_x"));
	modifies.insert(std::make_pair(prefix_from + "_y", prefix_to + "_y"));
	modifies.insert(std::make_pair(prefix_from + "_offset", prefix_to + "_offset"));
	
	for (std::map<std::string, std::string>::const_iterator it = modifies.begin(); it != modifies.end(); ++ it) {
		if (cfg_.has_attribute(it->first)) {
			cfg_[it->second] = cfg_[it->first];
			cfg_.remove_attribute(it->first);
		}
	}

	config cfg;
	BOOST_FOREACH (const config& frame, cfg_.child_range(from)) {
		cfg.add_child(to, frame);
	}
	cfg_.clear_children(from);
	cfg_.append_children(cfg);
}

bool tanim::from_ui_particular_edit(HWND hdlgP, int n)
{
	tparticular* l = &unit_anim_;
	std::string frame_tag_prefix = "";
	if (n > 0) {
		std::map<std::string, tparticular>::iterator it = sub_anims_.begin();
		std::advance(it, n - 1);
		l = &it->second;
		frame_tag_prefix = it->first.substr(0, it->first.size() - 5);
	}

	std::stringstream strstr;
	char text[4096];
	std::set<std::string> attribute;
	config particular_cfg;
	
	int start_time = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_ANIMPARTICULAR_STARTTIME));
	strstr.str("");
	strstr << frame_tag_prefix << "start_time";
	if (start_time) {
		particular_cfg[strstr.str()] = start_time;
	}
	attribute.insert(strstr.str());

	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_ANIMPARTICULAR_X), text, sizeof(text) / sizeof(text[0]));
	strstr.str("");
	strstr << frame_tag_prefix << "x";
	if (text[0] != '\0') {
		particular_cfg[strstr.str()] = text;
		
	}
	attribute.insert(strstr.str());

	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_ANIMPARTICULAR_Y), text, sizeof(text) / sizeof(text[0]));
	strstr.str("");
	strstr << frame_tag_prefix << "y";
	if (text[0] != '\0') {
		particular_cfg[strstr.str()] = text;
		
	}
	attribute.insert(strstr.str());

	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_ANIMPARTICULAR_OFFSET_X), text, sizeof(text) / sizeof(text[0]));
	strstr.str("");
	strstr << frame_tag_prefix << "offset_x";
	if (text[0] != '\0') {
		particular_cfg[strstr.str()] = text;
	}
	attribute.insert(strstr.str());

	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_ANIMPARTICULAR_OFFSET_Y), text, sizeof(text) / sizeof(text[0]));
	strstr.str("");
	strstr << frame_tag_prefix << "offset_y";
	if (text[0] != '\0') {
		particular_cfg[strstr.str()] = text;
	}
	attribute.insert(strstr.str());

	bool cycles = Button_GetCheck(GetDlgItem(hdlgP, IDC_CHK_ANIMPARTICULAR_CYCLES));
	strstr.str("");
	strstr << frame_tag_prefix << "cycles";
	if (cycles) {
		particular_cfg[strstr.str()] = "yes";
	}
	attribute.insert(strstr.str());

	int layer = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_ANIMPARTICULAR_LAYER));
	strstr.str("");
	strstr << frame_tag_prefix << "layer";
	if (layer) {
		particular_cfg[strstr.str()] = layer;
	}
	attribute.insert(strstr.str());

	int zoom = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_ANIMPARTICULAR_ZOOM_X));
	strstr.str("");
	strstr << frame_tag_prefix << "zoom_x";
	if (zoom) {
		particular_cfg[strstr.str()] = zoom;
	}
	attribute.insert(strstr.str());

	zoom = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_ANIMPARTICULAR_ZOOM_Y));
	strstr.str("");
	strstr << frame_tag_prefix << "zoom_y";
	if (zoom) {
		particular_cfg[strstr.str()] = zoom;
	}
	attribute.insert(strstr.str());

	bool changed = false;
	for (std::set<std::string>::const_iterator it = attribute.begin(); it != attribute.end(); ++ it) {
		const std::string& attri = *it;
		bool a_has = particular_cfg.has_attribute(attri);
		bool b_has = cfg_.has_attribute(attri);
		if (a_has && !b_has) {
			cfg_[attri] = particular_cfg[attri];
		} else if (!a_has && b_has) {
			cfg_.remove_attribute(attri);
		} else if (a_has && b_has && particular_cfg[attri] != cfg_[attri]) {
			cfg_[attri] = particular_cfg[attri];
		} else {
			continue;
		}
		changed = true;
	}

	if (n > 0) {
		std::map<std::string, tparticular>::iterator it = sub_anims_.begin();
		std::advance(it, n - 1);

		Edit_GetText(GetDlgItem(hdlgP, IDC_ET_ANIMPARTICULAR_TAG), text, sizeof(text) / sizeof(text[0]));
		std::string user_frame_tag = text;
		if (user_frame_tag != it->first) {
			change_particular_name(it->first, text);
		}
		changed = true;
	}
	return changed;
}

bool tanim::from_ui_filter_edit(HWND hdlgP)
{
	HWND hctl;
	char text[_MAX_PATH];
	std::set<hit_type> hits;
	
	if (Button_GetCheck(GetDlgItem(hdlgP, IDC_CHK_ANIMFILTER_HIT))) {
		hits.insert(HIT);
	}
	if (Button_GetCheck(GetDlgItem(hdlgP, IDC_CHK_ANIMFILTER_MISS))) {
		hits.insert(MISS);
	}
	if (Button_GetCheck(GetDlgItem(hdlgP, IDC_CHK_ANIMFILTER_KILL))) {
		hits.insert(KILL);
	}
	
	std::set<map_location::DIRECTION> directions;
	if (Button_GetCheck(GetDlgItem(hdlgP, IDC_CHK_ANIMFILTER_N))) {
		directions.insert(map_location::NORTH);
	}
	if (Button_GetCheck(GetDlgItem(hdlgP, IDC_CHK_ANIMFILTER_NE))) {
		directions.insert(map_location::NORTH_EAST);
	}
	if (Button_GetCheck(GetDlgItem(hdlgP, IDC_CHK_ANIMFILTER_NW))) {
		directions.insert(map_location::NORTH_WEST);
	}
	if (Button_GetCheck(GetDlgItem(hdlgP, IDC_CHK_ANIMFILTER_S))) {
		directions.insert(map_location::SOUTH);
	}
	if (Button_GetCheck(GetDlgItem(hdlgP, IDC_CHK_ANIMFILTER_SE))) {
		directions.insert(map_location::SOUTH_EAST);
	}
	if (Button_GetCheck(GetDlgItem(hdlgP, IDC_CHK_ANIMFILTER_SW))) {
		directions.insert(map_location::SOUTH_WEST);
	}

	std::set<std::string> secondary_weapon_type;
#ifndef _ROSE_EDITOR
	const std::vector<std::string>& atype_ids = unit_types.atype_ids();
	for (size_t n = 0; n < atype_ids.size(); n ++) {
		// IDM of weapon type checkbox must align with atype!
		if (Button_GetCheck(GetDlgItem(hdlgP, IDC_CHK_ANIMFILTER_BLADE + n))) {
			secondary_weapon_type.insert(atype_ids[n]);
		}
	}
#endif	
	hctl = GetDlgItem(hdlgP, IDC_CMB_ANIMFILTER_FEATURE);
	int feature = ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));

	hctl = GetDlgItem(hdlgP, IDC_CMB_ANIMFILTER_ALIGN);
	int align = ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));

	// primary_attack_filter
	config primary_attack_filter;
	hctl = GetDlgItem(hdlgP, IDC_CMB_ANIMFILTER_ID);
	ComboBox_GetText(hctl, text, _MAX_PATH);
	if (text[0] != '\0') {
		primary_attack_filter["name"] = text;
	}
	hctl = GetDlgItem(hdlgP, IDC_CMB_ANIMFILTER_RANGE);
	ComboBox_GetText(hctl, text, _MAX_PATH);
	if (text[0] != '\0') {
		primary_attack_filter["range"] = text;
	}

	// update fitler to cfg_
	std::stringstream strstr;
	bool changed = false;

	if (hits != hits_) {
		changed = true;
		strstr.str("");
		for (std::set<hit_type>::const_iterator it = hits.begin(); it != hits.end(); ++ it) {
			if (it != hits.begin()) {
				strstr << ", ";
			}
			if (*it == HIT) {
				strstr << "hit";
			} else if (*it == MISS) {
				strstr << "miss";
			} else if (*it == KILL) {
				strstr << "kill";
			} else {
				strstr << "invalid";
			} 
		}
		if (!strstr.str().empty()) {
			cfg_["hits"] = strstr.str();
		} else {
			cfg_.remove_attribute("hits");
		}
	}
	if (directions != directions_) {
		changed = true;
		strstr.str("");
		for (std::set<map_location::DIRECTION>::const_iterator it = directions.begin(); it != directions.end(); ++ it) {
			if (it != directions.begin()) {
				strstr << ", ";
			}
			strstr << map_location::write_direction(*it);
		}
		if (!strstr.str().empty()) {
			cfg_["direction"] = strstr.str();
		} else {
			cfg_.remove_attribute("direction");
		}
	}
	if (feature != feature_) {
		changed = true;
		if (feature != HEROS_NO_FEATURE) {
			cfg_["feature"] = feature;
		} else {
			cfg_.remove_attribute("feature");
		}
	}
	if (align != align_) {
		changed = true;
		if (align != ALIGN_NONE) {
			cfg_["align"] = align;
		} else {
			cfg_.remove_attribute("align");
		}
	}
	// simplely, support one only.
	config primary_attack_filter0_;
	if (!primary_attack_filter_.empty()) {
		primary_attack_filter0_ = primary_attack_filter_[0];
	}
	if (primary_attack_filter != primary_attack_filter0_) {
		changed = true;
		cfg_.clear_children("filter_attack");
		if (!primary_attack_filter.empty()) {
			cfg_.add_child("filter_attack", primary_attack_filter);
		}
	}
	if (secondary_weapon_type != secondary_weapon_type_) {
		changed = true;
		strstr.str("");
		for (std::set<std::string>::const_iterator it = secondary_weapon_type.begin(); it != secondary_weapon_type.end(); ++ it) {
			if (it != secondary_weapon_type.begin()) {
				strstr << ", ";
			}
			strstr << *it;
		}
		if (!strstr.str().empty()) {
			cfg_["secondary_weapon_type"] = strstr.str();
		} else {
			cfg_.remove_attribute("secondary_weapon_type");
		}
	}

	return changed;
}

std::string tanim::particular_frame_tag(const tparticular& l) const
{
	std::string frame_tag = "frame";
	for (std::map<std::string, tparticular>::const_iterator it = sub_anims_.begin(); it != sub_anims_.end(); ++ it) {
		if (&it->second == &l) {
			frame_tag = it->first;
			break;
		}
	}
	return frame_tag;
}

bool tanim::has_particular(const std::string& tag) const
{
	if (tag.empty()) {
		return true;
	}
	return sub_anims_.find(tag) != sub_anims_.end();
}

void tanim::add_particular()
{
	if (has_particular(default_particular_frame_tag)) {
		return;
	}
	std::stringstream strstr;
	strstr << default_particular_frame_tag.substr(0, default_particular_frame_tag.size() - 5) << "start_time";
	cfg_[strstr.str()] = 0;

	config add;
	add["duration"] = 1;
	cfg_.add_child(default_particular_frame_tag, add);
}

void tanim::delete_particular(int n)
{
	tparticular* l = &unit_anim_;
	std::string frame_tag_prefix = "";
	if (n > 0) {
		std::map<std::string, tparticular>::iterator it = sub_anims_.begin();
		std::advance(it, n - 1);
		l = &it->second;
		frame_tag_prefix = it->first.substr(0, it->first.size() - 5);
	}

	cfg_.remove_attribute(frame_tag_prefix + "start_time");
	cfg_.remove_attribute(frame_tag_prefix + "x");
	cfg_.remove_attribute(frame_tag_prefix + "y");
	cfg_.remove_attribute(frame_tag_prefix + "offset_x");
	cfg_.remove_attribute(frame_tag_prefix + "offset_y");

	cfg_.clear_children(frame_tag_prefix + "frame");
}

void tanim::add_frame(tparticular& l, int n, bool front)
{
	config add;
	add["duration"] = 1;

	std::string frame_tag = particular_frame_tag(l);
	cfg_.add_child_at(frame_tag, add, front? n: n + 1);
}

void tanim::delete_frame(tparticular& l, int n)
{
	std::string frame_tag = particular_frame_tag(l);
	cfg_.remove_child(frame_tag, n);
}

bool tanim::from_ui_frame_edit(HWND hdlgP, tparticular& l, int n)
{
	char text[4096];
	config cfg;
	cfg["duration"] = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_ANIMFRAME_DURATION));
	
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_ANIMFRAME_IMAGE), text, sizeof(text) / sizeof(text[0]));
	if (text[0] != '\0') {
		cfg["image"] = text;
	}
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_ANIMFRAME_IMAGEMOD), text, sizeof(text) / sizeof(text[0]));
	if (text[0] != '\0') {
		cfg["image_mod"] = text;
	}
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_ANIMFRAME_IMAGEDIAGONAL), text, sizeof(text) / sizeof(text[0]));
	if (text[0] != '\0') {
		cfg["image_diagonal"] = text;
	}
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_ANIMFRAME_IMAGEHORIZONTAL), text, sizeof(text) / sizeof(text[0]));
	if (text[0] != '\0') {
		cfg["image_horizontal"] = text;
	}
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_ANIMFRAME_X), text, sizeof(text) / sizeof(text[0]));
	if (text[0] != '\0') {
		cfg["x"] = text;
	}
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_ANIMFRAME_Y), text, sizeof(text) / sizeof(text[0]));
	if (text[0] != '\0') {
		cfg["y"] = text;
	}
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_ANIMFRAME_HALO), text, sizeof(text) / sizeof(text[0]));
	if (text[0] != '\0') {
		cfg["halo"] = text;
	}
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_ANIMFRAME_HALOX), text, sizeof(text) / sizeof(text[0]));
	if (text[0] != '\0') {
		cfg["halo_x"] = text;
	}
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_ANIMFRAME_HALOY), text, sizeof(text) / sizeof(text[0]));
	if (text[0] != '\0') {
		cfg["halo_y"] = text;
	}
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_ANIMFRAME_OFFSET_X), text, sizeof(text) / sizeof(text[0]));
	if (text[0] != '\0') {
		cfg["offset_x"] = text;
	}
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_ANIMFRAME_OFFSET_Y), text, sizeof(text) / sizeof(text[0]));
	if (text[0] != '\0') {
		cfg["offset_y"] = text;
	}
	if (!Button_GetCheck(GetDlgItem(hdlgP, IDC_CHK_ANIMFRAME_HFLIP))) {
		cfg["auto_hflip"] = "no";
	}
	if (!Button_GetCheck(GetDlgItem(hdlgP, IDC_CHK_ANIMFRAME_VFLIP))) {
		cfg["auto_vflip"] = "no";
	}
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_ANIMFRAME_SOUND), text, sizeof(text) / sizeof(text[0]));
	if (text[0] != '\0') {
		cfg["sound"] = text;
	}
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_ANIMFRAME_STEXT), text, sizeof(text) / sizeof(text[0]));
	const std::string stext = text;
	if (!stext.empty()) {
		cfg["stext"] = text;
	}
	int font_size = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_ANIMFRAME_FONTSIZE));
	if (font_size) {
		cfg["font_size"] = font_size;
	}

	if (!stext.empty()) {
		int color = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_ANIMFRAME_RED));
		color = (color << 8) + UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_ANIMFRAME_GREEN));
		color = (color << 8) + UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_ANIMFRAME_BLUE));
		if (color != frame_builder::default_text_color) {
			std::stringstream strstr;
			strstr << ((color & 0x00FF0000) >> 16) << "," << ((color & 0x0000FF00) >> 8) << "," << (color & 0x000000FF);
			cfg["text_color"] = strstr.str();
		}
	}

	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_ANIMFRAME_ALPHA), text, sizeof(text) / sizeof(text[0]));
	if (text[0] != '\0') {
		cfg["alpha"] = text;
	}

	std::string frame_tag = particular_frame_tag(l);
	config& frame_cfg_ = cfg_.child(frame_tag, n);
	if (frame_cfg_ != cfg) {
		frame_cfg_ = cfg;
		return true;
	}
	return false;
}

std::string tanim::filter_description() const
{
	std::stringstream strstr;
	if (!hits_.empty()) {
		strstr << "hits[" << cfg_["hits"] << "]";
	}
	if (!directions_.empty()) {
		if (!strstr.str().empty()) {
			strstr << "  ";
		}
		strstr << "direction[" << cfg_["direction"] << "]";
	}
	if (feature_ != HEROS_NO_FEATURE) {
		if (!strstr.str().empty()) {
			strstr << "  ";
		}
		strstr << "feature[" << hero::feature_str(feature_) << "]";
	}
	if (align_ != ALIGN_NONE) {
		if (!strstr.str().empty()) {
			strstr << "  ";
		}
		strstr << "align[" << align_filter_names[align_] << "]";
	}
	if (!primary_attack_filter_.empty()) {
		if (!strstr.str().empty()) {
			strstr << "  ";
		}
		for (std::vector<config>::const_iterator it = primary_attack_filter_.begin(); it != primary_attack_filter_.end(); ++ it) {
			const config& filter = *it;
			strstr << "primary_attack_filter[";
			bool first = true;
			BOOST_FOREACH (const config::attribute &istrmap, filter.attribute_range()) {
				if (!first) {
					strstr << "  ";
				} else {
					first = false;
				}
				strstr << istrmap.first << "(" << istrmap.second << ")";
			}
			strstr << "]";
		}
	}
	if (!secondary_weapon_type_.empty()) {
		if (!strstr.str().empty()) {
			strstr << "  ";
		}
		strstr << "secondary_seapon_type[" << cfg_["secondary_weapon_type"] << "]";
	}
	return strstr.str();
}

int tanim::get_end_time() const
{
	int result = unit_anim_.get_end_time();
	std::map<std::string, tparticular>::const_iterator anim_itor =sub_anims_.begin();
	for( /*null*/; anim_itor != sub_anims_.end() ; ++anim_itor) {
		result= std::max<int>(result,anim_itor->second.get_end_time());
	}
	return result;
}

int tanim::get_begin_time() const
{
	int result = unit_anim_.get_begin_time();
	std::map<std::string, tparticular>::const_iterator anim_itor =sub_anims_.begin();
	for( /*null*/; anim_itor != sub_anims_.end() ; ++anim_itor) {
		result= std::min<int>(result,anim_itor->second.get_begin_time());
	}
	return result;
}

void tanim::filter_update_to_ui_anim_edit(HWND hctl, HTREEITEM branch) const
{
	std::stringstream strstr;
	char text[_MAX_PATH];
	HTREEITEM htvi_filter, htvi;
	bool none_filter = true;

	htvi_filter = TreeView_AddLeaf(hctl, branch);
	strstr.str("");
	strstr << _("Filter");
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	TreeView_SetItem1(hctl, htvi_filter, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		PARAM_FILTER, ns::iico_anim_filter, ns::iico_anim_filter, 1, text);
	if (!hits_.empty()) {
		none_filter = false;
		htvi = TreeView_AddLeaf(hctl, htvi_filter);
		strstr.str("");
		strstr << "hits" << ": " << cfg_["hits"];
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_anim_attribute, ns::iico_anim_attribute, 0, text);
	}
	if (!directions_.empty()) {
		none_filter = false;
		htvi = TreeView_AddLeaf(hctl, htvi_filter);
		strstr.str("");
		strstr << "direction" << ": " << cfg_["direction"];
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_anim_attribute, ns::iico_anim_attribute, 0, text);
	}
	if (feature_ != HEROS_NO_FEATURE) {
		none_filter = false;
		htvi = TreeView_AddLeaf(hctl, htvi_filter);
		strstr.str("");
		strstr << "feature" << ": " << hero::feature_str(feature_);
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_anim_attribute, ns::iico_anim_attribute, 0, text);
	}
	if (align_ != ALIGN_NONE) {
		none_filter = false;
		htvi = TreeView_AddLeaf(hctl, htvi_filter);
		strstr.str("");
		strstr << "align" << ": " << align_filter_names[align_];
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_anim_attribute, ns::iico_anim_attribute, 0, text);
	}
	if (!primary_attack_filter_.empty()) {
		none_filter = false;
		for (std::vector<config>::const_iterator it = primary_attack_filter_.begin(); it != primary_attack_filter_.end(); ++ it) {
			const config& filter = *it;
			htvi = TreeView_AddLeaf(hctl, htvi_filter);
			strstr.str("");
			strstr << "primary_attack_filter" << ": ";
			bool first = true;
			BOOST_FOREACH (const config::attribute &istrmap, filter.attribute_range()) {
				if (!first) {
					strstr << "  ";
				} else {
					first = false;
				}
				strstr << istrmap.first << "(" << istrmap.second << ")";
			}
			strcpy(text, utf8_2_ansi(strstr.str().c_str()));
			TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
				0, ns::iico_anim_attribute, ns::iico_anim_attribute, 0, text);
		}
	}
	if (!secondary_attack_filter_.empty()) {
		none_filter = false;
		for (std::vector<config>::const_iterator it = secondary_attack_filter_.begin(); it != secondary_attack_filter_.end(); ++ it) {
			const config& filter = *it;
			htvi = TreeView_AddLeaf(hctl, htvi_filter);
			strstr.str("");
			strstr << "secondary_attack_filter" << ": ";
			bool first = true;
			BOOST_FOREACH (const config::attribute &istrmap, filter.attribute_range()) {
				if (!first) {
					strstr << "  ";
				} else {
					first = false;
				}
				strstr << istrmap.first << "(" << istrmap.second << ")";
			}
			strcpy(text, utf8_2_ansi(strstr.str().c_str()));
			TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
				0, ns::iico_anim_attribute, ns::iico_anim_attribute, 0, text);
		}
	}
	if (none_filter) {
		htvi = TreeView_AddLeaf(hctl, htvi_filter);
		strstr.str("");
		strstr << _("None");
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_anim_attribute, ns::iico_anim_attribute, 0, text);
	}
	if (!secondary_weapon_type_.empty()) {
		none_filter = false;
		htvi = TreeView_AddLeaf(hctl, htvi_filter);
		strstr.str("");
		strstr << "secondary_weapon_type" << ": " << cfg_["secondary_weapon_type"];
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_anim_attribute, ns::iico_anim_attribute, 0, text);
	}
}

void cb_treeview_update_scroll_anim(HWND htv, HTREEITEM htvi, TVITEMEX& tvi, void* ctx)
{
	tanim& anim = *reinterpret_cast<tanim*>(ctx);
	HTREEITEM htvi1;
	TVITEMEX tvi1;
	if (!tvi.cChildren) {
		htvi1 = TreeView_GetParent(htv, htvi);
		TreeView_GetItem1(htv, htvi1, &tvi1, TVIF_PARAM | TVIF_CHILDREN, NULL);
		scroll::first_visible_lparam = tvi1.lParam;
	}
	
	anim.update_to_ui_anim_edit(GetParent(htv));
}

void tanim::update_to_ui_anim_edit(HWND hdlgP)
{
	std::stringstream strstr;
	char text[_MAX_PATH];

	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_ANIMEDIT_ID), id_.c_str());
	
	HWND hctl = GetDlgItem(hdlgP, IDC_TV_ANIMEDIT_EXPLORER);
	TreeView_DeleteAllItems(hctl);
	HTREEITEM htvi_root, htvi_time;
	// id
	htvi_root = TreeView_AddLeaf(hctl, TVI_ROOT);
	strstr.str("");
	strstr << id_ << " (" << get_begin_time() << ", " << get_end_time() << ")";
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hctl, htvi_root, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		PARAM_ANIM, ns::iico_anim_anim, ns::iico_anim_anim, 1, text);

	HTREEITEM htvi_screen_mode = TreeView_AddLeaf(hctl, htvi_root);
	strstr.str("");
	strstr << _("Area mode") << ": " << (screen_mode_? _("Yes"): _("No"));
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	TreeView_SetItem1(hctl, htvi_screen_mode, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		PARAM_MODE, ns::iico_anim_attribute, ns::iico_anim_attribute, 0, text);
	
	// unit_anim_
	unit_anim_.paramsters_update_to_ui_anim_edit(hctl, htvi_root, _("Master particular"), PARAM_PARTICULAR);

	// sub_anim_
	for (std::map<std::string, tparticular>::iterator it = sub_anims_.begin(); it != sub_anims_.end(); ++ it) {
		const tparticular& l = it->second;
		int index = std::distance(sub_anims_.begin(), it);
		LPARAM param = PARAM_PARTICULAR + 1 + index;

		strstr.str("");
		strstr << _("Second particular") << "#" << index << "(" << it->first << ")";
		l.paramsters_update_to_ui_anim_edit(hctl, htvi_root, strstr.str(), param);
	}

	// filter
	if (!global_) {
		filter_update_to_ui_anim_edit(hctl, htvi_root);
	}

	// [frame] section
	int anim_end_time = get_end_time();
	std::map<int, int> analying_indexs, next_frame_start_time;
	analying_indexs.insert(std::make_pair(0, 0));
	if (!unit_anim_.frames_.empty()) {
		next_frame_start_time.insert(std::make_pair(0, unit_anim_.frames_[0].start_time_));
	} else {
		next_frame_start_time.insert(std::make_pair(0, anim_end_time));
	}
	for (std::map<std::string, tparticular>::iterator it = sub_anims_.begin(); it != sub_anims_.end(); ++ it) {
		const tparticular& l = it->second;
		int index = std::distance(sub_anims_.begin(), it);
		analying_indexs.insert(std::make_pair(1 + index, 0));
		if (!l.frames_.empty()) {
			next_frame_start_time.insert(std::make_pair(1 + index, l.frames_[0].start_time_));
		} else {
			next_frame_start_time.insert(std::make_pair(1 + index, anim_end_time));
		}
	}

	frame_cookie_.clear();

	bool one_particual_unfinised = false;
	int analying_time = get_begin_time();
	do {
		one_particual_unfinised = false;

		htvi_time = TreeView_AddLeaf(hctl, htvi_root);
		strstr.str("");
		strstr << analying_time << "(ms)";
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		TreeView_SetItem1(hctl, htvi_time, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			PARAM_TIME, ns::iico_anim_time, ns::iico_anim_time, 0, text);

		if (analying_indexs[0] < (int)unit_anim_.frames_.size()) {
			const animated<unit_frame>::frame& f = unit_anim_.frames_[analying_indexs[0]];
			if (f.start_time_ == analying_time) {
				frame_cookie_.push_back(std::make_pair(&unit_anim_, analying_indexs[0]));
				unit_anim_.frame_update_to_ui_anim_edit(hctl, htvi_time, _("Master particular"), analying_indexs[0], frame_cookie_.size() - 1);

				analying_indexs[0] ++;
				if (analying_indexs[0] < (int)unit_anim_.frames_.size()) {
					next_frame_start_time[0] = f.start_time_ + f.duration_;
				} else {
					next_frame_start_time[0] = anim_end_time;
				}
			}
			if (analying_indexs[0] < (int)unit_anim_.frames_.size()) {
				one_particual_unfinised = true;
			}
		}
		for (std::map<std::string, tparticular>::iterator it = sub_anims_.begin(); it != sub_anims_.end(); ++ it) {
			tparticular& l = it->second;
			int index = std::distance(sub_anims_.begin(), it);
			if (analying_indexs[index + 1] < (int)l.frames_.size()) {
				const animated<unit_frame>::frame& f = l.frames_[analying_indexs[index + 1]];
				if (f.start_time_ == analying_time) {
					strstr.str("");
					strstr << _("Second particular") << "#" << index << "(" << it->first << ")";
					frame_cookie_.push_back(std::make_pair(&l, analying_indexs[index + 1]));
					l.frame_update_to_ui_anim_edit(hctl, htvi_time, strstr.str(), analying_indexs[index + 1], frame_cookie_.size() - 1);

					analying_indexs[index + 1] ++;
					if (analying_indexs[index + 1] < (int)l.frames_.size()) {
						next_frame_start_time[index + 1] = f.start_time_ + f.duration_;
					} else {
						next_frame_start_time[index + 1] = anim_end_time;
					}
				}
				if (analying_indexs[index + 1] < (int)l.frames_.size()) {
					one_particual_unfinised = true;
				}
			}
		}

		int min_next = INT_MAX;
		for (std::map<int, int>::iterator it = next_frame_start_time.begin(); it != next_frame_start_time.end(); ++ it) {
			min_next = std::min<int>(min_next, it->second);
		}
		analying_time = min_next;
	} while (one_particual_unfinised);


	TreeView_Walk(hctl, TVI_ROOT, TRUE, cb_treeview_walk_expand, NULL, FALSE);
	scroll::treeview_scroll_to(hctl);
}

void tanim::update_to_ui_filter_edit(HWND hdlgP) const
{
	HWND hctl;

	if (std::find(hits_.begin(), hits_.end(), HIT) != hits_.end()) {
		Button_SetCheck(GetDlgItem(hdlgP, IDC_CHK_ANIMFILTER_HIT), TRUE);
	}
	if (std::find(hits_.begin(), hits_.end(), MISS) != hits_.end()) {
		Button_SetCheck(GetDlgItem(hdlgP, IDC_CHK_ANIMFILTER_MISS), TRUE);
	}
	if (std::find(hits_.begin(), hits_.end(), KILL) != hits_.end()) {
		Button_SetCheck(GetDlgItem(hdlgP, IDC_CHK_ANIMFILTER_KILL), TRUE);
	}

	if (std::find(directions_.begin(), directions_.end(), map_location::NORTH) != directions_.end()) {
		Button_SetCheck(GetDlgItem(hdlgP, IDC_CHK_ANIMFILTER_N), TRUE);
	}
	if (std::find(directions_.begin(), directions_.end(), map_location::NORTH_EAST) != directions_.end()) {
		Button_SetCheck(GetDlgItem(hdlgP, IDC_CHK_ANIMFILTER_NE), TRUE);
	}
	if (std::find(directions_.begin(), directions_.end(), map_location::NORTH_WEST) != directions_.end()) {
		Button_SetCheck(GetDlgItem(hdlgP, IDC_CHK_ANIMFILTER_NW), TRUE);
	}
	if (std::find(directions_.begin(), directions_.end(), map_location::SOUTH) != directions_.end()) {
		Button_SetCheck(GetDlgItem(hdlgP, IDC_CHK_ANIMFILTER_S), TRUE);
	}
	if (std::find(directions_.begin(), directions_.end(), map_location::SOUTH_EAST) != directions_.end()) {
		Button_SetCheck(GetDlgItem(hdlgP, IDC_CHK_ANIMFILTER_SE), TRUE);
	}
	if (std::find(directions_.begin(), directions_.end(), map_location::SOUTH_WEST) != directions_.end()) {
		Button_SetCheck(GetDlgItem(hdlgP, IDC_CHK_ANIMFILTER_SW), TRUE);
	}
#ifndef _ROSE_EDITOR
	const std::vector<std::string>& atype_ids = unit_types.atype_ids();
	for (size_t n = 0; n < atype_ids.size(); n ++) {
		// IDM of weapon type checkbox must align with atype!
		if (std::find(secondary_weapon_type_.begin(), secondary_weapon_type_.end(), atype_ids[n]) != secondary_weapon_type_.end()) {
			Button_SetCheck(GetDlgItem(hdlgP, IDC_CHK_ANIMFILTER_BLADE + n), TRUE);
		}
	}
#endif
	hctl = GetDlgItem(hdlgP, IDC_CMB_ANIMFILTER_FEATURE);
	for (int n = 0; n < ComboBox_GetCount(hctl); n ++) {
		if (ComboBox_GetItemData(hctl, n) == feature_) {
			ComboBox_SetCurSel(hctl, n);
		}
	}

	hctl = GetDlgItem(hdlgP, IDC_CMB_ANIMFILTER_ALIGN);
	for (int n = 0; n < ComboBox_GetCount(hctl); n ++) {
		if (ComboBox_GetItemData(hctl, n) == align_) {
			ComboBox_SetCurSel(hctl, n);
		}
	}
	
	if (!primary_attack_filter_.empty()) {
		const config& filter = primary_attack_filter_[0];

		BOOST_FOREACH (const config::attribute &istrmap, filter.attribute_range()) {
			if (istrmap.first == "name") {
				hctl = GetDlgItem(hdlgP, IDC_CMB_ANIMFILTER_ID);
				ComboBox_SetCurSel(hctl, ComboBox_FindString(hctl, -1, istrmap.second.str().c_str()));
			} else if (istrmap.first == "range") {
				hctl = GetDlgItem(hdlgP, IDC_CMB_ANIMFILTER_RANGE);
				ComboBox_SetCurSel(hctl, ComboBox_FindString(hctl, -1, istrmap.second.str().c_str()));
			}
		}
	}
}

std::string tanim::generate() const
{
	std::stringstream strstr;

	if (!global_) {
		strstr << "[utype_anim]\n";
	} else {
		strstr << "[area_anim]\n";
	}

	strstr << "\tid=" << id_ << "\n";
	strstr << "\t[anim]\n";
	::write(strstr, cfg_, 2);
	strstr << "\t[/anim]\n";

	if (!global_) {
		strstr << "[/utype_anim]";
	} else {
		strstr << "[/area_anim]";
	}
	strstr << "\n";

	return strstr.str();
}

void tcore::update_to_ui_anim(HWND hdlgP)
{
	HWND hctl = GetDlgItem(hdlgP, IDC_LV_ANIM_EXPLORER);
	LVITEM lvi;
	ListView_DeleteAllItems(hctl);
	std::stringstream strstr;
	char text[_MAX_PATH];
	int row = 0;

	for (std::vector<tanim>::const_iterator it = anims_updating_.begin(); it != anims_updating_.end(); ++ it) {
		const tanim& anim = *it;
		int column = 0;

		lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
		// number
		lvi.iItem = row;
		lvi.iSubItem = column ++;
		strstr.str("");
		strstr << row ++;
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		lvi.iImage = select_iimage_according_fname(text, 0);
		lvi.lParam = (LPARAM)0;
		ListView_InsertItem(hctl, &lvi);

		// id
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = column ++;
		strstr.str("");
		strstr << anim.id_;
		if (!anim.global_) {
			strstr << "(" << _("arms^Type") << ")";
		}
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// particular
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = column ++;
		strstr.str("");
		strstr << (1 + anim.sub_anims_.size());
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// screen mode
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = column ++;
		strstr.str("");
		strstr << (anim.screen_mode_? _("Yes"): _("No"));
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// filter
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = column ++;
		strstr.str("");
		strstr << anim.filter_description();
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// animation time
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = column ++;
		strstr.str("");
		strstr << "(" << anim.get_begin_time() << ", " << anim.get_end_time() << ")";
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// description
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = column ++;
		strstr.str("");
		strstr << tanim::anim_type(anim.id_).description_;
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);
	}
}

bool tcore::anims_dirty() const
{
	return ns::core.anims_updating_ != ns::core.anims_from_cfg_;
}

bool tanim::dirty() const
{
	if (*this != anim_from_cfg_) {
		return true;
	}
	return false;
}

BOOL On_DlgAnimInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	HWND hwndParent = GetParent(hdlgP); 
    DLGHDR *pHdr = (DLGHDR *) GetWindowLong(hwndParent, GWL_USERDATA);
    SetWindowPos(hdlgP, HWND_TOP, pHdr->rcDisplay.left, pHdr->rcDisplay.top, 0, 0, SWP_NOSIZE); 
/*
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_CANDIDATE), utf8_2_ansi(_("Candidate")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_OFFICE), utf8_2_ansi(_("Office")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_WANDER), utf8_2_ansi(_("Wander")));
*/
	HWND hctl = GetDlgItem(hdlgP, IDC_LV_ANIM_EXPLORER);
	LVCOLUMN lvc;
	char text[_MAX_PATH];
	std::stringstream strstr;
	int column = 0;

	//
	// anims
	//
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 40;
	strcpy(text, utf8_2_ansi(_("Number")));
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = column;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 140;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << _("ID");
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << _("Particular");
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << _("Area mode");
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 200;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << _("Filter");
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 90;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << _("Animation time");
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 600;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << _("Description");
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	return FALSE;
}

BOOL On_DlgAnimEditInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	std::stringstream strstr;
	if (ns::action_anim == ma_edit) {
		strstr << _("Edit animation");
	} else {
		strstr << _("Add animation");
	}
	strstr << "(" << _("Number") << ":" << ns::clicked_anim << ")";
	SetWindowText(hdlgP, utf8_2_ansi(strstr.str().c_str()));

	ShowWindow(GetDlgItem(hdlgP, IDCANCEL), SW_HIDE);

	ns::himl_anim = ImageList_Create(15, 15, ILC_COLOR24, 7, 0);
	ImageList_SetBkColor(ns::himl_anim, RGB(236, 233, 216));

    HICON hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_EVT_EVENT));
	ns::iico_anim_anim = ImageList_AddIcon(ns::himl_anim, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_PARTICULAR));
	ns::iico_anim_particular = ImageList_AddIcon(ns::himl_anim, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_EVT_FILTER));
	ns::iico_anim_filter = ImageList_AddIcon(ns::himl_anim, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_CLOCK));
	ns::iico_anim_time = ImageList_AddIcon(ns::himl_anim, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_PARTICULAR));
	ns::iico_anim_frame = ImageList_AddIcon(ns::himl_anim, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_EVT_ATTRIBUTE));
	ns::iico_anim_attribute = ImageList_AddIcon(ns::himl_anim, hicon);
	
	HWND hctl = GetDlgItem(hdlgP, IDC_TV_ANIMEDIT_EXPLORER);
	TreeView_SetImageList(hctl, ns::himl_anim, TVSIL_NORMAL);

	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_ID), utf8_2_ansi(_("ID")));

	tanim& anim = ns::core.anims_updating_[ns::clicked_anim];
	anim.update_to_ui_anim_edit(hdlgP);

	return FALSE;
}


BOOL On_DlgAnimFilterEditInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	std::stringstream strstr;
	strstr << _("anim^Edit filter");
	SetWindowText(hdlgP, utf8_2_ansi(strstr.str().c_str()));

	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_FEATURE), dgettext_2_ansi("wesnoth-hero", "feature"));

	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_ANIMFILTER_FEATURE);
	ComboBox_AddString(hctl, "");
	ComboBox_SetItemData(hctl, 0, HEROS_NO_FEATURE);

	std::vector<int>& features = hero::valid_features();
	for (std::vector<int>::const_iterator it = features.begin(); it != features.end(); ++ it) {
		if (*it >= HEROS_BASE_FEATURE_COUNT) {
			continue;
		}
		strstr.str("");
		strstr << HERO_PREFIX_STR_FEATURE << *it;
		ComboBox_AddString(hctl, utf8_2_ansi(dgettext("wesnoth-card", strstr.str().c_str()))); 
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, *it);
	}

	hctl = GetDlgItem(hdlgP, IDC_CMB_ANIMFILTER_ALIGN);
	for (int i = ALIGN_NONE; i < ALIGN_COUNT; i ++) {
		ComboBox_AddString(hctl, align_filter_names[i]);
		ComboBox_SetItemData(hctl, i, i);
	}

	hctl = GetDlgItem(hdlgP, IDC_CMB_ANIMFILTER_ID);
	ComboBox_AddString(hctl, "");
	ComboBox_AddString(hctl, "$attack_id");

	hctl = GetDlgItem(hdlgP, IDC_CMB_ANIMFILTER_RANGE);
	ComboBox_AddString(hctl, "");
	ComboBox_AddString(hctl, "$range");

	tanim& anim = ns::core.anims_updating_[ns::clicked_anim];
	anim.update_to_ui_filter_edit(hdlgP);

	return FALSE;
}

void On_DlgAnimFilterEditCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;

	switch (id) {
	case IDOK:
		changed = ns::core.anims_updating_[ns::clicked_anim].from_ui_filter_edit(hdlgP);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

BOOL CALLBACK DlgAnimFilterEditProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message) {
	case WM_INITDIALOG:
		return On_DlgAnimFilterEditInitDialog(hDlg, (HWND)(wParam), lParam);
	HANDLE_MSG(hDlg, WM_COMMAND, On_DlgAnimFilterEditCommand);
	HANDLE_MSG(hDlg, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	}
	
	return FALSE;
}

void OnAnimFilterEditBt(HWND hdlgP)
{
	RECT rcBtn;
	LPARAM lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_TV_ANIMEDIT_EXPLORER), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_ANIMFILTER), hdlgP, DlgAnimFilterEditProc, lParam)) {
		tanim& anim = ns::core.anims_updating_[ns::clicked_anim];
		anim.parse(anim.cfg_);
		scroll::treeview_update_scroll(GetDlgItem(hdlgP, IDC_TV_ANIMEDIT_EXPLORER), cb_treeview_update_scroll_anim, &anim);
	}
	return;
}

void variables_to_lv(HWND hctl, const std::string& id)
{
	char text[_MAX_PATH];
	LVCOLUMN lvc;
	// variable
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 150;
	strcpy(text, utf8_2_ansi(_("ID")));
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = 0;
	ListView_InsertColumn(hctl, 0, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 100;
	lvc.iSubItem = 1;
	strcpy(text, "");
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 1, &lvc);

	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	LVITEM lvi;
	int count = ListView_GetItemCount(hctl);
	ListView_DeleteAllItems(hctl);
	int index = 0;
	const tanim_type2& type = tanim::anim_type(id);

	for (std::map<std::string, std::string>::const_iterator it = type.variables_.begin(); it != type.variables_.end(); ++ it) {
		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		// id
		lvi.iItem = index;
		lvi.iSubItem = 0;
		strcpy(text, it->first.c_str());
		lvi.pszText = text;
		lvi.lParam = index ++;
		ListView_InsertItem(hctl, &lvi);

		// remark
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 1;
		strcpy(text, it->second.c_str());
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);
	}
}

BOOL On_DlgAnimFrameEditInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	std::stringstream strstr;
	strstr << _("anim^Edit frame");
	SetWindowText(hdlgP, utf8_2_ansi(strstr.str().c_str()));

	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_DURATION), utf8_2_ansi(_("frame^Duration")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_IMAGE), utf8_2_ansi(_("frame^Image")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_HALO), utf8_2_ansi(_("frame^Halo")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_IMAGEMOD), utf8_2_ansi(_("frame^Image mod")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_IMAGEDIAGONAL), utf8_2_ansi(_("frame^Image diagonal")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_X), utf8_2_ansi(_("frame^X")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_Y), utf8_2_ansi(_("frame^Y")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_HALO), utf8_2_ansi(_("frame^Halo")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_HALOX), utf8_2_ansi(_("frame^Halo x")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_HALOY), utf8_2_ansi(_("frame^Halo y")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_OFFSET_X), utf8_2_ansi(_("frame^Offset x")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_OFFSET_Y), utf8_2_ansi(_("frame^Offset y")));
	Button_SetText(GetDlgItem(hdlgP, IDC_CHK_ANIMFRAME_HFLIP), utf8_2_ansi(_("Enable auto-h-flip (Both image and halo)")));
	Button_SetText(GetDlgItem(hdlgP, IDC_CHK_ANIMFRAME_VFLIP), utf8_2_ansi(_("Enable auto-v-flip (Both image and halo)")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_SOUND), utf8_2_ansi(_("frame^Sound")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_STEXT), utf8_2_ansi(_("frame^Stext")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_FONTSIZE), utf8_2_ansi(_("frame^Font size")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_TEXTCOLOR), utf8_2_ansi(_("frame^Text color")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_ALPHA), utf8_2_ansi(_("frame^Alpha")));

	tanim& anim = ns::core.anims_updating_[ns::clicked_anim];

	HWND hctl = GetDlgItem(hdlgP, IDC_LV_ANIMFRAME_VARIABLE);
	variables_to_lv(hctl, anim.id_);

	hctl = GetDlgItem(hdlgP, IDC_UD_ANIMFRAME_DURATION);
	UpDown_SetRange(hctl, 1, 10000);	// [1, 10000]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_ANIMFRAME_DURATION));

	hctl = GetDlgItem(hdlgP, IDC_UD_ANIMFRAME_FONTSIZE);
	UpDown_SetRange(hctl, 0, 48);
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_ANIMFRAME_FONTSIZE));

	hctl = GetDlgItem(hdlgP, IDC_UD_ANIMFRAME_RED);
	UpDown_SetRange(hctl, 0, 255);
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_ANIMFRAME_RED));

	hctl = GetDlgItem(hdlgP, IDC_UD_ANIMFRAME_GREEN);
	UpDown_SetRange(hctl, 0, 255);
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_ANIMFRAME_GREEN));

	hctl = GetDlgItem(hdlgP, IDC_UD_ANIMFRAME_BLUE);
	UpDown_SetRange(hctl, 0, 255);
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_ANIMFRAME_BLUE));
	
	std::pair<tparticular*, int>& cookie = anim.frame_cookie_[ns::clicked_param - tanim::PARAM_FRAME];
	cookie.first->update_to_ui_frame_edit(hdlgP, cookie.second);

	return FALSE;
}

void On_DlgAnimFrameEditCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;
	tanim& anim = ns::core.anims_updating_[ns::clicked_anim];
	std::pair<tparticular*, int>& cookie = anim.frame_cookie_[ns::clicked_param - tanim::PARAM_FRAME];

	switch (id) {
	case IDOK:
		changed = anim.from_ui_frame_edit(hdlgP, *cookie.first, cookie.second);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

void animframeedit_notify_handler_dblclk(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	LVITEM					lvi;
	LPNMITEMACTIVATE		lpnmitem;

	// NM_表示对通用控件都通用,范围丛(0, 99)
	// TVN_表示只能TreeView通用,范围丛(400, 499)
	lpnmitem = (LPNMITEMACTIVATE)lpNMHdr;
	// 如果单击到的是复选框位置,把复选框状态置反
	// 当前定义的图标大小是16x16. ptAction反回的(x,y)是整个列表视图内的坐标,因而y值不大好判断
	// 认为如果x是小于16的就认为是击中复选框
	
	lvi.iItem = lpnmitem->iItem;
	lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
	lvi.iSubItem = 0;
	lvi.pszText = gdmgr._menu_text;
	lvi.cchTextMax = _MAX_PATH;
	ListView_GetItem(lpNMHdr->hwndFrom, &lvi);

	if (lpnmitem->iItem >= 0) {
		if (lpNMHdr->idFrom == IDC_LV_ANIMFRAME_VARIABLE) {
			Edit_SetText(GetDlgItem(hdlgP, IDC_ET_ANIMFRAME_IMAGE), lvi.pszText);
		}
	}
    return;
}

BOOL On_DlgAnimFrameEditNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	if (lpNMHdr->code == NM_DBLCLK) {
		animframeedit_notify_handler_dblclk(hdlgP, DlgItem, lpNMHdr);
	}
	return FALSE;
}

BOOL CALLBACK DlgAnimFrameEditProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message) {
	case WM_INITDIALOG:
		return On_DlgAnimFrameEditInitDialog(hDlg, (HWND)(wParam), lParam);
	HANDLE_MSG(hDlg, WM_COMMAND, On_DlgAnimFrameEditCommand);
	HANDLE_MSG(hDlg, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	HANDLE_MSG(hDlg, WM_NOTIFY,  On_DlgAnimFrameEditNotify);
	}
	
	return FALSE;
}

void OnAnimFrameEditBt(HWND hdlgP)
{
	RECT rcBtn;
	LPARAM lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_TV_ANIMEDIT_EXPLORER), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_ANIMFRAME), hdlgP, DlgAnimFrameEditProc, lParam)) {
		tanim& anim = ns::core.anims_updating_[ns::clicked_anim];
		anim.parse(anim.cfg_);
		scroll::treeview_update_scroll(GetDlgItem(hdlgP, IDC_TV_ANIMEDIT_EXPLORER), cb_treeview_update_scroll_anim, &anim);
	}
	return;
}

void OnAnimFrameAddBt(HWND hdlgP, bool front)
{
	tanim& anim = ns::core.anims_updating_[ns::clicked_anim];
	std::pair<tparticular*, int>& cookie = anim.frame_cookie_[ns::clicked_param - tanim::PARAM_FRAME];

	anim.add_frame(*cookie.first, cookie.second, front);
	
	anim.parse(anim.cfg_);
	scroll::treeview_update_scroll(GetDlgItem(hdlgP, IDC_TV_ANIMEDIT_EXPLORER), cb_treeview_update_scroll_anim, &anim);
	return;
}

void OnAnimFrameDelBt(HWND hdlgP)
{
	tanim& anim = ns::core.anims_updating_[ns::clicked_anim];
	std::pair<tparticular*, int>& cookie = anim.frame_cookie_[ns::clicked_param - tanim::PARAM_FRAME];

	char text[_MAX_PATH];
	std::stringstream strstr;
	utils::string_map symbols;
	symbols["particular"] = anim.particular_frame_tag(*cookie.first);
	strstr << utf8_2_ansi(vgettext2("Do you want to delete frame on $particular?", symbols).c_str());

	strcpy(text, utf8_2_ansi(_("Confirm delete")));
	int retval = MessageBox(gdmgr._hwnd_main, strstr.str().c_str(), text, MB_YESNO);
	if (retval == IDNO) {
		return;
	}

	anim.delete_frame(*cookie.first, cookie.second);
	
	anim.parse(anim.cfg_);
	scroll::treeview_update_scroll(GetDlgItem(hdlgP, IDC_TV_ANIMEDIT_EXPLORER), cb_treeview_update_scroll_anim, &anim);
	return;
}

void OnAnimAnimEditBt(HWND hdlgP)
{
	tanim& anim = ns::core.anims_updating_[ns::clicked_anim];

	if (anim.screen_mode_) {
		anim.cfg_.remove_attribute("screen_mode");
	} else {
		anim.cfg_["screen_mode"] = true;
	}
	anim.parse(anim.cfg_);
	scroll::treeview_update_scroll(GetDlgItem(hdlgP, IDC_TV_ANIMEDIT_EXPLORER), cb_treeview_update_scroll_anim, &anim);
}

BOOL On_DlgAnimParticularEditInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);
	std::stringstream strstr;
	strstr << _("anim^Edit particular");
	SetWindowText(hdlgP, utf8_2_ansi(strstr.str().c_str()));

	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_PREFIX), utf8_2_ansi(_("particular^Prefix")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_STARTTIME), utf8_2_ansi(_("particular^Start time")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_X), utf8_2_ansi(_("particular^X")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_Y), utf8_2_ansi(_("particular^Y")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_OFFSET_X), utf8_2_ansi(_("particular^Offset x")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_OFFSET_Y), utf8_2_ansi(_("particular^Offset y")));
	Button_SetText(GetDlgItem(hdlgP, IDC_CHK_ANIMPARTICULAR_CYCLES), utf8_2_ansi(_("particular^Cycles")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_LAYER), utf8_2_ansi(_("particular^Layer")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_ZOOM_X), utf8_2_ansi(_("particular^Zoom_x")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_ZOOM_Y), utf8_2_ansi(_("particular^Zoom_y")));

	HWND hctl = GetDlgItem(hdlgP, IDC_UD_ANIMPARTICULAR_STARTTIME);
	UpDown_SetRange(hctl, -10000, 10000);
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_ANIMPARTICULAR_STARTTIME));

	hctl = GetDlgItem(hdlgP, IDC_UD_ANIMPARTICULAR_LAYER);
	UpDown_SetRange(hctl, -100, 100);
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_ANIMPARTICULAR_LAYER));

	hctl = GetDlgItem(hdlgP, IDC_UD_ANIMPARTICULAR_ZOOM_X);
	UpDown_SetRange(hctl, 0, 500);
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_ANIMPARTICULAR_ZOOM_X));

	hctl = GetDlgItem(hdlgP, IDC_UD_ANIMPARTICULAR_ZOOM_Y);
	UpDown_SetRange(hctl, 0, 500);
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_ANIMPARTICULAR_ZOOM_Y));

	tanim& anim = ns::core.anims_updating_[ns::clicked_anim];
	int n = ns::clicked_param - tanim::PARAM_PARTICULAR;
	if (n == 0) {
		anim.unit_anim_.update_to_ui_particular_edit(hdlgP, "frame");
	} else {
		std::map<std::string, tparticular>::iterator it = anim.sub_anims_.begin();
		std::advance(it, n - 1);
		it->second.update_to_ui_particular_edit(hdlgP, it->first);
	}

	return FALSE;
}

void OnAnimParticularEditEt(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	char text[_MAX_PATH];
	std::stringstream strstr;

	if (codeNotify != EN_CHANGE || id != IDC_ET_ANIMPARTICULAR_PREFIX) {
		return;
	}

	if (reserved_particular_frame_tag.empty()) {
		reserved_particular_frame_tag.insert(default_particular_frame_tag);
		reserved_particular_frame_tag.insert("_add_text");
	}

	const tanim& anim = ns::core.anims_updating_[ns::clicked_anim];

	Edit_GetText(hwndCtrl, text, sizeof(text) / sizeof(text[0]));
	std::string str = text;
	std::transform(str.begin(), str.end(), str.begin(), std::tolower);
	if (!isvalid_id(str)) {
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_ANIMPARTICULAR_PREFIXSTATUS), utf8_2_ansi(_("Invalid string")));
		return;
	}
	if (reserved_particular_frame_tag.find(str) != reserved_particular_frame_tag.end()) {
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_ANIMPARTICULAR_PREFIXSTATUS), utf8_2_ansi(_("Reserved string")));
		return;
	}
	// cannot be exist same.
	int n = ns::clicked_param - tanim::PARAM_PARTICULAR - 1;
	for (std::map<std::string, tparticular>::const_iterator it = anim.sub_anims_.begin(); it != anim.sub_anims_.end(); ++ it) {
		int index = std::distance(anim.sub_anims_.begin(), it);
		if (index == n) {
			continue;
		}
		const std::string prefix = prefix_from_tag(it->first);
		if (str == prefix) {
			Edit_SetText(GetDlgItem(hdlgP, IDC_ET_ANIMPARTICULAR_PREFIXSTATUS), utf8_2_ansi(_("Other has holden it")));
			return;
		}
	}
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_ANIMPARTICULAR_PREFIXSTATUS), "");

	strstr.str("");
	strstr << str << "_frame";
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_ANIMPARTICULAR_TAG), strstr.str().c_str());
}

void On_DlgAnimParticularEditCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;
	
	switch (id) {
	case IDC_ET_ANIMPARTICULAR_PREFIX:
		OnAnimParticularEditEt(hdlgP, id, hwndCtrl, codeNotify);
		break;

	case IDOK:
		changed = ns::core.anims_updating_[ns::clicked_anim].from_ui_particular_edit(hdlgP, ns::clicked_param - tanim::PARAM_PARTICULAR);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

BOOL CALLBACK DlgAnimParticularEditProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message) {
	case WM_INITDIALOG:
		return On_DlgAnimParticularEditInitDialog(hDlg, (HWND)(wParam), lParam);
	HANDLE_MSG(hDlg, WM_COMMAND, On_DlgAnimParticularEditCommand);
	HANDLE_MSG(hDlg, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	}
	
	return FALSE;
}

void OnAnimParticularEditBt(HWND hdlgP)
{
	RECT rcBtn;
	LPARAM lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_TV_ANIMEDIT_EXPLORER), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_ANIMPARTICULAR), hdlgP, DlgAnimParticularEditProc, lParam)) {
		tanim& anim = ns::core.anims_updating_[ns::clicked_anim];
		anim.parse(anim.cfg_);
		scroll::treeview_update_scroll(GetDlgItem(hdlgP, IDC_TV_ANIMEDIT_EXPLORER), cb_treeview_update_scroll_anim, &anim);
	}
	return;
}

void OnAnimParticularAddBt(HWND hdlgP)
{
	tanim& anim = ns::core.anims_updating_[ns::clicked_anim];
	anim.add_particular();
	
	anim.parse(anim.cfg_);
	scroll::treeview_update_scroll(GetDlgItem(hdlgP, IDC_TV_ANIMEDIT_EXPLORER), cb_treeview_update_scroll_anim, &anim);
	return;
}

void OnAnimParticularDelBt(HWND hdlgP)
{
	tanim& anim = ns::core.anims_updating_[ns::clicked_anim];

	char text[_MAX_PATH];
	std::stringstream strstr;
	utils::string_map symbols;
	strstr << utf8_2_ansi(vgettext2("Do you want to delete this particular?", symbols).c_str());

	strcpy(text, utf8_2_ansi(_("Confirm delete")));
	int retval = MessageBox(gdmgr._hwnd_main, strstr.str().c_str(), text, MB_YESNO);
	if (retval == IDNO) {
		return;
	}

	anim.delete_particular(ns::clicked_param - tanim::PARAM_PARTICULAR);
	
	anim.parse(anim.cfg_);
	scroll::treeview_update_scroll(GetDlgItem(hdlgP, IDC_TV_ANIMEDIT_EXPLORER), cb_treeview_update_scroll_anim, &anim);
	return;
}

bool tvi_is_particular(int param)
{
	return param >= tanim::PARAM_PARTICULAR && param < tanim::PARAM_FRAME;
}

bool tvi_is_frame(int param)
{
	return param >= tanim::PARAM_FRAME;
}

void On_DlgAnimEditCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;

	switch (id) {

	case IDM_ADD:
		if (ns::clicked_param == tanim::PARAM_ANIM) {
			OnAnimParticularAddBt(hdlgP);
		}
		break;
	case IDM_ADD_FRONT:
	case IDM_ADD_BEHIND:
		if (ns::clicked_param >= tanim::PARAM_FRAME) {
			OnAnimFrameAddBt(hdlgP, id == IDM_ADD_FRONT);
		}
		break;
	
	case IDM_DELETE:
		TreeView_SetItemState(GetDlgItem(hdlgP, IDC_TV_ANIMEDIT_EXPLORER), ns::clicked_htvi, TVIS_BOLD, TVIS_BOLD);
		if (tvi_is_particular(ns::clicked_param)) {
			OnAnimParticularDelBt(hdlgP);
		} else if (tvi_is_frame(ns::clicked_param)) {
			OnAnimFrameDelBt(hdlgP);
		}
		TreeView_SetItemState(GetDlgItem(hdlgP, IDC_TV_ANIMEDIT_EXPLORER), ns::clicked_htvi, 0, TVIS_BOLD);
		break;

	case IDM_EDIT:
		TreeView_SetItemState(GetDlgItem(hdlgP, IDC_TV_ANIMEDIT_EXPLORER), ns::clicked_htvi, TVIS_BOLD, TVIS_BOLD);
		if (ns::clicked_param == tanim::PARAM_FILTER) {
			OnAnimFilterEditBt(hdlgP);
		} else if (tvi_is_particular(ns::clicked_param)) {
			OnAnimParticularEditBt(hdlgP);
		} else if (ns::clicked_param >= tanim::PARAM_FRAME) {
			OnAnimFrameEditBt(hdlgP);
		} else if (ns::clicked_param == tanim::PARAM_ANIM) {
			OnAnimAnimEditBt(hdlgP);
		}
		TreeView_SetItemState(GetDlgItem(hdlgP, IDC_TV_ANIMEDIT_EXPLORER), ns::clicked_htvi, 0, TVIS_BOLD);
		break;

	case IDOK:
		changed = TRUE;
		ns::core.anims_updating_[ns::clicked_anim].from_ui_anim_edit(hdlgP);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

void animedit_notify_handler_rclick(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	LPNMTREEVIEW			lpnmitem;
	HTREEITEM				htvi;
	TVITEMEX				tvi;
	POINT					point;

	if (lpNMHdr->idFrom != IDC_TV_ANIMEDIT_EXPLORER) {
		return;
	}

	lpnmitem = (LPNMTREEVIEW)lpNMHdr;

	// NM_RCLICK/NM_CLICK/NM_DBLCLK这些通知被发来后,其附代参数没法指定是哪个叶子句柄,
	// 需通过判断鼠标坐标来判断是哪个叶子被按下?
	// 1. GetCursorPos, 得到屏幕坐标系下的鼠标坐标
	// 2. TreeView_HitTest1(自写宏),由屏幕坐标系下的鼠标坐标返回指向的叶子句柄
	GetCursorPos(&point);	// 得到的是屏幕坐标
	TreeView_HitTest1(lpNMHdr->hwndFrom, point, htvi);
	TreeView_GetItem1(lpNMHdr->hwndFrom, htvi, &tvi, TVIF_PARAM | TVIF_CHILDREN, NULL);
	if (!htvi) {
		return;
	}
	if (!tvi.cChildren) {
		return;
	}
	if (tvi.lParam == tanim::PARAM_TIME) {
		return;
	}

	const tanim& anim = ns::core.anims_updating_[ns::clicked_anim];

	HMENU hpopup_new = CreatePopupMenu();
	AppendMenu(hpopup_new, MF_STRING, IDM_ADD_FRONT, utf8_2_ansi(_("In front of")));
	AppendMenu(hpopup_new, MF_STRING, IDM_ADD_BEHIND, utf8_2_ansi(_("Behind")));

	HMENU hpopup_anim = CreatePopupMenu();
	if (tvi.lParam == tanim::PARAM_ANIM) {
		AppendMenu(hpopup_anim, MF_STRING, IDM_ADD, utf8_2_ansi(_("Add particular")));
		if (anim.has_particular(default_particular_frame_tag)) {
			EnableMenuItem(hpopup_anim, IDM_ADD, MF_BYCOMMAND | MF_GRAYED);
		}
	} else if (tvi_is_frame(tvi.lParam)) {
		AppendMenu(hpopup_anim, MF_POPUP, (UINT_PTR)(hpopup_new), utf8_2_ansi(_("Add frame")));
	}
	if (tvi.lParam != tanim::PARAM_ANIM) {
		AppendMenu(hpopup_anim, MF_STRING, IDM_EDIT, utf8_2_ansi(_("Edit...")));
	}
	if (tvi_is_particular(tvi.lParam) || tvi_is_frame(tvi.lParam)) {
		AppendMenu(hpopup_anim, MF_STRING, IDM_DELETE, utf8_2_ansi(_("Delete...")));
	}

	TrackPopupMenuEx(hpopup_anim, 0, 
		point.x, 
		point.y, 
		hdlgP, 
		NULL);

	DestroyMenu(hpopup_new);
	DestroyMenu(hpopup_anim);

	ns::clicked_param = tvi.lParam;
	ns::clicked_htvi = htvi;

    return;
}

void animedit_notify_handler_dblclk(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	LPNMTREEVIEW			lpnmitem;
	HTREEITEM				htvi;
	TVITEMEX				tvi;
	POINT					point;

	if (lpNMHdr->idFrom != IDC_TV_ANIMEDIT_EXPLORER) {
		return;
	}

	lpnmitem = (LPNMTREEVIEW)lpNMHdr;

	// NM_RCLICK/NM_CLICK/NM_DBLCLK这些通知被发来后,其附代参数没法指定是哪个叶子句柄,
	// 需通过判断鼠标坐标来判断是哪个叶子被按下?
	// 1. GetCursorPos, 得到屏幕坐标系下的鼠标坐标
	// 2. TreeView_HitTest1(自写宏),由屏幕坐标系下的鼠标坐标返回指向的叶子句柄
	GetCursorPos(&point);	// 得到的是屏幕坐标
	TreeView_HitTest1(lpNMHdr->hwndFrom, point, htvi);
	TreeView_GetItem1(lpNMHdr->hwndFrom, htvi, &tvi, TVIF_PARAM | TVIF_CHILDREN, NULL);
	if (!htvi) {
		return;
	}
	if (tvi.cChildren) {
		return;
	}
	
	htvi = TreeView_GetParent(lpNMHdr->hwndFrom, htvi);
	TreeView_GetItem1(lpNMHdr->hwndFrom, htvi, &tvi, TVIF_PARAM | TVIF_CHILDREN, NULL);

	ns::clicked_param = tvi.lParam;
	ns::clicked_htvi = htvi;

	PostMessage(hdlgP, WM_COMMAND, IDM_EDIT, 0);

    return;
}

BOOL On_DlgAnimEditNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	if (lpNMHdr->code == NM_RCLICK) {
		animedit_notify_handler_rclick(hdlgP, DlgItem, lpNMHdr);
	} else if (lpNMHdr->code == NM_DBLCLK) {
		animedit_notify_handler_dblclk(hdlgP, DlgItem, lpNMHdr);
	}
	return FALSE;
}

BOOL CALLBACK DlgAnimEditProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message) {
	case WM_INITDIALOG:
		return On_DlgAnimEditInitDialog(hDlg, (HWND)(wParam), lParam);
	HANDLE_MSG(hDlg, WM_COMMAND, On_DlgAnimEditCommand);
	HANDLE_MSG(hDlg, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	HANDLE_MSG(hDlg, WM_NOTIFY,  On_DlgAnimEditNotify);
	}
	
	return FALSE;
}

void OnAnimAddBt(HWND hdlgP)
{
/*
	ns::core.factions_updating_.push_back(tfaction());
	ns::clicked_faction = ns::core.factions_updating_.size() - 1;

	ns::core.update_to_ui_faction(hdlgP, ns::clicked_faction);
	ns::core.set_dirty(tcore::BIT_FACTION, ns::core.factions_dirty()); 
*/
}

void OnAnimDelBt(HWND hdlgP)
{
/*
	std::vector<tfaction>::iterator it = ns::core.factions_updating_.begin();
	std::advance(it, ns::clicked_faction);
	ns::core.factions_updating_.erase(it);

	int clicked = ns::clicked_faction;
	if (clicked >= (int)ns::core.factions_updating_.size()) {
		clicked = -1;
	}
	ns::core.update_to_ui_faction(hdlgP, clicked);
	ns::core.set_dirty(tcore::BIT_FACTION, ns::core.factions_dirty());
*/
}

void OnAnimEditBt(HWND hdlgP)
{
	RECT rcBtn;
	LPARAM lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_LV_ANIM_EXPLORER), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	ns::action_anim = ma_edit;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_ANIMEDIT), hdlgP, DlgAnimEditProc, lParam)) {
		ns::core.update_to_ui_anim(hdlgP);
		ns::core.set_dirty(tcore::BIT_ANIM, ns::core.anims_dirty());
	}

	return;
}

void On_DlgAnimCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	tanim& f = ns::core.anims_updating_[ns::clicked_anim];
/*
	switch (id) {
	case IDM_ADD:
		OnFactionAddBt(hdlgP);
		break;
	case IDM_DELETE:
		OnFactionDelBt(hdlgP);
		break;
	case IDM_EDIT:
		OnFactionEditBt(hdlgP);
		break;

	case IDM_TOSERVICE:
	case IDM_TOWANDER:
		if (id == IDM_TOSERVICE) {
			f.freshes_.insert(ns::clicked_hero);
		} else {
			f.wanderes_.insert(ns::clicked_hero);
		}
		ns::core.update_to_ui_faction(hdlgP, ns::clicked_faction);
		ns::core.set_dirty(tcore::BIT_FACTION, ns::core.factions_dirty());
		break;
	case IDM_DELETE_ITEM0:
		if (ns::type == IDC_LV_FACTION_SERVICE) {
			if (f.leader_ == ns::clicked_hero) {
				f.leader_ = HEROS_INVALID_NUMBER;
			}
			f.freshes_.erase(ns::clicked_hero);
		} else if (ns::type == IDC_LV_FACTION_WANDER) {
			f.wanderes_.erase(ns::clicked_hero);
		}
		ns::core.update_to_ui_faction(hdlgP, ns::clicked_faction);
		ns::core.set_dirty(tcore::BIT_FACTION, ns::core.factions_dirty());
		break;
	case IDM_DELETE_ITEM1:
		if (ns::type == IDC_LV_FACTION_SERVICE) {
			f.leader_ = HEROS_INVALID_NUMBER;
			f.freshes_.clear();
		} else if (ns::type == IDC_LV_FACTION_WANDER) {
			f.wanderes_.clear();
		}
		ns::core.update_to_ui_faction(hdlgP, ns::clicked_faction);
		ns::core.set_dirty(tcore::BIT_FACTION, ns::core.factions_dirty());
		break;
	}
*/
	return;
}

void anim_notify_handler_dblclk(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	LVITEM					lvi;
	LPNMITEMACTIVATE		lpnmitem;

	// NM_表示对通用控件都通用,范围丛(0, 99)
	// TVN_表示只能TreeView通用,范围丛(400, 499)
	lpnmitem = (LPNMITEMACTIVATE)lpNMHdr;
	// 如果单击到的是复选框位置,把复选框状态置反
	// 当前定义的图标大小是16x16. ptAction反回的(x,y)是整个列表视图内的坐标,因而y值不大好判断
	// 认为如果x是小于16的就认为是击中复选框
	
	lvi.iItem = lpnmitem->iItem;
	lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
	lvi.iSubItem = 0;
	lvi.pszText = gdmgr._menu_text;
	lvi.cchTextMax = _MAX_PATH;
	ListView_GetItem(lpNMHdr->hwndFrom, &lvi);

	if (lpnmitem->iItem >= 0) {
		if (lpNMHdr->idFrom == IDC_LV_ANIM_EXPLORER) {
			ns::clicked_anim = lpnmitem->iItem;
			OnAnimEditBt(hdlgP);
		}
	}
    return;
}

BOOL On_DlgAnimNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	if (lpNMHdr->code == NM_RCLICK) {
		// anim_notify_handler_rclick(hdlgP, DlgItem, lpNMHdr);
	} else if (lpNMHdr->code == NM_DBLCLK) {
		anim_notify_handler_dblclk(hdlgP, DlgItem, lpNMHdr);
	}
	return FALSE;
}

void On_DlgAnimDestroy(HWND hdlgP)
{
	ImageList_Destroy(ns::himl_anim);
	return;
}

BOOL CALLBACK DlgAnimProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_INITDIALOG:
		return On_DlgAnimInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgAnimCommand);
	HANDLE_MSG(hdlgP, WM_NOTIFY,  On_DlgAnimNotify);
	HANDLE_MSG(hdlgP, WM_DESTROY, On_DlgAnimDestroy);
	}
	
	return FALSE;
}