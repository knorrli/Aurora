#include "usb_names.h"

#define MIDI_NAME {'A','u','r','o','r','a',' ','C','o','n','t','r','o','l','l','e','r'}
#define MIDI_NAME_LEN 17

struct usb_string_descriptor_struct usb_string_product_name = {
    2 + MIDI_NAME_LEN * 2,
    3,
    MIDI_NAME
};
