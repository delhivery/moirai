#ifndef MOIRAI_UTILS
#define MOIRAI_UTILS

#include <boost/bimap.hpp>
#include <string>

typedef boost::bimap<std::string, std::string> StringToStringMap;

std::string
indexAndTypeToPath(const std::string&, const std::string&);

std::string
indexAndTypeToPath(const std::string&, const std::string&, const std::string&);

std::string
getEncodedCredentials(const std::string&, const std::string&);

#endif
