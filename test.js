const addon = require('./build/Release/addon');

let obj = new addon.TestObj(10, 4);
console.log(obj.getSum());
console.log(obj.getProd());

obj = new addon.TestObj(14040, 8321);
console.log(obj.getSum());
console.log(obj.getProd());
console.log(obj.test(123));

