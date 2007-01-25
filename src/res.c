/*
 * A rewrite of Darren Reeds original res.c As there is nothing
 * left of Darrens original code, this is now licensed by the hybrid group.
 * (Well, some of the function names are the same, and bits of the structs..)
 * You can use it where it is useful, free even. Buy us a beer and stuff.
 *
 * The authors takes no responsibility for any damage or loss
 * of property which results from the use of this software.
 *
 * $Id: res.c 2023 2006-09-02 23:47:27Z jilles $
 * from Hybrid Id: res.c 459 2006-02-12 22:21:37Z db $
 *
 * July 1999 - Rewrote a bunch of stuff here. Change hostent builder code,
 *     added callbacks and reference counting of returned hostents.
 *     --Bleep (Thomas Helvey <tomh@inxpress.net>)
 *
 * This was all needlessly complicated for irc. Simplified. No more hostent
 * All we really care about is the IP -> hostname mappings. Thats all. 
 *
 * Apr 28, 2003 --cryogen and Dianora
 *
 * DNS server flooding lessened, AAAA-or-A lookup removed, ip6.int support
 * removed, various robustness fixes
 *
 * 2006 --jilles and nenolod
 */

#include "stdinc.h"
#include "ircd_defs.h"
#include "common.h"
#include "ircd.h"
#include "commio.h"
#include "res.h"
#include "reslib.h"
#include "tools.h"
#include "event.h"
#include "irc_string.h"
#include "sprintf_irc.h"
#include "numeric.h"
#include "client.h" /* SNO_* */

#if (CHAR_BIT != 8)
#error this code needs to be able to address individual octets
#endif

static PF res_readreply;

#define MAXPACKET      1024	/* rfc sez 512 but we expand names so ... */
#define RES_MAXALIASES 35	/* maximum aliases allowed */
#define RES_MAXADDRS   35	/* maximum addresses allowed */
#define AR_TTL         600	/* TTL in seconds for dns cache entries */

/* RFC 1104/1105 wasn't very helpful about what these fields
 * should be named, so for now, we'll just name them this way.
 * we probably should look at what named calls them or something.
 */
#define TYPE_SIZE         (size_t)2
#define CLASS_SIZE        (size_t)2
#define TTL_SIZE          (size_t)4
#define RDLENGTH_SIZE     (size_t)2
#define ANSWER_FIXED_SIZE (TYPE_SIZE + CLASS_SIZE + TTL_SIZE + RDLENGTH_SIZE)

typedef enum
{
	REQ_IDLE,		/* We're doing not much at all */
	REQ_PTR,		/* Looking up a PTR */
	REQ_A,			/* Looking up an A or AAAA */
	REQ_CNAME		/* We got a CNAME in response, we better get a real answer next */
} request_state;

struct reslist
{
	dlink_node node;
	int id;
	int sent;		/* number of requests sent */
	request_state state;	/* State the resolver machine is in */
	time_t ttl;
	char type;
	char queryname[128];	/* name currently being queried */
	char retries;		/* retry counter */
	char sends;		/* number of sends (>1 means resent) */
	char resend;		/* send flag. 0 == dont resend */
	time_t sentat;
	time_t timeout;
	struct irc_sockaddr_storage addr;
	char *name;
	struct DNSQuery *query;	/* query callback for this request */
};

static int res_fd;
static dlink_list request_list = { NULL, NULL, 0 };

static void rem_request(struct reslist *request);
static struct reslist *make_request(struct DNSQuery *query);
static void do_query_name(struct DNSQuery *query, const char *name, struct reslist *request, int);
static void do_query_number(struct DNSQuery *query, const struct irc_sockaddr_storage *,
			    struct reslist *request);
static void query_name(struct reslist *request);
static int send_res_msg(const char *buf, int len, int count);
static void resend_query(struct reslist *request);
static int check_question(struct reslist *request, HEADER * header, char *buf, char *eob);
static int proc_answer(struct reslist *request, HEADER * header, char *, char *);
static struct reslist *find_id(int id);
static struct DNSReply *make_dnsreply(struct reslist *request);

extern struct irc_sockaddr_storage irc_nsaddr_list[IRCD_MAXNS];
extern int irc_nscount;
extern char irc_domain[HOSTLEN + 1];


/*
 * int
 * res_ourserver(inp)
 *      looks up "inp" in irc_nsaddr_list[]
 * returns:
 *      0  : not found
 *      >0 : found
 * author:
 *      paul vixie, 29may94
 *      revised for ircd, cryogen(stu) may03
 */
static int res_ourserver(const struct irc_sockaddr_storage *inp)
{
#ifdef IPV6
	struct sockaddr_in6 *v6;
	struct sockaddr_in6 *v6in = (struct sockaddr_in6 *)inp;
#endif
	struct sockaddr_in *v4;
	struct sockaddr_in *v4in = (struct sockaddr_in *)inp;
	int ns;

	for (ns = 0; ns < irc_nscount; ns++)
	{
		const struct irc_sockaddr_storage *srv = &irc_nsaddr_list[ns];
#ifdef IPV6
		v6 = (struct sockaddr_in6 *)srv;
#endif
		v4 = (struct sockaddr_in *)srv;

		/* could probably just memcmp(srv, inp, srv.ss_len) here
		 * but we'll air on the side of caution - stu
		 */
		switch (srv->ss_family)
		{
#ifdef IPV6
		  case AF_INET6:
			  if (srv->ss_family == inp->ss_family)
				  if (v6->sin6_port == v6in->sin6_port)
					  if ((memcmp(&v6->sin6_addr.s6_addr, &v6in->sin6_addr.s6_addr,
						sizeof(struct in6_addr)) == 0) ||
					      (memcmp(&v6->sin6_addr.s6_addr, &in6addr_any,
						sizeof(struct in6_addr)) == 0))
						  return 1;
			  break;
#endif
		  case AF_INET:
			  if (srv->ss_family == inp->ss_family)
				  if (v4->sin_port == v4in->sin_port)
					  if ((v4->sin_addr.s_addr == INADDR_ANY)
					      || (v4->sin_addr.s_addr == v4in->sin_addr.s_addr))
						  return 1;
			  break;
		  default:
			  break;
		}
	}

	return 0;
}

/*
 * timeout_query_list - Remove queries from the list which have been 
 * there too long without being resolved.
 */
static time_t timeout_query_list(time_t now)
{
	dlink_node *ptr;
	dlink_node *next_ptr;
	struct reslist *request;
	time_t next_time = 0;
	time_t timeout = 0;

	DLINK_FOREACH_SAFE(ptr, next_ptr, request_list.head)
	{
		request = ptr->data;
		timeout = request->sentat + request->timeout;

		if (now >= timeout)
		{
			if (--request->retries <= 0)
			{
				(*request->query->callback) (request->query->ptr, NULL);
				rem_request(request);
				continue;
			}
			else
			{
				request->sentat = now;
				request->timeout += request->timeout;
				resend_query(request);
			}
		}

		if ((next_time == 0) || timeout < next_time)
		{
			next_time = timeout;
		}
	}

	return (next_time > now) ? next_time : (now + AR_TTL);
}

/*
 * timeout_resolver - check request list
 */
static void timeout_resolver(void *notused)
{
	timeout_query_list(CurrentTime);
}

/*
 * start_resolver - do everything we need to read the resolv.conf file
 * and initialize the resolver file descriptor if needed
 */
static void start_resolver(void)
{
	irc_res_init();

	if (res_fd <= 0)	/* there isn't any such thing as fd 0, that's just a myth. */
	{
		if ((res_fd = comm_socket(irc_nsaddr_list[0].ss_family, SOCK_DGRAM, 0,
			       "UDP resolver socket")) == -1)
			return;

		/* At the moment, the resolver FD data is global .. */
		comm_setselect(res_fd, FDLIST_NONE, COMM_SELECT_READ, res_readreply, NULL, 0);
		eventAdd("timeout_resolver", timeout_resolver, NULL, 1);
	}
}

/*
 * init_resolver - initialize resolver and resolver library
 */
void init_resolver(void)
{
#ifdef HAVE_SRAND48
	srand48(CurrentTime);
#endif
	start_resolver();
}

/*
 * restart_resolver - reread resolv.conf, reopen socket
 */
void restart_resolver(void)
{
	comm_close(res_fd);
	res_fd = -1;
	eventDelete(timeout_resolver, NULL);	/* -ddosen */
	start_resolver();
}

/*
 * add_local_domain - Add the domain to hostname, if it is missing
 * (as suggested by eps@TOASTER.SFSU.EDU)
 */
void add_local_domain(char *hname, size_t size)
{
	/* try to fix up unqualified names */
	if (strchr(hname, '.') == NULL)
	{
		if (irc_domain[0])
		{
			size_t len = strlen(hname);

			if ((strlen(irc_domain) + len + 2) < size)
			{
				hname[len++] = '.';
				strcpy(hname + len, irc_domain);
			}
		}
	}
}

/*
 * rem_request - remove a request from the list. 
 * This must also free any memory that has been allocated for 
 * temporary storage of DNS results.
 */
static void rem_request(struct reslist *request)
{
	dlinkDelete(&request->node, &request_list);
	MyFree(request->name);
	MyFree(request);
}

/*
 * make_request - Create a DNS request record for the server.
 */
static struct reslist *make_request(struct DNSQuery *query)
{
	struct reslist *request = MyMalloc(sizeof(struct reslist));

	request->sentat = CurrentTime;
	request->retries = 3;
	request->resend = 1;
	request->timeout = 4;	/* start at 4 and exponential inc. */
	request->query = query;
	request->state = REQ_IDLE;

	dlinkAdd(request, &request->node, &request_list);

	return request;
}

/*
 * delete_resolver_queries - cleanup outstanding queries 
 * for which there no longer exist clients or conf lines.
 */
void delete_resolver_queries(const struct DNSQuery *query)
{
	dlink_node *ptr;
	dlink_node *next_ptr;
	struct reslist *request;

	DLINK_FOREACH_SAFE(ptr, next_ptr, request_list.head)
	{
		if ((request = ptr->data) != NULL)
		{
			if (query == request->query)
				rem_request(request);
		}
	}
}

/*
 * send_res_msg - sends msg to all nameservers found in the "_res" structure.
 * This should reflect /etc/resolv.conf. We will get responses
 * which arent needed but is easier than checking to see if nameserver
 * isnt present. Returns number of messages successfully sent to 
 * nameservers or -1 if no successful sends.
 */
static int send_res_msg(const char *msg, int len, int rcount)
{
	int i;
	int sent = 0;
	int max_queries = IRCD_MIN(irc_nscount, rcount);

	/* RES_PRIMARY option is not implemented
	 * if (res.options & RES_PRIMARY || 0 == max_queries)
	 */
	if (max_queries == 0)
		max_queries = 1;

	for (i = 0; i < max_queries; i++)
	{
		if (sendto(res_fd, msg, len, 0,
		     (struct sockaddr *)&(irc_nsaddr_list[i]), 
				GET_SS_LEN(irc_nsaddr_list[i])) == len)
			++sent;
	}

	return (sent);
}

/*
 * find_id - find a dns request id (id is determined by dn_mkquery)
 */
static struct reslist *find_id(int id)
{
	dlink_node *ptr;
	struct reslist *request;

	DLINK_FOREACH(ptr, request_list.head)
	{
		request = ptr->data;

		if (request->id == id)
			return (request);
	}

	return (NULL);
}

/* 
 * gethost_byname_type - get host address from name
 *
 */
void gethost_byname_type(const char *name, struct DNSQuery *query, int type)
{
	assert(name != 0);
	do_query_name(query, name, NULL, type);
}

/*
 * gethost_byaddr - get host name from address
 */
void gethost_byaddr(const struct irc_sockaddr_storage *addr, struct DNSQuery *query)
{
	do_query_number(query, addr, NULL);
}

/*
 * do_query_name - nameserver lookup name
 */
static void do_query_name(struct DNSQuery *query, const char *name, struct reslist *request,
			  int type)
{
	char host_name[HOSTLEN + 1];

	strlcpy(host_name, name, HOSTLEN);
	add_local_domain(host_name, HOSTLEN);

	if (request == NULL)
	{
		request = make_request(query);
		request->name = (char *)MyMalloc(strlen(host_name) + 1);
		strcpy(request->name, host_name);
		request->state = REQ_A;
	}

	strlcpy(request->queryname, host_name, sizeof(request->queryname));
	request->type = type;
	query_name(request);
}

/*
 * do_query_number - Use this to do reverse IP# lookups.
 */
static void do_query_number(struct DNSQuery *query, const struct irc_sockaddr_storage *addr,
			    struct reslist *request)
{
	const unsigned char *cp;

	if (request == NULL)
	{
		request = make_request(query);
		memcpy(&request->addr, addr, sizeof(struct irc_sockaddr_storage));
		request->name = (char *)MyMalloc(HOSTLEN + 1);
	}

	if (addr->ss_family == AF_INET)
	{
		struct sockaddr_in *v4 = (struct sockaddr_in *)addr;
		cp = (const unsigned char *)&v4->sin_addr.s_addr;

		ircsprintf(request->queryname, "%u.%u.%u.%u.in-addr.arpa", (unsigned int)(cp[3]),
			   (unsigned int)(cp[2]), (unsigned int)(cp[1]), (unsigned int)(cp[0]));
	}
#ifdef IPV6
	else if (addr->ss_family == AF_INET6)
	{
		struct sockaddr_in6 *v6 = (struct sockaddr_in6 *)addr;
		cp = (const unsigned char *)&v6->sin6_addr.s6_addr;

		(void)sprintf(request->queryname, "%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x."
			      "%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.ip6.arpa",
			      (unsigned int)(cp[15] & 0xf), (unsigned int)(cp[15] >> 4),
			      (unsigned int)(cp[14] & 0xf), (unsigned int)(cp[14] >> 4),
			      (unsigned int)(cp[13] & 0xf), (unsigned int)(cp[13] >> 4),
			      (unsigned int)(cp[12] & 0xf), (unsigned int)(cp[12] >> 4),
			      (unsigned int)(cp[11] & 0xf), (unsigned int)(cp[11] >> 4),
			      (unsigned int)(cp[10] & 0xf), (unsigned int)(cp[10] >> 4),
			      (unsigned int)(cp[9] & 0xf), (unsigned int)(cp[9] >> 4),
			      (unsigned int)(cp[8] & 0xf), (unsigned int)(cp[8] >> 4),
			      (unsigned int)(cp[7] & 0xf), (unsigned int)(cp[7] >> 4),
			      (unsigned int)(cp[6] & 0xf), (unsigned int)(cp[6] >> 4),
			      (unsigned int)(cp[5] & 0xf), (unsigned int)(cp[5] >> 4),
			      (unsigned int)(cp[4] & 0xf), (unsigned int)(cp[4] >> 4),
			      (unsigned int)(cp[3] & 0xf), (unsigned int)(cp[3] >> 4),
			      (unsigned int)(cp[2] & 0xf), (unsigned int)(cp[2] >> 4),
			      (unsigned int)(cp[1] & 0xf), (unsigned int)(cp[1] >> 4),
			      (unsigned int)(cp[0] & 0xf), (unsigned int)(cp[0] >> 4));
	}
#endif

	request->type = T_PTR;
	query_name(request);
}

/*
 * query_name - generate a query based on class, type and name.
 */
static void query_name(struct reslist *request)
{
	char buf[MAXPACKET];
	int request_len = 0;

	memset(buf, 0, sizeof(buf));

	if ((request_len =
	     irc_res_mkquery(request->queryname, C_IN, request->type, (unsigned char *)buf, sizeof(buf))) > 0)
	{
		HEADER *header = (HEADER *) buf;
#ifndef HAVE_LRAND48
		int k = 0;
		struct timeval tv;
#endif
		/*
		 * generate an unique id
		 * NOTE: we don't have to worry about converting this to and from
		 * network byte order, the nameserver does not interpret this value
		 * and returns it unchanged
		 */
#ifdef HAVE_LRAND48
		do
		{
			header->id = (header->id + lrand48()) & 0xffff;
		} while (find_id(header->id));
#else
		gettimeofday(&tv, NULL);
		do
		{
			header->id = (header->id + k + tv.tv_usec) & 0xffff;
			k++;
		} while (find_id(header->id));
#endif /* HAVE_LRAND48 */
		request->id = header->id;
		++request->sends;

		request->sent += send_res_msg(buf, request_len, request->sends);
	}
}

static void resend_query(struct reslist *request)
{
	if (request->resend == 0)
		return;

	switch (request->type)
	{
	  case T_PTR:
		  do_query_number(NULL, &request->addr, request);
		  break;
	  case T_A:
#ifdef IPV6
	  case T_AAAA:
#endif
		  do_query_name(NULL, request->name, request, request->type);
		  break;
	  default:
		  break;
	}
}

/*
 * check_question - check if the reply really belongs to the
 * name we queried (to guard against late replies from previous
 * queries with the same id).
 */
static int check_question(struct reslist *request, HEADER * header, char *buf, char *eob)
{
	char hostbuf[128];	/* working buffer */
	unsigned char *current;	/* current position in buf */
	int n;			/* temp count */

	current = (unsigned char *)buf + sizeof(HEADER);
	if (header->qdcount != 1)
		return 0;
	n = irc_dn_expand((unsigned char *)buf, (unsigned char *)eob, current, hostbuf,
			  sizeof(hostbuf));
	if (n <= 0)
		return 0;
	if (strcasecmp(hostbuf, request->queryname))
		return 0;
	return 1;
}

/*
 * proc_answer - process name server reply
 */
static int proc_answer(struct reslist *request, HEADER * header, char *buf, char *eob)
{
	char hostbuf[HOSTLEN + 100];	/* working buffer */
	unsigned char *current;	/* current position in buf */
	int query_class;	/* answer class */
	int type;		/* answer type */
	int n;			/* temp count */
	int rd_length;
	struct sockaddr_in *v4;	/* conversion */
#ifdef IPV6
	struct sockaddr_in6 *v6;
#endif
	current = (unsigned char *)buf + sizeof(HEADER);

	for (; header->qdcount > 0; --header->qdcount)
	{
		if ((n = irc_dn_skipname(current, (unsigned char *)eob)) < 0)
			return 0;

		current += (size_t) n + QFIXEDSZ;
	}

	/*
	 * process each answer sent to us blech.
	 */
	while (header->ancount > 0 && (char *)current < eob)
	{
		header->ancount--;

		n = irc_dn_expand((unsigned char *)buf, (unsigned char *)eob, current, hostbuf,
				  sizeof(hostbuf));

		if (n < 0)
		{
			/*
			 * broken message
			 */
			return (0);
		}
		else if (n == 0)
		{
			/*
			 * no more answers left
			 */
			return (0);
		}

		hostbuf[HOSTLEN] = '\0';

		/* With Address arithmetic you have to be very anal
		 * this code was not working on alpha due to that
		 * (spotted by rodder/jailbird/dianora)
		 */
		current += (size_t) n;

		if (!(((char *)current + ANSWER_FIXED_SIZE) < eob))
			break;

		type = irc_ns_get16(current);
		current += TYPE_SIZE;

		query_class = irc_ns_get16(current);
		current += CLASS_SIZE;

		request->ttl = irc_ns_get32(current);
		current += TTL_SIZE;

		rd_length = irc_ns_get16(current);
		current += RDLENGTH_SIZE;

		/* 
		 * Wait to set request->type until we verify this structure 
		 */
		switch (type)
		{
		  case T_A:
			  if (request->type != T_A)
				  return (0);

			  /*
			   * check for invalid rd_length or too many addresses
			   */
			  if (rd_length != sizeof(struct in_addr))
				  return (0);
			  v4 = (struct sockaddr_in *)&request->addr;
			  SET_SS_LEN(request->addr, sizeof(struct sockaddr_in));
			  v4->sin_family = AF_INET;
			  memcpy(&v4->sin_addr, current, sizeof(struct in_addr));
			  return (1);
			  break;
#ifdef IPV6
		  case T_AAAA:
			  if (request->type != T_AAAA)
				  return (0);
			  if (rd_length != sizeof(struct in6_addr))
				  return (0);
			  SET_SS_LEN(request->addr, sizeof(struct sockaddr_in6));
			  v6 = (struct sockaddr_in6 *)&request->addr;
			  v6->sin6_family = AF_INET6;
			  memcpy(&v6->sin6_addr, current, sizeof(struct in6_addr));
			  return (1);
			  break;
#endif
		  case T_PTR:
			  if (request->type != T_PTR)
				  return (0);
			  n = irc_dn_expand((unsigned char *)buf, (unsigned char *)eob, current,
					    hostbuf, sizeof(hostbuf));
			  if (n < 0)
				  return (0);	/* broken message */
			  else if (n == 0)
				  return (0);	/* no more answers left */

			  strlcpy(request->name, hostbuf, HOSTLEN);

			  return (1);
			  break;
		  case T_CNAME:	/* first check we already havent started looking 
					   into a cname */
			  if (request->type != T_PTR)
				  return (0);

			  if (request->state == REQ_CNAME)
			  {
				  n = irc_dn_expand((unsigned char *)buf, (unsigned char *)eob,
						    current, hostbuf, sizeof(hostbuf));

				  if (n < 0)
					  return (0);
				  return (1);
			  }

			  request->state = REQ_CNAME;
			  current += rd_length;
			  break;

		  default:
			  /* XXX I'd rather just throw away the entire bogus thing
			   * but its possible its just a broken nameserver with still
			   * valid answers. But lets do some rudimentary logging for now...
			   */
			  ilog(L_MAIN, "irc_res.c bogus type %d", type);
			  break;
		}
	}

	return (1);
}

/*
 * res_readreply - read a dns reply from the nameserver and process it.
 */
static void res_readreply(int fd, void *data)
{
	char buf[sizeof(HEADER) + MAXPACKET]
		/* Sparc and alpha need 16bit-alignment for accessing header->id 
		 * (which is uint16_t). Because of the header = (HEADER*) buf; 
		 * lateron, this is neeeded. --FaUl
		 */
#if defined(__sparc__) || defined(__alpha__)
		__attribute__ ((aligned(16)))
#endif
		;
	HEADER *header;
	struct reslist *request = NULL;
	struct DNSReply *reply = NULL;
	int rc;
	int answer_count;
	socklen_t len = sizeof(struct irc_sockaddr_storage);
	struct irc_sockaddr_storage lsin;

	rc = recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr *)&lsin, &len);

	/* Re-schedule a read *after* recvfrom, or we'll be registering
	 * interest where it'll instantly be ready for read :-) -- adrian
	 */
	comm_setselect(fd, FDLIST_NONE, COMM_SELECT_READ, res_readreply, NULL, 0);
	/* Better to cast the sizeof instead of rc */
	if (rc <= (int)(sizeof(HEADER)))
		return;

	/*
	 * convert DNS reply reader from Network byte order to CPU byte order.
	 */
	header = (HEADER *) buf;
	header->ancount = ntohs(header->ancount);
	header->qdcount = ntohs(header->qdcount);
	header->nscount = ntohs(header->nscount);
	header->arcount = ntohs(header->arcount);

	/*
	 * response for an id which we have already received an answer for
	 * just ignore this response.
	 */
	if (0 == (request = find_id(header->id)))
		return;

	/*
	 * check against possibly fake replies
	 */
	if (!res_ourserver(&lsin))
		return;

	if (!check_question(request, header, buf, buf + rc))
		return;

	if ((header->rcode != NO_ERRORS) || (header->ancount == 0))
	{
		if (NXDOMAIN == header->rcode)
		{
			(*request->query->callback) (request->query->ptr, NULL);
			rem_request(request);
		}
		else
		{
			/*
			 * If a bad error was returned, we stop here and dont send
			 * send any more (no retries granted).
			 */
			(*request->query->callback) (request->query->ptr, NULL);
			rem_request(request);
		}
		return;
	}
	/*
	 * If this fails there was an error decoding the received packet, 
	 * give up. -- jilles
	 */
	answer_count = proc_answer(request, header, buf, buf + rc);

	if (answer_count)
	{
		if (request->type == T_PTR)
		{
			if (request->name == NULL)
			{
				/*
				 * got a PTR response with no name, something bogus is happening
				 * don't bother trying again, the client address doesn't resolve
				 */
				(*request->query->callback) (request->query->ptr, reply);
				rem_request(request);
				return;
			}

			/*
			 * Lookup the 'authoritative' name that we were given for the
			 * ip#. 
			 *
			 */
#ifdef IPV6
			if (request->addr.ss_family == AF_INET6)
				gethost_byname_type(request->name, request->query, T_AAAA);
			else
#endif
				gethost_byname_type(request->name, request->query, T_A);
			rem_request(request);
		}
		else
		{
			/*
			 * got a name and address response, client resolved
			 */
			reply = make_dnsreply(request);
			(*request->query->callback) (request->query->ptr, reply);
			MyFree(reply);
			rem_request(request);
		}
	}
	else
	{
		/* couldn't decode, give up -- jilles */
		(*request->query->callback) (request->query->ptr, NULL);
		rem_request(request);
	}
}

static struct DNSReply *make_dnsreply(struct reslist *request)
{
	struct DNSReply *cp;
	s_assert(request != 0);

	cp = (struct DNSReply *)MyMalloc(sizeof(struct DNSReply));

	cp->h_name = request->name;
	memcpy(&cp->addr, &request->addr, sizeof(cp->addr));
	return (cp);
}

void report_dns_servers(struct Client *source_p)
{
	int i;
	char ipaddr[128];

	for (i = 0; i < irc_nscount; i++)
	{
		if (!inetntop_sock((struct sockaddr *)&(irc_nsaddr_list[i]),
				ipaddr, sizeof ipaddr))
			strlcpy(ipaddr, "?", sizeof ipaddr);
		sendto_one_numeric(source_p, RPL_STATSDEBUG,
				"A %s", ipaddr);
	}
}
