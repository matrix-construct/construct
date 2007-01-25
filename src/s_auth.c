/*
 *  ircd-ratbox: A slightly useful ircd.
 *  s_auth.c: Functions for querying a users ident.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2005 ircd-ratbox development team
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
 *
 *  $Id: s_auth.c 1683 2006-06-20 14:26:16Z jilles $ */

/*
 * Changes:
 *   July 6, 1999 - Rewrote most of the code here. When a client connects
 *     to the server and passes initial socket validation checks, it
 *     is owned by this module (auth) which returns it to the rest of the
 *     server when dns and auth queries are finished. Until the client is
 *     released, the server does not know it exists and does not process
 *     any messages from it.
 *     --Bleep  Thomas Helvey <tomh@inxpress.net>
 */
#include "stdinc.h"
#include "config.h"
#include "tools.h"
#include "s_auth.h"
#include "s_conf.h"
#include "client.h"
#include "common.h"
#include "event.h"
#include "irc_string.h"
#include "sprintf_irc.h"
#include "ircd.h"
#include "numeric.h"
#include "packet.h"
#include "res.h"
#include "commio.h"
#include "s_log.h"
#include "s_stats.h"
#include "send.h"
#include "memory.h"
#include "hook.h"
#include "blacklist.h"

/*
 * a bit different approach
 * this replaces the original sendheader macros
 */

static const char *HeaderMessages[] =
{
	"NOTICE AUTH :*** Looking up your hostname...",
	"NOTICE AUTH :*** Found your hostname",
	"NOTICE AUTH :*** Couldn't look up your hostname",
	"NOTICE AUTH :*** Checking Ident",
	"NOTICE AUTH :*** Got Ident response",
	"NOTICE AUTH :*** No Ident response",
	"NOTICE AUTH :*** Your hostname is too long, ignoring hostname",
	"NOTICE AUTH :*** Your forward and reverse DNS do not match, ignoring hostname",
	"NOTICE AUTH :*** Cannot verify hostname validity, ignoring hostname",
};

typedef enum
{
	REPORT_DO_DNS,
	REPORT_FIN_DNS,
	REPORT_FAIL_DNS,
	REPORT_DO_ID,
	REPORT_FIN_ID,
	REPORT_FAIL_ID,
	REPORT_HOST_TOOLONG,
	REPORT_HOST_MISMATCH,
	REPORT_HOST_UNKNOWN
}
ReportType;

#define sendheader(c, r) sendto_one(c, HeaderMessages[(r)]) 

static dlink_list auth_poll_list;
static BlockHeap *auth_heap;
static EVH timeout_auth_queries_event;

static PF read_auth_reply;
static CNCB auth_connect_callback;

/*
 * init_auth()
 *
 * Initialise the auth code
 */
void
init_auth(void)
{
	/* This hook takes a struct Client for its argument */
	memset(&auth_poll_list, 0, sizeof(auth_poll_list));
	eventAddIsh("timeout_auth_queries_event", timeout_auth_queries_event, NULL, 1);
	auth_heap = BlockHeapCreate(sizeof(struct AuthRequest), LCLIENT_HEAP_SIZE);
}

/*
 * make_auth_request - allocate a new auth request
 */
static struct AuthRequest *
make_auth_request(struct Client *client)
{
	struct AuthRequest *request = BlockHeapAlloc(auth_heap);
	client->localClient->auth_request = request;
	request->fd = -1;
	request->client = client;
	request->timeout = CurrentTime + ConfigFileEntry.connect_timeout;
	return request;
}

/*
 * free_auth_request - cleanup auth request allocations
 */
static void
free_auth_request(struct AuthRequest *request)
{
	BlockHeapFree(auth_heap, request);
}

/*
 * release_auth_client - release auth client from auth system
 * this adds the client into the local client lists so it can be read by
 * the main io processing loop
 */
static void
release_auth_client(struct AuthRequest *auth)
{
	struct Client *client = auth->client;
	
	if(IsDNSPending(auth) || IsDoingAuth(auth))
		return;

	client->localClient->auth_request = NULL;
	dlinkDelete(&auth->node, &auth_poll_list);
	free_auth_request(auth);	
	if(client->localClient->fd > highest_fd)
		highest_fd = client->localClient->fd;

	/*
	 * When a client has auth'ed, we want to start reading what it sends
	 * us. This is what read_packet() does.
	 *     -- adrian
	 */
	client->localClient->allow_read = MAX_FLOOD;
	comm_setflush(client->localClient->fd, 1000, flood_recalc, client);
	dlinkAddTail(client, &client->node, &global_client_list);
	read_packet(client->localClient->fd, client);
}

/*
 * auth_dns_callback - called when resolver query finishes
 * if the query resulted in a successful search, hp will contain
 * a non-null pointer, otherwise hp will be null.
 * set the client on it's way to a connection completion, regardless
 * of success of failure
 */
static void
auth_dns_callback(void *vptr, struct DNSReply *reply)
{
        struct AuthRequest *auth = (struct AuthRequest *) vptr;
        ClearDNSPending(auth);

	/* XXX: this shouldn't happen, but it does. -nenolod */
	if(auth->client->localClient == NULL)
	{
		sendto_realops_snomask(SNO_GENERAL, L_ALL,
			"auth_dns_callback(): auth->client->localClient (%s) is NULL", get_client_name(auth->client, HIDE_IP));

		dlinkDelete(&auth->node, &auth_poll_list);
		free_auth_request(auth);

		/* and they will silently drop through and all will hopefully be ok... -nenolod */
		return;
	}

        if(reply)
        {
		int good = 1;

		if(auth->client->localClient->ip.ss_family == AF_INET)
		{
			struct sockaddr_in *ip, *ip_fwd;

			ip = (struct sockaddr_in *) &auth->client->localClient->ip;
			ip_fwd = (struct sockaddr_in *) &reply->addr;

			if(ip->sin_addr.s_addr != ip_fwd->sin_addr.s_addr)
			{
				sendheader(auth->client, REPORT_HOST_MISMATCH);
				good = 0;
			}
		}
#ifdef IPV6
		else if(auth->client->localClient->ip.ss_family == AF_INET6)
		{
			struct sockaddr_in6 *ip, *ip_fwd;

			ip = (struct sockaddr_in6 *) &auth->client->localClient->ip;
			ip_fwd = (struct sockaddr_in6 *) &reply->addr;

			if(memcmp(&ip->sin6_addr, &ip_fwd->sin6_addr, sizeof(struct in6_addr)) != 0)
			{
				sendheader(auth->client, REPORT_HOST_MISMATCH);
				good = 0;
			}
		}
#endif
		else	/* can't verify it, don't know how. reject it. */
		{
			sendheader(auth->client, REPORT_HOST_UNKNOWN);
			good = 0;
		}

                if(good && strlen(reply->h_name) <= HOSTLEN)
                {
                        strlcpy(auth->client->host, reply->h_name, sizeof(auth->client->host));
                        sendheader(auth->client, REPORT_FIN_DNS);
                }
                else if (strlen(reply->h_name) > HOSTLEN)
                        sendheader(auth->client, REPORT_HOST_TOOLONG);
        }
	else
                sendheader(auth->client, REPORT_FAIL_DNS);

        release_auth_client(auth);
}

/*
 * authsenderr - handle auth send errors
 */
static void
auth_error(struct AuthRequest *auth)
{
	++ServerStats->is_abad;

	comm_close(auth->fd);
	auth->fd = -1;

	ClearAuth(auth);
	sendheader(auth->client, REPORT_FAIL_ID);
		
	release_auth_client(auth);
}

/*
 * start_auth_query - Flag the client to show that an attempt to 
 * contact the ident server on
 * the client's host.  The connect and subsequently the socket are all put
 * into 'non-blocking' mode.  Should the connect or any later phase of the
 * identifing process fail, it is aborted and the user is given a username
 * of "unknown".
 */
static int
start_auth_query(struct AuthRequest *auth)
{
	struct irc_sockaddr_storage localaddr;
	socklen_t locallen = sizeof(struct irc_sockaddr_storage);
	int fd;
	int family;
	
	if(IsAnyDead(auth->client))
		return 0;
	
	family = auth->client->localClient->ip.ss_family;
	if((fd = comm_socket(family, SOCK_STREAM, 0, "ident")) == -1)
	{
		report_error("creating auth stream socket %s:%s",
			     get_client_name(auth->client, SHOW_IP), 
			     log_client_name(auth->client, SHOW_IP), errno);
		++ServerStats->is_abad;
		return 0;
	}
	if((MAXCONNECTIONS - 10) < fd)
	{
		sendto_realops_snomask(SNO_GENERAL, L_ALL,
				     "Can't allocate fd for auth on %s",
				     get_client_name(auth->client, SHOW_IP));
		comm_close(fd);
		return 0;
	}

	sendheader(auth->client, REPORT_DO_ID);

	/* 
	 * get the local address of the client and bind to that to
	 * make the auth request.  This used to be done only for
	 * ifdef VIRTUAL_HOST, but needs to be done for all clients
	 * since the ident request must originate from that same address--
	 * and machines with multiple IP addresses are common now
	 */
	memset(&localaddr, 0, locallen);
	getsockname(auth->client->localClient->fd,
		    (struct sockaddr *) &localaddr, &locallen);
	
	mangle_mapped_sockaddr((struct sockaddr *)&localaddr);
#ifdef IPV6
	if(localaddr.ss_family == AF_INET6)
	{
		((struct sockaddr_in6 *)&localaddr)->sin6_port = 0;
	} else
#endif
	((struct sockaddr_in *)&localaddr)->sin_port = 0;
	
	auth->fd = fd;
	SetAuthConnect(auth);

	comm_connect_tcp(fd, auth->client->sockhost, 113,
			 (struct sockaddr *) &localaddr, GET_SS_LEN(localaddr),
			 auth_connect_callback, auth, 
			 localaddr.ss_family, GlobalSetOptions.ident_timeout);
	return 1;		/* We suceed here for now */
}

/*
 * GetValidIdent - parse ident query reply from identd server
 * 
 * Inputs        - pointer to ident buf
 * Output        - NULL if no valid ident found, otherwise pointer to name
 * Side effects  -
 */
static char *
GetValidIdent(char *buf)
{
	int remp = 0;
	int locp = 0;
	char *colon1Ptr;
	char *colon2Ptr;
	char *colon3Ptr;
	char *commaPtr;
	char *remotePortString;

	/* All this to get rid of a sscanf() fun. */
	remotePortString = buf;

	colon1Ptr = strchr(remotePortString, ':');
	if(!colon1Ptr)
		return 0;

	*colon1Ptr = '\0';
	colon1Ptr++;
	colon2Ptr = strchr(colon1Ptr, ':');
	if(!colon2Ptr)
		return 0;

	*colon2Ptr = '\0';
	colon2Ptr++;
	commaPtr = strchr(remotePortString, ',');

	if(!commaPtr)
		return 0;

	*commaPtr = '\0';
	commaPtr++;

	remp = atoi(remotePortString);
	if(!remp)
		return 0;

	locp = atoi(commaPtr);
	if(!locp)
		return 0;

	/* look for USERID bordered by first pair of colons */
	if(!strstr(colon1Ptr, "USERID"))
		return 0;

	colon3Ptr = strchr(colon2Ptr, ':');
	if(!colon3Ptr)
		return 0;

	*colon3Ptr = '\0';
	colon3Ptr++;
	return (colon3Ptr);
}

/*
 * start_auth - starts auth (identd) and dns queries for a client
 */
void
start_auth(struct Client *client)
{
	struct AuthRequest *auth = 0;
	s_assert(0 != client);
	if(client == NULL)
		return;

	/* to aid bopm which needs something unique to match against */
	sendto_one(client, "NOTICE AUTH :*** Processing connection to %s",
			me.name);

	auth = make_auth_request(client);

	auth->dns_query.ptr = auth;
	auth->dns_query.callback = auth_dns_callback;

	sendheader(client, REPORT_DO_DNS);

	/* No DNS cache now, remember? -- adrian */
	gethost_byaddr(&client->localClient->ip, &auth->dns_query);

	SetDNSPending(auth);

	if(ConfigFileEntry.disable_auth == 0)
		start_auth_query(auth);

	dlinkAdd(auth, &auth->node, &auth_poll_list);
}

/*
 * timeout_auth_queries - timeout resolver and identd requests
 * allow clients through if requests failed
 */
static void
timeout_auth_queries_event(void *notused)
{
	dlink_node *ptr;
	dlink_node *next_ptr;
	struct AuthRequest *auth;

	DLINK_FOREACH_SAFE(ptr, next_ptr, auth_poll_list.head)
	{
		auth = ptr->data;

		if(auth->timeout < CurrentTime)
		{
			if(auth->fd >= 0)
				comm_close(auth->fd);

			if(IsDoingAuth(auth))
			{
				ClearAuth(auth);
				++ServerStats->is_abad;
				sendheader(auth->client, REPORT_FAIL_ID);
				auth->client->localClient->auth_request = NULL;
			}
			if(IsDNSPending(auth))
			{
				ClearDNSPending(auth);
				delete_resolver_queries(&auth->dns_query);
				sendheader(auth->client, REPORT_FAIL_DNS);
			}

			auth->client->localClient->lasttime = CurrentTime;
			release_auth_client(auth);
		}
	}
}

/*
 * auth_connect_callback() - deal with the result of comm_connect_tcp()
 *
 * If the connection failed, we simply close the auth fd and report
 * a failure. If the connection suceeded send the ident server a query
 * giving "theirport , ourport". The write is only attempted *once* so
 * it is deemed to be a fail if the entire write doesn't write all the
 * data given.  This shouldnt be a problem since the socket should have
 * a write buffer far greater than this message to store it in should
 * problems arise. -avalon
 */
static void
auth_connect_callback(int fd, int error, void *data)
{
	struct AuthRequest *auth = data;
	struct sockaddr_in us;
	struct sockaddr_in them;
	char authbuf[32];
	socklen_t ulen = sizeof(struct sockaddr_in);
	socklen_t tlen = sizeof(struct sockaddr_in);

	/* Check the error */
	if(error != COMM_OK)
	{
		/* We had an error during connection :( */
		auth_error(auth);
		return;
	}

	if(getsockname
	   (auth->client->localClient->fd, (struct sockaddr *) &us,
	    (socklen_t *) & ulen)
	   || getpeername(auth->client->localClient->fd,
			  (struct sockaddr *) &them, (socklen_t *) & tlen))
	{
		ilog(L_IOERROR, "auth get{sock,peer}name error for %s:%m",
		     log_client_name(auth->client, SHOW_IP));
		auth_error(auth);
		return;
	}
	ircsnprintf(authbuf, sizeof(authbuf), "%u , %u\r\n",
		   (unsigned int) ntohs(them.sin_port), (unsigned int) ntohs(us.sin_port));

	if(write(auth->fd, authbuf, strlen(authbuf)) == -1)
	{
		auth_error(auth);
		return;
	}
	ClearAuthConnect(auth);
	SetAuthPending(auth);
	read_auth_reply(auth->fd, auth);
}


/*
 * read_auth_reply - read the reply (if any) from the ident server 
 * we connected to.
 * We only give it one shot, if the reply isn't good the first time
 * fail the authentication entirely. --Bleep
 */
#define AUTH_BUFSIZ 128

static void
read_auth_reply(int fd, void *data)
{
	struct AuthRequest *auth = data;
	char *s = NULL;
	char *t = NULL;
	int len;
	int count;
	char buf[AUTH_BUFSIZ + 1];	/* buffer to read auth reply into */

	len = read(auth->fd, buf, AUTH_BUFSIZ);

	if(len < 0 && ignoreErrno(errno))
	{
		comm_setselect(fd, FDLIST_IDLECLIENT, COMM_SELECT_READ, read_auth_reply, auth, 0);
		return;
	}

	if(len > 0)
	{
		buf[len] = '\0';

		if((s = GetValidIdent(buf)))
		{
			t = auth->client->username;

			while (*s == '~' || *s == '^')
				s++;

			for (count = USERLEN; *s && count; s++)
			{
				if(*s == '@')
				{
					break;
				}
				if(!IsSpace(*s) && *s != ':' && *s != '[')
				{
					*t++ = *s;
					count--;
				}
			}
			*t = '\0';
		}
	}

	comm_close(auth->fd);
	auth->fd = -1;
	ClearAuth(auth);

	if(s == NULL)
	{
		++ServerStats->is_abad;
		strcpy(auth->client->username, "unknown");
		sendheader(auth->client, REPORT_FAIL_ID);
	}
	else
	{
		sendheader(auth->client, REPORT_FIN_ID);
		++ServerStats->is_asuc;
		SetGotId(auth->client);
	}

	release_auth_client(auth);
}



/*
 * delete_auth_queries()
 *
 */
void
delete_auth_queries(struct Client *target_p)
{
	struct AuthRequest *auth;

	if(target_p == NULL || target_p->localClient == NULL ||
	   target_p->localClient->auth_request == NULL)
		return;
	
	auth = target_p->localClient->auth_request;
	target_p->localClient->auth_request = NULL;

	if(IsDNSPending(auth))
		delete_resolver_queries(&auth->dns_query);

	if(auth->fd >= 0)
		comm_close(auth->fd);
		
	dlinkDelete(&auth->node, &auth_poll_list);
	free_auth_request(auth);
}
