/* main.c - Application main entry point */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <misc/printk.h>
#include <misc/util.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>

static u8_t mfg_data[] = { 0xff, 0xff, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 };

static const struct bt_data ad[] = {
	BT_DATA(BT_DATA_MANUFACTURER_DATA, mfg_data, 10),
};

static void scan_cb(const bt_addr_le_t *addr, s8_t rssi, u8_t adv_type,
		    struct net_buf_simple *buf)
{	

	int n = 0;
	u8_t data[buf->len];
	
	char addr_str[BT_ADDR_LE_STR_LEN];

	if(adv_type == 0x07 ){										//only for extended advertising packets....
	//	printk(" \n scan report adv_type : %d \n", adv_type);
		bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
	//	printk("Device found: %s (RSSI %d)\n", addr_str, rssi);
		printk("buffer len : %d \n", buf->len);

		for( int i=0 ; i<= buf->len; i++){
			data[i] = *(buf->data ++);
		}
		n = buf->len;
		printk("Here is the message: \n");
		for (int i = 0; i < n; i++)
		{
			if((i==30) || (i==60) || (i==90) || (i==120) || (i==150) || (i==160) || (i==180) || (i==210) || (i==240)){
				printk("\n ");	
			}
			printk("0x%02X ", data[i]);		
		}		
		printk("\n");
		printk("\n");
	
	}
}


void main(void)
{
	struct bt_le_scan_param scan_param = {
		.type       = BT_HCI_LE_SCAN_PASSIVE,
		.filter_dup = BT_HCI_LE_SCAN_FILTER_DUP_DISABLE,
		.interval   = 0x0010,
		.window     = 0x0010,
	};
	int err;

	printk("Starting Scanner Demo\n");

	/* Initialize the Bluetooth Subsystem */
	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

	err = bt_le_scan_start(&scan_param, scan_cb);
	if (err) {
		printk("Starting scanning failed (err %d)\n", err);
		return;
	}
	printk("scanning started\n");
}
