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

#include <stdlib.h>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <map>

struct CNFClause
{
    std::vector<int> variables;

    bool isEmpty()
    {
        return variables.empty();
    }

    bool isUnitClause()
    {
        return variables.size() == 1;
    }

    int getUnitElement()
    {
        int last = variables.back();
        variables.pop_back();
        return last;
    }

    bool contains(int var)
    {
        return std::find(variables.begin(), variables.end(), var) != variables.end();
    }

    void remove(int var)
    {
        variables.erase(std::find(variables.begin(), variables.end(), var));
    }

    CNFClause() : variables() {}

    CNFClause(const CNFClause &copied) : variables(copied.variables) {}

    CNFClause &operator=(const CNFClause &copied)
    {
        if (this != &copied)
        {
            variables = copied.variables;
        }

        return *this;
    }

    template <class Archive>
    void serialize(Archive &ar, const unsigned int version)
    {
        ar & variables;
    }
};

struct CNFFormula
{
    std::vector<CNFClause> clauses;
    int max_var;

    int size()
    {
        return clauses.size();
    }

    void eraseClauseAt(int i)
    {
        clauses.erase(clauses.begin() + i);
    }

    bool isEmpty()
    {
        return clauses.empty();
    }

    bool containsEmptyClause()
    {
        for (auto &c : clauses)
        {
            if (c.isEmpty())
                return true;
        }
        return false;
    }

    void insertClause(CNFClause clause)
    {
        clauses.push_back(clause);
    }

    void propagate(int var)
    {
        for (int i = clauses.size() - 1; i >= 0; i--)
        {
            if (clauses[i].contains(var))
            {
                // clause satisfied
                eraseClauseAt(i);
                continue;
            }
            if (clauses[i].contains(-var))
            {
                // variable var cannot satisfy clause c, remove -var from c
                clauses[i].remove(-var);
            }
        }
    }

    void pureLiteralElimination()
    {
        std::map<int, bool> pure, not_pure;
        std::map<int, int> occurrences;
        // Note(kubajj): not_pure does not really need a value
        std::map<int, bool>::iterator it;
        int absvar;
        bool value;
        for (auto &c : clauses)
        {
            for (auto &var : c.variables)
            {
                absvar = abs(var);
                it = not_pure.find(absvar);
                if (it != not_pure.end())
                {
                    // not pure
                    occurrences[absvar]++;
                    continue;
                }
                it = pure.find(absvar);
                value = (var > 0);
                if (it != pure.end())
                {
                    // check the value
                    if (it->second != value)
                    {
                        // not pure
                        pure.erase(it);
                        not_pure.emplace(absvar, true);
                        occurrences.emplace(absvar, 2);
                    }
                    continue;
                }
                pure.emplace(absvar, value);
            }
        }
        int var;
        for (auto it = pure.begin(); it != pure.end(); ++it)
        {
            var = it->first;
            value = it->second;
            if (!value)
                var = -var;
            for (int i = clauses.size() - 1; i >= 0; i--)
            {
                if (clauses[i].contains(var))
                {
                    // clause satisfied
                    eraseClauseAt(i);
                    continue;
                }
            }
        }
        int max_val = 0;
        if (!occurrences.empty())
        {
            for (auto &var_occurrence : occurrences)
            {
                if (var_occurrence.second > max_val)
                {
                    max_val = var_occurrence.second;
                    max_var = var_occurrence.first;
                }
            }
        }
    }

    CNFFormula deepcopy() const
    {
        CNFFormula newFormula;
        for (const CNFClause &clause : clauses)
        {
            newFormula.clauses.push_back(clause);
        }
        return newFormula;
    }
};

CNFFormula parse(std::string filename, int *n_vars)
{
    int n_clauses, current_variable;
    std::string p, cnf;
    std::ifstream file(filename);
    CNFFormula formula;
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open file");
    }
    std::string line;
    bool header = false;
    while (std::getline(file, line))
    {
        if (line.empty() || line[0] == 'c' || line[0] == '%' || line[0] == '0')
        {
            continue;
        }
        std::istringstream split_element(line);
        // Handle header line
        if (!header && line[0] == 'p')
        {
            split_element >> p >> cnf >> *n_vars >> n_clauses;
            if (cnf != "cnf")
            {
                throw std::runtime_error("Invalid CNF file format: Header line missing 'cnf'");
            }
            header = true;
            continue;
        }
        if (line[0] == 'p')
        {
            throw std::runtime_error("Invalid CNF file format: Multiple header lines");
        }
        // Handle clauses
        CNFClause c;
        while (split_element >> current_variable && current_variable != 0)
        {
            c.variables.push_back(current_variable);
        }
        formula.insertClause(c);
    }
    if (header && formula.size() != n_clauses)
    {
        throw std::runtime_error("Error: Number of clauses stated in the header line does not match the file content");
    }
    return formula;
}

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
    int chosen_literal;
    bool first;

    GenNode(const Empty &, const DPLLNode &node)
    {
        first = true;
        sat = false;
        new_formula = node.formula.deepcopy();
        // unit propagation
        int var;
        bool there_was_a_unit_clause;
        do
        {
            there_was_a_unit_clause = false;
            for (int i = 0; i < new_formula.size(); i++)
            {
                if (new_formula.clauses[i].isUnitClause())
                {
                    there_was_a_unit_clause = true;
                    var = new_formula.clauses[i].getUnitElement();
                    new_formula.eraseClauseAt(i);
                    new_formula.propagate(var);
                }
            }
        } while (there_was_a_unit_clause);
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
            numChildren = 0;
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
    bool csv = static_cast<bool>(opts.count("csv"));
    if (csv)
    {
        std::string just_filename;
        size_t last_slash = inputFile.find_last_of('/');
        if (last_slash != std::string::npos)
        {
            just_filename = str.substr(last_slash + 1);
        }
        else
        {
            just_filename = inputFile;
        }
        std::cout << just_filename << "," << formula.size() << "," << n_vars << ",";
    }
    else
    {
        std::cout << "CNF formula " << inputFile << " has " << formula.size() << " clauses and " << n_vars << " variables" << std::endl;
    }

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
    bool verbose = static_cast<bool>(opts.count("verbose"));
    if (skeleton == "seq")
    {
        if (verbose)
        {
            sol = YewPar::Skeletons::Seq<
                GenNode,
                YewPar::Skeletons::API::Decision,
                YewPar::Skeletons::API::MoreVerbose>::search(empty, root, searchParameters);
        }
        else
        {
            sol = YewPar::Skeletons::Seq<
                GenNode,
                YewPar::Skeletons::API::Decision>::search(empty, root, searchParameters);
        }
    }
    else if (skeleton == "depthbounded")
    {
        searchParameters.spawnDepth = opts["spawn-depth"].as<std::uint64_t>();
        auto poolType = opts["poolType"].as<std::string>();
        if (poolType == "deque")
        {
            sol = YewPar::Skeletons::DepthBounded<
                GenNode,
                YewPar::Skeletons::API::Decision,
                YewPar::Skeletons::API::DepthBoundedPoolPolicy<
                    Workstealing::Policies::Workpool>,
                YewPar::Skeletons::API::MoreVerbose>::search(empty, root, searchParameters);
        }
        else
        {
            sol = YewPar::Skeletons::DepthBounded<
                GenNode,
                YewPar::Skeletons::API::Decision,
                YewPar::Skeletons::API::DepthBoundedPoolPolicy<
                    Workstealing::Policies::DepthPoolPolicy>,
                YewPar::Skeletons::API::MoreVerbose>::search(empty, root, searchParameters);
        }
    }
    else if (skeleton == "stacksteal")
    {
        searchParameters.stealAll = static_cast<bool>(opts.count("chunked"));
        sol = YewPar::Skeletons::StackStealing<
            GenNode,
            YewPar::Skeletons::API::Decision,
            YewPar::Skeletons::API::MoreVerbose>::search(empty, root, searchParameters);
    }
    else if (skeleton == "budget")
    {
        searchParameters.backtrackBudget = opts["backtrack-budget"].as<std::uint64_t>();
        sol = YewPar::Skeletons::Budget<
            GenNode,
            YewPar::Skeletons::API::Decision,
            YewPar::Skeletons::API::MoreVerbose>::search(empty, root, searchParameters);
    }
    else if (skeleton == "ordered")
    {
        searchParameters.spawnDepth = opts["spawn-depth"].as<std::uint64_t>();
        if (opts.count("discrepancyOrder"))
        {
            sol = YewPar::Skeletons::Ordered<
                GenNode,
                YewPar::Skeletons::API::Decision,
                YewPar::Skeletons::API::DiscrepancySearch,
                YewPar::Skeletons::API::MoreVerbose>::search(empty, root, searchParameters);
        }
        else
        {
            sol = YewPar::Skeletons::Ordered<
                GenNode,
                YewPar::Skeletons::API::Decision,
                YewPar::Skeletons::API::MoreVerbose>::search(empty, root, searchParameters);
        }
    }
    else
    {
        std::cerr << "Invalid skeleton type\n";
        return hpx::finalize();
    }
    auto overall_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time);

    if (csv)
    {
        std::cout << (sol.satisfied ? 1 : 0) << "," << overall_time.count() << std::endl;
    }
    else
    {
        std::cout << "CNF formula is ";
        if (!sol.satisfied)
            std::cout << "not ";
        std::cout << "satisfiable" << std::endl;
        hpx::cout << "cpu = " << overall_time.count() << std::endl;
    }

    return hpx::finalize();
}

int main(int argc, char *argv[])
{
    hpx::program_options::options_description
        desc_commandline("Usage: " HPX_APPLICATION_STRING " [options]");

    // clang-format off
    desc_commandline.add_options()
      ("skeleton",
        hpx::program_options::value<std::string>()->default_value("seq"),
        "Which skeleton to use: seq, depthbound, stacksteal, budget, or ordered")
      ("spawn-depth,d",
        hpx::program_options::value<std::uint64_t>()->default_value(0),
        "Depth in the tree to spawn at")
      ("backtrack-budget,b",
        hpx::program_options::value<std::uint64_t>()->default_value(0),
        "Backtrack budget for budget skeleton")
      ("poolType",
       hpx::program_options::value<std::string>()->default_value("depthpool"),
       "Pool type for depthbounded skeleton")
      ("discrepancyOrder", "Use discrepancy order for the ordered skeleton")
      ("chunked", "Use chunking with stack stealing")
      ("verbose", "Use MoreVerbose")
      ("csv", "Output in CSV format")
      ("satfile",
        hpx::program_options::value<std::string>()->required(),
        "Where to find the SAT file which contains the clause");
    // clang-format on

    YewPar::registerPerformanceCounters();

    hpx::init_params args;
    args.desc_cmdline = desc_commandline;
    return hpx::init(argc, argv, args);
}