{
  lib,
  stdenv,
  gnumake,
  libiconv,
}:

stdenv.mkDerivation {
  pname = "epubworm";
  version = "0-unstable-2026-09-22";

  src = lib.fileset.toSource {
    root = ../.;
    fileset = lib.fileset.unions [
      ../Makefile
      ../LICENSE
      ../THIRD_PARTY_LICENSES
      ../completions
      ../fixtures
      ../src
      ../vendor
    ];
  };

  nativeBuildInputs = [ gnumake ];
  buildInputs = lib.optionals stdenv.hostPlatform.isDarwin [ libiconv ];

  makeFlags = [ "CXX=${stdenv.cc.targetPrefix}c++" ];

  doCheck = true;
  preCheck = ''
    export LANG=${if stdenv.hostPlatform.isDarwin then "en_US.UTF-8" else "C.UTF-8"}
  '';

  installFlags = [ "PREFIX=$(out)" ];

  meta = {
    description = "Fast, image-capable terminal EPUB reader";
    homepage = "https://github.com/xinghao-wu/epubworm";
    license = with lib.licenses; [
      mit
      unlicense
      zlib
    ];
    mainProgram = "epubworm";
    platforms = lib.platforms.unix;
  };
}
