cmake_minimum_required(VERSION 3.13)

project(ultimate_mechanics)

find_package(geode REQUIRED)

add_geode_mod(
    ID mmora143.ultimate_mechanics
    SOURCES main.cpp
)
