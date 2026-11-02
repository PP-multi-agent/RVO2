# -*- mode: cmake; -*-
# vi: set ft=cmake:

#
# profiling/CMakeLists.txt
# RVO2 Library
#
# SPDX-FileCopyrightText: 2008 University of North Carolina at Chapel Hill
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Please send all bug reports to <geom@cs.unc.edu>.
#
# The authors may be contacted via:
#
# Jur van den Berg, Stephen J. Guy, Jamie Snape, Ming C. Lin, Dinesh Manocha
# Dept. of Computer Science
# 201 S. Columbia St.
# Frederick P. Brooks, Jr. Computer Science Bldg.
# Chapel Hill, N.C. 27599-3175
# United States of America
#
# <https://gamma.cs.unc.edu/RVO2/>
#

# The library stays C++98. The benchmark uses std::chrono, so this target
# is C++11. It is built on Linux, where the profiling scripts run.
if(UNIX)
  add_executable(rvo-bench Bench.cc)
  set_target_properties(rvo-bench PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF)
  target_link_libraries(rvo-bench PRIVATE ${RVO_LIBRARY})
  if(ENABLE_OPENMP AND OpenMP_FOUND)
    target_link_libraries(rvo-bench PRIVATE OpenMP::OpenMP_CXX)
  endif()
  if(ENABLE_INTERPROCEDURAL_OPTIMIZATION
      AND RVO_INTERPROCEDURAL_OPTIMIZATION_SUPPORTED)
    set_target_properties(rvo-bench PROPERTIES
      INTERPROCEDURAL_OPTIMIZATION ON)
  endif()
  if(CMAKE_VERSION VERSION_GREATER_EQUAL 4.1)
    set_target_properties(rvo-bench PROPERTIES
      SPDX_LICENSE Apache-2.0)
  endif()
endif()
