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

#include "stdinc.h"
#include "util.h"

#ifdef __cplusplus
inline namespace ircd {


IRCD_OVERLOAD(pass_name)

/** The root exception type.
 *
 *  All exceptions in the project inherit from this type.
 *  To catch any exception from a project developer's code:
 *    catch(const ircd::exception &) {}
 *
 *  Remember: not all exceptions are from project developer's code,
 *  such as std::out_of_range. In most contexts if you have to deal with this
 *  someone else was lazy, which is bad. To be sure and catch any exception:
 *    catch(const std::exception &) {}
 *
 *  Remember: not all exceptions have to inherit from std::exception, but
 *  those are rogue exceptions. We do not allow this. To be sure nothing
 *  can possibly get through, add to the bottom:
 *    catch(...) {}
 *
 *  Note: Prefer 'noexcept' instead of catch(...), noexcept is like an 'assert'
 *  for exceptions, and the rogue can be found-out in testing.
 */
struct exception
:std::exception
{
	char buf[1024];

	const char *what() const noexcept override   { return buf;                                     }

	exception(const pass_name_t, const char *const &name = "exception", const char *fmt = " ", ...) noexcept AFP(4, 5);
};


// Disambiguates the constructors in the macro below.

/** Exception generator convenience macro
 *
 *  If you want to create your own exception type, you have found the right
 *  place! This macro allows creating an exception in the the hierarchy.
 *
 *  To create an exception, invoke this macro in your header. Examples:
 *
 *     IRCD_EXCEPTION(ircd::exception, my_exception)
 *     IRCD_EXCEPTION(my_exception, my_specific_exception)
 *
 *  Then your catch sequence can look like the following:
 *
 *     catch(const my_specific_exception &e)
 *     {
 *         log("something specifically bad happened: %s", e.what());
 *     }
 *     catch(const my_exception &e)
 *     {
 *         log("something generically bad happened: %s", e.what());
 *     }
 *     catch(const ircd::exception &e)
 *     {
 *         log("unrelated bad happened: %s", e.what());
 *     }
 *     catch(const std::exception &e)
 *     {
 *         log("unhandled bad happened: %s", e.what());
 *     }
 *
 *  Remember: the order of the catch blocks is important.
 */
#define IRCD_EXCEPTION(parent, name)                       \
struct name                                                \
:parent                                                    \
{                                                          \
    template<class... A>                                   \
    name(const pass_name_t &, A&&... a):                   \
    parent { pass_name, std::forward<A>(a)... } {}         \
                                                           \
    template<class... A>                                   \
    name(A&&... a):                                        \
    parent { pass_name, #name, std::forward<A>(a)... } {}  \
};


/** Generic base exceptions.
 *  List your own exception somewhere else (unless you're overhauling libircd).
 *
 *  These are useful for random places and spare you the effort of having to
 *  redefine them. You can also inherit from these if appropriate.
 */
IRCD_EXCEPTION(exception,    error)             // throw ircd::error("something bad")
IRCD_EXCEPTION(error,        not_found)         // throw ircd::not_found("looks like a 404")
IRCD_EXCEPTION(error,        access_denied)     // throw ircd::access_denied("you didn't say the magic word")


}       // namespace ircd
#endif  // __cplusplus
