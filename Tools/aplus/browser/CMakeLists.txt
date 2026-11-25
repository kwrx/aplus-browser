add_executable(aplus-browser
    BrowserWindow.cpp
    BrowserUI.cpp
    Delegates.cpp
    Draw.cpp
    Glyphs.cpp
    Pages.cpp
    Tab.cpp
    TextField.cpp
    main.cpp
)

set_target_properties(aplus-browser PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

target_include_directories(aplus-browser PRIVATE
    "${WEBKITLEGACY_DIR}/aplus/API"
    "${WEBCORE_DIR}/platform/aplus"
    "${Cairo_INCLUDE_DIR}"
    "${APLUS_SDK_DIR}/include"
)

add_dependencies(aplus-browser WebKitLegacy)

target_link_directories(aplus-browser PRIVATE ${APLUS_SDK_DIR}/lib)

target_link_libraries(aplus-browser PRIVATE
    -static
    -Wl,--start-group
    WebKitLegacy
    WebCore
    PAL
    JavaScriptCore
    WTF
    $<TARGET_NAME_IF_EXISTS:bmalloc>
    "${Cairo_LIBRARY}"
    ${APLUS_SDK_DIR}/lib/libpsl.a
    ${APLUS_SDK_DIR}/lib/libxslt.a
    ${APLUS_SDK_DIR}/lib/libxml2.a
    ${APLUS_SDK_DIR}/lib/libicui18n.a
    ${APLUS_SDK_DIR}/lib/libicuuc.a
    ${APLUS_SDK_DIR}/lib/libicudata.a
    -lui -lulog -laudio
    -lavformat -lavcodec -lswresample -lswscale -lavutil
    -lcurl -lssl -lcrypto
    -lpixman-1 -lharfbuzz-icu -lharfbuzz -lfontconfig -lexpat -lfreetype -lbz2
    -lwebp -lwebpdemux -ljpeg -lpng16 -lsqlite3 -lz -llzma -lzstd
    -lstdc++ -lpthread -latomic -lm
    -Wl,--end-group
)

install(TARGETS aplus-browser RUNTIME DESTINATION bin)
install(FILES data/aplus-browser.desktop DESTINATION share/applications)
install(DIRECTORY data/icons DESTINATION share)
