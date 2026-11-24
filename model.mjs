export const pins = ['P0.06','P0.08','P0.17','P0.20','P0.22','P0.24','P1.00','P0.11','P1.04','P1.06','P0.09','P0.10','P1.11','P1.13','P1.15','P0.02','P0.29','P0.31'];
export const actions = ['↑','↓','←','→','Start','Back','LS 点击','RS 点击','LB','RB','Guide','AUX','A','B','X','Y','LT','RT','禁用','下一配置'];
const layout = [2,1,3,0,14,15,9,8,12,13,17,16,5,4,6,7,10,11];
export function defaults() {
  return {schema:1,active:0,divider:0,profiles:Array.from({length:4},()=>({socd:0,output:0,debounce:2,map:[...layout]}))};
}
const inRange = (x,min,max) => Number.isInteger(x) && x>=min && x<=max;
export function validate(c) {
  return c?.schema===1 && inRange(c.active,0,3) && (c.divider===0 || inRange(c.divider,15,50)) &&
    Array.isArray(c.profiles) && c.profiles.length===4 && c.profiles.every(p =>
      p && inRange(p.socd,0,3) && inRange(p.output,0,2) && inRange(p.debounce,1,10) &&
      Array.isArray(p.map) && p.map.length===18 && p.map.every(a=>inRange(a,0,19)));
}
function featureBytes(view) {
  let data=new Uint8Array(view.buffer,view.byteOffset,view.byteLength);
  if (data.length===65 && data[0]===0) data=data.subarray(1);
  if (data.length!==64) throw new Error('设备返回的报告长度异常');
  return data;
}
export class Link {
  constructor(device) { this.device=device; this.token=crypto.getRandomValues(new Uint16Array(1))[0]; }
  async command(op,slot=0,profile=null,active=0,divider=0,network=null) {
    const q=new Uint8Array(64), token=++this.token & 65535;
    q.set([72,88,1,op,token & 255,token >> 8,0,slot,active,divider]);
    if (network) q.set(network,8);
    if (profile) q.set([profile.socd,profile.output,profile.debounce,...profile.map],10);
    await this.device.sendFeatureReport(0,q);
    const until=performance.now()+2500;
    do {
      const r=featureBytes(await this.device.receiveFeatureReport(0));
      if (q.subarray(0,6).every((v,i)=>v===r[i])) {
        if (r[6]) throw new Error(r[6]===3?'写入 Flash 失败，原设置仍有效':'固件拒绝了配置请求');
        return r;
      }
      await new Promise(ok=>setTimeout(ok,12));
    } while (performance.now()<until);
    throw new Error('设备应答超时，请重新连接并读取配置');
  }
  async read() {
    const c=defaults();
    for (let i=0;i<4;i++) {
      const r=await this.command(1,i);
      c.active=r[8]; c.divider=r[9];
      c.profiles[i]={socd:r[10],output:r[11],debounce:r[12],map:Array.from(r.slice(13,31))};
    }
    if (!validate(c)) throw new Error('设备配置格式异常');
    return c;
  }
  async save(c) {
    if (!validate(c)) throw new Error('配置内容无效');
    await this.command(2);
    try {
      for (let i=0;i<4;i++) await this.command(3,i,c.profiles[i]);
      await this.command(4,0,null,c.active,c.divider);
    } catch(err) { try { await this.command(5); } catch {} throw err; }
    const saved=await this.read();
    if (JSON.stringify(c.profiles.map(p=>[p.socd,p.output,p.debounce,...p.map]))!==
        JSON.stringify(saved.profiles.map(p=>[p.socd,p.output,p.debounce,...p.map])) ||
        c.active!==saved.active || c.divider!==saved.divider) throw new Error('写后回读不一致');
    return saved;
  }
}
export function liveReport(view) {
  if (view.byteLength!==64 || view.getUint8(0)!==72 || view.getUint8(1)!==76 || view.getUint8(2)!==1) return null;
  return {profile:view.getUint8(3),pressed:view.getUint32(8,true),mv:view.getUint16(32,true),
    source:view.getUint8(34),pinMv:view.getUint16(35,true)};
}

export function networkBytes(id,channel,slots) {
  if (!/^[a-f0-9]{8}$/i.test(id) || !inRange(channel,2,80) || !inRange(slots,1,4))
    throw new Error('请输入 8 位十六进制 ID、2–80 信道和 1–4 槽位。');
  return Uint8Array.from([...id.match(/../g).map(v=>parseInt(v,16)),channel,slots]);
}
export function slotStats(view) {
  if (view.byteLength!==64 || view.getUint8(0)!==82 || view.getUint8(1)!==83 ||
      view.getUint8(2)!==1 || !inRange(view.getUint8(3),1,4)) return null;
  return Array.from({length:view.getUint8(3)},(_,i)=>{
    const p=4+15*i;
    return [i+1,view.getUint8(p)?'在线':'离线',view.getUint32(p+1,true),view.getUint32(p+5,true),
      view.getUint32(p+9,true),view.getUint16(p+13,true)];
  });
}
