cmake_minimum_required(VERSION 3.10)
project(ip_monitor)

# Set compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -g")
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_subdirectory(thirdparty/ur-threadder-api)
add_subdirectory(thirdparty/ur-rpc-template)

# Set directories
set(SRC_DIR src)
set(INC_DIR include)

# Find source files
file(GLOB SOURCES "${SRC_DIR}/*.c")

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE ${INC_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE thirdparty/ur-threadded-api/include)
target_include_directories(${PROJECT_NAME} PRIVATE thirdparty/ur-rpc-template)


# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE ur-rpc-template ur-threadmanager Threads::Threads)

# Install target (optional)
install(TARGETS ${PROJECT_NAME} DESTINATION bin)

# Find cJSON (if needed)
# find_package(cJSON REQUIRED)
# target_link_libraries(${PROJECT_NAME} PRIVATE cJSON::cJSON)
