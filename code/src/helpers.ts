
export function configToString(config: object | string[]): string {
  if (Array.isArray(config)) {
    return config.join(",");
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
