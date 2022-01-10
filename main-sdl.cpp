#include "tetrino-sdl.hpp"

int main(int arc, char **argv) {

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cout << "SDL could not initialize: " << SDL_GetError() << std::endl;
        exit(1);
    }

    if (TTF_Init() < 0) {
        std::cout << "SDL could not initialize: " << TTF_GetError() << std::endl;
        exit(1);
    }

    {
        auto game = TetrisSDL();
        while (game.tic()) {
            game.draw();
            game.present();
        }
    }

    TTF_Quit();
    SDL_Quit();

    return 0;
}
