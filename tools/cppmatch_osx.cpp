/*
 * cppmatch.cpp
 *
 *  Created on: Nov 4, 2010
 *      Author: burkholderab
 *
 *  refactored and commented by Kris Zarns on 2013-10-04
 *  To make:
 *    g++ -03 -o cppmatch_kz cppmatch_kz.cpp
 *
 */

#include <iostream>
#include <vector>
#include <fstream>
#include <map>
#include <sstream>
#include <tr1/unordered_map>
#include <errno.h>
#include <set>
#include <getopt.h>
#include <stdio.h>
#include <string.h>

enum processing_options {IGNORE_STRAND, SAME_STRAND, OPPOSITE_STRAND, SENSE, SENSE_SPLIT};
enum cache_types {CACHE_IGNORE_STRAND, CACHE_STRAND, CACHE_SENSE};

using namespace std;
using tr1::unordered_map;

class chr_entry {
	public:
		vector<string> dbdesc1;
		vector<string> dbdesc2;
		vector<long> dbphysical_start;
		vector<long> dbphysical_end;
		vector<string> dbwhole;
		vector<int> dbfound;
};

class chr_entry_s : public chr_entry {
	public:
		vector<string> dbstrand;
};

//describes the fields in an entry
struct entry {
	string desc1;
	string chr;
	long physical_start;
	long physical_end;
	string strand;
	string whole;
	int found;
};

//description2 for a database entry is a string
struct dentry : public entry {
	string desc2;
};

//description2 for a query entry is a long
struct qentry : public entry {
	long desc2;
};

struct ptr_entry {
	string *desc1;
	string *desc2;
	string *chr;
	long *physical_start;
	long *physical_end;
	string *strand;
	string *whole;
	int *found;
};

struct option long_options[] = {
		{"help", 0, NULL, 'h'},
		{"strands", 1, NULL, 's'},
		{"no_zeros", 0, NULL, 'z'}
};

ofstream outfile, outfile2;
ofstream totalfile, totalfile2;
unordered_map<string, chr_entry> db;
unordered_map<string, unordered_map<string, chr_entry> > db_strand;
unordered_map<string, chr_entry_s> db_sense;
map<string, long> table;
map<string, long> table2;
set<string> strand_list;
unordered_map<string, string> strand_map;

void usage(void) {
	cout << "Usage: cppmatch [options] [DB File Name] [Query File Name] [Output File Name]\n";
	cout << "Available Options:\n";
	cout << "  --help                      produce this help message\n";
	cout << "  -s [ --strands] arg (=i)    specify handling of strand identifiers:\n";
	cout << "                                 i    ignore strand identifiers\n";
	cout << "                                 s    report only same-strand hits\n";
	cout << "                                 o    report only opposite-strand hits\n";
	cout << "                                 bf   flag each hit as sense or antisense\n";
	cout << "                                 bs   write sense and antisense hits to separate\n";
	cout << "                                      files\n";
	cout << "  -z [ --no_zeros]             remove zero valued genes in the final result, default false\n";
	return;
}

//caching reads in the transcription start sites from the genome data and
//organizes them into a map for indexed access the different flavors of
//caching here could be combined into one general function cache ignore drops
//the strand information from the db file and assumes everything is on the
//same strand
void cache_ignore_strand(ifstream &db_file) {
	dentry dbentry;
	string line;
	getline(db_file, line);
	while(!db_file.eof()) {
		istringstream in_stream(line);

		in_stream >> dbentry.desc1 >> dbentry.desc2 >> dbentry.chr >>
		  	dbentry.physical_start >> dbentry.physical_end;

		if(in_stream.fail()) {
			cout << "DB File contains bad line, skipping: " << line << endl;
		}else {
			ostringstream out_stream;
			out_stream << dbentry.desc1 << '\t' << dbentry.desc2 << '\t' <<
				dbentry.chr << '\t' << dbentry.physical_start << '\t' <<
				dbentry.physical_end;
			db[dbentry.chr].dbdesc1.push_back(dbentry.desc1);
			db[dbentry.chr].dbdesc2.push_back(dbentry.desc2);
			db[dbentry.chr].dbphysical_start.push_back(dbentry.physical_start);
			db[dbentry.chr].dbphysical_end.push_back(dbentry.physical_end);
			db[dbentry.chr].dbfound.push_back(0);
			db[dbentry.chr].dbwhole.push_back(out_stream.str());
		}
		getline(db_file, line);
	}
	db_file.close();
	return;
}

//cache_strand splits the db file into separate caches for the + and - strands
void cache_strand(ifstream &db_file) {
	dentry dbentry;
	string line;
	getline(db_file, line);
	while(!db_file.eof()) {
		istringstream in_stream(line);
		in_stream >> dbentry.desc1 >> dbentry.desc2 >> dbentry.chr >>
			dbentry.physical_start >> dbentry.physical_end >> dbentry.strand;

		if(in_stream.fail()) {
			cout << "DB File contains bad line, skipping: " << line << endl;
		}else {
			ostringstream out_stream;
			out_stream << dbentry.desc1 << '\t' << dbentry.desc2 << '\t' <<
				dbentry.chr << '\t' << dbentry.physical_start << '\t' <<
				dbentry.physical_end << '\t' << dbentry.strand;

			strand_list.insert(dbentry.strand);
			db_strand[dbentry.strand][dbentry.chr].dbdesc1.push_back(dbentry.desc1);
			db_strand[dbentry.strand][dbentry.chr].dbdesc2.push_back(dbentry.desc2);
			db_strand[dbentry.strand][dbentry.chr].dbphysical_start.push_back(
				dbentry.physical_start
			);

			db_strand[dbentry.strand][dbentry.chr].dbphysical_end.push_back(
				dbentry.physical_end
			);

			db_strand[dbentry.strand][dbentry.chr].dbfound.push_back(0);
			db_strand[dbentry.strand][dbentry.chr].dbwhole.push_back(
				out_stream.str()
			);

		}
		getline(db_file, line);
	}
	db_file.close();
	return;
}

//cache_sense maintains the strand information but does not split the data
//into seperate caches
void cache_sense(ifstream &db_file) {
	dentry dbentry;
	string line;
	getline(db_file, line);
	while(!db_file.eof()) {
		istringstream in_stream(line);
		in_stream >> dbentry.desc1 >> dbentry.desc2 >> dbentry.chr >>
			dbentry.physical_start >> dbentry.physical_end >> dbentry.strand;

		if(in_stream.fail()) {
			cout << "DB File contains bad line, skipping: " << line << endl;
		}else {
			ostringstream out_stream;
			out_stream << dbentry.desc1 << '\t' << dbentry.desc2 << '\t' <<
				dbentry.chr << '\t' << dbentry.physical_start << '\t' <<
				dbentry.physical_end << '\t' << dbentry.strand;

			strand_list.insert(dbentry.strand);
			db_sense[dbentry.chr].dbdesc1.push_back(dbentry.desc1);
			db_sense[dbentry.chr].dbdesc2.push_back(dbentry.desc2);
			db_sense[dbentry.chr].dbphysical_start.push_back(dbentry.physical_start);
			db_sense[dbentry.chr].dbphysical_end.push_back(dbentry.physical_end);
			db_sense[dbentry.chr].dbstrand.push_back(dbentry.strand);
			db_sense[dbentry.chr].dbfound.push_back(0);
			db_sense[dbentry.chr].dbwhole.push_back(out_stream.str());
		}
		getline(db_file, line);
	}
	db_file.close();
	return;
}

//"query" the "database"
//do a linear scan over the subset of the cache corresponding to this chromosome
//check for matches
void query_ignore_strand(void *line) {
	qentry q;
	bool qstartcmp;
	bool qendcmp;
	ptr_entry arrays;
	ostringstream acc;
	istringstream in_stream(*reinterpret_cast<string*>(line));
	in_stream >> q.desc1 >> q.desc2 >> q.chr >> q.physical_start >> q.physical_end;
	if(in_stream.fail()) {
		cout << "Query File contains bad line, skipping: " <<
			*reinterpret_cast<string*>(line) << endl;

	}else {
		ostringstream out_stream;
		out_stream << q.desc1 << '\t' << q.desc2 << '\t' << q.chr << '\t' <<
			q.physical_start << '\t' << q.physical_end;

		q.whole = out_stream.str();
		//is this chromosome in the db?
		if(db.find(q.chr) != db.end()) {
			//find our length and set starting pointers
			size_t max = db[q.chr].dbwhole.size();
			arrays.desc1 = &db[q.chr].dbdesc1[0];
			arrays.desc2 = &db[q.chr].dbdesc2[0];
			arrays.physical_start = &db[q.chr].dbphysical_start[0];
			arrays.physical_end = &db[q.chr].dbphysical_end[0];
			arrays.found = &db[q.chr].dbfound[0];
			arrays.whole = &db[q.chr].dbwhole[0];
			//iterate over our pointers
			for(size_t i = 0; i < max; i++) {
				//if the array end is before the q start
				// we are off the right side of the array
				//if the array start is before the q end
				// we are off the left side of the array
				//in either case, skip to the next entry
				//if we knew these were sorted
				// we could stop once we are off the left side
				if(arrays.physical_end[i] < q.physical_start
					 || arrays.physical_start[i] > q.physical_end
				)
					 continue;
				//we didn't skip so we know there is some type of overlap
				//determine what type

				arrays.found[i] += 1;
				qstartcmp = (q.physical_start >= arrays.physical_start[i]);
				qendcmp = (q.physical_end <= arrays.physical_end[i]);
				if(qstartcmp && qendcmp){
					//the query is contained by the db entry
					acc << arrays.whole[i] << '\t' << q.whole  << "\tB" << endl;
				}else if(qstartcmp == 1){
					//the query overlaps the start of the db entry
					acc << arrays.whole[i] << '\t' << q.whole  << "\tS" << endl;
				}else if(qendcmp == 1){
					//the query overlaps the end of the db entry
					acc << arrays.whole[i] << '\t' << q.whole  << "\tE" << endl;
				}else{
					//the query contains the db entry
					acc << arrays.whole[i] << '\t' << q.whole  << "\tC" << endl;
				}

				//generate data for the total file
				//note desc1 and desc2 here are the desc
				//values from the db file not the query file
				ostringstream table_key;
				table_key << arrays.desc1[i] << '\t' << arrays.desc2[i];

				//set a number of hits
				//or if we already encountered this, increase the number of hits
				//later we will conglomerate all this
				if(table.find(table_key.str()) == table.end()) {
					table[table_key.str()] = q.desc2;
				}else {
					table[table_key.str()] += q.desc2;
				}
			}
			outfile << acc.str();
		}
	}
	return;
}

void query_split(void *line) {
	qentry q;
	bool qstartcmp;
	bool qendcmp;
	ptr_entry arrays;
	ostringstream acc;
	istringstream in_stream(*reinterpret_cast<string*>(line));
	in_stream >> q.desc1 >> q.desc2 >> q.chr >> q.physical_start >>
		q.physical_end >> q.strand;

	if(in_stream.fail()) {
		cout << "Query File contains bad line, skipping: " <<
			*reinterpret_cast<string*>(line) << endl;

	}else {
		ostringstream out_stream;
		out_stream << q.desc1 << '\t' << q.desc2 << '\t' << q.chr << '\t' <<
			q.physical_start << '\t' << q.physical_end << '\t' << q.strand;

		q.whole = out_stream.str();
		string strand = strand_map[q.strand];
		if(strand == "") {
			strand = strand_map["dummy"];
		}
		if(db_strand.find(strand) != db_strand.end()) {
			if(db_strand[strand].find(q.chr) != db_strand[strand].end()) {
				size_t max = db_strand[strand][q.chr].dbwhole.size();
				arrays.desc1 = &db_strand[strand][q.chr].dbdesc1[0];
				arrays.desc2 = &db_strand[strand][q.chr].dbdesc2[0];
				arrays.physical_start = &db_strand[strand][q.chr].dbphysical_start[0];
				arrays.physical_end = &db_strand[strand][q.chr].dbphysical_end[0];
				arrays.found = &db_strand[strand][q.chr].dbfound[0];
				arrays.whole = &db_strand[strand][q.chr].dbwhole[0];
				for(size_t i = 0; i < max; i++) {
					if(arrays.physical_end[i] < q.physical_start
						|| arrays.physical_start[i] > q.physical_end)
						continue;
					arrays.found[i] += 1;

					qstartcmp = (q.physical_start >= arrays.physical_start[i]);
					qendcmp = (q.physical_end <= arrays.physical_end[i]);
					if(qstartcmp && qendcmp){
						acc << arrays.whole[i] << '\t' << q.whole  << "\tB" << endl;
						/* seq in QUERY within DB */
					}else if(qstartcmp == 1){
						acc << arrays.whole[i] << '\t' << q.whole  << "\tS" << endl;
					}else if(qendcmp == 1){
						acc << arrays.whole[i] << '\t' << q.whole  << "\tE" << endl;
					}else{
						acc << arrays.whole[i] << '\t' << q.whole  << "\tC" << endl;
						/* containment */
					}

					ostringstream table_key;
					table_key << arrays.desc1[i] << '\t' << arrays.desc2[i];

					if(table.find(table_key.str()) == table.end()) {
						table[table_key.str()] = q.desc2;
					}else {
						table[table_key.str()] += q.desc2;
					}
				}
				outfile << acc.str();
			}
		}
	}
	return;
}

void query_sf(void *line) {
	qentry q;
	bool qstartcmp;
	bool qendcmp;
	ptr_entry arrays;
	ostringstream acc;
	istringstream in_stream(*reinterpret_cast<string*>(line));
	in_stream >> q.desc1 >> q.desc2 >> q.chr >> q.physical_start >>
		q.physical_end >> q.strand;

	if(in_stream.fail()) {
		cout << "Query File contains bad line, skipping: " <<
			*reinterpret_cast<string*>(line) << endl;

	}else {
		ostringstream out_stream;
		out_stream << q.desc1 << '\t' << q.desc2 << '\t' << q.chr << '\t' <<
			q.physical_start << '\t' << q.physical_end << '\t' << q.strand;

		q.whole = out_stream.str();
		if(db_sense.find(q.chr) != db_sense.end()) {
			size_t max = db_sense[q.chr].dbwhole.size();
			arrays.desc1 = &db_sense[q.chr].dbdesc1[0];
			arrays.desc2 = &db_sense[q.chr].dbdesc2[0];
			arrays.physical_start = &db_sense[q.chr].dbphysical_start[0];
			arrays.physical_end = &db_sense[q.chr].dbphysical_end[0];
			arrays.found = &db_sense[q.chr].dbfound[0];
			arrays.strand = &db_sense[q.chr].dbstrand[0];
			arrays.whole = &db_sense[q.chr].dbwhole[0];
			for(size_t i = 0; i < max; i++) {
				if(arrays.physical_end[i] < q.physical_start
					|| arrays.physical_start[i] > q.physical_end)
					continue;

				arrays.found[i] += 1;
				qstartcmp = (q.physical_start >= arrays.physical_start[i]);
				qendcmp = (q.physical_end <= arrays.physical_end[i]);
				if(qstartcmp && qendcmp){
					acc << arrays.whole[i] << '\t' << q.whole  << "\tB";
					/* seq in QUERY within DB */
				}else if(qstartcmp == 1){
					acc << arrays.whole[i] << '\t' << q.whole  << "\tS";
				}else if(qendcmp == 1){
					acc << arrays.whole[i] << '\t' << q.whole  << "\tE";
				}else{
					acc << arrays.whole[i] << '\t' << q.whole  << "\tC";
					/* containment */
				}

				if(q.strand == arrays.strand[i]) {
					acc << "\tS\n";
				}else {
					acc << "\tA\n";
				}

				ostringstream table_key;
				table_key << arrays.desc1[i] << '\t' << arrays.desc2[i];

				if(table.find(table_key.str()) == table.end()) {
					table[table_key.str()] = q.desc2;
				}else {
					table[table_key.str()] += q.desc2;
				}
			}
			outfile << acc.str();
		}
	}
	return;
}

void query_ss(void *line) {
	qentry q;
	bool qstartcmp;
	bool qendcmp;
	ptr_entry arrays;
	ostringstream acc;
	ostringstream acc2;
	istringstream in_stream(*reinterpret_cast<string*>(line));
	in_stream >> q.desc1 >> q.desc2 >> q.chr >> q.physical_start >>
		q.physical_end >> q.strand;

	if(in_stream.fail()) {
		cout << "Query File contains bad line, skipping: " <<
			*reinterpret_cast<string*>(line) << endl;

	}else {
		ostringstream out_stream;
		out_stream << q.desc1 << '\t' << q.desc2 << '\t' << q.chr << '\t' <<
			q.physical_start << '\t' << q.physical_end << '\t' << q.strand;

		q.whole = out_stream.str();
		if(db_sense.find(q.chr) != db_sense.end()) {
			size_t max = db_sense[q.chr].dbwhole.size();
			arrays.desc1 = &db_sense[q.chr].dbdesc1[0];
			arrays.desc2 = &db_sense[q.chr].dbdesc2[0];
			arrays.physical_start = &db_sense[q.chr].dbphysical_start[0];
			arrays.physical_end = &db_sense[q.chr].dbphysical_end[0];
			arrays.found = &db_sense[q.chr].dbfound[0];
			arrays.strand = &db_sense[q.chr].dbstrand[0];
			arrays.whole = &db_sense[q.chr].dbwhole[0];
			for(size_t i = 0; i < max; i++) {
				if(arrays.physical_end[i] < q.physical_start
					|| arrays.physical_start[i] > q.physical_end)
					continue;

				qstartcmp = (q.physical_start >= arrays.physical_start[i]);
				qendcmp = (q.physical_end <= arrays.physical_end[i]);
				if(q.strand == arrays.strand[i]) {
					arrays.found[i] += 1;
					if(qstartcmp && qendcmp){
						acc << arrays.whole[i] << '\t' << q.whole  << "\tB" << endl;
						/* seq in QUERY within DB */
					}else if(qstartcmp == 1){
						acc << arrays.whole[i] << '\t' << q.whole  << "\tS" << endl;
					}else if(qendcmp == 1){
						acc << arrays.whole[i] << '\t' << q.whole  << "\tE" << endl;
					}else{
						acc << arrays.whole[i] << '\t' << q.whole  << "\tC" << endl;
						/* containment */
					}

					ostringstream table_key;
					table_key << arrays.desc1[i] << '\t' << arrays.desc2[i];

					if(table.find(table_key.str()) == table.end()) {
						table[table_key.str()] = q.desc2;
					}else {
						table[table_key.str()] += q.desc2;
					}
				}else {
					arrays.found[i] += 1;
					if(qstartcmp && qendcmp){
						acc2 << arrays.whole[i] << '\t' << q.whole  << "\tB" << endl;
						/* seq in QUERY within DB */
					}else if(qstartcmp == 1){
						acc2 << arrays.whole[i] << '\t' << q.whole  << "\tS" << endl;
					}else if(qendcmp == 1){
						acc2 << arrays.whole[i] << '\t' << q.whole  << "\tE" << endl;
					}else{
						acc2 << arrays.whole[i] << '\t' << q.whole  << "\tC" << endl;
						/* containment */
					}

					ostringstream table_key;
					table_key << arrays.desc1[i] << '\t' << arrays.desc2[i];

					if(table2.find(table_key.str()) == table2.end()) {
						table2[table_key.str()] = q.desc2;
					}else {
						table2[table_key.str()] += q.desc2;
					}
				}
			}
			outfile << acc.str();
			outfile2 << acc2.str();
		}
	}
	return;
}

void *add_zeros_ignore_strand(){

	//iterater over the chromasomes
	for(unordered_map<string, chr_entry>::iterator db_it = db.begin();
		db_it != db.end();
		db_it++
	){
		//get the size of a entry
		size_t max = db_it->second.dbwhole.size();
		ptr_entry arrays;
		string chr = db_it->first;	
		//point to the first entry	of each vector
		arrays.desc1 = &db_it->second.dbdesc1[0];
		arrays.desc2 = &db_it->second.dbdesc2[0];
		arrays.physical_start = &db_it->second.dbphysical_start[0];
		arrays.physical_end = &db_it->second.dbphysical_end[0];
		arrays.whole = &db_it->second.dbwhole[0];
		arrays.found = &db_it->second.dbfound[0];

		//iterate over our pointers
		for(size_t i = 0; i < max; i++) {
			if(!arrays.found[i]){
				totalfile << arrays.desc1[i] << '\t' << arrays.desc2[i] << '\t'
					<< "0" << endl;

				outfile << arrays.desc1[i] << '\t' << arrays.desc2[i] << '\t' <<
					chr << '\t' << arrays.physical_start[i] << '\t' <<
					arrays.physical_end[i] << "\t\t\t\t\t\t" << endl;
			}
		}
	}
}

void *add_zeros_strand(){

	//iterate over strands
	for(
		unordered_map<string, unordered_map<string, chr_entry> >::iterator
		  	db_strand_it = db_strand.begin();
		db_strand_it != db_strand.end();
		db_strand_it++
	){
		//might be able to form an iterator on db_strand_it->second
		//unordered_map<string, chr_entry> db_inner = db_strand[db_strand_it];
		unordered_map<string, chr_entry> *db_inner = &db_strand_it->second;
		//iterate over chromasomes
		string strand = db_strand_it->first;
		for(unordered_map<string, chr_entry>::iterator
			  	db_it = (*db_inner).begin();
			db_it != (*db_inner).end();
			db_it++
		){
			//get the size of a entry
			size_t max = db_it->second.dbwhole.size();
			ptr_entry arrays;
			string chr = db_it->first;	
	
			//could probably use second here too	
			//point to the first entry	
			arrays.desc1 = &db_it->second.dbdesc1[0];
			arrays.desc2 = &db_it->second.dbdesc2[0];
			arrays.physical_start = &db_it->second.dbphysical_start[0];
			arrays.physical_end = &db_it->second.dbphysical_end[0];
			arrays.whole = &db_it->second.dbwhole[0];
			arrays.found = &db_it->second.dbfound[0];

			//iterate over the vectors of the chr entries by pointer
			//note that's 6 or seven vectors at once, pointer math saves us from
			//declaring 6 or 7 iterators.
			for(size_t i = 0; i < max; i++) {
				if(!arrays.found[i]){
					totalfile << arrays.desc1[i] << '\t' << arrays.desc2[i] << '\t'
						<< "0" << endl;

					outfile << arrays.desc1[i] << '\t' << arrays.desc2[i] << '\t' <<
						chr << '\t' << arrays.physical_start[i] << '\t' <<
						arrays.physical_end[i] << '\t' << strand <<
						"\t\t\t\t\t\t\t" << endl;
				}
			}
		}
	}
}


void *add_zeros_sense(){

	//iterater over the chromasomes
	for(unordered_map<string, chr_entry_s>::iterator db_it = db_sense.begin();
		db_it != db_sense.end();
		db_it++
	){
		//get the size of a entry
		size_t max = db_it->second.dbwhole.size();
		ptr_entry arrays;
		string chr = db_it->first;	
	
		//point to the first entry	
		arrays.desc1 = &db_it->second.dbdesc1[0];
		arrays.desc2 = &db_it->second.dbdesc2[0];
		arrays.physical_start = &db_it->second.dbphysical_start[0];
		arrays.physical_end = &db_it->second.dbphysical_end[0];
		arrays.strand = &db_it->second.dbstrand[0];
		arrays.whole = &db_it->second.dbwhole[0];
		arrays.found = &db_it->second.dbfound[0];

		//iterate over our pointers
		for(size_t i = 0; i < max; i++) {
			if(!arrays.found[i]){
				totalfile  << arrays.desc1[i] << '\t' << arrays.desc2[i] << '\t'
					<< "0" << endl;

				totalfile2 << arrays.desc1[i] << '\t' << arrays.desc2[i] << '\t'
					<< "0" << endl;

				outfile << arrays.desc1[i] << '\t' << arrays.desc2[i] << '\t' <<
					chr << '\t' << arrays.physical_start[i] << '\t' <<
					arrays.physical_end[i] << '\t' << arrays.strand[i] <<
					"\t\t\t\t\t\t\t" << endl;

				outfile2 << arrays.desc1[i] << '\t' << arrays.desc2[i] << '\t' <<
					chr << '\t' << arrays.physical_start[i] << '\t' <<
					arrays.physical_end[i] << '\t' << arrays.strand[i] <<
					"\t\t\t\t\t\t\t" << endl;
			}
		}
	}
}

int main(int argc, char** args) {

	ifstream db_file, query_file;
	string line;
	string db_file_name, query_file_name, data_output_file_name;
	string total_output_file_name;
	string antisense_file_name;
	string nonsense_file_name;
	int opt;
	int temp_index;
	int option = 0;
	int no_zeros = 0;

	while(
		(opt = getopt_long(argc, args, "s:h", long_options, &temp_index)) != -1
	) {
		switch(opt) {
			case 'h':
				usage();
				return(0);
			case 's':
				if(!strcmp(optarg, "i")) {
					//ignore strand identifiers
					option = IGNORE_STRAND;
				}else if(!strcmp(optarg, "s")) {
					//read only same strand hits
					option = SAME_STRAND;
				}else if(!strcmp(optarg, "o")) {
					//read only opposite strand hits
					option = OPPOSITE_STRAND;
				}else if(!strcmp(optarg, "bf")) {
					//write sense and anti sense to one file
					option = SENSE;
				}else if(!strcmp(optarg, "bs")) {
					//write sense and anti sense to seperate files
					option = SENSE_SPLIT;
				}else {
					cout << "Error: \"" << optarg <<
						"\" is not a supported argument for the"
						<< " \"-s [ --strands ]\" option\n";

					usage();
					return(1);
				}
				break;
			case 'z':
				no_zeros = 1;
			default:
				usage();
				return(1);
		}
	}

	int arg_count = argc-optind;
	if(arg_count == 0) {
		cout << "Error: DB file name must be specified\n";
		usage();
		return(1);
	}else if(arg_count == 1) {
		cout << "Error: query file name must be specified\n";
		usage();
		return(1);
	}else if(arg_count == 2) {
		cout << "Error: output file name must be specified\n";
		usage();
		return(1);
	}

	db_file_name = args[optind];
	query_file_name = args[optind + 1];
	data_output_file_name = args[optind + 2];
	db_file.open(db_file_name.c_str());
	if(db_file.fail()) {
		cout << "Error: Could not open DB file \"" << db_file_name << "\"\n";
		return(1);
	}

	query_file.open(query_file_name.c_str());
	if(query_file.fail()) {
		cout << "Error: Could not open query file \"" << query_file_name << "\"\n";
		return(1);
	}

	switch(option) {
		case IGNORE_STRAND:
			cache_ignore_strand(db_file);
			break;
		case SAME_STRAND:
		case OPPOSITE_STRAND:
			cache_strand(db_file);
			break;
		default:
			cache_sense(db_file);
			break;
	}

	if(option != IGNORE_STRAND) {
		//if we care about strandedness, verify we have two strand identifiers
		//exiting on finding bad strand data is probably a better approach
		switch(strand_list.size()) {
			case IGNORE_STRAND:
				cerr << "Error: DB File does not contain a strand identifier column"
					<< endl;
				return(1);
			case SAME_STRAND:
				strand_list.insert("dummy");
				break;
			case OPPOSITE_STRAND:
				break;
			default:
				cerr << "Error: DB File contains more than two strand identifiers"
					<< endl;
				return(1);
		}

		switch(option) {
			case SAME_STRAND:
				strand_map[*strand_list.begin()] = *strand_list.begin();
				strand_map[*strand_list.rbegin()] = *strand_list.rbegin();
				break;
			case OPPOSITE_STRAND:
				strand_map[*strand_list.begin()] = *strand_list.rbegin();
				strand_map[*strand_list.rbegin()] = *strand_list.begin();
				break;
			case SENSE_SPLIT:
				antisense_file_name = data_output_file_name + "_antisense";
				nonsense_file_name = data_output_file_name + "_antisense";
				outfile2.open(antisense_file_name.c_str());
				if(outfile2.fail()) {
					cout << "Error: Could not create output file \"" <<
					  	antisense_file_name << "\"\n";

					return(1);
				}

				data_output_file_name += "_sense";
				break;
		}
	}

	outfile.open(data_output_file_name.c_str());
	if(outfile.fail()) {
		cout << "Error: Could not create output file \"" << data_output_file_name
		  	<< "\"\n";

		return(1);
	}

	//write out a header
	switch(option) {
		case IGNORE_STRAND:
			outfile << "db.desc1\tdb.desc2\tdb.chr\tdb.start\tdb.end"
				<< "\tq.desc1\tq.hits\tq.chr\tq.start\tq.end\tmatch\n";

			break;
		case SAME_STRAND:
		case OPPOSITE_STRAND:
		case SENSE:
			outfile << "db.desc1\tdb.desc2\tdb.chr\tdb.start\tdb.end\tdb.strand"
				<< "\tq.desc1\tq.hits\tq.chr\tq.start\tq.end\tq.strand\tmatch\n";
			break;
		case SENSE_SPLIT:
			outfile << "db.desc1\tdb.desc2\tdb.chr\tdb.start\tdb.end\tdb.strand"
				<< "\tq.desc1\tq.hits\tq.chr\tq.start\tq.end\tq.strand\tmatch\n";
			outfile2 << "db.desc1\tdb.desc2\tdb.chr\tdb.start\tdb.end\tdb.strand"
				<< "\tq.desc1\tq.hits\tq.chr\tq.start\tq.end\tq.strand\tmatch\n";
			break;
	}

	getline(query_file, line);
	while(!query_file.eof()) {
		switch(option) {
			case IGNORE_STRAND:
				query_ignore_strand(reinterpret_cast<void*>(&line));
				break;
			case SAME_STRAND:
			case OPPOSITE_STRAND:
				query_split(reinterpret_cast<void*>(&line));
				break;
			case SENSE:
				query_sf(reinterpret_cast<void*>(&line));
				break;
			case SENSE_SPLIT:
				query_ss(reinterpret_cast<void*>(&line));
				break;
		}
		getline(query_file, line);
	}

	string base_file_name;
	base_file_name = data_output_file_name + "_total";
	totalfile.open(base_file_name.c_str());
	if(totalfile.fail()) {
		cout << "Error: Could not create output file \"" << base_file_name <<
			"\"\n";

		return(1);
	}

	totalfile << "db_file.desc1\tdb_file.desc2\thits\n";
	for(map<string, long>::iterator table_iter = table.begin();
	  	table_iter != table.end();
		table_iter++
	) {
		totalfile << table_iter->first << '\t' << table_iter->second << endl;
	}



	if(option == SENSE_SPLIT) {
		base_file_name = total_output_file_name + "_total";
		totalfile2.open(base_file_name.c_str());
		if(totalfile2.fail()) {
			cout << "Error: Could not create output file \"" << base_file_name <<
				"\"\n";

			return(1);
		}
		totalfile2 << "db_file.desc1\tdb_file.desc2\thits";
		for(map<string, long>::iterator table_iter = table2.begin();
			table_iter != table2.end();
			table_iter++
		) {
			totalfile2 << table_iter->first << '\t' << table_iter->second << endl;
		}
	}

	if(no_zeros == 0){
		switch(option) {
			case IGNORE_STRAND:
				add_zeros_ignore_strand();
				break;
			case SAME_STRAND:
			case OPPOSITE_STRAND:
			case SENSE:
				add_zeros_strand();
				break;
			case SENSE_SPLIT:
				add_zeros_sense();
				break;
		}
	}

	if(option == SENSE_SPLIT) {
		totalfile2.close();
		outfile2.close();
	}
	totalfile.close();
	query_file.close();
	outfile.close();
	return(0);
}
