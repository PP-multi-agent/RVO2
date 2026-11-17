/*
 * Team.cc
 * RVO2 Library
 *
 * SPDX-FileCopyrightText: 2008 University of North Carolina at Chapel Hill
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Please send all bug reports to <geom@cs.unc.edu>.
 *
 * The authors may be contacted via:
 *
 * Jur van den Berg, Stephen J. Guy, Jamie Snape, Ming C. Lin, Dinesh Manocha
 * Dept. of Computer Science
 * 201 S. Columbia St.
 * Frederick P. Brooks, Jr. Computer Science Bldg.
 * Chapel Hill, N.C. 27599-3175
 * United States of America
 *
 * <https://gamma.cs.unc.edu/RVO2/>
 */

/**
 * @file  Team.cc
 * @brief Defines the Team class.
 */

#include "Team.h"

#include <algorithm>
#include <chrono>
#include <system_error>

#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || \
    defined(_M_IX86)
#include <immintrin.h>
#endif

namespace RVO {
namespace {
typedef unsigned long long Word;
typedef std::chrono::steady_clock Clock;

/* The state of a chunk is four times the number of the loop that last
 * touched it, plus one of these. States only grow, so a thread that is still
 * in an earlier loop can neither take a chunk nor store one. */
const Word kComputing = 1U;
const Word kStoring = 2U;
const Word kDone = 3U;

const std::size_t kMaxThreads = 256U;

/* A worker without work polls for this long before it blocks. Waking a
 * blocked thread costs far more than a step of a small simulation. */
const double kSpinUs = 10000.0;

/* A hedged loop waits at least this long for an item before computing it
 * again, and otherwise kGraceItems times what an item took on average. A
 * thread that is not running stays away for a millisecond or more. */
const double kMinGraceUs = 2.0;
const double kGraceItems = 4.0;

inline void relax() {
#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || \
    defined(_M_IX86)
  _mm_pause();
#elif (defined(__aarch64__) || defined(__arm__)) && \
    (defined(__GNUC__) || defined(__clang__))
  __asm__ __volatile__("yield");
#endif
}

inline double microsecondsSince(Clock::time_point start) {
  return std::chrono::duration<double, std::micro>(Clock::now() - start)
      .count();
}
} /* namespace */

/* The pin of a worker sits on a cache line of its own. */
struct Team::Slot {
  char before[64];
  std::atomic<const void *> pin;
  char after[64];
};

Team::Job::~Job() {}

void Team::Job::store(std::size_t /*item*/, std::size_t /*thread*/) {}

Team::Team()
    : gate_(0U),
      job_(NULL),
      flags_(NULL),
      count_(0U),
      grain_(1U),
      threads_(0U),
      pin_(NULL),
      hedged_(false),
      capacity_(0U),
      phase_(0U),
      sleepers_(0),
      started_(0U) {}

Team &Team::instance() {
  /* Never destroyed: its workers may outlive every other static object. */
  static Team *const team = new Team();
  return *team;
}

void Team::run(Job *job, std::size_t count, std::size_t threads,
               std::size_t grain) {
  execute(job, count, threads, std::max<std::size_t>(grain, 1U), false, this);
}

void Team::runHedged(Job *job, std::size_t count, std::size_t threads,
                     const void *pin) {
  /* One item at a time: a second thread computes what it stores. */
  execute(job, count, threads, 1U, true, pin);
}

bool Team::isPinned(const void *pin) const {
  const std::lock_guard<std::mutex> lock(slotsMutex_);

  for (std::size_t i = 0U; i < slots_.size(); ++i) {
    if (slots_[i]->pin.load() == pin) {
      return true;
    }
  }

  return false;
}

void Team::waitUnpinned(const void *pin) const {
  while (isPinned(pin)) {
    std::this_thread::yield();
  }
}

void Team::execute(Job *job, std::size_t count, std::size_t threads,
                   std::size_t grain, bool hedged, const void *pin) {
  std::unique_lock<std::mutex> lock(runMutex_, std::defer_lock);

  /* One loop at a time. A second caller, or a job that starts a loop of its
   * own, runs on the calling thread alone. */
  if (threads < 2U || count < 2U || !lock.try_lock()) {
    for (std::size_t i = 0U; i < count; ++i) {
      job->compute(i, 0U);

      if (hedged) {
        job->store(i, 0U);
      }
    }

    return;
  }

  threads = std::min(threads, kMaxThreads);

  while (workers_.size() + 1U < threads) {
    Slot *const slot = new Slot();
    slot->pin.store(NULL);

    try {
      const std::lock_guard<std::mutex> slotsLock(slotsMutex_);
      slots_.push_back(slot);
      workers_.push_back(
          std::thread(&Team::workerMain, this, workers_.size() + 1U, slot));
    } catch (const std::system_error &) {
      /* No more threads to be had; the slot stays, unused. */
      break;
    }
  }

  /* A loop does not wait for threads that are not there yet, and the first
   * loops of a short simulation would be over before a new thread runs. */
  while (started_.load() < workers_.size()) {
    std::this_thread::yield();
  }

  Loop loop;
  loop.job = job;
  loop.count = count;
  loop.grain = grain;
  loop.threads = std::min(threads, workers_.size() + 1U);
  loop.hedged = hedged;

  const std::size_t chunks = (count + grain - 1U) / grain;

  if (capacity_ < chunks) {
    /* Workers that fell behind may still be reading the old flags. */
    for (std::size_t i = 0U; i < slots_.size(); ++i) {
      while (slots_[i]->pin.load() != NULL) {
        std::this_thread::yield();
      }
    }

    delete[] flags_.load();
    capacity_ = std::max(chunks, 2U * capacity_);
    std::atomic<Word> *const flags = new std::atomic<Word>[capacity_];

    for (std::size_t i = 0U; i < capacity_; ++i) {
      flags[i].store(4U * phase_ + kDone, std::memory_order_relaxed);
    }

    flags_.store(flags);
  }

  loop.flags = flags_.load();
  loop.phase = ++phase_;
  job_.store(job, std::memory_order_relaxed);
  count_.store(count, std::memory_order_relaxed);
  grain_.store(grain, std::memory_order_relaxed);
  threads_.store(loop.threads, std::memory_order_relaxed);
  pin_.store(pin, std::memory_order_relaxed);
  hedged_.store(hedged, std::memory_order_relaxed);

  const Clock::time_point start = Clock::now();
  gate_.store(2U * loop.phase + 1U);

  if (sleepers_.load() != 0) {
    /* Taking the mutex orders this after a sleeper's last look at the gate. */
    { const std::lock_guard<std::mutex> sleepLock(sleepMutex_); }
    wake_.notify_all();
  }

  busy_.clear();
  work(loop, 0U, &busy_);

  if (!busy_.empty()) {
    /* What the calling thread did not compute itself was computed at about
     * the same pace by the others. */
    const double perChunkUs = microsecondsSince(start) *
                              static_cast<double>(loop.threads) /
                              static_cast<double>(chunks);
    finish(loop, std::max(kMinGraceUs, kGraceItems * perChunkUs));
  }

  gate_.store(2U * loop.phase);
}

bool Team::attempt(const Loop &loop, std::size_t chunk, std::size_t thread) {
  const Word computing = 4U * loop.phase + kComputing;
  Word flag = loop.flags[chunk].load(std::memory_order_relaxed);

  if (flag < computing &&
      loop.flags[chunk].compare_exchange_strong(flag, computing)) {
    compute(loop, chunk, thread);
    complete(loop, chunk, thread);
    return true;
  }

  /* Done in this loop, or the loop is long over. */
  return flag >= 4U * loop.phase + kDone;
}

void Team::compute(const Loop &loop, std::size_t chunk, std::size_t thread) {
  const std::size_t begin = chunk * loop.grain;
  const std::size_t end = std::min(loop.count, begin + loop.grain);

  for (std::size_t i = begin; i < end; ++i) {
    loop.job->compute(i, thread);
  }
}

void Team::complete(const Loop &loop, std::size_t chunk, std::size_t thread) {
  if (loop.hedged) {
    Word expected = 4U * loop.phase + kComputing;

    if (!loop.flags[chunk].compare_exchange_strong(
            expected, 4U * loop.phase + kStoring)) {
      /* Another thread got there first, or the loop is over. */
      return;
    }

    /* The chunk of a hedged loop is one item. */
    loop.job->store(chunk, thread);
  }

  loop.flags[chunk].store(4U * loop.phase + kDone, std::memory_order_release);
}

void Team::work(const Loop &loop, std::size_t thread,
                std::vector<std::size_t> *busy) {
  const std::size_t chunks = (loop.count + loop.grain - 1U) / loop.grain;

  for (std::size_t other = 0U; other < loop.threads; ++other) {
    const std::size_t share = (thread + other) % loop.threads;
    const std::size_t begin = share * chunks / loop.threads;
    const std::size_t end = (share + 1U) * chunks / loop.threads;

    if (other == 0U) {
      for (std::size_t i = begin; i < end; ++i) {
        if (!attempt(loop, i, thread) && busy != NULL) {
          busy->push_back(i);
        }
      }
    } else {
      /* From the back, away from the thread whose share this is. */
      for (std::size_t i = end; i-- > begin;) {
        if (!attempt(loop, i, thread) && busy != NULL) {
          busy->push_back(i);
        }
      }
    }
  }
}

void Team::finish(const Loop &loop, double graceUs) {
  const Clock::time_point start = Clock::now();
  bool patient = loop.hedged;

  while (!busy_.empty()) {
    if (patient && microsecondsSince(start) > graceUs) {
      patient = false;
    }

    for (std::size_t i = 0U; i < busy_.size();) {
      const std::size_t chunk = busy_[i];
      const Word flag = loop.flags[chunk].load(std::memory_order_acquire);

      if (flag == 4U * loop.phase + kComputing && loop.hedged && !patient) {
        /* Its thread is probably not running. Compute it here as well;
         * whoever is first stores it. */
        compute(loop, chunk, 0U);
        complete(loop, chunk, 0U);
      } else if (flag >= 4U * loop.phase + kDone) {
        busy_[i] = busy_.back();
        busy_.pop_back();
      } else {
        ++i;
      }
    }

    relax();
  }
}

void Team::workerMain(std::size_t thread, Slot *slot) {
  Word seen = 0U;
  unsigned int spins = 0U;
  Clock::time_point idleSince = Clock::now();
  started_.fetch_add(1U);

  for (;;) {
    const Word gate = gate_.load(std::memory_order_acquire);

    if ((gate & 1U) == 0U || gate == seen) {
      relax();

      if ((++spins & 1023U) == 0U && microsecondsSince(idleSince) > kSpinUs) {
        sleep(seen);
        idleSince = Clock::now();
      }

      continue;
    }

    seen = gate;

    Loop loop;
    loop.job = job_.load(std::memory_order_relaxed);
    loop.flags = flags_.load(std::memory_order_relaxed);
    loop.phase = gate >> 1;
    loop.count = count_.load(std::memory_order_relaxed);
    loop.grain = grain_.load(std::memory_order_relaxed);
    loop.threads = threads_.load(std::memory_order_relaxed);
    loop.hedged = hedged_.load(std::memory_order_relaxed);

    /* Pin first, then look at the gate again. If the loop is still open, the
     * values above are its values, and whoever wants to change what the pin
     * stands for will see the pin. */
    slot->pin.store(pin_.load(std::memory_order_relaxed));

    if (gate_.load() == gate && thread < loop.threads) {
      work(loop, thread, NULL);
    }

    slot->pin.store(NULL);
    idleSince = Clock::now();
  }
}

void Team::sleep(Word seen) {
  std::unique_lock<std::mutex> lock(sleepMutex_);
  sleepers_.fetch_add(1);

  for (;;) {
    const Word gate = gate_.load();

    if ((gate & 1U) != 0U && gate != seen) {
      break;
    }

    wake_.wait(lock);
  }

  sleepers_.fetch_sub(1);
}
} /* namespace RVO */
