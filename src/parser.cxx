#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <rapidjson/document.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <string>

void
read_json(std::filesystem::path filename)
{
  std::ifstream file_stream{ filename, std::ios::in | std::ios::binary };
  assert(file_stream.is_open());

  rapidjson::IStreamWrapper file_stream_wrapper{ file_stream };

  rapidjson::Document document{};
  document.ParseStream(file_stream_wrapper);

  rapidjson::StringBuffer buffer{};
  rapidjson::Writer<rapidjson::StringBuffer> writer{ buffer };
  document.Accept(writer);

  assert(!document.HasParseError());

  const std::string json_string{ buffer.GetString() };

  std::cout << "JSON: " << json_string << std::endl;
}
