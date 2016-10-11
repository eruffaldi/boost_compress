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
#include <boost/iostreams/device/null.hpp>
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
	bool none = false;
    std::cerr << "go! " << argv[1] << std::endl;

	switch(argv[1][1]){
		case 'c':
		std::cerr << "LZ4 compress\n";
			bifo.push( ext::bio::lz4_compressor() );
			break;
		case 'd':
		std::cerr << "LZ4 decompress\n";
			bifo.push( ext::bio::lz4_decompressor() );
			break;
		case 'C':
		std::cerr << "ZLIB compress\n";
			bifo.push( bio::zlib_compressor() );
			break;
		case 'D':
		std::cerr << "ZLIB decompress\n";
			bifo.push( bio::zlib_decompressor() );
			break;
		case 'M':
		std::cerr << "nothing\n";
			none = true;
			break;
		default:
			cerr << "error: invalid argument!" << endl;
			return 3;
	}
    bio::basic_array_source<char> input_source((char*)&buf[0], buf.size());
    bio::stream<bio::basic_array_source<char> > input_stream(input_source);


	auto start = std::chrono::high_resolution_clock::now();
    bifo.push(bio::counter());
    boost::iostreams::stream< boost::iostreams::null_sink > nullOstream( ( boost::iostreams::null_sink() ) );

    bifo.push( nullOstream );
    bifo.exceptions( std::ifstream::eofbit | std::ifstream::failbit | std::ifstream::badbit );
    int insize = 0;
    bool withcounter = true;
    const int bufsize = 64*1024;
    if(buf.size() == 0)
    {
	    bio::filtering_istream bifi;
	    bifi.push(std::cin);
	    if(withcounter)
	    	bifi.push(bio::counter());
	    boost::iostreams::copy(bifi, bifo,bufsize);
	    insize = withcounter ? bifi.component<0, bio::counter>()->characters() : -1;
		auto end = std::chrono::high_resolution_clock::now();
    }
    else
    {    	
	    boost::iostreams::copy(input_stream, bifo,bufsize);
    	insize = buf.size();
    }
    auto outsize = withcounter ? (none ? bifo.component<0, bio::counter>()->characters() : bifo.component<1, bio::counter>()->characters() ) : -1;
	auto end = std::chrono::high_resolution_clock::now();

	std::chrono::duration<double> diff = end-start;
	std::cerr << "Insize " << insize << std::endl;
	std::cerr << "Outsize "  << outsize << std::endl;
	std::cerr << "Ratio "  << insize/(double)outsize <<":1" << std::endl;
	std::cerr << "Duration "  << diff.count() <<  std::endl;
	std::cerr << "Inspeed  "  <<  (insize/diff.count())/1E6 << " MB/s" << std::endl;
	std::cerr << "Outspeed "  <<  (outsize/diff.count())/1E6 << " MB/s" << std::endl;

	return 0;
}