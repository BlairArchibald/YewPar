#include "dpll.hpp"

CNFFormula parse(std::string filename, int *n_vars)
{
    int n_clauses, current_variable;
    std::string p, cnf;
    std::ifstream file(filename);
    std::vector<CNFClause> clauses;
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open file");
    }
    std::string line;
    bool header = false;
    while (std::getline(file, line))
    {
        if (line.empty() || line[0] == 'c')
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
        clauses.push_back(c);
    }
    if (header && clauses.size() != n_clauses)
    {
        throw std::runtime_error("Error: Number of clauses stated in the header line does not match the file content");
    }
    return clauses;
}
