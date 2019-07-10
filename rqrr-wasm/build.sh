#!/bin/sh

set -ex

# Build the `hello_world.wasm` file using Cargo/rustc
cd rust-src
cargo build --target wasm32-unknown-unknown --release

cd ..
mkdir -p docs
wasm-bindgen rust-src/target/wasm32-unknown-unknown/release/qr_rust.wasm --out-dir ./docs --no-modules --no-typescript

wasm-opt -Os docs/qr_rust_bg.wasm -o docs/qr_rust.wasm

gzip -c docs/qr_rust.wasm > docs/qr_rust.wasm.gz
gzip -c docs/qr_rust.js > docs/qr_rust.js.gz
