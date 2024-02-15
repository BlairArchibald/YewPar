#include <hpx/hpx_init.hpp>
#include <hpx/iostream.hpp>

#include <boost/serialization/access.hpp>

#include "YewPar.hpp"

#include "skeletons/Seq.hpp"
#include "skeletons/DepthBounded.hpp"
#include "skeletons/StackStealing.hpp"
#include "skeletons/Ordered.hpp"
#include "skeletons/Budget.hpp"

#include "util/func.hpp"
#include "util/NodeGenerator.hpp"

#include "dpll.hpp"

// DPLL does not need a space
struct Empty
{
};

struct DPLLNode
{
    friend class boost::serialization::access;

    bool satisfied;
    CNFFormula formula;

    bool getObj() const
    {
        return satisfied;
    }

    template <class Archive>
    void serialize(Archive &ar, const unsigned int version)
    {
        ar & satisfied;
        ar & formula;
    }
};

struct GenNode : YewPar::NodeGenerator<DPLLNode, Empty>
{
    bool sat;
    CNFFormula new_formula, copy_of_new_formula;
    std::vector<CNFClause> phi;
    int chosen_literal;
    bool first;

    GenNode(const Empty &, const DPLLNode &node)
    {
        first = true;
        sat = false;
        new_formula = node.formula.deepcopy();
        // unit propagation
        int var;
        for (auto it = new_formula.clauses.begin(); it != new_formula.clauses.end();)
        {
            if (it->isUnitClause())
            {
                var = it->getUnitElement();
                new_formula.eraseClauseAt(it);
                new_formula.propagate(var);
            }
            else
            {
                ++it;
            }
        }
        // pure literal elimination
        new_formula.pureLiteralElimination();
        // stopping conditions
        if (new_formula.isEmpty())
        {
            sat = true;
            numChildren = 1;
        }
        else if (new_formula.containsEmptyClause())
        {
            sat = false;
            numChildren = 1;
        }
        else
        {
            // choose literal - choose the maximum occurring variable
            // This is stored in new_formula.max_occur_var during pure literal elimination
            chosen_literal = new_formula.max_var;
            numChildren = 2;
            copy_of_new_formula = new_formula.deepcopy();
            new_formula.propagate(chosen_literal);
            copy_of_new_formula.propagate(-chosen_literal);
        }
    }

    DPLLNode next() override
    {
        DPLLNode node;
        node.satisfied = sat;
        if (first)
        {
            node.formula = new_formula;
            first = false;
        }
        else
        {
            node.formula = copy_of_new_formula;
        }
        return node;
    }
};

int hpx_main(hpx::program_options::variables_map &opts)
{
    auto inputFile = opts["satfile"].as<std::string>();
    int n_vars;
    CNFFormula formula = parse(inputFile, &n_vars);
    std::cout << "CNF formula with " << formula.size() << " clauses and " << n_vars << " variables" << std::endl;

    auto start_time = std::chrono::steady_clock::now();
    /*
      Main body
    */
    Empty empty;
    DPLLNode root;
    root.satisfied = false;
    root.formula = formula;
    YewPar::Skeletons::API::Params<bool> searchParameters;
    searchParameters.expectedObjective = true;
    auto skeleton = opts["skeleton"].as<std::string>();
    auto sol = root;
    if (skeleton == "seq")
    {
        sol = YewPar::Skeletons::Seq<GenNode,
                                     YewPar::Skeletons::API::Decision,
                                     YewPar::Skeletons::API::MoreVerbose>::search(empty, root, searchParameters);
    }
    else
    {
        std::cerr << "Invalid skeleton type\n";
        return hpx::finalize();
    }
    auto overall_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time);

    std::cout << "CNF formula is ";
    if (!sol.satisfied)
        std::cout << "not ";
    std::cout << "satisfiable" << std::endl;
    hpx::cout << "cpu = " << overall_time.count() << std::endl;

    return hpx::finalize();
}

int main(int argc, char *argv[])
{
    hpx::program_options::options_description
        desc_commandline("Usage: " HPX_APPLICATION_STRING " [options]");

    // clang-format off
    desc_commandline.add_options()
      ( "skeleton",
        hpx::program_options::value<std::string>()->default_value("seq"),
        "Which skeleton to use: only seq possible for now"
        )
      ( "satfile",
        hpx::program_options::value<std::string>()->required(),
        "Where to find the SAT file which contains the clause"
      );
    // clang-format on

    YewPar::registerPerformanceCounters();

    hpx::init_params args;
    args.desc_cmdline = desc_commandline;
    return hpx::init(argc, argv, args);
}