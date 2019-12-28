
#ifndef _SYSTEMINFO_H_
#define _SYSTEMINFO_H_

#include <iostream>
#include <cstdlib>
#include <string>
using namespace std;


//string getOsInfo()
//{
//	FILE* fp = fopen("/proc/version", "r");
//	if (NULL == fp)
//		printf("failed to open version\n");
//	char szTest[1000] = { 0 };
//	string str;
//	while (!feof(fp))
//	{
//		memset(szTest, 0, sizeof(szTest));
//		fgets(szTest, sizeof(szTest) - 1, fp); // leave out \n  
//		str = str + szTest;
//		//printf("%s", szTest);
//	}
//	fclose(fp);
//	return str;
//}

static std::string getCpuInfo()
{
	FILE* fp = fopen("/proc/cpuinfo", "r");
	if (NULL == fp)
		printf("failed to open cpuinfo\n");
	char szTest[1000] = { 0 };
	// read file line by line   
	string str;
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

static std::string getMemoryInfo()
{
	FILE* fp = fopen("/proc/meminfo", "r");
	if (NULL == fp)
		printf("failed to open meminfo\n");
	char szTest[1000] = { 0 };
	string str;
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