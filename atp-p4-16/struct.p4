/*******************************************************************************
 * BAREFOOT NETWORKS CONFIDENTIAL & PROPRIETARY
 *
 * Copyright (c) 2015-2019 Barefoot Networks, Inc.

 * All Rights Reserved.
 *
 * NOTICE: All information contained herein is, and remains the property of
 * Barefoot Networks, Inc. and its suppliers, if any. The intellectual and
 * technical concepts contained herein are proprietary to Barefoot Networks,
 * Inc.
 * and its suppliers and may be covered by U.S. and Foreign Patents, patents in
 * process, and are protected by trade secret or copyright law.
 * Dissemination of this information or reproduction of this material is
 * strictly forbidden unless prior written permission is obtained from
 * Barefoot Networks, Inc.
 *
 * No warranty, explicit or implicit is provided, unless granted under a
 * written agreement with Barefoot Networks, Inc.
 *
 ******************************************************************************/


#include "headers.p4"


struct switch_ingress_metadata_t {
    p4ml_meta_h mdata;
    //p4ml_constant_h p4ml_constant;
    //p4ml_agtr_index_h p4ml_agtr_index;
}

struct switch_egress_metadata_t {
 
}


struct switch_header_t {
    ethernet_h ethernet;
    ipv4_h ipv4;
    udp_h udp;
    p4ml_h p4ml;
    p4ml_agtr_index_h p4ml_agtr_index;
    p4ml_bg_h p4ml_bg;
    entry_h p4ml_entries;
    entry_h p4ml_entries1;

}

