import { makeid } from "../tests/raw/utils.js";
import { Collection } from "./collection.js";
import { RemainingSymbol, schema } from "./collectionSchema.js";
import { WiredTigerDB } from "./db.js";
import { CustomCollator, CustomExtractor, wiredTigerStructUnpack } from "./getModule.js";
import { configToString } from "./helpers.js";
import { CreateTypeAndName, Operation, StringKeysOfT, WiredTigerCursor, WiredTigerSession } from "./types.js";


let db = new WiredTigerDB("WT-HOME", { in_memory: true, create: true, statistics: ["all"] });
db.open();
db.native.addExtractor(
  "TEST",
  new CustomExtractor(
    () => {
      console.log("HERE1");
    },
    (session: WiredTigerSession, uri: string, appcfg: string) => {
      console.log(uri, appcfg);
      return new CustomExtractor(
        (session: WiredTigerSession, key: Buffer, value: Buffer, cursor: WiredTigerCursor) => {
          try {
            const docValues = wiredTigerStructUnpack(session, value, "Siu");
            cursor.setKey(docValues[2]);
            cursor.insert();
          }
          catch (e: any) {
            if (e.message.endsWith("item not found")) {
              return;
            }
            console.log(e);
            throw e;
          }
        })
      },
    () => {console.log("HERE2");}
  ),
  ""
);

// db.native.addCollator(
//   "TEST",
//   new CustomCollator(
//     (session, value1, value2) => {
//       return value1.length - value2.length;
//     }
//   ),
//   null
// );

let col = new Collection(db, "Hello", {
  config: {
    // colgroups: [
    //   {
    //     name: "test",
    //     columns: ["stringThing"]
    //   },
    //   {
    //     name: "test2",
    //     columns: ["intThing"]
    //   }
    // ]
  },
  schema: {
    _id: schema.String(),
    stringThing: schema.String(),
    stringThing2: schema.String(),
    intThing: schema.Int(),
    bigThing: schema.BigInt(),
    doubleThing: schema.Double(),
    longThing: schema.Long(),
    // binaryThing: schema.Binary(),
    booleanThing: schema.Boolean(),
    bitFieldThing: schema.BitField(4),
    bsonThing: schema.BSON(schema.String()),
    bsonBigThing: schema.BSON(schema.BigInt()),
    [RemainingSymbol]: schema.BSON({
      a: schema.String()
    })
    // TODO
    // bsonThing2: schema.BSON(schema.BSON({
    //   a: schema.String()
    // })),
  }
});

col.createIndex("inorder", ["stringThing"]);
col.createIndex("reverse", ["stringThing"]);
// col.createIndex("nocase", ["stringThing"], ",collator=nocase");
// col.createIndex("test", [], ",key_format=SS,extractor=TEST,collator=TEST");
col.createIndex("words", [{ extractor: "words", columns: ["stringThing"]}]);
col.createIndex("ngrams3", [{ extractor: "ngrams", ngrams: 3, columns: ["stringThing"] }]);
col.createIndex("ngrams5", [{ extractor: "ngrams", ngrams: 5, columns: ["stringThing"] }]);
col.createIndex("testNgrams", [{ columns: ["stringThing", "stringThing2" ], extractor: "ngrams", ngrams: 3 }]);
col.createIndex("double", ["doubleThing"]);
col.createIndex("bigThing", ["bigThing"]);
col.createIndex("testCompound", [
  { columns: ["stringThing"], extractor: "words", direction: 1 },
  { columns: ["stringThing"], extractor: "words", direction: -1 }
])
// col.createIndex("bsonBigThing", ["bsonBigThing"]); // TODO: this causes an invalidPointer error
console.log("finished index creation");
col.insertMany([
  {
    _id: "aaa",
    stringThing: "test1 z",
    stringThing2: "test1 z",
    intThing: 10,
    doubleThing: Math.random(),
    longThing: 0x7fffffffffffffffn,
    bigThing: 100n,//BigInt(-50000 + Math.floor(Math.random() * 100000))** BigInt(10),
    bsonBigThing: BigInt(-50000 + Math.floor(Math.random() * 100000))** BigInt(10),
    booleanThing: true,
    bitFieldThing: 0b11,
    bsonThing: "Hi",
    a: "Hello World"
  },
  {
    _id: "AAA",
    stringThing: "test", //new Array(10).fill(0).map(() => makeid(10)).join(" "),
    stringThing2: "test", //new Array(10).fill(0).map(() => makeid(10)).join(" "),
    intThing: 2,
    doubleThing: Math.random(),
    longThing: 0x7fffffffffffffffn,
    bigThing: -1n, //BigInt(-50000 + Math.floor(Math.random() * 100000))** BigInt(10),
    bsonBigThing: BigInt(-50000 + Math.floor(Math.random() * 100000))** BigInt(10),
    booleanThing: true,
    bitFieldThing: 0b10,
    bsonThing: "Bye",
    a: "Goodbye World"
  },
  {
    _id: "BBB",
    stringThing: " test5",
    stringThing2: " test5",
    intThing: 2,
    doubleThing: -Math.random(),
    longThing: 0x7fffffffffffffffn,
    bigThing: 3n, //BigInt(-50000 + Math.floor(Math.random() * 100000))** BigInt(10),
    bsonBigThing: BigInt(-50000 + Math.floor(Math.random() * 100000))** BigInt(10),
    booleanThing: true,
    bitFieldThing: 0b01,
    bsonThing: "Hi",
    a: "123"
  },
  {
    _id: "CCC",
    stringThing: "test", //new Array(10).fill(0).map(() => makeid(10)).join(" " ) + " test3" + " test4" + " ABC" + " test5",
    stringThing2: new Array(10).fill(0).map(() => makeid(10)).join(" " ) + " test3" + " test4" + " ABC" + " test5",
    intThing: 2,
    doubleThing: -Math.random(),
    longThing: 0x7fffffffffffffffn,
    bigThing: 4n, //BigInt(-50000 + Math.floor(Math.random() * 100000))** BigInt(10),
    bsonBigThing: BigInt(-50000 + Math.floor(Math.random() * 100000))** BigInt(10),
    booleanThing: false,
    bitFieldThing: 0b00,
    bsonThing: "Hi",
    a: "456"
  }
]);
console.log("finished multi insert");

col.insertOne({
  _id: "DDD",
  stringThing: "test4", //new Array(10).fill(0).map(() => makeid(10)).join(" " ) + " test3" + " test4" + " ABC" + " test5",
  stringThing2: "tester",
  intThing: 2,
  doubleThing: -Math.random(),
  longThing: -8000000000000000n,
  bigThing: 5n, //BigInt(-50000 + Math.floor(Math.random() * 100000)) ** BigInt(10),
  bsonBigThing: BigInt(-50000 + Math.floor(Math.random() * 100000)) ** BigInt(10),
  booleanThing: true,
  bitFieldThing: 0b11,
  bsonThing: "Hi",
  a: "789"
});

col.insertOne({
  _id: "EEE",
  stringThing: "test2", // new Array(10).fill(0).map(() => makeid(10)).join(" " ) + " test3" + " test4" + " ABC" + " test5",
  stringThing2: "tester",
  intThing: 2,
  doubleThing: -Math.random(),
  longThing: -8000000000000000n,
  bigThing: 6n, //-12345n,
  bsonBigThing: -12345n,
  booleanThing: true,
  bitFieldThing: 0b11,
  bsonThing: "Hi",
  a: "10"
});
console.log("Finished insert");

let session = col.db.openSession();
let realCursor = session.openCursor("file:Hello_testCompound.wti", null);
while (realCursor.next()) {
  console.log(realCursor.getKey("SSS")/*, realCursor.getValue()*/);
}
realCursor.close();

console.log("Finished file read");

// const directCursor = col.db.openSession().openCursor('index:Hello:bigint', null);
// while (directCursor.next()) {
//   console.log(directCursor.getKey('z')[0], directCursor.getValue("SidqzTt"));
// }


// const arr = col.find([{ index: 'index:Hello:bsonBigint', operation: Operation.INDEX }]).toArray();

// TODO: the format enforced is that of the table, not the index. Might be a bit painful
// const arr = col.find([{ index: 'index:Hello:test', operation: Operation.EQ, values: ["test", "A"] }]).toArray();
// const session = col.db.openSession();
// console.log(arr);

// const testCursor = session.openCursor("index:Hello:test", null);
// testCursor.setKey("test", "B");
// testCursor.search();
// console.log(testCursor.getKey(), testCursor.getValue());


// const session = col.db.openSession();
// const insertCursor = session.openCursor("table:Hello", null);
// insertCursor.setKey("AAA");
// insertCursor.setValue("test1");
// insertCursor.insert();
// const c1 = session.openCursor("index:Hello:ngrams5", null);
// c1.setKey("test");
// console.log(c1.searchNear()); // if searchNear returns -1, there are no results we could early exit.
// const c2 = session.openCursor("index:Hello:ngrams5", null);
// c2.setKey("tesu");
// const upper = c2.searchNear(); // if searchNear returns 1, don't add it.
// console.log(upper);
// const j = session.openCursor("join:table:Hello", null);
// session.join(j, c1, "operation=and,compare=ge");
// if (upper !== -1) {
//   session.join(j, c2, "operation=and,compare=lt");
// }
// let count = 0;
// while(j.next() == true){
//   console.log(j.getKey(), j.getValue()[0]);
//   count++;
// }
// console.log("Found: ", count);
let cursor = col.find([
  {
    operation: Operation.AND,
    conditions: [
      { index: "index:Hello:ngrams5", values: ["test"], operation: Operation.GE },
      { index: "index:Hello:ngrams5", values: ["tesu"], operation: Operation.LT }
    ]
  }
], { columns: ["doubleThing"] });
console.log(cursor.toArray());
cursor.close();

console.log("started multi update");
col.updateMany([
  {
    operation: Operation.AND,
    conditions: [
      { index: "index:Hello:ngrams5", values: ["test"], operation: Operation.GE },
      { index: "index:Hello:ngrams5", values: ["tesu"], operation: Operation.LT }
    ]
  }
], { $set: { intThing: 1 } });
console.log("ended multi update");
cursor = col.find([
  {
    operation: Operation.AND,
    conditions: [
      { index: "index:Hello:ngrams5", values: ["test"], operation: Operation.GE },
      { index: "index:Hello:ngrams5", values: ["tesu"], operation: Operation.LT }
    ]
  }
], { columns: ["_id", "intThing"] });
console.log(cursor.nextBatch());
cursor.close();

console.log(col.deleteMany([
  {
    operation: Operation.AND,
    conditions: [
      { index: "index:Hello:ngrams5", values: ["test"], operation: Operation.GE },
      { index: "index:Hello:ngrams5", values: ["tesu"], operation: Operation.LT }
    ]
  }
]));

// let next;
// function stats(type: `statistics:${CreateTypeAndName}`) {
//   const cursor = col.db.openSession().openCursor(type, null);
//   const obj: Record<string, Record<string, number | bigint>> = {};
//   while(next = cursor.next()) {
//     const [fullKey, _strValue, numValue] = cursor.getValue();
//     const [prefix, key] = fullKey.split(": ");
//     if (!obj[prefix]) {
//       obj[prefix] = {};
//     }
//     obj[prefix][key] = numValue;
//   }
//   cursor.close();

//   return obj;
// }
// console.log(stats("statistics:table:Hello"));


col = null;
db.close();
// cursor = null;

db = null;
gc({execution: "sync", flavor: "last-resort", type: "major"});
// console.log(stats("statistics:table:Hello")["cursor: next calls"]);
// console.log(stats("statistics:index:Hello:ngrams5")["cursor: next calls"]);


