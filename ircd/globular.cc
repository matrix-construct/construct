// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

//
// globular_equals
//

bool
ircd::globular_equals::operator()(const string_view &a, const string_view &b)
const
{
	size_t ap(0), bp(0);
	while(ap < a.size() && bp < b.size())
	{
		const auto ca(tolower(a.at(ap))),  cb(tolower(b.at(bp)));
		const auto globa(ca == '*'),       globb(cb == '*');
		const auto wilda(ca == '?'),       wildb(cb == '?');

		if(!globa && !globb && !wilda && !wildb && ca != cb)
			return false;

		if((globa && ap + 1 >= a.size()) || (globb && bp + 1 >= b.size()))
			break;

		if(globa && cb == tolower(a.at(ap + 1)))
			ap += 2;

		if(globb && ca == tolower(b.at(bp + 1)))
			bp += 2;

		if(globa && globb)
			++ap, ++bp;

		if(!globa)
			++ap;

		if(!globb)
			++bp;
	}

	if(ap < a.size() && !b.empty() && b.back() == '*')
		return true;

	if(bp < b.size() && !a.empty() && a.back() == '*')
		return true;

	return iequals(a.substr(ap), b.substr(bp));
}

//
// globular_match
//

bool
ircd::globular_match::operator()(const string_view &a)
const
{
	//TODO: optimize.
	const globular_equals globular_equals(expr, a);
	return bool(globular_equals);
}
