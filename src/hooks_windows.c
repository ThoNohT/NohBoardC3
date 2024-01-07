// Also includes noh.h
#include "hooks.c" // Common code used by all platforms.

// This arena is used all throughout this file for storing temporary data.
static Noh_Arena hooks_arena = {0};

NB_Input_Devices hooks_devices = {0};

static char *guid_to_cstr(Noh_Arena *arena, const GUID *guid) {
    return noh_arena_sprintf(arena, "%0X-%0X-%0X-%0X%0X-%0X%0X%0X%0X%0X%0X", 
        guid->Data1, // First 8 hex digits.
        guid->Data2, // First group of 4 hex digits.
        guid->Data3, // Second group of 4 hex digits.
        guid->Data4[0], guid->Data4[1], // Third group of 4 hexdigits.
         // Final 12 hex digits.
        guid->Data4[2], guid->Data4[3], guid->Data4[4], guid->Data4[5], guid->Data4[6], guid->Data4[7]);
}

// Find the device at the specified index, returns NULL if out of bounds.
NB_Input_Device *hooks_find_device_by_index(size_t device_index) {
    (void)device_index;
    noh_assert(false && "Not yet implemented.");
    return (NB_Input_Device *)NULL;
}

static int add_input_device(LPCDIDEVICEINSTANCE instance, LPVOID pvRef) {
    NB_Input_Device device = {0};

    LPDIRECTINPUT directInput = (LPDIRECTINPUT)pvRef;

    // TODO *: The same device can be returned twice in the enumeration, if it supports different types.
    // Do we just skip the second one, or maybe support multiple device types?

    // Determine type from least significant byte of dwDevType.
    switch (instance->dwDevType & 0xFF) {
        case DI8DEVTYPE_KEYBOARD:
            device.type = NB_Keyboard;
            break;

        case DI8DEVTYPE_MOUSE: 
            if ((((instance->dwDevType << 8) & 0xFF) & DI8DEVTYPEMOUSE_TOUCHPAD) > 0) {
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

    HRESULT result = IDirectInput_CreateDevice(directInput, &instance->guidInstance, &deviceInstance, NULL);
    if (result != DI_OK) {
        noh_log(NOH_WARNING, "Could not create instance device %s: %i.", device.name, result);
    } else {
        device.instance = deviceInstance;
        noh_da_append(&hooks_devices, device);
    }



    return DIENUM_CONTINUE;
}

static bool init_devices(LPDIRECTINPUT directInput, NB_Input_Devices *devices) {
    HRESULT result = IDirectInput_EnumDevices(directInput, DI8DEVCLASS_ALL, add_input_device, (LPVOID)directInput, DIEDFL_ATTACHEDONLY);

    (void)result;
    (void)devices;
    noh_assert(false && "Not yet implemented.");
    return false;
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
        noh_log(NOH_ERROR, "Could not create instance of DirectInput: 0x%0X.", result);
        if (result == DIERR_BETADIRECTINPUTVERSION) noh_log(NOH_ERROR, "DIERR_BETADIRECTINPUTVERSION");
        if (result == DIERR_INVALIDPARAM) noh_log(NOH_ERROR, "DIERR_INVALIDPARAM");
        if (result == DIERR_OLDDIRECTINPUTVERSION) noh_log(NOH_ERROR, "DIERR_OLDDIRECTINPUTVERSION");
        if (result == DIERR_OUTOFMEMORY) noh_log(NOH_ERROR, "DIERR_OUTOFMEMORY");
        return false;
    }

    if (!init_devices(directInputInstance, &hooks_devices)) {
        return false;
    }

    noh_assert(false && "Not yet implemented.");
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
