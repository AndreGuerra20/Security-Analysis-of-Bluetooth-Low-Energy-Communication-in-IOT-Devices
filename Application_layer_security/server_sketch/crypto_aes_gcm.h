#pragma once
#include <stdint.h>
#include <stddef.h>

// AES-GCM Typical Sizes
#define AES_GCM_KEY_SIZE   16   // 128-bit
#define AES_GCM_IV_SIZE    12
#define AES_GCM_TAG_SIZE   16

bool aes_gcm_encrypt(
  const uint8_t* key,
  const uint8_t* iv,
  const uint8_t* plaintext,
  size_t plaintext_len,
  uint8_t* ciphertext,
  uint8_t* tag
);

bool aes_gcm_decrypt(
  const uint8_t* key,
  const uint8_t* iv,
  const uint8_t* ciphertext,
  size_t ciphertext_len,
  const uint8_t* tag,
  uint8_t* plaintext
);
