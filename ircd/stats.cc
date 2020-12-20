// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

decltype(ircd::stats::NAME_MAX_LEN)
ircd::stats::NAME_MAX_LEN
{
	127
};

decltype(ircd::stats::items)
ircd::stats::items
{};

std::ostream &
ircd::stats::operator<<(std::ostream &s,
                        const item<void> &item_)
{
	thread_local char tmp[256];
	s << string(tmp, item_);
	return s;
}

ircd::string_view
ircd::stats::string(const mutable_buffer &buf,
                    const item<void> &item_)
{
	if(item_.type == typeid(uint64_t *))
	{
		const auto &item
		{
			dynamic_cast<const stats::item<uint64_t *> &>(item_)
		};

		assert(item.val);
		return fmt::sprintf
		{
			buf, "%lu", *item.val
		};
	}
	else if(item_.type == typeid(uint32_t *))
	{
		const auto &item
		{
			dynamic_cast<const stats::item<uint32_t *> &>(item_)
		};

		assert(item.val);
		return fmt::sprintf
		{
			buf, "%u", *item.val
		};
	}
	else if(item_.type == typeid(uint16_t *))
	{
		const auto &item
		{
			dynamic_cast<const stats::item<uint16_t *> &>(item_)
		};

		assert(item.val);
		return fmt::sprintf
		{
			buf, "%u", *item.val
		};
	}
	else throw error
	{
		"Unsupported value type '%s'",
		item_.type.name(),
	};
}

//
// item
//

//
// item::item
//

ircd::stats::item<void>::item(const std::type_index &type,
                              const json::members &opts)
:type
{
	type
}
,feature
{
	opts
}
,name
{
	this->operator[]("name")
}
{
	if(unlikely(!name))
		throw invalid
		{
			"Stats item must have a 'name' string feature"
		};

	if(unlikely(name.size() > NAME_MAX_LEN))
		throw invalid
		{
			"Stats item '%s' name length:%zu exceeds max:%zu",
			name,
			name.size(),
			NAME_MAX_LEN
		};

	const auto exists
	{
		end(items) != std::find_if(begin(items), end(items), [this]
		(const auto &item)
		{
			return item->name == this->name;
		})
	};

	if(unlikely(exists))
		throw invalid
		{
			"Stats item named '%s' already exists",
			name
		};

	if(items.empty())
		items.reserve(4096);

	items.emplace_back(this);
}

ircd::stats::item<void>::~item()
noexcept
{
	if(!name)
		return;

	const auto eit
	{
		std::remove_if(begin(items), end(items), [this]
		(const auto &item)
		{
			return item->name == this->name;
		})
	};

	items.erase(eit, end(items));
}

ircd::string_view
ircd::stats::item<void>::operator[](const string_view &key)
const noexcept
{
	const json::object feature
	{
		this->feature
	};

	return feature[key];
}

//
// pointer-to-value items
//

//
// item<uint64_t *>
//

ircd::stats::item<uint64_t *>::item(uint64_t *const &val,
                                    const json::members &feature)
:item<void>
{
	typeid(uint64_t *), feature
}
,val
{
	val
}
{
}

//
// item<uint32_t *>
//

ircd::stats::item<uint32_t *>::item(uint32_t *const &val,
                                    const json::members &feature)
:item<void>
{
	typeid(uint32_t *), feature
}
,val
{
	val
}
{
}

//
// item<uint16_t *>
//

ircd::stats::item<uint16_t *>::item(uint16_t *const &val,
                                    const json::members &feature)
:item<void>
{
	typeid(uint16_t *), feature
}
,val
{
	val
}
{
}

//
// value-carrying items
//

//
// item<uint64_t>
//

ircd::stats::item<uint64_t>::item(const json::members &feature)
:item<uint64_t *>
{
	std::addressof(this->val), feature
}
,val
{
	0UL
}
{
}

//
// item<uint32_t>
//

ircd::stats::item<uint32_t>::item(const json::members &feature)
:item<uint32_t *>
{
	std::addressof(this->val), feature
}
,val
{
	0U
}
{
}

//
// item<uint16_t>
//

ircd::stats::item<uint16_t>::item(const json::members &feature)
:item<uint16_t *>
{
	std::addressof(this->val), feature
}
,val
{
	0U
}
{
}
