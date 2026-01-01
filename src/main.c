#include "libyasdi.h"
#include "libyasdimaster.h"
#include "tools.h"
#include <sys/time.h>
#include "common.h"
#include "ve_dbus.h"

static yasdi_bridge_keymap_t bridge_keymap[] = {
    { "/Ac/L1/Voltage", "Uac" },
    { "/Ac/L1/Current", "Iac-Ist"},
    { "/Ac/L1/Power", "Pac" },

    { "/Ac/Power", "Pac" },
    { "/Ac/Frequency", "Fac"},

    { "/Pv/0/V", "Upv-Soll" },

    { "/Ac/Energy/Forward", "E-Total" },
    { "/Ac/L1/Energy/Forward", "E-Total" },

    { "/Ac/MaxPower", "Plimit" },
    { "/FirmwareVersion", "Software-BFR" },
    { "/Serial", "Seriennummer" },
    { "/StatusCode", "Status"}          // Stop/Offset/Warten/Mpp
    //{ "DC_CURRENT_TOTA"L, "Ipv"}
};


typedef struct {
    char* name;
    //char* name_units;
    char* value;
    double raw_value;
    bool has_timed_out;
    ve_dbus_path_t* dbus_ptrs[DBUS_FIELDS_PER_CHANNEL];
} yasdi_channel_t;

typedef struct {
    DWORD handle;
    char* device_name;
    yasdi_channel_t* channels[MAX_CHANNEL_COUNT];
    uint16_t channel_count;
    uint16_t channel_param_count;
    uint16_t channels_timed_out;
    bool have_static;
} yasdi_device_descriptor_t;

yasdi_device_descriptor_t devices[DEVICE_MAX] = { 0 };
uint8_t devices_count = 0;

bool detect_devices( int device_count);
void record_devices(void);
bool fetch_device_data(yasdi_device_descriptor_t* descriptor, TChanType channel_type);
void process_data();

bool _device_search_complete = false;
bool _discovery_complete = false;
struct timeval __millis_start;


void init_millis()
{
    gettimeofday(&__millis_start, NULL);
}


uint32_t get_millis()
{
    long mtime, seconds, useconds;
    struct timeval end;
    gettimeofday(&end, NULL);
    seconds  = end.tv_sec  - __millis_start.tv_sec;
    useconds = end.tv_usec - __millis_start.tv_usec;

    mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
    return mtime;
}


bool detect_devices( int device_count)
{
    int error;

    printf("[sman] trying to detect %u devices...\n", device_count);

    /* Blocking call to detect devices */
    error = DoStartDeviceDetection(device_count, FALSE);
    switch(error) 
    {
        case YE_OK:
            return true;

        case YE_DEV_DETECT_IN_PROGRESS:
            printf("[sman] detection in progress\n");
            return false;

        case YE_NOT_ALL_DEVS_FOUND:
            printf("[sman] not all devices were found\n");
            return false;

        default:
            printf("[sman] unknown YASDI error\n");
            return false;
    }
}


void record_devices(void)
{
    DWORD handles_array[DEVICE_MAX], device, count = -1;
    char namebuf[SIZE_NAME] = "";

    count = GetDeviceHandles(handles_array, DEVICE_MAX);
    if (count > 0) {
        for (device=0; device < count ; device++)
        {
            GetDeviceName(handles_array[device], namebuf, sizeof(namebuf)-1);
            printf("[sman] found device with a handle of : %u and a name of: %s\n", handles_array[device], namebuf);
            if (devices[device].device_name != NULL)
            {
                free(devices[device].device_name);
            }
            devices[device].device_name = (char*)malloc(strlen(namebuf)+1);
            strcpy(devices[device].device_name, namebuf);
            devices[device].handle = handles_array[device];
            devices_count++;
        }
    }
    else
    {
        printf("[sman] no devices found\n");
    }
}


bool fetch_device_data(yasdi_device_descriptor_t* descriptor, TChanType channel_type)
{
    DWORD channel_array[MAX_CHANNEL_COUNT];
    int channel_count = -1;
    ve_dbus_path_t* changed_channel_paths[MAX_CHANNEL_COUNT];
    int changed_channels_count = 0;
    char channel_name[SIZE_NAME];
    //char channel_units[SIZE_NAME];
    char channel_value_str[SIZE_NAME];

    int result;
    double channel_value_dbl = 0;
    uint8_t timed_out_channels = 0;

    // how old a value from the inverter can be (shorter time = more frequent requests = higher CPU)
    DWORD max_age = 5;

    channel_count = GetChannelHandlesEx(descriptor->handle, channel_array, MAX_CHANNEL_COUNT, channel_type);
    if (channel_count < 1) 
    {
        printf("[sman] could not get the channel count for device %s\n", descriptor->device_name);
        return false;
    }

    uint16_t start_offset = (channel_type == SPOTCHANNELS) ? descriptor->channel_param_count : 0;

    for(int i = 0; i < channel_count; i++) 
    {
        uint16_t local_index = start_offset + i;

        if (local_index >= descriptor->channel_count)
        {
            // if a new channel, get it's name
            descriptor->channels[local_index] = (yasdi_channel_t*)malloc(sizeof(yasdi_channel_t));
            memset(descriptor->channels[local_index], 0, sizeof(yasdi_channel_t));

            // static buffer for the name
            descriptor->channels[local_index]->value = (char*)malloc(SIZE_NAME);

            descriptor->channel_count++;
        }
        yasdi_channel_t* channel = descriptor->channels[local_index];

        result = GetChannelName(channel_array[i], channel_name, sizeof(channel_name)-1);
        if(result == YE_OK)
        {
            // GetChannelUnit(channel_array[i], channel_units, sizeof(channel_units)-1);
        }
        else 
        {
            printf("[sman] error reading channel '%s' value\n", channel_name);
            continue;
        }

        // copy formatting info into channel list
        if (channel->name != NULL)
        {
            free(channel->name);
        }
        channel->name = (char*)malloc(strlen(channel_name)+1);
        strcpy(channel->name, channel_name);

        if (channel->dbus_ptrs[0] == NULL)
        {
            uint16_t last_index = 0;
            for (int k = 0; k < DBUS_FIELDS_PER_CHANNEL; k++)
            {
                if (channel->dbus_ptrs[k] == NULL)
                {
                    for (int j = last_index; j < sizeof(bridge_keymap)/sizeof(yasdi_bridge_keymap_t); j++)
                    {
                        if (strcmp(bridge_keymap[j].channel_name, channel->name) == 0)
                        {
                            if (bridge_keymap[j].dbus_ptr != NULL)
                            {
                                printf("[sman] mapped dbus channel for %s => %s & %u\n", channel->name, bridge_keymap[j].ve_key, bridge_keymap[j].dbus_ptr);
                                channel->dbus_ptrs[k] = bridge_keymap[j].dbus_ptr;
                            }
                            last_index = j+1;
                            break;
                        }
                    }
                }
                else
                {
                    break;
                }
            }
        }

        /*
        Not needed
        if (channel->name_units != NULL)
        {
            free(channel->name_units);
        }
        channel->name_units = (char*)malloc(strlen(channel_units)+1);
        strcpy(channel->name_units, channel_units);
        */

        channel_value_str[0] = 0;
        channel_value_dbl = 0;
        result = GetChannelValue(channel_array[i], descriptor->handle, &channel_value_dbl, &channel_value_str[0], sizeof(channel_value_str)-1, max_age);
        if(result == YE_OK)
        {
            channel->has_timed_out = false;

            // if we didn't get a string value then just format the numeric one
            if (channel_value_str[0] == 0)
            {
                sprintf(channel->value, "%.2f", channel_value_dbl);
            }
            else
            {
                strcpy(channel->value, channel_value_str);
            }
            channel->raw_value = channel_value_dbl;

            //printf(">> %s\t%s\n", channel->name, channel->value);

            // sync the changes to dbus, if they have changed tell the dbus-network
            for (int k = 0; k < DBUS_FIELDS_PER_CHANNEL; k++)
            {
                bool changed = false;

                if (channel->dbus_ptrs[k] != NULL)
                {
                    if (strcmp(channel->value, channel->dbus_ptrs[k]->output_str) != 0)
                    {
                        strcpy(channel->dbus_ptrs[k]->output_str, channel->value);
                        printf(">> %s\t%s\n", channel->name, channel->dbus_ptrs[k]->output_str);
                        changed = true;
                    }
                    if (channel->raw_value != channel->dbus_ptrs[k]->output_dec)
                    {
                        channel->dbus_ptrs[k]->output_dec = channel->raw_value;
                        channel->dbus_ptrs[k]->output_uint = channel->raw_value;
                        changed = true;
                    }
                }
                else
                {
                    break;
                }
        
                if (changed)
                {
                    changed_channel_paths[changed_channels_count] = channel->dbus_ptrs[k];
                    changed_channels_count++;
                }
            }
        }
        else if (result == YE_TIMEOUT)
        {
            if (!channel->has_timed_out)
            {
                // blank all the fields on dbus (that we are forwarding)
                for (int k = 0; k < DBUS_FIELDS_PER_CHANNEL; k++)
                {
                    if (channel->dbus_ptrs[k] != NULL)
                    {
                        strcpy(channel->dbus_ptrs[k]->output_str, "");
                        channel->dbus_ptrs[k]->output_dec = 0;
                        channel->dbus_ptrs[k]->output_uint = 0;
                    }
                }
                channel->has_timed_out = true;
                printf("[sman] error reading channel '%s' value, device timed out\n", channel_name);
            }
            timed_out_channels++;

            // in case timeouts have blocked a long time above, reply to messages
            dbus_check_for_message();
            continue;
        }
        else
        {
            printf("[sman] error reading channel '%s' value\n", channel_name);
            dbus_check_for_message();
            continue;
        }
    }

    descriptor->channels_timed_out = timed_out_channels;

    if (changed_channels_count > 0)
    {
        ve_dbus_items_changed(changed_channel_paths, changed_channels_count);
    }

    return true;
}


void on_yasdi_device_detection(TYASDIDetectionSub event, DWORD DeviceHandle, DWORD param1 )
{
    switch(event)
   {
      case YASDI_EVENT_DEVICE_ADDED:
         printf("[sman] device found\n");
         break;

      case YASDI_EVENT_DEVICE_REMOVED:
         printf("[sman] device removed\n");
         break; 
      
      case YASDI_EVENT_DEVICE_SEARCH_END:
         printf("[sman] device search complete\n");
         _device_search_complete = true;
         break;
         
      case YASDI_EVENT_DOWNLOAD_CHANLIST:
         break;
         
      default: 
         printf("[sman] unknown yasdi (0x%2x) event...\n", event);
         break;
   }
   
   return;
}


int main(int argc, char *argv[])
{
    DWORD drivers = 0;
    char driver_name[SIZE_NAME];
    bool any_driver = false;
    DWORD driver_handle[MAXDRIVERS];

    uint8_t number_of_devices = 1;

    init_millis();
    ve_dbus_init();

    // map the user-defined bridge string [yasdi] paths to a dbus/victron dbus path
    for (int i = 0; i < sizeof(bridge_keymap)/sizeof(yasdi_bridge_keymap_t); i++)
    {
        ve_dbus_path_t* dbus_paths;
        uint16_t path_count = ve_dbus_get_path_list(&dbus_paths);

        for (int j = 0; j < path_count; j++)
        {
            if (strcmp(bridge_keymap[i].ve_key, dbus_paths[j].path) == 0)
            {
                bridge_keymap[i].dbus_ptr = &dbus_paths[j];
            }
        }

        if (bridge_keymap[i].dbus_ptr == NULL)
        {
            printf("[dbus] no match for %s in the ve path descriptors\n", bridge_keymap[i].channel_name);
        }
    }

    int result = yasdiMasterInitialize("yasdi.ini", &drivers);
    if (result < 0)
    {
        printf("[yasi] can't initialise yasdi library or ini file missing\n");
        return 1;
    }

    drivers = yasdiMasterGetDriver(driver_handle, MAXDRIVERS );
    for(DWORD i = 0; i < drivers; i++)
    {
        yasdiGetDriverName(driver_handle[i], driver_name, sizeof(driver_name) - 1);
        printf("[yasi] switching on driver: %s\n", driver_name);
        if (yasdiSetDriverOnline(driver_handle[i])) 
        {
            any_driver = true;
        }
    }

    if (any_driver == false) {
        printf("[yasi] no drivers loaded!\n");
        return 1;
    }

    dbus_check_for_message();

    // search async otherwise we block dbus responses
    _device_search_complete = false;
    _discovery_complete = false;
    yasdiMasterAddEventListener( on_yasdi_device_detection, YASDI_EVENT_DEVICE_DETECTION );
    detect_devices(number_of_devices);

    uint32_t last_probe = get_millis();
    uint32_t probe_interval = DEFAULT_PROBE_INTERVAL;

    while (1)
    {
        if ((get_millis() - last_probe) > probe_interval)
        {
            if (_device_search_complete)
            {
                if (!_discovery_complete)
                {
                    record_devices();
                    _discovery_complete = true;
                }
                //printf(">---- Update SMA started\n");
                for(uint8_t device_index = 0; device_index < devices_count; device_index++)
                {
                    if (!devices[device_index].have_static)
                    {
                        if(fetch_device_data(&devices[device_index], PARAMCHANNELS))
                        {
                            devices[device_index].have_static = true;
                            devices[device_index].channel_param_count = devices[device_index].channel_count;
                        }
                    }
                    else
                    {
                        fetch_device_data(&devices[device_index], SPOTCHANNELS);
                    }
                    
                    if (devices[device_index].channels_timed_out >= devices[device_index].channel_count)
                    {
                        // lost all data from it
                        probe_interval = OFFLINE_PROBE_INTERVAL;
                    }
                    else
                    {
                        probe_interval = DEFAULT_PROBE_INTERVAL;
                    }
                }
                //printf(">---- Update SMA finished\n");
            }
            else
            {
                printf("[sman] device search pending\n");
            }
            last_probe = get_millis();
            
        }
        dbus_check_for_message();
    }

    for(DWORD i = 0; i < drivers; i++)
    {
        yasdiGetDriverName(driver_handle[i], driver_name, sizeof(driver_name));
        yasdiSetDriverOffline( driver_handle[i] );
    }

    yasdiMasterShutdown();
    return 0;
}
