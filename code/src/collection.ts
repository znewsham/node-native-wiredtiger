import { ExternalSchema, ColumnSpec, ExternalSchemaInternalSchema, InternalSchema, RawCollectionConfiguration, RemainingSymbol, SchemaConfiguration, SupportedTypes, ProjectedTSchemaOf } from "./collectionSchema.js";
import { WiredTigerDB } from "./db.js";
import { WiredTigerTable, WiredTigerSession } from "./getModule.js";
import { configToString } from "./helpers.js";
import { FindCursor } from "./findCursor.js";
import { CreateTypeAndName, FindOptions, FlatFindOptions, Operation, QueryCondition } from "./types.js";
import { BSON } from "bson";



export class Collection
  <ISchema extends InternalSchema,
  ESchema extends ExternalSchema = ExternalSchemaInternalSchema<ISchema>
> /* hard to extend table as we need to parse the config */ {
  static ID_NAME = "_id";
  static REMAINING_NAME = "_remaining";
  static DEFAULT_COLUMN_NAME = "value";

  #table?: WiredTigerTable;
  #db: WiredTigerDB;
  #name: string;
  #created: boolean = false;
  #config: RawCollectionConfiguration | string | SchemaConfiguration<ISchema>;
  // @ts-expect-error assigned in extractConfig
  #configString: string;
  // @ts-expect-error assigned in extractConfig
  #valueDefinitions: ColumnSpec[];
  #columnDefinitionMap = new Map<string, ColumnSpec>();
  // @ts-expect-error assigned in extractConfig
  #keyDefinition: ColumnSpec;
  // @ts-expect-error assigned in extractConfig
  #columnGroups: { name: string, columns: string[] }[];

  #remainingSpecs: ColumnSpec[] = [];

  constructor(
    db: WiredTigerDB,
    name: string,
    config: RawCollectionConfiguration | SchemaConfiguration<ISchema>
  ) {
    this.#db = db;
    this.#config = config;
    if (!this.#db.opened) {
      throw new Error("Should have been opened");
    }
    this.#name = name;
    this.#extractConfig();
  }

  get _table() {
    return this.#table;
  }

  #extractConfig() {
    let configString: string;
    if (typeof this.#config === "string") {
      configString = this.#config;
    }
    else if ("schema" in this.#config) {
      const config: SchemaConfiguration<ISchema> = this.#config;
      const columnDefinitions: ColumnSpec[] = Object.entries(config.schema).map(([name, spec]) => {
        return {
          actualType: spec?.actualType,
          columnFormat: spec?.columnFormat,
          readFormat: spec?.readFormat,
          name
        };
      });

      if (config.schema[RemainingSymbol]) {
        columnDefinitions.push({
          name: Collection.REMAINING_NAME,
          ...config.schema[RemainingSymbol]
        });

        // @ts-ignore TODO - I think remaining needs to be a special BSON type that is always an object
        Object.entries(config.schema[RemainingSymbol].bsonType).forEach(([name, spec]) => {
          this.#remainingSpecs.push({
            name,
            actualType: spec?.actualType,
            columnFormat: spec?.columnFormat,
            readFormat: spec?.readFormat
          });
        });
      }
      this.#valueDefinitions = columnDefinitions.filter(({ name }) => name !== Collection.ID_NAME);
      const keyDefinition = columnDefinitions.find(({ name }) => name === Collection.ID_NAME);
      if (!keyDefinition) {
        throw new Error(`Missing ${Collection.ID_NAME} definition`);
      }
      this.#keyDefinition = keyDefinition;
      configString = configToString({
        ...config.config,
        value_format: this.#valueDefinitions.map(({ columnFormat }) => columnFormat).join(""),
        key_format: this.#keyDefinition.columnFormat,
        columns: `(${columnDefinitions.map(({ name }) => name).join(",")})`,
        ...(config.config?.colgroups ? { colgroups: `(${config.config.colgroups?.map(({ name }) => name).join(",")})` } : {})
      });
      this.#columnGroups = config.config?.colgroups || [];
    }
    else {
      const config: RawCollectionConfiguration = this.#config;
      const columns = this.#config.columns || [Collection.ID_NAME, Collection.DEFAULT_COLUMN_NAME];
      const valueFormats = config.value_format || ["u"];
      this.#keyDefinition = {
        name: Collection.ID_NAME,
        columnFormat: this.#config.key_format || "r",
        actualType: SupportedTypes.ulong
      };
      this.#valueDefinitions = columns.slice(1).map((name, i) => ({
        name,
        columnFormat: valueFormats[i],
        actualType: SupportedTypes.binary, // TODO: Map this? Maybe not - you use raw, you get raw :shrug:
      }));

      configString = configToString({
        ...this.#config,
        value_format: this.#config.value_format?.join(""),
        ...(columns ? { columns: `(${columns?.map((name) => name).join(",")})` } : {}) ,
        ...(this.#config.colgroups ? { colgroups: `(${this.#config.colgroups?.map(({ name }) => name).join(",")})` } : {})
      });
      this.#columnGroups = config.colgroups || [];
    }
    this.#valueDefinitions.forEach(def => this.#columnDefinitionMap.set(def.name, def));
    this.#columnDefinitionMap.set(Collection.ID_NAME, this.#keyDefinition);
    this.#configString = configString;
  }

  #ensureTable(): asserts this is { _table: WiredTigerTable } {
    if (this.#created) {
      return;
    }
    this.#created = true;
    let createdSession = false;
    // if (!session) {
    //   createdSession = true;
    //   session = this.#db.openSession();
    // }
    this.#table = new WiredTigerTable(this.#db.native, this.#name, this.#configString);
    // session.create(`table:${this.#name}`, this.#configString);
    // this.#columnGroups.forEach(cg => session.create(`colgroup:${this.#name}:${cg.name}`, `columns=(${cg.columns.join(",")})`));
    // if (createdSession) {
    //   session.close();
    // }
  }

  createIndex(name: string, columns: (keyof Omit<ISchema, "_id"> & string)[], extra: string = "") {
    this.#ensureTable();
    const columnStr = columns.length ? `columns=(${columns.join(",")})` : "";
    this._table.createIndex(name, `${columnStr}${extra}`);
  }

  findOne(): ESchema | undefined {
    this.#ensureTable();
    return this.find<FindOptions<keyof ESchema & string>, ESchema>([]).nextBatch(1)[0];
  }
  find<
    ESchemaFindOptions extends FindOptions<keyof ESchema & string> = FindOptions<keyof ESchema & string>,
    TSchema extends object = ProjectedTSchemaOf<ESchema, FlatArray<ESchemaFindOptions["columns"], 0> | "_id">,
  >(conditions?: QueryCondition<any[]>[], findOptions?: ESchemaFindOptions) {
    this.#ensureTable();

    // @ts-expect-error if we specify a column that doesn't exist we'll find it below
    const columnDefinitions: ColumnSpec[] = findOptions?.columns ? findOptions.columns.map(column => this.#columnDefinitionMap.get(column)) : this.#valueDefinitions;
    if (columnDefinitions.find(c => c === undefined)) {
      throw new Error("Invalid column");
    }
    const valueFormat = columnDefinitions.map(({ readFormat, columnFormat }) => readFormat || columnFormat).join('');

    const flatFindOptions: FlatFindOptions = {
      ...(findOptions?.valueFormat ? { valueFormat: findOptions.valueFormat } : { valueFormat }),
      ...(findOptions?.keyFormat && { keyFormat: findOptions.keyFormat }),
      ...(findOptions?.columns && { columns: `(${findOptions.columns.join(",")})` }),
    };
    const cursor = new FindCursor<TSchema>(this._table.find(conditions, flatFindOptions), columnDefinitions);
    return cursor;
  }

  #getValuesForInsert(doc: ESchema): any[] {
    return this.#valueDefinitions.map(({ name, actualType }) => {
      if (name === Collection.REMAINING_NAME) {
        return BSON.serialize(Object.fromEntries(this.#remainingSpecs.map(({ name }) => [name, doc[name]])));
      }
      else if (actualType === SupportedTypes.bson) {
        // annoyingly BSON can't serialize JUST a value - it doesn't expose the internals (e.g., serializeString). I may copy them out eventually
        return BSON.serialize({ _: doc[name] });
      }
      return doc[name]
    });
  }

  insertOne(doc: ESchema) {
    this.#ensureTable();
    const keyValue = doc[this.#keyDefinition.name];
    this._table.insertMany([
      [
        keyValue,
        this.#getValuesForInsert(doc)
      ]
    ]);
  }

  insertMany(docs: ESchema[]) {
    this.#ensureTable();
    this._table.insertMany(docs.map((doc) => [
      doc[this.#keyDefinition.name],
      this.#getValuesForInsert(doc)
    ]));
  }

  get db() {
    return this.#db;
  }
}
