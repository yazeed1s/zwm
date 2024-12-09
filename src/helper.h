/*
 * BSD 2-Clause License
 * Copyright (c) 2024, Yazeed Alharthi
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *	  1. Redistributions of source code must retain the above copyright
 *	  notice, this list of conditions and the following disclaimer.
 *
 *	  2. Redistributions in binary form must reproduce the above copyright
 *	  notice, this list of conditions and the following disclaimer in the
 *	  documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef ZWM_HELPER_H
#define ZWM_HELPER_H

#include "logger.h"

#define LEN(x)								   (sizeof(x) / sizeof(*x))
#define CLEANMASK(mask)						   (mask & ~(0 | XCB_MOD_MASK_LOCK))
#define IS_TILED(c)							   (c->state == TILED)
#define IS_FLOATING(c)						   (c->state == FLOATING)
#define IS_FULLSCREEN(c)					   (c->state == FULLSCREEN)
#define IS_EXTERNAL(n)						   (n->node_type == EXTERNAL_NODE)
#define IS_INTERNAL(n)						   (n->node_type == INTERNAL_NODE)
#define IS_ROOT(n)							   (n->node_type == ROOT_NODE)

#define DEFINE_KEY(mask, keysym, handler, arg) {mask, keysym, handler, arg}
#define DEFINE_MAPPING(name, value)			   {name, value}

/* spent way too many hours hunting double-free bugs. This should handle it. */
#define _FREE_(ptr)                                                            \
	do {                                                                       \
		if (ptr) {                                                             \
			free(ptr);                                                         \
			ptr = NULL;                                                        \
		}                                                                      \
	} while (0)

#define _LOG_(level, format, ...)                                              \
	do {                                                                       \
		log_message(level,                                                     \
					"[%s:%s():%d] " format,                                    \
					__FILE__,                                                  \
					__func__,                                                  \
					__LINE__,                                                  \
					##__VA_ARGS__);                                            \
	} while (0)

#define MAX(a, b)                                                              \
	({                                                                         \
		__typeof__(a) _a = (a);                                                \
		__typeof__(b) _b = (b);                                                \
		_a > _b ? _a : _b;                                                     \
	})

#endif /* ZWM_HELPER_H */
