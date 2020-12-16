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
#define HAVE_IRCD_M_VM_PHASE_H

namespace ircd::m::vm
{
	enum phase :uint;

	string_view reflect(const phase &);
}

/// Evaluation phases
enum ircd::m::vm::phase
:uint
{
	NONE,                  ///< No phase; not entered.
	EXECUTE,               ///< Execution entered.
	CONFORM,               ///< Conformity check phase.
	DUPWAIT,               ///< Duplicate eval check & hold.
	DUPCHK,                ///< Duplicate existence check.
	ISSUE,                 ///< Issue phase (for my(event)'s only).
	ACCESS,                ///< Access control phase.
	EMPTION,               ///< Emption control phase.
	VERIFY,                ///< Signature verification.
	FETCH_AUTH,            ///< Authentication events fetch phase.
	AUTH_STATIC,           ///< Static authentication phase.
	FETCH_PREV,            ///< Previous events fetch phase.
	FETCH_STATE,           ///< State events fetch phase.
	PRECOMMIT,             ///< Precommit sequence.
	PREINDEX,              ///< Prefetch indexing & transaction dependencies.
	AUTH_RELA,             ///< Relative authentication phase.
	COMMIT,                ///< Commit sequence.
	AUTH_PRES,             ///< Authentication phase.
	EVALUATE,              ///< Evaluation phase.
	INDEX,                 ///< Indexing & transaction building phase.
	POST,                  ///< Transaction-included effects phase.
	WRITE,                 ///< Write transaction.
	RETIRE,                ///< Retire phase
	NOTIFY,                ///< Notifications phase.
	EFFECTS,               ///< Effects phase.
	_NUM_
};
