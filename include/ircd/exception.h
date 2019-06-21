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
#define HAVE_IRCD_EXCEPTION_H

/// Forward declarations for boost::system because it is not included here.
namespace boost::system
{
	struct error_code;
	struct error_category;
	struct system_error;
	namespace errc {}
}

namespace ircd
{
	struct terminate;
	struct exception; // Root exception

	// util
	bool system_category(const std::error_category &) noexcept;
	bool system_category(const std::error_code &) noexcept;
	bool system_category(const boost::system::error_code &) noexcept;
	bool is(const std::error_code &, const std::errc &) noexcept;
	bool is(const boost::system::error_code &, const std::errc &) noexcept;
	std::error_code make_error_code(const int &code = errno);
	std::error_code make_error_code(const std::error_code &);
	std::error_code make_error_code(const std::system_error &);
	std::error_code make_error_code(const boost::system::error_code &);
	std::error_code make_error_code(const boost::system::system_error &);
	std::system_error make_system_error(const int &code = errno);
	std::system_error make_system_error(const std::errc &);
	std::system_error make_system_error(const std::error_code &);
	std::system_error make_system_error(const boost::system::error_code &);
	std::system_error make_system_error(const boost::system::system_error &);
	template<class... args> std::exception_ptr make_system_eptr(args&&...);
	template<class... args> [[noreturn]] void throw_system_error(args&&...);
	template<class E, class... args> std::exception_ptr make_exception_ptr(args&&...);

	string_view string(const mutable_buffer &, const std::error_code &);
	string_view string(const mutable_buffer &, const std::system_error &);
	string_view string(const mutable_buffer &, const boost::system::error_code &);
	string_view string(const mutable_buffer &, const boost::system::system_error &);
	std::string string(const std::error_code &);
	std::string string(const std::system_error &);
	std::string string(const boost::system::error_code &);
	std::string string(const boost::system::system_error &);

	void panicking(const std::exception &) noexcept;
	void panicking(const std::exception_ptr &) noexcept;
	void _aborting_() noexcept;
}

/// The root exception type.
///
/// All exceptions in the project inherit from this type. We generally don't
/// have catch blocks on this type. Instead we use `ircd::error` to catch
/// project-specific exceptions. This gives us just a little more indirection
/// to play with before inheriting from std::exception.
///
/// Not all exceptions are from project developer's code, such as
/// `std::out_of_range`, etc. It's not necessarily bad to just catch
/// `std::exception` and we do it often enough, but be considerate.
///
/// Remember: Not all exceptions have to inherit from `std::exception` either.
/// We have only one example of this: ctx::terminated. To be sure nothing can
/// possibly get through you can `catch(...)` but with extreme care that you
/// are not discarding a termination which will hang the `ircd::ctx` you're on.
///
/// !!!
/// NOTE: CONTEXT SWITCHES CANNOT OCCUR INSIDE CATCH BLOCKS UNLESS YOU USE THE
/// MITIGATION TOOLS PROVIDED IN `ircd::ctx` WHICH RESULT IN THE LOSS OF THE
/// ABILITY TO RETHROW THE EXCEPTION (i.e `throw` statement). BEST PRACTICE
/// IS TO RETURN CONTROL FROM THE CATCH BLOCK BEFORE CONTEXT SWITCHING.
/// !!!
struct ircd::exception
:virtual std::exception
{
	static constexpr const size_t &BUFSIZE
	{
		512UL
	};

  protected:
	IRCD_OVERLOAD(generate_skip)

	char buf[BUFSIZE];

	ssize_t generate(const char *const &name, const string_view &fmt, const va_rtti &ap) noexcept;
	ssize_t generate(const string_view &fmt, const va_rtti &ap) noexcept;

  public:
	const char *what() const noexcept override;

	exception(generate_skip_t = {}) noexcept
	{
		buf[0] = '\0';
	}

	exception(exception &&) = delete;
	exception(const exception &) = delete;
	exception &operator=(exception &&) = delete;
	exception &operator=(const exception &) = delete;
};

/// Always prefer ircd::terminate() to std::terminate() for all project code.
struct ircd::terminate
{
	[[noreturn]] terminate(const std::exception &) noexcept;
	[[noreturn]] terminate(std::exception_ptr) noexcept;
	[[noreturn]] terminate() noexcept;
};

/// Exception generator convenience macro
///
/// If you want to create your own exception type, you have found the right
/// place! This macro allows creating an exception in the the hierarchy.
///
/// To create an exception, invoke this macro in your header. Examples:
///
///    IRCD_EXCEPTION(ircd::exception, my_exception)
///    IRCD_EXCEPTION(my_exception, my_specific_exception)
///
/// Then your catch sequence can look like the following:
///
///    catch(const my_specific_exception &e)
///    {
///        log("something specifically bad happened: %s", e.what());
///    }
///    catch(const my_exception &e)
///    {
///        log("something generically bad happened: %s", e.what());
///    }
///    catch(const ircd::exception &e)
///    {
///        log("unrelated bad happened: %s", e.what());
///    }
///    catch(const std::exception &e)
///    {
///        log("unhandled bad happened: %s", e.what());
///    }
///
/// Remember: the order of the catch blocks is important.
///
#define IRCD_EXCEPTION(parent, name)                                          \
struct name                                                                   \
:parent                                                                       \
{                                                                             \
    template<class... args>                                                   \
    name(const string_view &fmt, args&&... ap) noexcept                       \
    :parent{generate_skip}                                                    \
    {                                                                         \
        generate(#name, fmt, ircd::va_rtti{std::forward<args>(ap)...});       \
    }                                                                         \
                                                                              \
    template<class... args>                                                   \
    name(const string_view &fmt = " ") noexcept                               \
    :parent{generate_skip}                                                    \
    {                                                                         \
        generate(#name, fmt, ircd::va_rtti{});                                \
    }                                                                         \
                                                                              \
    name(generate_skip_t) noexcept                                            \
    :parent{generate_skip}                                                    \
    {                                                                         \
    }                                                                         \
};

/// Hides the name of the exception when generating a string
#define IRCD_EXCEPTION_HIDENAME(parent, name)                                 \
struct name                                                                   \
:parent                                                                       \
{                                                                             \
    template<class... args>                                                   \
    name(const string_view &fmt, args&&... ap) noexcept                       \
    :parent{generate_skip}                                                    \
    {                                                                         \
        generate(fmt, ircd::va_rtti{std::forward<args>(ap)...});              \
    }                                                                         \
                                                                              \
    template<class... args>                                                   \
    name(const string_view &fmt = " ") noexcept                               \
    :parent{generate_skip}                                                    \
    {                                                                         \
        generate(fmt, ircd::va_rtti{});                                       \
    }                                                                         \
                                                                              \
    name(generate_skip_t = {}) noexcept                                       \
    :parent{generate_skip}                                                    \
    {                                                                         \
    }                                                                         \
};

/// Creates a panic-type exception.
///
/// Throwable which will terminate on construction in debug mode but throw
/// normally in release mode. Ideally this should never be thrown in release
/// mode because the termination in debug means a test can never pass and
/// the triggering callsite should be eliminated. Nevertheless it throws
/// normally in release mode for recovering at an exception handler.
#define IRCD_PANICKING(parent, name)                                          \
struct name                                                                   \
:parent                                                                       \
{                                                                             \
    template<class... args>                                                   \
    name(const string_view &fmt, args&&... ap) noexcept                       \
    :parent{generate_skip}                                                    \
    {                                                                         \
        generate(#name, fmt, ircd::va_rtti{std::forward<args>(ap)...});       \
        ircd::panicking(*this);                                               \
    }                                                                         \
                                                                              \
    template<class... args>                                                   \
    name(const string_view &fmt = " ") noexcept                               \
    :parent{generate_skip}                                                    \
    {                                                                         \
        generate(#name, fmt, ircd::va_rtti{});                                \
        ircd::panicking(*this);                                               \
    }                                                                         \
                                                                              \
    name(generate_skip_t)                                                     \
    :parent{generate_skip}                                                    \
    {                                                                         \
    }                                                                         \
};

namespace ircd
{
	/// Root error exception type. Inherit from this.
	/// List your own exception somewhere else (unless you're overhauling libircd).
	/// example, in your namespace:
	///
	/// IRCD_EXCEPTION(ircd::error, error)
	///
	IRCD_EXCEPTION(exception, error)             // throw ircd::error("something bad")
	IRCD_EXCEPTION(error, user_error)            // throw ircd::user_error("something silly")

	// panic errors; see IRCD_PANICKING docs.
	IRCD_PANICKING(exception, panic)
	IRCD_PANICKING(panic, not_implemented)
}

template<class E,
         class... args>
std::exception_ptr
ircd::make_exception_ptr(args&&... a)
try
{
	throw E{std::forward<args>(a)...};
}
catch(const E &)
{
	return std::current_exception();
};

template<class... args>
void
ircd::throw_system_error(args&&... a)
{
	throw make_system_error(std::forward<args>(a)...);
}

template<class... args>
std::exception_ptr
ircd::make_system_eptr(args&&... a)
{
	return std::make_exception_ptr(make_system_error(std::forward<args>(a)...));
}
