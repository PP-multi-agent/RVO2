/*
 * Team.h
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

#ifndef RVO_TEAM_H_
#define RVO_TEAM_H_

/**
 * @file  Team.h
 * @brief Declares the Team class.
 */

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <thread>
#include <vector>

namespace RVO {
/**
 * @brief Defines the threads that run the parallel loops of the library.
 *
 * A loop is over when every item is done, not when every thread has checked
 * in. A thread takes one chunk of items at a time: first from its own share
 * of the loop, then from the far end of the shares of the others. A thread
 * that the operating system is not running at the moment (asleep, preempted,
 * or on a virtual CPU that the host has paused) therefore holds back one
 * chunk at most, and the loop never waits for a thread that holds none. As
 * long as the threads keep pace, each computes the same items in every loop
 * and the data of an item stays in the cache of one core.
 *
 * run() calls the job exactly once for every item, so it still has to wait
 * for that one chunk. runHedged() does not: past a short grace period the
 * calling thread computes the item again and the first result is stored. The
 * job of a hedged loop has to make that harmless. It computes into storage of
 * the calling thread and reads nothing that changes while a thread may still
 * be computing; isPinned() tells whether one is.
 */
class Team {
 public:
  /**
   * @brief Defines the body of a parallel loop.
   */
  class Job {
   public:
    /**
     * @brief Destroys this job instance.
     */
    virtual ~Job();

    /**
     * @brief     Computes an item. In a hedged loop this is called from more
     *            than one thread for the same item, at the same time, and a
     *            call can return after the loop is over; it writes only to
     *            the storage of the calling thread.
     * @param[in] item   The number of the item.
     * @param[in] thread The number of the calling thread, less than the
     *                   number of threads of the loop.
     */
    virtual void compute(std::size_t item, std::size_t thread) = 0;

    /**
     * @brief     Stores what the calling thread has just computed for an
     *            item. Hedged loops call this exactly once for every item,
     *            before the loop is over.
     * @param[in] item   The number of the item.
     * @param[in] thread The number of the calling thread.
     */
    virtual void store(std::size_t item, std::size_t thread);
  };

  /**
   * @brief  Returns the threads of the process. They are started when a loop
   *         first asks for them and are never stopped.
   * @return The team.
   */
  static Team &instance();

  /**
   * @brief     Calls job->compute() exactly once for every item and returns
   *            when all calls have returned.
   * @param[in] job     The body of the loop.
   * @param[in] count   The number of items.
   * @param[in] threads The number of threads, including the calling thread.
   * @param[in] grain   The number of items that a thread takes at a time.
   *                    More than one pays off when an item costs less than
   *                    taking it.
   */
  void run(Job *job, std::size_t count, std::size_t threads,
           std::size_t grain);

  /**
   * @brief     Calls job->compute() at least once and job->store() exactly
   *            once for every item, and returns when all items are stored. A
   *            thread that fell behind can still be inside job->compute()
   *            after that; until isPinned(pin) is false, the job and
   *            everything it reads must stay as they are.
   * @param[in] job     The body of the loop.
   * @param[in] count   The number of items.
   * @param[in] threads The number of threads, including the calling thread.
   * @param[in] pin     Stands for the job and what it reads; not NULL.
   */
  void runHedged(Job *job, std::size_t count, std::size_t threads,
                 const void *pin);

  /**
   * @brief     Tells whether a thread can still be inside a hedged loop.
   * @param[in] pin The pin of the loop.
   * @return    True if what the pin stands for must not change yet.
   */
  bool isPinned(const void *pin) const;

  /**
   * @brief     Waits until no thread is inside a hedged loop any more.
   * @param[in] pin The pin of the loop.
   */
  void waitUnpinned(const void *pin) const;

 private:
  typedef unsigned long long Word;

  struct Slot;

  /**
   * @brief Constructs a team instance without threads.
   */
  Team();

  /**
   * @brief Defines what a thread knows about the loop it is in.
   */
  struct Loop {
    Job *job;
    std::atomic<Word> *flags;
    Word phase;
    std::size_t count;
    std::size_t grain;
    std::size_t threads;
    bool hedged;
  };

  /**
   * @brief     Runs a loop; see run() and runHedged().
   * @param[in] job     The body of the loop.
   * @param[in] count   The number of items.
   * @param[in] threads The number of threads, including the calling thread.
   * @param[in] grain   The number of items in a chunk.
   * @param[in] hedged  True for runHedged().
   * @param[in] pin     The pin of the loop.
   */
  void execute(Job *job, std::size_t count, std::size_t threads,
               std::size_t grain, bool hedged, const void *pin);

  /**
   * @brief     Takes a chunk if nobody has, and computes it.
   * @param[in] loop   The loop.
   * @param[in] chunk  The number of the chunk.
   * @param[in] thread The number of the calling thread.
   * @return    True if the chunk is done, false if another thread is still
   *            computing or storing it.
   */
  static bool attempt(const Loop &loop, std::size_t chunk, std::size_t thread);

  /**
   * @brief     Computes the items of a chunk.
   * @param[in] loop   The loop.
   * @param[in] chunk  The number of the chunk.
   * @param[in] thread The number of the calling thread.
   */
  static void compute(const Loop &loop, std::size_t chunk, std::size_t thread);

  /**
   * @brief     Marks a chunk that the calling thread has computed as done,
   *            storing it first in a hedged loop unless another thread has.
   * @param[in] loop   The loop.
   * @param[in] chunk  The number of the chunk.
   * @param[in] thread The number of the calling thread.
   */
  static void complete(const Loop &loop, std::size_t chunk,
                       std::size_t thread);

  /**
   * @brief     The loop as seen from one thread: its own share from the
   *            front, then the shares of the others from the back.
   * @param[in] loop   The loop.
   * @param[in] thread The number of the calling thread.
   * @param[in] busy   If not NULL, receives the chunks that other threads
   *                   were still computing.
   */
  static void work(const Loop &loop, std::size_t thread,
                   std::vector<std::size_t> *busy);

  /**
   * @brief     Waits on behalf of the calling thread of a loop for the chunks
   *            that other threads were still computing, and computes them
   *            again in a hedged loop when they take too long.
   * @param[in] loop    The loop.
   * @param[in] graceUs How long to wait before computing a chunk again, in
   *                    microseconds.
   */
  void finish(const Loop &loop, double graceUs);

  /**
   * @brief     The main function of a worker thread.
   * @param[in] thread The number of the thread.
   * @param[in] slot   The slot of the thread.
   */
  void workerMain(std::size_t thread, Slot *slot);

  /**
   * @brief     Blocks a worker thread until a loop opens.
   * @param[in] seen The last gate value that the thread has handled.
   */
  void sleep(Word seen);

  /* Not implemented. */
  Team(const Team &other);

  /* Not implemented. */
  Team &operator=(const Team &other);

  /* Twice the number of the latest loop, plus one while the loop is open. */
  std::atomic<Word> gate_;

  /* The open loop. Written only while the gate is closed. */
  std::atomic<Job *> job_;
  std::atomic<std::atomic<Word> *> flags_;
  std::atomic<std::size_t> count_;
  std::atomic<std::size_t> grain_;
  std::atomic<std::size_t> threads_;
  std::atomic<const void *> pin_;
  std::atomic<bool> hedged_;

  /* Keeps what the calling thread writes off the cache line of the gate. */
  char padding_[64];

  /* Owned by the thread that holds runMutex_. */
  std::mutex runMutex_;
  std::vector<std::thread> workers_;
  std::vector<Slot *> slots_;
  std::vector<std::size_t> busy_;
  std::size_t capacity_;
  Word phase_;

  /* Lets other threads read slots_ while a loop adds workers. */
  mutable std::mutex slotsMutex_;

  std::mutex sleepMutex_;
  std::condition_variable wake_;
  std::atomic<int> sleepers_;
  std::atomic<std::size_t> started_;
};
} /* namespace RVO */

#endif /* RVO_TEAM_H_ */
