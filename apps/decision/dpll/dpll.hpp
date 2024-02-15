#ifndef _DPLL_
#define _DPLL_
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
        variables.empty();
    }

    bool isUnitClause()
    {
        return variables.size() == 1;
    }

    int getLast()
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
};

struct CNFFormula
{
    std::vector<CNFClause> clauses;

    int size()
    {
        return clauses.size();
    }

    void insertClause(CNFClause clause)
    {
        clauses.push_back(clause);
    }

    void unitPropagate(CNFClause clause)
    {
        int var = clause.getLast();
        for (int i = clauses.size() - 1; i >= 0; i--)
        {
            if (clauses[i].contains(var))
            {
                // clause satisfied
                clauses.erase(clauses.begin() + i);
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
                        not_pure.insert(std::pair<int, bool>(absvar, true));
                    }
                    continue;
                }
                pure.insert(std::pair<int, bool>(absvar, value));
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
                    clauses.erase(clauses.begin() + i);
                    continue;
                }
            }
        }
    }
};
CNFFormula parse(std::string filename, int *n_vars);

#endif