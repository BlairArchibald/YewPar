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
    GenNode(const Empty &, const DPLLNode &node)
    {
        sat = true;
        numChildren = 1;
    }

    DPLLNode next() override
    {
        DPLLNode node;
        node.satisfied = sat;
        CNFFormula form;
        node.formula = form;
        return node;
    }
};

int hpx_main(hpx::program_options::variables_map &opts)
{
    auto inputFile = opts["satfile"].as<std::string>();
    int n_vars;
    CNFFormula formula = parse(inputFile, &n_vars);
    std::cout << "CNF formula with " << clauses.size() << " clauses and " << n_vars << " variables" << std::endl;

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
    if (skeleton == "seq")
    {
        sol = YewPar::Skeletons::Seq<GenNode<NWORDS>,
                                     YewPar::Skeletons::API::Decision,
                                     YewPar::Skeletons::API::MoreVerbose>::search(empty, root, searchParameters);
    }
    else
    {
        std::cerr << "Invalid skeleton type\n";
        return hpx::finalize();
    }
    auto overall_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time);

    std::cout << "CNF formula is";
    if (!sol)
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