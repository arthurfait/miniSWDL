#include "UdevWatch.h"
#include <libudev.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <fnmatch.h>
#include <glib.h>
#include <signal.h>

#include <algorithm>
#include <functional>

#include "utils.h"

#define __TRACE__

PlatformUdevDevice::PlatformUdevDevice(tPathListener listener)
    : m_listener(listener)
    , udev(NULL)
    , monitor(NULL)
    , fd(-1)
{
    __TRACE__;
}

PlatformUdevDevice::~PlatformUdevDevice()
{
    if (monitor)
        udev_monitor_unref(monitor);
    if (udev)
        udev_unref(udev);
    __TRACE__;
}


bool PlatformUdevDevice::startPollingThread()
{
    if (!createUDevCtx())
        return false;

    // netlink: kernel, udev
    // Applications should usually not connect directly to the
    // "kernel" events, because the devices might not be useable
    // at that time, before udev has configured them, and created device nodes.
    // Accessing devices at the same time as udev,
    // might result in unpredictable behavior.
    // The "udev" events are sent out after udev has finished
    // its event processing, all rules have been processed,
    // and needed device nodes are created.
    monitor = udev_monitor_new_from_netlink(udev, "kernel");
    if (NULL == monitor) {
        log_error("failed to create monitor");
        return false;
    }
    // type see: ls /sys/class
    udev_monitor_filter_add_match_subsystem_devtype(monitor, "block", NULL);
    // udev_monitor_filter_add_match_subsystem_devtype(monitor, "usb", NULL);
    udev_monitor_enable_receiving(monitor);

    fd = udev_monitor_get_fd(monitor);
    startPollFd();
    return true;
}

void PlatformUdevDevice::startPollFd()
{
    const guint CHECKOUT_PERIOD = 100;
    g_timeout_add(CHECKOUT_PERIOD, dev_poll_timeout_cb, this);
}

// void dump_udev_device(struct udev_device* dev)
// {
//     const char* source = udev_device_get_devnode(dev);
//     const char* devpath = udev_device_get_devpath(dev);
//     const char* subsystem = udev_device_get_subsystem(dev);
//     const char* devtype = udev_device_get_devtype(dev);
//     const char* syspath = udev_device_get_syspath(dev);
//     const char* sysname = udev_device_get_sysname(dev);
//     const char* sysnum = udev_device_get_sysnum(dev);
//     const char* devnode = udev_device_get_devnode(dev);

//     log_info("=dump START=");
//     if (source) {
//         log_info("source: %s", source);
//     }
//     if (devpath) {
//         log_info("devpath: %s", devpath);
//     }


//     log_info("=porop list START=");
//     struct udev_list_entry *iter;
//     struct udev_list_entry *plist =
//         udev_device_get_properties_list_entry(dev);
//     udev_list_entry_foreach(iter, plist) {
//         const char* name = udev_list_entry_get_name(iter);
//         const char* value = udev_list_entry_get_value(iter);
//         if (name) {
//             log_info("name: %s", name);
//         }
//         if (value) {
//             log_info("value: %s", value);
//         }
//     }
//     log_info("=porop list END=");
//     log_info("=dump END=");
// }

// bool getDeviceProperties(struct udev_device* dev, Device& device)
// {
//     const char* source = udev_device_get_devnode(dev);
//     if (!source || (fnmatch("/dev/sd*[0-9]", source, 0) != 0))
//         return false;

//     device.nodeFile.assign(source);

//     if (0 == strcmp("partition", udev_device_get_devtype(dev))) {
//         struct udev_list_entry *iter;
//         struct udev_list_entry *plist = udev_device_get_properties_list_entry(dev);
//         udev_list_entry_foreach (iter, plist) {
//             const char* name = udev_list_entry_get_name(iter);
//             const char* value = udev_list_entry_get_value(iter);
//             if (value) {
//                 if (strcmp("ID_FS_UUID" , name) == 0) {
//                     device.UUID = std::string(value);
//                 } else if (strcmp("ID_FS_TYPE" , name) == 0) {
//                     device.fsType = std::string(value);
//                 }
//             }
//         }
//     } else {
//         log_info("other device type %s", udev_device_get_devtype(dev));
//         return false;
//     }
//     return true;
// }

void PlatformUdevDevice::pollUdevFd()
{
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    struct timeval tv = { 0, 5000 };
    int ret = select(fd + 1, &fds, NULL, NULL, &tv);

    if (0 < ret && FD_ISSET(fd, &fds)) {
        struct udev_device* dev = udev_monitor_receive_device(monitor);
        // const  udev_device_get_devtype(dev)
        handleUDevDevice(dev);
        udev_device_unref(dev);
    }
}

void PlatformUdevDevice::handleUDevDevice(struct udev_device* dev)
{
    if (NULL == dev)
        return;
    // dump_udev_device(dev);
    bool isAddition = true;
    const char* action = udev_device_get_action(dev);
    if (action) {
        if (0 == strcmp("add", action)) {
            type = DeviceEvent::Type::eADD;
        } else if (0 == strcmp("remove", action)) {
            type = DeviceEvent::Type::eREMOVE;
        } else {
            // log_error("udev_device_get_action: %s", action);
        }
        m_listener(DeviceEvent(type, device));
    }
}

std::vector<std::string> PlatformUdevDevice::enumerateDevices(bool triggerEvents)
{
    // some flash drive are sda not sda1 ...
    auto entries = listDirectory("/dev", "sd*[0-9]");
    for (auto i : entries) {
        if (access(i.c_str(), F_OK) == 0) {
            Device device(i, "", "");
            getDeviceInfoViaBlkid(device);
            if (device.fsType.empty())
                continue;


        }
    }
    return devices;
}

bool PlatformUdevDevice::createUDevCtx()
{
    __TRACE__;
    if (!udev) {
        udev = udev_new();
        if (NULL == udev) {
            log_error("failed to get udev ctx");
            return false;
        }
    }
    return true;
}
