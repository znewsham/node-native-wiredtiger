extern crate napi_build;

fn main() {
  napi_build::setup();
  println!("cargo:rustc-link-search=native=/usr/local/lib/");
  println!("cargo:rustc-link-lib=dylib=wiredtiger");
  println!("cargo:rustc-link-lib=static=zstd");
  println!("cargo:rustc-link-lib=static=z");
  println!("cargo:rustc-link-lib=static=snappy");
  println!("cargo:rustc-link-lib=static=lz4");
  println!("cargo:rustc-link-lib=static=ffcall");
}
