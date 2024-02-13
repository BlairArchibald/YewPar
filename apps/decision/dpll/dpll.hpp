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

std::vector<CNFClause> parse(std::string filename, int *n_vars);

#endif