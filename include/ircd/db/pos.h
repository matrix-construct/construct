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
#define HAVE_IRCD_DB_POS_H

namespace ircd::db
{
	enum class pos :int8_t;

	string_view reflect(const pos &);
}

/// Types of iterator operations
enum class ircd::db::pos
:int8_t
{
	FRONT   = -2,    // .front()    | first element
	PREV    = -1,    // std::prev() | previous element
	END     = 0,     // break;      | exit iteration (or past the end)
	NEXT    = 1,     // continue;   | next element
	BACK    = 2,     // .back()     | last element
};
