#include "ui.h"
#include "equalizer_ui.h"
#include "player.h"
#include "ui_style.h"

#include <ctime>
#include <cstdlib>

int main() {
    initWindow();

    applyCustomStyle();

    initUITextures();
    
    initAudioPlayer();
    loadPlaylistsFromFile();
    loadEQConfig();

    srand(static_cast<unsigned>(time(nullptr)));

    while (!shouldClose()) {
        startFrame();

        updateTrackPosition();
        drawUI();
        updatePlayback();
        renderFrame();
    }

    shutdownUI();
    shutdownAudio();

    return 0;
}

