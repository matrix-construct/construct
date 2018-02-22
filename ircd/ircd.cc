// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <ircd/asio.h>
#include <ircd/m/m.h>

namespace ircd
{
	enum runlevel _runlevel;                     // Current libircd runlevel
	const enum runlevel &runlevel{_runlevel};    // Observer for current RL
	runlevel_handler runlevel_changed;           // user's callback

	boost::asio::io_context *ios;                // user's io service
	struct strand *strand;                       // libircd event serializer

	std::string _conf;                           // JSON read from configfile
	const std::string &conf{_conf};              // Observer for conf data

	ctx::ctx *main_context;                      // Main program loop
	bool debugmode;                              // meaningful ifdef RB_DEBUG

	std::string read_conf(std::string file);
	void set_runlevel(const enum runlevel &);
	void at_main_exit() noexcept;
	void main();
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

void
ircd::init(boost::asio::io_context &ios,
           runlevel_handler function)
{
	init(ios, std::string{}, std::move(function));
}

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
           const std::string &configfile,
           runlevel_handler runlevel_changed)
try
{
	if(runlevel != runlevel::HALT)
		throw error
		{
			"Cannot init() IRCd from runlevel %s", reflect(runlevel)
		};

	// Sample the ID of this thread. Since this is the first transfer of
	// control to libircd after static initialization we have nothing to
	// consider a main thread yet. We need something set for many assetions
	// to pass until ircd::main() is entered which will reset this to where
	// ios.run() is really running.
	ircd::thread_id = std::this_thread::get_id();

	// Global ircd:: reference to the user's io_context and setup our main
	// strand on that service.
	ircd::ios = &ios;
	ircd::strand = new struct strand(ios);

	// Saves the user's runlevel_changed callback which we invoke.
	ircd::runlevel_changed = std::move(runlevel_changed);

	// The log is available, but it is console-only until conf opens files.
	log::init();
	log::mark("READY");

	// This starts off the log with library information.
	info::init();

	// The configuration file is a user-converted Synapse homeserver.yaml
	// converted into JSON. The configuration file is only truly meaningful
	// the first time IRCd is ever run. Subsequently, only the database must
	// be found. Real configuration is stored in the !config channel.
	//
	// This subroutine reads a file either at the user-supplied path or the
	// default path specified in ircd::fs, vets for basic syntax issues, and
	// then returns a string of JSON (the file's contents). The validity of
	// the actual configuration is not known until specific subsystems are
	// init'ed later.
	//
	// *NOTE* This expects *canonical JSON* right now. That means converting
	// your homeserver.yaml may be a two step process: 1. YAML to JSON, 2.
	// whitespace-stripping the JSON. Tools to do both of these things are
	// first hits in a google search.
	ircd::_conf = read_conf(configfile);

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
	delete strand;
	strand = nullptr;
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
	if(runlevel != runlevel::RUN)
		return false;

	if(!main_context)
		return false;

	ctx::notify(*main_context);
	return true;
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
try
{
	// Resamples the thread this context was executed on which should be where
	// the user ran ios.run(). The user may have invoked ios.run() on multiple
	// threads, but we consider this one thread a main thread for now...
	ircd::thread_id = std::this_thread::get_id();

	// When this function is entered IRCd will transition to START indicating
	// that subsystems are initializing.
	ircd::set_runlevel(runlevel::START);

	// When this function completes, subsystems are done shutting down and IRCd
	// transitions to HALT.
	const unwind halted{[]
	{
		at_main_exit();
		set_runlevel(runlevel::HALT);
	}};

	// These objects are the init()'s and fini()'s for each subsystem.
	// Appearing here ties their life to the main context. Initialization can
	// also occur in ircd::init() or static initialization itself if either are
	// more appropriate.

	magic::init _magic_;     // libmagic
	fs::init _fs_;           // Local filesystem
	ctx::ole::init _ole_;    // Thread OffLoad Engine
	nacl::init _nacl_;       // nacl crypto
	openssl::init _ossl_;    // openssl crypto
	net::init _net_;         // Networking
	db::init _db_;           // RocksDB
	server::init _server_;   // Server related
	client::init _client_;   // Client related
	js::init _js_;           // SpiderMonkey
	m::init _matrix_;        // Matrix

	// Any deinits which have to be done with all subsystems intact
	const unwind shutdown{[&]
	{
		_server_.interrupt();
		_client_.interrupt();
	}};

	// When the call to wait() below completes, IRCd exits from the RUN state
	// and enters one of the two states below depending on whether the unwind
	// is taking place normally or because of an exception.
	const unwind::nominal nominal
	{
		std::bind(&ircd::set_runlevel, runlevel::QUIT)
	};

	const unwind::exceptional exceptional
	{
		std::bind(&ircd::set_runlevel, runlevel::FAULT)
	};

	// IRCd will now transition to the RUN state indicating full functionality.
	ircd::set_runlevel(runlevel::RUN);

	// This call blocks until the main context is notified or interrupted etc.
	// Waiting here will hold open this stack with all of the above objects
	// living on it. Once this call completes this function effectively
	// executes backwards from this point and shuts down IRCd.
	ctx::wait();
}
catch(const ctx::interrupted &e)
{
	log::warning
	{
		"IRCd main interrupted..."
	};
}
#ifndef RB_DEBUG
catch(const std::exception &e)
{
	// When not in debug mode this is a clean return to not crash through
	// the embedder's ios.run() which would terminate the rest of their
	// program. Instead they have the right to handle the error and try again.
	log::critical
	{
		"IRCd main exited: %s", e.what()
	};
}
#else
catch(...)
{
	// In debug mode we terminate with a message and a coredump
	ircd::terminate();
}
#endif // RB_DEBUG

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

/// Sets the runlevel of IRCd and notifies users. This should never be called
/// manually/directly, as it doesn't trigger a runlevel change itself, it just
/// notifies of one.
///
/// The notification will be posted to the io_context. This is important to
/// prevent the callback from continuing execution on some ircd::ctx stack and
/// instead invoke their function on the main stack in their own io_context
/// event slice.
void
ircd::set_runlevel(const enum runlevel &new_runlevel)
try
{
	log::debug
	{
		"IRCd runlevel transition from '%s' to '%s'%s",
		reflect(ircd::runlevel),
		reflect(new_runlevel),
		ircd::runlevel_changed? " (notifying user)" : ""
	};

	ircd::_runlevel = new_runlevel;

	// This function will notify the user of the change to IRCd. The
	// notification is posted to the io_context ensuring THERE IS NO
	// CONTINUATION ON THIS STACK by the user.
	if(ircd::runlevel_changed)
		ios->post([new_runlevel]
		{
			if(new_runlevel == runlevel::HALT)
				log::notice
				{
					"IRCd %s", reflect(new_runlevel)
				};

			ircd::log::fini();
			ircd::runlevel_changed(new_runlevel);
		});

	if(!ircd::runlevel_changed || new_runlevel != runlevel::HALT)
	{
		log::notice
		{
			"IRCd %s", reflect(new_runlevel)
		};

		ircd::log::flush();
	}
}
catch(const std::exception &e)
{
	log::critical
	{
		"IRCd runlevel change to '%s': %s", reflect(new_runlevel), e.what()
	};

	ircd::terminate();
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

std::string
ircd::read_conf(std::string filename)
try
{
	if(!filename.empty())
		log::debug
		{
			"User supplied a configuration file path: `%s'", filename
		};

	if(filename.empty())
		filename = fs::CPATH;

	if(!fs::exists(filename))
		return {};

	std::string read
	{
		fs::read(filename)
	};

	// ensure any trailing cruft is removed to not set off the validator
	if(endswith(read, '\n'))
		read.pop_back();

	if(endswith(read, '\r'))
		read.pop_back();

	// grammar check; throws on error
	json::valid(read);

	const json::object object{read};
	const size_t key_count{object.count()};
	log::info
	{
		"Using configuration from: `%s': JSON object with %zu members in %zu bytes",
		filename,
		key_count,
		read.size()
	};

	return read;
}
catch(const std::exception &e)
{
	log::error
	{
		"Configuration @ `%s': %s", filename, e.what()
	};

	throw;
}
