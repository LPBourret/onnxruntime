// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "core/providers/nuphar/compiler/x86/op_ir_creator/all_ops.h"

#include "core/providers/nuphar/mti_x86/math/reduce_ops.h"
#include "core/framework/op_kernel_info.h"
#include "core/providers/common.h"

#include <algorithm>  // for sort

namespace onnxruntime {
namespace nuphar {

using ReduceVFunc = tvm::Tensor (*)(const tvm::Tensor& X,
                                    const std::vector<int64_t>& axes,
                                    bool keep_dims,
                                    int32_t vector_size,
                                    bool last_dim_aligned,
                                    int32_t fuse_dim,
                                    const std::string& name);

// This function gives a proper vector width and fuse dim for reduce
// It avoids vector_width larger than shape
// Fuse dim implies mulitple reduce axis could be fused together to form a longer vector_width
// It can avoid too small vector_width
static std::tuple<int, int> VectorWidthAndFuseDimForReduce(int natural_width,
                                                           std::vector<int64_t> axes,
                                                           const NodeArg* def) {
  int64_t rank = ShapeRank(def);
  if (rank == 0) {
    return std::make_tuple(1, 0);
  }

  int tail_size = 1;

  // reduce all
  if (axes.size() == 0) {
    for (int i = gsl::narrow_cast<int>(rank) - 1; i >= 0; --i) {
      if (ShapeHasValue(def, i)) {
        tail_size *= gsl::narrow_cast<int>(ShapeValue(def, i));
      } else {
        if (i > 0)
          return std::make_tuple(tail_size, i - 1);
        else
          return std::make_tuple(natural_width, 0);
      }

      if (tail_size >= natural_width) {
        return std::make_tuple(natural_width, i);
      }
    }

    return std::make_tuple(tail_size, 0);
  }

  //reduce last
  int j = axes.size() - 1;
  if (axes.back() == (rank - 1)) {
    for (int i = gsl::narrow_cast<int>(rank) - 1; i >= 0; --i) {
      if (ShapeHasValue(def, i) && axes[j] == gsl::narrow_cast<int64_t>(i)) {
        tail_size *= gsl::narrow_cast<int>(ShapeValue(def, i));
        if (j > 0)
          --j;
      } else {
        if (i > 0) {
          return std::make_tuple(tail_size, i - 1);
        } else {
          return std::make_tuple(natural_width, 0);
        }
      }

      if (tail_size >= natural_width) {
        return std::make_tuple(natural_width, i);
      }
    }

    return std::make_tuple(tail_size, 0);
  }

  // reduce other
  for (int i = gsl::narrow_cast<int>(rank) - 1; i >= 0; --i) {
    if (ShapeHasValue(def, i) && axes[j] != gsl::narrow_cast<int64_t>(i)) {
      tail_size *= gsl::narrow_cast<int>(ShapeValue(def, i));
      if (j > 0)
        --j;
    } else {
      if (i > 0)
        return std::make_tuple(tail_size, i - 1);
      else
        return std::make_tuple(natural_width, 0);
    }

    if (tail_size >= natural_width) {
      return std::make_tuple(natural_width, i);
    }
  }

  return std::make_tuple(tail_size, 0);
}

class FuncReduceV {
 public:
  FuncReduceV(const Node& node,
              ReduceVFunc func,
              std::function<int(int)> natural_vector,
              const NodeArg* def,
              const std::string& name) : def_(def) {
    ProtoHelperNodeContext ctx(node);
    OpNodeProtoHelper<ProtoHelperNodeContext> info(&ctx);
    axes_ = info.GetAttrsOrDefault<int64_t>("axes");
    std::sort(axes_.begin(), axes_.end());  //ReduceV requires sorted axes
    int64_t keepdims_i = 1;
    ORT_ENFORCE(info.GetAttr("keepdims", &keepdims_i).IsOK());
    keep_dims_ = (keepdims_i == 1);
    func_ = func;
    name_ = node.Name() + "_" + name;
    natural_vector_ = natural_vector;
  }

  tvm::Tensor operator()(const tvm::Tensor& X) const {
    std::vector<int64_t> axes;
    for (auto i : axes_) {
      axes.push_back(HandleNegativeAxis(i, gsl::narrow_cast<int64_t>(X->shape.size())));
    }

    auto p = VectorWidthAndFuseDimForReduce(natural_vector_(X->dtype.bits()), axes, def_);
    int vector_width = std::get<0>(p);
    int fuse_dim = std::get<1>(p);

    bool last_dim_aligned = false;
    const int64_t* p_last_dim_size = tvm::as_const_int(X->shape[X->shape.size() - 1]);

    if (p_last_dim_size != nullptr) {
      last_dim_aligned = (*p_last_dim_size) % vector_width == 0;
    }

    return func_(X, axes, keep_dims_, vector_width, last_dim_aligned, fuse_dim, name_);
  }

 private:
  std::vector<int64_t> axes_;
  bool keep_dims_;
  ReduceVFunc func_;
  std::string name_;
  std::function<int(int)> natural_vector_;
  const NodeArg* def_;
};

#define REDUCE_V_OP(name)                                                                                    \
  Status NUPHAR_TVM_X86_OP_IR_CREATOR_CLASS(name)::Evaluate(                                                 \
      const tvm::Array<tvm::Tensor>& inputs,                                                                 \
      const Node& node,                                                                                      \
      tvm_codegen::CodeGenContext& ctx_codegen,                                                              \
      tvm::Array<tvm::Tensor>& outputs) {                                                                    \
    auto natural_vector = [&](int bits) {                                                                    \
      return ctx_codegen.GetCodeGenHandle()->codegen_target->NaturalVectorWidth(bits);                       \
    };                                                                                                       \
    tvm::Tensor Y = FuncReduceV(node, &nuphar::name, natural_vector, node.InputDefs()[0], #name)(inputs[0]); \
    outputs.push_back(Y);                                                                                    \
    return Status::OK();                                                                                     \
  }

LIST_REDUCE_V_OPS()

#undef REDUCE_V_OP

}  // namespace nuphar
}  // namespace onnxruntime
