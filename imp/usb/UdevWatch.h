#include <string>
#include <vector>
#include <functional>

class UdevWatch {
public:
    typedef std::function<void(const std::string, bool)> tNodeListener;

    UdevWatch(tNodeListener listener);
    ~UdevWatch();

    bool startPollingThread();
    void cancelPolling();
    // std::vector<Device> enumerateDevices(bool triggerEvents);

private:
    bool createUDevCtx();

private:
    tNodeListener m_listener;
    struct udev *udev;
    struct udev_monitor *monitor;
    int fd;
};



