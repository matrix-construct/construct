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
	enum runlevel _runlevel {runlevel::HALT};    // Current libircd runlevel
	const enum runlevel &runlevel {_runlevel};   // Observer for current RL

	bool debugmode;                              // meaningful ifdef RB_DEBUG
	bool nolisten;                               // indicates no listener binding
	bool noautomod;                              // no module loading on init
	bool checkdb;                                // check databases when opening
	bool pitrecdb;                               // point-in-time recovery for db
	bool nojs;                                   // no ircd::js system init.
	bool nodirect;                               // no use of O_DIRECT.

	std::string _hostname;                       // user's supplied param
	boost::asio::io_context *ios;                // user's io service
	ctx::ctx *main_context;                      // Main program loop

	bool set_runlevel(const enum runlevel &);
	void main() noexcept;
}

/// Record of the ID of the thread static initialization took place on.
const std::thread::id
ircd::static_thread_id
{
	std::this_thread::get_id()
};

/// "main" thread for IRCd; the one the main context landed on.
std::thread::id
ircd::thread_id
{};

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
ircd::init(boost::asio::io_context &ios,
           const string_view &hostname)
try
{
	if(runlevel != runlevel::HALT)
		throw error
		{
			"Cannot init() IRCd from runlevel %s", reflect(runlevel)
		};

	// Sample the ID of this thread. Since this is the first transfer of
	// control to libircd after static initialization we have nothing to
	// consider a main thread yet. We need something set for many assertions
	// to pass until ircd::main() is entered which will reset this to where
	// ios.run() is really running.
	ircd::thread_id = std::this_thread::get_id();

	// Global ircd:: reference to the user's io_context
	ircd::ios = &ios;

	// Save the hostname param used for m::init later.
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
	ircd::set_runlevel(runlevel::READY);
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
			ircd::set_runlevel(runlevel::HALT);
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
	ircd::thread_id = std::this_thread::get_id();

	// When this function completes, subsystems are done shutting down and IRCd
	// transitions to HALT.
	const unwind halted{[]
	{
		set_runlevel(runlevel::HALT);
	}};

	// When this function is entered IRCd will transition to START indicating
	// that subsystems are initializing.
	ircd::set_runlevel(runlevel::START);

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
	m::init _matrix_         // Matrix
	{
		_hostname // the hostname string saved here by ircd::init().
	};

	// Any deinits which have to be done with all subsystems intact
	const unwind shutdown{[&]
	{
		_matrix_.close();
		server::interrupt_all();
		client::close_all();
		server::close_all();
		client::terminate_all();
		server::wait_all();
		client::wait_all();
	}};

	// When the call to wait() below completes, IRCd exits from the RUN state.
	const unwind nominal
	{
		std::bind(&ircd::set_runlevel, runlevel::QUIT)
	};

	// IRCd will now transition to the RUN state indicating full functionality.
	ircd::set_runlevel(runlevel::RUN);

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

//
// runlevel
//

/// Sets the runlevel of IRCd and notifies users. This should never be called
/// manually/directly, as it doesn't trigger a runlevel change itself, it just
/// notifies of one.
///
/// The notification will be posted to the io_context. This is important to
/// prevent the callback from continuing execution on some ircd::ctx stack and
/// instead invoke their function on the main stack in their own io_context
/// event slice.
bool
ircd::set_runlevel(const enum runlevel &new_runlevel)
try
{
	if(ircd::runlevel == new_runlevel)
		return false;

	log::debug
	{
		"IRCd runlevel transition from '%s' to '%s' (notifying %zu)",
		reflect(ircd::runlevel),
		reflect(new_runlevel),
		runlevel_changed::list.size()
	};

	ircd::_runlevel = new_runlevel;
	ircd::runlevel_changed::dock.notify_all();

	// This function will notify the user of the change to IRCd. When there
	// are listeners, function is posted to the io_context ensuring THERE IS
	// NO CONTINUATION ON THIS STACK by the user.
	const auto call_users{[new_runlevel]
	{
		assert(new_runlevel == ircd::_runlevel);

		log::notice
		{
			"IRCd %s", reflect(new_runlevel)
		};

		if(new_runlevel == runlevel::HALT)
			ircd::log::fini();
		else
			ircd::log::flush();

		for(const auto &handler : ircd::runlevel_changed::list)
			(*handler)(new_runlevel);
	}};

	if(!ircd::runlevel_changed::list.empty())
		ircd::post(call_users);
	else
		call_users();

	return true;
}
catch(const std::exception &e)
{
	log::critical
	{
		"IRCd runlevel change to '%s': %s", reflect(new_runlevel), e.what()
	};

	ircd::terminate();
	return false;
}

ircd::string_view
ircd::reflect(const enum runlevel &level)
{
	switch(level)
	{
		case runlevel::HALT:      return "HALT";
		case runlevel::READY:     return "READY";
		case runlevel::START:     return "START";
		case runlevel::RUN:       return "RUN";
		case runlevel::QUIT:      return "QUIT";
		case runlevel::FAULT:     return "FAULT";
	}

	return "??????";
}

//
// runlevel_changed
//

template<>
decltype(ircd::runlevel_changed::list)
ircd::util::instance_list<ircd::runlevel_changed>::list
{};

decltype(ircd::runlevel_changed::dock)
ircd::runlevel_changed::dock
{};

//
// runlevel_changed::runlevel_changed
//

ircd::runlevel_changed::runlevel_changed(handler function)
:handler{std::move(function)}
{}

ircd::runlevel_changed::~runlevel_changed()
noexcept
{}
