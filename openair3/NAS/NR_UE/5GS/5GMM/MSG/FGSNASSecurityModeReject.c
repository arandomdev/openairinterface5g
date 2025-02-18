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

#include <stdint.h>

#include "FGSNASSecurityModeReject.h"
#include "FGSNasCause.h"

int encode_fgs_security_mode_reject(const fgs_security_mode_reject_msg *fgs_security_mode_rej, uint8_t *buffer, uint32_t len)
{
    int encoded = 0;
    int encode_result = 0;

    /* Encode the Cause, which has no IEI */
    if((encode_result = encode_fgs_nas_cause(&fgs_security_mode_rej->cause,0 , buffer + encoded, len - encoded)) < 0) {
        return encode_result;
    } else {
        encoded += encode_result;
    }

    return encoded;
}

int decode_fgs_security_mode_reject(fgs_security_mode_reject_msg *securitymodereject, uint8_t *buffer, uint32_t len){
    int decoded = 0;

    /* Decode the Cause, which has no IEI */
    if((decoded = decode_fgs_nas_cause(&securitymodereject->cause, 0, buffer, len)) < 0) {
        return decoded;
    }

    return decoded;
}