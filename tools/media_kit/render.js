// usage: node render.js jobs.json  ; job = {page, q:{...}, w, h, out}
const {chromium}=require('playwright');const fs=require('fs'),path=require('path');
(async()=>{const jobs=JSON.parse(fs.readFileSync(process.argv[2]));const b=await chromium.launch();
const ctx=await b.newContext();
for(const j of jobs){const p=await ctx.newPage({});await p.setViewportSize({width:j.w,height:j.h});
const qs=new URLSearchParams({...j.q,w:j.w,h:j.h}).toString();
await p.goto('file://'+path.resolve(j.page)+'?'+qs);
await p.evaluate(async()=>{for(const f of ['400 20px Noto','700 20px Noto','800 20px Nunito','600 20px Nunito','700 20px Inter','500 20px Inter'])await document.fonts.load(f);await document.fonts.ready;if(window.build)window.build();else document.body.innerHTML=document.body.innerHTML});
if(j.fitW){const a=await p.evaluate(()=>window.ASPECT);await p.setViewportSize({width:j.fitW,height:Math.max(1,Math.round(j.fitW/a*1.0+j.fitW*0.06))});await p.evaluate(()=>window.build())}
fs.mkdirSync(path.dirname(j.out),{recursive:true});
await p.screenshot({path:j.out,omitBackground:!!j.transparent});await p.close();process.stdout.write('.')}
await b.close();console.log(' done',jobs.length)})();
