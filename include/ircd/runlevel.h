// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#define HAVE_IRCD_IRCD_RUNLEVEL_H

namespace ircd
{
	enum class runlevel :int;
	struct runlevel_changed;

	extern const enum runlevel &runlevel;

	string_view reflect(const enum runlevel &);
	bool runlevel_set(const enum runlevel &);
}

// Forward declarations for because ctx.h is included after runlevel.h.
namespace ircd::ctx
{
	struct dock;
}

/// An instance of this class registers itself to be called back when
/// the ircd::runlevel has changed.
///
/// Note: Its destructor will access a list in libircd; after a callback
/// for a HALT do not unload libircd.so until destructing this object.
///
/// A static ctx::dock is also available for contexts to wait for a runlevel
/// change notification.
///
struct ircd::runlevel_changed
:instance_list<ircd::runlevel_changed>
,std::function<void (const enum runlevel &)>
{
	using handler = std::function<void (const enum runlevel &)>;

	// Users on an ircd::ctx who wish to use the dock interface to wait for
	// a runlevel change can directly access this static instance.
	static ctx::dock dock;

	/// The handler function will be called back for any runlevel change while
	/// this instance remains in scope.
	runlevel_changed(handler function);
	~runlevel_changed() noexcept;
};

/// The runlevel allows all observers to know the coarse state of IRCd and to
/// react accordingly. This can be used by the embedder of libircd to know
/// when it's safe to use or delete libircd resources. It is also used
/// similarly by the library and its modules.
///
/// Primary modes:
///
/// * HALT is the off mode. Nothing is/will be running in libircd until
/// an invocation of ircd::init();
///
/// * RUN is the service mode. Full client and application functionality
/// exists in this mode. Leaving the RUN mode is done with ircd::quit();
///
/// - Transitional modes: Modes which are working towards the next mode.
/// - Interventional modes:  Modes which are *not* working towards the next
/// mode and may require some user action to continue.
///
enum class ircd::runlevel
:int
{
	HALT     = 0,    ///< [inter] IRCd Powered off.
	READY    = 1,    ///< [inter] Ready for user to run ios event loop.
	START    = 2,    ///< [trans] Starting up subsystems for service.
	RUN      = 3,    ///< [inter] IRCd in service.
	QUIT     = 4,    ///< [trans] Clean shutdown in progress
	FAULT    = -1,   ///< [trans] QUIT with exception (dirty shutdown)
};
