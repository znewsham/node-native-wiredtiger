// @ts-nocheck
import { after, before, describe, it } from 'node:test';
import { WiredTigerDB, WiredTigerSession } from '../../src/getModule.js';
import { setTimeout } from 'timers/promises';

const KEY_COUNT = 10000;
const characters = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789';

function makeid(length) {
  let result = '';
  const charactersLength = characters.length;
  let counter = 0;
  while (counter < length) {
    result += characters.charAt(Math.floor(Math.random() * charactersLength));
    counter += 1;
  }
  return result;
}

describe("Scratch", () => {
  let db: WiredTigerDB, session: WiredTigerSession;
  before(() => {
    db = new WiredTigerDB();
    console.log(db.open(null, "create,in_memory,cache_size=1GB"));
    session = db.openSession();
  });

  after(() => {
    db.close();
  });

  it("Works", () => {
    console.log(session.create("table:test", "key_format=S,value_format=S,columns=(id,value1)"));

    const keys = new Array(KEY_COUNT).fill(0).map((a, i) => makeid(10));
    const map = new Map();
    keys.forEach(key => {
      const value = makeid(10);
      session.set("test", key, value);
      map.set(key, value)
    });

    const iterations = 100000;
    let start = performance.now();
    // for (let i = 0; i < iterations; i++) {
    //   const key = keys[Math.floor(Math.random() * keys.length)];
    //   // session.get("test", key);
    //   map.get(key);
    // }


    const OPERATION_EQ = 0
    const OPERATION_NE = 1
    const OPERATION_GE = 2
    const OPERATION_LE = 3
    const OPERATION_GT = 4
    const OPERATION_LT = 5
    const OPERATION_AND = 6;
    const OPERATION_OR = 7;
    const key = keys[0];
    const lowerBound = key.slice(0, 2);
    const upperBound = `${lowerBound.slice(0, 1)}${String.fromCharCode(lowerBound.charCodeAt(1) + 1)}`;

    console.log(key, keys.length);

    // console.log("LT", session.find("test", [{ values: [key], operation: OPERATION_LT }]).nextBatch().length);
    // console.log("GT", session.find("test", [{ values: [key], operation: OPERATION_GT }]).nextBatch().length);
    const index = keys.sort((a, b) => a.localeCompare(b)).indexOf(key);
    const cursor = session.find("test", [
      {
        values: [lowerBound],
        operation: OPERATION_GE,
      },
      {
        values: [upperBound],
        operation: OPERATION_LT
      }
      // {
      //   values: [key],
      //   operation: OPERATION_NE
      // }
      // {
      //   operation: OPERATION_OR,
      //   conditions: [
      //     {
      //       values: [key],
      //       operation: OPERATION_LT
      //     },
      //     {
      //       values: [key],
      //       operation: OPERATION_GT
      //     }
      //   ]
      // },
      // {
      //   operation: OPERATION_OR,
      //   conditions: [
      //     {
      //       values: [keys[0]],
      //     },
      //     {
      //       values: [keys[1]],
      //     },
      //     {
      //       values: [keys[2]],
      //     }
      //   ]
      // },
      // {
      //   operation: OPERATION_OR,
      //   conditions: [
      //     {
      //       values: [keys[0]],
      //     },
      //     {
      //       values: [keys[0]],
      //       operation: OPERATION_GT
      //     },
      //   ]
      // }
    ]);
    console.log(keys[0], keys[1], keys[2]);
    const relevantKeys = keys.slice(0, index);
    const batch = cursor.nextBatch();
    console.log(batch.length, batch);

    // console.log(cursor.nextBatch());

    // console.log(cursor.nextBatch());
    // console.log(session.list("test", "00"));// 0.000012, 0.000012, 0.000010
    // console.log(Array.from(map.keys()).filter(k => k.startsWith("00"))); // 0.000079, 0.000079, 0.000094
    let end = performance.now();
    console.log((end - start) / iterations);
  })
});
