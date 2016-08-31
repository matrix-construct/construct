/*
 *  charybdis: A slightly useful ircd.
 *  ircd.c: Starts up and runs the ircd.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2008 ircd-ratbox development team
 *  Copyright (C) 2005-2013 charybdis development team
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *  USA
 */

bool ircd::debugmode;


namespace ircd {

static void
ircd_die_cb(const char *str) __attribute__((noreturn));

/* /quote set variables */
struct SetOptions GlobalSetOptions;

/* configuration set from ircd.conf */
struct config_file_entry ConfigFileEntry;
/* server info set from ircd.conf */
struct server_info ServerInfo;
/* admin info set from ircd.conf */
struct admin_info AdminInfo;

struct Counter Count;
struct ServerStatistics ServerStats;

int maxconnections;
client::client me;		/* That's me */
struct LocalUser meLocalUser;	/* That's also part of me */

rb_dlink_list global_client_list;

/* unknown/client pointer lists */
rb_dlink_list unknown_list;        /* unknown clients ON this server only */
rb_dlink_list lclient_list;	/* local clients only ON this server */
rb_dlink_list serv_list;           /* local servers to this server ONLY */
rb_dlink_list global_serv_list;    /* global servers on the network */
rb_dlink_list local_oper_list;     /* our opers, duplicated in lclient_list */
rb_dlink_list oper_list;           /* network opers */

char * const *myargv;
volatile sig_atomic_t dorehash = false;
volatile sig_atomic_t dorehashbans = false;
volatile sig_atomic_t doremotd = false;
bool kline_queued = false;
bool server_state_foreground = true;
bool opers_see_all_users = false;
bool ircd_ssl_ok = false;
bool ircd_zlib_ok = true;

int testing_conf = 0;
//time_t startup_time;

int default_server_capabs;

int splitmode;
int splitchecking;
int split_users;
int split_servers;
int eob_count;

void
ircd_shutdown(const char *reason)
{
	client::client *target_p;
	rb_dlink_node *ptr;

	RB_DLINK_FOREACH(ptr, lclient_list.head)
	{
		target_p = (client::client *)ptr->data;

		sendto_one(target_p, ":%s NOTICE %s :Server Terminating. %s",
			me.name, target_p->name, reason);
	}

	RB_DLINK_FOREACH(ptr, serv_list.head)
	{
		target_p = (client::client *)ptr->data;

		sendto_one(target_p, ":%s ERROR :Terminated by %s",
			me.name, reason);
	}

	log::critical("Server Terminating. %s", reason);
	log::close();

	exit(0);
}

/*
 * print_startup - print startup information
 */
static void
print_startup(const pid_t &pid)
{
	log::notice("Server started @ %s pid[%d]",
	            ConfigFileEntry.dpath,
	            pid);
}

/*
 * init_sys
 *
 * inputs	- boot_daemon flag
 * output	- none
 * side effects	- if boot_daemon flag is not set, don't daemonize
 */
static void
init_sys(void)
{
#if !defined(_WIN32) && defined(RLIMIT_NOFILE) && defined(HAVE_SYS_RESOURCE_H)
	struct rlimit limit;

	if(!getrlimit(RLIMIT_NOFILE, &limit))
	{
		maxconnections = limit.rlim_cur;
		if(maxconnections <= MAX_BUFFER)
		{
			fprintf(stderr, "ERROR: Shell FD limits are too low.\n");
			fprintf(stderr, "ERROR: charybdis reserves %d FDs, shell limits must be above this\n", MAX_BUFFER);
			exit(EXIT_FAILURE);
		}
		return;
	}
#endif /* RLIMIT_FD_MAX */
	maxconnections = MAXCONNECTIONS;
}

static void
check_rehash(void *unused)
{
	/*
	 * Check to see whether we have to rehash the configuration ..
	 */
	if(dorehash)
	{
		rehash(true);
		dorehash = false;
	}

	if(dorehashbans)
	{
		rehash_bans();
		dorehashbans = false;
	}

	if(doremotd)
	{
		sendto_realops_snomask(sno::GENERAL, L_ALL,
				     "Got signal SIGUSR1, reloading ircd motd file");
		cache::motd::cache_user();
		doremotd = false;
	}
}

/*
 * initalialize_global_set_options
 *
 * inputs       - none
 * output       - none
 * side effects - This sets all global set options needed
 */
static void
initialize_global_set_options(void)
{
	memset(&GlobalSetOptions, 0, sizeof(GlobalSetOptions));
	/* memset( &ConfigFileEntry, 0, sizeof(ConfigFileEntry)); */

	GlobalSetOptions.maxclients = ServerInfo.default_max_clients;

	if(GlobalSetOptions.maxclients > (maxconnections - MAX_BUFFER) || (GlobalSetOptions.maxclients <= 0))
		GlobalSetOptions.maxclients = maxconnections - MAX_BUFFER;

	GlobalSetOptions.autoconn = 1;

	GlobalSetOptions.spam_time = MIN_JOIN_LEAVE_TIME;
	GlobalSetOptions.spam_num = MAX_JOIN_LEAVE_COUNT;

	GlobalSetOptions.floodcount = ConfigFileEntry.default_floodcount;

	split_servers = ConfigChannel.default_split_server_count;
	split_users = ConfigChannel.default_split_user_count;

	if(split_users && split_servers
	   && (ConfigChannel.no_create_on_split || ConfigChannel.no_join_on_split))
	{
		splitmode = 1;
		splitchecking = 1;
	}

	GlobalSetOptions.ident_timeout = ConfigFileEntry.default_ident_timeout;

	rb_strlcpy(GlobalSetOptions.operstring,
		ConfigFileEntry.default_operstring,
		sizeof(GlobalSetOptions.operstring));
	rb_strlcpy(GlobalSetOptions.adminstring,
		ConfigFileEntry.default_adminstring,
		sizeof(GlobalSetOptions.adminstring));

	/* memset( &ConfigChannel, 0, sizeof(ConfigChannel)); */

	/* End of global set options */

}

/*
 * initialize_server_capabs
 *
 * inputs       - none
 * output       - none
 */
static void
initialize_server_capabs(void)
{
	default_server_capabs &= ~CAP_ZIP;
}


/*
 * setup_corefile
 *
 * inputs       - nothing
 * output       - nothing
 * side effects - setups corefile to system limits.
 * -kre
 */
static void
setup_corefile(void)
{
#ifdef HAVE_SYS_RESOURCE_H
	struct rlimit rlim;	/* resource limits */

	/* Set corefilesize to maximum */
	if(!getrlimit(RLIMIT_CORE, &rlim))
	{
		rlim.rlim_cur = rlim.rlim_max;
		setrlimit(RLIMIT_CORE, &rlim);
	}
#endif
}

static void
ircd_log_cb(const char *str)
{
	ilog(L_MAIN, "librb reports: %s", str);
}

static void
ircd_restart_cb(const char *str)
{
	inotice("librb has called the restart callback: %s", str);
	restart(str);
}

/*
 * Why EXIT_FAILURE here?
 * Because if ircd_die_cb() is called it's because of a fatal
 * error inside libcharybdis, and we don't know how to handle the
 * exception, so it is logical to return a FAILURE exit code here.
 *    --nenolod
 */
static void
ircd_die_cb(const char *str)
{
	if(str != NULL)
	{
		/* Try to get the message out to currently logged in operators. */
		sendto_realops_snomask(sno::GENERAL, L_NETWIDE, "librb has called the die callback..aborting: %s", str);
		inotice("librb has called the die callback..aborting: %s", str);
	}
	else
		inotice("librb has called the die callback..aborting");

	exit(EXIT_FAILURE);
}

struct ev_entry *check_splitmode_ev = NULL;

static int
seed_with_urandom(void)
{
	unsigned int seed;
	int fd;

	fd = open("/dev/urandom", O_RDONLY);
	if(fd >= 0)
	{
		if(read(fd, &seed, sizeof(seed)) == sizeof(seed))
		{
			close(fd);
			srand(seed);
			return 1;
		}
		close(fd);
	}
	return 0;
}

static void
seed_with_clock(void)
{
 	const struct timeval *tv;
	rb_set_time();
	tv = rb_current_time_tv();
	srand(tv->tv_sec ^ (tv->tv_usec | (getpid() << 20)));
}

static void
seed_random(void *unused)
{
	unsigned int seed;
	if(rb_get_random(&seed, sizeof(seed)) == -1)
	{
		if(!seed_with_urandom())
			seed_with_clock();
		return;
	}
	srand(seed);
}

/*
 * main
 *
 * Initializes the IRCd.
 *
 * Inputs       - number of commandline args, args themselves
 * Outputs      - none
 * Side Effects - this is where the ircd gets going right now
 */
int
run()
{
	init_sys();
	rb_set_time();

	/*
	 * Setup corefile size immediately after boot -kre
	 */
	setup_corefile();

	/* initialise operhash fairly early. */
	init_operhash();

	me.localClient = &meLocalUser;
	rb_dlinkAddTail(&me, &me.node, &global_client_list);

	memset(&Count, 0, sizeof(Count));
	memset(&ServerInfo, 0, sizeof(ServerInfo));
	memset(&AdminInfo, 0, sizeof(AdminInfo));
	memset(&ServerStats, 0, sizeof(struct ServerStatistics));

	setup_signals();

	server_state_foreground = true;

	/* Check if there is pidfile and daemon already running */
	if(!testing_conf)
	{
		inotice("starting %s ...", info::version.c_str());
		inotice("%s", rb_lib_version());
	}

	/* Init the event subsystem */
	rb_lib_init(ircd_log_cb, ircd_restart_cb, ircd_die_cb, false, maxconnections, DNODE_HEAP_SIZE, FD_HEAP_SIZE);
	rb_linebuf_init(LINEBUF_HEAP_SIZE);

	rb_init_prng(NULL, RB_PRNG_DEFAULT);

	seed_random(NULL);

	init_builtin_capabs();
	default_server_capabs = CAP_MASK;

	newconf_init();
	init_s_conf();
	init_s_newconf();

	log::init();
	log::mark("log started");

	init_hash();
	init_host_hash();
	client::init();
	init_hook();
	chan::init();
	initclass();
	whowas::init();
	init_reject();
	cache::init();
	init_monitor();
	chan::mode::init();

	init_authd();		/* Start up authd. */
	init_dns();		/* Start up DNS query system */

	privilegeset_set_new("default", "", 0);

	if (testing_conf)
		fprintf(stderr, "\nBeginning config test\n");
	read_conf_files(true);	/* cold start init conf files */

	mods::autoload();

	supported::init();

	init_bandb();
	init_ssld();
	init_wsockd();

	rehash_bans();

	initialize_server_capabs();	/* Set up default_server_capabs */
	initialize_global_set_options();

	if(ServerInfo.name == NULL)
	{
		ierror("no server name specified in serverinfo block.");
		return -1;
	}
	rb_strlcpy(me.name, ServerInfo.name, sizeof(me.name));

	if(ServerInfo.sid[0] == '\0')
	{
		ierror("no server sid specified in serverinfo block.");
		return -2;
	}
	rb_strlcpy(me.id, ServerInfo.sid, sizeof(me.id));
	client::init_uid();

	/* serverinfo{} description must exist.  If not, error out. */
	if(ServerInfo.description == NULL)
	{
		ierror("no server description specified in serverinfo block.");
		return -3;
	}
	rb_strlcpy(me.info, ServerInfo.description, sizeof(me.info));

	if(ServerInfo.ssl_cert != NULL)
	{
		/* just do the rb_setup_ssl_server to validate the config */
		if(!rb_setup_ssl_server(ServerInfo.ssl_cert, ServerInfo.ssl_private_key, ServerInfo.ssl_dh_params, ServerInfo.ssl_cipher_list))
		{
			ilog(L_MAIN, "WARNING: Unable to setup SSL.");
			ircd_ssl_ok = false;
		}
		else
			ircd_ssl_ok = true;
	}

	if (testing_conf)
	{
		fprintf(stderr, "\nConfig testing complete.\n");
		fflush(stderr);
		return 0;	/* Why? We want the launcher to exit out. */
	}

	me.from = &me;
	me.servptr = &me;
	set_me(me);
	make_serv(me);
	add_to_client_hash(me.name, &me);
	add_to_id_hash(me.id, &me);
	nameinfo(serv(me)) = cache::serv::connect(me.name, me.info);

	rb_dlinkAddAlloc(&me, &global_serv_list);

	check_class();
	cache::help::load();

	log::open();

	configure_authd();

	/* We want try_connections to be called as soon as possible now! -- adrian */
	/* No, 'cause after a restart it would cause all sorts of nick collides */
	/* um.  by waiting even longer, that just means we have even *more*
	 * nick collisions.  what a stupid idea. set an event for the IO loop --fl
	 */
	rb_event_addish("try_connections", try_connections, NULL, STARTUP_CONNECTIONS_TIME);
	rb_event_addonce("try_connections_startup", try_connections, NULL, 2);
	rb_event_add("check_rehash", check_rehash, NULL, 3);
	rb_event_addish("reseed_srand", seed_random, NULL, 300); /* reseed every 10 minutes */

	if(splitmode)
		check_splitmode_ev = rb_event_add("check_splitmode", chan::check_splitmode, NULL, 5);

	print_startup(getpid());

	rb_lib_loop(0);

	return 0;
}

} // namespace ircd
