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
#define HAVE_IRCD_CONF_H

/// Configuration system.
///
/// This system disseminates mutable runtime values throughout IRCd. All users
/// that integrate a configurable value will create a [static] conf::item<>
/// instantiated with one of the explicit types available; also a name and
/// default value.
///
/// All conf::items are collected by this system. Users that administrate
/// configuration will push values to the conf::item's. The various items have
/// O(1) access to the value contained in their item instance. Administrators
/// have logarithmic access through this interface using the items map by name.
///
/// All conf::items can be controlled by environmental variables at program
/// startup. The name of the conf::item in the environment uses underscore
/// '_' rather than '.' and the environment takes precedence over both defaults
/// and databased values. This means you can set a conf through an env var
/// to override a broken value.
///
namespace ircd::conf
{
	template<class T = void> struct item;  // doesn't exist
	template<> struct item<void>;          // base class of all conf items
	template<> struct item<std::string>;
	template<> struct item<bool>;
	template<> struct item<uint64_t>;
	template<> struct item<int64_t>;
	template<> struct item<uint32_t>;
	template<> struct item<int32_t>;
	template<> struct item<float>;
	template<> struct item<double>;
	template<> struct item<hours>;
	template<> struct item<seconds>;
	template<> struct item<milliseconds>;
	template<> struct item<microseconds>;

	template<class T> struct value;        // abstraction for carrying item value
	template<class T> struct lex_castable; // abstraction for lex_cast compatible

	IRCD_EXCEPTION(ircd::error, error)
	IRCD_EXCEPTION(error, not_found)
	IRCD_EXCEPTION(error, bad_value)

	using set_cb = std::function<void ()>;

	extern std::map<string_view, item<> *> items;
	extern callbacks<void (item<> &)> on_init;

	bool exists(const string_view &key);
	bool persists(const string_view &key);
	string_view get(const string_view &key, const mutable_buffer &out);
	std::string get(const string_view &key);
	bool set(const string_view &key, const string_view &value);
	bool set(std::nothrow_t, const string_view &key, const string_view &value);
	bool fault(std::nothrow_t, const string_view &key) noexcept;
	void fault(const string_view &key);
	bool reset(std::nothrow_t, const string_view &key);
	bool reset(const string_view &key);
	size_t reset();
}

/// Conf item base class. You don't create this directly; use one of the
/// derived templates instead.
template<>
struct ircd::conf::item<void>
{
	static const size_t NAME_MAX_LEN;

	json::strung feature_;
	json::object feature;
	string_view name;
	conf::set_cb set_cb;

  protected:
	virtual string_view on_get(const mutable_buffer &) const;
	virtual bool on_set(const string_view &);
	void call_init();

  public:
	virtual size_t size() const = 0;
	string_view get(const mutable_buffer &) const;
	std::string get() const;
	bool set(const string_view &);
	void fault() noexcept;

	item(const json::members &, conf::set_cb);
	item(item &&) = delete;
	item(const item &) = delete;
	virtual ~item() noexcept;
};

/// Conf item value abstraction. If possible, the conf item will also
/// inherit from this template to deduplicate functionality between
/// conf items which contain similar classes of values.
template<class T>
struct ircd::conf::value
{
	using value_type = T;

	T _value;

	operator const T &() const noexcept
	{
		return _value;
	}

	template<class... A>
	value(A&&... a)
	:_value(std::forward<A>(a)...)
	{}
};

template<class T>
struct ircd::conf::lex_castable
:conf::item<>
,conf::value<T>
{
	string_view on_get(const mutable_buffer &out) const override
	{
		return lex_cast(this->_value, out);
	}

	bool on_set(const string_view &s) override
	{
		this->_value = lex_cast<T>(s);
		return true;
	}

	size_t size() const override
	{
		return LEX_CAST_BUFSIZE;
	}

	lex_castable(const json::members &members,
	             conf::set_cb set_cb = {})
	:conf::item<>
	{
		members, std::move(set_cb)
	}
	,conf::value<T>
	(
		feature.get("default", T(0))
	)
	{
		call_init();
	}
};

template<>
struct ircd::conf::item<std::string>
:conf::item<>
,conf::value<std::string>
{
	explicit operator const std::string &() const
	{
		return _value;
	}

	operator string_view() const
	{
		return _value;
	}

	string_view on_get(const mutable_buffer &out) const override;
	bool on_set(const string_view &s) override;
	size_t size() const override;

	item(const json::members &members, conf::set_cb set_cb = {});
};

template<>
struct ircd::conf::item<bool>
:conf::item<>
,conf::value<bool>
{
	bool operator!() const
	{
		return !static_cast<const bool &>(*this);
	}

	string_view on_get(const mutable_buffer &out) const override;
	bool on_set(const string_view &s) override;
	size_t size() const override;

	item(const json::members &members, conf::set_cb set_cb = {});
};

template<>
struct ircd::conf::item<uint64_t>
:lex_castable<uint64_t>
{
	using lex_castable::lex_castable;
};

template<>
struct ircd::conf::item<int64_t>
:lex_castable<int64_t>
{
	using lex_castable::lex_castable;
};

template<>
struct ircd::conf::item<uint32_t>
:lex_castable<uint32_t>
{
	using lex_castable::lex_castable;
};

template<>
struct ircd::conf::item<int32_t>
:lex_castable<int32_t>
{
	using lex_castable::lex_castable;
};

template<>
struct ircd::conf::item<double>
:lex_castable<double>
{
	using lex_castable::lex_castable;
};

template<>
struct ircd::conf::item<float>
:lex_castable<float>
{
	using lex_castable::lex_castable;
};

template<>
struct ircd::conf::item<ircd::hours>
:lex_castable<ircd::hours>
{
	using lex_castable::lex_castable;
};

template<>
struct ircd::conf::item<ircd::seconds>
:lex_castable<ircd::seconds>
{
	using lex_castable::lex_castable;
};

template<>
struct ircd::conf::item<ircd::milliseconds>
:lex_castable<ircd::milliseconds>
{
	using lex_castable::lex_castable;
};

template<>
struct ircd::conf::item<ircd::microseconds>
:lex_castable<ircd::microseconds>
{
	using lex_castable::lex_castable;
};
