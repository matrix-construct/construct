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
	struct error; // custom exception
	struct opts;
	struct copts;
	struct eval;
	struct phase;
	struct accepted;
	enum fault :uint;
	using fault_t = std::underlying_type<fault>::type;

	extern ctx::shared_view<accepted> accept;
	extern uint64_t current_sequence;
	extern const opts default_opts;
	extern const copts default_copts;

	string_view reflect(const fault &);
	const uint64_t &sequence(const eval &);
	uint64_t retired_sequence(id::event::buf &);
	uint64_t retired_sequence();
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
:instance_list<eval>
{
	static uint64_t id_ctr; // monotonic

	const vm::opts *opts {&default_opts};
	const vm::copts *copts {nullptr};
	ctx::ctx *ctx {ctx::current};
	uint64_t id {++id_ctr};
	string_view room_id;
	const json::iov *issue {nullptr};
	const event *event_ {nullptr};
	vm::phase *phase {nullptr};
	uint64_t sequence {0};
	db::txn *txn {nullptr};
	event::id::buf event_id;

  public:
	operator const event::id::buf &() const;

	fault operator()(const event &);
	fault operator()(json::iov &event, const json::iov &content);
	fault operator()(const room &, json::iov &event, const json::iov &content);

	eval(const vm::opts &);
	eval(const vm::copts &);
	eval(const event &, const vm::opts & = default_opts);
	eval(json::iov &event, const json::iov &content, const vm::copts & = default_copts);
	eval(const room &, json::iov &event, const json::iov &content);
	eval() = default;
	eval(eval &&) = delete;
	eval(const eval &) = delete;
	~eval() noexcept;
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
	/// Make writes to database
	bool write {true};

	/// Apply effects of the eval
	bool effects {true};

	/// Broadcast to clients/servers. When true, individual notify options
	/// that follow are considered. When false, no notifications occur.
	short notify {true};

	/// Broadcast to local clients (/sync stream).
	bool notify_clients {true};

	/// Broadcast to federation servers (/federation/send/).
	bool notify_servers {true};

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

	/// Toggles whether event may be added to the room head table which means
	/// it is considered unreferenced by any other event at this time. It is
	/// safe for this to always be true if events are evaluated in order. If
	/// `present` is false this should be set to false but they are not tied.
	bool head {true};

	/// Toggles whether the prev_events of this event are removed from the
	/// room head table, now that this event has referenced them. It is safe
	/// for this to always be true.
	bool refs {true};

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

	/// Evaluators can set this value to optimize the creation of the database
	/// transaction where the event will be stored. This value should be set
	/// to the amount of space the event consumes; the JSON-serialized size is
	/// a good value here. Default of -1 will automatically use serialized().
	size_t reserve_bytes = -1;

	/// This value is added to reserve_bytes to account for indexing overhead
	/// in the database transaction allocation. Most evaluators have little
	/// reason to ever adjust this.
	size_t reserve_index {1536};

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

/// Extension structure to vm::opts which includes additional options for
/// commissioning events originating from this server which are then passed
/// through eval (this process is committing).
struct ircd::m::vm::copts
:opts
{
	/// Hash and include hashes object.
	bool add_hash {true};

	/// Sign and include signatures object
	bool add_sig {true};

	/// Generate and include event_id
	bool add_event_id {true};

	/// Include our origin
	bool add_origin {true};

	/// Include origin_server_ts
	bool add_origin_server_ts {true};

	/// Add prev_events
	bool add_prev_events {true};

	/// Add prev_state
	bool add_prev_state {true};

	/// Add auth_events
	bool add_auth_events {true};

	/// Whether to log a debug message before commit
	bool debuglog_precommit {false};

	/// Whether to log an info message after commit accepted
	bool infolog_postcommit {false};
};

struct ircd::m::vm::phase
:instance_list<phase>
{
	string_view name;

	phase(const string_view &name);
};

struct ircd::m::vm::accepted
:m::event
{
	ctx::ctx *context;
	const vm::opts *opts;
	const event::conforms *report;
	shared_buffer<mutable_buffer> strung;

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
