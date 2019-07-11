// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

decltype(ircd::stats::items)
ircd::stats::items
{};

std::ostream &
ircd::stats::operator<<(std::ostream &s, const item &item)
{
	s << static_cast<long long>(item.val);
	return s;
}

//
// item
//

decltype(ircd::stats::item::NAME_MAX_LEN)
ircd::stats::item::NAME_MAX_LEN
{
	127
};

//
// item::item
//

ircd::stats::item::item(const json::members &opts)
:feature_
{
	opts
}
,feature
{
	feature_
}
,name
{
	unquote(feature.at("name"))
}
,val
{
	feature.get<long>("default", 0L)
}
{
	if(name.size() > NAME_MAX_LEN)
		throw error
		{
			"Stats item '%s' name length:%zu exceeds max:%zu",
			name,
			name.size(),
			NAME_MAX_LEN
		};

	if(!items.emplace(name, this).second)
		throw error
		{
			"Stats item named '%s' already exists", name
		};
}

ircd::stats::item::~item()
noexcept
{
	if(name)
	{
		const auto it{items.find(name)};
		assert(data(it->first) == data(name));
		items.erase(it);
	}
}
