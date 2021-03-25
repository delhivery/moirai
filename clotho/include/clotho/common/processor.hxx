#ifndef CLOTHO_PRODUCER_PROCESSOR_HXX
#define CLOTHO_PRODUCER_PROCESSOR_HXX

#include <clotho/common/event_consumer.hxx>
#include <clotho/common/metrics.hxx>
#include <clotho/common/record.hxx>
#include <filesystem>
#include <iterator>
#include <memory>
#include <vector>

namespace clotho {

class Processor
{
protected:
  std::vector<Metric*> m_metrics;
  MetricCounter m_processed_count;
  MetricStreamingLatency m_latency;

  Processor();

public:
  virtual ~Processor();

  virtual bool good() const;

  virtual bool eof() const = 0;

  virtual void close() = 0;

  const std::vector<Metric*>& get_metrics() const;

  int64_t get_metric(std::string);

  virtual void flush() = 0;

  inline std::string record_type_name() const;

  virtual std::string key_type_name() const = 0;

  virtual std::string value_type_name() const = 0;

  virtual std::string log_name() const = 0;

  virtual size_t process(int64_t) = 0;

  virtual size_t queue_size() const = 0;

  virtual int64_t next_event_time() const = 0;

  virtual size_t outbound_queue_length() const;

  virtual void poll(int);

  virtual void punctuate(int64_t);

  virtual void gc(int64_t);

  virtual std::string topic() const;

  virtual std::string precondition_topic() const;

  void add_metrics_label(std::string, std::string);

  void add_metric(Metric*);
};

class PartitionProcessor : public Processor
{
protected:
  std::vector<PartitionProcessor*> m_upstream;
  const int32_t m_partition;

  PartitionProcessor(PartitionProcessor*, int32_t);

  void add_upstream(PartitionProcessor*);

public:
  virtual ~PartitionProcessor();

  size_t depth() const;

  virtual bool eof() const;

  virtual void close();

  inline int32_t partition() const;

  virtual void flush();

  virtual void start(int64_t);

  virtual void commit(bool) = 0;

  bool is_upstream(const PartitionProcessor*) const;
};

template<class K, class V>
class PartitionSink
  : public EventConsumer<K, V>
  , public PartitionProcessor
{
protected:
  PartitionSink(int32_t);

public:
  typedef K key_type;
  typedef V value_type;

  std::string key_type_name() const override;

  std::string value_type_name() const override;

  virtual int64_t next_event_ts() const;

  size_t queue_size() const override;
};

template<class PK, class CODEC>
inline uint32_t
get_partition_hash(const PK&,
                   std::shared_ptr<CODEC> = std::make_shared<CODEC>());

template<class PK, class CODEC>
inline uint32_t
get_partition_hash(const PK&,
                   size_t,
                   std::shared_ptr<CODEC> = std::make_shared<CODEC>());

template<class K, class V>
class TopicSink
  : public EventConsumer<K, V>
  , public Processor
{
public:
  typedef K key_type;
  typedef V value_type;

  std::string key_type_name() const override;

  std::string value_type_name() const override;

  size_t queue_size() const override;

  virtual int64_t next_event_ts() const;
};

template<class K, class V>
class PartitionSource : public PartitionProcessor
{
public:
  using SinkFunction =
    typename std::function<void(std::shared_ptr<Event<K, V>>)>;

protected:
  std::vector<SinkFunction> _sinks;

  virtual void send_to_sinks(std::shared_ptr<Event<K, V>>);

public:
  PartitionSource(PartitionProcessor&, int32_t);

  std::string key_type_name() const override;

  std::string value_type_name() const override;

  template<class SINK>
  typename std::enable_if<std::is_base_of<PartitionSink<K, V>, SINK>::value,
                          void>::type
  add_sink(SINK*);

  template<class SINK>
  typename std::enable_if<std::is_base_of<PartitionSink<K, V>, SINK>::value,
                          void>::type add_sink(std::shared_ptr<SINK>);

  template<class SINK>
  typename std::enable_if<std::is_base_of<TopicSink<K, V>, SINK>::value,
                          void>::type add_sink(std::shared_ptr<SINK>);

  void add_sink(SinkFunction);
};

namespace detail {};

template<class K, class V>
class MaterializedSourceIteratorImpl
{
public:
  virtual ~MaterializedSourceIteratorImpl();

  virtual void next() = 0;

  virtual std::shared_ptr<const Record<K, V>> item() const = 0;

  virtual bool valid() const = 0;

  virtual bool operator==(const MaterializedSourceIteratorImpl&) const = 0;

  bool operator!=(const MaterializedSourceIteratorImpl&) const;
};

namespace detail {
template<class K, class V>
class Iterator
  : public std::iterator<std::forward_iterator_tag,
                         std::shared_ptr<const Record<K, V>>,
                         long,
                         std::shared_ptr<const Record<K, V>>*,
                         std::shared_ptr<const Record<K, V>>>
{
  std::shared_ptr<MaterializedSourceIteratorImpl<K, V>> m_impl;

public:
  explicit Iterator(std::shared_ptr<MaterializedSourceIteratorImpl<K, V>>);

  Iterator& operator++();

  Iterator operator++(int);

  bool operator==(const Iterator&) const;

  bool operator!=(const Iterator&) const;

  std::shared_ptr<const Record<K, V>> operator*() const;
};
}

template<class K, class V>
class MaterializedSource : public PartitionSource<K, V>
{
public:
  virtual detail::Iterator<K, V> begin() const = 0;

  virtual detail::Iterator<K, V> end() const = 0;

  virtual std::shared_ptr<const Record<K, V>> get(const K&) const = 0;

  MaterializedSource(PartitionProcessor*, int32_t);

  virtual std::filesystem::path get_storage_path(std::filesystem::path);
};

}
#endif
