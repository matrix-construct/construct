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

using namespace ircd;

bool printversion;
lgetopt opts[] =
{
	{ "help", NULL, lgetopt::USAGE, "Print this text" },
	{ "version", &printversion, lgetopt::BOOL, "Print version and exit" },
	{ "configfile", &ConfigFileEntry.configfile, lgetopt::STRING, "File to use for ircd.conf" },
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

static void print_version();
static bool startup_checks();

int main(int argc, char *const *argv)
try
{
	umask(077);       // better safe than sorry --SRB

	ConfigFileEntry.dpath = path::get(path::PREFIX);
	ConfigFileEntry.configfile = path::get(path::IRCD_CONF); // Server configuration file
	ConfigFileEntry.connect_timeout = 30; // Default to 30

	parseargs(&argc, &argv, opts);
	if(!startup_checks())
		return 1;

	if(printversion)
	{
		print_version();
		return 0;
	}

	return ircd::run();
}
catch(const std::exception &e)
{
	if(ircd::debugmode)
		throw;

	fprintf(stderr, fatalerrstr, e.what());
	return 1;
}

void print_version()
{
	printf("VERSION :%s\n",
	       info::version.c_str());

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
		throw error("Don't run ircd as root!!!");
	#endif

	path::chdir(ConfigFileEntry.dpath);
	return true;
}
catch(const std::exception &e)
{
	fprintf(stderr, usererrstr, e.what());
	return false;
}
