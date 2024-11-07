import { makeid } from "../tests/raw/utils.js";
import { Collection } from "./collection.js";
import { RemainingSymbol, schema } from "./collectionSchema.js";
import { WiredTigerDB } from "./db.js";
import { CustomCollator } from "./getModule.js";
import { CustomExtractor } from "./getModule.js";
import { CreateTypeAndName, Operation, WiredTigerCursor, WiredTigerSession } from "./types.js";


const db = new WiredTigerDB("WT-HOME", { in_memory: true, create: true, statistics: ["all"] });
db.open();
let current = "A";
// db.native.addExtractor(
//   "TEST",
//   new CustomExtractor(
//     () => {},
//     () => {
//       return new CustomExtractor(
//         (session: WiredTigerSession, key: Buffer, value: Buffer, cursor: WiredTigerCursor) => {
//           cursor.setKey("test", current);
//           current = String.fromCharCode(current.charCodeAt(0) + 1);
//           cursor.insert();
//         })
//       },
//     () => {console.log("HERE2");}
//   ),
//   ""
// );

// db.native.addCollator(
//   "TEST",
//   new CustomCollator(
//     (session, value1, value2) => {
//       return value1.length - value2.length;
//     }
//   ),
//   null
// );

const col = new Collection(db, "Hello", {
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
    _id: schema.CharArray(17),
    stringThing: schema.String(),
    intThing: schema.Int(),
    doubleThing: schema.Double(),
    longThing: schema.Long(),
    bigThing: schema.BigInt(),
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
col.createIndex("reverse", ["stringThing"], ",collator=reverse");
col.createIndex("nocase", ["stringThing"], ",collator=nocase");
// col.createIndex("test", [], ",key_format=SS,extractor=TEST,collator=TEST");
col.createIndex("words", ["stringThing"], ",key_format=S,extractor=words");
col.createIndex("ngrams3", ["stringThing"], ",key_format=S,extractor=ngrams,collator=nocase,app_metadata={ngrams=3}");
col.createIndex("ngrams5", ["stringThing"], ",key_format=S,extractor=ngrams,app_metadata={ngrams=5}");
col.createIndex("double", ["doubleThing"]);
col.createIndex("bigint", ["bigThing"]);
col.createIndex("bsonBigint", ["bigThing"]);
col.insertMany([{
    _id: "aaa",
    stringThing: "test1 z",
    intThing: 10,
    doubleThing: Math.random(),
    longThing: 0x7fffffffffffffffn,
    bigThing: BigInt(-50000 + Math.floor(Math.random() * 100000))** BigInt(10),
    bsonBigThing: BigInt(-50000 + Math.floor(Math.random() * 100000))** BigInt(10),
    booleanThing: true,
    bitFieldThing: 0b11,
    bsonThing: "Hi",
    a: "Hello World"
  },
  {
    _id: "AAA",
    stringThing: new Array(10).fill(0).map(() => makeid(10)).join(" "),
    intThing: 2,
    doubleThing: Math.random(),
    longThing: 0x7fffffffffffffffn,
    bigThing: BigInt(-50000 + Math.floor(Math.random() * 100000))** BigInt(10),
    bsonBigThing: BigInt(-50000 + Math.floor(Math.random() * 100000))** BigInt(10),
    booleanThing: true,
    bitFieldThing: 0b10,
    bsonThing: "Bye",
    a: "Goodbye World"
  },
  {
    _id: "BBB",
    stringThing: new Array(10).fill(0).map(() => makeid(10)).join(" " ) + " test3" + " test4" + " ABC" + " test5",
    intThing: 2,
    doubleThing: -Math.random(),
    longThing: 0x7fffffffffffffffn,
    bigThing: BigInt(-50000 + Math.floor(Math.random() * 100000))** BigInt(10),
    bsonBigThing: BigInt(-50000 + Math.floor(Math.random() * 100000))** BigInt(10),
    booleanThing: true,
    bitFieldThing: 0b01,
    bsonThing: "Hi",
    a: "123"
  },
  {
    _id: "CCC",
    stringThing: new Array(10).fill(0).map(() => makeid(10)).join(" " ) + " test3" + " test4" + " ABC" + " test5",
    intThing: 2,
    doubleThing: Math.random(),
    longThing: 0x7fffffffffffffffn,
    bigThing: BigInt(-50000 + Math.floor(Math.random() * 100000))** BigInt(10),
    bsonBigThing: BigInt(-50000 + Math.floor(Math.random() * 100000))** BigInt(10),
    booleanThing: false,
    bitFieldThing: 0b00,
    bsonThing: "Hi",
    a: "456"
  }
]);

col.insertOne({
  _id: "DDD",
  stringThing: new Array(10).fill(0).map(() => makeid(10)).join(" " ) + " test3" + " test4" + " ABC" + " test5",
  intThing: 2,
  doubleThing: Math.random(),
  longThing: -8000000000000000n,
  bigThing: BigInt(-50000 + Math.floor(Math.random() * 100000)) ** BigInt(10),
  bsonBigThing: BigInt(-50000 + Math.floor(Math.random() * 100000)) ** BigInt(10),
  booleanThing: true,
  bitFieldThing: 0b11,
  bsonThing: "Hi",
  a: "789"
});

col.insertOne({
  _id: "EEE",
  stringThing: new Array(10).fill(0).map(() => makeid(10)).join(" " ) + " test3" + " test4" + " ABC" + " test5",
  intThing: 2,
  doubleThing: -Math.random(),
  longThing: -8000000000000000n,
  bigThing: -12345n,
  bsonBigThing: -12345n,
  booleanThing: true,
  bitFieldThing: 0b11,
  bsonThing: "Hi",
  a: "10"
});


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

const cursor = col.find([
  {
    operation: Operation.AND,
    conditions: [
      { index: "index:Hello:ngrams5", values: ["test"], operation: Operation.GE },
      { index: "index:Hello:ngrams5", values: ["tesu"], operation: Operation.LT }
    ]
  }
], { columns: ["_id"] });
console.log(cursor.nextBatch());
cursor.close();


let next;
function stats(type: `statistics:${CreateTypeAndName}`) {
  const cursor = col.db.openSession().openCursor(type, null);
  const obj: Record<string, number | bigint> = {};
  while(next = cursor.next()) {
    console.log(cursor.getKey(), cursor.getValue());
    // const buffer: Buffer = cursor.getValue();
    // const sepIndex = buffer.indexOf(0);
    // const sep2Index = buffer.indexOf(0, sepIndex + 1);
    // obj[buffer.slice(0, sepIndex).toString()] = intFromUnsignedBuffer(buffer.slice(sep2Index + 1));
  }
  cursor.close();

  return obj;
}

// console.log(stats("statistics:table:Hello")["cursor: next calls"]);
// console.log(stats("statistics:index:Hello:ngrams5")["cursor: next calls"]);


//db.close();
