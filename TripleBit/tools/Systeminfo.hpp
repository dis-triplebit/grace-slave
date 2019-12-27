
#ifndef SYSTEMINFO_HPP_
#define SYSTEMINFO_HPP_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
std::string getosInfo()
{
	FILE* fp = fopen("/proc/version", "r");
	if (NULL == fp)
		printf("failed to open version\n");
	char szTest[1000] = { 0 };
	std::string str;
	while (!feof(fp))
	{
		memset(szTest, 0, sizeof(szTest));
		fgets(szTest, sizeof(szTest) - 1, fp); // leave out \n  
		str = str + szTest;
		//printf("%s", szTest);
	}
	fclose(fp);
	return str;
}

std::string getcpuInfo()
{
	FILE* fp = fopen("/proc/cpuinfo", "r");
	if (NULL == fp)
		printf("failed to open cpuinfo\n");
	char szTest[1000] = { 0 };
	// read file line by line   
	std::string str;
	while (!feof(fp))
	{
		memset(szTest, 0, sizeof(szTest));
		fgets(szTest, sizeof(szTest) - 1, fp); // leave out \n  
		str = str + szTest;
		//printf("%s", szTest);
	}
	fclose(fp);
	return str;
}

std::string getmemoryInfo()
{
	FILE* fp = fopen("/proc/meminfo", "r");
	if (NULL == fp)
		printf("failed to open meminfo\n");
	char szTest[1000] = { 0 };
	std::string str;
	while (!feof(fp))
	{
		memset(szTest, 0, sizeof(szTest));
		fgets(szTest, sizeof(szTest) - 1, fp);
		str = str + szTest;
		//printf("%s", szTest);
	}
	fclose(fp);
	return str;
}



#endif