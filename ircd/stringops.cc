// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

std::string
ircd::replace(const string_view &s,
              const char &before,
              const string_view &after)
{
	const uint32_t occurs
	(
		std::count(begin(s), end(s), before)
	);

	const size_t size
	{
		occurs? s.size() + (occurs * after.size()):
		        s.size() - occurs
	};

	return string(size, [&s, &before, &after]
	(const mutable_buffer &buf)
	{
		char *p{begin(buf)};
		std::for_each(begin(s), end(s), [&before, &after, &p]
		(const char &c)
		{
			if(c == before)
			{
				memcpy(p, after.data(), after.size());
				p += after.size();
			}
			else *p++ = c;
		});

		return std::distance(begin(buf), p);
	});
}

//
// gequals
//

bool
ircd::gequals::operator()(const string_view &a, const string_view &b)
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
// gmatch
//

bool
ircd::gmatch::operator()(const string_view &a)
const
{
	//TODO: optimize.
	const gequals gequals(expr, a);
	return bool(gequals);
}
