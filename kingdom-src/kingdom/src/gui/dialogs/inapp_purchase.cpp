/* $Id: language_selection.cpp 48740 2011-03-05 10:01:34Z mordante $ */
/*
   Copyright (C) 2008 - 2011 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "gui/dialogs/inapp_purchase.hpp"

#include "gui/dialogs/helper.hpp"
#include "formula_string_utils.hpp"
#include "gettext.hpp"
#ifdef GUI2_EXPERIMENTAL_LISTBOX
#include "gui/widgets/list.hpp"
#else
#include "gui/widgets/listbox.hpp"
#endif
#include "gui/widgets/button.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/scroll_label.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "language.hpp"
#include "game_preferences.hpp"
#include "help.hpp"
#include "gui/auxiliary/timer.hpp"

#include <boost/bind.hpp>

#if defined(__APPLE__) && TARGET_OS_IPHONE
extern "C" void startShop();
extern "C" void stopShop();
extern "C" void getProducts();
extern "C" void startAppPayment(const char* identifier);
extern "C" void restoreAppPayment();
#else

void startShop() {}
void stopShop() {}
void startAppPayment(const char* identifier) {}
void restoreAppPayment() {}

#endif

extern "C" void inapp_purchase_cb(const char* identifier, int complete)
{
	const std::string id = identifier;
	std::string short_id;
	bool restoring = (identifier == "@restore");
	int cursel = -1;

	if (!restoring) {
		size_t pos = id.rfind(".");
		if (pos != std::string::npos) {
			short_id = id.substr(pos + 1);
		} else {
			short_id = id;
		}
		for (std::map<int, std::string>::const_iterator it = game_config::inapp_items.begin(); it != game_config::inapp_items.end(); ++ it) {
			if (it->second == short_id) {
				cursel = it->first;
				break;
			}
		}
		if (cursel < 0) {
			return;
		}

		if (complete) {
			preferences::set_inapp_purchased(cursel, true);
			preferences::write_preferences();
		}
	}

	if (gui2::tinapp_purchase::get_singleton()) {
		gui2::tinapp_purchase& dlg = *gui2::tinapp_purchase::get_singleton();

		dlg.purchase_status(true);

		std::stringstream strstr;
		utils::string_map symbols;
		if (complete) {
			symbols["result"] = help::tintegrate::generate_format(_("Success"), "green");
		} else {
			symbols["result"] = help::tintegrate::generate_format(_("Failed"), "red");
		}

		if (!restoring) {
			gui2::tinapp_item& item = dlg.get_item(cursel);

			if (complete) {
				item.purchased = true;
				dlg.refresh_list(true);
			}
			symbols["name"] = help::tintegrate::generate_format(item.name, "blue");
			strstr << vgettext("wesnoth-lib", "Purchas '$name' $result|!", symbols);
		} else {
			strstr << vgettext("wesnoth-lib", "Restore purchased items $result|!", symbols);
		}
		dlg.status().set_label(strstr.str());
	}
}

namespace gui2 {

/*WIKI
 * @page = GUIWindowDefinitionWML
 * @order = 2_language_selection
 *
 * == Language selection ==
 *
 * This shows the dialog to select the language to use. When the dialog is
 * closed with the OK button it also updates the selected language in the
 * preferences.
 *
 * @begin{table}{dialog_widgets}
 *
 * language_list & & listbox & m &
 *         This listbox contains the list with available languages. $
 *
 * - & & control & o &
 *         Show the name of the language in the current row. $
 *
 * @end{table}
 */

/**
 * @todo show we also reset the translations and is the tips of day call
 * really needed?
 */

REGISTER_DIALOG(inapp_purchase)

#define NOUN_PREFIX_STR_INAPP		"inapp-"
#define NOUN_PREFIX_STR_INAPP_DESC	"inapp_desc-"

tinapp_item::tinapp_item(int _index, const std::string& _short_id)
	: index(_index)
	, short_id(_short_id)
	, purchased(false)
{
	if (!short_id.empty()) {
		id = std::string("com.ancientcc.testwin.") + short_id;

		std::stringstream strstr;
		strstr << NOUN_PREFIX_STR_INAPP << short_id;
		name = dgettext("wesnoth-card", strstr.str().c_str());

		strstr.str("");
		strstr << NOUN_PREFIX_STR_INAPP_DESC << short_id;
		description = dgettext("wesnoth-card", strstr.str().c_str());

		purchased = preferences::inapp_purchased(index);

		icon = std::string("inapp-purchase/") + short_id + ".png";
		price = 98 + index;
	} else {
		id = "";
		name = "";
		icon = "";
		price = 0;
	}
}

tinapp_purchase* tinapp_purchase::singleton_ = NULL;

tinapp_purchase::tinapp_purchase(bool browse)
	: browse_(browse)
	, items_()
	, purchase_(NULL)
	, status_(NULL)
	, list_(NULL)
	, ing_timer_(0)
	, ing_item_(-1)
{
	singleton_ = this;
}

tinapp_purchase::~tinapp_purchase()
{
	if (ing_timer_) {
		gui2::remove_timer(ing_timer_);
	}
	singleton_ = NULL;
}

void tinapp_purchase::item_selected(twindow& window)
{
	twindow::tinvalidate_layout_blocker blocker(window);

	tlistbox& list = find_widget<tlistbox>(&window, "item_list", false);

	int cursel = list.get_selected_row();

	purchase_->set_active(!browse_ && !items_[cursel].purchased && !ing_timer_);

	refresh_tip(window, items_[cursel]);
}

void tinapp_purchase::refresh_tip(twindow& window, const tinapp_item& item)
{
	std::stringstream strstr;

	strstr.str("");
	strstr << help::tintegrate::generate_format(_("Description"), "green") << "\n";
	strstr << item.description;
	tscroll_label* tip = find_widget<tscroll_label>(&window, "tip", false, true);
	tip->set_label(strstr.str());

	strstr.str("");
	strstr << help::tintegrate::generate_format(_("Notice"), "red") << "\n";
	strstr << _("Uninstall \"War of Kingdom\" will clear purchase recorder") << "\n";
	strstr << _("Update \"War of Kingdom\" will clear purchase recorder");
	tscroll_label* notice = find_widget<tscroll_label>(&window, "notice", false, true);
	notice->set_label(strstr.str());
}

void tinapp_purchase::refresh_list(bool set_purchase) const
{
	list_->clear();

	std::stringstream strstr;
	for (std::vector<tinapp_item>::const_iterator it = items_.begin(); it != items_.end(); ++ it) {
		string_map list_item;
		std::map<std::string, string_map> list_item_item;

		strstr.str("");
		strstr << it->icon;
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("icon", list_item));

		strstr.str("");
		strstr << it->name;
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("name", list_item));

		strstr.str("");
		strstr << "USD $1.99";
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("cost", list_item));

		list_->add_row(list_item_item);
		tgrid* grid = list_->get_row_grid(list_->get_item_count() - 1);
		twidget* widget = grid->find("purchase", false);
		widget->set_visible(it->purchased? twidget::VISIBLE: twidget::INVISIBLE);
	}

	if (set_purchase) {
		purchase_->set_active(!browse_ && !items_[0].purchased);
	}
}

void tinapp_purchase::pre_show(CVideo& /*video*/, twindow& window)
{
	std::stringstream strstr;
	if (browse_) {
		utils::string_map symbols;
		symbols["platforms"] = help::tintegrate::generate_format("iOS", "green");

		tlabel* label = find_widget<tlabel>(&window, "flag", false, true);
		strstr.str("");
		strstr << "(";
		strstr << vgettext("wesnoth-lib", "Only $platforms support in-app purchase", symbols);
		strstr << ")";
		label->set_label(strstr.str());
	}

	restore_ = find_widget<tbutton>(&window, "restore", false, true);
	purchase_ = find_widget<tbutton>(&window, "purchase", false, true);
	status_ = find_widget<tlabel>(&window, "status", false, true);
	list_ = find_widget<tlistbox>(&window, "item_list", false, true);

	// restore_->set_visible(twidget::INVISIBLE);

	for (std::map<int, std::string>::const_iterator it = game_config::inapp_items.begin(); it != game_config::inapp_items.end(); ++ it) {
		items_.push_back(tinapp_item(it->first, it->second));
	}

	refresh_list();

	list_->set_callback_value_change(dialog_callback<tinapp_purchase, &tinapp_purchase::item_selected>);
	item_selected(window);

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "restore", false)
		, boost::bind(
			&tinapp_purchase::restore
			, this
			, boost::ref(window)));
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "purchase", false)
		, boost::bind(
			&tinapp_purchase::purchase
			, this
			, boost::ref(window)));

	startShop();
}

void tinapp_purchase::post_show(twindow& window)
{
	stopShop();
}

void tinapp_purchase::restore(twindow& window)
{
	restoreAppPayment();

	ing_item_ = -1;
	purchase_status(false);
}

void tinapp_purchase::purchase(twindow& window)
{
	int res = find_widget<tlistbox>(&window, "item_list", false).get_selected_row();
	tinapp_item& item = items_[res];

	startAppPayment(item.id.c_str());

	ing_item_ = res;
	purchase_status(false);
}

void tinapp_purchase::timer_handler()
{
	std::stringstream strstr;
	utils::string_map symbols;
	if (ing_item_ >= 0) {
		symbols["name"] = help::tintegrate::generate_format(items_[ing_item_].name, "blue");
		strstr << vgettext("wesnoth-lib", "Purchasing '$name'", symbols);
	} else {
		strstr << vgettext("wesnoth-lib", "Restoring purchased items", symbols);
	}
	
	std::string bar = "..........";
	bar.resize(ing_ticks_ % 10);
	strstr << bar;
	status_->set_label(strstr.str());

	ing_ticks_ ++;
}

tinapp_item& tinapp_purchase::get_item(int index)
{
	static tinapp_item null_item(-1);

	for (std::vector<tinapp_item>::iterator it = items_.begin(); it != items_.end(); ++ it) {
		if (it->index == index) {
			return *it;
		}
	}

	return null_item;
}

void tinapp_purchase::purchase_status(bool exit)
{
	restore_->set_active(exit);

	if (!exit) {
		purchase_->set_active(false);

		ing_ticks_ = 0;
		ing_timer_ = add_timer(50, boost::bind(&tinapp_purchase::timer_handler, this), true);
	} else {
		gui2::remove_timer(ing_timer_);
		ing_timer_ = 0;
		ing_item_ = -1;
	}
}

} // namespace gui2
