/*
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
 * SERVICES; LOSS OF USE, DATA, OR oPROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <ircd/ircd.h>
#include <boost/asio.hpp>
#include <ircd/ctx/continuation.h>
#include "charybdis.h"

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

void
console_spawn()
{
	if(console_active)
		return;

	// The console function is executed asynchronously.
	// The DETACH indicates it will clean itself up.
	ircd::context(std::bind(&console), ircd::context::DETACH);
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
console_hangup()
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
	ircd::log::error("console_hangup(): %s", e.what());
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

ircd::m::client *moi;

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
	using namespace ircd;

	const scope atexit([]
	{
		console_active = false;
		console_in = nullptr;
		delete moi; moi = nullptr;
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
		// so the output of the command (if log messages) can be seen.
		{
			const log::console_quiet quiet(false);
			boost::asio::async_read_until(in, buf, '\n', yield(continuation()));
		}

		std::getline(is, line);
		if(!line.empty())
			handle_line(line);
	}
	catch(const ircd::resource::not_found &e)
	{
		std::cerr << e.what() << std::endl;
	}
}
catch(const std::exception &e)
{
	std::cout << std::endl;
	std::cout << "***\n";
	std::cout << "*** The console session has ended: " << e.what() << "\n";
	std::cout << "***" << std::endl;
	ircd::log::debug("The console session has ended: %s", e.what());
	return;
}

void
handle_line(const std::string &line)
try
{
	using namespace ircd;

	if(line == "ABORT")
		abort();

	if(line == "EXIT")
		exit(0);

	switch(hash(token(line, " ", 0)))
	{
		case hash("GET"):
		case hash("POST"):
		{
			if(!moi)
			{
				std::cerr << "No current session" << std::endl;
				break;
			}

			const auto raw(line);
			m::request::quote q
			{
				"GET", "/foo", "{}"
			};
			moi->quote(q);
			break;
		}

		case hash("connect"):
		{
			if(moi)
			{
				std::cerr << "Already have session." << std::endl;
				break;
			}

			const auto args(tokens_after(line, " ", 0));
			const params token{args, " ", {"host", "port"}};
			const std::string host{token.at(0)};
			const auto port{token.at<uint16_t>(1)};
			moi = new m::client{{host, port}};
			break;
		}

		case hash("disconnect"):
		{
			if(!moi)
			{
				std::cerr << "No current session" << std::endl;
				break;
			}

			delete moi; moi = nullptr;
			break;
		}

		case hash("versions"):
		{
			if(!moi)
			{
				std::cerr << "No current session" << std::endl;
				break;
			}

			m::request::versions
			{
				*moi, [](const json::doc &doc)
				{
					std::cout << doc << std::endl;
				}
			};

			break;
		}

		case hash("register"):
		{
			if(!moi)
			{
				std::cerr << "No current session" << std::endl;
				//break;
			}

			const auto args(tokens_after(line, " ", 0));
			const params token{args, " ", {"user", "pass"}};
			moi->reg(token.at(0), token.at(1));
			break;
		}

		case hash("login"):
		{
			if(!moi)
			{
				std::cerr << "No current session" << std::endl;
				break;
			}

			const auto args(tokens_after(line, " ", 0));
			const params token{args, " ", {"user", "password"}};
			m::request::login login
			{{
				{ "user",      token.at(0)        },
				{ "password",  token.at(1)        },
				{ "type",      "m.login.password" }
			}};

			const auto session
			{
				moi->login(login)
			};

			std::cout << "access_token: " << session.access_token << std::endl;

			break;
		}

		case hash("sync"):
		{
			if(!moi)
			{
				std::cerr << "No current session" << std::endl;
				break;
			}

			m::request::sync sync;
			moi->sync(sync);
			break;
		}

		default:
			std::cerr << "Bad command or filename" << std::endl;
	}
}
catch(const std::out_of_range &e)
{
	std::cerr << "missing required arguments." << std::endl;
	return;
}
catch(const ircd::http::error &e)
{
	ircd::log::error("console: %s %s", e.what(), e.content.c_str());
	return;
}
catch(const std::exception &e)
{
	ircd::log::error("console: %s", e.what());
	return;
}
