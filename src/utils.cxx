#include "utils.hxx"
#include <Poco/Base64Encoder.h>
#include <algorithm>
#include <sstream>
// #include <fmt/format.h>

/*
std::string
indexAndTypeToPath(const std::string& search_index, const std::string& doc_type)
{
  return fmt::format("/{}/{}/", search_index, doc_type);
}

std::string
indexAndTypeToPath(const std::string& search_index,
                   const std::string& doc_type,
                   const std::string& doc_id)
{
  return fmt::format("/{}/{}/{}/", search_index, doc_type, doc_id);
}
*/

auto
getEncodedCredentials(std::string_view username, std::string_view password)
  -> std::string
{
  std::ostringstream stringStream;
  Poco::Base64Encoder encoder(stringStream);
  encoder.rdbuf()->setLineLength(0);
  encoder << username << ":" << password;
  encoder.close();
  return stringStream.str();
}

void
to_lower(std::string& input_string)
{
  std::transform(input_string.begin(),
                 input_string.end(),
                 input_string.begin(),
                 [](unsigned char c) { return std::tolower(c); });
}
