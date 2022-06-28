#ifndef TRANSPORT_EDGE
#define TRANSPORT_EDGE

#include "edge.hxx"
#include "hash.hxx"
#include "transport.hxx"
#include <string>

class Route : public CostAttributes {
private:
  std::string mCode;
  std::string mName;

  Vehicle mVehicle;
  Movement mMovement;

public:
  Route(std::string, std::string);

  Route(std::string,                 // code
        std::string,                 // name
        const Vehicle &,             // vehicle
        const Movement &,            // movement
        const minutes &,             // outDockSource
        const minutes &,             // inDockTarget
        const minutes &,             // loadingTime
        const time_of_day &,         // departure
        const minutes &,             // duration
        const minutes &,             // unloadingTime
        const std::vector<uint8_t> & // workingDays
  );

  [[nodiscard]] auto code() const -> std::string;

  [[nodiscard]] auto vehicle() const -> Vehicle;

  friend auto std::hash<Route>::operator()(const Route &) const -> size_t;
};

#endif
