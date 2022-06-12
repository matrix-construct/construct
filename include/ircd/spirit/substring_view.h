// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2022 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_SPIRIT_SUBSTRING_VIEW_H

// This file is not part of the IRCd standard include list (stdinc.h) because
// it involves extremely expensive boost headers for creating formal spirit
// grammars. This file is automatically included in the spirit.h group.

namespace ircd::spirit
{
	struct substring_view;
}

struct ircd::spirit::substring_view
:ircd::string_view
{
	using _iterator = const char *;
	using _iterator_range = boost::iterator_range<_iterator>;
	using _indirect_iterator = karma::detail::indirect_iterator<_iterator>;
	using _indirect_iterator_range = boost::iterator_range<_indirect_iterator>;

	explicit substring_view(const _indirect_iterator_range &range);
	explicit substring_view(const _iterator_range &range);
	using ircd::string_view::string_view;
};

inline
ircd::spirit::substring_view::substring_view(const _iterator_range &range)
:ircd::string_view
{
	std::addressof(*range.begin()),
	std::addressof(*range.end())
}
{}

inline
ircd::spirit::substring_view::substring_view(const _indirect_iterator_range &range)
:ircd::string_view
{
	std::addressof(*range.begin()),
	std::addressof(*range.end())
}
{}
