/* SPDX-License-Identifier: MIT */
#include "air.h"
#include "match.h"
static uint8_t local_pipe, pipe_mask;
#include <esb.h>
#include <zephyr/kernel.h>
#include <zephyr/random/random.h>
#include <zephyr/drivers/clock_control/nrf_clock_control.h>

static bool prx,running,temporary,surveying,pair_requested,pairing_active;
static struct match active_net;
static uint32_t last_probe;

static atomic_t busy,boot_pending,boot_due;
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
	if (running) { temporary=false; return 0; }
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
    rc=prx ? esb_start_rx() : 0;
    if (!rc) {running=true;active_net=match_saved;}
    return rc;
}

bool air_send_pipe(uint8_t pipe, const struct gp_frame *frame)
{
	if (atomic_get(&boot_pending) || pair_requested || (surveying && !prx && k_uptime_get_32()!=last_probe)) return false;
	struct esb_payload p = {.pipe = pipe, .length = GP_WIRE_SIZE};
	if (!gp_encode(p.data, p.length, frame)) return false;
	if (!prx && !atomic_cas(&busy, 0, 1)) return false;
	if (esb_write_payload(&p) == 0) return true;
	if (!prx) atomic_clear(&busy);
	return false;
}

bool air_receive_pipe(uint8_t *pipe, struct gp_frame *frame)
{
	if (pairing_active) return false;
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

/* Built with air.c: the main loop exclusively owns maintenance and ESB. */
#include <zephyr/drivers/entropy.h>
#include <hal/nrf_gpio.h>
#include "survey.h"

static struct match previous;
static struct pairing pairing;
static uint8_t peaks[CHANNELS],sample_index,recommended,pair_status;
static uint32_t last_request,done_at;
static bool scan_valid;
static enum match_role role;
static bool wait_event(volatile uint32_t *reg)
{
    for (unsigned i=0;i<250;i++) { if (*reg) return true; k_busy_wait(1); }
    return false;
}
/* Borrow RADIO for one frequency, keeping the ESB driver, its FIFOs and all
 * application sessions alive. PTX must finish its current transaction first. */
static int measure(unsigned channel,uint8_t *result)
{
    if (atomic_get(&busy) || (!prx && !esb_is_idle())) return -EBUSY;
    if (prx && esb_stop_rx()) return -EBUSY;
    if (esb_suspend()) {
        if (prx && esb_start_rx()) return -EIO;
        return -EBUSY;
    }
    uint32_t ints=NRF_RADIO->INTENSET, shorts=NRF_RADIO->SHORTS;
    uint32_t freq=NRF_RADIO->FREQUENCY,addresses=NRF_RADIO->RXADDRESSES;
    NRF_RADIO->INTENCLR=UINT32_MAX;NRF_RADIO->SHORTS=RADIO_SHORTS_READY_START_Msk;
    NRF_RADIO->RXADDRESSES=0;NRF_RADIO->FREQUENCY=channel;NRF_RADIO->EVENTS_READY=0;
    NRF_RADIO->TASKS_RXEN=1;
    bool ok=wait_event(&NRF_RADIO->EVENTS_READY);
    uint8_t peak=127;
    for (unsigned i=0;i<4 && ok;i++) {
        NRF_RADIO->EVENTS_RSSIEND=0;NRF_RADIO->TASKS_RSSISTART=1;
        ok=wait_event(&NRF_RADIO->EVENTS_RSSIEND);
        peak=MIN(peak,NRF_RADIO->RSSISAMPLE);
        NRF_RADIO->TASKS_RSSISTOP=1;
    }
    NRF_RADIO->TASKS_RSSISTOP=1;NRF_RADIO->SHORTS=0;NRF_RADIO->EVENTS_DISABLED=0;
    NRF_RADIO->TASKS_DISABLE=1;ok=wait_event(&NRF_RADIO->EVENTS_DISABLED) && ok;
    NRF_RADIO->EVENTS_DISABLED=0;NRF_RADIO->EVENTS_READY=0;NRF_RADIO->EVENTS_RSSIEND=0;
    NRF_RADIO->FREQUENCY=freq;NRF_RADIO->RXADDRESSES=addresses;
    NRF_RADIO->SHORTS=shorts;NRF_RADIO->INTENSET=ints;
    if (prx && esb_start_rx()) return -EIO;
    *result=peak;return ok?0:-EIO;
}
static void shutdown_radio(void)
{
    if (!running) return;
    if (prx) esb_stop_rx();
    esb_disable(); running=false;
    onoff_release(z_nrf_clock_control_get_onoff(CLOCK_CONTROL_NRF_SUBSYS_HF));
}
void air_release(void)
{
    if (surveying || pairing_active || pair_requested) temporary=true;
    else shutdown_radio();
}
static int acquire(void)
{
    if (running) return 0;
    int rc=air_start(false); if (!rc) temporary=true;
    return rc;
}
static int start_scan(void)
{
    if (acquire()) return -EIO;
    surveying=true;scan_valid=false;sample_index=0;
    return 0;
}
int match_pair_start(void)
{
    if (pair_requested || pairing_active || surveying) return -EBUSY;
    if (acquire()) return -EIO;
    pair_status=1;pair_requested=true;
    if (prx) start_scan();
    return 0;
}
static int tune(const struct match *net,bool public)
{
    static const uint8_t rendezvous[4]={0x97,0xb4,0x5a,0x39};
    if (prx && esb_stop_rx()) return -EIO;
    esb_flush_tx();esb_flush_rx();
    int rc=esb_set_base_address_1(public?rendezvous:net->base);
    if (!rc) rc=esb_enable_pipes(public?2:match_mask(*net,prx));
    if (!rc) rc=esb_set_rf_channel(public?76:net->channel);
    if (!rc) rc=esb_set_retransmit_delay(match_delay(public || prx ? 1 : net->slots));
    if (!rc) rc=esb_set_tx_power(public?-20:0);
    if (!rc) NRF_RADIO->TXPOWER=public ? RADIO_TXPOWER_TXPOWER_Neg20dBm : RADIO_TXPOWER_TXPOWER_0dBm;
    if (!rc && prx) rc=esb_start_rx();
    if (!public) {local_pipe=prx?1:net->slots;pipe_mask=match_mask(*net,prx);}
    return rc;
}
static bool random_bytes(uint8_t *dst,size_t n)
{
    const struct device *rng=DEVICE_DT_GET(DT_CHOSEN(zephyr_entropy));
    return device_is_ready(rng) && !entropy_get_entropy(rng,dst,n);
}
static void end_pair(bool success)
{
    pair_status=finish_network(success,pairing.stored,previous,match_persist,match_checkpoint_clear);
    active_net=match_saved;
    if (tune(&active_net,false)) pair_status=4;
    pairing_active=pair_requested=false;
}
static void pairing_tick(uint32_t now)
{
    if (atomic_get(&busy)) return;
    if (!pairing_active) {
        previous=match_saved;
        pairing=(struct pairing){.start=now,.receiver=prx,.msg={.magic={'H','B','3'}}};
        if (!random_bytes(prx?pairing.msg.rx:pairing.msg.tx,8) ||
            (prx && !random_bytes(pairing.msg.base,4))) {pair_requested=false;pair_status=4;return;}
        pairing.msg.op=prx?0:FIND;pairing.msg.channel=prx?recommended:0;
        if (tune(&active_net,true)) {end_pair(false);return;}
        pairing_active=true;
    }
    if (pairing.finished || (prx && pairing.msg.op==RECEIPT && now-done_at>=1000)) {end_pair(true);return;}
    if (pair_expired(&pairing,now) && !(prx && pairing.msg.op==RECEIPT)) {end_pair(false);return;}
    struct esb_payload p;
    while (!esb_read_rx_payload(&p)) {
        struct pair_packet in;
        if (p.pipe!=1 || p.length!=sizeof(in)) continue;
        memcpy(&in,p.data,sizeof(in));
        uint8_t previous_op=pairing.msg.op;
        if (!pair_accept(&pairing,&in,p.rssi)) continue;
        if (previous_op==DONE && pairing.msg.op==RECEIPT) done_at=now;
        if (!pairing.stored && ((prx && in.op==SAVED && pairing.msg.op==READY) ||
            (!prx && in.op==READY && pairing.msg.op==CONFIRM))) {
            struct match next=previous;
            memcpy(next.base,in.base,4);next.channel=in.channel;
            if (!prx) next.slots=1;
            if (match_checkpoint(previous) || match_persist(next)) {end_pair(false);pair_status=4;return;}
            pairing.stored=true;pairing.msg.op=prx?DONE:SAVED;done_at=now;
        }
        if (prx && pairing.msg.op) {
            p=(struct esb_payload){.pipe=1,.length=sizeof(pairing.msg)};
            memcpy(p.data,&pairing.msg,p.length);
            esb_write_payload(&p);
        }
    }
    if (!prx && now-last_request>=5) {
        p=(struct esb_payload){.pipe=1,.length=sizeof(pairing.msg)};
        memcpy(p.data,&pairing.msg,p.length);
        atomic_set(&busy,1);
        if (esb_write_payload(&p)) atomic_clear(&busy);
        last_request=now;
    }
}
__weak void match_prepare_boot(void) {}
static void reboot_work(struct k_work *work)
{
    ARG_UNUSED(work);
    atomic_set(&boot_due,1);
}
K_WORK_DELAYABLE_DEFINE(reboot_job,reboot_work);
void match_boot_request(void)
{
    if (atomic_cas(&boot_pending,0,1)) k_work_schedule(&reboot_job,K_MSEC(100));
}
void match_service(void)
{
    uint32_t now=k_uptime_get_32();
    if (atomic_get(&boot_due) && !atomic_get(&busy)) {
        shutdown_radio();match_prepare_boot();
        NRF_POWER->GPREGRET=0x57;__DSB();NVIC_SystemReset();
    }
    if (role!=HITBOX) {
        static bool held,fired;
        static uint32_t start;
        if (nrf_gpio_pin_read(39)) held=fired=false;
        else if (!held) {held=true;start=now;}
        else if (!fired && now-start>=2000) {fired=true;match_pair_start();}
    }
    if (surveying && now!=last_probe) {
        int rc=measure(2+sample_index,&peaks[sample_index]);
        if (!rc) {
            last_probe=now;
            if (++sample_index==CHANNELS) {
                recommended=quiet_channel(peaks,active_net.channel);scan_valid=true;surveying=false;
            }
        } else if (rc!=-EBUSY) {surveying=pair_requested=false;pair_status=4;}
    } else if (!surveying && pair_requested) pairing_tick(now);
    if (!surveying && !pair_requested && temporary) {shutdown_radio();temporary=false;}
}
void match_role_init(enum match_role r)
{
    role=r;
    if (role!=HITBOX) nrf_gpio_cfg_input(39,NRF_GPIO_PIN_PULLUP);
}
bool maintenance_command(const uint8_t *q,uint8_t *r)
{
    switch (q[3]) {
    case MATCH_WRITE:
        if (!pair_requested && !surveying) return false;
        r[6]=2;break;
    case MATCH_SCAN:
        if (q[7]>1 || q[8]>1 || pair_requested) {r[6]=2;break;}
        if (q[8] && !q[7] && !surveying && start_scan()) {r[6]=3;break;}
        if (surveying) r[9]=0;
        else if (!scan_valid || !survey_page(r,q[7],peaks,recommended)) r[6]=2;
        break;
    case MATCH_PAIR:
        if (q[8]==1 && match_pair_start()) r[6]=2;
        else if (q[8]>1) r[6]=2;
        r[8]=pair_status;break;
    case MATCH_BOOTLOADER:
        if (memcmp(q+8,"UF2!5284",8)) r[6]=2;
        /* GET_REPORT schedules the reset after returning this accepted reply. */
        break;
    default:return false;
    }
    return true;
}
