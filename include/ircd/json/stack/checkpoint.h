// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2023 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_JSON_STACK_CHECKPOINT_H

/// Checkpoint captures the current state of the json::stack on construction
/// and allows a restoration to that state in one of three ways:
///
/// - Calling rollback() will immediately rewind the json::stack buffer and
/// allow continuing from the check point. This should be used with care, as
/// other json::stack objects may still be pending on the stack and destruct
/// after calling rollback(), leaving an incoherent attempt to close the JSON.
///
/// - Calling decommit() will defer the rollback() until destruction time. Take
/// care again that the checkpoint was still placed on the stack to avoid the
/// rollback() pitfall.
///
/// - Destruction under an exception is equivalent to a decommit() and will
/// perform a rollback() if exception_rollback is set.
///
/// Flushes are avoided under the scope of a checkpoint, but they are still
/// forced if the json::stack buffer fills up. In this case all active
/// checkpoints are invalidated and cannot be rolled back.
///
struct ircd::json::stack::checkpoint
{
	stack *s {nullptr};
	checkpoint *pc {nullptr};
	size_t point {0};
	size_t vc {0};
	bool committed {true};
	bool exception_rollback {true};

  public:
	bool committing() const noexcept;         ///< When false, destructor will rollback()
	bool committing(const bool &) noexcept;   ///< Sets committing() to value.
	bool rollback();                          ///< Performs rollback of buffer.

	checkpoint(stack &s,
	           const bool committed = true,
	           const bool exception_rollback = true);

	checkpoint(checkpoint &&) = delete;
	checkpoint(const checkpoint &) = delete;
	~checkpoint() noexcept;
};

inline bool
ircd::json::stack::checkpoint::committing(const bool &committed)
noexcept
{
	const bool ret(this->committed);
	this->committed = committed;
	return ret;
}

inline bool
ircd::json::stack::checkpoint::committing()
const noexcept
{
	return committed;
}
