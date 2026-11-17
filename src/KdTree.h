/*
 * KdTree.h
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

#ifndef RVO_KD_TREE_H_
#define RVO_KD_TREE_H_

/**
 * @file  KdTree.h
 * @brief Declares the Frame and KdTree classes.
 */

#include <cstddef>
#include <vector>

#include "Agent.h"
#include "Team.h"
#include "Vector2.h"

namespace RVO {
class KdTree;
class Obstacle;
class RVOSimulator;

/**
 * @brief Defines the agents as one simulation step reads them: the state of
 *        every agent before the step and the agent k-D tree over those
 *        states.
 *
 * Nothing in a frame changes while the new velocities are computed. A step
 * reads one frame and writes the states after the step to another, which the
 * following step reads in turn. An agent is therefore moved as soon as its
 * new velocity is known, by the thread that computed it, and no thread waits
 * for the others before it writes.
 *
 * It also makes a step a hedged loop of the team. Computing an agent reads
 * the frame and writes the scratch of the calling thread, so an agent held by
 * a thread that is not running can be computed a second time by another
 * thread, with the same result. A frame is not written again while such a
 * thread can still be reading it.
 */
class Frame : public Team::Job {
 private:
  /**
   * @brief Defines an entry of the agent k-D tree. The entries of a leaf are
   *        next to each other, so a query reads the positions in a leaf from
   *        two or three cache lines instead of one per agent.
   */
  class Entry {
   public:
    /**
     * @brief The position of the agent.
     */
    Vector2 position;

    /**
     * @brief The number of the agent.
     */
    unsigned int agentNo;
  };

  class Node;

  /**
   * @brief     Constructs a frame instance.
   * @param[in] kdTree The k-D trees of the simulation.
   */
  explicit Frame(const KdTree *kdTree);

  /**
   * @brief Destroys this frame instance.
   */
  virtual ~Frame();

  /**
   * @brief         Builds the agent k-D tree over the states in this frame.
   * @param[in,out] order The agent numbers, in the order of the leaves of the
   *                      previous agent k-D tree on entry and of this one on
   *                      return.
   */
  void build(std::vector<unsigned int> &order); /* NOLINT(runtime/references) */

  /**
   * @brief     Recursive function to build an agent k-D tree.
   * @param[in] begin The beginning agent k-D tree entry.
   * @param[in] end   The ending agent k-D tree entry.
   * @param[in] node  The current agent k-D tree node.
   */
  void buildRecursive(std::size_t begin, std::size_t end, std::size_t node);

  /**
   * @brief     Computes the neighbors and the new velocity of an agent in the
   *            scratch of the calling thread.
   * @param[in] item   The number of the agent.
   * @param[in] thread The number of the calling thread.
   */
  virtual void compute(std::size_t item, std::size_t thread);

  /**
   * @brief         Recursive function to compute the agent neighbors of an
   *                agent.
   * @param[in,out] scratch The scratch in which the agent is computed.
   * @param[in]     agentNo The number of the agent.
   * @param[in,out] rangeSq The squared range around the agent.
   * @param[in]     node    The current agent k-D tree node.
   */
  void queryRecursive(AgentScratch *scratch, std::size_t agentNo,
                      float &rangeSq, /* NOLINT(runtime/references) */
                      std::size_t node) const;

  /**
   * @brief     Moves the agent that the calling thread has just computed:
   *            writes its state after the step to the next frame and to the
   *            agent, and hands the agent its neighbors and constraints.
   * @param[in] item   The number of the agent.
   * @param[in] thread The number of the calling thread.
   */
  virtual void store(std::size_t item, std::size_t thread);

  /* Not implemented. */
  Frame(const Frame &other);

  /* Not implemented. */
  Frame &operator=(const Frame &other);

  std::vector<AgentState, CacheLineAllocator<AgentState> > states_;
  std::vector<Entry> entries_;
  std::vector<Node> nodes_;
  std::vector<AgentScratch *> scratch_;
  Agent *const *agents_;
  Frame *next_;
  const KdTree *kdTree_;
  float timeStep_;

  friend class KdTree;
  friend class RVOSimulator;
};

/**
 * @brief Defines k-D trees for agents and static obstacles in the simulation.
 */
class KdTree {
 private:
  class ObstacleTreeNode;

  /**
   * @brief     Constructs a k-D tree instance.
   * @param[in] simulator The simulator instance.
   */
  explicit KdTree(RVOSimulator *simulator);

  /**
   * @brief Destroys this k-D tree instance.
   */
  ~KdTree();

  /**
   * @brief     Adds the state of a new agent to the frame that the next step
   *            reads.
   * @param[in] state The state of the agent.
   */
  void addAgent(const AgentState &state);

  /**
   * @brief     Builds an agent k-D tree in the frame that the next step
   *            reads, and picks a frame that no thread is reading for the
   *            states after the step.
   * @param[in] threads The number of threads that will compute agents.
   * @return    The frame that the step reads.
   */
  Frame *beginStep(std::size_t threads);

  /**
   * @brief Makes the frame that the step has written the frame that the next
   *        step reads.
   */
  void endStep();

  /**
   * @brief Builds an obstacle k-D tree.
   */
  void buildObstacleTree();

  /**
   * @brief     Recursive function to build an obstacle k-D tree.
   * @param[in] obstacles List of obstacles from which to build the obstacle k-D
   *                      tree.
   */
  ObstacleTreeNode *buildObstacleTreeRecursive(
      const std::vector<Obstacle *> &obstacles);

  /**
   * @brief     Computes the obstacle neighbors of the specified agent.
   * @param[in] agent   A pointer to the scratch of the agent for which
   *                    obstacle neighbors are to be computed.
   * @param[in] rangeSq The squared range around the agent.
   */
  void computeObstacleNeighbors(AgentScratch *agent, float rangeSq) const;

  /**
   * @brief     Deletes the specified obstacle tree node.
   * @param[in] node A pointer to the obstacle tree node to be deleted.
   */
  void deleteObstacleTree(ObstacleTreeNode *node);

  /**
   * @brief         Recursive function to compute the neighbors of the specified
   *                obstacle.
   * @param[in]     agent   A pointer to the scratch of the agent for which
   *                        neighbors are to be computed.
   * @param[in,out] rangeSq The squared range around the agent.
   * @param[in]     node    The current obstacle k-D tree node.
   */
  void queryObstacleTreeRecursive(AgentScratch *agent, float rangeSq,
                                  const ObstacleTreeNode *node) const;

  /**
   * @brief     Queries the visibility between two points within a specified
   *            radius.
   * @param[in] vector1 The first point between which visibility is to be
   * tested.
   * @param[in] vector2 The second point between which visibility is to be
   *                    tested.
   * @param[in] radius  The radius within which visibility is to be tested.
   * @return    True if q1 and q2 are mutually visible within the radius; false
   *            otherwise.
   */
  bool queryVisibility(const Vector2 &vector1, const Vector2 &vector2,
                       float radius) const;

  /**
   * @brief     Recursive function to query the visibility between two points
   *            within a specified radius.
   * @param[in] vector1 The first point between which visibility is to be
   *                    tested.
   * @param[in] vector2 The second point between which visibility is to be
   *                    tested.
   * @param[in] radius  The radius within which visibility is to be tested.
   * @param[in] node    The current obstacle k-D tree node.
   * @return    True if q1 and q2 are mutually visible within the radius; false
   *            otherwise.
   */
  bool queryVisibilityRecursive(const Vector2 &vector1, const Vector2 &vector2,
                                float radius,
                                const ObstacleTreeNode *node) const;

  /**
   * @brief Waits for the threads that fell behind in an earlier step. Call
   *        before changing anything that computing an agent reads, other than
   *        a frame.
   */
  void quiesce();

  /* Not implemented. */
  KdTree(const KdTree &other);

  /* Not implemented. */
  KdTree &operator=(const KdTree &other);

  std::vector<unsigned int> agents_;
  std::vector<Frame *> frames_;
  std::vector<AgentScratch *> scratch_;
  Frame *frame_;
  ObstacleTreeNode *obstacleTree_;
  RVOSimulator *simulator_;

  friend class Frame;
  friend class RVOSimulator;
};
} /* namespace RVO */

#endif /* RVO_KD_TREE_H_ */
