// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd
{
	std::string _origin;                         // user's supplied param
	std::string _hostname;                       // user's supplied param
	ctx::ctx *main_context;                      // Main program loop

	void main() noexcept;
}

decltype(ircd::debugmode)
ircd::debugmode
{
	{ "name",     "ircd.debugmode"  },
	{ "default",  false             },
	{ "persist",  false             },
};

/// Sets up the IRCd and its main context, then returns without blocking.
//
/// Pass your io_context instance, it will share it with the rest of your program.
/// An exception will be thrown on error.
///
/// This function will setup the main program loop of libircd. The execution will
/// occur when your io_context.run() or poll() is further invoked.
///
/// init() can only be called from a runlevel::HALT state
void
ircd::init(boost::asio::io_context &user_ios,
           const string_view &origin,
           const string_view &hostname)
try
{
	if(runlevel != runlevel::HALT)
		throw error
		{
			"Cannot init() IRCd from runlevel %s", reflect(runlevel)
		};

	ios::init(user_ios);

	// Save the params used for m::init later.
	_origin = std::string{origin};
	_hostname = std::string{hostname};

	// The log is available. but it is console-only until conf opens files.
	log::init();
	log::mark("DEADSTART"); // 6600

	// This starts off the log with library information.
	info::init();
	info::dump();

	// Setup the main context, which is a new stack executing the function
	// ircd::main(). The main_context is the first ircd::ctx to be spawned
	// and will be the last to finish.
	//
	// The context::POST will delay this spawn until the next io_context
	// event slice, so no context switch will occur here. Note that POST has
	// to be used here because: A. This init() function is executing on the
	// main stack, and context switches can only occur between context stacks,
	// not between contexts and the main stack. B. The user's io_context may or
	// may not even be running yet anyway.
	//
	ircd::context main_context
	{
		"main", 256_KiB, &ircd::main, context::POST
	};

	// The default behavior for ircd::context is to join the ctx on dtor. We
	// can't have that here because this is strictly an asynchronous function
	// on the main stack. Under normal circumstances, the mc will be entered
	// and be able to delete this pointer itself when it finishes. Otherwise
	// this must be manually deleted with assurance that mc will never enter.
	ircd::main_context = main_context.detach();

	// Finally, without prior exception, the commitment to runlevel::READY
	// is made here. The user can now invoke their ios.run(), or, if they
	// have already, IRCd will begin main execution shortly...
	ircd::runlevel_set(runlevel::READY);
}
catch(const std::exception &e)
{
	throw;
}

/// Notifies IRCd to shutdown. A shutdown will occur asynchronously and this
/// function will return immediately. A runlevel change to HALT will be
/// indicated when IRCd has no more work for the ios. When the HALT state
/// is observed the user is free to destruct all resources related to libircd.
///
/// This function is the proper way to shutdown libircd after an init(), and while
/// your io_context.run() is invoked without stopping your io_context shared by
/// other activities unrelated to libircd. If your io_context has no other activities
/// the run() will then return immediately after IRCd posts its transition to
/// the HALT state.
///
bool
ircd::quit()
noexcept
{
	log::debug
	{
		"IRCd quit requested from runlevel:%s ctx:%p main_context:%p",
		reflect(runlevel),
		(const void *)ctx::current,
		(const void *)main_context
	};

	if(main_context) switch(runlevel)
	{
		case runlevel::READY:
		{
			ctx::terminate(*main_context);
			main_context = nullptr;
			ircd::runlevel_set(runlevel::HALT);
			return true;
		}

		case runlevel::START:
		{
			ctx::terminate(*main_context);
			main_context = nullptr;
			return true;
		}

		case runlevel::RUN:
		{
			ctx::notify(*main_context);
			main_context = nullptr;
			return true;
		}

		case runlevel::HALT:
		case runlevel::QUIT:
		case runlevel::FAULT:
			return false;
	}

	return false;
}

/// Main context; Main program. Do not call this function directly.
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
/// of HALT before destroying IRCd related resources and stopping their
/// io_context from running more jobs.
///
void
ircd::main()
noexcept try
{
	// Resamples the thread this context was executed on which should be where
	// the user ran ios.run(). The user may have invoked ios.run() on multiple
	// threads, but we consider this one thread a main thread for now...
	ircd::ios::main_thread_id = std::this_thread::get_id();

	// When this function completes, subsystems are done shutting down and IRCd
	// transitions to HALT.
	const unwind halted{[]
	{
		runlevel_set(runlevel::HALT);
	}};

	// When this function is entered IRCd will transition to START indicating
	// that subsystems are initializing.
	ircd::runlevel_set(runlevel::START);

	// These objects are the init()'s and fini()'s for each subsystem.
	// Appearing here ties their life to the main context. Initialization can
	// also occur in ircd::init() or static initialization itself if either are
	// more appropriate.

	fs::init _fs_;           // Local filesystem
	magic::init _magic_;     // libmagic
	ctx::ole::init _ole_;    // Thread OffLoad Engine
	nacl::init _nacl_;       // nacl crypto
	openssl::init _ossl_;    // openssl crypto
	net::init _net_;         // Networking
	db::init _db_;           // RocksDB
	server::init _server_;   // Server related
	client::init _client_;   // Client related
	js::init _js_;           // SpiderMonkey
	ap::init _ap_;           // ActivityPub
	m::init _matrix_         // Matrix
	{
		string_view{_origin},
		string_view{_hostname}
	};

	// Any deinits which have to be done with all subsystems intact
	const unwind shutdown{[&]
	{
		_matrix_.close();
		server::interrupt_all();
		client::terminate_all();
		client::close_all();
		server::close_all();
		server::wait_all();
		client::wait_all();
	}};

	// When the call to wait() below completes, IRCd exits from the RUN state.
	const unwind nominal
	{
		std::bind(&ircd::runlevel_set, runlevel::QUIT)
	};

	// IRCd will now transition to the RUN state indicating full functionality.
	ircd::runlevel_set(runlevel::RUN);

	// This call blocks until the main context is notified or interrupted etc.
	// Waiting here will hold open this stack with all of the above objects
	// living on it. Once this call completes this function effectively
	// executes backwards from this point and shuts down IRCd.
	ctx::wait();
}
catch(const m::error &e)
{
	log::critical
	{
		"IRCd main exited :%s %s", e.what(), e.content
	};
}
catch(...)
{
	log::critical
	{
		"IRCd main exited :%s", what(std::current_exception())
	};
}

/// IRCd uptime in seconds
ircd::seconds
ircd::uptime()
{
	return seconds(ircd::time() - info::startup_time);
}
