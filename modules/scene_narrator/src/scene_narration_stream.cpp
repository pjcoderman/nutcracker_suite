#include "llama.h"

//#include "scene_narrator.hpp"

#include "scene_narrator.hpp"

#include <iostream>
//#include <fstream>
#include <vector>
#include <iterator>
#include <format>
#include <string>
#include <chrono>
#include <algorithm>
#include <cctype>

//const std::string MISTRAL_SMALL_24B_PATH = "C:\\Users\\pperv\\code\\unnamed_rpg\\models\\mistralai_Mistral-Small-3.1-24B-Instruct-2503-Q4_K_M.gguf";
const std::string QWEN3_8B_PATH = "C:\\Users\\pperv\\code\\unnamed_rpg\\models\\Qwen3-8B-Q4_K_M.gguf";

namespace scene_narrator {

    SceneNarrationStream::SceneNarrationStream(llama_model* model, llama_batch batch, llama_context *ctx, std::ostream *debug_log) 
        : std::istream(&this->buffer),
        ctx(ctx),
        buffer(model, ctx, batch, debug_log) { }

     SceneNarrationStream::~SceneNarrationStream() { 
        
        if (this->ctx != nullptr) {
            llama_free(this->ctx);
            this->ctx = nullptr;
        }
     }
}
