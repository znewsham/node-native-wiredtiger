import { setTimeout } from "timers/promises";

const characters = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789';

// random alpha numeric strings of a specific length
function makeid(length) {
  let result = '';
  const charactersLength = characters.length;
  let counter = 0;
  // const resultArr = new Array(length);
  while (counter < length) {
    // resultArr[counter] = characters.charAt(Math.floor(Math.random() * charactersLength));
    result += characters.charAt(Math.floor(Math.random() * charactersLength));
    counter += 1;
  }
  return result;
  // return resultArr.join("");
}

// test setup - 100,000 keys 17 chars long, 1mn iterations, values are 1000 chars long
const keyCount = 100000;
const iterations = 1_000_000;
const keyLength = 17;
const valueLength = 1000;
const valueMultiplier = 1;
const splitStrings = false;
const keys = new Array(keyCount).fill(0).map(() => makeid(keyLength));

function testMap(map) {
  const startSetup = performance.now();

  keys.forEach(key => {
    const randomValue = makeid(splitStrings ? valueLength : valueLength * valueMultiplier);
    let value = splitStrings ? new Array(valueMultiplier).fill(0).map(() => randomValue).join("") : randomValue;
    map.set(key, value);
  });
  const endSetup = performance.now();
  console.log(map.get(keys[0]));
  const start = performance.now();
  for (let i = 0; i < iterations; i++) {
    const key = keys[Math.floor(Math.random() * keys.length)];
    const value = map.get(key);

    // v8 optimisation busting - without this the loop is 4x faster due to optimising out the get call
    globalThis.value = value;
  }
  const end = performance.now();
  return { time: end - start, setup: endSetup - startSetup };
}

// a naive implementation that keeps the API the same but converts value's into buffers
class ConvertToBufferMap extends Map {
  set(key, value) {
    super.set(key, Buffer.from(value, "utf-8"));
  }
  get(key) {
    return super.get(key)?.toString("utf-8");
  }
}


async function test() {
  const map = new Map();
  console.log("map", testMap(map));
  // const bufferMap = new ConvertToBufferMap();
  // console.log("bufferMap", testMap(bufferMap));
  gc();
  console.log("Memory usage: ", process.memoryUsage().rss / 1024 / 1024);

  // pause to go get a heap snapshot or whatever
  // await setTimeout(100000);
}

test();
