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

/* Largely adapted from old s_auth.c, but reworked for authd. rDNS code
 * moved to its own provider.
 *
 * --Elizafox 13 March 2016
 */

#include "stdinc.h"
#include "defaults.h"
#include "match.h"
#include "authd.h"
#include "notice.h"
#include "provider.h"
#include "res.h"

#define SELF_PID (ident_provider.id)

#define IDENT_BUFSIZE 128

struct ident_query
{
	rb_fde_t *F;				/* Our FD */
};

/* Goinked from old s_auth.c --Elizafox */
static const char *messages[] =
{
	"*** Checking Ident",
	"*** Got Ident response",
	"*** No Ident response",
	"*** Cannot verify ident validity, ignoring ident",
	"*** Ident disabled, not checking ident",
};

typedef enum
{
	REPORT_LOOKUP,
	REPORT_FOUND,
	REPORT_FAIL,
	REPORT_INVALID,
	REPORT_DISABLED,
} ident_message;

static CNCB ident_connected;
static PF read_ident_reply;

static void client_fail(struct auth_client *auth, ident_message message);
static void client_success(struct auth_client *auth);
static char * get_valid_ident(char *buf);

static int ident_timeout = IDENT_TIMEOUT_DEFAULT;
static bool ident_enable = true;


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
static void
ident_connected(rb_fde_t *F __unused, int error, void *data)
{
	struct auth_client *auth = data;
	struct ident_query *query;
	char authbuf[32];
	int authlen;

	lrb_assert(auth != NULL);
	query = get_provider_data(auth, SELF_PID);
	lrb_assert(query != NULL);

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
	struct ident_query *query;
	char buf[IDENT_BUFSIZE + 1];	/* buffer to read auth reply into */
	ident_message message = REPORT_FAIL;
	char *s = NULL;
	char *t = NULL;
	ssize_t len;
	int count;

	lrb_assert(auth != NULL);
	query = get_provider_data(auth, SELF_PID);
	lrb_assert(query != NULL);

	len = rb_read(F, buf, IDENT_BUFSIZE);
	if(len < 0 && rb_ignore_errno(errno))
	{
		rb_setselect(F, RB_SELECT_READ, read_ident_reply, auth);
		return;
	}

	if(len > 0)
	{
		if((s = get_valid_ident(buf)) != NULL)
		{
			t = auth->username;

			while (*s == '~' || *s == '^')
				s++;

			for (count = USERLEN; *s && count; s++)
			{
				if(*s == '@' || *s == '\r' || *s == '\n')
					break;

				if(*s != ' ' && *s != ':' && *s != '[')
				{
					*t++ = *s;
					count--;
				}
			}
			*t = '\0';
		}
		else
			message = REPORT_INVALID;
	}

	if(s == NULL)
		client_fail(auth, message);
	else
		client_success(auth);
}

static void
client_fail(struct auth_client *auth, ident_message report)
{
	struct ident_query *query = get_provider_data(auth, SELF_PID);

	lrb_assert(query != NULL);

	rb_strlcpy(auth->username, "*", sizeof(auth->username));

	if(query->F != NULL)
		rb_close(query->F);

	rb_free(query);
	set_provider_data(auth, SELF_PID, NULL);
	set_provider_timeout_absolute(auth, SELF_PID, 0);

	notice_client(auth->cid, messages[report]);
	provider_done(auth, SELF_PID);
}

static void
client_success(struct auth_client *auth)
{
	struct ident_query *query = get_provider_data(auth, SELF_PID);

	lrb_assert(query != NULL);

	if(query->F != NULL)
		rb_close(query->F);

	rb_free(query);
	set_provider_data(auth, SELF_PID, NULL);
	set_provider_timeout_absolute(auth, SELF_PID, 0);

	notice_client(auth->cid, messages[REPORT_FOUND]);
	provider_done(auth, SELF_PID);
}

/* get_valid_ident
 * parse ident query reply from identd server
 *
 * Taken from old s_auth.c --Elizafox
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
		return NULL;

	*colon1Ptr = '\0';
	colon1Ptr++;
	colon2Ptr = strchr(colon1Ptr, ':');
	if(!colon2Ptr)
		return NULL;

	*colon2Ptr = '\0';
	colon2Ptr++;
	commaPtr = strchr(remotePortString, ',');

	if(!commaPtr)
		return NULL;

	*commaPtr = '\0';
	commaPtr++;

	remp = atoi(remotePortString);
	if(!remp)
		return NULL;

	locp = atoi(commaPtr);
	if(!locp)
		return NULL;

	/* look for USERID bordered by first pair of colons */
	if(!strstr(colon1Ptr, "USERID"))
		return NULL;

	colon3Ptr = strchr(colon2Ptr, ':');
	if(!colon3Ptr)
		return NULL;

	*colon3Ptr = '\0';
	colon3Ptr++;
	return (colon3Ptr);
}

static void
ident_destroy(void)
{
	struct auth_client *auth;
	rb_dictionary_iter iter;

	/* Nuke all ident queries */
	RB_DICTIONARY_FOREACH(auth, &iter, auth_clients)
	{
		if(get_provider_data(auth, SELF_PID) != NULL)
			client_fail(auth, REPORT_FAIL);
	}
}

static bool
ident_start(struct auth_client *auth)
{
	struct ident_query *query = rb_malloc(sizeof(struct ident_query));
	struct rb_sockaddr_storage l_addr, c_addr;
	int family = GET_SS_FAMILY(&auth->c_addr);

	lrb_assert(get_provider_data(auth, SELF_PID) == NULL);

	if(!ident_enable)
	{
		rb_free(query);
		notice_client(auth->cid, messages[REPORT_DISABLED]);
		set_provider_done(auth, SELF_PID);
		return true;
	}

	notice_client(auth->cid, messages[REPORT_LOOKUP]);

	set_provider_data(auth, SELF_PID, query);
	set_provider_timeout_relative(auth, SELF_PID, ident_timeout);

	if((query->F = rb_socket(family, SOCK_STREAM, 0, "ident")) == NULL)
	{
		warn_opers(L_DEBUG, "Could not create ident socket: %s", strerror(errno));
		client_fail(auth, REPORT_FAIL);
		return true;	/* Not a fatal error */
	}

	/* Build sockaddr_storages for rb_connect_tcp below */
	l_addr = auth->l_addr;
	c_addr = auth->c_addr;

	SET_SS_PORT(&l_addr, 0);
	SET_SS_PORT(&c_addr, htons(113));

	rb_connect_tcp(query->F, (struct sockaddr *)&c_addr,
			(struct sockaddr *)&l_addr,
			GET_SS_LEN(&l_addr), ident_connected,
			auth, ident_timeout);

	set_provider_running(auth, SELF_PID);

	return true;
}

static void
ident_cancel(struct auth_client *auth)
{
	struct ident_query *query = get_provider_data(auth, SELF_PID);

	if(query != NULL)
		client_fail(auth, REPORT_FAIL);
}

static void
add_conf_ident_timeout(const char *key __unused, int parc __unused, const char **parv)
{
	int timeout = atoi(parv[0]);

	if(timeout < 0)
	{
		warn_opers(L_CRIT, "Ident: ident timeout < 0 (value: %d)", timeout);
		exit(EX_PROVIDER_ERROR);
	}

	ident_timeout = timeout;
}

static void
set_ident_enabled(const char *key __unused, int parc __unused, const char **parv)
{
	ident_enable = (*parv[0] == '1');
}

struct auth_opts_handler ident_options[] =
{
	{ "ident_timeout", 1, add_conf_ident_timeout },
	{ "ident_enabled", 1, set_ident_enabled },
	{ NULL, 0, NULL },
};


struct auth_provider ident_provider =
{
	.name = "ident",
	.letter = 'I',
	.start = ident_start,
	.destroy = ident_destroy,
	.cancel = ident_cancel,
	.timeout = ident_cancel,
	.opt_handlers = ident_options,
};
