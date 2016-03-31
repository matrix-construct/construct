/* authd/providers/opm.c - small open proxy monitor
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

#include "rb_lib.h"
#include "stdinc.h"
#include "setup.h"
#include "authd.h"
#include "notice.h"
#include "provider.h"

#define OPM_READSIZE 128

typedef enum protocol_t
{
	PROTO_NONE,
	PROTO_SOCKS4,
	PROTO_SOCKS5,
} protocol_t;

struct opm_lookup
{
	rb_dlink_list scans;	/* List of scans */
};

struct opm_proxy
{
	const char *note;
	protocol_t proto;
	uint16_t port;
};

struct opm_scan
{
	struct auth_client *auth;

	struct opm_proxy *proxy;
	rb_fde_t *F;		/* fd for scan */

	rb_dlink_node node;
};

struct opm_listener
{
	char ip[HOSTIPLEN];
	uint16_t port;
	struct rb_sockaddr_storage addr;
	rb_fde_t *F;
};

/* Proxies that we scan for */
static struct opm_proxy opm_proxy_scans[] =
{
	{ "socks4-1080",	PROTO_SOCKS4,		1080  },
	{ "socks5-1080",	PROTO_SOCKS5,		1080  },
	{ "socks4-80",		PROTO_SOCKS4,		80    },
	{ "socks5-80",		PROTO_SOCKS5,		80    },
	{ "socks4-8080",	PROTO_SOCKS4,		8080  },
	{ "socks5-8080",	PROTO_SOCKS5,		8080  },
	{ NULL,			PROTO_NONE,		0     }
};


static ACCB accept_opm;
static PF read_opm_reply;

static CNCB socks4_connected;
static CNCB socks5_connected;

static void opm_cancel(struct auth_client *auth);

static int opm_timeout = 5;
static bool opm_enable = false;

#define LISTEN_IPV4 0
#define LISTEN_IPV6 1

/* IPv4 and IPv6 */
static struct opm_listener listeners[2];


static void
read_opm_reply(rb_fde_t *F, void *data)
{
	struct auth_client *auth = data;
	struct opm_lookup *lookup;
	char readbuf[OPM_READSIZE];
	ssize_t len;

	if(auth == NULL || (lookup = auth->data[PROVIDER_OPM]) == NULL)
	{
		rb_close(F);
		return;
	}

	if((len = rb_read(F, readbuf, sizeof(readbuf))) < 0 && rb_ignore_errno(errno))
	{
		rb_setselect(F, RB_SELECT_READ, read_opm_reply, auth);
		return;
	}
	else if(len <= 0)
	{
		/* Dead */
		rb_close(F);
		return;
	}

	for(struct opm_proxy *proxy = opm_proxy_scans; proxy->note != NULL; proxy++)
	{
		if(strncmp(proxy->note, readbuf, sizeof(readbuf)) != 0)
			/* Nope */
			continue;

		/* If we get here we have an open proxy */
		reject_client(auth, PROVIDER_OPM, readbuf, "Open proxy detected");
		opm_cancel(auth);
		break;
	}

	rb_close(F);
}

static void
accept_opm(rb_fde_t *F, int status, struct sockaddr *addr, rb_socklen_t len, void *data)
{
	struct auth_client *auth = NULL;
	struct opm_listener *listener = data;
	struct rb_sockaddr_storage localaddr;
	unsigned int llen = sizeof(struct rb_sockaddr_storage);
	rb_dictionary_iter iter;

	if(status != 0 || listener == NULL)
	{
		rb_close(F);
		return;
	}

	if(getsockname(rb_get_fd(F), (struct sockaddr *)&localaddr, &llen))
	{
		/* This can happen if the client goes away after accept */
		rb_close(F);
		return;
	}

	/* Correlate connection with client(s) */
	RB_DICTIONARY_FOREACH(auth, &iter, auth_clients)
	{
		if(GET_SS_FAMILY(&auth->c_addr) != GET_SS_FAMILY(&localaddr))
			continue;

		/* Compare the addresses */
		switch(GET_SS_FAMILY(&localaddr))
		{
		case AF_INET:
			{
				struct sockaddr_in *s = (struct sockaddr_in *)&localaddr, *c = (struct sockaddr_in *)&auth->c_addr;

				if(s->sin_addr.s_addr == c->sin_addr.s_addr)
				{
					/* Match... check if it's real */
					rb_setselect(F, RB_SELECT_READ, read_opm_reply, auth);
					return;
				}
				break;
			}
#ifdef RB_IPV6
		case AF_INET6:
			{
				struct sockaddr_in6 *s = (struct sockaddr_in6 *)&localaddr, *c = (struct sockaddr_in6 *)&auth->c_addr;

				if(memcmp(s->sin6_addr.s6_addr, c->sin6_addr.s6_addr, 16) == 0)
				{
					rb_setselect(F, RB_SELECT_READ, read_opm_reply, auth);
					return;
				}
				break;
			}
#endif
		default:
			warn_opers(L_CRIT, "OPM: unknown address type in listen function");
			exit(EX_PROVIDER_ERROR);
		}
	}

	/* We don't care about the socket if we get here */
	rb_close(F);
}

/* Scanners */

static void
socks4_connected(rb_fde_t *F, int error, void *data)
{
	struct opm_scan *scan = data;
	struct opm_lookup *lookup;
	struct auth_client *auth;
	uint8_t sendbuf[9]; /* Size we're building */
	uint8_t *c = sendbuf;
	ssize_t ret;

	if(scan == NULL || (auth = scan->auth) == NULL || (lookup = auth->data[PROVIDER_OPM]) == NULL)
		return;

	if(error || !opm_enable)
		goto end;

	memcpy(c, "\x04\x01", 2); c += 2; /* Socks version 4, connect command */

	switch(GET_SS_FAMILY(&auth->c_addr))
	{
	case AF_INET:
		if(listeners[LISTEN_IPV4].F == NULL)
			/* They cannot respond to us */
			goto end;

		memcpy(c, &(((struct sockaddr_in *)&listeners[LISTEN_IPV4].addr)->sin_port), 2); c += 2; /* Port */
		memcpy(c, &(((struct sockaddr_in *)&listeners[LISTEN_IPV4].addr)->sin_addr.s_addr), 4); c += 4; /* Address */
		break;
#ifdef RB_IPV6
	case AF_INET6:
	/* socks4 doesn't support IPv6 */
#endif
	default:
		goto end;
	}

	*c = '\x00'; /* No userid */

	/* Send header */
	if(rb_write(scan->F, sendbuf, sizeof(sendbuf)) < 0)
		goto end;

	/* Send note */
	if(rb_write(scan->F, scan->proxy->note, strlen(scan->proxy->note) + 1) < 0)
		goto end;

end:
	rb_close(scan->F);
	rb_dlinkDelete(&scan->node, &lookup->scans);
	rb_free(scan);
}

static void
socks5_connected(rb_fde_t *F, int error, void *data)
{
	struct opm_scan *scan = data;
	struct opm_lookup *lookup; 
	struct opm_listener *listener;
	struct auth_client *auth;
	uint8_t sendbuf[25]; /* Size we're building */
	uint8_t *c = sendbuf;
	ssize_t ret;

	if(scan == NULL || (auth = scan->auth) == NULL || (lookup = auth->data[PROVIDER_OPM]) == NULL)
		return;

	if(error || !opm_enable)
		goto end;

	/* Build the version header and socks request
	 * version header (3 bytes): version, number of auth methods, auth type (0 for none) 
	 * connect req (3 bytes): version, command (1 = connect), reserved (0)
	 */
	memcpy(c, "\x05\x01\x00\x05\x01\x00", 6); c += 6;

	switch(GET_SS_FAMILY(&auth->c_addr))
	{
	case AF_INET:
		listener = &listeners[LISTEN_IPV4];
		if(!listener->F)
			goto end;

		*(c++) = '\x01'; /* Address type (1 = IPv4) */
		memcpy(c, &(((struct sockaddr_in *)&listener->addr)->sin_addr.s_addr), 4); c += 4;/* Address */
		memcpy(c, &(((struct sockaddr_in *)&listener->addr)->sin_port), 2); c += 2; /* Port */
		break;
#ifdef RB_IPV6
	case AF_INET6:
		listener = &listeners[LISTEN_IPV6];
		if(!listener->F)
			goto end;

		*(c++) = '\x04'; /* Address type (4 = IPv6) */
		memcpy(c, ((struct sockaddr_in6 *)&listener->addr)->sin6_addr.s6_addr, 16); c += 16; /* Address */
		memcpy(c, &(((struct sockaddr_in6 *)&listener->addr)->sin6_port), 2); c += 2; /* Port */
		break;
#endif
	default:
		goto end;
	}

	/* Send header */
	if(rb_write(scan->F, sendbuf, (size_t)(sendbuf - c)) <= 0)
		goto end;

	/* Now the note in a separate write */
	if(rb_write(scan->F, scan->proxy->note, strlen(scan->proxy->note) + 1) <= 0)
		goto end;

end:
	rb_close(scan->F);
	rb_dlinkDelete(&scan->node, &lookup->scans);
	rb_free(scan);
}

/* Establish connections */
static inline void
establish_connection(struct auth_client *auth, struct opm_proxy *proxy)
{
	struct opm_lookup *lookup = auth->data[PROVIDER_OPM];
	struct opm_listener *listener;
	struct opm_scan *scan = rb_malloc(sizeof(struct opm_scan));
	struct rb_sockaddr_storage c_a, l_a;
	int opt = 1;
	CNCB *callback;

	switch(proxy->proto)
	{
	case PROTO_SOCKS4:
		callback = socks4_connected;
		break;
	case PROTO_SOCKS5:
		callback = socks5_connected;
		break;
	default:
		return;
	}

#ifdef RB_IPV6
	if(GET_SS_FAMILY(&auth->c_addr) == AF_INET6)
		listener = &listeners[LISTEN_IPV6];
	else
#endif
		listener = &listeners[LISTEN_IPV4];

	c_a = auth->c_addr;	/* Client */
	l_a = listener->addr;	/* Listener */

	scan->auth = auth;
	scan->proxy = proxy;
	if((scan->F = rb_socket(GET_SS_FAMILY(&auth->c_addr), SOCK_STREAM, 0, proxy->note)) == NULL)
	{
		warn_opers(L_CRIT, "OPM: could not create OPM socket (proto %s): %s", proxy->note, strerror(errno));
		rb_free(scan);
		return;
	}

	/* Disable Nagle's algorithim - buffering could affect scans */
	(void)setsockopt(rb_get_fd(scan->F), IPPROTO_TCP, TCP_NODELAY, (char *)&opt, sizeof(opt));

        /* Set the ports correctly */
#ifdef RB_IPV6
	if(GET_SS_FAMILY(&l_a) == AF_INET6)
		((struct sockaddr_in6 *)&l_a)->sin6_port = 0;
	else
#endif
		((struct sockaddr_in *)&l_a)->sin_port = 0;

#ifdef RB_IPV6
	if(GET_SS_FAMILY(&c_a) == AF_INET6)
		((struct sockaddr_in6 *)&c_a)->sin6_port = ((struct sockaddr_in6 *)&listener->addr)->sin6_port;
	else
#endif
		((struct sockaddr_in *)&c_a)->sin_port = ((struct sockaddr_in *)&listener->addr)->sin_port;

	rb_dlinkAdd(scan, &scan->node, &lookup->scans);
	rb_connect_tcp(scan->F,
			(struct sockaddr *)&c_a,
			(struct sockaddr *)&l_a,
			GET_SS_LEN(&l_a),
			callback, scan, opm_timeout);
}

static void
opm_destroy(void)
{
	struct auth_client *auth;
	rb_dictionary_iter iter;

	/* Nuke all opm lookups */
	RB_DICTIONARY_FOREACH(auth, &iter, auth_clients)
	{
		opm_cancel(auth);
	}
}

static bool
opm_start(struct auth_client *auth)
{
	struct opm_lookup *lookup = rb_malloc(sizeof(struct opm_lookup));

	if(!opm_enable)
	{
		notice_client(auth->cid, "*** Proxy scanning disabled, not scanning");
		return true;
	}

	auth->data[PROVIDER_OPM] = lookup = rb_malloc(sizeof(struct opm_lookup));
	auth->timeout[PROVIDER_OPM] = rb_current_time() + opm_timeout;

	for(struct opm_proxy *proxy = opm_proxy_scans; proxy->note != NULL; proxy++)
		establish_connection(auth, proxy);

	notice_client(auth->cid, "*** Scanning for open proxies...");
	set_provider_on(auth, PROVIDER_OPM);

	return true;
}

static void
opm_cancel(struct auth_client *auth)
{
	struct opm_lookup *lookup = auth->data[PROVIDER_OPM];

	if(lookup != NULL)
	{
		rb_dlink_node *ptr, *nptr;

		RB_DLINK_FOREACH_SAFE(ptr, nptr, lookup->scans.head)
		{
			struct opm_scan *scan = ptr->data;

			rb_close(scan->F);
			rb_free(scan);
		}

		rb_free(lookup);
		provider_done(auth, PROVIDER_OPM);
	}
}

static void
add_conf_opm_timeout(const char *key __unused, int parc __unused, const char **parv)
{
	int timeout = atoi(parv[0]);

	if(timeout < 0)
	{
		warn_opers(L_CRIT, "opm: opm timeout < 0 (value: %d)", timeout);
		return;
	}

	opm_timeout = timeout;
}

static void
set_opm_enabled(const char *key __unused, int parc __unused, const char **parv)
{
	if(listeners[LISTEN_IPV4].F != NULL || listeners[LISTEN_IPV6].F != NULL)
		opm_enable = (*parv[0] == '1');
	else
		/* No listener, no point... */
		opm_enable = false;
}

static void
set_opm_listener(const char *key __unused, int parc __unused, const char **parv)
{
	struct auth_client *auth;
	struct rb_sockaddr_storage addr;
	struct opm_listener *listener;
	int port = atoi(parv[1]), opt = 1;
	rb_dictionary_iter iter;
	rb_fde_t *F;
	size_t i;

	if(port > 65535 || port <= 0 || !rb_inet_pton_sock(parv[0], (struct sockaddr *)&addr))
	{
		warn_opers(L_CRIT, "OPM: got a bad listener: %s:%s", parv[0], parv[1]);
		exit(EX_PROVIDER_ERROR);
	}

#ifdef RB_IPV6
	if(GET_SS_FAMILY(&addr) == AF_INET6)
		listener = &listeners[LISTEN_IPV6];
	else
#endif
		listener = &listeners[LISTEN_IPV4];

	if(strcmp(listener->ip, parv[0]) == 0 || listener->port == port)
		return;

	if((F = rb_socket(GET_SS_FAMILY(&addr), SOCK_STREAM, 0, "OPM listener socket")) == NULL)
	{
		/* This shouldn't fail, or we have problems... */
		warn_opers(L_CRIT, "OPM: cannot create socket: %s", strerror(errno));
		exit(EX_PROVIDER_ERROR);
	}

	if(setsockopt(rb_get_fd(F), SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)))
	{
		/* This shouldn't fail either... */
		warn_opers(L_CRIT, "OPM: cannot set options on socket: %s", strerror(errno));
		exit(EX_PROVIDER_ERROR);
	}

	/* Set up ports for binding */
#ifdef RB_IPV6
	if(GET_SS_FAMILY(&addr) == AF_INET6)
		((struct sockaddr_in6 *)&addr)->sin6_port = htons(port);
	else
#endif
		((struct sockaddr_in *)&addr)->sin_port = htons(port);

	if(bind(rb_get_fd(F), (struct sockaddr *)&addr, GET_SS_LEN(&addr)))
	{
		/* Shit happens, let's not cripple authd over this */
		warn_opers(L_WARN, "OPM: cannot bind on socket: %s", strerror(errno));
		rb_close(F);
		return;
	}

	if(rb_listen(F, SOMAXCONN, false)) /* deferred accept could interfere with detection */
	{
		warn_opers(L_WARN, "OPM: cannot listen on socket: %s", strerror(errno));
		rb_close(F);
		return;
	}

	/* From this point forward we assume we have a listener */

	if(listener->F != NULL)
		/* Close old listener */
		rb_close(listener->F);

	listener->F = F;

	/* Cancel old clients that may be on old stale listener
	 * XXX - should rescan clients that need it
	 */
	RB_DICTIONARY_FOREACH(auth, &iter, auth_clients)
	{
		opm_cancel(auth);
	}

	/* Copy data */
	rb_strlcpy(listener->ip, parv[0], sizeof(listener->ip));
	listener->port = (uint16_t)port;
	listener->addr = addr;

	opm_enable = true; /* Implicitly set this to true for now if we have a listener */
	rb_accept_tcp(listener->F, NULL, accept_opm, listener);
}

struct auth_opts_handler opm_options[] =
{
	{ "opm_timeout", 1, add_conf_opm_timeout },
	{ "opm_enabled", 1, set_opm_enabled },
	{ "opm_listener", 2, set_opm_listener },
	{ NULL, 0, NULL },
};

struct auth_provider opm_provider =
{
	.id = PROVIDER_OPM,
	.destroy = opm_destroy,
	.start = opm_start,
	.cancel = opm_cancel,
	.timeout = opm_cancel,
	.opt_handlers = opm_options,
};
