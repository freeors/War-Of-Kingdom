#ifndef __STRUCT_H_
#define __STRUCT_H_

// #include "streambfr.h"
#include "hero.hpp"

#define MAX_BYTES_PERLINE			8
#define TB_BTNS_COUNT_SYS			6
#define TB_BTNS_COUNT_SYNC			6
#define TB_BTNS_COUNT_WGEN			17
#define TB_BTNS_COUNT_XCHG			7

typedef enum {
	da_unknown		= 0,
	da_sync,
	da_wgen,
	da_tb,
	da_cfg,
	da_tbox,
	da_about,
} do_action_t;

typedef enum {
	dsrc_unknown	= 0,
	dsrc_auto,
	dsrc_network,
	dsrc_removabledisk,
} dvr_source_t;

#define HWID_REMOVABLEDISK			0xFFFFFFFFFFFFFFF0I64

typedef struct {
	HINSTANCE		_hinst;
	HWND			_hwnd_main;
	char			_curexedir[_MAX_PATH];
	char			_userdir[_MAX_PATH];
	char			_programfilesdir[_MAX_PATH];
	char			_markbmp[_MAX_PATH];
	char			_pcbmp[_MAX_PATH];
	char			_dvrbmp[_MAX_PATH];
	HICON			_markico;
	HBITMAP			_welcomebmp;;

	CRITICAL_SECTION	_cshbeat;
	dvr_source_t	_dvrsrc;

	int				_cfgidx;
	time_t			_lasthbeattime;
	
	// 配置选项
	dvr_source_t	_appointed_dvrsrc;
	char			_appointed_removabledir[4];		// 只在_appointed_dvrsrc=dsrc_removabledisk时生效
	BOOL			_autocalendar;	// sync界面下,自动同步时间
	BOOL			_autosave;	// wgen界面下,按保存时自动保存到相关DVR目录
	BOOL			_autoxchg;	// xchg界面下,投了源文件名后自动转换
	BOOL			_autoexit;	// xchg界面下(没有设备浏览器),转换完后自动退出
	BOOL			_shengchan;	// 程序是否要处于自使用状态
	
	char			_fulloemini[_MAX_PATH];
	char			_fulldvrmgrini[_MAX_PATH];
	char			_corp_name[_MAX_PATH];
	char			_corp_http[_MAX_PATH];
	uint32_t		_corp_oem_desc;
	int32_t			_has_osdlib_page;
	HWND			_hdlg_title;
	HWND			_hdlg_ddesc;
	HWND			_hdlg_sync;
	HWND			_hdlg_wgen;
	HWND			_hdlg_tb;
	HWND			_hdlg_cfg;
	HWND			_hdlg_tbox;
	HWND			_hdlg_about;
	HWND			_hwnd_status;
	HWND			_hpb_task;		// 任务进度条

	do_action_t		_da;
		
	TBBUTTON		_tbBtns_sys[TB_BTNS_COUNT_SYS];
	HWND			_htb_sys;
	TBBUTTON		_tbBtns_sync[TB_BTNS_COUNT_SYNC];
	HWND			_htb_sync;
	TBBUTTON		_tbBtns_wgen[TB_BTNS_COUNT_WGEN];
	HWND			_htb_wgen;
	TBBUTTON		_tbBtns_xchg[TB_BTNS_COUNT_XCHG];
	HWND			_htb_xchg;

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

	BOOL			_hit_if_may_continue_tftp;
	
	HMENU			_hpopup_ddesc;
	HMENU			_hpopup_generate;
	HMENU			_hpopup_coherence;
	HMENU			_hpopup_delete;
	HMENU			_hpopup_pc;
	HMENU			_hpopup_editor;
	HMENU			_hpopup_osditem;

	HANDLE			_hthdSync;
	BOOL			_exit_sync_thread;

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

	int				list_header_height_;
} dvrmgr_t;
extern dvrmgr_t			gdmgr;

#define DEVICE_INI				"DEVICE.INI"

#define SECNAME_SYSTEM			"system"
#define KEYNAME_WWWROOT			"wwwroot"
#define KEYNAME_STARTUPSTRATEGY	"StartupStrategy"

#define SECNAME_SYNC			"sync"
#define KEYNAME_AUTOCALENDAR	"AutoCalendar"

#define SECNAME_WGEN			"wgen"
#define KEYNAME_AUTOSAVE		"AutoSave"

#define SECNAME_XCHG			"xchg"
#define KEYNAME_AUTOXCHG		"AutoXChg"
#define KEYNAME_AUTOEXIT		"AutoExit"

#define ID_STATUS_TIMER			2
#define ID_STARTUP_TIMER		3

typedef enum {
	tt_unknown		= 0,
	tt_filetrans,
	tt_synccheck,
} task_type_t;

#define SYNC_BYTES					CONSTANT_16K
#define XCHG_CASH_LEN				CONSTANT_1M
#define XCHG_CASH_ALMOST_FULL		CONSTANT_768K
#define XCHG_CASH_ALMOST_EMPTY		CONSTANT_256K

#define PAGE_NUM				256

#define MAX_OSDSTRING_ITEMS		12	// 除了时间戳外,最多还允许存在5项.(没限制项数,限制总长度,但因为要静态分配原因只好主观限制一个)
#define MAX_OSDITEM_CHARS		31	// osd_item_t定的BYTE是64,为丢一空位.只好限定31了

#define MAX_CFGITEM_CHARS		8
#define	MAXLEN_DESC				16	// 不是主观定的，和LCD等显示有关，详细见menu.c
#define MAX_LCD_CHARS			8

// osd
#define OSD_MAXLINEPERAREA		3

#define OSDMAXPERLINE_720				720
#define OSDMAXPERLINE_352				352
#define OSDEDGE_X_720					48
#define OSDEDGE_X_352					16
#define OSDEDGE_Y_720					32
#define OSDEDGE_Y_352					16
#define OSDTOP_Y_720					OSDEDGE_Y_720
#define OSDTOP_Y_352					OSDEDGE_Y_352
#define OSDBOTTOM_Y_720_PAL				(576 - OSDEDGE_Y_720 - 16 * OSD_MAXLINEPERAREA)
#define OSDBOTTOM_Y_720_NTSC			(480 - OSDEDGE_Y_720 - 16 * OSD_MAXLINEPERAREA)
#define OSDBOTTOM_Y_352_PAL				(288 - OSDEDGE_Y_352 - 16 * OSD_MAXLINEPERAREA)
#define OSDBOTTOM_Y_352_NTSC			(240 - OSDEDGE_Y_352 - 16 * OSD_MAXLINEPERAREA)
#define OSDMAXPERLINE_VALID_720			(OSDMAXPERLINE_720 - 2 * OSDEDGE_X_720)
#define OSDMAXPERLINE_VALID_352			(OSDMAXPERLINE_352 - 2 * OSDEDGE_X_352)



typedef struct {
	//
	char			_cfgname[MAX_CFGITEM_CHARS + 1];
	uint32_t		_width;		// 和制式相关,制式改动后连着改它
	uint32_t		_height;
	uint16_t		_osdmaxcharperline;
	uint16_t		_osdedge_x;
	uint16_t		_osdtop_y;
	uint16_t		_osdbottom_y;
	uint32_t		_osdhadchars_line[2 * OSD_MAXLINEPERAREA]; // 累计要叠回了字符数,这个变量只在那次选择时有效.(不包括时间戳)
	HWND			_osdtimestampctrl;	// 占据时间戳的那个组合框句柄,如果不存在,NULL

	// 以下是flash中环境变量
	// 它们放置的是可以直接写入flash的,因此像_lcd_line0,它就是转换好了的
	uint32_t		_fps;
	uint32_t		_channel;
	uint32_t		_channctl;
	uint32_t		_video_source;
	uint32_t		_audio_source;
	uint32_t		_volume;
	uint32_t		_tv_standard;
	uint32_t		_exportvt;
	uint32_t		_network_desc;
	uint32_t		_lsave_desc;
	// uint32_t		_timestamp_desc;
	// uint8_t		_osd_text0[6][64];
	uint32_t		_startlsave;
	uint32_t		_ctl_bitmap;
	uint32_t		_poweroff;
	uint32_t		_lcdpoweroff;
	uint32_t		_updoldknl;
	uint32_t		_menu_desc;
	uint32_t		_brightness0;
	uint32_t		_contrast0;
	uint32_t		_saturation0;
	uint32_t		_hue0;
	uint32_t		_mode;
	uint32_t		_interlace;
	uint32_t		_resolution;
	uint32_t		_sequence;
	uint32_t		_targetrate;
	uint32_t		_peakrate;
	uint32_t		_gopsize;
	uint32_t		_beacon_desc;
	uint32_t		_beacon_coor0;
	uint32_t		_beacon_coor1;
	uint32_t		_beacon_coor2;
	uint32_t		_beacon_coor3;
	uint32_t		_beacon_coor4;
	uint32_t		_beacon_coor5;
	uint32_t		_beacon_coor6;
	uint32_t		_beacon_masktime;
	uint32_t		_aformat;
	uint32_t		_startnxmit;
	uint32_t		_remote_desc;
	uint32_t		_prediscardbframes;
	uint32_t		_blk_bitmap;
	uint32_t		_md_sen;
	uint32_t		_lsduration;
	uint32_t		_gps_desc;

	char			_hostname[MAXLEN_DESC + 1];
	char			_workgroup[MAXLEN_DESC + 1];
	char			_scenedesc[MAXLEN_DESC + 1];
	char			_carrierdesc[MAXLEN_DESC + 1];
	char			_lfprefix[MAXLEN_DESC + 1];
	char			_lcd_line0[MAX_LCD_CHARS + 1];
	char			_lcd_line1[MAX_LCD_CHARS + 1];

	// 为了osd而增加的一些变量
	uint16_t		_tspos;
	uint16_t		_frame_buff[96];
	uint16_t		_frame_idx;
	uint16_t		_coor_buff[2 * (MAX_OSDSTRING_ITEMS + 1)];
	uint16_t		_coor_idx;

} environ_var_t;

//
// DlgWGenProc.cpp
//

// tv_standard

// sensor type
#define SENSOR_CHAN_MIN				0
#define SENSOR_CHAN_CH0D1			0
#define SENSOR_CHAN_CH1D1			1
#define SENSOR_CHAN_CH2D1			2
#define SENSOR_CHAN_CH3D1			3 
#define SENSOR_CHAN_4CIF			4
#define SENSOR_CHAN_PIP				5 
#define SENSOR_CHAN_PIP2			6 
#define SENSOR_CHAN_PIP2B			7 
#define SENSOR_CHAN_HSPLIT			8 
#define SENSOR_CHAN_VSPLIT			9
#define SENSOR_CHAN_TRIAD			10
#define SENSOR_CHAN_TRIADV			11
#define SENSOR_CHAN_MAX				11

#define FPS_FULL_NTSC				30
#define FPS_FULL_PAL				25

#define NAME_FPS_FULL_NTSC			"29.97fps"
#define NAME_FPS_FULL_PAL			"25.00fps"

// input_source
#define INPUT_SOURCE_MIN			0
#define INPUT_SOURCE_COMPOSITE0     0
#define INPUT_SOURCE_COMPOSITE1     1
#define INPUT_SOURCE_SVIDEO			2
#define INPUT_SOURCE_MAX			2

#define NAME_INPUT_SOURCE_COMPOSITE0	"Comp0"
#define NAME_INPUT_SOURCE_COMPOSITE1    "Comp1"
#define NAME_INPUT_SOURCE_SVIDEO		"S-Video"

// audio_source
#define AUDIO_SOURCE_MIN				0
#define AUDIO_SOURCE_LINEIN				0
#define AUDIO_SOURCE_EXTMIC				1
#define AUDIO_SOURCE_INTMIC				2
#define AUDIO_SOURCE_MAX				2

#define NAME_AUDIO_SOURCE_LINEIN		"Line In"
#define NAME_AUDIO_SOURCE_EXTMIC		"External MIC"
#define NAME_AUDIO_SOURCE_INTMIC		"Internal MIC"

#define NAME_LSAVE_METHOD_NORMAL		"Normal"
#define NAME_LSAVE_METHOD_CSF			"Cycle(Single File)"
#define NAME_LSAVE_METHOD_CIRCULARCAP	"Cycle(Disk)"

// mode
#define ENCODER_MODE_MODE_START_BIT		0
#define ENCODER_MODE_MODE_MASK			0x3F

#define ENCODER_MODE_MODE_MIN	(1<<0)
#define ENCODER_MODE_MPEG4      (1<<0)
#define ENCODER_MODE_MPEG2      (1<<1)
#define ENCODER_MODE_MPEG1      (1<<2)
#define ENCODER_MODE_H263       (1<<3)
#define ENCODER_MODE_H264       (1<<4)
#define ENCODER_MODE_MJPEG      (1<<5)
#define ENCODER_MODE_MODE_MAX	(1<<5)

#define NAME_ENCODER_MODE_MPEG4      "MPEG-4"
#define NAME_ENCODER_MODE_MPEG2      "MPEG-2"
#define NAME_ENCODER_MODE_MPEG1      "MPEG1"
#define NAME_ENCODER_MODE_H263       "H263"
#define NAME_ENCODER_MODE_H264       "H264"
#define NAME_ENCODER_MODE_MJPEG      "MJPEG"


// interlace
#define ENCODER_MODE_INTERLACED_START_BIT     6
#define ENCODER_MODE_INTERLACED_MASK          0xC0

#define ENCODER_MODE_INTERLACED     (1<<6)
#define ENCODER_MODE_PROGRESSIVE    (1<<7)

// resolution
#define ENCODER_MODE_RESOLUTION_START_BIT     8
#define ENCODER_MODE_RESOLUTION_MASK          0x3F00

#define ENCODER_MODE_160_X_128      (1<<8)  /* QQVGA */
#define ENCODER_MODE_176_X_144      (1<<9)  /* QCIF */
#define ENCODER_MODE_320_X_240      (1<<10)  /* QVGA */
#define ENCODER_MODE_352_X_288      (1<<11)  /* CIF */
#define ENCODER_MODE_640_X_480      (1<<12) /* VGA */
#define ENCODER_MODE_720_X_480      (1<<13) /* D1 */

#define NAME_ENCODER_MODE_160_X_128     "160 x 128"		/* QQVGA */
#define NAME_ENCODER_MODE_176_X_144     "176 x 144"		/* QCIF */
#define NAME_ENCODER_MODE_320_X_240     "320 x 240"		/* QVGA */
#define NAME_ENCODER_MODE_CIF_NTSC		"352 x 240"		/* NTSC D1 */
#define NAME_ENCODER_MODE_CIF_PAL		"352 x 288"		/* PAL D1 */
#define NAME_ENCODER_MODE_640_X_480     "640 x 480"		/* VGA */
#define NAME_ENCODER_MODE_D1_NTSC		"720 x 480"		/* NTSC D1 */
#define NAME_ENCODER_MODE_D1_PAL		"720 x 576"		/* PAL D1 */

#define ENCODER_MODE_AV_SYNC        (1<<14) /* AV Sync supported */

// sequence
#define ENCODER_MODE_SEQUENCE_START_BIT     24
#define ENCODER_MODE_SEQUENCE_MASK          0x7000000

#define ENCODER_MODE_SEQUENCE_MIN   (1<<24)
#define ENCODER_MODE_IBP            (1<<24)
#define ENCODER_MODE_IP_ONLY        (1<<25)
#define ENCODER_MODE_I_ONLY         (1<<26)
#define ENCODER_MODE_SEQUENCE_MAX   (1<<26)

#define NAME_ENCODER_MODE_IBP            "IBP"
#define NAME_ENCODER_MODE_IP_ONLY        "IP"
#define NAME_ENCODER_MODE_I_ONLY         "I"

// peakrate
#define NAME_PEAKRATE_1M			"LQ"	// 1M
#define NAME_PEAKRATE_1500			"1.5M"
#define NAME_PEAKRATE_2M			"EP"	// 2M(EP)
#define NAME_PEAKRATE_4M			"LP"	// 4M(LP)
#define NAME_PEAKRATE_5M			"5M"
#define NAME_PEAKRATE_6M			"SP"	// 6M(SP)
#define NAME_PEAKRATE_8M			"HQ"	// 8M(HQ)
#define NAME_PEAKRATE_10M			"UQ"	// 10M(UQ)

// gopsize
#define NAME_GOPSIZE_15				"15"
#define NAME_GOPSIZE_30				"30"
#define NAME_GOPSIZE_60				"60"
#define NAME_GOPSIZE_90				"90"

// aformat
enum AUDIO_CAPS
{
	CAP_AUDIO_FORMAT_NONE		= 0x00000000,

	CAP_AUDIO_FORMAT_PCM		= 0x00000001,
	CAP_AUDIO_FORMAT_ADPCM_MS	= 0x00000002,
	CAP_AUDIO_FORMAT_ADPCM_IMA	= 0x00000004,
	CAP_AUDIO_FORMAT_ALAW		= 0x00000008,
	CAP_AUDIO_FORMAT_ULAW		= 0x00000010,
	CAP_AUDIO_FORMAT_MP3		= 0x00000020,
	CAP_AUDIO_I2S_MASTER_MODE   = 0x00000040,

	CAP_AUDIO_SAMPLERATE_8K		= 0x00000100,
	CAP_AUDIO_SAMPLERATE_11025	= 0x00000200,
	CAP_AUDIO_SAMPLERATE_16K	= 0x00000400,
	CAP_AUDIO_SAMPLERATE_22050	= 0x00000800,
	CAP_AUDIO_SAMPLERATE_32K	= 0x00001000,
	CAP_AUDIO_SAMPLERATE_44100	= 0x00002000,
	CAP_AUDIO_SAMPLERATE_48K	= 0x00004000,

	CAP_AUDIO_CHANNEL_MONO		= 0x00010000,
	CAP_AUDIO_CHANNEL_STEREO	= 0x00020000,

	CAP_AUDIO_SAMPLE_4BIT		= 0x00040000,
	CAP_AUDIO_SAMPLE_8BIT		= 0x00080000,
	CAP_AUDIO_SAMPLE_16BIT		= 0x00100000,
};

// audio_format
#define AUDIO_FORMT_MASK				0x7F

#define NAME_AUDIO_FORMAT_PCM			"pcm"
#define NAME_AUDIO_FORMAT_ADPCM_MS		"adpcm_ms"

// audio_samplerate
#define AUDIO_SAMPLERATE_MASK			0x7F00
#define NAME_AUDIO_SAMPLERATE_8K		"8K"
#define NAME_AUDIO_SAMPLERATE_16K		"16K"
#define NAME_AUDIO_SAMPLERATE_32K		"32K"
#define NAME_AUDIO_SAMPLERATE_48K		"48K"

// audio_channel
#define AUDIO_CHANNEL_MASK				0x30000
#define NAME_AUDIO_CHANNEL_MONO			"mono"
#define NAME_AUDIO_CHANNEL_STEREO		"stereo"

// audio_samplebit
#define AUDIO_SAMPLEBIT_MASK			0x1C0000

// beacon_coor
#define NAME_BEACON_ITEM_SCENE			"Scene"
#define NAME_BEACON_ITEM_PREV			"Previous(Name)"
#define NAME_BEACON_ITEM_CURR			"Current(Name)"
#define NAME_BEACON_ITEM_BEST			"Best(Name)"
#define NAME_BEACON_ITEM_PREV_TC		"Previous(Time)"
#define NAME_BEACON_ITEM_CURR_TC		"Current(Time)"
#define NAME_BEACON_ITEM_BEST_TC		"Best(Time)"

//
// -------------------------------------------------------
//
HBITMAP LoadBitmap(char *fname, DWORD *biWidth, DWORD *biHeight);
void transparent_24bmp(HBITMAP hBitmap, DWORD transparentclr);
void title_select(do_action_t da);

void OnLSBt(BOOL checkrunning);
int CALLBACK fn_dvr_compare(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);

void sync_enter_ui(void);

// main.cpp
void update_locale_dir();

//
// DlgCfgProc.cpp
//
void cfg_enter_ui(void);
BOOL cfg_hide_ui(void);

//
// DlgTbProc.cpp
//
void tb_enable_ui(BOOL fEnable);
void tb_enter_ui(void);
BOOL tb_hide_ui(void);

// dlgwgenproc.cpp
void wgen_enter_ui(void);
BOOL wgen_hide_ui(void);

void sync_enable_ui(BOOL fEnable);

void title_enable_ui(BOOL fEnable);
void title_enter(void);
void title_plugin_ui(void);
void title_plugout_ui(void);

void main_ui_sysmenu(BOOL fEnable);
int select_iimage_according_fname(char *name, uint32_t flags);

// DlgTBoxProc.cpp
void tbox_enter_ui(void);
BOOL tbox_hide_ui(void);

void do_coherence(const char* siteloc, char *bfname, int update, char *specialfname = NULL);
void do_empty_directory(char *path);
void do_xgenfile(char *path, int del);
void do_publisher(char *siteloc, char *pagecfgbfname);

void exe_pc_exe(char *cmdline, BOOL fSync);

void StatusBar_Trans(void);
void StatusBar_Idle(void);

// dlgsyncproc.cpp
UINT ListView_GetCheckedCount(HWND hwnd);

// nmea.cpp
typedef struct {
	int64_t		_ldtime;		// 初始时刻,符合c运行库要求的时间,据此可使用localtime
	double		_maxLatitude;
	double		_maxLongitude; 
	double		_minLatitude;
	double		_minLongitude;
	double		_latitudeCenter;
	double		_longitudeCenter;
	int			_valid_rmcs;
	int			_novalid_rmcs;
} gpsglbvar_t;
extern gpsglbvar_t		ggpsglbvar;

#endif // __STRUCT_H_