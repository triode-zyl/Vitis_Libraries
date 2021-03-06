/**********
 * Copyright 2019 Xilinx, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * **********/

#include <assert.h>
#include "kernel.hpp"
#include "fcnKernel.hpp"

extern "C" {

/**
 * @brief kernelOp defines the kernel util function
 *
 * @param p_DdrRd is DDR/HBM memory address used for read
 * @param p_DdrWr is DDR/HBM memory address used for write
 * @param p_Time is the stream of timestamps
 */
void kernelOp(DdrIntType* p_DdrRd, DdrIntType* p_DdrWr, hls::stream<TimeStampType::TimeType>& p_Time) {
#pragma HLS INLINE self off

    typedef KargsType::OpType KargsOpType;
    typedef xf::blas::ControlArgs ControlArgsType;
    FcnType l_fcn;
    typedef FcnType::FcnArgsType FcnArgsType;

    ///////////////////////////////////////////////////////////////////////////
    // VLIW op decoding
    ///////////////////////////////////////////////////////////////////////////
    unsigned int l_pc = 0;
    bool l_isLastOp = false;
    static const unsigned int l_tsDepth = TimeStampType::t_FifoDepth;

    // Checks for code, result, and data segment sizes
    KargsDdrInstrType l_code[BLAS_numInstr], l_res[BLAS_numInstr];
#ifndef __SYNTHESIS__
    assert(sizeof(l_code) <= (BLAS_resPage - BLAS_codePage) * BLAS_pageSizeBytes);
    assert(sizeof(l_code) <= (BLAS_dataPage - BLAS_resPage) * BLAS_pageSizeBytes);
#endif

    // Prefetch all instructions for more accurate cycle measurements
    for (unsigned int l_pc = 0; l_pc < BLAS_numInstr; ++l_pc) {
        l_code[l_pc].loadFromDdr(p_DdrRd, BLAS_codePage * DdrType::per4k() + l_pc * KargsType::getInstrWidth());
    }

    // Decode and execute
    TimeStampType::TimeType l_tsPrev = 0;
    KargsType l_kargsRes;
    for (unsigned int l_pc = 0; l_pc < BLAS_numInstr; ++l_pc) {
        KargsType l_kargs;
        KargsOpType l_op = l_kargs.loadFromInstr(l_code[l_pc]);
        switch (l_op) {
            case KargsType::OpControl: {
                ControlArgsType l_controlArgs = l_kargs.getControlArgs();
                l_isLastOp = l_controlArgs.getIsLastOp();
#ifndef __SYNTHESIS__
                assert(!l_isLastOp || (l_pc == BLAS_numInstr - 1));
#endif
                break;
            }
            case KargsType::OpFcn: {
                FcnArgsType l_fcnArgs = l_kargs.getFcnArgs();
                l_fcn.runFcn(p_DdrRd, p_DdrWr, l_fcnArgs);
                break;
            }
            default: {
#ifndef __SYNTHESIS__
                assert(false);
#endif
            }
        }

        // Collect and store cycle count
        TimeStampType::TimeType l_ts = p_Time.read();
        if (l_pc >= l_tsDepth) {
            xf::blas::InstrResArgs l_instrRes(l_tsPrev, reg(l_ts));
            l_kargsRes.setInstrResArgs(l_instrRes);
            l_kargsRes.storeToInstr(l_res[l_pc - l_tsDepth]);
        }
        l_tsPrev = reg(l_ts);
    }

    for (unsigned int l_d = 0; l_d < l_tsDepth; ++l_d) {
        TimeStampType::TimeType l_ts = p_Time.read();
        xf::blas::InstrResArgs l_instrRes(l_tsPrev, l_ts);
        l_kargsRes.setInstrResArgs(l_instrRes);
        l_kargsRes.storeToInstr(l_res[BLAS_numInstr - l_tsDepth + l_d]);
        l_tsPrev = l_ts;
    }

    // Store instruction results in DDR result segment
    for (unsigned int l_pc = 0; l_pc < BLAS_numInstr; ++l_pc) {
        l_res[l_pc].storeToDdr(p_DdrWr, BLAS_resPage * DdrType::per4k() + l_pc * KargsType::getInstrWidth());
    }
}

/**
 * @brief fcnKernel defines the kernel top function, with DDR/HBM as an interface
 *
 * @param p_DdrRd is DDR/HBM memory address used for read
 * @param p_DdrWr is DDR/HBM memory address used for write
 */
void fcnKernel(DdrIntType* p_DdrRd, DdrIntType* p_DdrWr) {
//#pragma HLS INTERFACE m_axi port = p_DdrRd offset = slave bundle = gmemm num_write_outstanding = \
    16 num_read_outstanding = 16 max_write_burst_length = 16 max_read_burst_length = 16 depth = 16 latency = 125
//#pragma HLS INTERFACE m_axi port = p_DdrWr offset = slave bundle = gmemm num_write_outstanding = \
    16 num_read_outstanding = 16 max_write_burst_length = 16 max_read_burst_length = 16 depth = 16 latency = 125

#pragma HLS INTERFACE m_axi port = p_DdrRd offset = slave bundle = gmemm
#pragma HLS INTERFACE m_axi port = p_DdrWr offset = slave bundle = gmemm
#pragma HLS INTERFACE s_axilite port = p_DdrRd bundle = control
#pragma HLS INTERFACE s_axilite port = p_DdrWr bundle = control
#pragma HLS INTERFACE s_axilite port = return bundle = control
#pragma HLS DATA_PACK variable = p_DdrRd
#pragma HLS DATA_PACK variable = p_DdrWr

    TimeStampType l_tr;
    // hls::stream<TimeStampType::OpType> l_controlStream;
    hls::stream<TimeStampType::TimeType> l_timeStream;
//#pragma HLS STREAM   variable=l_controlStream  depth=1
#pragma HLS STREAM variable = l_timeStream depth = 1

#pragma HLS DATAFLOW
    l_tr.runTs(l_timeStream);
    kernelOp(p_DdrRd, p_DdrWr, l_timeStream);
}

} // extern C
