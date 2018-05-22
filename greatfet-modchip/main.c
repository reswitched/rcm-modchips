/*
 * This file is part of GreatFET
 */

#include <stddef.h>
#include <errno.h>

#include <libopencm3/cm3/vector.h>
#include <libopencm3/lpc43xx/m4/nvic.h>

#include <greatfet_core.h>
#include <usb_type.h>

#include <usb.h>
#include <usb_host.h>
#include <usb_queue_host.h>
#include <usb_registers.h>

#include "usb_host_stack.h"

// Endpoint objects -- represent our USB data channels.
ehci_queue_head_t *control_endpoint;
ehci_queue_head_t *rcm_endpoint;

usb_peripheral_t usb_peripherals[] = {
	{ .controller = 0, },
	{ .controller = 1, }
};

void set_up_host_controller(void)
{
	// Initialize the USB host controller.
	usb_host_initialize_storage_pools();

	// Set up the device in host mode...
	usb_controller_reset(&usb_peripherals[1]);
	usb_host_init(&usb_peripherals[1]);

	// Provide VBUS to the target, if possible.
	// TODO: disable this when we're not host powered
	usb_provide_vbus(&usb_peripherals[1]);

	// Enable the USB controller-- this will allow start the point
	// where interrupts can be issued.
	usb_run(&usb_peripherals[1]);
}

void wait_for_device(void)
{
	// Reset the device...
	delay(100 * 1000);
	usb_host_reset_device(&usb_peripherals[1]);
	delay(100 * 1000);

	// Repeatedly reset the device until it shows up.
	// TODO: Do we need to do this? Are our delays just off?
	while (!(USB_REG(1)->PORTSC1 & USB0_PORTSC1_H_CCS)) {
		// Reset the device...
		delay(100 * 1000);
		usb_host_reset_device(&usb_peripherals[1]);
		delay(100 * 1000);
	}
}

// Magic numbers for the RCM communications.
enum {
	USB_ADDRESS_DEFAULT  = 0,
	USB_ENDPOINT_NUMBER_CONTROL = 0,
	USB_ENDPOINT_NUMBER_RCM = 1,

	RCM_ACTIVE_CONFIGURATION = 1,
	RCM_MAX_PACKET_SIZE_EP0 = 64,
	RCM_MAX_PACKET_SIZE_RCM_EP = 512,

	SWITCH_RCM_VID = 0x0955,
	SWITCH_RCM_PID = 0x7321,
};


void initialize_endpoints(void)
{
	// Set up the standard control endpoint.
	control_endpoint = usb_host_set_up_asynchronous_endpoint_queue(
		&usb_peripherals[1],
		NULL, // Allocate a new endpoint.
		USB_ADDRESS_DEFAULT,
		USB_ENDPOINT_NUMBER_CONTROL,
		USB_SPEED_FULL, // TODO: switch this to the other controller, and High speed
		true,  // This is a control endpoint.
		false, // We'll manually specify the data toggle.
		RCM_MAX_PACKET_SIZE_EP0);

	// Set up the RCM endpoint; used for the actual recovery mode data.
	rcm_endpoint = usb_host_set_up_asynchronous_endpoint_queue(
		&usb_peripherals[1],
		NULL, // Allocate a new endpoint.
		USB_ADDRESS_DEFAULT,
		USB_ENDPOINT_NUMBER_RCM,
		USB_SPEED_FULL, // TODO: switch this to the other controller, and High speed
		false, // This is NOT a control endpoint.
		true,  // We'll let the controller handle DATA toggling for us.
		RCM_MAX_PACKET_SIZE_RCM_EP);
}



/**
 * Validates that the connected device is a Nintendo Switch.
 *
 * @return 0 on success, or an error code on failure
 */
int validate_connected_device(void)
{
	// Read the device descriptor, which contains our VID and PID.
	usb_descriptor_device_t device_descriptor;
	usb_host_read_device_descriptor(&usb_peripherals[1], control_endpoint, &device_descriptor);

	// If our VID or PID don't match, fail out.
	if (device_descriptor.idVendor != SWITCH_RCM_VID)
		return EINVAL;
	if (device_descriptor.idProduct != SWITCH_RCM_PID)
		return EINVAL;

	return 0;
}


int main(void) 
{
	int rc;

	// Perform the core GreatFET setup.
	cpu_clock_init();
	cpu_clock_pll1_max_speed();
	pin_setup();
	rtc_init();

	set_up_host_controller();
	wait_for_device();
	led_on(LED1);

	initialize_endpoints();
	rc = validate_connected_device();
	if (rc) {
		led_on(LED4);
		return rc;
	}

	led_on(LED2);

	while(true);
	return 0;
}
