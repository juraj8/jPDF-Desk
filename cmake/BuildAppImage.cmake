set(appdir "${BUILD_DIR}/AppDir")
set(output_dir "${BUILD_DIR}/packages")
file(REMOVE_RECURSE "${appdir}")
file(MAKE_DIRECTORY "${output_dir}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${BUILD_DIR}"
        --prefix "${appdir}/usr" --component Runtime
    COMMAND_ERROR_IS_FATAL ANY
)
# linuxdeploy discovers and bundles shared dependencies; its AppImage output
# plugin must be installed alongside it (or bundled with it). The output
# plugin may download the AppImage runtime when packaging.
# The tools themselves may be AppImages. Extract-and-run mode also propagates
# to the output plugin, avoiding FUSE mounts in unprivileged containers.
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        "APPIMAGE_EXTRACT_AND_RUN=1" "OUTPUT=${APPIMAGE_NAME}"
        "${LINUXDEPLOY}" --appdir "${appdir}"
        --executable "${appdir}/usr/bin/jpdf-desk"
        --desktop-file "${appdir}/usr/share/applications/jpdf-desk.desktop"
        --icon-file "${appdir}/usr/share/icons/hicolor/256x256/apps/jpdf-desk.png"
        --output appimage
    WORKING_DIRECTORY "${output_dir}"
    COMMAND_ERROR_IS_FATAL ANY
)
