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

#include "rb_lib.h"
#include "stdinc.h"
#include "setup.h"
#include "defaults.h"
#include "ircd.h"
#include "channel.h"
#include "class.h"
#include "client.h"
#include "hash.h"
#include "match.h"
#include "ircd_signal.h"
#include "msg.h"		/* msgtab */
#include "hostmask.h"
#include "numeric.h"
#include "parse.h"
#include "restart.h"
#include "s_conf.h"
#include "logger.h"
#include "s_serv.h"		/* try_connections */
#include "s_user.h"
#include "s_stats.h"
#include "scache.h"
#include "send.h"
#include "supported.h"
#include "whowas.h"
#include "modules.h"
#include "hook.h"
#include "ircd_getopt.h"
#include "newconf.h"
#include "reject.h"
#include "s_newconf.h"
#include "cache.h"
#include "monitor.h"
#include "patchlevel.h"
#include "serno.h"
#include "sslproc.h"
#include "wsproc.h"
#include "chmode.h"
#include "privilege.h"
#include "bandbi.h"
#include "authproc.h"
#include "operhash.h"

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
struct Client me;		/* That's me */
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
bool server_state_foreground = false;
bool opers_see_all_users = false;
bool ircd_ssl_ok = false;
bool ircd_zlib_ok = true;

int testing_conf = 0;
time_t startup_time;

int default_server_capabs;

int splitmode;
int splitchecking;
int split_users;
int split_servers;
int eob_count;

const char *ircd_paths[IRCD_PATH_COUNT] = {
	[IRCD_PATH_PREFIX] = DPATH,
	[IRCD_PATH_MODULES] = MODPATH,
	[IRCD_PATH_AUTOLOAD_MODULES] = AUTOMODPATH,
	[IRCD_PATH_ETC] = ETCPATH,
	[IRCD_PATH_LOG] = LOGPATH,
	[IRCD_PATH_USERHELP] = UHPATH,
	[IRCD_PATH_OPERHELP] = HPATH,
	[IRCD_PATH_IRCD_EXEC] = SPATH,
	[IRCD_PATH_IRCD_CONF] = CPATH,
	[IRCD_PATH_IRCD_MOTD] = MPATH,
	[IRCD_PATH_IRCD_LOG] = LPATH,
	[IRCD_PATH_IRCD_PID] = PPATH,
	[IRCD_PATH_IRCD_OMOTD] = OPATH,
	[IRCD_PATH_BANDB] = DBPATH,
	[IRCD_PATH_BIN] = BINPATH,
	[IRCD_PATH_LIBEXEC] = PKGLIBEXECDIR,
};

const char *ircd_pathnames[IRCD_PATH_COUNT] = {
	[IRCD_PATH_PREFIX] = "prefix",
	[IRCD_PATH_MODULES] = "modules",
	[IRCD_PATH_AUTOLOAD_MODULES] = "autoload modules",
	[IRCD_PATH_ETC] = "config",
	[IRCD_PATH_LOG] = "log",
	[IRCD_PATH_USERHELP] = "user help",
	[IRCD_PATH_OPERHELP] = "oper help",
	[IRCD_PATH_IRCD_EXEC] = "ircd binary",
	[IRCD_PATH_IRCD_CONF] = "ircd.conf",
	[IRCD_PATH_IRCD_MOTD] = "ircd.motd",
	[IRCD_PATH_IRCD_LOG] = "ircd.log",
	[IRCD_PATH_IRCD_PID] = "ircd.pid",
	[IRCD_PATH_IRCD_OMOTD] = "oper motd",
	[IRCD_PATH_BANDB] = "bandb",
	[IRCD_PATH_BIN] = "binary dir",
	[IRCD_PATH_LIBEXEC] = "libexec dir",
};

const char *logFileName = NULL;
const char *pidFileName = NULL;

void
ircd_shutdown(const char *reason)
{
	struct Client *target_p;
	rb_dlink_node *ptr;

	RB_DLINK_FOREACH(ptr, lclient_list.head)
	{
		target_p = ptr->data;

		sendto_one(target_p, ":%s NOTICE %s :Server Terminating. %s",
			me.name, target_p->name, reason);
	}

	RB_DLINK_FOREACH(ptr, serv_list.head)
	{
		target_p = ptr->data;

		sendto_one(target_p, ":%s ERROR :Terminated by %s",
			me.name, reason);
	}

	ilog(L_MAIN, "Server Terminating. %s", reason);
	close_logfiles();

	unlink(pidFileName);
	exit(0);
}

/*
 * print_startup - print startup information
 */
static void
print_startup(int pid)
{
	int fd;

#ifndef _WIN32
	close(1);
	fd = open("/dev/null", O_RDWR);
	if (fd == -1) {
		perror("open /dev/null");
		exit(EXIT_FAILURE);
	}
	if (fd == 0)
		fd = dup(fd);
	if (fd != 1)
		abort();
#endif
	inotice("now running in %s mode from %s as pid %d ...",
	       !server_state_foreground ? "background" : "foreground",
        	ConfigFileEntry.dpath, pid);

#ifndef _WIN32
	/* let the parent process know the initialization was successful
	 * -- jilles */
	if (!server_state_foreground)
	{
		/* GCC complains on Linux if we don't check the value of write pedantically.
		 * Technically you're supposed to check the value, yes, but it probably can't fail.
		 * No, casting to void is of no use to shut the warning up. You HAVE to use the value.
		 * --Elizfaox
		 */
		if(write(0, ".", 1) < 1)
			abort();
	}
	if (dup2(1, 0) == -1)
		abort();
	if (dup2(1, 2) == -1)
		abort();
#endif
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

static int
make_daemon(void)
{
#ifndef _WIN32
	int pid;
	int pip[2];
	char c;

	if (pipe(pip) < 0)
	{
		perror("pipe");
		exit(EXIT_FAILURE);
	}
	dup2(pip[1], 0);
	close(pip[1]);
	if((pid = fork()) < 0)
	{
		perror("fork");
		exit(EXIT_FAILURE);
	}
	else if(pid > 0)
	{
		close(0);
		/* Wait for initialization to finish, successfully or
		 * unsuccessfully. Until this point the child may still
		 * write to stdout/stderr.
		 * -- jilles */
		if (read(pip[0], &c, 1) > 0)
			exit(EXIT_SUCCESS);
		else
			exit(EXIT_FAILURE);
	}

	close(pip[0]);
	setsid();
/*	fclose(stdin);
	fclose(stdout);
	fclose(stderr); */
#endif
	return 0;
}

static int printVersion = 0;

struct lgetopt myopts[] = {
	{"configfile", &ConfigFileEntry.configfile,
	 STRING, "File to use for ircd.conf"},
	{"logfile", &logFileName,
	 STRING, "File to use for ircd.log"},
	{"pidfile", &pidFileName,
	 STRING, "File to use for process ID"},
	{"foreground", &server_state_foreground,
	 YESNO, "Run in foreground (don't detach)"},
	{"version", &printVersion,
	 YESNO, "Print version and exit"},
	{"conftest", &testing_conf,
	 YESNO, "Test the configuration files and exit"},
	{"help", NULL, USAGE, "Print this text"},
	{NULL, NULL, STRING, NULL},
};

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
		sendto_realops_snomask(SNO_GENERAL, L_ALL,
				     "Got signal SIGUSR1, reloading ircd motd file");
		cache_user_motd();
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

#ifdef _WIN32
/*
 * relocate_paths
 *
 * inputs       - none
 * output       - none
 * side effects - items in ircd_paths[] array are relocated
 */
static void
relocate_paths(void)
{
	char prefix[PATH_MAX], workbuf[PATH_MAX];
	char *p;

	rb_strlcpy(prefix, rb_path_to_self(), sizeof prefix);

	ircd_paths[IRCD_PATH_IRCD_EXEC] = rb_strdup(prefix);

	/* if we're running from inside the source tree, we probably do not want to relocate any other paths */
	if (strstr(prefix, ".libs") != NULL)
		return;

	/* prefix = /home/kaniini/ircd/bin/ircd */
	p = strrchr(prefix, RB_PATH_SEPARATOR);
	if (rb_unlikely(p == NULL))
		return;
	*p = 0;

	/* prefix = /home/kaniini/ircd/bin */
	p = strrchr(prefix, RB_PATH_SEPARATOR);
	if (rb_unlikely(p == NULL))
		return;
	*p = 0;

	/* prefix = /home/kaniini/ircd */
	ircd_paths[IRCD_PATH_PREFIX] = rb_strdup(prefix);

	/* now that we have our prefix, we can relocate the other paths... */
	snprintf(workbuf, sizeof workbuf, "%s%cmodules", prefix, RB_PATH_SEPARATOR);
	ircd_paths[IRCD_PATH_MODULES] = rb_strdup(workbuf);

	snprintf(workbuf, sizeof workbuf, "%s%cmodules%cautoload", prefix, RB_PATH_SEPARATOR, RB_PATH_SEPARATOR);
	ircd_paths[IRCD_PATH_AUTOLOAD_MODULES] = rb_strdup(workbuf);

	snprintf(workbuf, sizeof workbuf, "%s%cetc", prefix, RB_PATH_SEPARATOR);
	ircd_paths[IRCD_PATH_ETC] = rb_strdup(workbuf);

	snprintf(workbuf, sizeof workbuf, "%s%clog", prefix, RB_PATH_SEPARATOR);
	ircd_paths[IRCD_PATH_LOG] = rb_strdup(workbuf);

	snprintf(workbuf, sizeof workbuf, "%s%chelp%cusers", prefix, RB_PATH_SEPARATOR, RB_PATH_SEPARATOR);
	ircd_paths[IRCD_PATH_USERHELP] = rb_strdup(workbuf);

	snprintf(workbuf, sizeof workbuf, "%s%chelp%copers", prefix, RB_PATH_SEPARATOR, RB_PATH_SEPARATOR);
	ircd_paths[IRCD_PATH_OPERHELP] = rb_strdup(workbuf);

	snprintf(workbuf, sizeof workbuf, "%s%cetc%circd.conf", prefix, RB_PATH_SEPARATOR, RB_PATH_SEPARATOR);
	ircd_paths[IRCD_PATH_IRCD_CONF] = rb_strdup(workbuf);

	snprintf(workbuf, sizeof workbuf, "%s%cetc%circd.motd", prefix, RB_PATH_SEPARATOR, RB_PATH_SEPARATOR);
	ircd_paths[IRCD_PATH_IRCD_MOTD] = rb_strdup(workbuf);

	snprintf(workbuf, sizeof workbuf, "%s%cetc%copers.motd", prefix, RB_PATH_SEPARATOR, RB_PATH_SEPARATOR);
	ircd_paths[IRCD_PATH_IRCD_OMOTD] = rb_strdup(workbuf);

	snprintf(workbuf, sizeof workbuf, "%s%cetc%cban.db", prefix, RB_PATH_SEPARATOR, RB_PATH_SEPARATOR);
	ircd_paths[IRCD_PATH_BANDB] = rb_strdup(workbuf);

	snprintf(workbuf, sizeof workbuf, "%s%cetc%circd.pid", prefix, RB_PATH_SEPARATOR, RB_PATH_SEPARATOR);
	ircd_paths[IRCD_PATH_IRCD_PID] = rb_strdup(workbuf);

	snprintf(workbuf, sizeof workbuf, "%s%clogs%circd.log", prefix, RB_PATH_SEPARATOR, RB_PATH_SEPARATOR);
	ircd_paths[IRCD_PATH_IRCD_LOG] = rb_strdup(workbuf);

	snprintf(workbuf, sizeof workbuf, "%s%cbin", prefix, RB_PATH_SEPARATOR);
	ircd_paths[IRCD_PATH_BIN] = rb_strdup(workbuf);
	ircd_paths[IRCD_PATH_LIBEXEC] = rb_strdup(workbuf);

	inotice("runtime paths:");
	for (int i = 0; i < IRCD_PATH_COUNT; i++)
	{
		inotice("  %s: %s", ircd_pathnames[i], ircd_paths[i]);
	}
}
#endif

/*
 * write_pidfile
 *
 * inputs       - filename+path of pid file
 * output       - none
 * side effects - write the pid of the ircd to filename
 */
static void
write_pidfile(const char *filename)
{
	FILE *fb;
	char buff[32];
	if((fb = fopen(filename, "w")))
	{
		unsigned int pid = (unsigned int) getpid();

		snprintf(buff, sizeof(buff), "%u\n", pid);
		if((fputs(buff, fb) == -1))
		{
			ilog(L_MAIN, "Error writing %u to pid file %s (%s)",
			     pid, filename, strerror(errno));
		}
		fclose(fb);
		return;
	}
	else
	{
		ilog(L_MAIN, "Error opening pid file %s", filename);
	}
}

/*
 * check_pidfile
 *
 * inputs       - filename+path of pid file
 * output       - none
 * side effects - reads pid from pidfile and checks if ircd is in process
 *                list. if it is, gracefully exits
 * -kre
 */
static void
check_pidfile(const char *filename)
{
	FILE *fb;
	char buff[32];
	pid_t pidfromfile;

	/* Don't do logging here, since we don't have log() initialised */
	if((fb = fopen(filename, "r")))
	{
		if(fgets(buff, 20, fb) != NULL)
		{
			pidfromfile = atoi(buff);
			if(!rb_kill(pidfromfile, 0))
			{
				printf("ircd: daemon is already running\n");
				exit(-1);
			}
		}
		fclose(fb);
	}
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
		sendto_realops_snomask(SNO_GENERAL, L_NETWIDE, "librb has called the die callback..aborting: %s", str);
		inotice("librb has called the die callback..aborting: %s", str);
	}
	else
		inotice("librb has called the die callback..aborting");

	unlink(pidFileName);
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
charybdis_main(int argc, char * const argv[])
{
	int fd;

#ifndef _WIN32
	/* Check to see if the user is running us as root, which is a nono */
	if(geteuid() == 0)
	{
		fprintf(stderr, "Don't run ircd as root!!!\n");
		return -1;
	}
#endif

#ifdef _WIN32
	relocate_paths();
#endif

	logFileName = ircd_paths[IRCD_PATH_IRCD_LOG];
	pidFileName = ircd_paths[IRCD_PATH_IRCD_PID];

	ConfigFileEntry.dpath = ircd_paths[IRCD_PATH_PREFIX];
	ConfigFileEntry.configfile = ircd_paths[IRCD_PATH_IRCD_CONF];	/* Server configuration file */
	ConfigFileEntry.connect_timeout = 30;	/* Default to 30 */

	init_sys();

	umask(077);		/* better safe than sorry --SRB */

	myargv = argv;
	parseargs(&argc, &argv, myopts);

	if(chdir(ConfigFileEntry.dpath))
	{
		fprintf(stderr, "Unable to chdir to %s: %s\n", ConfigFileEntry.dpath, strerror(errno));
		exit(EXIT_FAILURE);
	}

	rb_set_time();

	/*
	 * Setup corefile size immediately after boot -kre
	 */
	setup_corefile();

	/* initialise operhash fairly early. */
	init_operhash();

	memset(&me, 0, sizeof(me));
	memset(&meLocalUser, 0, sizeof(meLocalUser));
	me.localClient = &meLocalUser;

	/* Make sure all lists are zeroed */
	memset(&unknown_list, 0, sizeof(unknown_list));
	memset(&lclient_list, 0, sizeof(lclient_list));
	memset(&serv_list, 0, sizeof(serv_list));
	memset(&global_serv_list, 0, sizeof(global_serv_list));
	memset(&local_oper_list, 0, sizeof(local_oper_list));
	memset(&oper_list, 0, sizeof(oper_list));

	rb_dlinkAddTail(&me, &me.node, &global_client_list);

	memset(&Count, 0, sizeof(Count));
	memset(&ServerInfo, 0, sizeof(ServerInfo));
	memset(&AdminInfo, 0, sizeof(AdminInfo));
	memset(&ServerStats, 0, sizeof(struct ServerStatistics));

	if(printVersion)
	{
		printf("ircd: version %s(%s)\n", ircd_version, serno);
#ifdef CUSTOM_BRANDING
		printf("ircd: based on %s-%s\n", PACKAGE_NAME, PACKAGE_VERSION);
#endif
		printf("ircd: %s\n", rb_lib_version());
		exit(EXIT_SUCCESS);
	}

	setup_signals();

	if (testing_conf)
		server_state_foreground = true;

#ifndef _WIN32
	/* Make sure fd 0, 1 and 2 are in use -- jilles */
	do
	{
		fd = open("/dev/null", O_RDWR);
	} while (fd < 2 && fd != -1);
	if (fd > 2)
		close(fd);
	else if (fd == -1)
		exit(1);
#endif

	/* Check if there is pidfile and daemon already running */
	if(!testing_conf)
	{
		check_pidfile(pidFileName);

		if(!server_state_foreground)
			make_daemon();
		inotice("starting %s ...", ircd_version);
		inotice("%s", rb_lib_version());
	}

	/* Init the event subsystem */
	rb_lib_init(ircd_log_cb, ircd_restart_cb, ircd_die_cb, !server_state_foreground, maxconnections, DNODE_HEAP_SIZE, FD_HEAP_SIZE);
	rb_linebuf_init(LINEBUF_HEAP_SIZE);

	rb_init_prng(NULL, RB_PRNG_DEFAULT);

	seed_random(NULL);

	init_builtin_capabs();
	default_server_capabs = CAP_MASK;

	init_main_logfile();
	newconf_init();
	init_s_conf();
	init_s_newconf();
	init_hash();
	clear_scache_hash_table();	/* server cache name table */
	init_host_hash();
	clear_hash_parse();
	init_client();
	init_hook();
	init_channels();
	initclass();
	whowas_init();
	init_reject();
	init_cache();
	init_monitor();

        construct_cflags_strings();

	init_authd();		/* Start up authd. */
	init_dns();		/* Start up DNS query system */
	init_modules();		/* Start up modules system */

	privilegeset_set_new("default", "", 0);

	if (testing_conf)
		fprintf(stderr, "\nBeginning config test\n");
	read_conf_files(true);	/* cold start init conf files */

	load_all_modules(1);
	load_core_modules(1);

	init_isupport();

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
	init_uid();

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
	SetMe(&me);
	make_server(&me);
	startup_time = rb_current_time();
	add_to_client_hash(me.name, &me);
	add_to_id_hash(me.id, &me);
	me.serv->nameinfo = scache_connect(me.name, me.info, 0);

	rb_dlinkAddAlloc(&me, &global_serv_list);

	construct_umodebuf();

	check_class();
	write_pidfile(pidFileName);
	load_help();
	open_logfiles();

	configure_authd();

	ilog(L_MAIN, "Server Ready");

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
		check_splitmode_ev = rb_event_add("check_splitmode", check_splitmode, NULL, 5);

	print_startup(getpid());

	rb_lib_loop(0);

	return 0;
}
