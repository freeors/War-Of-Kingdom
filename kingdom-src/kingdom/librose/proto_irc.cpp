/* X-Chat
 * Copyright (C) 2002 Peter Zelezny.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

/* IRC RFC1459(+commonly used extensions) protocol implementation */

#include "global.hpp"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>

#ifndef WIN32
#include <sys/time.h>
#include <unistd.h>
#else
#include <WinSock.h>
#endif

#include "proto_irc.hpp"
#include "wml_exception.hpp"

#include <algorithm>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <set>

namespace irc 
{
struct hexchatprefs prefs;
void safe_strcpy(char *dest, const char *src, int bytes_left);
int ignore_check(char *host, int type);
int rfc_casecmp (const char *s1, const char *s2);
bool chanopt_is_set (unsigned int global, unsigned char per_chan_setting);
bool is_session(session * sess);
void inbound_notice(server *serv, char *to, char *nick, char *msg, char *ip, int id, const message_tags_data *tags_data);
void inbound_login_start(session *sess, char *nick, char *servname, const message_tags_data *tags_data);
void inbound_ujoin(server* serv, char* chan, char* nick, char* ip, const message_tags_data* tags_data);
void inbound_join(server* serv, char* chan, char* user, char* ip, char* account, char* realname, const message_tags_data* tags_data);
void inbound_005(server* serv, char *word[], const message_tags_data* tags_data);
void inbound_login_end(session *sess, char *text, const message_tags_data *tags_data);
void inbound_next_nick(session* sess, char* nick, int error, const message_tags_data *tags_data);
void inbound_topic(server* serv, char* chan, char* topic_text, const message_tags_data* tags_data);
void inbound_ping_reply(session* sess, char* timestring, char* from, const message_tags_data* tags_data);
void inbound_nameslist(server* serv, char *chan, char *names, const message_tags_data *tags_data);
bool inbound_nameslist_end(server* serv, char* chan, const message_tags_data* tags_data);
void inbound_user_info (session *sess, char *chan, char *user, char *host,
						 char *servname, char *nick, char *realname,
						 char *account, unsigned int away,
						 const message_tags_data *tags_data);
void inbound_privmsg(server* serv, char* from, char* ip, char* text, int id, const message_tags_data *tags_data);
void inbound_chanmsg(server* serv, session* sess, char* chan, char *from, char *text, char fromme, int id, const message_tags_data *tags_data);
void inbound_upart(server *serv, char *chan, char *ip, char *reason, const message_tags_data *tags_data);
void inbound_part(server* serv, char* chan, char* user, char* ip, char* reason, const message_tags_data *tags_data);
void inbound_quit(server *serv, char *nick, char *ip, char *reason, const message_tags_data *tags_data);
void fe_chan_list_start(server *serv);
void fe_add_chan_list(server *serv, char *chan, char *users, char *topic);
void fe_chan_list_end(server *serv);
void notify_set_offline_list(server * serv, char *users, int quiet, const message_tags_data *tags_data);
void notify_set_online_list(server * serv, char *users, const message_tags_data *tags_data);
void chan_forbid_join(server* serv, char* chan, char* reason);

static const char* nul_chars = "\0";
#define fill_params_5(serv, p1, p2, p3, p4, p5)	\
	(serv).params[0] = p1;	\
	(serv).params[1] = p2;	\
	(serv).params[2] = p3;	\
	(serv).params[3] = p4;	\
	(serv).params[4] = p5
	
static void irc_login (server *serv, const char *user, const char *realname)
{
	// tcp_sendf(serv, "CAP LS\r\n");		/* start with CAP LS as Charybdis sasl.txt suggests */
	serv->sent_capend = false;	/* track if we have finished */

	if (serv->password[0] && serv->loginmethod == LOGIN_PASS) {
		tcp_sendf(serv, "PASS %s\r\n", serv->password);
	}

	tcp_sendf(serv,
				  "NICK %s\r\n"
				  "USER %s %s %s :%s\r\n",
				  serv->nick, user, user, serv->servername, realname);
}

static void
irc_nickserv (server *serv, char *cmd, char *arg1, char *arg2, char *arg3)
{
	/* are all ircd authors idiots? */
	switch (serv->loginmethod)
	{
		case LOGIN_MSG_NICKSERV:
			tcp_sendf (serv, "PRIVMSG NICKSERV :%s %s%s%s\r\n", cmd, arg1, arg2, arg3);
			break;
		case LOGIN_NICKSERV:
			tcp_sendf (serv, "NICKSERV %s %s%s%s\r\n", cmd, arg1, arg2, arg3);
			break;
		default: /* This may not work but at least it tries something when using /id or /ghost cmd */
			tcp_sendf (serv, "NICKSERV %s %s%s%s\r\n", cmd, arg1, arg2, arg3);
			break;
#if 0
		case LOGIN_MSG_NS:
			tcp_sendf (serv, "PRIVMSG NS :%s %s%s%s\r\n", cmd, arg1, arg2, arg3);
			break;
		case LOGIN_NS:
			tcp_sendf (serv, "NS %s %s%s%s\r\n", cmd, arg1, arg2, arg3);
			break;
		case LOGIN_AUTH:
			/* why couldn't QuakeNet implement one of the existing ones? */
			tcp_sendf (serv, "AUTH %s %s\r\n", arg1, arg2);
			break;
#endif
	}
}

static void
irc_ns_identify (server *serv, char *pass)
{
	switch (serv->loginmethod)
	{
		case LOGIN_CHALLENGEAUTH:
			tcp_sendf (serv, "PRIVMSG %s :CHALLENGE\r\n", CHALLENGEAUTH_NICK);	/* request a challenge from Q */
			break;
#if 0
		case LOGIN_AUTH:
			irc_nickserv (serv, "", serv->nick, pass, "");
			break;
#endif
		default:
			irc_nickserv (serv, "IDENTIFY", pass, "", "");
	}
}

static void
irc_ns_ghost (server *serv, char *usname, char *pass)
{
	if (serv->loginmethod != LOGIN_CHALLENGEAUTH)
	{
		irc_nickserv (serv, "GHOST", usname, " ", pass);
	}
}

static void
irc_join (server *serv, const char *channel, const char *key)
{
	if (key[0])
		tcp_sendf (serv, "JOIN %s %s\r\n", channel, key);
	else
		tcp_sendf (serv, "JOIN %s\r\n", channel);
}

static void
irc_join_list_flush(server *serv, char* channels, char* keys, int send_keys)
{
	if (send_keys) {
		tcp_sendf(serv, "JOIN %s %s\r\n", channels, keys);	/* send the actual command */
	} else {
		tcp_sendf(serv, "JOIN %s\r\n", channels);	/* send the actual command */
	}
}

/* Join a whole list of channels & keys, split to multiple lines
 * to get around the 512 limit.
 */

static void irc_join_list(server *serv, const std::list<favchannel*>& favorites)
{
	int first_item = 1;										/* determine whether we add commas or not */
	int send_keys = 0;										/* if none of our channels have keys, we can omit the 'x' fillers altogether */
	int len = 9;											/* JOIN<space>channels<space>keys\r\n\0 */
	char* buf = (char*)malloc(2048);
	char* chanlist = buf;
	char* keylist = buf + 1024;
	int chanlist_pos = 0, keylist_pos = 0;

	for (std::list<favchannel*>::const_iterator it = favorites.begin(); it != favorites.end(); ++ it) {
		favchannel* fav = *it;

		len += strlen(fav->name);
		if (fav->key) {
			len += strlen(fav->key);
		}

		if (len >= 512) { /* command length exceeds the IRC hard limit, flush it and start from scratch */
			chanlist[chanlist_pos] = '\0';
			keylist[keylist_pos] = '\0';
			irc_join_list_flush (serv, chanlist, keylist, send_keys);

			chanlist_pos = 0;
			keylist_pos = 0;

			len = 9;
			first_item = 1;									/* list dumped, omit commas once again */
			send_keys = 0;									/* also omit keys until we actually find one */
		}

		if (!first_item) {
			/* This should be done before the length check, but channel names
			 * are already at least 2 characters long so it would trigger the
			 * flush anyway.
			 */
			len += 2;

			/* add separators but only if it's not the 1st element */
			chanlist[chanlist_pos ++] = ',';
			keylist[keylist_pos ++] = ',';
		}

		memcpy(chanlist + chanlist_pos, fav->name, strlen(fav->name));
		chanlist_pos += strlen(fav->name);

		if (fav->key) {
			memcpy(keylist + keylist_pos, fav->key, strlen(fav->key));
			keylist_pos += strlen(fav->key);

			send_keys = 1;
		} else {
			keylist[keylist_pos ++] = 'x'; /* 'x' filler for keyless channels so that our JOIN command is always well-formatted */
		}

		first_item = 0;
	}

	chanlist[chanlist_pos] = '\0';
	keylist[keylist_pos] = '\0';
	irc_join_list_flush (serv, chanlist, keylist, send_keys);
	free(buf);
}

static void
irc_part (server *serv, const char *channel, const char *reason)
{
	if (reason[0])
		tcp_sendf (serv, "PART %s :%s\r\n", channel, reason);
	else
		tcp_sendf (serv, "PART %s\r\n", channel);
}

static void
irc_quit (server *serv, char *reason)
{
	if (reason[0])
		tcp_sendf (serv, "QUIT :%s\r\n", reason);
	else
		serv->sock->tcp_send_len("QUIT\r\n", 6);
}

static void
irc_set_back (server *serv)
{
	serv->sock->tcp_send_len("AWAY\r\n", 6);
}

static void
irc_set_away (server *serv, char *reason)
{
	if (reason)
	{
		if (!reason[0])
			reason = " ";
	}
	else
	{
		reason = " ";
	}

	tcp_sendf (serv, "AWAY :%s\r\n", reason);
}

static void
irc_ctcp (server *serv, char *to, char *msg)
{
	tcp_sendf (serv, "PRIVMSG %s :\001%s\001\r\n", to, msg);
}

static void
irc_nctcp (server *serv, char *to, char *msg)
{
	tcp_sendf (serv, "NOTICE %s :\001%s\001\r\n", to, msg);
}

static void
irc_cycle (server *serv, char *channel, char *key)
{
	tcp_sendf (serv, "PART %s\r\nJOIN %s %s\r\n", channel, channel, key);
}

static void
irc_kick (server *serv, char *channel, char *nick, char *reason)
{
	if (reason[0])
		tcp_sendf (serv, "KICK %s %s :%s\r\n", channel, nick, reason);
	else
		tcp_sendf (serv, "KICK %s %s\r\n", channel, nick);
}

static void
irc_invite (server *serv, char *channel, char *nick)
{
	tcp_sendf (serv, "INVITE %s %s\r\n", nick, channel);
}

static void
irc_mode (server *serv, char *target, char *mode)
{
	tcp_sendf (serv, "MODE %s %s\r\n", target, mode);
}

/* find channel info when joined */

static void
irc_join_info (server *serv, const char *channel)
{
	tcp_sendf (serv, "MODE %s\r\n", channel);
}

/* initiate userlist retreival */

static void
irc_user_list (server *serv, const char *channel)
{
	if (serv->have_whox)
		tcp_sendf (serv, "WHO %s %%chtsunfra,152\r\n", channel);
	else
		tcp_sendf (serv, "WHO %s\r\n", channel);
}

/* userhost */

static void
irc_userhost (server *serv, char *nick)
{
	tcp_sendf (serv, "USERHOST %s\r\n", nick);
}

static void
irc_away_status (server *serv, const char *channel)
{
	if (serv->have_whox)
		tcp_sendf (serv, "WHO %s %%chtsunfra,152\r\n", channel);
	else
		tcp_sendf (serv, "WHO %s\r\n", channel);
}

/*static void
irc_get_ip (server *serv, char *nick)
{
	tcp_sendf (serv, "WHO %s\r\n", nick);
}*/


/*
 *  Command: WHOIS
 *     Parameters: [<server>] <nickmask>[,<nickmask>[,...]]
 */
static void
irc_user_whois (server *serv, char *nicks)
{
	tcp_sendf (serv, "WHOIS %s\r\n", nicks);
}

static void
irc_message (server *serv, const char *channel, const char *text)
{
	tcp_sendf (serv, "PRIVMSG %s :%s\r\n", channel, text);
}

static void
irc_action (server *serv, char *channel, char *act)
{
	tcp_sendf (serv, "PRIVMSG %s :\001ACTION %s\001\r\n", channel, act);
}

static void
irc_notice (server *serv, char *channel, char *text)
{
	tcp_sendf (serv, "NOTICE %s :%s\r\n", channel, text);
}

static void
irc_topic (server *serv, char *channel, char *topic)
{
	if (!topic)
		tcp_sendf (serv, "TOPIC %s :\r\n", channel);
	else if (topic[0])
		tcp_sendf (serv, "TOPIC %s :%s\r\n", channel, topic);
	else
		tcp_sendf (serv, "TOPIC %s\r\n", channel);
}

static void
irc_list_channels (server *serv, const char *arg, int min_users)
{
	if (arg[0])
	{
		tcp_sendf (serv, "LIST %s\r\n", arg);
		return;
	}

	if (serv->use_listargs)
		tcp_sendf (serv, "LIST >%d,<10000\r\n", min_users - 1);
	else
		serv->sock->tcp_send_len("LIST\r\n", 6);
}

static void
irc_names (server *serv, char *channel)
{
	tcp_sendf (serv, "NAMES %s\r\n", channel);
}

static void
irc_change_nick (server *serv, const char *new_nick)
{
	tcp_sendf (serv, "NICK %s\r\n", new_nick);
}

static void
irc_ping (server *serv, const char *to, const char *timestring)
{
	if (*to) {
		tcp_sendf (serv, "PRIVMSG %s :\001PING %s\001\r\n", to, timestring);
	} else {
		tcp_sendf (serv, "PING %s\r\n", timestring);
	}
}

static void
irc_monitor (server* serv, const char* nick, bool add)
{
	char addchar = '+';

	if (!add) {
		addchar = '-';
	}

	if (*nick) {
		tcp_sendf(serv, "MONITOR %c %s\r\n", addchar, nick);
	}
}

static int
irc_raw (server *serv, char *raw)
{
	int len;
	char tbuf[4096];
	if (*raw)
	{
		len = strlen (raw);
		if (len < sizeof (tbuf) - 3) {
			len = snprintf(tbuf, sizeof(tbuf), "%s\r\n", raw);
			serv->sock->tcp_send_len(tbuf, len);
		} else {
			serv->sock->tcp_send_len(raw, len);
			serv->sock->tcp_send_len("\r\n", 2);
		}
		return true;
	}
	return false;
}

/* ============================================================== */
/* ======================= IRC INPUT ============================ */
/* ============================================================== */


static void
channel_date (session *sess, char *chan, char *timestr,
				  const message_tags_data *tags_data)
{
	time_t timestamp = (time_t) atol (timestr);
	char *tim = ctime (&timestamp);
	tim[24] = 0;	/* get rid of the \n */
	EMIT_SIGNAL_TIMESTAMP (XP_TE_CHANDATE, sess, chan, tim, NULL, NULL, 0,
								  tags_data->timestamp);
}

static void
process_numeric(session * sess, const char* buf, int n, char *word[], char *word_eol[], char *text, const message_tags_data *tags_data)
{
	server *serv = sess->server;
	/* show whois is the server tab */
	session *whois_sess = serv->server_session;
	
	/* unless this setting is on */
	if (prefs.hex_irc_whois_front)
		whois_sess = serv->sess_list.front();

	switch (n)
	{
	case 1:
		inbound_login_start(sess, word[3], word[1], tags_data);
		/* if network is PTnet then you must get your IP address
			from "001" server message */
		if ((strncmp(word[7], "PTnet", 5) == 0) &&
			(strncmp(word[8], "IRC", 3) == 0) &&
			(strncmp(word[9], "Network", 7) == 0) &&
			(strrchr(word[10], '@') != NULL))
		{
			serv->use_who = false;
			if (prefs.hex_dcc_ip_from_server) {
				// dbg!! inbound_foundip (sess, strrchr(word[10], '@')+1, tags_data);
			}
		}

		goto def;

	case 4:	/* check the ircd type */
		serv->use_listargs = false;
		serv->modes_per_line = 3;		/* default to IRC RFC */
		if (strncmp (word[5], "bahamut", 7) == 0)				/* DALNet */
		{
			serv->use_listargs = true;		/* use the /list args */
		} else if (strncmp (word[5], "u2.10.", 6) == 0)		/* Undernet */
		{
			serv->use_listargs = true;		/* use the /list args */
			serv->modes_per_line = 6;		/* allow 6 modes per line */
		} else if (strncmp (word[5], "glx2", 4) == 0)
		{
			serv->use_listargs = true;		/* use the /list args */
		}
		return;

	case 5:
		inbound_005 (serv, word, tags_data);
		goto def;

	case 263:	/*Server load is temporarily too heavy */
		/* dbg!!
		if (fe_is_chanwindow (sess->server)) {
			fe_chan_list_end (sess->server);
			fe_message (word_eol[4], FE_MSG_ERROR);
		}
		*/
		goto def;

	case 301:
		/* dbg!!
		inbound_away (serv, word[4],
						  (word_eol[5][0] == ':') ? word_eol[5] + 1 : word_eol[5],
						  tags_data);
		*/
		break;

	case 302:
		if (serv->skip_next_userhost)
		{
			char *eq = strchr (word[4], '=');
			if (eq)
			{
				*eq = 0;
				if (!serv->p_cmp (word[4] + 1, serv->nick))
				{
					char *at = strrchr (eq + 1, '@');
					if (at) {
						// dbg!! inbound_foundip (sess, at + 1, tags_data);
					}
				}
			}

			serv->skip_next_userhost = false;
			break;
		}
		else goto def;

	case 303:
		word[4]++;
		// dbg!! notify_markonline (serv, word, tags_data);
		break;

	case 305:
		// dbg!! inbound_uback (serv, tags_data);
		goto def;

	case 306:
		// dbg!! inbound_uaway (serv, tags_data);
		goto def;

	case 312:
		if (!serv->skip_next_whois) {
			EMIT_SIGNAL_TIMESTAMP (XP_TE_WHOIS3, whois_sess, word[4], word_eol[5], NULL, NULL, 0, tags_data->timestamp);
		} else {
			inbound_user_info (sess, NULL, NULL, NULL, word[5], word[4], NULL, NULL, 0xff, tags_data);
		}
		break;

	case 311:	/* WHOIS 1st line */
		serv->inside_whois = 1;
		// dbg!! inbound_user_info_start (sess, word[4], tags_data);
		if (!serv->skip_next_whois) {
			EMIT_SIGNAL_TIMESTAMP (XP_TE_WHOIS1, whois_sess, word[4], word[5],
										  word[6], word_eol[8] + 1, 0, tags_data->timestamp);
		} else {
			inbound_user_info (sess, NULL, word[5], word[6], NULL, word[4],
									 word_eol[8][0] == ':' ? word_eol[8] + 1 : word_eol[8],
									 NULL, 0xff, tags_data);
		}
		break;

	case 314:	/* WHOWAS */
		// dbg!! inbound_user_info_start (sess, word[4], tags_data);
		EMIT_SIGNAL_TIMESTAMP (XP_TE_WHOIS1, whois_sess, word[4], word[5],
									  word[6], word_eol[8] + 1, 0, tags_data->timestamp);
		break;

	case 317:
		if (!serv->skip_next_whois)
		{
			time_t timestamp = (time_t) atol (word[6]);
			long idle = atol (word[5]);
			char *tim;
			char outbuf[64];

			snprintf (outbuf, sizeof (outbuf),
						"%02ld:%02ld:%02ld", idle / 3600, (idle / 60) % 60,
						idle % 60);
			if (timestamp == 0)
				EMIT_SIGNAL_TIMESTAMP (XP_TE_WHOIS4, whois_sess, word[4],
											  outbuf, NULL, NULL, 0, tags_data->timestamp);
			else
			{
				tim = ctime (&timestamp);
				tim[19] = 0; 	/* get rid of the \n */
				EMIT_SIGNAL_TIMESTAMP (XP_TE_WHOIS4T, whois_sess, word[4],
											  outbuf, tim, NULL, 0, tags_data->timestamp);
			}
		}
		break;

	case 318:	/* END OF WHOIS */
		if (!serv->skip_next_whois)
			EMIT_SIGNAL_TIMESTAMP (XP_TE_WHOIS6, whois_sess, word[4], NULL,
										  NULL, NULL, 0, tags_data->timestamp);
		serv->skip_next_whois = 0;
		serv->inside_whois = 0;
		break;

	case 313:
	case 319:
		if (!serv->skip_next_whois)
			EMIT_SIGNAL_TIMESTAMP (XP_TE_WHOIS2, whois_sess, word[4],
										  word_eol[5] + 1, NULL, NULL, 0,
										  tags_data->timestamp);
		break;

	case 307:	/* dalnet version */
	case 320:	/* :is an identified user */
		if (!serv->skip_next_whois)
			EMIT_SIGNAL_TIMESTAMP (XP_TE_WHOIS_ID, whois_sess, word[4],
										  word_eol[5] + 1, NULL, NULL, 0,
										  tags_data->timestamp);
		break;

	case 321:
		fe_chan_list_start(sess->server);
		break;

	case 322:
		fe_add_chan_list(sess->server, word[4], word[5], word_eol[6] + 1);

		break;

	case 323:
		fe_chan_list_end(sess->server);
		break;

	case 324:
		sess = find_channel (serv, word[4]);
		if (!sess)
			sess = serv->server_session;
		if (sess->ignore_mode)
			sess->ignore_mode = false;
		else
			EMIT_SIGNAL_TIMESTAMP (XP_TE_CHANMODES, sess, word[4], word_eol[5],
										  NULL, NULL, 0, tags_data->timestamp);
		// dbg!! fe_update_mode_buttons (sess, 'c', '-');
		// dbg!! fe_update_mode_buttons (sess, 'r', '-');
		// dbg!! fe_update_mode_buttons (sess, 't', '-');
		// dbg!! fe_update_mode_buttons (sess, 'n', '-');
		// dbg!! fe_update_mode_buttons (sess, 'i', '-');
		// dbg!! fe_update_mode_buttons (sess, 'm', '-');
		// dbg!! fe_update_mode_buttons (sess, 'l', '-');
		// dbg!! fe_update_mode_buttons (sess, 'k', '-');
		// dbg!! handle_mode(serv, word, word_eol, "", true, tags_data);
		break;

	case 328: /* channel url */
		sess = find_channel (serv, word[4]);
		if (sess)
		{
			EMIT_SIGNAL_TIMESTAMP (XP_TE_CHANURL, sess, word[4], word[5] + 1,
									NULL, NULL, 0, tags_data->timestamp); 
		}
		break;

	case 329:
		sess = find_channel (serv, word[4]);
		if (sess)
		{
			if (sess->ignore_date)
				sess->ignore_date = false;
			else
				channel_date (sess, word[4], word[5], tags_data);
		}
		break;

	case 330:
		if (!serv->skip_next_whois)
			EMIT_SIGNAL_TIMESTAMP (XP_TE_WHOIS_AUTH, whois_sess, word[4],
										  word_eol[6] + 1, word[5], NULL, 0,
										  tags_data->timestamp);
		inbound_user_info (sess, NULL, NULL, NULL, NULL, word[4], NULL, word[5], 0xff, tags_data);
		break;

	case 332:
		inbound_topic (serv, word[4],
							(word_eol[5][0] == ':') ? word_eol[5] + 1 : word_eol[5],
							tags_data);
		break;

	case 333:
		// dbg!! inbound_topictime (serv, word[4], word[5], atol (word[6]), tags_data);
		break;

#if 0
	case 338:  /* Undernet Real user@host, Real IP */
		EMIT_SIGNAL_TIMESTAMP (XP_TE_WHOIS_REALHOST, sess, word[4], word[5], word[6], 
									  (word_eol[7][0]==':') ? word_eol[7]+1 : word_eol[7],
									  0, tags_data->timestamp);
		break;
#endif

	case 341:						  /* INVITE ACK */
		EMIT_SIGNAL_TIMESTAMP (XP_TE_UINVITE, sess, word[4], word[5],
									  serv->servername, NULL, 0, tags_data->timestamp);
		break;

	case 352:						  /* WHO */
		{
			unsigned int away = 0;
			session *who_sess = find_channel (serv, word[4]);

			if (*word[9] == 'G') {
				away = 1;
			}

			inbound_user_info (sess, word[4], word[5], word[6], word[7],
									 word[8], word_eol[11], NULL, away,
									 tags_data);
			/* try to show only user initiated whos */
			if (!who_sess || !who_sess->doing_who)
				EMIT_SIGNAL_TIMESTAMP (XP_TE_SERVTEXT, serv->server_session, text, word[1],
											  word[2], NULL, 0, tags_data->timestamp);
		}
		break;

	case 354:	/* undernet WHOX: used as a reply for irc_away_status */
		{
			unsigned int away = 0;
			session *who_sess;

			/* irc_away_status and irc_user_list sends out a "152" */
			if (!strcmp (word[4], "152")) {
				who_sess = find_channel(serv, word[5]);

				if (*word[10] == 'G') {
					away = 1;
				}

				/* :server 354 yournick 152 #channel ~ident host servname nick H account :realname */
				inbound_user_info(sess, word[5], word[6], word[7], word[8],
										 word[9], word_eol[12]+1, word[11], away,
										 tags_data);

				/* try to show only user initiated whos */
				if (!who_sess || !who_sess->doing_who)
					EMIT_SIGNAL_TIMESTAMP (XP_TE_SERVTEXT, serv->server_session, text,
												  word[1], word[2], NULL, 0,
												  tags_data->timestamp);
			} else
				goto def;
		}
		break;

	case 315:						  /* END OF WHO */
		{
			session *who_sess;
			who_sess = find_channel (serv, word[4]);
			if (who_sess)
			{
				if (!who_sess->doing_who)
					EMIT_SIGNAL_TIMESTAMP (XP_TE_SERVTEXT, serv->server_session, text,
												  word[1], word[2], NULL, 0,
												  tags_data->timestamp);
				who_sess->doing_who = false;
			} else
			{
				if (!serv->doing_dns)
					EMIT_SIGNAL_TIMESTAMP (XP_TE_SERVTEXT, serv->server_session, text,
												  word[1], word[2], NULL, 0, tags_data->timestamp);
				serv->doing_dns = false;
			}
		}
		break;

	case 346:	/* +I-list entry */
		/* dbg!!
		if (!inbound_banlist (sess, atol (word[7]), word[4], word[5], word[6], 346, tags_data)) {
			goto def;
		}
		*/
		break;

	case 347:	/* end of invite list */
		/* dbg!!
		if (!fe_ban_list_end (sess, 347)) {
			goto def;
		}
		*/
		break;

	case 348:	/* +e-list entry */
		/* dbg!!
		if (!inbound_banlist (sess, atol (word[7]), word[4], word[5], word[6], 348, tags_data)) {
			goto def;
		}
		*/
		break;

	case 349:	/* end of exemption list */
		sess = find_channel (serv, word[4]);
		if (!sess)
			goto def;
		/* dbg!!
		if (!fe_ban_list_end (sess, 349)) {
			goto def;
		}
		*/
		break;

	case 353:						  /* NAMES */
		inbound_nameslist(serv, word[5],
								 (word_eol[6][0] == ':') ? word_eol[6] + 1 : word_eol[6],
								 tags_data);
		break;

	case 366:
		if (!inbound_nameslist_end(serv, word[4], tags_data)) {
			goto def;
		}
		break;

	case 367: /* banlist entry */
		/* dbg!!
		if (!inbound_banlist (sess, atol (word[7]), word[4], word[5], word[6], 367, tags_data)) {
			goto def;
		}
		*/
		break;

	case 368:
		sess = find_channel (serv, word[4]);
		if (!sess)
			goto def;
		/* dbg!!
		if (!fe_ban_list_end (sess, 368)) {
			goto def;
		}
		*/
		break;

	case 369:	/* WHOWAS end */
	case 406:	/* WHOWAS error */
		EMIT_SIGNAL_TIMESTAMP (XP_TE_SERVTEXT, whois_sess, text, word[1], word[2],
									  NULL, 0, tags_data->timestamp);
		serv->inside_whois = 0;
		break;

	case 372:	/* motd text */
	case 375:	/* motd start */
		if (!prefs.hex_irc_skip_motd || serv->motd_skipped)
			EMIT_SIGNAL_TIMESTAMP (XP_TE_MOTD, serv->server_session, text, NULL,
										  NULL, NULL, 0, tags_data->timestamp);
		break;

	case 376:	/* end of motd */
	case 422:	/* motd file is missing */
		inbound_login_end(sess, text, tags_data);
		break;

	case 432:	/* erroneous nickname */
		if (serv->end_of_motd) {
			goto def;
		}
		inbound_next_nick(sess, word[4], 1, tags_data);
		break;

	case 433:	/* nickname in use */
		if (serv->end_of_motd) {
			goto def;
		}
		inbound_next_nick(sess, word[4], 0, tags_data);
		break;

	case 437:
		if (serv->end_of_motd || is_channel(serv, word[4]))
			goto def;
		// dbg!! inbound_next_nick (sess, word[4], 0, tags_data);
		break;

	case 471:
		EMIT_SIGNAL_TIMESTAMP (XP_TE_USERLIMIT, sess, word[4], NULL, NULL, NULL, 0,
									  tags_data->timestamp);
		break;

	case 473:
		EMIT_SIGNAL_TIMESTAMP (XP_TE_INVITE, sess, word[4], NULL, NULL, NULL, 0,
									  tags_data->timestamp);
		break;

	case 474:
		EMIT_SIGNAL_TIMESTAMP (XP_TE_BANNED, sess, word[4], NULL, NULL, NULL, 0,
									  tags_data->timestamp);
		break;

	case 475:
		EMIT_SIGNAL_TIMESTAMP (XP_TE_KEYWORD, sess, word[4], NULL, NULL, NULL, 0,
									  tags_data->timestamp);
		break;

	case 477:
		// :verne.freenode.net 477 ancientcc #docker :Cannot join channel (+r) - you need to be identified with services
		chan_forbid_join(sess->server, word[4], word_eol[5] + 1);

		break;

	case 601:
		// dbg!! notify_set_offline (serv, word[4], false, tags_data);
		break;

	case 605:
		// dbg!! notify_set_offline (serv, word[4], true, tags_data);
		break;

	case 600:
	case 604:
		// dbg!! notify_set_online (serv, word[4], tags_data);
		break;

	case 728:	/* +q-list entry */
		/* NOTE:  FREENODE returns these results inconsistent with e.g. +b */
		/* Who else has imlemented MODE_QUIET, I wonder? */
		/* dbg!!
		if (!inbound_banlist (sess, atol (word[8]), word[4], word[6], word[7], 728, tags_data)) {
			goto def;
		}
		*/
		break;

	case 729:	/* end of quiet list */
		/* dbg!!
		if (!fe_ban_list_end (sess, 729)) {
			goto def;
		}
		*/
		break;

	case 730: /* RPL_MONONLINE */
		notify_set_online_list(serv, word[4] + 1, tags_data);
		break;

	case 731: /* RPL_MONOFFLINE */
		notify_set_offline_list(serv, word[4] + 1, false, tags_data);
		break;

	case 900:	/* successful SASL 'logged in as ' */
		EMIT_SIGNAL_TIMESTAMP (XP_TE_SERVTEXT, serv->server_session, 
									  word_eol[6]+1, word[1], word[2], NULL, 0,
									  tags_data->timestamp);
		break;
	case 903:	/* successful SASL auth */
	case 904:	/* failed SASL auth */
		/* dbg!!
		if (inbound_sasl_error (serv)) {
			break; // might retry
		}
		*/
	case 905:	/* failed SASL auth */
	case 906:	/* aborted */
	case 907:	/* attempting to re-auth after a successful auth */
		EMIT_SIGNAL_TIMESTAMP (XP_TE_SASLRESPONSE, serv->server_session, word[1],
									  word[2], word[3], ++word_eol[4], 0,
									  tags_data->timestamp);
		if (!serv->sent_capend)
		{
			serv->sent_capend = true;
			serv->sock->tcp_send_len("CAP END\r\n", 9);
		}
		break;
	case 908:	/* Supported SASL Mechs */
		// dbg!! inbound_sasl_supportedmechs (serv, word[4]);
		break;

	default:

		if (serv->inside_whois && word[4][0])
		{
			/* some unknown WHOIS reply, ircd coders make them up weekly */
			if (!serv->skip_next_whois)
				EMIT_SIGNAL_TIMESTAMP (XP_TE_WHOIS_SPECIAL, whois_sess, word[4],
											  (word_eol[5][0] == ':') ? word_eol[5] + 1 : word_eol[5],
											  word[2], NULL, 0, tags_data->timestamp);
			return;
		}

	def:
		{
			session *sess;
		
			if (is_channel(serv, word[4])) {
				sess = find_channel (serv, word[4]);
				if (!sess)
					sess = serv->server_session;
			} /* dbg!! else if ((sess = find_dialog(serv,word[4]))) { // user with an open dialog 
				;
			} */ else {
				sess=serv->server_session;
			}
			
			EMIT_SIGNAL_TIMESTAMP (XP_TE_SERVTEXT, sess, text, word[1], word[2],
										  NULL, 0, tags_data->timestamp);
		}
	}
}

/* handle named messages that starts with a ':' */

static void
process_named_msg(session *sess, const char* buf, char* type, char* word[], char* word_eol[], const message_tags_data *tags_data)
{
	server *serv = sess->server;
	char ip[128], nick[NICKLEN];
	char *text, *ex;
	int len = strlen (type);

	/* fill in the "ip" and "nick" buffers */
	ex = strchr (word[1], '!');
	if (!ex) {							  /* no '!', must be a server message */
		safe_strcpy (ip, word[1], sizeof (ip));
		safe_strcpy (nick, word[1], sizeof (nick));
	} else {
		safe_strcpy (ip, ex + 1, sizeof (ip));
		ex[0] = 0;
		safe_strcpy (nick, word[1], sizeof (nick));
		ex[0] = '!';
	}

	if (len == 4) {
		unsigned int t;

		t = WORDL((unsigned char)type[0], (unsigned char)type[1], (unsigned char)type[2], (unsigned char)type[3]); 	
		/* this should compile to a bunch of: CMP.L, JE ... nice & fast */
		switch (t) {
		case WORDL('J','O','I','N'):
			{
				char *chan = word[3];
				char *account = word[4];
				char *realname = word_eol[5];

				if (account && strcmp (account, "*") == 0)
					account = NULL;
				if (realname && *realname == ':')
					realname++;
				if (*chan == ':')
					chan++;
				if (!serv->p_cmp (nick, serv->nick)) {
					inbound_ujoin(serv, chan, nick, ip, tags_data);
				} else {
					inbound_join(serv, chan, nick, ip, account, realname, tags_data);
				}
			}
			return;

		case WORDL('K','I','C','K'):
			{
				char *kicked = word[4];
				char *reason = word_eol[5];
				if (*kicked)
				{
					if (*reason == ':')
						reason++;
					if (!strcmp (kicked, serv->nick)) {
	 					// dbg!! inbound_ukick (serv, word[3], nick, reason, tags_data);
					} else {
						// dbg!! inbound_kick (serv, word[3], kicked, nick, reason, tags_data);
					}
				}
			}
			return;

		case WORDL('K','I','L','L'):
			{
				char *reason = word_eol[4];
				if (*reason == ':')
					reason++;

				EMIT_SIGNAL_TIMESTAMP (XP_TE_KILL, sess, nick, reason, NULL, NULL,
											  0, tags_data->timestamp);
			}
			return;

		case WORDL('M','O','D','E'):
			// dbg!! handle_mode (serv, word, word_eol, nick, false, tags_data);	/* modes.c */
			return;

		case WORDL('N','I','C','K'):
			/* dbg!!
			inbound_newnick (serv, nick, 
								  (word_eol[3][0] == ':') ? word_eol[3] + 1 : word_eol[3],
								  false, tags_data);
			*/
			return;

		case WORDL('P','A','R','T'):
			{
				char *chan = word[3];
				char *reason = word_eol[4];

				if (*chan == ':')
					chan++;
				if (*reason == ':')
					reason++;
				if (!strcmp (nick, serv->nick)) {
					inbound_upart(serv, chan, ip, reason, tags_data);
				} else {
					inbound_part(serv, chan, nick, ip, reason, tags_data);
				}
			}
			return;

		case WORDL('P','O','N','G'):
			inbound_ping_reply(serv->server_session,
									  (word[4][0] == ':') ? word[4] + 1 : word[4],
									  word[3], tags_data);
			return;

		case WORDL('Q','U','I','T'):
			inbound_quit(serv, nick, ip,
							  (word_eol[3][0] == ':') ? word_eol[3] + 1 : word_eol[3],
							  tags_data);
			return;

		case WORDL('A','W','A','Y'):
			/* dbg!!
			inbound_away_notify (serv, nick,
										(word_eol[3][0] == ':') ? word_eol[3] + 1 : NULL,
										tags_data);
			*/
			return;
		}

		goto garbage;
	}

	else if (len >= 5)
	{
		unsigned int t;

		t = WORDL((unsigned char)type[0], (unsigned char)type[1], (unsigned char)type[2], (unsigned char)type[3]); 	
		/* this should compile to a bunch of: CMP.L, JE ... nice & fast */
		switch (t)
		{

		case WORDL('A','C','C','O'):
			// dbg!! inbound_account (serv, nick, word[3], tags_data);
			return;
			
		case WORDL('I','N','V','I'):
			if (ignore_check (word[1], IG_INVI)) {
				return;
			}
			
			if (word[4][0] == ':')
				EMIT_SIGNAL_TIMESTAMP (XP_TE_INVITED, sess, word[4] + 1, nick,
											  serv->servername, NULL, 0,
											  tags_data->timestamp);
			else
				EMIT_SIGNAL_TIMESTAMP (XP_TE_INVITED, sess, word[4], nick,
											  serv->servername, NULL, 0,
											  tags_data->timestamp);
				
			return;

		case WORDL('N','O','T','I'):
			{
				int id = false;								/* identified */

				text = word_eol[4];
				if (*text == ':')
				{
					text++;
				}

#ifdef USE_OPENSSL
				if (!strncmp (text, "CHALLENGE ", 10))		/* QuakeNet CHALLENGE upon our request */
				{
					char *response = challengeauth_response (((ircnet *)serv->network)->user ? ((ircnet *)serv->network)->user : prefs.hex_irc_user_name, serv->password, word[5]);

					tcp_sendf (serv, "PRIVMSG %s :CHALLENGEAUTH %s %s %s\r\n",
						CHALLENGEAUTH_NICK,
						((ircnet *)serv->network)->user ? ((ircnet *)serv->network)->user : prefs.hex_irc_user_name,
						response,
						CHALLENGEAUTH_ALGO);

					free(response);
					return;									/* omit the CHALLENGE <hash> ALGOS message */
				}
#endif

				if (serv->have_idmsg)
				{
					if (*text == '+')
					{
						id = true;
						text++;
					} else if (*text == '-')
						text++;
				}

				if (!ignore_check (word[1], IG_NOTI)) {
					inbound_notice (serv, word[3], nick, text, ip, id, tags_data);
				}
			}
			return;

		case WORDL('P','R','I','V'):
			{
				char *to = word[3];
				int len;
				int id = false;	/* identified */
				if (*to) {
					/* Handle limited channel messages, for now no special event */
					if (strchr (serv->chantypes, to[0]) == NULL && strchr (serv->nick_prefixes, to[0]) != NULL) {
						to ++;
					}
						
					text = word_eol[4];
					if (*text == ':') {
						text++;
					}
					if (serv->have_idmsg) {
						if (*text == '+') {
							id = true;
							text ++;
						} else if (*text == '-') {
							text ++;
						}
					}
					len = strlen (text);
					if (text[0] == 1 && text[len - 1] == 1)	 { /* ctcp */
						text[len - 1] = 0;
						text++;
						/* dbg!!
						if (strncasecmp (text, "ACTION", 6) != 0) {
							flood_check (nick, ip, serv, sess, 0);
						}
						*/
						if (strncasecmp (text, "DCC ", 4) == 0) {
							// redo this with handle_quotes true
							process_data_init (word[1], word_eol[1], word, word_eol, true, false);
						}
						// dbg!! ctcp_handle (sess, to, nick, ip, text, word, word_eol, id, tags_data);
					} else {
						if (is_channel(serv, to)) {
							if (ignore_check (word[1], IG_CHAN)) {
								return;
							}
							inbound_chanmsg(serv, NULL, to, nick, text, false, id, tags_data);
						} else {
							if (ignore_check (word[1], IG_PRIV)) {
								return;
							}
							inbound_privmsg(serv, nick, ip, text, id, tags_data);
						}
					}
				}
			}
			return;

		case WORDL('T','O','P','I'):
			/* dbg!!
			inbound_topicnew (serv, nick, word[3],
									(word_eol[4][0] == ':') ? word_eol[4] + 1 : word_eol[4],
									tags_data);
			*/
			return;

		case WORDL('W','A','L','L'):
			text = word_eol[3];
			if (*text == ':')
				text++;
			EMIT_SIGNAL_TIMESTAMP (XP_TE_WALLOPS, sess, nick, text, NULL, NULL, 0,
										  tags_data->timestamp);
			return;
		}
	}

	else if (len == 3)
	{
		unsigned int t;

		t = WORDL((unsigned char)type[0], (unsigned char)type[1], (unsigned char)type[2], (unsigned char)type[3]);
		switch (t)
		{
			case WORDL('C','A','P','\0'):
				if (strncasecmp (word[4], "ACK", 3) == 0) {
					/* dbg!!
					inbound_cap_ack (serv, word[1], 
										  word[5][0] == ':' ? word_eol[5] + 1 : word_eol[5],
										  tags_data);
					*/
				} else if (strncasecmp (word[4], "LS", 2) == 0) {
					/* dbg!!
					inbound_cap_ls (serv, word[1], 
										 word[5][0] == ':' ? word_eol[5] + 1 : word_eol[5],
										 tags_data);
					*/
				} else if (strncasecmp (word[4], "NAK", 3) == 0) {
					// dbg!! inbound_cap_nak (serv, tags_data);
				} else if (strncasecmp (word[4], "LIST", 4) == 0) {
					/* dbg!!
					inbound_cap_list (serv, word[1], 
											word[5][0] == ':' ? word_eol[5] + 1 : word_eol[5],
											tags_data);
					*/
				}

				return;
		}
	}

garbage:
	/* unknown message */
	// dbg!! PrintTextTimeStampf (sess, tags_data->timestamp, "GARBAGE: %s\n", word_eol[1]);
	;
}

/* handle named messages that DON'T start with a ':' */

static void
process_named_servermsg(session *sess, const char *buf, char *rawname, char *word_eol[], const message_tags_data *tags_data)
{
	server* serv = sess->server;
	sess = sess->server->server_session;

	if (!strncmp (buf, "PING ", 5)) {
		tcp_sendf (sess->server, "PONG %s\r\n", buf + 5);
		return;
	}
	if (!strncmp (buf, "ERROR", 5)) {
		EMIT_SIGNAL_TIMESTAMP (XP_TE_SERVERERROR, sess, buf + 7, NULL, NULL, NULL,
									  0, tags_data->timestamp);
		return;
	}
	if (!strncmp (buf, "NOTICE ", 7)) {
		buf = word_eol[3];
		if (*buf == ':') {
			buf ++;
		}
		EMIT_SIGNAL_TIMESTAMP (XP_TE_SERVNOTICE, sess, buf, 
									  sess->server->servername, NULL, NULL, 0,
									  tags_data->timestamp);
		return;
	}
	if (!strncmp (buf, "AUTHENTICATE", 12)) {
		// dbg!! inbound_sasl_authenticate (sess->server, word_eol[2]);
		return;
	}

	EMIT_SIGNAL_TIMESTAMP (XP_TE_SERVTEXT, sess, buf, sess->server->servername,
								  rawname, NULL, 0, tags_data->timestamp);
}

/* Returns the timezone offset. This should be the same as the variable
 * "timezone" in time.h, but *BSD doesn't have it.
 */
static time_t
get_timezone (void)
{
	struct tm tm_utc, tm_local;
	time_t t, time_utc, time_local;

	time (&t);

	/* gmtime() and localtime() are thread-safe on windows.
	 * on other systems we should use {gmtime,localtime}_r().
	 */
#if WIN32
	tm_utc = *gmtime (&t);
	tm_local = *localtime (&t);
#else
	gmtime_r (&t, &tm_utc);
	localtime_r (&t, &tm_local);
#endif

	time_utc = mktime (&tm_utc);
	time_local = mktime (&tm_local);

	return time_utc - time_local;
}

/* Handle time-server tags.
 * 
 * Sets tags_data->timestamp to the correct time (in unix time). 
 * This received time is always in UTC.
 *
 * See http://ircv3.atheme.org/extensions/server-time-3.2
 */
static void
handle_message_tag_time (const char *time, message_tags_data *tags_data)
{
	/* The time format defined in the ircv3.2 specification is
	 *       YYYY-MM-DDThh:mm:ss.sssZ
	 * but znc simply sends a unix time (with 3 decimal places for miliseconds)
	 * so we might as well support both.
	 */
	if (!*time)
		return;
	
	if (time[strlen (time) - 1] == 'Z')
	{
		/* as defined in the specification */
		struct tm t;
		int z;

		/* we ignore the milisecond part */
		z = sscanf (time, "%d-%d-%dT%d:%d:%d", &t.tm_year, &t.tm_mon, &t.tm_mday,
					&t.tm_hour, &t.tm_min, &t.tm_sec);

		if (z != 6)
			return;

		t.tm_year -= 1900;
		t.tm_mon -= 1;
		t.tm_isdst = 0; /* day light saving time */

		tags_data->timestamp = mktime (&t);

		if (tags_data->timestamp < 0)
		{
			tags_data->timestamp = 0;
			return;
		}

		/* get rid of the local time (mktime() receives a local calendar time) */
		tags_data->timestamp -= get_timezone();
	}
	else
	{
		/* znc */
		long long int t;

		/* we ignore the milisecond part */
		if (sscanf (time, "%lld", &t) != 1)
			return;

		tags_data->timestamp = (time_t) t;
	}
}

char** strsplit(const char* str, const char* delimiter, int max_tokens)
{
	// simple, assume cannot large 64 token
	int _max = 64;
	int tokens = 0;
	const char* p = str;
	const char* p1;
	size_t s;
	size_t delimiter_s = strlen(delimiter);
	char** ret = (char**)malloc(_max * sizeof(char*));

	while (p1 = strstr(p, delimiter)) {
		s = p1 - p;
		ret[tokens] = (char*)malloc(s + 1);
		memcpy(ret[tokens], p, s);
		ret[tokens ++][s] = '\0';

		p = p1 + delimiter_s;
	}
	if (p[0] != '\0') {
		s = str + strlen(str) - p;
		ret[tokens] = (char*)malloc(s + 1);
		memcpy(ret[tokens], p, s);
		ret[tokens ++][s] = '\0';
	}
	return ret;
}

void strfreev(char** strv)
{
	int i;
	for (i = 0; strv[i]; i ++) {
		free(strv[i]);
	}
	free(strv);
}

/* Handle message tags.
 *
 * See http://ircv3.atheme.org/specification/message-tags-3.2 
 */
static void
handle_message_tags (server *serv, const char *tags_str,
							message_tags_data *tags_data)
{
	char **tags;
	int i;

	/* FIXME We might want to avoid the allocation overhead here since 
	 * this might be called for every message from the server.
	 */
	tags = strsplit (tags_str, ";", 0);

	for (i=0; tags[i]; i++) {
		char *key = tags[i];
		char *value = strchr(tags[i], '=');

		if (!value)
			continue;

		*value = '\0';
		value++;

		if (serv->have_server_time && !strcmp (key, "time"))
			handle_message_tag_time (value, tags_data);
	}
	
	strfreev(tags);
}

/* irc_inline() - 1 single line received from serv */
static void irc_inline(server* serv, char* buf, int len)
{
	session *sess, *tmp;
	char *type, *text;
	char *word[PDIWORDS + 1];
	char *word_eol[PDIWORDS + 1];
	char *pdibuf;
	message_tags_data tags_data = MESSAGE_TAGS_DATA_INIT;

	pdibuf = (char*)malloc(len + 1);

	sess = serv->sess_list.front();

	/* Python relies on this */
	word[PDIWORDS] = NULL;
	word_eol[PDIWORDS] = NULL;

	if (*buf == '@') {
		char* tags = buf + 1; /* skip the '@' */
		char* sep = strchr (buf, ' ');

		if (!sep) {
			goto xit;
		}
		
		*sep = '\0';
		buf = sep + 1;

		handle_message_tags(serv, tags, &tags_data);
	}

	// dbg!! url_check_line (buf);

	/* split line into words and words_to_end_of_line */
	process_data_init(pdibuf, buf, word, word_eol, false, false);

	if (buf[0] == ':')
	{
		/* find a context for this message */
		if (is_channel(serv, word[3])) {
			tmp = find_channel (serv, word[3]);
			if (tmp) {
				sess = tmp;
			}
		}

		/* for server messages, the 2nd word is the "message type" */
		type = word[2];

		word[0] = type;
		word_eol[1] = buf;	/* keep the ":" for plugins */

		/* dbg!!
		if (plugin_emit_server (sess, type, word, word_eol, tags_data.timestamp)) {
			goto xit;
		}
		*/

		word[1]++;
		word_eol[1] = buf + 1;	/* but not for HexChat internally */

	} else {
		word[0] = type = word[1];

		/* dbg!!
		if (plugin_emit_server (sess, type, word, word_eol, tags_data.timestamp)) {
			goto xit;
		}
		*/
	}

	if (buf[0] != ':')
	{
		process_named_servermsg (sess, buf, word[0], word_eol, &tags_data);
		goto xit;
	}

	/* see if the second word is a numeric */
	if (isdigit ((unsigned char) word[2][0])) {
		text = word_eol[4];
		if (*text == ':') {
			text ++;
		}

		process_numeric(sess, buf, atoi (word[2]), word, word_eol, text, &tags_data);
	} else {
		process_named_msg(sess, buf, type, word, word_eol, &tags_data);
	}

xit:
	free(pdibuf);
}

void
proto_fill_her_up(server *serv)
{
	serv->p_inline = irc_inline;
	serv->p_invite = irc_invite;
	serv->p_cycle = irc_cycle;
	serv->p_ctcp = irc_ctcp;
	serv->p_nctcp = irc_nctcp;
	serv->p_quit = irc_quit;
	serv->p_kick = irc_kick;
	serv->p_part = irc_part;
	serv->p_ns_identify = irc_ns_identify;
	serv->p_ns_ghost = irc_ns_ghost;
	serv->p_join = irc_join;
	serv->p_join_list = irc_join_list;
	serv->p_login = irc_login;
	serv->p_join_info = irc_join_info;
	serv->p_mode = irc_mode;
	serv->p_user_list = irc_user_list;
	serv->p_away_status = irc_away_status;
	/*serv->p_get_ip = irc_get_ip;*/
	serv->p_whois = irc_user_whois;
	serv->p_get_ip = irc_user_list;
	serv->p_get_ip_uh = irc_userhost;
	serv->p_set_back = irc_set_back;
	serv->p_set_away = irc_set_away;
	serv->p_message = irc_message;
	serv->p_action = irc_action;
	serv->p_notice = irc_notice;
	serv->p_topic = irc_topic;
	serv->p_list_channels = irc_list_channels;
	serv->p_change_nick = irc_change_nick;
	serv->p_names = irc_names;
	serv->p_ping = irc_ping;
	serv->p_monitor = irc_monitor;
	serv->p_raw = irc_raw;
	serv->p_cmp = rfc_casecmp;	/* can be changed by 005 in modes.c */
}

//
// move other function to it.
//

server* server_new(const ircnet& net)
{
	static int id;
	server* serv = new server;
	proto_fill_her_up(serv);

	serv->id = id ++;
	serv->sok = -1;
	strcpy (serv->nick, net.nick.c_str());

	//
	// server_set_defaults
	//
	serv->chantypes = strdup("#&!+");
	serv->chanmodes = strdup("beI,k,l");
	serv->nick_prefixes = strdup("@%+");
	serv->nick_modes = strdup("ohv");

	// server_set_encoding(serv, "UTF-8");

	serv->connected = false;
	serv->nickcount = 1;
	serv->end_of_motd = false;
	serv->is_away = false;
	serv->supports_watch = false;
	serv->supports_monitor = false;
	serv->bad_prefix = false;
	serv->use_who = true;
	serv->have_namesx = false;
	serv->have_awaynotify = false;
	serv->have_uhnames = false;
	serv->have_whox = false;
	serv->have_idmsg = false;
	serv->have_accnotify = false;
	serv->have_extjoin = false;
	serv->have_server_time = false;
	serv->have_sasl = false;
	serv->have_except = false;
	serv->have_invite = false;

	if (net.logintype) {
		serv->loginmethod = net.logintype;
	} else {
		serv->loginmethod = LOGIN_DEFAULT_REAL;
	}

	serv->password[0] = 0;

	serv->network = net;

	session* sess = new_ircwindow(serv, NULL, SESS_SERVER);

	return serv;
}

server::~server()
{
	for (std::list<session*>::const_iterator it = sess_list.begin(); it != sess_list.end(); ++ it) {
		session* sess = *it;
		delete sess;
	}
}

session* session_new(server *serv, char *from, int type)
{
	session* sess = new session;

	sess->server = serv;
	sess->logfd = -1;
	sess->scrollfd = -1;
	sess->type = type;

	sess->alert_beep = SET_DEFAULT;
	sess->alert_taskbar = SET_DEFAULT;
	sess->alert_tray = SET_DEFAULT;

	sess->text_hidejoinpart = SET_DEFAULT;
	sess->text_logging = SET_DEFAULT;
	sess->text_scrollback = SET_DEFAULT;
	sess->text_strip = SET_DEFAULT;

	sess->lastact_idx = LACT_NONE;

	if (from != NULL) {
		safe_strcpy(sess->channel, from, CHANLEN);
		safe_strcpy(sess->session_name, from, CHANLEN);
	}

	serv->sess_list.push_front(sess);

	return sess;
}

session::session()
{
}

session::~session()
{
}

bool part_channel(server* serv, const char* chan)
{
	serv->p_part(serv, chan, "Leave");
	for (std::list<session*>::iterator it = serv->sess_list.begin(); it != serv->sess_list.end(); ++ it) {
		session* sess = *it;
		if (sess->type == SESS_CHANNEL && !serv->p_cmp(chan, sess->channel)) {
			serv->sess_list.erase(it);
			return true;
		}
	}
	return false;
}

session* new_ircwindow(server* serv, char* name, int type)
{
	session* sess;

	switch (type) {
	case SESS_SERVER:
		sess = session_new(serv, name, SESS_SERVER);
		serv->server_session = sess;
		serv->front_session = sess;
		break;
	case SESS_DIALOG:
		sess = session_new(serv, name, type);
		break;
	default:
		sess = session_new(serv, name, type);
		break;
	}

	/* dbg!!
	irc_init (sess);
	chanopt_load (sess);
	scrollback_load (sess);
	if (sess->scrollwritten && sess->scrollback_replay_marklast) {
		sess->scrollback_replay_marklast (sess);
	}
	if (type == SESS_DIALOG) {
		struct User *user;

		log_open_or_close (sess);

		user = userlist_find_global (serv, name);
		if (user && user->hostname) {
			set_topic (sess, user->hostname, user->hostname);
		}
	}
	plugin_emit_dummy_print (sess, "Open Context");
	*/
	return sess;
}

unsigned long make_ping_time(void)
{
	struct timeval timev;
#ifndef WIN32
	gettimeofday(&timev, 0);
#else
	time_t clock;
	struct tm tm;
	SYSTEMTIME wtm;
	GetLocalTime(&wtm);
	tm.tm_year = wtm.wYear - 1900;
	tm.tm_mon = wtm.wMonth - 1;
	tm.tm_mday = wtm.wDay;
	tm.tm_hour = wtm.wHour;
	tm.tm_min = wtm.wMinute;
	tm.tm_sec = wtm.wSecond;
	tm.tm_isdst = -1;
	clock = mktime(&tm);
	timev.tv_sec = clock;
	timev.tv_usec = wtm.wMilliseconds * 1000;
#endif

	return (timev.tv_sec - 50000) * 1000 + timev.tv_usec/1000;
}

void tcp_sendf(server *serv, const char *fmt, ...)
{
	va_list args;
	/* keep this buffer in BSS. Converting UTF-8 to ISO-8859-x might make the
      string shorter, so allow alot more than 512 for now. */
	static char send_buf[1540];	/* good code hey (no it's not overflowable) */
	int len;

	va_start (args, fmt);
	len = vsnprintf(send_buf, sizeof(send_buf) - 1, fmt, args);
	va_end(args);

	send_buf[sizeof(send_buf) - 1] = '\0';
	if (len < 0 || len > (sizeof(send_buf) - 1)) {
		len = strlen (send_buf);
	}

	serv->sock->tcp_send_len(send_buf, len);
}

/* black n white(0/1) are bad colors for nicks, and we'll use color 2 for us */
/* also light/dark gray (14/15) */
/* 5,7,8 are all shades of yellow which happen to look damn near the same */

static char rcolors[] = { 19, 20, 22, 24, 25, 26, 27, 28, 29 };

int text_color_of (const char *name)
{
	int i = 0, sum = 0;

	while (name[i])
		sum += name[i++];
	sum %= sizeof (rcolors) / sizeof (char);
	return rcolors[sum];
}

/*
	CL: format_event now handles filtering of arguments:
	1) if prefs.hex_text_stripcolor_msg is set, filter all style control codes from arguments
	2) always strip \010 (ATTR_HIDDEN) from arguments: it is only for use in the format string itself
*/
#define ARG_FLAG(argn) (1 << (argn))

/* called by EMIT_SIGNAL macro */
void text_emit (int index, session *sess, const char *a, const char *b, const char *c, const char *d, time_t timestamp)
{
	char *word[PDIWORDS];
	int i;
	unsigned int stripcolor_args = (chanopt_is_set (prefs.hex_text_stripcolor_msg, sess->text_strip) ? 0xFFFFFFFF : 0);
	char tbuf[NICKLEN + 4];

	if (prefs.hex_text_color_nicks && (index == XP_TE_CHANACTION || index == XP_TE_CHANMSG))
	{
		snprintf (tbuf, sizeof (tbuf), "\003%d%s", text_color_of (a), a);
		a = tbuf;
		stripcolor_args &= ~ARG_FLAG(1);	/* don't strip color from this argument */
	}

	// dbg!! word[0] = te[index].name;
	word[1] = (char*)(a ? a : nul_chars);
	word[2] = (char*)(b ? b : nul_chars);
	word[3] = (char*)(c ? c : nul_chars);
	word[4] = (char*)(d ? d : nul_chars);
	for (i = 5; i < PDIWORDS; i++)
		word[i] = (char*)nul_chars;

	/* dbg!!
	if (plugin_emit_print (sess, word, timestamp)) {
		return;
	}
	*/

	/* If a plugin's callback executes "/close", 'sess' may be invalid */
	if (!is_session (sess))
		return;

	switch (index)
	{
	case XP_TE_JOIN:
	case XP_TE_PART:
	case XP_TE_PARTREASON:
	case XP_TE_QUIT:
		/* implement ConfMode / Hide Join and Part Messages */
		if (chanopt_is_set (prefs.hex_irc_conf_mode, sess->text_hidejoinpart))
			return;
		break;

	/* ===Private message=== */
	case XP_TE_PRIVMSG:
	case XP_TE_DPRIVMSG:
	case XP_TE_PRIVACTION:
	case XP_TE_DPRIVACTION:
		/* dbg!!
		if (chanopt_is_set (prefs.hex_input_beep_priv, sess->alert_beep) && (!prefs.hex_away_omit_alerts || !sess->server->is_away)) {
			sound_beep (sess);
		}
		*/
		/* dbg!!
		if (chanopt_is_set (prefs.hex_input_flash_priv, sess->alert_taskbar) && (!prefs.hex_away_omit_alerts || !sess->server->is_away)) {
			fe_flash_window (sess);
		}
		*/
		/* why is this one different? because of plugin-tray.c's hooks! ugly */
		/* dbg!!
		if (sess->alert_tray == SET_ON) {
			fe_tray_set_icon (FE_ICON_MESSAGE);
		}
		*/
		break;

	/* ===Highlighted message=== */
	case XP_TE_HCHANACTION:
	case XP_TE_HCHANMSG:
		/* dbg!!
		if (chanopt_is_set (prefs.hex_input_beep_hilight, sess->alert_beep) && (!prefs.hex_away_omit_alerts || !sess->server->is_away)) {
			sound_beep (sess);
		}
		*/
		/* dbg!!
		if (chanopt_is_set (prefs.hex_input_flash_hilight, sess->alert_taskbar) && (!prefs.hex_away_omit_alerts || !sess->server->is_away)) {
			fe_flash_window (sess);
		}
		*/
		/* dbg!!
		if (sess->alert_tray == SET_ON) {
			fe_tray_set_icon (FE_ICON_MESSAGE);
		}
		*/
		break;

	/* ===Channel message=== */
	case XP_TE_CHANACTION:
	case XP_TE_CHANMSG:
		/* dbg!!
		if (chanopt_is_set (prefs.hex_input_beep_chans, sess->alert_beep) && (!prefs.hex_away_omit_alerts || !sess->server->is_away)) {
			sound_beep (sess);
		}
		*/
		/* dbg!!
		if (chanopt_is_set (prefs.hex_input_flash_chans, sess->alert_taskbar) && (!prefs.hex_away_omit_alerts || !sess->server->is_away)) {
			fe_flash_window (sess);
		}
		*/
		/* dbg!!
		if (sess->alert_tray == SET_ON) {
			fe_tray_set_icon (FE_ICON_MESSAGE);
		}
		*/
		break;

	/* ===Nick change message=== */
	case XP_TE_CHANGENICK:
		if (prefs.hex_irc_hide_nickchange)
			return;
		break;
	}

	// dbg!! sound_play_event (index);
	// dbg!! display_event (sess, index, word, stripcolor_args, timestamp);
}

bool is_session(session * sess)
{
	server* serv = sess->server;
	return std::find(serv->sess_list.begin(), serv->sess_list.end(), sess) != serv->sess_list.end();
}

/* does 'chan' have a valid prefix? e.g. # or & */
bool is_channel(const server* serv, const char* chan)
{
	if (strchr(serv->chantypes, chan[0])) {
		return true;
	}
	return false;
}

session* find_channel(server *serv, const char* chan)
{
	for (std::list<session*>::const_iterator it = serv->sess_list.begin(); it != serv->sess_list.end(); ++ it) {
		session* sess = *it;
		if ((serv == sess->server) && sess->type == SESS_CHANNEL) {
			if (!serv->p_cmp(chan, sess->channel))
				return sess;
		}
	}
	return NULL;
}

static int byte_size_from_utf8_first(unsigned char ch)
{
	int count;

	if ((ch & 0x80) == 0) {
		count = 1;
	} else if ((ch & 0xE0) == 0xC0) {
		count = 2;
	} else if ((ch & 0xF0) == 0xE0) {
		count = 3;
	} else if ((ch & 0xF8) == 0xF0) {
		count = 4;
	} else if ((ch & 0xFC) == 0xF8) {
		count = 5;
	} else if ((ch & 0xFE) == 0xFC) {
		count = 6;
	} else {
		// error. in order to ignore, temparal set.
		count = 1;
	}

	return count;
}

/* features: 1. "src" must be valid, NULL terminated UTF-8 */
/*           2. "dest" will be left with valid UTF-8 - no partial chars! */
void safe_strcpy(char* dest, const char* src, int bytes_left)
{
	int mbl;

	while (1) {
		mbl = byte_size_from_utf8_first(*((unsigned char *)src));

		if (bytes_left < (mbl + 1)) { /* can't fit with NULL? */
			*dest = 0;
			break;
		}

		if (mbl == 1) {	/* one byte char */
			*dest = *src;
			if (*src == 0)
				break;	/* it all fit */
			dest++;
			src++;
			bytes_left--;
		} else { /* multibyte char */
			memcpy (dest, src, mbl);
			dest += mbl;
			src += mbl;
			bytes_left -= mbl;
		}
	}
}

void process_data_init(char *buf, const char *cmd, char *word[],
						 char *word_eol[], bool handle_quotes,
						 bool allow_escape_quotes)
{
	int wordcount = 2;
	int space = false;
	int quote = false;
	int j = 0;

	word[0] = (char*)nul_chars;
	word_eol[0] = (char*)nul_chars;
	word[1] = (char *)buf;
	word_eol[1] = (char *)cmd;

	while (1) {
		switch (*cmd) {
		case 0:
			buf[j] = 0;
			for (j = wordcount; j < PDIWORDS; j++) {
				word[j] = (char*)nul_chars;
				word_eol[j] = (char*)nul_chars;
			}
			return;
		case '\042':
			if (!handle_quotes) {
				goto def;
			}
			/* two quotes turn into 1 */
			if (allow_escape_quotes && cmd[1] == '\042') {
				cmd++;
				goto def;
			}
			if (quote) {
				quote = false;
				space = false;
			} else {
				quote = true;
			}
			cmd ++;
			break;
		case ' ':
			if (!quote) {
				if (!space) {
					buf[j] = 0;
					j++;

					if (wordcount < PDIWORDS) {
						word[wordcount] = &buf[j];
						word_eol[wordcount] = (char*)(cmd + 1);
						wordcount++;
					}

					space = true;
				}
				cmd++;
				break;
			}
		default:
def:
			space = false;
			// there is maybe non-utf8 unicode charactor.
			// don't use first byte to skip len of char.
			// if one char is error, there is result to non.
			buf[j] = *cmd;
			j++;
			cmd++;
		}
	}
}

/* check if a msg should be ignored by browsing our ignore list */
int ignore_check (char *host, int type)
{
/*	struct ignore *ig;
	GSList *list = ignore_list;

	// check if there's an UNIGNORE first, they take precendance. 
	while (list) {
		ig = (struct ignore *) list->data;
		if (ig->type & IG_UNIG) {
			if (ig->type & type) {
				if (match (ig->mask, host))
					return false;
			}
		}
		list = list->next;
	}

	list = ignore_list;
	while (list) {
		ig = (struct ignore *) list->data;

		if (ig->type & type) {
			if (match (ig->mask, host)) {
				ignored_total++;
				if (type & IG_PRIV)
					ignored_priv++;
				if (type & IG_NOTI)
					ignored_noti++;
				if (type & IG_CHAN)
					ignored_chan++;
				if (type & IG_CTCP)
					ignored_ctcp++;
				if (type & IG_INVI)
					ignored_invi++;
				fe_ignore_update (2);
				return true;
			}
		}
		list = list->next;
	}
*/
	return false;
}

/************************************************************************
 *    This technique was borrowed in part from the source code to 
 *    ircd-hybrid-5.3 to implement case-insensitive string matches which
 *    are fully compliant with Section 2.2 of RFC 1459, the copyright
 *    of that code being (C) 1990 Jarkko Oikarinen and under the GPL.
 *    
 *    A special thanks goes to Mr. Okarinen for being the one person who
 *    seems to have ever noticed this section in the original RFC and
 *    written code for it.  Shame on all the rest of you (myself included).
 *    
 *        --+ Dagmar d'Surreal
 */

const unsigned char rfc_tolowertab[] =
	{ 0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa,
	0xb, 0xc, 0xd, 0xe, 0xf, 0x10, 0x11, 0x12, 0x13, 0x14,
	0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d,
	0x1e, 0x1f,
	' ', '!', '"', '#', '$', '%', '&', 0x27, '(', ')',
	'*', '+', ',', '-', '.', '/',
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
	':', ';', '<', '=', '>', '?',
	'@', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i',
	'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's',
	't', 'u', 'v', 'w', 'x', 'y', 'z', '{', '|', '}', '~',
	'_',
	'`', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i',
	'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's',
	't', 'u', 'v', 'w', 'x', 'y', 'z', '{', '|', '}', '~',
	0x7f,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
	0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99,
	0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
	0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9,
	0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
	0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9,
	0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
	0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9,
	0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
	0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9,
	0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
	0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
	0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9,
	0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

#define rfc_tolower(c) (rfc_tolowertab[(unsigned char)(c)])

int rfc_casecmp (const char *s1, const char *s2)
{
	register unsigned char *str1 = (unsigned char *) s1;
	register unsigned char *str2 = (unsigned char *) s2;
	register int res;

	while ((res = rfc_tolower (*str1) - rfc_tolower (*str2)) == 0)
	{
		if (*str1 == '\0')
			return 0;
		str1++;
		str2++;
	}
	return (res);
}

int
rfc_ncasecmp (char *str1, char *str2, int n)
{
	unsigned char *s1 = (unsigned char *) str1;
	unsigned char *s2 = (unsigned char *) str2;
	int res;

	while ((res = rfc_tolower (*s1) - rfc_tolower (*s2)) == 0)
	{
		s1++;
		s2++;
		n--;
		if (n == 0 || (*s1 == '\0' && *s2 == '\0'))
			return 0;
	}
	return (res);
}

bool chanopt_is_set (unsigned int global, unsigned char per_chan_setting)
{
	if (per_chan_setting == SET_ON || per_chan_setting == SET_OFF) {
		return per_chan_setting;
	} else {
		return global;
	}
}

void server_set_name(server *serv, char *name)
{
	if (name[0] == 0) {
		name = serv->hostname;
	}

	/* strncpy parameters must NOT overlap */
	if (name != serv->servername) {
		safe_strcpy(serv->servername, name, sizeof (serv->servername));
	}

	for (std::list<session*>::const_iterator it = serv->sess_list.begin(); it != serv->sess_list.end(); ++ it) {
		session* sess = *it;
		if (sess->server == serv) {
			// fe_set_title(sess);
		}
	}

	if (serv->server_session->type == SESS_SERVER) {
		/*
		if (serv->network) {
			safe_strcpy (serv->server_session->channel, ((ircnet *)serv->network)->name, CHANLEN);
		} else {
			safe_strcpy (serv->server_session->channel, name, CHANLEN);
		}
		fe_set_channel (serv->server_session);
		*/
	}
}

void inbound_newnick(server *serv, char *nick, char *newnick, int quiet, const message_tags_data *tags_data)
{
	int me = false;

	if (!serv->p_cmp(nick, serv->nick)) {
		me = true;
		safe_strcpy(serv->nick, newnick, NICKLEN);
	}

	for (std::list<session*>::const_iterator it = serv->sess_list.begin(); it != serv->sess_list.end(); ++ it) {
		session* sess = *it;
		if (sess->server == serv) {
			/* dbg!!
			if (userlist_change(sess, nick, newnick) || (me && sess->type == SESS_SERVER)) {
				if (!quiet) {
					if (me) {
						EMIT_SIGNAL_TIMESTAMP (XP_TE_UCHANGENICK, sess, nick, 
													  newnick, NULL, NULL, 0,
													  tags_data->timestamp);
					} else {
						EMIT_SIGNAL_TIMESTAMP (XP_TE_CHANGENICK, sess, nick,
													  newnick, NULL, NULL, 0, tags_data->timestamp);
					}
				}
			}
			if (sess->type == SESS_DIALOG && !serv->p_cmp(sess->channel, nick)) {
				safe_strcpy (sess->channel, newnick, CHANLEN);
				fe_set_channel (sess);
			}
			fe_set_title (sess);
			*/
		}
	}

	// dbg!! dcc_change_nick (serv, nick, newnick);

	if (me) {
		// dbg!! fe_set_nick (serv, newnick);
	}
}

void inbound_login_start(session *sess, char *nick, char *servname, const message_tags_data *tags_data)
{
	inbound_newnick (sess->server, sess->server->nick, nick, true, tags_data);
	server_set_name (sess->server, servname);
	if (sess->type == SESS_SERVER) {
		// dbg!! log_open_or_close (sess);
	}
	// reset our away status 
	if (sess->server->reconnect_away) {
		sess->server->reconnect_away = false;
	}
}

void inbound_notice(server *serv, char *to, char *nick, char *msg, char *ip, int id, const message_tags_data *tags_data)
{
	char *po,*ptr=to;
	session *sess = 0;
	int server_notice = false;

	if (is_channel (serv, ptr))
		sess = find_channel (serv, ptr);

	/* /notice [mode-prefix]#channel should end up in that channel */
	if (!sess && strchr(serv->nick_prefixes, ptr[0]) != NULL)
	{
		ptr++;
		sess = find_channel (serv, ptr);
	}

	if (strcmp (nick, ip) == 0)
		server_notice = true;

	if (!sess)
	{
		ptr = 0;
		if (prefs.hex_irc_notice_pos == 0) {
											/* paranoia check */
			if (msg[0] == '[' && (!serv->have_idmsg || id)) {
				/* guess where chanserv meant to post this -sigh- */
				/* dbg!!
				if (!strcasecmp (nick, "ChanServ") && !find_dialog (serv, nick)) {
					char *dest = strdup(msg + 1);
					char *end = strchr(dest, ']');
					if (end) {
						*end = 0;
						sess = find_channel (serv, dest);
					}
					free(dest);
				}
				*/
			}
			if (!sess) {
				// dbg!! sess = find_session_from_nick (nick, serv);
			}
		} else if (prefs.hex_irc_notice_pos == 1) {
			int stype = server_notice ? SESS_SNOTICES : SESS_NOTICES;
			// dbg!! sess = find_session_from_type (stype, serv);
			if (!sess) 	{
				if (stype == SESS_NOTICES) {
					// dbg!! sess = new_ircwindow (serv, "(notices)", SESS_NOTICES);
				} else {
					// dbg!! sess = new_ircwindow (serv, "(snotices)", SESS_SNOTICES);
				}
				/* dbg!!
				fe_set_channel (sess);
				fe_set_title (sess);
				fe_set_nonchannel (sess, false);
				userlist_clear (sess);
				log_open_or_close (sess);
				*/
			}
			/* Avoid redundancy with some Undernet notices */
			if (!strncmp (msg, "*** Notice -- ", 14))
				msg += 14;
		} else {
			sess = serv->front_session;
		}

		if (!sess) {
			if (server_notice) {
				sess = serv->server_session;
			} else {
				sess = serv->front_session;
			}
		}
	}

	if (msg[0] == 1) {
		msg++;
		if (!strncmp (msg, "PING", 4)) {
			inbound_ping_reply (sess, msg + 5, nick, tags_data);
			return;
		}
	}
	po = strchr (msg, '\001');
	if (po) {
		po[0] = 0;
	}

	if (server_notice) {
		EMIT_SIGNAL_TIMESTAMP (XP_TE_SERVNOTICE, sess, msg, nick, NULL, NULL, 0,
									  tags_data->timestamp);
	} else if (ptr) {
		EMIT_SIGNAL_TIMESTAMP (XP_TE_CHANNOTICE, sess, nick, to, msg, NULL, 0,
									  tags_data->timestamp);
	} else {
		EMIT_SIGNAL_TIMESTAMP (XP_TE_NOTICE, sess, nick, msg, NULL, NULL, 0,
									  tags_data->timestamp);
	}
}

void inbound_ujoin(server* serv, char* chan, char* nick, char* ip, const message_tags_data* tags_data)
{
	session *sess;
	int found_unused = false;

	/* already joined? probably a bnc */
	sess = find_channel (serv, chan);
	if (!sess) {
		sess = new_ircwindow(serv, chan, SESS_CHANNEL);
		/* dbg!!
		// see if a window is waiting to join this channel
		sess = find_session_from_waitchannel (chan, serv);
		if (!sess) {
			// find a "<none>" tab and use that
			sess = find_unused_session (serv);
			found_unused = sess != NULL;
			if (!sess)
				// last resort, open a new tab/window
				sess = new_ircwindow(serv, chan, SESS_CHANNEL);
		}
		*/
	}

	safe_strcpy (sess->channel, chan, CHANLEN);
	/* dbg!!
	if (found_unused) {
		chanopt_load (sess);
		scrollback_load (sess);
		if (sess->scrollwritten && sess->scrollback_replay_marklast)
			sess->scrollback_replay_marklast (sess);
	}

	fe_set_channel (sess);
	fe_set_title (sess);
	fe_set_nonchannel (sess, true);
	userlist_clear (sess);

	log_open_or_close (sess);
	*/

	sess->waitchannel[0] = 0;
	sess->ignore_date = true;
	sess->ignore_mode = true;
	sess->ignore_names = true;
	sess->end_of_names = false;

	fill_params_5(*serv, "UJOINED", chan, "\0", "\0", "\0");
	serv->sock->handle_command(serv->params);
}

void inbound_join(server* serv, char* chan, char* user, char* ip, char* account, char* realname, const message_tags_data* tags_data)
{
	session *sess = find_channel(serv, chan);
	if (sess) {
		EMIT_SIGNAL_TIMESTAMP (XP_TE_JOIN, sess, user, chan, ip, account, 0,
									  tags_data->timestamp);
		fill_params_5(*serv, "JOIN", chan, user, "\0", "\0");
		serv->sock->handle_command(serv->params);
	}
}

void inbound_005(server* serv, char *word[], const message_tags_data* tags_data)
{
	int w;
	char *pre;

	w = 4;							  /* start at the 4th word */
	while (w < PDIWORDS && *word[w]) {
		if (strncmp (word[w], "MODES=", 6) == 0) {
			serv->modes_per_line = atoi (word[w] + 6);
		} else if (strncmp (word[w], "CHANTYPES=", 10) == 0) {
			free(serv->chantypes);
			serv->chantypes = strdup(word[w] + 10);
		} else if (strncmp (word[w], "CHANMODES=", 10) == 0) {
			free (serv->chanmodes);
			serv->chanmodes = strdup (word[w] + 10);
		} else if (strncmp (word[w], "PREFIX=", 7) == 0) {
			pre = strchr (word[w] + 7, ')');
			if (pre) {
				pre[0] = 0;			  /* NULL out the ')' */
				free (serv->nick_prefixes);
				free (serv->nick_modes);
				serv->nick_prefixes = strdup (pre + 1);
				serv->nick_modes = strdup (word[w] + 8);
			} else {
				/* bad! some ircds don't give us the modes. */
				/* in this case, we use it only to strip /NAMES */
				serv->bad_prefix = true;
				free (serv->bad_nick_prefixes);
				serv->bad_nick_prefixes = strdup (word[w] + 7);
			}
		} else if (strncmp (word[w], "WATCH=", 6) == 0) {
			serv->supports_watch = true;
		} else if (strncmp (word[w], "MONITOR=", 8) == 0) {
			serv->supports_monitor = true;
		} else if (strncmp (word[w], "NETWORK=", 8) == 0) {
			if (serv->server_session->type == SESS_SERVER) {
				safe_strcpy (serv->server_session->channel, word[w] + 8, CHANLEN);
				// dbg!! fe_set_channel (serv->server_session);
			}

		} else if (strncmp (word[w], "CASEMAPPING=", 12) == 0) {
			if (strcmp (word[w] + 12, "ascii") == 0) { /* bahamut */
				// dbg!! serv->p_cmp = (void *)strcasecmp;
			}
		} else if (strncmp (word[w], "CHARSET=", 8) == 0) {
			if (strncasecmp (word[w] + 8, "UTF-8", 5) == 0) {
				// dbg!! server_set_encoding (serv, "UTF-8");
			}
		} else if (strcmp (word[w], "NAMESX") == 0) {
									/* 12345678901234567 */
			serv->sock->tcp_send_len("PROTOCTL NAMESX\r\n", 17);
		} else if (strcmp (word[w], "WHOX") == 0) {
			serv->have_whox = true;
		} else if (strcmp (word[w], "EXCEPTS") == 0) {
			serv->have_except = true;
		} else if (strcmp (word[w], "INVEX") == 0) {
			/* supports mode letter +I, default channel invite */
			serv->have_invite = true;
		} else if (strncmp (word[w], "ELIST=", 6) == 0) {
			/* supports LIST >< min/max user counts? */
			if (strchr (word[w] + 6, 'U') || strchr (word[w] + 6, 'u')) {
				serv->use_listargs = true;
			}
		}

		w++;
	}
}

void inbound_login_end(session *sess, char *text, const message_tags_data *tags_data)
{
	server* serv = sess->server;

	if (!serv->end_of_motd) {
		if (prefs.hex_dcc_ip_from_server && serv->use_who) {
			serv->skip_next_userhost = true;
			serv->p_get_ip_uh (serv, serv->nick);	// sends USERHOST mynick
		}
		/* dbg!!
		set_default_modes (serv);

		if (serv->network) {
			// there may be more than 1, separated by \n

			cmdlist = ((ircnet *)serv->network)->commandlist;
			while (cmdlist) {
				cmd = cmdlist->data;
				inbound_exec_eom_cmd (cmd->command, sess);
				cmdlist = cmdlist->next;
			}

			// send nickserv password
			if (((ircnet *)serv->network)->pass && inbound_nickserv_login (serv)) {
				serv->p_ns_identify (serv, ((ircnet *)serv->network)->pass);
			}
		}

		// wait for join if command or nickserv set
		if (serv->network && prefs.hex_irc_join_delay
			&& ((((ircnet *)serv->network)->pass && inbound_nickserv_login (serv))
				|| ((ircnet *)serv->network)->commandlist))
		{
			serv->joindelay_tag = fe_timeout_add (prefs.hex_irc_join_delay * 1000, check_autojoin_channels, serv);
		} else {
			check_autojoin_channels (serv);
		}

		if (serv->supports_watch || serv->supports_monitor) {
			notify_send_watches (serv);
		}
		*/

		serv->end_of_motd = true;
	}

	fill_params_5(*serv, "LOGIN", "\0", "\0", "\0", "\0");
	serv->sock->handle_command(serv->params);

	if (prefs.hex_irc_skip_motd && !serv->motd_skipped) {
		serv->motd_skipped = true;
		EMIT_SIGNAL_TIMESTAMP (XP_TE_MOTDSKIP, serv->server_session, NULL, NULL,
									  NULL, NULL, 0, tags_data->timestamp);
		return;
	}

	EMIT_SIGNAL_TIMESTAMP (XP_TE_MOTD, serv->server_session, text, NULL, NULL,
								  NULL, 0, tags_data->timestamp);
}

void inbound_next_nick(session* sess, char* nick, int error, const message_tags_data *tags_data)
{
	server* serv = sess->server;
	char outbuf[12];
	snprintf(outbuf, sizeof(outbuf), "%d", error);

	fill_params_5(*serv, "NICKERR", nick, outbuf, "\0", "\0");
	serv->sock->handle_command(serv->params);
}

void inbound_ping_reply(session* sess, char* timestring, char* from, const message_tags_data* tags_data)
{
	unsigned long tim, nowtim, dif;
	int lag = 0;
	char outbuf[64];

	if (strncmp (timestring, "LAG", 3) == 0) {
		timestring += 3;
		lag = 1;
	}

	tim = strtoul(timestring, NULL, 10);
	nowtim = make_ping_time();
	dif = nowtim - tim;

	sess->server->ping_recv = time (0); 

	fill_params_5(*sess->server, "PONG", from, timestring, "\0", "\0");
	sess->server->sock->handle_command(sess->server->params);

	if (lag) {
		sess->server->lag_sent = 0;
		sess->server->lag = dif;
		// dbg!! fe_set_lag (sess->server, dif);
		return;
	}

	if (atol (timestring) == 0) {
		if (sess->server->lag_sent) {
			sess->server->lag_sent = 0;
		} else {
			EMIT_SIGNAL_TIMESTAMP (XP_TE_PINGREP, sess, from, "?", NULL, NULL, 0,
										  tags_data->timestamp);
		}
	} else {
		snprintf(outbuf, sizeof (outbuf), "%ld.%03ld", dif / 1000, dif % 1000);
		EMIT_SIGNAL_TIMESTAMP (XP_TE_PINGREP, sess, from, outbuf, NULL, NULL, 0,
									  tags_data->timestamp);
	}
}

#define STRIP_COLOR 1
#define STRIP_ATTRIB 2
#define STRIP_HIDDEN 4
#define STRIP_ESCMARKUP 8
#define STRIP_ALL 7

/* CL: strip_color2 strips src and writes the output at dst; pass the same pointer
	in both arguments to strip in place. */
int strip_color2(const char *src, int len, char *dst, int flags)
{
	int rcol = 0, bgcol = 0;
	char *start = dst;

	if (len == -1) len = strlen (src);
	while (len-- > 0)
	{
		if (rcol > 0 && (isdigit ((unsigned char)*src) ||
			(*src == ',' && isdigit ((unsigned char)src[1]) && !bgcol)))
		{
			if (src[1] != ',') rcol--;
			if (*src == ',') {
				rcol = 2;
				bgcol = 1;
			}
		} else {
			rcol = bgcol = 0;
			switch (*src) {
			case '\003':			  /*ATTR_COLOR: */
				if (!(flags & STRIP_COLOR)) goto pass_char;
				rcol = 2;
				break;
			case HIDDEN_CHAR:	/* CL: invisible text (for event formats only) */	/* this takes care of the topic */
				if (!(flags & STRIP_HIDDEN)) goto pass_char;
				break;
			case '\007':			  /*ATTR_BEEP: */
			case '\017':			  /*ATTR_RESET: */
			case '\026':			  /*ATTR_REVERSE: */
			case '\002':			  /*ATTR_BOLD: */
			case '\037':			  /*ATTR_UNDERLINE: */
			case '\035':			  /*ATTR_ITALICS: */
				if (!(flags & STRIP_ATTRIB)) goto pass_char;
				break;
			default:
			pass_char:
				*dst++ = *src;
			}
		}
		src++;
	}
	*dst = 0;

	return (int) (dst - start);
}

char* strip_color(const char *text, int len, int flags)
{
	char* new_str;

	if (len == -1) {
		len = strlen(text);
	}

	new_str = (char*)malloc(len + 2);
	strip_color2(text, len, new_str, flags);

	/* dbg!!
	if (flags & STRIP_ESCMARKUP) {
		char* esc = g_markup_escape_text(new_str, -1);
		free (new_str);
		return esc;
	}
	*/

	return new_str;
}

void inbound_topic(server* serv, char* chan, char* topic_text, const message_tags_data* tags_data)
{
	session *sess = find_channel(serv, chan);
	char *stripped_topic;

	if (sess) {
		stripped_topic = strip_color(topic_text, -1, STRIP_ALL);

		fill_params_5(*serv, "TOPIC", chan, stripped_topic, "\0", "\0");
		serv->sock->handle_command(serv->params);

		// set_topic(sess, topic_text, stripped_topic);
		free(stripped_topic);
	} else {
		sess = serv->server_session;
	}

	EMIT_SIGNAL_TIMESTAMP (XP_TE_TOPIC, sess, chan, topic_text, NULL, NULL, 0,
								  tags_data->timestamp);
}

void inbound_nameslist(server* serv, char *chan, char *names, const message_tags_data *tags_data)
{
	fill_params_5(*serv, "NAMREPLAY", chan, names, "\0", "\0");
	serv->sock->handle_command(serv->params);
}

bool inbound_nameslist_end(server* serv, char* chan, const message_tags_data* tags_data)
{
	fill_params_5(*serv, "ENDOFNAMES", chan, "\0", "\0", "\0");
	serv->sock->handle_command(serv->params);

	return true;
}

/* reporting new information found about this user. chan may be NULL.
 * away may be 0xff to indicate UNKNOWN. */
void inbound_user_info (session *sess, char *chan, char *user, char *host,
						 char *servname, char *nick, char *realname,
						 char *account, unsigned int away,
						 const message_tags_data *tags_data)
{
	server* serv = sess->server;

	fill_params_5(*serv, "WHOREPLAY", chan, nick, "true", away? "true": "false");
	serv->params[5] = user;
	serv->params[6] = host;
	serv->params[7] = realname;
	serv->params[8] = servname;
	serv->params[9] = "\0";
	serv->sock->handle_command(serv->params);
}

void inbound_privmsg(server* serv, char* from, char* ip, char* text, int id, const message_tags_data *tags_data)
{
	if (ip && ip[0]) {
		// dbg!! set_topic(sess, ip, ip);
	}
	inbound_chanmsg (serv, NULL, NULL, from, text, false, id, tags_data);
	return;
}

void inbound_chanmsg(server* serv, session* sess, char* chan, char *from, char *text, char fromme, int id, const message_tags_data *tags_data)
{
	fill_params_5(*serv, "PRIVMSG", chan? chan: "\0", from, text, "\0");
	serv->sock->handle_command(serv->params);
}

void inbound_part(server* serv, char* chan, char* user, char* ip, char* reason, const message_tags_data *tags_data)
{
	session *sess = find_channel(serv, chan);
	if (sess) {
		fill_params_5(*serv, "PART", chan, user, reason? reason: "\0", "\0");
		serv->sock->handle_command(serv->params);
	}
}

void inbound_upart(server *serv, char *chan, char *ip, char *reason, const message_tags_data *tags_data)
{
	fill_params_5(*serv, "UPARTED", chan, "\0", "\0", "\0");
	serv->sock->handle_command(serv->params);
}

void inbound_quit(server *serv, char *nick, char *ip, char *reason, const message_tags_data *tags_data)
{
	fill_params_5(*serv, "QUIT", nick, "\0", "\0", "\0");
	serv->sock->handle_command(serv->params);
}

void fe_chan_list_start(server *serv)
{
	fill_params_5(*serv, "LISTSTART", "\0", "\0", "\0", "\0");
	serv->sock->handle_command(serv->params);
}

void fe_add_chan_list(server *serv, char *chan, char *users, char *topic)
{
	fill_params_5(*serv, "LIST", chan, users, topic, "\0");
	serv->sock->handle_command(serv->params);
}

void fe_chan_list_end(server *serv)
{
	fill_params_5(*serv, "LISTEND", "\0", "\0", "\0", "\0");
	serv->sock->handle_command(serv->params);
}

/* monitor can send lists for numeric 730/731 */
void notify_set_offline_list(server* serv, char* users, int quiet, const message_tags_data *tags_data)
{
	fill_params_5(*serv, "OFFLINE", users, "\0", "\0", "\0");
	serv->sock->handle_command(serv->params);
}

void notify_set_online_list(server* serv, char* users, const message_tags_data *tags_data)
{
	fill_params_5(*serv, "ONLINE", users, "\0", "\0", "\0");
	serv->sock->handle_command(serv->params);
}

void chan_forbid_join(server* serv, char* chan, char* reason)
{
	fill_params_5(*serv, "FORBIDJOIN", chan, reason, "\0", "\0");
	serv->sock->handle_command(serv->params);
}

int split_up_text(const char* nick, const char* chan, char* text, int cmd_length)
{
	int max, space_offset, original_max;
	char* space;
	char original_ch;

	/* maximum allowed text */
	/* :nickname!username@host.com cmd_length */
	max = 512; /* rfc 2812 */
	max -= 3; /* :, !, @ */
	max -= cmd_length;
	max -= strlen(nick);
	max -= strlen(chan);
	
	max -= 9;	/* username */
	max -= 65;	/* max possible hostname and '@' */

	if ((int)strlen(text) > max) {
		int i = 0;
		int size;

		/* traverse the utf8 string and find the nearest cut point that
			doesn't split 1 char in half */
		while (1) {
			size = byte_size_from_utf8_first(((unsigned char *)text)[i]);
			if ((i + size) >= max) {
				break;
			}
			i += size;
		}
		max = i;

		original_max = max;
		original_ch = text[original_max];
		text[original_max] = '\0';

		/* Try splitting at last space */
		space = strrchr(text, ' ');
		if (space) {
			space_offset = space - text;

			/* Only split if last word is of sane length */
			if (max != space_offset && max - space_offset < 20) {
				max = space_offset + 1;
			}
		}

		text[original_max] = original_ch;
		return max;
	}

	return 0;
}

void send_msg(struct server* serv, const char* nick, const char* chan, char* text)
{
	int split_text;
	int cmd_length = 13; /* " PRIVMSG ", " ", :, \r, \n */
	int offset = 0;
	char original_ch;

	if (!strlen(text)) {
		return;
	}

	while ((split_text = split_up_text(nick, chan, text + offset, cmd_length))) {
		original_ch = text[offset + split_text];
		text[offset + split_text] = '\0';

		serv->p_message(serv, chan, text + offset);

		text[offset + split_text] = original_ch;
		offset += split_text;
	}

	serv->p_message(serv, chan, text + offset);
	offset = 0;
}

}