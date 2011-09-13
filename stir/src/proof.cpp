/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * STIR -- Sifteo Tiled Image Reducer
 * M. Elizabeth Scott <beth@sifteo.com>
 *
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#include "proof.h"
#include "lodepng.h"

namespace Stir {

void Base64Encode(const std::vector<uint8_t> &data, std::string &out)
{
    /*
     * A very simple MIME Base64 encoder.
     */

    static const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    for (unsigned i = 0; i < data.size(); i += 3) {
	unsigned len = data.size() - i;
	uint8_t b1 = data[i];
	uint8_t b2 = len >= 2 ? data[i+1] : 0;
	uint8_t b3 = len >= 3 ? data[i+2] : 0;
	uint32_t word = (b1 << 16) | (b2 << 8) | b3;

	out.append(1, charset[(word >> (6*3)) & 63]);
	out.append(1, charset[(word >> (6*2)) & 63]);
	out.append(1, len >= 2 ? charset[(word >> (6*1)) & 63] : '=');
	out.append(1, len >= 3 ? charset[(word >> (6*0)) & 63] : '=');
    }
}

void DataURIEncode(const std::vector<uint8_t> &data, const std::string &mime, std::string &out)
{
    /*
     * Encode a data: URI. with the specified data and MIME type.
     */

    out.append("data:");
    out.append(mime);
    out.append(";base64,");
    Base64Encode(data, out);
}

void TileURIEncode(const Tile &t, std::string &out)
{
    /*
     * Encode a tile image as a data URI
     */

    LodePNG::Encoder encoder;
    std::vector<uint8_t> png;
    std::vector<uint8_t> image;

    image.reserve(4 * Tile::SIZE * Tile::SIZE);
    for (unsigned y = 0; y < Tile::SIZE; y++)
	for (unsigned x = 0; x < Tile::SIZE; x++) {
	    RGB565 color = t.pixel(x, y);
	    image.push_back(color.red());
	    image.push_back(color.green());
	    image.push_back(color.blue());
	    image.push_back(0xFF);
	}

    encoder.encode(png, &image[0], Tile::SIZE, Tile::SIZE);
    DataURIEncode(png, "image/png", out);
}

std::string HTMLEscape(const std::string &s)
{
    std::string out;

    for (unsigned i = 0; i < s.length(); i++) {
	char c = s[i];
 
	if (c == '&')
	    out += "&amp;";
	else if (c == '>')
	    out += "&gt;";
	else if (c == '<')
	    out += "&lt;";
	else
	    out.append(1, c);
    }
    
    return out;
}


ProofWriter::ProofWriter(Logger &log, const char *filename)
    : mLog(log)
{
    if (filename) {
	mStream.open(filename);
	if (!mStream.is_open())
	    log.error("Error opening proof file '%s'", filename);
    }
    
    if (mStream.is_open()) {
	mStream.setf(std::ios::fixed, std::ios::floatfield);  
	mStream.precision(2);
	
	head();
    }
}

void ProofWriter::head()
{
    unsigned ts = Tile::SIZE;

    mStream <<
	"<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \n"
	"    \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n"
	"<html xmlns=\"http://www.w3.org/1999/xhtml\">\n"
	"<head>\n"
	"  <script>\n"
	"\n"
	"     function tileRef(i) { \n"
	"       document.write('<img src=\"' + poolPrefix + pool[i] + '\" width=\""<<ts<<"\" height=\""<<ts<<"\" />'); \n"
	"     } \n"
	"\n"
	"     function tileRange(begin, end) { \n"
	"       for (var i = begin; i < end; i++) \n"
	"         tileRef(i); \n"
	"     } \n"
	"\n"
	"     function tileArray(arr) { \n"
	"       for (var i = 0; i < arr.length; i++) \n"
	"         tileRef(arr[i]); \n"
	"     } \n"
	"\n"
	"  </script>\n"
	"  <style>\n"
	"\n"
	"    body { \n"
	"      color: #eee; \n"
	"      background: #222; \n"
        "      font-family: verdana, tahoma, helvetica, arial, sans-serif; \n"
	"      font-size: 12px; \n"
	"      margin: 10px 5px 50px 5px; \n"
	"    } \n"
	"\n"
	"    div.grid { \n"
	"      float: left; \n"
	"    } \n"
	"\n"
	"    div.clear { \n"
	"      clear: both; \n"
	"    } \n"
	"\n"
	"    h1 { \n"
	"      background: #fff; \n"
	"      color: #222; \n"
	"      font-size: 22px; \n"
	"      font-weight: normal; \n"
	"      clear: both; \n"
	"      padding: 10px; \n"
	"      margin: 0; \n"
	"    } \n"
	"\n"
	"    h2 { \n"
	"      color: #fff; \n"
	"      font-size: 16px; \n"
	"      font-weight: normal; \n"
	"      clear: both; \n"
	"      padding: 10px; \n"
	"      margin: 0; \n"
	"    } \n"
	"\n"
	"    p { \n"
	"      padding: 0 10px; \n"
	"    } \n"
	"\n"
	"  </style>\n"
	"</head>\n"
	"<body>\n";
}
 
void ProofWriter::writeGroup(const Group &group)
{
    if (!mStream.is_open())
	return;

    mLog.taskBegin("Generating proof");
    
    mStream <<
	"<h1>" << HTMLEscape(group.getName()) << "</h1>\n"
	"<p>\n"
	       << group.getPool().size() << " tiles, "
	       << group.getLoadstream().size() / 1024.0 << " kB stream\n"
	"</p>\n";

    defineTiles(group.getPool());

    tileRange(0, group.getPool().size());

    for (std::set<Image*>::iterator i = group.getImages().begin();
	 i != group.getImages().end(); i++) {

	Image *image = *i;

	mStream << "<h2>" << HTMLEscape(image->getName()) << "</h2>\n";

	for (std::vector<TileGrid>::const_iterator j = image->getGrids().begin();
	     j != image->getGrids().end(); j++) {

	    tileGrid(*j);
	}
    }

    mLog.taskEnd();
}

void ProofWriter::close()
{
    if (!mStream.is_open())
	return;

    mStream <<
	"<div class=\"clear\" />\n"
	"</body></html>\n";

    mStream.close();
}

void ProofWriter::defineTiles(const TilePool &pool)
{
    /*
     * Define a set of tile data, in Javascript. Tiles are represented
     * as a common prefix (to optimize out the PNG header) plus an
     * array of tile-specific suffixes.
     */

    if (!pool.size())
	return;

    std::vector<std::string> uri(pool.size());

    for (unsigned i = 0; i < pool.size(); i++)
	TileURIEncode(*pool.tile(i), uri[i]);

    // Find the longest common prefix
    unsigned prefixLen = uri[0].length();
    for (unsigned i = 0; i < pool.size(); i++)
	while (prefixLen && uri[i].compare(0, prefixLen, uri[0], 0, prefixLen))
	    prefixLen--;

    mStream << 
	"<script>\n"
	"poolPrefix = \"" << uri[0].substr(0, prefixLen) << "\";\n"
	"pool = [\n";

    for (unsigned i = 0; i < pool.size(); i++)
	mStream << "\"" << uri[i].substr(prefixLen) << "\",\n";

    mStream << 
	"];\n"
	"</script>\n";
}

void ProofWriter::tileRange(unsigned begin, unsigned end)
{
    mStream << "<script>tileRange(" << begin << ", " << end << ");</script>\n";
}

void ProofWriter::tileGrid(const TileGrid &grid)
{
    unsigned pixelW = grid.width() * Tile::SIZE;
    unsigned pixelH = grid.height() * Tile::SIZE;

    mStream <<
	"<div class=\"grid\" style=\"width: "<<pixelW<<"px; height: "<<pixelH<<"px;\" >\n"
	"  <script>\n"
	"    tileArray([\n";
    
    for (unsigned y = 0; y < grid.height(); y++)
	for (unsigned x = 0; x < grid.width(); x++) {
	    TilePool::Index index = grid.getPool().index(grid.tile(x, y));
	    mStream << index << ",";
	}

    mStream <<
	"\n"
	"    ]);\n"
	"  </script>\n"
	"</div>\n";
}


};  // namespace Stir
