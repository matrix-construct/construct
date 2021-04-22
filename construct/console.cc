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
#include "construct.h"
#include "console.h"

using namespace ircd;

IRCD_EXCEPTION_HIDENAME(ircd::error, bad_command)

decltype(construct::console)
construct::console;

decltype(construct::console::stack_sz)
construct::console::stack_sz
{
	{ "name",     "construct.console.stack.size" },
	{ "default",  long(2_MiB)                    },
};

decltype(construct::console::input_max)
construct::console::input_max
{
	{ "name",     "construct.console.input.max" },
	{ "default",  long(64_KiB)                  },
};

decltype(construct::console::ratelimit_sleep)
construct::console::ratelimit_sleep
{
	{ "name",     "construct.console.ratelimit.sleep" },
	{ "default",  75L                                 },
};

decltype(construct::console::ratelimit_bytes)
construct::console::ratelimit_bytes
{
	{ "name",     "construct.console.ratelimit.bytes" },
	{ "default",  long(2_KiB)                         },
};

decltype(construct::console::generic_message)
construct::console::generic_message
{R"(
*** - To end the console session: type exit, or ctrl-d    -> EOF
*** - To shutdown cleanly: type die, or ctrl-\            -> SIGQUIT
*** - To generate a coredump for developers, type ABORT   -> abort()
***)"_sv
};

decltype(construct::console::console_message)
construct::console::console_message
{R"(
***
*** The server is still running in the background. This is the
*** terminal console also available in your !control room.
***)"_sv
};

decltype(construct::console::seen_message)
construct::console::seen_message;

decltype(construct::console::queue)
construct::console::queue;

decltype(construct::console::quit_when_done)
construct::console::quit_when_done;

bool
construct::console::spawn()
{
	if(active())
		return false;

	construct::console = new console;
	return true;
}

bool
construct::console::interrupt()
{
	if(active())
	{
		construct::console->context.interrupt();
		return true;
	}
	else return false;
}

bool
construct::console::terminate()
{
	if(active())
	{
		construct::console->context.terminate();
		return true;
	}
	else return false;
}

bool
construct::console::active()
{
	return construct::console != nullptr;
}

//
// console::console
//

construct::console::console()
:outbuf
{
	1_MiB
}
,context
{
	"console",
	stack_sz,
	std::bind(&console::main, this),
	ircd::context::DISPATCH | ircd::context::SLICE_EXEMPT,
}
,runlevel_changed
{
	std::bind(&console::on_runlevel, this, std::placeholders::_1)
}
{
}

void
construct::console::main()
{
	const unwind dtor{[]
	{
		assert(construct::console);
		construct::console->context.detach();
		delete construct::console;
		construct::console = nullptr;
	}};

	if(!wait_running())
		return;

	ircd::module module
	{
		"console"
	};

	this->module = &module;
	loop();
}

void
construct::console::loop()
try
{
	if(next_command())
		if(handle_queued())
			return;

	show_message(); do
	{
		ctx::interruption_point();
		wait_input();
	}
	while(handle_line());
}
catch(const std::exception &e)
{
	std::cout
	<< "\n***"
	<< "\n*** The console session has ended: " << e.what()
	<< "\n***"
	<< std::endl;

	log::debug
	{
		"The console session has ended: %s", e.what()
	};
}
catch(...)
{
	log::debug
	{
		"The console session has terminated."
	};
}

bool
construct::console::handle_queued()
{
	while(handle_line())
		if(!next_command())
			break;

	if(!quit_when_done)
		return true;

	if(run::level != run::level::RUN)
		return true;

	static ircd::ios::descriptor descriptor
	{
		"construct.console.quit"
	};

	ircd::dispatch
	{
		descriptor, ios::defer, ircd::quit
	};

	return true;
}

bool
construct::console::handle_line()
try
{
	if(line == "ABORT")
		abort();

	if(line == "TERMINATE")
		std::terminate();

	if(line == "terminate")
		ircd::terminate();

	if(line == "EXIT")
		exit(0);

	if(line == "TRAP")
	{
		ircd::debugtrap();
		return true;
	}

	if(startswith(line, "record"))
		return cmd__record();

	if(startswith(line, "watch"))
		return cmd__watch();

	int ret{-1};
	if(module) switch((ret = handle_line_bymodule()))
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
catch(const http::error &e)
{
	log::error
	{
		"%s %s", e.what(), e.content
	};

	return true;
}
catch(const std::exception &e)
{
	log::error
	{
		"%s", e.what()
	};

	return true;
}

int
construct::console::handle_line_bymodule()
{
	using prototype = int (std::ostream &, const string_view &, const string_view &);

	const mods::import<prototype> command
	{
		*module, "console_command"
	};

	std::ostringstream out;
	out.exceptions(out.badbit | out.failbit | out.eofbit);
	pubsetbuf(out, outbuf);

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

			const string_view str
			{
				view(out, outbuf)
			};

			// If this string is set, the user wants to log a copy of the output
			// to the file at this path.
			if(!empty(record_path))
			{
				const fs::fd fd
				{
					record_path, std::ios::out | std::ios::app
				};

				// Generate a copy of the command line to give some context
				// to the output following it.
				const std::string cmdline
				{
					"\n> "s + line + "\n\n"
				};

				append(fd, string_view(cmdline));
				append(fd, string_view(str));
			}

			// The string is iterated for rate-limiting. After a configured
			// number of bytes sent to stdout we sleep the ircd::ctx for a
			// configured number of milliseconds. If these settings are too
			// aggressive then the output heading to stdout won't appear in
			// the terminal after the buffers are filled.
			size_t off(0); if(off < str.size()) do
			{
				const string_view substr
				{
					str.data() + off, std::min(str.size() - off, size_t(ratelimit_bytes))
				};

				std::cout << substr << std::flush;
				off += size_t(ratelimit_bytes);
				if(off >= str.size())
					break;

				ctx::sleep(milliseconds(ratelimit_sleep));
			}
			while(1);

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
			view(out, outbuf)
		};

		// Command isn't handled by the module; continue handling here
		default:
			break;
	}

	return ret;
}

bool
construct::console::cmd__record()
{
	const string_view &args
	{
		tokens_after(line, ' ', 0)
	};

	if(empty(args) && empty(record_path))
	{
		std::cout << "Console not currently recorded to any file." << std::endl;
		return true;
	}

	if(empty(args) && !empty(record_path))
	{
		std::cout << "Stopped recording to file `" << record_path << "'" << std::endl;
		record_path = {};
		return true;
	}

	const auto path
	{
		token(args, ' ', 0)
	};

	std::cout << "Recording console to file `" << path << "'" << std::endl;
	record_path = path;
	return true;
}

bool
construct::console::cmd__watch()
{
	const auto delay
	{
		lex_cast<double>(token(this->line, ' ', 1))
	};

	const ircd::milliseconds sleep_time
	{
		long(delay * 1000.0)
	};

	const string_view &line
	{
		tokens_after(this->line, ' ', 1)
	};

	this->line = line; do
	{
		const ircd::ctx::uninterruptible::nothrow ui;

		std::cout << '\n';
		handle_line(); try
		{
			const log::console_quiet quiet(false);
			ctx::interruptible(ctx::cur(), true);
			ctx::interruption_point();
			ctx::sleep(sleep_time);
		}
		catch(const ctx::interrupted &)
		{
			break;
		}
	}
	while(1);

	return true;
}

void
construct::console::wait_input()
{
	line = {}; do
	{
		// Suppression scope ends after the command is entered
		// so the output of the command (if log messages) can be seen.
		const log::console_quiet quiet(false);
		std::cout << "\n> " << std::flush;

		line.resize(size_t(input_max));
		const mutable_buffer buffer
		{
			const_cast<char *>(line.data()), line.size()
		};

		const string_view read
		{
			fs::stdin::readline(buffer)
		};

		line.resize(size(read));

		if(startswith(line, "\x1B"_sv))
			esc_handle();
	}
	while(line.empty());

	history.emplace_back(line);
}

bool
construct::console::esc_handle()
{
	if(startswith(line, "\x1B\x5B"_sv) && size(line) >= 3)
		return esc_handle_bra();

	line = {};
	return true;
}

bool
construct::console::esc_handle_bra()
{
	switch(line[2])
	{
		case 'A': // up-arrow
		{
			if(history.empty())
			{
				line = {};
				return false;
			}

			line = history.front();
			history.pop_front();
			return true;
		}
	}

	line = {};
	return true;
}

bool
construct::console::next_command()
{
	line = {};
	while(!queue.empty() && line.empty())
	{
		line = std::move(queue.front());
		queue.pop_front();
	}

	return !line.empty();
}

void
construct::console::on_runlevel(const enum ircd::run::level &runlevel)
{
	switch(runlevel)
	{
		case ircd::run::level::QUIT:
		case ircd::run::level::HALT:
			terminate();
			break;

		default:
			break;
	}
}

bool
construct::console::wait_running()
const
{
	ircd::run::changed::dock.wait([]
	{
		return ircd::run::level == ircd::run::level::RUN ||
		       ircd::run::level == ircd::run::level::QUIT ||
		       ircd::run::level == ircd::run::level::HALT;
	});

	return ircd::run::level == ircd::run::level::RUN;
}

void
construct::console::show_message()
const
{
	// Determine if the user is in -quiet mode or similar so we can skip this
	// output too. Note that the level given here is arbitrary, but if they
	// did suppress it we won't show this message either...
	if(!ircd::log::console_enabled(ircd::log::level::NOTICE))
		return;

	std::call_once(seen_message, []
	{
		std::cout << console_message << generic_message;
	});
}
