#include <core.p4>
#if __TARGET_TOFINO__ == 2
#include <t2na.p4>
#else
#include <tna.p4>
#endif
#include "headers.p4"
#include "struct.p4"
#include "parde.p4"
#include "dcqcn.p4"
#include "atp.p4"
#include "route.p4"
//-----------------------------------------------------------------------------
// Destination MAC lookup
// - Bridge out the packet of the interface in the MAC entry.
// - Flood the packet out of all ports within the ingress BD.
//-----------------------------------------------------------------------------

control SwitchIngress(
        inout switch_header_t hdr,
        inout switch_ingress_metadata_t ig_md,
        in ingress_intrinsic_metadata_t ig_intr_md,
        in ingress_intrinsic_metadata_from_parser_t ig_prsr_md,
        inout ingress_intrinsic_metadata_for_deparser_t ig_intr_md_for_dprsr,
        inout ingress_intrinsic_metadata_for_tm_t ig_intr_md_for_tm){

    action dmac_forward(PortId_t port) {
        ig_intr_md_for_tm.ucast_egress_port = port;
    }

	action dmac_miss() {
		ig_intr_md_for_dprsr.drop_ctl = 3w1;
	}

	table dmac {
		key = {
			hdr.ethernet.dst_addr : exact;
		}

		actions = {
			dmac_forward;
			@defaultonly dmac_miss;
		}

		const default_action = dmac_miss;
		size = 32;
	}

    AppIdSeq() appid_seq;

    apply{

        if(hdr.p4ml.isValid()){
                  //handle normal p4ml packets
                appid_seq.apply(hdr, ig_intr_md_for_dprsr, ig_intr_md_for_tm, hdr.p4ml, hdr.p4ml_entries, hdr.p4ml_entries1, ig_md.mdata, hdr.p4ml_agtr_index);
        
        }else{
            //bg traffic; only forward;
            dmac.apply();
        }
	}

}

control SwitchEgress(
        inout switch_header_t hdr,
        inout switch_egress_metadata_t eg_md,
        in egress_intrinsic_metadata_t eg_intr_md,
        in egress_intrinsic_metadata_from_parser_t eg_intr_from_prsr,
        inout egress_intrinsic_metadata_for_deparser_t eg_intr_md_for_dprsr,
        inout egress_intrinsic_metadata_for_output_port_t eg_intr_md_for_oport) {

    Dcqcn() dcqcn;

	apply {
        dcqcn.apply(hdr, eg_intr_md);
       
    }

}
Pipeline(SwitchIngressParser(),
        SwitchIngress(),
        SwitchIngressDeparser(),
        SwitchEgressParser(),
        SwitchEgress(),
        SwitchEgressDeparser()) pipe;

Switch(pipe) main;

