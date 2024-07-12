// Also includes noh.h
#include "hooks.c" // Common code used by all platforms.

// This arena is used all throughout this file for storing temporary data.
static Noh_Arena hooks_arena = {0};

NB_Input_Devices hooks_devices = {0};

// Return a helpful string representing the error result.
char *print_dierr_hresult(HRESULT result) {
    switch (result) {
        case DIERR_DEVICENOTREG: return "DIERR_DEVICENOTREG";
        case DIERR_INVALIDPARAM: return "DIERR_INVALIDPARAM";
        case DIERR_NOINTERFACE: return "DIERR_NOINTERFACE";
        case DIERR_GENERIC: return "DIERR_GENERIC";
        case DIERR_OUTOFMEMORY: return "DIERR_OUTOFMEMORY";
        case DIERR_UNSUPPORTED: return "DIERR_UNSUPPORTED";
        case DIERR_NOTINITIALIZED: return "DIERR_NOTINITIALIZED";
        case DIERR_ALREADYINITIALIZED: return "DIERR_ALREADYINITIALIZED";
        case DIERR_NOAGGREGATION: return "DIERR_NOAGGREGATION";
        case DIERR_INPUTLOST: return "DIERR_INPUTLOST";
        case DIERR_ACQUIRED: return "DIERR_ACQUIRED";
        case DIERR_NOTACQUIRED: return "DIERR_NOTACQUIRED";
        case DIERR_READONLY: return "DIERR_READONLY";
        case DIERR_INSUFFICIENTPRIVS: return "DIERR_INSUFFICIENTPRIVS";
        case DIERR_DEVICEFULL: return "DIERR_DEVICEFULL";
        case DIERR_MOREDATA: return "DIERR_MOREDATA";
        case DIERR_NOTDOWNLOADED: return "DIERR_NOTDOWNLOADED";
        case DIERR_HASEFFECTS: return "DIERR_HASEFFECTS";
        case DIERR_NOTEXCLUSIVEACQUIRED: return "DIERR_NOTEXCLUSIVEACQUIRED";
        case DIERR_INCOMPLETEEFFECT: return "DIERR_INCOMPLETEEFFECT";
        case DIERR_NOTBUFFERED: return "DIERR_NOTBUFFERED";
        case DIERR_EFFECTPLAYING: return "DIERR_EFFECTPLAYING";
        case DIERR_UNPLUGGED: return "DIERR_UNPLUGGED";
        case DIERR_REPORTFULL: return "DIERR_REPORTFULL";
        case DIERR_MAPFILEFAIL: return "DIERR_MAPFILEFAIL";
        default: return "Unknown error";
    };
}

// Returns the data size of a device object type.
size_t device_object_data_size(DWORD type) {
    switch (type) {
        case DIDFT_BUTTON: return 1; // byte
        case DIDFT_RELAXIS: return 4; // DWORD
        case DIDFT_ABSAXIS: return 4; // DWORD
        default: assert(false && "Unknown device object type.");
    }
}

static char *guid_to_cstr(Noh_Arena *arena, const GUID *guid) {
    return noh_arena_sprintf(arena, "%0X-%0X-%0X-%0X%0X-%0X%0X%0X%0X%0X%0X", 
        guid->Data1, // First 8 hex digits.
        guid->Data2, // First group of 4 hex digits.
        guid->Data3, // Second group of 4 hex digits.
        guid->Data4[0], guid->Data4[1], // Third group of 4 hexdigits.
         // Final 12 hex digits.
        guid->Data4[2], guid->Data4[3], guid->Data4[4], guid->Data4[5], guid->Data4[6], guid->Data4[7]);
}

// Checks whether 2 guids are equal.
static bool guid_eq(const GUID *guid1, const GUID *guid2) {
    return guid1->Data1 == guid2->Data1
        && guid1->Data2 == guid2->Data2
        && guid1->Data3 == guid2->Data3
        && guid1->Data4[0] == guid2->Data4[0]
        && guid1->Data4[1] == guid2->Data4[1]
        && guid1->Data4[2] == guid2->Data4[2]
        && guid1->Data4[3] == guid2->Data4[3]
        && guid1->Data4[4] == guid2->Data4[4]
        && guid1->Data4[5] == guid2->Data4[5]
        && guid1->Data4[6] == guid2->Data4[6]
        && guid1->Data4[7] == guid2->Data4[7];
}

// Find the device at the specified index, returns NULL if out of bounds.
NB_Input_Device *hooks_find_device_by_index(size_t device_index) {
    (void)device_index;
    noh_assert(false && "Not yet implemented.");
    return (NB_Input_Device *)NULL;
}

static int align_offset(size_t cur_offset, DWORD type) {
    size_t data_size = device_object_data_size(type);
    size_t rem = cur_offset % data_size;
    return cur_offset + rem;
}

static int add_device_object(LPCDIDEVICEOBJECTINSTANCE lpddoi, LPVOID pvRef) {
    NB_Device_Object_Infos *dev_objs = (NB_Device_Object_Infos *)pvRef;

    for (size_t i = 0; i < dev_objs->count; i++) {
        // If we have handled an object with this offset before, don't process it again.
        if (dev_objs->elems[i].org_offset == lpddoi->dwOfs) return DIENUM_CONTINUE;
    }
    
    NB_Device_Object_Info info = {0};
    info.type = dev_objs->type; // Set the type with which we are enumerating, we don't care about the exact type.
    info.org_offset = lpddoi->dwOfs; // The offset to check for duplicates against.
    // The offset is the current data size, aligned to the required size.
    info.offset = align_offset(dev_objs->data_size, dev_objs->type);
    dev_objs->data_size = info.offset; // After aligning the data, also reflect this in dev_objs.
    info.name = noh_arena_strdup(&hooks_arena, lpddoi->tszName);
    dev_objs->data_size += device_object_data_size(info.type); // Increment the data size so this object fits.
    // Determine the exact instance of this button so we can refer to it later.
    info.instance = DIDFT_GETINSTANCE(lpddoi->dwType); 

    noh_da_append(dev_objs, info);
    return DIENUM_CONTINUE;
}

static int add_input_device(LPCDIDEVICEINSTANCE instance, LPVOID pvRef) {
    NB_Input_Device device = {0};

    LPDIRECTINPUT directInput = (LPDIRECTINPUT)pvRef;
    NB_Input_Devices *devices = &hooks_devices;

    // The same device can be returned twice in the enumeration, if it supports different types.
    // Skip the consecutive ones, since we already mapped all of its relevant objects.
    for (size_t i = 0; i < devices->count; i++) {
        if (guid_eq(&devices->elems[i].guid, &instance->guidInstance)) {
            return DIENUM_CONTINUE;
        }
    }

    // Determine the type of the device.
    switch (GET_DIDEVICE_TYPE(instance->dwDevType)) {
        case DI8DEVTYPE_KEYBOARD:
            device.type = NB_Keyboard;
            break;

        case DI8DEVTYPE_MOUSE: 
            if (GET_DIDEVICE_SUBTYPE(instance->dwDevType) == DI8DEVTYPEMOUSE_TOUCHPAD) {
                device.type = NB_Touchpad;
            } else {
                device.type = NB_Mouse;
            }
            break;

        case DI8DEVTYPE_JOYSTICK:
            device.type = NB_Joystick;
            break;

        default:
            device.type = NB_Unknown_Device;
            break;
    }

    device.guid = instance->guidInstance;
    device.unique_id = guid_to_cstr(&hooks_arena, &instance->guidInstance);
    device.name = noh_arena_sprintf(&hooks_arena, "%s: %s", instance->tszInstanceName, instance->tszProductName);

    LPDIRECTINPUTDEVICE deviceInstance;

    // Create a device instance.
    HRESULT result = IDirectInput_CreateDevice(directInput, &instance->guidInstance, &deviceInstance, NULL);
    if (result != DI_OK) {
        noh_log(NOH_WARNING, "Could not create instance of device %s: %s.", device.name, print_dierr_hresult(result));
        return DIENUM_CONTINUE;
    } 
    device.instance = deviceInstance;

    // Enumerate device objects and set data mapping.
    // Will fill in the device names in the hooks_arena.
    NB_Device_Object_Infos dev_objs = { .type = DIDFT_BUTTON };
    result = IDirectInputDevice_EnumObjects(deviceInstance, add_device_object, (LPVOID)&dev_objs, DIDFT_BUTTON);
    if (result != DI_OK) {
        noh_log(NOH_WARNING, "Could not enumerate buttons of device %s: %s.", device.name, print_dierr_hresult(result));
        return DIENUM_CONTINUE;
    }

    dev_objs.type = DIDFT_ABSAXIS;
    result = IDirectInputDevice_EnumObjects(deviceInstance, add_device_object, (LPVOID)&dev_objs, DIDFT_ABSAXIS);
    if (result != DI_OK) {
        noh_log(NOH_WARNING, "Could not enumerate absolute axes of device %s: %s.", device.name, print_dierr_hresult(result));
        return DIENUM_CONTINUE;
    }

    dev_objs.type = DIDFT_RELAXIS;
    result = IDirectInputDevice_EnumObjects(deviceInstance, add_device_object, (LPVOID)&dev_objs, DIDFT_RELAXIS);
    if (result != DI_OK) {
        noh_log(NOH_WARNING, "Could not enumerate relative axes of device %s: %s.", device.name, print_dierr_hresult(result));
        return DIENUM_CONTINUE;
    }

    // If the device has no relevant objects, skip it.
    if (dev_objs.count == 0) {
        noh_log(NOH_INFO, "Device %s has no relevant objects, skipping.", device.name);
        return DIENUM_CONTINUE;
    }

    // Windows expects the total data size to be a multiple of 4.
    int round_offset = dev_objs.data_size % 4;
    if (round_offset > 0) dev_objs.data_size += (4 - round_offset);

    // Apparently we need to specify globally whether we want to set the axes in absolute mode, so if we found
    // one object that reports itself as an absolute axis, set this flag.
    DWORD device_flags = DIDF_RELAXIS;
    for (size_t i = 0; i < dev_objs.count; i++) {
        if (dev_objs.elems[i].type == DIDFT_ABSAXIS) {
            device_flags = DIDF_ABSAXIS;
        }
    }

    // Fill in data formats.
    noh_arena_save(&hooks_arena);
    LPDIOBJECTDATAFORMAT data_formats = noh_arena_alloc(&hooks_arena, dev_objs.count * sizeof(DIOBJECTDATAFORMAT));
    for (size_t i = 0; i < dev_objs.count; i++) {
        NB_Device_Object_Info *obj_info = &dev_objs.elems[i];
        DWORD type = (obj_info->type == 12) ? DIDFT_BUTTON : DIDFT_AXIS;
        DIOBJECTDATAFORMAT obj_format = {
            .pguid = 0,
            .dwOfs = obj_info->offset,
            .dwType = type | DIDFT_MAKEINSTANCE(obj_info->instance),
            .dwFlags = 0
        };

        data_formats[i] = obj_format;
    }

    DIDATAFORMAT data_format = {
        .dwSize = sizeof(DIDATAFORMAT),
        .dwObjSize = sizeof(DIOBJECTDATAFORMAT),
        .dwFlags = device_flags,
        .dwDataSize = dev_objs.data_size,
        .dwNumObjs = dev_objs.count,
        .rgodf = data_formats
    };
    
    result = IDirectInputDevice_SetDataFormat(deviceInstance, &data_format);
    noh_arena_rewind(&hooks_arena);
    if (result != DI_OK) {
        noh_log(NOH_WARNING, "Could not set data format of device %s: %s.", device.name, print_dierr_hresult(result));
        return DIENUM_CONTINUE;
    }
    device.objects = dev_objs;

    // Acquire device.
    result = IDirectInputDevice_Acquire(deviceInstance);
    if (result != DI_OK) {
        noh_log(NOH_WARNING, "Could not acquire device %s: %s", device.name, print_dierr_hresult(result));
        return DIENUM_CONTINUE;
    }

    device.index = devices->count; // The current count will be the index of this device.
    noh_da_append(devices, device);

    return DIENUM_CONTINUE;
}

bool hooks_initialize() {
    noh_log(NOH_INFO, "Initializing hooks.");

    if (hooks_arena.blocks.count > 0) {
        noh_arena_reset(&hooks_arena);
    } else {
        hooks_arena = noh_arena_init(20 KB);
    }

    LPDIRECTINPUT directInputInstance;
    HRESULT result =  DirectInput8Create(
        GetModuleHandle(NULL), DIRECTINPUT_VERSION, &IID_IDirectInput8A, (void**)&directInputInstance, NULL);
    if (result != DI_OK) {
        noh_log(NOH_ERROR, "Could not create instance of DirectInput: 0x%0X.", print_dierr_hresult(result));
        return false;
    }

    // Initialize devices. Uses the EnumDevices method, which will callback to add_input_device,
    // that fills the list of devices.
    result = IDirectInput_EnumDevices(
        directInputInstance, DI8DEVCLASS_ALL, add_input_device, (LPVOID)directInputInstance, DIEDFL_ATTACHEDONLY);
    if (result != DI_OK) {
        return false;
    }

    // TODO *: Start checking device data in a loop.

    return true;
}

bool hooks_reinitialize() {
    hooks_shutdown();
    noh_arena_reset(&hooks_arena);
    return hooks_initialize();
}

NB_Input_State hooks_get_state(Noh_Arena *arena) {
    (void)arena;
    noh_assert(false && "Not yet implemented.");
    NB_Input_State state = {0};
    return state;
}

void hooks_shutdown() {
    noh_da_free(&hooks_devices);
    noh_assert(false && "Not yet implemented.");
}
