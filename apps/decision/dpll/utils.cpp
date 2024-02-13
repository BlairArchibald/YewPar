#include "dpll.hpp"

std::vector<CNFClause> parse(std::string filename)
{
    int vars, clauses;
    std::string p, cnf;
    std::ifstream file(filename);
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
        if (!header && line[0] == 'p')
        {
            std::istringstream tokenStream(line);
            tokenStream >> p >> cnf >> vars >> clauses;
            bool ps, cnfs;
            ps = (p == "p");
            cnfs = (cnf == "cnf");
            std::cout << "p = 'p' == " << ps << " and cnf = 'cnf' == " << cnfs << std::endl;
        }
    }
}
