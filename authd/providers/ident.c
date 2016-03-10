/* authd/providers/ident.c - ident lookup provider for authd
 * Copyright (c) 2016 Elizabeth Myers <elizabeth@interlinked.me>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
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
 */

#include "stdinc.h"
#include "match.h"
#include "authd.h"
#include "provider.h"
#include "res.h"

#define IDENT_BUFSIZE 128

struct ident_query
{
	rb_dlink_node node;

	struct auth_client *auth;	/* Our client */
	time_t timeout;			/* Timeout interval */
	rb_fde_t *F;			/* Our FD */
};

/* Goinked from old s_auth.c --Elizafox */
static const char *messages[] =
{
	":*** Checking Ident",
	":*** Got Ident response",
	":*** No Ident response",
};

typedef enum
{
	REPORT_LOOKUP,
	REPORT_FOUND,
	REPORT_FAIL,
} ident_message;

static EVH timeout_ident_queries_event;
static CNCB ident_connected;
static PF read_ident_reply;

static void client_fail(struct ident_query *query, ident_message message);
static void client_success(struct ident_query *query);
static void cleanup_query(struct ident_query *query);
static char * get_valid_ident(char *buf);

static rb_dlink_list queries;
static struct ev_entry *timeout_ev;
static int ident_timeout = 5;


bool ident_init(void)
{
	timeout_ev = rb_event_addish("timeout_ident_queries_event", timeout_ident_queries_event, NULL, 1);
	return (timeout_ev != NULL);
}

void ident_destroy(void)
{
	rb_dlink_node *ptr, *nptr;

	/* Nuke all ident queries */
	RB_DLINK_FOREACH_SAFE(ptr, nptr, queries.head)
	{
		struct ident_query *query = ptr->data;

		notice_client(query->auth, messages[REPORT_FAIL]);

		rb_close(query->F);
		rb_free(query);
		rb_dlinkDelete(ptr, &queries);
	}
}

bool ident_start(struct auth_client *auth)
{
	struct ident_query *query = rb_malloc(sizeof(struct ident_query));
	struct rb_sockaddr_storage localaddr, clientaddr;
	int family;
	rb_fde_t *F;

	query->auth = auth;
	query->timeout = rb_current_time() + ident_timeout;

	if((F = rb_socket(family, SOCK_STREAM, 0, "ident")) == NULL)
	{
		client_fail(query, REPORT_FAIL);
		return true;	/* Not a fatal error */
	}

	query->F = F;

	/* Build sockaddr_storages for rb_connect_tcp below */
	memcpy(&localaddr, &auth->l_addr, sizeof(struct rb_sockaddr_storage));
	memcpy(&clientaddr, &auth->c_addr, sizeof(struct rb_sockaddr_storage));

	/* Set the ports correctly */
#ifdef RB_IPV6
	if(GET_SS_FAMILY(&localaddr) == AF_INET6)
		((struct sockaddr_in6 *)&localaddr)->sin6_port = 0;
	else
#endif
		((struct sockaddr_in *)&localaddr)->sin_port = 0;

#ifdef RB_IPV6
	if(GET_SS_FAMILY(&clientaddr) == AF_INET6)
		((struct sockaddr_in6 *)&clientaddr)->sin6_port = htons(113);
	else
#endif
		((struct sockaddr_in *)&clientaddr)->sin_port = htons(113);

	rb_connect_tcp(F, (struct sockaddr *)&auth->c_addr,
			(struct sockaddr *)&auth->l_addr,
			GET_SS_LEN(&auth->l_addr), ident_connected,
			query, ident_timeout);

	set_provider(auth, PROVIDER_IDENT);

	rb_dlinkAdd(query, &query->node, &queries);

	notice_client(auth, messages[REPORT_LOOKUP]);

	return true;
}

void ident_cancel(struct auth_client *auth)
{
	rb_dlink_node *ptr, *nptr;

	RB_DLINK_FOREACH_SAFE(ptr, nptr, queries.head)
	{
		struct ident_query *query = ptr->data;

		if(query->auth == auth)
		{
			client_fail(query, REPORT_FAIL);

			rb_close(query->F);
			rb_free(query);
			rb_dlinkDelete(ptr, &queries);

			return;
		}
	}
}

/* Timeout outstanding queries */
static void timeout_ident_queries_event(void *notused)
{
	rb_dlink_node *ptr, *nptr;

	RB_DLINK_FOREACH_SAFE(ptr, nptr, queries.head)
	{
		struct ident_query *query = ptr->data;

		if(query->timeout < rb_current_time())
		{
			client_fail(query, REPORT_FAIL);

			rb_close(query->F);
			rb_free(query);
			rb_dlinkDelete(ptr, &queries);
		}
	}
}

/*
 * ident_connected() - deal with the result of rb_connect_tcp()
 *
 * If the connection failed, we simply close the auth fd and report
 * a failure. If the connection suceeded send the ident server a query
 * giving "theirport , ourport". The write is only attempted *once* so
 * it is deemed to be a fail if the entire write doesn't write all the
 * data given.  This shouldnt be a problem since the socket should have
 * a write buffer far greater than this message to store it in should
 * problems arise. -avalon
 */
static void ident_connected(rb_fde_t *F, int error, void *data)
{
	struct ident_query *query = data;
	struct auth_client *auth = query->auth;
	uint16_t c_port, l_port;
	char authbuf[32];
	int authlen;

	/* Check the error */
	if(error != RB_OK)
	{
		/* We had an error during connection :( */
		client_fail(query, REPORT_FAIL);
		cleanup_query(query);
		return;
	}

#ifdef RB_IPV6
	if(GET_SS_FAMILY(&auth->c_addr) == AF_INET6)
		c_port = ntohs(((struct sockaddr_in6 *)&auth->c_addr)->sin6_port);
	else
#endif
		c_port = ntohs(((struct sockaddr_in *)&auth->c_addr)->sin_port);

#ifdef RB_IPV6
	if(GET_SS_FAMILY(&auth->l_addr) == AF_INET6)
		l_port = ntohs(((struct sockaddr_in6 *)&auth->l_addr)->sin6_port);
	else
#endif
		l_port = ntohs(((struct sockaddr_in *)&auth->l_addr)->sin_port);


	snprintf(authbuf, sizeof(authbuf), "%u , %u\r\n",
			c_port, l_port);
	authlen = strlen(authbuf);

	if(rb_write(query->F, authbuf, authlen) != authlen)
	{
		client_fail(query, REPORT_FAIL);
		return;
	}

	read_ident_reply(query->F, query);
}

static void
read_ident_reply(rb_fde_t *F, void *data)
{
	struct ident_query *query = data;
	struct auth_client *auth = query->auth;
	char *s = NULL;
	char *t = NULL;
	int len;
	int count;
	char buf[IDENT_BUFSIZE + 1];	/* buffer to read auth reply into */

	len = rb_read(F, buf, IDENT_BUFSIZE);
	if(len < 0 && rb_ignore_errno(errno))
	{
		rb_setselect(F, RB_SELECT_READ, read_ident_reply, query);
		return;
	}

	if(len > 0)
	{
		buf[len] = '\0';

		if((s = get_valid_ident(buf)))
		{
			t = auth->username;

			while (*s == '~' || *s == '^')
				s++;

			for (count = USERLEN; *s && count; s++)
			{
				if(*s == '@')
				{
					break;
				}
				if(*s != ' ' && *s != ':' && *s != '[')
				{
					*t++ = *s;
					count--;
				}
			}
			*t = '\0';
		}
	}

	if(s == NULL)
		client_fail(query, REPORT_FAIL);
	else
		client_success(query);

	cleanup_query(query);
}

static void client_fail(struct ident_query *query, ident_message report)
{
	struct auth_client *auth = query->auth;

	if(auth)
	{
		rb_strlcpy(auth->username, "*", sizeof(auth->username));
		notice_client(auth, messages[report]);
		provider_done(auth, PROVIDER_IDENT);
	}
}

static void client_success(struct ident_query *query)
{
	struct auth_client *auth = query->auth;

	if(auth)
	{
		notice_client(auth, messages[REPORT_FOUND]);
		provider_done(auth, PROVIDER_IDENT);
	}
}

static void cleanup_query(struct ident_query *query)
{
	rb_dlink_node *ptr, *nptr;

	RB_DLINK_FOREACH_SAFE(ptr, nptr, queries.head)
	{
		struct ident_query *query_l = ptr->data;

		if(query_l == query)
		{
			rb_close(query->F);
			rb_free(query);
			rb_dlinkDelete(ptr, &queries);
		}
	}
}

/* get_valid_ident
 * parse ident query reply from identd server
 *
 * Torn out of old s_auth.c because there was nothing wrong with it
 * --Elizafox
 *
 * Inputs	- pointer to ident buf
 * Outputs	- NULL if no valid ident found, otherwise pointer to name
 * Side effects - None
 */
static char *
get_valid_ident(char *buf)
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


struct auth_provider ident_provider =
{
	.id = PROVIDER_IDENT,
	.init = ident_init,
	.destroy = ident_destroy,
	.start = ident_start,
	.cancel = ident_cancel,
	.completed = NULL,
};
