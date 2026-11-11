cmake_minimum_required(VERSION 3.16)

project(RixoraAI VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Подключаем Qt модули, включая Concurrent для фонового синтеза
find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Network Multimedia Concurrent Svg)

qt_standard_project_setup(REQUIRES 6.5)

# QML-файлы лежат в подкаталоге qml/ — политика QTP0004 требует qmldir
# для каждого дополнительного каталога с .qml.
qt_policy(SET QTP0004 NEW)

# === НАСТРОЙКА SHERPA-ONNX ===
set(SHERPA_ONNX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/sherpa-onnx")

# Shared library with core memory components
add_library(RixoraCore STATIC
    constants.h
    apiconfigmanager.h
    apiconfigmanager.cpp
    geminiadapter.h
    geminiadapter.cpp
    geminiapi.h
    geminiapi.cpp
    geminiclient.h
    geminiclient.cpp
    geminiresponseparser.h
    geminiresponseparser.cpp
    memorycontextbuilder.h
    memorycontextbuilder.cpp
    speechapi.h
    speechapi.cpp
    bm25index.h
    bm25index.cpp
    memorymanager.h
    memorymanager.cpp
    chatmanager.h
    chatmanager.cpp
    longtermmemory.h
    longtermmemory.cpp
    audioengine.h
    audioengine.cpp
    networkmanager.h
    networkmanager.cpp
    texttokenizer.h
    texttokenizer.cpp
    memorylang.h
    memorylang.cpp
    chatutils.h
    chatutils.cpp
    memoryorchestrator.h
    memoryorchestrator.cpp
    messageactions.h
    messageactions.cpp
    chatpersistence.h
    chatpersistence.cpp
    longtermmemorypersistence.h
    longtermmemorypersistence.cpp
    memorysearch.h
    memorysearch.cpp
    longtermmemorycore.h
    longtermmemorycore.cpp
    memorytypes.h
    textutils.h
    textutils.cpp
    ttsengine.h
    ttsengine.cpp
    audioplayback.h
    audioplayback.cpp
    audiorecording.h
    audiorecording.cpp
)

target_include_directories(RixoraCore PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SHERPA_ONNX_DIR}/include
)
target_link_libraries(RixoraCore PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Multimedia
    Qt6::Concurrent
    Qt6::Test
)

target_link_directories(RixoraCore PRIVATE ${SHERPA_ONNX_DIR}/lib)

qt_add_executable(appRixoraAI
    main.cpp
)

qt_add_qml_module(appRixoraAI
    URI RixoraAI
    VERSION 1.0

    SOURCES
        # All sources are in RixoraCore library

    QML_FILES
        qml/LoginView.qml
        qml/ChatView.qml
        qml/ChatInputBar.qml
        qml/GlowBorder.qml
        qml/ChatHistoryPanel.qml
        qml/ChatScrollBar.qml
        qml/DeleteConfirmDialog.qml
        qml/MessageDeleteDialog.qml
        qml/MessageRewriteDialog.qml
        qml/MessageActionButton.qml
        qml/ChatDeleteMenu.qml
        qml/SettingsMenu.qml
        qml/LanguagePickerMenu.qml
        qml/PersonalizationMenu.qml
        qml/ChatStore.qml
        qml/ChatSession.qml
        qml/ChatChrome.qml
        qml/ChatScreen.qml
        qml/Main.qml
    qml/SidePanelButton.qml
    qml/translations.js

)

set_target_properties(appRixoraAI PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

# Используем modern CMake стилевые вызовы для подключаемой библиотеки
target_include_directories(appRixoraAI PRIVATE ${SHERPA_ONNX_DIR}/include)
target_link_directories(appRixoraAI PRIVATE ${SHERPA_ONNX_DIR}/lib)

target_link_libraries(appRixoraAI PRIVATE
    RixoraCore
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Multimedia
    Qt6::Concurrent
    Qt6::Svg
    sherpa-onnx-c-api
)

# SVG-иконки для кнопок интерфейса (папка qml/resources/icons).
# Доступны в QML как "qrc:/icons/<FileName>.svg".
qt_add_resources(appRixoraAI "icons"
    PREFIX "/icons"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/qml/resources/icons"
    FILES
        qml/resources/icons/Add.svg
        qml/resources/icons/ChangeLanguages.svg
        qml/resources/icons/Copy.svg
        qml/resources/icons/Delete.svg
        qml/resources/icons/Edit.svg
        qml/resources/icons/keys.svg
        qml/resources/icons/Mic.svg
        "qml/resources/icons/More...svg"
        "qml/resources/icons/New chat.svg"
        qml/resources/icons/Personalization.svg
        qml/resources/icons/Refresh.svg
        qml/resources/icons/Send.svg
        qml/resources/icons/Settings.svg
        qml/resources/icons/Stop.svg
        qml/resources/icons/Volume.svg
        qml/resources/icons/VolumeOff.svg
)

include(GNUInstallDirs)
install(TARGETS appRixoraAI
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# ============================================================
# Модели TTS (Supertonic 3, ~145 МБ)
# ============================================================
# TtsEngine ищет их рядом с исполняемым файлом (applicationDirPath()).
# Без этого шага озвучка не стартует ВООБЩЕ, а отказ молча гас в
# TtsEngine::errorOccurred. Копируем пофайлово через copy_if_different, а не
# copy_directory: иначе каждый build перезаписывает 145 МБ, сборка никогда не
# бывает «чистой», а Qt Creator постоянно пересобирает цель и теряет
# привязку тестов.
set(RIXORA_TTS_MODELS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/models/supertonic3")

if(EXISTS "${RIXORA_TTS_MODELS_DIR}")
    file(GLOB RIXORA_TTS_MODEL_FILES "${RIXORA_TTS_MODELS_DIR}/*")
    set(RIXORA_TTS_COPY_COMMANDS "")
    # Каталог назначения создаём явно. copy_if_different не создаёт
    # недостающие родительские каталоги, поэтому на ЧИСТОЙ сборке
    # (клонирование репозитория, новый build-каталог) копирование
    # падало бы с ошибкой "path does not exist".
    list(APPEND RIXORA_TTS_COPY_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:appRixoraAI>/models/supertonic3")
    foreach(model_file IN LISTS RIXORA_TTS_MODEL_FILES)
        if(NOT IS_DIRECTORY "${model_file}")
            list(APPEND RIXORA_TTS_COPY_COMMANDS
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${model_file}"
                        "$<TARGET_FILE_DIR:appRixoraAI>/models/supertonic3")
        endif()
    endforeach()

    add_custom_command(TARGET appRixoraAI POST_BUILD
        ${RIXORA_TTS_COPY_COMMANDS}
        COMMENT "Копирование моделей TTS Supertonic 3 в каталог сборки"
        VERBATIM
    )
    # Слеш в конце обязателен: без него install(DIRECTORY) копирует САМ
    # каталог supertonic3 внутрь supertonic3, и TtsEngine не найдёт модели
    # по ожидаемому пути applicationDirPath()/models/supertonic3.
    install(DIRECTORY "${RIXORA_TTS_MODELS_DIR}/"
            DESTINATION ${CMAKE_INSTALL_BINDIR}/models/supertonic3)
endif()

# ============================================================
# Тесты слоёв памяти (QtTest)
# Консольный исполняемый файл, намеренно без qt_add_executable/WIN32_EXECUTABLE,
# чтобы ctest видел вывод qDebug/QVERIFY.
# ============================================================
include(CTest)
if(BUILD_TESTING)
    # Network нужен из-за geminiapi.h (QNetworkAccessManager) — его подключает
    # тест ради parseGeminiText, не ради самих сетевых вызовов.
    find_package(Qt6 REQUIRED COMPONENTS Test Network)

    add_executable(tst_memory
        tst_memory.cpp
    )
    set_target_properties(tst_memory PROPERTIES AUTOMOC ON)
    target_include_directories(tst_memory PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(tst_memory PRIVATE RixoraCore Qt6::Test Qt6::Network sherpa-onnx-c-api)
    target_link_directories(tst_memory PRIVATE ${SHERPA_ONNX_DIR}/lib)

    add_test(NAME memory COMMAND tst_memory)
endif()

file(GLOB SHERPA_DLLS "${CMAKE_CURRENT_SOURCE_DIR}/sherpa-onnx/lib/*.dll")

add_custom_command(TARGET appRixoraAI POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${SHERPA_DLLS}
    $<TARGET_FILE_DIR:appRixoraAI>
)