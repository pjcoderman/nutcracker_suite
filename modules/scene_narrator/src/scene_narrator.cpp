#include "scene_narrator.hpp"

#include "llama.h"

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

    SceneNarrator::SceneNarrator(
        llama_model * model, 
        size_t context_window_size,
        std::ostream *debug_log
    ) : 
        context_window_size(context_window_size),
        model(model),
        debug_log(debug_log),
        created_model(false) {

        if (model == nullptr) {
            llama_model_params model_params = llama_model_default_params();
            
            this->model = llama_model_load_from_file(QWEN3_8B_PATH.c_str(), model_params);
            this->created_model = true;
        }
    }

    SceneNarrationStream SceneNarrator::NarrateScene(std::string scene_base_description) { 
        
        // Use the model's native chat template structure (system / user / assistant).
        // This is not about simulating a chat UI; it is the exact instruction contract 
        // instruction-tuned GGUF models (like Qwen) require to understand output boundaries 
        // and naturally emit the EOS/EOG (<|im_end|>) token when finished.
        std::string prompt = 
            "<|im_start|>system\n"
            "You are an atmospheric text-based RPG game engine. Write an immersive, evocative third-person description of the room or scene based on the technical details provided. "
            "Output ONLY the scene description text. Never use first-person pronouns (I, me, my, our). Never include conversational filler, meta-commentary, AI pleasantries, or suggestions for player actions.\n"
            "Use strictly standard ASCII characters only. Never use smart quotes, typographic apostrophes, or em-dashes; use only straight single quotes (') and standard hyphens (-)."
            "<|im_end|>\n"
            "<|im_start|>user\n"
            "Generate the scene description for these details:\n" + scene_base_description + "\n"
            "<|im_end|>\n"
            "<|im_start|>assistant\n";

        if (this->debug_log != nullptr) {
            (*this->debug_log) << prompt;
        }

        std::vector<llama_token> tokens(prompt.size() + 4);
        int n_tokens = llama_tokenize(llama_model_get_vocab(model), prompt.c_str(), prompt.length(), tokens.data(), tokens.size(), true, true);
        tokens.resize(n_tokens);

        // 5. Create a batch for evaluation
        llama_batch batch = llama_batch_get_one(tokens.data(), tokens.size());

        llama_context_params ctx_params = llama_context_default_params();
        ctx_params.n_ctx = context_window_size;

        return SceneNarrationStream(this->model, batch, llama_init_from_model(model, ctx_params), this->debug_log);
    }

    SceneNarrator::~SceneNarrator() {
     
        if (this->created_model && this->model != nullptr) {
            llama_model_free(this->model);
            this->model = nullptr;
        }
    }    
}
