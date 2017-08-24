#ifndef YEWPAR_EXPONENTIAL_BACKOFF_HPP
#define YEWPART_EXPONENTIAL_BACKOFF_HPP

#include <chrono>
#include <cstdint>

// Max scheduler sleep time (microseconds)
#define MAX_BACKOFF 5000000
#define INITIAL_BACKOFF 100
#define BACKOFF_MUL 2

namespace workstealing {

class ExponentialBackoff {
private:
  std::uint64_t sleep_time;
public:
  ExponentialBackoff() : sleep_time(INITIAL_BACKOFF) {}
  std::chrono::microseconds getSleepTime() { return std::chrono::microseconds(sleep_time); }
  void reset() { sleep_time = INITIAL_BACKOFF; }
  void failed() {
    sleep_time *= BACKOFF_MUL;
    if (sleep_time > MAX_BACKOFF) {
      sleep_time = MAX_BACKOFF;
    }
  }
};

}

#endif
