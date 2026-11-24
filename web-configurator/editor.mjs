import {maxSteps,estimatedCurrent} from './effects.mjs';
import {actions} from './model.mjs';
const by=id=>document.getElementById(id);
function maskChoices(parent,bits,change) {
  parent.replaceChildren();
  actions.slice(0,18).forEach((name,i)=>{
    const label=document.createElement('label'), check=document.createElement('input');
    check.type='checkbox'; check.checked=!!(bits & (1<<i));
    check.onchange=()=>{ bits=check.checked ? bits|(1<<i) : bits&~(1<<i); change(bits); };
    label.append(check,name); parent.append(label);
  });
}
function numberField(parent,name,value,min,max,change) {
  const label=document.createElement('label'), input=document.createElement('input');
  Object.assign(input,{type:'number',min,max,value,step:1});
  input.onchange=()=>{
    if (input.value==='' || !input.validity.valid) { input.reportValidity(); input.value=value; return; }
    value=input.valueAsNumber; change(value);
  };
  label.append(name,input); parent.append(label);
}
export function effectsEditor(getEffects,mark) {
  let selected=0;
  for (let i=0;i<6;i++) by('macro-index').add(new Option('宏 '+(i+1),i));
  const macro=()=>getEffects().macros[selected];
  function renderSteps() {
    const m=macro(), list=by('steps'); list.replaceChildren();
    by('macro-mode').selectedIndex=m.mode;
    by('add-step').disabled=m.steps.length>=maxSteps;
    m.steps.forEach((step,i)=>{
      const row=document.createElement('div'), bar=document.createElement('div'), keys=document.createElement('div');
      row.className='step'; bar.className='stepbar'; keys.className='checks';
      const title=document.createElement('strong'); title.textContent='第 '+(i+1)+' 步'; bar.append(title);
      numberField(bar,'持续 ms',step.hold,1,65535,v=>{step.hold=v;mark();});
      numberField(bar,'间隔 ms',step.gap,0,65535,v=>{step.gap=v;mark();});
      const remove=document.createElement('button'); remove.textContent='移除'; remove.type='button';
      remove.onclick=()=>{m.steps.splice(i,1);renderSteps();mark();}; bar.append(remove);
      maskChoices(keys,step.keys,v=>{step.keys=v;mark();}); row.append(bar,keys); list.append(row);
    });
  }
  by('macro-index').onchange=e=>{selected=+e.target.value;renderSteps();};
  by('macro-mode').onchange=e=>{macro().mode=e.target.selectedIndex; mark();};
  by('add-step').onclick=()=>{if (macro().steps.length<maxSteps) macro().steps.push({keys:0,hold:16,gap:0});renderSteps();mark();};
  function showOutput(e) {
    by('oled-controls').hidden=![1,2].includes(e.accessory);
    by('binary-controls').hidden=e.accessory!==3;
    by('rgb-controls').hidden=!e.rgbMode;
    by('color').disabled=e.rgbMode===4;
    by('bright-value').textContent=e.brightness+' / 255';
    by('current').textContent='全组点亮时估算上限约 '+estimatedCurrent(e)+' mA（含软限流）。';
  }
  for (const id of ['accessory','rgbMode','hz','count','brightness','idle','address']) {
    by(id).addEventListener(id==='brightness'?'input':'change',event=>{
      const el=event.target, e=getEffects();
      if (el.value==='' || !el.validity.valid) { el.reportValidity(); el.value=e[id]; return; }
      e[id]=['accessory','rgbMode'].includes(id) ? el.selectedIndex : Number(el.value);showOutput(e);mark();
    });
  }
  by('color').oninput=event=>{
    const e=getEffects();
    e.color=event.target.value.slice(1).match(/../g).map(v=>parseInt(v,16));showOutput(e);mark();
  };
  for (let i=0;i<5;i++) {
    const label=document.createElement('label'), select=document.createElement('select');
    for (let n=0;n<4;n++) select.add(new Option(n.toString(2).padStart(2,'0'),n));
    label.append(i?i+'P':'离线 / 灭灯',select);by('led-map').append(label);
    select.onchange=()=>{getEffects().ledMap[i]=+select.value;mark();};
  }
  return function render(e) {
    for (const id of ['accessory','rgbMode','hz','count','brightness','idle','address']) {
      if (['accessory','rgbMode'].includes(id)) by(id).selectedIndex=e[id];
      else by(id).value=e[id];
    }
    by('color').value='#'+e.color.map(v=>v.toString(16).padStart(2,'0')).join('');
    by('led-map').querySelectorAll('select').forEach((el,i)=>{el.value=e.ledMap[i];});
    maskChoices(by('allow'),e.allow,v=>{e.allow=v;mark();});
    showOutput(e);renderSteps();
  };
}
