/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

/*! \file FGSNASSecurityModeReject.c

\brief security mode complete reject for gNB
\author Eduard Vlad
\email: eduard.vlad@rwth-aachen.de
\date 2025
*/

#include "FGSNasCause.h"
#include "TLVEncoder.h"
#include "TLVDecoder.h"

int encode_fgs_nas_cause(const FGSNasCause *emmcause, uint8_t iei, uint8_t *buffer, uint32_t len)
{
  uint32_t encoded = 0;
  /* Checking IEI and pointer */
  CHECK_PDU_POINTER_AND_LENGTH_ENCODER(buffer, FGS_NAS_CAUSE_MINIMUM_LENGTH, len);

  /* Cause has no IE */
  if (iei > 0) {
    *buffer = iei;
    encoded++;
  }

  *(buffer + encoded) = *emmcause;
  encoded++;
  return encoded;
}

int decode_fgs_nas_cause(FGSNasCause *fgs_nas_cause, uint8_t iei, uint8_t *buffer, uint32_t len){
  int decoded = 0;

  /* Cause has no IE */
  if (iei > 0) {
    CHECK_IEI_DECODER(iei, *buffer);
    decoded++;
  }

  (*fgs_nas_cause) = *(buffer + decoded);
  decoded++;

  return decoded;
}