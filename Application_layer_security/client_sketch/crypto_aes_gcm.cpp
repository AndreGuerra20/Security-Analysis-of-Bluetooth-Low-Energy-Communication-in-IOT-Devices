#include "crypto_aes_gcm.h"
#include "mbedtls/gcm.h"

bool aes_gcm_encrypt(
  const uint8_t* key,
  const uint8_t* iv,
  const uint8_t* plaintext,
  size_t plaintext_len,
  uint8_t* ciphertext,
  uint8_t* tag
) {
  mbedtls_gcm_context ctx;
  mbedtls_gcm_init(&ctx);

  if (mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key, 128) != 0) {
    mbedtls_gcm_free(&ctx);
    return false;
  }

  int ret = mbedtls_gcm_crypt_and_tag(
    &ctx,
    MBEDTLS_GCM_ENCRYPT,
    plaintext_len,
    iv,
    AES_GCM_IV_SIZE,
    nullptr,
    0,
    plaintext,
    ciphertext,
    AES_GCM_TAG_SIZE,
    tag
  );

  mbedtls_gcm_free(&ctx);
  return ret == 0;
}

bool aes_gcm_decrypt(
  const uint8_t* key,
  const uint8_t* iv,
  const uint8_t* ciphertext,
  size_t ciphertext_len,
  const uint8_t* tag,
  uint8_t* plaintext
) {
  mbedtls_gcm_context ctx;
  mbedtls_gcm_init(&ctx);

  if (mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key, 128) != 0) {
    mbedtls_gcm_free(&ctx);
    return false;
  }

  int ret = mbedtls_gcm_auth_decrypt(
    &ctx,
    ciphertext_len,
    iv,
    AES_GCM_IV_SIZE,
    nullptr,
    0,
    tag,
    AES_GCM_TAG_SIZE,
    ciphertext,
    plaintext
  );

  mbedtls_gcm_free(&ctx);
  return ret == 0;
}
