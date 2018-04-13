// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_VM_H

/// Matrix Virtual Machine
///
namespace ircd::m::vm
{
	enum fault :uint;
	struct error; // custom exception
	struct opts;
	struct eval;
	struct accepted;

	using fault_t = std::underlying_type<fault>::type;
	using closure = std::function<void (const event &)>;
	using closure_bool = std::function<bool (const event &)>;

	extern struct log::log log;
	extern uint64_t current_sequence;
	extern ctx::shared_view<accepted> accept;
	extern const opts default_opts;
}

/// Event Evaluation Device
///
/// This object conducts the evaluation of an event or a tape of multiple
/// events. An event is evaluated in an attempt to execute it. Events which
/// fail during evaluation won't be executed; such is the case for events which
/// have already been executed, or events which are invalid or lead to invalid
/// transitions or actions of the machine etc.
///
struct ircd::m::vm::eval
{
	const vm::opts *opts {&default_opts};
	db::txn txn{*dbs::events};

	fault operator()(const event &);

	eval(const event &, const vm::opts & = default_opts);
	eval(const vm::opts &);
	eval() = default;

	friend string_view reflect(const fault &);
};

/// Evaluation faults. These are reasons which evaluation has halted but may
/// continue after the user defaults the fault. They are basically types of
/// interrupts and traps, which are supposed to be recoverable. Only the
/// GENERAL protection fault (#gp) is an abort and is not supposed to be
/// recoverable. The fault codes have the form of bitflags so they can be
/// used in masks; outside of that case only one fault is dealt with at
/// a time so they can be switched as they appear in the enum.
///
enum ircd::m::vm::fault
:uint
{
	ACCEPT        = 0x00,  ///< No fault.
	EXISTS        = 0x01,  ///< Replaying existing event. (#ex)
	INVALID       = 0x02,  ///< Non-conforming event format. (#ud)
	DEBUGSTEP     = 0x04,  ///< Debug step. (#db)
	BREAKPOINT    = 0x08,  ///< Debug breakpoint. (#bp)
	GENERAL       = 0x10,  ///< General protection fault. (#gp)
	EVENT         = 0x20,  ///< Eval requires addl events in the ef register (#ef)
	STATE         = 0x40,  ///< Required state is missing (#st)
	INTERRUPT     = 0x80,  ///< ctx::interrupted (#nmi)
};

/// Evaluation Options
struct ircd::m::vm::opts
{
	// Extended opts specific to creating events originating from this server.
	struct commit;

	/// Make writes to database
	bool write {true};

	/// Apply effects of the eval
	bool effects {true};

	/// Broadcast to clients/servers
	bool notify {true};

	/// False to allow a dirty conforms report (not recommended).
	bool conforming {true};

	/// Mask of conformity failures to allow without considering dirty.
	event::conforms non_conform;

	/// If the event was already checked before the eval, set this to true
	/// and include the report (see below).
	bool conformed {false};

	/// When conformed=true, this report will be included instead of generating
	/// one during the eval. This is useful if a conformity check was already
	/// done before eval.
	event::conforms report;

	/// Toggles whether event may be considered a "present event" and may
	/// update the optimized present state table of the room if it is proper.
	bool present {true};

	/// Toggles whether the state btree is updated; this should be consistently
	/// true or false for all events in a room.
	bool history {true};

	/// Bypass check for event having already been evaluated so it can be
	/// replayed through the system (not recommended).
	bool replays {false};

	// Verify the origin signature
	bool verify {true};

	/// TODO: Y
	bool prev_check_exists {true};

	/// TODO: Y
	bool head_must_exist {false};

	/// Mask of faults that are not thrown as exceptions out of eval(). If
	/// masked, the fault is returned from eval(). By default, the EXISTS
	/// fault is masked which means existing events won't kill eval loops
	/// as well as the debug related.
	fault_t nothrows
	{
		EXISTS | DEBUGSTEP | BREAKPOINT
	};

	/// Mask of faults that are logged to the error facility in vm::log.
	fault_t errorlog
	{
		~(EXISTS | DEBUGSTEP | BREAKPOINT)
	};

	/// Mask of faults that are logged to the warning facility in vm::log
	fault_t warnlog
	{
		EXISTS
	};

	/// Whether to log a debug message on successful eval.
	bool debuglog_accept {false};

	/// Whether to log an info message on successful eval.
	bool infolog_accept {false};
};

namespace ircd::m::vm
{
	extern const opts::commit default_commit_opts;

	fault commit(const m::event &, const opts::commit & = default_commit_opts);
	event::id::buf commit(json::iov &event, const json::iov &content, const opts::commit & = default_commit_opts);
}

/// Extension structure to vm::opts which includes additional options for
/// commissioning events originating from this server which are then passed
/// through eval (this process is committing).
struct ircd::m::vm::opts::commit
:opts
{
	// Hash and include hashes object.
	bool hash {true};

	// Sign and include signatures object
	bool sign {true};

	// Generate and include event_id
	bool event_id {true};

	// Include our origin
	bool origin {true};

	// Include origin_server_ts
	bool origin_server_ts {true};

	/// Whether to log a debug message before commit
	bool debuglog_precommit {false};

	/// Whether to log an info message after commit accepted
	bool infolog_postcommit {false};
};

struct ircd::m::vm::accepted
:m::event
{
	ctx::ctx *context;
	const vm::opts *opts;
	const event::conforms *report;
	std::string strung;

	accepted(const m::event &event,
	         const vm::opts *const &opts,
	         const event::conforms *const &report);
};

struct ircd::m::vm::error
:m::error
{
	vm::fault code;

	template<class... args> error(const fault &code, const char *const &fmt, args&&... a);
	template<class... args> error(const char *const &fmt, args&&... a);
};

template<class... args>
ircd::m::vm::error::error(const fault &code,
                          const char *const &fmt,
                          args&&... a)
:m::error
{
	http::NOT_MODIFIED, "M_VM_FAULT", fmt, std::forward<args>(a)...
}
,code
{
	code
}
{}

template<class... args>
ircd::m::vm::error::error(const char *const &fmt,
                          args&&... a)
:m::error
{
	http::INTERNAL_SERVER_ERROR, "M_VM_FAULT", fmt, std::forward<args>(a)...
}
,code
{
	fault::GENERAL
}
{}
