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

#include <ircd/asio.h>

namespace ircd
{
	extern const uint boost_version[3];

	enum runlevel _runlevel;
	const enum runlevel &runlevel{_runlevel};
	runlevel_handler runlevel_changed;
	boost::asio::io_service *ios;                // user's io service
	struct strand *strand;               // our main strand
	ctx::ctx *main_context;
	bool debugmode;

	void set_runlevel(const enum runlevel &);
	void enable_coredumps();
	void at_main_exit() noexcept;
	void main();
}

const std::thread::id
ircd::static_thread_id
{
	std::this_thread::get_id()
};

std::thread::id
ircd::thread_id
{};

void
ircd::init(boost::asio::io_service &ios,
           runlevel_handler function)
{
	init(ios, std::string{}, std::move(function));
}

///
/// Sets up the IRCd and its main context, then returns without blocking.
//
/// Pass your io_service instance, it will share it with the rest of your program.
/// An exception will be thrown on error.
///
/// This function will setup the main program loop of libircd. The execution will
/// occur when your io_service.run() or poll() is further invoked.
///
void
ircd::init(boost::asio::io_service &ios,
           const std::string &configfile,
           runlevel_handler runlevel_changed)
try
{
	// cores are not dumped without consent of the user to maintain the privacy
	// of cryptographic key material in memory at the time of the crash.
	if(RB_DEBUG_LEVEL || debugmode)
		enable_coredumps();

	assert(runlevel == runlevel::STOPPED);

	ircd::thread_id = std::this_thread::get_id();
	ircd::ios = &ios;
	ircd::strand = new struct strand(ios);

	// The log is available, but it is console-only until conf opens files.
	log::init();
	log::mark("START");

	ircd::context main_context
	{
		"main", 256_KiB, &ircd::main, context::POST
	};

	ircd::main_context = main_context.detach();
	ircd::runlevel_changed = std::move(runlevel_changed);
	log::info("%s. boost %u.%u.%u. rocksdb %s. sodium %s.",
	          PACKAGE_STRING,
	          boost_version[0],
	          boost_version[1],
	          boost_version[2],
	          db::version,
	          nacl::version());

	log::info("%s %ld %s. configured: %s. compiled: %s %s",
	          BRANDING_VERSION,
	          __cplusplus,
	          __VERSION__,
	          RB_DATE_CONFIGURED,
	          __TIMESTAMP__,
	          RB_DEBUG_LEVEL? "(DEBUG MODE)" : "");

	ircd::set_runlevel(runlevel::READY);
}
catch(const std::exception &e)
{
	delete strand;
	strand = nullptr;
	throw;
}

///
/// Notifies IRCd to shutdown. A shutdown will occur asynchronously and this
/// function will return immediately. main_exit_cb will be called when IRCd
/// has no more work for the ios (main_exit_cb will be the last operation from
/// IRCd posted to the ios).
///
/// This function is the proper way to shutdown libircd after an init(), and while
/// your io_service.run() is invoked without stopping your io_service shared by
/// other activities unrelated to libircd. If your io_service has no other activities
/// the run() will then return.
///
/// This is useful when your other activities prevent run() from returning.
///
bool
ircd::stop()
noexcept
{
	if(runlevel != runlevel::RUN)
		return false;

	if(!main_context)
		return false;

	ctx::notify(*main_context);
	return true;
}

/// Main context; Main program loop. Do not call this function directly.
///
/// This function manages the lifetime for all resources and subsystems
/// that don't/can't have their own static initialization. When this
/// function is entered, subsystem init objects are constructed on the
/// frame. The lifetime of those objects is the handle to the lifetime
/// of the subsystem, so destruction will shut down that subsystem.
///
/// The status of this function and IRCd overall can be observed through
/// the ircd::runlevel. The ircd::runlevel_changed callback can be set
/// to be notified on a runlevel change. The user should wait for a runlevel
/// of STOPPED before destroying IRCd related resources and stopping their
/// io_service from running more jobs.
///
void
ircd::main()
try
{
	// When this function is entered IRCd will transition to START indicating
	// that subsystems are initializing.
	ircd::set_runlevel(runlevel::START);

	// When this function completes, subsystems are done shutting down and IRCd
	// transitions to STOPPED.
	const unwind stopped{[]
	{
		at_main_exit();
		set_runlevel(runlevel::STOPPED);
	}};

	// These objects are the init()'s and fini()'s for each subsystem.
	// Appearing here ties their life to the main context. Initialization can
	// also occur in ircd::init() or static initialization itself if either are
	// more appropriate.

	ctx::ole::init _ole_;    // Thread OffLoad Engine
	nacl::init _nacl_;       // nacl crypto
	net::init _net_;         // Networking
	client::init _client_;   // Client related
	db::init _db_;           // RocksDB
	js::init _js_;           // SpiderMonkey
	m::init _matrix_;        // Matrix

	// IRCd will now transition to the RUN state indicating full functionality.
	ircd::set_runlevel(runlevel::RUN);

	// When the call to wait() below completes, IRCd exits from the RUN state
	// and enters one of the two states below depending on whether the unwind
	// is taking place normally or because of an exception.
	const unwind::nominal nominal{std::bind(&ircd::set_runlevel, runlevel::STOP)};
	const unwind::exceptional exceptional{std::bind(&ircd::set_runlevel, runlevel::FAULT)};

	// This call blocks until the main context is notified or interrupted etc.
	// Waiting here will hold open this stack with all of the above objects
	// living on it. Once this call completes this function effectively
	// executes backwards from this point and shuts down IRCd.
	ctx::wait();
}
catch(const ctx::interrupted &e)
{
	log::warning("IRCd main interrupted...");
}
catch(const std::exception &e)
{
	log::critical("IRCd terminated: %s", e.what());
	std::terminate();
}

void
ircd::at_main_exit()
noexcept
{
	strand->post([]
	{
		delete strand;
		strand = nullptr;
	});
}

void
ircd::set_runlevel(const enum runlevel &new_runlevel)
try
{
	log::debug("IRCd runlevel transition from '%s' to '%s'%s",
	           reflect(ircd::runlevel),
	           reflect(new_runlevel),
	           ircd::runlevel_changed? " (notifying user)" : "");

	ircd::_runlevel = new_runlevel;

	// This function will notify the user of the change to IRCd. The
	// notification is posted to the io_service ensuring THERE IS NO
	// CONTINUATION ON THIS STACK by the user.
	if(ircd::runlevel_changed)
		ios->post([new_runlevel]
		{
			ircd::runlevel_changed(new_runlevel);
		});

	log::notice("IRCd %s", reflect(ircd::runlevel));
}
catch(const std::exception &e)
{
	log::critical("IRCd runlevel change to '%s': %s",
	              reflect(new_runlevel),
	              e.what());

	std::terminate();
}

ircd::string_view
ircd::reflect(const enum runlevel &level)
{
	switch(level)
	{
		case runlevel::FAULT:     return "FAULT";
		case runlevel::STOPPED:   return "STOPPED";
		case runlevel::READY:     return "READY";
		case runlevel::START:     return "START";
		case runlevel::RUN:       return "RUN";
		case runlevel::STOP:      return "STOP";
	}

	return "??????";
}

const uint
ircd::boost_version[3]
{
	BOOST_VERSION / 100000,
	BOOST_VERSION / 100 % 1000,
	BOOST_VERSION % 100,
};

void
#ifdef HAVE_SYS_RESOURCE_H
ircd::enable_coredumps()
try
{
	//
	// Setup corefile size immediately after boot -kre
	//

	rlimit rlim;	// resource limits
	syscall(getrlimit, RLIMIT_CORE, &rlim);

	// Set corefilesize to maximum
	rlim.rlim_cur = rlim.rlim_max;
	syscall(setrlimit, RLIMIT_CORE, &rlim);
}
catch(const std::exception &e)
{
	std::cerr << "Failed to adjust rlimit: " << e.what() << std::endl;
}
#else
ircd::enable_coredumps()
{
}
#endif

// namespace ircd {

/* /quote set variables */
/*
struct SetOptions GlobalSetOptions;

struct Counter Count;
struct ServerStatistics ServerStats;

struct ev_entry *check_splitmode_ev;

int maxconnections;
//struct LocalUser meLocalUser;     // That's also part of me

rb_dlink_list global_client_list;
*/

/* unknown/client pointer lists */
//rb_dlink_list unknown_list;        /* unknown clients ON this server only */
//rb_dlink_list lclient_list;	/* local clients only ON this server */
//rb_dlink_list serv_list;           /* local servers to this server ONLY */
//rb_dlink_list global_serv_list;    /* global servers on the network */
//rb_dlink_list local_oper_list;     /* our opers, duplicated in lclient_list */
//rb_dlink_list oper_list;           /* network opers */

/*
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
*/

// }

/*
static void
check_rehash(void *unused)
{
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
*/

/*
 * initalialize_global_set_options
 *
 * inputs       - none
 * output       - none
 * side effects - This sets all global set options needed
 */
/*
static void
initialize_global_set_options(void)
{
	memset(&GlobalSetOptions, 0, sizeof(GlobalSetOptions));
	// memset( &ConfigFileEntry, 0, sizeof(ConfigFileEntry));

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
}
*/



	// initialise operhash fairly early.
	//init_operhash();

	//me.localClient = &meLocalUser;
	//rb_dlinkAddTail(&me, &me.node, &global_client_list);

	//init_builtin_capabs();
	//default_server_capabs = CAP_MASK;
	//newconf_init();
	//init_s_conf();
	//init_s_newconf();
	//init_hash();
	//init_host_hash();
	//client::init();
	//init_hook();
	//chan::init();
	//initclass();
	//whowas::init();
	//init_reject();
	//cache::init();
	//init_monitor();
	//chan::mode::init();
	//init_authd();		/* Start up authd. */
	//init_dns();		/* Start up DNS query system */

	//privilegeset_set_new("default", "", 0);

	//read_conf_files(true);	/* cold start init conf files */

	//mods::autoload();
	//supported::init();
	//init_bandb();
	//init_ssld();
	//init_wsockd();
	//rehash_bans();

	//default_server_capabs &= ~CAP_ZIP;  // Set up default_server_capabs
	//initialize_global_set_options();
/*
	if(ServerInfo.name == NULL)
		throw error("no server name specified in serverinfo block.");

	rb_strlcpy(me.name, ServerInfo.name, sizeof(me.name));

	if(ServerInfo.sid[0] == '\0')
		throw error("no server sid specified in serverinfo block.");

	rb_strlcpy(me.id, ServerInfo.sid, sizeof(me.id));
	//client::init_uid();

	// serverinfo{} description must exist.  If not, error out.
	if(ServerInfo.description == NULL)
		throw error("no server description specified in serverinfo block.");

	rb_strlcpy(me.info, ServerInfo.description, sizeof(me.info));
*/
	/*
	if(ServerInfo.ssl_cert != NULL)
	{
		// just do the rb_setup_ssl_server to validate the config
		if(!rb_setup_ssl_server(ServerInfo.ssl_cert, ServerInfo.ssl_private_key, ServerInfo.ssl_dh_params, ServerInfo.ssl_cipher_list))
		{
			ilog(L_MAIN, "WARNING: Unable to setup SSL.");
			ircd_ssl_ok = false;
		}
		else
			ircd_ssl_ok = true;
	}
	*/

	//me.from = &me;
	//me.servptr = &me;
	//set_me(me);
	//make_serv(me);
	//add_to_client_hash(me.name, &me);
	//add_to_id_hash(me.id, &me);
	//nameinfo(serv(me)) = cache::serv::connect(me.name, me.info);

	//rb_dlinkAddAlloc(&me, &global_serv_list);

	//check_class();
	//cache::help::load();

	//configure_authd();

	/* We want try_connections to be called as soon as possible now! -- adrian */
	/* No, 'cause after a restart it would cause all sorts of nick collides */
	/* um.  by waiting even longer, that just means we have even *more*
	 * nick collisions.  what a stupid idea. set an event for the IO loop --fl
	 */
	// rb_event_addish("try_connections", try_connections, NULL, STARTUP_CONNECTIONS_TIME);
	// rb_event_addonce("try_connections_startup", try_connections, NULL, 2);
	// rb_event_add("check_rehash", check_rehash, NULL, 3);
	// rb_event_addish("reseed_srand", seed_random, NULL, 300); /* reseed every 10 minutes */

	// if(splitmode)
	//	check_splitmode_ev = rb_event_add("check_splitmode", chan::check_splitmode, NULL, 5);
