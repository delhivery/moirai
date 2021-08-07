#include <format.hxx>
#include <moirai.hxx>
#include <version.hxx>

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
