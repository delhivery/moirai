#include <clotho/typedefs.hxx>
#include <iostream>
#include <limits>
#include <thread>

namespace ambasta {
MINUTES::MINUTES(const int16_t minutes)
  : m_value(minutes)
{}

// TODO - Should check if timestapm exceeds max value and set output to max
MINUTES::MINUTES(const TIMESTAMP& other)
  : m_value(other.value() / 60)
{}

MINUTES
MINUTES::max()
{
  return MINUTES(std::numeric_limits<int16_t>::max());
}

MINUTES
MINUTES::min()
{
  return MINUTES(std::numeric_limits<int16_t>::min());
}

MINUTES&
MINUTES::operator=(const MINUTES& other)
{
  if (this != &other)
    m_value = other.m_value;
  return *this;
}

MINUTES::operator TIME_OF_DAY() const
{
  return TIME_OF_DAY{ *this };
}

MINUTES::operator TIMESTAMP() const
{
  return TIMESTAMP{ m_value * 60 };
}

// TODO should check for overflow
MINUTES
MINUTES::operator+(const MINUTES& other) const
{
  return MINUTES(m_value + other.m_value);
}

// TODO check for underflow
MINUTES
MINUTES::operator-(const MINUTES& other) const
{
  return MINUTES(m_value - other.m_value);
}

std::ostream&
operator<<(std::ostream& os, const MINUTES& minutes)
{
  os << minutes.m_value << std::endl;
  return os;
}

int16_t
MINUTES::value() const
{
  return m_value;
}

TIME_OF_DAY::TIME_OF_DAY(const int16_t minutes)
  : m_value(minutes)
{

  if (m_value > MINUTES_DIURNAL or m_value < 0)
    m_value = m_value % MINUTES_DIURNAL;
  if (m_value < 0)
    m_value += MINUTES_DIURNAL;
}

TIME_OF_DAY::TIME_OF_DAY(const MINUTES& other)
  : TIME_OF_DAY(other.value())
{}

TIME_OF_DAY::TIME_OF_DAY(const TIMESTAMP& other)
  : TIME_OF_DAY((MINUTES)other)
{}

TIME_OF_DAY&
TIME_OF_DAY::operator=(const TIME_OF_DAY& other)
{
  if (this != &other)
    m_value = other.m_value;
  return *this;
}

TIME_OF_DAY::operator MINUTES() const
{
  return MINUTES{ m_value };
}

MINUTES
TIME_OF_DAY::operator-(const TIME_OF_DAY& other) const
{
  int16_t delta = m_value - other.m_value;

  if (delta < 0)
    delta += MINUTES_DIURNAL;
  return MINUTES{ delta };
}

TIME_OF_DAY
TIME_OF_DAY::operator+(const MINUTES& other) const
{
  return TIME_OF_DAY(m_value + other.value());
}

TIME_OF_DAY
TIME_OF_DAY::operator-(const MINUTES& other) const
{
  return TIME_OF_DAY(m_value - other.value());
}

TIMESTAMP::TIMESTAMP(const int32_t timestamp)
  : m_value(timestamp)
{}

TIMESTAMP::TIMESTAMP(const MINUTES& other)
  : m_value(other.value() * 60)
{}

TIMESTAMP&
TIMESTAMP::operator=(const MINUTES& other)
{
  m_value = other.value() * 60;
  return *this;
}

const TIMESTAMP&
TIMESTAMP::max()
{
  static const TIMESTAMP max_value{ std::numeric_limits<int32_t>::max() };
  return max_value;
}

const TIMESTAMP&
TIMESTAMP::min()
{
  static const TIMESTAMP min_value{ std::numeric_limits<int32_t>::min() };
  return min_value;
}

TIMESTAMP::operator MINUTES() const
{
  return MINUTES{ static_cast<int16_t>(m_value / 60) };
}

std::ostream&
operator<<(std::ostream& os, const TIMESTAMP& other)
{
  os << other.m_value << std::endl;
  return os;
}
}
