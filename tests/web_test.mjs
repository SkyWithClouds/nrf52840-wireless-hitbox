import assert from 'node:assert/strict';
import {defaults,validate,Link,liveReport} from '../web-configurator/model.mjs';
const initial=defaults();assert(validate(initial));
const invalid=structuredClone(initial);invalid.profiles[2].map[16]=99;assert(!validate(invalid));
let stored=defaults(),draft=null,reply;
const fake={
  async sendFeatureReport(id,q) {
    assert.equal(id,0);assert.equal(q.length,64);reply=new Uint8Array(q);
    const slot=q[7];
    if(q[3]===1) {const p=stored.profiles[slot];reply.set([p.socd,p.output,p.debounce,...p.map],10);}
    if(q[3]===2) draft=structuredClone(stored);
    if(q[3]===3) draft.profiles[slot]={socd:q[10],output:q[11],debounce:q[12],map:[...q.slice(13,31)]};
    if(q[3]===4) {draft.active=q[8];draft.divider=q[9];stored=draft;}
    reply[8]=stored.active;reply[9]=stored.divider;
  },
  async receiveFeatureReport(){return new DataView(reply.buffer);}
};
const link=new Link(fake);initial.active=3;initial.profiles[1].map[0]=19;
assert.deepEqual(await link.save(initial),initial);
const live=new Uint8Array(64);live.set([72,76,1,2]);live[8]=8;live[34]=3;
assert.equal(liveReport(new DataView(live.buffer)).pressed,8);
console.log('WebHID schema, transactions, round-trip and live report passed');
