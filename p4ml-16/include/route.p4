control Route(
        inout switch_header_t hdr,
        in ingress_intrinsic_metadata_t ig_intr_md,
        inout ingress_intrinsic_metadata_for_deparser_t ig_intr_md_for_dprsr,
        inout ingress_intrinsic_metadata_for_tm_t ig_intr_md_for_tm,
        inout  bit<1> dataIndex,
        in bit<1> isMyAppIDandMyCurrentSeq
        ){

    Register<bit<32>, bit<12>>(4096,0) kk_count;
    RegisterAction<bit<32>, bit<12>, bit<32>>(kk_count) kk_count_read = {
        void apply(inout bit<32> value, out bit<32> read_value) {
            value = value + 1;
            read_value = value;
        }
    };

    
    action dmac_forward(PortId_t port) {
        ig_intr_md_for_tm.ucast_egress_port = port;
        kk_count_read.execute(0);
    }

    action dmac_forward_and_set_dataIndex(PortId_t port){
        ig_intr_md_for_tm.ucast_egress_port = port;
        dataIndex = 1;
    }
	action dmac_miss() {
		ig_intr_md_for_dprsr.drop_ctl = 3w1;
	}

	table dmac {
		key = {
			hdr.p4ml.appIDandSeqNum: ternary;
        	ig_intr_md.ingress_port: exact;
        	hdr.p4ml.dataIndex: exact;
            // isMyAppIDandMyCurrentSeq : exact;
        	// hdr.p4ml.PSIndex: exact;
		}

		actions = {
			dmac_forward;
            dmac_forward_and_set_dataIndex;
			// @defaultonly dmac_forward_and_set_dataIndex;
            @defaultonly dmac_miss;
		}

		const default_action = dmac_miss();
		size = 64;
	}
    apply{
        dmac.apply();
    }

        }

