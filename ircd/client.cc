/*
 *  charybdis: an advanced ircd.
 *  client.c: Controls clients.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2005 ircd-ratbox development team
 *  Copyright (C) 2007 William Pitcock
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

#define DEBUG_EXITED_CLIENTS

using namespace ircd;

#include "client_user.h"
#include "client_serv.h"

namespace ircd   {
namespace client {

static void check_pings_list(rb_dlink_list * list);
static void check_pings(void *notused);
static void check_unknowns_list(rb_dlink_list * list);
static void free_exited_clients(void *unused);
static void exit_aborted_clients(void *unused);
static void free_local_client(client *);
static void update_client_exit_stats(client *client_p);
static int exit_remote_client(client *, client *, client *,const char *);
static int exit_remote_server(client *, client *, client *,const char *);
static int exit_local_client(client *, client *, client *,const char *);
static int exit_unknown_client(client *, client *, client *,const char *);
static int exit_local_server(client *, client *, client *,const char *);
static int qs_server(client *, client *, client *, const char *comment);
static void notify_banned_client(client *client_p, struct ConfItem *aconf, int ban);
static void remove_client_from_list(client *client_p);
static void recurse_remove_clients(client *source_p, const char *comment);
static void remove_dependents(client *client_p, client *source_p, client *from, const char *comment, const char *comment1);
static void exit_generic_client(client *client_p, client *source_p, client *from, const char *comment);

} // namespace client

static EVH check_pings;

static rb_bh *lclient_heap = NULL;
static rb_bh *pclient_heap = NULL;
static char current_uid[client::IDLEN];
static uint32_t current_connid = 0;

rb_dictionary *nd_dict = NULL;

enum
{
	D_LINED,
	K_LINED
};

rb_dlink_list dead_list;
#ifdef DEBUG_EXITED_CLIENTS
static rb_dlink_list dead_remote_list;
#endif

struct abort_client
{
 	rb_dlink_node node;
  	client::client *client;
  	char notice[REASONLEN];
};

static rb_dlink_list abort_list;

} // namespace ircd


using namespace ircd;


client::client::client()
:servptr{0}
,from{nullptr}
,wwid{0}
,tsinfo{0}
,mode{(umode)0}
,flags{(enum flags)0}
,snomask{0}
,hopcount{0}
,status{(enum status)0}
,handler{0}
,serial{0}
,first_received_message_time{0}
,received_number_of_privmsgs{0}
,flood_noticed{0}
,localClient{nullptr}
,preClient{nullptr}
,large_ctcp_sent{0}
,certfp{nullptr}
{
}

client::client::~client()
noexcept
{
}

client::user::user &
client::make_user(client &client,
                  const std::string &login)
{
	if (!client.user)
		client.user.reset(new user::user(login));
	else if (client.user->suser != login)
		client.user->suser = login;

	return *client.user;
}

client::serv::serv &
client::make_serv(client &client)
{
	if (!client.serv)
		client.serv.reset(new serv::serv);

	return *client.serv;
}

client::user::user &
ircd::user(client::client &client)
{
	return *client.user;
}

const client::user::user &
ircd::user(const client::client &client)
{
	return *client.user;
}

client::serv::serv &
ircd::serv(client::client &client)
{
	return *client.serv;
}

const client::serv::serv &
ircd::serv(const client::client &client)
{
	return *client.serv;
}


/*
 * init_client
 *
 * inputs	- NONE
 * output	- NONE
 * side effects	- initialize client free memory
 */
void
client::init()
{
	/*
	 * start off the check ping event ..  -- adrian
	 * Every 30 seconds is plenty -- db
	 */
	lclient_heap = rb_bh_create(sizeof(struct LocalUser), LCLIENT_HEAP_SIZE, "lclient_heap");
	pclient_heap = rb_bh_create(sizeof(struct PreClient), PCLIENT_HEAP_SIZE, "pclient_heap");

	rb_event_addish("check_pings", check_pings, NULL, 30);
	rb_event_addish("free_exited_clients", &free_exited_clients, NULL, 4);
	rb_event_addish("exit_aborted_clients", exit_aborted_clients, NULL, 1);
	rb_event_add("flood_recalc", flood_recalc, NULL, 1);

	nd_dict = rb_dictionary_create("nickdelay", []
	(const void *a, const void *b) -> int
	{
		return irccmp((const char *)a, (const char *)b);
	});
}

/*
 * connid_get - allocate a connid
 *
 * inputs       - none
 * outputs      - a connid token which is used to represent a logical circuit
 * side effects - current_connid is incremented, possibly multiple times.
 *                the association of the connid to it's client is committed.
 */
uint32_t
client::connid_get(client *client_p)
{
	s_assert(my_connect(*client_p));
	if (!my_connect(*client_p))
		return 0;

	/* find a connid that is available */
	while (find_cli_connid_hash(++current_connid) != NULL)
	{
		/* handle wraparound, current_connid must NEVER be 0 */
		if (current_connid == 0)
			++current_connid;
	}

	add_to_cli_connid_hash(client_p, current_connid);
	rb_dlinkAddAlloc(RB_UINT_TO_POINTER(current_connid), &client_p->localClient->connids);

	return current_connid;
}

/*
 * connid_put - free a connid
 *
 * inputs       - connid to free
 * outputs      - nothing
 * side effects - connid bookkeeping structures are freed
 */
void
client::connid_put(uint32_t id)
{
	client *client_p;

	s_assert(id != 0);
	if (id == 0)
		return;

	client_p = find_cli_connid_hash(id);
	if (client_p == NULL)
		return;

	del_from_cli_connid_hash(id);
	rb_dlinkFindDestroy(RB_UINT_TO_POINTER(id), &client_p->localClient->connids);
}

/*
 * client_release_connids - release any connids still attached to a client
 *
 * inputs       - client to garbage collect
 * outputs      - none
 * side effects - client's connids are garbage collected
 */
void
client::client_release_connids(client *client_p)
{
	rb_dlink_node *ptr, *ptr2;

	if (client_p->localClient->connids.head)
		s_assert(my_connect(*client_p));

	if (!my_connect(*client_p))
		return;

	RB_DLINK_FOREACH_SAFE(ptr, ptr2, client_p->localClient->connids.head)
		connid_put(RB_POINTER_TO_UINT(ptr->data));
}

/*
 * make_client - create a new Client struct and set it to initial state.
 *
 *      from == NULL,   create local client (a client connected
 *                      to a socket).
 *
 *      from,   create remote client (behind a socket
 *                      associated with the client defined by
 *                      'from'). ('from' is a local client!!).
 */
client::client *
client::make_client(client *from)
{
	struct LocalUser *localClient;
	client *client_p = new client;

	if(from == NULL)
	{
		client_p->from = client_p;	/* 'from' of local client is self! */

		localClient = reinterpret_cast<LocalUser *>(rb_bh_alloc(lclient_heap));
		set_my_connect(*client_p);
		client_p->localClient = localClient;

		client_p->localClient->lasttime = client_p->localClient->firsttime = rb_current_time();

		client_p->localClient->F = NULL;

		client_p->preClient = reinterpret_cast<PreClient *>(rb_bh_alloc(pclient_heap));

		/* as good a place as any... */
		rb_dlinkAdd(client_p, &client_p->localClient->tnode, &unknown_list);
	}
	else
	{			/* from is not NULL */
		client_p->localClient = NULL;
		client_p->preClient = NULL;
		client_p->from = from;	/* 'from' of local client is self! */
	}

	set_unknown(*client_p);
	rb_strlcpy(client_p->username, "unknown", sizeof(client_p->username));

	return client_p;
}

void
client::free_pre_client(client *client_p)
{
	s_assert(NULL != client_p);

	if(client_p->preClient == NULL)
		return;

	s_assert(client_p->preClient->auth.cid == 0);

	rb_free(client_p->preClient->auth.data);
	rb_free(client_p->preClient->auth.reason);

	rb_bh_free(pclient_heap, client_p->preClient);
	client_p->preClient = NULL;
}

void
client::free_local_client(client *client_p)
{
	s_assert(NULL != client_p);
	s_assert(&me != client_p);

	if(client_p->localClient == NULL)
		return;

	/*
	 * clean up extra sockets from P-lines which have been discarded.
	 */
	if(client_p->localClient->listener)
	{
		s_assert(0 < client_p->localClient->listener->ref_count);
		if(0 == --client_p->localClient->listener->ref_count
		   && !client_p->localClient->listener->active)
			free_listener(client_p->localClient->listener);
		client_p->localClient->listener = 0;
	}

	client_release_connids(client_p);
	if(client_p->localClient->F != NULL)
	{
		rb_close(client_p->localClient->F);
	}

	if(client_p->localClient->passwd)
	{
		memset(client_p->localClient->passwd, 0,
			strlen(client_p->localClient->passwd));
		rb_free(client_p->localClient->passwd);
	}

	rb_free(client_p->localClient->auth_user);
	rb_free(client_p->localClient->challenge);
	rb_free(client_p->localClient->fullcaps);
	rb_free(client_p->localClient->opername);
	rb_free(client_p->localClient->mangledhost);
	if (client_p->localClient->privset)
		privilegeset_unref(client_p->localClient->privset);

	if (IsSSL(client_p))
		ssld_decrement_clicount(client_p->localClient->ssl_ctl);

	if (IsCapable(client_p, CAP_ZIP))
		ssld_decrement_clicount(client_p->localClient->z_ctl);

	if (client_p->localClient->ws_ctl != NULL)
		wsockd_decrement_clicount(client_p->localClient->ws_ctl);

	rb_bh_free(lclient_heap, client_p->localClient);
	client_p->localClient = NULL;
}

void
client::free_client(client *client_p)
{
	s_assert(NULL != client_p);
	s_assert(&me != client_p);
	free_local_client(client_p);
	free_pre_client(client_p);
	rb_free(client_p->certfp);
	delete client_p;
}

/*
 * check_pings - go through the local client list and check activity
 * kill off stuff that should die
 *
 * inputs       - NOT USED (from event)
 * output       - next time_t when check_pings() should be called again
 * side effects -
 *
 *
 * A PING can be sent to clients as necessary.
 *
 * Client/Server ping outs are handled.
 */

/*
 * Addon from adrian. We used to call this after nextping seconds,
 * however I've changed it to run once a second. This is only for
 * PING timeouts, not K/etc-line checks (thanks dianora!). Having it
 * run once a second makes life a lot easier - when a new client connects
 * and they need a ping in 4 seconds, if nextping was set to 20 seconds
 * we end up waiting 20 seconds. This is stupid. :-)
 * I will optimise (hah!) check_pings() once I've finished working on
 * tidying up other network IO evilnesses.
 *     -- adrian
 */
void
client::check_pings(void *notused)
{
	check_pings_list(&lclient_list);
	check_pings_list(&serv_list);
	check_unknowns_list(&unknown_list);
}

/*
 * Check_pings_list()
 *
 * inputs	- pointer to list to check
 * output	- NONE
 * side effects	-
 */
void
client::check_pings_list(rb_dlink_list * list)
{
	char scratch[32];	/* way too generous but... */
	client *client_p;	/* current local client_p being examined */
	int ping = 0;		/* ping time value from client */
	rb_dlink_node *ptr, *next_ptr;

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, list->head)
	{
		client_p = reinterpret_cast<client *>(ptr->data);

		if(!my_connect(*client_p) || is_dead(*client_p))
			continue;

		ping = get_client_ping(client_p);

		if(ping < (rb_current_time() - client_p->localClient->lasttime))
		{
			/*
			 * If the client/server hasnt talked to us in 2*ping seconds
			 * and it has a ping time, then close its connection.
			 */
			if(((rb_current_time() - client_p->localClient->lasttime) >= (2 * ping)
			    && (client_p->flags & flags::PINGSENT)))
			{
				if(is_server(*client_p))
				{
					sendto_realops_snomask(sno::GENERAL, L_ALL,
							     "No response from %s, closing link",
							     client_p->name);
					ilog(L_SERVER,
					     "No response from %s, closing link",
					     log_client_name(client_p, HIDE_IP));
				}
				(void) snprintf(scratch, sizeof(scratch),
						  "Ping timeout: %d seconds",
						  (int) (rb_current_time() - client_p->localClient->lasttime));

				exit_client(client_p, client_p, &me, scratch);
				continue;
			}
			else if((client_p->flags & flags::PINGSENT) == 0)
			{
				/*
				 * if we havent PINGed the connection and we havent
				 * heard from it in a while, PING it to make sure
				 * it is still alive.
				 */
				client_p->flags |= flags::PINGSENT;
				/* not nice but does the job */
				client_p->localClient->lasttime = rb_current_time() - ping;
				sendto_one(client_p, "PING :%s", me.name);
			}
		}
		/* ping_timeout: */

	}
}

/*
 * check_unknowns_list
 *
 * inputs	- pointer to list of unknown clients
 * output	- NONE
 * side effects	- unknown clients get marked for termination after n seconds
 */
void
client::check_unknowns_list(rb_dlink_list * list)
{
	rb_dlink_node *ptr, *next_ptr;
	client *client_p;
	int timeout;

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, list->head)
	{
		client_p = reinterpret_cast<client *>(ptr->data);

		if(is_dead(*client_p) || is_closing(*client_p))
			continue;

		/* Still querying with authd */
		if(client_p->preClient != NULL && client_p->preClient->auth.cid != 0)
			continue;

		/*
		 * Check UNKNOWN connections - if they have been in this state
		 * for > 30s, close them.
		 */

		timeout = is_any_server(*client_p) ? ConfigFileEntry.connect_timeout : 30;
		if((rb_current_time() - client_p->localClient->firsttime) > timeout)
		{
			if(is_any_server(*client_p))
			{
				sendto_realops_snomask(sno::GENERAL, is_remote_connect(client_p) ? L_NETWIDE : L_ALL,
						     "No response from %s, closing link",
						     client_p->name);
				ilog(L_SERVER,
				     "No response from %s, closing link",
				     log_client_name(client_p, HIDE_IP));
			}
			exit_client(client_p, client_p, &me, "Connection timed out");
		}
	}
}

void
client::notify_banned_client(client *client_p, struct ConfItem *aconf, int ban)
{
	static const char conn_closed[] = "Connection closed";
	static const char d_lined[] = "D-lined";
	static const char k_lined[] = "K-lined";
	const char *reason = NULL;
	const char *exit_reason = conn_closed;

	if(ConfigFileEntry.kline_with_reason)
	{
		reason = get_user_ban_reason(aconf);
		exit_reason = reason;
	}
	else
	{
		reason = aconf->status == D_LINED ? d_lined : k_lined;
	}

	if(ban == D_LINED && !is_person(*client_p))
		sendto_one(client_p, "NOTICE DLINE :*** You have been D-lined");
	else
		sendto_one(client_p, form_str(ERR_YOUREBANNEDCREEP),
			   me.name, client_p->name, reason);

	exit_client(client_p, client_p, &me,
			EmptyString(ConfigFileEntry.kline_reason) ? exit_reason :
			 ConfigFileEntry.kline_reason);
}

/*
 * check_banned_lines
 * inputs	- NONE
 * output	- NONE
 * side effects - Check all connections for a pending k/dline against the
 * 		  client, exit the client if found.
 */
void
client::check_banned_lines(void)
{
	check_dlines();
	check_klines();
	check_xlines();
}

/* check_klines_event()
 *
 * inputs	-
 * outputs	-
 * side effects - check_klines() is called, kline_queued unset
 */
void
client::check_klines_event(void *unused)
{
	kline_queued = false;
	check_klines();
}

/* check_klines
 *
 * inputs       -
 * outputs      -
 * side effects - all clients will be checked for klines
 */
void
client::check_klines(void)
{
	client *client_p;
	struct ConfItem *aconf;
	rb_dlink_node *ptr;
	rb_dlink_node *next_ptr;

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, lclient_list.head)
	{
		client_p = reinterpret_cast<client *>(ptr->data);

		if(is_me(*client_p) || !is_person(*client_p))
			continue;

		if((aconf = find_kline(client_p)) != NULL)
		{
			if(is_exempt_kline(*client_p))
			{
				sendto_realops_snomask(sno::GENERAL, L_ALL,
						     "KLINE over-ruled for %s, client is kline_exempt [%s@%s]",
						     get_client_name(client_p, HIDE_IP),
						     aconf->user, aconf->host);
				continue;
			}

			sendto_realops_snomask(sno::GENERAL, L_ALL,
					     "KLINE active for %s",
					     get_client_name(client_p, HIDE_IP));

			notify_banned_client(client_p, aconf, K_LINED);
			continue;
		}
	}
}

/* check_dlines()
 *
 * inputs       -
 * outputs      -
 * side effects - all clients will be checked for dlines
 */
void
client::check_dlines(void)
{
	client *client_p;
	struct ConfItem *aconf;
	rb_dlink_node *ptr;
	rb_dlink_node *next_ptr;

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, lclient_list.head)
	{
		client_p = reinterpret_cast<client *>(ptr->data);

		if(is_me(*client_p))
			continue;

		if((aconf = find_dline((struct sockaddr *)&client_p->localClient->ip, GET_SS_FAMILY(&client_p->localClient->ip))) != NULL)
		{
			if(aconf->status & CONF_EXEMPTDLINE)
				continue;

			sendto_realops_snomask(sno::GENERAL, L_ALL,
					     "DLINE active for %s",
					     get_client_name(client_p, HIDE_IP));

			notify_banned_client(client_p, aconf, D_LINED);
			continue;
		}
	}

	/* dlines need to be checked against unknowns too */
	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, unknown_list.head)
	{
		client_p = reinterpret_cast<client *>(ptr->data);

		if((aconf = find_dline((struct sockaddr *)&client_p->localClient->ip, GET_SS_FAMILY(&client_p->localClient->ip))) != NULL)
		{
			if(aconf->status & CONF_EXEMPTDLINE)
				continue;

			notify_banned_client(client_p, aconf, D_LINED);
		}
	}
}

/* check_xlines
 *
 * inputs       -
 * outputs      -
 * side effects - all clients will be checked for xlines
 */
void
client::check_xlines(void)
{
	client *client_p;
	struct ConfItem *aconf;
	rb_dlink_node *ptr;
	rb_dlink_node *next_ptr;

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, lclient_list.head)
	{
		client_p = reinterpret_cast<client *>(ptr->data);

		if(is_me(*client_p) || !is_person(*client_p))
			continue;

		if((aconf = find_xline(client_p->info, 1)) != NULL)
		{
			if(is_exempt_kline(*client_p))
			{
				sendto_realops_snomask(sno::GENERAL, L_ALL,
						     "XLINE over-ruled for %s, client is kline_exempt [%s]",
						     get_client_name(client_p, HIDE_IP),
						     aconf->host);
				continue;
			}

			sendto_realops_snomask(sno::GENERAL, L_ALL, "XLINE active for %s",
					     get_client_name(client_p, HIDE_IP));

			(void) exit_client(client_p, client_p, &me, "Bad user info");
			continue;
		}
	}
}

/* resv_nick_fnc
 *
 * inputs		- resv, reason, time
 * outputs		- NONE
 * side effects	- all local clients matching resv will be FNC'd
 */
void
client::resv_nick_fnc(const char *mask, const char *reason, int temp_time)
{
	client *client_p, *target_p;
	rb_dlink_node *ptr;
	rb_dlink_node *next_ptr;
	char *nick;
	char note[NICKLEN+10];

	if (!ConfigFileEntry.resv_fnc)
		return;

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, lclient_list.head)
	{
		client_p = reinterpret_cast<client *>(ptr->data);

		if(is_me(*client_p) || !is_person(*client_p) || is_exempt_resv(*client_p))
			continue;

		/* Skip users that already have UID nicks. */
		if(rfc1459::is_digit(client_p->name[0]))
			continue;

		if(match_esc(mask, client_p->name))
		{
			nick = client_p->id;

			/* Tell opers. */
			sendto_realops_snomask(sno::GENERAL, L_ALL,
				"RESV forced nick change for %s!%s@%s to %s; nick matched [%s] (%s)",
				client_p->name, client_p->username, client_p->host, nick, mask, reason);

			sendto_realops_snomask(sno::NCHANGE, L_ALL,
				"Nick change: From %s to %s [%s@%s]",
				client_p->name, nick, client_p->username, client_p->host);

			/* Tell the user. */
			if (temp_time > 0)
			{
				sendto_one_notice(client_p,
					":*** Nick %s is temporarily unavailable on this server.",
					client_p->name);
			}
			else
			{
				sendto_one_notice(client_p,
					":*** Nick %s is no longer available on this server.",
					client_p->name);
			}

			/* Do all of the nick-changing gymnastics. */
			client_p->tsinfo = rb_current_time();
			whowas::add(*client_p);

			monitor_signoff(client_p);

			chan::invalidate_bancache_user(client_p);

			sendto_common_channels_local(client_p, NOCAPS, NOCAPS, ":%s!%s@%s NICK :%s",
				client_p->name, client_p->username, client_p->host, nick);
			sendto_server(client_p, NULL, CAP_TS6, NOCAPS, ":%s NICK %s :%ld",
				use_id(client_p), nick, (long) client_p->tsinfo);

			del_from_client_hash(client_p->name, client_p);
			rb_strlcpy(client_p->name, nick, sizeof(client_p->name));
			add_to_client_hash(nick, client_p);

			monitor_signon(client_p);

			RB_DLINK_FOREACH_SAFE(ptr, next_ptr, client_p->on_allow_list.head)
			{
				target_p = reinterpret_cast<client *>(ptr->data);
				rb_dlinkFindDestroy(client_p, &target_p->localClient->allow_list);
				rb_dlinkDestroy(ptr, &client_p->on_allow_list);
			}

			snprintf(note, sizeof(note), "Nick: %s", nick);
			rb_note(client_p->localClient->F, note);
		}
	}
}

/*
 * update_client_exit_stats
 *
 * input	- pointer to client
 * output	- NONE
 * side effects	-
 */
void
client::update_client_exit_stats(client *client_p)
{
	if(is_server(*client_p))
	{
		sendto_realops_snomask(sno::EXTERNAL, L_ALL,
				     "Server %s split from %s",
				     client_p->name, client_p->servptr->name);
		if(has_sent_eob(*client_p))
			eob_count--;
	}
	else if(is_client(*client_p))
	{
		--Count.total;
		if(is(*client_p, umode::OPER))
			--Count.oper;
		if(is(*client_p, umode::INVISIBLE))
			--Count.invisi;
	}

	if(splitchecking && !splitmode)
		chan::check_splitmode(NULL);
}

/*
 * remove_client_from_list
 * inputs	- point to client to remove
 * output	- NONE
 * side effects - taken the code from ExitOneClient() for this
 *		  and placed it here. - avalon
 */
void
client::remove_client_from_list(client *client_p)
{
	s_assert(NULL != client_p);

	if(client_p == NULL)
		return;

	/* A client made with make_client()
	 * is on the unknown_list until removed.
	 * If it =does= happen to exit before its removed from that list
	 * and its =not= on the global_client_list, it will core here.
	 * short circuit that case now -db
	 */
	if(client_p->node.prev == NULL && client_p->node.next == NULL)
		return;

	rb_dlinkDelete(&client_p->node, &global_client_list);

	update_client_exit_stats(client_p);
}


/* clean_nick()
 *
 * input	- nickname to check, flag for nick from local client
 * output	- 0 if erroneous, else 1
 * side effects -
 */
int
client::clean_nick(const char *nick, int loc_client)
{
	int len = 0;

	/* nicks cant start with a digit or -, and must have a length */
	if(*nick == '-' || *nick == '\0')
		return 0;

	if(loc_client && rfc1459::is_digit(*nick))
		return 0;

	for(; *nick; nick++)
	{
		len++;
		if(!rfc1459::is_nick(*nick))
			return 0;
	}

	/* nicklen is +1 */
	if(len >= NICKLEN && (unsigned int)len >= ConfigFileEntry.nicklen)
		return 0;

	return 1;
}

/*
 * find_person	- find person by (nick)name.
 * inputs	- pointer to name
 * output	- return client pointer
 * side effects -
 */
client::client *
client::find_person(const char *name)
{
	client *c2ptr;

	c2ptr = find_client(name);

	if(c2ptr && is_person(*c2ptr))
		return (c2ptr);
	return (NULL);
}

client::client *
client::find_named_person(const char *name)
{
	client *c2ptr;

	c2ptr = find_named_client(name);

	if(c2ptr && is_person(*c2ptr))
		return (c2ptr);
	return (NULL);
}

client::client *
client::find_named_person(const std::string &name)
{
	return find_named_person(name.c_str());
}

/*
 * find_chasing - find the client structure for a nick name (user)
 *      using history mechanism if necessary. If the client is not found,
 *      an error message (NO SUCH NICK) is generated. If the client was found
 *      through the history, chasing will be 1 and otherwise 0.
 */
client::client *
client::find_chasing(client *source_p, const char *user, int *chasing)
{
	client *who;

	if(my(*source_p))
		who = find_named_person(user);
	else
		who = find_person(user);

	if(chasing)
		*chasing = 0;

	if(who || rfc1459::is_digit(*user))
		return who;

	const auto history(whowas::history(user, KILLCHASETIMELIMIT));
	if(history.empty())
	{
		sendto_one_numeric(source_p, ERR_NOSUCHNICK, form_str(ERR_NOSUCHNICK), user);
		return nullptr;
	}

	if(chasing)
		*chasing = 1;

	return history.back()->online;
}

/*
 * get_client_name -  Return the name of the client
 *    for various tracking and
 *      admin purposes. The main purpose of this function is to
 *      return the "socket host" name of the client, if that
 *        differs from the advertised name (other than case).
 *        But, this can be used to any client structure.
 *
 * NOTE 1:
 *        Watch out the allocation of "nbuf", if either source_p->name
 *        or source_p->sockhost gets changed into pointers instead of
 *        directly allocated within the structure...
 *
 * NOTE 2:
 *        Function return either a pointer to the structure (source_p) or
 *        to internal buffer (nbuf). *NEVER* use the returned pointer
 *        to modify what it points!!!
 */

const char *
client::get_client_name(client *client, int showip)
{
	static char nbuf[HOSTLEN * 2 + USERLEN + 5];

	s_assert(NULL != client);
	if(client == NULL)
		return NULL;

	if(my_connect(*client))
	{
		if(!irccmp(client->name, client->host))
			return client->name;

		if(ConfigFileEntry.hide_spoof_ips &&
		   showip == SHOW_IP && is_ip_spoof(*client))
			showip = MASK_IP;
		if(is_any_server(*client))
			showip = MASK_IP;

		/* And finally, let's get the host information, ip or name */
		switch (showip)
		{
		case SHOW_IP:
			snprintf(nbuf, sizeof(nbuf), "%s[%s@%s]",
				   client->name, client->username,
				   client->sockhost);
			break;
		case MASK_IP:
			snprintf(nbuf, sizeof(nbuf), "%s[%s@255.255.255.255]",
				   client->name, client->username);
			break;
		default:
			snprintf(nbuf, sizeof(nbuf), "%s[%s@%s]",
				   client->name, client->username, client->host);
		}
		return nbuf;
	}

	/* As pointed out by Adel Mezibra
	 * Neph|l|m@EFnet. Was missing a return here.
	 */
	return client->name;
}

/* log_client_name()
 *
 * This version is the same as get_client_name, but doesnt contain the
 * code that will hide IPs always.  This should be used for logfiles.
 */
const char *
client::log_client_name(client *target_p, int showip)
{
	static char nbuf[HOSTLEN * 2 + USERLEN + 5];

	if(target_p == NULL)
		return NULL;

	if(my_connect(*target_p))
	{
		if(irccmp(target_p->name, target_p->host) == 0)
			return target_p->name;

		switch (showip)
		{
		case SHOW_IP:
			snprintf(nbuf, sizeof(nbuf), "%s[%s@%s]", target_p->name,
				   target_p->username, target_p->sockhost);
			break;

		default:
			snprintf(nbuf, sizeof(nbuf), "%s[%s@%s]", target_p->name,
				   target_p->username, target_p->host);
		}

		return nbuf;
	}

	return target_p->name;
}

/* is_remote_connect - Returns whether a server was /connect'ed by a remote
 * oper (send notices netwide) */
int
client::is_remote_connect(client *client_p)
{
	client *oper;

	if (client_p->serv == NULL)
		return FALSE;

	oper = find_named_person(client_p->serv->by);
	return oper != NULL && my_oper(*oper);
}

void
client::free_exited_clients(void *unused)
{
	rb_dlink_node *ptr, *next;
	client *target_p;

	RB_DLINK_FOREACH_SAFE(ptr, next, dead_list.head)
	{
		target_p = reinterpret_cast<client *>(ptr->data);

#ifdef DEBUG_EXITED_CLIENTS
		{
			struct abort_client *abt;
			rb_dlink_node *aptr;
			int found = 0;

			RB_DLINK_FOREACH(aptr, abort_list.head)
			{
				abt = reinterpret_cast<abort_client *>(aptr->data);
				if(abt->client == target_p)
				{
					s_assert(0);
					sendto_realops_snomask(sno::GENERAL, L_ALL,
						"On abort_list: %s stat: %u flags: %llu handler: %c",
						target_p->name, (unsigned int) target_p->status,
						(unsigned long long)target_p->flags,  target_p->handler);
					sendto_realops_snomask(sno::GENERAL, L_ALL,
						"Please report this to the charybdis developers!");
					found++;
				}
			}

			if(found)
			{
				rb_dlinkDestroy(ptr, &dead_list);
				continue;
			}
		}
#endif

		if(ptr->data == NULL)
		{
			sendto_realops_snomask(sno::GENERAL, L_ALL,
					     "Warning: null client on dead_list!");
			rb_dlinkDestroy(ptr, &dead_list);
			continue;
		}

		free_client(target_p);
		rb_dlinkDestroy(ptr, &dead_list);
	}

#ifdef DEBUG_EXITED_CLIENTS
	RB_DLINK_FOREACH_SAFE(ptr, next, dead_remote_list.head)
	{
		target_p = reinterpret_cast<client *>(ptr->data);

		if(ptr->data == NULL)
		{
			sendto_realops_snomask(sno::GENERAL, L_ALL,
					     "Warning: null client on dead_list!");
			rb_dlinkDestroy(ptr, &dead_list);
			continue;
		}

		free_client(target_p);
		rb_dlinkDestroy(ptr, &dead_remote_list);
	}
#endif

}

/*
** Remove all clients that depend on source_p; assumes all (S)QUITs have
** already been sent.  we make sure to exit a server's dependent clients
** and servers before the server itself; exit_one_client takes care of
** actually removing things off llists.   tweaked from +CSr31  -orabidoo
 */
/*
 * added sanity test code.... source_p->serv might be NULL...
 */
void
client::recurse_remove_clients(client *source_p, const char *comment)
{
	using ircd::serv;

	if (is_me(*source_p))
		return;

	if (!source_p->serv)
		return;

	/* this is very ugly, but it saves cpu :P */
	if (ConfigFileEntry.nick_delay > 0)
	{
		for (const auto &user : users(serv(*source_p)))
		{
			user->flags |= flags::KILLED;
			add_nd_entry(user->name);
			if (!is_dead(*user) && !is_closing(*user))
				exit_remote_client(NULL, user, &me, comment);
		}
	}
	else
	{
		for (const auto &user : users(serv(*source_p)))
		{
			user->flags |= flags::KILLED;
			if (!is_dead(*user) && !is_closing(*user))
				exit_remote_client(NULL, user, &me, comment);
		}
	}

	for (const auto &server : servers(serv(*source_p)))
	{
		recurse_remove_clients(server, comment);
		qs_server(NULL, server, &me, comment);
	}
}

/*
** Remove *everything* that depends on source_p, from all lists, and sending
** all necessary SQUITs.  source_p itself is still on the lists,
** and its SQUITs have been sent except for the upstream one  -orabidoo
 */
void
client::remove_dependents(client *client_p,
		  client *source_p,
		  client *from, const char *comment, const char *comment1)
{
	client *to;
	rb_dlink_node *ptr, *next;

	RB_DLINK_FOREACH_SAFE(ptr, next, serv_list.head)
	{
		to = reinterpret_cast<client *>(ptr->data);

		if(is_me(*to) || to == source_p->from || to == client_p)
			continue;

		sendto_one(to, "SQUIT %s :%s", get_id(source_p, to), comment);
	}

	recurse_remove_clients(source_p, comment1);
}

void
client::exit_aborted_clients(void *unused)
{
	struct abort_client *abt;
 	rb_dlink_node *ptr, *next;
 	RB_DLINK_FOREACH_SAFE(ptr, next, abort_list.head)
 	{
		abt = reinterpret_cast<abort_client *>(ptr->data);

#ifdef DEBUG_EXITED_CLIENTS
		{
			if(rb_dlinkFind(abt->client, &dead_list))
			{
				s_assert(0);
				sendto_realops_snomask(sno::GENERAL, L_ALL,
					"On dead_list: %s stat: %u flags: %llu handler: %c",
					abt->client->name, (unsigned int) abt->client->status,
					(unsigned long long)abt->client->flags, abt->client->handler);
				sendto_realops_snomask(sno::GENERAL, L_ALL,
					"Please report this to the charybdis developers!");
				continue;
			}
		}
#endif

 		s_assert(*((unsigned long*)abt->client) != 0xdeadbeef); /* This is lame but its a debug thing */
 	 	rb_dlinkDelete(ptr, &abort_list);

 	 	if(is_any_server(*abt->client))
 	 	 	sendto_realops_snomask(sno::GENERAL, L_ALL,
  	 	 	                     "Closing link to %s: %s",
   	 	 	                      abt->client->name, abt->notice);

		/* its no longer on abort list - we *must* remove
		 * FLAGS_CLOSING otherwise exit_client() will not run --fl
		 */
		abt->client->flags &= ~flags::CLOSING;
 	 	exit_client(abt->client, abt->client, &me, abt->notice);
 	 	rb_free(abt);
 	}
}


/*
 * dead_link - Adds client to a list of clients that need an exit_client()
 */
void
client::dead_link(client *client_p, int sendqex)
{
	struct abort_client *abt;

	s_assert(!is_me(*client_p));
	if(is_dead(*client_p) || is_closing(*client_p) || is_me(*client_p))
		return;

	abt = (struct abort_client *) rb_malloc(sizeof(struct abort_client));

	if(sendqex)
		rb_strlcpy(abt->notice, "Max SendQ exceeded", sizeof(abt->notice));
	else
		snprintf(abt->notice, sizeof(abt->notice), "Write error: %s", strerror(errno));

    	abt->client = client_p;
	set_io_error(*client_p);
	set_dead(*client_p);
	set_closing(*client_p);
	rb_dlinkAdd(abt, &abt->node, &abort_list);
}


/* This does the remove of the user from channels..local or remote */
void
client::exit_generic_client(client *client_p, client *source_p, client *from,
		   const char *comment)
{
	using ircd::user;

	rb_dlink_node *ptr, *next_ptr;

	if(is(*source_p, umode::OPER))
		rb_dlinkFindDestroy(source_p, &oper_list);

	sendto_common_channels_local(source_p, NOCAPS, NOCAPS, ":%s!%s@%s QUIT :%s",
				     source_p->name,
				     source_p->username, source_p->host, comment);

	chan::del(*source_p);

	/* Should not be in any channels now */
	s_assert(chans(user(*source_p)).empty());

	// Clean up invitefield
	for (auto &chan : invites(user(*source_p)))
		chan->invites.erase(source_p);

	/* Clean up allow lists */
	del_all_accepts(source_p);

	whowas::add(*source_p);
	whowas::off(*source_p);

	monitor_signoff(source_p);

	if(has_id(source_p))
		del_from_id_hash(source_p->id, source_p);

	del_from_hostname_hash(source_p->orighost, source_p);
	del_from_client_hash(source_p->name, source_p);
	remove_client_from_list(source_p);
}

/*
 * Assumes is_person(*source_p) && !my_connect(*source_p)
 */

int
client::exit_remote_client(client *client_p, client *source_p, client *from,
		   const char *comment)
{
	exit_generic_client(client_p, source_p, from, comment);

	if(source_p->servptr && source_p->servptr->serv)
		source_p->servptr->serv->users.erase(source_p->lnode);

	if((source_p->flags & flags::KILLED) == 0)
	{
		sendto_server(client_p, NULL, CAP_TS6, NOCAPS,
			      ":%s QUIT :%s", use_id(source_p), comment);
	}

	set_dead(*source_p);
#ifdef DEBUG_EXITED_CLIENTS
	rb_dlinkAddAlloc(source_p, &dead_remote_list);
#else
	rb_dlinkAddAlloc(source_p, &dead_list);
#endif
	return(CLIENT_EXITED);
}

/*
 * This assumes is_unknown(*source_p) == true and my_connect(*source_p) == true
 */

int
client::exit_unknown_client(client *client_p, /* The local client originating the
                                              * exit or NULL, if this exit is
                                              * generated by this server for
                                              * internal reasons.
                                              * This will not get any of the
                                              * generated messages. */
		client *source_p,     /* Client exiting */
		client *from,         /* Client firing off this Exit,
                                              * never NULL! */
		const char *comment)
{
	authd_abort_client(source_p);
	rb_dlinkDelete(&source_p->localClient->tnode, &unknown_list);

	if(!is_io_error(*source_p))
		sendto_one(source_p, "ERROR :Closing Link: %s (%s)",
			source_p->user != NULL ? source_p->host : "127.0.0.1",
			comment);

	close_connection(source_p);

	if(has_id(source_p))
		del_from_id_hash(source_p->id, source_p);

	del_from_hostname_hash(source_p->host, source_p);
	del_from_client_hash(source_p->name, source_p);
	remove_client_from_list(source_p);
	set_dead(*source_p);
	rb_dlinkAddAlloc(source_p, &dead_list);

	/* Note that we don't need to add unknowns to the dead_list */
	return(CLIENT_EXITED);
}

int
client::exit_remote_server(client *client_p, client *source_p, client *from,
		  const char *comment)
{
	static char comment1[(HOSTLEN*2)+2];
	static char newcomment[BUFSIZE];
	client *target_p;

	if(ConfigServerHide.flatten_links)
		strcpy(comment1, "*.net *.split");
	else
	{
		strcpy(comment1, source_p->servptr->name);
		strcat(comment1, " ");
		strcat(comment1, source_p->name);
	}
	if (is_person(*from))
		snprintf(newcomment, sizeof(newcomment), "by %s: %s",
				from->name, comment);

	if(source_p->serv != NULL)
		remove_dependents(client_p, source_p, from, is_person(*from) ? newcomment : comment, comment1);

	if(source_p->servptr && source_p->servptr->serv)
		source_p->servptr->serv->servers.erase(source_p->lnode);
	else
		s_assert(0);

	rb_dlinkFindDestroy(source_p, &global_serv_list);
	target_p = source_p->from;

	if(target_p != NULL && is_server(*target_p) && target_p != client_p &&
	   !is_me(*target_p) && (source_p->flags & flags::KILLED) == 0)
	{
		sendto_one(target_p, ":%s SQUIT %s :%s",
			   get_id(from, target_p), get_id(source_p, target_p),
			   comment);
	}

	if(has_id(source_p))
		del_from_id_hash(source_p->id, source_p);

	del_from_client_hash(source_p->name, source_p);
	remove_client_from_list(source_p);
	split(nameinfo(*source_p->serv));

	set_dead(*source_p);
#ifdef DEBUG_EXITED_CLIENTS
	rb_dlinkAddAlloc(source_p, &dead_remote_list);
#else
	rb_dlinkAddAlloc(source_p, &dead_list);
#endif
	return 0;
}

int
client::qs_server(client *client_p, client *source_p, client *from,
		  const char *comment)
{
	if(source_p->servptr && source_p->servptr->serv)
		source_p->servptr->serv->servers.erase(source_p->lnode);
	else
		s_assert(0);

	rb_dlinkFindDestroy(source_p, &global_serv_list);

	if(has_id(source_p))
		del_from_id_hash(source_p->id, source_p);

	del_from_client_hash(source_p->name, source_p);
	remove_client_from_list(source_p);
	split(nameinfo(*source_p->serv));

	set_dead(*source_p);
	rb_dlinkAddAlloc(source_p, &dead_list);
	return 0;
}

int
client::exit_local_server(client *client_p, client *source_p, client *from,
		  const char *comment)
{
	static char comment1[(HOSTLEN*2)+2];
	static char newcomment[BUFSIZE];
	unsigned int sendk, recvk;

	rb_dlinkDelete(&source_p->localClient->tnode, &serv_list);
	rb_dlinkFindDestroy(source_p, &global_serv_list);

	sendk = source_p->localClient->sendK;
	recvk = source_p->localClient->receiveK;

	/* Always show source here, so the server notices show
	 * which side initiated the split -- jilles
	 */
	snprintf(newcomment, sizeof(newcomment), "by %s: %s",
			from == source_p ? me.name : from->name, comment);
	if (!is_io_error(*source_p))
		sendto_one(source_p, "SQUIT %s :%s", use_id(source_p),
				newcomment);
	if(client_p != NULL && source_p != client_p && !is_io_error(*source_p))
	{
		sendto_one(source_p, "ERROR :Closing Link: 127.0.0.1 %s (%s)",
			   source_p->name, comment);
	}

	if(source_p->servptr && source_p->servptr->serv)
		source_p->servptr->serv->servers.erase(source_p->lnode);
	else
		s_assert(0);


	close_connection(source_p);

	if(ConfigServerHide.flatten_links)
		strcpy(comment1, "*.net *.split");
	else
	{
		strcpy(comment1, source_p->servptr->name);
		strcat(comment1, " ");
		strcat(comment1, source_p->name);
	}

	if(source_p->serv != NULL)
		remove_dependents(client_p, source_p, from, is_person(*from) ? newcomment : comment, comment1);

	sendto_realops_snomask(sno::GENERAL, L_ALL, "%s was connected"
			     " for %ld seconds.  %d/%d sendK/recvK.",
			     source_p->name, (long) rb_current_time() - source_p->localClient->firsttime, sendk, recvk);

	ilog(L_SERVER, "%s was connected for %ld seconds.  %d/%d sendK/recvK.",
	     source_p->name, (long) rb_current_time() - source_p->localClient->firsttime, sendk, recvk);

	if(has_id(source_p))
		del_from_id_hash(source_p->id, source_p);

	del_from_client_hash(source_p->name, source_p);
	remove_client_from_list(source_p);
	split(nameinfo(*source_p->serv));

	set_dead(*source_p);
	rb_dlinkAddAlloc(source_p, &dead_list);
	return 0;
}


/*
 * This assumes is_person(*source_p) == true && my_connect(*source_p) == true
 */

int
client::exit_local_client(client *client_p, client *source_p, client *from,
		  const char *comment)
{
	unsigned long on_for;
	char tbuf[26];

	exit_generic_client(client_p, source_p, from, comment);
	clear_monitor(source_p);

	s_assert(is_person(*source_p));
	rb_dlinkDelete(&source_p->localClient->tnode, &lclient_list);
	me.serv->users.erase(source_p->lnode);

	if(is(*source_p, umode::OPER))
		rb_dlinkFindDestroy(source_p, &local_oper_list);

	sendto_realops_snomask(sno::CCONN, L_ALL,
			     "Client exiting: %s (%s@%s) [%s] [%s]",
			     source_p->name,
			     source_p->username, source_p->host, comment,
                             show_ip(NULL, source_p) ? source_p->sockhost : "255.255.255.255");

	sendto_realops_snomask(sno::CCONNEXT, L_ALL,
			"CLIEXIT %s %s %s %s 0 %s",
			source_p->name, source_p->username, source_p->host,
                        show_ip(NULL, source_p) ? source_p->sockhost : "255.255.255.255",
			comment);

	on_for = rb_current_time() - source_p->localClient->firsttime;

	ilog(L_USER, "%s (%3lu:%02lu:%02lu): %s!%s@%s %s %d/%d",
		rb_ctime(rb_current_time(), tbuf, sizeof(tbuf)), on_for / 3600,
		(on_for % 3600) / 60, on_for % 60,
		source_p->name, source_p->username, source_p->host,
		source_p->sockhost,
		source_p->localClient->sendK, source_p->localClient->receiveK);

	sendto_one(source_p, "ERROR :Closing Link: %s (%s)", source_p->host, comment);
	close_connection(source_p);

	if((source_p->flags & flags::KILLED) == 0)
	{
		sendto_server(client_p, NULL, CAP_TS6, NOCAPS,
			      ":%s QUIT :%s", use_id(source_p), comment);
	}

	set_dead(*source_p);
	rb_dlinkAddAlloc(source_p, &dead_list);
	return(CLIENT_EXITED);
}


/*
** exit_client - This is old "m_bye". Name  changed, because this is not a
**        protocol function, but a general server utility function.
**
**        This function exits a client of *any* type (user, server, etc)
**        from this server. Also, this generates all necessary prototol
**        messages that this exit may cause.
**
**   1) If the client is a local client, then this implicitly
**        exits all other clients depending on this connection (e.g.
**        remote clients having 'from'-field that points to this.
**
**   2) If the client is a remote client, then only this is exited.
**
** For convenience, this function returns a suitable value for
** m_function return value:
**
**        CLIENT_EXITED        if (client_p == source_p)
**        0                if (client_p != source_p)
 */
int
client::exit_client(client *client_p,	/* The local client originating the
					 * exit or NULL, if this exit is
					 * generated by this server for
					 * internal reasons.
					 * This will not get any of the
					 * generated messages. */
	    client *source_p,	/* Client exiting */
	    client *from,	/* Client firing off this Exit,
					 * never NULL! */
	    const char *comment	/* Reason for the exit */
	)
{
	hook_data_client_exit hdata;
	if(is_closing(*source_p))
		return -1;

	/* note, this HAS to be here, when we exit a client we attempt to
	 * send them data, if this generates a write error we must *not* add
	 * them to the abort list --fl
	 */
	set_closing(*source_p);

	hdata.local_link = client_p;
	hdata.target = source_p;
	hdata.from = from;
	hdata.comment = comment;
	call_hook(h_client_exit, &hdata);

	if(my_connect(*source_p))
	{
		/* Local clients of various types */
		if(is_person(*source_p))
			return exit_local_client(client_p, source_p, from, comment);
		else if(is_server(*source_p))
			return exit_local_server(client_p, source_p, from, comment);
		/* IsUnknown || IsConnecting || IsHandShake */
		else if(!is_reject(*source_p))
			return exit_unknown_client(client_p, source_p, from, comment);
	}
	else
	{
		/* Remotes */
		if(is_person(*source_p))
			return exit_remote_client(client_p, source_p, from, comment);
		else if(is_server(*source_p))
			return exit_remote_server(client_p, source_p, from, comment);
	}

	return -1;
}

/*
 * Count up local client memory
 */

/* XXX one common Client list now */
void
client::count_local_client_memory(size_t * count, size_t * local_client_memory_used)
{
	size_t lusage;
	rb_bh_usage(lclient_heap, count, NULL, &lusage, NULL);
	*local_client_memory_used = lusage + (*count * (sizeof(void *) + sizeof(client)));
}

/*
 * Count up remote client memory
 */
void
client::count_remote_client_memory(size_t * count, size_t * remote_client_memory_used)
{
	size_t lcount(0), rcount(0);
	rb_bh_usage(lclient_heap, &lcount, NULL, NULL, NULL);
	*count = rcount - lcount;
	*remote_client_memory_used = *count * (sizeof(void *) + sizeof(client));
}


/*
 * accept processing, this adds a form of "caller ID" to ircd
 *
 * If a client puts themselves into "caller ID only" mode,
 * only clients that match a client pointer they have put on
 * the accept list will be allowed to message them.
 *
 * [ source.on_allow_list ] -> [ target1 ] -> [ target2 ]
 *
 * [target.allow_list] -> [ source1 ] -> [source2 ]
 *
 * i.e. a target will have a link list of source pointers it will allow
 * each source client then has a back pointer pointing back
 * to the client that has it on its accept list.
 * This allows for exit_one_client to remove these now bogus entries
 * from any client having an accept on them.
 */
/*
 * del_all_accepts
 *
 * inputs	- pointer to exiting client
 * output	- NONE
 * side effects - Walk through given clients allow_list and on_allow_list
 *                remove all references to this client
 */
void
client::del_all_accepts(client *client_p)
{
	rb_dlink_node *ptr;
	rb_dlink_node *next_ptr;
	client *target_p;

	if(my(*client_p) && client_p->localClient->allow_list.head)
	{
		/* clear this clients accept list, and remove them from
		 * everyones on_accept_list
		 */
		RB_DLINK_FOREACH_SAFE(ptr, next_ptr, client_p->localClient->allow_list.head)
		{
			target_p = reinterpret_cast<client *>(ptr->data);
			rb_dlinkFindDestroy(client_p, &target_p->on_allow_list);
			rb_dlinkDestroy(ptr, &client_p->localClient->allow_list);
		}
	}

	/* remove this client from everyones accept list */
	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, client_p->on_allow_list.head)
	{
		target_p = reinterpret_cast<client *>(ptr->data);
		rb_dlinkFindDestroy(client_p, &target_p->localClient->allow_list);
		rb_dlinkDestroy(ptr, &client_p->on_allow_list);
	}
}

/*
 * show_ip() - asks if the true IP should be shown when source is
 *             asking for info about target
 *
 * Inputs	- source_p who is asking
 *		- target_p who do we want the info on
 * Output	- returns 1 if clear IP can be shown, otherwise 0
 * Side Effects	- none
 */

int
client::show_ip(client *source_p, client *target_p)
{
	if(is_any_server(*target_p))
	{
		return 0;
	}
	else if(is_ip_spoof(*target_p))
	{
		/* source == NULL indicates message is being sent
		 * to local opers.
		 */
		if(!ConfigFileEntry.hide_spoof_ips &&
		   (source_p == NULL || my_oper(*source_p)))
			return 1;
		return 0;
	}
	else if(is_dyn_spoof(*target_p) && (source_p != NULL && !is(*source_p, umode::OPER)))
		return 0;
	else
		return 1;
}

int
client::show_ip_conf(struct ConfItem *aconf, client *source_p)
{
	if(IsConfDoSpoofIp(aconf))
	{
		if(!ConfigFileEntry.hide_spoof_ips && my_oper(*source_p))
			return 1;

		return 0;
	}
	else
		return 1;
}

int
client::show_ip_whowas(const whowas::whowas &whowas, client &source)
{
	if(whowas.flag & whowas.IP_SPOOFING)
		if(ConfigFileEntry.hide_spoof_ips || !my_oper(source))
			return 0;

	if(whowas.flag & whowas.DYNSPOOF)
		if(!is(source, umode::OPER))
			return 0;

	return 1;
}

void
client::init_uid(void)
{
	int i;

	for(i = 0; i < 3; i++)
		current_uid[i] = me.id[i];

	for(i = 3; i < 9; i++)
		current_uid[i] = 'A';

	current_uid[9] = '\0';
}


char *
client::generate_uid(void)
{
	static int flipped = 0;
	int i;

uid_restart:
	for(i = 8; i > 3; i--)
	{
		if(current_uid[i] == 'Z')
		{
			current_uid[i] = '0';
			goto out;
		}
		else if(current_uid[i] != '9')
		{
			current_uid[i]++;
			goto out;
		}
		else
			current_uid[i] = 'A';
	}

	/* if this next if() triggers, we're fucked. */
	if(current_uid[3] == 'Z')
	{
		current_uid[i] = 'A';
		flipped = 1;
	}
	else
		current_uid[i]++;
out:
	/* if this happens..well, i'm not sure what to say, but lets handle it correctly */
	if(rb_unlikely(flipped))
	{
		/* this slows down uid generation a bit... */
		if(find_id(current_uid) != NULL)
			goto uid_restart;
	}
	return current_uid;
}

/*
 * close_connection
 *        Close the physical connection. This function must make
 *        my_connect(*client_p) == FALSE, and set client_p->from == NULL.
 */
void
client::close_connection(client *client_p)
{
	s_assert(client_p != NULL);
	if(client_p == NULL)
		return;

	s_assert(my_connect(*client_p));
	if(!my_connect(*client_p))
		return;

	if(is_server(*client_p))
	{
		struct server_conf *server_p;

		ServerStats.is_sv++;
		ServerStats.is_sbs += client_p->localClient->sendB;
		ServerStats.is_sbr += client_p->localClient->receiveB;
		ServerStats.is_sti += (unsigned long long)(rb_current_time() - client_p->localClient->firsttime);

		/*
		 * If the connection has been up for a long amount of time, schedule
		 * a 'quick' reconnect, else reset the next-connect cycle.
		 */
		if((server_p = find_server_conf(client_p->name)) != NULL)
		{
			/*
			 * Reschedule a faster reconnect, if this was a automatically
			 * connected configuration entry. (Note that if we have had
			 * a rehash in between, the status has been changed to
			 * CONF_ILLEGAL). But only do this if it was a "good" link.
			 */
			server_p->hold = time(NULL);
			server_p->hold +=
				(server_p->hold - client_p->localClient->lasttime >
				 HANGONGOODLINK) ? HANGONRETRYDELAY : ConFreq(server_p->_class);
		}

	}
	else if(is_client(*client_p))
	{
		ServerStats.is_cl++;
		ServerStats.is_cbs += client_p->localClient->sendB;
		ServerStats.is_cbr += client_p->localClient->receiveB;
		ServerStats.is_cti += (unsigned long long)(rb_current_time() - client_p->localClient->firsttime);
	}
	else
		ServerStats.is_ni++;

	client_release_connids(client_p);

	if(client_p->localClient->F != NULL)
	{
		/* attempt to flush any pending dbufs. Evil, but .. -- adrian */
		if(!is_io_error(*client_p))
			send_queued(client_p);

		rb_close(client_p->localClient->F);
		client_p->localClient->F = NULL;
	}

	rb_linebuf_donebuf(&client_p->localClient->buf_sendq);
	rb_linebuf_donebuf(&client_p->localClient->buf_recvq);
	detach_conf(client_p);

	/* XXX shouldnt really be done here. */
	detach_server_conf(client_p);

	client_p->from = NULL;	/* ...this should catch them! >:) --msa */
	clear_my_connect(*client_p);
	set_io_error(*client_p);
}



void
client::error_exit_client(client *client_p, int error)
{
	/*
	 * ...hmm, with non-blocking sockets we might get
	 * here from quite valid reasons, although.. why
	 * would select report "data available" when there
	 * wasn't... so, this must be an error anyway...  --msa
	 * actually, EOF occurs when read() returns 0 and
	 * in due course, select() returns that fd as ready
	 * for reading even though it ends up being an EOF. -avalon
	 */
	char errmsg[255];
	int current_error = rb_get_sockerr(client_p->localClient->F);

	set_io_error(*client_p);

	if(is_server(*client_p) || is_handshake(*client_p))
	{
		if(error == 0)
		{
			sendto_realops_snomask(sno::GENERAL, is_remote_connect(client_p) && !is_server(*client_p) ? L_NETWIDE : L_ALL,
					     "Server %s closed the connection",
					     client_p->name);

			ilog(L_SERVER, "Server %s closed the connection",
			     log_client_name(client_p, SHOW_IP));
		}
		else
		{
			sendto_realops_snomask(sno::GENERAL, is_remote_connect(client_p) && !is_server(*client_p) ? L_NETWIDE : L_ALL,
					"Lost connection to %s: %s",
					client_p->name, strerror(current_error));
			ilog(L_SERVER, "Lost connection to %s: %s",
				log_client_name(client_p, SHOW_IP), strerror(current_error));
		}
	}

	if(error == 0)
		rb_strlcpy(errmsg, "Remote host closed the connection", sizeof(errmsg));
	else
		snprintf(errmsg, sizeof(errmsg), "Read error: %s", strerror(current_error));

	exit_client(client_p, client_p, &me, errmsg);
}
