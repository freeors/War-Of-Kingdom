#define GETTEXT_DOMAIN "wesnoth-maker"

#include "game_config.hpp"
#include "loadscreen.hpp"
#include "gettext.hpp"
#include "serialization/binary_or_text.hpp"
#include "stdafx.h"
#include <windowsx.h>
#include <string.h>
#include "integrate.hpp"
#include "rectangle.hpp"
#include "resource.h"
#include "font.hpp"
#include "xfunc.h"
#include "win32x.h"
#include "struct.h"
#include "video.hpp"

#include <boost/foreach.hpp>

tintegrate2 integrate;

#define integrate_enable_save_btn(enable)	ToolBar_EnableButton(integrate.htb_integrate, IDM_SAVE, enable)
#define integrate_get_save_btn()			(ToolBar_GetState(integrate.htb_integrate, IDM_SAVE) & TBSTATE_ENABLED)

void integrate_enter_ui(void)
{
	StatusBar_Idle();

	StatusBar_SetText(gdmgr._hwnd_status, 0, utf8_2_ansi(_("Interate text, image...")));

	integrate.refresh(gdmgr._hdlg_integrate);
	return;
}

static bool save_if_dirty()
{
	if (integrate_get_save_btn()) {
		std::stringstream title, message;
		utils::string_map symbols;
		HWND hdlgP = gdmgr._hdlg_integrate;

		DLGHDR *pHdr = (DLGHDR *) GetWindowLong(hdlgP, GWL_USERDATA); 
		
		symbols["campaign"] = "";
		title << utf8_2_ansi(_("Save"));
		message << utf8_2_ansi(vgettext2("Integrate is dirty, do you want to save modify?", symbols).c_str());
		int retval = MessageBox(hdlgP, message.str().c_str(), title.str().c_str(), MB_YESNO);
		if (retval == IDYES) {
			integrate.save(gdmgr._hdlg_integrate);
			return true;
		} else {
			integrate_enable_save_btn(FALSE);
			return false;
		}
	}
	return false;
}

BOOL integrate_hide_ui(void)
{
	if (save_if_dirty()) {
		return FALSE;
	}
	return TRUE;
}

tintegrate2::tintegrate2()
	: formaters()
{
}

HWND tintegrate2::init_toolbar(HINSTANCE hinst, HWND hdlgP)
{
	// Create a toolbar
	htb_integrate = CreateWindowEx(0, TOOLBARCLASSNAME, (LPSTR)NULL, 
		WS_CHILD | /*CCS_ADJUSTABLE |*/ TBSTYLE_TOOLTIPS | TBSTYLE_FLAT, 0, 0, 0, 0, hdlgP, 
		(HMENU)IDR_WGENMENU, gdmgr._hinst, NULL);

	//Enable multiple image lists
    SendMessage(htb_integrate, CCM_SETVERSION, (WPARAM) 5, 0); 

	// Send the TB_BUTTONSTRUCTSIZE message, which is required for backward compatibility
	ToolBar_ButtonStructSize(htb_integrate, sizeof(TBBUTTON));
	
	TBBUTTON tbBtns_integrate[2];

	tbBtns_integrate[0].iBitmap = MAKELONG(gdmgr._iico_save, 0);
	tbBtns_integrate[0].idCommand = IDM_SAVE;	
	tbBtns_integrate[0].fsState = TBSTATE_ENABLED;
	tbBtns_integrate[0].fsStyle = BTNS_BUTTON;
	tbBtns_integrate[0].dwData = 0L;
	tbBtns_integrate[0].iString = -1;

	tbBtns_integrate[1].iBitmap = 100;
	tbBtns_integrate[1].idCommand = 0;	
	tbBtns_integrate[1].fsState = 0;
	tbBtns_integrate[1].fsStyle = TBSTYLE_SEP;
	tbBtns_integrate[1].dwData = 0L;
	tbBtns_integrate[1].iString = 0;

	ToolBar_AddButtons(htb_integrate, 2, &tbBtns_integrate);

	ToolBar_AutoSize(htb_integrate);
	
	ToolBar_SetExtendedStyle(htb_integrate, TBSTYLE_EX_DRAWDDARROWS);
	
	ToolBar_SetImageList(htb_integrate, gdmgr._himl_24x24, 0);
	ToolBar_SetDisabledImageList(htb_integrate, gdmgr._himl_24x24_dis);
	
	ShowWindow(htb_integrate, SW_SHOW);

	return htb_integrate;
}

void tintegrate2::refresh(HWND hdlgP)
{
	fill_formaters();

	Button_SetText(GetDlgItem(hdlgP, IDC_BT_INTEGRATE_CONVERT), utf8_2_ansi(_("Convert to")));

	std::stringstream strstr;
	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_INTEGRATE_FORMATER);
	ComboBox_ResetContent(hctl);
	for (std::vector<tformater>::const_iterator it = formaters.begin(); it != formaters.end(); ++ it) {
		const tformater& formater = *it;
		strstr.str("");
		strstr << utf8_2_ansi(formater.name) << " <=====> ";
		strstr << formater.example;
		ComboBox_AddString(hctl, strstr.str().c_str());
	}
	ComboBox_SetCurSel(hctl, 0);
}

bool tintegrate2::save(HWND hdlgP)
{
	return true;
}

void tintegrate2::insert_formater(const std::string& name, const std::string& example)
{
	formaters.push_back(tformater(name, example));
}

void tintegrate2::fill_formaters()
{
	if (!formaters.empty()) {
		return;
	}
	insert_formater(_("fomrat"), "<format>text='[modified]' color=green font_size=16 bold=yes italic=yes</format>");
	insert_formater(_("bold"), "<bold>text='[modified]'</bold>");
	insert_formater(_("image"), "<img>src='[modified]' align=left float=yes</img>");
	insert_formater(_("link"), "<ref>text='[modified]' dst='[id]'</ref>");
}

BOOL On_DlgIntegrateInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	gdmgr._hdlg_integrate = hdlgP;
	integrate.init_toolbar(gdmgr._hinst, hdlgP);
	integrate_enable_save_btn(FALSE);

	return FALSE;
}

void OnIntegrateConvertBt(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	RECT rc;
	GetWindowRect(hwndCtrl, &rc);

	HMENU hpopup_convert = CreatePopupMenu();
	AppendMenu(hpopup_convert, MF_STRING, IDM_CONVERT_NORMAL, utf8_2_ansi(_("Normal")));
	AppendMenu(hpopup_convert, MF_STRING, IDM_CONVERT_POEDIT, utf8_2_ansi(_("Poedit")));

	TrackPopupMenuEx(hpopup_convert, 0, 
			rc.left,
			rc.bottom,
			hdlgP, 
			NULL);

	DestroyMenu(hpopup_convert);
}

void replace_all(std::string& str, const std::string& old_value, const std::string& new_value)
{
	while (true) {
		std::string::size_type pos(0);
		if ((pos = str.find(old_value)) != std::string::npos) {
			str.replace(pos, old_value.length(), new_value);
		} else {
			break;
		}
	}
}
 
void replace_all_distinct(std::string& str, const std::string& before_forbid, const std::string& old_value, const std::string& new_value)
{
	size_t before_forbid_size = before_forbid.size();
	for (std::string::size_type pos(0); pos != std::string::npos; pos += new_value.length()) {
		if ((pos = str.find(old_value,pos)) != std::string::npos) {
			if (before_forbid_size > 0 && pos >= before_forbid_size && str.substr(pos - before_forbid_size, before_forbid_size) == before_forbid) {
				continue;
			}
			str.replace(pos, old_value.length(), new_value);
		} else {
			break;
		}
	}
}

void text_convert_to(HWND hdlgP, int id)
{
	HWND hctl = GetDlgItem(hdlgP, IDC_ET_INTEGRATE_TEXT);
	int len = Edit_GetTextLength(hctl);
	tmalloc_free_lock lock(len + 1);
	Edit_GetText(hctl, lock.buf, lock.len);
	std::string str(ansi_2_utf8(lock.buf));

	std::stringstream err;
	try {
		help::parse_text(str);

	} catch (game::error& e) {
		err << e.message;

	} catch (utils::invalid_utf8_exception&) {
		err << _("Encounter unknown format character!");
	}
	if (!err.str().empty()) {
		err << "\n\n";
		err << _("Exist grammar error, don't execute converting!");
		MessageBox(NULL, utf8_2_ansi(err.str().c_str()), "Error", MB_OK | MB_ICONWARNING);
		return;
	}

	if (id == IDM_CONVERT_NORMAL) {
		// \\n\r\n to \r\n
		replace_all_distinct(str, null_str, "\\n\r\n", "\r\n");
	} else if (id == IDM_CONVERT_POEDIT) {
		// \r\n to \\n\r\n
		replace_all_distinct(str, "\\n", "\r\n", "\\n\r\n");
	} else {
		return;
	}
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_INTEGRATE_TEXT), utf8_2_ansi(str));
}

void On_DlgIntegrateCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;
	
	switch (id) {
	case IDC_BT_INTEGRATE_CONVERT:
		OnIntegrateConvertBt(hdlgP, id, hwndCtrl, codeNotify);
		break;

	case IDM_CONVERT_NORMAL:
	case IDM_CONVERT_POEDIT:
		text_convert_to(hdlgP, id);
		break;
	}
}

BOOL On_DlgIntegrateNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	if (lpNMHdr->code == TCN_SELCHANGE) {

	} else if (lpNMHdr->code == TCN_SELCHANGING) {

	}
	return FALSE;
}

void On_DlgIntegrateDestroy(HWND hdlgP)
{
	return;
}

BOOL CALLBACK DlgIntegrateProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_INITDIALOG:
		return On_DlgIntegrateInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND,  On_DlgIntegrateCommand);
	HANDLE_MSG(hdlgP, WM_NOTIFY,  On_DlgIntegrateNotify);
	HANDLE_MSG(hdlgP, WM_DESTROY,  On_DlgIntegrateDestroy);
	}
	
	return FALSE;
}
