declare module '/wasm/player.js' {
  const createModule: (opts?: Record<string, unknown>) => Promise<any>;
  export default createModule;
}
