/*
 * charybdis: 21st Century IRC++d
 *
 * Copyright (C) 2016 Charybdis Development Team
 * Copyright (C) 2016 Jason Volk <jason@zemos.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <ircd/cuckoo.h>

#pragma once
#define HAVE_IRCD_M_VM_H

/// Matrix Virtual Machine
///
/// This is the processing unit for matrix events. The front-end interface to
/// m::vm consists of two parts: The query suite (observation) and the eval
/// (modification). The back-end interface consists of modules which supply
/// the features for the vm's logic. The m::vm operates primarily as a global
/// singleton and facilitates synchronization between multiple ircd::ctx's
/// using its interfaces.
///
/// The query suite tries to satisfy a query using the current state of the vm,
/// the database, and may even use the network. An idea of a basic pseudo-
/// query: was user X a member of room Y when event Z happened? Is X a member
/// of Y right now? etc.
///
/// The vm::eval is the primary input receptacle for events. The eval of an
/// event leads to its execution and will advance the global state of IRCd. That
/// advance may broadcast its effects to clients of IRCd, federation servers,
/// and alter the responses to future queries. Execution of an event is a
/// unique phenomenon: it only happens once. IRCd has thenceforth borne witness
/// to the event and will never execute it again unless it is forgotten by
/// erasing the db. Simulation options allow for evals without execution.
///
/// The back-end provides the business logic and features for the above user
/// interfaces.
///
namespace ircd::m::vm
{
	IRCD_M_EXCEPTION(m::error, VM_ERROR, http::INTERNAL_SERVER_ERROR);
	IRCD_M_EXCEPTION(VM_ERROR, VM_FAULT, http::BAD_REQUEST);

	enum fault :uint;
	struct frame;
	struct capstan;
	struct witness;
	struct accumulator;
	struct front;
	struct eval;
	struct pipe;
	struct port;
	struct space;
	struct opts;

	using closure = std::function<void (const event &)>;
	using closure_bool = std::function<bool (const event &)>;

	extern struct log::log log;
	extern uint64_t current_sequence;
	extern ctx::view<const event> inserted;
	extern struct fronts fronts;
	extern struct pipe pipe;

	bool test(const query<> &, const closure_bool &);
	bool test(const query<> &);
	void for_each(const query<> &, const closure &);
	void for_each(const closure &);
	size_t count(const query<> &, const closure_bool &);
	size_t count(const query<> &);
	bool exists(const event::id &);

	int test(eval &, const query<> &, const closure_bool &);
	int test(eval &, const query<> &);
}

/// Evaluation Options
struct ircd::m::vm::opts
{
	/// Setting to false will disable the eval from exiting with a fault and
	/// instead consider all faults as errors which throw exceptions. If
	/// nothrow is also specified then the exception will be stored in the
	/// exception_ptr and #gp will be set.
	bool faults {true};

	/// Setting to true will prevent any exception throwing from an eval and
	/// instead only use the exception_ptr field. Note that eval does not make
	/// a noexcept guarantee with or without this option and it may still throw
	/// some rare or unexpected exception aborting the eval.
	bool nothrow {false};

	/// Setting to true will cause a successful eval to commit the result
	/// which writes to the db and has global effects for the server.
	/// False is the simulation mode to test evaluation without effects.
	bool commit {false};

	/// Setting to false will prevent the release sequence after a commit.
	/// This is a really bad thing to do. testing/debug/devel use only.
	bool release {true};

	/// Setting to true will force the eval to be aware of an m.room.create
	/// for a room for anything to succeed. There will be #ef's until found.
	bool genesis {false};

	/// Setting to true will cause #ef's until there is at least one unbroken
	/// link of events down to an m.room.create.
	bool cosmogonic {false};

	/// Setting to true will cause #ef's until the entire reference cone is
	/// known by the evaluation (if legit). i.e full history tree.
	bool fullcone {false};

	/// Setting to 1 forces evaluations to only proceed from oldest to newest
	/// events; the capstan will only operate using forward mode and positive
	/// accumulations. Because events only reference the past the machine won't
	/// always #ef, it will just stop. More events can be input. Setting to
	/// -1 is anti-causal: evaluations only proceed from newest to oldest and
	/// only negative accumulations are used. 0 is automatic and may use both.
	int causal {0};

	/// Option to skip event signature verification when false.
	bool verify_signature {true};
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

struct ircd::m::vm::capstan
{
	int64_t dc{0};
	std::vector<std::unique_ptr<vm::accumulator>> acc;

	ssize_t count(const query<> &) const;
	int test(const query<> &) const;

	void fwd(const event &);
	void rev(const event &);

	capstan();
	~capstan() noexcept;
};

struct ircd::m::vm::port
{
	const m::event *event {nullptr};
	std::exception_ptr error;
	fault code;
	bool h {false};
	bool w {false};
};

struct ircd::m::vm::space
{
	struct capstan capstan;
};

struct ircd::m::vm::pipe
{
	std::unordered_map<string_view, space> s;
	std::deque<port> p;
};

/// Event Evaluation Device
///
/// This object conducts the evaluation of an event or a tape of multiple
/// events. An event is evaluated in an attempt to execute it. Events which
/// fail during evaluation won't be executed; such is the case for events which
/// have already been executed, or events which are invalid or lead to invalid
/// transitions or actions of the machine etc.
///
/// Basic usage of the eval is simply calling the ctor like a function with an
/// event; with the default options this evaluates and executes the event
/// throwing all errors. Advanced usage of eval takes the form of an instance
/// constructed with options; the user then drives the eval in a loop,
/// responding to its needs for more data or other issues, and then continuing
/// the loop until satisfied.
///
/// The back-end interface consists of a set of modules which register with the
/// vm::eval class and provide their piece of eval functionality to instances.
///
struct ircd::m::vm::eval
{
	static const struct opts default_opts;

	const struct opts *opts;
	struct capstan capstan;
	db::iov txn{*event::events};
	std::set<event::id> ef;
	uint64_t cs {0};

	fault operator()();
	fault operator()(const event &);
	fault operator()(const vector_view<const event> &);
	fault operator()(const json::vector &);
	fault operator()(const json::array &);

	template<class events> eval(const struct opts &, events&&);
	template<class events> eval(const events &);
	eval(const struct opts &);
	eval();

	friend string_view reflect(const fault &);
};

/// Convenience construction passes events through to operator().
template<class events>
ircd::m::vm::eval::eval(const events &event)
:eval(default_opts)
{
	operator()(event);
}

/// Convenience construction passes events through to operator().
template<class events>
ircd::m::vm::eval::eval(const struct opts &opts,
                        events&& event)
:eval(opts)
{
	operator()(std::forward<events>(event));
}

// state accumulators:
//
// - (bool) event_id: Adds all event_ids to accumulator. On eval, capstan
// checks if event_id is not in set: accepts; Otherwise event_id duplicate in
// set: rejects.
//
// - (bool) membership: Capstan has a accumulator for each membership state.
// Adds accumulator[membership] = state_key. On eval, capstan knows about user not
// having membership of claimed type. Negative for all accumulators indicates no
// membership in the set.
//
// - (count) types: Counts all type names encountered to respond to
// queries about that type maybe or definitely not being in the poset.
//
// - (count) state types: Counts all type names encountered when there is
// a state_key defined.
//
// - (count) sender: Counts all sender strings. Responds to queries about
// whether a sender may have or definitely did not take part in the poset.
//
// - (count) origin: Counts all origin strings.
//
struct ircd::m::vm::witness
:ircd::instance_list<witness>
{
	std::string name;

	virtual ssize_t count(const accumulator *const &, const query<> &)
	{
		return -1;
	}

	virtual int test(const accumulator *const &, const query<> &)
	{
		return -1;
	}

	virtual int add(accumulator *const &, const event &)
	{
		return -1;
	}

	virtual int del(accumulator *const &, const event &)
	{
		return -1;
	}

	virtual std::unique_ptr<accumulator> init()
	{
		return {};
	}

	witness(std::string name);
	virtual ~witness() noexcept;
};

struct ircd::m::vm::accumulator
{
	virtual ~accumulator() noexcept;
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
	vm::capstan capstan;
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

namespace ircd::m::vm
{
	// Synchronous fetch and eval
	size_t acquire(const vector_view<const id::event> &, const vector_view<const mutable_buffer> &);
	json::object acquire(const id::event &, const mutable_buffer &);
    void statefill(const room::id &, const event::id &);
    void backfill(const room::id &, const event::id &v, const size_t &limit);

	using tracer = std::function<bool (const event &, event::id::buf &)>;
	void trace(const id::event &, const tracer &);

	event::id::buf commit(json::iov &event);
    event::id::buf join(const room::id &, json::iov &iov);
}
