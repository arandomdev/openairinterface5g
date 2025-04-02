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

/*! \file FGSNASSecurityModeComplete.h

\brief security mode reject procedures for gNB
\author Eduard Vlad
\email: eduard.vlad@rwth-aachen.de
\date 2025
\version 0.1
*/

#ifndef FGS_NAS_security_mode_reject_H_
#define FGS_NAS_security_mode_reject_H_

#include <stdint.h>

#include "MessageType.h"
#include "FGSNasCause.h"

/*
 * Message name: security mode reject
 * Description: The SECURITY MODE REJECT message is sent by the UE to the AMF to
 * indicate that the corresponding security mode command has been rejected.
 * See table 8.2.27.1.1.
 *
 * Significance: dual
 * Direction: UE to AMF
 */

typedef struct {
    /* Mandatory fields */
    FGSNasCause                                cause;
} fgs_security_mode_reject_msg;

int encode_fgs_security_mode_reject(const fgs_security_mode_reject_msg *fgs_security_mode_comp, uint8_t *buffer, uint32_t len);
int decode_fgs_security_mode_reject(fgs_security_mode_reject_msg *securitymodereject, uint8_t *buffer, uint32_t len);

#endif /* ! defined(FGS_NAS_security_mode_reject_H_) */