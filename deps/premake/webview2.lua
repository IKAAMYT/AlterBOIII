-- WebView2 SDK (Microsoft Edge / Chromium embedded webview)
-- The SDK is fetched as a NuGet package during CI into deps/webview2/.
-- Layout expected after extraction:
--   deps/webview2/build/native/include/WebView2.h
--   deps/webview2/build/native/include/WebView2EnvironmentOptions.h
--   deps/webview2/build/native/x64/WebView2LoaderStatic.lib
--
-- We link the *static* loader so there is no extra DLL to ship.

webview2 = {
  source = path.join(dependencies.basePath, "webview2"),
}

function webview2.import()
  -- Static loader: no WebView2Loader.dll needed at runtime.
  filter({ "platforms:x64" })
  libdirs({ path.join(webview2.source, "build/native/x64") })
  filter({})

  links({ "WebView2LoaderStatic.lib" })

  -- COM / WinRT helpers used by the host.
  links({ "version.lib" })

  webview2.includes()
end

function webview2.includes()
  includedirs({
    path.join(webview2.source, "build/native/include"),
  })

  defines({
    "ALTERBO3_HAS_WEBVIEW2=1",
  })
  filter({})
end

function webview2.project()
  -- Header-only from our perspective (prebuilt loader lib), no project to build.
end

-- NOTE: intentionally NOT added to the global `dependencies` table.
-- WebView2 is only needed by the `client` project, so it is imported
-- explicitly there via `webview2.import()` rather than auto-imported
-- into every project (common / tlsdll don't need it).

return webview2
