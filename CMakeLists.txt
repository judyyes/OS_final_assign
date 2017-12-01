 cmake_minimum_required(VERSION 3.5)
project(OS_final_assign)

set(CMAKE_C_STANDARD 99)

set(SOURCE_FILES
        bitmap.c
        bitmap.h
        disk_emu.c
        disk_emu.h
        sfs_api.c
        sfs_api.h
        Makefile
#        sfs_mytest.c
#        sfs_mytest2.c
        sfs_test.c
        )

add_executable(OS_final_assign ${SOURCE_FILES})