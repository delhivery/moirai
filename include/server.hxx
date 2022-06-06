#ifndef MOIRAI_SERVER
#define MOIRAI_SERVER

#include "utils.hxx"
#include <Poco/Util/ServerApplication.h>
#include <filesystem>
#include <string>
#include <vector>

class Moirai : public Poco::Util::ServerApplication
{
private:
#ifdef WITH_NODE_FILE
  std::filesyste::path file_nodes;
#else
  std::string node_sync_uri, node_sync_idx, node_sync_user, node_sync_pass;
#endif

#ifdef WITH_EDGE_FILE
  std::filesystem::path file_edges;
#else
  std::string edge_uri, edge_token;
#endif

#ifdef WITH_LOAD_FILE
  std::filesystem::path file_loads;
#else
  // StringToStringMap topic_map;
  std::string load_topic;
  std::vector<std::string> load_broker_uris;
  uint8_t load_batch_size, load_consumer_timeout;
#endif

#ifdef ENABLE_SYNC
  std::string sync_uri, sync_idx, sync_user, sync_pass;
#endif

  bool help_requested = false;

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

#ifdef WITH_NODE_FILE
  void set_node_file(const std::string&, const std::string&);
#else
  void set_node_uri(const std::string&, const std::string&);
  void set_node_idx(const std::string&, const std::string&);
  void set_node_user(const std::string&, const std::string&);
  void set_node_pass(const std::string&, const std::string&);
#endif

#ifdef WITH_EDGE_FILE
  void set_edge_file(const std::string&, const std::string&);
#else
  void set_edge_uri(const std::string&, const std::string&);
  void set_edge_auth(const std::string&, const std::string&);
#endif

#ifdef WITH_LOAD_FILE
  void set_load_file(const std::string&, const std::string&);
#else
  void set_load_broker_timeout(const std::string&, const std::string&);
  void set_load_broker_size(const std::string&, const std::string&);
  void set_load_broker_uri(const std::string&, const std::string&);
  void set_load_topic(const std::string&, const std::string&);

  // void set_edge_topic(const std::string&, const std::string&);
  // void set_node_topic(const std::string&, const std::string&);
#endif

#ifdef ENABLE_SYNC
  void set_sync_uri(const std::string&, const std::string&);
  void set_sync_idx(const std::string&, const std::string&);
  void set_sync_user(const std::string&, const std::string&);
  void set_sync_pass(const std::string&, const std::string&);
#endif

  void handle_help(const std::string&, const std::string&);

  int main(const ArgVec&);
};

#endif
