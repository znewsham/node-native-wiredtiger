/* tslint:disable */
/* eslint-disable */

/* auto-generated by NAPI-RS */

export interface FindOptions {
  keyFormat?: string
  valueFormat?: string
  session?: Session
  columns?: string
}
export const enum Comparison {
  LessThan = -1,
  Equal = 0,
  GreaterThan = 1
}
export declare function structUnpack(session: Session | undefined | null, buffer: Buffer, format: string): unknown[]
export interface Document {
  key: unknown[]
  value: unknown[]
}
export interface Times {
  extract: number
  populate: number
}
export interface IndexSpec {
  indexName?: string
  operation: Operation
  subConditions?: Array<IndexSpec>
  queryValues?: unknown[]
}
export const enum Operation {
  AND = 0,
  OR = 1,
  LT = 2,
  GT = 3,
  LE = 4,
  GE = 5,
  EQ = 6,
  NE = 7,
  INDEX = 8
}
export declare class Connection {
  constructor(home: string, config: string)
  close(config?: string | undefined | null): void
  addCollator(name: string, collator: CustomCollator): number
  addExtractor(name: string, extractor: CustomExtractor): number
}
export declare class Session {
  constructor(conn: Connection, config?: string | undefined | null)
  create(uri: string, config?: string | undefined | null): void
  close(config?: string | undefined | null): void
  openCursor(cursorSpec: string, config?: string | undefined | null): Cursor
  beginTransaction(config?: string | undefined | null): void
  commitTransaction(config?: string | undefined | null): void
  rollbackTransaction(config?: string | undefined | null): void
  prepareTransaction(config?: string | undefined | null): void
  join(joinCursor: Cursor, refCursor: Cursor, config?: string | undefined | null): void
}
export declare class Cursor {
  close(): void
  next(): boolean
  getNext(keyFormatStr?: string | undefined | null, valueFormatStr?: string | undefined | null): Document | null
  prev(): boolean
  getPrev(keyFormatStr?: string | undefined | null, valueFormatStr?: string | undefined | null): Document | null
  reset(): void
  setKey(keyValues: unknown[]): void
  getKey(formatStr?: string | undefined | null): unknown[]
  setValue(valueValues: unknown[]): void
  getValue(formatStr?: string | undefined | null): unknown[]
  insert(): void
  update(): void
  remove(): void
  search(): boolean
  searchNear(): number
  compare(cursor: Cursor): number
  equals(cursor: Cursor): boolean
  bound(config: string): void
  get keyFormat(): string
  get valueFormat(): string
}
export declare class Table {
  constructor(connection: Connection, tableName: string, config: string)
  insertMany(documents: Array<Document>): number
  createIndex(indexName: string, config: string): void
  deleteMany(conditions?: Array<IndexSpec> | undefined | null): number
  updateMany(conditions: Array<IndexSpec> | undefined | null, newValue: unknown[]): number
  find(conditions?: Array<IndexSpec> | undefined | null, options?: FindOptions | undefined | null): FindCursor
}
export declare class FindCursor {
  close(): void
  next(): boolean
  prev(): boolean
  reset(): void
  nextBatch(keyFormatStr?: string | undefined | null, valueFormatStr?: string | undefined | null, batchSize?: number | undefined | null): Array<Document>
  getNext(keyFormatStr?: string | undefined | null, valueFormatStr?: string | undefined | null): Document | null
  getPrev(keyFormatStr?: string | undefined | null, valueFormatStr?: string | undefined | null): Document | null
  getKey(formatStr?: string | undefined | null): unknown[]
  getValue(formatStr?: string | undefined | null): unknown[]
  compare(cursor: FindCursor): number
  equals(cursor: FindCursor): boolean
  get keyFormat(): string
  get valueFormat(): string
}
export declare class CustomCollator {
  constructor(compare?: (session: Session, key1: Buffer, key2: Buffer) => number, customize?: (session: Session, uri: string, config: string) => number, terminate?: (session: Session) => number)
}
export declare class CustomExtractor {
  constructor(extract?: (session: Session, key: Buffer, value: Buffer, resultCursor: ResultCursor) => number, customize?: (session: Session, uri: string, config: string) => number, terminate?: (session: Session) => number)
}
export declare class ResultCursor {
  close(): void
  next(): boolean
  reset(): void
  setKey(keyValues: unknown[]): void
  insert(): void
  searchNear(): number
  get keyFormat(): string
}
export declare class MapTable {
  constructor(connection: Connection, tableName: string, config: string)
  list(prefix: Array<unknown>): Array<Array<unknown>>
  get(key: Array<unknown>): Array<unknown> | null
  set(key: Array<unknown>, value: Array<unknown>): void
  delete(key: Array<unknown>): boolean
  /** A helper function - returns an error if the key_format != "S" or the value_format != "S" */
  getStringString(key: string): string | null
  /** A helper function - returns an error if the key_format != "S" or the value_format != "S" */
  setStringString(key: string, value: string): void
  /** A helper function - returns an error if the key_format != "S" */
  deleteString(key: string): boolean
}
