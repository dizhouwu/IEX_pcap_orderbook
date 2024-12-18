cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
project(iex_pcap)
set(CMAKE_CXX_STANDARD 17)

############################################################
### PcapPlusPlus

# Set paths for the installed PcapPlusPlus library and headers
find_path(PCAPPLUSPLUS_INCLUDE_DIR NAMES "PcapPlusPlusVersion.h" PATHS "/usr/local/include/pcapplusplus" REQUIRED)
find_library(PCAPPLUSPLUS_COMMON_LIB NAMES "Common++" PATHS "/usr/local/lib" REQUIRED)
find_library(PCAPPLUSPLUS_PACKET_LIB NAMES "Packet++" PATHS "/usr/local/lib" REQUIRED)
find_library(PCAPPLUSPLUS_PCAP_LIB NAMES "Pcap++" PATHS "/usr/local/lib" REQUIRED)

include_directories(${PCAPPLUSPLUS_INCLUDE_DIR})
link_directories("/usr/local/lib")

############################################################
### Gtest

include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)
ExternalProject_Add(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG "main"
    UPDATE_COMMAND ""
    PATCH_COMMAND ""

    CMAKE_ARGS -Dgtest_force_shared_crt=ON
               -DBUILD_GTEST=ON
    PREFIX "${CMAKE_BINARY_DIR}/gtest"
    INSTALL_COMMAND ""
)

ExternalProject_Get_Property(googletest source_dir)
set(GTEST_INCLUDE_DIRS ${source_dir}/googletest/include)

ExternalProject_Get_Property(googletest binary_dir)
set(GTEST_LIBS_DIR ${binary_dir}/lib)

include_directories(${GTEST_INCLUDE_DIRS})
link_directories(${GTEST_LIBS_DIR})

############################################################
### IEX library
include_directories("include")

SET(EXT_LIBRARIES ${PCAPPLUSPLUS_PACKET_LIB}
                  ${PCAPPLUSPLUS_PCAP_LIB}
                  ${PCAPPLUSPLUS_COMMON_LIB}
                  pcap)

# Build iex_pcap library
add_library(iex_pcap "src/iex_decoder.cpp" "src/iex_messages.cpp" "src/orderbook.cpp" "src/l3book.cpp")
install(TARGETS iex_pcap DESTINATION "${CMAKE_SOURCE_DIR}/lib")
add_dependencies(iex_pcap googletest)

# Build example executable
add_executable(csv_example "src/csv_example.cpp")
target_link_libraries(csv_example iex_pcap ${EXT_LIBRARIES})
install(TARGETS csv_example DESTINATION "${CMAKE_SOURCE_DIR}/bin")

add_executable(deep_plus_example "src/deep_plus_example.cpp")
target_link_libraries(deep_plus_example iex_pcap ${EXT_LIBRARIES})
install(TARGETS deep_plus_example DESTINATION "${CMAKE_SOURCE_DIR}/bin")


# Find Google Test
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

# Set test directory and gather test sources
set(TEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/test")
file(GLOB TEST_SOURCES "${TEST_DIR}/*.cpp")

# Unit tests executable
add_executable(test_iex ${TEST_SOURCES} "test/test_main.cpp")
target_link_libraries(test_iex gtest gmock iex_pcap pthread ${EXT_LIBRARIES})

# Optional: set installation prefix for easier local testing
set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/install")
