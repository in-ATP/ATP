/* todo define the register values as signed */

typedef bit<32> const_t;
const const_t total_aggregator_cnt = 32w0x4fff;
typedef bit<16> total_aggregator_idx_t;


/* note that in the case of that the sume is already saturateed, 
        it is possible that the max positive + negative value;*/
// && value > -2147483647 should add this;
//&& register_lo > -2147483647
// #define REG_TABLES(n)                                                                                           \
// control ProcessData##n(                                                                                         \
//         inout p4ml_h p4ml,                                                                                      \
//         inout entry_h p4ml_entries,                                                                             \
//         inout entry_h p4ml_entries1,                                                                            \
//         inout p4ml_meta_h mdata,                                                                                \
//         in p4ml_agtr_index_h p4ml_agtr_index                                                                    \
//   ){                                                                                                            \
//                                                                                                                 \
//     Register<pair_t, total_aggregator_idx_t>(total_aggregator_cnt) register##n;                                 \
//     RegisterAction<pair_t, total_aggregator_idx_t, data_type_t>(register##n) noequ0_write_read_data_entry##n= { \
//         void apply(inout pair_t value, out data_type_t rv){                                                     \
//             if(value.first[30:0] != 0x7fffffff && value.second[30:0] != 0x7fffffff){                            \
//                 value.first = value.first |+| p4ml_entries.data##n;                                             \
//                 value.second = value.second |+| p4ml_entries1.data##n;                                          \
//             }                                                                                                   \
//             rv = value.first;                                                                                   \
//         }                                                                                                       \
//     };                                                                                                          \
//     action noequ0_read_write_entry##n() {                                                                       \
//         p4ml_entries.data##n= noequ0_write_read_data_entry##n.execute(p4ml_agtr_index.agtr);                    \
//     }                                                                                                           \
//                                                                                                                 \
//     RegisterAction<pair_t, total_aggregator_idx_t, data_type_t>(register##n) write_data_entry##n= {             \
//         void apply(inout pair_t value, out data_type_t rv){                                                     \
//                 value.first = p4ml_entries.data##n;                                                             \
//                 value.second = p4ml_entries1.data##n;                                                           \
//         }                                                                                                       \
//     };                                                                                                          \
//     action processentry##n() {                                                                                  \
//         write_data_entry##n.execute(p4ml_agtr_index.agtr);                                                      \
//     }                                                                                                           \
//                                                                                                                 \
//     RegisterAction<pair_t, total_aggregator_idx_t, data_type_t>(register##n) read_first_data_entry##n= {        \
//         void apply(inout pair_t value, out data_type_t rv){                                                     \
//             rv = value.first;                                                                                   \
//         }                                                                                                       \
//     };                                                                                                          \
//     action read_first_entry##n() {                                                                              \
//       p4ml_entries.data##n= read_first_data_entry##n.execute(p4ml_agtr_index.agtr);                             \
//     }                                                                                                           \
//     RegisterAction<pair_t, total_aggregator_idx_t, data_type_t>(register##n) read_second_data_entry##n= {       \
//         void apply(inout pair_t value, out data_type_t rv){                                                     \
//             rv = value.second;                                                                                  \
//         }                                                                                                       \
//     };                                                                                                          \
//     action read_second_entry##n() {                                                                             \
//       p4ml_entries1.data##n= read_second_data_entry##n.execute(p4ml_agtr_index.agtr);                           \
//     }                                                                                                           \
//     table processEntry##n {                                                                                     \
//         key =  {                                                                                                \
//             mdata.bitmap : ternary;                                                                             \
//             // mdata.need_send_out : exact;                                                                        \
//             mdata.isAggregate : ternary;                                                                        \
//             p4ml.dataIndex : exact;                                                                             \
//         }                                                                                                       \
//         actions = {                                                                                             \
//             processentry##n;                                                                                    \
//             noequ0_read_write_entry##n;                                                                         \
//             read_first_entry##n;                                                                                \
//             read_second_entry##n;                                                                               \
//         }                                                                                                       \
//         size = 8;                                                                                               \
//     }                                                                                                           \
//     apply{                                                                                                      \
//         processEntry##n.apply();                                                                                \
//     }                                                                                                           \
//                                                                                                                 \
// }                                                                                                               \


// #define PROCESS_ENTRY                                                                      \
// process_data0.apply( p4ml, p4ml_entries, p4ml_entries1,  mdata, p4ml_agtr_index);          \
// process_data1.apply( p4ml, p4ml_entries, p4ml_entries1,  mdata, p4ml_agtr_index);          \
// process_data2.apply( p4ml, p4ml_entries, p4ml_entries1,  mdata, p4ml_agtr_index);          \
// process_data3.apply( p4ml, p4ml_entries, p4ml_entries1,  mdata, p4ml_agtr_index);          \
// process_data4.apply( p4ml, p4ml_entries, p4ml_entries1,  mdata, p4ml_agtr_index);          \
// process_data5.apply( p4ml, p4ml_entries, p4ml_entries1,  mdata, p4ml_agtr_index);          \
// process_data6.apply( p4ml, p4ml_entries, p4ml_entries1,  mdata, p4ml_agtr_index);          \
// process_data7.apply( p4ml, p4ml_entries, p4ml_entries1,  mdata, p4ml_agtr_index);          \
// process_data8.apply( p4ml, p4ml_entries, p4ml_entries1,  mdata, p4ml_agtr_index);          \
// process_data9.apply( p4ml, p4ml_entries, p4ml_entries1,  mdata, p4ml_agtr_index);          \
// process_data10.apply(p4ml, p4ml_entries, p4ml_entries1,  mdata, p4ml_agtr_index);          \
// process_data11.apply(p4ml, p4ml_entries, p4ml_entries1,  mdata, p4ml_agtr_index);          \
// process_data12.apply(p4ml, p4ml_entries, p4ml_entries1,  mdata, p4ml_agtr_index);          \
// process_data13.apply(p4ml, p4ml_entries, p4ml_entries1,  mdata, p4ml_agtr_index);          \
// process_data14.apply(p4ml, p4ml_entries, p4ml_entries1,  mdata, p4ml_agtr_index);          \
// process_data15.apply(p4ml, p4ml_entries, p4ml_entries1,  mdata, p4ml_agtr_index);          \
// process_data16.apply(p4ml, p4ml_entries, p4ml_entries1,  mdata, p4ml_agtr_index);          \
// process_data17.apply(p4ml, p4ml_entries, p4ml_entries1,  mdata, p4ml_agtr_index);          \
// process_data18.apply(p4ml, p4ml_entries, p4ml_entries1,  mdata, p4ml_agtr_index);          \
// process_data19.apply(p4ml, p4ml_entries, p4ml_entries1,  mdata, p4ml_agtr_index);          \
// process_data20.apply(p4ml, p4ml_entries, p4ml_entries1,  mdata, p4ml_agtr_index);          \
// process_data21.apply(p4ml, p4ml_entries, p4ml_entries1,  mdata, p4ml_agtr_index);          \
// process_data22.apply(p4ml, p4ml_entries, p4ml_entries1,  mdata, p4ml_agtr_index);          \
// process_data23.apply(p4ml, p4ml_entries, p4ml_entries1,  mdata, p4ml_agtr_index);          \
// process_data24.apply(p4ml, p4ml_entries, p4ml_entries1,  mdata, p4ml_agtr_index);          \
// process_data25.apply(p4ml, p4ml_entries, p4ml_entries1,  mdata, p4ml_agtr_index);          \
// process_data26.apply(p4ml, p4ml_entries, p4ml_entries1,  mdata, p4ml_agtr_index);          \
// process_data27.apply(p4ml, p4ml_entries, p4ml_entries1,  mdata, p4ml_agtr_index);          \
// process_data28.apply(p4ml, p4ml_entries, p4ml_entries1,  mdata, p4ml_agtr_index);          \
// process_data29.apply(p4ml, p4ml_entries, p4ml_entries1,  mdata, p4ml_agtr_index);          \
// process_data30.apply(p4ml, p4ml_entries, p4ml_entries1,  mdata, p4ml_agtr_index);          \
 

// REG_TABLES(0) 
// REG_TABLES(1) 
// REG_TABLES(2) 
// REG_TABLES(3) 
// REG_TABLES(4) 
// REG_TABLES(5) 
// REG_TABLES(6) 
// REG_TABLES(7) 
// REG_TABLES(8) 
// REG_TABLES(9) 
// REG_TABLES(10) 
// REG_TABLES(11) 
// REG_TABLES(12) 
// REG_TABLES(13) 
// REG_TABLES(14) 
// REG_TABLES(15) 
// REG_TABLES(16) 
// REG_TABLES(17) 
// REG_TABLES(18) 
// REG_TABLES(19) 
// REG_TABLES(20) 
// REG_TABLES(21) 
// REG_TABLES(22) 
// REG_TABLES(23) 
// REG_TABLES(24) 
// REG_TABLES(25) 
// REG_TABLES(26) 
// REG_TABLES(27) 
// REG_TABLES(28) 
// REG_TABLES(29) 
// REG_TABLES(30) 
