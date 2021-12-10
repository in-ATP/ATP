

/*  Egress -------------------------------*/
control Dcqcn(
        inout switch_header_t hdr2,
        in egress_intrinsic_metadata_t eg_intr_md){
    Register<bit<32>, bit<1>>(1, 0) ecn;
    RegisterAction<bit<32>, bit<1>, bit<32>>(ecn) ecn_mark = {
        void apply(inout bit<32> value, out bit<32> read_value) {
            read_value=0;
            if(eg_intr_md.deq_qdepth>1250){
                read_value=6;
            
            }
        }
    };
    
    apply{
        bit<32> s3;
        s3=ecn_mark.execute(0);
        if(s3== 6){
            if(hdr2.ethernet.ether_type == 0x0800){
                hdr2.ipv4.diffserv =0b11;
            }
        }
    }

}


