#!/usr/bin/env bash
# Build polyir-lang-<version>.vsix without Node: a .vsix is a zip of the extension files plus two manifests
# (`vsce package` works too when it is installed). Install with: code --install-extension polyir-lang-<version>.vsix
set -euo pipefail
cd "$(dirname "$0")"

version="$(sed -n 's/.*"version": "\([^"]*\)".*/\1/p' package.json | head -1)"
publisher="$(sed -n 's/.*"publisher": "\([^"]*\)".*/\1/p' package.json | head -1)"
out="polyir-lang-${version}.vsix"
stage="$(mktemp -d)"
trap 'rm -rf "$stage"' EXIT

mkdir -p "$stage/extension"
cp package.json extension.js run.js preview.js README.md ./*language-configuration.json "$stage/extension/"
cp -R syntaxes "$stage/extension/"

cat > "$stage/[Content_Types].xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/ContentTypes">
  <Default Extension=".json" ContentType="application/json"/>
  <Default Extension=".js" ContentType="application/javascript"/>
  <Default Extension=".md" ContentType="text/markdown"/>
  <Default Extension=".vsixmanifest" ContentType="text/xml"/>
  <Default Extension="" ContentType="text/plain"/>
</Types>
XML

cat > "$stage/extension.vsixmanifest" <<XML
<?xml version="1.0" encoding="utf-8"?>
<PackageManifest Version="2.0.0" xmlns="http://schemas.microsoft.com/developer/vsx-schema/2011">
  <Metadata>
    <Identity Language="en-US" Id="polyir-lang" Version="${version}" Publisher="${publisher}"/>
    <DisplayName>polyir Language</DisplayName>
    <Description xml:space="preserve">polyir (.poly): highlighting, run on any target, diagnostics, Polyfile projects and a live LLVM IR preview.</Description>
    <Categories>Programming Languages</Categories>
    <GalleryFlags>Public</GalleryFlags>
  </Metadata>
  <Installation><InstallationTarget Id="Microsoft.VisualStudio.Code" Version="^1.75.0"/></Installation>
  <Dependencies/>
  <Assets>
    <Asset Type="Microsoft.VisualStudio.Code.Manifest" Path="extension/package.json" Addressable="true"/>
    <Asset Type="Microsoft.VisualStudio.Services.Content.Details" Path="extension/README.md" Addressable="true"/>
  </Assets>
</PackageManifest>
XML

rm -f "$out"
(cd "$stage" && zip -q -r "$OLDPWD/$out" '[Content_Types].xml' extension.vsixmanifest extension)
echo "built $out"
