/*
 * Sorter.h
 * Create on: 2011-3-15
 * 	  Author: liupu
 */
#ifndef SORTER_H
#define SORTER_H
#include<iostream>
class TempFile;

/// Sort a temporary file
class Sorter {
   public:
   /// Sort a file
   static void sort(std::string filename,TempFile& out,const char* (*skip)(const char*),int (*compare)(const char*,const char*),bool eliminateDuplicates=false);
};
#endif /*SOTER_H*/
