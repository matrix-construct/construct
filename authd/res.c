/*
 * A rewrite of Darren Reeds original res.c As there is nothing
 * left of Darrens original code, this is now licensed by the hybrid group.
 * (Well, some of the function names are the same, and bits of the structs..)
 * You can use it where it is useful, free even. Buy us a beer and stuff.
 *
 * The authors takes no responsibility for any damage or loss
 * of property which results from the use of this software.
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
 *
 * Resend queries to other servers if the DNS server replies with an error or
 * an invalid response. Also, avoid servers that return errors or invalid
 * responses.
 *
 * October 2012 --mr_flea
 *
 * ircd-ratbox changes for random IDs merged back in.
 *
 * January 2016 --kaniini
 */

#include <rb_lib.h>
#include "setup.h"
#include "res.h"
#include "reslib.h"

#if (CHAR_BIT != 8)
#error this code needs to be able to address individual octets
#endif

static PF res_readreply;

#define MAXPACKET      1024	/* rfc sez 512 but we expand names so ... */
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

#ifdef RB_IPV6
struct in6_addr ipv6_addr;
#endif
struct in_addr ipv4_addr;

struct reslist
{
	rb_dlink_node node;
	int id;
	time_t ttl;
	char type;
	char queryname[IRCD_RES_HOSTLEN + 1]; /* name currently being queried */
	char retries;		/* retry counter */
	char sends;		/* number of sends (>1 means resent) */
	time_t sentat;
	time_t timeout;
	int lastns;	/* index of last server sent to */
	struct rb_sockaddr_storage addr;
	char *name;
	struct DNSQuery *query;	/* query callback for this request */
};

static rb_fde_t *res_fd;
static rb_dlink_list request_list = { NULL, NULL, 0 };
static int ns_failure_count[IRCD_MAXNS]; /* timeouts and invalid/failed replies */

static void rem_request(struct reslist *request);
static struct reslist *make_request(struct DNSQuery *query);
static void gethost_byname_type_fqdn(const char *name, struct DNSQuery *query,
		int type);
static void do_query_name(struct DNSQuery *query, const char *name, struct reslist *request, int);
static void do_query_number(struct DNSQuery *query, const struct rb_sockaddr_storage *,
			    struct reslist *request);
static void query_name(struct reslist *request);
static int send_res_msg(const char *buf, int len, int count);
static void resend_query(struct reslist *request);
static int check_question(struct reslist *request, HEADER * header, char *buf, char *eob);
static int proc_answer(struct reslist *request, HEADER * header, char *, char *);
static struct reslist *find_id(int id);
static struct DNSReply *make_dnsreply(struct reslist *request);
static uint16_t generate_random_id(void);

/*
 * int
 * res_ourserver(inp)
 *      looks up "inp" in irc_nsaddr_list[]
 * returns:
 *      server ID or -1 for not found
 * author:
 *      paul vixie, 29may94
 *      revised for ircd, cryogen(stu) may03
 *      slightly modified for charybdis, mr_flea oct12
 */
static int
res_ourserver(const struct rb_sockaddr_storage *inp)
{
#ifdef RB_IPV6
	const struct sockaddr_in6 *v6;
	const struct sockaddr_in6 *v6in = (const struct sockaddr_in6 *)inp;
#endif
	const struct sockaddr_in *v4;
	const struct sockaddr_in *v4in = (const struct sockaddr_in *)inp;
	int ns;

	for(ns = 0; ns < irc_nscount; ns++)
	{
		const struct rb_sockaddr_storage *srv = &irc_nsaddr_list[ns];
#ifdef RB_IPV6
		v6 = (const struct sockaddr_in6 *)srv;
#endif
		v4 = (const struct sockaddr_in *)srv;

		/* could probably just memcmp(srv, inp, srv.ss_len) here
		 * but we'll air on the side of caution - stu
		 */
		switch (GET_SS_FAMILY(srv))
		{
#ifdef RB_IPV6
		case AF_INET6:
			if(GET_SS_FAMILY(srv) == GET_SS_FAMILY(inp))
				if(v6->sin6_port == v6in->sin6_port)
					if((memcmp(&v6->sin6_addr.s6_addr, &v6in->sin6_addr.s6_addr,
						   sizeof(struct in6_addr)) == 0) ||
					   (memcmp(&v6->sin6_addr.s6_addr, &in6addr_any,
						   sizeof(struct in6_addr)) == 0))
						return 1;
			break;
#endif
		case AF_INET:
			if(GET_SS_FAMILY(srv) == GET_SS_FAMILY(inp))
				if(v4->sin_port == v4in->sin_port)
					if((v4->sin_addr.s_addr == INADDR_ANY)
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
	rb_dlink_node *ptr;
	rb_dlink_node *next_ptr;
	struct reslist *request;
	time_t next_time = 0;
	time_t timeout = 0;

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, request_list.head)
	{
		request = ptr->data;
		timeout = request->sentat + request->timeout;

		if (now >= timeout)
		{
			ns_failure_count[request->lastns]++;
			request->sentat = now;
			request->timeout += request->timeout;
			resend_query(request);
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
	timeout_query_list(rb_current_time());
}

static struct ev_entry *timeout_resolver_ev = NULL;

/*
 * start_resolver - do everything we need to read the resolv.conf file
 * and initialize the resolver file descriptor if needed
 */
static void start_resolver(void)
{
	int i;

	irc_res_init();
	for (i = 0; i < irc_nscount; i++)
		ns_failure_count[i] = 0;

	if (res_fd == NULL)
	{
		if ((res_fd = rb_socket(GET_SS_FAMILY(&irc_nsaddr_list[0]), SOCK_DGRAM, 0,
			       "UDP resolver socket")) == NULL)
			return;

		/* At the moment, the resolver FD data is global .. */
		rb_setselect(res_fd, RB_SELECT_READ, res_readreply, NULL);
		timeout_resolver_ev = rb_event_add("timeout_resolver", timeout_resolver, NULL, 1);
	}
}

/*
 * init_resolver - initialize resolver and resolver library
 */
void init_resolver(void)
{
#ifdef HAVE_SRAND48
	srand48(rb_current_time());
#endif
	start_resolver();
}

/*
 * restart_resolver - reread resolv.conf, reopen socket
 */
void restart_resolver(void)
{
	rb_close(res_fd);
	res_fd = NULL;
	rb_event_delete(timeout_resolver_ev);	/* -ddosen */
	start_resolver();
}

/*
 * add_local_domain - Add the domain to hostname, if it is missing
 * (as suggested by eps@TOASTER.SFSU.EDU)
 */
static void add_local_domain(char *hname, size_t size)
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
	rb_dlinkDelete(&request->node, &request_list);
	rb_free(request->name);
	rb_free(request);
}

/*
 * make_request - Create a DNS request record for the server.
 */
static struct reslist *make_request(struct DNSQuery *query)
{
	struct reslist *request = rb_malloc(sizeof(struct reslist));

	request->sentat = rb_current_time();
	request->retries = 3;
	request->timeout = 4;	/* start at 4 and exponential inc. */
	request->query = query;

	/*
	 * generate a unique id
	 * NOTE: we don't have to worry about converting this to and from
	 * network byte order, the nameserver does not interpret this value
	 * and returns it unchanged
	 *
	 * we generate an id per request now (instead of per send) to allow
	 * late replies to be used.
	 */
	request->id = generate_random_id();

	rb_dlinkAdd(request, &request->node, &request_list);

	return request;
}

/*
 * retryfreq - determine how many queries to wait before resending
 * if there have been that many consecutive timeouts
 *
 * This is a cubic backoff btw, if anyone didn't pick up on it. --Elizafox
 */
static int retryfreq(int timeouts)
{
	switch (timeouts)
	{
	case 1:
		return 3;
	case 2:
		return 9;
	case 3:
		return 27;
	case 4:
		return 81;
	default:
		return 243;
	}
}

/*
 * send_res_msg - sends msg to a nameserver.
 * This should reflect /etc/resolv.conf.
 * Returns number of nameserver successfully sent to
 * or -1 if no successful sends.
 */
static int send_res_msg(const char *msg, int len, int rcount)
{
	int i;
	int ns;
	static int retrycnt;

	retrycnt++;
	/* First try a nameserver that seems to work.
	 * Every once in a while, try a possibly broken one to check
	 * if it is working again.
	 */
	for (i = 0; i < irc_nscount; i++)
	{
		ns = (i + rcount - 1) % irc_nscount;
		if (ns_failure_count[ns] && retrycnt % retryfreq(ns_failure_count[ns]))
			continue;
		if (sendto(rb_get_fd(res_fd), msg, len, 0,
		     (struct sockaddr *)&(irc_nsaddr_list[ns]),
				GET_SS_LEN(&irc_nsaddr_list[ns])) == len)
			return ns;
	}

	/* No known working nameservers, try some broken one. */
	for (i = 0; i < irc_nscount; i++)
	{
		ns = (i + rcount - 1) % irc_nscount;
		if (!ns_failure_count[ns])
			continue;
		if (sendto(rb_get_fd(res_fd), msg, len, 0,
		     (struct sockaddr *)&(irc_nsaddr_list[ns]),
				GET_SS_LEN(&irc_nsaddr_list[ns])) == len)
			return ns;
	}

	return -1;
}

/*
 * find_id - find a dns request id (id is determined by dn_mkquery)
 */
static struct reslist *find_id(int id)
{
	rb_dlink_node *ptr;
	struct reslist *request;

	RB_DLINK_FOREACH(ptr, request_list.head)
	{
		request = ptr->data;

		if (request->id == id)
			return (request);
	}

	return (NULL);
}

static uint16_t
generate_random_id(void)
{
	uint16_t id;

	do
	{
		rb_get_random(&id, sizeof(id));
		if(id == 0xffff)
			continue;
	}
	while(find_id(id));
	return id;
}

/*
 * gethost_byname_type - get host address from name, adding domain if needed
 */
void gethost_byname_type(const char *name, struct DNSQuery *query, int type)
{
	char fqdn[IRCD_RES_HOSTLEN + 1];
	assert(name != 0);

	rb_strlcpy(fqdn, name, sizeof fqdn);
	add_local_domain(fqdn, IRCD_RES_HOSTLEN);
	gethost_byname_type_fqdn(fqdn, query, type);
}

/*
 * gethost_byname_type_fqdn - get host address from fqdn
 */
static void gethost_byname_type_fqdn(const char *name, struct DNSQuery *query,
		int type)
{
	assert(name != 0);
	do_query_name(query, name, NULL, type);
}

/*
 * gethost_byaddr - get host name from address
 */
void gethost_byaddr(const struct rb_sockaddr_storage *addr, struct DNSQuery *query)
{
	do_query_number(query, addr, NULL);
}

/*
 * do_query_name - nameserver lookup name
 */
static void do_query_name(struct DNSQuery *query, const char *name, struct reslist *request,
			  int type)
{
	if (request == NULL)
	{
		request = make_request(query);
		request->name = rb_strdup(name);
	}

	rb_strlcpy(request->queryname, name, sizeof(request->queryname));
	request->type = type;
	query_name(request);
}

/* Build an rDNS style query - if suffix is NULL, use the appropriate .arpa zone */
void build_rdns(char *buf, size_t size, const struct rb_sockaddr_storage *addr, const char *suffix)
{
	const unsigned char *cp;

	if (GET_SS_FAMILY(addr) == AF_INET)
	{
		const struct sockaddr_in *v4 = (const struct sockaddr_in *)addr;
		cp = (const unsigned char *)&v4->sin_addr.s_addr;

		(void) snprintf(buf, size, "%u.%u.%u.%u.%s",
			(unsigned int)(cp[3]),
			(unsigned int)(cp[2]),
			(unsigned int)(cp[1]),
			(unsigned int)(cp[0]),
			suffix == NULL ? "in-addr.arpa" : suffix);
	}
#ifdef RB_IPV6
	else if (GET_SS_FAMILY(addr) == AF_INET6)
	{
		const struct sockaddr_in6 *v6 = (const struct sockaddr_in6 *)addr;
		cp = (const unsigned char *)&v6->sin6_addr.s6_addr;

#define HI_NIBBLE(x) (unsigned int)((x) >> 4)
#define LO_NIBBLE(x) (unsigned int)((x) & 0xf)

		(void) snprintf(buf, size,
			"%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%s",
			LO_NIBBLE(cp[15]), HI_NIBBLE(cp[15]),
			LO_NIBBLE(cp[14]), HI_NIBBLE(cp[14]),
			LO_NIBBLE(cp[13]), HI_NIBBLE(cp[13]),
			LO_NIBBLE(cp[12]), HI_NIBBLE(cp[12]),
			LO_NIBBLE(cp[11]), HI_NIBBLE(cp[11]),
			LO_NIBBLE(cp[10]), HI_NIBBLE(cp[10]),
			LO_NIBBLE(cp[9]),  HI_NIBBLE(cp[9]),
			LO_NIBBLE(cp[8]),  HI_NIBBLE(cp[8]),
			LO_NIBBLE(cp[7]),  HI_NIBBLE(cp[7]),
			LO_NIBBLE(cp[6]),  HI_NIBBLE(cp[6]),
			LO_NIBBLE(cp[5]),  HI_NIBBLE(cp[5]),
			LO_NIBBLE(cp[4]),  HI_NIBBLE(cp[4]),
			LO_NIBBLE(cp[3]),  HI_NIBBLE(cp[3]),
			LO_NIBBLE(cp[2]),  HI_NIBBLE(cp[2]),
			LO_NIBBLE(cp[1]),  HI_NIBBLE(cp[1]),
			LO_NIBBLE(cp[0]),  HI_NIBBLE(cp[0]),
			suffix == NULL ? "ip6.arpa" : suffix);
	}

#undef HI_NIBBLE
#undef LO_NIBBLE

#endif
}

/*
 * do_query_number - Use this to do reverse IP# lookups.
 */
static void do_query_number(struct DNSQuery *query, const struct rb_sockaddr_storage *addr,
			    struct reslist *request)
{
	if (request == NULL)
	{
		request = make_request(query);
		memcpy(&request->addr, addr, sizeof(struct rb_sockaddr_storage));
		request->name = (char *)rb_malloc(IRCD_RES_HOSTLEN + 1);
	}

	build_rdns(request->queryname, IRCD_RES_HOSTLEN + 1, addr, NULL);

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
	int ns;

	memset(buf, 0, sizeof(buf));

	if ((request_len =
	     irc_res_mkquery(request->queryname, C_IN, request->type, (unsigned char *)buf, sizeof(buf))) > 0)
	{
		HEADER *header = (HEADER *)(void *)buf;
		header->id = request->id;
		++request->sends;

		ns = send_res_msg(buf, request_len, request->sends);
		if (ns != -1)
			request->lastns = ns;
	}
}

static void resend_query(struct reslist *request)
{
	if (--request->retries <= 0)
	{
		(*request->query->callback) (request->query->ptr, NULL);
		rem_request(request);
		return;
	}

	switch (request->type)
	{
	case T_PTR:
		do_query_number(NULL, &request->addr, request);
		break;
	case T_A:
#ifdef RB_IPV6
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
	char hostbuf[IRCD_RES_HOSTLEN + 1];	/* working buffer */
	unsigned char *current;	/* current position in buf */
	int n;			/* temp count */

	current = (unsigned char *)buf + sizeof(HEADER);
	if (header->qdcount != 1)
		return 0;
	n = irc_dn_expand((unsigned char *)buf, (unsigned char *)eob, current, hostbuf,
			  sizeof(hostbuf));
	if (n <= 0)
		return 0;
	if (rb_strcasecmp(hostbuf, request->queryname))
		return 0;
	return 1;
}

/*
 * proc_answer - process name server reply
 */
static int proc_answer(struct reslist *request, HEADER * header, char *buf, char *eob)
{
	char hostbuf[IRCD_RES_HOSTLEN + 100];	/* working buffer */
	unsigned char *current;	/* current position in buf */
	int type;		/* answer type */
	int n;			/* temp count */
	int rd_length;
	struct sockaddr_in *v4;	/* conversion */
#ifdef RB_IPV6
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

		hostbuf[IRCD_RES_HOSTLEN] = '\0';

		/* With Address arithmetic you have to be very anal
		 * this code was not working on alpha due to that
		 * (spotted by rodder/jailbird/dianora)
		 */
		current += (size_t) n;

		if (!(((char *)current + ANSWER_FIXED_SIZE) < eob))
			break;

		type = irc_ns_get16(current);
		current += TYPE_SIZE;

		(void) irc_ns_get16(current);
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
			SET_SS_LEN(&request->addr, sizeof(struct sockaddr_in));
			v4->sin_family = AF_INET;
			memcpy(&v4->sin_addr, current, sizeof(struct in_addr));
			return (1);
#ifdef RB_IPV6
		case T_AAAA:
			if (request->type != T_AAAA)
				return (0);
			if (rd_length != sizeof(struct in6_addr))
				return (0);
			SET_SS_LEN(&request->addr, sizeof(struct sockaddr_in6));
			v6 = (struct sockaddr_in6 *)&request->addr;
			v6->sin6_family = AF_INET6;
			memcpy(&v6->sin6_addr, current, sizeof(struct in6_addr));
			return (1);
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

			rb_strlcpy(request->name, hostbuf, IRCD_RES_HOSTLEN + 1);

			return (1);
		case T_CNAME:
			/* real answer will follow */
			current += rd_length;
			break;
		default:
			break;
		}
	}

	return (1);
}

/*
 * res_read_single_reply - read a dns reply from the nameserver and process it.
 * Return value: 1 if a packet was read, 0 otherwise
 */
static int res_read_single_reply(rb_fde_t *F, void *data)
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
	socklen_t len = sizeof(struct rb_sockaddr_storage);
	struct rb_sockaddr_storage lsin;
	int ns;

	rc = recvfrom(rb_get_fd(F), buf, sizeof(buf), 0, (struct sockaddr *)&lsin, &len);

	/* No packet */
	if (rc == 0 || rc == -1)
		return 0;

	/* Too small */
	if (rc <= (int)(sizeof(HEADER)))
		return 1;

	/*
	 * convert DNS reply reader from Network byte order to CPU byte order.
	 */
	header = (HEADER *)(void *)buf;
	header->ancount = ntohs(header->ancount);
	header->qdcount = ntohs(header->qdcount);
	header->nscount = ntohs(header->nscount);
	header->arcount = ntohs(header->arcount);

	/*
	 * response for an id which we have already received an answer for
	 * just ignore this response.
	 */
	if (0 == (request = find_id(header->id)))
		return 1;

	/*
	 * check against possibly fake replies
	 */
	ns = res_ourserver(&lsin);
	if (ns == -1)
		return 1;

	if (ns != request->lastns)
	{
		/*
		 * We'll accept the late reply, but penalize it a little more to make
		 * sure a laggy server doesn't end up favored.
		 */
		ns_failure_count[ns] += 3;
	}


	if (!check_question(request, header, buf, buf + rc))
		return 1;

	if ((header->rcode != NO_ERRORS) || (header->ancount == 0))
	{
		/*
		 * RFC 2136 states that in the event of a server returning SERVFAIL
		 * or NOTIMP, the request should be resent to the next server.
		 * Additionally, if the server refuses our query, resend it as well.
		 * -- mr_flea
		 */
		if (SERVFAIL == header->rcode || NOTIMP == header->rcode ||
				REFUSED == header->rcode)
		{
			ns_failure_count[ns]++;
			resend_query(request);
		}
		else
		{
			/*
			 * Either a fatal error was returned or no answer. Cancel the
			 * request.
			 */
			if (NXDOMAIN == header->rcode)
			{
				/* If the rcode is NXDOMAIN, treat it as a good response. */
				ns_failure_count[ns] /= 4;
			}
			(*request->query->callback) (request->query->ptr, NULL);
			rem_request(request);
		}
		return 1;
	}
	/*
	 * If this fails there was an error decoding the received packet.
	 * -- jilles
	 */
	answer_count = proc_answer(request, header, buf, buf + rc);

	if (answer_count)
	{
		if (request->type == T_PTR)
		{
			if (request->name == NULL)
			{
				/*
				 * Got a PTR response with no name, something strange is
				 * happening. Try another DNS server.
				 */
				ns_failure_count[ns]++;
				resend_query(request);
				return 1;
			}

			/*
			 * Lookup the 'authoritative' name that we were given for the
			 * ip#.
			 */
#ifdef RB_IPV6
			if (GET_SS_FAMILY(&request->addr) == AF_INET6)
				gethost_byname_type_fqdn(request->name, request->query, T_AAAA);
			else
#endif
				gethost_byname_type_fqdn(request->name, request->query, T_A);
			rem_request(request);
		}
		else
		{
			/*
			 * got a name and address response, client resolved
			 */
			reply = make_dnsreply(request);
			(*request->query->callback) (request->query->ptr, reply);
			rb_free(reply);
			rem_request(request);
		}

		ns_failure_count[ns] /= 4;
	}
	else
	{
		/* Invalid or corrupt reply - try another resolver. */
		ns_failure_count[ns]++;
		resend_query(request);
	}
	return 1;
}

static void
res_readreply(rb_fde_t *F, void *data)
{
	while (res_read_single_reply(F, data))
		;
	rb_setselect(F, RB_SELECT_READ, res_readreply, NULL);
}

static struct DNSReply *
make_dnsreply(struct reslist *request)
{
	struct DNSReply *cp;
	lrb_assert(request != 0);

	cp = (struct DNSReply *)rb_malloc(sizeof(struct DNSReply));

	cp->h_name = request->name;
	memcpy(&cp->addr, &request->addr, sizeof(cp->addr));
	return (cp);
}
