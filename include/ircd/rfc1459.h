// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_RFC1459_H

/// Legacy IRC grammars & tools
namespace ircd::rfc1459
{
	struct nick;
	struct user;
	struct host;
	struct cmd;
	struct parv;
	struct pfx;
	struct line;

	IRCD_EXCEPTION(ircd::error, error)
	IRCD_EXCEPTION(error, syntax_error)

	std::ostream &operator<<(std::ostream &, const pfx &);
	std::ostream &operator<<(std::ostream &, const cmd &);
	std::ostream &operator<<(std::ostream &, const parv &);
	std::ostream &operator<<(std::ostream &, const line &);   // unterminated
}

namespace ircd::rfc1459::character
{
	enum attr :uint;
	using attr_t = std::underlying_type<attr>::type;

	extern const std::array<attr_t, 256> attrs;
	extern const std::array<unsigned char, 256> tolower_tab;
	extern const std::array<unsigned char, 256> toupper_tab;

	// Tests
	bool is(const uint8_t &c, const attr &attr);

	// Transforms
	const uint8_t &tolower(const uint8_t &c);
	const uint8_t &toupper(const uint8_t &c);

	// Get all characters for an attribute mask
	size_t gather(const attr &attr, uint8_t *const &buf, const size_t &max);
	std::string gather(const attr &attr);

	// Like gather() but with special considerations for boost::spirit's char_()
	size_t charset(const attr &attr, uint8_t *const &buf, const size_t &max);
	std::string charset(const attr &attr);
}

enum ircd::rfc1459::character::attr
:uint
{
	PRINT    = 0x00000001,
	CNTRL    = 0x00000002,
	ALPHA    = 0x00000004,
	PUNCT    = 0x00000008,
	DIGIT    = 0x00000010,
	SPACE    = 0x00000020,
	NICK     = 0x00000040,
	CHAN     = 0x00000080,
	KWILD    = 0x00000100,
	CHANPFX  = 0x00000200,
	USER     = 0x00000400,
	HOST     = 0x00000800,
	NONEOS   = 0x00001000,
	SERV     = 0x00002000,
	EOL      = 0x00004000,
	MWILD    = 0x00008000,
	LET      = 0x00010000,    // an actual letter
	FCHAN    = 0x00020000,    // a 'fake' channel char
};

namespace ircd::rfc1459
{
	struct less;

	using character::is;
	using character::toupper;
	using character::tolower;
	using character::gather;

	inline bool is_print(const char &c)          { return is(c, character::PRINT);                 }
	inline bool is_host(const char &c)           { return is(c, character::HOST);                  }
	inline bool is_user(const char &c)           { return is(c, character::USER);                  }
	inline bool is_chan(const char &c)           { return is(c, character::CHAN);                  }
	inline bool is_chan_prefix(const char &c)    { return is(c, character::CHANPFX);               }
	inline bool is_fake_chan(const char &c)      { return is(c, character::FCHAN);                 }
	inline bool is_kwild(const char &c)          { return is(c, character::KWILD);                 }
	inline bool is_mwild(const char &c)          { return is(c, character::MWILD);                 }
	inline bool is_nick(const char &c)           { return is(c, character::NICK);                  }
	inline bool is_letter(const char &c)         { return is(c, character::LET);                   }
	inline bool is_digit(const char &c)          { return is(c, character::DIGIT);                 }
	inline bool is_cntrl(const char &c)          { return is(c, character::CNTRL);                 }
	inline bool is_alpha(const char &c)          { return is(c, character::ALPHA);                 }
	inline bool is_space(const char &c)          { return is(c, character::SPACE);                 }
	inline bool is_noneos(const char &c)         { return is(c, character::NONEOS);                }
	inline bool is_eol(const char &c)            { return is(c, character::EOL);                   }
	inline bool is_serv(const char &c)           { return is(c, character::SERV) || is_nick(c);    }
	inline bool is_id(const char &c)             { return is_digit(c) || is_letter(c);             }
	inline bool is_alnum(const char &c)          { return is_digit(c) || is_alpha(c);              }
	inline bool is_punct(const char &c)          { return !is_cntrl(c) && !is_alnum(c);            }
	inline bool is_lower(const char &c)          { return is_alpha(c) && uint8_t(c) > 0x5f;        }
	inline bool is_upper(const char &c)          { return is_alpha(c) && uint8_t(c) < 0x60;        }
	inline bool is_graph(const char &c)          { return is_print(c) && uint8_t(c) != 0x32;       }
	inline bool is_ascii(const char &c)          { return uint8_t(c) < 0x80;                       }
	inline bool is_xdigit(const char &c)
	{
		return is_digit(c) || ('a' <= c && c <= 'f') || ('A' <= c && c <= 'F');
	}
}

struct ircd::rfc1459::nick
:string_view
{
	using string_view::string_view;
};

struct ircd::rfc1459::user
:string_view
{
	using string_view::string_view;
};

struct ircd::rfc1459::host
:string_view
{
	using string_view::string_view;
};

struct ircd::rfc1459::cmd
:string_view
{
	using string_view::string_view;
};

struct ircd::rfc1459::parv
:std::vector<string_view>
{
	using std::vector<string_view>::vector;
};

struct ircd::rfc1459::pfx
{
	struct nick nick;
	struct user user;
	struct host host;

	bool empty() const;
};

struct ircd::rfc1459::line
{
	struct pfx pfx;
	struct cmd cmd;
	struct parv parv;

	bool empty() const;
	auto operator[](const size_t &pos) const     { return parv.at(pos);                            }
	auto operator[](const size_t &pos)           { return parv.at(pos);                            }

	line(const char *&start, const char *const &stop);
	line() = default;
};

struct ircd::rfc1459::less
{
	bool operator()(const char *const &a, const char *const &b) const;
	bool operator()(const std::string &a, const std::string &b) const;
	bool operator()(const std::string_view &a, const std::string_view &b) const;
};

inline bool
ircd::rfc1459::less::operator()(const std::string_view &a,
                                const std::string_view &b)
const
{
	return std::lexicographical_compare(begin(a), end(a), begin(b), end(b), []
	(const char &a, const char &b)
	{
		return tolower(a) < tolower(b);
	});
}

inline bool
ircd::rfc1459::less::operator()(const std::string &a,
                                const std::string &b)
const
{
	return std::lexicographical_compare(begin(a), end(a), begin(b), end(b), []
	(const char &a, const char &b)
	{
		return tolower(a) < tolower(b);
	});
}

inline bool
ircd::rfc1459::less::operator()(const char *const &a,
                                const char *const &b)
const
{
	return std::lexicographical_compare(a, a + strlen(a), b, b + strlen(b), []
	(const char &a, const char &b)
	{
		return tolower(a) < tolower(b);
	});
}

inline const uint8_t &
ircd::rfc1459::character::tolower(const uint8_t &c)
{
	return tolower_tab[c];
}

inline const uint8_t &
ircd::rfc1459::character::toupper(const uint8_t &c)
{
	return toupper_tab[c];
}

inline bool
ircd::rfc1459::character::is(const uint8_t &c,
                             const attr &attr)
{
	return (attrs[c] & attr) == attr;
}
