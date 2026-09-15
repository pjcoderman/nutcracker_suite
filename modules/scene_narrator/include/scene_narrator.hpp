#pragma once

#include "scene_narration_stream.hpp"
#include "scene_narration_buffer.hpp"

#include "llama.h"

#include <iterator>
#include <string>
#include <vector>

#include <iostream>
#include <streambuf>

#define NARRATOR_CONTEXT_WINDOW_SIZE_DEFAULT 16384

namespace scene_narrator {

    class SceneNarrator {
        private:

            bool created_model;
            llama_model * model;
            size_t context_window_size;
            std::ostream * debug_log;
        
        public:

            SceneNarrator(
                llama_model * model=nullptr, 
                size_t context_window_size=NARRATOR_CONTEXT_WINDOW_SIZE_DEFAULT,
                std::ostream * debug_log = nullptr
            );

            ~SceneNarrator();

            SceneNarrationStream NarrateScene(std::string scene_base_description);
    };
}