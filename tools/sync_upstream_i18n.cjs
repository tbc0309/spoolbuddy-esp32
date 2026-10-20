// Extract data-only upstream locale objects without changing the reference checkout.
const fs=require('fs'),path=require('path'),vm=require('vm'),crypto=require('crypto');
const root=path.resolve(__dirname,'..');
const base=path.join(root,'upstream/bambuddy/frontend/src/i18n/locales');
const dest=path.join(root,'vendor/spoolbuddy/i18n');fs.mkdirSync(dest,{recursive:true});
let provenance={repository:'https://github.com/maziggy/bambuddy',
  commit:JSON.parse(fs.readFileSync(path.join(root,'upstream-versions.json'),'utf8')).bambuddy.tested_main_commit,
  normalization:'LF',files:{}};
for(const lang of ['en','zh-CN']) {
  const source=fs.readFileSync(path.join(base,lang+'.ts'),'utf8').replace(/\r\n/g,'\n');
  const object=vm.runInNewContext('('+source.replace(/^export default\s*/,'').replace(/;\s*$/,'')+')',Object.create(null),{timeout:1000});
  let flat={};
  function walk(v,p='') {for(const [k,x] of Object.entries(v)) typeof x==='string' ? flat[p+k]=x : walk(x,p+k+'.');}
  walk(object);
  fs.writeFileSync(path.join(dest,lang+'.json'),JSON.stringify(flat,null,2)+'\n');
  provenance.files[lang]={source:'frontend/src/i18n/locales/'+lang+'.ts',sha256:crypto.createHash('sha256').update(source).digest('hex')};
}
const defaults={};
for(const dir of ['pages/spoolbuddy','components/spoolbuddy'])for(const name of fs.readdirSync(path.join(root,'upstream/bambuddy/frontend/src',dir))) {
  if(!name.endsWith('.tsx'))continue;
  const source=fs.readFileSync(path.join(root,'upstream/bambuddy/frontend/src',dir,name),'utf8').replace(/\r\n/g,'\n');
  for(const m of source.matchAll(/\bt\(\s*(['"])([\w.]+)\1\s*,\s*((?:'(?:[^'\\]|\\.)*')|(?:"(?:[^"\\]|\\.)*"))/g))
    defaults[m[2]]=vm.runInNewContext(m[3],Object.create(null),{timeout:1000});
  provenance.files[dir+'/'+name]={sha256:crypto.createHash('sha256').update(source).digest('hex')};
}
fs.writeFileSync(path.join(dest,'defaults.json'),JSON.stringify(defaults,null,2)+'\n');
fs.writeFileSync(path.join(dest,'provenance.json'),JSON.stringify(provenance,null,2)+'\n');
