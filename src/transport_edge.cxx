#include "transport_edge.hxx"

Route::Route(std::string code, std::string name)
    : mCode(std::move(code)), mName(std::move(name)), mVehicle(Vehicle::SFC),
      mMovement(Movement::LOC) {}

Route::Route(std::string code, std::string name, const Vehicle &vehicle,
             const Movement &movement, const minutes &outDockSource,
             const minutes &inDockTarget, const minutes &loadingTime,
             const time_of_day &departure, const minutes &duration,
             const minutes &unloadingTime,
             const std::vector<uint8_t> &workingDays)
    : CostAttributes(outDockSource + loadingTime, departure, duration,
                     unloadingTime + inDockTarget, workingDays),
      mCode(std::move(code)), mName(std::move(name)), mVehicle(vehicle),
      mMovement(movement) {}

auto Route::code() const -> std::string { return mCode; }

auto Route::vehicle() const -> Vehicle { return mVehicle; }
