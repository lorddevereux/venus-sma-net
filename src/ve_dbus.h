#ifndef VE_DBUS_H
#define VE_DBUS_H

#include <stdint.h>
#include <stdbool.h>
#include <dbus-1.0/dbus/dbus.h>
#include "common.h"

/*
-- Some debug commands
 dbus-monitor --system interface='org.freedesktop.DBus.Introspectable'
dbus-send --system --print-reply --dest=com.victronenergy.system /Devices/NumberOfVebusDevices org.freedesktop.DBus.Introspectable.Introspect
dbus-send --system --print-reply --type=method_call --dest=com.victronenergy.pvinverters.smanet / com.victronenergy.BusItem.GetItems
 /opt/victronenergy/serial-starter/stop-tty.sh /dev/ttyUSB0
*/

bool ve_dbus_init(void);
void ve_dbus_print_error(char *str);
bool dbus_check_for_message();

void ve_dbus_get_value(DBusMessage* msg, const char* path);
void ve_dbus_get_text(DBusMessage* msg, const char* path);
void ve_dbus_get_invalid(DBusMessage* msg, const char* path);
void ve_dbus_get_items(DBusMessage* msg, const char* path);
void ve_dbus_items_changed(ve_dbus_path_t** paths, uint16_t path_count);

void ve_dbus_set_offline(void);
void ve_dbus_set_online(void);

uint16_t ve_dbus_get_path_list(ve_dbus_path_t** path_ptr);

#endif