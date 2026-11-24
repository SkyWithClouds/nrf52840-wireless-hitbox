import assert from 'node:assert/strict';
import {defaults,validate,Link,liveReport} from '../web-configurator/model.mjs';
const initial=defaults();assert(validate(initial));
const invalid=structuredClone(initial);invalid.profiles[2].map[16]=99;assert(!validate(invalid));
const {encodeRecord,decodeRecord,estimatedCurrent}=await import('../web-configurator/effects.mjs');
let extended=true;
let stored=defaults(),draft=null,reply;
const fake={
  async sendFeatureReport(id,q) {
    assert.equal(id,0);assert.equal(q.length,64);reply=new Uint8Array(q);
    const slot=q[7];
    if(q[3]===1) {reply[31]=extended?12:0;const p=stored.profiles[slot];reply.set([p.socd,p.output,p.debounce,...p.map],10);}
    if(q[3]===2) draft=structuredClone(stored);
    if(q[3]===3) draft.profiles[slot]={socd:q[10],output:q[11],debounce:q[12],map:[...q.slice(13,31)]};
    if(q[3]===6) reply.set(encodeRecord(stored.effects,slot),10);
    if(q[3]===7) decodeRecord(draft.effects,slot,q.subarray(10));
    if(q[3]===4) {draft.active=q[8];draft.divider=q[9];stored=draft;}
    reply[8]=stored.active;reply[9]=stored.divider;
  },
  async receiveFeatureReport(){return new DataView(reply.buffer);}
};
const link=new Link(fake);initial.active=3;initial.profiles[1].map[0]=19;
initial.effects.macros[5]={mode:2,steps:[{keys:0x30001,hold:65535,gap:512}]};
initial.effects.accessory=3;initial.effects.ledMap=[3,2,1,0,1];
initial.effects.allow=0x3ffff;initial.profiles[2].map[0]=26;
assert.deepEqual(await link.save(initial),initial);
const bad=structuredClone(initial);
bad.effects.macros[5].steps[0].hold=0;assert(!validate(bad));
assert.equal(estimatedCurrent({...initial.effects,count:18,rgbMode:1,color:[255,255,255],brightness:64}),100);
stored=defaults();extended=false;
const legacy=await link.read();assert.equal(legacy.schema,1);assert(!legacy.effects);
assert.deepEqual(await link.save(legacy),legacy);
await assert.rejects(link.save(initial));extended=true;
const live=new Uint8Array(64);live.set([72,76,1,2]);live[8]=8;live[34]=3;
assert.equal(liveReport(new DataView(live.buffer)).pressed,8);
console.log('WebHID schema, transactions, round-trip and live report passed');
const {networkBytes,slotStats}=await import('../web-configurator/model.mjs');
const factory=networkBytes('b74632da',40,2);
assert.deepEqual([...factory],[183,70,50,218,40,2]);
for(const args of [['0000000G',40,1],['00000000',81,2],['12345678',1,1],['11111111',40,5]])
  assert.throws(()=>networkBytes(...args));
await link.command(33,0,null,0,0,factory);
assert.deepEqual([...reply.slice(10,14)],[50,218,40,2]);
const status=new DataView(new ArrayBuffer(64));
[82,83,1,4].forEach((b,i)=>status.setUint8(i,b));
status.setUint8(19,1);status.setUint32(24,314,true);
assert.deepEqual(slotStats(status)[1],[2,'在线',0,314,0,0]);
assert.equal(slotStats(status)[0][1],'离线');
