#ifndef UTIL_LAZY_NODEGENERATOR_HPP
#define UTIL_LAZY_NODEGENERATOR_HPP

#include<type_traits>

namespace YewPar {

template <typename T>
concept NodeGenerator = requires(T t) {
    typename T::Nodetype;
    typename T::Spacetype;
    { t.numChildren } -> std::convertible_to<unsigned>; 
    { t.next() } -> std::same_as<typename T::Nodetype>;
};

}

#endif
