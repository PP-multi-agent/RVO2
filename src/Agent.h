/*
 * Agent.h
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

#ifndef RVO_AGENT_H_
#define RVO_AGENT_H_

/**
 * @file  Agent.h
 * @brief Declares the Agent, AgentState and AgentScratch classes.
 */

#include <cstddef>
#include <utility>
#include <vector>

#include "Line.h"
#include "Vector2.h"

namespace RVO {
class Frame;
class KdTree;
class Obstacle;

/**
 * @brief The size of a cache line, in bytes.
 */
const std::size_t RVO_CACHE_LINE = 64U;

/**
 * @brief Defines what a simulation step reads from an agent. A state fills
 *        one cache line, so the states of two agents never share one.
 */
class AgentState {
 public:
  /**
   * @brief Constructs an agent state instance.
   */
  AgentState();

  Vector2 position_;
  Vector2 prefVelocity_;
  Vector2 velocity_;
  std::size_t maxNeighbors_;
  float maxSpeed_;
  float neighborDist_;
  float radius_;
  float timeHorizon_;
  float timeHorizonObst_;
  char padding_[RVO_CACHE_LINE - 3U * sizeof(Vector2) - sizeof(std::size_t) -
                5U * sizeof(float)];
};

/**
 * @brief Defines an allocator that starts every allocation on a cache line.
 */
template <class T>
class CacheLineAllocator {
 public:
  typedef T value_type;

  template <class U>
  struct rebind {
    typedef CacheLineAllocator<U> other;
  };

  CacheLineAllocator() {}

  template <class U>
  CacheLineAllocator(const CacheLineAllocator<U> & /*other*/) {}

  T *allocate(std::size_t count) {
    /* Room to move up to a cache line, and for the pointer to give back. */
    void *const block = ::operator new(count * sizeof(T) + RVO_CACHE_LINE +
                                       sizeof(void *));
    const std::size_t address =
        (reinterpret_cast<std::size_t>(block) + sizeof(void *) +
         RVO_CACHE_LINE - 1U) &
        ~(RVO_CACHE_LINE - 1U);
    reinterpret_cast<void **>(address)[-1] = block;
    return reinterpret_cast<T *>(address);
  }

  void deallocate(T *pointer, std::size_t /*count*/) {
    ::operator delete(reinterpret_cast<void **>(pointer)[-1]);
  }
};

template <class T, class U>
bool operator==(const CacheLineAllocator<T> & /*allocator1*/,
                const CacheLineAllocator<U> & /*allocator2*/) {
  return true;
}

template <class T, class U>
bool operator!=(const CacheLineAllocator<T> & /*allocator1*/,
                const CacheLineAllocator<U> & /*allocator2*/) {
  return false;
}

/**
 * @brief Defines an agent in the simulation: its state, and the neighbors and
 *        constraints that the latest step found for it.
 */
class Agent : public AgentState {
 private:
  /**
   * @brief Constructs an agent instance.
   */
  Agent();

  /**
   * @brief Destroys this agent instance.
   */
  ~Agent();

  /* Not implemented. */
  Agent(const Agent &other);

  /* Not implemented. */
  Agent &operator=(const Agent &other);

  std::vector<std::pair<float, std::size_t> > agentNeighbors_;
  std::vector<std::pair<float, const Obstacle *> > obstacleNeighbors_;
  std::vector<Line> orcaLines_;
  std::size_t id_;

  friend class AgentScratch;
  friend class Frame;
  friend class KdTree;
  friend class RVOSimulator;
};

/**
 * @brief Defines the storage in which one thread computes the neighbors and
 *        the new velocity of one agent at a time. A thread writes nothing
 *        else while it computes, so two threads can compute the same agent
 *        at once and either result can be stored.
 */
class AgentScratch : public AgentState {
 private:
  /**
   * @brief Constructs an agent scratch instance.
   */
  AgentScratch();

  /**
   * @brief     Computes the new velocity of the agent held in this scratch.
   * @param[in] timeStep The time step of the simulation.
   */
  void computeNewVelocity(float timeStep);

  /**
   * @brief          Inserts an agent neighbor into the set of neighbors of the
   *                 agent held in this scratch.
   * @param[in]      position The position of the agent to be inserted.
   * @param[in]      agentNo  The number of the agent to be inserted.
   * @param[in, out] rangeSq  The squared range around the agent.
   */
  void insertAgentNeighbor(const Vector2 &position, std::size_t agentNo,
                           float &rangeSq); /* NOLINT(runtime/references) */

  /**
   * @brief          Inserts a static obstacle neighbor into the set of
   *                 neighbors of the agent held in this scratch.
   * @param[in]      obstacle The number of the static obstacle to be inserted.
   * @param[in, out] rangeSq  The squared range around the agent.
   */
  void insertObstacleNeighbor(const Obstacle *obstacle, float rangeSq);

  /**
   * @brief         Hands the neighbors and constraints in this scratch to an
   *                agent and takes the previous ones of the agent in
   *                exchange, so nothing is copied or allocated.
   * @param[in,out] agent The agent that was computed in this scratch.
   */
  void store(Agent *agent);

  /* Not implemented. */
  AgentScratch(const AgentScratch &other);

  /* Not implemented. */
  AgentScratch &operator=(const AgentScratch &other);

  std::vector<std::pair<float, std::size_t> > agentNeighbors_;
  std::vector<std::pair<float, const Obstacle *> > obstacleNeighbors_;
  std::vector<Line> orcaLines_;
  Vector2 newVelocity_;
  /* The state of every agent, by agent number, as the step reads it. */
  const AgentState *states_;
  /* Keeps the scratch of one thread off the cache lines of the next. */
  char scratchPadding_[RVO_CACHE_LINE];

  friend class Frame;
  friend class KdTree;
};
} /* namespace RVO */

#endif /* RVO_AGENT_H_ */
