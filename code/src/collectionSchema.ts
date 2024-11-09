type ColumnFormatTypesWithoutPrefix = 'x' | 'b' | 'B' | 'h' | 'H' | 'i' | 'I' | 'l' | 'L' | 'q' | 'Q' | 'r' | 's' | 'S' | 't' | 'u';

type FakeColumnFormats = 'T' | 'z' | 'd';
type ColumnFormatTypesAllowedPrefixes = 's' | 'S' | 't' | 'u';
// TODO `${number}t` is incorrect - it's 0-8, but that makes the other types hard
type ColumnFormatTypes = ColumnFormatTypesWithoutPrefix | `${number}s`| `${number}S` | `${number}t` | `${number}u`;
export type RawCollectionConfiguration = {
  colgroups?: { name: string, columns: string[] }[],
  columns?: string[],
  key_format?: ColumnFormatTypes,
  value_format?: ColumnFormatTypes[],
  prefix_compression?: boolean,
  prefix_compression_min?: 0 | 1 | 2 | 3 | 4,
};

export enum SupportedTypes {
  // these are all pretty much straight maps (string maps to both S and s)
  string,
  int,
  uint,
  long,
  ulong,
  half,
  uhalf,
  byte,
  ubyte,

  // maps to t
  bitfield,

  // arbitrary binary data - will map to a u/Buffer
  binary,

  // could equally map to t or ubyte
  boolean,

  // BSON formatted binary data - will map to a u/BSON
  bson,

  // maps to binary (sortable)
  bigint,

  // maps to binary (sortable)
  double
};

type SchemaEntry<
  AT extends SupportedTypes = SupportedTypes,
  CF extends ColumnFormatTypes = ColumnFormatTypes,
  BT extends AnySchemaEntry | SubSchema | undefined = undefined
> = {
  columnFormat: CF,
  actualType: AT,
  // the length or the definition of the field (e.g., bson)
  bsonType?: BT,
  readFormat?: FakeColumnFormats
}

type AnySchemaEntry = SchemaEntry<SupportedTypes, ColumnFormatTypes, SchemaEntry>
  | SchemaEntry<SupportedTypes, ColumnFormatTypes, SubSchema>
  | SchemaEntry<SupportedTypes, ColumnFormatTypes, undefined>;


type MaybePrefixed<Length extends number | undefined, Type extends ColumnFormatTypesAllowedPrefixes> = Length extends number
  ? `${Length}${Type}`
  : Type;

function maybePrefix<Length extends number | undefined, Format extends ColumnFormatTypesAllowedPrefixes, Type extends SupportedTypes>(
  length: Length, format: Format, type: Type)
  : SchemaEntry<Type, MaybePrefixed<Length, Format>> {
  if (length !== undefined) {
    if (typeof length === 'number') {
      return {
        columnFormat: <MaybePrefixed<Length, Format>>`${length}${format}`,
        actualType: type
      };
    }
  }
  return {
    columnFormat: <MaybePrefixed<Length, Format>>format,
    actualType: type
  };
}

function schemaEntry<CF extends ColumnFormatTypes, ST extends SupportedTypes>(x: { actualType: ST, columnFormat: CF, readFormat?: FakeColumnFormats }): SchemaEntry<ST, CF> {
  return x;
}
// type SchemaType = "String" | "CharArray" | "Int" | ""

export const schema = {
  String<Length extends number | undefined>(length?: Length) {
    return maybePrefix(length, 'S', SupportedTypes.string);
  },
  CharArray<Length extends number>(length: Length) {
    return maybePrefix(length, 's', SupportedTypes.string);
  },
  Int() {
    return schemaEntry({
      columnFormat: 'i',
      actualType: SupportedTypes.int,
    });
  },
  UInt() {
    return schemaEntry({
      columnFormat: 'I',
      actualType: SupportedTypes.uint
    });
  },
  Half() {
    return schemaEntry({
      columnFormat: 'h',
      actualType: SupportedTypes.half
    });
  },
  UHalf() {
    return schemaEntry({
      columnFormat: 'H',
      actualType: SupportedTypes.uhalf
    });
  },
  Byte() {
    return schemaEntry({
      columnFormat: 'b',
      actualType: SupportedTypes.byte
    });
  },
  UByte() {
    return schemaEntry({
      columnFormat: 'B',
      actualType: SupportedTypes.ubyte
    });
  },
  Long() {
    return schemaEntry({
      columnFormat: 'q',
      actualType: SupportedTypes.long
    });
  },
  BigInt() {
    return schemaEntry({
      columnFormat: 'u',
      actualType: SupportedTypes.bigint,
      readFormat: 'z'
    });
  },
  ULong() {
    return schemaEntry({
      columnFormat: 'Q',
      actualType: SupportedTypes.ulong
    });
  },
  Double() {
    return schemaEntry({
      columnFormat: 'u',
      actualType: SupportedTypes.double,
      readFormat: 'd'
    });
  },
  Binary<Length extends number | undefined>(length?: Length) {
    return maybePrefix(length, 'u', SupportedTypes.binary);
  },
  BitField<Length extends (1 | 2 | 3 | 4 | 5 | 6 | 7 | 8)>(length: Length) {
    return maybePrefix(length, 't', SupportedTypes.bitfield);
  },
  Boolean() {
    return schemaEntry({
      columnFormat: 't',
      actualType: SupportedTypes.boolean,
      readFormat: 'T'
    });
  },
  BSON<BT extends AnySchemaEntry | SubSchema>(value: BT): SchemaEntry<SupportedTypes.bson, 'u', BT> {
    return {
      columnFormat: "u",
      actualType: SupportedTypes.bson,
      bsonType: value
    };
  }
}

export type UpdateModifier<ESchema extends ExternalSchema> = {
  $set: Partial<ESchema>
}

// no idea why 4 works and 5 doesn't - but it's fine, I'd expect to only really need 1 depth
type MagicDepth = 4;

type TypeForSupportedType<
  ST extends SupportedTypes,
  SS extends AnySchemaEntry | SubSchema | undefined = SchemaEntry | SubSchema | undefined,
  Depth extends number[] = []
> = ST extends SupportedTypes.string
  ? string
  : ST extends SupportedTypes.binary
    ? Buffer
    : ST extends SupportedTypes.bigint
      ? bigint
      : ST extends SupportedTypes.double
        ? number
        : ST extends (SupportedTypes.boolean)
          ? boolean
          : ST extends (SupportedTypes.ulong | SupportedTypes.long)
            ? number | bigint
            : ST extends (SupportedTypes.uint | SupportedTypes.int | SupportedTypes.uhalf | SupportedTypes.half | SupportedTypes.ubyte | SupportedTypes.byte | SupportedTypes.bitfield)
              ? number
              : ST extends SupportedTypes.bson
                ? SS extends AnySchemaEntry
                  ? Depth["length"] extends MagicDepth //
                    ? any
                    : TypeForSupportedType<SS["actualType"], SS["bsonType"], [1, ...Depth]>
                  : SS extends SubSchema
                    ? Depth["length"] extends MagicDepth
                      ? any
                      :ExternalSchemaInternalSchema<SS, [1, ...Depth]>
                    : never
              : never;


export const RemainingSymbol = Symbol("Remaining");
type InternalSchemaWithRemainingSchedule = {
  [k in typeof RemainingSymbol]: AnySchemaEntry
};


export type ExternalSchemaInternalSchema<S extends InternalSchema, Depth extends number[] = []> = {
  [k in keyof S & string]: S[k] extends AnySchemaEntry ? TypeForSupportedType<S[k]["actualType"], S[k]["bsonType"]> : never
} & (S extends InternalSchemaWithRemainingSchedule
  ? S[typeof RemainingSymbol] extends AnySchemaEntry
    ? TypeForSupportedType<S[typeof RemainingSymbol]["actualType"], S[typeof RemainingSymbol]["bsonType"], [1, ...Depth]>
    : {}
  : {});



type SubSchema = {
  [k in string]: AnySchemaEntry
};
export type InternalSchema = SubSchema & { [k in typeof RemainingSymbol]?: AnySchemaEntry };

// TODO: this isn't right - this is being merged with the true schema somehow
export type ExternalSchema = {
  [k in string | symbol]: string | number | boolean | Buffer | ExternalSchema | bigint
};

// @ts-expect-error can't get the "Columns" thing to work properly, conditional logic causes trouble - for now it works externally and that's enough
export type ProjectedTSchemaOf<ESchema extends ExternalSchema, Columns extends (keyof ESchema & string) | undefined> = Pick<ESchema, Columns>;

export type SchemaConfiguration<T extends InternalSchema> = {
  config?: Omit<RawCollectionConfiguration, "columns" | "key_format" | "value_format" | "colgroups"> & { colgroups?: {name: string, columns: ((string & keyof Omit<T, "_id">) | "_remaining")[]}[]},
  schema: T
}

export type ColumnSpec = {
  name: string,
  columnFormat: ColumnFormatTypes,
  actualType: SupportedTypes,
  readFormat?: FakeColumnFormats
};
