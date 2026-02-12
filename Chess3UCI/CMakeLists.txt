cmake_minimum_required(VERSION 3.5)

project(Chess3UCI LANGUAGES CXX)

add_compile_definitions(IS_64BIT)
add_compile_options(-mavx2 -mfma)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB APP_HEADERS "*.h")
file(GLOB APP_SRC "*.cpp")

add_executable(Chess3UCI
  ${APP_SRC} ${APP_HEADERS}
)

# Enable AVX2 for MSVC, GNU and Clang
if(MSVC)
  # MSVC: /arch:AVX2 enables AVX2 code generation
  target_compile_options(Chess3UCI PRIVATE /arch:AVX2)
else()
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(Chess3UCI PRIVATE -mavx2)
  endif()
endif()

if(WIN32)
else()
	target_link_libraries(Chess3UCI pthread)
endif()