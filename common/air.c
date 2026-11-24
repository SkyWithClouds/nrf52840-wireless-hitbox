/* SPDX-License-Identifier: MIT */
#include "air.h"
#include "match.h"
static uint8_t local_pipe, pipe_mask;
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
	local_pipe = receiver ? 1 : match_saved.slots;
    pipe_mask = match_mask(match_saved, receiver);
    cfg.retransmit_delay = match_delay(local_pipe);
	cfg.retransmit_count = 2;
	cfg.selective_auto_ack = true;
	cfg.event_handler = event;
	if ((rc = esb_init(&cfg)) || (rc = esb_set_base_address_1(match_saved.base)) ||
        (rc = esb_set_prefixes(match_prefix,5)) || (rc = esb_enable_pipes(pipe_mask)) ||
        (rc = esb_set_rf_channel(match_saved.channel))) return rc;
    return prx ? esb_start_rx() : 0;
}

bool air_send_pipe(uint8_t pipe, const struct gp_frame *frame)
{
	struct esb_payload p = {.pipe = pipe, .length = GP_WIRE_SIZE};
	if (!gp_encode(p.data, p.length, frame)) return false;
	if (!prx && !atomic_cas(&busy, 0, 1)) return false;
	if (esb_write_payload(&p) == 0) return true;
	if (!prx) atomic_clear(&busy);
	return false;
}

bool air_receive_pipe(uint8_t *pipe, struct gp_frame *frame)
{
	struct esb_payload p;
	while (esb_read_rx_payload(&p) == 0) {
		if (p.pipe<5 && (pipe_mask & (1u << p.pipe)) && gp_decode(frame,p.data,p.length)) {
            *pipe=p.pipe;
            return true;
        }
	}
	return false;
}

bool air_send(const struct gp_frame *f) { return air_send_pipe(local_pipe,f); }
bool air_receive(struct gp_frame *f) { uint8_t pipe; return air_receive_pipe(&pipe,f); }
