import { WiredTigerDB as WiredTigerDBNative } from './getModule.js';
import { configToString } from './helpers.js';
import { CreateTypeAndName, VerboseOperations } from './types.js';

type SizeSuffix = 'MB' | 'GB' | 'TB';
type Size = `${number}${SizeSuffix}`;


type DBConfiguration = {
  block_cache?: {
    //TODO
  },
  cache_cursors?: boolean,
  cache_size?: Size,
  checkpoint?: {
    log_size: 0 | Size,
    wait: number
  },
  create?: boolean,
  exclusive?: boolean,
  in_memory?: boolean,
  readonly?: boolean,
  session_max?: number,
  statistics?: ("all" | "cache_walk" | "fast" | "none" | "clear" | "tree_walk")[],
  statistics_log?: {
    json?: boolean,
    on_close?: boolean,
    path?: string,
    sources?: CreateTypeAndName[],
    timestamp?: string,
    wait?: number
  },
  verbose?: VerboseOperations[]
}

export class WiredTigerDB extends WiredTigerDBNative {
  #config: DBConfiguration | string;
  #home: string | null;
  #opened?: boolean;

  constructor(home: string | null, config: DBConfiguration | string) {
    super();
    this.#home = home;
    this.#config = config;
  }

  get native() {
    return this;
  }

  get opened() {
    return this.#opened;
  }

  #getConfig() {
    if (typeof this.#config === "string") {
      return this.#config;
    }
    return configToString(this.#config);
  }

  open() {
    if (this.#opened) {
      return;
    }
    super.open(this.#home, this.#getConfig());
    this.#opened = true;
  }

  close() {
    this.#opened = false;
    super.close();
  }
}
