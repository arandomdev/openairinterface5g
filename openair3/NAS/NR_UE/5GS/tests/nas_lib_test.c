#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <arpa/inet.h>
#include "RegistrationAccept.h"
#include "fgs_service_request.h"
#include "nr_nas_msg.h"

void exit_function(const char *file, const char *function, const int line, const char *s, const int assert)
{
  printf("detected error at %s:%d:%s: %s\n", file, line, function, s);
  abort();
}

/**
 * @brief Equality check for NAS Service Request enc/dec
 */
static bool eq_service_request(const fgs_service_request_msg_t *a, const fgs_service_request_msg_t *b)
{
  bool result = true;
  result &= memcmp(&a->naskeysetidentifier, &b->naskeysetidentifier, sizeof(NasKeySetIdentifier)) == 0;
  result &= a->serviceType == b->serviceType;
  result &= memcmp(&a->fiveg_s_tmsi, &b->fiveg_s_tmsi, sizeof(Stmsi5GSMobileIdentity_t)) == 0;
  return result;
}

/** @brief Test regression of NAS Registration Accept decoding
 *         decode the message received from OAI CN5G
 *         and compare with the expected one */
static void test_regression_registration_accept(void)
{
  // Registration Accept message
  registration_accept_msg orig = {
      .guti = malloc_or_fail(sizeof(*orig.guti)),
      .fgsregistrationresult.registrationresult = 0x01,
      .nas_allowed_nssai[0].sst = 0x01,
      .nas_allowed_nssai[1].sst = 0x01,
      .nas_allowed_nssai[2].sst = 0x01,
      .config_nssai[0].sst = 0x01,
      .config_nssai[1].sst = 0x01,
      .config_nssai[2].sst = 0x01,
  };

  orig.guti->guti.typeofidentity = FGS_MOBILE_IDENTITY_5G_GUTI;
  orig.guti->guti.amfpointer = 0x01;
  orig.guti->guti.amfregionid = 0x01;
  orig.guti->guti.amfsetid = 0x01;
  orig.guti->guti.mccdigit1 = 0x00;
  orig.guti->guti.mccdigit2 = 0x00;
  orig.guti->guti.mccdigit3 = 0x01;
  orig.guti->guti.mncdigit1 = 0x00;
  orig.guti->guti.mncdigit2 = 0x01;
  orig.guti->guti.mncdigit3 = 0x0F;
  orig.guti->guti.tmsi = 0xb0207806;
  orig.guti->guti.spare = 0b1111;
  orig.guti->guti.oddeven = 0;

  // Encoded Registration Accept message from OAI CN5G
  uint8_t cn5g_msg[] = {0x01, 0x01, // Registration Result
                        0x77, 0x00, 0x0B, 0xF2, 0x00, 0xF1, 0x10, // 5G-GUTI
                        0x01, 0x00, 0x41, 0xB0, 0x20, 0x78, 0x06, // 5G-GUTI
                        0x15, 0x06, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, // Allowed N-NSSAI
                        0x31, 0x06, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01}; // Configured N-NSSAI

  // Decode NAS Registration Accept
  int encoded_length;
  registration_accept_msg decoded = {0};
  AssertFatal((encoded_length = decode_registration_accept(&decoded, cn5g_msg, sizeof(cn5g_msg))) >= 0,
              "encode_fgs_service_request() failed\n");

  // Compare the raw encoded buffer with expected encoded data
  bool ret = eq_fgmm_registration_accept((const registration_accept_msg *)&orig, (const registration_accept_msg *)&decoded);
  AssertFatal(ret, "Encoding mismatch!\n");

  // Free dynamic allocated memory
  free_fgmm_registration_accept(&orig);
  free_fgmm_registration_accept(&decoded);
}

/**
 * @brief Test NAS Service Request enc/dec
 */
static void test_service_request(void)
{
  // Dummy NAS Service Request message
  uint16_t amf_set_id = 0x48;
  uint16_t amf_pointer = 0x34;
  uint32_t tmsi = 0x56789ABC;
  fgs_service_request_msg_t original_msg = {
      .naskeysetidentifier = {.tsc = NAS_KEY_SET_IDENTIFIER_NATIVE, .naskeysetidentifier = NAS_KEY_SET_IDENTIFIER_NOT_AVAILABLE},
      .serviceType = SERVICE_TYPE_DATA,
      .fiveg_s_tmsi = {.spare = 0,
                       .typeofidentity = FGS_MOBILE_IDENTITY_5GS_TMSI,
                       .amfsetid = amf_set_id,
                       .amfpointer = amf_pointer,
                       .tmsi = tmsi}};

  uint8_t expected_encoded_data[] = {0x71,
                                     0x00,
                                     0x00,
                                     0xF4,
                                     (amf_set_id >> 2) & 0xFF,
                                     (amf_set_id << 6) | (amf_pointer & 0x3F),
                                     (tmsi >> 24) & 0xFF,
                                     (tmsi >> 16) & 0xFF,
                                     (tmsi >> 8) & 0xFF,
                                     tmsi & 0xFF};
  uint16_t tmp = htons(7); // length of 5GS mobile identity IE for 5G-S-TMSI (bytes)
  memcpy(expected_encoded_data + 1, &tmp, sizeof(tmp));

  // Buffer
  uint8_t buffer[64];
  memset(buffer, 0, sizeof(buffer));

  // Encode NAS Service Request
  int encoded_length = encode_fgs_service_request(buffer, &original_msg, sizeof(buffer));
  AssertFatal(encoded_length >= 0, "encode_fgs_service_request() failed\n");

  // Compare the raw encoded buffer with expected encoded data
  AssertFatal(memcmp(buffer, expected_encoded_data, encoded_length) == 0, "Encoding mismatch!\n");

  // Decode NAS Service Request
  fgs_service_request_msg_t decoded_service_request = {0};
  int decoded_length = decode_fgs_service_request(&decoded_service_request, buffer, sizeof(buffer));
  AssertFatal(decoded_length >= 0, "decode_fgs_service_request() failed\n");

  // Compare original and decoded messages
  AssertFatal(eq_service_request(&original_msg, &decoded_service_request) == 0,
              "test_service_request() failed: original and decoded messages do not match\n");
}

int main()
{
  test_service_request();
  test_regression_registration_accept();
  return 0;
}
