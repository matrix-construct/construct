/*
 * charybdis: 21st Century IRCd
 * exception.cc: Exception root
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

[[noreturn]] static void
ircd_terminate_handler()
noexcept
{
	std::abort();
}

void
ircd::aborting()
noexcept
{
	std::set_terminate(&ircd_terminate_handler);
}

void
ircd::throw_system_error(const int &code)
{
	throw std::system_error(code, std::system_category());
}

std::exception_ptr
ircd::make_system_error(const int &code)
{
	return std::make_exception_ptr(std::system_error(code, std::system_category()));
}

ssize_t
ircd::exception::generate(const char *const &fmt,
                          const va_rtti &ap)
noexcept
{
	return fmt::vsnprintf(buf, sizeof(buf), fmt, ap);
}

ssize_t
ircd::exception::generate(const char *const &name,
                          const char *const &fmt,
                          const va_rtti &ap)
noexcept
{
	size_t size(0);
	const bool empty(!fmt || !fmt[0] || fmt[0] == ' ');
	size = strlcat(buf, name, sizeof(buf));
	size = strlcat(buf, empty? "." : ": ", sizeof(buf));
	if(size < sizeof(buf))
		size += fmt::vsnprintf(buf + size, sizeof(buf) - size, fmt, ap);

	return size;
}

void
ircd::assertion()
noexcept(RB_DEBUG_LEVEL)
{
	if(std::uncaught_exceptions())
	{
		assertion(std::current_exception());
	} else {
		log::critical("IRCd Assertion without active exception.");
		assert(0);
		throw assertive{};
	}
}

void
ircd::assertion(std::exception_ptr eptr)
noexcept(RB_DEBUG_LEVEL) try
{
	std::rethrow_exception(eptr);
}
catch(const std::exception &e)
{
	assertion(e);
}

void
ircd::assertion(const std::exception &e)
noexcept(RB_DEBUG_LEVEL)
{
	#ifdef RB_DEBUG
		terminate(e);
	#else
		log::critical("IRCd Assertion %s", e.what());
		throw e;
	#endif
}

void
ircd::terminate()
noexcept
{
	terminate(std::current_exception());
}

void
ircd::terminate(std::exception_ptr eptr)
noexcept
{
	if(eptr) try
	{
		std::rethrow_exception(eptr);
	}
	catch(const std::exception &e)
	{
		terminate(e);
	}

	log::critical("IRCd Terminate without exception");
	std::terminate();
}

void
ircd::terminate(const std::exception &e)
noexcept
{
	log::critical("IRCd Terminated: %s", e.what());
	throw e;
}
