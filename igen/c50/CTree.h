//
// Created by KH on 3/6/2020.
//

#ifndef IGEN4_CTREE_H
#define IGEN4_CTREE_H

#include <klib/common.h>
#include <igen/Context.h>
#include <igen/Location.h>
#include <igen/Config.h>
#include "CNode.h"

namespace igen {

class CTree : public Object {
public:
    enum ExprStrat {
        FreeMix,
        DisjOfConj
    };

    explicit CTree(PMutContext ctx);

    void prepare_data_for_loc(const PLocation &loc);

    void build_tree();

    vec<PConfig> &miss_configs() { return configs_[0]; }

    vec<PConfig> &hit_configs() { return configs_[1]; }

    const vec<PConfig> &miss_configs() const { return configs_[0]; }

    const vec<PConfig> &hit_configs() const { return configs_[1]; }

    z3::expr build_zexpr(ExprStrat strat) const;

    std::ostream &print_tree(std::ostream &output) const;

    void cleanup() override;

public: // For gen CEX
    int n_min_cases_in_one_leaf() const { return root_->min_cases_in_one_leaf; };

    vec<PConfig> gather_small_leaves(int max_confs) const;

private:
    std::array<vec<PConfig>, 2> configs_;

    PMutCNode root_;

    bool default_hit_;
    bool multi_val_;
    int n_cases_;
    double avgain_wt_, mdl_wt_;

    friend class CNode;
};

using PCTree = ptr<const CTree>;
using PMutCTree = ptr<CTree>;

std::ostream &operator<<(std::ostream &output, const CTree &t);

}

#endif //IGEN4_CTREE_H
