/* -*- c++ -*-
 * Fimex, CDMReaderWriter.h
 *
 * (C) Copyright 2013-2022, met.no
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
 *
 *  Created on: Feb 4, 2013
 *      Author: heikok
 */

#ifndef CDMREADERWRITER_H_
#define CDMREADERWRITER_H_

#include "fimex/CDMReader.h"
#include "fimex/UnitsConverterDecl.h"

namespace MetNoFimex {

/**
 * @headerfile fimex/CDMReaderWriter.h
 */
/**
 * The CDMReaderWriter is an interface allowing to write data to an opened CDMReader.
 */
class CDMReaderWriter : public CDMReader
{
public:
    CDMReaderWriter();
    ~CDMReaderWriter();

    /**
     * @brief flush data to disc
     */
    virtual void sync() = 0;

    /**
     * @brief data-writing function
     *
     * This methods needs to be implemented by the CDMReaderWriter. It will write back the data
     * to a variable at unLimPos, without any changes (except eventually required change of datatypes).
     *
     * This function will write the whole data for a dataset without unlimited dimension if
     * the unLimDimPos == 0.
     *
     * @param varName name of the variable to read
     * @param unLimDimPos (optional) if the variable contains a unlimited dimension (max one allowed) an slice of this position is returned
     * @param data Data to be written to unLimDimPos
     * @throw CDMException on errors related to the CDM in combination with the underlying data-structure. It might also throw other (IO-)exceptions.
     */
    virtual void putDataSlice(const std::string& varName, size_t unLimDimPos, const DataPtr data) = 0;

    /**
     * @brief data-writing  function for general slices
     *
     * @param varName name of the variable to read
     * @param sb a SliceBuilder generated from this CDMReaders CDM
     * @param data Data to be written at the slicePositions
     * @throw CDMException on errors related to the CDM in combination with the underlying data-structure. It might also throw other (IO-)exceptions.
     * @warning This method must be implemented for performance reasons.
     */
    virtual void putDataSlice(const std::string& varName, const SliceBuilder& sb, const DataPtr data) = 0;

    /**
     * @brief unscale and write a dataslice
     *
     * This un-scales (back to the in-file scaling) and writes a data slice; the inverse of getScaledDataSlice.
     *
     * @param varName name of the variable to write
     * @param unLimDimPos (optional) if the variable contains a unlimited dimension (max one allowed) a slice of this position is written
     * @param data the data slice to write
     * @throw CDMException on errors related to the CDM in combination with the underlying data-structure. It might also throw other (IO-)exceptions.
     * @see getScaledDataSlice(varName, unLimDimPos)
     */
    virtual void putScaledDataSlice(const std::string& varName, size_t unLimDimPos, const DataPtr data);

    /**
     * @brief unscale and write a dataslice from a known unit
     *
     * This un-scales (back to the in-file scaling and unit) and
     * writes a data slice; the inverse of getScaledDataSliceInUnit.
     * The data must be in unit 'unit'.
     *
     * @param varName name of the variable to read
     * @param unit unit-string, the unit of the data passed in 'data'
     * @param unLimDimPos (optional) if the variable contains a unlimited dimension (max one allowed) a slice of this position is written
     * @param data the data slice to write, must be in unit 'unit'
     * @throw CDMException on errors related to the CDM in combination with the underlying data-structure. It might also throw other (IO-)exceptions.
     * @see getScaledDataSliceInUnit(varName, unit, unLimDimPos)
     */
    virtual void putScaledDataSliceInUnit(const std::string& varName, const std::string& unit, size_t unLimDimPos, const DataPtr data);

    /**
     * @brief unscale and write a dataslice
     * @param varName name of the variable to read
     * @param sb SliceBuilder to restrict the data
     * @param data the data slice to write
     * @throw CDMException on errors related to the CDM in combination with the underlying data-structure. It might also throw other (IO-)exceptions.
     * @see getScaledDataSlice(varName, sb)
     */
    virtual void putScaledDataSlice(const std::string& varName, const SliceBuilder& sb, const DataPtr data);

    /**
     * @brief unscale and write a dataslice from a known unit
     * @param varName name of the variable to read
     * @param dataUnit unit-string, the unit of the data passed in 'data'
     * @param sb SliceBuilder to restrict the data
     * @param data the data slice to write, must be in unit 'unit'
     * @throw CDMException on errors related to the CDM in combination with the underlying data-structure. It might also throw other (IO-)exceptions.
     * @see getScaledDataSliceInUnit(varName, unit, unLimDimPos)
     */
    virtual void putScaledDataSliceInUnit(const std::string& varName, const std::string& dataUnit, const SliceBuilder& sb, const DataPtr data);

private:
    DataPtr unscaleDataOf(const std::string& varName, DataPtr data, double unitScale=1, double unitOffset=0);
    DataPtr unscaleDataOf(const std::string& varName, DataPtr data, UnitsConverter_p uc);
    DataPtr unscaleDataFromUnitOf(const std::string& varName, DataPtr data, const std::string& dataUnit);
}; /* class */

} // namespace MetNoFimex

#endif /* CDMREADERWRITER_H_ */
