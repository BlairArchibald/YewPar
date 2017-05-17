#ifndef FIB_HPP
#define FIB_HPP

std::uint64_t fib(std::uint64_t n) {
  return n <= 2 ? 1 : fib(n - 1) + fib(n - 2);
}

#endif
