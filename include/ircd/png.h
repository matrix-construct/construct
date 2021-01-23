// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2021 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_PNG_H

/// Portable Network Graphics; wrappers a la carte
namespace ircd::png
{
	IRCD_EXCEPTION(ircd::error, error)

	bool is_animated(const const_buffer &);

	extern const info::versions version_api, version_abi;
}
