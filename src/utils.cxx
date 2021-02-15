#include "utils.hxx"
#include "format.hxx"
#include <Poco/Base64Encoder.h>

std::string
indexAndTypeToPath(const std::string& search_index, const std::string& doc_type)
{
  return std::format("/{}/{}/", search_index, doc_type);
}

std::string
indexAndTypeToPath(const std::string& search_index,
                   const std::string& doc_type,
                   const std::string& doc_id)
{
  return std::format("/{}/{}/{}/", search_index, doc_type, doc_id);
}

std::string
getEncodedCredentials(const std::string& username, const std::string& password)
{
  std::ostringstream out_stringstream;
  Poco::Base64Encoder encoder(out_stringstream);
  encoder.rdbuf()->setLineLength(0);
  encoder << username << ":" << password;
  encoder.close();
  return out_stringstream.str();
}

void
to_lower(std::string& input_string)
{
  std::transform(input_string.begin(),
                 input_string.end(),
                 input_string.begin(),
                 [](unsigned char c) { return std::tolower(c); });
}
