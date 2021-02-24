clear_all()

p4_pd.register_reset_all_agtr_time()
p4_pd.register_reset_all_appID_and_Seq()
p4_pd.register_reset_all_bitmap()
p4_pd.register_reset_all_register1()
p4_pd.register_reset_all_register2()
p4_pd.register_reset_all_register3()
p4_pd.register_reset_all_register4()
p4_pd.register_reset_all_register5()
p4_pd.register_reset_all_register6()
p4_pd.register_reset_all_register7()
p4_pd.register_reset_all_register8()
p4_pd.register_reset_all_register9()
p4_pd.register_reset_all_register10()
p4_pd.register_reset_all_register11()
p4_pd.register_reset_all_register12()
p4_pd.register_reset_all_register13()
p4_pd.register_reset_all_register14()
p4_pd.register_reset_all_register15()
p4_pd.register_reset_all_register16()
p4_pd.register_reset_all_register17()
p4_pd.register_reset_all_register18()
p4_pd.register_reset_all_register19()
p4_pd.register_reset_all_register20()
p4_pd.register_reset_all_register21()
p4_pd.register_reset_all_register22()
p4_pd.register_reset_all_register23()
p4_pd.register_reset_all_register24()
p4_pd.register_reset_all_register25()
p4_pd.register_reset_all_register26()
p4_pd.register_reset_all_register27()
p4_pd.register_reset_all_register28()
p4_pd.register_reset_all_register29()
p4_pd.register_reset_all_register30()
p4_pd.register_reset_all_register31()
# p4_pd.register_reset_all_register32()


# These are background traffic
# p4_pd.bg_outPort_table_table_add_with_set_egr(
#     p4_pd.bg_outPort_table_match_spec_t(0), 
#     p4_pd.set_egr_action_spec_t(4)
# )

# p4_pd.bg_outPort_table_table_add_with_set_egr(
#     p4_pd.bg_outPort_table_match_spec_t(1), 
#     p4_pd.set_egr_action_spec_t(0)
# )

# first Zero for pending
port_of_worker = [0, 56, 48, 40, 32, 24, 16, 8, 0, 4]
single_loopback_port = 20

MAC_address_of_worker = [ "0", 
                          "b8:59:9f:1d:04:f2"
                        , "b8:59:9f:0b:30:72"
                        , "98:03:9b:03:46:50"
                        , "b8:59:9f:02:0d:14"
                        , "b8:59:9f:b0:2d:50"
                        , "b8:59:9f:b0:2b:b0"
                        , "b8:59:9f:b0:2b:b8"
                        , "b8:59:9f:b0:2d:18"
                        , "b8:59:9f:b0:2d:58" ]

# first Zero for pending
# PSs = [0, 9, 8]
PSs = [0, 9]

len_workers = len(port_of_worker)
len_PS = len(PSs)

# Normal Switch traffic
for i in range(1, len_workers):
    p4_pd.forward_table_add_with_set_egr(
        p4_pd.forward_match_spec_t(macAddr_to_string(MAC_address_of_worker[i])),
        p4_pd.set_egr_action_spec_t(port_of_worker[i])
    )


# P4ML Traffic

# No Pending packet, First time enter switch
for i in range(1, len_workers - 1):
    for j in range(1, len_PS):
        p4_pd.outPort_table_table_add_with_set_egr_and_set_index(
        p4_pd.outPort_table_match_spec_t(
            1 << 16,
            port_of_worker[i],
            0,
            j-1), 
        # app1 -> worker3
        p4_pd.set_egr_and_set_index_action_spec_t(single_loopback_port))

# Not Pending packet, Second time enter switch
for j in range(1, len_PS):
    print(j, PSs[j])
    p4_pd.outPort_table_table_add_with_set_egr(
    p4_pd.outPort_table_match_spec_t(
        1 << 16,
        single_loopback_port,
        1,
        j-1), 
    # app1 -> worker3
    p4_pd.set_egr_action_spec_t(port_of_worker[PSs[j]]))

# INGRESSPORT, Index
# Worker1 to Worker8
for i in range(1, len_workers - 1):
    p4_pd.drop_table_table_add_with_drop_pkt(
        p4_pd.drop_table_match_spec_t(
            port_of_worker[i],
            1)
    )

####### Server ########
for j in range(1, len_PS):
    p4_pd.multicast_table_table_add_with_multicast(
        p4_pd.multicast_table_match_spec_t(
            1,
            1 << 16,
            port_of_worker[PSs[j]],
            0),
        # multicast app1 -> worker1, 2
        p4_pd.multicast_action_spec_t(999)
    )


p4_pd.modify_packet_bitmap_table_table_add_with_modify_packet_bitmap(
    p4_pd.modify_packet_bitmap_table_match_spec_t(1)
)

p4_pd.modify_packet_bitmap_table_table_add_with_nop(
    p4_pd.modify_packet_bitmap_table_match_spec_t(0)
)

p4_pd.processEntry1_table_add_with_processentry1(
    p4_pd.processEntry1_match_spec_t(hex_to_i32(0), hex_to_i32(0xFFFFFFFF)), 1,
)
p4_pd.processEntry1_table_add_with_noequ0_processentry1(
    p4_pd.processEntry1_match_spec_t(hex_to_i32(0), hex_to_i32(0x00000000)), 1,
)
p4_pd.processEntry2_table_add_with_processentry2(
    p4_pd.processEntry2_match_spec_t(hex_to_i32(0), hex_to_i32(0xFFFFFFFF)), 1,
)
p4_pd.processEntry2_table_add_with_noequ0_processentry2(
    p4_pd.processEntry2_match_spec_t(hex_to_i32(0), hex_to_i32(0x00000000)), 1
)
p4_pd.processEntry3_table_add_with_processentry3(
    p4_pd.processEntry3_match_spec_t(hex_to_i32(0), hex_to_i32(0xFFFFFFFF)), 1,
)
p4_pd.processEntry3_table_add_with_noequ0_processentry3(
    p4_pd.processEntry3_match_spec_t(hex_to_i32(0), hex_to_i32(0x00000000)), 1
)
p4_pd.processEntry4_table_add_with_processentry4(
    p4_pd.processEntry4_match_spec_t(hex_to_i32(0), hex_to_i32(0xFFFFFFFF)), 1,
)
p4_pd.processEntry4_table_add_with_noequ0_processentry4(
    p4_pd.processEntry4_match_spec_t(hex_to_i32(0), hex_to_i32(0x00000000)), 1
)
p4_pd.processEntry5_table_add_with_processentry5(
    p4_pd.processEntry5_match_spec_t(hex_to_i32(0), hex_to_i32(0xFFFFFFFF)), 1,
)
p4_pd.processEntry5_table_add_with_noequ0_processentry5(
    p4_pd.processEntry5_match_spec_t(hex_to_i32(0), hex_to_i32(0x00000000)), 1
)
p4_pd.processEntry6_table_add_with_processentry6(
    p4_pd.processEntry6_match_spec_t(hex_to_i32(0), hex_to_i32(0xFFFFFFFF)), 1,
)
p4_pd.processEntry6_table_add_with_noequ0_processentry6(
    p4_pd.processEntry6_match_spec_t(hex_to_i32(0), hex_to_i32(0x00000000)), 1
)
p4_pd.processEntry7_table_add_with_processentry7(
    p4_pd.processEntry7_match_spec_t(hex_to_i32(0), hex_to_i32(0xFFFFFFFF)), 1,
)
p4_pd.processEntry7_table_add_with_noequ0_processentry7(
    p4_pd.processEntry7_match_spec_t(hex_to_i32(0), hex_to_i32(0x00000000)), 1
)
p4_pd.processEntry8_table_add_with_processentry8(
    p4_pd.processEntry8_match_spec_t(hex_to_i32(0), hex_to_i32(0xFFFFFFFF)), 1,
)
p4_pd.processEntry8_table_add_with_noequ0_processentry8(
    p4_pd.processEntry8_match_spec_t(hex_to_i32(0), hex_to_i32(0x00000000)), 1
)
p4_pd.processEntry9_table_add_with_processentry9(
    p4_pd.processEntry9_match_spec_t(hex_to_i32(0), hex_to_i32(0xFFFFFFFF)), 1,
)
p4_pd.processEntry9_table_add_with_noequ0_processentry9(
    p4_pd.processEntry9_match_spec_t(hex_to_i32(0), hex_to_i32(0x00000000)), 1
)
p4_pd.processEntry10_table_add_with_processentry10(
    p4_pd.processEntry10_match_spec_t(hex_to_i32(0), hex_to_i32(0xFFFFFFFF)), 1,
)
p4_pd.processEntry10_table_add_with_noequ0_processentry10(
    p4_pd.processEntry10_match_spec_t(hex_to_i32(0), hex_to_i32(0x00000000)), 1
)
p4_pd.processEntry11_table_add_with_processentry11(
    p4_pd.processEntry11_match_spec_t(hex_to_i32(0), hex_to_i32(0xFFFFFFFF)), 1,
)
p4_pd.processEntry11_table_add_with_noequ0_processentry11(
    p4_pd.processEntry11_match_spec_t(hex_to_i32(0), hex_to_i32(0x00000000)), 1
)
p4_pd.processEntry12_table_add_with_processentry12(
    p4_pd.processEntry12_match_spec_t(hex_to_i32(0), hex_to_i32(0xFFFFFFFF)), 1,
)
p4_pd.processEntry12_table_add_with_noequ0_processentry12(
    p4_pd.processEntry12_match_spec_t(hex_to_i32(0), hex_to_i32(0x00000000)), 1
)
p4_pd.processEntry13_table_add_with_processentry13(
    p4_pd.processEntry13_match_spec_t(hex_to_i32(0), hex_to_i32(0xFFFFFFFF)), 1,
)
p4_pd.processEntry13_table_add_with_noequ0_processentry13(
    p4_pd.processEntry13_match_spec_t(hex_to_i32(0), hex_to_i32(0x00000000)), 1
)
p4_pd.processEntry14_table_add_with_processentry14(
    p4_pd.processEntry14_match_spec_t(hex_to_i32(0), hex_to_i32(0xFFFFFFFF)), 1,
)
p4_pd.processEntry14_table_add_with_noequ0_processentry14(
    p4_pd.processEntry14_match_spec_t(hex_to_i32(0), hex_to_i32(0x00000000)), 1
)
p4_pd.processEntry15_table_add_with_processentry15(
    p4_pd.processEntry15_match_spec_t(hex_to_i32(0), hex_to_i32(0xFFFFFFFF)), 1,
)
p4_pd.processEntry15_table_add_with_noequ0_processentry15(
    p4_pd.processEntry15_match_spec_t(hex_to_i32(0), hex_to_i32(0x00000000)), 1
)
p4_pd.processEntry16_table_add_with_processentry16(
    p4_pd.processEntry16_match_spec_t(hex_to_i32(0), hex_to_i32(0xFFFFFFFF)), 1,
)
p4_pd.processEntry16_table_add_with_noequ0_processentry16(
    p4_pd.processEntry16_match_spec_t(hex_to_i32(0), hex_to_i32(0x00000000)), 1
)
p4_pd.processEntry17_table_add_with_processentry17(
    p4_pd.processEntry17_match_spec_t(hex_to_i32(0), hex_to_i32(0xFFFFFFFF)), 1,
)
p4_pd.processEntry17_table_add_with_noequ0_processentry17(
    p4_pd.processEntry17_match_spec_t(hex_to_i32(0), hex_to_i32(0x00000000)), 1
)
p4_pd.processEntry18_table_add_with_processentry18(
    p4_pd.processEntry18_match_spec_t(hex_to_i32(0), hex_to_i32(0xFFFFFFFF)), 1,
)
p4_pd.processEntry18_table_add_with_noequ0_processentry18(
    p4_pd.processEntry18_match_spec_t(hex_to_i32(0), hex_to_i32(0x00000000)), 1
)
p4_pd.processEntry19_table_add_with_processentry19(
    p4_pd.processEntry19_match_spec_t(hex_to_i32(0), hex_to_i32(0xFFFFFFFF)), 1,
)
p4_pd.processEntry19_table_add_with_noequ0_processentry19(
    p4_pd.processEntry19_match_spec_t(hex_to_i32(0), hex_to_i32(0x00000000)), 1
)
p4_pd.processEntry20_table_add_with_processentry20(
    p4_pd.processEntry20_match_spec_t(hex_to_i32(0), hex_to_i32(0xFFFFFFFF)), 1,
)
p4_pd.processEntry20_table_add_with_noequ0_processentry20(
    p4_pd.processEntry20_match_spec_t(hex_to_i32(0), hex_to_i32(0x00000000)), 1
)
p4_pd.processEntry21_table_add_with_processentry21(
    p4_pd.processEntry21_match_spec_t(hex_to_i32(0), hex_to_i32(0xFFFFFFFF)), 1,
)
p4_pd.processEntry21_table_add_with_noequ0_processentry21(
    p4_pd.processEntry21_match_spec_t(hex_to_i32(0), hex_to_i32(0x00000000)), 1
)
p4_pd.processEntry22_table_add_with_processentry22(
    p4_pd.processEntry22_match_spec_t(hex_to_i32(0), hex_to_i32(0xFFFFFFFF)), 1,
)
p4_pd.processEntry22_table_add_with_noequ0_processentry22(
    p4_pd.processEntry22_match_spec_t(hex_to_i32(0), hex_to_i32(0x00000000)), 1
)
p4_pd.processEntry23_table_add_with_processentry23(
    p4_pd.processEntry23_match_spec_t(hex_to_i32(0), hex_to_i32(0xFFFFFFFF)), 1,
)
p4_pd.processEntry23_table_add_with_noequ0_processentry23(
    p4_pd.processEntry23_match_spec_t(hex_to_i32(0), hex_to_i32(0x00000000)), 1
)
p4_pd.processEntry24_table_add_with_processentry24(
    p4_pd.processEntry24_match_spec_t(hex_to_i32(0), hex_to_i32(0xFFFFFFFF)), 1,
)
p4_pd.processEntry24_table_add_with_noequ0_processentry24(
    p4_pd.processEntry24_match_spec_t(hex_to_i32(0), hex_to_i32(0x00000000)), 1
)
p4_pd.processEntry25_table_add_with_processentry25(
    p4_pd.processEntry25_match_spec_t(hex_to_i32(0), hex_to_i32(0xFFFFFFFF)), 1,
)
p4_pd.processEntry25_table_add_with_noequ0_processentry25(
    p4_pd.processEntry25_match_spec_t(hex_to_i32(0), hex_to_i32(0x00000000)), 1
)
p4_pd.processEntry26_table_add_with_processentry26(
    p4_pd.processEntry26_match_spec_t(hex_to_i32(0), hex_to_i32(0xFFFFFFFF)), 1,
)
p4_pd.processEntry26_table_add_with_noequ0_processentry26(
    p4_pd.processEntry26_match_spec_t(hex_to_i32(0), hex_to_i32(0x00000000)), 1
)
p4_pd.processEntry27_table_add_with_processentry27(
    p4_pd.processEntry27_match_spec_t(hex_to_i32(0), hex_to_i32(0xFFFFFFFF)), 1,
)
p4_pd.processEntry27_table_add_with_noequ0_processentry27(
    p4_pd.processEntry27_match_spec_t(hex_to_i32(0), hex_to_i32(0x00000000)), 1
)
p4_pd.processEntry28_table_add_with_processentry28(
    p4_pd.processEntry28_match_spec_t(hex_to_i32(0), hex_to_i32(0xFFFFFFFF)), 1,
)
p4_pd.processEntry28_table_add_with_noequ0_processentry28(
    p4_pd.processEntry28_match_spec_t(hex_to_i32(0), hex_to_i32(0x00000000)), 1
)
p4_pd.processEntry29_table_add_with_processentry29(
    p4_pd.processEntry29_match_spec_t(hex_to_i32(0), hex_to_i32(0xFFFFFFFF)), 1,
)
p4_pd.processEntry29_table_add_with_noequ0_processentry29(
    p4_pd.processEntry29_match_spec_t(hex_to_i32(0), hex_to_i32(0x00000000)), 1
)
p4_pd.processEntry30_table_add_with_processentry30(
    p4_pd.processEntry30_match_spec_t(hex_to_i32(0), hex_to_i32(0xFFFFFFFF)), 1,
)
p4_pd.processEntry30_table_add_with_noequ0_processentry30(
    p4_pd.processEntry30_match_spec_t(hex_to_i32(0), hex_to_i32(0x00000000)), 1
)
p4_pd.processEntry31_table_add_with_processentry31(
    p4_pd.processEntry31_match_spec_t(hex_to_i32(0), hex_to_i32(0xFFFFFFFF)), 1,
)
p4_pd.processEntry31_table_add_with_noequ0_processentry31(
    p4_pd.processEntry31_match_spec_t(hex_to_i32(0), hex_to_i32(0x00000000)), 1
)
try:
    # TODO: understand it
    # dont know why, but if group = input port,
    # then the packet followed by that packet will execute multicast
    # therefore make it 20, no 20th port is used.
    mcg_all  = mc.mgrp_create(999)
    mcg1  = mc.mgrp_create(998)
    mcg2  = mc.mgrp_create(997)
    # mcg3  = mc.mgrp_create(996)
except:
    print """
clean_all() does not yet support cleaning the PRE programming.
You need to restart the driver before running this script for the second time
"""
    quit()

node_all = mc.node_create(
    rid=999,
    port_map=devports_to_mcbitmap([56,48,40,32,24,16,8,0]),
    # port_map=devports_to_mcbitmap([port_of_worker[2], port_of_worker[3], port_of_worker[4],]),
    lag_map=lags_to_mcbitmap(([]))
)
mc.associate_node(mcg_all, node_all, xid=0, xid_valid=False)

node1 = mc.node_create(
    rid=998,
    # Not multicast to "0" ( 0 as bg traffic )
    port_map=devports_to_mcbitmap([56,48,40,32,24,16,8]),
    # port_map=devports_to_mcbitmap([56,48,40]),
    lag_map=lags_to_mcbitmap(([]))
)
mc.associate_node(mcg1, node1, xid=0, xid_valid=False)

node2 = mc.node_create(
    rid=997,
    # Not multicast to "0" ( 0 as bg traffic )
    # port_map=devports_to_mcbitmap([56,48,40,32,24,16,8]),
    port_map=devports_to_mcbitmap([24,16,8]),
    lag_map=lags_to_mcbitmap(([]))
)
mc.associate_node(mcg2, node2, xid=0, xid_valid=False)


conn_mgr.complete_operations()

def hex_to_i32(h):
    x = int(h, 0)
    if (x > 0xFFFFFFFF):
        raise UIn_Error("Integer cannot fit within 32 bits")
    if (x > 0x7FFFFFFF): x-= 0x100000000
    return x