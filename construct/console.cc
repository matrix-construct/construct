// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <ircd/ircd.h>
#include <ircd/asio.h>
#include <ircd/m/m.h>
#include <ircd/util/params.h>
#include "construct.h"

using namespace ircd;

IRCD_EXCEPTION_HIDENAME(ircd::error, bad_command)

const char *const generic_message
{R"(
*** - To end the console session: type ctrl-d             -> EOF
*** - To exit cleanly: type exit, die, or ctrl-\          -> SIGQUIT
*** - To generate a coredump for developers, type ABORT   -> abort()
***
)"};

const size_t stack_sz
{
	8_MiB
};

bool console_active;
bool console_inwork;
ircd::ctx::ctx *console_ctx;
boost::asio::posix::stream_descriptor *console_in;
ircd::module *console_module;

static void check_console_active();
static void console_fini();
static bool handle_line(const string_view &line);
static void execute(const std::vector<std::string> lines);
static void console();

void
console_spawn()
{
	check_console_active();
	ircd::context
	{
		"console",
		stack_sz,
		std::bind(&console),
		ircd::context::DETACH | ircd::context::POST
	};
}

void
console_execute(const std::vector<std::string> &lines)
{
	check_console_active();
	ircd::context
	{
		"execute",
		stack_sz,
		std::bind(&execute, lines),
		ircd::context::DETACH | ircd::context::POST
	};
}

const char *const console_message
{R"(
***
*** The server is still running in the background. A command line is now available below.
*** This is a client and your commands will originate from the server itself.
***)"};

void
console()
try
{
	if(ircd::runlevel != ircd::runlevel::RUN)
		return;

	const unwind atexit([]
	{
		std::cout << std::endl;
		console_fini();
	});

	console_active = true;
	console_ctx = &ctx::cur();

	console_in = new boost::asio::posix::stream_descriptor
	{
		*::ios, dup(STDIN_FILENO)
	};

	console_module = new ircd::module
	{
		"console.so"
	};

	std::cout << console_message << generic_message;

	boost::asio::streambuf buf{BUFSIZE};
	std::istream is{&buf};
	std::string line;

	while(1)
	{
		std::cout << "\n> " << std::flush;

		// Suppression scope ends after the command is entered
		// so the output of the command (if log messages) can be seen.
		{
			const log::console_quiet quiet(false);
			boost::asio::async_read_until(*console_in, buf, '\n', yield_context{to_asio{}});
		}

		std::getline(is, line);
		std::cin.clear();
		if(line.empty())
			continue;

		if(!handle_line(line))
			break;
	}
}
catch(const std::exception &e)
{
	std::cout << "***" << std::endl;
	std::cout << "*** The console session has ended: " << e.what() << std::endl;
	std::cout << "***" << std::endl;

	ircd::log::debug("The console session has ended: %s", e.what());
}

const char *const termstop_message
{R"(
***
*** The server has been paused and will resume when you hit enter.
*** This is a client and your commands will originate from the server itself.
***)"};

void
console_termstop()
try
{
	console_cancel();

	std::cout << termstop_message << generic_message;

	std::string line;
	std::cout << "\n> " << std::flush;
	std::getline(std::cin, line);
	if(std::cin.eof())
	{
		std::cout << std::endl;
		std::cin.clear();
		return;
	}

	handle_line(line);
}
catch(const std::exception &e)
{
	ircd::log::error("console_termstop(): %s", e.what());
}

void
execute(const std::vector<std::string> lines)
try
{
	if(ircd::runlevel != ircd::runlevel::RUN)
		return;

	const unwind atexit([]
	{
		console_active = false;
	});

	console_active = true;
	console_ctx = &ctx::cur();

	for(const auto &line : lines)
	{
		if(line.empty())
			continue;

		if(!handle_line(line))
			break;
	}
}
catch(const std::exception &e)
{
	std::cout << std::endl;
	std::cout << "***\n";
	std::cout << "*** The execution aborted: " << e.what() << "\n";
	std::cout << "***" << std::endl;

	std::cout << std::flush;
	std::cout.clear();

	ircd::log::debug("The execution aborted: %s", e.what());
}

bool
handle_line(const string_view &line)
try
{
	// _theirs is copied for recursive reentrance
	const unwind outwork{[console_inwork_theirs(console_inwork)]
	{
		console_inwork = console_inwork_theirs;
	}};

	console_inwork = true;

	if(line == "ABORT")
		abort();

	if(line == "EXIT")
		exit(0);

	if(line == "exit" || line == "die")
	{
		ircd::quit();
		return false;
	}

	if(console_module)
	{
		const ircd::mods::import<int (const string_view &, std::string &)> command
		{
			"console.so", "console_command"
		};

		int ret;
		std::string output;
		switch((ret = command(line, output)))
		{
			case 0:
			case 1:
				std::cout << output;
				if(endswith(output, '\n'))
					std::flush(std::cout);
				else
					std::cout << std::endl;

				return ret;

			// The command was handled but the arguments were bad_command.
			// The module has its own class for a bad_command exception which
			// is a local and separate symbol from the bad_command here so
			// we use this code to translate it.
			case -2:
				throw bad_command
				{
					"%s", output
				};

			// Command isn't handled by the module; continue handling here
			default:
				break;
		}
	}

	throw bad_command{};
}
catch(const std::out_of_range &e)
{
	std::cerr << "missing required arguments. " << std::endl;
	return true;
}
catch(const bad_command &e)
{
	std::cerr << "Bad command or file name: " << e.what() << std::endl;
	return true;
}
catch(const ircd::http::error &e)
{
	ircd::log::error("%s %s", e.what(), e.content);
	return true;
}
catch(const std::exception &e)
{
	ircd::log::error("%s", e.what());
	return true;
}

void
check_console_active()
{
	if(console_active)
		throw ircd::error("Console is already active and cannot be reentered");
}

void
console_hangup()
try
{
	using log::console_quiet;

	console_cancel();

	static console_quiet *quieted;
	if(!quieted)
	{
		log::notice("Suppressing console log output after terminal hangup");
		quieted = new console_quiet;
		return;
	}

	log::notice("Reactivating console logging after second hangup");
	delete quieted;
	quieted = nullptr;
}
catch(const std::exception &e)
{
	ircd::log::error("console_hangup(): %s", e.what());
}

void
console_cancel()
try
{
	if(!console_active)
		return;

	if(console_inwork && console_ctx)
	{
		interrupt(*console_ctx);
		return;
	}

	if(console_in)
	{
		console_in->cancel();
		console_in->close();
	}
}
catch(const std::exception &e)
{
	ircd::log::error("Interrupting console: %s", e.what());
}

void
console_fini()
{
	console_active = false;
	delete console_in; console_in = nullptr;
	delete console_module; console_module = nullptr;
	std::cin.clear();
}
