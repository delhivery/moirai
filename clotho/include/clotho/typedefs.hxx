#ifndef TYPEDEFS_HXX
#define TYPEDEFS_HXX

#include <compare>
#include <cstdint>
#include <ostream>

// typedef int64_t TIMESTAMP;
// typedef int32_t TIMESTAMP_MINUTES;
typedef int16_t LEVY;
struct MINUTES;
struct TIME_OF_DAY;
struct TIMESTAMP;
struct TIMESTAMP_MINUTES;

struct MINUTES
{
private:
  int16_t value = 0;

public:
  MINUTES() = default;

  MINUTES(const int16_t);

  MINUTES& operator=(const MINUTES&);

  explicit operator TIME_OF_DAY() const;

  MINUTES operator+(const MINUTES&) const;

  MINUTES operator-(const MINUTES&) const;
};

struct TIME_OF_DAY
{
private:
  int16_t value = 0;

public:
  TIME_OF_DAY() = default;

  TIME_OF_DAY(const int16_t);

  TIME_OF_DAY(const TIMESTAMP_MINUTES&);

  TIME_OF_DAY(const TIMESTAMP);

  TIME_OF_DAY& operator=(const TIME_OF_DAY&);

  explicit operator MINUTES() const;

  TIME_OF_DAY operator+(const TIME_OF_DAY&) const;

  TIME_OF_DAY operator-(const TIME_OF_DAY&) const;

  TIME_OF_DAY operator+(const MINUTES&) const;

  TIME_OF_DAY operator-(const MINUTES&) const;
};

struct TIMESTAMP
{
private:
  int16_t value = 0;

public:
  TIMESTAMP() = default;

  static TIMESTAMP max();

  static TIMESTAMP min();

  // For following functionality
  // TIMESTAMP_MINUTES timestamp_minutes;
  // TIMESTAMP timestamp{timestamp_minutes};
  TIMESTAMP(const TIMESTAMP_MINUTES&);

  // For following functionality
  // TIMESTAMP_MINUTES timestamp_minutes;
  // TIMESTAMP timestamp = timestamp_minutes;
  TIMESTAMP& operator=(const TIMESTAMP_MINUTES&);

  // For the following functionality
  // TIMESTAMP timestamp;
  // auto ts = (TIMESTAMP_MINUTES)timestamp;
  explicit operator TIMESTAMP_MINUTES() const;

  // Arithmetic operators for TIMESTAMP
  TIMESTAMP_MINUTES operator+(const MINUTES&) const;

  TIMESTAMP_MINUTES operator-(const MINUTES&) const;

  // Spaceship operator
  auto operator<=>(const TIMESTAMP&) const = default;

  // Stream operator
  friend std::ostream& operator<<(std::ostream&, const TIMESTAMP&);
};

struct TIMESTAMP_MINUTES
{
private:
  int16_t value = 0;

public:
  TIMESTAMP_MINUTES() = default;

  TIMESTAMP_MINUTES(const TIMESTAMP&);

  static TIMESTAMP_MINUTES max();

  static TIMESTAMP_MINUTES min();

  TIMESTAMP& operator=(const TIMESTAMP_MINUTES&);

  explicit operator TIMESTAMP() const;

  // Arithmetic operators for TIMESTAMP_MINUTES
  TIMESTAMP_MINUTES operator+(const MINUTES&) const;

  TIMESTAMP_MINUTES operator-(const MINUTES&) const;

  auto operator<=>(const TIMESTAMP_MINUTES&) const = default;

  friend std::ostream& operator<<(std::ostream&, const TIMESTAMP_MINUTES&);
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

#endif
