// Browser-only independent ROM fixture assertions; never part of hardware builds.
function assert(ok,message='Fixture assertion failed'){if(!ok)throw Error(message);}
assert.equal=(a,b)=>assert(a===b,'Fixture equality');assert.deepEqual=(a,b)=>assert(JSON.stringify(a)===JSON.stringify(b),'Fixture bytes/fields');export default assert;
