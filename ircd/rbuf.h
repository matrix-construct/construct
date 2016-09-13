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

#ifdef __cplusplus
namespace ircd {

struct rbuf
{
	tape reel;                                   // @0       +80    ||
	std::exception_ptr eptr;                     // @80      +8      |
	uint16_t checked;                            // @88      +2      |
	uint16_t length;                             // @90      +2      |
	std::array<char, BUFSIZE> buf alignas(16);   // @96(92)  +512    |||||||||
	                                             // @608                     | 640
	bool terminated() const;                     // rbuf has LF termination
	uint16_t remaining() const;                  // bytes remaining in rbuf

	size_t handle_pck(const error_code &, const size_t) noexcept;
	void reset();

	rbuf();
};

static_assert(BUFSIZE == 512, "");

inline
rbuf::rbuf()
:checked{0}
,length{0}
{
}

inline void
rbuf::reset()
{
	checked = 0;
	length = 0;
}

inline size_t
rbuf::handle_pck(const error_code &ec,
                 const size_t bytes)
noexcept try
{
	if(ec)
		return 0;

	length += bytes;
	if(reel.append(buf.data(), length))
		return 0;

	if(terminated())
		throw rfc1459::syntax_error("invalid syntax"); //TODO: eps + ERR_

	checked = length;
	return remaining()?: throw rfc1459::syntax_error("message length exceeded"); //TODO: ERR_
}
catch(...)
{
	eptr = std::current_exception();
	return 0;
}

inline uint16_t
ircd::rbuf::remaining()
const
{
	return buf.size() - length;
}

inline bool
rbuf::terminated()
const
{
	const auto b(std::next(buf.rbegin(), remaining()));
	const auto e(std::next(buf.rbegin(), buf.size() - checked));
	return std::find(b, e, '\n') != e;
}

}      // namespace ircd
#endif // __cplusplus
