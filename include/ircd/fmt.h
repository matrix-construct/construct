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
#define HAVE_IRCD_FMT_H

/// Typesafe format strings from formal grammars & standard RTTI
namespace ircd::fmt
{
	IRCD_EXCEPTION(ircd::error, error);
	IRCD_EXCEPTION(error, invalid_format);
	IRCD_EXCEPTION(error, invalid_type);
	IRCD_EXCEPTION(error, illegal);

	struct sprintf;
	struct vsprintf;
	struct snprintf;
	struct vsnprintf;
	struct snstringf;
	struct vsnstringf;
	template<size_t MAX> struct bsprintf;

	using arg = std::tuple<const void *, const std::type_index &>;
}

/// Typesafe snprintf() from formal grammar and RTTI.
///
/// This function accepts a format string and a variable number of arguments
/// composing formatted null-terminated output in the provided output buffer.
/// The format string is compliant with standard snprintf() (TODO: not yet).
/// The type information of the arguments is grabbed from the variadic template
/// and references are passed to the formal output grammars. This means you can
/// pass an std::string directly without calling c_str(), as well as pass a
/// non-null-terminated string_view safely.
///
class ircd::fmt::snprintf
{
	window_buffer out;                           // Window on the output buffer.
	const_buffer fmt;                            // Current running position in the fmtstr.
	short idx;                                   // Keeps count of the args for better err msgs

  protected:
	bool finished() const;
	size_t remaining() const;
	size_t consumed() const                      { return out.consumed();                          }
	string_view completed() const                { return out.completed();                         }

	void append(const string_view &);
	void argument(const arg &);

	IRCD_OVERLOAD(internal)
	snprintf(internal_t, const mutable_buffer &, const string_view &, const va_rtti &);

  public:
	operator ssize_t() const                     { return consumed();                              }
	operator string_view() const                 { return completed();                             }

	template<class... Args>
	snprintf(char *const &buf,
	         const size_t &max,
	         const string_view &fmt,
	         Args&&... args)
	:snprintf
	{
		internal, mutable_buffer{buf, max}, fmt, va_rtti{std::forward<Args>(args)...}
	}{}
};

struct ircd::fmt::sprintf
:snprintf
{
	template<class... Args>
	sprintf(const mutable_buffer &buf,
	        const string_view &fmt,
	        Args&&... args)
	:snprintf
	{
		internal, buf, fmt, va_rtti{std::forward<Args>(args)...}
	}{}
};

/// A complement to fmt::snprintf() accepting an already-made va_rtti.
///
/// This function has no variadic template; instead it accepts the type
/// which would be composed by such a variadic template called
/// ircd::va_rtti directly.
///
/// ircd::va_rtti is a lightweight pairing of argument pointers to runtime
/// type indexes. ircd::va_rtti is not a template itself because its purpose
/// is to bring this type information out of the header files to where the
/// grammar is instantiated.
///
struct ircd::fmt::vsnprintf
:snprintf
{
	vsnprintf(char *const &buf,
	          const size_t &max,
	          const string_view &fmt,
	          const va_rtti &ap)
	:snprintf
	{
		internal, mutable_buffer{buf, max}, fmt, ap
	}{}
};

struct ircd::fmt::vsprintf
:snprintf
{
	vsprintf(const mutable_buffer &buf,
	         const string_view &fmt,
	         const va_rtti &ap)
	:snprintf
	{
		internal, buf, fmt, ap
	}{}
};

struct ircd::fmt::vsnstringf
:std::string
{
	vsnstringf(const size_t &max,
	           const string_view &fmt,
	           const va_rtti &ap)
	:std::string
	{
		ircd::string(max, [&fmt, &ap]
		(const mutable_buffer &buf) -> string_view
		{
			return vsprintf
			{
				buf, fmt, ap
			};
		})
	}{}
};

struct ircd::fmt::snstringf
:vsnstringf
{
	template<class... args>
	snstringf(const size_t &max,
	          const string_view &fmt,
	          args&&... a)
	:vsnstringf
	{
		max, fmt, va_rtti{std::forward<args>(a)...}
	}{}
};

template<size_t MAX>
struct ircd::fmt::bsprintf
:snprintf
,string_view
{
	std::array<char, MAX> buf;

	template<class... args>
	bsprintf(const string_view &fmt,
	         args&&... a)
	:snprintf
	{
		internal, buf, fmt, va_rtti{std::forward<args>(a)...}
	}
	,string_view
	{
		buf.data(), size_t(static_cast<snprintf &>(*this))
	}{}
};
