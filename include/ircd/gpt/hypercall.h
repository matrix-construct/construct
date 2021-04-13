// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2021 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_GPT_HYPERCALL_H

/// Hypercalling code enumeration.
///
/// Error codes are all negative values. Zero is also an error.
enum ircd_gpt_hypercall
{
	/// General nominal completion code; similar to EXIT_SUCCESS, etc.
	IRCD_GPT_ACCEPT = 1,

	/// Failed or incomplete execution occurred. After an execution attempt
	/// it indicates no execution likely took place. Device software never
	/// sets this value; it is the initial value set by the host before
	/// execution.
	IRCD_GPT_ECOMPLETE = 0,

	/// Erroneous token buffer
	IRCD_GPT_ETOKENS = -1,
};

#ifdef __cplusplus
namespace ircd::gpt
{
	string_view reflect(const enum ircd_gpt_hypercall) noexcept;
}
#endif
