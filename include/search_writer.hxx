#pragma once

#include "blocking_queue.hxx"
#include "http.hxx"
#include <stop_token>
#include <string>

class SearchWriter {
private:
  moirai::Uri m_uri;
  const std::string m_username;
  const std::string m_password;
  const std::string m_search_index;
  BlockingQueue<std::string> &m_solution_queue;

public:
  SearchWriter(moirai::Uri uri, std::string search_user,
               std::string search_pass, std::string search_index,
               BlockingQueue<std::string> *solution_queue);

  void run(const std::stop_token &stop_token);
};
