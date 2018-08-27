# Copyright 2018, mike at myasnikov.mike@gmail.com
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required ( VERSION 2.8.12 )

project ( wilton_smartec_reader CXX )

# dependencies
set ( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} CACHE INTERNAL "" )
set ( CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_BINARY_DIR} CACHE INTERNAL "" )
set ( CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_BINARY_DIR} CACHE INTERNAL "" )

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11")

set(SOURCES
    src/wilton_smartec_reader.cpp)

 set(LIBS
    pthread
    X11
 )
if ( WIN32 )
	set(LIBS "")
endif ( )

add_library ( ${PROJECT_NAME} SHARED
        ${SOURCES} )

target_include_directories ( ${PROJECT_NAME} BEFORE PRIVATE
     $ENV{WILTON_HOME}/devel/include
)

if ( WIN32 )
    target_link_libraries ( ${PROJECT_NAME} PRIVATE
            $ENV{WILTON_HOME}/devel/wilton_core.lib )
endif ( )

target_link_libraries ( ${PROJECT_NAME} ${LIBS})

add_custom_target ( dist ALL
        COMMAND ${CMAKE_COMMAND} -E remove_directory dist
        COMMAND ${CMAKE_COMMAND} -E make_directory dist
        # copy wilton libs to app bundle
        COMMAND ${CMAKE_COMMAND} -E copy_directory $ENV{WILTON_HOME}/bin dist/bin
        # copy example module
        COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}${PROJECT_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}
            dist/bin/
        # copy JS stdlib
        COMMAND ${CMAKE_COMMAND} -E copy $ENV{WILTON_HOME}/std.wlib dist/
        # copy JS app
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_LIST_DIR}/index.js dist/index.js
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        DEPENDS ${PROJECT_NAME}
        COMMENT "Preparing application bundle, directory: [${CMAKE_CURRENT_BINARY_DIR}/dist] ..." )
