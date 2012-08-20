#include <windows.h>
#include <time.h>

#include <string>
#include "gettext.hpp"
/*
// for memory lead detect begin
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <malloc.h>
#include <crtdbg.h>
// for memory lead detect end
*/
#ifndef PACKAGE
#define PACKAGE "wesnoth"
#endif

std::string get_intl_dir()
{
	return std::string("c:\\kingdom-src") + "/translations";
}

static void init_locale()
{
	BOOL fok = SetEnvironmentVariable("LANG", "en_GB");
	// BOOL fok = SetEnvironmentVariable("LANG", "zh_CN");
	const std::string& intl_dir = get_intl_dir();
	textdomain(PACKAGE "-hero");
	bindtextdomain (PACKAGE, intl_dir.c_str());
	bind_textdomain_codeset (PACKAGE, "UTF-8");
	bindtextdomain (PACKAGE "-hero", intl_dir.c_str());
	bind_textdomain_codeset (PACKAGE "-hero", "UTF-8");

	char* msgstr = gettext("name-1");
	msgstr = dgettext(PACKAGE, "Name");
	msgstr = gettext("desc1");
	msgstr = gettext("desc0");
}

int PASCAL WinMain(HINSTANCE inst, HINSTANCE, LPSTR lpCmdLine, int nCmdShow)
{
	_CrtSetDbgFlag (_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	
	init_locale();

	return 0;
}


