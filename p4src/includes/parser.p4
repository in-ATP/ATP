

metadata p4ml_meta_t mdata;
metadata p4ml_constant_t p4ml_constant;

header ethernet_t ethernet;
header ipv4_t     ipv4;
header udp_t      udp;
header p4ml_agtr_index_t p4ml_agtr_index;
header p4ml_agtr_index_t p4ml_agtr_index_useless;
header p4ml_agtr_index_t p4ml_agtr_index_useless2;

header p4ml_t p4ml;
header entry_t p4ml_entries;
header entry_t p4ml_entries_useless;

header bg_p4ml_t     p4ml_bg;
// header blank3_t blank3;
/*************************************************************************
 ***********************  P A R S E R  ***********************************
 *************************************************************************/

parser start {
    extract(ethernet);
    set_metadata(mdata.value_one, 1);   
    return select(ethernet.etherType) {
        0x0700   : parse_ipv4;
        0x0800   : parse_rdma;
        0x0900   : parse_bg;
        default  : ingress;
    }
    // return parse_ipv4;

}

parser parse_ipv4 {
    extract(ipv4);
    return parse_p4ml;
}

parser parse_p4ml {
    extract(p4ml);
    return  select(p4ml.dataIndex) {
        0x0     : check_if_resubmit;
        0x1     : use_second_p4ml_agtr_index_recirculate;
        default : ingress;
    }
}

parser check_if_resubmit {
    return  select(ig_intr_md.resubmit_flag) {
        // 0x0     : parse_p4ml_agtr_index;
        0x0     : use_first_p4ml_agtr_index_recirculate;
        // 0x1     : skip_first_p4ml_agtr_index;
        0x1     : use_second_p4ml_agtr_index_recirculate;
        default : ingress;
    }
}

/// resubmit 0x0

parser parse_p4ml_agtr_index {
	extract(p4ml_agtr_index);
	return skip_second_p4ml_agtr_index; 
}

@pragma force_shift ingress 16 /* 2 bytes */
parser skip_second_p4ml_agtr_index {
	return parse_entry;
}

parser parse_entry {
	extract(p4ml_entries);
	return ingress;
}

/// resubmit 0x1

parser parse_p4ml_agtr_index2 {
	extract(p4ml_agtr_index);
	return skip_header_c_0_31;
}

@pragma force_shift ingress 16 /* 2 bytes */
parser skip_first_p4ml_agtr_index {
	return parse_p4ml_agtr_index2;
}

/// recirculate 2

parser use_second_p4ml_agtr_index_recirculate {
	extract(p4ml_agtr_index_useless2);
	return parse_p4ml_agtr_index_recirculate;
}

parser parse_p4ml_agtr_index_recirculate {
	extract(p4ml_agtr_index);
	return parse_entry2;
}

parser parse_entry2 {
	extract(p4ml_entries_useless);
	return parse_entry;
}

/// recirculate 1

parser use_first_p4ml_agtr_index_recirculate {
	extract(p4ml_agtr_index);
	return useless_second_p4ml_agtr_index_recirculate;
}

parser useless_second_p4ml_agtr_index_recirculate {
	extract(p4ml_agtr_index_useless);
	return parse_entry;
}
///
 
@pragma force_shift ingress 256 /* 32 bytes */
parser skip_header_c_0_31 {
    return skip_header_c_32_63;
}

@pragma force_shift ingress 256 /* 32 bytes */
parser skip_header_c_32_63 {
    return skip_header_c_64_95;
}

@pragma force_shift ingress 256 /* 32 bytes */
parser skip_header_c_64_95 {
    return skip_header_c_96_127;
}

@pragma force_shift ingress 256 /* 32 bytes */
parser skip_header_c_96_127 {
    return parse_entry;
}


// /* RDMA */
parser parse_rdma {
    extract(ipv4);
    return ingress;
}

// /* BG */
parser parse_bg {
    extract(ipv4);
    return parse_udp_bg;
}

parser parse_udp_bg {
    extract(udp);
    return parse_p4ml_bg;
}

parser parse_p4ml_bg {
    extract(p4ml_bg);
//set_metadata(mdata.qdepth, 0);    
//    return ingress; 
   return ingress;
}
