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
// Copyright (C) 2014, Itseez, Inc, all rights reserved.
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

#include "test_precomp.hpp"

#ifndef DEBUG_IMAGES
#define DEBUG_IMAGES 0
#endif

using namespace cv;
using namespace std;

static string getTestCaseName(const string& picture_name, double minDist, double edgeThreshold, double accumThreshold, int minRadius, int maxRadius)
{
    string results_name = format("circles_%s_%.0f_%.0f_%.0f_%d_%d",
        picture_name.c_str(), minDist, edgeThreshold, accumThreshold, minRadius, maxRadius);
    string temp(results_name);
    size_t pos = temp.find_first_of("\\/.");
    while (pos != string::npos) {
        temp.replace(pos, 1, "_");
        pos = temp.find_first_of("\\/.");
    }
    return temp;
}

#if DEBUG_IMAGES
static void highlightCircles(const string& imagePath, const vector<Vec3f>& circles, const string& outputImagePath)
{
    Mat imgDebug = imread(imagePath, IMREAD_COLOR);
    const Scalar yellow(0, 255, 255);

    for (vector<Vec3f>::const_iterator iter = circles.begin(); iter != circles.end(); ++iter)
    {
        const Vec3f& circle = *iter;
        float x = circle[0];
        float y = circle[1];
        float r = max(circle[2], 2.0f);
        cv::circle(imgDebug, Point(int(x), int(y)), int(r), yellow);
    }
    imwrite(outputImagePath, imgDebug);
}
#endif

typedef std::tr1::tuple<string, double, double, double, int, int> Image_MinDist_EdgeThreshold_AccumThreshold_MinRadius_MaxRadius_t;
class HoughCirclesTestFixture : public testing::TestWithParam<Image_MinDist_EdgeThreshold_AccumThreshold_MinRadius_MaxRadius_t>
{
    string picture_name;
    double minDist;
    double edgeThreshold;
    double accumThreshold;
    int minRadius;
    int maxRadius;

public:
    HoughCirclesTestFixture()
    {
        picture_name = std::tr1::get<0>(GetParam());
        minDist = std::tr1::get<1>(GetParam());
        edgeThreshold = std::tr1::get<2>(GetParam());
        accumThreshold = std::tr1::get<3>(GetParam());
        minRadius = std::tr1::get<4>(GetParam());
        maxRadius = std::tr1::get<5>(GetParam());
    }

    HoughCirclesTestFixture(const string& picture, double minD, double edge, double accum, int minR, int maxR) :
        picture_name(picture), minDist(minD), edgeThreshold(edge), accumThreshold(accum), minRadius(minR), maxRadius(maxR)
    {
    }

    void run_test()
    {
        string test_case_name = getTestCaseName(picture_name, minDist, edgeThreshold, accumThreshold, minRadius, maxRadius);
        string filename = cvtest::TS::ptr()->get_data_path() + picture_name;
        Mat src = imread(filename, IMREAD_GRAYSCALE);
        EXPECT_FALSE(src.empty()) << "Invalid test image: " << filename;

        GaussianBlur(src, src, Size(9, 9), 2, 2);

        vector<Vec3f> circles;
        const double dp = 1.0;
        HoughCircles(src, circles, CV_HOUGH_GRADIENT, dp, minDist, edgeThreshold, accumThreshold, minRadius, maxRadius);

        string imgProc = string(cvtest::TS::ptr()->get_data_path()) + "imgproc/";
#if DEBUG_IMAGES
        highlightCircles(filename, circles, imgProc + test_case_name + ".png");
#endif

        string xml = imgProc + "HoughCircles.xml";
        FileStorage fs(xml, FileStorage::READ);
        FileNode node = fs[test_case_name];
        if (node.empty())
        {
            fs.release();
            fs.open(xml, FileStorage::APPEND);
            EXPECT_TRUE(fs.isOpened()) << "Cannot open sanity data file: " << xml;
            fs << test_case_name << circles;
            fs.release();
            fs.open(xml, FileStorage::READ);
            EXPECT_TRUE(fs.isOpened()) << "Cannot open sanity data file: " << xml;
        }

        vector<Vec3f> exp_circles;
        read(fs[test_case_name], exp_circles, vector<Vec3f>());
        fs.release();

        EXPECT_EQ(exp_circles.size(), circles.size());
    }
};

TEST_P(HoughCirclesTestFixture, regression)
{
    run_test();
}

INSTANTIATE_TEST_CASE_P(ImgProc, HoughCirclesTestFixture, testing::Combine(
    // picture_name:
    testing::Values("imgproc/stuff.jpg"),
    // minDist:
    testing::Values(20),
    // edgeThreshold:
    testing::Values(20),
    // accumThreshold:
    testing::Values(30),
    // minRadius:
    testing::Values(20),
    // maxRadius:
    testing::Values(200)
    ));

TEST(HoughCirclesTest, DefaultMaxRadius)
{
    string picture_name = "imgproc/stuff.jpg";
    const double dp = 1.0;
    double minDist = 20;
    double edgeThreshold = 20;
    double accumThreshold = 30;
    int minRadius = 20;
    int maxRadius = 0;

    string filename = cvtest::TS::ptr()->get_data_path() + picture_name;
    Mat src = imread(filename, IMREAD_GRAYSCALE);
    EXPECT_FALSE(src.empty()) << "Invalid test image: " << filename;

    GaussianBlur(src, src, Size(9, 9), 2, 2);

    vector<Vec3f> circles;
    HoughCircles(src, circles, CV_HOUGH_GRADIENT, dp, minDist, edgeThreshold, accumThreshold, minRadius, maxRadius);

#if DEBUG_IMAGES
    string imgProc = string(cvtest::TS::ptr()->get_data_path()) + "imgproc/";
    highlightCircles(filename, circles, imgProc + "HoughCirclesTest_DefaultMaxRadius.png");
#endif

    int maxDimension = std::max(src.rows, src.cols);

    EXPECT_GT(circles.size(), size_t(0)) << "Should find at least some circles";
    for (size_t i = 0; i < circles.size(); ++i)
    {
        EXPECT_GE(circles[i][2], minRadius) << "Radius should be >= minRadius";
        EXPECT_LE(circles[i][2], maxDimension) << "Radius should be <= max image dimension";
    }
}

TEST(HoughCirclesTest, CentersOnly)
{
    string picture_name = "imgproc/stuff.jpg";
    const double dp = 1.0;
    double minDist = 20;
    double edgeThreshold = 20;
    double accumThreshold = 30;
    int minRadius = 20;
    int maxRadius = -1;

    string filename = cvtest::TS::ptr()->get_data_path() + picture_name;
    Mat src = imread(filename, IMREAD_GRAYSCALE);
    EXPECT_FALSE(src.empty()) << "Invalid test image: " << filename;

    GaussianBlur(src, src, Size(9, 9), 2, 2);

    vector<Vec3f> circles;
    HoughCircles(src, circles, CV_HOUGH_GRADIENT, dp, minDist, edgeThreshold, accumThreshold, minRadius, maxRadius);

#if DEBUG_IMAGES
    string imgProc = string(cvtest::TS::ptr()->get_data_path()) + "imgproc/";
    highlightCircles(filename, circles, imgProc + "HoughCirclesTest_CentersOnly.png");
#endif

    EXPECT_GT(circles.size(), size_t(0)) << "Should find at least some circles";
    for (size_t i = 0; i < circles.size(); ++i)
    {
        EXPECT_EQ(circles[i][2], 0.0f) << "Did not ask for radius";
    }
}
