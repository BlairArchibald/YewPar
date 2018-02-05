#ifndef YEWPAR_POLICY_HPP
#define YEWPAR_POLICY_HPP

#include <hpx/util/function.hpp>

class Policy {
 public:
  // Scheduler hook point
  virtual hpx::util::function<void(), false> getWork() = 0;
};

#endif
