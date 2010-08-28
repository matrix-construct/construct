/* This code is in the public domain.
 * $Id: newconf.c 3550 2007-08-09 06:47:26Z nenolod $
 */

#include "stdinc.h"

#ifdef HAVE_LIBCRYPTO
#include <openssl/pem.h>
#include <openssl/rsa.h>
#endif

#include "newconf.h"
#include "ircd_defs.h"
#include "common.h"
#include "logger.h"
#include "s_conf.h"
#include "s_user.h"
#include "s_newconf.h"
#include "send.h"
#include "setup.h"
#include "modules.h"
#include "listener.h"
#include "hostmask.h"
#include "s_serv.h"
#include "hash.h"
#include "cache.h"
#include "ircd.h"
#include "snomask.h"
#include "blacklist.h"
#include "sslproc.h"
#include "privilege.h"

#define CF_TYPE(x) ((x) & CF_MTYPE)

struct TopConf *conf_cur_block;
static char *conf_cur_block_name;

static rb_dlink_list conf_items;

static struct ConfItem *yy_aconf = NULL;

static struct Class *yy_class = NULL;

static struct remote_conf *yy_shared = NULL;
static struct server_conf *yy_server = NULL;

static rb_dlink_list yy_aconf_list;
static rb_dlink_list yy_oper_list;
static rb_dlink_list yy_shared_list;
static rb_dlink_list yy_cluster_list;
static struct oper_conf *yy_oper = NULL;

static struct alias_entry *yy_alias = NULL;

static char *yy_blacklist_host = NULL;
static char *yy_blacklist_reason = NULL;
static char *yy_privset_extends = NULL;

static const char *
conf_strtype(int type)
{
	switch (type & CF_MTYPE)
	{
	case CF_INT:
		return "integer value";
	case CF_STRING:
		return "unquoted string";
	case CF_YESNO:
		return "yes/no value";
	case CF_QSTRING:
		return "quoted string";
	case CF_TIME:
		return "time/size value";
	default:
		return "unknown type";
	}
}

int
add_top_conf(const char *name, int (*sfunc) (struct TopConf *), 
		int (*efunc) (struct TopConf *), struct ConfEntry *items)
{
	struct TopConf *tc;

	tc = rb_malloc(sizeof(struct TopConf));

	tc->tc_name = name;
	tc->tc_sfunc = sfunc;
	tc->tc_efunc = efunc;
	tc->tc_entries = items;

	rb_dlinkAddAlloc(tc, &conf_items);
	return 0;
}

struct TopConf *
find_top_conf(const char *name)
{
	rb_dlink_node *d;
	struct TopConf *tc;

	RB_DLINK_FOREACH(d, conf_items.head)
	{
		tc = d->data;
		if(strcasecmp(tc->tc_name, name) == 0)
			return tc;
	}

	return NULL;
}


struct ConfEntry *
find_conf_item(const struct TopConf *top, const char *name)
{
	struct ConfEntry *cf;
	rb_dlink_node *d;

	if(top->tc_entries)
	{
		int i;

		for(i = 0; top->tc_entries[i].cf_type; i++)
		{
			cf = &top->tc_entries[i];

			if(!strcasecmp(cf->cf_name, name))
				return cf;
		}
	}

	RB_DLINK_FOREACH(d, top->tc_items.head)
	{
		cf = d->data;
		if(strcasecmp(cf->cf_name, name) == 0)
			return cf;
	}

	return NULL;
}

int
remove_top_conf(char *name)
{
	struct TopConf *tc;
	rb_dlink_node *ptr;

	if((tc = find_top_conf(name)) == NULL)
		return -1;

	if((ptr = rb_dlinkFind(tc, &conf_items)) == NULL)
		return -1;

	rb_dlinkDestroy(ptr, &conf_items);
	rb_free(tc);

	return 0;
}

static void
conf_set_serverinfo_name(void *data)
{
	if(ServerInfo.name == NULL)
	{
		const char *s;
		int dots = 0;

		for(s = data; *s != '\0'; s++)
		{
			if(!IsServChar(*s))
			{
				conf_report_error("Ignoring serverinfo::name "
						  "-- bogus servername.");
				return;
			}
			else if(*s == '.')
				++dots;
		}

		if(!dots)
		{
			conf_report_error("Ignoring serverinfo::name -- must contain '.'");
			return;
		}

		s = data;

		if(IsDigit(*s))
		{
			conf_report_error("Ignoring serverinfo::name -- cannot begin with digit.");
			return;
		}

		/* the ircd will exit() in main() if we dont set one */
		if(strlen(s) <= HOSTLEN)
			ServerInfo.name = rb_strdup((char *) data);
	}
}

static void
conf_set_serverinfo_sid(void *data)
{
	char *sid = data;

	if(ServerInfo.sid[0] == '\0')
	{
		if(!IsDigit(sid[0]) || !IsIdChar(sid[1]) ||
		   !IsIdChar(sid[2]) || sid[3] != '\0')
		{
			conf_report_error("Ignoring serverinfo::sid "
					  "-- bogus sid.");
			return;
		}

		strcpy(ServerInfo.sid, sid);
	}
}

static void
conf_set_serverinfo_network_name(void *data)
{
	char *p;

	if((p = strchr((char *) data, ' ')))
		*p = '\0';

	rb_free(ServerInfo.network_name);
	ServerInfo.network_name = rb_strdup((char *) data);
}

static void
conf_set_serverinfo_vhost(void *data)
{
	if(rb_inet_pton(AF_INET, (char *) data, &ServerInfo.ip.sin_addr) <= 0)
	{
		conf_report_error("Invalid netmask for server IPv4 vhost (%s)", (char *) data);
		return;
	}
	ServerInfo.ip.sin_family = AF_INET;
	ServerInfo.specific_ipv4_vhost = 1;
}

static void
conf_set_serverinfo_vhost6(void *data)
{
#ifdef RB_IPV6
	if(rb_inet_pton(AF_INET6, (char *) data, &ServerInfo.ip6.sin6_addr) <= 0)
	{
		conf_report_error("Invalid netmask for server IPv6 vhost (%s)", (char *) data);
		return;
	}

	ServerInfo.specific_ipv6_vhost = 1;
	ServerInfo.ip6.sin6_family = AF_INET6;
#else
	conf_report_error("Warning -- ignoring serverinfo::vhost6 -- IPv6 support not available.");
#endif
}

static void
conf_set_modules_module(void *data)
{
#ifndef STATIC_MODULES
	char *m_bn;

	m_bn = rb_basename((char *) data);

	if(findmodule_byname(m_bn) != -1)
		return;

	load_one_module((char *) data, 0);

	rb_free(m_bn);
#else
	conf_report_error("Ignoring modules::module -- loadable module support not present.");
#endif
}

static void
conf_set_modules_path(void *data)
{
#ifndef STATIC_MODULES
	mod_add_path((char *) data);
#else
	conf_report_error("Ignoring modules::path -- loadable module support not present.");
#endif
}

struct mode_table
{
	const char *name;
	int mode;
};

/* *INDENT-OFF* */
static struct mode_table umode_table[] = {
	{"callerid",	UMODE_CALLERID	},
	{"deaf",	UMODE_DEAF	},
	{"invisible",	UMODE_INVISIBLE	},
	{"locops",	UMODE_LOCOPS	},
	{"noforward",	UMODE_NOFORWARD	},
	{"regonlymsg",	UMODE_REGONLYMSG},
	{"servnotice",	UMODE_SERVNOTICE},
	{"wallop",	UMODE_WALLOP	},
	{"operwall",	UMODE_OPERWALL	},
	{NULL, 0}
};

static struct mode_table oper_table[] = {
	{"encrypted",		OPER_ENCRYPTED		},
	{"need_ssl",		OPER_NEEDSSL		},
	{NULL, 0}
};

static struct mode_table auth_table[] = {
	{"encrypted",		CONF_FLAGS_ENCRYPTED	},
	{"spoof_notice",	CONF_FLAGS_SPOOF_NOTICE	},
	{"exceed_limit",	CONF_FLAGS_NOLIMIT	},
	{"dnsbl_exempt",	CONF_FLAGS_EXEMPTDNSBL	},
	{"kline_exempt",	CONF_FLAGS_EXEMPTKLINE	},
	{"flood_exempt",	CONF_FLAGS_EXEMPTFLOOD	},
	{"spambot_exempt",	CONF_FLAGS_EXEMPTSPAMBOT },
	{"shide_exempt",	CONF_FLAGS_EXEMPTSHIDE	},
	{"jupe_exempt",		CONF_FLAGS_EXEMPTJUPE	},
	{"resv_exempt",		CONF_FLAGS_EXEMPTRESV	},
	{"no_tilde",		CONF_FLAGS_NO_TILDE	},
	{"need_ident",		CONF_FLAGS_NEED_IDENTD	},
	{"have_ident",		CONF_FLAGS_NEED_IDENTD	},
	{"need_ssl", 		CONF_FLAGS_NEED_SSL	},
	{"need_sasl",		CONF_FLAGS_NEED_SASL	},
	{NULL, 0}
};

static struct mode_table connect_table[] = {
	{ "autoconn",	SERVER_AUTOCONN		},
	{ "compressed",	SERVER_COMPRESSED	},
	{ "encrypted",	SERVER_ENCRYPTED	},
	{ "topicburst",	SERVER_TB		},
	{ "ssl",	SERVER_SSL		},
	{ NULL,		0			},
};

static struct mode_table cluster_table[] = {
	{ "kline",	SHARED_PKLINE	},
	{ "tkline",	SHARED_TKLINE	},
	{ "unkline",	SHARED_UNKLINE	},
	{ "locops",	SHARED_LOCOPS	},
	{ "xline",	SHARED_PXLINE	},
	{ "txline",	SHARED_TXLINE	},
	{ "unxline",	SHARED_UNXLINE	},
	{ "resv",	SHARED_PRESV	},
	{ "tresv",	SHARED_TRESV	},
	{ "unresv",	SHARED_UNRESV	},
	{ "all",	CLUSTER_ALL	},
	{NULL, 0}
};

static struct mode_table shared_table[] =
{
	{ "kline",	SHARED_PKLINE|SHARED_TKLINE	},
	{ "xline",	SHARED_PXLINE|SHARED_TXLINE	},
	{ "resv",	SHARED_PRESV|SHARED_TRESV	},
	{ "dline",  SHARED_PDLINE|SHARED_TDLINE },
	{ "tdline", SHARED_TDLINE	},
	{ "pdline", SHARED_PDLINE   },
	{ "undline",    SHARED_UNDLINE  },
	{ "tkline",	SHARED_TKLINE	},
	{ "unkline",	SHARED_UNKLINE	},
	{ "txline",	SHARED_TXLINE	},
	{ "unxline",	SHARED_UNXLINE	},
	{ "tresv",	SHARED_TRESV	},
	{ "unresv",	SHARED_UNRESV	},
	{ "locops",	SHARED_LOCOPS	},
	{ "rehash",	SHARED_REHASH	},
	{ "all",	SHARED_ALL	},
	{ "none",	0		},
	{NULL, 0}
};
/* *INDENT-ON* */

static int
find_umode(struct mode_table *tab, const char *name)
{
	int i;

	for (i = 0; tab[i].name; i++)
	{
		if(strcmp(tab[i].name, name) == 0)
			return tab[i].mode;
	}

	return -1;
}

static void
set_modes_from_table(int *modes, const char *whatis, struct mode_table *tab, conf_parm_t * args)
{
	for (; args; args = args->next)
	{
		const char *umode;
		int dir = 1;
		int mode;

		if((args->type & CF_MTYPE) != CF_STRING)
		{
			conf_report_error("Warning -- %s is not a string; ignoring.", whatis);
			continue;
		}

		umode = args->v.string;

		if(*umode == '~')
		{
			dir = 0;
			umode++;
		}

		mode = find_umode(tab, umode);

		if(mode == -1)
		{
			conf_report_error("Warning -- unknown %s %s.", whatis, args->v.string);
			continue;
		}

		if(mode)
		{
			if(dir)
				*modes |= mode;
			else
				*modes &= ~mode;
		}
		else
			*modes = 0;
	}
}

static void
conf_set_privset_extends(void *data)
{
	yy_privset_extends = rb_strdup((char *) data);
}

static void
conf_set_privset_privs(void *data)
{
	char *privs = NULL;
	conf_parm_t *args = data;

	for (; args; args = args->next)
	{
		if (privs == NULL)
			privs = rb_strdup(args->v.string);
		else
		{
			char *privs_old = privs;

			privs = rb_malloc(strlen(privs_old) + 1 + strlen(args->v.string) + 1);
			strcpy(privs, privs_old);
			strcat(privs, " ");
			strcat(privs, args->v.string);

			rb_free(privs_old);
		}
	}

	if (privs)
	{
		if (yy_privset_extends)
		{
			struct PrivilegeSet *set = privilegeset_get(yy_privset_extends);

			if (!set)
			{
				conf_report_error("Warning -- unknown parent privilege set %s for %s; assuming defaults", yy_privset_extends, conf_cur_block_name);

				set = privilegeset_get("default");
			}

			privilegeset_extend(set, conf_cur_block_name != NULL ? conf_cur_block_name : "<unknown>", privs, 0);

			rb_free(yy_privset_extends);
			yy_privset_extends = NULL;
		}
		else
			privilegeset_set_new(conf_cur_block_name != NULL ? conf_cur_block_name : "<unknown>", privs, 0);

		rb_free(privs);
	}
}

static int
conf_begin_oper(struct TopConf *tc)
{
	rb_dlink_node *ptr;
	rb_dlink_node *next_ptr;

	if(yy_oper != NULL)
	{
		free_oper_conf(yy_oper);
		yy_oper = NULL;
	}

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, yy_oper_list.head)
	{
		free_oper_conf(ptr->data);
		rb_dlinkDestroy(ptr, &yy_oper_list);
	}

	yy_oper = make_oper_conf();
	yy_oper->flags |= OPER_ENCRYPTED;

	return 0;
}

static int
conf_end_oper(struct TopConf *tc)
{
	struct oper_conf *yy_tmpoper;
	rb_dlink_node *ptr;
	rb_dlink_node *next_ptr;

	if(conf_cur_block_name != NULL)
	{
		if(strlen(conf_cur_block_name) > OPERNICKLEN)
			conf_cur_block_name[OPERNICKLEN] = '\0';

		yy_oper->name = rb_strdup(conf_cur_block_name);
	}

	if(EmptyString(yy_oper->name))
	{
		conf_report_error("Ignoring operator block -- missing name.");
		return 0;
	}

#ifdef HAVE_LIBCRYPTO
	if(EmptyString(yy_oper->passwd) && EmptyString(yy_oper->rsa_pubkey_file))
#else
	if(EmptyString(yy_oper->passwd))
#endif
	{
		conf_report_error("Ignoring operator block for %s -- missing password",
					yy_oper->name);
		return 0;
	}


	if (!yy_oper->privset)
		yy_oper->privset = privilegeset_get("default");

	/* now, yy_oper_list contains a stack of oper_conf's with just user
	 * and host in, yy_oper contains the rest of the information which
	 * we need to copy into each element in yy_oper_list
	 */
	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, yy_oper_list.head)
	{
		yy_tmpoper = ptr->data;

		yy_tmpoper->name = rb_strdup(yy_oper->name);

		/* could be an rsa key instead.. */
		if(!EmptyString(yy_oper->passwd))
			yy_tmpoper->passwd = rb_strdup(yy_oper->passwd);

		yy_tmpoper->flags = yy_oper->flags;
		yy_tmpoper->umodes = yy_oper->umodes;
		yy_tmpoper->snomask = yy_oper->snomask;
		yy_tmpoper->privset = yy_oper->privset;

#ifdef HAVE_LIBCRYPTO
		if(yy_oper->rsa_pubkey_file)
		{
			BIO *file;

			if((file = BIO_new_file(yy_oper->rsa_pubkey_file, "r")) == NULL)
			{
				conf_report_error("Ignoring operator block for %s -- "
						"rsa_public_key_file cant be opened",
						yy_tmpoper->name);
				return 0;
			}
				
			yy_tmpoper->rsa_pubkey =
				(RSA *) PEM_read_bio_RSA_PUBKEY(file, NULL, 0, NULL);

			(void)BIO_set_close(file, BIO_CLOSE);
			BIO_free(file);

			if(yy_tmpoper->rsa_pubkey == NULL)
			{
				conf_report_error("Ignoring operator block for %s -- "
						"rsa_public_key_file key invalid; check syntax",
						yy_tmpoper->name);
				return 0;
			}
		}
#endif

		/* all is ok, put it on oper_conf_list */
		rb_dlinkMoveNode(ptr, &yy_oper_list, &oper_conf_list);
	}

	free_oper_conf(yy_oper);
	yy_oper = NULL;

	return 0;
}

static void
conf_set_oper_flags(void *data)
{
	conf_parm_t *args = data;

	set_modes_from_table(&yy_oper->flags, "flag", oper_table, args);
}

static void
conf_set_oper_fingerprint(void *data)
{
	yy_oper->certfp = rb_strdup((char *) data);
}

static void
conf_set_oper_privset(void *data)
{
	yy_oper->privset = privilegeset_get((char *) data);
}

static void
conf_set_oper_user(void *data)
{
	struct oper_conf *yy_tmpoper;
	char *p;
	char *host = (char *) data;

	yy_tmpoper = make_oper_conf();

	if((p = strchr(host, '@')))
	{
		*p++ = '\0';

		yy_tmpoper->username = rb_strdup(host);
		yy_tmpoper->host = rb_strdup(p);
	}
	else
	{

		yy_tmpoper->username = rb_strdup("*");
		yy_tmpoper->host = rb_strdup(host);
	}

	if(EmptyString(yy_tmpoper->username) || EmptyString(yy_tmpoper->host))
	{
		conf_report_error("Ignoring user -- missing username/host");
		free_oper_conf(yy_tmpoper);
		return;
	}

	rb_dlinkAddAlloc(yy_tmpoper, &yy_oper_list);
}

static void
conf_set_oper_password(void *data)
{
	if(yy_oper->passwd)
	{
		memset(yy_oper->passwd, 0, strlen(yy_oper->passwd));
		rb_free(yy_oper->passwd);
	}

	yy_oper->passwd = rb_strdup((char *) data);
}

static void
conf_set_oper_rsa_public_key_file(void *data)
{
#ifdef HAVE_LIBCRYPTO
	rb_free(yy_oper->rsa_pubkey_file);
	yy_oper->rsa_pubkey_file = rb_strdup((char *) data);
#else
	conf_report_error("Warning -- ignoring rsa_public_key_file (OpenSSL support not available");
#endif
}

static void
conf_set_oper_umodes(void *data)
{
	set_modes_from_table(&yy_oper->umodes, "umode", umode_table, data);
}

static void
conf_set_oper_snomask(void *data)
{
	yy_oper->snomask = parse_snobuf_to_mask(0, (const char *) data);
}

static int
conf_begin_class(struct TopConf *tc)
{
	if(yy_class)
		free_class(yy_class);

	yy_class = make_class();
	return 0;
}

static int
conf_end_class(struct TopConf *tc)
{
	if(conf_cur_block_name != NULL)
		yy_class->class_name = rb_strdup(conf_cur_block_name);

	if(EmptyString(yy_class->class_name))
	{
		conf_report_error("Ignoring connect block -- missing name.");
		return 0;
	}

	add_class(yy_class);
	yy_class = NULL;
	return 0;
}

static void
conf_set_class_ping_time(void *data)
{
	yy_class->ping_freq = *(unsigned int *) data;
}

static void
conf_set_class_cidr_ipv4_bitlen(void *data)
{
	unsigned int maxsize = 32;
	if(*(unsigned int *) data > maxsize)
		conf_report_error
			("class::cidr_ipv4_bitlen argument exceeds maxsize (%d > %d) - ignoring.",
			 *(unsigned int *) data, maxsize);
	else
		yy_class->cidr_ipv4_bitlen = *(unsigned int *) data;

}

#ifdef RB_IPV6
static void
conf_set_class_cidr_ipv6_bitlen(void *data)
{
	unsigned int maxsize = 128;
	if(*(unsigned int *) data > maxsize)
		conf_report_error
			("class::cidr_ipv6_bitlen argument exceeds maxsize (%d > %d) - ignoring.",
			 *(unsigned int *) data, maxsize);
	else
		yy_class->cidr_ipv6_bitlen = *(unsigned int *) data;

}
#endif

static void
conf_set_class_number_per_cidr(void *data)
{
	yy_class->cidr_amount = *(unsigned int *) data;
}

static void
conf_set_class_number_per_ip(void *data)
{
	yy_class->max_local = *(unsigned int *) data;
}


static void
conf_set_class_number_per_ip_global(void *data)
{
	yy_class->max_global = *(unsigned int *) data;
}

static void
conf_set_class_number_per_ident(void *data)
{
	yy_class->max_ident = *(unsigned int *) data;
}

static void
conf_set_class_connectfreq(void *data)
{
	yy_class->con_freq = *(unsigned int *) data;
}

static void
conf_set_class_max_number(void *data)
{
	yy_class->max_total = *(unsigned int *) data;
}

static void
conf_set_class_sendq(void *data)
{
	yy_class->max_sendq = *(unsigned int *) data;
}

static char *listener_address;

static int
conf_begin_listen(struct TopConf *tc)
{
	rb_free(listener_address);
	listener_address = NULL;
	return 0;
}

static int
conf_end_listen(struct TopConf *tc)
{
	rb_free(listener_address);
	listener_address = NULL;
	return 0;
}



static void
conf_set_listen_port_both(void *data, int ssl)
{
	conf_parm_t *args = data;
	for (; args; args = args->next)
	{
		if((args->type & CF_MTYPE) != CF_INT)
		{
			conf_report_error
				("listener::port argument is not an integer " "-- ignoring.");
			continue;
		}
                if(listener_address == NULL)
                {
			add_listener(args->v.number, listener_address, AF_INET, ssl);
#ifdef RB_IPV6
			add_listener(args->v.number, listener_address, AF_INET6, ssl);
#endif
                }
		else
                {
			int family;
#ifdef RB_IPV6
			if(strchr(listener_address, ':') != NULL)
				family = AF_INET6;
			else 
#endif
				family = AF_INET;
		
			add_listener(args->v.number, listener_address, family, ssl);
                
                }

	}
}

static void
conf_set_listen_port(void *data)
{
	conf_set_listen_port_both(data, 0);
}

static void
conf_set_listen_sslport(void *data)
{
	conf_set_listen_port_both(data, 1);
}

static void
conf_set_listen_address(void *data)
{
	rb_free(listener_address);
	listener_address = rb_strdup(data);
}

static int
conf_begin_auth(struct TopConf *tc)
{
	rb_dlink_node *ptr;
	rb_dlink_node *next_ptr;

	if(yy_aconf)
		free_conf(yy_aconf);

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, yy_aconf_list.head)
	{
		free_conf(ptr->data);
		rb_dlinkDestroy(ptr, &yy_aconf_list);
	}

	yy_aconf = make_conf();
	yy_aconf->status = CONF_CLIENT;

	return 0;
}

static int
conf_end_auth(struct TopConf *tc)
{
	struct ConfItem *yy_tmp, *found_conf;
	rb_dlink_node *ptr;
	rb_dlink_node *next_ptr;

	if(EmptyString(yy_aconf->info.name))
		yy_aconf->info.name = rb_strdup("NOMATCH");

	/* didnt even get one ->host? */
	if(EmptyString(yy_aconf->host))
	{
		conf_report_error("Ignoring auth block -- missing user@host");
		return 0;
	}

	/* so the stacking works in order.. */
	collapse(yy_aconf->user);
	collapse(yy_aconf->host);
	conf_add_class_to_conf(yy_aconf);
	if ((found_conf = find_exact_conf_by_address("*", CONF_CLIENT, "*")) && found_conf->spasswd == NULL)
		conf_report_error("Ignoring redundant auth block (after *@*)");
	else if ((found_conf = find_exact_conf_by_address(yy_aconf->host, CONF_CLIENT, yy_aconf->user)) &&
			(!found_conf->spasswd || (yy_aconf->spasswd &&
			    0 == irccmp(found_conf->spasswd, yy_aconf->spasswd))))
		conf_report_error("Ignoring duplicate auth block for %s@%s",
				yy_aconf->user, yy_aconf->host);
	else
		add_conf_by_address(yy_aconf->host, CONF_CLIENT, yy_aconf->user, yy_aconf->spasswd, yy_aconf);

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, yy_aconf_list.head)
	{
		yy_tmp = ptr->data;

		if(yy_aconf->passwd)
			yy_tmp->passwd = rb_strdup(yy_aconf->passwd);

		if(yy_aconf->spasswd)
			yy_tmp->spasswd = rb_strdup(yy_aconf->spasswd);
		
		/* this will always exist.. */
		yy_tmp->info.name = rb_strdup(yy_aconf->info.name);

		if(yy_aconf->className)
			yy_tmp->className = rb_strdup(yy_aconf->className);

		yy_tmp->flags = yy_aconf->flags;
		yy_tmp->port = yy_aconf->port;

		collapse(yy_tmp->user);
		collapse(yy_tmp->host);

		conf_add_class_to_conf(yy_tmp);

		if (find_exact_conf_by_address("*", CONF_CLIENT, "*"))
			conf_report_error("Ignoring redundant auth block (after *@*)");
		else if (find_exact_conf_by_address(yy_tmp->host, CONF_CLIENT, yy_tmp->user))
			conf_report_error("Ignoring duplicate auth block for %s@%s",
					yy_tmp->user, yy_tmp->host);
		else
			add_conf_by_address(yy_tmp->host, CONF_CLIENT, yy_tmp->user, yy_tmp->spasswd, yy_tmp);
		rb_dlinkDestroy(ptr, &yy_aconf_list);
	}

	yy_aconf = NULL;
	return 0;
}

static void
conf_set_auth_user(void *data)
{
	struct ConfItem *yy_tmp;
	char *p;

	/* The first user= line doesn't allocate a new conf */
	if(!EmptyString(yy_aconf->host))
	{
		yy_tmp = make_conf();
		yy_tmp->status = CONF_CLIENT;
	}
	else
		yy_tmp = yy_aconf;

	if((p = strchr(data, '@')))
	{
		*p++ = '\0';

		yy_tmp->user = rb_strdup(data);
		yy_tmp->host = rb_strdup(p);
	}
	else
	{
		yy_tmp->user = rb_strdup("*");
		yy_tmp->host = rb_strdup(data);
	}

	if(yy_aconf != yy_tmp)
		rb_dlinkAddAlloc(yy_tmp, &yy_aconf_list);
}

static void
conf_set_auth_auth_user(void *data)
{
	if(yy_aconf->spasswd)
		memset(yy_aconf->spasswd, 0, strlen(yy_aconf->spasswd));
	rb_free(yy_aconf->spasswd);
	yy_aconf->spasswd = rb_strdup(data);
}

static void
conf_set_auth_passwd(void *data)
{
	if(yy_aconf->passwd)
		memset(yy_aconf->passwd, 0, strlen(yy_aconf->passwd));
	rb_free(yy_aconf->passwd);
	yy_aconf->passwd = rb_strdup(data);
}

static void
conf_set_auth_spoof(void *data)
{
	char *p;
	char *user = NULL;
	char *host = NULL;

	host = data;

	/* user@host spoof */
	if((p = strchr(host, '@')) != NULL)
	{
		*p = '\0';
		user = data;
		host = p+1;

		if(EmptyString(user))
		{
			conf_report_error("Warning -- spoof ident empty.");
			return;
		}

		if(strlen(user) > USERLEN)
		{
			conf_report_error("Warning -- spoof ident length invalid.");
			return;
		}

		if(!valid_username(user))
		{
			conf_report_error("Warning -- invalid spoof (ident).");
			return;
		}

		/* this must be restored! */
		*p = '@';
	}

	if(EmptyString(host))
	{
		conf_report_error("Warning -- spoof host empty.");
		return;
	}

	if(strlen(host) > HOSTLEN)
	{
		conf_report_error("Warning -- spoof host length invalid.");
		return;
	}

	if(!valid_hostname(host))
	{
		conf_report_error("Warning -- invalid spoof (host).");
		return;
	}

	rb_free(yy_aconf->info.name);
	yy_aconf->info.name = rb_strdup(data);
	yy_aconf->flags |= CONF_FLAGS_SPOOF_IP;
}

static void
conf_set_auth_flags(void *data)
{
	conf_parm_t *args = data;

	set_modes_from_table((int *) &yy_aconf->flags, "flag", auth_table, args);
}

static void
conf_set_auth_redir_serv(void *data)
{
	yy_aconf->flags |= CONF_FLAGS_REDIR;
	rb_free(yy_aconf->info.name);
	yy_aconf->info.name = rb_strdup(data);
}

static void
conf_set_auth_redir_port(void *data)
{
	int port = *(unsigned int *) data;

	yy_aconf->flags |= CONF_FLAGS_REDIR;
	yy_aconf->port = port;
}

static void
conf_set_auth_class(void *data)
{
	rb_free(yy_aconf->className);
	yy_aconf->className = rb_strdup(data);
}

/* ok, shared_oper handles the stacking, shared_flags handles adding
 * things.. so all we need to do when we start and end a shared block, is
 * clean up anything thats been left over.
 */
static int
conf_cleanup_shared(struct TopConf *tc)
{
	rb_dlink_node *ptr, *next_ptr;

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, yy_shared_list.head)
	{
		free_remote_conf(ptr->data);
		rb_dlinkDestroy(ptr, &yy_shared_list);
	}

	if(yy_shared != NULL)
	{
		free_remote_conf(yy_shared);
		yy_shared = NULL;
	}

	return 0;
}

static void
conf_set_shared_oper(void *data)
{
	conf_parm_t *args = data;
	const char *username;
	char *p;

	if(yy_shared != NULL)
		free_remote_conf(yy_shared);

	yy_shared = make_remote_conf();

	if(args->next != NULL)
	{
		if((args->type & CF_MTYPE) != CF_QSTRING)
		{
			conf_report_error("Ignoring shared::oper -- server is not a qstring");
			return;
		}

		yy_shared->server = rb_strdup(args->v.string);
		args = args->next;
	}
	else
		yy_shared->server = rb_strdup("*");

	if((args->type & CF_MTYPE) != CF_QSTRING)
	{
		conf_report_error("Ignoring shared::oper -- oper is not a qstring");
		return;
	}

	if((p = strchr(args->v.string, '@')) == NULL)
	{
		conf_report_error("Ignoring shard::oper -- oper is not a user@host");
		return;
	}

	username = args->v.string;
	*p++ = '\0';

	if(EmptyString(p))
		yy_shared->host = rb_strdup("*");
	else
		yy_shared->host = rb_strdup(p);

	if(EmptyString(username))
		yy_shared->username = rb_strdup("*");
	else
		yy_shared->username = rb_strdup(username);

	rb_dlinkAddAlloc(yy_shared, &yy_shared_list);
	yy_shared = NULL;
}

static void
conf_set_shared_flags(void *data)
{
	conf_parm_t *args = data;
	int flags = 0;
	rb_dlink_node *ptr, *next_ptr;

	if(yy_shared != NULL)
		free_remote_conf(yy_shared);

	set_modes_from_table(&flags, "flag", shared_table, args);

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, yy_shared_list.head)
	{
		yy_shared = ptr->data;

		yy_shared->flags = flags;
		rb_dlinkDestroy(ptr, &yy_shared_list);
		rb_dlinkAddTail(yy_shared, &yy_shared->node, &shared_conf_list);
	}

	yy_shared = NULL;
}

static int
conf_begin_connect(struct TopConf *tc)
{
	if(yy_server)
		free_server_conf(yy_server);

	yy_server = make_server_conf();
	yy_server->port = PORTNUM;
	yy_server->flags |= SERVER_TB;

	if(conf_cur_block_name != NULL)
		yy_server->name = rb_strdup(conf_cur_block_name);

	return 0;
}

static int
conf_end_connect(struct TopConf *tc)
{
	if(EmptyString(yy_server->name))
	{
		conf_report_error("Ignoring connect block -- missing name.");
		return 0;
	}

	if(ServerInfo.name != NULL && !irccmp(ServerInfo.name, yy_server->name))
	{
		conf_report_error("Ignoring connect block for %s -- name is equal to my own name.",
				yy_server->name);
		return 0;
	}

	if(EmptyString(yy_server->passwd) || EmptyString(yy_server->spasswd))
	{
		conf_report_error("Ignoring connect block for %s -- missing password.",
					yy_server->name);
		return 0;
	}

	if(EmptyString(yy_server->host))
	{
		conf_report_error("Ignoring connect block for %s -- missing host.",
					yy_server->name);
		return 0;
	}

#ifndef HAVE_LIBZ
	if(ServerConfCompressed(yy_server))
	{
		conf_report_error("Ignoring connect::flags::compressed -- zlib not available.");
		yy_server->flags &= ~SERVER_COMPRESSED;
	}
#endif

	add_server_conf(yy_server);
	rb_dlinkAdd(yy_server, &yy_server->node, &server_conf_list);

	yy_server = NULL;
	return 0;
}

static void
conf_set_connect_host(void *data)
{
	rb_free(yy_server->host);
	yy_server->host = rb_strdup(data);
	if (strchr(yy_server->host, ':'))
		yy_server->aftype = AF_INET6;
}

static void
conf_set_connect_vhost(void *data)
{
	if(rb_inet_pton_sock(data, (struct sockaddr *)&yy_server->my_ipnum) <= 0)
	{
		conf_report_error("Invalid netmask for server vhost (%s)",
		    		  (char *) data);
		return;
	}

	yy_server->flags |= SERVER_VHOSTED;
}

static void
conf_set_connect_send_password(void *data)
{
	if(yy_server->spasswd)
	{
		memset(yy_server->spasswd, 0, strlen(yy_server->spasswd));
		rb_free(yy_server->spasswd);
	}

	yy_server->spasswd = rb_strdup(data);
}

static void
conf_set_connect_accept_password(void *data)
{
	if(yy_server->passwd)
	{
		memset(yy_server->passwd, 0, strlen(yy_server->passwd));
		rb_free(yy_server->passwd);
	}
	yy_server->passwd = rb_strdup(data);
}

static void
conf_set_connect_port(void *data)
{
	int port = *(unsigned int *) data;

	if(port < 1)
		port = PORTNUM;

	yy_server->port = port;
}

static void
conf_set_connect_aftype(void *data)
{
	char *aft = data;

	if(strcasecmp(aft, "ipv4") == 0)
		yy_server->aftype = AF_INET;
#ifdef RB_IPV6
	else if(strcasecmp(aft, "ipv6") == 0)
		yy_server->aftype = AF_INET6;
#endif
	else
		conf_report_error("connect::aftype '%s' is unknown.", aft);
}

static void
conf_set_connect_flags(void *data)
{
	conf_parm_t *args = data;

	/* note, we allow them to set compressed, then remove it later if
	 * they do and LIBZ isnt available
	 */
	set_modes_from_table(&yy_server->flags, "flag", connect_table, args);
}

static void
conf_set_connect_hub_mask(void *data)
{
	struct remote_conf *yy_hub;

	if(EmptyString(yy_server->name))
		return;

	yy_hub = make_remote_conf();
	yy_hub->flags = CONF_HUB;

	yy_hub->host = rb_strdup(data);
	yy_hub->server = rb_strdup(yy_server->name);
	rb_dlinkAdd(yy_hub, &yy_hub->node, &hubleaf_conf_list);
}

static void
conf_set_connect_leaf_mask(void *data)
{
	struct remote_conf *yy_leaf;

	if(EmptyString(yy_server->name))
		return;

	yy_leaf = make_remote_conf();
	yy_leaf->flags = CONF_LEAF;

	yy_leaf->host = rb_strdup(data);
	yy_leaf->server = rb_strdup(yy_server->name);
	rb_dlinkAdd(yy_leaf, &yy_leaf->node, &hubleaf_conf_list);
}

static void
conf_set_connect_class(void *data)
{
	rb_free(yy_server->class_name);
	yy_server->class_name = rb_strdup(data);
}

static void
conf_set_exempt_ip(void *data)
{
	struct ConfItem *yy_tmp;

	if(parse_netmask(data, NULL, NULL) == HM_HOST)
	{
		conf_report_error("Ignoring exempt -- invalid exempt::ip.");
		return;
	}

	yy_tmp = make_conf();
	yy_tmp->passwd = rb_strdup("*");
	yy_tmp->host = rb_strdup(data);
	yy_tmp->status = CONF_EXEMPTDLINE;
	add_conf_by_address(yy_tmp->host, CONF_EXEMPTDLINE, NULL, NULL, yy_tmp);
}

static int
conf_cleanup_cluster(struct TopConf *tc)
{
	rb_dlink_node *ptr, *next_ptr;

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, yy_cluster_list.head)
	{
		free_remote_conf(ptr->data);
		rb_dlinkDestroy(ptr, &yy_cluster_list);
	}

	if(yy_shared != NULL)
	{
		free_remote_conf(yy_shared);
		yy_shared = NULL;
	}

	return 0;
}

static void
conf_set_cluster_name(void *data)
{
	if(yy_shared != NULL)
		free_remote_conf(yy_shared);

	yy_shared = make_remote_conf();
	yy_shared->server = rb_strdup(data);
	rb_dlinkAddAlloc(yy_shared, &yy_cluster_list);

	yy_shared = NULL;
}

static void
conf_set_cluster_flags(void *data)
{
	conf_parm_t *args = data;
	int flags = 0;
	rb_dlink_node *ptr, *next_ptr;

	if(yy_shared != NULL)
		free_remote_conf(yy_shared);

	set_modes_from_table(&flags, "flag", cluster_table, args);

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, yy_cluster_list.head)
	{
		yy_shared = ptr->data;
		yy_shared->flags = flags;
		rb_dlinkAddTail(yy_shared, &yy_shared->node, &cluster_conf_list);
		rb_dlinkDestroy(ptr, &yy_cluster_list);
	}

	yy_shared = NULL;
}

static void
conf_set_general_havent_read_conf(void *data)
{
	if(*(unsigned int *) data)
	{
		conf_report_error("You haven't read your config file properly.");
		conf_report_error
			("There is a line in the example conf that will kill your server if not removed.");
		conf_report_error
			("Consider actually reading/editing the conf file, and removing this line.");
		if (!testing_conf)
			exit(0);
	}
}

static void
conf_set_general_hide_error_messages(void *data)
{
	char *val = data;

	if(strcasecmp(val, "yes") == 0)
		ConfigFileEntry.hide_error_messages = 2;
	else if(strcasecmp(val, "opers") == 0)
		ConfigFileEntry.hide_error_messages = 1;
	else if(strcasecmp(val, "no") == 0)
		ConfigFileEntry.hide_error_messages = 0;
	else
		conf_report_error("Invalid setting '%s' for general::hide_error_messages.", val);
}

static void
conf_set_general_kline_delay(void *data)
{
	ConfigFileEntry.kline_delay = *(unsigned int *) data;

	/* THIS MUST BE HERE to stop us being unable to check klines */
	kline_queued = 0;
}

static void
conf_set_general_stats_k_oper_only(void *data)
{
	char *val = data;

	if(strcasecmp(val, "yes") == 0)
		ConfigFileEntry.stats_k_oper_only = 2;
	else if(strcasecmp(val, "masked") == 0)
		ConfigFileEntry.stats_k_oper_only = 1;
	else if(strcasecmp(val, "no") == 0)
		ConfigFileEntry.stats_k_oper_only = 0;
	else
		conf_report_error("Invalid setting '%s' for general::stats_k_oper_only.", val);
}

static void
conf_set_general_stats_i_oper_only(void *data)
{
	char *val = data;

	if(strcasecmp(val, "yes") == 0)
		ConfigFileEntry.stats_i_oper_only = 2;
	else if(strcasecmp(val, "masked") == 0)
		ConfigFileEntry.stats_i_oper_only = 1;
	else if(strcasecmp(val, "no") == 0)
		ConfigFileEntry.stats_i_oper_only = 0;
	else
		conf_report_error("Invalid setting '%s' for general::stats_i_oper_only.", val);
}

static void
conf_set_general_compression_level(void *data)
{
#ifdef HAVE_LIBZ
	ConfigFileEntry.compression_level = *(unsigned int *) data;

	if((ConfigFileEntry.compression_level < 1) || (ConfigFileEntry.compression_level > 9))
	{
		conf_report_error
			("Invalid general::compression_level %d -- using default.",
			 ConfigFileEntry.compression_level);
		ConfigFileEntry.compression_level = 0;
	}
#else
	conf_report_error("Ignoring general::compression_level -- zlib not available.");
#endif
}

static void
conf_set_general_default_umodes(void *data)
{
	char *pm;
	int what = MODE_ADD, flag;

	ConfigFileEntry.default_umodes = 0;
	for (pm = (char *) data; *pm; pm++)
	{
		switch (*pm)
		{
		case '+':
			what = MODE_ADD;
			break;
		case '-':
			what = MODE_DEL;
			break;

		/* don't allow +o */
		case 'o':
		case 'S':
		case ' ':
			break;

		default:
			if ((flag = user_modes[(unsigned char) *pm]))
			{
				/* Proper value has probably not yet been set
				 * so don't check oper_only_umodes -- jilles */
				if (what == MODE_ADD)
					ConfigFileEntry.default_umodes |= flag;
				else
					ConfigFileEntry.default_umodes &= ~flag;
			}
			break;
		}
	}			
}

static void
conf_set_general_oper_umodes(void *data)
{
	set_modes_from_table(&ConfigFileEntry.oper_umodes, "umode", umode_table, data);
}

static void
conf_set_general_oper_only_umodes(void *data)
{
	set_modes_from_table(&ConfigFileEntry.oper_only_umodes, "umode", umode_table, data);
}

static void
conf_set_general_oper_snomask(void *data)
{
	char *pm;
	int what = MODE_ADD, flag;

	ConfigFileEntry.oper_snomask = 0;
	for (pm = (char *) data; *pm; pm++)
	{
		switch (*pm)
		{
		case '+':
			what = MODE_ADD;
			break;
		case '-':
			what = MODE_DEL;
			break;

		default:
			if ((flag = snomask_modes[(unsigned char) *pm]))
			{
				if (what == MODE_ADD)
					ConfigFileEntry.oper_snomask |= flag;
				else
					ConfigFileEntry.oper_snomask &= ~flag;
			}
			break;
		}
	}
}

static void
conf_set_serverhide_links_delay(void *data)
{
	int val = *(unsigned int *) data;

	ConfigServerHide.links_delay = val;
}

static int
conf_begin_service(struct TopConf *tc)
{
	struct Client *target_p;
	rb_dlink_node *ptr;

	RB_DLINK_FOREACH(ptr, global_serv_list.head)
	{
		target_p = ptr->data;

		target_p->flags &= ~FLAGS_SERVICE;
	}

	return 0;
}

static void
conf_set_service_name(void *data)
{
	struct Client *target_p;
	const char *s;
	char *tmp;
	int dots = 0;

	for(s = data; *s != '\0'; s++)
	{
		if(!IsServChar(*s))
		{
			conf_report_error("Ignoring service::name "
					 "-- bogus servername.");
			return;
		}
		else if(*s == '.')
			 dots++;
	}

	if(!dots)
	{
		conf_report_error("Ignoring service::name -- must contain '.'");
		return;
	}

	tmp = rb_strdup(data);
	rb_dlinkAddAlloc(tmp, &service_list);

	if((target_p = find_server(NULL, tmp)))
		target_p->flags |= FLAGS_SERVICE;
}

static int
conf_begin_alias(struct TopConf *tc)
{
	yy_alias = rb_malloc(sizeof(struct alias_entry));

	if (conf_cur_block_name != NULL)
		yy_alias->name = rb_strdup(conf_cur_block_name);

	yy_alias->flags = 0;
	yy_alias->hits = 0;

	return 0;
}

static int
conf_end_alias(struct TopConf *tc)
{
	if (yy_alias == NULL)
		return -1;

	if (yy_alias->name == NULL)
	{
		conf_report_error("Ignoring alias -- must have a name.");

		rb_free(yy_alias);

		return -1;
	}

	if (yy_alias->target == NULL)
	{
		conf_report_error("Ignoring alias -- must have a target.");

		rb_free(yy_alias);

		return -1;
	}

	irc_dictionary_add(alias_dict, yy_alias->name, yy_alias);

	return 0;
}

static void
conf_set_alias_name(void *data)
{
	if (data == NULL || yy_alias == NULL)	/* this shouldn't ever happen */
		return;

	yy_alias->name = rb_strdup(data);
}

static void
conf_set_alias_target(void *data)
{
	if (data == NULL || yy_alias == NULL)	/* this shouldn't ever happen */
		return;

	yy_alias->target = rb_strdup(data);
}

static void
conf_set_blacklist_host(void *data)
{
	yy_blacklist_host = rb_strdup(data);
}

static void
conf_set_blacklist_reason(void *data)
{
	yy_blacklist_reason = rb_strdup(data);

	if (yy_blacklist_host && yy_blacklist_reason)
	{
		new_blacklist(yy_blacklist_host, yy_blacklist_reason);
		rb_free(yy_blacklist_host);
		rb_free(yy_blacklist_reason);
		yy_blacklist_host = NULL;
		yy_blacklist_reason = NULL;
	}
}

/* public functions */


void
conf_report_error(const char *fmt, ...)
{
	va_list ap;
	char msg[IRCD_BUFSIZE + 1] = { 0 };

	va_start(ap, fmt);
	rb_vsnprintf(msg, IRCD_BUFSIZE, fmt, ap);
	va_end(ap);

	if (testing_conf)
	{
		fprintf(stderr, "\"%s\", line %d: %s\n", current_file, lineno + 1, msg);
		return;
	}

	ierror("\"%s\", line %d: %s", current_file, lineno + 1, msg);
	sendto_realops_snomask(SNO_GENERAL, L_ALL, "\"%s\", line %d: %s", current_file, lineno + 1, msg);
}

int
conf_start_block(char *block, char *name)
{
	if((conf_cur_block = find_top_conf(block)) == NULL)
	{
		conf_report_error("Configuration block '%s' is not defined.", block);
		return -1;
	}

	if(name)
		conf_cur_block_name = rb_strdup(name);
	else
		conf_cur_block_name = NULL;

	if(conf_cur_block->tc_sfunc)
		if(conf_cur_block->tc_sfunc(conf_cur_block) < 0)
			return -1;

	return 0;
}

int
conf_end_block(struct TopConf *tc)
{
	if(tc->tc_efunc)
		return tc->tc_efunc(tc);

	rb_free(conf_cur_block_name);
	return 0;
}

static void
conf_set_generic_int(void *data, void *location)
{
	*((int *) location) = *((unsigned int *) data);
}

static void
conf_set_generic_string(void *data, int len, void *location)
{
	char **loc = location;
	char *input = data;

	if(len && strlen(input) > (unsigned int)len)
		input[len] = '\0';

	rb_free(*loc);
	*loc = rb_strdup(input);
}

int
conf_call_set(struct TopConf *tc, char *item, conf_parm_t * value, int type)
{
	struct ConfEntry *cf;
	conf_parm_t *cp;

	if(!tc)
		return -1;

	if((cf = find_conf_item(tc, item)) == NULL)
	{
		conf_report_error
			("Non-existant configuration setting %s::%s.", tc->tc_name, (char *) item);
		return -1;
	}

	/* if it takes one thing, make sure they only passed one thing,
	   and handle as needed. */
	if(value->type & CF_FLIST && !cf->cf_type & CF_FLIST)
	{
		conf_report_error
			("Option %s::%s does not take a list of values.", tc->tc_name, item);
		return -1;
	}

	cp = value->v.list;


	if(CF_TYPE(value->v.list->type) != CF_TYPE(cf->cf_type))
	{
		/* if it expects a string value, but we got a yesno, 
		 * convert it back
		 */
		if((CF_TYPE(value->v.list->type) == CF_YESNO) &&
		   (CF_TYPE(cf->cf_type) == CF_STRING))
		{
			value->v.list->type = CF_STRING;

			if(cp->v.number == 1)
				cp->v.string = rb_strdup("yes");
			else
				cp->v.string = rb_strdup("no");
		}

		/* maybe it's a CF_TIME and they passed CF_INT --
		   should still be valid */
		else if(!((CF_TYPE(value->v.list->type) == CF_INT) &&
			  (CF_TYPE(cf->cf_type) == CF_TIME)))
		{
			conf_report_error
				("Wrong type for %s::%s (expected %s, got %s)",
				 tc->tc_name, (char *) item,
				 conf_strtype(cf->cf_type), conf_strtype(value->v.list->type));
			return -1;
		}
	}

	if(cf->cf_type & CF_FLIST)
	{
#if 0
		if(cf->cf_arg)
			conf_set_generic_list(value->v.list, cf->cf_arg);
		else
#endif
		/* just pass it the extended argument list */
		cf->cf_func(value->v.list);
	}
	else
	{
		/* it's old-style, needs only one arg */
		switch (cf->cf_type)
		{
		case CF_INT:
		case CF_TIME:
		case CF_YESNO:
			if(cf->cf_arg)
				conf_set_generic_int(&cp->v.number, cf->cf_arg);
			else
				cf->cf_func(&cp->v.number);
			break;
		case CF_STRING:
		case CF_QSTRING:
			if(EmptyString(cp->v.string))
				conf_report_error("Ignoring %s::%s -- empty field",
						tc->tc_name, item);
			else if(cf->cf_arg)
				conf_set_generic_string(cp->v.string, cf->cf_len, cf->cf_arg);
			else
				cf->cf_func(cp->v.string);
			break;
		}
	}


	return 0;
}

int
add_conf_item(const char *topconf, const char *name, int type, void (*func) (void *))
{
	struct TopConf *tc;
	struct ConfEntry *cf;

	if((tc = find_top_conf(topconf)) == NULL)
		return -1;

	if((cf = find_conf_item(tc, name)) != NULL)
		return -1;

	cf = rb_malloc(sizeof(struct ConfEntry));

	cf->cf_name = name;
	cf->cf_type = type;
	cf->cf_func = func;
	cf->cf_arg = NULL;

	rb_dlinkAddAlloc(cf, &tc->tc_items);

	return 0;
}

int
remove_conf_item(const char *topconf, const char *name)
{
	struct TopConf *tc;
	struct ConfEntry *cf;
	rb_dlink_node *ptr;

	if((tc = find_top_conf(topconf)) == NULL)
		return -1;

	if((cf = find_conf_item(tc, name)) == NULL)
		return -1;

	if((ptr = rb_dlinkFind(cf, &tc->tc_items)) == NULL)
		return -1;

	rb_dlinkDestroy(ptr, &tc->tc_items);
	rb_free(cf);

	return 0;
}

/* *INDENT-OFF* */
static struct ConfEntry conf_serverinfo_table[] =
{
	{ "description", 	CF_QSTRING, NULL, 0, &ServerInfo.description	},
	{ "network_desc", 	CF_QSTRING, NULL, 0, &ServerInfo.network_desc	},
	{ "hub", 		CF_YESNO,   NULL, 0, &ServerInfo.hub		},

	{ "network_name", 	CF_QSTRING, conf_set_serverinfo_network_name,	0, NULL },
	{ "name", 		CF_QSTRING, conf_set_serverinfo_name,	0, NULL },
	{ "sid", 		CF_QSTRING, conf_set_serverinfo_sid,	0, NULL },
	{ "vhost", 		CF_QSTRING, conf_set_serverinfo_vhost,	0, NULL },
	{ "vhost6", 		CF_QSTRING, conf_set_serverinfo_vhost6,	0, NULL },

	{ "ssl_private_key",    CF_QSTRING, NULL, 0, &ServerInfo.ssl_private_key },
	{ "ssl_ca_cert",        CF_QSTRING, NULL, 0, &ServerInfo.ssl_ca_cert },
	{ "ssl_cert",           CF_QSTRING, NULL, 0, &ServerInfo.ssl_cert },   
	{ "ssl_dh_params",      CF_QSTRING, NULL, 0, &ServerInfo.ssl_dh_params },
	{ "ssld_count",		CF_INT,	    NULL, 0, &ServerInfo.ssld_count },

	{ "default_max_clients",CF_INT,     NULL, 0, &ServerInfo.default_max_clients },

	{ "\0",	0, NULL, 0, NULL }
};

static struct ConfEntry conf_admin_table[] =
{
	{ "name",	CF_QSTRING, NULL, 200, &AdminInfo.name		},
	{ "description",CF_QSTRING, NULL, 200, &AdminInfo.description	},
	{ "email",	CF_QSTRING, NULL, 200, &AdminInfo.email		},
	{ "\0",	0, NULL, 0, NULL }
};

static struct ConfEntry conf_log_table[] =
{
	{ "fname_userlog", 	CF_QSTRING, NULL, MAXPATHLEN, &ConfigFileEntry.fname_userlog	},
	{ "fname_fuserlog", 	CF_QSTRING, NULL, MAXPATHLEN, &ConfigFileEntry.fname_fuserlog	},
	{ "fname_operlog", 	CF_QSTRING, NULL, MAXPATHLEN, &ConfigFileEntry.fname_operlog	},
	{ "fname_foperlog", 	CF_QSTRING, NULL, MAXPATHLEN, &ConfigFileEntry.fname_foperlog	},
	{ "fname_serverlog", 	CF_QSTRING, NULL, MAXPATHLEN, &ConfigFileEntry.fname_serverlog	},
	{ "fname_killlog", 	CF_QSTRING, NULL, MAXPATHLEN, &ConfigFileEntry.fname_killlog	},
	{ "fname_klinelog", 	CF_QSTRING, NULL, MAXPATHLEN, &ConfigFileEntry.fname_klinelog	},
	{ "fname_operspylog", 	CF_QSTRING, NULL, MAXPATHLEN, &ConfigFileEntry.fname_operspylog	},
	{ "fname_ioerrorlog", 	CF_QSTRING, NULL, MAXPATHLEN, &ConfigFileEntry.fname_ioerrorlog },
	{ "\0",			0,	    NULL, 0,          NULL }
};

static struct ConfEntry conf_operator_table[] =
{
	{ "rsa_public_key_file",  CF_QSTRING, conf_set_oper_rsa_public_key_file, 0, NULL },
	{ "flags",	CF_STRING | CF_FLIST, conf_set_oper_flags,	0, NULL },
	{ "umodes",	CF_STRING | CF_FLIST, conf_set_oper_umodes,	0, NULL },
	{ "privset",	CF_QSTRING, conf_set_oper_privset,	0, NULL },
	{ "snomask",    CF_QSTRING, conf_set_oper_snomask,      0, NULL },
	{ "user",	CF_QSTRING, conf_set_oper_user,		0, NULL },
	{ "password",	CF_QSTRING, conf_set_oper_password,	0, NULL },
	{ "fingerprint",	CF_QSTRING, conf_set_oper_fingerprint,	0, NULL },
	{ "\0",	0, NULL, 0, NULL }
};

static struct ConfEntry conf_privset_table[] =
{
	{ "extends",	CF_QSTRING,		conf_set_privset_extends,	0, NULL },
	{ "privs",	CF_STRING | CF_FLIST,	conf_set_privset_privs,		0, NULL },
	{ "\0", 0, NULL, 0, NULL }
};

static struct ConfEntry conf_class_table[] =
{
	{ "ping_time", 		CF_TIME, conf_set_class_ping_time,		0, NULL },
	{ "cidr_ipv4_bitlen",	CF_INT,  conf_set_class_cidr_ipv4_bitlen,		0, NULL },
#ifdef RB_IPV6
	{ "cidr_ipv6_bitlen",	CF_INT,  conf_set_class_cidr_ipv6_bitlen,		0, NULL },
#endif
	{ "number_per_cidr",	CF_INT,  conf_set_class_number_per_cidr,	0, NULL },
	{ "number_per_ip",	CF_INT,  conf_set_class_number_per_ip,		0, NULL },
	{ "number_per_ip_global", CF_INT,conf_set_class_number_per_ip_global,	0, NULL },
	{ "number_per_ident", 	CF_INT,  conf_set_class_number_per_ident,	0, NULL },
	{ "connectfreq", 	CF_TIME, conf_set_class_connectfreq,		0, NULL },
	{ "max_number", 	CF_INT,  conf_set_class_max_number,		0, NULL },
	{ "sendq", 		CF_TIME, conf_set_class_sendq,			0, NULL },
	{ "\0",	0, NULL, 0, NULL }
};

static struct ConfEntry conf_auth_table[] =
{
	{ "user",	CF_QSTRING, conf_set_auth_user,		0, NULL },
	{ "auth_user",  CF_QSTRING, conf_set_auth_auth_user,    0, NULL },
	{ "password",	CF_QSTRING, conf_set_auth_passwd,	0, NULL },
	{ "class",	CF_QSTRING, conf_set_auth_class,	0, NULL },
	{ "spoof",	CF_QSTRING, conf_set_auth_spoof,	0, NULL },
	{ "redirserv",	CF_QSTRING, conf_set_auth_redir_serv,	0, NULL },
	{ "redirport",	CF_INT,     conf_set_auth_redir_port,	0, NULL },
	{ "flags",	CF_STRING | CF_FLIST, conf_set_auth_flags,	0, NULL },
	{ "\0",	0, NULL, 0, NULL }
};

static struct ConfEntry conf_connect_table[] =
{
	{ "send_password",	CF_QSTRING,   conf_set_connect_send_password,	0, NULL },
	{ "accept_password",	CF_QSTRING,   conf_set_connect_accept_password,	0, NULL },
	{ "flags",	CF_STRING | CF_FLIST, conf_set_connect_flags,	0, NULL },
	{ "host",	CF_QSTRING, conf_set_connect_host,	0, NULL },
	{ "vhost",	CF_QSTRING, conf_set_connect_vhost,	0, NULL },
	{ "port",	CF_INT,     conf_set_connect_port,	0, NULL },
	{ "aftype",	CF_STRING,  conf_set_connect_aftype,	0, NULL },
	{ "hub_mask",	CF_QSTRING, conf_set_connect_hub_mask,	0, NULL },
	{ "leaf_mask",	CF_QSTRING, conf_set_connect_leaf_mask,	0, NULL },
	{ "class",	CF_QSTRING, conf_set_connect_class,	0, NULL },
	{ "\0",	0, NULL, 0, NULL }
};

static struct ConfEntry conf_general_table[] =
{
	{ "oper_only_umodes", 	CF_STRING | CF_FLIST, conf_set_general_oper_only_umodes, 0, NULL },
	{ "oper_umodes", 	CF_STRING | CF_FLIST, conf_set_general_oper_umodes,	 0, NULL },
	{ "oper_snomask",	CF_QSTRING, conf_set_general_oper_snomask, 0, NULL },
	{ "compression_level", 	CF_INT,    conf_set_general_compression_level,	0, NULL },
	{ "havent_read_conf", 	CF_YESNO,  conf_set_general_havent_read_conf,	0, NULL },
	{ "hide_error_messages",CF_STRING, conf_set_general_hide_error_messages,0, NULL },
	{ "kline_delay", 	CF_TIME,   conf_set_general_kline_delay,	0, NULL },
	{ "stats_k_oper_only", 	CF_STRING, conf_set_general_stats_k_oper_only,	0, NULL },
	{ "stats_i_oper_only", 	CF_STRING, conf_set_general_stats_i_oper_only,	0, NULL },
	{ "default_umodes",	CF_QSTRING, conf_set_general_default_umodes, 0, NULL },

	{ "default_operstring",	CF_QSTRING, NULL, REALLEN,    &ConfigFileEntry.default_operstring },
	{ "default_adminstring",CF_QSTRING, NULL, REALLEN,    &ConfigFileEntry.default_adminstring },
	{ "servicestring",	CF_QSTRING, NULL, REALLEN,    &ConfigFileEntry.servicestring },
	{ "egdpool_path",	CF_QSTRING, NULL, MAXPATHLEN, &ConfigFileEntry.egdpool_path },
	{ "kline_reason",	CF_QSTRING, NULL, REALLEN, &ConfigFileEntry.kline_reason },
	{ "identify_service",	CF_QSTRING, NULL, REALLEN, &ConfigFileEntry.identifyservice },
	{ "identify_command",	CF_QSTRING, NULL, REALLEN, &ConfigFileEntry.identifycommand },

	{ "anti_spam_exit_message_time", CF_TIME,  NULL, 0, &ConfigFileEntry.anti_spam_exit_message_time },
	{ "disable_fake_channels",	 CF_YESNO, NULL, 0, &ConfigFileEntry.disable_fake_channels },
	{ "min_nonwildcard_simple",	 CF_INT,   NULL, 0, &ConfigFileEntry.min_nonwildcard_simple },
	{ "non_redundant_klines",	 CF_YESNO, NULL, 0, &ConfigFileEntry.non_redundant_klines },
	{ "tkline_expire_notices",	 CF_YESNO, NULL, 0, &ConfigFileEntry.tkline_expire_notices },

	{ "anti_nick_flood",	CF_YESNO, NULL, 0, &ConfigFileEntry.anti_nick_flood	},
	{ "burst_away",		CF_YESNO, NULL, 0, &ConfigFileEntry.burst_away		},
	{ "caller_id_wait",	CF_TIME,  NULL, 0, &ConfigFileEntry.caller_id_wait	},
	{ "client_exit",	CF_YESNO, NULL, 0, &ConfigFileEntry.client_exit		},
	{ "client_flood",	CF_INT,   NULL, 0, &ConfigFileEntry.client_flood	},
	{ "collision_fnc",	CF_YESNO, NULL, 0, &ConfigFileEntry.collision_fnc	},
	{ "connect_timeout",	CF_TIME,  NULL, 0, &ConfigFileEntry.connect_timeout	},
	{ "default_floodcount", CF_INT,   NULL, 0, &ConfigFileEntry.default_floodcount	},
	{ "default_ident_timeout",	CF_INT, NULL, 0, &ConfigFileEntry.default_ident_timeout		},
	{ "disable_auth",	CF_YESNO, NULL, 0, &ConfigFileEntry.disable_auth	},
	{ "dots_in_ident",	CF_INT,   NULL, 0, &ConfigFileEntry.dots_in_ident	},
	{ "failed_oper_notice",	CF_YESNO, NULL, 0, &ConfigFileEntry.failed_oper_notice	},
	{ "global_snotices",	CF_YESNO, NULL, 0, &ConfigFileEntry.global_snotices	},
	{ "hide_spoof_ips",	CF_YESNO, NULL, 0, &ConfigFileEntry.hide_spoof_ips	},
	{ "dline_with_reason",	CF_YESNO, NULL, 0, &ConfigFileEntry.dline_with_reason	},
	{ "kline_with_reason",	CF_YESNO, NULL, 0, &ConfigFileEntry.kline_with_reason	},
	{ "map_oper_only",	CF_YESNO, NULL, 0, &ConfigFileEntry.map_oper_only	},
	{ "max_accept",		CF_INT,   NULL, 0, &ConfigFileEntry.max_accept		},
	{ "max_monitor",	CF_INT,   NULL, 0, &ConfigFileEntry.max_monitor		},
	{ "max_nick_time",	CF_TIME,  NULL, 0, &ConfigFileEntry.max_nick_time	},
	{ "max_nick_changes",	CF_INT,   NULL, 0, &ConfigFileEntry.max_nick_changes	},
	{ "max_targets",	CF_INT,   NULL, 0, &ConfigFileEntry.max_targets		},
	{ "min_nonwildcard",	CF_INT,   NULL, 0, &ConfigFileEntry.min_nonwildcard	},
	{ "nick_delay",		CF_TIME,  NULL, 0, &ConfigFileEntry.nick_delay		},
	{ "no_oper_flood",	CF_YESNO, NULL, 0, &ConfigFileEntry.no_oper_flood	},
	{ "operspy_admin_only",	CF_YESNO, NULL, 0, &ConfigFileEntry.operspy_admin_only	},
	{ "operspy_dont_care_user_info", CF_YESNO, NULL, 0, &ConfigFileEntry.operspy_dont_care_user_info },
	{ "pace_wait",		CF_TIME,  NULL, 0, &ConfigFileEntry.pace_wait		},
	{ "pace_wait_simple",	CF_TIME,  NULL, 0, &ConfigFileEntry.pace_wait_simple	},
	{ "ping_cookie",	CF_YESNO, NULL, 0, &ConfigFileEntry.ping_cookie		},
	{ "reject_after_count",	CF_INT,   NULL, 0, &ConfigFileEntry.reject_after_count	},
	{ "reject_ban_time",	CF_TIME,  NULL, 0, &ConfigFileEntry.reject_ban_time	},
	{ "reject_duration",	CF_TIME,  NULL, 0, &ConfigFileEntry.reject_duration	},
	{ "throttle_count",	CF_INT,   NULL, 0, &ConfigFileEntry.throttle_count	},
	{ "throttle_duration",	CF_TIME,  NULL, 0, &ConfigFileEntry.throttle_duration	},
	{ "short_motd",		CF_YESNO, NULL, 0, &ConfigFileEntry.short_motd		},
	{ "stats_c_oper_only",	CF_YESNO, NULL, 0, &ConfigFileEntry.stats_c_oper_only	},
	{ "stats_e_disabled",	CF_YESNO, NULL, 0, &ConfigFileEntry.stats_e_disabled	},
	{ "stats_h_oper_only",	CF_YESNO, NULL, 0, &ConfigFileEntry.stats_h_oper_only	},
	{ "stats_o_oper_only",	CF_YESNO, NULL, 0, &ConfigFileEntry.stats_o_oper_only	},
	{ "stats_P_oper_only",	CF_YESNO, NULL, 0, &ConfigFileEntry.stats_P_oper_only	},
	{ "stats_y_oper_only",	CF_YESNO, NULL, 0, &ConfigFileEntry.stats_y_oper_only	},
	{ "target_change",	CF_YESNO, NULL, 0, &ConfigFileEntry.target_change	},
	{ "ts_max_delta",	CF_TIME,  NULL, 0, &ConfigFileEntry.ts_max_delta	},
	{ "use_egd",		CF_YESNO, NULL, 0, &ConfigFileEntry.use_egd		},
	{ "ts_warn_delta",	CF_TIME,  NULL, 0, &ConfigFileEntry.ts_warn_delta	},
	{ "use_whois_actually", CF_YESNO, NULL, 0, &ConfigFileEntry.use_whois_actually	},
	{ "warn_no_nline",	CF_YESNO, NULL, 0, &ConfigFileEntry.warn_no_nline	},
	{ "use_propagated_bans",CF_YESNO, NULL, 0, &ConfigFileEntry.use_propagated_bans	},
	{ "\0", 		0, 	  NULL, 0, NULL }
};

static struct ConfEntry conf_channel_table[] =
{
	{ "default_split_user_count",	CF_INT,  NULL, 0, &ConfigChannel.default_split_user_count	 },
	{ "default_split_server_count",	CF_INT,	 NULL, 0, &ConfigChannel.default_split_server_count },
	{ "burst_topicwho",	CF_YESNO, NULL, 0, &ConfigChannel.burst_topicwho	},
	{ "kick_on_split_riding", CF_YESNO, NULL, 0, &ConfigChannel.kick_on_split_riding },
	{ "knock_delay",	CF_TIME,  NULL, 0, &ConfigChannel.knock_delay		},
	{ "knock_delay_channel",CF_TIME,  NULL, 0, &ConfigChannel.knock_delay_channel	},
	{ "max_bans",		CF_INT,   NULL, 0, &ConfigChannel.max_bans		},
	{ "max_bans_large",	CF_INT,   NULL, 0, &ConfigChannel.max_bans_large	},
	{ "max_chans_per_user", CF_INT,   NULL, 0, &ConfigChannel.max_chans_per_user 	},
	{ "no_create_on_split", CF_YESNO, NULL, 0, &ConfigChannel.no_create_on_split 	},
	{ "no_join_on_split",	CF_YESNO, NULL, 0, &ConfigChannel.no_join_on_split	},
	{ "only_ascii_channels", CF_YESNO, NULL, 0, &ConfigChannel.only_ascii_channels },
	{ "use_except",		CF_YESNO, NULL, 0, &ConfigChannel.use_except		},
	{ "use_invex",		CF_YESNO, NULL, 0, &ConfigChannel.use_invex		},
	{ "use_knock",		CF_YESNO, NULL, 0, &ConfigChannel.use_knock		},
	{ "use_forward",	CF_YESNO, NULL, 0, &ConfigChannel.use_forward		},
	{ "resv_forcepart",     CF_YESNO, NULL, 0, &ConfigChannel.resv_forcepart	},
	{ "channel_target_change", CF_YESNO, NULL, 0, &ConfigChannel.channel_target_change	},
	{ "\0", 		0, 	  NULL, 0, NULL }
};

static struct ConfEntry conf_serverhide_table[] =
{
	{ "disable_hidden",	CF_YESNO, NULL, 0, &ConfigServerHide.disable_hidden	},
	{ "flatten_links",	CF_YESNO, NULL, 0, &ConfigServerHide.flatten_links	},
	{ "hidden",		CF_YESNO, NULL, 0, &ConfigServerHide.hidden		},
	{ "links_delay",	CF_TIME,  conf_set_serverhide_links_delay, 0, NULL	},
	{ "\0", 		0, 	  NULL, 0, NULL }
};
/* *INDENT-ON* */

void
newconf_init()
{
	add_top_conf("modules", NULL, NULL, NULL);
	add_conf_item("modules", "path", CF_QSTRING, conf_set_modules_path);
	add_conf_item("modules", "module", CF_QSTRING, conf_set_modules_module);

	add_top_conf("serverinfo", NULL, NULL, conf_serverinfo_table);
	add_top_conf("admin", NULL, NULL, conf_admin_table);
	add_top_conf("log", NULL, NULL, conf_log_table);
	add_top_conf("operator", conf_begin_oper, conf_end_oper, conf_operator_table);
	add_top_conf("class", conf_begin_class, conf_end_class, conf_class_table);
	add_top_conf("privset", NULL, NULL, conf_privset_table);

	add_top_conf("listen", conf_begin_listen, conf_end_listen, NULL);
	add_conf_item("listen", "port", CF_INT | CF_FLIST, conf_set_listen_port);
	add_conf_item("listen", "sslport", CF_INT | CF_FLIST, conf_set_listen_sslport);
	add_conf_item("listen", "ip", CF_QSTRING, conf_set_listen_address);
	add_conf_item("listen", "host", CF_QSTRING, conf_set_listen_address);

	add_top_conf("auth", conf_begin_auth, conf_end_auth, conf_auth_table);

	add_top_conf("shared", conf_cleanup_shared, conf_cleanup_shared, NULL);
	add_conf_item("shared", "oper", CF_QSTRING|CF_FLIST, conf_set_shared_oper);
	add_conf_item("shared", "flags", CF_STRING | CF_FLIST, conf_set_shared_flags);

	add_top_conf("connect", conf_begin_connect, conf_end_connect, conf_connect_table);

	add_top_conf("exempt", NULL, NULL, NULL);
	add_conf_item("exempt", "ip", CF_QSTRING, conf_set_exempt_ip);

	add_top_conf("cluster", conf_cleanup_cluster, conf_cleanup_cluster, NULL);
	add_conf_item("cluster", "name", CF_QSTRING, conf_set_cluster_name);
	add_conf_item("cluster", "flags", CF_STRING | CF_FLIST, conf_set_cluster_flags);

	add_top_conf("general", NULL, NULL, conf_general_table);
	add_top_conf("channel", NULL, NULL, conf_channel_table);
	add_top_conf("serverhide", NULL, NULL, conf_serverhide_table);

	add_top_conf("service", conf_begin_service, NULL, NULL);
	add_conf_item("service", "name", CF_QSTRING, conf_set_service_name);

	add_top_conf("alias", conf_begin_alias, conf_end_alias, NULL);
	add_conf_item("alias", "name", CF_QSTRING, conf_set_alias_name);
	add_conf_item("alias", "target", CF_QSTRING, conf_set_alias_target);

	add_top_conf("blacklist", NULL, NULL, NULL);
	add_conf_item("blacklist", "host", CF_QSTRING, conf_set_blacklist_host);
	add_conf_item("blacklist", "reject_reason", CF_QSTRING, conf_set_blacklist_reason);
}
