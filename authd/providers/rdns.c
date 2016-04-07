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

#define SELF_PID (rdns_provider.id)

struct user_query
{
	struct dns_query *query;		/* Pending DNS query */
};

/* Goinked from old s_auth.c --Elizabeth */
static const char *messages[] =
{
	"*** Looking up your hostname...",
	"*** Couldn't look up your hostname",
	"*** Your hostname is too long, ignoring hostname",
};

typedef enum
{
	REPORT_LOOKUP,
	REPORT_FAIL,
	REPORT_TOOLONG,
} dns_message;

static void client_fail(struct auth_client *auth, dns_message message);
static void client_success(struct auth_client *auth);
static void dns_answer_callback(const char *res, bool status, query_type type, void *data);

static int rdns_timeout = RDNS_TIMEOUT_DEFAULT;

static void
dns_answer_callback(const char *res, bool status, query_type type, void *data)
{
	struct auth_client *auth = data;
	struct user_query *query = get_provider_data(auth, SELF_PID);

	lrb_assert(query != NULL);

	if(res == NULL || status == false)
		client_fail(auth, REPORT_FAIL);
	else if(strlen(res) > HOSTLEN)
		client_fail(auth, REPORT_TOOLONG);
	else
	{
		rb_strlcpy(auth->hostname, res, HOSTLEN + 1);
		client_success(auth);
	}
}

static void
client_fail(struct auth_client *auth, dns_message report)
{
	struct user_query *query = get_provider_data(auth, SELF_PID);

	lrb_assert(query != NULL);

	rb_strlcpy(auth->hostname, "*", sizeof(auth->hostname));

	notice_client(auth->cid, messages[report]);
	cancel_query(query->query);

	rb_free(query);

	set_provider_data(auth, SELF_PID, NULL);
	set_provider_timeout_absolute(auth, SELF_PID, 0);
	provider_done(auth, SELF_PID);
}

static void
client_success(struct auth_client *auth)
{
	struct user_query *query = get_provider_data(auth, SELF_PID);

	lrb_assert(query != NULL);

	notice_client(auth->cid, "*** Found your hostname: %s", auth->hostname);
	cancel_query(query->query);

	rb_free(query);

	set_provider_data(auth, SELF_PID, NULL);
	set_provider_timeout_absolute(auth, SELF_PID, 0);
	provider_done(auth, SELF_PID);
}

static void
rdns_destroy(void)
{
	struct auth_client *auth;
	rb_dictionary_iter iter;

	RB_DICTIONARY_FOREACH(auth, &iter, auth_clients)
	{
		if(get_provider_data(auth, SELF_PID) != NULL)
			client_fail(auth, REPORT_FAIL);
	}
}

static bool
rdns_start(struct auth_client *auth)
{
	struct user_query *query = rb_malloc(sizeof(struct user_query));

	set_provider_data(auth, SELF_PID, query);
	set_provider_timeout_relative(auth, SELF_PID, rdns_timeout);

	query->query = lookup_hostname(auth->c_ip, dns_answer_callback, auth);

	notice_client(auth->cid, messages[REPORT_LOOKUP]);
	set_provider_running(auth, SELF_PID);
	return true;
}

static void
rdns_cancel(struct auth_client *auth)
{
	struct user_query *query = get_provider_data(auth, SELF_PID);

	if(query != NULL)
		client_fail(auth, REPORT_FAIL);
}

static void
add_conf_dns_timeout(const char *key, int parc, const char **parv)
{
	int timeout = atoi(parv[0]);

	if(timeout < 0)
	{
		warn_opers(L_CRIT, "rDNS: DNS timeout < 0 (value: %d)", timeout);
		exit(EX_PROVIDER_ERROR);
	}

	rdns_timeout = timeout;
}

struct auth_opts_handler rdns_options[] =
{
	{ "rdns_timeout", 1, add_conf_dns_timeout },
	{ NULL, 0, NULL },
};

struct auth_provider rdns_provider =
{
	.name = "rdns",
	.letter = 'R',
	.destroy = rdns_destroy,
	.start = rdns_start,
	.cancel = rdns_cancel,
	.timeout = rdns_cancel,
	.opt_handlers = rdns_options,
};
