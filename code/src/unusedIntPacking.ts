
const NEG_MULTI_MARKER = 0x10;
const NEG_1BYTE_MARKER = 0x40;
const NEG_2BYTE_MARKER = 0x20;
const POS_1BYTE_MARKER = 0x80;
const POS_2BYTE_MARKER = 0xc0;
const POS_MULTI_MARKER = 0xe0;
const NEG_2BYTE_MIN = (-(1 << 13) + (-(1 << 6)));
const POS_1BYTE_MAX = ((1 << 6) - 1);
const POS_2BYTE_MAX = ((1 << 13) + POS_1BYTE_MAX);

const UINT64_ISH_MAX = BigInt("0xFFFFFFFFFFFFFFFF");


function unpackNegInt(buffer: Buffer) {
  let length = 2; //8 - (buffer[0] & 0xf);
  let position = 0;
  let x = BigInt(UINT64_ISH_MAX);
  for (x = BigInt(UINT64_ISH_MAX); length != 0; --length) {
    x = ((x << BigInt(8)) | BigInt(buffer[position++])) & UINT64_ISH_MAX;
  }
  return x;
}

function unpackPosInt(buffer: Buffer): bigint {
  let length = buffer[0] & 0x0f;
  let x: bigint = 0n;
  let i = 1;
  for (x = 0n; length != 0; --length) {
    x = (x << 8n) | BigInt(buffer[i++]);
  }
  return x;
}
const GET_BITS = (x: number, start: number, end = 0) => (((x) & ((1 << (start)) - 1)) >> (end));

function intFromUnsignedBuffer(buffer: Buffer) {
  const flags = buffer[0] & 0xf0;
  let ret;
  switch (flags) {
    case POS_1BYTE_MARKER:
    case POS_1BYTE_MARKER | 0x10:
    case POS_1BYTE_MARKER | 0x20:
    case POS_1BYTE_MARKER | 0x30:
      return GET_BITS(buffer[0], 6);
    case POS_2BYTE_MARKER:
    case POS_2BYTE_MARKER | 0x10:
      return ((GET_BITS(buffer[0], 5) << 8) | buffer[1]) + POS_1BYTE_MAX + 1;
      break;
    case POS_MULTI_MARKER:
      const big = unpackPosInt(buffer) + BigInt(POS_2BYTE_MAX) + BigInt(1);
      if (big < Number.MAX_SAFE_INTEGER) {
        return Number(big);
      }
      return big; // TODO - gonna have to deal with this
    default:
      throw new Error("Invalid");
  }
}

function numberFromBuffer(buffer: Buffer) {
  const flags = buffer[0] & 0xf0;
  switch (flags) {
    case NEG_MULTI_MARKER:
      return unpackNegInt(buffer);
    case NEG_2BYTE_MARKER:
    case NEG_2BYTE_MARKER | 0x10:
      return ((GET_BITS(buffer[0], 5) <<8) | buffer[1]) + NEG_2BYTE_MARKER;
    case NEG_1BYTE_MARKER:
    case NEG_1BYTE_MARKER | 0x10:
    case NEG_1BYTE_MARKER | 0x20:
    case NEG_1BYTE_MARKER | 0x30:
      return GET_BITS(buffer[0], 6, 0) + 1;
    default:
      return intFromUnsignedBuffer(buffer);
    }
}
