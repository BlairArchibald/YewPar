/**
 * Read in a graph from a DIMACS format file. Produces a pair whose first
 * value is the number of vertices, and whose second is a map from vertex
 * number to a set of adjacent vertices.
 */

#include "DimacsParser.hpp"

#include <iostream>
#include <fstream>
#include <regex>

namespace dimacs {

  auto read_dimacs(const std::string & filename) -> GraphFromFile
  {
    GraphFromFile result;

    std::ifstream infile{ filename };
    if (! infile)
      throw SomethingWentWrong{ "unable to open file" };

    std::string line;
    while (std::getline(infile, line)) {
      if (line.empty())
        continue;

      /* Lines are comments, a problem description (contains the number of
      * vertices), or an edge. */
      static const std::regex
        comment{ R"(c(\s.*)?)" },
        problem{ R"(p\s+(edge|col)\s+(\d+)\s+(\d+)?\s*)" },
          edge{ R"(e\s+(\d+)\s+(\d+)\s*)" };

          std::smatch match;
          if (regex_match(line, match, comment)) {
            /* Comment, ignore */
          }
          else if (regex_match(line, match, problem)) {
            /* Problem. Specifies the size of the graph. Must happen exactly
            * once. */
            if (0 != result.first)
              throw SomethingWentWrong{ "multiple 'p' lines encountered" };
            result.first = std::stoi(match.str(2));
            for (unsigned i = 0 ; i < result.first ; ++i)
              result.second[i];
          }
          else if (regex_match(line, match, edge)) {
            /* An edge. DIMACS files are 1-indexed. We assume we've already had
            * a problem line (if not our size will be 0, so we'll throw). */
            unsigned long a{ std::stoul(match.str(1)) }, b{ std::stoul(match.str(2)) };
            if (0 == a || 0 == b || a > result.first || b > result.first)
              throw SomethingWentWrong{ "line '" + line + "' edge index out of bounds" };
            else if (a == b)
              throw SomethingWentWrong{ "line '" + line + "' contains a loop" };
            result.second[a - 1].insert(b - 1);
            result.second[b - 1].insert(a - 1);
          }
          else
            throw SomethingWentWrong{ "cannot parse line '" + line + "'" };
    }

    if (! infile.eof())
      throw SomethingWentWrong{ "error reading file" };

    return result;
  }
};
