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

	struct spec;
	struct specifier;
	struct sprintf;
	struct vsprintf;
	struct snprintf;
	struct vsnprintf;
	struct snstringf;
	struct vsnstringf;
	template<size_t MAX> struct bsprintf;

	//
	// Module API
	//
	constexpr char SPECIFIER
	{
		'%'
	};

	constexpr char SPECIFIER_TERMINATOR
	{
		'$'
	};

	using arg = std::tuple<const void *, std::type_index>;

	const std::map<string_view, specifier *, std::less<>> &specifiers();
}

// Structural representation of a format specifier
struct ircd::fmt::spec
{
	char sign {'+'};
	ushort width {0};
	string_view name;

	spec() = default;
};

// A format specifier handler module.
// This allows a new "%foo" to be defined with custom handling.
class ircd::fmt::specifier
{
	std::set<std::string> names;

  public:
	virtual bool operator()(char *&out, const size_t &max, const spec &, const arg &) const = 0;

	specifier(const std::initializer_list<std::string> &names);
	specifier(const std::string &name);
	virtual ~specifier() noexcept;
};

//
// User API
//

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
/// Furthermore, other features of ircd::fmt enable custom format specifiers
/// and handling of types not recognized by existing grammars through this
/// function.
///
class ircd::fmt::snprintf
{
	const char *fstart;                          // Current running position in the fmtstr
	const char *fstop;                           // Saved state from the last position
	const char *fend;                            // past-the-end iterator of the fmtstr
	const char *obeg;                            // Saved beginning of the output buffer
	const char *oend;                            // past-the-end iterator of the output buffer
	char *out;                                   // Current running position of the output buffer
	short idx;                                   // Keeps count of the args for better err msgs

  protected:
	bool finished() const;
	size_t remaining() const;
	size_t consumed() const                      { return out - obeg;                              }
	auto &buffer() const                         { return obeg;                                    }

	void append(const char *const &begin, const char *const &end);
	void argument(const arg &);

	IRCD_OVERLOAD(internal)
	snprintf(internal_t, char *const &, const size_t &, const char *const &, const va_rtti &);

  public:
	operator ssize_t() const                     { return consumed();                              }
	operator string_view() const                 { return { obeg, consumed() };                    }

	template<class... Args>
	snprintf(char *const &buf,
	         const size_t &max,
	         const char *const &fmt,
	         Args&&... args)
	:snprintf
	{
		internal, buf, max, fmt, va_rtti{std::forward<Args>(args)...}
	}{}
};

struct ircd::fmt::sprintf
:snprintf
{
	template<class... Args>
	sprintf(const mutable_buffer &buf,
	        const char *const &fmt,
	        Args&&... args)
	:snprintf
	{
		internal, data(buf), size(buf), fmt, va_rtti{std::forward<Args>(args)...}
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
	          const char *const &fmt,
	          const va_rtti &ap)
	:snprintf
	{
		internal, buf, max, fmt, ap
	}{}
};

struct ircd::fmt::vsprintf
:snprintf
{
	vsprintf(const mutable_buffer &buf,
	         const char *const &fmt,
	         const va_rtti &ap)
	:snprintf
	{
		internal, data(buf), size(buf), fmt, ap
	}{}
};

struct ircd::fmt::vsnstringf
:std::string
{
	vsnstringf(const size_t &max,
	           const char *const &fmt,
	           const va_rtti &ap)
	:std::string
	{
		[&max, &fmt, &ap]
		{
			std::string ret(max, char{});
			ret.resize(vsnprintf(const_cast<char *>(ret.data()), ret.size() + 1, fmt, ap));
			return ret;
		}()
	}{}
};

struct ircd::fmt::snstringf
:vsnstringf
{
	template<class... args>
	snstringf(const size_t &max,
	          const char *const &fmt,
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
	bsprintf(const char *const &fmt,
	         args&&... a)
	:snprintf
	{
		internal, buf.data(), buf.size(), fmt, va_rtti{std::forward<args>(a)...}
	}
	,string_view
	{
		buf.data(), size_t(static_cast<snprintf &>(*this))
	}{}
};
