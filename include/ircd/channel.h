/*
 *  ircd-ratbox: A slightly useful ircd.
 *  channel.h: The ircd channel header.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2004 ircd-ratbox development team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *  USA
 */

#ifndef INCLUDED_channel_h
#define INCLUDED_channel_h

#include "stdinc.h"
#include "chmode.h"

/* channel status flags */
#define CHFL_PEON		0x0000	/* normal member of channel */
#define CHFL_VOICE      	0x0001	/* the power to speak */
#define CHFL_CHANOP	     	0x0002	/* Channel operator */
#define CHFL_BANNED		0x0008  /* cached as banned */
#define CHFL_QUIETED		0x0010  /* cached as being +q victim */
#define ONLY_SERVERS		0x0020
#define ONLY_OPERS		0x0040
#define ALL_MEMBERS		CHFL_PEON
#define ONLY_CHANOPS		CHFL_CHANOP
#define ONLY_CHANOPSVOICED	(CHFL_CHANOP|CHFL_VOICE)

/* mode structure for channels */
struct Mode
{
	unsigned int mode;
	int limit;
	char key[KEYLEN];
	unsigned int join_num;
	unsigned int join_time;
	char forward[LOC_CHANNELLEN + 1];
};

/* channel structure */
struct Channel
{
	rb_dlink_node node;
	struct Mode mode;
	char *mode_lock;
	char *topic;
	char *topic_info;
	time_t topic_time;
	time_t last_knock;	/* don't allow knock to flood */

	rb_dlink_list members;	/* channel members */
	rb_dlink_list locmembers;	/* local channel members */

	rb_dlink_list invites;
	rb_dlink_list banlist;
	rb_dlink_list exceptlist;
	rb_dlink_list invexlist;
	rb_dlink_list quietlist;

	time_t first_received_message_time;	/* channel flood control */
	int received_number_of_privmsgs;
	int flood_noticed;

	unsigned int join_count;  /* joins within delta */
	unsigned int join_delta;  /* last ts of join */

	time_t bants;
	time_t channelts;
	char *chname;

	struct Client *last_checked_client;
	time_t last_checked_ts;
	unsigned int last_checked_type;
	int last_checked_result;
};

struct membership
{
	rb_dlink_node channode;
	rb_dlink_node locchannode;
	rb_dlink_node usernode;

	struct Channel *chptr;
	struct Client *client_p;
	unsigned int flags;

	time_t bants;
};

#define BANLEN 195
struct Ban
{
	char *banstr;
	char *who;
	time_t when;
	char *forward;
	rb_dlink_node node;
};

struct mode_letter
{
	int mode;
	char letter;
};

struct ChModeChange
{
	char letter;
	const char *arg;
	const char *id;
	int dir;
	int mems;
};

/* can_send results */
#define CAN_SEND_NO	0
#define CAN_SEND_NONOP  1
#define CAN_SEND_OPV	2

#define is_chanop(x)	((x) && (x)->flags & CHFL_CHANOP)
#define is_voiced(x)	((x) && (x)->flags & CHFL_VOICE)
#define is_chanop_voiced(x) ((x) && (x)->flags & (CHFL_CHANOP|CHFL_VOICE))
#define can_send_banned(x) ((x) && (x)->flags & (CHFL_BANNED|CHFL_QUIETED))

/* mode flags for direction indication */
#define MODE_QUERY     0
#define MODE_ADD       1
#define MODE_DEL       -1

#define SecretChannel(x)        ((x) && ((x)->mode.mode & MODE_SECRET))
#define HiddenChannel(x)        ((x) && ((x)->mode.mode & MODE_PRIVATE))
#define PubChannel(x)           ((!x) || ((x)->mode.mode &\
                                 (MODE_PRIVATE | MODE_SECRET)) == 0)

/* channel visible */
#define ShowChannel(v,c)        (PubChannel(c) || IsMember((v),(c)))

#define IsMember(who, chan) ((who && who->user && \
                find_channel_membership(chan, who)) ? 1 : 0)

#define IsChannelName(name) ((name) && (*(name) == '#' || *(name) == '&'))

extern rb_dlink_list global_channel_list;
void init_channels(void);

struct Channel *allocate_channel(const char *chname);
void free_channel(struct Channel *chptr);
struct Ban *allocate_ban(const char *, const char *, const char *);
void free_ban(struct Ban *bptr);


extern void destroy_channel(struct Channel *);

extern int can_send(struct Channel *chptr, struct Client *who,
		    struct membership *);
extern bool flood_attack_channel(int p_or_n, struct Client *source_p,
				struct Channel *chptr, char *chname);
extern int is_banned(struct Channel *chptr, struct Client *who,
		    struct membership *msptr, const char *, const char *, const char **);
extern int is_quieted(struct Channel *chptr, struct Client *who,
		     struct membership *msptr, const char *, const char *);
extern int can_join(struct Client *source_p, struct Channel *chptr,
		    const char *key, const char **forward);

extern struct membership *find_channel_membership(struct Channel *, struct Client *);
extern const char *find_channel_status(struct membership *msptr, int combine);
extern void add_user_to_channel(struct Channel *, struct Client *, int flags);
extern void remove_user_from_channel(struct membership *);
extern void remove_user_from_channels(struct Client *);
extern void invalidate_bancache_user(struct Client *);

extern void free_channel_list(rb_dlink_list *);

extern bool check_channel_name(const char *name);

extern void channel_member_names(struct Channel *chptr, struct Client *,
				 int show_eon);

extern void del_invite(struct Channel *chptr, struct Client *who);

const char *channel_modes(struct Channel *chptr, struct Client *who);

extern struct Channel *find_bannickchange_channel(struct Client *client_p);

extern void check_spambot_warning(struct Client *source_p, const char *name);

extern void check_splitmode(void *);

void set_channel_topic(struct Channel *chptr, const char *topic,
		       const char *topic_info, time_t topicts);

extern void init_chcap_usage_counts(void);
extern void set_chcap_usage_counts(struct Client *serv_p);
extern void unset_chcap_usage_counts(struct Client *serv_p);
extern void send_cap_mode_changes(struct Client *client_p, struct Client *source_p,
				  struct Channel *chptr, struct ChModeChange foo[], int);

void resv_chan_forcepart(const char *name, const char *reason, int temp_time);

extern void set_channel_mode(struct Client *client_p, struct Client *source_p,
            	struct Channel *chptr, struct membership *msptr, int parc, const char *parv[]);
extern void set_channel_mlock(struct Client *client_p, struct Client *source_p,
            	struct Channel *chptr, const char *newmlock, bool propagate);

extern bool add_id(struct Client *source_p, struct Channel *chptr, const char *banid,
	const char *forward, rb_dlink_list * list, long mode_type);

extern struct Ban * del_id(struct Channel *chptr, const char *banid, rb_dlink_list * list,
	long mode_type);

extern int match_extban(const char *banstr, struct Client *client_p, struct Channel *chptr, long mode_type);
extern int valid_extban(const char *banstr, struct Client *client_p, struct Channel *chptr, long mode_type);
const char * get_extban_string(void);

extern int get_channel_access(struct Client *source_p, struct Channel *chptr, struct membership *msptr, int dir, const char *modestr);

extern void send_channel_join(struct Channel *chptr, struct Client *client_p);

#endif /* INCLUDED_channel_h */
