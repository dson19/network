#include "auth.h"

#include <string.h>

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

int auth_hash(const char *password, const uint8_t salt[SALT_LEN], int iterations,
              uint8_t out[HASH_LEN])
{
    int ok = PKCS5_PBKDF2_HMAC(password, (int)strlen(password), salt, SALT_LEN,
                               iterations, EVP_sha256(), HASH_LEN, out);
    return ok == 1 ? 0 : -1;
}

int auth_new_salt(uint8_t salt[SALT_LEN])
{
    return RAND_bytes(salt, SALT_LEN) == 1 ? 0 : -1;
}

int auth_equal(const uint8_t a[HASH_LEN], const uint8_t b[HASH_LEN])
{
    return CRYPTO_memcmp(a, b, HASH_LEN) == 0;
}

void auth_wipe(void *p, unsigned long n)
{
    OPENSSL_cleanse(p, n);
}
