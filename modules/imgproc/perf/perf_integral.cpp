// This file is part of OpenCV project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://opencv.org/license.html.
#include "perf_precomp.hpp"

namespace opencv_test {

enum PerfSqMatDepth{
    CV_32S32S = 0,
    CV_32S32F,
    CV_32S64F,
    CV_32F32F,
    CV_32F64F,
    CV_64F64F};

CV_ENUM(SqSizes, CV_32S32S, CV_32S32F, CV_32S64F, CV_32F32F, CV_32F64F, CV_64F64F);

typedef tuple<Size, MatType, MatDepth> Size_MatType_OutMatDepth_t;
typedef perf::TestBaseWithParam<Size_MatType_OutMatDepth_t> Size_MatType_OutMatDepth;

typedef tuple<Size, MatType, SqSizes> Size_MatType_OutMatDepthArray_t;
typedef perf::TestBaseWithParam<Size_MatType_OutMatDepthArray_t> Size_MatType_OutMatDepthArray;

PERF_TEST_P(Size_MatType_OutMatDepth, integral,
            testing::Combine(
                testing::Values(TYPICAL_MAT_SIZES),
                testing::Values(CV_8UC1, CV_8UC2, CV_8UC3, CV_8UC4),
                testing::Values(CV_32S, CV_32F, CV_64F)
                )
            )
{
    Size sz = get<0>(GetParam());
    int matType = get<1>(GetParam());
    int sdepth = get<2>(GetParam());

    Mat src(sz, matType);
    Mat sum(sz, sdepth);

    declare.in(src, WARMUP_RNG).out(sum);

    TEST_CYCLE() integral(src, sum, sdepth);

    SANITY_CHECK(sum, 1e-6);
}


int vals[6][2] = {{CV_32S, CV_32S}, {CV_32S, CV_32F}, {CV_32S, CV_64F}, {CV_32F, CV_32F}, {CV_32F, CV_64F}, {CV_64F, CV_64F}};
PERF_TEST_P(Size_MatType_OutMatDepthArray, integral_sqsum,
            testing::Combine(
                testing::Values(TYPICAL_MAT_SIZES),
                testing::Values(CV_8UC1, CV_8UC2, CV_8UC3, CV_8UC4),
                testing::Values(0,1,2,3,4,5)
                )
            )
{
    Size sz = get<0>(GetParam());
    int matType = get<1>(GetParam());
    int *sizes = (int *)vals[get<2>(GetParam())];
    int sdepth = sizes[0];
    int sqdepth = sizes[1];

    Mat src(sz, matType);
    Mat sum(sz, sdepth);
    Mat sqsum(sz, sqdepth);

    declare.in(src, WARMUP_RNG).out(sum, sqsum);
    declare.time(100);

    TEST_CYCLE() integral(src, sum, sqsum, sdepth, sqdepth);

    SANITY_CHECK(sum, 1e-6);
    SANITY_CHECK(sqsum, 1e-6);

}

PERF_TEST_P( Size_MatType_OutMatDepth, integral_sqsum_tilted,
             testing::Combine(
                 testing::Values(TYPICAL_MAT_SIZES),
                 testing::Values( CV_8UC1, CV_8UC2, CV_8UC3, CV_8UC4 ),
                 testing::Values( CV_32S, CV_32F, CV_64F )
                 )
             )
{
    Size sz = get<0>(GetParam());
    int matType = get<1>(GetParam());
    int sdepth = get<2>(GetParam());

    Mat src(sz, matType);
    Mat sum(sz, sdepth);
    Mat sqsum(sz, sdepth);
    Mat tilted(sz, sdepth);

    declare.in(src, WARMUP_RNG).out(sum, sqsum, tilted);
    declare.time(100);

    TEST_CYCLE() integral(src, sum, sqsum, tilted, sdepth);

    SANITY_CHECK(sum, 1e-6);
    SANITY_CHECK(sqsum, 1e-6);
    SANITY_CHECK(tilted, 1e-6, tilted.depth() > CV_32S ? ERROR_RELATIVE : ERROR_ABSOLUTE);
}

} // namespace
