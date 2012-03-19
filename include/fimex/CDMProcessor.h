/*
 * Fimex, CDMProcessor.h
 *
 * (C) Copyright 2012, met.no
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
 *  Created on: Mar 19, 2012
 *      Author: Heiko Klein
 */

#ifndef CDMPROCESSOR_H_
#define CDMPROCESSOR_H_

#include "fimex/CDMReader.h"
#include <boost/shared_ptr.hpp>

namespace MetNoFimex
{
/**
 * @headerfile "fimex/SliceBuilder.h"
 */

// forward decl
class CDMProcessorImpl;

/**
 * The CDMProcessor is a class for various smaller data-manipulations.
 * Examples are deaccumulation along the time-axis, ...
 */
class CDMProcessor: public MetNoFimex::CDMReader
{
public:
    CDMProcessor(boost::shared_ptr<CDMReader> dataReader);
    virtual ~CDMProcessor();
    /**
     * mark a variable for de-accumulation along the unlimited dimension, i.e.
     * vnew(n) = vold(n)-vold(n-1)
     * @param varName name of the variable to de-accumulate
     */
    void deAccumulate(const std::string& varName);
    virtual boost::shared_ptr<Data> getDataSlice(const std::string& varName, size_t unLimDimPos);
private:
    // pimpl
    boost::shared_ptr<CDMProcessorImpl> p_;

};

} /* namespace MetNoFimex */
#endif /* CDMPROCESSOR_H_ */
