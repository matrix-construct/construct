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
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <ircd/spirit.h>

namespace ircd {
namespace http {

namespace spirit = boost::spirit;
namespace qi = spirit::qi;
namespace ascii = qi::ascii;

using spirit::unused_type;

using qi::lit;
using qi::string;
using qi::char_;
using qi::short_;
using qi::int_;
using qi::long_;
using qi::repeat;
using qi::omit;
using qi::raw;
using qi::attr;
using qi::eps;

std::map<code, std::string> reason
{
	{ code::CONTINUE,                            "Continue"s                                       },
	{ code::SWITCHING_PROTOCOLS,                 "Switching Protocols"s                            },

	{ code::OK,                                  "OK"s                                             },
	{ code::CREATED,                             "Created"s                                        },
	{ code::ACCEPTED,                            "Accepted"s                                       },
	{ code::NON_AUTHORITATIVE_INFORMATION,       "Non-Authoritative Information"s                  },
	{ code::NO_CONTENT,                          "No Content"s                                     },

	{ code::BAD_REQUEST,                         "Bad Request"s                                    },
	{ code::UNAUTHORIZED,                        "Unauthorized"s                                   },
	{ code::FORBIDDEN,                           "Forbidden"s                                      },
	{ code::NOT_FOUND,                           "Not Found"s                                      },
	{ code::METHOD_NOT_ALLOWED,                  "Method Not Allowed"s                             },
	{ code::REQUEST_TIMEOUT,                     "Request Time-out"s                               },
	{ code::EXPECTATION_FAILED,                  "Expectation Failed"s                             },
	{ code::TOO_MANY_REQUESTS,                   "Too Many Requests"s                              },
	{ code::REQUEST_HEADER_FIELDS_TOO_LARGE,     "Request Header Fields Too Large"s                },

	{ code::INTERNAL_SERVER_ERROR,               "Internal Server Error"s                          },
	{ code::NOT_IMPLEMENTED,                     "Not Implemented"s                                },
	{ code::SERVICE_UNAVAILABLE,                 "Service Unavailable"s                            },
	{ code::HTTP_VERSION_NOT_SUPPORTED,          "HTTP Version Not Supported"s                     },
};

} // namespace http
} // namespace ircd

BOOST_FUSION_ADAPT_STRUCT
(
    ircd::http::line::request,
    ( decltype(ircd::http::line::request::method),    method   )
    ( decltype(ircd::http::line::request::resource),  resource )
    ( decltype(ircd::http::line::request::version),   version  )
)

BOOST_FUSION_ADAPT_STRUCT
(
    ircd::http::line::response,
    ( decltype(ircd::http::line::response::version),  version )
    ( decltype(ircd::http::line::response::status),   status  )
    ( decltype(ircd::http::line::response::reason),   reason  )
)

BOOST_FUSION_ADAPT_STRUCT
(
    ircd::http::line::header,
    ( decltype(ircd::http::line::header::first),   first  )
    ( decltype(ircd::http::line::header::second),  second )
)

namespace ircd {
namespace http {

template<class it,
         class top = unused_type>
struct grammar
:qi::grammar<it, top>
,parse::grammar
{
	template<class R = unused_type> using rule = qi::rule<it, R>;

	rule<> NUL                         { lit('\0')                                          ,"nul" };

	// insignificant whitespaces
	rule<> SP                          { lit('\x20')                                      ,"space" };
	rule<> HT                          { lit('\x09')                             ,"horizontal tab" };
	rule<> CR                          { lit('\x0D')                            ,"carriage return" };
	rule<> LF                          { lit('\x0A')                                  ,"line feed" };

	// whitespace skipping
	rule<> ws                          { SP | HT                                     ,"whitespace" };

	rule<> CRLF                        { lit('\r') >> lit('\n')      ,"carriage return, line feed" };
	rule<> colon                       { lit(':')                                         ,"colon" };

	rule<string_view> token            { raw[+(char_ - (NUL | CR | LF | ws))]             ,"token" };
	rule<string_view> string           { raw[+(char_ - (NUL | CR | LF))]                 ,"string" };
	rule<string_view> line             { *ws >> -string >> CRLF                            ,"line" };

	rule<string_view> method           { raw[+(char_ - (NUL | CR | LF | ws))]            ,"method" };
	rule<string_view> resource         { raw[+(char_ - (NUL | CR | LF | ws))]          ,"resource" };
	rule<string_view> version          { raw[+(char_ - (NUL | CR | LF | ws))]           ,"version" };

	rule<string_view> status           { raw[repeat(3)[char_("0-9")]]                    ,"status" };
	rule<short> status_code            { short_                                     ,"status code" };
	rule<string_view> reason           { raw[+(char_ - (NUL | CR | LF))]                 ,"status" };

	rule<string_view> key              { raw[+(char_ - (NUL | CR | LF | ws | colon))]       ,"key" };
	rule<string_view> value            { string                                           ,"value" };
	rule<line::header> header          { key >> *ws >> colon >> *ws >> value             ,"header" };
	rule<unused_type> headers          { (header % (*ws >> CRLF))                       ,"headers" };

	rule<line::request> request_line
	{
		method >> +SP >> resource >> +SP >> version
		,"request line"
	};

	rule<line::response> response_line
	{
		version >> +SP >> status >> -(+SP >> reason)
		,"response line"
	};

	rule<unused_type> request
	{
		request_line >> *ws >> CRLF >> -headers >> CRLF
		,"request"
	};

	rule<unused_type> response
	{
		response_line >> *ws >> CRLF >> -headers >> CRLF
		,"response"
	};

	grammar(rule<top> &top_rule, const char *const &name)
	:grammar<it, top>::base_type
	{
		top_rule
	}
	,parse::grammar
	{
		name
	}
	{}
};

struct parser
:grammar<const char *, unused_type>
{
	static size_t content_length(const string_view &val);

	parser(): grammar { grammar::ws, "http.request" } {}
}
const parser;

size_t printed_size(const std::initializer_list<line::header> &headers);
size_t print(char *const &buf, const size_t &max, const std::initializer_list<line::header> &headers);

} // namespace http
} // namespace ircd

size_t
ircd::http::print(char *const &buf,
                  const size_t &max,
                  const std::initializer_list<line::header> &headers)
{
	size_t ret(0);
	for(const auto &header : headers)
		ret += snprintf(buf + ret, max - ret, "%s: %s\r\n",
		                header.first.data(),
		                header.second.data());
	return ret;
}

size_t
ircd::http::printed_size(const std::initializer_list<line::header> &headers)
{
	return std::accumulate(begin(headers), end(headers), size_t(0), []
	(auto &ret, const auto &pair)
	{
		//            key                 :   SP  value                CRLF
		return ret += pair.first.size() + 1 + 1 + pair.second.size() + 2;
	});
}

ircd::http::request::request(parse::capstan &pc,
                             content *const &c,
                             const write_closure &write_closure,
                             const proffer &proffer,
                             const headers::closure &headers_closure)
try
{
	const head h{pc, headers_closure};
	const char *const content_mark(pc.parsed);
	const scope discard_unused_content{[&pc, &h, &content_mark]
	{
		const size_t consumed(pc.parsed - content_mark);
		const size_t remain(h.content_length - consumed);
		http::content{pc, remain, content::discard};
	}};

	if(proffer)
		proffer(h);

	if(c)
		*c = content{pc, h};
}
catch(const http::error &e)
{
	if(write_closure)
		http::response{e.code, e.content, write_closure};

	throw;
}
catch(const std::exception &e)
{
	if(write_closure)
		http::response{http::INTERNAL_SERVER_ERROR, e.what(), write_closure};

	throw;
}

ircd::http::request::request(const string_view &host,
                             const string_view &method,
                             const string_view &resource,
                             const string_view &content,
                             const write_closure &closure,
                             const std::initializer_list<line::header> &headers)
{
	assert(!method.empty());
	assert(!resource.empty());

	char host_line[128] {"Host: "}; const auto host_line_len
	{
		6 + snprintf(host_line + 6, std::min(sizeof(host_line) - 6, host.size() + 3), "%s\r\n",
		             host.data())
	};

	char content_len[64]; const auto content_len_len
	{
		snprintf(content_len, sizeof(content_len), "Content-Length: %zu\r\n",
		         content.size())
	};

	char user_headers[printed_size(headers) + 1]; const auto user_headers_len
	{
		print(user_headers, sizeof(user_headers), headers)
	};

	const auto &space       {" "s          };
	const auto &version     { "HTTP/1.1"s  };
	const auto &terminator  { "\r\n"s      };
	const_buffers vector
	{
		{ method.data(),      method.size()             },
		{ space.data(),       space.size(),             },
		{ resource.data(),    resource.size(),          },
		{ space.data(),       space.size(),             },
		{ version.data(),     version.size(),           },
		{ terminator.data(),  terminator.size()         },
		{ host_line,          size_t(host_line_len)     },
		{ content_len,        size_t(content_len_len)   },
		{ user_headers,       size_t(user_headers_len)  },
		{ terminator.data(),  terminator.size()         },
		{ content.data(),     content.size()            },
	};

	closure(vector);
}

ircd::http::request::head::head(parse::capstan &pc,
                                const headers::closure &c)
:line::request{pc}
{
	headers{pc, [this, &c](const auto &h)
	{
		if(iequals(h.first, "host"s))
			host = h.second;
		else if(iequals(h.first, "expect"s))
			expect = h.second;
		else if(iequals(h.first, "te"s))
			te = h.second;
		else if(iequals(h.first, "content-length"s))
			content_length = parser.content_length(h.second);

		if(c)
			c(h);
	}};
}

ircd::http::response::response(parse::capstan &pc,
                               content *const &c,
                               const proffer &proffer,
                               const headers::closure &headers_closure)
{
	const head h{pc, headers_closure};
	const char *const content_mark(pc.parsed);
	const scope discard_unused_content{[&pc, &h, &content_mark]
	{
		const size_t consumed(pc.parsed - content_mark);
		const size_t remain(h.content_length - consumed);
		http::content{pc, remain, content::discard};
	}};

	if(proffer)
		proffer(h);

	if(c)
		*c = content{pc, h};
}

ircd::http::response::response(const code &code,
                               const string_view &content,
                               const write_closure &closure,
                               const std::initializer_list<line::header> &headers)
{
	char status_line[64]; const auto status_line_len
	{
		snprintf(status_line, sizeof(status_line), "HTTP/1.1 %u %s\r\n",
		         uint(code),
		         http::reason[code].data())
	};

	char server_line[128]; const auto server_line_len
	{
		code >= 200 && code < 300?
		snprintf(server_line, sizeof(server_line), "Server: %s (IRCd) %s\r\n",
		         BRANDING_NAME,
		         BRANDING_VERSION):
		0
	};

	const time_t ltime(time(nullptr));
	struct tm *const tm(localtime(&ltime));
	char date_line[64]; const auto date_line_len
	{
		code < 400 || code >= 500?
		strftime(date_line, sizeof(date_line), "Date: %a, %d %b %Y %T %z\r\n", tm):
		0
	};

	char content_len[64]; const auto content_len_len
	{
		code != NO_CONTENT?
		snprintf(content_len, sizeof(content_len), "Content-Length: %zu\r\n",
		         content.size()):
		0
	};

	const auto user_headers_bufsize
	{
		std::accumulate(begin(headers), end(headers), size_t(1), []
		(auto &ret, const auto &pair)
		{
			return ret += pair.first.size() + 1 + 1 + pair.second.size() + 2;
		})
	};

	char user_headers[user_headers_bufsize];
	const auto user_headers_len(print(user_headers, sizeof(user_headers), headers));

	const_buffers iov
	{
		{ status_line,      size_t(status_line_len)     },
		{ server_line,      size_t(server_line_len)     },
		{ date_line,        size_t(date_line_len)       },
		{ content_len,      size_t(content_len_len)     },
		{ user_headers,     size_t(user_headers_len)    },
		{ "\r\n",           2                           },
		{ content.data(),   content.size()              },
	};

	closure(iov);
}

ircd::http::response::head::head(parse::capstan &pc,
                                 const headers::closure &c)
:line::response{pc}
{
	headers{pc, [this, &c](const auto &h)
	{
		if(iequals(h.first, "content-length"s))
			content_length = parser.content_length(h.second);

		if(c)
			c(h);
	}};
}

ircd::http::content::content(parse::capstan &pc,
                             const size_t &length)
:string_view{[&pc, &length]
{
	const char *const base(pc.parsed);
	const size_t have(std::min(pc.unparsed(), length));
	size_t remain(length - have);
	pc.parsed += have;

	while(remain && pc.remaining())
	{
		const auto read_max(std::min(remain, pc.remaining()));
		pc.reader(pc.read, pc.read + read_max);
		remain -= pc.unparsed();
		pc.parsed = pc.read;
	}

	assert(pc.parsed == base + length);
	assert(pc.parsed == pc.read);

	if(pc.remaining())
		*pc.read = '\0';

	return string_view { base, pc.parsed };
}()}
{
}

ircd::http::content::content(parse::capstan &pc,
                             const size_t &length,
                             discard_t)
:string_view{}
{
	static char buf[512] alignas(16);

	const size_t have(std::min(pc.unparsed(), length));
	size_t remain(length - have);
	pc.read -= have;

	while(remain)
	{
		char *start(buf);
		__builtin_prefetch(start, 1, 0);    // 1 = write, 0 = no cache
		pc.reader(start, start + std::min(remain, sizeof(buf)));
		remain -= std::distance(buf, start);
	}
}

ircd::http::headers::headers(parse::capstan &pc,
                             const closure &c)
{
	for(line::header h{pc}; !h.first.empty(); h = line::header{pc})
		if(c)
			c(h);
}

ircd::http::line::header::header(const line &line)
try
{
	static const auto grammar
	{
		eps > parser.header
	};

	if(line.empty())
		return;

	const char *start(line.data());
	const char *const stop(line.data() + line.size());
	qi::parse(start, stop, grammar, *this);
}
catch(const qi::expectation_failure<const char *> &e)
{
	char buf[256];
	const auto rule(ircd::string(e.what_));
	fmt::snprintf(buf, sizeof(buf),
	              "I require a valid HTTP %s. You sent %zu invalid characters starting with `%s'.",
	              between(rule, "<", ">"),
	              ssize_t(e.last - e.first),
	              string_view{e.first, e.last});

	throw error(code::BAD_REQUEST, buf);
}

ircd::http::line::response::response(const line &line)
{
	static const auto grammar
	{
		eps > parser.response_line
	};

	const char *start(line.data());
	const char *const stop(line.data() + line.size());
	qi::parse(start, stop, grammar, *this);
}

ircd::http::line::request::request(const line &line)
try
{
	static const auto grammar
	{
		eps > parser.request_line
	};

	const char *start(line.data());
	const char *const stop(line.data() + line.size());
	qi::parse(start, stop, grammar, *this);
}
catch(const qi::expectation_failure<const char *> &e)
{
	char buf[256];
	const auto rule(ircd::string(e.what_));
	fmt::snprintf(buf, sizeof(buf),
	              "I require a valid HTTP %s. You sent %zu invalid characters starting with `%s'.",
	              between(rule, "<", ">"),
	              ssize_t(e.last - e.first),
	              string_view{e.first, e.last});

	throw error(code::BAD_REQUEST, buf);
}

ircd::http::line::line(parse::capstan &pc)
:string_view{[&pc]
{
	static const auto grammar
	{
		parser.line
	};

	string_view ret;
	pc([&ret](const char *&start, const char *const &stop)
	{
		if(!qi::parse(start, stop, grammar, ret))
		{
			ret = {};
			return false;
		}
		else return true;
	});

	return ret;
}()}
{
}

ircd::http::error::error(const enum code &code,
                         std::string content)
:ircd::error{generate_skip}
,code{code}
,content{std::move(content)}
{
	snprintf(buf, sizeof(buf), "%d %s", int(code), reason[code].data());
}

ircd::http::code
ircd::http::status(const string_view &str)
{
	static const auto grammar
	{
		parser.status_code
	};

	short ret;
	const char *start(str.data());
	const bool parsed(qi::parse(start, start + str.size(), grammar, ret));
	if(!parsed || ret < 0 || ret >= 1000)
		throw ircd::error("Invalid HTTP status code");

	return http::code(ret);
}

size_t
ircd::http::parser::content_length(const string_view &str)
{
	static const parser::rule<long> grammar
	{
		long_
	};

	long ret;
	const char *start(str.data());
	const bool parsed(qi::parse(start, start + str.size(), grammar, ret));
	if(!parsed || ret < 0)
		throw error(BAD_REQUEST, "Invalid content-length value");

	return ret;
}
