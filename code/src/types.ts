import { AnySchemaEntry, SupportedTypes } from "./collectionSchema.js";
import { IndexSpec, Operation } from "./getModule.js";


type ValueOperations = Operation.EQ | Operation.NE | Operation.GE | Operation.LE | Operation.GT | Operation.LT;

type VerboseOperationTypes = "all" | "api" | "backup" | "block" | "block_cache" | "checkpoint" | "checkpoint_cleanup" | "checkpoint_progress" | "chunkcache" | "compact" | "compact_progress" | "error_returns" | "evict" | "evict_stuck" | "evictserver" | "fileops" | "generation" | "handleops" | "history_store" | "history_store_activity" | "log" | "lsm" | "lsm_manager" | "metadata" | "mutex" | "out_of_order" | "overflow" | "read" | "reconcile" | "recovery" | "recovery_progress" | "rts" | "salvage" | "shared_cache" | "split" | "temporary" | "thread_group" | "tiered" | "timestamp" | "transaction" | "verify" | "version" | "write";
type VerbosityLevel = 0 | 1 | 2 | 3 | 4 | 5;
export type VerboseOperations = VerboseOperationTypes | `${VerboseOperationTypes}:${VerbosityLevel}`;
type ValueQueryCondition<Values extends any[]> = {
  index?: CreateTypeAndName;
  operation?: ValueOperations;
  queryValues: Values; // TODO
}
type IndexOnlyQueryCondition = {
  index: CreateTypeAndName;
  operation: Operation.INDEX
}
type ConditionQueryCondition<Values extends any[]> = {
  index?: CreateTypeAndName;
  operation: Operation.AND | Operation.OR;
  subConditions: QueryCondition<Values>[];
}
export type QueryCondition<Values extends any[]> = IndexSpec & (ConditionQueryCondition<Values> | ValueQueryCondition<Values> | IndexOnlyQueryCondition);

type BasicRecord = {
  [k in string]: string | number | string[] | number[] | BasicRecord | BasicRecord[]
};

export type StringKeysOfT<T extends Record<string, AnySchemaEntry>> = {[k in keyof T]: k extends string ? T[k]["actualType"] extends SupportedTypes.string ? k : never : never}[keyof T];


export type IndexColumnOptions<ISchema extends Record<string, any>> = ((keyof ISchema) & string)
  | { extractor: "ngrams", ngrams: number, columns: StringKeysOfT<ISchema>[], direction?: 1 | -1 }
  | { extractor: "words", columns: StringKeysOfT<ISchema>[], direction?: 1 | -1 }
  | { columns: ((keyof ISchema) & string)[], direction?: 1 | -1 }

export type IndexCreationOptions = {
  extractor?: string,
  collator?: string,
  immutable?: boolean,
}

type CreateTypes = 'table' | 'colgroup' | 'index';

type TableCreateTypeAndName = `table:${string}`;
type ColgroupCreateTypeAndName = `colgroup:${string}:${string}`;
type IndexCreateTypeAndName = `index:${string}:${string}`;

export type FindOptions<availableColumns extends string> = {
  valueFormat?: string,
  keyFormat?: string,
  session?: WiredTigerSession,
  columns?: availableColumns[]
}

export type CreateTypeAndName = TableCreateTypeAndName | ColgroupCreateTypeAndName | IndexCreateTypeAndName;

export type CreateTypeAndNameWithProjection = `${CreateTypeAndName}(${string})`;

type JoinTypeAndName = `join:${string}`;

type JoinTypeAndNameWithProjection = `${JoinTypeAndName}(${string})`

export declare class FindCursor<Key extends any[], Value extends any[]> {
  close(): void;
  reset(): void;
  nextBatch(batchSize?: number): [Key, Value][]
}

export declare class WiredTigerSession {
  create(typeAndName: CreateTypeAndName, tableConfig: string): void;
  close(): void;
  openCursor(typeAndName: `statistics:${CreateTypeAndName}` | CreateTypeAndName | CreateTypeAndNameWithProjection | JoinTypeAndName | JoinTypeAndNameWithProjection, config: string | null): WiredTigerCursor;
  join(joinCursor: WiredTigerCursor, refCursor: WiredTigerCursor, config: string): void;
  beginTransaction(config: string): void;
  prepareTransaction(config: string): void;
  commitTransaction(config: string): void;
  rollbackTransaction(config: string): void;
}

type TODO = any;
type extractCallback = (session: WiredTigerSession, key: Buffer, value: Buffer, resultCursor: WiredTigerCursor) => void;
type compareCallback = (session: WiredTigerSession, value1: Buffer, value2: Buffer) => number;
type customizeCallback<Custom extends CustomExtractor | CustomCollator> = (session: WiredTigerSession, resourceURI: string, configItem: TODO) => Custom | void;
type terminateCallback = (session: WiredTigerSession) => void;

export declare class CustomExtractor {
  constructor(extract: extractCallback, customize?: customizeCallback<CustomExtractor>, terminate?: terminateCallback);
}

export declare class CustomCollator {
  constructor(compare: compareCallback, customize?: customizeCallback<CustomCollator>, terminate?: terminateCallback);
}

export declare class WiredTigerDB {
  open(directory: string | null, config: string): void;
  close(): void;
  openSession(config?: string): WiredTigerSession;
  configureMethod(method: string, uriPrefix: string, config: string, type: string, check: string): void;
  addExtractor(name: string, extractor: CustomExtractor, config: string | null): void;
  addCollator(name: string, collator: CustomCollator, config: string | null): void;
}

export declare class WiredTigerCursor {
  next(): boolean;
  prev(): boolean;
  getKey(formatPattern?: string): any[];
  getValue(formatPattern?: string): any[];
  setKey(...args: any[]): void;
  setValue(...args: any[]): void;
  close(): void;
  insert(): void;
  update(): void;
  remove(): void;
  search(): boolean;
  searchNear(): number;

  bound(config: string): void;
  equals(cursor: WiredTigerCursor): boolean;
  compare(cursor: WiredTigerCursor): number;
  get valueFormat(): string;
  get keyFormat(): string;
  get session(): WiredTigerSession;
}

export type FlatFindOptions = {
  keyFormat?: string,
  valueFormat?: string,
  columns?: string
};

export declare class WiredTigerTable {
  constructor(db: WiredTigerDB, tableName: string, tableConfig: string);
  find<Key extends any[], Value extends any[]>(conditions?: QueryCondition<Value>[], options?: FlatFindOptions): FindCursor<Key, Value>;
  insertMany(documents: [any[], any[]][]): void;
  createIndex(indexName: string, config: string): void;
  deleteMany(conditions?: QueryCondition<any[]>[]): number;
  updateMany(conditions: QueryCondition<any[]>[], newValues: any[]): number;
}

export declare class WiredTigerMapTable extends WiredTigerTable {
  constructor(db: WiredTigerDB, tableName: string);
  get(key: string): string;
  set(key: string, value: string): void;
  del(key: string): void;
  list(keyPrefix?: string, limit?: number): string[];
}
