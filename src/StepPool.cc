/*
 * StepPool.cc
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
 * @file  StepPool.cc
 * @brief Defines the StepPool class.
 */

#include "StepPool.h"

#if RVO_STEP_POOL
#include <time.h>

#include <cstdlib>
#include <new>

namespace RVO {
namespace {
typedef unsigned long long Word;

const Word kAgentPhase = 0U;
const Word kUpdatePhase = 1U;
const Word kPrefPhase = 2U;
const Word kIdlePhase = 3U;

/* An agent takes a few microseconds; small chunks keep the end even. */
const Word kAgentChunk = 4U;
/* A position update takes nanoseconds. */
const Word kUpdateChunk = 64U;
/* Longer than a chunk of agents, much shorter than a paused thread. */
const double kRedoGraceUs = 100.0;
const unsigned kSpinsBeforeNap = 200000U;
const long kNapNs = 50000L;

inline void cpuRelax() {
#if defined(__x86_64__) || defined(__i386__)
  __builtin_ia32_pause();
#elif defined(__aarch64__)
  __asm__ __volatile__("yield" ::: "memory");
#endif
}

inline double monotonicUs() {
  timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return static_cast<double>(ts.tv_sec) * 1e6 +
         static_cast<double>(ts.tv_nsec) * 1e-3;
}

/* A phase is (step << 32) | (counter << 2) | kind. The low 32 bits are
 * unique among recent phases and tag the hand-out counters. */
inline Word phaseKind(Word phase) { return phase & 3U; }

inline unsigned phaseStep(Word phase) {
  return static_cast<unsigned>(phase >> 32);
}

inline Word phaseTag(Word phase) { return phase & 0xffffffffULL; }
} /* namespace */

StepPool::StepPool(RVOSimulator *simulator, int threads)
    : simulator_(simulator),
      function_(NULL),
      context_(NULL),
      slots_(NULL),
      threads_(threads > 1 ? threads : 1),
      agents_(0U),
      step_(0U),
      counter_(0U),
      phase_(kIdlePhase),
      stop_(0) {
  void *memory = NULL;

  if (posix_memalign(&memory, 64U,
                     static_cast<std::size_t>(threads_) * sizeof(Slot)) != 0) {
    throw std::bad_alloc();
  }

  slots_ = static_cast<Slot *>(memory);

  for (int t = 0; t < threads_; ++t) {
    slots_[t].claim = 0U;
    slots_[t].end = 0U;
    slots_[t].done = 0U;
    slots_[t].busy = 0;
    scratch_.push_back(RVOSimulator::newScratchAgent());
  }

  handles_.resize(static_cast<std::size_t>(threads_));
  starts_.resize(static_cast<std::size_t>(threads_));

  for (int t = 1; t < threads_; ++t) {
    starts_[t].pool = this;
    starts_[t].tid = t;

    if (pthread_create(&handles_[t], NULL, &StepPool::workerMain,
                       &starts_[t]) != 0) {
      threads_ = t;
      break;
    }
  }
}

StepPool::~StepPool() {
  quiesce();
  __atomic_store_n(&stop_, 1, __ATOMIC_SEQ_CST);

  for (int t = 1; t < threads_; ++t) {
    pthread_join(handles_[t], NULL);
  }

  for (std::size_t t = 0U; t < scratch_.size(); ++t) {
    RVOSimulator::deleteScratchAgent(scratch_[t]);
  }

  std::free(slots_);
}

void *StepPool::workerMain(void *arg) {
  const Start *const start = static_cast<const Start *>(arg);
  start->pool->workerLoop(start->tid);
  return NULL;
}

void StepPool::workerLoop(int tid) {
  Word last = kIdlePhase;
  unsigned spins = 0U;

  while (__atomic_load_n(&stop_, __ATOMIC_ACQUIRE) == 0) {
    Word phase = __atomic_load_n(&phase_, __ATOMIC_ACQUIRE);

    if (phase == last || phaseKind(phase) == kIdlePhase) {
      last = phase;

      if (spins < kSpinsBeforeNap) {
        ++spins;
        cpuRelax();
      } else {
        const timespec nap = {0, kNapNs};
        nanosleep(&nap, NULL);
      }

      continue;
    }

    /* Announce first, then read the phase again: quiesce() either sees this
     * thread as busy, or this thread sees the idle phase. */
    __atomic_store_n(&slots_[tid].busy, 1, __ATOMIC_SEQ_CST);
    phase = __atomic_load_n(&phase_, __ATOMIC_SEQ_CST);

    if (phase != last && phaseKind(phase) != kIdlePhase) {
      participate(tid, phase);
    }

    last = phase;
    __atomic_store_n(&slots_[tid].busy, 0, __ATOMIC_RELEASE);
    spins = 0U;
  }
}

StepPool::Word StepPool::nextPhase(Word kind) {
  counter_ = (counter_ + 1U) & 0x3fffffffU;
  return (static_cast<Word>(step_) << 32) | (static_cast<Word>(counter_) << 2) |
         kind;
}

void StepPool::quiesce() {
  __atomic_store_n(&phase_, nextPhase(kIdlePhase), __ATOMIC_SEQ_CST);

  for (int t = 1; t < threads_; ++t) {
    while (__atomic_load_n(&slots_[t].busy, __ATOMIC_SEQ_CST) != 0) {
      cpuRelax();
    }
  }
}

void StepPool::prepare() {
  if (simulator_->agents_.size() != agents_) {
    quiesce();
    agents_ = simulator_->agents_.size();
    published_.assign(agents_, step_);
  }
}

void StepPool::run() {
  if (agents_ == 0U) {
    return;
  }

  ++step_;
  runPhase(nextPhase(kAgentPhase));
  runPhase(nextPhase(kUpdatePhase));
}

void StepPool::setPrefVelocities(RVOSimulator::PrefVelocityFunction function,
                                 void *context) {
  if (agents_ == 0U) {
    return;
  }

  function_ = function;
  context_ = context;
  runPhase(nextPhase(kPrefPhase));
}

void StepPool::runPhase(Word phase) {
  const Word n = agents_;
  const Word t = static_cast<Word>(threads_);

  for (Word b = 0U; b < t; ++b) {
    __atomic_store_n(&slots_[b].end, (b + 1U) * n / t, __ATOMIC_RELAXED);
    __atomic_store_n(&slots_[b].done, 0U, __ATOMIC_RELAXED);
    __atomic_store_n(&slots_[b].claim, (phaseTag(phase) << 32) | (b * n / t),
                     __ATOMIC_RELEASE);
  }

  __atomic_store_n(&phase_, phase, __ATOMIC_SEQ_CST);
  participate(0, phase);
}

bool StepPool::claim(int block, Word phase, Word chunk, Word *begin,
                     Word *end) {
  Slot &slot = slots_[block];
  Word value = __atomic_load_n(&slot.claim, __ATOMIC_ACQUIRE);

  for (;;) {
    if ((value >> 32) != phaseTag(phase)) {
      return false;
    }

    const Word next = value & 0xffffffffULL;
    const Word last = __atomic_load_n(&slot.end, __ATOMIC_RELAXED);

    if (next >= last) {
      return false;
    }

    const Word stop = next + chunk < last ? next + chunk : last;

    if (__atomic_compare_exchange_n(&slot.claim, &value,
                                    (value & ~0xffffffffULL) | stop, false,
                                    __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
      *begin = next;
      *end = stop;
      return true;
    }
  }
}

bool StepPool::complete() const {
  Word sum = 0U;

  for (int t = 0; t < threads_; ++t) {
    sum += __atomic_load_n(&slots_[t].done, __ATOMIC_ACQUIRE);
  }

  return sum >= agents_;
}

void StepPool::markDone(int tid, Word count) {
  if (count != 0U) {
    __atomic_store_n(
        &slots_[tid].done,
        __atomic_load_n(&slots_[tid].done, __ATOMIC_RELAXED) + count,
        __ATOMIC_RELEASE);
  }
}

bool StepPool::runAgent(int tid, std::size_t agentNo, unsigned step) {
  if (__atomic_load_n(&published_[agentNo], __ATOMIC_ACQUIRE) == step) {
    return false;
  }

  simulator_->computeAgentInto(agentNo, scratch_[tid]);

  unsigned expected = step - 1U;

  if (__atomic_compare_exchange_n(&published_[agentNo], &expected, step, false,
                                  __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
    simulator_->publishAgent(agentNo, scratch_[tid]);
    return true;
  }

  return false;
}

void StepPool::redo(int tid, Word phase) {
  const unsigned step = phaseStep(phase);
  const std::size_t n = agents_;
  const std::size_t offset =
      static_cast<std::size_t>(tid) * n / static_cast<std::size_t>(threads_);

  for (std::size_t k = 0U; k < n; ++k) {
    if (__atomic_load_n(&phase_, __ATOMIC_ACQUIRE) != phase) {
      return;
    }

    const std::size_t i = (offset + k) % n;

    if (__atomic_load_n(&published_[i], __ATOMIC_ACQUIRE) != step &&
        runAgent(tid, i, step)) {
      markDone(tid, 1U);
    }
  }
}

void StepPool::participate(int tid, Word phase) {
  const Word kind = phaseKind(phase);
  const unsigned step = phaseStep(phase);
  const Word chunk = kind == kUpdatePhase ? kUpdateChunk : kAgentChunk;
  Word begin = 0U;
  Word end = 0U;

  for (int k = 0; k < threads_; ++k) {
    const int block = (tid + k) % threads_;

    while (claim(block, phase, chunk, &begin, &end)) {
      Word finished = 0U;

      if (kind == kAgentPhase) {
        for (Word i = begin; i < end; ++i) {
          if (runAgent(tid, static_cast<std::size_t>(i), step)) {
            ++finished;
          }
        }
      } else if (kind == kUpdatePhase) {
        for (Word i = begin; i < end; ++i) {
          simulator_->updateAgent(static_cast<std::size_t>(i));
        }

        finished = end - begin;
      } else {
        for (Word i = begin; i < end; ++i) {
          simulator_->setPrefVelocityFrom(static_cast<std::size_t>(i),
                                          function_, context_);
        }

        finished = end - begin;
      }

      markDone(tid, finished);
    }
  }

  /* Workers only stay to redo stalled agents; the caller waits for all. */
  if (tid != 0 && kind != kAgentPhase) {
    return;
  }

  double graceStart = monotonicUs();

  while (!complete()) {
    if (__atomic_load_n(&phase_, __ATOMIC_ACQUIRE) != phase) {
      return;
    }

    if (kind == kAgentPhase && monotonicUs() - graceStart > kRedoGraceUs) {
      redo(tid, phase);
      graceStart = monotonicUs();
    }

    cpuRelax();
  }
}
} /* namespace RVO */
#endif /* RVO_STEP_POOL */
