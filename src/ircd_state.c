/*
 * charybdis: An advanced ircd.
 * ircd_state.c: Functions for backing up and synchronizing IRCd's state
 *
 * Copyright (c) 2006 William Pitcock <nenolod@nenolod.net>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id: main.c 867 2006-02-16 14:25:09Z nenolod $
 */

#include "stdinc.h"
#include "setup.h"
#include "config.h"

#include "client.h"
#include "tools.h"
#include "tools.h"
#include "ircd.h"
#include "channel.h"
#include "class.h"
#include "client.h"
#include "common.h"
#include "event.h"
#include "hash.h"
#include "irc_string.h"
#include "ircd_signal.h"
#include "sprintf_irc.h"
#include "s_gline.h"
#include "msg.h"                /* msgtab */
#include "hostmask.h"
#include "numeric.h"
#include "parse.h"
#include "res.h"
#include "restart.h"
#include "s_auth.h"
#include "commio.h"
#include "s_conf.h"
#include "s_log.h"
#include "s_serv.h"             /* try_connections */
#include "s_user.h"
#include "s_stats.h"
#include "scache.h"
#include "send.h"
#include "whowas.h"
#include "modules.h"
#include "memory.h"
#include "hook.h"
#include "ircd_getopt.h"
#include "balloc.h"
#include "newconf.h"
#include "patricia.h"
#include "reject.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "cache.h"
#include "monitor.h"
#include "libcharybdis.h"
#include "patchlevel.h"
#include "serno.h"

dlink_list lclient_list = { NULL, NULL, 0 };
dlink_list global_client_list = { NULL, NULL, 0 };
dlink_list global_channel_list = { NULL, NULL, 0 };

dlink_list unknown_list;        /* unknown clients ON this server only */
dlink_list serv_list;           /* local servers to this server ONLY */
dlink_list global_serv_list;    /* global servers on the network */
dlink_list local_oper_list;     /* our opers, duplicated in lclient_list */
dlink_list oper_list;           /* network opers */

/* /quote set variables */
struct SetOptions GlobalSetOptions;

/* configuration set from ircd.conf */
struct config_file_entry ConfigFileEntry;
/* server info set from ircd.conf */
struct server_info ServerInfo;
/* admin info set from ircd.conf */
struct admin_info AdminInfo;

struct Counter Count;

struct timeval SystemTime;
int ServerRunning;              /* GLOBAL - server execution state */
struct Client me;               /* That's me */
struct LocalUser meLocalUser;   /* That's also part of me */

time_t startup_time;

int default_server_capabs = CAP_MASK;

int splitmode;
int splitchecking;
int split_users;
int split_servers;
int eob_count;

unsigned long initialVMTop = 0;  /* top of virtual memory at init */
const char *logFileName = LPATH;
const char *pidFileName = PPATH;

char **myargv;
int dorehash = 0;
int dorehashbans = 0;
int doremotd = 0;
int kline_queued = 0;
int server_state_foreground = 0;
int opers_see_all_users = 0;

int testing_conf = 0;

struct config_channel_entry ConfigChannel;
BlockHeap *channel_heap;
BlockHeap *ban_heap;
BlockHeap *topic_heap;
BlockHeap *member_heap;

BlockHeap *client_heap = NULL;
BlockHeap *lclient_heap = NULL;
BlockHeap *pclient_heap = NULL;

char current_uid[IDLEN];

/* patricia */
BlockHeap *prefix_heap;
BlockHeap *node_heap;
BlockHeap *patricia_heap;

BlockHeap *linebuf_heap;

BlockHeap *dnode_heap;

#ifdef NOTYET

void charybdis_initstate(struct IRCdState *self)
{

}

#endif
