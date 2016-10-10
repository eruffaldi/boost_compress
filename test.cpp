/**
 * Comparison and Benchmarking using Boost Iostreams: LZ4 and ZLib
 */
#include <iostream>
#include <fstream>
#include <chrono>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filter/counter.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream.hpp>
#include "lz4_filter.hpp"

using namespace std;

namespace bio = boost::iostreams;
namespace ext { namespace bio = ext::boost::iostreams; }


bool load(std::string filename, std::vector<uint8_t> &buf)
{
	std::ifstream t(filename.c_str(),std::ios::binary);
	t.seekg(0, std::ios::end);
	size_t size = t.tellg();
	buf.resize(size);
	t.seekg(0);
	t.read((char*)&buf[0], size); 	
	return (bool)t;
}
int main(int argc, char** argv){
	if( 1 == argc ){
		cerr << "USAGE: " << endl
		     << "\t" << argv[0] << " -f FILE - preloads file" << endl
		     << "\t" << argv[0] << " -c   - compress STDIN to STDOUT lz4" << endl
		     << "\t" << argv[0] << " -d   - decompress STDIN to STDOUT lz4" << endl
		     << "\t" << argv[0] << " -C   - compress STDIN to STDOUT zlib" << endl
		     << "\t" << argv[0] << " -D   - decompress STDIN to STDOUT zlib" << endl
		     << "\t" << argv[0] << " -M   - pass" << endl
		     ;
		return 1;
	}

	if( 2 != strlen(argv[1])){
		cerr << "error: invalid argument length!" << endl;
		return 2;
	}

	if( '-' != argv[1][0] ){
		cerr << "error: invalid argument!" << endl;
		return 3;
	}

    bio::filtering_ostream bifo;
    std::vector<uint8_t> buf;


    if( argv[1][1] == 'f' )
    {
    	load(argv[2],buf);
    	std::cerr << "loaded " << argv[2] << " " << buf.size() << std::endl;
    	argv+=2;
    	argc -= 2;
    }

	if( '-' != argv[1][0] ){
		cerr << "error: invalid argument!" << endl;
		return 3;
	}
    std::cerr << "go! " << argv[1] << std::endl;

	switch(argv[1][1]){
		case 'c':
			bifo.push( ext::bio::lz4_compressor() );
			break;
		case 'd':
			bifo.push( ext::bio::lz4_decompressor() );
			break;
		case 'C':
			bifo.push( bio::zlib_compressor() );
			break;
		case 'D':
			bifo.push( bio::zlib_decompressor() );
			break;
		case 'M':
			break;
		default:
			cerr << "error: invalid argument!" << endl;
			return 3;
	}
    bio::basic_array_source<char> input_source((char*)&buf[0], buf.size());
    bio::stream<bio::basic_array_source<char> > input_stream(input_source);


	auto start = std::chrono::high_resolution_clock::now();
    bifo.push(bio::counter());
    bifo.push( cout );
    bifo.exceptions( std::ifstream::eofbit | std::ifstream::failbit | std::ifstream::badbit );
    boost::iostreams::copy(buf.size() == 0 ? cin : input_stream, bifo,64*1024);
    //bifo.flush();

	auto end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> diff = end-start;
	auto n = bifo.component<1, bio::counter>()->characters() ;
	if(buf.size() != 0)
		std::cerr << "Insize " << buf.size() << std::endl;
	std::cerr << "Outsize "  << n << " ints : " << diff.count() << " s " << (n/diff.count())/1E6 << " MB/s \n";
	if(buf.size() != 0)
			std::cerr << "Ratio "  << buf.size()/(double)n << std::endl;

	return 0;
}