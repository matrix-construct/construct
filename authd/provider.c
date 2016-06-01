/* authd/provider.c - authentication provider framework
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

/* The basic design here is to have "authentication providers" that do things
 * like query ident and blacklists and even open proxies.
 *
 * Providers are registered in the auth_providers linked list. It is planned to
 * use a bitmap to store provider ID's later.
 *
 * Providers can either return failure immediately, immediate acceptance, or do
 * work in the background (calling set_provider to signal this).
 *
 * Provider-specific data for each client can be kept in an index of the data
 * struct member (using the provider's ID).
 *
 * All providers must implement at a minimum a perform_provider function. You
 * don't have to implement the others if you don't need them.
 *
 * Providers may kick clients off by rejecting them. Upon rejection, all
 * providers are cancelled. They can also unconditionally accept them.
 *
 * When a provider is done and is neutral on accepting/rejecting a client, it
 * should call provider_done. Do NOT call this if you have accepted or rejected
 * the client.
 *
 * Eventually, stuff like *:line handling will be moved here, but that means we
 * have to talk to bandb directly first.
 *
 * --Elizafox, 9 March 2016
 */

#include "stdinc.h"
#include "rb_dictionary.h"
#include "rb_lib.h"
#include "authd.h"
#include "provider.h"
#include "notice.h"

static EVH provider_timeout_event;

rb_dictionary *auth_clients;
rb_dlink_list auth_providers;

static rb_dlink_list free_pids;
static uint32_t allocated_pids;
static struct ev_entry *timeout_ev;

/* Initalise all providers */
void
init_providers(void)
{
	auth_clients = rb_dictionary_create("pending auth clients", rb_uint32cmp);
	timeout_ev = rb_event_addish("provider_timeout_event", provider_timeout_event, NULL, 1);

	load_provider(&rdns_provider);
	load_provider(&ident_provider);
	load_provider(&blacklist_provider);
	load_provider(&opm_provider);
}

/* Terminate all providers */
void
destroy_providers(void)
{
	rb_dlink_node *ptr, *nptr;
	rb_dictionary_iter iter;
	struct auth_client *auth;

	/* Cancel outstanding connections */
	RB_DICTIONARY_FOREACH(auth, &iter, auth_clients)
	{
		auth_client_ref(auth);

		/* TBD - is this the right thing? */
		reject_client(auth, UINT32_MAX, "destroy",
			"Authentication system is down... try reconnecting in a few seconds");

		auth_client_unref(auth);
	}

	RB_DLINK_FOREACH_SAFE(ptr, nptr, auth_providers.head)
	{
		struct auth_provider *provider = ptr->data;

		if(provider->destroy)
			provider->destroy();

		rb_dlinkDelete(ptr, &auth_providers);
	}

	rb_dictionary_destroy(auth_clients, NULL, NULL);
	rb_event_delete(timeout_ev);
}

/* Load a provider */
void
load_provider(struct auth_provider *provider)
{
	/* Assign a PID */
	if(rb_dlink_list_length(&free_pids) > 0)
	{
		/* use the free list */
		provider->id = RB_POINTER_TO_UINT(free_pids.head->data);
		rb_dlinkDestroy(free_pids.head, &free_pids);
	}
	else
	{
		if(allocated_pids == MAX_PROVIDERS || allocated_pids == UINT32_MAX)
		{
			warn_opers(L_WARN, "Cannot load additional provider, max reached!");
			return;
		}

		provider->id = allocated_pids++;
	}

	if(provider->opt_handlers != NULL)
	{
		struct auth_opts_handler *handler;

		for(handler = provider->opt_handlers; handler->option != NULL; handler++)
			rb_dictionary_add(authd_option_handlers, handler->option, handler);
	}

	if(provider->stats_handler.letter != '\0')
		authd_stat_handlers[(unsigned char)provider->stats_handler.letter] = provider->stats_handler.handler;

	if(provider->init != NULL)
		provider->init();

	rb_dlinkAdd(provider, &provider->node, &auth_providers);
}

void
unload_provider(struct auth_provider *provider)
{
	if(provider->opt_handlers != NULL)
	{
		struct auth_opts_handler *handler;

		for(handler = provider->opt_handlers; handler->option != NULL; handler++)
			rb_dictionary_delete(authd_option_handlers, handler->option);
	}

	if(provider->stats_handler.letter != '\0')
		authd_stat_handlers[(unsigned char)provider->stats_handler.letter] = NULL;

	if(provider->destroy != NULL)
		provider->destroy();

	rb_dlinkDelete(&provider->node, &auth_providers);

	/* Reclaim ID */
	rb_dlinkAddAlloc(RB_UINT_TO_POINTER(provider->id), &free_pids);
}

void
auth_client_free(struct auth_client *auth)
{
	rb_dictionary_delete(auth_clients, RB_UINT_TO_POINTER(auth->cid));
	rb_free(auth->data);
	rb_free(auth);
}

/* Cancel outstanding providers for a client (if any). */
void
cancel_providers(struct auth_client *auth)
{
	if(auth->providers_cancelled)
		return;

	auth->providers_cancelled = true;

	if(auth->providers_active > 0)
	{
		rb_dlink_node *ptr;

		RB_DLINK_FOREACH(ptr, auth_providers.head)
		{
			struct auth_provider *provider = ptr->data;

			if(provider->cancel != NULL && is_provider_running(auth, provider->id))
				/* Cancel if required */
				provider->cancel(auth);
		}
	}
}

/* Provider is done */
void
provider_done(struct auth_client *auth, uint32_t id)
{
	rb_dlink_node *ptr;

	lrb_assert(is_provider_running(auth, id));
	lrb_assert(id != UINT32_MAX);
	lrb_assert(id < allocated_pids);

	set_provider_done(auth, id);

	if(auth->providers_active == 0 && !auth->providers_starting)
	{
		/* All done */
		accept_client(auth);
		return;
	}

	RB_DLINK_FOREACH(ptr, auth_providers.head)
	{
		struct auth_provider *provider = ptr->data;

		if(provider->completed != NULL && is_provider_running(auth, provider->id))
			/* Notify pending clients who asked for it */
			provider->completed(auth, id);
	}
}

/* Reject a client and cancel any outstanding providers */
void
reject_client(struct auth_client *auth, uint32_t id, const char *data, const char *fmt, ...)
{
	char buf[BUFSIZE];
	va_list args;

	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	/* We send back username and hostname in case ircd wants to overrule our decision.
	 * In the future this may not be the case.
	 * --Elizafox
	 */
	rb_helper_write(authd_helper, "R %x %c %s %s %s :%s",
		auth->cid, id != UINT32_MAX ? auth->data[id].provider->letter : '*',
		auth->username, auth->hostname,
		data == NULL ? "*" : data, buf);

	if(id != UINT32_MAX)
		set_provider_done(auth, id);

	cancel_providers(auth);
}

/* Accept a client and cancel outstanding providers if any */
void
accept_client(struct auth_client *auth)
{
	rb_helper_write(authd_helper, "A %x %s %s", auth->cid, auth->username, auth->hostname);
	cancel_providers(auth);
}

/* Begin authenticating user */
static void
start_auth(const char *cid, const char *l_ip, const char *l_port, const char *c_ip, const char *c_port)
{
	struct auth_client *auth;
	unsigned long long lcid = strtoull(cid, NULL, 16);
	rb_dlink_node *ptr;

	if(lcid == 0 || lcid > UINT32_MAX)
		return;

	auth = rb_malloc(sizeof(struct auth_client));
	auth_client_ref(auth);
	auth->cid = (uint32_t)lcid;

	if(rb_dictionary_find(auth_clients, RB_UINT_TO_POINTER(auth->cid)) == NULL)
		rb_dictionary_add(auth_clients, RB_UINT_TO_POINTER(auth->cid), auth);
	else
	{
		warn_opers(L_CRIT, "provider: duplicate client added via start_auth: %s", cid);
		exit(EX_PROVIDER_ERROR);
	}

	rb_strlcpy(auth->l_ip, l_ip, sizeof(auth->l_ip));
	auth->l_port = (uint16_t)atoi(l_port);	/* should be safe */
	(void) rb_inet_pton_sock(l_ip, (struct sockaddr *)&auth->l_addr);
	SET_SS_PORT(&auth->l_addr, htons(auth->l_port));

	rb_strlcpy(auth->c_ip, c_ip, sizeof(auth->c_ip));
	auth->c_port = (uint16_t)atoi(c_port);
	(void) rb_inet_pton_sock(c_ip, (struct sockaddr *)&auth->c_addr);
	SET_SS_PORT(&auth->c_addr, htons(auth->c_port));

	rb_strlcpy(auth->hostname, "*", sizeof(auth->hostname));
	rb_strlcpy(auth->username, "*", sizeof(auth->username));

	auth->data = rb_malloc(allocated_pids * sizeof(struct auth_client_data));

	auth->providers_starting = true;
	RB_DLINK_FOREACH(ptr, auth_providers.head)
	{
		struct auth_provider *provider = ptr->data;

		auth->data[provider->id].provider = provider;

		lrb_assert(provider->start != NULL);

		/* Execute providers */
		if(!provider->start(auth))
			/* Rejected immediately */
			goto done;

		if(auth->providers_cancelled)
			break;
	}
	auth->providers_starting = false;

	/* If no providers are running, accept the client */
	if(auth->providers_active == 0)
		accept_client(auth);

done:
	auth_client_unref(auth);
}

/* Callback for the initiation */
void
handle_new_connection(int parc, char *parv[])
{
	if(parc < 6)
	{
		warn_opers(L_CRIT, "provider: received too few params for new connection (6 expected, got %d)", parc);
		exit(EX_PROVIDER_ERROR);
	}

	start_auth(parv[1], parv[2], parv[3], parv[4], parv[5]);
}

void
handle_cancel_connection(int parc, char *parv[])
{
	struct auth_client *auth;
	unsigned long long lcid;

	if(parc < 2)
	{
		warn_opers(L_CRIT, "provider: received too few params for new connection (2 expected, got %d)", parc);
		exit(EX_PROVIDER_ERROR);
	}

	lcid = strtoull(parv[1], NULL, 16);
	if(lcid == 0 || lcid > UINT32_MAX)
	{
		warn_opers(L_CRIT, "provider: got a request to cancel a connection that can't exist: %s", parv[1]);
		exit(EX_PROVIDER_ERROR);
	}

	if((auth = rb_dictionary_retrieve(auth_clients, RB_UINT_TO_POINTER((uint32_t)lcid))) == NULL)
	{
		/* This could happen as a race if we've accepted/rejected but they cancel, so don't die here.
		 * --Elizafox */
		return;
	}

	auth_client_ref(auth);
	cancel_providers(auth);
	auth_client_unref(auth);
}

static void
provider_timeout_event(void *notused __unused)
{
	struct auth_client *auth;
	rb_dictionary_iter iter;
	const time_t curtime = rb_current_time();

	RB_DICTIONARY_FOREACH(auth, &iter, auth_clients)
	{
		rb_dlink_node *ptr;

		auth_client_ref(auth);

		RB_DLINK_FOREACH(ptr, auth_providers.head)
		{
			struct auth_provider *provider = ptr->data;
			const time_t timeout = get_provider_timeout(auth, provider->id);

			if(is_provider_running(auth, provider->id) && provider->timeout != NULL &&
				timeout > 0 && timeout < curtime)
			{
				provider->timeout(auth);
			}
		}

		auth_client_unref(auth);
	}
}
