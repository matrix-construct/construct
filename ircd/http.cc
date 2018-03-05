// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

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

	extern const std::unordered_map<ircd::http::code, ircd::string_view> reason;

	[[noreturn]] void throw_error(const qi::expectation_failure<const char *> &);
}

BOOST_FUSION_ADAPT_STRUCT
(
    ircd::http::query,
    ( decltype(ircd::http::query::first),   first  )
    ( decltype(ircd::http::query::second),  second )
)

BOOST_FUSION_ADAPT_STRUCT
(
    ircd::http::header,
    ( decltype(ircd::http::header::first),   first  )
    ( decltype(ircd::http::header::second),  second )
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
    ircd::http::line::request,
    ( decltype(ircd::http::line::request::method),    method   )
    ( decltype(ircd::http::line::request::path),      path     )
    ( decltype(ircd::http::line::request::query),     query    )
    ( decltype(ircd::http::line::request::fragment),  fragment )
    ( decltype(ircd::http::line::request::version),   version  )
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
	{ code::PAYLOAD_TOO_LARGE,                   "Payload Too Large"                               },
	{ code::REQUEST_URI_TOO_LONG,                "Request URI Too Long"                            },
	{ code::EXPECTATION_FAILED,                  "Expectation Failed"                              },
	{ code::IM_A_TEAPOT,                         "Negative, I Am A Meat Popsicle"                  },
	{ code::UNPROCESSABLE_ENTITY,                "Unprocessable Entity"                            },
	{ code::TOO_MANY_REQUESTS,                   "Too Many Requests"                               },
	{ code::REQUEST_HEADER_FIELDS_TOO_LARGE,     "Request Header Fields Too Large"                 },

	{ code::INTERNAL_SERVER_ERROR,               "Internal Server Error"                           },
	{ code::NOT_IMPLEMENTED,                     "Not Implemented"                                 },
	{ code::BAD_GATEWAY,                         "Bad Gateway"                                     },
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
	rule<http::header> header          { head_key >> *ws >> colon >> *ws >> head_val     ,"header" };
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

/// Compose a request. This prints an HTTP head into the buffer. No real IO is
/// done here. After composing into the buffer, the user can then drive the
/// socket by sending the header and the content as specified.
///
/// If termination is false, no extra CRLF is printed to the buffer allowing
/// additional headers not specified to be appended later.
ircd::http::request::request(window_buffer &out,
                             const string_view &host,
                             const string_view &method,
                             const string_view &uri,
                             const size_t &content_length,
                             const string_view &content_type,
                             const vector_view<const header> &headers,
                             const bool &termination)
{
	writeline(out, [&method, &uri](const mutable_buffer &out) -> size_t
	{
		assert(!method.empty());
		assert(!uri.empty());
		return fmt::sprintf
		{
			out, "%s %s HTTP/1.1", method, uri
		};
	});

	writeline(out, [&host](const mutable_buffer &out) -> size_t
	{
		assert(!host.empty());
		return fmt::sprintf
		{
			out, "Host: %s", host
		};
	});

	if(content_length)
		writeline(out, [&content_type](const mutable_buffer &out) -> size_t
		{
			return fmt::sprintf
			{
				out, "Content-Type: %s", content_type?: "text/plain; charset=utf-8"
			};
		});

	writeline(out, [&content_length](const mutable_buffer &out) -> size_t
	{
		return fmt::sprintf
		{
			out, "Content-Length: %zu", content_length
		};
	});

	write(out, headers);

	if(termination)
		writeline(out);
}

ircd::http::request::head::head(parse::capstan &pc,
                                const headers::closure &c)
:line::request{pc}
,uri
{
	fragment? string_view { begin(path), end(fragment) }:
	query?    string_view { begin(path), end(query)    }:
	          string_view { begin(path), end(path)     }
}
,headers
{
	http::headers{pc, [this, &c](const auto &h)
	{
		if(iequals(h.first, "host"_sv))
			this->host = h.second;
		else if(iequals(h.first, "expect"_sv))
			this->expect = h.second;
		else if(iequals(h.first, "te"_sv))
			this->te = h.second;
		else if(iequals(h.first, "content-length"_sv))
			this->content_length = parser.content_length(h.second);
		else if(iequals(h.first, "authorization"_sv))
			this->authorization = h.second;
		else if(iequals(h.first, "connection"_sv))
			this->connection = h.second;
		else if(iequals(h.first, "content-type"_sv))
			this->content_type = h.second;
		else if(iequals(h.first, "user-agent"_sv))
			this->user_agent = h.second;

		if(c)
			c(h);
	}}
}
{
}

ircd::http::response::response(window_buffer &out,
                               const code &code,
                               const size_t &content_length,
                               const string_view &content_type,
                               const string_view &headers_string,
                               const vector_view<const header> &headers_vector,
                               const bool &termination)
{
	writeline(out, [&code](const mutable_buffer &out) -> size_t
	{
		return fmt::sprintf
		{
			out, "HTTP/1.1 %u %s", uint(code), status(code)
		};
	});

	if(code >= 200 && code < 300)
		writeline(out, [&code](const mutable_buffer &out) -> size_t
		{
			size_t ret{0};
			ret += copy(out, "Server: "_sv);
			ret += copy(out + ret, ircd::info::server_agent);
			return ret;
		});

	if(code < 400)
		writeline(out, [](const mutable_buffer &out) -> size_t
		{
			char date_buf[96];
			return fmt::sprintf
			{
				out, "Date: %s", timef(date_buf, ircd::localtime)
			};
		});

	if(code != NO_CONTENT && content_type && content_length)
		writeline(out, [&content_type](const mutable_buffer &out) -> size_t
		{
			return fmt::sprintf
			{
				out, "Content-Type: %s", content_type?: "text/plain; charset=utf-8"
			};
		});

	if(code != NO_CONTENT && content_length != std::numeric_limits<size_t>::max())
		writeline(out, [&content_length](const mutable_buffer &out) -> size_t
		{
			return fmt::sprintf
			{
				out, "Content-Length: %zu", content_length
			};
		});

	if(!headers_string.empty())
		out([&headers_string](const mutable_buffer &out)
		{
			return copy(out, headers_string);
		});

	if(!headers_vector.empty())
		write(out, headers_vector);

	if(termination)
		writeline(out);
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

	header headers[num_headers]
	{
		{ "Transfer-Encoding", "chunked" }
	};

	std::copy(begin(user_headers), end(user_headers), headers + 1);

	//TODO: bitrot
	assert(0);
/*
	response
	{
		code, {}, closure, { headers, headers + num_headers }
	};
*/
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

	const ilist<const const_buffer> iov
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

		else if(iequals(h.first, "content-type"s))
			this->content_type = h.second;

		else if(iequals(h.first, "transfer-encoding"s))
			this->transfer_encoding = h.second;

		else if(iequals(h.first, "server"s))
			this->server = h.second;

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
		throw parse::buffer_error
		{
			"parse buffer short by %zu to hold %zu total bytes of content",
			remain,
			length
		};

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
	header h{pc};
	const char *const &started{h.first.data()}, *stopped{started};
	for(; !h.first.empty(); stopped = h.second.data() + h.second.size(), h = header{pc})
		if(c)
			c(h);

	return { started, stopped };
}()}
{
}

ircd::http::header::header(const line &line)
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
	throw_error(e);
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
	throw_error(e);
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
		throw std::out_of_range{"Failed to find value for required query string key"};

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
		throw error
		{
			BAD_REQUEST, "Invalid content-length value"
		};

	return ret;
}

std::string
ircd::http::strung(const vector_view<const header> &headers)
{
	return ircd::string(serialized(headers), [&]
	(window_buffer out)
	{
		write(out, headers);
		return out.consumed();
	});
}

/// Indicates the buffer size required to write these headers. This size
/// may include room for a terminating null character which may be written
/// by write(headers). Only use write(headers) to know the actually written
/// string size (without null) not this.
size_t
ircd::http::serialized(const vector_view<const header> &headers)
{
	// Because the write(header) functions use fmt::sprintf we have to
	// indicate an extra space for a null string terminator to not overlof
	const size_t initial{!headers.empty()};
	return std::accumulate(std::begin(headers), std::end(headers), initial, []
	(auto &ret, const auto &pair)
	{
		//            key                 :   SP  value                CRLF
		return ret += pair.first.size() + 1 + 1 + pair.second.size() + 2;
	});
}

void
ircd::http::write(window_buffer &out,
                  const vector_view<const header> &headers)
{
	for(const auto &header : headers)
		write(out, header);
}

void
ircd::http::write(window_buffer &out,
                  const header &header)
{
	if(header.second.empty())
		return;

	assert(!header.first.empty());
	if(unlikely(header.first.empty()))
		return;

	writeline(out, [&header](const mutable_buffer &out) -> size_t
	{
		return fmt::sprintf
		{
			out, "%s: %s", header.first, header.second
		};
	});
}

/// Close over the user's closure to append a newline.
void
ircd::http::writeline(window_buffer &write,
                      const window_buffer::closure &closure)
{
	// A new window_buffer is implicit constructed out of the mutable_buffer
	// otherwise presented to this closure as its write window.
	write([&closure](window_buffer write)
	{
		const auto newline{[](const mutable_buffer &out)
		{
			return copy(out, "\r\n"_sv);
		}};

		write(closure);
		write(newline);
		return write.consumed();
	});
}

void
ircd::http::writeline(window_buffer &write)
{
	writeline(write, [](const mutable_buffer &out)
	{
		return 0;
	});
}

void
ircd::http::throw_error(const qi::expectation_failure<const char *> &e)
{
	const auto rule
	{
		ircd::string(e.what_)
	};

	throw error
	{
		code::BAD_REQUEST, fmt::snstringf
		{
			512, "I require a valid HTTP %s. You sent %zu invalid characters starting with `%s'.",
			between(rule, "<", ">"),
			size_t(e.last - e.first),
			string_view{e.first, e.last}
		}
	};
}

ircd::http::error::error(const enum code &code,
                         std::string content,
                         const vector_view<const header> &headers)
:error
{
	code, std::move(content), strung(headers)
}
{
}

ircd::http::error::error(const enum code &code,
                         std::string content,
                         std::string headers)
:ircd::error{generate_skip}
,code{code}
,content{std::move(content)}
,headers{std::move(headers)}
{
	snprintf(buf, sizeof(buf), "%d %s", int(code), status(code).c_str());
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
		throw ircd::error{"Invalid HTTP status code"};

	return http::code(ret);
}

ircd::string_view
ircd::http::status(const enum code &code)
try
{
	return reason.at(code);
}
catch(const std::out_of_range &e)
{
	log::warning
	{
		"No reason string for HTTP status code %d", int(code)
	};

	return ""_sv;
}
