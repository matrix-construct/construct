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
#define HAVE_IRCD_RFC1035_H

/// RFC 1035 "Domain Names" (Nov. 1987)
///
namespace ircd::rfc1035
{
	IRCD_EXCEPTION(ircd::error, error)

	struct header;
	struct question;
	struct answer;
	struct record;
	enum class op :uint8_t;

	// Section 2.3.4 Size Limits
	constexpr size_t LABEL_MAX {63};
	constexpr size_t NAME_MAX {255};
	constexpr size_t TTL_MAX {std::numeric_limits<int32_t>::max()};

	constexpr size_t LABEL_BUF_SIZE {LABEL_MAX + 1};
	constexpr size_t NAME_BUF_SIZE {NAME_MAX + 1};

	extern const std::array<string_view, 25> rcode;
	extern const std::unordered_map<string_view, uint16_t> qtype;
	extern const std::map<uint16_t, string_view> rqtype;

	bool valid_label(const string_view &);
	bool valid_name(const string_view &);
	const_buffer make_name(const mutable_buffer &out, const string_view &fqdn);
	size_t parse_name(const mutable_buffer &out, const const_buffer &in);

	mutable_buffer make_query(const mutable_buffer &, const header &, const vector_view<const question> &);
	mutable_buffer make_query(const mutable_buffer &, const uint16_t &id, const vector_view<const question> &);
	mutable_buffer make_query(const mutable_buffer &, const uint16_t &id, const question &);
}

/// Direct representation of the DNS header. This is laid out for
/// little-endian platforms only. The uint16_t's are big endian when this is
/// punned on the wire data. The debug() function makes it into a pretty
/// string but makes no endian adjustments.
///
struct ircd::rfc1035::header
{
	uint16_t id;         ///< query identification number
	uint8_t rd:1;        ///< 0 = recursion not desired; 1 = desired
	uint8_t tc:1;        ///< 0 = not-truncated; 1 = 512 bytes of reply only
	uint8_t aa:1;        ///< 0 = non-authoritative; 1 = authoritative
	uint8_t opcode:4;    ///< purpose of message
	uint8_t qr:1;        ///< 0 = query; 1 = respnse
	uint8_t rcode:4;     ///< response code
	uint8_t cd:1;        ///< checking disabled by resolver
	uint8_t ad:1;        ///< authentic data from named
	uint8_t unused:1;    ///< unused bits (MBZ as of 4.9.3a3)
	uint8_t ra:1;        ///< 1 = recursion available
	uint16_t qdcount;    ///< number of question entries
	uint16_t ancount;    ///< number of answer entries
	uint16_t nscount;    ///< number of authority entries
	uint16_t arcount;    ///< number of resource entries

	std::string debug() const;
}
__attribute__((packed));

static_assert
(
	sizeof(ircd::rfc1035::header) == 12,
	"The RFC1035 header is not the right size in this environment"
);

/// DNS operation code
///
enum class ircd::rfc1035::op
:uint8_t
{
	QUERY    = 0,   ///< Query [RFC 1035]
	IQUERY   = 1,   ///< Inverse Query [RFC 1035, RFC 3425]
	STATUS   = 2,   ///< Server status request [RFC 1035]
	NOTIFY   = 4,   ///< Notify [RFC 1996]
	UPDATE   = 5,   ///< Update [RFC 2136]
};

/// Helper class to construct or parse a question. An object is constructed
/// with a fully qualified domain string and the query type (qtype).
///
/// Note that each part of the fqdn cannot be longer than 63 characters. The
/// supplied buffer must be large enough to hold the output, which is about
/// the length of the fqdn + 6 bytes. The qtype can be specified as a string
/// i.e "A" or "PTR" and it will be translated into the protocol number for
/// you in the constructor. All integers are dealt with in host byte order.
///
struct ircd::rfc1035::question
{
	uint16_t qtype {0};
	uint16_t qclass {0x01};
	string_view name;
	char namebuf[NAME_BUF_SIZE];

	/// Composes the question into buffer, returns used portion
	mutable_buffer print(const mutable_buffer &) const;
	const_buffer parse(const const_buffer &);

	/// Supply fully qualified domain name and numerical query type
	question(const string_view &fqdn, const uint16_t &qtype);

	/// Supply fully qualified domain name and name of query type i.e "A"
	question(const string_view &fqdn, const string_view &qtype)
	:question{fqdn, rfc1035::qtype.at(qtype)}
	{}

	question() = default;
};

/// Helper class to parse an answer. When the DNS header is received we get
/// an answer count. For each answer in the answer section parse() is called
/// which stocks this object and then returns a buffer tight to that specific
/// answer section. The `rdata` is the actual record content which the user
/// can then treat later with rfc1035::record. All integers are dealt with in
/// host byte order.
///
struct ircd::rfc1035::answer
{
	uint16_t qtype {0};
	uint16_t qclass {0};
	uint32_t ttl {0};
	uint16_t rdlength {0};
	const_buffer rdata;
	string_view name;
	char namebuf[NAME_BUF_SIZE];

	const_buffer parse(const const_buffer &);

	answer() = default;
};

/// Resource record abstract base. The purpose of this abstraction is to allow
/// records of any variety to all be dealt with via a `rfc1035::record *` ptr
/// and then be downcasted to the specific derived type elaborated below. Use
/// the as() template to downcast.
///
/// Generally this object is not instantiated directly; each record type will
/// construct this instead. Nevertheless, the full raw data and type number
/// for the record is available in here. All constructors (for both this
/// abstraction and for the derivations) take an already-parsed rfc1035::answer
/// as their argument; the qtype and ttl information from the answer header is
/// included while the legacy qclass is omitted.
///
struct ircd::rfc1035::record
{
	struct A;
	struct AAAA;
	struct CNAME;
	struct SRV;

	uint16_t type {0};
	time_t ttl {0};
	const_buffer rdata;

	template<class T> const T &as() const;

	record(const answer &);
	record(const uint16_t &type);
	record() = default;
	virtual ~record() noexcept;
};

namespace ircd::rfc1035
{
	bool operator==(const record::A &, const record::A &);
	bool operator!=(const record::A &, const record::A &);
	bool operator==(const record::AAAA &, const record::AAAA &);
	bool operator!=(const record::AAAA &, const record::AAAA &);
	bool operator==(const record::CNAME &, const record::CNAME &);
	bool operator!=(const record::CNAME &, const record::CNAME &);
	bool operator==(const record::SRV &, const record::SRV &);
	bool operator!=(const record::SRV &, const record::SRV &);
}

/// Downcast an abstract record reference to the specific record structure.
template<class T>
const T &
ircd::rfc1035::record::as()
const
{
	return dynamic_cast<T &>(*this);
}

//
// Types of records
//

/// IPv4 address record.
/// The integer is laid out in host byte order.
struct ircd::rfc1035::record::A
:record
{
	uint32_t ip4 {0};

	A(const answer &);
	A();
};

/// IPv6 address record.
/// The integer is laid out in host byte order.
struct ircd::rfc1035::record::AAAA
:record
{
	uint128_t ip6 {0};

	AAAA(const answer &);
	AAAA();
};

/// Canonical name aliasing record
struct ircd::rfc1035::record::CNAME
:record
{
	string_view name;
	char namebuf[NAME_BUF_SIZE];

	CNAME(const answer &);
	CNAME();
};

/// Service record.
/// The integers are laid out in host byte order.
struct ircd::rfc1035::record::SRV
:record
{
	uint16_t priority {0};
	uint16_t weight {0};
	uint16_t port {0};
	string_view tgt;
	char tgtbuf[NAME_BUF_SIZE];

	SRV(const answer &);
	SRV();
};
