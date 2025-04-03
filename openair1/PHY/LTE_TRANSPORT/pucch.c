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

/*! \file PHY/LTE_TRANSPORT/pucch.c
* \brief Top-level routines for generating and decoding the PUCCH physical channel V8.6 2009-03
* \author R. Knopp
* \date 2011
* \version 0.1
* \company Eurecom
* \email: knopp@eurecom.fr
* \note
* \warning
*/
#include "PHY/defs_eNB.h"
#include "PHY/phy_extern.h"
#include "LAYER2/MAC/mac.h"
#include "PHY/LTE_REFSIG/lte_refsig.h"

#include "common/utils/LOG/log.h"
#include "common/utils/LOG/vcd_signal_dumper.h"
#include "executables/softmodem-common.h"

//uint8_t ncs_cell[20][7];
//#define DEBUG_PUCCH_TXS
//#define DEBUG_PUCCH_RX

#include "pucch_extern.h"

static const int16_t cfo_pucch_np[24 * 7] = {
    20787,  -25330, 27244, -18205, 31356, -9512, 32767, 0,     31356, 9511,  27244,  18204, 20787, 25329,  27244, -18205, 30272,
    -12540, 32137,  -6393, 32767,  0,     32137, 6392,  30272, 12539, 27244, 18204,  31356, -9512, 32137,  -6393, 32609,  -3212,
    32767,  0,      32609, 3211,   32137, 6392,  31356, 9511,  32767, 0,     32767,  0,     32767, 0,      32767, 0,      32767,
    0,      32767,  0,     32767,  0,     31356, 9511,  32137, 6392,  32609, 3211,   32767, 0,     32609,  -3212, 32137,  -6393,
    31356,  -9512,  27244, 18204,  30272, 12539, 32137, 6392,  32767, 0,     32137,  -6393, 30272, -12540, 27244, -18205, 20787,
    25329,  27244,  18204, 31356,  9511,  32767, 0,     31356, -9512, 27244, -18205, 20787, -25330};

static const int16_t cfo_pucch_ep[24 * 6] = {
    24278, -22005, 29621, -14010, 32412, -4808, 32412, 4807,  29621, 14009, 24278, 22004, 28897, -15447, 31356, -9512, 32609,
    -3212, 32609,  3211,  31356,  9511,  28897, 15446, 31785, -7962, 32412, -4808, 32727, -1608, 32727,  1607,  32412, 4807,
    31785, 7961,   32767, 0,      32767, 0,     32767, 0,     32767, 0,     32767, 0,     32767, 0,      31785, 7961,  32412,
    4807,  32727,  1607,  32727,  -1608, 32412, -4808, 31785, -7962, 28897, 15446, 31356, 9511,  32609,  3211,  32609, -3212,
    31356, -9512,  28897, -15447, 24278, 22004, 29621, 14009, 32412, 4807,  32412, -4808, 29621, -14010, 24278, -22005};

void dump_uci_stats(FILE *fd,PHY_VARS_eNB *eNB,int frame) {

  int strpos=0;
  char output[16384];

  for (int i=0;i<NUMBER_OF_SCH_STATS_MAX;i++){
    if (eNB->uci_stats[i].rnti>0) {
      eNB_UCI_STATS_t *uci_stats = &eNB->uci_stats[i];
      strpos+=sprintf(output+strpos,"UCI %d RNTI %x: pucch1_trials %d, pucch1_n0 %d dB, pucch1_thres %d dB, current pucch1_stat_pos %d dB, current pucch1_stat_neg %d dB, positive SR count %d\n",
	i,uci_stats->rnti,uci_stats->pucch1_trials,eNB->measurements.n0_pucch_dB/*max(eNB->measurements.n0_subband_power_tot_dB[0], eNB->measurements.n0_subband_power_tot_dB[eNB->frame_parms.N_RB_UL-1])*/,uci_stats->pucch1_thres,dB_fixed(uci_stats->current_pucch1_stat_pos),dB_fixed(uci_stats->current_pucch1_stat_neg),uci_stats->pucch1_positive_SR);
      strpos+=sprintf(output+strpos,"UCI %d RNTI %x: pucch1_low (%d,%d)dB pucch1_high (%d,%d)dB\n",
	    i,uci_stats->rnti,
            dB_fixed(uci_stats->pucch1_low_stat[0]),
            dB_fixed(uci_stats->pucch1_low_stat[1]),
            dB_fixed(uci_stats->pucch1_high_stat[0]),
            dB_fixed(uci_stats->pucch1_high_stat[1]));
      
      strpos+=sprintf(output+strpos,"UCI %d RNTI %x: pucch1a_trials %d, pucch1a_stat (%d,%d), pucch1b_trials %d, pucch1b_stat (%d,%d) pucch1ab_DTX %d\n",
            i,uci_stats->rnti,
            uci_stats->pucch1a_trials,
            uci_stats->current_pucch1a_stat_re,
            uci_stats->current_pucch1a_stat_im,
            uci_stats->pucch1b_trials,
	    uci_stats->current_pucch1b_stat_re,
	    uci_stats->current_pucch1b_stat_im,
            uci_stats->pucch1ab_DTX);
    }
  }
  if (fd) fprintf(fd,"%s",output);
  else    printf("%s",output);  
}

/* PUCCH format3 >> */
/* SubCarrier Demap */
static unsigned int pucchfmt3_subCarrierDeMapping(PHY_VARS_eNB *eNB, c16_t SubCarrierDeMapData[4][14][12], unsigned int n3_pucch)
{
  LTE_eNB_COMMON *eNB_common_vars  = &eNB->common_vars;
  LTE_DL_FRAME_PARMS *frame_parms = &eNB->frame_parms;
  const unsigned int N_UL_symb = D_NSYM1SLT; // only Normal CP format
  const unsigned int m = n3_pucch / D_NPUCCH_SF5;

  // Do detection
  for (unsigned int aa = 0; aa < frame_parms->nb_antennas_rx; aa++) {
    for (unsigned int l = 0; l < D_NSYM1SF; l++) {
      unsigned int carrier_offset;
      if ((l<N_UL_symb) && ((m&1) == 0))
        carrier_offset = (m*6) + frame_parms->first_carrier_offset;
      else if ((l<N_UL_symb) && ((m&1) == 1))
        carrier_offset = frame_parms->first_carrier_offset + (frame_parms->N_RB_DL - (m>>1) - 1)*12;
      else if ((m&1) == 0)
        carrier_offset = frame_parms->first_carrier_offset + (frame_parms->N_RB_DL - (m>>1) - 1)*12;
      else
        carrier_offset = (((m-1)*6) + frame_parms->first_carrier_offset);

      if (carrier_offset > frame_parms->ofdm_symbol_size)
        carrier_offset -= (frame_parms->ofdm_symbol_size);

      unsigned int symbol_offset = (unsigned int)frame_parms->ofdm_symbol_size * l;
      c16_t *rxptr = (c16_t *)&eNB_common_vars->rxdataF[aa][symbol_offset];

      for (unsigned int k = 0; k < 12; k++, carrier_offset++) {
        SubCarrierDeMapData[aa][l][k] = rxptr[carrier_offset];
        if (carrier_offset==frame_parms->ofdm_symbol_size)
          carrier_offset = 0;
      }
    }
  }

  return 0;
}

/* cyclic shift hopping remove */
static unsigned int pucchfmt3_Baseseq_csh_remove(c16_t SubCarrierDeMapData[4][14][12],
                                                 c16_t CshData_fmt3[4][14][12],
                                                 LTE_DL_FRAME_PARMS *frame_parms,
                                                 unsigned int subframe,
                                                 uint8_t ncs_cell[20][7])
{
  int32_t     NSym1slot       = D_NSYM1SLT; // Symbol per 1slot
  int32_t     NSym1subframe   = D_NSYM1SF;  // Symbol per 1subframe

  for (unsigned int aa = 0; aa < frame_parms->nb_antennas_rx; aa++) { // Antenna
    for (unsigned int symNo = 0; symNo < NSym1subframe; symNo++) { // Symbol
      unsigned int slotNo = symNo / NSym1slot;
      unsigned int sym = symNo % NSym1slot;
      int32_t n_cell_cs_div64 = (int32_t)(ncs_cell[2 * subframe + slotNo][sym] / 64.0);
      int32_t n_cell_cs_modNSC_RB = ncs_cell[2 * subframe + slotNo][sym] % 12;
      // for canceling e^(j*PI|_n_cs^cell(ns,l)/64_|/2).
      c16_t calctmp_beta = RotTBL[(n_cell_cs_div64) & 0x3];

      for (unsigned int k = 0; k < 12; k++) { // Sub Carrier
        // for canceling being cyclically shifted"(i+n_cs^cell(ns,l))".
        // e^((j*2PI(n_cs^cell(ns,l) mod N_SC)/N_SC)*k).
        c16_t calctmp_alphak = alphaTBL[(n_cell_cs_modNSC_RB * k) % 12];
        // e^(-alphar*k)*r_l,m,n,k
        c16_t calctmp_SCDeMapData_alphak = c16MulConjShift(SubCarrierDeMapData[aa][symNo][k], calctmp_alphak, 15);
        // (e^(-alphar*k)*r_l,m,n,k) * e^(-beta)
        CshData_fmt3[aa][symNo][k] = c16MulConjShift(calctmp_SCDeMapData_alphak, calctmp_beta, 15);
      }
    }
  }

  return 0;
}

#define MAXROW_TBL_SF5_OS_IDX    (5)    // Orthogonal sequence index
static const int16_t TBL_3_SF5_GEN_N_DASH_NS[MAXROW_TBL_SF5_OS_IDX] = {0, 3, 6, 8, 10};

#define MAXROW_TBL_SF4_OS_IDX    (4)    // Orthogonal sequence index
static const int16_t TBL_3_SF4_GEN_N_DASH_NS[MAXROW_TBL_SF4_OS_IDX] = {0, 3, 6, 9};

/* Channel estimation */
static unsigned int pucchfmt3_ChannelEstimation(c16_t SubCarrierDeMapData[4][14][12],
                                                double delta_theta[4][12],
                                                c16_t ChestValue[4][2][12],
                                                int16_t *Interpw,
                                                unsigned int subframe,
                                                unsigned int shortened_format,
                                                LTE_DL_FRAME_PARMS *frame_parms,
                                                unsigned int n3_pucch,
                                                uint16_t n3_pucch_array[NUMBER_OF_UE_MAX],
                                                uint8_t ncs_cell[20][7])
{
  int16_t np, np_n;
  //int16_t         npucch_sf;
  c16_t BsCshData[4][D_NSYM1SF][D_NSC1RB];
  c16_t CsData_temp[4][D_NSYM1SF][D_NSC1RB];
  c32_t IP_CsData_allsfavg[4][14][4];
  int16_t m[NUMBER_OF_UE_MAX], m_self = 0;
  uint16_t        n3_pucch_sameRB[NUMBER_OF_UE_MAX];
  int16_t         n_oc0[NUMBER_OF_UE_MAX];
  int16_t         n_oc1[NUMBER_OF_UE_MAX];
  int16_t         np_n_array[2][NUMBER_OF_UE_MAX]; //Cyclic shift
  unsigned int N_PUCCH_SF0 = 5;
  unsigned int N_PUCCH_SF1 = (shortened_format == 0) ? 5 : 4;
  uint32_t u0 = (frame_parms->Nid_cell + frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.grouphop[subframe<<1]) % 30;
  uint32_t u1 = (frame_parms->Nid_cell + frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.grouphop[1+(subframe<<1)]) % 30;
  uint32_t v0=frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.seqhop[subframe<<1];
  uint32_t v1=frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.seqhop[1+(subframe<<1)];
  uint32_t u=u0;
  uint32_t v=v0;

  //double d_theta[32]={0.0};
  //int32_t temp_theta[32][2]={0};

  for (unsigned int aa = 0; aa < frame_parms->nb_antennas_rx; aa++) {
    for (unsigned int symNo = 0; symNo < D_NSYM1SF; symNo++) {
      for (unsigned int ip_ind = 0; ip_ind < D_NPUCCH_SF5 - 1; ip_ind++) {
        IP_CsData_allsfavg[aa][symNo][ip_ind] = (c32_t){0};
      }
    }
  }

  // compute m[], m_self
  for (unsigned int i = 0; i < NUMBER_OF_UE_MAX; i++) {
    m[i] = n3_pucch_array[i] / N_PUCCH_SF0; // N_PUCCH_SF0 = 5

    if(n3_pucch_array[i] == n3_pucch) {
      m_self = i;
    }
  }

  // compute n3_pucch_sameRB[] // Not 4 not be equally divided
  unsigned int same_m_number = 0;
  for (unsigned int i = 0; i < NUMBER_OF_UE_MAX; i++) {
    if(m[i] == m[m_self]) {
      n3_pucch_sameRB[same_m_number] = n3_pucch_array[i];
      same_m_number++;
    }
  }

  // compute n_oc1[], n_oc0[]
  for (unsigned int i = 0; i < same_m_number; i++) {
    n_oc0[i] = n3_pucch_sameRB[i] % N_PUCCH_SF1; //N_PUCCH_SF1 = (shortened_format==0)? 5:4;

    if (N_PUCCH_SF1 == 5) {
      n_oc1[i] = (3 * n_oc0[i]) % N_PUCCH_SF1;
    } else {
      n_oc1[i] = n_oc0[i] % N_PUCCH_SF1;
    }
  }

  // np_n_array[][]
  for (unsigned int i = 0; i < same_m_number; i++) {
    if (N_PUCCH_SF1 == 5) {
      np_n_array[0][i] = TBL_3_SF5_GEN_N_DASH_NS[n_oc0[i]]; //slot0
      np_n_array[1][i] = TBL_3_SF5_GEN_N_DASH_NS[n_oc1[i]]; //slot1
    } else {
      np_n_array[0][i] = TBL_3_SF4_GEN_N_DASH_NS[n_oc0[i]];
      np_n_array[1][i] = TBL_3_SF4_GEN_N_DASH_NS[n_oc1[i]];
    }
  }
  unsigned int ip_ind = 0;
  for (unsigned int aa = 0; aa < frame_parms->nb_antennas_rx; aa++) {
    for (unsigned int symNo = 0; symNo < D_NSYM1SF; symNo++) { // #define D_NSYM1SF       2*7
      unsigned int slotNo = symNo / D_NSYM1SLT;
      unsigned int sym = symNo % D_NSYM1SLT;

      for (unsigned int k = 0; k < D_NSC1RB; k++) { // #define D_NSC1RB        12
        // remove Base Sequence (c_r^*)*(r_l,m,m,n,k) = BsCshData
        BsCshData[aa][symNo][k] = c16MulConjShift(SubCarrierDeMapData[aa][symNo][k], ul_ref_sigs[u][v][0][k], 15);
        if(shortened_format == 1) {
          if (symNo < D_NSYM1SLT) {
            np = n3_pucch % D_NPUCCH_SF4;   // np = n_oc
            np_n = TBL_3_SF4_GEN_N_DASH_NS[np]; //
          } else {
            np = n3_pucch % D_NPUCCH_SF4;   //
            np_n = TBL_3_SF4_GEN_N_DASH_NS[np]; //
          }

          //npucch_sf = D_NPUCCH_SF4;// = 4
        } else {
          if (symNo < D_NSYM1SLT) {
            np = n3_pucch % D_NPUCCH_SF5;
            np_n = TBL_3_SF5_GEN_N_DASH_NS[np];
          } else {
            np = (3 * n3_pucch) % D_NPUCCH_SF5;
            np_n = TBL_3_SF5_GEN_N_DASH_NS[np];
          }

          //npucch_sf = D_NPUCCH_SF5;// = 5
        }

        // cyclic shift e^(-j * beta_n * k)
        c16_t calctmp = alphaTBL[(((ncs_cell[2 * subframe + slotNo][sym] + np_n) % D_NSC1RB) * k) % 12];
        // Channel Estimation 1A, g'(n_cs)_l,m,n
        // CsData_temp = g_l,m,n,k
        // remove cyclic shift BsCshData * e^(-j * beta_n * k)
        c16_t tmp = c16MulConjShift(BsCshData[aa][symNo][k], calctmp, 15);
        CsData_temp[aa][symNo][k] = (c16_t){tmp.r / D_NSC1RB, tmp.i / D_NSC1RB};
        // Interference power for Channel Estimation 1A, No use Cyclic Shift g'(n_cs)_l,m,n
        // Calculated by the cyclic shift that is not used  S(ncs)_est
        ip_ind = 0;

        for (unsigned int i = 0; i < N_PUCCH_SF1; i++) {
          for (unsigned int j = 0; j < same_m_number; j++) { // np_n_array Loop
            if(shortened_format == 1) {
              if(symNo < D_NSYM1SLT) { // if SF==1 slot0
                if(TBL_3_SF4_GEN_N_DASH_NS[i] == np_n_array[0][j]) {
                  break;
                }
              } else { // if SF==1 slot1
                if(TBL_3_SF4_GEN_N_DASH_NS[i] == np_n_array[1][j]) {
                  break;
                }
              }
            } else {
              if(symNo < D_NSYM1SLT) { // if SF==0 slot0
                if(TBL_3_SF5_GEN_N_DASH_NS[i] == np_n_array[0][j]) {
                  break;
                }
              } else { // if SF==0 slot1
                if(TBL_3_SF5_GEN_N_DASH_NS[i] == np_n_array[1][j]) {
                  break;
                }
              }
            }

            if(j == same_m_number - 1) { //when even once it has not been used
              c16_t calctmp;
              if(shortened_format == 1) {
                calctmp = alphaTBL[(((ncs_cell[2 * subframe + slotNo][sym] + TBL_3_SF4_GEN_N_DASH_NS[i]) % D_NSC1RB) * k) % 12];
              } else {
                calctmp = alphaTBL[(((ncs_cell[2 * subframe + slotNo][sym] + TBL_3_SF5_GEN_N_DASH_NS[i]) % D_NSC1RB) * k) % 12];
              }

              // IP_CsData_allsfavg = g'(n_cs)_l,m,n
              c16_t tmp = c16MulConjShift(BsCshData[aa][symNo][k], calctmp, 15);

              IP_CsData_allsfavg[aa][symNo][ip_ind].r += tmp.r;
              IP_CsData_allsfavg[aa][symNo][ip_ind].i += tmp.i;

              if((symNo == 1 || symNo == 5 || symNo == 8 || symNo == 12)) {
              }

              ip_ind++;
            }
          }
        }
      }

      if(symNo > D_NSYM1SLT-1) {
        u=u1;
        v=v1;
      }
    }
  }
  cd_t CsData_allavg[4][14] = {};
  for (unsigned int aa = 0; aa < frame_parms->nb_antennas_rx; aa++) {
    for (unsigned int symNo = 0; symNo < D_NSYM1SF; symNo++) {
      for (unsigned int k = 0; k < D_NSC1RB; k++) {
        CsData_allavg[aa][symNo].r += CsData_temp[aa][symNo][k].r;
        CsData_allavg[aa][symNo].i += CsData_temp[aa][symNo][k].i;
      }
    }
  }

  // Frequency deviation estimation
  for (unsigned int aa = 0; aa < frame_parms->nb_antennas_rx; aa++) {
    for (unsigned int k = 0; k < 12; k++) {
      c16_t tmp1 = c16MulConjShift(CsData_temp[aa][1][k], CsData_temp[aa][5][k], 8);
      c16_t tmp2 = c16MulConjShift(CsData_temp[aa][8][k], CsData_temp[aa][12][k], 8);
      delta_theta[aa][k] = atan2(tmp1.i + tmp2.i, tmp1.r + tmp2.r) / 4.0;
    }
  }

  for (unsigned int aa = 0; aa < frame_parms->nb_antennas_rx; aa++) {
    for (unsigned int k = 0; k < D_NSC1RB; k++) {
      cd_t dt = {cos(delta_theta[aa][k] * 4), sin(delta_theta[aa][k] * 4)};
      cd_t tmp = cdMulConj(CsData_allavg[aa][5], dt);
      ChestValue[aa][0][k].r = (CsData_allavg[aa][1].r + tmp.r) / (2 * D_NSC1RB);
      ChestValue[aa][0][k].i = (CsData_allavg[aa][1].i + tmp.i) / (2 * D_NSC1RB);
      cd_t tmp2 = cdMulConj(CsData_allavg[aa][12], dt);
      ChestValue[aa][1][k].r = (CsData_allavg[aa][8].r + tmp2.r) / (2 * D_NSC1RB);
      ChestValue[aa][1][k].i = (CsData_allavg[aa][8].i + tmp2.i) / (2 * D_NSC1RB);
    }
  }

  *Interpw = 0;

  for (unsigned int aa = 0; aa < frame_parms->nb_antennas_rx; aa++) {
    if(ip_ind == 0) {//ip_ind= The total number of cyclic shift of non-use
      *Interpw = 1;
      break;
    }

    for (unsigned int i = 0; i < ip_ind; i++) {
      int64_t IP_allavg = squaredMod(IP_CsData_allsfavg[aa][1][i]) >> 8;
      IP_allavg += squaredMod(IP_CsData_allsfavg[aa][5][i]) >> 8;
      IP_allavg += squaredMod(IP_CsData_allsfavg[aa][8][i]) >> 8;
      IP_allavg += squaredMod(IP_CsData_allsfavg[aa][12][i]) >> 8;
      *Interpw += IP_allavg / (2 * D_NSLT1SF * frame_parms->nb_antennas_rx * ip_ind * 12);
    }
  }

  return 0;
}

/* Channel Equalization */
static unsigned int pucchfmt3_Equalization(c16_t CshData_fmt3[4][14][12],
                                           c16_t ChdetAfterValue_fmt3[4][14][12],
                                           c16_t ChestValue[4][2][12],
                                           LTE_DL_FRAME_PARMS *frame_parms)
{
  for (unsigned int aa = 0; aa < frame_parms->nb_antennas_rx; aa++) {
    unsigned int sltNo = 0;

    for (unsigned int symNo = 0; symNo < D_NSYM1SF; symNo++) {
      if(symNo >= D_NSYM1SLT) {
        sltNo = 1;
      }

      for (unsigned int k = 0; k < D_NSC1RB; k++) {
        ChdetAfterValue_fmt3[aa][symNo][k] = c16MulConjShift(CshData_fmt3[aa][symNo][k], ChestValue[aa][sltNo][k], 8);
      }
    }
  }

  return 0;
}

/* Frequency deviation remove AFC */
static unsigned int pucchfmt3_FrqDevRemove(c16_t ChdetAfterValue_fmt3[4][14][12],
                                           double delta_theta[4][12],
                                           c16_t RemoveFrqDev_fmt3[4][2][5][12],
                                           LTE_DL_FRAME_PARMS *frame_parms)
{
  for (unsigned int aa = 0; aa < frame_parms->nb_antennas_rx; aa++) {
    for (unsigned int sltNo = 0; sltNo < D_NSLT1SF; sltNo++) {
      unsigned int n = 0;

      for (unsigned int symNo1slt = 0; symNo1slt < D_NSYM1SLT; symNo1slt++) {
        if(!((symNo1slt==1) || (symNo1slt==5))) {
          for (unsigned int k = 0; k < D_NSC1RB; k++) {
            cd_t calctmp = {cos(delta_theta[aa][k] * (n - 1)), sin(delta_theta[aa][k] * (n - 1))};
            cd_t tmp = {ChdetAfterValue_fmt3[aa][(sltNo * D_NSYM1SLT) + symNo1slt][k].r,
                        ChdetAfterValue_fmt3[aa][(sltNo * D_NSYM1SLT) + symNo1slt][k].i};
            cd_t tmp2 = cdMulConj(tmp, calctmp);
            RemoveFrqDev_fmt3[aa][sltNo][n][k] = (c16_t){tmp2.r, tmp2.i};
          }
          n++;
        }
      }
    }
  }

  return 0;
}

//for opt.Lev.2
#define  MAXROW_TBL_SF5  5
#define  MAXCLM_TBL_SF5  5
static const c16_t TBL_3_SF5[MAXROW_TBL_SF5][MAXCLM_TBL_SF5] = {
    {{32767, 0}, {32767, 0}, {32767, 0}, {32767, 0}, {32767, 0}},
    {{32767, 0}, {10126, 31163}, {-26509, 19260}, {-26509, -19260}, {10126, -31163}},
    {{32767, 0}, {-26509, 19260}, {10126, -31163}, {10126, 31163}, {-26509, -19260}},
    {{32767, 0}, {-26509, -19260}, {10126, 31163}, {10126, -31163}, {-26509, 19260}},
    {{32767, 0}, {10126, -31163}, {-26509, -19260}, {-26509, 19260}, {10126, 31163}}};

#define  MAXROW_TBL_SF4_fmt3 4
#define  MAXCLM_TBL_SF4      4
static const c16_t TBL_3_SF4[MAXROW_TBL_SF4_fmt3][MAXCLM_TBL_SF4] = {{{32767, 0}, {32767, 0}, {32767, 0}, {32767, 0}},
                                                                     {{32767, 0}, {-32767, 0}, {32767, 0}, {-32767, 0}},
                                                                     {{32767, 0}, {32767, 0}, {-32767, 0}, {-32767, 0}},
                                                                     {{32767, 0}, {-32767, 0}, {-32767, 0}, {32767, 0}}};

/* orthogonal sequence remove */
static unsigned int pucchfmt3_OrthSeqRemove(c16_t RemoveFrqDev_fmt3[4][2][5][12],
                                            c16_t Fmt3xDataRmvOrth[4][2][5][12],
                                            unsigned int shortened_format,
                                            unsigned int n3_pucch,
                                            LTE_DL_FRAME_PARMS *frame_parms)
{
  for (unsigned int aa = 0; aa < frame_parms->nb_antennas_rx; aa++) {
    for (unsigned int sltNo = 0; sltNo < D_NSLT1SF; sltNo++) {
      unsigned int noc, Npucch_sf;
      if(shortened_format == 1) {
        if(sltNo == 0) {
          noc = n3_pucch % D_NPUCCH_SF4;
          Npucch_sf = D_NPUCCH_SF5;
        } else {
          noc = n3_pucch % D_NPUCCH_SF4;
          Npucch_sf = D_NPUCCH_SF4;
        }
      } else {
        if(sltNo == 0) {
          noc = n3_pucch % D_NPUCCH_SF5;
          Npucch_sf = D_NPUCCH_SF5;
        } else {
          noc = (3 * n3_pucch) % D_NPUCCH_SF5;
          Npucch_sf = D_NPUCCH_SF5;
        }
      }

      for (unsigned int n = 0; n < Npucch_sf; n++) {
        for (unsigned int k = 0; k < D_NSC1RB; k++) {
          if ((sltNo == 1) && (shortened_format == 1)) {
            Fmt3xDataRmvOrth[aa][sltNo][n][k] = c16MulConjShift(RemoveFrqDev_fmt3[aa][sltNo][n][k], TBL_3_SF4[noc][n], 15);
          } else {
            Fmt3xDataRmvOrth[aa][sltNo][n][k] = c16MulConjShift(RemoveFrqDev_fmt3[aa][sltNo][n][k], TBL_3_SF5[noc][n], 15);
          }
        }
      }
    }
  }

  return 0;
}

/* averaging antenna */
static unsigned int pucchfmt3_AvgAnt(c16_t Fmt3xDataRmvOrth[4][2][5][12],
                                     c16_t Fmt3xDataAvgAnt[2][5][12],
                                     unsigned int shortened_format,
                                     LTE_DL_FRAME_PARMS *frame_parms)
{
  for (unsigned int sltNo = 0; sltNo < D_NSLT1SF; sltNo++) {
    unsigned int Npucch_sf;
    if((sltNo == 1) && (shortened_format == 1)) {
      Npucch_sf = D_NPUCCH_SF4;
    } else {
      Npucch_sf = D_NPUCCH_SF5;
    }

    for (unsigned int n = 0; n < Npucch_sf; n++) {
      for (unsigned int k = 0; k < D_NSC1RB; k++) {
        Fmt3xDataAvgAnt[sltNo][n][k] = (c16_t){0};
        for (unsigned int aa = 0; aa < frame_parms->nb_antennas_rx; aa++) {
          Fmt3xDataAvgAnt[sltNo][n][k].r += Fmt3xDataRmvOrth[aa][sltNo][n][k].r / frame_parms->nb_antennas_rx;
          Fmt3xDataAvgAnt[sltNo][n][k].i += Fmt3xDataRmvOrth[aa][sltNo][n][k].i / frame_parms->nb_antennas_rx;
        }
      }
    }
  }

  return 0;
}

/* averaging symbol */
static unsigned int pucchfmt3_AvgSym(c16_t Fmt3xDataAvgAnt[2][5][12], c16_t Fmt3xDataAvgSym[2][12], unsigned int shortened_format)
{
  for (unsigned int sltNo = 0; sltNo < D_NSLT1SF; sltNo++) {
    unsigned int Npucch_sf;
    if((sltNo == 1) && (shortened_format == 1)) {
      Npucch_sf = D_NPUCCH_SF4;
    } else {
      Npucch_sf = D_NPUCCH_SF5;
    }

    for (unsigned int k = 0; k < D_NSC1RB; k++) {
      Fmt3xDataAvgSym[sltNo][k] = (c16_t){0};

      for (unsigned int n = 0; n < Npucch_sf; n++) {
        Fmt3xDataAvgSym[sltNo][k].r += Fmt3xDataAvgAnt[sltNo][n][k].r / Npucch_sf;
        Fmt3xDataAvgSym[sltNo][k].i += Fmt3xDataAvgAnt[sltNo][n][k].i / Npucch_sf;
      }
    }
  }

  return 0;
}

/* iDFT */
static void pucchfmt3_IDft2(c16_t *x, c16_t *y)
{
  for (int k = 0; k < D_NSC1RB; k++) {
    c32_t calctmp = {0};
    for (int i = 0; i < D_NSC1RB; i++) {
      c16_t tmp = c16mulShift(x[i], alphaTBL[((i * k) % 12)], 15);
      calctmp.r += tmp.r;
      calctmp.i += tmp.i;
    }

    y[k].r = (int16_t)(calctmp.r / sqrt(D_NSC1RB));
    y[k].i = (int16_t)(calctmp.i / sqrt(D_NSC1RB));
  }
}

/* descramble */
static unsigned int pucchfmt3_Descramble(c16_t IFFTOutData_Fmt3[2][12],
                                         int16_t b[48],
                                         unsigned int subframe,
                                         uint32_t Nid_cell,
                                         uint32_t rnti)
{
  uint32_t x1;
  uint32_t cinit = (subframe + 1) * ((2 * Nid_cell + 1) << 16) + rnti;
  uint32_t s0 = lte_gold_generic(&x1, &cinit, 1);
  uint32_t s1 = lte_gold_generic(&x1, &cinit, 0);
  int16_t i = 0;

  for (unsigned int m = 0; m < D_NSLT1SF; m++) {
    for (unsigned int k = 0; k < D_NSC1RB; k++) {
      uint32_t s = (i < 32) ? s0 : s1;
      int16_t j = (i < 32) ? i : (i - 32);
      int16_t c = ((s >> j) & 1);
      b[i] = (IFFTOutData_Fmt3[m][k].r * (1 - 2 * c));
      i++;
      s = (i<32)? s0:s1;
      j = (i<32)? i:(i-32);
      c=((s>>j)&1);
      b[i] = (IFFTOutData_Fmt3[m][k].i * (1 - 2 * c));
      i++;
    }
  }
  return 0;
}

static int16_t pucchfmt3_Decode(int16_t b[48], unsigned int subframe, int16_t DTXthreshold, int16_t Interpw, unsigned int do_sr)
{
  /* Is payload 6bit or 7bit? */
  int16_t bit_pattern;
  if( do_sr == 1 ) {
    bit_pattern = 128;
  } else {
    bit_pattern = 64;
  }

  int32_t Rho_tmp = 0;
  int16_t c = 0;
  for (unsigned int i = 0; i < 48; i++) {
    Rho_tmp += b[i] * (1-2*chcod_tbl[c][i]);
  }

  int16_t c_max = c;
  int32_t Rho_max = Rho_tmp;

  for(c=1; c<bit_pattern; c++) {
    Rho_tmp = 0;

    for (unsigned int i = 0; i < 48; i++) {
      Rho_tmp += b[i] * (1-2*chcod_tbl[c][i]);
    }

    if (Rho_tmp > Rho_max) {
      c_max = c;
      Rho_max = Rho_tmp;
    }
  }

  if(Interpw<1) {
    Interpw=1;
  }

  if((Rho_max/Interpw) > DTXthreshold) {
    // ***Log
    return c_max;
  } else {
    // ***Log
    return -1;
  }
}

uint32_t calc_pucch_1x_interference(PHY_VARS_eNB *eNB, int frame, unsigned int subframe, unsigned int shortened_format)
//-----------------------------------------------------------------------------
{
  LTE_eNB_COMMON *common_vars = &eNB->common_vars;
  LTE_DL_FRAME_PARMS *frame_parms = &eNB->frame_parms;
  c16_t z[12 * 14] = {0};

  const int16_t W4_nouse[4] = {32767, 32767, -32768, -32768};

  const uint32_t u0 = (frame_parms->pucch_config_common.grouphop[subframe << 1]) % 30;
  const uint32_t u1 = (frame_parms->pucch_config_common.grouphop[1 + (subframe << 1)]) % 30;
  const uint32_t v0 = frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.seqhop[subframe << 1];
  const uint32_t v1 = frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.seqhop[1 + (subframe << 1)];

  const unsigned int N_UL_symb = (frame_parms->Ncp == NORMAL) ? 7 : 6;

  double interference_power = 0.0;
  int calc_cnt = 0;

  // loop over 2 slots
  for (unsigned int n_cs_base = 0; n_cs_base < 12; n_cs_base++) {
    c16_t *zptr = z;
    for (unsigned int ns = (subframe << 1), u = u0, v = v0; ns < (2 + (subframe << 1)); ns++, u = u1, v = v1) {
      //loop over symbols in slot
      for (unsigned int l = 0; l < N_UL_symb; l++) {
        unsigned int n_cs = eNB->ncs_cell[ns][l] + n_cs_base;
        if(((l>1)&&(l<N_UL_symb-2)) || ((ns==(1+(subframe<<1))) && (shortened_format==1)) ){
          zptr += 12;
          continue;
        }
        c16_t W = {};
        if (l<2) {                                         // data
          W = (c16_t){W4_nouse[l], 0};
        } else if ((l>=N_UL_symb-2)) {                     // data
          W = (c16_t){W4_nouse[l - N_UL_symb + 4], 0};
        }

        unsigned int alpha_ind = 0;
        // compute output sequence

        for (unsigned int n = 0; n < 12; n++) {
          // this is r_uv^alpha(n)
          c16_t tmp = c16mulShift(alphaTBL[alpha_ind], ul_ref_sigs[u][v][0][n], 15);
          // this is S(ns)*w_noc(m)*r_uv^alpha(n)
          zptr[n] = c16mulShift(tmp, W, 15);
          alpha_ind = (alpha_ind + n_cs)%12;
        } // n

        zptr += 12;
      } // l
    } // ns

    const unsigned int m = 1;

    unsigned int nsymb = N_UL_symb << 1;

    zptr = z;

    // Do detection
    for (unsigned int aa = 0; aa < frame_parms->nb_antennas_rx; aa++) {
      c32_t n0_IQ = {0};
      for (int j = 0, l = 0; l < nsymb; l++) {
        if((((l%N_UL_symb)>1)&&((l%N_UL_symb)<N_UL_symb-2)) || ((nsymb>=N_UL_symb) && (shortened_format==1)) ){
          j += 12;
          continue;
        }
        int re_offset;
        if ((l < (nsymb >> 1)) && ((m & 1) == 0))
          re_offset = (m * 6) + frame_parms->first_carrier_offset;
        else if ((l < (nsymb >> 1)) && ((m & 1) == 1))
          re_offset = frame_parms->first_carrier_offset + (frame_parms->N_RB_DL - (m >> 1) - 1) * 12;
        else if ((m & 1) == 0)
          re_offset = frame_parms->first_carrier_offset + (frame_parms->N_RB_DL - (m >> 1) - 1) * 12;
        else
          re_offset = ((m - 1) * 6) + frame_parms->first_carrier_offset;

        if (re_offset > frame_parms->ofdm_symbol_size)
          re_offset -= (frame_parms->ofdm_symbol_size);

        unsigned int symbol_offset = (unsigned int)frame_parms->ofdm_symbol_size * l;
        c16_t *rxptr = (c16_t *)&common_vars->rxdataF[aa][symbol_offset];

        for (int i = 0; i < 12; i++, j++, re_offset++) {
          if (re_offset==frame_parms->ofdm_symbol_size)
            re_offset = 0;
          c16_t rxcomp = c16mulShift(rxptr[re_offset], zptr[j], 15);
          n0_IQ = (c32_t){n0_IQ.r + rxcomp.r, n0_IQ.i + rxcomp.i};
        } // re
        calc_cnt++;
      } // symbol
      n0_IQ.r /= 12;
      n0_IQ.i /= 12;
      interference_power += squaredMod(n0_IQ);
    } // antenna
  }
  interference_power /= calc_cnt;
  eNB->measurements.n0_pucch_dB = dB_fixed_x10((int)interference_power)/10;
  LOG_D(PHY,"estimate pucch noise %lf %d %d\n",interference_power,calc_cnt,eNB->measurements.n0_pucch_dB);
  return 0;
}


/* PUCCH format3 << */

uint32_t rx_pucch(PHY_VARS_eNB *eNB,
                  PUCCH_FMT_t fmt,
                  unsigned int UCI_id,
                  unsigned int n1_pucch,
                  unsigned int n2_pucch,
                  unsigned int shortened_format,
                  uint8_t *payload,
                  int frame,
                  unsigned int subframe,
                  unsigned int pucch1_thres,
                  int br_flag)
//-----------------------------------------------------------------------------
{
  LTE_eNB_COMMON *common_vars = &eNB->common_vars;
  LTE_DL_FRAME_PARMS *frame_parms = &eNB->frame_parms;
  int8_t sigma2_dB = eNB->measurements.n0_pucch_dB;
  c16_t z[12 * 14];
  c16_t rxcomp[4][12 * 14];
  const unsigned int c = (frame_parms->Ncp == 0) ? 3 : 2;
  unsigned int phase_max = 0;
  int32_t chest_mag = 0;
  ;
  uint32_t stat_max = 0, stat0_max[4], stat1_max[4];
  uint8_t log2_maxh = 0;
  const unsigned int deltaPUCCH_Shift = frame_parms->pucch_config_common.deltaPUCCH_Shift;
  const unsigned int NRB2 = frame_parms->pucch_config_common.nRB_CQI;
  const unsigned int Ncs1_div_deltaPUCCH_Shift = frame_parms->pucch_config_common.nCS_AN;

  const uint32_t u0 = (frame_parms->pucch_config_common.grouphop[subframe << 1]) % 30;
  const uint32_t u1 = (frame_parms->pucch_config_common.grouphop[1 + (subframe << 1)]) % 30;
  const uint32_t v0 = frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.seqhop[subframe << 1];
  const uint32_t v1 = frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.seqhop[1 + (subframe << 1)];
  /* PUCCH format3 >> */
  int16_t b[48];                                                //[bit]
  int16_t payload_entity = -1;
  int16_t payload_max;

  const unsigned int do_sr = 1;
  const rnti_t crnti = 0x1234;
  const int16_t DTXthreshold = 10;
  /* PUCCH format3 << */

  static int first_call = 1;
  if (first_call == 1) {
    for (int i = 0; i < 10; i++) {
      for (int j = 0; j < NUMBER_OF_UE_MAX; j++) {
        eNB->pucch1_stats_cnt[j][i]=0;
        eNB->pucch1ab_stats_cnt[j][i]=0;
        if ( IS_SOFTMODEM_IQPLAYER)
          eNB->pucch1_stats_thres[j][i]=0;
      }
    }
    first_call=0;
  }
  eNB_UCI_STATS_t *uci_stats = NULL;
  if(fmt!=pucch_format3) {  /* PUCCH format3 */
 
    eNB_UCI_STATS_t *first_uci_stats=NULL;
    for (int i=0;i<NUMBER_OF_SCH_STATS_MAX;i++) 
      if (eNB->uci_stats[i].rnti == eNB->uci_vars[UCI_id].rnti) { 
        uci_stats = &eNB->uci_stats[i];
        break;
      } else if (first_uci_stats == NULL && eNB->uci_stats[i].rnti == 0) first_uci_stats = &eNB->uci_stats[i];

    if (uci_stats == NULL) {
      if (first_uci_stats == NULL) {
        LOG_E(PHY,"first_uci_stats is NULL\n");
        return -1;
      }
      uci_stats=first_uci_stats;
      uci_stats->rnti = eNB->uci_vars[UCI_id].rnti;
    }

    AssertFatal(uci_stats!=NULL,"No stat index found\n");
    uci_stats->frame = frame;
    // TODO
    // "SR+ACK/NACK" length is only 7 bits.
    // This restriction will be lifted in the future.
    // "CQI/PMI/RI+ACK/NACK" will be supported in the future.
    if ((deltaPUCCH_Shift==0) || (deltaPUCCH_Shift>3)) {
      LOG_E(PHY,"[eNB] rx_pucch: Illegal deltaPUCCH_shift %d (should be 1,2,3)\n",deltaPUCCH_Shift);
      return(-1);
    }

    if (Ncs1_div_deltaPUCCH_Shift > 7) {
      LOG_E(PHY,"[eNB] rx_pucch: Illegal Ncs1_div_deltaPUCCH_Shift %d (should be 0...7)\n",Ncs1_div_deltaPUCCH_Shift);
      return(-1);
    }
    c16_t *zptr = z;
    unsigned int thres = (c * Ncs1_div_deltaPUCCH_Shift);
    const unsigned int Nprime_div_deltaPUCCH_Shift = (n1_pucch < thres) ? Ncs1_div_deltaPUCCH_Shift : (12 / deltaPUCCH_Shift);
    const unsigned int Nprime = Nprime_div_deltaPUCCH_Shift * deltaPUCCH_Shift;
#ifdef DEBUG_PUCCH_RX
    printf("[eNB] PUCCH: cNcs1/deltaPUCCH_Shift %d, Nprime %d, n1_pucch %d\n",thres,Nprime,n1_pucch);
#endif
    const unsigned int N_UL_symb = (frame_parms->Ncp == NORMAL) ? 7 : 6;
    int16_t nprime0, nprime1;
    if (n1_pucch < thres)
      nprime0=n1_pucch;
    else
      nprime0 = (n1_pucch - thres)%(12*c/deltaPUCCH_Shift);

    if (n1_pucch >= thres)
      nprime1= ((c*(nprime0+1))%((12*c/deltaPUCCH_Shift)+1))-1;
    else {
      unsigned int d = (frame_parms->Ncp == 0) ? 2 : 0;
      unsigned int h = (nprime0 + d) % (c * Nprime_div_deltaPUCCH_Shift);
      nprime1 = (h/c) + (h%c)*Nprime_div_deltaPUCCH_Shift;
    }

#ifdef DEBUG_PUCCH_RX
    printf("PUCCH: nprime0 %d nprime1 %d\n",nprime0,nprime1);
#endif
    unsigned int n_oc0 = nprime0 / Nprime_div_deltaPUCCH_Shift;

    if (frame_parms->Ncp==1)
      n_oc0<<=1;

    unsigned int n_oc1 = nprime1 / Nprime_div_deltaPUCCH_Shift;

    if (frame_parms->Ncp==1)  // extended CP
      n_oc1<<=1;

#ifdef DEBUG_PUCCH_RX
    printf("[eNB] PUCCH: noc0 %d noc11 %d\n",n_oc0,n_oc1);
#endif
    int16_t nprime = nprime0;
    unsigned int n_oc = n_oc0;
    c16_t W = {};
    // loop over 2 slots
    for (unsigned int ns = (subframe << 1), u = u0, v = v0; ns < (2 + (subframe << 1)); ns++, u = u1, v = v1) {
      unsigned int S;
      if ((nprime&1) == 0)
        S=0;  // 1
      else
        S=1;  // j

      //loop over symbols in slot
      for (unsigned int l = 0; l < N_UL_symb; l++) {
        // Compute n_cs (36.211 p. 18)
        unsigned int n_cs = eNB->ncs_cell[ns][l];

        if (frame_parms->Ncp==0) { // normal CP
          n_cs = (n_cs + (nprime * deltaPUCCH_Shift + (n_oc % deltaPUCCH_Shift)) % Nprime) % 12;
        } else {
          n_cs = (n_cs + (nprime * deltaPUCCH_Shift + (n_oc >> 1)) % Nprime) % 12;
        }

        unsigned int refs = 0;

        // Comput W_noc(m) (36.211 p. 19)
        if ((ns==(1+(subframe<<1))) && (shortened_format==1)) {  // second slot and shortened format
          if (l<2) {                                         // data
            W = W3[n_oc][l];
          } else if ((l<N_UL_symb-2)&&(frame_parms->Ncp==0)) { // reference and normal CP
            W = W3[n_oc][l - 2];
            refs=1;
          } else if ((l<N_UL_symb-2)&&(frame_parms->Ncp==1)) { // reference and extended CP
            W = (c16_t){W4[n_oc][l - 2], 0};
            refs=1;
          } else if ((l>=N_UL_symb-2)) {                      // data
            W = W3[n_oc][l - N_UL_symb + 4];
          }
        } else {
          if (l<2) {                                         // data
            W = (c16_t){W4[n_oc][l], 0};
          } else if ((l<N_UL_symb-2)&&(frame_parms->Ncp==NORMAL)) { // reference and normal CP
            W = W3[n_oc][l - 2];
            refs=1;
          } else if ((l<N_UL_symb-2)&&(frame_parms->Ncp==EXTENDED)) { // reference and extended CP
            W = (c16_t){W4[n_oc][l - 2], 0};
            refs=1;
          } else if ((l>=N_UL_symb-2)) {                     // data
            W = (c16_t){W4[n_oc][l - N_UL_symb + 4], 0};
          }
        }

        // multiply W by S(ns) (36.211 p.17). only for data, reference symbols do not have this factor
        if ((S==1)&&(refs==0)) {
          W = (c16_t){-W.i, W.r};
        }

#ifdef DEBUG_PUCCH_RX
        printf("[eNB] PUCCH: ncs[%d][%d]=%d, W_re %d, W_im %d, S %d, refs %d\n",ns,l,n_cs,W_re,W_im,S,refs);
#endif
        unsigned int alpha_ind = 0;
        // compute output sequence

        for (unsigned int n = 0; n < 12; n++) {
          // this is r_uv^alpha(n)
          c16_t tmp = c16mulShift(alphaTBL[alpha_ind], ul_ref_sigs[u][v][0][n], 15);
          // this is S(ns)*w_noc(m)*r_uv^alpha(n)
          zptr[n] = c16mulShift(tmp, W, 15);
#ifdef DEBUG_PUCCH_RX
          printf("[eNB] PUCCH subframe %d z(%d,%u) => %d,%d, alpha(%d) => %d,%d\n",subframe,l,n,zptr[n<<1],zptr[(n<<1)+1],
                 alpha_ind,alpha_re[alpha_ind],alpha_im[alpha_ind]);
#endif
          alpha_ind = (alpha_ind + n_cs)%12;
        } // n

        zptr += 12;
      } // l

      nprime=nprime1;
      n_oc  =n_oc1;
    } // ns

    unsigned int rem = ((((deltaPUCCH_Shift * Ncs1_div_deltaPUCCH_Shift) >> 3) & 7) > 0) ? 1 : 0;
    unsigned int m = (n1_pucch < thres) ? NRB2
                                : (((n1_pucch - thres) / (12 * c / deltaPUCCH_Shift)) + NRB2
                                   + ((deltaPUCCH_Shift * Ncs1_div_deltaPUCCH_Shift) >> 3) + rem);
#ifdef DEBUG_PUCCH_RX
    printf("[eNB] PUCCH: m %d, thres %d, NRB2 %d\n",m,thres,NRB2);
#endif
    unsigned int nsymb = N_UL_symb << 1;
    zptr = z;
    // Do detection
    for (unsigned int aa = 0; aa < frame_parms->nb_antennas_rx; aa++) {
      for (int j = 0, l = 0; l < nsymb; l++) {
        unsigned int re_offset;
        if (br_flag > 0 ) {
          if ((m&1) == 0)
            re_offset = (m*6) + frame_parms->first_carrier_offset;
          else
            re_offset = frame_parms->first_carrier_offset + (frame_parms->N_RB_DL - (m>>1) - 1)*12;
        } else {
          if ((l<(nsymb>>1)) && ((m&1) == 0))
            re_offset = (m*6) + frame_parms->first_carrier_offset;
          else if ((l<(nsymb>>1)) && ((m&1) == 1))
            re_offset = frame_parms->first_carrier_offset + (frame_parms->N_RB_DL - (m>>1) - 1)*12;
          else if ((m&1) == 0)
            re_offset = frame_parms->first_carrier_offset + (frame_parms->N_RB_DL - (m>>1) - 1)*12;
          else
            re_offset = ((m-1)*6) + frame_parms->first_carrier_offset;
        }

        if (re_offset > frame_parms->ofdm_symbol_size)
          re_offset -= (frame_parms->ofdm_symbol_size);

        unsigned int symbol_offset = frame_parms->ofdm_symbol_size * l;
        c16_t *rxptr = (c16_t *)&common_vars->rxdataF[aa][symbol_offset];

        for (int i = 0; i < 12; i++, j++, re_offset++) {
          if (re_offset==frame_parms->ofdm_symbol_size)
            re_offset = 0;

          rxcomp[aa][j] = c16mulShift(rxptr[re_offset], zptr[j], 15);
#ifdef DEBUG_PUCCH_RX
          printf("[eNB] PUCCH subframe %d (%d,%d,%d,%d,%d) => (%d,%d) x (%d,%d) : (%d,%d)\n",subframe,l,i,re_offset,m,j,
                 rxptr[re_offset<<1],rxptr[1+(re_offset<<1)],
                 zptr[j],zptr[1+j],
                 rxcomp[aa][j],rxcomp[aa][1+j]);
#endif
        } // re
      } // symbol
    } // antenna

    // PUCCH Format 1
    // Do cfo correction and MRC across symbols

    if (fmt == pucch_format1) {
      uci_stats->pucch1_trials++;
#ifdef DEBUG_PUCCH_RX
      printf("Doing PUCCH detection for format 1\n");
#endif
      stat_max = 0;

      for (unsigned int phase = 0; phase < 7; phase++) {
        int stat=0;
        c32_t stat0_cmul[4] = {};
        c32_t stat1_cmul[4] = {};
        uint32_t stat0[4]={};
        uint32_t stat1[4]={};
        for (unsigned int aa = 0; aa < frame_parms->nb_antennas_rx; aa++) {
          for (unsigned int re = 0; re < 12; re++) {
            unsigned int off = re;
            const c16_t *cfo = (c16_t *)(frame_parms->Ncp == 0 ? &cfo_pucch_np[14 * phase] : &cfo_pucch_ep[12 * phase]);

            for (unsigned int l = 0; l < (nsymb >> 1); l++) {
              c16_t tmp =  c16x32div(c32x16mulShift(rxcomp[aa][off], cfo[l], 15),nsymb);
              stat0_cmul[aa].r += tmp.r;
              stat0_cmul[aa].i += tmp.i;
              off++;
            }

            for (unsigned int l2 = 0, l = (nsymb >> 1); l < nsymb; l++, l2++) {
              c16_t tmp = c16x32div(c32x16mulShift(rxcomp[aa][off], cfo[l2], 15),nsymb);
              stat1_cmul[aa].r += tmp.r;
              stat1_cmul[aa].i += tmp.i;
              off++;
            }

            stat0[aa] += squaredMod(stat0_cmul[aa]);
            stat1[aa] += squaredMod(stat1_cmul[aa]);
          } // re
          stat+=(stat0[aa]+stat1[aa]);
        } // aa
        if (stat>stat_max) {
          stat_max = stat;
          phase_max = phase;
          for (unsigned int aa = 0; aa < frame_parms->nb_antennas_rx; aa++) {
            stat0_max[aa] = stat0[aa];
            stat1_max[aa] = stat1[aa];
          }
        }
      } // phase

      stat_max /= 12;
#ifdef DEBUG_PUCCH_RX
      printf("[eNB] PUCCH: stat %d, stat_max %u, phase_max %d\n", stat,stat_max,phase_max);
#endif
#ifdef DEBUG_PUCCH_RX
      LOG_I(PHY,"[eNB] PUCCH fmt1:  stat_max : %d, sigma2_dB %d (%d, %d), phase_max : %d\n",dB_fixed(stat_max),sigma2_dB,eNB->measurements.n0_subband_power_tot_dBm[6],pucch1_thres,phase_max);
#endif
      eNB->pucch1_stats[UCI_id][(subframe<<10)+eNB->pucch1_stats_cnt[UCI_id][subframe]] = stat_max;
      eNB->pucch1_stats_thres[UCI_id][(subframe<<10)+eNB->pucch1_stats_cnt[UCI_id][subframe]] = sigma2_dB+pucch1_thres;
      eNB->pucch1_stats_cnt[UCI_id][subframe] = (eNB->pucch1_stats_cnt[UCI_id][subframe]+1)&1023;
      uci_stats->pucch1_thres = sigma2_dB+pucch1_thres;
      T(T_ENB_PHY_PUCCH_1_ENERGY, T_INT(eNB->Mod_id), T_INT(eNB->uci_vars[UCI_id].rnti), T_INT(frame), T_INT(subframe),
        T_INT(stat_max), T_INT(sigma2_dB+pucch1_thres));

      
      /*
      if (eNB->pucch1_stats_cnt[UE_id][subframe] == 0) {
        LOG_M("pucch_debug.m","pucch_energy",
         &eNB->pucch1_stats[UE_id][(subframe<<10)],
         1024,1,2);
        AssertFatal(0,"Exiting for PUCCH 1 debug\n");

      }
      */

      // This is a moving average of the PUCCH1 statistics conditioned on being above or below the threshold
      if (sigma2_dB<(dB_fixed(stat_max)-pucch1_thres))  {
        *payload = 1;
        uci_stats->current_pucch1_stat_pos = stat_max;
        for (int aa=0;aa<frame_parms->nb_antennas_rx;aa++) {
          uci_stats->pucch1_low_stat[aa]=stat0_max[aa];
          uci_stats->pucch1_high_stat[aa]=stat1_max[aa];
          uci_stats->pucch1_positive_SR++;
        }
      } else {
        uci_stats->current_pucch1_stat_neg = stat_max;
        *payload = 0;
      }

      if (UCI_id==0) {
        VCD_SIGNAL_DUMPER_DUMP_VARIABLE_BY_NAME(VCD_SIGNAL_DUMPER_VARIABLES_UE0_SR_ENERGY,dB_fixed(stat_max));
        VCD_SIGNAL_DUMPER_DUMP_VARIABLE_BY_NAME(VCD_SIGNAL_DUMPER_VARIABLES_UE0_SR_THRES,sigma2_dB+pucch1_thres);
      }
    } else if ((fmt == pucch_format1a)||(fmt == pucch_format1b)) {
      stat_max = 0;
#ifdef DEBUG_PUCCH_RX
      LOG_D(PHY,"Doing PUCCH detection for format 1a/1b\n");
#endif
      int stat_re=0,stat_im=0;
      for (unsigned int phase = 0; phase < 7; phase++) {
        int stat=0;
        c64_t stat0_cumul[4] = {};
        c64_t stat1_cumul[4] = {};
        c64_t stat0_ref_cumul[4] = {};
        c64_t stat1_ref_cumul[4] = {};
        uint32_t stat0[4]={};
        uint32_t stat1[4]={};
        for (unsigned int aa = 0; aa < frame_parms->nb_antennas_rx; aa++) {
          for (unsigned int re = 0; re < 12; re++) {
            // compute received energy first slot, seperately for data and reference
            // by coherent combining across symbols but not resource elements
            // Note: assumption is that channel is stationary over symbols in slot after CFO
            unsigned int off = re;
            const c16_t *cfo = (c16_t *)(frame_parms->Ncp == 0 ? &cfo_pucch_np[14 * phase] : &cfo_pucch_ep[12 * phase]);

            for (unsigned int l = 0; l < (nsymb >> 1); l++) {
              c16_t tmp = c16mulShift(rxcomp[aa][off], cfo[l], 15);
              if ((l<2)||(l>(nsymb>>1) - 3)) {  //data symbols
                stat0_cumul[aa].r += tmp.r;
                stat0_cumul[aa].i += tmp.i;
              } else { //reference symbols
                stat0_ref_cumul[aa].r += tmp.r;
                stat0_ref_cumul[aa].i += tmp.i;
              }

              off++;
            }

            // this is total energy received, summed over data and reference
            stat0[aa] += (squaredMod(stat0_cumul[aa]) + squaredMod(stat0_ref_cumul[aa]))/nsymb;
            // now second slot
            for (unsigned int l2 = 0, l = (nsymb >> 1); l < nsymb; l++, l2++) {
              c16_t tmp = c16mulShift(rxcomp[aa][off], cfo[l2], 15);
              if ((l2<2) || ((l2>(nsymb>>1) - 3)) ) {  // data symbols
                stat1_cumul[aa].r += tmp.r;
                stat1_cumul[aa].i += tmp.i;
              } else { //reference_symbols
                stat1_ref_cumul[aa].r += tmp.r;
                stat1_ref_cumul[aa].i += tmp.i;
              }
              off++;
            }

#ifdef DEBUG_PUCCH_RX
            printf("aa%u re %d : phase %d : stat %d\n",aa,re,phase,stat);
#endif
            stat1[aa] += (squaredMod(stat1_cumul[aa]) + squaredMod(stat1_ref_cumul[aa]))/nsymb;
          } // re
          stat+=(stat0[aa]+stat1[aa]);
        } // aa

#ifdef DEBUG_PUCCH_RX
        LOG_D(PHY,"Format 1A: phase %d : stat %d\n",phase,stat);
#endif
        
        if (stat>stat_max) {
          stat_max = stat;
          phase_max = phase;
        }
      } // phase

      stat_max/=(12);  //normalize to energy per symbol and RE
#ifdef DEBUG_PUCCH_RX
      LOG_I(PHY,"[eNB] PUCCH fmt1a/b:  stat_max : %d (%d : sigma2 %d), phase_max : %d\n",stat_max,dB_fixed(stat_max),sigma2_dB,phase_max);
#endif
      // Do detection now


      // It looks like the pucch1_thres value is a bit messy when RF is replayed.
      // For instance i assume to skip pucch1_thres from the test below.
      // Not 100% sure
        
      if (sigma2_dB<(dB_fixed(stat_max) - (IS_SOFTMODEM_IQPLAYER?0:pucch1_thres)) ) {//
        unsigned int chL = (nsymb >> 1) - 4;
        chest_mag=0;
        const c16_t *cfo = (c16_t *)(frame_parms->Ncp == 0 ? &cfo_pucch_np[14 * phase_max] : &cfo_pucch_ep[12 * phase_max]);
        c16_t chest0[4][12];
        c16_t chest1[4][12];
        for (unsigned int aa = 0; aa < frame_parms->nb_antennas_rx; aa++) {
          for (unsigned int re = 0; re < 12; re++) {

            // channel estimate for first slot
            chest0[aa][re] = (c16_t){0};
            for (unsigned int l = 2; l < (nsymb >> 1) - 2; l++) {
              unsigned int off = re + 12 * l;
              c16_t tmp  = c16x32div(c32x16mulShift(rxcomp[aa][off], cfo[l], 15),chL);
              chest0[aa][re].r += tmp.r;
              chest0[aa][re].i += tmp.i;
            }

            // channel estimate for second slot
            chest1[aa][re] = (c16_t){0};
            for (unsigned int l = 2; l < (nsymb >> 1) - 2; l++) {
              unsigned int off = re + 12 * l + (nsymb >> 1) * 12;
              c16_t tmp = c16x32div(c32x16mulShift(rxcomp[aa][off], cfo[l], 15),chL);
              chest1[aa][re].r += tmp.r;
              chest1[aa][re].i += tmp.i;
            }

            chest_mag = max(chest_mag, squaredMod(chest0[aa][re]));
            chest_mag = max(chest_mag, squaredMod(chest1[aa][re]));
          }
        }

        log2_maxh = log2_approx(chest_mag)/2;
#ifdef DEBUG_PUCCH_RX
        printf("PUCCH 1A: log2_maxh %d\n",log2_maxh);
#endif

        // now do channel matched filter
        c32_t stat0_cumul[4] = {};
        c32_t stat1_cumul[4] = {};
        for (unsigned int aa = 0; aa < frame_parms->nb_antennas_rx; aa++) {
          for (unsigned int re = 0; re < 12; re++) {
            // first slot, left of RS
            for (unsigned int l = 0; l < 2; l++) {
              unsigned int off = re + 12 * l;
              c16_t tmp = c16mulShift(rxcomp[aa][off], cfo[l], 15);
              c16_t tmp2 = c16MulConjShift(tmp, chest0[aa][re], log2_maxh);
              stat0_cumul[aa].r += tmp2.r;
              stat0_cumul[aa].i += tmp2.i;
            }

            // first slot, right of RS
            for (unsigned int l = (nsymb >> 1) - 2; l < (nsymb >> 1); l++) {
              unsigned int off = re + 12 * l;
              c16_t tmp = c16mulShift(rxcomp[aa][off], cfo[l], 15);
              c16_t tmp2 = c16MulConjShift(tmp, chest0[aa][re], log2_maxh);
              stat0_cumul[aa].r += tmp2.r;
              stat0_cumul[aa].i += tmp2.i;
            }

#ifdef DEBUG_PUCCH_RX
            printf("[eNB] PUCCH subframe %d chest1[%u][%d] => (%d,%d)\n",subframe,aa,re,
                   chest0_re[aa][re],chest0_im[aa][re]);
#endif

            // second slot, left of RS
            for (unsigned int l = 0; l < 2; l++) {
              unsigned int off = re + 12 * l + (nsymb >> 1) * 12;
              c16_t tmp = c16mulShift(rxcomp[aa][off], cfo[l], 15);
              c16_t tmp2 = c16MulConjShift(tmp, chest1[aa][re], log2_maxh);
              stat1_cumul[aa].r += tmp2.r;
              stat1_cumul[aa].i += tmp2.i;
            }

            // second slot, right of RS
            for (unsigned int l = (nsymb >> 1) - 2; l < (nsymb >> 1) - 1; l++) {
              unsigned int off = re + 12 * l + (nsymb >> 1) * 12;
              c16_t tmp = c16mulShift(rxcomp[aa][off], cfo[l], 15);
              c16_t tmp2 = c16MulConjShift(tmp, chest1[aa][re], log2_maxh);
              stat1_cumul[aa].r += tmp2.r;
              stat1_cumul[aa].i += tmp2.i;
            }

#ifdef DEBUG_PUCCH_RX
            printf("aa%u re %d : stat %d,%d\n",aa,re,stat_re,stat_im);
#endif
          } // re
          stat_re += stat0_cumul[aa].r + stat1_cumul[aa].r;
          stat_im += stat0_cumul[aa].i + stat1_cumul[aa].i;
        } // aa

        LOG_D(PHY,"PUCCH 1a/b: subframe %d : stat %d,%d (pos %d)\n",subframe,stat_re,stat_im,
              (subframe<<10) + (eNB->pucch1ab_stats_cnt[UCI_id][subframe]));
        LOG_D(PHY,"In pucch.c PUCCH 1a/b: ACK subframe %d : sigma2_dB %d, stat_max %d, pucch1_thres %d\n",subframe,sigma2_dB,dB_fixed(stat_max),pucch1_thres);
        eNB->pucch1ab_stats[UCI_id][(subframe<<11) + 2*(eNB->pucch1ab_stats_cnt[UCI_id][subframe])] = (stat_re);
        eNB->pucch1ab_stats[UCI_id][(subframe<<11) + 1+2*(eNB->pucch1ab_stats_cnt[UCI_id][subframe])] = (stat_im);
        eNB->pucch1ab_stats_cnt[UCI_id][subframe] = (eNB->pucch1ab_stats_cnt[UCI_id][subframe]+1)&1023;
        /* frame not available here - set to -1 for the moment */
        T(T_ENB_PHY_PUCCH_1AB_IQ, T_INT(eNB->Mod_id), T_INT(eNB->uci_vars[UCI_id].rnti), T_INT(-1), T_INT(subframe), T_INT(stat_re), T_INT(stat_im));
        *payload = (stat_re<0) ? 1 : 2; // 1 == ACK, 2 == NAK

        if (fmt==pucch_format1b) {
          uci_stats->pucch1b_trials++;
          *(1+payload) = (stat_im<0) ? 1 : 2;
          uci_stats->current_pucch1b_stat_re = stat_re;
          uci_stats->current_pucch1b_stat_im = stat_im;
        }
        else {
          uci_stats->pucch1a_trials++;
          uci_stats->current_pucch1a_stat_re = stat_re;
          uci_stats->current_pucch1a_stat_im = stat_im;
        }

      } else { // insufficient energy on PUCCH so NAK
        LOG_D(PHY,"In pucch.c PUCCH 1a/b: NAK subframe %d : sigma2_dB %d, stat_max %d, pucch1_thres %d\n",subframe,sigma2_dB,dB_fixed(stat_max),pucch1_thres);
        *payload = 4;  // DTX
        ((int16_t *)&eNB->pucch1ab_stats[UCI_id][(subframe<<10) + (eNB->pucch1ab_stats_cnt[UCI_id][subframe])])[0] = (int16_t)(stat_re);
        ((int16_t *)&eNB->pucch1ab_stats[UCI_id][(subframe<<10) + (eNB->pucch1ab_stats_cnt[UCI_id][subframe])])[1] = (int16_t)(stat_im);
        eNB->pucch1ab_stats_cnt[UCI_id][subframe] = (eNB->pucch1ab_stats_cnt[UCI_id][subframe]+1)&1023;

        if (fmt==pucch_format1b)
          *(1+payload) = 4;
        uci_stats->pucch1ab_DTX++;
      }
    } else {
      LOG_E(PHY,"[eNB] PUCCH fmt2/2a/2b not supported\n");
    }

    /* PUCCH format3 >> */
  } else {
    /* SubCarrier Demap */
    c16_t SubCarrierDeMapData[4][14][12]; //[Antenna][Symbol][Subcarrier][Complex]
    unsigned int n3_pucch = 20;
    int Ret = pucchfmt3_subCarrierDeMapping(eNB, SubCarrierDeMapData, n3_pucch);

    if(Ret != 0) {
      //***log pucchfmt3_subCarrierDeMapping Error!
      return(-1);
    }

    /* cyclic shift hopping remove */
    c16_t CshData_fmt3[4][14][12]; //[Antenna][Symbol][Subcarrier][Complex]
    Ret = pucchfmt3_Baseseq_csh_remove( SubCarrierDeMapData, CshData_fmt3, frame_parms, subframe, eNB->ncs_cell );

    if(Ret != 0) {
      //***log pucchfmt3_Baseseq_csh_remove Error!
      return(-1);
    }

    /* Channel Estimation */
    c16_t ChestValue[4][2][12]; //[Antenna][Slot][Subcarrier][Complex]
    double delta_theta[4][12]; //[Antenna][Subcarrier][Complex]
    int16_t Interpw;
    // TODO
    // When using PUCCH format3, it must be an argument of rx_pucch function
    uint16_t n3_pucch_array[NUMBER_OF_UE_MAX] = {1};
    n3_pucch_array[0] = n3_pucch;
    Ret = pucchfmt3_ChannelEstimation( SubCarrierDeMapData, delta_theta, ChestValue, &Interpw, subframe, shortened_format, frame_parms, n3_pucch, n3_pucch_array, eNB->ncs_cell );

    if(Ret != 0) {
      //***log pucchfmt3_ChannelEstimation Error!
      return(-1);
    }

    /* Channel Equalization */
    c16_t ChdetAfterValue_fmt3[4][14][12]; //[Antenna][Symbol][Subcarrier][Complex]
    Ret = pucchfmt3_Equalization( CshData_fmt3, ChdetAfterValue_fmt3, ChestValue, frame_parms );

    if(Ret != 0) {
      //***log pucchfmt3_Equalization Error!
      return(-1);
    }

    /* Frequency deviation remove AFC */
    c16_t RemoveFrqDev_fmt3[4][2][5][12]; //[Antenna][Slot][PUCCH_Symbol][Subcarrier][Complex]
    Ret = pucchfmt3_FrqDevRemove( ChdetAfterValue_fmt3, delta_theta, RemoveFrqDev_fmt3, frame_parms );

    if(Ret != 0) {
      //***log pucchfmt3_FrqDevRemove Error!
      return(-1);
    }

    /* orthogonal sequence remove */
    c16_t Fmt3xDataRmvOrth[4][2][5][12]; //[Antenna][Slot][PUCCH_Symbol][Subcarrier][Complex]
    Ret = pucchfmt3_OrthSeqRemove( RemoveFrqDev_fmt3, Fmt3xDataRmvOrth, shortened_format, n3_pucch, frame_parms );

    if(Ret != 0) {
      //***log pucchfmt3_OrthSeqRemove Error!
      return(-1);
    }

    /* averaging antenna */
    c16_t Fmt3xDataAvgAnt[2][5][12]; //[Slot][PUCCH_Symbol][Subcarrier][Complex]
    pucchfmt3_AvgAnt( Fmt3xDataRmvOrth, Fmt3xDataAvgAnt, shortened_format, frame_parms );
    /* averaging symbol */
    c16_t Fmt3xDataAvgSym[2][12]; //[Slot][Subcarrier][Complex]
    pucchfmt3_AvgSym( Fmt3xDataAvgAnt, Fmt3xDataAvgSym, shortened_format );
    /* IDFT */
    c16_t IFFTOutData_Fmt3[2][12]; //[Slot][Subcarrier][Complex]
    pucchfmt3_IDft2(Fmt3xDataAvgSym[0], IFFTOutData_Fmt3[0]);
    pucchfmt3_IDft2(Fmt3xDataAvgSym[1], IFFTOutData_Fmt3[1]);
    /* descramble */
    pucchfmt3_Descramble(IFFTOutData_Fmt3, b, subframe, frame_parms->Nid_cell, crnti);

    /* Is payload 6bit or 7bit? */
    if( do_sr == 1 ) {
      payload_max = 7;
    } else {
      payload_max = 6;
    }

    /* decode */
    payload_entity = pucchfmt3_Decode( b, subframe, DTXthreshold, Interpw, do_sr );

    if (payload_entity == -1) {
      //***log pucchfmt3_Decode Error!
      return(-1);
    }

    for (int i = 0; i < payload_max; i++) {
      payload[i] = (uint8_t)((payload_entity >> i) & 0x01);
    }
  }

  /* PUCCH format3 << */
  return((int32_t)stat_max);
}
