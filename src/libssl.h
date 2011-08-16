#ifndef _LIBSSL_H
#define _LIBSSL_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "common.h"
#include "dl.h"
#include <bdlib/src/String.h>

#include ".defs/libssl_pre.h"

#ifdef EGG_SSL_EXT
# ifndef EGG_SSL_INCS
#  include <openssl/ssl.h>
#  define EGG_SSL_INCS 1
# endif

typedef DH* (*dh_callback_t)(SSL*, int, int);
typedef int (*verify_callback_t)(int, X509_STORE_CTX*);

#include ".defs/libssl_post.h"

typedef void (*SSL_CTX_set_tmp_dh_callback_t)(SSL_CTX*, dh_callback_t);
typedef void (*SSL_CTX_set_verify_t)(SSL_CTX*, int, verify_callback_t);
typedef void (*SSL_set_verify_t)(SSL*, int, verify_callback_t);

#endif /* EGG_SSL_EXT */

int load_libssl();
int unload_libssl();

#endif /* !_LIBSSL_H */
