
add_library(status STATIC print_status.c)
target_include_directories(status PUBLIC .)
target_link_libraries(status moves planner ioqueue output)
