#!/usr/bin/env node

import esbuild from "esbuild";

const watch = process.argv.includes("--watch");

const frontendConfig = {
    entryPoints: {
        app: "src/frontend/index.ts",
    },
    bundle: true,
    sourcemap: true,
    outdir: "html/build/",
    loader: {
        ".ttf": "dataurl",
        ".woff": "dataurl",
        ".woff2": "dataurl",
        ".eot": "dataurl",
        ".html": "text",
        ".svg": "text",
        ".css": "css",
    },
    logLevel: "info",
    minify: !watch,
};

const serverConfig = {
    entryPoints: {
        server: "src/server.ts",
    },
    bundle: true,
    sourcemap: true,
    platform: "node",
    format: "cjs",
    external: ["fsevents"],
    outdir: "build/",
    outExtension: { ".js": ".cjs" },
    logLevel: "info",
    minify: !watch,
};

if (!watch) {
    console.log("Building frontend and server");
    await Promise.all([
        esbuild.build(frontendConfig),
        esbuild.build(serverConfig)
    ]);
} else {
    console.log("Watching frontend and server");
    const [frontendCtx, serverCtx] = await Promise.all([
        esbuild.context(frontendConfig),
        esbuild.context(serverConfig)
    ]);
    await Promise.all([
        frontendCtx.watch(),
        serverCtx.watch()
    ]);
}