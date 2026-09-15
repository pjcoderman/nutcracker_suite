#include "llama.h"
#include "scene_narrator.hpp"
#include "semantic_database.hpp"

#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include <format>

const std::string MISTRAL_SMALL_24B_PATH = "C:\\Users\\pperv\\code\\unnamed_rpg\\models\\mistralai_Mistral-Small-3.1-24B-Instruct-2503-Q4_K_M.gguf";
const std::string QWEN3_8B_PATH = "C:\\Users\\pperv\\code\\unnamed_rpg\\models\\Qwen3-8B-Q4_K_M.gguf";

// Define a custom log callback function
void llama_log_callback(ggml_log_level level, const char * text, void * user_data) {
    // Optionally cast user_data to your logger/ofstream if you want to write to a log file
    std::ostream *log_file = static_cast<std::ostream *>(user_data);
    // 1. Get the current time point from the system clock
    auto now = std::chrono::system_clock::now();

    // 2. Format the time point to the exact string format requested
    // %FT%T formats as YYYY-MM-DDThh:mm:ss.uz
    std::string timestamp = std::format("{:%FT%TZ}", now);

    std::string log_level;
    switch(level) {
        case GGML_LOG_LEVEL_NONE:
            log_level = "NONE";
            break;
        case GGML_LOG_LEVEL_DEBUG:
            log_level = "DEBUG";
            break;
        case GGML_LOG_LEVEL_INFO:
            log_level = "INFO";
            break;
        case GGML_LOG_LEVEL_WARN:
            log_level = "WARN";
            break;
        case GGML_LOG_LEVEL_ERROR:
            log_level = "ERROR";
            break;
        case GGML_LOG_LEVEL_CONT: // cont
            //log_level = "CONT";
            if (log_file != nullptr){
                *log_file << text;
                log_file->flush();
            }
            return;
        default:
            log_level = "UNKNOWN";
            break;
    }

    // Filter or route based on log level (GGML_LOG_LEVEL_INFO, WARN, ERROR, DEBUG)
    if (log_file != nullptr) {
        *log_file << timestamp << " " << log_level << " [llama.cpp] " << text;
        log_file->flush();
    }

    // If you want errors to still show up in stderr during development, check the level:
    //if (level >= GGML_LOG_LEVEL_WARN) {
    //    std::cerr << "[llama warning/error] " << text;
    //}
}

void try_semantic_database() {
    auto sdb = semantic_database::SemanticDatabase();
    (void)sdb;

}

int main() {
    try_semantic_database();
    
    std::ofstream narrator_debug_log("narrator.log", std::ios::out | std::ios::app);

    // Register the callback with llama.cpp
    llama_log_set(llama_log_callback, &narrator_debug_log);

    // 1. Initialize the global llama engine system
    llama_backend_init();

    /*

    // 2. Configure model and context settings
    llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = 99; // Offload layers to GPU (Vulkan)
    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx = 16384; // Context window size

    // 3. Load the GGUF model directly into code memory
    llama_model * model = llama_model_load_from_file(QWEN3_8B_PATH.c_str(), model_params);
    llama_context * ctx = llama_init_from_model(model, ctx_params);

    // 4. Tokenize your prompt text string into integer tokens
    std::string prompt = "You are an NPC shopkeeper. Say hello:";
    std::vector<llama_token> tokens(prompt.size() + 4);
    int n_tokens = llama_tokenize(llama_model_get_vocab(model), prompt.c_str(), prompt.length(), tokens.data(), tokens.size(), true, true);
    tokens.resize(n_tokens);

    // 5. Create a batch for evaluation
    llama_batch batch = llama_batch_get_one(tokens.data(), tokens.size());

    // 6. Evaluate the batch (Send to CPU/GPU for processing)
    if (llama_decode(ctx, batch) == 0) {
        // 7. Sample and print the actual response from the model
        const llama_vocab * vocab = llama_model_get_vocab(model);
        auto sparams = llama_sampler_chain_default_params();
        llama_sampler * smpl = llama_sampler_chain_init(sparams);
        llama_sampler_chain_add(smpl, llama_sampler_init_greedy());

        int n_predict = 512;
        std::cout << "\n--- NPC Response ---" << std::endl;

        for (int i = 0; i < n_predict; ++i) {
            llama_token new_token_id = llama_sampler_sample(smpl, ctx, -1);

            if (llama_vocab_is_eog(vocab, new_token_id)) {
                break;
            }

            char buf[128];
            int n = llama_token_to_piece(vocab, new_token_id, buf, sizeof(buf), 0, true);
            if (n < 0) {
                break;
            }
            std::string piece(buf, n);
            std::cout << piece << std::flush;

            batch = llama_batch_get_one(&new_token_id, 1);
            if (llama_decode(ctx, batch) != 0) {
                break;
            }
        }
        std::cout << std::endl;

        llama_sampler_free(smpl);
    }

    // Cleanup memory when game closes or level changes
    llama_free(ctx);
    llama_model_free(model);
    */

    auto narrator = scene_narrator::SceneNarrator(nullptr, NARRATOR_CONTEXT_WINDOW_SIZE_DEFAULT, &narrator_debug_log);
    auto narration = narrator.NarrateScene("A dingy open room with a darkly lit corner containing a treasure chest the player cannot quite make out unless they inspect closer.  Walls are lightly covered in moss between the bricks.  Blood stains are scattered here and there of varying age.  There's only one entrance to the room.  The ceiling is not much larger than the hallway leading into the room.  The masonry around several bricks is cracked, but one brick opposite the dark corner has no remaining cement around it.");

    std::cout << narration.rdbuf() << std::endl;
    llama_backend_free();
    return 0;
}