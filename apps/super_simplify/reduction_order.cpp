#include "expr_util.h"
#include "Halide.h"

using namespace Halide;
using namespace Halide::Internal;

using std::set;
using std::map;
using std::string;
using std::ostringstream;

IRNodeType node_ordering[18] = {IRNodeType::Ramp,IRNodeType::Broadcast,IRNodeType::Select,IRNodeType::Div,IRNodeType::Mul,IRNodeType::Mod,IRNodeType::Sub,IRNodeType::Add,IRNodeType::Min,IRNodeType::Not,IRNodeType::Or,IRNodeType::And,IRNodeType::GE,IRNodeType::GT,IRNodeType::LE,IRNodeType::LT,IRNodeType::NE,IRNodeType::EQ};

std::map<IRNodeType, int> nto = {
    {IRNodeType::Ramp,23},
    {IRNodeType::Broadcast,22},
    {IRNodeType::Select,21},
    {IRNodeType::Div,20},
    {IRNodeType::Mul,19},
    {IRNodeType::Mod,18},
    {IRNodeType::Sub,17},
    {IRNodeType::Add,16},
    {IRNodeType::Max,14}, // note max and min have same weight
    {IRNodeType::Min,14},
    {IRNodeType::Not,13},
    {IRNodeType::Or,12},
    {IRNodeType::And,11},
    {IRNodeType::GE,10},
    {IRNodeType::GT,9},
    {IRNodeType::LE,8},
    {IRNodeType::LT,7},
    {IRNodeType::NE,6},
    {IRNodeType::EQ,5},
    {IRNodeType::Cast,4},
    {IRNodeType::FloatImm,2},
    {IRNodeType::UIntImm,1},
    {IRNodeType::IntImm,0}
};

class DivisorSet : public IRVisitor {
    Scope<> lets;

    void visit(const Div *op) override {
        std::ostringstream term;
        term << op->b;
        divisors.insert(term.str());
        op->a.accept(this);
        op->b.accept(this);
    }

    void visit(const Mod *op) override {
        std::ostringstream term;
        term << op->b;
        divisors.insert(term.str());
        op->a.accept(this);
        op->b.accept(this);
    }
public:
    std::set<std::string> divisors;
};

std::set<std::string> find_divisors(const Expr &e) {
    DivisorSet d;
    e.accept(&d);
    return d.divisors;
}

bool check_divisors(const Expr &LHS, const Expr &RHS) {
    // check that all divisors on RHS appear as divisors on LHS
    std::set<std::string> lhs_divisors = find_divisors(LHS);
    std::set<std::string> rhs_divisors = find_divisors(RHS);
    for (auto const& rhs_term : rhs_divisors) {
        if (lhs_divisors.count(rhs_term) == 0) {
            return false;
        }
    }
    return true;
}

class NodeHistogram : public IRVisitor {
    Scope<> lets;

    void visit(const Select *op) override {
        increment_histo(IRNodeType::Select);
        op->condition.accept(this);
        op->true_value.accept(this);
        op->false_value.accept(this);
    }

    void visit(const Ramp *op) override {
        increment_histo(IRNodeType::Ramp);
        op->base.accept(this);
        op->stride.accept(this);
    }

    void visit(const Broadcast *op) override {
        increment_histo(IRNodeType::Broadcast);
        op->value.accept(this);
    }

    void visit(const Add *op) override {
        increment_histo(IRNodeType::Add);
        op->a.accept(this);
        op->b.accept(this);
    }

    void visit(const Sub *op) override {
        increment_histo(IRNodeType::Sub);
        op->a.accept(this);
        op->b.accept(this);
    }

    void visit(const Mul *op) override {
        increment_histo(IRNodeType::Mul);
        op->a.accept(this);
        op->b.accept(this);
    }

    void visit(const Div *op) override {
        increment_histo(IRNodeType::Div);
        op->a.accept(this);
        op->b.accept(this);
    }

    void visit(const Mod *op) override {
        increment_histo(IRNodeType::Mod);
        op->a.accept(this);
        op->b.accept(this);
    }

    void visit(const LT *op) override {
        increment_histo(IRNodeType::LT);
        op->a.accept(this);
        op->b.accept(this);
    }

    void visit(const LE *op) override {
        increment_histo(IRNodeType::LE);
        op->a.accept(this);
        op->b.accept(this);
    }

    void visit(const GT *op) override {
        increment_histo(IRNodeType::GT);
        op->a.accept(this);
        op->b.accept(this);
    }

    void visit(const GE *op) override {
        increment_histo(IRNodeType::GE);
        op->a.accept(this);
        op->b.accept(this);
    }

    void visit(const EQ *op) override {
        increment_histo(IRNodeType::EQ);
        op->a.accept(this);
        op->b.accept(this);
    }

    void visit(const Min *op) override {
        increment_histo(IRNodeType::Min);
        op->a.accept(this);
        op->b.accept(this);
    }

    void visit(const Max *op) override {
        increment_histo(IRNodeType::Min); // put max counts into min bucket so we count them the same
        op->a.accept(this);
        op->b.accept(this);
    }

    void visit(const Not *op) override {
        increment_histo(IRNodeType::Not);
        op->a.accept(this);
    }

    void visit(const And *op) override {
        increment_histo(IRNodeType::And);
        op->a.accept(this);
        op->b.accept(this);
    }

    void visit(const Or *op) override {
        increment_histo(IRNodeType::Or);
        op->a.accept(this);
        op->b.accept(this);
    }

    void visit(const Let *op) override {
        op->value.accept(this);
        {
            ScopedBinding<> bind(lets, op->name);
            op->body.accept(this);
        }
    }
public:
    std::map<IRNodeType, int> histogram;
    void increment_histo(IRNodeType node_type) {
        if (histogram.count(node_type) == 0) {
            histogram[node_type] = 1;
        } else {
            histogram[node_type] = histogram[node_type] + 1;
        }
    }
};

std::map<IRNodeType, int> build_histogram(const Expr &e) {
    NodeHistogram histo;
    e.accept(&histo);
    return histo.histogram;
}

// return 1 if correctly ordered, -1 if incorrectly ordered, 0 if tied
int compare_histograms(const Expr &LHS, const Expr &RHS) {
    std::map<IRNodeType, int> lhs_histo = build_histogram(LHS);
    std::map<IRNodeType, int> rhs_histo = build_histogram(RHS);
    int lhs_node_count, rhs_node_count;
    for (auto const& node : node_ordering) {
        lhs_node_count = 0;
        rhs_node_count = 0;
        if (lhs_histo.count(node) == 1) {
            lhs_node_count = lhs_histo[node];
        }
        if (rhs_histo.count(node) == 1) {
            rhs_node_count = rhs_histo[node];
        }

        std::cout << node << " LHS count " << lhs_node_count << " RHS count " << rhs_node_count << "\n";
        // RHS side has more of some op than LHS
        if (lhs_node_count < rhs_node_count) {
            return -1;
            // LHS side has strictly more of some op than RHS
        } else if (lhs_node_count > rhs_node_count) {
            return 1;
        }
    }
    return 0;
}

bool valid_reduction_order(const Expr &LHS, const Expr &RHS) {
    // check that occurrences of variables on RHS is equal or lesser to those in LHS
    std::map<std::string, int> lhs_vars = find_vars(LHS);
    std::map<std::string, int> rhs_vars = find_vars(RHS);
    for (auto const& varcount : rhs_vars) {
        // constant wildcards don't count bc they can't match terms so can't cause reduction order failures
        if (varcount.first.front() != 'c' && (lhs_vars[varcount.first] == 0 || varcount.second > lhs_vars[varcount.first])) {
            return false;
        }
    }

    // check that histogram of operations obeys ordering
    int rule_histogram_ordering = compare_histograms(LHS, RHS);
    // std::cout << "Rule histogram ordering is " << rule_histogram_ordering << "\n";
    if (rule_histogram_ordering == 1) {
        return true;
    } else if (rule_histogram_ordering == -1) {
        return false;
    }

    // check that root symbol obeys ordering
    IRNodeType lhs_root_type = LHS.node_type();
    IRNodeType rhs_root_type = RHS.node_type();

    if (nto[lhs_root_type] >= nto[rhs_root_type]) {
        return false;
    }

    return true;
}