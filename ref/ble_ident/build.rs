use std::env;
use std::path::PathBuf;

fn main() {
    let manifest = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    let out = PathBuf::from(env::var("OUT_DIR").unwrap());

    compile("ui/ident.slint", &manifest, &out, "IDENT_UI");
    compile("ui/decode.slint", &manifest, &out, "DECODE_UI");
    println!(
        "cargo:rerun-if-changed={}",
        manifest.join("ui/theme.slint").display()
    );
}

fn compile(relative: &str, manifest: &std::path::Path, out: &std::path::Path, env_key: &str) {
    let input = manifest.join(relative);
    let output = out.join(
        PathBuf::from(relative)
            .file_stem()
            .unwrap()
            .to_string_lossy()
            .to_string()
            + "_ui.rs",
    );
    slint_build::compile_with_output_path(
        &input,
        &output,
        slint_build::CompilerConfiguration::new(),
    )
    .unwrap();
    println!("cargo:rerun-if-changed={}", input.display());
    println!("cargo:rustc-env={}={}", env_key, output.display());
}
