/*
 fimex

 Copyright (C) 2011 met.no

 Contact information:
 Norwegian Meteorological Institute
 Box 43 Blindern
 0313 OSLO
 NORWAY
 E-mail: post@met.no

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 MA  02110-1301, USA
 */

#include "fimex/WdbCDMReader.h"
#include "fimex/CDM.h"
#include <sstream>
#include <set>
#include <functional>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

namespace MetNoFimex
{
class GxWdbCDMReader::Level
{
	std::string levelName_;
	float from_;
	float to_;
public:
	Level(const std::string & levelName, float from, float to) :
		levelName_(levelName), from_(from), to_(to)
	{}

	const std::string & levelName() const { return levelName_; }
	float from() const { return from_; }
	float to() const { return to_; }

	std::string str() const
	{
		std::ostringstream s;
		if ( from() != to() )
			s << from() << " to ";
		s << to() << " " << levelName();
		return s.str();
	}
};

bool operator < (const GxWdbCDMReader::Level & a, const GxWdbCDMReader::Level & b)
{
	if ( a.levelName() != b.levelName() )
		return a.levelName() < b.levelName();
	if ( a.from() != b.from() )
		return a.from() < b.from();
	return a.to() < b.to();
}



GxWdbCDMReader::GxWdbCDMReader(const std::string& source, const std::string& configfilename)
{
//	try
//	{
		connectToDatabase_();
		setupIndex_();

		cdm_->toXMLStream(std::cout);

//	}
//	catch (...)
//	{
//		std::cout << PQerrorMessage(wdbConnection_) << std::endl;
//		PQfinish(wdbConnection_);
//		throw;
//	}
}

GxWdbCDMReader::~GxWdbCDMReader()
{
	closeDatabaseConnection_();
}

boost::shared_ptr<Data> GxWdbCDMReader::getDataSlice(
		const std::string& varName, size_t unLimDimPos) throw (CDMException)
{
	return boost::shared_ptr<Data>();
}


std::ostream & GxWdbCDMReader::indexSummary(std::ostream & s) const
{
	for ( ParameterEntry::const_iterator pe = data_.begin(); pe != data_.end(); ++ pe )
	{
		s << pe->first << '\n';
		for ( LevelEntry::const_iterator le = pe->second.begin(); le != pe->second.end(); ++ le )
		{
			s << " " << le->first.str() << '\n';
			for ( VersionEntry::const_iterator ve = le->second.begin(); ve != le->second.end(); ++ ve )
			{
				s << "  " << ve->first << '\n';
				for ( TimeEntry::const_iterator te = ve->second.begin(); te != ve->second.end(); ++ te )
					s << "   " << te->first << ":\t" << std::showbase << std::hex << te->second << '\n';
			}
		}
	}
	std::flush(s);

	return s;
}


void GxWdbCDMReader::connectToDatabase_()
{
	wdbConnection_ = PQconnectdb("dbname=wdb");
	if ( CONNECTION_OK != PQstatus(wdbConnection_) )
		throw CDMException("Unable to connect to wdb database");

	PQclear(call_("SELECT wci.begin('wdb')"));
}

void GxWdbCDMReader::closeDatabaseConnection_()
{
	if (wdbConnection_)
		PQfinish(wdbConnection_);
}

PGresult * GxWdbCDMReader::call_(const std::string & query)
{
	PGresult * result = PQexec(wdbConnection_, query.c_str());
	if ( PQresultStatus(result) != PGRES_TUPLES_OK )
	{
		PQclear(result);
		throw CDMException(std::string("wdb database error: ") + PQerrorMessage(wdbConnection_));
	}
	return result;
}

namespace
{
enum ReadIdx
{
	ValueParameterName, LevelParameterName, LevelFrom, LevelTo, DataVersion, ValidTimeFrom, ValidTimeTo, Value
};

#define GET(idx) PQgetvalue(indexBase, i, idx)
#define GETFLOAT(idx) boost::lexical_cast<float>(PQgetvalue(indexBase, i, idx))
#define GETTIME(idx) boost::posix_time::from_time_t(boost::lexical_cast<long long>(GET(idx)))
#define GETINT32(idx)boost::lexical_cast<int>(PQgetvalue(indexBase, i, idx))
#define GETINT64(idx)boost::lexical_cast<long long>(PQgetvalue(indexBase, i, idx))
}


void GxWdbCDMReader::setupIndex_()
{
	std::string query = "SELECT "
			"ValueParameterName, "
			"LevelParameterName, LevelFrom, LevelTo, "
			"DataVersion, "
			"extract(epoch from ValidTimeFrom), "
			"extract(epoch from ValidTimeTo), "
			"value"
			" FROM "
			"wci.read('{met.no eceps modification}',NULL, NULL,NULL, NULL,NULL, NULL,NULL::wci.returngid)";

	PGresult * indexBase = call_(query);

	int tuples = PQntuples(indexBase);
	for ( int i = 0; i < tuples; ++ i )
	{
		data_
		[GET(ValueParameterName)]
		 [Level(GET(LevelParameterName), GETFLOAT(LevelFrom), GETFLOAT(LevelTo)) ]
		  [GETINT32(DataVersion)]
		   [GETTIME(ValidTimeTo)] =
				   GETINT64(Value);
	}

	PQclear(indexBase);

	setupCDM_();
}

namespace
{
std::string toCdmName(const std::string & what)
{
	return boost::algorithm::replace_all_copy(what, " ", "_");
}
}

void GxWdbCDMReader::setupCDM_()
{

	typedef std::map<std::string, std::set<std::pair<float, float> > > LevelMap;
	// Collection of all levels in use
	LevelMap levelDimensions;

	// Highest number of dataversions in data set.
	std::size_t maxDataVersionSize = 1;

	// All times that are used in data set.
	std::set<Time> times;

	for ( ParameterEntry::const_iterator pe = data_.begin(); pe != data_.end(); ++ pe )
	{
		const std::string & parameterName = pe->first;
		const LevelEntry & levelEntry = pe->second;

		// collects names of all dimensions a particular varialbe has.
		std::vector<std::string> dimensions;

		for ( LevelEntry::const_iterator le = levelEntry.begin(); le != levelEntry.end(); ++ le )
		{
			const Level & level = le->first;
			const VersionEntry & versionEntry = le->second;


			// Find and register time dimension
			for ( VersionEntry::const_iterator ve = versionEntry.begin(); ve != versionEntry.end(); ++ ve )
			{
				const TimeEntry & timeEntry = ve->second;
				for ( TimeEntry::const_iterator te = timeEntry.begin(); te != timeEntry.end(); ++ te )
					times.insert(te->first);
				if ( timeEntry.size() > 1 and (dimensions.empty() or dimensions.front() != "time") )
					dimensions.push_back("time");
			}

			// Register data versions
			if ( versionEntry.size() > 1 )
			{
				maxDataVersionSize = std::max(maxDataVersionSize, versionEntry.size());
				dimensions.push_back("dataversion");
			}

			// Register levels
			if  (levelEntry.size() > 1 )
			{
				std::string levelName = toCdmName(level.levelName());
				levelDimensions[levelName].insert(std::make_pair(level.from(), level.to()));
				dimensions.push_back(levelName);
			}
		}
		dimensions.push_back("longitude");
		dimensions.push_back("latitude");

		CDMVariable parameter(toCdmName(parameterName), CDM_FLOAT, dimensions);
		cdm_->addVariable(parameter);
//		CDMAttribute attribute()
//		cdm_->addAttribute(parameter.getName(), )
	}

	for ( LevelMap::const_iterator it = levelDimensions.begin(); it != levelDimensions.end(); ++ it )
	{
		if ( it->second.size() > 1 )
		{
			CDMDimension dim(it->first, it->second.size());
			cdm_->addDimension(dim);
		}
	}
	if ( maxDataVersionSize > 1 )
	{
		CDMDimension dataVersion("dataversion", maxDataVersionSize);
		cdm_->addDimension(dataVersion);
	}

	cdm_->addDimension(CDMDimension("longitude", 100));
	cdm_->addDimension(CDMDimension("latitude", 100));

	CDMDimension time("time", times.size());
	time.setUnlimited(true);
	cdm_->addDimension(time);
}


}
