cmake_minimum_required(VERSION 3.20)
project(os_simulator VERSION 1.0.0 LANGUAGES CXX)

# ------------------------------------------------------------------------------
# Hard Technical Constraint: C++20 is mandatory for std::counting_semaphore
# ------------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ------------------------------------------------------------------------------
# Dependency Management via FetchContent
# Do not rely on system-installed libraries to guarantee reproducible builds.
# ------------------------------------------------------------------------------
include(FetchContent)

# Fetch FTXUI (Pinned to v5.0.0)
FetchContent_Declare(ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/ftxui
  GIT_TAG v5.0.0
)

# Fetch yaml-cpp (Pinned to 0.8.0)
FetchContent_Declare(yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp
  GIT_TAG 0.8.0
)

# Make dependencies available to the build
FetchContent_MakeAvailable(ftxui yaml-cpp)

# Ensure Threading support is available (Required for std::thread)
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

# ------------------------------------------------------------------------------
# Executable Definition & Source Files
# Strictly mapping to the defined directory structure in Rule A.
# ------------------------------------------------------------------------------
add_executable(os_simulator
    main.cpp
    
    # Core Subsystem
    src/core/process.cpp
    src/core/cpu_scheduler.cpp
    src/core/concurrency_manager.cpp
    
    # Memory Subsystem
    src/memory/mmu.cpp
    src/memory/fifo_strategy.cpp
    src/memory/lru_strategy.cpp
    src/memory/clock_strategy.cpp
    src/memory/opt_strategy.cpp
    
    # UI Frontend
    src/ui/dashboard.cpp
    src/ui/memory_view.cpp
    src/ui/lock_monitor.cpp
    src/ui/telemetry_view.cpp
    
    # Utils
    src/utils/config_parser.cpp
    src/utils/telemetry.cpp
)

# Include directories mapping
target_include_directories(os_simulator PRIVATE include)

# ------------------------------------------------------------------------------
# Linking Phase
# ------------------------------------------------------------------------------
target_link_libraries(os_simulator PRIVATE
    ftxui::component 
    ftxui::dom 
    ftxui::screen 
    yaml-cpp
    Threads::Threads
)