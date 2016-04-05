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
	PROVIDER_OPM,
} provider_t;

typedef enum
{
	PROVIDER_STATUS_NOTRUN = 0,
	PROVIDER_STATUS_RUNNING,
	PROVIDER_STATUS_DONE,
} provider_status_t;

struct auth_client_data
{
	time_t timeout;			/* Provider timeout */
	void *data;			/* Provider data */
	provider_status_t status;	/* Provider status */
};

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

	bool providers_starting;		/* Providers are still warming up */
	unsigned int refcount;			/* Held references */

	struct auth_client_data *data;		/* Provider-specific data */
};

typedef bool (*provider_init_t)(void);
typedef void (*provider_destroy_t)(void);

typedef bool (*provider_start_t)(struct auth_client *);
typedef void (*provider_cancel_t)(struct auth_client *);
typedef void (*provider_timeout_t)(struct auth_client *);
typedef void (*provider_complete_t)(struct auth_client *, provider_t);

struct auth_stats_handler
{
	const char letter;
	authd_stat_handler handler;
};

struct auth_provider
{
	rb_dlink_node node;

	provider_t id;

	provider_init_t init;		/* Initalise the provider */
	provider_destroy_t destroy;	/* Terminate the provider */

	provider_start_t start;		/* Perform authentication */
	provider_cancel_t cancel;	/* Authentication cancelled */
	provider_timeout_t timeout;	/* Timeout callback */
	provider_complete_t completed;	/* Callback for when other performers complete (think dependency chains) */

	struct auth_stats_handler stats_handler;

	struct auth_opts_handler *opt_handlers;
};

extern struct auth_provider rdns_provider;
extern struct auth_provider ident_provider;
extern struct auth_provider blacklist_provider;
extern struct auth_provider opm_provider;

extern rb_dlink_list auth_providers;
extern rb_dictionary *auth_clients;

void load_provider(struct auth_provider *provider);
void unload_provider(struct auth_provider *provider);

void init_providers(void);
void destroy_providers(void);
void cancel_providers(struct auth_client *auth);

void provider_done(struct auth_client *auth, provider_t id);
void accept_client(struct auth_client *auth, provider_t id);
void reject_client(struct auth_client *auth, provider_t id, const char *data, const char *fmt, ...);

void handle_new_connection(int parc, char *parv[]);
void handle_cancel_connection(int parc, char *parv[]);


/* Get a provider's raw status */
static inline provider_status_t
get_provider_status(struct auth_client *auth, provider_t provider)
{
	return auth->data[provider].status;
}

/* Set a provider's raw status */
static inline void
set_provider_status(struct auth_client *auth, provider_t provider, provider_status_t status)
{
	auth->data[provider].status = status;
}

/* Set the provider as running
 * If you're doing asynchronous work call this */
static inline void
set_provider_running(struct auth_client *auth, provider_t provider)
{
	auth->refcount++;
	set_provider_status(auth, provider, PROVIDER_STATUS_RUNNING);
}

/* Provider is no longer operating on this auth client
 * You should use provider_done and not this */
static inline void
set_provider_done(struct auth_client *auth, provider_t provider)
{
	auth->refcount--;
	set_provider_status(auth, provider, PROVIDER_STATUS_DONE);
}

/* Check if provider is operating on this auth client */
static inline bool
is_provider_running(struct auth_client *auth, provider_t provider)
{
	return get_provider_status(auth, provider) == PROVIDER_STATUS_RUNNING;
}

/* Check if provider has finished on this client */
static inline bool
is_provider_done(struct auth_client *auth, provider_t provider)
{
	return get_provider_status(auth, provider) == PROVIDER_STATUS_DONE;
}

/* Get provider auth client data */
static inline void *
get_provider_data(struct auth_client *auth, uint32_t id)
{
	lrb_assert(id < rb_dlink_list_length(&auth_providers));
	return auth->data[id].data;
}

/* Set provider auth client data */
static inline void
set_provider_data(struct auth_client *auth, uint32_t id, void *data)
{
	lrb_assert(id < rb_dlink_list_length(&auth_providers));
	auth->data[id].data = data;
}

/* Set timeout relative to current time on provider
 * When the timeout lapses, the provider's timeout call will execute */
static inline void
set_provider_timeout_relative(struct auth_client *auth, uint32_t id, time_t timeout)
{
	lrb_assert(id < rb_dlink_list_length(&auth_providers));
	auth->data[id].timeout = timeout + rb_current_time();
}

/* Set timeout value in absolute time (Unix timestamp)
 * When the timeout lapses, the provider's timeout call will execute */
static inline void
set_provider_timeout_absolute(struct auth_client *auth, uint32_t id, time_t timeout)
{
	lrb_assert(id < rb_dlink_list_length(&auth_providers));
	auth->data[id].timeout = timeout;
}

/* Get the timeout value for the provider */
static inline time_t
get_provider_timeout(struct auth_client *auth, uint32_t id)
{
	lrb_assert(id < rb_dlink_list_length(&auth_providers));
	return auth->data[id].timeout;
}

#endif /* __CHARYBDIS_AUTHD_PROVIDER_H__ */
