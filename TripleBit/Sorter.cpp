#include "Sorter.h"
#include "TempFile.h"
#include <vector>
#include <algorithm>
#include <cassert>
#include <cstring>
using namespace std;
//---------------------------------------------------------------------------
/// Maximum amount of usable memory. XXX detect at runtime!
static const unsigned memoryLimit = sizeof(void*) * (1 << 27);//438MB
//---------------------------------------------------------------------------
namespace {
//---------------------------------------------------------------------------
/// A memory range
struct Range {
	const char* from, *to;

	/// Constructor
	Range(const char* from, const char* to) :
		from(from), to(to) {
	}

	/// Some content?
	bool equals(const Range& o) {
		return ((to - from) == (o.to - o.from)) && (memcmp(from, o.from, to - from) == 0);
	}
};
//---------------------------------------------------------------------------
/// Sort wrapper that cotlls the comparison function
struct CompareSorter {
	/// Comparison function
	typedef int (*func)(const char*, const char*);

	/// Comparison function
	const func compare;

	/// Constructor
	CompareSorter(func compare) :
		compare(compare) {
	}

	/// Compare two entries
	bool operator()(const Range& a, const Range& b) const {
		return compare(a.from, b.from) < 0;
	}
};
//---------------------------------------------------------------------------
static char* spool(char* ofs, TempFile& out, const vector<Range>& items, bool eliminateDuplicates)
// Spool items to disk
{
	Range last(0, 0);
	for (vector<Range>::const_iterator iter = items.begin(), limit = items.end(); iter != limit; ++iter) {
		if ((!eliminateDuplicates) || (!last.equals(*iter))) {
			last = *iter;
			out.write(last.to - last.from, last.from);
			ofs += last.to - last.from;
		}
	}
	return ofs;
}
//---------------------------------------------------------------------------
}
//---------------------------------------------------------------------------
void Sorter::sort(string logfilename, string filename, TempFile& out, const char* (*skip)(const char*), int(*compare)(const char*, const char*), bool eliminateDuplicates)
// Sort a temporary file
{
	// Open the input
	//in.close();
	MemoryMappedFile mappedIn;
	// assert(mappedIn.open(in.getFile().c_str()));
	assert(mappedIn.open(filename.c_str()));
	const char* reader = mappedIn.getBegin(), *limit = mappedIn.getEnd();

	// Produce runs
	vector<Range> runs;
	TempFile intermediate(out.getBaseFile());//这里的intermediate是由out又生成的一个文件，用来存当文件大小没有超过memoryLimit的最后一个分段文件
	char* ofs = 0;

	ofstream fout(logfilename,ios::out);
	int num = 0;//用来记录轮数，填满一个item数组为一轮
	while (reader < limit) {
		// Collect items
		//vector<Range> items;//item里存的是每个triple！每个triple占用一个vector元素，因此可以直接调用sort对单个triple进行排序。这里的vector存的东西在栈里，而且经过测试，貌似在没有编译器优化的情况下，每一轮的vector都不会被回收，即使后边没有用到，即使超过了变量的生命范围
		vector<Range>* items = new vector<Range>();//改为了在堆里申请，并且进行了手动回收

		const char* maxReader = reader + memoryLimit;
		while (reader < limit) {
			const char* start = reader;
			reader = skip(reader);

			//如果rawfact文件不完整，那么会出现这里的最后一个三元组不完整，那么这里加一个忽略最后一个三元组的语句
			if(reader>limit) break;

			items->push_back(Range(start, reader));

			// Memory Overflow?
			if ((reader + (sizeof(Range) * items->size())) > maxReader) {
				num++;
				fout << "item size = " << items->size() << endl;
				fout << "num=" << num << "\tsize=" << (sizeof(Range) * items->size()) << endl;
				fout << getMemoryInfo();
				fout << "---------------------------------" << endl;
				break;
			}
				
		}

		// Sort the run
		std::sort(items->begin(), items->end(), CompareSorter(compare));

		// Did everything fit?
		if ((reader == limit) && (runs.empty())) {//这里的触发，当且仅当传入文件大小小于memoryLimit的438M时，并且文件没有问题（数据完整）
			spool(0, out, *items, eliminateDuplicates);//这里的out是外边传进来的out源文件
			fout << "如果看到这条信息表示文件大小没有超过memoryLimit限制，文件完整" << endl;
			break;
			//这里break之后，runs里边为空，并且items里存有所有数据（没有分段），且已全部存入out文件，注意：intermediate文件为空！
		}
		if((runs.empty()) && (reader>limit)){//这里的触发，表示当且仅当传入文件的大小小于memoryLimit的438M时，并且文件有问题，最后一个triple不完整
			spool(0, out, *items, eliminateDuplicates);
			fout << "如果看到这条信息表示文件大小没有超过memoryLimit限制，但是文件不完整，最后一个triple信息不完整！" << endl;
			break;
		}

		// No, spool to intermediate file
		char* newOfs = spool(ofs, intermediate, *items, eliminateDuplicates);
		runs.push_back(Range(ofs, newOfs));
		ofs = newOfs;
		//如果这里的循环正常结束，runs里边存有所有数据的分段信息，这个分段信息是每512M一个vector元素
		//item里的数据都被存入到intermediate这个文件里，注意：out文件为空！
		//intermediate文件里存的是排过序的分段信息
		delete items;
	}
	intermediate.close();
	mappedIn.close();

	fout << "--------------part sort end---------------" << endl;
	fout << getMemoryInfo();
	fout << "---------------------------------" << endl;

	// Do we habe to merge runs?
	if (!runs.empty()) {//runs不为空，说明上阶段的while正常结束，所有分段数据都在intermediate里，out文件为空！因此这里的分段数据需要进行merge
		// Map the ranges
		MemoryMappedFile tempIn;//指向intermediate里的分段数据
		assert(tempIn.open(intermediate.getFile().c_str()));
		for (vector<Range>::iterator iter = runs.begin(), limit = runs.end(); iter != limit; ++iter) {
			//这里再次赋值runs里边的分段数据定位信息，感觉没有必要，因为在上一个while循环里已经赋值过了
			//有必要，因为上个循环，runs里边存的是TempFile的分段定位信息，现在要改成MemoryMappedFile的分段定位信息
			(*iter).from = tempIn.getBegin() + ((*iter).from - static_cast<char*> (0));
			(*iter).to = tempIn.getBegin() + ((*iter).to - static_cast<char*> (0));
		}

		// Sort the run heads，注意！只是head！
		std::sort(runs.begin(), runs.end(), CompareSorter(compare));
		//按range的from排序runs里边的range元素

		fout << "runs.size = " << runs.size() << endl;
		if(runs.size()<10000){
			for (int i = 0; i < runs.size(); i++) {
				fout << "runs[" << i << "].from = " << runs[i].from << "\truns[" << i << "].to = " << runs[i].to << endl;
			}
		}else{
			fout << "runs.size > 10000 !!!!!!!!!!!" << endl;
			for (int i = 0; i < 10000; i++) {
				fout << "runs[" << i << "].from = " << runs[i].from << "\truns[" << i << "].to = " << runs[i].to << endl;
			}
		}


		// And merge them
		Range last(0, 0);
		while (!runs.empty()) {
			// Write the first entry if no duplicate
			Range head(runs.front().from, skip(runs.front().from));//一个triple
			if ((!eliminateDuplicates) || (!last.equals(head)))
				out.write(head.to - head.from, head.from);
			last = head;

			// Update the first entry. First entry done?
			if ((runs.front().from = head.to) == runs.front().to) {
				runs[0] = runs[runs.size() - 1];
				runs.pop_back();
			}

			// Check the heap condition
			unsigned pos = 0, size = runs.size();
			while (pos < size) {
				unsigned left = 2 * pos + 1, right = left + 1;//难道这个left和right有超限？超限的话说明我最前边的memoryLimit=512M的分段标准不对？那这样的话runs也极有可能超过STL的限制啊！所以我必须测试确定分段标准到底是多大！//经过测试，这个分段标准非常大，差不多438M分一段，不可能出现run.size的范围大于unsighed的表示范围，虽然跟512M不一样，但差的不多
				if (left >= size)
					break;
				if (right < size) {
					if (compare(runs[pos].from, runs[left].from) > 0) {
						if (compare(runs[pos].from, runs[right].from) > 0) {
							if (compare(runs[left].from, runs[right].from) < 0) {
								std::swap(runs[pos], runs[left]);
								pos = left;
							} else {
								std::swap(runs[pos], runs[right]);
								pos = right;
							}
						} else {
							std::swap(runs[pos], runs[left]);
							pos = left;
						}
					} else if (compare(runs[pos].from, runs[right].from) > 0) {
						std::swap(runs[pos], runs[right]);
						pos = right;
					} else
						break;
				} else {
					if (compare(runs[pos].from, runs[left].from) > 0) {
						std::swap(runs[pos], runs[left]);
						pos = left;
					} else
						break;
				}
			}
		}
		tempIn.close();
	}
	fout << "--------------all sort end---------------" << endl;
	fout << getMemoryInfo();
	fout << "---------------------------------" << endl;
	intermediate.discard();
	out.close();
}
//---------------------------------------------------------------------------
