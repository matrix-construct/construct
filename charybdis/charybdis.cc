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
#include "lgetopt.h"
#include "charybdis.h"

namespace fs = ircd::fs;

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
	{ "console",    &cmdline,         lgetopt::BOOL,    "Interrupt to a command line immediately after startup" },
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

// TODO: XXX: This has to be declared first before any modules
// are loaded otherwise the module will not be unloadable.
boost::asio::ip::tcp::acceptor _dummy_acceptor_ { *ios };
boost::asio::ip::tcp::socket _dummy_sock_ { *ios };
boost::asio::ip::tcp::resolver _dummy_resolver_ { *ios };

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

	const std::string confpath(configfile?: fs::get(fs::IRCD_CONF));
	ircd::init(*ios, confpath);

	sigs.add(SIGHUP);
	sigs.add(SIGINT);
	sigs.add(SIGTSTP);
	sigs.add(SIGQUIT);
	sigs.add(SIGTERM);
	sigs.add(SIGUSR1);
	sigs.add(SIGUSR2);
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
catch(const ircd::user_error &e)
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

	printf("VERSION :boost %d\n", BOOST_VERSION);
}

bool startup_checks()
try
{
	#ifndef _WIN32
	if(geteuid() == 0)
		throw ircd::error("Don't run ircd as root!!!");
	#endif

	fs::chdir(fs::get(fs::PREFIX));
	return true;
}
catch(const std::exception &e)
{
	fprintf(stderr, usererrstr, e.what());
	return false;
}

static void handle_usr2();
static void handle_usr1();
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
		case SIGUSR1:  handle_usr1();           break;
		case SIGUSR2:  handle_usr2();           break;
		case SIGINT:   handle_interruption();   break;
		case SIGTSTP:  handle_termstop();       break;
		case SIGHUP:   handle_hangup();         break;
		case SIGQUIT:  handle_quit();           return;
		case SIGTERM:  handle_quit();           return;
		default:                                break;
	}

	sigs.async_wait(sigfd_handler);
}

void
handle_quit()
try
{
	console_cancel();
	ircd::stop();
}
catch(const std::exception &e)
{
	ircd::log::error("SIGQUIT handler: %s", e.what());
}

void
handle_usr1()
try
{
	// Do ircd rehash config
}
catch(const std::exception &e)
{
	ircd::log::error("SIGUSR1 handler: %s", e.what());
}

void
handle_usr2()
try
{
	// Do ircd rehash bans
	// Do refresh motd
}
catch(const std::exception &e)
{
	ircd::log::error("SIGUSR2 handler: %s", e.what());
}

void
handle_hangup()
try
{
	console_hangup();
}
catch(const std::exception &e)
{
	ircd::log::error("SIGHUP handler: %s", e.what());
}

void
handle_termstop()
try
{
	console_termstop();
}
catch(const std::exception &e)
{
	ircd::log::error("SIGTSTP handler: %s", e.what());
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
	ircd::log::error("SIGINT handler: %s", e.what());
}
