cmake_minimum_required(VERSION 3.25)

project(ultimate-mechanics)

find_package(geode REQUIRED)

add_geode_mod(
    ultimate-mechanics
    VERSION v1.0.0
    
    SOURCES
        src/main.cpp
)
