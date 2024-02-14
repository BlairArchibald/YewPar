#ifndef _DPLL_
#define _DPLL_
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>

struct CNFClause
{
    std::vector<int> variables;
};

struct CNFFormula
{
    std::vector<CNFClause> formula;

    int size()
    {
        return formula.size();
    }
};
CNFFormula parse(std::string filename, int *n_vars);

#endif