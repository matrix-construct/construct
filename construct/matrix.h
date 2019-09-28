// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd
{
	struct matrix;
}

namespace construct
{
	struct matrix;
}

struct construct::matrix
{
	ircd::ctx::dock dock;
	ircd::context context;
	ircd::matrix *instance {nullptr};

	void main() noexcept;

	matrix();
	~matrix() noexcept;

	static void init();
	static void quit();
};
