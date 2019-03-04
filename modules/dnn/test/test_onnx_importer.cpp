// This file is part of OpenCV project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://opencv.org/license.html.

// Copyright (C) 2018, Intel Corporation, all rights reserved.
// Third party copyrights are property of their respective owners.


#include "test_precomp.hpp"
#include "npy_blob.hpp"
#include <opencv2/dnn/shape_utils.hpp>

namespace opencv_test { namespace {

template<typename TString>
static std::string _tf(TString filename)
{
    String rootFolder = "dnn/onnx/";
    return findDataFile(rootFolder + filename, false);
}

class Test_ONNX_layers : public DNNTestLayer
{
public:
    enum Extension
    {
        npy,
        pb
    };

    void testONNXModels(const String& basename, const Extension ext = npy,
                        const double l1 = 0, const float lInf = 0, const bool useSoftmax = false)
    {
        String onnxmodel = _tf("models/" + basename + ".onnx");
        Mat inp, ref;
        if (ext == npy) {
            inp = blobFromNPY(_tf("data/input_" + basename + ".npy"));
            ref = blobFromNPY(_tf("data/output_" + basename + ".npy"));
        }
        else if (ext == pb) {
            inp = readTensorFromONNX(_tf("data/input_" + basename + ".pb"));
            ref = readTensorFromONNX(_tf("data/output_" + basename + ".pb"));
        }
        else
            CV_Error(Error::StsUnsupportedFormat, "Unsupported extension");

        checkBackend(&inp, &ref);
        Net net = readNetFromONNX(onnxmodel);
        ASSERT_FALSE(net.empty());

        net.setPreferableBackend(backend);
        net.setPreferableTarget(target);

        net.setInput(inp);
        Mat out = net.forward("");

        if (useSoftmax) {
            Net netSoftmax;
            LayerParams lp;
            netSoftmax.addLayerToPrev("softmaxLayer", "Softmax", lp);

            netSoftmax.setInput(out);
            netSoftmax.setPreferableBackend(backend);
            netSoftmax.setPreferableTarget(target);
            out = netSoftmax.forward();

            netSoftmax.setInput(ref);
            netSoftmax.setPreferableBackend(DNN_BACKEND_OPENCV);
            netSoftmax.setPreferableTarget(DNN_TARGET_CPU);
            ref = netSoftmax.forward();
        }

        normAssert(ref, out, "", l1 ? l1 : default_l1, lInf ? lInf : default_lInf);
    }
};

TEST_P(Test_ONNX_layers, MaxPooling)
{
    testONNXModels("maxpooling");
    testONNXModels("two_maxpooling");
}

TEST_P(Test_ONNX_layers, Convolution)
{
    testONNXModels("convolution");

    // Reference output values are in range [-0.855, 0.611]
    double l1 = (target == DNN_TARGET_MYRIAD) ? 0.4 : 0.0;
    float lInf = (target == DNN_TARGET_MYRIAD) ? 1.36 : 0.0;
    testONNXModels("two_convolution", npy, l1, lInf);
}

TEST_P(Test_ONNX_layers, Deconvolution)
{
    testONNXModels("deconvolution");
    testONNXModels("two_deconvolution");
    testONNXModels("deconvolution_group");
}

TEST_P(Test_ONNX_layers, Dropout)
{
    testONNXModels("dropout");
}

TEST_P(Test_ONNX_layers, Linear)
{
    if (backend == DNN_BACKEND_OPENCV && target == DNN_TARGET_OPENCL_FP16)
        throw SkipTestException("");
    testONNXModels("linear");
}

TEST_P(Test_ONNX_layers, ReLU)
{
    testONNXModels("ReLU");
}

TEST_P(Test_ONNX_layers, MaxPooling_Sigmoid)
{
    testONNXModels("maxpooling_sigmoid");
}

TEST_P(Test_ONNX_layers, Concatenation)
{
    if (backend == DNN_BACKEND_INFERENCE_ENGINE &&
         (target == DNN_TARGET_OPENCL_FP16 || target == DNN_TARGET_OPENCL || target == DNN_TARGET_MYRIAD))
        throw SkipTestException("");
    testONNXModels("concatenation");
}

TEST_P(Test_ONNX_layers, AveragePooling)
{
    testONNXModels("average_pooling");
}

TEST_P(Test_ONNX_layers, BatchNormalization)
{
    testONNXModels("batch_norm");
}

TEST_P(Test_ONNX_layers, Transpose)
{
    if (backend == DNN_BACKEND_INFERENCE_ENGINE &&
         (target == DNN_TARGET_OPENCL_FP16 || target == DNN_TARGET_OPENCL || target == DNN_TARGET_MYRIAD))
        throw SkipTestException("");
    testONNXModels("transpose");
}

TEST_P(Test_ONNX_layers, Multiplication)
{
    if ((backend == DNN_BACKEND_OPENCV && target == DNN_TARGET_OPENCL_FP16) ||
        (backend == DNN_BACKEND_INFERENCE_ENGINE && target == DNN_TARGET_MYRIAD))
        throw SkipTestException("");
    testONNXModels("mul");
}

TEST_P(Test_ONNX_layers, Constant)
{
    testONNXModels("constant");
}

TEST_P(Test_ONNX_layers, Padding)
{
    testONNXModels("padding");
}

TEST_P(Test_ONNX_layers, MultyInputs)
{
    const String model =  _tf("models/multy_inputs.onnx");

    Net net = readNetFromONNX(model);
    ASSERT_FALSE(net.empty());

    net.setPreferableBackend(backend);
    net.setPreferableTarget(target);

    Mat inp1 = blobFromNPY(_tf("data/input_multy_inputs_0.npy"));
    Mat inp2 = blobFromNPY(_tf("data/input_multy_inputs_1.npy"));
    Mat ref  = blobFromNPY(_tf("data/output_multy_inputs.npy"));
    checkBackend(&inp1, &ref);

    net.setInput(inp1, "0");
    net.setInput(inp2, "1");
    Mat out = net.forward();

    normAssert(ref, out, "", default_l1,  default_lInf);
}

TEST_P(Test_ONNX_layers, DynamicReshape)
{
    if (backend == DNN_BACKEND_INFERENCE_ENGINE && (target == DNN_TARGET_OPENCL || target == DNN_TARGET_OPENCL_FP16))
        throw SkipTestException("");
    testONNXModels("dynamic_reshape");
}

INSTANTIATE_TEST_CASE_P(/*nothing*/, Test_ONNX_layers, dnnBackendsAndTargets());

class Test_ONNX_nets : public Test_ONNX_layers {};
TEST_P(Test_ONNX_nets, Alexnet)
{
    const String model =  _tf("models/alexnet.onnx");

    Net net = readNetFromONNX(model);
    ASSERT_FALSE(net.empty());

    net.setPreferableBackend(backend);
    net.setPreferableTarget(target);

    Mat inp = imread(_tf("../grace_hopper_227.png"));
    Mat ref = blobFromNPY(_tf("../caffe_alexnet_prob.npy"));
    checkBackend(&inp, &ref);

    net.setInput(blobFromImage(inp, 1.0f, Size(227, 227), Scalar(), false));
    ASSERT_FALSE(net.empty());
    Mat out = net.forward();

    normAssert(out, ref, "", default_l1,  default_lInf);
}

TEST_P(Test_ONNX_nets, Squeezenet)
{
    testONNXModels("squeezenet", pb);
}

TEST_P(Test_ONNX_nets, Googlenet)
{
    if (backend == DNN_BACKEND_INFERENCE_ENGINE)
        throw SkipTestException("");

    const String model = _tf("models/googlenet.onnx");

    Net net = readNetFromONNX(model);
    ASSERT_FALSE(net.empty());

    net.setPreferableBackend(backend);
    net.setPreferableTarget(target);

    std::vector<Mat> images;
    images.push_back( imread(_tf("../googlenet_0.png")) );
    images.push_back( imread(_tf("../googlenet_1.png")) );
    Mat inp = blobFromImages(images, 1.0f, Size(), Scalar(), false);
    Mat ref = blobFromNPY(_tf("../googlenet_prob.npy"));
    checkBackend(&inp, &ref);

    net.setInput(inp);
    ASSERT_FALSE(net.empty());
    Mat out = net.forward();

    normAssert(ref, out, "", default_l1,  default_lInf);
}

TEST_P(Test_ONNX_nets, CaffeNet)
{
    testONNXModels("caffenet", pb);
}

TEST_P(Test_ONNX_nets, RCNN_ILSVRC13)
{
    // Reference output values are in range [-4.992, -1.161]
    testONNXModels("rcnn_ilsvrc13", pb, 0.0045);
}

#ifdef OPENCV_32BIT_CONFIGURATION
TEST_P(Test_ONNX_nets, DISABLED_VGG16)  // memory usage >2Gb
#else
TEST_P(Test_ONNX_nets, VGG16)
#endif
{
    testONNXModels("vgg16", pb, default_l1, default_lInf, true);
}

#ifdef OPENCV_32BIT_CONFIGURATION
TEST_P(Test_ONNX_nets, DISABLED_VGG16_bn)  // memory usage >2Gb
#else
TEST_P(Test_ONNX_nets, VGG16_bn)
#endif
{
    const double lInf = (target == DNN_TARGET_MYRIAD) ? 0.038 : default_lInf;
    testONNXModels("vgg16-bn", pb, default_l1, lInf, true);
}

TEST_P(Test_ONNX_nets, ZFNet)
{
    testONNXModels("zfnet512", pb);
}

TEST_P(Test_ONNX_nets, ResNet18v1)
{
    // output range: [-16; 22]
    double l1 = (target == DNN_TARGET_OPENCL_FP16) ? 0.022 : default_l1;
    double lInf = (target == DNN_TARGET_OPENCL_FP16) ? 0.12 : default_lInf;
    if (target == DNN_TARGET_MYRIAD) {
        // problem layer: resnetv15_pool0_fwd - does not use pads_begin
        l1 = 1.26;
        lInf = 4.85;
    }
    testONNXModels("resnet18v1", pb, l1, lInf);
}

TEST_P(Test_ONNX_nets, ResNet50v1)
{
    // output range: [-67; 75]
    double l1 = (target == DNN_TARGET_OPENCL_FP16) ? 0.6 : 1.25e-5;
    double lInf = (target == DNN_TARGET_OPENCL_FP16) ? 0.51 : 1.2e-4;
    if (target == DNN_TARGET_MYRIAD) {
        // problem layer: resnetv17_pool0_fwd - does not use pads_begin
        l1 = 1.1;
        lInf = 5.1;
    }
    testONNXModels("resnet50v1", pb, l1, lInf);
}

TEST_P(Test_ONNX_nets, ResNet101_DUC_HDC)
{
    if (target == DNN_TARGET_OPENCL_FP16 || target == DNN_TARGET_OPENCL
                || target == DNN_TARGET_MYRIAD) {
        throw SkipTestException("");
    }
    testONNXModels("resnet101_duc_hdc", pb);
}

TEST_P(Test_ONNX_nets, TinyYolov2)
{
    if (cvtest::skipUnstableTests ||
        (backend == DNN_BACKEND_INFERENCE_ENGINE && (target == DNN_TARGET_OPENCL || target == DNN_TARGET_OPENCL_FP16))) {
        throw SkipTestException("");
    }
    // output range: [-11; 8]
    double l1 = (target == DNN_TARGET_OPENCL_FP16) ? 0.017 : default_l1;
    double lInf = (target == DNN_TARGET_OPENCL_FP16) ? 0.14 : default_lInf;
    if (target == DNN_TARGET_MYRIAD) {
        l1 = 1.93;
        lInf = 9.7;
    }
    testONNXModels("tiny_yolo2", pb, l1, lInf);
}

TEST_P(Test_ONNX_nets, CNN_MNIST)
{
    testONNXModels("cnn_mnist", pb, default_l1, default_lInf, true);
}

TEST_P(Test_ONNX_nets, MobileNet_v2)
{
    testONNXModels("mobilenetv2", pb, default_l1, default_lInf, true);
}

TEST_P(Test_ONNX_nets, LResNet100E_IR)
{
    if (backend == DNN_BACKEND_INFERENCE_ENGINE &&
         (target == DNN_TARGET_OPENCL_FP16 || target == DNN_TARGET_OPENCL || target == DNN_TARGET_MYRIAD))
        throw SkipTestException("");

    double l1 = default_l1;
    double lInf = default_lInf;
    // output range: [-3; 3]
    if (backend == DNN_BACKEND_OPENCV && target == DNN_TARGET_OPENCL_FP16) {
        l1 = 0.009;
        lInf = 0.035;
    }
    else if (backend == DNN_BACKEND_INFERENCE_ENGINE && target == DNN_TARGET_CPU) {
        l1 = 4.5e-5;
        lInf = 1.9e-4;
    }
    testONNXModels("LResNet100E_IR", pb, l1, lInf);
}

TEST_P(Test_ONNX_nets, Emotion_ferplus)
{
    double l1 = default_l1;
    double lInf = default_lInf;
    // Output values are in range [-2.011, 2.111]
    if (backend == DNN_BACKEND_OPENCV && target == DNN_TARGET_OPENCL_FP16)
        l1 = 0.007;
    else if (backend == DNN_BACKEND_INFERENCE_ENGINE && target == DNN_TARGET_OPENCL_FP16)
    {
        l1 = 0.021;
        lInf = 0.034;
    }
    else if (backend == DNN_BACKEND_INFERENCE_ENGINE && (target == DNN_TARGET_CPU || target == DNN_TARGET_OPENCL)) {
        l1 = 2.4e-4;
        lInf = 6e-4;
    }
    else if (backend == DNN_BACKEND_INFERENCE_ENGINE && target == DNN_TARGET_MYRIAD) {
        l1 = 0.06;
        lInf = 0.16;
    }
    testONNXModels("emotion_ferplus", pb, l1, lInf);
}

TEST_P(Test_ONNX_nets, Inception_v2)
{
    if (backend == DNN_BACKEND_INFERENCE_ENGINE)
        throw SkipTestException("");

    testONNXModels("inception_v2", pb, default_l1, default_lInf, true);
}

TEST_P(Test_ONNX_nets, DenseNet121)
{
    testONNXModels("densenet121", pb, default_l1, default_lInf, true);
}

TEST_P(Test_ONNX_nets, Inception_v1)
{
#if defined(INF_ENGINE_RELEASE) && INF_ENGINE_RELEASE >= 2018050000
    if (backend == DNN_BACKEND_INFERENCE_ENGINE && target == DNN_TARGET_MYRIAD)
        throw SkipTestException("Test is disabled for OpenVINO 2018R5");
#endif
    testONNXModels("inception_v1", pb);
}

TEST_P(Test_ONNX_nets, Shufflenet)
{
    if (backend == DNN_BACKEND_INFERENCE_ENGINE &&
         (target == DNN_TARGET_OPENCL_FP16 || target == DNN_TARGET_OPENCL || target == DNN_TARGET_MYRIAD))
        throw SkipTestException("");
    testONNXModels("shufflenet", pb);
}

INSTANTIATE_TEST_CASE_P(/**/, Test_ONNX_nets, dnnBackendsAndTargets());

}} // namespace
