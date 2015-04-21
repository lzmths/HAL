#pragma once

#include <functional>

#include <HAL/Devices/DriverInterface.h>

#include <HAL/Messages/Pose.pb.h>


namespace hal {

typedef std::function<void (pb::PoseMsg&)> PosysDriverDataCallback;

///
/// Generic Posys Driver Interface
class PosysDriverInterface : public DriverInterface {
 public:
  virtual ~PosysDriverInterface() {}
  virtual void RegisterPosysDataCallback(PosysDriverDataCallback callback) = 0;
  virtual bool IsRunning() const = 0;
};

} /* namespace hal */
