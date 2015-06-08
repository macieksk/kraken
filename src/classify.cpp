/*
 * Copyright 2013-2015, Derrick Wood <dwood@cs.jhu.edu>
 *
 * This file is part of the Kraken taxonomic sequence classification system.
 *
 * Kraken is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Kraken is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Kraken.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "kraken_headers.hpp"
#include "krakendb.hpp"
#include "krakenutil.hpp"
#include "quickfile.hpp"
#include "seqreader.hpp"
#include <boost/algorithm/string/trim.hpp>

#include <iterator>
#include <numeric>
#include <map>
#include <utility>

const size_t DEF_WORK_UNIT_SIZE = 500000;

using namespace std;
using namespace kraken;

void parse_command_line(int argc, char **argv);
void usage(int exit_code=EX_USAGE);
void process_file(char *filename);
void classify_sequence(DNASequence &dna, ostringstream &koss,
                       ostringstream &coss, ostringstream &uoss);
string hitlist_string(vector<uint32_t> &taxa, vector<uint8_t> &ambig);
set<uint32_t> get_ancestry(uint32_t taxon);
void report_stats(struct timeval time1, struct timeval time2);

int Num_threads = 1;
string DB_filename, Index_filename, Nodes_filename, Spaced_seed;
const char * Spaced_seed_cstr;
bool Original_assignment_algorithm = false;
bool Quick_mode = false;
bool Fastq_input = false;
bool Print_classified = false;
bool Print_unclassified = false;
bool Print_kraken = true;
bool Populate_memory = false;
bool Only_classified_kraken_output = false;
uint32_t Minimum_hit_count = 1;
map<uint32_t, uint32_t> Parent_map;
KrakenDB Database;
string Classified_output_file, Unclassified_output_file, Kraken_output_file;
ostream *Classified_output;
ostream *Unclassified_output;
ostream *Kraken_output;
size_t Work_unit_size = DEF_WORK_UNIT_SIZE;

uint64_t total_classified = 0;
uint64_t total_sequences = 0;
uint64_t total_bases = 0;

int main(int argc, char **argv) {
  #ifdef _OPENMP
  omp_set_num_threads(1);
  #endif

  parse_command_line(argc, argv);
  if (! Nodes_filename.empty())
    Parent_map = build_parent_map(Nodes_filename);

  if (Populate_memory)
    cerr << "Loading database... ";

  QuickFile db_file;
  db_file.open_file(DB_filename);
  if (Populate_memory)
    db_file.load_file();
  Database = KrakenDB(db_file.ptr());
  //KmerScanner::set_k(Database.get_k()); //changed to seed span
  KmerScanner::set_k(Spaced_seed.length());

  QuickFile idx_file;
  idx_file.open_file(Index_filename);
  if (Populate_memory)
    idx_file.load_file();
  KrakenDBIndex db_index(idx_file.ptr());
  Database.set_index(&db_index);

  if (Populate_memory)
    cerr << "complete." << endl;

  if (Print_classified) {
    if (Classified_output_file == "-")
      Classified_output = &cout;
    else
      Classified_output = new ofstream(Classified_output_file.c_str());
  }

  if (Print_unclassified) {
    if (Unclassified_output_file == "-")
      Unclassified_output = &cout;
    else
      Unclassified_output = new ofstream(Unclassified_output_file.c_str());
  }

  if (! Kraken_output_file.empty()) {
    if (Kraken_output_file == "-")
      Print_kraken = false;
    else
      Kraken_output = new ofstream(Kraken_output_file.c_str());
  }
  else
    Kraken_output = &cout;

  struct timeval tv1, tv2;
  gettimeofday(&tv1, NULL);
  for (int i = optind; i < argc; i++)
    process_file(argv[i]);
  gettimeofday(&tv2, NULL);

  report_stats(tv1, tv2);

  return 0;
}

void report_stats(struct timeval time1, struct timeval time2) {
  time2.tv_usec -= time1.tv_usec;
  time2.tv_sec -= time1.tv_sec;
  if (time2.tv_usec < 0) {
    time2.tv_sec--;
    time2.tv_usec += 1000000;
  }
  double seconds = time2.tv_usec;
  seconds /= 1e6;
  seconds += time2.tv_sec;

  cerr << "\r";
  fprintf(stderr, 
          "%llu sequences (%.2f Mbp) processed in %.3fs (%.1f Kseq/m, %.2f Mbp/m).\n",
          (unsigned long long) total_sequences, total_bases / 1.0e6, seconds,
          total_sequences / 1.0e3 / (seconds / 60),
          total_bases / 1.0e6 / (seconds / 60) );
  fprintf(stderr, "  %llu sequences classified (%.2f%%)\n",
          (unsigned long long) total_classified, total_classified * 100.0 / total_sequences);
  fprintf(stderr, "  %llu sequences unclassified (%.2f%%)\n",
          (unsigned long long) (total_sequences - total_classified),
          (total_sequences - total_classified) * 100.0 / total_sequences);
}

void process_file(char *filename) {
  string file_str(filename);
  DNASequenceReader *reader;
  DNASequence dna;

  if (Fastq_input)
    reader = new FastqReader(file_str);
  else
    reader = new FastaReader(file_str);

  #pragma omp parallel
  {
    vector<DNASequence> work_unit;
    ostringstream kraken_output_ss, classified_output_ss, unclassified_output_ss;

    while (reader->is_valid()) {
      work_unit.clear();
      size_t total_nt = 0;
      #pragma omp critical(get_input)
      {
        while (total_nt < Work_unit_size) {
          dna = reader->next_sequence();
          if (! reader->is_valid())
            break;
          work_unit.push_back(dna);
          total_nt += dna.seq.size();
        }
      }
      if (total_nt == 0)
        break;
      
      kraken_output_ss.str("");
      classified_output_ss.str("");
      unclassified_output_ss.str("");
      for (size_t j = 0; j < work_unit.size(); j++)
        classify_sequence( work_unit[j], kraken_output_ss,
                           classified_output_ss, unclassified_output_ss );

      #pragma omp critical(write_output)
      {
        if (Print_kraken)
          (*Kraken_output) << kraken_output_ss.str();
        if (Print_classified)
          (*Classified_output) << classified_output_ss.str();
        if (Print_unclassified)
          (*Unclassified_output) << unclassified_output_ss.str();
        total_sequences += work_unit.size();
        total_bases += total_nt;
        cerr << "\rProcessed " << total_sequences << " sequences (" << total_bases << " bp) ...";
      }
    }
  }  // end parallel section

  delete reader;
}

void classify_sequence(DNASequence &dna, ostringstream &koss,
                       ostringstream &coss, ostringstream &uoss) {
  vector<uint32_t> taxa, taxa_rc;  //taxa for sense and anti-sense strand
  vector<uint8_t> ambig_list;
  map<uint32_t, uint32_t> hit_counts, hit_counts_rc;
  KmerScanner::base_type_in *kmer_ptr;
  KmerScanner::base_type_in rev_kmer;
  uint32_t taxon = 0, taxon_rc = 0;
  uint32_t hits = 0, hits_rc = 0;  // only maintained if in quick mode

  uint64_t current_bin_key1,current_bin_key2;
  int64_t current_min_pos1 = 1,current_min_pos2 = 1;
  int64_t current_max_pos1 = 0,current_max_pos2 = 0;

  uint64_t kmer_squashed;

  if (dna.seq.size() >= KmerScanner::get_k()) {
    KmerScanner scanner(dna.seq);
    while ((kmer_ptr = scanner.next_kmer()) != NULL) {
      taxon = taxon_rc = 0;
      if (scanner.ambig_kmer()) {
        ambig_list.push_back(1);
      }
      else {
        ambig_list.push_back(0);

        KmerScanner::squash_kmer_for_read(Spaced_seed_cstr,*kmer_ptr,kmer_squashed);

		uint32_t *val_ptr = Database.kmer_query(
							  //Database.canonical_representation(*kmer_ptr),
		                      kmer_squashed,
							  &current_bin_key1,
							  &current_min_pos1, &current_max_pos1
							);
		taxon = val_ptr ? *val_ptr : 0;

		if (taxon) {
		  hit_counts[taxon]++;
		  if (Quick_mode && ++hits >= Minimum_hit_count)
			break;
		}

		///////////////////// Anti-sense strand
		rev_kmer = KmerScanner::reverse_complement(*kmer_ptr);
		KmerScanner::squash_kmer_for_read(Spaced_seed_cstr,rev_kmer,kmer_squashed);

		val_ptr = Database.kmer_query(
							  kmer_squashed,
							  &current_bin_key2,
							  &current_min_pos2, &current_max_pos2
							);
		taxon_rc = val_ptr ? *val_ptr : 0;

		if (taxon_rc) {
		  hit_counts_rc[taxon_rc]++;
		  if (Quick_mode && ++hits_rc >= Minimum_hit_count)
			break;
		}
      }
      taxa.push_back(taxon);
      taxa_rc.push_back(taxon_rc);
    }
  }

  uint32_t call = 0, call_rc = 0;
  auto map_acc = [](const size_t prev, const std::pair<uint32_t,uint32_t>& p)
    		       { return prev + p.second; };
		       
  if (Quick_mode) {
    call = hits >= Minimum_hit_count ? taxon : 0;
    call_rc = hits_rc >= Minimum_hit_count ? taxon_rc : 0;
  } else if (Original_assignment_algorithm){
      	auto copy_elem = [&](const std::pair<uint32_t,uint32_t>& p)
    		            { hit_counts[p.first] += p.second; };
	std::for_each(std::begin(hit_counts_rc), std::end(hit_counts_rc),copy_elem);
	hits = std::accumulate(std::begin(hit_counts), std::end(hit_counts),0,map_acc);	
	call = resolve_tree(hit_counts, Parent_map);	
	hits_rc = -1;
	call_rc = 0;
  } else {	
	hits = std::accumulate(std::begin(hit_counts), std::end(hit_counts),0,map_acc);
	hits_rc = std::accumulate(std::begin(hit_counts_rc), std::end(hit_counts_rc),0,map_acc);
	if (hits >= hits_rc) {
	    call = resolve_tree(hit_counts, Parent_map);
	    call_rc = 0;
	} else {
	    call_rc = resolve_tree(hit_counts_rc, Parent_map);
	    call = 0;
	}
  }
  //Choose the call with more hits
  call = hits >= hits_rc ? call : call_rc;

  if (call)
    #pragma omp atomic
    total_classified++;

  if (Print_unclassified || Print_classified) {
    ostringstream *oss_ptr = call ? &coss : &uoss;
    bool print = call ? Print_classified : Print_unclassified;
    if (print) {
      if (Fastq_input) {
        (*oss_ptr) << "@" << dna.header_line << endl
            << dna.seq << endl
            << "+" << endl
            << dna.quals << endl;
      }
      else {
        (*oss_ptr) << ">" << dna.header_line << endl
            << dna.seq << endl;
      }
    }
  }

  if (! Print_kraken)
    return;

  if (call) {
    koss << "C\t";
  }
  else {
    if (Only_classified_kraken_output)
      return;
    koss << "U\t";
  }
  koss << dna.id << "\t" << call << "\t" << dna.seq.size() << "\t";

  if (Quick_mode) {
    koss << "Q:" << hits;
  }
  else {
    if (taxa.empty())
      koss << "0:0";
    else
      koss << hitlist_string(taxa, ambig_list);

    koss << '|';  //Mark end of sense/beginning of anti-sense output

    if (taxa_rc.empty())
      koss << "0:0";
    else
      koss << hitlist_string(taxa_rc, ambig_list);
  }

  koss << endl;
}

string hitlist_string(vector<uint32_t> &taxa, vector<uint8_t> &ambig)
{
  int64_t last_code;
  int code_count = 1;
  ostringstream hitlist;

  if (ambig[0])   { last_code = -1; }
  else            { last_code = taxa[0]; }

  for (size_t i = 1; i < taxa.size(); i++) {
    int64_t code;
    if (ambig[i]) { code = -1; }
    else          { code = taxa[i]; }

    if (code == last_code) {
      code_count++;
    }
    else {
      if (last_code >= 0) {
        hitlist << last_code << ":" << code_count << " ";
      }
      else {
        hitlist << "A:" << code_count << " ";
      }
      code_count = 1;
      last_code = code;
    }
  }
  if (last_code >= 0) {
    hitlist << last_code << ":" << code_count;
  }
  else {
    hitlist << "A:" << code_count;
  }
  return hitlist.str();
}

set<uint32_t> get_ancestry(uint32_t taxon) {
  set<uint32_t> path;

  while (taxon > 0) {
    path.insert(taxon);
    taxon = Parent_map[taxon];
  }
  return path;
}

void parse_command_line(int argc, char **argv) {
  int opt;
  long long sig;

  if (argc > 1 && strcmp(argv[1], "-h") == 0)
    usage(0);
  while ((opt = getopt(argc, argv, "d:i:t:u:n:m:o:qfcC:U:MaZ:")) != -1) {
    switch (opt) {
      case 'd' :
        DB_filename = optarg;
        break;
      case 'i' :
        Index_filename = optarg;
        break;
      case 't' :
        sig = atoll(optarg);
        if (sig <= 0)
          errx(EX_USAGE, "can't use nonpositive thread count");
        #ifdef _OPENMP
        if (sig > omp_get_num_procs())
          errx(EX_USAGE, "thread count exceeds number of processors");
        Num_threads = sig;
        omp_set_num_threads(Num_threads);
        #endif
        break;
      case 'n' :
        Nodes_filename = optarg;
        break;
      case 'q' :
        Quick_mode = true;
        break;
      case 'm' :
        sig = atoll(optarg);
        if (sig <= 0)
          errx(EX_USAGE, "can't use nonpositive minimum hit count");
        Minimum_hit_count = sig;
        break;
      case 'f' :
        Fastq_input = true;
        break;
      case 'c' :
        Only_classified_kraken_output = true;
        break;
      case 'C' :
        Print_classified = true;
        Classified_output_file = optarg;
        break;
      case 'U' :
        Print_unclassified = true;
        Unclassified_output_file = optarg;
        break;
      case 'o' :
        Kraken_output_file = optarg;
        break;
      case 'u' :
        sig = atoll(optarg);
        if (sig <= 0)
          errx(EX_USAGE, "can't use nonpositive work unit size");
        Work_unit_size = sig;
        break;
      case 'M' :
        Populate_memory = true;
        break;
      case 'a' :
        Original_assignment_algorithm = true;
        break;	
      case 'Z' :
        Spaced_seed = optarg;
        boost::algorithm::trim(Spaced_seed);
        cerr << endl<<"SpacedSeed: "<<Spaced_seed <<endl;
        Spaced_seed_cstr = Spaced_seed.c_str();
        break;
      default:
        usage();
        break;
    }
  }

  if (DB_filename.empty()) {
    cerr << "Missing mandatory option -d" << endl;
    usage();
  }
  if (Index_filename.empty()) {
    cerr << "Missing mandatory option -i" << endl;
    usage();
  }
  if (Nodes_filename.empty() && ! Quick_mode) {
    cerr << "Must specify one of -q or -n" << endl;
    usage();
  }
  if (Spaced_seed.empty() || Spaced_seed.length()<=0) {
      cerr << "Must specify spaced seed -Z" << endl;
      usage();
  }
  if (optind == argc) {
    cerr << "No sequence data files specified" << endl;
  }
}

void usage(int exit_code) {
  cerr << "Usage: classify [options] <fasta/fastq file(s)>" << endl
       << endl
       << "Options: (*mandatory)" << endl
       << "* -d filename      Kraken DB filename" << endl
       << "* -i filename      Kraken DB index filename" << endl
	   << "* -Z spaced_seed   Spaced seed, where match is denoted by #, eg. ###__#_###_###" << endl
       << "  -n filename      NCBI Taxonomy nodes file" << endl
       << "  -o filename      Output file for Kraken output" << endl
       << "  -t #             Number of threads" << endl
       << "  -u #             Thread work unit size (in bp)" << endl
       << "  -q               Quick operation" << endl
       << "  -m #             Minimum hit count (ignored w/o -q)" << endl
       << "  -C filename      Print classified sequences" << endl
       << "  -U filename      Print unclassified sequences" << endl
       << "  -f               Input is in FASTQ format" << endl
       << "  -c               Only include classified reads in output" << endl
       << "  -M               Preload database files" << endl
       << "  -a               Original assignment algorithm" << endl
       << "  -h               Print this message" << endl
       << endl
       << "At least one FASTA or FASTQ file must be specified." << endl
       << "Kraken output is to standard output by default." << endl;
  exit(exit_code);
}
