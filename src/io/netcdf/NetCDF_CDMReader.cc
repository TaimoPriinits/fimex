/*
 * Fimex
 *
 * (C) Copyright 2008-2022, met.no
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

#define MIFI_IO_READER_SUPPRESS_DEPRECATED
#include "NetCDF_CDMReader.h"
#undef MIFI_IO_READER_SUPPRESS_DEPRECATED

#include "fimex/CDM.h"
#include "fimex/Data.h"
#include "fimex/Logger.h"
#include "fimex/SliceBuilder.h"
#include "fimex/String2Type.h"
#include "fimex/StringUtils.h"

#include "NetCDF_Utils.h"

#include <cassert>
#include <cstdlib>

namespace MetNoFimex {

using namespace std;

static Logger_p logger = getLogger("fimex.NetCDF_CDMReader");

NetCDF_CDMReader::NetCDF_CDMReader(const std::string& filename, bool writeable)
    : ncFile(new Nc())
{
    size_t fimexSlots = 521;
    if (char* fimexSlotsChar = getenv("FIMEX_CHUNK_CACHE_SLOTS"))
        fimexSlots = string2type<size_t>(fimexSlotsChar);
    if (char* fimexCache = getenv("FIMEX_CHUNK_CACHE_SIZE")) {
        size_t cacheSize = string2type<size_t>(fimexCache);
        cacheSize /= fimexSlots;
        LOG4FIMEX(logger, Logger::DEBUG, "setting chunk cache to " << fimexSlots << " slots of size " << (long)(cacheSize / 1024 / 1024) << "MB");

        OmpScopedLock lock(Nc::getMutex());
        nc_set_chunk_cache(cacheSize, fimexSlots, 0.75);
    }
    ncFile->filename = filename;

    OmpScopedLock lock(Nc::getMutex());
    ncCheck(nc_open(ncFile->filename.c_str(), writeable ? NC_WRITE : NC_NOWRITE, &ncFile->ncId), "opening " + ncFile->filename);
    ncFile->isOpen = true;

    // investigate the dimensions
    {
        int ndims;
        ncCheck(nc_inq_ndims(ncFile->ncId, &ndims));
        int recid;
        ncCheck(nc_inq_unlimdim(ncFile->ncId, &recid));
        // read metadata to cdm
        // define dimensions
        char ncName[NC_MAX_NAME + 1];
        for (int i = 0; i < ndims; ++i) {
            size_t dimlen;
            ncCheck(nc_inq_dimname(ncFile->ncId, i, ncName));
            ncCheck(nc_inq_dimlen(ncFile->ncId, i, &dimlen));
            OmpScopedUnlock unlock(Nc::getMutex());
            CDMDimension d(string(ncName), dimlen);
            d.setUnlimited(recid == i);
            cdm_->addDimension(d);
        }
    }

    // define variables
    {
        int nvars;
        ncCheck(nc_inq_nvars(ncFile->ncId, &nvars));
        int ndims;
        int dimids[NC_MAX_VAR_DIMS];
        int natts;
        char ncName[NC_MAX_NAME + 1];
        for (int i = 0; i < nvars; ++i) {
            nc_type dtype;
            nc_inq_var(ncFile->ncId, i, ncName, &dtype, &ndims, dimids, &natts);
            std::vector<std::string> shape;
            shape.reserve(ndims);
            // reverse dimensions
            char dimName[NC_MAX_NAME + 1];
            for (int j = ndims - 1; j >= 0; --j) {
                ncCheck(nc_inq_dimname(ncFile->ncId, dimids[j], dimName));
                shape.push_back(dimName);
            }
            {
                OmpScopedUnlock unlock(Nc::getMutex());
                CDMDataType type = ncType2cdmDataType(dtype);
                cdm_->addVariable(CDMVariable(ncName, type, shape));
            }
            // define the attributes of the variable
            char attName[NC_MAX_NAME + 1];
            for (int j = 0; j < natts; ++j) {
                ncCheck(nc_inq_attname(ncFile->ncId, i, j, attName));
                addAttribute(ncName, i, attName);
            }
        }
    }

    // define global attributes
    {
        int natts;
        nc_inq_varnatts(ncFile->ncId, NC_GLOBAL, &natts);
        char attName[NC_MAX_NAME + 1];
        for (int j = 0; j < natts; ++j) {
            ncCheck(nc_inq_attname(ncFile->ncId, NC_GLOBAL, j, attName));
            addAttribute(cdm_->globalAttributeNS(), NC_GLOBAL, attName);
        }
    }
}

NetCDF_CDMReader::~NetCDF_CDMReader() {}

DataPtr NetCDF_CDMReader::getDataSlice(const std::string& varName, size_t unLimDimPos)
{
    const CDMVariable& var = cdm_->getVariable(varName);
    if (var.hasData()) {
        return getDataSliceFromMemory(var, unLimDimPos);
    }

    OmpScopedLock lock(Nc::getMutex());
    ncFile->reopen_if_forked();
    int varid;
    ncCheck(nc_inq_varid(ncFile->ncId, var.getName().c_str(), &varid));
    nc_type dtype;
    ncCheck(nc_inq_vartype(ncFile->ncId, varid, &dtype));
    int dimLen;
    ncCheck(nc_inq_varndims(ncFile->ncId, varid, &dimLen));
    int dimIds[dimLen];
    ncCheck(nc_inq_vardimid(ncFile->ncId, varid, &dimIds[0]));
    size_t count[dimLen];
    size_t start[dimLen];
    for (int i = 0; i < dimLen; ++i) {
        start[i] = 0;
        ncCheck(nc_inq_dimlen(ncFile->ncId, dimIds[i], &count[i]));
    }
    if (cdm_->hasUnlimitedDim(var)) {
        // unlimited dim always at 0
        start[0] = unLimDimPos;
        count[0] = 1;
    }
    {
        OmpScopedUnlock unlock(Nc::getMutex());
        LOG4FIMEX(logger, Logger::DEBUG,
                  "ncGetValues for " << varName << ": (" << join(start, start + dimLen) << ") size (" << join(count, count + dimLen) << ")");
    }
    return ncGetValues(ncFile->ncId, varid, dtype, static_cast<size_t>(dimLen), start, count);
}

DataPtr NetCDF_CDMReader::getDataSlice(const std::string& varName, const SliceBuilder& sb)
{
    const CDMVariable& var = cdm_->getVariable(varName);
    if (var.hasData()) {
        return var.getData()->slice(sb.getMaxDimensionSizes(), sb.getDimensionStartPositions(), sb.getDimensionSizes());
    }

    ncFile->reopen_if_forked();

    int varid, dimLen;
    nc_type dtype;
    {
        OmpScopedLock lock(Nc::getMutex());
        ncCheck(nc_inq_varid(ncFile->ncId, var.getName().c_str(), &varid));
        ncCheck(nc_inq_vartype(ncFile->ncId, varid, &dtype));
        ncCheck(nc_inq_varndims(ncFile->ncId, varid, &dimLen));
    }

    const vector<size_t> start(sb.getDimensionStartPositions().rbegin(), sb.getDimensionStartPositions().rend());
    assert(start.size() == static_cast<size_t>(dimLen));

    const vector<size_t> count(sb.getDimensionSizes().rbegin(), sb.getDimensionSizes().rend());
    assert(count.size() == static_cast<size_t>(dimLen));

    LOG4FIMEX(logger, Logger::DEBUG,
              "ncGetValues SB for " << varName << ": (" << join(start.begin(), start.end()) << ") size (" << join(count.begin(), count.end()) << ")");

    OmpScopedLock lock(Nc::getMutex());
    return ncGetValues(ncFile->ncId, varid, dtype, static_cast<size_t>(dimLen), &start[0], &count[0]);
}

void NetCDF_CDMReader::sync()
{
    OmpScopedLock lock(Nc::getMutex());
    ncCheck(nc_sync(ncFile->ncId));
}

void NetCDF_CDMReader::putDataSlice(const std::string& varName, size_t unLimDimPos, const DataPtr data)
{
    CDMVariable& var = cdm_->getVariable(varName);

    OmpScopedLock lock(Nc::getMutex()); // FIXME abusing ncMutex as a little bit of protection for "var.setData"
    if (var.hasData()) {
        var.setData(DataPtr());
    }

    // no data, no write
    if (!data || data->size() == 0)
        return;

    int varid, dimLen;
    nc_type dtype;
    ncCheck(nc_inq_varid(ncFile->ncId, var.getName().c_str(), &varid));
    ncCheck(nc_inq_vartype(ncFile->ncId, varid, &dtype));
    ncCheck(nc_inq_varndims(ncFile->ncId, varid, &dimLen));
    int dimIds[dimLen];
    ncCheck(nc_inq_vardimid(ncFile->ncId, varid, &dimIds[0]));
    size_t count[dimLen];
    size_t start[dimLen];
    for (int i = 0; i < dimLen; ++i) {
        start[i] = 0;
        ncCheck(nc_inq_dimlen(ncFile->ncId, dimIds[i], &count[i]));
    }
    {
        OmpScopedUnlock unlock(Nc::getMutex());
        if (cdm_->hasUnlimitedDim(var)) {
            // unlimited dim always at 0
            start[0] = unLimDimPos;
            count[0] = 1;
        }
        LOG4FIMEX(logger, Logger::DEBUG,
                  "ncPutValues for " << varName << ": (" << join(start, start + dimLen) << ") size (" << join(count, count + dimLen) << ")");
    }
    ncPutValues(data, ncFile->ncId, varid, dtype, static_cast<size_t>(dimLen), start, count);
}

void NetCDF_CDMReader::putDataSlice(const std::string& varName, const SliceBuilder& sb, const DataPtr data)
{
    CDMVariable& var = cdm_->getVariable(varName);
    if (var.hasData()) {
        var.setData(DataPtr());
    }

    // no data, no write
    if (!data || data->size() == 0)
        return;

    int varid, dimLen;
    nc_type dtype;
    {
        OmpScopedLock lock(Nc::getMutex());
        ncCheck(nc_inq_varid(ncFile->ncId, var.getName().c_str(), &varid));
        ncCheck(nc_inq_vartype(ncFile->ncId, varid, &dtype));
        ncCheck(nc_inq_varndims(ncFile->ncId, varid, &dimLen));
    }

    // netcdf/c++ uses opposite dimension numbering => rbegin/rend
    const vector<size_t> start(sb.getDimensionStartPositions().rbegin(), sb.getDimensionStartPositions().rend());
    assert(start.size() == static_cast<size_t>(dimLen));

    // netcdf/c++ uses opposite dimension numbering => rbegin/rend
    const vector<size_t> count(sb.getDimensionSizes().rbegin(), sb.getDimensionSizes().rend());
    assert(count.size() == static_cast<size_t>(dimLen));

    LOG4FIMEX(logger, Logger::DEBUG,
              "ncPutValues SB for " << varName << ": (" << join(start.begin(), start.end()) << ") size (" << join(count.begin(), count.end()) << ")");

    OmpScopedLock lock(Nc::getMutex());
    ncPutValues(data, ncFile->ncId, varid, dtype, static_cast<size_t>(dimLen), &start[0], &count[0]);
}

void NetCDF_CDMReader::addAttribute(const std::string& varName, int varid, const string& attName)
{
    nc_type dtype;
    ncCheck(nc_inq_atttype(ncFile->ncId, varid, attName.c_str(), &dtype));
    DataPtr attrData = ncGetAttValues(ncFile->ncId, varid, attName, dtype);

    OmpScopedUnlock unlock(Nc::getMutex());
    cdm_->addAttribute(varName, CDMAttribute(attName, attrData));
}

} // namespace MetNoFimex
