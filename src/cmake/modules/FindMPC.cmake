# - Find MPC library
# Find the native MPC includes and library
# This module defines
#  MPC_INCLUDE_DIRS, where to find mpc.h, Set when
#                        MPC_INCLUDE_DIR is found.
#  MPC_LIBRARIES, libraries to link against to use MPC.
#  MPC_ROOT_DIR, The base directory to search for MPC.
#                    This can also be an environment variable.
#  MPC_FOUND, If false, do not try to use MPC.
#
# also defined, but not for general use are
#  MPC_LIBRARY, where to find the MPC library.

#=============================================================================
# Copyright 2011 Blender Foundation.
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================

# If MPC_ROOT_DIR was defined in the environment, use it.
IF(NOT MPC_ROOT_DIR AND NOT $ENV{MPC_ROOT_DIR} STREQUAL "")
  SET(MPC_ROOT_DIR $ENV{MPC_ROOT_DIR})
ENDIF()

SET(_mpc_SEARCH_DIRS
  ${MPC_ROOT_DIR}
  /usr/local
  /usr/local/MPC
  /usr/local/mpc
  /sw # Fink
  /opt/local # DarwinPorts
  /opt/csw # Blastwave
)

FIND_PATH(MPC_INCLUDE_DIR
  NAMES
    mpc.h
  HINTS
    ${_mpc_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

FIND_LIBRARY(MPC_LIBRARY
  NAMES
    mpc
  HINTS
    ${_mpc_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )
message( STATUS "MPC_LIBRARY = ${MPC_LIBRARY}" )

if( MPC_LIBRARY AND MPC_INCLUDE_DIR)
  SET( MPC_FOUND TRUE )
  message( STATUS "MPC Found: MPC_INCLUDE_DIR = ${MPC_INCLUDE_DIR}" )
  message( STATUS "MPC Found: MPC_LIBRARY = ${MPC_LIBRARY}" )
endif( MPC_LIBRARY AND MPC_INCLUDE_DIR)


MARK_AS_ADVANCED(
  MPC_INCLUDE_DIR
  MPC_LIBRARY
)
