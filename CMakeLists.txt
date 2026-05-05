cmake_minimum_required(VERSION 3.20)
project(figgie LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(figgie_core STATIC
  src/deck.cpp
  src/game.cpp
  src/order_book.cpp
  src/random_bot.cpp
)
target_include_directories(figgie_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(figgie_demo
  src/main.cpp
)
target_link_libraries(figgie_demo PRIVATE figgie_core)
