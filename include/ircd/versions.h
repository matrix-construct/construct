// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2022 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.


#pragma once
#define HAVE_IRCD_VERSIONS_H

namespace ircd
{
	struct versions;
}

/// Instances of `versions` create a dynamic version registry identifying
/// third-party dependencies throughout the project and its loaded modules.
///
/// Create a static instance of this class in a definition file or module
/// which has access to the version information of the dependency. Often there
/// can be two version identifiers for a dependency, one for headers and the
/// other for dynamically loaded shared object. In that case, create two
/// instances of this class with the same name.
///
struct ircd::versions
:instance_list<versions>
{
	/// Our own name for the dependency.
	string_view name;

	/// Set the type to either API or ABI to indicate where this version
	/// information has been sourced. Defaults to API.
	enum type {API, ABI} type {API};

	/// If the version number is a single (likely monotonic) integer.
	long monotonic {0};

	/// Alternative semantic version number.
	std::array<long, 3> semantic {0};

	/// Version string buffer. Copy any provided version string here.
	char string[128] {0};

	/// Convenience access to read the semantic version numbers.
	auto &operator[](const int &idx) const;

	/// Convenience access to read the monotonic integer; note that if zero
	/// the semantic major number is read instead.
	explicit operator const long &() const;

	/// Convenience access to read the string
	explicit operator string_view() const;

	versions(const string_view &name,
	         const enum type &type                = type::API,
	         const long &monotonic                = 0,
	         const std::array<long, 3> &semantic  = {0L},
	         const string_view &string            = {}) noexcept;

	versions(const string_view &name,
	         const enum type &type,
	         const long &monotonic,
	         const std::array<long, 3> &semantic,
	         const std::function<void (versions &, const mutable_buffer &)> &generator) noexcept;

	versions() = default;
	versions(versions &&) = delete;
	versions(const versions &) = delete;
	~versions() noexcept;
};

namespace ircd
{
	template<>
	decltype(versions::allocator)
	instance_list<versions>::allocator;

	template<>
	decltype(versions::list)
	instance_list<versions>::list;
}

inline ircd::versions::operator
const long &()
const
{
	return monotonic?: semantic[0];
}

inline ircd::versions::operator
ircd::string_view()
const
{
	return string;
}

inline auto &
ircd::versions::operator[](const int &idx)
const
{
	return semantic.at(idx);
}
