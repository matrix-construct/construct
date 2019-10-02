// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#define HAVE_IRCD_RUN_H

// Forward declarations for because ctx.h is included after runlevel.h.
namespace ircd::ctx
{
	struct dock;
}

/// Library control details and patch-panel. This namespace contains the
/// current runlevel state for the library and provides callback interfaces
/// which can be notified for, or augment the behavior of runlevel transitions
/// (i.e when the lib is being initialized for service or shutting down, etc).
namespace ircd::run
{
	enum class level :int;
	struct changed;

	string_view reflect(const enum level &);

	// Access to the current runlevel indicator.
	extern const enum level &level;

	// Access to the desired runlevel. When this differs from the current
	// level, a command to change the runlevel has been given but not all
	// tasks have completed at the current runlevel.
	extern const enum level &chadburn;
}

/// An instance of this class registers itself to be called back when
/// the ircd::run::level has changed. The context for this callback differs
/// based on the level argument; not all invocations are on an ircd::ctx, etc.
///
/// Note: Its destructor will access a list in libircd; after a callback
/// for a HALT do not unload libircd.so until destructing this object.
///
struct ircd::run::changed
:instance_list<ircd::run::changed>
{
	using handler = std::function<void (const enum level &)>;

	static ctx::dock dock;

	handler function;

	/// The handler function will be called back for any run::level change while
	/// this instance remains in scope.
	changed(handler function) noexcept;
};

/// The run::level allows all observers to know the coarse state of IRCd and to
/// react accordingly. This can be used by the embedder of libircd to know
/// when it's safe to use or delete libircd resources. It is also used
/// similarly by the library and its modules. Only one runlevel is active at
/// any time.
///
/// * HALT is the off mode. Nothing is/will be running in libircd until
/// an invocation of ircd::init();
///
/// * READY is the state after calling ircd::init(). Leaving READY is done with
/// the user either calling their ios.run() to start executing tasks or calling
/// ircd::quit() to HALT again.
///
/// * START indicates the daemon is executing its startup procedures. Leaving
/// the START state occurs internally when there is success or a fatal error.
///
/// * IDLE indicates the library is ready for use. The user can load their
/// application in this mode before transitioning to RUN.
///
/// * RUN is the service mode. Full client and application functionality exists
/// in this mode. Leaving the RUN mode is done with ircd::quit();
///
/// * QUIT indicates the daemon is executing the shutdown procedures. This
/// will eventually return back to the HALT state.
///
/// * FAULT is a special mode indicating something really bad. The exact
/// details of this mode are ambiguous. Users do not have to handle this.
///
enum class ircd::run::level
:int
{
	FAULT    = -1,   ///<           Unrecoverable fault.
	HALT     = 0,    ///<   x <--   IRCd Powered off.
	READY    = 1,    ///<   |   |   Ready for user to run ios event loop.
	START    = 2,    ///<   |   |   Starting internal subsystems.
	IDLE     = 3,    ///<   |   |   Ready for user to load application.
	RUN      = 4,    ///<   O   |   IRCd in service.
	QUIT     = 5,    ///<   --> ^   Clean shutdown in progress.
};

inline
ircd::run::changed::changed(handler function)
noexcept
:function(std::move(function))
{}
