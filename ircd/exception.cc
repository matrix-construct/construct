// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <boost/system/system_error.hpp>

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

std::string
ircd::string(const boost::system::system_error &e)
{
	return string(e.code());
}

std::string
ircd::string(const boost::system::error_code &ec)
{
	return ircd::string(128, [&ec]
	(const mutable_buffer &buf)
	{
		return string(buf, ec);
	});
}

std::string
ircd::string(const std::system_error &e)
{
	return string(e.code());
}

std::string
ircd::string(const std::error_code &ec)
{
	return ircd::string(128, [&ec]
	(const mutable_buffer &buf)
	{
		return string(buf, ec);
	});
}

ircd::string_view
ircd::string(const mutable_buffer &buf,
             const boost::system::system_error &e)
{
	return string(buf, e.code());
}

ircd::string_view
ircd::string(const mutable_buffer &buf,
             const boost::system::error_code &ec)
{
	return string(buf, make_system_error(ec));
}

ircd::string_view
ircd::string(const mutable_buffer &buf,
             const std::system_error &e)
{
	return string(buf, e.code());
}

ircd::string_view
ircd::string(const mutable_buffer &buf,
             const std::error_code &ec)
{
	return fmt::sprintf
	{
		buf, "%s: %s", ec.category().name(), ec.message()
	};
}

std::system_error
ircd::make_system_error(const boost::system::system_error &e)
{
	return std::system_error
	{
		make_error_code(e.code()), e.what()
	};
}

std::system_error
ircd::make_system_error(const boost::system::error_code &ec)
{
	return std::system_error
	{
		make_error_code(ec)
	};
}

std::system_error
ircd::make_system_error(const int &code)
{
	return std::system_error
	{
		make_error_code(code)
	};
}

std::error_code
ircd::make_error_code(const boost::system::system_error &e)
{
	return std::error_code
	{
		e.code().value(), e.code().category()
	};
}

std::error_code
ircd::make_error_code(const boost::system::error_code &ec)
{
	return std::error_code
	{
		ec.value(), ec.category()
	};
}

std::error_code
ircd::make_error_code(const std::system_error &e)
{
	return e.code();
}

std::error_code
ircd::make_error_code(const int &code)
{
	return std::error_code
	{
		code, std::system_category()
	};
}

std::error_code
ircd::make_error_code(const std::error_code &ec)
{
	return ec;
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
	static const auto &default_message
	{
		"without exception"_sv
	};

	assertion(default_message);
}

void
ircd::assertion(const string_view &msg)
noexcept(RB_DEBUG_LEVEL)
{
	log::critical
	{
		"IRCd Assertion :%s", msg
	};

	if(std::uncaught_exceptions())
		assertion(std::current_exception());

	assert(0);
	throw assertive
	{
		"IRCd Assertion :%s", msg
	};
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
		log::critical
		{
			"IRCd Assertion %s", e.what()
		};

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

	log::critical
	{
		"IRCd Terminate without exception"
	};

	fputs("\nIRCd Terminate without exception\n", stderr);

	::fflush(stdout);
	::fflush(stderr);
	std::terminate();
}

void
ircd::terminate(const std::exception &e)
noexcept
{
	log::critical
	{
		"IRCd Terminated: %s", e.what()
	};

	fprintf(stderr, "\nIRCd Terminated: %s\n", e.what());

	::fflush(stdout);
	::fflush(stderr);
	std::terminate();
}
