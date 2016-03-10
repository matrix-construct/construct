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
#include "res.h"
#include "dns.h"

struct dns_query
{
	rb_dlink_node node;

	struct auth_client *auth;		/* Our client */
	struct DNSQuery query;			/* DNS query */
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

static EVH timeout_dns_queries_event;
static void client_fail(struct dns_query *query, dns_message message);
static void client_success(struct dns_query *query);
static void get_dns_answer(void *userdata, struct DNSReply *reply);

rb_dlink_list queries;
static struct ev_entry *timeout_ev;
int timeout = 30;


bool client_dns_init(void)
{
	timeout_ev = rb_event_addish("timeout_dns_queries_event", timeout_dns_queries_event, NULL, 1);
	return (timeout_ev != NULL);
}

void client_dns_destroy(void)
{
	rb_dlink_node *ptr, *nptr;
	struct dns_query *query;

	RB_DLINK_FOREACH_SAFE(ptr, nptr, queries.head)
	{
		client_fail(ptr->data, REPORT_FAIL);
		rb_dlinkDelete(ptr, &queries);
		rb_free(ptr);
	}

	rb_event_delete(timeout_ev);
}

bool client_dns_start(struct auth_client *auth)
{
	struct dns_query *query = rb_malloc(sizeof(struct dns_query));

	query->auth = auth;
	query->timeout = rb_current_time() + timeout;

	query->query.ptr = query;
	query->query.callback = get_dns_answer;

	gethost_byaddr(&auth->c_addr, &query->query);
	notice_client(auth, messages[REPORT_LOOKUP]);
	set_provider(auth, PROVIDER_RDNS);
	return true;
}

void client_dns_cancel(struct auth_client *auth)
{
	rb_dlink_node *ptr;

	/* Bah, the stupid DNS resolver code doesn't have a cancellation
	 * func... */
	RB_DLINK_FOREACH(ptr, queries.head)
	{
		struct dns_query *query = ptr->data;

		if(query->auth == auth)
		{
			/* This will get cleaned up later by the DNS stuff */
			client_fail(query, REPORT_FAIL);
			return;
		}
	}
}

static void
get_dns_answer(void *userdata, struct DNSReply *reply)
{
	struct dns_query *query = userdata;
	struct auth_client *auth = query->auth;
	rb_dlink_node *ptr, *nptr;
	bool fail = false;
	dns_message response;

	if(reply == NULL || auth == NULL)
	{
		response = REPORT_FAIL;
		fail = true;
		goto cleanup;
	}

	if(!sockcmp(&auth->c_addr, &reply->addr, GET_SS_FAMILY(&auth->c_addr)))
	{
		response = REPORT_FAIL;
		fail = true;
		goto cleanup;
	}

	if(strlen(reply->h_name) > HOSTLEN)
	{
		/* Ah well. */
		response = REPORT_TOOLONG;
		fail = true;
		goto cleanup;
	}

	rb_strlcpy(auth->hostname, reply->h_name, HOSTLEN + 1);

cleanup:
	/* Clean us up off the pending queries list */
	RB_DLINK_FOREACH_SAFE(ptr, nptr, queries.head)
	{
		struct dns_query *query_l = ptr->data;

		if(query == query_l)
		{
			/* Found */
			if(fail)
				client_fail(query, response);
			else
				client_success(query);

			rb_dlinkDelete(ptr, &queries);
			rb_free(query);
			return;
		}
	}
}

/* Timeout outstanding queries */
static void timeout_dns_queries_event(void *notused)
{
	rb_dlink_node *ptr;

	/* NOTE - we do not delete queries from the list from a timeout, when
	 * the query times out later it will be deleted.
	 */
	RB_DLINK_FOREACH(ptr, queries.head)
	{
		struct dns_query *query = ptr->data;

		if(query->auth && query->timeout < rb_current_time())
		{
			client_fail(query, REPORT_FAIL);
			return;
		}
	}
}

static void client_fail(struct dns_query *query, dns_message report)
{
	struct auth_client *auth = query->auth;

	if(auth)
	{
		rb_strlcpy(auth->hostname, "*", sizeof(auth->hostname));
		notice_client(auth, messages[report]);
		provider_done(auth, PROVIDER_RDNS);
		query->auth = NULL;
	}
}

static void client_success(struct dns_query *query)
{
	struct auth_client *auth = query->auth;

	if(auth)
	{
		notice_client(auth, messages[REPORT_FOUND]);
		provider_done(auth, PROVIDER_RDNS);
		query->auth = NULL;
	}
}

struct auth_provider rdns_provider =
{
	.id = PROVIDER_RDNS,
	.init = client_dns_init,
	.destroy = client_dns_destroy,
	.start = client_dns_start,
	.cancel = client_dns_cancel,
	.completed = NULL,
};
