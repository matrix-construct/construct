// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2022 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

//
// Version registry
//

template<>
decltype(ircd::versions::allocator)
ircd::util::instance_list<ircd::versions>::allocator
{};

template<>
decltype(ircd::versions::list)
ircd::util::instance_list<ircd::versions>::list
{
	allocator
};

/// Straightforward construction of versions members; string is copied
/// into member buffer with null termination.
ircd::versions::versions(const string_view &name,
                         const enum type &type,
                         const long &monotonic,
                         const std::array<long, 3> &semantic,
                         const string_view &string)
noexcept
:versions
{
	name, type, monotonic, semantic, [&string]
	(auto &that, const mutable_buffer &buf) noexcept
	{
		strlcpy(buf, string);
	}
}
{
}

/// Construction of versions members with closure for custom string generation.
/// The version string must be stored into buffer with null termination.
ircd::versions::versions(const string_view &name,
                         const enum type &type,
                         const long &monotonic,
                         const std::array<long, 3> &semantic,
                         const std::function<void (versions &, const mutable_buffer &)> &closure)
noexcept
:name{name}
,type{type}
,monotonic{monotonic}
,semantic{semantic}
,string{'\0'}
{
	if(closure) try
	{
		closure(*this, this->string);
	}
	catch(const std::exception &e)
	{
		log::critical
		{
			"Querying %s version of '%s' :%s",
			type == type::ABI? "ABI"_sv : "API"_sv,
			name,
			e.what(),
		};
	}

	// User provided a string already; nothing else to do
	if(strnlen(this->string, sizeof(this->string)) != 0)
		return;

	// Generate a string from the semantic version number or if all zeroes
	// from the monotonic version number instead.
	if(!this->semantic[0] && !this->semantic[1] && !this->semantic[2])
		::snprintf(this->string, sizeof(this->string), "%ld",
		           this->monotonic);
	else
		::snprintf(this->string, sizeof(this->string), "%ld.%ld.%ld",
		           this->semantic[0],
		           this->semantic[1],
		           this->semantic[2]);
}

// Required for instance_list template instantiation.
ircd::versions::~versions()
noexcept
{
}
