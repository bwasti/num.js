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

function benchmark(f) {
  const it0 = performance.now();
  for (let i = 0; i < 10; ++i) {
    f();
  }
  const it1 = performance.now();
  const iters = 1e6 / Math.max((it1 - it0), 1e-9);
 
  // warmup 
  for (let i = 0; i < iters / 100; ++i) {
    f();
  }
  const t0 = performance.now();
  for (let i = 0; i < iters; ++i) {
    f();
  }
  const t1 = performance.now();
  console.log(1e6 * (t1 - t0) / iters, "nanoseconds per iter");
}

benchmark(function() {
  obj.getProd();
});

benchmark(function() {
  const t = new addon.Tensor([1,2]);
});

const t = new addon.Tensor([1,2]);
benchmark(function() {
  const s = t.sum();
});

// Not yet supported
//let tensor = addon.randn();

