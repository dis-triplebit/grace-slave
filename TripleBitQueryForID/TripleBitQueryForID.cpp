/*
 * TripleBitQuery.cpp
 *
 *  Created on: Apr 12, 2011
 *      Author: root
 */

#include "../TripleBit/TripleBit.h"
#include "../TripleBit/TripleBitRepository.h"
#include "../TripleBit/OSFile.h"
#include "../TripleBit/MMapBuffer.h"
#include "tripleBitQueryAPI.h"
#include <vector>
char* DATABASE_PATH;
char* QUERY_PATH;
int main(int argc, char* argv[]) {
	if (argc < 3) {
		fprintf(stderr, "Usage: %s <TripleBit Directory> <Query files Directory>\n", argv[0]);
		return -1;
	}

	DATABASE_PATH = argv[1];
	QUERY_PATH = argv[2];

	TripleBitRepository* repo = TripleBitRepository::create(argv[1]);
	if (repo == NULL) {
		return -1;
	}
	/*
	vector<string> res;
	char* qustr = "select ?x where { ?x <http://www.w3.org/1999/02/22-rdf-syntax-ns#type> <http://www.lehigh.edu/~zhp2/2004/0401/univ-bench.owl#UndergraduateStudent> .}";
	res = search(qustr, repo);

	cout<< "res.szie:"<< res.size()<<endl;
	for(int i=0;i<res.size();i++)
		cout<<res[i]<<endl;
	cout<<"searvg sa ds fas"<<endl;
	*/
	
	if (argc == 3) {
		repo->cmd_line_sm(stdin, stdout, argv[2]);
	} else if (argc == 5 && strcmp(argv[4], "--cold") == 0) {
		repo->cmd_line_cold(stdin, stderr, argv[3]);
	} else if (argc == 5 && strcmp(argv[4], "--warm") == 0) {
		repo->cmd_line_warm(stdin, stderr, argv[3]);
	}
	
	return 0;
}
