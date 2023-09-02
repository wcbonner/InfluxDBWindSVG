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
