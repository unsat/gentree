//
// Created by KH on 3/7/2020.
//

#ifndef IGEN4_CNODE_H
#define IGEN4_CNODE_H

#include <boost/range/sub_range.hpp>
#include <igen/Config.h>

namespace igen {

class CTree;

class CNode;

using PCNode = std::unique_ptr<CNode>;

class CNode {
public:
    CNode(CTree *tree, CNode *parent, std::array<boost::sub_range<vec<PConfig>>, 2> configs);

    [[nodiscard]] boost::sub_range<vec<PConfig>> &miss_configs() { return configs_[0]; }

    [[nodiscard]] const boost::sub_range<vec<PConfig>> &miss_configs() const { return configs_[0]; }

    [[nodiscard]] boost::sub_range<vec<PConfig>> &hit_configs() { return configs_[1]; }

    [[nodiscard]] const boost::sub_range<vec<PConfig>> &hit_configs() const { return configs_[1]; }

    [[nodiscard]] int n_hits() const { return int(hit_configs().size()); }

    [[nodiscard]] int n_misses() const { return int(miss_configs().size()); }

    [[nodiscard]] int n_total() const { return n_hits() + n_misses(); }

    [[nodiscard]] bool is_leaf() const { return hit_configs().empty() || miss_configs().empty(); }

    // Return true => HIT, false => MISS
    [[nodiscard]] bool leaf_value() const;

    int depth() const { return depth_; }

public:
    bool evaluate_split();

private:
    CTree *tree;
    CNode *parent;
    int depth_;
    std::array<boost::sub_range<vec<PConfig>>, 2> configs_;

    PVarDomain split_by;
    vec<PCNode> childs;

    friend class CTree;

    PDomain dom() const;

private: // TEMP DATA
    std::array<vec<vec<int>>, 2> freq;
    vec<double> inf, gain;
    double base_info, avgain;
    int bestvar;

    void calc_freq();

    void calc_inf_gain();

    void clear_all_tmp_data();

    std::ostream &print_tmp_state(std::ostream &output, const str &indent = "    ") const;
};


}

#endif //IGEN4_CNODE_H
