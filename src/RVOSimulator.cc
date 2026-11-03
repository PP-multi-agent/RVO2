/*
 * RVOSimulator.cc
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
 * @file  RVOSimulator.cc
 * @brief Defines the RVOSimulator class.
 */

#include "RVOSimulator.h"

#include <limits>
#include <utility>

#include "Agent.h"
#include "KdTree.h"
#include "Line.h"
#include "Obstacle.h"
#include "StepPool.h"
#include "Vector2.h"

#ifdef _OPENMP
#include <omp.h>
#endif /* _OPENMP */

namespace RVO {
const std::size_t RVO_ERROR = std::numeric_limits<std::size_t>::max();

RVOSimulator::RVOSimulator()
    : defaultAgent_(NULL),
      kdTree_(new KdTree(this)),
      globalTime_(0.0F),
      timeStep_(0.0F),
      pool_(NULL) {}

RVOSimulator::RVOSimulator(float timeStep, float neighborDist,
                           std::size_t maxNeighbors, float timeHorizon,
                           float timeHorizonObst, float radius, float maxSpeed)
    : defaultAgent_(new Agent()),
      kdTree_(new KdTree(this)),
      globalTime_(0.0F),
      timeStep_(timeStep),
      pool_(NULL) {
  defaultAgent_->maxNeighbors_ = maxNeighbors;
  defaultAgent_->maxSpeed_ = maxSpeed;
  defaultAgent_->neighborDist_ = neighborDist;
  defaultAgent_->radius_ = radius;
  defaultAgent_->timeHorizon_ = timeHorizon;
  defaultAgent_->timeHorizonObst_ = timeHorizonObst;
}

RVOSimulator::RVOSimulator(float timeStep, float neighborDist,
                           std::size_t maxNeighbors, float timeHorizon,
                           float timeHorizonObst, float radius, float maxSpeed,
                           const Vector2 &velocity)
    : defaultAgent_(new Agent()),
      kdTree_(new KdTree(this)),
      globalTime_(0.0F),
      timeStep_(timeStep),
      pool_(NULL) {
  defaultAgent_->velocity_ = velocity;
  defaultAgent_->maxNeighbors_ = maxNeighbors;
  defaultAgent_->maxSpeed_ = maxSpeed;
  defaultAgent_->neighborDist_ = neighborDist;
  defaultAgent_->radius_ = radius;
  defaultAgent_->timeHorizon_ = timeHorizon;
  defaultAgent_->timeHorizonObst_ = timeHorizonObst;
}

RVOSimulator::~RVOSimulator() {
#if RVO_STEP_POOL
  delete pool_;
#endif /* RVO_STEP_POOL */
  delete defaultAgent_;
  delete kdTree_;

  for (std::size_t i = 0U; i < agents_.size(); ++i) {
    delete agents_[i];
  }

  for (std::size_t i = 0U; i < obstacles_.size(); ++i) {
    delete obstacles_[i];
  }
}

std::size_t RVOSimulator::addAgent(const Vector2 &position) {
#if RVO_STEP_POOL
  if (pool_ != NULL) {
    pool_->quiesce();
  }
#endif /* RVO_STEP_POOL */

  if (defaultAgent_ != NULL) {
    Agent *const agent = new Agent();
    agent->position_ = position;
    agent->velocity_ = defaultAgent_->velocity_;
    agent->id_ = agents_.size();
    agent->maxNeighbors_ = defaultAgent_->maxNeighbors_;
    agent->maxSpeed_ = defaultAgent_->maxSpeed_;
    agent->neighborDist_ = defaultAgent_->neighborDist_;
    agent->radius_ = defaultAgent_->radius_;
    agent->timeHorizon_ = defaultAgent_->timeHorizon_;
    agent->timeHorizonObst_ = defaultAgent_->timeHorizonObst_;
    agents_.push_back(agent);

    return agents_.size() - 1U;
  }

  return RVO_ERROR;
}

std::size_t RVOSimulator::addAgent(const Vector2 &position, float neighborDist,
                                   std::size_t maxNeighbors, float timeHorizon,
                                   float timeHorizonObst, float radius,
                                   float maxSpeed) {
  return addAgent(position, neighborDist, maxNeighbors, timeHorizon,
                  timeHorizonObst, radius, maxSpeed, Vector2());
}

std::size_t RVOSimulator::addAgent(const Vector2 &position, float neighborDist,
                                   std::size_t maxNeighbors, float timeHorizon,
                                   float timeHorizonObst, float radius,
                                   float maxSpeed, const Vector2 &velocity) {
#if RVO_STEP_POOL
  if (pool_ != NULL) {
    pool_->quiesce();
  }
#endif /* RVO_STEP_POOL */

  Agent *const agent = new Agent();
  agent->position_ = position;
  agent->velocity_ = velocity;
  agent->id_ = agents_.size();
  agent->maxNeighbors_ = maxNeighbors;
  agent->maxSpeed_ = maxSpeed;
  agent->neighborDist_ = neighborDist;
  agent->radius_ = radius;
  agent->timeHorizon_ = timeHorizon;
  agent->timeHorizonObst_ = timeHorizonObst;
  agents_.push_back(agent);

  return agents_.size() - 1U;
}

std::size_t RVOSimulator::addObstacle(const std::vector<Vector2> &vertices) {
#if RVO_STEP_POOL
  if (pool_ != NULL) {
    pool_->quiesce();
  }
#endif /* RVO_STEP_POOL */

  if (vertices.size() > 1U) {
    const std::size_t obstacleNo = obstacles_.size();

    for (std::size_t i = 0U; i < vertices.size(); ++i) {
      Obstacle *const obstacle = new Obstacle();
      obstacle->point_ = vertices[i];

      if (i != 0U) {
        obstacle->previous_ = obstacles_.back();
        obstacle->previous_->next_ = obstacle;
      }

      if (i == vertices.size() - 1U) {
        obstacle->next_ = obstacles_[obstacleNo];
        obstacle->next_->previous_ = obstacle;
      }

      obstacle->direction_ = normalize(
          vertices[(i == vertices.size() - 1U ? 0U : i + 1U)] - vertices[i]);

      if (vertices.size() == 2U) {
        obstacle->isConvex_ = true;
      } else {
        obstacle->isConvex_ =
            leftOf(vertices[i == 0U ? vertices.size() - 1U : i - 1U],
                   vertices[i],
                   vertices[i == vertices.size() - 1U ? 0U : i + 1U]) >= 0.0F;
      }

      obstacle->id_ = obstacles_.size();

      obstacles_.push_back(obstacle);
    }

    return obstacleNo;
  }

  return RVO_ERROR;
}

void RVOSimulator::doStep() {
#if RVO_STEP_POOL
  const int threads = omp_get_max_threads();

  if (threads > 1) {
    preparePool(threads);
    kdTree_->buildAgentTree();
    pool_->run();
    globalTime_ += timeStep_;
    return;
  }
#endif /* RVO_STEP_POOL */

  kdTree_->buildAgentTree();

#ifdef _OPENMP
#pragma omp parallel for
#endif /* _OPENMP */
  for (std::size_t i = 0U; i < agents_.size(); ++i) {
    agents_[i]->computeNeighbors(kdTree_);
    agents_[i]->computeNewVelocity(timeStep_);
  }

#ifdef _OPENMP
#pragma omp parallel for
#endif /* _OPENMP */
  for (std::size_t i = 0U; i < agents_.size(); ++i) {
    agents_[i]->update(timeStep_);
  }

  globalTime_ += timeStep_;
}

#if RVO_STEP_POOL
void RVOSimulator::preparePool(int threads) {
  if (pool_ != NULL && pool_->threads() != threads) {
    delete pool_;
    pool_ = NULL;
  }

  if (pool_ == NULL) {
    pool_ = new StepPool(this, threads);
  }

  pool_->prepare();
}
#endif /* RVO_STEP_POOL */

void RVOSimulator::setAgentPrefVelocities(PrefVelocityFunction function,
                                          void *context) {
#if RVO_STEP_POOL
  const int threads = omp_get_max_threads();

  if (threads > 1) {
    preparePool(threads);
    pool_->setPrefVelocities(function, context);
    return;
  }
#endif /* RVO_STEP_POOL */

#ifdef _OPENMP
#pragma omp parallel for
#endif /* _OPENMP */
  for (std::size_t i = 0U; i < agents_.size(); ++i) {
    setPrefVelocityFrom(i, function, context);
  }
}

void RVOSimulator::computeAgentInto(std::size_t agentNo, Agent *out) const {
  const Agent *const agent = agents_[agentNo];

  out->position_ = agent->position_;
  out->velocity_ = agent->velocity_;
  out->prefVelocity_ = agent->prefVelocity_;
  out->id_ = agent->id_;
  out->maxNeighbors_ = agent->maxNeighbors_;
  out->maxSpeed_ = agent->maxSpeed_;
  out->neighborDist_ = agent->neighborDist_;
  out->radius_ = agent->radius_;
  out->timeHorizon_ = agent->timeHorizon_;
  out->timeHorizonObst_ = agent->timeHorizonObst_;

  /* Agent::computeNeighbors, with the agent search run for agent. */
  out->obstacleNeighbors_.clear();
  const float range = out->timeHorizonObst_ * out->maxSpeed_ + out->radius_;
  kdTree_->computeObstacleNeighbors(out, range * range);

  out->agentNeighbors_.clear();

  if (out->maxNeighbors_ > 0U) {
    float rangeSq = out->neighborDist_ * out->neighborDist_;
    kdTree_->computeAgentNeighborsInto(agent, out, rangeSq);
  }

  out->computeNewVelocity(timeStep_);
}

void RVOSimulator::publishAgent(std::size_t agentNo, const Agent *from) {
  Agent *const agent = agents_[agentNo];

  /* A copy, not a swap: swapping would rotate buffers between agents and
   * threads, and every step would write into another core's cache. */
  agent->agentNeighbors_ = from->agentNeighbors_;
  agent->obstacleNeighbors_ = from->obstacleNeighbors_;
  agent->orcaLines_ = from->orcaLines_;
  agent->newVelocity_ = from->newVelocity_;
}

void RVOSimulator::updateAgent(std::size_t agentNo) {
  agents_[agentNo]->update(timeStep_);
}

void RVOSimulator::setPrefVelocityFrom(std::size_t agentNo,
                                       PrefVelocityFunction function,
                                       void *context) {
  agents_[agentNo]->prefVelocity_ = function(this, agentNo, context);
}

Agent *RVOSimulator::newScratchAgent() { return new Agent(); }

void RVOSimulator::deleteScratchAgent(Agent *agent) { delete agent; }

std::size_t RVOSimulator::getAgentAgentNeighbor(std::size_t agentNo,
                                                std::size_t neighborNo) const {
  return agents_[agentNo]->agentNeighbors_[neighborNo].second->id_;
}

std::size_t RVOSimulator::getAgentMaxNeighbors(std::size_t agentNo) const {
  return agents_[agentNo]->maxNeighbors_;
}

float RVOSimulator::getAgentMaxSpeed(std::size_t agentNo) const {
  return agents_[agentNo]->maxSpeed_;
}

float RVOSimulator::getAgentNeighborDist(std::size_t agentNo) const {
  return agents_[agentNo]->neighborDist_;
}

std::size_t RVOSimulator::getAgentNumAgentNeighbors(std::size_t agentNo) const {
  return agents_[agentNo]->agentNeighbors_.size();
}

std::size_t RVOSimulator::getAgentNumObstacleNeighbors(
    std::size_t agentNo) const {
  return agents_[agentNo]->obstacleNeighbors_.size();
}

std::size_t RVOSimulator::getAgentNumORCALines(std::size_t agentNo) const {
  return agents_[agentNo]->orcaLines_.size();
}

std::size_t RVOSimulator::getAgentObstacleNeighbor(
    std::size_t agentNo, std::size_t neighborNo) const {
  return agents_[agentNo]->obstacleNeighbors_[neighborNo].second->id_;
}

const Line &RVOSimulator::getAgentORCALine(std::size_t agentNo,
                                           std::size_t lineNo) const {
  return agents_[agentNo]->orcaLines_[lineNo];
}

const Vector2 &RVOSimulator::getAgentPosition(std::size_t agentNo) const {
  return agents_[agentNo]->position_;
}

const Vector2 &RVOSimulator::getAgentPrefVelocity(std::size_t agentNo) const {
  return agents_[agentNo]->prefVelocity_;
}

float RVOSimulator::getAgentRadius(std::size_t agentNo) const {
  return agents_[agentNo]->radius_;
}

float RVOSimulator::getAgentTimeHorizon(std::size_t agentNo) const {
  return agents_[agentNo]->timeHorizon_;
}

float RVOSimulator::getAgentTimeHorizonObst(std::size_t agentNo) const {
  return agents_[agentNo]->timeHorizonObst_;
}

const Vector2 &RVOSimulator::getAgentVelocity(std::size_t agentNo) const {
  return agents_[agentNo]->velocity_;
}

const Vector2 &RVOSimulator::getObstacleVertex(std::size_t vertexNo) const {
  return obstacles_[vertexNo]->point_;
}

std::size_t RVOSimulator::getNextObstacleVertexNo(std::size_t vertexNo) const {
  return obstacles_[vertexNo]->next_->id_;
}

std::size_t RVOSimulator::getPrevObstacleVertexNo(std::size_t vertexNo) const {
  return obstacles_[vertexNo]->previous_->id_;
}

void RVOSimulator::processObstacles() {
#if RVO_STEP_POOL
  if (pool_ != NULL) {
    pool_->quiesce();
  }
#endif /* RVO_STEP_POOL */

  kdTree_->buildObstacleTree();
}

bool RVOSimulator::queryVisibility(const Vector2 &point1,
                                   const Vector2 &point2) const {
  return kdTree_->queryVisibility(point1, point2, 0.0F);
}

bool RVOSimulator::queryVisibility(const Vector2 &point1, const Vector2 &point2,
                                   float radius) const {
  return kdTree_->queryVisibility(point1, point2, radius);
}

void RVOSimulator::setAgentDefaults(float neighborDist,
                                    std::size_t maxNeighbors, float timeHorizon,
                                    float timeHorizonObst, float radius,
                                    float maxSpeed) {
  setAgentDefaults(neighborDist, maxNeighbors, timeHorizon, timeHorizonObst,
                   radius, maxSpeed, Vector2());
}

void RVOSimulator::setAgentDefaults(float neighborDist,
                                    std::size_t maxNeighbors, float timeHorizon,
                                    float timeHorizonObst, float radius,
                                    float maxSpeed, const Vector2 &velocity) {
  if (defaultAgent_ == NULL) {
    defaultAgent_ = new Agent();
  }

  defaultAgent_->maxNeighbors_ = maxNeighbors;
  defaultAgent_->maxSpeed_ = maxSpeed;
  defaultAgent_->neighborDist_ = neighborDist;
  defaultAgent_->radius_ = radius;
  defaultAgent_->timeHorizon_ = timeHorizon;
  defaultAgent_->timeHorizonObst_ = timeHorizonObst;
  defaultAgent_->velocity_ = velocity;
}

void RVOSimulator::setAgentMaxNeighbors(std::size_t agentNo,
                                        std::size_t maxNeighbors) {
  agents_[agentNo]->maxNeighbors_ = maxNeighbors;
}

void RVOSimulator::setAgentMaxSpeed(std::size_t agentNo, float maxSpeed) {
  agents_[agentNo]->maxSpeed_ = maxSpeed;
}

void RVOSimulator::setAgentNeighborDist(std::size_t agentNo,
                                        float neighborDist) {
  agents_[agentNo]->neighborDist_ = neighborDist;
}

void RVOSimulator::setAgentPosition(std::size_t agentNo,
                                    const Vector2 &position) {
  agents_[agentNo]->position_ = position;
}

void RVOSimulator::setAgentPrefVelocity(std::size_t agentNo,
                                        const Vector2 &prefVelocity) {
  agents_[agentNo]->prefVelocity_ = prefVelocity;
}

void RVOSimulator::setAgentRadius(std::size_t agentNo, float radius) {
  agents_[agentNo]->radius_ = radius;
}

void RVOSimulator::setAgentTimeHorizon(std::size_t agentNo, float timeHorizon) {
  agents_[agentNo]->timeHorizon_ = timeHorizon;
}

void RVOSimulator::setAgentTimeHorizonObst(std::size_t agentNo,
                                           float timeHorizonObst) {
  agents_[agentNo]->timeHorizonObst_ = timeHorizonObst;
}

void RVOSimulator::setAgentVelocity(std::size_t agentNo,
                                    const Vector2 &velocity) {
  agents_[agentNo]->velocity_ = velocity;
}
} /* namespace RVO */
