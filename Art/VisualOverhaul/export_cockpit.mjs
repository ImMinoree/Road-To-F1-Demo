import * as T from '../KartLab/vendor/three.module.js';
import {createKart} from '../KartLab/scene.mjs';
import {writeFileSync,mkdirSync} from 'node:fs';
import {fileURLToPath} from 'node:url';
const out=fileURLToPath(new URL('./exports/',import.meta.url));mkdirSync(out,{recursive:true});
function exportGroup(root,prefix){root.updateMatrixWorld(true);const batches=new Map();root.traverse(o=>{if(!o.isMesh)return;const m=o.material;if(!batches.has(m.name))batches.set(m.name,{material:m,meshes:[]});batches.get(m.name).meshes.push(o);});const result=[];
for(const [name,batch]of batches){let lines=['# RoadToF1 authored geometry. Centimetres, Z up.'],base=1;for(const o of batch.meshes){const g=o.geometry,p=g.attributes.position,uv=g.attributes.uv;for(let i=0;i<p.count;i++){const v=new T.Vector3().fromBufferAttribute(p,i).applyMatrix4(o.matrixWorld);lines.push(`v ${(v.x*100).toFixed(3)} ${(-v.z*100).toFixed(3)} ${(v.y*100).toFixed(3)}`);}for(let i=0;i<p.count;i++)lines.push(`vt ${uv?uv.getX(i).toFixed(5):0} ${uv?uv.getY(i).toFixed(5):0}`);const index=g.index,count=index?index.count:p.count;for(let i=0;i<count;i+=3){const ids=[0,1,2].map(k=>base+(index?index.getX(i+k):i+k));lines.push('f '+ids.map(n=>`${n}/${n}`).join(' '));}base+=p.count;}
const file=`${prefix}_${name}.obj`;writeFileSync(out+file,lines.join('\n'));const color=batch.material.color.clone().convertLinearToSRGB();result.push({file,material:name,color:[color.r,color.g,color.b],metalness:batch.material.metalness,roughness:batch.material.roughness});}return result;}

const kart=createKart();kart.remove(kart.userData.parts['Seated racing driver']);
let wheel; kart.traverse(o=>{if(o.name==='Steering wheel rim')wheel=o.parent;});
if(!wheel)throw new Error('Original authored steering wheel not found');
wheel.removeFromParent();wheel.position.set(0,0,0);wheel.rotation.set(0,0,0);
const manifest={kart:exportGroup(kart,'Kart'),wheel:exportGroup(wheel,'Wheel'),notice:'Original Road to F1 source. Wheel extracted with local pivot; remaining kart geometry unchanged.'};
writeFileSync(out+'manifest.json',JSON.stringify(manifest,null,2));console.log('Exported separate wheel and unchanged remaining kart geometry');
