/*
 *  Copyright (C) 2016 Charybdis Development Team
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

#include <ircd/ircd.h>
#include <boost/asio.hpp>
#include <ircd/ctx_ctx.h>

namespace path = ircd::path;
using ircd::lgetopt;

static void console_spawn();
static void sigfd_handler(const boost::system::error_code &, int);
static bool startup_checks();
static void print_version();

const char *const fatalerrstr
{R"(
***
*** A fatal error has occurred. Please contact the developer with the message below.
*** Create a coredump by reproducing the error using the -debug command-line option.
***
%s
)"};

const char *const usererrstr
{R"(
***
*** A fatal startup error has occurred. Please fix the problem to continue. ***
***
%s
)"};

bool printversion;
bool testing_conf;
bool cmdline;
const char *configfile;
lgetopt opts[] =
{
	{ "help",       nullptr,          lgetopt::USAGE,   "Print this text" },
	{ "version",    &printversion,    lgetopt::BOOL,    "Print version and exit" },
	{ "configfile", &configfile,      lgetopt::STRING,  "File to use for ircd.conf" },
	{ "conftest",   &testing_conf,    lgetopt::YESNO,   "Test the configuration files and exit" },
	{ "debug",      &ircd::debugmode, lgetopt::BOOL,    "Enable options for debugging" },
	{ "cmd",        &cmdline,         lgetopt::BOOL,    "Interrupt to a command line immediately after startup" },
	{ nullptr,      nullptr,          lgetopt::STRING,  nullptr },
};

boost::asio::io_service *ios
{
	// Having trouble with static destruction in clang so this
	// has to become still-reachable
	new boost::asio::io_service
};

boost::asio::signal_set sigs
{
	*ios
};

int main(int argc, char *const *argv)
try
{
	umask(077);       // better safe than sorry --SRB

	parseargs(&argc, &argv, opts);
	if(!startup_checks())
		return 1;

	if(printversion)
	{
		print_version();
		return 0;
	}

	const std::string confpath(configfile?: path::get(path::IRCD_CONF));
	ircd::init(*ios, confpath);

	sigs.add(SIGHUP);
	sigs.add(SIGINT);
	sigs.add(SIGTSTP);
	sigs.add(SIGQUIT);
	sigs.add(SIGTERM);
	ircd::at_main_exit([]
	{
		// Entered when IRCd's main context has finished. ios.run() won't
		// return because our signal handler out here is still using it.
		sigs.cancel();
	});
	sigs.async_wait(sigfd_handler);

	if(cmdline)
		console_spawn();

	ios->run();  // Blocks until a clean exit or an exception comes out of it.
}
catch(const ircd::conf::newconf::syntax_error &e)
{
	if(ircd::debugmode)
		throw;

	fprintf(stderr, usererrstr, e.what());
	return EXIT_FAILURE;
}
catch(const std::exception &e)
{
	if(ircd::debugmode)
		throw;

	/*
	* Why EXIT_FAILURE here?
	* Because if ircd_die_cb() is called it's because of a fatal
	* error inside libcharybdis, and we don't know how to handle the
	* exception, so it is logical to return a FAILURE exit code here.
	*    --nenolod
	*
	* left the comment but the code is gone -jzk
	*/
	fprintf(stderr, fatalerrstr, e.what());
	return EXIT_FAILURE;
}

void print_version()
{
	printf("VERSION :%s\n",
	       ircd::info::version.c_str());

	#ifdef CUSTOM_BRANDING
	printf("VERSION :based on %s-%s\n",
	       PACKAGE_NAME,
	       PACKAGE_VERSION);
	#endif

	printf("VERSION :%s\n", rb_lib_version());
}

bool startup_checks()
try
{
	#ifndef _WIN32
	if(geteuid() == 0)
		throw ircd::error("Don't run ircd as root!!!");
	#endif

	path::chdir(path::get(path::PREFIX));
	return true;
}
catch(const std::exception &e)
{
	fprintf(stderr, usererrstr, e.what());
	return false;
}

const char *const generic_message
{R"(
*** - To end the console session, type ctrl-d             -> EOF
*** - To exit cleanly, type DIE or ctrl-\                 -> SIGQUIT
*** - To exit immediately, type EXIT                      -> exit(0)
*** - To generate a coredump for developers, type ABORT   -> abort()
***
)"};

bool console_active;
ircd::ctx::ctx *console_ctx;
boost::asio::posix::stream_descriptor *console_in;

static void handle_line(const std::string &line);
static void console();
static void console_cancel();
static void handle_quit();
static void handle_interruption();
static void handle_termstop();
static void handle_hangup();

void
sigfd_handler(const boost::system::error_code &ec,
              int signum)
{
	switch(ec.value())
	{
		using namespace boost::system::errc;

		case success:
			break;

		case operation_canceled:
			console_cancel();
			return;

		default:
			console_cancel();
			throw std::runtime_error(ec.message());
	}

	switch(signum)
	{
		case SIGINT:   handle_interruption();   break;
		case SIGTSTP:  handle_termstop();       break;
		case SIGHUP:   handle_hangup();         break;
		case SIGQUIT:  handle_quit();           return;
		case SIGTERM:                           return;
		default:                                break;
	}

	sigs.async_wait(sigfd_handler);
}

void
handle_quit()
try
{
	console_cancel();
}
catch(const std::exception &e)
{
	ircd::log::error("SIGQUIT handler: %s", e.what());
}

void
handle_hangup()
try
{
	using namespace ircd;
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
	ircd::log::error("SIGHUP handler: %s", e.what());
}

const char *const termstop_message
{R"(
***
*** The server has been paused and will resume when you hit enter.
*** This is an IRC client and your commands will originate from the
*** server itself.
***)"};

void
handle_termstop()
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
	std::cerr << "\033[1;31merror\033[0m: " << e.what() << std::endl;
}

void
handle_interruption()
try
{
	console_cancel();
	console_spawn();
}
catch(const std::exception &e)
{
	std::cerr << "\033[1;31merror\033[0m: " << e.what() << std::endl;
}

void
console_spawn()
{
	if(console_active)
		return;

	// The console function is executed asynchronously.
	// This call may or may not return immediately.
	ircd::context context(std::bind(&console));

	// The console may be joined unless it is released here;
	// now cleanup must occur by other means.
	context.detach();
}

const char *const console_message
{R"(
***
*** The server is still running in the background. A command line
*** is now available below. This is an IRC client and your commands
*** will originate from the server itself.
***)"};

void
console()
try
{
	using namespace ircd;

	const scope atexit([]
	{
		console_active = false;
		console_in = nullptr;
		free(console_ctx);
	});

	console_active = true;
	console_ctx = &ctx::cur();

	std::cout << console_message << generic_message;

	boost::asio::posix::stream_descriptor in{*::ios, dup(STDIN_FILENO)};
	console_in = &in;

	boost::asio::streambuf buf{BUFSIZE};
	std::istream is{&buf};
	std::string line;

	while(1) try
	{
		std::cout << "\n> " << std::flush;

		// Suppression scope ends after the command is entered
		// so the output of the command (log messages) can be seen.
		{
			const log::console_quiet quiet(false);
			boost::asio::async_read_until(in, buf, '\n', yield(continuation()));
		}

		std::getline(is, line);
		if(!line.empty())
			handle_line(line);
	}
	catch(const ircd::cmds::not_found &e)
	{
		std::cerr << e.what() << std::endl;
	}
}
catch(const std::exception &e)
{
	std::cout << "\n***" << std::endl;
	std::cout << "*** The console session has ended: " << e.what() << std::endl;
	std::cout << "***" << std::endl;
}

void
console_cancel()
{
	if(!console_active)
		return;

	if(!console_in)
		return;

	console_in->cancel();
}

void
handle_line(const std::string &line)
{
	if(line == "ABORT")
		abort();

	if(line == "EXIT")
		exit(0);

	ircd::execute(ircd::me, line);
}
