import {spawnSync,execFileSync} from 'node:child_process';import {mkdir,writeFile,readFile} from 'node:fs/promises';import {createHash} from 'node:crypto';
const dir=process.argv[2];if(!/^delivery\/R9_[0-9]{8}\/software(-[a-z0-9]+)?$/.test(dir))throw Error('Dedicated evidence directory required');await mkdir(dir,{recursive:true});
const args=['--test','--test-reporter=tap','tests/r9-package.test.mjs','tests/r9-recovery.test.mjs','tests/rom-status.test.mjs','tests/r7-candidate.test.mjs'];
const start=new Date().toISOString();const run=spawnSync(process.execPath,args,{encoding:'utf8',maxBuffer:8*1024*1024});
await writeFile(`${dir}/targeted-tests.tap`,run.stdout||'',{flag:'wx'});await writeFile(`${dir}/targeted-tests.stderr.txt`,run.stderr||'',{flag:'wx'});
const summary={startedAt:start,endedAt:new Date().toISOString(),node:process.version,platform:process.platform,arch:process.arch,command:['node',...args],exitCode:run.status,realDeviceAccess:false,fixtureType:'REAL_ROM_DRIVER_WITH_SYNTHETIC_TRANSPORT_AND_SYNTHETIC_FLASH',
 sourceCommit:execFileSync('git',['rev-parse','HEAD'],{encoding:'utf8'}).trim(),trackedWorktreeClean:spawnSync('git',['diff','--quiet','HEAD','--','.']).status===0,
 totals:Object.fromEntries(['tests','pass','fail','skipped'].map(k=>[k,Number(new RegExp(`^# ${k} (\\d+)$`,'m').exec(run.stdout||'')?.[1]||0)])),
 stdoutSha256:createHash('sha256').update(run.stdout||'').digest('hex')};
await writeFile(`${dir}/summary.json`,JSON.stringify(summary,null,2)+'\n',{flag:'wx'});console.log(JSON.stringify(summary));if(run.status!==0)process.exitCode=1;
