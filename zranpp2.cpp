
// TODO
// same as zranpp but C++
// with continuing after seek
// note: read needs to consume some special left out data, and then continue with the block
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <algorithm>
#include "zlib.h"



class gzipseekableio 
{
public:
	static const int SPAN = 1048576L;       /* desired distance between access points */
	static const int WINSIZE = 32768U;      /* sliding window size */
	static const int CHUNK = 16384;  /* file input buffer size */
	/* access point entry */
	struct point {
	    off_t out;          /* corresponding offset in uncompressed data */
	    off_t in;           /* offset in input file of first full byte */
	    int bits;           /* number of bits (1-7) from byte at in - 1, or 0 */
	    unsigned char window[WINSIZE];  /* preceding 32K of uncompressed data */
	};

	/* access point list */
	struct access {
	    int have;           /* number of list entries filled in */
	    int size;           /* number of list entries allocated */
	    struct point *list; /* allocated list */
	};

	gzipseekableio(std::istream & in): in_(in) {}
	~gzipseekableio() { if(hasstrm_) inflateEnd(&strm); }

	/// loads the index
	bool loadindex(std::istream & ins); 

	/// saves the index
	void saveindex(std::ostream & ons) const;

	/// seeks to offset
	bool seek(off_t offset);

	/// goes forward skipping data
	//void seekfwd(off_t offset);

	int read(char * output, int size);

	void reset();

	int goodplace() const
	{
		return ((points_[points_.size() - 1].out << 1) / 3);
	}
private:
	std::istream & in_;
	std::vector<point> points_;
	bool hasstrm_ = false;
    z_stream strm;
    char input_[CHUNK];
};

int gzipseekableio::read(char * output, int size)
{
	int ret;
	if(!hasstrm_)
	{
		return 0;
	}
    strm.avail_out = size;
    strm.next_out = (Bytef*)output;

    /* uncompress until avail_out filled, or end of stream */
    do {
        if (strm.avail_in == 0) {
        	in_.read(input_,CHUNK);
            strm.avail_in = in_.gcount();
            if (!in_) {
            	ret = Z_ERRNO;
                goto extract_ret;
            }
            if (strm.avail_in == 0) {
                ret = Z_DATA_ERROR;
                goto extract_ret;
            }
            strm.next_in =  (Bytef*)input_; 
        }
        ret = inflate(&strm, Z_NO_FLUSH);       /* normal inflate */
        if (ret == Z_NEED_DICT)
            ret = Z_DATA_ERROR;
        if (ret == Z_MEM_ERROR || ret == Z_DATA_ERROR)
            goto extract_ret;
        if (ret == Z_STREAM_END)
            break;
    } while (strm.avail_out != 0);
extract_ret:
    return size-strm.avail_out;
}

void gzipseekableio::reset()
{
	if(hasstrm_)
	{
	    (void)inflateEnd(&strm);
	    hasstrm_ = false;		
	}
	in_.seekg(0);
}

bool gzipseekableio::seek(off_t offset)
{
    unsigned char discard[WINSIZE];
	int ret;
	bool skip;

    /* find where in stream to start */
    // TODO: use binary search
    #if 0
        struct point * here = &points_[0];
        ret = points_.size();
        while (--ret && here[1].out <= offset) // if next is lower than, then go
            here++;
    #else
        point p;
        p.out = offset;
        auto here = std::upper_bound(points_.begin(),points_.end(),p,             
           [](const point &lhs, const point & rhs) {                      return lhs.out < rhs.out;        });
        if(here == points_.end()) // last block
        {
            here = points_.begin() + (points_.size()-1);
            if(offset > here->out)
                return false;
        }
        else if(here == points_.begin()) // offset is less than first block
            return false;
        else if(here != points_.begin()) // previous with 
            here--;
    #endif
    std::cout << "ended " << offset << " " << here->out << " in " << here->in << " bits " << here->bits << "\n";

    /* initialize file and inflate state to start there */
	if(hasstrm_)
	{
	    (void)inflateEnd(&strm);
	    hasstrm_ = false;
	}

    memset(&strm,0,sizeof(strm));
    ret = inflateInit2(&strm, -15);         /* raw inflate */
    if (ret != Z_OK)
        return false;
	hasstrm_ = true;

    in_.seekg(here->in - (here->bits ? 1 : 0));
    if (!in_)
        goto extract_ret;

    if (here->bits) {
        ret = in_.get();
        if (ret == -1) {
            ret = !in_ ? Z_ERRNO : Z_DATA_ERROR;
            goto extract_ret;
        }
        (void)inflatePrime(&strm, here->bits, ret >> (8 - here->bits));
    }
    (void)inflateSetDictionary(&strm, here->window, WINSIZE);

    /* skip uncompressed bytes until offset reached, then satisfy request */
    offset -= here->out;
    strm.avail_in = 0;
     skip = true;                               /* while skipping to offset */
    do {
        /* define where to put uncompressed data, and how much */
        if (offset == 0 && skip) {          /* at offset now */
    		//read will do a full read
            strm.avail_out = 0;
            strm.next_out = 0;
            skip = false;         
            return true;
        }
        else if (offset > WINSIZE) {             /* skip WINSIZE bytes */
            strm.avail_out = WINSIZE;
            strm.next_out = discard;
            offset -= WINSIZE;
        }
        else if (offset != 0) {             /* last full skip */
            strm.avail_out = (unsigned)offset;
            strm.next_out = discard;
            offset = 0;
        }

        /* uncompress until avail_out filled, or end of stream */
        do {
            if (strm.avail_in == 0) {
            	in_.read(input_,CHUNK);
                strm.avail_in = in_.gcount();
                if (!in_) {
                	ret = Z_ERRNO;
                    goto extract_ret;
                }
                if (strm.avail_in == 0) {
                    ret = Z_DATA_ERROR;
                    goto extract_ret;
                }
                strm.next_in = (Bytef *)input_; 
            }
            ret = inflate(&strm, Z_NO_FLUSH);       /* normal inflate */
            if (ret == Z_NEED_DICT)
                ret = Z_DATA_ERROR;
            if (ret == Z_MEM_ERROR || ret == Z_DATA_ERROR)
                goto extract_ret;
            if (ret == Z_STREAM_END)
                break;
        } while (strm.avail_out != 0);

        /* if reach end of stream, then don't keep trying to get more */
        if (ret == Z_STREAM_END)
            break;
        /* do until offset reached and requested data read, or stream ends */
    } while (skip);

    // whatever the reason we have prepared the avail_in,

    /* compute number of uncompressed bytes read after offset */

    /* clean up and return bytes read or error */
extract_ret:
    strm.avail_out = 0;
    strm.next_out = 0;
    return false;
}

/*
skip data from current position (skipping blocks or skipping data with search)
void gzipseekableio::seekfwd(off_t offset)
{
	
}
*/

bool gzipseekableio::loadindex(std::istream & ins)
{
  int size = 0;
  ins.read((char*)&size,sizeof(size));
  points_.resize(size);
  ins.read((char*)&points_[0],sizeof(point)*size);
  return points_.size() && ins;
}

void gzipseekableio::saveindex(std::ostream & ons) const
{
	int size = points_.size();
	ons.write((const char*)&size,sizeof(size));
	ons.write((const char*)&points_[0],sizeof(point)*size);
}


#if 0
// convert this to have everything here

/* Add an entry to the access point list.  If out of memory, deallocate the
   existing list and return NULL. */
inline struct access *addpoint(struct access *index, int bits,
    off_t in, off_t out, unsigned left, unsigned char *window)
{
    struct point *next;

    /* if list is empty, create it (start with eight points) */
    if (index == NULL) {
        index = (struct access*)malloc(sizeof(struct access));
        if (index == NULL) return NULL;
        index->list = (struct point*)malloc(sizeof(struct point) << 3);
        if (index->list == NULL) {
            free(index);
            return NULL;
        }
        index->size = 8;
        index->have = 0;
    }

    /* if list is full, make it bigger */
    else if (index->have == index->size) {
        index->size <<= 1;
        next = (struct point*)realloc(index->list, sizeof(struct point) * index->size);
        if (next == NULL) {
            free_index(index);
            return NULL;
        }
        index->list = next;
    }

    /* fill in entry and increment how many we have */
    next = index->list + index->have;
    next->bits = bits;
    next->in = in;
    next->out = out;
    if (left)
        memcpy(next->window, window + WINSIZE - left, left);
    if (left < WINSIZE)
        memcpy(next->window + left, window, WINSIZE - left);
    index->have++;

    /* return list, possibly reallocated */
    return index;
}

/* Make one entire pass through the compressed stream and build an index, with
   access points about every span bytes of uncompressed output -- span is
   chosen to balance the speed of random access against the memory requirements
   of the list, about 32K bytes per access point.  Note that data after the end
   of the first zlib or gzip stream in the file is ignored.  build_index()
   returns the number of access points on success (>= 1), Z_MEM_ERROR for out
   of memory, Z_DATA_ERROR for an error in the input file, or Z_ERRNO for a
   file read error.  On success, *built points to the resulting index. */
inline int build_index(std::istream & ins, off_t span, struct access **built)
{
    int ret;
    off_t totin, totout;        /* our own total counters to avoid 4GB limit */
    off_t last;                 /* totout value of last access point */
    struct access *index;       /* access points being generated */
    z_stream strm;
    unsigned char input[CHUNK];
    unsigned char window[WINSIZE];

    /* initialize inflate */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;
    ret = inflateInit2(&strm, 47);      /* automatic zlib or gzip decoding */
    if (ret != Z_OK)
        return ret;

    /* inflate the input, maintain a sliding window, and build an index -- this
       also validates the integrity of the compressed data using the check
       information at the end of the gzip or zlib stream */
    totin = totout = last = 0;
    index = NULL;               /* will be allocated by first addpoint() */
    strm.avail_out = 0;
    do {
        /* get some compressed data from input file */
        //fread(input, 1, CHUNK, in);
        in.read((char*)input,CHUNK);
        strm.avail_in = in.gcount();
        if (!in) {
            ret = Z_ERRNO;
            goto build_index_error;
        }
        if (strm.avail_in == 0) {
            ret = Z_DATA_ERROR;
            goto build_index_error;
        }
        strm.next_in = input;

        /* process all of that, or until end of stream */
        do {
            /* reset sliding window if necessary */
            if (strm.avail_out == 0) {
                strm.avail_out = WINSIZE;
                strm.next_out = window;
            }

            /* inflate until out of input, output, or at end of block --
               update the total input and output counters */
            totin += strm.avail_in;
            totout += strm.avail_out;
            ret = inflate(&strm, Z_BLOCK);      /* return at end of block */
            totin -= strm.avail_in;
            totout -= strm.avail_out;
            if (ret == Z_NEED_DICT)
                ret = Z_DATA_ERROR;
            if (ret == Z_MEM_ERROR || ret == Z_DATA_ERROR)
                goto build_index_error;
            if (ret == Z_STREAM_END)
                break;

            /* if at end of block, consider adding an index entry (note that if
               data_type indicates an end-of-block, then all of the
               uncompressed data from that block has been delivered, and none
               of the compressed data after that block has been consumed,
               except for up to seven bits) -- the totout == 0 provides an
               entry point after the zlib or gzip header, and assures that the
               index always has at least one access point; we avoid creating an
               access point after the last block by checking bit 6 of data_type
             */
            if ((strm.data_type & 128) && !(strm.data_type & 64) &&
                (totout == 0 || totout - last > span)) {
                index = addpoint(index, strm.data_type & 7, totin,
                                 totout, strm.avail_out, window);
                if (index == NULL) {
                    ret = Z_MEM_ERROR;
                    goto build_index_error;
                }
                last = totout;
            }
        } while (strm.avail_in != 0);
    } while (ret != Z_STREAM_END);

    /* clean up and return index (release unused entries in list) */
    (void)inflateEnd(&strm);
    index = (struct access*)realloc(index, sizeof(struct point) * index->have);
    index->size = index->have;
    *built = index;
    return index->size;

    /* return error */
  build_index_error:
    (void)inflateEnd(&strm);
    if (index != NULL)
        free_index(index);
    return ret;
}
#endif

int main(int argc, char **argv)
{
	if(argc < 3)
		return -1;
	std::ifstream inf(argv[1],std::ios::binary);
	gzipseekableio io(inf);
	std::cout << "loadindex" << std::endl;
	std::ifstream iinf(argv[2],std::ios::binary);
	io.loadindex(iinf);
	std::cout << "seek at " << io.goodplace() << " " << gzipseekableio::CHUNK << std::endl;
	bool b;
    b = io.seek(1000000000);
    b = io.seek(0);
    b = io.seek(io.goodplace());
	std::cout << "seeked " << b << std::endl;
	char buf[gzipseekableio::CHUNK];
	int len = io.read(buf,sizeof(buf));
	std::cout << "read " << len << std::endl;
	if(argc > 3)
	{
		std::cout << "store to " << argv[3] << std::endl;
		std::ofstream onf(argv[3],std::ios::binary);
		onf.write(buf,len);
	}
	return 0;
}
