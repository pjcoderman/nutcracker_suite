#pragma once

#include "scene_narration_buffer.hpp"
#include "llama.h"

#include <iterator>
#include <string>
#include <vector>

#include <iostream>
#include <streambuf>

#define NARRATOR_CONTEXT_WINDOW_SIZE_DEFAULT 16384

namespace scene_narrator {

    class SceneNarrationStream : public std::istream {
        private:
        SceneNarrationBuffer buffer;
        llama_context *ctx;
     
        public:
        SceneNarrationStream(llama_model* model, llama_batch batch, llama_context *ctx, std::ostream *debug_log);
    
        virtual ~SceneNarrationStream();
    };
}