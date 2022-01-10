#include "tetrino-console.hpp"

int main(int argc, char **argv) {
    TetrisConsole game;

    while (game.tic()) {
        game.draw();
        game.present();
        game.throttle();
    }

    return 0;
}