set(CTEST_PROJECT_NAME "heos2mqtt")
set(CTEST_NIGHTLY_START_TIME "01:00:00 UTC")
set(CTEST_SUBMIT_URL "https://my.cdash.org/submit.php?project=heos2mqtt")

set(CTEST_CUSTOM_COVERAGE_EXCLUDE
  ".*/external/.*"
  ".*/vcpkg_installed/.*"
)
