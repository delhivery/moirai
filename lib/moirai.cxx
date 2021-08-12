#include <moirai/format.hxx>
#include <moirai/moirai.hxx>
#include <moirai/version.hxx>

const char*
project()
{
  return PROJECT_NAME;
}

const char*
version()
{
  return PROJECT_VERSION_STRING;
}

std::string
usage()
{
  return std::format("{}-{}", project(), version());
}
