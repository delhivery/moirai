#ifndef MOIRAI_SERVER
#define MOIRAI_SERVER

#include "utils.hxx"
#include <Poco/Util/ServerApplication.h>
#include <filesystem>

class Moirai : public Poco::Util::ServerApplication
{
private:
#ifdef WITH_NODE_FILE
  std::filesystem::path mNodeFile;
#else
  std::string mNodeUri;
  std::string mNodeIndex;
  std::string mNodeAuthUser;
  std::string mNodeAuthPass;
#endif

#ifdef WITH_EDGE_FILE
  std::filesystem::path mEdgeFile;
#else
  std::string mEdgeUri;
  std::string mEdgeToken;
#endif

#ifdef WITH_LOAD_FILE
  std::filesystem::path mLoadFile;
#else
  // StringToStringMap topic_map;
  std::string mLoadTopic;
  std::vector<std::string> mLoadBrokerUris;
  uint8_t mLoadBatchSize;
  uint8_t mLoadConsumerTimeout;
#endif

#ifdef ENABLE_SYNC
  std::string mSyncUri;
  std::string mSyncIndex;
  std::string mSyncAuthUser;
  std::string mSyncAuthPass;
#endif

  bool mHelpRequested = false;

public:
  Moirai() = default;

  ~Moirai() override = default;

private:
  void display_help();

protected:
  void initialize(Poco::Util::Application& self) override;

  void uninitialize() override;

  void defineOptions(Poco::Util::OptionSet& optionset) override;

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

  auto main(const ArgVec& args) -> int override;
};

#endif
