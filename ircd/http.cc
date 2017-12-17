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

namespace ircd::http
{
	namespace spirit = boost::spirit;
	namespace qi = spirit::qi;
	namespace karma = spirit::karma;
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
	using qi::attr_cast;

	using karma::maxwidth;

	template<class it, class top = unused_type> struct grammar;
	struct parser extern const parser;

	size_t printed_size(const vector_view<const line::header> &headers);
	size_t print(char *const &buf, const size_t &max, const vector_view<const line::header> &headers);

	extern const std::unordered_map<ircd::http::code, ircd::string_view> reason;
}

BOOST_FUSION_ADAPT_STRUCT
(
    ircd::http::line::request,
    ( decltype(ircd::http::line::request::method),    method   )
    ( decltype(ircd::http::line::request::path),      path     )
    ( decltype(ircd::http::line::request::query),     query    )
    ( decltype(ircd::http::line::request::fragment),  fragment )
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

BOOST_FUSION_ADAPT_STRUCT
(
    ircd::http::query,
    ( decltype(ircd::http::query::first),   first  )
    ( decltype(ircd::http::query::second),  second )
)

const decltype(ircd::http::reason) ircd::http::reason
{
	{ code::CONTINUE,                            "Continue"                                        },
	{ code::SWITCHING_PROTOCOLS,                 "Switching Protocols"                             },

	{ code::OK,                                  "OK"                                              },
	{ code::CREATED,                             "Created"                                         },
	{ code::ACCEPTED,                            "Accepted"                                        },
	{ code::NON_AUTHORITATIVE_INFORMATION,       "Non-Authoritative Information"                   },
	{ code::NO_CONTENT,                          "No Content"                                      },
	{ code::PARTIAL_CONTENT,                     "Partial Content"                                 },

	{ code::MULTIPLE_CHOICES,                    "Multiple Choices"                                },
	{ code::MOVED_PERMANENTLY,                   "Moved Permanently"                               },
	{ code::FOUND,                               "Found"                                           },
	{ code::SEE_OTHER,                           "See Other"                                       },
	{ code::NOT_MODIFIED,                        "Not Modified"                                    },
	{ code::TEMPORARY_REDIRECT,                  "Temporary Redirect"                              },
	{ code::PERMANENT_REDIRECT,                  "Permanent Redirect"                              },

	{ code::BAD_REQUEST,                         "Bad Request"                                     },
	{ code::UNAUTHORIZED,                        "Unauthorized"                                    },
	{ code::FORBIDDEN,                           "Forbidden"                                       },
	{ code::NOT_FOUND,                           "Not Found"                                       },
	{ code::METHOD_NOT_ALLOWED,                  "Method Not Allowed"                              },
	{ code::REQUEST_TIMEOUT,                     "Request Time-out"                                },
	{ code::CONFLICT,                            "Conflict"                                        },
	{ code::REQUEST_URI_TOO_LONG,                "Request URI Too Long"                            },
	{ code::EXPECTATION_FAILED,                  "Expectation Failed"                              },
	{ code::IM_A_TEAPOT,                         "Negative, I Am A Meat Popsicle"                  },
	{ code::UNPROCESSABLE_ENTITY,                "Unprocessable Entity"                            },
	{ code::TOO_MANY_REQUESTS,                   "Too Many Requests"                               },
	{ code::REQUEST_HEADER_FIELDS_TOO_LARGE,     "Request Header Fields Too Large"                 },

	{ code::INTERNAL_SERVER_ERROR,               "Internal Server Error"                           },
	{ code::NOT_IMPLEMENTED,                     "Not Implemented"                                 },
	{ code::SERVICE_UNAVAILABLE,                 "Service Unavailable"                             },
	{ code::HTTP_VERSION_NOT_SUPPORTED,          "HTTP Version Not Supported"                      },
	{ code::INSUFFICIENT_STORAGE,                "Insufficient Storage"                            },
};

template<class it,
         class top>
struct ircd::http::grammar
:qi::grammar<it, top>
{
	template<class R = unused_type, class... S> using rule = qi::rule<it, R, S...>;

	rule<> NUL                         { lit('\0')                                          ,"nul" };

	// insignificant whitespaces
	rule<> SP                          { lit('\x20')                                      ,"space" };
	rule<> HT                          { lit('\x09')                             ,"horizontal tab" };
	rule<> ws                          { SP | HT                                     ,"whitespace" };

	rule<> CR                          { lit('\x0D')                            ,"carriage return" };
	rule<> LF                          { lit('\x0A')                                  ,"line feed" };
	rule<> CRLF                        { CR >> LF                    ,"carriage return, line feed" };

	rule<> illegal                     { NUL | CR | LF                                  ,"illegal" };
	rule<> colon                       { lit(':')                                         ,"colon" };
	rule<> slash                       { lit('/')                               ,"forward solidus" };
	rule<> question                    { lit('?')                                 ,"question mark" };
	rule<> pound                       { lit('#')                                    ,"pound sign" };
	rule<> equal                       { lit('=')                                    ,"equal sign" };
	rule<> ampersand                   { lit('&')                                     ,"ampersand" };

	rule<string_view> token            { raw[+(char_ - (illegal | ws))]                   ,"token" };
	rule<string_view> string           { raw[+(char_ - illegal)]                         ,"string" };
	rule<string_view> line             { *ws >> -string >> CRLF                            ,"line" };

	rule<string_view> status           { raw[repeat(3)[char_("0-9")]]                    ,"status" };
	rule<short> status_code            { short_                                     ,"status code" };
	rule<string_view> reason           { string                                          ,"status" };

	rule<string_view> head_key         { raw[+(char_ - (illegal | ws | colon))]        ,"head key" };
	rule<string_view> head_val         { string                                      ,"head value" };
	rule<line::header> header          { head_key >> *ws >> colon >> *ws >> head_val     ,"header" };
	rule<unused_type> headers          { (header % (*ws >> CRLF))                       ,"headers" };

	rule<> query_terminator            { equal | question | ampersand | pound  ,"query terminator" };
	rule<> query_illegal               { illegal | ws | query_terminator          ,"query illegal" };
	rule<string_view> query_key        { raw[+(char_ - query_illegal)]                ,"query key" };
	rule<string_view> query_val        { raw[*(char_ - query_illegal)]              ,"query value" };

	rule<string_view> method           { token                                           ,"method" };
	rule<string_view> path             { raw[-slash >> *(char_ - query_illegal)]           ,"path" };
	rule<string_view> fragment         { pound >> -token                               ,"fragment" };
	rule<string_view> version          { token                                          ,"version" };

	rule<size_t> chunk_size
	{
		qi::uint_parser<size_t, 16, 1, 8>{} >> CRLF
		,"chunk size"
	};

	rule<http::query> query
	{
		query_key >> -(equal >> query_val)
		,"query"
	};

	rule<string_view> query_string
	{
		question >> -raw[(query_key >> -(equal >> query_val)) % ampersand]
		,"query string"
	};

	rule<line::request> request_line
	{
		method >> +SP >> path >> -query_string >> -fragment >> +SP >> version
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

	grammar(const rule<top> &top_rule)
	:grammar<it, top>::base_type
	{
		top_rule
	}
	{}
};

struct ircd::http::parser
:grammar<const char *, unused_type>
{
	static size_t content_length(const string_view &val);

	using http::grammar<const char *, unused_type>::grammar;
	parser(): grammar { grammar::ws } {}
}
const ircd::http::parser;

size_t
ircd::http::print(char *const &buf,
                  const size_t &max,
                  const vector_view<const line::header> &headers)
{
	size_t ret(0);
	for(auto it(std::begin(headers)); it != std::end(headers); ++it)
	{
		ret += fmt::snprintf(buf + ret, max - ret, "%s: %s",
		                     it->first,
		                     it->second);

		ret += snprintf(buf + ret, max - ret, "\r\n");
	}

	return ret;
}

size_t
ircd::http::printed_size(const vector_view<const line::header> &headers)
{
	return std::accumulate(std::begin(headers), std::end(headers), size_t(1), []
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
{
	const head h{pc, headers_closure};
	const char *const content_mark(pc.parsed);
	const unwind discard_unused_content{[&pc, &h, &content_mark]
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

ircd::http::request::request(const string_view &host,
                             const string_view &method,
                             const string_view &path,
                             const string_view &query,
                             const string_view &content,
                             const write_closure &closure,
                             const vector_view<const header> &headers)
{
	assert(!method.empty());
	assert(!path.empty());

	static const auto &version
	{
		"HTTP/1.1"
	};

	char request_line[2048]; const auto request_line_len
	{
		snprintf(request_line, sizeof(request_line), "%s /%s%s%s %s\r\n",
		         method.c_str(),
		         path.c_str(),
		         query.empty()? "" : "?",
		         query.empty()? "" : query.c_str(),
		         version)
	};

	char host_line[128] {"Host: "}; const auto host_line_len
	{
		6 + snprintf(host_line + 6, std::min(sizeof(host_line) - 6, host.size() + 3), "%s\r\n",
		             host.c_str())
	};

	char content_len[64]; const auto content_len_len
	{
		snprintf(content_len, sizeof(content_len), "Content-Length: %zu\r\n",
		         content.size())
	};

	char user_headers[printed_size(headers) + 2]; auto user_headers_len
	{
		print(user_headers, sizeof(user_headers), headers)
	};

	assert(user_headers_len + 3 == sizeof(user_headers));
	user_headers[user_headers_len++] = '\r';
	user_headers[user_headers_len++] = '\n';
	//user_headers[user_headers_len++] = '\0';

	const ilist<const_buffer> vector
	{
		{ request_line,      size_t(request_line_len)  },
		{ host_line,         size_t(host_line_len)     },
		{ content_len,       size_t(content_len_len)   },
		{ user_headers,      size_t(user_headers_len)  },
		{ content.data(),    content.size()            },
	};

	closure(vector);
}

ircd::http::request::head::head(parse::capstan &pc,
                                const headers::closure &c)
:line::request{pc}
,headers
{
	http::headers{pc, [this, &c](const auto &h)
	{
		if(iequals(h.first, "host"s))
			this->host = h.second;
		else if(iequals(h.first, "expect"s))
			this->expect = h.second;
		else if(iequals(h.first, "te"s))
			this->te = h.second;
		else if(iequals(h.first, "content-length"s))
			this->content_length = parser.content_length(h.second);
		else if(iequals(h.first, "authorization"s))
			this->authorization = h.second;
		else if(iequals(h.first, "connection"s))
			this->connection = h.second;

		if(c)
			c(h);
	}}
}
{
}

ircd::http::response::response(parse::capstan &pc,
                               content *const &c,
                               const proffer &proffer,
                               const headers::closure &headers_closure)
{
	const head h{pc, headers_closure};
	const char *const content_mark(pc.parsed);
	const unwind discard_unused_content{[&pc, &h, &content_mark]
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
                               const vector_view<const header> &headers)
{
	char status_line[128]; const auto status_line_len
	{
		snprintf(status_line, sizeof(status_line), "HTTP/1.1 %u %s\r\n",
		         uint(code),
		         status(code).data())
	};

	char server_line[128]; const auto server_line_len
	{
		code >= 200 && code < 300?
		snprintf(server_line, sizeof(server_line), "Server: %s (IRCd %s)\r\n",
		         BRANDING_NAME,
		         BRANDING_VERSION):
		0
	};

	char date_line[128], date_buf[96]; const auto date_line_len
	{
		code < 400?
		snprintf(data(date_line), size(date_line), "Date: %s\r\n",
		         timef(date_buf, ircd::localtime).c_str()):
		0
	};

	char cache_line[64]; const auto cache_line_len
	{
		//TODO: real cache control subsystem
		(code >= 200 && code < 300) || (code >= 403 && code <= 405) || (code >= 300 && code < 400)?
		snprintf(cache_line, sizeof(cache_line), "Cache-Control: %s\r\n",
		         "no-cache"):
		0
	};

	const bool has_transfer_encoding
	{
		std::any_of(std::begin(headers), std::end(headers), []
		(const auto &header)
		{
			return header == "Transfer-Encoding";
		})
	};

	char content_len[64]; const auto content_len_len
	{
		code != NO_CONTENT && !has_transfer_encoding?
		snprintf(content_len, sizeof(content_len), "Content-Length: %zu\r\n",
		         content.size()):
		0
	};

	const auto user_headers_bufsize
	{
		printed_size(headers)
	};

	char user_headers[user_headers_bufsize]; const auto user_headers_len
	{
		print(user_headers, sizeof(user_headers), headers)
	};

	const ilist<const_buffer> iov
	{
		{ status_line,      size_t(status_line_len)     },
		{ server_line,      size_t(server_line_len)     },
		{ date_line,        size_t(date_line_len)       },
		{ cache_line,       size_t(cache_line_len)      },
		{ user_headers,     size_t(user_headers_len)    },
		{ content_len,      size_t(content_len_len)     },
		{ "\r\n",           2                           },
		{ content.data(),   content.size()              },
	};

	closure(iov);
}

ircd::http::response::chunked::chunked(const code &code,
                                       const write_closure &closure,
                                       const vector_view<const header> &user_headers)
:closure{closure}
{
	const auto num_headers
	{
		user_headers.size() + 1
	};

	line::header headers[num_headers]
	{
		{ "Transfer-Encoding", "chunked" }
	};

	std::copy(begin(user_headers), end(user_headers), headers + 1);

	response
	{
		code, {}, closure, { headers, headers + num_headers }
	};
}

ircd::http::response::chunked::~chunked()
noexcept
{
	chunk(*this, null_buffer);
}

ircd::http::response::chunked::chunk::chunk(chunked &chunked,
                                            const const_buffer &buffer)
{
	char size_buf[16];
	const auto size_size
	{
		snprintf(size_buf, sizeof(size_buf), "%lx", size(buffer))
	};

	const ilist<const_buffer> iov
	{
		{ size_buf,     size_t(size_size) },
		{ "\r\n",       2                 },
		{ buffer                          },
		{ "\r\n",       2                 },
	};

	assert(bool(chunked.closure));
	chunked.closure(iov);
}

ircd::http::response::head::head(parse::capstan &pc,
                                 const headers::closure &c)
:line::response{pc}
,headers
{
	http::headers{pc, [this, &c](const auto &h)
	{
		if(iequals(h.first, "content-length"s))
			this->content_length = parser.content_length(h.second);

		else if(iequals(h.first, "transfer-encoding"s))
			this->transfer_encoding = h.second;

		if(c)
			c(h);
	}}
}
{
}

ircd::http::content::content(parse::capstan &pc,
                             chunked_t)
:string_view{[&pc]
{
	size_t size;
	pc([&size](const char *&start, const char *const &stop)
	{
		static const auto &grammar
		{
			parser.chunk_size
		};

		if(!qi::parse(start, stop, grammar, size))
		{
			size = 0;
			return false;
		}
		else return true;
	});

	if(size >= pc.remaining() - 2)
		throw parse::buffer_error("parse buffer must be for %zu bytes of chunk content + crlf",
		                          size + 2);

	const string_view ret
	{
		pc.read, pc.read + size
	};

	if(size)
	{
		pc.reader(pc.read, pc.read + size);
		pc.parsed = pc.read;
	}

	pc([](const char *&start, const char *const &stop)
	{
		static const auto &grammar
		{
			parser.CRLF
		};

		return qi::parse(start, stop, grammar);
	});

	return ret;
}()}
{
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

	//assert(pc.parsed == base + length);
	if(unlikely(pc.parsed < base + length))
		throw parse::buffer_error("parse buffer short by %zu to hold %zu total bytes of content",
		                          remain,
		                          length);

	if(pc.remaining())
		*pc.read = '\0';

	assert(pc.parsed == pc.read);
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
:string_view{[&pc, &c]
() -> string_view
{
	line::header h{pc};
	const char *const &started{h.first.data()}, *stopped{started};
	for(; !h.first.empty(); stopped = h.second.data() + h.second.size(), h = line::header{pc})
		if(c)
			c(h);

	return { started, stopped };
}()}
{
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
	const auto rule
	{
		ircd::string(e.what_)
	};

	throw error
	{
		code::BAD_REQUEST, fmt::snstringf
		{
			BUFSIZE,
			"I require a valid HTTP %s. You sent %zu invalid characters starting with `%s'.",
			between(rule, "<", ">"),
			size_t(e.last - e.first),
			string_view{e.first, e.last}
		}
	};
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
	const auto rule
	{
		ircd::string(e.what_)
	};

	throw error
	{
		code::BAD_REQUEST, fmt::snstringf
		{
			BUFSIZE,
			"I require a valid HTTP %s. You sent %zu invalid characters starting with `%s'.",
			between(rule, "<", ">"),
			size_t(e.last - e.first),
			string_view{e.first, e.last}
		}
	};
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

ircd::string_view
ircd::http::query::string::at(const string_view &key)
const
{
	const auto ret(operator[](key));
	if(ret.empty())
		throw std::out_of_range("Failed to find value for required query string key");

	return ret;
}

ircd::string_view
ircd::http::query::string::operator[](const string_view &key)
const
{
	string_view ret;
	const auto match([&key, &ret](const query &query) -> bool
	{
		if(query.first == key)
		{
			ret = query.second;
			return false;         // false to break out of until()
		}
		else return true;
	});

	until(match);
	return ret;
}

bool
ircd::http::query::string::until(const std::function<bool (const query &)> &closure)
const
{
	const auto action([&closure](const auto &attribute, const auto &context, auto &halt)
	{
		halt = closure(attribute);
	});

	const parser::rule<unused_type> grammar
	{
		-parser.question >> (parser.query[action] % parser.ampersand)
	};

	const string_view &s(*this);
	const char *start(s.data()), *const stop(s.data() + s.size());
	return qi::parse(start, stop, grammar);
}

void
ircd::http::query::string::for_each(const std::function<void (const query &)> &closure)
const
{
	const auto action([&closure](const auto &attribute, const auto &context, auto &halt)
	{
		closure(attribute);
	});

	const parser::rule<unused_type> grammar
	{
		-parser.question >> (parser.query[action] % parser.ampersand)
	};

	const string_view &s(*this);
	const char *start(s.data()), *const stop(s.data() + s.size());
	qi::parse(start, stop, grammar);
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

ircd::http::error::error(const enum code &code,
                         std::string content)
:ircd::error{generate_skip}
,code{code}
,content{std::move(content)}
{
	snprintf(buf, sizeof(buf), "%d %s", int(code), status(code).data());
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

ircd::string_view
ircd::http::status(const enum code &code)
{
	return reason.at(code);
}
