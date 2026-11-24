import {pins,actions,defaults,validate,Link,liveReport,networkBytes,slotStats} from './model.mjs';
const $=id=>document.getElementById(id);
let config=defaults(), tab=0, key=0, device=null, link=null, busy=false;
const xy=[[11,56],[21,52],[31,56],[37,80],[52,45],[64,38],[76,40],[88,47],
          [52,66],[64,59],[76,61],[88,68],[22,19],[33,19],[44,19],[55,19],[66,19],[77,19]];
const setStatus=text=>{$('status').textContent=text;};
let role=0;
const roleNames=['未连接','单板 Hitbox','PC 迷你接收器','主机盒无线桥','外挂发射端'];
const hex=bytes=>Array.from(bytes,b=>b.toString(16).padStart(2,'0')).join('').toUpperCase();
function elements() {
  for(let i=2;i<=80;i++) $('channel').add(new Option(i+' · '+(2400+i)+' MHz',i));
  $('channel').value=40;
  for(let i=1;i<=4;i++) $('slots').add(new Option(String(i),i));
  for (const [i,name] of actions.entries()) $('mapping').add(new Option(name,i));
  for (const [i,[x,y]] of xy.entries()) {
    const b=document.createElement('button');
    b.className='key'+(i>=12?' small':''); b.style.left=x+'%'; b.style.top=y+'%';
    b.onclick=()=>{key=i;render();}; $('pad').append(b);
  }
  for(let i=0;i<4;i++) {
    const b=document.createElement('button'); b.textContent='0'+(i+1);
    b.onclick=()=>{tab=i;render();}; $('profiles').append(b);
  }
}
function render() {
  const p=config.profiles[tab];
  const editing=!device || role===1 || role===-1;
  document.querySelector('.workspace').hidden=!editing;
  document.querySelector('.workspace').style.display=editing?'':'none';
  $('role').textContent=roleNames[role]??'旧版 Hitbox';
  $('rf-fields').disabled=busy || !(role>0);
  $('stats').hidden=role!==2;
  $('slots').disabled=role===3;
  $('slot-label').textContent=role===2?'启用槽位数':'发射槽位';
  const locks=busy;
  Array.from($('pad').children).forEach((b,i)=>{
    b.textContent=actions[p.map[i]]; b.title=pins[i]+' · '+b.textContent;
    b.setAttribute('aria-label',b.title); b.setAttribute('aria-pressed',i===key); b.disabled=locks;
  });
  Array.from($('profiles').children).forEach((b,i)=>{ b.setAttribute('aria-pressed',i===tab); b.disabled=locks; });
  $('keyname').textContent='按键 '+String(key+1).padStart(2,'0')+' · '+pins[key]; $('mapping').value=p.map[key];
  $('socd').selectedIndex=p.socd; $('output').selectedIndex=p.output;
  $('debounce').value=p.debounce; $('debval').textContent=p.debounce+' ms';
  $('active').selectedIndex=config.active; $('divider').value=config.divider/10;
  $('connect').textContent=device?'断开设备':'连接设备'; $('connect').disabled=busy;
  $('save').disabled=!link || busy;
  for(const id of ['mapping','socd','output','debounce','active','divider','import','export']) $(id).disabled=busy;
}
function resetDevice() {
  device?.removeEventListener('inputreport',onLive);
  device=null; link=null; role=0;
  $('stats').tBodies[0].replaceChildren();
  for(const b of $('pad').children) b.classList.remove('held');
  for(const id of ['source','battery','liveprofile']) $(id).textContent='—';
  $('teststate').textContent='实时测试未连接'; render();
}
function onLive(event) {
  if(event.device!==device || event.reportId!==0) return;
  const rows=slotStats(event.data);
  if(role===2 && rows) {
    const body=$('stats').tBodies[0]; body.replaceChildren();
    for(const row of rows) {const tr=body.insertRow();for(const cell of row) tr.insertCell().textContent=cell;}
    return;
  }
  const s=liveReport(event.data); if(!s) return;
  Array.from($('pad').children).forEach((b,i)=>b.classList.toggle('held',!!(s.pressed&(1<<i))));
  $('teststate').textContent='实时测试已连接'; $('liveprofile').textContent='0'+(s.profile+1);
  const pct=Math.round(Math.min(1,Math.max(0,(s.mv-3300)/900))*100);
  $('battery').textContent=s.source ? pct+'% · '+(s.mv/1000).toFixed(2)+' V':'待测';
  $('source').textContent=['无有效读数','P0.04 分压','电池供电','上次拔线读数'][s.source]??'未知';
  $('source').title='P0.04 ADC：'+s.pinMv+' mV';
}
async function work(task) {
  if(busy) return;
  busy=true; render();
  try {await task();} catch(e) {setStatus(e.name==='NotFoundError'?'未选择设备。':e.message);}
  finally {busy=false;render();}
}
$('connect').onclick=()=>work(async()=>{
  if(device) {const old=device; resetDevice(); await old.close(); setStatus('已断开，当前配置可离线编辑。'); return;}
  if(!navigator.hid) throw new Error('请使用桌面 Chrome 或 Edge，通过 HTTPS 打开配置台。');
  const [chosen]=await navigator.hid.requestDevice({filters:[3,4,5,6,7,8,9].map(productId=>({vendorId:0x1209,productId,usagePage:0xff60,usage:1}))});
  if(!chosen) return;
  if(!chosen.opened) await chosen.open();
  device=chosen; link=new Link(chosen); chosen.addEventListener('inputreport',onLive);
  try {
    try {
      const r=await link.command(32);
      role=r[7]; if(!roleNames[role] || !role) throw new Error('设备角色无效');
      $('netid').value=hex(r.slice(8,12)); $('channel').value=r[12]; $('slots').value=r[13];
    } catch(e) { if(chosen.productId!==3) throw e; role=-1; }
    if(role===1 || role===-1) {config=await link.read(); tab=config.active;}
    setStatus(role===-1?'已读取旧版配置档；无线匹配需要升级固件。':'已读取设备配置。');
  } catch(e) {resetDevice(); await chosen.close().catch(()=>{}); throw e;}
});
navigator.hid?.addEventListener('disconnect',event=>{
  if(event.device===device) { resetDevice(); setStatus('设备已拔出，编辑内容仍保留在本页。'); }
});
$('save').onclick=()=>work(async()=>{
  config=await link.save(config); setStatus('已写入设备，并回读核对 4 个配置档。');
});
const dirty=()=>{render();setStatus('当前编辑尚未保存到设备。');};
$('mapping').onchange=e=>{config.profiles[tab].map[key]=Number(e.target.value);dirty();};
for (const name of ['socd','output']) $(name).onchange=e=>{config.profiles[tab][name]=e.target.selectedIndex;dirty();};
$('debounce').oninput=e=>{config.profiles[tab].debounce=Number(e.target.value);dirty();};
$('active').onchange=e=>{config.active=e.target.selectedIndex;dirty();};
$('divider').onchange=e=>{
  const n=Math.round(10*Number(e.target.value));
  if(n!==0 && (n<15 || n>50 || !Number.isInteger(n))) {render();setStatus('倍率可选 0 或 1.5–5.0。');return;}
  config.divider=n;dirty();
};
$('export').onclick=()=>{
  const url=URL.createObjectURL(new Blob([JSON.stringify(config,null,2)+'\n'],{type:'application/json'}));
  const a=document.createElement('a');a.href=url;a.download='hitbox-profiles.json';a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);
};
$('import').onclick=()=>$('file').click();
$('file').onchange=()=>work(async()=>{
  const f=$('file').files[0];$('file').value='';if(!f) return;
  if(f.size>32768) throw new Error('JSON 文件过大');
  const candidate=JSON.parse(await f.text());
  if(!validate(candidate)) throw new Error('JSON 格式或按键参数无效，当前配置已保留');
  config=candidate;tab=config.active;setStatus('已导入 4 个配置档，保存到设备后生效。');
});
$('random').onclick=()=>{$('netid').value=hex(crypto.getRandomValues(new Uint8Array(4)));};
$('rf-save').onclick=()=>work(async()=>{
  const bytes=networkBytes($('netid').value,Number($('channel').value),Number($('slots').value));
  await link.command(33,0,null,0,0,bytes);
  const r=await link.command(32);
  if(!bytes.every((b,i)=>b===r[8+i])) throw new Error('无线设置回读不一致');
  setStatus(role===1?'已保存并回读；拔线进入无线后生效。':'已保存并回读；请将此板完全断电再上电后生效。');
});
elements();render();
