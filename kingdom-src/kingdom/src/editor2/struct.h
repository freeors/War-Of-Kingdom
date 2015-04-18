#ifndef __STRUCT_H_
#define __STRUCT_H_


#include "hero.hpp"
#include "serialization/string_utils.hpp"
#include "filesystem.hpp"
#include "terrain_translation.hpp"

#ifndef NUMELMS
   #define NUMELMS(aa) (sizeof(aa)/sizeof((aa)[0]))
#endif

#define TB_BTNS_COUNT_SYS		6
#define TB_BTNS_COUNT_SYNC		6
#define TB_BTNS_COUNT_WGEN		17
#define TB_BTNS_COUNT_CAMPAIGN	7
#define TB_BTNS_COUNT_CORE		7
#define MAX_MODS				5

typedef enum {
	da_unknown		= 0,
	da_sync,
	da_wgen,
	da_core,
	da_visual,
	da_campaign,
	da_integrate
} do_action_t;

typedef struct tag_dlghdr {
    HWND hwndTab;       // tab control
    HWND hwndDisplay;   // current child dialog box 
    RECT rcDisplay;     // display rectangle for the tab control 
	DLGTEMPLATE** apRes;
	int reserved_pages;
	int valid_pages;
} DLGHDR;

enum task_type {TASK_NEW, TASK_DELETE, TASK_EXPLORER};

typedef struct {
	HINSTANCE		_hinst;
	HWND			_hwnd_main;
	char			_curexedir[_MAX_PATH];
	char			_userdir[_MAX_PATH];
	char			_programfilesdir[_MAX_PATH];
	HICON			_markico;
	HBITMAP			_welcomebmp;;

	CRITICAL_SECTION	_cshbeat;

	char			_corp_name[_MAX_PATH];
	char			_corp_http[_MAX_PATH];
	HWND			_hdlg_title;
	HWND			_hdlg_ddesc;
	HWND			_hdlg_sync;
	HWND			_hdlg_wgen;
	HWND			_hdlg_core;
	HWND			_hdlg_visual;
	HWND			_hdlg_campaign;
	HWND			_hdlg_integrate;
	HWND			_hwnd_status;
	HWND			_hpb_task;		// 任务进度条

	do_action_t		_da;
		
	TBBUTTON		_tbBtns_sys[TB_BTNS_COUNT_SYS];
	HWND			_htb_sys;
	TBBUTTON		_tbBtns_sync[TB_BTNS_COUNT_SYNC];
	HWND			_htb_sync;
	TBBUTTON		_tbBtns_wgen[TB_BTNS_COUNT_WGEN];
	HWND			_htb_wgen;
	TBBUTTON		_tbBtns_campaign[TB_BTNS_COUNT_CAMPAIGN];
	HWND			_htb_campaign;
	TBBUTTON		_tbBtns_core[TB_BTNS_COUNT_CORE];
	HWND			_htb_core;

	HWND			_htb_subcfg;
	HWND			_tt_ok;
	HWND			_tt_cancel;

	HWND			_tt_reboot;		// wgen界面时,针对reboot按钮的tooltip
	HWND			_tt_refresh;	// ddesc界面时,针对refresh按钮(ddesc部分)的tooltip

	HIMAGELIST		_himl;
	int				_iico_dir;
	int				_iico_trans_min;
	int				_iico_with_sync;
	int				_iico_without_sync;
	int				_iico_csf;
	int				_iico_txt;
	int				_iico_nma;
	int				_iico_ini;
	int				_iico_trans_max;
	int				_iico_unknown;

    int				_iico_download;
	int				_iico_upload;

	HIMAGELIST		_himl_24x24;
	HIMAGELIST		_himl_24x24_dis;
	HIMAGELIST		_himl_checkbox;
	int				_iico_open;			// 24x24
	int				_iico_reset;		// 24x24
	int				_iico_save;			// 24x24
	int				_iico_new;			// 24x24
	int				_iico_del;			// 24x24
	int				_iico_rename;		// 24x24
	int				_iico_xchg;			// 24x24
	int				_iico_refresh;		// 24x24
	int				_iico_sync;			// 24x24
	int				_iico_build;			// 24x24
	int				_iico_openpcdir;	// 24x24
	int				_iico_ok;			// 24x24
	int				_iico_cancel;		// 24x24
	int				_iico_backup;		// 24x24

	HIMAGELIST		_himl_80x80;
	HIMAGELIST		_himl_80x80_dis;
	int				_iico_title_sync;
	int				_iico_title_wgen;
	int				_iico_title_xchg;
	int				_iico_title_play;
	int				_iico_title_update;
	int				_iico_title_about;

	HBITMAP			_hbm_ok_select;
	HBITMAP			_hbm_ok_unselect;
	HBITMAP			_hbm_cancel_select;
	HBITMAP			_hbm_cancel_unselect;

	HBITMAP			_hbm_reboot_select;
	HBITMAP			_hbm_reboot_unselect;
	HBITMAP			_hbm_reboot_unselect_light;
	HBITMAP			_hbm_reboot_disable;

	HBITMAP			_hbm_refresh_select;
	HBITMAP			_hbm_refresh_unselect;
	HBITMAP			_hbm_refresh_disable;

	HMENU			_hpopup_ddesc;
	HMENU			_hpopup_new;
	HMENU			_hpopup_mod[MAX_MODS];
	HMENU			_hpopup_explorer;
	HMENU			_hpopup_delete;
	HMENU			_hpopup_delete2;
	HMENU			_hpopup_editor;
	HMENU			_hpopup_candidate;
	HMENU			_hpopup_mtype;

	HANDLE			_hthdSync;

	HTREEITEM		_htvroot;
	
	// 以下两个参数用于弹出式菜单
	char			_menu_text[_MAX_PATH];
	uint32_t		_menu_lparam;

	RECT			_main_rect;
	RECT			_video_area;
	BOOL			_fFullScreen;

	BOOL			_syncing;	// 正在执行同步
	
	BOOL			_main_timer_count;	// 主定时器计数

	hero_map		heros_;
	char			heros_fname_[_MAX_PATH];
	char			cfg_fname_[_MAX_PATH]; // processing file in cfg page
} dvrmgr_t;
extern dvrmgr_t			gdmgr;

#define ID_STATUS_TIMER			2

#define PAGE_NUM				256

//
// -------------------------------------------------------
//
bool isvalid_id(const std::string& id);
bool isvalid_variable(const std::string& id);

HBITMAP LoadBitmap(char *fname, DWORD *biWidth, DWORD *biHeight);
void transparent_24bmp(HBITMAP hBitmap, DWORD transparentclr);
void title_select(do_action_t da);

#define null_2_space(c) ((c) != '\0'? (c): '$')
std::string format_t_terrain(const t_translation::t_terrain& t);
std::string t_terrain_2_str(const t_translation::t_terrain& t);
std::string hex_t_terrain(const t_translation::t_terrain& t);

void OnLSBt(BOOL checkrunning);
int CALLBACK fn_dvr_compare(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);

bool can_execute_tack(int task);

void sync_enter_ui(void);
BOOL sync_hide_ui(void);
void do_sync2();
void sync_refresh_sync();

// main.cpp
void update_locale_dir();

//
// DlgCfgProc.cpp
//
void visual_enter_ui(void);
BOOL visual_hide_ui(void);

//
// integrate.cpp
//
void integrate_enter_ui(void);
BOOL integrate_hide_ui(void);

//
// DlgCoreProc.cpp
//
void core_enter_ui(void);
BOOL core_hide_ui(void);

bool core_can_execute_tack(int task);

// dlgwgenproc.cpp
void wgen_enter_ui(void);
BOOL wgen_hide_ui(void);

void sync_enable_ui(BOOL fEnable);

void title_enable_ui(BOOL fEnable);
void title_enter(void);

void main_ui_sysmenu(BOOL fEnable);
int select_iimage_according_fname(char *name, uint32_t flags);

// DlgCampaignProc.cpp
void campaign_enter_ui(void);
BOOL campaign_hide_ui(void);

bool campaign_new();
bool campaign_can_execute_tack(int task);

void exe_pc_exe(char *cmdline, BOOL fSync);

void StatusBar_Trans(void);
void StatusBar_Idle(void);

bool is_app_res(const std::string& folder);
bool check_wok_root_folder(const std::string& folder);
std::string vgettext2(const char *msgid, const utils::string_map& symbols);
const char* dgettext_2_ansi(const char* domain, const char* msgid);
void set_language_text(HWND hdlgP, int id, const char* domain, const char* msgid);

std::string get_rid_of_return(const std::string& str);
std::string insert_return(const std::string& str);

#endif // __STRUCT_H_