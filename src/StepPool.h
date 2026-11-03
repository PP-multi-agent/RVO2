/*
 * StepPool.h
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

#ifndef RVO_STEP_POOL_H_
#define RVO_STEP_POOL_H_

/**
 * @file  StepPool.h
 * @brief Declares the StepPool class.
 */

#if defined(_OPENMP) && defined(__linux__) && \
    (defined(__GNUC__) || defined(__clang__))
#define RVO_STEP_POOL 1
#else
#define RVO_STEP_POOL 0
#endif

#if RVO_STEP_POOL
#include <pthread.h>

#include <cstddef>
#include <vector>

#include "RVOSimulator.h"

namespace RVO {
class Agent;

/**
 * @brief Worker threads for RVOSimulator::doStep() and
 *        RVOSimulator::setAgentPrefVelocities() when OpenMP asks for more
 *        than one thread.
 *
 * An OpenMP parallel region ends only when every thread of its team reaches
 * the closing barrier. When the operating system or a hypervisor pauses one
 * worker for a few milliseconds, the whole step waits for it, even after the
 * other threads have finished its share. Here a phase ends when every agent
 * is done, whichever thread did it.
 *
 * New velocities: each thread computes an agent into its own scratch agent,
 * and the first thread to finish moves the result into the real agent. Past a
 * grace period, idle threads compute agents that are still missing, so a
 * paused worker costs only the agents it was holding. The result does not
 * depend on which thread computed it.
 *
 * Positions and preferred velocities are handed out once each, in chunks.
 *
 * A paused worker can resume after its phase has ended. It then only reads
 * shared state, fails to publish, and waits again. Anything that reallocates
 * agents, obstacles or the k-D trees calls quiesce() first.
 */
class StepPool {
 public:
  /**
   * @brief     Starts threads - 1 workers; the calling thread is the last one.
   * @param[in] simulator The simulator whose agents are stepped.
   * @param[in] threads   The number of threads, including the caller.
   */
  StepPool(RVOSimulator *simulator, int threads);

  /**
   * @brief Stops and joins the workers.
   */
  ~StepPool();

  /**
   * @brief Catches up with agents added since the last step. Call before the
   *        agent k-D tree is built.
   */
  void prepare();

  /**
   * @brief Computes the new velocity of every agent, then moves every agent.
   *        Call after the agent k-D tree is built.
   */
  void run();

  /**
   * @brief     Sets the preferred velocity of every agent from function.
   * @param[in] function Called once for every agent.
   * @param[in] context  Passed to function unchanged.
   */
  void setPrefVelocities(RVOSimulator::PrefVelocityFunction function,
                         void *context);

  /**
   * @brief Waits until no worker is inside a phase.
   */
  void quiesce();

  /**
   * @brief  The number of threads, including the caller.
   * @return The number of threads.
   */
  int threads() const { return threads_; }

 private:
  typedef unsigned long long Word;

  struct Slot {
    /* Written when agents are handed out, read by threads looking for work. */
    Word claim; /* (phase tag << 32) | next agent to hand out */
    Word end;   /* one past the last agent of this block */
    char claimPad[64 - 2 * sizeof(Word)];
    /* Written by the owning thread, read by threads waiting for the phase. */
    Word done; /* agents this thread finished in the current phase */
    int busy;  /* the thread is inside a phase */
    char donePad[64 - sizeof(Word) - sizeof(int)];
  };

  struct Start {
    StepPool *pool;
    int tid;
  };

  static void *workerMain(void *arg);
  void workerLoop(int tid);
  Word nextPhase(Word kind);
  void runPhase(Word phase);
  void participate(int tid, Word phase);
  bool claim(int block, Word phase, Word chunk, Word *begin, Word *end);
  bool complete() const;
  void markDone(int tid, Word count);
  bool runAgent(int tid, std::size_t agentNo, unsigned step);
  void redo(int tid, Word phase);

  /* Not implemented. */
  StepPool(const StepPool &other);

  /* Not implemented. */
  StepPool &operator=(const StepPool &other);

  RVOSimulator *simulator_;
  RVOSimulator::PrefVelocityFunction function_;
  void *context_;
  Slot *slots_;
  int threads_;
  std::size_t agents_;
  unsigned step_;
  unsigned counter_;
  Word phase_;
  int stop_;
  std::vector<unsigned> published_;
  std::vector<Agent *> scratch_;
  std::vector<pthread_t> handles_;
  std::vector<Start> starts_;
};
} /* namespace RVO */
#endif /* RVO_STEP_POOL */

#endif /* RVO_STEP_POOL_H_ */
