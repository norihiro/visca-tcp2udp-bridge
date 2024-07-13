#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/detail/file_parser_error.hpp>
#include <string>
#include <cstdio>
#include "tcp2udp.h"

using namespace boost::property_tree;

bool load_ptree(prop_t &pt, const char *fname)
{
	try {
		read_json(fname, pt);
		return true;
	} catch (json_parser_error &e) {
		fprintf(stderr, "Error: %s\n", e.what());
		return false;
	}
}
