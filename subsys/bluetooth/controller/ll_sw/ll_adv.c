/*
 * Copyright (c) 2016-2017 Nordic Semiconductor ASA
 * Copyright (c) 2016 Vinayak Kariappa Chettimada
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include <zephyr.h>
#include <bluetooth/hci.h>

#include "util/util.h"

#include "pdu.h"
#include "ctrl.h"
#include "ll.h"

#include "hal/debug.h"

#include "ll_filter.h"
#include "ll_adv.h"

static struct ll_adv_set ll_adv;
//static u8_t flag;
static u8_t *set_data_ptr = NULL;


struct ll_adv_set *ll_adv_set_get(void)
{
	return &ll_adv;
}

#if defined(CONFIG_BT_CTLR_ADV_EXT)
u32_t ll_adv_params_set(u8_t Adv_handle, u16_t evt_prop, u16_t interval,
			u8_t own_addr_type,
			u8_t direct_addr_type, u8_t const *const direct_addr,
			u8_t chan_map, u8_t filter_policy, u8_t *tx_pwr,
			u8_t phy_p, u8_t skip, u8_t phy_s, u8_t sid, u8_t sreq)
{
	u8_t const pdu_adv_type[] = {PDU_ADV_TYPE_ADV_IND,
				     PDU_ADV_TYPE_DIRECT_IND,
				     PDU_ADV_TYPE_SCAN_IND,
				     PDU_ADV_TYPE_NONCONN_IND,
				     PDU_ADV_TYPE_DIRECT_IND,
				     PDU_ADV_TYPE_EXT_IND};
#else /* !CONFIG_BT_CTLR_ADV_EXT */
u32_t ll_adv_params_set(u16_t interval, u8_t adv_type,
			u8_t own_addr_type, u8_t direct_addr_type,
			u8_t const *const direct_addr, u8_t chan_map,
			u8_t filter_policy)
{
	u8_t const pdu_adv_type[] = {PDU_ADV_TYPE_ADV_IND,
				     PDU_ADV_TYPE_DIRECT_IND,
				     PDU_ADV_TYPE_SCAN_IND,
				     PDU_ADV_TYPE_NONCONN_IND,
				     PDU_ADV_TYPE_DIRECT_IND};
#endif /* !CONFIG_BT_CTLR_ADV_EXT */

	struct radio_adv_data *radio_adv_data;
	struct radio_adv_data *radio_aux_adv_data;
	struct pdu_adv *pdu;
	struct pdu_adv *aux_pdu;
	
	if (ll_adv_is_enabled()) {
		return BT_HCI_ERR_CMD_DISALLOWED;
	}

#if defined(CONFIG_BT_CTLR_ADV_EXT)
	/* TODO: check and fail (0x12, invalid HCI cmd param) if invalid
	 * evt_prop bits.
	 */
	ll_adv.max_skip = skip;
	
	u8_t adv_type;
	ll_adv.phy_p = BIT(0);
	
	if(!(evt_prop & BIT(4))){								//bit 4 =0 for extended ADV
		adv_type = 0x05;
	}			
	/* extended */
	//if (adv_type > 0x04) {
		/* legacy */
		if (evt_prop & BIT(4)) {
			u8_t const leg_adv_type[] = { 0x03, 0x04, 0x02, 0x00};

			adv_type = leg_adv_type[evt_prop & 0x03];

			/* high duty cycle directed */
			if (evt_prop & BIT(3)) {
				adv_type = 0x01;
			}
		} else {
			/* - Connectable and scannable not allowed;
			 * - High duty cycle directed connectable not allowed
			 */
			if (((evt_prop & 0x03) == 0x03) ||
			    ((evt_prop & 0x0C) == 0x0C)) {
				return 0x12; /* invalid HCI cmd param */
			}

			adv_type = 0x05; /* PDU_ADV_TYPE_EXT_IND */

			ll_adv.phy_p = phy_p;
		}
	//}
#endif /* CONFIG_BT_CTLR_ADV_EXT */

	/* remember params so that set adv/scan data and adv enable
	 * interface can correctly update adv/scan data in the
	 * double buffer between caller and controller context.
	 */
	/* Set interval for Undirected or Low Duty Cycle Directed Advertising */
	if (adv_type == 0x01) {
		ll_adv.interval = 0;  				//interval = 0 for ADV_DIRECT_IND
	} else {
		ll_adv.interval = interval;
	}
	ll_adv.chan_map = chan_map;  			
	ll_adv.filter_policy = filter_policy;
	
	/* update the "current" primary adv data and Aux packet */
	radio_adv_data = radio_adv_data_get();
	radio_aux_adv_data = radio_aux_adv_data_get();

	pdu = (struct pdu_adv *)&radio_adv_data->data[radio_adv_data->last][0];
	aux_pdu = (struct pdu_adv *)&radio_aux_adv_data->data[radio_aux_adv_data->last][0];

	pdu->type = pdu_adv_type[adv_type];
	aux_pdu->type = pdu->type;
	
	pdu->rfu = 0;
	aux_pdu->rfu = pdu->rfu;

	if (IS_ENABLED(CONFIG_BT_CTLR_CHAN_SEL_2) && 
	    ((pdu->type == PDU_ADV_TYPE_ADV_IND) ||
	     (pdu->type == PDU_ADV_TYPE_DIRECT_IND))) {
		pdu->chan_sel = 1;           // could be same for ext adv  LE Channel Selection Algorithm #2 feature 
		aux_pdu->chan_sel = pdu->chan_sel;
	} else {
		pdu->chan_sel = 0;
		aux_pdu->chan_sel = pdu->chan_sel;
	}

#if defined(CONFIG_BT_CTLR_PRIVACY)
	ll_adv.own_addr_type = own_addr_type;
	if (ll_adv.own_addr_type == BT_ADDR_LE_PUBLIC_ID ||
	    ll_adv.own_addr_type == BT_ADDR_LE_RANDOM_ID) {
		ll_adv.id_addr_type = direct_addr_type;
		memcpy(&ll_adv.id_addr, direct_addr, BDADDR_SIZE);
	}
#endif /* CONFIG_BT_CTLR_PRIVACY */
	pdu->tx_addr =  own_addr_type & 0x1;
	aux_pdu->tx_addr = 0;						//not sending transmitting address in aux packet
	pdu->rx_addr = 0;  							
	aux_pdu->rx_addr = pdu->rx_addr;
	if(evt_prop & BIT(2)){							// for directed ADV wuth public address
		pdu->rx_addr = 0;//direct_addr_type;
	}

	if (pdu->type == PDU_ADV_TYPE_DIRECT_IND) {
		pdu->rx_addr = direct_addr_type;
		memcpy(&pdu->direct_ind.tgt_addr[0], direct_addr, BDADDR_SIZE);
		pdu->len = sizeof(struct pdu_adv_direct_ind);

#if defined(CONFIG_BT_CTLR_ADV_EXT)
	} else if (pdu->type == PDU_ADV_TYPE_EXT_IND) {
		
		struct pdu_adv_com_ext_adv *p;  		//payload with hdr len, adv mode
		struct ext_adv_hdr *h;					//extnd header with adi, auxptr,
		struct ext_adv_adi *adi_ptr;					// adi= did, sid
		struct ext_adv_aux_ptr *ap;				// aux ptr

		/* aux packet pointers */
		struct pdu_adv_com_ext_adv *aux_p;  		//payload with hdr len, adv mode
		struct ext_adv_hdr *aux_h;					//extnd header with adi, auxptr,
		struct ext_adv_adi *aux_adi_ptr;					// adi= did, sid
		
		u8_t *ptr;
		u8_t len;
		u8_t *aux_ptr;
		u8_t aux_len;

		p = (void *)&pdu->adv_ext_ind;
		h = (void *)p->ext_hdr_adi_adv_data;
		ptr = (u8_t *)h + sizeof(*h);

		/* assign address to aux pointers */
		aux_p = (void *)&aux_pdu->adv_ext_ind;
		aux_h = (void *)aux_p->ext_hdr_adi_adv_data;
		aux_ptr = (u8_t *)aux_h + sizeof(*aux_h);


		/* No ACAD and no AdvData */
		p->ext_hdr_len = 0;
		p->adv_mode = 0x00;//evt_prop & 0x03;

		aux_p->ext_hdr_len = p->ext_hdr_len;
		aux_p->adv_mode = p->adv_mode;


		/* Zero-init header flags */
		*(u8_t *)h = 0;
		*(u8_t *)aux_h = 0;


		/* AdvA flag */
		if (!((evt_prop & BIT(5)) && !p->adv_mode && (phy_p != BIT(2)))) {
			/* TODO: optional on 1M */
			h->adv_addr = 1;

			/* NOTE: AdvA is filled at enable */
			ptr += BDADDR_SIZE;
		}

		/* TODO: TargetA flag */
		if(evt_prop & BIT(2)){
			h->tgt_addr = 1;
			ptr += BDADDR_SIZE;
		}
	
		/* TODO: ADI flag */
		if(evt_prop & BIT(6)){
			h->adi = 1;
			aux_h->adi = h->adi;

			adi_ptr = ptr;
			aux_adi_ptr = aux_ptr;

			ptr += sizeof(*adi_ptr);
			aux_ptr += sizeof(*aux_adi_ptr);

		}
	
		/* TODO: AuxPtr flag */
		if(evt_prop & BIT(6)){
			h->aux_ptr = 1;
			ap = ptr;
		
			ptr += sizeof(*ap);
		}
	
		/* TODO: SyncInfo flag */

		/* Tx Power flag */
		if (evt_prop & BIT(6)) {
			h->tx_pwr = 1;
			aux_h->tx_pwr = h->tx_pwr;
			ptr ++;
			aux_ptr ++;
		}

		/* Calc primary PDU len */
		len = ptr - (u8_t *)p;
		if (len > (offsetof(struct pdu_adv_com_ext_adv,
				    ext_hdr_adi_adv_data) + sizeof(*h))) {
			p->ext_hdr_len = (len -
				offsetof(struct pdu_adv_com_ext_adv,
					 ext_hdr_adi_adv_data));
			pdu->len = len;
		} else {
			pdu->len = offsetof(struct pdu_adv_com_ext_adv,
					    ext_hdr_adi_adv_data);
		}


		/* Calc AUX PDU len */
		aux_len = aux_ptr - (u8_t *)aux_p;
		if (aux_len > (offsetof(struct pdu_adv_com_ext_adv,
				    ext_hdr_adi_adv_data) + sizeof(*aux_h))) {
			aux_p->ext_hdr_len = (aux_len -
				offsetof(struct pdu_adv_com_ext_adv,
					 ext_hdr_adi_adv_data));
			aux_pdu->len = aux_len;
		} else {
			aux_pdu->len = offsetof(struct pdu_adv_com_ext_adv,
					    ext_hdr_adi_adv_data);
		}

		/* Start filling primary and Aux PDU payload based on flags */
		ptr = (u8_t *)h + sizeof(*h);
		aux_ptr = (u8_t *)aux_h + sizeof(*aux_h);

		/* Advertising Address */
		if(h->adv_addr){
			ptr += BDADDR_SIZE;
		}

		/* target address */
		if(h->tgt_addr){
			memcpy(ptr, direct_addr, BDADDR_SIZE);
			ptr += BDADDR_SIZE;
		}

		/* TODO: ADI */
		if(h->adi){
			adi_ptr->did = 0x00;
			adi_ptr->sid = 0x03;//sid & 0x0f;
			ptr += sizeof(*adi_ptr);

			/* filling aux pdu */
			aux_adi_ptr->did = adi_ptr->did;
			aux_adi_ptr->sid = 0x02;//adi_ptr->sid;
			aux_ptr += sizeof(*aux_adi_ptr);
			
		}

		/* AuxPtr */
		if(h->aux_ptr){
			ap->chan_idx = 0x03;
			ap->ca = 0;
			ap->offs = 00;	
			ap->offs_units = 0;
			ap->phy = 0;
			ptr += sizeof(*ap);
		}
		/* TODO: AdvData */
		// filling all the aux packet data in   ll_adv_data_set function


		/* TODO: ACAD */

		/* Tx Power */
		if (h->tx_pwr) {
			u8_t _tx_pwr;

			_tx_pwr = 0;
			if (tx_pwr) {
				if (*tx_pwr != 0x7F) {
					_tx_pwr = *tx_pwr;
				} else {
					*tx_pwr = _tx_pwr;
				}
			}

			*ptr = _tx_pwr;
			*aux_ptr = _tx_pwr;

			ptr++;
			aux_ptr++;

			
		}

		set_data_ptr = aux_ptr; 
		/* TODO: SyncInfo */

		

		
		

		/* NOTE: TargetA, filled at enable and RPA timeout  but for direct adv address filled at set param function*/

		/* NOTE: AdvA, filled at enable and RPA timeout */
		
#endif /* CONFIG_BT_CTLR_ADV_EXT */

	} else if (pdu->len == 0) {
		pdu->len = BDADDR_SIZE;
	}

	/* update the current scan data */
	radio_adv_data = radio_scan_data_get();
	pdu = (struct pdu_adv *)&radio_adv_data->data[radio_adv_data->last][0];
	pdu->type = PDU_ADV_TYPE_SCAN_RSP;
	pdu->rfu = 0;
	pdu->chan_sel = 0;
	pdu->tx_addr = own_addr_type & 0x1;
	pdu->rx_addr = 0;
	if (pdu->len == 0) {
		pdu->len = BDADDR_SIZE;
	}

	return 0;
}

void ll_adv_data_set(u8_t len, u8_t const *const data)
{

//#if defined(CONFIG_BT_CTLR_ADV_EXT)
	
/*	if ((IS_ENABLED(CONFIG_BT_CTLR_ADV_EXT) &&															//disable it if you have to update
	     (aux_pdu->type == PDU_ADV_TYPE_EXT_IND))){
			 memcpy(&set_data_ptr, data, len);
			 aux_pdu->len += len;
*/			 /* dont know data has to add in extened  header len or only pdu len */
//		}

//#else	/* CONFIG_BT_CTLR_ADV_EXT */
	struct radio_adv_data *radio_adv_data;
	struct pdu_adv *prev;
	struct pdu_adv *pdu;
	u8_t last;


	/* Dont update data if directed or extended advertising. */
	radio_adv_data = radio_adv_data_get();
	prev = (struct pdu_adv *)&radio_adv_data->data[radio_adv_data->last][0];
	if(prev->type == PDU_ADV_TYPE_EXT_IND){
		/* set data into aux packet */
	
		struct radio_adv_data *radio_aux_adv_data;
		struct pdu_adv *aux_pdu;
		u8_t last;

		radio_aux_adv_data = radio_aux_adv_data_get();
		aux_pdu =  (struct pdu_adv *)&radio_aux_adv_data->data[radio_aux_adv_data->last][0]; 			//cofused about data[radio_aux_adv_data->last][0] name

		memcpy(set_data_ptr, data, len);
		aux_pdu->len += len;
	}else{
		if ((prev->type == PDU_ADV_TYPE_DIRECT_IND)) {
		/* TODO: remember data, to be used if type is changed using
		 * parameter set function ll_adv_params_set afterwards.
		 */
		
		return;
		}

		/* use the last index in double buffer, */
		if (radio_adv_data->first == radio_adv_data->last) {
			last = radio_adv_data->last + 1;
			if (last == DOUBLE_BUFFER_SIZE) {
				last = 0;
			}
		} else {
			last = radio_adv_data->last;
		}

		/* update adv pdu fields. */
		pdu = (struct pdu_adv *)&radio_adv_data->data[last][0];
		pdu->type = prev->type;
		pdu->rfu = 0;

		if (IS_ENABLED(CONFIG_BT_CTLR_CHAN_SEL_2)) {
			pdu->chan_sel = prev->chan_sel;
		} else {
			pdu->chan_sel = 0;
		}

		pdu->tx_addr = prev->tx_addr;
		pdu->rx_addr = prev->rx_addr;
		
		
		memcpy(&pdu->adv_ind.addr[0], &prev->adv_ind.addr[0], BDADDR_SIZE);
		memcpy(&pdu->adv_ind.data[0], data, len);
		pdu->len = BDADDR_SIZE + len;
		
		/* commit the update so controller picks it. */
		radio_adv_data->last = last;

	}
	
//#endif	/* CONFIG_BT_CTLR_ADV_EXT */
}

void ll_scan_data_set(u8_t len, u8_t const *const data)
{
	struct radio_adv_data *radio_scan_data;
	struct pdu_adv *prev;
	struct pdu_adv *pdu;
	u8_t last;

	/* use the last index in double buffer, */
	radio_scan_data = radio_scan_data_get();
	if (radio_scan_data->first == radio_scan_data->last) {
		last = radio_scan_data->last + 1;
		if (last == DOUBLE_BUFFER_SIZE) {
			last = 0;
		}
	} else {
		last = radio_scan_data->last;
	}

	/* update scan pdu fields. */
	prev = (struct pdu_adv *)
	       &radio_scan_data->data[radio_scan_data->last][0];
	pdu = (struct pdu_adv *)&radio_scan_data->data[last][0];
	pdu->type = PDU_ADV_TYPE_SCAN_RSP;
	pdu->rfu = 0;
	pdu->chan_sel = 0;
	pdu->tx_addr = prev->tx_addr;
	pdu->rx_addr = 0;
	pdu->len = BDADDR_SIZE + len;
	memcpy(&pdu->scan_rsp.addr[0], &prev->scan_rsp.addr[0], BDADDR_SIZE);
	memcpy(&pdu->scan_rsp.data[0], data, len);

	/* commit the update so controller picks it. */
	radio_scan_data->last = last;
}

u32_t ll_adv_enable(u8_t enable)
{
	struct radio_adv_data *radio_scan_data;
	struct radio_adv_data *radio_adv_data;
	//struct radio_adv_data *radio_aux_adv_data;
	u8_t   rl_idx = FILTER_IDX_NONE;
	struct pdu_adv *pdu_scan;
	struct pdu_adv *pdu_adv;
	//struct pdu_adv *aux_pdu_adv;
	u32_t status;

	if (!enable) {
		return radio_adv_disable();
	} else if (ll_adv_is_enabled()) {
		return 0;
	}

	/* TODO: move the addr remembered into controller
	 * this way when implementing Privacy 1.2, generated
	 * new resolvable addresses can be used instantly.
	 */

	/* remember addr to use and also update the addr in
	 * both adv and scan response PDUs.
	 */
	radio_adv_data = radio_adv_data_get();
	//radio_aux_adv_data = radio_aux_adv_data_get();

	radio_scan_data = radio_scan_data_get();
	pdu_adv = (struct pdu_adv *)&radio_adv_data->data
			[radio_adv_data->last][0];
	pdu_scan = (struct pdu_adv *)&radio_scan_data->data
			[radio_scan_data->last][0];
//	aux_pdu_adv = (struct pdu_adv *)&radio_aux_adv_data->data
//			[radio_aux_adv_data->last][0];

	if (0) {

#if defined(CONFIG_BT_CTLR_ADV_EXT)
	} else if (pdu_adv->type == PDU_ADV_TYPE_EXT_IND) {
		struct pdu_adv_com_ext_adv *p;
		struct ext_adv_hdr *h;
		u8_t *ptr;

		p = (void *)&pdu_adv->adv_ext_ind;
		h = (void *)p->ext_hdr_adi_adv_data;
		ptr = (u8_t *)h + sizeof(*h);
		
		/* AdvA, fill here at enable */
		if (h->adv_addr) {
			memcpy(ptr, ll_addr_get(pdu_adv->tx_addr, NULL),
			       BDADDR_SIZE);

			ptr += BDADDR_SIZE;
			
		}

		/* TODO: TargetA, fill here at enable */
	/*	if (h->adv_addr) {
			memcpy(ptr, ll_addr_get(pdu_adv->rx_addr, NULL),
			       BDADDR_SIZE);
			
		}
	*/

#endif /* CONFIG_BT_CTLR_ADV_EXT */
	} else {
		bool priv = false;
#if defined(CONFIG_BT_CTLR_PRIVACY)
		/* Prepare whitelist and optionally resolving list */
		ll_filters_adv_update(ll_adv.filter_policy);

		if (ll_adv.own_addr_type == BT_ADDR_LE_PUBLIC_ID ||
		    ll_adv.own_addr_type == BT_ADDR_LE_RANDOM_ID) {
			/* Look up the resolving list */
			rl_idx = ll_rl_find(ll_adv.id_addr_type,
					    ll_adv.id_addr, NULL);

			if (rl_idx != FILTER_IDX_NONE) {
				/* Generate RPAs if required */
				ll_rl_rpa_update(false);
			}

			ll_rl_pdu_adv_update(rl_idx, pdu_adv);
			ll_rl_pdu_adv_update(rl_idx, pdu_scan);
			priv = true;
		}
#endif /* !CONFIG_BT_CTLR_PRIVACY */
		if (!priv) {
			memcpy(&pdu_adv->adv_ind.addr[0],
			       ll_addr_get(pdu_adv->tx_addr, NULL),
			       BDADDR_SIZE);
			memcpy(&pdu_scan->scan_rsp.addr[0],
			       ll_addr_get(pdu_adv->tx_addr, NULL),
			       BDADDR_SIZE);
		}
	}
#if defined(CONFIG_BT_CTLR_ADV_EXT)
	status = radio_adv_enable(ll_adv.phy_p, ll_adv.interval,
				  ll_adv.chan_map, ll_adv.filter_policy,
				  rl_idx, ll_adv.max_skip);
#else /* !CONFIG_BT_CTLR_ADV_EXT */
	status = radio_adv_enable(ll_adv.interval, ll_adv.chan_map,
				  ll_adv.filter_policy, rl_idx);
#endif /* !CONFIG_BT_CTLR_ADV_EXT */

	return status;
}
