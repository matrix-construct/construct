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
#define HAVE_IRCD_DB_DATABASE_DESCRIPTOR_H

/// Descriptor of a column when opening database. Database must be opened with
/// a consistent set of descriptors describing what will be found upon opening.
struct ircd::db::database::descriptor
{
	using typing = std::pair<std::type_index, std::type_index>;

	std::string name;
	std::string explain;
	typing type { typeid(string_view), typeid(string_view) };
	std::string options {};
	db::comparator cmp {};
	db::prefix_transform prefix {};
	size_t cache_size { 16_MiB };
	size_t cache_size_comp { 8_MiB };
	size_t bloom_bits { 10 };
};
