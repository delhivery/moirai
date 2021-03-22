#ifndef TYPEDEFS_HXX
#define TYPEDEFS_HXX

#include <compare>
#include <cstdint>
#include <ostream>

namespace ambasta {
// typedef int64_t TIMESTAMP;
// typedef int32_t MINUTES;
constexpr int16_t MINUTES_DIURNAL = 24 * 60;
typedef int16_t LEVY;
struct MINUTES;
struct TIME_OF_DAY;
struct TIMESTAMP;

struct MINUTES
{
private:
  int16_t m_value = 0;

public:
  MINUTES() = default;

  MINUTES(const int16_t);

  MINUTES(const TIMESTAMP&);

  static MINUTES max();

  static MINUTES min();

  MINUTES& operator=(const MINUTES&);

  explicit operator TIME_OF_DAY() const;

  explicit operator TIMESTAMP() const;

  // Arithmetic operators for MINUTES
  MINUTES operator+(const MINUTES&) const;

  MINUTES operator-(const MINUTES&) const;

  auto operator<=>(const MINUTES&) const = default;

  friend std::ostream& operator<<(std::ostream&, const MINUTES&);

  int16_t value() const;
};

struct TIME_OF_DAY
{
private:
  int16_t m_value = 0;

public:
  TIME_OF_DAY() = default;

  TIME_OF_DAY(const int16_t);

  TIME_OF_DAY(const MINUTES&);

  TIME_OF_DAY(const TIMESTAMP&);

  TIME_OF_DAY& operator=(const TIME_OF_DAY&);

  explicit operator MINUTES() const;

  MINUTES operator-(const TIME_OF_DAY&) const;

  TIME_OF_DAY operator+(const MINUTES&) const;

  TIME_OF_DAY operator-(const MINUTES&) const;
};

struct TIMESTAMP
{
private:
  int32_t m_value = 0;

public:
  TIMESTAMP() = default;

  TIMESTAMP(const int32_t);

  // For following functionality
  // MINUTES timestamp_minutes;
  // TIMESTAMP timestamp{timestamp_minutes};
  TIMESTAMP(const MINUTES&);

  static const TIMESTAMP& max();

  static const TIMESTAMP& min();

  // For following functionality
  // MINUTES timestamp_minutes;
  // TIMESTAMP timestamp = timestamp_minutes;
  TIMESTAMP& operator=(const MINUTES&);

  // For the following functionality
  // TIMESTAMP timestamp;
  // auto ts = (MINUTES)timestamp;
  explicit operator MINUTES() const;

  // Spaceship operator
  auto operator<=>(const TIMESTAMP&) const = default;

  // Stream operator
  friend std::ostream& operator<<(std::ostream&, const TIMESTAMP&);

  int16_t value() const;
};

enum Algorithm
{
  // Shortest path
  SHORTEST = 0,
  // Inverse shortest path
  INVERSE_SHORTEST = 1,
  // Shortest path with length constraint
  SHORTEST_CONSTRAINED = 2,
};
}
#endif
