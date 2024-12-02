import { setTimeout } from "timers/promises";
// import { Map as NativeMap, WiredTigerMapTable } from "../src/getModule.js";
import { makeid } from "./raw/utils.js";
// import { WiredTigerDB } from "../src/db.js";
import { Connection, getTimeMillis, MapTable, Session } from "../src/getModule.js";
import { WiredTigerDB } from "../src/db.js";

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
const iterations = 10_000_000;
const keyInts = new Array(keyCount).fill(0).map((_, i) => i);
const keys = new Array(keyCount).fill(0).map(() => makeid(17));
function testMap(map: { get(key: string): string | undefined, set(key: string, value: string): void }) {
  const startSetup = performance.now();
  keys.forEach(key => map.set(key, makeid(1000)));
  const endSetup = performance.now();
  const start = performance.now();
  for (let i = 0; i < iterations; i++) {
    const key = keys[Math.floor(Math.random() * keys.length)];
    const value = map.get(key);
    globalThis.value = value;
  }
  const end = performance.now();
  return { time: end - start, setup: endSetup - startSetup };
}
function testMapInt(map: { get(key: number): number | undefined, set(key: number, value: number): void }) {
  const startSetup = performance.now();
  keyInts.forEach(key => map.set(key, 1));
  const endSetup = performance.now();
  const start = performance.now();
  for (let i = 0; i < iterations; i++) {
    const key = keyInts[Math.floor(Math.random() * keys.length)];
    const value = map.get(key);
    globalThis.value = value;
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

class StringMapTable extends MapTable {
  constructor(conn: Connection, name: string) {
    super(conn, name, "key_format=S,value_format=S");
  }

  get(key: string): string {
    return super.getStringString(key) as string;
  }

  set(key: string, value: string) {
    return super.setStringString(key, value);
  }

  list(prefix: string) {
    return super.list([prefix])?.map(([key]) => key);
  }
}

class IntMapTable extends MapTable {
  constructor(conn: Connection, name: string) {
    super(conn, name, "key_format=i,value_format=i");
  }

  get(key: number): number {
    return super.get([key])?.[0] as number;
  }

  set(key: number, value: number) {
    return super.set([key], [value]);
  }

  list(prefix: number) {
    return super.list([prefix])?.map(([key]) => key);
  }
}

// class  ConvertToBufferNativeMap extends NativeMap {
//   #retain = new Array(keyCount);
//   set(key, value) {
//     const buffer = Buffer.from(value, "utf-8");
//     super.set(key, buffer);
//   }

//   get(key) {
//     return super.get(key)?.toString("utf-8");
//   }
// }

// class ConvertToBufferWiredTigerMapTable extends WiredTigerMapTable {
//   set(key, value) {
//     super.set(key, Buffer.from(value, "utf-8"));
//   }
// }

async function test() {
  // const map = new Map<string, string>();
  // console.log("map", testMap(map));
  // const oMap = new ConvertToBufferMap();
  // console.log("oMap", testMap(oMap));
  // const nativeMap = new ConvertToBufferNativeMap();
  // console.log("NativeMap", testMap(nativeMap));
  // console.log(nativeMap.get(keys[0]) == map.get(keys[0]));
  const db = new WiredTigerDB("NULL", { create: true, in_memory: true, cache_size: "1GB" });
  db.open();
  const mt = new StringMapTable(db.native, "testStringMap");
  console.log("StringMapTable", testMap(mt));
  // const mti = new IntMapTable(db.native, "testIntMap");
  // console.log("IntMapTable", testMapInt(mti));

  // const db2 = new WiredTigerDB("WT-HOME", { create: true, cache_size: "2MB" });
  // db2.open();
  // console.log("WiredTigerMapTable(Disk)", testMap(new WiredTigerMapTable(db2.native, "testMap")));
  gc();
  console.log(process.memoryUsage().rss / 1024 / 1024);
  console.log(getTimeMillis());
  // await setTimeout(100000);
}

test();
