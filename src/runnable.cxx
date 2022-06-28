#include "runnable.hxx"

auto Runnable::stop() const -> bool { return mStop; }

void Runnable::stop(bool stop) { mStop = stop; }
