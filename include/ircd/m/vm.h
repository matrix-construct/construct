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
	IRCD_M_EXCEPTION(m::error, VM_ERROR, http::INTERNAL_SERVER_ERROR);
	IRCD_M_EXCEPTION(VM_ERROR, VM_FAULT, http::BAD_REQUEST);

	enum fault :uint;
	struct front;
	struct eval;

	using closure = std::function<void (const event &)>;
	using closure_bool = std::function<bool (const event &)>;
	using dbs::cursor;

	extern struct log::log log;
	extern struct fronts fronts;
	extern uint64_t current_sequence;
	extern ctx::view<const event> inserted;

	event::id::buf commit(json::iov &event);
	event::id::buf commit(json::iov &event, const json::iov &content);
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
	struct opts;

	struct opts static const default_opts;

	const struct opts *opts {&default_opts};
	db::txn txn{*dbs::events};
	uint64_t cs{0};

	fault operator()(const event &);

	eval(const event &, const struct opts & = default_opts);
	eval(const struct opts &);
	eval() = default;

	friend string_view reflect(const fault &);
};

/// Evaluation Options
struct ircd::m::vm::eval::opts
{

};

/// Individual evaluation state
///
/// Evaluation faults. These are reasons which evaluation has halted but may
/// continue after the user defaults the fault. They are basically types of
/// interrupts and traps, which are recoverable. Only the GENERAL protection
/// fault (#gp) is an abort and is not recoverable. An exception_ptr will be
/// set; aborts are otherwise just thrown as exceptions from eval. If any
/// fault isn't handled properly that is an abort.
///
enum ircd::m::vm::fault
:uint
{
	ACCEPT,          ///< No fault.
	DEBUGSTEP,       ///< Debug step. (#db)
	BREAKPOINT,      ///< Debug breakpoint. (#bp)
	GENERAL,         ///< General protection fault. (#gp)
	EVENT,           ///< Eval requires addl events in the ef register (#ef)
	STATE,           ///< Required state is missing (#st)
};

/// The "event front" for a graph. This holds all of the childless events
/// for a room. Each childless event may exist at a different depth, but
/// we track the highest depth to increment it for issuing the next event.
/// The contents of the front will then be used as the prev references for
/// that new event. The front is then replaced by the new event.  This is
/// managed by the vm core.
///
struct ircd::m::vm::front
{
	int64_t top {0};
	std::map<std::string, uint64_t, std::less<std::string_view>> map;
};

/// Singleton iface to all "event fronts" - the top depth in an active graph
///
/// This extern singleton is a fundamental structure which holds the highest
/// depth events in a graph which have no children. The fronts collection
/// itself is a map of rooms by ID, and one 'front' is held in RAM for each
/// room. Each front is a collection of those events, which ideally, will
/// become the prev references for the next event this server issues in the
/// room.
///
/// This is managed by the vm core. The fronts interface is the root of the
/// RAM footprint for a room known to IRCd. In other words, all room data is
/// stored in the database except what is reachable through here.
///
struct ircd::m::vm::fronts
{
	std::map<std::string, front, std::less<std::string_view>> map;

	friend front &fetch(const room::id &, front &, const event &);

  public:
	front &get(const room::id &, const event &);
	front &get(const room::id &);
};
