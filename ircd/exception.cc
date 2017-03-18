/*
 * charybdis: 21st Century IRCd
 * exception.cc: Exception root
 *
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
 *
 */

ssize_t
ircd::exception::generate(const char *const &fmt,
                          const va_rtti &ap)
noexcept
{
	return fmt::vsnprintf(buf, sizeof(buf), fmt, ap);
}

ssize_t
ircd::exception::generate(const char *const &name,
                          const char *const &fmt,
                          const va_rtti &ap)
noexcept
{
	size_t size(0);
	const bool empty(!fmt || !fmt[0] || fmt[0] == ' ');
	size = strlcat(buf, name, sizeof(buf));
	size = strlcat(buf, empty? "." : ": ", sizeof(buf));
	if(size < sizeof(buf))
		size += fmt::vsnprintf(buf + size, sizeof(buf) - size, fmt, ap);

	return size;
}
