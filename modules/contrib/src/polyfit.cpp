/*M///////////////////////////////////////////////////////////////////////////////////////
//
//  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
//
//  By downloading, copying, installing or using the software you agree to this license.
//  If you do not agree to this license, do not download, install,
//  copy or use the software.
//
//
//                           License Agreement
//                For Open Source Computer Vision Library
//
// Copyright (C) 2000-2008, Intel Corporation, all rights reserved.
// Copyright (C) 2009, Willow Garage Inc., all rights reserved.
// Third party copyrights are property of their respective owners.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
//   * Redistribution's of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//
//   * Redistribution's in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//
//   * The name of the copyright holders may not be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
// This software is provided by the copyright holders and contributors "as is" and
// any express or implied warranties, including, but not limited to, the implied
// warranties of merchantability and fitness for a particular purpose are disclaimed.
// In no event shall the Intel Corporation or contributors be liable for any direct,
// indirect, incidental, special, exemplary, or consequential damages
// (including, but not limited to, procurement of substitute goods or services;
// loss of use, data, or profits; or business interruption) however caused
// and on any theory of liability, whether in contract, strict liability,
// or tort (including negligence or otherwise) arising in any way out of
// the use of this software, even if advised of the possibility of such damage.
//
//M*/

// This original code was written by
//  Onkar Raut
//  Graduate Student,
//  University of North Carolina at Charlotte

#include "precomp.hpp"

typedef double polyfit_type;

void cv::polyfit(const Mat& src_x, const Mat& src_y, Mat& dst, int order, bool outDoublePrecision = 0)
{
    CV_Assert(src_x.rows == src_y.rows && src_x.cols ==1 && src_y.cols == 1 && order >= 1 && src_x.channels() == 1 && src_y.channels() == 1);

    Mat srcX, srcY;
    if(src_y.depth() != CV_MAKETYPE(DataType<polyfit_type>::depth, 1))
        src_y.convertTo(srcY, CV_MAKETYPE(DataType<polyfit_type>::depth, 1));
    else srcY = src_y;

    if(src_x.depth() != CV_MAKETYPE(DataType<polyfit_type>::depth, 1))
        src_x.convertTo(srcX, CV_MAKETYPE(DataType<polyfit_type>::depth, 1));
    else srcX = src_x;

    int npoints;
    CV_Assert((npoints = srcX.checkVector(1,CV_MAKETYPE(DataType<polyfit_type>::depth, 1),false)) >= (order+1));

    Mat X = Mat::zeros(order + 1, npoints, CV_MAKETYPE(DataType<polyfit_type>::depth, 1));
    polyfit_type* pSrcX = (polyfit_type*)srcX.data;
    polyfit_type* pXData = (polyfit_type*)X.data;
    int stepX = (int)(X.step/X.elemSize1());
    for (int y = 0; y < order + 1; ++y)
    {
        for (int x = 0; x < npoints; ++x)
        {
            if (y == 0)
                pXData[x] = 1;
            else if (y == 1)
                pXData[x + stepX] = pSrcX[x];
            else pXData[x + y*stepX] = pSrcX[x]* pXData[x + (y-1)*stepX];
        }
    }

    Mat X_t, X_inv;
    transpose(X,X_t);
    Mat temp = X*X_t;
    Mat temp2;
    invert (temp,temp2);
    Mat temp3 = temp2*X;
    
    Mat W = temp3*srcY;
    if (!outDoublePrecision)
		W.convertTo(W, CV_32F);
    W.copyTo(dst);
}
