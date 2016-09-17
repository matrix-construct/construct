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

#ifdef __cplusplus
namespace ircd {
namespace fmt  {

IRCD_EXCEPTION(ircd::error, error);
IRCD_EXCEPTION(error, invalid_format);
IRCD_EXCEPTION(error, invalid_type);
IRCD_EXCEPTION(error, illegal);

//
// module/internal API
//
extern const char SPECIFIER;
extern const char SPECIFIER_TERMINATOR;

using ptrs = std::vector<const void *>;
using types = std::vector<std::type_index>;
using arg = std::tuple<const void *const &, const std::type_index &>;

// Structural representation of a format specifier
struct spec
{
	char sign;
	int width;
	std::string name;

	spec();
};

// A format specifier handler module.
// This allows a new "%foo" to be defined with custom handling.
class specifier
{
	std::vector<std::string> names;

  public:
	virtual bool operator()(char *&out, const size_t &max, const spec &, const arg &) const = 0;

	specifier(const std::initializer_list<std::string> &names);
	specifier(const std::string &name);
	virtual ~specifier() noexcept;
};

const std::map<std::string, specifier *> &specifiers();
ssize_t _snprintf(char *const &, const size_t &, const char *const &, const ptrs &, const types &);

//
// public API
//

// Implementation of the traditional snprintf(), as best as practical:
// * The arguments are not restricted by va_list limitations. You can pass a real std::string.
// * The function participates in the custom protocol-safe ruleset, and the behavior is non-standard.
//   To be sure to get truly /standard/ snprintf() behavior use ::snprintf() instead.
template<class... Args> ssize_t snprintf(char *const &buf, const size_t &max, const char *const &fmt, Args&&... args);


template<class... Args>
ssize_t
snprintf(char *const &buf,
         const size_t &max,
         const char *const &fmt,
         Args&&... args)
{
	static const std::vector<std::type_index> types
	{
		typeid(Args)...
	};

	const std::vector<const void *> ptrs
	{
		std::addressof(args)...
	};

	return _snprintf(buf, max, fmt, ptrs, types);
}

}      // namespace fmt
}      // namespace ircd
#endif // __cplusplus
