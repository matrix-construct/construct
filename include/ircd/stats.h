// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_STATS_H

/// Statistics & Metrics
///
/// This is a central collection of registered items each representing a
/// counter or metric of some kind. To collect items of various types we orient
/// the collection around an abstract item<void> template class. To keep things
/// simple, the abstract instance holds a typeinfo provided by the derived
/// instance. User can then downcast the item<void> to a derived template.
///
/// There are two levels in the class hierarchy under the abstract root
/// item<void>. The next level is a pointer-to-value such that the item
/// registers the location of an existing value. This is useful for
/// incorporating external values in existing structures into this collection
/// non-intrusively; for example in third-party libraries etc.
///
/// The next derived level after this is where the item instance itself
/// roots (contains) the value and its parent class at the pointer-level points
/// down to the derived class. This is a convenience for developers so that
/// extern values don't have to be created separately for every stats item.
/// Note that when this subsystem works abstractly with items, it considers the
/// pointer-level templates to be the principal level of derivation; in other
/// words there is no reason to downcast to a value-level template when working
/// with this system, and every value-level template must have a matching
/// pointer-level parent.
namespace ircd::stats
{
	IRCD_EXCEPTION(ircd::error, error)
	IRCD_EXCEPTION(error, not_found)

	// Abstract item
	template<class T = void> struct item;
	template<> struct item<void>;

	// Pointer-to-value items
	template<> struct item<uint64_t *>;
	template<> struct item<uint32_t *>;
	template<> struct item<uint16_t *>;

	// Value-carrying items
	template<> struct item<uint64_t>;
	template<> struct item<uint32_t>;
	template<> struct item<uint16_t>;

	extern const size_t NAME_MAX_LEN;
	extern std::map<string_view, item<void> *> items;

	std::ostream &operator<<(std::ostream &, const item<void> &);
}

/// Abstract stats item.
///
/// This object contains type information about its derived class. There is no
/// known use for constructing this on its own without a derived item.
///
/// Feature information must contain a 'name' string. It is advised that this
/// is appropriately namespaced i.e "ircd.net.socket.xxx" and when third-party
/// values are gathered i.e "rocksdb.xxx." such that the entire map of items
/// can be serialized into a single JSON object tree.
///
/// Feature information can also contain a 'desc' string describing more about
/// the value to administrators and developers.
template<>
struct ircd::stats::item<void>
{
	std::type_index type {typeid(void)};
	json::strung feature;

  public:
	// Access features
	string_view operator[](const string_view &key) const noexcept;

	virtual bool operator!() const = 0;

	item() = default;
	item(const std::type_index &, const json::members &);
	item(item &&) = delete;
	item(const item &) = delete;
	virtual ~item() noexcept;
};

template<>
struct ircd::stats::item<uint64_t *>
:item<void>
{
	uint64_t *val {nullptr};

  public:
	bool operator!() const override
	{
		return !val || !*val;
	}

	operator const uint64_t &() const
	{
		assert(val);
		return *val;
	}

	operator uint64_t &()
	{
		assert(val);
		return *val;
	}

	item(uint64_t *const &, const json::members &);
	item() = default;
};

template<>
struct ircd::stats::item<uint32_t *>
:item<void>
{
	uint32_t *val {nullptr};

  public:
	bool operator!() const override
	{
		return !val || !*val;
	}

	operator const uint32_t &() const
	{
		assert(val);
		return *val;
	}

	operator uint32_t &()
	{
		assert(val);
		return *val;
	}

	item(uint32_t *const &, const json::members &);
	item() = default;
};

template<>
struct ircd::stats::item<uint16_t *>
:item<void>
{
	uint16_t *val {nullptr};

  public:
	bool operator!() const override
	{
		return !val || !*val;
	}

	operator const uint16_t &() const
	{
		assert(val);
		return *val;
	}

	operator uint16_t &()
	{
		assert(val);
		return *val;
	}

	item(uint16_t *const &, const json::members &);
	item() = default;
};

template<>
struct ircd::stats::item<uint64_t>
:item<uint64_t *>
{
	uint64_t val {0};

  public:
	operator const uint64_t &() const noexcept
	{
		return val;
	}

	operator uint64_t &() noexcept
	{
		return val;
	}

	item(const json::members &);
	item() = default;
};

template<>
struct ircd::stats::item<uint32_t>
:item<uint32_t *>
{
	uint32_t val {0};

  public:
	operator const uint32_t &() const noexcept
	{
		return val;
	}

	operator uint32_t &() noexcept
	{
		return val;
	}

	item(const json::members &);
	item() = default;
};

template<>
struct ircd::stats::item<uint16_t>
:item<uint16_t *>
{
	uint16_t val {0};

  public:
	operator const uint16_t &() const noexcept
	{
		return val;
	}

	operator uint16_t &() noexcept
	{
		return val;
	}

	item(const json::members &);
	item() = default;
};
