set(kritaimagesplit_SOURCES
    imagesplit.cpp
    dlg_imagesplit.cpp
    wdg_imagesplit.cpp
    )

ki18n_wrap_ui(kritaimagesplit_SOURCES
    wdg_imagesplit.ui
    )

kis_add_library(kritaimagesplit MODULE ${kritaimagesplit_SOURCES})
target_link_libraries(kritaimagesplit kritaui)
install(TARGETS kritaimagesplit  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
install( FILES  imagesplit.xmlgui  DESTINATION ${KDE_INSTALL_DATADIR}/kritaplugins)
