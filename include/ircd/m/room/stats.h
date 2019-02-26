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
#define HAVE_IRCD_M_ROOM_STATS_H

struct ircd::m::room::stats
{
	static size_t bytes_json_compressed(const m::room &);
	static size_t bytes_json(const m::room &);

	static size_t bytes_total_compressed(const m::room &);
	static size_t bytes_total(const m::room &);
};
