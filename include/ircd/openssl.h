/*
 * charybdis: 21st Century IRC++d
 *
 * Copyright (C) 2017 Charybdis Development Team
 * Copyright (C) 2017 Jason Volk <jason@zemos.net>
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

#pragma once
#define HAVE_IRCD_OPENSSL_H

// Forward declarations for OpenSSL because it is not included here.
struct ssl_st;
struct x509_st;

/// OpenSSL library interface. Provides things we need to expose from OpenSSL
/// to the rest of the project. Anything that employs forward declared types
/// here is lower level meant for definition files which may or may not include
/// OpenSSL directly but use this interface for DRY nonetheless. Otherwise
/// higher level wrappers should be used if available here.
namespace ircd::openssl
{
	IRCD_EXCEPTION(ircd::error, error)

	struct init;

	using SSL = ::ssl_st;
	using X509 = ::x509_st;

	string_view version();

	// Observers
	string_view error_string(const mutable_buffer &buf, const ulong &);
	ulong peek_error();

	// Using these will clobber other libraries (like boost); so don't
	ulong get_error();
	void clear_error();

	// Convert PEM string to X509
	X509 &read(X509 *out, const string_view &cert);

	// Convert X509 certificate to DER encoding
	const_raw_buffer i2d(const mutable_raw_buffer &out, const X509 &);

	// Convert PEM string to DER
	const_raw_buffer cert2d(const mutable_raw_buffer &out, const string_view &cert);

	// Get X509 certificate from struct SSL (get SSL from a socket)
	const X509 &get_peer_cert(const SSL &);
	X509 &get_peer_cert(SSL &);
}

struct ircd::openssl::init
{
	init();
	~init() noexcept;
};
