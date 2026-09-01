#ifndef BASE_HASH_CTXT_H
#define BASE_HASH_CTXT_H

#include "hash.h"
#include <cstdint>

// OpenSSL is required in this project, so the non-OpenSSL branch was removed
#include <openssl/md5.h>
#include <openssl/sha.h>

// SHA256_CTX is defined in <openssl/sha.h>

void sha256_init(SHA256_CTX *ctxt);
void sha256_update(SHA256_CTX *ctxt, const void *data, size_t data_len);
SHA256_DIGEST sha256_finish(SHA256_CTX *ctxt);

void md5_init(MD5_CTX *ctxt);
void md5_update(MD5_CTX *ctxt, const void *data, size_t data_len);
MD5_DIGEST md5_finish(MD5_CTX *ctxt);

#endif // BASE_HASH_CTXT_H
