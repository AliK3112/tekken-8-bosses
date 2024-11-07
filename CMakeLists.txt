cmake_minimum_required(VERSION 3.10)
project(DLLInjectionProject)

# Set output directory for DLLs
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Add DLL sources
add_library(DLLInjection SHARED src/DllMain.cpp)

# Add the injector executable
add_executable(Injector src/Injector.cpp)

# Link dependencies if needed (e.g., Psapi)
target_link_libraries(DLLInjection PRIVATE Psapi)
target_link_libraries(Injector PRIVATE Psapi)

# Correct target name for DLLInjection
set_target_properties(DLLInjection PROPERTIES OUTPUT_NAME "bosses")
set_target_properties(Injector PROPERTIES OUTPUT_NAME "bosses")
