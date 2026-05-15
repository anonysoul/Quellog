#ifndef QUELLOG_DEVICE_STATE_H_
#define QUELLOG_DEVICE_STATE_H_

enum DeviceState {
    kDeviceStateUnknown = 0,
    kDeviceStateStarting,
    kDeviceStateIdle,
    kDeviceStateRefreshing,
    kDeviceStateSettings,
    kDeviceStateError
};

#endif  // QUELLOG_DEVICE_STATE_H_
