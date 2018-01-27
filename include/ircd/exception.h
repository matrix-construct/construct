/*
 * charybdis: 21st Century IRCd
 * exception.h: Exception root
 *
 * Copyright (C) 2016 Charybdis Development Team
 * Copyright (C) 2016 Jason Volk <jason@zemos.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#pragma once
#define HAVE_IRCD_EXCEPTION_H

namespace ircd
{
	// Root exception
	struct exception;

	// Prefer ircd::terminate() to std::terminate() if possible.
	[[noreturn]] void terminate(const std::exception &) noexcept;
	[[noreturn]] void terminate(std::exception_ptr) noexcept;
	[[noreturn]] void terminate() noexcept;

	// Terminates in debug mode; throws in release mode; always logs critical.
	[[noreturn]] void assertion(const std::exception &) noexcept(RB_DEBUG_LEVEL);
	[[noreturn]] void assertion(std::exception_ptr) noexcept(RB_DEBUG_LEVEL);
	[[noreturn]] void assertion() noexcept(RB_DEBUG_LEVEL);

	// util
	std::exception_ptr make_system_error(const int &code = errno);
	[[noreturn]] void throw_system_error(const int &code = errno);

	// Can be used to clobber the std::terminate_handler
	void aborting() noexcept;
}

/// The root exception type.
///
/// All exceptions in the project inherit from this type.
/// To catch any exception from a project developer's code:
///   catch(const ircd::exception &) {}
///
/// Remember: not all exceptions are from project developer's code,
/// such as std::out_of_range. In most contexts if you have to deal with this
/// someone else was lazy, which is bad. To be sure and catch any exception:
///   catch(const std::exception &) {}
///
/// Remember: not all exceptions have to inherit from std::exception, but
/// those are rogue exceptions. We do not allow this. To be sure nothing
/// can possibly get through, add to the bottom:
///   catch(...) {}
///
/// Note: Prefer 'noexcept' instead of catch(...), noexcept is like an 'assert'
/// for exceptions, and the rogue can be found-out in testing.
///
struct ircd::exception
:std::exception
{
  protected:
	IRCD_OVERLOAD(generate_skip)

	char buf[BUFSIZE];

	ssize_t generate(const char *const &name, const char *const &fmt, const va_rtti &ap) noexcept;
	ssize_t generate(const char *const &fmt, const va_rtti &ap) noexcept;

  public:
	const char *what() const noexcept override
	{
		return buf;
	}

	exception(const generate_skip_t = {}) noexcept
	{
		buf[0] = '\0';
	}
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
    name(const char *const &fmt = " ", args&&... ap) noexcept                 \
    :parent{generate_skip}                                                    \
    {                                                                         \
        generate(#name, fmt, va_rtti{std::forward<args>(ap)...});             \
    }                                                                         \
                                                                              \
    name(generate_skip_t) noexcept                                            \
    :parent{generate_skip}                                                    \
    {                                                                         \
    }                                                                         \
};                                                                            \

/// Hides the name of the exception when generating a string
#define IRCD_EXCEPTION_HIDENAME(parent, name)                                 \
struct name                                                                   \
:parent                                                                       \
{                                                                             \
    template<class... args>                                                   \
    name(const char *const &fmt = " ", args&&... ap) noexcept                 \
    :parent{generate_skip}                                                    \
    {                                                                         \
        generate(fmt, va_rtti{std::forward<args>(ap)...});                    \
    }                                                                         \
                                                                              \
    name(generate_skip_t = {}) noexcept                                       \
    :parent{generate_skip}                                                    \
    {                                                                         \
    }                                                                         \
};

/// Creates an assertion-type exception.
///
/// Throwable exception which will terminate on construction in debug mode
/// but throw normally in release mode. Ideally this should never be thrown
/// in release mode because the termination in debug means a test can never
/// pass and the triggering callsite should be eliminated. Nevertheless it
/// throws normally in release mode.
#define IRCD_ASSERTION(parent, name)                                          \
struct name                                                                   \
:parent                                                                       \
{                                                                             \
    template<class... args>                                                   \
    name(const char *const &fmt = " ", args&&... ap) noexcept(RB_DEBUG_LEVEL) \
    :parent{generate_skip}                                                    \
    {                                                                         \
        generate(#name, fmt, va_rtti{std::forward<args>(ap)...});             \
        ircd::assertion(*this);                                               \
    }                                                                         \
                                                                              \
    name(generate_skip_t) noexcept(RB_DEBUG_LEVEL)                            \
    :parent{generate_skip}                                                    \
    {                                                                         \
    }                                                                         \
};                                                                            \

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

	// Assertion errors; see IRCD_ASSERTION docs.
	IRCD_ASSERTION(exception, assertive)
	IRCD_ASSERTION(assertive, not_implemented)
}
