// This file is part of OpenCV project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://opencv.org/license.html

#include "precomp.hpp"
#include "opencl_kernels_core.hpp"

#include "convert.simd.hpp"
#include "convert.simd_declarations.hpp" // defines CV_CPU_DISPATCH_MODES_ALL=AVX2,...,BASELINE based on CMakeLists.txt content

namespace cv {

namespace hal {
void cvt16f32f(const fp16_t* src, float* dst, int len)
{
    CV_INSTRUMENT_REGION();
    CV_CPU_DISPATCH(cvt16f32f, (src, dst, len),
        CV_CPU_DISPATCH_MODES_ALL);
}
void cvt32f16f(const float* src, fp16_t* dst, int len)
{
    CV_INSTRUMENT_REGION();
    CV_CPU_DISPATCH(cvt32f16f, (src, dst, len),
        CV_CPU_DISPATCH_MODES_ALL);
}
void cvt32f16bf(const float* src, bfloat16_t* dst, int len)
{
    CV_INSTRUMENT_REGION();
    CV_CPU_DISPATCH(cvt32f16bf, (src, dst, len),
        CV_CPU_DISPATCH_MODES_ALL);
}
void addRNGBias32f(float* arr, const float* scaleBiasPairs, int len, int cn)
{
    CV_INSTRUMENT_REGION();
    CV_CPU_DISPATCH(addRNGBias32f, (arr, scaleBiasPairs, len, cn),
        CV_CPU_DISPATCH_MODES_ALL);
}
void addRNGBias64f(double* arr, const double* scaleBiasPairs, int len, int cn)
{
    CV_INSTRUMENT_REGION();
    CV_CPU_DISPATCH(addRNGBias64f, (arr, scaleBiasPairs, len, cn),
        CV_CPU_DISPATCH_MODES_ALL);
}

} // namespace


BinaryFunc getConvertFunc(int sdepth, int ddepth)
{
    CV_INSTRUMENT_REGION();
    CV_CPU_DISPATCH(getConvertFunc, (sdepth, ddepth),
        CV_CPU_DISPATCH_MODES_ALL);
}

#ifdef HAVE_OPENCL
static bool ocl_convertFp16( InputArray _src, OutputArray _dst, int sdepth, int ddepth )
{
    int type = _src.type(), cn = CV_MAT_CN(type);

    _dst.createSameSize( _src, CV_MAKETYPE(ddepth, cn) );
    int kercn = 1;
    int rowsPerWI = 1;
    String build_opt = format("-D HALF_SUPPORT -D srcT=%s -D dstT=%s -D rowsPerWI=%d%s",
                              sdepth == CV_32F ? "float" : "half",
                              sdepth == CV_32F ? "half" : "float",
                              rowsPerWI,
                              sdepth == CV_32F ? " -D FLOAT_TO_HALF " : "");
    ocl::Kernel k(sdepth == CV_32F ? "convertFp16_FP32_to_FP16" : "convertFp16_FP16_to_FP32", ocl::core::halfconvert_oclsrc, build_opt);
    if (k.empty())
        return false;

    UMat src = _src.getUMat();
    UMat dst = _dst.getUMat();

    ocl::KernelArg srcarg = ocl::KernelArg::ReadOnlyNoSize(src),
    dstarg = ocl::KernelArg::WriteOnly(dst, cn, kercn);

    k.args(srcarg, dstarg);

    size_t globalsize[2] = { (size_t)src.cols * cn / kercn, ((size_t)src.rows + rowsPerWI - 1) / rowsPerWI };
    return k.run(2, globalsize, NULL, false);
}

static bool ocl_convertTo(InputArray src_, OutputArray dst_, int ddepth, bool noScale, double alpha, double beta)
{
    CV_INSTRUMENT_REGION();

    CV_Assert(ddepth >= 0);

    int stype = src_.type();
    int sdepth = CV_MAT_DEPTH(stype);
    int cn = CV_MAT_CN(stype);

    int dtype = CV_MAKETYPE(ddepth, cn);

    int wdepth = (sdepth == CV_64F) ? CV_64F : CV_32F;

    bool needDouble = sdepth == CV_64F || ddepth == CV_64F;
    bool doubleCheck = true;
    if (needDouble)
    {
        doubleCheck = ocl::Device::getDefault().hasFP64();
    }
    bool halfCheck = true;
    bool needHalf = sdepth == CV_16F || ddepth == CV_16F;
    if (needHalf)
    {
        halfCheck = ocl::Device::getDefault().hasFP16();
    }

    if (!doubleCheck)
        return false;
    if (!halfCheck)
        return false;

    const int rowsPerWI = 4;

    char cvt[2][50];
    ocl::Kernel k("convertTo", ocl::core::convert_oclsrc,
                  format("-D srcT=%s -D WT=%s -D dstT=%s -D convertToWT=%s -D convertToDT=%s -D rowsPerWI=%d%s%s%s",
                         ocl::typeToStr(sdepth), ocl::typeToStr(wdepth), ocl::typeToStr(ddepth),
                         ocl::convertTypeStr(sdepth, wdepth, 1, cvt[0], sizeof(cvt[0])),
                         ocl::convertTypeStr(wdepth, ddepth, 1, cvt[1], sizeof(cvt[1])),
                         rowsPerWI,
                         needDouble ? " -D DOUBLE_SUPPORT" : "",
                         needHalf ? " -D HALF_SUPPORT" : "",
                         noScale ? " -D NO_SCALE" : ""
                  )
    );

    if (k.empty())
        return false;

    UMat src = src_.getUMat();
    dst_.createSameSize(src_, dtype);
    UMat dst = dst_.getUMat();

    float alphaf = (float)alpha, betaf = (float)beta;

    if (noScale)
        k.args(ocl::KernelArg::ReadOnlyNoSize(src), ocl::KernelArg::WriteOnly(dst, cn));
    else if (wdepth == CV_32F)
        k.args(ocl::KernelArg::ReadOnlyNoSize(src), ocl::KernelArg::WriteOnly(dst, cn), alphaf, betaf);
    else
        k.args(ocl::KernelArg::ReadOnlyNoSize(src), ocl::KernelArg::WriteOnly(dst, cn), alpha, beta);

    size_t globalsize[2] = {
        (size_t)dst.cols * cn,
        divUp((size_t)dst.rows, rowsPerWI)
    };
    if (!k.run(2, globalsize, NULL, false))
        return false;

    CV_IMPL_ADD(CV_IMPL_OCL);
    return true;
}
#endif

void Mat::convertTo(OutputArray dst, int type_, double alpha, double beta) const
{
    CV_INSTRUMENT_REGION();

    if (empty())
    {
        dst.release();
        return;
    }

    int stype = type();
    int sdepth = CV_MAT_DEPTH(stype);

    int ddepth = sdepth;
    if (type_ >= 0)
        ddepth = CV_MAT_DEPTH(type_);
    else
        ddepth = dst.fixedType() ? dst.depth() : sdepth;

    bool noScale = std::fabs(alpha - 1) < DBL_EPSILON && std::fabs(beta) < DBL_EPSILON;
    if (sdepth == ddepth && noScale)
    {
        copyTo(dst);
        return;
    }

    CV_OCL_RUN(dims <= 2 && dst.isUMat(),
               ocl_convertTo(*this, dst, ddepth, noScale, alpha, beta))

    int cn = channels();
    int dtype = CV_MAKETYPE(ddepth, cn);

    Mat src = *this;
    bool allowTransposed = dims == 1 ||
        dst.kind() == _InputArray::STD_VECTOR ||
        (dst.fixedSize() && dst.dims() == 1);
    dst.create( dims, size, dtype, -1, allowTransposed );
    Mat dstMat = dst.getMat();

    BinaryFunc func = noScale ? getConvertFunc(sdepth, ddepth) : getConvertScaleFunc(sdepth, ddepth);
    double scale[] = {alpha, beta};
    CV_Assert( func != 0 );

    if( dims <= 2 )
    {
        Size sz = getContinuousSize2D(src, dstMat, cn);
        func(src.data, src.step, 0, 0, dstMat.data, dstMat.step, sz, scale);
    }
    else
    {
        const Mat* arrays[] = {&src, &dstMat, 0};
        uchar* ptrs[2] = {};
        NAryMatIterator it(arrays, ptrs);
        Size sz((int)(it.size*cn), 1);

        for( size_t i = 0; i < it.nplanes; i++, ++it )
            func(ptrs[0], 1, 0, 0, ptrs[1], 1, sz, scale);
    }
}

void UMat::convertTo(OutputArray dst, int type_, double alpha, double beta) const
{
    CV_INSTRUMENT_REGION();

    if (empty())
    {
        dst.release();
        return;
    }

#ifdef HAVE_OPENCL
    int stype = type();
    int sdepth = CV_MAT_DEPTH(stype);

    int ddepth = sdepth;
    if (type_ >= 0)
        ddepth = CV_MAT_DEPTH(type_);
    else
        ddepth = dst.fixedType() ? dst.depth() : sdepth;

    bool noScale = std::fabs(alpha - 1) < DBL_EPSILON && std::fabs(beta) < DBL_EPSILON;
    if (sdepth == ddepth && noScale)
    {
        copyTo(dst);
        return;
    }

    CV_OCL_RUN(dims <= 2,
               ocl_convertTo(*this, dst, ddepth, noScale, alpha, beta))
#endif // HAVE_OPENCL

    UMat src = *this;  // Fake reference to itself.
                       // Resolves issue 8693 in case of src == dst.
    Mat m = getMat(ACCESS_READ);
    m.convertTo(dst, type_, alpha, beta);
    (void)src;
}

//==================================================================================================

void convertFp16(InputArray _src, OutputArray _dst)
{
    CV_INSTRUMENT_REGION();

    int sdepth = _src.depth(), ddepth = 0;
    BinaryFunc func = 0;

    switch( sdepth )
    {
    case CV_32F:
        if(_dst.fixedType())
        {
            ddepth = _dst.depth();
            CV_Assert(ddepth == CV_16S || ddepth == CV_16F);
            CV_Assert(_dst.channels() == _src.channels());
        }
        else
            ddepth =  CV_16S;
        func = getConvertFunc(CV_32F, CV_16F);
        break;
    case CV_16S:
    case CV_16F:
        ddepth = CV_32F;
        func = getConvertFunc(CV_16F, CV_32F);
        break;
    default:
        CV_Error(Error::StsUnsupportedFormat, "Unsupported input depth");
        return;
    }

    CV_OCL_RUN(_src.dims() <= 2 && _dst.isUMat(),
               ocl_convertFp16(_src, _dst, sdepth, ddepth))

    Mat src = _src.getMat();

    int type = CV_MAKETYPE(ddepth, src.channels());
    _dst.create( src.dims, src.size, type );
    Mat dst = _dst.getMat();
    int cn = src.channels();

    CV_Assert( func != 0 );

    if( src.dims <= 2 )
    {
        Size sz = getContinuousSize2D(src, dst, cn);
        func( src.data, src.step, 0, 0, dst.data, dst.step, sz, 0);
    }
    else
    {
        const Mat* arrays[] = {&src, &dst, 0};
        uchar* ptrs[2] = {};
        NAryMatIterator it(arrays, ptrs);
        Size sz((int)(it.size*cn), 1);

        for( size_t i = 0; i < it.nplanes; i++, ++it )
            func(ptrs[0], 0, 0, 0, ptrs[1], 0, sz, 0);
    }
}

} // namespace cv
