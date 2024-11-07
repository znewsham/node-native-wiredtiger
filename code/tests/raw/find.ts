import { after, before, describe, it } from 'node:test';
import { Operation, WiredTigerDBNative, WiredTigerSession } from '../../src/index.js';
import assert from 'node:assert';
import { makeid } from './utils.js';

describe("Find", () => {
  describe("Single key field", () => {
    let db: WiredTigerDBNative, session: WiredTigerSession;
    const keys: string[] = [];
    before(() => {
      db = new WiredTigerDBNative();
      db.open(null, "create,in_memory,cache_size=1GB");
      session = db.openSession();
      session.create("table:test", "key_format=S,value_format=S,columns=(id,value)");

      for (let i = 0; i < 1000; i++) {
        const key = makeid(10);
        keys.push(key);
        session.set("test", key, "test");
      }
      keys.sort();
    });

    after(() => {
      session.close();
      db.close();
    });

    it("Should work with simple equality", () => {
      const key = keys[10];
      const cursor = session.find<[string], [string]>(
        "test",
        [{
          operation: Operation.EQ,
          values: [key]
        }]
      );
      const result = cursor.nextBatch();
      cursor.close();
      assert.deepEqual(result, [[[key], ["test"]]]);
    });

    it("Should work with multiple equality", () => {
      // for unknown reasons, this test is flaky.
      const cursor = session.find<[string], [string]>(
        "test",
        [
          {
            operation: Operation.OR,
            conditions: [
              {
                operation: Operation.EQ,
                values: [keys[10]]
              },
              {
                operation: Operation.EQ,
                values: [keys[11]]
              },
              {
                operation: Operation.EQ,
                values: [keys[12]]
              }
            ]
          }
        ]
      );
      const result = cursor.nextBatch();
      cursor.close();
      assert.deepEqual(result.map(([[k]]) => k), [keys[10], keys[11], keys[12]]);
    });

    it("Should work with prefix", () => {
      const prefix = keys[10].slice(0, 2);
      const lowerBound = prefix;
      const upperBound = `${prefix[0]}${String.fromCharCode(prefix.charCodeAt(1) + 1)}`
      const prefixes = keys.filter(key => key.startsWith(prefix));
      const cursor = session.find<[string], [string]>(
        "test",
        [
          {
            operation: Operation.GE,
            values: [lowerBound]
          },
          {
            operation: Operation.LT,
            values: [upperBound]
          }
        ]
      );
      const result = cursor.nextBatch();
      cursor.close();
      assert.strictEqual(result.length, prefixes.length);
    });

    it("Should work with NE", () => {
      const cursor = session.find<[string], [string]>(
        "test",
        [
          {
            operation: Operation.NE,
            values: [keys[10]]
          }
        ]
      );
      const result = cursor.nextBatch(10000);
      cursor.close();
      assert.strictEqual(result.length, keys.length - 1);
    });

    it("Should work with LT or GT exclusion (NE)", () => {
      const cursor = session.find<[string], [string]>(
        "test",
        [
          {
            operation: Operation.OR,
            conditions: [
              {
                operation: Operation.GT,
                values: [keys[10]]
              },
              {
                operation: Operation.LT,
                values: [keys[10]]
              }
            ]
          }
        ]
      );

      const result = cursor.nextBatch(10000);
      cursor.close();
      assert.strictEqual(result.length, keys.length - 1);
    });

    it("Should work with LT or GT inclusion (range)", () => {
      const cursor = session.find<[string], [string]>(
        "test",
        [
          {
            operation: Operation.GT,
            values: [keys[11]]
          },
          {
            operation: Operation.LT,
            values: [keys[13]]
          }
        ]
      );
      const result = cursor.nextBatch();
      cursor.close();
      assert.deepEqual(result, [[[keys[12]], ["test"]]]);
    });
  });

  describe("Using an index", () => {
    let db: WiredTigerDBNative, session: WiredTigerSession;
    const keys: string[] = [];
    const docs: [string, [string, string]][] = [];
    before(() => {
      db = new WiredTigerDBNative();
      db.open(null, "create,in_memory, cache_size=1GB");
      session = db.openSession();
      session.create("table:test", "key_format=S,value_format=SS,columns=(id,value1,value2)");
      session.create("index:test:value1", "columns=(value1)");
      session.create("index:test:both", "columns=(value1,value2)");

      for (let i = 0; i < 1000; i++) {
        const key = makeid(10);
        keys.push(key);
        docs.push([key, [makeid(10), makeid(20)]]);
      }
      session.insertMany("test", docs);
    });

    after(() => {
      session.close();
      db.close();
    });

    it("Should search the index correctly", () => {
      const cursor = session.find("test", [{
        index: "index:test:value1",
        values: [docs[0][1][0]]
      }]);

      const result = cursor.nextBatch();
      cursor.close();
      assert.deepEqual(result, [[[docs[0][0]], docs[0][1]]], `Should find value1:${docs[0][1][0]}`);
    });

    it("Should search a compound index correctly (found)", () => {
      session.insertMany("test", [["document", [docs[0][1][0], "INCORRECT"]]]);
      const cursor = session.find("test", [{
        index: "index:test:both",
        values: docs[0][1]
      }]);

      const result = cursor.nextBatch();
      cursor.close();
      assert.deepEqual(result, [[[docs[0][0]], docs[0][1]]], `Should find value1:${docs[0][1][0]}`);
    });

    it("Should search a compound index correctly (partial range - not implemented)", () => {
      return;
      // not 100% sure how to express this in these raw terms
      const cursor = session.find("test", [{
        operation: Operation.GE,
        index: "index:test:both",
        values: [docs[0][1][0]]
      }]);

      const result = cursor.nextBatch();
      cursor.close();
      assert.deepEqual(result, [], `Should not find find value1:${docs[0][1][0]}`);
    });

    it("Should search a compound index correctly (partial exact match does not return - broken)", () => {
      return;
      // I thought that by appending a delimiter (e.g., \0) in the find itself, that this could be supported, but I was incorrect
      const cursor = session.find("test", [{
        index: "index:test:both",
        values: [docs[0][1][0]]
      }]);

      const result = cursor.nextBatch();
      cursor.close();
      assert.deepEqual(result, [], `Should not find find value1:${docs[0][1][0]}`);
    });
  });
});
