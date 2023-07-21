// This file is part of OpenCV project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://opencv.org/license.html.

#include "../precomp.hpp"
#include "layers_common.hpp"
#include <opencv2/dnn/shape_utils.hpp>

namespace cv
{
namespace dnn
{

/**
 * Returns the index associated with the input character.
 * - Returns a value between 0 and 25 for inputs in the range 'a' to 'z'.
 * - Returns a value between 26 and 51 for inputs in the range 'A' to 'Z'.
 * - Returns -1 for invalid input that is not in the range 'a' to 'z' or 'A' to 'Z' (the caller should handle the returned result accordingly).
 */
int64_t letterToIndex(const char ch) {
    if (ch >= 'a' && ch <= 'z') {
    return static_cast<int64_t>(ch) - 'a';
    }

    if (ch >= 'A' && ch <= 'Z') {
    return 26 + static_cast<int64_t>(ch) - 'A';
    }

    // invalid character - return error value
    return -1;
}

// Implimentation of Einsum layer is havily inflovensed by Onnxrutime as the time of writing
// Main logic from is borrowed from onnxrutime:
// https://github.com/microsoft/onnxruntime/blob/eaea34f8e29df9fb21fab675a3a895084407f306/onnxruntime/core/providers/cpu/math/einsum_utils/einsum_compute_preprocessor.cc#L8

class LayerEinsumImpl CV_FINAL : public EinsumLayer
{
public:
    // Number of inputs and outputs of the layer
    int inputSize, outputSize;

    // These hold equation subring, left hand side and right it of
    mutable String equation, lhs_eq, rhs_eq;

    // Holds token from left hand side of the equation
    mutable std::vector<String> lhs_eq_tokens;

    // Idicates if equation substring is defined in explit way such as "ij, jk->ik"
    // as opposed to "ij->"
    mutable bool explicitEquation = false;
    mutable bool is_parsed = false;

    // Stores the subscript indices for each input in the equation
    mutable std::vector<std::vector<int64_t>> inputSubscriptIndices;

    // Keeps track of the input index of the last input that had the subscript label
    // If the value is `-1`, it means the subscript label was never encountered or it appears in the output
    mutable std::vector<int64_t> subscriptIndicesToLastInput;

    // Holds the dimension value of the index corresponding to the subscript label
    // `-1` indicates that the corresponding label was not encountered at all
    mutable std::vector<int64_t> subscriptIndicesToDimValue;

    // Index corresponding to each output dim corresponding to each subscript index
    // A value of -1 means the corresponding subscript index is not found in the output
    mutable std::vector<int64_t> subscriptIndicesToOutputIndices;

    // Hold max number of alphabetic numbers
    static const size_t numOfLetters = 52;

    // Stores the count corresponding to each letter encountered
    // A value of `0` indicates that the corresponding letter hasn't been seen at all
    mutable std::array<int64_t, numOfLetters> letter2count;

    // Hold the assigned index corresponding to the letter seen
    // `-1` means the corresponding letter wasn't seen at all
    mutable std::array<int64_t, numOfLetters> letter2index;

    // Represents the count of unique subscript labels (subscript indices)
    // Example 1: For the equation 'ij, jk -> ik', num_subscript_indices_ = 3 (i, j, k)
    // Example 2: For the equation '...ij', 'jk' -> '...ik', num_subscript_indices_ = 3 (i, j, k) + number of dimensions specified by an ellipsis (across all inputs)
    mutable int64_t numLetterIndices = 0;

    // The number of dimensions that are encompassed by an "ellipsis" - "...".
    size_t numOfEllipsisDims = 0;


    // bool processEquation(const String& equation,  const LayerParams& params);
    bool processEquation(const String& equation, const std::vector<MatShape>& inputs) const;
    bool processBroadcastedDims() const;
    bool createOutputSubsctipt() const;
    bool calculateOutputShape(std::vector<MatShape>& output_dims) const;

    // constructor
    LayerEinsumImpl(const LayerParams& params)
    {
        setParamsFrom(params);
        equation = params.get<String>("equation");
        outputSize = params.get<int>("outputSize");
        inputSize  = params.get<int>("inputSize");

        // fill in vectors to avoid getting random numbers
        letter2count.fill(0);
        letter2index.fill(-1);
    }

    // getMeoryShapes
    bool getMemoryShapes(const std::vector<MatShape> &inputs,
                         const int requiredOutputs,
                         std::vector<MatShape> &outputs,
                         std::vector<MatShape> &) const CV_OVERRIDE
    {
        bool result = true;

        // Start preprocessing related to equation parsing
        // and dimention broadcasting
        if (!is_parsed)
        {
            CV_Assert(processEquation(equation, inputs));
            CV_Assert(processBroadcastedDims());
            CV_Assert(createOutputSubsctipt());
            CV_Assert(calculateOutputShape(outputs));
            is_parsed = true;
        }
        std::cout << "Shape: [";
        for(int i = 0; i < outputs[0].size(); i++){
            std::cout << outputs[0][i] << " ";
        }
        std::cout << "]";

        return result;
    }

    // forward
    void forward(InputArrayOfArrays inputs_arr, OutputArrayOfArrays outputs_arr, OutputArrayOfArrays internals_arr) CV_OVERRIDE
    {
        CV_Error(Error::StsError, "Forward is not implemented");
    }

    void parseEquation(const String& equation) const
    {
        // copy copy of an eqution, will be changed
        String eq = equation;

        // remove white spaces in the copy
        eq.erase(std::remove_if(eq.begin(), eq.end(), ::isspace), eq.end());

        // check if '->' - the output subscript label is present in the equation;
        std::size_t arrow_idx = eq.find("->");
        if (arrow_idx != std::string::npos)
        {
            // split left and righ hand sides of the equation
            lhs_eq = eq.substr(0, arrow_idx);
            rhs_eq = eq.substr(arrow_idx + 2);
            explicitEquation = true;
        } else {
            lhs_eq = eq;
        }

        // split lhs_eq by ',' - comma and put all created token - splits
        // into lhs_eq_tokens vector
        String token, comma = ",";
        size_t idx = 0;
        while(idx != String::npos){
            idx = lhs_eq.find(comma);
            token = lhs_eq.substr(0, idx);
            lhs_eq.erase(0, idx + comma.length());
            lhs_eq_tokens.push_back(token);
        }
    }
};

bool LayerEinsumImpl::calculateOutputShape(std::vector<MatShape>& output_dims) const
{
    bool result = true;
    MatShape dims; // vector to store output dimentions

    if (outputSize!= 1)
    {
        CV_Error(Error::StsError,
        cv::format("Einsum layer should only have one output, currenly [%d]", outputSize));
    }

    // Traverse through each of the subscript labels within the output subscript.
    bool middleOfEllipsis = false;
    int64_t ellipsisCharCount = 0;

    subscriptIndicesToOutputIndices.resize(numLetterIndices, -1);

    std::array<int64_t, numOfLetters> outputLetterToCount;
    outputLetterToCount.fill(0);

    int64_t outputDimCounter = 0;
    for (auto letter : rhs_eq)
    {
        if(letter == '.')
        {
            CV_Error(Error::StsNotImplemented, "Ellipsis are not supported yet");
        } else {
            if (middleOfEllipsis)
            {
                CV_Error(Error::StsError, "Encountered '.' character that is"
                " not part of output subscript");
            }

            auto letterIndex = letterToIndex(letter);

            if (letterIndex == -1)
            {
                 CV_Error(Error::StsError,
                    "The only permissible subscript labels are"
                    " lowercase letters (a-z) and uppercase letters (A-Z).");
            }

            if (outputLetterToCount[letterIndex] != 0)
            {
                CV_Error(Error::StsError,
                 "Output subscript constains repeated letters");
            }

            ++outputLetterToCount[letterIndex];

            auto mappedIndex = letter2index[letterIndex];

            if(mappedIndex == -1)
            {
                CV_Error(Error::StsError,
                "Output subscript has letters that were not encountered in the inputs");
            }

            std::cout << "Segmentation fault happens here!" << std::endl;
            // Push output dimention
            // Einsum layer only has one output vector
            dims.push_back(subscriptIndicesToDimValue[mappedIndex]);
            std::cout << "Conferm" << std::endl;

            // Reset the last input index for this subscript label
            // given that it is seen in the output and hence can't be reduced
            subscriptIndicesToLastInput[mappedIndex] = -1;
            subscriptIndicesToOutputIndices[mappedIndex] = outputDimCounter++;
        }
    }
    output_dims.clear();
    output_dims.push_back(dims);
    return result;
}

bool LayerEinsumImpl::createOutputSubsctipt() const
{
    // The explicit form requires no operation, as the output
    // would have already been parsed during the input parsing process.
    bool result = true;
    if(explicitEquation)
    {
        // Ensure that the provided explicit equation includes an ellipsis if the input contains ellipses.
        if(numOfEllipsisDims > 0)
        {
            if(rhs_eq.find("...") == std::string::npos)
            {
                CV_Error(Error::StsError,
                "Provided output subscript does not include ellipsis while Inputs subscrits constain ellipsis");
                result = false;
            }
        }
    }
    return result;
}

bool LayerEinsumImpl::processBroadcastedDims() const
{
    bool result = true;
    // Only compute this function if ellipsis "..." was found in the equation
    if (numOfEllipsisDims > 0){
        CV_Error(Error::StsError, "Ellipsis are not supperted currenly");
        result = false;
    }
    return result;
}



bool LayerEinsumImpl::processEquation(const String& equation, const std::vector<MatShape>& inputs) const
{
    bool result = true;

    // parser equation and extract tokens from the equation
    // save token to lhs_eq_tokens variable
    parseEquation(equation); // TODO: return lhs_eq_tokens

    const auto& left_eq_tokes = lhs_eq_tokens;

    // Check if number of tokens in equal to number of inputs.
    // For install "ij, jk -> ik" needs to have 2 inputs tensors
    int num_input_tensors = inputs.size();
    if (lhs_eq_tokens.size() != num_input_tensors)
    {
        CV_Error(
            Error::StsAssert,
            cv::format("Number of input tensors [%d] does not "
            "match the number of subscribts [%ld] "
            "in the input equation", num_input_tensors, lhs_eq_tokens.size())
            );
    }
    int64_t inputIdx = 0;

    // Maintains a mapping between input indices and their corresponding subscript labels for each input
    inputSubscriptIndices.reserve(num_input_tensors);

    // We allocate space for 10 values as a precaution,
    // assuming that we won't encounter any input with a rank greater than 10.
    // In such cases, the value of num_subscript_indices_ would be greater than 10.
    subscriptIndicesToLastInput.reserve(10);
    subscriptIndicesToDimValue.reserve(10);

    for (const auto& token : left_eq_tokes)
    {
        const MatShape shape = inputs[inputIdx];
        size_t rank = shape.size();
        size_t dim_count = 0;

        std::vector<int64_t> currTokenIndices;
        currTokenIndices.reserve(rank);

        // Variable to deal with "ellipsis" - '...' in the input
        bool middleOfellipsis = false;
        for (auto letter : token)
        {
            // Broadcasting based tokens are not implemented yet
            if (letter == '.')
            {
                CV_Error(Error::StsNotImplemented,
                 "Broad casting based indices are not supported currently");
            } else
            {

                if (middleOfellipsis)
                {
                    CV_Error(Error::StsAssert,
                    cv::format(
                        "Encountered '.' character that is not part of an ellipsis in the input: [%ld]",
                        inputIdx));
                }

                int letterIdx = letterToIndex(letter);
                if (letterIdx == -1)
                {
                    CV_Error(Error::StsError,
                    "The only permissible subscript labels are lowercase letters (a-z) and uppercase letters (A-Z).");
                }

                int dimValue = shape[dim_count];

                // The subscript label was not found in the global subscript label array
                // Therefore, it is added to both the local and global subscript arrays
                if(letter2count[letterIdx] == 0)
                {
                    letter2index[letterIdx] = numLetterIndices++;
                    subscriptIndicesToDimValue.push_back(dimValue);
                    subscriptIndicesToLastInput.push_back(inputIdx);

                } else {
                    // This letter has been seen in at least one other operand's subscript
                    // It must be equal unless one of them is a 1 (Numpy allows this)
                    auto mappedIndx = letter2index[letterIdx];
                    subscriptIndicesToLastInput[mappedIndx] = inputIdx;

                    if (subscriptIndicesToDimValue[mappedIndx] != dimValue)
                    {
                        if(subscriptIndicesToDimValue[mappedIndx] == 1){
                            subscriptIndicesToDimValue[mappedIndx] == dimValue;
                        } else
                        {
                            if (dimValue != 1)
                            {
                                CV_Error(Error::StsError, cv::format("Einsum operands can not be broadcasted."
                                                                     "Check input shapes/equation passed."
                                                                     "Input shape of operand [%ld]"
                                                                     " is incompatible in the dimention [%ld]."
                                                                    ,inputIdx
                                                                    ,dim_count));
                            }
                        }
                    }
                }

                ++letter2count[letterIdx];
                currTokenIndices.push_back(letter2index[letterIdx]);
                if (++dim_count > rank)
                {
                    CV_Error(Error::StsError,
                    "The Einsum subscripts string has an excessive number of subscript labels compared to the rank of the input.");
                }
            }
        }

        // When no broadcasting is requested, the number of subscript labels (dim_counter) should match the input's rank.
        if (numOfEllipsisDims == 0)
        {
            if (dim_count != rank)
            {
                CV_Error(Error::StsError,
                "The Einsum subscripts string does not contain required amount of subsprit labels and no ellipsis is provided in the input");
            }
        }

        inputSubscriptIndices.push_back(std::move(currTokenIndices));
        ++inputIdx;
    }
    return result;
}


Ptr<EinsumLayer> EinsumLayer::create(const LayerParams& params)
{
    return makePtr<LayerEinsumImpl>(params);
}


}} // namespace cv::dnn