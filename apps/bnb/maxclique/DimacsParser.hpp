#ifndef _DIMACSPARSER_HPP_
#define _DIMACSPARSER_HPP_

#include <string>
#include <map>
#include <set>

class SomethingWentWrong : public std::exception
{
private:
  std::string _what;

public:
  SomethingWentWrong(const std::string & message) throw () :
    _what(message) {}

  auto what() const throw () -> const char * {
    return _what.c_str();
  }
};

namespace dimacs {
  using GraphFromFile = std::pair<unsigned, std::map<int, std::set<int> > >;
  auto read_dimacs(const std::string & filename) -> GraphFromFile;
}

#endif
