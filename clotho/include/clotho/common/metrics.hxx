#ifndef CLOTHO_METRICS_HXX
#define CLOTHO_METRICS_HXX

#include <memory>
#include <unordered_map>
namespace clotho {
struct Metric
{
  std::string m_name;
  std::unordered_map<std::string, std::string> labels;

  enum MetricType
  {
    RATE,
    COUNT,
    GAUGE,
    COUNTER,
    TIMESTAMP,
    SUMMARY,
    HISTOGRAM
  };

  Metric(std::string, MetricType, std::string);

  virtual double value() const = 0;

  inline std::string name() const;

  virtual void finalize_label() = 0;

  void add_label(std::string, std::string);
};

struct MetricCounter : public Metric
{
  MetricCounter(std::string, std::string);

  MetricCounter(std::string,
                std::string,
                const std::unordered_map<std::string, std::string>&);

  void finalize_label() override;

  virtual double value() const;

  inline MetricCounter& operator++();

  inline MetricCounter& operator+=(double);
};

struct MetricStreamingLatency : public Metric
{
  MetricStreamingLatency();

  void finalize_label() override;

  inline void add_event_time(int64_t, int64_t);

  virtual double value() const;
};
}
#endif
