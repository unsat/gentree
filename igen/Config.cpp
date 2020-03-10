//
// Created by KH on 3/5/2020.
//

#include "Config.h"
#include "Domain.h"
#include "CoverageStore.h"

namespace igen {


Config::Config(PMutContext ctx, int id)
        : Object(move(ctx)), id_(id), values_(size_t(dom()->n_vars())) {
}

Config::Config(PMutContext ctx, const vec<int> &values, int id) : Config(move(ctx), id) {
    set_all(values);
}

Config::Config(const PConfig &c) : Config(c->ctx_mut(), c->values()) {
}

Config::Config(const PMutConfig &c) : Config(c->ctx_mut(), c->values()) {
}

vec<str> Config::value_labels() const {
    vec<str> labels(size_t(dom()->n_vars()));
    CHECK_EQ(labels.size(), values_.size());
    for (int i = 0; i < int(labels.size()); ++i) {
        labels[i] = dom()->label(i, values_[i]);
    }
    return labels;
}

void Config::set_all(int value) {
    for (int i = 0; i < dom()->n_vars(); ++i) {
        set(i, value);
    }
}

void Config::set_all(const vec<int> &values) {
    CHECK_EQ(values.size(), dom()->n_vars());
    for (int i = 0; i < dom()->n_vars(); ++i) {
        set(i, values[i]);
    }
}

bool Config::eval(z3::expr e) const {
    z3::context &zctx = ctx_mut()->zctx();
    z3::expr_vector sub_to(zctx);
    sub_to.resize(unsigned(dom()->n_vars()));
    for (int i = 0; i < dom()->n_vars(); ++i) {
        auto expr_val = dom()->vars()[i]->zval(value(i));
        sub_to.set(i, expr_val);
    }
    z3::expr sub_res = e.substitute(dom()->vars_expr_vector(), sub_to).simplify();
    CHECK(sub_res.kind() == Z3_APP_AST);
    int bool_val = sub_res.bool_value();
    CHECK(bool_val != Z3_L_UNDEF);
    return bool_val == Z3_L_TRUE;
}

bool Config::cov_loc(const str &loc_name) const {
    const PLocation &loc = cov()->loc(loc_name);
    if (loc == nullptr) return false;
    return std::binary_search(cov_locs_.begin(), cov_locs_.end(), loc->id());
}

hash128_t Config::hash_128(bool bypass_cache) const {
    if (!bypass_cache && cached_hash_ != hash128_empty)
        return cached_hash_;

    return cached_hash_ = calc_hash_128(values());
}

std::ostream &operator<<(std::ostream &output, const Config &d) {
    output << "Config " << d.id() << ": ";
    bool first_var = true;
    for (const auto &e : d) {
        if (!first_var) output << ", "; else first_var = false;
        output << e.name() << ' ' << e.label();
    }
    return output;
}

const ptr<const Location> Config::const_location_iterator::dereference() const {
    return ctx->cov()->loc(*base_reference());
}

}