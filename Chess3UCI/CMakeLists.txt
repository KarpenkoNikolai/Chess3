cmake_minimum_required(VERSION 3.5)

project(Chess3UCI LANGUAGES CXX)

add_compile_definitions(IS_64BIT)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB APP_HEADERS "*.h")
file(GLOB APP_SRC "*.cpp")

add_executable(Chess3UCI
  ${APP_SRC} ${APP_HEADERS}
)

if(WIN32)
else()
	target_link_libraries(Chess3UCI pthread dl tbb)
endif()