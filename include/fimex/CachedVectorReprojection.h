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

#ifndef CACHEDVECTORREPROJECTION_H_
#define CACHEDVECTORREPROJECTION_H_

#include "fimex/SharedArray.h"

namespace MetNoFimex {
namespace reproject {
struct Matrix;
typedef std::shared_ptr<const Matrix> Matrix_cp;
} // namespace reproject

class CachedVectorReprojection
{
public:
    CachedVectorReprojection() = delete;

    CachedVectorReprojection(reproject::Matrix_cp matrix);

    virtual ~CachedVectorReprojection() {}

    /**
     *  reproject the vector values
     *
     * @param uValues the values in x-direction. These will be changed in-place.
     * @param vValues the values in y-direction. These will be changed in-place.
     * @param size the size of both arrays
     */
    void reprojectValues(shared_array<float>& uValues, shared_array<float>& vValues, size_t size) const;

    /**
     * reproject directions given in angles in degree
     * @param angles direction of vector in each grid-cell, given in degree
     * @param size the size of the angles-array
     */
    void reprojectDirectionValues(shared_array<float>& angles, size_t size) const;

    // @return size of the spatial plane in x-direction
    size_t getXSize() const {return ox;}

    // @return size of the spatial plane in y-direction
    size_t getYSize() const {return oy;}

private:
    reproject::Matrix_cp matrix;
    size_t ox;
    size_t oy;
};

} // namespace MetNoFimex

#endif /*CACHEDVECTORREPROJECTION_H_*/
