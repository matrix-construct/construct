/*
 *  ircd-ratbox: A slightly useful ircd.
 *  listener.c: Listens on a port.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2005 ircd-ratbox development team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 *
 *  $Id: listener.c 25169 2008-03-29 21:41:31Z jilles $
 */

#include "stdinc.h"
#include "ratbox_lib.h"
#include "listener.h"
#include "client.h"
#include "ircd.h"
#include "numeric.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "s_stats.h"
#include "send.h"
#include "s_auth.h"
#include "reject.h"
#include "s_log.h"
#include "hash.h"

static rb_dlink_list listener_list;
static int accept_precallback(rb_fde_t *F, struct sockaddr *addr, rb_socklen_t addrlen, void *data);
static void accept_callback(rb_fde_t *F, int status, struct sockaddr *addr, rb_socklen_t addrlen, void *data);



static struct Listener *
make_listener(struct rb_sockaddr_storage *addr)
{
	struct Listener *listener = rb_malloc(sizeof(struct Listener));
	s_assert(0 != listener);
	listener->name = ServerInfo.name; /* me.name may not be valid yet -- jilles */
	listener->F = NULL;
	memcpy(&listener->addr, addr, sizeof(struct rb_sockaddr_storage));
	return listener;
}

void
free_listener(struct Listener *listener)
{
	s_assert(NULL != listener);
	if(listener == NULL)
		return;
	
	rb_dlinkDelete(&listener->node, &listener_list);
	rb_free(listener);
}

#define PORTNAMELEN 6		/* ":31337" */

/*
 * get_listener_name - return displayable listener name and port
 * returns "host.foo.org:6667" for a given listener
 */
const char *
get_listener_name(struct Listener *listener)
{
	static char buf[HOSTLEN + HOSTLEN + PORTNAMELEN + 4];
	int port = 0;

	s_assert(NULL != listener);
	if(listener == NULL)
		return NULL;

#ifdef IPV6
	if(GET_SS_FAMILY(&listener->addr) == AF_INET6)
		port = ntohs(((const struct sockaddr_in6 *)&listener->addr)->sin6_port);
	else
#endif
		port = ntohs(((const struct sockaddr_in *)&listener->addr)->sin_port);	

	rb_snprintf(buf, sizeof(buf), "%s[%s/%u]", me.name, listener->name, port);
	return buf;
}

/*
 * show_ports - send port listing to a client
 * inputs       - pointer to client to show ports to
 * output       - none
 * side effects - show ports
 */
void
show_ports(struct Client *source_p)
{
	struct Listener *listener;
	rb_dlink_node *ptr;
	
	RB_DLINK_FOREACH(ptr, listener_list.head)
	{
		listener = ptr->data;
		sendto_one_numeric(source_p, RPL_STATSPLINE, 
				   form_str(RPL_STATSPLINE), 'P',
#ifdef IPV6
			   ntohs(GET_SS_FAMILY(&listener->addr) == AF_INET ? ((struct sockaddr_in *)&listener->addr)->sin_port :
				 ((struct sockaddr_in6 *)&listener->addr)->sin6_port),
#else
			   ntohs(((struct sockaddr_in *)&listener->addr)->sin_port),
#endif
			   IsOperAdmin(source_p) ? listener->name : me.name,
			   listener->ref_count, (listener->active) ? "active" : "disabled",
			   listener->ssl ? " ssl" : "");
	}
}

/*
 * inetport - create a listener socket in the AF_INET or AF_INET6 domain,
 * bind it to the port given in 'port' and listen to it
 * returns true (1) if successful false (0) on error.
 *
 * If the operating system has a define for SOMAXCONN, use it, otherwise
 * use RATBOX_SOMAXCONN
 */
#ifdef SOMAXCONN
#undef RATBOX_SOMAXCONN
#define RATBOX_SOMAXCONN SOMAXCONN
#endif

static int
inetport(struct Listener *listener)
{
	rb_fde_t *F;
	int ret;
	int opt = 1;

	/*
	 * At first, open a new socket
	 */
	
	F = rb_socket(GET_SS_FAMILY(&listener->addr), SOCK_STREAM, 0, "Listener socket");

#ifdef IPV6
	if(GET_SS_FAMILY(&listener->addr) == AF_INET6)
	{
		struct sockaddr_in6 *in6 = (struct sockaddr_in6 *)&listener->addr;
		if(!IN6_ARE_ADDR_EQUAL(&in6->sin6_addr, &in6addr_any))
		{
			rb_inet_ntop(AF_INET6, &in6->sin6_addr, listener->vhost, sizeof(listener->vhost));
			listener->name = listener->vhost;
		}
	} else
#endif
	{
		struct sockaddr_in *in = (struct sockaddr_in *)&listener->addr;
		if(in->sin_addr.s_addr != INADDR_ANY)
		{
			rb_inet_ntop(AF_INET, &in->sin_addr, listener->vhost, sizeof(listener->vhost));
			listener->name = listener->vhost;
		}	
	}


	if(F == NULL)
	{
		report_error("opening listener socket %s:%s",
			     get_listener_name(listener), 
			     get_listener_name(listener), errno);
		return 0;
	}
	else if((maxconnections - 10) < rb_get_fd(F)) /* XXX this is kinda bogus*/
	{
		report_error("no more connections left for listener %s:%s",
			     get_listener_name(listener), 
			     get_listener_name(listener), errno);
		rb_close(F);
		return 0;
	}
	/*
	 * XXX - we don't want to do all this crap for a listener
	 * set_sock_opts(listener);
	 */
	if(setsockopt(rb_get_fd(F), SOL_SOCKET, SO_REUSEADDR, (char *) &opt, sizeof(opt)))
	{
		report_error("setting SO_REUSEADDR for listener %s:%s",
			     get_listener_name(listener), 
			     get_listener_name(listener), errno);
		rb_close(F);
		return 0;
	}

	/*
	 * Bind a port to listen for new connections if port is non-null,
	 * else assume it is already open and try get something from it.
	 */

	if(bind(rb_get_fd(F), (struct sockaddr *) &listener->addr, GET_SS_LEN(&listener->addr)))
	{
		report_error("binding listener socket %s:%s",
			     get_listener_name(listener), 
			     get_listener_name(listener), errno);
		rb_close(F);
		return 0;
	}

	if((ret = rb_listen(F, RATBOX_SOMAXCONN)))
	{
		report_error("listen failed for %s:%s", 
			     get_listener_name(listener), 
			     get_listener_name(listener), errno);
		rb_close(F);
		return 0;
	}

	listener->F = F;

	rb_accept_tcp(listener->F, accept_precallback, accept_callback, listener);
	return 1;
}

static struct Listener *
find_listener(struct rb_sockaddr_storage *addr)
{
	struct Listener *listener = NULL;
	struct Listener *last_closed = NULL;
	rb_dlink_node *ptr;

	RB_DLINK_FOREACH(ptr, listener_list.head)
	{
		listener = ptr->data;
		if(GET_SS_FAMILY(addr) != GET_SS_FAMILY(&listener->addr))
			continue;
		
		switch(GET_SS_FAMILY(addr))
		{
			case AF_INET:
			{
				struct sockaddr_in *in4 = (struct sockaddr_in *)addr;
				struct sockaddr_in *lin4 = (struct sockaddr_in *)&listener->addr;
				if(in4->sin_addr.s_addr == lin4->sin_addr.s_addr && 
					in4->sin_port == lin4->sin_port )
				{
					if(listener->F == NULL)
						last_closed = listener;
					else
						return(listener);
				}
				break;
			}
#ifdef IPV6
			case AF_INET6:
			{
				struct sockaddr_in6 *in6 = (struct sockaddr_in6 *)addr;
				struct sockaddr_in6 *lin6 =(struct sockaddr_in6 *)&listener->addr;
				if(IN6_ARE_ADDR_EQUAL(&in6->sin6_addr, &lin6->sin6_addr) &&
				  in6->sin6_port == lin6->sin6_port)
				{
					if(listener->F == NULL)
						last_closed = listener;
					else
						return(listener);
				}
				break;
				
			}
#endif

			default:
				break;
		}
	}
	return last_closed;
}

/*
 * add_listener- create a new listener
 * port - the port number to listen on
 * vhost_ip - if non-null must contain a valid IP address string in
 * the format "255.255.255.255"
 */
void
add_listener(int port, const char *vhost_ip, int family, int ssl)
{
	struct Listener *listener;
	struct rb_sockaddr_storage vaddr;

	/*
	 * if no port in conf line, don't bother
	 */
	if(port == 0)
		return;
		
	memset(&vaddr, 0, sizeof(vaddr));
	GET_SS_FAMILY(&vaddr) = family;

	if(vhost_ip != NULL)
	{
		if(rb_inet_pton_sock(vhost_ip, (struct sockaddr *)&vaddr) <= 0)
			return;
	} else
	{
		switch(family)
		{
			case AF_INET:
				((struct sockaddr_in *)&vaddr)->sin_addr.s_addr = INADDR_ANY;
				break;
#ifdef IPV6
			case AF_INET6:
				memcpy(&((struct sockaddr_in6 *)&vaddr)->sin6_addr, &in6addr_any, sizeof(struct in6_addr));
				break;
#endif
			default:
				return;
		} 
	}
	switch(family)
	{
		case AF_INET:
			SET_SS_LEN(&vaddr, sizeof(struct sockaddr_in));
			((struct sockaddr_in *)&vaddr)->sin_port = htons(port);
			break;
#ifdef IPV6
		case AF_INET6:
			SET_SS_LEN(&vaddr, sizeof(struct sockaddr_in6));
			((struct sockaddr_in6 *)&vaddr)->sin6_port = htons(port);
			break;
#endif
		default:
			break;
	}
	if((listener = find_listener(&vaddr)))
	{
		if(listener->F != NULL)
			return;
	}
	else
	{
		listener = make_listener(&vaddr);
		rb_dlinkAdd(listener, &listener->node, &listener_list);
	}

	listener->F = NULL;
	listener->ssl = ssl;
	if(inetport(listener))
		listener->active = 1;
	else
		close_listener(listener);
}

/*
 * close_listener - close a single listener
 */
void
close_listener(struct Listener *listener)
{
	s_assert(listener != NULL);
	if(listener == NULL)
		return;
	if(listener->F != NULL)
	{
		rb_close(listener->F);
		listener->F = NULL;
	}

	listener->active = 0;

	if(listener->ref_count)
		return;

	free_listener(listener);
}

/*
 * close_listeners - close and free all listeners that are not being used
 */
void
close_listeners()
{
	struct Listener *listener;
	rb_dlink_node *ptr, *next;

	RB_DLINK_FOREACH_SAFE(ptr, next, listener_list.head)
	{
		listener = ptr->data;
		close_listener(listener);
	}
}

/*
 * add_connection - creates a client which has just connected to us on 
 * the given fd. The sockhost field is initialized with the ip# of the host.
 * The client is sent to the auth module for verification, and not put in
 * any client list yet.
 */
static void
add_connection(struct Listener *listener, rb_fde_t *F, struct sockaddr *sai, struct sockaddr *lai, void *ssl_ctl)
{
	struct Client *new_client;
	s_assert(NULL != listener);
	/* 
	 * get the client socket name from the socket
	 * the client has already been checked out in accept_connection
	 */
	new_client = make_client(NULL);

	memcpy(&new_client->localClient->ip, sai, sizeof(struct rb_sockaddr_storage));
	new_client->localClient->lip = rb_malloc(sizeof(struct rb_sockaddr_storage));
	memcpy(new_client->localClient->lip, lai, sizeof(struct rb_sockaddr_storage));

	/* 
	 * copy address to 'sockhost' as a string, copy it to host too
	 * so we have something valid to put into error messages...
	 */
	rb_inet_ntop_sock((struct sockaddr *)&new_client->localClient->ip, new_client->sockhost, 
		sizeof(new_client->sockhost));


	rb_strlcpy(new_client->host, new_client->sockhost, sizeof(new_client->host));

#ifdef IPV6
	if(GET_SS_FAMILY(&new_client->localClient->ip) == AF_INET6 && ConfigFileEntry.dot_in_ip6_addr == 1)
	{
		rb_strlcat(new_client->host, ".", sizeof(new_client->host));
	}
#endif

	new_client->localClient->F = F;
	add_to_cli_fd_hash(new_client);
	new_client->localClient->listener = listener;
	new_client->localClient->ssl_ctl = ssl_ctl;
	if(ssl_ctl != NULL || rb_fd_ssl(F))
		SetSSL(new_client);

	++listener->ref_count;

	start_auth(new_client);
}

static time_t last_oper_notice = 0;

static const char *toofast = "ERROR :Reconnecting too fast, throttled.\r\n";

static int
accept_precallback(rb_fde_t *F, struct sockaddr *addr, rb_socklen_t addrlen, void *data)
{
	struct Listener *listener = (struct Listener *)data;
	char buf[BUFSIZE];
	struct ConfItem *aconf;

	if(listener->ssl && (!ssl_ok || !get_ssld_count()))
	{
		fprintf(stderr, "closed socket\n");
		rb_close(F);
		return 0;
	}
	
	if((maxconnections - 10) < rb_get_fd(F)) /* XXX this is kinda bogus */
	{
		++ServerStats.is_ref;
		/*
		 * slow down the whining to opers bit
		 */
		if((last_oper_notice + 20) <= rb_current_time())
		{
			sendto_realops_flags(UMODE_ALL, L_ALL,
					     "All connections in use. (%s)",
					     get_listener_name(listener));
			last_oper_notice = rb_current_time();
		}
			
		rb_write(F, "ERROR :All connections in use\r\n", 32);
		rb_close(F);
		/* Re-register a new IO request for the next accept .. */
		return 0;
	}

	aconf = find_dline(addr);
	if(aconf != NULL && (aconf->status & CONF_EXEMPTDLINE))
		return 1;
	
	/* Do an initial check we aren't connecting too fast or with too many
	 * from this IP... */
	if(aconf != NULL)
	{
		ServerStats.is_ref++;
			
		if(ConfigFileEntry.dline_with_reason)
		{
			if (rb_snprintf(buf, sizeof(buf), "ERROR :*** Banned: %s\r\n", aconf->passwd) >= (int)(sizeof(buf)-1))
			{
				buf[sizeof(buf) - 3] = '\r';
				buf[sizeof(buf) - 2] = '\n';
				buf[sizeof(buf) - 1] = '\0';
			}
		}
		else
			strcpy(buf, "ERROR :You have been D-lined.\r\n");
	
		rb_write(F, buf, strlen(buf));
		rb_close(F);
		return 0;
	}

	if(check_reject(F, addr))
		return 0;
		
	if(throttle_add(addr))
	{
		rb_write(F, toofast, strlen(toofast));
		rb_close(F);
		return 0;
	}

	return 1;
}

static void
accept_ssld(rb_fde_t *F, struct sockaddr *addr, struct sockaddr *laddr, struct Listener *listener)
{
	ssl_ctl_t *ctl;
	rb_fde_t *xF[2];
	rb_socketpair(AF_UNIX, SOCK_STREAM, 0, &xF[0], &xF[1], "Incoming ssld Connection");
	ctl = start_ssld_accept(F, xF[1], rb_get_fd(xF[0])); /* this will close F for us */
	add_connection(listener, xF[0], addr, laddr, ctl);
}

static void
accept_callback(rb_fde_t *F, int status, struct sockaddr *addr, rb_socklen_t addrlen, void *data)
{
	struct Listener *listener = data;
	struct rb_sockaddr_storage lip;
	unsigned int locallen = sizeof(struct rb_sockaddr_storage);
	
	ServerStats.is_ac++;
	if(getsockname(rb_get_fd(F), (struct sockaddr *) &lip, &locallen) < 0)
	{
		/* this shouldn't fail so... */
		/* XXX add logging of this */
		rb_close(F);
	}
	if(listener->ssl)
		accept_ssld(F, addr, (struct sockaddr *)&lip, listener);
	else
		add_connection(listener, F, addr, (struct sockaddr *)&lip, NULL);
}
