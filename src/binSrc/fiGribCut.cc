/*
 * Fimex, fiGribCut.cc
 *
 * (C) Copyright 2009-2022, met.no
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
 *  Created on: Dec 11, 2009
 *      Author: Heiko Klein
 */

#include "fimex/CDMconstants.h"
#include "fimex/Data.h"
#include "fimex/GribUtils.h"
#include "fimex/SharedArray.h"
#include "fimex/String2Type.h"
#include "fimex/StringUtils.h"
#include "fimex/ThreadPool.h"
#include "fimex/Type2String.h"

#include <mi_programoptions.h>

#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>

#include <grib_api.h>

namespace po = miutil::program_options;

using namespace std;
using namespace MetNoFimex;

static int debug = 0;

static void writeUsage(ostream& out, const po::option_set& options)
{
    out << "usage: fiGribCut --outputFile PATH --inputFile gribFile [--inputfile gribfile] " << endl;
    out << "                 --parameter PARAM1 [--parameter PARAM2]" << endl;
    out << endl;
    options.help(out);
}

static bool gribMatchParameters(const std::shared_ptr<grib_handle>& gh, vector<long> parameters)
{
    if (parameters.size() == 0) return true; // all parameters requested
    long param;
    MIFI_GRIB_CHECK(grib_get_long(gh.get(), "paramId", &param), 0);
    return find(parameters.begin(), parameters.end(), param) != parameters.end();
}

/**
 * find the first and last index of 'first' and 'last' in
 * the linear data described by 'start + i*incr', 0 <= i < n
 * first and last are included in the
 * range f(firstPos), f(lastPos) if firstPos >= 0 and lastPos < n (first and last inside data-range)
 * The algorithm tends to select a rather larger area to make sure above is true.
 *
 * @param start of the linear data
 * @param incr of the linear data
 * @param n size of the linear elements
 * @param first value of position to find
 * @param last value of position to find
 * @return firstPos, lastPos
 */
static pair<size_t, size_t> findFirstLastIndex(double start, double incr, size_t n, double first, double last)
{
    // avoid rounding errors, use a bit more
    if (incr > 0) {
        first -= 1e-5;
        last += 1e-5;
        if (first > last) throw runtime_error("cannot detect inverse bounding-box with: " + type2string(first) + " > " + type2string(last));
    } else {
        first += 1e-5;
        last += 1e-5;
        if (first < last) throw runtime_error("cannot detect inverse bounding-box with: " + type2string(first) + " < " + type2string(last));
    }

    size_t firstPos = 0;
    size_t lastPos = 0;
    for (size_t i = 0; i < n; ++i) {
        double current = start + i * incr;
        if (incr > 0) {
            if (first >= current) firstPos = i; // forward position
            if (last > current) lastPos = i; // forward position
        } else {
            if (first <= current) firstPos = i; // forward position
            if (last < current) lastPos = i; // forward position
        }
    }
    // last should be <= f(lastPos)
    lastPos += 1;
    // range-check
    if (lastPos >= n) lastPos = n-1;

    return make_pair(firstPos, lastPos);
}

static std::shared_ptr<grib_handle> cutBoundingBox(const std::shared_ptr<grib_handle>& gh, map<string, double> bb)
{
    std::shared_ptr<grib_handle> newGh = gh;
    if (bb.size() == 4) {
        char msg[1024];
        size_t msgLength = 1024;
        MIFI_GRIB_CHECK(grib_get_string(gh.get(), "typeOfGrid", msg, &msgLength), 0);
        string typeOfGrid(msg);
        if (typeOfGrid != "regular_ll") {
            throw runtime_error("cannot only attach bounding-box to type regular_ll, got type " + MetNoFimex::type2string(typeOfGrid));
        } else {
            // create a clone of the handle for later modifications
            newGh = std::shared_ptr<grib_handle>(grib_handle_clone(gh.get()), grib_handle_delete);
            GridDefinition::Orientation orient = MetNoFimex::gribGetGridOrientation(gh);
            if (!(orient == GridDefinition::LeftLowerHorizontal
                  || orient == MetNoFimex::GridDefinition::LeftUpperHorizontal)) {
                throw runtime_error("cannot change bounding-box of data without LeftLowerHorizontal/LeftUpperHorizontal orientation/scanning mode "+type2string(orient));
            }
            // modify bounding box
            double latFirst, lonFirst, latIncr, lonIncr;
            long latN, lonN;
            // latitude scans starts in south
            MIFI_GRIB_CHECK(grib_get_double(gh.get(), "latitudeOfFirstGridPointInDegrees", &latFirst), 0);
            MIFI_GRIB_CHECK(grib_get_double(gh.get(), "jDirectionIncrementInDegrees", &latIncr), 0);
            if (orient == GridDefinition::LeftLowerHorizontal) {
                latIncr *= -1; // j scans (usually) negatively (see orientation check above)
            }

            MIFI_GRIB_CHECK(grib_get_double(gh.get(), "longitudeOfFirstGridPointInDegrees", &lonFirst), 0);
            MIFI_GRIB_CHECK(grib_get_double(gh.get(), "iDirectionIncrementInDegrees", &lonIncr), 0);
            MIFI_GRIB_CHECK(grib_get_long(gh.get(), "Nj", &latN), 0);
            MIFI_GRIB_CHECK(grib_get_long(gh.get(), "Ni", &lonN), 0);

            pair<size_t,size_t> latFirstLast;
            if (orient == GridDefinition::LeftLowerHorizontal) {
                // north > south, due to negative scan
                latFirstLast = findFirstLastIndex(latFirst,latIncr,latN, bb["north"], bb["south"]);
            } else {
                latFirstLast = findFirstLastIndex(latFirst,latIncr,latN, bb["south"], bb["north"]);
            }
            if (debug) cerr << "found latitude bounds at (" << latFirstLast.first << ","<< latFirstLast.second << ") of max " << latN << endl;
            pair<size_t,size_t> lonFirstLast = findFirstLastIndex(lonFirst,lonIncr,lonN, bb["west"], bb["east"]);
            if (debug) cerr << "found longitude bounds at (" << lonFirstLast.first << ","<< lonFirstLast.second << ") of max " << lonN << endl;

            // read the data
            size_t nv;
            MIFI_GRIB_CHECK(grib_get_size(gh.get(), "values", &nv), 0);
            if (nv != static_cast<unsigned long>(latN*lonN)) {
                throw runtime_error("numberOfValues ("+type2string(nv) + ") != latN*lonN ("+type2string(latN)+"*"+type2string(lonN)+")");
            }
            auto array = make_shared_array<double>(nv);
            if (debug) cerr << "reading " << nv << " values" << endl;
            MIFI_GRIB_CHECK(grib_get_double_array(gh.get(), "values", &array[0], &nv), 0);
            if (debug) cerr << "got " << nv << " values" << endl;
            DataPtr data = createData(nv, array);

            // slice the data
            vector<size_t> orgDim(2,0);
            orgDim[0] = lonN;
            orgDim[1] = latN;
            vector<size_t> newDim(2,0);
            newDim[0] = lonFirstLast.second - lonFirstLast.first;
            newDim[1] = latFirstLast.second - latFirstLast.first;
            vector<size_t> startPos(2,0);
            startPos[0] = lonFirstLast.first;
            startPos[1] = latFirstLast.first;
            if (debug) cerr << "slicing values" << endl;
            DataPtr outData = data->slice(orgDim, startPos, newDim);
            assert(outData->size() == (newDim[0]*newDim[1]));

            // write the new data
            string gridSimple("grid_simple");
            size_t size = gridSimple.size();
            // don't support other packing types
            if (debug) cerr << "setting packing type" << endl;
            MIFI_GRIB_CHECK(grib_set_string(newGh.get(), "typeOfPacking", gridSimple.c_str(), &size),0);

            // set the bounding-box
            if (debug) cerr << "setting new bounding box" << endl;
            MIFI_GRIB_CHECK(grib_set_double(newGh.get(), "latitudeOfFirstGridPointInDegrees", (latFirst + latIncr*latFirstLast.first)),0);
            MIFI_GRIB_CHECK(grib_set_double(newGh.get(), "latitudeOfLastGridPointInDegrees", (latFirst + latIncr*latFirstLast.second)),0);
            MIFI_GRIB_CHECK(grib_set_double(newGh.get(), "Nj", (latFirstLast.second-latFirstLast.first)),0);
            MIFI_GRIB_CHECK(grib_set_double(newGh.get(), "longitudeOfFirstGridPointInDegrees", (lonFirst + lonIncr*lonFirstLast.first)),0);
            MIFI_GRIB_CHECK(grib_set_double(newGh.get(), "longitudeOfLastGridPointInDegrees", (lonFirst + lonIncr*lonFirstLast.second)),0);
            MIFI_GRIB_CHECK(grib_set_double(newGh.get(), "Ni", (lonFirstLast.second-lonFirstLast.first)),0);

            // set the data
            auto outArray = outData->asDouble();
            if (debug) cerr << "setting new data" << endl;
            MIFI_GRIB_CHECK(grib_set_double_array(newGh.get(), "values", &outArray[0], outData->size()), 0);

        }
    }
    return newGh;
}

// work on one grib_hanlde, return number of errors
static int gribCutHandle(ostream& outStream, const std::shared_ptr<grib_handle>& gh, const vector<long>& parameters, const map<string, double>& bb)
{
    // skip parameters if not matching
    if (!gribMatchParameters(gh, parameters)) return 0;

    // test the bounding box
    std::shared_ptr<grib_handle> output_gh = cutBoundingBox(gh, bb);

    // write data to file
    size_t bufferSize;
    const void* buffer;
    /* get the coded message in a buffer */
    MIFI_GRIB_CHECK(grib_get_message(output_gh.get(),&buffer,&bufferSize),0);
    outStream.write(reinterpret_cast<const char*>(buffer), bufferSize);
    return 0;
}

// work on all files/all messages, return number of errors
static int gribCut(ostream& outStream, const vector<string>& inputFiles, const vector<long>& parameters, const map<string, double>& bb)
{
    int errors = 0;
    for (vector<string>::const_iterator file = inputFiles.begin(); file != inputFiles.end(); ++file) {
        std::shared_ptr<FILE> fh(fopen(file->c_str(), "rb"), fclose);
        if (fh.get() == 0) {
            cerr << "cannot open file: " << *file << endl;
            ++errors;
        } else {
            // enable multi-messages
            grib_multi_support_on(0);
            while (!feof(fh.get())) {
                // read the messages of interest
                size_t pos = ftell(fh.get());
                int err = 0;
                std::shared_ptr<grib_handle> gh(grib_handle_new_from_file(0, fh.get(), &err), grib_handle_delete);
                size_t newPos = ftell(fh.get());
                if (debug > 0) {
                    cerr << "fetching handle from file " << *file << " from pos " << pos << " to pos " << newPos << endl;
                }
                // check for errors
                if (gh.get() != 0) {
                    // something wrong with file, abbort
                    try {
                        if (err != GRIB_SUCCESS) MIFI_GRIB_CHECK(err,0);
                        // parse the grib handle
                        errors += gribCutHandle(outStream, gh, parameters, bb);
                    } catch (exception& ex) {
                        errors++;
                        cerr << "ERROR: " << ex.what() << endl;
                    }
                }
            }
        }
    }
    return errors;
}

int main(int argc, char* args[])
{
    // only use one thread
    mifi_setNumThreads(1);

    /*
     * inputFile: path; repeatable = concatenation
     * outputFile: path; not-repeatable
     * parameter: grib-api parameterIds; repeatable, not required
     * boundingBox: north,east,south,west values of geographical boundingbox, i.e. 90,30,60,-30; non-repeatable, not required
     */

    const po::option op_help = po::option("help", "help message").set_shortkey("h").set_narg(0);
    const po::option op_debug = po::option("debug", "enable debug").set_shortkey("d").set_narg(0);
    const po::option op_version = po::option("version", "program version").set_shortkey("v").set_narg(0);
    const po::option op_outputFile = po::option("outputFile", "outputFile").set_shortkey("o");
    const po::option op_inputFile = po::option("inputFile", "input gribFile").set_composing().set_shortkey("i");
    const po::option op_parameter = po::option("parameter", "grib-parameterID").set_composing().set_shortkey("p");
    const po::option op_boundingBox = po::option("boundingBox", "bounding-box, north,east,south,west").set_shortkey("b");

    po::option_set options;
    options
        << op_help
        << op_debug
        << op_version
        << op_outputFile
        << op_inputFile
        << op_parameter
        << op_boundingBox
        ;

    // read the options
    po::string_v positional;
    po::value_set vm = po::parse_command_line(argc, args, options, positional);

    debug = vm.is_set(op_debug);

    if (argc == 1 || vm.is_set(op_help)) {
        writeUsage(cout, options);
        return 0;
    }
    if (vm.is_set(op_version)) {
        cout << "fiIndexGribs version " << fimexVersion() << endl;
        return 0;
    }
    if (!vm.is_set(op_inputFile)) {
        cerr << "missing input file" << endl;
        writeUsage(cout, options);
        return 1;
    }
    const vector<string>& inputFiles = vm.values(op_inputFile);

    if (!vm.is_set(op_outputFile)) {
        cerr << "missing output file" << endl;
        writeUsage(cout, options);
        return 1;
    }

    const std::string& outputFile = vm.value(op_outputFile);
    vector<long> parameters;
    if (vm.is_set(op_parameter)) {
        parameters = strings2types<long>(vm.values(op_parameter));
    }

    map<string, double> bb;
    if (vm.is_set(op_boundingBox)) {
        const vector<string> bbVec = tokenize(vm.value(op_boundingBox), ",");
        if (bbVec.size() < 4) {
            cerr << "boundingBox requires 4 values: north,east,south,west, got " << bb.size() << " values: " << vm.value(op_boundingBox) << endl;
            return 1;
        }
        double north = MetNoFimex::string2type<double>(bbVec.at(0));
        double east  = MetNoFimex::string2type<double>(bbVec.at(1));
        double south = MetNoFimex::string2type<double>(bbVec.at(2));
        double west  = MetNoFimex::string2type<double>(bbVec.at(3));
        if (north > 90 || north < -90) {
            cerr << "north needs to be between -90 and 90 degree, is: "<< north << endl;
            return 1;
        }
        if (south > 90 || south < -90) {
            cerr << "south needs to be between -90 and 90 degree, is: "<< south << endl;
            return 1;
        }
        if (south > north) {
            cerr << "south must be <= north" << endl;
            return 1;
        }
        if (east > 180 || east < -180) {
            cerr << "east needs to be between -180 and 180 degree, is: "<< east << endl;
            return 1;
        }
        if (west > 180 || west < -180) {
            cerr << "west needs to be between -180 and 180 degree, is: "<< west << endl;
            return 1;
        }
        if (west > east) {
            cerr << "west must be <= east" << endl;
            return 1;
        }
        bb["north"] = north;
        bb["south"] = south;
        bb["east"] = east;
        bb["west"] = west;
    }

    int errors;
    if (outputFile != "-") {
        std::ofstream outStream(outputFile, std::ios::binary);
        errors = gribCut(outStream, inputFiles, parameters, bb);
    } else {
        errors = gribCut(std::cout, inputFiles, parameters, bb);
    }
    if (errors > 0) {
        std::cerr << "found " << errors << " errors" << endl;
    } else {
        if (debug)
            std::cerr << "success, no errors detected" << endl;
    }
    return errors;
}
