add_executable(dinner
    client.cpp
    config.cpp
    main.cpp
    view.cpp
)

target_link_libraries(dinner PRIVATE
    arg
    tempo

    base
    booka-lib
    repa-lib
    resources

    SDL2::SDL2
    SDL2::SDL2main
    SDL2_image::SDL2_image
    SDL2_mixer::SDL2_mixer
    SDL2_ttf::SDL2_ttf
    yaml-cpp::yaml-cpp
)
add_dependencies(dinner pack-stories)

if(WIN32)
    add_custom_command(TARGET dinner POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_RUNTIME_DLLS:dinner> $<TARGET_FILE_DIR:dinner>
        COMMAND_EXPAND_LISTS
    )
endif()
