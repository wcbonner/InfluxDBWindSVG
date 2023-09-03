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

#include "influxd-wind-svg.h"

using namespace std;

/////////////////////////////////////////////////////////////////////////////
static const std::string ProgramVersionString("influxd-wind-svg Version 2.20230902-1 Built on: " __DATE__ " at " __TIME__);
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
int ConsoleVerbosity(1);
std::filesystem::path SVGDirectory;	// If this remains empty, SVG Files are not created. If it's specified, _day, _week, _month, and _year.svg files are created for each bluetooth address seen.
int SVGMinMax(0); // 0x01 = Draw Temperature and Humiditiy Minimum and Maximum line on daily, 0x02 = on weekly, 0x04 = on monthly, 0x08 = on yearly
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
		ApparentWindSpeed = wind;
		Averages = 1;
	};
	Influx_Wind(const std::string& data);
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
std::vector<Influx_Wind> InfluxMRTGLogs; // vector structure similar to MRTG Log File
// Takes current datapoint and updates the mapped structure in memory simulating the contents of a MRTG log file.
void UpdateMRTGData(std::vector<Influx_Wind>& FakeMRTGFile, Influx_Wind& TheValue)
{
	if (FakeMRTGFile.empty())
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
		DaySampleFirst->NormalizeTime(Influx_Wind::granularity::day);
		if (difftime(DaySampleFirst->Time, (DaySampleFirst + 1)->Time) > DAY_SAMPLE)
			DaySampleFirst->Time = (DaySampleFirst + 1)->Time + DAY_SAMPLE;
		if (DaySampleFirst->GetTimeGranularity() == Influx_Wind::granularity::year)
		{
			if (ConsoleVerbosity > 1)
				std::cout << "[" << getTimeISO8601() << "] shuffling year " << timeToExcelLocal(DaySampleFirst->Time) << " > " << timeToExcelLocal(YearSampleFirst->Time) << std::endl;
			// shuffle all the year samples toward the end
			std::copy_backward(YearSampleFirst, YearSampleLast - 1, YearSampleLast);
			*YearSampleFirst = Influx_Wind();
			for (auto iter = DaySampleFirst; (iter->IsValid() && ((iter - DaySampleFirst) < (12 * 24))); iter++) // One Day of day samples
				*YearSampleFirst += *iter;
		}
		if ((DaySampleFirst->GetTimeGranularity() == Influx_Wind::granularity::year) ||
			(DaySampleFirst->GetTimeGranularity() == Influx_Wind::granularity::month))
		{
			if (ConsoleVerbosity > 1)
				std::cout << "[" << getTimeISO8601() << "] shuffling month " << timeToExcelLocal(DaySampleFirst->Time) << std::endl;
			// shuffle all the month samples toward the end
			std::copy_backward(MonthSampleFirst, MonthSampleLast - 1, MonthSampleLast);
			*MonthSampleFirst = Influx_Wind();
			for (auto iter = DaySampleFirst; (iter->IsValid() && ((iter - DaySampleFirst) < (12 * 2))); iter++) // two hours of day samples
				*MonthSampleFirst += *iter;
		}
		if ((DaySampleFirst->GetTimeGranularity() == Influx_Wind::granularity::year) ||
			(DaySampleFirst->GetTimeGranularity() == Influx_Wind::granularity::month) ||
			(DaySampleFirst->GetTimeGranularity() == Influx_Wind::granularity::week))
		{
			if (ConsoleVerbosity > 1)
				std::cout << "[" << getTimeISO8601() << "] shuffling week " << timeToExcelLocal(DaySampleFirst->Time) << std::endl;
			// shuffle all the month samples toward the end
			std::copy_backward(WeekSampleFirst, WeekSampleLast - 1, WeekSampleLast);
			*WeekSampleFirst = Influx_Wind();
			for (auto iter = DaySampleFirst; (iter->IsValid() && ((iter - DaySampleFirst) < 6)); iter++) // Half an hour of day samples
				*WeekSampleFirst += *iter;
		}
	}
	if (ZeroAccumulator)
		FakeMRTGFile[1] = Influx_Wind();
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
		struct stat64 SVGStat;
		SVGStat.st_mtim.tv_sec = 0;
		if (-1 == stat64(SVGFileName.c_str(), &SVGStat))
			if (ConsoleVerbosity > 0)
				perror(SVGFileName.c_str());
		//std::cout << "[" << getTimeISO8601() << "] stat returned error on : " << SVGFileName << std::endl;
		if (TheValues.begin()->Time > SVGStat.st_mtim.tv_sec)	// only write the file if we have new data
		{
			std::ofstream SVGFile(SVGFileName);
			if (SVGFile.is_open())
			{
				if (ConsoleVerbosity > 0)
					std::cout << "[" << getTimeISO8601() << "] Writing: " << SVGFileName.string() << " With Title: " << Title << std::endl;
				else
					std::cerr << "Writing: " << SVGFileName.string() << " With Title: " << Title << std::endl;
				std::ostringstream tempOString;
				tempOString << "Wind Speed (" << std::fixed << std::setprecision(1) << TheValues[0].GetApparentWindSpeed();
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
					// Temperature Values as a filled polygon showing the minimum and maximum
					SVGFile << "\t<!-- Temperature MinMax -->" << std::endl;
					SVGFile << "\t<polygon style=\"fill:blue;stroke:blue\" points=\"";
					for (auto index = 1; index < (GraphWidth < TheValues.size() ? GraphWidth : TheValues.size()); index++)
						SVGFile << index + GraphLeft << "," << int(((TempMax - TheValues[index].GetApparentWindSpeedMax()) * TempVerticalFactor) + GraphTop) << " ";
					for (auto index = (GraphWidth < TheValues.size() ? GraphWidth : TheValues.size()) - 1; index > 0; index--)
						SVGFile << index + GraphLeft << "," << int(((TempMax - TheValues[index].GetApparentWindSpeedMin()) * TempVerticalFactor) + GraphTop) << " ";
					SVGFile << "\" />" << std::endl;
				}
				else
				{
					// Temperature Values as a continuous line
					SVGFile << "\t<!-- Temperature -->" << std::endl;
					SVGFile << "\t<polyline style=\"fill:none;stroke:blue\" points=\"";
					for (auto index = 1; index < (GraphWidth < TheValues.size() ? GraphWidth : TheValues.size()); index++)
						SVGFile << index + GraphLeft << "," << int(((TempMax - TheValues[index].GetApparentWindSpeed()) * TempVerticalFactor) + GraphTop) << " ";
					SVGFile << "\" />" << std::endl;
				}

				SVGFile << "</svg>" << std::endl;
				SVGFile.close();
				struct utimbuf SVGut;
				SVGut.actime = TheValues.begin()->Time;
				SVGut.modtime = TheValues.begin()->Time;
				utime(SVGFileName.c_str(), &SVGut);
			}
		}
	}
}
/////////////////////////////////////////////////////////////////////////////
int main(int argc, char** argv)
{
	cout << "Hello CMake." << endl;

	auto db = influxdb::InfluxDBFactory::Get("http://localhost:8086?db=sola");
	//db->createDatabaseIfNotExists();

	std::cout << "SHOW DATABASES" << std::endl;
	for (auto i : db->query("SHOW DATABASES"))
		std::cout << i.getTags() << std::endl;
	std::cout << std::endl;

	std::cout << "SHOW MEASUREMENTS" << std::endl;
	for (auto i : db->query("SHOW MEASUREMENTS"))
		std::cout << i.getTags() << std::endl;
	std::cout << std::endl;

	std::cout << "SELECT * FROM \"environment.wind.speedApparent\" WHERE time > now()-5m" << std::endl;
	//for (auto i : db->query("SELECT * FROM \"environment.wind.speedApparent\" WHERE time > now()-1h"))
	for (auto i : db->query("SELECT * FROM \"environment.wind.speedApparent\" WHERE time > now()-5m"))
	{
		std::cout << i.getName() << ":";
		std::cout << i.getTags() << ":";
		std::cout << i.getFields() << ":";
		std::cout << std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(i.getTimestamp().time_since_epoch()).count()) << std::endl;
	}

	std::cout << "SELECT value FROM \"environment.wind.speedApparent\" WHERE time > now()-5m" << std::endl;
	for (auto i : db->query("SELECT value FROM \"environment.wind.speedApparent\" WHERE time > now()-5m"))
	{
		std::cout << i.getFields() << ":";
		std::cout << std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(i.getTimestamp().time_since_epoch()).count()) << std::endl;
	}

	return 0;
}
