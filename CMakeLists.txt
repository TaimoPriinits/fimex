# Fimex, CMakeLists.txt
#
# Copyright (C) 2018-2022 met.no
#
# Contact information:
# Norwegian Meteorological Institute
# Box 43 Blindern
# 0313 OSLO
# NORWAY
# email: diana@met.no
#
# Project Info:  https://wiki.met.no/fimex/start
#
# This library is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation; either version 2.1 of the License, or
# (at your option) any later version.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
# License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
# USA.

CMAKE_MINIMUM_REQUIRED(VERSION 3.10)
PROJECT(fimex)

SET(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
INCLUDE(FimexUtils)

FIMEX_CMAKE_SETUP()
FIMEX_VERSION_DEFINES(FIMEX "include/fimex/CDMconstants.h")

FIMEX_FIND_PACKAGE(libxml2
  VERSION_MIN "2.6.0"
  CMAKE_NAME "libxml2"
  PKGCONFIG_NAME "libxml-2.0"
  LIBRARY_NAME "xml2"
  INCLUDE_HDR "libxml/xmlversion.h"
)

FIMEX_FIND_PACKAGE(udunits2
  VERSION_MIN "2"
  PKGCONFIG_NAME "udunits"
  LIBRARY_NAME "udunits2"
  INCLUDE_HDR "udunits2.h"
)

FIMEX_FIND_PACKAGE(proj
  CMAKE_NAME "PROJ"
  PKGCONFIG_NAME "proj"
  LIBRARY_NAME "proj"
  INCLUDE_HDR "proj.h" "proj_api.h"
)
IF (proj_PACKAGE_VERSION STREQUAL "")
  # unknown version
  FIND_PATH(proj_h_inc_dir
    NAMES "proj.h"
    HINTS "${proj_INC_DIR}" "${proj_DIR}/include"
  )
  IF (proj_h_inc_dir)
    # proj.h was found
    SET(HAVE_PROJ_H 1)
  ENDIF()
ELSEIF (proj_PACKAGE_VERSION VERSION_LESS "4.6.0")
  # <4.6
  MESSAGE(SEND_ERROR "proj version '${proj_PACKAGE_VERSION}' is too old")
ELSEIF (${proj_PACKAGE_VERSION} VERSION_LESS "5.0.0")
  # >=4.6,<5.0
  # proj_api.h
ELSEIF (${proj_PACKAGE_VERSION} VERSION_LESS "6.2.0")
  # >=5.0,<6.2
  MESSAGE(SEND_ERROR "proj version '${proj_PACKAGE_VERSION}' not supported")
ELSEIF (${proj_PACKAGE_VERSION} VERSION_LESS "8.0.0")
  # >=6.2,<8.0
  # ok, proj_api.h
ELSE ()
  # >=8.0
  SET(HAVE_PROJ_H 1)
ENDIF()

OPTION(ENABLE_PYTHON "Use python" ON)

OPTION(ENABLE_MPI "Use MPI" OFF)
IF(ENABLE_MPI)
  FIND_PACKAGE(MPI)
ENDIF()

OPTION(ENABLE_FIMEX_OMP "Use OpenMP" OFF)
IF(ENABLE_FIMEX_OMP)
  FIND_PACKAGE(OpenMP REQUIRED)
  SET(openmp_CXX_PACKAGE OpenMP::OpenMP_CXX)
  SET(openmp_F90_PACKAGE OpenMP::OpenMP_Fortran)
ENDIF()

OPTION(ENABLE_LOG4CPP "Use Log4Cpp" OFF)
IF(ENABLE_LOG4CPP)
  FIMEX_FIND_PACKAGE(log4cpp
    PKGCONFIG_NAME "log4cpp"
    LIBRARY_NAME "liblog4cpp"
    INCLUDE_HDR "log4cpp/Priority.hh"
  )
ENDIF()

OPTION(ENABLE_NETCDF "Use NetCDF" ON)
IF(ENABLE_NETCDF)
  FIMEX_FIND_PACKAGE(netCDF
    CMAKE_NAME "netCDF"
    CMAKE_TARGETS
      "netCDF::netcdf" # focal, debian sid, ...
      "netcdf"         # bionic
    PKGCONFIG_NAME "netcdf"
    LIBRARY_NAME "netcdf"
    INCLUDE_HDR "netcdf.h"
  )
  CHECK_NETCDF_HAS_HDF5(HAVE_NETCDF_HDF5_LIB)
ENDIF()

OPTION(ENABLE_FELT "Use Felt library" ON)

OPTION(ENABLE_ECCODES "Use ecCodes library" OFF)
OPTION(ENABLE_GRIBAPI "Use GribAPI library" OFF)

IF((ENABLE_GRIBAPI) AND (ENABLE_ECCODES))
  MESSAGE(FATAL_ERROR "Cannot use both ecCodes libraries")
ELSEIF(ENABLE_ECCODES)
  FIND_PACKAGE(eccodes REQUIRED)
  SET(eccodes_PACKAGE "eccodes")
  MESSAGE(STATUS "Detected ecCodes version ${eccodes_VERSION}")
  # cmake variable "eccodes_FEATURES" was called "ECCODES_FEATURES" before 2.13.0
  IF((NOT eccodes_FEATURES) AND (ECCODES_FEATURES))
    SET(eccodes_FEATURES "${ECCODES_FEATURES}")
  ENDIF()
  # MESSAGE(STATUS "Detected ecCodes features '${eccodes_FEATURES}'")
  IF("${eccodes_FEATURES}" MATCHES "ECCODES_OMP_THREADS")
    MESSAGE(STATUS "Detected ecCodes OpenMP threading support")
    SET(HAVE_GRIB_THREADSAFE 1)
  ENDIF()
ELSEIF(ENABLE_GRIBAPI)
  FIMEX_FIND_PACKAGE(grib_api_PACKAGE
    LIBRARY_NAME "grib_api"
    INCLUDE_HDR "grib_api.h"
  )
  OPTION(ENABLE_GRIBAPIPTHREAD "Use GribAPI library with pthread support" ON)
  IF(ENABLE_GRIBAPIPTHREAD)
    SET(HAVE_GRIB_THREADSAFE 1)
  ENDIF()
ENDIF()

OPTION(ENABLE_FORTRAN "Use fortran" OFF)
IF(ENABLE_FORTRAN)
  ENABLE_LANGUAGE(Fortran)
ENDIF()

OPTION(ENABLE_FIMEX_VERSIONNUMBERED "Use version numbers in filenames" ON)

# github/HowardHinnant/date
IF (NOT date_INC_DIR)
  SET (DATE_3RDPARTY "third_party/date")
  IF ((NOT date_DIR) AND (EXISTS "${CMAKE_SOURCE_DIR}/${DATE_3RDPARTY}/CMakeLists.txt"))
    SET (date_INC_DIR "${CMAKE_SOURCE_DIR}/${DATE_3RDPARTY}/include")
  ENDIF ()
ENDIF ()
FIMEX_FIND_PACKAGE(date
  CMAKE_TARGETS
  date::date     # 3.0
  date_interface # 2.4.1
  INCLUDE_HDR "date/date.h"
)

SET (MI_PROGRAMOPTIONS_3RDPARTY "third_party/mi-programoptions")
IF ((NOT mi-programoptions_DIR) AND (EXISTS "${CMAKE_SOURCE_DIR}/${MI_PROGRAMOPTIONS_3RDPARTY}/CMakeLists.txt"))
  ADD_SUBDIRECTORY(${MI_PROGRAMOPTIONS_3RDPARTY})
ELSE ()
  FIND_PACKAGE(mi-programoptions REQUIRED)
  IF (mi-programoptions_VERSION VERSION_LESS "1.0.0")
    MESSAGE(SEND_ERROR "Found mi-programoptions version ${mi-programoptions_VERSION} -- too old.")
  ENDIF ()
ENDIF ()

IF(ENABLE_FIMEX_VERSIONNUMBERED)
  SET(MINUS_FIMEX_VERSION "-${FIMEX_VERSION}")
  SET(pc_fimex_includedir "/fimex${MINUS_FIMEX_VERSION}")
  CONFIGURE_FILE("fimex-noversion.pc.in" "fimex.pc" @ONLY)
  INSTALL(FILES "${CMAKE_BINARY_DIR}/fimex.pc"    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
  SET(lib_soversion 0)
  SET(lib_version "${lib_soversion}.${FIMEX_VERSION_PATCH}.${FIMEX_VERSION_STATUS_DEC}")
ELSE()
  SET(MINUS_FIMEX_VERSION "")
  SET(pc_fimex_includedir "")
  SET(lib_soversion 0)
  SET(lib_version "${lib_soversion}.0.0")
ENDIF()
SET(lib_name "fimex${MINUS_FIMEX_VERSION}")
SET(FIMEX_INSTALL_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}${pc_fimex_includedir}")


SET(FIMEX_PC "fimex${MINUS_FIMEX_VERSION}.pc")
FIMEX_GEN_PKGCONFIG(fimex.pc.in "${FIMEX_PC}"
  ""
  "" 
  "${CMAKE_INSTALL_LIBDIR}"
  "${FIMEX_INSTALL_INCLUDEDIR};${udunits2_INC_DIR};${proj_INC_DIR};${grib_api_INC_DIR}"
)
INSTALL(FILES "${CMAKE_BINARY_DIR}/${FIMEX_PC}" DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

IF(ENABLE_FORTRAN)
  IF(ENABLE_FIMEX_VERSIONNUMBERED)
    CONFIGURE_FILE("fimexf-noversion.pc.in" "fimexf.pc" @ONLY)
    INSTALL(FILES "${CMAKE_BINARY_DIR}/fimexf.pc" DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
  ENDIF()

  CONFIGURE_FILE("fimexf.pc.in" "fimexf${MINUS_FIMEX_VERSION}.pc" @ONLY)
  INSTALL(FILES "${CMAKE_BINARY_DIR}/fimexf${MINUS_FIMEX_VERSION}.pc" DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
ENDIF()

ENABLE_TESTING()

SET (FIMEX_IO_PLUGINS_PATH "")
IF (ENABLE_NETCDF)
  SET (FIMEX_IO_PLUGINS_PATH "${FIMEX_IO_PLUGINS_PATH}:${CMAKE_BINARY_DIR}/src/io/netcdf")
ENDIF ()
IF (ENABLE_FELT)
  SET (FIMEX_IO_PLUGINS_PATH "${FIMEX_IO_PLUGINS_PATH}:${CMAKE_BINARY_DIR}/src/io/felt")
ENDIF ()
IF (ENABLE_GRIBAPI OR ENABLE_ECCODES)
  SET (FIMEX_IO_PLUGINS_PATH "${FIMEX_IO_PLUGINS_PATH}:${CMAKE_BINARY_DIR}/src/io/grib")
ENDIF ()
MESSAGE (STATUS "FIMEX_IO_PLUGINS_PATH='${FIMEX_IO_PLUGINS_PATH}'")

ADD_SUBDIRECTORY(include)
ADD_SUBDIRECTORY(src)
IF(ENABLE_FORTRAN)
  ADD_SUBDIRECTORY(modules/F90)
ENDIF()

IF(ENABLE_PYTHON)
  SET (PYBIND11_3RDPARTY_PROJECT "${CMAKE_SOURCE_DIR}/third_party/pybind11")
  IF ((NOT PYBIND11_PROJECT) AND (NOT NOT pybind11_DIR) AND (EXISTS "${PYBIND11_3RDPARTY_PROJECT}/CMakeLists.txt"))
    SET (PYBIND11_PROJECT "${PYBIND11_3RDPARTY_PROJECT}")
  ENDIF ()
  IF (PYBIND11_PROJECT)
    MESSAGE(STATUS "Including pybind11 project from ${PYBIND11_PROJECT}")
    ADD_SUBDIRECTORY(${PYBIND11_PROJECT})
  ELSE ()
    FIND_PACKAGE(pybind11 REQUIRED)
  ENDIF ()

  ADD_SUBDIRECTORY(modules/python)
ENDIF()

SET (MI_CPPTEST_3RDPARTY "third_party/mi-cpptest")
IF ((NOT mi-cpptest_DIR) AND (EXISTS "${CMAKE_SOURCE_DIR}/${MI_CPPTEST_3RDPARTY}/CMakeLists.txt"))
  ADD_SUBDIRECTORY(${MI_CPPTEST_3RDPARTY})
ELSE ()
  FIND_PACKAGE(mi-cpptest REQUIRED)
ENDIF ()

ADD_SUBDIRECTORY(test)
ADD_SUBDIRECTORY(share/etc)

SET(EXPORTNAMES "fimex")
IF(ENABLE_FORTRAN)
  LIST(APPEND EXPORTNAMES "fimexf")
ENDIF()

SET(FIMEX_VERSION_CMAKE_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/fimex${MINUS_FIMEX_VERSION}")
IF(ENABLE_FIMEX_VERSIONNUMBERED)
  SET(FIMEX_NOVERSION_CMAKE_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/fimex")
ENDIF()

FOREACH(EXPORTNAME ${EXPORTNAMES})
  INSTALL (
    EXPORT ${EXPORTNAME}
    DESTINATION "${FIMEX_VERSION_CMAKE_DIR}"
    FILE "${EXPORTNAME}${MINUS_FIMEX_VERSION}-targets.cmake"
  )

  WRITE_BASIC_PACKAGE_VERSION_FILE(
    "${CMAKE_CURRENT_BINARY_DIR}/${EXPORTNAME}${MINUS_FIMEX_VERSION}-config-version.cmake"
    VERSION
    "${FIMEX_VERSION_MAJOR}.${FIMEX_VERSION_MINOR}.${FIMEX_VERSION_PATCH}"
    COMPATIBILITY
    SameMajorVersion
  )

  CONFIGURE_FILE("export-config.cmake.in" "${EXPORTNAME}${MINUS_FIMEX_VERSION}-config.cmake" @ONLY)

  INSTALL(
    FILES
      "${CMAKE_CURRENT_BINARY_DIR}/${EXPORTNAME}${MINUS_FIMEX_VERSION}-config.cmake"
      "${CMAKE_CURRENT_BINARY_DIR}/${EXPORTNAME}${MINUS_FIMEX_VERSION}-config-version.cmake"
    DESTINATION "${FIMEX_VERSION_CMAKE_DIR}"
  )

  IF(ENABLE_FIMEX_VERSIONNUMBERED)
    CONFIGURE_FILE("export-noversion-config.cmake.in" "${EXPORTNAME}-config.cmake" @ONLY)
    CONFIGURE_FILE("export-noversion-config-version.cmake.in" "${EXPORTNAME}-config-version.cmake" @ONLY)
    INSTALL(
      FILES
        "${CMAKE_CURRENT_BINARY_DIR}/${EXPORTNAME}-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/${EXPORTNAME}-config-version.cmake"
      DESTINATION "${FIMEX_NOVERSION_CMAKE_DIR}"
    )
  ENDIF()
ENDFOREACH()

FIND_PACKAGE(Doxygen)
IF(DOXYGEN_FOUND)
  CONFIGURE_FILE("Doxyfile.in" "Doxyfile" @ONLY)
  ADD_CUSTOM_TARGET(doc
    ${DOXYGEN_EXECUTABLE} "${CMAKE_BINARY_DIR}/Doxyfile"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Generating documentation with Doxygen" VERBATIM
    )
ENDIF()
