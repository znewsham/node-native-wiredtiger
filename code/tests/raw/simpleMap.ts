import { after, afterEach, before, beforeEach, describe, it } from 'node:test';
import { WiredTigerDBNative, WiredTigerSession } from '../../src/index.js';
import assert from 'node:assert';

describe("Simple Map", () => {
  let db: WiredTigerDBNative, session: WiredTigerSession;
  beforeEach(() => {
    db = new WiredTigerDBNative();
    db.open(null, "create,in_memory");
    session = db.openSession();
    session.create("table:test", "key_format=S,value_format=S");
  });

  afterEach(() => {
    db.close();
  });

  it("Should correctly return string values", () => {
    const key = `${Math.random()}`;
    const value = `${Math.random()}`;
    session.set("test", key, value);
    assert.strictEqual(session.get("test", key), value, "The fetched value matches");
  });

  it("Should correctly list keys", () => {
    const key1 = "Hello World";
    const key2 = "Hello Other World";
    const key3 = "Goodbye";
    session.set("test", key1, "test");
    session.set("test", key2, "test");
    session.set("test", key3, "test");
    assert.deepEqual(session.list("test"), ["Goodbye", "Hello Other World", "Hello World"], "All");
    assert.deepEqual(session.list("test", "Hello"), ["Hello Other World", "Hello World"], "Prefix");
    assert.deepEqual(session.list("test", "Hello", 1), ["Hello Other World"], "Limit");
    // assert.deepEqual(session.list("test"), [], "No keys");
  });
})
