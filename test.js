const addon = require('./build/Release/addon');

let obj = new addon.TestObj(10, 4);
console.log(obj.getSum());
console.log(obj.getProd());

obj.setFloat(234);
console.log(obj.getSum());
console.log(obj.getProd());
let l = new Float32Array(12);
for (let i = 0; i < l.length; ++i) {
  l[i] = i;
}
console.log(obj.sum(l));
console.log(obj.array());
