// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

decltype(ircd::conf::items)
ircd::conf::items
{};

decltype(ircd::conf::on_init)
ircd::conf::on_init
{};

size_t
ircd::conf::reset()
{
	size_t ret{0};
	for(const auto &p : items)
		ret += reset(p.first);

	return ret;
}

bool
ircd::conf::reset(const string_view &key)
try
{
	return reset(std::nothrow, key);
}
catch(const std::exception &e)
{
	return false;
}

bool
ircd::conf::reset(std::nothrow_t,
                  const string_view &key)
try
{
	auto &item(*items.at(key));
	if(!item.set_cb)
		return false;

	item.set_cb();
	return true;
}
catch(const std::out_of_range &e)
{
	throw not_found
	{
		"Conf item '%s' is not available", key
	};
}
catch(const std::exception &e)
{
	log::error
	{
		"conf item[%s] set callback :%s",
		key,
		e.what()
	};

	throw;
}

bool
ircd::conf::set(std::nothrow_t,
                const string_view &key,
                const string_view &value)
try
{
	return set(key, value);
}
catch(const std::exception &e)
{
	log::error
	{
		"%s", e.what()
	};

	return false;
}

bool
ircd::conf::set(const string_view &key,
                const string_view &value)
try
{
	auto &item(*items.at(key));
	return item.set(value);
}
catch(const bad_lex_cast &e)
{
	throw bad_value
	{
		"Conf item '%s' rejected value '%s'", key, value
	};
}
catch(const std::out_of_range &e)
{
	throw not_found
	{
		"Conf item '%s' is not available", key
	};
}

ircd::string_view
ircd::conf::get(const string_view &key,
                const mutable_buffer &out)
try
{
	const auto &item(*items.at(key));
	return item.get(out);
}
catch(const std::out_of_range &e)
{
	throw not_found
	{
		"Conf item '%s' is not available", key
	};
}

bool
ircd::conf::exists(const string_view &key)
{
	return items.count(key);
}

//
// item
//

decltype(ircd::conf::item<void>::NAME_MAX_LEN)
ircd::conf::item<void>::NAME_MAX_LEN
{
	127
};

/// Conf item abstract constructor.
ircd::conf::item<void>::item(const json::members &opts,
                             conf::set_cb set_cb)
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
,set_cb
{
	std::move(set_cb)
}
{
	if(size(name) > NAME_MAX_LEN)
		throw error
		{
			"Conf item '%s' name length:%zu exceeds max:%zu",
			name,
			size(name),
			NAME_MAX_LEN
		};

	if(!items.emplace(name, this).second)
		throw error
		{
			"Conf item named '%s' already exists", name
		};
}

ircd::conf::item<void>::~item()
noexcept
{
	if(name)
	{
		const auto it{items.find(name)};
		assert(data(it->first) == data(name));
		items.erase(it);
	}
}

bool
ircd::conf::item<void>::set(const string_view &val)
{
	const bool ret
	{
		on_set(val)
	};

	if(set_cb) try
	{
		set_cb();
	}
	catch(const std::exception &e)
	{
		log::error
		{
			"conf item[%s] set callback :%s", e.what()
		};
	}

	return ret;
}

ircd::string_view
ircd::conf::item<void>::get(const mutable_buffer &buf)
const
{
	return on_get(buf);
}

void
ircd::conf::item<void>::call_init()
try
{
	on_init(*this);
}
catch(const std::exception &e)
{
	log::error
	{
		"conf item[%s] init callback :%s",
		name,
		e.what()
	};
}

bool
ircd::conf::item<void>::on_set(const string_view &)
{
	return false;
}

ircd::string_view
ircd::conf::item<void>::on_get(const mutable_buffer &)
const
{
	return {};
}

//
// Non-inline template specialization definitions
//

//
// std::string
//

ircd::conf::item<std::string>::item(const json::members &members,
                                    conf::set_cb set_cb)
:conf::item<>
{
	members, std::move(set_cb)
}
,value
{
	unquote(feature.get("default"))
}
{
	call_init();
}

bool
ircd::conf::item<std::string>::on_set(const string_view &s)
{
	_value = std::string{s};
	return true;
}

ircd::string_view
ircd::conf::item<std::string>::on_get(const mutable_buffer &out)
const
{
	return { data(out), _value.copy(data(out), size(out)) };
}

//
// bool
//

ircd::conf::item<bool>::item(const json::members &members,
                             conf::set_cb set_cb)
:conf::item<>
{
	members, std::move(set_cb)
}
,value
{
	feature.get<bool>("default", false)
}
{
	call_init();
}

bool
ircd::conf::item<bool>::on_set(const string_view &s)
{
	switch(hash(s))
	{
		case "true"_:
			_value = true;
			return true;

		case "false"_:
			_value = false;
			return true;

		default: throw bad_value
		{
			"Conf item '%s' not assigned a bool literal", name
		};
	}
}

ircd::string_view
ircd::conf::item<bool>::on_get(const mutable_buffer &out)
const
{
	return _value?
		strlcpy(out, "true"_sv):
		strlcpy(out, "false"_sv);
}
