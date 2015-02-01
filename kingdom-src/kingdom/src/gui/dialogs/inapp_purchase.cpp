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
#include "game_display.hpp"
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
#include "gui/dialogs/alipay.hpp"
#include "gui/dialogs/message.hpp"
#include "language.hpp"
#include "game_preferences.hpp"
#include "gui/auxiliary/timer.hpp"
#include "filesystem.hpp"
#include "multiplayer.hpp"

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

std::string format_time_local2(time_t t)
{
	char time_buf[256] = {0};
	tm* tm_l = localtime(&t);
	if (tm_l) {
		strftime(time_buf, sizeof(time_buf), dsgettext("wesnoth", "%b %d %y"), tm_l);
	}

	return time_buf;
}

void show_fail_tip(display& disp, const std::string& item)
{
	std::stringstream err;
	utils::string_map symbols;

	symbols["mail"] = tintegrate::generate_format(game_config::service_email, "green");
	symbols["date"] = tintegrate::generate_format(format_time_local2(time(NULL)), "yellow");
	symbols["item"] = tintegrate::generate_format(item, "yellow");
	symbols["username"] = tintegrate::generate_format(preferences::login(), "yellow");
	err << vgettext("wesnoth-lib", "Update database fail when execute In-App Purchase! In order to compensate data you should get, please send In-App Purchase information to $mail. In-App Purchase information include: date($date), item($item), username($username).", symbols);
	gui2::show_message(disp.video(), "", err.str());
}

void upload_inapp_2_data_server(int inapp, display& disp, hero_map& heros, const std::string& name)
{
	const std::string number = "ios_magic_number_123";
	const std::string buyer = "ios_magic_buyer_1234";
	http::membership m = http::insert_transaction(disp, heros, number, buyer, group.leader().uid(), inapp); 
	if (m.uid >= 0) {
		group.from_local_membership(disp, heros, m, false);
	} else {
		show_fail_tip(disp, name);
	}
}

extern "C" void inapp_purchase_cb(const char* identifier, int complete)
{
	const std::string id = identifier;
	std::string short_id;
	bool restoring = std::string(identifier) == "@restore";
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
			gui2::tinapp_purchase* dlg = gui2::tinapp_purchase::get_singleton();
			if (dlg) {
				gui2::tinapp_item& item = dlg->get_item(cursel);
				upload_inapp_2_data_server(cursel, dlg->disp(), dlg->heros(), item.name);
			}
		}
	}

	if (gui2::tinapp_purchase::get_singleton()) {
		gui2::tinapp_purchase& dlg = *gui2::tinapp_purchase::get_singleton();

		dlg.purchase_status(true);

		std::stringstream strstr;
		utils::string_map symbols;
		if (complete) {
			symbols["result"] = tintegrate::generate_format(_("Success"), "green");
		} else {
			symbols["result"] = tintegrate::generate_format(_("Failed"), "red");
		}

		if (!restoring) {
			gui2::tinapp_item& item = dlg.get_item(cursel);

			if (complete) {
				if (!item.consumable) {
					item.purchased = true;
				}
				dlg.refresh_list(false, true);
			}

			symbols["name"] = tintegrate::generate_format(item.name, "blue");
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
	, consumable(true)
{
	if (!short_id.empty()) {
		id = std::string("com.ancientcc.testwin.") + short_id;

		std::stringstream strstr;
		strstr << NOUN_PREFIX_STR_INAPP << short_id;
		name = dgettext("wesnoth-card", strstr.str().c_str());
		
		strstr.str("");
		strstr << NOUN_PREFIX_STR_INAPP_DESC << short_id;
		description = dgettext("wesnoth-card", strstr.str().c_str());
		
		strstr.str("");
#if (defined(__APPLE__) && TARGET_OS_IPHONE)
#else
		if (index == game_config::transaction_type_vip) {
			int start;
			int increase = 30 * 24 * 3600;
			if (preferences::vip_expire() < time(NULL)) {
				start = time(NULL);
			} else {
				start = preferences::vip_expire();
			}
			strstr << format_time_ymd(start) << "--" << format_time_ymd(start + increase);
		}
#endif
		remark = strstr.str();

		purchased = !consumable && preferences::inapp_purchased(index);

		icon = std::string("inapp-purchase/") + short_id + ".png";
		price = 98 + index;
	} else {
		id = "";
		name = "";
		icon = "";
		remark = "";
		price = 0;
	}
}

tinapp_purchase* tinapp_purchase::singleton_ = NULL;

tinapp_purchase::tinapp_purchase(display& disp, hero_map& heros, bool browse)
	: disp_(disp)
	, heros_(heros)
	, browse_(browse)
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

	purchase_->set_active(!browse_ && !ing_timer_);

	refresh_tip(window, items_[cursel]);
}

void tinapp_purchase::refresh_tip(twindow& window, const tinapp_item& item)
{
	std::stringstream strstr;
	utils::string_map symbols;

	strstr.str("");
	strstr << tintegrate::generate_format(_("Description"), "green") << "\n";
	strstr << item.description;
	strstr << "\n\n";
	strstr << tintegrate::generate_format(_("Notice"), "red") << "\n";
	if (item.index == game_config::transaction_type_vip) {
#if (defined(__APPLE__) && TARGET_OS_IPHONE)
#else
		if (preferences::vip2()) {
			int day = 30;
			if (preferences::vip_expire() >= time(NULL)) {
				day = (preferences::vip_expire() - time(NULL)) / (24 * 3600);
			}
			symbols["day"] = tintegrate::generate_format(day, "green");
			strstr << vgettext("wesnoth-lib", "You is VIP currently, hold $day day still", symbols) << "\n";
		} else {
			strstr << _("You isn't VIP currently") << "\n";
		}
#endif
	} else {
		symbols["username"] = tintegrate::generate_format(_("You"));
		symbols["coin"] = tintegrate::generate_format(preferences::coin(), "yellow");
		symbols["score"] = tintegrate::generate_format(preferences::score(), "yellow");
		strstr << vgettext("wesnoth-lib", "$username has $coin coin and $score score", symbols) << "\n";
	}
	tscroll_label* tip = find_widget<tscroll_label>(&window, "tip", false, true);
	tip->set_label(strstr.str());
}

void tinapp_purchase::refresh_list(bool clear_items, bool set_purchase)
{
	if (clear_items) {
		items_.clear();
		for (std::map<int, std::string>::const_iterator it = game_config::inapp_items.begin(); it != game_config::inapp_items.end(); ++ it) {
#if (defined(__APPLE__) && TARGET_OS_IPHONE)
			if (it->first >= game_config::transaction_type_8coin) {
				break;
			}
#endif
			items_.push_back(tinapp_item(it->first, it->second));
		}
	}
	
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
		strstr << tintegrate::generate_format(it->remark, "green");
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("remark", list_item));

		strstr.str("");
		if (it->index == game_config::transaction_type_vip) {
#if (defined(__APPLE__) && TARGET_OS_IPHONE)
			strstr << "USD $2.99";
#else
			strstr << "RMB 15";
#endif
		} else if (it->index == game_config::transaction_type_2coin) {
#if (defined(__APPLE__) && TARGET_OS_IPHONE)
			strstr << "USD $4.99";
#else
			strstr << "RMB 25";
#endif
		} else if (it->index == game_config::transaction_type_8coin) {
			strstr << "RMB 100";

		} else if (it->index == game_config::transaction_type_45coin) {
			strstr << "RMB 500";

		} else if (it->index == game_config::transaction_type_100coin) {
			strstr << "RMB 1000";

		}
		strstr << _("coin^Unit");

		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("cost", list_item));

		list_->add_row(list_item_item);
		tgrid* grid = list_->get_row_grid(list_->get_item_count() - 1);
		twidget* widget = grid->find("purchase", false);
		widget->set_visible(it->purchased? twidget::VISIBLE: twidget::INVISIBLE);
	}

	if (ing_item_ != -1) {
		list_->select_row(ing_item_);
	}

	if (set_purchase) {
		purchase_->set_active(!browse_ && !items_[list_->get_selected_row()].purchased);
	}
	refresh_tip(*list_->get_window(), items_[list_->get_selected_row()]);
}

void tinapp_purchase::pre_show(CVideo& /*video*/, twindow& window)
{
	std::stringstream strstr;
	if (browse_) {
		utils::string_map symbols;
		symbols["platforms"] = tintegrate::generate_format("iOS", "green");

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

	refresh_list(true);
	list_->set_callback_value_change(dialog_callback<tinapp_purchase, &tinapp_purchase::item_selected>);

	// current no non-consumable in-purchase, don't use restore
	purchase_->set_active(!browse_);
	bool no_nonconsumable = true;
	for (std::vector<tinapp_item>::const_iterator it = items_.begin(); it != items_.end(); ++ it) {
		if (!it->consumable) {
			no_nonconsumable = false;
			break;
		}
	}
	if (no_nonconsumable) {
		restore_->set_visible(twidget::INVISIBLE);
	}

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

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "quit", false)
		, boost::bind(
			&tinapp_purchase::quit
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
#if (defined(__APPLE__) && TARGET_OS_IPHONE)
	int res = find_widget<tlistbox>(&window, "item_list", false).get_selected_row();
	tinapp_item& item = items_[res];

	startAppPayment(item.id.c_str());

	ing_item_ = res;
	purchase_status(false);
#else
	tlistbox& list = find_widget<tlistbox>(&window, "item_list", false);
	int cursel = list.get_selected_row();

	gui2::talipay dlg(disp_, heros_, "", cursel);
	dlg.show(disp_.video());
	if (dlg.get_retval() != gui2::twindow::OK) {
		return;
	}
	refresh_list(true, true);
#endif
}

void tinapp_purchase::quit(twindow& window)
{
	if (!ing_timer_) {
		window.set_retval(twindow::OK);
		return;
	}
	
	std::string message = _("System is processing inapp-purchase request, and force to quit maybe result data not integrated. Do you want to Quit?");
	int res = gui2::show_message(disp_.video(), "", message, gui2::tmessage::yes_no_buttons);
	if (res != gui2::twindow::OK) {
		return;
	}
	window.set_retval(twindow::OK);
}

void tinapp_purchase::timer_handler()
{
	std::stringstream strstr;
	utils::string_map symbols;
	if (ing_item_ >= 0) {
		symbols["name"] = tintegrate::generate_format(items_[ing_item_].name, "blue");
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
	if (!exit) {
        restore_->set_active(false);
		purchase_->set_active(false);

		ing_ticks_ = 0;
		ing_timer_ = add_timer(50, boost::bind(&tinapp_purchase::timer_handler, this), true);
	} else {
		gui2::remove_timer(ing_timer_);

        restore_->set_active(true);
		purchase_->set_active(!items_[list_->get_selected_row()].purchased);
		ing_timer_ = 0;

		ing_item_ = -1;
	}
}

} // namespace gui2
