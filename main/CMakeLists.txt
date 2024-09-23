cmake_minimum_required(VERSION 3.5)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

idf_component_register(SRCS "rotary_example.cpp" "DFRobot_LCD.cpp" 
                    INCLUDE_DIRS "."
                    REQUIRES driver)
