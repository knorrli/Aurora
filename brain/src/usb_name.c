#include "usb_names.h"

#define MIDI_NAME {'A','u','r','o','r','a',' ','B','r','a','i','n'}
#define MIDI_NAME_LEN 12

struct usb_string_descriptor_struct usb_string_product_name = {
    2 + MIDI_NAME_LEN * 2,
    3,
    MIDI_NAME
};
