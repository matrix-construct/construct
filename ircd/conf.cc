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

void
ircd::conf::fault(const string_view &key)
try
{
	auto &item(*items.at(key));
	item.fault();
}
catch(const std::out_of_range &e)
{
	throw not_found
	{
		"Conf item '%s' is not available", key
	};
}

bool
ircd::conf::fault(std::nothrow_t,
                  const string_view &key)
noexcept try
{
	auto &item(*items.at(key));
	item.fault();
	return true;
}
catch(const std::out_of_range &e)
{
	return false;
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

std::string
ircd::conf::get(const string_view &key)
try
{
	const auto &item(*items.at(key));
	return item.get();
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
ircd::conf::persists(const string_view &key)
try
{
	const auto &item(*items.at(key));
	return item.feature.get("persist", true);
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
	if(name.size() > NAME_MAX_LEN)
		throw error
		{
			"Conf item '%s' name length:%zu exceeds max:%zu",
			name,
			name.size(),
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

void
ircd::conf::item<void>::fault()
noexcept try
{
	const json::string &default_
	{
		feature.get("default")
	};

	log::warning
	{
		"conf item[%s] defaulting with featured value :%s",
		name,
		string_view{default_}
	};

	if(on_set(default_))
		if(set_cb)
			set_cb();
}
catch(const std::exception &e)
{
	throw panic
	{
		"Conf item '%s' failed to set its default value (double-fault) :%s",
		name,
		e.what()
	};
}

bool
ircd::conf::item<void>::set(const string_view &val)
{
	std::string existing(get()); try
	{
		if(on_set(val))
			if(set_cb)
				set_cb();
	}
	catch(...)
	{
		try
		{
			if(on_set(existing))
				if(set_cb)
					set_cb();
		}
		catch(...)
		{
			fault();
		}

		throw;
	}

	return true;
}

std::string
ircd::conf::item<void>::get()
const
{
	return ircd::string(size(), [this]
	(const mutable_buffer &buf)
	{
		return get(buf);
	});
}

ircd::string_view
ircd::conf::item<void>::get(const mutable_buffer &buf)
const
{
	return on_get(buf);
}

bool
ircd::conf::item<void>::on_set(const string_view &)
{
	return true;
}

ircd::string_view
ircd::conf::item<void>::on_get(const mutable_buffer &)
const
{
	return {};
}

namespace ircd::conf
{
	static void call_env(item<void> &) noexcept;
	static void call_init(item<void> &) noexcept;
}

void
ircd::conf::item<void>::call_init()
{
	// The conf item's default value specified by the constructor is its
	// current value at this point; now we make callbacks to allow that value
	// to be replaced with a better one (i.e reading a saved value from DB).
	conf::call_init(*this);

	// Environmental variables now get the final say; this allows any
	// misconfiguration to be overridden by env vars. The variable name is
	// the conf item name with any '.' replaced to '_', case is preserved.
	conf::call_env(*this);
}

void
ircd::conf::call_init(item<void> &item)
noexcept try
{
	on_init(item);
}
catch(const std::exception &e)
{
	log::error
	{
		"conf item[%s] on_init callback :%s",
		item.name,
		e.what()
	};
}

void
ircd::conf::call_env(item<void> &item)
noexcept try
{
	assert(size(item.name) <= item.NAME_MAX_LEN);
	thread_local char key[conf::item<void>::NAME_MAX_LEN];
	const string_view name
	{
		key,
		std::replace_copy(begin(item.name), end(item.name), key, '.', '_')
	};

	const string_view val
	{
		util::getenv(name)
	};

	if(!empty(val))
		item.set(val);
}
catch(const std::exception &e)
{
	log::error
	{
		"conf item[%s] environmental variable :%s",
		item.name,
		e.what()
	};
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

size_t
ircd::conf::item<std::string>::size()
const
{
	return _value.size();
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
	return { data(out), _value.copy(data(out), buffer::size(out)) };
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

size_t
ircd::conf::item<bool>::size()
const
{
	return _value?
		ircd::size("true"_sv):
		ircd::size("false"_sv);
}

bool
ircd::conf::item<bool>::on_set(const string_view &s)
try
{
	_value = lex_cast<bool>(s);
	return true;
}
catch(const bad_lex_cast &e)
{
	throw bad_value
	{
		"Conf item '%s' not assigned a bool literal :%s",
		name,
		e.what()
	};
}

ircd::string_view
ircd::conf::item<bool>::on_get(const mutable_buffer &out)
const
{
	return _value?
		string_view { data(out), copy(out, "true"_sv)  }:
		string_view { data(out), copy(out, "false"_sv) };
}
