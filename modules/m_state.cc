// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

using namespace ircd;

mapi::header
IRCD_MODULE
{
	"Matrix state library; modular components."
};

namespace ircd::m::state
{
	extern "C" void ircd__m__state__clear(void);
	extern "C" size_t ircd__m__state__gc(void);
}

size_t
ircd::m::state::ircd__m__state__gc()
{
	std::set<id_buffer> heads;
	{
		const db::gopts opts{db::get::NO_CACHE};
		//opts.readahead = 4_MiB;

		db::column &column{m::dbs::room_events};
		for(auto it(column.begin(opts)); it; ++it)
		{
			const auto &root(it->second);
			id_buffer buf;
			copy(buf, root);
			heads.emplace(buf);
		}
	}

	std::set<id_buffer> active;
	for(auto it(begin(heads)); it != end(heads); it = heads.erase(it))
	{
		const auto &root(*it);
		std::cout << "root: " << string_view{root} << std::endl;
		state::for_each(root, state::iter_bool_closure{[&active]
		(const json::array &key, const string_view &val)
		{
			std::cout << "key: " << key << std::endl;
			std::cout << "val: " << val << std::endl;
			return true;
		}});

		break;
	}
/*
	static const db::gopts opts
	{
		db::get::NO_CACHE
	};

	auto &column{m::dbs::state_node};
	for(auto it(column.begin(opts)); it; ++it)
	{
		id_buffer buf;
		const auto &root(it->second);
		copy(buf, root);
		active.emplace(buf);
	}
*/
	size_t ret(active.size());
	return ret;
}

void
ircd::m::state::ircd__m__state__clear()
{
	auto &column
	{
		m::dbs::state_node
	};

	// Find the key range for the column. The key is copied here so we don't
	// hold an iterator on it and prevent its demise.
	std::pair<std::string, std::string> range;
	{
		const auto front
		{
			column.begin()
		};

		const auto back
		{
			column.rbegin()
		};

		// If these iterators are invalid there is nothing to delete
		if(!front || !back)
			return;

		range =
		{
			front->first, back->first
		};
	};

	db::txn txn
	{
		*m::dbs::events
	};

	db::txn::append
	{
		txn, column, db::column::delta
		{
			db::op::DELETE_RANGE, range.first, range.second
		}
	};

	// DELETE_RANGE second key is exclusive and won't be included in the
	// range so we have to add a single delete for it.
	db::txn::append
	{
		txn, column, db::column::delta
		{
			db::op::DELETE, range.second
		}
	};

	txn();
}
