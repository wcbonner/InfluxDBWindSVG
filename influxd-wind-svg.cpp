/////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 William C Bonner
//
//	MIT License
//
//	Permission is hereby granted, free of charge, to any person obtaining a copy
//	of this software and associated documentation files(the "Software"), to deal
//	in the Software without restriction, including without limitation the rights
//	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//	copies of the Software, and to permit persons to whom the Software is
//	furnished to do so, subject to the following conditions :
//
//	The above copyright notice and this permission notice shall be included in all
//	copies or substantial portions of the Software.
//
//	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
//	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//	SOFTWARE.
//
/////////////////////////////////////////////////////////////////////////////

// influxd-wind-svg.cpp : Defines the entry point for the application.
//

// https://www.influxdata.com/blog/getting-started-c-influxdb/
// https://github.com/offa/influxdb-cxx
// https://github.com/libcpr/cpr
// https://thenewstack.io/getting-started-with-c-and-influxdb/
// https://www.w3schools.com/sqL/sql_top.asp Because I've forgotten way too much SQL

#include "influxd-wind-svg.h"
#include "influxdbwindsvg-version.h"

/////////////////////////////////////////////////////////////////////////////
static const std::string ProgramVersionString("InfluxDBWindSVG Version " InfluxDBWindSVG_VERSION " Built on: " __DATE__ " at " __TIME__);
/////////////////////////////////////////////////////////////////////////////
std::string timeToISO8601(const time_t& TheTime, const bool LocalTime = false)
{
	std::ostringstream ISOTime;
	struct tm UTC;
	struct tm* timecallresult(nullptr);
	if (LocalTime)
		timecallresult = localtime_r(&TheTime, &UTC);
	else
		timecallresult = gmtime_r(&TheTime, &UTC);
	if (nullptr != timecallresult)
	{
		ISOTime.fill('0');
		if (!((UTC.tm_year == 70) && (UTC.tm_mon == 0) && (UTC.tm_mday == 1)))
		{
			ISOTime << UTC.tm_year + 1900 << "-";
			ISOTime.width(2);
			ISOTime << UTC.tm_mon + 1 << "-";
			ISOTime.width(2);
			ISOTime << UTC.tm_mday << "T";
		}
		ISOTime.width(2);
		ISOTime << UTC.tm_hour << ":";
		ISOTime.width(2);
		ISOTime << UTC.tm_min << ":";
		ISOTime.width(2);
		ISOTime << UTC.tm_sec;
	}
	return(ISOTime.str());
}
std::string getTimeISO8601(const bool LocalTime = false)
{
	time_t timer;
	time(&timer);
	std::string isostring(timeToISO8601(timer, LocalTime));
	std::string rval;
	rval.assign(isostring.begin(), isostring.end());
	return(rval);
}
time_t ISO8601totime(const std::string& ISOTime)
{
	time_t timer(0);
	if (ISOTime.length() >= 19)
	{
		struct tm UTC;
		UTC.tm_year = stoi(ISOTime.substr(0, 4)) - 1900;
		UTC.tm_mon = stoi(ISOTime.substr(5, 2)) - 1;
		UTC.tm_mday = stoi(ISOTime.substr(8, 2));
		UTC.tm_hour = stoi(ISOTime.substr(11, 2));
		UTC.tm_min = stoi(ISOTime.substr(14, 2));
		UTC.tm_sec = stoi(ISOTime.substr(17, 2));
		UTC.tm_gmtoff = 0;
		UTC.tm_isdst = -1;
		UTC.tm_zone = 0;
#ifdef _MSC_VER
		_tzset();
		_get_daylight(&(UTC.tm_isdst));
#endif
#ifdef __USE_MISC
		timer = timegm(&UTC);
		if (timer == -1)
			return(0);	// if timegm() returned an error value, leave time set at epoch
#else
		timer = mktime(&UTC);
		if (timer == -1)
			return(0);	// if mktime() returned an error value, leave time set at epoch
		timer -= timezone; // HACK: Works in my initial testing on the raspberry pi, but it's currently not DST
#endif
#ifdef _MSC_VER
		long Timezone_seconds = 0;
		_get_timezone(&Timezone_seconds);
		timer -= Timezone_seconds;
		int DST_hours = 0;
		_get_daylight(&DST_hours);
		long DST_seconds = 0;
		_get_dstbias(&DST_seconds);
		timer += DST_hours * DST_seconds;
#endif
	}
	return(timer);
}
// Microsoft Excel doesn't recognize ISO8601 format dates with the "T" seperating the date and time
// This function puts a space where the T goes for ISO8601. The dates can be decoded with ISO8601totime()
std::string timeToExcelDate(const time_t& TheTime, const bool LocalTime = false) { std::string ExcelDate(timeToISO8601(TheTime, LocalTime)); ExcelDate.replace(10, 1, " "); return(ExcelDate); }
std::string timeToExcelLocal(const time_t& TheTime) { return(timeToExcelDate(TheTime, true)); }
/////////////////////////////////////////////////////////////////////////////
bool ValidateDirectory(const std::filesystem::path& DirectoryName)
{
	bool rval = false;
	// https://linux.die.net/man/2/stat
	struct stat64 StatBuffer;
	if (0 == stat64(DirectoryName.c_str(), &StatBuffer))
		if (S_ISDIR(StatBuffer.st_mode))
		{
			// https://linux.die.net/man/2/access
			if (0 == access(DirectoryName.c_str(), R_OK | W_OK))
				rval = true;
			else
			{
				switch (errno)
				{
				case EACCES:
					std::cerr << DirectoryName << " (" << errno << ") The requested access would be denied to the file, or search permission is denied for one of the directories in the path prefix of pathname." << std::endl;
					break;
				case ELOOP:
					std::cerr << DirectoryName << " (" << errno << ") Too many symbolic links were encountered in resolving pathname." << std::endl;
					break;
				case ENAMETOOLONG:
					std::cerr << DirectoryName << " (" << errno << ") pathname is too long." << std::endl;
					break;
				case ENOENT:
					std::cerr << DirectoryName << " (" << errno << ") A component of pathname does not exist or is a dangling symbolic link." << std::endl;
					break;
				case ENOTDIR:
					std::cerr << DirectoryName << " (" << errno << ") A component used as a directory in pathname is not, in fact, a directory." << std::endl;
					break;
				case EROFS:
					std::cerr << DirectoryName << " (" << errno << ") Write permission was requested for a file on a read-only file system." << std::endl;
					break;
				case EFAULT:
					std::cerr << DirectoryName << " (" << errno << ") pathname points outside your accessible address space." << std::endl;
					break;
				case EINVAL:
					std::cerr << DirectoryName << " (" << errno << ") mode was incorrectly specified." << std::endl;
					break;
				case EIO:
					std::cerr << DirectoryName << " (" << errno << ") An I/O error occurred." << std::endl;
					break;
				case ENOMEM:
					std::cerr << DirectoryName << " (" << errno << ") Insufficient kernel memory was available." << std::endl;
					break;
				case ETXTBSY:
					std::cerr << DirectoryName << " (" << errno << ") Write access was requested to an executable which is being executed." << std::endl;
					break;
				default:
					std::cerr << DirectoryName << " (" << errno << ") An unknown error." << std::endl;
				}
			}
		}
	return(rval);
}
/////////////////////////////////////////////////////////////////////////////
int ConsoleVerbosity(1);
std::filesystem::path SVGDirectory;	// If this remains empty, SVG Files are not created. If it's specified, _day, _week, _month, and _year.svg files are created for each bluetooth address seen.
int SVGMinMax(0); // 0x01 = Draw Temperature and Humiditiy Minimum and Maximum line on daily, 0x02 = on weekly, 0x04 = on monthly, 0x08 = on yearly
std::string InfluxDBHost;
std::string InfluxDBDatabase;
int InfluxDBPort(0);
int InfluxDBQueryLimit(40000);
std::filesystem::path InfluxDBCacheDirectory;	// Because it takes so long to load all the data from the database, I'm going to cache the MRTG version of the data here.
bool bMonitorWind(false);
bool bMonitorPressure(false);
/////////////////////////////////////////////////////////////////////////////
// The following details were taken from https://github.com/oetiker/mrtg
const size_t DAY_COUNT(600);			/* 400 samples is 33.33 hours */
const size_t WEEK_COUNT(600);			/* 400 samples is 8.33 days */
const size_t MONTH_COUNT(600);			/* 400 samples is 33.33 days */
const size_t YEAR_COUNT(2 * 366);		/* 1 sample / day, 366 days, 2 years */
const size_t DAY_SAMPLE(5 * 60);		/* Sample every 5 minutes */
const size_t WEEK_SAMPLE(30 * 60);		/* Sample every 30 minutes */
const size_t MONTH_SAMPLE(2 * 60 * 60);	/* Sample every 2 hours */
const size_t YEAR_SAMPLE(24 * 60 * 60);	/* Sample every 24 hours */
/////////////////////////////////////////////////////////////////////////////
class  Influx_Wind {
public:
	time_t Time;
	Influx_Wind() : Time(0), ApparentWindSpeed(0), ApparentWindSpeedMin(DBL_MAX), ApparentWindSpeedMax(-DBL_MAX), Averages(0) { };
	Influx_Wind(const time_t tim, const double wind)
	{
		Time = tim;
		ApparentWindSpeedMax = ApparentWindSpeedMin = ApparentWindSpeed = wind;
		Averages = 1;
	};
	Influx_Wind(const influxdb::Point& data);
	Influx_Wind(const std::string& data);
	std::string WriteTXT(const char seperator = '\t') const;
	double GetApparentWindSpeed(void) const { return(ApparentWindSpeed); };
	double GetApparentWindSpeedMin(void) const { return(std::min(ApparentWindSpeed, ApparentWindSpeedMin)); };
	double GetApparentWindSpeedMax(void) const { return(std::max(ApparentWindSpeed, ApparentWindSpeedMax)); };
	void SetMinMax(const Influx_Wind& a);
	enum granularity { day, week, month, year };
	void NormalizeTime(granularity type);
	granularity GetTimeGranularity(void) const;
	bool IsValid(void) const { return(Averages > 0); };
	Influx_Wind& operator +=(const Influx_Wind& b);
protected:
	double ApparentWindSpeed;
	double ApparentWindSpeedMin;
	double ApparentWindSpeedMax;
	int Averages;
};
Influx_Wind::Influx_Wind(const influxdb::Point& data)
{
	Time = std::chrono::system_clock::to_time_t(data.getTimestamp());
	std::string value(data.getFields());
	value.erase(0, value.find("=") + 1);
	ApparentWindSpeedMax = ApparentWindSpeedMin = ApparentWindSpeed = std::atof(value.c_str()) * 1.9438445; // data is recorded in m/s and I want it in knots
	Averages = 1;
}
Influx_Wind::Influx_Wind(const std::string& data)
{
	std::istringstream ssValue(data);
	std::string theDay;
	ssValue >> theDay;
	std::string theHour;
	ssValue >> theHour;
	std::string theDate(theDay + " " + theHour);
	Time = ISO8601totime(theDate);
	ssValue >> ApparentWindSpeed;
	ssValue >> ApparentWindSpeedMin;
	ssValue >> ApparentWindSpeedMax;
	ssValue >> Averages;
}
std::string Influx_Wind::WriteTXT(const char seperator) const
{
	std::ostringstream ssValue;
	ssValue << timeToExcelDate(Time);
	ssValue << seperator << ApparentWindSpeed;
	ssValue << seperator << ApparentWindSpeedMin;
	ssValue << seperator << ApparentWindSpeedMax;
	ssValue << seperator << Averages;
	return(ssValue.str());
}
void Influx_Wind::SetMinMax(const Influx_Wind& a)
{
	ApparentWindSpeedMin = ApparentWindSpeedMin < ApparentWindSpeed ? ApparentWindSpeedMin : ApparentWindSpeed;
	ApparentWindSpeedMax = ApparentWindSpeedMax > ApparentWindSpeed ? ApparentWindSpeedMax : ApparentWindSpeed;

	ApparentWindSpeedMin = ApparentWindSpeedMin < a.ApparentWindSpeedMin ? ApparentWindSpeedMin : a.ApparentWindSpeedMin;
	ApparentWindSpeedMax = ApparentWindSpeedMax > a.ApparentWindSpeedMax ? ApparentWindSpeedMax : a.ApparentWindSpeedMax;
}
void Influx_Wind::NormalizeTime(granularity type)
{
	if (type == day)
		Time = (Time / DAY_SAMPLE) * DAY_SAMPLE;
	else if (type == week)
		Time = (Time / WEEK_SAMPLE) * WEEK_SAMPLE;
	else if (type == month)
		Time = (Time / MONTH_SAMPLE) * MONTH_SAMPLE;
	else if (type == year)
	{
		struct tm UTC;
		if (0 != localtime_r(&Time, &UTC))
		{
			UTC.tm_hour = 0;
			UTC.tm_min = 0;
			UTC.tm_sec = 0;
			Time = mktime(&UTC);
		}
	}
}
Influx_Wind::granularity Influx_Wind::GetTimeGranularity(void) const
{
	granularity rval = granularity::day;
	struct tm UTC;
	if (0 != localtime_r(&Time, &UTC))
	{
		//if (((UTC.tm_hour == 0) && (UTC.tm_min == 0)) || ((UTC.tm_hour == 23) && (UTC.tm_min == 0) && (UTC.tm_isdst == 1)))
		if ((UTC.tm_hour == 0) && (UTC.tm_min == 0))
			rval = granularity::year;
		else if ((UTC.tm_hour % 2 == 0) && (UTC.tm_min == 0))
			rval = granularity::month;
		else if ((UTC.tm_min == 0) || (UTC.tm_min == 30))
			rval = granularity::week;
	}
	return(rval);
}
Influx_Wind& Influx_Wind::operator +=(const Influx_Wind& b)
{
	if (b.IsValid())
	{
		Time = std::max(Time, b.Time); // Use the maximum time (newest time)
		ApparentWindSpeed = ((ApparentWindSpeed * Averages) + (b.ApparentWindSpeed * b.Averages)) / (Averages + b.Averages);
		ApparentWindSpeedMin = std::min(std::min(ApparentWindSpeed, ApparentWindSpeedMin), b.ApparentWindSpeedMin);
		ApparentWindSpeedMax = std::max(std::max(ApparentWindSpeed, ApparentWindSpeedMax), b.ApparentWindSpeedMax);
		Averages += b.Averages; // existing average + new average
	}
	return(*this);
}
/////////////////////////////////////////////////////////////////////////////
enum class GraphType { daily, weekly, monthly, yearly };
void WriteSVG(std::vector<Influx_Wind>& TheValues, const std::filesystem::path& SVGFileName, const std::string& Title = "", const GraphType graph = GraphType::daily, const bool MinMax = false)
{
	// By declaring these items here, I'm then basing all my other dimensions on these
	const int SVGWidth(500);
	const int SVGHeight(135);
	const int FontSize(12);
	const int TickSize(2);
	int GraphWidth = SVGWidth - (FontSize * 5);
	if (!TheValues.empty())
	{
		struct stat64 SVGStat(0);
		if (-1 == stat64(SVGFileName.c_str(), &SVGStat))
			if (ConsoleVerbosity > 0)
				std::cout << "[" << getTimeISO8601(true) << "] " << std::strerror(errno) << ": " << SVGFileName << std::endl;
		if (TheValues.begin()->Time > SVGStat.st_mtim.tv_sec)	// only write the file if we have new data
		{
			std::ofstream SVGFile(SVGFileName);
			if (SVGFile.is_open())
			{
				if (ConsoleVerbosity > 0)
					std::cout << "[" << getTimeISO8601(true) << "] Writing: " << SVGFileName.string() << " With Title: " << Title << std::endl;
				else
					std::cerr << "Writing: " << SVGFileName.string() << " With Title: " << Title << std::endl;
				std::ostringstream tempOString;
				tempOString << "Wind Speed (" << std::fixed << std::setprecision(1) << TheValues[0].GetApparentWindSpeed() << " kn)";
				std::string YLegendWindSpeed(tempOString.str());
				int GraphTop = FontSize + TickSize;
				int GraphBottom = SVGHeight - GraphTop;
				int GraphRight = SVGWidth - GraphTop;
				int GraphLeft = GraphRight - GraphWidth;
				int GraphVerticalDivision = (GraphBottom - GraphTop) / 4;
				double TempMin = DBL_MAX;
				double TempMax = -DBL_MAX;
				if (MinMax)
					for (auto index = 0; index < (GraphWidth < TheValues.size() ? GraphWidth : TheValues.size()); index++)
					{
						TempMin = std::min(TempMin, TheValues[index].GetApparentWindSpeedMin());
						TempMax = std::max(TempMax, TheValues[index].GetApparentWindSpeedMax());
					}
				else
					for (auto index = 0; index < (GraphWidth < TheValues.size() ? GraphWidth : TheValues.size()); index++)
					{
						TempMin = std::min(TempMin, TheValues[index].GetApparentWindSpeed());
						TempMax = std::max(TempMax, TheValues[index].GetApparentWindSpeed());
					}

				double TempVerticalDivision = (TempMax - TempMin) / 4;
				double TempVerticalFactor = (GraphBottom - GraphTop) / (TempMax - TempMin);

				SVGFile << "<?xml version=\"1.0\" encoding=\"utf-8\" standalone=\"no\"?>" << std::endl;
				SVGFile << "<svg xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" width=\"" << SVGWidth << "\" height=\"" << SVGHeight << "\">" << std::endl;
				SVGFile << "\t<!-- Created by: " << ProgramVersionString << " -->" << std::endl;
				SVGFile << "\t<style>" << std::endl;
				SVGFile << "\t\ttext { font-family: sans-serif; font-size: " << FontSize << "px; fill: black; }" << std::endl;
				SVGFile << "\t\tline { stroke: black; }" << std::endl;
				SVGFile << "\t\tpolygon { fill-opacity: 0.5; }" << std::endl;
#ifdef _DARK_STYLE_
				SVGFile << "\t@media only screen and (prefers-color-scheme: dark) {" << std::endl;
				SVGFile << "\t\ttext { fill: grey; }" << std::endl;
				SVGFile << "\t\tline { stroke: grey; }" << std::endl;
				SVGFile << "\t}" << std::endl;
#endif // _DARK_STYLE_
				SVGFile << "\t</style>" << std::endl;
				SVGFile << "\t<rect style=\"fill-opacity:0;stroke:grey;stroke-width:2\" width=\"" << SVGWidth << "\" height=\"" << SVGHeight << "\" />" << std::endl;

				// Legend Text
				int LegendIndex = 1;
				SVGFile << "\t<text x=\"" << GraphLeft << "\" y=\"" << GraphTop - 2 << "\">" << Title << "</text>" << std::endl;
				SVGFile << "\t<text style=\"text-anchor:end\" x=\"" << GraphRight << "\" y=\"" << GraphTop - 2 << "\">" << timeToExcelLocal(TheValues[0].Time) << "</text>" << std::endl;
				SVGFile << "\t<text style=\"fill:blue;text-anchor:middle\" x=\"" << FontSize * LegendIndex << "\" y=\"" << (GraphTop + GraphBottom) / 2 << "\" transform=\"rotate(270 " << FontSize * LegendIndex << "," << (GraphTop + GraphBottom) / 2 << ")\">" << YLegendWindSpeed << "</text>" << std::endl;

				// Top Line
				SVGFile << "\t<line x1=\"" << GraphLeft - TickSize << "\" y1=\"" << GraphTop << "\" x2=\"" << GraphRight + TickSize << "\" y2=\"" << GraphTop << "\"/>" << std::endl;
				SVGFile << "\t<text style=\"fill:blue;text-anchor:end\" x=\"" << GraphLeft - TickSize << "\" y=\"" << GraphTop + 5 << "\">" << std::fixed << std::setprecision(1) << TempMax << "</text>" << std::endl;

				// Bottom Line
				SVGFile << "\t<line x1=\"" << GraphLeft - TickSize << "\" y1=\"" << GraphBottom << "\" x2=\"" << GraphRight + TickSize << "\" y2=\"" << GraphBottom << "\"/>" << std::endl;
				SVGFile << "\t<text style=\"fill:blue;text-anchor:end\" x=\"" << GraphLeft - TickSize << "\" y=\"" << GraphBottom + 5 << "\">" << std::fixed << std::setprecision(1) << TempMin << "</text>" << std::endl;

				// Left Line
				SVGFile << "\t<line x1=\"" << GraphLeft << "\" y1=\"" << GraphTop << "\" x2=\"" << GraphLeft << "\" y2=\"" << GraphBottom << "\"/>" << std::endl;

				// Right Line
				SVGFile << "\t<line x1=\"" << GraphRight << "\" y1=\"" << GraphTop << "\" x2=\"" << GraphRight << "\" y2=\"" << GraphBottom << "\"/>" << std::endl;

				// Vertical Division Dashed Lines
				for (auto index = 1; index < 4; index++)
				{
					SVGFile << "\t<line style=\"stroke-dasharray:1\" x1=\"" << GraphLeft - TickSize << "\" y1=\"" << GraphTop + (GraphVerticalDivision * index) << "\" x2=\"" << GraphRight + TickSize << "\" y2=\"" << GraphTop + (GraphVerticalDivision * index) << "\" />" << std::endl;
					SVGFile << "\t<text style=\"fill:blue;text-anchor:end\" x=\"" << GraphLeft - TickSize << "\" y=\"" << GraphTop + 4 + (GraphVerticalDivision * index) << "\">" << std::fixed << std::setprecision(1) << TempMax - (TempVerticalDivision * index) << "</text>" << std::endl;
				}

				// Horizontal Division Dashed Lines
				for (auto index = 0; index < (GraphWidth < TheValues.size() ? GraphWidth : TheValues.size()); index++)
				{
					struct tm UTC;
					if (0 != localtime_r(&TheValues[index].Time, &UTC))
					{
						if (graph == GraphType::daily)
						{
							if (UTC.tm_min == 0)
							{
								if (UTC.tm_hour == 0)
									SVGFile << "\t<line style=\"stroke:red\" x1=\"" << GraphLeft + index << "\" y1=\"" << GraphTop << "\" x2=\"" << GraphLeft + index << "\" y2=\"" << GraphBottom + TickSize << "\" />" << std::endl;
								else
									SVGFile << "\t<line style=\"stroke-dasharray:1\" x1=\"" << GraphLeft + index << "\" y1=\"" << GraphTop << "\" x2=\"" << GraphLeft + index << "\" y2=\"" << GraphBottom + TickSize << "\" />" << std::endl;
								if (UTC.tm_hour % 2 == 0)
									SVGFile << "\t<text style=\"text-anchor:middle\" x=\"" << GraphLeft + index << "\" y=\"" << SVGHeight - 2 << "\">" << UTC.tm_hour << "</text>" << std::endl;
							}
						}
						else if (graph == GraphType::weekly)
						{
							const std::string Weekday[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
							if ((UTC.tm_hour == 0) && (UTC.tm_min == 0))
							{
								if (UTC.tm_wday == 0)
									SVGFile << "\t<line style=\"stroke:red\" x1=\"" << GraphLeft + index << "\" y1=\"" << GraphTop << "\" x2=\"" << GraphLeft + index << "\" y2=\"" << GraphBottom + TickSize << "\" />" << std::endl;
								else
									SVGFile << "\t<line style=\"stroke-dasharray:1\" x1=\"" << GraphLeft + index << "\" y1=\"" << GraphTop << "\" x2=\"" << GraphLeft + index << "\" y2=\"" << GraphBottom + TickSize << "\" />" << std::endl;
							}
							else if ((UTC.tm_hour == 12) && (UTC.tm_min == 0))
								SVGFile << "\t<text style=\"text-anchor:middle\" x=\"" << GraphLeft + index << "\" y=\"" << SVGHeight - 2 << "\">" << Weekday[UTC.tm_wday] << "</text>" << std::endl;
						}
						else if (graph == GraphType::monthly)
						{
							if ((UTC.tm_mday == 1) && (UTC.tm_hour == 0) && (UTC.tm_min == 0))
								SVGFile << "\t<line style=\"stroke:red\" x1=\"" << GraphLeft + index << "\" y1=\"" << GraphTop << "\" x2=\"" << GraphLeft + index << "\" y2=\"" << GraphBottom + TickSize << "\" />" << std::endl;
							if ((UTC.tm_wday == 0) && (UTC.tm_hour == 0) && (UTC.tm_min == 0))
								SVGFile << "\t<line style=\"stroke-dasharray:1\" x1=\"" << GraphLeft + index << "\" y1=\"" << GraphTop << "\" x2=\"" << GraphLeft + index << "\" y2=\"" << GraphBottom + TickSize << "\" />" << std::endl;
							else if ((UTC.tm_wday == 3) && (UTC.tm_hour == 12) && (UTC.tm_min == 0))
								SVGFile << "\t<text style=\"text-anchor:middle\" x=\"" << GraphLeft + index << "\" y=\"" << SVGHeight - 2 << "\">Week " << UTC.tm_yday / 7 + 1 << "</text>" << std::endl;
						}
						else if (graph == GraphType::yearly)
						{
							const std::string Month[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
							if ((UTC.tm_yday == 0) && (UTC.tm_mday == 1) && (UTC.tm_hour == 0) && (UTC.tm_min == 0))
								SVGFile << "\t<line style=\"stroke:red\" x1=\"" << GraphLeft + index << "\" y1=\"" << GraphTop << "\" x2=\"" << GraphLeft + index << "\" y2=\"" << GraphBottom + TickSize << "\" />" << std::endl;
							else if ((UTC.tm_mday == 1) && (UTC.tm_hour == 0) && (UTC.tm_min == 0))
								SVGFile << "\t<line style=\"stroke-dasharray:1\" x1=\"" << GraphLeft + index << "\" y1=\"" << GraphTop << "\" x2=\"" << GraphLeft + index << "\" y2=\"" << GraphBottom + TickSize << "\" />" << std::endl;
							else if ((UTC.tm_mday == 15) && (UTC.tm_hour == 0) && (UTC.tm_min == 0))
								SVGFile << "\t<text style=\"text-anchor:middle\" x=\"" << GraphLeft + index << "\" y=\"" << SVGHeight - 2 << "\">" << Month[UTC.tm_mon] << "</text>" << std::endl;
						}
					}
				}

				// Directional Arrow
				SVGFile << "\t<polygon style=\"fill:red;stroke:red;fill-opacity:1;\" points=\"" << GraphLeft - 3 << "," << GraphBottom << " " << GraphLeft + 3 << "," << GraphBottom - 3 << " " << GraphLeft + 3 << "," << GraphBottom + 3 << "\" />" << std::endl;

				if (MinMax)
				{
					// ApparentWindSpeed Values as a filled polygon showing the minimum and maximum
					SVGFile << "\t<!-- ApparentWindSpeed MinMax -->" << std::endl;
					SVGFile << "\t<polygon style=\"fill:blue;stroke:blue\" points=\"";
					for (auto index = 1; index < (GraphWidth < TheValues.size() ? GraphWidth : TheValues.size()); index++)
						SVGFile << index + GraphLeft << "," << int(((TempMax - TheValues[index].GetApparentWindSpeedMax()) * TempVerticalFactor) + GraphTop) << " ";
					for (auto index = (GraphWidth < TheValues.size() ? GraphWidth : TheValues.size()) - 1; index > 0; index--)
						SVGFile << index + GraphLeft << "," << int(((TempMax - TheValues[index].GetApparentWindSpeedMin()) * TempVerticalFactor) + GraphTop) << " ";
					SVGFile << "\" />" << std::endl;
				}
				// always draw this line over the top of the MinMax
				// ApparentWindSpeed Values as a continuous line
				SVGFile << "\t<!-- ApparentWindSpeed -->" << std::endl;
				SVGFile << "\t<polyline style=\"fill:none;stroke:blue\" points=\"";
				for (auto index = 1; index < (GraphWidth < TheValues.size() ? GraphWidth : TheValues.size()); index++)
					SVGFile << index + GraphLeft << "," << int(((TempMax - TheValues[index].GetApparentWindSpeed()) * TempVerticalFactor) + GraphTop) << " ";
				SVGFile << "\" />" << std::endl;

				SVGFile << "</svg>" << std::endl;
				SVGFile.close();
				struct utimbuf SVGut({ TheValues.begin()->Time, TheValues.begin()->Time });
				utime(SVGFileName.c_str(), &SVGut);
			}
		}
	}
}
/////////////////////////////////////////////////////////////////////////////
std::vector<Influx_Wind> InfluxMRTGWind; // vector structure similar to MRTG Log File
// Takes current datapoint and updates the mapped structure in memory simulating the contents of a MRTG log file.
template <class T> void UpdateMRTGData(std::vector<T>& FakeMRTGFile, T& TheValue)
{
	if (FakeMRTGFile.size() != 2 + DAY_COUNT + WEEK_COUNT + MONTH_COUNT + YEAR_COUNT)
	{
		FakeMRTGFile.resize(2 + DAY_COUNT + WEEK_COUNT + MONTH_COUNT + YEAR_COUNT);
		FakeMRTGFile[0] = TheValue;	// current value
		FakeMRTGFile[1] = TheValue;
		for (auto index = 0; index < DAY_COUNT; index++)
			FakeMRTGFile[index + 2].Time = FakeMRTGFile[index + 1].Time - DAY_SAMPLE;
		for (auto index = 0; index < WEEK_COUNT; index++)
			FakeMRTGFile[index + 2 + DAY_COUNT].Time = FakeMRTGFile[index + 1 + DAY_COUNT].Time - WEEK_SAMPLE;
		for (auto index = 0; index < MONTH_COUNT; index++)
			FakeMRTGFile[index + 2 + DAY_COUNT + WEEK_COUNT].Time = FakeMRTGFile[index + 1 + DAY_COUNT + WEEK_COUNT].Time - MONTH_SAMPLE;
		for (auto index = 0; index < YEAR_COUNT; index++)
			FakeMRTGFile[index + 2 + DAY_COUNT + WEEK_COUNT + MONTH_COUNT].Time = FakeMRTGFile[index + 1 + DAY_COUNT + WEEK_COUNT + MONTH_COUNT].Time - YEAR_SAMPLE;
	}
	else
	{
		if (TheValue.Time > FakeMRTGFile[0].Time)
		{
			FakeMRTGFile[0] = TheValue;	// current value
			FakeMRTGFile[1] += TheValue; // averaged value up to DAY_SAMPLE size
		}
	}
	bool ZeroAccumulator = false;
	auto DaySampleFirst = FakeMRTGFile.begin() + 2;
	auto DaySampleLast = FakeMRTGFile.begin() + 1 + DAY_COUNT;
	auto WeekSampleFirst = FakeMRTGFile.begin() + 2 + DAY_COUNT;
	auto WeekSampleLast = FakeMRTGFile.begin() + 1 + DAY_COUNT + WEEK_COUNT;
	auto MonthSampleFirst = FakeMRTGFile.begin() + 2 + DAY_COUNT + WEEK_COUNT;
	auto MonthSampleLast = FakeMRTGFile.begin() + 1 + DAY_COUNT + WEEK_COUNT + MONTH_COUNT;
	auto YearSampleFirst = FakeMRTGFile.begin() + 2 + DAY_COUNT + WEEK_COUNT + MONTH_COUNT;
	auto YearSampleLast = FakeMRTGFile.begin() + 1 + DAY_COUNT + WEEK_COUNT + MONTH_COUNT + YEAR_COUNT;
	// For every time difference between FakeMRTGFile[1] and FakeMRTGFile[2] that's greater than DAY_SAMPLE we shift that data towards the back.
	while (difftime(FakeMRTGFile[1].Time, DaySampleFirst->Time) > DAY_SAMPLE)
	{
		ZeroAccumulator = true;
		// shuffle all the day samples toward the end
		std::copy_backward(DaySampleFirst, DaySampleLast - 1, DaySampleLast);
		*DaySampleFirst = FakeMRTGFile[1];
		DaySampleFirst->NormalizeTime(T::granularity::day);
		if (difftime(DaySampleFirst->Time, (DaySampleFirst + 1)->Time) > DAY_SAMPLE)
			DaySampleFirst->Time = (DaySampleFirst + 1)->Time + DAY_SAMPLE;
		if (DaySampleFirst->GetTimeGranularity() == T::granularity::year)
		{
			if (ConsoleVerbosity > 2)
				std::cout << "[" << getTimeISO8601(true) << "] shuffling year " << timeToExcelLocal(DaySampleFirst->Time) << " > " << timeToExcelLocal(YearSampleFirst->Time) << std::endl;
			// shuffle all the year samples toward the end
			std::copy_backward(YearSampleFirst, YearSampleLast - 1, YearSampleLast);
			*YearSampleFirst = T();
			for (auto iter = DaySampleFirst; (iter->IsValid() && ((iter - DaySampleFirst) < (12 * 24))); iter++) // One Day of day samples
				*YearSampleFirst += *iter;
		}
		if ((DaySampleFirst->GetTimeGranularity() == T::granularity::year) ||
			(DaySampleFirst->GetTimeGranularity() == T::granularity::month))
		{
			if (ConsoleVerbosity > 2)
				std::cout << "[" << getTimeISO8601(true) << "] shuffling month " << timeToExcelLocal(DaySampleFirst->Time) << std::endl;
			// shuffle all the month samples toward the end
			std::copy_backward(MonthSampleFirst, MonthSampleLast - 1, MonthSampleLast);
			*MonthSampleFirst = T();
			for (auto iter = DaySampleFirst; (iter->IsValid() && ((iter - DaySampleFirst) < (12 * 2))); iter++) // two hours of day samples
				*MonthSampleFirst += *iter;
		}
		if ((DaySampleFirst->GetTimeGranularity() == T::granularity::year) ||
			(DaySampleFirst->GetTimeGranularity() == T::granularity::month) ||
			(DaySampleFirst->GetTimeGranularity() == T::granularity::week))
		{
			if (ConsoleVerbosity > 2)
				std::cout << "[" << getTimeISO8601(true) << "] shuffling week " << timeToExcelLocal(DaySampleFirst->Time) << std::endl;
			// shuffle all the month samples toward the end
			std::copy_backward(WeekSampleFirst, WeekSampleLast - 1, WeekSampleLast);
			*WeekSampleFirst = T();
			for (auto iter = DaySampleFirst; (iter->IsValid() && ((iter - DaySampleFirst) < 6)); iter++) // Half an hour of day samples
				*WeekSampleFirst += *iter;
		}
	}
	if (ZeroAccumulator)
		FakeMRTGFile[1] = T();
}
// Returns a curated vector of data points specific to the requested graph type from the internal memory structure
template <class T> void ReadMRTGData(const std::vector<T>& FakeMRTGFile, std::vector<T>& TheValues, const GraphType graph = GraphType::daily)
{
	if (FakeMRTGFile.size() == 2 + DAY_COUNT + WEEK_COUNT + MONTH_COUNT + YEAR_COUNT)
	{
		auto DaySampleFirst = FakeMRTGFile.begin() + 2;
		auto DaySampleLast = FakeMRTGFile.begin() + 1 + DAY_COUNT;
		auto WeekSampleFirst = FakeMRTGFile.begin() + 2 + DAY_COUNT;
		auto WeekSampleLast = FakeMRTGFile.begin() + 1 + DAY_COUNT + WEEK_COUNT;
		auto MonthSampleFirst = FakeMRTGFile.begin() + 2 + DAY_COUNT + WEEK_COUNT;
		auto MonthSampleLast = FakeMRTGFile.begin() + 1 + DAY_COUNT + WEEK_COUNT + MONTH_COUNT;
		auto YearSampleFirst = FakeMRTGFile.begin() + 2 + DAY_COUNT + WEEK_COUNT + MONTH_COUNT;
		auto YearSampleLast = FakeMRTGFile.begin() + 1 + DAY_COUNT + WEEK_COUNT + MONTH_COUNT + YEAR_COUNT;
		if (graph == GraphType::daily)
		{
			TheValues.resize(DAY_COUNT);
			std::copy(DaySampleFirst, DaySampleLast, TheValues.begin());
			auto iter = TheValues.begin();
			while (iter->IsValid() && (iter != TheValues.end()))
				iter++;
			TheValues.resize(iter - TheValues.begin());
			TheValues.begin()->Time = FakeMRTGFile.begin()->Time; //HACK: include the most recent time sample
		}
		else if (graph == GraphType::weekly)
		{
			TheValues.resize(WEEK_COUNT);
			std::copy(WeekSampleFirst, WeekSampleLast, TheValues.begin());
			auto iter = TheValues.begin();
			while (iter->IsValid() && (iter != TheValues.end()))
				iter++;
			TheValues.resize(iter - TheValues.begin());
		}
		else if (graph == GraphType::monthly)
		{
			TheValues.resize(MONTH_COUNT);
			std::copy(MonthSampleFirst, MonthSampleLast, TheValues.begin());
			auto iter = TheValues.begin();
			while (iter->IsValid() && (iter != TheValues.end()))
				iter++;
			TheValues.resize(iter - TheValues.begin());
		}
		else if (graph == GraphType::yearly)
		{
			TheValues.resize(YEAR_COUNT);
			std::copy(YearSampleFirst, YearSampleLast, TheValues.begin());
			auto iter = TheValues.begin();
			while (iter->IsValid() && (iter != TheValues.end()))
				iter++;
			TheValues.resize(iter - TheValues.begin());
		}
	}
}
/////////////////////////////////////////////////////////////////////////////
class  Influx_Pressure {
public:
	time_t Time;
	Influx_Pressure() : Time(0), OutsidePressure(0), OutsidePressureMin(DBL_MAX), OutsidePressureMax(-DBL_MAX), Averages(0) { };
	Influx_Pressure(const time_t tim, const double pressure)
	{
		Time = tim;
		OutsidePressureMax = OutsidePressureMin = OutsidePressure = pressure;
		Averages = 1;
	};
	Influx_Pressure(const influxdb::Point& data);
	Influx_Pressure(const std::string& data);
	std::string WriteTXT(const char seperator = '\t') const;
	double GetOutsidePressure(void) const { return(OutsidePressure); };
	double GetOutsidePressureMin(void) const { return(std::min(OutsidePressure, OutsidePressureMin)); };
	double GetOutsidePressureMax(void) const { return(std::max(OutsidePressure, OutsidePressureMax)); };
	void SetMinMax(const Influx_Pressure& a);
	enum granularity { day, week, month, year };
	void NormalizeTime(granularity type);
	granularity GetTimeGranularity(void) const;
	bool IsValid(void) const { return(Averages > 0); };
	Influx_Pressure& operator +=(const Influx_Pressure& b);
protected:
	double OutsidePressure;
	double OutsidePressureMin;
	double OutsidePressureMax;
	int Averages;
};
Influx_Pressure::Influx_Pressure(const influxdb::Point& data)
{
	Time = std::chrono::system_clock::to_time_t(data.getTimestamp());
	std::string value(data.getFields());
	value.erase(0, value.find("=") + 1);
	OutsidePressureMax = OutsidePressureMin = OutsidePressure = std::atof(value.c_str()) / 100;
	Averages = 1;
}
Influx_Pressure::Influx_Pressure(const std::string& data)
{
	std::istringstream ssValue(data);
	std::string theDay;
	ssValue >> theDay;
	std::string theHour;
	ssValue >> theHour;
	std::string theDate(theDay + " " + theHour);
	Time = ISO8601totime(theDate);
	ssValue >> OutsidePressure;
	ssValue >> OutsidePressureMin;
	ssValue >> OutsidePressureMax;
	ssValue >> Averages;
}
std::string Influx_Pressure::WriteTXT(const char seperator) const
{
	std::ostringstream ssValue;
	ssValue << timeToExcelDate(Time);
	ssValue << seperator << OutsidePressure;
	ssValue << seperator << OutsidePressureMin;
	ssValue << seperator << OutsidePressureMax;
	ssValue << seperator << Averages;
	return(ssValue.str());
}
void Influx_Pressure::SetMinMax(const Influx_Pressure& a)
{
	OutsidePressureMin = OutsidePressureMin < OutsidePressure ? OutsidePressureMin : OutsidePressure;
	OutsidePressureMax = OutsidePressureMax > OutsidePressure ? OutsidePressureMax : OutsidePressure;

	OutsidePressureMin = OutsidePressureMin < a.OutsidePressureMin ? OutsidePressureMin : a.OutsidePressureMin;
	OutsidePressureMax = OutsidePressureMax > a.OutsidePressureMax ? OutsidePressureMax : a.OutsidePressureMax;
}
void Influx_Pressure::NormalizeTime(granularity type)
{
	if (type == day)
		Time = (Time / DAY_SAMPLE) * DAY_SAMPLE;
	else if (type == week)
		Time = (Time / WEEK_SAMPLE) * WEEK_SAMPLE;
	else if (type == month)
		Time = (Time / MONTH_SAMPLE) * MONTH_SAMPLE;
	else if (type == year)
	{
		struct tm UTC;
		if (0 != localtime_r(&Time, &UTC))
		{
			UTC.tm_hour = 0;
			UTC.tm_min = 0;
			UTC.tm_sec = 0;
			Time = mktime(&UTC);
		}
	}
}
Influx_Pressure::granularity Influx_Pressure::GetTimeGranularity(void) const
{
	granularity rval = granularity::day;
	struct tm UTC;
	if (0 != localtime_r(&Time, &UTC))
	{
		//if (((UTC.tm_hour == 0) && (UTC.tm_min == 0)) || ((UTC.tm_hour == 23) && (UTC.tm_min == 0) && (UTC.tm_isdst == 1)))
		if ((UTC.tm_hour == 0) && (UTC.tm_min == 0))
			rval = granularity::year;
		else if ((UTC.tm_hour % 2 == 0) && (UTC.tm_min == 0))
			rval = granularity::month;
		else if ((UTC.tm_min == 0) || (UTC.tm_min == 30))
			rval = granularity::week;
	}
	return(rval);
}
Influx_Pressure& Influx_Pressure::operator +=(const Influx_Pressure& b)
{
	if (b.IsValid())
	{
		Time = std::max(Time, b.Time); // Use the maximum time (newest time)
		OutsidePressure = ((OutsidePressure * Averages) + (b.OutsidePressure * b.Averages)) / (Averages + b.Averages);
		OutsidePressureMin = std::min(std::min(OutsidePressure, OutsidePressureMin), b.OutsidePressureMin);
		OutsidePressureMax = std::max(std::max(OutsidePressure, OutsidePressureMax), b.OutsidePressureMax);
		Averages += b.Averages; // existing average + new average
	}
	return(*this);
}
/////////////////////////////////////////////////////////////////////////////
void WriteSVG(std::vector<Influx_Pressure>& TheValues, const std::filesystem::path& SVGFileName, const std::string& Title = "", const GraphType graph = GraphType::daily, const bool MinMax = false)
{
	// By declaring these items here, I'm then basing all my other dimensions on these
	const int SVGWidth(500);
	const int SVGHeight(135);
	const int FontSize(12);
	const int TickSize(2);
	int GraphWidth = SVGWidth - (FontSize * 7);
	if (!TheValues.empty())
	{
		struct stat64 SVGStat(0);
		if (-1 == stat64(SVGFileName.c_str(), &SVGStat))
			if (ConsoleVerbosity > 0)
				std::cout << "[" << getTimeISO8601(true) << "] " << std::strerror(errno) << ": " << SVGFileName << std::endl;
		if (TheValues.begin()->Time > SVGStat.st_mtim.tv_sec)	// only write the file if we have new data
		{
			std::ofstream SVGFile(SVGFileName);
			if (SVGFile.is_open())
			{
				if (ConsoleVerbosity > 0)
					std::cout << "[" << getTimeISO8601(true) << "] Writing: " << SVGFileName.string() << " With Title: " << Title << std::endl;
				else
					std::cerr << "Writing: " << SVGFileName.string() << " With Title: " << Title << std::endl;
				std::ostringstream tempOString;
				tempOString << "Pressure (" << std::fixed << std::setprecision(1) << TheValues[0].GetOutsidePressure() << " hPa)";
				std::string YLegendWindSpeed(tempOString.str());
				int GraphTop = FontSize + TickSize;
				int GraphBottom = SVGHeight - GraphTop;
				int GraphRight = SVGWidth - GraphTop;
				int GraphLeft = GraphRight - GraphWidth;
				int GraphVerticalDivision = (GraphBottom - GraphTop) / 4;
				double TempMin = DBL_MAX;
				double TempMax = -DBL_MAX;
				if (MinMax)
					for (auto index = 0; index < (GraphWidth < TheValues.size() ? GraphWidth : TheValues.size()); index++)
					{
						TempMin = std::min(TempMin, TheValues[index].GetOutsidePressureMin());
						TempMax = std::max(TempMax, TheValues[index].GetOutsidePressureMax());
					}
				else
					for (auto index = 0; index < (GraphWidth < TheValues.size() ? GraphWidth : TheValues.size()); index++)
					{
						TempMin = std::min(TempMin, TheValues[index].GetOutsidePressure());
						TempMax = std::max(TempMax, TheValues[index].GetOutsidePressure());
					}

				double TempVerticalDivision = (TempMax - TempMin) / 4;
				double TempVerticalFactor = (GraphBottom - GraphTop) / (TempMax - TempMin);

				SVGFile << "<?xml version=\"1.0\" encoding=\"utf-8\" standalone=\"no\"?>" << std::endl;
				SVGFile << "<svg xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" width=\"" << SVGWidth << "\" height=\"" << SVGHeight << "\">" << std::endl;
				SVGFile << "\t<!-- Created by: " << ProgramVersionString << " -->" << std::endl;
				SVGFile << "\t<style>" << std::endl;
				SVGFile << "\t\ttext { font-family: sans-serif; font-size: " << FontSize << "px; fill: black; }" << std::endl;
				SVGFile << "\t\tline { stroke: black; }" << std::endl;
				SVGFile << "\t\tpolygon { fill-opacity: 0.5; }" << std::endl;
#ifdef _DARK_STYLE_
				SVGFile << "\t@media only screen and (prefers-color-scheme: dark) {" << std::endl;
				SVGFile << "\t\ttext { fill: grey; }" << std::endl;
				SVGFile << "\t\tline { stroke: grey; }" << std::endl;
				SVGFile << "\t}" << std::endl;
#endif // _DARK_STYLE_
				SVGFile << "\t</style>" << std::endl;
				SVGFile << "\t<rect style=\"fill-opacity:0;stroke:grey;stroke-width:2\" width=\"" << SVGWidth << "\" height=\"" << SVGHeight << "\" />" << std::endl;

				// Legend Text
				int LegendIndex = 1;
				SVGFile << "\t<text x=\"" << GraphLeft << "\" y=\"" << GraphTop - 2 << "\">" << Title << "</text>" << std::endl;
				SVGFile << "\t<text style=\"text-anchor:end\" x=\"" << GraphRight << "\" y=\"" << GraphTop - 2 << "\">" << timeToExcelLocal(TheValues[0].Time) << "</text>" << std::endl;
				SVGFile << "\t<text style=\"fill:blue;text-anchor:middle\" x=\"" << FontSize * LegendIndex << "\" y=\"" << (GraphTop + GraphBottom) / 2 << "\" transform=\"rotate(270 " << FontSize * LegendIndex << "," << (GraphTop + GraphBottom) / 2 << ")\">" << YLegendWindSpeed << "</text>" << std::endl;

				// Top Line
				SVGFile << "\t<line x1=\"" << GraphLeft - TickSize << "\" y1=\"" << GraphTop << "\" x2=\"" << GraphRight + TickSize << "\" y2=\"" << GraphTop << "\"/>" << std::endl;
				SVGFile << "\t<text style=\"fill:blue;text-anchor:end\" x=\"" << GraphLeft - TickSize << "\" y=\"" << GraphTop + 5 << "\">" << std::fixed << std::setprecision(1) << TempMax << "</text>" << std::endl;

				// Bottom Line
				SVGFile << "\t<line x1=\"" << GraphLeft - TickSize << "\" y1=\"" << GraphBottom << "\" x2=\"" << GraphRight + TickSize << "\" y2=\"" << GraphBottom << "\"/>" << std::endl;
				SVGFile << "\t<text style=\"fill:blue;text-anchor:end\" x=\"" << GraphLeft - TickSize << "\" y=\"" << GraphBottom + 5 << "\">" << std::fixed << std::setprecision(1) << TempMin << "</text>" << std::endl;

				// Left Line
				SVGFile << "\t<line x1=\"" << GraphLeft << "\" y1=\"" << GraphTop << "\" x2=\"" << GraphLeft << "\" y2=\"" << GraphBottom << "\"/>" << std::endl;

				// Right Line
				SVGFile << "\t<line x1=\"" << GraphRight << "\" y1=\"" << GraphTop << "\" x2=\"" << GraphRight << "\" y2=\"" << GraphBottom << "\"/>" << std::endl;

				// Vertical Division Dashed Lines
				for (auto index = 1; index < 4; index++)
				{
					SVGFile << "\t<line style=\"stroke-dasharray:1\" x1=\"" << GraphLeft - TickSize << "\" y1=\"" << GraphTop + (GraphVerticalDivision * index) << "\" x2=\"" << GraphRight + TickSize << "\" y2=\"" << GraphTop + (GraphVerticalDivision * index) << "\" />" << std::endl;
					SVGFile << "\t<text style=\"fill:blue;text-anchor:end\" x=\"" << GraphLeft - TickSize << "\" y=\"" << GraphTop + 4 + (GraphVerticalDivision * index) << "\">" << std::fixed << std::setprecision(1) << TempMax - (TempVerticalDivision * index) << "</text>" << std::endl;
				}

				// Horizontal Division Dashed Lines
				for (auto index = 0; index < (GraphWidth < TheValues.size() ? GraphWidth : TheValues.size()); index++)
				{
					struct tm UTC;
					if (0 != localtime_r(&TheValues[index].Time, &UTC))
					{
						if (graph == GraphType::daily)
						{
							if (UTC.tm_min == 0)
							{
								if (UTC.tm_hour == 0)
									SVGFile << "\t<line style=\"stroke:red\" x1=\"" << GraphLeft + index << "\" y1=\"" << GraphTop << "\" x2=\"" << GraphLeft + index << "\" y2=\"" << GraphBottom + TickSize << "\" />" << std::endl;
								else
									SVGFile << "\t<line style=\"stroke-dasharray:1\" x1=\"" << GraphLeft + index << "\" y1=\"" << GraphTop << "\" x2=\"" << GraphLeft + index << "\" y2=\"" << GraphBottom + TickSize << "\" />" << std::endl;
								if (UTC.tm_hour % 2 == 0)
									SVGFile << "\t<text style=\"text-anchor:middle\" x=\"" << GraphLeft + index << "\" y=\"" << SVGHeight - 2 << "\">" << UTC.tm_hour << "</text>" << std::endl;
							}
						}
						else if (graph == GraphType::weekly)
						{
							const std::string Weekday[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
							if ((UTC.tm_hour == 0) && (UTC.tm_min == 0))
							{
								if (UTC.tm_wday == 0)
									SVGFile << "\t<line style=\"stroke:red\" x1=\"" << GraphLeft + index << "\" y1=\"" << GraphTop << "\" x2=\"" << GraphLeft + index << "\" y2=\"" << GraphBottom + TickSize << "\" />" << std::endl;
								else
									SVGFile << "\t<line style=\"stroke-dasharray:1\" x1=\"" << GraphLeft + index << "\" y1=\"" << GraphTop << "\" x2=\"" << GraphLeft + index << "\" y2=\"" << GraphBottom + TickSize << "\" />" << std::endl;
							}
							else if ((UTC.tm_hour == 12) && (UTC.tm_min == 0))
								SVGFile << "\t<text style=\"text-anchor:middle\" x=\"" << GraphLeft + index << "\" y=\"" << SVGHeight - 2 << "\">" << Weekday[UTC.tm_wday] << "</text>" << std::endl;
						}
						else if (graph == GraphType::monthly)
						{
							if ((UTC.tm_mday == 1) && (UTC.tm_hour == 0) && (UTC.tm_min == 0))
								SVGFile << "\t<line style=\"stroke:red\" x1=\"" << GraphLeft + index << "\" y1=\"" << GraphTop << "\" x2=\"" << GraphLeft + index << "\" y2=\"" << GraphBottom + TickSize << "\" />" << std::endl;
							if ((UTC.tm_wday == 0) && (UTC.tm_hour == 0) && (UTC.tm_min == 0))
								SVGFile << "\t<line style=\"stroke-dasharray:1\" x1=\"" << GraphLeft + index << "\" y1=\"" << GraphTop << "\" x2=\"" << GraphLeft + index << "\" y2=\"" << GraphBottom + TickSize << "\" />" << std::endl;
							else if ((UTC.tm_wday == 3) && (UTC.tm_hour == 12) && (UTC.tm_min == 0))
								SVGFile << "\t<text style=\"text-anchor:middle\" x=\"" << GraphLeft + index << "\" y=\"" << SVGHeight - 2 << "\">Week " << UTC.tm_yday / 7 + 1 << "</text>" << std::endl;
						}
						else if (graph == GraphType::yearly)
						{
							const std::string Month[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
							if ((UTC.tm_yday == 0) && (UTC.tm_mday == 1) && (UTC.tm_hour == 0) && (UTC.tm_min == 0))
								SVGFile << "\t<line style=\"stroke:red\" x1=\"" << GraphLeft + index << "\" y1=\"" << GraphTop << "\" x2=\"" << GraphLeft + index << "\" y2=\"" << GraphBottom + TickSize << "\" />" << std::endl;
							else if ((UTC.tm_mday == 1) && (UTC.tm_hour == 0) && (UTC.tm_min == 0))
								SVGFile << "\t<line style=\"stroke-dasharray:1\" x1=\"" << GraphLeft + index << "\" y1=\"" << GraphTop << "\" x2=\"" << GraphLeft + index << "\" y2=\"" << GraphBottom + TickSize << "\" />" << std::endl;
							else if ((UTC.tm_mday == 15) && (UTC.tm_hour == 0) && (UTC.tm_min == 0))
								SVGFile << "\t<text style=\"text-anchor:middle\" x=\"" << GraphLeft + index << "\" y=\"" << SVGHeight - 2 << "\">" << Month[UTC.tm_mon] << "</text>" << std::endl;
						}
					}
				}

				// Directional Arrow
				SVGFile << "\t<polygon style=\"fill:red;stroke:red;fill-opacity:1;\" points=\"" << GraphLeft - 3 << "," << GraphBottom << " " << GraphLeft + 3 << "," << GraphBottom - 3 << " " << GraphLeft + 3 << "," << GraphBottom + 3 << "\" />" << std::endl;

				if (MinMax)
				{
					// OutsidePressure Values as a filled polygon showing the minimum and maximum
					SVGFile << "\t<!-- OutsidePressure MinMax -->" << std::endl;
					SVGFile << "\t<polygon style=\"fill:blue;stroke:blue\" points=\"";
					for (auto index = 1; index < (GraphWidth < TheValues.size() ? GraphWidth : TheValues.size()); index++)
						SVGFile << index + GraphLeft << "," << int(((TempMax - TheValues[index].GetOutsidePressureMax()) * TempVerticalFactor) + GraphTop) << " ";
					for (auto index = (GraphWidth < TheValues.size() ? GraphWidth : TheValues.size()) - 1; index > 0; index--)
						SVGFile << index + GraphLeft << "," << int(((TempMax - TheValues[index].GetOutsidePressureMin()) * TempVerticalFactor) + GraphTop) << " ";
					SVGFile << "\" />" << std::endl;
				}
				// always draw this line over the top of the MinMax
				// OutsidePressure Values as a continuous line
				SVGFile << "\t<!-- OutsidePressure -->" << std::endl;
				SVGFile << "\t<polyline style=\"fill:none;stroke:blue\" points=\"";
				for (auto index = 1; index < (GraphWidth < TheValues.size() ? GraphWidth : TheValues.size()); index++)
					SVGFile << index + GraphLeft << "," << int(((TempMax - TheValues[index].GetOutsidePressure()) * TempVerticalFactor) + GraphTop) << " ";
				SVGFile << "\" />" << std::endl;

				SVGFile << "</svg>" << std::endl;
				SVGFile.close();
				struct utimbuf SVGut({ TheValues.begin()->Time, TheValues.begin()->Time });
				utime(SVGFileName.c_str(), &SVGut);
			}
		}
	}
}
/////////////////////////////////////////////////////////////////////////////
std::vector<Influx_Pressure> InfluxMRTGPressure; // vector structure similar to MRTG Log File
// Takes current datapoint and updates the mapped structure in memory simulating the contents of a MRTG log file.
/////////////////////////////////////////////////////////////////////////////
volatile bool bRun = true; // This is declared volatile so that the compiler won't optimized it out of loops later in the code
void SignalHandlerSIGINT(int signal)
{
	bRun = false;
	std::cerr << "***************** SIGINT: Caught Ctrl-C, finishing loop and quitting. *****************" << std::endl;
}
void SignalHandlerSIGHUP(int signal)
{
	bRun = false;
	std::cerr << "***************** SIGHUP: Caught HangUp, finishing loop and quitting. *****************" << std::endl;
}
/////////////////////////////////////////////////////////////////////////////
static void usage(int argc, char** argv)
{
	std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
	std::cout << "  " << ProgramVersionString << std::endl;
	std::cout << "  Options:" << std::endl;
	std::cout << "    -? | --help          Print this message" << std::endl;
	std::cout << "    -v | --verbose level stdout verbosity level [" << ConsoleVerbosity << "]" << std::endl;
	std::cout << "    -W | --wind          monitor environment.wind.speedApparent" << std::endl;
	std::cout << "    -P | --pressure      monitor environment.outside.pressure" << std::endl;
	std::cout << "    -s | --svg name      SVG output directory [" << SVGDirectory << "]" << std::endl;
	std::cout << "    -x | --minmax graph  Draw the minimum and maximum temperature and humidity status on SVG graphs. 1:daily, 2:weekly, 4:monthly, 8:yearly" << std::endl;
	std::cout << "    -h | --host name     InfluxDBHost [" << InfluxDBHost << "]" << std::endl;
	std::cout << "    -p | --port number   InfluxDBPort [" << InfluxDBPort << "]" << std::endl;
	std::cout << "    -d | --database name InfluxDBDatabase [" << InfluxDBDatabase << "]" << std::endl;
	std::cout << "    -c | --cache name    InfluxDBCacheDirectory [" << InfluxDBCacheDirectory << "]" << std::endl;
	std::cout << std::endl;
}
static const char short_options[] = "?v:WPs:x:h:p:d:c:";
static const struct option long_options[] = {
		{ "help",   no_argument,       NULL, '?' },
		{ "verbose",required_argument, NULL, 'v' },
		{ "wind",   no_argument,       NULL, 'W' },
		{ "pressure",no_argument,      NULL, 'P' },
		{ "svg",	required_argument, NULL, 's' },
		{ "minmax",	required_argument, NULL, 'x' },
		{ "host",	required_argument, NULL, 'h' },
		{ "port",	required_argument, NULL, 'p' },
		{ "database",required_argument,NULL, 'd' },
		{ "cache",  required_argument, NULL, 'c' },
		{ 0, 0, 0, 0 }
};
/////////////////////////////////////////////////////////////////////////////
int main(int argc, char** argv)
{
	for (;;)
	{
		std::string TempString;
		std::filesystem::path TempPath;
		int idx;
		int c = getopt_long(argc, argv, short_options, long_options, &idx);
		if (-1 == c)
			break;
		switch (c)
		{
		case 0: /* getopt_long() flag */
			break;
		case '?':	// --help
			usage(argc, argv);
			exit(EXIT_SUCCESS);
		case 'v':	// --verbose
			try { ConsoleVerbosity = std::stoi(optarg); }
			catch (const std::invalid_argument& ia) { std::cerr << "Invalid argument: " << ia.what() << std::endl; exit(EXIT_FAILURE); }
			catch (const std::out_of_range& oor) { std::cerr << "Out of Range error: " << oor.what() << std::endl; exit(EXIT_FAILURE); }
			break;
		case 'W':	// --wind
			bMonitorWind = true;
			break;
		case 'P':	// --pressure
			bMonitorPressure = true;
			break;
		case 's':	// --svg
			TempPath = std::string(optarg);
			while (TempPath.filename().empty() && (TempPath != TempPath.root_directory())) // This gets rid of the "/" on the end of the path
				TempPath = TempPath.parent_path();
			if (ValidateDirectory(TempPath))
				SVGDirectory = TempPath;
			break;
		case 'x':	// --minmax
			try { SVGMinMax = std::stoi(optarg); }
			catch (const std::invalid_argument& ia) { std::cerr << "Invalid argument: " << ia.what() << std::endl; exit(EXIT_FAILURE); }
			catch (const std::out_of_range& oor) { std::cerr << "Out of Range error: " << oor.what() << std::endl; exit(EXIT_FAILURE); }
			break;
		case 'h':	// --host
			InfluxDBHost = std::string(optarg);
			break;
		case 'p':	// --port
			try { InfluxDBPort = std::stoi(optarg); }
			catch (const std::invalid_argument& ia) { std::cerr << "Invalid argument: " << ia.what() << std::endl; exit(EXIT_FAILURE); }
			catch (const std::out_of_range& oor) { std::cerr << "Out of Range error: " << oor.what() << std::endl; exit(EXIT_FAILURE); }
			break;
		case 'd':	// --database
			InfluxDBDatabase = std::string(optarg);
			break;
		case 'c':	// --cache
			TempPath = std::string(optarg);
			while (TempPath.filename().empty() && (TempPath != TempPath.root_directory())) // This gets rid of the "/" on the end of the path
				TempPath = TempPath.parent_path();
			if (ValidateDirectory(TempPath))
				InfluxDBCacheDirectory = TempPath;
			break;
		default:
			usage(argc, argv);
			exit(EXIT_FAILURE);
		}
	}
	///////////////////////////////////////////////////////////////////////////////////////////////
	if (ConsoleVerbosity > 0)
	{
		std::cout << "[" << getTimeISO8601(true) << "] " << ProgramVersionString << std::endl;
		if (ConsoleVerbosity > 1)
		{
			std::cout << "[                   ]     wind: " << std::boolalpha << bMonitorWind << std::endl;
			std::cout << "[                   ] pressure: " << std::boolalpha << bMonitorPressure << std::endl;
			std::cout << "[                   ]      svg: " << SVGDirectory << std::endl;
			std::cout << "[                   ]   minmax: " << SVGMinMax << std::endl;
			std::cout << "[                   ]     host: " << InfluxDBHost << std::endl;
			std::cout << "[                   ]     port: " << InfluxDBPort << std::endl;
			std::cout << "[                   ] database: " << InfluxDBDatabase << std::endl;
			std::cout << "[                   ]    cache: " << InfluxDBCacheDirectory << std::endl;
		}
	}
	else
		std::cerr << ProgramVersionString << " (starting)" << std::endl;
	///////////////////////////////////////////////////////////////////////////////////////////////
	if (InfluxDBHost.empty() || 
		(!bMonitorWind && !bMonitorPressure) ||	// If we aren't monitoring Wind or Pressure, what are we doing?
		InfluxDBDatabase.empty() || 
		(InfluxDBPort == 0) || 
		SVGDirectory.empty())
	{
		// If these are not supplied or valid, there's no purpose to be running
		usage(argc, argv);
		exit(EXIT_FAILURE);
	}
	///////////////////////////////////////////////////////////////////////////////////////////////
	tzset();
	///////////////////////////////////////////////////////////////////////////////////////////////
	std::filesystem::path InfluxDBCacheFileWind;
	std::filesystem::path InfluxDBCacheFilePressure;
	if (!InfluxDBCacheDirectory.empty())
	{
		if (bMonitorWind)
		{
			InfluxDBCacheFileWind = std::filesystem::path(InfluxDBCacheDirectory / "influxdbwind.txt");
			std::ifstream TheFile(InfluxDBCacheFileWind);
			if (TheFile.is_open())
			{
				if (ConsoleVerbosity > 0)
					std::cout << "[" << getTimeISO8601(true) << "] Reading: " << InfluxDBCacheFileWind.string() << std::endl;
				else
					std::cerr << "Reading: " << InfluxDBCacheFileWind.string() << std::endl;
				std::string TheLine;
				while (std::getline(TheFile, TheLine))
					InfluxMRTGWind.push_back(Influx_Wind(TheLine));
				TheFile.close();
			}
		}
		if (bMonitorPressure)
		{
			InfluxDBCacheFilePressure = std::filesystem::path(InfluxDBCacheDirectory / "influxdbpressure.txt");
			std::ifstream TheFile(InfluxDBCacheFilePressure);
			if (TheFile.is_open())
			{
				if (ConsoleVerbosity > 0)
					std::cout << "[" << getTimeISO8601(true) << "] Reading: " << InfluxDBCacheFilePressure.string() << std::endl;
				else
					std::cerr << "Reading: " << InfluxDBCacheFilePressure.string() << std::endl;
				std::string TheLine;
				while (std::getline(TheFile, TheLine))
					InfluxMRTGPressure.push_back(Influx_Pressure(TheLine));
				TheFile.close();
			}
		}
	}
	///////////////////////////////////////////////////////////////////////////////////////////////
	std::stringstream InfluxDB;
	InfluxDB << "http://" << InfluxDBHost << ":" << InfluxDBPort << "?db=" << InfluxDBDatabase;
	if (ConsoleVerbosity > 0)
		std::cout << "[" << getTimeISO8601(true) << "] " << InfluxDB.str() << std::endl;
	auto db = influxdb::InfluxDBFactory::Get(InfluxDB.str());
	if (bMonitorWind)
	{
		std::stringstream InfluxDBQuery;
		int RecordsReturned(0);
		if (InfluxMRTGWind.empty())
			InfluxDBQuery << "SELECT value FROM \"environment.wind.speedApparent\" WHERE time > now()-450d LIMIT " << InfluxDBQueryLimit;
		else
			InfluxDBQuery << "SELECT value FROM \"environment.wind.speedApparent\" WHERE time > '" << timeToExcelDate(InfluxMRTGWind[0].Time) << "' LIMIT " << InfluxDBQueryLimit;
		do
		{
			if (ConsoleVerbosity > 0)
				std::cout << "[" << getTimeISO8601(true) << "] " << InfluxDBQuery.str();
			RecordsReturned = 0;
			for (auto i : db->query(InfluxDBQuery.str()))
			{
				Influx_Wind myWind(i);
				UpdateMRTGData(InfluxMRTGWind, myWind);
				RecordsReturned++;
			}
			if (ConsoleVerbosity > 0)
				std::cout << " (" << RecordsReturned << " records returned)" << std::endl;
			InfluxDBQuery = std::stringstream(); // reset query string to empty
			InfluxDBQuery << "SELECT value FROM \"environment.wind.speedApparent\" WHERE time > '" << timeToExcelDate(InfluxMRTGWind[0].Time) << "' LIMIT " << InfluxDBQueryLimit;
		} while (RecordsReturned > 50);
	}
	if (bMonitorPressure)
	{
		std::stringstream InfluxDBQuery;
		int RecordsReturned(0);
		if (InfluxMRTGPressure.empty())
			InfluxDBQuery << "SELECT value FROM \"environment.outside.pressure\" WHERE time > now()-450d LIMIT " << InfluxDBQueryLimit;
		else
			InfluxDBQuery << "SELECT value FROM \"environment.outside.pressure\" WHERE time > '" << timeToExcelDate(InfluxMRTGPressure[0].Time) << "' LIMIT " << InfluxDBQueryLimit;
		do
		{
			if (ConsoleVerbosity > 0)
				std::cout << "[" << getTimeISO8601(true) << "] " << InfluxDBQuery.str();
			RecordsReturned = 0;
			for (auto i : db->query(InfluxDBQuery.str()))
			{
				Influx_Pressure myPressure(i);
				UpdateMRTGData(InfluxMRTGPressure, myPressure);
				RecordsReturned++;
			}
			if (ConsoleVerbosity > 0)
				std::cout << " (" << RecordsReturned << " records returned)" << std::endl;
			InfluxDBQuery = std::stringstream(); // reset query string to empty
			InfluxDBQuery << "SELECT value FROM \"environment.outside.pressure\" WHERE time > '" << timeToExcelDate(InfluxMRTGPressure[0].Time) << "' LIMIT " << InfluxDBQueryLimit;
		} while (RecordsReturned > 50);
	}
	auto previousHandlerSIGINT = signal(SIGINT, SignalHandlerSIGINT);	// Install CTR-C signal handler
	auto previousHandlerSIGHUP = signal(SIGHUP, SignalHandlerSIGHUP);	// Install Hangup signal handler
	while (bRun)
	{
		if (bMonitorWind)
		{
			std::string SVGTitle("Apparent Wind Speed");
			SVGTitle.insert(0, " ");
			SVGTitle.insert(0, InfluxDBDatabase);
			SVGTitle.front() = std::toupper(SVGTitle.front());
			std::vector<Influx_Wind> TheValues;
			std::filesystem::path OutputPath;
			std::ostringstream OutputFilename;
			OutputFilename.str("");
			OutputFilename << InfluxDBDatabase;
			OutputFilename << "_wind-day.svg";
			OutputPath = SVGDirectory / OutputFilename.str();
			ReadMRTGData(InfluxMRTGWind, TheValues, GraphType::daily);
			WriteSVG(TheValues, OutputPath, SVGTitle, GraphType::daily, true);
			OutputFilename.str("");
			OutputFilename << InfluxDBDatabase;
			OutputFilename << "_wind-week.svg";
			OutputPath = SVGDirectory / OutputFilename.str();
			ReadMRTGData(InfluxMRTGWind, TheValues, GraphType::weekly);
			WriteSVG(TheValues, OutputPath, SVGTitle, GraphType::weekly, true);
			OutputFilename.str("");
			OutputFilename << InfluxDBDatabase;
			OutputFilename << "_wind-month.svg";
			OutputPath = SVGDirectory / OutputFilename.str();
			ReadMRTGData(InfluxMRTGWind, TheValues, GraphType::monthly);
			WriteSVG(TheValues, OutputPath, SVGTitle, GraphType::monthly, true);
			OutputFilename.str("");
			OutputFilename << InfluxDBDatabase;
			OutputFilename << "_wind-year.svg";
			OutputPath = SVGDirectory / OutputFilename.str();
			ReadMRTGData(InfluxMRTGWind, TheValues, GraphType::yearly);
			WriteSVG(TheValues, OutputPath, SVGTitle, GraphType::yearly, true);
			if (!InfluxDBCacheFileWind.empty())
			{
				struct stat64 Stat(0);
				stat64(InfluxDBCacheFileWind.c_str(), &Stat);
				if (difftime(InfluxMRTGWind[0].Time, Stat.st_mtim.tv_sec) > 60 * 60) // If Cache File has data older than 60 minutes, write it
				{
					std::ofstream LogFile(InfluxDBCacheFileWind, std::ios_base::out | std::ios_base::trunc);
					if (LogFile.is_open())
					{
						if (ConsoleVerbosity > 0)
							std::cout << "[" << getTimeISO8601(true) << "] Writing: " << InfluxDBCacheFileWind.string() << std::endl;
						else
							std::cerr << "Writing: " << InfluxDBCacheFileWind.string() << std::endl;
						for (auto i : InfluxMRTGWind)
							LogFile << i.WriteTXT() << std::endl;
						LogFile.close();
						struct utimbuf SVGut({ InfluxMRTGWind.begin()->Time, InfluxMRTGWind.begin()->Time });
						utime(InfluxDBCacheFileWind.c_str(), &SVGut);
					}
				}
			}
		}
		if (bMonitorPressure)
		{
			std::string SVGTitle("Air Pressure");
			SVGTitle.insert(0, " ");
			SVGTitle.insert(0, InfluxDBDatabase);
			SVGTitle.front() = std::toupper(SVGTitle.front());
			std::vector<Influx_Pressure> TheValues;
			std::filesystem::path OutputPath;
			std::ostringstream OutputFilename;
			OutputFilename.str("");
			OutputFilename << InfluxDBDatabase;
			OutputFilename << "_pressure-day.svg";
			OutputPath = SVGDirectory / OutputFilename.str();
			ReadMRTGData(InfluxMRTGPressure, TheValues, GraphType::daily);
			WriteSVG(TheValues, OutputPath, SVGTitle, GraphType::daily, true);
			OutputFilename.str("");
			OutputFilename << InfluxDBDatabase;
			OutputFilename << "_pressure-week.svg";
			OutputPath = SVGDirectory / OutputFilename.str();
			ReadMRTGData(InfluxMRTGPressure, TheValues, GraphType::weekly);
			WriteSVG(TheValues, OutputPath, SVGTitle, GraphType::weekly, true);
			OutputFilename.str("");
			OutputFilename << InfluxDBDatabase;
			OutputFilename << "_pressure-month.svg";
			OutputPath = SVGDirectory / OutputFilename.str();
			ReadMRTGData(InfluxMRTGPressure, TheValues, GraphType::monthly);
			WriteSVG(TheValues, OutputPath, SVGTitle, GraphType::monthly, true);
			OutputFilename.str("");
			OutputFilename << InfluxDBDatabase;
			OutputFilename << "_pressure-year.svg";
			OutputPath = SVGDirectory / OutputFilename.str();
			ReadMRTGData(InfluxMRTGPressure, TheValues, GraphType::yearly);
			WriteSVG(TheValues, OutputPath, SVGTitle, GraphType::yearly, true);
			if (!InfluxDBCacheFilePressure.empty())
			{
				struct stat64 Stat(0);
				stat64(InfluxDBCacheFilePressure.c_str(), &Stat);
				if (difftime(InfluxMRTGPressure[0].Time, Stat.st_mtim.tv_sec) > 60 * 60) // If Cache File has data older than 60 minutes, write it
				{
					std::ofstream LogFile(InfluxDBCacheFilePressure, std::ios_base::out | std::ios_base::trunc);
					if (LogFile.is_open())
					{
						if (ConsoleVerbosity > 0)
							std::cout << "[" << getTimeISO8601(true) << "] Writing: " << InfluxDBCacheFilePressure.string() << std::endl;
						else
							std::cerr << "Writing: " << InfluxDBCacheFilePressure.string() << std::endl;
						for (auto i : InfluxMRTGPressure)
							LogFile << i.WriteTXT() << std::endl;
						LogFile.close();
						struct utimbuf SVGut({ InfluxMRTGPressure.begin()->Time, InfluxMRTGPressure.begin()->Time });
						utime(InfluxDBCacheFilePressure.c_str(), &SVGut);
					}
				}
			}
		}
		sigset_t set;
		sigemptyset(&set);
		sigaddset(&set, SIGINT);
		sigaddset(&set, SIGHUP);
		siginfo_t sig(0);
		//time_t TimeNow;
		//time(&TimeNow);
		//time_t TimeSVG = (TimeNow / DAY_SAMPLE) * DAY_SAMPLE; // hack to try to line up TimeSVG to be on a five minute period
		timespec MyTimeout(5*60, 0);
		if (ConsoleVerbosity > 0)
			std::cout << "[" << getTimeISO8601(true) << "] Waiting for signal or timeout (" << MyTimeout.tv_sec << " seconds)" << std::endl;
		int s = sigtimedwait(&set, &sig, &MyTimeout);
		switch (sig.si_signo)
		{
		case SIGINT:
			if (ConsoleVerbosity > 0)
				std::cout << "[" << getTimeISO8601(true) << "] ***************** SIGINT: Caught Ctrl-C, finishing loop and quitting. *****************" << std::endl;
			else
				std::cerr << "***************** SIGINT: Caught Ctrl-C, finishing loop and quitting. *****************" << std::endl;
			bRun = false;
			break;
		case SIGHUP:
			if (ConsoleVerbosity > 0)
				std::cout << "[" << getTimeISO8601(true) << "] ***************** SIGHUP: Caught HangUp, finishing loop and quitting. *****************" << std::endl;
			else
				std::cerr << "***************** SIGHUP: Caught HangUp, finishing loop and quitting. *****************" << std::endl;
			bRun = false;
			break;
		default:
			if (bMonitorWind)
			{
				std::stringstream InfluxDBQuery;
				InfluxDBQuery << "SELECT value FROM \"environment.wind.speedApparent\" WHERE time > '" << timeToExcelDate(InfluxMRTGWind[0].Time) << "'";
				if (ConsoleVerbosity > 0)
					std::cout << "[" << getTimeISO8601(true) << "] " << InfluxDBQuery.str();
				int count = 0;
				for (auto i : db->query(InfluxDBQuery.str()))
				{
					Influx_Wind myWind(i);
					UpdateMRTGData(InfluxMRTGWind, myWind);
					count++;
				}
				if (ConsoleVerbosity > 0)
					std::cout << " (" << count << " records returned)" << std::endl;
			}
			if (bMonitorPressure)
			{
				std::stringstream InfluxDBQuery;
				InfluxDBQuery << "SELECT value FROM \"environment.outside.pressure\" WHERE time > '" << timeToExcelDate(InfluxMRTGPressure[0].Time) << "'";
				if (ConsoleVerbosity > 0)
					std::cout << "[" << getTimeISO8601(true) << "] " << InfluxDBQuery.str();
				int count = 0;
				for (auto i : db->query(InfluxDBQuery.str()))
				{
					Influx_Pressure myPressure(i);
					UpdateMRTGData(InfluxMRTGPressure, myPressure);
					count++;
				}
				if (ConsoleVerbosity > 0)
					std::cout << " (" << count << " records returned)" << std::endl;
			}
		}
	}
	signal(SIGHUP, previousHandlerSIGHUP);	// Restore original Hangup signal handler
	signal(SIGINT, previousHandlerSIGINT);	// Restore original Ctrl-C signal handler
	///////////////////////////////////////////////////////////////////////////////////////////////
	if (ConsoleVerbosity > 0)
		std::cout << "[" << getTimeISO8601(true) << "] " << ProgramVersionString << " (exiting)" << std::endl;
	else
		std::cerr << ProgramVersionString << " (exiting)" << std::endl;
	///////////////////////////////////////////////////////////////////////////////////////////////
	return 0;
}
