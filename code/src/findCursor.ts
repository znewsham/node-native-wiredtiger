import { BSON } from "bson";
import { Collection } from "./collection.js";
import { ColumnSpec, SupportedTypes } from "./collectionSchema.js";
import { FindCursor as NativeFindCursor } from "./getModule.js";

export class FindCursor<TSchema extends object> {
  #cursor: NativeFindCursor<any, any>;
  #valueDefinitions: ColumnSpec[];
  #batch: TSchema[] = [];
  #batchSize = 100;

  constructor(
    cursor: NativeFindCursor<any, any>,
    valueDefinitions: ColumnSpec[]
  ) {
    this.#cursor = cursor;
    this.#valueDefinitions = valueDefinitions;
  }

  get native() {
    return this.#cursor;
  }

  nextBatch(batchSize?: number): TSchema[] {
    return this.#cursor.nextBatch(batchSize).map(([keyColumns, valueColumns]) => {
      let remaining;
      const doc = Object.fromEntries(valueColumns.map((value: any, index: number) => {
        const vd = this.#valueDefinitions[index];
        if (!vd) {
          throw new Error("Invalid Column");
        }
        if (vd.name == Collection.REMAINING_NAME) {
          remaining = BSON.deserialize(value);
          return undefined;
        }
        else if (vd.actualType == SupportedTypes.bson) {
          return [vd.name, BSON.deserialize(value)._];
        }
        return [vd.name, value];
      }).filter(Boolean));
      if (remaining) {
        Object.entries(remaining).forEach(([key, value]) => doc[key] = value);
      }
      return doc;
    }) as TSchema[];
  }

  close() {
    return this.#cursor.close();
  }

  reset() {
    return this.#cursor.reset();
  }

  forEach(callback: (doc: TSchema, index: number) => void): void {
    let i = 0;
    do {
      this.#batch = this.nextBatch(this.#batchSize);
      for (let item of this.#batch) {
        callback(item, i++);
      }
      if (this.#batch.length < this.#batchSize) {
        break;
      }
    }
    while (this.#batch.length);
  }

  toArray(): TSchema[] {
    const arr: TSchema[] = [];
    this.forEach((doc) => arr.push(doc));
    return arr;
  }
}
