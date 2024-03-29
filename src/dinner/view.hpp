#pragma once

#include "booka.hpp"
#include "repa.hpp"
#include "sdl.hpp"
#include "widget.hpp"

#include <SDL.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class View {
public:
    View(booka::Booka& booka);

    bool processInput();
    void present();

    void showTest();

private:
    bool update();

    booka::Booka& _booka;
    booka::Actions::Iterator _actionIterator;
    size_t _backgroundIndex = size_t(-1);

    sdl::Window _window;
    sdl::Renderer _renderer;

    std::vector<sdl::Texture> _textures;

    repa::Repa _repa;
    ttf::Font _font;
    SpeechBox* _characterBox = nullptr;
    SpeechBox* _speechBox = nullptr;
    Widgets _widgets;
    std::unique_ptr<Mix_Music, void(*)(Mix_Music*)> _music {nullptr, Mix_FreeMusic};

    // This is a (very) poor man's event queue from UI elements. Temporary.
    bool _signalToExit = false;
};
