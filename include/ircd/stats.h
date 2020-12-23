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
	IRCD_PANICKING(error, invalid)

	// Abstract item
	template<class T = void> struct item;
	template<> struct item<void>;

	// Category item
	template<class T> struct ptr_item;
	template<class T> struct int_item;

	// Pointer-to-value items
	template<> struct item<uint64_t *>;
	template<> struct item<uint32_t *>;
	template<> struct item<uint16_t *>;
	template<> struct item<int64_t *>;
	template<> struct item<int32_t *>;
	template<> struct item<int16_t *>;
	template<> struct item<nanoseconds *>;
	template<> struct item<microseconds *>;
	template<> struct item<milliseconds *>;
	template<> struct item<seconds *>;

	// Value-carrying items
	template<> struct item<uint64_t>;
	template<> struct item<uint32_t>;
	template<> struct item<uint16_t>;
	template<> struct item<int64_t>;
	template<> struct item<int32_t>;
	template<> struct item<int16_t>;
	template<> struct item<nanoseconds>;
	template<> struct item<microseconds>;
	template<> struct item<milliseconds>;
	template<> struct item<seconds>;

	extern const size_t NAME_MAX_LEN;
	extern std::vector<item<void> *> items;

	string_view string(const mutable_buffer &, const item<void> &);
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
	json::string name;

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

/// Abstract pointer item
template<class T>
struct ircd::stats::ptr_item
:item<void>
{
	T *val {nullptr};

  public:
	bool operator!() const override
	{
		return !val || *val == T{0};
	}

	operator const T &() const
	{
		assert(val);
		return *val;
	}

	operator T &()
	{
		assert(val);
		return *val;
	}

	ptr_item &operator=(const T &val) &
	{
		static_cast<T &>(*this) = val;
		return *this;
	}

	ptr_item(T *const &val, const json::members &feature)
	:item<void>{typeid(T *), feature}
	,val{val}
	{}

	ptr_item() = default;
};

/// Abstract value item
template<class T>
struct ircd::stats::int_item
:ptr_item<T>
{
	T val {0};

  public:
	operator const T &() const noexcept
	{
		return val;
	}

	operator T &() noexcept
	{
		return val;
	}

	int_item &operator=(const T &val) &
	{
		static_cast<T &>(*this) = val;
		return *this;
	}

	int_item(const json::members &feature)
	:ptr_item<T>{std::addressof(this->val), feature}
	,val{0}
	{}

	int_item() = default;
};

template<>
struct ircd::stats::item<uint64_t *>
:ptr_item<uint64_t>
{
	using ptr_item<uint64_t>::ptr_item;
	using ptr_item<uint64_t>::operator=;
};

template<>
struct ircd::stats::item<uint64_t>
:int_item<uint64_t>
{
	using int_item<uint64_t>::int_item;
	using int_item<uint64_t>::operator=;
};

template<>
struct ircd::stats::item<int64_t *>
:ptr_item<int64_t>
{
	using ptr_item<int64_t>::ptr_item;
	using ptr_item<int64_t>::operator=;
};

template<>
struct ircd::stats::item<int64_t>
:int_item<int64_t>
{
	using int_item<int64_t>::int_item;
	using int_item<int64_t>::operator=;
};

template<>
struct ircd::stats::item<uint32_t *>
:ptr_item<uint32_t>
{
	using ptr_item<uint32_t>::ptr_item;
	using ptr_item<uint32_t>::operator=;
};

template<>
struct ircd::stats::item<uint32_t>
:int_item<uint32_t>
{
	using int_item<uint32_t>::int_item;
	using int_item<uint32_t>::operator=;
};

template<>
struct ircd::stats::item<int32_t *>
:ptr_item<int32_t>
{
	using ptr_item<int32_t>::ptr_item;
	using ptr_item<int32_t>::operator=;
};

template<>
struct ircd::stats::item<int32_t>
:int_item<int32_t>
{
	using int_item<int32_t>::int_item;
	using int_item<int32_t>::operator=;
};

template<>
struct ircd::stats::item<uint16_t *>
:ptr_item<uint16_t>
{
	using ptr_item<uint16_t>::ptr_item;
	using ptr_item<uint16_t>::operator=;
};

template<>
struct ircd::stats::item<uint16_t>
:int_item<uint16_t>
{
	using int_item<uint16_t>::int_item;
	using int_item<uint16_t>::operator=;
};

template<>
struct ircd::stats::item<int16_t *>
:ptr_item<int16_t>
{
	using ptr_item<int16_t>::ptr_item;
	using ptr_item<int16_t>::operator=;
};

template<>
struct ircd::stats::item<int16_t>
:int_item<int16_t>
{
	using int_item<int16_t>::int_item;
	using int_item<int16_t>::operator=;
};

template<>
struct ircd::stats::item<ircd::nanoseconds *>
:ptr_item<nanoseconds>
{
	using ptr_item<nanoseconds>::ptr_item;
	using ptr_item<nanoseconds>::operator=;
};

template<>
struct ircd::stats::item<ircd::nanoseconds>
:int_item<nanoseconds>
{
	using int_item<nanoseconds>::int_item;
	using int_item<nanoseconds>::operator=;
};

template<>
struct ircd::stats::item<ircd::microseconds *>
:ptr_item<microseconds>
{
	using ptr_item<microseconds>::ptr_item;
	using ptr_item<microseconds>::operator=;
};

template<>
struct ircd::stats::item<ircd::microseconds>
:int_item<microseconds>
{
	using int_item<microseconds>::int_item;
	using int_item<microseconds>::operator=;
};

template<>
struct ircd::stats::item<ircd::milliseconds *>
:ptr_item<milliseconds>
{
	using ptr_item<milliseconds>::ptr_item;
	using ptr_item<milliseconds>::operator=;
};

template<>
struct ircd::stats::item<ircd::milliseconds>
:int_item<milliseconds>
{
	using int_item<milliseconds>::int_item;
	using int_item<milliseconds>::operator=;
};

template<>
struct ircd::stats::item<ircd::seconds *>
:ptr_item<seconds>
{
	using ptr_item<seconds>::ptr_item;
	using ptr_item<seconds>::operator=;
};

template<>
struct ircd::stats::item<ircd::seconds>
:int_item<seconds>
{
	using int_item<seconds>::int_item;
	using int_item<seconds>::operator=;
};
