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
	IRCD_M_EXCEPTION(VM_ERROR, VM_INVALID, http::BAD_REQUEST);

	enum fault :uint;
	struct eval;

	using closure = std::function<void (const event &)>;
	using closure_bool = std::function<bool (const event &)>;

	extern struct log::log log;
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
