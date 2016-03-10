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

/* Arbitrary limit */
#define MAX_CLIENTS 4096

/* Registered providers */
typedef enum
{
	PROVIDER_NULL = 0x0, /* Dummy value */
	PROVIDER_RDNS = 0x1,
	PROVIDER_IDENT = 0x2,
	PROVIDER_BLACKLIST = 0x4,
} provider_t;

struct auth_client
{
	uint16_t cid;				/* Client ID */

	struct rb_sockaddr_storage l_addr;	/* Listener IP address */
	struct rb_sockaddr_storage c_addr;	/* Client IP address */

	char hostname[HOSTLEN + 1];		/* Used for DNS lookup */
	char username[USERLEN + 1];		/* Used for ident lookup */

	unsigned int providers;			/* Providers at work,
						 * none left when set to 0 */
};

typedef bool (*provider_init_t)(void);
typedef bool (*provider_perform_t)(struct auth_client *);
typedef void (*provider_complete_t)(struct auth_client *, provider_t provider);
typedef void (*provider_cancel_t)(struct auth_client *);
typedef void (*provider_destroy_t)(void);

struct auth_provider
{
	rb_dlink_node node;

	provider_t id;

	provider_init_t init;		/* Initalise the provider */
	provider_destroy_t destroy;	/* Terminate the provider */

	provider_perform_t start;	/* Perform authentication */
	provider_cancel_t cancel;	/* Authentication cancelled */
	provider_complete_t completed;	/* Callback for when other performers complete (think dependency chains) */
};

extern rb_dlink_list auth_providers;
extern struct auth_provider rdns_provider;

extern struct auth_client auth_clients[MAX_CLIENTS];

void load_provider(struct auth_provider *provider);
void unload_provider(struct auth_provider *provider);

void init_providers(void);
void destroy_providers(void);
void cancel_providers(struct auth_client *auth);

void provider_done(struct auth_client *auth, provider_t provider);
void reject_client(struct auth_client *auth, provider_t provider, const char *reason);
void accept_client(struct auth_client *auth, provider_t provider);

void notice_client(struct auth_client *auth, const char *notice);

void handle_new_connection(int parc, char *parv[]);

/* Provider is operating on this auth_client (set this if you have async work to do) */
static inline void set_provider(struct auth_client *auth, provider_t provider)
{
	auth->providers |= provider;
}

/* Provider is no longer operating on this auth client (you should use provider_done) */
static inline void unset_provider(struct auth_client *auth, provider_t provider)
{
	auth->providers &= ~provider;
}

/* Check if provider is operating on this auth client */
static inline bool is_provider(struct auth_client *auth, provider_t provider)
{
	return auth->providers & provider;
}

#endif /* __CHARYBDIS_AUTHD_PROVIDER_H__ */
