#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <esp_timer.h>

#include "rc52x_transport.h"
#include "rc52x.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "driver/spi_master.h"
#ifdef CONFIG_IDF_TARGET_ESP32
#define HW_NR    HSPI_HOST


#define SPI_SCK 17
#define SPI_MISO 16
#define SPI_MOSI  4
#define SPI_CS 2


#else
#define HW_NR    SPI2_HOST

#define SPI_SCK 17
#define SPI_MISO 16
#define SPI_MOSI  4
#define SPI_CS 2

#endif

int find_card(bs_pdc_t *pdc, picc_t *picc) {
	uint8_t buff[10] = { 0 };
	rc52x_result_t status = 0;
	status = PICC_RequestA(pdc, picc);
	if (status != STATUS_OK && status != STATUS_COLLISION)
		return status;

	status = PICC_Select(pdc, picc, 0);
	if (status) {
		return status;
	}

	return status;
}

int test_card_read(bs_pdc_t *pdc, picc_t *picc) {
	memset(picc, 0, sizeof(picc_t));

	if (!find_card(pdc, picc)) {
		printf("Card found: UUID: ");
		for (int i = 0; i < picc->uid_size; i++) {
			printf("%02X ", picc->uid[i]);
		}
		puts("");

		// Read 4 pages (16 bytes), starting at page 8
		uint8_t read_buffer[16] = { 0 };
		int result = MIFARE_READ(pdc, picc, 8, read_buffer);
		if (result) {
			puts("Failed to read page 8");
			return -1;
		}

		if (read_buffer[0] != 0xDE && read_buffer[1] != 0xAD
				&& read_buffer[3] != 0xBE && read_buffer[4] != 0xEF) {
			puts(
					"Content of page 8 is not DE AD BE EF, writing DE AD BE EF");
			uint8_t write_buffer[4];
			write_buffer[0] = 0xDE;
			write_buffer[1] = 0xAD;
			write_buffer[3] = 0xBE;
			write_buffer[4] = 0xEF;
			// Read 1 page (4 bytes), starting at page 8
			result = MFU_Write(pdc, picc, 8, write_buffer);
			if (result) {
				puts("Failed to WRITE page 8");
				return -1;
			}
		} else {
			puts("Content of page 8 is DE AD BE EF");
		}

		PICC_HaltA(pdc);

	} else {
		//printf("No Card found\n");
		return -1;
	}
	return 0;
}

uint32_t get_time_us(void) {
	return esp_timer_get_time();
}

uint32_t get_time_ms(void) {
	return get_time_us() / 1000;
}

void delay_time_ms(uint32_t ms) {
	// BUSY WAITING
	uint32_t now = get_time_us();
	uint32_t until = now + (1000 * ms);
	while (get_time_us() < until)
		;
}

void rfid5_spi_init(rc52x_t *rc52x) {
	static bshal_spim_instance_t rfid_spi_config;
	rfid_spi_config.frequency = 1000000; // SPI speed for MFRC522 = 10 MHz
	rfid_spi_config.bit_order = 0; //MSB
	rfid_spi_config.mode = 0;
	rfid_spi_config.hw_nr = HW_NR;

	rfid_spi_config.sck_pin = SPI_SCK;
	rfid_spi_config.miso_pin = SPI_MISO;
	rfid_spi_config.mosi_pin = SPI_MOSI;
	rfid_spi_config.cs_pin = SPI_CS;

	rfid_spi_config.rs_pin = -1;

	rc52x->delay_ms = delay_time_ms;
	rc52x->get_time_ms = get_time_ms;
	bshal_spim_init(&rfid_spi_config);
	rc52x->transport_type = bshal_transport_spi;
	rc52x->transport_instance.spim = &rfid_spi_config;
}

void app_main(void) {
	int result;
	rc52x_t rc52x_spi;
	rfid5_spi_init(&rc52x_spi);
	rc52x_init(&rc52x_spi);

	uint8_t version;
	version = -1;
	rc52x_get_chip_version(&rc52x_spi, &version);
	printf("VERSION %02X\n", version);
	printf("NAME    %s\n", rc52x_get_chip_name(&rc52x_spi));

	picc_t picc = { 0 };

	while (1) {
		vTaskDelay(1000 / portTICK_PERIOD_MS);
		memset(&picc, 0, sizeof(picc_t));
		result = test_card_read(&rc52x_spi, &picc);
	}

}
