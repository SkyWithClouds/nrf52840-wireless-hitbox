#include "survey.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    uint8_t rf[79],out[64]={0};memset(rf,90,sizeof(rf));
    assert(quiet_channel(rf,40)==40);
    for (unsigned i=24;i<55;i++) rf[i]=36;
    unsigned ch=quiet_channel(rf,40);assert(ch<20 || ch>60);
    for (unsigned i=0;i<79;i++) rf[i]=i;
    assert(survey_page(out,0,rf,40) && !memcmp(out+11,rf,40) && out[9]==2);
    assert(survey_page(out,1,rf,40) && !memcmp(out+11,rf+40,39) && out[9]==42);
    assert(!survey_page(out,2,rf,40));
    struct pairing rx={.receiver=true,.msg={.magic={'H','B','3'},.rx={99},.channel=70,.base={5,6,7,8}}};
    struct pairing tx={.msg={.magic={'H','B','3'},.tx={19},.op=FIND}};
    assert(!pair_accept(&rx,&tx.msg,55) && !pair_accept(&rx,&tx.msg,0) && !rx.msg.op);
    assert(pair_accept(&rx,&tx.msg,54) && rx.msg.op==OFFER);
    assert(pair_accept(&rx,&tx.msg,42)); /* lost offer */
    struct pair_packet stray=rx.msg;stray.tx[0]++;
    assert(!pair_accept(&tx,&stray,22));
    assert(pair_accept(&tx,&rx.msg,43) && tx.msg.op==CONFIRM);
    stray=tx.msg;stray.rx[0]++;assert(!pair_accept(&rx,&stray,25));
    assert(pair_accept(&rx,&tx.msg,32) && rx.msg.op==READY);
    assert(pair_accept(&tx,&rx.msg,40));tx.msg.op=SAVED;
    assert(pair_accept(&rx,&tx.msg,40));rx.msg.op=DONE;
    assert(pair_accept(&tx,&rx.msg,40) && tx.msg.op==FINAL);
    assert(pair_accept(&rx,&tx.msg,40) && rx.msg.op==RECEIPT);
    assert(pair_accept(&tx,&rx.msg,40) && tx.finished);
    rx.start=UINT32_MAX-20;assert(!pair_expired(&rx,rx.start+14999));
    assert(pair_expired(&rx,rx.start+15000));
    puts("79-channel survey, interference scoring, nonce binding, bilateral RSSI and pairing timeout passed");
}
