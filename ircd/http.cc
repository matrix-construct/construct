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

#include <boost/spirit/include/qi.hpp>
#include <boost/fusion/include/std_pair.hpp>
#include <boost/fusion/adapted/std_pair.hpp>
#include <boost/fusion/adapted/std_tuple.hpp>
#include <ircd/lex_cast.h>

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
/*
BOOST_FUSION_ADAPT_STRUCT
(
    ircd::http::request,
    ( decltype(ircd::http::request::command),   command  )
    ( decltype(ircd::http::request::headers),   headers  )
)

BOOST_FUSION_ADAPT_STRUCT
(
    ircd::http::response,
    ( decltype(ircd::http::response::status),   status  )
    ( decltype(ircd::http::response::headers),  headers )
)
*/
namespace ircd {
namespace http {

namespace spirit = boost::spirit;
namespace qi = spirit::qi;

using spirit::unused_type;

using qi::lit;
using qi::string;
using qi::char_;
using qi::repeat;
using qi::omit;
using qi::raw;
using qi::attr;
using qi::eps;

template<class it,
         class top = unused_type>
struct grammar
:qi::grammar<it, top>
,parse::grammar
{
	template<class R = unused_type> using rule = qi::rule<it, R>;

	rule<> NUL;
	rule<> SP;
	rule<> HT;
	rule<> CR;
	rule<> LF;
	rule<> CRLF;
	rule<> colon;

	rule<> WS;
	rule<> ws;
	rule<string_view> token;
	rule<string_view> string;

	rule<string_view> line;
	rule<string_view> method;
	rule<string_view> resource;
	rule<string_view> version;
	rule<string_view> status;
	rule<string_view> reason;
	rule<http::line::request> request_line;
	rule<http::line::response> response_line;

	rule<string_view> key;
	rule<string_view> value;
	rule<http::line::header> header;

	rule<unused_type> headers;
	rule<unused_type> request;
	rule<unused_type> response;

	grammar(rule<top> &top_rule, const char *const &name);
};

[[noreturn]]
void bail()
{
	throw error(code::BAD_REQUEST);
}

template<class it,
         class top>
grammar<it, top>::grammar(qi::rule<it, top> &top_rule,
                          const char *const &name)
:grammar<it, top>::base_type
{
	top_rule
}
,parse::grammar
{
	name
}
,NUL
{
	lit('\0')
	,"NUL"
}
,SP
{
	lit(' ')
	,"SP"
}
,HT
{
	lit('\t')
	,"HT"
}
,CR
{
	lit('\r')
	,"CR"
}
,CRLF
{
	lit('\r') >> lit('\n')
	,"CRLF"
}
,colon
{
	lit(':')
	,"colon"
}
,WS
{
	(SP | HT)
	,"whitespace"
}
,ws
{
	+(SP | HT)
	,"whitespaces"
}
,token
{
	raw[+(char_ - (NUL | CR | LF | WS))]
	,"token"
}
,string
{
	raw[+(char_ - (NUL | CR | LF))]
	,"string"
}
,line
{
	-ws >> -string >> CRLF
	,"line"
}
,method
{
	raw[+(char_ - (NUL | CR | LF | WS))]
	,"method"
}
,resource
{
	raw[+(char_ - (NUL | CR | LF | WS))]
	,"resource"
}
,version
{
	raw[+(char_ - (NUL | CR | LF | SP))]
	,"version"
}
,status
{
	raw[repeat(3)[char_("0-9")]]
	,"status"
}
,reason
{
	raw[+(char_ - (NUL | CR | LF))]
	,"status"
}
,request_line
{
	method >> +SP >> resource >> +SP >> version
	,"request line"
}
,response_line
{
	version >> +SP >> status >> +SP >> reason
	,"response line"
}
,key
{
	raw[+(char_ - (NUL | CR | LF | WS | colon))]
	,"key"
}
,value
{
	string
	,"value"
}
,header
{
	key >> -ws >> colon >> -ws >> value
	,"header"
}
,headers
{
	+(-header % CRLF) >> CRLF
	,"headers"
}
,request
{
	request_line >> -ws >> CRLF >> headers
	,"request"
}
,response
{
	response_line >> -ws >> CRLF >> headers
	,"response"
}
{
}

struct parser
:grammar<const char *, unused_type>
{
	parser(): grammar { grammar::ws, "http.request" } {}
}
const parser;

} // namespace http
} // namespace ircd

ircd::http::content::content(parse::context &pc,
                             const headers &h)
:std::string(h.content_length, char())
{
	char *data(const_cast<char *>(this->data()));
	char *const stop(data + size());
	const size_t unparsed(pc.read - pc.parsed);
	const size_t have(std::min(unparsed, size()));
	memcpy(data, pc.parsed, have);
	data[have] = '\0';
	pc.parsed += have;
	data += have;
	if(data != stop)
	{
		pc.reader(data, stop);
		*data = '\0';
	}

	assert(data == stop);
}

ircd::http::headers::headers(parse::context &pc,
                             const closure &c)
{
	for(line::header h{pc}; !h.first.empty(); h = line::header{pc})
	{
		if(iequals(h.first, "host"))
			host = h.second;
		else if(iequals(h.first, "expect"))
			expect = h.second;
		else if(iequals(h.first, "te"))
			te = h.second;
		else if(iequals(h.first, "content-length"))
			content_length = lex_cast<size_t>(h.second);

		if(c)
			c(h);
	}
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
	throw error(code::BAD_REQUEST);
}

ircd::http::line::response::response(const line &line)
{
	static const auto grammar
	{
		parser.response_line
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
	throw error(code::BAD_REQUEST);
}

ircd::http::line::line(parse::context &pc)
:string_view{[&pc]
{
	static const auto grammar
	{
		parser.line
	};

	string_view ret;
	pc([&ret](const char *&start, const char *stop)
	{
		return qi::parse(start, stop, grammar, ret);
	});

	return ret;
}()}
{
}

namespace ircd {
namespace http {

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

ircd::http::error::error(const enum code &code,
                         const string_view &text)
:ircd::error{generate_skip}
,code{code}
,content{text}
{
	snprintf(buf, sizeof(buf), "%d %s", int(code), reason[code].data());
}
