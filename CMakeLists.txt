cmake_minimum_required (VERSION 3.0.0)
project (PacDepLinearizer)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules")

#Cargo culted
SET(Boost_USE_MULTITHREAD ON)
FIND_PACKAGE(Boost REQUIRED)
FIND_PACKAGE(Alpm REQUIRED)

FIND_PATH(TCLAP_INCLUDEDIR tclap/CmdLine.h
  /usr/local/include
  /usr/include
)

IF(NOT TCLAP_INCLUDEDIR )
  message( FATAL_ERROR "TCLAP library is missing" )
ENDIF()

INCLUDE_DIRECTORIES(
  ${INCLUDE_DIRECTORIES}
  ${Boost_INCLUDE_DIRS}
  ${ALPM_INCLUDE_DIR}
  ${TCLAP_INCLUDEDIR}
)

LINK_DIRECTORIES(
  ${LINK_DIRECTORIES}
  ${Boost_LIBRARY_DIRS}
)

# use target_compile_features once available
# target_compile_features(pacdeplinearizer PUBLIC cxx_constexpr)
add_definitions(-std=c++11)

add_executable (pacdeplinearizer depend.cpp)
target_link_libraries( pacdeplinearizer ${ALPM_LIBS})

install(TARGETS pacdeplinearizer DESTINATION bin)
