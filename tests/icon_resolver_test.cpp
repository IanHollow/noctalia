#include "system/icon_resolver.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <print>
#include <string>
#include <unistd.h>

namespace fs = std::filesystem;

namespace {

  bool expect(bool condition, const char* message) {
    if (!condition) {
      std::println(stderr, "icon_resolver_test: {}", message);
    }
    return condition;
  }

} // namespace

int main() {
  char tempDir[] = "/tmp/noctalia-icon-resolver-test-XXXXXX";
  if (mkdtemp(tempDir) == nullptr) {
    std::perror("mkdtemp");
    return 1;
  }

  const fs::path root(tempDir);
  const fs::path iconThemeRoot = root / "icons/hicolor";
  const fs::path iconDir = iconThemeRoot / "scalable/apps";
  const fs::path app16Dir = iconThemeRoot / "16x16/apps";
  const fs::path bitmapIconDir = iconThemeRoot / "48x48/apps";
  const fs::path status16Dir = iconThemeRoot / "16x16/status";
  const fs::path status24Dir = iconThemeRoot / "24x24/status";
  const fs::path deniedDataHome = root / "denied";
  const fs::path deniedIcon = deniedDataHome / "private-icon.svg";
  fs::create_directories(iconDir);
  fs::create_directories(app16Dir);
  fs::create_directories(bitmapIconDir);
  fs::create_directories(status16Dir);
  fs::create_directories(status24Dir);
  const std::string indexContents = "[Icon Theme]\n"
         "Directories = 48x48/apps, scalable/apps, 16x16/apps, 16x16/status, 24x24/status\n"
         "Inherits = noctalia-bare-test\n"
         "[48x48/apps]\n"
         "Size = 48\n"
         "Type = Fixed\n"
         "[scalable/apps]\n"
         "Size = 64\n"
         "Type = Scalable\n"
         "MaxSize = 128\n"
         "[16x16/apps]\n"
         "Size = 16\n"
         "Type = Fixed\n"
         "[16x16/status]\n"
         "Size = 16\n"
         "Type = Fixed\n"
         "[24x24/status]\n"
         "Size = 24\n"
         "Type = Fixed\n";
  std::ofstream(iconThemeRoot / "index.theme") << indexContents;
  const fs::path status16 = status16Dir / "test-status-symbolic.svg";
  const fs::path status24 = status24Dir / "test-status-symbolic.svg";
  const fs::path ordinaryIcon = iconDir / "ordinary-icon.svg";
  const fs::path symbolicApp16 = app16Dir / "test-app-symbolic.svg";
  const fs::path symbolicAppScalable = iconDir / "test-app-symbolic.svg";
  const fs::path bitmapStatus = bitmapIconDir / "bitmap-status.png";
  std::ofstream(status16) << "<svg/>";
  std::ofstream(status24) << "<svg/>";
  std::ofstream(ordinaryIcon) << "<svg/>";
  std::ofstream(symbolicApp16) << "<svg/>";
  std::ofstream(symbolicAppScalable) << "<svg/>";
  std::ofstream(bitmapStatus) << "png";

  // An inherited theme with no index.theme exercises the fallback search paths
  // (theme root and 512x512/apps) that only apply to index-less themes.
  const fs::path bareThemeRoot = root / "icons/noctalia-bare-test";
  const fs::path bareBitmapDir = bareThemeRoot / "512x512/apps";
  fs::create_directories(bareBitmapDir);
  const fs::path bareSizedIcon = bareBitmapDir / "bare-sized-icon.png";
  const fs::path bareRootIcon = bareThemeRoot / "bare-root-icon.png";
  std::ofstream(bareSizedIcon) << "png";
  std::ofstream(bareRootIcon) << "png";
  const fs::path themedBitmap = bitmapIconDir / "theme-priority.png";
  std::ofstream(themedBitmap) << "png";
  std::ofstream(bareThemeRoot / "theme-priority.svg") << "<svg/>";
  std::ofstream(bitmapIconDir / "bitmap-priority.png") << "png";
  std::ofstream(bareBitmapDir / "bitmap-priority.png") << "png";
  const fs::path extraData = root / "extra";
  const fs::path extraIcons = extraData / "icons/hicolor/scalable/apps";
  fs::create_directories(extraIcons);
  std::ofstream(extraIcons / "later-root.svg") << "<svg/>";
  std::ofstream(bareThemeRoot / "later-root.svg") << "<svg/>";
  fs::create_directories(deniedDataHome);
  std::ofstream(deniedIcon) << "<svg/>";
  fs::permissions(deniedDataHome, fs::perms::none);
  setenv("HOME", tempDir, 1);
  setenv("XDG_DATA_HOME", deniedDataHome.c_str(), 1);
  setenv("XDG_DATA_DIRS", (root.string() + ":" + extraData.string()).c_str(), 1);

  bool ok = true;

  std::array<int, 2> logPipe{-1, -1};
  if (!expect(::pipe(logPipe.data()) == 0, "failed to create stderr capture pipe")) {
    return 1;
  }
  std::fflush(stderr);
  const int savedStderr = ::dup(STDERR_FILENO);
  if (!expect(savedStderr >= 0 && ::dup2(logPipe[1], STDERR_FILENO) >= 0, "failed to redirect stderr")) {
    ::close(logPipe[0]);
    ::close(logPipe[1]);
    if (savedStderr >= 0) {
      ::close(savedStderr);
    }
    return 1;
  }

  IconResolver resolver(true);
  const auto initialGeneration = IconResolver::themeGeneration();
  std::ofstream(iconThemeRoot / "index.theme") << indexContents;
  ok = expect(!IconResolver::checkThemePlanChanged(), "an equivalent theme metadata rewrite should be ignored") && ok;
  ok = expect(
           IconResolver::themeGeneration() == initialGeneration,
           "an equivalent theme metadata rewrite should preserve the theme generation"
       )
      && ok;
  std::fflush(stderr);
  (void)::dup2(savedStderr, STDERR_FILENO);
  ::close(savedStderr);
  ::close(logPipe[1]);

  std::string startupLogs;
  std::array<char, 1024> logBuffer{};
  ssize_t count = 0;
  while ((count = ::read(logPipe[0], logBuffer.data(), logBuffer.size())) > 0) {
    startupLogs.append(logBuffer.data(), static_cast<std::size_t>(count));
  }
  ::close(logPipe[0]);

  ok = expect(
           resolver.resolveStatusVector("test-status-symbolic", 16) == status16.string(),
           "status vector should use the matching 16px theme asset"
       )
      && ok;
  ok = expect(
           resolver.resolveStatusVector("test-status-symbolic", 24) == status24.string(),
           "status vector should use the matching 24px theme asset"
       )
      && ok;
  ok = expect(
           resolver.resolveStatusVector("ordinary-icon", 16).empty()
               && resolver.resolve("ordinary-icon", 32) == ordinaryIcon.string(),
           "ordinary application icons should keep normal resolution"
       )
      && ok;
  ok = expect(
           resolver.resolveStatusVector("test-app-symbolic", 16).empty()
               && resolver.resolve("test-app-symbolic", 32) == symbolicAppScalable.string(),
           "symbolic application icons should keep normal resolution"
       )
      && ok;
  ok = expect(
           resolver.resolveStatusVector("bitmap-status", 16).empty()
               && resolver.resolve("bitmap-status", 32) == bitmapStatus.string(),
           "bitmap status icons should keep the larger raster request"
       )
      && ok;

  const bool permissionsEnforced = (geteuid() != 0);
  if (permissionsEnforced) {
    ok = expect(startupLogs.contains(deniedDataHome.string()), "an inaccessible icon directory should be logged") && ok;
    ok = expect(
             resolver.resolve(deniedIcon.string(), 32).empty(),
             "an icon beneath an inaccessible directory should not terminate resolution"
         )
        && ok;
    fs::permissions(deniedDataHome, fs::perms::owner_all);
  }
  ok =
      expect(!startupLogs.contains((root / ".icons/hicolor").string()), "a missing icon directory should not be logged")
      && ok;
  ok = expect(
           resolver.resolve(deniedIcon.string(), 32) == deniedIcon.string(),
           "an absolute icon denied earlier should resolve once it is readable"
       )
      && ok;

  const fs::path invalidatedIcon = iconDir / "invalidated-icon.svg";
  ok = expect(resolver.resolve("invalidated-icon", 32).empty(), "initial missing icon should not resolve") && ok;
  std::ofstream(invalidatedIcon) << "<svg/>";
  ok = expect(resolver.resolve("invalidated-icon", 32).empty(), "named icon miss should be cached") && ok;
  resolver.invalidateMissingCache();
  ok = expect(
           resolver.resolve("invalidated-icon", 32) == invalidatedIcon.string(),
           "invalidating misses should discover a newly created icon"
       )
      && ok;

  const fs::path polledIcon = iconDir / "polled-icon.svg";
  ok = expect(resolver.resolve("polled-icon", 32).empty(), "second initial icon miss should be cached") && ok;
  std::ofstream(polledIcon) << "<svg/>";
  IconResolver::invalidateThemeContent();
  ok = expect(
           resolver.resolve("polled-icon", 32) == polledIcon.string(),
           "icon content invalidation should clear cached misses"
       )
      && ok;

  const fs::path absoluteIcon = root / "absolute-icon.svg";
  ok = expect(resolver.resolve(absoluteIcon.string(), 32).empty(), "missing absolute icon should not resolve") && ok;
  std::ofstream(absoluteIcon) << "<svg/>";
  ok = expect(
           resolver.resolve(absoluteIcon.string(), 32) == absoluteIcon.string(),
           "absolute icon misses should not be cached"
       )
      && ok;
  ok = expect(
           resolver.resolve(absoluteIcon.string(), 32) == absoluteIcon.string(),
           "absolute icon should be cached while present"
       )
      && ok;
  std::error_code ec;
  fs::remove(absoluteIcon, ec);
  ok =
      expect(resolver.resolve(absoluteIcon.string(), 32).empty(), "deleted absolute icon should not stay cached") && ok;
  std::ofstream(absoluteIcon) << "<svg/>";
  ok = expect(
           resolver.resolve(absoluteIcon.string(), 32) == absoluteIcon.string(),
           "recreated absolute icon should resolve after cache eviction"
       )
      && ok;

  ok = expect(
           resolver.resolve("bare-sized-icon", 32) == bareSizedIcon.string(),
           "index-less inherited theme should resolve icons under 512x512/apps"
       )
      && ok;
  ok = expect(
           resolver.resolve("bare-root-icon", 32) == bareRootIcon.string(),
           "index-less inherited theme should resolve icons at the theme root"
       )
      && ok;

  for (const int size : {0, 32, 64, 128}) {
    ok = expect(
             resolver.resolve("bitmap-priority", size) == (bitmapIconDir / "bitmap-priority.png").string(),
             "current-theme bitmap must precede a better-sized inherited bitmap"
         )
        && ok;
    ok = expect(
             resolver.resolve("theme-priority", size) == themedBitmap.string(),
             "current-theme bitmap must precede an inherited vector icon"
         )
        && ok;
    ok = expect(
             resolver.resolve("later-root", size) == (extraIcons / "later-root.svg").string(),
             "all roots of the current theme must precede inherited themes"
         )
        && ok;
  }

  fs::remove_all(root, ec);
  return ok ? 0 : 1;
}
