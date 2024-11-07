import { setTimeout } from "timers/promises";
import { Map as NativeMap, WiredTigerMapTable } from "../src/getModule.js";
import { makeid } from "./raw/utils.js";
import { WiredTigerDB } from "../src/db.js";
import { WiredTigerSession } from "../src/types.js";

// class WiredTigerMap {
//   db: WiredTigerDB;
//   session: WiredTigerSession;
//   constructor(home: string) {
//     this.db = new WiredTigerDB();
//     this.db.open(home, "create,in_memory,cache_size=1GB");
//     this.session = this.db.openSession();
//     this.session.create("table:test","key_format=S,value_format=S");
//   }

//   set(key: string, value: string) {
//     this.session.set("test", key, value);
//   }

//   get(key: string) {
//     return this.session.get("test", key);
//   }
// }
const keyCount = 100000;
const iterations = 1_000_000;
const keys = new Array(keyCount).fill(0).map(() => makeid(17));
function testMap(map: { get(key: string): string | undefined, set(key: string, value: string): void }) {
  const startSetup = performance.now();
  keys.forEach(key => map.set(key, makeid(1000)));
  const endSetup = performance.now();
  const start = performance.now();
  for (let i = 0; i < iterations; i++) {
    const key = keys[Math.floor(Math.random() * keys.length)];
    const value = map.get(key);
  }
  const end = performance.now();
  return { time: end - start, setup: endSetup - startSetup };
}

class ConvertToBufferMap<K, V extends string> extends Map<K, Buffer> {
  set(key: K, value: string) {
    super.set(key, Buffer.from(value, "utf-8"));
  }
  get(key: K): string | undefined {
    return super.get(key)?.toString("utf-8");
  }
}

class ObjectMap {
  #obj: Record<string, string> = {};
  #fake: string[] = new Array(keyCount);
  #size = 0;
  set(key: string, value: string) {
    this.#fake[this.#size++] = Buffer.from(value, "utf-8");
    // this.#obj[key] = value;
  }
  get(key: string): string | undefined {
    // return this.#obj[key];
  }
}


class  ConvertToBufferNativeMap extends NativeMap {
  #retain = new Array(keyCount);
  set(key, value) {
    const buffer = Buffer.from(value, "utf-8");
    super.set(key, buffer);
  }

  get(key) {
    return super.get(key)?.toString("utf-8");
  }
}

class ConvertToBufferWiredTigerMapTable extends WiredTigerMapTable {
  set(key, value) {
    console.log("HERE");
    super.set(key, Buffer.from(value, "utf-8"));
  }
}

async function test() {
  const db = new WiredTigerDB(null, { create: true, in_memory: true, cache_size: "1GB" });
  db.open();
  const map = new ConvertToBufferMap<string, string>();
  //console.log("map", testMap(map));
  // const oMap = new ObjectMap();
  // console.log("oMap", testMap(oMap));
  const nativeMap = new ConvertToBufferNativeMap();
  //console.log("NativeMap", testMap(nativeMap));
  // console.log(nativeMap.get(keys[0]) == map.get(keys[0]));
  console.log("WiredTigerMapTable", testMap(new ConvertToBufferWiredTigerMapTable(db.native, "testMap")));
  gc();
  await setTimeout(100000);
}

test();
