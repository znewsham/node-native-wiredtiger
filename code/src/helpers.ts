
export function configToString(config: string | object | string[]): string {
  if (typeof config === "string") {
    return config;
  }
  if (Array.isArray(config)) {
    // NOTE: array of arrays not supported
    return config.map(sC => typeof sC === "object" ? `${configToString(sC)}` : sC).join(",");
  }
  return Object.entries(config).filter(([key, value]) => value !== undefined).map(([key, value]) => {
    if (typeof value === "boolean") {
      return key;
    }
    else if (typeof value === "string" || typeof value === "number") {
      return `${key}=${value}`;
    }
    else if (Array.isArray(value)) {
      return `${key}=[${configToString(value)}]`
    }
    else if (typeof value === "object") {
      return `${key}={${configToString(value)}}`
    }
  }).join(",");
}
