#ifndef CDMREADER_H_
#define CDMREADER_H_

#include <boost/shared_ptr.hpp>
#include "CDM.h"
#include "Data.h"
#include "Time.h"

namespace MetNoUtplukk
{

class CDMReader
{
public:
	CDMReader() {}
	CDMReader(const CDM& cdm) : cdm(cdm) {}
	virtual ~CDMReader() {}

	virtual const CDM& getCDM() const {return cdm;}
	virtual boost::shared_ptr<Data> getDataSlice(const std::string& variableName, const Time& time) const = 0;
	
protected:
	CDM cdm;
};

}

#endif /*CDMREADER_H_*/
