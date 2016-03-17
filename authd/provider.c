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

/* So the basic design here is to have "authentication providers" that do
 * things like query ident and blacklists and even open proxies.
 *
 * Providers are registered statically in the struct auth_providers array. You will
 * probably want to add an item to the provider_t enum also.
 *
 * Providers can either return failure immediately, immediate acceptance, or
 * do work in the background (calling set_provider to signal this).
 *
 * It is up to providers to keep their own state on clients if they need to.
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
 * --Elizafox, 9 March 2016
 */

#include "authd.h"
#include "provider.h"

rb_dlink_list auth_providers;

/* Clients waiting */
struct auth_client auth_clients[MAX_CLIENTS];

/* Load a provider */
void load_provider(struct auth_provider *provider)
{
	provider->init();
	rb_dlinkAdd(provider, &provider->node, &auth_providers);
}

void unload_provider(struct auth_provider *provider)
{
	provider->destroy();
	rb_dlinkDelete(&provider->node, &auth_providers);
}

/* Initalise all providers */
void init_providers(void)
{
	load_provider(&rdns_provider);
	load_provider(&ident_provider);
}

/* Terminate all providers */
void destroy_providers(void)
{
	rb_dlink_node *ptr;
	struct auth_provider *provider;

	/* Cancel outstanding connections */
	for (size_t i = 0; i < MAX_CLIENTS; i++)
	{
		if(auth_clients[i].cid)
		{
			/* TBD - is this the right thing?
			 * (NOTE - this error message is designed for morons) */
			reject_client(&auth_clients[i], 0, true,
					"IRC server reloading... try reconnecting in a few seconds");
		}
	}

	RB_DLINK_FOREACH(ptr, auth_providers.head)
	{
		provider = ptr->data;

		if(provider->destroy)
			provider->destroy();
	}
}

/* Cancel outstanding providers for a client */
void cancel_providers(struct auth_client *auth)
{
	rb_dlink_node *ptr;
	struct auth_provider *provider;

	RB_DLINK_FOREACH(ptr, auth_providers.head)
	{
		provider = ptr->data;

		if(provider->cancel && is_provider(auth, provider->id))
			/* Cancel if required */
			provider->cancel(auth);
	}
}

/* Provider is done */
void provider_done(struct auth_client *auth, provider_t id)
{
	rb_dlink_node *ptr;
	struct auth_provider *provider;

	unset_provider(auth, id);

	if(!auth->providers)
	{
		/* No more providers, done */
		accept_client(auth, 0);
		return;
	}

	RB_DLINK_FOREACH(ptr, auth_providers.head)
	{
		provider = ptr->data;

		if(provider->completed && is_provider(auth, provider->id))
			/* Notify pending clients who asked for it */
			provider->completed(auth, id);
	}
}

/* Reject a client, cancel outstanding providers if any if hard set to true */
void reject_client(struct auth_client *auth, provider_t id, bool hard, const char *reason)
{
	uint16_t cid = auth->cid;
	char reject;

	switch(id)
	{
	case PROVIDER_RDNS:
		reject = 'D';
		break;
	case PROVIDER_IDENT:
		reject = 'I';
		break;
	case PROVIDER_BLACKLIST:
		reject = 'B';
		break;
	case PROVIDER_NULL:
	default:
		reject = 'N';
		break;
	}

	rb_helper_write(authd_helper, "R %x %c :%s", auth->cid, reject, reason);

	unset_provider(auth, id);

	if(hard && auth->providers)
	{
		cancel_providers(auth);
		memset(&auth_clients[cid], 0, sizeof(struct auth_client));
	}
}

/* Accept a client, cancel outstanding providers if any */
void accept_client(struct auth_client *auth, provider_t id)
{
	uint16_t cid = auth->cid;

	rb_helper_write(authd_helper, "A %x %s %s", auth->cid, auth->username, auth->hostname);

	unset_provider(auth, id);

	if(auth->providers)
		cancel_providers(auth);

	memset(&auth_clients[cid], 0, sizeof(struct auth_client));
}

/* Send a notice to a client */
void notice_client(struct auth_client *auth, const char *notice)
{
	rb_helper_write(authd_helper, "N %x :%s", auth->cid, notice);
}

/* Begin authenticating user */
static void start_auth(const char *cid, const char *l_ip, const char *l_port, const char *c_ip, const char *c_port)
{
	struct auth_provider *provider;
	struct auth_client *auth;
	long lcid = strtol(cid, NULL, 16);
	rb_dlink_node *ptr;

	if(lcid >= MAX_CLIENTS)
		return;

	auth = &auth_clients[lcid];
	if(auth->cid != 0)
		/* Shouldn't get here */
		return;

	auth->cid = (uint16_t)lcid;

	rb_strlcpy(auth->l_ip, l_ip, sizeof(auth->l_ip));
	auth->l_port = (uint16_t)atoi(l_port);	/* should be safe */

	rb_strlcpy(auth->c_ip, c_ip, sizeof(auth->c_ip));
	auth->c_port = (uint16_t)atoi(c_port);

	RB_DLINK_FOREACH(ptr, auth_providers.head)
	{
		provider = ptr->data;

		/* Execute providers */
		if(!provider->start(auth))
		{
			/* Rejected immediately */
			cancel_providers(auth);
			return;
		}
	}

	/* If no providers are running, accept the client */
	if(!auth->providers)
		accept_client(auth, 0);
}

/* Callback for the initiation */
void handle_new_connection(int parc, char *parv[])
{
	if(parc < 7)
		return;

	start_auth(parv[1], parv[2], parv[3], parv[4], parv[5]);
}
