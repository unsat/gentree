//
// Created by KH on 3/7/2020.
//

#include "CNode.h"
#include "CTree.h"

#include <klib/math.h>
#include <boost/scope_exit.hpp>

namespace igen {


static const double Epsilon = 1E-4;

CNode::CNode(CTree *tree, CNode *parent, std::array<boost::sub_range<vec<PConfig>>, 2> configs) :
        tree(tree), parent(parent), configs_(std::move(configs)) {
    depth_ = (parent ? parent->depth() + 1 : 0);
}

PDomain CNode::dom() const {
    return tree->ctx()->dom();
}

PVarDomain CNode::dom(int var_id) const {
    return dom()->vars()[var_id];
}

void CNode::calc_freq() {
    for (int hit = 0; hit <= 1; ++hit) {
        freq[hit] = dom()->create_vec_vars_values<int>();
        for (const auto &c : configs_[hit]) {
            const auto &vals = c->values();
            for (int var_id = 0; var_id < dom()->n_vars(); ++var_id) {
                freq[hit][var_id][vals[var_id]]++;
            }
        }
    }
}

void CNode::calc_inf_gain() {
    bestvar = -1;
    double log2ntotal = log2(n_total());

    // Calc info
    info = dom()->create_vec_vars<double>();
    for (int var_id = 0; var_id < dom()->n_vars(); ++var_id) {
        double sum = 0;
        for (int val = 0; val < dom()->n_values(var_id); ++val) {
            int n = freq[0][var_id][val] + freq[1][var_id][val];
            sum += n * log2(n);
        }
        info[var_id] = log2ntotal - sum / n_total();
    }

    // Calc base_info
    double base_info = 0;
    for (int hit = 0; hit <= 1; ++hit) {
        int n = int(configs_[hit].size());
        base_info += n * log2(n);
    }
    base_info = log2ntotal - base_info / n_total();

    // Calc gain
    double total_gain = 0;
    gain = dom()->create_vec_vars<double>();
    for (int var_id = 0; var_id < dom()->n_vars(); ++var_id) {
        double &g = gain[var_id];
        for (int val = 0; val < dom()->n_values(var_id); ++val) {
            double sum = 0;
            int total = 0;
            for (int hit = 0; hit <= 1; ++hit) {
                int n = freq[hit][var_id][val];
                sum += n * log2(n);
                total += n;
            }
            sum = total * log2(total) - sum;
            g += sum;
        }
        g = base_info - g / n_total();
        total_gain += g;
    }

    // Calc avgain & mdl
    avgain = 0;
    possible = 0;
    for (int var = 0; var < dom()->n_vars(); ++var) {
        if (gain[var] >= Epsilon &&
            (tree->multi_val_ || dom(var)->n_values() < 0.3 * (tree->n_cases_ + 1))) {
            possible++;
            avgain += gain[var];
        } else {
            //Gain[Att] = None;
        }
    }

    avgain /= possible;
    mdl = log2(possible) / tree->n_cases_;
    mingain = avgain * tree->avgain_wt_ + mdl * tree->mdl_wt_;
}

int CNode::select_best_var(bool first_pass) {
    bestvar = -1;
    double bestratio = -1000;
    int bestnbr = dom()->n_all_values();

    for (int var = 0; var < dom()->n_vars(); ++var) {
        double inf = info[var];

        if (first_pass) {
            if (gain[var] < 0.999 * mingain || inf <= 0) continue;
        } else {
            if (inf <= 0) inf = Epsilon;
        }

        double val = gain[var] / inf;
        int nbr = dom()->n_values(var);

        if (val > bestratio
            || (val > 0.999 * bestratio && (nbr < bestnbr || (nbr == bestnbr && gain[var] > gain[var])))) {
            bestvar = var;
            bestratio = val;
            bestnbr = nbr;
        }
    }

    return bestvar;
}

void CNode::create_childs() {
    CHECK(0 <= bestvar && bestvar < dom()->n_vars());
    int nvalues = dom()->n_values(bestvar);
    for (int hit = 0; hit <= 1; ++hit) {
        auto &c = configs_[hit];
        std::sort(c.begin(), c.end(), [bestvar = bestvar](const auto &a, const auto &b) {
            return a->get(bestvar) < b->get(bestvar);
        });
    }

    childs.resize(nvalues);
    vec<PConfig>::iterator beg[2], end[2] = {miss_configs().begin(), hit_configs().begin()};
    vec<PConfig>::iterator real_end[2] = {miss_configs().end(), hit_configs().end()};
    for (int val = 0; val < nvalues; ++val) {
        for (int hit = 0; hit <= 1; ++hit) {
            beg[hit] = end[hit];
            while (end[hit] != real_end[hit] && (*end[hit])->value(bestvar) == val)
                end[hit]++;
        }

        boost::sub_range<vec<PConfig>> miss_conf = {beg[0], end[0]};
        boost::sub_range<vec<PConfig>> hit_conf = {beg[1], end[1]};
        childs[val] = new CNode(tree, this, {miss_conf, hit_conf});
    }
}

bool CNode::evaluate_split() {
    BOOST_SCOPE_EXIT(this_) { this_->clear_all_tmp_data(); }
    BOOST_SCOPE_EXIT_END
    if (is_leaf()) {
        GVLOG(30) << "\n<" << depth() << ">: " << n_total() << " cases\n    "
                  << (leaf_value() ? "HIT" : "MISS");
        return false;
    }

    calc_freq();
    calc_inf_gain();
    if (select_best_var(true) == -1) select_best_var(false);
    VLOG_BLOCK(30, print_tmp_state(log << "\n<" << depth() << ">: " << n_total() << " cases\n"));

    split_by = dom()->vars().at(bestvar);
    create_childs();
    for (auto &c : childs)
        c->evaluate_split();

    return split_by != nullptr;
}


void CNode::clear_all_tmp_data() {
    freq[0].clear(), freq[1].clear();
    info.clear(), gain.clear();
}

std::ostream &CNode::print_tmp_state(std::ostream &output, const str &indent) const {
    for (const auto &var : dom()->vars()) {
        if (info[var->id()] < 0 || gain[var->id()] < 0)
            continue;
        output << indent << "Var " << var->name() << ": \n";
        fmt::print(output, "{}{}[{:>4}{:>8}{:>8}]\n", indent, indent, "Val", "MISS", "HIT");
        for (int i = 0; i < var->n_values(); ++i) {
            fmt::print(output, "{}{}[{:>4}{:>8}{:>8}]\n", indent, indent,
                       i, freq[0][var->id()][i], freq[1][var->id()][i]);
        }
        fmt::print(output, "{}{}info {:.3f}, gain {:.3f}, val {:.3f}\n", indent, indent,
                   info[var->id()], gain[var->id()], gain[var->id()] / info[var->id()]);
    }
    fmt::print(output, "{}av gain={:.3f}, MDL ({}) = {:.3f}, min={:.3f}\n",
               indent, avgain, possible, mdl, mingain);
    fmt::print(output, "{}best var ", indent);
    if (bestvar == -1) {
        output << "N/A\n";
    } else {
        fmt::print(output, "{}: info {:.3f}, gain {:.3f}, val {:.3f}\n",
                   dom()->name(bestvar), info[bestvar], gain[bestvar], gain[bestvar] / info[bestvar]);
    }
    return output;
}

bool CNode::leaf_value() const {
    CHECK(is_leaf());
    if (hit_configs().empty() && miss_configs().empty())
        return tree->default_hit_;
    return !hit_configs().empty();
}


}