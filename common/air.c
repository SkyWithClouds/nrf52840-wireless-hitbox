/* SPDX-License-Identifier: MIT */
#include "air.h"
#include <esb.h>
#include <zephyr/kernel.h>
#include <zephyr/random/random.h>
#include <zephyr/drivers/clock_control/nrf_clock_control.h>

static bool prx;
static atomic_t busy;
static void event(const struct esb_evt *e)
{
	if (!prx && e->evt_id == ESB_EVENT_TX_FAILED) {
		esb_flush_tx(); /* ESB retains a failed PTX payload. Application retries in order. */
		atomic_clear(&busy);
	} else if (!prx && e->evt_id == ESB_EVENT_TX_SUCCESS) {
		atomic_clear(&busy);
	}
}

uint64_t air_nonce(void)
{
	uint64_t id;
	do {
		id = sys_rand64_get();
	} while (!id);
	return id;
}

int air_start(bool receiver)
{
	struct onoff_client client = {0};
	struct onoff_manager *manager = z_nrf_clock_control_get_onoff(CLOCK_CONTROL_NRF_SUBSYS_HF);
	sys_notify_init_spinwait(&client.notify);
	int rc = onoff_request(manager, &client);
	if (rc < 0) return rc;
	int result;
	do {
		rc = sys_notify_fetch_result(&client.notify, &result);
	} while (rc == -EAGAIN);
	if (rc || result) return rc ? rc : result;

	atomic_clear(&busy);
	prx = receiver;
	struct esb_config cfg = ESB_DEFAULT_CONFIG;
	cfg.mode = prx ? ESB_MODE_PRX : ESB_MODE_PTX;
	cfg.protocol = ESB_PROTOCOL_ESB_DPL;
	cfg.bitrate = ESB_BITRATE_2MBPS;
	cfg.retransmit_delay = 600;
	cfg.retransmit_count = 2;
	cfg.selective_auto_ack = true;
	cfg.event_handler = event;
	const uint8_t base[4] = {0xb7, 0x46, 0x32, 0xda};
	const uint8_t prefix = 0xc5;
	if ((rc = esb_init(&cfg)) || (rc = esb_set_base_address_0(base)) ||
	    (rc = esb_set_prefixes(&prefix, 1)) || (rc = esb_set_rf_channel(40))) return rc;
	return prx ? esb_start_rx() : 0;
}

bool air_send(const struct gp_frame *frame)
{
	struct esb_payload p = {.pipe = 0, .length = GP_WIRE_SIZE};
	if (!gp_encode(p.data, p.length, frame)) return false;
	if (!prx && !atomic_cas(&busy, 0, 1)) return false;
	if (esb_write_payload(&p) == 0) return true;
	if (!prx) atomic_clear(&busy);
	return false;
}

bool air_receive(struct gp_frame *frame)
{
	struct esb_payload p;
	while (esb_read_rx_payload(&p) == 0) {
		if (p.pipe == 0 && gp_decode(frame, p.data, p.length)) return true;
	}
	return false;
}
