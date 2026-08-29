idf_component_register(SRCS usb_host_common.c usb_host_modem.c usb_host_hid.c
                       INCLUDE_DIRS "."
                       REQUIRES utilities mbedtls driver usb
                       PRIV_REQUIRES iot_usbh_modem usb_host_hid)