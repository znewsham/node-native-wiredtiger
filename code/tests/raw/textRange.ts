import { makeid } from "./utils.js";
import { Collection } from "../../src/collection.js";
import { RemainingSymbol, schema } from "../../src/collectionSchema.js";
import { WiredTigerDB } from "../../src/db.js";
import { CreateTypeAndName, Operation, WiredTigerCursor, WiredTigerSession } from "../../src/types.js";


const db = new WiredTigerDB("WT-HOME", { in_memory: true, create: true, statistics: ["all"] });
db.open();
let current = "A";

const col = new Collection(db, "Hello", {
  schema: {
    _id: schema.CharArray(17),
    stringThing: schema.String(),
    intThing: schema.Int(),
  }
});


col.createIndex("intThing", ["intThing"]);
col.insertMany([
  {
    _id: "AAA",
    stringThing: "test1",
    intThing: 1
  },
  {
    _id: "BBB",
    stringThing: "test2",
    intThing: 3
  },
  {
    _id: "CCC",
    stringThing: "test3",
    intThing: 5
  },
  {
    _id: "DDD",
    stringThing: "test4",
    intThing: 6
  }
]);
const session = col.db.openSession();
const c1 = session.openCursor("index:Hello:intThing", null);
c1.setKey(3);
console.log(c1.searchNear()); // if searchNear returns -1, there are no results we could early exit.
const c2 = session.openCursor("index:Hello:intThing", null);
c2.setKey(6);
const upper = c2.searchNear(); // if searchNear returns 1, don't add it.
console.log(upper);
const j = session.openCursor("join:table:Hello", null);
session.join(j, c1, "operation=and,compare=ge");
if (upper !== -1) {
  session.join(j, c2, "operation=and,compare=lt");
}
let count = 0;
while(j.next() == true){
  console.log(j.getKey());
  count++;
}
console.log("Found: ", count);

db.close();
