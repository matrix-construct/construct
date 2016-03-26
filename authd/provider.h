/* authd/provider.h - authentication provider framework
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

#ifndef __CHARYBDIS_AUTHD_PROVIDER_H__
#define __CHARYBDIS_AUTHD_PROVIDER_H__

#include "stdinc.h"
#include "authd.h"
#include "rb_dictionary.h"

#define MAX_PROVIDERS 32	/* This should be enough */

/* Registered providers */
typedef enum
{
	PROVIDER_RDNS,
	PROVIDER_IDENT,
	PROVIDER_BLACKLIST,
} provider_t;

struct auth_client
{
	uint16_t cid;				/* Client ID */

	char l_ip[HOSTIPLEN + 1];		/* Listener IP address */
	uint16_t l_port;			/* Listener port */
	struct rb_sockaddr_storage l_addr;	/* Listener address/port */

	char c_ip[HOSTIPLEN + 1];		/* Client IP address */
	uint16_t c_port;			/* Client port */
	struct rb_sockaddr_storage c_addr;	/* Client address/port */

	char hostname[HOSTLEN + 1];		/* Used for DNS lookup */
	char username[USERLEN + 1];		/* Used for ident lookup */

	uint32_t providers;			/* Providers at work,
						 * none left when set to 0 */
	uint32_t providers_done;		/* Providers completed */

	void *data[MAX_PROVIDERS];		/* Provider-specific data slots */
};

typedef bool (*provider_init_t)(void);
typedef void (*provider_destroy_t)(void);

typedef bool (*provider_start_t)(struct auth_client *);
typedef void (*provider_cancel_t)(struct auth_client *);
typedef void (*provider_complete_t)(struct auth_client *, provider_t);

struct auth_provider
{
	rb_dlink_node node;

	provider_t id;

	provider_init_t init;		/* Initalise the provider */
	provider_destroy_t destroy;	/* Terminate the provider */

	provider_start_t start;		/* Perform authentication */
	provider_cancel_t cancel;	/* Authentication cancelled */
	provider_complete_t completed;	/* Callback for when other performers complete (think dependency chains) */

	struct auth_opts_handler *opt_handlers;
};

extern rb_dlink_list auth_providers;
extern rb_dictionary *auth_clients;

extern struct auth_provider rdns_provider;
extern struct auth_provider ident_provider;
extern struct auth_provider blacklist_provider;

void load_provider(struct auth_provider *provider);
void unload_provider(struct auth_provider *provider);

void init_providers(void);
void destroy_providers(void);
void cancel_providers(struct auth_client *auth);

void provider_done(struct auth_client *auth, provider_t id);
void accept_client(struct auth_client *auth, provider_t id);
void reject_client(struct auth_client *auth, provider_t id, const char *reason);

void handle_new_connection(int parc, char *parv[]);

/* Provider is operating on this auth_client (set this if you have async work to do) */
static inline void set_provider_on(struct auth_client *auth, provider_t provider)
{
	auth->providers |= (1 << provider);
}

/* Provider is no longer operating on this auth client (you should use provider_done) */
static inline void set_provider_off(struct auth_client *auth, provider_t provider)
{
	auth->providers &= ~(1 << provider);
}

/* Set the provider to done (you should use provider_done) */
static inline void set_provider_done(struct auth_client *auth, provider_t provider)
{
	auth->providers_done |= (1 << provider);
}

/* Check if provider is operating on this auth client */
static inline bool is_provider_on(struct auth_client *auth, provider_t provider)
{
	return auth->providers & (1 << provider);
}

static inline bool is_provider_done(struct auth_client *auth, provider_t provider)
{
	return auth->providers_done & (1 << provider);
}

#endif /* __CHARYBDIS_AUTHD_PROVIDER_H__ */
