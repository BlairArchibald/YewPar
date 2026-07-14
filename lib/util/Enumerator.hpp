#ifndef UTIL_ENUMERATOR_HPP
#define UTIL_ENUMERATOR_HPP

#include <cstdint>

namespace YewPar {

// Enumerators capture the ability to accumulate information about nodes over
// the search and can be seen as a monoid.

template <typename Space, typename NodeType, typename ResultType>
struct Enumerator {
    using ResT = ResultType;
    virtual void accumulate(const Space & s, const NodeType & n) = 0;
    virtual void combine(const ResultType & n) = 0;
    virtual ResultType get() = 0;
};

// Identity Enumerator - Don't save anything
template <typename Space, typename NodeType>
struct IdentityEnumerator : Enumerator<Space, NodeType, std::string> {
    void accumulate(const Space&, const NodeType & n) override {};
    void combine(const std::string & s) override {};
    std::string get() override { return "Identity Enumerator"; };
};

template <typename Space, typename NodeType>
struct CountNodesEnumerator : Enumerator<Space, NodeType, std::uint64_t> {
    std::uint64_t count = 0;
    void accumulate(const Space&, const NodeType & n) override { count++; };
    void combine(const std::uint64_t & n) override { count += n; };
    std::uint64_t get() override { return count; };
};

} // Namespace YewPar

#endif // UTIL_ENUMERATOR_HPP
