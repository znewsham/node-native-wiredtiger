import { Connection } from './getModule.js';
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

export class WiredTigerDB extends Connection {
  #opened?: boolean;

  constructor(home: string, config: DBConfiguration | string) {
    super(home, configToString(config));
    this.#opened = true;
  }

  get native() {
    return this;
  }

  get opened() {
    return this.#opened;
  }

  open() {
    if (this.#opened) {
      return;
    }
  }

  close() {
    this.#opened = false;
    super.close();
  }
}
