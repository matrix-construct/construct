// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_VM_OPTS_H

namespace ircd::m::vm
{
	struct opts;
	struct copts;

	extern const opts default_opts;
	extern const copts default_copts;
}

/// Evaluation Options
struct ircd::m::vm::opts
{
	/// The remote server name which is conducting this eval.
	string_view node_id;

	/// The mxid of the user which is conducting this eval.
	string_view user_id;

	/// The txnid from the node conducting the eval.
	string_view txn_id;

	/// Enabled phases of evaluation.
	std::bitset<num_of<vm::phase>()> phase {-1UL};

	/// Custom write_opts to use during write.
	dbs::write_opts wopts;

	/// Broadcast to local clients (/sync stream).
	bool notify_clients {true};

	/// Broadcast to federation servers (/federation/send/).
	bool notify_servers {true};

	/// False to allow a dirty conforms report (not recommended).
	bool conforming {true};

	/// False to bypass all auth phases.
	bool auth {true};

	/// False to bypass all fetch phases.
	bool fetch {true};

	/// Mask of conformity failures to allow without considering dirty.
	event::conforms non_conform;

	/// If the event was already checked before the eval, set this to true
	/// and include the report (see below).
	bool conformed {false};

	/// When conformed=true, this report will be included instead of generating
	/// one during the eval. This is useful if a conformity check was already
	/// done before eval.
	event::conforms report;

	/// Supply the room version; overrides/avoids any internal query.
	string_view room_version;

	/// When true, the event is expected to have its content; hash mismatch
	/// is not permitted. When false, hash mismatch is permitted. When -1,
	/// by default the origin server is allowed to redact the content.
	int8_t require_content {-1};

	/// Toggles whether event may be considered a "present event" and may
	/// update the optimized present state table of the room if it is proper.
	bool present {true};

	/// Evaluate in EDU mode. Input must not have event_id and none will be
	/// generated for it.
	bool edu {false};

	/// Bypass check for event having already been evaluated so it can be
	/// replayed through the system (not recommended).
	bool replays {false};

	/// Bypass check for another evaluation of the same event_id already
	/// occurring. If this is false (not recommended) two duplicate events
	/// being evaluated may race through the core.
	bool unique {true};

	/// When true, events in array inputs are evaluated as they are provided
	/// without any reordering before eval.
	bool ordered {false};

	/// If the input event has a reference to already-strung json we can use
	/// that directly when writing to the DB. When this is false we will
	/// re-stringify the event internally either from a referenced source or
	/// the tuple if no source is referenced. This should only be set to true
	/// if the evaluator already performed this and the json source is good.
	bool json_source {false};

	/// Whether to gather all unknown keys from an input vector of events and
	/// perform a parallel/mass fetch before proceeding with the evals.
	bool mfetch_keys {true};

	/// Whether to launch prefetches for all event_id's (found at standard
	/// locations) from the input vector, in addition to some other related
	/// local db prefetches. Disabled by default because it operates prior
	/// to verification and access phases; can be enabled explicitly.
	bool mprefetch_refs {false};

	/// Throws fault::EVENT if *all* of the prev_events do not exist locally.
	/// This is used to enforce that at least one path is traversable. This
	/// test is conducted after waiting if fetch_prev and fetch_prev_wait.
	bool fetch_prev_any {false};

	/// Throws fault::EVENT if *any* of the prev_events do not exist locally.
	/// This is used to enforce that all references have been acquired; other
	/// corollary conditions are similar to fetch_prev_any.
	bool fetch_prev_all {false};

	/// The number of iterations of the wait cycle which checks for missing
	/// prev_events will iterate before issuing remote fetches for them.
	/// The default is 0 which bypasses the functionality, and is recommended
	/// when the evaluator is confident missing prev_events won't arrive
	/// elsehow. Setting to -1 enables it with an auto/conf value.
	size_t fetch_prev_wait_count {0};

	/// Base time to wait for missing prev_events to arrive at the server by
	/// some other means before issuing remote fetches for them. The waiting
	/// occurs in a loop where prev_events satisfaction is checked at each
	/// iteration. This value is multiplied by the number of iterations for
	/// multiplicative backoff. The default of -1 is auto / conf.
	milliseconds fetch_prev_wait_time {-1};

	/// The limit on the number of events to backfill if any of the prev_events
	/// are missing. -1 is auto / conf.
	size_t fetch_prev_limit = -1;

	/// Considers any missing prev_event as an indication of possible missing
	/// state from a history we don't have; allowing a state acquisition. This
	/// is not practical to apply by default as internal decisions are better.
	bool fetch_state_any {false};

	/// This option affects the behavior for a case where we are missing the
	/// (depth - 1) prev_events reference, thus other resolved references
	/// are not adjacent. Yet at the claimed depth there is no apparent gap
	/// in the timeline. If true, we assume possible missing state in this
	/// case, but by default that is far too unrealistic in practice.
	bool fetch_state_shallow {false};

	/// Evaluators can set this value to optimize the creation of the database
	/// transaction where the event will be stored. This value should be set
	/// to the amount of space the event consumes; the JSON-serialized size is
	/// a good value here. Default of -1 will automatically use serialized().
	size_t reserve_bytes = -1;

	/// This value is added to reserve_bytes to account for indexing overhead
	/// in the database transaction allocation. Most evaluators have little
	/// reason to ever adjust this.
	size_t reserve_index {1024};

	/// Coarse limit for array evals. The counter is incremented for every
	/// event; both accepted and faulted.
	size_t limit = -1;

	/// Mask of faults that are not thrown as exceptions out of eval(). If
	/// masked, the fault is returned from eval(). By default, the EXISTS
	/// fault is masked which means existing events won't kill eval loops.
	/// ACCEPT is ignored in the mask.
	fault_t nothrows
	{
		EXISTS
	};

	/// Mask of faults that are logged to the error facility in vm::log.
	/// ACCEPT is ignored in the mask.
	fault_t errorlog
	{
		~(EXISTS)
	};

	/// Mask of faults that are logged to the warning facility in vm::log
	/// ACCEPT is ignored in the mask.
	fault_t warnlog
	{
		EXISTS
	};

	/// Mask of faults that are transcribed to the json::stack output.
	fault_t outlog
	{
		~(EXISTS | ACCEPT)
	};

	/// Whether to log a debug message on successful eval.
	bool debuglog_accept {false};

	/// Whether to log an info message on successful eval.
	bool infolog_accept {false};

	/// Point this at a json::stack to transcribe output.
	json::stack::object *out {nullptr};

	opts() noexcept;
};

/// Extension structure to vm::opts which includes additional options for
/// commissioning events originating from this server which are then passed
/// through eval (this process is also known as issuing).
struct ircd::m::vm::copts
:opts
{
	/// A matrix-spec opaque token from a client identifying this eval.
	string_view client_txnid;

	/// This bitmask covers all of the top-level properties of m::event
	/// which will be generated internally during injection unless they
	/// already exist. Clearing any of these bits will prevent the internal
	/// generation of these properties (i.e. for EDU's).
	event::keys::selection prop_mask
	{
		event::keys::include
		{
			"auth_events",
			"depth",
			"event_id",
			"hashes",
			"origin",
			"origin_server_ts",
			"prev_events",
			"prev_state",
			"signatures",
		}
	};

	/// Call the issue hook or bypass
	bool issue {true};

	/// Whether to log a debug message before commit
	bool debuglog_precommit {false};

	/// Whether to log an info message after commit accepted
	bool infolog_postcommit {false};

	copts() noexcept;
};
