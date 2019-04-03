cmake_minimum_required(VERSION 3.12.4)

cmake_policy(SET CMP0048 NEW)

project(GHEX VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)

include_directories(${CMAKE_SOURCE_DIR}/include)

find_package(MPI REQUIRED)

find_package(GridTools 0.20 REQUIRED)

message(">>>>>>>>>>>>>>>>${GridTools_MODULE_PATH}")

list(APPEND CMAKE_MODULE_PATH "${GridTools_MODULE_PATH}")

add_executable( test_iteration_spaces tests/test_iteration_spaces.cpp )
target_link_libraries(test_iteration_spaces ${exe_LIBS} GridTools::gridtools GridTools::gcl)


add_executable( regular_grid_descriptor tests/regular_grid_descriptor.cpp )
target_link_libraries( regular_grid_descriptor ${exe_LIBS} GridTools::gridtools )

add_executable( range_loops tests/range_loops.cpp )
target_link_libraries( range_loops ${exe_LIBS} GridTools::gridtools )


add_executable( generic_interfaces tests/generic_interfaces.cpp )
target_link_libraries(generic_interfaces ${exe_LIBS}  ${MPI_CXX_LIBRARIES} GridTools::gridtools GridTools::gcl)

add_executable( generic_interfaces8 tests/generic_interfaces8.cpp )
target_link_libraries(generic_interfaces8 ${exe_LIBS}  ${MPI_CXX_LIBRARIES} GridTools::gridtools GridTools::gcl)

add_executable( generic_interfaces1D tests/generic_interfaces1D.cpp )
target_link_libraries(generic_interfaces1D ${exe_LIBS}  ${MPI_CXX_LIBRARIES} GridTools::gridtools GridTools::gcl)

add_executable( generic_interfaces1D_mt tests/generic_interfaces1D_mt.cpp )
target_link_libraries(generic_interfaces1D_mt ${exe_LIBS}  ${MPI_CXX_LIBRARIES} GridTools::gridtools GridTools::gcl)
