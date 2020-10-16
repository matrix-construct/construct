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
#define HAVE_IRCD_RFC3986_H

/// Universal Resource Indicator (URI) grammars & tools
namespace ircd::rfc3986
{
	struct uri;

	IRCD_EXCEPTION(ircd::error, error)
	IRCD_EXCEPTION(error, coding_error)
	IRCD_EXCEPTION(coding_error, encoding_error)
	IRCD_EXCEPTION(coding_error, decoding_error)

	constexpr size_t HOSTNAME_MAX      { rfc1035::LABEL_MAX  };
	constexpr size_t HOSTNAME_BUFSIZE  { HOSTNAME_MAX + 1    };
	constexpr size_t DOMAIN_MAX        { rfc1035::NAME_MAX   };
	constexpr size_t DOMAIN_BUFSIZE    { DOMAIN_MAX + 1      };
	constexpr size_t REMOTE_MAX        { DOMAIN_MAX + 6      };
	constexpr size_t REMOTE_BUFSIZE    { REMOTE_MAX + 1      };

	// Percent-encode arbitrary string; binary/non-printable characters OK
	string_view encode(const mutable_buffer &, const string_view &url);

	// x-www-form-urlencoded generator. We make use of the existing key-value
	// aggregator `json::members` for the inputs, but result is a www-form.
	string_view encode(const mutable_buffer &, const json::members &);

	// Decode percent-encoded strings. N.B. this refuses to decode potentially
	// troublesome non-printable characters preventing accidental leakage into
	// the system; exception will be thrown.
	string_view decode(const mutable_buffer &, const string_view &url);

	// Decode percent-encoded strings. N.B. this decodes into any character
	// including control codes like %00 into '\0' etc. Use with caution.
	const_buffer decode_unsafe(const mutable_buffer &, const string_view &url);

	// extractor suite
	uint16_t port(const string_view &remote); // get portnum from valid remote
	string_view host(const string_view &remote); // get host without portnum
}

namespace ircd
{
	/// Convenience namespace ircd::url allows developers to use `url::encode`
	/// and `url::decode` rather than rfc3986:: explicitly.
	namespace url = rfc3986;
}

/// URI component decomposition. The ctor will throw on invalid inputs. This
/// device is completely thin and only creates views into the input string.
struct ircd::rfc3986::uri
{
	string_view scheme;
	string_view user;
	string_view remote;
	string_view path;
	string_view query;
	string_view fragment;

	uri(const string_view &);
	uri() = default;
};

#pragma GCC visibility push(default)
// Exposition of individual grammatical elements. Due to the diverse and
// foundational applications of this unit, we offer a public list of references
// to individual rules in the grammar; many of these are directly specified in
// the RFC. Developers can select or compose from these elements while
// developing within other units in the project. This avoids exposing a large
// suite of arbitrary wrapper functions; instead abstract functions are offered
// which take a reference to any apropos rule. To avoid exposure of
// boost::spirit in project headers these types are carefully crafted thin forward
// declarations, so spirit itself is not included here.
namespace ircd::rfc3986::parser
{
	using it = const char *;
	using unused = boost::spirit::unused_type;

	template<class R = unused>
	using rule = boost::spirit::qi::rule<it, R, unused, unused, unused>;

	extern const rule<> sub_delims;
	extern const rule<> gen_delims;
	extern const rule<> reserved;
	extern const rule<> unreserved;
	extern const rule<> pct_encoded;
	extern const rule<> pchar;
	extern const rule<> query;
	extern const rule<> fragment;
	extern const rule<> segment;
	extern const rule<> segment_nz;
	extern const rule<> segment_nz_nc;
	extern const rule<> path_rootless;
	extern const rule<> path_noscheme;
	extern const rule<> path_absolute;
	extern const rule<> path_abempty;
	extern const rule<> path;
	extern const rule<> reg_name;
	extern const rule<> userinfo;

	extern const rule<uint16_t> port; // note in examples portnums are optional

	extern const rule<> ip4_octet;
	extern const rule<> ip4_address;  // 1.2.3.4
	extern const rule<> ip4_literal;  // 1.2.3.4
	extern const rule<> ip4_remote;   // 1.2.3.4:12345

	extern const rule<> ip6_char;
	extern const rule<> ip6_h16;
	extern const rule<> ip6_piece;
	extern const rule<> ip6_ipiece;
	extern const rule<> ip6_ls32;
	extern const rule<> ip6_addr[9];
	extern const rule<> ip6_address;  // ::1
	extern const rule<> ip6_literal;  // [::1]
	extern const rule<> ip6_remote;   // [::1]:12345

	extern const rule<> ip_address;   // 1.2.3.4 | ::1
	extern const rule<> ip_literal;   // 1.2.3.4 | [::1]
	extern const rule<> ip_remote;    // 1.2.3.4:12345 | [::1]:12345

	extern const rule<> hostname;     // foo
	extern const rule<> domain;       // foo.com
	extern const rule<> hostport;     // foo.bar.com:12345

	extern const rule<> host;         // 1.2.3.4 | ::1 | foo.com
	extern const rule<> host_literal; // 1.2.3.4 | [::1] | foo.com

	extern const rule<> remote;       // 1.2.3.4:12345 | [::1]:12345 | foo.com:12345

	extern const rule<> authority;
	extern const rule<> scheme;
	extern const rule<> hier_part;
	extern const rule<> relative_part;
	extern const rule<> relative_ref;
	extern const rule<> absolute_uri;
	extern const rule<> uri;
	extern const rule<> uri_ref;     // uri | relative_ref
}
#pragma GCC visibility pop

// Validator suite
namespace ircd::rfc3986
{
	// Variable rule...
	void valid(const parser::rule<> &, const string_view &);
	bool valid(std::nothrow_t, const parser::rule<> &, const string_view &);

	// convenience: parser::hostname
	void valid_hostname(const string_view &);
	bool valid_hostname(std::nothrow_t, const string_view &);

	// convenience: parser::host
	void valid_host(const string_view &);
	bool valid_host(std::nothrow_t, const string_view &);

	// convenience: parser::domainname
	void valid_domain(const string_view &);
	bool valid_domain(std::nothrow_t, const string_view &);

	// convenience: parser::remote
	void valid_remote(const string_view &);
	bool valid_remote(std::nothrow_t, const string_view &);
}
