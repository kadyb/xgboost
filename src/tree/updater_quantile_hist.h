/*!
 * Copyright 2017-2022 by XGBoost Contributors
 * \file updater_quantile_hist.h
 * \brief use quantized feature values to construct a tree
 * \author Philip Cho, Tianqi Chen, Egor Smirnov
 */
#ifndef XGBOOST_TREE_UPDATER_QUANTILE_HIST_H_
#define XGBOOST_TREE_UPDATER_QUANTILE_HIST_H_

#include <xgboost/tree_updater.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "xgboost/base.h"
#include "xgboost/data.h"
#include "xgboost/json.h"

#include "hist/evaluate_splits.h"
#include "hist/histogram.h"
#include "hist/expand_entry.h"

#include "common_row_partitioner.h"
#include "constraints.h"
#include "./param.h"
#include "./driver.h"
#include "../common/random.h"
#include "../common/timer.h"
#include "../common/hist_util.h"
#include "../common/row_set.h"
#include "../common/partition_builder.h"
#include "../common/column_matrix.h"

namespace xgboost::tree {
inline BatchParam HistBatch(TrainParam const* param) {
  return {param->max_bin, param->sparse_threshold};
}

/*! \brief construct a tree using quantized feature values */
class QuantileHistMaker: public TreeUpdater {
 public:
  explicit QuantileHistMaker(Context const* ctx, ObjInfo task) : TreeUpdater(ctx), task_{task} {}
  void Configure(const Args&) override {}

  void Update(TrainParam const* param, HostDeviceVector<GradientPair>* gpair, DMatrix* dmat,
              common::Span<HostDeviceVector<bst_node_t>> out_position,
              const std::vector<RegTree*>& trees) override;

  bool UpdatePredictionCache(const DMatrix *data,
                             linalg::VectorView<float> out_preds) override;

  void LoadConfig(Json const&) override {}
  void SaveConfig(Json*) const override {}

  [[nodiscard]] char const* Name() const override { return "grow_quantile_histmaker"; }
  [[nodiscard]] bool HasNodePosition() const override { return true; }

 protected:
  // actual builder that runs the algorithm
  struct Builder {
   public:
    // constructor
    explicit Builder(const size_t n_trees, TrainParam const* param, DMatrix const* fmat,
                     ObjInfo task, Context const* ctx)
        : n_trees_(n_trees),
          param_(param),
          p_last_fmat_(fmat),
          histogram_builder_{new HistogramBuilder<CPUExpandEntry>},
          task_{task},
          ctx_{ctx},
          monitor_{std::make_unique<common::Monitor>()} {
      monitor_->Init("Quantile::Builder");
    }
    // update one tree, growing
    void UpdateTree(HostDeviceVector<GradientPair>* gpair, DMatrix* p_fmat, RegTree* p_tree,
                    HostDeviceVector<bst_node_t>* p_out_position);

    bool UpdatePredictionCache(DMatrix const* data, linalg::VectorView<float> out_preds) const;

   private:
    // initialize temp data structure
    void InitData(DMatrix* fmat, const RegTree& tree, std::vector<GradientPair>* gpair);

    size_t GetNumberOfTrees();

    CPUExpandEntry InitRoot(DMatrix* p_fmat, RegTree* p_tree,
                            const std::vector<GradientPair>& gpair_h);

    void BuildHistogram(DMatrix* p_fmat, RegTree* p_tree,
                        std::vector<CPUExpandEntry> const& valid_candidates,
                        std::vector<GradientPair> const& gpair);

    void LeafPartition(RegTree const& tree, common::Span<GradientPair const> gpair,
                       std::vector<bst_node_t>* p_out_position);

    void ExpandTree(DMatrix* p_fmat, RegTree* p_tree, const std::vector<GradientPair>& gpair_h,
                    HostDeviceVector<bst_node_t>* p_out_position);

   private:
    const size_t n_trees_;
    TrainParam const* param_;
    std::shared_ptr<common::ColumnSampler> column_sampler_{
        std::make_shared<common::ColumnSampler>()};

    std::vector<GradientPair> gpair_local_;

    std::unique_ptr<HistEvaluator<CPUExpandEntry>> evaluator_;
    std::vector<CommonRowPartitioner> partitioner_;

    // back pointers to tree and data matrix
    const RegTree* p_last_tree_{nullptr};
    DMatrix const* const p_last_fmat_;

    std::unique_ptr<HistogramBuilder<CPUExpandEntry>> histogram_builder_;
    ObjInfo task_;
    // Context for number of threads
    Context const* ctx_;

    std::unique_ptr<common::Monitor> monitor_;
  };

 protected:
  std::unique_ptr<Builder> pimpl_;
  ObjInfo task_;
};
}  // namespace xgboost::tree

#endif  // XGBOOST_TREE_UPDATER_QUANTILE_HIST_H_
