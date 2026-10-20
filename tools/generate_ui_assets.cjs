// Convert the upstream SVG navigation icons into LVGL images; no page screenshots are used.
const fs=require('fs');
const path=require('path');
const {Resvg}=require('../.tools/fontgen/node_modules/@resvg/resvg-js');
const root=path.resolve(__dirname,'..');
const source=fs.readFileSync(path.join(root,'upstream/bambuddy/frontend/src/components/spoolbuddy/SpoolBuddyBottomNav.tsx'),'utf8');
const svgs=[...source.matchAll(/<svg[\s\S]*?<\/svg>/g)].map(x=>x[0]);
let out='// SPDX-License-Identifier: AGPL-3.0-or-later\n// Upstream SVG geometry, rasterized as individual icon assets.\n#include "lvgl.h"\n';
function add(name,svg,size){
 const s=svg.replace(/className="[^"]*"/g,'').replace(/strokeLinecap/g,'stroke-linecap').replace(/strokeLinejoin/g,'stroke-linejoin').replace(/strokeWidth/g,'stroke-width').replace(/=\{(\d+)\}/g,'="$1"').replace('<svg ',`<svg xmlns="http://www.w3.org/2000/svg" width="${size}" height="${size}" `);
 const png=new Resvg(s).render();const pixels=png.pixels;let bytes=[];
 for(let i=0;i<pixels.length;i+=4)bytes.push(pixels[i+2],pixels[i+1],pixels[i],pixels[i+3]);
 out+=`static const unsigned char ${name}_pixels[]={${bytes.join(',')}};\nconst lv_image_dsc_t ${name}={.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_ARGB8888,.w=${size},.h=${size},.stride=${size*4}},.data_size=sizeof(${name}_pixels),.data=${name}_pixels};\n`;
}
for(let i=0;i<5;i++)for(const [suffix,color] of [['gray','#979da4'],['green','#00ae42']])add(`sb_nav_${i}_${suffix}`,svgs[i].replaceAll('currentColor',color),24);
const shapes={
 offline:'<circle cx="12" cy="12" r="9"/><path d="m6 6 12 12"/>',
 wifi:'<path d="M8.3 15a5.25 5.25 0 0 1 7.4 0M5.1 11.9a9.75 9.75 0 0 1 13.8 0M1.9 8.7a14.25 14.25 0 0 1 20.2 0"/><circle cx="12" cy="19" r=".5"/>',
 search:'<circle cx="10.5" cy="10.5" r="6.5"/><path d="m16 16 4 4"/>',
 scale:'<path d="M12 3v17M5 21h14M4 7h16M6 7 2 15h8L6 7Zm12 0-4 8h8l-4-8Z"/>',
 tag:'<path d="M20 13 12 21 3 12V3h9l8 9Z"/><circle cx="7.5" cy="7.5" r="1"/>',
 settings:'<circle cx="12" cy="12" r="4"/><path d="M12 1v3m0 16v3M1 12h3m16 0h3M4 4l2 2m12 12 2 2M4 20l2-2M18 6l2-2"/>',
 info:'<circle cx="12" cy="12" r="9"/><path d="M12 11v6m0-10v1"/>',
 chevron:'<path d="m6 9 6 6 6-6"/>',
};
for(const [name,shape]of Object.entries(shapes))add('sb_icon_'+name,`<svg viewBox="0 0 24 24" fill="none" stroke="${name==='wifioff'?'#d46073':'#71717a'}" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round">${shape}</svg>`,name==='offline'||name==='wifi'?40:16);
// TopBar uses CSS bars, not the 40px offline illustration used inside pages.
// Mirror its original w-1 / gap-0.5 / level*4px / rounded-sm / bg-white geometry.
const topbar=fs.readFileSync(path.join(root,'upstream/bambuddy/frontend/src/components/spoolbuddy/SpoolBuddyTopBar.tsx'),'utf8');
if(!topbar.includes('[1, 2, 3, 4]') || !topbar.includes('level * 4') || !topbar.includes('w-1 rounded-sm'))throw Error('Review upstream signal bar geometry');
// add() accepts square sizes; preserve the original 22px row in a 22x20 canvas.
let bars='<svg xmlns="http://www.w3.org/2000/svg" width="22" height="20" viewBox="0 0 22 20">';
for(let i=0;i<4;i++)bars+=`<rect x="${i*6}" y="${18-(i+1)*4}" width="4" height="${(i+1)*4}" rx="2" fill="white"/>`;
bars+='</svg>';
const pixels=new Resvg(bars).render().pixels, bytes=[];
for(let i=0;i<pixels.length;i+=4)bytes.push(pixels[i+2],pixels[i+1],pixels[i],pixels[i+3]);
out+=`static const unsigned char sb_icon_signal_pixels[]={${bytes.join(',')}};\nconst lv_image_dsc_t sb_icon_signal={.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_ARGB8888,.w=22,.h=20,.stride=88},.data_size=sizeof(sb_icon_signal_pixels),.data=sb_icon_signal_pixels};\n`;
const wifiOff=fs.readFileSync(path.join(root,'assets/upstream/lucide/wifi-off.svg'),'utf8').replace(/width="24"/, 'width="20"').replace(/height="24"/,'height="20"').replace('currentColor','#f87171');
// Already has an XML namespace and dimensions; feed the unmodified SVG geometry directly.
const offPixels=new Resvg(wifiOff).render().pixels, offBytes=[];
for(let i=0;i<offPixels.length;i+=4)offBytes.push(offPixels[i+2],offPixels[i+1],offPixels[i],offPixels[i+3]);
out+=`static const unsigned char sb_icon_wifioff_pixels[]={${offBytes.join(',')}};\nconst lv_image_dsc_t sb_icon_wifioff={.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_ARGB8888,.w=20,.h=20,.stride=80},.data_size=sizeof(sb_icon_wifioff_pixels),.data=sb_icon_wifioff_pixels};\n`;
fs.writeFileSync(path.join(root,'assets/ui_icons.c'),out);
