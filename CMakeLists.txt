cmake_minimum_required(VERSION 3.5)

project(Chess3)

option(BMI2_SUPPORT "BMI2" ON)

if (BMI2_SUPPORT)
	add_compile_definitions(BMI2_SUPPORT)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mbmi")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mbmi2")
endif()

add_subdirectory(GigantuaTest)
add_subdirectory(Chess3UCI)
