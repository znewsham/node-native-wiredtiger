import module from 'node:module';

const importRequire = module.createRequire(import.meta.url);

const native = importRequire('../../build/Debug/WiredTiger');
export const {
  WiredTigerDB,
  Map,
  WiredTigerTable,
  WiredTigerMapTable,
  CustomExtractor,
  CustomCollator,
  wiredTigerStructUnpack
} = native;
