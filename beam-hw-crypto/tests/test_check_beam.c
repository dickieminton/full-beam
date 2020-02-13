// Copyright 2019 The Beam Team
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.#include "misc.h"

#include <malloc.h>
#include <string.h>
#include <time.h>
#include "../beam/functions.h"
#include "../beam/inner_product.h"
#include "../beam/kernel.h"
#include "../beam/misc.h"
#include "../beam/rangeproof.h"
#include "beam_tools/base64.h"
#include "beam_tools/definitions_test.h"
#include "rand.h"
#include "../beam/sign.h"
#include "../beam/debug.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "../lib/secp256k1_primitives/field.h"
#include "../lib/secp256k1_primitives/group.h"
#include "../lib/secp256k1_primitives/scalar.h"
#pragma GCC diagnostic pop

#define BEAM_DEBUG 1
void printAsBytes(const char *name, const void *mem, size_t len) {
  uint8_t tmp[len];
  memcpy(tmp, mem, len);
  printf("const uint8_t %s[] = { ", name);
  for (size_t i = 0; i < len; i++) {
    if (i < len - 1)
      printf("0x%02x, ", tmp[i]);
    else
      printf("0x%02x };", tmp[i]);
  }
  printf("\n\n");
}

void test_key_generation(void) {
  uint8_t seed[DIGEST_LENGTH];
  phrase_to_seed(
      "edge video genuine moon vibrant hybrid forum climb history iron involve sausage",
      79U,
      seed);
  HKdf_t kdf;
  get_HKdf(0, seed, &kdf);
  key_idv_t kidv;
  key_idv_init(&kidv);
  kidv.value = 3;

  secp256k1_gej commitment;
  create_kidv_image(&kdf, &kidv, &commitment, 1);

  point_t image;
  export_gej_to_point(&commitment, &image);
  DEBUG_PRINT("Generated key X:", image.x, DIGEST_LENGTH);
  DEBUG_PRINT("Generated key Y:", ((uint8_t *)&image.y), 1);
  VERIFY_TEST(IS_EQUAL_HEX(
      "a1adc5fbecb22ee47e7136de7ab44eff072004bcee43dfc7723deb9662b2f69f",
      image.x, DIGEST_LENGTH));
  VERIFY_TEST(image.y == 0);
}

void test_range_proof_confidential(void) {
  const uint8_t asset_id[] = {0xcc, 0xb2, 0xcd, 0xc6, 0x9b, 0xb4, 0x54, 0x11,
                              0x0e, 0x82, 0x74, 0x41, 0x21, 0x3d, 0xdc, 0x87,
                              0x70, 0xe9, 0x3e, 0xa1, 0x41, 0xe1, 0xfc, 0x67,
                              0x3e, 0x01, 0x7e, 0x97, 0xea, 0xdc, 0x6b, 0x96};
  const uint8_t sk_bytes[] = {0x96, 0x6b, 0xdc, 0xea, 0x97, 0x7e, 0x01, 0x3e,
                              0x67, 0xfc, 0xe1, 0x41, 0xa1, 0x3e, 0xe9, 0x70,
                              0x87, 0xdc, 0x3d, 0x21, 0x41, 0x74, 0x82, 0x0e,
                              0x11, 0x54, 0xb4, 0x9b, 0xc6, 0xcd, 0xb2, 0xab};

  secp256k1_gej asset_tag_h_gen;
  switch_commitment(asset_id, &asset_tag_h_gen);
  uint8_t asset_first_32[32];
  memcpy(asset_first_32, &asset_tag_h_gen, 32);
  DEBUG_PRINT("asset_id", asset_first_32, 32);
  VERIFY_TEST(IS_EQUAL_HEX(
      "2febca014feb9c00a1d961037119b90126b7a00071d6ec01fc388b00a4a75202",
      asset_first_32, 64));

  rangeproof_creator_params_t crp;
  memset(crp.seed, 1, 32);
  crp.kidv.value = 23110;
  crp.kidv.id.idx = 1;
  crp.kidv.id.type = 11;
  crp.kidv.id.sub_idx = 111;

  secp256k1_scalar sk;
  secp256k1_scalar_set_b32(&sk, sk_bytes, NULL);
  rangeproof_confidential_t rp;
  beam_sha256_ctx oracle;
  beam_hash_sha256_init(&oracle);

  rangeproof_confidential_create(&rp, &sk, &crp, &oracle, &asset_tag_h_gen);

  beam_sha256_ctx rp_hash;
  uint8_t rp_digest[DIGEST_LENGTH];
  beam_hash_sha256_init(&rp_hash);
  beam_hash_sha256_update(&rp_hash, (const uint8_t *)&rp, sizeof(rp));
  beam_hash_sha256_final(&rp_hash, rp_digest);
  DEBUG_PRINT("rangeproof confidential digest", rp_digest,
              DIGEST_LENGTH);
  VERIFY_TEST(IS_EQUAL_HEX(
      "95d3d13d5c056f61461e57e13173cbfb82e2c24410d5ae72482537052c7db928",
      rp_digest, 64));
}

void test_range_proof_public(void) {
  // Range proof
  const uint8_t asset_id[] = {0xcc, 0xb2, 0xcd, 0xc6, 0x9b, 0xb4, 0x54, 0x11,
                              0x0e, 0x82, 0x74, 0x41, 0x21, 0x3d, 0xdc, 0x87,
                              0x70, 0xe9, 0x3e, 0xa1, 0x41, 0xe1, 0xfc, 0x67,
                              0x3e, 0x01, 0x7e, 0x97, 0xea, 0xdc, 0x6b, 0x96};
  const uint8_t sk_bytes[] = {0x96, 0x6b, 0xdc, 0xea, 0x97, 0x7e, 0x01, 0x3e,
                              0x67, 0xfc, 0xe1, 0x41, 0xa1, 0x3e, 0xe9, 0x70,
                              0x87, 0xdc, 0x3d, 0x21, 0x41, 0x74, 0x82, 0x0e,
                              0x11, 0x54, 0xb4, 0x9b, 0xc6, 0xcd, 0xb2, 0xab};

  secp256k1_gej asset_tag_h_gen;
  switch_commitment(asset_id, &asset_tag_h_gen);
  uint8_t asset_first_32[32];
  memcpy(asset_first_32, &asset_tag_h_gen, 32);
  DEBUG_PRINT("asset_id", asset_first_32, 32);
  VERIFY_TEST(IS_EQUAL_HEX(
      "2febca014feb9c00a1d961037119b90126b7a00071d6ec01fc388b00a4a75202",
      asset_first_32, 64));

  rangeproof_creator_params_t crp;
  memset(crp.seed, 1, 32);
  crp.kidv.value = 345000;
  crp.kidv.id.idx = 1;
  crp.kidv.id.type = 11;
  crp.kidv.id.sub_idx = 111;

  secp256k1_scalar sk;
  secp256k1_scalar_set_b32(&sk, sk_bytes, NULL);
  rangeproof_public_t rp;
  beam_sha256_ctx oracle;
  beam_hash_sha256_init(&oracle);

  rangeproof_public_create(&rp, &sk, &crp, &oracle);
  DEBUG_PRINT("checksum:", rp.recovery.checksum, 32);
  VERIFY_TEST(IS_EQUAL_HEX(
      "fb4c45f75b6bc159d0d17afd1700896c33eb3fb8b95d6c6a917dd34f2766e47d",
      rp.recovery.checksum, 64));

  uint8_t hash_value[32];
  secp256k1_gej pk;
  beam_hash_sha256_init(&oracle);
  rangeproof_public_get_msg(&rp, hash_value, &oracle);
  generator_mul_scalar(&pk, get_context()->generator.G_pts, &sk);
  VERIFY_TEST(signature_is_valid(hash_value, &rp.signature, &pk,
                                 get_context()->generator.G_pts));

  secp256k1_gej comm;
  asset_tag_commit(&asset_tag_h_gen, &sk, crp.kidv.value, &comm);
  uint8_t comm_first_32[32];
  memcpy(comm_first_32, &comm, 32);
  DEBUG_PRINT("comm", comm_first_32, 32);
  VERIFY_TEST(IS_EQUAL_HEX(
      "d5448218e78bc41b5ce49c1d2e6571183e55ff1ce2c1821c0ff0451be370971b",
      comm_first_32, 64));
}

void test_inner_product(void) {
  secp256k1_scalar dot;
  secp256k1_scalar *pA = get_pa();
  secp256k1_scalar *pB = get_pb();
  inner_product_get_dot(&dot, pA, pB);

  uint8_t dot_bytes[sizeof(secp256k1_scalar)];
  memcpy(dot_bytes, &dot, sizeof(secp256k1_scalar));
  DEBUG_PRINT("inner_product dot", dot_bytes, sizeof(secp256k1_scalar));
  VERIFY_TEST(IS_EQUAL_HEX(
      "6ff4ce5bb57f2907012b1eaf5b4b3f6ffc5a38bc0506ee25edfe621312c237de",
      dot_bytes, 64));

  inner_product_modifier_t mod;
  inner_product_modifier_init(&mod);
  mod.multiplier[1] = get_pwr_mul();

  secp256k1_gej comm;
  inner_product_t sig;
  beam_sha256_ctx oraclee;
  beam_hash_sha256_init(&oraclee);
  inner_product_create(&sig, &oraclee, &comm, &dot, pA, pB, &mod);

  uint8_t comm_first_32[32];
  memcpy(comm_first_32, &comm, 32);
  DEBUG_PRINT("comm(pAB)", comm_first_32, 32);
  VERIFY_TEST(IS_EQUAL_HEX(
      "7871671df832511da604b81cfb7de520b6bfd419c363cc1b41ab421b17e82d20",
      comm_first_32, 64));

  beam_sha256_ctx sig_hash;
  uint8_t sig_digest[DIGEST_LENGTH];
  beam_hash_sha256_init(&sig_hash);
  beam_hash_sha256_update(&sig_hash, (const uint8_t *)&sig, sizeof(sig));
  beam_hash_sha256_final(&sig_hash, sig_digest);
  DEBUG_PRINT("inner product sig digest", sig_digest, DIGEST_LENGTH);
  VERIFY_TEST(IS_EQUAL_HEX(
      "c7cdf73898af6edbda95be89e5f4a05a7da20cf5bcf71b9fbc409fffacfd273f",
      sig_digest, 64));
}

void test_common(void) {
  uint8_t seed[DIGEST_LENGTH];
  phrase_to_seed(
      "edge video genuine moon vibrant hybrid forum climb history iron involve sausage",
      79U,
      seed);
  DEBUG_PRINT("sha256 of pbkdf2 of phrase: ", seed, DIGEST_LENGTH);
  VERIFY_TEST(IS_EQUAL_HEX(
      "751b77ab415ed14573b150b66d779d429e48cd2a40c51bf6ce651ce6c38fd620", seed,
      64));

  uint8_t secret_key[DIGEST_LENGTH];
  secp256k1_scalar cofactor;
  uint8_t cofactor_data[DIGEST_LENGTH];
  seed_to_kdf(seed, DIGEST_LENGTH, secret_key, &cofactor);
  secp256k1_scalar_get_b32(cofactor_data, &cofactor);
  DEBUG_PRINT("seed_to_kdf (gen / secret_key): ", secret_key, DIGEST_LENGTH);
  VERIFY_TEST(IS_EQUAL_HEX(
      "d497d3d7dc9819a80e9035dd99d0877ebd61fd4cc7c19ee9a796c0aea6d04faf",
      secret_key, 64));
  DEBUG_PRINT("seed_to_kdf (coF): ", cofactor_data, DIGEST_LENGTH);
  VERIFY_TEST(IS_EQUAL_HEX(
      "d6265c09c4ace3d6d01cb5528149fb0d751a2d5fa69172b67ee5cc9c1a320e73",
      cofactor_data, 64));

  uint8_t id[DIGEST_LENGTH];
  generate_hash_id(123456, get_context()->key.Bbs, 0, id);
  DEBUG_PRINT("generate_hash_id: ", id, DIGEST_LENGTH);
  VERIFY_TEST(IS_EQUAL_HEX(
      "8d3a2b7de4c7757cdd8591a06db8c2d85dfec748ec598baaa5dc1ede8d171fd2", id,
      64));

  secp256k1_scalar key;
  uint8_t key_data[DIGEST_LENGTH];
  derive_key(secret_key, DIGEST_LENGTH, id, DIGEST_LENGTH, &cofactor, &key);
  secp256k1_scalar_get_b32(key_data, &key);
  DEBUG_PRINT("derive_key (res): ", key_data, DIGEST_LENGTH);
  VERIFY_TEST(IS_EQUAL_HEX(
      "1569368acd9ae88d2dd008643753312034c39c20d77ea27a5ac5091e9541d782",
      key_data, 64));

  uint8_t new_address_data[DIGEST_LENGTH];
  sk_to_pk(&key, get_context()->generator.G_pts, new_address_data);
  DEBUG_PRINT("sk_to_pk: ", new_address_data, DIGEST_LENGTH);
  VERIFY_TEST(IS_EQUAL_HEX(
      "e27ba10a67f9b95140e2c6771df5b29674118832d3a51d2b79640370575538e4",
      new_address_data, 64));

  uint8_t msg[64];
  random_buffer(msg, 64);
  DEBUG_PRINT("generated message: ", msg, 64);

  point_t nonce_point;
  uint8_t k_data[DIGEST_LENGTH];
  ecc_signature_t signature;
  signature_sign(msg, &key, get_context()->generator.G_pts, &signature);
  secp256k1_scalar_get_b32(k_data, &signature.k);
  export_gej_to_point(&signature.nonce_pub, &nonce_point);
  DEBUG_PRINT("signature_sign k: ", k_data, DIGEST_LENGTH);
  DEBUG_PRINT("signature_sign nonce_point.x: ", nonce_point.x, DIGEST_LENGTH);

  secp256k1_gej pk;
  generator_mul_scalar(&pk, get_context()->generator.G_pts, &key);
  VERIFY_TEST(signature_is_valid(msg, &signature, &pk,
                                 get_context()->generator.G_pts));  // passed
  msg[0]++;
  VERIFY_TEST(!signature_is_valid(msg, &signature, &pk,
                                  get_context()->generator.G_pts));  // failed

  uint8_t owner_key[108];
  get_owner_key(secret_key, &cofactor, (uint8_t *)"qwerty", 7, owner_key);
  char *owner_key_encoded = b64_encode(owner_key, 108);
  printf("owner_key encoded: %s\n", owner_key_encoded);
#if defined (LEDGER_SDK)
  // Ledger version has only 2048 iterations and some salt
  VERIFY_TEST(
      0 ==
      strncmp(
          "3UQNoFSs3QF2GF4MaTMv7jxIlGYnXy7r0mS/S4tTxi0MXQQD7PY2Ji21qyFX4Gdq",
          owner_key_encoded, 64));
#else
  VERIFY_TEST(
      0 ==
      strncmp(
          "mJrVrOiyjaMFCjxRsfGahBkiVzC+ymIXDv2qJdJxR4WMBY4rCJ+vTkkcCdVXw41p",
          owner_key_encoded, 64));
#endif // LEDGER_SDK
  free(owner_key_encoded);
}

void test_transaction_signature(void) {
  init_context();

  HKdf_t kdf;
  uint8_t kdf_seed[DIGEST_LENGTH];
  test_set_buffer(kdf_seed, DIGEST_LENGTH, 3);
  get_HKdf(0, kdf_seed, &kdf);

  kidv_vec_t inputs;
  kidv_vec_t outputs;

  vec_init(&inputs);
  vec_init(&outputs);

  {
    key_idv_t kidv;
    key_idv_init(&kidv);

    kidv.value = 350000;
    vec_push(&inputs, kidv);

    kidv.value = 250000;
    vec_push(&inputs, kidv);

    kidv.value = 170000;
    vec_push(&outputs, kidv);
  }

  // Set transaction data
  transaction_data_t tx_data;
  tx_data.fee = 100;
  tx_data.min_height = 25000;
  tx_data.max_height = 27500;

  test_set_buffer(tx_data.kernel_nonce.x, DIGEST_LENGTH, 3);
  tx_data.kernel_nonce.y = 1;

  test_set_buffer(tx_data.kernel_commitment.x, DIGEST_LENGTH, 3);
  tx_data.kernel_commitment.y = 1;

  tx_data.nonce_slot = 6;

  secp256k1_scalar_set_int(&tx_data.offset, 3);

  secp256k1_scalar sk_total;
  secp256k1_scalar_clear(&sk_total);
  int64_t value_transferred = 0;

  sign_transaction_part_1(&value_transferred, &sk_total, inputs.data, inputs.length, outputs.data, outputs.length,
                          &tx_data, &kdf);

  secp256k1_scalar res_sk;
  secp256k1_scalar_clear(&res_sk);

  secp256k1_scalar nonce;
  secp256k1_scalar_set_int(&nonce, 3);

  ecc_signature_t sig;
  point_import_nnz(&sig.nonce_pub, &tx_data.kernel_nonce);

  sign_transaction_part_2(&res_sk, &tx_data, &nonce, &sk_total, &sig.nonce_pub);
  verify_scalar_data(
      "HW Wallet test. Sign tx scalar: ",
      "007edf32385721084a78f1b8b8d9bc8e377aa2787be38b37e28361fdaf06780c",
      &res_sk);

  vec_deinit(&inputs);
  vec_deinit(&outputs);
}

#if defined (BEAM_CRYPTO_HW_LIB_TEST) && BEAM_CRYPTO_HW_LIB_TEST == 1
int main(void) {
#else
int test_crypto_hw_lib(void) {
#endif
  random_reseed(time(NULL));
  init_context();

  START_TEST(test_common);
  START_TEST(test_inner_product);
  START_TEST(test_range_proof_public);
  START_TEST(test_range_proof_confidential);
  START_TEST(test_key_generation);
  START_TEST(test_transaction_signature);

  malloc_stats();

  return 0;
}