#include "Sorter.h"
#include "TempFile.h"
#include <vector>
#include <algorithm>
#include <cassert>
#include <cstring>
using namespace std;
//---------------------------------------------------------------------------
/// Maximum amount of usable memory. XXX detect at runtime!
static const unsigned memoryLimit = sizeof(void*) * (1 << 27);//512MB
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
void Sorter::sort(string filename, TempFile& out, const char* (*skip)(const char*), int(*compare)(const char*, const char*), bool eliminateDuplicates)
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
	TempFile intermediate(out.getBaseFile());//�����intermediate����out�����ɵ�һ���ļ��������浱�ļ���Сû�г���memoryLimit�����һ���ֶ��ļ�
	char* ofs = 0;

	ofstream fout("sortingSystemInfo.txt");
	int num = 0;
	while (reader < limit) {
		// Collect items
		vector<Range> items;//item������ÿ��triple��ÿ��tripleռ��һ��vectorԪ�أ���˿���ֱ�ӵ���sort�Ե���triple��������
		const char* maxReader = reader + memoryLimit;
		while (reader < limit) {
			const char* start = reader;
			reader = skip(reader);
			items.push_back(Range(start, reader));

			// Memory Overflow?
			if ((reader + (sizeof(Range) * items.size())) > maxReader) {
				num++;
				fout << "num=" << num << "\tsize=" << (sizeof(Range) * items.size()) << endl;
				fout << getMemoryInfo() << endl;
				fout << "---------------------------------" << endl;
				fout << "item size = " << items.size() << endl;
				break;
			}
				
		}

		// Sort the run
		std::sort(items.begin(), items.end(), CompareSorter(compare));

		// Did everything fit?
		if ((reader == limit) && (runs.empty())) {//����Ĵ��������ҽ��������ļ���СС��memoryLimit��512Mʱ
			spool(0, out, items, eliminateDuplicates);//�����out����ߴ�������outԴ�ļ�
			fout << "�������������Ϣ��ʾ�ļ���Сû�г���memoryLimit����" << endl;
			break;
			//����break֮��runs���Ϊ�գ�����items������������ݣ�û�зֶΣ�������ȫ������out�ļ���ע�⣺intermediate�ļ�Ϊ�գ�
		}

		// No, spool to intermediate file
		char* newOfs = spool(ofs, intermediate, items, eliminateDuplicates);
		runs.push_back(Range(ofs, newOfs));
		ofs = newOfs;
		//��������ѭ������������runs��ߴ����������ݵķֶ���Ϣ������ֶ���Ϣ��ÿ512Mһ��vectorԪ��
		//item������ݶ������뵽intermediate����ļ��ע�⣺out�ļ�Ϊ�գ�
		//intermediate�ļ��������Ź���ķֶ���Ϣ
	}
	intermediate.close();
	mappedIn.close();

	fout << "--------------part sort end---------------" << endl;
	fout << getMemoryInfo() << endl;
	fout << "---------------------------------" << endl;

	// Do we habe to merge runs?
	if (!runs.empty()) {//runs��Ϊ�գ�˵���Ͻ׶ε�while�������������зֶ����ݶ���intermediate�out�ļ�Ϊ�գ��������ķֶ�������Ҫ����merge
		// Map the ranges
		MemoryMappedFile tempIn;//ָ��intermediate��ķֶ�����
		assert(tempIn.open(intermediate.getFile().c_str()));
		for (vector<Range>::iterator iter = runs.begin(), limit = runs.end(); iter != limit; ++iter) {
			//�����ٴθ�ֵruns��ߵķֶ����ݶ�λ��Ϣ���о�û�б�Ҫ����Ϊ����һ��whileѭ�����Ѿ���ֵ����
			//�б�Ҫ����Ϊ�ϸ�ѭ����runs��ߴ����TempFile�ķֶζ�λ��Ϣ������Ҫ�ĳ�MemoryMappedFile�ķֶζ�λ��Ϣ
			(*iter).from = tempIn.getBegin() + ((*iter).from - static_cast<char*> (0));
			(*iter).to = tempIn.getBegin() + ((*iter).to - static_cast<char*> (0));
		}

		// Sort the run heads��ע�⣡ֻ��head��
		std::sort(runs.begin(), runs.end(), CompareSorter(compare));
		//��range��from����runs��ߵ�rangeԪ��

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
			Range head(runs.front().from, skip(runs.front().from));//һ��triple
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
				unsigned left = 2 * pos + 1, right = left + 1;//�ѵ����left��right�г��ޣ����޵Ļ�˵������ǰ�ߵ�memoryLimit=512M�ķֶα�׼���ԣ��������Ļ�runsҲ���п��ܳ���STL�����ư��������ұ������ȷ���ֶα�׼�����Ƕ��
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

	intermediate.discard();
	out.close();
}
//---------------------------------------------------------------------------
