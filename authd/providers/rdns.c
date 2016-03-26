/* authd/providers/rdns.c - rDNS lookup provider for authd
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
#include "rb_commio.h"
#include "authd.h"
#include "provider.h"
#include "notice.h"
#include "res.h"
#include "dns.h"

struct user_query
{
	struct dns_query *query;		/* Pending DNS query */
	time_t timeout;				/* When the request times out */
};

/* Goinked from old s_auth.c --Elizabeth */
static const char *messages[] =
{
	"*** Looking up your hostname...",
	"*** Found your hostname",
	"*** Couldn't look up your hostname",
	"*** Your hostname is too long, ignoring hostname",
};

typedef enum
{
	REPORT_LOOKUP,
	REPORT_FOUND,
	REPORT_FAIL,
	REPORT_TOOLONG,
} dns_message;

static void client_fail(struct auth_client *auth, dns_message message);
static void client_success(struct auth_client *auth);
static void dns_answer_callback(const char *res, bool status, query_type type, void *data);

static struct ev_entry *timeout_ev;
static EVH timeout_dns_queries_event;
static int rdns_timeout = 15;

static void
dns_answer_callback(const char *res, bool status, query_type type, void *data)
{
	struct auth_client *auth = data;
	struct user_query *query = auth->data[PROVIDER_RDNS];

	if(query == NULL || res == NULL || status == false)
		client_fail(auth, REPORT_FAIL);
	else if(strlen(res) > HOSTLEN)
		client_fail(auth, REPORT_TOOLONG);
	else
	{
		rb_strlcpy(auth->hostname, res, HOSTLEN + 1);
		client_success(auth);
	}
}

/* Timeout outstanding queries */
static void
timeout_dns_queries_event(void *notused)
{
	struct auth_client *auth;
	rb_dictionary_iter iter;

	RB_DICTIONARY_FOREACH(auth, &iter, auth_clients)
	{
		struct user_query *query = auth->data[PROVIDER_RDNS];

		if(query != NULL && query->timeout < rb_current_time())
		{
			client_fail(auth, REPORT_FAIL);
			return;
		}
	}
}

static void
client_fail(struct auth_client *auth, dns_message report)
{
	struct user_query *query = auth->data[PROVIDER_RDNS];

	if(query == NULL)
		return;

	rb_strlcpy(auth->hostname, "*", sizeof(auth->hostname));

	notice_client(auth->cid, messages[report]);
	cancel_query(query->query);

	rb_free(query);
	auth->data[PROVIDER_RDNS] = NULL;

	provider_done(auth, PROVIDER_RDNS);
}

static void
client_success(struct auth_client *auth)
{
	struct user_query *query = auth->data[PROVIDER_RDNS];

	notice_client(auth->cid, messages[REPORT_FOUND]);
	cancel_query(query->query);

	rb_free(query);
	auth->data[PROVIDER_RDNS] = NULL;

	provider_done(auth, PROVIDER_RDNS);
}

static bool
client_dns_init(void)
{
	timeout_ev = rb_event_addish("timeout_dns_queries_event", timeout_dns_queries_event, NULL, 1);
	return (timeout_ev != NULL);
}

static void
client_dns_destroy(void)
{
	struct auth_client *auth;
	rb_dictionary_iter iter;

	RB_DICTIONARY_FOREACH(auth, &iter, auth_clients)
	{
		if(auth->data[PROVIDER_RDNS] != NULL)
			client_fail(auth, REPORT_FAIL);
	}

	rb_event_delete(timeout_ev);
}

static bool
client_dns_start(struct auth_client *auth)
{
	struct user_query *query = rb_malloc(sizeof(struct user_query));

	query->timeout = rb_current_time() + rdns_timeout;

	auth->data[PROVIDER_RDNS] = query;

	query->query = lookup_hostname(auth->c_ip, dns_answer_callback, auth);

	notice_client(auth->cid, messages[REPORT_LOOKUP]);
	set_provider_on(auth, PROVIDER_RDNS);
	return true;
}

static void
client_dns_cancel(struct auth_client *auth)
{
	struct user_query *query = auth->data[PROVIDER_RDNS];

	if(query != NULL)
		client_fail(auth, REPORT_FAIL);
}

static void
add_conf_dns_timeout(const char *key, int parc, const char **parv)
{
	int timeout = atoi(parv[0]);

	if(timeout < 0)
	{
		warn_opers(L_CRIT, "BUG: DNS timeout < 0 (value: %d)", timeout);
		return;
	}

	rdns_timeout = timeout;
}

struct auth_opts_handler rdns_options[] =
{
	{ "dns_timeout", 1, add_conf_dns_timeout },
	{ NULL, 0, NULL },
};

struct auth_provider rdns_provider =
{
	.id = PROVIDER_RDNS,
	.init = client_dns_init,
	.destroy = client_dns_destroy,
	.start = client_dns_start,
	.cancel = client_dns_cancel,
	.completed = NULL,
	.opt_handlers = rdns_options,
};
