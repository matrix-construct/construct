/**
 * Copyright (C) 2016 Charybdis Development Team
 * Copyright (C) 2016 Jason Volk
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

#pragma once
#define HAVE_IRCD_FMT_H

namespace ircd {
namespace fmt  {

IRCD_EXCEPTION(ircd::error, error);
IRCD_EXCEPTION(error, invalid_format);
IRCD_EXCEPTION(error, invalid_type);
IRCD_EXCEPTION(error, illegal);

//
// Module API
//
extern const char SPECIFIER;
extern const char SPECIFIER_TERMINATOR;

using arg = std::tuple<const void *, std::type_index>;

// Structural representation of a format specifier
struct spec
{
	char sign         = '+';
	int width         = 0;
	string_view name;

	spec() = default;
};

// A format specifier handler module.
// This allows a new "%foo" to be defined with custom handling.
class specifier
{
	std::set<std::string> names;

  public:
	virtual bool operator()(char *&out, const size_t &max, const spec &, const arg &) const = 0;

	specifier(const std::initializer_list<std::string> &names);
	specifier(const std::string &name);
	virtual ~specifier() noexcept;
};

const std::map<string_view, specifier *> &specifiers();

//
// User API
//

// * The arguments are not restricted by stdarg limitations. You can pass a real std::string.
// * The function participates in the custom protocol-safe ruleset.
class snprintf
{
	const char *fstart;                          // Current running position in the fmtstr
	const char *fstop;                           // Saved state from the last position
	const char *fend;                            // past-the-end iterator of the fmtstr
	const char *obeg;                            // Saved beginning of the output buffer
	const char *oend;                            // past-the-end iterator of the output buffer
	char *out;                                   // Current running position of the output buffer
	short idx;                                   // Keeps count of the args for better err msgs

  protected:
	auto finished() const                        { return !fstart || fstop >= fend;                }
	auto remaining() const                       { return (oend - out) - 1;                        }
	auto consumed() const                        { return size_t(out - obeg);                      }
	auto &buffer() const                         { return obeg;                                    }

	void append(const char *const &begin, const char *const &end);
	void argument(const arg &);

	IRCD_OVERLOAD(internal)
	snprintf(internal_t, char *const &, const size_t &, const char *const &, const va_rtti &);

  public:
	operator ssize_t() const                     { return consumed();                              }

	template<class... Args>
	snprintf(char *const &buf,
	         const size_t &max,
	         const char *const &fmt,
	         Args&&... args)
	:snprintf
	{
		internal, buf, max, fmt, va_rtti(args...)
	}{}
};

struct vsnprintf
:snprintf
{
	vsnprintf(char *const &buf,
	          const size_t &max,
	          const char *const &fmt,
	          const va_rtti &ap)
	:snprintf
	{
		internal, buf, max, fmt, ap
	}{}
};

} // namespace fmt
} // namespace ircd
