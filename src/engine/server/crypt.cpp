#include "crypt.h"

#include <base/system.h>

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

#include <vector>

// PBKDF2_HMAC_SHA_512
void Crypt(const char *pass, const char *salt, int32_t iterations, uint32_t outputBytes, char *hexResult)
{
	std::vector<unsigned char> digest;
	digest.resize(outputBytes);
	PKCS5_PBKDF2_HMAC(pass, str_length(pass), reinterpret_cast<const unsigned char *>(salt), str_length(salt), iterations, EVP_sha512(), outputBytes, digest.data());
	for(std::size_t i = 0; i < sizeof(digest); i++)
	{
		sprintf(hexResult + (i * 2), "%02x", static_cast<unsigned int>(digest[i]));
	}
}
