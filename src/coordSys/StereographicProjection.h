/*
 * Fimex, StereographicProjection.h
 *
 * (C) Copyright 2010, met.no
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
 *  Created on: Apr 28, 2010
 *      Author: Heiko Klein
 */

#ifndef STEREOGRAPHICPROJECTION_H_
#define STEREOGRAPHICPROJECTION_H_

#include "ProjectionImpl.h"

namespace MetNoFimex
{

class StereographicProjection: public MetNoFimex::ProjectionImpl
{

public:
    StereographicProjection() : ProjectionImpl("stereographic", false) {}
    virtual ~StereographicProjection() {}
protected:
    StereographicProjection(std::string name) : ProjectionImpl(name, false) {}
    virtual std::ostream& getProj4ProjectionPart(std::ostream& oproj) const {
        oproj << "+proj=stere";
        addParameterToStream(oproj, "latitude_of_projection_origin", " +lat_0=");
        addParameterToStream(oproj, "straight_vertical_longitude_from_pole", " +lon_0="); // polar-stereographic
        addParameterToStream(oproj, "longitude_of_projection_origin", " +lon_0="); // stereographic
        addParameterToStream(oproj, "scale_factor_at_projection_origin", " +k=");
        addParameterToStream(oproj, "standard_parallel", " +lat_ts="); // only polar-stereographic, exclusive with k
        addParameterToStream(oproj, "false_easting", " +x_0=");
        addParameterToStream(oproj, "false_northing", " +y_0=");
        return oproj;
    }


};

}


#endif /* STEREOGRAPHICPROJECTION_H_ */
