/*
 * KdTree.cc
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
 * @file  KdTree.cc
 * @brief Defines the Frame and KdTree classes.
 */

#include "KdTree.h"

#include <algorithm>
#include <utility>

#include "Agent.h"
#include "Obstacle.h"
#include "RVOSimulator.h"
#include "Vector2.h"

namespace RVO {
namespace {
/**
 * @relates Frame
 * @brief   The maximum k-D tree node leaf size.
 */
const std::size_t RVO_MAX_LEAF_SIZE = 10U;
} /* namespace */

/**
 * @brief Defines an agent k-D tree node. Two nodes fit in a cache line; the
 *        tree is written by one thread and read by all of them in every
 *        step.
 */
class Frame::Node {
 public:
  /**
   * @brief Constructs an agent k-D tree node instance.
   */
  Node();

  /**
   * @brief The beginning node number.
   */
  unsigned int begin;

  /**
   * @brief The ending node number.
   */
  unsigned int end;

  /**
   * @brief The left node number.
   */
  unsigned int left;

  /**
   * @brief The right node number.
   */
  unsigned int right;

  /**
   * @brief The maximum x-coordinate.
   */
  float maxX;

  /**
   * @brief The maximum y-coordinate.
   */
  float maxY;

  /**
   * @brief The minimum x-coordinate.
   */
  float minX;

  /**
   * @brief The minimum y-coordinate.
   */
  float minY;
};

Frame::Node::Node()
    : begin(0U),
      end(0U),
      left(0U),
      right(0U),
      maxX(0.0F),
      maxY(0.0F),
      minX(0.0F),
      minY(0.0F) {}

/**
 * @brief Defines an obstacle k-D tree node.
 */
class KdTree::ObstacleTreeNode {
 public:
  /**
   * @brief Constructs an obstacle k-D tree node instance.
   */
  ObstacleTreeNode();

  /**
   * @brief Destroys this obstacle k-D tree node instance.
   */
  ~ObstacleTreeNode();

  /**
   * @brief The obstacle number.
   */
  const Obstacle *obstacle;

  /**
   * @brief The left obstacle tree node.
   */
  ObstacleTreeNode *left;

  /**
   * @brief The right obstacle tree node.
   */
  ObstacleTreeNode *right;

 private:
  /* Not implemented. */
  ObstacleTreeNode(const ObstacleTreeNode &other);

  /* Not implemented. */
  ObstacleTreeNode &operator=(const ObstacleTreeNode &other);
};

KdTree::ObstacleTreeNode::ObstacleTreeNode()
    : obstacle(NULL), left(NULL), right(NULL) {}

KdTree::ObstacleTreeNode::~ObstacleTreeNode() {}

Frame::Frame(const KdTree *kdTree)
    : agents_(NULL), next_(NULL), kdTree_(kdTree), timeStep_(0.0F) {}

Frame::~Frame() {}

void Frame::build(std::vector<unsigned int> &order) {
  const std::size_t count = states_.size();

  if (entries_.size() != count) {
    entries_.resize(count);
    nodes_.resize(count == 0U ? 0U : 2U * count - 1U);
  }

  for (std::size_t i = 0U; i < count; ++i) {
    entries_[i].position = states_[order[i]].position_;
    entries_[i].agentNo = order[i];
  }

  if (count != 0U) {
    buildRecursive(0U, count, 0U);
  }

  for (std::size_t i = 0U; i < count; ++i) {
    order[i] = entries_[i].agentNo;
  }
}

void Frame::buildRecursive(std::size_t begin, std::size_t end,
                           std::size_t node) {
  nodes_[node].begin = static_cast<unsigned int>(begin);
  nodes_[node].end = static_cast<unsigned int>(end);
  nodes_[node].minX = nodes_[node].maxX = entries_[begin].position.x();
  nodes_[node].minY = nodes_[node].maxY = entries_[begin].position.y();

  for (std::size_t i = begin + 1U; i < end; ++i) {
    nodes_[node].maxX = std::max(nodes_[node].maxX, entries_[i].position.x());
    nodes_[node].minX = std::min(nodes_[node].minX, entries_[i].position.x());
    nodes_[node].maxY = std::max(nodes_[node].maxY, entries_[i].position.y());
    nodes_[node].minY = std::min(nodes_[node].minY, entries_[i].position.y());
  }

  if (end - begin > RVO_MAX_LEAF_SIZE) {
    /* No leaf node. */
    const bool isVertical = nodes_[node].maxX - nodes_[node].minX >
                            nodes_[node].maxY - nodes_[node].minY;
    const float splitValue =
        0.5F * (isVertical ? nodes_[node].maxX + nodes_[node].minX
                           : nodes_[node].maxY + nodes_[node].minY);

    std::size_t left = begin;
    std::size_t right = end;

    while (left < right) {
      while (left < right &&
             (isVertical ? entries_[left].position.x()
                         : entries_[left].position.y()) < splitValue) {
        ++left;
      }

      while (right > left &&
             (isVertical ? entries_[right - 1U].position.x()
                         : entries_[right - 1U].position.y()) >= splitValue) {
        --right;
      }

      if (left < right) {
        std::swap(entries_[left], entries_[right - 1U]);
        ++left;
        --right;
      }
    }

    if (left == begin) {
      ++left;
      ++right;
    }

    nodes_[node].left = static_cast<unsigned int>(node + 1U);
    nodes_[node].right = static_cast<unsigned int>(node + 2U * (left - begin));

    buildRecursive(begin, left, nodes_[node].left);
    buildRecursive(left, end, nodes_[node].right);
  }
}

void Frame::compute(std::size_t item, std::size_t thread) {
  AgentScratch *const scratch = scratch_[thread];
  static_cast<AgentState &>(*scratch) = states_[item];
  scratch->states_ = &states_[0];

  scratch->obstacleNeighbors_.clear();
  const float range =
      scratch->timeHorizonObst_ * scratch->maxSpeed_ + scratch->radius_;
  kdTree_->computeObstacleNeighbors(scratch, range * range);

  scratch->agentNeighbors_.clear();

  if (scratch->maxNeighbors_ > 0U) {
    float rangeSq = scratch->neighborDist_ * scratch->neighborDist_;
    queryRecursive(scratch, item, rangeSq, 0U);
  }

  scratch->computeNewVelocity(timeStep_);
}

void Frame::queryRecursive(AgentScratch *scratch, std::size_t agentNo,
                           float &rangeSq, std::size_t node) const {
  if (nodes_[node].end - nodes_[node].begin <= RVO_MAX_LEAF_SIZE) {
    for (std::size_t i = nodes_[node].begin; i < nodes_[node].end; ++i) {
      if (entries_[i].agentNo != agentNo) {
        scratch->insertAgentNeighbor(entries_[i].position, entries_[i].agentNo,
                                     rangeSq);
      }
    }
  } else {
    const float distLeftMinX = std::max(
        0.0F, nodes_[nodes_[node].left].minX - scratch->position_.x());
    const float distLeftMaxX = std::max(
        0.0F, scratch->position_.x() - nodes_[nodes_[node].left].maxX);
    const float distLeftMinY = std::max(
        0.0F, nodes_[nodes_[node].left].minY - scratch->position_.y());
    const float distLeftMaxY = std::max(
        0.0F, scratch->position_.y() - nodes_[nodes_[node].left].maxY);

    const float distSqLeft =
        distLeftMinX * distLeftMinX + distLeftMaxX * distLeftMaxX +
        distLeftMinY * distLeftMinY + distLeftMaxY * distLeftMaxY;

    const float distRightMinX = std::max(
        0.0F, nodes_[nodes_[node].right].minX - scratch->position_.x());
    const float distRightMaxX = std::max(
        0.0F, scratch->position_.x() - nodes_[nodes_[node].right].maxX);
    const float distRightMinY = std::max(
        0.0F, nodes_[nodes_[node].right].minY - scratch->position_.y());
    const float distRightMaxY = std::max(
        0.0F, scratch->position_.y() - nodes_[nodes_[node].right].maxY);

    const float distSqRight =
        distRightMinX * distRightMinX + distRightMaxX * distRightMaxX +
        distRightMinY * distRightMinY + distRightMaxY * distRightMaxY;

    if (distSqLeft < distSqRight) {
      if (distSqLeft < rangeSq) {
        queryRecursive(scratch, agentNo, rangeSq, nodes_[node].left);

        if (distSqRight < rangeSq) {
          queryRecursive(scratch, agentNo, rangeSq, nodes_[node].right);
        }
      }
    } else if (distSqRight < rangeSq) {
      queryRecursive(scratch, agentNo, rangeSq, nodes_[node].right);

      if (distSqLeft < rangeSq) {
        queryRecursive(scratch, agentNo, rangeSq, nodes_[node].left);
      }
    }
  }
}

void Frame::store(std::size_t item, std::size_t thread) {
  AgentScratch *const scratch = scratch_[thread];
  Agent *const agent = agents_[item];
  AgentState &state = next_->states_[item];

  state = *scratch;
  state.velocity_ = scratch->newVelocity_;
  state.position_ += state.velocity_ * timeStep_;

  agent->velocity_ = state.velocity_;
  agent->position_ = state.position_;
  scratch->store(agent);
}

KdTree::KdTree(RVOSimulator *simulator)
    : frame_(new Frame(this)), obstacleTree_(NULL), simulator_(simulator) {
  frames_.push_back(frame_);
}

KdTree::~KdTree() {
  quiesce();
  deleteObstacleTree(obstacleTree_);

  for (std::size_t i = 0U; i < frames_.size(); ++i) {
    delete frames_[i];
  }

  for (std::size_t i = 0U; i < scratch_.size(); ++i) {
    delete scratch_[i];
  }
}

void KdTree::addAgent(const AgentState &state) {
  frame_->states_.push_back(state);
}

Frame *KdTree::beginStep(std::size_t threads) {
  Frame *const frame = frame_;
  const std::size_t count = frame->states_.size();

  for (std::size_t i = agents_.size(); i < count; ++i) {
    agents_.push_back(static_cast<unsigned int>(i));
  }

  /* Almost always the frame that the previous step read. A thread that fell
   * behind in that step keeps the frame until it notices, and this step
   * writes to another. */
  Frame *next = NULL;

  for (std::size_t i = 0U; i < frames_.size() && next == NULL; ++i) {
    if (frames_[i] != frame && !Team::instance().isPinned(frames_[i])) {
      next = frames_[i];
    }
  }

  if (next == NULL) {
    next = new Frame(this);
    frames_.push_back(next);
  }

  next->states_.resize(count);

  while (scratch_.size() < threads) {
    scratch_.push_back(new AgentScratch());
  }

  frame->scratch_ = scratch_;
  frame->agents_ = count == 0U ? NULL : &simulator_->agents_[0];
  frame->next_ = next;
  frame->timeStep_ = simulator_->timeStep_;
  frame->build(agents_);

  return frame;
}

void KdTree::endStep() { frame_ = frame_->next_; }

void KdTree::buildObstacleTree() {
  quiesce();
  deleteObstacleTree(obstacleTree_);

  const std::vector<Obstacle *> obstacles(simulator_->obstacles_);
  obstacleTree_ = buildObstacleTreeRecursive(obstacles);
}

KdTree::ObstacleTreeNode *KdTree::buildObstacleTreeRecursive(
    const std::vector<Obstacle *> &obstacles) {
  if (!obstacles.empty()) {
    ObstacleTreeNode *const node = new ObstacleTreeNode();

    std::size_t optimalSplit = 0U;
    std::size_t minLeft = obstacles.size();
    std::size_t minRight = obstacles.size();

    for (std::size_t i = 0U; i < obstacles.size(); ++i) {
      std::size_t leftSize = 0U;
      std::size_t rightSize = 0U;

      const Obstacle *const obstacleI1 = obstacles[i];
      const Obstacle *const obstacleI2 = obstacleI1->next_;

      /* Compute optimal split node. */
      for (std::size_t j = 0U; j < obstacles.size(); ++j) {
        if (i != j) {
          const Obstacle *const obstacleJ1 = obstacles[j];
          const Obstacle *const obstacleJ2 = obstacleJ1->next_;

          const float j1LeftOfI = leftOf(obstacleI1->point_, obstacleI2->point_,
                                         obstacleJ1->point_);
          const float j2LeftOfI = leftOf(obstacleI1->point_, obstacleI2->point_,
                                         obstacleJ2->point_);

          if (j1LeftOfI >= -RVO_EPSILON && j2LeftOfI >= -RVO_EPSILON) {
            ++leftSize;
          } else if (j1LeftOfI <= RVO_EPSILON && j2LeftOfI <= RVO_EPSILON) {
            ++rightSize;
          } else {
            ++leftSize;
            ++rightSize;
          }

          if (std::make_pair(std::max(leftSize, rightSize),
                             std::min(leftSize, rightSize)) >=
              std::make_pair(std::max(minLeft, minRight),
                             std::min(minLeft, minRight))) {
            break;
          }
        }
      }

      if (std::make_pair(std::max(leftSize, rightSize),
                         std::min(leftSize, rightSize)) <
          std::make_pair(std::max(minLeft, minRight),
                         std::min(minLeft, minRight))) {
        minLeft = leftSize;
        minRight = rightSize;
        optimalSplit = i;
      }
    }

    /* Build split node. */
    std::vector<Obstacle *> leftObstacles(minLeft);
    std::vector<Obstacle *> rightObstacles(minRight);

    std::size_t leftCounter = 0U;
    std::size_t rightCounter = 0U;
    const std::size_t i = optimalSplit;

    const Obstacle *const obstacleI1 = obstacles[i];
    const Obstacle *const obstacleI2 = obstacleI1->next_;

    for (std::size_t j = 0U; j < obstacles.size(); ++j) {
      if (i != j) {
        Obstacle *const obstacleJ1 = obstacles[j];
        Obstacle *const obstacleJ2 = obstacleJ1->next_;

        const float j1LeftOfI =
            leftOf(obstacleI1->point_, obstacleI2->point_, obstacleJ1->point_);
        const float j2LeftOfI =
            leftOf(obstacleI1->point_, obstacleI2->point_, obstacleJ2->point_);

        if (j1LeftOfI >= -RVO_EPSILON && j2LeftOfI >= -RVO_EPSILON) {
          leftObstacles[leftCounter++] = obstacles[j];
        } else if (j1LeftOfI <= RVO_EPSILON && j2LeftOfI <= RVO_EPSILON) {
          rightObstacles[rightCounter++] = obstacles[j];
        } else {
          /* Split obstacle j. */
          const float t = det(obstacleI2->point_ - obstacleI1->point_,
                              obstacleJ1->point_ - obstacleI1->point_) /
                          det(obstacleI2->point_ - obstacleI1->point_,
                              obstacleJ1->point_ - obstacleJ2->point_);

          const Vector2 splitPoint =
              obstacleJ1->point_ +
              t * (obstacleJ2->point_ - obstacleJ1->point_);

          Obstacle *const newObstacle = new Obstacle();
          newObstacle->direction_ = obstacleJ1->direction_;
          newObstacle->point_ = splitPoint;
          newObstacle->next_ = obstacleJ2;
          newObstacle->previous_ = obstacleJ1;
          newObstacle->id_ = simulator_->obstacles_.size();
          newObstacle->isConvex_ = true;
          simulator_->obstacles_.push_back(newObstacle);

          obstacleJ1->next_ = newObstacle;
          obstacleJ2->previous_ = newObstacle;

          if (j1LeftOfI > 0.0F) {
            leftObstacles[leftCounter++] = obstacleJ1;
            rightObstacles[rightCounter++] = newObstacle;
          } else {
            rightObstacles[rightCounter++] = obstacleJ1;
            leftObstacles[leftCounter++] = newObstacle;
          }
        }
      }
    }

    node->obstacle = obstacleI1;
    node->left = buildObstacleTreeRecursive(leftObstacles);
    node->right = buildObstacleTreeRecursive(rightObstacles);

    return node;
  }

  return NULL;
}

void KdTree::computeObstacleNeighbors(AgentScratch *agent,
                                      float rangeSq) const {
  queryObstacleTreeRecursive(agent, rangeSq, obstacleTree_);
}

void KdTree::deleteObstacleTree(ObstacleTreeNode *node) {
  if (node != NULL) {
    deleteObstacleTree(node->left);
    deleteObstacleTree(node->right);
    delete node;
  }
}

void KdTree::queryObstacleTreeRecursive(AgentScratch *agent, float rangeSq,
                                        const ObstacleTreeNode *node) const {
  if (node != NULL) {
    const Obstacle *const obstacle1 = node->obstacle;
    const Obstacle *const obstacle2 = obstacle1->next_;

    const float agentLeftOfLine =
        leftOf(obstacle1->point_, obstacle2->point_, agent->position_);

    queryObstacleTreeRecursive(
        agent, rangeSq, agentLeftOfLine >= 0.0F ? node->left : node->right);

    const float distSqLine = agentLeftOfLine * agentLeftOfLine /
                             absSq(obstacle2->point_ - obstacle1->point_);

    if (distSqLine < rangeSq) {
      if (agentLeftOfLine < 0.0F) {
        /* Try obstacle at this node only if agent is on right side of obstacle
         * and can see obstacle. */
        agent->insertObstacleNeighbor(node->obstacle, rangeSq);
      }

      /* Try other side of line. */
      queryObstacleTreeRecursive(
          agent, rangeSq, agentLeftOfLine >= 0.0F ? node->right : node->left);
    }
  }
}

bool KdTree::queryVisibility(const Vector2 &vector1, const Vector2 &vector2,
                             float radius) const {
  return queryVisibilityRecursive(vector1, vector2, radius, obstacleTree_);
}

bool KdTree::queryVisibilityRecursive(const Vector2 &vector1,
                                      const Vector2 &vector2, float radius,
                                      const ObstacleTreeNode *node) const {
  if (node != NULL) {
    const Obstacle *const obstacle1 = node->obstacle;
    const Obstacle *const obstacle2 = obstacle1->next_;

    const float q1LeftOfI =
        leftOf(obstacle1->point_, obstacle2->point_, vector1);
    const float q2LeftOfI =
        leftOf(obstacle1->point_, obstacle2->point_, vector2);
    const float invLengthI =
        1.0F / absSq(obstacle2->point_ - obstacle1->point_);

    if (q1LeftOfI >= 0.0F && q2LeftOfI >= 0.0F) {
      return queryVisibilityRecursive(vector1, vector2, radius, node->left) &&
             ((q1LeftOfI * q1LeftOfI * invLengthI >= radius * radius &&
               q2LeftOfI * q2LeftOfI * invLengthI >= radius * radius) ||
              queryVisibilityRecursive(vector1, vector2, radius, node->right));
    }

    if (q1LeftOfI <= 0.0F && q2LeftOfI <= 0.0F) {
      return queryVisibilityRecursive(vector1, vector2, radius, node->right) &&
             ((q1LeftOfI * q1LeftOfI * invLengthI >= radius * radius &&
               q2LeftOfI * q2LeftOfI * invLengthI >= radius * radius) ||
              queryVisibilityRecursive(vector1, vector2, radius, node->left));
    }

    if (q1LeftOfI >= 0.0F && q2LeftOfI <= 0.0F) {
      /* One can see through obstacle from left to right. */
      return queryVisibilityRecursive(vector1, vector2, radius, node->left) &&
             queryVisibilityRecursive(vector1, vector2, radius, node->right);
    }

    const float point1LeftOfQ = leftOf(vector1, vector2, obstacle1->point_);
    const float point2LeftOfQ = leftOf(vector1, vector2, obstacle2->point_);
    const float invLengthQ = 1.0F / absSq(vector2 - vector1);

    return point1LeftOfQ * point2LeftOfQ >= 0.0F &&
           point1LeftOfQ * point1LeftOfQ * invLengthQ > radius * radius &&
           point2LeftOfQ * point2LeftOfQ * invLengthQ > radius * radius &&
           queryVisibilityRecursive(vector1, vector2, radius, node->left) &&
           queryVisibilityRecursive(vector1, vector2, radius, node->right);
  }

  return true;
}

void KdTree::quiesce() {
  for (std::size_t i = 0U; i < frames_.size(); ++i) {
    Team::instance().waitUnpinned(frames_[i]);
  }
}
} /* namespace RVO */
