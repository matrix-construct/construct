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

namespace ircd::run
{
	enum class level :int;
	struct changed;

	string_view reflect(const enum level &);
	bool set(const enum level &);

	extern const enum level &level;
}

/// An instance of this class registers itself to be called back when
/// the ircd::run::level has changed.
///
/// Note: Its destructor will access a list in libircd; after a callback
/// for a HALT do not unload libircd.so until destructing this object.
///
/// A static ctx::dock is also available for contexts to wait for a run::level
/// change notification.
///
struct ircd::run::changed
:instance_list<ircd::run::changed>
,std::function<void (const enum level &)>
{
	using handler = std::function<void (const enum level &)>;

	// Users on an ircd::ctx who wish to use the dock interface to wait for
	// a run::level change can directly access this static instance.
	static ctx::dock dock;

	/// The handler function will be called back for any run::level change while
	/// this instance remains in scope.
	changed(handler function);
	~changed() noexcept;
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
	HALT     = 0,    ///<   x <--   IRCd Powered off.
	READY    = 1,    ///<   |   |   Ready for user to run ios event loop.
	START    = 2,    ///<   |   |   Starting up subsystems for service.
	RUN      = 3,    ///<   O   |   IRCd in service.
	QUIT     = 4,    ///<   --> ^   Clean shutdown in progress.
	FAULT    = -1,   ///<           Unrecoverable fault.
};
