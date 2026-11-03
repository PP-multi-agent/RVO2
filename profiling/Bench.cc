/*
 * Bench.cc
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

/*
 * @file  Bench.cc
 * @brief Runs a fixed number of simulation steps and prints the time.
 *        One step is the preferred-velocity update followed by doStep().
 *        The step count does not depend on agents reaching their goals,
 *        so a faster or slower algorithm stays comparable.
 *        With no arguments, or with "all", every workload below is timed.
 */

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <map>
#include <utility>
#include <vector>

#if _OPENMP
#include <omp.h>
#endif /* _OPENMP */

#include "RVO.h"

namespace {
const float kTwoPi = 6.28318530717958647692F;
const std::size_t kWarmupSteps = 5U;
const std::size_t kLoadCount = 4U;
const std::size_t kLoads[kLoadCount] = {100U, 400U, 1000U, 2000U};

int threadCount() {
#ifdef _OPENMP
  return omp_get_max_threads();
#else
  return 1;
#endif
}

struct GoalContext {
  const std::vector<RVO::Vector2> *goals;
  bool perturb;
};

RVO::Vector2 goalPrefVelocity(const RVO::RVOSimulator *simulator,
                              std::size_t i, void *context) {
  const GoalContext *const data = static_cast<GoalContext *>(context);
  RVO::Vector2 goalVector = (*data->goals)[i] - simulator->getAgentPosition(i);

  if (RVO::absSq(goalVector) > 1.0F) {
    goalVector = RVO::normalize(goalVector);
  }

  /* Deterministic nudge. Blocks uses noise to break symmetry; a fixed
   * angle keeps repeated benchmark runs comparable. */
  if (data->perturb) {
    const float angle = static_cast<float>(i) * 0.01F;
    const float dist = 0.0001F;
    return goalVector + dist * RVO::Vector2(std::cos(angle), std::sin(angle));
  }

  return goalVector;
}

void setPreferredVelocities(RVO::RVOSimulator *simulator,
                            const std::vector<RVO::Vector2> &goals,
                            bool perturb) {
  GoalContext context;
  context.goals = &goals;
  context.perturb = perturb;

  /* With more than one thread, this runs on the same workers as doStep(),
   * instead of on a second OpenMP team. */
  if (threadCount() > 1) {
    simulator->setAgentPrefVelocities(&goalPrefVelocity, &context);
    return;
  }

#ifdef _OPENMP
#pragma omp parallel for
#endif /* _OPENMP */
  for (std::size_t i = 0U; i < simulator->getNumAgents(); ++i) {
    simulator->setAgentPrefVelocity(i, goalPrefVelocity(simulator, i, &context));
  }
}

void setupCircle(
    RVO::RVOSimulator *simulator,
    std::vector<RVO::Vector2> &goals, /* NOLINT(runtime/references) */
    std::size_t agentCount) {
  simulator->setTimeStep(0.25F);
  simulator->setAgentDefaults(15.0F, 10U, 10.0F, 10.0F, 1.5F, 2.0F);

  for (std::size_t i = 0U; i < agentCount; ++i) {
    const float angle = static_cast<float>(i) * kTwoPi /
                        static_cast<float>(agentCount);
    simulator->addAgent(
        200.0F * RVO::Vector2(std::cos(angle), std::sin(angle)));
    goals.push_back(-simulator->getAgentPosition(i));
  }
}

void addDemoObstacles(RVO::RVOSimulator *simulator) {
  std::vector<RVO::Vector2> obstacle1;
  std::vector<RVO::Vector2> obstacle2;
  std::vector<RVO::Vector2> obstacle3;
  std::vector<RVO::Vector2> obstacle4;

  obstacle1.push_back(RVO::Vector2(-10.0F, 40.0F));
  obstacle1.push_back(RVO::Vector2(-40.0F, 40.0F));
  obstacle1.push_back(RVO::Vector2(-40.0F, 10.0F));
  obstacle1.push_back(RVO::Vector2(-10.0F, 10.0F));

  obstacle2.push_back(RVO::Vector2(10.0F, 40.0F));
  obstacle2.push_back(RVO::Vector2(10.0F, 10.0F));
  obstacle2.push_back(RVO::Vector2(40.0F, 10.0F));
  obstacle2.push_back(RVO::Vector2(40.0F, 40.0F));

  obstacle3.push_back(RVO::Vector2(10.0F, -40.0F));
  obstacle3.push_back(RVO::Vector2(40.0F, -40.0F));
  obstacle3.push_back(RVO::Vector2(40.0F, -10.0F));
  obstacle3.push_back(RVO::Vector2(10.0F, -10.0F));

  obstacle4.push_back(RVO::Vector2(-10.0F, -40.0F));
  obstacle4.push_back(RVO::Vector2(-10.0F, -10.0F));
  obstacle4.push_back(RVO::Vector2(-40.0F, -10.0F));
  obstacle4.push_back(RVO::Vector2(-40.0F, -40.0F));

  simulator->addObstacle(obstacle1);
  simulator->addObstacle(obstacle2);
  simulator->addObstacle(obstacle3);
  simulator->addObstacle(obstacle4);
  simulator->processObstacles();
}

std::size_t gridColumns(std::size_t count) {
  const float side = std::ceil(std::sqrt(static_cast<float>(count)));
  std::size_t columns = static_cast<std::size_t>(side);
  if (columns == 0U) {
    columns = 1U;
  }
  return columns;
}

void splitFourWays(std::size_t total, std::size_t count[4]) {
  const std::size_t base = total / 4U;
  const std::size_t extra = total % 4U;
  for (std::size_t i = 0U; i < 4U; ++i) {
    count[i] = base;
    if (i < extra) {
      ++count[i];
    }
  }
}

template <typename Goal>
void addGrid(
    RVO::RVOSimulator *simulator,
    std::vector<Goal> &goals, /* NOLINT(runtime/references) */
    std::size_t count, float originX, float originY, float stepX, float stepY,
    const Goal &goal) {
  if (count == 0U) {
    return;
  }
  const std::size_t columns = gridColumns(count);
  for (std::size_t n = 0U; n < count; ++n) {
    const std::size_t col = n % columns;
    const std::size_t row = n / columns;
    simulator->addAgent(RVO::Vector2(
        originX + stepX * static_cast<float>(col),
        originY + stepY * static_cast<float>(row)));
    goals.push_back(goal);
  }
}

template <typename Goal>
void addFourCorners(
    RVO::RVOSimulator *simulator,
    std::vector<Goal> &goals, /* NOLINT(runtime/references) */
    std::size_t agentCount, const Goal cornerGoal[4]) {
  std::size_t group[4];
  splitFourWays(agentCount, group);
  addGrid(simulator, goals, group[0], 55.0F, 55.0F, 10.0F, 10.0F,
          cornerGoal[0]);
  addGrid(simulator, goals, group[1], -55.0F, 55.0F, -10.0F, 10.0F,
          cornerGoal[1]);
  addGrid(simulator, goals, group[2], 55.0F, -55.0F, 10.0F, -10.0F,
          cornerGoal[2]);
  addGrid(simulator, goals, group[3], -55.0F, -55.0F, -10.0F, -10.0F,
          cornerGoal[3]);
}

void setupBlocks(
    RVO::RVOSimulator *simulator,
    std::vector<RVO::Vector2> &goals, /* NOLINT(runtime/references) */
    std::size_t agentCount) {
  simulator->setTimeStep(0.25F);
  simulator->setAgentDefaults(15.0F, 10U, 5.0F, 5.0F, 2.0F, 2.0F);

  const RVO::Vector2 cornerGoal[4] = {
      RVO::Vector2(-75.0F, -75.0F), RVO::Vector2(75.0F, -75.0F),
      RVO::Vector2(-75.0F, 75.0F), RVO::Vector2(75.0F, 75.0F)};
  addFourCorners(simulator, goals, agentCount, cornerGoal);
  addDemoObstacles(simulator);
}

class RoadmapVertex {
 public:
  RVO::Vector2 position;
  std::vector<std::size_t> neighbors;
  std::vector<float> distToGoal;
};

void addRoadmapVertex(std::vector<RoadmapVertex> &roadmap, /* NOLINT */
                      float x, float y) {
  RoadmapVertex vertex;
  vertex.position = RVO::Vector2(x, y);
  roadmap.push_back(vertex);
}

void setupRoadmap(
    RVO::RVOSimulator *simulator,
    std::vector<RoadmapVertex> &roadmap, /* NOLINT(runtime/references) */
    std::vector<std::size_t> &goals,     /* NOLINT(runtime/references) */
    std::size_t agentCount) {
  simulator->setTimeStep(0.25F);
  addDemoObstacles(simulator);

  /* Goals are the first four vertices. The rest ring the obstacles. */
  addRoadmapVertex(roadmap, -75.0F, -75.0F);
  addRoadmapVertex(roadmap, 75.0F, -75.0F);
  addRoadmapVertex(roadmap, -75.0F, 75.0F);
  addRoadmapVertex(roadmap, 75.0F, 75.0F);
  addRoadmapVertex(roadmap, -42.0F, -42.0F);
  addRoadmapVertex(roadmap, -42.0F, -8.0F);
  addRoadmapVertex(roadmap, -42.0F, 8.0F);
  addRoadmapVertex(roadmap, -42.0F, 42.0F);
  addRoadmapVertex(roadmap, -8.0F, -42.0F);
  addRoadmapVertex(roadmap, -8.0F, -8.0F);
  addRoadmapVertex(roadmap, -8.0F, 8.0F);
  addRoadmapVertex(roadmap, -8.0F, 42.0F);
  addRoadmapVertex(roadmap, 8.0F, -42.0F);
  addRoadmapVertex(roadmap, 8.0F, -8.0F);
  addRoadmapVertex(roadmap, 8.0F, 8.0F);
  addRoadmapVertex(roadmap, 8.0F, 42.0F);
  addRoadmapVertex(roadmap, 42.0F, -42.0F);
  addRoadmapVertex(roadmap, 42.0F, -8.0F);
  addRoadmapVertex(roadmap, 42.0F, 8.0F);
  addRoadmapVertex(roadmap, 42.0F, 42.0F);

  simulator->setAgentDefaults(15.0F, 10U, 5.0F, 5.0F, 2.0F, 2.0F);

  const std::size_t cornerGoal[4] = {0U, 1U, 2U, 3U};
  addFourCorners(simulator, goals, agentCount, cornerGoal);
}

void buildRoadmap(
    RVO::RVOSimulator *simulator,
    std::vector<RoadmapVertex> &roadmap) { /* NOLINT(runtime/references) */
  typedef std::multimap<float, std::size_t> DistQueue;

#ifdef _OPENMP
#pragma omp parallel for
#endif /* _OPENMP */
  for (std::size_t i = 0U; i < roadmap.size(); ++i) {
    for (std::size_t j = 0U; j < roadmap.size(); ++j) {
      if (simulator->queryVisibility(roadmap[i].position, roadmap[j].position,
                                     simulator->getAgentRadius(0U))) {
        roadmap[i].neighbors.push_back(j);
      }
    }

    roadmap[i].distToGoal.resize(4U, std::numeric_limits<float>::infinity());
  }

#ifdef _OPENMP
#pragma omp parallel for
#endif /* _OPENMP */
  for (std::size_t goal = 0U; goal < 4U; ++goal) {
    DistQueue queue;
    std::vector<DistQueue::iterator> posInQueue(roadmap.size(), queue.end());

    roadmap[goal].distToGoal[goal] = 0.0F;
    posInQueue[goal] = queue.insert(std::make_pair(0.0F, goal));

    while (!queue.empty()) {
      const std::size_t from = queue.begin()->second;
      queue.erase(queue.begin());
      posInQueue[from] = queue.end();

      for (std::size_t n = 0U; n < roadmap[from].neighbors.size(); ++n) {
        const std::size_t to = roadmap[from].neighbors[n];
        const float dist =
            RVO::abs(roadmap[to].position - roadmap[from].position);
        const float through = roadmap[from].distToGoal[goal] + dist;

        if (roadmap[to].distToGoal[goal] > through) {
          roadmap[to].distToGoal[goal] = through;

          if (posInQueue[to] == queue.end()) {
            posInQueue[to] = queue.insert(std::make_pair(through, to));
          } else {
            queue.erase(posInQueue[to]);
            posInQueue[to] = queue.insert(std::make_pair(through, to));
          }
        }
      }
    }
  }
}

struct RoadmapContext {
  const std::vector<RoadmapVertex> *roadmap;
  const std::vector<std::size_t> *goals;
};

RVO::Vector2 roadmapPrefVelocity(const RVO::RVOSimulator *simulator,
                                 std::size_t i, void *context) {
  const RoadmapContext *const data = static_cast<RoadmapContext *>(context);
  const std::vector<RoadmapVertex> &roadmap = *data->roadmap;
  const std::vector<std::size_t> &goals = *data->goals;
  float minDist = std::numeric_limits<float>::infinity();
  std::size_t minVertex = std::numeric_limits<std::size_t>::max();

  for (std::size_t j = 0U; j < roadmap.size(); ++j) {
    const float dist =
        RVO::abs(roadmap[j].position - simulator->getAgentPosition(i)) +
        roadmap[j].distToGoal[goals[i]];
    const bool visible = simulator->queryVisibility(
        simulator->getAgentPosition(i), roadmap[j].position,
        simulator->getAgentRadius(i));

    if (dist < minDist && visible) {
      minDist = dist;
      minVertex = j;
    }
  }

  const std::size_t missing = std::numeric_limits<std::size_t>::max();
  if (minVertex == missing) {
    return RVO::Vector2(0.0F, 0.0F);
  }

  if (RVO::absSq(roadmap[minVertex].position -
                 simulator->getAgentPosition(i)) == 0.0F) {
    if (minVertex == goals[i]) {
      return RVO::Vector2();
    }

    return RVO::normalize(roadmap[goals[i]].position -
                          simulator->getAgentPosition(i));
  }

  return RVO::normalize(roadmap[minVertex].position -
                        simulator->getAgentPosition(i));
}

void setRoadmapVelocities(RVO::RVOSimulator *simulator,
                          const std::vector<RoadmapVertex> &roadmap,
                          const std::vector<std::size_t> &goals) {
  RoadmapContext context;
  context.roadmap = &roadmap;
  context.goals = &goals;

  /* With more than one thread, the visibility search runs on the same
   * workers as doStep(), instead of on a second OpenMP team. */
  if (threadCount() > 1) {
    simulator->setAgentPrefVelocities(&roadmapPrefVelocity, &context);
  } else {
#ifdef _OPENMP
#pragma omp parallel for
#endif /* _OPENMP */
    for (std::size_t i = 0U; i < simulator->getNumAgents(); ++i) {
      simulator->setAgentPrefVelocity(
          i, roadmapPrefVelocity(simulator, i, &context));
    }
  }

  for (std::size_t i = 0U; i < simulator->getNumAgents(); ++i) {
    const float angle = static_cast<float>(i) * 0.01F;
    const float dist = 0.0001F;
    simulator->setAgentPrefVelocity(
        i, simulator->getAgentPrefVelocity(i) +
               dist * RVO::Vector2(std::cos(angle), std::sin(angle)));
  }
}

void runSteps(RVO::RVOSimulator *simulator,
              const std::vector<RVO::Vector2> &goals, std::size_t count,
              bool perturb) {
  for (std::size_t step = 0U; step < count; ++step) {
    setPreferredVelocities(simulator, goals, perturb);
    simulator->doStep();
  }
}

bool parsePositive(const char *text, std::size_t *value) {
  char *end = NULL;
  const unsigned long parsed = std::strtoul(text, &end, 10);

  if (text[0] == '\0' || *end != '\0' || parsed == 0UL) {
    return false;
  }

  *value = static_cast<std::size_t>(parsed);
  return true;
}

void printUsage() {
  std::cerr
      << "Usage: rvo-bench [all|circle|blocks|roadmap|list] [steps] [agents]\n"
      << "  all      100, 400, 1000 and 2000 agents, each scenario\n"
      << "  circle   ring of agents, no obstacles; default runs all loads\n"
      << "  blocks   four corners and four obstacles; default runs all loads\n"
      << "  roadmap  blocks plus a visibility graph; default runs all loads\n"
      << "  list     print the default scenario/agent pairs\n"
      << "  steps    timed steps after 5 warmup steps (default 200)\n"
      << "  agents   optional; one load instead of 100 400 1000 2000\n";
}

void printLoads(const char *scenario) {
  for (std::size_t i = 0U; i < kLoadCount; ++i) {
    std::cout << scenario << " " << kLoads[i] << std::endl;
  }
}

double millisecondsSince(std::chrono::steady_clock::time_point start) {
  const std::chrono::steady_clock::time_point end =
      std::chrono::steady_clock::now();
  return std::chrono::duration<double, std::milli>(end - start).count();
}

void printResult(const char *scenario, std::size_t agents, std::size_t steps,
                 double elapsedMs) {
  const double perStepUs = elapsedMs * 1000.0 / static_cast<double>(steps);

  std::cout.setf(std::ios::fixed);
  std::cout.precision(3);
  std::cout << "scenario=" << scenario << " agents=" << agents
            << " steps=" << steps << " warmup=" << kWarmupSteps
            << " threads=" << threadCount() << " elapsed_ms=" << elapsedMs
            << " per_step_us=" << perStepUs << std::endl;
}

void timeGoalSteps(RVO::RVOSimulator *simulator,
                   const std::vector<RVO::Vector2> &goals, bool perturb,
                   const char *scenario, std::size_t agents,
                   std::size_t steps) {
  runSteps(simulator, goals, kWarmupSteps, perturb);

  const std::chrono::steady_clock::time_point start =
      std::chrono::steady_clock::now();
  runSteps(simulator, goals, steps, perturb);
  printResult(scenario, agents, steps, millisecondsSince(start));
}

void runCircle(std::size_t steps, std::size_t agents) {
  RVO::RVOSimulator simulator;
  std::vector<RVO::Vector2> goals;
  setupCircle(&simulator, goals, agents);
  timeGoalSteps(&simulator, goals, false, "circle", agents, steps);
}

void runBlocks(std::size_t steps, std::size_t agents) {
  RVO::RVOSimulator simulator;
  std::vector<RVO::Vector2> goals;
  setupBlocks(&simulator, goals, agents);
  timeGoalSteps(&simulator, goals, true, "blocks", agents, steps);
}

void runRoadmapSteps(RVO::RVOSimulator *simulator,
                     const std::vector<RoadmapVertex> &roadmap,
                     const std::vector<std::size_t> &goals,
                     std::size_t count) {
  for (std::size_t step = 0U; step < count; ++step) {
    setRoadmapVelocities(simulator, roadmap, goals);
    simulator->doStep();
  }
}

void runRoadmap(std::size_t steps, std::size_t agents) {
  RVO::RVOSimulator simulator;
  std::vector<RoadmapVertex> roadmap;
  std::vector<std::size_t> goals;
  setupRoadmap(&simulator, roadmap, goals, agents);
  buildRoadmap(&simulator, roadmap);
  runRoadmapSteps(&simulator, roadmap, goals, kWarmupSteps);

  const std::chrono::steady_clock::time_point start =
      std::chrono::steady_clock::now();
  runRoadmapSteps(&simulator, roadmap, goals, steps);
  printResult("roadmap", agents, steps, millisecondsSince(start));
}

void runOne(const char *scenario, std::size_t steps, std::size_t agents) {
  if (std::strcmp(scenario, "circle") == 0) {
    runCircle(steps, agents);
  } else if (std::strcmp(scenario, "blocks") == 0) {
    runBlocks(steps, agents);
  } else {
    runRoadmap(steps, agents);
  }
}

void runLoads(const char *scenario, std::size_t steps) {
  for (std::size_t i = 0U; i < kLoadCount; ++i) {
    runOne(scenario, steps, kLoads[i]);
  }
}

void runSuite(std::size_t steps) {
  runLoads("circle", steps);
  runLoads("blocks", steps);
  runLoads("roadmap", steps);
}
} /* namespace */

int main(int argc, char *argv[]) {
  const char *scenario = "all";
  std::size_t steps = 200U;
  std::size_t agents = 0U;

  if (argc > 4) {
    printUsage();
    return 1;
  }
  if (argc > 1) {
    scenario = argv[1];
  }
  if (std::strcmp(scenario, "list") == 0) {
    printLoads("circle");
    printLoads("blocks");
    printLoads("roadmap");
    return 0;
  }
  if (argc > 2 && !parsePositive(argv[2], &steps)) {
    printUsage();
    return 1;
  }
  if (argc > 3 && !parsePositive(argv[3], &agents)) {
    printUsage();
    return 1;
  }

  const bool all = std::strcmp(scenario, "all") == 0;
  const bool circle = std::strcmp(scenario, "circle") == 0;
  const bool blocks = std::strcmp(scenario, "blocks") == 0;
  const bool roadmap = std::strcmp(scenario, "roadmap") == 0;

  if (!all && !circle && !blocks && !roadmap) {
    printUsage();
    return 1;
  }
  if (all && argc > 3) {
    std::cerr << "all uses 100, 400, 1000 and 2000 agents; omit agents\n";
    printUsage();
    return 1;
  }

  if (all) {
    runSuite(steps);
  } else if (argc > 3) {
    runOne(scenario, steps, agents);
  } else if (circle) {
    runLoads("circle", steps);
  } else if (blocks) {
    runLoads("blocks", steps);
  } else {
    runLoads("roadmap", steps);
  }
  return 0;
}
