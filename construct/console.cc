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
*** - To end the console session: type exit, or ctrl-d    -> EOF
*** - To shutdown cleanly: type die, or ctrl-\            -> SIGQUIT
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
ircd::module *console_module;

static void check_console_active();
static void console_fini();
static void console_init();
static int handle_line_bymodule(const string_view &line);
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
		console,
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

void
console_init()
{
	console_cancel();
	console_active = true;
	console_ctx = &ctx::cur();
	console_module = new ircd::module{"console"};
}

void
console_fini()
{
	console_active = false;
	console_ctx = nullptr;
	delete console_module; console_module = nullptr;
	std::cin.clear();
}

const char *const console_message
{R"(
***
*** The server is still running in the background. This is the
*** terminal console also available in your !control room.
***)"};

void
console()
try
{
	ircd::runlevel_changed::dock.wait([]
	{
		return ircd::runlevel == ircd::runlevel::RUN ||
		       ircd::runlevel == ircd::runlevel::HALT;
	});

	if(ircd::runlevel == ircd::runlevel::HALT)
		return;

	const unwind atexit([]
	{
		console_fini();
	});

	console_init();
	std::cout << console_message << generic_message;
	while(1)
	{
		std::cout << "\n> " << std::flush;

		char buf[1024];
		string_view line;
		// Suppression scope ends after the command is entered
		// so the output of the command (if log messages) can be seen.
		{
			const log::console_quiet quiet(false);
			line = fs::stdin::readline(buf);
		}

		if(line.empty())
			continue;

		if(!handle_line(line))
			break;
	}

	std::cout << std::endl;
}
catch(const std::exception &e)
{
	std::cout << std::endl;
	std::cout << "***" << std::endl;
	std::cout << "*** The console session has ended: " << e.what() << std::endl;
	std::cout << "***" << std::endl;

	ircd::log::debug
	{
		"The console session has ended: %s", e.what()
	};
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
	const unwind atexit([]
	{
		console_fini();
	});

	console_init();
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
	ircd::log::error
	{
		"console_termstop(): %s", e.what()
	};
}

void
execute(const std::vector<std::string> lines)
try
{
	ircd::runlevel_changed::dock.wait([]
	{
		return ircd::runlevel == ircd::runlevel::RUN ||
		       ircd::runlevel == ircd::runlevel::HALT;
	});

	if(ircd::runlevel == ircd::runlevel::HALT)
		return;

	const unwind atexit([]
	{
		console_fini();
	});

	console_init();
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
	ircd::log::debug
	{
		"The execution aborted: %s", e.what()
	};
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

	if(line == "exit")
		return false;

	if(line == "die")
	{
		ircd::quit();
		return false;
	}

	int ret{-1};
	if(console_module) switch((ret = handle_line_bymodule(line)))
	{
		default:  break;
		case 0:   return false;
		case 1:   return true;
	}

	throw bad_command
	{
		"%s", line
	};
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
	ircd::log::error
	{
		"%s %s", e.what(), e.content
	};

	return true;
}
catch(const std::exception &e)
{
	ircd::log::error
	{
		"%s", e.what()
	};

	return true;
}

int
handle_line_bymodule(const string_view &line)
{
	using prototype = int (std::ostream &, const string_view &, const string_view &);
	const ircd::mods::import<prototype> command
	{
		*console_module, "console_command"
	};

	std::ostringstream out;
	out.exceptions(out.badbit | out.failbit | out.eofbit);

	int ret;
	static const string_view opts;
	switch((ret = command(out, line, opts)))
	{
		case 0:
		case 1:
		{
			// For this scope we have to suppress again because there's some
			// yielding business going on below where log messages can break
			// into the command output.
			const log::console_quiet quiet{false};

			const auto str
			{
				out.str()
			};

			static const size_t mlen(2048);
			for(size_t off(0); off < str.size(); off += mlen)
			{
				const string_view substr
				{
					str.data() + off, std::min(str.size() - off, mlen)
				};

				std::cout << substr << std::flush;
				ctx::sleep(milliseconds(50));
			}

			if(!endswith(str, '\n'))
				std::cout << std::endl;

			return ret;
		}

		// The command was handled but the arguments were bad_command.
		// The module has its own class for a bad_command exception which
		// is a local and separate symbol from the bad_command here so
		// we use this code to translate it.
		case -2: throw bad_command
		{
			"%s", out.str()
		};

		// Command isn't handled by the module; continue handling here
		default:
			break;
	}

	return ret;
}

void
check_console_active()
{
	if(console_active)
		throw ircd::error
		{
			"Console is already active and cannot be reentered"
		};
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
	ircd::log::error
	{
		"console_hangup(): %s", e.what()
	};
}

void
console_cancel()
try
{
	if(!console_active)
		return;

	if(console_ctx)
		interrupt(*console_ctx);
}
catch(const std::exception &e)
{
	ircd::log::error
	{
		"Interrupting console: %s", e.what()
	};
}
