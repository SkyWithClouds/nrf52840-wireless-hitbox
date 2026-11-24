// WebHID commands 6/7 carry one settings record or one seven-step macro at offset 10.
export const maxSteps=7, maxBrightness=64, rgbBudget=100;
export function defaultEffects() {
  return {accessory:0,idle:30,ledMap:[0,1,2,3,0],hz:12,allow:0,
    rgbMode:0,count:1,brightness:16,color:[0,200,100],address:60,
    macros:Array.from({length:6},()=>({mode:0,steps:[]}))};
}
const integer=(v,min,max)=>Number.isInteger(v)&&v>=min&&v<=max;
const array=(v,length,test)=>Array.isArray(v)&&v.length===length&&Array.from(v).every(test);
export function validEffects(e) {
  return e && integer(e.accessory,0,3) && [30,60].includes(e.idle) && array(e.ledMap,5,v=>integer(v,0,3)) &&
    integer(e.hz,5,30) && integer(e.allow,0,0x3ffff) && integer(e.rgbMode,0,4) &&
    integer(e.count,1,18) && integer(e.brightness,0,maxBrightness) && array(e.color,3,v=>integer(v,0,255)) &&
    [60,61].includes(e.address) && array(e.macros,6,m=>m && integer(m.mode,0,2) && Array.isArray(m.steps) &&
      m.steps.length<=maxSteps && Array.from(m.steps).every(s=>s && integer(s.keys,0,0x3ffff) &&
        integer(s.hold,1,65535) && integer(s.gap,0,65535)));
}
const le=(v,n)=>Array.from({length:n},(_,i)=>(v >>> (8*i)) & 255);
const u24=(r,i)=>r[i]+256*r[i+1]+65536*r[i+2];
export function encodeRecord(e,index) {
  if (!index) return [e.accessory,e.idle,...e.ledMap,e.hz,...le(e.allow,3),e.rgbMode,
    e.count,e.brightness,...e.color,e.address];
  const m=e.macros[index-1];
  return [m.mode,m.steps.length,...m.steps.flatMap(s=>[...le(s.keys,3),...le(s.hold,2),...le(s.gap,2)])];
}
export function decodeRecord(e,index,r) {
  if (!index) {
    Object.assign(e,{accessory:r[0],idle:r[1],ledMap:[...r.slice(2,7)],hz:r[7],allow:u24(r,8),
      rgbMode:r[11],count:r[12],brightness:r[13],color:[...r.slice(14,17)],address:r[17]});
  } else {
    if (r[1]>maxSteps) throw new Error('设备宏步数超出范围');
    e.macros[index-1]={mode:r[0],steps:Array.from({length:r[1]},(_,j)=>{
      const i=2+j*7;
      return {keys:u24(r,i),hold:r[i+3]+256*r[i+4],gap:r[i+5]+256*r[i+6]};
    })};
  }
}
export function estimatedCurrent(e) {
  if (!e.rgbMode || !e.brightness) return 0;
  const color=e.rgbMode===4 ? [255,0,0] : e.color;
  const sum=color.reduce((n,c)=>n+Math.floor(c*e.brightness/255),0);
  return Math.min(rgbBudget,Math.ceil(e.count*(1+20*sum/255)));
}
