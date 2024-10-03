message(STATUS "Copying static webview content from `${SOURCE}` to `${DESTINATION}`")
file(
    COPY "${SOURCE}/"
    DESTINATION "${DESTINATION}"
    FILES_MATCHING
    PATTERN "**/*.html"
    PATTERN "**/*.css"
    PATTERN node_modules EXCLUDE
)