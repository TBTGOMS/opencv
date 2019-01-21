// This file is part of OpenCV project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://opencv.org/license.html.
//
// Copyright (C) 2018 Intel Corporation


#include "precomp.hpp"
#include <opencv2/gapi/opencv_includes.hpp>
#include <opencv2/gapi/own/mat.hpp> //gapi::own::Mat

#include "opencv2/gapi/gmat.hpp"
#include "api/gapi_priv.hpp" // GOrigin

// cv::GMat public implementation //////////////////////////////////////////////
cv::GMat::GMat()
    : m_priv(new GOrigin(GShape::GMAT, GNode::Param()))
{
}

cv::GMat::GMat(const GNode &n, std::size_t out)
    : m_priv(new GOrigin(GShape::GMAT, n, out))
{
}

cv::GOrigin& cv::GMat::priv()
{
    return *m_priv;
}

const cv::GOrigin& cv::GMat::priv() const
{
    return *m_priv;
}

#if !defined(GAPI_STANDALONE)
cv::GMatDesc cv::descr_of(const cv::Mat &mat)
{
    return GMatDesc{mat.depth(), mat.channels(), {mat.cols, mat.rows}};
}

cv::GMatDesc cv::descr_of(const cv::UMat &mat)
{
    return GMatDesc{ mat.depth(), mat.channels(),{ mat.cols, mat.rows } };
}

std::vector<cv::GMatDesc> cv::descr_of(std::vector<cv::Mat> const& vec)
{
    std::vector<cv::GMatDesc> vec_descr;
    for(auto& mat : vec){
        vec_descr.emplace_back(descr_of(mat));
    }
    return vec_descr;
}

std::vector<cv::GMatDesc> cv::descr_of(std::vector<cv::UMat> const& vec)
{
    std::vector<cv::GMatDesc> vec_descr;
    for(auto& mat : vec){
        vec_descr.emplace_back(descr_of(mat));
    }
    return vec_descr;
}
#endif

cv::GMatDesc cv::gapi::own::descr_of(const cv::gapi::own::Mat &mat)
{
    return GMatDesc{mat.depth(), mat.channels(), {mat.cols, mat.rows}};
}

std::vector<cv::GMatDesc> cv::gapi::own::descr_of(std::vector<cv::gapi::own::Mat> const& vec)
{
    std::vector<cv::GMatDesc> vec_descr;
    for(auto& mat : vec){
        vec_descr.emplace_back(descr_of(mat));
    }
    return vec_descr;
}

namespace cv {
std::ostream& operator<<(std::ostream& os, const cv::GMatDesc &desc)
{
    switch (desc.depth)
    {
#define TT(X) case CV_##X: os << #X; break;
        TT(8U);
        TT(8S);
        TT(16U);
        TT(16S);
        TT(32S);
        TT(32F);
        TT(64F);
#undef TT
    default:
        os << "(user type "
           << std::hex << desc.depth << std::dec
           << ")";
        break;
    }

    os << "C" << desc.chan << " ";
    os << desc.size.width << "x" << desc.size.height;

    return os;
}
}
