const {
  performance
} = require('perf_hooks');
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

const iters = 100000000;
for (let i = 0; i < iters / 100; ++i) {
  obj.getProd();
}
const t0 = performance.now();
for (let i = 0; i < iters; ++i) {
  obj.getProd();
}
const t1 = performance.now();
console.log(1e6 * (t1 - t0) / iters, "nanoseconds per iter");
