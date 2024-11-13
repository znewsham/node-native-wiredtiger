import { ExternalSchema, ColumnSpec, ExternalSchemaInternalSchema, InternalSchema, RawCollectionConfiguration, RemainingSymbol, SchemaConfiguration, SupportedTypes, ProjectedTSchemaOf, UpdateModifier } from "./collectionSchema.js";
import { WiredTigerDB } from "./db.js";
import { WiredTigerTable, WiredTigerSession } from "./getModule.js";
import { configToString } from "./helpers.js";
import { FindCursor } from "./findCursor.js";
import { CreateTypeAndName, FindOptions, FlatFindOptions, IndexColumnOptions, IndexCreationOptions, Operation, QueryCondition } from "./types.js";
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
      const columnDefinitions: ColumnSpec[] = Object.entries(config.schema).map(([name, spec], index) => {
        if (index === 0 && name != Collection.ID_NAME) {
          throw new Error(`${Collection.ID_NAME} must be the first item in the schema, not ${name}`);
        }
        return {
          actualType: spec?.actualType,
          columnFormat: spec?.columnFormat,
          readFormat: spec?.readFormat,
          name,
          // indexes start at 1 because of the _id
          columnIndex: index
        };
      });

      if (config.schema[RemainingSymbol]) {
        columnDefinitions.push({
          name: Collection.REMAINING_NAME,
          columnIndex: columnDefinitions.length,
          ...config.schema[RemainingSymbol]
        });

        // @ts-ignore TODO - I think remaining needs to be a special BSON type that is always an object
        Object.entries(config.schema[RemainingSymbol].bsonType).forEach(([name, spec], index) => {
          this.#remainingSpecs.push({
            name,
            actualType: spec?.actualType,
            columnFormat: spec?.columnFormat,
            readFormat: spec?.readFormat,
            columnIndex: index
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
        actualType: SupportedTypes.ulong,
        columnIndex: -1,
      };
      this.#valueDefinitions = columns.slice(1).map((name, i) => ({
        name,
        columnFormat: valueFormats[i],
        actualType: SupportedTypes.binary, // TODO: Map this? Maybe not - you use raw, you get raw :shrug:
        columnIndex: i
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

  createIndex(
    name: string,
    columns: IndexColumnOptions<Omit<ISchema, "_id">>[],
    options?: IndexCreationOptions
  ) {
    this.#ensureTable();
    const useCustomExtractorOrCollator = !!columns.find(k => typeof k !== "string" && (k.extractor || k.direction));
    // TODO: custom collator but not custom extractor?
    if (!useCustomExtractorOrCollator) {
      this._table.createIndex(name, `columns=[${columns.join(",")}]`);
    }

    const columnSet = new Set<string>(columns.flatMap(column => typeof column === "string" ? column : column.columns));
    let direction = 1;
    const directions = columns.map(column => typeof column === "string" ? 1 : column.direction || 1);
    if (Math.max(...directions) === Math.min(...directions)) {
      direction = directions[0];
    }
    else {
      console.log(directions);
      direction = 0; // compound per-column direction
    }
    const keyFormat = columns.map(column => {
      if (typeof column === "string") {
        return this.#columnDefinitionMap.get(column)?.columnFormat;
      }
      else if (column.columns.length == 1) {
        return this.#columnDefinitionMap.get(column.columns[0])?.columnFormat;
      }
      return "S";
    }).join("");
    const config = {
      ...options,
      // right now we only support multi key indexes on strings - so we can just default to "S" if it's an object
      // down the road, we'll need to detect what the key is
      key_format: keyFormat,
      extractor: "multiKey",
      collator: "compound",
      app_metadata: {
        table_value_format: this.#valueDefinitions.map(({ columnFormat }) => columnFormat).join(""),
        key_extract_format: this.#valueDefinitions.map(({ name, readFormat, columnFormat }) => columnSet.has(name) ? readFormat || columnFormat : 'x').join(""),
        key_format: keyFormat,
        // used to share extractors across indexes where possible
        index_id: `${Math.random()}`, // TODO: this should be set to the unique combo of table_value_format (maybe with padding replacing the keys we don't care about?) and the column specs

        direction,
        columns: columns.length,
        ...(Object.fromEntries(columns.map((column, i) => {
          if (typeof column === "string") {
            return [
              `column${i}`,
              { columns: [this.#columnDefinitionMap.get(column)?.columnIndex], direction: 1 }
            ];
          }
          return [
            `column${i}`,
            {
              columns: column.columns.map(c => this.#columnDefinitionMap.get(c)?.columnIndex),
              ngrams: column.ngrams,
              direction: column.direction,
              extractor: column.extractor
            }
          ];
        })))
      }
    };
    this._table.createIndex(name, configToString(config));
  }

  findOne(): ESchema | undefined {
    this.#ensureTable();
    return this.find<FindOptions<keyof ESchema & string>, ESchema>([]).nextBatch(1)[0];
  }

  deleteMany(conditions?: QueryCondition<any[]>[]): number {
    this.#ensureTable();
    return this._table.deleteMany(conditions);
  }

  updateMany(conditions: QueryCondition<any[]>[], modifier: UpdateModifier<Omit<ESchema, typeof Collection.ID_NAME>>): number {
    this.#ensureTable();
    // TODO: remaining
    const newValues = this.#valueDefinitions.map(({ name }) => {
      if (modifier.$set.hasOwnProperty(name as keyof ESchema)) {
        // @ts-expect-error
        return modifier.$set[name];
      }
      return undefined;
    });

    return this._table.updateMany(conditions, newValues);
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
        [keyValue],
        this.#getValuesForInsert(doc)
      ]
    ]);
  }

  insertMany(docs: ESchema[]) {
    this.#ensureTable();
    this._table.insertMany(docs.map((doc) => [
      [doc[this.#keyDefinition.name]],
      this.#getValuesForInsert(doc)
    ]));
  }

  get db() {
    return this.#db;
  }
}
