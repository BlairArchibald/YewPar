#ifndef UTIL_ENUMERATOR_HPP
#define UTIL_ENUMERATOR_HPP

namespace YewPar {

// Enumerators capture the ability to accumulate information about nodes over
// the search and can be seen as a monoid.

template <typename NodeType, typename ResultType>
struct Enumerator {
    using ResT = ResultType;
    virtual void accumulate(const NodeType & n) = 0;
    virtual void combine(const ResultType & n) = 0;
    virtual ResultType get() = 0;
};

// Identity Enumerator - Don't save anything
template <typename NodeType>
struct IdentityEnumerator : Enumerator<NodeType, std::string> {
    void accumulate(const NodeType & n) override {};
    void combine(const std::string & s) override {};
    std::string get() override { return "Identity Enumerator"; };
};

} // Namespace YewPar

#endif // UTIL_ENUMERATOR_HPP
