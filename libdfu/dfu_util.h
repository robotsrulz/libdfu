#ifndef DFU_UTIL_H
#define DFU_UTIL_H

/* USB string descriptor should contain max 126 UTF-16 characters
 * but 253 would even accomodate any UTF-8 encoding */
#define MAX_DESC_STR_LEN 253

enum mode {
	MODE_NONE,
	MODE_VERSION,
	MODE_LIST,
	MODE_DETACH,
	MODE_UPLOAD,
	MODE_DOWNLOAD
};

extern char *match_path;
extern int match_vendor;
extern int match_product;
extern int match_vendor_dfu;
extern int match_product_dfu;
extern int match_config_index;
extern int match_iface_index;
extern int match_iface_alt_index;
extern const char *match_iface_alt_name;
extern const char *match_serial;
extern const char *match_serial_dfu;

struct dfu_if *probe_configuration(libusb_device *dev, libusb_device_handle *devh, struct libusb_device_descriptor *desc);
void disconnect_device(struct dfu_if *dfu_root);

char *get_path(libusb_device *dev);

#endif /* DFU_UTIL_H */
