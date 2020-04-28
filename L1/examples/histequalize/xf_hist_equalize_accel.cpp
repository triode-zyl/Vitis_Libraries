/*
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
 */

#include "xf_hist_equalize_config.h"

static constexpr int __XF_DEPTH=(HEIGHT*WIDTH*(XF_PIXELWIDTH(XF_8UC1,NPC_T))/8) / (INPUT_PTR_WIDTH/8);

void equalizeHist_accel(ap_uint<INPUT_PTR_WIDTH>* img_inp,
                        ap_uint<INPUT_PTR_WIDTH>* img_inp1,
                        ap_uint<OUTPUT_PTR_WIDTH>* img_out,
                        int rows,
                        int cols) {
// clang-format off
    #pragma HLS INTERFACE m_axi     port=img_inp  offset=slave bundle=gmem1 depth=__XF_DEPTH
    #pragma HLS INTERFACE m_axi     port=img_inp1  offset=slave bundle=gmem2 depth=__XF_DEPTH
    #pragma HLS INTERFACE m_axi     port=img_out  offset=slave bundle=gmem3 depth=__XF_DEPTH
    
    #pragma HLS INTERFACE s_axilite port=rows              
    #pragma HLS INTERFACE s_axilite port=cols              
    #pragma HLS INTERFACE s_axilite port=return                
    // clang-format on

    xf::cv::Mat<XF_8UC1, HEIGHT, WIDTH, NPC_T> in_mat(rows, cols);
    xf::cv::Mat<XF_8UC1, HEIGHT, WIDTH, NPC_T> in_mat1(rows,cols);
    xf::cv::Mat<XF_8UC1, HEIGHT, WIDTH, NPC_T> out_mat(rows, cols);

// clang-format off
    #pragma HLS DATAFLOW
    // clang-format on
    xf::cv::Array2xfMat<INPUT_PTR_WIDTH, XF_8UC1, HEIGHT, WIDTH, NPC_T>(img_inp, in_mat);
    xf::cv::Array2xfMat<INPUT_PTR_WIDTH, XF_8UC1, HEIGHT, WIDTH, NPC_T>(img_inp1, in_mat1);
    xf::cv::equalizeHist<XF_8UC1, HEIGHT, WIDTH, NPC_T>(in_mat, in_mat1, out_mat);
    xf::cv::xfMat2Array<OUTPUT_PTR_WIDTH, XF_8UC1, HEIGHT, WIDTH, NPC_T>(out_mat, img_out);
}

