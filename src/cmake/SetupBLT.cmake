# Copyright (c) Lawrence Livermore National Security, LLC and other Devil Ray
# Project developers. See top-level LICENSE AND COPYRIGHT files for dates and
# other details.
# No copyright assignment is required to contribute to Devil Ray.

################################################################
# don't use BLT's all warnings feature
################################################################
set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "")

################################################################
# if BLT_SOURCE_DIR is not set - use "blt" as default
################################################################
if(NOT BLT_SOURCE_DIR)
    set(BLT_SOURCE_DIR "blt")
endif()

################################################################
# if not set, prefer c++11 lang standard
################################################################
if(NOT BLT_CXX_STD)
    set(BLT_CXX_STD "c++11" CACHE STRING "")
endif()

################################################################
# if not set, prefer folder grouped targets
################################################################
if(NOT ENABLE_FOLDERS)
    set(ENABLE_FOLDERS TRUE CACHE STRING "")
endif()

################################################################
# Make sure BLT exports its built-in targets
################################################################
# OFF OF HIP EXPERIMENT
#set(BLT_EXPORT_THIRDPARTY ON CACHE BOOL "")

################################################################
# init blt using BLT_SOURCE_DIR
################################################################
include(${BLT_SOURCE_DIR}/SetupBLT.cmake)

if(ENABLE_MPI)
    # on some platforms (mostly cray systems) folks skip mpi
    # detection in BLT by setting ENABLE_FIND_MPI = OFF
    # in these cases, we need to set MPI_FOUND = TRUE,
    # since the rest of our cmake logic to include MPI uses MPI_FOUND
    if(NOT ENABLE_FIND_MPI)
        set(MPI_FOUND ON CACHE BOOL "")
    endif()

    # adjust MPI from BLT
    if( ${CMAKE_VERSION} VERSION_LESS "3.15.0" )
        # older cmake, we use BLT's mpi support, it uses 
        # the name mpi
        set(dray_blt_mpi_deps mpi CACHE STRING "")
        set(DRAY_USE_CMAKE_MPI_TARGETS FALSE CACHE BOOL "")
    else()
        # if we are using BLT's enable mpi, then we must
        # make sure the MPI targets exist
        if(ENABLE_FIND_MPI)
            # our import logic needs this info if hdf5 depends
            # on mpi
            if(TARGET MPI::MPI_CXX)
                set(DRAY_USE_CMAKE_MPI_TARGETS TRUE CACHE BOOL "")
                message(STATUS "Using MPI CMake imported target: MPI::MPI_CXX")
                # newer cmake we use find mpi targets directly
                set(dray_blt_mpi_deps MPI::MPI_CXX CACHE STRING "")
            else()
                message(FATAL_ERROR "Cannot use CMake imported targets for MPI."
                                    "(CMake > 3.15, ENABLE_MPI == ON, but "
                                    "MPI::MPI_CXX CMake target is missing.)")
            endif()
        else()
            set(DRAY_USE_CMAKE_MPI_TARGETS FALSE CACHE BOOL "")
            # compiler will handle them implicitly
            set(dray_blt_mpi_deps "" CACHE STRING "")
        endif()
    endif()
endif()
################################################################
# apply folders to a few ungrouped blt targets
################################################################

###############################################
# group main blt docs targets into docs folder
###############################################
blt_set_target_folder( TARGET docs FOLDER docs)

if(TARGET sphinx_docs)
    blt_set_target_folder( TARGET sphinx_docs FOLDER docs)
endif()

if(TARGET doxygen_docs)
    blt_set_target_folder( TARGET doxygen_docs FOLDER docs)
endif()

####################################################
# group top level blt health checks into blt folder
####################################################
if(TARGET check)
    blt_set_target_folder( TARGET check FOLDER blt)
endif()

if(TARGET style)
    blt_set_target_folder( TARGET style FOLDER blt)
endif()
