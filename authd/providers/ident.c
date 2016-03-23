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
	time_t timeout;				/* Timeout interval */
	rb_fde_t *F;				/* Our FD */
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

static void client_fail(struct auth_client *auth, ident_message message);
static void client_success(struct auth_client *auth);
static char * get_valid_ident(char *buf);

static struct ev_entry *timeout_ev;
static int ident_timeout = 5;


bool ident_init(void)
{
	timeout_ev = rb_event_addish("timeout_ident_queries_event", timeout_ident_queries_event, NULL, 1);
	return (timeout_ev != NULL);
}

void ident_destroy(void)
{
	struct auth_client *auth;
	rb_dictionary_iter iter;

	/* Nuke all ident queries */
	DICTIONARY_FOREACH(auth, &iter, auth_clients)
	{
		if(auth->data[PROVIDER_IDENT] != NULL)
			client_fail(auth, REPORT_FAIL);
	}
}

bool ident_start(struct auth_client *auth)
{
	struct ident_query *query = rb_malloc(sizeof(struct ident_query));
	struct rb_sockaddr_storage l_addr, c_addr;
	int family;
	rb_fde_t *F;

	auth->data[PROVIDER_IDENT] = query;
	query->timeout = rb_current_time() + ident_timeout;

	if((F = rb_socket(family, SOCK_STREAM, 0, "ident")) == NULL)
	{
		client_fail(auth, REPORT_FAIL);
		return true;	/* Not a fatal error */
	}

	query->F = F;

	/* Build sockaddr_storages for rb_connect_tcp below */
	if(!rb_inet_ntop_sock((struct sockaddr *)&l_addr, auth->l_ip, sizeof(l_addr)) ||
		!rb_inet_ntop_sock((struct sockaddr *)&c_addr, auth->c_ip, sizeof(c_addr)))
	{
		client_fail(auth, REPORT_FAIL);
		return true;
	}

	/* Set the ports correctly */
#ifdef RB_IPV6
	if(GET_SS_FAMILY(&l_addr) == AF_INET6)
		((struct sockaddr_in6 *)&l_addr)->sin6_port = 0;
	else
#endif
		((struct sockaddr_in *)&l_addr)->sin_port = 0;

#ifdef RB_IPV6
	if(GET_SS_FAMILY(&c_addr) == AF_INET6)
		((struct sockaddr_in6 *)&c_addr)->sin6_port = htons(113);
	else
#endif
		((struct sockaddr_in *)&c_addr)->sin_port = htons(113);

	rb_connect_tcp(F, (struct sockaddr *)&c_addr,
			(struct sockaddr *)&l_addr,
			GET_SS_LEN(&l_addr), ident_connected,
			query, ident_timeout);

	notice_client(auth, messages[REPORT_LOOKUP]);
	set_provider(auth, PROVIDER_IDENT);

	return true;
}

void ident_cancel(struct auth_client *auth)
{
	struct ident_query *query = auth->data[PROVIDER_IDENT];

	if(query != NULL)
		client_fail(auth, REPORT_FAIL);
}

/* Timeout outstanding queries */
static void timeout_ident_queries_event(void *notused)
{
	struct auth_client *auth;
	rb_dictionary_iter iter;

	DICTIONARY_FOREACH(auth, &iter, auth_clients)
	{
		struct ident_query *query = auth->data[PROVIDER_IDENT];

		if(query != NULL && query->timeout < rb_current_time())
			client_fail(auth, REPORT_FAIL);
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
	struct auth_client *auth = data;
	struct ident_query *query = auth->data[PROVIDER_IDENT];
	char authbuf[32];
	int authlen;

	/* Check the error */
	if(error != RB_OK)
	{
		/* We had an error during connection :( */
		client_fail(auth, REPORT_FAIL);
		return;
	}

	snprintf(authbuf, sizeof(authbuf), "%u , %u\r\n",
			auth->c_port, auth->l_port);
	authlen = strlen(authbuf);

	if(rb_write(query->F, authbuf, authlen) != authlen)
	{
		client_fail(auth, REPORT_FAIL);
		return;
	}

	read_ident_reply(query->F, auth);
}

static void
read_ident_reply(rb_fde_t *F, void *data)
{
	struct auth_client *auth = data;
	struct ident_query *query = auth->data[PROVIDER_IDENT];
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
		client_fail(auth, REPORT_FAIL);
	else
		client_success(auth);
}

static void client_fail(struct auth_client *auth, ident_message report)
{
	struct ident_query *query = auth->data[PROVIDER_IDENT];

	rb_strlcpy(auth->username, "*", sizeof(auth->username));

	rb_close(query->F);
	rb_free(query);
	auth->data[PROVIDER_IDENT] = NULL;

	notice_client(auth, messages[report]);
	provider_done(auth, PROVIDER_IDENT);
}

static void client_success(struct auth_client *auth)
{
	struct ident_query *query = auth->data[PROVIDER_IDENT];

	rb_close(query->F);
	rb_free(query);
	auth->data[PROVIDER_IDENT] = NULL;

	notice_client(auth, messages[REPORT_FOUND]);
	provider_done(auth, PROVIDER_IDENT);
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
