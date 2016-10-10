#include <iostream>
#include <fstream>
#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/serialization/array.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>



int main(int argc, char const *argv[])
{
    std::ifstream in(argv[1],std::ios::binary);
    if(!in)
        return false;
    boost::iostreams::filtering_streambuf<boost::iostreams::input> fi;
    fi.push(boost::iostreams::gzip_decompressor());
    fi.push(in);
    std::istream rin(&fi);

    
    //r.seekg();
    //r.read ...
	return 0;
}