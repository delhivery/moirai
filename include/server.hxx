#ifndef MOIRAI_SERVER
#define MOIRAI_SERVER

#include "utils.hxx"
#include <Poco/Util/ServerApplication.h>
#include <string>
#include <vector>

class Moirai : public Poco::Util::ServerApplication
{
private:
  std::vector<std::string> broker_url;
  StringToStringMap topic_map;
  uint16_t batch_size;
  uint16_t timeout;

  std::string search_uri;
  std::string search_user;
  std::string search_pass;
  std::string search_index;

  std::string facility_timings_filename;
  std::string facility_uri;
  std::string facility_token;

  std::string route_uri;
  std::string route_token;

  bool help_requested;

public:
  Moirai() {}

  ~Moirai() {}

private:
  void display_help();

protected:
  void initialize(Poco::Util::Application&);

  void uninitialize();

  void reinitialize();

  void defineOptions(Poco::Util::OptionSet&);

  void set_facility_api(const std::string&, const std::string&);

  void set_facility_api_token(const std::string&, const std::string&);

  void set_route_api(const std::string&, const std::string&);

  void set_route_token(const std::string&, const std::string&);

  void set_search_uri(const std::string&, const std::string&);

  void set_search_username(const std::string&, const std::string&);

  void set_search_password(const std::string&, const std::string&);

  void set_search_index(const std::string&, const std::string&);

  void set_batch_timeout(const std::string&, const std::string&);

  void set_batch_size(const std::string&, const std::string&);

  void set_edge_topic(const std::string&, const std::string&);

  void set_node_topic(const std::string&, const std::string&);

  void set_load_topic(const std::string&, const std::string&);

  void set_broker_url(const std::string&, const std::string&);

  void set_facility_timings_file(const std::string&, const std::string&);

  void handle_help(const std::string&, const std::string&);

  int main(const ArgVec&);
};

#endif
