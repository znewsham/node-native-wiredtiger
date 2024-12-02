import { Comparison, Operation, structUnpack, Connection, CustomCollator, CustomExtractor, Session, Table } from '../code/src/getModule.js'


function test() {
  const wt = new Connection("WT-HOME", "in_memory,create");
  const session = new Session(wt);

  const collator = new CustomCollator(
    null,
    function (innerSession, uri, config) {
      return new CustomCollator(
        function (innerSession, key1, key2) {
          if (key1.length != key2.length) {
            return [0, key1.length > key2.length ? Comparison.GreaterThan : Comparison.LessThan];
          }
          for (let i = 0; i < Math.min(key1.length, key2.length); i++) {
            if (key1[i] != key2[i]) {
              return [0, key1[i] > key2[i] ? Comparison.GreaterThan : Comparison.LessThan];
            }
          }
          return [0, Comparison.Equal];
        }
      )
    },
    null
  );
  wt.addCollator("test", collator);

  const extractor = new CustomExtractor(
    null,
    function (innerSession, uri, config) {
      return new CustomExtractor(
        function (innerSession, key, value, resultCursor) {
          let values = structUnpack(innerSession, value, "SSdz");
          resultCursor.setKey([values[3], null]);
          resultCursor.insert();
          resultCursor.reset();
        }
      )
    }
  )
  wt.addExtractor("test", extractor);


  const table = new Table(wt, "test", "key_format=17s,value_format=SSuu,columns=(_id,value1,value2,doubleValue,bigIntValue)");

  console.log(table.insertMany([
    {
      key: ["Test"],
      value: ["Hello Hek World", "Value2" ,123.45, 1n << 64n]
    },
    {
      key: ["Test2"],
      value: ["Hello World", "Value2" ,123.45, -1n]
    },
    {
      key: ["Test1"],
      value: ["Value3", "Value4" ,-123.45, -1n]
    },
    {
      key: ["Test3"],
      value: ["Value3", "Value4" ,-123.45, 1n << 64n]
    }
  ]));

  // table.createIndex("bigInt", "key_format=u,collator=test,extractor=test,columns=(bigIntValue),app_metadata=test");
  table.createIndex("value1", "key_format=SS,columns=(value1,value2),extractor=multikey,app_metadata={table_value_format=SSuu,key_extract_format=SSxx,index_id=test,columns=2,column0={direction=1,ngrams=3,extractor=ngrams,format=S,columns=(1)},column1={direction=1,format=S,columns=(2)}}");
  // table.createIndex("compound", "key_format=uu,columns=(doubleValue,bigIntValue),collator=compound,app_metadata={table_value_format=SSuu,key_format=uu,key_extract_format=xxuu,index_id=test,columns=2,column0={direction=1,format=u},column1={direction=1,format=u}}");
  table.createIndex("compoundstr", "key_format=SS,columns=(value1,value2),collator=compound,app_metadata={table_value_format=SSuu,key_format=SS17s,key_extract_format=SSxx,index_id=test,columns=2,column0={direction=1,format=S},column1={direction=-1,format=S}}");

  console.log("STARTED FIND");
  let next;
  const findCursor1 = table.find([
    {
      operation: Operation.GE,
      indexName: "index:test:value1",
      queryValues: ["Hek", "Value2"]
    },
    {
      operation: Operation.LT,
      indexName: "index:test:value1",
      queryValues: ["Hem", ""]
    }
  ]);
  console.log("ENDED FIND");
  while (next = findCursor1.next()) {
    console.log(findCursor1.getKey(), findCursor1.getValue("SSdz"));
  }
  findCursor1.close();
  console.log("Started update");
  table.updateMany(
    [
      {
        operation: Operation.GE,
        indexName: "index:test:value1",
        queryValues: ["Hek", "Value2"]
      },
      {
        operation: Operation.LT,
        indexName: "index:test:value1",
        queryValues: ["Hem", ""]
      }
    ],
    [undefined, "Goodbye World", 1.23, -1n]
  )
  console.log("Ended update");
  const findCursor2 = table.find([
    {
      operation: Operation.GE,
      indexName: "index:test:value1",
      queryValues: ["Hek", "Goodbye World"]
    },
    {
      operation: Operation.LT,
      indexName: "index:test:value1",
      queryValues: ["Hem", ""]
    }
  ], { keyFormat: "17s", valueFormat: "SSdz", session });
  console.log(findCursor2.nextBatch());


  let writeCursor = session.openCursor("table:test");
  writeCursor.setKey(["TestWrite"]);
  writeCursor.setValue(["Write World", "Write" ,123.45, 1n << 64n]);
  writeCursor.insert();
  // const cursor = session.openCursor("table:test");
  // console.log(cursor.getNext("17s", "SSdz"));
  // console.log(cursor.getNext("17s", "SSdz"));


  const indexCursor = session.openCursor("index:test:value1");
  while (next = indexCursor.getNext("SS")) {
    console.log(next.key);
  }

  // const metadataLower = session.openCursor("metadata:");
  // metadataLower.setKey(["index:test:"]);
  // metadataLower.searchNear();

  // const metadataUpper = session.openCursor("metadata:");
  // metadataUpper.setKey(["index:test;"]);
  // metadataUpper.searchNear();
  // console.log(metadataLower.getKey(), metadataUpper.getKey());
  // let next;
  // while (next = metadata.getNext()) {
  //   console.log(next);
  // }
  session.close();
  wt.close();
}

test();
