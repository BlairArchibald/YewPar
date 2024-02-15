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

    CNFClause(const CNFClause &copied) : variables(copied.variables) {}

    CNFClause &operator=(const CNFClause &copied)
    {
        if (this != &copied)
        {
            variables = copied.variables;
        }

        return *this;
    }
};

struct CNFFormula
{
    std::vector<CNFClause> clauses;
    int max_occur_var;

    int size()
    {
        return clauses.size();
    }

    void eraseClauseAt(std::vector<CNFClause>::iterator i)
    {
        clauses.erase(i);
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
                eraseClauseAt(clauses.begin() + i);
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
                    eraseClauseAt(clauses.begin() + i);
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
                    max_val = var_occurrence.second;
            }
        }
        max_occur_var = max_val;
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
CNFFormula parse(std::string filename, int *n_vars);

#endif