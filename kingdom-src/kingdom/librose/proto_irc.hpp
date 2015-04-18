#ifndef LIBROSE_PROTO_IRC_HPP_INCLUDED
#define LIBROSE_PROTO_IRC_HPP_INCLUDED

#include <time.h>
#include <list>
#include <string>
#include "ichat.hpp"

namespace irc
{
/* force a 32bit CMP.L */
#define WORDL(c0, c1, c2, c3) (unsigned int)(c0 | (c1 << 8) | (c2 << 16) | (c3 << 24))

struct favchannel {
	char *name;
	char *key;
};

#define FONTNAMELEN	127
#define PATHLEN		255
#define DOMAINLEN	100
#define NICKLEN		64				/* including the NULL, so 63 really */
#define CHANLEN		300
#define PDIWORDS		32
#define USERNAMELEN 10
#define HIDDEN_CHAR	8			/* invisible character for xtext */

struct session
{
	session();
	~session();

	/* Per-Channel Alerts */
	/* use a byte, because we need a pointer to each element */
	unsigned char alert_beep;
	unsigned char alert_taskbar;
	unsigned char alert_tray;

	/* Per-Channel Settings */
	unsigned char text_hidejoinpart;
	unsigned char text_logging;
	unsigned char text_scrollback;
	unsigned char text_strip;

	struct server *server;
	// tree *usertree;					/* alphabetical tree */
	// struct User *me;					/* points to myself in the usertree */
	char channel[CHANLEN];
	char waitchannel[CHANLEN];		  /* waiting to join channel (/join sent) */
	char willjoinchannel[CHANLEN];	  /* will issue /join for this channel */
	char session_name[CHANLEN];		 /* the name of the session, should not modified */
	char channelkey[64];			  /* XXX correct max length? */
	int limit;						  /* channel user limit */
	int logfd;
	int scrollfd;							/* scrollback filedes */
	int scrollwritten;					/* number of lines written */

	char lastnick[NICKLEN];			  /* last nick you /msg'ed */

	// struct history history;

	int ops;								/* num. of ops in channel */
	int hops;						  /* num. of half-oped users */
	int voices;							/* num. of voiced people */
	int total;							/* num. of users in channel */

	char *quitreason;
	char *topic;
	char *current_modes;					/* free() me */

	int mode_timeout_tag;

	struct session *lastlog_sess;
	struct nbexec *running_exec;

	struct session_gui *gui;		/* initialized by fe_new_window */
	struct restore_gui *res;

	int type;					/* SESS_* */

	int lastact_idx;		/* the sess_list_by_lastact[] index of the list we're in.
							 * For valid values, see defines of LACT_*. */

	int new_data:1;			/* new data avail? (purple tab) */
	int nick_said:1;		/* your nick mentioned? (blue tab) */
	int msg_said:1;			/* new msg available? (red tab) */

	int ignore_date:1;
	int ignore_mode:1;
	int ignore_names:1;
	int end_of_names:1;
	int doing_who:1;		/* /who sent on this channel */
	int done_away_check:1;	/* done checking for away status changes */
	// gtk_xtext_search_flags lastlog_flags;
	void (*scrollback_replay_marklast) (struct session *sess);
};

struct ircnet {
	std::string name;
	std::string nick;
	std::string nick2;
	std::string user;
	std::string real;
	std::string pass;
	int logintype;
	// char *encoding;
	// GSList *servlist;
	// GSList *commandlist;
	// GSList *favchanlist;
	int selected;
	unsigned int flags;
};

#define MAX_PARAMS	10
struct server 
{
	~server();

	std::list<session*> sess_list;
	ichat* sock;
	const char* params[MAX_PARAMS];

	/*  server control operations (in server*.c) */
	void (*connect)(struct server *, char *hostname, int port, int no_login);
	void (*disconnect)(struct session *, int sendquit, int err);
	int  (*cleanup)(struct server *);
	void (*flush_queue)(struct server *);
	void (*auto_reconnect)(struct server *, int send_quit, int err);
	/* irc protocol functions (in proto*.c) */
	void (*p_inline)(struct server *, char *buf, int len);
	void (*p_invite)(struct server *, char *channel, char *nick);
	void (*p_cycle)(struct server *, char *channel, char *key);
	void (*p_ctcp)(struct server *, char *to, char *msg);
	void (*p_nctcp)(struct server *, char *to, char *msg);
	void (*p_quit)(struct server *, char *reason);
	void (*p_kick)(struct server *, char *channel, char *nick, char *reason);
	void (*p_part)(struct server *, const char *channel, const char *reason);
	void (*p_ns_identify)(struct server *, char *pass);
	void (*p_ns_ghost)(struct server *, char *usname, char *pass);
	void (*p_join)(struct server *, const char *channel, const char *key);
	void (*p_join_list)(struct server *, const std::list<favchannel*>& favorites);
	void (*p_login)(struct server *, const char *user, const char *realname);
	void (*p_join_info)(struct server *, const char *channel);
	void (*p_mode)(struct server *, char *target, char *mode);
	void (*p_user_list)(struct server *, const char *channel);
	void (*p_away_status)(struct server *, const char *channel);
	void (*p_whois)(struct server *, char *nicks);
	void (*p_get_ip)(struct server *, const char *nick);
	void (*p_get_ip_uh)(struct server *, char *nick);
	void (*p_set_back)(struct server *);
	void (*p_set_away)(struct server *, char *reason);
	void (*p_message)(struct server *, const char *channel, const char *text);
	void (*p_action)(struct server *, char *channel, char *act);
	void (*p_notice)(struct server *, char *channel, char *text);
	void (*p_topic)(struct server *, char *channel, char *topic);
	void (*p_list_channels)(struct server *, const char *arg, int min_users);
	void (*p_change_nick)(struct server *, const char *new_nick);
	void (*p_names)(struct server *, char *channel);
	void (*p_ping)(struct server *, const char *to, const char *timestring);
	void (*p_monitor)(struct server *, const char* nick, bool add);
/*	void (*p_set_away)(struct server *);*/
	int (*p_raw)(struct server *, char *raw);
	int (*p_cmp)(const char *s1, const char *s2);

	int port;
	int sok;					/* is equal to sok4 or sok6 (the one we are using) */
	int sok4;					/* tcp4 socket */
	int sok6;					/* tcp6 socket */
	int proxy_type;
	int proxy_sok;				/* Additional information for MS Proxy beast */
	int proxy_sok4;
	int proxy_sok6;
	int id;					/* unique ID number (for plugin API) */
#ifdef USE_OPENSSL
	SSL_CTX *ctx;
	SSL *ssl;
	int ssl_do_connect_tag;
#else
	void *ssl;
#endif
	int childread;
	int childwrite;
	int childpid;
	int iotag;
	int recondelay_tag;				/* reconnect delay timeout */
	int joindelay_tag;				/* waiting before we send JOIN */
	char hostname[128];				/* real ip number */
	char servername[128];			/* what the server says is its name */
	char password[86];
	char nick[NICKLEN];
	char linebuf[2048];				/* RFC says 512 chars including \r\n */
	char *last_away_reason;
	int pos;								/* current position in linebuf */
	int nickcount;
	int loginmethod;					/* see login_types[] */

	char *chantypes;					/* for 005 numeric - free me */
	char *chanmodes;					/* for 005 numeric - free me */
	char *nick_prefixes;				/* e.g. "*@%+" */
	char *nick_modes;					/* e.g. "aohv" */
	char *bad_nick_prefixes;		/* for ircd that doesn't give the modes */
	int modes_per_line;				/* 6 on undernet, 4 on efnet etc... */

	ircnet network;						/* points to entry in servlist.c or NULL! */

	// std::list outbound_queue;
	time_t next_send;						/* cptr->since in ircu */
	time_t prev_now;					/* previous now-time */
	int sendq_len;						/* queue size */
	int lag;								/* milliseconds */

	struct session *front_session;	/* front-most window/tab */
	struct session *server_session;	/* server window/tab */

	struct server_gui *gui;		  /* initialized by fe_new_server */

	unsigned int ctcp_counter;	  /*flood */
	time_t ctcp_last_time;

	unsigned int msg_counter;	  /*counts the msg tab opened in a certain time */
	time_t msg_last_time;

	/*time_t connect_time;*/				/* when did it connect? */
	unsigned long lag_sent;   /* we are still waiting for this ping response*/
	time_t ping_recv;					/* when we last got a ping reply */
	time_t away_time;					/* when we were marked away */

	char *encoding;
	// GIConv read_converter;  /* iconv converter for converting from server encoding to UTF-8. */
	// GIConv write_converter; /* iconv converter for converting from UTF-8 to server encoding. */

	std::list<favchannel*> favlist;			/* list of channels & keys to join */

	unsigned int motd_skipped:1;
	unsigned int connected:1;
	unsigned int connecting:1;
	unsigned int no_login:1;
	unsigned int skip_next_userhost:1;/* used for "get my ip from server" */
	unsigned int skip_next_whois:1;	/* hide whois output */
	unsigned int inside_whois:1;
	unsigned int doing_dns:1;			/* /dns has been done */
	unsigned int retry_sasl:1;		/* retrying another sasl mech */
	unsigned int end_of_motd:1;		/* end of motd reached (logged in) */
	unsigned int sent_quit:1;			/* sent a QUIT already? */
	unsigned int use_listargs:1;		/* undernet and dalnet need /list >0,<10000 */
	unsigned int is_away:1;
	unsigned int reconnect_away:1;	/* whether to reconnect in is_away state */
	unsigned int dont_use_proxy:1;	/* to proxy or not to proxy */
	unsigned int supports_watch:1;	/* supports the WATCH command */
	unsigned int supports_monitor:1;	/* supports the MONITOR command */
	unsigned int bad_prefix:1;			/* gave us a bad PREFIX= 005 number */
	unsigned int have_namesx:1;		/* 005 tokens NAMESX and UHNAMES */
	unsigned int have_awaynotify:1;
	unsigned int have_uhnames:1;
	unsigned int have_whox:1;		/* have undernet's WHOX features */
	unsigned int have_idmsg:1;		/* freenode's IDENTIFY-MSG */
	unsigned int have_accnotify:1; /* cap account-notify */
	unsigned int have_extjoin:1;	/* cap extended-join */
	unsigned int have_server_time:1;	/* cap server-time */
	unsigned int have_sasl:1;		/* SASL capability */
	unsigned int have_except:1;	/* ban exemptions +e */
	unsigned int have_invite:1;	/* invite exemptions +I */
	unsigned int have_cert:1;	/* have loaded a cert */
	unsigned int use_who:1;			/* whether to use WHO command to get dcc_ip */
	unsigned int sasl_mech;			/* mechanism for sasl auth */
	unsigned int sent_saslauth:1;	/* have sent AUTHENICATE yet */
	unsigned int sent_capend:1;	/* have sent CAP END yet */
#ifdef USE_OPENSSL
	unsigned int use_ssl:1;				  /* is server SSL capable? */
	unsigned int accept_invalid_cert:1;/* ignore result of server's cert. verify */
#endif
};

struct hexchatprefs
{
	/* these are the rebranded, consistent, sorted hexchat variables */

	/* BOOLEANS */
	unsigned int hex_away_auto_unmark;
	unsigned int hex_away_omit_alerts;
	unsigned int hex_away_show_once;
	unsigned int hex_away_track;
	unsigned int hex_completion_auto;
	unsigned int hex_dcc_auto_chat;
	unsigned int hex_dcc_auto_resume;
	unsigned int hex_dcc_fast_send;
	unsigned int hex_dcc_ip_from_server;
	unsigned int hex_dcc_remove;
	unsigned int hex_dcc_save_nick;
	unsigned int hex_dcc_send_fillspaces;
	unsigned int hex_gui_autoopen_chat;
	unsigned int hex_gui_autoopen_dialog;
	unsigned int hex_gui_autoopen_recv;
	unsigned int hex_gui_autoopen_send;
	unsigned int hex_gui_compact;
	unsigned int hex_gui_filesize_iec;
	unsigned int hex_gui_focus_omitalerts;
	unsigned int hex_gui_hide_menu;
	unsigned int hex_gui_input_attr;
	unsigned int hex_gui_input_icon;
	unsigned int hex_gui_input_nick;
	unsigned int hex_gui_input_spell;
	unsigned int hex_gui_input_style;
	unsigned int hex_gui_join_dialog;
	unsigned int hex_gui_mode_buttons;
	unsigned int hex_gui_quit_dialog;
	/* unsigned int hex_gui_single; */
	unsigned int hex_gui_slist_fav;
	unsigned int hex_gui_slist_skip;
	unsigned int hex_gui_tab_chans;
	unsigned int hex_gui_tab_dialogs;
	unsigned int hex_gui_tab_dots;
	unsigned int hex_gui_tab_icons;
	unsigned int hex_gui_tab_scrollchans;
	unsigned int hex_gui_tab_server;
	unsigned int hex_gui_tab_sort;
	unsigned int hex_gui_tab_utils;
	unsigned int hex_gui_topicbar;
	unsigned int hex_gui_tray;
	unsigned int hex_gui_tray_away;
	unsigned int hex_gui_tray_blink;
	unsigned int hex_gui_tray_close;
	unsigned int hex_gui_tray_minimize;
	unsigned int hex_gui_tray_quiet;
	unsigned int hex_gui_ulist_buttons;
	unsigned int hex_gui_ulist_color;
	unsigned int hex_gui_ulist_count;
	unsigned int hex_gui_ulist_hide;
	unsigned int hex_gui_ulist_icons;
	unsigned int hex_gui_ulist_resizable;
	unsigned int hex_gui_ulist_show_hosts;
	unsigned int hex_gui_ulist_style;
	unsigned int hex_gui_usermenu;
	unsigned int hex_gui_win_modes;
	unsigned int hex_gui_win_save;
	unsigned int hex_gui_win_swap;
	unsigned int hex_gui_win_ucount;
	unsigned int hex_identd;
	unsigned int hex_input_balloon_chans;
	unsigned int hex_input_balloon_hilight;
	unsigned int hex_input_balloon_priv;
	unsigned int hex_input_beep_chans;
	unsigned int hex_input_beep_hilight;
	unsigned int hex_input_beep_priv;
	unsigned int hex_input_filter_beep;
	unsigned int hex_input_flash_chans;
	unsigned int hex_input_flash_hilight;
	unsigned int hex_input_flash_priv;
	unsigned int hex_input_perc_ascii;
	unsigned int hex_input_perc_color;
	unsigned int hex_input_tray_chans;
	unsigned int hex_input_tray_hilight;
	unsigned int hex_input_tray_priv;
	unsigned int hex_irc_auto_rejoin;
	unsigned int hex_irc_conf_mode;
	unsigned int hex_irc_hidehost;
	unsigned int hex_irc_hide_nickchange;
	unsigned int hex_irc_hide_version;
	unsigned int hex_irc_invisible;
	unsigned int hex_irc_logging;
	unsigned int hex_irc_raw_modes;
	unsigned int hex_irc_servernotice;
	unsigned int hex_irc_skip_motd;
	unsigned int hex_irc_wallops;
	unsigned int hex_irc_who_join;
	unsigned int hex_irc_whois_front;
	unsigned int hex_irc_cap_server_time;
	unsigned int hex_net_auto_reconnect;
	unsigned int hex_net_auto_reconnectonfail;
	unsigned int hex_net_proxy_auth;
	unsigned int hex_net_throttle;
	unsigned int hex_notify_whois_online;
	unsigned int hex_perl_warnings;
	unsigned int hex_stamp_log;
	unsigned int hex_stamp_text;
	unsigned int hex_text_autocopy_color;
	unsigned int hex_text_autocopy_stamp;
	unsigned int hex_text_autocopy_text;
	unsigned int hex_text_color_nicks;
	unsigned int hex_text_indent;
	unsigned int hex_text_replay;
	unsigned int hex_text_search_case_match;
	unsigned int hex_text_search_highlight_all;
	unsigned int hex_text_search_follow;
	unsigned int hex_text_search_regexp;
	unsigned int hex_text_show_marker;
	unsigned int hex_text_show_sep;
	unsigned int hex_text_stripcolor_msg;
	unsigned int hex_text_stripcolor_replay;
	unsigned int hex_text_stripcolor_topic;
	unsigned int hex_text_thin_sep;
	unsigned int hex_text_transparent;
	unsigned int hex_text_wordwrap;
	unsigned int hex_url_grabber;
	unsigned int hex_url_logging;

	/* NUMBERS */
	int hex_away_size_max;
	int hex_away_timeout;
	int hex_completion_amount;
	int hex_completion_sort;
	int hex_dcc_auto_recv;
	int hex_dcc_blocksize;
	int hex_dcc_global_max_get_cps;
	int hex_dcc_global_max_send_cps;
	int hex_dcc_max_get_cps;
	int hex_dcc_max_send_cps;
	int hex_dcc_permissions;
	int hex_dcc_port_first;
	int hex_dcc_port_last;
	int hex_dcc_stall_timeout;
	int hex_dcc_timeout;
	int hex_flood_ctcp_num;				/* flood */
	int hex_flood_ctcp_time;			/* seconds of floods */
	int hex_flood_msg_num;				/* same deal */
	int hex_flood_msg_time;
	int hex_gui_chanlist_maxusers;
	int hex_gui_chanlist_minusers;
	int hex_gui_dialog_height;
	int hex_gui_dialog_left;
	int hex_gui_dialog_top;
	int hex_gui_dialog_width;
	int hex_gui_lagometer;
	int hex_gui_lang;
	int hex_gui_pane_divider_position;
	int hex_gui_pane_left_size;
	int hex_gui_pane_right_size;
	int hex_gui_pane_right_size_min;
	int hex_gui_search_pos;
	int hex_gui_slist_select;
	int hex_gui_tab_layout;
	int hex_gui_tab_middleclose;
	int hex_gui_tab_newtofront;
	int hex_gui_tab_pos;
	int hex_gui_tab_small;
	int hex_gui_tab_trunc;
	int hex_gui_transparency;
	int hex_gui_throttlemeter;
	int hex_gui_ulist_pos;
	int hex_gui_ulist_sort;
	int hex_gui_url_mod;
	int hex_gui_win_height;
	int hex_gui_win_fullscreen;
	int hex_gui_win_left;
	int hex_gui_win_state;
	int hex_gui_win_top;
	int hex_gui_win_width;
	int hex_identd_port;
	int hex_irc_ban_type;
	int hex_irc_join_delay;
	int hex_irc_notice_pos;
	int hex_net_ping_timeout;
	int hex_net_proxy_port;
	int hex_net_proxy_type;				/* 0=disabled, 1=wingate 2=socks4, 3=socks5, 4=http */
	int hex_net_proxy_use;				/* 0=all 1=IRC_ONLY 2=DCC_ONLY */
	int hex_net_reconnect_delay;
	int hex_notify_timeout;
	int hex_text_max_indent;
	int hex_text_max_lines;
	int hex_url_grabber_limit;

	/* STRINGS */
	char hex_away_reason[256];
	char hex_completion_suffix[4];		/* Only ever holds a one-character string. */
	char hex_dcc_completed_dir[PATHLEN + 1];
	char hex_dcc_dir[PATHLEN + 1];
	char hex_dcc_ip[DOMAINLEN + 1];
	char hex_gui_ulist_doubleclick[256];
	char hex_input_command_char[4];
	char hex_irc_extra_hilight[300];
	char hex_irc_id_ntext[64];
	char hex_irc_id_ytext[64];
	char hex_irc_logmask[256];
	char hex_irc_nick1[NICKLEN];
	char hex_irc_nick2[NICKLEN];
	char hex_irc_nick3[NICKLEN];
	char hex_irc_nick_hilight[300];
	char hex_irc_no_hilight[300];
	char hex_irc_part_reason[256];
	char hex_irc_quit_reason[256];
	char hex_irc_real_name[127];
	char hex_irc_user_name[127];
	char hex_net_bind_host[127];
	char hex_net_proxy_host[64];
	char hex_net_proxy_pass[32];
	char hex_net_proxy_user[32];
	char hex_stamp_log_format[64];
	char hex_stamp_text_format[64];
	char hex_text_background[PATHLEN + 1];
	char hex_text_font[4 * FONTNAMELEN + 1];
	char hex_text_font_main[FONTNAMELEN + 1];
	char hex_text_font_alternative[3 * FONTNAMELEN + 1];
	char hex_text_spell_langs[64];

	/* these are the private variables */
	unsigned int local_ip;
	unsigned int dcc_ip;

	unsigned int wait_on_exit;	/* wait for logs to be flushed to disk IF we're connected */

	/* Tells us if we need to save, only when they've been edited.
		This is so that we continue using internal defaults (which can
		change in the next release) until the user edits them. */
	unsigned int save_pevents:1;
};

/* Session types */
#define SESS_SERVER		1
#define SESS_CHANNEL	2
#define SESS_DIALOG		3
#define SESS_NOTICES	4
#define SESS_SNOTICES	5

/* Per-Channel Settings */
#define SET_OFF		0
#define SET_ON		1
#define SET_DEFAULT 2 /* use global setting */

/* Priorities in the "interesting sessions" priority queue
 * (see xchat.c:sess_list_by_lastact) */
#define LACT_NONE		-1		/* no queues */
#define LACT_QUERY_HI	0		/* query with hilight */
#define LACT_QUERY		1		/* query with messages */
#define LACT_CHAN_HI	2		/* channel with hilight */
#define LACT_CHAN		3		/* channel with messages */
#define LACT_CHAN_DATA	4		/* channel with other data */

#define MESSAGE_TAGS_DATA_INIT			\
	{									\
		(time_t)0, /* timestamp */		\
	}

/* Message tag information that might be passed along with a server message
 *
 * See http://ircv3.atheme.org/specification/capability-negotiation-3.1
 */
typedef struct 
{
	time_t timestamp;
} message_tags_data;

void proto_fill_her_up(server* serv);
void tcp_sendf(server *serv, const char *fmt, ...);

bool is_channel(const server * serv, const char *chan);
unsigned long make_ping_time(void);
server* server_new(const ircnet& net);
session* new_ircwindow(server* serv, char* name, int type);
void process_data_init(char *buf, const char *cmd, char *word[],
						 char *word_eol[], bool handle_quotes,
						 bool allow_escape_quotes);
session* find_channel(server *serv, const char* chan);
bool part_channel(server* serv, const char* chan);
void send_msg(struct server* serv, const char* nick, const char* chan, char* text);

/* Login methods. Use server password by default - if we had a NickServ password, it'd be set to 2 already by servlist_load() */
#define LOGIN_DEFAULT_REAL		LOGIN_PASS		/* this is to set the default login method for unknown servers */
#define LOGIN_DEFAULT			0				/* this is for the login type dropdown, doesn't serve any other purpose */
#define LOGIN_MSG_NICKSERV		1
#define LOGIN_NICKSERV			2
#if 0
#define LOGIN_NS				3
#define LOGIN_MSG_NS			4
#define LOGIN_AUTH				5
#endif
#define LOGIN_SASL				6
#define LOGIN_PASS				7
#define LOGIN_CHALLENGEAUTH		8
#define LOGIN_CUSTOM			9
#define LOGIN_SASLEXTERNAL		10

#define CHALLENGEAUTH_ALGO		"HMAC-SHA-256"
#define CHALLENGEAUTH_NICK		"Q@CServe.quakenet.org"

/* this file is auto generated, edit textevents.in instead! */

enum
{
	XP_TE_ADDNOTIFY,		XP_TE_BANLIST,
	XP_TE_BANNED,		XP_TE_BEEP,
	XP_TE_CAPACK,		XP_TE_CAPLIST,
	XP_TE_CAPREQ,		XP_TE_CHANGENICK,
	XP_TE_CHANACTION,		XP_TE_HCHANACTION,
	XP_TE_CHANBAN,		XP_TE_CHANDATE,
	XP_TE_CHANDEHOP,		XP_TE_CHANDEOP,
	XP_TE_CHANDEVOICE,		XP_TE_CHANEXEMPT,
	XP_TE_CHANHOP,		XP_TE_CHANINVITE,
	XP_TE_CHANLISTHEAD,		XP_TE_CHANMSG,
	XP_TE_CHANMODEGEN,		XP_TE_CHANMODES,
	XP_TE_HCHANMSG,		XP_TE_CHANNOTICE,
	XP_TE_CHANOP,		XP_TE_CHANQUIET,
	XP_TE_CHANRMEXEMPT,		XP_TE_CHANRMINVITE,
	XP_TE_CHANRMKEY,		XP_TE_CHANRMLIMIT,
	XP_TE_CHANSETKEY,		XP_TE_CHANSETLIMIT,
	XP_TE_CHANUNBAN,		XP_TE_CHANUNQUIET,
	XP_TE_CHANURL,		XP_TE_CHANVOICE,
	XP_TE_CONNECTED,		XP_TE_CONNECT,
	XP_TE_CONNFAIL,		XP_TE_CTCPGEN,
	XP_TE_CTCPGENC,		XP_TE_CTCPSEND,
	XP_TE_CTCPSND,		XP_TE_CTCPSNDC,
	XP_TE_DCCCHATABORT,		XP_TE_DCCCONCHAT,
	XP_TE_DCCCHATF,		XP_TE_DCCCHATOFFER,
	XP_TE_DCCCHATOFFERING,		XP_TE_DCCCHATREOFFER,
	XP_TE_DCCCONFAIL,		XP_TE_DCCGENERICOFFER,
	XP_TE_DCCHEAD,		XP_TE_MALFORMED,
	XP_TE_DCCOFFER,		XP_TE_DCCIVAL,
	XP_TE_DCCRECVABORT,		XP_TE_DCCRECVCOMP,
	XP_TE_DCCCONRECV,		XP_TE_DCCRECVERR,
	XP_TE_DCCFILEERR,		XP_TE_DCCRENAME,
	XP_TE_DCCRESUMEREQUEST,		XP_TE_DCCSENDABORT,
	XP_TE_DCCSENDCOMP,		XP_TE_DCCCONSEND,
	XP_TE_DCCSENDFAIL,		XP_TE_DCCSENDOFFER,
	XP_TE_DCCSTALL,		XP_TE_DCCTOUT,
	XP_TE_DELNOTIFY,		XP_TE_DISCON,
	XP_TE_FOUNDIP,		XP_TE_GENMSG,
	XP_TE_IGNOREADD,		XP_TE_IGNORECHANGE,
	XP_TE_IGNOREFOOTER,		XP_TE_IGNOREHEADER,
	XP_TE_IGNOREREMOVE,		XP_TE_IGNOREEMPTY,
	XP_TE_INVITE,		XP_TE_INVITED,
	XP_TE_JOIN,		XP_TE_KEYWORD,
	XP_TE_KICK,		XP_TE_KILL,
	XP_TE_MSGSEND,		XP_TE_MOTD,
	XP_TE_MOTDSKIP,		XP_TE_NICKCLASH,
	XP_TE_NICKERROR,		XP_TE_NICKFAIL,
	XP_TE_NODCC,		XP_TE_NOCHILD,
	XP_TE_NOTICE,		XP_TE_NOTICESEND,
	XP_TE_NOTIFYAWAY,		XP_TE_NOTIFYBACK,
	XP_TE_NOTIFYEMPTY,		XP_TE_NOTIFYHEAD,
	XP_TE_NOTIFYNUMBER,		XP_TE_NOTIFYOFFLINE,
	XP_TE_NOTIFYONLINE,		XP_TE_OPENDIALOG,
	XP_TE_PART,		XP_TE_PARTREASON,
	XP_TE_PINGREP,		XP_TE_PINGTIMEOUT,
	XP_TE_PRIVACTION,		XP_TE_DPRIVACTION,
	XP_TE_PRIVMSG,		XP_TE_DPRIVMSG,
	XP_TE_ALREADYPROCESS,		XP_TE_QUIT,
	XP_TE_RAWMODES,		XP_TE_WALLOPS,
	XP_TE_RESOLVINGUSER,		XP_TE_SASLAUTH,
	XP_TE_SASLRESPONSE,		XP_TE_SERVERCONNECTED,
	XP_TE_SERVERERROR,		XP_TE_SERVERLOOKUP,
	XP_TE_SERVNOTICE,		XP_TE_SERVTEXT,
	XP_TE_SSLMESSAGE,		XP_TE_STOPCONNECT,
	XP_TE_TOPIC,		XP_TE_NEWTOPIC,
	XP_TE_TOPICDATE,		XP_TE_UKNHOST,
	XP_TE_USERLIMIT,		XP_TE_USERSONCHAN,
	XP_TE_WHOIS_AUTH,		XP_TE_WHOIS5,
	XP_TE_WHOIS2,		XP_TE_WHOIS6,
	XP_TE_WHOIS_ID,		XP_TE_WHOIS4,
	XP_TE_WHOIS4T,		XP_TE_WHOIS1,
	XP_TE_WHOIS_REALHOST,		XP_TE_WHOIS3,
	XP_TE_WHOIS_SPECIAL,		XP_TE_UJOIN,
	XP_TE_UKICK,		XP_TE_UPART,
	XP_TE_UPARTREASON,		XP_TE_UACTION,
	XP_TE_UINVITE,		XP_TE_UCHANMSG,
	XP_TE_UCHANGENICK,
	NUM_XP
};

/* timestamp is non-zero if we are using server-time */
#define EMIT_SIGNAL_TIMESTAMP(i, sess, a, b, c, d, e, timestamp) \
	text_emit(i, sess, a, b, c, d, timestamp)
#define EMIT_SIGNAL(i, sess, a, b, c, d, e) \
	text_emit(i, sess, a, b, c, d, 0)
void text_emit (int index, session *sess, const char *a, const char *b, const char *c, const char *d, time_t timestamp);

#define IG_PRIV	1
#define IG_NOTI	2
#define IG_CHAN	4
#define IG_CTCP	8
#define IG_INVI	16
#define IG_UNIG	32
#define IG_NOSAVE	64
#define IG_DCC		128

struct ignore {
	char *mask;
	unsigned int type;	/* one of more of IG_* ORed together */
};

}
#endif
