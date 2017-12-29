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
#include <ircd/asio.h>
#include <ircd/m/m.h>
#include "charybdis.h"

using namespace ircd;

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

static bool handle_line(const std::string &line);
static void execute(const std::vector<std::string> lines);
static void console();

static
void check_console_active()
{
	if(console_active)
		throw ircd::error("Console is already active and cannot be reentered");
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

ircd::m::session *moi;

const char *const console_message
{R"(
***
*** The server is still running in the background. A command line is now available below.
*** This is a client and your commands will originate from the server itself.
***)"};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wframe-larger-than="
void
console()
try
{
	if(ircd::runlevel != ircd::runlevel::RUN)
		return;

	const unwind atexit([]
	{
		console_active = false;
		console_in = nullptr;
		delete moi; moi = nullptr;
		std::cin.clear();
	});

	console_active = true;
	console_ctx = &ctx::cur();

	std::cout << console_message << generic_message;

	boost::asio::posix::stream_descriptor in{*::ios, dup(STDIN_FILENO)};
	console_in = &in;

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
			boost::asio::async_read_until(in, buf, '\n', yield_context{to_asio{}});
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
	std::cout << std::endl;
	std::cout << "***" << std::endl;
	std::cout << "*** The console session has ended: " << e.what() << std::endl;
	std::cout << "***" << std::endl;

	ircd::log::debug("The console session has ended: %s", e.what());
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
		delete moi; moi = nullptr;
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
handle_line(const std::string &line)
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

	switch(hash(token(line, " ", 0)))
	{
		case hash("reload"):
		{
			module matrix("matrix");
			auto &root(matrix.get<module *>("root_module"));
			root->reset();
			*root = module("root");
			break;
		}

		case hash("show"):
		{
			const auto args(tokens_after(line, " ", 0));
			const params token{args, " ", {"what"}};
			const std::string what(token.at(0));
			switch(hash(what))
			{
				case hash("dbs"):
				{
					const auto dirs(ircd::db::available());
					for(const auto &dir : dirs)
						std::cout << dir << ", ";

					std::cout << std::endl;
					break;
				}

				case hash("front"):
				{
					for(const auto &front : ircd::m::vm::fronts.map)
						for(const auto &map : front.second.map)
							std::cout << std::setw(48) << std::right << front.first << " " << map.second << " " << map.first << std::endl;

					break;
				}

				case hash("conf"):
				{
					const json::object &conf{ircd::conf};
					for(const auto &kv : conf)
						std::cout << std::setw(32) << std::right << kv.first << "  " << kv.second << std::endl;

					break;
				}
			}

			if(startswith(what, '$'))
			{
				static char buf[65536];
				const m::event event
				{
					m::event::id{what}, buf
				};

				std::cout << pretty(event) << std::endl;
			}

			break;
		}

		case hash("resolve"):
		{
			const auto args(tokens_after(line, " ", 0));
			const params token{args, " ", {"what", "host", "port"}};
			const auto &what(token.at(0));
			const auto &host{token.at(1)};
			const auto &port{token.at(2)};
			const net::hostport hostport
			{
				host, port
			};

			switch(hash(what))
			{
				case hash("ips"):
				{
					ctx::future<std::vector<net::ipport>> future;
					net::resolve{hostport, future};
					const auto ips{future.get()};
					for(const auto &ip : ips)
						std::cout << ip << std::endl;

					break;
				}

				case hash("ip"):
				{
					ctx::future<net::ipport> future;
					net::resolve{hostport, future};
					std::cout << future.get() << std::endl;
					break;
				}
			}

			break;
		}

		case hash("reconnect"):
		{
			handle_line("disconnect");
			handle_line("connect");
			break;
		}

		case hash("messages"):
		{
			if(!moi)
			{
				std::cerr << "No current session" << std::endl;
				break;
			}

			const auto args(tokens_after(line, " ", 0));
			const params token{args, " ", {"room_id"}};
			const string_view &room_id
			{
				token.at(0, ircd::m::my_room.room_id)
			};

			char room_id_encoded[512];
			char url[512]; const auto url_len
			{
				fmt::sprintf(url, "_matrix/client/r0/rooms/%s/messages",
				             url::encode(room_id, room_id_encoded))
			};

			char query[1024];
			snprintf(query, sizeof(query), "%s=%s",
			         "access_token",
			         moi->access_token.c_str());

			m::request request
			{
				"GET", string_view{url, size_t(url_len)}, query, {}
			};

			static char buf[65536];
			ircd::parse::buffer pb{buf};
			const json::object response{(*moi)(pb, request)};
			const json::array chunk(response["chunk"]);
			for(const string_view &chunk : chunk)
				std::cout << chunk << std::endl;

			break;
		}

		case hash("members"):
		{
			if(!moi)
			{
				std::cerr << "No current session" << std::endl;
				break;
			}

			const auto args(tokens_after(line, " ", 0));
			const params token{args, " ", {"room_id"}};
			const string_view &room_id
			{
				token.at(0, ircd::m::my_room.room_id)
			};

			char room_id_encoded[512];
			char url[512]; const auto url_len
			{
				fmt::sprintf(url, "_matrix/client/r0/rooms/%s/members",
				             url::encode(room_id, room_id_encoded))
			};

			char query[1024];
			snprintf(query, sizeof(query), "%s=%s",
			         "access_token",
			         moi->access_token.c_str());

			m::request request
			{
				"GET", string_view{url, size_t(url_len)}, query, {}
			};

			static char buf[65536];
			ircd::parse::buffer pb{buf};
			const json::object response{(*moi)(pb, request)};
			const json::array chunk(response["chunk"]);
			for(const string_view &chunk : chunk)
				std::cout << chunk << std::endl;

			break;
		}

		case hash("context"):
		{
			if(!moi)
			{
				std::cerr << "No current session" << std::endl;
				break;
			}

			const auto args(tokens_after(line, " ", 0));
			const params token{args, " ", {"room_id", "event_id"}};

			char room_id_buf[768], event_id_buf[768];
			const auto room_id{url::encode(token.at(0), room_id_buf)};
			const auto event_id{url::encode(token.at(1), event_id_buf)};

			char url[512]; const auto url_len
			{
				fmt::snprintf(url, sizeof(url), "_matrix/client/r0/rooms/%s/context/%s",
				              room_id,
				              event_id)
			};

			m::request request
			{
				"GET", string_view{url, size_t(url_len)}, {}
			};

			static char buf[65536];
			ircd::parse::buffer pb{buf};
			const json::object response{(*moi)(pb, request)};
			for(const auto &member : response)
				std::cout << member.first << " " << member.second << std::endl;

			break;
		}

		case hash("state"):
		{
			if(!moi)
			{
				std::cerr << "No current session" << std::endl;
				break;
			}

			const auto args(tokens_after(line, " ", 0));
			const params token{args, " ", {"room_id", "event_type", "state_key"}};

			char room_id_buf[512];
			const auto room_id{url::encode(token.at(0), room_id_buf)};

			const auto event_type{token[1]};

			char state_key_buf[512];
			const auto state_key
			{
				url::encode(token[2], state_key_buf)
			};

			char url[512]; const auto url_len
			{
				event_type && state_key?
					fmt::snprintf(url, sizeof(url), "_matrix/client/r0/rooms/%s/state/%s/%s",
					              room_id,
					              event_type,
					              state_key):

				event_type?
					fmt::snprintf(url, sizeof(url), "_matrix/client/r0/rooms/%s/state/%s",
					              room_id,
					              event_type):

					fmt::snprintf(url, sizeof(url), "_matrix/client/r0/rooms/%s/state",
					              room_id)
			};

			static char query[512]; const auto query_len
			{
				fmt::snprintf(query, sizeof(query), "%s=%s",
				              "access_token",
				              moi->access_token)
			};

			m::request request
			{
				"GET", string_view{url, size_t(url_len)}, string_view{query, size_t(query_len)}
			};

			static char buf[65536];
			ircd::parse::buffer pb{buf};
			const json::array response{(*moi)(pb, request)};
			for(const auto &event : response)
				std::cout << event << std::endl << std::endl;

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
			const std::string host{token.at(0, "127.0.0.1"s)};
			const auto port{token.at<uint16_t>(1, 8448)};
			moi = new m::session{{host, port}};
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

			m::request request
			{
				"GET", "_matrix/client/versions"
			};

			static char buf[1024];
			ircd::parse::buffer pb{buf};
			const auto doc((*moi)(pb, request));
			std::cout << doc << std::endl;
			break;
		}

		case hash("register"):
		{
			if(!moi)
			{
				std::cerr << "No current session" << std::endl;
				break;
			}

			const auto args(tokens_after(line, " ", 0));
			const params token{args, " ", {"username", "password"}};

			m::request request
			{
				"POST", "_matrix/client/r0/register?kind=user", {},
				{
					{ "username",  token.at(0) },
					{ "password",  token.at(1) },
					{
						"auth",
						{
							{ "type",  "m.login.dummy" }
						}
					}
				}
			};

			static char buf[4096];
			ircd::parse::buffer pb{buf};
			const string_view doc((*moi)(pb, request));
			std::cout << doc << std::endl;
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
			if(args.empty())
			{
				m::request request
				{
					"GET", "_matrix/client/r0/login", {}, {}
				};

				static char buf[4096];
				ircd::parse::buffer pb{buf};
				const json::object doc((*moi)(pb, request));
				const json::array flows(doc.at("flows"));

				size_t i(0);
				for(const auto &flow : flows)
					std::cout << i++ << ": " << flow << std::endl;

				break;
			}

			const params token
			{
				args, " ",
				{
					"username", "password"
				}
			};

			m::request request
			{
				"POST", "_matrix/client/r0/login", {},
				{
					{ "user",         token.at(0) },
					{ "password",     token.at(1) },
					{ "type",  "m.login.password" },
				}
			};

			static char buf[4096];
			ircd::parse::buffer pb{buf};
			const json::object doc((*moi)(pb, request));
			std::cout << doc << std::endl;
			moi->access_token = std::string(unquote(doc.at("access_token")));
			break;
		}

		case hash("sync"):
		{
			if(!moi)
			{
				std::cerr << "No current session" << std::endl;
				break;
			}

			const auto args
			{
				tokens_after(line, " ", 0)
			};

			const params token
			{
				args, " ",
				{
					"timeout", "filter_id", "full_state", "set_presence"
				}
			};

			const time_t timeout
			{
				token.at(0, 0)
			};

			static char query[2048];
			snprintf(query, sizeof(query), "%s=%s&timeout=%zd",
			         "access_token",
			         moi->access_token.c_str(),
			         timeout * 1000);

			while(1)
			{
				m::request request
				{
					"GET", "_matrix/client/r0/sync", query,
					{
					}
				};

				static char buf[8192];
				ircd::parse::buffer pb{buf};
				const json::object doc((*moi)(pb, request));
				const auto since(doc.at("next_batch"));
				for(const auto &member : doc)
					std::cout << string_view{member.first} << " => " << string_view{member.second} << std::endl;

				fmt::snprintf(query, sizeof(query), "%s=%s&since=%s&timeout=%zd",
				             "access_token",
				              moi->access_token,
				              since,
				              timeout * 1000);
			}
			break;
		}

		case hash("createroom"):
		{
			if(!moi)
			{
				std::cerr << "No current session" << std::endl;
				break;
			}

			const auto args(tokens_after(line, " ", 0));
			const params token{args, " ", {"name"}};

			char query[1024];
			snprintf(query, sizeof(query), "%s=%s",
			         "access_token",
			         moi->access_token.c_str());

			m::request request
			{
				"POST", "_matrix/client/r0/createRoom", query,
				{
					{ "name",      token.at(0) },
				}
			};

			static char buf[4096];
			ircd::parse::buffer pb{buf};
			const auto doc((*moi)(pb, request));
			std::cout << doc << std::endl;
			break;
		}
/*
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
*/
		case hash("privmsg"):
		{
			if(!moi)
			{
				std::cerr << "No current session" << std::endl;
				break;
			}

			static uint txnid;
			const auto args(tokens_after(line, " ", 0));
			const params token{args, " ", {"room_id", "msgtype"}};
			const auto &room_id{token.at(0)};
			const auto &msgtype{token.at(1)};
			const auto &event_type{"m.room.message"};
			const auto text(tokens_after(line, " ", 2));

			static char query[512]; const auto query_len
			{
				fmt::snprintf(query, sizeof(query), "%s=%s",
				              "access_token",
				              moi->access_token)
			};

			static char url[512]; const auto url_len
			{
				fmt::snprintf(url, sizeof(url), "_matrix/client/r0/rooms/%s/send/%s/%u",
				              room_id,
				              event_type,
				              txnid++)
			};

			m::request request
			{
				"PUT", url, query, json::members
				{
					{ "body", text },
					{ "msgtype", msgtype }
				}
			};

			static char buf[4096];
			ircd::parse::buffer pb{buf};
			const json::object response{(*moi)(pb, request)};
			std::cout << string_view{response} << std::endl;
			break;
		}

		case hash("password"):
		{
			if(!moi)
			{
				std::cerr << "No current session" << std::endl;
				break;
			}

			const auto args(tokens_after(line, " ", 0));
			const params token{args, " ", {"new_password"}};
			static char query[512]; const auto query_len
			{
				fmt::snprintf(query, sizeof(query), "%s=%s",
				              "access_token",
				              moi->access_token)
			};

			m::request request
			{
				"POST", "_matrix/client/r0/account/password", query, json::members
				{
					{ "new_password",  token.at(0) },
					{ "auth",          json::members
					{
						{ "type",     "m.login.password"  }
					}},
				}
			};

			static char buf[4096];
			ircd::parse::buffer pb{buf};
			const json::object response{(*moi)(pb, request)};
			std::cout << string_view{response} << std::endl;
			break;
		}

		case hash("deactivate"):
		{
			if(!moi)
			{
				std::cerr << "No current session" << std::endl;
				break;
			}

			const auto args(tokens_after(line, " ", 0));
			static char query[512]; const auto query_len
			{
				fmt::snprintf(query, sizeof(query), "%s=%s",
				              "access_token",
				              moi->access_token)
			};

			m::request request
			{
				"POST", "_matrix/client/r0/account/deactivate", query, json::members
				{
					{ "auth",          json::members
					{
						{ "type",     "m.login.password"  }
					}},
				}
			};

			static char buf[4096];
			ircd::parse::buffer pb{buf};
			const json::object response{(*moi)(pb, request)};
			std::cout << string_view{response} << std::endl;
			break;
		}

		case hash("setfilter"):
		{
			if(!moi)
			{
				std::cerr << "No current session" << std::endl;
				break;
			}

			const auto args
			{
				tokens_after(line, " ", 0)
			};

			const auto user_id
			{
				token(args, " ", 0)
			};

			const json::object filter
			{
				tokens_after(args, " ", 0)
			};

			static char url[128]; const auto url_len
			{
				fmt::snprintf(url, sizeof(url), "_matrix/client/r0/user/%s/filter", user_id)
			};

			static char query[512]; const auto query_len
			{
				fmt::snprintf(query, sizeof(query), "%s=%s",
				              "access_token",
				              moi->access_token)
			};

			m::request request
			{
				"POST", url, query, filter
			};

			static char buf[4096];
			ircd::parse::buffer pb{buf};
			const json::object response{(*moi)(pb, request)};
			std::cout << string_view{response} << std::endl;
			break;
		}

		case hash("getfilter"):
		{
			if(!moi)
			{
				std::cerr << "No current session" << std::endl;
				break;
			}

			const auto args
			{
				tokens_after(line, " ", 0)
			};

			const auto user_id
			{
				token(args, " ", 0)
			};

			const auto filter_id
			{
				tokens_after(args, " ", 0)
			};

			static char url[128]; const auto url_len
			{
				fmt::snprintf(url, sizeof(url), "_matrix/client/r0/user/%s/filter/%s",
				              user_id,
				              filter_id)
			};

			static char query[512]; const auto query_len
			{
				fmt::snprintf(query, sizeof(query), "%s=%s",
				              "access_token",
				              moi->access_token)
			};

			m::request request
			{
				"GET", url, query, {}
			};

			static char buf[4096];
			ircd::parse::buffer pb{buf};
			const json::object response{(*moi)(pb, request)};
			std::cout << string_view{response} << std::endl;
			break;
		}

		case hash("keys"):
		{
			const auto args
			{
				tokens_after(line, " ", 0)
			};

			const auto argc
			{
				token_count(args, " ")
			};

			size_t arg(0);
			const string_view server_name
			{
				token(args, " ", arg++)
			};

			const string_view key_id
			{
				argc >= ++arg? token(args, " ", arg - 1) : ""
			};

			const string_view query_server
			{
				argc >= ++arg? token(args, " ", arg - 1) : ""
			};

			if(key_id && query_server)
			{
				m::keys::get(server_name, key_id, query_server, []
				(const auto &keys)
				{
					std::cout << keys << std::endl;
				});
			}
			else if(key_id)
			{
				const string_view key_id{token(args, " ", 1)};
				m::keys::get(server_name, key_id, []
				(const auto &key)
				{
					std::cout << key << std::endl;
				});
			}
			else
			{
				m::keys::get(server_name, []
				(const auto &keys)
				{
					std::cout << keys << std::endl;
				});
			}
			break;
		}

		case hash("backfill"):
		{
			const auto args
			{
				tokens_after(line, ' ', 0)
			};

			const m::room room
			{
				m::room::id{token(args, ' ', 0)}
			};

			const m::event::id event_id
			{
				token(args, ' ', 1)
			};

			const auto limit
			{
				token_count(args, ' ') >= 2? lex_cast<uint>(token(args, ' ', 2)) : 0U
			};

			m::vm::backfill(room, event_id, limit);
			break;
		}

		case hash("statefill"):
		{
			const auto args
			{
				tokens_after(line, ' ', 0)
			};

			const m::room::id room_id
			{
				token(args, ' ', 0)
			};

			const m::event::id event_id
			{
				token(args, ' ', 1)
			};

			m::vm::statefill(room_id, event_id);
			break;
		}

		case hash("fedstate"):
		{
			const auto args
			{
				tokens_after(line, ' ', 0)
			};

			const m::room::id room_id
			{
				token(args, ' ', 0)
			};

			const m::event::id event_id
			{
				token(args, ' ', 1)
			};

			const unique_buffer<mutable_buffer> buf
			{
				64_MiB
			};

			const m::room::state state
			{
				room_id, event_id, buf
			};

			std::cout << pretty(state) << std::endl;

			for_each(state, [](const auto &key, const auto &val)
			{
				if(bool(json::get<"event_id"_>(val)))
					std::cout << pretty_oneline(val) << std::endl;
			});

			break;
		}

		case hash("pull"):
		{
			if(!moi)
			{
				std::cerr << "No current session" << std::endl;
				break;
			}

			const auto args
			{
				tokens_after(line, " ", 0)
			};

			const auto origin_token
			{
				token(args, " ", 0)
			};

			char origin_buf[512];
			const auto origin
			{
				url::encode(origin_token, origin_buf)
			};

			const auto event_id_token
			{
				token(args, " ", 0)
			};

			char event_id_buf[512];
			const auto event_id
			{
				url::encode(event_id_token, event_id_buf)
			};

			static char url[128]; const auto url_len
			{
				fmt::snprintf(url, sizeof(url), "_matrix/federation/v1/pull/")
			};

			static char query[512]; const auto query_len
			{
				fmt::snprintf(query, sizeof(query), "origin=%s&v=%s",
				              origin,
				              event_id)
			};

			m::request request
			{
				"GET", url, string_view{query, size_t(query_len)}, {}
			};

			static char buf[65536];
			ircd::parse::buffer pb{buf};
			const string_view response{(*moi)(pb, request)};
			std::cout << response << std::endl;
			break;
		}

		case hash("sjoin"):
		{
			const auto args
			{
				tokens_after(line, ' ', 0)
			};

			const m::room::id room_id
			{
				token(args, ' ', 0)
			};

			const m::user::id user_id
			{
				token(args, ' ', 1)
			};

			json::iov iov;
			json::iov::push push[]
			{
				{ iov, { "sender", user_id } }
			};

			m::vm::join(room_id, iov);
			break;
		}

		case hash("get"):
		{
			const auto args
			{
				tokens_after(line, ' ', 0)
			};

			const m::event::id event_id
			{
				token(args, ' ', 0)
			};

			static char buf[65536];
			const m::event event
			{
				event_id, buf
			};

			std::cout << event << std::endl;
			break;
		}

		case hash("fetch"):
		{
			const auto args
			{
				tokens_after(line, ' ', 0)
			};

			const m::event::id event_id
			{
				token(args, ' ', 0)
			};

			struct m::io::fetch::opts opts;
			opts.hint = net::remote
			{
				token(args, ' ', 1)
			};

			static char buf[65536];
			m::event::fetch fetch
			{
				event_id, buf, &opts
			};

			std::cout << m::io::acquire(fetch) << std::endl;
			break;
		}

		case hash("mfetch"):
		{
			const auto args
			{
				tokens_after(line, ' ', 0)
			};

			const auto event_ids
			{
				tokens_after(line, ' ', 0)
			};

//			const net::remote remote
//			{
//				token(args, ' ', 0)
//			};

			string_view event_id[8];
			const auto count
			{
				tokens(event_ids, ' ', event_id)
			};

			static char buf[8][65536];
			m::event::fetch tab[count];
			for(size_t i(0); i < count; ++i)
			{
				tab[i].event_id = event_id[i];
				tab[i].buf = buf[i];
//				tab[i].hint = remote;
			}

			m::io::acquire({tab, count});

			for(size_t i(0); i < count; ++i)
			{
				std::cout << tab[i].event_id << ": size: " << string_view{tab[i].pdu}.size() << std::endl;
			}

			break;
		}

		case hash("eval"):
		{
			const auto args{tokens_after(line, ' ', 0)};
			const params token{args, " ",
			{
				"event_id"
			}};

			m::event::id event_id
			{
				token.at(0)
			};

			if(m::vm::exists(event_id))
			{
				std::cout << "exists" << std::endl;
				break;
			}

			static char buf[65536];
			m::event::fetch fetch
			{
				event_id, buf
			};

			m::io::acquire(fetch);
			if(bool(fetch.error))
				std::rethrow_exception(fetch.error);

			assert(bool(fetch.pdu));
			const m::event event
			{
				fetch.pdu
			};

			m::vm::eval{event};
			std::cout << pretty_oneline(event) << std::endl;
			break;
		}

		case hash("exec"):
		{
			const auto args
			{
				tokens_after(line, ' ', 0)
			};

			const params token{args, " ",
			{
				"file path", "limit"
			}};

			const auto path
			{
				token.at(0)
			};

			const auto limit
			{
				token.at<size_t>(1)
			};

			const auto start
			{
				token[2]? lex_cast<size_t>(token[2]) : 0
			};

			//TODO: The file might be too large for a single read into ram for
			// a big synapse server. We will need an iteration of the file
			// instead.

			const std::string data
			{
				ircd::fs::read(std::string(path))
			};

			std::cout << "read " << data.size() << " bytes " << std::endl;

			const json::vector vector
			{
				data
			};

			struct ircd::m::vm::opts opts;
			ircd::m::vm::eval eval
			{
				opts
			};

			size_t j(0), s(0);
			static const auto max{1024};
			for(auto it(begin(vector)); it != end(vector) && j < limit;)
			{
				if(s++ < start)
				{
					++it;
					continue;
				}

				size_t i(0);
				std::array<m::event, max> a;
				for(; i < max && it != end(vector) && j < limit; ++it)
				{
					const json::object obj{*it};
					{
						a[i] = m::event{*it};
						++i, ++j;
					}
				}

				switch(eval(vector_view<const m::event>(a.data(), i)))
				{
					using fault = m::vm::fault;

					case fault::ACCEPT:
						continue;

					case fault::EVENT:
						std::cout << "EVENT FAULT " << eval.ef.size() << std::endl;
						//for(const auto &eid : eval.ef)
						//	std::cout << eid << std::endl;
						break;

					case fault::STATE:
						std::cout << "STATE FAULT " << std::endl;
						break;

					default:
						std::cout << "FAULT " << std::endl;
						break;
				}
			}

			break;
		}

		case hash("directory"):
		{
			if(!moi)
			{
				std::cerr << "No current session" << std::endl;
				break;
			}

			const auto args
			{
				tokens_after(line, " ", 0)
			};

			const auto query_type
			{
				"directory"
			};

			const auto room_alias_token
			{
				token(args, " ", 0)
			};

			char room_alias_buf[512];
			const auto room_alias
			{
				url::encode(room_alias_token, room_alias_buf)
			};

			static char url[128]; const auto url_len
			{
				fmt::snprintf(url, sizeof(url), "_matrix/federation/v1/query/%s",
				              query_type)
			};

			static char query[512]; const auto query_len
			{
				fmt::snprintf(query, sizeof(query), "%s=%s",
				              "room_alias",
				              room_alias)
			};

			m::request request
			{
				"GET", url, string_view{query, size_t(query_len)}, {}
			};

			static char buf[65536];
			ircd::parse::buffer pb{buf};
			const json::object response{(*moi)(pb, request)};
			std::cout << response << std::endl;
			break;
		}

		case hash("profile"):
		{
			if(!moi)
			{
				std::cerr << "No current session" << std::endl;
				break;
			}

			const auto args
			{
				tokens_after(line, " ", 0)
			};

			const auto query_type
			{
				"profile"
			};

			const auto user_id_token
			{
				token(args, " ", 0)
			};

			char user_id_buf[512];
			const auto user_id
			{
				url::encode(user_id_token, user_id_buf)
			};

			static char url[128]; const auto url_len
			{
				fmt::snprintf(url, sizeof(url), "_matrix/federation/v1/query/%s",
				              query_type)
			};

			static char query[512]; const auto query_len
			{
				fmt::snprintf(query, sizeof(query), "%s=%s",
				              "user_id",
				              user_id)
			};

			m::request request
			{
				"GET", url, string_view{query, size_t(query_len)}, {}
			};

			static char buf[65536];
			ircd::parse::buffer pb{buf};
			const json::object response{(*moi)(pb, request)};
			std::cout << response << std::endl;
			break;
		}

		default:
			std::cerr << "Bad command or filename" << std::endl;
	}

	return true;
}
catch(const std::out_of_range &e)
{
	std::cerr << "missing required arguments." << std::endl;
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
#pragma GCC diagnostic pop
