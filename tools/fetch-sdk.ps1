$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$sdk = Join-Path $root 'vendor/vst3sdk'
$revision = '3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96'
if (Test-Path -LiteralPath $sdk) {
    $head = & git -C $sdk rev-parse HEAD
    if ($LASTEXITCODE -ne 0 -or $head -ne $revision) {
        throw 'SDK directory already exists at a different or unknown revision. Preserve it and choose a fresh checkout.'
    }
} else {
    & git clone --branch v3.8.1_build_84 --depth 1 https://github.com/steinbergmedia/vst3sdk.git $sdk
    if ($LASTEXITCODE -ne 0) { throw 'SDK clone failed.' }
    $head = & git -C $sdk rev-parse HEAD
    if ($LASTEXITCODE -ne 0 -or $head -ne $revision) { throw 'Unexpected SDK revision.' }
}
& git -C $sdk submodule update --init --depth 1 base cmake pluginterfaces public.sdk
if ($LASTEXITCODE -ne 0) { throw 'SDK submodule setup failed.' }
Write-Output "VST3 SDK ready at $revision"
