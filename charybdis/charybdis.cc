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

namespace path = ircd::path;
using ircd::lgetopt;

bool printversion;
bool testing_conf;
lgetopt opts[] =
{
	{ "help", NULL, lgetopt::USAGE, "Print this text" },
	{ "version", &printversion, lgetopt::BOOL, "Print version and exit" },
	{ "configfile", &ircd::ConfigFileEntry.configfile, lgetopt::STRING, "File to use for ircd.conf" },
	{ "conftest", &testing_conf, lgetopt::YESNO, "Test the configuration files and exit" },
	{ "debug", &ircd::debugmode, lgetopt::BOOL, "Enable options for debugging" },
	{ NULL, NULL, lgetopt::STRING, NULL },
};

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

static void handle_interruption();
static void sigfd_handler(const boost::system::error_code &, int);
static bool startup_checks();
static void print_version();

boost::asio::io_service ios;
boost::asio::signal_set sigs(ios);

int main(int argc, char *const *argv)
try
{
	umask(077);       // better safe than sorry --SRB

	ircd::ConfigFileEntry.dpath = path::get(path::PREFIX);
	ircd::ConfigFileEntry.configfile = path::get(path::IRCD_CONF); // Server configuration file
	ircd::ConfigFileEntry.connect_timeout = 30; // Default to 30

	parseargs(&argc, &argv, opts);
	if(!startup_checks())
		return 1;

	if(printversion)
	{
		print_version();
		return 0;
	}

	sigs.add(SIGINT);
	sigs.add(SIGQUIT);
	sigs.async_wait(sigfd_handler);
	ircd::init(ios);
	ios.run();  // Blocks until a clean exit or an exception comes out of it.
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

	path::chdir(ircd::ConfigFileEntry.dpath);
	return true;
}
catch(const std::exception &e)
{
	fprintf(stderr, usererrstr, e.what());
	return false;
}

void
sigfd_handler(const boost::system::error_code &ec,
              int signum)
{
	switch(ec.value())
	{
		using namespace boost::system::errc;

		case success:             break;
		case operation_canceled:  return;
		default:                  throw std::runtime_error(ec.message());
	}

	switch(signum)
	{
		case SIGINT:
			handle_interruption();
			break;

		case SIGQUIT:
			printf("\nQUIT\n");
			ios.stop();
			break;

		default:
			break;
	}

	sigs.async_wait(sigfd_handler);
}

const char *const interruption_message
{R"(
*** Charybdis is now paused. A command line has been presented below.
*** This is actually an IRC client and your commands will originate from
*** the server itself. The server will resume when you hit enter.
***
*** - To exit cleanly, type DIE
*** - To exit quickly, type ctrl-\ [SIGQUIT]
*** - To exit immediately, type EXIT
*** - To generate a coredump for developers, type ABORT
***
*** This message will be skipped on the next interrupt.
)"};

void
handle_interruption()
{
	static bool seen_the_message;
	if(!seen_the_message)
	{
		seen_the_message = true;
		std::cout << interruption_message;
	}

	std::string line;
	std::cout << "\n> " << std::flush;
	std::getline(std::cin, line);
	if(std::cin.eof())
	{
		std::cout << std::endl;
		std::cin.clear();
		return;
	}

	if(line.empty())
		return;

	if(line == "ABORT")
		abort();

	if(line == "EXIT")
		exit(0);
}
