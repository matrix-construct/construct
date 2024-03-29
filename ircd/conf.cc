// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::conf
{
	static string_view make_env_name(const mutable_buffer &, const string_view &) noexcept;
	static string_view make_env_name(const mutable_buffer &, const item<void> &) noexcept;
	static string_view make_env_name(const mutable_buffer &, const item<void> &, const string_view &);
	static void prepend_from_env(item<void> &) noexcept;
	static void append_from_env(item<void> &) noexcept;
	static void set_from_env(item<void> &) noexcept;
	static void set_from_closure(item<void> &) noexcept;
}

decltype(ircd::conf::items)
ircd::conf::items;

decltype(ircd::conf::on_init)
ircd::conf::on_init;

decltype(ircd::defaults)
ircd::defaults
{
	{ "name",     "ircd.defaults" },
	{ "default",  false           },
	{ "persist",  false           },
};

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

	item.set_cb(item);
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

template<>
bool
ircd::conf::as(const string_view &key)
{
	char buf[5]; // "true" or "false"
	const auto val
	{
		get(buf, key)
	};

	return lex_cast<bool>(val);
}

template<>
bool
ircd::conf::as(const string_view &key,
               bool def)
{
	char buf[5]; // "true" or "false"
	const auto val
	{
		get(std::nothrow, buf, key)
	};

	if(val && ircd::lex_castable<bool>(val))
		def = lex_cast<bool>(val);

	return def;
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
ircd::conf::get(const mutable_buffer &out,
                const string_view &key)
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

std::string
ircd::conf::get(std::nothrow_t,
                const string_view &key)
{
	const auto it(items.find(key));
	if(it == end(items))
		return {};

	const auto &item(*it->second);
	return item.get();
}

ircd::string_view
ircd::conf::get(std::nothrow_t,
                const mutable_buffer &out,
                const string_view &key)
{
	const auto it(items.find(key));
	if(it == end(items))
		return {};

	const auto &item(*it->second);
	return item.get(out);
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
ircd::conf::environ(const string_view &key)
noexcept try
{
	char buf[conf::NAME_MAX_LEN];
	const auto env_key
	{
		make_env_name(buf, key)
	};

	const auto val
	{
		util::getenv(env_key)
	};

	return bool(val);
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
ircd::conf::exists(const string_view &key)
noexcept
{
	return items.count(key);
}

//
// item
//

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
			set_cb(*this);
}
catch(const std::exception &e)
{
	terminate
	{
		panic
		{
			"Conf item '%s' failed to set its default value (double-fault) :%s",
			name,
			e.what()
		}
	};
}

bool
ircd::conf::item<void>::set(const string_view &val)
{
	std::string existing(get()); try
	{
		if(on_set(val))
			if(set_cb)
				set_cb(*this);
	}
	catch(...)
	{
		try
		{
			if(on_set(existing))
				if(set_cb)
					set_cb(*this);
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

void
ircd::conf::item<void>::call_init()
{
	// The conf item's default value specified by the constructor is its
	// current value at this point; now we make callbacks to allow that value
	// to be replaced with a better one (i.e reading a saved value from DB).
	conf::set_from_closure(*this);

	// Environment variables now get the final say; this allows any
	// misconfiguration to be overridden by env vars. The variable name is
	// the conf item name with any '.' replaced to '_', case is preserved.

	// Prepend to item's current value from env.
	conf::prepend_from_env(*this);

	// Append to item's current value from env.
	conf::append_from_env(*this);

	// Overwrite item's value if env is set.
	conf::set_from_env(*this);
}

void
ircd::conf::set_from_closure(item<void> &item)
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
ircd::conf::prepend_from_env(item<void> &item)
noexcept try
{
	thread_local char buf[conf::NAME_MAX_LEN];
	const auto key
	{
		make_env_name(buf, item, "PREPEND")
	};

	const auto env
	{
		util::getenv(key)
	};

	if(empty(env))
		return;

	auto val
	{
		item.get()
	};

	val = std::string{env} + val;
	item.set(val);
}
catch(const std::exception &e)
{
	log::error
	{
		"conf item[%s] prepending from environment variable :%s",
		item.name,
		e.what()
	};
}

void
ircd::conf::append_from_env(item<void> &item)
noexcept try
{
	thread_local char buf[conf::NAME_MAX_LEN];
	const auto key
	{
		make_env_name(buf, item, "APPEND")
	};

	const auto env
	{
		util::getenv(key)
	};

	if(empty(env))
		return;

	auto val
	{
		item.get()
	};

	val += env;
	item.set(val);
}
catch(const std::exception &e)
{
	log::error
	{
		"conf item[%s] appending from environment variable :%s",
		item.name,
		e.what()
	};
}

void
ircd::conf::set_from_env(item<void> &item)
noexcept try
{
	thread_local char buf[conf::NAME_MAX_LEN];
	const auto key
	{
		make_env_name(buf, item)
	};

	const auto val
	{
		util::getenv(key)
	};

	if(empty(val) && !null(val))
		return;

	item.set(val);
}
catch(const std::exception &e)
{
	log::error
	{
		"conf item[%s] setting from environment variable :%s",
		item.name,
		e.what()
	};
}

ircd::string_view
ircd::conf::make_env_name(const mutable_buffer &buf,
                          const item<void> &item,
                          const string_view &feature)
{
	char tmp[conf::NAME_MAX_LEN] {0};
	const auto name
	{
		make_env_name(tmp, item)
	};

	return string_view
	{
		data(buf), unsigned(::snprintf
		(
			data(buf), size(buf), "%s__%s",
			name.c_str(),
			feature.c_str()
		))
	};
}

ircd::string_view
ircd::conf::make_env_name(const mutable_buffer &buf,
                          const item<void> &item)
noexcept
{
	return make_env_name(buf, item.name);
}

ircd::string_view
ircd::conf::make_env_name(const mutable_buffer &buf,
                          const string_view &name)
noexcept
{
	assert(size(name) <= conf::NAME_MAX_LEN);
	return replace(buf, name, '.', '_');
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
