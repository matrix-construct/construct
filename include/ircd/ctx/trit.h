// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_CTX_TRIT_H

namespace ircd::ctx
{
	struct trit;
}

namespace ircd
{
	using ctx::trit;
}

/// Implementation of a tribool with custom semantics in a stackful coroutine
/// environment. Tribools have three states: true, false, and unknown. Our goal
/// here is to integrate this "unknown" property with the context-switching
/// system. In a nutshell, when a `trit` in an unknown state is sampled, the
/// context blocks until the `trit` leaves the unknown state. This action is
/// based on the existing promise/future system and waiting for the known state
/// can be thought of as a `future<bool>::get()`. The value of this endeavor
/// though is greater than mere syntactic sugar for future<bool>.
/// ```
/// if(trit == true)    // context blocks here until the value is known
///     ...             // branch taken if value==true once known.
///
/// ```
///
/// Overloaded logic operators make it possible to optimize
/// conditional predicates by employing Kleene's strong logic of indeterminacy.
/// This allows us to optimize these predicates for I/O rather than
/// computation. In the example below, traditionally under boolean logic we
/// evaluate trit[0] first, and if it's false, short-circuit evaluation elides
/// observing trit[1]:
/// ```
/// if(trit[0] && trit[1])
///     ...
///
/// ```
/// Under the trilean logic in our system, we first test if trit[0] is known,
/// because if it isn't, we can test if trit[1] is knowably false to conclude
/// the predicate. The benefit is seen when these objects represent the result
/// of latent asynchronous operations; head-of-line blocking is avoided in
/// this case because any false value can abrogate any further blocking.
///
struct ircd::ctx::trit
{
	/// The boolean value state of true or false. This value is undefined when
	/// `unknown` is true
	bool value;

	/// Whether the boolean value is determinable/determined or not. We name
	/// this negatively such that any zero pre-initialization creates a known
	/// false value without requiring any code to be executed.
	bool unknown;

	trit();
};

inline
ircd::ctx::trit::trit()
:value{false}
,unknown{false}
{}
