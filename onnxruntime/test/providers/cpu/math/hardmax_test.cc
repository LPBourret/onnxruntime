// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "core/providers/cpu/math/softmax_shared.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "test/providers/provider_test_utils.h"

namespace onnxruntime {
namespace test {

static void RunTest(const std::vector<float>& x_vals,
                    const std::vector<float>& expected_vals,
                    const std::vector<int64_t>& dimensions,
                    int64_t axis = 1,
                    OpTester::ExpectResult expect_result = OpTester::ExpectResult::kExpectSuccess,
                    const std::string& expected_err_str = "") {
  OpTester test("Hardmax");

  if (axis != 1) {
    test.AddAttribute("axis", axis);
  }

  test.AddInput<float>("X", dimensions, x_vals);
  test.AddOutput<float>("Y", dimensions, expected_vals);

  std::unordered_set<std::string> excluded_eps; 
  if (axis < 0) {
    excluded_eps.insert(kNGraphExecutionProvider); // NGraph EP cannot handle negative axis values
  }

  test.Run(expect_result, expected_err_str, excluded_eps);
}

TEST(HardmaxOperator, Simple) {
  // https://github.com/onnx/onnx/blob/master/docs/Operators.md#Hardmax
  std::vector<float> x_vals = {-1.0f, 0.0f, 1.0f};
  std::vector<float> expected_vals = {0.0f, 0.0f, 1.0f};
  std::vector<int64_t> dimensions = {1, 3};

  RunTest(x_vals, expected_vals, dimensions);
}

TEST(HardmaxOperator, LargeNumber) {
  // x = np.array([[0, 1, 2, 3], [10000, 10001, 10002, 10003]]).astype(np.float32)
  // expected output[[0.0f, 0.0f, 0.0f, 1.0f],
  //                 [0.0f, 0.0f, 0.0f, 1.0f]]

  std::vector<float> x_vals = {0.0f, 1.0f, 2.0f, 3.0f, 10000.0f, 10001.0f, 10002.0f, 10003.0f};
  std::vector<float> expected_vals = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};
  std::vector<int64_t> dimensions = {2, 4};

  RunTest(x_vals, expected_vals, dimensions);
}

//np.random.seed(123) # Use a seed so we can replicate the input and expected values here and in python
//x = np.abs(np.random.randn(3, 4, 5).astype(np.float32))
static std::vector<int64_t> three_dimensions = {3, 4, 5};
static std::vector<float> x_vals_3dims = {
    1.0856307f, 0.99734545f, 0.2829785f, 1.5062947f, 0.5786002f,
    1.6514366f, 2.4266791f, 0.42891264f, 1.2659363f, 0.8667404f,
    0.6788862f, 0.09470897f, 1.4913896f, 0.638902f, 0.44398195f,
    0.43435127f, 2.20593f, 2.1867862f, 1.004054f, 0.3861864f,

    0.7373686f, 1.4907321f, 0.9358339f, 1.175829f, 1.2538806f,
    0.6377515f, 0.9071052f, 1.4286807f, 0.14006872f, 0.8617549f,
    0.25561938f, 2.798589f, 1.7715331f, 0.69987726f, 0.92746246f,
    0.17363568f, 0.002845916f, 0.6882227f, 0.87953633f, 0.28362733f,

    0.8053665f, 1.7276695f, 0.3908998f, 0.57380587f, 0.33858904f,
    0.011830495f, 2.3923652f, 0.41291216f, 0.978736f, 2.2381434f,
    1.2940853f, 1.0387882f, 1.7437122f, 0.79806274f, 0.02968323f,
    1.0693159f, 0.8907064f, 1.7548862f, 1.4956441f, 1.0693927f};

TEST(HardmaxOperator, ThreeDimsAxis0) {
  // x = <see x_vals_3dims>
  // import cntk as C
  // expected = C.hardmax(x.reshape(1,60)).eval().reshape(3, 4, 5)
  std::vector<float> expected_vals = {
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f,

      0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f,

      0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

  RunTest(x_vals_3dims, expected_vals, three_dimensions, /*axis*/ 0);
}

TEST(HardmaxOperator, ThreeDimsAxis1) {
  // x = <see x_vals_3dims>
  // import cntk as C
  // expected = C.hardmax(x.reshape(3,20)).eval().reshape(3, 4, 5)
  std::vector<float> expected_vals = {
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f,

      0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f,

      0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

  RunTest(x_vals_3dims, expected_vals, three_dimensions, /*axis*/ 1);
}

TEST(HardmaxOperator, ThreeDimsAxis2) {
  // x = <see x_vals_3dims>
  // import cntk as C
  // expected = C.hardmax(x.reshape(12,5)).eval().reshape(3, 4, 5)
  std::vector<float> expected_vals = {
      0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
      0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
      0.0f, 1.0f, 0.0f, 0.0f, 0.0f,

      0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
      0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 1.0f, 0.0f,

      0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 1.0f, 0.0f, 0.0f};

  RunTest(x_vals_3dims, expected_vals, three_dimensions, /*axis*/ 2);
}

TEST(HardmaxOperator, ThreeDimsNegAxis2) {
  // x = <see x_vals_3dims>
  // import cntk as C
  // expected = C.hardmax(x.reshape(12,5)).eval().reshape(3, 4, 5)
  std::vector<float> expected_vals = {
      0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
      0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
      0.0f, 1.0f, 0.0f, 0.0f, 0.0f,

      0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
      0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 1.0f, 0.0f,

      0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 1.0f, 0.0f, 0.0f};

  RunTest(x_vals_3dims, expected_vals, three_dimensions, /*axis*/ -1);
}

}  // namespace test
}  // namespace onnxruntime
