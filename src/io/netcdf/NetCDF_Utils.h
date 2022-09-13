/*
 * Fimex
 *
 * (C) Copyright 2008, met.no
 *
 * Project Info:  https://wiki.met.no/fimex/start
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */

#ifndef NETCDF_UTILS_H_
#define NETCDF_UTILS_H_

#include "fimex/CDMDataType.h"
#include "fimex/DataDecl.h"
#include "fimex/MutexLock.h"

#include <memory>

#include "fimex_netcdf_config.h"
#ifndef HAVE_NETCDF_HDF5_LIB
#undef NC_NETCDF4 /* netcdf4.1.2-4.2 define NC_NETCDF4 even when functions are not in library */
#endif            // HAVE_NETCDF_HDF5_LIB
#ifdef HAVE_MPI
#include "fimex/mifi_mpi.h"
extern "C" {
#include "netcdf_par.h"
}
#else // !HAVE_MPI
extern "C" {
#include "netcdf.h"
}
#endif // !HAVE_MPI

#include "unistd.h"

#define NCMUTEX_LOCKED(x)                                                                                                                                      \
    do {                                                                                                                                                       \
        OmpScopedLock lock(Nc::getMutex());                                                                                                                    \
        x;                                                                                                                                                     \
    } while (0)

namespace MetNoFimex {

/// storage class for netcdf-file pointer
class Nc
{
public:
    Nc();
    ~Nc();
    static OmpMutex& getMutex(); // lock against common reading/writing in nc4
    std::string filename;
    int ncId;
    int format;
    bool isOpen;
    pid_t pid;
    void reopen_if_forked();
    bool supports_nc_string() const { return format == NC_FORMAT_NETCDF4; }
};

/**
 * @headerfile "fimex/NetCDF_Utils.h"
 */

/**
 * conversion from CDMDataType to NcType
 */
nc_type cdmDataType2ncType(CDMDataType dt);

/**
 * conversion from nc_type to CDMDataType
 */
CDMDataType ncType2cdmDataType(nc_type nctype);

/**
 * read a nc-status and throw an error if status != NC_NOERR
 */
void ncCheck(int status);
void ncCheck(int status, const std::string& msg);

/**
 * read values from an attribute to a data
 * @param ncId netcdf file id
 * @param varId variable id or NC_GLOBAL
 * @param attName attribute name
 * @param type attribute datatype (in netcdf-notation)
 */
DataPtr ncGetAttValues(int ncId, int varId, const std::string& attName, nc_type type);
/**
 * read value-slices from a variable to a data
 * @param ncId netcdf file id
 * @param varId variable id or NC_GLOBAL
 * @param type attribute datatype (in netcdf-notation)
 * @param dimLen number of dimensions
 * @param start start-point for each dimension
 * @param count size in each dimension
 */
DataPtr ncGetValues(int ncId, int varId, nc_type type, size_t dimLen, const size_t* start, const size_t* count);

/**
 * write value-slices from a variable to disk
 * @param data the data to put
 * @param ncId netcdf file id
 * @param varId variable id or NC_GLOBAL
 * @param type attribute datatype (in netcdf-notation)
 * @param dimLen number of dimensions
 * @param start start-point for each dimension
 * @param count size in each dimension
 */
void ncPutValues(DataPtr data, int ncId, int varId, nc_type type, size_t dimLen, const size_t* start, const size_t* count);

} // namespace MetNoFimex

#endif /*NETCDF_UTILS_H_*/
