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
#include <string.h>




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

	if(adv_type == 0x07){
		printk("scan report adv_type : %d \n", adv_type);
		bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
		printk("Device found: %s (RSSI %d)\n", addr_str, rssi);
		printk("buffer len : %d \n", buf->len);
	//	snprintk(data ,buf->len, buf->data);
		for( int i=0 ; i<= buf->len; i++){
			data[i] = *(buf->data ++);
		}
		//char buffer[buf->len];
		//bzero(buffer, buf->len);
		//n = read(buf->data,buffer,buf->len);
		//if (n < 0) printf("ERROR reading from socket");
		n = buf->len;
		printk("Here is the message: \n");
		for (int i = 0; i < n; i++)
		{
			printk("0x%02X ", data[i]);
		}
		printk("\n");
		printk("\n");
	/*	printk("buffer data :  ");
		for(i=0 ; i< buf->len ; i++){
			printk("%d ", data[i]);
		}
		printk(" \n");
		mfg_data[2]++;
	*/
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

	printk("Starting Scanner/Advertiser Demo\n");

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
	//printk("scanning started\n");
	do {
		k_sleep(K_MSEC(400));

		/* Start advertising */
		err = bt_le_adv_start(BT_LE_ADV_NCONN, ad, ARRAY_SIZE(ad),
				      NULL, 0);
		if (err) {
			printk("Advertising failed to start (err %d)\n", err);
			return;
		}
		//printk("Advertising started\n");
		k_sleep(K_MSEC(400));

		err = bt_le_adv_stop();
		if (err) {
			printk("Advertising failed to stop (err %d)\n", err);
			return;
		}
	} while (1);
}
