#define GETTEXT_DOMAIN "wesnoth-maker"

#include "global.hpp"
#include "game_config.hpp"
#include "loadscreen.hpp"
#include "DlgCoreProc.hpp"
#include "gettext.hpp"
#include "serialization/binary_or_text.hpp"
#include "stdafx.h"
#include <windowsx.h>
#include <string.h>

#include "resource.h"

#include "xfunc.h"
#include "win32x.h"
#include "struct.h"
#include "rectangle.hpp"

#include "map.hpp"
#include <boost/foreach.hpp>

namespace ns {
	int clicked_book;
	int action_book;

	HIMAGELIST himl_book;
	int iico_book_book;
	int iico_book_section;
	int iico_book_topic;
}

namespace ns_tree {
struct tcookie {
	tcookie(help::section* s, help::topic* t, HTREEITEM node)
		: s(s)
		, t(t)
		, node(node)
	{}

	help::section* s;
	help::topic* t;
	HTREEITEM node;
};

int increase_id;
std::map<int, tcookie> cookies;
HTREEITEM htvroot_book;
tcookie* clicked_cookie;

bool is_duplicate_id(const std::string& desire_id, const void* exclude)
{
	for (std::map<int, tcookie>::const_iterator it = cookies.begin(); it != cookies.end(); ++ it) {
		const tcookie& cookie = it->second;
		if (cookie.s) {
			if (exclude && cookie.s == exclude) {
				continue;
			}
			if (cookie.s->id == desire_id) {
				return true;
			}
		} else {
			if (exclude && cookie.t == exclude) {
				continue;
			}
			if (cookie.t->id == desire_id) {
				return true;
			}
		}
	}
	return false;
}

tcookie* rfind(const void* target)
{
	for (std::map<int, tcookie>::iterator it = cookies.begin(); it != cookies.end(); ++ it) {
		tcookie& cookie = it->second;
		if (cookie.s == target || cookie.t == target) {
			return &cookie;
		}
	}
	return NULL;
}

std::string form_section_str(const std::string& id, const t_string& title)
{
	std::stringstream strstr;

	strstr << title;
	strstr << "(" << id << ")";
	return strstr.str();
}

std::string form_topic_str(const std::string& id, const t_string& title)
{
	std::stringstream strstr;

	strstr << title;
	strstr << "(" << id << ")";
	if (help::is_section_topic(id)) {
		strstr << "[hide]";
	}
	return strstr.str();
}

void section_2_tv_internal(HWND hctl, HTREEITEM htvroot, help::section& parent, help::section& toplevel)
{
	std::stringstream strstr;
	char text[_MAX_PATH];

	for (std::vector<help::section *>::iterator it = parent.sections.begin(); it != parent.sections.end(); ++ it) {
		help::section& sec = **it;

		strstr.str("");
		strstr << form_section_str(sec.id, sec.title);

		HTREEITEM htvi = TreeView_AddLeaf(hctl, htvroot);
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		int cChildren = !sec.sections.empty() || !sec.topics.empty()? 1: 0;
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			increase_id, ns::iico_book_section, ns::iico_book_section, cChildren, text);

		cookies.insert(std::make_pair(increase_id ++, tcookie(help::find_section(toplevel, sec.id), NULL, htvi)));

		if (!sec.sections.empty() || !sec.topics.empty()) {
			section_2_tv_internal(hctl, htvi, sec, toplevel);
		}
	}

	for (std::list<help::topic>::iterator it = parent.topics.begin(); it != parent.topics.end(); ++ it) {
		help::topic& t = *it;

		strstr.str("");
		strstr << form_topic_str(t.id, t.title);

		HTREEITEM htvi = TreeView_AddLeaf(hctl, htvroot);
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			increase_id, ns::iico_book_topic, ns::iico_book_topic, 0, text);

		cookies.insert(std::make_pair(increase_id ++, tcookie(NULL, &t, htvi)));
	}
}

void section_2_tv(HWND hctl, HTREEITEM htvroot, help::section& toplevel)
{
	increase_id = 0;
	cookies.clear();
	section_2_tv_internal(hctl, htvroot, toplevel, toplevel);
}

void topicedit_save_bh(tbook& book, HWND hdlgP, const tcookie& cookie)
{
	const help::topic& t = *cookie.t;

	HWND hctl = GetDlgItem(hdlgP, IDC_TV_BOOK_EXPLORER);
	TreeView_SetItem1(hctl, cookie.node, TVIF_TEXT, 0, 0, 0, 0, utf8_2_ansi(form_topic_str(t.id, t.title)));

	if (help::is_section_topic(t.id)) {
		help::section* sec = help::find_section(book.toplevel_, help::extract_section_topic(t.id));
		tcookie& ret = *rfind(sec);
		TreeView_SetItem1(hctl, ret.node, TVIF_TEXT, 0, 0, 0, 0, utf8_2_ansi(form_section_str(ret.s->id, ret.s->title)));
	}
}

void rename_save_bh(HWND hdlgP, const tcookie& cookie)
{
	HWND hctl = GetDlgItem(hdlgP, IDC_TV_BOOK_EXPLORER);
	if (cookie.s) {
		TreeView_SetItem1(hctl, cookie.node, TVIF_TEXT, 0, 0, 0, 0, utf8_2_ansi(form_topic_str(cookie.s->id, cookie.s->title)));
		help::topic* t = help::find_topic(ns::core.current_book().toplevel_, help::form_section_topic(cookie.s->id));
		tcookie& ret = *rfind(t);
		TreeView_SetItem1(hctl, ret.node, TVIF_TEXT, 0, 0, 0, 0, utf8_2_ansi(form_topic_str(ret.t->id, ret.t->title)));
	} else {
		TreeView_SetItem1(hctl, cookie.node, TVIF_TEXT, 0, 0, 0, 0, utf8_2_ansi(form_topic_str(cookie.t->id, cookie.t->title)));
	}
}

std::string next_new_section_id()
{
	const std::string default_section_id = "__new_section";
	std::string id = default_section_id;
	do {
		if (!is_duplicate_id(id, NULL)) {
			return id;
		}
		id = std::string("_") + id;
	} while (true);

	return null_str;
}

std::string next_new_topic_id()
{
	const std::string default_topic_id = "__new_topic";
	std::string id = default_topic_id;
	do {
		if (!is_duplicate_id(id, NULL)) {
			return id;
		}
		id = std::string("_") + id;
	} while (true);

	return null_str;
}

}

void tbook::from_config(const config& cfg)
{
	id_ = cfg["id"].str();

	std::vector<t_string_base::trans_str> trans = cfg["title"].t_str().valuex();
	for (std::vector<t_string_base::trans_str>::const_iterator ti = trans.begin(); ti != trans.end(); ti ++) {
		// only support one textdomain
		textdomain_ = ti->td;
		title_ = ti->str;
		break;
	}
	textdomain_ = std::string(PACKAGE) + "-" + id_;

	book_from_cfg_ = *this;
}

void tbook::from_ui_topic_edit(HWND hdlgP)
{
	char text[_MAX_PATH];
	help::topic* t = ns_tree::clicked_cookie->t;

	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_TOPICEDIT_TITLE_TEXTDOMAIN), text, sizeof(text) / sizeof(text[0]));
	std::string textdomain = text[0]? text: textdomain_;
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_TOPICEDIT_TITLE_MSGID), text, sizeof(text) / sizeof(text[0]));
	t->title = t_string(text, textdomain);
	if (help::is_section_topic(t->id)) {
		help::section* sec = help::find_section(toplevel_, help::extract_section_topic(t->id));
		sec->title = t->title;
	}

	{
		HWND hctl = GetDlgItem(hdlgP, IDC_ET_TOPICEDIT_TEXT_MSGID);
		int len = Edit_GetTextLength(hctl);
		tmalloc_free_lock lock(len + 2);
		Edit_GetText(hctl, lock.buf, lock.len);

		t->text.set_parsed_text(t_string(get_rid_of_return(lock.buf), textdomain_));
	}
}

void tbook::from_ui_rename(HWND hdlgP)
{
	char text[_MAX_PATH];
	ns_tree::tcookie& cookie = *ns_tree::clicked_cookie;

	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_RENAME_NAME), text, sizeof(text) / sizeof(text[0]));
	std::string name = text;
	std::transform(name.begin(), name.end(), name.begin(), std::tolower);

	if (cookie.s) {
		help::topic* t = help::find_topic(toplevel_, help::form_section_topic(cookie.s->id));
		t->id = help::form_section_topic(name);
		cookie.s->id = name;
	} else {
		cookie.t->id = name;
	}
}

void tbook::update_to_ui_topic_edit(HWND hdlgP) const
{
	const help::topic* t = ns_tree::clicked_cookie->t;
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_TOPICEDIT_ID), t->id.c_str());

	std::vector<t_string_base::trans_str> trans = t->title.valuex();
	for (std::vector<t_string_base::trans_str>::const_iterator ti = trans.begin(); ti != trans.end(); ti ++) {
		// only support one textdomain
		const std::string& textdomain = ti->td;
		const std::string& msgid = ti->str;

		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_TOPICEDIT_TITLE_MSGID), msgid.c_str());
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_TOPICEDIT_TITLE), dgettext_2_ansi(textdomain.c_str(), msgid.c_str()));
		if (textdomain != textdomain_) {
			Edit_SetText(GetDlgItem(hdlgP, IDC_ET_TOPICEDIT_TITLE_TEXTDOMAIN), textdomain.c_str());
		} else {
			Edit_SetText(GetDlgItem(hdlgP, IDC_ET_TOPICEDIT_TITLE_TEXTDOMAIN), null_str.c_str());
		}
		break;
	}

	trans = t->text.parsed_text().valuex();
	for (std::vector<t_string_base::trans_str>::const_iterator ti = trans.begin(); ti != trans.end(); ti ++) {
		// only support one textdomain
		const std::string& textdomain = ti->td;
		const std::string& msgid = ti->str;

		std::string txt = insert_return(msgid);
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_TOPICEDIT_TEXT_MSGID), txt.c_str());
		
		if (!textdomain.empty()) {
			txt = insert_return(dgettext_2_ansi(textdomain.c_str(), msgid.c_str()));
		}
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_TOPICEDIT_TEXT), txt.c_str());
		break;
	}
}

std::string tbook::generate()
{
	std::stringstream strstr;

	strstr << "#textdomain " << textdomain_ << "\n";
	strstr << "[textdomain]\n";
	strstr << "\tname = \"" << textdomain_ << "\"\n";
	strstr << "[/textdomain]\n";
	strstr << "\n";

	strstr << "[book]\n";
	strstr << "\tid = " << id_ << "\n";
	strstr << "\ttitle = _ \"" << title_ << "\"\n";
	strstr << "\n";

	toplevel_.generate(strstr, "\t", textdomain_, toplevel_);

	strstr << "\n";
	strstr << "[/book]\n";

	return strstr.str();
}

std::string book_pot(const std::string& id, bool absolute) 
{
	std::stringstream strstr;
	std::string wesnoth_id = std::string(PACKAGE) + "-" + id;
	if (absolute) {
		strstr << game_config::path << "\\po\\" << wesnoth_id << "\\" << wesnoth_id << ".pot";
	} else {
		strstr << "po/" << wesnoth_id << "/" << wesnoth_id << ".pot";
	}
	return strstr.str();
}

void tbook::generate_pot() const
{
	std::stringstream strstr;
	uint32_t bytertd;
	posix_file_t fp;
	const std::string fname = book_pot(id_, true);

	MakeDirectory(fname.substr(0, fname.rfind('\\')));
	posix_fopen(fname.c_str(), GENERIC_WRITE, CREATE_ALWAYS, fp);
	if (fp == INVALID_FILE) {
		strstr << "Write file(" << fname << ") fail!";
		MessageBox(NULL, strstr.str().c_str(), "Error", MB_OK); 
		return;
	}
	
	strstr << "msgid \"\"\n";
	strstr << "msgstr \"\"\n";
	strstr << "\"Project-Id-Version: kingdom-" << id_ << "\\n\"\n";
	strstr << "\"POT-Creation-Date: \\n\"\n";
	strstr << "\"PO-Revision-Date: \\n\"\n";
	strstr << "\"Last-Translator: ancientcc <ancientcc@leagor.com>\\n\"\n";
	strstr << "\"Language-Team: \\n\"\n";
	strstr << "\"MIME-Version: 1.0\\n\"\n";
	strstr << "\"Content-Type: text/plain; charset=UTF-8\\n\"\n";
	strstr << "\"Content-Transfer-Encoding: 8bit\\n\"\n";
	strstr << "\n";

	std::set<std::string> msgids;
	msgids.insert(title_);
	toplevel_.generate_pot(msgids, textdomain_);

	for (std::set<std::string>::const_iterator it = msgids.begin(); it != msgids.end(); ++ it) {
		strstr << "\n";
		strstr << "msgid \"" << *it << "\"\n";
		strstr << "msgstr \"\"\n";
	}

	posix_fwrite(fp, strstr.str().c_str(), strstr.str().length(), bytertd);
	posix_fclose(fp);
}

void tbook::switch_to(HWND hdlgP)
{
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_BOOK_TEXTDOMAIN), textdomain_.c_str());
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_BOOK_TITLE_MSGID), title_.c_str());
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_BOOK_TITLE), dgettext_2_ansi(textdomain_.c_str(), title_.c_str()));

	char text[_MAX_PATH];
	std::stringstream strstr;

	//
	// book to treeview
	//
	help::clear_book();
	help::init_book(&editor_config::data_cfg, NULL, true);
	if (toplevel_.is_null()) {
		help::generate_contents(id_, toplevel_);
		// first read,
		book_from_cfg_.toplevel_ = toplevel_;
	}

	HWND hctl = GetDlgItem(hdlgP, IDC_TV_BOOK_EXPLORER);
	// 1. clear treeview
	TreeView_DeleteAllItems(hctl);

	// 2. fill content
	ns_tree::htvroot_book = TreeView_AddLeaf(hctl, TVI_ROOT);
	strstr.str("");
	strstr << dgettext_2_ansi(textdomain_.c_str(), title_.c_str());
	strcpy(text, strstr.str().c_str());
	// must set TVIF_CHILDREN
	TreeView_SetItem1(hctl, ns_tree::htvroot_book, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		0, ns::iico_book_book, ns::iico_book_book, 1, text);
	ns_tree::section_2_tv(hctl, ns_tree::htvroot_book, toplevel_);

	TreeView_Walk(hctl, TVI_ROOT, TRUE, cb_treeview_walk_expand, NULL, FALSE);
	scroll::treeview_scroll_to(hctl);
}

bool tbook::dirty() const
{
	return *this != book_from_cfg_;
}

tbook& tcore::current_book()
{
	return books.find(current_book_)->second;
}

void cb_treeview_update_scroll_book(HWND htv, HTREEITEM htvi, TVITEMEX& tvi, void* ctx)
{
	ns::core.update_to_ui_book(GetParent(htv));
}

void tcore::update_to_ui_book(HWND hdlgP)
{
	std::stringstream strstr;

	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_BOOKS);
	ComboBox_ResetContent(hctl);
	for (std::map<std::string, tbook>::const_iterator it = books.begin(); it != books.end(); ++ it) {
		const tbook& book = it->second;
		strstr.str("");
		strstr << dgettext(book.textdomain_.c_str(), book.title_.c_str());
		strstr << "(id=" << book.id_ << ")";
		ComboBox_AddString(hctl, utf8_2_ansi(strstr.str().c_str()));

		if (current_book_.empty()) {
			current_book_ = book.id_;
		}
	}
	ComboBox_SetCurSel(hctl, 0);
	if (!books.empty()) {
		current_book().switch_to(hdlgP);
	}
}

bool tcore::book_dirty() const
{
	for (std::map<std::string, tbook>::const_iterator it = books.begin(); it != books.end(); ++ it) {
		if (it->second.dirty()) {
			return true;
		}
	}
	return false;
}

BOOL On_DlgBookInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	HWND hwndParent = GetParent(hdlgP); 
    DLGHDR *pHdr = (DLGHDR *) GetWindowLong(hwndParent, GWL_USERDATA);
    SetWindowPos(hdlgP, HWND_TOP, pHdr->rcDisplay.left, pHdr->rcDisplay.top, 0, 0, SWP_NOSIZE); 

	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_DOMAIN), utf8_2_ansi(_("Domain")));

	ns::himl_book = ImageList_Create(15, 15, ILC_COLOR24, 7, 0);
	ImageList_SetBkColor(ns::himl_book, RGB(236, 233, 216));

	HICON hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_EVT_EVENT));
	ns::iico_book_book = ImageList_AddIcon(ns::himl_book, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_PARTICULAR));
	ns::iico_book_section = ImageList_AddIcon(ns::himl_book, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_EVT_FILTER));
	ns::iico_book_topic = ImageList_AddIcon(ns::himl_book, hicon);

	return FALSE;
}

BOOL On_DlgTopicEditInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	utils::string_map symbols;
	std::stringstream strstr;
	strstr << _("Edit topic");
	SetWindowText(hdlgP, utf8_2_ansi(strstr.str().c_str()));

	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_DOMAIN), utf8_2_ansi(_("Domain")));
	symbols["key"] = _("Domain");
	symbols["textdomain"] = ns::core.current_book().textdomain_;
	strstr.str("");
	strstr << vgettext2("If title isn't from $textdomain, set \"$key\"", symbols);
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_TIP), utf8_2_ansi(strstr.str().c_str()));
	
	ns::core.current_book().update_to_ui_topic_edit(hdlgP);

	return FALSE;
}

void OnTopicEditEt(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	char text[_MAX_PATH];
	std::string msgid, textdomain;
	std::stringstream strstr;

	if (codeNotify != EN_CHANGE) {
		return;
	}

	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_TOPICEDIT_TITLE_MSGID), text, sizeof(text) / sizeof(text[0]));
	msgid = text;
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_TOPICEDIT_TITLE_TEXTDOMAIN), text, sizeof(text) / sizeof(text[0]));
	textdomain = text;

	if (id == IDC_ET_TOPICEDIT_TITLE_TEXTDOMAIN) {
		Edit_GetText(hwndCtrl, text, sizeof(text) / sizeof(text[0]));
		textdomain = text;

		std::transform(textdomain.begin(), textdomain.end(), textdomain.begin(), std::tolower);
		if (!textdomain.empty() && !isvalid_id(textdomain)) {
			Edit_SetText(GetDlgItem(hdlgP, IDC_ET_TOPICEDIT_TITLE_TEXTDOMAINSTATUS), utf8_2_ansi(_("Invalid string")));
			return;
		}
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_TOPICEDIT_TITLE_TEXTDOMAINSTATUS), "");

	} else if (id != IDC_ET_TOPICEDIT_TITLE_MSGID) {
		return;
	}

	if (textdomain.empty()) {
		textdomain = ns::core.current_book().textdomain_;
	}
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_TOPICEDIT_TITLE), utf8_2_ansi(dgettext(textdomain.c_str(), msgid.c_str())));
	
	return;
}

void OnTopicEditEt2(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	if (codeNotify != EN_CHANGE) {
		return;
	}

	if (id != IDC_ET_TOPICEDIT_TEXT_MSGID) {
		return;
	}

	int len = Edit_GetTextLength(hwndCtrl);
	tmalloc_free_lock lock(len + 2);
	Edit_GetText(hwndCtrl, lock.buf, lock.len);

	const std::string& msgid = get_rid_of_return(lock.buf);
	const std::string& textdomain = ns::core.current_book().textdomain_;
	const std::string& to_text_ctrl = insert_return(dgettext(textdomain.c_str(), msgid.c_str()));
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_TOPICEDIT_TEXT), utf8_2_ansi(to_text_ctrl));
	
	return;
}

void On_DlgTopicEditCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;

	switch (id) {
	case IDC_ET_TOPICEDIT_TITLE_MSGID:
	case IDC_ET_TOPICEDIT_TITLE_TEXTDOMAIN:
		OnTopicEditEt(hdlgP, id, hwndCtrl, codeNotify);
		break;

	case IDC_ET_TOPICEDIT_TEXT_MSGID:
		OnTopicEditEt2(hdlgP, id, hwndCtrl, codeNotify);
		break;			

	case IDOK:
		changed = TRUE;
		ns::core.current_book().from_ui_topic_edit(hdlgP);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

BOOL CALLBACK DlgTopicEditProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message) {
	case WM_INITDIALOG:
		return On_DlgTopicEditInitDialog(hDlg, (HWND)(wParam), lParam);
	HANDLE_MSG(hDlg, WM_COMMAND, On_DlgTopicEditCommand);
	HANDLE_MSG(hDlg, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	}
	
	return FALSE;
}

void OnBookAddBt(HWND hdlgP, bool section)
{
	const ns_tree::tcookie& cookie = *ns_tree::clicked_cookie;
	tbook& book = ns::core.current_book();
	std::pair<help::section*, int> parent = help::find_parent(book.toplevel_, cookie.s? cookie.s->id: cookie.t->id);
	if (cookie.s && section) {
		help::section s;
		s.id = ns_tree::next_new_section_id();
		s.title = t_string("New section");
		s.level = cookie.s->level;
		s.add_topic(help::topic(s.title, help::form_section_topic(s.id)));
		parent.first->add_section(s, parent.second);
	} else {
		help::topic t;
		t.id = ns_tree::next_new_topic_id();
		t.title = t_string("New topic");
		if (cookie.s) {
			cookie.s->add_topic(t, 0);
		} else {
			parent.first->add_topic(t, parent.second);
		}
	}

	scroll::treeview_update_scroll(GetDlgItem(hdlgP, IDC_TV_BOOK_EXPLORER), cb_treeview_update_scroll_book, NULL);
	ns::core.set_dirty(tcore::BIT_BOOK, ns::core.book_dirty());
}

void OnBookDelBt(HWND hdlgP)
{
	std::stringstream strstr;
	utils::string_map symbols;

	const ns_tree::tcookie& cookie = *ns_tree::clicked_cookie;
	strstr.str("");
	if (cookie.s) {
		symbols["name"] = ns_tree::form_section_str(cookie.s->id, cookie.s->title);
		strstr << utf8_2_ansi(vgettext2("Do you want to delete section: $name?", symbols));
	} else {
		symbols["name"] = ns_tree::form_topic_str(cookie.t->id, cookie.t->title);
		strstr << utf8_2_ansi(vgettext2("Do you want to delete topic: $name?", symbols));
	}

	std::string caption = utf8_2_ansi(_("Confirm delete"));
	int retval = MessageBox(gdmgr._hwnd_main, strstr.str().c_str(), caption.c_str(), MB_YESNO);
	if (retval == IDNO) {
		return;
	}

	tbook& book = ns::core.current_book();
	help::section* parent = help::find_parent(book.toplevel_, cookie.s? cookie.s->id: cookie.t->id).first;
	if (cookie.s) {
		parent->erase_section(*cookie.s);
	} else {
		parent->erase_topic(*cookie.t);
	}
	
	scroll::treeview_update_scroll(GetDlgItem(hdlgP, IDC_TV_BOOK_EXPLORER), cb_treeview_update_scroll_book, NULL);
	ns::core.set_dirty(tcore::BIT_BOOK, ns::core.book_dirty());
}

BOOL On_DlgRenameInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	utils::string_map symbols;
	std::stringstream strstr;
	strstr << _("Rename ID");
	SetWindowText(hdlgP, utf8_2_ansi(strstr.str().c_str()));

	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_DOMAIN), utf8_2_ansi(_("Domain")));
	symbols["key"] = _("Domain");
	symbols["textdomain"] = ns::core.current_book().textdomain_;
	strstr.str("");
	strstr << vgettext2("If title isn't from $textdomain, set \"$key\"", symbols);
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_TIP), utf8_2_ansi(strstr.str().c_str()));
	
	const ns_tree::tcookie& cookie = *ns_tree::clicked_cookie;
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_RENAME_NAME), cookie.s? cookie.s->id.c_str(): cookie.t->id.c_str());

	return FALSE;
}

void OnRenameEt(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	char text[_MAX_PATH];
	std::string name;
	std::stringstream err;

	if (codeNotify != EN_CHANGE) {
		return;
	}

	const tbook& book = ns::core.current_book();
	const ns_tree::tcookie& cookie = *ns_tree::clicked_cookie;
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_RENAME_NAME), text, sizeof(text) / sizeof(text[0]));
	name = text;

	if (id == IDC_ET_RENAME_NAME) {
		std::transform(name.begin(), name.end(), name.begin(), std::tolower);
		if (!isvalid_id(name) || help::is_section_topic(name)) {
			err << _("Invalid string");
		} else if (help::is_reserve_section(name) || name == book.id_) {
			err << _("Reserve id");
		} else if (ns_tree::is_duplicate_id(name, cookie.s?  reinterpret_cast<void*>(cookie.s): reinterpret_cast<void*>(cookie.t))) {
			err << _("Duplicate id");
		}

		if (!err.str().empty()) {
			Edit_SetText(GetDlgItem(hdlgP, IDC_ET_RENAME_STATUS), utf8_2_ansi(err.str()));
			Button_Enable(GetDlgItem(hdlgP, IDOK), false);
			return;
		}
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_RENAME_STATUS), "");
		Button_Enable(GetDlgItem(hdlgP, IDOK), true);

	} else {
		return;
	}
}

void On_DlgRenameCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;

	switch (id) {
	case IDC_ET_RENAME_NAME:
		OnRenameEt(hdlgP, id, hwndCtrl, codeNotify);
		break;

	case IDOK:
		changed = TRUE;
		ns::core.current_book().from_ui_rename(hdlgP);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

BOOL CALLBACK DlgRenameProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message) {
	case WM_INITDIALOG:
		return On_DlgRenameInitDialog(hDlg, (HWND)(wParam), lParam);
	HANDLE_MSG(hDlg, WM_COMMAND, On_DlgRenameCommand);
	HANDLE_MSG(hDlg, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	}
	
	return FALSE;
}

void OnBookRenameBt(HWND hdlgP)
{
	RECT rcBtn;
	LPARAM lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_TV_BOOK_EXPLORER), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_RENAME), hdlgP, DlgRenameProc, lParam)) {
		ns_tree::rename_save_bh(hdlgP, *ns_tree::clicked_cookie);
		ns::core.set_dirty(tcore::BIT_BOOK, ns::core.book_dirty());
	}

	return;
}

void OnBookMoveBt(HWND hdlgP, bool up)
{
	const ns_tree::tcookie& cookie = *ns_tree::clicked_cookie;
	tbook& book = ns::core.current_book();
	std::pair<help::section*, int> parent = help::find_parent(book.toplevel_, cookie.s? cookie.s->id: cookie.t->id);
	if (cookie.s) {
		parent.first->move(*cookie.s, up);
	} else {
		parent.first->move(*cookie.t, up);
	}

	scroll::treeview_update_scroll(GetDlgItem(hdlgP, IDC_TV_BOOK_EXPLORER), cb_treeview_update_scroll_book, NULL);
	ns::core.set_dirty(tcore::BIT_BOOK, ns::core.book_dirty());
}

void OnBookEditBt(HWND hdlgP)
{
	RECT rcBtn;
	LPARAM lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_TV_BOOK_EXPLORER), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	ns::action_book = ma_edit;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_TOPICEDIT), hdlgP, DlgTopicEditProc, lParam)) {
		ns_tree::topicedit_save_bh(ns::core.current_book(), hdlgP, *ns_tree::clicked_cookie);
		ns::core.set_dirty(tcore::BIT_BOOK, ns::core.book_dirty());
	}

	return;
}

std::string book_id_from_title(const std::string& title)
{
	size_t pos = title.find("(id=");
	std::string id = title.substr(pos + 4);
	if (id.at(id.size() - 1) == ')') {
		id.erase(id.size() - 1);
	}
	return id;
}

void OnBookCmb(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	char text[_MAX_PATH];

	if (codeNotify != CBN_SELCHANGE) {
		return;
	}

	ComboBox_GetText(hwndCtrl, text, sizeof(text) / sizeof(text[0]));
	ns::core.current_book_ = book_id_from_title(text);
	ns::core.current_book().switch_to(hdlgP);
	
	return;
}

void OnBookEt(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	char text[_MAX_PATH];
	std::string msgid, textdomain;
	std::stringstream strstr;

	if (codeNotify != EN_CHANGE) {
		return;
	}

	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_BOOK_TITLE_MSGID), text, sizeof(text) / sizeof(text[0]));
	msgid = text;
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_BOOK_TEXTDOMAIN), text, sizeof(text) / sizeof(text[0]));
	textdomain = text;

	if (id == IDC_ET_BOOK_TEXTDOMAIN) {
		std::transform(textdomain.begin(), textdomain.end(), textdomain.begin(), std::tolower);
		if (!isvalid_id(textdomain)) {
			Edit_SetText(GetDlgItem(hdlgP, IDC_ET_BOOK_TEXTDOMAINSTATUS), utf8_2_ansi(_("Invalid string")));
			return;
		}
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_BOOK_TEXTDOMAINSTATUS), "");
		ns::core.current_book().textdomain_ = textdomain;

	} else if (id == IDC_ET_BOOK_TITLE_MSGID) {
		ns::core.current_book().title_ = msgid;

	} else {
		return;
	}

	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_BOOK_TITLE), utf8_2_ansi(dgettext(textdomain.c_str(), msgid.c_str())));

	ns::core.set_dirty(tcore::BIT_BOOK, ns::core.book_dirty());
	
	return;
}

void On_DlgBookCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	switch (id) {
	case IDC_CMB_BOOKS:
		OnBookCmb(hdlgP, id, hwndCtrl, codeNotify);
		break;

	case IDC_ET_BOOK_TEXTDOMAIN:
	case IDC_ET_BOOK_TITLE_MSGID:
		OnBookEt(hdlgP, id, hwndCtrl, codeNotify);
		break;

	case IDM_ADD_SECTION:
	case IDM_ADD_TOPIC:
		OnBookAddBt(hdlgP, id == IDM_ADD_SECTION);
		break;
	case IDM_EDIT:
		OnBookEditBt(hdlgP);
		break;
	case IDM_DELETE:
		OnBookDelBt(hdlgP);
		break;
	case IDM_RENAME:
		OnBookRenameBt(hdlgP);
		break;
	case IDM_UP:
	case IDM_DOWN:
		OnBookMoveBt(hdlgP, id == IDM_UP);
		break;
	case IDM_GENERATE_ITEM2:
		ns::core.current_book().generate_pot();
		break;
	}

	return;
}

void book_notify_handler_rclick(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	LPNMTREEVIEW			lpnmitem;
	HTREEITEM				htvi;
	TVITEMEX				tvi;
	POINT					point;
	utils::string_map symbols;
	std::stringstream strstr;

	if (lpNMHdr->idFrom != IDC_TV_BOOK_EXPLORER) {
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

	if (lpNMHdr->idFrom == IDC_TV_BOOK_EXPLORER) {
		ns_tree::clicked_cookie = &(ns_tree::cookies.find(tvi.lParam)->second);
		ns_tree::tcookie& cookie = *ns_tree::clicked_cookie;
		std::pair<help::section*, int> parent = help::find_parent(ns::core.current_book().toplevel_, cookie.s? cookie.s->id: cookie.t->id);

		HMENU hpopup_new = CreatePopupMenu();
		AppendMenu(hpopup_new, MF_STRING, IDM_ADD_SECTION, utf8_2_ansi(_("book^Section")));
		AppendMenu(hpopup_new, MF_STRING, IDM_ADD_TOPIC, utf8_2_ansi(_("book^Topic")));

		HMENU hpopup_move = CreatePopupMenu();
		AppendMenu(hpopup_move, MF_STRING, IDM_UP, utf8_2_ansi(_("Up")));
		AppendMenu(hpopup_move, MF_STRING, IDM_DOWN, utf8_2_ansi(_("Down")));

		HMENU hpopup_book = CreatePopupMenu();
		AppendMenu(hpopup_book, MF_POPUP, (UINT_PTR)(hpopup_new), utf8_2_ansi(_("Add")));
		AppendMenu(hpopup_book, MF_STRING, IDM_EDIT, utf8_2_ansi(_("Edit...")));
		AppendMenu(hpopup_book, MF_STRING, IDM_DELETE, utf8_2_ansi(_("Delete...")));
		AppendMenu(hpopup_book, MF_SEPARATOR, 0, NULL);
		strstr.str("");
		strstr << _("Rename ID") << "...";
		AppendMenu(hpopup_book, MF_STRING, IDM_RENAME, utf8_2_ansi(strstr.str()));
		AppendMenu(hpopup_book, MF_SEPARATOR, 0, NULL);
		AppendMenu(hpopup_book, MF_POPUP, (UINT_PTR)(hpopup_move), utf8_2_ansi(_("Move")));
		AppendMenu(hpopup_book, MF_SEPARATOR, 0, NULL);
		
		strstr.str("");
		symbols["textdomain"] = book_pot(ns::core.current_book().id_, false);
		strstr << vgettext2("Gegenerate $textdomain", symbols);
		AppendMenu(hpopup_book, MF_STRING, IDM_GENERATE_ITEM2, utf8_2_ansi(strstr.str()));

		if (!htvi || htvi == ns_tree::htvroot_book) {
			EnableMenuItem(hpopup_book, IDM_EDIT, MF_BYCOMMAND | MF_GRAYED);
			EnableMenuItem(hpopup_book, IDM_DELETE, MF_BYCOMMAND | MF_GRAYED);
			EnableMenuItem(hpopup_book, IDM_RENAME, MF_BYCOMMAND | MF_GRAYED);
			EnableMenuItem(hpopup_book, (UINT_PTR)hpopup_move, MF_BYCOMMAND | MF_GRAYED);

		} else if (ns_tree::clicked_cookie->s) {
			// section
			EnableMenuItem(hpopup_book, IDM_EDIT, MF_BYCOMMAND | MF_GRAYED);
			if (!parent.second) {
				EnableMenuItem(hpopup_book, IDM_UP, MF_BYCOMMAND | MF_GRAYED);
			} else if (parent.second + 1 == parent.first->sections.size()) {
				EnableMenuItem(hpopup_book, IDM_DOWN, MF_BYCOMMAND | MF_GRAYED);
			}

		} else {
			// topic
			EnableMenuItem(hpopup_new, IDM_ADD_SECTION, MF_BYCOMMAND | MF_GRAYED);
			if (help::is_section_topic(ns_tree::clicked_cookie->t->id)) {
				EnableMenuItem(hpopup_book, IDM_DELETE, MF_BYCOMMAND | MF_GRAYED);
				EnableMenuItem(hpopup_book, IDM_RENAME, MF_BYCOMMAND | MF_GRAYED);
			}
			if (parent.second <= 1) {
				EnableMenuItem(hpopup_book, IDM_UP, MF_BYCOMMAND | MF_GRAYED);
				if (!parent.second) {
					EnableMenuItem(hpopup_book, IDM_DOWN, MF_BYCOMMAND | MF_GRAYED);
				}
			} else if (parent.second + 1 == parent.first->topics.size()) {
				EnableMenuItem(hpopup_book, IDM_DOWN, MF_BYCOMMAND | MF_GRAYED);
			}
		}

		if (core_get_save_btn()) {
			EnableMenuItem(hpopup_book, IDM_GENERATE_ITEM2, MF_BYCOMMAND | MF_GRAYED);
		}

		TrackPopupMenuEx(hpopup_book, 0, 
			point.x, 
			point.y, 
			hdlgP, 
			NULL);

		DestroyMenu(hpopup_new);
		DestroyMenu(hpopup_move);
		DestroyMenu(hpopup_book);
	} 
}

void book_notify_handler_dblclk(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
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
		if (lpNMHdr->idFrom == IDC_TV_FORMATION_EXPLORER) {
			ns::clicked_book = lpnmitem->iItem;
			OnBookEditBt(hdlgP);
		}
	}
    return;
}

BOOL On_DlgBookNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	if (lpNMHdr->code == NM_RCLICK) {
		book_notify_handler_rclick(hdlgP, DlgItem, lpNMHdr);
	} else if (lpNMHdr->code == NM_DBLCLK) {
		book_notify_handler_dblclk(hdlgP, DlgItem, lpNMHdr);
	}
	return FALSE;
}

BOOL CALLBACK DlgBookProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_INITDIALOG:
		return On_DlgBookInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgBookCommand);
	HANDLE_MSG(hdlgP, WM_NOTIFY,  On_DlgBookNotify);
	}
	
	return FALSE;
}