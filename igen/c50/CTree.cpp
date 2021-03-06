//
// Created by KH on 3/6/2020.
//

#include "CTree.h"

#include <igen/CoverageStore.h>

namespace igen {


CTree::CTree(PMutContext ctx) :
        Object(move(ctx)),
        t_freq{dom()->create_vec_vars_values_sm<int>(), dom()->create_vec_vars_values_sm<int>()},
        t_info(dom()->create_vec_vars<double>()), t_gain(dom()->create_vec_vars<double>()) {
}

void CTree::prepare_data(const PLocation &loc) {
    CHECK_NE(loc, nullptr);
    CHECK_NE(loc->id(), -1);
    CHECK(root_ == nullptr);
    hit_configs().reserve((size_t) cov()->n_configs()), miss_configs().reserve((size_t) cov()->n_configs());
    auto it = loc->cov_by_ids().begin(), it_end = loc->cov_by_ids().end();
    for (const auto &c : cov()->configs()) {
        if (it != it_end && *it == c->id()) {
            ++it;
            hit_configs().emplace_back(c);
        } else {
            miss_configs().emplace_back(c);
        }
    }
    default_hit_ = hit_configs().size() > miss_configs().size();
    n_cases_ = (int) cov()->configs().size();

    // PREPARE VALS
    multi_val_ = true;
    for (const auto &c : *dom()) {
        multi_val_ = (c->n_values() >= 0.3 * (n_cases_ + 1));
    }

    // MIN THRES
    if ((n_cases_ + 1) / 2 <= 500) {
        avgain_wt_ = 1.0;
        mdl_wt_ = 0.0;
    } else if ((n_cases_ + 1) / 2 >= 1000) {
        avgain_wt_ = 0.0;
        mdl_wt_ = 0.9;
    } else {
        double frac = ((n_cases_ + 1) / 2 - 500) / 500.0;
        avgain_wt_ = 1 - frac;
        mdl_wt_ = 0.9 * frac;
    }
}

void CTree::build_tree() {
    root_ = new CNode(this, nullptr, 0, {miss_configs(), hit_configs()});
    root_->evaluate_split();
}

void CTree::cleanup() {
    configs_ = {};
    root_ = {};
    interpreter_.clear();
    cached_hash_ = hash128_empty;
    n_new_configs_ = {0, 0};
    default_hit_ = {};
    multi_val_ = {};
    n_cases_ = {};
    avgain_wt_ = {}, mdl_wt_ = {};
}

std::pair<bool, int> CTree::test_config(const PConfig &conf) const {
    CHECK_NE(root_, nullptr);
    return root_->test_config(conf);
}

std::pair<bool, int> CTree::test_add_config(const PConfig &conf, bool val) {
    CHECK_NE(root_, nullptr);
    n_new_configs_[val]++;
    return root_->test_add_config(conf, val);
}

z3::expr CTree::build_zexpr(ExprStrat strat) const {
    CHECK_NE(root_, nullptr);
    switch (strat) {
        case FreeMix:
            return root_->build_zexpr_mixed();
        case DisjOfConj: {
            z3::expr_vector res(ctx_mut()->zctx());
            root_->build_zexpr_disj_conj(res, ctx()->ztrue());
            return z3::mk_or(res);
        }
        default:
            CHECK(0);
    }
    abort();
}

std::ostream &CTree::print_tree(std::ostream &output) const {
    CHECK(root_ != nullptr);
    str prefix;
    root_->print_node(output, prefix);
    return output;
}

std::ostream &operator<<(std::ostream &output, const CTree &t) {
    return t.print_tree(output);
}

std::ostream &CTree::serialize(std::ostream &out) const {
    CHECK(root_ != nullptr);
    bool last_is_char = false;
    return root_->serialize(out, last_is_char);
}

std::istream &CTree::deserialize(std::istream &inp) {
    CHECK(root_ == nullptr);
    configs_[0].clear(), configs_[0].resize(1);
    root_ = new CNode(this, nullptr, 0, {configs_[0], configs_[0]});
    return root_->deserialize(inp);
}

void CTree::build_interpreter() {
    // CHECK(interpreter_.empty());
    if (!interpreter_.empty()) return;
    interpreter_.reserve(2 * (hit_configs().size() + miss_configs().size()));
    root_->build_interpreter(interpreter_);
    // interpreter_.shrink_to_fit();
}

bool CTree::interpret(const Config &conf) const {
    int it = 0;
    for (;;) {
        CHECK(0 <= it && it < sz(interpreter_));
        int instr = interpreter_[it];
        if (instr == kInterpretHit) return true;
        else if (instr == kInterpretMiss) return false;
        int v = conf.get(instr);
        if (v == 0) it += dom()->n_values(instr);
        else it = interpreter_[it + v];
    }
    CHECK(0);
}

bool CTree::interpret_add(const Config &conf) {
    bool val = interpret(conf);
    n_new_configs_[val]++;
    return val;
}

hash_t CTree::hash(bool bypass_cache) {
    if (!bypass_cache && cached_hash_ != hash128_empty)
        return cached_hash_;

    build_interpreter();
    return cached_hash_ = calc_hash_128(interpreter_);
}

// =====================================new CNode================================================================================

vec<PConfig> CTree::gather_small_leaves(int min_confs, int max_confs) const {
    CHECK(root_ != nullptr);
    vec<PConfig> res;
    PMutConfig curtmpl = new Config(ctx_mut());
    curtmpl->set_all(-1);
    root_->gather_small_leaves(res, min_confs, max_confs, curtmpl);
    return res;
}

void CTree::gather_nodes(vec<PCNode> &res, int min_confs, int max_confs) const {
    CHECK(root_ != nullptr);
    root_->gather_leaves_nodes(res, min_confs, max_confs);
}

}