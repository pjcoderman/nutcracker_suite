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

    std::string_view strip_whitespace_left(std::string_view str) {
        auto it = std::find_if(str.begin(), str.end(), [](unsigned char ch) {
            return !std::isspace(ch);
        });
        return str.substr(std::distance(str.begin(), it));
    }

    SceneNarrationBuffer::SceneNarrationBuffer(llama_model* model, llama_context *ctx, llama_batch batch, std::ostream *debug_log): 
        model(model), 
        ctx(ctx), 
        batch(batch), 
        debug_log(debug_log), 
        pending(""), 
        done(false), 
        state(CHECKING_THINK),
        vocab(nullptr),
        sampler(nullptr) {

        // Retrieve the model's vocabulary
        this->vocab = llama_model_get_vocab(model);

        // Setup the sampler chain
        auto sparams = llama_sampler_chain_default_params();
        this->sampler = llama_sampler_chain_init(sparams);
        llama_sampler_chain_add(this->sampler, llama_sampler_init_greedy());

        // Evaluate the batch (Send to CPU/GPU for processing)
        if (llama_decode(this->ctx, this->batch) == 0) {
            this->done = false;   
        }

        // Initialize the stream contents to empty
        this->setg(this->pending.data(), this->pending.data(), this->pending.data());
    }

    std::string SceneNarrationBuffer::read_next_token() {
        
        this->token_id = llama_sampler_sample(this->sampler, this->ctx, -1);

        // Detect eog to terminate narration
        if (llama_vocab_is_eog(this->vocab, this->token_id)) {
            this->done = true;
            return "";
        }

        int n_bytes = -1;
        std::string token_str;
        while (n_bytes < 0 && !this->done) {

            // Pass 1: Probe exact required size (returns negative length)
            n_bytes = -llama_token_to_piece(this->vocab, this->token_id, nullptr, 0, 0, true);
            
            // Pass 2: Allocate exact size and write directly into the string                
            if (n_bytes > 0) {
                token_str = std::string(n_bytes, '\0');
                int check = llama_token_to_piece(this->vocab, this->token_id, token_str.data(), token_str.size(), 0, true);
                if (check < 0) {
                    break;
                }
            }

            // Advance to next token
            this->batch = llama_batch_get_one(&this->token_id, 1);
            if (llama_decode(ctx, batch) != 0) {
                this->done = true;
                break;
            }

            if (n_bytes == 0 && !this->done) {
                // TODO: determine if this happens and if it warrants a yield or sleep (or is ok as a tight loop)
            }
        }

        if (n_bytes <= 0) {
            return "";
        }

        return token_str;
    }

    void SceneNarrationBuffer::emit_debug_state_header(std::string_view state_str) {
        if (this->debug_log == nullptr) {
            return;
        }

        auto now = std::chrono::system_clock::now();

        // %FT%T formats as YYYY-MM-DDThh:mm:ss.uz
        std::string timestamp = std::format("{:%FT%TZ}", now);

        (*this->debug_log) << std::endl << timestamp << " PRETHINK [scene_narrator.cpp] ";
        
        if (this->debug_log != nullptr) {
            this->debug_log->flush();
        }
    }

    void SceneNarrationBuffer::emit_debug_content(std::string_view content) {
        if(this->debug_log == nullptr || content.empty()){
            return;
        }
        (*this->debug_log) << content;
        if (this->debug_log != nullptr) {
            this->debug_log->flush();
        }
    }

    void SceneNarrationBuffer::set_state(SceneNarrationBuffer::NarrationState state) {
        if (this->state != state) {
            this->state = state;
            this->content_encountered = false;

            switch(state) {
                case IN_THINK:
                    this->emit_debug_state_header("THINK");
                    break;
                case IN_NARRATE:
                    this->emit_debug_state_header("NARRATE");
                    break;

                case CHECKING_THINK: 
                    //  CHECK_THINK doesn't get a header
                    break;
                
                default:
                    // intentionally no header
                    // NOTE: PRETHINK is an unexpected state not represented in the enum (over when it starts)
                    break;
            }
        }
    }
    
    void SceneNarrationBuffer::capture_pre_think_content(std::string_view content) {
        if (this->state != CHECKING_THINK || content.empty()) {
            return;
        }
        
        if (!this->content_encountered) {
            this->emit_debug_state_header("PRETHINK");
        }

        this->content_encountered = !content.empty();
        this->emit_debug_content(content);
    }

    void SceneNarrationBuffer::capture_think_content(std::string_view content) {

        if (this->state == CHECKING_THINK && !content.empty()) {
            this->set_state(IN_THINK);
        }
        
        if (this->state != IN_THINK) {
            return;
        }

        this->content_encountered = !content.empty();
        this->emit_debug_content(content);
    }

    void SceneNarrationBuffer::capture_narration_content(std::string_view content) {

        if (this->state == IN_THINK && !content.empty()) {
            this->set_state(IN_NARRATE);
        }

        if (this->state != IN_NARRATE) {
            return;
        }

        this->content_encountered = !content.empty();
        this->emit_debug_content(content);

        this->pending = this->pending.append(content);
    }

    SceneNarrationBuffer::int_type SceneNarrationBuffer::underflow() {
        if (gptr() < egptr()) {
            return traits_type::to_int_type(*gptr());
        }
        this->pending = "";

        while(!this->done && this->pending.empty()) {
            
            std::string current_token = this->read_next_token();
            std::string_view token_view = current_token;
            if (!this->content_encountered) {
                token_view = strip_whitespace_left(token_view);
            }
            size_t think_idx;
            std::string_view prethink_content, think_content, narration_content;

            switch(this->state) {

            case CHECKING_THINK:
                think_idx = token_view.find("<think>");
                
                if (think_idx != std::string::npos) {
                    if (0 == think_idx) {
                        prethink_content = "";
                    } else {
                        prethink_content = token_view.substr(0, think_idx);
                    }

                    think_content = token_view.substr(think_idx);
                    
                    this->capture_pre_think_content(prethink_content);
                    this->set_state(IN_THINK);
                    this->capture_think_content(think_content);
                    
                } else if (!token_view.empty()) {
                    // The first non-whitespace token was not <think>, therefore its narration
                    this->set_state(IN_NARRATE);
                    this->capture_narration_content(token_view);
                }
                break;

            case IN_THINK:
                if (!token_view.empty()) {
                    size_t end_idx = token_view.find("</think>");
                    if (end_idx != std::string::npos) {
                        think_content = token_view.substr(0, end_idx + 8);
                        narration_content = strip_whitespace_left(token_view.substr(end_idx + 8));
                        
                        this->capture_think_content(think_content);
                        this->set_state(IN_NARRATE);
                        this->capture_narration_content(narration_content);
                    } else {
                        this->capture_think_content(token_view);
                    }
                }
                break;
            case IN_NARRATE:
                if (!token_view.empty()) {
                    this->capture_narration_content(token_view);
                }
                break;
            }

            if (this->pending.empty() && token_view.empty() && !this->done) {
                // TODO: figure out if this situation happens ever, and if it warrants a yield, sleep, or is fine as tight loop                
            }
        }

        if (this->done && this->pending.empty()) {
            return traits_type::eof();
        }

        this->setg(this->pending.data(), this->pending.data(), this->pending.data() + this->pending.length());
        return traits_type::to_int_type(*this->gptr());
    }

    SceneNarrationBuffer::~SceneNarrationBuffer() {
        if (this->sampler != nullptr) {
            llama_sampler_free(this->sampler);
            this->sampler = nullptr;
        }
    }
}
