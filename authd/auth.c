/* authd/auth.c - authentication provider framework
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
 * When a provider has done its work, it should call provider_done.
 *
 * --Elizafox, 9 March 2016
 */

#include "authd.h"
#include "auth.h"

/*****************************************************************************/
/* This is just some test code for the system */
bool dummy_init(void)
{
	/* Do a dummy init */
	return true;
}

void dummy_destroy(void)
{
	/* Do a dummy destroy */
}

bool dummy_start(struct auth_client *auth)
{
	/* Set the client's username to testhost as a test */
	strcpy(auth->username, "testhost");
	return true;
}

void dummy_cancel(struct auth_client *auth)
{
	/* Does nothing */
}
/*****************************************************************************/

#define NULL_PROVIDER {			\
	.provider = PROVIDER_NULL,	\
	.init = NULL,			\
	.destroy = NULL,		\
	.start = NULL,			\
	.cancel = NULL,			\
	.completed = NULL,		\
}

#define DUMMY_PROVIDER {		\
	.provider = PROVIDER_DUMMY,	\
	.init = dummy_init,		\
	.destroy = dummy_destroy,	\
	.start = dummy_start,		\
	.cancel = dummy_cancel,		\
	.completed = NULL,		\
}

/* Providers */
static struct auth_provider auth_providers[] =
{
	NULL_PROVIDER,
	DUMMY_PROVIDER
};

/* Clients waiting */
struct auth_client auth_clients[MAX_CLIENTS];

/* Initalise all providers */
void init_providers(void)
{
	struct auth_provider *pptr;

	AUTH_PROVIDER_FOREACH(pptr)
	{
		if(pptr->init && !pptr->init())
			/* Provider failed to init, time to go */
			exit(1);
	}
}

/* Terminate all providers */
void destroy_providerss(void)
{
	struct auth_provider *pptr;

	/* Cancel outstanding connections */
	for (size_t i = 0; i < MAX_CLIENTS; i++)
	{
		if(auth_clients[i].cid)
		{
			/* TBD - is this the right thing?
			 * (NOTE - this error message is designed for morons) */
			reject_client(&auth_clients[i],
					"IRC server reloading... try reconnecting in a few seconds");
		}
	}

	AUTH_PROVIDER_FOREACH(pptr)
	{
		if(pptr->destroy)
			pptr->destroy();
	}
}

/* Cancel outstanding providers for a client */
void cancel_providers(struct auth_client *auth)
{
	struct auth_provider *pptr;

	AUTH_PROVIDER_FOREACH(pptr)
	{
		if(pptr->cancel && is_provider(auth, pptr->provider))
			/* Cancel if required */
			pptr->cancel(auth);
	}
}

/* Provider is done */
void provider_done(struct auth_client *auth, provider_t provider)
{
	struct auth_provider *pptr;

	unset_provider(auth, provider);

	if(!auth->providers)
	{
		/* No more providers, done */
		accept_client(auth);
		return;
	}

	AUTH_PROVIDER_FOREACH(pptr)
	{
		if(pptr->completed && is_provider(auth, pptr->provider))
			/* Notify pending clients who asked for it */
			pptr->completed(auth, provider);
	}
}

/* Reject a client, cancel outstanding providers if any */
void reject_client(struct auth_client *auth, const char *reason)
{
	uint16_t cid = auth->cid;

	rb_helper_write(authd_helper, "R %x :%s", auth->cid, reason);

	if(auth->providers)
		cancel_providers(auth);

	memset(&auth_clients[cid], 0, sizeof(struct auth_client));
}

/* Accept a client, cancel outstanding providers if any */
void accept_client(struct auth_client *auth)
{
	uint16_t cid = auth->cid;

	rb_helper_write(authd_helper, "A %x %s %s", auth->cid, auth->username, auth->hostname);

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
void start_auth(const char *cid, const char *l_ip, const char *l_port, const char *c_ip, const char *c_port)
{
	struct auth_provider *pptr;
	struct auth_client *auth;
	long lcid = strtol(cid, NULL, 16);

	if(lcid >= MAX_CLIENTS)
		return;

	auth = &auth_clients[lcid];
	if(auth->cid != 0)
		/* Shouldn't get here */
		return;

	auth->cid = (uint16_t)lcid;

	rb_strlcpy(auth->l_ip, l_ip, sizeof(auth->l_ip));
	auth->l_port = (uint16_t)atoi(l_port); /* Safe cast, port shouldn't exceed 16 bits  */

	rb_strlcpy(auth->c_ip, c_ip, sizeof(auth->c_ip));
	auth->c_port = (uint16_t)atoi(c_port);

	AUTH_PROVIDER_FOREACH(pptr)
	{
		/* Execute providers */
		if(!pptr->start(auth))
		{
			/* Rejected immediately */
			cancel_providers(auth);
			return;
		}
	}

	/* If no providers are running, accept the client */
	if(!auth->providers)
		accept_client(auth);
}

/* Callback for the initiation */
void handle_new_connection(int parc, char *parv[])
{
	if(parc < 5)
		return;

	start_auth(parv[1], parv[2], parv[3], parv[4], parv[5]);
}
