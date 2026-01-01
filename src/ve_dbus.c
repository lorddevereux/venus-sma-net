#include "ve_dbus.h"
#include "common.h"

#define NODE_CHILD_MAX      20

struct dbus_path_item_s_t{
    char* node_name;
    struct dbus_path_item_s_t* children[NODE_CHILD_MAX];
    uint8_t child_count;
};

typedef struct dbus_path_item_s_t dbus_path_item_t;


static const char* dbus_introspection = 
"<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
"\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
"<node name=\"%s\">\n"
"  <interface name=\"org.freedesktop.DBus.Introspectable\">\n"
"    <method name=\"Introspect\">\n"
"      <arg name=\"data\" direction=\"out\" type=\"s\"/>\n"
"    </method>\n"
"  </interface>\n"
"  <interface name=\"com.victronenergy.BusItem\">\n"
"    <method name=\"GetValue\">\n"
"       <arg direction=\"out\" type=\"v\"/>\n"
"    </method>\n"
"    <method name=\"GetText\">\n"
"       <arg direction=\"out\" type=\"s\"/>\n"
"    </method>\n"
"    <method name=\"GetItems\">\n"
"       <arg direction=\"out\" type=\"a{sa{sv}}\"/>\n"
"    </method>\n";

static const char* dbus_introspect_method_get_min =
"    <method name=\"GetMin\">\n"
"      <arg direction=\"out\" type=\"u\"/>\n"
"    </method>\n";

static const char* dbus_introspect_method_get_max = 
"    <method name=\"GetMax\">\n"
"      <arg direction=\"out\" type=\"u\"/>\n"
"    </method>\n";

static const char* dbus_introspect_method_set_value = 
"    <method name=\"SetValue\">\n"
"       <arg direction=\"in\" type=\"v\" name=\"newvalue\" />\n"
"       <arg direction=\"out\" type=\"i\"/>\n"
"    </method>\n";

static const char* dbus_introspect_method_get_description = 
"   <method name=\"GetDescription\">"
"       <arg direction=\"in\"  type=\"s\" name=\"language\" />"
"       <arg direction=\"in\"  type=\"i\" name=\"length\" />"
"       <arg direction=\"out\" type=\"s\" />"
"   </method>";

/*
static const char* dbus_introspect_signal_propertieschanged = 
"   <signal name=\"PropertiesChanged\">"
"       <arg type=\"a{sv}\" name=\"changes\" />"
"   </signal>";
*/

static const char* dbus_introspect_signal_itemschanged = 
"   <signal name=\"ItemsChanged\">"
"       <arg type=\"a{sa{sv}}\" name=\"changes\" />"
"   </signal>";

static const char* dbus_introspect_close_interface = "  </interface>\n";
static const char* dbus_introspect_child_node = "  <node name=\"%s\">\n";
static const char* dbus_introspect_close_node = "</node>\n";

static const char* dbus_getitems_value = "Value";
static const char* dbus_getitems_text = "Text";

static dbus_path_item_t dbus_path_tree;
static ve_dbus_path_t dbus_paths[] = {
    { "/Mgmt/ProcessName", DBUS_TYPE_STRING, 0, "venus-sma-net"},
    { "/Mgmt/ProcessVersion", DBUS_TYPE_UINT32, VERSION, VERSION_STR },
    { "/Mgmt/Connection", DBUS_TYPE_STRING, 0, "RS485-SMANet" },
    { "/DeviceInstance", DBUS_TYPE_UINT32, 1, "1" },
    { "/ProductId", DBUS_TYPE_UINT32, 0xFFFF, "0xffff" },
    { "/ProductName", DBUS_TYPE_STRING, 0, "YASDI SMAnet device" },
    { "/CustomName", DBUS_TYPE_STRING, 0, "SMA 3800" },
    { "/FirmwareVersion", DBUS_TYPE_STRING, 0, VERSION_STR },
    { "/Serial", DBUS_TYPE_UINT32, 0 },
    { "/Connected", DBUS_TYPE_UINT32, 1 },
    { "/Latency", DBUS_TYPE_UINT32, 0 },
    { "/ErrorCode", DBUS_TYPE_UINT32, 0 },
    { "/Position", DBUS_TYPE_UINT32, 0 },
    { "/StatusCode", DBUS_TYPE_UINT32, 0 },

    { "/Pv/0/V", DBUS_TYPE_DOUBLE, 0 },

    { "/NrOfPhases", DBUS_TYPE_UINT32, 1 },
    { "/NrOfTrackers", DBUS_TYPE_UINT32, 1 },
    { "/Ac/Frequency", DBUS_TYPE_DOUBLE, 0 },

    { "/Ac/L1/Power", DBUS_TYPE_UINT32, 0 },
    { "/Ac/L1/Current", DBUS_TYPE_DOUBLE, 0 },
    { "/Ac/L1/Voltage", DBUS_TYPE_DOUBLE, 0 },
    { "/Ac/L1/Energy/Forward", DBUS_TYPE_UINT32, 0 },

    { "/Ac/Energy/Forward", DBUS_TYPE_UINT32, 0 },
    { "/Ac/Power", DBUS_TYPE_UINT32, 0 },

    /*{ "/Ac/L2/Power", DBUS_TYPE_UINT32, 0 },
    { "/Ac/L2/Current", DBUS_TYPE_DOUBLE, 0 },
    { "/Ac/L2/Voltage", DBUS_TYPE_DOUBLE, 0 },
    { "/Ac/L3/Power", DBUS_TYPE_UINT32, 0 },
    { "/Ac/L3/Current", DBUS_TYPE_DOUBLE, 0 },
    { "/Ac/L3/Voltage", DBUS_TYPE_DOUBLE, 0 },*/

    //{ "/Ac/Voltage", DBUS_TYPE_DOUBLE, 0 },
    //{ "/Ac/Current", DBUS_TYPE_DOUBLE, 0 },
    { "/Ac/MaxPower", DBUS_TYPE_UINT32, 3800 },
    { "/Ac/Position", DBUS_TYPE_UINT32, 0 },
    //{ "/Ac/StatusCode", DBUS_TYPE_UINT32, 7 },
    { "/UpdateIndex", DBUS_TYPE_UINT32, 0 }
};

const char* ve_service = "com.victronenergy.pvinverter.smanet";

DBusConnection *dbus_connection;
DBusError dbus_error;

static int8_t find_full_path_match(const char* path);

void ve_dbus_print_error(char *str)
{
    printf("%s: %s\n", str, dbus_error.message);
    dbus_error_free(&dbus_error);
}


char* ve_dbus_create_introspect(const char* node_name, dbus_path_item_t* tree_path, bool settable, bool has_min, bool has_max, bool has_desc)
{
    // -2 to correct for %s
    uint16_t array_size = strlen(dbus_introspection) + strlen(dbus_introspect_close_interface) + strlen(dbus_introspect_close_node) + strlen(node_name) - 2;
    uint8_t child_xml_len = strlen(dbus_introspect_child_node) - 2;
    char sprint_buffer[128];

    if (settable)
        array_size += strlen(dbus_introspect_method_set_value);

    if (has_min)
        array_size += strlen(dbus_introspect_method_get_min);

    if (has_max)
        array_size += strlen(dbus_introspect_method_get_max);

    if (has_desc)
        array_size += strlen(dbus_introspect_method_get_description);

    array_size += strlen(dbus_introspect_signal_itemschanged);

    if (tree_path->child_count > 0)
    {
        // remove the %s
        for (int i = 0; i < tree_path->child_count; i++)
        {
            if (tree_path->children[i] != NULL)
            {
                if (tree_path->children[i]->node_name != NULL)
                {
                    uint8_t length_of_node = strlen(tree_path->children[i]->node_name);
                    array_size += child_xml_len + length_of_node;
                }
            }
        }
    }

    char* xml = (char*)malloc(array_size);

    if (xml == NULL)
    {
        printf("[dbus] out of memory for dbus xml\n");
        return NULL;
    }

    sprintf(xml, dbus_introspection, node_name);

    if (settable)
        strcat(xml, dbus_introspect_method_set_value);
    
    if (has_min)
        strcat(xml, dbus_introspect_method_get_min);

    if (has_max)
        strcat(xml, dbus_introspect_method_get_max);

    if (has_desc)
        strcat(xml, dbus_introspect_method_get_description);

    strcat(xml, dbus_introspect_signal_itemschanged);
    strcat(xml, dbus_introspect_close_interface);

    if (tree_path->child_count > 0)
    {
        // remove the %s
        for (int i = 0; i < tree_path->child_count; i++)
        {
            if (tree_path->children[i] != NULL)
            {
                if (tree_path->children[i]->node_name != NULL)
                {
                    sprintf(sprint_buffer, dbus_introspect_child_node, tree_path->children[i]->node_name);
                    strcat(xml, sprint_buffer);
                }
            }
        }
    }

    strcat(xml, dbus_introspect_close_node);
    return xml;
}


char* ve_dbus_create_introspect_basic(const char* node_name, dbus_path_item_t* tree_path)
{
    return ve_dbus_create_introspect(node_name, tree_path, false, false, false, false);
}


dbus_path_item_t* find_dbus_tree_match(dbus_path_item_t* parent, char* node, char* path_name)
{
    printf("search for '%s' in '%s' parent node '%s'\n", node, path_name, parent->node_name);

    char* next_token = strsep(&path_name, "/");
    bool is_end_of_tree = false;

    if (next_token == NULL)
    {
        // is the end of the query
        is_end_of_tree = true;
    }
    else if (next_token[0] == 0)
    {
        // is a trailing slash, so end of query
        is_end_of_tree = true;
    }

    if (is_end_of_tree)
    {
        if (strcmp(parent->node_name, node) == 0)
        {
            printf("> matched end of tree\n");
            return parent;
        }
    }

    for (int i = 0; i < parent->child_count; i++)
    {
        if (is_end_of_tree)
        {
            printf("> end of tree\n");
            if (strcmp(parent->children[i]->node_name, node) == 0)
            {
                return parent->children[i];
            }
        }
        else
        {
            if (strcmp(parent->children[i]->node_name, node) == 0)
            {
                printf("> search children of '%s'\n", parent->children[i]->node_name);

                dbus_path_item_t* result = find_dbus_tree_match(parent->children[i], next_token, path_name);
                if (result != NULL)
                {
                    return result;
                }
            }
        }
    }

    // didn't find it here
    return NULL;
}


dbus_path_item_t* ve_dbus_find_tree_item(const char* path)
{
    char* path_temp = strdup(path);
    char* free_point = path_temp;
    char* token = strsep(&path_temp, "/");
    if (token[0] == 0)
    {
        token = strsep(&path_temp, "/");
    }
    dbus_path_item_t* tree_path = find_dbus_tree_match(&dbus_path_tree, token, path_temp);
    free(free_point);
    return tree_path;
}


bool build_dbus_path_tree_recurse(dbus_path_item_t* parent, char* node, char* path_name)
{
    printf("Parse %s out of %s\n", node, path_name);
    if (node[0] == 0)
    {
        printf("> ignore trailing slash\n");
        return true;
    }

    for (int i = 0; i < NODE_CHILD_MAX; i++)
    {
        if (parent->children[i] == NULL)
        {
            // we didn't find it, so make it
            printf("> created it\n");
            parent->children[i] = (dbus_path_item_t*)malloc(sizeof(dbus_path_item_t));
            parent->child_count++;
            if (parent->children[i] == NULL)
            {
                printf("build_dbus_tree - out of memory\n");
                return false;
            }
            else
            {
                parent->children[i]->node_name = strdup(node);
            }

            if ((node = strsep(&path_name, "/")))
            {
                build_dbus_path_tree_recurse(parent->children[i], node, path_name);
                return true;
            }
            else
            {
                printf("> no more levels\n");
                return true;
            }
        }
        else
        {
            if (strcmp(parent->children[i]->node_name, node) == 0)
            {
                printf("> traverse\n");
                if ((node = strsep(&path_name, "/")))
                {
                    build_dbus_path_tree_recurse(parent->children[i], node, path_name);
                    return true;
                }
            }
        }
    }

    printf("out of node space for %s\n", parent->node_name);
    return false;
}


bool ve_dbus_init(void)
{
    int ret;
    char* token;
    char* free_point;
    char* path_temp;

    // build the introspection/output table from the path config
    dbus_path_tree.node_name = "";

    for (int i = 0; i < sizeof(dbus_paths)/sizeof(ve_dbus_path_t); i++)
    {
        dbus_paths[i].id = i;
        path_temp = strdup(dbus_paths[i].path);
        free_point = path_temp;
        token = strsep(&path_temp, "/");
        token = strsep(&path_temp, "/");
        build_dbus_path_tree_recurse(&dbus_path_tree, token, path_temp);
        free(free_point);

        if (dbus_paths[i].default_str != NULL)
        {
            dbus_paths[i].output_str = strdup(dbus_paths[i].default_str);
        }
        else
        {
            dbus_paths[i].output_str = (char*)malloc(5);

            // round to integers to fix precision issues
            uint32_t default_num_int = dbus_paths[i].default_num;
            sprintf(dbus_paths[i].output_str, "%u", default_num_int);
        }
        dbus_paths[i].output_dec = dbus_paths[i].default_num;
        dbus_paths[i].output_uint = dbus_paths[i].default_num;
    }

    dbus_error_init(&dbus_error);
    dbus_connection = dbus_bus_get(DBUS_BUS_SYSTEM, &dbus_error);

    if (dbus_error_is_set(&dbus_error))
    {
        ve_dbus_print_error("dbus_bus_get");
    }

    if (!dbus_connection)
    {
        printf("[dbus] unable to initialise dbus connection\n");
        return false;
    }

    ret = dbus_bus_request_name(dbus_connection, ve_service, DBUS_NAME_FLAG_DO_NOT_QUEUE, &dbus_error);

    if (dbus_error_is_set(&dbus_error))
    {
        ve_dbus_print_error("dbus_bus_get");
    }

    if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
    {
        printf("[dbus] not primary owner of service name, ret = %d", ret);
        return false;
    }

    printf("[dbus] server initialised on %s\n", ve_service);

    return true;
}


bool dbus_check_for_message()
{
    // non blocking read of the next available message
    dbus_connection_read_write(dbus_connection, DEFAULT_PROBE_INTERVAL);
    DBusMessage* msg = dbus_connection_pop_message(dbus_connection);

    if (NULL == msg)
    { 
        return false;
    }
    
    const char* path = dbus_message_get_path(msg);

    // check this is a method call for the right interface & method
    if (dbus_message_has_interface(msg, "com.victronenergy.BusItem"))
    {
        if (dbus_message_is_method_call(msg, "com.victronenergy.BusItem", "GetValue"))
        {
            printf("[dbus] get value %s\n", path);
            ve_dbus_get_value(msg, path);
        }
        else if (dbus_message_is_method_call(msg, "com.victronenergy.BusItem", "GetText"))
        {
            printf("[dbus] get text %s\n", path);
            ve_dbus_get_text(msg, path);
        }
        else if (dbus_message_is_method_call(msg, "com.victronenergy.BusItem", "GetItems"))
        {
            printf("[dbus] get items %s\n", path);
            ve_dbus_get_items(msg, path);
        }
        else
        {
            printf("[dbus] invalid method call on %s\n", path);
            ve_dbus_get_invalid(msg, path);
        }
    }

    else if (dbus_message_has_interface(msg, "org.freedesktop.DBus.Introspectable.Introspect"))
    {
        dbus_path_item_t* tree_path = ve_dbus_find_tree_item(path);

        if (tree_path == NULL)
        {
            printf("[dbus] unknown introspect path %s\n", path);
            return false;
        }
        else
        {
            printf("[dbus] get introspect %s\n", path);
            char* introspect = ve_dbus_create_introspect_basic(path, tree_path);
            DBusMessage* reply = dbus_message_new_method_return(msg);
            DBusMessageIter args;

            if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &introspect)) 
            {
                printf("[dbus] memory allocation failed, can't continue\n");
                exit(1);
            }

            if (!dbus_connection_send( dbus_connection, reply, NULL))
            {
                printf("[dbus] error 2 in dbus_handle_introspect\n");
            }

            dbus_connection_flush(dbus_connection);
            dbus_message_unref(reply);
            free(introspect);
        }
    }

    dbus_message_unref(msg);
    return true;
}


int8_t find_full_path_match(const char* path)
{
    for (int i = 0; i < sizeof(dbus_paths)/sizeof(ve_dbus_path_t); i++)
    {
        int p = strcmp(path, dbus_paths[i].path);
        if (p == 0) 
        {
            return i;
        }
    }

    return -1;
}


void staple_value_as_variant(DBusMessageIter* container, uint16_t path_id, bool force_text)
{
    DBusMessageIter args;
    if ((dbus_paths[path_id].type == DBUS_TYPE_DOUBLE) && !force_text)
    {
        dbus_message_iter_open_container(container, DBUS_TYPE_VARIANT, "dv", &args);
        //printf(">> A %s\t%s\n", dbus_paths[path_id].path, dbus_paths[path_id].output_str);
        if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_DOUBLE, &dbus_paths[path_id].output_dec)) 
        { 
            printf("[dbus] memory allocation failed, can't continue\n");
            exit(1);
        }
    }
    else if ((dbus_paths[path_id].type == DBUS_TYPE_UINT32) && !force_text)
    {
        dbus_message_iter_open_container(container, DBUS_TYPE_VARIANT, "uv", &args);
        //printf(">> B %s\t%s\n", dbus_paths[path_id].path, dbus_paths[path_id].output_str);
        if (!dbus_message_iter_append_basic(&args, dbus_paths[path_id].type, &dbus_paths[path_id].output_uint)) 
        { 
            printf("[dbus] memory allocation failed, can't continue\n");
            exit(1);
        }
    }
    else
    {
        dbus_message_iter_open_container(container, DBUS_TYPE_VARIANT, "sv", &args);
        //printf(">> C %s\t%s\n", dbus_paths[path_id].path, dbus_paths[path_id].output_str);
        if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &dbus_paths[path_id].output_str)) 
        { 
            printf("[dbus] memory allocation failed, can't continue\n");
            exit(1);
        }
    }

    dbus_message_iter_close_container(container, &args);
}


void ve_dbus_get_value(DBusMessage* msg, const char* path)
{
    dbus_uint32_t serial = 0;

    int8_t path_match = find_full_path_match(path);
    if (path_match < 0)
    {
        // invalid path
        ve_dbus_get_invalid(msg, path);
        return;
    }

    DBusMessage* reply = dbus_message_new_method_return(msg);
    DBusMessageIter container;

    if (dbus_paths[path_match].output_str == NULL)
    {
        printf("[dbus] corrupted output string is empty for %s\n", dbus_paths[path_match].path);
        return;
    }

    dbus_message_iter_init_append(reply, &container);
    staple_value_as_variant(&container, path_match, false);

    //printf("[dbus] dispatch\n");
    if (!dbus_connection_send(dbus_connection, reply, &serial)) 
    {
      printf("[dbus] memory allocation failed, can't continue\n");
      exit(1);
   }
   dbus_connection_flush(dbus_connection);
   dbus_message_unref(reply);
}


void ve_dbus_get_text(DBusMessage* msg, const char* path)
{
    dbus_uint32_t serial = 0;

    int8_t path_match = find_full_path_match(path);
    if (path_match < 0)
    {
        // invalid path
        ve_dbus_get_invalid(msg, path);
        return;
    }

    DBusMessage* reply = dbus_message_new_method_return(msg);
    DBusMessageIter container, args;

    if (dbus_paths[path_match].output_str == NULL)
    {
        printf("[dbus] corrupted output string is empty for %s\n", dbus_paths[path_match].path);
        return;
    }

    dbus_message_iter_init_append(reply, &container);
    //printf("[dbus] attach string\n");
    dbus_message_iter_open_container(&container, DBUS_TYPE_VARIANT, "sv", &args);
    if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &dbus_paths[path_match].output_str)) 
    { 
        printf("[dbus] memory allocation failed, can't continue\n");
        exit(1);
    }

    dbus_message_iter_close_container(&container, &args);

    //printf("[dbus] dispatch\n");

    if (!dbus_connection_send(dbus_connection, reply, &serial)) 
    {
        printf("[dbus] memory allocation failed, can't continue\n");
        exit(1);
    }

    //printf("[dbus] flush\n");
    dbus_connection_flush(dbus_connection);
    dbus_message_unref(reply);
}


void ve_dbus_get_items(DBusMessage* msg, const char* path)
{
    dbus_uint32_t serial = 0;

    DBusMessage* reply = dbus_message_new_method_return(msg);
    DBusMessageIter container, entry_array;

    dbus_message_iter_init_append(reply, &container);
    //printf("[dbus] attach container\n");
    dbus_message_iter_open_container(&container, DBUS_TYPE_ARRAY, "{sa{sv}}", &entry_array);

    for (int i = 0; i < sizeof(dbus_paths)/sizeof(ve_dbus_path_t); i++)
    {
        //printf("[dbus] write item\n");
        DBusMessageIter entry_obj;
        dbus_message_iter_open_container(&entry_array, DBUS_TYPE_DICT_ENTRY, NULL, &entry_obj);

            //printf("[dbus] attach entry\n");
            if (!dbus_message_iter_append_basic(&entry_obj, DBUS_TYPE_STRING, &dbus_paths[i].path)) 
            { 
                printf("[dbus] memory allocation failed, can't continue\n");
                exit(1);
            }

            DBusMessageIter array_keys;
            //printf("> [dbus] attach entry fields\n");
            dbus_message_iter_open_container(&entry_obj, DBUS_TYPE_ARRAY, "{sv}", &array_keys);

                DBusMessageIter entry_value, entry_text;
                //printf("  > [dbus] attach dict_entry\n");
                dbus_message_iter_open_container(&array_keys, DBUS_TYPE_DICT_ENTRY, NULL, &entry_value);
                
                    //printf("    > [dbus] attach dict_entry_title\n");
                    if (!dbus_message_iter_append_basic(&entry_value, DBUS_TYPE_STRING, &dbus_getitems_value)) 
                    { 
                        printf("[dbus] memory allocation failed, can't continue\n");
                        exit(1);
                    }
                    //printf("    > [dbus] attach dict_entry_value\n");
                    staple_value_as_variant(&entry_value, i, false);
                    dbus_message_iter_close_container(&array_keys, &entry_value);

                //printf("  > [dbus] attach dict_entry 2\n");
                dbus_message_iter_open_container(&array_keys, DBUS_TYPE_DICT_ENTRY, NULL, &entry_text);

                    //printf("    > [dbus] attach dict_entry_title 2\n");
                    if (!dbus_message_iter_append_basic(&entry_text, DBUS_TYPE_STRING, &dbus_getitems_text)) 
                    { 
                        printf("[dbus] memory allocation failed, can't continue\n");
                        exit(1);
                    }
                    //printf("    > [dbus] attach dict_entry_value 2\n");
                    staple_value_as_variant(&entry_text, i, true);

                dbus_message_iter_close_container(&array_keys, &entry_text);

            dbus_message_iter_close_container(&entry_obj, &array_keys);

        dbus_message_iter_close_container(&entry_array, &entry_obj);
    }
    dbus_message_iter_close_container(&container, &entry_array);

    printf("[dbus] dispatch\n");

    if (!dbus_connection_send(dbus_connection, reply, &serial)) 
    {
        printf("[dbus] memory allocation failed, can't continue\n");
        exit(1);
    }

    //printf("[dbus] flush\n");
    dbus_connection_flush(dbus_connection);
    dbus_message_unref(reply);
}


void ve_dbus_items_changed(ve_dbus_path_t** paths, uint16_t path_count)
{
    dbus_uint32_t serial = 0;

    DBusMessage* reply = dbus_message_new_signal("/", "com.victronenergy.BusItem", "ItemsChanged");
    DBusMessageIter container, entry_array;

    // add the arguments to the reply
    dbus_message_iter_init_append(reply, &container);
    //printf("[dbus] attach container\n");
    dbus_message_iter_open_container(&container, DBUS_TYPE_ARRAY, "{sa{sv}}", &entry_array);

    for (int i = 0; i < path_count; i++)
    {
        uint16_t path_id = paths[i]->id;
        //printf("[dbus] write item\n");
        DBusMessageIter entry_obj;
        dbus_message_iter_open_container(&entry_array, DBUS_TYPE_DICT_ENTRY, NULL, &entry_obj);

            //printf("[dbus] attach entry\n");
            if (!dbus_message_iter_append_basic(&entry_obj, DBUS_TYPE_STRING, &paths[i]->path)) 
            { 
                printf("[dbus] memory allocation failed, can't continue\n");
                exit(1);
            }

            DBusMessageIter array_keys;
            //printf("> [dbus] attach entry fields\n");
            dbus_message_iter_open_container(&entry_obj, DBUS_TYPE_ARRAY, "{sv}", &array_keys);

                DBusMessageIter entry_value, entry_text;
                //printf("  > [dbus] attach dict_entry\n");
                
                dbus_message_iter_open_container(&array_keys, DBUS_TYPE_DICT_ENTRY, NULL, &entry_value);
                //printf("    > [dbus] attach dict_entry_title\n");
                if (!dbus_message_iter_append_basic(&entry_value, DBUS_TYPE_STRING, &dbus_getitems_value)) 
                { 
                    printf("[dbus] memory allocation failed, can't continue\n");
                    exit(1);
                }
                //("    > [dbus] attach dict_entry_value\n");
                staple_value_as_variant(&entry_value, path_id, false);
                dbus_message_iter_close_container(&array_keys, &entry_value);

                //printf("  > [dbus] attach dict_entry 2\n");
                dbus_message_iter_open_container(&array_keys, DBUS_TYPE_DICT_ENTRY, NULL, &entry_text);
                if (!dbus_message_iter_append_basic(&entry_text, DBUS_TYPE_STRING, &dbus_getitems_text)) 
                { 
                    printf("[dbus] memory allocation failed, can't continue\n");
                    exit(1);
                }
                staple_value_as_variant(&entry_text, path_id, true);
                dbus_message_iter_close_container(&array_keys, &entry_text);
        

            dbus_message_iter_close_container(&entry_obj, &array_keys);

        dbus_message_iter_close_container(&entry_array, &entry_obj);
    }
    dbus_message_iter_close_container(&container, &entry_array);

    printf("[dbus] dispatch\n");

    if (!dbus_connection_send(dbus_connection, reply, &serial)) 
    {
        printf("[dbus] memory allocation failed, can't continue\n");
        exit(1);
    }

    //printf("[dbus] flush\n");
    dbus_connection_flush(dbus_connection);

    // free the reply
    dbus_message_unref(reply);
}


void ve_dbus_get_invalid(DBusMessage* msg, const char* path)
{
    // nothing to do here
}


void ve_dbus_tests()
{
    dbus_path_item_t* ath_item = ve_dbus_find_tree_item("Mgmt");
    if (ath_item == NULL)
    {
        printf("Didnt' find\n");
    }
    else
    {
        printf("Did find %s <>\n", ath_item->node_name);
    }
    ve_dbus_create_introspect_basic("Mgmt", ath_item);
}


uint16_t ve_dbus_get_path_list(ve_dbus_path_t** path_ptr)
{
    *path_ptr = dbus_paths;
    return sizeof(dbus_paths)/sizeof(ve_dbus_path_t);
}


void ve_dbus_set_offline(void)
{

}


void ve_dbus_set_online(void)
{

}